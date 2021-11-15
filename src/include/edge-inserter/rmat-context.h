// SPDX-License-Identifier: MIT

#pragma once

#include <string.h>

#include "util/util.h"
#include "util/datatypes.h"

namespace evolving_graphs {

class RMATContext {
public:
  RMATContext(double a, double b, double c, uint64_t count_vertices,
              uint64_t seed) { hash_ = util::hash(seed); }

  ~RMATContext() {}

  edge_t rMat(uint64_t n, uint64_t rand_start, uint64_t rand_stride) {
    if (n == 1) {
      return {0, 0};
    }

    edge_t edge = rMat(n / 2, rand_start + rand_stride, rand_stride);

    double r = util::hashDouble(rand_start);
    if (r < a_) {
      return edge;
    }
    if (r < ab_) {
      return {edge.src, edge.tgt + n / 2};
    }
    if (r < abc_) {
      return {edge.src + n / 2, edge.tgt};
    }
    return {edge.src + n / 2, edge.tgt + n / 2};
  }

  edge_t rMatIterative(uint64_t n, uint64_t rand_start, uint64_t rand_stride) {
    uint64_t local_n = n;
    uint64_t local_rand_start = rand_start;

    while (local_n != 1) {
      local_n = local_n / 2;
      local_rand_start = local_rand_start + rand_stride;
    }
    edge_t edge = {0, 0};

    while (local_n != n) {
      local_n = local_n * 2;
      local_rand_start = local_rand_start - rand_stride;
      double r = util::hashDouble(local_rand_start);
      if (r < a_) {
      } else if (r < ab_) {
        edge.tgt = edge.tgt + local_n / 2;
      } else if (r < abc_) {
        edge.src = edge.src + local_n / 2;
      } else {
        edge.tgt = edge.tgt + local_n / 2;
        edge.src = edge.src + local_n / 2;
      }
    }

    return edge;
  }

  edge_t getEdge(uint64_t index) {
    uint64_t rand_start = util::hash(2ul * index * hash_);
    uint64_t rand_stride = util::hash((2ul * index + 1) * hash_);

    // return rMat(count_vertices_, rand_start, rand_stride);
    return rMatIterative(count_vertices_, rand_start, rand_stride);
  }

private:
  double a_;
  double ab_;
  double abc_;

  uint64_t count_vertices_;
  uint64_t hash_;
};
}
