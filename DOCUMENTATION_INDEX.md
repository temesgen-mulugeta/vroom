# VROOM Documentation Index

Quick navigation to all VROOM documentation.

## 📚 Documentation Files

### Getting Started

1. **[README_IMPROVEMENTS.md](README_IMPROVEMENTS.md)** ⭐ START HERE
   - Overview of improvements made to VROOM
   - What was fixed and why
   - Migration guide for existing users
   - Testing results and known issues

### Feature Documentation

2. **[FEATURES_GUIDE.md](FEATURES_GUIDE.md)** 📖 COMPREHENSIVE GUIDE
   - Complete guide to all VROOM features
   - Jobs vs Shipments
   - Time Windows explained
   - **Vehicle Steps** (fixed job sequences)
   - **Relations** (shipment sequences with `in_direct_sequence`)
   - Capacity constraints (multi-dimensional)
   - Skills and breaks
   - Complete working examples
   - Common patterns and best practices

3. **[TEMPLATES.md](TEMPLATES.md)** 📋 QUICK REFERENCE
   - Ready-to-use JSON templates
   - Medical transport scenarios (single trip, round trip, wheelchair)
   - Delivery service templates
   - Complex multi-vehicle scenarios
   - Time calculation helpers
   - Validation checklists

### Troubleshooting

4. **[TROUBLESHOOTING.md](TROUBLESHOOTING.md)** 🔧 PROBLEM SOLVING
   - Common error messages and solutions
   - Data integrity issue detection
   - Validation scripts
   - Fix scripts for broken input files
   - Performance optimization tips
   - Step-by-step debugging guides

---

## 🎯 Quick Links by Task

### "I want to..."

#### Learn VROOM Features
→ Start with [FEATURES_GUIDE.md](FEATURES_GUIDE.md)
- Read sections in order for complete understanding
- Focus on specific sections if you know what you need

#### Create My First Input File
→ Use [TEMPLATES.md](TEMPLATES.md)
1. Find template matching your use case
2. Copy template
3. Replace placeholder values
4. Validate with scripts from TROUBLESHOOTING.md
5. Run VROOM

#### Fix a Broken Input File
→ Go to [TROUBLESHOOTING.md](TROUBLESHOOTING.md)
1. Run validation script: `python3 validate_vroom_input.py input.json`
2. Follow specific fix instructions
3. Re-validate
4. Run VROOM

#### Understand Vehicle Steps
→ See [FEATURES_GUIDE.md#vehicle-steps](FEATURES_GUIDE.md#vehicle-steps)
- Use when jobs must be assigned to specific vehicle in fixed order
- Example: Mandatory facility checkpoints

#### Understand Relations
→ See [FEATURES_GUIDE.md#relations](FEATURES_GUIDE.md#relations)
- Use for shipment sequences (round trips, multi-leg journeys)
- Type: `in_direct_sequence` ensures shipments follow in order

#### Handle Round Trips
→ See [TEMPLATES.md#template-2-round-trip-using-relations](TEMPLATES.md#template-2-round-trip-using-relations)
- Two shipments with relation
- Outbound: Home → Clinic
- Return: Clinic → Home

#### Debug "All Jobs Unassigned"
→ See [TROUBLESHOOTING.md#all-jobs-unassigned](TROUBLESHOOTING.md#all-jobs-unassigned)
1. Check time windows
2. Check skills
3. Check capacity
4. Check travel times

#### Fix Missing Shipment IDs
→ See [TROUBLESHOOTING.md#missing-shipment-ids](TROUBLESHOOTING.md#missing-shipment-ids)
- Use provided `fix_shipment_ids.py` script
- Or manually add IDs to match relation references

---

## 📖 Reading Order

### For New Users

1. **[README_IMPROVEMENTS.md](README_IMPROVEMENTS.md)** - Understand what VROOM can do
2. **[TEMPLATES.md](TEMPLATES.md)** - Start with a working template
3. **[FEATURES_GUIDE.md](FEATURES_GUIDE.md)** - Deep dive into features you need
4. **[TROUBLESHOOTING.md](TROUBLESHOOTING.md)** - When things go wrong

### For Existing VROOM Users

1. **[README_IMPROVEMENTS.md](README_IMPROVEMENTS.md)** - See what's changed
2. **[FEATURES_GUIDE.md#vehicle-steps](FEATURES_GUIDE.md#vehicle-steps)** - Learn about steps
3. **[FEATURES_GUIDE.md#relations](FEATURES_GUIDE.md#relations)** - Learn about relations
4. **[TROUBLESHOOTING.md](TROUBLESHOOTING.md)** - Bookmark for later

### For Debugging

1. **[TROUBLESHOOTING.md](TROUBLESHOOTING.md)** - Error message lookup
2. **[TEMPLATES.md](TEMPLATES.md)** - Compare your input to working examples
3. **[FEATURES_GUIDE.md](FEATURES_GUIDE.md)** - Understand how features should work

---

## 🔍 Feature Matrix

Quick reference for what feature does what:

| Feature | Use Case | Doc Link |
|---------|----------|----------|
| **Jobs** | Single-location deliveries/pickups | [Features](FEATURES_GUIDE.md#jobs-vs-shipments) |
| **Shipments** | Pickup + delivery pairs | [Features](FEATURES_GUIDE.md#jobs-vs-shipments) |
| **Time Windows** | Restrict when service can occur | [Features](FEATURES_GUIDE.md#time-windows) |
| **Vehicle Steps** | Fix job sequence to specific vehicle | [Features](FEATURES_GUIDE.md#vehicle-steps) |
| **Relations** | Require shipments in sequence | [Features](FEATURES_GUIDE.md#relations) |
| **Skills** | Match jobs to qualified vehicles | [Features](FEATURES_GUIDE.md#skills) |
| **Capacity** | Multi-dimensional load tracking | [Features](FEATURES_GUIDE.md#capacity-constraints) |
| **Breaks** | Mandatory rest periods | [Features](FEATURES_GUIDE.md#breaks) |

---

## 💡 Common Scenarios

### Medical Transport

| Scenario | Template | Features Used |
|----------|----------|---------------|
| One-way patient trip | [Template 1](TEMPLATES.md#template-1-single-patient-trip) | Shipments, Time Windows |
| Round trip | [Template 2](TEMPLATES.md#template-2-round-trip-using-relations) | Shipments, Relations |
| Wheelchair accessible | [Template 3](TEMPLATES.md#template-3-wheelchair-transport) | Skills, Capacity |
| Fixed facility checkpoint | [Template 4](TEMPLATES.md#template-4-multi-stop-with-facility-checkpoint) | Vehicle Steps, Shipments |

### Delivery Service

| Scenario | Template | Features Used |
|----------|----------|---------------|
| Package delivery | [Template 5](TEMPLATES.md#template-5-package-delivery) | Jobs, Capacity |
| Pickup and deliver | [Template 6](TEMPLATES.md#template-6-pickup-and-delivery) | Shipments |
| With lunch break | [Template 7](TEMPLATES.md#template-7-with-mandatory-lunch-break) | Breaks |

### Complex Routes

| Scenario | Template | Features Used |
|----------|----------|---------------|
| Multiple vehicle types | [Template 8](TEMPLATES.md#template-8-multi-vehicle-with-skills) | Skills, Capacity |
| Multiple round trips | [Template 9](TEMPLATES.md#template-9-multiple-round-trips) | Relations (multiple) |
| Three-leg journey | [Template 10](TEMPLATES.md#template-10-three-leg-journey) | Relations (3+ shipments) |

---

## 🛠 Validation & Tools

### Scripts Provided

Located in [TROUBLESHOOTING.md](TROUBLESHOOTING.md):

1. **validate_vroom_input.py** - Check for common issues
   ```bash
   python3 validate_vroom_input.py input.json
   ```

2. **fix_shipment_ids.py** - Auto-assign missing IDs
   ```bash
   python3 fix_shipment_ids.py input.json
   ```

3. **Quick jq commands** - Data inspection
   ```bash
   # Check shipment IDs
   jq '[.shipments[].id] | unique' input.json

   # Check capacity dimensions
   jq '.vehicles[0].capacity | length' input.json
   ```

---

## ⚠️ Common Pitfalls

### 1. Missing Shipment IDs
**Problem:** Relations reference shipment IDs but shipments have `"id": null`

**Solution:** [TROUBLESHOOTING.md#missing-shipment-ids](TROUBLESHOOTING.md#missing-shipment-ids)

### 2. Time Window Infeasibility
**Problem:** "Job cannot be reached within any time window"

**Solution:** [TROUBLESHOOTING.md#time-window-problems](TROUBLESHOOTING.md#time-window-problems)

### 3. Capacity Mismatch
**Problem:** "Assertion failed: (a.size() == b.size())"

**Solution:** [TROUBLESHOOTING.md#capacity-dimension-mismatch](TROUBLESHOOTING.md#capacity-dimension-mismatch)

### 4. All Jobs Unassigned
**Problem:** Optimization completes but all jobs unassigned

**Solution:** [TROUBLESHOOTING.md#all-jobs-unassigned](TROUBLESHOOTING.md#all-jobs-unassigned)

---

## 📝 JSON Schema Reference

### Complete Input Structure

```json
{
  "vehicles": [
    {
      "id": <number>,
      "profile": <string, optional>,
      "start": [<longitude>, <latitude>],
      "end": [<longitude>, <latitude>, optional],
      "capacity": [<array of numbers>, optional],
      "skills": [<array of skill IDs>, optional],
      "time_window": [<start>, <end>],
      "breaks": [<array of breaks>, optional],
      "steps": [<array of steps>, optional]
    }
  ],
  "jobs": [
    {
      "id": <number, required>,
      "location": [<longitude>, <latitude>],
      "service": <number, seconds>,
      "delivery": [<array matching capacity dims>],
      "pickup": [<array matching capacity dims>],
      "time_windows": [[<start>, <end>]],
      "skills": [<array of required skills>],
      "priority": <0-100>
    }
  ],
  "shipments": [
    {
      "id": <number, required for relations>,
      "amount": [<array matching capacity dims>],
      "skills": [<array of required skills>],
      "pickup": {
        "id": <number>,
        "location": [<longitude>, <latitude>],
        "service": <number>,
        "time_windows": [[<start>, <end>]]
      },
      "delivery": {
        "id": <number>,
        "location": [<longitude>, <latitude>],
        "service": <number>,
        "time_windows": [[<start>, <end>]]
      }
    }
  ],
  "relations": [
    {
      "type": "in_direct_sequence",
      "steps": [
        {"type": "shipment", "id": <shipment_id>}
      ]
    }
  ]
}
```

See [FEATURES_GUIDE.md](FEATURES_GUIDE.md) for detailed explanations of each field.

---

## 🚀 Getting Started Checklist

- [ ] Read [README_IMPROVEMENTS.md](README_IMPROVEMENTS.md)
- [ ] Choose template from [TEMPLATES.md](TEMPLATES.md)
- [ ] Replace placeholders with your data
- [ ] Run validation: `python3 validate_vroom_input.py input.json`
- [ ] Fix any issues found
- [ ] Run VROOM: `./bin/vroom -i input.json`
- [ ] Check output for `unassigned` jobs
- [ ] If issues, consult [TROUBLESHOOTING.md](TROUBLESHOOTING.md)
- [ ] Bookmark this index for future reference

---

## 📞 Additional Resources

- **VROOM GitHub:** https://github.com/VROOM-Project/vroom
- **Official API Docs:** https://github.com/VROOM-Project/vroom/blob/master/docs/API.md
- **This Documentation:** Local files in vroom directory

---

## 📄 Document Versions

- **Features Guide:** Comprehensive (15,000+ words)
- **Templates:** Quick reference with 10 ready-to-use templates
- **Troubleshooting:** Detailed with validation scripts
- **Last Updated:** February 2026

---

Happy routing! 🚗📦🏥
