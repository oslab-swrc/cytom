// SPDX-License-Identifier: MIT

#ifndef EVOLVING_GRAPHS_EXECUTE_H
#define EVOLVING_GRAPHS_EXECUTE_H

#include <cmath>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <time.h>
#include <sys/mman.h>
#include <pthread.h>
// #include <getopt.h>
#include <errno.h>
#include <sys/stat.h>
#include <memory>
#include <thread>
#include <mutex>
#include <functional>

#include <flatbuffers/flatbuffers.h>
#include <boost/program_options.hpp>

#include "util/datatypes.h"
#include "util/util.h"
#include "util/config.h"
#include "core/meta-tile-store.h"
#include "core/vertex-store.h"
#include "core/continuous-algorithm-executor.h"
#include "core/tile-store.h"
#include "core/jobs.h"
#include "edge-inserter/edge-inserter.h"
#include "edge-inserter/rmat-edge-provider.h"
#include "edge-inserter/binary-edge-provider.h"
#include "traversal/traversal.h"
#include "traversal/hilbert.h"
#include "traversal/row_first.h"
#include "traversal/column_first.h"
#include "perf-event/perf-event-manager.h"
#include "fcntl.h"
#include "schema_generated.h"

namespace evolving_graphs {
static int parseOption(char* argv[], const std::shared_ptr<Config> & config) {
  namespace po = boost::program_options;

  po::options_description description("Usage:");

  description.add_options()
  ("max-vertices", po::value<uint64_t>()->required(), "Max Vertices")
  ("vertices-per-tile", po::value<uint64_t>()->required(), "Vertices Per Tile")
  ("batch-insertion-count", po::value<uint64_t>()->required(), "Batch Insertion Count")
  ("meta-tile-manager-count", po::value<uint64_t>()->required(), "Meta-tile-manager Count")
  ("count-edges", po::value<uint64_t>()->required(), "Count Edges")
  ("edge-inserters-count", po::value<uint64_t>()->required(), "Edge-inserters Count")
  ("use-dynamic-compaction", po::value<int>()->required(), "Use Dynamic Compaction")
  ("count-parallel-compaction", po::value<uint64_t>()->required(), "Count Parallel Compaction")
  ("enable-edge-apis", po::value<int>()->required(), "Enable Edge Apis")
  ("count-algorithm-executors", po::value<uint64_t>()->required(), "Count Algorithm Executors")
  ("algorithm-tile-batching", po::value<uint64_t>()->required(), "Algorithm Tile Batching")
  ("algorithm-iterations", po::value<uint64_t>()->required(), "Algorithm Iterations")
  ("path-perf-events", po::value<std::string>()->required(), "Path of Perf Events")
  ("enable-perf-event-collection", po::value<int>()->required(), "Enable Perf Event Collection")
  ("enable-static-algorithm-before-compaction", po::value<int>()->required(), "Enable Static Algorithm before Compaction")
  ("enable-static-algorithm-after-compaction", po::value<int>()->required(), "Enable Static Algorithm after Compaction")
  ("enable-rewrite-ids", po::value<int>()->required(), "Enable Rewrite-ids")
  ("traversal", po::value<std::string>()->required(), "Traversal")
  ("generator", po::value<std::string>()->required(), "Generator")
  ("binary-input-file", po::value<std::string>()->required(), "Binary Input File")
  ("algorithm", po::value<std::string>()->required(), "Algorithm")  // ignore
  ("deletion-percentage", po::value<double>()->required(), "Deletion Percentage")
  ("enable-interactive-algorithm", po::value<int>()->required(), "Enable Interactive Algorithm")
  ("enable-lazy-algorithm", po::value<int>()->required(), "Enable Lazy Algorithm")
  ("count-algorithm-appliers", po::value<uint64_t>()->required(), "Count Algorithm Appliers")
  ("enable-adaptive-scheduling", po::value<int>()->required(), "Enable Adaptive Scheduling")
  ("tile-distribution-strategy", po::value<std::string>()->required(), "Tile Distribution Strategy")
  ("path-to-edges-output", po::value<std::string>()->required(), "Path To Edges Output")
  ("enable-writing-edges", po::value<int>()->required(), "Enable Writing Edges")
  ("enable-in-memory-ingestion", po::value<int>()->required(), "Enable In-memory Ingestion")
  ("td-count-meta-tiles-per-dimension", po::value<uint64_t>()->required(), "TD Count Meta Tiles Per Dimension")
  ("algo-start-id", po::value<uint64_t>()->required(), "Algorithm Start (Vertex) ID")
  ("enable-reading-edges", po::value<int>()->required(), "Enable Reading Edges")
  ("path-to-edges-input", po::value<std::string>()->required(), "Path to Edges Input")
  ("enable-algorithm-reexecution", po::value<int>()->required(), "Enable Algorithm Reexecution")
  ("path-to-results", po::value<std::string>()->required(), "Path to Results")
  ("enable-write-results", po::value<int>()->required(), "Enable Write Results")
  ("delta", po::value<double>()->required(), "Delta")
  ("start-algorithm-count", po::value<uint64_t>()->required(), "Start Algorithm Count")
  ("output-detailed-tile-stats", po::value<int>()->required(), "Output Detailed Tile Stats")
  ;

  po::variables_map vm;
  po::store(po::command_line_parser(Jobs::NUM_OF_ARGS + 1, argv).options(description).run(), vm);
  po::notify(vm);

  int arg_cnt = 0;

  if (vm.count("max-vertices")){
    config->context_.max_count_vertices = vm["max-vertices"].as<uint64_t>();
    // sg_log("max-vertices: %lu\n", vm["max-vertices"].as<uint64_t>());
    arg_cnt++;
  } 

  if (vm.count("vertices-per-tile")){
    config->context_.tile_store_config.vertices_per_tile = vm["vertices-per-tile"].as<uint64_t>();
    // sg_log("vertices-per-tile: %lu\n", vm["vertices-per-tile"].as<uint64_t>());
    arg_cnt++;
  } 

  if (vm.count("batch-insertion-count")){
    config->context_.batch_insertion_count = vm["batch-insertion-count"].as<uint64_t>();
    arg_cnt++;
  } 

  if (vm.count("meta-tile-manager-count")){
    config->context_.meta_tile_manager_count = static_cast<uint8_t>(vm["meta-tile-manager-count"].as<uint64_t>());
    // sg_log("meta-tile-manager-count: %lu\n", vm["meta-tile-manager-count"].as<uint64_t>());
    arg_cnt++;
  } 

  if (vm.count("count-edges")){
    config->context_.count_edges = vm["count-edges"].as<uint64_t>();
    arg_cnt++;
  } 

  if (vm.count("edge-inserters-count")){
    config->context_.edge_inserters_count = static_cast<uint8_t>(vm["edge-inserters-count"].as<uint64_t>());
    // sg_log("edge-inserters-count: %lu\n", vm["edge-inserters-count"].as<uint64_t>());
    arg_cnt++;
  } 

  if (vm.count("use-dynamic-compaction")){
    config->context_.use_dynamic_compaction = (1 == vm["use-dynamic-compaction"].as<int>());
    // sg_log("use-dynamic-compaction: %d\n", vm["use-dynamic-compaction"].as<int>());
    arg_cnt++;
  } 

  if (vm.count("count-parallel-compaction")){
    config->context_.count_parallel_compaction = vm["count-parallel-compaction"].as<uint64_t>();
    arg_cnt++;
  } 

  if (vm.count("enable-edge-apis")){
    config->context_.enable_edge_apis = (1 == vm["enable-edge-apis"].as<int>());
    arg_cnt++;
  } 

  if (vm.count("count-algorithm-executors")){
    config->context_.algorithm_executor_count = static_cast<uint8_t>(vm["count-algorithm-executors"].as<uint64_t>());
    arg_cnt++;
  } 

  if (vm.count("algorithm-tile-batching")){
    config->context_.algorithm_tile_batching = vm["algorithm-tile-batching"].as<uint64_t>();
    arg_cnt++;
  } 

  if (vm.count("algorithm-iterations")){
    config->context_.algorithm_iterations = vm["algorithm-iterations"].as<uint64_t>();
    // sg_log("algorithm-iterations: %lu", vm["algorithm-iterations"].as<uint64_t>());
    arg_cnt++;
  } 

  if (vm.count("path-perf-events")){
    // std::string path_to_perf_events = vm["path-perf-events"].as<std::string>(); // boost cannot cast c_str to string??
    config->context_.path_to_perf_events = util::prepareDirPath(std::string(argv[26]));
    arg_cnt++;
  } 

  if (vm.count("enable-perf-event-collection")){
    config->context_.enable_perf_event_collection = (1 == vm["enable-perf-event-collection"].as<int>());
    arg_cnt++;
  } 

  if (vm.count("enable-static-algorithm-before-compaction")){
    config->context_.enable_static_algorithm_before_compaction = (1 == vm["enable-static-algorithm-before-compaction"].as<int>());
    arg_cnt++;
  } 

  if (vm.count("enable-static-algorithm-after-compaction")){
    config->context_.enable_static_algorithm_after_compaction = (1 == vm["enable-static-algorithm-after-compaction"].as<int>());
    arg_cnt++;
  } 

  if (vm.count("enable-rewrite-ids")){
    config->context_.enable_rewrite_ids = (1 == vm["enable-rewrite-ids"].as<int>());
    arg_cnt++;
  } 

  if (vm.count("traversal")){
    // std::string traversal = vm["traversal"].as<std::string>();
    // sg_log("traversal: %s\n", argv[36]);
    std::string traversal = std::string(argv[36]);
    std::transform(traversal.begin(),
                   traversal.end(),
                   traversal.begin(),
                   ::tolower);
    if (traversal == "hilbert") {
      config->context_.traversal = traversal_t::HILBERT;
    } else if (traversal == "row-first") {
      config->context_.traversal = traversal_t::ROW_FIRST;
    } else if (traversal == "column-first") {
      config->context_.traversal = traversal_t::COLUMN_FIRST;
    } else {
      sg_log("Wrong option given for traversal: %s\n", traversal.c_str());
      return -EINVAL;
    }
    arg_cnt++;
  } 

  if (vm.count("generator")){
    // std::string generator = vm["generator"].as<std::string>();
    std::string generator = std::string(argv[38]);
    // sg_log("generator: %s\n", argv[38]);
    std::transform(generator.begin(),
                   generator.end(),
                   generator.begin(),
                   ::tolower);
    if (generator == "rmat") {
      config->context_.generator = edge_generator_t::RMAT;
    } else if (generator == "binary") {
      config->context_.generator = edge_generator_t::BINARY_EDGES;
    } else {
      sg_log("Wrong option given for generator: %s\n", generator.c_str());
      return -EINVAL;
    }
    arg_cnt++;
  } 

  if (vm.count("binary-input-file")){
    // config->context_.binary_input_file = vm["binary-input-file"].as<std::string>();
    config->context_.binary_input_file = std::string(argv[40]);
    // sg_log("binary-input-file: %s\n", argv[40]);
    arg_cnt++;
  } 

  if (vm.count("algorithm")){
    // do nothing
    arg_cnt++;
  } 

  if (vm.count("deletion-percentage")){
    config->context_.deletion_percentage = vm["deletion-percentage"].as<double>();
    arg_cnt++;
  } 

  if (vm.count("enable-interactive-algorithm")){
    config->context_.enable_dynamic_algorithm = (1 == vm["enable-interactive-algorithm"].as<int>());
    arg_cnt++;
  } 

  if (vm.count("enable-lazy-algorithm")){
    config->context_.enable_lazy_algorithm = (1 == vm["enable-lazy-algorithm"].as<int>());
    arg_cnt++;
  } 

  if (vm.count("count-algorithm-appliers")){
    config->context_.algorithm_appliers_count = static_cast<uint8_t>(vm["count-algorithm-appliers"].as<uint64_t>());
    arg_cnt++;
  } 

  if (vm.count("enable-adaptive-scheduling")){
    config->context_.enable_adaptive_scheduling = (1 == vm["enable-adaptive-scheduling"].as<int>());
    arg_cnt++;
  } 

  if (vm.count("tile-distribution-strategy")){
    // std::string tile_distribution = vm["tile-distribution-strategy"].as<std::string>();
    std::string tile_distribution = std::string(argv[54]);
    // sg_log("tile_distribution: %s\n", argv[54]);
    std::transform(tile_distribution.begin(),
                   tile_distribution.end(),
                   tile_distribution.begin(),
                   ::tolower);
    if (tile_distribution == "tile-distributor") {
      config->context_.tile_distribution = tile_distribution_t::TILE_DISTRIBUTOR;
    } else if (tile_distribution == "atomics") {
      config->context_.tile_distribution = tile_distribution_t::ATOMICS;
    } else if (tile_distribution == "static") {
      config->context_.tile_distribution = tile_distribution_t::STATIC;
    } else {
      sg_log("Wrong option given for tile_distribution: %s\n", tile_distribution.c_str());
      return -EINVAL;
    }
    arg_cnt++;
  } 

  if (vm.count("path-to-edges-output")){
    // config->context_.persistence_config.path_to_edges_output = util::prepareDirPath(vm["path-to-edges-output"].as<std::string>());
    config->context_.persistence_config.path_to_edges_output = util::prepareDirPath(argv[56]);
    // sg_log("path-to-edges-output: %s\n", argv[56]);
    arg_cnt++;
  } 

  if (vm.count("enable-writing-edges")){
    config->context_.persistence_config.enable_writing_edges = (1 == vm["enable-writing-edges"].as<int>());
    arg_cnt++;
  } 

  if (vm.count("enable-in-memory-ingestion")){
    config->context_.enable_in_memory_ingestion = (1 == vm["enable-in-memory-ingestion"].as<int>());
    arg_cnt++;
  } 

  if (vm.count("td-count-meta-tiles-per-dimension")){
    config->context_.tile_store_config.td_count_meta_tiles_per_dimension = vm["td-count-meta-tiles-per-dimension"].as<uint64_t>();
    arg_cnt++;
  } 

  if (vm.count("algo-start-id")){
    config->context_.algorithm_config.algo_start_id = vm["algo-start-id"].as<uint64_t>();
    arg_cnt++;
  } 

  if (vm.count("enable-reading-edges")){
    config->context_.persistence_config.enable_reading_edges = (1 == vm["enable-reading-edges"].as<int>());
    arg_cnt++;
  } 

  if (vm.count("path-to-edges-input")){
    // config->context_.persistence_config.path_to_edges_input = util::prepareDirPath(vm["path-to-edges-input"].as<std::string>());
    config->context_.persistence_config.path_to_edges_input = util::prepareDirPath(argv[68]);
    // sg_log("path-to-edges-input: %s\n", argv[68]);
    arg_cnt++;
  } 

  if (vm.count("enable-algorithm-reexecution")){
    config->context_.enable_algorithm_reexecution = (1 == vm["enable-algorithm-reexecution"].as<int>());
    arg_cnt++;
  } 

  if (vm.count("path-to-results")){
    // config->context_.path_to_results = util::prepareDirPath(vm["path-to-results"].as<std::string>());
    config->context_.path_to_results = util::prepareDirPath(argv[72]);
    // sg_log("path-to-results: %s\n", argv[72]);
    arg_cnt++;
  } 

  if (vm.count("enable-write-results")){
    config->context_.enable_write_results = (1 == vm["enable-write-results"].as<int>());
    arg_cnt++;
  } 

  if (vm.count("delta")){
    config->context_.algorithm_config.delta = vm["delta"].as<double>();
    // sg_log("delta: %f\n", vm["delta"].as<double>());
    arg_cnt++;
  } 

  if (vm.count("start-algorithm-count")){
    config->context_.min_count_edges_algorithm = vm["start-algorithm-count"].as<uint64_t>();
    arg_cnt++;
  } 

  if (vm.count("output-detailed-tile-stats")){
    config->context_.detailed_tile_stats = (1 == vm["output-detailed-tile-stats"].as<int>());
    arg_cnt++;
  } 

  return arg_cnt;
}


template<typename VertexType, class Algorithm, bool is_weighted>
void outputTileStats(const std::shared_ptr<Config> & config, const std::shared_ptr<TileStore<VertexType, Algorithm, is_weighted> > & tile_store) {
  std::vector<size_t> tile_counts;

  for (uint32_t i = 0; i < config->context_.tile_store_config.count_tiles; ++i) {
    TileManager<VertexType, Algorithm, is_weighted>
        * tm = tile_store->getTileManager(i);
    if (tm->countEdges() > 0) {
      tile_counts.push_back(tm->countEdges());
    }
  }

  std::sort(tile_counts.rbegin(), tile_counts.rend());

  for (size_t i = 0; i < tile_counts.size(); ++i) {
    printf("%lu %lu\n", i, tile_counts[i]);
  }
  fflush(stdout);
}

template<typename VertexType, class Algorithm, bool is_weighted>
void outputTileDistStats(const std::shared_ptr<Config> & config, const std::shared_ptr<TileStore<VertexType, Algorithm, is_weighted> > & tile_store) {
  std::vector<size_t> tile_counts;

  for (uint32_t i = 0; i < config->context_.tile_store_config.count_tiles; ++i) {
    TileManager<VertexType, Algorithm, is_weighted>
        * tm = tile_store->getTileManager(i);
    if (tm->countEdges() > 0) {
      tile_counts.push_back(tm->countEdges());
    }
  }

  double sum = std::accumulate(tile_counts.begin(), tile_counts.end(), 0.0);
  double mean = sum / tile_counts.size();

  std::vector<double> diff(tile_counts.size());
  std::transform(tile_counts.begin(),
                 tile_counts.end(),
                 diff.begin(),
                 std::bind2nd(std::minus<double>(), mean));
  double sq_sum = std::inner_product(diff.begin(), diff.end(), diff.begin(), 0.0);
  double stddev = std::sqrt(sq_sum / tile_counts.size());

  // sg_log("Tile Stats: Count: %lu, Mean size: %f (%f)\n", tile_counts.size(), mean, stddev);
}

template<typename VertexType, class Algorithm, bool is_weighted>
static void outputStatistics(const std::string& prefix, const std::shared_ptr<TileStore<VertexType, Algorithm, is_weighted> > & tile_store) {
  size_t count_edges = tile_store->countEdges();
  size_t count_bytes = tile_store->countBytes();
  size_t count_compacted_vertex_storage = tile_store->countCompactVertexStorage();
  size_t count_compactions = tile_store->countCompactions();

  // sg_log("%s: Total number of edges: %lu\n", prefix.c_str(), count_edges);
  // sg_log("%s: Total MB: %f\n", prefix.c_str(), (double) count_bytes / (double) MB);
  // sg_log("%s: Total count compactions: %lu\n", prefix.c_str(), count_compactions);
  // sg_log("%s: Total count of compacted vertex storage: %lu\n",
  //       prefix.c_str(), count_compacted_vertex_storage);
}

template<typename VertexType, class Algorithm, bool is_weighted>
static void executeAlgorithm(const uint64_t count_edges, const std::string& prefix, 
                             const std::shared_ptr<ContinuousAlgorithmExecutor<VertexType, Algorithm, is_weighted> > & continuous_algorithm_executor) {
  double time_before_us = util::get_time_usec();
  bool converged;
  uint64_t total_count_edges;
  continuous_algorithm_executor->executeAlgorithm(
      count_edges,
      &converged,
      &total_count_edges);
  double time_taken = (util::get_time_usec() - time_before_us) / 1000000.;
  // sg_log(
  //    "%s: Time taken for algorithm (s): %f edges (M)/second %f count edges %lu\n",
  //    prefix.c_str(),
  //    time_taken,
  //    total_count_edges / time_taken / (1000. * 1000.),
  //    total_count_edges);
}

template<typename VertexType, class Algorithm, bool is_weighted>
// std::vector<util::Runnable*> initEdgeGenerators(pthread_barrier_t* in_memory_barrier,
void initEdgeGenerators(pthread_barrier_t* in_memory_barrier,
                                                const Ringbuffer& edge_inserter_rb, 
                                                Jobs * jobs,
                                                uint8_t job_idx,
                                                const std::shared_ptr<Config> & config, 
                                                const std::shared_ptr<MetaTileStore<VertexType, Algorithm, is_weighted> > & meta_tile_store,
                                                const std::shared_ptr<VertexStore<VertexType> > & vertex_store) {
  std::vector<util::Runnable*> threads;

  uint64_t count_edges;

  // file_edge_t* file_edges = nullptr;

  switch (config->context_.generator) {
    case edge_generator_t::RMAT:
      count_edges = config->context_.count_edges;
      break;
    case edge_generator_t::BINARY_EDGES:
      // Open file in case of reading binary input.
      // int fd = open(config->context_.binary_input_file.c_str(), O_RDONLY);
      // struct stat file_stats{};
      // fstat(fd, &file_stats);
      // auto file_size = static_cast<size_t>(file_stats.st_size);

      // file_edges = (file_edge_t*) mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
      // if (file_edges == MAP_FAILED) {
      //   sg_err("Could not open file %s: %d\n", config->context_.binary_input_file.c_str(), errno);
      //   util::die(1);
      // }
      count_edges = std::min(jobs->count_edges, config->context_.count_edges);
      break;
  }

  // Offset from where to start reading.
  size_t offset = 0;
  if (config->context_.persistence_config.enable_reading_edges) {
    // Deduct count of edges to be read from persistence from count of edges to be read from file.
    size_t count_edges_to_read =
        meta_tile_store->getCountEdgesToRead();

    assert(count_edges >= count_edges_to_read);
    count_edges = count_edges - count_edges_to_read;
    offset = count_edges_to_read;
  }

  uint64_t edges_per_thread = int_ceil(count_edges / config->context_.edge_inserters_count, 1);

  for (uint8_t i = 0; i < config->context_.edge_inserters_count; ++i) {
    uint64_t local_start = i * edges_per_thread;
    uint64_t start = offset + local_start;
    uint64_t end = offset + std::min(local_start + edges_per_thread, count_edges);
    // On the last thread, max with the edges count.
    if (i == (config->context_.edge_inserters_count - 1)) {
      end = std::max(end, count_edges);
    }

    switch (config->context_.generator) {
      case edge_generator_t::RMAT:
        // threads.push_back(new RmatEdgeProvider<VertexType, Algorithm, is_weighted>(edge_inserter_rb,
        //                                                                            in_memory_barrier,
        //                                                                            i,
        //                                                                            start,
        //                                                                            end,
        //                                                                            config,
        //                                                                            vertex_store,
								// 		   meta_tile_store)); // TODO: Mingyu
        break;
      case edge_generator_t::BINARY_EDGES:
        // threads.push_back(new BinaryEdgeProvider<VertexType, Algorithm, is_weighted>(
        //     edge_inserter_rb,
        //     in_memory_barrier,
        //     i,
        //     config,
        //     vertex_store,
	       //    meta_tile_store));
        jobs->edge_providers[i][job_idx] = new BinaryEdgeProvider<VertexType, Algorithm, is_weighted>(
            edge_inserter_rb,
            in_memory_barrier,
            i,
            config,
            vertex_store,
            meta_tile_store);
        break;
    }
  }
  // return threads;
}

template<typename VertexType, class Algorithm, bool is_weighted>
void execute(char* argv[], Jobs * jobs, uint8_t job_idx) {
  char next[] = "Next";
  // sg_log("-----Execute %s Algorithm-----\n", next);

  // create config for this job
  std::shared_ptr<Config> config = std::make_shared<Config>();

  // Parse command line options.
  int count_expected_arguments = 40;
  int count_parsed_arguments = parseOption(argv, config);
  if (count_parsed_arguments != count_expected_arguments) {
    sg_err("Wrong number of arguments, %d, expected count: %d \n",
           count_parsed_arguments,
           count_expected_arguments);
    return;
  }

  // Clean und normalize config.
  config->normalizeConfig();
  
  // Launch PerfEventManager.
  auto perf_event_manager = std::make_shared<perf_event::PerfEventManager >();

  if (config->context_.enable_perf_event_collection) {
    perf_event_manager->start(config);
    sg_log("Using perf event %d", 1);
  }

  Traversal* traversal;
  switch (config->context_.traversal) {
    case traversal_t::HILBERT:
      traversal = new Hilbert;
      break;
    case traversal_t::ROW_FIRST:
      traversal = new RowFirst;
      break;
    case traversal_t::COLUMN_FIRST:
      traversal = new ColumnFirst;
      break;
  }

  Ringbuffer edge_inserter_control_rb;
  Ringbuffer edge_inserter_sync_rb;
  Ringbuffer continuous_algo_rb;
  Ringbuffer meta_tile_manager_sync_rb;
  edge_inserter_control_rb.create(16 * MB, L1D_CACHELINE_SIZE, true, nullptr, nullptr);
  edge_inserter_sync_rb.create(16 * MB, L1D_CACHELINE_SIZE, true, nullptr, nullptr);
  continuous_algo_rb.create(16 * MB, L1D_CACHELINE_SIZE, true, nullptr, nullptr);
  meta_tile_manager_sync_rb.create(16 * MB, L1D_CACHELINE_SIZE, true, nullptr, nullptr);

  pthread_barrier_t read_edges_barrier;
  pthread_barrier_init(&read_edges_barrier, nullptr, 1 + config->context_.meta_tile_manager_count);

  auto vertex_store = std::make_shared<VertexStore<VertexType> >();
  vertex_store->init(config->context_.max_count_vertices);

  auto tile_store = std::make_shared<TileStore<VertexType, Algorithm, is_weighted> >(config);
  tile_store->init(traversal, vertex_store);

  // Create algorithm instance.
  auto algorithm = std::make_shared<Algorithm>(config, vertex_store);

  auto continuous_algorithm_executor = std::make_shared<ContinuousAlgorithmExecutor<VertexType, Algorithm, is_weighted> >(continuous_algo_rb,
                                                                                                                          traversal,
                                                                                                                          "ContinuousExecutor",
                                                                                                                          config,
                                                                                                                          perf_event_manager,
                                                                                                                          algorithm,
															  vertex_store);
  continuous_algorithm_executor->initAlgorithm();

  auto meta_tile_store = std::make_shared<MetaTileStore<VertexType, Algorithm, is_weighted> >(config, tile_store);
  meta_tile_store->init(edge_inserter_sync_rb, meta_tile_manager_sync_rb, &read_edges_barrier, algorithm, continuous_algorithm_executor, vertex_store);

  // If enabled, start continuous algorithm executor.
  if (config->context_.enable_dynamic_algorithm ||
      config->context_.enable_static_algorithm_before_compaction ||
      config->context_.enable_static_algorithm_after_compaction) {
    continuous_algorithm_executor->init(vertex_store, tile_store, meta_tile_store, perf_event_manager);

    // Only run the thread in the lazy mode, otherwise it's not needed and the algorithm is executed
    // synchronously.
    if (config->context_.enable_lazy_algorithm) {
      continuous_algorithm_executor->start();
      continuous_algorithm_executor->setName("ContExecutor");
    }
  }

  uint64_t time_before_us = util::get_time_usec();

  meta_tile_store->start();

  // If we read edges in, wait for them.
  size_t count_edges_already_inserted = 0;
  if (config->context_.persistence_config.enable_reading_edges) {
    sg_log2("Reading back edges from persistent backup.\n");
    pthread_barrier_wait(&read_edges_barrier);
    double time_taken = double(util::get_time_usec() - time_before_us) / 1000000.;
    sg_log("Reading edges from persistent backup completed in %f s.\n", time_taken);
    count_edges_already_inserted =
        meta_tile_store->getCountEdgesToRead();

    time_before_us = util::get_time_usec();
  }

  EdgeInserter<VertexType, Algorithm, is_weighted> inserter
      (traversal,
       edge_inserter_control_rb,
       edge_inserter_sync_rb,
       meta_tile_manager_sync_rb,
       count_edges_already_inserted,
       config,
       meta_tile_store,
       vertex_store,
       perf_event_manager,
       continuous_algorithm_executor,
       algorithm);
  inserter.start();
  inserter.setName("EdgeInserter_1");

  pthread_barrier_t in_memory_barrier;
  pthread_barrier_init(&in_memory_barrier, nullptr, config->context_.edge_inserters_count + 1);

  // std::vector<util::Runnable*> threads =
      initEdgeGenerators<VertexType, Algorithm, is_weighted>(&in_memory_barrier,
                                                             edge_inserter_control_rb,
                                                             jobs,
                                                             job_idx,
                                                             config, 
                                                             meta_tile_store,
                                                             vertex_store);

  // uint8_t i = 1;
  // for (auto& thread : threads) {
  //   thread->start();
    // thread->setName("EdgeProvider_" + std::to_string(i++));
  // }

  // In the in-memory ingestion mode, wait for all edges to be read into memory first and take time
  // for that.
  if (config->context_.enable_in_memory_ingestion) {
    pthread_barrier_wait(&in_memory_barrier);

    double time_taken = double(util::get_time_usec() - time_before_us) / 1000000.;
    sg_log("Time taken for reading edges (s): %f\n", time_taken);

    time_before_us = util::get_time_usec();
  }

  // for (auto& thread : threads) {
  //   thread->join();
  //   delete thread;
  // }
  
  inserter.join();
  
  meta_tile_store->join();
  double time_taken = double(util::get_time_usec() - time_before_us) / 1000000.;
  
  size_t count_edges =
      tile_store->countEdges() - count_edges_already_inserted;

  // sg_log("Time taken for insertion (s): %f\n", time_taken);
  // sg_log("Insertion throughput (edges/s): %f\n", count_edges / time_taken);

  outputStatistics<VertexType, Algorithm, is_weighted>("Before compaction", tile_store);

  // Run one round of static algorithm, if requested.
  if (config->context_.enable_static_algorithm_before_compaction) {
    executeAlgorithm<VertexType, Algorithm, is_weighted>(count_edges, "Before compaction", continuous_algorithm_executor);
  }

  // Run one round of static algorithm, if requested, globally compact the graph first.
  if (config->context_.enable_static_algorithm_after_compaction) {
    size_t
        count_bytes_before_compaction = tile_store->countBytes();
    // Check cost for compaction.
    uint64_t time_before_compaction_us = util::get_time_usec();

    if (config->context_.count_parallel_compaction > 1) {
      tile_store->doGlobalCompaction(config->context_.count_parallel_compaction);
    } else {
      tile_store->doGlobalCompaction();
    }

    time_taken = double(util::get_time_usec() - time_before_compaction_us) / 1000000.;

    size_t
        count_bytes_after_compaction = tile_store->countBytes();

    // sg_log("Time taken for compaction (s): %f\n", time_taken);
    // sg_log("MB before/after compaction and ratio: %f, %f, %f\n",
    //       (double) count_bytes_before_compaction / (double) MB,
    //       (double) count_bytes_after_compaction / (double) MB,
    //       (double) count_bytes_after_compaction / count_bytes_before_compaction);

    outputStatistics<VertexType, Algorithm, is_weighted>("After compaction", tile_store);

    executeAlgorithm<VertexType, Algorithm, is_weighted>(count_edges, "After compaction", continuous_algorithm_executor);
  }

  if (config->context_.enable_dynamic_algorithm) {
    // Finish algorithm executor, if enabled.
    if (config->context_.enable_lazy_algorithm) {
      flatbuffers::FlatBufferBuilder builder;
      auto message = CreateAlgorithmMessage(builder, true);
      builder.Finish(message);
      util::sendMessage(continuous_algo_rb, builder);
    }

    continuous_algorithm_executor->destroy();

    if (config->context_.enable_lazy_algorithm) {
      continuous_algorithm_executor->join();
    }
  }

  time_taken = double(util::get_time_usec() - time_before_us) / 1000000.;
  // sg_log("Time taken until algorithm complete (s): %f\n", time_taken);
  // sg_log("Overall throughput (edges/s): %f\n", count_edges / time_taken);

  outputStatistics<VertexType, Algorithm, is_weighted>("Before compaction", tile_store);

  if (config->context_.detailed_tile_stats) {
    outputTileStats<VertexType, Algorithm, is_weighted>(config, tile_store);
  }
  outputTileDistStats<VertexType, Algorithm, is_weighted>(config, tile_store);

  vertex_store->destroy();
  meta_tile_store->destroy();
  tile_store->destroy();

  // Stop PerfEventManager.
  if (config->context_.enable_perf_event_collection) {
    perf_event_manager->stop(); // not in use?
  }
}

} 

#endif //EVOLVING_GRAPHS_EXECUTE_H
