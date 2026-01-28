# Shipment Atomicity Constraint Tests

## Overview

Added comprehensive tests to validate that **shipments are atomic** - meaning nothing can be inserted between a shipment's pickup and delivery.

## The Problem

In the current VROOM implementation, vehicle steps (fixed jobs) and other shipments can be inserted between a shipment's pickup and delivery, breaking the atomicity constraint.

### Example Violation

```
Expected: pickup 100 → delivery 100
Actual:   pickup 100 → job 200 → job 201 → delivery 100
                        ^^^^^^^^^^^^^^^^
                        SHOULD NOT BE HERE
```

## Test Results (Current VROOM)

### Real NEMT Data - **18 Violations Found** ✅

```
Vehicle 1: Shipment 216 interrupted - 2 steps between pickup and delivery
  interrupting_steps: ['job 10000', 'job 10001']

Vehicle 1: Shipment 197 interrupted - 4 steps between pickup and delivery
  interrupting_steps: ['job 10002', 'job 10003', 'job 10004', 'job 10005']

Vehicle 5: Shipment 11 interrupted - 2 steps between pickup and delivery
  interrupting_steps: ['job 10006', 'job 10007']

... (15 more violations across 9 vehicles)
```

### Test Coverage

**32 Total Tests**:
- **12 passed** - Edge cases and simple scenarios
- **20 failed** - Proving constraint violations exist

## New Validators

### 1. `ShipmentAtomicityValidator.validate_shipment_atomicity()`

Validates that **NOTHING** is between shipment pickup and delivery - not jobs, not other shipments, nothing.

**Usage**:
```python
validator = ShipmentAtomicityValidator()
result = validator.validate_shipment_atomicity(input_data, output_data)

if not result.passed:
    for violation in result.violations:
        print(f"{violation.constraint}: {violation.message}")
```

**Checks**:
- ✓ No jobs between pickup and delivery
- ✓ No other shipments between pickup and delivery
- ✓ Direct sequence (pickup at position N, delivery at position N+1)

### 2. `StepValidator.validate_no_steps_between_shipment_pickup_delivery()`

Validates that **vehicle steps** specifically don't interrupt shipments.

**Usage**:
```python
validator = StepValidator()
result = validator.validate_no_steps_between_shipment_pickup_delivery(input_data, output_data)
```

**Checks**:
- ✓ Vehicle steps (from vehicle.steps) don't interrupt shipments
- ✓ Reports which vehicle steps are interrupting which shipments

## Test Classes

### `TestShipmentAtomicityGeneral`

Tests that ALL shipments are atomic (general constraint).

**Test Cases**:
1. `test_relation_fixtures_shipment_atomicity` - Validates relation fixtures
2. `test_real_nemt_all_shipments_atomic` - **Real NEMT data test** (found 18 violations)
3. `test_no_shipments_passes` - Edge case (no shipments)

### `TestShipmentAtomicity`

Tests that vehicle steps don't interrupt shipments (specific constraint).

**Test Cases**:
1. `test_steps_with_shipments_no_interruption` - Valid scenario
2. `test_steps_interrupt_shipment_detected` - Intentionally invalid scenario
3. `test_multiple_shipments_with_steps` - Multiple shipments with steps
4. `test_real_nemt_data_shipment_atomicity` - Real NEMT data (found 18 violations)

## Running the Tests

```bash
# Run all shipment atomicity tests
pytest e2e/test_vehicle_steps.py::TestShipmentAtomicityGeneral -v
pytest e2e/test_vehicle_steps.py::TestShipmentAtomicity -v

# Run specific test
pytest e2e/test_vehicle_steps.py::TestShipmentAtomicityGeneral::test_real_nemt_all_shipments_atomic -v
```

## Test Fixtures

### New Fixtures Created

1. **`steps_with_shipments_valid.json`** - Valid scenario (jobs before/after shipment, not between)
2. **`steps_interrupt_shipment_invalid.json`** - Invalid scenario (job between pickup/delivery)
3. **`multiple_shipments_with_steps.json`** - Multiple shipments with vehicle steps

### Existing Fixtures Tested

- All relation fixtures (5 files)
- Real NEMT data (`real_nemt_data.json` - 67KB)

## Violations Detected

### By Vehicle (Real NEMT Data)

| Vehicle | Shipments Interrupted | Total Interrupting Jobs |
|---------|----------------------|------------------------|
| 1       | 2                    | 6                      |
| 5       | 3                    | 6                      |
| 8       | 2                    | 4                      |
| 10      | 2                    | 2                      |
| 11      | 1                    | 2                      |
| 13      | 2                    | 5                      |
| 21      | 2                    | 4                      |
| 23      | 2                    | 4                      |
| 25      | 2                    | 3                      |
| **Total** | **18** | **36** |

### Most Severe Violation

**Vehicle 1, Shipment 197**:
- 4 jobs inserted between pickup and delivery
- Jobs: 10002, 10003, 10004, 10005
- Gap size: 4 positions

## Expected Behavior (After Fix)

Once VROOM is fixed to enforce shipment atomicity:

1. All shipment atomicity tests should **PASS**
2. VROOM should either:
   - **Reject** vehicle steps that would interrupt shipments (validation error)
   - **Reorder** vehicle steps to not interrupt shipments
   - **Enforce** that shipments are scheduled atomically during optimization

## Integration with Other Constraints

Shipment atomicity works alongside:

1. **Relations** (`in_direct_sequence`):
   - Relations ensure shipments are on same vehicle and in sequence
   - Atomicity ensures each shipment is not interrupted

2. **Vehicle Steps**:
   - Steps define fixed jobs on vehicles
   - Atomicity ensures steps don't break shipments
   - Steps must be placed before or after shipments, not during

3. **Time Windows**:
   - Shipment time windows apply to pickup and delivery separately
   - Atomicity ensures they remain consecutive

## Benefits

1. **Correctness**: Ensures shipments are completed without interruption
2. **Realistic**: Matches real-world constraints (can't drop off before picking up)
3. **Validation**: Catches violations in test environment before production
4. **Regression**: Prevents future changes from breaking atomicity

## Next Steps

1. **Fix VROOM** to enforce shipment atomicity constraint
2. **Re-run tests** - all should pass
3. **Add to CI/CD** for continuous validation
4. **Monitor** in production to ensure constraint is maintained
