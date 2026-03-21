#pragma once

#include <utils/error.hpp>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>

namespace ipc {

class SharedMemoryRegion {
public:
  static SharedMemoryRegion Create(const char *path, size_t size) {
    SharedMemoryRegion region;
    region.m_path = path;
    region.m_size = size;
    region.m_isCreator = true;

    region.m_fd = shm_open(path, O_CREAT | O_EXCL | O_RDWR, 0666);
    if (region.m_fd == -1) {
      if (errno == EEXIST) {
        // Already exists — attach as non-creator
        return Open(path, size);
      }
      throw std::runtime_error{
          std::string("shm_open create failed: ") + strerror(errno)};
    }

    if (ftruncate(region.m_fd, static_cast<off_t>(size)) == -1) {
      close(region.m_fd);
      shm_unlink(path);
      throw std::runtime_error{
          std::string("ftruncate failed: ") + strerror(errno)};
    }

    region.m_ptr =
        mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, region.m_fd, 0);
    if (region.m_ptr == MAP_FAILED) {
      close(region.m_fd);
      shm_unlink(path);
      throw std::runtime_error{
          std::string("mmap failed: ") + strerror(errno)};
    }

    std::memset(region.m_ptr, 0, size);
    return region;
  }

  static SharedMemoryRegion Open(const char *path, size_t size) {
    SharedMemoryRegion region;
    region.m_path = path;
    region.m_size = size;
    region.m_isCreator = false;

    region.m_fd = shm_open(path, O_RDWR, 0666);
    if (region.m_fd == -1) {
      throw std::runtime_error{
          std::string("shm_open failed: ") + strerror(errno)};
    }

    region.m_ptr =
        mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, region.m_fd, 0);
    if (region.m_ptr == MAP_FAILED) {
      close(region.m_fd);
      throw std::runtime_error{
          std::string("mmap failed: ") + strerror(errno)};
    }

    return region;
  }

  SharedMemoryRegion() = default;

  ~SharedMemoryRegion() {
    if (m_ptr && m_ptr != MAP_FAILED) {
      munmap(m_ptr, m_size);
    }
    if (m_fd != -1) {
      close(m_fd);
    }
  }

  SharedMemoryRegion(SharedMemoryRegion &&other) noexcept
      : m_ptr(std::exchange(other.m_ptr, nullptr)),
        m_size(std::exchange(other.m_size, 0)),
        m_fd(std::exchange(other.m_fd, -1)),
        m_path(std::move(other.m_path)),
        m_isCreator(std::exchange(other.m_isCreator, false)) {}

  SharedMemoryRegion &operator=(SharedMemoryRegion &&other) noexcept {
    if (this != &other) {
      if (m_ptr && m_ptr != MAP_FAILED) {
        munmap(m_ptr, m_size);
      }
      if (m_fd != -1) {
        close(m_fd);
      }
      m_ptr = std::exchange(other.m_ptr, nullptr);
      m_size = std::exchange(other.m_size, 0);
      m_fd = std::exchange(other.m_fd, -1);
      m_path = std::move(other.m_path);
      m_isCreator = std::exchange(other.m_isCreator, false);
    }
    return *this;
  }

  SharedMemoryRegion(const SharedMemoryRegion &) = delete;
  SharedMemoryRegion &operator=(const SharedMemoryRegion &) = delete;

  void *Data() { return m_ptr; }
  const void *Data() const { return m_ptr; }
  size_t Size() const { return m_size; }
  bool IsCreator() const { return m_isCreator; }

  void Unlink() {
    if (!m_path.empty()) {
      shm_unlink(m_path.c_str());
    }
  }

private:
  void *m_ptr = nullptr;
  size_t m_size = 0;
  int m_fd = -1;
  std::string m_path;
  bool m_isCreator = false;
};

} // namespace ipc
