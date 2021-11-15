// SPDX-License-Identifier: MIT

#ifndef EVOLVING_GRAPHS_JOBS_H
#define EVOLVING_GRAPHS_JOBS_H

#include <cmath>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <unistd.h>
#include <sched.h>

#include <boost/program_options.hpp>

#include "util/util.h"
#include "util/datatypes.h"
#include "edge-inserter/binary-edge-provider-interface.h"

namespace evolving_graphs {

class Jobs
{
public:
  Jobs(int argc, char * argv[]) {
    if((argc - 1) % NUM_OF_ARGS != 0) {
      sg_err("Wrong number of arguments, %d, expected multiple of : %d \n",
             argc,
             NUM_OF_ARGS);
    }
    num_of_jobs_ = (argc - 1) / NUM_OF_ARGS;
    // sg_log("Number of jobs: %d\n", num_of_jobs_);

    argv_ = argv;
    count_edges = 0;
    cur_job_ = 0;
    // file_edges = nullptr;

    parse_options();

    if (generator_ == "rmat") {
      // do nothing
    } else if (generator_ == "binary") {
        // Open file in case of reading binary input.
        int fd = open(binary_input_file_.c_str(), O_RDONLY);
        struct stat file_stats{};
        fstat(fd, &file_stats);
        auto file_size = static_cast<size_t>(file_stats.st_size);

        file_edges_map = (file_edge_t*) mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (file_edges_map == MAP_FAILED) {
          sg_err("Could not open file %s: %d\n", argv[40], errno);
          util::die(1);
        }

        count_edges = file_size / sizeof(file_edge_t);
        // file_edges.resize(count_edges, nullptr);
        edge_providers.resize(count_edge_inserters_, std::vector<BinaryEdgeProviderInterface*>(num_of_jobs_));

        // trick: resize mutexes (std::mutex is not movable so cannot be resized!)
        std::vector<std::mutex> temp_mutexes(count_edge_inserters_);
        mutexes.swap(temp_mutexes);
        
        // trick: resize cvs
        std::vector<std::condition_variable> temp_cvs(count_edge_inserters_);
        cvs.swap(temp_cvs);
    }
  }

  template <class Fn>
  void add_job(Fn && fn) {
    char ** args = gen_args(cur_job_);
    threads.emplace_back(fn, args, this, cur_job_);
    cur_job_++;
  }

  void edge_reader_thread(Jobs * jobs, int reader_idx, uint64_t start_id, uint64_t end_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    if (CPU_COUNT(&cpuset)) {
      int rc = sched_setaffinity(0, sizeof(cpuset), &cpuset);
      if (rc) {
        sg_err("Fail to pin a thread: %s\n", strerror(errno));
        util::die(1);
      }
    }

    // check if reader can start: busy wait but should be quick
    bool can_start = false;
    while(!can_start) {
      // sg_log("Edge Reader %d waits starting...", reader_idx);
      can_start = true;
      for(uint8_t job_idx = 0; job_idx < jobs->num_of_jobs_; job_idx++) {
        if(!jobs->edge_providers[reader_idx][job_idx]){
          can_start = false;
          break;
        }
      }

      usleep(10);
    }

    for (uint64_t i = start_id; i < end_id; ++i) {
      file_edge_t next_edge = file_edges_map[i];
      Edge converted_edge(static_cast<vertex_id_t>(next_edge.src),
                        static_cast<vertex_id_t>(next_edge.tgt), 0.0);

      for(uint8_t job_idx = 0; job_idx < jobs->num_of_jobs_; job_idx++) {
        jobs->edge_providers[reader_idx][job_idx]->addEdgeWrapper(converted_edge);
      }
    }

    for(uint8_t job_idx = 0; job_idx < jobs->num_of_jobs_; job_idx++) {
      jobs->edge_providers[reader_idx][job_idx]->insertionFinishedWrapper();
    }
  }

  void launch_edge_readers() {
    uint64_t edges_per_thread = int_ceil(count_edges / count_edge_inserters_, 1);

    for (uint8_t i = 0; i < count_edge_inserters_; ++i) {
      uint64_t start = i * edges_per_thread;
      uint64_t end = std::min(start + edges_per_thread, count_edges);

      // On the last thread, max with the edges count.
      if (i == (count_edge_inserters_ - 1)) {
        end = std::max(end, count_edges);
      }

      edge_readers_.emplace_back(&Jobs::edge_reader_thread, this, this, i, start, end);
    }
  }

  void join_all() {
    // join edge_readers
    for(auto& edge_reader: edge_readers_)
    {
        edge_reader.join();

    }

    // join all executed jobs
    for(auto& job: threads)
    {
        job.join();

    }
  }

  static const int MAX_ARG_LEN = 100; // max length of one arg
  static const int NUM_OF_ARGS = 80; // num of args for each job (including options)
  std::vector<std::thread> threads;
  file_edge_t* file_edges_map;
  std::vector<std::mutex> mutexes;
  std::vector<std::condition_variable> cvs;
  uint64_t count_edges;
  std::vector<std::vector<BinaryEdgeProviderInterface*> > edge_providers;

private:
  void parse_options() {
    int argc = NUM_OF_ARGS + 1;
    char ** argv = this->gen_args(0);

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
    po::store(po::command_line_parser(argc, argv).options(description).run(), vm);
    po::notify(vm);

    if (vm.count("generator")){
      // std::string generator = vm["generator"].as<std::string>();
      generator_ = std::string(argv[38]);
      // sg_log("generator: %s\n", argv[38]);
      std::transform(generator_.begin(),
                     generator_.end(),
                     generator_.begin(),
                     ::tolower);

      // sg_log("Edge-generator: %s", generator_.c_str());
    }

    if (vm.count("binary-input-file")){
      binary_input_file_ = std::string(argv[40]);
      // sg_log("Binary-input-file: %s", binary_input_file_.c_str());
    } 

    if(vm.count("edge-inserters-count")) {
      count_edge_inserters_ = static_cast<uint8_t>(vm["edge-inserters-count"].as<uint64_t>());
      // sg_log("count-edge-inserters: %d", count_edge_inserters_);
    }
  }

  char ** gen_args(int job_idx) {
    // job index starts at 0
    char ** args = new char*[NUM_OF_ARGS + 1]; // include program's name
    args[0] = new char[MAX_ARG_LEN + 1];
    strcpy(args[0], argv_[0]);

    for (int arg_idx = 1; arg_idx <= NUM_OF_ARGS; arg_idx++) {
      args[arg_idx] = new char[MAX_ARG_LEN + 1];
      strcpy(args[arg_idx], argv_[job_idx*NUM_OF_ARGS+arg_idx]);
    }

    return args;
  }

  uint8_t num_of_jobs_;
  char ** argv_;
  uint8_t cur_job_;
  std::vector<std::thread> edge_readers_;
  /* global arg options */
  std::string generator_;
  std::string binary_input_file_;
  uint8_t count_edge_inserters_;
};

}

#endif //EVOLVING_GRAPHS_JOBS_H
