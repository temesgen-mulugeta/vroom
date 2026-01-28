# VROOM Relation Constraint Bug

## Problem Summary
Relations are **parsed and tracked** but **NOT ENFORCED** in the solver.

## What Works ✅
1. **JSON Parsing** (`input_parser.cpp:686-717`)
   - Correctly parses `in_direct_sequence` relations
   - Validates JSON structure
   - Creates `Relation` objects

2. **Storage** (`input.cpp:255-284`)
   - Stores relations in `input.relations`
   - Maps job ranks to relations
   - Builds lookup structures

3. **Constraint Tracking** (`solution_state.cpp:544-597`)
   - Sets up `relation_next_job[v]` and `relation_prev_job[v]`
   - Tracks which job must come after which

## What's Broken ❌
**NO ENFORCEMENT**: The constraints are never checked!

### Missing Checks
1. **Initial Solution Building**: No code ensures relations are respected when first assigning jobs to vehicles
2. **Local Search Operators**: No validation that moves preserve relation constraints
3. **Route Modifications**: No checks when swapping/moving jobs

### Evidence
```bash
$ grep -rn "relation_next_job\|relation_prev_job" vroom/src --include="*.cpp"
# Only shows:
# - Declaration (solution_state.cpp:37-38)
# - Setting values (solution_state.cpp:544-597)
# - NO USAGE/CHECKS!
```

## How to Fix

### Option 1: Enforce in Initial Solution (Easiest)
**File**: `src/algorithms/heuristics/heuristics.cpp`

When building initial routes, group relation shipments and assign them together:
```cpp
// Pseudo-code
for (const auto& relation : input.relations) {
  // Find best vehicle for ALL steps in relation
  Index best_vehicle = find_best_vehicle_for_relation(relation);

  // Add all shipments to that vehicle in sequence
  for (const auto& step : relation.steps) {
    assign_to_vehicle(step.id, best_vehicle);
  }
}
```

### Option 2: Enforce in Operators (Complete)
**Files**: `src/problems/cvrp/operators/*.cpp`

Add validation in each operator (pd_shift, route_exchange, etc.):
```cpp
bool is_valid_move() {
  // Check if move violates relation constraints
  if (state.relation_next_job[v][pos].has_value()) {
    Index required_next = state.relation_next_job[v][pos].value();
    if (new_route[pos + 1] != required_next) {
      return false;  // Violates relation!
    }
  }
  return true;
}
```

### Option 3: Simple Pre-Assignment (Quick Fix)
**File**: `src/structures/vroom/input/input.cpp`

Before optimization starts, pre-assign relation shipments to vehicles:
```cpp
void Input::add_relation(Relation&& relation) {
  // Existing validation code...

  // NEW: Mark these shipments as requiring same vehicle
  for (size_t i = 0; i < relation.pickup_ranks.size(); ++i) {
    Index pickup = relation.pickup_ranks[i];
    Index delivery = relation.delivery_ranks[i];

    // Force these on same vehicle (use unique skill)
    jobs[pickup].required_skills.insert(relation_skill_id);
    jobs[delivery].required_skills.insert(relation_skill_id);
  }

  // Assign skill to all vehicles (for now)
  // Later, heuristic will pick best vehicle for the group

  relations.push_back(std::move(relation));
}
```

## Recommended Fix: Option 3 (Skill-Based)
Convert relations to **skills** at load time:
1. Assign unique skill ID to each relation
2. Mark all jobs in relation with that skill
3. Only one vehicle gets that skill
4. Existing skill enforcement handles the constraint

### Why This Works
- Reuses existing skill enforcement code
- No operator changes needed
- Ensures same-vehicle constraint
- Sequence enforced by insertion order

## Test Case
```json
{
  "vehicles": [{"id": 1, "start": [0,0], "end": [0,0], "capacity": [10]}],
  "shipments": [
    {"pickup": {"id": 25, "location": [1,1], "service": 60},
     "delivery": {"id": 25, "location": [2,2], "service": 60},
     "amount": [1]},
    {"pickup": {"id": 26, "location": [3,3], "service": 60},
     "delivery": {"id": 26, "location": [4,4], "service": 60},
     "amount": [1]}
  ],
  "relations": [{
    "type": "in_direct_sequence",
    "steps": [{"type": "shipment", "id": 25}, {"type": "shipment", "id": 26}]
  }]
}
```

**Expected**:
- Both shipments on vehicle 1
- Shipment 25 pickup → delivery 25 → Shipment 26 pickup → delivery 26

**Actual**:
- Relations ignored
- Random order

## Files to Modify
1. `src/structures/vroom/input/input.cpp` - Add enforcement in `add_relation()`
2. `src/algorithms/heuristics/heuristics.cpp` - Respect relations in initial solution
3. OR `src/problems/cvrp/operators/*.cpp` - Add checks to each operator

## Priority: HIGH
Relations are completely non-functional without enforcement!
first