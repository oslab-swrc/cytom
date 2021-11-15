// SPDX-License-Identifier: MIT

#ifndef EVOLVING_GRAPHS_CONFIG_H
#define EVOLVING_GRAPHS_CONFIG_H

#include "util/datatypes.h"

namespace evolving_graphs {

enum class traversal_t {
  HILBERT, ROW_FIRST, COLUMN_FIRST
};

enum class edge_generator_t {
  RMAT, BINARY_EDGES
};

//enum class algorithm_t {
//  PAGERANK,
//  PAGERANK_DELTA,
//  CONNECTED_COMPONENTS,
//  CONNECTED_COMPONENTS_ASYNC,
//  BFS,
//  BFS_ASYNC,
//  SSSP,
//  SSSP_ASYNC,
//  SSWP
//};

enum class tile_distribution_t {
  TILE_DISTRIBUTOR, ATOMICS, STATIC
};

struct test_template_arguments_t {
  uint32_t td_count_meta_tiles_per_dimension;
  tile_distribution_t tile_distribution;
  bool enable_adaptive_scheduling;

  friend std::ostream& operator<<(std::ostream& os, const test_template_arguments_t& args) {
    return os << args.td_count_meta_tiles_per_dimension << " " << static_cast<std::underlying_type<
        tile_distribution_t>::type>(args.tile_distribution) << " "
              << args.enable_adaptive_scheduling;
  }
};


struct tile_store_config_t {
  uint64_t vertices_per_tile;
  uint64_t count_tiles_per_dimension;
  uint64_t count_tiles;
  uint64_t td_count_meta_tiles_per_dimension;
  uint64_t td_count_meta_tiles;
  uint64_t td_count_tiles_per_meta_tiles_per_dimension;
  uint64_t td_count_tiles_per_meta_tiles;
};

struct algorithm_config_t {
  uint64_t algo_start_id;
  double delta;
};

struct edge_persistence_config_t {
  bool enable_writing_edges;
  bool enable_reading_edges;
  std::string path_to_edges_input;
  std::string path_to_edges_output;
};

struct ConfigContext {
  tile_store_config_t tile_store_config;
  algorithm_config_t algorithm_config;
  edge_persistence_config_t persistence_config;
  uint64_t count_vertices;
  uint64_t max_count_vertices;
  uint64_t batch_insertion_count;
  uint64_t count_edges;
  uint64_t count_parallel_compaction;
  uint64_t algorithm_tile_batching;
  uint64_t algorithm_iterations;
  uint8_t meta_tile_manager_count;
  uint8_t edge_inserters_count;
  uint8_t algorithm_executor_count;
  uint8_t algorithm_appliers_count;
  bool use_dynamic_compaction;
  bool enable_static_algorithm_before_compaction;
  bool enable_static_algorithm_after_compaction;
  bool enable_dynamic_algorithm;
  bool enable_perf_event_collection;
  bool enable_edge_apis;
  bool enable_rewrite_ids;
  bool enable_lazy_algorithm;
  bool enable_adaptive_scheduling;
  bool enable_in_memory_ingestion;
  bool enable_two_level_tile_distributor;
  bool enable_algorithm_reexecution;
  bool enable_write_results;
  bool detailed_tile_stats;
  double deletion_percentage;
  std::string path_to_perf_events;
  std::string binary_input_file;
  std::string path_to_results;
  traversal_t traversal;
  edge_generator_t generator;
//  algorithm_t algorithm;
  tile_distribution_t tile_distribution;
  uint64_t min_count_edges_algorithm;
};

class Config {
public:
  ConfigContext context_;
  void normalizeConfig();
};

}

#endif //EVOLVING_GRAPHS_CONFIG_H
