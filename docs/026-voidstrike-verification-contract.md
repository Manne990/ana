# VOIDSTRIKE verification contract

This contract connects the core game harness, A1200 runner, visual capture, and
release evidence. It deliberately keeps the runner independent of game-state
internals: VOIDSTRIKE executes its normal title-to-terminal flow and writes
one result file only after that flow has completed.

## Result protocol

The deterministic harness writes `DH0:ana_voidstrike_result.txt` into the
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
input_keyboard_ctrl_events input_keyboard_space_events
input_joystick_direction_events input_joystick_fire_events input_joystick_space_events
module_speed_installs module_twin_shot_installs module_wide_shot_installs
module_laser_installs module_rail_wraps module_repeat_install_events
minimum_fps_x100 average_fps_x100 minimum_five_second_fps_x100
slowest_frame_ms_x100 slow_frame_count
gameplay_window_count included_gameplay_window_count excluded_nongameplay_window_count
gameplay_window_min_fps_x100
update_stage_us draw_stage_us render_stage_us present_stage_us
visible_enemies visible_projectiles visible_effects visible_modules
result_complete pass failure_reasons
```

`source_commit` and `adf_sha256` must identify the exact source and mounted
artifact. Before launch the runner writes `DH0:ana_voidstrike_request.txt`; the
game reads and echoes its source identity, ADF hash, build ID, requested scenario,
and machine profile only after it has observed the requested path. `pass=1` requires `result_complete=1`, zero invariant
failures, and an empty `failure_reasons` value. FPS values use hundredths of an
FPS to avoid float formatting differences between host and m68k builds.

The timing fields are measured over the reported active-gameplay sample. The
window counts identify exactly which five-second windows were included or
excluded, and `gameplay_window_min_fps_x100` is the floor over the included
windows. Stage timings use microseconds; a backend without a stage measurement
must report `0`, never omit the field. Visible counts describe the densest
sampled active-gameplay frame for enemies, player projectiles, effects, and
installed/visible module presentation. A completed run must have non-zero
frames, elapsed simulated time, and at least one included gameplay window.

The supported scenario names are `victory`, `game-over`, `input-keyboard`,
`input-joystick`, `module-progression`, and `boss`. They all start on the title
screen and use real input/update/collision/render paths. The victory and boss
scenarios must finish in `terminal_state=victory`; the game-over scenario must
finish in `terminal_state=game-over`. Keyboard and joystick scenarios must
increment their respective observed-input counters. `input-keyboard` must also
observe Ctrl fire and Space install; `input-joystick` must observe joystick
direction, joystick fire, and Amiga-keyboard Space install. `module-progression`
must install Speed, Twin Shot, Wide Shot, and Laser, wrap the rail, and exercise
the defined repeat-install behavior. A normal A1200 result is
rejected below 45 average FPS or below 40 FPS for any five-second gameplay
window; debug builds report separately and have a 35 FPS guidance floor.

## Runner boundary

Each runner invocation creates a new ignored directory under
`build/emulator-results/voidstrike/<build-kind>/<machine>/<scenario>/<run-id>/` containing
the generated FS-UAE config, logs, writable drive, phase markers, final result,
ADF SHA-256, and visual artifacts. It must mount the requested ADF directly,
hash it before launch, disable Fast RAM for `a1200`, and reject stale or partial
files. It must never rely on FS-UAE Launcher state or a writable cached floppy.

The core owner owns the harness controls and the result writer in
`examples/VOIDSTRIKE`. The verification owner owns runner isolation, result
validation, visual capture, and release-facing commands. The validator is
intentionally usable before the game exists:

```sh
python3 tests/emulator_visual_test.py
python3 tools/emulator/voidstrike_result.py \
  --result build/emulator-results/voidstrike/.../ana_voidstrike_result.txt \
  --source-commit "$(git rev-parse HEAD)" --adf-sha256 "$(shasum -a 256 ... | cut -d ' ' -f 1)" \
  --scenario victory --machine-profile a1200 --build-kind normal
```

This parser is a release gate, not a substitute for FS-UAE execution or visual
inspection. Once the core harness lands, the Makefile targets must call the
runner and leave a contact sheet plus the validated result next to the ADF hash.

## Stable feedback commands

All commands are non-interactive and write ignored evidence under
`build/emulator-results/`. A command exits non-zero for a missing, partial,
stale, wrong-scenario, or rejected result; `emulator-voidstrike-visual` also
rejects missing or unusable visible captures.

```sh
make emulator-voidstrike-full-run    # stock A1200 normal victory path
make emulator-voidstrike-performance # alias for the normal full-run gate
make emulator-voidstrike-input       # keyboard and joystick scenarios
make emulator-voidstrike-visual      # isolated real FS-UAE window captures + contact sheet
make emulator-voidstrike-all         # full run, input coverage, and visual evidence
```

`emulator-voidstrike-all` is the release-feedback entry point. It intentionally
stops at the first failed required subcommand so incomplete evidence cannot be
mistaken for a complete release run.
