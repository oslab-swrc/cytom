// SPDX-License-Identifier: MIT

#pragma once

#include <stdint.h>

#include "traversal/traversal.h"

namespace evolving_graphs {
class ColumnFirst : public Traversal {
public:
  // convert (x,y) to d
  int64_t xy2d(int64_t n, int64_t x, int64_t y) override;

  // convert d to (x,y)
  void d2xy(int64_t n, int64_t d, int64_t* x, int64_t* y) override;
};
}
