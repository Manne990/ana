# ANA for VS Code

ANA for VS Code streamlines ANA Amiga game development in Visual Studio Code.
It brings the core ANA workflow into the editor without replacing the C/C++
extension or ANA's command-line tools.

## v0.1 Features

- Check ANA toolchain paths and show actionable status.
- Build host, Amiga, ADF, and A1200 ADF targets through existing Make targets.
- Launch a selected ADF in FS-UAE, WinUAE, or a custom emulator command.
- Launch FS-UAE with an optional `.fs-uae` configuration file while overriding
  the floppy drive with the selected ADF.
- Create new ANA projects from bundled templates.
- Copy a bundled ANA SDK snapshot into new projects as `.ana-sdk`.
- Open ANA asset manifests from commands and editor buttons.
- Convert PNG/PPM assets through `ana-convert`.
- Suggest generated ANA asset paths inside ANA asset-loading calls.

The default target profile is `a1200-baseline`: stock A1200 without Fast RAM.
A500 and AGA profiles are present only as future/experimental profile names in
v0.1.

## Commands

| Command | Description |
| --- | --- |
| `ANA: Check Toolchain` | Checks `make`, host C compiler, Python, Amiga toolchain or Docker fallback, `vasmm68k_mot`, `gadf`, `ana-convert`, and the configured emulator. Results are written to the ANA output channel. |
| `ANA: Configure Paths` | Opens ANA extension settings so SDK, tool, emulator, ADF, and FS-UAE configuration paths can be edited. |
| `ANA: Install Templates` | Copies the current bundled ANA project templates into the extension storage area. Old installed template sets are ignored if their version does not match the extension. |
| `ANA: Create Project` | Creates a new ANA starter project from a bundled template such as Hello World, Sprite Demo, Scrolling Demo, or Game Template, and copies the bundled SDK into the project as `.ana-sdk`. |
| `ANA: Build Host` | Runs the host build through the workspace Makefile, equivalent to `make all`. |
| `ANA: Run Host Example` | Runs one of the built host example binaries from `build/examples/`. |
| `ANA: Build Amiga` | Builds Amiga example executables through the workspace Makefile, equivalent to `make amiga-examples`. |
| `ANA: Build ADF` | Packages the normal example ADF images, equivalent to `make adfs`. |
| `ANA: Build A1200 ADF` | Builds A1200 baseline examples and ADFs through the documented A1200 Make targets. |
| `ANA: Run ADF in Emulator` | Lets you choose an ADF and launches it in FS-UAE, WinUAE, or a custom emulator. FS-UAE can use `ana.fsUaeConfigPath` and the selected ADF overrides `floppy_drive_0`. |
| `ANA: Convert Asset` | Converts selected PNG/PPM assets through `ana-convert`, including image, spritesheet, palette, and bitmap font flows. |
| `ANA: Open Asset Manifest` | Opens an `.ana` asset manifest from the active file, Explorer context menu, or a workspace picker. |

## Development

```sh
npm ci
npm run lint
npm run compile
npm test
npm run package
```

`npm run package` increments the patch/build number and creates
`ana-vscode-<version>.vsix`. Use `npm run package:current` only when packaging
the version already recorded in `package.json`.
