#pragma once
/*
FATP_META:
  meta_version: 1
  component: ExpectedAsyncTask
  file_role: public_header
  path: include/fat_p/ExpectedAsyncTask.h
  namespace: fat_p
  layer: Foundation
  summary: "AsyncTask support for Expected, split from core Expected.h to keep dependencies lean."
  api_stability: in_work
  related:
    docs_search: "AsyncOperations"
    tests:
      - components/Expected/tests/test_AsyncOperations.cpp
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
*/

/**
 * @file ExpectedAsyncTask.h
 * @brief std::future-backed AsyncTask integration for fat_p::Expected.
 *
 * This header is intentionally separate from Expected.h so code that only needs
 * the core result type does not pay for <future>, <optional>, and async support.
 */

#include "Expected.h"

#include <chrono>
#include <future>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

namespace fat_p
{

// ====================================================================
// Async Operations (Expected-integrated async tasks)
// ====================================================================

/// @brief Helper to extract value_type from Expected return types.
template <typename T>
struct ExtractExpectedValue
{
    using type = T;
};

template <typename T, typename E, template <typename, typename> class SP>
struct ExtractExpectedValue<ExpectedImpl<T, E, SP>>
{
    using type = T;
};

template <typename T>
using ExtractExpectedValue_t = typename ExtractExpectedValue<T>::type;

/// @brief Helper to extract error_type from Expected return types.
template <typename T>
struct ExtractExpectedError
{
    using type = std::string; // Default for non-Expected callables, if used.
};

template <typename T, typename E, template <typename, typename> class SP>
struct ExtractExpectedError<ExpectedImpl<T, E, SP>>
{
    using type = E;
};

template <typename T>
using ExtractExpectedError_t = typename ExtractExpectedError<T>::type;

namespace detail
{

template <typename T>
struct is_expected_impl : std::false_type
{
};

template <typename T, typename E, template <typename, typename> class SP>
struct is_expected_impl<ExpectedImpl<T, E, SP>> : std::true_type
{
};

template <typename T>
inline constexpr bool is_expected_impl_v = is_expected_impl<std::remove_cvref_t<T>>::value;

template <typename R, typename E>
using normalized_expected_t = Expected<ExtractExpectedValue_t<std::remove_cvref_t<R>>, E>;

template <typename E, typename R>
[[nodiscard]] auto normalize_async_result(R&& r) -> normalized_expected_t<R, E>
{
    using Raw = std::remove_cvref_t<R>;
    using T = ExtractExpectedValue_t<Raw>;

    if constexpr (is_expected_impl_v<Raw>)
    {
        if (r.has_value())
        {
            if constexpr (std::is_void_v<T>)
            {
                return Expected<void, E>();
            }
            else
            {
                return Expected<T, E>(std::in_place, *std::forward<R>(r));
            }
        }
        return Expected<T, E>(unexpected<E>(std::forward<R>(r).error()));
    }
    else
    {
        return Expected<T, E>(std::forward<R>(r));
    }
}

} // namespace detail

/**
 * @brief Asynchronous task wrapper producing Expected<T, E> results.
 *
 * Wraps std::future with Expected integration, providing monadic continuation
 * (.then), error observation (.error), and non-blocking poll().
 * poll() returns std::nullopt when the task is not ready; "not ready" is task
 * state, not a fabricated domain error.
 */
template <typename T, typename E = std::string>
class AsyncTask
{
private:
    std::future<Expected<T, E>> mFuture;
    std::optional<Expected<T, E>> mCachedResult;

    explicit AsyncTask(std::future<Expected<T, E>> fut)
        : mFuture(std::move(fut))
    {
    }

    template <typename, typename>
    friend class AsyncTask;

public:
    AsyncTask() = delete;

    template <typename Func, typename... Args>
    static AsyncTask create(Func&& func, Args&&... args)
    {
        return AsyncTask(std::async(std::launch::async, std::forward<Func>(func), std::forward<Args>(args)...));
    }

    [[nodiscard]] Expected<T, E> wait()
    {
        if (mCachedResult)
        {
            return *mCachedResult;
        }
        mCachedResult = mFuture.get();
        return *mCachedResult;
    }

    [[nodiscard]] bool valid() const
    {
        return mCachedResult.has_value() || mFuture.valid();
    }

    template <typename Func>
    [[nodiscard]] auto then(Func&& continuation)
    {
        if constexpr (std::is_void_v<T>)
        {
            using ResultType = std::invoke_result_t<Func>;
            using NewT = ExtractExpectedValue_t<std::remove_cvref_t<ResultType>>;

            return AsyncTask<NewT, E>::create(
                [fut = std::move(mFuture), cont = std::forward<Func>(continuation)]() mutable -> Expected<NewT, E> {
                    auto result = fut.get();
                    if (!result)
                    {
                        return Expected<NewT, E>(unexpected<E>(result.error()));
                    }

                    if constexpr (std::is_void_v<ResultType>)
                    {
                        cont();
                        return Expected<void, E>();
                    }
                    else
                    {
                        return detail::normalize_async_result<E>(cont());
                    }
                });
        }
        else
        {
            using ResultType = std::invoke_result_t<Func, T>;
            using NewT = ExtractExpectedValue_t<std::remove_cvref_t<ResultType>>;

            return AsyncTask<NewT, E>::create(
                [fut = std::move(mFuture), cont = std::forward<Func>(continuation)]() mutable -> Expected<NewT, E> {
                    auto result = fut.get();
                    if (!result)
                    {
                        return Expected<NewT, E>(unexpected<E>(result.error()));
                    }

                    if constexpr (std::is_void_v<ResultType>)
                    {
                        cont(*result);
                        return Expected<void, E>();
                    }
                    else
                    {
                        return detail::normalize_async_result<E>(cont(*result));
                    }
                });
        }
    }

    template <typename Func>
    [[nodiscard]] AsyncTask<T, E> error(Func&& error_handler)
    {
        return create(
            [fut = std::move(mFuture), handler = std::forward<Func>(error_handler)]() mutable -> Expected<T, E> {
                auto result = fut.get();
                if (result)
                {
                    return result;
                }
                handler(result.error());
                return Expected<T, E>(unexpected<E>(result.error()));
            });
    }

    /// @brief Non-blocking check for result availability. std::nullopt means not ready.
    [[nodiscard]] std::optional<Expected<T, E>> poll()
    {
        if (mCachedResult)
        {
            return *mCachedResult;
        }
        if (mFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            mCachedResult = mFuture.get();
            return *mCachedResult;
        }
        return std::nullopt;
    }
};

/**
 * @brief Factory function for creating async tasks that produce Expected results.
 *
 * @tparam Func Callable returning Expected<T, E>
 * @tparam Args Arguments forwarded to the callable
 */
template <typename Func, typename... Args>
[[nodiscard]] auto async_task(Func&& func, Args&&... args)
{
    using ResultType = std::invoke_result_t<Func, Args...>;
    using ValueType = ExtractExpectedValue_t<ResultType>;
    using ErrorType = ExtractExpectedError_t<ResultType>;

    return AsyncTask<ValueType, ErrorType>::create(std::forward<Func>(func), std::forward<Args>(args)...);
}

} // namespace fat_p
