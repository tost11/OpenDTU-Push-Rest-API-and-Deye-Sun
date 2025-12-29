// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <memory>
#include <mutex>
#include <condition_variable>
#include <chrono>

// Forward declaration
template<typename T>
class LightPromise;

/**
 * @brief Lightweight future implementation to replace std::future
 *
 * Provides only the features actually used in the codebase:
 * - wait_for() with timeout
 * - get() to retrieve value
 * - Non-blocking status check
 *
 * Saves ~2-3KB flash compared to std::future by eliminating:
 * - Exception handling infrastructure
 * - Complex template machinery
 * - future_error exception class
 * - Deferred/async execution support
 */
template<typename T>
class LightFuture {
public:
    enum class Status {
        NOT_READY,
        READY,
        TIMEOUT
    };

    /**
     * @brief Check if value is ready with timeout
     * @param timeoutMs Timeout in milliseconds (0 = non-blocking check)
     * @return Status indicating if value is ready
     */
    Status wait_for(uint32_t timeoutMs) {
        std::unique_lock<std::mutex> lock(_state->mutex);

        if (_state->hasValue) {
            return Status::READY;
        }

        if (timeoutMs == 0) {
            return Status::NOT_READY;
        }

        auto timeout = std::chrono::milliseconds(timeoutMs);
        if (_state->cv.wait_for(lock, timeout, [this] {
            return _state->hasValue;
        })) {
            return Status::READY;
        }

        return _state->hasValue ? Status::READY : Status::TIMEOUT;
    }

    /**
     * @brief Get the value (blocks until ready)
     * @return The stored value
     */
    T get() {
        std::unique_lock<std::mutex> lock(_state->mutex);
        _state->cv.wait(lock, [this] { return _state->hasValue; });
        return _state->value;
    }

    /**
     * @brief Check if value is available without blocking
     * @return true if value is ready
     */
    bool is_ready() const {
        std::lock_guard<std::mutex> lock(_state->mutex);
        return _state->hasValue;
    }

private:
    friend class LightPromise<T>;

    struct SharedState {
        T value;
        mutable std::mutex mutex;
        std::condition_variable cv;
        bool hasValue = false;
    };

    std::shared_ptr<SharedState> _state;

    explicit LightFuture(std::shared_ptr<SharedState> state)
        : _state(state) {}
};

/**
 * @brief Lightweight promise implementation to replace std::promise
 *
 * Thread-safe value setter for LightFuture.
 * Idempotent set_value() - safe to call multiple times.
 */
template<typename T>
class LightPromise {
public:
    LightPromise()
        : _state(std::make_shared<typename LightFuture<T>::SharedState>())
        , _futureRetrieved(false) {}

    /**
     * @brief Set the value (thread-safe, idempotent)
     * @param value Value to store
     *
     * Note: Unlike std::promise, this is idempotent - calling multiple
     * times is safe (subsequent calls are ignored).
     */
    void set_value(T value) {
        std::lock_guard<std::mutex> lock(_state->mutex);
        if (!_state->hasValue) {
            _state->value = std::move(value);
            _state->hasValue = true;
            _state->cv.notify_all();
        }
    }

    /**
     * @brief Get associated future
     * @return LightFuture that will receive the value
     *
     * Note: Can be called multiple times (unlike std::promise)
     */
    LightFuture<T> get_future() {
        _futureRetrieved = true;
        return LightFuture<T>(_state);
    }

private:
    std::shared_ptr<typename LightFuture<T>::SharedState> _state;
    bool _futureRetrieved;
};
