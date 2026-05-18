# ANA

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

void game_load(void) {
    player = ana_load_image("player.anaimg");
}

void game_update(ANA_Time time) {
    if (ana_joy_down(0, ANA_JOY_RIGHT)) {
        player_x++;
    }

    if (ana_joy_down(0, ANA_JOY_LEFT)) {
        player_x--;
    }
}

void game_draw(void) {
    ana_clear(0);
    ana_draw_image(player, player_x, player_y);
}

int main(void) {
    ANA_Game game = {
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

## Current Status

ANA is currently in the planning/specification phase. The repository contains the initial 0.1 specification documents, and implementation order will be planned from those specs.

## License

See [LICENSE](LICENSE).
