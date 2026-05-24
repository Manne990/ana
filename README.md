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

ANA is in 0.1 preview development. The current theme is the "Invaders Release":
a small framework release demonstrated by a playable Space Invaders-style
example.

Implemented today:

- fixed-step game loop
- PAL lores profile validation
- 16-color image loading and rendering
- bitmap font text
- semantic input directions/actions with keyboard mapping
- short SFX playback
- small helpers for rectangles, clamp, and timers
- host-side image conversion to `.anaimg`
- PNG source assets, `.anapal` palettes, and simple asset manifests
- Amiga executable and ADF builds
- `hello` and `invaders` examples

The practical showcase baseline is currently stock A1200-class hardware. A500
performance remains useful feedback, but it is not the current floor for the
complete Invaders demo.

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

ADF images are written to `build/adf/`:

- `build/adf/hello.adf`
- `build/adf/invaders.adf`

The GitHub Actions workflow uploads the same ADFs as the `ana-example-adfs`
artifact.

## Documentation

Start here:

- [Getting started](docs/getting-started.md)
- [API overview](docs/api-overview.md)
- [Asset pipeline guide](docs/asset-pipeline-guide.md)
- [Build and release package guide](docs/build-and-release.md)
- [Development routine](docs/development-routine.md)
- [Performance guide](docs/performance-guide.md)
- [Known limitations](docs/known-limitations.md)
- [ANA 0.1 release notes draft](docs/release-notes-0.1.md)

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
    ANA_Game game;

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

The current public converter supports PNG and PPM P3/P6 image input. Broader
font conversion, sound conversion, and XNA/MonoGame import experiments are
planned for later work.

## Release Package

Create a source package:

```sh
make release-package
```

The archive is written to:

```text
build/release/ana-0.1.0-dev.tar.gz
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

## License

MIT. See [LICENSE](LICENSE).
