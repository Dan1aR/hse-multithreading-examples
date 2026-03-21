#pragma once

#include "protocol.h"

#include <cstring>
#include <optional>
#include <vector>

namespace ipc {

class MpscQueue {
public:
  struct Message {
    MessageType type;
    std::vector<char> payload;
  };

  // Construct a queue view over a shared memory region.
  // If `initialize` is true, placement-new the metadata and zero the buffer.
  MpscQueue(void *shmBase, size_t totalSize, bool initialize)
      : m_meta(static_cast<QueueMetadata *>(shmBase)),
        m_ringBuffer(static_cast<char *>(shmBase) + sizeof(QueueMetadata)),
        m_capacity(totalSize - sizeof(QueueMetadata)) {
    if (totalSize <= sizeof(QueueMetadata)) {
      throw std::runtime_error("shared memory region too small for queue");
    }
    if (initialize) {
      new (m_meta) QueueMetadata{};
      m_meta->protocolVersion = kProtocolVersion;
      m_meta->capacity = static_cast<uint32_t>(m_capacity);
      std::memset(m_ringBuffer, 0, m_capacity);
    }
  }

  uint32_t ProtocolVersion() const { return m_meta->protocolVersion; }

  // Producer: try to enqueue a message. Returns false if queue is full.
  bool TryPush(MessageType type, const void *payload, uint32_t payloadLength) {
    const size_t dataSize = sizeof(SlotHeader) + sizeof(MessageHeader) + payloadLength;
    const size_t slotSize = AlignUp(dataSize, kAlignment);

    if (slotSize > m_capacity) {
      return false; // message too large for this queue
    }

    while (true) {
      uint64_t oldHead = m_meta->writeHead.load(std::memory_order_acquire);
      uint64_t tail = m_meta->readTail.load(std::memory_order_acquire);
      size_t ringOffset = oldHead % m_capacity;

      if (ringOffset + slotSize > m_capacity) {
        // Need padding to fill remainder, then write at offset 0
        size_t paddingSize = m_capacity - ringOffset;
        uint64_t newHead = oldHead + paddingSize + slotSize;

        if (newHead - tail > m_capacity) {
          return false; // full
        }

        if (!m_meta->writeHead.compare_exchange_weak(
                oldHead, newHead, std::memory_order_acq_rel)) {
          continue;
        }

        // Write padding slot
        WritePaddingSlot(ringOffset, paddingSize);

        // Write actual message at offset 0
        WriteMessageSlot(0, slotSize, type, payload, payloadLength);
        return true;
      }

      // Normal case: message fits without wrapping
      uint64_t newHead = oldHead + slotSize;

      if (newHead - tail > m_capacity) {
        return false; // full
      }

      if (!m_meta->writeHead.compare_exchange_weak(
              oldHead, newHead, std::memory_order_acq_rel)) {
        continue;
      }

      WriteMessageSlot(ringOffset, slotSize, type, payload, payloadLength);
      return true;
    }
  }

  // Consumer: try to dequeue the next message.
  std::optional<Message> TryPop() {
    while (true) {
      uint64_t tail = m_meta->readTail.load(std::memory_order_relaxed);
      uint64_t head = m_meta->writeHead.load(std::memory_order_acquire);

      if (tail >= head) {
        return std::nullopt; // empty
      }

      size_t ringOffset = tail % m_capacity;
      auto *slot = reinterpret_cast<SlotHeader *>(m_ringBuffer + ringOffset);
      auto status =
          static_cast<SlotStatus>(slot->status.load(std::memory_order_acquire));

      if (status == SlotStatus::Empty) {
        return std::nullopt; // producer hasn't finished writing yet
      }

      if (status == SlotStatus::Padding) {
        // Skip padding, advance tail to start of buffer
        uint32_t padSize = slot->slotSize;
        slot->status.store(static_cast<uint32_t>(SlotStatus::Empty),
                           std::memory_order_release);
        m_meta->readTail.store(tail + padSize, std::memory_order_release);
        continue;
      }

      // SlotStatus::Ready — read the message
      auto *msgHeader = reinterpret_cast<const MessageHeader *>(
          m_ringBuffer + ringOffset + sizeof(SlotHeader));

      Message msg;
      msg.type = msgHeader->type;
      msg.payload.resize(msgHeader->payloadLength);
      if (msgHeader->payloadLength > 0) {
        std::memcpy(msg.payload.data(),
                    m_ringBuffer + ringOffset + sizeof(SlotHeader) +
                        sizeof(MessageHeader),
                    msgHeader->payloadLength);
      }

      uint32_t totalSlotSize = slot->slotSize;
      slot->status.store(static_cast<uint32_t>(SlotStatus::Empty),
                         std::memory_order_release);
      m_meta->readTail.store(tail + totalSlotSize, std::memory_order_release);

      return msg;
    }
  }

  // Consumer: try to dequeue the next message matching the given type.
  // Non-matching messages are discarded.
  std::optional<Message> TryPop(MessageType filter) {
    while (true) {
      auto msg = TryPop();
      if (!msg.has_value()) {
        return std::nullopt;
      }
      if (msg->type == filter) {
        return msg;
      }
      // discard non-matching message, continue
    }
  }

private:
  void WritePaddingSlot(size_t ringOffset, size_t paddingSize) {
    auto *slot = reinterpret_cast<SlotHeader *>(m_ringBuffer + ringOffset);
    slot->slotSize = static_cast<uint32_t>(paddingSize);
    slot->status.store(static_cast<uint32_t>(SlotStatus::Padding),
                       std::memory_order_release);
  }

  void WriteMessageSlot(size_t ringOffset, size_t slotSize, MessageType type,
                        const void *payload, uint32_t payloadLength) {
    auto *slot = reinterpret_cast<SlotHeader *>(m_ringBuffer + ringOffset);
    slot->slotSize = static_cast<uint32_t>(slotSize);

    auto *msgHeader = reinterpret_cast<MessageHeader *>(
        m_ringBuffer + ringOffset + sizeof(SlotHeader));
    msgHeader->type = type;
    msgHeader->payloadLength = payloadLength;

    if (payloadLength > 0) {
      std::memcpy(m_ringBuffer + ringOffset + sizeof(SlotHeader) +
                      sizeof(MessageHeader),
                  payload, payloadLength);
    }

    // Signal that this slot is ready to be consumed
    slot->status.store(static_cast<uint32_t>(SlotStatus::Ready),
                       std::memory_order_release);
  }

  QueueMetadata *m_meta;
  char *m_ringBuffer;
  size_t m_capacity;
};

} // namespace ipc
