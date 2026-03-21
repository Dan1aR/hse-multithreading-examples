#include "condition_variable.h"

#include <gtest/gtest.h>

#include <atomic>
#include <latch>
#include <mutex>
#include <thread>
#include <type_traits>
#include <vector>

TEST(FutexConditionVariable, HasBasicApiShape) {
    static_assert(!std::is_copy_constructible_v<hw::futex::FutexConditionVariable>);
    static_assert(!std::is_copy_assignable_v<hw::futex::FutexConditionVariable>);

    [[maybe_unused]] hw::futex::FutexConditionVariable conditionVariable;
    SUCCEED();
}

TEST(FutexConditionVariable, NotifyOneLetsWaitingThreadFinish) {
    hw::futex::FutexConditionVariable conditionVariable;
    std::mutex mutex;
    std::latch waiterStarted{1};
    bool ready = false;
    bool finished = false;

    std::jthread waiter([&] {
        std::unique_lock lock(mutex);
        waiterStarted.count_down();

        while (!ready) {
            conditionVariable.wait(lock);
        }

        finished = true;
    });

    waiterStarted.wait();

    {
        std::lock_guard lock(mutex);
        ready = true;
    }

    conditionVariable.notify_one();
    waiter.join();

    EXPECT_TRUE(finished);
}

TEST(FutexConditionVariable, NotifyAllLetsAllWaitingThreadsFinish) {
    static constexpr int kWaiters = 4;

    hw::futex::FutexConditionVariable conditionVariable;
    std::mutex mutex;
    std::latch waitersStarted{kWaiters};
    bool ready = false;
    std::atomic<int> finished{0};
    std::vector<std::jthread> waiters;

    waiters.reserve(kWaiters);
    for (int i = 0; i < kWaiters; ++i) {
        waiters.emplace_back([&] {
            std::unique_lock lock(mutex);
            waitersStarted.count_down();

            while (!ready) {
                conditionVariable.wait(lock);
            }

            ++finished;
        });
    }

    waitersStarted.wait();

    {
        std::lock_guard lock(mutex);
        ready = true;
    }

    conditionVariable.notify_all();

    for (auto& waiter : waiters) {
        waiter.join();
    }

    EXPECT_EQ(finished.load(), kWaiters);
}

TEST(FutexConditionVariable, WaitWithPredicateReturnsWhenConditionBecomesTrue) {
    hw::futex::FutexConditionVariable conditionVariable;
    std::mutex mutex;
    std::latch waiterStarted{1};
    bool ready = false;
    bool finished = false;

    std::jthread waiter([&] {
        std::unique_lock lock(mutex);
        waiterStarted.count_down();

        conditionVariable.wait(lock, [&] { return ready; });
        finished = true;
    });

    waiterStarted.wait();

    {
        std::lock_guard lock(mutex);
        ready = true;
    }

    conditionVariable.notify_one();
    waiter.join();

    EXPECT_TRUE(finished);
}
