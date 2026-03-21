#pragma once

#include "future.h"

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace hw::future {

class ThreadPool {
public:
    explicit ThreadPool(std::size_t num_threads) {
        workers_.reserve(num_threads);
        for (std::size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this] { WorkerLoop(); });
        }
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    ~ThreadPool() {
        {
            std::lock_guard lock(mutex_);
            stopped_ = true;
        }
        cv_.notify_all();
        for (auto& w : workers_) {
            w.join();
        }
    }

    template <typename F>
    Future<std::invoke_result_t<F>> Submit(F&& func) {
        using R = std::invoke_result_t<F>;

        Promise<R> promise;
        auto future = promise.GetFuture();

        auto task = [promise = std::move(promise),
                     f = std::forward<F>(func)]() mutable {
            try {
                if constexpr (std::is_void_v<R>) {
                    f();
                    promise.SetValue();
                } else {
                    promise.SetValue(f());
                }
            } catch (...) {
                promise.SetException(std::current_exception());
            }
        };

        {
            std::lock_guard lock(mutex_);
            if (stopped_) {
                throw std::runtime_error("Submit on stopped ThreadPool");
            }
            tasks_.push(std::move(task));
        }
        cv_.notify_one();

        return future;
    }

private:
    void WorkerLoop() {
        while (true) {
            std::move_only_function<void()> task;
            {
                std::unique_lock lock(mutex_);
                cv_.wait(lock, [this] { return stopped_ || !tasks_.empty(); });
                if (stopped_ && tasks_.empty()) {
                    return;
                }
                task = std::move(tasks_.front());
                tasks_.pop();
            }
            task();
        }
    }

    std::vector<std::jthread> workers_;
    std::queue<std::move_only_function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stopped_ = false;
};

}  // namespace hw::future
