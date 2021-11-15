// SPDX-License-Identifier: MIT

#include "util/util.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <fstream>

#include "util/datatypes.h"
#include "ring_buffer_i.h"

namespace evolving_graphs {
namespace util {
void __die(int rc, const char* func, int line) {
  int* p = NULL;

  fprintf(stderr, "\033[91m");
  fprintf(stderr, "XXX [%s:%d] error exit with %d\n", func, line, rc);
  //    fprintf(stderr, "\033[92m");
  //    rb_print_stack_trace();
  //    fprintf(stderr, "\033[0m");

  assert(0);
  *p = rc;
}

std::vector<int> splitToIntVector(std::string input) {
  std::vector<int> list;
  std::stringstream ss(input);
  std::string element;
  int number;

  while (std::getline(ss, element, ':')) {
    number = std::stoi(element);
    list.push_back(number);
  }
  return list;
}

std::vector<std::string> splitDirPaths(const std::string& s) {
  std::vector<std::string> path_list;
  std::stringstream ss(s);
  std::string path;

  while (std::getline(ss, path, ':')) {
    path = prepareDirPath(path);
    path_list.push_back(path);
  }

  return path_list;
}

std::string prepareDirPath(const std::string& s) {
  std::string path = s;
  if (path.back() != '/') {
    path += "/";
  }
  return path;
}

void writeDataToFile(const std::string& file_name, const void* data,
                     size_t size) {
  sg_dbg("Write data to %s\n", file_name.c_str());

  FILE* file = fopen(file_name.c_str(), "w");
  if (!file) {
    sg_err("File %s couldn't be written: %s\n", file_name.c_str(),
           strerror(errno));
    die(1);
  }
  if (fwrite(data, 1, size, file) != size) {
    sg_err("Fail to write file %s: %s\n", file_name.c_str(), strerror(errno));
    die(1);
  }
  fclose(file);
}

void writeDataToFileSync(const std::string& file_name, const void* data,
                         size_t size) {
  sg_dbg("Write data to %s\n", file_name.c_str());

  int fd = open(file_name.c_str(), O_WRONLY | O_CREAT | O_SYNC, 755);
  if (fd < 0) {
    sg_err("File %s couldn't be written: %s\n", file_name.c_str(),
           strerror(errno));
    die(1);
  }
  if (write(fd, data, size) != size) {
    sg_err("Fail to write to file %s: %s\n", file_name.c_str(),
           strerror(errno));
    die(1);
  }
  close(fd);
}

void writeDataToFileDirectly(const std::string& file_name, const void* data,
                             size_t size) {
  sg_dbg("Write data to %s\n", file_name.c_str());

  int fd = open(file_name.c_str(), O_WRONLY | O_CREAT | O_DIRECT, 755);
  if (fd < 0) {
    sg_err("File %s couldn't be written: %s\n", file_name.c_str(),
           strerror(errno));
    die(1);
  }
  if (write(fd, data, size) != size) {
    sg_err("Fail to write to file %s: %s\n", file_name.c_str(),
           strerror(errno));
    die(1);
  }
  close(fd);
}

void appendDataToFile(const std::string& file_name, const void* data,
                      size_t size) {
  sg_dbg("Append data to %s\n", file_name.c_str());

  FILE* file = fopen(file_name.c_str(), "a");
  if (!file) {
    sg_err("File %s couldn't be written: %s\n", file_name.c_str(),
           strerror(errno));
    die(1);
  }

  size_t ret_size = fwrite(data, 1, size, file);
  if (ret_size != size) {
    sg_err("Fail to read file %s: %s [ret_size: %lu vs req_size: %lu]\n",
           file_name.c_str(), strerror(errno), ret_size, size);
    die(1);
  }
  fclose(file);
}

void readDataFromFile(const std::string& file_name, size_t size, void* data) {
  sg_dbg("Read: %s\n", file_name.c_str());

  FILE* file = fopen(file_name.c_str(), "r");
  if (!file) {
    sg_err("Unable to open file %s: %s\n", file_name.c_str(),
           strerror(errno));
    die(1);
  }

  size_t ret_size = fread(data, 1, size, file);
  if (ret_size != size) {
    sg_err("Fail to read file %s: %s [ret_size: %lu vs req_size: %lu]\n",
           file_name.c_str(), strerror(errno), ret_size, size);
    die(1);
  }
  fclose(file);
}

void readDataFromFileDirectly(const std::string& file_name, size_t size,
                              void* data) {
  sg_dbg("Read(O_DIRECT): %s\n", file_name.c_str());

  int fd = open(file_name.c_str(), O_RDONLY | O_DIRECT);
  if (fd == -1) {
    sg_err("Unable to open file %s: %s\n", file_name.c_str(),
           strerror(errno));
    die(1);
  }
  if (read(fd, data, size) != size) {
    sg_err("Fail to read file %s(size: %lu , data: %p): %s\n",
           file_name.c_str(), size, data, strerror(errno));
    die(1);
  }
  close(fd);
}

int openFileDirectly(const std::string& file_name) {
  int fd = open(file_name.c_str(), O_RDONLY | O_DIRECT);
  if (fd == -1) {
    sg_err("Unable to open file %s: %s\n", file_name.c_str(),
           strerror(errno));
    die(1);
  }
  return fd;
}

void readFileOffset(int fd, void* buf, size_t count, size_t offset) {
  size_t bytes_read = pread(fd, buf, count, offset);
  if (bytes_read != count) {
    sg_err(
        "Error while reading %d, only read %lu bytes of %lu at %lu: %s %d\n",
        fd, bytes_read, count, offset, strerror(errno), errno);
    util::die(1);
  }
}

void writeFileOffset(int fd, void* buf, size_t count, size_t offset) {
  size_t bytes_written = pwrite(fd, buf, count, offset);

  if (bytes_written != count) {
    sg_err("Error while writing %d, only wrote %lu bytes at %lu: %s %d\n", fd,
           bytes_written, offset, strerror(errno), errno);
    util::die(1);
  }
}

void sendMessage(Ringbuffer& rb, flatbuffers::FlatBufferBuilder& builder) {
  uint32_t size = builder.GetSize();
  ring_buffer_req_t req{};
  ring_buffer_put_req_init(&req, BLOCKING, size);
  rb.put(&req);
  sg_rb_check(&req);
  rb.copyToBuffer(req.data, builder.GetBufferPointer(), size);
  rb.set_ready(&req);
}

std::string getFilenameForProfilingData(
    const std::string& path_to_perf_events) {
  std::string filename = path_to_perf_events;
  filename += "profile";
  return filename;
}

FILE* initFileProfilingData(const std::string& path_to_perf_events) {
  std::string file_name = getFilenameForProfilingData(path_to_perf_events);

  FILE* file = fopen(file_name.c_str(), "w");
  if (!file) {
    sg_dbg("File %s couldn't be written!\n", file_name.c_str());
    evolving_graphs::util::die(1);
  }

  fwrite("[", 1, 1, file);

  return file;
}

std::string getThreadId(const profiling_data_t& data) {
  // First use the Component name, then append global id and, if necessary,
  // the local id, i.e.: VertexFetcher_2_1
  std::string tid;

  switch (data.component) {
    case ComponentType::CT_GlobalReducer:
      tid += "GlobalReducer";
      break;
    case ComponentType::CT_IndexReader:
      tid += "IndexReader";
      break;
    case ComponentType::CT_None:
      break;
    case ComponentType::CT_RingBufferSizes:
      tid += "RingBufferSizes";
      break;
    case ComponentType::CT_TileProcessor:
      tid += "TileProcessor";
      break;
    case ComponentType::CT_TileReader:
      tid += "TileReader";
      break;
    case ComponentType::CT_VertexApplier:
      tid += "VertexApplier";
      break;
    case ComponentType::CT_VertexFetcher:
      tid += "VertexFetcher";
      break;
    case ComponentType::CT_VertexReducer:
      tid += "VertexReducer";
      break;
    default:
      break;
  }

  tid += "_" + std::to_string(data.global_id);

  // In case of the VertexFetcher or VertexReducer, append the local_id as
  // well.
  if (data.component == ComponentType::CT_IndexReader ||
      data.component == ComponentType::CT_VertexFetcher ||
      data.component == ComponentType::CT_VertexReducer ||
      data.component == ComponentType::CT_TileReader ||
      data.component == ComponentType::CT_TileProcessor) {
    tid += "_" + std::to_string(data.local_id);
  }

  return tid;
}

void writeProfilingDuration(const PerfEventMessage* message, FILE* file) {
  std::string thread_id =
      message->component()->str() + std::to_string(message->thread_id());

  double time_start_usec = message->duration()->start() / (double) 1000;
  double time_end_usec = message->duration()->end() / (double) 1000;

  // Print first event ('B').
  fprintf(file,
          "{\"tid\": \"%s\",\"ts\": %f,\"pid\": %d, \"name\": \"%s\", "
          "\"ph\": \"B\", \"args\": { \"metadata\": \"%s\" }},\n",
          thread_id.c_str(),
          time_start_usec,
          message->pid(),
          message->name()->c_str(),
          message->metadata()->c_str());
  // Print end event ('E').
  fprintf(file, "{\"tid\": \"%s\",\"ts\": %f,\"pid\": %d, \"name\": \"%s\", "
                "\"ph\": \"E\", \"args\": { \"metadata\": \"%s\" }},\n",
          thread_id.c_str(),
          time_end_usec,
          message->pid(),
          message->name()->c_str(),
          message->metadata()->c_str());
}

void writeRingBufferSizes(const PerfEventMessage* message, FILE* file) {
  std::string name = message->name()->str();

  double timestamp_usec = message->ringbuffer_size()->time() / (double) 1000;

  fprintf(file, "{\"ts\": %f,\"pid\": %d, \"name\": \"%s_index\", "
                "\"ph\": \"C\", \"args\": {\"index_rb\": %lu}},\n",
          timestamp_usec, message->pid(), name.c_str(),
          message->ringbuffer_size()->size_distributor());
}
}
}
