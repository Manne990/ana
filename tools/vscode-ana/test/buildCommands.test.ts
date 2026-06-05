import * as assert from "node:assert";
import { suite, test } from "mocha";
import { formatCommand, getBuildCommand } from "../src/buildCommands";

suite("build commands", () => {
  test("maps A1200 ADF build to the documented Make targets", () => {
    const command = getBuildCommand("buildA1200Adf");

    assert.strictEqual(command.command, "make");
    assert.deepStrictEqual(command.args, [
      "amiga-a1200-examples",
      "invaders-a1200-adf",
      "amaze-a1200-adf",
      "byte-brothers-a1200-adf"
    ]);
  });

  test("formats commands with quoted arguments", () => {
    assert.strictEqual(
      formatCommand("make", ["ADFTOOL=/Users/me/My Tools/gadf", "adfs"]),
      'make "ADFTOOL=/Users/me/My Tools/gadf" adfs'
    );
  });
});
