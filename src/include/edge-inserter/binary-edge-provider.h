// SPDX-License-Identifier: MIT

#ifndef EVOLVING_GRAPHS_BINARY_EDGE_PROVIDER_H
#define EVOLVING_GRAPHS_BINARY_EDGE_PROVIDER_H

#include <memory>
#include <unistd.h>

// #include "util/runnable.h"
#include "edge-inserter/binary-edge-provider-interface.h"
#include "util/config.h"
#include "core/vertex-store.h"
#include "util/datatypes.h"
#include "core/jobs.h"

namespace evolving_graphs {

template<typename VertexType, class Algorithm, bool is_weighted>
class BinaryEdgeProvider
    : public BinaryEdgeProviderInterface, public IEdgeProvider<VertexType, Algorithm, is_weighted> {
public:
  BinaryEdgeProvider(const Ringbuffer& edge_inserter_rb, 
  					 pthread_barrier_t* in_memory_barrier,
                     uint64_t id, 
                     // uint64_t start_id,
                     // uint64_t end_id, 
                     const std::shared_ptr<Config> & config,
                     const std::shared_ptr<VertexStore<VertexType> > & vertex_store,
		     		 const std::shared_ptr<MetaTileStore<VertexType, Algorithm, is_weighted> > & meta_tile_store)
  : IEdgeProvider<VertexType, Algorithm, is_weighted>(edge_inserter_rb,
                                                        in_memory_barrier,
                                                        id,
                                                        true,
                                                        config,
                                          meta_tile_store,
                                                        vertex_store) {}

void addEdgeWrapper(const Edge& edge) {
	this->addEdge(edge);
}

void insertionFinishedWrapper() {
	this->insertionFinished();
}

// protected:
//   void run() override {
// 		std::unique_lock<std::mutex> lock(jobs_->mutexes[inserter_id_]);

// 		while(jobs_->file_edges[end_id_ - 1] == nullptr) {
// 			sg_log("Edge Provider %d is waiting...", inserter_id_);
// 			jobs_->cvs[inserter_id_].wait(lock);
// 		}

// 		lock.unlock();

// 	for (uint64_t i = start_id_; i < end_id_; ++i) {

// 			// lock.unlock();
// 			// while(!jobs->file_edges[i])
// 		//  	usleep(100);

// 		// file_edge_t next_edge = *file_edges_[i];
// 		Edge converted_edge(static_cast<vertex_id_t>(jobs_->file_edges[i]->src),
// 		                    static_cast<vertex_id_t>(jobs_->file_edges[i]->tgt), 0.0);

// 		// If necessary, rewrite src and tgt ids.
// 		if (config_->context_.enable_rewrite_ids) {
// 		  converted_edge.mutate_src(vertex_store_->getTranslatedId(converted_edge.src()));
// 		  converted_edge.mutate_tgt(vertex_store_->getTranslatedId(converted_edge.tgt()));
// 		}

// 		this->addEdge(converted_edge);
// 	}
// 	// Send out remaining edges.
// 	this->insertionFinished();
// 	}
};

// ALGORITHM_TEMPLATE_WEIGHTED_H(BinaryEdgeProvider)

}

#endif //EVOLVING_GRAPHS_BINARY_EDGE_PROVIDER_H
