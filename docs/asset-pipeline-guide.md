# Asset pipeline guide

ANA expects game assets to be converted before runtime. This keeps Amiga-side
loading simple and avoids expensive decoding during gameplay.

## Current asset formats

Implemented for 0.1:

- `.anaimg` for indexed images and animation frames
- `.anafnt` for bitmap fonts used by the Invaders example
- `.anasnd` for short sound effects used by the Invaders example

The public converter currently supports image conversion. The Invaders example
also has a small build-time asset generator for its bundled showcase assets.

## Image conversion

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

Convert a spritesheet:

```sh
build/tools/ana-convert/ana-convert image explosion_sheet.ppm \
  --out explosion.anaimg \
  --colors 16 \
  --frame-width 16 \
  --frame-height 16 \
  --transparent #ff00ff
```

Supported input today:

- PPM P3
- PPM P6

Planned but not yet implemented:

- PNG import
- richer palette workflows
- external font conversion
- external sound conversion
- XNA/MonoGame content import experiments

## Loading assets at runtime

Load assets from disk:

```c
ANA_Image player;

player = ana_load_image("assets/player.anaimg");
ana_draw_image(player, x, y);
```

Load embedded asset bytes:

```c
ANA_Image player;

player = ana_load_image_data(player_anaimg, player_anaimg_size);
```

The Invaders example uses embedded C arrays for predictable showcase builds and
also packages generated asset files onto the ADF. Normal games should be able to
choose either model:

- files on disk when iteration and external assets matter
- embedded bytes when startup simplicity and single-binary behavior matter

## Palette rules

For the 0.1 baseline:

- target 16 colors
- use one shared game palette when possible
- reserve a transparent color for masked sprites
- avoid runtime color conversion

If an image needs more than 16 colors, simplify the source art before
conversion. The current converter does not perform advanced quantization.

## Amiga packaging

The Invaders ADF includes an `assets/` directory:

```text
assets/
  bullet.anaimg
  explosion.anaimg
  explosion.anasnd
  fire.anasnd
  gameover.anasnd
  hud.anafnt
  invader.anaimg
  player.anaimg
  step.anasnd
```

Build it with:

```sh
make amiga-examples
make adfs
```

The generated ADF is `build/adf/invaders.adf`.
