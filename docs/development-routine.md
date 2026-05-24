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
- Amiga build: `make clean amiga-examples`
- ADF output: `make adfs`
- release packaging: `make release-package`

For CI-related changes, reproduce the failing CI command locally when possible.

## Release discipline

Before a public release:

- verify docs against a clean checkout
- verify ADF artifacts from GitHub Actions
- confirm `ANA_VERSION_STRING`
- update release notes
- state the intended hardware baseline
