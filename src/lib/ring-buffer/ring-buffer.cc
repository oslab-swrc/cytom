// SPDX-License-Identifier: MIT

#include <cstdio>

#include "ring-buffer/ring-buffer.h"

namespace evolving_graphs {

Ringbuffer::Ringbuffer() {
}

Ringbuffer::Ringbuffer(const Ringbuffer& other) {
  rb_ = other.rb_;
}

Ringbuffer::~Ringbuffer() {
}

int Ringbuffer::create(size_t size_hint,
                       size_t align,
                       bool is_blocking,
                       ring_buffer_reap_cb_t reap_cb,
                       void* reap_cb_arg) {
  return ring_buffer_create(size_hint,
                            align,
                            is_blocking ? RING_BUFFER_BLOCKING
                                        : RING_BUFFER_NON_BLOCKING,
                            reap_cb,
                            reap_cb_arg,
                            &rb_);
}

void Ringbuffer::init() {
  ring_buffer_init(rb_);
}

void Ringbuffer::destroy() {
  ring_buffer_destroy(rb_);
}

int Ringbuffer::put(ring_buffer_req_t* req) {
  return ring_buffer_put(rb_, req);
}

int Ringbuffer::get(ring_buffer_req_t* req) {
  return ring_buffer_get(rb_, req);
}

int Ringbuffer::putNoLock(ring_buffer_req_t* req) {
  return ring_buffer_put_nolock(rb_, req);
}

int Ringbuffer::getNoLock(ring_buffer_req_t* req) {
  return ring_buffer_get_nolock(rb_, req);
}

bool Ringbuffer::isFull() {
  return ring_buffer_is_full(rb_) == 1;
}

bool Ringbuffer::isEmpty() {
  return ring_buffer_is_empty(rb_) == 1;
}

size_t Ringbuffer::getFreeSpace() {
  return ring_buffer_free_space(rb_);
}

int Ringbuffer::copyToBuffer(void* destination, const void* source,
                             size_t num) {
  return copy_to_ring_buffer(rb_, destination, source, num);
}

int Ringbuffer::copyFromBuffer(void* destination, const void* source,
                               size_t num) {
  return copy_from_ring_buffer(rb_, destination, source, num);
}

void Ringbuffer::set_ready(ring_buffer_req_t* req) {
  ring_buffer_elm_set_ready(rb_, req->data);
}

void Ringbuffer::set_ready(void* data) {
  ring_buffer_elm_set_ready(rb_, data);
}

void Ringbuffer::set_done(ring_buffer_req_t* req) {
  ring_buffer_elm_set_done(rb_, req->data);
}

void Ringbuffer::set_done(void* data) {
  ring_buffer_elm_set_done(rb_, data);
}

ring_buffer_t* Ringbuffer::rb() {
  return rb_;
}

}
