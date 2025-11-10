#include <iostream>
#include <thread>
#include <vector>

#include "LockFreeRingBuffer.h"
#include "test_LockFreeRingBuffer.h"
#include "test_Utilities.h"


namespace cpp_utilities::testing
{

bool test_ring_buffer_basic() {
    LockFreeRingBuffer<int> buffer(8);
    
    SIMPLE_ASSERT(buffer.empty(), "Buffer should be empty");
    SIMPLE_ASSERT(buffer.capacity() == 8, "Capacity should be 8");
    
    SIMPLE_ASSERT(buffer.push(42), "Push should succeed");
    SIMPLE_ASSERT(!buffer.empty(), "Buffer should not be empty");
    
    auto val = buffer.pop();
    SIMPLE_ASSERT(val.has_value(), "Pop should return value");
    SIMPLE_ASSERT(*val == 42, "Value should be 42");
    SIMPLE_ASSERT(buffer.empty(), "Buffer should be empty after pop");
    
    return true;
}

bool test_ring_buffer_multiple() {
    LockFreeRingBuffer<int> buffer(8);
    
    for (int i = 0; i < 5; ++i) {
        SIMPLE_ASSERT(buffer.push(i), "Push should succeed");
    }
    
    for (int i = 0; i < 5; ++i) {
        auto val = buffer.pop();
        SIMPLE_ASSERT(val.has_value(), "Pop should return value");
        SIMPLE_ASSERT(*val == i, "Value should match");
    }
    
    SIMPLE_ASSERT(buffer.empty(), "Buffer should be empty");
    
    return true;
}

bool test_ring_buffer_full() {
    LockFreeRingBuffer<int> buffer(4);
    
    // Fill buffer
    for (int i = 0; i < 4; ++i) {
        SIMPLE_ASSERT(buffer.push(i), "Push should succeed");
    }
    
    SIMPLE_ASSERT(buffer.full(), "Buffer should be full");
    SIMPLE_ASSERT(!buffer.push(99), "Push should fail when full");
    
    return true;
}

bool test_ring_buffer_wrap_around() {
    LockFreeRingBuffer<int> buffer(4);
    
    // Fill and empty multiple times
    for (int cycle = 0; cycle < 3; ++cycle) {
        for (int i = 0; i < 4; ++i) {
            SIMPLE_ASSERT(buffer.push(i + cycle * 10), "Push should succeed");
        }
        
        for (int i = 0; i < 4; ++i) {
            auto val = buffer.pop();
            SIMPLE_ASSERT(val.has_value(), "Pop should return value");
            SIMPLE_ASSERT(*val == i + cycle * 10, "Value should match");
        }
    }
    
    return true;
}

bool test_ring_buffer_peek() {
    LockFreeRingBuffer<int> buffer(4);
    
    (void)buffer.push(42);
    
    auto peeked = buffer.peek();
    SIMPLE_ASSERT(peeked.has_value(), "Peek should return value");
    SIMPLE_ASSERT(*peeked == 42, "Peeked value should be 42");
    
    SIMPLE_ASSERT(!buffer.empty(), "Buffer should not be empty after peek");
    
    auto popped = buffer.pop();
    SIMPLE_ASSERT(*popped == 42, "Popped value should be 42");
    
    return true;
}

bool test_ring_buffer_mpmc_basic() {
    LockFreeRingBufferMPMC<int> buffer(8);
    
    SIMPLE_ASSERT(buffer.empty(), "Buffer should be empty");
    
    SIMPLE_ASSERT(buffer.push(42), "Push should succeed");
    
    auto val = buffer.pop();
    SIMPLE_ASSERT(val.has_value(), "Pop should return value");
    SIMPLE_ASSERT(*val == 42, "Value should be 42");
    
    return true;
}

bool test_ring_buffer_spsc_threaded() {
    LockFreeRingBuffer<int> buffer(1024);
    
    constexpr int NUM_ITEMS = 10000;
    std::atomic<bool> producer_done{false};
    
    // Producer thread
    std::thread producer([&]() {
        for (int i = 0; i < NUM_ITEMS; ++i) {
            while (!buffer.push(i)) {
                // Spin until push succeeds
            }
        }
        producer_done = true;
    });
    
    // Consumer thread
    std::thread consumer([&]() {
        int expected = 0;
        while (expected < NUM_ITEMS) {
            auto val = buffer.pop();
            if (val) {
                if (*val != expected) {
                    std::cerr << "Expected " << expected << " got " << *val << "\n";
                }
                ++expected;
            }
        }
    });
    
    producer.join();
    consumer.join();
    
    SIMPLE_ASSERT(buffer.empty(), "Buffer should be empty after threads finish");
    
    return true;
}

void benchmark_ring_buffer() {
    std::cout << "\n" << colors::cyan() << "LockFreeRingBuffer Benchmarks:" << colors::reset() << "\n\n";
    
    LockFreeRingBuffer<int> buffer(1024);
    
    // Benchmark push
    double push_time = measure_perf([&buffer, i=0]() mutable {
        (void)buffer.push(i++);
    }, 100000, 1000);
    std::cout << "Push: " << format_time(push_time) << "\n";
    
    // Fill buffer for pop benchmark
    LockFreeRingBuffer<int> buffer2(100000);
    for (int i = 0; i < 100000; ++i) {
        (void)buffer2.push(i);
    }
    
    // Benchmark pop
    double pop_time = measure_perf([&buffer2]() {
        auto val = buffer2.pop();
        DoNotOptimize(val);
    }, 100000, 0);
    std::cout << "Pop: " << format_time(pop_time) << "\n";
    
    // Benchmark MPMC
    LockFreeRingBufferMPMC<int> mpmc_buffer(1024);
    
    double mpmc_push_time = measure_perf([&mpmc_buffer, i=0]() mutable {
        (void)mpmc_buffer.push(i++);
    }, 100000, 1000);
    std::cout << "MPMC Push: " << format_time(mpmc_push_time) << "\n";
}

bool test_LockFreeRingBuffer() {

    PRINT_HEADER(LOCK-FREE RING BUFFER)

    TestRunner runner;

    RUN_TEST(runner, ring_buffer_basic);
    RUN_TEST(runner, ring_buffer_multiple);
    RUN_TEST(runner, ring_buffer_full);
    RUN_TEST(runner, ring_buffer_wrap_around);
    RUN_TEST(runner, ring_buffer_peek);
    RUN_TEST(runner, ring_buffer_mpmc_basic);
    RUN_TEST(runner, ring_buffer_spsc_threaded);

    benchmark_ring_buffer();

    return 0 == runner.print_summary();
}

} // namespace cpp_utilities::testing
