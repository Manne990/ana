#!/usr/bin/env python3
"""Render deterministic Byte Brothers host frames and validate actor pixels."""

from __future__ import annotations

import argparse
import dataclasses
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

import analyze_byte_brothers_frames as visual
import run_byte_brothers as bb


ROOT = Path(__file__).resolve().parents[2]
RESULT_ROOT = ROOT / "build" / "emulator-results" / "byte-brothers-host-visual"
HOST_BINARY = ROOT / "build" / "host-harness" / "examples" / "byte_brothers" / "byte_brothers"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--scenario", choices=sorted(bb.SCENARIOS), default="scroll")
    parser.add_argument("--frames", type=int, default=720)
    parser.add_argument("--dump-every", type=int, default=2)
    parser.add_argument("--timeout", type=float, default=30.0)
    parser.add_argument("--no-build", action="store_true")
    return parser.parse_args()


def build(scenario_id: int, frames: int) -> None:
    try:
        HOST_BINARY.unlink()
    except FileNotFoundError:
        pass
    subprocess.run(
        [
            "make",
            str(HOST_BINARY.relative_to(ROOT)),
            f"BB_HARNESS_SCENARIO_ID={scenario_id}",
            f"BB_HARNESS_FRAMES={frames}",
        ],
        cwd=ROOT,
        check=True,
    )


def write_contact_sheet(frame_dir: Path, output: Path) -> None:
    frames = sorted(frame_dir.glob("frame-*.ppm"))
    if not frames:
        return
    count = min(16, len(frames))
    selected = [
        frames[round(index * (len(frames) - 1) / max(1, count - 1))]
        for index in range(count)
    ]
    list_path = output.with_suffix(".txt")
    list_path.write_text(
        "".join(f"file '{path}'\n" for path in selected),
        encoding="utf-8",
    )
    subprocess.run(
        [
            "ffmpeg",
            "-y",
            "-v",
            "error",
            "-f",
            "concat",
            "-safe",
            "0",
            "-i",
            str(list_path),
            "-vf",
            "scale=320:-1,tile=4x4",
            str(output),
        ],
        cwd=ROOT,
        check=True,
    )


def run(args: argparse.Namespace) -> int:
    scenario_id = int(bb.SCENARIOS[args.scenario]["id"])
    result_dir = RESULT_ROOT / args.scenario
    frame_dir = result_dir / "frames"
    if result_dir.exists():
        shutil.rmtree(result_dir)
    frame_dir.mkdir(parents=True)

    if not args.no_build:
        build(scenario_id, args.frames)

    env = os.environ.copy()
    env["ANA_HOST_UNPACED"] = "1"
    env["ANA_FRAME_DUMP_DIR"] = str(frame_dir)
    env["ANA_FRAME_DUMP_EVERY"] = str(max(1, args.dump_every))
    proc = subprocess.run(
        [str(HOST_BINARY)],
        cwd=ROOT,
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=args.timeout,
        check=False,
    )
    (result_dir / "host-run.log").write_text(proc.stdout, encoding="utf-8")
    if proc.returncode != 0:
        print(proc.stdout, file=sys.stderr)
        print(f"Host harness exited with {proc.returncode}.", file=sys.stderr)
        return 2

    frame_paths = sorted(frame_dir.glob("frame-*.ppm"))
    if len(frame_paths) < 20:
        print(f"Only {len(frame_paths)} host frames were dumped.", file=sys.stderr)
        return 3

    decoded = visual.decode_png_sequence(frame_dir / "frame-%06d.ppm")
    analysis = visual.analyze_rgb_frames(
        decoded,
        visual.ANALYSIS_WIDTH,
        visual.ANALYSIS_HEIGHT,
        expected_max_enemies=6 if args.scenario == "enemy-overflow" else 4,
        minimum_frames=20,
    )
    analysis_path = result_dir / "visual-analysis.json"
    analysis_path.write_text(
        json.dumps(dataclasses.asdict(analysis), indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    write_contact_sheet(frame_dir, result_dir / "contact-sheet.png")

    print(f"Host visual frames: {len(frame_paths)}")
    print(f"Host visual analysis: {analysis_path}")
    print(analysis_path.read_text(encoding="utf-8"))
    if analysis.failures:
        return 4
    return 0


def main() -> int:
    return run(parse_args())


if __name__ == "__main__":
    raise SystemExit(main())
