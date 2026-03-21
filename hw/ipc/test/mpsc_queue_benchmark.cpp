#include "mpsc_queue.h"
#include "protocol.h"

#include <benchmark/benchmark.h>

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <latch>
#include <thread>
#include <vector>

namespace {

static constexpr size_t kQueueCapacity = 1 << 20; // 1 MB ring buffer
static constexpr size_t kTotalSize =
    sizeof(ipc::QueueMetadata) + kQueueCapacity;

class QueueBenchmarkFixture : public benchmark::Fixture {
public:
  void SetUp(benchmark::State &) override {
    m_buffer = static_cast<char *>(std::aligned_alloc(64, kTotalSize));
    m_queue = std::make_unique<ipc::MpscQueue>(m_buffer, kTotalSize, true);
  }

  void TearDown(benchmark::State &) override {
    m_queue.reset();
    std::free(m_buffer);
  }

  ipc::MpscQueue &Queue() { return *m_queue; }

private:
  char *m_buffer = nullptr;
  std::unique_ptr<ipc::MpscQueue> m_queue;
};

BENCHMARK_DEFINE_F(QueueBenchmarkFixture, SingleProducer)
(benchmark::State &state) {
  const size_t payloadSize = static_cast<size_t>(state.range(0));
  std::vector<char> payload(payloadSize, 'A');

  std::atomic<bool> running{true};
  std::atomic<int64_t> consumed{0};

  // Consumer thread
  std::jthread consumer([this, &running, &consumed] {
    while (running.load(std::memory_order_relaxed) ||
           Queue().TryPop().has_value()) {
      auto msg = Queue().TryPop();
      if (msg) {
        consumed.fetch_add(1, std::memory_order_relaxed);
      } else {
        std::this_thread::yield();
      }
    }
  });

  for (auto _ : state) {
    while (!Queue().TryPush(ipc::MessageType::Binary, payload.data(),
                            static_cast<uint32_t>(payloadSize))) {
      std::this_thread::yield();
    }
  }

  // Drain
  running.store(false, std::memory_order_relaxed);
  consumer.join();

  state.SetItemsProcessed(state.iterations());
  state.SetBytesProcessed(state.iterations() *
                          static_cast<int64_t>(payloadSize));
}

BENCHMARK_REGISTER_F(QueueBenchmarkFixture, SingleProducer)
    ->Arg(8)
    ->Arg(64)
    ->Arg(256)
    ->Arg(1024);

BENCHMARK_DEFINE_F(QueueBenchmarkFixture, MultiProducer)
(benchmark::State &state) {
  const int numProducers = static_cast<int>(state.range(0));
  const size_t payloadSize = 64;
  std::vector<char> payload(payloadSize, 'B');

  std::atomic<bool> running{true};

  // Consumer thread
  std::jthread consumer([this, &running] {
    while (running.load(std::memory_order_relaxed)) {
      if (!Queue().TryPop().has_value()) {
        std::this_thread::yield();
      }
    }
    // Drain remaining
    while (Queue().TryPop().has_value()) {
    }
  });

  // Producer threads
  std::atomic<int64_t> totalPushed{0};
  std::latch startLatch{numProducers};
  std::atomic<bool> producerRunning{true};

  std::vector<std::jthread> producers;
  for (int p = 0; p < numProducers; ++p) {
    producers.emplace_back(
        [this, &payload, payloadSize, &totalPushed, &startLatch, &producerRunning] {
          startLatch.arrive_and_wait();
          while (producerRunning.load(std::memory_order_relaxed)) {
            if (Queue().TryPush(ipc::MessageType::Binary, payload.data(),
                                static_cast<uint32_t>(payloadSize))) {
              totalPushed.fetch_add(1, std::memory_order_relaxed);
            } else {
              std::this_thread::yield();
            }
          }
        });
  }

  for (auto _ : state) {
    // Just measure time; producers are pushing concurrently
    benchmark::DoNotOptimize(totalPushed.load(std::memory_order_relaxed));
  }

  producerRunning.store(false, std::memory_order_relaxed);
  for (auto &t : producers) {
    t.join();
  }
  running.store(false, std::memory_order_relaxed);
  consumer.join();

  state.SetItemsProcessed(totalPushed.load());
}

BENCHMARK_REGISTER_F(QueueBenchmarkFixture, MultiProducer)
    ->Arg(1)
    ->Arg(2)
    ->Arg(4)
    ->Arg(8);

} // namespace

BENCHMARK_MAIN();
