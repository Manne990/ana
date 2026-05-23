# ANA

[![CI](https://github.com/Manne990/ana/actions/workflows/ci.yml/badge.svg)](https://github.com/Manne990/ana/actions/workflows/ci.yml)

ANA, short for "ANA's Now on Amiga", is an early-stage C game framework for the Amiga.

The goal is to bring some of the approachable workflow from XNA and MonoGame to Amiga development, while staying close to the hardware and keeping runtime overhead predictable. ANA is not intended to be a direct XNA clone. It should feel familiar in structure, but be designed around C, Amiga constraints, and preconverted assets.

## Vision

ANA 0.1 is planned as the "Invaders Release": a small, focused framework release demonstrated by a complete Space Invaders-style example game.

The first release should provide:

- a C runtime library
- a simple fixed-step game loop
- Amiga-friendly graphics setup
- image rendering
- joystick and keyboard input
- short sound effects
- bitmap font text
- a host-side asset conversion tool
- small helper APIs for common game code
- a polished `ana-invaders` showcase

## Design Goals

- Keep the public API small and readable.
- Avoid hidden allocations during `update` and `draw`.
- Use preconverted assets instead of expensive runtime decoding.
- Target OCS/ECS first, with PAL 320x256 as the initial baseline.
- Keep performance close to hand-written vanilla C.
- Make the easy path pleasant without blocking low-level C or assembler escape hatches.
- Prefer beginner-friendly names like `ANA_Image`, `ANA_Font`, and `ANA_Sound` in the public API.
- Let example games drive the framework instead of designing a large engine up front.

## API Shape

ANA should make simple games read like game code:

```c
#include <ana.h>

static ANA_Image player;
static int player_x = 100;
static int player_y = 80;

void game_init(void) {
    ana_input_clear_key_map();
    ana_input_map_key_to_direction(ANA_KEY_LEFT, ANA_INPUT_DEVICE_0, ANA_INPUT_LEFT);
    ana_input_map_key_to_direction(ANA_KEY_A, ANA_INPUT_DEVICE_0, ANA_INPUT_LEFT);
    ana_input_map_key_to_direction(ANA_KEY_RIGHT, ANA_INPUT_DEVICE_0, ANA_INPUT_RIGHT);
    ana_input_map_key_to_direction(ANA_KEY_D, ANA_INPUT_DEVICE_0, ANA_INPUT_RIGHT);
    ana_input_map_key_to_action(ANA_KEY_SPACE, ANA_INPUT_DEVICE_0, ANA_ACTION_1);
}

void game_load(void) {
    player = ana_load_image("player.anaimg");
}

void game_update(ANA_Time time) {
    if (ana_input_direction(ANA_INPUT_DEVICE_0, ANA_INPUT_RIGHT)) {
        player_x++;
    }

    if (ana_input_direction(ANA_INPUT_DEVICE_0, ANA_INPUT_LEFT)) {
        player_x--;
    }
}

void game_draw(void) {
    ana_clear(0);
    ana_draw_image(player, player_x, player_y);
}

int main(void) {
    ANA_Game game = {
        .init = game_init,
        .load = game_load,
        .update = game_update,
        .draw = game_draw,

        .width = 320,
        .height = 256,
        .fps = 50,
        .colors = 16,
        .screen_mode = ANA_SCREEN_PAL_LORES
    };

    return ana_run(&game);
}
```

The implementation can still use Amiga-specific concepts such as bitplanes, BOBs, blitter operations, hardware sprites, or custom assembler routines. Those details should remain available for optimization and advanced use, but they should not be required for the first successful game.

## ANA 0.1 Specs

The initial 0.1 work is split into focused specs:

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

## Current Status

ANA is currently in the early implementation phase. The repository contains the initial 0.1 specification documents plus the first C library foundation: platform/profile validation, a minimal game loop, visible Amiga PAL lores graphics, image loading/rendering, bitmap font text, input mapping, short SFX playback, example skeletons, and the first host-side asset conversion flow.

## Build

The current development build uses a simple Makefile:

```sh
make all
make test
```

This builds `libana.a`, the host-side `ana-convert` tool, and the current example programs under `build/`.

## Asset Conversion

The first `ana-convert` implementation converts PPM P3/P6 images into ANA's preconverted `.anaimg` format:

```sh
build/tools/ana-convert/ana-convert image player.ppm --out player.anaimg --colors 16 --transparent 255,0,255
build/tools/ana-convert/ana-convert image explosion_sheet.ppm --out explosion.anaimg --colors 16 --frame-width 16 --frame-height 16
```

PNG support is still planned, but PPM keeps the current pipeline dependency-free while the runtime and asset format settle.

The CI pipeline builds and tests the host code as strict C89 with both GCC and Clang. The Amiga-target build uses GNU89 mode because the m68k Amiga toolchain headers use GCC extensions.

The CI pipeline also builds Amiga-targeted example executables and packages them as ADF images. Download the `ana-example-adfs` artifact from a successful workflow run to try the current examples in an emulator.

For local ADF builds you need `m68k-amigaos-gcc`, `m68k-amigaos-ar`, and `gadf` available on your path:

```sh
make amiga-examples
make adfs
```

Local ADF images are written to `build/adf/`, for example
`build/adf/invaders.adf`.

The normal Amiga build uses ANA's direct-present renderer by default. That
path updates the visible bitmap with final dirty-rect contents and avoids the
screen-buffer safe-wait cost that was too expensive for the current Invaders
sample.

For performance investigations you can build a debug-statistics Invaders ADF:

```sh
make amiga-invaders-debug
make invaders-debug-adf
```

This writes `build/adf/invaders-debug.adf` and keeps the normal
`build/adf/invaders.adf` output unchanged. It uses the same renderer as the
normal build, but prints timing counters when the program exits.

To test the direct-present path synchronized before writing to the visible
bitmap:

```sh
make amiga-invaders-sync
make invaders-sync-adf
```

This writes `build/adf/invaders-sync.adf`. It is intended to compare
visual stability against the default direct-present path.

To force the older screen-buffer renderer for comparison, rebuild from clean
without the default Amiga present flags:

```sh
make clean
make amiga-examples AMIGA_PRESENT_CFLAGS=
make adfs AMIGA_PRESENT_CFLAGS=
```

## License

See [LICENSE](LICENSE).
