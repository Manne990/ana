import * as fs from "node:fs/promises";
import * as path from "node:path";
import * as vscode from "vscode";
import { convertAsset, openAssetManifest } from "./assets";
import { getBuildCommand } from "./buildCommands";
import { readAnaConfig, workspaceRootFromVscode, type AnaExtensionConfig } from "./config";
import { registerContentCompletion } from "./contentCompletion";
import { emulatorArgs, resolveEmulatorExecutable } from "./emulator";
import { runProcess } from "./runner";
import { createProjectFromTemplate, installTemplates } from "./templates";
import { checkToolchain, realToolProbe } from "./toolchain";
import {
  isAnaSdkRoot,
  isAnaWorkspaceRoot,
  readAnaProjectSdkPath,
  readAnaVersionString
} from "./workspace";

const output = vscode.window.createOutputChannel("ANA");

function resolveConfiguredPath(root: string | undefined, configuredPath: string): string {
  if (!configuredPath) {
    return "";
  }

  return path.isAbsolute(configuredPath)
    ? configuredPath
    : path.resolve(root ?? process.cwd(), configuredPath);
}

function workspaceRootOrConfiguredSdk(): string | undefined {
  const workspaceRoot = workspaceRootFromVscode();
  const config = readAnaConfig(workspaceRoot);

  if (workspaceRoot) {
    return workspaceRoot;
  }

  return resolveConfiguredPath(undefined, config.sdkPath) || undefined;
}

async function resolveAnaSdkPath(
  workspaceRoot: string | undefined,
  config: AnaExtensionConfig
): Promise<string> {
  const configuredSdkPath = resolveConfiguredPath(workspaceRoot, config.sdkPath);

  if (configuredSdkPath) {
    return configuredSdkPath;
  }

  if (!workspaceRoot) {
    return "";
  }

  const projectSdkPath = await readAnaProjectSdkPath(workspaceRoot);

  if (projectSdkPath) {
    return projectSdkPath;
  }

  return (await isAnaSdkRoot(workspaceRoot)) ? workspaceRoot : "";
}

async function readResolvedAnaConfig(workspaceRoot: string | undefined): Promise<AnaExtensionConfig> {
  const config = readAnaConfig(workspaceRoot);
  const sdkPath = await resolveAnaSdkPath(workspaceRoot, config);

  return {
    ...config,
    sdkPath
  };
}

function resolveWorkspacePath(root: string, configuredPath: string): string {
  return path.isAbsolute(configuredPath) ? configuredPath : path.join(root, configuredPath);
}

async function ensureAnaRoot(): Promise<string | undefined> {
  const root = workspaceRootOrConfiguredSdk();

  if (!root) {
    vscode.window.showErrorMessage("Open an ANA workspace or set ana.sdkPath first.");
    return undefined;
  }

  if (!(await isAnaWorkspaceRoot(root))) {
    const choice = await vscode.window.showWarningMessage(
      `This does not look like an ANA workspace: ${root}`,
      "Use Anyway",
      "Cancel"
    );

    if (choice !== "Use Anyway") {
      return undefined;
    }
  }

  return root;
}

async function runBuild(key: Parameters<typeof getBuildCommand>[0]): Promise<void> {
  const root = await ensureAnaRoot();

  if (!root) {
    return;
  }

  const config = await readResolvedAnaConfig(root);
  const command = getBuildCommand(key, config.makePath);
  const exitCode = await runProcess({
    cwd: root,
    label: command.label,
    command: command.command,
    args: command.args,
    output,
    showOutput: config.showCommandsBeforeRun
  });

  if (exitCode === 0) {
    vscode.window.showInformationMessage(`ANA ${command.label} completed.`);
  } else {
    vscode.window.showErrorMessage(`ANA ${command.label} failed. See the ANA output channel.`);
  }
}

async function runHostExample(): Promise<void> {
  const root = await ensureAnaRoot();

  if (!root) {
    return;
  }

  const config = await readResolvedAnaConfig(root);
  const candidates = [
    "build/examples/hello/hello",
    "build/examples/invaders/invaders",
    "build/examples/amaze/amaze",
    "build/examples/byte_brothers/byte_brothers"
  ];

  const existing = [];

  for (const candidate of candidates) {
    const fullPath = path.join(root, candidate);

    try {
      await fs.access(fullPath);
      existing.push({ label: candidate, path: fullPath });
    } catch {
      // Candidate is not built yet.
    }
  }

  if (existing.length === 0) {
    vscode.window.showWarningMessage("No host examples found. Run ANA: Build Host first.");
    return;
  }

  const selected = await vscode.window.showQuickPick(existing, {
    title: "Run ANA host example"
  });

  if (!selected) {
    return;
  }

  await runProcess({
    cwd: root,
    label: "Run Host Example",
    command: selected.path,
    args: [],
    output,
    showOutput: config.showCommandsBeforeRun
  });
}

async function pickAdf(root: string): Promise<string | undefined> {
  const config = readAnaConfig(root);

  if (config.defaultAdf) {
    return path.isAbsolute(config.defaultAdf)
      ? config.defaultAdf
      : path.join(root, config.defaultAdf);
  }

  const adfs = await vscode.workspace.findFiles("build/adf/*.adf", "**/{node_modules,.git}/**", 50);

  if (adfs.length > 0) {
    const selected = await vscode.window.showQuickPick(
      adfs.map((uri) => ({
        label: path.relative(root, uri.fsPath),
        path: uri.fsPath
      })),
      { title: "Select ANA ADF" }
    );

    return selected?.path;
  }

  const picked = await vscode.window.showOpenDialog({
    title: "Select ANA ADF",
    canSelectFiles: true,
    canSelectFolders: false,
    canSelectMany: false,
    filters: {
      "ADF images": ["adf"]
    }
  });

  return picked?.[0]?.fsPath;
}

async function runAdf(): Promise<void> {
  const root = await ensureAnaRoot();

  if (!root) {
    return;
  }

  const config = await readResolvedAnaConfig(root);
  const adfPath = await pickAdf(root);

  if (!adfPath) {
    return;
  }

  let fsUaeConfigPath: string | undefined;

  if (config.emulator === "fs-uae" && config.fsUaeConfigPath) {
    fsUaeConfigPath = resolveWorkspacePath(root, config.fsUaeConfigPath);

    try {
      await fs.access(fsUaeConfigPath);
    } catch {
      vscode.window.showErrorMessage(`FS-UAE config not found: ${fsUaeConfigPath}`);
      return;
    }
  }

  const emulator = await resolveEmulatorExecutable(
    config.emulator,
    config.emulatorPath,
    realToolProbe
  );

  if (!emulator.command) {
    output.show(true);
    output.appendLine("");
    output.appendLine("== Run ADF in Emulator ==");
    output.appendLine(`cwd: ${root}`);
    output.appendLine(`error: emulator not found (${emulator.detail})`);
    output.appendLine(
      "Set ana.emulatorPath to the emulator executable, for example /Applications/FS-UAE.app/Contents/MacOS/fs-uae."
    );
    vscode.window.showErrorMessage("Emulator not found. Set ana.emulatorPath or install FS-UAE on PATH.");
    return;
  }

  const exitCode = await runProcess({
    cwd: root,
    label: "Run ADF in Emulator",
    command: emulator.command,
    args: emulatorArgs(config.emulator, adfPath, { fsUaeConfigPath }),
    output,
    showOutput: config.showCommandsBeforeRun
  });

  if (exitCode !== 0) {
    vscode.window.showErrorMessage("Emulator launch failed. See the ANA output channel.");
  }
}

async function runToolchainCheck(): Promise<void> {
  const root = workspaceRootOrConfiguredSdk();
  const config = await readResolvedAnaConfig(root);
  const results = await checkToolchain(config);
  const sdkVersion = config.sdkPath
    ? await readAnaVersionString(config.sdkPath)
    : root
      ? await readAnaVersionString(root)
      : undefined;

  output.show(true);
  output.appendLine("");
  output.appendLine("== Check Toolchain ==");

  if (root) {
    output.appendLine(`workspace: ${root}`);
  }

  if (config.sdkPath && config.sdkPath !== root) {
    output.appendLine(`ANA SDK: ${config.sdkPath}`);
  }

  if (sdkVersion) {
    output.appendLine(`ANA SDK version: ${sdkVersion}`);
  }

  for (const result of results) {
    output.appendLine(`${result.name}: ${result.status} (${result.detail})`);
  }

  const missing = results.filter(
    (result) =>
      result.status === "missing" || result.status === "configured path does not exist"
  );

  if (missing.length === 0) {
    vscode.window.showInformationMessage("ANA toolchain check passed.");
  } else {
    vscode.window.showWarningMessage(`ANA toolchain check found ${missing.length} issue(s).`);
  }
}

async function configurePaths(): Promise<void> {
  await vscode.commands.executeCommand("workbench.action.openSettings", "@ext:ana.ana-vscode");
}

async function convertSelectedAsset(resource?: vscode.Uri): Promise<void> {
  const root = await ensureAnaRoot();

  if (!root) {
    return;
  }

  const config = await readResolvedAnaConfig(root);
  await convertAsset(resource, config, root, output);
}

export function activate(context: vscode.ExtensionContext): void {
  context.subscriptions.push(output);
  registerContentCompletion(context);

  context.subscriptions.push(
    vscode.commands.registerCommand("ana.checkToolchain", () => {
      void runToolchainCheck();
    }),
    vscode.commands.registerCommand("ana.configurePaths", () => {
      void configurePaths();
    }),
    vscode.commands.registerCommand("ana.installTemplates", () => {
      void installTemplates(context);
    }),
    vscode.commands.registerCommand("ana.createProject", () => {
      void createProjectFromTemplate(context);
    }),
    vscode.commands.registerCommand("ana.buildHost", () => {
      void runBuild("buildHost");
    }),
    vscode.commands.registerCommand("ana.runHostExample", () => {
      void runHostExample();
    }),
    vscode.commands.registerCommand("ana.buildAmiga", () => {
      void runBuild("buildAmiga");
    }),
    vscode.commands.registerCommand("ana.buildAdf", () => {
      void runBuild("buildAdf");
    }),
    vscode.commands.registerCommand("ana.buildA1200Adf", () => {
      void runBuild("buildA1200Adf");
    }),
    vscode.commands.registerCommand("ana.runAdf", () => {
      void runAdf();
    }),
    vscode.commands.registerCommand("ana.convertAsset", (resource?: vscode.Uri) => {
      void convertSelectedAsset(resource);
    }),
    vscode.commands.registerCommand("ana.openAssetManifest", (resource?: vscode.Uri) => {
      void openAssetManifest(resource);
    })
  );
}

export function deactivate(): void {
  output.dispose();
}
