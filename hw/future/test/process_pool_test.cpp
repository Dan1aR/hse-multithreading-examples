#include "process_pool.h"

#include <gtest/gtest.h>

#include <sys/types.h>
#include <unistd.h>

#include <chrono>
#include <stdexcept>
#include <vector>

TEST(ProcessPool, SubmitSingleTask) {
    hw::future::ProcessPool pool(2);
    auto future = pool.Submit([] { return 42; });
    EXPECT_EQ(future.Get(), 42);
}

TEST(ProcessPool, SubmitVoidTask) {
    hw::future::ProcessPool pool(2);
    auto future = pool.Submit([] { /* noop */ });
    EXPECT_NO_THROW(future.Get());
}

TEST(ProcessPool, SubmitMultipleTasks) {
    hw::future::ProcessPool pool(4);
    std::vector<hw::future::Future<int>> futures;

    for (int i = 0; i < 20; ++i) {
        futures.push_back(pool.Submit([i] { return i * i; }));
    }

    for (int i = 0; i < 20; ++i) {
        EXPECT_EQ(futures[i].Get(), i * i);
    }
}

TEST(ProcessPool, ChildProcessIsolation) {
    hw::future::ProcessPool pool(2);

    // Child gets a different PID than parent
    pid_t parent_pid = getpid();
    auto future = pool.Submit([parent_pid] { return getpid() != parent_pid; });
    EXPECT_TRUE(future.Get());
}

TEST(ProcessPool, TaskThrowsInChild) {
    hw::future::ProcessPool pool(2);
    auto future = pool.Submit([]() -> int { throw std::runtime_error("child error"); });
    EXPECT_THROW(future.Get(), std::runtime_error);
}

TEST(ProcessPool, DifferentTriviallyCopyableTypes) {
    hw::future::ProcessPool pool(2);

    auto f_int = pool.Submit([] { return 42; });
    auto f_double = pool.Submit([] { return 3.14; });
    auto f_char = pool.Submit([] { return 'A'; });

    EXPECT_EQ(f_int.Get(), 42);
    EXPECT_DOUBLE_EQ(f_double.Get(), 3.14);
    EXPECT_EQ(f_char.Get(), 'A');
}

struct Point {
    int x;
    int y;
};

TEST(ProcessPool, StructResult) {
    hw::future::ProcessPool pool(2);
    auto future = pool.Submit([] { return Point{10, 20}; });
    auto result = future.Get();
    EXPECT_EQ(result.x, 10);
    EXPECT_EQ(result.y, 20);
}

TEST(ProcessPool, ConcurrencyLimiting) {
    hw::future::ProcessPool pool(2);

    auto start = std::chrono::steady_clock::now();

    // 4 tasks of 100ms each with pool size 2 -> ~200ms
    std::vector<hw::future::Future<int>> futures;
    for (int i = 0; i < 4; ++i) {
        futures.push_back(pool.Submit([i] {
            usleep(100'000);  // 100ms
            return i;
        }));
    }

    for (auto& f : futures) {
        f.Get();
    }

    auto elapsed = std::chrono::steady_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    // Should take ~200ms (2 batches of 2), not ~400ms (sequential)
    EXPECT_LT(ms, 350);
    // But should take more than 150ms (at least 2 batches)
    EXPECT_GT(ms, 150);
}

TEST(ProcessPool, DestructorWaitsForChildren) {
    // Just verify no crash/hang on destruction with pending tasks
    hw::future::ProcessPool pool(2);
    for (int i = 0; i < 5; ++i) {
        pool.Submit([i] { return i; });
    }
}
