# Development routine

Use this checklist after every code, build, asset, example, or workflow change.

## Documentation check

Review whether the change affects:

- `README.md`
- getting started instructions
- API overview
- asset pipeline guide
- build and release instructions
- performance guide
- known limitations
- relevant spec status
- example code or comments used as learning material

Update docs in the same change when behavior, commands, supported targets,
example flow, public API, generated artifacts, or known limitations change.

If no docs update is needed, be explicit about why in the final summary.

## Verification check

Pick the smallest useful verification for the change:

- docs-only: `git diff --check`
- host code: `make clean test`
- host visual changes: `make emulator-byte-brothers-host-visual`
- Amiga build: `make clean amiga-examples`
- ADF output: `make adfs`
- Byte Brothers behavior: `make emulator-byte-brothers-all`
- Byte Brothers sprites/rendering: `make emulator-byte-brothers-visual`
- release packaging: `make release-package`

For CI-related changes, reproduce the failing CI command locally when possible.

The visual target combines two independent oracles. A deterministic host run
dumps ANA's indexed frontbuffer and checks consecutive frames for missing,
partial, or duplicated actor components. The FS-UAE run checks the Amiga-only
path: channel allocation, control words, actor positions, collision invariants,
raster spans, and minimum FPS. Current macOS versions mask FS-UAE's chipset
surface from screen-recording APIs, so a black macOS capture is reported but is
not treated as visual evidence; the frontbuffer oracle supplies those pixels.

## Release discipline

Before a public release:

- verify docs against a clean checkout
- verify ADF artifacts from GitHub Actions
- confirm `ANA_VERSION_STRING`
- update release notes
- state the intended hardware baseline
