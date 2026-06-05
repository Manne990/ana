import * as vscode from "vscode";
import type { ToolchainConfig } from "./toolchain";

export interface AnaExtensionConfig extends ToolchainConfig {
  fsUaeConfigPath: string;
  defaultTargetProfile: string;
  defaultAdf: string;
  showCommandsBeforeRun: boolean;
  supportedSdkRange: string;
}

export function readAnaConfig(workspaceRoot?: string): AnaExtensionConfig {
  const config = vscode.workspace.getConfiguration("ana");

  return {
    workspaceRoot,
    sdkPath: config.get<string>("sdkPath", ""),
    makePath: config.get<string>("makePath", "make"),
    anaConvertPath: config.get<string>("anaConvertPath", ""),
    adfToolPath: config.get<string>("adfToolPath", ""),
    emulator: config.get<"fs-uae" | "winuae" | "custom">("emulator", "fs-uae"),
    emulatorPath: config.get<string>("emulatorPath", ""),
    fsUaeConfigPath: config.get<string>("fsUaeConfigPath", ""),
    defaultTargetProfile: config.get<string>("defaultTargetProfile", "a1200-baseline"),
    defaultAdf: config.get<string>("defaultAdf", ""),
    showCommandsBeforeRun: config.get<boolean>("showCommandsBeforeRun", true),
    supportedSdkRange: config.get<string>("supportedSdkRange", ">=0.2.0 <0.3.0")
  };
}

export function workspaceRootFromVscode(): string | undefined {
  return vscode.workspace.workspaceFolders?.[0]?.uri.fsPath;
}
