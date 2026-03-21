#pragma once

#include <condition_variable>
#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace hw::future {

template <typename T>
class Future;

template <typename T>
class Promise;

namespace detail {

template <typename T>
class PromiseBase;

template <typename T>
struct SharedState {
    std::mutex mutex;
    std::condition_variable cv;
    std::optional<T> value;
    std::exception_ptr exception;
    bool ready = false;
};

template <>
struct SharedState<void> {
    std::mutex mutex;
    std::condition_variable cv;
    std::exception_ptr exception;
    bool ready = false;
};

}  // namespace detail

template <typename T>
class Future {
public:
    Future() = default;
    Future(const Future&) = delete;
    Future& operator=(const Future&) = delete;
    Future(Future&&) noexcept = default;
    Future& operator=(Future&&) noexcept = default;

    T Get() {
        if (!state_) {
            throw std::runtime_error("Future has no shared state");
        }

        auto state = std::move(state_);
        std::unique_lock lock(state->mutex);
        state->cv.wait(lock, [&] { return state->ready; });

        if (state->exception) {
            std::rethrow_exception(state->exception);
        }

        if constexpr (!std::is_void_v<T>) {
            return std::move(*state->value);
        }
    }

    bool IsReady() const {
        if (!state_) {
            return false;
        }
        std::lock_guard lock(state_->mutex);
        return state_->ready;
    }

private:
    friend class Promise<T>;
    friend class detail::PromiseBase<T>;

    explicit Future(std::shared_ptr<detail::SharedState<T>> state)
        : state_(std::move(state)) {}

    std::shared_ptr<detail::SharedState<T>> state_;
};

namespace detail {

template <typename T>
class PromiseBase {
protected:
    PromiseBase()
        : state_(std::make_shared<SharedState<T>>()) {}

public:
    PromiseBase(const PromiseBase&) = delete;
    PromiseBase& operator=(const PromiseBase&) = delete;
    PromiseBase(PromiseBase&&) noexcept = default;
    PromiseBase& operator=(PromiseBase&&) noexcept = default;

    ~PromiseBase() {
        if (state_) {
            std::lock_guard lock(state_->mutex);
            if (!state_->ready) {
                state_->exception =
                    std::make_exception_ptr(std::runtime_error("Broken promise"));
                state_->ready = true;
                state_->cv.notify_all();
            }
        }
    }

    Future<T> GetFuture() {
        if (future_retrieved_) {
            throw std::runtime_error("Future already retrieved");
        }
        future_retrieved_ = true;
        return Future<T>(state_);
    }

    void SetException(std::exception_ptr e) {
        if (!state_) {
            throw std::runtime_error("Promise has no shared state");
        }
        std::lock_guard lock(state_->mutex);
        if (state_->ready) {
            throw std::runtime_error("Promise already satisfied");
        }
        state_->exception = std::move(e);
        state_->ready = true;
        state_->cv.notify_all();
    }

protected:
    std::shared_ptr<SharedState<T>> state_;
    bool future_retrieved_ = false;
};

}  // namespace detail

template <typename T>
class Promise : public detail::PromiseBase<T> {
public:
    using detail::PromiseBase<T>::PromiseBase;

    void SetValue(T value) {
        if (!this->state_) {
            throw std::runtime_error("Promise has no shared state");
        }
        std::lock_guard lock(this->state_->mutex);
        if (this->state_->ready) {
            throw std::runtime_error("Promise already satisfied");
        }
        this->state_->value = std::move(value);
        this->state_->ready = true;
        this->state_->cv.notify_all();
    }
};

template <>
class Promise<void> : public detail::PromiseBase<void> {
public:
    using detail::PromiseBase<void>::PromiseBase;

    void SetValue() {
        if (!state_) {
            throw std::runtime_error("Promise has no shared state");
        }
        std::lock_guard lock(state_->mutex);
        if (state_->ready) {
            throw std::runtime_error("Promise already satisfied");
        }
        state_->ready = true;
        state_->cv.notify_all();
    }
};

}  // namespace hw::future
