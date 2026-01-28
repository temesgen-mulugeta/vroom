"""
E2E tests for VROOM vehicle steps constraints

These tests validate that vehicle steps defined in the input are properly enforced:
- All steps MUST be assigned to the specified vehicle (hard constraint)
- Steps MUST appear in the correct ORDER
- NO other jobs/shipments between consecutive steps
- Time window constraints (forced_service) must be respected
"""

import json
import pytest
from pathlib import Path

from tests.utils.vroom_runner import VroomRunner, VroomRunnerError
from tests.utils.validators import StepValidator, ShipmentAtomicityValidator


class TestVehicleSteps:
    """Test suite for vehicle step constraints"""

    @pytest.fixture
    def runner(self):
        """Create VROOM runner instance"""
        return VroomRunner()

    @pytest.fixture
    def validator(self):
        """Create step validator instance"""
        return StepValidator()

    @pytest.fixture
    def fixtures_dir(self):
        """Get path to vehicle steps fixtures directory"""
        return Path(__file__).parent.parent / "fixtures" / "vehicle_steps"

    def load_fixture(self, fixtures_dir, filename):
        """Load a JSON fixture file"""
        filepath = fixtures_dir / filename
        with open(filepath) as f:
            return json.load(f)

    def test_two_jobs_no_interleaving(self, runner, validator, fixtures_dir):
        """
        Test 2 jobs at 8am and 6pm - no jobs between them

        Vehicle steps define these two jobs must be on vehicle 1 in order.
        Expected: Only these two jobs on vehicle 1, no other jobs in between.
        """
        input_data = self.load_fixture(fixtures_dir, "2_jobs_in_order.json")
        output = runner.run_with_json(input_data)

        result = validator.validate_vehicle_steps(input_data, output)

        if not result.passed:
            print(f"\n{result}")
            if output.get("routes"):
                for route in output["routes"]:
                    steps_str = " → ".join(
                        f"{s.get('type')} {s.get('id', 'N/A')}"
                        for s in route.get("steps", [])
                    )
                    print(f"  Vehicle {route['vehicle']}: {steps_str}")

        assert result.passed, f"Vehicle steps constraint violated:\n{result}"

    def test_steps_assigned_to_correct_vehicle(self, runner, validator, fixtures_dir):
        """
        Test steps cannot be moved to different vehicle

        Multiple vehicles with their own steps - each step must stay on
        its designated vehicle.
        """
        input_data = self.load_fixture(fixtures_dir, "multiple_vehicles_steps.json")
        output = runner.run_with_json(input_data)

        result = validator.validate_vehicle_steps(input_data, output)

        if not result.passed:
            print(f"\n{result}")

        assert result.passed, f"Vehicle steps constraint violated:\n{result}"

    def test_steps_in_correct_order(self, runner, validator, fixtures_dir):
        """
        Test steps appear in order specified

        Steps must maintain the order defined in vehicle.steps array.
        """
        input_data = self.load_fixture(fixtures_dir, "2_jobs_in_order.json")
        output = runner.run_with_json(input_data)

        result = validator.validate_vehicle_steps(input_data, output)

        if not result.passed:
            print(f"\n{result}")

        # Also check order explicitly
        if output.get("routes"):
            route = output["routes"][0]
            step_ids = [
                s.get("id")
                for s in route.get("steps", [])
                if s.get("type") == "job"
            ]

            expected_order = [10000, 10001]
            if step_ids != expected_order:
                pytest.fail(
                    f"Steps not in expected order. Got {step_ids}, expected {expected_order}"
                )

        assert result.passed, f"Vehicle steps constraint violated:\n{result}"

    def test_time_window_constraints(self, runner, validator, fixtures_dir):
        """
        Test forced_service timing constraints are respected

        Steps can have forced_service.after and forced_service.before constraints.
        """
        input_data = self.load_fixture(fixtures_dir, "jobs_with_time_windows.json")
        output = runner.run_with_json(input_data)

        result = validator.validate_vehicle_steps(input_data, output)

        if not result.passed:
            print(f"\n{result}")
            if output.get("routes"):
                for route in output["routes"]:
                    print(f"  Vehicle {route['vehicle']}:")
                    for step in route.get("steps", []):
                        if step.get("type") == "job":
                            print(
                                f"    {step.get('type')} {step.get('id')}: "
                                f"arrival={step.get('arrival')}, "
                                f"service={step.get('service', 0)}"
                            )

        assert result.passed, f"Vehicle steps constraint violated:\n{result}"

    def test_shipment_steps(self, runner, validator, fixtures_dir):
        """
        Test vehicle steps with shipments (pickup and delivery)

        Steps can reference shipment pickups and deliveries.
        """
        input_data = self.load_fixture(fixtures_dir, "shipment_steps.json")
        output = runner.run_with_json(input_data)

        result = validator.validate_vehicle_steps(input_data, output)

        if not result.passed:
            print(f"\n{result}")

        assert result.passed, f"Vehicle steps constraint violated:\n{result}"

    @pytest.mark.slow
    def test_real_nemt_scenario(self, runner, validator, fixtures_dir):
        """
        Test with real NEMT data (steps-test.json)

        This is a comprehensive test with real-world NEMT scheduling data
        including multiple vehicles, jobs with time windows, and complex constraints.
        """
        input_data = self.load_fixture(fixtures_dir, "real_nemt_data.json")
        output = runner.run_with_json(input_data)

        result = validator.validate_vehicle_steps(input_data, output)

        if not result.passed:
            print(f"\n{result}")
            # Print summary
            vehicles_with_steps = sum(
                1 for v in input_data.get("vehicles", []) if v.get("steps")
            )
            print(f"  Vehicles with steps: {vehicles_with_steps}")
            print(f"  Total violations: {len(result.violations)}")

        assert result.passed, f"Vehicle steps constraint violated:\n{result}"

    @pytest.mark.parametrize(
        "fixture_file",
        [
            "2_jobs_in_order.json",
            "jobs_with_time_windows.json",
            "multiple_vehicles_steps.json",
            "shipment_steps.json",
        ]
    )
    def test_all_step_fixtures(self, runner, validator, fixtures_dir, fixture_file):
        """
        Parameterized test for all vehicle step fixtures

        This runs the same validation against all fixture files.
        """
        input_data = self.load_fixture(fixtures_dir, fixture_file)
        output = runner.run_with_json(input_data)

        result = validator.validate_vehicle_steps(input_data, output)

        if not result.passed:
            print(f"\n[{fixture_file}] {result}")

        assert result.passed, f"[{fixture_file}] Vehicle steps constraint violated:\n{result}"


class TestShipmentAtomicityGeneral:
    """Test that ALL shipments are atomic (nothing can be between pickup and delivery)"""

    @pytest.fixture
    def runner(self):
        """Create VROOM runner instance"""
        return VroomRunner()

    @pytest.fixture
    def validator(self):
        """Create shipment atomicity validator instance"""
        return ShipmentAtomicityValidator()

    @pytest.fixture
    def fixtures_dir(self):
        """Get path to fixtures directory"""
        return Path(__file__).parent.parent / "fixtures"

    def load_fixture(self, fixtures_dir, filename):
        """Load a JSON fixture file"""
        filepath = fixtures_dir / filename
        with open(filepath) as f:
            return json.load(f)

    def test_relation_fixtures_shipment_atomicity(self, runner, validator, fixtures_dir):
        """
        Test that shipments in relation fixtures are atomic

        Relations contain shipments - validate that each shipment's pickup and delivery
        are not interrupted by other jobs/shipments.
        """
        fixtures = [
            "relations/2_shipments_valid.json",
            "relations/3_shipments_sequence.json",
            "relations/multiple_relations.json",
        ]

        for fixture_path in fixtures:
            input_data = self.load_fixture(fixtures_dir, fixture_path)
            output = runner.run_with_json(input_data)

            result = validator.validate_shipment_atomicity(input_data, output)

            if not result.passed:
                print(f"\n[{fixture_path}] {result}")

            assert result.passed, f"[{fixture_path}] Shipment atomicity violated:\n{result}"

    def test_real_nemt_all_shipments_atomic(self, runner, validator, fixtures_dir):
        """
        Test that ALL shipments in real NEMT data are atomic

        This validates that no jobs or other shipments are inserted between
        any shipment's pickup and delivery.
        """
        input_data = self.load_fixture(fixtures_dir, "vehicle_steps/real_nemt_data.json")
        output = runner.run_with_json(input_data)

        result = validator.validate_shipment_atomicity(input_data, output)

        if not result.passed:
            print(f"\n{result}")
            print(f"  Total shipment atomicity violations: {len(result.violations)}")
            # Show first few violations
            for i, violation in enumerate(result.violations[:5]):
                print(f"  [{i+1}] {violation}")

        assert result.passed, f"Shipment atomicity violated:\n{result}"

    def test_no_shipments_passes(self, runner, validator):
        """Test that input with no shipments passes trivially"""
        input_data = {
            "vehicles": [{"id": 1, "start_index": 0, "end_index": 0, "capacity": [10]}],
            "jobs": [{"id": 1, "location_index": 1, "service": 10, "delivery": [1]}],
            "matrices": {"car": {"durations": [[0, 100], [100, 0]]}},
        }

        output = runner.run_with_json(input_data)
        result = validator.validate_shipment_atomicity(input_data, output)

        assert result.passed, "Should pass when no shipments defined"


class TestShipmentAtomicity:
    """Test that vehicle steps don't interrupt shipments (pickup → delivery must be atomic)"""

    @pytest.fixture
    def runner(self):
        """Create VROOM runner instance"""
        return VroomRunner()

    @pytest.fixture
    def validator(self):
        """Create step validator instance"""
        return StepValidator()

    @pytest.fixture
    def fixtures_dir(self):
        """Get path to vehicle steps fixtures directory"""
        return Path(__file__).parent.parent / "fixtures" / "vehicle_steps"

    def load_fixture(self, fixtures_dir, filename):
        """Load a JSON fixture file"""
        filepath = fixtures_dir / filename
        with open(filepath) as f:
            return json.load(f)

    def test_steps_with_shipments_no_interruption(self, runner, validator, fixtures_dir):
        """
        Test that vehicle steps and shipments can coexist without interruption

        Expected: Job → Pickup → Delivery → Job (no jobs between pickup and delivery)
        """
        input_data = self.load_fixture(fixtures_dir, "steps_with_shipments_valid.json")
        output = runner.run_with_json(input_data)

        result = validator.validate_no_steps_between_shipment_pickup_delivery(
            input_data, output
        )

        if not result.passed:
            print(f"\n{result}")
            if output.get("routes"):
                for route in output["routes"]:
                    steps_str = " → ".join(
                        f"{s.get('type')} {s.get('id', 'N/A')}"
                        for s in route.get("steps", [])
                    )
                    print(f"  Vehicle {route['vehicle']}: {steps_str}")

        assert result.passed, f"Shipment atomicity violated:\n{result}"

    def test_steps_interrupt_shipment_detected(self, runner, validator, fixtures_dir):
        """
        Test that violation is detected when vehicle step interrupts shipment

        This fixture has a job step placed between pickup and delivery - should be detected.
        Expected: Pickup → [JOB INTERRUPTS HERE] → Delivery (VIOLATION)
        """
        input_data = self.load_fixture(fixtures_dir, "steps_interrupt_shipment_invalid.json")
        output = runner.run_with_json(input_data)

        result = validator.validate_no_steps_between_shipment_pickup_delivery(
            input_data, output
        )

        if not result.passed:
            print(f"\n{result}")
            if output.get("routes"):
                for route in output["routes"]:
                    steps_str = " → ".join(
                        f"{s.get('type')} {s.get('id', 'N/A')}"
                        for s in route.get("steps", [])
                    )
                    print(f"  Vehicle {route['vehicle']}: {steps_str}")

        # This test EXPECTS a violation (because the fixture is intentionally invalid)
        # In a perfect implementation, VROOM should reject this input or fix it
        # For now, our validator should detect it
        assert not result.passed, "Expected violation to be detected"
        assert len(result.violations) > 0, "Expected at least one violation"
        assert any(
            v.constraint == "shipment_atomicity" for v in result.violations
        ), "Expected shipment_atomicity violation"

    def test_multiple_shipments_with_steps(self, runner, validator, fixtures_dir):
        """
        Test multiple shipments with vehicle steps

        Expected: Job → Pickup1 → Delivery1 → Pickup2 → Delivery2 → Job
        Each shipment should be atomic (no interruptions).
        """
        input_data = self.load_fixture(fixtures_dir, "multiple_shipments_with_steps.json")
        output = runner.run_with_json(input_data)

        result = validator.validate_no_steps_between_shipment_pickup_delivery(
            input_data, output
        )

        if not result.passed:
            print(f"\n{result}")

        assert result.passed, f"Shipment atomicity violated:\n{result}"

    def test_real_nemt_data_shipment_atomicity(self, runner, validator, fixtures_dir):
        """
        Test real NEMT data for shipment atomicity violations

        The real NEMT data has both vehicle steps and shipments.
        Validate that no vehicle steps interrupt shipment pickup/delivery sequences.
        """
        input_data = self.load_fixture(fixtures_dir, "real_nemt_data.json")
        output = runner.run_with_json(input_data)

        result = validator.validate_no_steps_between_shipment_pickup_delivery(
            input_data, output
        )

        if not result.passed:
            print(f"\n{result}")
            print(f"  Total violations: {len(result.violations)}")
            # Show first few violations
            for i, violation in enumerate(result.violations[:3]):
                print(f"  [{i+1}] {violation}")

        assert result.passed, f"Shipment atomicity violated:\n{result}"


class TestVehicleStepsEdgeCases:
    """Test edge cases and error handling"""

    @pytest.fixture
    def runner(self):
        return VroomRunner()

    @pytest.fixture
    def validator(self):
        return StepValidator()

    def test_no_vehicle_steps_defined(self, runner, validator):
        """Test input with no vehicle steps (should pass trivially)"""
        input_data = {
            "vehicles": [{"id": 1, "start_index": 0, "end_index": 0, "capacity": [10]}],
            "jobs": [{"id": 1, "location_index": 1, "service": 10, "delivery": [1]}],
            "matrices": {"car": {"durations": [[0, 100], [100, 0]]}},
        }

        output = runner.run_with_json(input_data)
        result = validator.validate_vehicle_steps(input_data, output)

        # Should pass (no steps to validate)
        assert result.passed

    def test_vehicle_with_empty_steps(self, runner, validator):
        """Test vehicle with empty steps array (should be ignored)"""
        input_data = {
            "vehicles": [
                {"id": 1, "start_index": 0, "end_index": 0, "capacity": [10], "steps": []}
            ],
            "jobs": [{"id": 1, "location_index": 1, "service": 10, "delivery": [1]}],
            "matrices": {"car": {"durations": [[0, 100], [100, 0]]}},
        }

        output = runner.run_with_json(input_data)
        result = validator.validate_vehicle_steps(input_data, output)

        # Should pass (no steps to validate)
        assert result.passed

    def test_start_end_steps_ignored(self, runner, validator):
        """
        Test that START and END steps are ignored in validation

        START and END don't have job IDs and should not be validated.
        """
        input_data = {
            "vehicles": [
                {
                    "id": 1,
                    "start_index": 0,
                    "end_index": 0,
                    "capacity": [10],
                    "steps": [
                        {"type": "start"},
                        {"type": "job", "id": 100},
                        {"type": "end"},
                    ],
                }
            ],
            "jobs": [{"id": 100, "location_index": 1, "service": 10, "delivery": [1]}],
            "matrices": {"car": {"durations": [[0, 100], [100, 0]]}},
        }

        output = runner.run_with_json(input_data)
        result = validator.validate_vehicle_steps(input_data, output)

        # Should validate only the job step, not start/end
        assert result.passed or "start" not in str(result).lower()
