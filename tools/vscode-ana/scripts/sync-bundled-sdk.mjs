import { cp, mkdir, rm } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";

const scriptDir = path.dirname(fileURLToPath(import.meta.url));
const extensionRoot = path.resolve(scriptDir, "..");
const repositoryRoot = path.resolve(extensionRoot, "..", "..");
const sdkRoot = path.join(extensionRoot, "ana-sdk");

const entries = [
  "LICENSE",
  "Makefile",
  "include",
  "src",
  path.join("tools", "ana-convert")
];

async function main() {
  await rm(sdkRoot, { recursive: true, force: true });
  await mkdir(sdkRoot, { recursive: true });

  for (const entry of entries) {
    const source = path.join(repositoryRoot, entry);
    const target = path.join(sdkRoot, entry);

    await mkdir(path.dirname(target), { recursive: true });
    await cp(source, target, { recursive: true });
  }

  console.log(`Synchronized ANA SDK from ${repositoryRoot}`);
}

main().catch((error) => {
  console.error(error instanceof Error ? error.message : String(error));
  process.exitCode = 1;
});
