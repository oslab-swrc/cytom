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
#include <unordered_map>

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

struct rewrite_options_t {
  std::string input_graph;
  std::string output_graph;
};

static int parseOption(int argc, char* argv[], rewrite_options_t* rewrite_options) {
  static struct option options[] = {
      {"input-graph",  required_argument, nullptr, 'a'},
      {"output-graph", required_argument, nullptr, 'b'},
      {nullptr, 0,                        nullptr, 0},
  };
  int arg_cnt;

  for (arg_cnt = 0; true; ++arg_cnt) {
    int c, idx = 0;
    c = getopt_long(argc, argv, "a:b:", options, &idx);
    if (c == -1) {
      break;
    }

    switch (c) {
      case 'a':
        rewrite_options->input_graph = std::string(optarg);
        break;
      case 'b':
        rewrite_options->output_graph = std::string(optarg);
        break;
      default:
        return -EINVAL;
    }
  }
  return arg_cnt;
}

file_edge_t* createVertexMapping(const rewrite_options_t& options) {
  std::unordered_map<uint64_t, uint64_t> old_to_new_vertex_ids;
  uint64_t current_id = 0;

  int fd = open(options.input_graph.c_str(), O_RDONLY);
  struct stat file_stats{};
  fstat(fd, &file_stats);
  auto file_size = static_cast<size_t>(file_stats.st_size);

  auto file_edges = (file_edge_t*) mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (file_edges == MAP_FAILED) {
    sg_err("Could not open file %s: %d\n", options.input_graph.c_str(), errno);
    util::die(1);
  }
  uint64_t count_edges = file_size / sizeof(file_edge_t);

  auto* new_edges = new file_edge_t[count_edges];

  for (uint64_t i = 0; i < count_edges; ++i) {
    file_edge_t edge = file_edges[i];
    file_edge_t new_edge{};
    auto find_src = old_to_new_vertex_ids.find(edge.src);
    auto find_tgt = old_to_new_vertex_ids.find(edge.tgt);

    if (find_src != old_to_new_vertex_ids.end()) {
      new_edge.src = find_src->second;
    } else {
      old_to_new_vertex_ids[edge.src] = current_id;
      new_edge.src = current_id;
      ++current_id;
    }

    if (find_tgt != old_to_new_vertex_ids.end()) {
      new_edge.tgt = find_tgt->second;
    } else {
      old_to_new_vertex_ids[edge.tgt] = current_id;
      new_edge.tgt = current_id;
      ++current_id;
    }

    new_edges[i] = new_edge;
  }
  close(fd);

  sg_log("Unique vertices: %lu\n", old_to_new_vertex_ids.size());
  sg_log("Count edges: %lu\n", count_edges);

  return new_edges;
}

void writeNewEdges(const rewrite_options_t& options, file_edge_t* new_edges) {
  int fd = open(options.input_graph.c_str(), O_RDONLY);
  int fd_new = open(options.output_graph.c_str(), O_RDWR | O_CREAT, (mode_t) 0777);
  struct stat file_stats{};
  fstat(fd, &file_stats);
  auto file_size = static_cast<size_t>(file_stats.st_size);
  uint64_t count_edges = file_size / sizeof(file_edge_t);

  size_t size_to_write = count_edges * sizeof(file_edge_t);
  size_t individual_size_to_write = PAGE_SIZE * sizeof(file_edge_t);

  file_edge_t* edges_pointer = new_edges;

  while (size_to_write > 0) {
    size_t current_write = std::min(individual_size_to_write, size_to_write);
    size_to_write -= current_write;

    int ret = write(fd_new, edges_pointer, current_write);
    if (ret != current_write) {
      sg_err("Could not write to %s: %d, %s\n",
             options.output_graph.c_str(),
             errno,
             strerror(errno));
      util::die(1);
    }

    edges_pointer = &edges_pointer[PAGE_SIZE];
  }


  close(fd);
  close(fd_new);
}

int main(int argc, char** argv) {
  // Parse command line options.
  rewrite_options_t options;
  int count_expected_arguments = 2;
  int count_parsed_arguments = parseOption(argc, argv, &options);

  if (count_parsed_arguments != count_expected_arguments) {
    sg_err("Wrong number of arguments, %d, expected count: %d \n",
           count_parsed_arguments,
           count_expected_arguments);
    return 1;
  }

  double time_before = util::get_time_usec();
  file_edge_t* new_edges = createVertexMapping(options);
  sg_log("Read input in %f seconds.\n", (util::get_time_usec() - time_before) / 1000000.0);

  time_before = util::get_time_usec();
  writeNewEdges(options, new_edges);
  sg_log("Written output in %f seconds.\n", (util::get_time_usec() - time_before) / 1000000.0);

  return 0;
}

