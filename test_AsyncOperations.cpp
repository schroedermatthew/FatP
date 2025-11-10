#include <iostream>
#include <string>
#include <chrono>
#include <thread>

#include "AsyncOperations.h"
#include "Expected.h"
#include "test_AsyncOperations.h"
#include "test_Utilities.h"

namespace cpp_utilities::testing
{

bool test_basic_task() {
    auto task = async_task([]() -> Expected<int, std::string> {
        return 42;
    });
    
    auto result = task.wait();
    SIMPLE_ASSERT(result.has_value(), "Task should succeed");
    SIMPLE_ASSERT(*result == 42, "Result should be 42");
    
    return true;
}

bool test_continuation() {
    auto task = async_task([]() -> Expected<int, std::string> {
        return 10;
    }).then([](int val) -> Expected<int, std::string> {
        return val * 2;
    });
    
    auto result = task.wait();
    SIMPLE_ASSERT(result.has_value(), "Task should succeed");
    SIMPLE_ASSERT(*result == 20, "Result should be 20");
    
    return true;
}

bool test_error_handling() {
    bool error_called = false;
    
    auto task = async_task([]() -> Expected<int, std::string> {
        return unexpected<std::string>("error occurred");
    }).error([&](const std::string& err) {
        error_called = true;
    });
    
    auto result = task.wait();
    SIMPLE_ASSERT(!result.has_value(), "Task should fail");
    SIMPLE_ASSERT(error_called, "Error handler should be called");
    
    return true;
}

bool test_chained_continuations() {
    auto task = async_task([]() -> Expected<int, std::string> {
        return 5;
    }).then([](int val) -> Expected<int, std::string> {
        return val + 10;
    }).then([](int val) -> Expected<int, std::string> {
        return val * 2;
    });
    
    auto result = task.wait();
    SIMPLE_ASSERT(result.has_value(), "Task should succeed");
    SIMPLE_ASSERT(*result == 30, "Result should be 30 ((5+10)*2)");
    
    return true;
}

bool test_error_propagation() {
    auto task = async_task([]() -> Expected<int, std::string> {
        return unexpected<std::string>("initial error");
    }).then([](int val) -> Expected<int, std::string> {
        return val * 2; // Should not execute
    });
    
    auto result = task.wait();
    SIMPLE_ASSERT(!result.has_value(), "Task should fail");
    SIMPLE_ASSERT(result.error() == "initial error", "Error should propagate");
    
    return true;
}

bool test_poll() {
    auto task = async_task([]() -> Expected<int, std::string> {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return 42;
    });
    
    auto early_result = task.poll();
    // May or may not be ready depending on timing
    
    auto final_result = task.wait();
    SIMPLE_ASSERT(final_result.has_value(), "Final wait should succeed");
    
    return true;
}

bool test_valid() {
    auto task = async_task([]() -> Expected<int, std::string> {
        return 42;
    });
    
    SIMPLE_ASSERT(task.valid(), "Task should be valid before wait");
    
    (void)task.wait();
    
    // After wait, validity depends on implementation
    
    return true;
}

void benchmark_asyncoperations() {
    std::cout << "\n" << colors::cyan() << "AsyncOperations Benchmarks:" << colors::reset() << "\n\n";
    
    // Benchmark task creation and wait
    double task_time = measure_perf([]() {
        auto task = async_task([]() -> Expected<int, std::string> {
            return 42;
        });
        auto result = task.wait();
        DoNotOptimize(result);
    }, 1000, 10);
    std::cout << "Task creation + wait: " << format_time(task_time) << "\n";
    
    // Benchmark with continuation
    double chain_time = measure_perf([]() {
        auto task = async_task([]() -> Expected<int, std::string> {
            return 10;
        }).then([](int val) -> Expected<int, std::string> {
            return val * 2;
        });
        auto result = task.wait();
        DoNotOptimize(result);
    }, 1000, 10);
    std::cout << "Task with continuation: " << format_time(chain_time) << "\n";
}

bool test_AsyncOperations() {

    PRINT_HEADER(ASYNC OPERATIONS)

    TestRunner runner;

    RUN_TEST(runner, basic_task);
    RUN_TEST(runner, continuation);
    RUN_TEST(runner, error_handling);
    RUN_TEST(runner, chained_continuations);
    RUN_TEST(runner, error_propagation);
    RUN_TEST(runner, poll);
    RUN_TEST(runner, valid);

    benchmark_asyncoperations();

    return 0 == runner.print_summary();
}

} // namespace cpp_utilities::testing
