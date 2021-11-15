// SPDX-License-Identifier: MIT

#ifndef EVOLVING_GRAPHS_TILE_MANAGER_H
#define EVOLVING_GRAPHS_TILE_MANAGER_H

#include <cstdint>
#include <random>
#include <algorithm>
#include <cassert>
#include <memory>

#include <gtest/gtest_prod.h>

#include "util/datatypes.h"
#include "util/config.h"
#include "util/util.h"
#include "core/vertex-store.h"
#include "perf-event/perf-event-scoped.h"
#include "perf-event/perf-event-manager.h"
#include "schema_generated.h"


namespace evolving_graphs {

using namespace perf_event;

template<typename VertexType, class Algorithm, bool is_weighted>
class TileManager {
public:
  class EdgeIterator {
  public:
    typedef local_edge_t value_type;

    explicit EdgeIterator(size_t count_edges, size_t current_index, TileManager* tile_manager) 
          : count_edges_(count_edges),
          current_index_(current_index),
          tile_manager_(tile_manager),
          current_index_position_(0),
          current_index_count_(0),
          current_targets_offset_(0) {}

    EdgeIterator& operator++() {
      ++current_index_;

      size_t targets_size = tile_manager_->targets_.size();
      if (current_index_ < targets_size) {
        bool resetIndex;
        do {
          resetIndex = false;
          if (tile_manager_->usesCompactVertexStorage()) {
            // If all edges of the current source have been used up, move on to the next one.
            if (current_index_count_ >=
                tile_manager_->counts_[current_index_position_]) {
              resetIndex = true;
            }
          } else {
            if (current_index_count_ >=
                tile_manager_->id_counts_[current_index_position_].count) {
              resetIndex = true;
            }
          }
          if (resetIndex) {
            current_index_count_ = 0;
            ++current_index_position_;
          }
        } while (resetIndex);
        ++current_targets_offset_;
      }

      return *this;
    }

    value_type operator*() {
      // Edge is either in the CSR or the overflow list, locate accordingly.
      size_t targets_size = tile_manager_->targets_.size();
      if (current_index_ < targets_size) {
        // Return correct edge now.
        if (tile_manager_->usesCompactVertexStorage()) {
          return {static_cast<local_vertex_id_t>(current_index_count_),
                  tile_manager_->targets_[current_targets_offset_]};
        } else {
          return {tile_manager_->id_counts_[current_index_count_].id,
                  tile_manager_->targets_[current_targets_offset_]};
        }
      } else {
        // Edge is in the edge list, access by index and return.
        return tile_manager_->edges_[current_index_ - targets_size];
      }
    }

    inline bool operator==(const EdgeIterator& rhs) const {
      return current_index_ == rhs.current_index_;
    }

    bool operator!=(const EdgeIterator& rhs) const {
      return current_index_ != rhs.current_index_;
    }

  private:
    size_t count_edges_;

    size_t current_index_;

    size_t current_index_position_;

    size_t current_index_count_;

    size_t current_targets_offset_;

    TileManager* tile_manager_;
  };

  TileManager(uint64_t x, uint64_t y, uint64_t traversal_id, std::shared_ptr<Config> config, std::shared_ptr<VertexStore<VertexType> > vertex_store): x_(x),
        y_(y),
        traversal_id_(traversal_id),
        compact_storage_(false),
        next_compaction_(PAGE_SIZE),
        count_compactions_(0), counts_(nullptr),
        gen_(rd_()),
        meta_tile_id_(0),
        vertices_per_tile_(config->context_.tile_store_config.vertices_per_tile),
        config_(std::move(config)), 
	vertex_store_(std::move(vertex_store)){}

  inline uint64_t getId() {
    return config_->context_.tile_store_config.count_tiles_per_dimension * x_ + y_;
  }

  const inline Coordinates getTwoLevelCoordinates() {
    uint64_t
        tiles_per_meta_tiles =
        config_->context_.tile_store_config.td_count_tiles_per_meta_tiles_per_dimension;
    return Coordinates{x_ / tiles_per_meta_tiles, y_ / tiles_per_meta_tiles};
  }

  void insertEdge(const Edge& edge) {
    edge_t converted_edge{edge.src(), edge.tgt()};

    local_edge_t local_edge = getLocalEdge(converted_edge);
    edges_.push_back(local_edge);
    deleted_edges_bitset_.push_back(false);

    if (is_weighted) {
      edges_weights_.push_back(edge.weight());
    }

    edge_t global_edge = getGlobalEdge(local_edge);
    sg_assert(converted_edge == global_edge, "Edges do not match.");

    if (config_->context_.use_dynamic_compaction) {
      // Compact storage if next compaction threshold has been reached.
      if (countEdges() >= next_compaction_) {
        //      PerfEventScoped event(PerfEventManager::getInstance()->getRingBuffer(),
        //                            "dynamicCompaction-" + std::to_string(countEdges()),
        //                            "TileManager",
        //                            getId(),
        //                            config_->context_.enable_perf_event_collection);

        next_compaction_ = next_compaction_ * 2;
        compactStorage();
      }
    }
  }

  inline void deleteEdge(const edge_t& edge) {
    local_edge_t local_edge = getLocalEdge(edge);
    deleted_edges_.insert(local_edge);
  }

  void deleteEdge(const Edge& edge) {
    edge_t local_edge{edge.src(), edge.tgt()};
    deleteEdge(local_edge);
  }

  bool deleteRandomEdge(edge_t* global_edge) {
    // Select edge index.
    if (countEdges() == 0) {
      return false;
    }
    std::uniform_int_distribution<> dis(0, static_cast<int>(countEdges() - 1));
    int index_edge_to_delete = dis(gen_);

    local_edge_t local_edge = edges_[index_edge_to_delete];
    // Mark edge as deleted.

    deleted_edges_bitset_[index_edge_to_delete] = true;

    *global_edge = getGlobalEdge(local_edge);
    return true;
  }

  void compactStorage() {
    ++count_compactions_;

    size_t count_edges = countEdges();
    if (count_edges == 0) {
      return;
    }

    // Copy over edges from CSR to edge list to append to CSR.
    edges_.reserve(count_edges);
    if (is_weighted) {
      edges_weights_.reserve(count_edges);
    }

    size_t count_edges_before = edges_.size();
    size_t count_targets = targets_.size();

    uint32_t count_copied_edges = copyCSRToEdgeList();


    if (count_targets != count_copied_edges) {
      // Count target.
      size_t count_target_degrees = 0;
      for (const auto& source : id_counts_) {
        count_target_degrees += source.count;
      }
      printf("%lu, %lu, %u, %lu\n",
             count_edges_before,
             count_targets,
             count_copied_edges,
             count_target_degrees
      );
      util::die(1);
    }

    targets_.clear();
    targets_.reserve(count_edges);

    size_t new_count_edges = edges_.size();
    assert(new_count_edges == count_edges);

    // Simply sort the edges in the un-weighted case, otherwise keep relationship between edges and
    // their weights.
    if (!is_weighted) {
      std::sort(edges_.begin(), edges_.end());
    } else {
      // Get the sort permutation, then apply it to both the edges as well as the weights.
      auto p = util::sort_permutation(edges_,
                                      [](local_edge_t const& a,
                                         local_edge_t const& b) { return a < b; });

      util::apply_permutation_in_place(edges_, p);
      util::apply_permutation_in_place(edges_weights_, p);
    }

    local_vertex_id_t count_sources = 1;
    local_vertex_id_t current_src = edges_.front().src;
    for (const auto& edge : edges_) {
      // Skip deleted edges:
      if (deleted_edges_.find(edge) != deleted_edges_.end()) {
        continue;
      }

      if (current_src != edge.src) {
        current_src = edge.src;
        ++count_sources;
      }
    }

    // Use compact storage if more sources than half the max, then the wasted
    // space will be less.
    compact_storage_ = (count_sources >= vertices_per_tile_ / 2);

    if (compact_storage_) {
      if (counts_ == nullptr) {
        counts_ = new local_vertex_id_t[vertices_per_tile_];
      }
      // Reset counts.
      for (uint32_t i = 0; i < vertices_per_tile_; ++i) {
        counts_[i] = 0;
      }
    } else {
      // Reset counts.
      id_counts_.clear();

      id_counts_.reserve(count_sources);
    }

    // In case weights are used, clear those as well.
    if (is_weighted) {
      csr_weights_.clear();
      csr_weights_.resize(count_edges);
    }

    fillCSR();
    edges_.clear();
    if (is_weighted) {
      edges_weights_.clear();
    }
  }

  inline bool usesCompactVertexStorage() { return compact_storage_; }

  bool edgeExists(const edge_t& edge) {
    local_edge_t local_edge = getLocalEdge(edge);

    // Return directly if edge was deleted.
    if (deleted_edges_.find(local_edge) != deleted_edges_.end()) {
      return false;
    }

    // First check CSR, if it exists.
    // Look for source index, traverse list of outgoing edges from there.
    uint32_t offset = 0;
    if (compact_storage_) {
      for (uint32_t i = 0; i < local_edge.src; ++i) {
        offset += counts_[i];
      }
    } else {
      for (const auto& source: id_counts_) {
        if (source.id == local_edge.src) {
          break;
        }
        offset += source.count;
      }
    }

    if (!targets_.empty()) {
      // Check list of outgoing edges for proper target.
      for (uint32_t i = offset; i < offset + counts_[local_edge.src]; ++i) {
        if (targets_[i] == local_edge.tgt) {
          return true;
        }
      }
    }

    // If we come here, the edge was not found in the CSR, try the edge list instead.
    return std::find(edges_.begin(), edges_.end(), local_edge) != edges_.end();
}

  inline bool edgeExists(const Edge& edge) {
    edge_t local_edge{edge.src(), edge.tgt()};
    return edgeExists(local_edge);
  }

  inline EdgeIterator begin() {
    return TileManager::EdgeIterator(countEdges(), 0, this);
  }

  inline EdgeIterator end() {
    return TileManager::EdgeIterator(countEdges(), countEdges(), this);
  }

  inline size_t countEdges() {
    return edges_.size() + targets_.size();
  }

  size_t countBytes() {
    size_t vertex_storage = 0;
    if (compact_storage_) {
      vertex_storage = vertices_per_tile_ * sizeof(local_vertex_id_t);
    } else {
      vertex_storage = id_counts_.size() * sizeof(vertex_count_t);
    }
    size_t edge_size = edges_.size() * sizeof(local_edge_t);
    size_t targets_size = targets_.size() * sizeof(local_vertex_id_t);

    if (is_weighted) {
      edge_size += edges_weights_.size() * sizeof(float);
    }

    return edge_size + targets_size + vertex_storage;
  }

  inline size_t countCompactions() { return count_compactions_; }

  inline edge_t getGlobalEdge(const local_edge_t& local_edge) {
    edge_t edge{};
    edge.src = getGlobalSrc(local_edge.src);
    edge.tgt = getGlobalTgt(local_edge.tgt);
    return edge;
  }

  inline local_edge_t getLocalEdge(const Edge& edge) {
    return getLocalEdge({edge.src(), edge.tgt()});
  }

  local_edge_t getLocalEdge(const edge_t& edge) {
    size_t vertices_per_tile_mask = vertices_per_tile_ - 1;
    local_edge_t local_edge{};
    local_edge.src =
        static_cast<local_vertex_id_t>(edge.src &
                                       (UINT64_MAX & vertices_per_tile_mask));
    local_edge.tgt =
        static_cast<local_vertex_id_t>(edge.tgt &
                                       (UINT64_MAX & vertices_per_tile_mask));
    return local_edge;
  }

  inline uint64_t x() const { return x_; }

  inline uint64_t y() const { return y_; }

  inline uint64_t traversal_id() const { return traversal_id_; }

  Coordinates getMetaTileCoords() {
    return Coordinates{
        x_ / config_->context_.tile_store_config.td_count_tiles_per_meta_tiles_per_dimension,
        y_ / config_->context_.tile_store_config.td_count_tiles_per_meta_tiles_per_dimension};
  }

  bool isActive() {
    // Check sources and targets for active status.
    if (Algorithm::NEEDS_SOURCES_ACTIVE) {
      // Check if at least one source is active.
      vertex_id_t start_src = getGlobalSrc(0);
      vertex_id_t
          end_src = getGlobalSrc(static_cast<const local_vertex_id_t>(vertices_per_tile_ - 1));
      // Divide by 8 as each char stores 8 values.
      vertex_id_t current = start_src / 8;

      while (((int64_t) end_src / 8) - (int64_t) current > 64) {
        if ((uint64_t) vertex_store_->vertices_.active_current[current] != 0) {
          return true;
        }
        current += 64 / 8;
      }

      // Reset to the global coordinates.
      current = current * 8;
      for (vertex_id_t src_id = current; src_id <= end_src; ++src_id) {
        if (eval_bool_array(vertex_store_->vertices_.active_current, src_id)) {
          return true;
        }
      }
    }

    if (Algorithm::NEEDS_TARGETS_ACTIVE) {
      // Check if at least one target is active.
      vertex_id_t start_tgt = getGlobalTgt(0);
      vertex_id_t
          end_tgt = getGlobalTgt(static_cast<const local_vertex_id_t>(vertices_per_tile_ - 1));
      // Divide by 8 as each char stores 8 values.
      vertex_id_t current = start_tgt / 8;

      while (((int64_t) end_tgt / 8) - (int64_t) current > 64) {
        if ((uint64_t) vertex_store_->vertices_.active_current[current] != 0) {
          return true;
        }
        current += 64 / 8;
      }

      // Reset to the global coordinates.
      current = current * 8;
      for (vertex_id_t tgt_id = current; tgt_id <= end_tgt; ++tgt_id) {
        if (eval_bool_array(vertex_store_->vertices_.active_current, tgt_id)) {
          return true;
        }
      }
    }

    // Not a single source or target was active, tile is inactive.
    return false;
  }

  inline bool isCompactStorage() { return compact_storage_; }

  inline local_vertex_id_t* getCounts() { return counts_; }

  inline std::vector<local_vertex_id_t>* getTargets() { return &targets_; }

  std::vector<float>* getEdgeWeights() { return &edges_weights_; }

  inline std::vector<float>* getCSRWeights() { return &csr_weights_; }

  inline std::vector<vertex_count_t>* getIdCounts() { return &id_counts_; }

  inline std::vector<local_edge_t>* getEdges() { return &edges_; }

  inline std::vector<bool>* getDeletedEdgesBitset() {
    return &deleted_edges_bitset_;
  }

  inline uint64_t getMetaTileId() const {
    return meta_tile_id_;
  }

  inline void setMetaTileId(uint64_t metaTileId) {
    meta_tile_id_ = metaTileId;
  }

private:
  uint32_t copyCSRToEdgeList() {
    // Iterate edges in CSR, add to edge list.
    uint32_t current_count = 0;
    if (compact_storage_) {
      for (uint32_t i = 0; i < vertices_per_tile_; ++i) {
        uint32_t max_count = current_count + counts_[i];
        for (; current_count < max_count; ++current_count) {
          local_edge_t local_edge{};
          local_edge.src = static_cast<local_vertex_id_t>(i);
          local_edge.tgt = targets_[current_count];
          edges_.push_back(local_edge);
          if (is_weighted) {
            edges_weights_.push_back(csr_weights_[current_count]);
          }
        }
      }
    } else {
      for (const auto& source : id_counts_) {
        uint32_t max_count = current_count + source.count;
        for (; current_count < max_count; ++current_count) {
          local_edge_t local_edge{};
          local_edge.src = source.id;
          local_edge.tgt = targets_[current_count];
          edges_.push_back(local_edge);
          if (is_weighted) {
            edges_weights_.push_back(csr_weights_[current_count]);
          }
        }
      }
    }

    return current_count;
  }

  void fillCSR() {
    local_edge_t previous_edge{};
    previous_edge.src = UINT16_MAX;
    previous_edge.tgt = UINT16_MAX;

    local_vertex_id_t current_src = edges_.front().src;
    {
      vertex_count_t count{};
      count.count = 0;
      count.id = current_src;
      id_counts_.push_back(count);
    }
    for (uint64_t i = 0; i < edges_.size(); ++i) {
      local_edge_t edge = edges_[i];

      // Skip deleted edges.
      if (deleted_edges_.find(edge) != deleted_edges_.end()) {
        continue;
      }

      // Skip repeated edges.
      if (edge == previous_edge) {
        continue;
      }
      // Update previous edge with current edge.
      previous_edge = edge;

      if (compact_storage_) {
        ++counts_[edge.src];
        assert(counts_[edge.src] != UINT16_MAX);
      } else {
        // New source, add counter for it.
        if (current_src != edge.src) {
          current_src = edge.src;
          vertex_count_t count{};
          count.count = 0;
          count.id = current_src;
          id_counts_.push_back(count);
        }
        ++id_counts_.back().count;
        assert(id_counts_.back().count != UINT16_MAX);
      }
      targets_.push_back(edge.tgt);
      if (is_weighted) {
        csr_weights_.push_back(edges_weights_[i]);
      }
    }
  }

  inline vertex_id_t getGlobalSrc(const local_vertex_id_t local_src) {
    return (x_ * vertices_per_tile_) | local_src;
  }

  inline vertex_id_t getGlobalTgt(const local_vertex_id_t local_tgt) {
    return (y_ * vertices_per_tile_) | local_tgt;
  }

  const uint64_t vertices_per_tile_;

  const uint64_t x_;
  const uint64_t y_;

  const uint64_t traversal_id_;

  uint32_t next_compaction_;
  uint32_t count_compactions_;

  // CSR storage.
  local_vertex_id_t* counts_;
  std::vector<vertex_count_t> id_counts_;
  std::vector<local_vertex_id_t> targets_;
  std::vector<float> csr_weights_;

  bool compact_storage_;

  // Edge list storage.
  std::vector<local_edge_t> edges_;
  std::vector<float> edges_weights_;

  // Deleted edges.
  std::set<local_edge_t> deleted_edges_;
  std::vector<bool> deleted_edges_bitset_;

  // For selecting a random edge to delete.
  std::random_device rd_;
  std::mt19937 gen_;

  // The associated meta tile ID.
  uint64_t meta_tile_id_;

  FRIEND_TEST(TileManagerTest, InsertEdge);

  std::shared_ptr<Config> config_;

  std::shared_ptr<VertexStore<VertexType> > vertex_store_;
};

// ALGORITHM_TEMPLATE_WEIGHTED_H(TileManager)

} // evolving_graphs


#endif //EVOLVING_GRAPHS_TILE_MANAGER_H
