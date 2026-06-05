import * as assert from "node:assert";
import * as fs from "node:fs/promises";
import * as path from "node:path";
import { suite, test } from "mocha";

const extensionRoot = path.resolve(__dirname, "..", "..");

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
});
