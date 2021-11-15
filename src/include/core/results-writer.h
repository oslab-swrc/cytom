// SPDX-License-Identifier: MIT

#ifndef EVOLVING_GRAPHS_RESULTS_WRITER_H
#define EVOLVING_GRAPHS_RESULTS_WRITER_H

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <memory>

#include "util/runnable.h"
#include "util/util.h"
#include "util/config.h"
#include "core/vertex-store.h"
#include "perf-event/perf-event-collector.h"

namespace evolving_graphs {

using namespace perf_event;

template<typename VertexType, class Algorithm, bool is_weighted>
class ResultsWriter : public util::Runnable {
public:
  	ResultsWriter(std::shared_ptr<Config> config, std::shared_ptr<VertexStore<VertexType> > vertex_store) : 
  		shutdown_(false), config_(std::move(config)), vertex_store_(std::move(vertex_store)){
	  barrier_ = new pthread_barrier_t;
	  pthread_barrier_init(barrier_, nullptr, 2);
	}

  	void notifyAndWait() {
	  // Wait once for notifying.
	  pthread_barrier_wait(barrier_);

	  // Wait second time for completion.
	  pthread_barrier_wait(barrier_);
	}

  	void shutdown() {
	  shutdown_ = true;
	  pthread_barrier_wait(barrier_);
	}

protected:
  void run() override {
	  uint64_t count = 0;
	  while (true) {
	    pthread_barrier_wait(barrier_);
	    if (shutdown_) {
	      break;
	    }

	    std::string filename = config_->context_.path_to_results + "results_" + std::to_string(count);

	    int fd = open(filename.c_str(), O_WRONLY | O_CREAT | O_SYNC, 0755);
	    if (fd == -1) {
	      sg_err("Unable to open file %s: %s\n", filename.c_str(), strerror(errno));
	      util::die(1);
	    }

	    // Write the current output array.
	    size_t size_to_write = sizeof(VertexType) * vertex_store_->vertex_count();
	    if (write(fd, vertex_store_->vertices_.next, size_to_write) != size_to_write) {
	      sg_err("Fail to write to file %s: %s\n", filename.c_str(), strerror(errno));
	      util::die(1);
	    }

	    // Done writing, close file.
	    int err = close(fd);
	    if (err != 0) {
	      sg_err("File %s couldn't be written: %s\n", filename.c_str(), strerror(errno));
	      util::die(1);
	    }

	    ++count;

	    // Notify of completion.
	    pthread_barrier_wait(barrier_);
	  }
	}

private:
  pthread_barrier_t* barrier_;

  bool shutdown_;

  std::shared_ptr<Config> config_;

  std::shared_ptr<VertexStore<VertexType> > vertex_store_;
};

// ALGORITHM_TEMPLATE_WEIGHTED_H(ResultsWriter)
}

#endif //EVOLVING_GRAPHS_RESULTS_WRITER_H
