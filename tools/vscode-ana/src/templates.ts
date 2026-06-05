import * as fs from "node:fs/promises";
import * as path from "node:path";
import * as vscode from "vscode";
import { ensureDirectory } from "./workspace";

interface TemplateMetadata {
  id: string;
  name: string;
  description: string;
}

const TEMPLATE_VERSION_FILE = ".ana-template-version";

async function pathExists(filePath: string): Promise<boolean> {
  try {
    await fs.access(filePath);
    return true;
  } catch {
    return false;
  }
}

async function readTemplateMetadata(templateDir: string): Promise<TemplateMetadata | undefined> {
  const metadataPath = path.join(templateDir, "ana-template.json");

  try {
    const raw = await fs.readFile(metadataPath, "utf8");
    return JSON.parse(raw) as TemplateMetadata;
  } catch {
    return undefined;
  }
}

async function listTemplateMetadata(templateRoot: string): Promise<TemplateMetadata[]> {
  const entries = await fs.readdir(templateRoot, { withFileTypes: true });
  const templates: TemplateMetadata[] = [];

  for (const entry of entries) {
    if (!entry.isDirectory()) {
      continue;
    }

    const metadata = await readTemplateMetadata(path.join(templateRoot, entry.name));

    if (metadata) {
      templates.push(metadata);
    }
  }

  return templates.sort((a, b) => a.name.localeCompare(b.name));
}

function replacePlaceholders(content: string, projectName: string): string {
  const executableName = projectName
    .trim()
    .toLowerCase()
    .replace(/[^a-z0-9_ -]/g, "")
    .replace(/[\s-]+/g, "_") || "ana_game";

  return content
    .replace(/\{\{projectName\}\}/g, projectName)
    .replace(/\{\{executableName\}\}/g, executableName);
}

async function copyTemplateDirectory(
  sourceDir: string,
  targetDir: string,
  projectName: string
): Promise<void> {
  await ensureDirectory(targetDir);

  const entries = await fs.readdir(sourceDir, { withFileTypes: true });

  for (const entry of entries) {
    if (entry.name === "ana-template.json") {
      continue;
    }

    const sourcePath = path.join(sourceDir, entry.name);
    const targetPath = path.join(targetDir, entry.name);

    if (entry.isDirectory()) {
      await copyTemplateDirectory(sourcePath, targetPath, projectName);
      continue;
    }

    const content = await fs.readFile(sourcePath, "utf8");
    await fs.writeFile(targetPath, replacePlaceholders(content, projectName), "utf8");
  }
}

async function copyBundledSdk(context: vscode.ExtensionContext, targetDir: string): Promise<void> {
  const bundledSdk = path.join(context.extensionUri.fsPath, "ana-sdk");
  const targetSdk = path.join(targetDir, ".ana-sdk");

  if (!(await pathExists(bundledSdk))) {
    throw new Error(`Bundled ANA SDK not found: ${bundledSdk}`);
  }

  await fs.rm(targetSdk, { recursive: true, force: true });
  await fs.cp(bundledSdk, targetSdk, { recursive: true });
}

function templateSetVersion(context: vscode.ExtensionContext): string {
  const packageVersion = (context.extension.packageJSON as { version?: unknown }).version;

  return typeof packageVersion === "string" && packageVersion.length > 0
    ? packageVersion
    : "0.1.0";
}

export async function installTemplates(context: vscode.ExtensionContext): Promise<number> {
  const bundledTemplates = path.join(context.extensionUri.fsPath, "templates");
  const installedTemplates = path.join(context.globalStorageUri.fsPath, "templates");

  await ensureDirectory(context.globalStorageUri.fsPath);
  await fs.rm(installedTemplates, { recursive: true, force: true });
  await fs.cp(bundledTemplates, installedTemplates, { recursive: true });
  await fs.writeFile(
    path.join(installedTemplates, TEMPLATE_VERSION_FILE),
    `${templateSetVersion(context)}\n`,
    "utf8"
  );

  const templates = await listTemplateMetadata(installedTemplates);
  vscode.window.showInformationMessage(`Installed ${templates.length} ANA templates.`);

  return templates.length;
}

async function installedTemplatesAreCurrent(
  installedTemplates: string,
  currentVersion: string
): Promise<boolean> {
  try {
    const version = await fs.readFile(
      path.join(installedTemplates, TEMPLATE_VERSION_FILE),
      "utf8"
    );

    return version.trim() === currentVersion;
  } catch {
    return false;
  }
}

async function templateRoot(context: vscode.ExtensionContext): Promise<string> {
  const installedTemplates = path.join(context.globalStorageUri.fsPath, "templates");
  const currentVersion = templateSetVersion(context);

  if (
    (await pathExists(installedTemplates)) &&
    (await installedTemplatesAreCurrent(installedTemplates, currentVersion))
  ) {
    return installedTemplates;
  }

  return path.join(context.extensionUri.fsPath, "templates");
}

export async function createProjectFromTemplate(context: vscode.ExtensionContext): Promise<void> {
  const root = await templateRoot(context);
  const templates = await listTemplateMetadata(root);

  if (templates.length === 0) {
    vscode.window.showErrorMessage("No ANA templates are available.");
    return;
  }

  const selected = await vscode.window.showQuickPick(
    templates.map((template) => ({
      label: template.name,
      description: template.description,
      template
    })),
    { title: "Select ANA template" }
  );

  if (!selected) {
    return;
  }

  const projectName = await vscode.window.showInputBox({
    title: "ANA project name",
    prompt: "Enter the new project name",
    value: "ANA Game",
    validateInput(value) {
      return value.trim() ? undefined : "Project name is required.";
    }
  });

  if (!projectName) {
    return;
  }

  const parentFolders = await vscode.window.showOpenDialog({
    title: "Select parent folder for the ANA project",
    canSelectFolders: true,
    canSelectFiles: false,
    canSelectMany: false,
    openLabel: "Create Here"
  });

  const parentFolder = parentFolders?.[0];

  if (!parentFolder) {
    return;
  }

  const targetDir = path.join(parentFolder.fsPath, projectName.replace(/[/:\\]/g, "-"));

  if (await pathExists(targetDir)) {
    vscode.window.showErrorMessage(`Target folder already exists: ${targetDir}`);
    return;
  }

  await copyTemplateDirectory(path.join(root, selected.template.id), targetDir, projectName);
  await copyBundledSdk(context, targetDir);
  vscode.window.showInformationMessage(`Created ANA project: ${targetDir}`);
}
