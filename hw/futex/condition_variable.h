#pragma once

#include <atomic>
#include <mutex>

namespace hw::futex {

class FutexConditionVariable {
public:
    FutexConditionVariable() = default;
    FutexConditionVariable(const FutexConditionVariable&) = delete;
    FutexConditionVariable& operator=(const FutexConditionVariable&) = delete;
    FutexConditionVariable(FutexConditionVariable&&) = delete;
    FutexConditionVariable& operator=(FutexConditionVariable&&) = delete;
    ~FutexConditionVariable() = default;

    void notify_one();
    void notify_all();
    void wait(std::unique_lock<std::mutex>& lock);

    template <class Predicate>
    void wait(std::unique_lock<std::mutex>& lock, Predicate stop_waiting) {
        while (!stop_waiting()) {
            wait(lock);
        }
    }

private:
    alignas(std::atomic_ref<int>::required_alignment) int seq_{0};
};

}  // namespace hw::futex
