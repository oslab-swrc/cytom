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
class BFS {
public:
  typedef uint32_t VertexType;

  const static VertexType neutral_element = UINT32_MAX;

  static const bool NEEDS_TARGETS_ACTIVE = false;

  static const bool NEEDS_SOURCES_ACTIVE = true;

  static const bool ASYNC = false;

  static const bool USE_CHANGED = true;

  static const int ASYNC_CHECK_CONVERGENCE_INTERVAL = 1;

  BFS(std::shared_ptr<Config> config, std::shared_ptr<VertexStore<VertexType> > vertex_store):
            config_(std::move(config)),
            vertex_store_(std::move(vertex_store)) {}

  // ~BFS() = delete;

  inline bool pullGather(const VertexType& u, VertexType& v, uint64_t id_src,
                                uint64_t id_tgt, const vertex_degree_t& src_degree,
                                const vertex_degree_t& tgt_degree) {
    if (u != UINT32_MAX && v > u + 1) {
      v = u + 1;
      return true;
    }
    return false;
  }

  inline bool pullGatherWeighted(const VertexType& u, VertexType& v, const float weight,
                                        uint64_t id_src, uint64_t id_tgt,
                                        const vertex_degree_t& src_degree,
                                        const vertex_degree_t& tgt_degree) {
    // not applicable
    return false;
  }

  inline bool apply(const uint64_t id, const uint32_t iteration) {
    // pass, nothing to be done here
    uint32_t next = vertex_store_->vertices_.next[id];
    uint32_t current = vertex_store_->vertices_.current[id];
    if (next != current) {
      vertex_store_->vertices_.current[id] = next;
      set_active(vertex_store_->vertices_.active_next, id);

      return false;
    }

    return true;
  }

  inline bool edgeChanged(const Edge& edge, const EdgeChangeEvent& event) {
    switch (event) {
      case EdgeChangeEvent::INSERT: {
        // Only the target could potentially change its component, depending on the current
        // components:
        uint32_t source_value = vertex_store_->vertices_.next[edge.src()];
        // If the source has not been discovered yet, return.
        if (source_value == UINT32_MAX) {
          return false;
        }

        if ((source_value + 1) < vertex_store_->vertices_.next[edge.tgt()]) {
          vertex_store_->vertices_.next[edge.tgt()] = source_value + 1;
          vertex_store_->vertices_.current[edge.tgt()] = source_value + 1;
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

  inline bool isVertexActiveApply(const uint64_t id) {
    return is_active(vertex_store_->vertices_.changed, id);
  }

  inline bool isVertexActiveApplyMultiStep(const uint64_t start_id) {
    return (uint64_t) vertex_store_->vertices_.changed[start_id / 8] != 0;
  }

  inline bool isEdgeActive(const edge_t& edge) {
    return is_active(vertex_store_->vertices_.active_current, edge.src);
  }

  inline bool compareAndSwap(VertexType* ptr, const VertexType& old_value,
                                    const VertexType& new_value) {
    return smp_cas(ptr, old_value, new_value);
  }

  inline bool reduceVertex(VertexType& out, const VertexType& lhs, const VertexType& rhs,
                                  const uint64_t& id_tgt, const vertex_degree_t& degree) {
    if (lhs < rhs) {
      out = lhs;
      return true;
    } else {
      out = rhs;
      return false;
    }
  }

  void init_vertices(void* args) {
    sg_print("Init vertices\n");

    size_t count_vertices = config_->context_.count_vertices;
    double count_active = size_bool_array(count_vertices);

    // Also need to init array for next round, otherwise everything will be 0.
    for (uint32_t i = 0; i < count_vertices; ++i) {
      vertex_store_->vertices_.current[i] = UINT32_MAX;
      vertex_store_->vertices_.next[i] = UINT32_MAX;
    }

    // All vertices inactive in the beginning, for the first round.
    memset(vertex_store_->vertices_.active_current,
           (unsigned char) 0,
           static_cast<size_t>(count_active * sizeof(char)));
    memset(vertex_store_->vertices_.active_next,
           (unsigned char) 0,
           static_cast<size_t>(count_active * sizeof(char)));

    // Set only on vertex to active at the start.
    setStartVertex(config_->context_.algorithm_config.bfs_sssp_start_id);
  }

  void setStartVertex(uint64_t start_id) {
    vertex_store_->vertices_.current[start_id] = 0;
    vertex_store_->vertices_.next[start_id] = 0;

    set_active(vertex_store_->vertices_.active_current, start_id);
  }

  void resetWithoutEdgeApi() {
    size_t count_vertices = config_->context_.count_vertices;
    double count_active = size_bool_array(count_vertices);
    memset(vertex_store_->vertices_.active_current,
           (unsigned char) 255,
           static_cast<size_t>(count_active * sizeof(char)));
  }

  // Reset active current-array for next round.
  void reset_vertices(bool* switchCurrentNext, bool* switchCurrentNextActive) {
    size_t count_vertices = config_->context_.count_vertices;
    double count_active = size_bool_array(count_vertices);
    memset(vertex_store_->vertices_.active_current,
           0x00,
           static_cast<size_t>(count_active * sizeof(char)));
    *switchCurrentNext = false;
    *switchCurrentNextActive = true;
  }

  void init_vertices_local(VertexType* vertex_array,
                                  uint64_t count_vertices) {
    for (uint32_t i = 0; i < count_vertices; ++i) {
      vertex_array[i] = UINT32_MAX;
    }
  }

private:
  std::shared_ptr<Config> config_;

  std::shared_ptr<VertexStore<VertexType> > vertex_store_;
};
}
}

int main(int argc, char** argv) {
  // calculate number of jobs
  const int num_of_args = 80; // num of args for each job (including options)
  if((argc - 1) % num_of_args != 0) {
    sg_err("Wrong number of arguments, %d, expected multiple of : %d \n",
           argc,
           num_of_args);
  }
  int num_of_jobs = (argc - 1) / num_of_args;
  sg_log("Number of jobs: %d\n", num_of_jobs);

  // malloc arrays of args for each job
  const int MAX_ARG_LEN = 100;
  std::vector<std::thread> threads;
  for(int i = 0; i < num_of_jobs; i++) {
    char ** args_for_this_job = new char*[num_of_args + 1]; // include program's name
    args_for_this_job[0] = new char[MAX_ARG_LEN + 1];
    strcpy(args_for_this_job[0], argv[0]);

    for (int j = 1; j <= num_of_args; j++) {
      args_for_this_job[j] = new char[MAX_ARG_LEN + 1];
      strcpy(args_for_this_job[j], argv[i*num_of_args+j]);
    }

    threads.emplace_back([&](){evolving_graphs::execute<uint32_t, evolving_graphs::algorithm::BFS, false>(num_of_args + 1, args_for_this_job); });
    // evolving_graphs::execute<uint32_t, evolving_graphs::algorithm::CC, false>(num_of_args + 1, args_for_this_job);
  }

  for(auto& t: threads)
  {
      t.join();

  }

  return 0;
}
