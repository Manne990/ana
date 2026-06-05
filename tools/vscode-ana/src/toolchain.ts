import * as fs from "node:fs/promises";
import * as os from "node:os";
import * as path from "node:path";
import { execFile } from "node:child_process";
import { promisify } from "node:util";
import { resolveEmulatorExecutable, type EmulatorKind } from "./emulator";

const execFileAsync = promisify(execFile);

export type ToolStatus =
  | "ok"
  | "missing"
  | "found through Docker fallback"
  | "configured path does not exist"
  | "found but version unknown";

export interface ToolCheckResult {
  name: string;
  status: ToolStatus;
  detail: string;
}

export interface ToolchainConfig {
  workspaceRoot?: string;
  sdkPath: string;
  makePath: string;
  anaConvertPath: string;
  adfToolPath: string;
  emulator: EmulatorKind;
  emulatorPath: string;
}

export interface ToolProbe {
  exists(filePath: string): Promise<boolean>;
  findOnPath(command: string): Promise<string | undefined>;
}

export const realToolProbe: ToolProbe = {
  async exists(filePath: string): Promise<boolean> {
    try {
      await fs.access(filePath);
      return true;
    } catch {
      return false;
    }
  },

  async findOnPath(command: string): Promise<string | undefined> {
    const locator = process.platform === "win32" ? "where" : "sh";
    const args =
      process.platform === "win32"
        ? [command]
        : ["-c", `command -v -- '${command.replace(/'/g, "'\\''")}'`];

    try {
      const result = await execFileAsync(locator, args, { timeout: 5000 });
      return result.stdout.split(/\r?\n/).find(Boolean)?.trim();
    } catch {
      return undefined;
    }
  }
};

function looksLikePath(value: string): boolean {
  return path.isAbsolute(value) || value.includes("/") || value.includes("\\");
}

function executableName(name: string): string {
  if (process.platform === "win32" && !name.toLowerCase().endsWith(".exe")) {
    return `${name}.exe`;
  }

  return name;
}

async function checkConfiguredOrPath(
  name: string,
  configuredPath: string,
  commandName: string,
  probe: ToolProbe
): Promise<ToolCheckResult> {
  if (configuredPath) {
    if (looksLikePath(configuredPath)) {
      if (await probe.exists(configuredPath)) {
        return { name, status: "ok", detail: configuredPath };
      }

      return {
        name,
        status: "configured path does not exist",
        detail: configuredPath
      };
    }

    const foundConfiguredCommand = await probe.findOnPath(configuredPath);

    if (foundConfiguredCommand) {
      return { name, status: "ok", detail: foundConfiguredCommand };
    }

    return { name, status: "missing", detail: configuredPath };
  }

  const found = await probe.findOnPath(commandName);

  if (found) {
    return { name, status: "ok", detail: found };
  }

  return { name, status: "missing", detail: commandName };
}

async function findAnaConvert(config: ToolchainConfig, probe: ToolProbe): Promise<ToolCheckResult> {
  if (config.anaConvertPath) {
    return checkConfiguredOrPath("ana-convert", config.anaConvertPath, "ana-convert", probe);
  }

  const roots = [config.workspaceRoot, config.sdkPath].filter(Boolean) as string[];

  for (const root of roots) {
    const candidate = path.join(
      root,
      "build",
      "tools",
      "ana-convert",
      executableName("ana-convert")
    );

    if (await probe.exists(candidate)) {
      return { name: "ana-convert", status: "ok", detail: candidate };
    }
  }

  const found = await probe.findOnPath("ana-convert");

  if (found) {
    return { name: "ana-convert", status: "ok", detail: found };
  }

  return {
    name: "ana-convert",
    status: "missing",
    detail: "build/tools/ana-convert/ana-convert or PATH"
  };
}

async function findAdfTool(config: ToolchainConfig, probe: ToolProbe): Promise<ToolCheckResult> {
  if (config.adfToolPath) {
    return checkConfiguredOrPath("gadf", config.adfToolPath, "gadf", probe);
  }

  const pathResult = await probe.findOnPath("gadf");

  if (pathResult) {
    return { name: "gadf", status: "ok", detail: pathResult };
  }

  const goPath = path.join(os.homedir(), "go", "bin", executableName("gadf"));

  if (await probe.exists(goPath)) {
    return { name: "gadf", status: "ok", detail: goPath };
  }

  return { name: "gadf", status: "missing", detail: "PATH or $HOME/go/bin/gadf" };
}

async function checkM68kToolchain(probe: ToolProbe): Promise<ToolCheckResult[]> {
  const gcc = await probe.findOnPath("m68k-amigaos-gcc");
  const ar = await probe.findOnPath("m68k-amigaos-ar");

  if (gcc && ar) {
    return [
      { name: "m68k-amigaos-gcc", status: "ok", detail: gcc },
      { name: "m68k-amigaos-ar", status: "ok", detail: ar }
    ];
  }

  const docker = await probe.findOnPath("docker");

  if (docker) {
    return [
      {
        name: "m68k Amiga toolchain",
        status: "found through Docker fallback",
        detail: "amigadev/crosstools:m68k-amigaos-gcc10_amd64"
      }
    ];
  }

  return [
    {
      name: "m68k Amiga toolchain",
      status: "missing",
      detail: "m68k-amigaos-gcc/m68k-amigaos-ar or Docker"
    }
  ];
}

async function checkHostCompiler(probe: ToolProbe): Promise<ToolCheckResult> {
  for (const compiler of ["cc", "gcc", "clang"]) {
    const found = await probe.findOnPath(compiler);

    if (found) {
      return { name: "host C compiler", status: "ok", detail: found };
    }
  }

  return { name: "host C compiler", status: "missing", detail: "cc, gcc, or clang" };
}

async function checkVasm(probe: ToolProbe): Promise<ToolCheckResult> {
  const found = await probe.findOnPath("vasmm68k_mot");

  if (found) {
    return { name: "vasmm68k_mot", status: "ok", detail: found };
  }

  const docker = await probe.findOnPath("docker");

  if (docker) {
    return {
      name: "vasmm68k_mot",
      status: "found through Docker fallback",
      detail: "provided by the documented Amiga build image"
    };
  }

  return { name: "vasmm68k_mot", status: "missing", detail: "PATH or Docker fallback" };
}

export async function checkToolchain(
  config: ToolchainConfig,
  probe: ToolProbe = realToolProbe
): Promise<ToolCheckResult[]> {
  const results: ToolCheckResult[] = [];

  results.push(await checkConfiguredOrPath("make", config.makePath, "make", probe));
  results.push(await checkHostCompiler(probe));
  results.push(await checkConfiguredOrPath("python3", "", "python3", probe));
  results.push(...(await checkM68kToolchain(probe)));
  results.push(await checkVasm(probe));
  results.push(await findAdfTool(config, probe));
  results.push(await findAnaConvert(config, probe));

  const emulator = await resolveEmulatorExecutable(config.emulator, config.emulatorPath, probe);
  results.push(
    emulator.command
      ? { name: "emulator", status: "ok", detail: emulator.detail }
      : emulator.configuredPathMissing
        ? {
            name: "emulator",
            status: "configured path does not exist",
            detail: emulator.detail
          }
      : {
          name: "emulator",
          status: "missing",
          detail: emulator.detail
        }
  );

  return results;
}
