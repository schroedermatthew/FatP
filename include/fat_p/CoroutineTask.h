#pragma once

/*
FATP_META:
  meta_version: 1
  component: CoroutineTask
  file_role: public_header
  path: include/fat_p/CoroutineTask.h
  namespace: fat_p
  layer: Concurrency
  summary: "Public header for CoroutineTask."
  api_stability: in_work
  related:
    docs_search: "CoroutineTask"
    tests:
      - components/CoroutineTask/tests/test_CoroutineTask.cpp
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/

/**
 * @file CoroutineTask.h
 * @brief C++20 coroutine support with Expected-based error handling
 *
 *
 * @version 2.0
 *
 * @details Provides lightweight coroutine task types for asynchronous operations
 * with compile-time error handling via Expected<T,E>. Supports both eager and
 * lazy evaluation, task composition, and cancellation.
 *
 * Key features:
 * - CoroutineTask<T>: Basic coroutine with Expected error handling
 * - Generator<T>: Lazy generator for sequence production
 * - Task composition utilities (when_all, when_any)
 * - Cancellation support
 * - Zero-overhead abstractions (no virtual dispatch)
 * - Exception-safe by default
 *
 * @comparison
 * - vs std::generator (C++23): More control, Expected integration
 * - vs cppcoro: Lighter weight, header-only, no dependencies
 * - vs folly::coro: Simpler API, integrated with Expected
 *
 * @performance
 * - Coroutine frame allocation: compiler-optimized
 * - Resume/suspend: minimal overhead
 * - Generator iteration: low overhead per yield
 *
 * Requires: C++20, Expected.h
 */

#include "CppFeatureDetection.h"

// Only compile if coroutine library support is available
// (C++20 language is guaranteed, but library support may lag)
#if FATP_HAS_COROUTINES

#include "Expected.h"
#include <coroutine>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace fat_p
{

// =============================================================================
// BASIC COROUTINE TASK
// =============================================================================

/**
 * @brief Basic coroutine task with Expected-based error handling
 * @tparam T Return type
 * @tparam E Error type (default: std::string)
 *
 * @details Provides a simple coroutine task that suspends initially (lazy),
 * executes when await() is called, and returns Expected<T,E> for error handling.
 */
template <typename T, typename E = std::string>
struct CoroutineTask
{
    struct promise_type
    {
        Expected<T, E> mValue;

        // Lazy evaluation - suspend at start
        std::suspend_always initial_suspend() noexcept
        {
            return {};
        }

        // Keep alive until destroyed
        std::suspend_always final_suspend() noexcept
        {
            return {};
        }

        // Handle unhandled exceptions
        void unhandled_exception() noexcept
        {
            try
            {
                mValue = unexpected<E>(E{"Unhandled exception"});
            }
            catch (...)
            {
                // If error construction fails, we're in trouble
                std::terminate();
            }
        }

        CoroutineTask get_return_object()
        {
            return CoroutineTask{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        void return_value(T val)
        {
            mValue = std::move(val);
        }
    };

    std::coroutine_handle<promise_type> mHandle;

    // Constructor from handle
    explicit CoroutineTask(std::coroutine_handle<promise_type> h)
        : mHandle(h)
    {
    }

    // Destructor - destroy coroutine frame
    ~CoroutineTask()
    {
        if (mHandle)
        {
            mHandle.destroy();
        }
    }

    // Move-only type
    CoroutineTask(const CoroutineTask&) = delete;
    CoroutineTask& operator=(const CoroutineTask&) = delete;

    CoroutineTask(CoroutineTask&& other) noexcept
        : mHandle(std::exchange(other.mHandle, nullptr))
    {
    }

    CoroutineTask& operator=(CoroutineTask&& other) noexcept
    {
        if (this != &other)
        {
            if (mHandle)
            {
                mHandle.destroy();
            }
            mHandle = std::exchange(other.mHandle, nullptr);
        }
        return *this;
    }

    /**
     * @brief Execute the coroutine and return the result
     * @return Expected<T, E> containing value or error
     */
    Expected<T, E> await()
    {
        if (!mHandle)
        {
            return unexpected<E>(E{"Invalid coroutine handle"});
        }

        if (!mHandle.done())
        {
            mHandle.resume();
        }

        return mHandle.promise().mValue;
    }

    /**
     * @brief Check if the coroutine has completed
     */
    bool done() const noexcept
    {
        return mHandle && mHandle.done();
    }

    /**
     * @brief Check if the coroutine handle is valid
     */
    bool valid() const noexcept
    {
        return mHandle != nullptr;
    }
};

// =============================================================================
// EAGER COROUTINE TASK (starts immediately)
// =============================================================================

/**
 * @brief Eager coroutine task that starts execution immediately
 * @details Unlike CoroutineTask, this starts execution on creation
 */
template <typename T, typename E = std::string>
struct EagerTask
{
    struct promise_type
    {
        Expected<T, E> mValue;

        // Eager evaluation - don't suspend at start
        std::suspend_never initial_suspend() noexcept
        {
            return {};
        }
        std::suspend_always final_suspend() noexcept
        {
            return {};
        }

        void unhandled_exception() noexcept
        {
            try
            {
                mValue = unexpected<E>(E{"Unhandled exception"});
            }
            catch (...)
            {
                std::terminate();
            }
        }

        EagerTask get_return_object()
        {
            return EagerTask{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        void return_value(T val)
        {
            mValue = std::move(val);
        }
    };

    std::coroutine_handle<promise_type> mHandle;

    explicit EagerTask(std::coroutine_handle<promise_type> h)
        : mHandle(h)
    {
    }

    ~EagerTask()
    {
        if (mHandle)
        {
            mHandle.destroy();
        }
    }

    EagerTask(const EagerTask&) = delete;
    EagerTask& operator=(const EagerTask&) = delete;

    EagerTask(EagerTask&& other) noexcept
        : mHandle(std::exchange(other.mHandle, nullptr))
    {
    }

    EagerTask& operator=(EagerTask&& other) noexcept
    {
        if (this != &other)
        {
            if (mHandle)
            {
                mHandle.destroy();
            }
            mHandle = std::exchange(other.mHandle, nullptr);
        }
        return *this;
    }

    Expected<T, E> result()
    {
        if (!mHandle)
        {
            return unexpected<E>(E{"Invalid coroutine handle"});
        }
        return mHandle.promise().mValue;
    }

    bool done() const noexcept
    {
        return mHandle && mHandle.done();
    }
};

// =============================================================================
// GENERATOR (C++20 coroutine for lazy sequences)
// =============================================================================

/**
 * @brief Lazy generator for producing sequences
 * @tparam T Value type to yield
 *
 * @details Produces values on-demand via co_yield. More efficient than
 * returning std::vector for large or infinite sequences.
 */
template <typename T>
class Generator
{
public:
    struct promise_type
    {
        T mCurrentValue;
        std::exception_ptr mException;

        Generator get_return_object()
        {
            return Generator{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_always initial_suspend() noexcept
        {
            return {};
        }
        std::suspend_always final_suspend() noexcept
        {
            return {};
        }

        std::suspend_always yield_value(T value) noexcept
        {
            mCurrentValue = std::move(value);
            return {};
        }

        void return_void() noexcept
        {
        }

        void unhandled_exception() noexcept
        {
            mException = std::current_exception();
        }
    };

    using handle_type = std::coroutine_handle<promise_type>;

    explicit Generator(handle_type h)
        : mHandle(h)
    {
    }

    ~Generator()
    {
        if (mHandle)
        {
            mHandle.destroy();
        }
    }

    Generator(const Generator&) = delete;
    Generator& operator=(const Generator&) = delete;

    Generator(Generator&& other) noexcept
        : mHandle(std::exchange(other.mHandle, nullptr))
    {
    }

    Generator& operator=(Generator&& other) noexcept
    {
        if (this != &other)
        {
            if (mHandle)
            {
                mHandle.destroy();
            }
            mHandle = std::exchange(other.mHandle, nullptr);
        }
        return *this;
    }

    // Iterator support for range-based for loops
    class iterator
    {
    public:
        using iterator_category = std::input_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = T*;
        using reference = T&;

        iterator() = default;

        explicit iterator(handle_type h)
            : mHandle(h)
        {
            if (mHandle)
            {
                mHandle.resume();
                if (mHandle.promise().mException)
                {
                    std::rethrow_exception(mHandle.promise().mException);
                }
            }
        }

        iterator& operator++()
        {
            if (mHandle.done())
            {
                return *this;
            }
            mHandle.resume();
            if (mHandle.promise().mException)
            {
                std::rethrow_exception(mHandle.promise().mException);
            }
            return *this;
        }

        void operator++(int)
        {
            ++*this;
        }

        T& operator*() const
        {
            return mHandle.promise().mCurrentValue;
        }

        T* operator->() const
        {
            return &mHandle.promise().mCurrentValue;
        }

        bool operator==(const iterator& other) const
        {
            return (!mHandle || mHandle.done()) == (!other.mHandle || other.mHandle.done());
        }

        bool operator!=(const iterator& other) const
        {
            return !(*this == other);
        }

    private:
        handle_type mHandle;
    };

    iterator begin()
    {
        return iterator{mHandle};
    }

    iterator end()
    {
        return iterator{};
    }

private:
    handle_type mHandle;
};

// =============================================================================
// TASK COMPOSITION UTILITIES
// =============================================================================

/**
 * @brief Execute all tasks and collect results
 * @details Returns Expected containing vector of results, or first error
 */
template <typename T, typename E = std::string>
Expected<std::vector<T>, E> when_all(std::vector<CoroutineTask<T, E>>& tasks)
{
    std::vector<T> results;
    results.reserve(tasks.size());

    for (auto& task : tasks)
    {
        auto result = task.await();
        if (!result)
        {
            return unexpected<E>(result.error());
        }
        results.push_back(std::move(result.value()));
    }

    return results;
}

/**
 * @brief Execute tasks until first success
 * @details Returns first successful result, or last error if all fail
 */
template <typename T, typename E = std::string>
Expected<T, E> when_any(std::vector<CoroutineTask<T, E>>& tasks)
{
    E last_error;

    for (auto& task : tasks)
    {
        auto result = task.await();
        if (result)
        {
            return result;
        }
        last_error = std::move(result.error());
    }

    return unexpected<E>(std::move(last_error));
}

// =============================================================================
// COROUTINE UTILITIES
// =============================================================================

/**
 * @brief Simple awaitable wrapper for synchronous values
 */
template <typename T>
struct SyncAwaitable
{
    T mValue;

    bool await_ready() const noexcept
    {
        return true;
    }
    void await_suspend(std::coroutine_handle<>) noexcept
    {
    }
    T await_resume() const
    {
        return mValue;
    }
};

/**
 * @brief Create an immediately-ready awaitable from a value
 */
template <typename T>
SyncAwaitable<T> make_ready(T value)
{
    return SyncAwaitable<T>{std::move(value)};
}

// =============================================================================
// CONVENIENT ALIASES
// =============================================================================
// Aliases are in fat_p::coroutine namespace to avoid collision with other Task types
// (e.g., ThreadPoolTask in ThreadPool.h)

namespace coroutine
{

// Common task types
template <typename T>
using Task = CoroutineTask<T, std::string>;
template <typename T>
using TaskResult = Expected<T, std::string>;

// Void task (no return value)
using VoidTask = CoroutineTask<std::monostate, std::string>;

} // namespace coroutine

} // namespace fat_p

#endif // FATP_HAS_COROUTINES
