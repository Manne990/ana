# ANA 0.1 release notes draft

Status: draft, not yet released.

## Theme

ANA 0.1 is the Invaders preview release. Its purpose is to prove that a small C
framework can make Amiga game development feel approachable while keeping the
runtime model explicit and performant.

## Included systems

- fixed-step game runtime
- PAL lores profile validation
- 16-color image loading and drawing
- image frame drawing
- bitmap font text
- semantic input directions and actions
- keyboard mapping
- short SFX playback
- explicit music/SFX channel policy
- MOD music asset loading and Amiga playback through `ptplayer`
- small helpers for rectangles, clamp, and timers
- retained rendering helpers for BOB state, dirty labels, and small draw layers
- host-side `ana-convert` image, font, and SFX converter
- PNG image input
- text palette files (`.anapal`)
- simple asset manifests
- Amiga example ADF build
- ANA Invaders showcase

## Included examples

- `examples/hello`
- `examples/invaders`

`examples/invaders/main.c` is kept as a compact ANA entrypoint. The game rules
and the optimized Amiga renderer are split into separate modules so the example
can act both as a playable showcase and as readable sample code. Invaders also
packages a small MOD and uses it on title, clear, and game-over screens while
keeping active gameplay focused on SFX and frame rate.

## Included tools

- `tools/ana-convert`

## Release package

The source package is built with:

```sh
make release-package
```

The current draft package name is:

```text
build/release/ana-0.1.0-dev.tar.gz
```

## Target baseline

The current practical showcase target is stock A1200-class hardware. A500/OCS
compatibility and performance remain useful constraints, but the complete
Invaders showcase is not currently tuned for stock A500 as the floor.

## Known limitations

See `docs/known-limitations.md`.

## Before final 0.1

- decide final API names for the current public surface
- verify release package on a clean machine
- verify ADF artifact download from GitHub Actions
- run Invaders on the intended emulator/hardware baseline
- update `ANA_VERSION_STRING`
- replace this draft with final release notes
