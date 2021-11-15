// SPDX-License-Identifier: MIT

#ifndef EVOLVING_GRAPHS_EDGE_INSERTER_H
#define EVOLVING_GRAPHS_EDGE_INSERTER_H

#include <memory>

#include <flatbuffers/flatbuffers.h>

#include "util/runnable.h"
#include "ring-buffer/ring-buffer.h"
#include "traversal/traversal.h"
#include "schema_generated.h"
#include "util/util.h"
#include "util/config.h"
#include "tile-manager/meta-tile-manager.h"
#include "core/meta-tile-store.h"
#include "perf-event/perf-event-scoped.h"
#include "perf-event/perf-event-manager.h"
#include "core/continuous-algorithm-executor.h"
#include "core/vertex-store.h"
#include "core/results-writer.h"

namespace evolving_graphs {

using namespace perf_event;

template<typename VertexType, class Algorithm, bool is_weighted>
class EdgeInserter : public util::Runnable {
public:
  EdgeInserter(Traversal* traversal, const Ringbuffer& control_rb, const Ringbuffer& sync_rb,
               const Ringbuffer& meta_tile_manager_sync_rb, size_t count_edges_present,
               std::shared_ptr<Config> config, 
               std::shared_ptr<MetaTileStore<VertexType, Algorithm, is_weighted> > meta_tile_store,
               std::shared_ptr<VertexStore<VertexType> > vertex_store,
               std::shared_ptr<PerfEventManager> perf_event_manager,
               std::shared_ptr<ContinuousAlgorithmExecutor<VertexType, Algorithm, is_weighted> > continuous_algorithm_executor,
               std::shared_ptr<Algorithm> algorithm)
  	: count_edges_present_(count_edges_present),
	  traversal_(traversal),
	  control_rb_(control_rb),
	  sync_rb_(sync_rb),
	  meta_tile_sync_rb_(meta_tile_manager_sync_rb),
	  config_(std::move(config)),
	  meta_tile_store_(std::move(meta_tile_store)),
	  vertex_store_(std::move(vertex_store)),
	  perf_event_manager_(std::move(perf_event_manager)),
	  continuous_algorithm_executor_(std::move(continuous_algorithm_executor)),
	  algorithm_(std::move(algorithm)) {
	}

protected:
  void run() override {
	  uint8_t shutdowns_received = 0;
	  bool shutdown = false;

	  uint64_t count_edges = count_edges_present_;

	  ResultsWriter<VertexType, Algorithm, is_weighted> results_writer(config_, vertex_store_);

	  // Only run the results writer if necessary.
	  if (config_->context_.enable_write_results) {
	    results_writer.start();
	  }

	  while (!shutdown) {
	    ring_buffer_req_t req{};
	    ring_buffer_get_req_init(&req, BLOCKING);
	    control_rb_.get(&req);

	    auto message = flatbuffers::GetRoot<MetaEdgeInserterMessage>(req.data);
	    if (message->shutdown()) {
	      ++shutdowns_received;
	      sg_dbg("Single shutdown received, %u of %u.\n",
	             shutdowns_received,
	             config_->context_.edge_inserters_count);
	      if (shutdowns_received == config_->context_.edge_inserters_count) {
	        // Set shutdown flag, but continue to process the final set of edges.
	        shutdown = true;
	      }
	    }

	    //    PerfEventScoped event(perf_event_manager_->getRingBuffer(),
	    //                          "insertion-" + std::to_string(count++),
	    //                          "EdgeInserter",
	    //                          0,
	    //                          config_->context_.enable_perf_event_collection);

	    // If in lazy algorithm mode, add message with the count of remaining MetaTileManagers to
	    // determine the last one which notifies the AlgoExecutor.
	    uint64_t remaining_meta_tile_managers_pointer = 0;
	    if (config_->context_.enable_dynamic_algorithm) {
	      if (config_->context_.enable_lazy_algorithm) {
	        ring_buffer_req_t meta_req{};
	        ring_buffer_put_req_init(&meta_req, BLOCKING, sizeof(remaining_meta_tile_managers_t));
	        meta_tile_sync_rb_.put(&meta_req);
	        sg_rb_check(&meta_req);
	        reinterpret_cast<remaining_meta_tile_managers_t*>(meta_req.data)->count =
	            static_cast<uint16_t>(message->messages()->size());
	        remaining_meta_tile_managers_pointer = reinterpret_cast<uint64_t>(meta_req.data);
	        meta_tile_sync_rb_.set_ready(&meta_req);
	      }
	    }

	    for (const auto& inner_message : *message->messages()) {
	      uint64_t meta_id = inner_message->meta_tile_manager_id();

	      MetaTileManager<VertexType, Algorithm, is_weighted>
	          * meta_tm =
	          meta_tile_store_->getMetaTileManager(meta_id);
	      Ringbuffer rb = meta_tm->control_rb();

	      flatbuffers::FlatBufferBuilder builder;
	      auto meta_message = CreateEdgeInserterControlMessage(builder,
	                                                           inner_message->message_pointer(),
	                                                           inner_message->size_message_pointer(),
	                                                           remaining_meta_tile_managers_pointer,
	                                                           false);
	      builder.Finish(meta_message);

	      util::sendMessage(rb, builder);
	      sg_dbg("Forwarded: Id: %lu, count edges: %lu.\n",
	             inner_message->meta_tile_manager_id(),
	             inner_message->count_edges());
	    }

	    // Wait for message from all inserting MetaTileManagers.
	    // Only wait for this message if necessary by config.
	    if (config_->context_.enable_dynamic_algorithm) {
	      if (!config_->context_.enable_lazy_algorithm) {
	        bool reexecute_algorithm = false;
	        sg_dbg("Waiting for all %d TileManagers to reply.\n", message->messages()->size());
	        for (uint32_t i = 0; i < message->messages()->size(); ++i) {
	          ring_buffer_req_t sync_req{};
	          ring_buffer_get_req_init(&sync_req, BLOCKING);
	          sync_rb_.get(&sync_req);
	          auto edges_inserted_message = flatbuffers::GetRoot<EdgesInsertedMessage>(sync_req.data);
	          if (edges_inserted_message->algorithm_reexecution_needed()) {
	            reexecute_algorithm = true;
	          }

	          sync_rb_.set_done(&sync_req);

	          sg_dbg("Reply %u of %u received.\n", i + 1, message->messages()->size());
	          count_edges += message->messages()->Get(i)->count_edges();
	        }
	        if (count_edges >= config_->context_.min_count_edges_algorithm) {
	          sg_dbg("All %d replies received.\n", message->messages()->size());
	          // Switch active current and active next as the algorithms write onto next for now.
	          std::swap(vertex_store_->vertices_.active_current,
	                    vertex_store_->vertices_.active_next);

	          bool converged;
	          uint64_t total_count_edges;
	          //sg_log("Edges inserted: %lu\n", count_edges);

	          PerfEventScoped event_scoped(perf_event_manager_->getRingBuffer(),
	                                       "execute-algorithm",
	                                       "EdgeInserter",
	                                       0,
	                                       config_->context_.enable_perf_event_collection);

	          // Reinitialize the algorithm if running in the reexecution mode.
	          if (reexecute_algorithm || config_->context_.enable_algorithm_reexecution) {
	            sg_log("Reexecuting algorithm %d.\n", reexecute_algorithm);
	            continuous_algorithm_executor_->initAlgorithm();
	          }

	          // If edge APIs are disabled, mark graph as active to enable processing of new updates.
	          if (unlikely(!config_->context_.enable_edge_apis)) {
	            algorithm_->resetWithoutEdgeApi();
	          }

	          continuous_algorithm_executor_->executeAlgorithm(
	              count_edges,
	              &converged,
	              &total_count_edges,
	              "Test");

	          // Notify the results writer if necessary and wait for its completion.
	          if (config_->context_.enable_write_results) {
	            results_writer.notifyAndWait();
	          }
	        }
	      }
	    }

	    control_rb_.set_done(&req);
	  }

	  if (config_->context_.enable_write_results) {
	    results_writer.shutdown();
	  }
	}

private:
	size_t count_edges_present_;

	Traversal* traversal_;

	Ringbuffer control_rb_;

	Ringbuffer sync_rb_;

	Ringbuffer meta_tile_sync_rb_;

	std::shared_ptr<Config> config_;

	std::shared_ptr<MetaTileStore<VertexType, Algorithm, is_weighted> > meta_tile_store_;

	std::shared_ptr<VertexStore<VertexType> > vertex_store_;

	std::shared_ptr<PerfEventManager> perf_event_manager_;

	std::shared_ptr<ContinuousAlgorithmExecutor<VertexType, Algorithm, is_weighted> > continuous_algorithm_executor_;

	std::shared_ptr<Algorithm> algorithm_;
};

// ALGORITHM_TEMPLATE_WEIGHTED_H(EdgeInserter)

}

#endif //EVOLVING_GRAPHS_EDGE_INSERTER_H
