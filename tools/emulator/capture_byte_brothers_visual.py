#!/usr/bin/env python3
"""Capture visible FS-UAE frames from a deterministic Byte Brothers harness run."""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

import run_byte_brothers as bb


ROOT = Path(__file__).resolve().parents[2]
VISUAL_ROOT = ROOT / "build" / "emulator-results" / "byte-brothers-visual"
VISUAL_SIGNAL_SCALE = "64:36"
MIN_USABLE_NONBLACK_RATIO = 0.02
MIN_GAMEPLAY_SIGNAL_RATIO = 0.01
MIN_GAMEPLAY_DARK_RATIO = 0.35

CGWINDOW_SWIFT = r'''
import CoreGraphics
import Foundation

let targetPid = CommandLine.arguments.count > 1 ? Int(CommandLine.arguments[1]) ?? 0 : 0
let options = CGWindowListOption(arrayLiteral: .optionOnScreenOnly, .excludeDesktopElements)
let windows = CGWindowListCopyWindowInfo(options, kCGNullWindowID) as? [[String: Any]] ?? []

for window in windows {
    let owner = window[kCGWindowOwnerName as String] as? String ?? ""
    let title = window[kCGWindowName as String] as? String ?? ""
    let pid = window[kCGWindowOwnerPID as String] as? Int ?? 0
    let windowID = window[kCGWindowNumber as String] as? Int ?? 0
    let bounds = window[kCGWindowBounds as String] as? [String: Any] ?? [:]
    let width = Int(bounds["Width"] as? Double ?? 0)
    let height = Int(bounds["Height"] as? Double ?? 0)

    if width < 200 || height < 150 {
        continue
    }
    if targetPid > 0 {
        if pid != targetPid {
            continue
        }
    } else if owner.lowercased() != "fs-uae" && !title.hasPrefix("FS-UAE") {
        continue
    }
    if true {
        let x = Int(bounds["X"] as? Double ?? 0)
        let y = Int(bounds["Y"] as? Double ?? 0)
        print("\(windowID)|\(pid)|\(x),\(y),\(width),\(height)|\(owner)|\(title)")
        exit(0)
    }
}

exit(1)
'''

SCREEN_SCALE_SWIFT = r'''
import AppKit
let scale = NSScreen.main?.backingScaleFactor ?? 1.0
print(scale)
'''


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--machine", choices=sorted(bb.MACHINE_CONFIGS), default="a1200")
    parser.add_argument("--scenario", choices=sorted(bb.SCENARIOS), default="scroll")
    parser.add_argument("--frames", type=int, default=720)
    parser.add_argument("--timeout", type=float, default=90.0)
    parser.add_argument("--phase-timeout", type=float, default=45.0)
    parser.add_argument("--captures", type=int, default=8)
    parser.add_argument("--interval", type=float, default=1.25)
    parser.add_argument(
        "--capture-mode",
        choices=("window", "fs-uae", "both", "screen-recording"),
        default="window",
        help=(
            "window captures the real macOS FS-UAE window; fs-uae uses the "
            "emulator screenshot shortcut; both captures the window first and "
            "keeps FS-UAE screenshots as diagnostics; screen-recording records "
            "the full macOS display through avfoundation and crops to FS-UAE."
        ),
    )
    parser.add_argument("--no-build", action="store_true")
    parser.add_argument(
        "--avfoundation-screen",
        default=None,
        help=(
            "Override ffmpeg AVFoundation screen input, for example 4:none. "
            "By default the script auto-detects the first Capture screen device."
        ),
    )
    parser.add_argument(
        "--isolate-fsuae",
        action="store_true",
        help="Terminate existing fs-uae emulator processes before launching the harness.",
    )
    parser.add_argument(
        "--raster-trace",
        action="store_true",
        help=(
            "Build the harness with BB_HW_RASTER_TRACE=1 for sprite timing "
            "diagnostics."
        ),
    )
    parser.add_argument(
        "--strict-raster",
        action="store_true",
        help=(
            "Build with raster tracing and fail if hardware sprite registers "
            "are written during visible scanout."
        ),
    )
    return parser.parse_args()


def cli_option(key: str, value: str) -> str:
    return f"--{key.replace('_', '-')}={value}"


def current_pngs(path: Path) -> set[Path]:
    return set(path.glob("*.png"))


def parse_ints(text: str) -> list[int]:
    values: list[int] = []
    for part in text.replace("\n", ",").split(","):
        part = part.strip()
        if not part:
            continue
        values.append(int(float(part)))
    return values


def phase_value(path: Path) -> str | None:
    try:
        for line in path.read_text(errors="replace").splitlines():
            if line.startswith("phase="):
                return line.split("=", 1)[1].strip()
    except OSError:
        return None
    return None


def result_int(path: Path, key: str) -> int:
    try:
        return int(bb.result_value(path, key) or "0")
    except ValueError:
        return 0


def validate_result_file(
    path: Path | None,
    scenario_id: int,
    frames: int,
    strict_raster: bool,
) -> list[str]:
    if path is None:
        return ["No result file was produced."]

    failures: list[str] = []
    actual_scenario_id = bb.result_value(path, "harness_scenario_id")
    actual_frames = bb.result_value(path, "harness_frames")
    if actual_scenario_id != str(scenario_id):
        failures.append(
            "Result scenario mismatch: "
            f"expected {scenario_id}, got {actual_scenario_id or '<missing>'}."
        )
    if actual_frames != str(frames):
        failures.append(
            "Result frame-count mismatch: "
            f"expected {frames}, got {actual_frames or '<missing>'}."
        )
    if not bb.is_final_result(path):
        failures.append("Result file is not a complete shutdown result.")
    if strict_raster:
        raster_checks = result_int(path, "hw_sprite_raster_checks")
        unsafe_span_writes = result_int(path, "hw_sprite_unsafe_span_writes")
        visible_writes = result_int(path, "hw_sprite_visible_raster_writes")
        if raster_checks <= 0:
            failures.append("Raster trace did not record any hardware sprite writes.")
        if unsafe_span_writes is not None:
            if unsafe_span_writes != 0:
                failures.append(
                    "Hardware sprites were updated while their scanlines "
                    f"were active: {unsafe_span_writes} writes."
                )
        elif visible_writes != 0:
            failures.append(
                "Hardware sprites were updated during visible scanout: "
                f"{visible_writes} writes."
            )
    return failures


def wait_for_phase(result_dir: Path, desired: str, deadline: float) -> Path | None:
    last_phase = None
    while time.monotonic() < deadline:
        path = bb.phase_path(result_dir)
        if path is not None:
            value = phase_value(path)
            if value != last_phase:
                print(f"Phase: {value} ({path})")
                last_phase = value
            if value == desired:
                return path
        time.sleep(0.1)
    return None


def activate_fsuae() -> None:
    subprocess.run(
        [
            "osascript",
            "-e",
            'tell application "FS-UAE" to activate',
        ],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        check=False,
    )


def fsuae_window_rect(pid: int | None = None) -> tuple[int, int, int, int] | None:
    if pid is None:
        script = (
            'tell application "System Events" to tell process "fs-uae" '
            "to get {position, size} of window 1"
        )
    else:
        script = (
            'tell application "System Events" to tell '
            f"(first process whose unix id is {pid}) "
            "to get {position, size} of window 1"
        )
    proc = subprocess.run(
        ["osascript", "-e", script],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if proc.returncode != 0:
        return None
    try:
        values = parse_ints(proc.stdout)
    except ValueError:
        return None
    if len(values) != 4:
        return None
    x, y, width, height = values
    return x, y, width, height


def fsuae_cgwindow(pid: int | None = None) -> tuple[int, str] | None:
    proc = subprocess.run(
        ["swift", "-", str(pid or 0)],
        input=CGWINDOW_SWIFT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        check=False,
    )
    if proc.returncode != 0:
        return None
    line = proc.stdout.strip().splitlines()[0] if proc.stdout.strip() else ""
    parts = line.split("|", 4)
    if len(parts) != 5:
        return None
    try:
        window_id = int(parts[0])
    except ValueError:
        return None
    _pid, rect, owner, title = parts[1:]
    return window_id, f"window_id={window_id}, pid={_pid}, rect={rect}, owner={owner}, title={title}"


def screen_backing_scale() -> float:
    proc = subprocess.run(
        ["swift", "-"],
        input=SCREEN_SCALE_SWIFT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        check=False,
    )
    if proc.returncode != 0:
        return 1.0
    try:
        value = float(proc.stdout.strip())
    except ValueError:
        return 1.0
    if value <= 0.0:
        return 1.0
    return value


def avfoundation_screen_input(override: str | None = None) -> tuple[str, str]:
    if override:
        return override, "override"

    proc = subprocess.run(
        ["ffmpeg", "-hide_banner", "-f", "avfoundation", "-list_devices", "true", "-i", ""],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    best_index: str | None = None
    best_name = ""
    for line in proc.stdout.splitlines():
        match = re.search(r"\[(\d+)\]\s+(Capture screen[^\r\n]*)", line)
        if match is None:
            continue
        index = match.group(1)
        name = match.group(2).strip()
        if best_index is None or name == "Capture screen 0":
            best_index = index
            best_name = name
        if name == "Capture screen 0":
            break

    if best_index is None:
        return "4:none", "fallback-no-capture-screen-listed"
    return f"{best_index}:none", best_name


def terminate_existing_fsuae_processes() -> None:
    subprocess.run(
        ["pkill", "-x", "fs-uae"],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        check=False,
    )
    time.sleep(0.5)


def raise_fsuae(pid: int | None = None) -> None:
    subprocess.run(
        [
            "osascript",
            "-e",
            'tell application id "no.fengestad.fs-uae" to activate',
        ],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        check=False,
    )
    if pid is None:
        return
    script = (
        'tell application "System Events" to set frontmost of '
        f"(first process whose unix id is {pid}) to true"
    )
    subprocess.run(
        ["osascript", "-e", script],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        check=False,
    )


def parse_cgwindow_detail_rect(detail: str) -> tuple[int, int, int, int] | None:
    marker = "rect="
    start = detail.find(marker)
    if start < 0:
        return None
    rect_text = detail[start + len(marker):].split(",", 4)
    if len(rect_text) < 4:
        return None
    try:
        x = int(rect_text[0])
        y = int(rect_text[1])
        width = int(rect_text[2])
        height = int(rect_text[3])
    except ValueError:
        return None
    return x, y, width, height


def preflight_screen_capture() -> tuple[bool, str]:
    with tempfile.NamedTemporaryFile(suffix=".png", delete=False) as tmp:
        path = Path(tmp.name)
    try:
        proc = subprocess.run(
            ["screencapture", "-x", str(path)],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )
        ok = proc.returncode == 0 and path.exists() and path.stat().st_size > 0
        return ok, proc.stderr.strip()
    finally:
        try:
            path.unlink()
        except OSError:
            pass


def screen_capture_permission_message(detail: str) -> str:
    text = (
        "macOS screen capture is not available to this process. Grant Screen "
        "Recording permission to the app running Codex/this shell, then restart "
        "that app and rerun the visual harness."
    )
    if detail:
        text += f" screencapture said: {detail}"
    return text


def capture_fsuae_window(path: Path, pid: int | None = None) -> tuple[bool, str]:
    window = fsuae_cgwindow(pid)
    if window is not None:
        window_id, detail = window
        rect = parse_cgwindow_detail_rect(detail)
        if rect is not None:
            raise_fsuae(pid)
            time.sleep(0.08)
            x, y, width, height = rect
            proc = subprocess.run(
                [
                    "screencapture",
                    "-x",
                    f"-R{x},{y},{width},{height}",
                    str(path),
                ],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.PIPE,
                text=True,
                check=False,
            )
            ok = proc.returncode == 0 and path.exists() and path.stat().st_size > 0
            if ok:
                return True, detail + ", method=screen-rect"

        proc = subprocess.run(
            [
                "screencapture",
                "-x",
                f"-l{window_id}",
                str(path),
            ],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )
        ok = proc.returncode == 0 and path.exists() and path.stat().st_size > 0
        if ok:
            return True, detail
        error = proc.stderr.strip()
        if error:
            return False, f"{detail}, screencapture={error}"

    rect = fsuae_window_rect(pid)
    if rect is None and pid is not None:
        rect = fsuae_window_rect(None)
    if rect is None:
        return False, "could not locate FS-UAE window"
    x, y, width, height = rect
    proc = subprocess.run(
        [
            "screencapture",
            "-x",
            f"-R{x},{y},{width},{height}",
            str(path),
        ],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
        text=True,
        check=False,
    )
    ok = proc.returncode == 0 and path.exists() and path.stat().st_size > 0
    detail = proc.stderr.strip()
    if not detail:
        detail = f"rect={x},{y},{width},{height}"
    return ok, detail


def record_fsuae_screen_frames(
    output_pattern: Path,
    pid: int | None,
    duration: float,
    fps: float,
    avfoundation_input: str,
    avfoundation_detail: str,
) -> tuple[bool, str]:
    window = fsuae_cgwindow(pid)
    if window is None:
        return False, "could not locate FS-UAE CGWindow for screen recording"

    _window_id, detail = window
    rect = parse_cgwindow_detail_rect(detail)
    if rect is None:
        return False, f"could not parse FS-UAE window rect ({detail})"

    x, y, width, height = rect
    if width <= 0 or height <= 0:
        return False, f"invalid FS-UAE window rect ({detail})"

    raise_fsuae(pid)
    time.sleep(0.2)
    output_pattern.parent.mkdir(parents=True, exist_ok=True)
    scale = screen_backing_scale()
    crop_x = int(round(x * scale))
    crop_y = int(round(y * scale))
    crop_w = int(round(width * scale))
    crop_h = int(round(height * scale))
    proc = subprocess.run(
        [
            "ffmpeg",
            "-y",
            "-hide_banner",
            "-loglevel",
            "error",
            "-f",
            "avfoundation",
            "-pixel_format",
            "bgr0",
            "-framerate",
            "30",
            "-i",
            avfoundation_input,
            "-t",
            f"{duration:.2f}",
            "-vf",
            f"crop={crop_w}:{crop_h}:{crop_x}:{crop_y},fps={fps:.3f}",
            str(output_pattern),
        ],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
        text=True,
        check=False,
    )
    ok = proc.returncode == 0 and any(output_pattern.parent.glob(output_pattern.name.replace("%03d", "*")))
    if ok:
        return True, (
            detail +
            f", scale={scale:.2f}, crop={crop_x},{crop_y},{crop_w},{crop_h}, "
            f"avfoundation={avfoundation_input} ({avfoundation_detail}), "
            "method=avfoundation-screen-recording"
        )
    error = proc.stderr.strip()
    if not error:
        error = "ffmpeg screen recording produced no frames"
    return False, f"{detail}, ffmpeg={error}"


def image_nonblack_ratio(path: Path) -> float | None:
    proc = subprocess.run(
        [
            "ffmpeg",
            "-v",
            "error",
            "-i",
            str(path),
            "-vf",
            f"scale={VISUAL_SIGNAL_SCALE},format=rgb24",
            "-f",
            "rawvideo",
            "-",
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        check=False,
    )
    data = proc.stdout
    if proc.returncode != 0 or not data:
        return None
    pixels = len(data) // 3
    if pixels <= 0:
        return None
    nonblack = 0
    for index in range(0, pixels * 3, 3):
        if max(data[index], data[index + 1], data[index + 2]) > 16:
            nonblack += 1
    return nonblack / pixels


def image_gameplay_signal(path: Path) -> tuple[float, float] | None:
    proc = subprocess.run(
        [
            "ffmpeg",
            "-v",
            "error",
            "-i",
            str(path),
            "-vf",
            f"scale={VISUAL_SIGNAL_SCALE},format=rgb24",
            "-f",
            "rawvideo",
            "-",
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        check=False,
    )
    data = proc.stdout
    if proc.returncode != 0 or not data:
        return None
    pixels = len(data) // 3
    if pixels <= 0:
        return None
    dark = 0
    signal = 0
    for index in range(0, pixels * 3, 3):
        r = data[index]
        g = data[index + 1]
        b = data[index + 2]
        if max(r, g, b) < 32:
            dark += 1
        if (
            (b > 120 and g > 70 and r < 80) or
            (r > 150 and g < 90 and b < 110) or
            (r > 170 and g > 140 and b < 70) or
            (r > 180 and g > 180 and b > 180)
        ):
            signal += 1
    return signal / pixels, dark / pixels


def capture_quality(captures: list[Path]) -> dict[Path, float]:
    quality: dict[Path, float] = {}
    for path in captures:
        ratio = image_nonblack_ratio(path)
        if ratio is not None:
            quality[path] = ratio
    return quality


def usable_capture_paths(captures: list[Path]) -> list[Path]:
    quality = capture_quality(captures)
    usable: list[Path] = []
    for path in captures:
        if quality.get(path, 0.0) < MIN_USABLE_NONBLACK_RATIO:
            continue
        if path.name.startswith(("mac-window-", "screen-recording-")):
            gameplay = image_gameplay_signal(path)
            if gameplay is None:
                continue
            signal_ratio, dark_ratio = gameplay
            if (
                signal_ratio < MIN_GAMEPLAY_SIGNAL_RATIO
                or dark_ratio < MIN_GAMEPLAY_DARK_RATIO
            ):
                continue
        usable.append(path)
    return usable


def print_capture_quality(captures: list[Path]) -> None:
    quality = capture_quality(captures)
    if not quality:
        print("Capture quality: no readable PNG frames.")
        return
    print("Capture quality:")
    for path in captures[: min(len(captures), 20)]:
        ratio = quality.get(path)
        if ratio is None:
            print(f"  {path.name}: unreadable")
            continue

        marker = "usable" if ratio >= MIN_USABLE_NONBLACK_RATIO else "black/overlay"
        if path.name.startswith(("mac-window-", "screen-recording-")):
            gameplay = image_gameplay_signal(path)
            if gameplay is None:
                marker = "unreadable gameplay area"
                print(f"  {path.name}: nonblack={ratio:.3f} ({marker})")
                continue
            signal_ratio, dark_ratio = gameplay
            if (
                ratio < MIN_USABLE_NONBLACK_RATIO
                or signal_ratio < MIN_GAMEPLAY_SIGNAL_RATIO
                or dark_ratio < MIN_GAMEPLAY_DARK_RATIO
            ):
                marker = "window-only/overlay, no gameplay surface"
            print(
                f"  {path.name}: nonblack={ratio:.3f}, "
                f"gameplay_signal={signal_ratio:.3f}, dark={dark_ratio:.3f} "
                f"({marker})"
            )
        else:
            print(f"  {path.name}: nonblack={ratio:.3f} ({marker})")


def trigger_fsuae_screenshot() -> None:
    # Command+S is FS-UAE's host-side screenshot shortcut on macOS. Avoid
    # F12+S here: it opens the emulator menu in this setup and invalidates the
    # visual capture.
    subprocess.run(
        [
            "osascript",
            "-e",
            'tell application "FS-UAE" to activate',
            "-e",
            'delay 0.05',
            "-e",
            'tell application "System Events" to key code 1 using command down',
        ],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        check=False,
    )


def convert_to_contact_sheet(captures: list[Path], output: Path) -> bool:
    if not captures:
        return False
    list_path = output.with_suffix(".txt")
    list_path.write_text(
        "".join(f"file '{path}'\n" for path in captures),
        encoding="utf-8",
    )
    tile_cols = 4
    tile_rows = max(1, (len(captures) + tile_cols - 1) // tile_cols)
    cmd = [
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
        f"scale=320:-1,tile={tile_cols}x{tile_rows}",
        str(output),
    ]
    proc = subprocess.run(cmd, cwd=ROOT, check=False)
    return proc.returncode == 0 and output.exists()


def screenshot_sort_key(path: Path) -> tuple[str, int, str]:
    stem = path.stem
    base = stem.rsplit("-", 1)[0]
    suffix = stem.rsplit("-", 1)[-1]
    try:
        index = int(suffix)
    except ValueError:
        index = 0
    return (base, index, path.name)


def select_contact_frames(captures: list[Path]) -> list[Path]:
    for prefix in (
        "screen-recording-",
        "mac-window-",
        "fs-uae-real-",
        "fs-uae-full-",
        "fs-uae-crop-",
    ):
        selected = [path for path in captures if path.name.startswith(prefix)]
        if selected:
            return sorted(selected, key=screenshot_sort_key)
    return sorted(captures, key=screenshot_sort_key)


def run_visual_capture(args: argparse.Namespace) -> int:
    scenario = bb.SCENARIOS[args.scenario]
    scenario_id = int(scenario["id"])
    result_dir = VISUAL_ROOT / args.machine / args.scenario
    capture_dir = result_dir / "captures"
    avfoundation_input = "4:none"
    avfoundation_detail = "not-used"

    if args.capture_mode in ("window", "both"):
        ok, detail = preflight_screen_capture()
        if not ok:
            print(screen_capture_permission_message(detail), file=sys.stderr)
            print(
                "Use --capture-mode fs-uae only for diagnostics; in this setup "
                "FS-UAE's internal screenshots do not show the gameplay layer.",
                file=sys.stderr,
            )
            return 2

    result_dir.mkdir(parents=True, exist_ok=True)
    if capture_dir.exists():
        shutil.rmtree(capture_dir)
    capture_dir.mkdir(parents=True)
    (result_dir / "fs-uae-base").mkdir(parents=True, exist_ok=True)
    (result_dir / "logs").mkdir(parents=True, exist_ok=True)
    (result_dir / "save-states").mkdir(parents=True, exist_ok=True)

    if not args.no_build:
        extra_cflags = (
            ["-DBB_HW_RASTER_TRACE=1"]
            if args.raster_trace or args.strict_raster
            else []
        )
        bb.build_byte_brothers(scenario_id, args.frames, extra_cflags)
    if args.capture_mode == "screen-recording":
        avfoundation_input, avfoundation_detail = avfoundation_screen_input(
            args.avfoundation_screen)
        print(
            "AVFoundation screen input: "
            f"{avfoundation_input} ({avfoundation_detail})")

    bb.clean_result(result_dir)
    bb.setup_boot_drive(result_dir)
    options = bb.harness_options(args.machine, result_dir)
    options["screenshots_output_dir"] = str(capture_dir)
    options["automatic_input_grab"] = "0"
    options["full_keyboard"] = "0"
    options["initial_input_grab"] = "0"
    options["keyboard_input_grab"] = "0"
    config_path = bb.write_config(args.machine, result_dir, options)
    print(f"Config: {config_path}")
    print(f"Capture dir: {capture_dir}")

    if args.isolate_fsuae:
        terminate_existing_fsuae_processes()

    process = subprocess.Popen(
        [str(bb.FS_UAE)] + [cli_option(key, value) for key, value in sorted(options.items())],
        cwd=ROOT,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )

    captures: list[Path] = []
    seen_captures: set[Path] = set()
    deadline = time.monotonic() + args.timeout
    try:
        activate_fsuae()
        phase_deadline = min(deadline, time.monotonic() + args.phase_timeout)
        run_phase = wait_for_phase(result_dir, "run", phase_deadline)
        if run_phase is None:
            print(
                "Timed out waiting for Byte Brothers gameplay phase; "
                "capturing whatever FS-UAE is showing.",
                file=sys.stderr,
            )
        else:
            # Let the first rendered gameplay frame settle before taking screenshots.
            time.sleep(0.35)

        if args.capture_mode == "screen-recording":
            duration = max(1.0, (args.captures * args.interval) + 0.5)
            pattern = capture_dir / "screen-recording-%03d.png"
            ok, detail = record_fsuae_screen_frames(
                pattern,
                process.pid,
                duration,
                1.0 / max(args.interval, 0.001),
                avfoundation_input,
                avfoundation_detail,
            )
            if ok:
                captures = sorted(
                    capture_dir.glob("screen-recording-*.png"),
                    key=screenshot_sort_key,
                )
                seen_captures.update(captures)
                for path in captures:
                    print(f"Captured {path} ({detail})")
            else:
                print(f"Screen recording capture failed: {detail}")

        for _ in range(0 if args.capture_mode == "screen-recording" else args.captures):
            if time.monotonic() >= deadline or process.poll() is not None:
                break
            if args.capture_mode in ("window", "both"):
                path = capture_dir / f"mac-window-{len(captures) + 1:03d}.png"
                ok, detail = capture_fsuae_window(path, process.pid)
                if ok:
                    captures.append(path)
                    seen_captures.add(path)
                    print(f"Captured {path} ({detail})")
                else:
                    print(f"Window capture failed: {detail or 'unknown error'}")

            if args.capture_mode in ("fs-uae", "both"):
                trigger_fsuae_screenshot()
                time.sleep(0.2)
                new_files = sorted(
                    current_pngs(capture_dir) - seen_captures,
                    key=screenshot_sort_key,
                )
                for path in new_files:
                    captures.append(path)
                    seen_captures.add(path)
                    print(f"Captured {path}")
            elif args.capture_mode == "window":
                time.sleep(0.05)
            time.sleep(args.interval)

        if args.capture_mode == "fs-uae" and not usable_capture_paths(captures):
            print(
                "FS-UAE internal screenshots do not contain enough visible "
                "gameplay signal; trying macOS window capture fallback."
            )
            ok, detail = preflight_screen_capture()
            if not ok:
                print(screen_capture_permission_message(detail))
                deadline = time.monotonic()
            activate_fsuae()
            for _ in range(min(args.captures, 4)):
                if time.monotonic() >= deadline or process.poll() is not None:
                    break
                path = capture_dir / f"mac-window-{len(captures) + 1:03d}.png"
                ok, detail = capture_fsuae_window(path, process.pid)
                if ok:
                    captures.append(path)
                    seen_captures.add(path)
                    print(f"Captured {path} ({detail})")
                else:
                    print(f"Window capture failed: {detail or 'unknown error'}")
                time.sleep(args.interval)

        while time.monotonic() < deadline:
            found = bb.result_path(result_dir)
            if found is not None and bb.is_final_result(found):
                break
            if process.poll() is not None:
                break
            time.sleep(0.25)
    finally:
        bb.terminate(process)

    captures = sorted(current_pngs(capture_dir), key=screenshot_sort_key)
    print(f"Captured PNG frames: {len(captures)}")
    for path in captures[: min(len(captures), 20)]:
        print(path)
    print_capture_quality(captures)

    usable_frames = usable_capture_paths(captures)
    contact_frames = usable_frames or select_contact_frames(captures)
    contact = result_dir / "contact-sheet.png"
    if convert_to_contact_sheet(contact_frames, contact):
        print(f"Contact sheet: {contact}")
    else:
        print("No contact sheet produced.")

    result = bb.result_path(result_dir)
    if result is not None:
        print(f"Result file: {result}")
        print(result.read_text(errors="replace"))
    else:
        print("No result file was produced.")
    result_failures = validate_result_file(
        result,
        scenario_id,
        args.frames,
        args.strict_raster,
    )
    if result_failures:
        print("Harness result validation failed:", file=sys.stderr)
        for failure in result_failures:
            print(f"- {failure}", file=sys.stderr)

    if not captures:
        print(
            "No FS-UAE screenshots were captured. macOS/FS-UAE shortcut capture "
            "may need Accessibility/Screen Recording permission or a focused FS-UAE window.",
            file=sys.stderr,
        )
        return 1

    if result_failures:
        return 3

    if not usable_frames:
        print(
            "Captured PNG frames are not visually usable. In this setup the "
            "FS-UAE window and screen-recording paths can see the window "
            "chrome, but the active emulator/OpenGL surface is black or "
            "overlay-only, so this capture path is not a reliable visual "
            "oracle for gameplay artifacts.",
            file=sys.stderr,
        )
        return 2

    return 0


def main() -> int:
    args = parse_args()
    return run_visual_capture(args)


if __name__ == "__main__":
    raise SystemExit(main())
