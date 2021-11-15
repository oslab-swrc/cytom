// SPDX-License-Identifier: MIT

#pragma once

#include <string>
#include <vector>


#include "ring_buffer.h"
#include "util/datatypes_config.h"
#include "util/datatypes-algorithm.h"

// For intermediate usage when reading a graph into partitions
#define MB (1024ul * 1024ul)
#define GB (1024 * MB)

#define RMAT_A 0.57f
#define RMAT_B 0.19f
#define RMAT_C 0.19f
#define RMAT_SEED 2

#define MAX_WEIGHT 1.0

// alignment for tile reading
// NOTE: TILE_READ_ALIGN must be power of 2
#define TILE_READ_ALIGN (128 * 1024)
#define TILE_READ_ALIGN_MASK (~(TILE_READ_ALIGN - 1))
static_assert((TILE_READ_ALIGN & (TILE_READ_ALIGN - 1)) == 0,
              "TILE_READ_ALIGN must be power of two");

#define MAGIC_IDENTIFIER 11118719669451714817ul

enum class EdgeChangeEvent {
  INSERT, DELETE,
};

typedef uint64_t vertex_id_t;
typedef uint16_t local_vertex_id_t;
typedef uint32_t edge_local_id_t;

struct file_edge_t {
  int src;
  int tgt;
};

struct edge_t {
  vertex_id_t src;
  vertex_id_t tgt;

  bool operator==(const edge_t& other) const {
    return (src == other.src) && (tgt == other.tgt);
  }
};

struct edge_compact_t {
  uint32_t src;
  uint32_t tgt;
};

struct edge_compact_weighted_t {
  uint32_t src;
  uint32_t tgt;
  float weight;
};

struct edge_weighted_t {
  vertex_id_t src;
  vertex_id_t tgt;

  float weight;

  bool operator==(const edge_weighted_t& other) const {
    return (src == other.src) && (tgt == other.tgt) && (weight == other.weight);
  }
};

struct local_edge_t {
  local_vertex_id_t src;
  local_vertex_id_t tgt;

  // sort by source, then by target
  bool operator<(const local_edge_t& other) const {
    if (src == other.src) {
      return tgt < other.tgt;
    }
    return src < other.src;
  }

  bool operator==(const local_edge_t& other) const {
    return (src == other.src) && (tgt == other.tgt);
  }
};

struct local_edge_weighted_t {
  local_vertex_id_t tgt;
  local_vertex_id_t src;

  float weight;

  // sort by target, then by source
  bool operator<(const local_edge_weighted_t& other) const {
    if (tgt == other.tgt) {
      return src < other.src;
    }
    return tgt < other.tgt;
  }
};

struct Coordinates {
  uint64_t x;
  uint64_t y;
};

struct vertex_count_t {
  uint16_t count;
  local_vertex_id_t id;
};

struct vertex_degree_t {
  uint32_t in_degree;
  uint32_t out_degree;
};

struct scenario_stats_t {
  uint64_t count_vertices;
  uint64_t count_tiles;
  bool is_index_32_bits;
  bool is_weighted_graph;
  bool index_33_bit_extension;
};

struct tile_stats_t {
  uint64_t block_id;
  uint32_t count_vertex_src;
  uint32_t count_vertex_tgt;
  uint32_t count_edges;
  // indicates whether the target-block is encoded using run-length-encoding
  // if using RLE, the tgt-block is encoded as an array of vertex_count_t's
  bool use_rle;
};

struct command_line_args_grc_t {
  uint64_t nthreads;
  uint64_t count_partition_managers;
  uint64_t count_vertices;
  bool input_weighted;
  std::string graphname;
  std::string path_to_globals;
  std::vector<std::string> paths_to_partition;
};

// intuition: The current-array is of step T, the next-array of step T+1, they
// will get swapped when step T is done.
template <typename T>
struct vertex_array_t {
  vertex_degree_t* degrees;

  T* current;
  T* next;

  T* temp_next;

  uint8_t* active_current;
  uint8_t* active_next;

  // Keep track of the vertices which have changed in the current iteration.
  uint8_t* changed;

  uint32_t* critical_neighbor;
};

struct thread_index_t {
  size_t count;
  size_t id;
};

struct partition_meta_t {
  uint32_t count_edges;
};

struct partition_t {
  uint64_t i;
  uint64_t j;
};

struct meta_partition_meta_t {
  partition_t meta_partition_index;
  partition_t vertex_partition_index_start;
};

struct partition_edge_compact_t {
  uint32_t count_edges;
  uint32_t partition_offset_src;
  uint32_t partition_offset_tgt;
  local_edge_t edges[0];
};

struct partition_edge_list_t {
  uint32_t count_edges;
  bool shutdown_indicator;
  edge_t edges[0];
};

struct partition_edge_t {
  uint32_t count_edges;
  edge_t* edges;
};

struct remote_partition_edge_t {
  uint32_t count_edges;
  bool end_of_round;
  edge_t edges[0];
};

struct meta_partition_file_info_t {
  partition_t global_offset;
};

struct partition_manager_arguments_t {
  meta_partition_meta_t meta;
  std::vector<std::string> partitions;
  thread_index_t thread_index;
  ring_buffer_t* write_request_rb;
};

struct edge_write_request_t {
  int fd;
  local_edge_t edge;
  uint64_t offset;
};

struct vertex_area_t {
  uint64_t src_min;
  uint64_t src_max;
  uint64_t tgt_min;
  uint64_t tgt_max;
};

struct active_tiles_t {
  size_t count_active_tiles;
  char active_tiles[0];
};

enum class ProfilingType {
  PT_Duration,
  PT_RingbufferSizes,
};

enum class ComponentType {
  CT_GlobalReducer,
  CT_IndexReader,
  CT_None,
  CT_RingBufferSizes,
  CT_TileProcessor,
  CT_TileReader,
  CT_VertexApplier,
  CT_VertexFetcher,
  CT_VertexReducer,
};

struct profiling_duration_t {
  uint64_t time_start;
  uint64_t time_end;
  uint64_t metadata;
};

struct profiling_ringbuffer_sizes_t {
  uint64_t time;
  size_t size_index_rb;
  size_t size_tiles_rb;
  size_t size_response_rb;
};

struct profiling_data_t {
  // Determines the type of the union subtype.
  ProfilingType type;
  int64_t pid;
  // Support a two level id scheme, discriminating between local and global ids,
  // e.g. for MicId and FetcherId. In case of using only one level of Ids, e.g.
  // the VertexDomain, use the global_id to discriminate components.
  ComponentType component;
  int64_t local_id;
  int64_t global_id;

  union {
    profiling_duration_t duration;
    profiling_ringbuffer_sizes_t ringbuffer_sizes;
  };

  char name[0];
};

struct profiling_transport_t {
  // Indicates whether to shutdown, if set any other data is not meant to be
  // read.
  bool shutdown;

  // Has to be the last member to allow name to be packed in behind.
  profiling_data_t data;
};

enum class PerfEventMode { PEM_Host, PEM_Client };

#define VERTEX_LOCK_TABLE_SIZE 223

struct vertex_lock_table_t {
  size_t count;
  int salt;
  pthread_spinlock_t* locks;
};


template<typename VertexType>
struct tile_to_local_array_t {
  VertexType* array;
  uint32_t* critical_neighbor;
  uint64_t tile_y;
};

struct remaining_meta_tile_managers_t {
  uint16_t count;
};

struct vertex_value_type_t {
  float vertex_value;
  uint32_t vertex_id;

  bool operator<(const vertex_value_type_t& other) const {
    return vertex_value < other.vertex_value;
  }
};