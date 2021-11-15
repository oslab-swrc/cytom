// SPDX-License-Identifier: MIT

#ifndef EVOLVING_GRAPHS_BINARY_EDGE_PROVIDER_INTERFACE_H
#define EVOLVING_GRAPHS_BINARY_EDGE_PROVIDER_INTERFACE_H

// #include <memory>

// #include "util/runnable.h"
// #include "edge-inserter/i-edge-provider.h"
// #include "util/config.h"
// #include "core/vertex-store.h"
// #include "util/datatypes.h"
// #include "core/jobs.h"
#include "schema_generated.h"

namespace evolving_graphs {

// template<typename VertexType, class Algorithm, bool is_weighted>
class BinaryEdgeProviderInterface {
public:
  // BinaryEdgeProviderInterface()
  // : IEdgeProvider<VertexType, Algorithm, is_weighted>(edge_inserter_rb,
  //                                                       in_memory_barrier,
  //                                                       id,
  //                                                       true,
  //                                                       config,
		// 					                            meta_tile_store,
  //                                                       vertex_store) {}
	      // start_id_(start_id),
	      // end_id_(end_id),
	      // jobs_(jobs),
	      // inserter_id_(id),
	      // config_(std::move(config)),
	      // vertex_store_(std::move(vertex_store)) {}

virtual void addEdgeWrapper(const Edge & edge) = 0;

virtual void insertionFinishedWrapper() = 0;

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

		// // file_edge_t next_edge = *file_edges_[i];
		// Edge converted_edge(static_cast<vertex_id_t>(jobs_->file_edges[i]->src),
		//                     static_cast<vertex_id_t>(jobs_->file_edges[i]->tgt), 0.0);

// 		// If necessary, rewrite src and tgt ids.
		// if (config_->context_.enable_rewrite_ids) {
		//   converted_edge.mutate_src(vertex_store_->getTranslatedId(converted_edge.src()));
		//   converted_edge.mutate_tgt(vertex_store_->getTranslatedId(converted_edge.tgt()));
		// }

// 		this->addEdge(converted_edge);
// 	}
// 	// Send out remaining edges.
// 	this->insertionFinished();
// 	}

// private:
//   uint64_t start_id_;

//   uint64_t end_id_;

//   // file_edge_t* file_edges_;
  
//   std::shared_ptr<Config> config_;

//   std::shared_ptr<VertexStore<VertexType> > vertex_store_;

//   // std::vector<file_edge_t*> * file_edges_;
//   Jobs * jobs_;

//   uint64_t inserter_id_;
};

// ALGORITHM_TEMPLATE_WEIGHTED_H(BinaryEdgeProvider)

}

#endif //EVOLVING_GRAPHS_BINARY_EDGE_PROVIDER_INTERFACE_H
