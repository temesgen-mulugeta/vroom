"""Constraint validation for VROOM output"""

from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple


@dataclass
class Violation:
    """Represents a single constraint violation"""

    constraint: str
    message: str
    details: Optional[Dict] = None

    def __str__(self) -> str:
        if self.details:
            details_str = ", ".join(f"{k}={v}" for k, v in self.details.items())
            return f"{self.constraint}: {self.message} ({details_str})"
        return f"{self.constraint}: {self.message}"


@dataclass
class ValidationResult:
    """Result of constraint validation"""

    passed: bool
    violations: List[Violation] = field(default_factory=list)
    details: str = ""

    def add_violation(
        self,
        constraint: str,
        message: str,
        details: Optional[Dict] = None
    ) -> None:
        """Add a violation to the result"""
        self.passed = False
        self.violations.append(Violation(constraint, message, details))

    def __str__(self) -> str:
        if self.passed:
            return "✓ All validations passed"

        violations_str = "\n".join(f"  • {v}" for v in self.violations)
        return f"✗ {len(self.violations)} violation(s):\n{violations_str}"


class RelationValidator:
    """Validates in_direct_sequence relation constraints"""

    def validate_in_direct_sequence(
        self,
        input_data: Dict,
        output_data: Dict
    ) -> ValidationResult:
        """
        Validate in_direct_sequence constraints for all relations

        Args:
            input_data: Input JSON with relations defined
            output_data: Output JSON with routes

        Returns:
            ValidationResult with any violations found
        """
        result = ValidationResult(passed=True)

        relations = input_data.get("relations", [])
        if not relations:
            result.details = "No relations to validate"
            return result

        for rel_idx, relation in enumerate(relations):
            if relation.get("type") != "in_direct_sequence":
                continue

            self._validate_relation(
                relation, rel_idx, input_data, output_data, result
            )

        return result

    def _validate_relation(
        self,
        relation: Dict,
        rel_idx: int,
        input_data: Dict,
        output_data: Dict,
        result: ValidationResult,
    ) -> None:
        """Validate a single relation"""
        steps = relation.get("steps", [])
        if not steps:
            return

        # Extract shipment IDs from relation
        relation_shipment_ids = [step["id"] for step in steps if step.get("type") == "shipment"]

        if not relation_shipment_ids:
            return

        # Find assignments in output
        assigned_vehicles = {}
        unassigned_ids = set()

        routes = output_data.get("routes", [])
        for route in routes:
            vehicle_id = route["vehicle"]
            for route_step in route.get("steps", []):
                step_id = route_step.get("id")
                if step_id in relation_shipment_ids:
                    assigned_vehicles[step_id] = vehicle_id

        # Check unassigned
        for unassigned in output_data.get("unassigned", []):
            if unassigned.get("id") in relation_shipment_ids:
                unassigned_ids.add(unassigned["id"])

        # VALIDATION 1: All-or-nothing
        total_in_relation = len(relation_shipment_ids)
        total_assigned = len(assigned_vehicles)
        total_unassigned = len(unassigned_ids)

        if total_unassigned > 0 and total_unassigned != total_in_relation:
            result.add_violation(
                "all_or_nothing",
                f"Relation {rel_idx}: Partial assignment - {total_assigned} assigned, "
                f"{total_unassigned} unassigned (expected all or none)",
                {
                    "relation_idx": rel_idx,
                    "assigned": list(assigned_vehicles.keys()),
                    "unassigned": list(unassigned_ids),
                }
            )
            return

        # If all unassigned, that's ok (all-or-nothing satisfied)
        if total_assigned == 0:
            return

        # VALIDATION 2: Same vehicle
        vehicles = set(assigned_vehicles.values())
        if len(vehicles) > 1:
            result.add_violation(
                "same_vehicle",
                f"Relation {rel_idx}: Steps assigned to different vehicles",
                {
                    "relation_idx": rel_idx,
                    "assignments": assigned_vehicles,
                }
            )
            return

        # VALIDATION 3 & 4: Direct sequence and correct order
        vehicle_id = list(vehicles)[0]
        route = self._find_route_by_vehicle(routes, vehicle_id)

        if route:
            self._validate_sequence(
                relation_shipment_ids, rel_idx, route, result
            )

    def _validate_sequence(
        self,
        shipment_ids: List[int],
        rel_idx: int,
        route: Dict,
        result: ValidationResult,
    ) -> None:
        """Validate that shipments appear in direct sequence"""
        # Find all positions of pickups and deliveries for relation shipments
        positions = []
        route_steps = route.get("steps", [])

        for shipment_id in shipment_ids:
            # Find pickup
            pickup_pos = self._find_step_position(
                route_steps, shipment_id, "pickup"
            )
            # Find delivery
            delivery_pos = self._find_step_position(
                route_steps, shipment_id, "delivery"
            )

            if pickup_pos is None or delivery_pos is None:
                result.add_violation(
                    "missing_step",
                    f"Relation {rel_idx}: Shipment {shipment_id} missing pickup or delivery",
                    {
                        "relation_idx": rel_idx,
                        "shipment_id": shipment_id,
                        "pickup_pos": pickup_pos,
                        "delivery_pos": delivery_pos,
                    }
                )
                return

            # VALIDATION 4: Pickup before delivery for each shipment
            if pickup_pos > delivery_pos:
                result.add_violation(
                    "pickup_delivery_order",
                    f"Relation {rel_idx}: Delivery before pickup for shipment {shipment_id}",
                    {
                        "relation_idx": rel_idx,
                        "shipment_id": shipment_id,
                        "pickup_pos": pickup_pos,
                        "delivery_pos": delivery_pos,
                    }
                )
                return

            positions.append((pickup_pos, shipment_id, "pickup"))
            positions.append((delivery_pos, shipment_id, "delivery"))

        # Sort by position
        positions.sort(key=lambda x: x[0])

        # VALIDATION 3: Check no gaps in positions (direct sequence)
        for i in range(len(positions) - 1):
            current_pos = positions[i][0]
            next_pos = positions[i + 1][0]

            if next_pos - current_pos != 1:
                # Get steps in between
                between_steps = []
                for pos in range(current_pos + 1, next_pos):
                    step = route_steps[pos]
                    step_type = step.get("type", "unknown")
                    step_id = step.get("id", "N/A")
                    between_steps.append(f"{step_type} {step_id}")

                result.add_violation(
                    "direct_sequence",
                    f"Relation {rel_idx}: Jobs between relation steps (positions {current_pos} and {next_pos})",
                    {
                        "relation_idx": rel_idx,
                        "between": between_steps,
                        "gap_size": next_pos - current_pos - 1,
                    }
                )
                return

    @staticmethod
    def _find_route_by_vehicle(routes: List[Dict], vehicle_id: int) -> Optional[Dict]:
        """Find route for given vehicle ID"""
        for route in routes:
            if route.get("vehicle") == vehicle_id:
                return route
        return None

    @staticmethod
    def _find_step_position(
        steps: List[Dict],
        step_id: int,
        step_type: str
    ) -> Optional[int]:
        """Find position of step with given ID and type in route"""
        for pos, step in enumerate(steps):
            if step.get("id") == step_id and step.get("type") == step_type:
                return pos
        return None


class ShipmentAtomicityValidator:
    """Validates that shipments are atomic (no interruptions between pickup and delivery)"""

    def validate_shipment_atomicity(
        self,
        input_data: Dict,
        output_data: Dict
    ) -> ValidationResult:
        """
        Validate that NOTHING is inserted between shipment pickup and delivery

        Shipments must be atomic - no jobs, no other shipments, nothing can interrupt
        the pickup → delivery sequence.

        Args:
            input_data: Input JSON with shipments
            output_data: Output JSON with routes

        Returns:
            ValidationResult with any violations found
        """
        result = ValidationResult(passed=True)

        # Get all shipment IDs from input
        shipment_ids = set()
        for shipment in input_data.get("shipments", []):
            if "pickup" in shipment and "id" in shipment["pickup"]:
                shipment_ids.add(shipment["pickup"]["id"])

        routes = output_data.get("routes", [])

        # Validate each route
        for route in routes:
            vehicle_id = route["vehicle"]
            route_steps = route.get("steps", [])

            # Find all shipments in this route
            shipments_in_route = {}  # shipment_id -> (pickup_pos, delivery_pos)
            for pos, step in enumerate(route_steps):
                step_id = step.get("id")
                step_type = step.get("type")

                if step_id in shipment_ids:
                    if step_id not in shipments_in_route:
                        shipments_in_route[step_id] = {"pickup": None, "delivery": None}

                    if step_type == "pickup":
                        shipments_in_route[step_id]["pickup"] = pos
                    elif step_type == "delivery":
                        shipments_in_route[step_id]["delivery"] = pos

            # Check if ANYTHING is between pickup and delivery
            for shipment_id, positions in shipments_in_route.items():
                pickup_pos = positions.get("pickup")
                delivery_pos = positions.get("delivery")

                if pickup_pos is None or delivery_pos is None:
                    continue  # Incomplete shipment, skip

                # Check for ANY steps between pickup and delivery
                gap = delivery_pos - pickup_pos
                if gap > 1:
                    # There are steps between pickup and delivery
                    interrupting_steps = []
                    for pos in range(pickup_pos + 1, delivery_pos):
                        step = route_steps[pos]
                        step_type = step.get("type", "unknown")
                        step_id = step.get("id", "N/A")
                        interrupting_steps.append(f"{step_type} {step_id}")

                    result.add_violation(
                        "shipment_atomicity",
                        f"Vehicle {vehicle_id}: Shipment {shipment_id} interrupted - {len(interrupting_steps)} step(s) between pickup and delivery",
                        {
                            "vehicle_id": vehicle_id,
                            "shipment_id": shipment_id,
                            "pickup_pos": pickup_pos,
                            "delivery_pos": delivery_pos,
                            "gap_size": gap - 1,
                            "interrupting_steps": interrupting_steps,
                        }
                    )

        return result


class StepValidator:
    """Validates vehicle step constraints"""

    def validate_no_steps_between_shipment_pickup_delivery(
        self,
        input_data: Dict,
        output_data: Dict
    ) -> ValidationResult:
        """
        Validate that vehicle steps are NOT inserted between shipment pickup and delivery

        Shipments should be atomic - nothing can interrupt the pickup → delivery sequence.

        Args:
            input_data: Input JSON with vehicles and shipments
            output_data: Output JSON with routes

        Returns:
            ValidationResult with any violations found
        """
        result = ValidationResult(passed=True)

        vehicles = input_data.get("vehicles", [])
        routes = output_data.get("routes", [])

        # Build a set of vehicle step job IDs for quick lookup
        vehicle_step_ids = {}  # vehicle_id -> set of step IDs
        for vehicle in vehicles:
            if "steps" not in vehicle or not vehicle["steps"]:
                continue

            vehicle_id = vehicle["id"]
            step_ids = set()

            for step in vehicle["steps"]:
                if step.get("type") not in ["start", "end"] and "id" in step:
                    step_ids.add(step["id"])

            if step_ids:
                vehicle_step_ids[vehicle_id] = step_ids

        # Get all shipment IDs from input
        shipment_ids = set()
        for shipment in input_data.get("shipments", []):
            if "pickup" in shipment and "id" in shipment["pickup"]:
                shipment_ids.add(shipment["pickup"]["id"])

        # Validate each route
        for route in routes:
            vehicle_id = route["vehicle"]
            route_steps = route.get("steps", [])

            # Get vehicle steps for this vehicle
            if vehicle_id not in vehicle_step_ids:
                continue

            v_step_ids = vehicle_step_ids[vehicle_id]

            # Find all shipments in this route
            shipments_in_route = {}  # shipment_id -> (pickup_pos, delivery_pos)
            for pos, step in enumerate(route_steps):
                step_id = step.get("id")
                step_type = step.get("type")

                if step_id in shipment_ids:
                    if step_id not in shipments_in_route:
                        shipments_in_route[step_id] = {"pickup": None, "delivery": None}

                    if step_type == "pickup":
                        shipments_in_route[step_id]["pickup"] = pos
                    elif step_type == "delivery":
                        shipments_in_route[step_id]["delivery"] = pos

            # Check if vehicle steps are between pickup and delivery
            for shipment_id, positions in shipments_in_route.items():
                pickup_pos = positions.get("pickup")
                delivery_pos = positions.get("delivery")

                if pickup_pos is None or delivery_pos is None:
                    continue  # Incomplete shipment, skip

                # Find vehicle steps between pickup and delivery
                steps_between = []
                for pos in range(pickup_pos + 1, delivery_pos):
                    step = route_steps[pos]
                    step_id = step.get("id")

                    if step_id in v_step_ids:
                        step_type = step.get("type", "unknown")
                        steps_between.append(f"{step_type} {step_id}")

                if steps_between:
                    result.add_violation(
                        "shipment_atomicity",
                        f"Vehicle {vehicle_id}: Vehicle steps inserted between shipment {shipment_id} pickup and delivery",
                        {
                            "vehicle_id": vehicle_id,
                            "shipment_id": shipment_id,
                            "pickup_pos": pickup_pos,
                            "delivery_pos": delivery_pos,
                            "interrupting_steps": steps_between,
                        }
                    )

        return result

    def validate_vehicle_steps(
        self,
        input_data: Dict,
        output_data: Dict
    ) -> ValidationResult:
        """
        Validate vehicle step constraints

        Args:
            input_data: Input JSON with vehicle steps
            output_data: Output JSON with routes

        Returns:
            ValidationResult with any violations found
        """
        result = ValidationResult(passed=True)

        vehicles = input_data.get("vehicles", [])
        routes = output_data.get("routes", [])

        for vehicle in vehicles:
            if "steps" not in vehicle or not vehicle["steps"]:
                continue

            vehicle_id = vehicle["id"]
            expected_steps = vehicle["steps"]

            # Filter out START and END steps (they don't have IDs)
            expected_steps = [
                s for s in expected_steps
                if s.get("type") not in ["start", "end"]
            ]

            if not expected_steps:
                continue

            route = self._find_route_by_vehicle(routes, vehicle_id)
            if route is None:
                result.add_violation(
                    "vehicle_route_missing",
                    f"Vehicle {vehicle_id} has defined steps but no route in output",
                    {"vehicle_id": vehicle_id}
                )
                continue

            self._validate_vehicle_route(
                vehicle_id, expected_steps, route, result
            )

        return result

    def _validate_vehicle_route(
        self,
        vehicle_id: int,
        expected_steps: List[Dict],
        route: Dict,
        result: ValidationResult,
    ) -> None:
        """Validate steps for a single vehicle"""
        route_steps = route.get("steps", [])

        # VALIDATION 1: All steps assigned
        step_positions = []
        for expected_step in expected_steps:
            step_id = expected_step["id"]
            # Determine step type from job_type or type
            if "job_type" in expected_step:
                step_type = expected_step["job_type"].lower()
            else:
                step_type = expected_step.get("type", "job").lower()
                # Map STEP_TYPE to output types
                if step_type == "single_job":
                    step_type = "job"

            pos = self._find_step_position(route_steps, step_id, step_type)
            if pos is None:
                result.add_violation(
                    "step_not_assigned",
                    f"Vehicle {vehicle_id}: Step {step_id} ({step_type}) not found in route",
                    {
                        "vehicle_id": vehicle_id,
                        "step_id": step_id,
                        "step_type": step_type,
                    }
                )
                return

            step_positions.append((step_id, pos, step_type))

        # VALIDATION 2: Steps in correct order
        for i in range(len(step_positions) - 1):
            current_id, current_pos, current_type = step_positions[i]
            next_id, next_pos, next_type = step_positions[i + 1]

            if current_pos >= next_pos:
                result.add_violation(
                    "step_order",
                    f"Vehicle {vehicle_id}: Steps not in order (step {current_id} at pos {current_pos}, step {next_id} at pos {next_pos})",
                    {
                        "vehicle_id": vehicle_id,
                        "step_1": {"id": current_id, "pos": current_pos},
                        "step_2": {"id": next_id, "pos": next_pos},
                    }
                )
                return

        # VALIDATION 3: No interleaving (no jobs between consecutive steps)
        for i in range(len(step_positions) - 1):
            _, pos1, _ = step_positions[i]
            step_id_2, pos2, _ = step_positions[i + 1]

            steps_between = self._get_steps_between(route_steps, pos1, pos2)
            if steps_between:
                result.add_violation(
                    "step_interleaving",
                    f"Vehicle {vehicle_id}: {len(steps_between)} job(s) between consecutive steps",
                    {
                        "vehicle_id": vehicle_id,
                        "between_steps": [
                            f"{s.get('type')} {s.get('id', 'N/A')}" for s in steps_between
                        ],
                    }
                )
                return

        # VALIDATION 4: Time window constraints (if forced_service specified)
        for expected_step in expected_steps:
            if "forced_service" not in expected_step:
                continue

            self._validate_time_constraints(
                vehicle_id, expected_step, route_steps, result
            )

    def _validate_time_constraints(
        self,
        vehicle_id: int,
        expected_step: Dict,
        route_steps: List[Dict],
        result: ValidationResult,
    ) -> None:
        """Validate forced_service time constraints"""
        step_id = expected_step["id"]
        forced = expected_step["forced_service"]

        # Determine step type
        if "job_type" in expected_step:
            step_type = expected_step["job_type"].lower()
        else:
            step_type = expected_step.get("type", "job").lower()
            if step_type == "single_job":
                step_type = "job"

        # Find route step
        route_step = None
        for step in route_steps:
            if step.get("id") == step_id and step.get("type") == step_type:
                route_step = step
                break

        if not route_step:
            return  # Already caught by step_not_assigned validation

        arrival = route_step.get("arrival", 0)
        service = route_step.get("service", 0)
        completion = arrival + service

        # Check 'at' constraint
        if "at" in forced and forced["at"] is not None:
            if arrival != forced["at"]:
                result.add_violation(
                    "forced_service_at",
                    f"Vehicle {vehicle_id}: Step {step_id} arrival {arrival} != forced 'at' {forced['at']}",
                    {
                        "vehicle_id": vehicle_id,
                        "step_id": step_id,
                        "arrival": arrival,
                        "expected": forced["at"],
                    }
                )

        # Check 'after' constraint
        if "after" in forced and forced["after"] is not None:
            if arrival < forced["after"]:
                result.add_violation(
                    "forced_service_after",
                    f"Vehicle {vehicle_id}: Step {step_id} arrival {arrival} < forced 'after' {forced['after']}",
                    {
                        "vehicle_id": vehicle_id,
                        "step_id": step_id,
                        "arrival": arrival,
                        "expected_after": forced["after"],
                    }
                )

        # Check 'before' constraint
        if "before" in forced and forced["before"] is not None:
            if completion > forced["before"]:
                result.add_violation(
                    "forced_service_before",
                    f"Vehicle {vehicle_id}: Step {step_id} completion {completion} > forced 'before' {forced['before']}",
                    {
                        "vehicle_id": vehicle_id,
                        "step_id": step_id,
                        "completion": completion,
                        "expected_before": forced["before"],
                    }
                )

    @staticmethod
    def _find_route_by_vehicle(routes: List[Dict], vehicle_id: int) -> Optional[Dict]:
        """Find route for given vehicle ID"""
        for route in routes:
            if route.get("vehicle") == vehicle_id:
                return route
        return None

    @staticmethod
    def _find_step_position(
        steps: List[Dict],
        step_id: int,
        step_type: str
    ) -> Optional[int]:
        """Find position of step with given ID and type"""
        for pos, step in enumerate(steps):
            if step.get("id") == step_id and step.get("type") == step_type:
                return pos
        return None

    @staticmethod
    def _get_steps_between(
        steps: List[Dict],
        pos1: int,
        pos2: int
    ) -> List[Dict]:
        """Get all steps between two positions (exclusive)"""
        if pos2 - pos1 <= 1:
            return []
        return steps[pos1 + 1:pos2]
