// SPDX-License-Identifier: MIT

#include "perf-event/perf-event-collector.h"

#include "util/config.h"
#include "util/util.h"
#include "schema_generated.h"

namespace evolving_graphs {
namespace perf_event {

PerfEventCollector::PerfEventCollector(const Ringbuffer& new_event_rb, std::shared_ptr<Config> config)
    : new_event_rb_(new_event_rb), config_(std::move(config)) {
}

PerfEventCollector::~PerfEventCollector() = default;

void PerfEventCollector::run() {
  FILE* file = util::initFileProfilingData(config_->context_.path_to_perf_events);

  while (true) {
    ring_buffer_req_t req_event{};
    ring_buffer_get_req_init(&req_event, BLOCKING);
    new_event_rb_.get(&req_event);
    sg_rb_check(&req_event);

    auto message = flatbuffers::GetRoot<PerfEventMessage>(req_event.data);

    // Break from the loop if shutting down.
    if (message->shutdown()) {
      break;
    }

    if (message->duration() != nullptr) {
      util::writeProfilingDuration(message, file);
    } else if (message->ringbuffer_size() != nullptr) {
      util::writeRingBufferSizes(message, file);
    }

    new_event_rb_.set_done(&req_event);
  }

  fclose(file);
}
}
}
