#include "thread_pool.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <numeric>
#include <stdexcept>
#include <vector>

TEST(ThreadPool, SubmitSingleTask) {
    hw::future::ThreadPool pool(2);
    auto future = pool.Submit([] { return 42; });
    EXPECT_EQ(future.Get(), 42);
}

TEST(ThreadPool, SubmitVoidTask) {
    hw::future::ThreadPool pool(2);
    std::atomic<bool> executed{false};
    auto future = pool.Submit([&] { executed.store(true); });
    future.Get();
    EXPECT_TRUE(executed.load());
}

TEST(ThreadPool, SubmitMultipleTasks) {
    hw::future::ThreadPool pool(4);
    std::vector<hw::future::Future<int>> futures;

    for (int i = 0; i < 100; ++i) {
        futures.push_back(pool.Submit([i] { return i * i; }));
    }

    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(futures[i].Get(), i * i);
    }
}

TEST(ThreadPool, TaskThrowsException) {
    hw::future::ThreadPool pool(2);
    auto future = pool.Submit([]() -> int { throw std::runtime_error("task error"); });
    EXPECT_THROW(future.Get(), std::runtime_error);
}

TEST(ThreadPool, ConcurrentExecution) {
    hw::future::ThreadPool pool(4);

    auto start = std::chrono::steady_clock::now();

    std::vector<hw::future::Future<int>> futures;
    for (int i = 0; i < 4; ++i) {
        futures.push_back(pool.Submit([i] {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            return i;
        }));
    }

    for (auto& f : futures) {
        f.Get();
    }

    auto elapsed = std::chrono::steady_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    // 4 tasks of 100ms each on 4 threads should take ~100ms, not ~400ms
    EXPECT_LT(ms, 300);
}

TEST(ThreadPool, DestructorCompletesAllTasks) {
    std::atomic<int> counter{0};

    {
        hw::future::ThreadPool pool(2);
        for (int i = 0; i < 10; ++i) {
            pool.Submit([&counter] { counter.fetch_add(1); });
        }
    }

    EXPECT_EQ(counter.load(), 10);
}

TEST(ThreadPool, StressTest) {
    hw::future::ThreadPool pool(8);
    constexpr int kTasks = 1000;

    std::vector<hw::future::Future<int>> futures;
    futures.reserve(kTasks);

    for (int i = 0; i < kTasks; ++i) {
        futures.push_back(pool.Submit([i] { return i; }));
    }

    long long sum = 0;
    for (auto& f : futures) {
        sum += f.Get();
    }

    long long expected = static_cast<long long>(kTasks - 1) * kTasks / 2;
    EXPECT_EQ(sum, expected);
}
