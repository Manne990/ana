# Changelog

## 0.2.0

- Added ANA asset manifest diagnostics for the VS Code Problems panel.
- Added manifest validation for missing sources, unknown palettes, generated
  outputs, frame dimensions, and target-profile implications.
- Added exact local palette matching for PPM and paletted PNG assets when ANA
  palette data is available.
- Added transparent color validation and hardware-sprite candidate width
  warnings.
- Added a structured asset manifest preview.
- Added a command for copying the equivalent manifest build command.
- Added an approximate memory budget command for selected target profiles.
- Marked A500 and AGA diagnostics as experimental when those profiles are used.

## 0.1.x

- Added the initial ANA workflow extension for VS Code.
- Added toolchain checks, build commands, ADF launch support, templates, asset
  conversion, manifest opening, content-path suggestions, tests, and VSIX
  packaging.
- Bundled an ANA SDK snapshot so generated projects build against their local
  `.ana-sdk` instead of a hardcoded repository path.
- Treat the patch segment as the extension build number for local and CI VSIX
  packages.
- Added a packaged extension icon for the VS Code Extensions view and header.
