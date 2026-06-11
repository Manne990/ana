#!/usr/bin/env python3
"""Send short macOS keyboard events to a frontmost app using CoreGraphics."""

from __future__ import annotations

import argparse
import ctypes
import subprocess
import sys
import time


KEY_CODES = {
    "a": 0,
    "s": 1,
    "d": 2,
    "z": 6,
    "x": 7,
    "c": 8,
    "v": 9,
    "q": 12,
    "w": 13,
    "return": 36,
    "space": 49,
    "escape": 53,
    "esc": 53,
    "ctrl": 59,
    "control": 59,
    "left": 123,
    "right": 124,
    "down": 125,
    "up": 126,
}

K_CG_HID_EVENT_TAP = 0


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--delay", type=float, default=0.5)
    parser.add_argument("--key-delay", type=float, default=0.25)
    parser.add_argument("--press-delay", type=float, default=0.04)
    parser.add_argument("--settle-delay", type=float, default=0.25)
    parser.add_argument("--repeat", type=int, default=1)
    parser.add_argument("--app-name", default="FS-UAE")
    parser.add_argument("--click", action="store_true")
    parser.add_argument(
        "--target",
        choices=("hid", "pid", "both"),
        default="both",
        help="CoreGraphics delivery path.",
    )
    parser.add_argument("keys", nargs="+")
    return parser.parse_args()


def focus_app(app_name: str) -> None:
    subprocess.run(
        [
            "osascript",
            "-e",
            f'tell application "{app_name}" to activate',
            "-e",
            'tell application "System Events"',
            "-e",
            f'    if exists process "{app_name}" then',
            "-e",
            f'        set frontmost of process "{app_name}" to true',
            "-e",
            '    else if exists process "fs-uae" then',
            "-e",
            '        set frontmost of process "fs-uae" to true',
            "-e",
            "    end if",
            "-e",
            "end tell",
        ],
        check=False,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )


def click_app_center(app_name: str) -> None:
    script = [
        'tell application "System Events"',
        f'    if exists process "{app_name}" then',
        f'        tell process "{app_name}"',
        "            set window_position to position of window 1",
        "            set window_size to size of window 1",
        "        end tell",
        '    else if exists process "fs-uae" then',
        '        tell process "fs-uae"',
        "            set window_position to position of window 1",
        "            set window_size to size of window 1",
        "        end tell",
        "    else",
        "        return",
        "    end if",
        "    set click_x to (item 1 of window_position) + ((item 1 of window_size) div 2)",
        "    set click_y to (item 2 of window_position) + ((item 2 of window_size) div 2)",
        "    click at {click_x, click_y}",
        "end tell",
    ]
    args = ["osascript"]
    for line in script:
        args.extend(["-e", line])
    subprocess.run(
        args,
        check=False,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )


def frontmost_process_name() -> str:
    result = subprocess.run(
        [
            "osascript",
            "-e",
            'tell application "System Events" to get name of first process whose frontmost is true',
        ],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
    )
    return result.stdout.strip()


def process_id_for_name(name: str) -> int | None:
    script = (
        'tell application "System Events" to '
        f'if exists process "{name}" then get unix id of process "{name}"'
    )
    result = subprocess.run(
        ["osascript", "-e", script],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
    )
    output = result.stdout.strip()
    if output:
        try:
            return int(output)
        except ValueError:
            pass

    result = subprocess.run(
        ["pgrep", "-x", name],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
    )
    pids = []
    for line in result.stdout.splitlines():
        try:
            pids.append(int(line.strip()))
        except ValueError:
            pass
    return pids[-1] if pids else None


def fs_uae_pid(app_name: str) -> int | None:
    for name in (app_name, "fs-uae", "FS-UAE"):
        pid = process_id_for_name(name)
        if pid is not None:
            return pid
    return None


def load_application_services() -> tuple[ctypes.CDLL, ctypes.CDLL]:
    app_services = ctypes.CDLL(
        "/System/Library/Frameworks/ApplicationServices.framework/ApplicationServices"
    )
    core_foundation = ctypes.CDLL(
        "/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation"
    )

    app_services.AXIsProcessTrusted.restype = ctypes.c_bool
    app_services.CGEventCreateKeyboardEvent.argtypes = [
        ctypes.c_void_p,
        ctypes.c_uint16,
        ctypes.c_bool,
    ]
    app_services.CGEventCreateKeyboardEvent.restype = ctypes.c_void_p
    app_services.CGEventPost.argtypes = [ctypes.c_uint32, ctypes.c_void_p]
    app_services.CGEventPost.restype = None
    app_services.CGEventPostToPid.argtypes = [ctypes.c_int, ctypes.c_void_p]
    app_services.CGEventPostToPid.restype = None
    core_foundation.CFRelease.argtypes = [ctypes.c_void_p]
    core_foundation.CFRelease.restype = None
    return app_services, core_foundation


def make_key_event(
    app_services: ctypes.CDLL,
    key_code: int,
    key_down: bool,
) -> int:
    event = app_services.CGEventCreateKeyboardEvent(None, key_code, key_down)
    return int(event or 0)


def post_key(
    app_services: ctypes.CDLL,
    core_foundation: ctypes.CDLL,
    key_code: int,
    key_down: bool,
    target: str,
    pid: int | None,
) -> bool:
    if target in ("hid", "both"):
        event = make_key_event(app_services, key_code, key_down)
        if not event:
            return False
        app_services.CGEventPost(K_CG_HID_EVENT_TAP, event)
        core_foundation.CFRelease(event)

    if target in ("pid", "both"):
        if pid is None:
            return False
        event = make_key_event(app_services, key_code, key_down)
        if not event:
            return False
        app_services.CGEventPostToPid(pid, event)
        core_foundation.CFRelease(event)

    return True


def main() -> int:
    args = parse_args()
    app_services, core_foundation = load_application_services()

    if not app_services.AXIsProcessTrusted():
        print(
            "macOS Accessibility does not trust this key sender process.",
            file=sys.stderr,
        )
        return 3

    unknown = [key for key in args.keys if key.lower() not in KEY_CODES]
    if unknown:
        print("Unknown keys: " + ", ".join(unknown), file=sys.stderr)
        return 2

    time.sleep(args.delay)
    print(f"frontmost_before={frontmost_process_name()}")
    focus_app(args.app_name)
    time.sleep(0.25)
    if args.click:
        click_app_center(args.app_name)
        time.sleep(args.settle_delay)
    print(f"frontmost_after={frontmost_process_name()}")
    pid = fs_uae_pid(args.app_name)
    print(f"target={args.target}")
    print(f"pid={pid if pid is not None else 0}")

    for key in args.keys:
        key_code = KEY_CODES[key.lower()]
        repeat_count = max(1, args.repeat)
        for _ in range(repeat_count):
            if not post_key(
                    app_services,
                    core_foundation,
                    key_code,
                    True,
                    args.target,
                    pid):
                print(f"Could not create key-down event for {key}.", file=sys.stderr)
                return 1
            time.sleep(args.press_delay)
            if not post_key(
                    app_services,
                    core_foundation,
                    key_code,
                    False,
                    args.target,
                    pid):
                print(f"Could not create key-up event for {key}.", file=sys.stderr)
                return 1
            time.sleep(args.key_delay)
        print(f"sent={key}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
