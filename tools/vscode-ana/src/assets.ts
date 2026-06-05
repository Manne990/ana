import * as path from "node:path";
import * as vscode from "vscode";
import type { AnaExtensionConfig } from "./config";
import { runProcess, type OutputSink } from "./runner";
import { findFirstExisting } from "./workspace";

type ConvertKind = "image" | "spritesheet" | "palette" | "font";

function defaultAnaConvertPath(config: AnaExtensionConfig, workspaceRoot: string): string {
  if (config.anaConvertPath) {
    return config.anaConvertPath;
  }

  return path.join(
    config.sdkPath || workspaceRoot,
    "build",
    "tools",
    "ana-convert",
    "ana-convert"
  );
}

async function findAnaConvertPath(
  config: AnaExtensionConfig,
  workspaceRoot: string
): Promise<string> {
  if (config.anaConvertPath) {
    return config.anaConvertPath;
  }

  const roots = Array.from(new Set([workspaceRoot, config.sdkPath].filter(Boolean)));

  for (const root of roots) {
    const workspaceTool = await findFirstExisting(root, [
      "build/tools/ana-convert/ana-convert",
      "build/tools/ana-convert/ana-convert.exe"
    ]);

    if (workspaceTool) {
      return workspaceTool;
    }
  }

  return defaultAnaConvertPath(config, workspaceRoot);
}

function outputPathFor(inputPath: string, extension: string): string {
  const parsed = path.parse(inputPath);
  return path.join(parsed.dir, `${parsed.name}${extension}`);
}

async function inputNumber(title: string, defaultValue: string): Promise<string | undefined> {
  return vscode.window.showInputBox({
    title,
    value: defaultValue,
    validateInput(value) {
      return /^\d+$/.test(value) && Number(value) > 0 ? undefined : "Enter a positive integer.";
    }
  });
}

async function pickPalette(workspaceRoot: string): Promise<string | undefined> {
  const palettes = await vscode.workspace.findFiles("**/*.anapal", "**/{build,node_modules}/**", 50);

  if (palettes.length === 0) {
    return vscode.window.showInputBox({
      title: "Palette path",
      prompt: "Enter the .anapal path to use for this conversion"
    });
  }

  const selected = await vscode.window.showQuickPick(
    palettes.map((uri) => ({
      label: path.relative(workspaceRoot, uri.fsPath),
      path: uri.fsPath
    })),
    { title: "Select ANA palette" }
  );

  return selected?.path;
}

async function chooseOutputPath(defaultPath: string): Promise<string | undefined> {
  const output = await vscode.window.showSaveDialog({
    title: "Output file",
    defaultUri: vscode.Uri.file(defaultPath),
    saveLabel: "Use Output"
  });

  return output?.fsPath;
}

function convertKindItems(
  ext: string
): Array<{ label: string; convertKind: ConvertKind; description: string }> {
  const items: Array<{ label: string; convertKind: ConvertKind; description: string }> = [
    {
      label: "Convert to ANA image",
      convertKind: "image",
      description: "PNG/PPM -> .anaimg"
    },
    {
      label: "Convert spritesheet to ANA image",
      convertKind: "spritesheet",
      description: "PNG/PPM -> framed .anaimg"
    }
  ];

  if (ext === ".png") {
    items.push(
      {
        label: "Convert to ANA palette",
        convertKind: "palette",
        description: "PNG -> .anapal"
      },
      {
        label: "Convert to ANA bitmap font",
        convertKind: "font",
        description: "PNG -> .anafnt"
      }
    );
  }

  return items;
}

export async function openAssetManifest(resource?: vscode.Uri): Promise<void> {
  if (resource?.fsPath.endsWith(".ana")) {
    const document = await vscode.workspace.openTextDocument(resource);
    await vscode.window.showTextDocument(document);
    return;
  }

  const manifests = await vscode.workspace.findFiles("**/*.ana", "**/{build,node_modules}/**", 50);

  if (manifests.length === 0) {
    vscode.window.showWarningMessage("No ANA asset manifests found.");
    return;
  }

  const workspaceRoot = vscode.workspace.workspaceFolders?.[0]?.uri.fsPath ?? process.cwd();
  const selected = await vscode.window.showQuickPick(
    manifests.map((uri) => ({
      label: path.relative(workspaceRoot, uri.fsPath),
      uri
    })),
    { title: "Open ANA asset manifest" }
  );

  if (!selected) {
    return;
  }

  const document = await vscode.workspace.openTextDocument(selected.uri);
  await vscode.window.showTextDocument(document);
}

export async function convertAsset(
  resource: vscode.Uri | undefined,
  config: AnaExtensionConfig,
  workspaceRoot: string,
  output: OutputSink
): Promise<void> {
  const inputUri = resource ?? vscode.window.activeTextEditor?.document.uri;

  if (!inputUri) {
    vscode.window.showErrorMessage("No asset selected.");
    return;
  }

  const inputPath = inputUri.fsPath;
  const ext = path.extname(inputPath).toLowerCase();

  if (ext !== ".png" && ext !== ".ppm") {
    vscode.window.showErrorMessage("ANA asset conversion currently supports PNG and PPM files.");
    return;
  }

  const selected = await vscode.window.showQuickPick(convertKindItems(ext), {
    title: "ANA asset conversion"
  });

  if (!selected) {
    return;
  }

  const anaConvert = await findAnaConvertPath(config, workspaceRoot);
  const args: string[] = [
    selected.convertKind === "spritesheet" ? "image" : selected.convertKind,
    inputPath
  ];

  let defaultOutput = outputPathFor(inputPath, ".anaimg");

  if (selected.convertKind === "palette") {
    defaultOutput = outputPathFor(inputPath, ".anapal");
  } else if (selected.convertKind === "font") {
    defaultOutput = outputPathFor(inputPath, ".anafnt");
  }

  const outPath = await chooseOutputPath(defaultOutput);

  if (!outPath) {
    return;
  }

  args.push("--out", outPath);

  if (selected.convertKind === "image" || selected.convertKind === "spritesheet") {
    const palette = await pickPalette(workspaceRoot);

    if (!palette) {
      return;
    }

    args.push("--palette", palette, "--transparent", "#ff00ff");
  }

  if (selected.convertKind === "spritesheet") {
    const frameWidth = await inputNumber("Frame width", "16");
    const frameHeight = await inputNumber("Frame height", "16");

    if (!frameWidth || !frameHeight) {
      return;
    }

    args.push("--frame-width", frameWidth, "--frame-height", frameHeight);
  }

  if (selected.convertKind === "palette") {
    args.push("--colors", "16");
  }

  if (selected.convertKind === "font") {
    const charWidth = await inputNumber("Character width", "6");
    const charHeight = await inputNumber("Character height", "7");
    const firstChar = await inputNumber("First character code", "32");
    const chars = await inputNumber("Character count", "96");

    if (!charWidth || !charHeight || !firstChar || !chars) {
      return;
    }

    args.push(
      "--colors",
      "2",
      "--char-width",
      charWidth,
      "--char-height",
      charHeight,
      "--first-char",
      firstChar,
      "--chars",
      chars,
      "--transparent",
      "#ff00ff"
    );
  }

  const exitCode = await runProcess({
    cwd: workspaceRoot,
    label: "Convert Asset",
    command: anaConvert,
    args,
    output,
    showOutput: config.showCommandsBeforeRun
  });

  if (exitCode === 0) {
    vscode.window.showInformationMessage(`Converted asset: ${outPath}`);
  } else {
    vscode.window.showErrorMessage("ANA asset conversion failed. See the ANA output channel.");
  }
}
