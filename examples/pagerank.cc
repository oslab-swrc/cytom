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

#define ALPHA 0.85

#define EPSILON 0.01

namespace evolving_graphs {

namespace algorithm {
class PageRank {
public:
  typedef float VertexType;

  constexpr const static VertexType neutral_element = static_cast<const VertexType>(0.);

  static const bool NEEDS_TARGETS_ACTIVE = true;

  static const bool NEEDS_SOURCES_ACTIVE = true;

  static const bool ASYNC = false;

  static const bool USE_CHANGED = false;

  static const int ASYNC_CHECK_CONVERGENCE_INTERVAL = 1;

  PageRank(std::shared_ptr<Config> config, std::shared_ptr<VertexStore<VertexType> > vertex_store);

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

  // reset current-array for next round
  void reset_vertices(bool* switchCurrentNext, bool* switchCurrentNextActive);

  void init_vertices_local(VertexType* vertex_array,
                                  uint64_t count_vertices);

  // bool check_convergence(uint64_t id);

  // bool check_convergence();

private:
  std::shared_ptr<Config> config_;

  std::shared_ptr<VertexStore<VertexType> > vertex_store_;

};

PageRank::PageRank(std::shared_ptr<Config> config, std::shared_ptr<VertexStore<VertexType> > vertex_store):
          config_(std::move(config)),
          vertex_store_(std::move(vertex_store)) {}

bool PageRank::pullGather(const VertexType& u, VertexType& v, uint64_t id_src,
                              uint64_t id_tgt, const vertex_degree_t& src_degree,
                              const vertex_degree_t& tgt_degree) {
  v = v + (u / src_degree.out_degree);
  return false;
}

bool PageRank::pullGatherWeighted(const VertexType& u, VertexType& v, const float weight,
                                      uint64_t id_src, uint64_t id_tgt,
                                      const vertex_degree_t& src_degree,
                                      const vertex_degree_t& tgt_degree) {
  // not applicable
  return false;
}

bool PageRank::apply(const uint64_t id, const uint32_t iteration) {
  // Only apply to vertices receiving updates in this round, copy over old
  // value for inactive ones.
  //    if (eval_bool_array(VertexStore::vertices_.changed, id)) {
  vertex_store_->vertices_.next[id] =
      static_cast<VertexType>((1 - ALPHA) + ALPHA * vertex_store_->vertices_.next[id]);
  float diff = std::abs(
      vertex_store_->vertices_.current[id] - vertex_store_->vertices_.next[id]);
  if (diff > EPSILON) {
    set_bool_array(vertex_store_->vertices_.active_next, id, true);
//      sg_log("Set id active: %lu, diff: %f\n", id, diff);
  }
  //    } else {
  //      VertexStore::vertices_.next[id] = VertexStore::vertices_.current[id];
  //    }
  return diff <= EPSILON;
}

bool PageRank::edgeChanged(const Edge& edge, const EdgeChangeEvent& event) {
  switch (event) {
    case EdgeChangeEvent::INSERT: {
      set_active(vertex_store_->vertices_.active_next, edge.src());
      set_active(vertex_store_->vertices_.active_next, edge.tgt());
      return false;
    }
    case EdgeChangeEvent::DELETE:
      return true;
  }
}

bool PageRank::isVertexActiveApply(const uint64_t id) {
  return true;
}

bool PageRank::isVertexActiveApplyMultiStep(const uint64_t start_id) {
  return true;
}

bool PageRank::isEdgeActive(const edge_t& edge) {
  return (is_active(vertex_store_->vertices_.active_current, edge.src) ||
          is_active(vertex_store_->vertices_.active_current, edge.tgt));
}

bool PageRank::compareAndSwap(VertexType* ptr, const VertexType& old_value,
                                  const VertexType& new_value) {
  return smp_cas_float(ptr, old_value, new_value);
}

bool PageRank::reduceVertex(VertexType& out, const VertexType& lhs, const VertexType& rhs,
                                const uint64_t& id_tgt, const vertex_degree_t& degree) {
  out = lhs + rhs;
  return false;
}

void PageRank::init_vertices(void* args) {
  sg_print("Init vertices\n");

  size_t count_vertices = config_->context_.count_vertices;
  double count_active = size_bool_array(count_vertices);

  auto initial_value = static_cast<VertexType>(1. / count_vertices);
  for (uint64_t i = 0; i < count_vertices; ++i) {
    vertex_store_->vertices_.current[i] = static_cast<VertexType>(1.);
  }
  // all vertices active in the beginning
  memset(vertex_store_->vertices_.next, 0, sizeof(VertexType) * count_vertices);
  memset(vertex_store_->vertices_.temp_next, 0, sizeof(VertexType) * count_vertices);
  memset(vertex_store_->vertices_.active_current,
         (unsigned char) 255,
         static_cast<size_t>(count_active * sizeof(char)));
  memset(vertex_store_->vertices_.active_next,
         (unsigned char) 255,
         static_cast<size_t>(count_active * sizeof(char)));
}

void PageRank::resetWithoutEdgeApi() {
  size_t count_vertices = config_->context_.count_vertices;
  double count_active = size_bool_array(count_vertices);
  memset(vertex_store_->vertices_.active_current,
         (unsigned char) 255,
         static_cast<size_t>(count_active * sizeof(char)));
}

// reset current-array for next round
void PageRank::reset_vertices(bool* switchCurrentNext, bool* switchCurrentNextActive) {
  sg_print("Resetting vertices for next round\n");
  size_t count_vertices = config_->context_.count_vertices;
  double count_active = size_bool_array(count_vertices);

  //     activate all vertices for next round
  //     reset all values of current-array, for next round

  // Reset all (future) active values for the next round.
  //    for (uint64_t i = 0; i < count_vertices; ++i) {
  //      if (eval_bool_array(vertex_store_->vertices_.active_next, i)) {
  //        vertex_store_->vertices_.current[i] = static_cast<VertexType>(0.);
  //      }
  //    }

  // Reset active status for the next round.
  memset(vertex_store_->vertices_.current, 0, sizeof(VertexType) * count_vertices);
  memset(vertex_store_->vertices_.active_current, 0x00,
         static_cast<size_t>(count_active * sizeof(char)));
}

void PageRank::init_vertices_local(VertexType* vertex_array,
                                uint64_t count_vertices) {
  memset(vertex_array, 0, sizeof(VertexType) * count_vertices);
}

// bool PageRank::check_convergence(uint64_t id) {
//   float diff = std::abs(
//       vertex_store_->vertices_.current[id] - vertex_store_->vertices_.next[id]);
//   return diff <= EPSILON;
// }

// bool PageRank::check_convergence() {
//   // Simply check if all vertices are now inactive.
//   bool return_value = true;
//   size_t count_vertices = config_->context_.count_vertices;
//   for (uint64_t i = 0; i < count_vertices; ++i) {
//     if (!check_convergence(i)) {
//       set_active(vertex_store_->vertices_.active_next, i);
//       return_value = false;
//     } else {
//       set_inactive(vertex_store_->vertices_.active_next, i);
//     }
//   }
//   return return_value;
// }

} // end of algortihm
} // end of evolving_graphs

int main_pagerank(int argc, char** argv) {
  evolving_graphs::execute<float, evolving_graphs::algorithm::PageRank, false>(argv, NULL, 0);
  return 0;
}