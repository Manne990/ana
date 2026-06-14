#!/usr/bin/env python3
"""Run Byte Brothers emulator scenarios and enforce sprite backend invariants."""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
RUNNER = ROOT / "tools" / "emulator" / "run_byte_brothers.py"

SCENARIOS = {
    "scroll": {"frames": 240, "min_fps_x100": 4200},
    "enemy-overflow": {"frames": 240, "min_fps_x100": 4500},
    "input": {"frames": 100, "min_fps_x100": 4500},
    "stomp": {"frames": 80, "min_fps_x100": 4500},
    "stomp-drop": {"frames": 80, "min_fps_x100": 4500},
    "stomp-moving": {"frames": 100, "min_fps_x100": 4500},
    "stomp-fall": {"frames": 80, "min_fps_x100": 4500},
    "stomp-edge": {"frames": 80, "min_fps_x100": 4500},
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--machine", default="a1200")
    parser.add_argument(
        "--scenario",
        action="append",
        choices=sorted(SCENARIOS),
        help="Scenario to run. Defaults to all sprite/input scenarios.",
    )
    parser.add_argument("--timeout", type=float, default=180.0)
    parser.add_argument(
        "--strict-raster",
        action="store_true",
        help="Fail when hardware sprite updates occur during visible scanout.",
    )
    return parser.parse_args()


def parse_result(output: str) -> dict[str, str]:
    result: dict[str, str] = {}
    for line in output.splitlines():
        line = line.strip()
        if "=" not in line:
            continue
        key, value = line.split("=", 1)
        result[key.strip()] = value.strip()
    return result


def as_int(result: dict[str, str], key: str) -> int:
    try:
        return int(result.get(key, "0"))
    except ValueError:
        return 0


def require(checks: list[str], condition: bool, message: str) -> None:
    if not condition:
        checks.append(message)


def validate_result(
    name: str,
    result: dict[str, str],
    strict_raster: bool,
) -> list[str]:
    failures: list[str] = []
    expected = SCENARIOS[name]
    fps = as_int(result, "average_fps_x100")
    max_visible_enemies = as_int(result, "max_visible_enemies")
    enemy_slots = as_int(result, "hw_enemy_slot_count")

    require(failures, as_int(result, "result_complete") == 1, "result incomplete")
    require(failures, as_int(result, "hw_player_ready") == 1, "player is not hardware")
    require(failures, as_int(result, "hw_player_sprite_count") == 1,
            "player should use one hardware sprite")
    require(failures, as_int(result, "player_draws") == 0,
            "player fell back to bitmap drawing")
    require(failures, as_int(result, "hw_enemy_ready") == 1, "enemies are not hardware")
    require(failures, enemy_slots >= max_visible_enemies,
            "not enough hardware enemy slots")
    require(failures, as_int(result, "bitmap_enemy_draws") == 0,
            "enemy fell back to bitmap drawing")
    require(failures, as_int(result, "hw_sprite_position_checks") > 0,
            "hardware sprite positions were not checked")
    require(failures, as_int(result, "hw_sprite_position_mismatches") == 0,
            "hardware sprite control words did not match requested positions")
    require(failures, as_int(result, "hw_sprite_zero_control_words") == 0,
            "visible hardware sprite had zeroed control words")
    if strict_raster:
        require(failures, as_int(result, "hw_sprite_raster_checks") > 0,
                "hardware sprite raster writes were not traced")
        if "hw_sprite_unsafe_span_writes" in result:
            require(failures, as_int(result, "hw_sprite_unsafe_span_writes") == 0,
                    "hardware sprites were updated while their scanlines were active")
        else:
            require(failures, as_int(result, "hw_sprite_visible_raster_writes") == 0,
                    "hardware sprites were updated during visible scanout")
    if not strict_raster:
        require(failures, fps >= expected["min_fps_x100"],
                f"fps {fps / 100:.2f} below threshold")

    if name == "input":
        require(failures, as_int(result, "input_jump_seen") == 1,
                "jump input was not seen")
        require(failures, as_int(result, "input_dash_seen") == 1,
                "dash input was not seen")
        require(failures, as_int(result, "input_move_seen") == 1,
                "movement input was not seen")
        require(failures, as_int(result, "input_quit_scheduled") == 1,
                "quit input was not scheduled")
    elif name in {
        "stomp",
        "stomp-drop",
        "stomp-moving",
        "stomp-fall",
        "stomp-edge",
    }:
        require(failures, as_int(result, "alive_enemies") == 0,
                "enemy was not defeated by stomp")
        require(failures, as_int(result, "lives") == 3,
                "player lost a life during stomp")
        require(failures, as_int(result, "score") >= 25,
                "stomp did not award score")
        require(failures, as_int(result, "stomp_count") > 0,
                "stomp event was not recorded")
        require(failures, as_int(result, "player_hit_count") == 0,
                "player damage was recorded during stomp")

    return failures


def run_scenario(
    machine: str,
    name: str,
    timeout: float,
    strict_raster: bool,
) -> dict[str, str]:
    spec = SCENARIOS[name]
    cmd = [
        sys.executable,
        str(RUNNER),
        "--machine",
        machine,
        "--scenario",
        name,
        "--frames",
        str(spec["frames"]),
        "--timeout",
        str(timeout),
    ]
    if strict_raster:
        cmd += ["--extra-cflag=-DBB_HW_RASTER_TRACE=1"]
    proc = subprocess.run(
        cmd,
        cwd=ROOT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    sys.stdout.write(proc.stdout)
    if proc.returncode != 0:
        raise RuntimeError(f"{name} runner failed with exit {proc.returncode}")
    return parse_result(proc.stdout)


def main() -> int:
    args = parse_args()
    scenarios = args.scenario or ["scroll", "enemy-overflow", "input"]
    failures: list[str] = []

    for scenario in scenarios:
        print(f"\n== Byte Brothers {scenario} ==")
        result = run_scenario(
            args.machine,
            scenario,
            args.timeout,
            args.strict_raster)
        scenario_failures = validate_result(scenario, result, args.strict_raster)
        if scenario_failures:
            for failure in scenario_failures:
                failures.append(f"{scenario}: {failure}")
        else:
            fps = as_int(result, "average_fps_x100") / 100.0
            print(
                f"{scenario}: ok, fps={fps:.2f}, "
                f"player_hw={result.get('hw_player_sprite_count')}, "
                f"enemy_slots={result.get('hw_enemy_slot_count')}, "
                f"bitmap_enemy_draws={result.get('bitmap_enemy_draws')}, "
                f"sprite_pos_checks={result.get('hw_sprite_position_checks')}, "
                "visible_raster_writes="
                f"{result.get('hw_sprite_visible_raster_writes')}, "
                "unsafe_span_writes="
                f"{result.get('hw_sprite_unsafe_span_writes', 'n/a')}"
            )

    if failures:
        print("\nSprite backend check failed:")
        for failure in failures:
            print(f"- {failure}")
        return 1

    print("\nSprite backend check passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
