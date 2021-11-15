// SPDX-License-Identifier: MIT

#ifndef EVOLVING_GRAPHS_TILE_DISTRIBUTOR_H
#define EVOLVING_GRAPHS_TILE_DISTRIBUTOR_H

#include <memory>

#include <flatbuffers/flatbuffers.h>

#include "util/runnable.h"
#include "ring-buffer/ring-buffer.h"
#include "traversal/traversal.h"
#include "tile-manager/tile-manager.h"
#include "core/tile-store.h"
#include "util/config.h"
#include "schema_generated.h"
#include "util/util.h"
#include "perf-event/perf-event-scoped.h"
#include "perf-event/perf-event-manager.h"

namespace evolving_graphs {
template<typename VertexType, class Algorithm, bool is_weighted>
class MetaTileStore;

template<typename VertexType, class Algorithm, bool is_weighted>
class TileDistributor : public util::Runnable {
public:
  TileDistributor(const Ringbuffer& rb, Traversal* traversal, pthread_barrier_t* barrier,
                  bool* shutdown_flag, std::shared_ptr<Config> config,
                  std::shared_ptr<TileStore<VertexType, Algorithm, is_weighted> > tile_store,
                  std::shared_ptr<MetaTileStore<VertexType, Algorithm, is_weighted> > meta_tile_store,
                  std::shared_ptr<PerfEventManager> perf_event_manager) : rb_(rb),
      traversal_(traversal),
      barrier_(barrier),
      shutdown_flag_(shutdown_flag),
      config_(std::move(config)),
      tile_store_(std::move(tile_store)),
      meta_tile_store_(std::move(meta_tile_store)),
      perf_event_manager_(std::move(perf_event_manager)),
      count_skipped_tiles_(0), count_skipped_meta_tiles_(0), count_edges_(0) {}

  inline uint64_t getCountSkippedTiles() { return count_skipped_tiles_; }

  inline uint64_t getCountSkippedMetaTiles() { return count_skipped_meta_tiles_; }

  inline void resetStatistics() {
    count_skipped_tiles_ = 0;
    count_skipped_meta_tiles_ = 0;
  }

  inline void updateCountEdges(uint64_t count_edges) { count_edges_ = count_edges; }

protected:
  void run() override {
    uint8_t count_meta_tile_managers = config_->context_.meta_tile_manager_count;
    while (true) {
      pthread_barrier_wait(barrier_);
      PerfEventScoped scoped(perf_event_manager_->getRingBuffer(),
                             "tile-distributor",
                             "TileDistributor",
                             0,
                             config_->context_.enable_perf_event_collection);
      // Return if the shutdown flag has been set by the MetaTileStore.
      if (*shutdown_flag_) {
        return;
      }

      if (config_->context_.enable_two_level_tile_distributor) {
        // Iterate meta tiles first, then iterate tiles inside the meta tile
        auto tiles_per_meta_tile = config_->context_.tile_store_config.td_count_tiles_per_meta_tiles;
        for (uint64_t meta_tile_id = 0;
             meta_tile_id < config_->context_.tile_store_config.td_count_meta_tiles; ++meta_tile_id) {
          // Check if meta tile contains any edges at all, go to every MetaTileManager and check:
          uint64_t edges_in_meta_tile = 0;
          for (uint8_t meta_tm_id = 0; meta_tm_id < count_meta_tile_managers; ++meta_tm_id) {
            edges_in_meta_tile += meta_tile_store_->getMetaTileManager(meta_tm_id)->edgesPerMetaTile(meta_tile_id);
          }
          if (edges_in_meta_tile == 0) {
            // Skip this meta tile.
            count_skipped_tiles_ += tiles_per_meta_tile;
            count_skipped_meta_tiles_ += 1;
            sg_dbg("Skipping meta tile %lu\n", meta_tile_id);
            continue;
          }
          sg_dbg("Meta tile %lu active\n", meta_tile_id);
          uint64_t start_tile = meta_tile_id * tiles_per_meta_tile;
          uint64_t end_tile = start_tile + tiles_per_meta_tile;
          // Else execute algorithm on the tiles in this batch.
          sg_dbg("From %lu to %lu\n", start_tile, end_tile);
          scheduleTiles(tile_store_->getListOfTilesPerMetaTile(
              meta_tile_id));
          //        scheduleTiles(start_tile, end_tile);
        }
      } else {
        // Traditional mode, iterate all tiles at once, push assignments to ringbuffer and exit.
        uint64_t count_tiles = config_->context_.tile_store_config.count_tiles;
        scheduleTiles(0, count_tiles);
        //    delete scoped;
      }

      // Send a shutdown message to all executors to end the current iteration.
      uint64_t count_executors = config_->context_.algorithm_executor_count;
      for (int i = 0; i < count_executors; ++i) {
        flatbuffers::FlatBufferBuilder builder;
        auto message = CreateTileDistributorMessage(builder, true);
        builder.Finish(message);
        util::sendMessage(rb_, builder);
      }
    }
  }

private:
  void scheduleTiles(uint64_t start_tile_id, uint64_t end_tile_id) {
    uint64_t current_tile = start_tile_id;

    std::vector<TileManager<VertexType, Algorithm, is_weighted>*> tms;

    while (current_tile < end_tile_id) {
      auto tm = tile_store_->getTileManagerByTraversal(current_tile);
      tms.push_back(tm);
      ++current_tile;
    }
    scheduleTiles(&tms);
  }

  void scheduleTiles(const std::vector<TileManager<VertexType, Algorithm, is_weighted>*>* tiles) {
    uint64_t tile_batching = config_->context_.algorithm_tile_batching;
    uint64_t count_executors = config_->context_.algorithm_executor_count;

    uint64_t tiles_current_batch = 0;
    uint64_t edges_current_batch = 0;

    std::vector<uint64_t> tile_batch;
    tile_batch.reserve(tile_batching);

    // At most half of the total number of edges per executor should be used up
    // at once.
    uint64_t max_edges_per_batch = (count_edges_ / count_executors) / 128;

    for (auto it = tiles->begin(); it != tiles->end(); ++it) {
      TileManager<VertexType, Algorithm, is_weighted>* tm = *it;
      size_t current_edges = tm->countEdges();

      // Only continue with this tile if there are edges inside.
      if (current_edges != 0) {
        // Push onto vector and add related counts.
        tile_batch.emplace_back(tm->traversal_id());
        edges_current_batch += current_edges;
        ++tiles_current_batch;
      } else {
        ++count_skipped_tiles_;
      }

      // Either the threshold of tiles or edges has been reached or all tiles
      // have been processed, send out the message.
      if (tiles_current_batch >= tile_batching ||
          edges_current_batch > max_edges_per_batch ||
          *it == tiles->back()) {
        if (tile_batch.size() > 0) {
          flatbuffers::FlatBufferBuilder builder;

          uint8_t* buffer_pointer;
          auto tiles_to_send =
              builder.CreateUninitializedVector(tile_batch.size(), sizeof(uint64_t), &buffer_pointer);
          memcpy(buffer_pointer, tile_batch.data(), tile_batch.size() * sizeof(uint64_t));

          auto message = CreateTileDistributorMessage(builder, false, tiles_to_send);
          builder.Finish(message);
          util::sendMessage(rb_, builder);

          tile_batch.clear();
          tiles_current_batch = 0;
          edges_current_batch = 0;
        }
      }
    }
  }

  Traversal* traversal_;

  Ringbuffer rb_;

  pthread_barrier_t* barrier_;

  bool* shutdown_flag_;

  uint64_t count_skipped_tiles_;

  uint64_t count_skipped_meta_tiles_;

  uint64_t count_edges_;

  std::shared_ptr<Config> config_;

  std::shared_ptr<TileStore<VertexType, Algorithm, is_weighted> > tile_store_;

  std::shared_ptr<MetaTileStore<VertexType, Algorithm, is_weighted> > meta_tile_store_;

  std::shared_ptr<PerfEventManager> perf_event_manager_;
};

// ALGORITHM_TEMPLATE_WEIGHTED_H(TileDistributor)

}


#endif //EVOLVING_GRAPHS_TILE_DISTRIBUTOR_H
