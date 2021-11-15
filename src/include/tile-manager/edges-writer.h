// SPDX-License-Identifier: MIT

#ifndef EVOLVING_GRAPHS_EDGES_WRITER_H
#define EVOLVING_GRAPHS_EDGES_WRITER_H

#include "unistd.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <memory>

#include "util/runnable.h"
#include "ring-buffer/ring-buffer.h"
#include "util/config.h"
#include "schema_generated.h"
#include "tile-manager/meta-tile-manager.h"

namespace evolving_graphs {

template<typename VertexType, class Algorithm, bool is_weighted>
class MetaTileManager;

template<typename VertexType, class Algorithm, bool is_weighted>
class EdgesWriter : public util::Runnable {
public:
  EdgesWriter(uint64_t id, const Ringbuffer& rb, std::shared_ptr<Config> config) :
  			id_(id), rb_(rb),
  			config_(config) {}

  void readEdgesFromFile(MetaTileManager<VertexType, Algorithm, is_weighted>* meta_tm) {
	  auto count_edges = getCountEdgesToRead();

	  std::string filename =
	      config_->context_.persistence_config.path_to_edges_input + "edges_" + std::to_string(id_);
	  int fd = open(filename.c_str(), O_RDONLY);

	  edge_compact_t* file_edges = nullptr;
	  edge_compact_weighted_t* file_edges_weighted = nullptr;
	  if (is_weighted) {
	    file_edges_weighted =
	        (edge_compact_weighted_t*) mmap(nullptr,
	                                        count_edges * sizeof(edge_compact_weighted_t),
	                                        PROT_READ,
	                                        MAP_PRIVATE,
	                                        fd,
	                                        0);
	    if (file_edges_weighted == MAP_FAILED) {
	      sg_err("Could not open file %s: %d\n", config_->context_.binary_input_file.c_str(), errno);
	      util::die(1);
	    }
	  } else {
	    file_edges = (edge_compact_t*) mmap(nullptr,
	                                        count_edges * sizeof(edge_compact_t),
	                                        PROT_READ,
	                                        MAP_PRIVATE,
	                                        fd,
	                                        0);
	    if (file_edges == MAP_FAILED) {
	      sg_err("Could not open file %s: %d\n", config_->context_.binary_input_file.c_str(), errno);
	      util::die(1);
	    }
	  }


	  for (uint64_t i = 0; i < count_edges; ++i) {
	    Edge edge(0, 0, 0.0);
	    if (is_weighted) {
	      edge_compact_weighted_t file_edge = file_edges_weighted[i];
	      edge.mutate_src(file_edge.src);
	      edge.mutate_tgt(file_edge.tgt);
	      edge.mutate_weight(file_edge.weight);
	    } else {
	      edge_compact_t file_edge = file_edges[i];
	      edge.mutate_src(file_edge.src);
	      edge.mutate_tgt(file_edge.tgt);
	    }
	    meta_tm->insertEdge(edge);
	  }

	  int err = close(fd);
	  if (err != 0) {
	    sg_err("File %s couldn't be closed: %s\n", filename.c_str(), strerror(errno));
	    util::die(1);
	  }
	}

  size_t getCountEdgesToRead() {
	  std::string filename =
	      config_->context_.persistence_config.path_to_edges_input + "edges_" + std::to_string(id_);

	  int fd = open(filename.c_str(), O_RDONLY);
	  struct stat file_stats{};
	  fstat(fd, &file_stats);
	  auto file_size = static_cast<size_t>(file_stats.st_size);

	  int err = close(fd);
	  if (err != 0) {
	    sg_err("File %s couldn't be closed: %s\n", filename.c_str(), strerror(errno));
	    util::die(1);
	  }

	  size_t count_edges_in_file;
	  if (is_weighted) {
	    count_edges_in_file = file_size / sizeof(edge_compact_weighted_t);
	  } else {
	    count_edges_in_file = file_size / sizeof(edge_compact_t);
	  }

	  return count_edges_in_file;
	}

protected:
  void run() override {
	  std::string filename =
	      config_->context_.persistence_config.path_to_edges_output + "edges_" + std::to_string(id_);
	  int fd = open(filename.c_str(), O_WRONLY | O_CREAT | O_SYNC, 0755);
	  if (fd == -1) {
	    sg_err("Unable to open file %s: %s\n", filename.c_str(), strerror(errno));
	    util::die(1);
	  }

	  bool use_short_ids = (config_->context_.max_count_vertices <= UINT32_MAX);

	  while (true) {
	    ring_buffer_req_t req{};
	    ring_buffer_get_req_init(&req, BLOCKING);
	    rb_.get(&req);
	    auto message = flatbuffers::GetRoot<EdgeInserterDataMessage>(req.data);

	    if (message->shutdown()) {
	      rb_.set_done(&req);
	      break;
	    }

	    if (use_short_ids) {
	      if (is_weighted) {
	        std::vector<edge_compact_weighted_t> compact_edges;
	        compact_edges.reserve(message->edges()->size());

	        for (const auto edge : *message->edges()) {
	          compact_edges.push_back({(static_cast<uint32_t>(edge->src())),
	                                   static_cast<uint32_t>(edge->tgt()), edge->weight()});
	        }

	        // Write received edges out to already open file.
	        size_t size_to_write = sizeof(edge_compact_weighted_t) * compact_edges.size();
	        if (write(fd, compact_edges.data(), size_to_write) != size_to_write) {
	          sg_err("Fail to write to file %s: %s\n", filename.c_str(), strerror(errno));
	          util::die(1);
	        }
	      } else {
	        std::vector<edge_compact_t> compact_edges;
	        compact_edges.reserve(message->edges()->size());

	        for (const auto edge : *message->edges()) {
	          compact_edges.push_back({(static_cast<uint32_t>(edge->src())),
	                                   static_cast<uint32_t>(edge->tgt())});
	        }

	        // Write received edges out to already open file.
	        size_t size_to_write = sizeof(edge_compact_t) * compact_edges.size();
	        if (write(fd, compact_edges.data(), size_to_write) != size_to_write) {
	          sg_err("Fail to write to file %s: %s\n", filename.c_str(), strerror(errno));
	          util::die(1);
	        }
	      }
	    } else {
	      size_t size_to_write = sizeof(Edge) * message->edges()->size();
	      if (write(fd, message->edges()->data(), size_to_write) != size_to_write) {
	        sg_err("Fail to write to file %s: %s\n", filename.c_str(), strerror(errno));
	        util::die(1);
	      }
	    }

	    // Delete message.
	    rb_.set_done(&req);
	  }

	  int err = close(fd);
	  if (err != 0) {
	    sg_err("File %s couldn't be written: %s\n", filename.c_str(), strerror(errno));
	    util::die(1);
	  }
	}

private:
  const uint64_t id_;

  Ringbuffer rb_;

  std::shared_ptr<Config> config_;

};

// ALGORITHM_TEMPLATE_WEIGHTED_H(EdgesWriter)

}

#endif //EVOLVING_GRAPHS_EDGES_WRITER_H
