import { readFile, writeFile } from "node:fs/promises";
import path from "node:path";

const packagePath = path.resolve("package.json");
const packageLockPath = path.resolve("package-lock.json");

function parseArgs(argv) {
  const buildNumberFlag = "--build-number";
  const buildNumberIndex = argv.indexOf(buildNumberFlag);

  if (buildNumberIndex === -1) {
    return {};
  }

  const buildNumber = argv[buildNumberIndex + 1];

  if (!buildNumber) {
    throw new Error(`${buildNumberFlag} requires a value.`);
  }

  return { buildNumber };
}

function nextVersion(currentVersion, buildNumber) {
  const match = currentVersion.match(/^(\d+)\.(\d+)\.(\d+)$/);

  if (!match) {
    throw new Error(`Expected a major.minor.patch version, got ${currentVersion}.`);
  }

  const major = Number(match[1]);
  const minor = Number(match[2]);
  const patch = buildNumber === undefined ? Number(match[3]) + 1 : Number(buildNumber);

  if (!Number.isSafeInteger(patch) || patch < 0) {
    throw new Error(`Invalid build number: ${buildNumber}.`);
  }

  return `${major}.${minor}.${patch}`;
}

async function readJson(filePath) {
  return JSON.parse(await readFile(filePath, "utf8"));
}

async function writeJson(filePath, value) {
  await writeFile(filePath, `${JSON.stringify(value, null, 2)}\n`, "utf8");
}

async function main() {
  const { buildNumber } = parseArgs(process.argv.slice(2));
  const packageJson = await readJson(packagePath);
  const version = nextVersion(packageJson.version, buildNumber);

  packageJson.version = version;
  await writeJson(packagePath, packageJson);

  const packageLock = await readJson(packageLockPath);
  packageLock.version = version;

  if (packageLock.packages?.[""]) {
    packageLock.packages[""].version = version;
  }

  await writeJson(packageLockPath, packageLock);
  console.log(`ANA VS Code extension version: ${version}`);
}

main().catch((error) => {
  console.error(error.message);
  process.exitCode = 1;
});
