#!/usr/bin/env python3
"""Patch FS-UAE profiles with ANA's recommended keyboard mapping."""

from __future__ import annotations

import argparse
import datetime as dt
from pathlib import Path


CONFIG_ROOT = Path.home() / "Documents" / "FS-UAE" / "Configurations"

KNOWN_PROFILES = [
    CONFIG_ROOT / "A1200.fs-uae",
    CONFIG_ROOT / "A1200 - Fast.fs-uae",
    CONFIG_ROOT / "A500 - Standard.fs-uae",
    CONFIG_ROOT / "A500 - Terrible Fire 536 64MB.fs-uae",
    CONFIG_ROOT / "A500 - Terrible Fire 536 64MB - 3.1.fs-uae",
]

ANA_KEYBOARD_OPTIONS = {
    "automatic_input_grab": "1",
    "full_keyboard": "1",
    "initial_input_grab": "1",
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
}

ANA_BLOCK_COMMENT = "# ANA recommended keyboard input mapping"


def config_key(line: str) -> str | None:
    stripped = line.strip()
    if not stripped or stripped.startswith("#") or "=" not in stripped:
        return None
    return stripped.split("=", 1)[0].strip().lower()


def patched_text(text: str) -> str:
    lines = text.splitlines()
    kept = []
    for line in lines:
        if line.strip() == ANA_BLOCK_COMMENT:
            continue
        if config_key(line) in ANA_KEYBOARD_OPTIONS:
            continue
        kept.append(line)

    if not any(line.strip().lower() == "[fs-uae]" for line in kept):
        kept.insert(0, "[fs-uae]")

    while kept and kept[-1] == "":
        kept.pop()

    kept.append("")
    kept.append(ANA_BLOCK_COMMENT)
    for key in sorted(ANA_KEYBOARD_OPTIONS):
        kept.append(f"{key} = {ANA_KEYBOARD_OPTIONS[key]}")

    return "\n".join(kept) + "\n"


def patch_profile(path: Path, dry_run: bool, backup: bool) -> bool:
    if not path.exists():
        print(f"skip missing: {path}")
        return False

    original = path.read_text(encoding="utf-8")
    updated = patched_text(original)
    if updated == original:
        print(f"already ok: {path}")
        return False

    if dry_run:
        print(f"would update: {path}")
        return True

    if backup:
        stamp = dt.datetime.now().strftime("%Y%m%d-%H%M%S")
        backup_path = path.with_name(f"{path.name}.bak-ana-keyboard-{stamp}")
        backup_path.write_text(original, encoding="utf-8")
        print(f"backup: {backup_path}")

    path.write_text(updated, encoding="utf-8")
    print(f"updated: {path}")
    return True


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "profiles",
        nargs="*",
        type=Path,
        help="FS-UAE profile files to patch. Defaults to ANA's known profiles.",
    )
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--no-backup", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    profiles = args.profiles if args.profiles else KNOWN_PROFILES
    changed = False

    for profile in profiles:
        changed = patch_profile(
            profile.expanduser(),
            dry_run=args.dry_run,
            backup=not args.no_backup,
        ) or changed

    return 0 if changed or args.dry_run else 0


if __name__ == "__main__":
    raise SystemExit(main())
