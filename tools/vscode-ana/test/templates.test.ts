import * as assert from "node:assert";
import * as fs from "node:fs/promises";
import * as os from "node:os";
import * as path from "node:path";
import { execFile } from "node:child_process";
import { promisify } from "node:util";
import { suite, test } from "mocha";

const extensionRoot = path.resolve(__dirname, "..", "..");
const repositoryRoot = path.resolve(extensionRoot, "..", "..");
const execFileAsync = promisify(execFile);

async function readText(filePath: string): Promise<string> {
  return fs.readFile(filePath, "utf8");
}

async function templateDirectories(): Promise<string[]> {
  const templateRoot = path.join(extensionRoot, "templates");
  const entries = await fs.readdir(templateRoot, { withFileTypes: true });
  const directories: string[] = [];

  for (const entry of entries) {
    if (!entry.isDirectory()) {
      continue;
    }

    const templateDir = path.join(templateRoot, entry.name);

    try {
      await fs.access(path.join(templateDir, "ana-template.json"));
      directories.push(templateDir);
    } catch {
      // Not an ANA template directory.
    }
  }

  return directories;
}

async function relativeFiles(root: string, directory = ""): Promise<string[]> {
  const entries = await fs.readdir(path.join(root, directory), { withFileTypes: true });
  const files: string[] = [];

  for (const entry of entries) {
    const relative = path.join(directory, entry.name);

    if (entry.isDirectory()) {
      files.push(...(await relativeFiles(root, relative)));
    } else {
      files.push(relative);
    }
  }

  return files.sort();
}

async function assertDirectoriesEqual(source: string, target: string): Promise<void> {
  const sourceFiles = await relativeFiles(source);
  const targetFiles = await relativeFiles(target);

  assert.deepStrictEqual(targetFiles, sourceFiles);
  for (const relative of sourceFiles) {
    assert.deepStrictEqual(
      await fs.readFile(path.join(target, relative)),
      await fs.readFile(path.join(source, relative)),
      relative
    );
  }
}

suite("bundled templates", () => {
  test("use the local bundled SDK path", async () => {
    for (const templateDir of await templateDirectories()) {
      const anaJson = JSON.parse(await readText(path.join(templateDir, "ana.json"))) as {
        sdkPath?: unknown;
      };
      const makefile = await readText(path.join(templateDir, "Makefile"));
      const readme = await readText(path.join(templateDir, "README.md"));

      assert.strictEqual(anaJson.sdkPath, ".ana-sdk", templateDir);
      assert.match(makefile, /^ANA_SDK \?= \.ana-sdk/m, templateDir);
      assert.doesNotMatch(makefile, /\/Users\/|Projects\/ana|\{\{sdkPath\}\}/, templateDir);
      assert.doesNotMatch(readme, /Set `ANA_SDK`/, templateDir);
    }
  });

  test("bundle includes SDK targets required by generated projects", async () => {
    const sdkRoot = path.join(extensionRoot, "ana-sdk");
    const makefile = await readText(path.join(sdkRoot, "Makefile"));

    assert.match(makefile, /^lib:/m);
    assert.match(makefile, /^tools:/m);
    assert.match(makefile, /^amiga-a1200-lib:/m);
    await fs.access(path.join(sdkRoot, "include", "ana", "ana_version.h"));
    await fs.access(path.join(sdkRoot, "tools", "ana-convert", "vendor", "stb_image.h"));
    await fs.access(path.join(sdkRoot, "src", "sound", "vendor", "ptplayer", "ptplayer.asm"));
  });

  test("bundle is an exact snapshot of the canonical SDK sources", async () => {
    const sdkRoot = path.join(extensionRoot, "ana-sdk");

    assert.deepStrictEqual(
      await fs.readFile(path.join(sdkRoot, "LICENSE")),
      await fs.readFile(path.join(repositoryRoot, "LICENSE"))
    );
    assert.deepStrictEqual(
      await fs.readFile(path.join(sdkRoot, "Makefile")),
      await fs.readFile(path.join(repositoryRoot, "Makefile"))
    );
    await assertDirectoriesEqual(
      path.join(repositoryRoot, "include"),
      path.join(sdkRoot, "include")
    );
    await assertDirectoriesEqual(path.join(repositoryRoot, "src"), path.join(sdkRoot, "src"));
    await assertDirectoriesEqual(
      path.join(repositoryRoot, "tools", "ana-convert"),
      path.join(sdkRoot, "tools", "ana-convert")
    );
  });

  test("bundled SDK builds its host library and converter", async function () {
    this.timeout(30_000);
    const sdkRoot = path.join(extensionRoot, "ana-sdk");
    const buildRoot = await fs.mkdtemp(path.join(os.tmpdir(), "ana-sdk-build-"));

    try {
      await execFileAsync("make", ["lib", "tools", `BUILD_DIR=${buildRoot}`], {
        cwd: sdkRoot
      });
      await fs.access(path.join(buildRoot, "libana.a"));
      await fs.access(path.join(buildRoot, "tools", "ana-convert", "ana-convert"));
    } finally {
      await fs.rm(buildRoot, { recursive: true, force: true });
    }
  });
});
