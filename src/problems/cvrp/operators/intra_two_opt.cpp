/*

This file is part of VROOM.

Copyright (c) 2015-2025, Julien Coupey.
All rights reserved (see LICENSE).

*/

#include <algorithm>

#include "problems/cvrp/operators/intra_two_opt.h"
#include "utils/helpers.h"

namespace vroom::cvrp {

IntraTwoOpt::IntraTwoOpt(const Input& input,
                         const utils::SolutionState& sol_state,
                         RawRoute& s_route,
                         Index s_vehicle,
                         Index s_rank,
                         Index t_rank)
  : Operator(OperatorName::IntraTwoOpt,
             input,
             sol_state,
             s_route,
             s_vehicle,
             s_rank,
             s_route,
             s_vehicle,
             t_rank),
    delivery(source.delivery_in_range(s_rank, t_rank + 1)) {
  // Assume s_rank < t_rank for symmetry reasons. Set aside cases
  // where t_rank = s_rank + 1, as the move is also an intra_relocate.
  assert(s_route.size() >= 3);
  assert(s_rank < t_rank - 1);
  assert(t_rank < s_route.size());
}

void IntraTwoOpt::compute_gain() {
  stored_gain = std::get<1>(utils::addition_eval_delta(_input,
                                                       _sol_state,
                                                       source,
                                                       s_rank,
                                                       t_rank + 1,
                                                       source,
                                                       s_rank,
                                                       t_rank + 1));

  gain_computed = true;
}

bool IntraTwoOpt::reversal_ok_for_shipments() const {
  bool valid = true;
  Index current = s_rank;

  while (valid && current < t_rank) {
    const auto& job = _input.jobs[s_route[current]];

    // Check pickup-delivery constraint
    valid = (job.type != JOB_TYPE::PICKUP) ||
            (_sol_state.matching_delivery_rank[s_vehicle][current] > t_rank);

    // Check relation constraint - don't reverse if job is in a relation
    if (valid) {
      if ((job.type == JOB_TYPE::PICKUP &&
           _input.job_rank_to_relation.contains(s_route[current])) ||
          (job.type == JOB_TYPE::DELIVERY && s_route[current] > 0 &&
           _input.job_rank_to_relation.contains(s_route[current] - 1))) {
        valid = false;
      }
    }

    ++current;
  }

  return valid;
}

bool IntraTwoOpt::is_valid() {
  // Check if reversal would break relation sequences at boundaries
  if (s_rank > 0 && _sol_state.relation_next_job[s_vehicle][s_rank - 1].has_value()) {
    const Index required_next =
      _sol_state.relation_next_job[s_vehicle][s_rank - 1].value();
    if (required_next == s_route[s_rank]) {
      // Job at s_rank must follow previous job, can't reverse
      return false;
    }
  }

  if (t_rank < s_route.size() - 1) {
    if (_sol_state.relation_next_job[s_vehicle][t_rank].has_value()) {
      const Index required_next =
        _sol_state.relation_next_job[s_vehicle][t_rank].value();
      if (required_next == s_route[t_rank + 1]) {
        // Job after t_rank must follow, can't reverse
        return false;
      }
    }
  }

  // Check if reversal would break shipment atomicity at boundaries
  if (s_rank > 0) {
    const auto& before_job = _input.jobs[s_route[s_rank - 1]];
    const auto& boundary_job = _input.jobs[s_route[s_rank]];

    if (before_job.type == JOB_TYPE::PICKUP &&
        boundary_job.type == JOB_TYPE::DELIVERY &&
        s_route[s_rank - 1] + 1 == s_route[s_rank]) {
      // Would break shipment at boundary
      return false;
    }
  }

  if (t_rank < s_route.size() - 1) {
    const auto& boundary_job = _input.jobs[s_route[t_rank]];
    const auto& after_job = _input.jobs[s_route[t_rank + 1]];

    if (boundary_job.type == JOB_TYPE::PICKUP &&
        after_job.type == JOB_TYPE::DELIVERY &&
        s_route[t_rank] + 1 == s_route[t_rank + 1]) {
      // Would break shipment at boundary
      return false;
    }
  }

  bool valid = (!_input.has_shipments() || reversal_ok_for_shipments()) &&
               is_valid_for_range_bounds();

  if (valid) {
    auto rev_t = s_route.rbegin() + (s_route.size() - t_rank - 1);
    auto rev_s_next = s_route.rbegin() + (s_route.size() - s_rank);

    valid = source.is_valid_addition_for_capacity_inclusion(_input,
                                                            delivery,
                                                            rev_t,
                                                            rev_s_next,
                                                            s_rank,
                                                            t_rank + 1);
  }

  return valid;
}

void IntraTwoOpt::apply() {
  std::reverse(s_route.begin() + s_rank, s_route.begin() + t_rank + 1);

  source.update_amounts(_input);
}

std::vector<Index> IntraTwoOpt::addition_candidates() const {
  return {};
}

std::vector<Index> IntraTwoOpt::update_candidates() const {
  return {s_vehicle};
}

} // namespace vroom::cvrp
