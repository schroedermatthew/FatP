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

#include <iostream>
#include <thread>
#include <vector>
#include <atomic>

#include "LockFreeQueue.h"
#include "FatPTest.h"

namespace fat_p::testing::lockfreequeue
{

// Test 1: Basic enqueue/dequeue
FATP_TEST_CASE(lock_free_queue_basic_operations) {
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

// Test 2: FIFO ordering
FATP_TEST_CASE(lock_free_queue_fifo_ordering) {
    LockFreeQueue<int, 128> queue;
    
    for (int i = 0; i < 10; ++i) {
        FATP_ASSERT_TRUE(queue.enqueue(i), "Should enqueue");
    }
    
    for (int i = 0; i < 10; ++i) {
        int value = -1;
        FATP_ASSERT_TRUE(queue.dequeue(value), "Should dequeue");
        FATP_ASSERT_TRUE(value == i, "Values should be in FIFO order");
    }
    
    return true;
}

// Test 3: Queue full behavior
FATP_TEST_CASE(lock_free_queue_queue_full) {
    LockFreeQueue<int, 4> queue;
    
    // Fill queue
    for (int i = 0; i < 4; ++i) {
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

// Test 4: Queue empty behavior
FATP_TEST_CASE(lock_free_queue_queue_empty) {
    LockFreeQueue<int, 16> queue;
    
    int value = 0;
    FATP_ASSERT_TRUE(!queue.dequeue(value), "Should fail when empty");
    
    queue.enqueue(42);
    queue.dequeue(value);
    
    FATP_ASSERT_TRUE(!queue.dequeue(value), "Should fail when empty again");
    
    return true;
}

// Test 5: Multi-threaded producer-consumer
FATP_TEST_CASE(lock_free_queue_mpmc) {
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
    FATP_ASSERT_TRUE(total_produced.load() == expected, "All items should be produced");
    FATP_ASSERT_TRUE(total_consumed.load() == expected, "All items should be consumed");
    FATP_ASSERT_TRUE(queue.empty(), "Queue should be empty");
    
    return true;
}

// Test 6: Statistics tracking
FATP_TEST_CASE(lock_free_queue_statistics) {
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
    
    FATP_ASSERT_TRUE(stats.total_enqueues > 0, "Should track enqueues");
    FATP_ASSERT_TRUE(stats.total_dequeues == 5, "Should track dequeues");
    FATP_ASSERT_TRUE(stats.current_size == queue.size(), "Size should match");
    
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

} // namespace fat_p::testing::lockfreequeue

namespace fat_p::testing
{

bool test_LockFreeQueue() {

    FATP_PRINT_HEADER(LOCK-FREE QUEUE)

    TestRunner runner;

    FATP_RUN_TEST_NS(runner, lockfreequeue, lock_free_queue_basic_operations);
    FATP_RUN_TEST_NS(runner, lockfreequeue, lock_free_queue_fifo_ordering);
    FATP_RUN_TEST_NS(runner, lockfreequeue, lock_free_queue_queue_full);
    FATP_RUN_TEST_NS(runner, lockfreequeue, lock_free_queue_queue_empty);
    FATP_RUN_TEST_NS(runner, lockfreequeue, lock_free_queue_mpmc);
    FATP_RUN_TEST_NS(runner, lockfreequeue, lock_free_queue_statistics);

    lockfreequeue::benchmark_lock_free_queue();

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
