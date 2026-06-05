import * as path from "node:path";
import * as vscode from "vscode";
import {
  analyzeManifest,
  analyzeWorkspace,
  defaultAssetOutputDir,
  formatBudget,
  type AnaDiagnostic,
  type ManifestAnalysis
} from "./assetDiagnostics";
import type { AnaExtensionConfig } from "./config";
import type { OutputSink } from "./runner";

type WorkspaceRootProvider = () => string | undefined;
type ConfigProvider = (root: string | undefined) => Promise<AnaExtensionConfig>;

export interface DiagnosticsController {
  refresh(root?: string): Promise<void>;
  validateManifest(resource?: vscode.Uri): Promise<void>;
  previewManifest(resource?: vscode.Uri): Promise<void>;
  copyManifestBuildCommand(resource?: vscode.Uri): Promise<void>;
  showMemoryBudget(): Promise<void>;
  reportBuildFailure(root: string, label: string, log: string): void;
  clearBuildFailure(root: string): void;
  dispose(): void;
}

function diagnosticSeverity(severity: AnaDiagnostic["severity"]): vscode.DiagnosticSeverity {
  switch (severity) {
    case "error":
      return vscode.DiagnosticSeverity.Error;
    case "warning":
      return vscode.DiagnosticSeverity.Warning;
    case "info":
      return vscode.DiagnosticSeverity.Information;
  }
}

function toVscodeDiagnostic(diagnostic: AnaDiagnostic): vscode.Diagnostic {
  const line = Math.max(0, diagnostic.line - 1);
  const vscodeDiagnostic = new vscode.Diagnostic(
    new vscode.Range(line, 0, line, Number.MAX_SAFE_INTEGER),
    diagnostic.message,
    diagnosticSeverity(diagnostic.severity)
  );

  vscodeDiagnostic.code = diagnostic.code;
  vscodeDiagnostic.source = "ANA";
  return vscodeDiagnostic;
}

function setDiagnostics(
  collection: vscode.DiagnosticCollection,
  diagnostics: AnaDiagnostic[]
): void {
  const grouped = new Map<string, vscode.Diagnostic[]>();

  collection.clear();

  for (const diagnostic of diagnostics) {
    const current = grouped.get(diagnostic.filePath) ?? [];
    current.push(toVscodeDiagnostic(diagnostic));
    grouped.set(diagnostic.filePath, current);
  }

  for (const [filePath, vscodeDiagnostics] of grouped) {
    collection.set(vscode.Uri.file(filePath), vscodeDiagnostics);
  }
}

export function classifyBuildFailure(log: string): { code: string; message: string } {
  if (/spawn .*ENOENT|command not found/i.test(log)) {
    if (/gadf/i.test(log)) {
      return {
        code: "ANA_BUILD_MISSING_GADF",
        message: "ADF packaging tool 'gadf' was not found."
      };
    }

    if (/ana-convert/i.test(log)) {
      return {
        code: "ANA_BUILD_MISSING_ANA_CONVERT",
        message: "ana-convert was not found or could not be built."
      };
    }

    if (/docker/i.test(log)) {
      return {
        code: "ANA_BUILD_MISSING_DOCKER",
        message: "Docker was not found for the Amiga toolchain fallback."
      };
    }

    if (/m68k-amigaos|gcc|cc/i.test(log)) {
      return {
        code: "ANA_BUILD_MISSING_COMPILER",
        message: "The compiler was not found."
      };
    }
  }

  if (/No rule to make target/i.test(log)) {
    return {
      code: "ANA_BUILD_MAKE_TARGET",
      message: "The workspace Makefile does not contain the requested ANA target."
    };
  }

  if (/ana-convert:|failed conversion|invalid .*asset|missing source/i.test(log)) {
    return {
      code: "ANA_BUILD_ASSET_CONVERSION",
      message: "Asset conversion failed. Check the ANA output channel for the converter log."
    };
  }

  if (/undefined reference|collect2: error|ld:|failed .*link/i.test(log)) {
    return {
      code: "ANA_BUILD_LINK",
      message: "Amiga link failed. Check compiler and linker output."
    };
  }

  if (/adf|bootblock|floppy/i.test(log)) {
    return {
      code: "ANA_BUILD_ADF",
      message: "ADF packaging failed. Check the ADF tool output."
    };
  }

  return {
    code: "ANA_BUILD_FAILED",
    message: "ANA build failed. Check the ANA output channel for details."
  };
}

async function findManifestPaths(root: string): Promise<string[]> {
  const uris = await vscode.workspace.findFiles(
    "**/*.ana",
    "**/{build,node_modules,.git,.ana-sdk}/**",
    100
  );

  return uris
    .map((uri) => uri.fsPath)
    .filter((filePath) => path.relative(root, filePath).startsWith("..") === false)
    .sort((a, b) => a.localeCompare(b));
}

async function pickManifest(root: string): Promise<vscode.Uri | undefined> {
  const manifests = await findManifestPaths(root);

  if (manifests.length === 0) {
    vscode.window.showWarningMessage("No ANA asset manifests found.");
    return undefined;
  }

  if (manifests.length === 1) {
    return vscode.Uri.file(manifests[0]);
  }

  const selected = await vscode.window.showQuickPick(
    manifests.map((filePath) => ({
      label: path.relative(root, filePath),
      filePath
    })),
    { title: "Select ANA asset manifest" }
  );

  return selected ? vscode.Uri.file(selected.filePath) : undefined;
}

function summarizeDiagnostics(manifests: ManifestAnalysis[]): string {
  const diagnostics = manifests.flatMap((manifest) => manifest.diagnostics);
  const errors = diagnostics.filter((diagnostic) => diagnostic.severity === "error").length;
  const warnings = diagnostics.filter((diagnostic) => diagnostic.severity === "warning").length;
  const info = diagnostics.filter((diagnostic) => diagnostic.severity === "info").length;

  return `${errors} error(s), ${warnings} warning(s), ${info} info item(s)`;
}

function escapeHtml(value: string): string {
  return value
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;");
}

function renderDiagnostics(diagnostics: AnaDiagnostic[], root: string): string {
  if (diagnostics.length === 0) {
    return "<p>No diagnostics.</p>";
  }

  return `<table>
    <thead><tr><th>Severity</th><th>Code</th><th>File</th><th>Message</th></tr></thead>
    <tbody>${diagnostics.map((diagnostic) => `<tr>
      <td>${escapeHtml(diagnostic.severity)}</td>
      <td><code>${escapeHtml(diagnostic.code)}</code></td>
      <td>${escapeHtml(path.relative(root, diagnostic.filePath))}:${diagnostic.line}</td>
      <td>${escapeHtml(diagnostic.message)}</td>
    </tr>`).join("")}</tbody>
  </table>`;
}

function renderManifestPreview(analysis: ManifestAnalysis, root: string): string {
  const assetRows = analysis.manifest.assets.map((asset) => `<tr>
    <td>${asset.line}</td>
    <td>${escapeHtml(asset.type)}</td>
    <td>${escapeHtml(asset.name)}</td>
    <td>${escapeHtml(asset.source)}</td>
    <td>${escapeHtml(path.relative(root, path.join(analysis.outputDir, asset.outputName)))}</td>
    <td>${escapeHtml(Object.entries(asset.options).map(([key, value]) => `--${key}${value === true ? "" : ` ${value}`}`).join(" "))}</td>
  </tr>`).join("");
  const budgetRows = formatBudget(analysis.budget).map((line) => `<li>${escapeHtml(line)}</li>`).join("");

  return `<!doctype html>
  <html lang="en">
  <head>
    <meta charset="utf-8">
    <style>
      body { font-family: var(--vscode-font-family); color: var(--vscode-foreground); padding: 16px; }
      table { border-collapse: collapse; width: 100%; margin: 12px 0 24px; }
      th, td { border-bottom: 1px solid var(--vscode-panel-border); padding: 6px 8px; text-align: left; vertical-align: top; }
      th { color: var(--vscode-descriptionForeground); font-weight: 600; }
      code { font-family: var(--vscode-editor-font-family); }
      .meta { color: var(--vscode-descriptionForeground); }
    </style>
  </head>
  <body>
    <h1>ANA Asset Manifest</h1>
    <p class="meta">${escapeHtml(path.relative(root, analysis.manifest.filePath))}</p>
    <p class="meta">Output: ${escapeHtml(path.relative(root, analysis.outputDir))}</p>

    <h2>Assets</h2>
    <table>
      <thead><tr><th>Line</th><th>Type</th><th>Name</th><th>Source</th><th>Generated output</th><th>Options</th></tr></thead>
      <tbody>${assetRows}</tbody>
    </table>

    <h2>Diagnostics</h2>
    ${renderDiagnostics(analysis.diagnostics, root)}

    <h2>Memory Budget</h2>
    <ul>${budgetRows}</ul>
  </body>
  </html>`;
}

async function selectedManifestUri(resource: vscode.Uri | undefined, root: string): Promise<vscode.Uri | undefined> {
  return resource?.fsPath.endsWith(".ana")
    ? resource
    : vscode.window.activeTextEditor?.document.uri.fsPath.endsWith(".ana")
      ? vscode.window.activeTextEditor.document.uri
      : await pickManifest(root);
}

export function createDiagnosticsController(
  context: vscode.ExtensionContext,
  workspaceRootProvider: WorkspaceRootProvider,
  configProvider: ConfigProvider,
  output: OutputSink
): DiagnosticsController {
  const collection = vscode.languages.createDiagnosticCollection("ana");

  async function refresh(root = workspaceRootProvider()): Promise<void> {
    if (!root) {
      collection.clear();
      return;
    }

    const config = await configProvider(root);
    const manifestPaths = await findManifestPaths(root);

    if (manifestPaths.length === 0) {
      collection.clear();
      return;
    }

    const analysis = await analyzeWorkspace(root, manifestPaths, config.defaultTargetProfile);
    setDiagnostics(collection, analysis.diagnostics);
  }

  async function validateManifest(resource?: vscode.Uri): Promise<void> {
    const root = workspaceRootProvider();

    if (!root) {
      vscode.window.showErrorMessage("Open an ANA workspace first.");
      return;
    }

    const manifestUri = await selectedManifestUri(resource, root);

    if (!manifestUri) {
      return;
    }

    const config = await configProvider(root);
    const analysis = await analyzeManifest(root, manifestUri.fsPath, config.defaultTargetProfile);
    setDiagnostics(collection, analysis.diagnostics);
    vscode.window.showInformationMessage(
      `ANA manifest diagnostics: ${summarizeDiagnostics([analysis])}.`
    );
  }

  async function previewManifest(resource?: vscode.Uri): Promise<void> {
    const root = workspaceRootProvider();

    if (!root) {
      vscode.window.showErrorMessage("Open an ANA workspace first.");
      return;
    }

    const manifestUri = await selectedManifestUri(resource, root);

    if (!manifestUri) {
      return;
    }

    const config = await configProvider(root);
    const analysis = await analyzeManifest(root, manifestUri.fsPath, config.defaultTargetProfile);
    setDiagnostics(collection, analysis.diagnostics);

    const panel = vscode.window.createWebviewPanel(
      "anaManifestPreview",
      `ANA Manifest: ${path.basename(manifestUri.fsPath)}`,
      vscode.ViewColumn.Beside,
      {}
    );

    panel.webview.html = renderManifestPreview(analysis, root);
  }

  async function copyManifestBuildCommand(resource?: vscode.Uri): Promise<void> {
    const root = workspaceRootProvider();

    if (!root) {
      vscode.window.showErrorMessage("Open an ANA workspace first.");
      return;
    }

    const manifestUri = await selectedManifestUri(resource, root);

    if (!manifestUri) {
      return;
    }

    const outputDir = await defaultAssetOutputDir(root);
    const command = `ana-convert build ${path.relative(root, manifestUri.fsPath)} --out ${path.relative(root, outputDir)}`;

    await vscode.env.clipboard.writeText(command);
    vscode.window.showInformationMessage("Copied ANA asset build command.");
  }

  async function showMemoryBudget(): Promise<void> {
    const root = workspaceRootProvider();

    if (!root) {
      vscode.window.showErrorMessage("Open an ANA workspace first.");
      return;
    }

    const config = await configProvider(root);
    const manifestPaths = await findManifestPaths(root);

    if (manifestPaths.length === 0) {
      vscode.window.showWarningMessage("No ANA asset manifests found.");
      return;
    }

    const analysis = await analyzeWorkspace(root, manifestPaths, config.defaultTargetProfile);
    setDiagnostics(collection, analysis.diagnostics);
    output.show(true);
    output.appendLine("");
    output.appendLine("== Memory Budget ==");

    for (const manifest of analysis.manifests) {
      output.appendLine("");
      output.appendLine(path.relative(root, manifest.manifest.filePath));
      output.appendLine(`output: ${path.relative(root, manifest.outputDir)}`);

      for (const line of formatBudget(manifest.budget)) {
        output.appendLine(line);
      }
    }

    vscode.window.showInformationMessage(
      `ANA memory budget updated: ${summarizeDiagnostics(analysis.manifests)}.`
    );
  }

  context.subscriptions.push(
    collection,
    vscode.workspace.onDidSaveTextDocument((document) => {
      const ext = path.extname(document.uri.fsPath).toLowerCase();

      if (ext === ".ana" || ext === ".png" || ext === ".ppm" || ext === ".mod") {
        void refresh();
      }
    }),
    vscode.workspace.onDidChangeConfiguration((event) => {
      if (event.affectsConfiguration("ana.defaultTargetProfile")) {
        void refresh();
      }
    })
  );

  return {
    refresh,
    validateManifest,
    previewManifest,
    copyManifestBuildCommand,
    showMemoryBudget,
    reportBuildFailure(root: string, label: string, log: string): void {
      const failure = classifyBuildFailure(log);
      const makefilePath = path.join(root, "Makefile");
      const diagnostic = toVscodeDiagnostic({
        filePath: makefilePath,
        line: 1,
        severity: "error",
        code: failure.code,
        message: `${label}: ${failure.message}`
      });

      collection.set(vscode.Uri.file(makefilePath), [diagnostic]);
    },
    clearBuildFailure(root: string): void {
      collection.set(vscode.Uri.file(path.join(root, "Makefile")), []);
    },
    dispose() {
      collection.dispose();
    }
  };
}
