#pragma once

#include "future.h"

#include <sys/wait.h>
#include <unistd.h>

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace hw::future {

class ProcessPool {
public:
    explicit ProcessPool(std::size_t max_workers)
        : max_workers_(max_workers) {}

    ProcessPool(const ProcessPool&) = delete;
    ProcessPool& operator=(const ProcessPool&) = delete;

    ~ProcessPool() {
        {
            std::unique_lock lock(mutex_);
            stopped_ = true;
            cv_.wait(lock, [this] { return active_workers_ == 0; });
        }

        std::lock_guard lock(reapers_mutex_);
        for (auto& t : reapers_) {
            if (t.joinable()) {
                t.join();
            }
        }
    }

    template <typename F>
    Future<std::invoke_result_t<F>> Submit(F&& func) {
        using R = std::invoke_result_t<F>;
        static_assert(std::is_trivially_copyable_v<R> || std::is_void_v<R>,
                      "ProcessPool result type must be trivially copyable (pipe IPC)");

        Promise<R> promise;
        auto future = promise.GetFuture();

        // Wait for a free slot
        {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, [this] { return active_workers_ < max_workers_ || stopped_; });
            if (stopped_) {
                throw std::runtime_error("Submit on stopped ProcessPool");
            }
            ++active_workers_;
        }

        int pipe_fd[2];
        if (pipe(pipe_fd) == -1) {
            ReleaseSlot();
            throw std::runtime_error("pipe() failed");
        }

        pid_t pid = fork();
        if (pid == -1) {
            close(pipe_fd[0]);
            close(pipe_fd[1]);
            ReleaseSlot();
            throw std::runtime_error("fork() failed");
        }

        if (pid == 0) {
            // Child process
            close(pipe_fd[0]);
            ChildExecute<R>(std::forward<F>(func), pipe_fd[1]);
            // ChildExecute calls _exit, never returns
        }

        // Parent process
        close(pipe_fd[1]);

        auto reaper = std::jthread(
            [this, read_fd = pipe_fd[0], child_pid = pid,
             promise = std::move(promise)]() mutable {
                ParentReadResult<R>(read_fd, child_pid, promise);
            });

        {
            std::lock_guard lock(reapers_mutex_);
            reapers_.push_back(std::move(reaper));
        }

        return future;
    }

private:
    void ReleaseSlot() {
        {
            std::lock_guard lock(mutex_);
            --active_workers_;
        }
        cv_.notify_one();
    }

    template <typename R, typename F>
    [[noreturn]] static void ChildExecute(F&& func, int write_fd) {
        uint8_t status = 0;
        try {
            if constexpr (std::is_void_v<R>) {
                func();
                ::write(write_fd, &status, sizeof(status));
            } else {
                R result = func();
                ::write(write_fd, &status, sizeof(status));
                ::write(write_fd, &result, sizeof(R));
            }
        } catch (...) {
            status = 1;
            ::write(write_fd, &status, sizeof(status));
        }
        ::close(write_fd);
        ::_exit(0);
    }

    template <typename R>
    void ParentReadResult(int read_fd, pid_t child_pid, Promise<R>& promise) {
        uint8_t status = 1;
        bool read_ok = (::read(read_fd, &status, sizeof(status)) == sizeof(status));

        if constexpr (!std::is_void_v<R>) {
            R result{};
            if (read_ok && status == 0) {
                read_ok = (::read(read_fd, &result, sizeof(R)) == sizeof(R));
            }
            ::close(read_fd);
            ::waitpid(child_pid, nullptr, 0);

            if (read_ok && status == 0) {
                promise.SetValue(std::move(result));
            } else {
                promise.SetException(
                    std::make_exception_ptr(std::runtime_error("Child process failed")));
            }
        } else {
            ::close(read_fd);
            ::waitpid(child_pid, nullptr, 0);

            if (read_ok && status == 0) {
                promise.SetValue();
            } else {
                promise.SetException(
                    std::make_exception_ptr(std::runtime_error("Child process failed")));
            }
        }

        ReleaseSlot();
    }

    std::size_t max_workers_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::size_t active_workers_ = 0;
    bool stopped_ = false;

    std::mutex reapers_mutex_;
    std::vector<std::jthread> reapers_;
};

}  // namespace hw::future
