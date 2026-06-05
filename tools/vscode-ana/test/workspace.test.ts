import * as assert from "node:assert";
import * as fs from "node:fs/promises";
import * as os from "node:os";
import * as path from "node:path";
import { suite, test } from "mocha";
import {
  isAnaSdkRoot,
  isAnaWorkspaceRoot,
  readAnaProjectSdkPath,
  readAnaVersionString
} from "../src/workspace";

async function tempDir(): Promise<string> {
  return fs.mkdtemp(path.join(os.tmpdir(), "ana-vscode-"));
}

suite("workspace detection", () => {
  test("detects ANA by version header", async () => {
    const root = await tempDir();
    const includeDir = path.join(root, "include", "ana");
    await fs.mkdir(includeDir, { recursive: true });
    await fs.writeFile(
      path.join(includeDir, "ana_version.h"),
      '#define ANA_VERSION_STRING "0.2.0"\n',
      "utf8"
    );

    assert.strictEqual(await isAnaWorkspaceRoot(root), true);
    assert.strictEqual(await readAnaVersionString(root), "0.2.0");
  });

  test("distinguishes SDK roots from ANA project roots", async () => {
    const sdkRoot = await tempDir();
    const sdkIncludeDir = path.join(sdkRoot, "include", "ana");
    await fs.mkdir(sdkIncludeDir, { recursive: true });
    await fs.writeFile(path.join(sdkIncludeDir, "ana_version.h"), "#define ANA_VERSION_STRING \"0.2.0\"\n", "utf8");
    await fs.writeFile(path.join(sdkRoot, "Makefile"), "amiga-a1200-lib:\n\t@echo sdk\n", "utf8");

    const projectRoot = await tempDir();
    await fs.writeFile(
      path.join(projectRoot, "ana.json"),
      JSON.stringify({ sdkPath: sdkRoot }, null, 2),
      "utf8"
    );

    assert.strictEqual(await isAnaWorkspaceRoot(projectRoot), true);
    assert.strictEqual(await isAnaSdkRoot(projectRoot), false);
    assert.strictEqual(await isAnaSdkRoot(sdkRoot), true);
    assert.strictEqual(await readAnaProjectSdkPath(projectRoot), sdkRoot);
  });

  test("detects ANA by Makefile targets", async () => {
    const root = await tempDir();
    await fs.writeFile(root + "/Makefile", "amiga-examples:\n\t@echo ok\n", "utf8");

    assert.strictEqual(await isAnaWorkspaceRoot(root), true);
  });

  test("rejects unrelated folders", async () => {
    const root = await tempDir();
    await fs.writeFile(root + "/Makefile", "all:\n\t@echo ok\n", "utf8");

    assert.strictEqual(await isAnaWorkspaceRoot(root), false);
  });
});
