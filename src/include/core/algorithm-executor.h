// SPDX-License-Identifier: MIT

#ifndef EVOLVING_GRAPHS_ALGORITHM_EXECUTOR_H
#define EVOLVING_GRAPHS_ALGORITHM_EXECUTOR_H

#include <cassert>
#include <memory>

#include "util/runnable.h"
#include "util/datatypes.h"
#include "util/config.h"
#include "core/tile-store.h"
#include "core/vertex-store.h"
#include "traversal/hilbert.h"
#include "traversal/traversal.h"
#include "perf-event/perf-event-scoped.h"
#include "perf-event/perf-event-manager.h"
#include "ring-buffer/ring-buffer.h"
#include "tile-manager/tile-manager.h"

namespace evolving_graphs {

using namespace perf_event;

template<typename VertexType, class Algorithm, bool is_weighted>
class AlgorithmExecutor : public util::Runnable {
public:
  AlgorithmExecutor(const Ringbuffer& rb,
                     uint32_t id,
                     Traversal* traversal,
                     pthread_barrier_t* barrier,
                     pthread_barrier_t* pull_barrier,
                     bool* shutdown_flag, 
                     std::shared_ptr<Config> config,
                     std::shared_ptr<VertexStore<VertexType> > vertex_store,
                     std::shared_ptr<TileStore<VertexType, Algorithm, is_weighted> > tile_store,
                     std::shared_ptr<PerfEventManager> perf_event_manager,
                     std::shared_ptr<Algorithm> algorithm,
                     std::shared_ptr<uint64_t> current_tile)
      : id_(id),
        count_edges_(0),
        count_retries_(0),
        count_inactive_tiles_(0),
        count_active_tiles_(0),
        count_skipped_tiles_(0),
        next_start_(0),
        next_end_(0),
        current_index_(0),
        rb_(rb),
        traversal_(traversal),
        barrier_(barrier),
        pull_barrier_(pull_barrier),
        shutdown_flag_(shutdown_flag),
        vertices_per_tile_(config_->context_.tile_store_config.vertices_per_tile),
        enable_adaptive_scheduling_(config_->context_.enable_adaptive_scheduling),
        current_tile_(std::move(current_tile)),
        config_(std::move(config)),
        vertex_store_(std::move(vertex_store)),
        tile_store_(std::move(tile_store)),
        perf_event_manager_(std::move(perf_event_manager)),
        algorithm_(std::move(algorithm)) {
    uint64_t count_tile_batching = config_->context_.algorithm_tile_batching;

    // Allocate all tile-infos.
    tile_infos_ = new tile_to_local_array_t<VertexType>[count_tile_batching];
    memset(tile_infos_, 0, count_tile_batching * sizeof(tile_infos_[0]));

    local_next_array_.reserve(count_tile_batching);
    local_critical_neighbors_.reserve(count_tile_batching);

    for (int i = 0; i < count_tile_batching; ++i) {
      local_next_array_.push_back(new VertexType[vertices_per_tile_]);
      local_critical_neighbors_.push_back(new uint32_t[vertices_per_tile_]);
    }
  }

  ~AlgorithmExecutor() override{
    for (int i = 0; i < config_->context_.algorithm_tile_batching; ++i) {
      delete[] local_next_array_[i];
      delete[] local_critical_neighbors_[i];
    }
  }

  inline uint64_t countEdges() const{ return count_edges_; }

  inline uint64_t countRetries() const{ return count_retries_; }

  inline uint64_t countInactiveTiles() const{ return count_inactive_tiles_; }

  inline uint64_t countActiveTiles() const{ return count_active_tiles_; }

  inline uint64_t countSkippedTiles() const{ return count_skipped_tiles_; }

  void resetStatistics(){
    count_retries_ = 0;
    count_edges_ = 0;
    count_inactive_tiles_ = 0;
    count_active_tiles_ = 0;
    count_skipped_tiles_ = 0;
    // If leading thread, also reset the current-tile id.
    if (id_ == 0) {
      *current_tile_ = 0;
    }
    current_index_ = 0;
  }

  inline uint32_t id(){ return id_; }

protected:
  void run() override {
    while (true) {
      pthread_barrier_wait(barrier_);
      // Return if the shutdown flag has been set by the MetaTileStore.
      if (*shutdown_flag_) {
        return;
      }

      uint64_t count_tiles = 0;
      while (true) {
        // Reset all tile-infos.
        resetTileInfos();

        ring_buffer_req_t req{};
        uint64_t tile_index;
        size_t count_received_tiles;
        bool finished = receiveTilesToProcess(&req, &tile_index, &count_received_tiles);
        if (finished) {
          break;
        }

        uint64_t count_current_edges = 0;

        processTiles(tile_index, &req, &count_current_edges, &count_tiles);

        applyTiles(tile_index, count_received_tiles, count_current_edges);
      }
      pthread_barrier_wait(pull_barrier_);
    }
  }

private:
  std::shared_ptr<uint64_t> current_tile_;

  std::shared_ptr<Config> config_;

  std::shared_ptr<VertexStore<VertexType> > vertex_store_;

  std::shared_ptr<TileStore<VertexType, Algorithm, is_weighted> > tile_store_;

  std::shared_ptr<PerfEventManager> perf_event_manager_;

  std::shared_ptr<Algorithm> algorithm_;

  static inline vertex_id_t getGlobalTarget(uint64_t y,
                                            const local_vertex_id_t local_target,
                                            const uint64_t vertices_per_tile) {
    return (y * vertices_per_tile) | local_target;
  }

  tile_to_local_array_t<VertexType> getLocalNextArray(int64_t y){
    VertexType* local_next = nullptr;
    uint32_t* local_critical_neighbors = nullptr;

    bool matching_array_found = false;
    // Find appropriate vertex array.
    for (uint64_t i = 0; i < config_->context_.algorithm_tile_batching; ++i) {
      auto tile_info = &tile_infos_[i];
      if (tile_info->tile_y == y) {
        local_next = tile_info->array;
        local_critical_neighbors = tile_info->critical_neighbor;
        matching_array_found = true;
        break;
      }
    }
    // If array not found, create it at the end of all allocated arrays.
    if (!matching_array_found) {
      uint32_t count = 0;
      for (uint64_t i = 0; i < config_->context_.algorithm_tile_batching; ++i) {
        auto tile_info = &tile_infos_[i];
        if (tile_info->tile_y == UINT64_MAX) {
          tile_info->tile_y = static_cast<uint64_t>(y);
          tile_info->array = local_next_array_[count];
          tile_info->critical_neighbor = local_critical_neighbors_[count];
          local_next = tile_info->array;
          local_critical_neighbors = tile_info->critical_neighbor;
          break;
        }
        ++count;
      }
      algorithm_->init_vertices_local(local_next, vertices_per_tile_);
      memset(local_critical_neighbors, UINT8_MAX, vertices_per_tile_ * sizeof(uint32_t));
    }

    assert(local_next != nullptr);

    tile_to_local_array_t<VertexType> local_information{};
    local_information.critical_neighbor = local_critical_neighbors;
    local_information.array = local_next;
    return local_information;
  }

  void resetTileInfos() {
    for (uint64_t i = 0; i < config_->context_.algorithm_tile_batching; ++i) {
      auto tile_info = &tile_infos_[i];
      // Early exit if all successive tile infos have already been reset.
      if (tile_info->tile_y == UINT64_MAX) {
        break;
      }
      tile_info->array = nullptr;
      tile_info->tile_y = UINT64_MAX;
    }
  }

  uint32_t executeAlgoOnTile(TileManager<VertexType, Algorithm, is_weighted>* tm,
                             tile_to_local_array_t<VertexType> local_next) {
    // Iterate edges in CSR first.
    uint32_t current_count = 0;
    uint32_t count_edges = 0;
    local_vertex_id_t* counts = tm->getCounts();
    std::vector<local_vertex_id_t>* targets = tm->getTargets();
    std::vector<vertex_count_t>* id_counts = tm->getIdCounts();
    std::vector<local_edge_t>* edges = tm->getEdges();
    std::vector<bool>* deleted_edges = tm->getDeletedEdgesBitset();

    std::vector<float>* edge_weights = tm->getEdgeWeights();
    std::vector<float>* csr_weights = tm->getCSRWeights();

    if (tm->isCompactStorage()) {
      for (uint32_t i = 0; i < vertices_per_tile_; ++i) {
        uint32_t max_count = current_count + counts[i];
        for (; current_count < max_count; ++current_count) {
          local_edge_t local_edge{};
          local_edge.src = static_cast<local_vertex_id_t>(i);
          local_edge.tgt = (*targets)[current_count];

          if (is_weighted) {
            if (executeAlgorithmSingleEdgeWeighted(tm,
                                                   local_next,
                                                   local_edge,
                                                   (*csr_weights)[current_count])) {
              ++count_edges;
            }
          } else {
            if (executeAlgorithmSingleEdge(tm, local_next, local_edge)) {
              ++count_edges;
            }
          }
        }
      }
    } else {
      for (const auto& source : (*id_counts)) {
        uint32_t max_count = current_count + source.count;
        for (; current_count < max_count; ++current_count) {
          local_edge_t local_edge{};
          local_edge.src = source.id;
          local_edge.tgt = (*targets)[current_count];

          if (is_weighted) {
            if (executeAlgorithmSingleEdgeWeighted(tm,
                                                   local_next,
                                                   local_edge,
                                                   (*csr_weights)[current_count])) {
              ++count_edges;
            }
          } else {
            if (executeAlgorithmSingleEdge(tm, local_next, local_edge)) {
              ++count_edges;
            }
          }
        }
      }
    }

    // Then iterate edges in overflow list.
    for (size_t i = 0; i < edges->size(); ++i) {
      if (!(*deleted_edges)[i]) {
        if (is_weighted) {
          if (executeAlgorithmSingleEdgeWeighted(tm, local_next, (*edges)[i], (*edge_weights)[i])) {
            ++count_edges;
          }
        } else {
          if (executeAlgorithmSingleEdge(tm, local_next, (*edges)[i])) {
            ++count_edges;
          }
        }
      }
    }

    return count_edges;
  }

  // Return value indicates if edge was actually processed or not.
  bool executeAlgorithmSingleEdge(TileManager<VertexType, Algorithm, is_weighted>* tm,
                                         tile_to_local_array_t<VertexType> local_next,
                                         const local_edge_t& local_edge) {
    edge_t global_edge = tm->getGlobalEdge(local_edge);

    // Distinguish between active/non-active mode.
    if (enable_adaptive_scheduling_) {
      // Only execute algorithm if the algorithm says the edge is active.
      if (algorithm_->isEdgeActive(global_edge)) {
        //      sg_log("Edge active: %lu -> %lu\n", global_edge.src, global_edge.tgt);
        if (Algorithm::ASYNC) {
          bool updated =
              algorithm_->pullGather(vertex_store_->vertices_.next[global_edge.src],
                                    local_next.array[local_edge.tgt],
                                    global_edge.src,
                                    global_edge.tgt,
                                    vertex_store_->vertices_.degrees[global_edge.src],
                                    vertex_store_->vertices_.degrees[global_edge.tgt]);
          if (updated) {
            local_next.critical_neighbor[local_edge.tgt] = global_edge.src;
          }
        } else {
          bool updated =
              algorithm_->pullGather(vertex_store_->vertices_.current[global_edge.src],
                                    local_next.array[local_edge.tgt],
                                    global_edge.src,
                                    global_edge.tgt,
                                    vertex_store_->vertices_.degrees[global_edge.src],
                                    vertex_store_->vertices_.degrees[global_edge.tgt]);
          if (updated) {
            local_next.critical_neighbor[local_edge.tgt] = global_edge.src;
          }
        }
        return true;
      }
      //    sg_log("Edge inactive: %lu -> %lu\n", global_edge.src, global_edge.tgt);
      return false;
    } else {
      // In the non-active mode, always execute the algorithm.
      if (Algorithm::ASYNC) {
        bool updated =
            algorithm_->pullGather(vertex_store_->vertices_.next[global_edge.src],
                                  local_next.array[local_edge.tgt],
                                  global_edge.src,
                                  global_edge.tgt,
                                  vertex_store_->vertices_.degrees[global_edge.src],
                                  vertex_store_->vertices_.degrees[global_edge.tgt]);
        if (updated) {
          local_next.critical_neighbor[local_edge.tgt] = global_edge.src;
        }
      } else {
        bool updated =
            algorithm_->pullGather(vertex_store_->vertices_.current[global_edge.src],
                                  local_next.array[local_edge.tgt],
                                  global_edge.src,
                                  global_edge.tgt,
                                  vertex_store_->vertices_.degrees[global_edge.src],
                                  vertex_store_->vertices_.degrees[global_edge.tgt]);
        if (updated) {
          local_next.critical_neighbor[local_edge.tgt] = global_edge.src;
        }
      }
      return true;
    }
  }

  // Return value indicates if edge was actually processed or not.
  bool executeAlgorithmSingleEdgeWeighted(
      TileManager<VertexType, Algorithm, is_weighted>* tm,
      tile_to_local_array_t<VertexType> local_next, const local_edge_t& local_edge,
      const float weight) {
    edge_t global_edge = tm->getGlobalEdge(local_edge);

    // Distinguish between active/non-active mode.
    if (enable_adaptive_scheduling_) {
      // Only execute algorithm if the algorithm says the edge is active.
      if (algorithm_->isEdgeActive(global_edge)) {
        bool updated =
            algorithm_->pullGatherWeighted(vertex_store_->vertices_.current[global_edge.src],
                                          local_next.array[local_edge.tgt],
                                          weight,
                                          global_edge.src,
                                          global_edge.tgt,
                                          vertex_store_->vertices_.degrees[global_edge.src],
                                          vertex_store_->vertices_.degrees[global_edge.tgt]);
        if (updated) {
          local_next.critical_neighbor[local_edge.tgt] = global_edge.src;
        }
        return true;
      }
      return false;
    } else {
      // In the non-active mode, always execute the algorithm.
      bool updated =
          algorithm_->pullGatherWeighted(vertex_store_->vertices_.current[global_edge.src],
                                        local_next.array[local_edge.tgt],
                                        weight,
                                        global_edge.src,
                                        global_edge.tgt,
                                        vertex_store_->vertices_.degrees[global_edge.src],
                                        vertex_store_->vertices_.degrees[global_edge.tgt]);
      if (updated) {
        local_next.critical_neighbor[local_edge.tgt] = global_edge.src;
      }
      return true;
    }
  }

  void executeTile(uint64_t tile_id, uint64_t* count_current_edges) {
    TileManager<VertexType, Algorithm, is_weighted>
        * tm = tile_store_->getTileManagerByTraversal(tile_id);
    // Check if the current tile has any active edges in it, iff enabled, skip if no active
    // edges in the tile.
    if (enable_adaptive_scheduling_) {
      if (!tm->isActive()) {
        ++count_inactive_tiles_;
        return;
      }
    }

    // Skip empty tiles.
    if (tm->countEdges() == 0) {
      ++count_skipped_tiles_;
      return;
    }

    ++count_active_tiles_;

    // Find array to write into.
    tile_to_local_array_t<VertexType> local_next = getLocalNextArray(tm->y());

    size_t count_edges = tm->countEdges();
    count_edges_ += count_edges;
    *count_current_edges += count_edges;

    executeAlgoOnTile(tm, local_next);
  }

  void applyTiles(uint64_t tile_index, size_t count_received_tiles, uint64_t count_current_edges) {
    PerfEventScoped event(perf_event_manager_->getRingBuffer(),
                          "apply-tiles-" + std::to_string(tile_index),
                          "AlgorithmExecutor",
                          id_,
                          config_->context_.enable_perf_event_collection,
                          std::to_string(count_current_edges) + ", " +
                          std::to_string(count_received_tiles));

    // Apply result to global vertex array.
    // Go through all tile-infos that are in use and push their results to the
    // global array.
    for (uint64_t i = 0; i < config_->context_.algorithm_tile_batching; ++i) {
      auto tile_info = &tile_infos_[i];
      // If we found a tile-info with UINT64_MAX as its y-id, we are done.
      if (tile_info->tile_y == UINT64_MAX) {
        break;
      }

      const VertexType neutral_element = Algorithm::neutral_element;

      for (uint32_t j = 0; j < vertices_per_tile_; ++j) {
        // Only continue if a result has been computed for this vertex.
        if (likely(tile_info->array[j] == neutral_element)) {
          continue;
        }

        uint64_t global_target =
            getGlobalTarget(tile_info->tile_y,
                            static_cast<const local_vertex_id_t>(j),
                            vertices_per_tile_);

        VertexType* ptr = &vertex_store_->vertices_.next[global_target];
        VertexType old_value;
        VertexType new_value;

        bool successful;
        bool changed = false;

        do {
          old_value = vertex_store_->vertices_.next[global_target];

          bool updated =
              algorithm_->reduceVertex(new_value,
                                      tile_info->array[j],
                                      *ptr,
                                      global_target,
                                      vertex_store_->vertices_.degrees[global_target]);

          if (updated) {
            vertex_store_->vertices_.critical_neighbor[global_target] =
                tile_info->critical_neighbor[j];
          }

          if (old_value != new_value) {
            successful = algorithm_->compareAndSwap(ptr, old_value, new_value);
            changed = true;
          } else {
            successful = true;
          }
          if (!successful) {
            ++count_retries_;
          }
        } while (!successful);

        if (Algorithm::USE_CHANGED) {
          if (changed) {
            set_bool_array_atomically(vertex_store_->vertices_.changed,
                                      global_target,
                                      true);
          }
        }
      }
    }
  }

  void processTiles(uint64_t tile_index, ring_buffer_req_t* req, uint64_t* count_current_edges,
                    uint64_t* count_tiles) {
    PerfEventScoped event(perf_event_manager_->getRingBuffer(),
                          "process-tiles-" + std::to_string(tile_index),
                          "AlgorithmExecutor",
                          id_,
                          config_->context_.enable_perf_event_collection);

    if (config_->context_.tile_distribution == tile_distribution_t::TILE_DISTRIBUTOR) {
      // Iterate all tiles associated with the current executor.
      for (const auto& tile_id : *message_->tiles()) {
        ++(*count_tiles);
        executeTile(tile_id, count_current_edges);
      }
      rb_.set_done(req);
    } else if (config_->context_.tile_distribution == tile_distribution_t::ATOMICS ||
               config_->context_.tile_distribution == tile_distribution_t::STATIC) {
      // Iterate all tiles associated with the current executor.
      for (uint64_t tile_id = next_start_; tile_id < next_end_; ++tile_id) {
        ++(*count_tiles);
        executeTile(tile_id, count_current_edges);
      }
    }
  }

  bool receiveTilesToProcess(ring_buffer_req_t* req, uint64_t* tile_index,
                             size_t* count_received_tiles) {
    if (config_->context_.tile_distribution == tile_distribution_t::TILE_DISTRIBUTOR) {
      ring_buffer_get_req_init(req, BLOCKING);
      rb_.get(req);
      message_ = flatbuffers::GetRoot<TileDistributorMessage>(req->data);

      if (message_->shutdown()) {
        rb_.set_done(req);
        return true;
      }

      *count_received_tiles = message_->tiles()->size();
      if (*count_received_tiles > 0) {
        *tile_index = message_->tiles()->Get(0);
      } else {
        *tile_index = UINT64_MAX;
      }
      return false;
    } else if (config_->context_.tile_distribution == tile_distribution_t::ATOMICS) {
      next_start_ = smp_faa(current_tile_.get(), config_->context_.algorithm_tile_batching);
      next_end_ = std::min(next_start_ + config_->context_.algorithm_tile_batching,
                           config_->context_.tile_store_config.count_tiles);
      *tile_index = next_start_;
      *count_received_tiles = config_->context_.algorithm_tile_batching;
      return next_start_ >= config_->context_.tile_store_config.count_tiles;
    } else if (config_->context_.tile_distribution == tile_distribution_t::STATIC) {
      size_t tiles_per_executor =
          config_->context_.tile_store_config.count_tiles / config_->context_.algorithm_executor_count;

      next_start_ = (id_ * tiles_per_executor) + current_index_;
      current_index_ += config_->context_.algorithm_tile_batching;
      next_end_ = std::min(next_start_ + config_->context_.algorithm_tile_batching,
                           (id_ + 1) * tiles_per_executor);
      *tile_index = next_start_;
      *count_received_tiles = config_->context_.algorithm_tile_batching;
      return next_start_ >= (id_ + 1) * tiles_per_executor;
    }

    // TODO: output error message
    return false; 
  }

  const uint64_t vertices_per_tile_;

  const bool enable_adaptive_scheduling_;

  tile_to_local_array_t<VertexType>* tile_infos_;

  std::vector<VertexType*> local_next_array_;

  std::vector<uint32_t*> local_critical_neighbors_;

  uint32_t id_;

  uint64_t count_edges_;

  uint64_t count_retries_;

  uint64_t count_inactive_tiles_;

  uint64_t count_active_tiles_;

  uint64_t count_skipped_tiles_;

  uint64_t next_start_;

  uint64_t next_end_;

  uint64_t current_index_;

  Ringbuffer rb_;

  Traversal* traversal_;

  pthread_barrier_t* barrier_;

  pthread_barrier_t* pull_barrier_;

  bool* shutdown_flag_;

  const TileDistributorMessage* message_;
};

// template<typename VertexType, class Algorithm, bool is_weighted>
// uint64_t AlgorithmExecutor<VertexType, Algorithm, is_weighted>::current_tile = 0;
// ALGORITHM_TEMPLATE_WEIGHTED_H(AlgorithmExecutor)
}


#endif //EVOLVING_GRAPHS_ALGORITHM_EXECUTOR_H
