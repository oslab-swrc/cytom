// SPDX-License-Identifier: MIT

#include <cmath>
#include <limits.h>
#include <string.h>
#include <thread>

#include "util/util.h"
#include "util/datatypes.h"
#include "core/vertex-store.h"
#include "util/config.h"

#include "algorithm/algorithm-common.h"
#include "main/execute.h"

namespace evolving_graphs {
namespace algorithm {
class CC {
public:
  typedef uint32_t VertexType;

  constexpr const static VertexType neutral_element = UINT32_MAX;

  static const bool NEEDS_TARGETS_ACTIVE = false;

  static const bool NEEDS_SOURCES_ACTIVE = true;

  static const bool ASYNC = false;

  static const bool USE_CHANGED = true;

  static const int ASYNC_CHECK_CONVERGENCE_INTERVAL = 1;

  CC(std::shared_ptr<Config> config, std::shared_ptr<VertexStore<VertexType> > vertex_store);

  bool pullGather(const VertexType& u, VertexType& v, uint64_t id_src,
                                uint64_t id_tgt, const vertex_degree_t& src_degree,
                                const vertex_degree_t& tgt_degree);

  bool pullGatherWeighted(const VertexType& u, VertexType& v, const float weight,
                                        uint64_t id_src, uint64_t id_tgt,
                                        const vertex_degree_t& src_degree,
                                        const vertex_degree_t& tgt_degree);

  bool apply(const uint64_t id, const uint32_t iteration);

  bool edgeChanged(const Edge& edge, const EdgeChangeEvent& event);

  bool isVertexActiveApply(const uint64_t id);

  bool isVertexActiveApplyMultiStep(const uint64_t start_id);

  bool isEdgeActive(const edge_t& edge);

  bool compareAndSwap(VertexType* ptr, const VertexType& old_value,
                                    const VertexType& new_value);

  bool reduceVertex(VertexType& out, const VertexType& lhs, const VertexType& rhs,
                                  const uint64_t& id_tgt, const vertex_degree_t& degree);

  void init_vertices(void* args);

  void resetWithoutEdgeApi();

  // Reset active current-array for next round.
  void reset_vertices(bool* switchCurrentNext, bool* switchCurrentNextActive);

  void init_vertices_local(VertexType* vertex_array,
                                  uint64_t count_vertices);

private:
  std::shared_ptr<Config> config_;

  std::shared_ptr<VertexStore<VertexType> > vertex_store_;
};

CC::CC(std::shared_ptr<Config> config, std::shared_ptr<VertexStore<VertexType> > vertex_store):
          config_(std::move(config)),
          vertex_store_(std::move(vertex_store)) {}

bool CC::pullGather(const VertexType& u, VertexType& v, uint64_t id_src,
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

bool CC::pullGatherWeighted(const VertexType& u, VertexType& v, const float weight,
                                      uint64_t id_src, uint64_t id_tgt,
                                      const vertex_degree_t& src_degree,
                                      const vertex_degree_t& tgt_degree) {
  // not applicable
  return false;
}

bool CC::apply(const uint64_t id, const uint32_t iteration) {
  // pass, nothing to be done here
  if (vertex_store_->vertices_.next[id] !=
      vertex_store_->vertices_.current[id]) {
    vertex_store_->vertices_.current[id] = vertex_store_->vertices_.next[id];
    set_active(vertex_store_->vertices_.active_next, id);
    return false;
  }

  return true;
}

bool CC::edgeChanged(const Edge& edge, const EdgeChangeEvent& event) {
  switch (event) {
    case EdgeChangeEvent::INSERT: {
      // Only the target could potentially change its component, depending on the current
      // components:
      // TODO Add check for components?
      if (vertex_store_->vertices_.next[edge.src()] <
          vertex_store_->vertices_.next[edge.tgt()]) {
        vertex_store_->vertices_.next[edge.tgt()] =
            vertex_store_->vertices_.next[edge.src()];

        vertex_store_->vertices_.current[edge.tgt()] =
            vertex_store_->vertices_.next[edge.src()];

        set_active(vertex_store_->vertices_.active_current, edge.tgt());
        set_active_atomically(vertex_store_->vertices_.changed, edge.tgt());
      }
      return false;
    }
    case EdgeChangeEvent::DELETE:
      if (vertex_store_->vertices_.critical_neighbor[edge.tgt()] == edge.src()) {
        return true;
      }
      return false;
  }
}

bool CC::isVertexActiveApply(const uint64_t id) {
  return is_active(vertex_store_->vertices_.changed, id);
}

bool CC::isVertexActiveApplyMultiStep(const uint64_t start_id) {
  return (uint64_t) vertex_store_->vertices_.changed[start_id / 8] != 0;
}

bool CC::isEdgeActive(const edge_t& edge) {
  return is_active(vertex_store_->vertices_.active_current, edge.src);
}

bool CC::compareAndSwap(VertexType* ptr, const VertexType& old_value,
                                  const VertexType& new_value) {
  return smp_cas_float(ptr, old_value, new_value);
}

bool CC::reduceVertex(VertexType& out, const VertexType& lhs, const VertexType& rhs,
                                const uint64_t& id_tgt, const vertex_degree_t& degree) {
  if (lhs < rhs) {
    out = lhs;
    return true;
  } else {
    out = rhs;
    return false;
  }
  //      if (lhs < rhs) {
  //        set_active(active_array, id_tgt);
  //      }
}

void CC::init_vertices(void* args) {
  sg_print("Init vertices\n");

  size_t count_vertices = config_->context_.count_vertices;
  double count_active = size_bool_array(count_vertices);

  // also need to init array for next round, otherwise everything will be 0
  for (uint32_t i = 0; i < count_vertices; ++i) {
    vertex_store_->vertices_.current[i] = i;
    vertex_store_->vertices_.next[i] = i;
  }
  // all vertices active in the beginning, for the first round.
  memset(vertex_store_->vertices_.active_current,
         (unsigned char) 255,
         static_cast<size_t>(count_active * sizeof(char)));
  memset(vertex_store_->vertices_.active_next,
         (unsigned char) 0,
         static_cast<size_t>(count_active * sizeof(char)));
}

void CC::resetWithoutEdgeApi() {
  size_t count_vertices = config_->context_.count_vertices;
  double count_active = size_bool_array(count_vertices);
  memset(vertex_store_->vertices_.active_current,
         (unsigned char) 255,
         static_cast<size_t>(count_active * sizeof(char)));
}

// Reset active current-array for next round.
void CC::reset_vertices(bool* switchCurrentNext, bool* switchCurrentNextActive) {
  size_t count_vertices = config_->context_.count_vertices;
  double count_active = size_bool_array(count_vertices);
  memset(vertex_store_->vertices_.active_current, 0x00,
         static_cast<size_t>(count_active * sizeof(char)));
  *switchCurrentNext = false;
  *switchCurrentNextActive = true;
}

void CC::init_vertices_local(VertexType* vertex_array,
                                uint64_t count_vertices) {
  for (uint32_t i = 0; i < count_vertices; ++i) {
    vertex_array[i] = UINT32_MAX;
  }
}

} // end of algortihm
} // end of evolving_graphs

int main_cc(int argc, char** argv) {
  evolving_graphs::execute<uint32_t, evolving_graphs::algorithm::CC, false>(argv, NULL, 0);
  return 0;
}
