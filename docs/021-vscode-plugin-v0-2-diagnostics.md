# Spec 021: VS Code Plugin v0.2 Diagnostics

## Purpose

The second ANA VS Code plugin release should make Amiga constraints visible
before a game reaches the emulator. The core value is early feedback on assets,
target profiles, build outputs, and rough memory budget.

The default target remains the stock A1200 baseline without Fast RAM. A500
support should start appearing as an experimental diagnostic profile, because
warnings can be useful before full runtime support is complete. AGA graphics
support should remain explicitly marked as experimental until ANA has matching
runtime and asset pipeline support.

## MonoGame inspiration

The MonoGame Content Builder is the closest reference for v0.2. It treats
content as a first-class project with source items, processor settings, build
commands, and rebuild behavior. ANA should take the same lesson: asset
conversion should be visible, inspectable, and diagnosable, not just a shell
command hidden behind the build.

Useful patterns to adapt:

- content tree organized by manifest entries
- per-asset settings shown in a property view
- build, rebuild, clean, and rebuild-selected actions
- diagnostics tied to source assets and processor/converter settings
- future extension points for custom asset processors

ANA's version must stay target-profile aware. Unlike MonoGame, the same source
asset may be valid for one Amiga profile and invalid for another.

## Goals

- Surface ANA-specific warnings in the VS Code Problems panel.
- Add deterministic asset diagnostics for color count, palette use, dimensions,
  and target compatibility.
- Add a memory budget view based on assets and build outputs.
- Add manifest preview and validation.
- Add basic build-output parsing for common toolchain failures.
- Keep diagnostics tied to documented target profiles.

## Non-goals

- Perfect cycle counting.
- Exact chip RAM accounting for every runtime allocation.
- Guaranteed 50 Hz prediction.
- Full source-level debugging.
- A finished A500 or AGA runtime.

## Data sources

Diagnostics should come from explicit data sources rather than hidden editor
logic:

- asset manifests
- source asset files
- generated `.anaimg`, `.anafnt`, `.anasnd`, `.anapal`, and copied `.mod`
  files
- `ana-convert` validation output
- Make output
- linker map files when available
- optional debug statistics emitted by ANA debug builds
- the selected target profile

If `ana-convert` does not yet expose structured validation output, this version
should define the expected interface and use best-effort text parsing until the
tool catches up.

## Custom processors

MonoGame's content pipeline supports custom importers and processors. ANA does
not need that abstraction immediately, but v0.2 should reserve a simple
extension point in the manifest model so future tools can add project-specific
asset processors without changing the VS Code plugin.

The first step is metadata only:

```text
processor enemy_tiles tools/enemy_tile_pack --version 1
image enemies enemies.png --processor enemy_tiles --palette game
```

The plugin should treat unknown processors conservatively:

- show that the asset depends on a custom processor
- include the processor version in cache/diagnostic metadata when available
- avoid claiming profile compatibility unless the processor reports structured
  output

## Target profiles

v0.2 diagnostics should understand these profile categories:

- `a1200-baseline`: default. Stock A1200 without Fast RAM, current ANA
  performance baseline, PAL lores, 16-color workflow unless the project says
  otherwise.
- `portable-amiga`: current portable Amiga build path.
- `a500-experimental`: diagnostic-only until ANA has stable A500 build and
  runtime targets. Warnings should be allowed to be stricter than A1200.
- `aga-experimental`: diagnostic-only until AGA graphics support exists in the
  runtime and asset pipeline.

Profile definitions should include at least:

- CPU assumption
- Chip RAM limit
- Fast RAM assumption
- supported bitplane/color limits
- hardware sprite constraints
- preferred render modes
- whether AGA-only features are allowed

## Problems panel diagnostics

Diagnostics should be reported with file, line, and severity when possible.

Recommended severities:

- `error`: the asset or build output cannot work for the selected target.
- `warning`: the asset may work but violates a profile budget or likely cost.
- `info`: the asset is valid but has a noteworthy target implication.

Initial diagnostics:

- image uses more colors than the selected profile allows
- image colors are not present in the selected `.anapal`
- transparent color is missing or ambiguous
- spritesheet dimensions do not divide cleanly into frames
- hardware sprite candidate has unsupported width or alignment
- tilemap dimensions exceed configured project limits
- generated asset is missing from the manifest output directory
- manifest references a missing source file
- `.mod` or `.anasnd` assets exceed configured budget
- AGA-only profile requirement is used while target is `a1200-baseline`
- A500 experimental profile would exceed Chip RAM budget

## Asset manifest validation

The plugin should parse ANA asset manifests enough to show:

- declared palettes
- images and spritesheets
- fonts
- sounds
- music files
- output names
- source paths
- conversion options

The manifest view should answer:

- What will be generated?
- Which palette does each image use?
- Which files are missing?
- Which assets are included in the memory budget?
- Which warnings apply under the selected target profile?

The view should also expose commands that match the content-builder mental
model:

- build all manifest outputs
- rebuild all manifest outputs
- rebuild selected asset
- clean generated manifest outputs
- reveal generated file
- copy equivalent `ana-convert` command

## Memory budget view

The v0.2 memory panel should be approximate but useful:

```text
Target: A1200 baseline
Chip RAM: 412 KB / configured budget
Fast RAM: 0 KB / 0 KB

Images: 138 KB
Fonts: 8 KB
Sounds: 44 KB
Music: 84 KB
Code/Data/BSS: 96 KB
ADF payload: 360 KB
```

The first version may not know exact runtime allocator behavior. It should
therefore label values as:

- `measured`
- `derived`
- `estimated`
- `unknown`

Asset sizes should be derived from generated files where possible. Code/data
sizes should come from linker output or generated map files when available.

## 50 Hz warnings

Frame-budget warnings should be conservative in v0.2.

Allowed warnings:

- selected render mode is known to be expensive for the target profile
- debug run reports frame rate below target
- full-frame redraw is selected for a scrolling game
- very large dirty regions are reported by debug stats
- asset dimensions imply large per-frame blits

Disallowed warnings:

- exact frame-rate promises without measurement
- cycle-accurate claims without profiler data
- A500 pass/fail claims before a supported A500 runtime profile exists

## Build output parsing

The plugin should recognize common failure categories:

- missing compiler
- missing Docker
- missing `gadf`
- missing `ana-convert`
- missing source asset
- failed conversion
- failed Amiga link
- failed ADF packaging

The output channel should keep raw logs. The Problems panel should show concise
diagnostics.

## Structured CLI output request

This plugin version should define, and preferably consume, structured tool
output from ANA tools:

```json
{
  "tool": "ana-convert",
  "toolVersion": "0.2.0",
  "schemaVersion": 1,
  "sdkVersion": "0.2.0",
  "diagnostics": [
    {
      "file": "assets/player.png",
      "severity": "warning",
      "code": "ANA_ASSET_TOO_MANY_COLORS",
      "message": "Image uses 21 colors; a1200-baseline allows 16.",
      "profile": "a1200-baseline"
    }
  ],
  "assets": [
    {
      "name": "player",
      "type": "image",
      "output": "build/assets/game/player.anaimg",
      "bytes": 4096
    }
  ]
}
```

The plugin should not require this format on day one, but the spec should make
it the preferred direction so the editor and CLI can evolve together.

Structured output is also the version boundary between the extension workstream
and the ANA core/tooling workstream. The extension should reject unsupported
schema versions with a clear message instead of silently parsing unknown data.
Text output may still be shown in the ANA output channel, but diagnostics and
budget views should prefer versioned structured output whenever available.

## Acceptance criteria

- Invalid or suspicious assets produce VS Code diagnostics before build/run.
- The memory budget view is useful for comparing projects and target profiles.
- Manifest validation catches missing files and incompatible conversion options.
- A1200 baseline warnings are stable and actionable.
- A500 and AGA diagnostics are clearly marked experimental and never imply
  runtime support that ANA does not yet provide.
