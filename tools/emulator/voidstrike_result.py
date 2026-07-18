#!/usr/bin/env python3
"""Validate the machine-readable result emitted by a VOIDSTRIKE A1200 run.

The game owns writing the result file; this module intentionally has no game
source dependency.  Keeping the contract here lets the emulator runner reject
stale, partial, or differently-built artifacts before their metrics are used.
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path


SCHEMA_VERSION = "1"
SCENARIOS = frozenset(
    (
        "victory",
        "game-over",
        "input-keyboard",
        "input-joystick",
        "module-progression",
        "boss",
    )
)
TERMINAL_STATES = frozenset(("victory", "game-over"))
REQUIRED_FIELDS = frozenset(
    (
        "schema_version",
        "source_commit",
        "build_id",
        "adf_sha256",
        "requested_scenario",
        "actual_scenario",
        "machine_profile",
        "fast_memory_kib",
        "total_frames",
        "simulated_time_ms",
        "terminal_state",
        "score",
        "remaining_lives",
        "installed_modules",
        "enemies_spawned",
        "enemies_destroyed",
        "boss_phase",
        "boss_defeated",
        "collision_invariant_failures",
        "world_bound_invariant_failures",
        "input_keyboard_events",
        "input_joystick_events",
        "input_keyboard_ctrl_events",
        "input_keyboard_space_events",
        "input_joystick_direction_events",
        "input_joystick_fire_events",
        "input_joystick_space_events",
        "module_speed_installs",
        "module_twin_shot_installs",
        "module_wide_shot_installs",
        "module_laser_installs",
        "module_rail_wraps",
        "module_repeat_install_events",
        "minimum_fps_x100",
        "average_fps_x100",
        "minimum_five_second_fps_x100",
        "slowest_frame_ms_x100",
        "slow_frame_count",
        "gameplay_window_count",
        "included_gameplay_window_count",
        "excluded_nongameplay_window_count",
        "gameplay_window_min_fps_x100",
        "update_stage_us",
        "draw_stage_us",
        "render_stage_us",
        "present_stage_us",
        "visible_enemies",
        "visible_projectiles",
        "visible_effects",
        "visible_modules",
        "result_complete",
        "pass",
        "failure_reasons",
    )
)
INTEGER_FIELDS = REQUIRED_FIELDS - frozenset(
    (
        "schema_version",
        "source_commit",
        "build_id",
        "adf_sha256",
        "requested_scenario",
        "actual_scenario",
        "machine_profile",
        "terminal_state",
        "failure_reasons",
    )
)
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
COMMIT_RE = re.compile(r"^[0-9a-f]{7,64}$")


def expected_build_id(source_commit: str, adf_sha256: str, build_kind: str) -> str:
    """Return the identity written by the runner request file."""
    return f"voidstrike-{build_kind}-{source_commit[:12]}-{adf_sha256[:12]}"


def parse_result(path: Path) -> dict[str, str]:
    """Parse a simple UTF-8 ``key=value`` result file without ambiguity."""
    values: dict[str, str] = {}
    for line_number, raw_line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        if "=" not in line:
            raise ValueError(f"line {line_number}: expected key=value")
        key, value = line.split("=", 1)
        key = key.strip()
        if not key:
            raise ValueError(f"line {line_number}: empty key")
        if key in values:
            raise ValueError(f"line {line_number}: duplicate key '{key}'")
        values[key] = value.strip()
    return values


def _integer(values: dict[str, str], key: str, failures: list[str]) -> int | None:
    try:
        return int(values[key])
    except (KeyError, ValueError):
        failures.append(f"{key}: expected an integer, got {values.get(key)!r}")
        return None


def validate_result(
    values: dict[str, str],
    *,
    source_commit: str,
    adf_sha256: str,
    scenario: str,
    machine_profile: str,
    build_kind: str,
) -> list[str]:
    """Return validation failures; an empty list means release-grade evidence."""
    failures: list[str] = []
    missing = sorted(REQUIRED_FIELDS - values.keys())
    if missing:
        failures.append("missing required fields: " + ", ".join(missing))
        return failures

    if values["schema_version"] != SCHEMA_VERSION:
        failures.append(
            f"schema_version: expected {SCHEMA_VERSION}, got {values['schema_version']}"
        )
    if not COMMIT_RE.fullmatch(values["source_commit"]):
        failures.append("source_commit: expected a lowercase Git commit id")
    if values["source_commit"] != source_commit:
        failures.append(
            f"source_commit: expected {source_commit}, got {values['source_commit']}"
        )
    if not SHA256_RE.fullmatch(values["adf_sha256"]):
        failures.append("adf_sha256: expected a lowercase SHA-256")
    if values["adf_sha256"] != adf_sha256:
        failures.append("adf_sha256: result does not name the mounted ADF")
    if scenario not in SCENARIOS:
        failures.append(f"runner requested unsupported scenario: {scenario}")
    if values["requested_scenario"] != scenario or values["actual_scenario"] != scenario:
        failures.append(
            "scenario mismatch: requested/actual result fields do not match "
            f"the runner request {scenario}"
        )
    if values["machine_profile"] != machine_profile:
        failures.append(
            f"machine_profile: expected {machine_profile}, got {values['machine_profile']}"
        )
    if not values["build_id"]:
        failures.append("build_id: must not be empty")
    elif values["build_id"] != expected_build_id(source_commit, adf_sha256, build_kind):
        failures.append("build_id: result does not match the runner request identity")
    if values["terminal_state"] not in TERMINAL_STATES:
        failures.append("terminal_state: expected victory or game-over")

    integers = {key: _integer(values, key, failures) for key in INTEGER_FIELDS}
    for key, value in integers.items():
        if value is not None and value < 0:
            failures.append(f"{key}: expected a non-negative value, got {value}")
    if integers["result_complete"] != 1:
        failures.append("result_complete: expected 1")
    if integers["pass"] != 1:
        failures.append(f"game reported failure: {values['failure_reasons'] or '<none given>'}")
    if integers["enemies_destroyed"] is not None and integers["enemies_spawned"] is not None:
        if integers["enemies_destroyed"] > integers["enemies_spawned"]:
            failures.append("enemies_destroyed: cannot exceed enemies_spawned")
    if integers["collision_invariant_failures"] != 0:
        failures.append("collision invariant failures were recorded")
    if integers["world_bound_invariant_failures"] != 0:
        failures.append("world-bound invariant failures were recorded")
    for key in ("total_frames", "simulated_time_ms", "gameplay_window_count",
                "included_gameplay_window_count"):
        if integers[key] is not None and integers[key] <= 0:
            failures.append(f"{key}: expected a non-zero completed gameplay measurement")
    if (integers["included_gameplay_window_count"] is not None
            and integers["gameplay_window_count"] is not None
            and integers["included_gameplay_window_count"] > integers["gameplay_window_count"]):
        failures.append("included_gameplay_window_count: cannot exceed gameplay_window_count")
    if scenario == "victory" and values["terminal_state"] != "victory":
        failures.append("victory scenario did not reach victory")
    if scenario == "game-over" and values["terminal_state"] != "game-over":
        failures.append("game-over scenario did not reach game-over")
    if scenario == "input-keyboard" and integers["input_keyboard_events"] == 0:
        failures.append("input-keyboard scenario observed no keyboard input")
    if scenario == "input-keyboard":
        if integers["input_keyboard_ctrl_events"] == 0:
            failures.append("input-keyboard scenario did not observe Ctrl fire")
        if integers["input_keyboard_space_events"] == 0:
            failures.append("input-keyboard scenario did not observe Space install")
    if scenario == "input-joystick":
        if integers["input_joystick_events"] == 0:
            failures.append("input-joystick scenario observed no joystick input")
        if integers["input_joystick_direction_events"] == 0:
            failures.append("input-joystick scenario did not observe joystick direction")
        if integers["input_joystick_fire_events"] == 0:
            failures.append("input-joystick scenario did not observe joystick fire")
        if integers["input_joystick_space_events"] == 0:
            failures.append("input-joystick scenario did not observe Amiga keyboard Space install")
    if scenario == "module-progression":
        for key in (
            "module_speed_installs",
            "module_twin_shot_installs",
            "module_wide_shot_installs",
            "module_laser_installs",
        ):
            if integers[key] == 0:
                failures.append(f"module-progression scenario did not install {key[7:-9]}")
        if integers["module_rail_wraps"] == 0:
            failures.append("module-progression scenario did not wrap the module rail")
        if integers["module_repeat_install_events"] == 0:
            failures.append("module-progression scenario did not exercise repeat install")
    if scenario == "boss" and integers["boss_defeated"] != 1:
        failures.append("boss scenario did not defeat the boss")
    if build_kind == "normal":
        if integers["average_fps_x100"] is not None and integers["average_fps_x100"] < 4500:
            failures.append("normal build average FPS is below 45")
        if (
            integers["minimum_five_second_fps_x100"] is not None
            and integers["minimum_five_second_fps_x100"] < 4000
        ):
            failures.append("normal build five-second FPS floor is below 40")
    elif build_kind == "debug":
        if integers["average_fps_x100"] is not None and integers["average_fps_x100"] < 3500:
            failures.append("debug build average FPS is below 35 guidance floor")
    else:
        failures.append(f"unknown build kind: {build_kind}")
    return failures


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--result", type=Path, required=True)
    parser.add_argument("--source-commit", required=True)
    parser.add_argument("--adf-sha256", required=True)
    parser.add_argument("--scenario", choices=sorted(SCENARIOS), required=True)
    parser.add_argument("--machine-profile", default="a1200")
    parser.add_argument("--build-kind", choices=("normal", "debug"), required=True)
    args = parser.parse_args()
    try:
        values = parse_result(args.result)
    except (OSError, ValueError) as error:
        print(f"Invalid VOIDSTRIKE result: {error}")
        return 2
    failures = validate_result(
        values,
        source_commit=args.source_commit,
        adf_sha256=args.adf_sha256,
        scenario=args.scenario,
        machine_profile=args.machine_profile,
        build_kind=args.build_kind,
    )
    if failures:
        print("VOIDSTRIKE result rejected:")
        for failure in failures:
            print(f"  - {failure}")
        return 1
    print("VOIDSTRIKE result accepted.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
