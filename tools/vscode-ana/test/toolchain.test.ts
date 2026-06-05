import * as assert from "node:assert";
import { suite, test } from "mocha";
import { checkToolchain, type ToolProbe } from "../src/toolchain";

function probe(commands: Record<string, string>, existingPaths: Set<string> = new Set()): ToolProbe {
  return {
    async exists(filePath: string): Promise<boolean> {
      return existingPaths.has(filePath);
    },
    async findOnPath(command: string): Promise<string | undefined> {
      return commands[command];
    }
  };
}

suite("toolchain checks", () => {
  test("uses Docker fallback for missing m68k toolchain", async () => {
    const results = await checkToolchain(
      {
        workspaceRoot: "/workspace",
        sdkPath: "",
        makePath: "make",
        anaConvertPath: "",
        adfToolPath: "",
        emulator: "fs-uae",
        emulatorPath: ""
      },
      probe({
        make: "/usr/bin/make",
        cc: "/usr/bin/cc",
        python3: "/usr/bin/python3",
        docker: "/usr/bin/docker",
        gadf: "/usr/local/bin/gadf",
        "ana-convert": "/usr/local/bin/ana-convert",
        "fs-uae": "/usr/local/bin/fs-uae"
      })
    );

    assert.ok(results.some((result) => result.status === "found through Docker fallback"));
  });

  test("reports missing configured paths", async () => {
    const results = await checkToolchain(
      {
        workspaceRoot: "/workspace",
        sdkPath: "",
        makePath: "/missing/make",
        anaConvertPath: "",
        adfToolPath: "",
        emulator: "custom",
        emulatorPath: "/missing/emulator"
      },
      probe({})
    );

    assert.ok(
      results.some(
        (result) =>
          result.name === "make" && result.status === "configured path does not exist"
      )
    );
    assert.ok(
      results.some(
        (result) =>
          result.name === "emulator" && result.status === "configured path does not exist"
      )
    );
  });

  test("reports macOS FS-UAE app bundle as an available emulator", async () => {
    const results = await checkToolchain(
      {
        workspaceRoot: "/workspace",
        sdkPath: "",
        makePath: "make",
        anaConvertPath: "",
        adfToolPath: "",
        emulator: "fs-uae",
        emulatorPath: ""
      },
      probe(
        {
          make: "/usr/bin/make",
          cc: "/usr/bin/cc",
          python3: "/usr/bin/python3"
        },
        new Set(["/Applications/FS-UAE.app/Contents/MacOS/fs-uae"])
      )
    );

    assert.ok(
      results.some(
        (result) =>
          result.name === "emulator" &&
          result.status === "ok" &&
          result.detail === "/Applications/FS-UAE.app/Contents/MacOS/fs-uae"
      )
    );
  });
});
