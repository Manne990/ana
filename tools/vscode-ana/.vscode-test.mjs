import { defineConfig } from "@vscode/test-cli";

export default defineConfig({
  files: "dist/test/**/*.test.js",
  workspaceFolder: "./fixtures/ana-workspace",
  mocha: {
    ui: "tdd",
    timeout: 20000
  }
});
