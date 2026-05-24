# Performance guide

ANA's goal is to stay close to practical vanilla C performance. That requires
both framework discipline and game-side discipline.

## Baseline target

The practical 0.1 baseline is stock A1200-class hardware for the Invaders
showcase. A500/OCS remains interesting, but it is not the current performance
floor for the complete showcase.

The API still keeps OCS/ECS constraints in mind:

- PAL lores
- 320x256
- 16 colors
- 4 bitplanes
- preconverted assets

## Game-side rules

Do:

- load assets in `load`, not in `update` or `draw`
- allocate fixed arrays up front
- keep object counts realistic
- draw only what changed when possible
- use small sprites and simple masks
- reuse sounds instead of loading per playback
- profile on emulator and real hardware when possible

Avoid:

- heap allocation in `update`
- heap allocation in `draw`
- runtime PNG or compression decoding
- full-screen redraws when only a few objects moved
- large overlapping masked images
- per-frame text redraws when the value did not change

## Renderer notes

The current Amiga build defaults to ANA's direct-present renderer:

```make
AMIGA_PRESENT_CFLAGS ?= -DANA_AMIGA_DIRECT_PRESENT
```

This path was chosen because the previous screen-buffer path was too expensive
for the current Invaders sample. It is fast enough for the showcase, but it
requires careful dirty-rect handling.

Debug builds can print timing counters:

```sh
make amiga-invaders-debug
make invaders-debug-adf
```

The output includes:

- average FPS
- dirty rects per frame
- converted pixels per frame
- present timing
- flip path counters

## Text

Bitmap text is not free. Keep HUD redraws explicit:

- redraw score only when score changes
- redraw lives only when lives change
- redraw status only when state changes

For stable HUD text, consider drawing it once and leaving it in the retained
buffer until it changes.

## Escape hatches

ANA should not prevent lower-level optimization. Acceptable escape hatches:

- direct C data layouts for hot game state
- renderer-specific batching
- Amiga-specific code paths
- inline assembler in small, measured hot paths

Use these when measurements show a real bottleneck. Keep the high-level API as
the normal path for examples unless the optimization is part of the lesson.
