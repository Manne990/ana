#!/usr/bin/env python3
"""Capture auditable visible VOIDSTRIKE frames from an isolated FS-UAE run."""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import subprocess
import sys
import time
from pathlib import Path

import capture_byte_brothers_visual as capture
import run_voidstrike as runner


ROOT = Path(__file__).resolve().parents[2]
VISUAL_ROOT = ROOT / "build" / "emulator-results" / "voidstrike-visual"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def phase(path: Path) -> str | None:
    try:
        return next(
            (line.split("=", 1)[1].strip() for line in path.read_text(errors="replace").splitlines()
             if line.startswith("phase=")),
            None,
        )
    except OSError:
        return None


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--adf", type=Path, required=True)
    parser.add_argument("--source-commit", required=True)
    parser.add_argument("--scenario", choices=("victory", "game-over"), default="victory")
    parser.add_argument("--build-kind", choices=("normal", "debug"), default="normal")
    parser.add_argument("--machine", choices=("a1200",), default="a1200")
    parser.add_argument("--captures", type=int, default=4)
    parser.add_argument("--interval", type=float, default=0.75)
    parser.add_argument("--timeout", type=float, default=90.0)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if not args.adf.is_file():
        print(f"ADF not found: {args.adf}", file=sys.stderr)
        return 2
    ok, detail = capture.preflight_screen_capture()
    if not ok:
        print(capture.screen_capture_permission_message(detail), file=sys.stderr)
        return 2

    artifact_hash = sha256(args.adf)
    run_id = f"{time.time_ns()}-{artifact_hash[:12]}"
    result_dir = VISUAL_ROOT / args.build_kind / args.machine / args.scenario / run_id
    result_dir.mkdir(parents=True)
    for directory in ("fs-uae-base", "logs", "save-states", "captures"):
        (result_dir / directory).mkdir()
    mounted_adf = result_dir / args.adf.name
    shutil.copy2(args.adf, mounted_adf)
    if sha256(mounted_adf) != artifact_hash:
        print("Copied ADF hash does not match requested artifact.", file=sys.stderr)
        return 2
    (result_dir / "adf.sha256").write_text(f"{artifact_hash}  {args.adf.name}\n", encoding="ascii")
    runner.write_request(result_dir, source_commit=args.source_commit, adf_sha256=artifact_hash,
                         scenario=args.scenario, machine=args.machine, build_kind=args.build_kind)
    config = runner.write_config(runner.run_options(args.machine, mounted_adf, result_dir), result_dir)
    print(f"VOIDSTRIKE visual evidence directory: {result_dir}")
    process = subprocess.Popen([str(runner.FS_UAE), str(config)], cwd=ROOT,
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    captures: list[Path] = []
    observed_phase: str | None = None
    deadline = time.monotonic() + args.timeout
    try:
        while time.monotonic() < deadline and process.poll() is None:
            observed_phase = phase(result_dir / runner.PHASE_NAME)
            if observed_phase == "playing":
                break
            time.sleep(0.1)
        if observed_phase != "playing":
            print(f"No playing phase observed (last={observed_phase or 'missing'}).", file=sys.stderr)
        for index in range(args.captures):
            if process.poll() is not None:
                break
            frame = result_dir / "captures" / f"direct-window-{index + 1:03d}.png"
            captured, capture_detail = capture.capture_fsuae_window(frame, process.pid)
            if captured:
                captures.append(frame)
                print(f"Captured {frame.name} ({capture_detail})")
            else:
                print(f"Capture failed: {capture_detail}", file=sys.stderr)
            time.sleep(args.interval)
    finally:
        runner.terminate(process)

    usable = capture.usable_capture_paths(captures)
    contact = result_dir / "contact-sheet.png"
    if usable:
        capture.convert_to_contact_sheet(capture.sample_contact_frames(usable), contact)
    manifest = {
        "source_commit": args.source_commit,
        "adf_sha256": artifact_hash,
        "scenario": args.scenario,
        "build_kind": args.build_kind,
        "machine_profile": args.machine,
        "observed_phase": observed_phase,
        "capture_count": len(captures),
        "usable_capture_count": len(usable),
        "contact_sheet": str(contact) if contact.exists() else None,
    }
    (result_dir / "visual-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    if observed_phase != "playing" or not usable:
        print("VOIDSTRIKE visual evidence unavailable or unusable.", file=sys.stderr)
        return 1
    print("VOIDSTRIKE visual capture completed; inspect the contact sheet before accepting presentation.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
