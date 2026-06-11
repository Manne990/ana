#!/usr/bin/env python3
"""Run ANA's Amiga input probe in FS-UAE and read a host-mounted result file."""

from __future__ import annotations

import argparse
import os
import signal
import shutil
import subprocess
import sys
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
FS_UAE = Path("/Applications/FS-UAE.app/Contents/MacOS/fs-uae")
RESULT_NAME = "ana_input_probe_result.txt"
RESULT_DIR = ROOT / "build" / "emulator-results" / "input-probe"
ADF_PATH = ROOT / "build" / "adf" / "input-probe-harness.adf"
PROBE_BIN_PATH = ROOT / "build" / "amiga-harness" / "examples" / "input_probe" / "input_probe"
SYNTHETIC_PROBE_BIN_PATH = (
    ROOT / "build" / "amiga-harness" / "examples" / "input_probe_synthetic" / "input_probe"
)
LOG_PATH = Path.home() / "Documents" / "FS-UAE" / "Cache" / "Logs" / "fs-uae.log.txt"
KEY_SENDER_PATH = ROOT / "tools" / "emulator" / "send_macos_keys.py"

MACHINE_CONFIGS = {
    "a1200": Path.home() / "Documents" / "FS-UAE" / "Configurations" / "A1200.fs-uae",
    "a500": Path.home()
    / "Documents"
    / "FS-UAE"
    / "Configurations"
    / "A500 - Standard.fs-uae",
}

KEY_CODES = {
    "a": 0,
    "s": 1,
    "d": 2,
    "z": 6,
    "v": 9,
    "w": 13,
    "space": 49,
    "x": 7,
    "c": 8,
    "q": 12,
    "return": 36,
    "ctrl": 59,
    "control": 59,
    "escape": 53,
    "esc": 53,
    "left": 123,
    "right": 124,
    "up": 126,
    "down": 125,
}

OVERRIDE_KEYS = {
    "automatic_input_grab",
    "base_dir",
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
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--machine",
        choices=sorted(MACHINE_CONFIGS),
        default="a1200",
        help="FS-UAE machine profile to derive Kickstart and base settings from.",
    )
    parser.add_argument("--timeout", type=float, default=90.0)
    parser.add_argument("--no-build", action="store_true")
    parser.add_argument("--no-send-keys", action="store_true")
    parser.add_argument(
        "--synthetic",
        action="store_true",
        help="Run the Amiga-side synthetic input self-test instead of host key injection.",
    )
    parser.add_argument("--key-delay", type=float, default=0.5)
    parser.add_argument(
        "--keys",
        default="space,x,c,q,escape",
        help="Comma-separated host keys to send through macOS System Events.",
    )
    parser.add_argument("--show-config", action="store_true")
    return parser.parse_args()


def config_key(line: str) -> str | None:
    stripped = line.strip()
    if not stripped or stripped.startswith("#") or "=" not in stripped:
        return None
    return stripped.split("=", 1)[0].strip().lower()


def read_base_config(machine: str) -> list[str]:
    path = MACHINE_CONFIGS[machine]
    if not path.exists():
        model = "A1200" if machine == "a1200" else "A500/512K"
        return ["[fs-uae]\n", f"amiga_model = {model}\n"]
    return path.read_text(encoding="utf-8").splitlines(keepends=True)


def base_options(machine: str) -> dict[str, str]:
    options: dict[str, str] = {}
    for line in read_base_config(machine):
        key = config_key(line)
        if key is None or key in OVERRIDE_KEYS:
            continue
        value = line.split("=", 1)[1].strip()
        options[key] = value
    return options


def harness_options(machine: str, result_dir: Path) -> dict[str, str]:
    options = base_options(machine)
    base_dir = result_dir / "fs-uae-base"
    overrides = {
        "automatic_input_grab": "1",
        "base_dir": str(base_dir),
        "floppy_drive_count": "0",
        "full_keyboard": "1",
        "hard_drive_0": str(result_dir),
        "hard_drive_0_priority": "5",
        "initial_input_grab": "1",
        "joystick_port_0": "Mouse",
        "joystick_port_1": "none",
        "joystick_port_1_mode": "none",
        "keyboard_input_grab": "1",
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
    }

    if machine == "a500" and "chip_memory" not in options:
        overrides["chip_memory"] = "512"

    options.update(overrides)
    return options


def write_config(machine: str, result_dir: Path) -> tuple[dict[str, str], Path]:
    base_lines = read_base_config(machine)
    kept_lines = []

    for line in base_lines:
        key = config_key(line)
        if key is None or key not in OVERRIDE_KEYS:
            kept_lines.append(line)

    if not any(line.strip().lower() == "[fs-uae]" for line in kept_lines):
        kept_lines.insert(0, "[fs-uae]\n")

    options = harness_options(machine, result_dir)

    config_text = "".join(kept_lines)
    if not config_text.endswith("\n"):
        config_text += "\n"
    config_text += "\n# ANA emulator harness overrides\n"
    for key in sorted(options):
        if key in OVERRIDE_KEYS or (machine == "a500" and key == "chip_memory"):
            config_text += f"{key} = {options[key]}\n"

    config_path = result_dir / "input-probe.fs-uae"
    config_path.write_text(config_text, encoding="utf-8")
    return options, config_path


def build_probe(synthetic: bool) -> None:
    target = (
        "amiga-input-probe-synthetic-harness"
        if synthetic
        else "amiga-input-probe-harness"
    )
    subprocess.run(
        ["make", "-B", target],
        cwd=ROOT,
        check=True,
    )


def setup_boot_drive(result_dir: Path, probe_bin_path: Path) -> None:
    s_dir = result_dir / "S"
    s_dir.mkdir(parents=True, exist_ok=True)
    shutil.copy2(probe_bin_path, result_dir / "input_probe")
    (s_dir / "startup-sequence").write_text(
        "SYS:input_probe\n"
        "endcli\n",
        encoding="ascii",
    )


def clean_result(result_dir: Path) -> None:
    for path in result_dir.glob("*input_probe_result*"):
        path.unlink()
    result = result_dir / RESULT_NAME
    if result.exists():
        result.unlink()


def result_path(result_dir: Path) -> Path | None:
    direct = result_dir / RESULT_NAME
    if direct.exists():
        return direct

    for path in result_dir.iterdir():
        if path.name.lower() == RESULT_NAME:
            return path
    return None


KEY_RESULT_FIELDS = {
    "space": "seen_space",
    "x": "seen_x",
    "c": "seen_c",
    "q": "seen_q",
    "escape": "seen_escape",
    "esc": "seen_escape",
    "left": "seen_left",
    "right": "seen_right",
    "up": "seen_up",
    "down": "seen_down",
}


def parse_result(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in path.read_text(errors="replace").splitlines():
        if "=" not in line:
            continue
        key, value = line.split("=", 1)
        values[key.strip()] = value.strip()
    return values


def validate_sent_keys(path: Path, keys: list[str]) -> list[str]:
    values = parse_result(path)
    missing: list[str] = []

    for key in keys:
        field = KEY_RESULT_FIELDS.get(key)
        if field is None:
            continue
        if values.get(field) != "1":
            missing.append(key)

    return missing


def result_phase(path: Path) -> str | None:
    try:
        for line in path.read_text(errors="replace").splitlines():
            if line.startswith("phase="):
                return line.split("=", 1)[1].strip()
    except OSError:
        return None
    return None


def is_final_result(path: Path) -> bool:
    values = parse_result(path)
    return (
        values.get("phase") in ("pre_quit", "post_run")
        and values.get("write_complete") == "1"
    )


def send_keys_after_delay(keys: list[str], delay: float) -> subprocess.Popen | None:
    args = [
        sys.executable,
        str(KEY_SENDER_PATH),
        "--delay",
        str(delay),
        "--key-delay",
        "0.35",
        "--press-delay",
        "0.08",
        "--settle-delay",
        "0.75",
        "--repeat",
        "2",
        "--click",
    ] + keys

    try:
        return subprocess.Popen(
            args,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
    except OSError as exc:
        print(f"Could not start osascript key sender: {exc}", file=sys.stderr)
        return None


def terminate(process: subprocess.Popen) -> None:
    if process.poll() is not None:
        return

    process.terminate()
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=5)


def print_recent_log(machine: str, result_dir: Path) -> None:
    paths = [
        result_dir / "logs" / "fs-uae.log.txt",
        RESULT_DIR / machine / "logs" / "fs-uae.log.txt",
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
    result_name = args.machine
    if args.synthetic:
        result_name += "-synthetic"
    result_dir = RESULT_DIR / result_name
    probe_bin_path = SYNTHETIC_PROBE_BIN_PATH if args.synthetic else PROBE_BIN_PATH
    result_dir.mkdir(parents=True, exist_ok=True)
    (result_dir / "fs-uae-base").mkdir(parents=True, exist_ok=True)
    (result_dir / "logs").mkdir(parents=True, exist_ok=True)
    (result_dir / "save-states").mkdir(parents=True, exist_ok=True)

    if not FS_UAE.exists():
        print(f"FS-UAE binary not found: {FS_UAE}", file=sys.stderr)
        return 2

    if not args.no_build:
        build_probe(args.synthetic)

    if not probe_bin_path.exists():
        print(f"Input probe binary not found: {probe_bin_path}", file=sys.stderr)
        return 2

    clean_result(result_dir)
    setup_boot_drive(result_dir, probe_bin_path)
    options, config_path = write_config(args.machine, result_dir)

    if args.show_config:
        print(config_path.read_text(encoding="utf-8"))

    process = subprocess.Popen(
        [str(FS_UAE)] + [cli_option(key, value) for key, value in sorted(options.items())],
        cwd=ROOT,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    key_sender = None
    keys: list[str] = []
    if not args.no_send_keys:
        keys = [part.strip().lower() for part in args.keys.split(",") if part.strip()]

    deadline = time.monotonic() + args.timeout
    found = None
    last_seen = None
    try:
        while time.monotonic() < deadline:
            found = result_path(result_dir)
            if (
                not args.no_send_keys
                and key_sender is None
                and found is not None
                and result_phase(found) == "main_start"
            ):
                key_sender = send_keys_after_delay(keys, args.key_delay)
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

    key_sender_failed = False
    key_sender_stdout = ""
    if key_sender is not None:
        try:
            stdout, stderr = key_sender.communicate(timeout=2)
            if stdout.strip():
                key_sender_stdout = stdout.strip()
            if stderr.strip():
                print(f"osascript stderr: {stderr.strip()}", file=sys.stderr)
                if "not allowed to send keystrokes" in stderr:
                    print(
                        "Grant Accessibility permission to Codex/Terminal to "
                        "enable automated keypress verification.",
                        file=sys.stderr,
                    )
            if key_sender.returncode not in (0, None):
                key_sender_failed = True
        except subprocess.TimeoutExpired:
            key_sender.send_signal(signal.SIGTERM)
            key_sender_failed = True

    if found is None:
        found = result_path(result_dir)
        if found is not None and not is_final_result(found):
            last_seen = found
            found = None

    if found is None:
        print("No input probe result file was produced.", file=sys.stderr)
        if last_seen is not None:
            print(f"\n--- partial result: {last_seen} ---")
            print(last_seen.read_text(errors="replace"))
        print_recent_log(args.machine, result_dir)
        return 1

    print(f"Result file: {found}")
    print(found.read_text(errors="replace"))
    if args.synthetic:
        values = parse_result(found)
        if values.get("synthetic_input") != "1" or values.get("synthetic_ok") != "1":
            print("Synthetic input probe failed.", file=sys.stderr)
            return 1
    if not args.no_send_keys:
        missing = validate_sent_keys(found, keys)
        if key_sender is None:
            print("Input probe key sender never started.", file=sys.stderr)
            return 1
        if key_sender_failed:
            if key_sender_stdout:
                print(f"key sender stdout: {key_sender_stdout}", file=sys.stderr)
            print("Input probe key sender failed.", file=sys.stderr)
            return 1
        if missing:
            if key_sender_stdout:
                print(f"key sender stdout: {key_sender_stdout}", file=sys.stderr)
            print(
                "Input probe did not observe sent keys: " + ", ".join(missing),
                file=sys.stderr,
            )
            return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
