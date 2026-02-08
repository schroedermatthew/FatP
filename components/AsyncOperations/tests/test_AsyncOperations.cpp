/**
 * @file test_AsyncOperations.cpp
 * @brief Comprehensive unit tests for AsyncOperations.h
 */
/*
FATP_META:
  meta_version: 1
  component: AsyncOperations
  file_role: test
  path: components/AsyncOperations/tests/test_AsyncOperations.cpp
  layer: Testing
  namespace: fat_p::testing::asyncoperations
  summary: "Unit tests for AsyncOperations."
  api_stability: in_work
  related:
    docs_search: "AsyncOperations"
    headers:
      - include/fat_p/AsyncOperations.h
      - include/fat_p/Expected.h
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

#include "AsyncOperations.h"
#include "Expected.h"
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
    // May or may not be ready depending on timing

    auto final_result = task.wait();
    FATP_ASSERT_TRUE(final_result.has_value(), "Final wait should succeed");

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
} // namespace fat_p::testing::asyncoperations

namespace fat_p::testing
{


void run_benchmarks()
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
    FATP_RUN_TEST_NS(runner, asyncoperations, valid);


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
