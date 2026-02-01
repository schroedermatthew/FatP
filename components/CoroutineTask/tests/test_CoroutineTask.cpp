/*
FATP_META:
  meta_version: 1
  component: CoroutineTask
  file_role: test
  path: components/CoroutineTask/tests/test_CoroutineTask.cpp
  layer: Testing
  namespace: fat_p::testing::coroutinetask
  summary: "Unit tests for CoroutineTask."
  api_stability: in_work
  related:
    docs_search: "CoroutineTask"
    headers:
      - fat_p/CppStandardDetection.h
      - include/fat_p/FatPTest.h
      - include/fat_p/CoroutineTask.h
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

/**
 * @file test_CoroutineTask.cpp
 * @brief Comprehensive unit tests for CoroutineTask
 *
 * Tests cover:
 * - Basic coroutine operations
 * - Error handling with Expected
 * - Eager vs lazy execution
 * - Generator functionality
 * - Task composition (when_all, when_any)
 * - Move semantics
 * - Complex types
 * - Performance benchmarks
 *
 * Note: Requires C++20 for coroutine support
 */

#include <iostream>

#include "CppFeatureDetection.h"
#include "FatPTest.h"

// Only run tests if coroutine library support is available
#if FATP_HAS_COROUTINES

#include "CoroutineTask.h"
#include <numeric>
#include <string>
#include <vector>

namespace fat_p::testing::coroutinetask
{

FATP_TEST_CASE(simple_coroutine)
{
    // Simple coroutine that returns a value
    auto simple_task = []() -> CoroutineTask<int> {
        co_return 42;
    };

    auto task = simple_task();
    auto result = task.await();

    FATP_ASSERT_TRUE(result.has_value(), "Coroutine should return expected value");
    FATP_ASSERT_EQ(result.value(), 42, "Coroutine should return correct value");
    return true;
}

FATP_TEST_CASE(coroutine_with_computation)
{
    // Coroutine that performs computation
    auto compute_sum = [](int a, int b) -> CoroutineTask<int> {
        int result = a + b;
        co_return result;
    };

    auto task = compute_sum(10, 20);
    auto result = task.await();

    FATP_ASSERT_TRUE(result.has_value(), "Computation coroutine should succeed");
    FATP_ASSERT_EQ(result.value(), 30, "Computation result should be correct");
    return true;
}

FATP_TEST_CASE(coroutine_with_string)
{
    // Coroutine that returns string
    auto get_message = []() -> CoroutineTask<std::string> {
        co_return "Hello from coroutine";
    };

    auto task = get_message();
    auto result = task.await();

    FATP_ASSERT_TRUE(result.has_value(), "String coroutine should succeed");
    FATP_ASSERT_EQ(result.value(), "Hello from coroutine", "String value should be correct");
    return true;
}

FATP_TEST_CASE(coroutine_with_complex_type)
{
    struct Data
    {
        int x;
        double y;
        std::string z;

        bool operator==(const Data& other) const
        {
            return x == other.x && y == other.y && z == other.z;
        }
    };

    auto create_data = []() -> CoroutineTask<Data> {
        Data d{42, 3.14, "test"};
        co_return d;
    };

    auto task = create_data();
    auto result = task.await();

    FATP_ASSERT_TRUE(result.has_value(), "Complex type coroutine should succeed");
    FATP_ASSERT_EQ(result.value().x, 42, "Complex type field x");
    FATP_ASSERT_TRUE(std::abs(result.value().y - 3.14) < 1e-6, "Complex type field y");
    FATP_ASSERT_EQ(result.value().z, "test", "Complex type field z");
    return true;
}

// =============================================================================
// Error Handling Tests
// =============================================================================

FATP_TEST_CASE(coroutine_exception_handling)
{
    // Coroutine that throws an exception
    auto throwing_task = []() -> CoroutineTask<int> {
        throw std::runtime_error("Test exception");
        co_return 0; // Never reached
    };

    auto task = throwing_task();
    auto result = task.await();

    FATP_ASSERT_FALSE(result.has_value(), "Exception should result in error");
    return true;
}

FATP_TEST_CASE(invalid_handle_error)
{
    CoroutineTask<int> task{nullptr};
    auto result = task.await();

    FATP_ASSERT_FALSE(result.has_value(), "Invalid handle should produce error");
    return true;
}

// =============================================================================
// Eager Task Tests
// =============================================================================

FATP_TEST_CASE(eager_task_executes_immediately)
{
    int executed = 0;

    auto eager = [&executed]() -> EagerTask<int> {
        executed = 42;
        co_return executed;
    }();

    // Should already be executed
    FATP_ASSERT_TRUE(eager.done(), "Eager task should be done immediately");
    FATP_ASSERT_EQ(executed, 42, "Eager task should have executed");

    auto result = eager.result();
    FATP_ASSERT_TRUE(result.has_value(), "Eager task result should be valid");
    FATP_ASSERT_EQ(result.value(), 42, "Eager task result value");
    return true;
}

FATP_TEST_CASE(lazy_task_waits_for_await)
{
    int executed = 0;

    auto lazy = [&executed]() -> CoroutineTask<int> {
        executed = 42;
        co_return executed;
    }();

    // Should not be executed yet
    FATP_ASSERT_EQ(executed, 0, "Lazy task should not have executed yet");

    auto result = lazy.await();

    // Now should be executed
    FATP_ASSERT_EQ(executed, 42, "Lazy task should execute on await");
    FATP_ASSERT_TRUE(result.has_value(), "Lazy task result should be valid");
    FATP_ASSERT_EQ(result.value(), 42, "Lazy task result value");
    return true;
}

// =============================================================================
// Generator Tests
// =============================================================================

FATP_TEST_CASE(generator_simple_sequence)
{
    auto range = [](int n) -> Generator<int> {
        for (int i = 0; i < n; ++i)
        {
            co_yield i;
        }
    };

    auto gen = range(5);
    std::vector<int> values;

    for (int val : gen)
    {
        values.push_back(val);
    }

    FATP_ASSERT_EQ(values.size(), 5u, "Generator should produce 5 values");
    FATP_ASSERT_EQ(values[0], 0, "First value");
    FATP_ASSERT_EQ(values[4], 4, "Last value");
    return true;
}

FATP_TEST_CASE(generator_fibonacci)
{
    auto fibonacci = [](int n) -> Generator<int> {
        int a = 0, b = 1;
        for (int i = 0; i < n; ++i)
        {
            co_yield a;
            int next = a + b;
            a = b;
            b = next;
        }
    };

    auto gen = fibonacci(6);
    std::vector<int> fib_sequence;

    for (int val : gen)
    {
        fib_sequence.push_back(val);
    }

    FATP_ASSERT_EQ(fib_sequence.size(), 6u, "Should generate 6 fibonacci numbers");
    FATP_ASSERT_EQ(fib_sequence[0], 0, "fib[0]");
    FATP_ASSERT_EQ(fib_sequence[1], 1, "fib[1]");
    FATP_ASSERT_EQ(fib_sequence[2], 1, "fib[2]");
    FATP_ASSERT_EQ(fib_sequence[3], 2, "fib[3]");
    FATP_ASSERT_EQ(fib_sequence[4], 3, "fib[4]");
    FATP_ASSERT_EQ(fib_sequence[5], 5, "fib[5]");
    return true;
}

FATP_TEST_CASE(generator_string_values)
{
    auto string_gen = []() -> Generator<std::string> {
        co_yield "first";
        co_yield "second";
        co_yield "third";
    };

    auto gen = string_gen();
    std::vector<std::string> strings;

    for (auto& str : gen)
    {
        strings.push_back(str);
    }

    FATP_ASSERT_EQ(strings.size(), 3u, "Should generate 3 strings");
    FATP_ASSERT_EQ(strings[0], "first", "First string");
    FATP_ASSERT_EQ(strings[1], "second", "Second string");
    FATP_ASSERT_EQ(strings[2], "third", "Third string");
    return true;
}

// =============================================================================
// Task Composition Tests
// =============================================================================

// Helper function for task composition tests.
// Note: Coroutine parameters are stored in the coroutine frame, but lambda
// captures are stored in the lambda object which may be destroyed before
// the coroutine completes. Always prefer function parameters over lambda
// captures when creating coroutines in loops.
static CoroutineTask<int> make_value_task(int value)
{
    co_return value;
}

FATP_TEST_CASE(when_all_success)
{
    std::vector<CoroutineTask<int>> tasks;

    for (int i = 0; i < 3; ++i)
    {
        tasks.push_back(make_value_task(i * 10));
    }

    auto result = when_all(tasks);

    FATP_ASSERT_TRUE(result.has_value(), "when_all should succeed");
    FATP_ASSERT_EQ(result.value().size(), 3u, "Should have 3 results");
    FATP_ASSERT_EQ(result.value()[0], 0, "First result");
    FATP_ASSERT_EQ(result.value()[1], 10, "Second result");
    FATP_ASSERT_EQ(result.value()[2], 20, "Third result");
    return true;
}

FATP_TEST_CASE(when_all_failure)
{
    std::vector<CoroutineTask<int>> tasks;

    tasks.push_back([]() -> CoroutineTask<int> {
        co_return 1;
    }());

    tasks.push_back([]() -> CoroutineTask<int> {
        throw std::runtime_error("Task failed");
        co_return 2;
    }());

    tasks.push_back([]() -> CoroutineTask<int> {
        co_return 3;
    }());

    auto result = when_all(tasks);

    FATP_ASSERT_FALSE(result.has_value(), "when_all should fail if any task fails");
    return true;
}

FATP_TEST_CASE(when_any_first_succeeds)
{
    std::vector<CoroutineTask<int>> tasks;

    tasks.push_back([]() -> CoroutineTask<int> {
        co_return 42;
    }());

    tasks.push_back([]() -> CoroutineTask<int> {
        co_return 100;
    }());

    auto result = when_any(tasks);

    FATP_ASSERT_TRUE(result.has_value(), "when_any should succeed");
    FATP_ASSERT_EQ(result.value(), 42, "Should return first successful result");
    return true;
}

FATP_TEST_CASE(when_any_all_fail)
{
    std::vector<CoroutineTask<int>> tasks;

    // Helper that always throws
    auto make_failing_task = []() -> CoroutineTask<int> {
        throw std::runtime_error("Task failed");
        co_return 0; // Unreachable, but required for coroutine return type
    };

    for (int i = 0; i < 3; ++i)
    {
        tasks.push_back(make_failing_task());
    }

    auto result = when_any(tasks);

    FATP_ASSERT_FALSE(result.has_value(), "when_any should fail if all tasks fail");
    return true;
}

// =============================================================================
// Move Semantics Tests
// =============================================================================

FATP_TEST_CASE(coroutine_task_move_constructor)
{
    auto make_task = []() -> CoroutineTask<int> {
        co_return 42;
    };

    auto task1 = make_task();
    FATP_ASSERT_TRUE(task1.valid(), "Original task should be valid");

    auto task2 = std::move(task1);
    FATP_ASSERT_FALSE(task1.valid(), "Moved-from task should be invalid");
    FATP_ASSERT_TRUE(task2.valid(), "Moved-to task should be valid");

    auto result = task2.await();
    FATP_ASSERT_TRUE(result.has_value(), "Moved task should work");
    FATP_ASSERT_EQ(result.value(), 42, "Moved task result");
    return true;
}

FATP_TEST_CASE(coroutine_task_move_assignment)
{
    auto make_task = [](int val) -> CoroutineTask<int> {
        co_return val;
    };

    auto task1 = make_task(10);
    auto task2 = make_task(20);

    task2 = std::move(task1);

    auto result = task2.await();
    FATP_ASSERT_TRUE(result.has_value(), "Move-assigned task should work");
    FATP_ASSERT_EQ(result.value(), 10, "Move-assigned task result");
    return true;
}

FATP_TEST_CASE(generator_move_semantics)
{
    auto make_gen = []() -> Generator<int> {
        for (int i = 0; i < 3; ++i)
        {
            co_yield i;
        }
    };

    auto gen1 = make_gen();
    auto gen2 = std::move(gen1);

    std::vector<int> values;
    for (int val : gen2)
    {
        values.push_back(val);
    }

    FATP_ASSERT_EQ(values.size(), 3u, "Moved generator should work");
    return true;
}

// =============================================================================
// State and Validity Tests
// =============================================================================

FATP_TEST_CASE(task_done_state)
{
    auto task = []() -> CoroutineTask<int> {
        co_return 42;
    }();

    FATP_ASSERT_FALSE(task.done(), "Task should not be done before await");

    auto result = task.await();

    FATP_ASSERT_TRUE(task.done(), "Task should be done after await");
    FATP_ASSERT_TRUE(result.has_value(), "Result should be valid");
    return true;
}

FATP_TEST_CASE(task_validity_check)
{
    auto task = []() -> CoroutineTask<int> {
        co_return 42;
    }();

    FATP_ASSERT_TRUE(task.valid(), "Newly created task should be valid");

    auto moved = std::move(task);

    FATP_ASSERT_FALSE(task.valid(), "Moved-from task should be invalid");
    FATP_ASSERT_TRUE(moved.valid(), "Moved-to task should be valid");
    return true;
}

// =============================================================================
// Performance Benchmarks
// =============================================================================
// =============================================================================
// Main Test Driver
// =============================================================================

} // namespace fat_p::testing::coroutinetask

namespace fat_p::testing
{


void run_benchmarks()
{
    using namespace fat_p::testing;

    auto& out = *get_test_config().output;
    out << "\n" << colors::cyan() << "Sanity Benchmark:" << colors::reset() << "\n";
    out << "  (benchmarks are provided by benchmark executables; unit tests do not run full benchmarks)\n\n";
}

bool test_CoroutineTask()
{
    FATP_PRINT_HEADER(COROUTINE TASK)

    TestRunner runner;

    auto& config = get_test_config();
    config.verbose = true;

    auto& out = *config.output;
    out << colors::yellow() << "(C++20 Coroutines)" << colors::reset() << "\n\n";

    // Basic Coroutine Tests
    out << colors::blue() << "--- Basic Coroutine Tests ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, coroutinetask, simple_coroutine);
    FATP_RUN_TEST_NS(runner, coroutinetask, coroutine_with_computation);
    FATP_RUN_TEST_NS(runner, coroutinetask, coroutine_with_string);
    FATP_RUN_TEST_NS(runner, coroutinetask, coroutine_with_complex_type);

    // Error Handling
    out << "\n" << colors::blue() << "--- Error Handling ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, coroutinetask, coroutine_exception_handling);
    FATP_RUN_TEST_NS(runner, coroutinetask, invalid_handle_error);

    // Eager vs Lazy
    out << "\n" << colors::blue() << "--- Eager vs Lazy Execution ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, coroutinetask, eager_task_executes_immediately);
    FATP_RUN_TEST_NS(runner, coroutinetask, lazy_task_waits_for_await);

    // Generator Tests
    out << "\n" << colors::blue() << "--- Generator Tests ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, coroutinetask, generator_simple_sequence);
    FATP_RUN_TEST_NS(runner, coroutinetask, generator_fibonacci);
    FATP_RUN_TEST_NS(runner, coroutinetask, generator_string_values);

    // Task Composition
    out << "\n" << colors::blue() << "--- Task Composition ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, coroutinetask, when_all_success);
    FATP_RUN_TEST_NS(runner, coroutinetask, when_all_failure);
    FATP_RUN_TEST_NS(runner, coroutinetask, when_any_first_succeeds);
    FATP_RUN_TEST_NS(runner, coroutinetask, when_any_all_fail);

    // Move Semantics
    out << "\n" << colors::blue() << "--- Move Semantics ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, coroutinetask, coroutine_task_move_constructor);
    FATP_RUN_TEST_NS(runner, coroutinetask, coroutine_task_move_assignment);
    FATP_RUN_TEST_NS(runner, coroutinetask, generator_move_semantics);

    // State and Validity
    out << "\n" << colors::blue() << "--- State and Validity ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, coroutinetask, task_done_state);
    FATP_RUN_TEST_NS(runner, coroutinetask, task_validity_check);

    // Performance Benchmarks

    // Summary
    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

#else // !FATP_HAS_COROUTINES

namespace fat_p::testing
{

bool test_CoroutineTask()
{
    FATP_PRINT_HEADER(COROUTINE TASK)

    auto& out = *get_test_config().output;
    out << colors::yellow() << colors::bold() << "=== CoroutineTask Tests Skipped ===" << colors::reset() << "\n";
    out << colors::yellow() << "Coroutines require library support (__cpp_lib_coroutine)." << colors::reset() << "\n";
    return true;
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_CoroutineTask() ? 0 : 1;
}
#endif

#endif // FATP_HAS_COROUTINES

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_CoroutineTask() ? 0 : 1;
}
#endif
