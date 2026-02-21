#pragma once

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <stdexcept>

template <class T>
class UnbufferedChannel {
public:
    void Send(const T& value) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_send_.wait(lock, [this] {
            return closed_ || (waiting_receivers_ > 0 && !slot_.has_value());
        });
        if (closed_) {
            throw std::runtime_error("Channel is closed");
        }

        slot_.emplace(value);
        cv_recv_.notify_one();

        cv_send_.wait(lock, [this] {
            return closed_ || !slot_.has_value();
        });

        if (slot_.has_value()) {
            slot_.reset();
            throw std::runtime_error("Channel is closed");
        }
    }

    std::optional<T> Recv() {
        std::unique_lock<std::mutex> lock(mutex_);
        ++waiting_receivers_;
        cv_send_.notify_one();

        cv_recv_.wait(lock, [this] {
            return closed_ || slot_.has_value();
        });

        --waiting_receivers_;
        if (closed_) {
            return std::nullopt;
        }

        std::optional<T> result(std::move(*slot_));
        slot_.reset();
        cv_send_.notify_one();
        return result;
    }

    void Close() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            closed_ = true;
        }
        cv_send_.notify_all();
        cv_recv_.notify_all();
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_send_;
    std::condition_variable cv_recv_;
    bool closed_ = false;
    size_t waiting_receivers_ = 0;
    std::optional<T> slot_;
};
