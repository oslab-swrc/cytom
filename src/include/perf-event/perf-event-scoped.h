// SPDX-License-Identifier: MIT

#pragma once

#include "util/datatypes.h"
#include "ring-buffer/ring-buffer.h"

namespace evolving_graphs {
namespace perf_event {

class PerfEventScoped {
public:
  PerfEventScoped(const Ringbuffer& new_event_rb, const std::string& name,
                  bool active, const std::string& metadata = "");

  PerfEventScoped(const Ringbuffer& new_event_rb, const std::string& name,
                  const std::string& component, uint32_t thread_id,
                  bool active, const std::string& metadata = "");

  ~PerfEventScoped();

private:
  bool active_;
  
  Ringbuffer new_event_rb_;

  std::string name_;

  std::string component_;

  std::string metadata_;

  uint32_t thread_id_;

  profiling_duration_t duration_;
};
}
}
