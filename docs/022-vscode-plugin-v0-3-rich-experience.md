# Spec 022: VS Code Plugin v0.3 Rich Experience

## Purpose

The third ANA VS Code plugin release should feel like an Amiga-aware development
environment for ANA games. By this point the plugin can go beyond command
orchestration and diagnostics into ANA-specific editor help, emulator control,
asset previews, and packaging.

The stock A1200 baseline should remain the stable default. A500 and AGA support
should be added as real profiles only when the ANA runtime, build targets,
asset pipeline, and examples support them. Until then they remain experimental
profiles with explicit labels.

## MonoGame inspiration

v0.3 is where ANA can grow from a workflow extension into a small Amiga-aware
game-development environment. The strongest MonoGame references are:

- `MonoGame for VS Code`: new-project workflow, template installation, a clear
  content-editor command/button, and content-load suggestions.
- `MGCB Editor`: content project tree, property grid, undo/redo, build/rebuild
  actions, selected-item rebuilds, and custom importer/processor support.
- the newer MonoGame Content Builder project model: content can be built by a
  project-level tool, not only by a GUI editor.

The ANA equivalent should be an ANA Asset Workbench: a VS Code view that edits
and previews ANA manifests, target profiles, conversion settings, diagnostics,
and generated outputs while still delegating actual conversion to ANA tools.

## Goals

- Provide ANA API hovers, snippets, parameter help, and target notes.
- Show target-aware asset previews.
- Integrate emulator controls beyond simple launch.
- Display serial/debug/FPS output in VS Code panels.
- Support one-click release packaging.
- Promote A500 and AGA profiles when the framework is ready.
- Keep the plugin aligned with ANA metadata generated from the SDK.

## Non-goals

- Replace the official C/C++ extension.
- Hide Amiga constraints behind generic game-engine abstractions.
- Promise A500 or AGA compatibility without matching ANA support.
- Implement emulator-specific features that cannot be driven reliably.

## ANA API metadata

Rich editor features should be backed by generated metadata, not hard-coded
extension strings.

Recommended metadata file:

```json
{
  "version": "0.3.0",
  "symbols": [
    {
      "name": "ana_run",
      "kind": "function",
      "header": "ana.h",
      "summary": "Runs an ANA game lifecycle.",
      "parameters": [
        {
          "name": "game",
          "type": "ANA_Game*"
        }
      ],
      "targets": ["portable-amiga", "a1200-baseline"],
      "notes": ["Call once from main."]
    }
  ]
}
```

The metadata may be generated from headers, docs, or a maintained source file.
The plugin should consume it for:

- hover documentation
- parameter help
- snippets
- deprecated warnings
- target notes such as `A500 experimental`, `requires AGA`, `uses blitter`, or
  `debug-only`

## Snippets

Snippets should be ANA-specific and target-aware:

- initialize an `ANA_Game`
- map keyboard/joystick input
- load image asset
- draw image frame
- load and play SFX
- load and play MOD music
- configure render mode
- create dirty label
- create retained BOB
- initialize tile layer
- set camera
- configure double buffering or future display options

Snippets should avoid low-level Amiga setup unless ANA exposes a stable public
API for it.

## Emulator controls

v0.3 should support emulator-specific capability detection. FS-UAE and WinUAE
should not be treated as identical beyond launch.

Potential controls:

- launch with selected ADF/HDF
- reset
- attach or swap disk
- capture screenshot
- open emulator debugger
- open emulator config
- stream serial output
- show ANA debug statistics
- show measured FPS logs

Each emulator integration should declare supported capabilities. Unsupported
controls should be hidden or disabled with a clear reason.

## Debug and performance panels

The plugin should provide panels for:

- serial output
- ANA debug stats
- FPS/timing history
- render statistics
- asset budget changes between builds

The panels should preserve raw logs and also show summarized values. When a
debug ADF is used, the plugin should label it clearly so users do not confuse
instrumented performance with normal gameplay performance.

## Asset preview

Target-aware previews should show:

- source image
- indexed/palette-converted image
- transparent color
- palette swatches
- bitplane count
- frame grid for spritesheets
- estimated output size
- warnings for selected target profile

Future AGA previews may show expanded palette or bitplane behavior, but only
when ANA has a matching graphics pipeline.

## ANA Asset Workbench

The Asset Workbench is the long-term counterpart to MGCB Editor.

It should provide:

- editor title button and command-palette entry for opening the current
  manifest
- manifest tree grouped by palette, images, fonts, sounds, music, and custom
  processors
- property editor for selected manifest entries
- undo/redo for manifest edits
- add existing asset
- create new palette/font/sound recipe where ANA supports it
- build, rebuild, clean, and rebuild-selected actions
- source preview and converted preview
- target-profile compatibility badges
- generated output list
- equivalent CLI command display

The workbench must preserve the manifest as the source of truth. Users should
be able to edit the manifest by hand and see the workbench update.

## Packaging

One-click packaging should produce release-ready outputs without hiding the
underlying commands.

Supported package types:

- ADF
- HDF when ANA has a stable HDF flow
- source ZIP/tarball through existing release targets
- itch.io/GitHub release ZIP layout
- Aminet-style archive layout
- WHDLoad-like package only when there is a defined ANA-compatible structure

Packaging should include:

- executable
- generated assets
- README
- license
- target profile notes
- build metadata
- emulator-tested flag when available

## Profile maturity rules

Profiles should move through clear states:

- `reserved`: named for future planning, no user-facing promises
- `experimental`: diagnostics and optional builds exist, compatibility not
  guaranteed
- `supported`: runtime, build target, asset diagnostics, examples, and docs all
  exist

Expected path:

- `a1200-baseline`: supported
- `a500-experimental`: experimental until stable A500 examples and measurements
  exist
- `aga-experimental`: experimental until AGA graphics APIs, converter support,
  examples, and emulator profiles exist

This prevents the plugin from advertising features ahead of the framework.

## Project templates

v0.3 may add richer templates if the matching profile is mature:

- `A1200 Baseline Game`
- `A500 Experimental Game`
- `AGA Graphics Experiment`
- `Side-scrolling Game`
- `Single-screen Dirty Rendering Game`
- `Tilemap Game`

Each template should include target profile metadata, asset budgets, and
recommended emulator settings.

## Acceptance criteria

- ANA hover docs and snippets are useful without replacing the C/C++ extension.
- Emulator controls are capability-based and reliable for at least one primary
  emulator.
- Debug/FPS output can be viewed from VS Code after running a debug ADF.
- Asset preview shows target-aware conversion results and warnings.
- Packaging can create an artifact suitable for sharing.
- A500 and AGA support is exposed only at the maturity level the framework
  actually supports.
