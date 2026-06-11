#!/usr/bin/env python3
"""Run Byte Brothers in FS-UAE and read its host-mounted harness result."""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
FS_UAE = Path("/Applications/FS-UAE.app/Contents/MacOS/fs-uae")
RESULT_NAME = "ana_byte_brothers_result.txt"
PHASE_NAME = "ana_byte_brothers_phase.txt"
GFX_PHASE_NAME = "ana_gfx_phase.txt"
RESULT_ROOT = ROOT / "build" / "emulator-results" / "byte-brothers"
BB_BIN_PATH = ROOT / "build" / "amiga-harness" / "examples" / "byte_brothers" / "byte_brothers"
ASSET_DIR = ROOT / "build" / "assets" / "byte_brothers" / "assets"
LOG_PATH = Path.home() / "Documents" / "FS-UAE" / "Cache" / "Logs" / "fs-uae.log.txt"

SCENARIOS = {
    "static": {"id": 0, "frames": 120},
    "scroll": {"id": 1, "frames": 360},
    "input": {"id": 2, "frames": 80},
    "enemy-overflow": {"id": 3, "frames": 360},
}

MACHINE_CONFIGS = {
    "a1200": Path.home() / "Documents" / "FS-UAE" / "Configurations" / "A1200.fs-uae",
    "a1200-fast": Path.home()
    / "Documents"
    / "FS-UAE"
    / "Configurations"
    / "A1200 - Fast.fs-uae",
}

OVERRIDE_KEYS = {
    "automatic_input_grab",
    "base_dir",
    "chip_memory",
    "fast_memory",
    "floppy_drive_0",
    "floppy_drive_1",
    "floppy_drive_2",
    "floppy_drive_3",
    "floppy_drive_count",
    "full_keyboard",
    "hard_drive_0",
    "hard_drive_0_priority",
    "initial_input_grab",
    "joystick_port_0",
    "joystick_port_0_mode",
    "joystick_port_1",
    "joystick_port_1_mode",
    "keyboard_input_grab",
    "keyboard_key_a",
    "keyboard_key_c",
    "keyboard_key_ctrl",
    "keyboard_key_d",
    "keyboard_key_down",
    "keyboard_key_esc",
    "keyboard_key_escape",
    "keyboard_key_left",
    "keyboard_key_q",
    "keyboard_key_return",
    "keyboard_key_right",
    "keyboard_key_s",
    "keyboard_key_space",
    "keyboard_key_up",
    "keyboard_key_w",
    "keyboard_key_x",
    "keyboard_key_z",
    "keyboard_key_v",
    "logs_dir",
    "save_states_dir",
    "zorro_iii_memory",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--machine",
        choices=sorted(MACHINE_CONFIGS),
        default="a1200",
        help="FS-UAE machine profile to derive Kickstart and base settings from.",
    )
    parser.add_argument(
        "--scenario",
        choices=sorted(SCENARIOS),
        default="static",
        help="Deterministic Byte Brothers harness scenario to run.",
    )
    parser.add_argument(
        "--frames",
        type=int,
        default=None,
        help="Override the scenario's harness frame count.",
    )
    parser.add_argument("--timeout", type=float, default=120.0)
    parser.add_argument("--no-build", action="store_true")
    parser.add_argument("--show-config", action="store_true")
    return parser.parse_args()


def config_key(line: str) -> str | None:
    stripped = line.strip()
    if not stripped or stripped.startswith("#") or "=" not in stripped:
        return None
    return stripped.split("=", 1)[0].strip().lower()


def read_base_config(machine: str) -> list[str]:
    path = MACHINE_CONFIGS[machine]
    if path.exists():
        return path.read_text(encoding="utf-8").splitlines(keepends=True)
    return ["[fs-uae]\n", "amiga_model = A1200\n"]


def base_options(machine: str) -> dict[str, str]:
    options: dict[str, str] = {}
    for line in read_base_config(machine):
        key = config_key(line)
        if key is None or key in OVERRIDE_KEYS:
            continue
        options[key] = line.split("=", 1)[1].strip()
    return options


def harness_options(machine: str, result_dir: Path) -> dict[str, str]:
    options = base_options(machine)
    base_dir = result_dir / "fs-uae-base"
    overrides = {
        "automatic_input_grab": "0",
        "base_dir": str(base_dir),
        "chip_memory": "2048",
        "floppy_drive_count": "0",
        "full_keyboard": "1",
        "hard_drive_0": str(result_dir),
        "hard_drive_0_priority": "5",
        "initial_input_grab": "0",
        "joystick_port_0": "Mouse",
        "joystick_port_1": "Keyboard",
        "joystick_port_1_mode": "joystick",
        "keyboard_input_grab": "0",
        "keyboard_key_a": "action_key_a",
        "keyboard_key_c": "action_key_c",
        "keyboard_key_ctrl": "action_key_ctrl",
        "keyboard_key_d": "action_key_d",
        "keyboard_key_down": "action_key_s",
        "keyboard_key_esc": "action_key_esc",
        "keyboard_key_escape": "action_key_esc",
        "keyboard_key_left": "action_key_a",
        "keyboard_key_q": "action_key_q",
        "keyboard_key_return": "action_key_return",
        "keyboard_key_right": "action_key_d",
        "keyboard_key_s": "action_key_s",
        "keyboard_key_space": "action_key_space",
        "keyboard_key_up": "action_key_w",
        "keyboard_key_w": "action_key_w",
        "keyboard_key_x": "action_key_x",
        "keyboard_key_z": "action_key_z",
        "keyboard_key_v": "action_key_v",
        "logs_dir": str(result_dir / "logs"),
        "save_states_dir": str(result_dir / "save-states"),
        "zorro_iii_memory": "0",
    }

    if machine == "a1200-fast":
        overrides["fast_memory"] = "8192"
    else:
        overrides["fast_memory"] = "0"

    options.update(overrides)
    return options


def write_config(machine: str, result_dir: Path, options: dict[str, str]) -> Path:
    path = result_dir / "byte-brothers.fs-uae"
    text = "[fs-uae]\n\n# ANA emulator harness options\n"
    for key in sorted(options):
        text += f"{key} = {options[key]}\n"
    path.write_text(text, encoding="utf-8")
    return path


def build_byte_brothers(scenario_id: int, frames: int) -> None:
    subprocess.run(
        [
            "make",
            "-B",
            "amiga-byte-brothers-harness",
            f"BB_HARNESS_SCENARIO_ID={scenario_id}",
            f"BB_HARNESS_FRAMES={frames}",
        ],
        cwd=ROOT,
        check=True,
    )


def setup_boot_drive(result_dir: Path) -> None:
    s_dir = result_dir / "S"
    assets_out = result_dir / "assets"

    s_dir.mkdir(parents=True, exist_ok=True)
    if assets_out.exists():
        shutil.rmtree(assets_out)
    shutil.copytree(ASSET_DIR, assets_out)
    shutil.copy2(BB_BIN_PATH, result_dir / "byte_brothers")
    (s_dir / "startup-sequence").write_text(
        "SYS:byte_brothers\n"
        "endcli\n",
        encoding="ascii",
    )


def clean_result(result_dir: Path) -> None:
    for path in result_dir.glob("*byte_brothers_result*"):
        path.unlink()
    result = result_dir / RESULT_NAME
    if result.exists():
        result.unlink()
    phase = result_dir / PHASE_NAME
    if phase.exists():
        phase.unlink()
    gfx_phase = result_dir / GFX_PHASE_NAME
    if gfx_phase.exists():
        gfx_phase.unlink()


def result_path(result_dir: Path) -> Path | None:
    direct = result_dir / RESULT_NAME
    if direct.exists():
        return direct

    for path in result_dir.iterdir():
        if path.name.lower() == RESULT_NAME:
            return path
    return None


def phase_path(result_dir: Path) -> Path | None:
    direct = result_dir / PHASE_NAME
    if direct.exists():
        return direct

    for path in result_dir.iterdir():
        if path.name.lower() == PHASE_NAME:
            return path
    return None


def named_result_path(result_dir: Path, name: str) -> Path | None:
    direct = result_dir / name
    if direct.exists():
        return direct

    for path in result_dir.iterdir():
        if path.name.lower() == name:
            return path
    return None


def result_value(path: Path, key: str) -> str | None:
    prefix = f"{key}="
    try:
        for line in path.read_text(errors="replace").splitlines():
            if line.startswith(prefix):
                return line.split("=", 1)[1].strip()
    except OSError:
        return None
    return None


def result_int(path: Path, key: str) -> int | None:
    value = result_value(path, key)
    if value is None:
        return None
    try:
        return int(value)
    except ValueError:
        return None


def is_final_result(path: Path) -> bool:
    return result_value(path, "phase") == "shutdown" and (
        result_value(path, "result_complete") == "1"
    )


def terminate(process: subprocess.Popen) -> None:
    if process.poll() is not None:
        return

    process.terminate()
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=5)


def print_recent_log(result_dir: Path) -> None:
    paths = [
        result_dir / "logs" / "fs-uae.log.txt",
        LOG_PATH,
    ]
    log_path = None
    for path in paths:
        if path.exists():
            log_path = path
            break
    if log_path is None:
        return
    lines = log_path.read_text(errors="replace").splitlines()
    print(f"\n--- recent fs-uae log: {log_path} ---")
    for line in lines[-80:]:
        print(line)


def cli_option(key: str, value: str) -> str:
    return f"--{key.replace('_', '-')}={value}"


def main() -> int:
    args = parse_args()
    scenario_id = int(SCENARIOS[args.scenario]["id"])
    frames = args.frames
    if frames is None:
        frames = int(SCENARIOS[args.scenario]["frames"])
    result_dir = RESULT_ROOT / args.machine / args.scenario
    result_dir.mkdir(parents=True, exist_ok=True)
    (result_dir / "fs-uae-base").mkdir(parents=True, exist_ok=True)
    (result_dir / "logs").mkdir(parents=True, exist_ok=True)
    (result_dir / "save-states").mkdir(parents=True, exist_ok=True)

    if not FS_UAE.exists():
        print(f"FS-UAE binary not found: {FS_UAE}", file=sys.stderr)
        return 2

    if not args.no_build:
        build_byte_brothers(scenario_id, frames)

    if not BB_BIN_PATH.exists():
        print(f"Byte Brothers harness binary not found: {BB_BIN_PATH}", file=sys.stderr)
        return 2
    if not ASSET_DIR.exists():
        print(f"Byte Brothers assets not found: {ASSET_DIR}", file=sys.stderr)
        return 2

    clean_result(result_dir)
    setup_boot_drive(result_dir)
    options = harness_options(args.machine, result_dir)
    config_path = write_config(args.machine, result_dir, options)

    if args.show_config:
        print(config_path.read_text(encoding="utf-8"))

    process = subprocess.Popen(
        [str(FS_UAE)] + [cli_option(key, value) for key, value in sorted(options.items())],
        cwd=ROOT,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )

    deadline = time.monotonic() + args.timeout
    found = None
    last_seen = None
    try:
        while time.monotonic() < deadline:
            found = result_path(result_dir)
            if found is not None and is_final_result(found):
                time.sleep(0.75)
                break
            if found is not None:
                last_seen = found
                found = None
            if process.poll() is not None:
                break
            time.sleep(0.5)
    finally:
        terminate(process)

    if found is None:
        found = result_path(result_dir)
        if found is not None and not is_final_result(found):
            last_seen = found
            found = None

    if found is None:
        print("No Byte Brothers result file was produced.", file=sys.stderr)
        phase = phase_path(result_dir)
        if phase is not None:
            print(f"\n--- phase marker: {phase} ---")
            print(phase.read_text(errors="replace"))
        gfx_phase = named_result_path(result_dir, GFX_PHASE_NAME)
        if gfx_phase is not None:
            print(f"\n--- gfx phase marker: {gfx_phase} ---")
            print(gfx_phase.read_text(errors="replace"))
        if last_seen is not None:
            print(f"\n--- partial result: {last_seen} ---")
            print(last_seen.read_text(errors="replace"))
        print_recent_log(result_dir)
        return 1

    actual_scenario_id = result_value(found, "harness_scenario_id")
    actual_frames = result_value(found, "harness_frames")
    if actual_scenario_id != str(scenario_id) or actual_frames != str(frames):
        print(
            "Byte Brothers result did not match requested harness build "
            f"(scenario={actual_scenario_id}, frames={actual_frames}).",
            file=sys.stderr,
        )
        print(found.read_text(errors="replace"))
        return 1

    print(f"Result file: {found}")
    print(found.read_text(errors="replace"))
    if args.scenario == "input":
        missing = []
        for field in (
            "input_jump_seen",
            "input_dash_seen",
            "input_move_seen",
            "input_quit_scheduled",
        ):
            if result_value(found, field) != "1":
                missing.append(field)
        bb_frame = result_int(found, "bb_frame")
        quit_frame = result_int(found, "input_quit_frame")
        if quit_frame is None or quit_frame <= 0:
            missing.append("input_quit_frame")
        if bb_frame is None or quit_frame is None or bb_frame > quit_frame + 3:
            missing.append("input_quit_latency")
        if missing:
            print(
                "Byte Brothers input harness failed: " + ", ".join(missing),
                file=sys.stderr,
            )
            return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
