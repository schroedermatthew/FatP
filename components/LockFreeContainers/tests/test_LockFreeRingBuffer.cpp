/**
 * @file test_LockFreeRingBuffer.cpp
 * @brief Comprehensive unit tests for LockFreeRingBuffer.h
 */
/*
FATP_META:
  meta_version: 1
  component: LockFreeContainers
  file_role: test
  path: components/LockFreeContainers/tests/test_LockFreeRingBuffer.cpp
  layer: Testing
  namespace: fat_p::testing::lockfreeringbuffer
  summary: "Unit tests for LockFreeRingBuffer (SPSC and MPMC variants)."
  api_stability: stable
  related:
    docs_search: "LockFreeRingBuffer"
    headers:
      - include/fat_p/LockFreeRingBuffer.h
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
#include <iostream>
#include <numeric>
#include <thread>
#include <vector>

#include "FatPTest.h"
#include "LockFreeRingBuffer.h"

namespace fat_p::testing::lockfreeringbuffer
{

// ============================================================================
// SPSC Ring Buffer Tests
// ============================================================================

FATP_TEST_CASE(ring_buffer_basic)
{
    LockFreeRingBuffer<int> buffer(8);

    FATP_ASSERT_TRUE(buffer.empty(), "Buffer should be empty");
    FATP_ASSERT_TRUE(buffer.capacity() == 8, "Capacity should be 8");

    FATP_ASSERT_TRUE(buffer.push(42), "Push should succeed");
    FATP_ASSERT_TRUE(!buffer.empty(), "Buffer should not be empty");

    auto val = buffer.pop();
    FATP_ASSERT_TRUE(val.has_value(), "Pop should return value");
    FATP_ASSERT_TRUE(*val == 42, "Value should be 42");
    FATP_ASSERT_TRUE(buffer.empty(), "Buffer should be empty after pop");

    return true;
}

FATP_TEST_CASE(ring_buffer_multiple)
{
    LockFreeRingBuffer<int> buffer(8);

    for (int i = 0; i < 5; ++i)
    {
        FATP_ASSERT_TRUE(buffer.push(i), "Push should succeed");
    }

    for (int i = 0; i < 5; ++i)
    {
        auto val = buffer.pop();
        FATP_ASSERT_TRUE(val.has_value(), "Pop should return value");
        FATP_ASSERT_TRUE(*val == i, "Value should match");
    }

    FATP_ASSERT_TRUE(buffer.empty(), "Buffer should be empty");

    return true;
}

FATP_TEST_CASE(ring_buffer_full)
{
    LockFreeRingBuffer<int> buffer(4);

    // Fill buffer
    for (int i = 0; i < 4; ++i)
    {
        FATP_ASSERT_TRUE(buffer.push(i), "Push should succeed");
    }

    FATP_ASSERT_TRUE(buffer.full(), "Buffer should be full");
    FATP_ASSERT_TRUE(!buffer.push(99), "Push should fail when full");

    return true;
}

FATP_TEST_CASE(ring_buffer_wrap_around)
{
    LockFreeRingBuffer<int> buffer(4);

    // Fill and empty multiple times to test wrap-around
    for (int cycle = 0; cycle < 3; ++cycle)
    {
        for (int i = 0; i < 4; ++i)
        {
            FATP_ASSERT_TRUE(buffer.push(i + cycle * 10), "Push should succeed");
        }

        for (int i = 0; i < 4; ++i)
        {
            auto val = buffer.pop();
            FATP_ASSERT_TRUE(val.has_value(), "Pop should return value");
            FATP_ASSERT_TRUE(*val == i + cycle * 10, "Value should match");
        }
    }

    return true;
}

FATP_TEST_CASE(ring_buffer_peek)
{
    LockFreeRingBuffer<int> buffer(4);

    (void)buffer.push(42);

    auto peeked = buffer.peek();
    FATP_ASSERT_TRUE(peeked.has_value(), "Peek should return value");
    FATP_ASSERT_TRUE(*peeked == 42, "Peeked value should be 42");

    FATP_ASSERT_TRUE(!buffer.empty(), "Buffer should not be empty after peek");

    auto popped = buffer.pop();
    FATP_ASSERT_TRUE(*popped == 42, "Popped value should be 42");

    return true;
}

FATP_TEST_CASE(ring_buffer_spsc_threaded)
{
    LockFreeRingBuffer<int> buffer(1024);

    constexpr int kNumItems = 10000;
    std::atomic<bool> producerDone{false};
    std::atomic<int> errors{0};

    // Producer thread
    std::thread producer([&]() {
        for (int i = 0; i < kNumItems; ++i)
        {
            while (!buffer.push(i))
            {
                // Spin until push succeeds
            }
        }
        producerDone = true;
    });

    // Consumer thread
    std::thread consumer([&]() {
        int expected = 0;
        while (expected < kNumItems)
        {
            auto val = buffer.pop();
            if (val)
            {
                if (*val != expected)
                {
                    errors.fetch_add(1, std::memory_order_relaxed);
                }
                ++expected;
            }
        }
    });

    producer.join();
    consumer.join();

    FATP_ASSERT_TRUE(errors.load() == 0, "No ordering errors should occur");
    FATP_ASSERT_TRUE(buffer.empty(), "Buffer should be empty after threads finish");

    return true;
}

// ============================================================================
// MPMC Ring Buffer Tests
// ============================================================================

FATP_TEST_CASE(ring_buffer_mpmc_basic)
{
    LockFreeRingBufferMPMC<int> buffer(8);

    FATP_ASSERT_TRUE(buffer.empty(), "Buffer should be empty");

    FATP_ASSERT_TRUE(buffer.push(42), "Push should succeed");

    auto val = buffer.pop();
    FATP_ASSERT_TRUE(val.has_value(), "Pop should return value");
    FATP_ASSERT_TRUE(*val == 42, "Value should be 42");

    return true;
}

FATP_TEST_CASE(ring_buffer_mpmc_fifo)
{
    LockFreeRingBufferMPMC<int> buffer(16);

    // Single-threaded FIFO test
    for (int i = 0; i < 10; ++i)
    {
        FATP_ASSERT_TRUE(buffer.push(i), "Push should succeed");
    }

    for (int i = 0; i < 10; ++i)
    {
        auto val = buffer.pop();
        FATP_ASSERT_TRUE(val.has_value(), "Pop should return value");
        FATP_ASSERT_TRUE(*val == i, "FIFO order should be preserved");
    }

    return true;
}

FATP_TEST_CASE(ring_buffer_mpmc_full_empty)
{
    LockFreeRingBufferMPMC<int> buffer(4);

    // Fill buffer
    for (int i = 0; i < 4; ++i)
    {
        FATP_ASSERT_TRUE(buffer.push(i), "Push should succeed");
    }

    FATP_ASSERT_TRUE(buffer.full(), "Buffer should be full");
    FATP_ASSERT_TRUE(!buffer.push(99), "Push should fail when full");

    // Empty buffer
    for (int i = 0; i < 4; ++i)
    {
        auto val = buffer.pop();
        FATP_ASSERT_TRUE(val.has_value(), "Pop should succeed");
    }

    FATP_ASSERT_TRUE(buffer.empty(), "Buffer should be empty");
    FATP_ASSERT_TRUE(!buffer.pop().has_value(), "Pop should fail when empty");

    return true;
}

/**
 * @brief MPMC stress test - this is the critical test that catches data races
 *
 * Multiple producers and consumers operating concurrently. Each producer
 * writes unique values, and we verify that all values are consumed exactly once.
 */
FATP_TEST_CASE(ring_buffer_mpmc_stress)
{
    LockFreeRingBufferMPMC<uint64_t> buffer(1024);

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
        threads.emplace_back([&buffer, &producedCount, t]() {
            uint64_t base = static_cast<uint64_t>(t) * kItemsPerProducer;
            for (int i = 0; i < kItemsPerProducer; ++i)
            {
                uint64_t value = base + static_cast<uint64_t>(i);
                while (!buffer.push(value))
                {
                    std::this_thread::yield();
                }
                producedCount.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    // Consumers: pop values and mark them as consumed
    for (int t = 0; t < kNumConsumers; ++t)
    {
        threads.emplace_back([&buffer, &consumedCount, &consumed]() {
            while (consumedCount.load(std::memory_order_relaxed) < kTotalItems)
            {
                auto val = buffer.pop();
                if (val)
                {
                    // Mark this value as consumed (should only happen once)
                    bool expected = false;
                    if (consumed[*val].compare_exchange_strong(expected, true))
                    {
                        consumedCount.fetch_add(1, std::memory_order_relaxed);
                    }
                    else
                    {
                        // Double consumption! This indicates a bug
                        std::cerr << "ERROR: Value " << *val << " consumed twice!\n";
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

    // Verify all items were produced and consumed
    FATP_ASSERT_TRUE(producedCount.load() == kTotalItems, "All items should be produced");
    FATP_ASSERT_TRUE(consumedCount.load() == kTotalItems, "All items should be consumed exactly once");
    FATP_ASSERT_TRUE(buffer.empty(), "Buffer should be empty");

    // Verify each value was consumed exactly once
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
FATP_TEST_CASE(ring_buffer_mpmc_high_contention)
{
    LockFreeRingBufferMPMC<int> buffer(8); // Small buffer = high contention

    constexpr int kNumProducers = 8;
    constexpr int kNumConsumers = 8;
    constexpr int kOpsPerThread = 5000;

    std::atomic<int> producedSum{0};
    std::atomic<int> consumedSum{0};
    std::atomic<int> producedCount{0};
    std::atomic<int> consumedCount{0};

    std::vector<std::thread> threads;

    // Producers
    for (int t = 0; t < kNumProducers; ++t)
    {
        threads.emplace_back([&buffer, &producedSum, &producedCount, t]() {
            for (int i = 0; i < kOpsPerThread; ++i)
            {
                int value = t * 1000000 + i;
                while (!buffer.push(value))
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
        threads.emplace_back([&buffer, &consumedSum, &consumedCount]() {
            while (consumedCount.load(std::memory_order_relaxed) < kTotalOps)
            {
                auto val = buffer.pop();
                if (val)
                {
                    consumedSum.fetch_add(*val, std::memory_order_relaxed);
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

    // The sum of produced values should equal sum of consumed values
    // This catches data corruption where wrong values are read
    FATP_ASSERT_TRUE(producedCount.load() == kTotalOps, "All items should be produced");
    FATP_ASSERT_TRUE(consumedCount.load() == kTotalOps, "All items should be consumed");
    FATP_ASSERT_TRUE(producedSum.load() == consumedSum.load(), "Sum mismatch indicates data corruption");

    return true;
}

// ============================================================================
// Capacity/Size Tests
// ============================================================================

FATP_TEST_CASE(ring_buffer_capacity_rounding)
{
    // Capacity should be rounded up to power of 2
    LockFreeRingBuffer<int> buffer1(3);
    FATP_ASSERT_TRUE(buffer1.capacity() == 4, "Capacity should be rounded to 4");

    LockFreeRingBuffer<int> buffer2(5);
    FATP_ASSERT_TRUE(buffer2.capacity() == 8, "Capacity should be rounded to 8");

    LockFreeRingBuffer<int> buffer3(16);
    FATP_ASSERT_TRUE(buffer3.capacity() == 16, "Capacity should remain 16");

    LockFreeRingBufferMPMC<int> mpmc1(7);
    FATP_ASSERT_TRUE(mpmc1.capacity() == 8, "MPMC capacity should be rounded to 8");

    return true;
}

// ============================================================================
// Benchmarks
// ============================================================================
} // namespace fat_p::testing::lockfreeringbuffer

namespace fat_p::testing
{


void run_benchmarks()
{
    using namespace fat_p::testing;

    auto& out = *get_test_config().output;
    out << "\n" << colors::cyan() << "Sanity Benchmark:" << colors::reset() << "\n";
    out << "  (benchmarks are provided by benchmark executables; unit tests do not run full benchmarks)\n\n";
}

bool test_LockFreeRingBuffer()
{
    FATP_PRINT_HEADER(LOCK - FREE RING BUFFER)

    TestRunner runner;

    // SPSC tests
    FATP_RUN_TEST_NS(runner, lockfreeringbuffer, ring_buffer_basic);
    FATP_RUN_TEST_NS(runner, lockfreeringbuffer, ring_buffer_multiple);
    FATP_RUN_TEST_NS(runner, lockfreeringbuffer, ring_buffer_full);
    FATP_RUN_TEST_NS(runner, lockfreeringbuffer, ring_buffer_wrap_around);
    FATP_RUN_TEST_NS(runner, lockfreeringbuffer, ring_buffer_peek);
    FATP_RUN_TEST_NS(runner, lockfreeringbuffer, ring_buffer_spsc_threaded);

    // MPMC tests
    FATP_RUN_TEST_NS(runner, lockfreeringbuffer, ring_buffer_mpmc_basic);
    FATP_RUN_TEST_NS(runner, lockfreeringbuffer, ring_buffer_mpmc_fifo);
    FATP_RUN_TEST_NS(runner, lockfreeringbuffer, ring_buffer_mpmc_full_empty);
    FATP_RUN_TEST_NS(runner, lockfreeringbuffer, ring_buffer_mpmc_stress);
    FATP_RUN_TEST_NS(runner, lockfreeringbuffer, ring_buffer_mpmc_high_contention);

    // Capacity tests
    FATP_RUN_TEST_NS(runner, lockfreeringbuffer, ring_buffer_capacity_rounding);


    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_LockFreeRingBuffer() ? 0 : 1;
}
#endif
