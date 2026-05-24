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
- small helpers for rectangles, clamp, and timers
- host-side `ana-convert` image converter
- Amiga example ADF build
- ANA Invaders showcase

## Included examples

- `examples/hello`
- `examples/invaders`

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
