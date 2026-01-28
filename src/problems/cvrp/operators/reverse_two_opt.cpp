/*

This file is part of VROOM.

Copyright (c) 2015-2025, Julien Coupey.
All rights reserved (see LICENSE).

*/

#include "problems/cvrp/operators/reverse_two_opt.h"
#include "utils/helpers.h"

namespace vroom::cvrp {

ReverseTwoOpt::ReverseTwoOpt(const Input& input,
                             const utils::SolutionState& sol_state,
                             RawRoute& s_route,
                             Index s_vehicle,
                             Index s_rank,
                             RawRoute& t_route,
                             Index t_vehicle,
                             Index t_rank)
  : Operator(OperatorName::ReverseTwoOpt,
             input,
             sol_state,
             s_route,
             s_vehicle,
             s_rank,
             t_route,
             t_vehicle,
             t_rank),
    _s_delivery(source.bwd_deliveries(s_rank)),
    _t_delivery(target.fwd_deliveries(t_rank)) {
  assert(s_vehicle != t_vehicle);
  assert(!s_route.empty());
  assert(!t_route.empty());
  assert(s_rank < s_route.size());
  assert(t_rank < t_route.size());

  assert(_sol_state.bwd_skill_rank[s_vehicle][t_vehicle] <= s_rank + 1);
  assert(t_rank < _sol_state.fwd_skill_rank[t_vehicle][s_vehicle]);
}

void ReverseTwoOpt::compute_gain() {
  // Skip if any job in the moved portions is in a relation or was in initial vehicle.steps
  for (Index i = s_rank + 1; i < s_route.size(); ++i) {
    const auto& job = _input.jobs[s_route[i]];
    if ((job.type == JOB_TYPE::PICKUP &&
         _input.job_rank_to_relation.contains(s_route[i])) ||
        (job.type == JOB_TYPE::DELIVERY && s_route[i] > 0 &&
         _input.job_rank_to_relation.contains(s_route[i] - 1))) {
      gain_computed = true;
      return;
    }
    if (_input.fixed_job_ranks.contains(s_route[i])) {
      gain_computed = true;
      return;
    }
  }
  for (Index i = 0; i <= t_rank; ++i) {
    const auto& job = _input.jobs[t_route[i]];
    if ((job.type == JOB_TYPE::PICKUP &&
         _input.job_rank_to_relation.contains(t_route[i])) ||
        (job.type == JOB_TYPE::DELIVERY && t_route[i] > 0 &&
         _input.job_rank_to_relation.contains(t_route[i] - 1))) {
      gain_computed = true;
      return;
    }
    if (_input.fixed_job_ranks.contains(t_route[i])) {
      gain_computed = true;
      return;
    }
  }

  s_gain = std::get<1>(utils::addition_eval_delta(_input,
                                                  _sol_state,
                                                  source,
                                                  s_rank + 1,
                                                  s_route.size(),
                                                  target,
                                                  0,
                                                  t_rank + 1));

  t_gain = (s_rank + 1u < s_route.size())
             ? std::get<1>(utils::addition_eval_delta(_input,
                                                      _sol_state,
                                                      target,
                                                      0,
                                                      t_rank + 1,
                                                      source,
                                                      s_rank + 1,
                                                      s_route.size()))
             : utils::removal_gain(_input, _sol_state, target, 0, t_rank + 1);

  stored_gain = s_gain + t_gain;
  gain_computed = true;
}

bool ReverseTwoOpt::is_valid() {
  assert(gain_computed);

  // Check if reversing and swapping would break relation sequences
  if (s_rank < s_route.size() - 1) {
    if (_sol_state.relation_next_job[s_vehicle][s_rank].has_value()) {
      const Index required_next =
        _sol_state.relation_next_job[s_vehicle][s_rank].value();
      if (s_rank + 1 < s_route.size() && required_next == s_route[s_rank + 1]) {
        // Would break relation at source boundary
        return false;
      }
    }
  }

  if (t_rank > 0 && _sol_state.relation_next_job[t_vehicle][t_rank - 1].has_value()) {
    const Index required_next =
      _sol_state.relation_next_job[t_vehicle][t_rank - 1].value();
    if (required_next == t_route[t_rank]) {
      // Would break relation at target boundary
      return false;
    }
  }

  // Check if reversing would break shipment atomicity at boundaries
  if (s_rank < s_route.size() - 1) {
    const auto& boundary_job = _input.jobs[s_route[s_rank]];
    const auto& next_job = _input.jobs[s_route[s_rank + 1]];

    if (boundary_job.type == JOB_TYPE::PICKUP &&
        next_job.type == JOB_TYPE::DELIVERY &&
        s_route[s_rank] + 1 == s_route[s_rank + 1]) {
      // Would separate shipment at source boundary
      return false;
    }
  }

  if (t_rank > 0) {
    const auto& prev_job = _input.jobs[t_route[t_rank - 1]];
    const auto& boundary_job = _input.jobs[t_route[t_rank]];

    if (prev_job.type == JOB_TYPE::PICKUP &&
        boundary_job.type == JOB_TYPE::DELIVERY &&
        t_route[t_rank - 1] + 1 == t_route[t_rank]) {
      // Would separate shipment at target boundary
      return false;
    }
  }

  const auto& t_pickup = target.fwd_pickups(t_rank);

  const auto& s_pickup = source.bwd_pickups(s_rank);

  return is_valid_for_source_range_bounds() &&
         is_valid_for_target_range_bounds() &&
         source.is_valid_addition_for_capacity_margins(_input,
                                                       t_pickup,
                                                       _t_delivery,
                                                       s_rank + 1,
                                                       s_route.size()) &&
         target.is_valid_addition_for_capacity_margins(_input,
                                                       s_pickup,
                                                       _s_delivery,
                                                       0,
                                                       t_rank + 1) &&
         source.is_valid_addition_for_capacity_inclusion(_input,
                                                         _t_delivery,
                                                         t_route.rbegin() +
                                                           t_route.size() - 1 -
                                                           t_rank,
                                                         t_route.rend(),
                                                         s_rank + 1,
                                                         s_route.size()) &&
         target.is_valid_addition_for_capacity_inclusion(_input,
                                                         _s_delivery,
                                                         s_route.rbegin(),
                                                         s_route.rbegin() +
                                                           s_route.size() - 1 -
                                                           s_rank,
                                                         0,
                                                         t_rank + 1);
}

void ReverseTwoOpt::apply() {
  auto nb_source = s_route.size() - 1 - s_rank;

  t_route.insert(t_route.begin(),
                 s_route.rbegin(),
                 s_route.rbegin() + nb_source);
  s_route.erase(s_route.begin() + s_rank + 1, s_route.end());
  s_route.insert(s_route.end(),
                 t_route.rend() - t_rank - nb_source - 1,
                 t_route.rend() - nb_source);
  t_route.erase(t_route.begin() + nb_source,
                t_route.begin() + nb_source + t_rank + 1);

  source.update_amounts(_input);
  target.update_amounts(_input);
}

std::vector<Index> ReverseTwoOpt::addition_candidates() const {
  return {s_vehicle, t_vehicle};
}

std::vector<Index> ReverseTwoOpt::update_candidates() const {
  return {s_vehicle, t_vehicle};
}

} // namespace vroom::cvrp
