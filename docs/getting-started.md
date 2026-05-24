# Getting started with ANA 0.1

This guide shows the current 0.1 workflow: build the library, run the host
examples, build Amiga executables, and package ADF images.

ANA is still pre-release software. The API is small on purpose, but it is not
yet frozen.

## Prerequisites

For host builds and tests:

- a C compiler (`cc`, `gcc`, or `clang`)
- `make`
- `python3` for PNG test fixture generation
- standard POSIX shell tools

For Amiga executable builds:

- `m68k-amigaos-gcc`
- `m68k-amigaos-ar`

For ADF images:

- `gadf`

The GitHub Actions workflow uses the Docker image
`amigadev/crosstools:m68k-amigaos-gcc10_amd64` for the Amiga compiler and
`gadf` for disk images.

## Build the host version

```sh
make clean
make all
make test
```

This builds:

- `build/libana.a`
- `build/tools/ana-convert/ana-convert`
- generated example assets under `build/assets/`
- host smoke-test versions of `hello` and `invaders`
- the test binaries

## Build Amiga examples

With `m68k-amigaos-gcc` on your path:

```sh
make clean
make amiga-examples
```

The Amiga executables are written to:

- `build/amiga/examples/hello/hello`
- `build/amiga/examples/invaders/invaders`

To reproduce the CI Amiga build with Docker:

```sh
docker run --rm \
  --user "$(id -u):$(id -g)" \
  -v "$PWD:/work" \
  -w /work \
  amigadev/crosstools:m68k-amigaos-gcc10_amd64 \
  make clean amiga-examples
```

## Build ADF images

After `make amiga-examples`:

```sh
make adfs
```

If `gadf` is not on your path:

```sh
make adfs ADFTOOL="$HOME/go/bin/gadf"
```

ADF images are written to `build/adf/`:

- `build/adf/hello.adf`
- `build/adf/invaders.adf`

The CI artifact is called `ana-example-adfs`.

## Your first ANA game

ANA games are normal C programs that fill an `ANA_Game` struct and call
`ana_run`.

```c
#include <ana.h>

static int player_x = 100;

static void game_init(void)
{
    ana_input_clear_key_map();
    ana_input_map_default_keys(ANA_INPUT_DEVICE_0);
}

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

    game.init = game_init;
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

Important runtime rules:

- `ANA_Time.tick` is the fixed game-loop tick counter.
- At 50 fps, 50 ticks is about one second.
- Keep allocation and file loading out of `update` and `draw`.
- Use preconverted assets for images, fonts, and sounds.

## Debug ADFs

The normal Invaders ADF is optimized for the current direct-present renderer.
For performance counters:

```sh
make amiga-invaders-debug
make invaders-debug-adf
```

This writes `build/adf/invaders-debug.adf`.

For the direct-present path with synchronization:

```sh
make amiga-invaders-sync
make invaders-sync-adf
```

This writes `build/adf/invaders-sync.adf`.
