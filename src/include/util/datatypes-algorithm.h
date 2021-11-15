// SPDX-License-Identifier: MIT

#ifndef EVOLVING_GRAPHS_DATATYPES_ALGORITHM_H
#define EVOLVING_GRAPHS_DATATYPES_ALGORITHM_H

#include <iostream>

struct PageRankDeltaVertexType_t {
  float delta_sum;
  float rank;
  float delta;

  bool operator==(const PageRankDeltaVertexType_t& other) {
    return (rank == other.rank && delta_sum == other.delta_sum && delta == other.delta);
  }

  bool operator!=(const PageRankDeltaVertexType_t& other) {
    return (delta_sum != other.delta_sum);
  }
};

inline std::ostream& operator<<(std::ostream& os, const PageRankDeltaVertexType_t& v) {
  os << v.rank << " " << v.delta << " " << v.delta_sum;
  return os;
}

#endif //EVOLVING_GRAPHS_DATATYPES_ALGORITHM_H
