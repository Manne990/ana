import * as assert from "node:assert";
import { suite, test } from "mocha";
import { isAnaAssetLoadContext } from "../src/contentCompletion";

suite("content completion context", () => {
  test("matches ANA asset loading calls", () => {
    assert.strictEqual(isAnaAssetLoadContext('    player = ana_load_image("'), true);
    assert.strictEqual(isAnaAssetLoadContext('ana_load_music("assets/'), true);
  });

  test("ignores unrelated strings", () => {
    assert.strictEqual(isAnaAssetLoadContext('printf("assets/'), false);
    assert.strictEqual(isAnaAssetLoadContext("ana_draw_image(player, x, y);"), false);
  });
});
