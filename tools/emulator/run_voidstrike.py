#!/usr/bin/env python3
"""Run a deterministic VOIDSTRIKE ADF scenario in an isolated FS-UAE instance."""

from __future__ import annotations

import argparse
import hashlib
import shutil
import subprocess
import sys
import time
from pathlib import Path

import voidstrike_result as contract


ROOT = Path(__file__).resolve().parents[2]
FS_UAE = Path("/Applications/FS-UAE.app/Contents/MacOS/fs-uae")
RESULT_ROOT = ROOT / "build" / "emulator-results" / "voidstrike"
RESULT_NAME = "ana_voidstrike_result.txt"
PHASE_NAME = "ana_voidstrike_phase.txt"
REQUEST_NAME = "ana_voidstrike_request.txt"
MACHINE_CONFIGS = {
    "a1200": Path.home() / "Documents" / "FS-UAE" / "Configurations" / "A1200.fs-uae",
    "a1200-fast": Path.home()
    / "Documents"
    / "FS-UAE"
    / "Configurations"
    / "A1200 - Fast.fs-uae",
}
OVERRIDE_KEYS = frozenset(
    (
        "automatic_input_grab", "base_dir", "chip_memory", "fast_memory",
        "floppy_drive_0", "floppy_drive_1", "floppy_drive_2", "floppy_drive_3",
        "floppy_drive_count", "full_keyboard", "hard_drive_0",
        "hard_drive_0_priority", "initial_input_grab", "joystick_port_0",
        "joystick_port_0_mode", "joystick_port_1", "joystick_port_1_mode",
        "keyboard_input_grab", "logs_dir", "save_states_dir", "zorro_iii_memory",
    )
)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def git_commit() -> str:
    return subprocess.check_output(
        ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
    ).strip()


def config_key(line: str) -> str | None:
    stripped = line.strip()
    if not stripped or stripped.startswith("#") or "=" not in stripped:
        return None
    return stripped.split("=", 1)[0].strip().lower()


def base_options(machine: str) -> dict[str, str]:
    path = MACHINE_CONFIGS[machine]
    if path.exists():
        lines = path.read_text(encoding="utf-8").splitlines()
    else:
        lines = ["amiga_model = A1200"]
    options: dict[str, str] = {}
    for line in lines:
        key = config_key(line)
        if key is not None and key not in OVERRIDE_KEYS:
            options[key] = line.split("=", 1)[1].strip()
    return options


def run_options(machine: str, adf: Path, result_dir: Path) -> dict[str, str]:
    options = base_options(machine)
    options.update(
        {
            "automatic_input_grab": "0",
            "base_dir": str(result_dir / "fs-uae-base"),
            "chip_memory": "2048",
            "fast_memory": "8192" if machine == "a1200-fast" else "0",
            "floppy_drive_0": str(adf.resolve()),
            "floppy_drive_count": "1",
            "full_keyboard": "1",
            "hard_drive_0": str(result_dir),
            "hard_drive_0_priority": "5",
            "initial_input_grab": "0",
            "joystick_port_0": "Mouse",
            "joystick_port_1": "Keyboard",
            "joystick_port_1_mode": "joystick",
            "keyboard_input_grab": "0",
            "logs_dir": str(result_dir / "logs"),
            "save_states_dir": str(result_dir / "save-states"),
            "zorro_iii_memory": "0",
        }
    )
    return options


def write_config(options: dict[str, str], result_dir: Path) -> Path:
    config = result_dir / "voidstrike.fs-uae"
    text = "[fs-uae]\n\n# Generated VOIDSTRIKE isolated harness configuration\n"
    text += "".join(f"{key} = {options[key]}\n" for key in sorted(options))
    config.write_text(text, encoding="utf-8")
    return config


def write_request(
    result_dir: Path, *, source_commit: str, adf_sha256: str, scenario: str,
    machine: str, build_kind: str,
) -> None:
    build_id = f"voidstrike-{build_kind}-{source_commit[:12]}-{adf_sha256[:12]}"
    (result_dir / REQUEST_NAME).write_text(
        "\n".join(
            (
                f"source_commit={source_commit}",
                f"adf_sha256={adf_sha256}",
                f"build_id={build_id}",
                f"requested_scenario={scenario}",
                f"machine_profile={machine}",
                f"build_kind={build_kind}",
                "",
            )
        ),
        encoding="utf-8",
    )


def final_result(path: Path) -> bool:
    try:
        values = contract.parse_result(path)
    except (OSError, ValueError):
        return False
    return values.get("result_complete") == "1"


def terminate(process: subprocess.Popen[bytes]) -> None:
    if process.poll() is not None:
        return
    process.terminate()
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=5)


def print_diagnostics(result_dir: Path) -> None:
    for path in (result_dir / PHASE_NAME, result_dir / "logs" / "fs-uae.log.txt"):
        if path.exists():
            print(f"\n--- {path.name} ---")
            print(path.read_text(errors="replace")[-8000:])


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--adf", type=Path, required=True)
    parser.add_argument("--scenario", choices=sorted(contract.SCENARIOS), required=True)
    parser.add_argument("--build-kind", choices=("normal", "debug"), required=True)
    parser.add_argument("--machine", choices=sorted(MACHINE_CONFIGS), default="a1200")
    parser.add_argument("--source-commit", default=None)
    parser.add_argument("--timeout", type=float, default=360.0)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if not args.adf.is_file():
        print(f"ADF not found: {args.adf}", file=sys.stderr)
        return 2
    if not FS_UAE.is_file():
        print(f"FS-UAE binary not found: {FS_UAE}", file=sys.stderr)
        return 2
    source_commit = args.source_commit or git_commit()
    adf_sha256 = sha256(args.adf)
    run_id = f"{time.time_ns()}-{adf_sha256[:12]}"
    result_dir = RESULT_ROOT / args.build_kind / args.machine / args.scenario / run_id
    result_dir.mkdir(parents=True)
    for directory in ("fs-uae-base", "logs", "save-states"):
        (result_dir / directory).mkdir()
    shutil.copy2(args.adf, result_dir / args.adf.name)
    (result_dir / "adf.sha256").write_text(f"{adf_sha256}  {args.adf.name}\n", encoding="ascii")
    write_request(
        result_dir, source_commit=source_commit, adf_sha256=adf_sha256,
        scenario=args.scenario, machine=args.machine, build_kind=args.build_kind,
    )
    options = run_options(args.machine, args.adf, result_dir)
    config = write_config(options, result_dir)
    print(f"VOIDSTRIKE isolated result directory: {result_dir}")
    print(f"ADF SHA-256: {adf_sha256}")
    process = subprocess.Popen(
        [str(FS_UAE), str(config)], cwd=ROOT,
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
    )
    result_path = result_dir / RESULT_NAME
    try:
        deadline = time.monotonic() + args.timeout
        while time.monotonic() < deadline:
            if final_result(result_path):
                break
            if process.poll() is not None:
                break
            time.sleep(0.25)
    finally:
        terminate(process)
    if not final_result(result_path):
        print("No complete VOIDSTRIKE result was produced.", file=sys.stderr)
        print_diagnostics(result_dir)
        return 1
    values = contract.parse_result(result_path)
    failures = contract.validate_result(
        values, source_commit=source_commit, adf_sha256=adf_sha256,
        scenario=args.scenario, machine_profile=args.machine, build_kind=args.build_kind,
    )
    print(result_path.read_text(encoding="utf-8"))
    if failures:
        print("VOIDSTRIKE run failed:", file=sys.stderr)
        for failure in failures:
            print(f"  - {failure}", file=sys.stderr)
        return 1
    print("VOIDSTRIKE run accepted.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
