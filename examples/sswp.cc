// SPDX-License-Identifier: MIT

#include <cmath>
#include <limits.h>
#include <string.h>
#include <float.h>

#include "util/util.h"
#include "util/datatypes.h"
#include "core/vertex-store.h"
#include "util/config.h"

#include "algorithm/algorithm-common.h"
#include "main/execute.h"

namespace evolving_graphs {
namespace algorithm {
class SSWP {
public:
  typedef float VertexType;

  constexpr static VertexType neutral_element = 0;

  static const bool NEEDS_TARGETS_ACTIVE = false;

  static const bool NEEDS_SOURCES_ACTIVE = true;

  static const bool ASYNC = false;

  static const bool USE_CHANGED = false;

  static const int ASYNC_CHECK_CONVERGENCE_INTERVAL = 1;

  SSWP() = delete;

  ~SSWP() = delete;

  static inline bool pullGather(const VertexType& u, VertexType& v, uint64_t id_src,
                                uint64_t id_tgt, const vertex_degree_t& src_degree,
                                const vertex_degree_t& tgt_degree) {
    // not applicable
    return false;
  }

  static inline bool pullGatherWeighted(const VertexType& u, VertexType& v, const float weight,
                                        uint64_t id_src, uint64_t id_tgt,
                                        const vertex_degree_t& src_degree,
                                        const vertex_degree_t& tgt_degree) {
    if (u != 0) {
      v = std::max(v, std::min(u, weight));
      return true;
    }
    return false;
  }

  static inline bool apply(const uint64_t id, const uint32_t iteration) {
    // pass, nothing to be done here
    float next = VertexStore<float>::vertices_.next[id];
    float current = VertexStore<float>::vertices_.current[id];
    if (next != current) {
      VertexStore<float>::vertices_.current[id] = next;
      set_active(VertexStore<float>::vertices_.active_next, id);

      return false;
    }

    return true;
  }

  static inline bool edgeChanged(const Edge& edge, const EdgeChangeEvent& event) {
    switch (event) {
      case EdgeChangeEvent::INSERT: {
        // Only the target could potentially change its component, depending on the current
        // components:
        float source_value = VertexStore<float>::vertices_.next[edge.src()];
        // If the source has not been discovered yet, return.
        if (source_value == 0) {
          return false;
        }

        float tgt_value = VertexStore<float>::vertices_.next[edge.tgt()];

        float new_value = std::max(tgt_value, std::min(source_value, edge.weight()));
        if (new_value > tgt_value) {
          VertexStore<float>::vertices_.next[edge.tgt()] = new_value;
          VertexStore<float>::vertices_.current[edge.tgt()] = new_value;
          set_active(VertexStore<float>::vertices_.active_current, edge.tgt());
        }
        return false;
      }
      case EdgeChangeEvent::DELETE:
        if (VertexStore<float>::vertices_.critical_neighbor[edge.tgt()] == edge.src()) {
          return true;
        }
        return false;
    }
  }

  static inline bool isVertexActiveApply(const uint64_t id) {
    return true;
  }

  static inline bool isVertexActiveApplyMultiStep(const uint64_t start_id) {
    return true;
  }

  static inline bool isEdgeActive(const edge_t& edge) {
    return is_active(VertexStore<float>::vertices_.active_current, edge.src);
  }

  static inline bool compareAndSwap(VertexType* ptr, const VertexType& old_value,
                                    const VertexType& new_value) {
    return smp_cas_float(ptr, old_value, new_value);
  }

  static inline bool reduceVertex(VertexType& out, const VertexType& lhs, const VertexType& rhs,
                                  const uint64_t& id_tgt, const vertex_degree_t& degree) {
    if (lhs > rhs) {
      out = lhs;
      return true;
    } else {
      out = rhs;
      return false;
    }
  }

  static void init_vertices(void* args) {
    sg_print("Init vertices\n");

    size_t count_vertices = Config::context_.count_vertices;
    double count_active = size_bool_array(count_vertices);

    // Also need to init array for next round, otherwise everything will be 0.
    for (uint32_t i = 0; i < count_vertices; ++i) {
      VertexStore<float>::vertices_.current[i] = 0;
      VertexStore<float>::vertices_.next[i] = 0;
    }

    // All vertices inactive in the beginning, for the first round.
    memset(VertexStore<float>::vertices_.active_current,
           (unsigned char) 0,
           static_cast<size_t>(count_active * sizeof(char)));

    memset(VertexStore<float>::vertices_.active_next,
           (unsigned char) 0,
           static_cast<size_t>(count_active * sizeof(char)));

    // Set only on vertex to active at the start.
    setStartVertex(Config::context_.algorithm_config.bfs_sssp_start_id);
  }

  static void setStartVertex(uint64_t start_id) {
    VertexStore<float>::vertices_.current[start_id] = FLT_MAX;
    VertexStore<float>::vertices_.next[start_id] = FLT_MAX;

    set_active(VertexStore<float>::vertices_.active_current, start_id);
  }

  static void resetWithoutEdgeApi() {
    size_t count_vertices = Config::context_.count_vertices;
    double count_active = size_bool_array(count_vertices);
    memset(VertexStore<VertexType>::vertices_.active_current,
           (unsigned char) 255,
           static_cast<size_t>(count_active * sizeof(char)));
  }

  // Reset active current-array for next round.
  static void reset_vertices(bool* switchCurrentNext, bool* switchCurrentNextActive) {
    size_t count_vertices = Config::context_.count_vertices;
    double count_active = size_bool_array(count_vertices);
    memset(VertexStore<float>::vertices_.active_current,
           0x00,
           static_cast<size_t>(count_active * sizeof(char)));
    *switchCurrentNext = false;
    *switchCurrentNextActive = true;
  }

  static void init_vertices_local(VertexType* vertex_array,
                                  uint64_t count_vertices) {
    for (uint32_t i = 0; i < count_vertices; ++i) {
      vertex_array[i] = 0;
    }
  }
};
}
}

int main(int argc, char** argv) {
  evolving_graphs::execute<float, evolving_graphs::algorithm::SSWP, true>(argc, argv);
  return 0;
}
