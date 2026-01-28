/*

This file is part of VROOM.

Copyright (c) 2015-2025, Julien Coupey.
All rights reserved (see LICENSE).

*/

#include "problems/cvrp/operators/intra_relocate.h"
#include "utils/helpers.h"

namespace vroom::cvrp {

IntraRelocate::IntraRelocate(const Input& input,
                             const utils::SolutionState& sol_state,
                             RawRoute& s_raw_route,
                             Index s_vehicle,
                             Index s_rank,
                             Index t_rank)
  : Operator(OperatorName::IntraRelocate,
             input,
             sol_state,
             s_raw_route,
             s_vehicle,
             s_rank,
             s_raw_route,
             s_vehicle,
             t_rank),
    _moved_jobs((s_rank < t_rank) ? t_rank - s_rank + 1 : s_rank - t_rank + 1),
    _first_rank(std::min(s_rank, t_rank)),
    _last_rank(std::max(s_rank, t_rank) + 1),
    _delivery(source.delivery_in_range(_first_rank, _last_rank)) {
  assert(s_route.size() >= 2);
  assert(s_rank < s_route.size());
  assert(t_rank <= s_route.size() - 1);
  assert(s_rank != t_rank);

  if (t_rank < s_rank) {
    _moved_jobs[0] = s_route[s_rank];
    std::copy(s_route.begin() + t_rank,
              s_route.begin() + s_rank,
              _moved_jobs.begin() + 1);
  } else {
    std::copy(s_route.begin() + s_rank + 1,
              s_route.begin() + t_rank + 1,
              _moved_jobs.begin());
    _moved_jobs.back() = s_route[s_rank];
  }
}

void IntraRelocate::compute_gain() {
  const auto& job = _input.jobs[s_route[s_rank]];

  // Skip if job is a pickup or delivery in a relation
  if (job.type == JOB_TYPE::PICKUP &&
      _input.job_rank_to_relation.contains(s_route[s_rank])) {
    gain_computed = true;
    return;
  }
  if (job.type == JOB_TYPE::DELIVERY &&
      s_route[s_rank] > 0 &&
      _input.job_rank_to_relation.contains(s_route[s_rank] - 1)) {
    gain_computed = true;
    return;
  }

  const auto& v_target = _input.vehicles[s_vehicle];

  // For removal, we consider the cost of removing job at rank s_rank,
  // already stored in _sol_state.node_gains[s_vehicle][s_rank].

  // For addition, consider the cost of adding source job at new rank
  // *after* removal.
  auto new_rank = t_rank;
  if (s_rank < t_rank) {
    ++new_rank;
  }
  stored_gain =
    _sol_state.node_gains[s_vehicle][s_rank] -
    utils::addition_eval(_input, s_route[s_rank], v_target, t_route, new_rank);

  gain_computed = true;
}

bool IntraRelocate::is_valid() {
  // Check if removal would break a relation sequence
  if (s_rank > 0 && _sol_state.relation_next_job[s_vehicle][s_rank - 1].has_value()) {
    const Index required_next =
      _sol_state.relation_next_job[s_vehicle][s_rank - 1].value();
    if (required_next == s_route[s_rank]) {
      // This job must follow the previous job in a relation
      return false;
    }
  }

  if (s_rank < s_route.size() - 1) {
    if (_sol_state.relation_next_job[s_vehicle][s_rank].has_value()) {
      const Index required_next =
        _sol_state.relation_next_job[s_vehicle][s_rank].value();
      if (required_next == s_route[s_rank + 1]) {
        // Next job must follow this job in a relation
        return false;
      }
    }
  }

  // Check if removal would break shipment atomicity
  if (s_rank > 0 && s_rank < s_route.size() - 1) {
    const auto& before_job = _input.jobs[s_route[s_rank - 1]];
    const auto& after_job = _input.jobs[s_route[s_rank + 1]];

    if (before_job.type == JOB_TYPE::PICKUP &&
        after_job.type == JOB_TYPE::DELIVERY &&
        s_route[s_rank - 1] + 1 == s_route[s_rank + 1]) {
      // Removing this job would separate pickup from delivery
      return false;
    }
  }

  // Check if insertion would break shipment atomicity
  auto insert_pos = t_rank;
  if (s_rank < t_rank) {
    insert_pos = t_rank - 1;  // After removal, position shifts
  }

  if (insert_pos > 0 && insert_pos < s_route.size() - 1) {
    // Check the position where the job will be inserted (after removal)
    Index before_idx = (insert_pos - 1 < s_rank) ? insert_pos - 1 : insert_pos;
    Index after_idx = (insert_pos >= s_rank) ? insert_pos : insert_pos + 1;

    if (before_idx < s_route.size() && after_idx < s_route.size()) {
      const auto& before_job = _input.jobs[s_route[before_idx]];
      const auto& after_job = _input.jobs[s_route[after_idx]];

      if (before_job.type == JOB_TYPE::PICKUP &&
          after_job.type == JOB_TYPE::DELIVERY &&
          s_route[before_idx] + 1 == s_route[after_idx]) {
        // Would interrupt shipment
        return false;
      }
    }
  }

  return is_valid_for_range_bounds() &&
         source.is_valid_addition_for_capacity_inclusion(_input,
                                                         _delivery,
                                                         _moved_jobs.begin(),
                                                         _moved_jobs.end(),
                                                         _first_rank,
                                                         _last_rank);
}

void IntraRelocate::apply() {
  auto relocate_job_rank = s_route[s_rank];
  s_route.erase(s_route.begin() + s_rank);
  s_route.insert(t_route.begin() + t_rank, relocate_job_rank);

  source.update_amounts(_input);
}

std::vector<Index> IntraRelocate::addition_candidates() const {
  return {};
}

std::vector<Index> IntraRelocate::update_candidates() const {
  return {s_vehicle};
}

} // namespace vroom::cvrp
