// SPDX-License-Identifier: MIT

#ifndef EVOLVING_GRAPHS_RINGBUFFER_H
#define EVOLVING_GRAPHS_RINGBUFFER_H

#include "ring_buffer.h"

namespace evolving_graphs {

class Ringbuffer {
public:

  Ringbuffer();

  Ringbuffer(const Ringbuffer& other);

  ~Ringbuffer();

  int create(size_t size_hint, size_t align, bool is_blocking,
             ring_buffer_reap_cb_t reap_cb, void* reap_cb_arg);

  void init();

  void destroy();

  int put(ring_buffer_req_t* req);

  int get(ring_buffer_req_t* req);

  void set_ready(ring_buffer_req_t* req);

  void set_ready(void* data);

  void set_done(ring_buffer_req_t* req);

  void set_done(void* data);

  int putNoLock(ring_buffer_req_t* req);

  int getNoLock(ring_buffer_req_t* req);

  bool isFull();

  bool isEmpty();

  size_t getFreeSpace();

  int copyToBuffer(void* destination, const void* source, size_t num);

  int copyFromBuffer(void* destination, const void* source, size_t num);

  ring_buffer_t* rb();

private:
  ring_buffer_t* rb_;
};

}

#endif //EVOLVING_GRAPHS_RINGBUFFER_H
