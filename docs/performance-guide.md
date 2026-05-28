# Performance guide

ANA's goal is to stay close to practical vanilla C performance. That requires
both framework discipline and game-side discipline.

## Baseline target

The practical 0.1 baseline is a stock A1200 without Fast RAM for the Invaders
showcase. The target is stable 50 fps on that machine class, which means the
normal frame budget is 20 ms. A500/OCS remains interesting, but it is not the
current performance floor for the complete showcase.

Current measured baseline, 2026-05-26:

- Machine profile: A1200, no Fast RAM.
- Build/profile: Invaders A1200 debug measurement.
- Result: about 37 fps in active gameplay with debug timing enabled.
- Comparison: the same class of run with 1 MB Fast RAM reaches close to 50 fps;
  8 MB Fast RAM reached about 47 fps in one measured run.
- Main timing pressure: draw and present work, especially chunky-to-planar
  conversion and dirty object redraw.

This is now the optimization reference until a newer stock-A1200 measurement
replaces it.

Use the A1200 build targets for baseline measurements:

```sh
make amiga-a1200-examples
make amiga-invaders-a1200-debug
make invaders-a1200-adf invaders-a1200-debug-adf
```

Use `build/adf/invaders-a1200.adf` to judge normal game feel and
`build/adf/invaders-a1200-debug.adf` to collect timing data. The debug ADF
enables `ANA_DEBUG_STATS`, so it measures frame stages and render counters at
runtime. That diagnostic work is intentionally compiled out of release builds.

These targets compile C code with `-m68020` and keep the current 320x256,
16-color display profile. That treats the A1200 as the baseline CPU while
avoiding a higher bitplane count that would increase Chip RAM traffic.

The Fast RAM delta is important: even a small amount of Fast RAM is enough to
move the demo close to target speed. That suggests the current bottleneck is
strongly memory-bandwidth related rather than total memory size. On a stock
A1200 without Fast RAM, CPU code, data, and stack compete with display/audio DMA
in Chip RAM. Optimizations should therefore prioritize fewer Chip RAM
reads/writes, smaller dirty regions, less C2P work, and less per-frame renderer
bookkeeping before adding more gameplay features.

Additional A500 measurements, 2026-05-26:

- A500, 512 KB Chip RAM, 2 MB Fast RAM: about 12 fps.
- A500, 2 MB Chip RAM, 2 MB Fast RAM: about 15 fps.
- A500 with Terrible Fire 536, 512 KB Chip RAM, 1 MB Fast RAM: about 50 fps.

This indicates a second hard limit: the stock 68000-class CPU is not fast enough
for the current Invaders renderer, even when it has Fast RAM. More Chip RAM does
not add meaningful memory bandwidth; it mostly changes available address space.
The accelerated A500 result shows that CPU throughput, especially for dirty
object drawing and chunky-to-planar/present work, is also a primary constraint.

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

Use `ana_clear` for true full-frame resets. A fullscreen black `ana_fill_rect`
is just another draw operation and can leave stale buffer contents in
screen-buffer diagnostic builds.

If a renderer keeps per-buffer retained state, choose the slot from ANA's
presented frame count rather than game ticks. Game ticks can reset or skip;
the framebuffer rotation cannot.

A buffered debug ADF is available as a comparison point when investigating
frame pacing and visible judder:

```sh
make amiga-invaders-buffered-debug
make invaders-buffered-debug-adf
```

It uses the screen-buffer path instead of direct-present, so it may be smoother
while also costing more on Chip RAM-only systems.

Masked BOB clears can use `ana_bob_clear_previous_masked_x8_with_layers`.
This keeps the same byte-aligned dirty rectangle but only writes clear pixels
where the previous masked image was opaque. On Chip RAM-only systems this can
reduce draw-time memory traffic for sparse sprites and bullets.

Debug builds can print timing counters when the game sets
`ANA_Game.debug_stats = 1`:

```sh
make amiga-invaders-debug
make invaders-debug-adf
```

Release builds do not collect per-stage timing, dirty-area totals, or flip path
counters. They still track the presented frame count because retained renderers
use it to select their draw slot.

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

Retained repair callbacks should be filtered before they run. A moving BOB
only needs formation/shield repair layers when its previous dirty rectangle can
actually overlap those layers. Passing every retained layer to every BOB clear
adds CPU work even when the layer callback returns quickly.

Repair callbacks should also bound their own scans to the incoming dirty
rectangle. For example, shield repair should only inspect the shield cells that
can overlap the dirty rectangle instead of scanning the whole shield.

## Scrolling games

Dirty rectangles work well when the background is static. They are not enough
as the main model for a platform game, vertical shooter, or any game where the
camera moves most frames. Once the camera moves, most of the viewport can
effectively become dirty even if only a few sprites changed.

Byte Brothers currently uses a conservative fallback renderer for scrolling:
the camera is snapped to Amiga-friendly boundaries and the play viewport can be
redrawn when the camera changes. This keeps the image correct, but it is not
the final performance model.

The intended ANA direction is a framework-level scroll API:

- `ANA_Camera` for world/screen conversion and follow/continuous movement.
- `ANA_Tilemap` for drawing visible tiles without game code calculating tile
  intervals.
- `ANA_ScrollLayer` for tracking previous camera position, exposed strips, and
  redraw policy.

The long-term Amiga backend should move or hardware-scroll the existing
background and draw only newly exposed strips, while keeping HUD and sprites as
separate layers. See [Spec 017](017-scroll-camera-tilemap.md).

## Escape hatches

ANA should not prevent lower-level optimization. Acceptable escape hatches:

- direct C data layouts for hot game state
- renderer-specific batching
- Amiga-specific code paths
- inline assembler in small, measured hot paths

Use these when measurements show a real bottleneck. Keep the high-level API as
the normal path for examples unless the optimization is part of the lesson.
