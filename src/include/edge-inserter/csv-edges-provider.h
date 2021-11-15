// SPDX-License-Identifier: MIT

#ifndef EVOLVING_GRAPHS_CSV_EDGES_PROVIDER_H
#define EVOLVING_GRAPHS_CSV_EDGES_PROVIDER_H

#include <fstream>
#include <sstream>

#include "i-edge-provider.h"
#include "util/config.h"

namespace evolving_graphs {

template<typename VertexType, class Algorithm, bool is_weighted>
class CSVEdgesProvider : public IEdgeProvider<VertexType, Algorithm, is_weighted> {
public:
  CSVEdgesProvider(std::string file, const Ringbuffer& rb, bool read_weights, const std::shared_ptr<Config> & config)
  	: IEdgeProvider<VertexType, Algorithm, is_weighted>(rb, nullptr, 0, !read_weights, config),
      file_(std::move(file)),
      read_weights_(read_weights) {}

  void parse() {
	  std::ifstream file(file_);
	  std::string line;
	  while (std::getline(file, line)) {
	    Edge edge;

	    std::string part;
	    std::istringstream iss(line);
	    std::getline(iss, part, ',');
	    edge.mutate_src(std::stoull(part));
	    std::getline(iss, part, ',');
	    edge.mutate_tgt(std::stoull(part));

	    if (read_weights_) {
	      std::getline(iss, part, ',');
	      edge.mutate_weight(std::stof(part));
	    }

	    //      std::cout << "(" << edge.src << ", " << edge.tgt << ")" << std::endl;

	    this->addEdge(edge);
	  }

	  // Send out remaining edges.
	  this->insertionFinished();
	}

private:
  std::string file_;

  bool read_weights_;
};

// ALGORITHM_TEMPLATE_WEIGHTED_H(CSVEdgesProvider)

}

#endif //EVOLVING_GRAPHS_CSV_EDGES_PROVIDER_H
