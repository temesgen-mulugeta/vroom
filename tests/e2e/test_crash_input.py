"""Regression test for crash input handling."""

import json
from pathlib import Path

import pytest

from tests.utils.vroom_runner import VroomRunner


@pytest.mark.slow
def test_crash_input_graceful_fallback():
    fixture_path = (
        Path(__file__).parent.parent / "fixtures" / "crash" / "vroom-crash-100-matrix.json"
    )

    with open(fixture_path, "r", encoding="utf-8") as f:
        input_data = json.load(f)

    output = VroomRunner().run_with_json(input_data)

    assert output["code"] == 0
    assert len(output.get("unassigned", [])) > 0


@pytest.mark.slow
def test_crash_input_stress_matrix():
    fixture_path = (
        Path(__file__).parent.parent / "fixtures" / "crash" / "vroom-crash-matrix.json"
    )

    with open(fixture_path, "r", encoding="utf-8") as f:
        input_data = json.load(f)

    output = VroomRunner().run_with_json(input_data)

    assert output["code"] == 0
    assert len(output.get("unassigned", [])) > 0
