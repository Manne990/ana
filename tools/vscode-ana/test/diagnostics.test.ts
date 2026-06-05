import * as assert from "node:assert";
import { suite, test } from "mocha";
import { classifyBuildFailure } from "../src/diagnostics";

suite("build diagnostics", () => {
  test("classifies common build failures", () => {
    assert.strictEqual(
      classifyBuildFailure("make: *** No rule to make target 'amiga-a1200-lib'. Stop.").code,
      "ANA_BUILD_MAKE_TARGET"
    );
    assert.strictEqual(
      classifyBuildFailure("error: spawn gadf ENOENT").code,
      "ANA_BUILD_MISSING_GADF"
    );
    assert.strictEqual(
      classifyBuildFailure("collect2: error: ld returned 1 exit status").code,
      "ANA_BUILD_LINK"
    );
  });
});
