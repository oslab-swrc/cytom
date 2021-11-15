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
class SSSP {
public:
  typedef float VertexType;

  constexpr static VertexType neutral_element = FLT_MAX;

  static const bool NEEDS_TARGETS_ACTIVE = false;

  static const bool NEEDS_SOURCES_ACTIVE = true;

  static const bool ASYNC = false;

  static const bool USE_CHANGED = false;

  static const int ASYNC_CHECK_CONVERGENCE_INTERVAL = 1;

  SSSP(std::shared_ptr<Config> config, std::shared_ptr<VertexStore<VertexType> > vertex_store);

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

  // void setStartVertex(uint64_t start_id);

  void resetWithoutEdgeApi();

  // Reset active current-array for next round.
  void reset_vertices(bool* switchCurrentNext, bool* switchCurrentNextActive);

  void init_vertices_local(VertexType* vertex_array,
                                  uint64_t count_vertices);

private:
  std::shared_ptr<Config> config_;

  std::shared_ptr<VertexStore<VertexType> > vertex_store_;
};

SSSP::SSSP(std::shared_ptr<Config> config, std::shared_ptr<VertexStore<VertexType> > vertex_store):
          config_(std::move(config)),
          vertex_store_(std::move(vertex_store)) {}

bool SSSP::pullGather(const VertexType& u, VertexType& v, uint64_t id_src,
                              uint64_t id_tgt, const vertex_degree_t& src_degree,
                              const vertex_degree_t& tgt_degree) {
  // not applicable
  return false;
}

bool SSSP::pullGatherWeighted(const VertexType& u, VertexType& v, const float weight,
                                      uint64_t id_src, uint64_t id_tgt,
                                      const vertex_degree_t& src_degree,
                                      const vertex_degree_t& tgt_degree) {
  if (u != UINT32_MAX && v > u + weight + config_->context_.algorithm_config.delta) {
    v = u + weight;
    return true;
  }
  return false;
}

bool SSSP::apply(const uint64_t id, const uint32_t iteration) {
  // pass, nothing to be done here
  float next = vertex_store_->vertices_.next[id];
  float current = vertex_store_->vertices_.current[id];
  if (next != current) {
    vertex_store_->vertices_.current[id] = next;
    set_active(vertex_store_->vertices_.active_next, id);

    return false;
  }

  return true;
}

bool SSSP::edgeChanged(const Edge& edge, const EdgeChangeEvent& event) {
  switch (event) {
    case EdgeChangeEvent::INSERT: {
      // Only the target could potentially change its component, depending on the current
      // components:
      float source_value = vertex_store_->vertices_.next[edge.src()];
      // If the source has not been discovered yet, return.
      if (source_value == UINT32_MAX) {
        return false;
      }

      float new_value = source_value + edge.weight();
      float diff = vertex_store_->vertices_.next[edge.tgt()] - new_value;
      if (diff > config_->context_.algorithm_config.delta) {
        vertex_store_->vertices_.next[edge.tgt()] = new_value;
        vertex_store_->vertices_.current[edge.tgt()] = new_value;
        set_active(vertex_store_->vertices_.active_current, edge.tgt());
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

bool SSSP::isVertexActiveApply(const uint64_t id) {
  return true;
}

bool SSSP::isVertexActiveApplyMultiStep(const uint64_t start_id) {
  return true;
}

bool SSSP::isEdgeActive(const edge_t& edge) {
  return is_active(vertex_store_->vertices_.active_current, edge.src);
}

bool SSSP::compareAndSwap(VertexType* ptr, const VertexType& old_value,
                                  const VertexType& new_value) {
  return smp_cas_float(ptr, old_value, new_value);
}

bool SSSP::reduceVertex(VertexType& out, const VertexType& lhs, const VertexType& rhs,
                                const uint64_t& id_tgt, const vertex_degree_t& degree) {
  if (lhs + config_->context_.algorithm_config.delta < rhs) {
    out = lhs;
    return true;
  } else {
    out = rhs;
    return false;
  }
}

void SSSP::init_vertices(void* args) {
  sg_print("Init vertices\n");

  size_t count_vertices = config_->context_.count_vertices;
  double count_active = size_bool_array(count_vertices);

  // Also need to init array for next round, otherwise everything will be 0.
  for (uint32_t i = 0; i < count_vertices; ++i) {
    vertex_store_->vertices_.current[i] = FLT_MAX;
    vertex_store_->vertices_.next[i] = FLT_MAX;
  }

  // All vertices inactive in the beginning, for the first round.
  memset(vertex_store_->vertices_.active_current,
         (unsigned char) 0,
         static_cast<size_t>(count_active * sizeof(char)));

  memset(vertex_store_->vertices_.active_next,
         (unsigned char) 0,
         static_cast<size_t>(count_active * sizeof(char)));

  // Set only on vertex to active at the start.
  // setStartVertex(config_->context_.algorithm_config.bfs_sssp_start_id);
  set_active(vertex_store_->vertices_.active_current, config_->context_.algorithm_config.algo_start_id);
}

// void SSSP::setStartVertex(uint64_t start_id) {
//   vertex_store_->vertices_.current[start_id] = 0;
//   vertex_store_->vertices_.next[start_id] = 0;

//   set_active(vertex_store_->vertices_.active_current, start_id);
// }

void SSSP::resetWithoutEdgeApi() {
  size_t count_vertices = config_->context_.count_vertices;
  double count_active = size_bool_array(count_vertices);
  memset(vertex_store_->vertices_.active_current,
         (unsigned char) 255,
         static_cast<size_t>(count_active * sizeof(char)));
}

// Reset active current-array for next round.
void SSSP::reset_vertices(bool* switchCurrentNext, bool* switchCurrentNextActive) {
  size_t count_vertices = config_->context_.count_vertices;
  double count_active = size_bool_array(count_vertices);
  memset(vertex_store_->vertices_.active_current,
         0x00,
         static_cast<size_t>(count_active * sizeof(char)));
  *switchCurrentNext = false;
  *switchCurrentNextActive = true;
}

void SSSP::init_vertices_local(VertexType* vertex_array,
                                uint64_t count_vertices) {
  for (uint32_t i = 0; i < count_vertices; ++i) {
    vertex_array[i] = FLT_MAX;
  }
}

} // end of algortihm
} // end of evolving_graphs

int main_sssp(int argc, char** argv) {
  evolving_graphs::execute<float, evolving_graphs::algorithm::SSSP, true>(argv, NULL, 0);
  return 0;
}
