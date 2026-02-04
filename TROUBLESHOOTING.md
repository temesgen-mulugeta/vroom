# VROOM Troubleshooting Guide

Common issues and how to fix them.

## Table of Contents

- [Error Messages](#error-messages)
- [Data Integrity Issues](#data-integrity-issues)
- [All Jobs Unassigned](#all-jobs-unassigned)
- [Time Window Problems](#time-window-problems)
- [Capacity Issues](#capacity-issues)
- [Fixing Broken Input Files](#fixing-broken-input-files)

---

## Error Messages

### "Job cannot be reached within any time window"

**Symptom:** Error code 4, jobs marked as unassigned

**Causes:**
1. Time window is too tight given travel times
2. Cascading delays from sequenced shipments
3. Missing or null shipment IDs in relations

**Fix:**
```bash
# Check if relations reference existing shipments
jq '.relations[].steps[].id' input.json | sort -u > relation_ids.txt
jq '.shipments[].id' input.json | sort -u > shipment_ids.txt
diff relation_ids.txt shipment_ids.txt

# If shipments have null IDs, you need to add them
```

### "Assertion failed: (a.size() == b.size())"

**Symptom:** Program crashes with assertion in `amount.h`

**Cause:** Capacity dimension mismatch

**Debug:**
```bash
# Check vehicle capacity dimensions
jq '.vehicles[].capacity | length' input.json | sort -u

# Check job/shipment amount dimensions
jq '.jobs[].delivery | select(. != null) | length' input.json | sort -u
jq '.shipments[].amount | select(. != null) | length' input.json | sort -u
```

**Fix:** Ensure all capacity arrays have the same length:
- If vehicles have 4D capacity: `[100, 50, 10, 5]`
- Then all jobs must have 4D amounts: `[20, 10, 0, 0]` or `null`

### "Could not find route near location"

**Symptom:** Error code 3

**Cause:** Invalid coordinates (usually [0, 0])

**Fix:**
```bash
# Find invalid coordinates
jq '.vehicles[].start, .jobs[].location' input.json |
  grep -A1 "\[0" |
  grep -A1 "0\]"

# Replace with valid coordinates
```

### "Infeasible route for vehicle X"

**Symptom:** Error during initial route construction

**Cause:** Vehicle steps define an impossible sequence

**Debug:**
```bash
# Check vehicle steps
jq '.vehicles[] | select(.steps != null) | {id, steps}' input.json
```

**Fix:**
- Verify time windows allow the step sequence
- Ensure fixed jobs have feasible time windows
- Check if capacity is sufficient for the sequence

---

## Data Integrity Issues

### Missing Shipment IDs

**Problem:** Relations reference shipment IDs, but shipments have `"id": null`

**Detect:**
```bash
# Count unique shipment IDs
jq '[.shipments[].id] | unique | length' input.json

# If result is 1 and IDs are null, you have this problem
```

**Impact:**
- Relations cannot match shipments correctly
- Creates impossible constraints
- Causes cascading time window failures
- Results in "earliest: 177025620100" errors (absurd timestamps)

**Fix Script:**

Save as `fix_shipment_ids.py`:

```python
#!/usr/bin/env python3
import json
import sys

# Read input
with open(sys.argv[1], 'r') as f:
    data = json.load(f)

# Create mapping of relation-referenced IDs
relation_ids = set()
for relation in data.get('relations', []):
    for step in relation.get('steps', []):
        if step.get('type') == 'shipment':
            relation_ids.add(step['id'])

# Assign IDs to shipments
id_counter = 0
for shipment in data.get('shipments', []):
    if shipment.get('id') is None:
        # Use next available ID from relation references
        while id_counter in relation_ids:
            id_counter += 1
        shipment['id'] = id_counter
        id_counter += 1

# Write output
with open('fixed_' + sys.argv[1], 'w') as f:
    json.dump(data, f, indent=2)

print(f"Fixed shipment IDs written to fixed_{sys.argv[1]}")
```

**Usage:**
```bash
python3 fix_shipment_ids.py input.json
./bin/vroom -i fixed_input.json
```

### Capacity Dimension Mismatch

**Problem:** Vehicles have multi-dimensional capacity, but jobs/shipments have null or mismatched dimensions

**Detect:**
```bash
jq '{
  vehicle_dims: [.vehicles[].capacity | length] | unique,
  job_dims: [.jobs[].delivery | select(. != null) | length] | unique,
  shipment_dims: [.shipments[].amount | select(. != null) | length] | unique
}' input.json
```

**Fix:** Standardize all capacity dimensions:

```python
#!/usr/bin/env python3
import json
import sys

with open(sys.argv[1], 'r') as f:
    data = json.load(f)

# Determine target dimensions from vehicles
target_dims = len(data['vehicles'][0]['capacity']) if data.get('vehicles') else 4
zero_amount = [0] * target_dims

# Fix jobs
for job in data.get('jobs', []):
    if job.get('delivery') is None:
        job['delivery'] = zero_amount.copy()
    if job.get('pickup') is None:
        job['pickup'] = zero_amount.copy()

# Fix shipments
for shipment in data.get('shipments', []):
    if shipment.get('amount') is None:
        shipment['amount'] = zero_amount.copy()

with open('fixed_' + sys.argv[1], 'w') as f:
    json.dump(data, f, indent=2)
```

---

## All Jobs Unassigned

### Diagnostic Steps

**1. Check Time Windows**
```bash
# Compare vehicle and job time windows
jq '{
  vehicle_start: .vehicles[0].time_window[0],
  vehicle_end: .vehicles[0].time_window[1],
  job_earliest: [.jobs[].time_windows[0][0]] | min,
  job_latest: [.jobs[].time_windows[0][1]] | max
}' input.json
```

**Fix:** Ensure job time windows are within vehicle shift.

**2. Check Skills**
```bash
# List required skills vs available skills
jq '{
  required_skills: [.jobs[].skills | select(. != null) | .[]] | unique,
  available_skills: [.vehicles[].skills | select(. != null) | .[]] | unique
}' input.json
```

**Fix:** Ensure vehicles have all required skills.

**3. Check Capacity**
```bash
# Compare vehicle capacity to job demands
jq '{
  vehicle_capacity: .vehicles[0].capacity,
  max_job_delivery: [.jobs[] | .delivery | select(. != null)] | max_by(.[0]),
  max_shipment_amount: [.shipments[] | .amount | select(. != null)] | max_by(.[0])
}' input.json
```

**Fix:** Ensure vehicle capacity ≥ largest job demand.

**4. Check Travel Times**
```bash
# Calculate straight-line distances
jq -r '.jobs[] | [.location[0], .location[1]] | @csv' input.json |
  head -10
```

**Fix:** Verify locations are realistic and time windows allow travel.

---

## Time Window Problems

### Infeasible Sequences

**Problem:** Relations require shipments in sequence, but time windows don't allow it

**Example:**
```json
{
  "shipments": [
    {
      "id": 1,
      "pickup": {
        "time_windows": [[1770220800, 1770221000]]  // 9:00-9:03 AM
      },
      "delivery": {...}
    },
    {
      "id": 2,
      "pickup": {
        "time_windows": [[1770221200, 1770221400]]  // 9:06-9:10 AM
      },
      "delivery": {...}
    }
  ],
  "relations": [
    {
      "type": "in_direct_sequence",
      "steps": [
        {"type": "shipment", "id": 1},
        {"type": "shipment", "id": 2}
      ]
    }
  ]
}
```

**Analysis:**
- Shipment 1 pickup: 9:00-9:03 AM
- Shipment 1 service: assume 2 min
- Shipment 1 delivery: assume 20 min travel + 2 min service
- Earliest arrival at shipment 2 pickup: 9:00 + 2 + 20 + 2 = 9:24 AM
- But shipment 2 window closes at 9:10 AM → **INFEASIBLE**

**Fix:**
```json
{
  "id": 2,
  "pickup": {
    "time_windows": [[1770222600, 1770223000]]  // 9:30-9:36 AM
  }
}
```

**Formula:**
```
Next_Pickup_Start ≥ Prev_Pickup_End + Prev_Service + Travel_Time + Prev_Delivery_Service + Delivery_Travel
```

### Buffer Model Misunderstanding

**Common Mistake:**
```json
{
  "pickup": {
    "time_windows": [[1770220800, 1770221000]],  // 9:00-9:03 AM
    "service": 180  // 3 minutes
  }
}
```

**What users expect:** Vehicle arrives at 9:00, waits until 9:03, then serves for 3 min

**What VROOM does:** Vehicle can arrive 9:00-9:03, then serves for 3 min

**Correct approach for "must arrive by 2:00 PM, then serve 3 min":**
```json
{
  "pickup": {
    "time_windows": [[1770217200, 1770225000]],  // 1:00-2:00 PM arrival window
    "service": 180
  }
}
```

---

## Capacity Issues

### Overloaded Vehicle

**Problem:** Sum of pickups exceeds vehicle capacity

**Detect:**
```bash
jq '
  .jobs |
  map(select(.pickup != null) | .pickup[0]) |
  add
' input.json
```

**Fix:**
- Increase vehicle capacity
- Use multiple vehicles
- Reduce job amounts

### Break with Max Load Not Achievable

**Problem:** Break requires low load, but route never gets that empty

**Example:**
```json
{
  "breaks": [
    {
      "time_windows": [[1770230000, 1770235000]],
      "service": 1800,
      "max_load": [10, 0, 0, 0]  // Can only break with ≤10 units
    }
  ]
}
```

**Fix:**
- Schedule break after deliveries
- Relax `max_load` constraint
- Remove `max_load` if not critical

---

## Fixing Broken Input Files

### Complete Validation Script

Save as `validate_vroom_input.py`:

```python
#!/usr/bin/env python3
import json
import sys
from collections import defaultdict

def validate_input(filename):
    with open(filename, 'r') as f:
        data = json.load(f)

    issues = []

    # Check shipment IDs
    shipment_ids = set()
    for i, shipment in enumerate(data.get('shipments', [])):
        sid = shipment.get('id')
        if sid is None:
            issues.append(f"Shipment index {i} has null ID")
        else:
            if sid in shipment_ids:
                issues.append(f"Duplicate shipment ID: {sid}")
            shipment_ids.add(sid)

    # Check relations reference valid shipments
    for i, relation in enumerate(data.get('relations', [])):
        for step in relation.get('steps', []):
            if step.get('type') == 'shipment':
                sid = step.get('id')
                if sid not in shipment_ids:
                    issues.append(f"Relation {i} references non-existent shipment ID: {sid}")

    # Check capacity dimensions
    vehicle_dims = set()
    for vehicle in data.get('vehicles', []):
        if vehicle.get('capacity'):
            vehicle_dims.add(len(vehicle['capacity']))

    job_dims = set()
    for job in data.get('jobs', []):
        if job.get('delivery'):
            job_dims.add(len(job['delivery']))
        if job.get('pickup'):
            job_dims.add(len(job['pickup']))

    shipment_dims = set()
    for shipment in data.get('shipments', []):
        if shipment.get('amount'):
            shipment_dims.add(len(shipment['amount']))

    all_dims = vehicle_dims | job_dims | shipment_dims
    if len(all_dims) > 1:
        issues.append(f"Inconsistent capacity dimensions: vehicles={vehicle_dims}, jobs={job_dims}, shipments={shipment_dims}")

    # Check time windows
    for vehicle in data.get('vehicles', []):
        if vehicle.get('time_window'):
            v_start, v_end = vehicle['time_window']
            if v_start >= v_end:
                issues.append(f"Vehicle {vehicle['id']} has invalid time window: [{v_start}, {v_end}]")

    # Check locations are not [0, 0]
    for i, vehicle in enumerate(data.get('vehicles', [])):
        if vehicle.get('start') == [0, 0]:
            issues.append(f"Vehicle {i} has invalid start location [0, 0]")

    for i, job in enumerate(data.get('jobs', [])):
        if job.get('location') == [0, 0]:
            issues.append(f"Job {i} has invalid location [0, 0]")

    # Print results
    if issues:
        print("❌ VALIDATION FAILED")
        print(f"\nFound {len(issues)} issue(s):\n")
        for issue in issues:
            print(f"  - {issue}")
        return False
    else:
        print("✅ VALIDATION PASSED")
        print("\nInput file looks good!")
        return True

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python3 validate_vroom_input.py input.json")
        sys.exit(1)

    success = validate_input(sys.argv[1])
    sys.exit(0 if success else 1)
```

**Usage:**
```bash
python3 validate_vroom_input.py input.json
```

### Quick Fixes

**Remove all relations (if broken):**
```bash
jq 'del(.relations)' input.json > no_relations.json
```

**Set all shipment IDs by index:**
```bash
jq '.shipments |= [to_entries[] | .value + {id: .key}]' input.json > fixed.json
```

**Remove capacity constraints:**
```bash
jq '
  .vehicles |= map(del(.capacity)) |
  .jobs |= map(del(.delivery, .pickup)) |
  .shipments |= map(del(.amount))
' input.json > no_capacity.json
```

**Widen all time windows by 1 hour:**
```bash
jq '
  .jobs |= map(
    if .time_windows then
      .time_windows |= map([.[0] - 1800, .[1] + 1800])
    else . end
  )
' input.json > wider_windows.json
```

---

## Performance Tips

### Large Input Files

For 1000+ jobs:

1. **Use proper indexes** (VROOM does this automatically)
2. **Limit time windows** (tighter = faster)
3. **Reduce number of relations** (relations are expensive)
4. **Use skills effectively** (reduces search space)
5. **Pre-filter infeasible jobs** (validate before running)

### Slow Optimization

If optimization takes > 5 minutes:

```bash
# Run with time limit (e.g., 30 seconds)
./bin/vroom -i input.json -t 30
```

**Check:**
- Number of vehicles × number of jobs
- Number of relations
- Complexity of time windows
- Number of shipments in relations

---

## Getting Help

### Debug Output

```bash
# Run with verbose output
./bin/vroom -i input.json 2>&1 | tee debug.log
```

### Minimal Reproduction

Create minimal test case:

```bash
# Extract just first vehicle and first 5 jobs
jq '{
  vehicles: [.vehicles[0]],
  jobs: .jobs[0:5]
}' input.json > minimal.json

./bin/vroom -i minimal.json
```

### Check VROOM Logs

```bash
# Look for warnings/errors
./bin/vroom -i input.json 2>&1 | grep -i "error\|warning\|fail"
```

---

## Common Fixes Summary

| Problem | Quick Fix |
|---------|-----------|
| Null shipment IDs | Run `fix_shipment_ids.py` |
| Dimension mismatch | Standardize all capacity arrays |
| All unassigned | Widen vehicle time_window |
| Invalid coordinates | Replace [0,0] with real locations |
| Infeasible relations | Check time gaps between shipments |
| Break never taken | Remove or relax max_load |
| Slow optimization | Add time limit with `-t` flag |

---

## Prevention Checklist

Before running VROOM:

- [ ] Run validation script
- [ ] Check all IDs are unique and not null
- [ ] Verify capacity dimensions match
- [ ] Ensure time windows are feasible
- [ ] Test with minimal example first
- [ ] Validate coordinates are realistic
- [ ] Check relation shipment IDs exist
- [ ] Verify skills are consistent

---

For more information, see [FEATURES_GUIDE.md](FEATURES_GUIDE.md) and [TEMPLATES.md](TEMPLATES.md).
