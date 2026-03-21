#pragma once

#include "mpsc_queue.h"
#include "shared_memory.h"

#include <stdexcept>
#include <string>

namespace ipc {

class ConsumerNode {
public:
  // Opens existing shared memory and validates protocol version.
  ConsumerNode(const char *path, size_t size)
      : m_shm(SharedMemoryRegion::Open(path, size)),
        m_queue(m_shm.Data(), m_shm.Size(), /*initialize=*/false) {
    if (m_queue.ProtocolVersion() != kProtocolVersion) {
      throw std::runtime_error{
          "Protocol version mismatch: expected " +
          std::to_string(kProtocolVersion) + ", got " +
          std::to_string(m_queue.ProtocolVersion())};
    }
  }

  std::optional<MpscQueue::Message> Receive() { return m_queue.TryPop(); }

  std::optional<MpscQueue::Message> Receive(MessageType filter) {
    return m_queue.TryPop(filter);
  }

private:
  SharedMemoryRegion m_shm;
  MpscQueue m_queue;
};

} // namespace ipc
