import * as assert from "node:assert";
import { suite, test } from "mocha";
import { parseAssetManifestContent } from "../src/assetManifest";

suite("asset manifest parser", () => {
  test("parses ANA manifest entries and options", () => {
    const manifest = parseAssetManifestContent(
      [
        "ANA_ASSETS 1",
        "",
        "palette game palette.ppm --colors 16",
        "image player player.ppm --palette game --frame-width 16 --frame-height 16 --transparent #ff00ff",
        "font hud hud.ppm --colors 2 --char-width 6 --char-height 7 --chars 2",
        "sound hit hit.wav --rate 8000",
        "music theme theme.mod"
      ].join("\n"),
      "/tmp/project/assets/assets.ana"
    );

    assert.strictEqual(manifest.issues.length, 0);
    assert.strictEqual(manifest.assets.length, 5);
    assert.strictEqual(manifest.assets[1].type, "image");
    assert.strictEqual(manifest.assets[1].name, "player");
    assert.strictEqual(manifest.assets[1].outputName, "player.anaimg");
    assert.deepStrictEqual(manifest.assets[1].options, {
      palette: "game",
      "frame-width": "16",
      "frame-height": "16",
      transparent: "#ff00ff"
    });
  });

  test("reports invalid headers and unknown entries", () => {
    const manifest = parseAssetManifestContent(
      ["ANA 1", "processor enemy tools/enemy --version 1"].join("\n"),
      "/tmp/project/assets/assets.ana"
    );

    assert.deepStrictEqual(
      manifest.issues.map((issue) => issue.message),
      [
        "Expected manifest header 'ANA_ASSETS 1'.",
        "Unknown manifest entry type 'processor'."
      ]
    );
  });

  test("rejects unsafe names, duplicate names, and unsupported options", () => {
    const manifest = parseAssetManifestContent(
      [
        "ANA_ASSETS 1",
        "sound hit hit.wav",
        "sound ../outside hit.wav",
        "sound hit other.wav",
        "sound other other.wav --unknown 1"
      ].join("\n"),
      "/tmp/project/assets/assets.ana"
    );

    assert.deepStrictEqual(
      manifest.issues.map((issue) => issue.message),
      [
        "Unsafe asset name '../outside'. Use letters, digits, '_' or '-'.",
        "Duplicate asset name 'hit'.",
        "Unsupported sound option '--unknown'."
      ]
    );
  });

  test("requires the same structural options as ana-convert", () => {
    const manifest = parseAssetManifestContent(
      [
        "ANA_ASSETS 1",
        "palette game palette.png",
        "image player player.png --frame-width 16",
        "font hud hud.png --char-width 6 --char-height 7",
        "music theme theme.xm"
      ].join("\n"),
      "/tmp/project/assets/assets.ana"
    );

    assert.deepStrictEqual(
      manifest.issues.map((issue) => issue.message),
      [
        "palette entry requires conversion options.",
        "image frame width and height must be specified together.",
        "font entry requires '--chars'.",
        "music source must be a .mod file."
      ]
    );
  });
});
