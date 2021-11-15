// SPDX-License-Identifier: MIT

#ifndef EVOLVING_GRAPHS_RMAT_EDGE_PROVIDER_H
#define EVOLVING_GRAPHS_RMAT_EDGE_PROVIDER_H

#include <cstdint>
#include <memory>

#include "edge-inserter/rmat-context.h"
#include "edge-inserter/i-edge-provider.h"
#include "edge-inserter/rmat-edge-provider.h"
#include "util/runnable.h"
#include "util/config.h"
#include "util/util.h"
#include "core/vertex-store.h"
#include "core/meta-tile-store.h"
#include "schema_generated.h"

namespace evolving_graphs {

template<typename VertexType, class Algorithm, bool is_weighted>
class RmatEdgeProvider : public util::Runnable, IEdgeProvider<VertexType, Algorithm, is_weighted> {
public:
  RmatEdgeProvider(const Ringbuffer& edge_inserter_rb, pthread_barrier_t* in_memory_barrier,
                   uint64_t id, uint64_t start_id, uint64_t end_id, 
                   std::shared_ptr<Config> config,
                   std::shared_ptr<VertexStore<VertexType> > vertex_store,
		   const std::shared_ptr<MetaTileStore<VertexType, Algorithm, is_weighted> > & meta_tile_store)
  : IEdgeProvider<VertexType, Algorithm, is_weighted>(edge_inserter_rb,
                                                        in_memory_barrier,
                                                        id,
                                                        true,
                                                        config,
							meta_tile_store),
      start_id_(start_id),
      end_id_(end_id),
      rmat_context_(RMAT_A, RMAT_B, RMAT_C, config_->context_.max_count_vertices, RMAT_SEED),
      config_(std::move(config)),
      vertex_store_(std::move(vertex_store)){}

protected:
  void run() override {
	  for (uint64_t i = start_id_; i < end_id_; ++i) {
	    edge_t next_edge = rmat_context_.getEdge(i);

	    // If necessary, rewrite src and tgt ids.
	    // if (config_->context_.enable_rewrite_ids) {
	    //   next_edge.src = vertex_store_->getTranslatedId(next_edge.src);
	    //   next_edge.tgt = vertex_store_->getTranslatedId(next_edge.tgt);
	    // }

	    Edge edge(next_edge.src, next_edge.tgt, 0.0);
	    this->addEdge(edge);
	  }
	  // Send out remaining edges.
	  this->insertionFinished();
	}

private:
  uint64_t start_id_;
  uint64_t end_id_;

  RMATContext rmat_context_;

  std::shared_ptr<Config> config_;

  std::shared_ptr<VertexStore<VertexType> > vertex_store_;
};

// ALGORITHM_TEMPLATE_WEIGHTED_H(RmatEdgeProvider)

}

#endif //EVOLVING_GRAPHS_RMAT_EDGE_PROVIDER_H
