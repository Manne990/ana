import * as fs from "node:fs/promises";
import * as path from "node:path";

export type ManifestAssetType = "palette" | "image" | "font" | "sound" | "music";

export interface ManifestAsset {
  type: ManifestAssetType;
  name: string;
  source: string;
  sourcePath: string;
  outputName: string;
  outputExtension: string;
  options: Record<string, string | boolean>;
  line: number;
}

export interface ManifestParseIssue {
  line: number;
  message: string;
}

export interface ParsedManifest {
  filePath: string;
  directory: string;
  assets: ManifestAsset[];
  issues: ManifestParseIssue[];
}

const OUTPUT_EXTENSIONS: Record<ManifestAssetType, string> = {
  palette: ".anapal",
  image: ".anaimg",
  font: ".anafnt",
  sound: ".anasnd",
  music: ".mod"
};

function tokenizeLine(line: string): string[] {
  const tokens: string[] = [];
  let current = "";
  let quote: string | undefined;

  for (let i = 0; i < line.length; i++) {
    const char = line[i];

    if (quote) {
      if (char === quote) {
        quote = undefined;
      } else {
        current += char;
      }

      continue;
    }

    if (char === "#" && current.length === 0 && tokens.length === 0) {
      break;
    }

    if (char === "\"" || char === "'") {
      quote = char;
      continue;
    }

    if (/\s/.test(char)) {
      if (current.length > 0) {
        tokens.push(current);
        current = "";
      }

      continue;
    }

    current += char;
  }

  if (current.length > 0) {
    tokens.push(current);
  }

  return tokens;
}

function parseOptions(tokens: string[], startIndex: number): Record<string, string | boolean> {
  const options: Record<string, string | boolean> = {};

  for (let i = startIndex; i < tokens.length; i++) {
    const token = tokens[i];

    if (!token.startsWith("--")) {
      continue;
    }

    const key = token.slice(2);
    const next = tokens[i + 1];

    if (next && !next.startsWith("--")) {
      options[key] = next;
      i++;
    } else {
      options[key] = true;
    }
  }

  return options;
}

function outputExtension(type: ManifestAssetType): string {
  return OUTPUT_EXTENSIONS[type];
}

function parseAssetLine(
  tokens: string[],
  manifestDir: string,
  line: number
): ManifestAsset | ManifestParseIssue {
  const type = tokens[0] as ManifestAssetType;

  if (!Object.prototype.hasOwnProperty.call(OUTPUT_EXTENSIONS, type)) {
    return { line, message: `Unknown manifest entry type '${tokens[0]}'.` };
  }

  if (tokens.length < 3) {
    return { line, message: `${type} entry must include a name and source path.` };
  }

  const name = tokens[1];
  const source = tokens[2];

  return {
    type,
    name,
    source,
    sourcePath: path.resolve(manifestDir, source),
    outputName: `${name}${outputExtension(type)}`,
    outputExtension: outputExtension(type),
    options: parseOptions(tokens, 3),
    line
  };
}

export function parseAssetManifestContent(content: string, filePath: string): ParsedManifest {
  const directory = path.dirname(filePath);
  const assets: ManifestAsset[] = [];
  const issues: ManifestParseIssue[] = [];
  let sawHeader = false;

  for (const [index, rawLine] of content.split(/\r?\n/).entries()) {
    const line = index + 1;
    const tokens = tokenizeLine(rawLine.trim());

    if (tokens.length === 0) {
      continue;
    }

    if (!sawHeader) {
      if (tokens.length !== 2 || tokens[0] !== "ANA_ASSETS" || tokens[1] !== "1") {
        issues.push({ line, message: "Expected manifest header 'ANA_ASSETS 1'." });
      }

      sawHeader = true;
      continue;
    }

    const parsed = parseAssetLine(tokens, directory, line);

    if ("message" in parsed) {
      issues.push(parsed);
    } else {
      assets.push(parsed);
    }
  }

  if (!sawHeader) {
    issues.push({ line: 1, message: "Manifest is empty." });
  }

  return {
    filePath,
    directory,
    assets,
    issues
  };
}

export async function readAssetManifest(filePath: string): Promise<ParsedManifest> {
  return parseAssetManifestContent(await fs.readFile(filePath, "utf8"), filePath);
}

export function manifestOutputPath(outputDir: string, asset: ManifestAsset): string {
  return path.join(outputDir, asset.outputName);
}
