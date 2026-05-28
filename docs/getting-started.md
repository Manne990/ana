# Getting started with ANA

This guide shows the current workflow: build the library, run the host examples,
build Amiga executables, and package ADF images.

ANA is intentionally small. Later releases may still revise names and add
higher-level helpers as the framework matures.

## Prerequisites

For host builds and tests:

- a C compiler (`cc`, `gcc`, or `clang`)
- `make`
- `python3` for PNG test fixture generation
- standard POSIX shell tools

For Amiga executable builds:

- `m68k-amigaos-gcc` and `m68k-amigaos-ar`, or Docker for the automatic
  `amigadev/crosstools:m68k-amigaos-gcc10_amd64` fallback

For ADF images:

- `gadf`

The Makefile also looks for `gadf` in `$HOME/go/bin/gadf` if it is not already
on `PATH`. GitHub Actions uses the same Docker image for the Amiga compiler and
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
- host smoke-test versions of `hello`, `invaders`, `amaze`, and
  `byte_brothers`
- the test binaries

## Build Amiga examples

With `m68k-amigaos-gcc` on your path, or Docker available for the automatic
fallback:

```sh
make clean
make amiga-examples
```

The Amiga executables are written to:

- `build/amiga/examples/hello/hello`
- `build/amiga/examples/invaders/invaders`
- `build/amiga/examples/amaze/amaze`
- `build/amiga/examples/byte_brothers/byte_brothers`

To force the CI Amiga build image explicitly:

```sh
docker run --rm \
  --user "$(id -u):$(id -g)" \
  -v "$PWD:/work" \
  -w /work \
  amigadev/crosstools:m68k-amigaos-gcc10_amd64 \
  make clean amiga-examples
```

For the stock-A1200 baseline build:

```sh
docker run --rm \
  --user "$(id -u):$(id -g)" \
  -v "$PWD:/work" \
  -w /work \
  amigadev/crosstools:m68k-amigaos-gcc10_amd64 \
  make clean amiga-a1200-examples amiga-invaders-a1200-debug amiga-amaze-a1200-debug amiga-byte-brothers-a1200-debug
```

The A1200 targets compile the C code with `-m68020`. They keep the same PAL
lores 320x256, 16-color display profile as the portable Amiga targets.

## Build ADF images

The ADF targets build the matching Amiga executables first when needed:

```sh
make adfs
```

For A1200-specific disk images:

```sh
make invaders-a1200-adf
make amaze-a1200-adf
make amaze-a1200-debug-adf
make byte-brothers-a1200-adf
make byte-brothers-a1200-debug-adf
```

If `gadf` is installed somewhere else:

```sh
make adfs ADFTOOL="$HOME/go/bin/gadf"
make invaders-a1200-adf amaze-a1200-adf amaze-a1200-debug-adf byte-brothers-a1200-adf byte-brothers-a1200-debug-adf ADFTOOL="$HOME/go/bin/gadf"
```

ADF images are written to `build/adf/`:

- `build/adf/hello.adf`
- `build/adf/invaders.adf`
- `build/adf/amaze.adf`
- `build/adf/byte-brothers.adf`
- `build/adf/invaders-a1200.adf`
- `build/adf/amaze-a1200.adf`
- `build/adf/amaze-a1200-debug.adf`
- `build/adf/byte-brothers-a1200.adf`
- `build/adf/byte-brothers-a1200-debug.adf`

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
    ANA_Game game = {0};

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
    game.render_mode = ANA_RENDER_DIRTY;
    game.debug_stats = 0;

    return ana_run(&game);
}
```

Important runtime rules:

- `ANA_Time.tick` is the fixed game-loop tick counter.
- At 50 fps, 50 ticks is about one second.
- Keep allocation and file loading out of `update` and `draw`.
- Use preconverted assets for images, fonts, and sounds.
- Use `ANA_RENDER_DIRTY` for static-view games with moving objects. Use
  `ANA_RENDER_TILE_SCROLL` for scrolling games so ANA can choose the right
  backend as that support matures.

## Debug ADFs

The normal Invaders ADF is optimized for the current direct-present renderer.
For performance counters:

```sh
make amiga-invaders-debug
make invaders-debug-adf
```

This writes `build/adf/invaders-debug.adf`.

For stock-A1200 baseline measurements:

```sh
make amiga-invaders-a1200-debug
make invaders-a1200-debug-adf
```

This writes `build/adf/invaders-a1200-debug.adf`.

For AMAze stock-A1200 measurements:

```sh
make amiga-amaze-a1200-debug
make amaze-a1200-debug-adf
```

This writes `build/adf/amaze-a1200-debug.adf`.

For Byte Brothers stock-A1200 measurements:

```sh
make amiga-byte-brothers-a1200-debug
make byte-brothers-a1200-debug-adf
```

This writes `build/adf/byte-brothers-a1200-debug.adf`.

For the direct-present path with synchronization:

```sh
make amiga-invaders-sync
make invaders-sync-adf
```

This writes `build/adf/invaders-sync.adf`.

For comparison with the non-direct screen-buffer path:

```sh
make amiga-invaders-buffered-debug
make invaders-buffered-debug-adf
```

This writes `build/adf/invaders-buffered-debug.adf`.
