// SPDX-License-Identifier: MIT

#ifndef EVOLVING_GRAPHS_APPLY_EXECUTOR_H
#define EVOLVING_GRAPHS_APPLY_EXECUTOR_H

#include <memory>

#include "util/runnable.h"
#include "util/config.h"
#include "perf-event/perf-event-manager.h"
#include "perf-event/perf-event-scoped.h"
#include "core/vertex-store.h"

namespace evolving_graphs {

using namespace perf_event;
#define STEP_SIZE 4096

template<typename VertexType, class Algorithm, bool is_weighted>
class ApplyExecutor : public util::Runnable {
public:
  inline ApplyExecutor(uint32_t id, pthread_barrier_t* barrier, bool* shutdown_flag, 
  						std::shared_ptr<Config> config, 
  						std::shared_ptr<PerfEventManager> perf_event_manager,
  						std::shared_ptr<Algorithm> algorithm,
  						std::shared_ptr<uint64_t> current_tile_id)
  	: id_(id), barrier_(barrier), 
  		shutdown_flag_(shutdown_flag), 
  		converged_(true),
  		current_tile_id_(move(current_tile_id)),
  		config_(std::move(config)),
  		perf_event_manager_(std::move(perf_event_manager)),
  		algorithm_(std::move(algorithm)) {}

  inline bool isConverged() const{ return converged_; }

  inline void resetCurrentTileID() { *current_tile_id_ = 0; }

protected:
  void run() override {
	  uint32_t iteration = 0;
	  while (true) {
	    pthread_barrier_wait(barrier_);
	    if (*shutdown_flag_) {
	      break;
	    }

	    {
	     PerfEventScoped scoped(perf_event_manager_->getRingBuffer(),
	                             "apply-" + std::to_string(id_),
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

	        if (Algorithm::USE_CHANGED) {
	          int64_t current = next_begin_id / 8;
	          int64_t end_id = end / 8;

	          while (end_id - current > 8) {
	            uint64_t real_id = current * 8;
	            if (algorithm_->isVertexActiveApplyMultiStep(real_id)) {
	              for (uint64_t i = 0; i < 64; ++i) {
	                uint64_t shifted_real_id = real_id + i;
	                if (algorithm_->isVertexActiveApply(shifted_real_id)) {
	                  if (!algorithm_->apply(shifted_real_id, iteration)) {
	                    converged_ = false;
	                  }
	                }
	              }
	            }
	            current += 8;
	          }
	          // Reset to the global coordinates for the potentially overhanging vertices.
	          current = current * 8;
	          for (uint64_t current_id = current; current_id < end; ++current_id) {
	            if (algorithm_->isVertexActiveApply(current_id)) {
	              if (!algorithm_->apply(current_id, iteration)) {
	                converged_ = false;
	              }
	            }
	          }
	          //                    for (uint64_t i = next_begin_id; i < end; ++i) {
	          //                      if (algorithm_->isVertexActiveApply(i)) {
	          //                        if (!algorithm_->apply(i, iteration)) {
	          //                          converged_ = false;
	          //                        }
	          //                      }
	          //                    }
	        } else {
	          for (uint64_t i = next_begin_id; i < end; ++i) {
	            if (!algorithm_->apply(i, iteration)) {
	              converged_ = false;
	            }
	          }
	        }
	      }
	    }

	    int return_code = pthread_barrier_wait(barrier_);
	    if (return_code == PTHREAD_BARRIER_SERIAL_THREAD) {
	      resetCurrentTileID();
	    }
	    ++iteration;
	  }
	}

private:
  std::shared_ptr<uint64_t> current_tile_id_;

  uint32_t id_;

  pthread_barrier_t* barrier_;

  bool* shutdown_flag_;

  bool converged_;

  std::shared_ptr<Config> config_;

  std::shared_ptr<PerfEventManager> perf_event_manager_;

  std::shared_ptr<Algorithm> algorithm_;
};

// template<typename VertexType, class Algorithm, bool is_weighted>
// uint64_t ApplyExecutor<VertexType, Algorithm, is_weighted>::current_tile_id_ = 0;
// ALGORITHM_TEMPLATE_WEIGHTED_H(ApplyExecutor)

}

#endif //EVOLVING_GRAPHS_APPLY_EXECUTOR_H
