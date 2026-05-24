# Known limitations for ANA 0.1

ANA is not a finished engine. These are the known limits of the current 0.1
preview work.

## Platform

- PAL lores `320x256` is the only documented screen mode.
- 16 colors / 4 bitplanes is the practical baseline.
- Stock A1200 is the current showcase baseline.
- A500/OCS performance is not yet a release target for the full Invaders demo.

## API stability

- Public names are intended to be close to 0.1 shape, but not frozen.
- No ABI stability promise.
- Existing examples are the source of truth when docs and code disagree.

## Graphics

- No general sprite manager.
- No tilemap system.
- No scrolling background support.
- No hardware sprite API yet.
- No high-level animation system beyond image frames.
- Direct-present rendering is fast but still young.

## Assets

- Public image converter supports PPM P3/P6 only.
- PNG import is not implemented.
- Font and sound conversion are not yet exposed as general user-facing tools.
- No XNA/MonoGame project import yet.

## Sound

- Short SFX playback exists.
- Music is specified separately but not implemented.
- Explicit channel reservation and mixing policy are not implemented.

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

- Docs are practical 0.1 notes, not a complete manual.
- Examples may change as the API is tightened.
