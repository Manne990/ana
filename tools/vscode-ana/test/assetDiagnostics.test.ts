import * as assert from "node:assert";
import * as fs from "node:fs/promises";
import * as os from "node:os";
import * as path from "node:path";
import { suite, test } from "mocha";
import { analyzeManifest, formatBudget } from "../src/assetDiagnostics";

async function tempDir(): Promise<string> {
  return fs.mkdtemp(path.join(os.tmpdir(), "ana-vscode-diagnostics-"));
}

async function writeProject(files: Record<string, string>): Promise<string> {
  const root = await tempDir();

  for (const [relativePath, content] of Object.entries(files)) {
    const fullPath = path.join(root, relativePath);
    await fs.mkdir(path.dirname(fullPath), { recursive: true });
    await fs.writeFile(fullPath, content, "utf8");
  }

  return root;
}

suite("asset diagnostics", () => {
  test("reports manifest and asset diagnostics", async () => {
    const root = await writeProject({
      Makefile: "EXE_NAME := diag_game\n",
      "assets/player.ppm": [
        "P3",
        "2 2",
        "255",
        "255 0 0",
        "0 255 0",
        "0 0 255",
        "255 255 255"
      ].join("\n"),
      "assets/assets.ana": [
        "ANA_ASSETS 1",
        "palette game player.ppm --colors 32",
        "image player player.ppm --palette missing --frame-width 3 --transparent #ff00ff",
        "sound missing missing.wav"
      ].join("\n")
    });

    const analysis = await analyzeManifest(root, path.join(root, "assets", "assets.ana"), "a1200-baseline");
    const codes = analysis.diagnostics.map((diagnostic) => diagnostic.code);

    assert.ok(codes.includes("ANA_ASSET_TOO_MANY_COLORS"));
    assert.ok(codes.includes("ANA_ASSET_UNKNOWN_PALETTE"));
    assert.ok(codes.includes("ANA_ASSET_FRAME_WIDTH"));
    assert.ok(codes.includes("ANA_ASSET_MISSING_SOURCE"));
    assert.ok(codes.includes("ANA_ASSET_OUTPUT_MISSING"));
  });

  test("formats an experimental profile memory budget", async () => {
    const root = await writeProject({
      Makefile: "EXE_NAME := budget_game\n",
      "assets/player.ppm": [
        "P3",
        "1 1",
        "255",
        "255 0 255"
      ].join("\n"),
      "assets/assets.ana": [
        "ANA_ASSETS 1",
        "palette game player.ppm --colors 16",
        "image player player.ppm --palette game --transparent #ff00ff"
      ].join("\n")
    });

    const analysis = await analyzeManifest(root, path.join(root, "assets", "assets.ana"), "a500-experimental");
    const budget = formatBudget(analysis.budget).join("\n");

    assert.match(budget, /A500 experimental/);
    assert.match(budget, /experimental diagnostics/);
    assert.ok(
      analysis.diagnostics.some((diagnostic) => diagnostic.code === "ANA_TARGET_EXPERIMENTAL")
    );
  });

  test("checks palette colors, transparent colors, hardware sprite hints, and build output budget", async () => {
    const root = await writeProject({
      Makefile: "EXE_NAME := complete_game\n",
      "assets/palette.ppm": [
        "P3",
        "1 1",
        "255",
        "255 0 0"
      ].join("\n"),
      "assets/player.ppm": [
        "P3",
        "2 1",
        "255",
        "255 0 0",
        "0 0 255"
      ].join("\n"),
      "assets/assets.ana": [
        "ANA_ASSETS 1",
        "palette game palette.ppm --colors 16",
        "image player player.ppm --palette game --frame-width 20 --transparent #00ff00 --hardware-sprite"
      ].join("\n"),
      "build/adf/complete_game.adf": "adf-payload",
      "build/amiga-a1200/complete_game": "amiga-executable"
    });

    const analysis = await analyzeManifest(root, path.join(root, "assets", "assets.ana"), "a1200-baseline");
    const codes = analysis.diagnostics.map((diagnostic) => diagnostic.code);
    const budget = formatBudget(analysis.budget).join("\n");

    assert.ok(codes.includes("ANA_ASSET_COLOR_NOT_IN_PALETTE"));
    assert.ok(codes.includes("ANA_ASSET_TRANSPARENT_COLOR_MISSING"));
    assert.ok(codes.includes("ANA_ASSET_HARDWARE_SPRITE_WIDTH"));
    assert.match(budget, /ADF payload/);
    assert.match(budget, /Code\/Data\/BSS/);
  });
});
