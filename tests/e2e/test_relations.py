"""
E2E tests for VROOM in_direct_sequence relation constraints

These tests validate that relations defined in the input are properly enforced:
- All shipments in a relation must be on the SAME vehicle
- Shipments must appear in DIRECT SEQUENCE (no jobs in between)
- All-or-nothing: either all steps assigned or all unassigned
- Pickup before delivery for each shipment
"""

import json
import pytest
from pathlib import Path

from tests.utils.vroom_runner import VroomRunner, VroomRunnerError
from tests.utils.validators import RelationValidator


class TestRelations:
    """Test suite for in_direct_sequence relation constraints"""

    @pytest.fixture
    def runner(self):
        """Create VROOM runner instance"""
        return VroomRunner()

    @pytest.fixture
    def validator(self):
        """Create relation validator instance"""
        return RelationValidator()

    @pytest.fixture
    def fixtures_dir(self):
        """Get path to relation fixtures directory"""
        return Path(__file__).parent.parent / "fixtures" / "relations"

    def load_fixture(self, fixtures_dir, filename):
        """Load a JSON fixture file"""
        filepath = fixtures_dir / filename
        with open(filepath) as f:
            return json.load(f)

    def test_two_shipments_in_sequence(self, runner, validator, fixtures_dir):
        """
        Test basic 2-shipment relation is enforced

        Expected: pickup 25 → delivery 25 → pickup 26 → delivery 26
        Current behavior: Likely fails (relations not enforced)
        """
        input_data = self.load_fixture(fixtures_dir, "2_shipments_valid.json")
        output = runner.run_with_json(input_data)

        result = validator.validate_in_direct_sequence(input_data, output)

        # Print details for debugging
        if not result.passed:
            print(f"\n{result}")
            if output.get("routes"):
                for route in output["routes"]:
                    steps_str = " → ".join(
                        f"{s.get('type')} {s.get('id', 'N/A')}"
                        for s in route.get("steps", [])
                    )
                    print(f"  Vehicle {route['vehicle']}: {steps_str}")

        assert result.passed, f"Relation constraint violated:\n{result}"

    def test_three_shipments_in_sequence(self, runner, validator, fixtures_dir):
        """
        Test 3+ shipment relation chain

        Expected: pickup 30 → delivery 30 → pickup 31 → delivery 31 → pickup 32 → delivery 32
        """
        input_data = self.load_fixture(fixtures_dir, "3_shipments_sequence.json")
        output = runner.run_with_json(input_data)

        result = validator.validate_in_direct_sequence(input_data, output)

        if not result.passed:
            print(f"\n{result}")

        assert result.passed, f"Relation constraint violated:\n{result}"

    def test_unassigned_relation_all_or_nothing(self, runner, validator, fixtures_dir):
        """
        Test that if relation can't fit, ALL steps are unassigned

        This fixture has capacity constraint that prevents both shipments from
        being assigned together. Expected: Both unassigned (all-or-nothing rule).
        """
        input_data = self.load_fixture(fixtures_dir, "unassigned_relation.json")
        output = runner.run_with_json(input_data)

        result = validator.validate_in_direct_sequence(input_data, output)

        if not result.passed:
            print(f"\n{result}")
            print(f"Unassigned: {output.get('unassigned', [])}")

        assert result.passed, f"Relation constraint violated:\n{result}"

    def test_multiple_relations_independent(self, runner, validator, fixtures_dir):
        """
        Test multiple relations don't interfere with each other

        Each relation should be enforced independently on potentially different vehicles.
        """
        input_data = self.load_fixture(fixtures_dir, "multiple_relations.json")
        output = runner.run_with_json(input_data)

        result = validator.validate_in_direct_sequence(input_data, output)

        if not result.passed:
            print(f"\n{result}")
            if output.get("routes"):
                for route in output["routes"]:
                    steps_str = " → ".join(
                        f"{s.get('type')} {s.get('id', 'N/A')}"
                        for s in route.get("steps", [])
                    )
                    print(f"  Vehicle {route['vehicle']}: {steps_str}")

        assert result.passed, f"Relation constraint violated:\n{result}"

    def test_mixed_jobs_and_shipments(self, runner, validator, fixtures_dir):
        """
        Test relations with additional non-relation jobs in the input

        Regular jobs (not in relations) can be scheduled freely, but relation
        shipments must still be in direct sequence.
        """
        input_data = self.load_fixture(fixtures_dir, "mixed_jobs_shipments.json")
        output = runner.run_with_json(input_data)

        result = validator.validate_in_direct_sequence(input_data, output)

        if not result.passed:
            print(f"\n{result}")

        assert result.passed, f"Relation constraint violated:\n{result}"

    @pytest.mark.parametrize(
        "fixture_file",
        [
            "2_shipments_valid.json",
            "3_shipments_sequence.json",
            "unassigned_relation.json",
            "multiple_relations.json",
            "mixed_jobs_shipments.json",
        ]
    )
    def test_all_relation_fixtures(self, runner, validator, fixtures_dir, fixture_file):
        """
        Parameterized test for all relation fixtures

        This runs the same validation against all fixture files.
        """
        input_data = self.load_fixture(fixtures_dir, fixture_file)
        output = runner.run_with_json(input_data)

        result = validator.validate_in_direct_sequence(input_data, output)

        if not result.passed:
            print(f"\n[{fixture_file}] {result}")

        assert result.passed, f"[{fixture_file}] Relation constraint violated:\n{result}"


class TestRelationEdgeCases:
    """Test edge cases and error handling"""

    @pytest.fixture
    def runner(self):
        return VroomRunner()

    @pytest.fixture
    def validator(self):
        return RelationValidator()

    def test_no_relations_defined(self, runner, validator):
        """Test input with no relations defined (should pass trivially)"""
        input_data = {
            "vehicles": [{"id": 1, "start_index": 0, "end_index": 0, "capacity": [10]}],
            "jobs": [{"id": 1, "location_index": 1, "service": 10, "delivery": [1]}],
            "matrices": {"car": {"durations": [[0, 100], [100, 0]]}},
        }

        output = runner.run_with_json(input_data)
        result = validator.validate_in_direct_sequence(input_data, output)

        assert result.passed
        assert "No relations to validate" in result.details

    def test_empty_relation_steps(self, runner, validator):
        """Test relation with empty steps array (should be ignored)"""
        input_data = {
            "vehicles": [{"id": 1, "start_index": 0, "end_index": 0, "capacity": [10]}],
            "jobs": [{"id": 1, "location_index": 1, "service": 10, "delivery": [1]}],
            "relations": [{"type": "in_direct_sequence", "steps": []}],
            "matrices": {"car": {"durations": [[0, 100], [100, 0]]}},
        }

        output = runner.run_with_json(input_data)
        result = validator.validate_in_direct_sequence(input_data, output)

        # Should pass (no steps to validate)
        assert result.passed
