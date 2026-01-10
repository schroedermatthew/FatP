// AsyncOperations.h
#pragma once

/*
FATP_META:
  meta_version: 1
  component: AsyncOperations
  file_role: public_header
  path: fat_p/AsyncOperations.h
  namespace: fat_p
  summary: "Public header for AsyncOperations."
  api_stability: in_work
  related:
    docs_search: "AsyncOperations"
    tests:
      - tests/test_AsyncOperations.cpp
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
#include "Expected.h"  // Assuming Expected<T, E> is in Expected.h
#include <functional>
#include <future>
#include <memory>
#include <utility>
#include <type_traits>

namespace fat_p {

// Helper to extract value_type from Expected return types
template <typename T>
struct ExtractExpectedValue {
    using type = T;
};

template <typename T, typename E, template <typename, typename> class SP>
struct ExtractExpectedValue<ExpectedImpl<T, E, SP>> {
    using type = T;
};

template <typename T>
using ExtractExpectedValue_t = typename ExtractExpectedValue<T>::type;

// Helper to extract error_type from Expected return types
template <typename T>
struct ExtractExpectedError {
    using type = std::string; // Default error type
};

template <typename T, typename E, template <typename, typename> class SP>
struct ExtractExpectedError<ExpectedImpl<T, E, SP>> {
    using type = E;
};

template <typename T>
using ExtractExpectedError_t = typename ExtractExpectedError<T>::type;

template <typename T, typename E = std::string>
class AsyncTask {
private:
    std::future<Expected<T, E>> future_;

    AsyncTask(std::future<Expected<T, E>> fut) : future_(std::move(fut)) {}

public:
    AsyncTask() = delete;

    template <typename Func, typename... Args>
    static AsyncTask create(Func&& func, Args&&... args) {
        return AsyncTask(std::async(std::launch::async, std::forward<Func>(func), std::forward<Args>(args)...));
    }

    Expected<T, E> wait() {
        return future_.get();
    }

    bool valid() const {
        return future_.valid();
    }

    template <typename Func>
    auto then(Func&& continuation) -> AsyncTask<ExtractExpectedValue_t<std::invoke_result_t<Func, T>>, E> {
        using ResultType = std::invoke_result_t<Func, T>;
        using NewT = ExtractExpectedValue_t<ResultType>;
        
        return AsyncTask<NewT, E>::create(
            [fut = std::move(future_), cont = std::forward<Func>(continuation)]() mutable -> Expected<NewT, E> {
                auto result = fut.get();
                if (!result) return unexpected(result.error());
                return cont(*result);
            });
    }

    template <typename Func>
    AsyncTask<T, E> error(Func&& error_handler) {
        return create([fut = std::move(future_), handler = std::forward<Func>(error_handler)]() mutable -> Expected<T, E> {
            auto result = fut.get();
            if (result) return result;
            handler(result.error());
            return unexpected(result.error());  // Or handle differently
        });
    }

    // Poll method for non-blocking check
    Expected<T, E> poll() {
        if (future_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            return future_.get();
        }
        return unexpected(E("Not ready"));
    }

    // Cancel not directly supported in std::future, but could add promise-based version later
};

template <typename Func, typename... Args>
auto async_task(Func&& func, Args&&... args) {
    using ResultType = std::invoke_result_t<Func, Args...>;
    using ValueType = ExtractExpectedValue_t<ResultType>;
    using ErrorType = ExtractExpectedError_t<ResultType>;
    
    return AsyncTask<ValueType, ErrorType>::create(std::forward<Func>(func), std::forward<Args>(args)...);
}

}  // namespace fat_p
