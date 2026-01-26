#ifndef RELATION_H
#define RELATION_H

/*

This file is part of VROOM.

Copyright (c) 2015-2025, Julien Coupey.
All rights reserved (see LICENSE).

*/

#include <vector>

#include "structures/typedefs.h"

namespace vroom {

// Type of relation constraint
enum class RELATION_TYPE : std::uint8_t { IN_DIRECT_SEQUENCE };

// Step in a relation
struct RelationStep {
  enum class TYPE : std::uint8_t { SHIPMENT };

  TYPE type;
  Id id;  // Shipment pickup ID

  RelationStep(TYPE type, Id id) : type(type), id(id) {
  }
};

// Relation constraint structure
struct Relation {
  RELATION_TYPE type;
  std::vector<RelationStep> steps;

  // Derived data populated during Input construction
  std::vector<Index> pickup_ranks;    // Job ranks in Input.jobs vector
  std::vector<Index> delivery_ranks;  // Corresponding delivery ranks

  Relation(RELATION_TYPE type = RELATION_TYPE::IN_DIRECT_SEQUENCE)
    : type(type) {
  }
};

} // namespace vroom

#endif
