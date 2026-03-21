#pragma once

#include "mpsc_queue.h"
#include "shared_memory.h"

#include <span>
#include <string_view>

namespace ipc {

class ProducerNode {
public:
  // Creates shared memory (or attaches if it already exists) and initializes the queue.
  ProducerNode(const char *path, size_t size)
      : m_shm(SharedMemoryRegion::Create(path, size)),
        m_queue(m_shm.Data(), m_shm.Size(), m_shm.IsCreator()) {}

  bool Send(MessageType type, std::span<const char> payload) {
    return m_queue.TryPush(type, payload.data(),
                           static_cast<uint32_t>(payload.size()));
  }

  bool SendText(std::string_view text) {
    return m_queue.TryPush(MessageType::Text, text.data(),
                           static_cast<uint32_t>(text.size()));
  }

  bool SendCommand(std::string_view cmd) {
    return m_queue.TryPush(MessageType::Command, cmd.data(),
                           static_cast<uint32_t>(cmd.size()));
  }

  void Unlink() { m_shm.Unlink(); }

private:
  SharedMemoryRegion m_shm;
  MpscQueue m_queue;
};

} // namespace ipc
