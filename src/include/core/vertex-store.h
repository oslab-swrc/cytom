// SPDX-License-Identifier: MIT

#ifndef EVOLVING_GRAPHS_VERTEX_STORE_H
#define EVOLVING_GRAPHS_VERTEX_STORE_H

#pragma once

#include <cstring>
#include <iostream>
#include <memory>

#include "util/datatypes.h"
#include "util/util.h"

namespace evolving_graphs {

template<typename VertexType>
class VertexStore {
public:
  VertexStore():vertices_({}),
                current_id_(0),
                translations_(nullptr),
                vertex_count_(0)  {}

  void init(size_t vertex_count) {
    vertex_count_ = vertex_count;
    vertices_.degrees = new vertex_degree_t[vertex_count];
    vertices_.current = new VertexType[vertex_count];
    vertices_.next = new VertexType[vertex_count];

    vertices_.temp_next = new VertexType[vertex_count];

    auto size_active = static_cast<size_t>(size_bool_array(vertex_count));
    vertices_.active_current = new uint8_t[size_active];
    vertices_.active_next = new uint8_t[size_active];

    vertices_.changed = new uint8_t[size_active];

    vertices_.critical_neighbor = new uint32_t[vertex_count];

    memset(vertices_.changed,
           (unsigned char) 0,
           static_cast<size_t>(size_active * sizeof(uint8_t)));

    current_id_ = 0;
    translations_ = new vertex_id_t[vertex_count];

    memset(vertices_.degrees, 0, sizeof(vertex_degree_t) * vertex_count);
    for (size_t i = 0; i < vertex_count; ++i) {
      translations_[i] = UINT64_MAX;
      vertices_.critical_neighbor[i] = UINT32_MAX;
    }
  }

  inline void destroy() {
    delete[] vertices_.changed;
    delete[] vertices_.current;
    delete[] vertices_.next;
    delete[] vertices_.temp_next;
    delete[] vertices_.degrees;
    delete[] translations_;
  }

  inline void incrementInDegree(vertex_id_t id) {
    smp_faa(&vertices_.degrees[id].in_degree, 1);
  }

  inline void incrementOutDegree(vertex_id_t id) {
    smp_faa(&vertices_.degrees[id].out_degree, 1);
  }

  inline void decrementInDegree(vertex_id_t id) {
    smp_faa(&vertices_.degrees[id].in_degree, -1);
  }

  inline void decrementOutDegree(vertex_id_t id) {
    smp_faa(&vertices_.degrees[id].out_degree, -1);
  }

  inline void print() {
    for (size_t i = 0; i < vertex_count_; ++i) {
      std::cout << "Values at " << i << ": cur " << vertices_.current[i] << ", next "
                << vertices_.next[i] << ", active_cur "
                << eval_bool_array(vertices_.active_current, i) << ", active_next "
                << eval_bool_array(vertices_.active_next, i) << ", changed "
                << eval_bool_array(vertices_.changed, i) << std::endl;
    }
  }

  inline vertex_array_t<VertexType>* vertices() { return &vertices_; }

  vertex_id_t getTranslatedId(vertex_id_t original) {
    vertex_id_t translated_id = translations_[original];
    // If available, return translated id directly, otherwise create a new
    // mapping and ensure it's unique.
    if (translated_id != UINT64_MAX) {
      return translated_id;
    }

    translated_id = smp_faa(&current_id_, 1);
    bool
        successful = smp_cas(&translations_[original], UINT64_MAX, translated_id);
    // In case of collision, another thread already put an id for the original id, use that.
    if (!successful) {
      // Undo extra addition.
      smp_faa(&current_id_, -1);
      translated_id = translations_[original];
    }

    return translated_id;
  }

  vertex_array_t<VertexType> vertices_;

  inline size_t vertex_count() { return vertex_count_; }

private:
  size_t vertex_count_;

  vertex_id_t current_id_;

  vertex_id_t* translations_;
};

// template<typename VertexType>
// vertex_array_t<VertexType> VertexStore<VertexType>::vertices_ = {};

// template<typename VertexType>
// vertex_id_t VertexStore<VertexType>::current_id_ = 0;

// template<typename VertexType>
// vertex_id_t* VertexStore<VertexType>::translations_ = nullptr;

// template<typename VertexType>
// size_t VertexStore<VertexType>::vertex_count_ = 0;
// ALGORITHM_TEMPLATE_SINGLE_DATATYPE_H(VertexStore)

}

#endif //EVOLVING_GRAPHS_VERTEX_STORE_H
