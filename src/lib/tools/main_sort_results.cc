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

struct sort_options_t {
  std::string input_results;
  std::string output_results;
  algorithm_t algorithm;
  bool sort_ascending;
};

static int parseOption(int argc, char* argv[], sort_options_t* sort_options) {
  static struct option options[] = {
      {"input-results",  required_argument, nullptr, 'a'},
      {"output-results", required_argument, nullptr, 'b'},
      {"sort-ascending", required_argument, nullptr, 'c'},
      {"algorithm",      required_argument, nullptr, 'd'},
      {nullptr, 0,                          nullptr, 0},
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
        sort_options->input_results = std::string(optarg);
        break;
      case 'b':
        sort_options->output_results = std::string(optarg);
        break;
      case 'c':
        sort_options->sort_ascending = std::stoi(std::string(optarg)) == 1;
        break;
      case 'd': {
        std::string algorithm = std::string(optarg);
        std::transform(algorithm.begin(), algorithm.end(), algorithm.begin(), ::tolower);
        if (algorithm == "pagerank") {
          sort_options->algorithm = algorithm_t::PAGERANK;
        } else if (algorithm == "pagerankdelta" || algorithm == "pagerank-delta") {
          sort_options->algorithm = algorithm_t::PAGERANK_DELTA;
        } else if (algorithm == "connected-components") {
          sort_options->algorithm = algorithm_t::CONNECTED_COMPONENTS;
        } else if (algorithm == "connected-components-async") {
          sort_options->algorithm = algorithm_t::CONNECTED_COMPONENTS_ASYNC;
        } else if (algorithm == "bfs") {
          sort_options->algorithm = algorithm_t::BFS;
        } else if (algorithm == "bfs-async") {
          sort_options->algorithm = algorithm_t::BFS_ASYNC;
        } else if (algorithm == "sssp") {
          sort_options->algorithm = algorithm_t::SSSP;
        } else if (algorithm == "sssp-async") {
          sort_options->algorithm = algorithm_t::SSSP_ASYNC;
        } else {
          sg_log("Wrong option given for algorithm: %s\n", algorithm.c_str());
          return -EINVAL;
        }
        break;
      }
      default:
        return -EINVAL;
    }
  }
  return arg_cnt;
}

std::vector<vertex_value_type_t> readVertexValues(const sort_options_t& options) {
  std::vector<vertex_value_type_t> vertex_values;

  PageRankDeltaVertexType_t* file_vertices = nullptr;

  // Open file in case of reading binary input.
  int fd = open(options.input_results.c_str(), O_RDONLY);
  struct stat file_stats{};
  fstat(fd, &file_stats);
  auto file_size = static_cast<size_t>(file_stats.st_size);

  file_vertices =
      (PageRankDeltaVertexType_t*) mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (file_vertices == MAP_FAILED) {
    sg_err("Could not open file %s: %d\n", options.input_results.c_str(), errno);
    util::die(1);
  }
  uint64_t count_vertices = file_size / sizeof(PageRankDeltaVertexType_t);

  for (uint32_t i = 0; i < count_vertices; ++i) {
    vertex_value_type_t value{};
    value.vertex_id = i;
    value.vertex_value = file_vertices[i].rank;
    if (value.vertex_value > 0.1500001) {
      vertex_values.push_back(value);
    }
  }

  return vertex_values;
}

std::vector<vertex_value_type_t> readSSSPVertexValues(const sort_options_t& options) {
  std::vector<vertex_value_type_t> vertex_values;

  float* file_vertices = nullptr;

  // Open file in case of reading binary input.
  int fd = open(options.input_results.c_str(), O_RDONLY);
  struct stat file_stats{};
  fstat(fd, &file_stats);
  auto file_size = static_cast<size_t>(file_stats.st_size);

  file_vertices = (float*) mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (file_vertices == MAP_FAILED) {
    sg_err("Could not open file %s: %d\n", options.input_results.c_str(), errno);
    util::die(1);
  }
  uint64_t count_vertices = file_size / sizeof(float);

  for (uint32_t i = 0; i < count_vertices; ++i) {
    vertex_value_type_t value{};
    value.vertex_id = i;
    value.vertex_value = file_vertices[i];
    vertex_values.push_back(value);
  }

  return vertex_values;
}

void writeVertexValues(const std::vector<vertex_value_type_t>& values,
                       const sort_options_t& options) {
  int fd = open(options.output_results.c_str(), O_WRONLY | O_CREAT | O_SYNC, 0755);
  if (fd == -1) {
    sg_err("Unable to open file %s: %s\n", options.output_results.c_str(), strerror(errno));
    util::die(1);
  }

  // Write the current output array.
  size_t size_to_write = sizeof(vertex_value_type_t) * values.size();
  if (write(fd, values.data(), size_to_write) != size_to_write) {
    sg_err("Fail to write to file %s: %s\n", options.output_results.c_str(), strerror(errno));
    util::die(1);
  }

  // Done writing, close file.
  int err = close(fd);
  if (err != 0) {
    sg_err("File %s couldn't be written: %s\n", options.output_results.c_str(), strerror(errno));
    util::die(1);
  }
}

int main(int argc, char** argv) {
  // Parse command line options.
  sort_options_t options;
  int count_expected_arguments = 4;
  int count_parsed_arguments = parseOption(argc, argv, &options);

  if (count_parsed_arguments != count_expected_arguments) {
    sg_err("Wrong number of arguments, %d, expected count: %d \n",
           count_parsed_arguments,
           count_expected_arguments);
    return 1;
  }
  std::vector<vertex_value_type_t> vertex_values;
  switch (options.algorithm) {
    case algorithm_t::PAGERANK:
    case algorithm_t::PAGERANK_DELTA:
      vertex_values = readVertexValues(options);
      break;
    case algorithm_t::SSSP:
      vertex_values = readSSSPVertexValues(options);
      break;
    default:
      sg_err("Algorithm not implemented: %d\n",
             static_cast<std::underlying_type<algorithm_t>::type>(options.algorithm));
      util::die(1);
  }

  if (options.sort_ascending) {
    std::sort(vertex_values.begin(), vertex_values.end());
  } else {
    std::sort(vertex_values.rbegin(), vertex_values.rend());
  }

  sg_log("Value size: %lu\n", vertex_values.size());
  for (int i = 0; i < 10; ++i) {
    sg_log("Value at %d: %f, %u\n", i, vertex_values[i].vertex_value, vertex_values[i].vertex_id);
  }

  writeVertexValues(vertex_values, options);

  return 0;
}

