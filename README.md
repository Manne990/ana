# ANA

[![CI](https://github.com/Manne990/ana/actions/workflows/ci.yml/badge.svg)](https://github.com/Manne990/ana/actions/workflows/ci.yml)

ANA, short for "ANA's Now on Amiga", is an early-stage C game framework for
the Amiga.

The goal is to bring some of the approachable workflow from XNA and MonoGame to
Amiga development while still respecting Amiga constraints: fixed hardware,
limited memory, bitplanes, preconverted assets, and predictable runtime cost.

ANA is not intended to be a direct XNA clone. It should feel familiar in
structure, but be designed around C and Amiga hardware.

## Current Status

ANA 0.1.0 is tagged as the "Invaders Release": a small framework release
demonstrated by a playable Space Invaders-style example. Current `main` is
post-0.1 development toward 0.2 and adds `AMAze`, a compact maze-chase sample
with simple SFX and rudimentary pathfinding.

Implemented so far:

- fixed-step game loop
- PAL lores profile validation
- 16-color image loading and rendering
- bitmap font text
- semantic input directions/actions with keyboard mapping
- short SFX playback
- explicit music/SFX channel policy for Paula channels
- MOD music asset loading and Amiga playback through a vendored ProTracker
  replayer
- small helpers for rectangles, clamp, timers, retained BOBs, masked retained
  clear repair, and dirty labels
- host-side image/font conversion to `.anaimg` and `.anafnt`
- PNG/PPM source assets, `.anapal` palettes, `.mod` music assets, and simple
  asset manifests
- Amiga executable and ADF builds
- `hello`, `invaders`, and `amaze` examples

The practical showcase baseline is currently a stock A1200 without Fast RAM.
The performance goal for the complete Invaders demo is stable 50 fps on that
machine class. A500 performance remains useful feedback, but it is not the
current floor for the full showcase.

The Invaders example is split so `examples/invaders/main.c` stays close to the
normal ANA application shape, `invaders_game.c` owns the game rules, and
`invaders_render.c` owns the Amiga-oriented dirty rectangle state. Gameplay code
signals render invalidation through small render helpers instead of managing
draw slots directly. Its asset manifest also packages a small ProTracker MOD as
`assets/theme.mod`. The example plays music on title, clear, and game-over
screens, then stops it during active gameplay so the normal ADF keeps the arcade
loop close to the A1200 performance target.

`examples/amaze` follows the same readable structure as Invaders: `main.c`
registers the ANA callbacks, `amaze_game.c` owns the rules and pathfinding,
`amaze_render.c` owns retained tile redraws, and `amaze_assets.c` owns image
sprites, SFX, music, and channel policy. It draws a tile maze with converted PNG
sprites for the player, collectors, coins, and gold bags, uses a fixed-size BFS
distance map so chasers can take simple paths toward the player or flee while
corner power pellets are active, and loads four WAV-derived SFX assets plus a
small ProTracker MOD. The sample uses a fictional
business-and-tax-collector theme: dots are coins, power pellets are gold bags,
and the chasers are collectors. Its retained HUD shows score and lives, scoring
1 point per coin, 10 per gold bag, and 20 per captured collector. Captured
collectors immediately return to their normal color and behavior.

## Quick Build

Host build and tests:

```sh
make clean
make all
make test
```

Host tests require `python3` for small PNG fixture generation.

Amiga examples:

```sh
make clean
make amiga-examples
make adfs
```

A1200 baseline examples use the same assets and renderer but compile the C
code with `-m68020`. If the local `m68k-amigaos-*` tools are not on `PATH`,
the Makefile runs those compiler, assembler, and archiver commands through the
`amigadev/crosstools:m68k-amigaos-gcc10_amd64` Docker image automatically:

```sh
make amiga-a1200-examples
make invaders-a1200-adf
make amaze-a1200-adf
make amaze-a1200-debug-adf
```

ADF images are written to `build/adf/`:

- `build/adf/hello.adf`
- `build/adf/invaders.adf`
- `build/adf/amaze.adf`
- `build/adf/invaders-a1200.adf`
- `build/adf/amaze-a1200.adf`
- `build/adf/amaze-a1200-debug.adf`

Additional profiling ADFs can be built into the same directory with
`make invaders-debug-adf`, `make invaders-sync-adf`,
`make invaders-buffered-debug-adf`, `make invaders-a1200-debug-adf`, and
`make amaze-a1200-debug-adf` after their matching Amiga binaries have been
built.

Use the normal A1200 ADFs for gameplay checks. The debug ADFs compile in
runtime/render instrumentation and are intended for diagnostics.

The GitHub Actions workflow uploads the normal, A1200, and debug ADFs as the
`ana-example-adfs` artifact.

## Documentation

Start here:

- [Getting started](docs/getting-started.md)
- [API overview](docs/api-overview.md)
- [Asset pipeline guide](docs/asset-pipeline-guide.md)
- [Build and release package guide](docs/build-and-release.md)
- [Development routine](docs/development-routine.md)
- [Performance guide](docs/performance-guide.md)
- [Known limitations](docs/known-limitations.md)
- [ANA 0.1 release notes](docs/release-notes-0.1.md)

## Example Shape

ANA games are normal C programs that fill an `ANA_Game` struct and call
`ana_run`:

```c
#include <ana.h>

static int player_x = 100;

static void game_update(ANA_Time time)
{
    (void)time;

    if (ana_input_direction(ANA_INPUT_DEVICE_0, ANA_INPUT_LEFT)) {
        player_x--;
    }

    if (ana_input_direction(ANA_INPUT_DEVICE_0, ANA_INPUT_RIGHT)) {
        player_x++;
    }
}

static void game_draw(void)
{
    ana_clear(0);
    ana_fill_rect(2, player_x, 220, 16, 8);
}

int main(void)
{
    ANA_Game game = {0};

    game.init = 0;
    game.load = 0;
    game.update = game_update;
    game.draw = game_draw;
    game.shutdown = 0;
    game.width = ANA_DEFAULT_WIDTH;
    game.height = ANA_DEFAULT_HEIGHT;
    game.fps = ANA_DEFAULT_FPS;
    game.colors = ANA_DEFAULT_COLORS;
    game.screen_mode = ANA_SCREEN_PAL_LORES;
    game.debug_stats = 0;

    return ana_run(&game);
}
```

See [Getting started](docs/getting-started.md) for a fuller first-game example
with keyboard mapping.

## Asset Conversion

Build the converter:

```sh
make tools
```

Convert a PPM image:

```sh
build/tools/ana-convert/ana-convert image player.ppm \
  --out player.anaimg \
  --colors 16 \
  --transparent 255,0,255
```

Convert a PNG image with a shared palette:

```sh
build/tools/ana-convert/ana-convert palette palette.png \
  --out game.anapal \
  --colors 16

build/tools/ana-convert/ana-convert image player.png \
  --out player.anaimg \
  --palette game.anapal \
  --transparent "#ff00ff"
```

Build a manifest:

```sh
build/tools/ana-convert/ana-convert build assets.ana --out build/assets/game
```

The current public converter supports PNG and PPM P3/P6 image input for images
and fixed-width bitmap fonts, plus small text-based SFX recipes and PCM WAV
files that become `.anasnd` files. Manifest builds can also copy `.mod` music assets for
`ana_load_music`; examples should keep MOD files small enough for floppy load
time, Chip RAM, and frame-rate budgets. XNA/MonoGame import experiments are
planned for later work.

## Third-party Code

The Amiga music backend vendors Frank Wille's public-domain `ptplayer` replay
routine under `src/sound/vendor/ptplayer/`. It is assembled only for Amiga
targets and is not linked into host tests/tools.

## Release Package

Create a source package:

```sh
make release-package
```

The archive is written to:

```text
build/release/ana-0.2.0-dev.tar.gz
```

See [Build and release package guide](docs/build-and-release.md) for package
contents and binary artifact policy.

## 0.1 Specs

The 0.1 work is tracked in focused specs:

1. [Platform and design principles](docs/001-plattform-och-designprinciper.md)
2. [Project structure and build system](docs/002-projektstruktur-och-build-system.md)
3. [Core runtime and game loop](docs/003-core-runtime-och-game-loop.md)
4. [Graphics, screen, palette, and buffers](docs/004-grafik-skarm-palett-och-buffertar.md)
5. [Image rendering](docs/005-image-rendering.md)
6. [Asset pipeline 0.1](docs/006-asset-pipeline-01.md)
7. [Input](docs/007-input.md)
8. [Sound and SFX](docs/008-ljud-och-sfx.md)
9. [Bitmap font and text](docs/009-bitmapfont-och-text.md)
10. [Small game helpers](docs/010-sma-spelhjalpare.md)
11. [ANA Invaders showcase](docs/011-ana-invaders-showcase.md)
12. [Documentation and release package](docs/012-dokumentation-och-releasepaket.md)
13. [Asset pipeline 0.2](docs/013-asset-pipeline-02.md)
14. [Music and channel policy](docs/014-musik-och-kanalpolicy.md)
15. [Retained rendering helpers](docs/015-retained-rendering-helpers.md)

## License

MIT. See [LICENSE](LICENSE).
