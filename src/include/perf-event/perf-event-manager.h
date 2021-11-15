// SPDX-License-Identifier: MIT

#pragma once

#include <vector>
#include <memory>

#include "util/runnable.h"
#include "util/util.h"
#include "util/datatypes.h"
#include "util/datatypes_config.h"
#include "util/config.h"
#include "perf-event/perf-event-collector.h"
#include "ring-buffer/ring-buffer.h"

namespace evolving_graphs {
namespace perf_event {

class PerfEventManager {
public:
  PerfEventManager();

  ~PerfEventManager();

  void start(const std::shared_ptr<Config> & config);

  void stop();

  Ringbuffer getRingBuffer();

// public:
//   static PerfEventManager* getInstance();

private:
  // Instance for the singleton, is initialized to nullptr.
  // static PerfEventManager* instance;

  Ringbuffer new_event_rb_;

  std::vector<PerfEventCollector*> threads_;

  static const int SIZE_EVENT_RB = 1 * GB;
};
}
}
