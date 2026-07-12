#!/usr/bin/env python3
"""Detect Byte Brothers sprite flicker and partial-sprite artifacts in captures."""

from __future__ import annotations

import argparse
import json
import statistics
import subprocess
import sys
from dataclasses import asdict, dataclass
from pathlib import Path


ANALYSIS_WIDTH = 480
ANALYSIS_HEIGHT = 270


@dataclass(frozen=True)
class Component:
    x: int
    y: int
    width: int
    height: int
    area: int


@dataclass
class VisualAnalysis:
    frames: int
    frames_with_player: int
    frames_with_enemies: int
    player_reference_width: float
    player_reference_height: float
    player_reference_area: float
    enemy_reference_width: float
    enemy_reference_height: float
    enemy_reference_area: float
    player_flicker_frames: list[int]
    partial_player_frames: list[int]
    enemy_flicker_frames: list[int]
    partial_enemy_frames: list[int]
    enemy_overdraw_frames: list[int]
    failures: list[str]


def is_enemy_pixel(r: int, g: int, b: int) -> bool:
    return r >= 145 and r >= g + 45 and r >= b + 25 and g <= 115


def is_player_pixel(r: int, g: int, b: int) -> bool:
    return b >= 165 and g >= 145 and b >= r + 45 and g >= r + 25


def merge_stacked_components(items: list[Component]) -> list[Component]:
    """Join vertically separated color islands belonging to one sprite."""
    remaining = sorted(items, key=lambda item: (item.y, item.x))
    merged: list[Component] = []
    while remaining:
        current = remaining.pop(0)
        changed = True
        while changed:
            changed = False
            for index, other in enumerate(remaining):
                overlap_x = min(
                    current.x + current.width,
                    other.x + other.width,
                ) - max(current.x, other.x)
                vertical_gap = max(
                    other.y - (current.y + current.height),
                    current.y - (other.y + other.height),
                    0,
                )
                if overlap_x <= 0 or vertical_gap > 7:
                    continue
                min_x = min(current.x, other.x)
                min_y = min(current.y, other.y)
                max_x = max(current.x + current.width, other.x + other.width)
                max_y = max(current.y + current.height, other.y + other.height)
                current = Component(
                    min_x,
                    min_y,
                    max_x - min_x,
                    max_y - min_y,
                    current.area + other.area,
                )
                remaining.pop(index)
                changed = True
                break
        merged.append(current)
    return merged


def components_for_mask(
    frame: bytes,
    width: int,
    height: int,
    predicate,
) -> list[Component]:
    start_y = height // 5
    mask = bytearray(width * height)
    for y in range(start_y, height):
        row = y * width
        pixel = row * 3
        for x in range(width):
            if predicate(frame[pixel], frame[pixel + 1], frame[pixel + 2]):
                mask[row + x] = 1
            pixel += 3

    found: list[Component] = []
    stack: list[int] = []
    for y in range(start_y, height):
        for x in range(width):
            first = y * width + x
            if mask[first] != 1:
                continue
            mask[first] = 2
            stack.append(first)
            min_x = max_x = x
            min_y = max_y = y
            area = 0
            while stack:
                current = stack.pop()
                cy, cx = divmod(current, width)
                area += 1
                min_x = min(min_x, cx)
                max_x = max(max_x, cx)
                min_y = min(min_y, cy)
                max_y = max(max_y, cy)
                for nx, ny in (
                    (cx - 1, cy),
                    (cx + 1, cy),
                    (cx, cy - 1),
                    (cx, cy + 1),
                ):
                    if nx < 0 or nx >= width or ny < start_y or ny >= height:
                        continue
                    neighbor = ny * width + nx
                    if mask[neighbor] == 1:
                        mask[neighbor] = 2
                        stack.append(neighbor)
            found.append(
                Component(
                    min_x,
                    min_y,
                    max_x - min_x + 1,
                    max_y - min_y + 1,
                    area,
                )
            )
    return found


def actor_components(
    frame: bytes,
    width: int,
    height: int,
) -> tuple[list[Component], list[Component]]:
    enemies = [
        item
        for item in merge_stacked_components(
            components_for_mask(frame, width, height, is_enemy_pixel)
        )
        if item.area >= 30
        and item.width >= 3
        and item.height >= 8
        and item.width <= width // 7
        and item.height <= height // 3
    ]
    players = [
        item
        for item in components_for_mask(frame, width, height, is_player_pixel)
        if item.area >= 16
        and item.width >= 3
        and item.height >= 7
        and item.width <= width // 7
        and item.height <= height // 3
    ]
    return enemies, players


def robust_reference(components: list[Component]) -> tuple[float, float, float]:
    if not components:
        return 0.0, 0.0, 0.0
    areas = [item.area for item in components]
    median_area = statistics.median(areas)
    likely_full = [item for item in components if item.area >= median_area]
    return (
        float(statistics.median(item.width for item in likely_full)),
        float(statistics.median(item.height for item in likely_full)),
        float(statistics.median(item.area for item in likely_full)),
    )


def interior(component: Component, width: int) -> bool:
    margin = max(4, width // 25)
    return component.x > margin and component.x + component.width < width - margin


def components_near(first: Component, second: Component, margin: int = 6) -> bool:
    return not (
        first.x + first.width + margin < second.x
        or second.x + second.width + margin < first.x
        or first.y + first.height + margin < second.y
        or second.y + second.height + margin < first.y
    )


def analyze_rgb_frames(
    frames: list[bytes],
    width: int,
    height: int,
    expected_max_enemies: int,
    minimum_frames: int = 20,
) -> VisualAnalysis:
    enemy_frames: list[list[Component]] = []
    player_frames: list[list[Component]] = []
    for frame in frames:
        enemies, players = actor_components(frame, width, height)
        enemy_frames.append(enemies)
        player_frames.append(players)

    all_enemies = [item for items in enemy_frames for item in items]
    all_players = [item for items in player_frames for item in items]
    enemy_width, enemy_height, enemy_area = robust_reference(all_enemies)
    player_width, player_height, player_area = robust_reference(all_players)

    partial_enemy_frames: list[int] = []
    partial_player_frames: list[int] = []
    enemy_overdraw_frames: list[int] = []
    for index, items in enumerate(enemy_frames):
        if len(items) > expected_max_enemies:
            enemy_overdraw_frames.append(index)
        if enemy_width > 0 and enemy_height > 0:
            for item in items:
                if (
                    interior(item, width)
                    and item.width < enemy_width * 0.68
                    and item.height >= enemy_height * 0.65
                    and not any(
                        components_near(item, player)
                        for player in player_frames[index]
                    )
                ):
                    partial_enemy_frames.append(index)
                    break
    for index, items in enumerate(player_frames):
        if player_width <= 0 or player_height <= 0 or not items:
            continue
        item = max(items, key=lambda value: value.area)
        if (
            interior(item, width)
            and item.width < player_width * 0.55
            and item.height >= player_height * 0.60
        ):
            partial_player_frames.append(index)

    player_presence = [bool(items) for items in player_frames]
    player_flicker_frames = [
        index
        for index in range(1, max(1, len(player_presence) - 1))
        if not player_presence[index]
        and player_presence[index - 1]
        and player_presence[index + 1]
    ]

    enemy_counts = [len(items) for items in enemy_frames]
    enemy_flicker_frames: list[int] = []
    for index in range(1, max(1, len(enemy_counts) - 1)):
        if enemy_counts[index] >= min(enemy_counts[index - 1], enemy_counts[index + 1]):
            continue
        previous_interior = any(interior(item, width) for item in enemy_frames[index - 1])
        next_interior = any(interior(item, width) for item in enemy_frames[index + 1])
        if previous_interior and next_interior:
            enemy_flicker_frames.append(index)

    failures: list[str] = []
    frame_count = len(frames)
    player_count = sum(player_presence)
    enemy_count = sum(bool(items) for items in enemy_frames)
    if frame_count < minimum_frames:
        failures.append(
            f"only {frame_count} frames were decoded; expected at least {minimum_frames}"
        )
    if frame_count > 0 and player_count < int(frame_count * 0.70):
        failures.append(
            f"player detected in only {player_count}/{frame_count} frames"
        )
    if frame_count > 0 and enemy_count < int(frame_count * 0.20):
        failures.append(
            f"enemies detected in only {enemy_count}/{frame_count} frames"
        )
    if player_flicker_frames:
        failures.append(
            "player disappeared between adjacent frames: "
            + ", ".join(map(str, player_flicker_frames[:12]))
        )
    if partial_player_frames:
        failures.append(
            "partial player sprite candidates: "
            + ", ".join(map(str, sorted(set(partial_player_frames))[:12]))
        )
    if enemy_flicker_frames:
        failures.append(
            "enemy count dropped for one frame: "
            + ", ".join(map(str, enemy_flicker_frames[:12]))
        )
    if partial_enemy_frames:
        failures.append(
            "partial enemy sprite candidates: "
            + ", ".join(map(str, sorted(set(partial_enemy_frames))[:12]))
        )
    if enemy_overdraw_frames:
        failures.append(
            f"more than {expected_max_enemies} enemy components detected: "
            + ", ".join(map(str, enemy_overdraw_frames[:12]))
        )

    return VisualAnalysis(
        frames=frame_count,
        frames_with_player=player_count,
        frames_with_enemies=enemy_count,
        player_reference_width=player_width,
        player_reference_height=player_height,
        player_reference_area=player_area,
        enemy_reference_width=enemy_width,
        enemy_reference_height=enemy_height,
        enemy_reference_area=enemy_area,
        player_flicker_frames=player_flicker_frames,
        partial_player_frames=sorted(set(partial_player_frames)),
        enemy_flicker_frames=enemy_flicker_frames,
        partial_enemy_frames=sorted(set(partial_enemy_frames)),
        enemy_overdraw_frames=enemy_overdraw_frames,
        failures=failures,
    )


def decode_png_sequence(pattern: Path) -> list[bytes]:
    proc = subprocess.run(
        [
            "ffmpeg",
            "-v",
            "error",
            "-framerate",
            "30",
            "-i",
            str(pattern),
            "-vf",
            f"scale={ANALYSIS_WIDTH}:{ANALYSIS_HEIGHT},format=rgb24",
            "-f",
            "rawvideo",
            "-",
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if proc.returncode != 0:
        raise RuntimeError(proc.stderr.decode(errors="replace").strip())
    frame_bytes = ANALYSIS_WIDTH * ANALYSIS_HEIGHT * 3
    if len(proc.stdout) % frame_bytes != 0:
        raise RuntimeError("ffmpeg returned a partial RGB frame")
    return [
        proc.stdout[offset:offset + frame_bytes]
        for offset in range(0, len(proc.stdout), frame_bytes)
    ]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "pattern",
        type=Path,
        help="Image sequence such as frame-%%03d.png",
    )
    parser.add_argument("--expected-max-enemies", type=int, default=4)
    parser.add_argument("--minimum-frames", type=int, default=20)
    parser.add_argument("--output", type=Path, default=None)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    frames = decode_png_sequence(args.pattern)
    analysis = analyze_rgb_frames(
        frames,
        ANALYSIS_WIDTH,
        ANALYSIS_HEIGHT,
        args.expected_max_enemies,
        args.minimum_frames,
    )
    result = json.dumps(asdict(analysis), indent=2, sort_keys=True)
    print(result)
    if args.output is not None:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(result + "\n", encoding="utf-8")
    return 1 if analysis.failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
