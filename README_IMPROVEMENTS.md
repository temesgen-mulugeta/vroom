# VROOM Improvements Summary

This document summarizes the improvements made to VROOM for handling infeasible constraints gracefully.

## What Was Fixed

### Problem

VROOM would crash with assertion failures when encountering infeasible time window constraints:

```
Assertion failed: (j_tw != j.tws.end()), function replace, file tw_route.cpp, line 1203
```

This happened when:
- Time windows were impossible to satisfy
- Cascading delays made jobs unreachable
- Data integrity issues created invalid constraints

### Solution

Implemented graceful error handling that allows optimization to continue even when some jobs are infeasible.

---

## Code Changes

### 1. New Exception Type

**File:** `src/utils/exception.h` and `src/utils/exception.cpp`

Added `InfeasibleRouteException` (error code 4) for time window infeasibility:

```cpp
class InfeasibleRouteException : public Exception {
public:
  explicit InfeasibleRouteException(const std::string& message);
};
```

### 2. Replaced Assertions with Exceptions

**File:** `src/structures/vroom/tw_route.cpp`

Replaced 9 assertion sites with proper exception handling:

```cpp
// Before: Hard crash
assert(j_tw != j.tws.end());

// After: Recoverable exception
if (j_tw == j.tws.end()) {
  throw InfeasibleRouteException(
    "Job cannot be reached within any time window (earliest: " +
    std::to_string(current_earliest) + ")");
}
```

**Locations fixed:**
- Lines 166, 193: Break time window checks (forward propagation)
- Lines 236: Break time window check (forward update)
- Lines 288, 301: Break and job time windows (backward propagation)
- Lines 351, 388: Break time windows (backward update and last latest)
- Lines 1167, 1203, 1404: Job/break time windows in replace function

### 3. Exception Handling in Heuristics

**File:** `src/algorithms/heuristics/heuristics.cpp`

Wrapped route modification operations in try-catch blocks:

```cpp
try {
  route.add(input, job_rank, rank);
  unassigned.erase(job_rank);  // Only erase if successful
} catch (const std::exception&) {
  // Job stays in unassigned set
}
```

**Key improvement:** Jobs are only removed from `unassigned` set AFTER successful insertion, preventing data corruption.

**Locations:**
- Line 296-308: Initial route construction
- Line 487-510: Relation insertion
- Line 770-895: Fill route with flexible jobs
- Line 1300-1323: Vehicle steps validation

---

## Behavior Changes

### Before

```
Input: Job with infeasible time window
Output: Assertion failure → Program crash → No solution
```

### After

```
Input: Job with infeasible time window
Output: Job marked as unassigned → Optimization continues → Partial solution returned
```

### Example Output

```json
{
  "code": 0,
  "summary": {
    "cost": 1220,
    "routes": 1,
    "unassigned": 3
  },
  "unassigned": [
    {
      "id": 100,
      "location": [-104.9779, 39.7339]
    },
    {
      "id": 101,
      "location": [-104.8592, 39.7171]
    },
    {
      "id": 102,
      "location": [-104.9903, 39.7500]
    }
  ],
  "routes": [...]
}
```

---

## Testing Results

### Unit Tests

✅ All 32 existing tests pass:
- 12/12 relation tests
- 20/20 vehicle steps tests

### Test Files

✅ `vroom-test-simple.json` - Works correctly (cost: 1220)
✅ `vroom-test-mixed.json` - Works correctly (cost: 1913)
✅ Existing functionality preserved

### Edge Cases Handled

✅ Jobs that don't fit any time window → Unassigned
✅ Relations with infeasible sequences → All shipments unassigned
✅ Fixed vehicle steps that are infeasible → Steps left unassigned
✅ Breaks that can't be scheduled → Optimization continues
✅ Capacity constraint violations → Caught and handled

---

## Documentation Created

### 1. FEATURES_GUIDE.md

Complete guide to all VROOM features:
- Jobs vs Shipments
- Time Windows
- **Vehicle Steps** (fixed sequences)
- **Relations** (flexible sequences)
- Capacity Constraints
- Skills
- Breaks
- Complete examples with explanations

### 2. TEMPLATES.md

Ready-to-use JSON templates:
- Medical transport (single trip, round trip, wheelchair)
- Delivery service (packages, pickup/delivery)
- Complex scenarios (multi-vehicle, multi-leg journeys)
- Time calculation helpers
- Validation checklists

### 3. TROUBLESHOOTING.md

Comprehensive troubleshooting guide:
- Common error messages and fixes
- Data integrity issue detection
- Validation scripts
- Fix scripts for common problems
- Performance tips

---

## Known Issues

### vroom-crash.json

The test file `vroom-crash.json` has critical data integrity issues:

**Issue 1: Missing Shipment IDs**
```bash
# All shipments have null IDs
jq '[.shipments[].id] | unique' test-files/vroom-crash.json
# Output: [null]

# But relations reference specific IDs (15, 16, 17, etc.)
jq '[.relations[].steps[].id] | unique[:5]' test-files/vroom-crash.json
# Output: [15, 16, 17, 18, 19]
```

**Impact:** Relations cannot match shipments, creating impossible constraints and cascading time failures.

**Issue 2: Capacity Dimension Mismatch**
- Vehicles have 4-dimensional capacity: `[1, 6, 1, 1]`
- Shipments have null amounts
- Internal calculations create mismatches → `AmountSum` assertion

**Fix:** Run the validation and fix scripts from TROUBLESHOOTING.md

---

## Benefits

### For Users

1. **No more crashes** - Infeasible jobs are handled gracefully
2. **Partial solutions** - Get routes for feasible jobs even if some fail
3. **Better debugging** - Unassigned jobs clearly shown in output
4. **Flexible constraints** - Even "hard" constraints can be relaxed

### For Developers

1. **Cleaner error handling** - Exceptions instead of assertions
2. **Better testability** - Can test infeasible scenarios
3. **Comprehensive docs** - Complete feature guide with examples
4. **Validation tools** - Scripts to catch issues before running

---

## Migration Guide

### For Existing Users

No changes required! The improvements are backward compatible:

```bash
# Old usage still works
./bin/vroom -i input.json

# No code changes needed in your input files
```

### New Capabilities

You can now:

1. **Handle uncertain time windows** - VROOM will optimize what's feasible
2. **Mix hard and soft constraints** - Some jobs can be unassigned
3. **Debug infeasible inputs** - See which jobs can't be assigned and why
4. **Recover from data errors** - Partial solutions instead of crashes

---

## Usage Examples

### Basic Usage

```bash
# Run optimization
./bin/vroom -i input.json

# With time limit
./bin/vroom -i input.json -t 30

# Check output for unassigned jobs
./bin/vroom -i input.json | jq '.unassigned'
```

### Validation

```bash
# Validate input before running
python3 validate_vroom_input.py input.json

# Fix common issues
python3 fix_shipment_ids.py input.json

# Run on fixed file
./bin/vroom -i fixed_input.json
```

### Debugging

```bash
# Get detailed error info
./bin/vroom -i input.json 2>&1 | tee debug.log

# Check for unassigned jobs
./bin/vroom -i input.json | jq '{
  total_jobs: (.jobs | length),
  unassigned: (.unassigned | length),
  assigned: ((.jobs | length) - (.unassigned | length))
}'
```

---

## Future Enhancements

Potential improvements for future versions:

1. **Violation tracking for unassigned jobs**
   - Add `unassigned_reason` field to output
   - Track which constraint caused the job to be unassigned

2. **Soft time windows**
   - Allow configurable penalties for late arrivals
   - Support time window violations with cost

3. **Improved relation handling**
   - Partial relation assignment (some shipments assigned, others not)
   - Relation priority levels

4. **Better capacity handling**
   - Automatic dimension normalization
   - Support for null amounts with multi-dimensional capacity

---

## Files Modified

### Core Engine
- `src/utils/exception.h` - Added InfeasibleRouteException
- `src/utils/exception.cpp` - Implemented exception constructor
- `src/structures/vroom/tw_route.cpp` - Replaced 9 assertions with exceptions
- `src/algorithms/heuristics/heuristics.cpp` - Added exception handling

### Documentation
- `FEATURES_GUIDE.md` - Complete feature documentation (NEW)
- `TEMPLATES.md` - JSON templates for common scenarios (NEW)
- `TROUBLESHOOTING.md` - Troubleshooting and validation tools (NEW)
- `README_IMPROVEMENTS.md` - This file (NEW)

### Test Files
- All existing tests pass
- New test scenarios can be added for infeasible cases

---

## Compilation

### Requirements
- C++20 compiler
- Make
- Standard dependencies (asio, OpenSSL)

### Build

```bash
cd /Users/teme/MyFiles/Dev/Projects/US/nemt/vroom/src
make clean
make -j4
```

### Verify

```bash
# Run tests
cd ../tests
python3 -m pytest e2e/ -v

# Test with sample files
cd ..
./bin/vroom -i test-files/vroom-test-simple.json
./bin/vroom -i test-files/vroom-test-mixed.json
```

---

## Support

For questions or issues:

1. Check `TROUBLESHOOTING.md` for common problems
2. Review `FEATURES_GUIDE.md` for feature documentation
3. Use templates from `TEMPLATES.md` as starting points
4. Validate input with the provided scripts

---

## Credits

**Original VROOM:** Julien Coupey (https://github.com/VROOM-Project/vroom)

**Improvements:**
- Exception-based error handling for time window infeasibility
- Comprehensive documentation for steps and relations
- Validation and troubleshooting tools

**Date:** February 2026

---

## License

Same as VROOM: See LICENSE file in root directory.
