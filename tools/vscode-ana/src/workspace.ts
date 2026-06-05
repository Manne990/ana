import * as fs from "node:fs/promises";
import * as path from "node:path";

const ANA_MARKERS = [
  "include/ana/ana.h",
  "include/ana/ana_version.h",
  "ana.json"
];

async function pathExists(filePath: string): Promise<boolean> {
  try {
    await fs.access(filePath);
    return true;
  } catch {
    return false;
  }
}

async function makefileLooksLikeAna(root: string): Promise<boolean> {
  const makefilePath = path.join(root, "Makefile");

  try {
    const makefile = await fs.readFile(makefilePath, "utf8");
    return (
      makefile.includes("amiga-examples") ||
      makefile.includes("amiga-a1200-examples") ||
      makefile.includes("adfs")
    );
  } catch {
    return false;
  }
}

async function makefileLooksLikeAnaSdk(root: string): Promise<boolean> {
  const makefilePath = path.join(root, "Makefile");

  try {
    const makefile = await fs.readFile(makefilePath, "utf8");
    return (
      makefile.includes("amiga-a1200-lib") ||
      makefile.includes("ANA_SRCS") ||
      makefile.includes("build/tools/ana-convert/ana-convert")
    );
  } catch {
    return false;
  }
}

export async function isAnaWorkspaceRoot(root: string): Promise<boolean> {
  for (const marker of ANA_MARKERS) {
    if (await pathExists(path.join(root, marker))) {
      return true;
    }
  }

  return makefileLooksLikeAna(root);
}

export async function isAnaSdkRoot(root: string): Promise<boolean> {
  if (!(await pathExists(path.join(root, "include", "ana", "ana_version.h")))) {
    return false;
  }

  return makefileLooksLikeAnaSdk(root);
}

export async function readAnaProjectSdkPath(root: string): Promise<string | undefined> {
  const projectPath = path.join(root, "ana.json");

  try {
    const raw = await fs.readFile(projectPath, "utf8");
    const project = JSON.parse(raw) as { sdkPath?: unknown };

    if (typeof project.sdkPath !== "string" || project.sdkPath.length === 0) {
      return undefined;
    }

    return path.isAbsolute(project.sdkPath)
      ? project.sdkPath
      : path.resolve(root, project.sdkPath);
  } catch {
    return undefined;
  }
}

export async function readAnaVersionString(root: string): Promise<string | undefined> {
  const headerPath = path.join(root, "include", "ana", "ana_version.h");

  try {
    const header = await fs.readFile(headerPath, "utf8");
    const match = header.match(/ANA_VERSION_STRING\s+"([^"]+)"/);
    return match?.[1];
  } catch {
    return undefined;
  }
}

export async function findFirstExisting(root: string, candidates: string[]): Promise<string | undefined> {
  for (const candidate of candidates) {
    const fullPath = path.join(root, candidate);

    if (await pathExists(fullPath)) {
      return fullPath;
    }
  }

  return undefined;
}

export async function ensureDirectory(dirPath: string): Promise<void> {
  await fs.mkdir(dirPath, { recursive: true });
}
