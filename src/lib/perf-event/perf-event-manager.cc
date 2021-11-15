// SPDX-License-Identifier: MIT

#include "perf-event/perf-event-manager.h"

namespace evolving_graphs {
namespace perf_event {

// PerfEventManager* PerfEventManager::instance = nullptr;

PerfEventManager::PerfEventManager() {
  int rc = new_event_rb_.create(SIZE_EVENT_RB,
                                L1D_CACHELINE_SIZE,
                                RING_BUFFER_BLOCKING,
                                nullptr,
                                nullptr);
  if (rc) {
    sg_log("Fail to initialize ringbuffer for events: %d\n", rc);
    evolving_graphs::util::die(1);
  }
}

PerfEventManager::~PerfEventManager() {
  new_event_rb_.destroy();
  for (auto t : threads_) {
    delete t;
  }
}

void PerfEventManager::start(const std::shared_ptr<Config> & config) {
  auto* t = new PerfEventCollector(new_event_rb_, config);
  t->start();
  t->setName("PECollector");
  threads_.push_back(t);
}

void PerfEventManager::stop() {
  // Push an element with a set shutdown flag to the collector to force it
  // to shutdown.
  // Just set the shutdown flag, everything else will be ignored by the
  // receiving collector.
  flatbuffers::FlatBufferBuilder builder;
  auto message = CreatePerfEventMessage(builder, true);
  builder.Finish(message);
  util::sendMessage(new_event_rb_, builder);

  for (auto& t : threads_) {
    t->join();
  }
}

Ringbuffer PerfEventManager::getRingBuffer() {
  return new_event_rb_;
}

// PerfEventManager* PerfEventManager::getInstance() {
//   if (PerfEventManager::instance == nullptr) {
//     PerfEventManager::instance = new PerfEventManager();
//   }
//   return PerfEventManager::instance;
// }
}
}
