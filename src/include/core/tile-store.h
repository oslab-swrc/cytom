// SPDX-License-Identifier: MIT

#ifndef EVOLVING_GRAPHS_TILE_STORE_H
#define EVOLVING_GRAPHS_TILE_STORE_H

#include <memory>

#include "util/datatypes.h"
#include "util/config.h"
#include "util/util.h"
#include "util/runnable.h"
#include "tile-manager/compaction-worker.h"
#include "tile-manager/tile-manager.h"
#include "traversal/traversal.h"
#include "schema_generated.h"

namespace evolving_graphs {

template<typename VertexType, class Algorithm, bool is_weighted>
class TileStore: public std::enable_shared_from_this<TileStore<VertexType, Algorithm, is_weighted> > {
public:
  TileStore(std::shared_ptr<Config> config):tile_managers_(nullptr),
                                        traversal_to_tm_(nullptr),
                                        meta_tile_to_tm_(nullptr),
                                        config_(std::move(config)){}

  void init(Traversal* traversal, const std::shared_ptr<VertexStore<VertexType> > & vertex_store) {
    uint64_t count_tiles_per_dimension = config_->context_.tile_store_config.count_tiles_per_dimension;
    uint64_t count_tiles = config_->context_.tile_store_config.count_tiles;
    
    tile_managers_ =
        new TileManager<VertexType, Algorithm, is_weighted>** [count_tiles_per_dimension];
    traversal_to_tm_ = new TileManager<VertexType, Algorithm, is_weighted>* [count_tiles];
    //  meta_tile_to_tm_ = new TileManager<VertexType, Algorithm, is_weighted>* [count_tiles];
    meta_tile_to_tm_ = new std::vector<TileManager<VertexType, Algorithm,
        is_weighted>*>[config_->context_.tile_store_config.td_count_meta_tiles];

    for (uint64_t i = 0; i < count_tiles_per_dimension; ++i) {
      tile_managers_[i] =
          new TileManager<VertexType, Algorithm, is_weighted>* [count_tiles_per_dimension];
      for (uint64_t j = 0; j < count_tiles_per_dimension; ++j) {
        int64_t traversal_id = traversal->xy2d(count_tiles_per_dimension, i, j);
	tile_managers_[i][j] =
            new TileManager<VertexType, Algorithm, is_weighted>(i, j, traversal_id, config_, vertex_store);
	traversal_to_tm_[traversal_id] = tile_managers_[i][j];
      }
    }

    if (config_->context_.enable_two_level_tile_distributor) {
      uint64_t max_count_meta_tile = config_->context_.tile_store_config.td_count_tiles_per_meta_tiles;
      uint64_t current_meta_tile = 0;

      for (uint64_t traversal_id = 0; traversal_id < count_tiles; ++traversal_id) {
        TileManager<VertexType, Algorithm, is_weighted>* tm = traversal_to_tm_[traversal_id];
        meta_tile_to_tm_[current_meta_tile].push_back(tm);
        tm->setMetaTileId(current_meta_tile);
        if (meta_tile_to_tm_[current_meta_tile].size() >= max_count_meta_tile) {
          ++current_meta_tile;
        }
      }

      auto count_meta_tiles = config_->context_.tile_store_config.td_count_meta_tiles;
      for (uint64_t meta_tile_id = 0; meta_tile_id < count_meta_tiles; ++meta_tile_id) {
        size_t meta_tile_size = meta_tile_to_tm_[meta_tile_id].size();
        if (meta_tile_size != config_->context_.tile_store_config.td_count_tiles_per_meta_tiles) {
          sg_log("Meta tile %lu has wrong size: %lu instead of %lu\n",
                 meta_tile_id,
                 meta_tile_size,
                 config_->context_.tile_store_config.td_count_tiles_per_meta_tiles);
          util::die(1);
        }
      }
    }
  }

  Coordinates getTileManagerCoordinates(uint64_t tile_manager_id) {
    uint64_t tiles_per_dim =
        config_->context_.tile_store_config.count_tiles_per_dimension;
    Coordinates coords{};
    coords.x = tile_manager_id / tiles_per_dim;
    coords.y = tile_manager_id - coords.x * tiles_per_dim;
    if (coords.x < tiles_per_dim && coords.y < tiles_per_dim) {
      return coords;
    }
    return Coordinates{UINT64_MAX, UINT64_MAX};
  }

  Coordinates getTileManagerCoordinates(const edge_t& edge) {
    if (edge.src < config_->context_.count_vertices &&
        edge.tgt < config_->context_.count_vertices) {
      uint64_t x = edge.src / config_->context_.tile_store_config.vertices_per_tile;
      uint64_t y = edge.tgt / config_->context_.tile_store_config.vertices_per_tile;
      return Coordinates{x, y};
    }
    return Coordinates{UINT64_MAX, UINT64_MAX};
  }

  inline TileManager<VertexType, Algorithm, is_weighted>* getTileManager(const Coordinates& coords) {
    if (coords.x < UINT64_MAX && coords.y < UINT64_MAX) {
      return tile_managers_[coords.x][coords.y];
    }
    return nullptr;
  }

  inline TileManager<VertexType, Algorithm, is_weighted>* getTileManager(uint64_t tile_manager_id) {
    Coordinates coords = getTileManagerCoordinates(tile_manager_id);
    if (coords.x < UINT64_MAX && coords.y < UINT64_MAX) {
      return tile_managers_[coords.x][coords.y];
    }
    return nullptr;
  }

  inline TileManager<VertexType, Algorithm, is_weighted>* getTileManager(const edge_t& edge) {
    Coordinates coords = getTileManagerCoordinates(edge);
    if (coords.x < UINT64_MAX && coords.y < UINT64_MAX) {
      return tile_managers_[coords.x][coords.y];
    }
    return nullptr;
  }

  inline TileManager<VertexType, Algorithm, is_weighted>* getTileManager(const Edge& edge) {
    edge_t local_edge{edge.src(), edge.tgt()};
    return getTileManager(local_edge);
  } 

  inline TileManager<VertexType, Algorithm, is_weighted>* getTileManagerByTraversal(
      uint64_t traversal_id) {
    return traversal_to_tm_[traversal_id];
  }

  inline std::vector<TileManager<VertexType, Algorithm, is_weighted>*>* getListOfTilesPerMetaTile(
      uint64_t meta_tile_id) {
    return &meta_tile_to_tm_[meta_tile_id];
  }

  inline uint64_t getTileManagerId(const edge_t& edge) {
    TileManager<VertexType, Algorithm, is_weighted>* tile_manager = getTileManager(edge);
    if (tile_manager == nullptr) {
      return UINT64_MAX;
    }
    return tile_manager->getId();
  }

  void doGlobalCompaction() {
    for (uint64_t i = 0;
         i < config_->context_.tile_store_config.count_tiles_per_dimension;
         ++i) {
      for (uint64_t j = 0;
           j < config_->context_.tile_store_config.count_tiles_per_dimension; ++j) {
        tile_managers_[i][j]->compactStorage();
      }
    }
  }

  void doGlobalCompaction(uint64_t count_threads) {
    std::vector<util::Runnable*> threads;

    for (uint32_t i = 0; i < count_threads; ++i) {
      threads.push_back(new CompactionWorker<VertexType, Algorithm, is_weighted>(config_, this->shared_from_this()));
    }

    uint8_t i = 0;
    for (auto& thread : threads) {
      thread->start();
      thread->setName("Compactor_" + std::to_string(i++));
    }

    for (auto& thread : threads) {
      thread->join();
      delete thread;
    }
  }

  size_t countEdges() {
    size_t count = 0;
    for (uint64_t i = 0;
         i < config_->context_.tile_store_config.count_tiles_per_dimension;
         ++i) {
      for (uint64_t j = 0;
           j < config_->context_.tile_store_config.count_tiles_per_dimension; ++j) {
        count += tile_managers_[i][j]->countEdges();
      }
    }
    return count;
  }

  size_t countBytes() {
    size_t count = 0;
    for (uint64_t i = 0;
         i < config_->context_.tile_store_config.count_tiles_per_dimension;
         ++i) {
      for (uint64_t j = 0;
           j < config_->context_.tile_store_config.count_tiles_per_dimension; ++j) {
        count += tile_managers_[i][j]->countBytes();
      }
    }
    return count;
  }

  size_t countCompactVertexStorage() {
    size_t count = 0;
    for (uint64_t i = 0;
         i < config_->context_.tile_store_config.count_tiles_per_dimension;
         ++i) {
      for (uint64_t j = 0;
           j < config_->context_.tile_store_config.count_tiles_per_dimension; ++j) {
        if (tile_managers_[i][j]->usesCompactVertexStorage()) {
          ++count;
        }
      }
    }
    return count;
  }

  size_t countCompactions() {
    size_t count = 0;
    for (uint64_t i = 0;
         i < config_->context_.tile_store_config.count_tiles_per_dimension;
         ++i) {
      for (uint64_t j = 0;
           j < config_->context_.tile_store_config.count_tiles_per_dimension; ++j) {
        count += tile_managers_[i][j]->countCompactions();
      }
    }
    return count;
  }

  void destroy() {
    for (uint64_t i = 0;
         i < config_->context_.tile_store_config.count_tiles_per_dimension;
         ++i) {
      for (uint64_t j = 0;
           j < config_->context_.tile_store_config.count_tiles_per_dimension; ++j) {
        delete tile_managers_[i][j];
      }
      delete[] tile_managers_[i];
    }
    delete[] tile_managers_;
    delete[] traversal_to_tm_;
    delete[] meta_tile_to_tm_;
  }

private:
  std::shared_ptr<Config> config_;

  TileManager<VertexType, Algorithm, is_weighted>*** tile_managers_;

  // Maps the traversal ID to a specific TileManager.
  TileManager<VertexType, Algorithm, is_weighted>** traversal_to_tm_;

  // Maps the meta tiles to a list of TileManagers associated with that meta tile.
  std::vector<TileManager < VertexType, Algorithm, is_weighted>*>* meta_tile_to_tm_;
};

}

#endif //EVOLVING_GRAPHS_TILE_STORE_H
