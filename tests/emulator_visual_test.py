#!/usr/bin/env python3

from __future__ import annotations

import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "emulator"))

from analyze_byte_brothers_frames import analyze_rgb_frames  # noqa: E402


WIDTH = 160
HEIGHT = 90


def frame_with_actors(
    enemy_width: int = 14,
    player: bool = True,
) -> bytes:
    frame = bytearray(WIDTH * HEIGHT * 3)

    def fill(x: int, y: int, width: int, height: int, color: tuple[int, int, int]) -> None:
        for py in range(y, y + height):
            for px in range(x, x + width):
                offset = (py * WIDTH + px) * 3
                frame[offset:offset + 3] = bytes(color)

    fill(45, 35, enemy_width, 18, (235, 35, 55))
    if player:
        fill(80, 60, 12, 12, (35, 190, 240))
    return bytes(frame)


class VisualAnalysisTest(unittest.TestCase):
    def test_accepts_stable_full_sprites(self) -> None:
        analysis = analyze_rgb_frames(
            [frame_with_actors() for _ in range(12)],
            WIDTH,
            HEIGHT,
            expected_max_enemies=1,
            minimum_frames=8,
        )
        self.assertEqual([], analysis.failures)

    def test_detects_half_enemy_sprite(self) -> None:
        frames = [frame_with_actors() for _ in range(12)]
        frames[6] = frame_with_actors(enemy_width=6)
        analysis = analyze_rgb_frames(
            frames,
            WIDTH,
            HEIGHT,
            expected_max_enemies=1,
            minimum_frames=8,
        )
        self.assertIn(6, analysis.partial_enemy_frames)
        self.assertTrue(analysis.failures)

    def test_detects_single_frame_player_flicker(self) -> None:
        frames = [frame_with_actors() for _ in range(12)]
        frames[5] = frame_with_actors(player=False)
        analysis = analyze_rgb_frames(
            frames,
            WIDTH,
            HEIGHT,
            expected_max_enemies=1,
            minimum_frames=8,
        )
        self.assertIn(5, analysis.player_flicker_frames)
        self.assertTrue(analysis.failures)


if __name__ == "__main__":
    unittest.main()
