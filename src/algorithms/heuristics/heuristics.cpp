/*

This file is part of VROOM.

Copyright (c) 2015-2025, Julien Coupey.
All rights reserved (see LICENSE).

*/

#include <algorithm>
#include <iostream>
#include <optional>

#include "algorithms/heuristics/heuristics.h"
#include "utils/exception.h"
#include "utils/helpers.h"

namespace vroom::heuristics {

// Helper function to find position of a job in vehicle steps
inline std::optional<Index> find_job_position_in_steps(
  const std::vector<VehicleStep>& steps,
  const Input& input,
  Index job_rank) {
  for (Index i = 0; i < steps.size(); ++i) {
    const auto& step = steps[i];
    if (step.type != STEP_TYPE::JOB || !step.job_type.has_value()) {
      continue;
    }

    Index step_rank = 0;
    if (step.job_type.value() == JOB_TYPE::SINGLE) {
      if (!input.job_id_to_rank.contains(step.id)) {
        continue;
      }
      step_rank = input.job_id_to_rank.at(step.id);
    } else if (step.job_type.value() == JOB_TYPE::PICKUP) {
      if (!input.pickup_id_to_rank.contains(step.id)) {
        continue;
      }
      step_rank = input.pickup_id_to_rank.at(step.id);
    } else if (step.job_type.value() == JOB_TYPE::DELIVERY) {
      if (!input.delivery_id_to_rank.contains(step.id)) {
        continue;
      }
      step_rank = input.delivery_id_to_rank.at(step.id);
    }

    if (step_rank == job_rank) {
      return i;
    }
  }
  return std::nullopt;
}

// Helper function to check if a job is in a route
inline bool is_job_in_route(const std::vector<Index>& route, Index job_rank) {
  return std::find(route.begin(), route.end(), job_rank) != route.end();
}

// Check if inserting at position would break shipment atomicity
// (no jobs between pickup and delivery)
template <class Route>
inline bool would_break_shipment_atomicity(const Input& input,
                                           const Route& route,
                                           Index position) {
  if (position == 0 || position >= route.route.size()) {
    return false;  // Can't break atomicity at start or end
  }

  const Index before_job = route.route[position - 1];
  const Index after_job = route.route[position];

  const auto& before = input.jobs[before_job];
  const auto& after = input.jobs[after_job];

  // Check if before is a pickup and after is its delivery
  if (before.type == JOB_TYPE::PICKUP &&
      after.type == JOB_TYPE::DELIVERY &&
      before_job + 1 == after_job) {
    return true;  // Would interrupt shipment
  }

  return false;
}

// Check if inserting at position would break the consecutive vehicle steps constraint
// Vehicle steps must remain consecutive with NO jobs/shipments between them
template <class Route>
inline bool would_break_consecutive_steps(const Input& input,
                                          const Route& route,
                                          Index position,
                                          Index v_rank) {
  const auto& vehicle = input.vehicles[v_rank];

  if (vehicle.steps.empty() || position == 0) {
    return false;  // No steps or inserting at start
  }

  // Check if we're trying to insert between two consecutive vehicle steps
  if (position > 0 && position < route.route.size()) {
    Index before_job = route.route[position - 1];
    Index after_job = route.route[position];

    // Check if before_job is a vehicle step
    bool before_is_step = input.fixed_job_ranks.contains(before_job);
    bool after_is_step = input.fixed_job_ranks.contains(after_job);

    if (before_is_step && after_is_step) {
      // Both before and after are vehicle steps
      // Check if they are consecutive steps in vehicle.steps
      auto before_step_pos = find_job_position_in_steps(vehicle.steps, input, before_job);
      auto after_step_pos = find_job_position_in_steps(vehicle.steps, input, after_job);

      if (before_step_pos.has_value() && after_step_pos.has_value()) {
        // Check if after_step immediately follows before_step
        if (after_step_pos.value() == before_step_pos.value() + 1) {
          return true;  // Would break consecutive steps!
        }
      }
    }
  }

  return false;
}

// Check if all predecessor steps for a fixed job are already in the route
template <class Route>
inline bool can_insert_fixed_job(const Input& input,
                                 const Route& route,
                                 Index job_rank,
                                 Index v_rank) {
  const auto& vehicle = input.vehicles[v_rank];

  if (vehicle.steps.empty()) {
    return true;  // No step constraints
  }

  // Find position of this job in vehicle.steps
  auto step_pos = find_job_position_in_steps(vehicle.steps, input, job_rank);
  if (!step_pos.has_value()) {
    return true;  // Not a step, can insert freely
  }

  // Check if all previous steps are already in the route
  for (Index i = 0; i < step_pos.value(); ++i) {
    const auto& prev_step = vehicle.steps[i];
    if (prev_step.type != STEP_TYPE::JOB || !prev_step.job_type.has_value()) {
      continue;
    }

    Index prev_job_rank = 0;
    if (prev_step.job_type.value() == JOB_TYPE::SINGLE) {
      if (!input.job_id_to_rank.contains(prev_step.id)) {
        continue;
      }
      prev_job_rank = input.job_id_to_rank.at(prev_step.id);
    } else if (prev_step.job_type.value() == JOB_TYPE::PICKUP) {
      if (!input.pickup_id_to_rank.contains(prev_step.id)) {
        continue;
      }
      prev_job_rank = input.pickup_id_to_rank.at(prev_step.id);
    } else if (prev_step.job_type.value() == JOB_TYPE::DELIVERY) {
      if (!input.delivery_id_to_rank.contains(prev_step.id)) {
        continue;
      }
      prev_job_rank = input.delivery_id_to_rank.at(prev_step.id);
    }

    // Check if this predecessor is in the route
    if (!is_job_in_route(route.route, prev_job_rank)) {
      return false;  // Predecessor not in route yet
    }
  }

  return true;  // All predecessors are in route
}

// Add seed job to route if required and return current cost of route
// without vehicle fixed cost.
template <class Route>
inline void seed_route(const Input& input,
                       Route& route,
                       INIT init,
                       const std::vector<std::vector<Eval>>& evals,
                       std::set<Index>& unassigned,
                       auto job_not_ok) {
  assert(route.empty() && init != INIT::NONE);

  const auto v_rank = route.v_rank;
  const auto& vehicle = input.vehicles[v_rank];

  // Initialize current route with the "best" valid job.
  bool init_ok = false;

  Amount higher_amount(input.zero_amount());
  Cost furthest_cost = 0;
  Cost nearest_cost = std::numeric_limits<Cost>::max();
  Duration earliest_deadline = std::numeric_limits<Duration>::max();
  Index best_job_rank = 0;
  for (const auto job_rank : unassigned) {
    const auto& current_job = input.jobs[job_rank];

    if (!input.vehicle_ok_with_job(v_rank, job_rank) ||
        current_job.type == JOB_TYPE::DELIVERY || job_not_ok(job_rank)) {
      continue;
    }

    const bool is_pickup = (current_job.type == JOB_TYPE::PICKUP);

    // Skip pickups that are in a relation at position > 0
    // (they can only be inserted after their relation predecessor)
    if (is_pickup && input.job_rank_to_relation.contains(job_rank)) {
      Index position = input.job_rank_to_relation_position.at(job_rank);
      if (position > 0) {
        continue;  // Not first in relation, can't be a seed job
      }
    }

    if (route.size() + (is_pickup ? 2 : 1) > vehicle.max_tasks) {
      continue;
    }

    bool try_validity = false;

    if (init == INIT::HIGHER_AMOUNT) {
      try_validity = (higher_amount < current_job.pickup ||
                      higher_amount < current_job.delivery);
    }
    if (init == INIT::EARLIEST_DEADLINE) {
      const Duration current_deadline =
        is_pickup ? input.jobs[job_rank + 1].tws.back().end
                  : current_job.tws.back().end;
      try_validity = (current_deadline < earliest_deadline);
    }
    if (init == INIT::FURTHEST) {
      try_validity = (furthest_cost < evals[job_rank][v_rank].cost);
    }
    if (init == INIT::NEAREST) {
      try_validity = (evals[job_rank][v_rank].cost < nearest_cost);
    }

    if (!try_validity) {
      continue;
    }

    bool is_valid = false;
    try {
      is_valid = (vehicle.ok_for_range_bounds(evals[job_rank][v_rank])) &&
                 route.is_valid_addition_for_capacity(input,
                                                      current_job.pickup,
                                                      current_job.delivery,
                                                      0);
    } catch (const InfeasibleRouteException&) {
      // Route state issue or capacity infeasible - skip this job
      is_valid = false;
    }
    if (is_pickup) {
      std::vector<Index> p_d({job_rank, static_cast<Index>(job_rank + 1)});
      is_valid = is_valid && route.is_valid_addition_for_tw(input,
                                                            input.zero_amount(),
                                                            p_d.begin(),
                                                            p_d.end(),
                                                            0,
                                                            0);
    } else {
      assert(current_job.type == JOB_TYPE::SINGLE);
      is_valid = is_valid && route.is_valid_addition_for_tw(input, job_rank, 0);
    }

    if (is_valid) {
      init_ok = true;
      best_job_rank = job_rank;

      switch (init) {
        using enum INIT;
      case NONE:
        assert(false);
        break;
      case HIGHER_AMOUNT:
        if (higher_amount < current_job.pickup) {
          higher_amount = current_job.pickup;
        }
        if (higher_amount < current_job.delivery) {
          higher_amount = current_job.delivery;
        }
        break;
      case EARLIEST_DEADLINE:
        earliest_deadline = is_pickup ? input.jobs[job_rank + 1].tws.back().end
                                      : current_job.tws.back().end;
        break;
      case FURTHEST:
        furthest_cost = evals[job_rank][v_rank].cost;
        break;
      case NEAREST:
        nearest_cost = evals[job_rank][v_rank].cost;
        break;
      }
    }
  }

  if (init_ok) {
    try {
      if (input.jobs[best_job_rank].type == JOB_TYPE::SINGLE) {
        route.add(input, best_job_rank, 0);
        unassigned.erase(best_job_rank);
      }
      if (input.jobs[best_job_rank].type == JOB_TYPE::PICKUP) {
        std::vector<Index> p_d(
          {best_job_rank, static_cast<Index>(best_job_rank + 1)});
        route.replace(input, input.zero_amount(), p_d.begin(), p_d.end(), 0, 0);
        unassigned.erase(best_job_rank);
        unassigned.erase(best_job_rank + 1);
      }
    } catch (const std::exception&) {
      // Job cannot fit (time windows, capacity, or other constraints), leave unassigned
    }
  }
}

template <class Route> struct UnassignedCosts {
  const Vehicle& vehicle;
  Cost max_edge_cost;
  std::vector<Cost> min_route_to_unassigned;
  std::vector<Cost> min_unassigned_to_route;

  UnassignedCosts(const Input& input,
                  Route& route,
                  const std::set<Index>& unassigned)
    : vehicle(input.vehicles[route.v_rank]),
      max_edge_cost(utils::max_edge_eval(input, vehicle, route.route).cost),
      min_route_to_unassigned(input.jobs.size(),
                              std::numeric_limits<Cost>::max()),
      min_unassigned_to_route(input.jobs.size(),
                              std::numeric_limits<Cost>::max()) {
    for (const auto job_rank : unassigned) {
      const auto& unassigned_job = input.jobs[job_rank];
      const auto unassigned_job_index = unassigned_job.index();

      // The purpose here is to generate insertion lower bounds so we
      // only account for service times (no setup) which are
      // independent of insertion rank.
      const auto added_service = unassigned_job.services[vehicle.type];
      const auto service_cost = vehicle.task_eval(added_service).cost;

      if (vehicle.has_start()) {
        const auto start_to_job =
          vehicle.eval(vehicle.start.value().index(), unassigned_job_index)
            .cost;
        min_route_to_unassigned[job_rank] = start_to_job + service_cost;
      }

      if (vehicle.has_end()) {
        const auto job_to_end =
          vehicle.eval(unassigned_job_index, vehicle.end.value().index()).cost;
        min_unassigned_to_route[job_rank] = job_to_end + service_cost;
      }

      for (const auto j : route.route) {
        const auto job_index = input.jobs[j].index();

        const auto job_to_unassigned =
          vehicle.eval(job_index, unassigned_job_index).cost + service_cost;
        min_route_to_unassigned[job_rank] =
          std::min(min_route_to_unassigned[job_rank], job_to_unassigned);

        const auto unassigned_to_job =
          vehicle.eval(unassigned_job_index, job_index).cost + service_cost;
        min_unassigned_to_route[job_rank] =
          std::min(min_unassigned_to_route[job_rank], unassigned_to_job);
      }
    }
  }

  double get_insertion_lower_bound(Index j) {
    return static_cast<double>(min_route_to_unassigned[j] +
                               min_unassigned_to_route[j] - max_edge_cost);
  }

  double get_pd_insertion_lower_bound(const Input& input, Index p) {
    assert(input.jobs[p].type == JOB_TYPE::PICKUP);

    // Situation where pickup and delivery are not inserted in a row.
    const auto apart_insertion = static_cast<double>(
      min_route_to_unassigned[p] + min_unassigned_to_route[p] +
      min_route_to_unassigned[p + 1] + min_unassigned_to_route[p + 1] -
      2 * max_edge_cost);

    // Situation where delivery is inserted next to the pickup.
    const auto next_insertion = static_cast<double>(
      min_route_to_unassigned[p] + min_unassigned_to_route[p + 1] +
      vehicle.eval(input.jobs[p].index(), input.jobs[p + 1].index()).cost -
      max_edge_cost);

    return std::min(apart_insertion, next_insertion);
  }

  void update_max_edge(const Input& input, Route& route) {
    max_edge_cost = utils::max_edge_eval(input, vehicle, route.route).cost;
  }

  void update_min_costs(const Input& input,
                        const std::set<Index>& unassigned,
                        Index inserted_index) {
    for (const auto j : unassigned) {
      const auto& unassigned_job = input.jobs[j];
      const auto unassigned_job_index = unassigned_job.index();

      const auto added_service = unassigned_job.services[vehicle.type];
      const auto service_cost = vehicle.task_eval(added_service).cost;

      const auto to_unassigned =
        vehicle.eval(inserted_index, unassigned_job_index).cost + service_cost;
      min_route_to_unassigned[j] =
        std::min(min_route_to_unassigned[j], to_unassigned);

      const auto from_unassigned =
        vehicle.eval(unassigned_job_index, inserted_index).cost + service_cost;
      min_unassigned_to_route[j] =
        std::min(min_unassigned_to_route[j], from_unassigned);
    }
  }
};

// Pre-assign complete relations to vehicles with priority
template <class Route>
void assign_relations_to_routes(const Input& input,
                                std::vector<Route>& routes,
                                std::set<Index>& unassigned) {
  // Try to assign each relation to a vehicle
  for (const auto& relation : input.relations) {
    // Skip empty relations
    if (relation.pickup_ranks.empty()) {
      continue;
    }

    // Check if all shipments in this relation are still unassigned
    bool all_unassigned = true;
    for (Index pickup_rank : relation.pickup_ranks) {
      if (!unassigned.contains(pickup_rank)) {
        all_unassigned = false;
        break;
      }
    }

    if (!all_unassigned) {
      continue; // Some already assigned, skip
    }

    // Find a vehicle that can accommodate all shipments
    for (auto& route : routes) {
      const auto& vehicle = input.vehicles[route.v_rank];

      // Check capacity: each shipment = 2 tasks (pickup + delivery)
      Index total_tasks = relation.pickup_ranks.size() * 2;
      if (route.size() + total_tasks > vehicle.max_tasks) {
        continue; // Not enough room
      }

      // Check if vehicle is compatible with all shipments
      bool all_compatible = true;
      for (Index pickup_rank : relation.pickup_ranks) {
        if (!input.vehicle_ok_with_job(route.v_rank, pickup_rank)) {
          all_compatible = false;
          break;
        }
      }

      if (!all_compatible) {
        continue;
      }

      // Check if the relation shipments can be inserted at the end
      // First, build the shipments vector
      std::vector<Index> shipments_to_add;
      for (size_t i = 0; i < relation.pickup_ranks.size(); ++i) {
        shipments_to_add.push_back(relation.pickup_ranks[i]);     // pickup
        shipments_to_add.push_back(relation.delivery_ranks[i]);   // delivery
      }

      // Validate insertion at the end
      Index insert_pos = route.size();
      bool can_add = false;
      try {
        const Amount delivery_delta = input.zero_amount();
        can_add =
          route.is_valid_addition_for_capacity_inclusion(
            input,
            delivery_delta,
            shipments_to_add.begin(),
            shipments_to_add.end(),
            insert_pos,
            insert_pos) &&
          route.is_valid_addition_for_tw(input,
                                         delivery_delta,
                                         shipments_to_add.begin(),
                                         shipments_to_add.end(),
                                         insert_pos,
                                         insert_pos);
      } catch (const InfeasibleRouteException&) {
        can_add = false;
      }

      if (!can_add) {
        continue;  // Can't insert on this vehicle
      }

      // Use replace() instead of set_route() to properly handle TWRoute arrays
      std::optional<Route> route_backup = route;
      try {
        route.replace(input,
                      input.zero_amount(),
                      shipments_to_add.begin(),
                      shipments_to_add.end(),
                      insert_pos,
                      insert_pos);

        // Only remove from unassigned if insertion succeeded
        for (size_t i = 0; i < relation.pickup_ranks.size(); ++i) {
          unassigned.erase(relation.pickup_ranks[i]);
          unassigned.erase(relation.delivery_ranks[i]);
        }

        break; // Move to next relation
      } catch (const InfeasibleRouteException&) {
        // Relation cannot fit due to time window constraints, leave unassigned
        // Restore route if it was partially modified.
        route = std::move(*route_backup);
        continue; // Try next vehicle
      }
    }
  }
}

template <class Route>
inline Eval fill_route(const Input& input,
                       [[maybe_unused]] std::vector<Route>& all_routes,
                       Route& route,
                       std::set<Index>& unassigned,
                       const std::vector<Cost>& regrets,
                       double lambda) {
  const auto v_rank = route.v_rank;
  const auto& vehicle = input.vehicles[v_rank];

  const bool init_route_is_empty = route.empty();
  Eval route_eval = utils::route_eval_for_vehicle(input, v_rank, route.route);

  // Store bounds to be able to cut out some loops.
  UnassignedCosts unassigned_costs(input, route, unassigned);

  bool keep_going = true;
  while (keep_going) {
    keep_going = false;
    double best_cost = std::numeric_limits<double>::max();
    Index best_job_rank = 0;
    Index best_r = 0;
    Index best_pickup_r = 0;
    Index best_delivery_r = 0;
    Amount best_modified_delivery = input.zero_amount();
    Eval best_eval;

    for (const auto job_rank : unassigned) {
      if (!input.vehicle_ok_with_job(v_rank, job_rank)) {
        continue;
      }

      // Skip if this job is fixed to a specific vehicle (from vehicle.steps)
      // AND it's not the current vehicle
      if (input.fixed_job_ranks.contains(job_rank)) {
        if (input.fixed_job_to_vehicle.contains(job_rank) &&
            input.fixed_job_to_vehicle.at(job_rank) != v_rank) {
          continue;
        }

        // NEW: Check if this fixed job can be inserted now (predecessors in route)
        if (!can_insert_fixed_job(input, route, job_rank, v_rank)) {
          continue;  // Predecessor steps not yet in route, skip for now
        }
      }

      const auto& current_job = input.jobs[job_rank];

      if (current_job.type == JOB_TYPE::DELIVERY) {
        continue;
      }

      if (current_job.type == JOB_TYPE::SINGLE &&
          route.size() + 1 <= vehicle.max_tasks) {

        if (best_cost < unassigned_costs.get_insertion_lower_bound(job_rank) -
                          lambda * static_cast<double>(regrets[job_rank])) {
          // Bypass going through whole route if we're sure insertion
          // cost is not good enough.
          continue;
        }

        for (Index r = 0; r <= route.size(); ++r) {
          // NEW: Skip if inserting here would break shipment atomicity
          if (would_break_shipment_atomicity(input, route, r)) {
            continue;
          }

          // NEW: Skip if inserting here would break consecutive vehicle steps
          // Vehicle steps must remain consecutive with nothing in between
          if (would_break_consecutive_steps(input, route, r, v_rank)) {
            continue;
          }

          // NEW: Skip if inserting here would break a relation sequence
          // Check if the job before position r is part of a relation and requires a specific next job
          if (r > 0 && r < route.route.size()) {
            Index prev_job = route.route[r - 1];
            if (input.job_rank_to_relation.contains(prev_job)) {
              Index rel_idx = input.job_rank_to_relation.at(prev_job);
              Index pos_in_rel = input.job_rank_to_relation_position.at(prev_job);
              const auto& relation = input.relations[rel_idx];

              // Check if prev_job requires a specific next job
              Index expected_next = std::numeric_limits<Index>::max();
              if (pos_in_rel < relation.pickup_ranks.size()) {
                // Previous job is a pickup, next should be its delivery
                expected_next = relation.delivery_ranks[pos_in_rel];
              } else if (pos_in_rel < relation.pickup_ranks.size() - 1) {
                // Previous job is a delivery (but not the last), next should be next pickup
                expected_next = relation.pickup_ranks[pos_in_rel + 1];
              }

              if (expected_next != std::numeric_limits<Index>::max() &&
                  r < route.route.size() && route.route[r] != expected_next) {
                // The next job is not what the relation expects, but it's also not the expected job
                // This means inserting here would break the relation
                continue;
              }
            }
          }

          const auto current_eval =
            utils::addition_eval(input, job_rank, vehicle, route.route, r);

          const double current_cost =
            static_cast<double>(current_eval.cost) -
            lambda * static_cast<double>(regrets[job_rank]);

          try {
            if (current_cost < best_cost &&
                (vehicle.ok_for_range_bounds(route_eval + current_eval)) &&
                route.is_valid_addition_for_capacity(input,
                                                     current_job.pickup,
                                                     current_job.delivery,
                                                     r) &&
                route.is_valid_addition_for_tw(input, job_rank, r)) {
              best_cost = current_cost;
              best_job_rank = job_rank;
              best_r = r;
              best_eval = current_eval;
            }
          } catch (const InfeasibleRouteException&) {
            // Route state issue or infeasible - skip this position
            continue;
          }
        }
      }

      if (current_job.type == JOB_TYPE::PICKUP &&
          route.size() + 2 <= vehicle.max_tasks) {

        // No skipping - let all shipments be evaluated normally
        // Priority boost ensures first shipments are inserted first
        // Constraint checking below will prevent sequence violations

        if (best_cost <
            unassigned_costs.get_pd_insertion_lower_bound(input, job_rank) -
              lambda * static_cast<double>(regrets[job_rank])) {
          // Bypass going through whole route if we're sure insertion
          // cost is not good enough.
          continue;
        }

        // Pre-compute cost of addition for matching delivery.
        std::vector<Eval> d_adds(route.route.size() + 1);
        std::vector<unsigned char> valid_delivery_insertions(
          route.route.size() + 1);

        for (unsigned d_rank = 0; d_rank <= route.route.size(); ++d_rank) {
          d_adds[d_rank] = utils::addition_eval(input,
                                                job_rank + 1,
                                                vehicle,
                                                route.route,
                                                d_rank);
          valid_delivery_insertions[d_rank] =
            route.is_valid_addition_for_tw_without_max_load(input,
                                                            job_rank + 1,
                                                            d_rank);
        }

        for (Index pickup_r = 0; pickup_r <= route.size(); ++pickup_r) {
          // NEW: Skip if inserting pickup here would break shipment atomicity
          if (would_break_shipment_atomicity(input, route, pickup_r)) {
            continue;
          }

          // NEW: Skip if inserting here would break consecutive vehicle steps
          if (would_break_consecutive_steps(input, route, pickup_r, v_rank)) {
            continue;
          }

          try {
            if (!route.is_valid_addition_for_load(input,
                                                  current_job.pickup,
                                                  pickup_r) ||
                !route.is_valid_addition_for_tw_without_max_load(input,
                                                                 job_rank,
                                                                 pickup_r)) {
              continue;
            }
          } catch (const InfeasibleRouteException&) {
            continue;
          }

          // Check relation constraint: non-first shipments must follow previous delivery
          if (input.job_rank_to_relation.contains(job_rank)) {
            Index position = input.job_rank_to_relation_position.at(job_rank);
            if (position > 0) {
              // This is not the first shipment in the relation
              Index relation_idx = input.job_rank_to_relation.at(job_rank);
              const auto& relation = input.relations[relation_idx];
              Index prev_delivery = relation.delivery_ranks[position - 1];

              // Check if previous delivery is immediately before this insertion point
              if (pickup_r == 0 || route.route[pickup_r - 1] != prev_delivery) {
                // Previous delivery not immediately before - skip this position
                continue;
              }
            }
          }

          // Build replacement sequence for current insertion.
          std::vector<Index> modified_with_pd;
          modified_with_pd.reserve(route.size() - pickup_r + 2);
          modified_with_pd.push_back(job_rank);

          Amount modified_delivery = input.zero_amount();

          // HARD CONSTRAINT: Shipments MUST be atomic (pickup immediately followed by delivery)
          // For NEMT use case, we cannot have any jobs between pickup and dropoff
          // Only try atomic placement (delivery_r == pickup_r)
          Index delivery_r = pickup_r;

          if (static_cast<bool>(valid_delivery_insertions[delivery_r])) {
            Eval current_eval = utils::addition_eval(input,
                                                    job_rank,
                                                    vehicle,
                                                    route.route,
                                                    pickup_r,
                                                    pickup_r + 1);

            const double current_cost =
              current_eval.cost -
              lambda * static_cast<double>(regrets[job_rank]);

            if (current_cost < best_cost) {
              modified_with_pd.push_back(job_rank + 1);

              try {
                // Validate atomic placement
                const bool valid =
                  (vehicle.ok_for_range_bounds(route_eval + current_eval)) &&
                  route
                    .is_valid_addition_for_capacity_inclusion(input,
                                                              input.zero_amount(),
                                                              modified_with_pd
                                                                .begin(),
                                                              modified_with_pd
                                                                .end(),
                                                              pickup_r,
                                                              delivery_r) &&
                  route.is_valid_addition_for_tw(input,
                                                 input.zero_amount(),
                                                 modified_with_pd.begin(),
                                                 modified_with_pd.end(),
                                                 pickup_r,
                                                 delivery_r);

                if (valid) {
                  best_cost = current_cost;
                  best_job_rank = job_rank;
                  best_pickup_r = pickup_r;
                  best_delivery_r = delivery_r;
                  best_modified_delivery = input.zero_amount();
                  best_eval = current_eval;
                }
              } catch (const InfeasibleRouteException&) {
                // Route state issue or infeasible - skip this placement
              }

              modified_with_pd.pop_back();
            }
          }
          // If atomic placement is not valid, this shipment will remain unassigned
          // This ensures ALL assigned shipments are atomic
        }
      }
    }

    if (best_cost < std::numeric_limits<double>::max()) {
      // Relations are pre-assigned, so we only handle regular shipments here
      try {
        bool insertion_ok = false;
        std::optional<Eval> updated_route_eval;

        // Handle single job insertion
        const auto& best_job = input.jobs[best_job_rank];
        if (best_job.type == JOB_TYPE::SINGLE) {
          route.add(input, best_job_rank, best_r);
          unassigned.erase(best_job_rank);
          keep_going = true;
          insertion_ok = true;
        }

        if (best_job.type == JOB_TYPE::PICKUP) {
          bool can_insert = true;
          bool relation_first = false;
          const Relation* relation_ptr = nullptr;
          std::vector<Index> relation_jobs;
          if (input.job_rank_to_relation.contains(best_job_rank)) {
            Index relation_idx = input.job_rank_to_relation.at(best_job_rank);
            Index position = input.job_rank_to_relation_position.at(best_job_rank);
            const auto& relation = input.relations[relation_idx];
            relation_ptr = &relation;

            if (position == 0 && relation.pickup_ranks.size() > 1) {
              relation_first = true;

              // All shipments in relation must be unassigned.
              for (size_t i = 0; i < relation.pickup_ranks.size(); ++i) {
                if (unassigned.find(relation.pickup_ranks[i]) == unassigned.end()) {
                  can_insert = false;
                  break;
                }
              }

              if (can_insert) {
                // Check if there's enough space in the route for ALL relation shipments
                size_t total_relation_jobs = 2 * relation.pickup_ranks.size();
                if (route.size() + total_relation_jobs > vehicle.max_tasks) {
                  can_insert = false;
                }
              }

              if (can_insert) {
                relation_jobs.reserve(2 * relation.pickup_ranks.size());
                for (size_t i = 0; i < relation.pickup_ranks.size(); ++i) {
                  relation_jobs.push_back(relation.pickup_ranks[i]);
                  relation_jobs.push_back(relation.delivery_ranks[i]);
                }
              }
            }
          }

          if (can_insert && relation_first && relation_ptr != nullptr) {
            bool valid = false;
            try {
              const Amount delivery_delta = input.zero_amount();
              Index insert_pos = best_pickup_r;

              valid =
                route.is_valid_addition_for_capacity_inclusion(
                  input,
                  delivery_delta,
                  relation_jobs.begin(),
                  relation_jobs.end(),
                  insert_pos,
                  insert_pos) &&
                route.is_valid_addition_for_tw(input,
                                               delivery_delta,
                                               relation_jobs.begin(),
                                               relation_jobs.end(),
                                               insert_pos,
                                               insert_pos);

              if (valid) {
                auto candidate_route = route.route;
                candidate_route.insert(candidate_route.begin() + insert_pos,
                                       relation_jobs.begin(),
                                       relation_jobs.end());
                Eval new_eval =
                  utils::route_eval_for_vehicle(input, v_rank, candidate_route);
                if (vehicle.ok_for_range_bounds(new_eval)) {
                  route.replace(input,
                                delivery_delta,
                                relation_jobs.begin(),
                                relation_jobs.end(),
                                insert_pos,
                                insert_pos);
                  for (Index job_rank : relation_jobs) {
                    unassigned.erase(job_rank);
                  }
                  keep_going = true;
                  insertion_ok = true;
                  updated_route_eval = new_eval;
                }
              }
            } catch (const InfeasibleRouteException&) {
              valid = false;
            }

            if (!valid) {
              // Leave relation unassigned.
              can_insert = false;
            }
          }

          if (can_insert && !relation_first) {
            std::vector<Index> modified_with_pd;
            modified_with_pd.reserve(best_delivery_r - best_pickup_r + 2);
            modified_with_pd.push_back(best_job_rank);

            std::copy(route.route.begin() + best_pickup_r,
                      route.route.begin() + best_delivery_r,
                      std::back_inserter(modified_with_pd));
            modified_with_pd.push_back(best_job_rank + 1);

            route.replace(input,
                          best_modified_delivery,
                          modified_with_pd.begin(),
                          modified_with_pd.end(),
                          best_pickup_r,
                          best_delivery_r);

            unassigned.erase(best_job_rank);
            unassigned.erase(best_job_rank + 1);
            keep_going = true;
            insertion_ok = true;
          }

        }

        if (insertion_ok) {
          unassigned_costs.update_max_edge(input, route);
          unassigned_costs.update_min_costs(input, unassigned, best_job.index());
          if (best_job.type == JOB_TYPE::PICKUP) {
            unassigned_costs
              .update_min_costs(input,
                                unassigned,
                                input.jobs[best_job_rank + 1].index());
          }

          if (updated_route_eval.has_value()) {
            route_eval = *updated_route_eval;
          } else {
            route_eval += best_eval;
          }
        }
      } catch (const InfeasibleRouteException&) {
        // Job cannot fit due to time window constraints, leave unassigned
      }
    }
  }

  if (init_route_is_empty && !route.empty()) {
    // Account for fixed cost if we actually filled an empty route.
    route_eval.cost += vehicle.fixed_cost();
  }

  return route_eval;
}

template <class Route>
Eval basic(const Input& input,
           std::vector<Route>& routes,
           std::set<Index> unassigned,
           std::vector<Index> vehicles_ranks,
           INIT init,
           double lambda,
           SORT sort) {
  // Ordering is based on vehicles description only so do not account
  // for initial routes if any.
  const auto nb_vehicles = vehicles_ranks.size();

  switch (sort) {
  case SORT::AVAILABILITY: {
    // Sort vehicles by decreasing "availability".
    std::ranges::stable_sort(vehicles_ranks,
                             [&](const auto lhs, const auto rhs) {
                               return input.vehicles[lhs] < input.vehicles[rhs];
                             });
    break;
  }
  case SORT::COST:
    // Sort vehicles by increasing fixed cost, then same as above.
    std::ranges::stable_sort(vehicles_ranks,
                             [&](const auto lhs, const auto rhs) {
                               const auto& v_lhs = input.vehicles[lhs];
                               const auto& v_rhs = input.vehicles[rhs];
                               return v_lhs.costs < v_rhs.costs ||
                                      (v_lhs.costs == v_rhs.costs &&
                                       input.vehicles[lhs] <
                                         input.vehicles[rhs]);
                             });
    break;
  }

  const auto& evals = input.jobs_vehicles_evals();

  // Priority boost for relation shipments: set very low regret to ensure early insertion
  auto boost_relation_regrets = [&input](std::vector<Cost>& regret_vector) {
    for (const auto& relation : input.relations) {
      // Only boost the FIRST shipment in each relation
      // Others will be skipped in fill_route and handled separately
      if (!relation.pickup_ranks.empty()) {
        Index first_pickup = relation.pickup_ranks[0];
        regret_vector[first_pickup] = 0; // Zero regret = highest priority
      }
    }
  };

  // regrets[v][j] holds the min cost for reaching job j in an empty
  // route across all remaining vehicles **after** vehicle at rank v
  // in vehicles_ranks. Regrets are only computed for available
  // vehicles and unassigned jobs, but are based on empty routes
  // evaluations so do not account for initial routes if any.
  std::vector<std::vector<Cost>> regrets(nb_vehicles,
                                         std::vector<Cost>(input.jobs.size()));

  // Use own cost for last vehicle regret values.
  for (const auto j : unassigned) {
    regrets.back()[j] = evals[j][vehicles_ranks.back()].cost;
  }

  for (Index rev_v = 0; rev_v < nb_vehicles - 1; ++rev_v) {
    // Going trough vehicles backward from second to last.
    const auto v = nb_vehicles - 2 - rev_v;

    bool all_compatible_jobs_later_undoable = true;
    for (const auto j : unassigned) {
      regrets[v][j] =
        std::min(regrets[v + 1][j], (evals[j][vehicles_ranks[v + 1]]).cost);
      if (input.vehicle_ok_with_job(vehicles_ranks[v], j) &&
          regrets[v][j] < input.get_cost_upper_bound()) {
        all_compatible_jobs_later_undoable = false;
      }
    }

    if (all_compatible_jobs_later_undoable) {
      // We don't want to use all regrets equal to the cost upper
      // bound in this situation: it would defeat the purpose of using
      // regrets in the first place as all lambda values would yield
      // the same choices. Using the same approach as with last
      // vehicle.
      for (const auto j : unassigned) {
        regrets[v][j] = evals[j][vehicles_ranks[v]].cost;
      }
    }
  }

  // Apply priority boost to relation shipments in all regret vectors
  for (Index v = 0; v < nb_vehicles; ++v) {
    boost_relation_regrets(regrets[v]);
  }

  Eval sol_eval;

  // PRIORITY: Assign complete relations first before filling with other shipments
  if (!input.relations.empty()) {
    assign_relations_to_routes(input, routes, unassigned);
  }

  for (Index v = 0; v < nb_vehicles && !unassigned.empty(); ++v) {
    auto v_rank = vehicles_ranks[v];
    auto& current_r = routes[v_rank];

    if (current_r.empty() && init != INIT::NONE) {
      // Trivial lambda for no additional job validity constraint.
      constexpr auto job_not_ok = [](const Index) { return false; };

      seed_route(input, current_r, init, evals, unassigned, job_not_ok);
    }

    const auto current_eval =
      fill_route(input, routes, current_r, unassigned, regrets[v], lambda);
    sol_eval += current_eval;
  }

  // Note: Incomplete relations are prevented during construction by checking
  // all shipments can be inserted before committing the first one.

  return sol_eval;
}

template <class Route>
Eval dynamic_vehicle_choice(const Input& input,
                            std::vector<Route>& routes,
                            std::set<Index> unassigned,
                            std::vector<Index> vehicles_ranks,
                            INIT init,
                            double lambda,
                            SORT sort) {
  const auto& evals = input.jobs_vehicles_evals();

  Eval sol_eval;

  // PRIORITY: Assign complete relations first before filling with other shipments
  if (!input.relations.empty()) {
    assign_relations_to_routes(input, routes, unassigned);
  }

  while (!vehicles_ranks.empty() && !unassigned.empty()) {
    // For any unassigned job at j, jobs_min_costs[j]
    // (resp. jobs_second_min_costs[j]) holds the min cost
    // (resp. second min cost) of picking the job in an empty route
    // for any remaining vehicle. Evaluation are based on empty routes
    // so do not account for initial routes if any.
    std::vector<Cost> jobs_min_costs(input.jobs.size(),
                                     input.get_cost_upper_bound());
    std::vector<Cost> jobs_second_min_costs(input.jobs.size(),
                                            input.get_cost_upper_bound());
    for (const auto j : unassigned) {
      for (const auto v : vehicles_ranks) {
        if (evals[j][v].cost <= jobs_min_costs[j]) {
          jobs_second_min_costs[j] = jobs_min_costs[j];
          jobs_min_costs[j] = evals[j][v].cost;
        } else {
          if (evals[j][v].cost < jobs_second_min_costs[j]) {
            jobs_second_min_costs[j] = evals[j][v].cost;
          }
        }
      }
    }

    // Pick vehicle that has the biggest number of compatible
    // unassigned jobs closest to him than to any other different
    // vehicle still available.
    std::vector<unsigned> closest_jobs_count(input.vehicles.size(), 0);
    for (const auto j : unassigned) {
      for (const auto v : vehicles_ranks) {
        if (evals[j][v].cost == jobs_min_costs[j]) {
          ++closest_jobs_count[v];
        }
      }
    }

    Index v_rank;

    if (sort == SORT::AVAILABILITY) {
      const auto chosen_vehicle =
        std::ranges::min_element(vehicles_ranks,
                                 [&](const auto lhs, const auto rhs) {
                                   return closest_jobs_count[lhs] >
                                            closest_jobs_count[rhs] ||
                                          (closest_jobs_count[lhs] ==
                                             closest_jobs_count[rhs] &&
                                           input.vehicles[lhs] <
                                             input.vehicles[rhs]);
                                 });
      v_rank = *chosen_vehicle;
      vehicles_ranks.erase(chosen_vehicle);
    } else {
      assert(sort == SORT::COST);

      const auto chosen_vehicle =
        std::ranges::min_element(vehicles_ranks,
                                 [&](const auto lhs, const auto rhs) {
                                   const auto& v_lhs = input.vehicles[lhs];
                                   const auto& v_rhs = input.vehicles[rhs];
                                   return closest_jobs_count[lhs] >
                                            closest_jobs_count[rhs] ||
                                          (closest_jobs_count[lhs] ==
                                             closest_jobs_count[rhs] &&
                                           (v_lhs.costs < v_rhs.costs ||
                                            (v_lhs.costs == v_rhs.costs &&
                                             v_lhs < v_rhs)));
                                 });
      v_rank = *chosen_vehicle;
      vehicles_ranks.erase(chosen_vehicle);
    }

    // Once current vehicle is decided, then for any unassigned job at
    // j, regrets[j] holds the min cost of picking the job in an empty
    // route for other remaining vehicles. Regrets are only computed
    // for available vehicles and unassigned jobs, but are based on
    // empty routes evaluations so do not account for initial routes
    // if any.
    std::vector<Cost> regrets(input.jobs.size(), input.get_cost_upper_bound());

    bool all_compatible_jobs_later_undoable = true;
    for (const auto j : unassigned) {
      if (jobs_min_costs[j] < evals[j][v_rank].cost) {
        regrets[j] = jobs_min_costs[j];
      } else {
        regrets[j] = jobs_second_min_costs[j];
      }

      if (input.vehicle_ok_with_job(v_rank, j) &&
          regrets[j] < input.get_cost_upper_bound()) {
        all_compatible_jobs_later_undoable = false;
      }
    }

    if (all_compatible_jobs_later_undoable) {
      // Same approach as for basic heuristic.
      for (const auto j : unassigned) {
        regrets[j] = evals[j][v_rank].cost;
      }
    }

    // Apply priority boost to relation shipments
    for (const auto& relation : input.relations) {
      if (!relation.pickup_ranks.empty()) {
        Index first_pickup = relation.pickup_ranks[0];
        regrets[first_pickup] = 0; // Zero regret = highest priority
      }
    }

    auto& current_r = routes[v_rank];

    if (current_r.empty() && init != INIT::NONE) {
      auto job_not_ok =
        [&jobs_min_costs, &evals, v_rank](const Index job_rank) {
          // One of the remaining vehicles is closest to that job.
          return jobs_min_costs[job_rank] < evals[job_rank][v_rank].cost;
        };

      seed_route(input, current_r, init, evals, unassigned, job_not_ok);
    }

    const auto current_eval =
      fill_route(input, routes, current_r, unassigned, regrets, lambda);
    sol_eval += current_eval;
  }

  // Note: Incomplete relations are prevented during construction by checking
  // all shipments can be inserted before committing the first one.

  return sol_eval;
}

template <class Route>
void set_route(const Input& input,
               Route& route,
               std::unordered_set<Index>& assigned) {
  assert(route.empty());
  const auto& vehicle = input.vehicles[route.v_rank];

  // Startup load is the sum of deliveries for (single) jobs.
  Amount single_jobs_deliveries(input.zero_amount());
  for (const auto& step : vehicle.steps) {
    if (step.type == STEP_TYPE::JOB) {
      assert(step.job_type.has_value());

      if (step.job_type.value() == JOB_TYPE::SINGLE) {
        single_jobs_deliveries += input.jobs[step.rank].delivery;
      }
    }
  }
  if (!(single_jobs_deliveries <= vehicle.capacity)) {
    throw InputException(
      std::format("Route over capacity for vehicle {}.", vehicle.id));
  }

  // Track load and travel time during the route for validity.
  Amount current_load = single_jobs_deliveries;
  Eval eval_sum;
  std::optional<Index> previous_index;
  if (vehicle.has_start()) {
    previous_index = vehicle.start.value().index();
  }

  std::vector<Index> job_ranks;
  job_ranks.reserve(vehicle.steps.size());
  std::unordered_set<Index> expected_delivery_ranks;
  for (const auto& step : vehicle.steps) {
    if (step.type != STEP_TYPE::JOB) {
      continue;
    }

    const auto job_rank = step.rank;
    const auto& job = input.jobs[job_rank];
    job_ranks.push_back(job_rank);

    assert(!assigned.contains(job_rank));
    assigned.insert(job_rank);

    if (!input.vehicle_ok_with_job(route.v_rank, job_rank)) {
      throw InputException(
        std::format("Missing skill or step out of reach for vehicle {} and "
                    "job {}.",
                    vehicle.id,
                    job.id));
    }

    // Update current travel time.
    if (previous_index.has_value()) {
      eval_sum += vehicle.eval(previous_index.value(), job.index());
    }
    previous_index = job.index();

    // Handle load.
    assert(step.job_type.has_value());
    switch (step.job_type.value()) {
    case JOB_TYPE::SINGLE: {
      current_load += job.pickup;
      current_load -= job.delivery;
      break;
    }
    case JOB_TYPE::PICKUP: {
      expected_delivery_ranks.insert(job_rank + 1);

      current_load += job.pickup;
      break;
    }
    case JOB_TYPE::DELIVERY: {
      auto search = expected_delivery_ranks.find(job_rank);
      if (search == expected_delivery_ranks.end()) {
        throw InputException(
          std::format("Invalid shipment in route for vehicle {}.", vehicle.id));
      }
      expected_delivery_ranks.erase(search);

      current_load -= job.delivery;
      break;
    }
    default:
      assert(false);
    }

    // Check validity after this step wrt capacity.
    if (!(current_load <= vehicle.capacity)) {
      throw InputException(
        std::format("Route over capacity for vehicle {}.", vehicle.id));
    }
  }

  if (vehicle.has_end() && !job_ranks.empty()) {
    // Update with last route leg.
    assert(previous_index.has_value());
    eval_sum +=
      vehicle.eval(previous_index.value(), vehicle.end.value().index());
  }
  if (!vehicle.ok_for_travel_time(eval_sum.duration)) {
    throw InputException(
      std::format("Route over max_travel_time for vehicle {}.", vehicle.id));
  }
  if (!vehicle.ok_for_distance(eval_sum.distance)) {
    throw InputException(
      std::format("Route over max_distance for vehicle {}.", vehicle.id));
  }

  if (vehicle.max_tasks < job_ranks.size()) {
    throw InputException(
      std::format("Too many tasks for vehicle {}.", vehicle.id));
  }

  if (!expected_delivery_ranks.empty()) {
    throw InputException(
      std::format("Invalid shipment in route for vehicle {}.", vehicle.id));
  }

  // Now route is OK with regard to capacity, max_travel_time,
  // max_tasks, precedence and skills constraints.
  if (!job_ranks.empty()) {
    try {
      if (!route.is_valid_addition_for_tw(input,
                                          single_jobs_deliveries,
                                          job_ranks.begin(),
                                          job_ranks.end(),
                                          0,
                                          0)) {
        throw InfeasibleRouteException(
          std::format("Infeasible route for vehicle {}.", vehicle.id));
      }

      route.replace(input,
                    single_jobs_deliveries,
                    job_ranks.begin(),
                    job_ranks.end(),
                    0,
                    0);
    } catch (const InfeasibleRouteException&) {
      // Even hard constraints (vehicle steps) cannot fit, leave jobs unassigned
      // Jobs remain in unassigned set
    }
  }
}

template <class Route>
void set_initial_routes(const Input& input,
                        std::vector<Route>& routes,
                        std::unordered_set<Index>& assigned) {
  std::ranges::for_each(routes,
                        [&](auto& r) { set_route(input, r, assigned); });
}

using RawSolution = std::vector<RawRoute>;
using TWSolution = std::vector<TWRoute>;

template Eval basic(const Input& input,
                    RawSolution& routes,
                    std::set<Index> unassigned,
                    std::vector<Index> vehicles_ranks,
                    INIT init,
                    double lambda,
                    SORT sort);

template Eval dynamic_vehicle_choice(const Input& input,
                                     RawSolution& routes,
                                     std::set<Index> unassigned,
                                     std::vector<Index> vehicles_ranks,
                                     INIT init,
                                     double lambda,
                                     SORT sort);

template void set_initial_routes(const Input& input,
                                 RawSolution& routes,
                                 std::unordered_set<Index>& assigned);

template Eval basic(const Input& input,
                    TWSolution& routes,
                    std::set<Index> unassigned,
                    std::vector<Index> vehicles_ranks,
                    INIT init,
                    double lambda,
                    SORT sort);

template Eval dynamic_vehicle_choice(const Input& input,
                                     TWSolution& routes,
                                     std::set<Index> unassigned,
                                     std::vector<Index> vehicles_ranks,
                                     INIT init,
                                     double lambda,
                                     SORT sort);

template void set_initial_routes(const Input& input,
                                 TWSolution& routes,
                                 std::unordered_set<Index>& assigned);

} // namespace vroom::heuristics
