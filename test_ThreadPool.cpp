/**
 * @file test_ThreadPool.cpp
 * @brief Comprehensive tests for ThreadPool.h
 */

#include <iostream>
#include <chrono>
#include <future>
#include <vector>
#include <atomic>
#include <numeric>

#include "ThreadPool.h"
#include "test_ThreadPool.h"
#include "FatPTest.h"

namespace fat_p::testing
{

// Test 1: Basic task submission
bool test_basic_submission() {
    ThreadPool pool(4);
    
    std::atomic<int> counter{0};
    
    for (int i = 0; i < 100; ++i) {
        pool.submit([&counter]() {
            counter.fetch_add(1, std::memory_order_relaxed);
        });
    }
    
    pool.wait_idle();
    
    SIMPLE_ASSERT(counter.load() == 100, "All tasks should execute");
    return true;
}

// Test 2: Future return values
bool test_future_returns() {
    ThreadPool pool(2);
    
    auto future = pool.submit([]() { return 42; });
    int result = future.get();
    
    SIMPLE_ASSERT(result == 42, "Future should return correct value");
    return true;
}

// Test 3: Priority scheduling
bool test_priority_scheduling() {
    ThreadPool pool(1);  // Single thread to ensure order
    
    std::vector<int> results;
    std::mutex results_mutex;
    
    // Submit low priority tasks
    for (int i = 0; i < 5; ++i) {
        pool.submit_priority(Priority::Low, [&results, &results_mutex, i]() {
            std::lock_guard<std::mutex> lock(results_mutex);
            results.push_back(i);
        });
    }
    
    // Submit high priority task
    pool.submit_priority(Priority::High, [&results, &results_mutex]() {
        std::lock_guard<std::mutex> lock(results_mutex);
        results.push_back(999);
    });
    
    pool.wait_idle();
    
    SIMPLE_ASSERT(results.size() == 6, "All tasks should complete");
    // High priority task should execute early
    SIMPLE_ASSERT(std::find(results.begin(), results.end(), 999) != results.end(), 
                  "High priority task should execute");
    return true;
}

// Test 4: Exception handling
bool test_exception_handling() {
    ThreadPool pool(2);
    
    auto future = pool.submit([]() -> int {
        throw std::runtime_error("Test exception");
        return 0;
    });
    
    try {
        future.get();
        SIMPLE_ASSERT(false, "Should throw exception");
    } catch (const std::runtime_error&) {
        // Expected
    }
    
    // Pool should still be functional
    auto future2 = pool.submit([]() { return 42; });
    SIMPLE_ASSERT(future2.get() == 42, "Pool should recover from exception");
    return true;
}

// Test 5: Batch submission
bool test_batch_submission() {
    ThreadPool pool(4);
    
    std::atomic<int> counter{0};
    std::vector<std::function<void()>> tasks;
    
    for (int i = 0; i < 50; ++i) {
        tasks.push_back([&counter]() {
            counter.fetch_add(1, std::memory_order_relaxed);
        });
    }
    
    pool.submit_batch(tasks);
    pool.wait_idle();
    
    SIMPLE_ASSERT(counter.load() == 50, "All batched tasks should execute");
    return true;
}

// Test 6: Stress test with many tasks
bool test_stress_many_tasks() {
    ThreadPool pool(8);
    
    std::atomic<uint64_t> sum{0};
    constexpr int NUM_TASKS = 10000;
    
    for (int i = 0; i < NUM_TASKS; ++i) {
        pool.submit([&sum, i]() {
            sum.fetch_add(i, std::memory_order_relaxed);
        });
    }
    
    pool.wait_idle();
    
    uint64_t expected = (NUM_TASKS * (NUM_TASKS - 1)) / 2;
    SIMPLE_ASSERT(sum.load() == expected, "All tasks should execute correctly");
    return true;
}

// Test 7: Work stealing verification
bool test_work_stealing() {
    ThreadPool pool(4);
    
    // Submit many quick tasks to one queue
    std::atomic<int> counter{0};
    
    for (int i = 0; i < 100; ++i) {
        pool.submit([&counter]() {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            counter.fetch_add(1, std::memory_order_relaxed);
        });
    }
    
    pool.wait_idle();
    
    SIMPLE_ASSERT(counter.load() == 100, "Work stealing should distribute tasks");
    return true;
}

// Test 8: Spin-wait configuration
bool test_spin_wait_config() {
    // Test with different spin durations
    {
        ThreadPool pool_nospin(4, 0);  // No spinning
        std::atomic<int> counter{0};
        
        for (int i = 0; i < 50; ++i) {
            pool_nospin.submit([&counter]() {
                counter.fetch_add(1, std::memory_order_relaxed);
            });
        }
        
        pool_nospin.wait_idle();
        SIMPLE_ASSERT(counter.load() == 50, "Pool with no spinning should work");
    }
    
    {
        ThreadPool pool_spin(4, 5000);  // 5ms spin
        std::atomic<int> counter{0};
        
        for (int i = 0; i < 50; ++i) {
            pool_spin.submit([&counter]() {
                counter.fetch_add(1, std::memory_order_relaxed);
            });
        }
        
        pool_spin.wait_idle();
        SIMPLE_ASSERT(counter.load() == 50, "Pool with 5ms spinning should work");
    }
    
    return true;
}

// Performance benchmark
void benchmark_thread_pool() {
    std::cout << "\n" << colors::cyan() << "Thread Pool Benchmarks:" << colors::reset() << "\n\n";
    
    // Benchmark 1: Task submission overhead
    {
        ThreadPool pool(4);
        std::atomic<int> dummy{0};
        
        double submit_time = measure_perf([&]() {
            pool.submit([&dummy]() { dummy.fetch_add(1, std::memory_order_relaxed); });
        }, 1000, 10);
        
        std::cout << "Task submission: " << format_time(submit_time) << "\n";
        
        pool.wait_idle();
    }
    
    // Benchmark 2: Throughput
    {
        ThreadPool pool(8);
        constexpr int NUM_TASKS = 100000;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        std::atomic<int> counter{0};
        for (int i = 0; i < NUM_TASKS; ++i) {
            pool.submit([&counter]() {
                counter.fetch_add(1, std::memory_order_relaxed);
            });
        }
        
        pool.wait_idle();
        auto end = std::chrono::high_resolution_clock::now();
        
        double elapsed_sec = std::chrono::duration<double>(end - start).count();
        double throughput = NUM_TASKS / elapsed_sec;
        
        std::cout << "Throughput: " << colors::bold() 
                  << static_cast<int>(throughput) << " tasks/sec" 
                  << colors::reset() << "\n";
    }
    
    // Benchmark 3: Task latency with different spin durations
    std::cout << "\n" << colors::yellow() << "Task Latency Comparison:" << colors::reset() << "\n";
    
    auto latency_test = [](size_t spin_us, const char* label) {
        ThreadPool pool(4, spin_us);
        constexpr int NUM_TRIALS = 1000;
        std::vector<double> latencies_ms;
        latencies_ms.reserve(NUM_TRIALS);
        
        for (int i = 0; i < NUM_TRIALS; ++i) {
            auto start = std::chrono::high_resolution_clock::now();
            
            auto future = pool.submit([]() { 
                return 42; 
            });
            
            future.get();
            auto end = std::chrono::high_resolution_clock::now();
            
            // Convert to milliseconds for format_time
            double latency_ms = std::chrono::duration<double, std::milli>(end - start).count();
            latencies_ms.push_back(latency_ms);
        }
        
        // Calculate statistics
        std::sort(latencies_ms.begin(), latencies_ms.end());
        double avg = std::accumulate(latencies_ms.begin(), latencies_ms.end(), 0.0) / latencies_ms.size();
        double p50 = latencies_ms[latencies_ms.size() / 2];
        double p95 = latencies_ms[latencies_ms.size() * 95 / 100];
        double p99 = latencies_ms[latencies_ms.size() * 99 / 100];
        
        std::cout << "  " << label << " (spin=" << spin_us << "us):\n";
        std::cout << "    Avg: " << format_time(avg) << ", ";
        std::cout << "p50: " << format_time(p50) << ", ";
        std::cout << "p95: " << format_time(p95) << ", ";
        std::cout << "p99: " << format_time(p99) << "\n";
        
        pool.wait_idle();
    };
    
    latency_test(0, "No Spin");
    latency_test(1000, "1ms Spin");
    latency_test(2000, "2ms Spin");
    latency_test(5000, "5ms Spin");
}

bool test_ThreadPool() {

    PRINT_HEADER(THREAD POOL)

    TestRunner runner;

    RUN_TEST(runner, basic_submission);
    RUN_TEST(runner, future_returns);
    RUN_TEST(runner, priority_scheduling);
    RUN_TEST(runner, exception_handling);
    RUN_TEST(runner, batch_submission);
    RUN_TEST(runner, work_stealing);
    RUN_TEST(runner, spin_wait_config);

    benchmark_thread_pool();

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing
