/**
 * @file test_LockFreeQueue.cpp
 * @brief Comprehensive tests for LockFreeQueue.h
 */
/*
FATP_META:
  meta_version: 1
  component: LockFreeQueue
  file_role: test
  path: tests/test_LockFreeQueue.cpp
  namespace: fat_p::testing::lockfreequeue
  summary: "Unit tests for LockFreeQueue."
  related:
    docs_search: "LockFreeQueue"
    headers:
      - fat_p/LockFreeQueue.h
      - fat_p/FatPTest.h
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

#include <atomic>
#include <iostream>
#include <thread>
#include <vector>

#include "FatPTest.h"
#include "LockFreeQueue.h"

namespace fat_p::testing::lockfreequeue
{

// ============================================================================
// Basic Operations
// ============================================================================

FATP_TEST_CASE(lock_free_queue_basic_operations)
{
    LockFreeQueue<int, 16> queue;

    FATP_ASSERT_TRUE(queue.empty(), "Queue should start empty");
    FATP_ASSERT_TRUE(queue.enqueue(42), "Should enqueue");
    FATP_ASSERT_TRUE(!queue.empty(), "Queue should not be empty");

    int value = 0;
    FATP_ASSERT_TRUE(queue.dequeue(value), "Should dequeue");
    FATP_ASSERT_TRUE(value == 42, "Value should match");
    FATP_ASSERT_TRUE(queue.empty(), "Queue should be empty");

    return true;
}

FATP_TEST_CASE(lock_free_queue_fifo_ordering)
{
    LockFreeQueue<int, 128> queue;

    for (int i = 0; i < 10; ++i)
    {
        FATP_ASSERT_TRUE(queue.enqueue(i), "Should enqueue");
    }

    for (int i = 0; i < 10; ++i)
    {
        int value = -1;
        FATP_ASSERT_TRUE(queue.dequeue(value), "Should dequeue");
        FATP_ASSERT_TRUE(value == i, "Values should be in FIFO order");
    }

    return true;
}

// ============================================================================
// Boundary Conditions
// ============================================================================

FATP_TEST_CASE(lock_free_queue_queue_full)
{
    LockFreeQueue<int, 4> queue;

    // Fill queue
    for (int i = 0; i < 4; ++i)
    {
        FATP_ASSERT_TRUE(queue.enqueue(i), "Should enqueue");
    }

    // Queue should be full
    FATP_ASSERT_TRUE(!queue.enqueue(999), "Should fail when full");

    // Dequeue one
    int value;
    FATP_ASSERT_TRUE(queue.dequeue(value), "Should dequeue");

    // Can enqueue again
    FATP_ASSERT_TRUE(queue.enqueue(999), "Should enqueue after dequeue");

    return true;
}

FATP_TEST_CASE(lock_free_queue_queue_empty)
{
    LockFreeQueue<int, 16> queue;

    int value = 0;
    FATP_ASSERT_TRUE(!queue.dequeue(value), "Should fail when empty");

    (void)queue.enqueue(42);
    (void)queue.dequeue(value);

    FATP_ASSERT_TRUE(!queue.dequeue(value), "Should fail when empty again");

    return true;
}

// ============================================================================
// Concurrent Operations
// ============================================================================

FATP_TEST_CASE(lock_free_queue_mpmc)
{
    LockFreeQueue<int, 1024> queue;
    constexpr int kNumProducers = 4;
    constexpr int kNumConsumers = 4;
    constexpr int kItemsPerProducer = 1000;

    std::atomic<int> totalProduced{0};
    std::atomic<int> totalConsumed{0};
    std::vector<std::thread> threads;

    // Producers
    for (int t = 0; t < kNumProducers; ++t)
    {
        threads.emplace_back([&queue, &totalProduced, t]() {
            for (int i = 0; i < kItemsPerProducer; ++i)
            {
                while (!queue.enqueue(t * kItemsPerProducer + i))
                {
                    std::this_thread::yield();
                }
                totalProduced.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    // Consumers
    constexpr int kExpected = kNumProducers * kItemsPerProducer;
    for (int t = 0; t < kNumConsumers; ++t)
    {
        threads.emplace_back([&queue, &totalConsumed]() {
            int value;
            while (totalConsumed.load(std::memory_order_relaxed) < kExpected)
            {
                if (queue.dequeue(value))
                {
                    totalConsumed.fetch_add(1, std::memory_order_relaxed);
                }
                else
                {
                    std::this_thread::yield();
                }
            }
        });
    }

    for (auto& thread : threads)
    {
        thread.join();
    }

    FATP_ASSERT_TRUE(totalProduced.load() == kExpected, "All items should be produced");
    FATP_ASSERT_TRUE(totalConsumed.load() == kExpected, "All items should be consumed");
    FATP_ASSERT_TRUE(queue.empty(), "Queue should be empty");

    return true;
}

/**
 * @brief MPMC stress test - verifies no duplicates and no loss
 *
 * This is the critical correctness test that catches data races.
 * Each producer writes unique values, and we verify all values
 * are consumed exactly once (no duplicates, no loss).
 */
FATP_TEST_CASE(lock_free_queue_mpmc_stress)
{
    LockFreeQueue<uint64_t, 1024> queue;

    constexpr int kNumProducers = 4;
    constexpr int kNumConsumers = 4;
    constexpr int kItemsPerProducer = 10000;
    constexpr uint64_t kTotalItems = kNumProducers * kItemsPerProducer;

    std::atomic<uint64_t> producedCount{0};
    std::atomic<uint64_t> consumedCount{0};
    std::vector<std::atomic<bool>> consumed(kTotalItems);

    for (auto& c : consumed)
    {
        c.store(false, std::memory_order_relaxed);
    }

    std::vector<std::thread> threads;

    // Producers: each writes unique values in range [t*kItemsPerProducer, (t+1)*kItemsPerProducer)
    for (int t = 0; t < kNumProducers; ++t)
    {
        threads.emplace_back([&queue, &producedCount, t]() {
            uint64_t base = static_cast<uint64_t>(t) * kItemsPerProducer;
            for (int i = 0; i < kItemsPerProducer; ++i)
            {
                uint64_t value = base + static_cast<uint64_t>(i);
                while (!queue.enqueue(value))
                {
                    std::this_thread::yield();
                }
                producedCount.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    // Consumers: dequeue values and mark them as consumed
    for (int t = 0; t < kNumConsumers; ++t)
    {
        threads.emplace_back([&queue, &consumedCount, &consumed]() {
            while (consumedCount.load(std::memory_order_relaxed) < kTotalItems)
            {
                uint64_t val;
                if (queue.dequeue(val))
                {
                    // Mark this value as consumed (should only happen once)
                    bool expected = false;
                    if (consumed[val].compare_exchange_strong(expected, true))
                    {
                        consumedCount.fetch_add(1, std::memory_order_relaxed);
                    }
                    else
                    {
                        // Double consumption! This indicates a data race bug
                        std::cerr << "ERROR: Value " << val << " consumed twice!\n";
                    }
                }
                else
                {
                    std::this_thread::yield();
                }
            }
        });
    }

    for (auto& thread : threads)
    {
        thread.join();
    }

    // Verify all items were produced and consumed exactly once
    FATP_ASSERT_TRUE(producedCount.load() == kTotalItems, "All items should be produced");
    FATP_ASSERT_TRUE(consumedCount.load() == kTotalItems, "All items should be consumed exactly once");
    FATP_ASSERT_TRUE(queue.empty(), "Queue should be empty");

    // Verify each value was consumed exactly once (no loss)
    for (uint64_t i = 0; i < kTotalItems; ++i)
    {
        FATP_ASSERT_TRUE(consumed[i].load(), "Each value should be consumed");
    }

    return true;
}

/**
 * @brief High contention stress test with small buffer
 *
 * This test uses a small buffer to maximize contention and expose
 * any race conditions in the sequence number logic.
 */
FATP_TEST_CASE(lock_free_queue_mpmc_high_contention)
{
    LockFreeQueue<int, 8> queue; // Small buffer = high contention

    constexpr int kNumProducers = 8;
    constexpr int kNumConsumers = 8;
    constexpr int kOpsPerThread = 5000;

    std::atomic<int64_t> producedSum{0};
    std::atomic<int64_t> consumedSum{0};
    std::atomic<int> producedCount{0};
    std::atomic<int> consumedCount{0};

    std::vector<std::thread> threads;

    // Producers
    for (int t = 0; t < kNumProducers; ++t)
    {
        threads.emplace_back([&queue, &producedSum, &producedCount, t]() {
            for (int i = 0; i < kOpsPerThread; ++i)
            {
                int value = t * 1000000 + i;
                while (!queue.enqueue(value))
                {
                    std::this_thread::yield();
                }
                producedSum.fetch_add(value, std::memory_order_relaxed);
                producedCount.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    // Consumers
    constexpr int kTotalOps = kNumProducers * kOpsPerThread;
    for (int t = 0; t < kNumConsumers; ++t)
    {
        threads.emplace_back([&queue, &consumedSum, &consumedCount]() {
            while (consumedCount.load(std::memory_order_relaxed) < kTotalOps)
            {
                int val;
                if (queue.dequeue(val))
                {
                    consumedSum.fetch_add(val, std::memory_order_relaxed);
                    consumedCount.fetch_add(1, std::memory_order_relaxed);
                }
                else
                {
                    std::this_thread::yield();
                }
            }
        });
    }

    for (auto& thread : threads)
    {
        thread.join();
    }

    // Verify counts match
    FATP_ASSERT_TRUE(producedCount.load() == kTotalOps, "All items should be produced");
    FATP_ASSERT_TRUE(consumedCount.load() == kTotalOps, "All items should be consumed");

    // Verify sums match (detects lost or duplicated items)
    FATP_ASSERT_TRUE(producedSum.load() == consumedSum.load(),
                     "Sum of produced must equal sum of consumed");

    FATP_ASSERT_TRUE(queue.empty(), "Queue should be empty");

    return true;
}

FATP_TEST_CASE(lock_free_queue_try_dequeue)
{
    LockFreeQueue<int, 16> queue;

    // Try dequeue on empty queue should fail
    int value = 0;
    FATP_ASSERT_TRUE(!queue.tryDequeue(value, 10), "Should fail on empty queue");

    // Add item and try again
    (void)queue.enqueue(42);
    FATP_ASSERT_TRUE(queue.tryDequeue(value, 10), "Should succeed with item");
    FATP_ASSERT_TRUE(value == 42, "Value should match");

    return true;
}

// ============================================================================
// Statistics (when enabled)
// ============================================================================

FATP_TEST_CASE(lock_free_queue_statistics)
{
    LockFreeQueue<int, 64, true> queue; // Enable stats

    // Enqueue some items
    for (int i = 0; i < 10; ++i)
    {
        (void)queue.enqueue(i);
    }

    // Dequeue some items
    for (int i = 0; i < 5; ++i)
    {
        int value;
        (void)queue.dequeue(value);
    }

    // Fill up the rest
    for (int i = 0; i < 59; ++i)
    {
        (void)queue.enqueue(i);
    }

    // Try to enqueue when full (should fail)
    bool enqueuedWhenFull = queue.enqueue(999);
    FATP_ASSERT_TRUE(!enqueuedWhenFull, "Should fail when full");

    auto stats = queue.stats();

    FATP_ASSERT_TRUE(stats.totalEnqueues == 69, "Should track enqueues");
    FATP_ASSERT_TRUE(stats.totalDequeues == 5, "Should track dequeues");
    FATP_ASSERT_TRUE(stats.failedEnqueues == 1, "Should track failed enqueues");
    FATP_ASSERT_TRUE(stats.currentSize == queue.size(), "Size should match");

    // Reset stats
    queue.resetStats();
    auto resetStats = queue.stats();
    FATP_ASSERT_TRUE(resetStats.totalEnqueues == 0, "Stats should be reset");

    return true;
}

// ============================================================================
// Capacity
// ============================================================================

FATP_TEST_CASE(lock_free_queue_capacity)
{
    LockFreeQueue<int, 256> queue;

    FATP_ASSERT_TRUE(queue.capacity() == 256, "Capacity should be template parameter");
    FATP_ASSERT_TRUE(queue.size() == 0, "Initial size should be 0");

    for (int i = 0; i < 100; ++i)
    {
        (void)queue.enqueue(i);
    }

    FATP_ASSERT_TRUE(queue.size() == 100, "Size should be 100");
    FATP_ASSERT_TRUE(queue.capacity() == 256, "Capacity unchanged");

    return true;
}

// ============================================================================
// Benchmarks
// ============================================================================

void benchmarkLockFreeQueue()
{
    std::cout << "\n" << colors::cyan() << "LockFreeQueue Benchmarks:" << colors::reset() << "\n\n";

    // Benchmark 1: Single-threaded enqueue/dequeue
    {
        LockFreeQueue<int, 1024> queue;

        double enqueueTime = measure_perf(
            [&queue]() {
                (void)queue.enqueue(42);
            },
            10000,
            100);

        std::cout << "Single-threaded enqueue: " << format_time(enqueueTime) << "\n";

        double dequeueTime = measure_perf(
            [&queue]() {
                int value;
                (void)queue.dequeue(value);
            },
            10000,
            100);

        std::cout << "Single-threaded dequeue: " << format_time(dequeueTime) << "\n";
    }

    // Benchmark 2: Contended throughput
    {
        LockFreeQueue<int, 4096> queue;
        constexpr int kNumOps = 100000;

        auto start = std::chrono::high_resolution_clock::now();

        std::thread producer([&queue]() {
            for (int i = 0; i < kNumOps; ++i)
            {
                while (!queue.enqueue(i))
                {
                    std::this_thread::yield();
                }
            }
        });

        std::thread consumer([&queue]() {
            int value;
            for (int i = 0; i < kNumOps; ++i)
            {
                while (!queue.dequeue(value))
                {
                    std::this_thread::yield();
                }
            }
        });

        producer.join();
        consumer.join();

        auto end = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double>(end - start).count();
        double throughput = (2 * kNumOps) / elapsed;

        std::cout << "Contended throughput: " << colors::bold()
                  << static_cast<int>(throughput) << " ops/sec"
                  << colors::reset() << "\n";
    }
}

} // namespace fat_p::testing::lockfreequeue

namespace fat_p::testing
{

bool test_LockFreeQueue()
{
    FATP_PRINT_HEADER(LOCK - FREE QUEUE)

    TestRunner runner;

    FATP_RUN_TEST_NS(runner, lockfreequeue, lock_free_queue_basic_operations);
    FATP_RUN_TEST_NS(runner, lockfreequeue, lock_free_queue_fifo_ordering);
    FATP_RUN_TEST_NS(runner, lockfreequeue, lock_free_queue_queue_full);
    FATP_RUN_TEST_NS(runner, lockfreequeue, lock_free_queue_queue_empty);
    FATP_RUN_TEST_NS(runner, lockfreequeue, lock_free_queue_mpmc);
    FATP_RUN_TEST_NS(runner, lockfreequeue, lock_free_queue_mpmc_stress);
    FATP_RUN_TEST_NS(runner, lockfreequeue, lock_free_queue_mpmc_high_contention);
    FATP_RUN_TEST_NS(runner, lockfreequeue, lock_free_queue_try_dequeue);
    FATP_RUN_TEST_NS(runner, lockfreequeue, lock_free_queue_statistics);
    FATP_RUN_TEST_NS(runner, lockfreequeue, lock_free_queue_capacity);

    lockfreequeue::benchmarkLockFreeQueue();

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_LockFreeQueue() ? 0 : 1;
}
#endif
