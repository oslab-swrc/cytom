// SPDX-License-Identifier: MIT

#include "traversal/column_first.h"

namespace evolving_graphs {
int64_t ColumnFirst::xy2d(int64_t n, int64_t x, int64_t y) {
  // x is the column indicator, y the row one.
  // Simply multiply x with the dimension size and add y to get d.
  int64_t d = x * n + y;
  return d;
}

void ColumnFirst::d2xy(int64_t n, int64_t d, int64_t* x, int64_t* y) {
  // x is simply floor(d / n) while y is d - x.
  *x = d / n;
  *y = d - *x * n;
}
}
