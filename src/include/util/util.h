// SPDX-License-Identifier: MIT

#pragma once

#include <cmath>
#include <string>
#include <sstream>
#include <vector>
#include <fstream>
#include <unordered_map>
#include <numeric>

#include <sys/time.h>
#include <time.h>

#include "util/datatypes.h"
#include "ring-buffer/ring-buffer.h"
#include "schema_generated.h"

// add this for type_MAX
#ifndef UINT64_MAX
#define UINT64_MAX __UINT64_MAX__
#endif
#ifndef UINT32_MAX
#define UINT32_MAX __UINT32_MAX__
#endif

namespace evolving_graphs {

#define sg_rb_check(__req)                                                     \
  if ((__req)->rc != 0) {                                                      \
    sg_dbg("Ringbuffer returned %d\n", (__req)->rc);                           \
    evolving_graphs::util::die(1);                                             \
  }

#define int_ceil(__val, __ceil_to) ((__val + __ceil_to - 1) & ~(__ceil_to - 1))

#define size_bool_array(__count) (std::ceil(__count / 8.0))

#define eval_bool_array(__array, __index)                                      \
  ((__array[__index / 8] >> (__index % 8)) & 1u)

#define set_bool_array(__array, __index, __val)                                \
  if (__val) {                                                                 \
    __array[__index / 8] |= 1u << (__index % 8);                                \
  } else {                                                                     \
    __array[__index / 8] &= ~(1u << (__index % 8));                             \
  }

inline void set_bool_array_atomically(uint8_t* array, uint64_t index, bool value) {
  bool successful;
  do {
    uint8_t old_value = array[index / 8];
    uint8_t new_value;
    if (value) {
      new_value = array[index / 8] | 1u << (index % 8);
    } else {
      new_value = array[index / 8] & ~(1u << (index % 8));
    }
    if (old_value != new_value) {
      successful = smp_cas(&array[index / 8], old_value, new_value);
    } else {
      successful = true;
    }
  } while (!successful);
}

#ifndef EVOLVING_GRAPHS_DEBUG
#define sg_assert(__cond, __msg)
#define sg_dbg(__fmt, ...)
#define sg_print(__fmt)
#define sg_here()
#else
#define sg_assert(__cond, __msg)                                               \
  if (!(__cond)) {                                                             \
    int* __p = NULL;                                                           \
    fprintf(stderr, "[SG-ASSERT:%s:%d] %s\n", __func__, __LINE__, __msg);      \
    evolving_graphs::util::die(__cond);                                        \
    *__p = 0;                                                                  \
  }
#define sg_print(__fmt)                                                        \
  fprintf(stdout, "[SG-PRT:%s:%d] " __fmt, __func__, __LINE__)
#define sg_dbg(__fmt, ...)                                                     \
  fprintf(stdout, "[SG-DBG:%s:%d] " __fmt, __func__, __LINE__, __VA_ARGS__)
#define sg_here() fprintf(stderr, "[SG-HERE:%s:%d] <== \n", __func__, __LINE__)
#endif /* EVOLVING_GRAPHS_DEBUG */

#define sg_log(__fmt, ...) fprintf(stderr, "[SG-LOG] " __fmt, __VA_ARGS__)
#define sg_log2(__fmt) fprintf(stderr, "[SG-LOG] " __fmt)

#define sg_err(__fmt, ...)                                                     \
  fprintf(stderr, "[SG-ERR:%s:%d] " __fmt, __func__, __LINE__, __VA_ARGS__)

namespace util {
std::vector<std::string> splitDirPaths(const std::string& s);

std::string prepareDirPath(const std::string& s);

void writeDataToFile(const std::string& file_name, const void* data,
                     size_t size);

void writeDataToFileSync(const std::string& file_name, const void* data,
                         size_t size);

void writeDataToFileDirectly(const std::string& file_name, const void* data,
                             size_t size);

void appendDataToFile(const std::string& file_name, const void* data,
                      size_t size);

void readDataFromFile(const std::string& file_name, size_t size, void* data);

void readDataFromFileDirectly(const std::string& file_name, size_t size,
                              void* data);

int openFileDirectly(const std::string& file_name);

void readFileOffset(int fd, void* buf, size_t count, size_t offset);

void writeFileOffset(int fd, void* buf, size_t count, size_t offset);

std::string getFilenameForProfilingData(const std::string& path_to_perf_events);

FILE* initFileProfilingData(const std::string& path_to_perf_events);

void writeProfilingDuration(const PerfEventMessage* message, FILE* file);

void writeRingBufferSizes(const PerfEventMessage* message, FILE* file);

void __die(int rc, const char* func, int line);

#define die(__rc) __die(__rc, __func__, __LINE__)

std::vector<int> splitToIntVector(std::string input);

void sendMessage(Ringbuffer& rb, flatbuffers::FlatBufferBuilder& builder);

template<typename K, typename V>
void writeMapToFile(const std::string& file_name,
                    const std::unordered_map<K, V>& map) {
  sg_dbg("Write map to %s\n", file_name.c_str());
  FILE* file = fopen(file_name.c_str(), "wb");
  if (!file) {
    sg_dbg("File %s couldn't be written!\n", file_name.c_str());
    evolving_graphs::util::die(1);
  }
  for (auto& it : map) {
    fwrite(&it.first, sizeof(K), 1, file);
    fwrite(&it.second, sizeof(V), 1, file);
  }
  fclose(file);
}

template<typename K, typename V>
void readMapFromFile(const std::string& file_name,
                     std::unordered_map<K, V>& map) {
  sg_dbg("Read %s\n", file_name.c_str());
  FILE* file = fopen(file_name.c_str(), "rb");
  if (!file) {
    sg_dbg("File %s couldn't be read!\n", file_name.c_str());
    evolving_graphs::util::die(1);
  }
  K key;
  // while there are more keys, read one key and value:
  while (fread(&key, sizeof(K), 1, file) == 1) {
    V value;
    if (fread(&value, sizeof(V), 1, file) != 1) {
      sg_dbg("Error reading value in %s!\n", file_name.c_str());
      evolving_graphs::util::die(1);
    }
    map[key] = value;
  }
  fclose(file);
}

// Hash strategy adopted from Ligra:
// https://github.com/jshun/ligra/blob/master/utils/rMatGraph.C
inline uint32_t hash(uint32_t a) {
  a = (a + 0x7ed55d16) + (a << 12);
  a = (a ^ 0xc761c23c) ^ (a >> 19);
  a = (a + 0x165667b1) + (a << 5);
  a = (a + 0xd3a2646c) ^ (a << 9);
  a = (a + 0xfd7046c5) + (a << 3);
  a = (a ^ 0xb55a4f09) ^ (a >> 16);
  return a;
}

inline uint64_t hash(uint64_t a) {
  a = (a + 0x7ed55d166bef7a1d) + (a << 12);
  a = (a ^ 0xc761c23c510fa2dd) ^ (a >> 9);
  a = (a + 0x165667b183a9c0e1) + (a << 59);
  a = (a + 0xd3a2646cab3487e3) ^ (a << 49);
  a = (a + 0xfd7046c5ef9ab54c) + (a << 3);
  a = (a ^ 0xb55a4f090dd4a67b) ^ (a >> 32);
  return a;
}

inline double hashDouble(uint64_t i) {
  return ((double) (hash((uint64_t) i)) / ((double) UINT64_MAX));
}

inline uint64_t get_time_usec() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (tv.tv_usec + 1000000 * tv.tv_sec);
}

inline uint64_t get_time_nsec() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (ts.tv_nsec + 1000000000 * ts.tv_sec);
}

template<typename T>
int log2Up(T i) {
  int a = 0;
  T b = i - 1;
  while (b > 0) {
    b = b >> 1;
    ++a;
  }
  return a;
}

// https://stackoverflow.com/a/17074810
template<typename T, typename Compare>
std::vector<std::size_t> sort_permutation(const std::vector<T>& vec, const Compare& compare) {
  std::vector<std::size_t> p(vec.size());
  std::iota(p.begin(), p.end(), 0);
  std::sort(p.begin(),
            p.end(),
            [&](std::size_t i, std::size_t j) { return compare(vec[i], vec[j]); });
  return p;
}

template<typename T>
std::vector<T> apply_permutation(const std::vector<T>& vec, const std::vector<std::size_t>& p) {
  std::vector<T> sorted_vec(vec.size());
  std::transform(p.begin(), p.end(), sorted_vec.begin(), [&](std::size_t i) { return vec[i]; });
  return sorted_vec;
}

template<typename T>
void apply_permutation_in_place(std::vector<T>& vec, const std::vector<std::size_t>& p) {
  std::vector<bool> done(vec.size());
  for (std::size_t i = 0; i < vec.size(); ++i) {
    if (done[i]) {
      continue;
    }
    done[i] = true;
    std::size_t prev_j = i;
    std::size_t j = p[i];
    while (i != j) {
      std::swap(vec[prev_j], vec[j]);
      done[j] = true;
      prev_j = j;
      j = p[j];
    }
  }
}

}
}

