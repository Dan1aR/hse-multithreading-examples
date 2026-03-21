#include "condition_variable.h"

#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <atomic>
#include <limits>

namespace hw::futex {

namespace {

int FutexWait(int* value, const int expectedValue) {
    return static_cast<int>(
        syscall(SYS_futex, value, FUTEX_WAIT_PRIVATE, expectedValue, nullptr, nullptr, 0));
}

int FutexWake(int* value, const int count) {
    return static_cast<int>(
        syscall(SYS_futex, value, FUTEX_WAKE_PRIVATE, count, nullptr, nullptr, 0));
}

}  // namespace

void FutexConditionVariable::notify_one() {
    std::atomic_ref<int> seqRef(seq_);
    seqRef.fetch_add(1, std::memory_order_relaxed);
    FutexWake(&seq_, 1);
}

void FutexConditionVariable::notify_all() {
    std::atomic_ref<int> seqRef(seq_);
    seqRef.fetch_add(1, std::memory_order_relaxed);
    FutexWake(&seq_, std::numeric_limits<int>::max());
}

void FutexConditionVariable::wait(std::unique_lock<std::mutex>& lock) {
    std::atomic_ref<int> seqRef(seq_);
    const int expectedValue = seqRef.load(std::memory_order_relaxed);

    lock.unlock();
    FutexWait(&seq_, expectedValue);
    lock.lock();
}

}  // namespace hw::futex
