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
- forcing static layers to redraw as part of unrelated moving-object updates

## Renderer notes

The current Amiga build defaults to ANA's direct-present renderer:

```make
AMIGA_PRESENT_CFLAGS ?= -DANA_AMIGA_DIRECT_PRESENT
```

This path was chosen because the previous screen-buffer path was too expensive
for the current Invaders sample. It is fast enough for the showcase, but it
requires careful dirty-rect handling.

Debug builds can print timing counters when the game sets
`ANA_Game.debug_stats = 1`:

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

## Spike frames

Average FPS can hide uneven frames. The Invaders renderer keeps shield redraws
separate from formation movement, and only touches shield damage once the
formation reaches the shield band. That keeps alien step frames closer to normal
frames instead of periodically redrawing unrelated static state. It also bounds
small mover repair work to the formation rows and columns a dirty rect can
actually touch, which avoids whole-formation scans when bullets move through
the playfield. Shield damage tracks dirty state per shield, so one bullet hit
does not force all four shields or the alien formation to redraw unless the
damaged shield actually overlaps the formation.

ANA's image renderer has dedicated fast paths for common small masked BOB
sizes. In particular, 16-pixel-wide masked images avoid per-pixel work for
fully transparent and fully opaque mask bytes. Small unmasked images and common
masked widths use pointer-increment loops instead of repeated row offset
calculation. Prefer asset widths that match these common byte-aligned sprite
sizes when practical.

## Escape hatches

ANA should not prevent lower-level optimization. Acceptable escape hatches:

- direct C data layouts for hot game state
- renderer-specific batching
- Amiga-specific code paths
- inline assembler in small, measured hot paths

Use these when measurements show a real bottleneck. Keep the high-level API as
the normal path for examples unless the optimization is part of the lesson.
