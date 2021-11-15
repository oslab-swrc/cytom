#include "ring-buffer/ring-buffer.h"
#include "util/runnable.h"
#include "util/util.h"
#include <unistd.h>
#include "schema_generated.h"  // Already includes "flatbuffers/flatbuffers.h".
#include <flatbuffers/flatbuffers.h>

namespace eg = evolving_graphs;

class PushThread : public eg::util::Runnable {
public:
  explicit PushThread(const eg::Ringbuffer& rb) : rb_(rb) {
    printf("Ringbuffer: %p\n", rb_.rb());
  }

protected:
  void run() override {
    ring_buffer_req_t req{};
    for (int i = 0; i < 4; ++i) {
      flatbuffers::FlatBufferBuilder builder;

      eg::Edge edge_1(0, 0, 0.0);
      eg::Edge edge_2(1, 1, 0.0);
      std::vector<eg::Edge> edges_vector;
      edges_vector.push_back(edge_1);
      edges_vector.push_back(edge_2);

      auto edges = builder.CreateVectorOfStructs(edges_vector);

      auto message = eg::CreateEdgeInserterDataMessage(builder, i, edges);
      builder.Finish(message);
      uint32_t size = builder.GetSize();
      ring_buffer_put_req_init(&req, BLOCKING, size);
      rb_.put(&req);
      sg_rb_check(&req);
      printf("Input %lu bytes at position %p.\n", req.size, req.data);
      rb_.copyToBuffer(req.data, builder.GetBufferPointer(), size);
      rb_.set_ready(&req);
    }
  }

private:
  eg::Ringbuffer rb_;
};

class PullThread : public eg::util::Runnable {
public:
  explicit PullThread(const eg::Ringbuffer& rb) : rb_(rb) {
    printf("Ringbuffer: %p\n", rb_.rb());
  }

protected:
  void run() override {
    ring_buffer_req_t req{};
    for (int i = 0; i < 4; ++i) {
      ring_buffer_get_req_init(&req, BLOCKING);
      rb_.get(&req);
      auto message = flatbuffers::GetRoot<eg::EdgeInserterDataMessage>(req.data);
      printf("Got request for %lu bytes with data at %p: %d.\n",
             req.size,
             req.data,
             message->count());
      printf("Edges received: \n");
      for (const auto& edge : *message->edges()) {
        printf("Edge: %lu, %lu\n", edge->src(), edge->tgt());
      }
      rb_.set_done(&req);
      //      usleep(1000);
    }
  }

private:
  eg::Ringbuffer rb_;
};

int main(int /*argc*/, const char* /*argv*/ []) {
  eg::Ringbuffer rb;
  rb.create(4096, 64, true, nullptr, nullptr);
  rb.init();

  PushThread pt1(rb);
  PushThread pt2(rb);
  PullThread pr1(rb);
  PullThread pr2(rb);
  pr1.start();
  pr2.start();
  pt1.start();
  pt2.start();

  pr1.join();
  pr2.join();
  pt1.join();
  pt2.join();

  rb.destroy();
  printf("Ringbuffer destroyed, done!\n");
}
