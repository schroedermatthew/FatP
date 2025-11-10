/**
 * @file test_LockFreeQueue.cpp
 * @brief Comprehensive tests for LockFreeQueue.h
 */

#include <iostream>
#include <thread>
#include <vector>
#include <atomic>

#include "LockFreeQueue.h"
#include "test_LockFreeQueue.h"
#include "test_Utilities.h"

namespace cpp_utilities::testing
{

// Test 1: Basic enqueue/dequeue
bool test_lock_free_queue_basic_operations() {
    LockFreeQueue<int, 16> queue;
    
    SIMPLE_ASSERT(queue.empty(), "Queue should start empty");
    SIMPLE_ASSERT(queue.enqueue(42), "Should enqueue");
    SIMPLE_ASSERT(!queue.empty(), "Queue should not be empty");
    
    int value = 0;
    SIMPLE_ASSERT(queue.dequeue(value), "Should dequeue");
    SIMPLE_ASSERT(value == 42, "Value should match");
    SIMPLE_ASSERT(queue.empty(), "Queue should be empty");
    
    return true;
}

// Test 2: FIFO ordering
bool test_lock_free_queue_fifo_ordering() {
    LockFreeQueue<int, 128> queue;
    
    for (int i = 0; i < 10; ++i) {
        SIMPLE_ASSERT(queue.enqueue(i), "Should enqueue");
    }
    
    for (int i = 0; i < 10; ++i) {
        int value = -1;
        SIMPLE_ASSERT(queue.dequeue(value), "Should dequeue");
        SIMPLE_ASSERT(value == i, "Values should be in FIFO order");
    }
    
    return true;
}

// Test 3: Queue full behavior
bool test_lock_free_queue_queue_full() {
    LockFreeQueue<int, 4> queue;
    
    // Fill queue
    for (int i = 0; i < 4; ++i) {
        SIMPLE_ASSERT(queue.enqueue(i), "Should enqueue");
    }
    
    // Queue should be full
    SIMPLE_ASSERT(!queue.enqueue(999), "Should fail when full");
    
    // Dequeue one
    int value;
    SIMPLE_ASSERT(queue.dequeue(value), "Should dequeue");
    
    // Can enqueue again
    SIMPLE_ASSERT(queue.enqueue(999), "Should enqueue after dequeue");
    
    return true;
}

// Test 4: Queue empty behavior
bool test_lock_free_queue_queue_empty() {
    LockFreeQueue<int, 16> queue;
    
    int value = 0;
    SIMPLE_ASSERT(!queue.dequeue(value), "Should fail when empty");
    
    queue.enqueue(42);
    queue.dequeue(value);
    
    SIMPLE_ASSERT(!queue.dequeue(value), "Should fail when empty again");
    
    return true;
}

// Test 5: Multi-threaded producer-consumer
bool test_lock_free_queue_mpmc() {
    LockFreeQueue<int, 1024> queue;
    constexpr int NUM_PRODUCERS = 4;
    constexpr int NUM_CONSUMERS = 4;
    constexpr int ITEMS_PER_PRODUCER = 1000;
    
    std::atomic<int> total_produced{0};
    std::atomic<int> total_consumed{0};
    std::vector<std::thread> threads;
    
    // Producers
    for (int t = 0; t < NUM_PRODUCERS; ++t) {
        threads.emplace_back([&queue, &total_produced, t]() {
            for (int i = 0; i < ITEMS_PER_PRODUCER; ++i) {
                while (!queue.enqueue(t * ITEMS_PER_PRODUCER + i)) {
                    std::this_thread::yield();
                }
                total_produced.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    
    // Consumers
    for (int t = 0; t < NUM_CONSUMERS; ++t) {
        threads.emplace_back([&queue, &total_consumed, expected=NUM_PRODUCERS*ITEMS_PER_PRODUCER]() {
            int value;
            while (total_consumed.load(std::memory_order_relaxed) < expected) {
                if (queue.dequeue(value)) {
                    total_consumed.fetch_add(1, std::memory_order_relaxed);
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    int expected = NUM_PRODUCERS * ITEMS_PER_PRODUCER;
    SIMPLE_ASSERT(total_produced.load() == expected, "All items should be produced");
    SIMPLE_ASSERT(total_consumed.load() == expected, "All items should be consumed");
    SIMPLE_ASSERT(queue.empty(), "Queue should be empty");
    
    return true;
}

// Test 6: Statistics tracking
bool test_lock_free_queue_statistics() {
    LockFreeQueue<int, 64> queue;
    
    // Enqueue some items
    for (int i = 0; i < 10; ++i) {
        queue.enqueue(i);
    }
    
    // Dequeue some items
    for (int i = 0; i < 5; ++i) {
        int value;
        queue.dequeue(value);
    }
    
    // Try to enqueue when full
    for (int i = 0; i < 60; ++i) {
        queue.enqueue(i);
    }
    queue.enqueue(999);  // This should fail
    
    auto stats = queue.stats();
    
    SIMPLE_ASSERT(stats.total_enqueues > 0, "Should track enqueues");
    SIMPLE_ASSERT(stats.total_dequeues == 5, "Should track dequeues");
    SIMPLE_ASSERT(stats.current_size == queue.size(), "Size should match");
    
    return true;
}

// Performance benchmarks
void benchmark_lock_free_queue() {
    std::cout << "\n" << colors::cyan() << "LockFreeQueue Benchmarks:" << colors::reset() << "\n\n";
    
    // Benchmark 1: Single-threaded enqueue/dequeue
    {
        LockFreeQueue<int, 1024> queue;
        
        double enqueue_time = measure_perf([&queue]() {
            queue.enqueue(42);
        }, 10000, 100);
        
        std::cout << "Single-threaded enqueue: " << format_time(enqueue_time) << "\n";
        
        double dequeue_time = measure_perf([&queue]() {
            int value;
            queue.dequeue(value);
        }, 10000, 100);
        
        std::cout << "Single-threaded dequeue: " << format_time(dequeue_time) << "\n";
    }
    
    // Benchmark 2: Contended throughput
    {
        LockFreeQueue<int, 4096> queue;
        constexpr int NUM_OPS = 100000;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        std::thread producer([&queue]() {
            for (int i = 0; i < NUM_OPS; ++i) {
                while (!queue.enqueue(i)) {
                    std::this_thread::yield();
                }
            }
        });
        
        std::thread consumer([&queue]() {
            int value;
            for (int i = 0; i < NUM_OPS; ++i) {
                while (!queue.dequeue(value)) {
                    std::this_thread::yield();
                }
            }
        });
        
        producer.join();
        consumer.join();
        
        auto end = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double>(end - start).count();
        double throughput = (2 * NUM_OPS) / elapsed;  // Both enq and deq
        
        std::cout << "Contended throughput: " << colors::bold() 
                  << static_cast<int>(throughput) << " ops/sec" 
                  << colors::reset() << "\n";
    }
}

bool test_LockFreeQueue() {

    PRINT_HEADER(LOCK-FREE QUEUE)

    TestRunner runner;

    RUN_TEST(runner, lock_free_queue_basic_operations);
    RUN_TEST(runner, lock_free_queue_fifo_ordering);
    RUN_TEST(runner, lock_free_queue_queue_full);
    RUN_TEST(runner, lock_free_queue_queue_empty);
    RUN_TEST(runner, lock_free_queue_mpmc);
    RUN_TEST(runner, lock_free_queue_statistics);

    benchmark_lock_free_queue();

    return 0 == runner.print_summary();

}

} // namespace cpp_utilities::testing 
