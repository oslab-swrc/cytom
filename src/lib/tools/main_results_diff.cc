#include <cmath>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <time.h>
#include <sys/mman.h>
#include <pthread.h>
#include <getopt.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <map>

#include "util/datatypes.h"
#include "util/util.h"
#include "util/config.h"
#include "core/meta-tile-store.h"
#include "core/vertex-store.h"
#include "edge-inserter/edge-inserter.h"
#include "edge-inserter/rmat-edge-provider.h"
#include "core/tile-store.h"
#include "perf-event/perf-event-manager.h"
#include "traversal/traversal.h"
#include "traversal/hilbert.h"
#include "traversal/row_first.h"
#include "traversal/column_first.h"
#include "edge-inserter/binary-edge-provider.h"
#include "core/continuous-algorithm-executor.h"

#include <flatbuffers/flatbuffers.h>

using namespace evolving_graphs;

struct result_options_t {
  std::string result_1;
  std::string result_2;
  std::string output_file;
  uint64_t count_comparisons;
};

static int parseOption(int argc, char* argv[], result_options_t* result_options) {
  static struct option options[] = {
      {"result-1",          required_argument, nullptr, 'a'},
      {"result-2",          required_argument, nullptr, 'b'},
      {"output-file",       required_argument, nullptr, 'c'},
      {"count-comparisons", required_argument, nullptr, 'd'},
      {nullptr, 0,                             nullptr, 0},
  };
  int arg_cnt;

  for (arg_cnt = 0; true; ++arg_cnt) {
    int c, idx = 0;
    c = getopt_long(argc, argv, "a:b:c:d:", options, &idx);
    if (c == -1) {
      break;
    }

    switch (c) {
      case 'a':
        result_options->result_1 = std::string(optarg);
        break;
      case 'b':
        result_options->result_2 = std::string(optarg);
        break;
      case 'c':
        result_options->output_file = std::string(optarg);
        break;
      case 'd':
        result_options->count_comparisons = std::stoul(std::string(optarg));
        break;
      default:
        return -EINVAL;
    }
  }
  return arg_cnt;
}

std::vector<vertex_value_type_t> readVertexValues(const std::string& file) {
  std::vector<vertex_value_type_t> vertex_values;

  vertex_value_type_t* file_vertices_values = nullptr;

  // Open file in case of reading binary input.
  int fd = open(file.c_str(), O_RDONLY);
  struct stat file_stats{};
  fstat(fd, &file_stats);
  auto file_size = static_cast<size_t>(file_stats.st_size);

  file_vertices_values =
      (vertex_value_type_t*) mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (file_vertices_values == MAP_FAILED) {
    sg_err("Could not open file %s: %d\n", file.c_str(), errno);
    util::die(1);
  }
  uint64_t count_vertices = file_size / sizeof(vertex_value_type_t);

  for (uint32_t i = 0; i < count_vertices; ++i) {
    vertex_value_type_t value = file_vertices_values[i];
    vertex_values.push_back(value);
  }

  return vertex_values;
}

void compareVertexValues(const result_options_t& options,
                         const std::vector<vertex_value_type_t>& lhs,
                         const std::vector<vertex_value_type_t>& rhs) {
  std::map<uint32_t, uint32_t> lhs_rank_map;
  std::map<uint32_t, uint32_t> rhs_rank_map;

  for (uint32_t i = 0; i < options.count_comparisons; ++i) {
    lhs_rank_map[lhs[i].vertex_id] = i;
    rhs_rank_map[rhs[i].vertex_id] = i;
  }

  double global_diff = 0;

  for (auto const& value_lhs : lhs_rank_map) {
    auto value_rhs = rhs_rank_map.find(value_lhs.first);

    if (value_rhs != rhs_rank_map.end()) {
      double diff = (value_lhs.second - (double) value_rhs->second);
      global_diff += std::abs(diff);
    }
  }

  sg_log("Global diff: %f\n", global_diff);
}

void writeVertexValues(const result_options_t& options,
                       const std::vector<vertex_value_type_t>& lhs,
                       const std::vector<vertex_value_type_t>& rhs) {
  std::map<uint32_t, uint32_t> lhs_rank_map;
  std::map<uint32_t, uint32_t> rhs_rank_map;

  uint64_t max = std::min(lhs.size(), std::min(rhs.size(), 16 * options.count_comparisons));

  for (uint32_t i = 0; i < max; ++i) {
    vertex_value_type_t lhs_value = lhs[i];
    vertex_value_type_t rhs_value = rhs[i];

//    sg_log("%u lhs: %u (%f), rhs: %u (%f)\n",
//           i,
//           lhs_value.vertex_id,
//           lhs_value.vertex_value,
//           rhs_value.vertex_id,
//           rhs_value.vertex_value);
    lhs_rank_map[lhs[i].vertex_id] = i;
    rhs_rank_map[rhs[i].vertex_id] = i;
  }

  std::ofstream out(options.output_file);

  for (uint32_t i = 0; i < options.count_comparisons; ++i) {
    vertex_value_type_t lhs_value = lhs[i];
    vertex_value_type_t rhs_value = rhs[i];

//    auto value_rhs = rhs_rank_map.find(lhs_value.vertex_id);

//    if (value_rhs != rhs_rank_map.end()) {
//      auto value_lhs = lhs_rank_map.find(rhs_value.vertex_id);
//      if (value_lhs != lhs_rank_map.end()) {
        out << lhs_value.vertex_id << " " << rhs_value.vertex_id << std::endl;
//      }
//    }
  }

  out.close();
}

int main(int argc, char** argv) {
  // Parse command line options.
  result_options_t options;
  int count_expected_arguments = 4;
  int count_parsed_arguments = parseOption(argc, argv, &options);

  if (count_parsed_arguments != count_expected_arguments) {
    sg_err("Wrong number of arguments, %d, expected count: %d \n",
           count_parsed_arguments,
           count_expected_arguments);
    return 1;
  }

  std::vector<vertex_value_type_t> vertex_values_1 = readVertexValues(options.result_1);
  std::vector<vertex_value_type_t> vertex_values_2 = readVertexValues(options.result_2);

  writeVertexValues(options, vertex_values_1, vertex_values_2);

  return 0;
}

