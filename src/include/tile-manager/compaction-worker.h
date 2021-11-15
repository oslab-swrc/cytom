// SPDX-License-Identifier: MIT

#ifndef EVOLVING_GRAPHS_COMPACTION_WORKER_H
#define EVOLVING_GRAPHS_COMPACTION_WORKER_H

#include <cstdint>
#include <memory>

#include "util/runnable.h"
#include "util/config.h"
// #include "tile-store.h"

namespace evolving_graphs {
template<typename VertexType, class Algorithm, bool is_weighted>
class TileStore;

template<typename VertexType, class Algorithm, bool is_weighted>
class CompactionWorker : public util::Runnable {
public:
  CompactionWorker(std::shared_ptr<Config> config, 
  				   std::shared_ptr<TileStore<VertexType, Algorithm, is_weighted> > tile_store): 
  				   CURRENT_TILE(0),
  				   config_(std::move(config)),
  				   tile_store_(std::move(tile_store)) {}

protected:
  void run() override {
	  size_t count_tiles = config_->context_.tile_store_config.count_tiles;
	  while (true) {
	    uint32_t current_tile = smp_faa(&CURRENT_TILE, 1);
	    if (current_tile >= count_tiles) {
	      break;
	    }
	    TileManager<VertexType, Algorithm, is_weighted>
	        * tm = tile_store_->getTileManager(current_tile);
	    tm->compactStorage();
	  }
	}

private:
	uint32_t CURRENT_TILE;

	std::shared_ptr<Config> config_;

  	std::shared_ptr<TileStore<VertexType, Algorithm, is_weighted> > tile_store_;
};

// template<typename VertexType, class Algorithm, bool is_weighted>
// uint32_t CompactionWorker<VertexType, Algorithm, is_weighted>::CURRENT_TILE = 0;
// ALGORITHM_TEMPLATE_WEIGHTED_H(CompactionWorker)

}

#endif //EVOLVING_GRAPHS_COMPACTION_WORKER_H
