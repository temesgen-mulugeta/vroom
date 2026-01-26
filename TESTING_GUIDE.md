# VROOM Binary Testing Guide

## Quick Test Commands

### Build Binary
```bash
cd /Users/teme/MyFiles/Dev/Projects/US/nemt/vroom/src
make clean && make -j4 USE_ROUTING=false
```

Binary location: `../bin/vroom`

### Test Files

**test-relations-matrix.json** - Basic test that works
```bash
./bin/vroom -i test-relations-matrix.json | python3 -m json.tool
```

**test-relations-violation.json** - Proves enforcement bug
```bash
./bin/vroom -i test-relations-violation.json
```

### Verify Relation Enforcement

Run this command to see route order:
```bash
./bin/vroom -i test-relations-violation.json 2>&1 | python3 -c "
import json, sys
data = json.load(sys.stdin)
print('Route order:', ' -> '.join([f\"{s['type']} {s.get('id', '')}\" for s in data['routes'][0]['steps']]))
print(f\"Total cost: {data['summary']['cost']}\")
"
```

**Expected when enforced**: `pickup 25 -> delivery 25 -> pickup 26 -> delivery 26`
**Actual (bug)**: `pickup 25 -> pickup 26 -> delivery 25 -> delivery 26`

## Enforcement Fix Locations

Based on RELATION_BUG.md, you need to add constraint checks in:

### Option 1: Initial Solution (Easiest)
**File**: `src/algorithms/heuristics/heuristics.cpp`

Group relation shipments and assign together to same vehicle.

### Option 2: Operators (Complete)
**Files**: `src/problems/cvrp/operators/*.cpp`

Add validation in each operator:
```cpp
bool is_valid_move() {
  if (state.relation_next_job[v][pos].has_value()) {
    Index required_next = state.relation_next_job[v][pos].value();
    if (new_route[pos + 1] != required_next) {
      return false;  // Violates relation!
    }
  }
  return true;
}
```

### Option 3: Skills Conversion (Quick Workaround)
**File**: `src/structures/vroom/input/input.cpp`

Convert relations to unique skills at load time (reuses existing skill enforcement).

## After Fixing

Retest with:
```bash
cd src && make -j4 USE_ROUTING=false
./bin/vroom -i ../test-relations-violation.json
```

Route should change to: `pickup 25 -> delivery 25 -> pickup 26 -> delivery 26`
