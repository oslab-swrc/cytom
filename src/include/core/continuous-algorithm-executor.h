// SPDX-License-Identifier: MIT

#ifndef EVOLVING_GRAPHS_CONTINOUS_ALGORITHM_EXECUTOR_H
#define EVOLVING_GRAPHS_CONTINOUS_ALGORITHM_EXECUTOR_H

#include <map>
#include <cmath>
#include <memory>

#include <flatbuffers/flatbuffers.h>

#include "core/tile-distributor.h"
#include "core/algorithm-executor.h"
#include "core/apply-executor.h"
#include "core/convergence-checker.h"
#include "core/vertex-store.h"
#include "perf-event/perf-event-scoped.h"
#include "perf-event/perf-event-manager.h"
#include "util/runnable.h"
#include "util/util.h"
#include "util/config.h"
#include "ring-buffer/ring-buffer.h"
#include "traversal/traversal.h"
#include "schema_generated.h"

namespace evolving_graphs {

using namespace perf_event;

template<typename VertexType, class Algorithm, bool is_weighted>
class ContinuousAlgorithmExecutor : public util::Runnable {
public:
  ContinuousAlgorithmExecutor(const Ringbuffer& rb, Traversal* traversal,
                              const std::string& prefix, 
                              std::shared_ptr<Config> config,
                              std::shared_ptr<PerfEventManager> perf_event_manager,
                              std::shared_ptr<Algorithm> algorithm,
			      std::shared_ptr<VertexStore<VertexType> > vertex_store)
  : rb_(rb), traversal_(traversal), shutdown_flag_(false), prefix_(prefix), 
    config_(std::move(config)), 
    perf_event_manager_(std::move(perf_event_manager)),
    algorithm_(std::move(algorithm)),
    vertex_store_(std::move(vertex_store))	{}

  inline void executeAlgorithm(uint64_t total_count_edges, bool* converged, uint64_t* count_edges) {
    executeAlgorithm(total_count_edges, converged, count_edges, prefix_);
  }

  inline void executeAlgorithm(uint64_t total_count_edges, bool* converged, uint64_t* count_edges,
                  const std::string& prefix) {
    *converged = false;

    uint32_t current_iteration = 0;
    uint64_t total_count_processed_edges = 0;
    double total_time_taken_pull_gather = 0.;
    double total_time_taken_apply = 0.;
    uint64_t total_time_before_all_iterations_us = util::get_time_usec();

    uint64_t total_count_retries = 0;
    uint64_t total_count_skipped_tiles = 0;
    uint64_t total_count_tiles = 0;
    uint64_t total_count_skipped_meta_tiles = 0;
    uint64_t total_count_meta_tiles = 0;
    uint64_t total_count_active_tiles = 0;
    uint64_t total_count_inactive_tiles = 0;

    if (config_->context_.tile_distribution == tile_distribution_t::TILE_DISTRIBUTOR) {
      // Notify TileDistributor and let it read a new edges count.
      td_->updateCountEdges(total_count_edges);
    }

    while (current_iteration < config_->context_.algorithm_iterations) {
      // Notify the TileDistributor to start the next iteration.
      pthread_barrier_wait(&td_barrier_);

      PerfEventScoped event(perf_event_manager_->getRingBuffer(),
                            "iteration-" + std::to_string(current_iteration),
                            "Algorithm",
                            0,
                            config_->context_.enable_perf_event_collection);

      uint64_t time_before_us = util::get_time_usec();
      uint64_t total_time_before_us = util::get_time_usec();

      // Pull Phase.
      uint64_t count_processed_edges = 0;
      uint64_t count_retries = 0;
      uint64_t count_skipped_tiles = 0;
      uint64_t count_skipped_meta_tiles = 0;
      uint64_t count_active_tiles = 0;
      uint64_t count_inactive_tiles = 0;
      {
        PerfEventScoped event_pull(perf_event_manager_->getRingBuffer(),
                                   "pull-" + std::to_string(current_iteration),
                                   "MetaTileStore",
                                   0,
                                   config_->context_.enable_perf_event_collection);

        pthread_barrier_wait(&pull_barrier_);

        for (auto& thread : threads_) {
          count_processed_edges +=
              static_cast<AlgorithmExecutor<VertexType, Algorithm,
                  is_weighted>*>(thread)->countEdges();
          count_retries +=
              static_cast<AlgorithmExecutor<VertexType, Algorithm,
                  is_weighted>*>(thread)->countRetries();
          count_inactive_tiles +=
              static_cast<AlgorithmExecutor<VertexType, Algorithm,
                  is_weighted>*>(thread)->countInactiveTiles();
          count_active_tiles +=
              static_cast<AlgorithmExecutor<VertexType, Algorithm,
                  is_weighted>*>(thread)->countActiveTiles();

          if (config_->context_.tile_distribution == tile_distribution_t::ATOMICS ||
              config_->context_.tile_distribution == tile_distribution_t::STATIC) {
            // Get skipped info from AlgorithmExecutor.
            count_skipped_tiles +=
                static_cast<AlgorithmExecutor<VertexType, Algorithm,
                    is_weighted>*>(thread)->countSkippedTiles();
          }

          static_cast<AlgorithmExecutor<VertexType, Algorithm,
              is_weighted>*>(thread)->resetStatistics();
        }
        if (config_->context_.tile_distribution == tile_distribution_t::TILE_DISTRIBUTOR) {
          count_skipped_tiles = td_->getCountSkippedTiles();
          count_skipped_meta_tiles = td_->getCountSkippedMetaTiles();
          td_->resetStatistics();
        }
        double time_taken_pull_gather = double(util::get_time_usec() - time_before_us) / 1000000.;
        total_time_taken_pull_gather += time_taken_pull_gather;
      }

      {
        PerfEventScoped event_pull(perf_event_manager_->getRingBuffer(),
                                   "apply-" + std::to_string(current_iteration),
                                   "MetaTileStore",
                                   0,
                                   config_->context_.enable_perf_event_collection);

        // Apply phase.
        pthread_barrier_wait(&apply_barrier_);

        time_before_us = util::get_time_usec();

        // Wait until all appliers are done.
        int return_code = pthread_barrier_wait(&apply_barrier_);
        if (return_code == PTHREAD_BARRIER_SERIAL_THREAD) {
          if (Algorithm::ASYNC) {
            // ConvergenceChecker<VertexType, Algorithm, is_weighted>::resetCurrentTileID();
            for (auto& thread : convergence_check_threads_) {
              static_cast<ConvergenceChecker<VertexType, Algorithm, is_weighted>*>(thread)->resetCurrentTileID();
            }
          } else {
            // ApplyExecutor<VertexType, Algorithm, is_weighted>::resetCurrentTileID();
            for (auto& thread : apply_threads_) {
              static_cast<ApplyExecutor<VertexType, Algorithm, is_weighted>*>(thread)->resetCurrentTileID();
            }
          }
        }

        // Check for convergence and exit, if needed.
        bool convergence = this->checkConvergence();


        double time_taken_apply = double(util::get_time_usec() - time_before_us) / 1000000.;
        total_time_taken_apply += time_taken_apply;

        total_count_retries += count_retries;
        total_count_skipped_tiles += count_skipped_tiles;
        total_count_tiles += config_->context_.tile_store_config.count_tiles;
        total_count_skipped_meta_tiles += count_skipped_meta_tiles;
        total_count_meta_tiles += config_->context_.tile_store_config.td_count_meta_tiles;
        total_count_inactive_tiles += count_inactive_tiles;
        total_count_active_tiles += count_active_tiles;
        total_count_processed_edges += count_processed_edges;

        if (convergence) {
          *converged = true;
          break;
        }
      }

      // Reset Vertices.
      {
        PerfEventScoped
            event_pull(perf_event_manager_->getRingBuffer(),
                       "reset-" + std::to_string(current_iteration),
                       "MetaTileStore",
                       0,
                       config_->context_.enable_perf_event_collection);
        bool switchCurrentNext = true;
        bool switchCurrentNextActive = true;
        algorithm_->reset_vertices(&switchCurrentNext, &switchCurrentNextActive);
        // Do not switch arrays in the last iteration, output should be in current.
        if (current_iteration != (config_->context_.algorithm_iterations - 1)) {
          if (switchCurrentNext) {
            std::swap(vertex_store_->vertices_.current,
                      vertex_store_->vertices_.next);
          }
          if (switchCurrentNextActive) {
            std::swap(vertex_store_->vertices_.active_current,
                      vertex_store_->vertices_.active_next);
          }
        }

        // Also reset the changed field, if necessary.
        if (Algorithm::USE_CHANGED) {
          auto size_active =
              static_cast<size_t>(size_bool_array(vertex_store_->vertex_count()));
          memset(vertex_store_->vertices_.changed,
                 (unsigned char) 0,
                 static_cast<size_t>(size_active * sizeof(uint8_t)));
        }
      }

      ++current_iteration;
    }
    double total_time_taken =
        double(util::get_time_usec() - total_time_before_all_iterations_us) / 1000000.;
    double pull_gather_percentage = total_time_taken_pull_gather / total_time_taken * 100.;
    double apply_percentage = total_time_taken_apply / total_time_taken * 100.;
     //sg_log(
     //    "%s: Total Time taken for algorithm (s): %f, %u iterations, PullGather: %f (%f%%), "
     //    "Apply: %f (%f%%), edges (M)/second: %f, retries %lu, edges %lu, "
     //    "skipped meta tiles: %lu (%lu, %f%%) "
     //    "skipped tiles: %lu (%lu, %f%%), inactive tiles: %lu, active tiles: %lu\n",
     //    prefix_.c_str(),
     //    total_time_taken,
     //    current_iteration,
     //    total_time_taken_pull_gather,
     //    pull_gather_percentage,
     //    total_time_taken_apply,
     //    apply_percentage,
     //    total_count_processed_edges / total_time_taken / (1000. * 1000.),
     //    total_count_retries,
     //    total_count_processed_edges,
     //    total_count_skipped_meta_tiles,
     //    total_count_meta_tiles,
     //    (double) total_count_skipped_meta_tiles / total_count_meta_tiles * 100.0,
     //    total_count_skipped_tiles,
     //    total_count_tiles,
     //    (double) total_count_skipped_tiles / total_count_tiles * 100.0,
     //    total_count_inactive_tiles,
     //    total_count_active_tiles
     //);
    *count_edges = total_count_processed_edges;
  }

  void init(const std::shared_ptr<VertexStore<VertexType> > & vertex_store, 
            const std::shared_ptr<TileStore<VertexType, Algorithm, is_weighted> > & tile_store,
            const std::shared_ptr<MetaTileStore<VertexType, Algorithm, is_weighted> > & meta_tile_store,
            const std::shared_ptr<PerfEventManager> & perf_event_manager) {
    uint8_t count_executors = config_->context_.algorithm_executor_count;
    uint8_t count_appliers = config_->context_.algorithm_appliers_count;

    // Adjust count for td_barrier according to config.
    uint32_t count_td_barrier = 0;
    if (config_->context_.tile_distribution == tile_distribution_t::TILE_DISTRIBUTOR) {
      count_td_barrier = 2 + count_executors;
    } else if (config_->context_.tile_distribution == tile_distribution_t::ATOMICS ||
               config_->context_.tile_distribution == tile_distribution_t::STATIC) {
      count_td_barrier = 1 + count_executors;
    }

    pthread_barrier_init(&td_barrier_, nullptr, count_td_barrier);
    pthread_barrier_init(&pull_barrier_, nullptr, 1 + count_executors);
    pthread_barrier_init(&apply_barrier_, nullptr, 1 + count_appliers);
    pthread_barrier_init(&convergence_check_barrier_, nullptr, 1 + count_appliers);
    pthread_barrier_init(&shutdown_barrier_, nullptr, 2);
    // Is shared with all threads and is used to signal the end of the current execution.

    // Set up tile-distributor first.
    Ringbuffer rb;
    rb.create(32 * MB, L1D_CACHELINE_SIZE, true, nullptr, nullptr);

    if (config_->context_.tile_distribution == tile_distribution_t::TILE_DISTRIBUTOR) {
      td_ = new TileDistributor<VertexType, Algorithm, is_weighted>(rb,
                                                                    traversal_,
                                                                    &td_barrier_,
                                                                    &shutdown_flag_,
                                                                    config_,
                                                                    tile_store,
                                                                    meta_tile_store,
                                                                    perf_event_manager);
      td_->start();
      td_->setName("TileDistributor");
    }

    auto current_tile = std::make_shared<uint64_t>(0);
    for (uint32_t i = 0; i < count_executors; ++i) {
      auto* executor =
          new AlgorithmExecutor<VertexType, Algorithm, is_weighted>(rb,
                                                                    i,
                                                                    traversal_,
                                                                    &td_barrier_,
                                                                    &pull_barrier_,
                                                                    &shutdown_flag_,
                                                                    config_,
                                                                    vertex_store,
                                                                    tile_store,
                                                                    perf_event_manager,
                                                                    algorithm_,
                                                                    current_tile);
      executor->start();
      executor->setName("Executor_" + std::to_string(i));
      threads_.push_back(executor);
    }

    auto current_tile_id = std::make_shared<uint64_t>(0);
    if (Algorithm::ASYNC) {
      for (uint32_t i = 0; i < count_appliers; ++i) {
        auto* convergence_check =
            new ConvergenceChecker<VertexType, Algorithm, is_weighted>(i,
                                                                       &apply_barrier_,
                                                                       &shutdown_flag_,
                                                                       config_,
                                                                       vertex_store,
                                                                       perf_event_manager,
                                                                       current_tile_id);
        convergence_check->start();
        convergence_check->setName("Conver_" + std::to_string(i));
        convergence_check_threads_.push_back(convergence_check);
      }
    } else {
      for (uint32_t i = 0; i < count_appliers; ++i) {
        auto* apply = new ApplyExecutor<VertexType, Algorithm, is_weighted>(i,
                                                                            &apply_barrier_,
                                                                            &shutdown_flag_,
                                                                            config_,
                                                                            perf_event_manager,
                                                                            algorithm_,
                                                                            current_tile_id);
        apply->start();
        apply->setName("Apply_" + std::to_string(i));
        apply_threads_.push_back(apply);
      }
    }
  }

  inline void initAlgorithm() { algorithm_->init_vertices(nullptr); }

  inline void destroy() {
    if (config_->context_.enable_lazy_algorithm) {
      // Wait for main thread to be shut down if it is running.
      pthread_barrier_wait(&shutdown_barrier_);
    }

    // Last barrier, set shutdown flag and join threads.
    shutdown_flag_ = true;
    pthread_barrier_wait(&td_barrier_);
    pthread_barrier_wait(&apply_barrier_);
    if (config_->context_.tile_distribution == tile_distribution_t::TILE_DISTRIBUTOR) {
      td_->join();
    }

    for (auto& thread : threads_) {
      thread->join();
      delete thread;
    }

    for (auto& thread : apply_threads_) {
      thread->join();
      delete thread;
    }

    for (auto& thread : convergence_check_threads_) {
      thread->join();
      delete thread;
    }
  }

  inline Ringbuffer rb() { return rb_; }

  // inline static ContinuousAlgorithmExecutor<VertexType, Algorithm, is_weighted>* get() { return instance; }

  // inline static void set(ContinuousAlgorithmExecutor<VertexType, Algorithm, is_weighted>* executor) { instance = executor; }

protected:
  void run() override {
    ring_buffer_req_t req{};
    while (true) {
      ring_buffer_get_req_init(&req, BLOCKING);
      rb_.get(&req);

      auto message = flatbuffers::GetRoot<AlgorithmMessage>(req.data);

      if (message->shutdown()) {
        // Notify destroy we are done.
        pthread_barrier_wait(&shutdown_barrier_);
        sg_log("Exiting ContinuousAlgorithmExecutor: %d.\n", message->shutdown());
        rb_.set_done(&req);
        break;
      }

      rb_.set_done(&req);

      bool converged;
      uint64_t count_edges;
      executeAlgorithm(message->count_edges(), &converged, &count_edges);
    }
  }

private:
  // static ContinuousAlgorithmExecutor<VertexType, Algorithm, is_weighted>* instance;

  Ringbuffer rb_;

  Traversal* traversal_;

  pthread_barrier_t td_barrier_;

  pthread_barrier_t pull_barrier_;

  pthread_barrier_t apply_barrier_;

  pthread_barrier_t convergence_check_barrier_;

  pthread_barrier_t shutdown_barrier_;

  bool shutdown_flag_;

  const std::string prefix_;

  std::vector<util::Runnable*> threads_;

  std::vector<util::Runnable*> apply_threads_;

  std::vector<util::Runnable*> convergence_check_threads_;

  TileDistributor<VertexType, Algorithm, is_weighted>* td_;

  std::shared_ptr<Config> config_;

  std::shared_ptr<PerfEventManager> perf_event_manager_;

  std::shared_ptr<Algorithm> algorithm_;

  std::shared_ptr<VertexStore<VertexType> > vertex_store_;

  bool checkConvergence() {
    if (Algorithm::ASYNC) {
      for (auto& thread : convergence_check_threads_) {
        bool local_convergence = static_cast<ConvergenceChecker<VertexType, Algorithm,
            is_weighted>*>(thread)->isConverged();
        if (!local_convergence) {
          return false;
        }
      }
    } else {
      for (auto& thread : apply_threads_) {
        bool local_convergence =
            static_cast<ApplyExecutor<VertexType, Algorithm, is_weighted>*>(thread)->isConverged();
        if (!local_convergence) {
          return false;
        }
      }
    }
    return true;
  }
};

// template<typename VertexType, class Algorithm, bool is_weighted>
// ContinuousAlgorithmExecutor<VertexType, Algorithm, is_weighted>
//     * ContinuousAlgorithmExecutor<VertexType, Algorithm, is_weighted>::instance = nullptr;
// ALGORITHM_TEMPLATE_WEIGHTED_H(ContinuousAlgorithmExecutor)

}

#endif //EVOLVING_GRAPHS_CONTINOUS_ALGORITHM_EXECUTOR_H
