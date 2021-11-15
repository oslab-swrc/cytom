// SPDX-License-Identifier: MIT

#include "perf-event/perf-event-scoped.h"

#include <pthread.h>

#include "util/util.h"
#include "schema_generated.h"

namespace evolving_graphs {
namespace perf_event {

PerfEventScoped::PerfEventScoped(const Ringbuffer& new_event_rb,
                                 const std::string& name, bool active,
                                 const std::string& metadata)
    : active_(active), new_event_rb_(new_event_rb) {
  if (active) {
    duration_.time_start = util::get_time_nsec();
    component_ = "None";
    thread_id_ = static_cast<uint32_t>(pthread_self());
    name_ = name;
    metadata_ = metadata;
  }
}

PerfEventScoped::PerfEventScoped(const Ringbuffer& new_event_rb,
                                 const std::string& name,
                                 const std::string& component,
                                 uint32_t thread_id, bool active,
                                 const std::string& metadata)
    : active_(active), new_event_rb_(new_event_rb) {
  if (active) {
    duration_.time_start = util::get_time_nsec();
    name_ = name;
    component_ = component;
    thread_id_ = thread_id;
    metadata_ = metadata;
  }
}

PerfEventScoped::~PerfEventScoped() {
  if (active_) {
    duration_.time_end = util::get_time_nsec();

    flatbuffers::FlatBufferBuilder builder;
    auto duration = CreateProfilingDuration(builder,
                                            duration_.time_start,
                                            duration_.time_end);

    auto message = CreatePerfEventMessage(builder,
                                          false,
                                          PerfEventType_Duration,
                                          duration,
                                          0,
                                          thread_id_,
                                          builder.CreateString(component_),
                                          builder.CreateString(metadata_),
                                          builder.CreateString(name_));
    builder.Finish(message);
    util::sendMessage(new_event_rb_, builder);
  }
}
}
}
