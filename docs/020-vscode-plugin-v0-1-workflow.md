# Spec 020: VS Code Plugin v0.1 Workflow

## Purpose

The first ANA VS Code plugin release should make the existing ANA workflow
fast and reliable from inside the editor. It is not a replacement build system,
compiler frontend, asset converter, or C language server.

The plugin is a thin workflow layer over the project tools that already exist:

- `make`
- `ana-convert`
- `gadf`
- the m68k Amiga toolchain or the documented Docker fallback
- FS-UAE, WinUAE, or another configured emulator

The primary baseline for this version is a stock A1200 without Fast RAM. This
means the default workflow targets the current ANA A1200 build and profiling
assumptions, not AGA-specific graphics features. A500 and AGA profiles should
be planned in the configuration model, but they are not v0.1 promises.

## Product positioning

ANA for VS Code should streamline ANA game development in Visual Studio Code.
It should bring the most important ANA workflow actions into the editor:

- install or refresh ANA project templates
- create new ANA projects from the file menu or command palette
- build and run ADFs without memorizing Make targets
- open the ANA asset manifest or future Asset Workbench from an obvious button
  or command
- suggest known content paths when loading ANA assets from C code

This positioning should be visible in the extension README and marketplace
description. The plugin's value is workflow integration around ANA, not generic
C editing.

## Coordination boundaries

Another development stream may be changing ANA core, examples, asset formats,
or Make targets at the same time. The VS Code plugin work must avoid conflicts
with that stream.

The plugin should therefore live in its own directory, for example:

```text
tools/vscode-ana/
```

The plugin may read and invoke ANA core artifacts, but v0.1 work should not
modify:

- `include/`
- `src/`
- `examples/`
- existing Make target names
- existing asset format definitions

If the plugin needs a new core contract, the first change should be a small
documented interface request, such as structured `ana-convert` output or an
`ana-api.json` metadata file. Core/runtime changes should be handled by the ANA
core stream.

## MonoGame inspiration

ANA is intentionally close to the approachable XNA/MonoGame workflow, so the
plugin should study the VS Code MonoGame extensions as product references:

- `MonoGame for VS Code` exposes project creation from VS Code, installs
  templates, opens the MGCB editor from an editor button/command, and adds
  content-load suggestions.
- `MonoGame Content Builder (Editor)` shows the value of making the content
  pipeline accessible from the editor instead of forcing users to remember CLI
  details.

The ANA version should borrow the workflow shape, not the implementation
details. In v0.1 that means project templates, explicit install/check commands,
an obvious asset-pipeline entry point, and content-path suggestions where they
are cheap to implement.

## Goals

- Detect an ANA workspace and expose the most common build actions.
- Check the local toolchain and explain missing prerequisites clearly.
- Build host, Amiga, ADF, and A1200 ADF outputs through existing Make targets.
- Launch the selected ADF in a configured emulator.
- Create small ANA starter projects from maintained templates.
- Run basic asset conversion commands from file context menus.
- Keep all behavior transparent by showing the underlying command and output.

## Non-goals

- Reimplement `make`, `ana-convert`, or ADF packaging.
- Provide full C IntelliSense or a custom C parser.
- Predict exact frame rate or chip memory usage.
- Provide full emulator debugger control.
- Ship A500-safe or AGA graphics templates before ANA itself has stable
  support for those profiles.

## Workspace detection

The plugin should recognize an ANA workspace when one or more of these markers
exist:

- `include/ana/ana.h`
- `include/ana/ana_version.h`
- a top-level `Makefile` with ANA build targets
- an optional future `ana.json` project file

When detection is uncertain, commands may still be exposed, but the plugin
should ask for the ANA SDK path before running build or conversion actions.

## Target profiles

v0.1 should define the profile names early even if only one is fully active:

- `a1200-baseline`: supported default. Uses the current stock-A1200 workflow,
  including `-m68020` builds and 16-color PAL lores assets.
- `portable-amiga`: supported where the repository already has portable Amiga
  targets.
- `a500-experimental`: reserved. May appear in settings, but commands should
  mark it as unsupported unless matching build targets exist.
- `aga-experimental`: reserved. This is for future AGA graphics work, not the
  current A1200 CPU baseline.

The important distinction is that `a1200-baseline` is a machine/build profile,
while `aga-experimental` is a future graphics capability profile.

## Commands

The initial command palette surface should be small:

- `ANA: Check Toolchain`
- `ANA: Configure Paths`
- `ANA: Install Templates`
- `ANA: Create Project`
- `ANA: Build Host`
- `ANA: Run Host Example`
- `ANA: Build Amiga`
- `ANA: Build ADF`
- `ANA: Build A1200 ADF`
- `ANA: Run ADF in Emulator`
- `ANA: Convert Asset`
- `ANA: Open Asset Manifest`

Each command should print the exact shell command it runs in a dedicated ANA
output channel.

## Build mapping

The plugin should call existing Make targets:

| Plugin action | Default command |
| --- | --- |
| Build Host | `make all` |
| Test | `make test` |
| Build Amiga | `make amiga-examples` |
| Build ADF | `make adfs` |
| Build A1200 ADF | `make amiga-a1200-examples invaders-a1200-adf amaze-a1200-adf byte-brothers-a1200-adf` |
| Build Tools | `make tools` |

Project templates may override these mappings through a future `ana.json`.
Until that exists, the plugin should support workspace settings for custom
targets.

## Toolchain checks

`ANA: Check Toolchain` should detect:

- `make`
- a host C compiler
- `python3`
- `m68k-amigaos-gcc` and `m68k-amigaos-ar`, or Docker availability for the
  documented fallback image
- `vasmm68k_mot` when Amiga music backend assembly is required
- `gadf`
- `ana-convert`
- configured emulator executable

The check should return actionable status:

- `ok`
- `missing`
- `found through Docker fallback`
- `configured path does not exist`
- `found but version unknown`

## Settings

Recommended settings:

```json
{
  "ana.sdkPath": "",
  "ana.makePath": "make",
  "ana.anaConvertPath": "",
  "ana.adfToolPath": "",
  "ana.emulator": "fs-uae",
  "ana.emulatorPath": "",
  "ana.fsUaeConfigPath": "",
  "ana.defaultTargetProfile": "a1200-baseline",
  "ana.defaultAdf": "",
  "ana.showCommandsBeforeRun": true
}
```

The plugin should prefer explicit settings, then workspace-relative tools, then
tools on `PATH`.

## Extension package layout

The extension should be a normal VS Code extension project with its own Node
package and build lifecycle:

```text
tools/vscode-ana/
  package.json
  package-lock.json
  tsconfig.json
  src/
  test/
  fixtures/
  README.md
  CHANGELOG.md
```

Keeping the extension package below `tools/vscode-ana/` avoids adding Node
tooling to the ANA runtime root and reduces conflicts with core/runtime work.

## Versioning

The extension has its own `major.minor.build` version in
`tools/vscode-ana/package.json`. The initial public workflow line starts at
`0.1.x`; the final segment is treated as a monotonically increasing VSIX build
number.

Version rules:

- Extension version tracks extension behavior, not ANA core version directly.
- ANA SDK compatibility is declared separately, for example as
  `ana.supportedSdkRange`.
- The extension should read `ANA_VERSION_STRING` or future metadata when it
  needs to warn about SDK incompatibility.
- `engines.vscode` must be an explicit supported VS Code range, never `*`.
- Every release updates `CHANGELOG.md`.
- A packaged VSIX filename should include the extension version.
- Local `npm run package` increments the final version segment before building
  the VSIX.
- GitHub Actions sets the final version segment from `GITHUB_RUN_NUMBER` before
  packaging, so CI artifacts get unique build numbers without committing from
  the workflow.

If the project uses VS Code Marketplace pre-release publishing later, stable
and pre-release builds must use distinct version lines. A simple policy is even
minor versions for stable releases and odd minor versions for pre-release
channels.

## Build pipeline

The extension pipeline exists from v0.1, even before publishing to the
Marketplace.

Required local scripts:

```json
{
  "scripts": {
    "compile": "tsc -p .",
    "lint": "eslint src test",
    "test": "vscode-test",
    "bump-build": "node scripts/bump-build-version.mjs",
    "package": "npm run bump-build && npm run package:current",
    "package:current": "node scripts/package-current-version.mjs",
    "vscode:prepublish": "npm run compile"
  }
}
```

Recommended dependencies:

- TypeScript
- ESLint
- `@vscode/test-cli`
- `@vscode/test-electron`
- `@vscode/vsce`

The default CI job should run inside `tools/vscode-ana/`:

```sh
npm ci
npm run lint
npm run compile
npm test
node scripts/bump-build-version.mjs --build-number "$GITHUB_RUN_NUMBER"
npm run package:current
```

The package step produces an installable `.vsix` artifact. v0.1 CI should
upload the VSIX but should not publish automatically to the Marketplace.

The extension test suite should use fixtures and a mocked command runner for
normal CI. It should not build ANA core, mutate examples, or require a real
Amiga toolchain by default. A separate opt-in integration job may run against
the real ANA workspace and invoke `make`/`ana-convert` when the core build is
stable enough for that contract.

## Project templates

v0.1 templates should be derived from real maintained examples:

- `ANA Hello World`
- `ANA Sprite Demo`
- `ANA Scrolling Demo`
- `ANA Game Template`

Templates should include:

- source files
- a minimal asset manifest when needed
- a Makefile or project metadata that maps to ANA build commands
- README instructions for building and running

Templates should not claim A500-safe or AGA behavior in v0.1.

## Asset conversion

The file context menu should support:

- PNG/PPM to `.anaimg`
- PNG to `.anapal`
- spritesheet to `.anaimg` with frame size prompts
- bitmap font conversion when the user supplies font dimensions

The plugin should run `ana-convert` and show output. It should not duplicate
image parsing logic in v0.1.

As a MonoGame-inspired convenience, C source files should get lightweight
suggestions for known generated asset paths when calling ANA asset-loading
functions. This should be based on manifest/generated asset paths, not generic
C analysis.

## Emulator launch

The v0.1 emulator feature is launch-only:

- select ADF
- run configured emulator
- pass ADF path through command-line arguments or an emulator config template
- for FS-UAE, optionally pass a `.fs-uae` configuration file before the ADF
  override
- keep the emulator process output visible when available

Reset, attach-disk, screenshot, serial output, and debugger integration are
reserved for later versions.

## Acceptance criteria

- A user can open the ANA repository, run `ANA: Check Toolchain`, and see clear
  pass/fail results.
- A user can build the current ADF outputs from VS Code without typing Make
  targets manually.
- A user can launch a generated ADF in a configured emulator.
- A user can convert a PNG through `ana-convert` from the Explorer context
  menu.
- The plugin does not introduce a second, divergent build or asset pipeline.
- The extension can be linted, compiled, tested, and packaged into a `.vsix`
  without building or modifying ANA core/examples.
