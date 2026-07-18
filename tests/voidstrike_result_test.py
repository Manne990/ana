#!/usr/bin/env python3
"""Regression tests for the VOIDSTRIKE A1200 result contract."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "emulator"))
import voidstrike_result as result  # noqa: E402


COMMIT = "6fca1a9f29d8967fae48886448edfa1b82b90e8f"
ADF_SHA256 = "a" * 64


def passing_values() -> dict[str, str]:
    values = {key: "0" for key in result.REQUIRED_FIELDS}
    values.update(
        {
            "schema_version": result.SCHEMA_VERSION,
            "source_commit": COMMIT,
            "build_id": "voidstrike-normal-test",
            "adf_sha256": ADF_SHA256,
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
            "minimum_five_second_fps_x100": "4100",
            "result_complete": "1",
            "pass": "1",
        }
    )
    return values


class VoidstrikeResultTest(unittest.TestCase):
    def validate(self, values: dict[str, str], scenario: str = "victory") -> list[str]:
        return result.validate_result(
            values,
            source_commit=COMMIT,
            adf_sha256=ADF_SHA256,
            scenario=scenario,
            machine_profile="a1200",
            build_kind="normal",
        )

    def test_accepts_complete_matching_victory_result(self) -> None:
        self.assertEqual([], self.validate(passing_values()))

    def test_rejects_stale_adf_identity(self) -> None:
        values = passing_values()
        values["adf_sha256"] = "b" * 64
        self.assertIn("adf_sha256: result does not name the mounted ADF", self.validate(values))

    def test_rejects_low_sustained_normal_performance(self) -> None:
        values = passing_values()
        values["minimum_five_second_fps_x100"] = "3999"
        self.assertIn("normal build five-second FPS floor is below 40", self.validate(values))

    def test_requires_real_keyboard_evidence_for_keyboard_scenario(self) -> None:
        values = passing_values()
        values["requested_scenario"] = "input-keyboard"
        values["actual_scenario"] = "input-keyboard"
        values["input_keyboard_events"] = "0"
        self.assertIn(
            "input-keyboard scenario observed no keyboard input",
            self.validate(values, "input-keyboard"),
        )


if __name__ == "__main__":
    unittest.main()
