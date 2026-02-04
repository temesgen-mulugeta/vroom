# VROOM JSON Templates

Quick reference templates for common VROOM scenarios.

## Table of Contents

- [Basic Setup](#basic-setup)
- [Medical Transport Templates](#medical-transport-templates)
- [Delivery Service Templates](#delivery-service-templates)
- [Complex Scenarios](#complex-scenarios)

---

## Basic Setup

### Minimal Working Example

```json
{
  "vehicles": [
    {
      "id": 1,
      "start": [-104.9903, 39.7392],
      "end": [-104.9903, 39.7392]
    }
  ],
  "jobs": [
    {
      "id": 1,
      "location": [-104.8592, 39.7171],
      "service": 300
    }
  ]
}
```

### With Time Windows

```json
{
  "vehicles": [
    {
      "id": 1,
      "start": [-104.9903, 39.7392],
      "end": [-104.9903, 39.7392],
      "time_window": [1770202800, 1770267600]
    }
  ],
  "jobs": [
    {
      "id": 1,
      "location": [-104.8592, 39.7171],
      "service": 300,
      "time_windows": [[1770210000, 1770215000]]
    }
  ]
}
```

---

## Medical Transport Templates

### Template 1: Single Patient Trip

One-way patient transport (home → clinic):

```json
{
  "vehicles": [
    {
      "id": 1,
      "profile": "car",
      "start": [-104.9903, 39.7392],
      "end": [-104.9903, 39.7392],
      "capacity": [3, 1, 0, 0],
      "skills": [1],
      "time_window": [SHIFT_START, SHIFT_END],
      "description": "Ambulatory vehicle"
    }
  ],
  "shipments": [
    {
      "id": 100,
      "amount": [1, 0, 0, 0],
      "skills": [1],
      "description": "Patient transport",
      "pickup": {
        "id": 10001,
        "location": [HOME_LONG, HOME_LAT],
        "service": 120,
        "time_windows": [[PICKUP_START, PICKUP_END]],
        "description": "Patient home"
      },
      "delivery": {
        "id": 10002,
        "location": [CLINIC_LONG, CLINIC_LAT],
        "service": 120,
        "description": "Medical facility"
      }
    }
  ]
}
```

**Replace:**
- `SHIFT_START`, `SHIFT_END`: Vehicle operating hours (Unix seconds)
- `PICKUP_START`, `PICKUP_END`: Pickup time window
- `HOME_LONG`, `HOME_LAT`: Patient pickup coordinates
- `CLINIC_LONG`, `CLINIC_LAT`: Destination coordinates

### Template 2: Round Trip (Using Relations)

Patient outbound + return trip:

```json
{
  "vehicles": [
    {
      "id": 1,
      "start": [-104.9903, 39.7392],
      "end": [-104.9903, 39.7392],
      "capacity": [3, 1, 0, 0],
      "time_window": [SHIFT_START, SHIFT_END]
    }
  ],
  "shipments": [
    {
      "id": 100,
      "amount": [1, 0, 0, 0],
      "description": "Outbound trip",
      "pickup": {
        "id": 10001,
        "location": [HOME_LONG, HOME_LAT],
        "service": 120,
        "time_windows": [[OUTBOUND_PICKUP_START, OUTBOUND_PICKUP_END]]
      },
      "delivery": {
        "id": 10002,
        "location": [CLINIC_LONG, CLINIC_LAT],
        "service": 120
      }
    },
    {
      "id": 101,
      "amount": [1, 0, 0, 0],
      "description": "Return trip",
      "pickup": {
        "id": 10101,
        "location": [CLINIC_LONG, CLINIC_LAT],
        "service": 120,
        "time_windows": [[RETURN_PICKUP_START, RETURN_PICKUP_END]]
      },
      "delivery": {
        "id": 10102,
        "location": [HOME_LONG, HOME_LAT],
        "service": 120
      }
    }
  ],
  "relations": [
    {
      "type": "in_direct_sequence",
      "steps": [
        {"type": "shipment", "id": 100},
        {"type": "shipment", "id": 101}
      ]
    }
  ]
}
```

**Replace:**
- `OUTBOUND_PICKUP_START/END`: When to pick up patient
- `RETURN_PICKUP_START/END`: When to pick up from appointment (allow appointment duration)

**Gap Calculation:**
```
RETURN_PICKUP_START = OUTBOUND_PICKUP_END + TRAVEL_TIME + APPOINTMENT_DURATION
```

### Template 3: Wheelchair Transport

Vehicle with wheelchair capacity:

```json
{
  "vehicles": [
    {
      "id": 1,
      "capacity": [1, 1, 2, 0],
      "skills": [1, 2],
      "description": "Wheelchair accessible"
    }
  ],
  "shipments": [
    {
      "id": 100,
      "amount": [0, 0, 1, 0],
      "skills": [1, 2],
      "description": "Wheelchair patient",
      "pickup": {
        "id": 10001,
        "location": [HOME_LONG, HOME_LAT],
        "service": 180,
        "time_windows": [[PICKUP_START, PICKUP_END]]
      },
      "delivery": {
        "id": 10002,
        "location": [CLINIC_LONG, CLINIC_LAT],
        "service": 180
      }
    }
  ]
}
```

**Capacity Dimensions:**
- `[ambulatory_count, stretcher_count, wheelchair_count, reserved]`

**Skills:**
- `1`: Basic transport
- `2`: Wheelchair accessible

### Template 4: Multi-Stop with Facility Checkpoint

Driver must check in at facility first:

```json
{
  "vehicles": [
    {
      "id": 1,
      "start": [DEPOT_LONG, DEPOT_LAT],
      "end": [DEPOT_LONG, DEPOT_LAT],
      "capacity": [6, 1, 0, 0],
      "time_window": [SHIFT_START, SHIFT_END],
      "steps": [
        {"type": "start"},
        {"type": "job", "id": 1000},
        {"type": "end"}
      ]
    }
  ],
  "jobs": [
    {
      "id": 1000,
      "location": [FACILITY_LONG, FACILITY_LAT],
      "service": 300,
      "description": "Required facility check-in"
    }
  ],
  "shipments": [
    {
      "id": 100,
      "description": "Patient trip 1 (can insert anywhere)",
      "pickup": {
        "id": 10001,
        "location": [HOME1_LONG, HOME1_LAT],
        "service": 120,
        "time_windows": [[PICKUP1_START, PICKUP1_END]]
      },
      "delivery": {
        "id": 10002,
        "location": [CLINIC1_LONG, CLINIC1_LAT],
        "service": 120
      }
    },
    {
      "id": 101,
      "description": "Patient trip 2 (can insert anywhere)",
      "pickup": {
        "id": 10101,
        "location": [HOME2_LONG, HOME2_LAT],
        "service": 120,
        "time_windows": [[PICKUP2_START, PICKUP2_END]]
      },
      "delivery": {
        "id": 10102,
        "location": [CLINIC2_LONG, CLINIC2_LAT],
        "service": 120
      }
    }
  ]
}
```

**Result:** Vehicle checks in at facility (job 1000), then optimally schedules patient trips.

---

## Delivery Service Templates

### Template 5: Package Delivery

Simple delivery route:

```json
{
  "vehicles": [
    {
      "id": 1,
      "start": [WAREHOUSE_LONG, WAREHOUSE_LAT],
      "end": [WAREHOUSE_LONG, WAREHOUSE_LAT],
      "capacity": [1000],
      "time_window": [SHIFT_START, SHIFT_END]
    }
  ],
  "jobs": [
    {
      "id": 1,
      "location": [CUSTOMER1_LONG, CUSTOMER1_LAT],
      "service": 180,
      "delivery": [25],
      "time_windows": [[DELIVERY_START, DELIVERY_END]],
      "description": "Customer 1"
    },
    {
      "id": 2,
      "location": [CUSTOMER2_LONG, CUSTOMER2_LAT],
      "service": 180,
      "delivery": [15],
      "time_windows": [[DELIVERY_START, DELIVERY_END]],
      "description": "Customer 2"
    }
  ]
}
```

### Template 6: Pickup and Delivery

Pickup from warehouse, deliver to customer:

```json
{
  "vehicles": [
    {
      "id": 1,
      "start": [DEPOT_LONG, DEPOT_LAT],
      "capacity": [100]
    }
  ],
  "shipments": [
    {
      "id": 1,
      "amount": [25],
      "pickup": {
        "id": 101,
        "location": [WAREHOUSE_LONG, WAREHOUSE_LAT],
        "service": 300
      },
      "delivery": {
        "id": 102,
        "location": [CUSTOMER_LONG, CUSTOMER_LAT],
        "service": 180,
        "time_windows": [[DELIVERY_START, DELIVERY_END]]
      }
    }
  ]
}
```

### Template 7: With Mandatory Lunch Break

```json
{
  "vehicles": [
    {
      "id": 1,
      "start": [DEPOT_LONG, DEPOT_LAT],
      "capacity": [100],
      "time_window": [SHIFT_START, SHIFT_END],
      "breaks": [
        {
          "id": 1,
          "time_windows": [
            [LUNCH_START, LUNCH_END]
          ],
          "service": 1800,
          "max_load": [10]
        }
      ]
    }
  ],
  "jobs": [
    {
      "id": 1,
      "location": [CUSTOMER1_LONG, CUSTOMER1_LAT],
      "delivery": [20],
      "service": 180
    }
  ]
}
```

**Notes:**
- `max_load: [10]`: Can only break if carrying ≤ 10 units
- Forces deliveries before break

---

## Complex Scenarios

### Template 8: Multi-Vehicle with Skills

Different vehicle types:

```json
{
  "vehicles": [
    {
      "id": 1,
      "capacity": [100, 50],
      "skills": [1],
      "description": "Small van"
    },
    {
      "id": 2,
      "capacity": [200, 100],
      "skills": [1, 2],
      "description": "Large truck with lift gate"
    }
  ],
  "jobs": [
    {
      "id": 1,
      "delivery": [30, 20],
      "skills": [1],
      "description": "Standard delivery (any vehicle)"
    },
    {
      "id": 2,
      "delivery": [150, 80],
      "skills": [1, 2],
      "description": "Heavy item (requires truck with lift gate)"
    }
  ]
}
```

### Template 9: Multiple Round Trips

Several patients with round trips:

```json
{
  "vehicles": [
    {
      "id": 1,
      "capacity": [3, 1, 0, 0],
      "time_window": [SHIFT_START, SHIFT_END]
    }
  ],
  "shipments": [
    {
      "id": 100,
      "description": "Patient 1 outbound",
      "pickup": {
        "id": 10001,
        "location": [HOME1_LONG, HOME1_LAT],
        "service": 120,
        "time_windows": [[P1_OUT_START, P1_OUT_END]]
      },
      "delivery": {
        "id": 10002,
        "location": [CLINIC1_LONG, CLINIC1_LAT],
        "service": 120
      }
    },
    {
      "id": 101,
      "description": "Patient 1 return",
      "pickup": {
        "id": 10101,
        "location": [CLINIC1_LONG, CLINIC1_LAT],
        "service": 120,
        "time_windows": [[P1_RET_START, P1_RET_END]]
      },
      "delivery": {
        "id": 10102,
        "location": [HOME1_LONG, HOME1_LAT],
        "service": 120
      }
    },
    {
      "id": 200,
      "description": "Patient 2 outbound",
      "pickup": {
        "id": 20001,
        "location": [HOME2_LONG, HOME2_LAT],
        "service": 120,
        "time_windows": [[P2_OUT_START, P2_OUT_END]]
      },
      "delivery": {
        "id": 20002,
        "location": [CLINIC2_LONG, CLINIC2_LAT],
        "service": 120
      }
    },
    {
      "id": 201,
      "description": "Patient 2 return",
      "pickup": {
        "id": 20101,
        "location": [CLINIC2_LONG, CLINIC2_LAT],
        "service": 120,
        "time_windows": [[P2_RET_START, P2_RET_END]]
      },
      "delivery": {
        "id": 20102,
        "location": [HOME2_LONG, HOME2_LAT],
        "service": 120
      }
    }
  ],
  "relations": [
    {
      "type": "in_direct_sequence",
      "steps": [
        {"type": "shipment", "id": 100},
        {"type": "shipment", "id": 101}
      ]
    },
    {
      "type": "in_direct_sequence",
      "steps": [
        {"type": "shipment", "id": 200},
        {"type": "shipment", "id": 201}
      ]
    }
  ]
}
```

**Note:** Relations are independent - different vehicles can handle each round trip.

### Template 10: Three-Leg Journey

Patient needs multiple stops in sequence:

```json
{
  "shipments": [
    {
      "id": 10,
      "description": "Leg 1: Home to Pharmacy",
      "pickup": {
        "id": 1001,
        "location": [HOME_LONG, HOME_LAT],
        "service": 120,
        "time_windows": [[LEG1_START, LEG1_END]]
      },
      "delivery": {
        "id": 1002,
        "location": [PHARMACY_LONG, PHARMACY_LAT],
        "service": 300
      }
    },
    {
      "id": 11,
      "description": "Leg 2: Pharmacy to Doctor",
      "pickup": {
        "id": 1101,
        "location": [PHARMACY_LONG, PHARMACY_LAT],
        "service": 120,
        "time_windows": [[LEG2_START, LEG2_END]]
      },
      "delivery": {
        "id": 1102,
        "location": [DOCTOR_LONG, DOCTOR_LAT],
        "service": 120
      }
    },
    {
      "id": 12,
      "description": "Leg 3: Doctor to Home",
      "pickup": {
        "id": 1201,
        "location": [DOCTOR_LONG, DOCTOR_LAT],
        "service": 120,
        "time_windows": [[LEG3_START, LEG3_END]]
      },
      "delivery": {
        "id": 1202,
        "location": [HOME_LONG, HOME_LAT],
        "service": 120
      }
    }
  ],
  "relations": [
    {
      "type": "in_direct_sequence",
      "steps": [
        {"type": "shipment", "id": 10},
        {"type": "shipment", "id": 11},
        {"type": "shipment", "id": 12}
      ]
    }
  ]
}
```

**Sequence:**
```
P(10) → D(10) → P(11) → D(11) → P(12) → D(12)
Home → Pharmacy → Pharmacy → Doctor → Doctor → Home
```

---

## Time Calculation Helpers

### Unix Timestamp Conversion

```javascript
// JavaScript
const date = new Date('2026-02-03T08:00:00');
const unixTimestamp = Math.floor(date.getTime() / 1000);
console.log(unixTimestamp); // 1770202800

// Python
from datetime import datetime
dt = datetime(2026, 2, 3, 8, 0, 0)
unix_timestamp = int(dt.timestamp())
print(unix_timestamp)  # 1770202800
```

### Time Window With Buffer

For 2:00 PM pickup with 15-minute arrival window:

```javascript
const pickupTime = new Date('2026-02-03T14:00:00');
const pickupUnix = Math.floor(pickupTime.getTime() / 1000);

const timeWindow = [
  pickupUnix - 900,  // 15 min before (1:45 PM)
  pickupUnix         // Exact pickup time (2:00 PM)
];

// Result: [1770224100, 1770225000]
```

### Appointment Duration Calculation

For round trip with 2-hour appointment:

```javascript
const outboundPickup = 1770220800;  // 9:00 AM
const travelTime = 1200;              // 20 minutes each way
const appointmentDuration = 7200;     // 2 hours

const returnPickup = outboundPickup + travelTime + appointmentDuration;
// Return pickup window starts 2 hours 20 minutes after outbound pickup
```

---

## Validation Checklist

Before running VROOM, verify:

- [ ] All shipment `id` fields are unique and not null
- [ ] All shipments referenced in relations exist
- [ ] Capacity dimensions match across all entities
- [ ] Time windows are in Unix seconds
- [ ] Locations are [longitude, latitude] (not reversed!)
- [ ] Vehicle time_window encompasses all job time_windows
- [ ] Service times are in seconds
- [ ] Skills arrays contain only positive integers
- [ ] Relation shipment IDs match actual shipment IDs

---

## Common Replacements

When using templates, replace these placeholders:

| Placeholder | Meaning | Example Value |
|-------------|---------|---------------|
| `SHIFT_START` | Vehicle shift start time | `1770202800` |
| `SHIFT_END` | Vehicle shift end time | `1770267600` |
| `DEPOT_LONG` | Depot longitude | `-104.9903` |
| `DEPOT_LAT` | Depot latitude | `39.7392` |
| `HOME_LONG` | Patient home longitude | `-104.9779` |
| `HOME_LAT` | Patient home latitude | `39.7339` |
| `CLINIC_LONG` | Clinic longitude | `-104.8592` |
| `CLINIC_LAT` | Clinic latitude | `39.7171` |
| `PICKUP_START` | Pickup window start | `1770220800` |
| `PICKUP_END` | Pickup window end | `1770221000` |

---

## Next Steps

1. Copy appropriate template
2. Replace placeholder values
3. Validate JSON syntax
4. Run VROOM: `./bin/vroom -i input.json`
5. Check `unassigned` array in output
6. Review routes and timings

For complete feature documentation, see [FEATURES_GUIDE.md](FEATURES_GUIDE.md)
