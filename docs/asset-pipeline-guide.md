# Asset pipeline guide

ANA expects game assets to be converted before runtime. This keeps Amiga-side
loading simple and avoids expensive decoding during gameplay.

## Current asset formats

Implemented for 0.1:

- `.anaimg` for indexed images and animation frames
- `.anafnt` for bitmap fonts used by the Invaders example
- `.anasnd` for short sound effects used by the Invaders example
- `.mod` for ProTracker music assets copied by manifests
- `.anapal` for text palette files used by the host converter

The public converter supports PNG and PPM image conversion for images,
fixed-width bitmap fonts, and simple text recipes or PCM WAV files for bundled
sound effects.
The Invaders example is built from source assets only; it no longer needs a
custom host-side asset generator.

## Image conversion

Build the converter:

```sh
make tools
```

Create a palette from a PNG:

```sh
build/tools/ana-convert/ana-convert palette palette.png \
  --out game.anapal \
  --colors 16
```

Convert a PNG image:

```sh
build/tools/ana-convert/ana-convert image player.png \
  --out player.anaimg \
  --palette game.anapal \
  --transparent "#ff00ff"
```

Convert a spritesheet:

```sh
build/tools/ana-convert/ana-convert image explosion_sheet.png \
  --out explosion.anaimg \
  --palette game.anapal \
  --frame-width 16 \
  --frame-height 16 \
  --transparent "#ff00ff"
```

Convert a fixed-width bitmap font:

```sh
build/tools/ana-convert/ana-convert font hud_font.png \
  --out hud.anafnt \
  --colors 2 \
  --char-width 6 \
  --char-height 7 \
  --first-char 48 \
  --chars 43 \
  --transparent "#ff00ff"
```

Convert a short SFX recipe:

```sh
build/tools/ana-convert/ana-convert sound fire.anasfx \
  --out fire.anasnd
```

Convert a PCM WAV file:

```sh
build/tools/ana-convert/ana-convert sound coin.wav \
  --out coin.anasnd \
  --rate 8000 \
  --volume 48 \
  --priority 2
```

Minimal `.anasfx` recipe:

```text
ANA_SOUND 1
wave square
rate 8000
samples 160
volume 36
priority 2
attack 10
release 34
amplitude 46 5
period 6 2
```

Supported source image formats today:

- PNG
- PPM P3
- PPM P6

Build a manifest:

```sh
build/tools/ana-convert/ana-convert build assets.ana --out build/assets/game
```

Minimal manifest:

```text
ANA_ASSETS 1

palette game palette.png --colors 16
image player player.png --palette game --transparent #ff00ff
image explosion explosion.png --palette game --frame-width 16 --frame-height 16 --transparent #ff00ff
font hud hud_font.png --colors 2 --char-width 6 --char-height 7 --first-char 48 --chars 43 --transparent #ff00ff
sound fire fire.anasfx
sound coin coin.wav --rate 8000 --volume 48 --priority 2
music theme theme.mod
```

Planned but not yet implemented:

- richer palette workflows
- IFF sound import
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

The Invaders example loads asset files from `assets/` on the Amiga ADF and from
`build/assets/invaders/assets/` in host builds. Its image assets come from PNG
source files in `examples/invaders/assets/`, and its HUD font is converted from
a source bitmap font sheet. SFX files are converted from small `.anasfx` recipe
files in the same asset directory. AMAze demonstrates PNG source sprites, PCM
WAV source SFX that are downsampled to `.anasnd` by the same manifest flow, plus
a copied ProTracker MOD music asset.

Normal games should be able to choose either model:

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
  game.anapal
  hud.anafnt
  invader.anaimg
  player.anaimg
  step.anasnd
  theme.mod
```

When a manifest contains `music theme theme.mod`, the converter copies the
source MOD to `theme.mod` in the output asset directory. Games load it with
`ana_load_music("assets/theme.mod")` and start it with `ana_play_music`.

Build it with:

```sh
make amiga-examples
make adfs
```

The generated ADF is `build/adf/invaders.adf`.
