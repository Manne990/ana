# Build and release package guide

This document describes the current `main` build outputs and the source package.

## Host build outputs

```sh
make clean
make all
```

Outputs:

- `build/libana.a`
- `build/tools/ana-convert/ana-convert`
- `build/examples/hello/hello`
- `build/examples/invaders/invaders`
- `build/examples/amaze/amaze`
- generated example assets under `build/assets/`

Run tests:

```sh
make test
```

## Amiga build outputs

```sh
make clean
make amiga-examples
```

Outputs:

- `build/amiga/libana.a`
- `build/amiga/examples/hello/hello`
- `build/amiga/examples/invaders/invaders`
- `build/amiga/examples/amaze/amaze`

A1200 baseline builds use `-m68020`:

```sh
make amiga-a1200-examples
make amiga-invaders-a1200-debug
make amiga-amaze-a1200-debug
```

Outputs:

- `build/amiga-a1200/libana.a`
- `build/amiga-a1200/examples/hello/hello`
- `build/amiga-a1200/examples/invaders/invaders`
- `build/amiga-a1200/examples/amaze/amaze`
- `build/amiga-a1200-debug/examples/invaders-a1200-debug/invaders`
- `build/amiga-a1200-debug/examples/amaze-a1200-debug/amaze`

Build ADFs:

```sh
make adfs
make invaders-a1200-adf amaze-a1200-adf invaders-a1200-debug-adf amaze-a1200-debug-adf
```

Outputs:

- `build/adf/hello.adf`
- `build/adf/invaders.adf`
- `build/adf/amaze.adf`
- `build/adf/invaders-a1200.adf`
- `build/adf/amaze-a1200.adf`
- `build/adf/invaders-a1200-debug.adf`
- `build/adf/amaze-a1200-debug.adf`

## CI build

GitHub Actions currently runs:

- strict C89 host build with GCC
- strict C89 host build with Clang
- host tests
- PNG, palette, and manifest asset conversion tests
- Amiga executable build in Docker
- portable and A1200 ADF packaging
- ADF artifact upload as `ana-example-adfs`

When the local `m68k-amigaos-*` tools are not on `PATH`, the Makefile runs the
Amiga compiler, assembler, and archiver through Docker using:

```text
amigadev/crosstools:m68k-amigaos-gcc10_amd64
```

The Amiga music backend also requires `vasmm68k_mot` to assemble the vendored
`ptplayer` source. The Docker image above provides it under
`/opt/m68k-amigaos/bin/`.

## Source release package

Create a source package:

```sh
make release-package
```

Output:

```text
build/release/ana-0.2.0-dev.tar.gz
```

The package contains:

- `README.md`
- `LICENSE`
- `Makefile`
- `include/`
- `src/`
- `tools/`
- `examples/`
- `examples/invaders/assets/` PNG source assets and manifest
- `examples/amaze/assets/` SFX source assets and manifest
- `tests/`
- `docs/`
- `.github/`

It intentionally does not include generated `build/` outputs.

## Binary artifacts

For now, binary artifacts are produced by CI rather than checked into the
source package.

Current CI binary artifacts:

- `hello.adf`
- `invaders.adf`
- `invaders-a1200.adf`
- `invaders-a1200-debug.adf`
- `amaze.adf`
- `amaze-a1200.adf`
- `amaze-a1200-debug.adf`

If a future release includes prebuilt binaries, the release notes must state:

- compiler and version
- target CPU assumptions
- AmigaOS / Kickstart assumptions
- whether the direct-present renderer is enabled
- whether `ANA_DEBUG_STATS` is enabled

## Versioning

The current version string lives in:

```text
include/ana/ana_version.h
```

Current `main` uses `ANA_VERSION_STRING` `0.2.0-dev`. The frozen 0.1 release is
tagged as `v0.1.0`.
