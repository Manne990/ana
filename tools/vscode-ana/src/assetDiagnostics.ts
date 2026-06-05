import * as fs from "node:fs/promises";
import * as path from "node:path";
import {
  manifestOutputPath,
  readAssetManifest,
  type ManifestAsset,
  type ParsedManifest
} from "./assetManifest";
import { readImageInfo, type ImageInfo } from "./imageInfo";
import { targetProfileFromId, type TargetProfile } from "./targetProfiles";

export type AnaDiagnosticSeverity = "error" | "warning" | "info";

export interface AnaDiagnostic {
  filePath: string;
  line: number;
  severity: AnaDiagnosticSeverity;
  code: string;
  message: string;
}

export interface BudgetLine {
  label: string;
  bytes: number;
  kind: "measured" | "derived" | "estimated" | "unknown";
  countsAgainstChip?: boolean;
}

export interface MemoryBudget {
  profile: TargetProfile;
  chipBytes: number;
  chipLimitBytes: number;
  lines: BudgetLine[];
}

export interface ManifestAnalysis {
  manifest: ParsedManifest;
  diagnostics: AnaDiagnostic[];
  budget: MemoryBudget;
  outputDir: string;
}

export interface WorkspaceAnalysis {
  diagnostics: AnaDiagnostic[];
  manifests: ManifestAnalysis[];
}

const IMAGE_TYPES = new Set(["image", "font", "palette"]);
const HARDWARE_SPRITE_WIDTH = 16;

async function pathExists(filePath: string): Promise<boolean> {
  try {
    await fs.access(filePath);
    return true;
  } catch {
    return false;
  }
}

async function fileSize(filePath: string): Promise<number | undefined> {
  try {
    return (await fs.stat(filePath)).size;
  } catch {
    return undefined;
  }
}

async function findFiles(root: string, relativeDir: string, predicate: (filePath: string) => boolean): Promise<string[]> {
  const dir = path.join(root, relativeDir);
  const found: string[] = [];

  async function walk(current: string): Promise<void> {
    let entries;

    try {
      entries = await fs.readdir(current, { withFileTypes: true });
    } catch {
      return;
    }

    for (const entry of entries) {
      const fullPath = path.join(current, entry.name);

      if (entry.isDirectory()) {
        await walk(fullPath);
      } else if (predicate(fullPath)) {
        found.push(fullPath);
      }
    }
  }

  await walk(dir);
  return found;
}

function optionNumber(asset: ManifestAsset, key: string): number | undefined {
  const value = asset.options[key];

  if (typeof value !== "string") {
    return undefined;
  }

  const parsed = Number(value);
  return Number.isFinite(parsed) ? parsed : undefined;
}

function optionString(asset: ManifestAsset, key: string): string | undefined {
  const value = asset.options[key];
  return typeof value === "string" ? value : undefined;
}

function normalizeColor(value: string): string | undefined {
  const trimmed = value.trim().toLowerCase();
  const short = trimmed.match(/^#([0-9a-f]{3})$/);

  if (short) {
    return `#${short[1].split("").map((char) => `${char}${char}`).join("")}`;
  }

  return /^#[0-9a-f]{6}$/.test(trimmed) ? trimmed : undefined;
}

async function readAnaPalette(filePath: string): Promise<Map<string, number> | undefined> {
  let raw: string;

  try {
    raw = await fs.readFile(filePath, "utf8");
  } catch {
    return undefined;
  }

  const lines = raw.split(/\r?\n/);

  if (lines[0]?.trim() !== "ANA_PALETTE 1") {
    return undefined;
  }

  const colors = new Map<string, number>();

  for (const line of lines.slice(1)) {
    const match = line.trim().match(/^\d+\s+(#[0-9a-fA-F]{6})$/);

    if (match) {
      colors.set(match[1].toLowerCase(), 1);
    }
  }

  return colors.size > 0 ? colors : undefined;
}

function sourceLineDiagnostic(
  asset: ManifestAsset,
  severity: AnaDiagnosticSeverity,
  code: string,
  message: string
): AnaDiagnostic {
  return {
    filePath: asset.sourcePath,
    line: 1,
    severity,
    code,
    message
  };
}

function manifestLineDiagnostic(
  manifest: ParsedManifest,
  line: number,
  severity: AnaDiagnosticSeverity,
  code: string,
  message: string
): AnaDiagnostic {
  return {
    filePath: manifest.filePath,
    line,
    severity,
    code,
    message
  };
}

async function readProjectExecutableName(root: string): Promise<string> {
  const makefilePath = path.join(root, "Makefile");

  try {
    const makefile = await fs.readFile(makefilePath, "utf8");
    const match = makefile.match(/^EXE_NAME\s*:?=\s*([A-Za-z0-9_-]+)/m);

    if (match) {
      return match[1];
    }
  } catch {
    // Fall through to folder-name fallback.
  }

  return path.basename(root)
    .trim()
    .toLowerCase()
    .replace(/[^a-z0-9_ -]/g, "")
    .replace(/[\s-]+/g, "_") || "ana_game";
}

export async function defaultAssetOutputDir(root: string): Promise<string> {
  return path.join(root, "build", "assets", await readProjectExecutableName(root), "assets");
}

function declaredPalettes(manifest: ParsedManifest): Set<string> {
  return new Set(manifest.assets.filter((asset) => asset.type === "palette").map((asset) => asset.name));
}

async function paletteColorsForAsset(
  paletteAsset: ManifestAsset,
  outputDir: string
): Promise<Map<string, number> | undefined> {
  const generatedPalette = await readAnaPalette(manifestOutputPath(outputDir, paletteAsset));

  if (generatedPalette) {
    return generatedPalette;
  }

  return (await readImageInfo(paletteAsset.sourcePath))?.colors;
}

async function paletteColorsForReference(
  manifest: ParsedManifest,
  outputDir: string,
  paletteNameOrPath: string
): Promise<Map<string, number> | undefined> {
  if (paletteNameOrPath.endsWith(".anapal")) {
    const palettePath = path.isAbsolute(paletteNameOrPath)
      ? paletteNameOrPath
      : path.resolve(manifest.directory, paletteNameOrPath);

    return readAnaPalette(palettePath);
  }

  const paletteAsset = manifest.assets.find(
    (asset) => asset.type === "palette" && asset.name === paletteNameOrPath
  );

  return paletteAsset ? paletteColorsForAsset(paletteAsset, outputDir) : undefined;
}

function addPaletteDiagnostics(
  manifest: ParsedManifest,
  profile: TargetProfile,
  diagnostics: AnaDiagnostic[]
): void {
  const palettes = declaredPalettes(manifest);

  for (const asset of manifest.assets) {
    if (asset.type === "palette") {
      const colors = optionNumber(asset, "colors");

      if (colors !== undefined && colors > profile.maxColors) {
        diagnostics.push(
          manifestLineDiagnostic(
            manifest,
            asset.line,
            profile.id === "aga-experimental" ? "info" : "warning",
            "ANA_ASSET_TOO_MANY_COLORS",
            `${asset.name} declares ${colors} colors; ${profile.id} allows ${profile.maxColors}.`
          )
        );
      }
    }

    if (asset.type === "image" || asset.type === "font") {
      const palette = optionString(asset, "palette");

      if (!palette) {
        diagnostics.push(
          manifestLineDiagnostic(
            manifest,
            asset.line,
            "warning",
            "ANA_ASSET_MISSING_PALETTE",
            `${asset.name} has no declared palette.`
          )
        );
      } else if (!palette.endsWith(".anapal") && !palettes.has(palette)) {
        diagnostics.push(
          manifestLineDiagnostic(
            manifest,
            asset.line,
            "error",
            "ANA_ASSET_UNKNOWN_PALETTE",
            `${asset.name} references unknown palette '${palette}'.`
          )
        );
      }
    }
  }
}

function addFrameDiagnostics(
  asset: ManifestAsset,
  imageInfo: ImageInfo,
  diagnostics: AnaDiagnostic[]
): void {
  const frameWidth = optionNumber(asset, "frame-width");
  const frameHeight = optionNumber(asset, "frame-height");

  if (frameWidth !== undefined && imageInfo.width % frameWidth !== 0) {
    diagnostics.push(
      sourceLineDiagnostic(
        asset,
        "error",
        "ANA_ASSET_FRAME_WIDTH",
        `${asset.name} width ${imageInfo.width}px is not divisible by frame width ${frameWidth}px.`
      )
    );
  }

  if (frameHeight !== undefined && imageInfo.height % frameHeight !== 0) {
    diagnostics.push(
      sourceLineDiagnostic(
        asset,
        "error",
        "ANA_ASSET_FRAME_HEIGHT",
        `${asset.name} height ${imageInfo.height}px is not divisible by frame height ${frameHeight}px.`
      )
    );
  }
}

async function addSourceDiagnostics(
  manifest: ParsedManifest,
  outputDir: string,
  profile: TargetProfile,
  diagnostics: AnaDiagnostic[]
): Promise<void> {
  for (const asset of manifest.assets) {
    if (!(await pathExists(asset.sourcePath))) {
      diagnostics.push(
        manifestLineDiagnostic(
          manifest,
          asset.line,
          "error",
          "ANA_ASSET_MISSING_SOURCE",
          `${asset.type} '${asset.name}' references missing source '${asset.source}'.`
        )
      );
      continue;
    }

    if (!IMAGE_TYPES.has(asset.type)) {
      continue;
    }

    const imageInfo = await readImageInfo(asset.sourcePath);

    if (!imageInfo) {
      continue;
    }

    if (imageInfo.colorCount !== undefined && imageInfo.colorCount > profile.maxColors) {
      diagnostics.push(
        sourceLineDiagnostic(
          asset,
          profile.id === "aga-experimental" ? "info" : "warning",
          "ANA_ASSET_TOO_MANY_COLORS",
          `${asset.name} uses ${imageInfo.colorCount} colors; ${profile.id} allows ${profile.maxColors}.`
        )
      );
    } else if (imageInfo.colorCountKind === "unknown") {
      diagnostics.push(
        sourceLineDiagnostic(
          asset,
          "info",
          "ANA_ASSET_COLOR_COUNT_UNKNOWN",
          `${asset.name} is a truecolor PNG; exact color count requires ana-convert validation.`
        )
      );
    }

    const palette = optionString(asset, "palette");

    if ((asset.type === "image" || asset.type === "font") && palette && imageInfo.colors) {
      const paletteColors = await paletteColorsForReference(manifest, outputDir, palette);

      if (paletteColors) {
        const missingColors = [...imageInfo.colors.keys()].filter((color) => !paletteColors.has(color));

        if (missingColors.length > 0) {
          diagnostics.push(
            sourceLineDiagnostic(
              asset,
              "error",
              "ANA_ASSET_COLOR_NOT_IN_PALETTE",
              `${asset.name} uses ${missingColors.length} color(s) not present in palette '${palette}'.`
            )
          );
        }
      } else if (palette.endsWith(".anapal")) {
        diagnostics.push(
          manifestLineDiagnostic(
            manifest,
            asset.line,
            "warning",
            "ANA_ASSET_PALETTE_UNREADABLE",
            `${asset.name} references palette '${palette}', but it could not be read.`
          )
        );
      }
    }

    if ((asset.type === "image" || asset.type === "font") && !asset.options.transparent) {
      diagnostics.push(
        manifestLineDiagnostic(
          manifest,
          asset.line,
          "info",
          "ANA_ASSET_NO_TRANSPARENT_COLOR",
          `${asset.name} has no transparent color option.`
        )
      );
    } else if (asset.type === "image" || asset.type === "font") {
      const transparent = optionString(asset, "transparent");
      const normalizedTransparent = transparent ? normalizeColor(transparent) : undefined;

      if (transparent && !normalizedTransparent) {
        diagnostics.push(
          manifestLineDiagnostic(
            manifest,
            asset.line,
            "error",
            "ANA_ASSET_INVALID_TRANSPARENT_COLOR",
            `${asset.name} uses invalid transparent color '${transparent}'.`
          )
        );
      } else if (normalizedTransparent && imageInfo.colors) {
        const transparentPixels = imageInfo.colors.get(normalizedTransparent) ?? 0;

        if (transparentPixels === 0) {
          diagnostics.push(
            sourceLineDiagnostic(
              asset,
              "warning",
              "ANA_ASSET_TRANSPARENT_COLOR_MISSING",
              `${asset.name} declares transparent color ${normalizedTransparent}, but that color is not present.`
            )
          );
        }
      }
    }

    if (asset.options["hardware-sprite"]) {
      const frameWidth = optionNumber(asset, "frame-width") ?? imageInfo.width;

      if (frameWidth !== HARDWARE_SPRITE_WIDTH) {
        diagnostics.push(
          sourceLineDiagnostic(
            asset,
            "warning",
            "ANA_ASSET_HARDWARE_SPRITE_WIDTH",
            `${asset.name} is marked as a hardware sprite candidate, but frame width is ${frameWidth}px; classic Amiga hardware sprites are ${HARDWARE_SPRITE_WIDTH}px wide.`
          )
        );
      }
    }

    addFrameDiagnostics(asset, imageInfo, diagnostics);
  }
}

async function addOutputDiagnostics(
  manifest: ParsedManifest,
  outputDir: string,
  diagnostics: AnaDiagnostic[]
): Promise<void> {
  for (const asset of manifest.assets) {
    const outputPath = manifestOutputPath(outputDir, asset);

    if (!(await pathExists(outputPath))) {
      diagnostics.push(
        manifestLineDiagnostic(
          manifest,
          asset.line,
          "info",
          "ANA_ASSET_OUTPUT_MISSING",
          `${asset.outputName} has not been generated yet.`
        )
      );
    }
  }
}

async function collectBudget(
  root: string,
  manifest: ParsedManifest,
  outputDir: string,
  profile: TargetProfile
): Promise<MemoryBudget> {
  const groups = new Map<string, BudgetLine>();

  function addBudgetLine(line: BudgetLine): void {
    const current = groups.get(line.label);

    if (current) {
      current.bytes += line.bytes;
      current.kind = current.kind === "measured" && line.kind === "measured" ? "measured" : "estimated";
      current.countsAgainstChip = current.countsAgainstChip !== false && line.countsAgainstChip !== false;
    } else {
      groups.set(line.label, line);
    }
  }

  for (const asset of manifest.assets) {
    const outputPath = manifestOutputPath(outputDir, asset);
    const generatedSize = await fileSize(outputPath);
    const sourceSize = await fileSize(asset.sourcePath);
    const bytes = generatedSize ?? sourceSize ?? 0;
    const kind = generatedSize === undefined ? sourceSize === undefined ? "unknown" : "estimated" : "measured";
    const label =
      asset.type === "image" || asset.type === "palette"
        ? "Images and palettes"
        : asset.type === "font"
          ? "Fonts"
          : asset.type === "sound"
            ? "Sounds"
            : "Music";
    addBudgetLine({ label, bytes, kind });
  }

  for (const adf of await findFiles(root, "build/adf", (filePath) => filePath.endsWith(".adf"))) {
    addBudgetLine({
      label: "ADF payload",
      bytes: await fileSize(adf) ?? 0,
      kind: "measured",
      countsAgainstChip: false
    });
  }

  for (const executable of await findFiles(root, "build/amiga-a1200", (filePath) => !path.basename(filePath).includes("."))) {
    addBudgetLine({
      label: "Code/Data/BSS",
      bytes: await fileSize(executable) ?? 0,
      kind: "measured"
    });
  }

  const lines = [...groups.values()].sort((a, b) => a.label.localeCompare(b.label));
  const chipBytes = lines.reduce(
    (sum, line) => sum + (line.countsAgainstChip === false ? 0 : line.bytes),
    0
  );

  return {
    profile,
    chipBytes,
    chipLimitBytes: profile.chipRamKb * 1024,
    lines
  };
}

function addBudgetDiagnostics(
  manifest: ParsedManifest,
  budget: MemoryBudget,
  diagnostics: AnaDiagnostic[]
): void {
  if (budget.chipBytes <= budget.chipLimitBytes) {
    return;
  }

  diagnostics.push(
    manifestLineDiagnostic(
      manifest,
      1,
      "warning",
      "ANA_MEMORY_CHIP_BUDGET",
      `Estimated asset payload uses ${Math.ceil(budget.chipBytes / 1024)} KB; ${budget.profile.id} budget is ${budget.profile.chipRamKb} KB.`
    )
  );
}

export async function analyzeManifest(
  root: string,
  manifestPath: string,
  targetProfileId: string
): Promise<ManifestAnalysis> {
  const profile = targetProfileFromId(targetProfileId);
  const manifest = await readAssetManifest(manifestPath);
  const outputDir = await defaultAssetOutputDir(root);
  const diagnostics: AnaDiagnostic[] = manifest.issues.map((issue) =>
    manifestLineDiagnostic(manifest, issue.line, "error", "ANA_MANIFEST_PARSE", issue.message)
  );

  addPaletteDiagnostics(manifest, profile, diagnostics);
  await addSourceDiagnostics(manifest, outputDir, profile, diagnostics);
  await addOutputDiagnostics(manifest, outputDir, diagnostics);

  const budget = await collectBudget(root, manifest, outputDir, profile);
  addBudgetDiagnostics(manifest, budget, diagnostics);

  if (profile.diagnosticOnly) {
    diagnostics.push(
      manifestLineDiagnostic(
        manifest,
        1,
        "info",
        "ANA_TARGET_EXPERIMENTAL",
        `${profile.id} diagnostics are experimental and do not imply runtime support.`
      )
    );
  }

  return {
    manifest,
    diagnostics,
    budget,
    outputDir
  };
}

export async function analyzeWorkspace(
  root: string,
  manifestPaths: string[],
  targetProfileId: string
): Promise<WorkspaceAnalysis> {
  const manifests = await Promise.all(
    manifestPaths.map((manifestPath) => analyzeManifest(root, manifestPath, targetProfileId))
  );

  return {
    manifests,
    diagnostics: manifests.flatMap((manifest) => manifest.diagnostics)
  };
}

export function formatBudget(budget: MemoryBudget): string[] {
  const lines = [
    `Target: ${budget.profile.label}${budget.profile.diagnosticOnly ? " (experimental diagnostics)" : ""}`,
    `CPU: ${budget.profile.cpu}`,
    `Chip RAM: ${Math.ceil(budget.chipBytes / 1024)} KB / ${budget.profile.chipRamKb} KB`,
    `Fast RAM: 0 KB / ${budget.profile.fastRamKb} KB`,
    ""
  ];

  for (const line of budget.lines) {
    lines.push(`${line.label}: ${Math.ceil(line.bytes / 1024)} KB (${line.kind})`);
  }

  if (budget.lines.length === 0) {
    lines.push("No assets found.");
  }

  if (budget.profile.notes.length > 0) {
    lines.push("");
    lines.push(...budget.profile.notes.map((note) => `Note: ${note}`));
  }

  return lines;
}
