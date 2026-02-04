#!/usr/bin/env python3
"""Generate a matrix-based VROOM fixture from coordinate inputs."""

import argparse
import json
import math
from typing import Dict, List, Tuple


def haversine(a: Tuple[float, float], b: Tuple[float, float]) -> float:
    lon1, lat1 = a
    lon2, lat2 = b
    radius = 6371000.0
    phi1 = math.radians(lat1)
    phi2 = math.radians(lat2)
    dphi = math.radians(lat2 - lat1)
    dlambda = math.radians(lon2 - lon1)
    h = math.sin(dphi / 2) ** 2 + math.cos(phi1) * math.cos(phi2) * math.sin(dlambda / 2) ** 2
    return 2 * radius * math.asin(math.sqrt(h))


def build_indices(data: Dict) -> List[Tuple[float, float]]:
    coords: List[Tuple[float, float]] = []
    coord_to_index: Dict[Tuple[float, float], int] = {}

    def add_coord(coord: List[float]) -> int:
        key = (float(coord[0]), float(coord[1]))
        if key not in coord_to_index:
            coord_to_index[key] = len(coords)
            coords.append(key)
        return coord_to_index[key]

    for vehicle in data.get("vehicles", []):
        if "start" in vehicle:
            vehicle["start_index"] = add_coord(vehicle["start"])
        if "end" in vehicle:
            vehicle["end_index"] = add_coord(vehicle["end"])

    for job in data.get("jobs", []):
        if "location" in job:
            job["location_index"] = add_coord(job["location"])

    for shipment in data.get("shipments", []):
        for key in ("pickup", "delivery"):
            if "location" in shipment.get(key, {}):
                shipment[key]["location_index"] = add_coord(shipment[key]["location"])

    return coords


def build_durations(coords: List[Tuple[float, float]], speed_kmh: float) -> List[List[int]]:
    speed_mps = (speed_kmh * 1000.0) / 3600.0
    n = len(coords)
    durations = [[0] * n for _ in range(n)]

    for i in range(n):
        for j in range(n):
            if i == j:
                durations[i][j] = 0
                continue
            dist = haversine(coords[i], coords[j])
            durations[i][j] = int(round(dist / speed_mps))

    return durations


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate matrix-based VROOM fixture")
    parser.add_argument("input", help="Path to source JSON input")
    parser.add_argument("output", help="Path to output JSON with matrices")
    parser.add_argument(
        "--speed",
        type=float,
        default=30.0,
        help="Average speed in km/h for duration estimation (default: 30)",
    )
    args = parser.parse_args()

    with open(args.input, "r", encoding="utf-8") as f:
        data = json.load(f)

    coords = build_indices(data)
    durations = build_durations(coords, args.speed)

    data["matrices"] = {"car": {"durations": durations}}

    with open(args.output, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
