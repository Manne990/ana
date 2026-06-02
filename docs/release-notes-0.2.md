# ANA 0.2 release notes

Status: prepared as `0.2.0`.

## Theme

ANA 0.2 expands the framework from the Invaders baseline into asset-driven
sample games with music, sound effects, scrolling, and clearer game structure.

## Included systems

- PNG and WAV asset conversion through manifests
- palette assets (`.anapal`)
- MOD music asset copying and playback
- retained rendering helpers for sprites, labels, and dirty repair
- camera and rectangle helpers
- tile-layer oriented scrolling support
- render mode/backend declarations for scrolling games
- source release packaging through `make release-package`

## Included examples

- `examples/hello`
- `examples/invaders`
- `examples/amaze`
- `examples/byte_brothers`

`examples/amaze` demonstrates converted PNG sprites, WAV-derived SFX, a small
looped MOD, and compact pathfinding.

`examples/byte_brothers` demonstrates a side-scrolling platform sample with
PNG sprites, WAV-derived SFX, looped MOD music, a tile-layer playfield, and a
split between game rules, rendering, levels, and assets.

## Release package

The source package is built with:

```sh
make release-package
```

The source package name is:

```text
build/release/ana-0.2.0.tar.gz
```

## Target baseline

The current practical showcase target remains stock A1200-class hardware.
A500/OCS compatibility and performance remain useful constraints, but the
scrolling and MOD-backed examples are currently tuned around the A1200 target.

## Known limitations

See `docs/known-limitations.md`.
