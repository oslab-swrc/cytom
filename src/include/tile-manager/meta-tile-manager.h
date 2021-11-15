// SPDX-License-Identifier: MIT

#ifndef EVOLVING_GRAPHS_META_TILE_MANAGER_H
#define EVOLVING_GRAPHS_META_TILE_MANAGER_H

#include <memory>

#include <flatbuffers/flatbuffers.h>

#include "util/runnable.h"
#include "ring-buffer/ring-buffer.h"
#include "tile-manager/edges-writer.h"
#include "tile-manager/tile-manager.h"
#include "core/tile-store.h"
#include "core/vertex-store.h"
#include "schema_generated.h"
#include "util/util.h"
#include "perf-event/perf-event-manager.h"
#include "perf-event/perf-event-scoped.h"
#include "core/continuous-algorithm-executor.h"

namespace evolving_graphs {

using namespace perf_event;

template<typename VertexType, class Algorithm, bool is_weighted>
class MetaTileManager : public util::Runnable {
public:
  MetaTileManager(const Ringbuffer& edge_inserter_sync_rb,
                  const Ringbuffer& meta_tile_manager_sync_rb, uint32_t id,
                  pthread_barrier_t* read_edges_barrier, std::shared_ptr<Config> config,
                  std::shared_ptr<Algorithm> algorithm,
		  std::shared_ptr<TileStore<VertexType, Algorithm, is_weighted> > tile_store,
		  std::shared_ptr<ContinuousAlgorithmExecutor<VertexType, Algorithm, is_weighted> > continuous_algorithm_executor,
		  std::shared_ptr<VertexStore<VertexType> > vertex_store) 
        : edge_inserter_sync_rb_(edge_inserter_sync_rb),
        meta_tile_manager_sync_rb_(meta_tile_manager_sync_rb),
        id_(id),
        read_edges_barrier_(read_edges_barrier),
        gen_(rd_()),
        dis_(0,
             static_cast<int>(pow(config->context_.tile_store_config.count_tiles_per_dimension, 2) /
                              config->context_.meta_tile_manager_count)),
        config_(std::move(config)),
        algorithm_(std::move(algorithm)),
	tile_store_(std::move(tile_store)),
	continuous_algorithm_executor_(std::move(continuous_algorithm_executor)),
	vertex_store_(std::move(vertex_store)){
    data_rb_.create(512 * MB, L1D_CACHELINE_SIZE, true, nullptr, nullptr);
    control_rb_.create(16 * MB, L1D_CACHELINE_SIZE, true, nullptr, nullptr);
    if (config_->context_.persistence_config.enable_writing_edges ||
        config_->context_.persistence_config.enable_reading_edges) {
      edges_writer_rb_.create(64 * MB, L1D_CACHELINE_SIZE, true, nullptr, nullptr);
      edges_writer_ = new EdgesWriter<VertexType, Algorithm, is_weighted>(id_, edges_writer_rb_, config_);
    }
    if (config_->context_.enable_two_level_tile_distributor) {
      uint64_t
          count_meta_tiles = config_->context_.tile_store_config.td_count_meta_tiles;
      td_meta_tile_counts_ = new uint64_t[count_meta_tiles];
      memset(td_meta_tile_counts_, 0, count_meta_tiles * sizeof(td_meta_tile_counts_[0]));
    }

    // Set up mapping between an integer and the tiles belonging to this MetaTileManager.
    uint64_t count_tiles = config_->context_.tile_store_config.count_tiles;
    size_t
        count_meta_tile_managers = (size_t) std::max(1, config_->context_.meta_tile_manager_count - 1);
    auto count_tiles_per_manager =
        static_cast<size_t>(std::ceil(double(count_tiles) / count_meta_tile_managers));
    uint8_t meta_tile_manager_count = config_->context_.meta_tile_manager_count;

    tile_to_meta_manager_mapping_ = new uint64_t[count_tiles_per_manager];

    size_t count = 0;
    for (uint64_t i = 0; i < count_tiles; ++i) {
      Coordinates
          coords = tile_store_->getTileManagerCoordinates(i);
      auto assigned_tm_id = static_cast<uint8_t>((coords.x + coords.y) % meta_tile_manager_count);
      if (assigned_tm_id == id_) {
        if (count >= count_tiles_per_manager) {
          sg_log("i %lu, Count %lu, count-tiles-per-manager %lu\n",
                 i,
                 count,
                 count_tiles_per_manager);
          util::die(1);
        }
        tile_to_meta_manager_mapping_[count] = i;
        ++count;
      }
    }

  }

  ~MetaTileManager() override {
    if (config_->context_.persistence_config.enable_writing_edges) {
      delete edges_writer_;
    }
    if (config_->context_.enable_two_level_tile_distributor) {
      delete[] td_meta_tile_counts_;
    }

    delete[] tile_to_meta_manager_mapping_;
  }

  inline const Ringbuffer& data_rb() { return data_rb_;}

  inline const Ringbuffer& control_rb() { return control_rb_; }

  inline uint64_t edgesPerMetaTile(uint64_t meta_tile_id) { return td_meta_tile_counts_[meta_tile_id]; }

  inline size_t getCountEdgesToRead() { return edges_writer_->getCountEdgesToRead(); }

  void inline insertEdge(const Edge& edge) {
    TileManager<VertexType, Algorithm, is_weighted>
        * responsible_tm = tile_store_->getTileManager(edge);
    responsible_tm->insertEdge(edge);

    if (config_->context_.enable_two_level_tile_distributor) {
      // Add to meta td count.
      const uint64_t meta_tile_id = responsible_tm->getMetaTileId();
      td_meta_tile_counts_[meta_tile_id] += 1;
    }

    vertex_store_->incrementOutDegree(edge.src());
    vertex_store_->incrementInDegree(edge.tgt());
  }

  void inline deleteEdge(const Edge& edge) {
    TileManager<VertexType, Algorithm, is_weighted>
        * responsible_tm = tile_store_->getTileManager(edge);
    responsible_tm->deleteEdge(edge);

    if (config_->context_.enable_two_level_tile_distributor) {
      // Add to meta td count.
      const uint64_t meta_tile_id = responsible_tm->getMetaTileId();
      td_meta_tile_counts_[meta_tile_id] -= 1;
    }

    VertexStore<VertexType>::decrementOutDegree(edge.src());
    VertexStore<VertexType>::decrementInDegree(edge.tgt());
  }

protected:
  void run() override {
    // First, read in edges from persistent backup, if needed.
    if (config_->context_.persistence_config.enable_reading_edges) {
      edges_writer_->readEdgesFromFile(this);
      // Notify that we are done reading in edges.
      pthread_barrier_wait(read_edges_barrier_);
    }
    // Kick of edges writer, if needed.
    if (config_->context_.persistence_config.enable_writing_edges) {
      edges_writer_->start();
    }

    //  uint64_t count = 0;
    while (true) {
      ring_buffer_req_t req{};
      ring_buffer_get_req_init(&req, BLOCKING);
      control_rb_.get(&req);
      auto message = flatbuffers::GetRoot<EdgeInserterControlMessage>(req.data);
      if (message->shutdown()) {
        if (config_->context_.persistence_config.enable_writing_edges) {
          // Shutdown EdgesWriter if enabled.
          flatbuffers::FlatBufferBuilder builder;
          auto shutdown_message = CreateEdgeInserterDataMessage(builder, 0, 0, true);
          builder.Finish(shutdown_message);
          util::sendMessage(edges_writer_rb_, builder);
        }
        break;
      }

      //    sg_log("Received: Message Pointer: %lu, remaining pointer: %lu\n",
      //           message->message_pointer(), message->remaining_tile_managers_pointer());

      // Retrieve data message.
      auto data_rb_req = reinterpret_cast<uint8_t*>(message->message_pointer());
      auto data_message = flatbuffers::GetRoot<EdgeInserterDataMessage>(data_rb_req);

      //    PerfEventScoped event(PerfEventManager::getInstance()->getRingBuffer(),
      //                          "receiveEdges-" + std::to_string(count++),
      //                          "MetaTileManager",
      //                          id_,
      //                          config_->context_.enable_perf_event_collection,
      //                          std::to_string(data_message->edges()->size()));

      //    sg_dbg("Received: Id: %u, pointer: %lu / %p, edges received: %d\n",
      //           id_,
      //           message->message_pointer(),
      //           data_rb_req,
      //           data_message->edges()->size());
      //    sg_dbg("Edges received: %d\n", data_message->edges()->size());
      for (const auto& edge : *data_message->edges()) {
        insertEdge(*edge);
        // If enabled, use the edgeChanged API to allow post-processing by the algorithm.
        if (likely(config_->context_.enable_edge_apis)) {
          algorithm_->edgeChanged(*edge, EdgeChangeEvent::INSERT);
        }
      }

      bool reexecute_algorithm = false;
      // Delete a set number of edges.
      if (config_->context_.deletion_percentage > 0.0) {
        // Calculate number of edges to delete according to the inserted count, the deletion
        // percentage and the total count of MetaTileManagers.
        auto edges_to_delete = static_cast<size_t>(config_->context_.batch_insertion_count *
                                                   config_->context_.deletion_percentage /
                                                   config_->context_.meta_tile_manager_count);
        for (size_t i = 0; i < edges_to_delete; ++i) {
          int random_tile_manager = dis_(gen_);
          uint64_t tile_manager_id = tile_to_meta_manager_mapping_[random_tile_manager];
          TileManager<VertexType, Algorithm, is_weighted>* tile_manager =
              tile_store_->getTileManager(tile_manager_id);
          edge_t deleted_edge{};
          if (tile_manager->deleteRandomEdge(&deleted_edge)) {
            if (likely(config_->context_.enable_edge_apis)) {
              // In case the algorithm will anyhow be re-executed, don't run the edge deletion
              // optimization.
              if (config_->context_.enable_algorithm_reexecution) {
                reexecute_algorithm = true;
              } else {
                Edge deleted_edge_container(deleted_edge.src, deleted_edge.tgt, 0.0);
                if (algorithm_->edgeChanged(deleted_edge_container,
                                           EdgeChangeEvent::DELETE)) {
                  vertex_store_->vertices_.critical_neighbor[deleted_edge.tgt] = UINT32_MAX;
                  reexecute_algorithm = true;
                }
              }
            }
          }
        }
      }

      // Persist edges if needed.
      if (config_->context_.persistence_config.enable_writing_edges) {
        ring_buffer_req_t edges_req{};
        ring_buffer_put_req_init(&edges_req, BLOCKING, message->size_message_pointer());
        edges_writer_rb_.put(&edges_req);
        sg_rb_check(&edges_req);
        edges_writer_rb_.copyToBuffer(edges_req.data, data_rb_req, message->size_message_pointer());
        edges_writer_rb_.set_ready(&edges_req);
      }

      // Only do algorithm-related things when requested to do so.
      if (config_->context_.enable_dynamic_algorithm) {
        // If in lazy mode, notify continuous executor to handle an algo execution.
        // Otherwise, simply notify the EdgeInserter of the completion of this MetaTileManager to let
        // him handle the algo execution.
        if (config_->context_.enable_lazy_algorithm) {
          // Only the last MetaTileManager only has to do notify the algorithm executor.
          auto* count_item =
              reinterpret_cast<remaining_meta_tile_managers_t*>(message->remaining_tile_managers_pointer());
          if (smp_faa(&count_item->count, -1) == 1) {
            flatbuffers::FlatBufferBuilder builder;
            auto algorithm_message = CreateAlgorithmMessage(builder, false);
            builder.Finish(algorithm_message);
            Ringbuffer
                continuous_rb =
                continuous_algorithm_executor_->rb();
            util::sendMessage(continuous_rb, builder);

            // Set message done, last MetaTileManager notified the AlgoExecutor.
            meta_tile_manager_sync_rb_.set_done(count_item);
          }
        } else {
          flatbuffers::FlatBufferBuilder builder;
          auto edges_inserted_message = CreateEdgesInsertedMessage(builder, reexecute_algorithm);
          builder.Finish(edges_inserted_message);
          util::sendMessage(edge_inserter_sync_rb_, builder);
        }
      }

      data_rb_.set_done(data_rb_req);
      control_rb_.set_done(&req);
    }
  }

private:
  Ringbuffer data_rb_;

  Ringbuffer control_rb_;

  Ringbuffer edges_writer_rb_;

  Ringbuffer edge_inserter_sync_rb_;

  Ringbuffer meta_tile_manager_sync_rb_;

  pthread_barrier_t* read_edges_barrier_;

  EdgesWriter<VertexType, Algorithm, is_weighted>* edges_writer_;

  uint32_t id_;

  uint64_t* td_meta_tile_counts_;

  std::random_device rd_;

  std::mt19937 gen_;

  std::uniform_int_distribution<> dis_;

  uint64_t* tile_to_meta_manager_mapping_;

  std::shared_ptr<Config> config_;

  std::shared_ptr<Algorithm> algorithm_;

  std::shared_ptr<TileStore<VertexType, Algorithm, is_weighted> > tile_store_;

  std::shared_ptr<ContinuousAlgorithmExecutor<VertexType, Algorithm, is_weighted> > continuous_algorithm_executor_;

  std::shared_ptr<VertexStore<VertexType> > vertex_store_;
};

// ALGORITHM_TEMPLATE_WEIGHTED_H(MetaTileManager)

}

#endif //EVOLVING_GRAPHS_META_TILE_MANAGER_H
