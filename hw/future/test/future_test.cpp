#include "future.h"

#include <gtest/gtest.h>

#include <stdexcept>
#include <thread>
#include <type_traits>

TEST(FuturePromise, NonCopyable) {
    static_assert(!std::is_copy_constructible_v<hw::future::Future<int>>);
    static_assert(!std::is_copy_assignable_v<hw::future::Future<int>>);
    static_assert(!std::is_copy_constructible_v<hw::future::Promise<int>>);
    static_assert(!std::is_copy_assignable_v<hw::future::Promise<int>>);
}

TEST(FuturePromise, MoveConstructible) {
    static_assert(std::is_move_constructible_v<hw::future::Future<int>>);
    static_assert(std::is_move_constructible_v<hw::future::Promise<int>>);
}

TEST(FuturePromise, SetValueThenGet) {
    hw::future::Promise<int> promise;
    auto future = promise.GetFuture();

    promise.SetValue(42);
    EXPECT_EQ(future.Get(), 42);
}

TEST(FuturePromise, SetValueVoid) {
    hw::future::Promise<void> promise;
    auto future = promise.GetFuture();

    promise.SetValue();
    EXPECT_NO_THROW(future.Get());
}

TEST(FuturePromise, SetExceptionThenGet) {
    hw::future::Promise<int> promise;
    auto future = promise.GetFuture();

    promise.SetException(std::make_exception_ptr(std::runtime_error("test error")));
    EXPECT_THROW(future.Get(), std::runtime_error);
}

TEST(FuturePromise, GetBlocksUntilReady) {
    hw::future::Promise<int> promise;
    auto future = promise.GetFuture();

    std::jthread setter([&promise] {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        promise.SetValue(123);
    });

    EXPECT_EQ(future.Get(), 123);
}

TEST(FuturePromise, IsReady) {
    hw::future::Promise<int> promise;
    auto future = promise.GetFuture();

    EXPECT_FALSE(future.IsReady());
    promise.SetValue(1);
    EXPECT_TRUE(future.IsReady());
}

TEST(FuturePromise, BrokenPromise) {
    hw::future::Future<int> future;
    {
        hw::future::Promise<int> promise;
        future = promise.GetFuture();
    }
    EXPECT_THROW(future.Get(), std::runtime_error);
}

TEST(FuturePromise, GetOnEmptyFuture) {
    hw::future::Future<int> future;
    EXPECT_THROW(future.Get(), std::runtime_error);
}

TEST(FuturePromise, DoubleGetThrows) {
    hw::future::Promise<int> promise;
    auto future = promise.GetFuture();
    promise.SetValue(1);

    EXPECT_EQ(future.Get(), 1);
    EXPECT_THROW(future.Get(), std::runtime_error);
}

TEST(FuturePromise, DoubleGetFutureThrows) {
    hw::future::Promise<int> promise;
    auto f1 = promise.GetFuture();
    EXPECT_THROW(promise.GetFuture(), std::runtime_error);
}
