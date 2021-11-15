// SPDX-License-Identifier: MIT

#include <iostream>
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

// Convergence threshold.
#define EPSILON 0.01

// Threshold for changing vertex value.
#define DELTA 0.01

namespace evolving_graphs {
namespace algorithm {

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

/* Add by LLVM Pass*/
struct VertexType_t {
  PageRankDeltaVertexType_t first;
  PageRankDeltaVertexType_t second;

  bool operator==(const VertexType_t& other) {
    return (first == other.first && second == other.second);
  }

  bool operator!=(const VertexType_t& other) {
    return (first != other.first || second != other.second);
  }
};

class PageRankDelta {
public:
  /* add/Modify by llvm Pass */
  typedef VertexType_t VertexType;

  // Do not allow instantiation, will only be used via static, inlined functions.
  PageRankDelta(std::shared_ptr<Config> config, std::shared_ptr<VertexStore<VertexType> > vertex_store);

  // ~PageRankDelta() = delete;

  /* Modify by llvm Pass */
  constexpr const static PageRankDeltaVertexType_t
      neutral_element_temp = PageRankDeltaVertexType_t{static_cast<float>(0.),
                                                  static_cast<float>(0.),
                                                  static_cast<float>(0.)};

  /* add/Modify by llvm Pass */
  constexpr const static VertexType_t
      neutral_element = VertexType_t{neutral_element_temp, neutral_element_temp};

  static const bool NEEDS_TARGETS_ACTIVE = false;

  static const bool NEEDS_SOURCES_ACTIVE = true;

  static const bool ASYNC = false;

  static const bool USE_CHANGED = true;

  static const int ASYNC_CHECK_CONVERGENCE_INTERVAL = 1;

  // static bool GLOBAL_FIRST_ITERATION;

  bool pullGather(const VertexType& u, VertexType& v, uint64_t id_src,
                                uint64_t id_tgt, const vertex_degree_t& src_degree,
                                const vertex_degree_t& tgt_degree);

  bool pullGatherWeighted(const VertexType& u, VertexType& v, const float weight,
                                        uint64_t id_src, uint64_t id_tgt,
                                        const vertex_degree_t& src_degree,
                                        const vertex_degree_t& tgt_degree);

  bool apply(const uint64_t id, const uint32_t iteration);
  bool apply_1(const uint64_t id, const uint32_t iteration);
  bool apply_2(const uint64_t id, const uint32_t iteration);

  bool edgeChanged(const Edge& edge, const EdgeChangeEvent& event);

  bool isVertexActiveApply(const uint64_t id);

  bool isVertexActiveApplyMultiStep(const uint64_t start_id);

  bool isEdgeActive(const edge_t& edge);

  bool compareAndSwap(VertexType* ptr, const VertexType& old_value,
                                    const VertexType& new_value);
  bool compareAndSwap_1(VertexType* ptr, const VertexType& old_value,
                                    const VertexType& new_value);
  bool compareAndSwap_2(VertexType* ptr, const VertexType& old_value,
                                    const VertexType& new_value);

  bool reduceVertex(VertexType& out, const VertexType& lhs, const VertexType& rhs,
                                  const uint64_t& id_tgt, const vertex_degree_t& degree);

  void init_vertices(void* args);

  void resetWithoutEdgeApi();

  // reset current-array for next round
  void reset_vertices(bool* switchCurrentNext, bool* switchCurrentNextActive);

  void init_vertices_local(VertexType* vertex_array,
                                  uint64_t count_vertices);

private:
  std::shared_ptr<Config> config_;

  std::shared_ptr<VertexStore<VertexType> > vertex_store_;

  bool GLOBAL_FIRST_ITERATION;
};

// bool PageRankDelta::GLOBAL_FIRST_ITERATION = true;

//inline std::ostream& operator<<(std::ostream& os, const PageRankDeltaVertexType_t& v) {
//  os << v.rank << " " << v.delta << " " << v.delta_sum;
//  return os;
//}


PageRankDelta::PageRankDelta(std::shared_ptr<Config> config, std::shared_ptr<VertexStore<VertexType> > vertex_store):
            config_(std::move(config)),
            vertex_store_(std::move(vertex_store)) { GLOBAL_FIRST_ITERATION = true; 
}

bool PageRankDelta::pullGather(const VertexType& u, VertexType& v, uint64_t id_src,
                              uint64_t id_tgt, const vertex_degree_t& src_degree,
                              const vertex_degree_t& tgt_degree) {
  /* add/Modify by llvm Pass */
  v.first.delta_sum = v.first.delta_sum + (u.first.delta / src_degree.out_degree);
  v.second.delta_sum = v.first.delta_sum;
  return false;
}

bool PageRankDelta::pullGatherWeighted(const VertexType& u, VertexType& v, const float weight,
                                      uint64_t id_src, uint64_t id_tgt,
                                      const vertex_degree_t& src_degree,
                                      const vertex_degree_t& tgt_degree) {
  // not applicable
  return false;
}

bool PageRankDelta::apply(const uint64_t id, const uint32_t iteration) {
  float delta_sum = vertex_store_->vertices_.next[id].first.delta_sum;
  if (unlikely(GLOBAL_FIRST_ITERATION)) {
    vertex_store_->vertices_.next[id].first.delta =
        static_cast<float>((1. - ALPHA) + (ALPHA * delta_sum));
    vertex_store_->vertices_.next[id].first.rank +=
        vertex_store_->vertices_.next[id].first.delta;

    vertex_store_->vertices_.next[id].first.delta -= 1.0;

    /* add/Modify by llvm Pass */
    vertex_store_->vertices_.next[id].second.delta = vertex_store_->vertices_.next[id].first.delta;
    vertex_store_->vertices_.next[id].second.rank = vertex_store_->vertices_.next[id].first.rank;
  } else {
    if (delta_sum == 0.0) {
      vertex_store_->vertices_.current[id].first.delta = 0;
      vertex_store_->vertices_.next[id].first.delta = 0;

      /* add/Modify by llvm Pass */
      vertex_store_->vertices_.current[id].second.delta = 0;
      vertex_store_->vertices_.next[id].second.delta = 0;
      return true;
    }

    vertex_store_->vertices_.next[id].first.delta = static_cast<float>(ALPHA * delta_sum);
    vertex_store_->vertices_.next[id].first.rank +=
        vertex_store_->vertices_.next[id].first.delta;

    /* add/Modify by llvm Pass */
    vertex_store_->vertices_.next[id].second.delta = vertex_store_->vertices_.next[id].first.delta;
    vertex_store_->vertices_.next[id].second.rank = vertex_store_->vertices_.next[id].first.rank;
  }

  vertex_store_->vertices_.current[id].first.delta =
      vertex_store_->vertices_.next[id].first.delta;
  vertex_store_->vertices_.next[id].first.delta_sum = static_cast<float>(0.);

  /* add/Modify by llvm Pass */
  vertex_store_->vertices_.current[id].second.delta =
      vertex_store_->vertices_.current[id].first.delta;
  vertex_store_->vertices_.next[id].second.delta_sum = vertex_store_->vertices_.next[id].first.delta_sum;

  const double delta = config_->context_.algorithm_config.delta;
  if (std::abs(vertex_store_->vertices_.next[id].first.delta) >
      delta * vertex_store_->vertices_.next[id].first.rank) {
    set_active(vertex_store_->vertices_.active_next, id); // TODO
    return false;
  }
  return true;
}

// bool PageRankDelta::apply_2(const uint64_t id, const uint32_t iteration) {
//   float delta_sum = vertex_store_->vertices_.next[id].second.delta_sum;
//   if (unlikely(GLOBAL_FIRST_ITERATION)) {
//     vertex_store_->vertices_.next[id].second.delta =
//         static_cast<float>((1. - ALPHA) + (ALPHA * delta_sum));
//     vertex_store_->vertices_.next[id].second.rank +=
//         vertex_store_->vertices_.next[id].second.delta;

//     vertex_store_->vertices_.next[id].second.delta -= 1.0;
//   } else {
//     if (delta_sum == 0.0) {
//       vertex_store_->vertices_.current[id].second.delta = 0;
//       vertex_store_->vertices_.next[id].second.delta = 0;
//       return true;
//     }

//     vertex_store_->vertices_.next[id].second.delta = static_cast<float>(ALPHA * delta_sum);
//     vertex_store_->vertices_.next[id].second.rank +=
//         vertex_store_->vertices_.next[id].second.delta;
//   }

//   vertex_store_->vertices_.current[id].second.delta =
//       vertex_store_->vertices_.next[id].second.delta;
//   vertex_store_->vertices_.next[id].second.delta_sum = static_cast<float>(0.);

//   const double delta = config_->context_.algorithm_config.delta;
//   if (std::abs(vertex_store_->vertices_.next[id].second.delta) >
//       delta * vertex_store_->vertices_.next[id].second.rank) {
//     set_active(vertex_store_->vertices_.active_next, id);
//     return false;
//   }
//   return true;
// }

/* add/Modify by llvm Pass */
bool PageRankDelta::edgeChanged(const Edge& edge, const EdgeChangeEvent& event) {
  switch (event) {
    case EdgeChangeEvent::INSERT: {
      // Check if the change in delta would be significant enough:
      float delta = vertex_store_->vertices_.next[edge.src()].first.rank /
                    vertex_store_->vertices_.degrees[edge.src()].out_degree;

      vertex_store_->vertices_.next[edge.tgt()].first.delta_sum += delta;
      vertex_store_->vertices_.next[edge.tgt()].second.delta_sum = vertex_store_->vertices_.next[edge.tgt()].first.delta_sum;

      const double algo_delta = config_->context_.algorithm_config.delta;
      if (delta > algo_delta * vertex_store_->vertices_.next[edge.tgt()].first.rank) {
        set_active_atomically(vertex_store_->vertices_.active_next, edge.src());
        set_active_atomically(vertex_store_->vertices_.changed, edge.src());
      }
      return false;
    }
    case EdgeChangeEvent::DELETE: {
      float delta = vertex_store_->vertices_.next[edge.src()].first.rank /
                    vertex_store_->vertices_.degrees[edge.src()].out_degree;

      vertex_store_->vertices_.next[edge.tgt()].first.delta_sum -= delta;
      vertex_store_->vertices_.next[edge.tgt()].second.delta_sum = vertex_store_->vertices_.next[edge.tgt()].first.delta_sum;

      const double algo_delta = config_->context_.algorithm_config.delta;
      if (delta > algo_delta * vertex_store_->vertices_.next[edge.tgt()].first.rank) {
        set_active_atomically(vertex_store_->vertices_.active_next, edge.src());
        set_active_atomically(vertex_store_->vertices_.changed, edge.src());
        return true;
      }
      return false;
    }
  }
}

bool PageRankDelta::isVertexActiveApply(const uint64_t id) {
  if (GLOBAL_FIRST_ITERATION) {
    return true;
  }
  if (unlikely(!config_->context_.enable_adaptive_scheduling)) {
    return true;
  }
  return eval_bool_array(vertex_store_->vertices_.changed, id);
}

bool PageRankDelta::isVertexActiveApplyMultiStep(const uint64_t start_id) {
  if (unlikely(GLOBAL_FIRST_ITERATION)) {
    return true;
  }
  if (unlikely(!config_->context_.enable_adaptive_scheduling)) {
    return true;
  }
  return (uint64_t) vertex_store_->vertices_.changed[start_id / 8] != 0;
}

bool PageRankDelta::isEdgeActive(const edge_t& edge) {
  return is_active(vertex_store_->vertices_.active_current, edge.src);
}

bool PageRankDelta::compareAndSwap(VertexType* ptr, const VertexType& old_value,
                                  const VertexType& new_value) {
  return compareAndSwap_1(ptr, old_value, new_value) && compareAndSwap_2(ptr, old_value, new_value);
}

bool PageRankDelta::compareAndSwap_1(VertexType* ptr, const VertexType& old_value,
                                  const VertexType& new_value) {
  return smp_cas_float(ptr, old_value.first.delta_sum, new_value.first.delta_sum);
}

bool PageRankDelta::compareAndSwap_2(VertexType* ptr, const VertexType& old_value,
                                  const VertexType& new_value) {
  return smp_cas_float(&(ptr->second), old_value.second.delta_sum, new_value.second.delta_sum);
}

bool PageRankDelta::reduceVertex(VertexType& out, const VertexType& lhs, const VertexType& rhs,
                                const uint64_t& id_tgt, const vertex_degree_t& degree) {
  out.first.delta_sum = lhs.first.delta_sum + rhs.first.delta_sum;
  out.second.delta_sum = out.first.delta_sum;
  return false;
//    out.delta = rhs.delta;
//    out.rank = rhs.rank;
}

void PageRankDelta::init_vertices(void* args) {
  sg_print("Init vertices\n");

  GLOBAL_FIRST_ITERATION = true;

  size_t count_vertices = config_->context_.count_vertices;
  double count_active = size_bool_array(count_vertices);

  memset(vertex_store_->vertices_.next, 0, sizeof(VertexType) * count_vertices);
  memset(vertex_store_->vertices_.current, 0, sizeof(VertexType) * count_vertices);

  for (uint64_t i = 0; i < count_vertices; ++i) {
    vertex_store_->vertices_.current[i].first.delta = static_cast<float>(1.);
    vertex_store_->vertices_.next[i].first.delta = static_cast<float>(1.);
    vertex_store_->vertices_.current[i].second.delta = static_cast<float>(1.);
    vertex_store_->vertices_.next[i].second.delta = static_cast<float>(1.);
  }

  memset(vertex_store_->vertices_.temp_next, 0, sizeof(VertexType) * count_vertices);
  // all vertices active in the beginning
  memset(vertex_store_->vertices_.active_current,
         (unsigned char) 255,
         static_cast<size_t>(count_active * sizeof(char)));
  memset(vertex_store_->vertices_.active_next,
         0,
         static_cast<size_t>(count_active * sizeof(char)));
}

void PageRankDelta::resetWithoutEdgeApi() {
  // Set everything active.
  size_t count_vertices = config_->context_.count_vertices;
  double count_active = size_bool_array(count_vertices);
  memset(vertex_store_->vertices_.active_current,
         (unsigned char) 255,
         static_cast<size_t>(count_active * sizeof(char)));
  // Force new iteration.
  GLOBAL_FIRST_ITERATION = true;
  for (uint64_t i = 0; i < count_vertices; ++i) {
    vertex_store_->vertices_.current[i].first.delta = static_cast<float>(1.);
    vertex_store_->vertices_.next[i].first.delta = static_cast<float>(1.);
    vertex_store_->vertices_.current[i].second.delta = static_cast<float>(1.);
    vertex_store_->vertices_.next[i].second.delta = static_cast<float>(1.);
  }
}

void PageRankDelta::reset_vertices(bool* switchCurrentNext, bool* switchCurrentNextActive) {
  sg_print("Resetting vertices for next round\n");
  if (unlikely(GLOBAL_FIRST_ITERATION)) {
    GLOBAL_FIRST_ITERATION = false;
  }

  size_t count_vertices = config_->context_.count_vertices;
  double count_active = size_bool_array(count_vertices);

  // Reset active status for the next round.
  memset(vertex_store_->vertices_.active_current,
         0x00,
         static_cast<size_t>(count_active * sizeof(char)));

  *switchCurrentNext = false;
  *switchCurrentNextActive = true;
}

void PageRankDelta::init_vertices_local(VertexType* vertex_array,
                                uint64_t count_vertices) {
  memset(vertex_array, 0, sizeof(VertexType) * count_vertices);
}

} // end of algortihm
} // end of evolving_graphs

int main(int argc, char** argv) {
  // calculate number of jobs
  // const int num_of_args = 80; // num of args for each job (including options)
  // if((argc - 1) % num_of_args != 0) {
  //   sg_err("Wrong number of arguments, %d, expected multiple of : %d \n",
  //          argc,
  //          num_of_args);
  // }
  // int num_of_jobs = (argc - 1) / num_of_args;
  // sg_log("Number of jobs: %d\n", num_of_jobs);

  // // malloc arrays of args for each job
  // const int MAX_ARG_LEN = 100;
  // std::vector<std::thread> threads;
  // for(int i = 0; i < num_of_jobs; i++) {
  //   char ** args_for_this_job = new char*[num_of_args + 1]; // include program's name
  //   args_for_this_job[0] = new char[MAX_ARG_LEN + 1];
  //   strcpy(args_for_this_job[0], argv[0]);

  //   for (int j = 1; j <= num_of_args; j++) {
  //     args_for_this_job[j] = new char[MAX_ARG_LEN + 1];
  //     strcpy(args_for_this_job[j], argv[i*num_of_args+j]);
  //   }

  //   threads.emplace_back([&](){evolving_graphs::execute<PageRankDeltaVertexType_t, evolving_graphs::algorithm::PageRankDelta, false>(num_of_args + 1, args_for_this_job); });
  //   // evolving_graphs::execute<PageRankDeltaVertexType_t, evolving_graphs::algorithm::PageRankDelta, false>(num_of_args + 1, args_for_this_job);
  // }

  // for(auto& t: threads)
  // {
  //     t.join();

  // }

  evolving_graphs::execute<evolving_graphs::algorithm::VertexType_t, evolving_graphs::algorithm::PageRankDelta, false>(argc, argv);

  return 0;
}
