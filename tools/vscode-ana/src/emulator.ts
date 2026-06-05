import * as os from "node:os";
import * as path from "node:path";

export type EmulatorKind = "fs-uae" | "winuae" | "custom";

export interface EmulatorProbe {
  exists(filePath: string): Promise<boolean>;
  findOnPath(command: string): Promise<string | undefined>;
}

export interface ResolvedEmulator {
  command?: string;
  detail: string;
  configuredPathMissing: boolean;
}

function looksLikePath(value: string): boolean {
  return path.isAbsolute(value) || value.includes("/") || value.includes("\\");
}

function macAppExecutable(appPath: string, executableName: string): string {
  return path.join(appPath, "Contents", "MacOS", executableName);
}

function defaultCommand(kind: EmulatorKind): string {
  if (kind === "winuae") {
    return "winuae";
  }

  return "fs-uae";
}

function macAppCandidates(kind: EmulatorKind): string[] {
  if (kind !== "fs-uae") {
    return [];
  }

  return [
    macAppExecutable("/Applications/FS-UAE.app", "fs-uae"),
    macAppExecutable(path.join(os.homedir(), "Applications", "FS-UAE.app"), "fs-uae")
  ];
}

export async function resolveEmulatorExecutable(
  kind: EmulatorKind,
  configuredPath: string,
  probe: EmulatorProbe
): Promise<ResolvedEmulator> {
  if (configuredPath) {
    if (configuredPath.endsWith(".app")) {
      const executable = macAppExecutable(configuredPath, defaultCommand(kind));

      if (await probe.exists(executable)) {
        return { command: executable, detail: `${configuredPath} (${executable})`, configuredPathMissing: false };
      }
    }

    if (looksLikePath(configuredPath)) {
      if (await probe.exists(configuredPath)) {
        return { command: configuredPath, detail: configuredPath, configuredPathMissing: false };
      }

      return { detail: configuredPath, configuredPathMissing: true };
    }

    const configuredCommand = await probe.findOnPath(configuredPath);

    if (configuredCommand) {
      return { command: configuredCommand, detail: configuredCommand, configuredPathMissing: false };
    }

    return { detail: configuredPath, configuredPathMissing: false };
  }

  if (kind === "custom") {
    return { detail: "set ana.emulatorPath for a custom emulator", configuredPathMissing: false };
  }

  const commandName = defaultCommand(kind);
  const commandOnPath = await probe.findOnPath(commandName);

  if (commandOnPath) {
    return { command: commandOnPath, detail: commandOnPath, configuredPathMissing: false };
  }

  for (const candidate of macAppCandidates(kind)) {
    if (await probe.exists(candidate)) {
      return { command: candidate, detail: candidate, configuredPathMissing: false };
    }
  }

  return { detail: commandName, configuredPathMissing: false };
}

export interface EmulatorArgsOptions {
  fsUaeConfigPath?: string;
}

export function emulatorArgs(
  kind: EmulatorKind,
  adfPath: string,
  options: EmulatorArgsOptions = {}
): string[] {
  if (kind === "fs-uae") {
    const args: string[] = [];

    if (options.fsUaeConfigPath) {
      args.push(options.fsUaeConfigPath);
    }

    args.push(`--floppy-drive-0=${adfPath}`);
    return args;
  }

  if (kind === "winuae") {
    return ["-0", adfPath];
  }

  return [adfPath];
}
