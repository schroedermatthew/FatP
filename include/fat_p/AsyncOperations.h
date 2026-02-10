#pragma once

/*
FATP_META:
  meta_version: 1
  component: AsyncOperations
  file_role: public_header
  path: include/fat_p/AsyncOperations.h
  namespace: fat_p
  layer: Concurrency
  summary: "Public header for AsyncOperations."
  api_stability: in_work
  related:
    docs_search: "AsyncOperations"
    tests:
      - components/AsyncOperations/tests/test_AsyncOperations.cpp
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
 * @file AsyncOperations.h
 * @brief Asynchronous operation utilities with Expected integration
 */

#include "Expected.h" // Assuming Expected<T, E> is in Expected.h
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace fat_p
{

// Helper to extract value_type from Expected return types
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

// Helper to extract error_type from Expected return types
template <typename T>
struct ExtractExpectedError
{
    using type = std::string; // Default error type
};

template <typename T, typename E, template <typename, typename> class SP>
struct ExtractExpectedError<ExpectedImpl<T, E, SP>>
{
    using type = E;
};

template <typename T>
using ExtractExpectedError_t = typename ExtractExpectedError<T>::type;

template <typename T, typename E = std::string>
class AsyncTask
{
private:
    std::future<Expected<T, E>> mFuture;
    std::optional<Expected<T, E>> mCachedResult;

    AsyncTask(std::future<Expected<T, E>> fut)
        : mFuture(std::move(fut))
    {
    }

public:
    AsyncTask() = delete;

    template <typename Func, typename... Args>
    static AsyncTask create(Func&& func, Args&&... args)
    {
        return AsyncTask(std::async(std::launch::async, std::forward<Func>(func), std::forward<Args>(args)...));
    }

    Expected<T, E> wait()
    {
        if (mCachedResult)
        {
            return *mCachedResult;
        }
        mCachedResult = mFuture.get();
        return *mCachedResult;
    }

    bool valid() const
    {
        return mCachedResult.has_value() || mFuture.valid();
    }

    template <typename Func>
    auto then(Func&& continuation) -> AsyncTask<ExtractExpectedValue_t<std::invoke_result_t<Func, T>>, E>
    {
        using ResultType = std::invoke_result_t<Func, T>;
        using NewT = ExtractExpectedValue_t<ResultType>;

        return AsyncTask<NewT, E>::create(
            [fut = std::move(mFuture), cont = std::forward<Func>(continuation)]() mutable -> Expected<NewT, E> {
                auto result = fut.get();
                if (!result)
                {
                    return unexpected(result.error());
                }
                return cont(*result);
            });
    }

    template <typename Func>
    AsyncTask<T, E> error(Func&& error_handler)
    {
        return create(
            [fut = std::move(mFuture), handler = std::forward<Func>(error_handler)]() mutable -> Expected<T, E> {
                auto result = fut.get();
                if (result)
                {
                    return result;
                }
                handler(result.error());
                return unexpected(result.error()); // Or handle differently
            });
    }

    // Poll method for non-blocking check
    Expected<T, E> poll()
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
        return unexpected(notReadyError());
    }

    // Cancel not directly supported in std::future, but could add promise-based version later

private:
    static const E& notReadyError()
    {
        static const E kInstance("Not ready");
        return kInstance;
    }
};

template <typename Func, typename... Args>
auto async_task(Func&& func, Args&&... args)
{
    using ResultType = std::invoke_result_t<Func, Args...>;
    using ValueType = ExtractExpectedValue_t<ResultType>;
    using ErrorType = ExtractExpectedError_t<ResultType>;

    return AsyncTask<ValueType, ErrorType>::create(std::forward<Func>(func), std::forward<Args>(args)...);
}

} // namespace fat_p
