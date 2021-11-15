// SPDX-License-Identifier: MIT

#ifndef EVOLVING_GRAPHS_META_TILE_STORE_H
#define EVOLVING_GRAPHS_META_TILE_STORE_H

#include <memory>

#include <flatbuffers/flatbuffers.h>

#include "util/datatypes.h"
#include "tile-manager/meta-tile-manager.h"
#include "schema_generated.h"
#include "traversal/traversal.h"
#include "util/config.h"
#include "util/util.h"
#include "traversal/hilbert.h"
#include "perf-event/perf-event-scoped.h"
#include "perf-event/perf-event-manager.h"
#include "core/tile-store.h"
#include "core/algorithm-executor.h"
#include "core/apply-executor.h"
#include "core/vertex-store.h"

namespace evolving_graphs {

using namespace perf_event;

template<typename VertexType, class Algorithm, bool is_weighted>
class MetaTileStore {
public:
  MetaTileStore(std::shared_ptr<Config> config, std::shared_ptr<TileStore<VertexType, Algorithm, is_weighted> > tile_store):meta_tile_managers_(nullptr),
                                            tile_manager_mapping_(nullptr),
                                            config_(std::move(config)),
                                            tile_store_(std::move(tile_store)) {}

  void init(const Ringbuffer& edge_inserter_sync_rb,
                   const Ringbuffer& meta_tile_manager_sync_rb,
                   pthread_barrier_t* read_edges_barrier,
                   const std::shared_ptr<Algorithm> & algorithm,
		   const std::shared_ptr<ContinuousAlgorithmExecutor<VertexType, Algorithm, is_weighted> > & continuous_algorithm_executor,
		   const std::shared_ptr<VertexStore<VertexType> > & vertex_store) {
    uint8_t meta_tile_manager_count = config_->context_.meta_tile_manager_count;
    meta_tile_managers_ =
        new MetaTileManager<VertexType, Algorithm, is_weighted>* [meta_tile_manager_count];
    
    for (uint8_t i = 0; i < meta_tile_manager_count; ++i) {
      meta_tile_managers_[i] =
          new MetaTileManager<VertexType, Algorithm, is_weighted>(edge_inserter_sync_rb,
                                                                  meta_tile_manager_sync_rb,
                                                                  i,
                                                                  read_edges_barrier,
                                                                  config_,
                                                                  algorithm,
								  tile_store_,
								  continuous_algorithm_executor,
								  vertex_store);
      // Pin to proper NUMA socket.
      uint32_t num_socket = util::NUM_SOCKET;
      uint32_t socket_id = i % num_socket;
      uint32_t core_id = util::cpu_id_t::ANYWHERE;
      uint32_t smt_id = util::cpu_id_t::ANYWHERE;
      util::cpu_id_t cpu_id(socket_id, core_id, smt_id);

      meta_tile_managers_[i]->setAffinity(cpu_id);
    }

    uint64_t tiles_per_dimension =
        config_->context_.tile_store_config.count_tiles_per_dimension;
    tile_manager_mapping_ = new uint8_t* [tiles_per_dimension];

    for (uint64_t i = 0; i < tiles_per_dimension; ++i) {
      tile_manager_mapping_[i] = new uint8_t[tiles_per_dimension];
      for (uint64_t j = 0; j < tiles_per_dimension; ++j) {
        //      tile_manager_mapping_[i][j] =
        //          static_cast<uint8_t>((i * tiles_per_dimension + j) %
        //                               meta_tile_manager_count);
        tile_manager_mapping_[i][j] =
            static_cast<uint8_t>((i + j) % meta_tile_manager_count);
      }
    }
  }

  inline void start() {
    uint8_t meta_tile_manager_count = config_->context_.meta_tile_manager_count;
    for (uint8_t i = 0; i < meta_tile_manager_count; ++i) {
      meta_tile_managers_[i]->start();
      meta_tile_managers_[i]->setName("MetaTM_" + std::to_string(i));
    }
  }

  inline MetaTileManager<VertexType, Algorithm, is_weighted>* getMetaTileManager(uint64_t id) {
    return meta_tile_managers_[id];
  }

  inline MetaTileManager<VertexType, Algorithm, is_weighted>* getMetaTileManager(const edge_t& edge) {
    uint64_t id = getMetaTileManagerId(edge);
    if (id < UINT64_MAX) {
      return meta_tile_managers_[id];
    }
    return nullptr;
  }

  inline MetaTileManager<VertexType, Algorithm, is_weighted>* getMetaTileManager(const Edge& edge) {
    edge_t local_edge{edge.src(), edge.tgt()};
    return getMetaTileManager(local_edge);
  }

  uint64_t getMetaTileManagerId(const Edge& edge) {
    edge_t local_edge{edge.src(), edge.tgt()};
    return getMetaTileManagerId(local_edge);
  }

  uint64_t getMetaTileManagerId(const edge_t& edge) {
    Coordinates
        coords = tile_store_->getTileManagerCoordinates(edge);
    if (coords.x < UINT64_MAX && coords.y < UINT64_MAX) {
      return tile_manager_mapping_[coords.x][coords.y];
    }
    return UINT64_MAX;
  }

  inline size_t getCountEdgesToRead() {
    uint8_t meta_tile_manager_count = config_->context_.meta_tile_manager_count;
    size_t count_edges = 0;

    for (uint8_t i = 0; i < meta_tile_manager_count; ++i) {
      count_edges += meta_tile_managers_[i]->getCountEdgesToRead();
    }

    return count_edges;
  }

  void destroy() {
    uint8_t meta_tile_manager_count = config_->context_.meta_tile_manager_count;

    for (uint8_t i = 0; i < meta_tile_manager_count; ++i) {
      delete meta_tile_managers_[i];
    }
    delete[] meta_tile_managers_;

    uint64_t tiles_per_dimension =
        config_->context_.tile_store_config.count_tiles_per_dimension;
    for (uint64_t i = 0; i < tiles_per_dimension; ++i) {
      delete[] tile_manager_mapping_[i];
    }
    delete[] tile_manager_mapping_;
  }

  void join() {
    uint8_t meta_tile_manager_count = config_->context_.meta_tile_manager_count;

    // Send shutdown message to MetaTileManagers, join them afterwards.
    for (uint8_t i = 0; i < meta_tile_manager_count; ++i) {
      MetaTileManager<VertexType, Algorithm, is_weighted>* meta_tm = meta_tile_managers_[i];
      Ringbuffer rb = meta_tm->control_rb();

      flatbuffers::FlatBufferBuilder builder;

      auto message = CreateEdgeInserterControlMessage(builder, 0, 0, 0, true);
      builder.Finish(message);
      util::sendMessage(rb, builder);

      sg_dbg("Send shutdown message to MetaTileManager %u.\n", i);
    }

    for (uint8_t i = 0; i < meta_tile_manager_count; ++i) {
      meta_tile_managers_[i]->join();
    }
    sg_dbg("All %u MetaTileManagers were joined.\n", meta_tile_manager_count);
  }

private:
  std::shared_ptr<Config> config_;

  std::shared_ptr<TileStore<VertexType, Algorithm, is_weighted> > tile_store_;

  MetaTileManager<VertexType, Algorithm, is_weighted>** meta_tile_managers_;

  uint8_t** tile_manager_mapping_;
};

// template<typename VertexType, class Algorithm, bool is_weighted>
// MetaTileManager<VertexType, Algorithm, is_weighted>
//     ** MetaTileStore<VertexType, Algorithm, is_weighted>::meta_tile_managers_ = nullptr;

// template<typename VertexType, class Algorithm, bool is_weighted>
// uint8_t** MetaTileStore<VertexType, Algorithm, is_weighted>::tile_manager_mapping_ = nullptr;
// ALGORITHM_TEMPLATE_WEIGHTED_H(MetaTileStore)

}

#endif //EVOLVING_GRAPHS_META_TILE_STORE_H
