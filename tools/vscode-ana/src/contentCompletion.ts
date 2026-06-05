import * as path from "node:path";
import * as vscode from "vscode";

const ASSET_LOAD_CALL =
  /ana_load_(image|font|sound|music)\s*\(\s*"([^"]*)?$/;

const ASSET_EXTENSIONS = new Set([".anaimg", ".anafnt", ".anasnd", ".mod", ".anapal"]);

export function isAnaAssetLoadContext(linePrefix: string): boolean {
  return ASSET_LOAD_CALL.test(linePrefix);
}

function assetLabel(workspaceRoot: string, filePath: string): string {
  const relative = path.relative(workspaceRoot, filePath).replace(/\\/g, "/");
  const buildAssetPrefix = "build/assets/";
  const buildAssetIndex = relative.indexOf(buildAssetPrefix);

  if (buildAssetIndex >= 0) {
    const parts = relative.slice(buildAssetIndex + buildAssetPrefix.length).split("/");
    const assetIndex = parts.indexOf("assets");

    if (assetIndex >= 0) {
      return parts.slice(assetIndex).join("/");
    }
  }

  return relative;
}

async function collectAssetPaths(): Promise<string[]> {
  const workspaceRoot = vscode.workspace.workspaceFolders?.[0]?.uri.fsPath;

  if (!workspaceRoot) {
    return [];
  }

  const files = await vscode.workspace.findFiles(
    "**/*.{anaimg,anafnt,anasnd,mod,anapal}",
    "**/{node_modules,.git}/**",
    300
  );

  const labels = new Set<string>();

  for (const file of files) {
    if (!ASSET_EXTENSIONS.has(path.extname(file.fsPath).toLowerCase())) {
      continue;
    }

    labels.add(assetLabel(workspaceRoot, file.fsPath));
  }

  return [...labels].sort();
}

export function registerContentCompletion(context: vscode.ExtensionContext): void {
  const provider: vscode.CompletionItemProvider = {
    async provideCompletionItems(document, position) {
      const linePrefix = document.lineAt(position).text.slice(0, position.character);

      if (!isAnaAssetLoadContext(linePrefix)) {
        return undefined;
      }

      const assets = await collectAssetPaths();

      return assets.map((asset) => {
        const item = new vscode.CompletionItem(asset, vscode.CompletionItemKind.File);
        item.insertText = asset;
        item.detail = "ANA asset";
        return item;
      });
    }
  };

  context.subscriptions.push(
    vscode.languages.registerCompletionItemProvider(
      [
        { language: "c", scheme: "file" },
        { language: "cpp", scheme: "file" }
      ],
      provider,
      "\"",
      "/"
    )
  );
}
