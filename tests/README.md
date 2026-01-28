# VROOM E2E Test Suite

End-to-end tests for validating VROOM constraint enforcement, specifically:
1. **Relations (`in_direct_sequence`)**: Shipment sequence constraints
2. **Vehicle Steps**: Fixed step ordering and assignment constraints

## Setup

### 1. Build VROOM Binary

Before running tests, you must build the VROOM binary:

```bash
cd src
make clean && make -j4 USE_ROUTING=false
```

The binary will be created at `bin/vroom`.

### 2. Install Python Dependencies

```bash
cd tests
pip install -r requirements.txt
```

Or use a virtual environment:

```bash
python3 -m venv venv
source venv/bin/activate  # On Windows: venv\Scripts\activate
pip install -r requirements.txt
```

## Running Tests

### Run All Tests

```bash
# From the tests/ directory
pytest -v

# Or from the project root
pytest tests/ -v
```

### Run Specific Test Suites

```bash
# Only relation tests
pytest tests/e2e/test_relations.py -v

# Only vehicle steps tests
pytest tests/e2e/test_vehicle_steps.py -v
```

### Run Specific Tests

```bash
# Run a single test
pytest tests/e2e/test_relations.py::TestRelations::test_two_shipments_in_sequence -v

# Run all parameterized tests
pytest tests/e2e/test_relations.py::TestRelations::test_all_relation_fixtures -v
```

### Run with Markers

```bash
# Run only slow tests
pytest -m slow -v

# Skip slow tests
pytest -m "not slow" -v

# Run only relation tests
pytest -m relations -v

# Run only step tests
pytest -m steps -v
```

### Detailed Output

```bash
# Show detailed output with stdout/stderr
pytest -vv --tb=short -s

# Show only failed tests with full traceback
pytest -vv --tb=long
```

### Generate JSON Report

```bash
pytest --json-report --json-report-file=report.json
```

## Test Structure

```
tests/
├── e2e/
│   ├── test_relations.py           # Relation constraint tests
│   └── test_vehicle_steps.py       # Vehicle steps tests
├── utils/
│   ├── vroom_runner.py             # VROOM execution wrapper
│   └── validators.py               # Constraint validators
├── fixtures/
│   ├── relations/                  # Test JSON files for relations
│   │   ├── 2_shipments_valid.json
│   │   ├── 3_shipments_sequence.json
│   │   ├── unassigned_relation.json
│   │   ├── multiple_relations.json
│   │   └── mixed_jobs_shipments.json
│   └── vehicle_steps/              # Test JSON files for steps
│       ├── 2_jobs_in_order.json
│       ├── jobs_with_time_windows.json
│       ├── multiple_vehicles_steps.json
│       ├── shipment_steps.json
│       └── real_nemt_data.json
├── conftest.py                     # Pytest configuration
├── requirements.txt                # Python dependencies
└── README.md                       # This file
```

## Test Coverage

### Relation Tests (`test_relations.py`)

Tests validate `in_direct_sequence` constraints:

1. **Two Shipments in Sequence** - Basic 2-shipment relation
2. **Three Shipments in Sequence** - 3+ shipment chain
3. **Unassigned Relation (All-or-Nothing)** - Capacity constraints
4. **Multiple Independent Relations** - Multiple relations on different vehicles
5. **Mixed Jobs and Shipments** - Relations with non-relation jobs
6. **Parameterized Tests** - All fixtures in one test

**Validations**:
- ✓ All shipments in relation on SAME vehicle
- ✓ Shipments in DIRECT SEQUENCE (no jobs in between)
- ✓ All-or-nothing assignment
- ✓ Pickup before delivery for each shipment

### Shipment Atomicity Tests (`test_vehicle_steps.py`)

**CRITICAL NEW TESTS** - Validate that shipments are atomic (pickup → delivery cannot be interrupted):

1. **General Shipment Atomicity** (`TestShipmentAtomicityGeneral`):
   - Validates that NO jobs or shipments are inserted between ANY shipment's pickup and delivery
   - Tests relation fixtures to ensure shipments don't interrupt each other
   - Tests real NEMT data for all shipment violations

2. **Vehicle Steps Interrupting Shipments** (`TestShipmentAtomicity`):
   - Validates that vehicle steps (fixed jobs) don't interrupt shipments
   - Tests various scenarios with vehicle steps and shipments
   - Real NEMT scenario found **18 violations** where vehicle steps interrupt shipments

**Validations**:
- ✓ Nothing between shipment pickup and delivery
- ✓ Vehicle steps don't interrupt shipments
- ✓ Each shipment is atomic (uninterrupted sequence)

### Vehicle Steps Tests (`test_vehicle_steps.py`)

Tests validate vehicle step constraints:

1. **Two Jobs No Interleaving** - Jobs at 8am and 6pm
2. **Steps Assigned to Correct Vehicle** - Multiple vehicles with steps
3. **Steps in Correct Order** - Order preservation
4. **Time Window Constraints** - `forced_service` timing
5. **Shipment Steps** - Pickup/delivery steps
6. **Real NEMT Scenario** - Comprehensive real-world data
7. **Parameterized Tests** - All fixtures in one test

**Validations**:
- ✓ All steps assigned to specified vehicle
- ✓ Steps in correct order
- ✓ NO jobs between consecutive steps
- ✓ Time window constraints respected

## Expected Behavior

### Current State (Before Fix)

Tests are **expected to FAIL** because VROOM currently does not enforce these constraints (see `RELATION_BUG.md`). The tests will show violations like:

```
FAILED test_relations.py::TestRelations::test_two_shipments_in_sequence
✗ 1 violation(s):
  • direct_sequence: Relation 0: Jobs between relation steps (positions 1 and 3)
    between=['job 100'], gap_size=1
```

**Shipment Atomicity Violations** (Real NEMT Data):
```
FAILED test_vehicle_steps.py::TestShipmentAtomicityGeneral::test_real_nemt_all_shipments_atomic
✗ 18 violation(s):
  • shipment_atomicity: Vehicle 1: Shipment 216 interrupted - 2 step(s) between pickup and delivery
    interrupting_steps=['job 10000', 'job 10001']
  • shipment_atomicity: Vehicle 1: Shipment 197 interrupted - 4 step(s) between pickup and delivery
    interrupting_steps=['job 10002', 'job 10003', 'job 10004', 'job 10005']
  ...
```

### After Fix

Once VROOM is fixed to enforce constraints, tests should **PASS**:

```
test_relations.py::TestRelations::test_two_shipments_in_sequence PASSED
test_relations.py::TestRelations::test_three_shipments_in_sequence PASSED
...
```

## Adding New Fixtures

### Relation Fixtures

Create a new JSON file in `tests/fixtures/relations/`:

```json
{
  "vehicles": [{"id": 1, "start_index": 0, "end_index": 0, "capacity": [10]}],
  "shipments": [
    {
      "pickup": {"id": 100, "location_index": 1, "service": 10},
      "delivery": {"id": 100, "location_index": 2, "service": 10},
      "amount": [1]
    }
  ],
  "relations": [{
    "type": "in_direct_sequence",
    "steps": [{"type": "shipment", "id": 100}]
  }],
  "matrices": {
    "car": {"durations": [[0, 100, 200], [100, 0, 100], [200, 100, 0]]}
  }
}
```

### Vehicle Steps Fixtures

Create a new JSON file in `tests/fixtures/vehicle_steps/`:

```json
{
  "vehicles": [{
    "id": 1,
    "start": [-104.8592804, 39.7170955],
    "end": [-104.8592804, 39.7170955],
    "capacity": [4],
    "steps": [
      {"job_type": "single", "id": 10000},
      {"job_type": "single", "id": 10001}
    ]
  }],
  "jobs": [
    {"id": 10000, "location": [-104.8592804, 39.7170955], "service": 600},
    {"id": 10001, "location": [-104.8170536, 39.7202127], "service": 600}
  ]
}
```

## Interpreting Test Output

### Passed Test

```
test_relations.py::TestRelations::test_two_shipments_in_sequence PASSED
```

All constraints validated successfully.

### Failed Test

```
FAILED test_relations.py::TestRelations::test_two_shipments_in_sequence

✗ 2 violation(s):
  • same_vehicle: Relation 0: Steps assigned to different vehicles (assignments={25: 1, 26: 2})
  • direct_sequence: Relation 0: Jobs between relation steps (positions 1 and 4) (between=['job 100'], gap_size=2)
```

The test shows:
- Which constraint was violated (`same_vehicle`, `direct_sequence`, etc.)
- Detailed information about the violation
- Actual vs. expected values

## Troubleshooting

### Binary Not Found

```
VroomRunnerError: VROOM binary not found at ../bin/vroom
```

**Solution**: Build the binary first:
```bash
cd src && make clean && make -j4 USE_ROUTING=false
```

### Import Errors

```
ModuleNotFoundError: No module named 'pytest'
```

**Solution**: Install requirements:
```bash
pip install -r tests/requirements.txt
```

### Tests Timeout

If tests hang or timeout, check:
1. Input JSON is valid
2. VROOM binary is executable: `chmod +x bin/vroom`
3. Binary runs manually: `./bin/vroom -i tests/fixtures/relations/2_shipments_valid.json`

### All Tests Fail

If ALL tests fail with the same error, it's likely:
1. Binary not built or not executable
2. Binary path incorrect (use `--vroom-binary` option)
3. JSON fixtures corrupted

## Custom Binary Path

If your VROOM binary is in a different location:

```bash
pytest --vroom-binary=/path/to/vroom -v
```

## CI/CD Integration

Example GitHub Actions workflow:

```yaml
- name: Build VROOM
  run: cd src && make clean && make -j4 USE_ROUTING=false

- name: Install test dependencies
  run: pip install -r tests/requirements.txt

- name: Run E2E tests
  run: pytest tests/e2e/ -v --json-report --json-report-file=report.json

- name: Upload test results
  uses: actions/upload-artifact@v3
  with:
    name: test-results
    path: report.json
```

## Development

### Running a Single Test During Development

```bash
# Run with output
pytest tests/e2e/test_relations.py::TestRelations::test_two_shipments_in_sequence -vv -s

# Re-run only failed tests
pytest --lf -v
```

### Adding New Validators

To add new constraint validators, edit `tests/utils/validators.py`:

```python
class MyValidator:
    def validate_my_constraint(self, input_data: Dict, output_data: Dict) -> ValidationResult:
        result = ValidationResult(passed=True)
        # Add validation logic
        return result
```

## Performance

Tests are designed to run quickly (< 5s per test). The real NEMT scenario test is marked as `slow` and can be skipped:

```bash
pytest -m "not slow"
```

## Further Reading

- **VROOM Documentation**: https://github.com/VROOM-Project/vroom
- **Relation Bug Report**: See `RELATION_BUG.md` in project root
- **VROOM API**: https://github.com/VROOM-Project/vroom/blob/master/docs/API.md
