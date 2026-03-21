#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace ipc {

static constexpr uint32_t kProtocolVersion = 1;
static constexpr size_t kAlignment = 8;

inline size_t AlignUp(size_t value, size_t alignment) {
  return (value + alignment - 1) & ~(alignment - 1);
}

enum class MessageType : uint32_t {
  Text = 1,
  Binary = 2,
  Command = 3,
  Heartbeat = 4,
};

struct MessageHeader {
  MessageType type;
  uint32_t payloadLength;
};

enum class SlotStatus : uint32_t {
  Empty = 0,
  Ready = 1,
  Padding = 2,
};

struct SlotHeader {
  std::atomic<uint32_t> status; // SlotStatus
  uint32_t slotSize;            // total aligned size of this slot
};

static_assert(std::atomic<uint32_t>::is_always_lock_free);
static_assert(std::atomic<uint64_t>::is_always_lock_free);

struct QueueMetadata {
  uint32_t protocolVersion;
  uint32_t capacity; // ring buffer data area size in bytes

  alignas(64) std::atomic<uint64_t> writeHead{0};
  alignas(64) std::atomic<uint64_t> readTail{0};
};

} // namespace ipc
