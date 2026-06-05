import * as assert from "node:assert";
import { suite, test } from "mocha";
import {
  emulatorArgs,
  resolveEmulatorExecutable,
  type EmulatorProbe
} from "../src/emulator";

function probe(commands: Record<string, string>, existingPaths: Set<string> = new Set()): EmulatorProbe {
  return {
    async exists(filePath: string): Promise<boolean> {
      return existingPaths.has(filePath);
    },
    async findOnPath(command: string): Promise<string | undefined> {
      return commands[command];
    }
  };
}

suite("emulator resolution", () => {
  test("uses fs-uae from PATH when available", async () => {
    const resolved = await resolveEmulatorExecutable(
      "fs-uae",
      "",
      probe({ "fs-uae": "/usr/local/bin/fs-uae" })
    );

    assert.strictEqual(resolved.command, "/usr/local/bin/fs-uae");
  });

  test("detects the macOS FS-UAE app bundle executable", async () => {
    const resolved = await resolveEmulatorExecutable(
      "fs-uae",
      "",
      probe({}, new Set(["/Applications/FS-UAE.app/Contents/MacOS/fs-uae"]))
    );

    assert.strictEqual(resolved.command, "/Applications/FS-UAE.app/Contents/MacOS/fs-uae");
  });

  test("resolves an explicitly configured FS-UAE app bundle", async () => {
    const resolved = await resolveEmulatorExecutable(
      "fs-uae",
      "/Applications/FS-UAE.app",
      probe({}, new Set(["/Applications/FS-UAE.app/Contents/MacOS/fs-uae"]))
    );

    assert.strictEqual(resolved.command, "/Applications/FS-UAE.app/Contents/MacOS/fs-uae");
  });

  test("formats launch arguments per emulator", () => {
    assert.deepStrictEqual(emulatorArgs("fs-uae", "/tmp/game.adf"), [
      "--floppy-drive-0=/tmp/game.adf"
    ]);
    assert.deepStrictEqual(
      emulatorArgs("fs-uae", "/tmp/game.adf", {
        fsUaeConfigPath: "/Users/me/Documents/FS-UAE/Configurations/A1200.fs-uae"
      }),
      [
        "/Users/me/Documents/FS-UAE/Configurations/A1200.fs-uae",
        "--floppy-drive-0=/tmp/game.adf"
      ]
    );
    assert.deepStrictEqual(emulatorArgs("winuae", "C:\\game.adf"), ["-0", "C:\\game.adf"]);
  });
});
