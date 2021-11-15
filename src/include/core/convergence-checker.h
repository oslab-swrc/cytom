// SPDX-License-Identifier: MIT

#ifndef EVOLVING_GRAPHS_CONVERGENCE_CHECKER_H
#define EVOLVING_GRAPHS_CONVERGENCE_CHECKER_H

#include <memory>

#include "util/runnable.h"
#include "perf-event/perf-event-manager.h"
#include "perf-event/perf-event-scoped.h"
#include "util/config.h"
#include "core/vertex-store.h"

namespace evolving_graphs {

using namespace perf_event;
#define STEP_SIZE 4096

template<class VertexType, class Algorithm, bool is_weighted>
class ConvergenceChecker : public util::Runnable {
public:
  ConvergenceChecker(uint32_t id, pthread_barrier_t* barrier, bool* shutdown_flag, 
  					 std::shared_ptr<Config> config,
                     std::shared_ptr<VertexStore<VertexType> > vertex_store,
                     std::shared_ptr<PerfEventManager> perf_event_manager,
                     std::shared_ptr<uint64_t> current_tile_id)
  	: id_(id), barrier_(barrier), shutdown_flag_(shutdown_flag), converged_(true),
  	 	config_(std::move(config)),
	    vertex_store_(std::move(vertex_store)),
	    perf_event_manager_(std::move(perf_event_manager)),
	    current_tile_id_(std::move(current_tile_id)) {}

  inline bool isConverged() const { return converged_;}

  inline void resetCurrentTileID() { *current_tile_id_ = 0;}

protected:
  void run() override {
	  while (true) {
	    pthread_barrier_wait(barrier_);
	    if (*shutdown_flag_) {
	      break;
	    }

	    {
	      PerfEventScoped scoped(perf_event_manager_->getRingBuffer(),
	                             "checkConvergence-" + std::to_string(id_),
	                             "AlgorithmExecutor",
	                             id_,
	                             config_->context_.enable_perf_event_collection);
	      //      uint8_t count_executors = Config::context_.algorithm_executor_count;
	      uint64_t count_vertices = config_->context_.count_vertices;

	      uint64_t next_begin_id = 0;

	      converged_ = true;

	      while (next_begin_id < count_vertices) {
	        next_begin_id = smp_faa(current_tile_id_.get(), STEP_SIZE);
	        uint64_t end = std::min(next_begin_id + STEP_SIZE, count_vertices);

	        int64_t current = next_begin_id / 8;
	        int64_t end_id = end / 8;

	        while (end_id - current > 64) {
	          if ((uint64_t) vertex_store_->vertices_.active_next[current] != 0) {
	            converged_ = false;
	            break;
	          }
	          current += 8;
	        }

	        // Reset to the global coordinates.
	        current = current * 8;
	        for (uint64_t current_id = current; current_id <= end; ++current_id) {
	          if (eval_bool_array(vertex_store_->vertices_.active_next, current_id)) {
	            converged_ = false;
	            break;
	          }
	        }

	        // If convergence not reached: cut off any further evaluation.
	        if (!converged_) {
	          bool successful;
	          do {
	            uint64_t old_value = *current_tile_id_;
	            successful = smp_cas(current_tile_id_.get(), old_value, count_vertices);
	          } while (!successful);
	        }
	      }
	    }

	    int return_code = pthread_barrier_wait(barrier_);
	    if (return_code == PTHREAD_BARRIER_SERIAL_THREAD) {
	      resetCurrentTileID();
	    }
	  }
	}

private:
  std::shared_ptr<uint64_t> current_tile_id_;

  uint32_t id_;

  pthread_barrier_t* barrier_;

  bool* shutdown_flag_;

  bool converged_;

  std::shared_ptr<Config> config_;

  std::shared_ptr<VertexStore<VertexType> > vertex_store_;

  std::shared_ptr<PerfEventManager> perf_event_manager_;
};

// template<class VertexType, class Algorithm, bool is_weighted>
// uint64_t ConvergenceChecker<VertexType, Algorithm, is_weighted>::current_tile_id_ = 0;
// ALGORITHM_TEMPLATE_WEIGHTED_H(ConvergenceChecker)

}

#endif //EVOLVING_GRAPHS_CONVERGENCE_CHECKER_H
