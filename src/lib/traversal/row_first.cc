// SPDX-License-Identifier: MIT

#include "traversal/row_first.h"

namespace evolving_graphs {
int64_t RowFirst::xy2d(int64_t n, int64_t x, int64_t y) {
  // x is the column indicator, y the row one.
  // Simply multiply x with the dimension size and add y to get d.
  int64_t d = y * n + x;
  return d;
}

void RowFirst::d2xy(int64_t n, int64_t d, int64_t* x, int64_t* y) {
  // y is simply floor(d / n) while x is d - y.
  *y = d / n;
  *x = d - *y * n;
}
}
