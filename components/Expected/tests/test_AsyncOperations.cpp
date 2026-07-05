/**
 * @file test_AsyncOperations.cpp
 * @brief Comprehensive unit tests for AsyncOperations.h
 */
/*
FATP_META:
  meta_version: 1
  component: AsyncOperations
  file_role: test
  path: components/Expected/tests/test_AsyncOperations.cpp
  layer: Testing
  namespace: fat_p::testing::asyncoperations
  summary: "Unit tests for AsyncOperations."
  api_stability: in_work
  related:
    docs_search: "AsyncOperations"
    headers:
      - include/fat_p/ExpectedAsyncTask.h
      - include/fat_p/FatPTest.h
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include "ExpectedAsyncTask.h"
#include "FatPTest.h"

namespace fat_p::testing::asyncoperations
{

FATP_TEST_CASE(basic_task)
{
    auto task = async_task([]() -> Expected<int, std::string> {
        return 42;
    });

    auto result = task.wait();
    FATP_ASSERT_TRUE(result.has_value(), "Task should succeed");
    FATP_ASSERT_TRUE(*result == 42, "Result should be 42");

    return true;
}

FATP_TEST_CASE(continuation)
{
    auto task = async_task([]() -> Expected<int, std::string> {
                    return 10;
                }).then([](int val) -> Expected<int, std::string> {
        return val * 2;
    });

    auto result = task.wait();
    FATP_ASSERT_TRUE(result.has_value(), "Task should succeed");
    FATP_ASSERT_TRUE(*result == 20, "Result should be 20");

    return true;
}

FATP_TEST_CASE(error_handling)
{
    bool error_called = false;

    auto task = async_task([]() -> Expected<int, std::string> {
                    return unexpected<std::string>("error occurred");
                }).error([&](const std::string& /*err*/) {
        error_called = true;
    });

    auto result = task.wait();
    FATP_ASSERT_TRUE(!result.has_value(), "Task should fail");
    FATP_ASSERT_TRUE(error_called, "Error handler should be called");

    return true;
}

FATP_TEST_CASE(chained_continuations)
{
    auto task = async_task([]() -> Expected<int, std::string> {
                    return 5;
                })
                    .then([](int val) -> Expected<int, std::string> {
                        return val + 10;
                    })
                    .then([](int val) -> Expected<int, std::string> {
                        return val * 2;
                    });

    auto result = task.wait();
    FATP_ASSERT_TRUE(result.has_value(), "Task should succeed");
    FATP_ASSERT_TRUE(*result == 30, "Result should be 30 ((5+10)*2)");

    return true;
}

FATP_TEST_CASE(error_propagation)
{
    auto task = async_task([]() -> Expected<int, std::string> {
                    return unexpected<std::string>("initial error");
                }).then([](int val) -> Expected<int, std::string> {
        return val * 2; // Should not execute
    });

    auto result = task.wait();
    FATP_ASSERT_TRUE(!result.has_value(), "Task should fail");
    FATP_ASSERT_TRUE(result.error() == "initial error", "Error should propagate");

    return true;
}

FATP_TEST_CASE(poll)
{
    auto task = async_task([]() -> Expected<int, std::string> {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return 42;
    });

    auto early_result = task.poll();
    if (early_result)
    {
        FATP_ASSERT_TRUE(early_result->has_value(), "Ready poll should contain the task result");
    }

    auto final_result = task.wait();
    FATP_ASSERT_TRUE(final_result.has_value(), "Final wait should succeed");

    return true;
}

FATP_TEST_CASE(poll_with_non_string_error)
{
    enum class Err
    {
        Failed
    };

    auto task = async_task([]() -> Expected<int, Err> {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return 7;
    });

    auto maybe_result = task.poll();
    if (maybe_result)
    {
        FATP_ASSERT_TRUE(maybe_result->has_value(), "Ready poll should preserve value for enum error type");
    }

    auto result = task.wait();
    FATP_ASSERT_TRUE(result.has_value(), "Final wait should succeed for enum error type");
    FATP_ASSERT_TRUE(*result == 7, "Final value should be preserved");

    return true;
}

FATP_TEST_CASE(valid)
{
    auto task = async_task([]() -> Expected<int, std::string> {
        return 42;
    });

    FATP_ASSERT_TRUE(task.valid(), "Task should be valid before wait");

    (void)task.wait();

    // After wait, validity depends on implementation

    return true;
}

// ============================================================================
// Coverage Gap Tests — Type-changing Continuations
// ============================================================================

FATP_TEST_CASE(then_type_change_int_to_string)
{
    auto task = async_task([]() -> Expected<int, std::string> {
                    return 42;
                }).then([](int val) -> Expected<std::string, std::string> {
        return std::string("value=" + std::to_string(val));
    });

    auto result = task.wait();
    FATP_ASSERT_TRUE(result.has_value(), "Type-changing then should succeed");
    FATP_ASSERT_EQ(*result, std::string("value=42"),
                    "Type-changing then should transform correctly");
    return true;
}

FATP_TEST_CASE(then_type_change_error_propagation)
{
    auto task = async_task([]() -> Expected<int, std::string> {
                    return unexpected<std::string>("early fail");
                }).then([](int val) -> Expected<std::string, std::string> {
        return std::string("value=" + std::to_string(val));
    });

    auto result = task.wait();
    FATP_ASSERT_TRUE(!result.has_value(), "Error should propagate through type change");
    FATP_ASSERT_EQ(result.error(), std::string("early fail"),
                    "Original error preserved through type-change then");
    return true;
}

FATP_TEST_CASE(chained_type_transformations)
{
    auto task = async_task([]() -> Expected<int, std::string> {
                    return 10;
                })
                    .then([](int val) -> Expected<double, std::string> {
                        return val * 2.5;
                    })
                    .then([](double val) -> Expected<std::string, std::string> {
                        return std::to_string(static_cast<int>(val));
                    });

    auto result = task.wait();
    FATP_ASSERT_TRUE(result.has_value(), "Multi-type chain should succeed");
    FATP_ASSERT_EQ(*result, std::string("25"), "Multi-type chain result correct");
    return true;
}

// ============================================================================
// Coverage Gap Tests — Void Async Task
// ============================================================================

FATP_TEST_CASE(void_async_task_success)
{
    int side_effect = 0;
    auto task = async_task([&]() -> Expected<void, std::string> {
        side_effect = 99;
        return {};
    });

    auto result = task.wait();
    FATP_ASSERT_TRUE(result.has_value(), "Void async task should succeed");
    FATP_ASSERT_EQ(side_effect, 99, "Void task side effect should execute");
    return true;
}

FATP_TEST_CASE(void_async_task_error)
{
    auto task = async_task([]() -> Expected<void, std::string> {
        return unexpected<std::string>("void failed");
    });

    auto result = task.wait();
    FATP_ASSERT_TRUE(!result.has_value(), "Void async task should propagate error");
    FATP_ASSERT_EQ(result.error(), std::string("void failed"),
                    "Void async error preserved");
    return true;
}

FATP_TEST_CASE(void_task_with_continuation)
{
    auto task = async_task([]() -> Expected<void, std::string> {
                    return {};
                }).then([]() -> Expected<int, std::string> {
        return 42;
    });

    auto result = task.wait();
    FATP_ASSERT_TRUE(result.has_value(), "Void->int continuation should succeed");
    FATP_ASSERT_EQ(*result, 42, "Void->int continuation result correct");
    return true;
}

FATP_TEST_CASE(void_task_error_skips_continuation)
{
    auto task = async_task([]() -> Expected<void, std::string> {
                    return unexpected<std::string>("blocked");
                }).then([]() -> Expected<int, std::string> {
        return 42;
    });

    auto result = task.wait();
    FATP_ASSERT_TRUE(!result.has_value(), "Error should skip void->int continuation");
    FATP_ASSERT_EQ(result.error(), std::string("blocked"),
                    "Error preserved through void continuation");
    return true;
}

// ============================================================================
// Coverage Gap Tests — Cached Result / Double wait
// ============================================================================

FATP_TEST_CASE(double_wait_returns_cached)
{
    int call_count = 0;
    auto task = async_task([&]() -> Expected<int, std::string> {
        ++call_count;
        return 42;
    });

    auto result1 = task.wait();
    auto result2 = task.wait();

    FATP_ASSERT_TRUE(result1.has_value(), "First wait should succeed");
    FATP_ASSERT_TRUE(result2.has_value(), "Second wait should succeed");
    FATP_ASSERT_EQ(*result1, 42, "First wait value correct");
    FATP_ASSERT_EQ(*result2, 42, "Second wait returns same value");
    FATP_ASSERT_EQ(call_count, 1, "Task body should only execute once");
    return true;
}

// ============================================================================
// Coverage Gap Tests — poll() after wait()
// ============================================================================

FATP_TEST_CASE(poll_after_wait)
{
    auto task = async_task([]() -> Expected<int, std::string> {
        return 77;
    });

    (void)task.wait();

    auto polled = task.poll();
    FATP_ASSERT_TRUE(polled.has_value(), "poll() after wait() should return result");
    FATP_ASSERT_TRUE(polled->has_value(), "Polled result should have value");
    FATP_ASSERT_EQ(**polled, 77, "Polled value correct after wait");
    return true;
}

// ============================================================================
// Coverage Gap Tests — Error Handler
// ============================================================================

FATP_TEST_CASE(error_handler_preserves_error)
{
    std::string captured_error;

    auto task = async_task([]() -> Expected<int, std::string> {
                    return unexpected<std::string>("observed error");
                }).error([&](const std::string& err) {
        captured_error = err;
    });

    auto result = task.wait();
    FATP_ASSERT_TRUE(!result.has_value(), "Error handler should not swallow error");
    FATP_ASSERT_EQ(result.error(), std::string("observed error"),
                    "Error should be preserved after handler");
    FATP_ASSERT_EQ(captured_error, std::string("observed error"),
                    "Error handler should capture the error");
    return true;
}

FATP_TEST_CASE(error_handler_not_called_on_success)
{
    bool handler_called = false;

    auto task = async_task([]() -> Expected<int, std::string> {
                    return 42;
                }).error([&](const std::string&) {
        handler_called = true;
    });

    auto result = task.wait();
    FATP_ASSERT_TRUE(result.has_value(), "Success should pass through error handler");
    FATP_ASSERT_EQ(*result, 42, "Value preserved");
    FATP_ASSERT_TRUE(!handler_called, "Error handler should not fire on success");
    return true;
}

// ============================================================================
// Coverage Gap Tests — Non-string Error Type with Continuations
// ============================================================================

FATP_TEST_CASE(enum_error_continuation)
{
    enum class Err
    {
        NotFound,
        Timeout
    };

    auto task = async_task([]() -> Expected<int, Err> {
                    return 5;
                }).then([](int val) -> Expected<std::string, Err> {
        return std::string("got " + std::to_string(val));
    });

    auto result = task.wait();
    FATP_ASSERT_TRUE(result.has_value(), "Enum-error continuation should succeed");
    FATP_ASSERT_EQ(*result, std::string("got 5"), "Enum-error continuation value correct");
    return true;
}

FATP_TEST_CASE(enum_error_propagation_through_chain)
{
    enum class Err
    {
        NotFound,
        Timeout
    };

    auto task = async_task([]() -> Expected<int, Err> {
                    return unexpected{Err::Timeout};
                }).then([](int val) -> Expected<std::string, Err> {
        return std::string("got " + std::to_string(val));
    });

    auto result = task.wait();
    FATP_ASSERT_TRUE(!result.has_value(), "Enum error should propagate");
    FATP_ASSERT_TRUE(result.error() == Err::Timeout, "Enum error value preserved");
    return true;
}

} // namespace fat_p::testing::asyncoperations

namespace fat_p::testing
{


inline void run_benchmarks()
{
    using namespace fat_p::testing;

    auto& out = *get_test_config().output;
    out << "\n" << colors::cyan() << "Sanity Benchmark:" << colors::reset() << "\n";
    out << "  (benchmarks are provided by benchmark executables; unit tests do not run full benchmarks)\n\n";
}

bool test_AsyncOperations()
{
    FATP_PRINT_HEADER(ASYNC OPERATIONS)

    TestRunner runner;

    FATP_RUN_TEST_NS(runner, asyncoperations, basic_task);
    FATP_RUN_TEST_NS(runner, asyncoperations, continuation);
    FATP_RUN_TEST_NS(runner, asyncoperations, error_handling);
    FATP_RUN_TEST_NS(runner, asyncoperations, chained_continuations);
    FATP_RUN_TEST_NS(runner, asyncoperations, error_propagation);
    FATP_RUN_TEST_NS(runner, asyncoperations, poll);
    FATP_RUN_TEST_NS(runner, asyncoperations, poll_with_non_string_error);
    FATP_RUN_TEST_NS(runner, asyncoperations, valid);

    // Coverage gap tests
    FATP_RUN_TEST_NS(runner, asyncoperations, then_type_change_int_to_string);
    FATP_RUN_TEST_NS(runner, asyncoperations, then_type_change_error_propagation);
    FATP_RUN_TEST_NS(runner, asyncoperations, chained_type_transformations);
    FATP_RUN_TEST_NS(runner, asyncoperations, void_async_task_success);
    FATP_RUN_TEST_NS(runner, asyncoperations, void_async_task_error);
    FATP_RUN_TEST_NS(runner, asyncoperations, void_task_with_continuation);
    FATP_RUN_TEST_NS(runner, asyncoperations, void_task_error_skips_continuation);
    FATP_RUN_TEST_NS(runner, asyncoperations, double_wait_returns_cached);
    FATP_RUN_TEST_NS(runner, asyncoperations, poll_after_wait);
    FATP_RUN_TEST_NS(runner, asyncoperations, error_handler_preserves_error);
    FATP_RUN_TEST_NS(runner, asyncoperations, error_handler_not_called_on_success);
    FATP_RUN_TEST_NS(runner, asyncoperations, enum_error_continuation);
    FATP_RUN_TEST_NS(runner, asyncoperations, enum_error_propagation_through_chain);

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_AsyncOperations() ? 0 : 1;
}
#endif
