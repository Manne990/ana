#!/usr/bin/env python3

from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "emulator"))

from analyze_byte_brothers_frames import analyze_rgb_frames  # noqa: E402
import write_fsuae_launch_config as fsuae_launch  # noqa: E402
import voidstrike_result as voidstrike_result  # noqa: E402


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


class FsUaeLaunchConfigTest(unittest.TestCase):
    def test_resolves_relative_kickstart_from_fsuae_rom_directory(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            config_path = root / "Configurations" / "A1200.fs-uae"
            kickstart_path = root / "Kickstarts" / "amiga-os-310-a1200.rom"
            kickstart_path.parent.mkdir(parents=True)
            kickstart_path.write_bytes(b"test-rom")

            original_roots = fsuae_launch.KICKSTART_ROOTS
            try:
                fsuae_launch.KICKSTART_ROOTS = (kickstart_path.parent,)
                resolved = fsuae_launch.resolve_kickstart(
                    ["[fs-uae]\n", f"kickstart_file = {kickstart_path.name}\n"],
                    config_path,
                )
            finally:
                fsuae_launch.KICKSTART_ROOTS = original_roots

            self.assertEqual(kickstart_path.resolve(), resolved)


class VoidstrikeResultContractTest(unittest.TestCase):
    def test_rejects_stale_artifact_and_low_sustained_normal_performance(self) -> None:
        commit = "6fca1a9f29d8967fae48886448edfa1b82b90e8f"
        adf_sha256 = "a" * 64
        values = {key: "0" for key in voidstrike_result.REQUIRED_FIELDS}
        values.update(
            {
                "schema_version": voidstrike_result.SCHEMA_VERSION,
                "source_commit": commit,
                "build_id": "voidstrike-normal-test",
                "adf_sha256": "b" * 64,
                "requested_scenario": "victory",
                "actual_scenario": "victory",
                "machine_profile": "a1200",
                "terminal_state": "victory",
                "failure_reasons": "",
                "total_frames": "15000",
                "simulated_time_ms": "300000",
                "enemies_spawned": "10",
                "enemies_destroyed": "10",
                "boss_phase": "2",
                "boss_defeated": "1",
                "minimum_fps_x100": "4300",
                "average_fps_x100": "4700",
                "minimum_five_second_fps_x100": "3999",
                "slowest_frame_ms_x100": "2500",
                "slow_frame_count": "1",
                "gameplay_window_count": "2",
                "included_gameplay_window_count": "2",
                "excluded_nongameplay_window_count": "0",
                "gameplay_window_min_fps_x100": "3999",
                "update_stage_us": "10",
                "draw_stage_us": "20",
                "render_stage_us": "30",
                "present_stage_us": "40",
                "visible_enemies": "3",
                "visible_projectiles": "4",
                "visible_effects": "2",
                "visible_modules": "1",
                "result_complete": "1",
                "pass": "1",
            }
        )
        failures = voidstrike_result.validate_result(
            values,
            source_commit=commit,
            adf_sha256=adf_sha256,
            scenario="victory",
            machine_profile="a1200",
            build_kind="normal",
        )
        self.assertIn("adf_sha256: result does not name the mounted ADF", failures)
        self.assertIn("normal build five-second FPS floor is below 40", failures)

    def test_requires_explicit_input_and_module_progression_evidence(self) -> None:
        commit = "6fca1a9f29d8967fae48886448edfa1b82b90e8f"
        adf_sha256 = "a" * 64
        values = {key: "1" for key in voidstrike_result.REQUIRED_FIELDS}
        values.update(
            {
                "schema_version": voidstrike_result.SCHEMA_VERSION,
                "source_commit": commit,
                "build_id": "voidstrike-normal-test",
                "adf_sha256": adf_sha256,
                "requested_scenario": "input-keyboard",
                "actual_scenario": "input-keyboard",
                "machine_profile": "a1200",
                "terminal_state": "victory",
                "failure_reasons": "",
                "input_keyboard_ctrl_events": "0",
                "input_keyboard_space_events": "0",
                "minimum_fps_x100": "4300",
                "average_fps_x100": "4700",
                "minimum_five_second_fps_x100": "4100",
                "slowest_frame_ms_x100": "2500",
                "slow_frame_count": "1",
                "gameplay_window_count": "2",
                "included_gameplay_window_count": "2",
                "excluded_nongameplay_window_count": "0",
                "gameplay_window_min_fps_x100": "4100",
                "update_stage_us": "10",
                "draw_stage_us": "20",
                "render_stage_us": "30",
                "present_stage_us": "40",
                "visible_enemies": "3",
                "visible_projectiles": "4",
                "visible_effects": "2",
                "visible_modules": "1",
                "result_complete": "1",
                "pass": "1",
            }
        )
        failures = voidstrike_result.validate_result(
            values,
            source_commit=commit,
            adf_sha256=adf_sha256,
            scenario="input-keyboard",
            machine_profile="a1200",
            build_kind="normal",
        )
        self.assertIn("input-keyboard scenario did not observe Ctrl fire", failures)
        self.assertIn("input-keyboard scenario did not observe Space install", failures)

    def test_requires_full_freshness_and_active_gameplay_telemetry(self) -> None:
        commit = "6fca1a9f29d8967fae48886448edfa1b82b90e8f"
        adf_sha256 = "a" * 64
        values = {key: "1" for key in voidstrike_result.REQUIRED_FIELDS}
        values.update(
            {
                "schema_version": voidstrike_result.SCHEMA_VERSION,
                "source_commit": commit,
                "build_id": voidstrike_result.expected_build_id(commit, adf_sha256, "normal"),
                "adf_sha256": adf_sha256,
                "requested_scenario": "victory",
                "actual_scenario": "victory",
                "machine_profile": "a1200",
                "terminal_state": "victory",
                "failure_reasons": "",
                "result_complete": "1",
                "pass": "1",
                "total_frames": "0",
                "simulated_time_ms": "0",
                "gameplay_window_count": "0",
                "included_gameplay_window_count": "0",
                "excluded_nongameplay_window_count": "0",
                "minimum_fps_x100": "4500",
                "average_fps_x100": "4500",
                "minimum_five_second_fps_x100": "4000",
                "gameplay_window_min_fps_x100": "4000",
            }
        )
        failures = voidstrike_result.validate_result(
            values,
            source_commit=commit,
            adf_sha256=adf_sha256,
            scenario="victory",
            machine_profile="a1200",
            build_kind="normal",
        )
        self.assertIn("total_frames: expected a non-zero completed gameplay measurement", failures)
        self.assertIn("included_gameplay_window_count: expected a non-zero completed gameplay measurement", failures)


if __name__ == "__main__":
    unittest.main()
