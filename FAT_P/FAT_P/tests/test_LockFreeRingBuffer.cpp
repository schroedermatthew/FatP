/**
 * @file test_LockFreeRingBuffer.cpp
 * @brief Comprehensive unit tests for LockFreeRingBuffer.h
 */
/*
FATP_META:
  meta_version: 1
  component: LockFreeRingBuffer
  file_role: test
  path: tests/test_LockFreeRingBuffer.cpp
  namespace: fat_p::testing::lockfreeringbuffer
  summary: "Unit tests for LockFreeRingBuffer."
  related:
    docs_search: "LockFreeRingBuffer"
    headers:
      - fat_p/LockFreeRingBuffer.h
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

#include <iostream>
#include <thread>
#include <vector>

#include "FatPTest.h"
#include "LockFreeRingBuffer.h"


namespace fat_p::testing::lockfreeringbuffer
{

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

    // Fill and empty multiple times
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

FATP_TEST_CASE(ring_buffer_spsc_threaded)
{
    LockFreeRingBuffer<int> buffer(1024);

    constexpr int NUM_ITEMS = 10000;
    std::atomic<bool> producer_done{false};

    // Producer thread
    std::thread producer(
        [&]()
        {
            for (int i = 0; i < NUM_ITEMS; ++i)
            {
                while (!buffer.push(i))
                {
                    // Spin until push succeeds
                }
            }
            producer_done = true;
        });

    // Consumer thread
    std::thread consumer(
        [&]()
        {
            int expected = 0;
            while (expected < NUM_ITEMS)
            {
                auto val = buffer.pop();
                if (val)
                {
                    if (*val != expected)
                    {
                        std::cerr << "Expected " << expected << " got " << *val << "\n";
                    }
                    ++expected;
                }
            }
        });

    producer.join();
    consumer.join();

    FATP_ASSERT_TRUE(buffer.empty(), "Buffer should be empty after threads finish");

    return true;
}

void benchmark_ring_buffer()
{
    std::cout << "\n" << colors::cyan() << "LockFreeRingBuffer Benchmarks:" << colors::reset() << "\n\n";

    LockFreeRingBuffer<int> buffer(1024);

    // Benchmark push
    double push_time = measure_perf(
        [&buffer, i = 0]() mutable
        {
            (void)buffer.push(i++);
        },
        100000,
        1000);
    std::cout << "Push: " << format_time(push_time) << "\n";

    // Fill buffer for pop benchmark
    LockFreeRingBuffer<int> buffer2(100000);
    for (int i = 0; i < 100000; ++i)
    {
        (void)buffer2.push(i);
    }

    // Benchmark pop
    double pop_time = measure_perf(
        [&buffer2]()
        {
            auto val = buffer2.pop();
            DoNotOptimize(val);
        },
        100000,
        0);
    std::cout << "Pop: " << format_time(pop_time) << "\n";

    // Benchmark MPMC
    LockFreeRingBufferMPMC<int> mpmc_buffer(1024);

    double mpmc_push_time = measure_perf(
        [&mpmc_buffer, i = 0]() mutable
        {
            (void)mpmc_buffer.push(i++);
        },
        100000,
        1000);
    std::cout << "MPMC Push: " << format_time(mpmc_push_time) << "\n";
}

} // namespace fat_p::testing::lockfreeringbuffer

namespace fat_p::testing
{

bool test_LockFreeRingBuffer()
{
    FATP_PRINT_HEADER(LOCK - FREE RING BUFFER)

    TestRunner runner;

    FATP_RUN_TEST_NS(runner, lockfreeringbuffer, ring_buffer_basic);
    FATP_RUN_TEST_NS(runner, lockfreeringbuffer, ring_buffer_multiple);
    FATP_RUN_TEST_NS(runner, lockfreeringbuffer, ring_buffer_full);
    FATP_RUN_TEST_NS(runner, lockfreeringbuffer, ring_buffer_wrap_around);
    FATP_RUN_TEST_NS(runner, lockfreeringbuffer, ring_buffer_peek);
    FATP_RUN_TEST_NS(runner, lockfreeringbuffer, ring_buffer_mpmc_basic);
    FATP_RUN_TEST_NS(runner, lockfreeringbuffer, ring_buffer_spsc_threaded);

    lockfreeringbuffer::benchmark_ring_buffer();

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
