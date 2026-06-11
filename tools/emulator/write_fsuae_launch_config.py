#!/usr/bin/env python3
"""Write a reproducible FS-UAE launch config for ANA ADFs."""

from __future__ import annotations

import argparse
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
CONFIG_ROOT = Path.home() / "Documents" / "FS-UAE" / "Configurations"

PROFILES = {
    "a1200": CONFIG_ROOT / "A1200.fs-uae",
    "a1200-fast": CONFIG_ROOT / "A1200 - Fast.fs-uae",
    "a500-tf536": CONFIG_ROOT / "A500 - Terrible Fire 536 64MB.fs-uae",
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

KEYBOARD_MAPPING = {
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
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--profile",
        choices=sorted(PROFILES),
        default="a1200",
        help="Base FS-UAE profile used for ROM/model settings.",
    )
    parser.add_argument(
        "--base-config",
        type=Path,
        default=None,
        help="Explicit base .fs-uae file. Overrides --profile.",
    )
    parser.add_argument("--adf", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    return parser.parse_args()


def config_key(line: str) -> str | None:
    stripped = line.strip()
    if not stripped or stripped.startswith("#") or "=" not in stripped:
        return None
    return stripped.split("=", 1)[0].strip().lower()


def read_base_config(args: argparse.Namespace) -> list[str]:
    path = args.base_config if args.base_config is not None else PROFILES[args.profile]
    if path.exists():
        return path.read_text(encoding="utf-8").splitlines(keepends=True)
    if args.profile.startswith("a500"):
        return ["[fs-uae]\n", "amiga_model = A500/512K\n"]
    return ["[fs-uae]\n", "amiga_model = A1200\n"]


def launch_options(args: argparse.Namespace) -> dict[str, str]:
    output_base = (args.output.parent / args.output.stem).resolve()
    options = {
        "automatic_input_grab": "1",
        "base_dir": str(output_base / "base"),
        "floppy_drive_0": str(args.adf.resolve()),
        "floppy_drive_count": "1",
        "full_keyboard": "1",
        "initial_input_grab": "1",
        "joystick_port_0": "Mouse",
        "joystick_port_1": "none",
        "joystick_port_1_mode": "none",
        "keyboard_input_grab": "1",
        "logs_dir": str(output_base / "logs"),
        "save_states_dir": str(output_base / "save-states"),
    }
    options.update(KEYBOARD_MAPPING)
    return options


def write_config(args: argparse.Namespace) -> Path:
    kept_lines: list[str] = []

    for line in read_base_config(args):
        key = config_key(line)
        if key is None or key not in OVERRIDE_KEYS:
            kept_lines.append(line)

    if not any(line.strip().lower() == "[fs-uae]" for line in kept_lines):
        kept_lines.insert(0, "[fs-uae]\n")

    config_text = "".join(kept_lines)
    if not config_text.endswith("\n"):
        config_text += "\n"

    config_text += "\n# ANA reproducible launch overrides\n"
    for key, value in sorted(launch_options(args).items()):
        config_text += f"{key} = {value}\n"

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(config_text, encoding="utf-8")
    return args.output


def main() -> int:
    args = parse_args()
    if not args.adf.exists():
        raise SystemExit(f"ADF not found: {args.adf}")
    output = write_config(args)
    print(f"Wrote {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
