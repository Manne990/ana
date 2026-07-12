# Known limitations for ANA 0.2

ANA is not a finished engine. These are the known limits of the current 0.2
preview work.

## Platform

- PAL lores `320x256` is the only documented screen mode.
- 16 colors / 4 bitplanes is the practical baseline.
- Stock A1200 without Fast RAM is the current showcase baseline.
- Byte Brothers normally sustains at least 45 fps in the deterministic stock
  A1200 scroll harness; 50 fps is still the target rather than a universal
  guarantee for every scene.
- A500/OCS performance is not yet a release target for the full Invaders demo.

## API stability

- Public names are intended to be close to the 0.2 shape, but not frozen.
- No ABI stability promise.
- Existing examples are the source of truth when docs and code disagree.

## Graphics

- Retained BOB and label helpers exist, but there is no scene graph.
- A camera/world conversion helper and low-level rectangle scroll primitive
  exist, and games can declare specific scroll contracts such as
  `ANA_RENDER_SIDE_SCROLL`, `ANA_RENDER_VERTICAL_SCROLL`, and
  `ANA_RENDER_TILE_4WAY`. `ANA_TileLayer` also exists and Byte Brothers uses it
  for framework-owned playfield strip redraw. An opt-in synced visible-scroll
  bridge exists for A1200 direct-present builds, but it is disabled by default
  because it can produce stale-pixel artifacts. There is no stable
  high-performance native hardware-scroll backend yet. Spec 017 defines that
  planned direction.
- `ANA_AmigaSpriteBatch` manages allocation, image conversion, channel
  ownership, movement, hiding, and raster-safe updates for advanced Amiga
  renderers. It is deliberately Amiga-specific: sprites are 16 hardware pixels
  wide, use three visible colors plus transparency, and remain constrained by
  the eight chipset channels. Games still need a bitmap fallback when demand
  exceeds available channels.
- No high-level animation system beyond image frames.
- Direct-present rendering is fast but still young.

## Assets

- Public image converter supports PNG and PPM P3/P6.
- Public font converter supports fixed-width PNG and PPM P3/P6 font sheets.
- Public sound converter supports simple generated `.anasfx` SFX recipes and
  PCM WAV import.
- Palette files and manifests exist, but are intentionally simple. Manifest
  names are restricted to safe filename characters and duplicate outputs are
  rejected.
- Manifest builds can copy validated four-channel `.mod` music assets.
- IFF sound import is not implemented yet.
- No XNA/MonoGame project import yet.

## Sound

- Short SFX playback exists.
- Explicit music/SFX channel policy exists for the four Paula channels.
- MOD assets can be loaded and controlled through the music API.
- Audible MOD replay exists on Amiga through the vendored `ptplayer` backend.
- Host builds validate and track music state, but do not preview MOD audio.
- Large MOD files can cause slow floppy startup, high Chip RAM pressure, and
  lower frame rate. Current examples should use small MOD assets.
- Continuous MOD playback during busy gameplay can still cost too much CPU.
  Invaders currently keeps music to title, clear, and game-over screens for the
  normal performance profile.

## Input

- Keyboard mapping exists.
- Joystick-style directions/actions exist.
- Gamepad-specific mapping is not implemented.

## Runtime

- Fixed-step loop only.
- No scene manager.
- No entity/component system.
- No persistence or save data helpers.

## Documentation

- Docs are practical 0.2 notes, not a complete manual.
- Examples may change as the API is tightened.
