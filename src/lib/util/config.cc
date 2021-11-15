// SPDX-License-Identifier: MIT

#include "util/config.h"
#include "util/util.h"

namespace evolving_graphs {

// ConfigContext Config::context_;

void Config::normalizeConfig() {
  // Adjust max count to the next power of two.
  context_.max_count_vertices =
      static_cast<uint64_t>(pow(2, util::log2Up(context_.max_count_vertices)));

  // Set the initial number of vertices:
  context_.count_vertices = context_.max_count_vertices;

  context_.tile_store_config.count_tiles_per_dimension = int_ceil(
      context_.count_vertices / context_.tile_store_config.vertices_per_tile, 1);
  context_.tile_store_config.count_tiles =
      static_cast<uint64_t>( std::pow(context_.tile_store_config.count_tiles_per_dimension,
                                      2));

  // Make sure the meta count is at most as much as there are tiles.
  context_.tile_store_config.td_count_meta_tiles_per_dimension =
      std::min(context_.tile_store_config.td_count_meta_tiles_per_dimension,
               context_.tile_store_config.count_tiles_per_dimension);

  context_.enable_two_level_tile_distributor =
      context_.tile_store_config.td_count_meta_tiles_per_dimension >= 1;

  context_.tile_store_config.td_count_meta_tiles =
      static_cast<uint64_t>(
          std::pow(context_.tile_store_config.td_count_meta_tiles_per_dimension, 2));

  if (context_.enable_two_level_tile_distributor) {
    context_.tile_store_config.td_count_tiles_per_meta_tiles_per_dimension =
        context_.tile_store_config.count_tiles_per_dimension /
        context_.tile_store_config.td_count_meta_tiles_per_dimension;


    context_.tile_store_config.td_count_tiles_per_meta_tiles =
        static_cast<uint64_t>(
            std::pow(context_.tile_store_config.td_count_tiles_per_meta_tiles_per_dimension,
                     2));
  }
}
}
