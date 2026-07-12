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

const ALLOWED_OPTIONS: Record<ManifestAssetType, ReadonlySet<string>> = {
  palette: new Set(["colors"]),
  image: new Set([
    "palette",
    "colors",
    "frame-width",
    "frame-height",
    "transparent",
    "hardware-sprite"
  ]),
  font: new Set([
    "palette",
    "colors",
    "char-width",
    "char-height",
    "first-char",
    "chars",
    "transparent"
  ]),
  sound: new Set(["rate", "volume", "priority"]),
  music: new Set()
};

const POSITIVE_INTEGER_OPTIONS = new Set([
  "colors",
  "frame-width",
  "frame-height",
  "char-width",
  "char-height",
  "chars",
  "rate"
]);

const INTEGER_OPTIONS = new Set(["first-char", "volume", "priority"]);
const BOOLEAN_OPTIONS = new Set(["hardware-sprite"]);

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

function parseOptions(
  type: ManifestAssetType,
  tokens: string[],
  startIndex: number
): { options: Record<string, string>; issue?: string } {
  const options: Record<string, string> = {};

  for (let i = startIndex; i < tokens.length; i++) {
    const token = tokens[i];

    if (!token.startsWith("--")) {
      return { options, issue: `Unexpected value '${token}'.` };
    }

    const key = token.slice(2);
    const next = tokens[i + 1];

    if (!ALLOWED_OPTIONS[type].has(key)) {
      return { options, issue: `Unsupported ${type} option '--${key}'.` };
    }
    if (BOOLEAN_OPTIONS.has(key)) {
      options[key] = "true";
      continue;
    }
    if (!next || next.startsWith("--")) {
      return { options, issue: `Option '--${key}' requires a value.` };
    }
    if (Object.prototype.hasOwnProperty.call(options, key)) {
      return { options, issue: `Option '--${key}' is specified more than once.` };
    }
    if (POSITIVE_INTEGER_OPTIONS.has(key) && (!/^\d+$/.test(next) || Number(next) <= 0)) {
      return { options, issue: `Option '--${key}' requires a positive integer.` };
    }
    if (INTEGER_OPTIONS.has(key) && !/^-?\d+$/.test(next)) {
      return { options, issue: `Option '--${key}' requires an integer.` };
    }
    options[key] = next;
    i++;
  }

  return { options };
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

  if (!/^[A-Za-z0-9_-]+$/.test(name)) {
    return { line, message: `Unsafe asset name '${name}'. Use letters, digits, '_' or '-'.` };
  }
  if ((type === "palette" || type === "image" || type === "font") && tokens.length < 4) {
    return { line, message: `${type} entry requires conversion options.` };
  }
  if (type === "music" && tokens.length !== 3) {
    return { line, message: "music entry does not accept options." };
  }
  if (type === "music" && path.extname(source).toLowerCase() !== ".mod") {
    return { line, message: "music source must be a .mod file." };
  }

  const parsedOptions = parseOptions(type, tokens, 3);

  if (parsedOptions.issue) {
    return { line, message: parsedOptions.issue };
  }
  if (type === "palette" && parsedOptions.options.colors === undefined) {
    return { line, message: "palette entry requires '--colors'." };
  }
  if (type === "font") {
    for (const key of ["char-width", "char-height", "chars"]) {
      if (parsedOptions.options[key] === undefined) {
        return { line, message: `font entry requires '--${key}'.` };
      }
    }
  }
  const frameWidth = parsedOptions.options["frame-width"];
  const frameHeight = parsedOptions.options["frame-height"];
  if ((frameWidth === undefined) !== (frameHeight === undefined)) {
    return { line, message: "image frame width and height must be specified together." };
  }

  return {
    type,
    name,
    source,
    sourcePath: path.resolve(manifestDir, source),
    outputName: `${name}${outputExtension(type)}`,
    outputExtension: outputExtension(type),
    options: parsedOptions.options,
    line
  };
}

export function parseAssetManifestContent(content: string, filePath: string): ParsedManifest {
  const directory = path.dirname(filePath);
  const assets: ManifestAsset[] = [];
  const issues: ManifestParseIssue[] = [];
  const names = new Set<string>();
  let sawHeader = false;

  for (const [index, rawLine] of content.split(/\r?\n/).entries()) {
    const line = index + 1;
    if (rawLine.length >= 512) {
      issues.push({ line, message: "Manifest line exceeds 511 characters." });
      continue;
    }
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
      const assetKey = `${parsed.type}:${parsed.name}`;
      if (names.has(assetKey)) {
        issues.push({ line, message: `Duplicate asset name '${parsed.name}'.` });
        continue;
      }
      names.add(assetKey);
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
