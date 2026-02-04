# VROOM Features Guide

Complete guide to using VROOM for vehicle routing optimization with time windows (VRPTW).

## Table of Contents

- [Basic Concepts](#basic-concepts)
- [Jobs vs Shipments](#jobs-vs-shipments)
- [Time Windows](#time-windows)
- [Vehicle Steps](#vehicle-steps)
- [Relations](#relations)
- [Capacity Constraints](#capacity-constraints)
- [Skills](#skills)
- [Breaks](#breaks)
- [Complete Examples](#complete-examples)

---

## Basic Concepts

### Vehicles

Vehicles are the resources that perform deliveries/pickups. Each vehicle has:

```json
{
  "vehicles": [
    {
      "id": 1,
      "profile": "car",
      "start": [-104.9903, 39.7392],
      "end": [-104.9903, 39.7392],
      "capacity": [10, 5],
      "skills": [1, 2],
      "time_window": [1770202800, 1770267600],
      "breaks": [...]
    }
  ]
}
```

**Fields:**
- `id` (required): Unique identifier
- `profile`: Routing profile (car, bike, foot)
- `start`: Starting location [longitude, latitude]
- `end`: Ending location (if different from start)
- `capacity`: Array of capacity dimensions (e.g., [weight, volume])
- `skills`: Array of skill IDs this vehicle has
- `time_window`: [start_time, end_time] in Unix seconds
- `breaks`: Optional break requirements

---

## Jobs vs Shipments

### Jobs (Single Stop)

Use jobs for single-location tasks (delivery OR pickup, not both):

```json
{
  "jobs": [
    {
      "id": 1,
      "location": [-104.9903, 39.7392],
      "service": 300,
      "delivery": [2, 1],
      "time_windows": [[1770210000, 1770215000]],
      "skills": [1],
      "priority": 100
    }
  ]
}
```

**Fields:**
- `id` (required): Unique identifier
- `location` (required): [longitude, latitude]
- `service`: Service time in seconds (default: 0)
- `setup`: Setup time in seconds (default: 0)
- `delivery`: Amount being delivered (decreases vehicle load)
- `pickup`: Amount being picked up (increases vehicle load)
- `time_windows`: Array of acceptable time windows
- `skills`: Required skills (vehicle must have ALL listed skills)
- `priority`: Higher priority jobs preferred (0-100, default: 0)
- `description`: Optional description

### Shipments (Pickup + Delivery)

Use shipments for tasks requiring both pickup and delivery:

```json
{
  "shipments": [
    {
      "id": 10,
      "amount": [5, 2],
      "skills": [1],
      "priority": 50,
      "pickup": {
        "id": 100,
        "location": [-104.9903, 39.7392],
        "service": 180,
        "time_windows": [[1770210000, 1770212000]]
      },
      "delivery": {
        "id": 101,
        "location": [-104.8592, 39.7171],
        "service": 180,
        "time_windows": [[1770215000, 1770220000]]
      }
    }
  ]
}
```

**Fields:**
- `id` (required): Unique identifier for the shipment
- `amount`: Capacity used (e.g., [weight, volume])
- `skills`: Required skills
- `priority`: Higher priority preferred
- `pickup`: Pickup details (location, service time, time windows)
- `delivery`: Delivery details (location, service time, time windows)

**Important Notes:**
- Pickup must occur before delivery on the same vehicle
- The shipment `id` is used for relations (not the pickup/delivery sub-IDs)
- Both pickup and delivery IDs should be unique across all jobs

---

## Time Windows

Time windows specify when a job/shipment can be serviced.

### Single Time Window

```json
{
  "id": 1,
  "location": [-104.9903, 39.7392],
  "time_windows": [[1770210000, 1770215000]]
}
```

- Vehicle must arrive between 1770210000 and 1770215000 (Unix seconds)
- Can arrive early and wait
- Cannot arrive late (hard constraint)

### Multiple Time Windows

```json
{
  "id": 2,
  "location": [-104.9903, 39.7392],
  "time_windows": [
    [1770210000, 1770212000],
    [1770220000, 1770222000]
  ]
}
```

- Vehicle can arrive in EITHER window
- Useful for businesses with split shifts or specific appointment slots

### No Time Window (Unrestricted)

```json
{
  "id": 3,
  "location": [-104.9903, 39.7392]
  // No time_windows field = anytime during vehicle shift
}
```

---

## Vehicle Steps

Vehicle steps define **fixed sequences** of jobs that must be performed by a specific vehicle in a specific order.

### Use Cases

- Pre-assigned appointments
- Mandatory facility stops
- Meal delivery routes with fixed stops
- Medical transport with required facilities

### Example: Fixed Job Sequence

```json
{
  "vehicles": [
    {
      "id": 1,
      "start": [-104.9903, 39.7392],
      "end": [-104.9903, 39.7392],
      "time_window": [1770202800, 1770267600],
      "steps": [
        {
          "type": "start"
        },
        {
          "type": "job",
          "id": 100,
          "service": 300
        },
        {
          "type": "job",
          "id": 101,
          "service": 300
        },
        {
          "type": "end"
        }
      ]
    }
  ],
  "jobs": [
    {
      "id": 100,
      "location": [-104.8592, 39.7171],
      "service": 300,
      "description": "Required facility stop"
    },
    {
      "id": 101,
      "location": [-104.9779, 39.7339],
      "service": 300,
      "description": "Required checkpoint"
    },
    {
      "id": 200,
      "location": [-104.9903, 39.7500],
      "service": 180,
      "description": "Can be inserted anywhere"
    }
  ]
}
```

**Behavior:**
- Vehicle 1 MUST visit job 100, then job 101, in that order
- Job 200 can be inserted anywhere between the fixed jobs
- Fixed jobs cannot be reordered or assigned to other vehicles
- Other jobs can be interleaved with fixed jobs

### Example: Shipment Steps

```json
{
  "vehicles": [
    {
      "id": 1,
      "steps": [
        {
          "type": "start"
        },
        {
          "type": "pickup",
          "id": 10
        },
        {
          "type": "delivery",
          "id": 10
        },
        {
          "type": "end"
        }
      ]
    }
  ],
  "shipments": [
    {
      "id": 10,
      "pickup": {
        "location": [-104.9903, 39.7392],
        "service": 180
      },
      "delivery": {
        "location": [-104.8592, 39.7171],
        "service": 180
      }
    }
  ]
}
```

### Step Types

- `start`: Vehicle start location (optional, can be first step)
- `end`: Vehicle end location (optional, can be last step)
- `job`: Single job (references job `id`)
- `pickup`: Shipment pickup (references shipment `id`, not pickup sub-ID)
- `delivery`: Shipment delivery (references shipment `id`, not delivery sub-ID)
- `break`: Vehicle break

### Important Rules

1. **Jobs in steps are assigned exclusively** to that vehicle
2. **Order is fixed** - cannot be changed during optimization
3. **Other jobs can be inserted** between fixed steps
4. **Time windows still apply** - steps may be infeasible if time windows don't allow the sequence

---

## Relations

Relations define **flexible sequences** of shipments that must follow a specific order but can be assigned to any compatible vehicle.

### Use Cases

- Round-trip medical appointments (outbound → appointment → return)
- Multi-leg journeys (home → facility → doctor → home)
- Supply chain sequences (warehouse → distribution → retail)
- Parcel consolidation routes

### Relation Type: `in_direct_sequence`

Shipments must be performed in order with **no other shipments** in between.

```json
{
  "shipments": [
    {
      "id": 15,
      "pickup": {
        "id": 1501,
        "location": [-104.9903, 39.7392],
        "service": 120,
        "time_windows": [[1770220800, 1770220980]]
      },
      "delivery": {
        "id": 1502,
        "location": [-104.8592, 39.7171],
        "service": 120
      }
    },
    {
      "id": 16,
      "pickup": {
        "id": 1601,
        "location": [-104.8592, 39.7171],
        "service": 120,
        "time_windows": [[1770223200, 1770223380]]
      },
      "delivery": {
        "id": 1602,
        "location": [-104.9903, 39.7392],
        "service": 120
      }
    }
  ],
  "relations": [
    {
      "type": "in_direct_sequence",
      "steps": [
        {
          "type": "shipment",
          "id": 15
        },
        {
          "type": "shipment",
          "id": 16
        }
      ]
    }
  ]
}
```

**Resulting Sequence:**
```
Pickup(15) → Delivery(15) → Pickup(16) → Delivery(16)
```

**Behavior:**
- No other shipments can be inserted between shipment 15 and 16
- Regular jobs (non-shipments) CAN be inserted
- All shipments in relation must be on same vehicle
- If any shipment is infeasible, entire relation may be unassigned

### Multiple Shipments in Sequence

```json
{
  "relations": [
    {
      "type": "in_direct_sequence",
      "steps": [
        {
          "type": "shipment",
          "id": 20
        },
        {
          "type": "shipment",
          "id": 21
        },
        {
          "type": "shipment",
          "id": 22
        }
      ]
    }
  ]
}
```

**Resulting Sequence:**
```
P(20) → D(20) → P(21) → D(21) → P(22) → D(22)
```

### Multiple Independent Relations

```json
{
  "relations": [
    {
      "type": "in_direct_sequence",
      "steps": [
        {"type": "shipment", "id": 15},
        {"type": "shipment", "id": 16}
      ]
    },
    {
      "type": "in_direct_sequence",
      "steps": [
        {"type": "shipment", "id": 30},
        {"type": "shipment", "id": 31}
      ]
    }
  ]
}
```

**Behavior:**
- Relation 1 (shipments 15-16) is independent from Relation 2 (shipments 30-31)
- They can be on different vehicles
- Each relation maintains its internal sequence

### Important Rules

1. **Shipment IDs must match exactly** - all shipments referenced in relations must exist
2. **All-or-nothing** - if one shipment in a relation is infeasible, entire relation may be unassigned
3. **Same vehicle requirement** - all shipments in a relation use the same vehicle
4. **Atomicity** - pickup and delivery of each shipment are atomic (cannot be separated)

---

## Capacity Constraints

### Multi-Dimensional Capacity

Vehicles and jobs can have multiple capacity dimensions:

```json
{
  "vehicles": [
    {
      "id": 1,
      "capacity": [100, 50, 10]
      // [weight_kg, volume_cubic_feet, passenger_count]
    }
  ],
  "jobs": [
    {
      "id": 1,
      "delivery": [20, 5, 0],
      "pickup": [0, 0, 1]
    }
  ]
}
```

**Rules:**
- All dimensions must be satisfied simultaneously
- Capacity is tracked throughout the route
- Dimensions must match (all vehicles/jobs use same number of dimensions)

### Capacity Tracking

```
Start: [0, 0, 0]
After pickup job 1: [0, 0, 1]
After delivery job 2: [20, 5, 1]
After delivery job 3: [0, 0, 1]
```

### Breaks with Capacity

Breaks can have maximum load restrictions:

```json
{
  "breaks": [
    {
      "id": 1,
      "time_windows": [[1770230000, 1770233000]],
      "service": 1800,
      "max_load": [50, 25, 0]
    }
  ]
}
```

- Break can only occur when vehicle load ≤ max_load
- Useful for mandatory meal breaks (must be empty or nearly empty)

---

## Skills

Skills ensure jobs are only assigned to qualified vehicles.

```json
{
  "vehicles": [
    {
      "id": 1,
      "skills": [1, 2, 5]
    },
    {
      "id": 2,
      "skills": [1, 3]
    }
  ],
  "jobs": [
    {
      "id": 1,
      "skills": [1, 2]
      // Requires skills 1 AND 2 → only vehicle 1 qualifies
    },
    {
      "id": 2,
      "skills": [1]
      // Requires only skill 1 → both vehicles qualify
    },
    {
      "id": 3,
      "skills": [3, 4]
      // Requires skills 3 AND 4 → NO vehicle qualifies → unassigned
    }
  ]
}
```

**Rules:**
- Job requires ALL listed skills
- Vehicle must have ALL skills the job requires
- Vehicle can have extra skills
- If no vehicle has required skills, job remains unassigned

---

## Breaks

Breaks are mandatory rest periods for vehicles.

### Simple Break

```json
{
  "vehicles": [
    {
      "id": 1,
      "breaks": [
        {
          "id": 1,
          "time_windows": [[1770230000, 1770233000]],
          "service": 1800
        }
      ]
    }
  ]
}
```

- Break must start within time window [1770230000, 1770233000]
- Break duration is 1800 seconds (30 minutes)

### Multiple Time Windows for Breaks

```json
{
  "breaks": [
    {
      "id": 1,
      "time_windows": [
        [1770230000, 1770232000],
        [1770240000, 1770242000]
      ],
      "service": 1800
    }
  ]
}
```

- Break can start in either window
- Optimizer chooses best window

### Break with Max Load

```json
{
  "breaks": [
    {
      "id": 1,
      "time_windows": [[1770230000, 1770233000]],
      "service": 1800,
      "max_load": [10, 5, 0]
    }
  ]
}
```

- Break can only occur when vehicle load ≤ [10, 5, 0]
- Forces deliveries before break

---

## Complete Examples

### Example 1: Basic Medical Transport

Patient pickup → clinic → patient dropoff:

```json
{
  "vehicles": [
    {
      "id": 1,
      "profile": "car",
      "start": [-104.9903, 39.7392],
      "end": [-104.9903, 39.7392],
      "capacity": [3, 1, 0, 0],
      "time_window": [1770202800, 1770267600]
    }
  ],
  "shipments": [
    {
      "id": 100,
      "amount": [1, 0, 0, 0],
      "description": "Patient to clinic",
      "pickup": {
        "id": 1001,
        "location": [-104.9779, 39.7339],
        "service": 180,
        "time_windows": [[1770210000, 1770210180]],
        "description": "Patient home"
      },
      "delivery": {
        "id": 1002,
        "location": [-104.8592, 39.7171],
        "service": 180,
        "description": "Clinic"
      }
    }
  ]
}
```

### Example 2: Round Trip with Relations

Outbound trip + return trip in sequence:

```json
{
  "vehicles": [
    {
      "id": 1,
      "start": [-104.9903, 39.7392],
      "capacity": [6, 1, 0, 0],
      "time_window": [1770202800, 1770267600]
    }
  ],
  "shipments": [
    {
      "id": 15,
      "amount": [1, 0, 0, 0],
      "description": "Outbound: Home to Clinic",
      "pickup": {
        "id": 1501,
        "location": [-104.9779, 39.7339],
        "service": 120,
        "time_windows": [[1770220800, 1770220980]],
        "description": "Patient home"
      },
      "delivery": {
        "id": 1502,
        "location": [-104.8592, 39.7171],
        "service": 120,
        "description": "Clinic"
      }
    },
    {
      "id": 16,
      "amount": [1, 0, 0, 0],
      "description": "Return: Clinic to Home",
      "pickup": {
        "id": 1601,
        "location": [-104.8592, 39.7171],
        "service": 120,
        "time_windows": [[1770223200, 1770223380]],
        "description": "Clinic (after appointment)"
      },
      "delivery": {
        "id": 1602,
        "location": [-104.9779, 39.7339],
        "service": 120,
        "description": "Patient home"
      }
    }
  ],
  "relations": [
    {
      "type": "in_direct_sequence",
      "steps": [
        {"type": "shipment", "id": 15},
        {"type": "shipment", "id": 16}
      ]
    }
  ]
}
```

**Result:** Driver picks up patient, drives to clinic, waits for appointment, returns patient home.

### Example 3: Fixed Route with Flexible Insertions

```json
{
  "vehicles": [
    {
      "id": 1,
      "start": [-104.9903, 39.7392],
      "capacity": [6, 1, 0, 0],
      "time_window": [1770202800, 1770267600],
      "steps": [
        {"type": "start"},
        {"type": "job", "id": 100},
        {"type": "job", "id": 101},
        {"type": "end"}
      ]
    }
  ],
  "jobs": [
    {
      "id": 100,
      "location": [-104.8592, 39.7171],
      "service": 300,
      "description": "Required facility check-in"
    },
    {
      "id": 101,
      "location": [-104.9779, 39.7339],
      "service": 300,
      "description": "Required facility check-out"
    },
    {
      "id": 200,
      "location": [-104.9903, 39.7500],
      "service": 180,
      "delivery": [1, 0, 0, 0],
      "description": "Flexible delivery (can insert anywhere)"
    },
    {
      "id": 201,
      "location": [-104.9700, 39.7600],
      "service": 180,
      "delivery": [1, 0, 0, 0],
      "description": "Another flexible delivery"
    }
  ]
}
```

**Result:** Jobs 200 and 201 will be inserted optimally between/around the fixed jobs 100 and 101.

### Example 4: Skills and Capacity

Wheelchair-accessible vehicle required:

```json
{
  "vehicles": [
    {
      "id": 1,
      "capacity": [3, 1, 0, 0],
      "skills": [1],
      "description": "Ambulatory only"
    },
    {
      "id": 2,
      "capacity": [1, 1, 2, 0],
      "skills": [1, 2],
      "description": "Wheelchair accessible"
    }
  ],
  "jobs": [
    {
      "id": 1,
      "location": [-104.9903, 39.7392],
      "delivery": [0, 0, 1, 0],
      "skills": [1, 2],
      "description": "Wheelchair patient"
    },
    {
      "id": 2,
      "location": [-104.8592, 39.7171],
      "delivery": [1, 0, 0, 0],
      "skills": [1],
      "description": "Ambulatory patient"
    }
  ]
}
```

**Result:**
- Job 1 assigned to vehicle 2 (requires skills 1 AND 2)
- Job 2 can go to either vehicle (requires only skill 1)

---

## Common Patterns

### Pattern 1: Time Window Buffer Model

For pickups with arrival windows:

```json
{
  "pickup": {
    "time_windows": [[1770220800, 1770221000]],
    "service": 180
  }
}
```

- Vehicle can arrive between 1770220800-1770221000 (3:20 PM - 3:23 PM)
- Service takes 180 seconds (3 minutes)
- Can arrive early and wait
- Cannot arrive late

### Pattern 2: Round Trip Patient Transport

```json
{
  "shipments": [
    {
      "id": 10,
      "description": "Outbound",
      "pickup": {
        "location": "home",
        "time_windows": [[pickup_time_start, pickup_time_end]]
      },
      "delivery": {
        "location": "clinic"
      }
    },
    {
      "id": 11,
      "description": "Return",
      "pickup": {
        "location": "clinic",
        "time_windows": [[return_time_start, return_time_end]]
      },
      "delivery": {
        "location": "home"
      }
    }
  ],
  "relations": [
    {
      "type": "in_direct_sequence",
      "steps": [
        {"type": "shipment", "id": 10},
        {"type": "shipment", "id": 11}
      ]
    }
  ]
}
```

### Pattern 3: Meal Delivery Route

```json
{
  "vehicles": [
    {
      "id": 1,
      "steps": [
        {"type": "start"},
        {"type": "job", "id": 100},
        {"type": "end"}
      ]
    }
  ],
  "jobs": [
    {
      "id": 100,
      "description": "Kitchen pickup (fixed)",
      "location": "kitchen",
      "pickup": [20, 0, 0, 0]
    },
    {
      "id": 200,
      "description": "Client 1 delivery (flexible)",
      "location": "client1",
      "delivery": [1, 0, 0, 0],
      "time_windows": [[lunch_start, lunch_end]]
    },
    {
      "id": 201,
      "description": "Client 2 delivery (flexible)",
      "location": "client2",
      "delivery": [1, 0, 0, 0],
      "time_windows": [[lunch_start, lunch_end]]
    }
  ]
}
```

---

## Troubleshooting

### All Jobs Unassigned

**Check:**
1. Time windows are feasible
2. Shipment IDs match in relations
3. Vehicle capacity sufficient
4. Skills requirements are met
5. Travel times don't exceed time windows

### Assertion Errors

**Common causes:**
1. Missing shipment IDs in relation references
2. Capacity dimension mismatch (vehicle has 4D, jobs have null)
3. Invalid time window formats

### Poor Routes

**Check:**
1. Set appropriate vehicle `time_window`
2. Use realistic service times
3. Ensure breaks are properly configured
4. Check if priority values are reasonable

---

## Best Practices

1. **Always set shipment IDs** when using relations
2. **Use consistent capacity dimensions** across all vehicles and jobs
3. **Test time window feasibility** before running large optimizations
4. **Start simple** - add complexity incrementally
5. **Use descriptions** for debugging
6. **Validate input JSON** before sending to VROOM
7. **Check unassigned jobs** to understand why they couldn't be assigned

---

## Reference

- **Time format**: Unix timestamp (seconds since epoch)
- **Locations**: [longitude, latitude] in decimal degrees
- **Durations**: Seconds
- **Capacity**: Array of non-negative integers
- **Skills**: Array of positive integers
- **Priority**: 0-100 (higher = more important)

---

## Additional Resources

- VROOM GitHub: https://github.com/VROOM-Project/vroom
- API Documentation: https://github.com/VROOM-Project/vroom/blob/master/docs/API.md
- Time window handling: See `tw_route.cpp` in source code
- Relation constraints: See `src/structures/vroom/input/input.cpp`
