/**
 * @file test_ThreadPool.cpp
 * @brief Comprehensive tests for ThreadPool.h
 *
 * Tests cover:
 * - Basic task submission and execution
 * - Future return values and exception propagation
 * - Priority scheduling order verification
 * - Batch submission efficiency
 * - Work stealing distribution
 * - Shutdown behavior
 * - Stress testing with many tasks
 *
 * @version 2.0
 */
/*
FATP_META:
  meta_version: 1
  component: ThreadPool
  file_role: test
  path: components/ThreadPool/tests/test_ThreadPool.cpp
  layer: Testing
  namespace: fat_p
  summary: "Unit tests for ThreadPool."
  api_stability: in_work
  related:
    docs_search: "ThreadPool"
    headers:
      - include/fat_p/ThreadPool.h
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

#include <algorithm>
#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <numeric>
#include <thread>
#include <vector>

#include "FatPTest.h"
#include "ThreadPool.h"

// ============================================================================
// Tests in nested namespace per Fat-P guidelines
// ============================================================================

namespace fat_p::testing::thread_pool
{

// ----------------------------------------------------------------------------
// Test: Basic task submission and execution
// ----------------------------------------------------------------------------
FATP_TEST_CASE(basic_submission)
{
    ThreadPool pool(4);
    std::atomic<int> counter{0};

    for (int i = 0; i < 100; ++i)
    {
        (void)pool.submit([&counter]() {
            counter.fetch_add(1, std::memory_order_relaxed);
        });
    }

    pool.wait_idle();

    FATP_ASSERT_EQ(counter.load(), 100, "All 100 tasks should execute");
    return true;
}

// ----------------------------------------------------------------------------
// Test: Future return values
// ----------------------------------------------------------------------------
FATP_TEST_CASE(future_returns)
{
    ThreadPool pool(2);

    auto future1 = pool.submit([]() {
        return 42;
    });
    auto future2 = pool.submit([]() {
        return std::string("hello");
    });
    auto future3 = pool.submit(
        [](int a, int b) {
            return a + b;
        },
        10,
        20);

    FATP_ASSERT_EQ(future1.get(), 42, "Integer return should work");
    FATP_ASSERT_EQ(future2.get(), "hello", "String return should work");
    FATP_ASSERT_EQ(future3.get(), 30, "Arguments should be forwarded");

    return true;
}

// ----------------------------------------------------------------------------
// Test: Exception propagation through futures
// ----------------------------------------------------------------------------
FATP_TEST_CASE(exception_handling)
{
    ThreadPool pool(2);

    auto future = pool.submit([]() -> int {
        throw std::runtime_error("Test exception");
    });

    bool caught = false;
    try
    {
        future.get();
    }
    catch (const std::runtime_error& e)
    {
        caught = true;
        FATP_ASSERT_EQ(std::string(e.what()), std::string("Test exception"), "Exception message should propagate");
    }

    FATP_ASSERT_TRUE(caught, "Exception should propagate through future.get()");

    // Pool should remain healthy after exception
    auto recovery_future = pool.submit([]() {
        return 123;
    });
    FATP_ASSERT_EQ(recovery_future.get(), 123, "Pool should recover from exception");

    return true;
}

// ----------------------------------------------------------------------------
// Test: Priority scheduling order verification
// ----------------------------------------------------------------------------
FATP_TEST_CASE(priority_scheduling)
{
    // Single thread to force deterministic scheduling decisions
    ThreadPool pool(1);

    std::vector<int> results;
    std::mutex results_mutex;
    std::atomic<bool> release_blocker{false};

    // 1. Submit a blocking task to hold the worker (Critical to go to global queue first)
    (void)pool.submit_priority(Priority::Critical, [&release_blocker]() {
        while (!release_blocker.load(std::memory_order_acquire))
        {
            std::this_thread::yield();
        }
    });

    // Small delay to ensure blocker is picked up
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // 2. Queue tasks with different priorities - ALL High+ to go to global queue
    //    Note: Normal/Low go to local queues (different scheduling path)
    //    This test verifies priority ordering within the global queue only
    (void)pool.submit_priority(Priority::High, [&results, &results_mutex]() {
        std::lock_guard<std::mutex> lock(results_mutex);
        results.push_back(100); // High priority
    });

    (void)pool.submit_priority(Priority::Critical, [&results, &results_mutex]() {
        std::lock_guard<std::mutex> lock(results_mutex);
        results.push_back(200); // Critical priority
    });

    (void)pool.submit_priority(Priority::High, [&results, &results_mutex]() {
        std::lock_guard<std::mutex> lock(results_mutex);
        results.push_back(101); // High priority (submitted after 100)
    });

    // 3. Release the blocker
    release_blocker.store(true, std::memory_order_release);
    pool.wait_idle();

    FATP_ASSERT_EQ(results.size(), 3u, "All 3 tasks should complete");

    // Critical (200) should be first
    FATP_ASSERT_EQ(results[0], 200, "Critical priority task should execute first");

    // High priority tasks should follow in FIFO order (100, then 101)
    FATP_ASSERT_EQ(results[1], 100, "First High priority task should execute second");
    FATP_ASSERT_EQ(results[2], 101, "Second High priority task should execute third");

    return true;
}

// ----------------------------------------------------------------------------
// Test: Batch submission with single notification
// ----------------------------------------------------------------------------
FATP_TEST_CASE(batch_submission)
{
    ThreadPool pool(4);
    std::atomic<int> counter{0};
    std::vector<std::function<void()>> tasks;

    for (int i = 0; i < 100; ++i)
    {
        tasks.emplace_back([&counter]() {
            counter.fetch_add(1, std::memory_order_relaxed);
        });
    }

    pool.submit_batch(tasks);
    pool.wait_idle();

    FATP_ASSERT_EQ(counter.load(), 100, "All batched tasks should execute");

    return true;
}

// ----------------------------------------------------------------------------
// Test: Stress test with many tasks
// ----------------------------------------------------------------------------
FATP_TEST_CASE(stress_many_tasks)
{
    ThreadPool pool(8);
    std::atomic<uint64_t> sum{0};
    constexpr int NUM_TASKS = 10000;

    for (int i = 0; i < NUM_TASKS; ++i)
    {
        (void)pool.submit([&sum, i]() {
            sum.fetch_add(static_cast<uint64_t>(i), std::memory_order_relaxed);
        });
    }

    pool.wait_idle();

    uint64_t expected = static_cast<uint64_t>(NUM_TASKS) * (NUM_TASKS - 1) / 2;
    FATP_ASSERT_EQ(sum.load(), expected, "Sum should match expected value");

    return true;
}

// ----------------------------------------------------------------------------
// Test: Work stealing distribution
// ----------------------------------------------------------------------------
FATP_TEST_CASE(work_stealing)
{
    ThreadPool pool(4);
    std::atomic<int> counter{0};

    // Submit many tasks that take some time
    for (int i = 0; i < 100; ++i)
    {
        (void)pool.submit([&counter]() {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            counter.fetch_add(1, std::memory_order_relaxed);
        });
    }

    pool.wait_idle();

    FATP_ASSERT_EQ(counter.load(), 100, "All tasks should complete via work stealing");

    return true;
}

// ----------------------------------------------------------------------------
// Test: Shutdown behavior waits for pending tasks
// ----------------------------------------------------------------------------
FATP_TEST_CASE(shutdown_behavior)
{
    std::atomic<int> completed{0};

    {
        ThreadPool pool(2);

        // Submit tasks that take some time
        for (int i = 0; i < 20; ++i)
        {
            (void)pool.submit([&completed]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                completed.fetch_add(1, std::memory_order_relaxed);
            });
        }

        // Destructor calls shutdown(), which should wait for all tasks
    }

    FATP_ASSERT_EQ(completed.load(), 20, "Shutdown should wait for all tasks to complete");

    return true;
}

// ----------------------------------------------------------------------------
// Test: Spin configuration works correctly
// ----------------------------------------------------------------------------
FATP_TEST_CASE(spin_configuration)
{
    // Test with no spinning
    {
        ThreadPool pool_nospin(4, 0);
        std::atomic<int> counter{0};

        for (int i = 0; i < 50; ++i)
        {
            (void)pool_nospin.submit([&counter]() {
                counter.fetch_add(1, std::memory_order_relaxed);
            });
        }

        pool_nospin.wait_idle();
        FATP_ASSERT_EQ(counter.load(), 50, "Pool with no spinning should work");
    }

    // Test with extended spinning
    {
        ThreadPool pool_spin(4, 5000); // 5ms spin
        std::atomic<int> counter{0};

        for (int i = 0; i < 50; ++i)
        {
            (void)pool_spin.submit([&counter]() {
                counter.fetch_add(1, std::memory_order_relaxed);
            });
        }

        pool_spin.wait_idle();
        FATP_ASSERT_EQ(counter.load(), 50, "Pool with 5ms spinning should work");
    }

    return true;
}

// ----------------------------------------------------------------------------
// Test: Thread count auto-detection
// ----------------------------------------------------------------------------
FATP_TEST_CASE(auto_thread_count)
{
    ThreadPool pool(0); // Should use hardware_concurrency

    FATP_ASSERT_GT(pool.thread_count(), 0u, "Should auto-detect thread count");
    FATP_ASSERT_TRUE(pool.thread_count() <= std::thread::hardware_concurrency() ||
                         pool.thread_count() == 2, // Fallback if hardware_concurrency returns 0
                     "Thread count should be reasonable");

    return true;
}

// ----------------------------------------------------------------------------
// Test: Pending and active task counters
// ----------------------------------------------------------------------------
FATP_TEST_CASE(task_counters)
{
    ThreadPool pool(2);
    std::atomic<bool> release{false};

    // Initially no tasks
    FATP_ASSERT_EQ(pool.pending_tasks(), 0, "Initially no pending tasks");
    FATP_ASSERT_EQ(pool.active_tasks(), 0, "Initially no active tasks");

    // Submit blocking task
    (void)pool.submit([&release]() {
        while (!release.load(std::memory_order_acquire))
        {
            std::this_thread::yield();
        }
    });

    // Give task time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Should have 1 active task
    FATP_ASSERT_GE(pool.active_tasks(), 1, "Should have active task");

    // Submit more tasks that will be pending
    for (int i = 0; i < 10; ++i)
    {
        (void)pool.submit([]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        });
    }

    FATP_ASSERT_GE(pool.pending_tasks(), 1, "Should have pending tasks");

    release.store(true, std::memory_order_release);
    pool.wait_idle();

    FATP_ASSERT_EQ(pool.pending_tasks(), 0, "No pending tasks after idle");
    FATP_ASSERT_EQ(pool.active_tasks(), 0, "No active tasks after idle");

    return true;
}

FATP_TEST_CASE(wait_idle_stress)
{
    // This test specifically targets the counter ordering race condition:
    // If pending is decremented BEFORE active is incremented, there's a window
    // where wait_idle() could incorrectly see (pending==0 && active==0).
    //
    // We stress this by rapidly submitting single tasks and calling wait_idle()
    // to try to catch the race.

    ThreadPool pool(4);
    std::atomic<int> completed{0};
    constexpr int iterations = 1000;

    for (int i = 0; i < iterations; ++i)
    {
        // Submit a single task
        (void)pool.submit([&completed]() {
            completed.fetch_add(1, std::memory_order_relaxed);
        });

        // Immediately wait for idle - this should never return early
        pool.wait_idle();

        // After wait_idle returns, the task MUST have completed
        int current = completed.load(std::memory_order_acquire);
        if (current != i + 1)
        {
            std::cerr << "wait_idle race detected! Expected " << (i + 1) << " completed, got " << current << std::endl;
            return false;
        }
    }

    FATP_ASSERT_EQ(completed.load(), iterations, "All tasks should complete");

    return true;
}

// ----------------------------------------------------------------------------
// Benchmarks
// ----------------------------------------------------------------------------
} // namespace fat_p::testing::thread_pool

// ============================================================================
// Test Runner
// ============================================================================

namespace fat_p::testing
{


void run_benchmarks()
{
    using namespace fat_p::testing;

    auto& out = *get_test_config().output;
    out << "\n" << colors::cyan() << "Sanity Benchmark:" << colors::reset() << "\n";
    out << "  (benchmarks are provided by benchmark executables; unit tests do not run full benchmarks)\n\n";
}

bool test_ThreadPool()
{
    FATP_PRINT_HEADER(THREAD POOL)

    TestRunner runner;

    FATP_RUN_TEST_NS(runner, thread_pool, basic_submission);
    FATP_RUN_TEST_NS(runner, thread_pool, future_returns);
    FATP_RUN_TEST_NS(runner, thread_pool, exception_handling);
    FATP_RUN_TEST_NS(runner, thread_pool, priority_scheduling);
    FATP_RUN_TEST_NS(runner, thread_pool, batch_submission);
    FATP_RUN_TEST_NS(runner, thread_pool, stress_many_tasks);
    FATP_RUN_TEST_NS(runner, thread_pool, work_stealing);
    FATP_RUN_TEST_NS(runner, thread_pool, shutdown_behavior);
    FATP_RUN_TEST_NS(runner, thread_pool, spin_configuration);
    FATP_RUN_TEST_NS(runner, thread_pool, auto_thread_count);
    FATP_RUN_TEST_NS(runner, thread_pool, task_counters);
    FATP_RUN_TEST_NS(runner, thread_pool, wait_idle_stress);


    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_ThreadPool() ? 0 : 1;
}
#endif
