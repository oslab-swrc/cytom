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

#include <sys/mman.h>
#include <getopt.h>
#include <errno.h>
#include <sys/stat.h>
#include "util/datatypes.h"
#include "util/util.h"

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

  CC() = delete;

  ~CC() = delete;

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
    // pass, nothing to be done here
    if (VertexStore<uint32_t>::vertices_.next[id] !=
        VertexStore<uint32_t>::vertices_.current[id]) {
      VertexStore<uint32_t>::vertices_.current[id] = VertexStore<uint32_t>::vertices_.next[id];
      set_active(VertexStore<uint32_t>::vertices_.active_next, id);
      return false;
    }

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

          set_active(VertexStore<uint32_t>::vertices_.active_current, edge.tgt());
          set_active_atomically(VertexStore<VertexType>::vertices_.changed, edge.tgt());
        }
        return false;
      }
      case EdgeChangeEvent::DELETE:
        if (VertexStore<uint32_t>::vertices_.critical_neighbor[edge.tgt()] == edge.src()) {
          return true;
        }
        return false;
    }
  }

  static inline bool isVertexActiveApply(const uint64_t id) {
    return is_active(VertexStore<uint32_t>::vertices_.changed, id);
  }

  static inline bool isVertexActiveApplyMultiStep(const uint64_t start_id) {
    return (uint64_t) VertexStore<VertexType>::vertices_.changed[start_id / 8] != 0;
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
      return true;
    } else {
      out = rhs;
      return false;
    }
    //      if (lhs < rhs) {
    //        set_active(active_array, id_tgt);
    //      }
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
    memset(VertexStore<VertexType>::vertices_.active_current,
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

  static bool check_convergence(uint64_t id) {
    return !is_active(VertexStore<uint32_t>::vertices_.active_next, id);
  }

  static bool check_convergence() {
    // Simply check if all vertices are now converged.
    bool return_value = true;
    size_t count_vertices = Config::context_.count_vertices;
    for (uint64_t i = 0; i < count_vertices; ++i) {
      if (!check_convergence(i)) {
        return_value = false;
      } else {
        if (Config::context_.enable_adaptive_scheduling) {
          // Copy over values as we won't touch them otherwise.
          VertexStore<uint32_t>::vertices_.next[i] = VertexStore<uint32_t>::vertices_.current[i];
        }
      }
    }
    // Report on convergence result.
    return return_value;
  }
};
}
}

file_edge_t* global_file_edges;
size_t global_file_size;

int main(int argc, char** argv) {
  bool is_binary_input = false;
  std::string binary_input_file;

  for (int arg_cnt = 0; true; ++arg_cnt) {
    int c, idx = 0;
    c = getopt_long(argc,
                    argv,
                    "a:b:c:d:e:f:g:h:i:j:k:l:m:n:o:p:q:r:s:t:u:v:w:x:y:z:A:B:C:D:E:F:G:H:I:J:K:L:M:N:",
                    evolving_graphs::options,
                    &idx);
    if (c == -1) {
      break;
    }

    switch (c) {
      case 's': {
        std::string generator = std::string(optarg);
        std::transform(generator.begin(),
                       generator.end(),
                       generator.begin(),
                       ::tolower);
        if (generator == "binary") {
          is_binary_input = true;
        } 
        break;
      }
      case 't':
        binary_input_file = std::string(optarg);
        break;
      default:
        break;
    }
  }

  optind = 1; // reset optint to repeat getopt

  if(is_binary_input) {
    // mmap file to memory
    int fd = open(binary_input_file.c_str(), O_RDONLY);
    struct stat file_stats{};
    fstat(fd, &file_stats);
    global_file_size = static_cast<size_t>(file_stats.st_size);

    global_file_edges = (file_edge_t*) mmap(nullptr, global_file_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (global_file_edges == MAP_FAILED) {
      sg_err("Could not open file %s: %d\n", binary_input_file.c_str(), errno);
      return 1;
    }
  }

  evolving_graphs::execute<uint32_t, evolving_graphs::algorithm::CC, false>(argc, argv);
  return 0;
}
