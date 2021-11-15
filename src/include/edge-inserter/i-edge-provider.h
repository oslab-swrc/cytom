// SPDX-License-Identifier: MIT

#ifndef EVOLVING_GRAPHS_I_EDGE_PROVIDER_H
#define EVOLVING_GRAPHS_I_EDGE_PROVIDER_H

#include <random>
#include <memory>

#include "util/datatypes.h"
#include "schema_generated.h"
#include "ring-buffer/ring-buffer.h"
#include "util/config.h"
#include "core/meta-tile-store.h"
#include "util/util.h"
#include "perf-event/perf-event-scoped.h"
#include "perf-event/perf-event-manager.h"

namespace evolving_graphs {

template<typename VertexType, class Algorithm, bool is_weighted>
class IEdgeProvider {
public:
  explicit IEdgeProvider(const Ringbuffer& edge_inserter_control_rb,
                         pthread_barrier_t* in_memory_barrier, uint64_t id, bool add_weights,
                         std::shared_ptr<Config> config,
			                   std::shared_ptr<MetaTileStore<VertexType, Algorithm, is_weighted> > meta_tile_store,
                         std::shared_ptr<VertexStore<VertexType> > vertex_store)
      : id_(id),
        add_weights_(add_weights),
        size_current_batch_(0),
        weight_gen_(weight_rd_()),
        weight_dis_(0.0, MAX_WEIGHT),
        edge_inserter_rb_(edge_inserter_control_rb),
        in_memory_barrier_(in_memory_barrier),
        config_(std::move(config)),
        meta_tile_store_(std::move(meta_tile_store)),
        vertex_store_(std::move(vertex_store))	{
    uint8_t count_meta_tile_managers = config_->context_.meta_tile_manager_count;
    temp_edges_.resize(count_meta_tile_managers);

    for (uint8_t i = 0; i < count_meta_tile_managers; ++i) {
      temp_edges_[i].reserve(config_->context_.batch_insertion_count);
    }
  }

protected:
  // Edge:: pass by value (copy)
  void addEdge(Edge edge) {
    // If necessary, rewrite src and tgt ids.
    if (config_->context_.enable_rewrite_ids) {
      edge.mutate_src(vertex_store_->getTranslatedId(edge.src()));
      edge.mutate_tgt(vertex_store_->getTranslatedId(edge.tgt()));
    }

    // In case of the in-memory ingestion, just add the edge to the pre-prepared vector.
    // Otherwise, go ahead and add the edge immediately.
    if (config_->context_.enable_in_memory_ingestion) {
      edges_.emplace_back(edge);
    } else {
      this->processEdge(edge);
    }
  }

  void insertionFinished() {
    if (config_->context_.enable_in_memory_ingestion) {
      // In case of the in-memory ingestion, first wait for all other EdgeProviders to be done
      // reading.
      pthread_barrier_wait(in_memory_barrier_); // TODO: Mingyu

      // Now is the time to send out all edges.
      for (auto& edge : edges_) {
        this->processEdge(edge);
      }
      this->sendEdgesToTileManagers(true);
    } else {
      this->sendEdgesToTileManagers(true);
    }
  }

private:
  void processEdge(Edge& edge) {
    ++size_current_batch_;

    // Split edges into blocks per meta tile manager.
    uint64_t meta_id = meta_tile_store_->getMetaTileManagerId(edge);
    if (is_weighted) {
      if (add_weights_) {
        edge.mutate_weight(static_cast<float>(weight_dis_(weight_gen_)));
      }
    }
    temp_edges_[meta_id].emplace_back(edge);

    if (size_current_batch_ >= config_->context_.batch_insertion_count) {
      sendEdgesToTileManagers(false);
    }
  }

  void sendEdgesToTileManagers(bool final_batch) {
    //  PerfEventScoped event(PerfEventManager::getInstance()->getRingBuffer(),
    //                        "sendEdges-" + std::to_string(size_current_batch_),
    //                        "EdgeProvider",
    //                        id_,
    //                        config_->context_.enable_perf_event_collection);

    std::vector<EdgeInserterInfoMessage> info_messages;
    for (uint8_t id_meta_tm = 0;
         id_meta_tm < config_->context_.meta_tile_manager_count;
         ++id_meta_tm) {
      if (temp_edges_[id_meta_tm].empty()) {
        continue;
      }

      MetaTileManager<VertexType, Algorithm, is_weighted>* meta_tm =
          meta_tile_store_->getMetaTileManager(id_meta_tm);
      Ringbuffer rb = meta_tm->data_rb();

      flatbuffers::FlatBufferBuilder builder;
      auto edges = builder.CreateVectorOfStructs(temp_edges_[id_meta_tm]);
      auto message = CreateEdgeInserterDataMessage(builder, id_meta_tm, edges);
      builder.Finish(message);
      uint32_t size = builder.GetSize();
      ring_buffer_req_t req{};
      ring_buffer_put_req_init(&req, BLOCKING, size);
      rb.put(&req);
      sg_rb_check(&req);

      sg_dbg("Sent %lu edges to MetaTileManager %u.\n",
             temp_edges_[id_meta_tm].size(),
             id_meta_tm);

      // Add info to info-messages vector.
      EdgeInserterInfoMessage info_message
          (reinterpret_cast<uint64_t>(req.data),
           req.size,
           id_meta_tm,
           temp_edges_[id_meta_tm].size());
      info_messages.push_back(info_message);

      rb.copyToBuffer(req.data, builder.GetBufferPointer(), size);
      rb.set_ready(&req);
    }

    flatbuffers::FlatBufferBuilder builder;
    auto info_messages_struct = builder.CreateVectorOfStructs(info_messages);
    auto message = CreateMetaEdgeInserterMessage(builder, info_messages_struct, final_batch);

    builder.Finish(message);
    util::sendMessage(edge_inserter_rb_, builder);

    size_current_batch_ = 0;

    for (auto& edges : temp_edges_) {
      edges.clear();
    }
  }

  uint64_t id_;

  bool add_weights_;

  std::random_device weight_rd_;

  std::mt19937 weight_gen_;

  std::uniform_real_distribution<> weight_dis_;

  std::vector<std::vector<Edge>> temp_edges_;

  uint64_t size_current_batch_;

  Ringbuffer edge_inserter_rb_;

  std::vector<Edge> edges_;

  pthread_barrier_t* in_memory_barrier_;

  std::shared_ptr<Config> config_;

  std::shared_ptr<MetaTileStore<VertexType, Algorithm, is_weighted> > meta_tile_store_;

  std::shared_ptr<VertexStore<VertexType> > vertex_store_;
};

// ALGORITHM_TEMPLATE_WEIGHTED_H(IEdgeProvider)

}

#endif //EVOLVING_GRAPHS_I_EDGE_PROVIDER_H
