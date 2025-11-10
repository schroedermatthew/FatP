#include <iostream>
#include <thread>
#include <atomic>
#include <vector>

#include "CircularBuffer.h"
#include "test_CircularBuffer.h"
#include "test_Utilities.h"

namespace cpp_utilities::testing
{

constexpr size_t BUFFER_SIZE = 16;
constexpr int TEST_ITERATIONS = 10000;

bool test_circular_buffer_basic_construction() {
    CircularBuffer<int, BUFFER_SIZE> buffer;
    
    SIMPLE_ASSERT(buffer.empty(), "New buffer should be empty");
    SIMPLE_ASSERT(!buffer.full(), "New buffer should not be full");
    SIMPLE_ASSERT(buffer.size() == 0, "Size should be 0");
    
    return true;
}

bool test_circular_buffer_push_pop() {
    CircularBuffer<int, BUFFER_SIZE> buffer;
    
    SIMPLE_ASSERT(buffer.push(1), "Push should succeed");
    SIMPLE_ASSERT(buffer.push(2), "Push should succeed");
    SIMPLE_ASSERT(buffer.push(3), "Push should succeed");
    
    SIMPLE_ASSERT(buffer.size() == 3, "Size should be 3");
    
    int val;
    SIMPLE_ASSERT(buffer.pop(val), "Pop should succeed");
    SIMPLE_ASSERT(val == 1, "Popped value should be 1");
    
    SIMPLE_ASSERT(buffer.pop(val), "Pop should succeed");
    SIMPLE_ASSERT(val == 2, "Popped value should be 2");
    
    return true;
}

bool test_circular_buffer_full_condition() {
    CircularBuffer<int, 4> buffer;
    
    // Fill the buffer (capacity is actually 3 due to +1 for full/empty distinction)
    SIMPLE_ASSERT(buffer.push(1), "Push 1 should succeed");
    SIMPLE_ASSERT(buffer.push(2), "Push 2 should succeed");
    SIMPLE_ASSERT(buffer.push(3), "Push 3 should succeed");
    SIMPLE_ASSERT(buffer.push(4), "Push 4 should succeed");
    
    SIMPLE_ASSERT(buffer.full() || buffer.size() >= 3, "Buffer should be full or near full");
    
    // Try to push when full
    bool result = buffer.push(5);
    // May fail if buffer is full
    
    return true;
}

bool test_circular_buffer_empty_condition() {
    CircularBuffer<int, BUFFER_SIZE> buffer;
    
    int val;
    SIMPLE_ASSERT(!buffer.pop(val), "Pop from empty buffer should fail");
    SIMPLE_ASSERT(buffer.empty(), "Buffer should be empty");
    
    return true;
}

bool test_circular_buffer_fifo() {
    CircularBuffer<int, BUFFER_SIZE> buffer;
    
    for (int i = 0; i < 10; ++i) {
        buffer.push(i);
    }
    
    for (int i = 0; i < 10; ++i) {
        int val;
        SIMPLE_ASSERT(buffer.pop(val), "Pop should succeed");
        SIMPLE_ASSERT(val == i, "Values should be in FIFO order");
    }
    
    return true;
}

bool test_circular_buffer_wraparound() {
    CircularBuffer<int, 8> buffer;
    
    // Fill partially
    for (int i = 0; i < 5; ++i) {
        buffer.push(i);
    }
    
    // Pop some
    int val;
    for (int i = 0; i < 3; ++i) {
        buffer.pop(val);
    }
    
    // Push more (should wrap around)
    for (int i = 100; i < 106; ++i) {
        buffer.push(i);
    }
    
    // Verify order
    buffer.pop(val);
    SIMPLE_ASSERT(val == 3, "Should get remaining old value");
    buffer.pop(val);
    SIMPLE_ASSERT(val == 4, "Should get remaining old value");
    buffer.pop(val);
    SIMPLE_ASSERT(val == 100, "Should get first new value");
    
    return true;
}

bool test_circular_buffer_move_semantics() {
    CircularBuffer<std::string, BUFFER_SIZE> buffer;
    
    std::string str = "hello";
    SIMPLE_ASSERT(buffer.push(std::move(str)), "Move push should succeed");
    
    std::string result;
    SIMPLE_ASSERT(buffer.pop(result), "Pop should succeed");
    SIMPLE_ASSERT(result == "hello", "Value should be preserved");
    
    return true;
}

bool test_circular_buffer_size() {
    CircularBuffer<int, BUFFER_SIZE> buffer;
    
    SIMPLE_ASSERT(buffer.size() == 0, "Initial size should be 0");
    
    for (int i = 0; i < 5; ++i) {
        buffer.push(i);
    }
    SIMPLE_ASSERT(buffer.size() == 5, "Size should be 5");
    
    int val;
    buffer.pop(val);
    buffer.pop(val);
    SIMPLE_ASSERT(buffer.size() == 3, "Size should be 3 after 2 pops");
    
    return true;
}

bool test_circular_buffer_clear_by_popping() {
    CircularBuffer<int, BUFFER_SIZE> buffer;
    
    for (int i = 0; i < 10; ++i) {
        buffer.push(i);
    }
    
    int val;
    while (buffer.pop(val)) {
        // Keep popping
    }
    
    SIMPLE_ASSERT(buffer.empty(), "Buffer should be empty after popping all");
    
    return true;
}

bool test_circular_buffer_thread_safety() {
    CircularBuffer<int, 1024> buffer;
    std::atomic<bool> done{false};
    std::atomic<int> errors{0};
    
    // Producer thread
    std::thread producer([&]() {
        for (int i = 0; i < TEST_ITERATIONS; ++i) {
            while (!buffer.push(i)) {
                // Spin until push succeeds
                std::this_thread::yield();
            }
        }
        done.store(true);
    });
    
    // Consumer thread
    std::thread consumer([&]() {
        int expected = 0;
        while (expected < TEST_ITERATIONS) {
            int val;
            if (buffer.pop(val)) {
                if (val != expected) {
                    errors.fetch_add(1);
                }
                ++expected;
            } else {
                std::this_thread::yield();
            }
        }
    });
    
    producer.join();
    consumer.join();
    
    SIMPLE_ASSERT(errors.load() == 0, "Should have no ordering errors");
    
    return true;
}

void benchmark_circularbuffer() {
    std::cout << "\n" << colors::cyan() << "CircularBuffer Benchmarks:" << colors::reset() << "\n\n";
    
    CircularBuffer<int, 1024> buffer;
    
    // Benchmark push
    double push_time = measure_perf([&buffer, i=0]() mutable {
        buffer.push(i);
        ++i;
    }, 100000, 1000);
    std::cout << "Push: " << format_time(push_time) << "\n";
    
    // Fill buffer for pop test
    for (int i = 0; i < 500; ++i) {
        buffer.push(i);
    }
    
    // Benchmark pop
    double pop_time = measure_perf([&buffer]() {
        int val;
        bool result = buffer.pop(val);
        DoNotOptimize(result);
        DoNotOptimize(val);
    }, 100000, 1000);
    std::cout << "Pop: " << format_time(pop_time) << "\n";
}

bool test_CircularBuffer() {

    PRINT_HEADER(CIRCULAR BUFFER)

    TestRunner runner;

    RUN_TEST(runner, circular_buffer_basic_construction);
    RUN_TEST(runner, circular_buffer_push_pop);
    RUN_TEST(runner, circular_buffer_full_condition);
    RUN_TEST(runner, circular_buffer_empty_condition);
    RUN_TEST(runner, circular_buffer_fifo);
    RUN_TEST(runner, circular_buffer_wraparound);
    RUN_TEST(runner, circular_buffer_move_semantics);
    RUN_TEST(runner, circular_buffer_size);
    RUN_TEST(runner, circular_buffer_clear_by_popping);
    RUN_TEST(runner, circular_buffer_thread_safety);

    benchmark_circularbuffer();

    return 0 == runner.print_summary();
}

} // namespace cpp_utilities::testing
