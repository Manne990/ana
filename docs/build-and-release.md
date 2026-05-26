# Build and release package guide

This document describes the current 0.1 build outputs and the source release
package.

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

A1200 baseline builds use `-m68020`:

```sh
make amiga-a1200-examples
make amiga-invaders-a1200-debug
```

Outputs:

- `build/amiga-a1200/libana.a`
- `build/amiga-a1200/examples/hello/hello`
- `build/amiga-a1200/examples/invaders/invaders`
- `build/amiga-a1200-debug/examples/invaders-a1200-debug/invaders`

Build ADFs:

```sh
make adfs
make invaders-a1200-adf invaders-a1200-debug-adf
```

Outputs:

- `build/adf/hello.adf`
- `build/adf/invaders.adf`
- `build/adf/invaders-a1200.adf`
- `build/adf/invaders-a1200-debug.adf`

## CI build

GitHub Actions currently runs:

- strict C89 host build with GCC
- strict C89 host build with Clang
- host tests
- PNG, palette, and manifest asset conversion tests
- Amiga executable build in Docker
- portable and A1200 ADF packaging
- ADF artifact upload as `ana-example-adfs`

The Amiga Docker build uses:

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
build/release/ana-0.1.0.tar.gz
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

If a future 0.1 release includes prebuilt binaries, the release notes must state:

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

For this release, `ANA_VERSION_STRING` is `0.1.0`.
