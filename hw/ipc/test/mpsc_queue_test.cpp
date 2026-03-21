#include "mpsc_queue.h"
#include "protocol.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <latch>
#include <numeric>
#include <set>
#include <thread>
#include <vector>

namespace {

// Helper: allocate an aligned buffer and construct an MpscQueue on it.
class QueueTestFixture : public ::testing::Test {
protected:
  void SetUp(size_t capacity = 4096) {
    m_totalSize = sizeof(ipc::QueueMetadata) + capacity;
    m_buffer = static_cast<char *>(std::aligned_alloc(64, m_totalSize));
    ASSERT_NE(m_buffer, nullptr);
    m_queue = std::make_unique<ipc::MpscQueue>(m_buffer, m_totalSize, true);
  }

  void TearDown() override {
    m_queue.reset();
    std::free(m_buffer);
  }

  ipc::MpscQueue &Queue() { return *m_queue; }

  char *m_buffer = nullptr;
  size_t m_totalSize = 0;
  std::unique_ptr<ipc::MpscQueue> m_queue;
};

TEST_F(QueueTestFixture, BasicPushPop) {
  SetUp(4096);

  const char payload[] = "hello world";
  ASSERT_TRUE(
      Queue().TryPush(ipc::MessageType::Text, payload, sizeof(payload) - 1));

  auto msg = Queue().TryPop();
  ASSERT_TRUE(msg.has_value());
  EXPECT_EQ(msg->type, ipc::MessageType::Text);
  EXPECT_EQ(std::string_view(msg->payload.data(), msg->payload.size()),
            "hello world");
}

TEST_F(QueueTestFixture, QueueEmpty) {
  SetUp(4096);

  auto msg = Queue().TryPop();
  EXPECT_FALSE(msg.has_value());
}

TEST_F(QueueTestFixture, PushMultipleFIFO) {
  SetUp(4096);

  for (int i = 0; i < 10; ++i) {
    std::string data = std::to_string(i);
    ASSERT_TRUE(Queue().TryPush(ipc::MessageType::Text, data.data(),
                                static_cast<uint32_t>(data.size())));
  }

  for (int i = 0; i < 10; ++i) {
    auto msg = Queue().TryPop();
    ASSERT_TRUE(msg.has_value());
    EXPECT_EQ(std::string_view(msg->payload.data(), msg->payload.size()),
              std::to_string(i));
  }

  auto msg = Queue().TryPop();
  EXPECT_FALSE(msg.has_value());
}

TEST_F(QueueTestFixture, QueueFull) {
  // Small buffer: metadata overhead + very small ring buffer
  SetUp(128);

  // Fill the queue
  int pushed = 0;
  while (Queue().TryPush(ipc::MessageType::Text, "X", 1)) {
    ++pushed;
  }
  EXPECT_GT(pushed, 0);

  // Should fail now
  EXPECT_FALSE(Queue().TryPush(ipc::MessageType::Text, "Y", 1));

  // Pop one and push again
  auto msg = Queue().TryPop();
  ASSERT_TRUE(msg.has_value());
  EXPECT_TRUE(Queue().TryPush(ipc::MessageType::Text, "Z", 1));
}

TEST_F(QueueTestFixture, ProtocolVersion) {
  SetUp(4096);

  EXPECT_EQ(Queue().ProtocolVersion(), ipc::kProtocolVersion);
}

TEST_F(QueueTestFixture, TypeFilter) {
  SetUp(4096);

  Queue().TryPush(ipc::MessageType::Text, "text1", 5);
  Queue().TryPush(ipc::MessageType::Command, "cmd1", 4);
  Queue().TryPush(ipc::MessageType::Text, "text2", 5);

  // Filter for Command — should skip text1, return cmd1
  auto msg = Queue().TryPop(ipc::MessageType::Command);
  ASSERT_TRUE(msg.has_value());
  EXPECT_EQ(msg->type, ipc::MessageType::Command);
  EXPECT_EQ(std::string_view(msg->payload.data(), msg->payload.size()),
            "cmd1");
}

TEST_F(QueueTestFixture, VariableLengthMessages) {
  SetUp(8192);

  // Empty payload
  ASSERT_TRUE(Queue().TryPush(ipc::MessageType::Heartbeat, nullptr, 0));

  // Small payload
  ASSERT_TRUE(Queue().TryPush(ipc::MessageType::Text, "A", 1));

  // Medium payload
  std::string medium(200, 'B');
  ASSERT_TRUE(Queue().TryPush(ipc::MessageType::Binary, medium.data(),
                               static_cast<uint32_t>(medium.size())));

  // Verify
  auto msg1 = Queue().TryPop();
  ASSERT_TRUE(msg1.has_value());
  EXPECT_EQ(msg1->type, ipc::MessageType::Heartbeat);
  EXPECT_TRUE(msg1->payload.empty());

  auto msg2 = Queue().TryPop();
  ASSERT_TRUE(msg2.has_value());
  EXPECT_EQ(std::string_view(msg2->payload.data(), msg2->payload.size()), "A");

  auto msg3 = Queue().TryPop();
  ASSERT_TRUE(msg3.has_value());
  EXPECT_EQ(msg3->payload.size(), 200u);
  EXPECT_TRUE(std::all_of(msg3->payload.begin(), msg3->payload.end(),
                           [](char c) { return c == 'B'; }));
}

TEST_F(QueueTestFixture, Wraparound) {
  // Small capacity to force wraparound quickly
  SetUp(256);

  int totalPushed = 0;
  int totalPopped = 0;

  for (int round = 0; round < 50; ++round) {
    std::string data = "msg" + std::to_string(round);
    while (!Queue().TryPush(ipc::MessageType::Text, data.data(),
                            static_cast<uint32_t>(data.size()))) {
      auto msg = Queue().TryPop();
      ASSERT_TRUE(msg.has_value());
      ++totalPopped;
    }
    ++totalPushed;
  }

  // Drain remaining
  while (auto msg = Queue().TryPop()) {
    ++totalPopped;
  }

  EXPECT_EQ(totalPushed, totalPopped);
}

TEST_F(QueueTestFixture, MultiThreadedMPSC) {
  SetUp(65536);

  static constexpr int kNumProducers = 4;
  static constexpr int kMessagesPerProducer = 1000;
  static constexpr int kTotalMessages = kNumProducers * kMessagesPerProducer;

  std::latch startLatch{kNumProducers + 1};
  std::atomic<int> producersDone{0};

  // Producer threads
  std::vector<std::jthread> producers;
  for (int p = 0; p < kNumProducers; ++p) {
    producers.emplace_back([this, p, &startLatch] {
      startLatch.arrive_and_wait();

      for (int i = 0; i < kMessagesPerProducer; ++i) {
        // Encode producer id and message index in payload
        uint32_t tag[2] = {static_cast<uint32_t>(p), static_cast<uint32_t>(i)};
        while (!Queue().TryPush(ipc::MessageType::Binary, tag, sizeof(tag))) {
          std::this_thread::yield();
        }
      }
    });
  }

  // Consumer in main thread
  std::vector<std::set<int>> received(kNumProducers);
  int totalReceived = 0;

  startLatch.arrive_and_wait();

  while (totalReceived < kTotalMessages) {
    auto msg = Queue().TryPop();
    if (!msg) {
      std::this_thread::yield();
      continue;
    }

    ASSERT_EQ(msg->payload.size(), sizeof(uint32_t) * 2);
    uint32_t tag[2];
    std::memcpy(tag, msg->payload.data(), sizeof(tag));

    int producerId = static_cast<int>(tag[0]);
    int msgIndex = static_cast<int>(tag[1]);

    ASSERT_GE(producerId, 0);
    ASSERT_LT(producerId, kNumProducers);
    ASSERT_GE(msgIndex, 0);
    ASSERT_LT(msgIndex, kMessagesPerProducer);

    received[producerId].insert(msgIndex);
    ++totalReceived;
  }

  for (auto &t : producers) {
    t.join();
  }

  // Verify all messages received, no duplicates
  for (int p = 0; p < kNumProducers; ++p) {
    EXPECT_EQ(received[p].size(), static_cast<size_t>(kMessagesPerProducer))
        << "Producer " << p << " missing messages";
  }
}

TEST_F(QueueTestFixture, MultiThreadedStress) {
  SetUp(65536);

  static constexpr int kNumProducers = 8;
  static constexpr int kMessagesPerProducer = 5000;
  static constexpr int kTotalMessages = kNumProducers * kMessagesPerProducer;

  std::latch startLatch{kNumProducers + 1};

  std::vector<std::jthread> producers;
  for (int p = 0; p < kNumProducers; ++p) {
    producers.emplace_back([this, p, &startLatch] {
      startLatch.arrive_and_wait();

      for (int i = 0; i < kMessagesPerProducer; ++i) {
        uint32_t val = static_cast<uint32_t>(p * kMessagesPerProducer + i);
        while (!Queue().TryPush(ipc::MessageType::Binary, &val, sizeof(val))) {
          std::this_thread::yield();
        }
      }
    });
  }

  std::set<uint32_t> received;
  startLatch.arrive_and_wait();

  while (static_cast<int>(received.size()) < kTotalMessages) {
    auto msg = Queue().TryPop();
    if (!msg) {
      std::this_thread::yield();
      continue;
    }

    ASSERT_EQ(msg->payload.size(), sizeof(uint32_t));
    uint32_t val;
    std::memcpy(&val, msg->payload.data(), sizeof(val));
    received.insert(val);
  }

  for (auto &t : producers) {
    t.join();
  }

  EXPECT_EQ(received.size(), static_cast<size_t>(kTotalMessages));
}

} // namespace
