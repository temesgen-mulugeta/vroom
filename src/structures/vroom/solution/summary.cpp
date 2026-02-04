/*

This file is part of VROOM.

Copyright (c) 2015-2025, Julien Coupey.
All rights reserved (see LICENSE).

*/

#include "structures/vroom/solution/summary.h"

namespace vroom {

Summary::Summary() : routes(0), unassigned(0), delivery(), pickup() {
  // Note: delivery and pickup are explicitly default-constructed (0-dimensional)
  // This constructor should only be used when proper initialization will follow
}

Summary::Summary(unsigned routes,
                 unsigned unassigned,
                 const Amount& zero_amount)
  : routes(routes),
    unassigned(unassigned),
    delivery(zero_amount),
    pickup(zero_amount) {
}

} // namespace vroom
