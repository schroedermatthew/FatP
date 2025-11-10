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

#include "test_Utilities.h"
#include <iostream>

// Only run tests if C++20 or later
#if __cplusplus >= 202002L

#include "CoroutineTask.h"
#include <vector>
#include <string>
#include <numeric>

namespace cpp_utilities::testing
{

// =============================================================================
// Basic Coroutine Tests
// =============================================================================

TEST_CASE(simple_coroutine) {
    // Simple coroutine that returns a value
    auto simple_task = []() -> CoroutineTask<int> {
        co_return 42;
    };
    
    auto task = simple_task();
    auto result = task.await();
    
    ASSERT_TRUE(result.has_value(), "Coroutine should return expected value");
    ASSERT_EQ(result.value(), 42, "Coroutine should return correct value");
    return true;
}

TEST_CASE(coroutine_with_computation) {
    // Coroutine that performs computation
    auto compute_sum = [](int a, int b) -> CoroutineTask<int> {
        int result = a + b;
        co_return result;
    };
    
    auto task = compute_sum(10, 20);
    auto result = task.await();
    
    ASSERT_TRUE(result.has_value(), "Computation coroutine should succeed");
    ASSERT_EQ(result.value(), 30, "Computation result should be correct");
    return true;
}

TEST_CASE(coroutine_with_string) {
    // Coroutine that returns string
    auto get_message = []() -> CoroutineTask<std::string> {
        co_return "Hello from coroutine";
    };
    
    auto task = get_message();
    auto result = task.await();
    
    ASSERT_TRUE(result.has_value(), "String coroutine should succeed");
    ASSERT_EQ(result.value(), "Hello from coroutine", "String value should be correct");
    return true;
}

TEST_CASE(coroutine_with_complex_type) {
    struct Data {
        int x;
        double y;
        std::string z;
        
        bool operator==(const Data& other) const {
            return x == other.x && y == other.y && z == other.z;
        }
    };
    
    auto create_data = []() -> CoroutineTask<Data> {
        Data d{42, 3.14, "test"};
        co_return d;
    };
    
    auto task = create_data();
    auto result = task.await();
    
    ASSERT_TRUE(result.has_value(), "Complex type coroutine should succeed");
    ASSERT_EQ(result.value().x, 42, "Complex type field x");
    ASSERT_TRUE(std::abs(result.value().y - 3.14) < 1e-6, "Complex type field y");
    ASSERT_EQ(result.value().z, "test", "Complex type field z");
    return true;
}

// =============================================================================
// Error Handling Tests
// =============================================================================

TEST_CASE(coroutine_exception_handling) {
    // Coroutine that throws an exception
    auto throwing_task = []() -> CoroutineTask<int> {
        throw std::runtime_error("Test exception");
        co_return 0; // Never reached
    };
    
    auto task = throwing_task();
    auto result = task.await();
    
    ASSERT_FALSE(result.has_value(), "Exception should result in error");
    return true;
}

TEST_CASE(invalid_handle_error) {
    CoroutineTask<int> task{nullptr};
    auto result = task.await();
    
    ASSERT_FALSE(result.has_value(), "Invalid handle should produce error");
    return true;
}

// =============================================================================
// Eager Task Tests
// =============================================================================

TEST_CASE(eager_task_executes_immediately) {
    int executed = 0;
    
    auto eager = [&executed]() -> EagerTask<int> {
        executed = 42;
        co_return executed;
    }();
    
    // Should already be executed
    ASSERT_TRUE(eager.done(), "Eager task should be done immediately");
    ASSERT_EQ(executed, 42, "Eager task should have executed");
    
    auto result = eager.result();
    ASSERT_TRUE(result.has_value(), "Eager task result should be valid");
    ASSERT_EQ(result.value(), 42, "Eager task result value");
    return true;
}

TEST_CASE(lazy_task_waits_for_await) {
    int executed = 0;
    
    auto lazy = [&executed]() -> CoroutineTask<int> {
        executed = 42;
        co_return executed;
    }();
    
    // Should not be executed yet
    ASSERT_EQ(executed, 0, "Lazy task should not have executed yet");
    
    auto result = lazy.await();
    
    // Now should be executed
    ASSERT_EQ(executed, 42, "Lazy task should execute on await");
    ASSERT_TRUE(result.has_value(), "Lazy task result should be valid");
    ASSERT_EQ(result.value(), 42, "Lazy task result value");
    return true;
}

// =============================================================================
// Generator Tests
// =============================================================================

TEST_CASE(generator_simple_sequence) {
    auto range = [](int n) -> Generator<int> {
        for (int i = 0; i < n; ++i) {
            co_yield i;
        }
    };
    
    auto gen = range(5);
    std::vector<int> values;
    
    for (int val : gen) {
        values.push_back(val);
    }
    
    ASSERT_EQ(values.size(), 5u, "Generator should produce 5 values");
    ASSERT_EQ(values[0], 0, "First value");
    ASSERT_EQ(values[4], 4, "Last value");
    return true;
}

TEST_CASE(generator_fibonacci) {
    auto fibonacci = [](int n) -> Generator<int> {
        int a = 0, b = 1;
        for (int i = 0; i < n; ++i) {
            co_yield a;
            int next = a + b;
            a = b;
            b = next;
        }
    };
    
    auto gen = fibonacci(6);
    std::vector<int> fib_sequence;
    
    for (int val : gen) {
        fib_sequence.push_back(val);
    }
    
    ASSERT_EQ(fib_sequence.size(), 6u, "Should generate 6 fibonacci numbers");
    ASSERT_EQ(fib_sequence[0], 0, "fib[0]");
    ASSERT_EQ(fib_sequence[1], 1, "fib[1]");
    ASSERT_EQ(fib_sequence[2], 1, "fib[2]");
    ASSERT_EQ(fib_sequence[3], 2, "fib[3]");
    ASSERT_EQ(fib_sequence[4], 3, "fib[4]");
    ASSERT_EQ(fib_sequence[5], 5, "fib[5]");
    return true;
}

TEST_CASE(generator_string_values) {
    auto string_gen = []() -> Generator<std::string> {
        co_yield "first";
        co_yield "second";
        co_yield "third";
    };
    
    auto gen = string_gen();
    std::vector<std::string> strings;
    
    for (auto& str : gen) {
        strings.push_back(str);
    }
    
    ASSERT_EQ(strings.size(), 3u, "Should generate 3 strings");
    ASSERT_EQ(strings[0], "first", "First string");
    ASSERT_EQ(strings[1], "second", "Second string");
    ASSERT_EQ(strings[2], "third", "Third string");
    return true;
}

// =============================================================================
// Task Composition Tests
// =============================================================================

TEST_CASE(when_all_success) {
    std::vector<CoroutineTask<int>> tasks;
    
    for (int i = 0; i < 3; ++i) {
        tasks.push_back([i]() -> CoroutineTask<int> {
            co_return i * 10;
        }());
    }
    
    auto result = when_all(tasks);
    
    ASSERT_TRUE(result.has_value(), "when_all should succeed");
    ASSERT_EQ(result.value().size(), 3u, "Should have 3 results");
    ASSERT_EQ(result.value()[0], 0, "First result");
    ASSERT_EQ(result.value()[1], 10, "Second result");
    ASSERT_EQ(result.value()[2], 20, "Third result");
    return true;
}

TEST_CASE(when_all_failure) {
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
    
    ASSERT_FALSE(result.has_value(), "when_all should fail if any task fails");
    return true;
}

TEST_CASE(when_any_first_succeeds) {
    std::vector<CoroutineTask<int>> tasks;
    
    tasks.push_back([]() -> CoroutineTask<int> {
        co_return 42;
    }());
    
    tasks.push_back([]() -> CoroutineTask<int> {
        co_return 100;
    }());
    
    auto result = when_any(tasks);
    
    ASSERT_TRUE(result.has_value(), "when_any should succeed");
    ASSERT_EQ(result.value(), 42, "Should return first successful result");
    return true;
}

TEST_CASE(when_any_all_fail) {
    std::vector<CoroutineTask<int>> tasks;
    
    for (int i = 0; i < 3; ++i) {
        tasks.push_back([i]() -> CoroutineTask<int> {
            throw std::runtime_error("Task failed");
            co_return i;
        }());
    }
    
    auto result = when_any(tasks);
    
    ASSERT_FALSE(result.has_value(), "when_any should fail if all tasks fail");
    return true;
}

// =============================================================================
// Move Semantics Tests
// =============================================================================

TEST_CASE(coroutine_task_move_constructor) {
    auto make_task = []() -> CoroutineTask<int> {
        co_return 42;
    };
    
    auto task1 = make_task();
    ASSERT_TRUE(task1.valid(), "Original task should be valid");
    
    auto task2 = std::move(task1);
    ASSERT_FALSE(task1.valid(), "Moved-from task should be invalid");
    ASSERT_TRUE(task2.valid(), "Moved-to task should be valid");
    
    auto result = task2.await();
    ASSERT_TRUE(result.has_value(), "Moved task should work");
    ASSERT_EQ(result.value(), 42, "Moved task result");
    return true;
}

TEST_CASE(coroutine_task_move_assignment) {
    auto make_task = [](int val) -> CoroutineTask<int> {
        co_return val;
    };
    
    auto task1 = make_task(10);
    auto task2 = make_task(20);
    
    task2 = std::move(task1);
    
    auto result = task2.await();
    ASSERT_TRUE(result.has_value(), "Move-assigned task should work");
    ASSERT_EQ(result.value(), 10, "Move-assigned task result");
    return true;
}

TEST_CASE(generator_move_semantics) {
    auto make_gen = []() -> Generator<int> {
        for (int i = 0; i < 3; ++i) {
            co_yield i;
        }
    };
    
    auto gen1 = make_gen();
    auto gen2 = std::move(gen1);
    
    std::vector<int> values;
    for (int val : gen2) {
        values.push_back(val);
    }
    
    ASSERT_EQ(values.size(), 3u, "Moved generator should work");
    return true;
}

// =============================================================================
// State and Validity Tests
// =============================================================================

TEST_CASE(task_done_state) {
    auto task = []() -> CoroutineTask<int> {
        co_return 42;
    }();
    
    ASSERT_FALSE(task.done(), "Task should not be done before await");
    
    auto result = task.await();
    
    ASSERT_TRUE(task.done(), "Task should be done after await");
    ASSERT_TRUE(result.has_value(), "Result should be valid");
    return true;
}

TEST_CASE(task_validity_check) {
    auto task = []() -> CoroutineTask<int> {
        co_return 42;
    }();
    
    ASSERT_TRUE(task.valid(), "Newly created task should be valid");
    
    auto moved = std::move(task);
    
    ASSERT_FALSE(task.valid(), "Moved-from task should be invalid");
    ASSERT_TRUE(moved.valid(), "Moved-to task should be valid");
    return true;
}

// =============================================================================
// Performance Benchmarks
// =============================================================================

void run_coroutine_benchmarks() {
    auto& out = *get_test_config().output;
    
    out << "\n" << colors::cyan() << colors::bold() 
        << "=== CoroutineTask Performance Benchmarks ===" 
        << colors::reset() << "\n\n";
    
    // Simple coroutine overhead
    out << colors::blue() << "--- Coroutine Overhead ---" << colors::reset() << "\n";
    {
        benchmark("Simple coroutine creation and execution", []() {
            auto task = []() -> CoroutineTask<int> {
                co_return 42;
            }();
            volatile auto result = task.await();
        }, 100000);
    }
    
    // Eager vs Lazy
    out << "\n" << colors::blue() << "--- Eager vs Lazy ---" << colors::reset() << "\n";
    {
        benchmark("Lazy task", []() {
            auto task = []() -> CoroutineTask<int> {
                co_return 42;
            }();
            volatile auto result = task.await();
        }, 100000);
        
        benchmark("Eager task", []() {
            auto task = []() -> EagerTask<int> {
                co_return 42;
            }();
            volatile auto result = task.result();
        }, 100000);
    }
    
    // Generator iteration
    out << "\n" << colors::blue() << "--- Generator ---" << colors::reset() << "\n";
    {
        benchmark("Generator iteration (10 items)", []() {
            auto gen = []() -> Generator<int> {
                for (int i = 0; i < 10; ++i) {
                    co_yield i;
                }
            }();
            
            int sum = 0;
            for (int val : gen) {
                sum += val;
            }
            volatile int result = sum;
        }, 50000);
    }
    
    out << "\n";
}

// =============================================================================
// Main Test Driver
// =============================================================================

bool test_CoroutineTask() {

    PRINT_HEADER(COROUTINE TASK)

    TestRunner runner;
    
    auto& config = get_test_config();
    config.verbose = true;
    
    auto& out = *config.output;
    out << colors::yellow() << "(C++20 Coroutines)" 
        << colors::reset() << "\n\n";
    
    // Basic Coroutine Tests
    out << colors::blue() << "--- Basic Coroutine Tests ---" << colors::reset() << "\n";
    RUN_TEST(runner, simple_coroutine);
    RUN_TEST(runner, coroutine_with_computation);
    RUN_TEST(runner, coroutine_with_string);
    RUN_TEST(runner, coroutine_with_complex_type);
    
    // Error Handling
    out << "\n" << colors::blue() << "--- Error Handling ---" << colors::reset() << "\n";
    RUN_TEST(runner, coroutine_exception_handling);
    RUN_TEST(runner, invalid_handle_error);
    
    // Eager vs Lazy
    out << "\n" << colors::blue() << "--- Eager vs Lazy Execution ---" << colors::reset() << "\n";
    RUN_TEST(runner, eager_task_executes_immediately);
    RUN_TEST(runner, lazy_task_waits_for_await);
    
    // Generator Tests
    out << "\n" << colors::blue() << "--- Generator Tests ---" << colors::reset() << "\n";
    RUN_TEST(runner, generator_simple_sequence);
    RUN_TEST(runner, generator_fibonacci);
    RUN_TEST(runner, generator_string_values);
    
    // Task Composition
    out << "\n" << colors::blue() << "--- Task Composition ---" << colors::reset() << "\n";
    RUN_TEST(runner, when_all_success);
    RUN_TEST(runner, when_all_failure);
    RUN_TEST(runner, when_any_first_succeeds);
    RUN_TEST(runner, when_any_all_fail);
    
    // Move Semantics
    out << "\n" << colors::blue() << "--- Move Semantics ---" << colors::reset() << "\n";
    RUN_TEST(runner, coroutine_task_move_constructor);
    RUN_TEST(runner, coroutine_task_move_assignment);
    RUN_TEST(runner, generator_move_semantics);
    
    // State and Validity
    out << "\n" << colors::blue() << "--- State and Validity ---" << colors::reset() << "\n";
    RUN_TEST(runner, task_done_state);
    RUN_TEST(runner, task_validity_check);
    
    // Performance Benchmarks
    run_coroutine_benchmarks();
    
    // Summary
    return 0 == runner.print_summary();
}

} // namespace cpp_utilities::testing

#else // __cplusplus < 202002L

namespace cpp_utilities::testing
{

bool test_CoroutineTask() {

    PRINT_HEADER(COROUTINE TASK)

    auto& out = *get_test_config().output;
    out << colors::yellow() << colors::bold() 
        << "=== CoroutineTask Tests Skipped ===" 
        << colors::reset() << "\n";
    out << colors::yellow() 
        << "Coroutines require C++20 or later. Current standard: C++" 
        << (__cplusplus / 100 % 100) 
        << colors::reset() << "\n";
    return true;
}

} // namespace cpp_utilities::testing

#endif // __cplusplus >= 202002L
