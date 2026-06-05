import { readdir, rm, readFile } from "node:fs/promises";
import { spawnSync } from "node:child_process";
import path from "node:path";

async function removeExistingVsix() {
  const entries = await readdir(".");

  await Promise.all(
    entries
      .filter((entry) => /^ana-vscode-\d+\.\d+\.\d+\.vsix$/.test(entry))
      .map((entry) => rm(entry, { force: true }))
  );
}

async function main() {
  const packageJson = JSON.parse(await readFile(path.resolve("package.json"), "utf8"));
  const version = packageJson.version;

  if (typeof version !== "string" || !/^\d+\.\d+\.\d+$/.test(version)) {
    throw new Error(`Expected a major.minor.patch version, got ${version}.`);
  }

  await removeExistingVsix();

  const npx = process.platform === "win32" ? "npx.cmd" : "npx";
  const outFile = `ana-vscode-${version}.vsix`;
  const result = spawnSync(npx, ["vsce", "package", "--out", outFile], {
    stdio: "inherit"
  });

  if (result.status !== 0) {
    process.exitCode = result.status ?? 1;
    return;
  }

  console.log(`Packaged ${outFile}`);
}

main().catch((error) => {
  console.error(error.message);
  process.exitCode = 1;
});
