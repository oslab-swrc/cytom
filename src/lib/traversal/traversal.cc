// SPDX-License-Identifier: MIT

#include "traversal/traversal.h"

namespace evolving_graphs {

Coordinates Traversal::d2Coords(int64_t n, int64_t d) {
  int64_t x, y;
  d2xy(n, d, &x, &y);

  return {static_cast<uint64_t>(x), static_cast<uint64_t>(y)};
}

int64_t Traversal::coords2d(int64_t n, const Coordinates& coords) {
  return xy2d(n, coords.x, coords.y);
}

}
