/*

This file is part of VROOM.

Copyright (c) 2015-2025, Julien Coupey.
All rights reserved (see LICENSE).

*/

#include "problems/cvrp/operators/intra_or_opt.h"
#include "utils/helpers.h"

namespace vroom::cvrp {

IntraOrOpt::IntraOrOpt(const Input& input,
                       const utils::SolutionState& sol_state,
                       RawRoute& s_raw_route,
                       Index s_vehicle,
                       Index s_rank,
                       Index t_rank,
                       bool check_reverse)
  : Operator(OperatorName::IntraOrOpt,
             input,
             sol_state,
             s_raw_route,
             s_vehicle,
             s_rank,
             s_raw_route,
             s_vehicle,
             t_rank),
    // Required for consistency in compute_gain if check_reverse is
    // false.
    check_reverse(check_reverse),
    _moved_jobs((s_rank < t_rank) ? t_rank - s_rank + 2 : s_rank - t_rank + 2),
    _first_rank(std::min(s_rank, t_rank)),
    _last_rank(std::max(s_rank, t_rank) + 2),
    _delivery(source.delivery_in_range(_first_rank, _last_rank)) {
  assert(s_route.size() >= 4);
  assert(s_rank < s_route.size() - 1);
  assert(t_rank <= s_route.size() - 2);
  assert(s_rank != t_rank);
  // Either moving an edge of single jobs or a whole shipment.
  assert((_input.jobs[s_route[s_rank]].type == JOB_TYPE::SINGLE &&
          _input.jobs[s_route[s_rank + 1]].type == JOB_TYPE::SINGLE &&
          check_reverse) ||
         (_input.jobs[s_route[s_rank]].type == JOB_TYPE::PICKUP &&
          _input.jobs[s_route[s_rank + 1]].type == JOB_TYPE::DELIVERY &&
          !check_reverse &&
          _sol_state.matching_delivery_rank[s_vehicle][s_rank] == s_rank + 1));

  if (t_rank < s_rank) {
    _s_edge_first = 0;
    _s_edge_last = 1;

    std::copy(s_route.begin() + t_rank,
              s_route.begin() + s_rank,
              _moved_jobs.begin() + 2);
  } else {
    _s_edge_first = _moved_jobs.size() - 2;
    _s_edge_last = _moved_jobs.size() - 1;

    std::copy(s_route.begin() + s_rank + 2,
              s_route.begin() + t_rank + 2,
              _moved_jobs.begin());
  }

  _moved_jobs[_s_edge_first] = s_route[s_rank];
  _moved_jobs[_s_edge_last] = s_route[s_rank + 1];
}

Eval IntraOrOpt::gain_upper_bound() {
  // Skip if any job is in a relation
  for (Index i = s_rank; i <= s_rank + 1; ++i) {
    const auto& job = _input.jobs[s_route[i]];
    if ((job.type == JOB_TYPE::PICKUP &&
         _input.job_rank_to_relation.contains(s_route[i])) ||
        (job.type == JOB_TYPE::DELIVERY && s_route[i] > 0 &&
         _input.job_rank_to_relation.contains(s_route[i] - 1))) {
      _gain_upper_bound_computed = true;
      return NO_EVAL;
    }
  }

  // For addition, consider the cost of adding source edge at new rank
  // *after* removal.
  const auto new_rank = t_rank + ((s_rank < t_rank) ? 2 : 0);

  s_gain = utils::removal_gain(_input, _sol_state, source, s_rank, s_rank + 2);

  std::tie(_normal_t_gain, _reversed_t_gain) =
    utils::addition_eval_delta(_input,
                               _sol_state,
                               target,
                               new_rank,
                               new_rank,
                               source,
                               s_rank,
                               s_rank + 2);

  auto t_gain_upper_bound = _normal_t_gain;

  if (check_reverse) {
    t_gain_upper_bound = std::max(t_gain_upper_bound, _reversed_t_gain);
  }

  _gain_upper_bound_computed = true;

  return s_gain + t_gain_upper_bound;
}

void IntraOrOpt::compute_gain() {
  assert(_gain_upper_bound_computed);
  assert(is_normal_valid || is_reverse_valid);

  stored_gain = s_gain;

  if (_normal_t_gain < _reversed_t_gain) {
    // Biggest potential gain is obtained when reversing edge.
    if (is_reverse_valid) {
      reverse_s_edge = true;
      stored_gain += _reversed_t_gain;
    } else {
      stored_gain += _normal_t_gain;
    }
  } else {
    // Biggest potential gain is obtained when not reversing edge.
    if (is_normal_valid) {
      stored_gain += _normal_t_gain;
    } else {
      reverse_s_edge = true;
      stored_gain += _reversed_t_gain;
    }
  }

  gain_computed = true;
}

bool IntraOrOpt::is_valid() {
  assert(_gain_upper_bound_computed);

  // Check if moving edge would break relation sequences
  if (s_rank > 0) {
    if (_sol_state.relation_next_job[s_vehicle][s_rank - 1].has_value()) {
      const Index required_next =
        _sol_state.relation_next_job[s_vehicle][s_rank - 1].value();
      if (required_next == s_route[s_rank]) {
        return false;
      }
    }
  }

  if (s_rank + 1 < s_route.size() - 1) {
    if (_sol_state.relation_next_job[s_vehicle][s_rank + 1].has_value()) {
      const Index required_next =
        _sol_state.relation_next_job[s_vehicle][s_rank + 1].value();
      if (s_rank + 2 < s_route.size() && required_next == s_route[s_rank + 2]) {
        return false;
      }
    }
  }

  // Check if removing edge would break shipment atomicity
  if (s_rank > 0 && s_rank + 2 < s_route.size()) {
    const auto& before_job = _input.jobs[s_route[s_rank - 1]];
    const auto& after_job = _input.jobs[s_route[s_rank + 2]];

    if (before_job.type == JOB_TYPE::PICKUP &&
        after_job.type == JOB_TYPE::DELIVERY &&
        s_route[s_rank - 1] + 1 == s_route[s_rank + 2]) {
      return false;
    }
  }

  // Check if insertion would break shipment atomicity
  auto insert_pos = t_rank;
  if (s_rank < t_rank) {
    insert_pos = t_rank - 2;  // After removal of 2 jobs
  }

  if (insert_pos > 0 && insert_pos < s_route.size() - 2) {
    // Will need to check after the edge is moved, but we can check boundaries
    Index boundary_before = (insert_pos <= s_rank) ? insert_pos :
                           (insert_pos > s_rank + 1) ? insert_pos - 2 : 0;
    Index boundary_after = boundary_before + 1;

    if (boundary_before < s_route.size() && boundary_after < s_route.size()) {
      const auto& before_job = _input.jobs[s_route[boundary_before]];
      const auto& after_job = _input.jobs[s_route[boundary_after]];

      if (before_job.type == JOB_TYPE::PICKUP &&
          after_job.type == JOB_TYPE::DELIVERY &&
          s_route[boundary_before] + 1 == s_route[boundary_after]) {
        // Would interrupt existing shipment
        return false;
      }
    }
  }

  const auto& s_v = _input.vehicles[s_vehicle];
  const auto& s_eval = _sol_state.route_evals[s_vehicle];
  const auto normal_eval = s_gain + _normal_t_gain;

  is_normal_valid =
    s_v.ok_for_range_bounds(s_eval - normal_eval) &&
    source.is_valid_addition_for_capacity_inclusion(_input,
                                                    _delivery,
                                                    _moved_jobs.begin(),
                                                    _moved_jobs.end(),
                                                    _first_rank,
                                                    _last_rank);

  if (check_reverse) {
    const auto reversed_eval = s_gain + _reversed_t_gain;

    if (s_v.ok_for_range_bounds(s_eval - reversed_eval)) {
      std::swap(_moved_jobs[_s_edge_first], _moved_jobs[_s_edge_last]);

      is_reverse_valid =
        source.is_valid_addition_for_capacity_inclusion(_input,
                                                        _delivery,
                                                        _moved_jobs.begin(),
                                                        _moved_jobs.end(),
                                                        _first_rank,
                                                        _last_rank);

      // Reset to initial situation before potential application or TW
      // checks.
      std::swap(_moved_jobs[_s_edge_first], _moved_jobs[_s_edge_last]);
    }
  }

  return is_normal_valid || is_reverse_valid;
}

void IntraOrOpt::apply() {
  assert(!reverse_s_edge ||
         (_input.jobs[s_route[s_rank]].type == JOB_TYPE::SINGLE &&
          _input.jobs[s_route[s_rank + 1]].type == JOB_TYPE::SINGLE));

  auto first_job_rank = s_route[s_rank];
  auto second_job_rank = s_route[s_rank + 1];
  s_route.erase(s_route.begin() + s_rank, s_route.begin() + s_rank + 2);
  s_route.insert(s_route.begin() + t_rank, {first_job_rank, second_job_rank});
  if (reverse_s_edge) {
    std::swap(t_route[t_rank], t_route[t_rank + 1]);
  }

  source.update_amounts(_input);
}

std::vector<Index> IntraOrOpt::addition_candidates() const {
  return {};
}

std::vector<Index> IntraOrOpt::update_candidates() const {
  return {s_vehicle};
}

} // namespace vroom::cvrp
