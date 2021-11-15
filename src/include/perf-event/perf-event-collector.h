// SPDX-License-Identifier: MIT

#pragma once

#include <memory>

#include "util/runnable.h"
#include "util/util.h"
#include "util/datatypes.h"
#include "util/config.h"
#include "ring-buffer/ring-buffer.h"


namespace evolving_graphs {
namespace perf_event {

class PerfEventCollector : public util::Runnable {
public:
  explicit PerfEventCollector(const Ringbuffer& new_event_rb, std::shared_ptr<Config> config);

  ~PerfEventCollector() override;

protected:
  void run() override;

private:
  Ringbuffer new_event_rb_;
  std::shared_ptr<Config> config_;
};
}
}
