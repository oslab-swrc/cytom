// SPDX-License-Identifier: MIT

#include <cmath>
#include <limits.h>
#include <string.h>

#include "util/util.h"
#include "util/datatypes.h"
#include "core/vertex-store.h"
#include "util/config.h"

#include "algorithm/algorithm-common.h"
#include "main/execute.h"

namespace evolving_graphs {
namespace algorithm {
class CCAsync {
public:
  typedef uint32_t VertexType;

  constexpr const static VertexType neutral_element = UINT32_MAX;

  static const bool NEEDS_TARGETS_ACTIVE = false;

  static const bool NEEDS_SOURCES_ACTIVE = true;

  static const bool ASYNC = true;

  static const bool USE_CHANGED = false;

  static const int ASYNC_CHECK_CONVERGENCE_INTERVAL = 2;

  CCAsync() = delete;

  ~CCAsync() = delete;

  static inline bool pullGather(const VertexType& u, VertexType& v, uint64_t id_src,
                                uint64_t id_tgt, const vertex_degree_t& src_degree,
                                const vertex_degree_t& tgt_degree) {
    if (u != UINT32_MAX) {
      if (u < v) {
        v = u;
        return true;
      }
    }
    return false;
  }

  static inline bool pullGatherWeighted(const VertexType& u, VertexType& v, const float weight,
                                        uint64_t id_src, uint64_t id_tgt,
                                        const vertex_degree_t& src_degree,
                                        const vertex_degree_t& tgt_degree) {
    // not applicable
    return false;
  }

  static inline bool apply(const uint64_t id, const uint32_t iteration) {
    // not applicable
    return true;
  }

  static inline bool edgeChanged(const Edge& edge, const EdgeChangeEvent& event) {
    switch (event) {
      case EdgeChangeEvent::INSERT: {
        // Only the target could potentially change its component, depending on the current
        // components:
        // TODO Add check for components?
        if (VertexStore<uint32_t>::vertices_.next[edge.src()] <
            VertexStore<uint32_t>::vertices_.next[edge.tgt()]) {
          VertexStore<uint32_t>::vertices_.next[edge.tgt()] =
              VertexStore<uint32_t>::vertices_.next[edge.src()];

          VertexStore<uint32_t>::vertices_.current[edge.tgt()] =
              VertexStore<uint32_t>::vertices_.next[edge.src()];

          set_active(VertexStore<uint32_t>::vertices_.active_next, edge.tgt());
        }
        return false;
      }
      case EdgeChangeEvent::DELETE:
        return true;
    }
  }

  static inline bool isVertexActiveApply(const uint64_t id) {
    return true;
  }

  static inline bool isVertexActiveApplyMultiStep(const uint64_t start_id) {
    return true;
  }

  static inline bool isEdgeActive(const edge_t& edge) {
    return is_active(VertexStore<uint32_t>::vertices_.active_current, edge.src);
  }

  static inline bool compareAndSwap(VertexType* ptr, const VertexType& old_value,
                                    const VertexType& new_value) {
    return smp_cas_float(ptr, old_value, new_value);
  }

  static inline bool reduceVertex(VertexType& out, const VertexType& lhs, const VertexType& rhs,
                                  const uint64_t& id_tgt, const vertex_degree_t& degree) {
    if (lhs < rhs) {
      out = lhs;
      set_active(VertexStore<uint32_t>::vertices_.active_next, id_tgt);
      return true;
    } else {
      out = rhs;
      return false;
    }
    //    out = std::min(lhs, rhs);
    //    if (lhs < rhs) {
    //    }
  }

  static void init_vertices(void* args) {
    sg_print("Init vertices\n");

    size_t count_vertices = Config::context_.count_vertices;
    double count_active = size_bool_array(count_vertices);

    // also need to init array for next round, otherwise everything will be 0
    for (uint32_t i = 0; i < count_vertices; ++i) {
      VertexStore<uint32_t>::vertices_.current[i] = i;
      VertexStore<uint32_t>::vertices_.next[i] = i;
    }
    // all vertices active in the beginning, for the first round.
    memset(VertexStore<uint32_t>::vertices_.active_current,
           (unsigned char) 255,
           static_cast<size_t>(count_active * sizeof(char)));
    memset(VertexStore<uint32_t>::vertices_.active_next,
           (unsigned char) 0,
           static_cast<size_t>(count_active * sizeof(char)));
  }

  static void resetWithoutEdgeApi() {
    size_t count_vertices = Config::context_.count_vertices;
    double count_active = size_bool_array(count_vertices);
    memset(VertexStore<VertexType>::vertices_.active_next,
           (unsigned char) 255,
           static_cast<size_t>(count_active * sizeof(char)));
  }

  // Reset active current-array for next round.
  static void reset_vertices(bool* switchCurrentNext, bool* switchCurrentNextActive) {
    size_t count_vertices = Config::context_.count_vertices;
    double count_active = size_bool_array(count_vertices);
    memset(VertexStore<uint32_t>::vertices_.active_current, 0x00,
           static_cast<size_t>(count_active * sizeof(char)));
    *switchCurrentNext = false;
    *switchCurrentNextActive = true;
  }

  static void init_vertices_local(VertexType* vertex_array,
                                  uint64_t count_vertices) {
    for (uint32_t i = 0; i < count_vertices; ++i) {
      vertex_array[i] = UINT32_MAX;
    }
  }
};
}
}

int main(int argc, char** argv) {
  evolving_graphs::execute<uint32_t, evolving_graphs::algorithm::CCAsync, false>(argc, argv);
  return 0;
}