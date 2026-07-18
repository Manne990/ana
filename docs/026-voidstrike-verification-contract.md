# VOIDSTRIKE verification contract

This contract connects the core game harness, A1200 runner, visual capture, and
release evidence. It deliberately keeps the runner independent of game-state
internals: VOIDSTRIKE executes its normal title-to-terminal flow and writes
one result file only after that flow has completed.

## Result protocol

The deterministic harness writes `ana_voidstrike_result.txt` into the
host-mounted writable directory. Its final write is a UTF-8 `key=value` file;
keys are unique, blank lines and `#` comments are ignored. `phase=shutdown` in
`ana_voidstrike_phase.txt` is the completion marker, but only a result accepted
by `tools/emulator/voidstrike_result.py` is passing evidence.

The required schema is version `1`. The game must report these keys:

```text
schema_version source_commit build_id adf_sha256
requested_scenario actual_scenario machine_profile fast_memory_kib
total_frames simulated_time_ms terminal_state score remaining_lives
installed_modules enemies_spawned enemies_destroyed boss_phase boss_defeated
collision_invariant_failures world_bound_invariant_failures
input_keyboard_events input_joystick_events
minimum_fps_x100 average_fps_x100 minimum_five_second_fps_x100
result_complete pass failure_reasons
```

`source_commit` and `adf_sha256` must identify the exact source and mounted
artifact. The runner supplies the requested scenario and machine profile through
the harness build/configuration; the game echoes them only after it has observed
the requested path. `pass=1` requires `result_complete=1`, zero invariant
failures, and an empty `failure_reasons` value. FPS values use hundredths of an
FPS to avoid float formatting differences between host and m68k builds.

The supported scenario names are `victory`, `game-over`, `input-keyboard`,
`input-joystick`, `module-progression`, and `boss`. They all start on the title
screen and use real input/update/collision/render paths. The victory and boss
scenarios must finish in `terminal_state=victory`; the game-over scenario must
finish in `terminal_state=game-over`. Keyboard and joystick scenarios must
increment their respective observed-input counters. A normal A1200 result is
rejected below 45 average FPS or below 40 FPS for any five-second gameplay
window; debug builds report separately and have a 35 FPS guidance floor.

## Runner boundary

Each runner invocation creates a new ignored directory under
`build/emulator-results/voidstrike/<build-kind>/<machine>/<scenario>/` containing
the generated FS-UAE config, logs, writable drive, phase markers, final result,
ADF SHA-256, and visual artifacts. It must mount the requested ADF directly,
hash it before launch, disable Fast RAM for `a1200`, and reject stale or partial
files. It must never rely on FS-UAE Launcher state or a writable cached floppy.

The core owner owns the harness controls and the result writer in
`examples/VOIDSTRIKE`. The verification owner owns runner isolation, result
validation, visual capture, and release-facing commands. The validator is
intentionally usable before the game exists:

```sh
python3 tests/voidstrike_result_test.py
python3 tools/emulator/voidstrike_result.py \
  --result build/emulator-results/voidstrike/.../ana_voidstrike_result.txt \
  --source-commit "$(git rev-parse HEAD)" --adf-sha256 "$(shasum -a 256 ... | cut -d ' ' -f 1)" \
  --scenario victory --machine-profile a1200 --build-kind normal
```

This parser is a release gate, not a substitute for FS-UAE execution or visual
inspection. Once the core harness lands, the Makefile targets must call the
runner and leave a contact sheet plus the validated result next to the ADF hash.
