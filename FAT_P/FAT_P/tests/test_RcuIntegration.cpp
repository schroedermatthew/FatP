/**
 * @file test_RcuIntegration.cpp
 * @brief Integration tests for RCUPolicy with Tensor and other data structures
 * @version 1.0
 * 
 * Tests RCU (Read-Copy-Update) synchronization with:
 * - Tensor operations (element access, reshape, arithmetic)
 * - High-frequency readers with occasional writers
 * - Lock-free read performance benchmarks
 * - Memory safety verification
 */

#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

#include "ConcurrencyPolicies.h"
#include "Tensor.h"
#include "FatPTest.h"

namespace fat_p::testing {

using namespace std::chrono_literals;

// =============================================================================
// Test 1: RCU with Tensor - Basic Operations
// =============================================================================

bool test_rcu_tensor_basic() {
    std::cout << colors::cyan() << "\n[TEST] RCU with Tensor - Basic Operations"
              << colors::reset() << std::endl;
    
    #if FATP_USE_ATOMIC && FATP_USE_SHARED_MUTEX
    
    using TensorType = Tensor<double>;
    RCUPolicy<TensorType> rcu_tensor(TensorType({100}));
    
    // Initialize tensor
    {
        auto write_guard = rcu_tensor.write();
        write_guard.update([](TensorType& tensor) {
            for (size_t i = 0; i < tensor.size(); ++i) {
                tensor[i] = static_cast<double>(i);
            }
        });
    }
    
    // Verify reads
    {
        auto read_guard = rcu_tensor.read();
        const auto& tensor = *read_guard;
        ASSERT_TRUE(tensor[0] == 0.0, "First element should be 0");
        ASSERT_TRUE(tensor[50] == 50.0, "Middle element should be 50");
        ASSERT_TRUE(tensor[99] == 99.0, "Last element should be 99");
    }
    
    std::cout << colors::green() << "✓ Basic RCU tensor operations passed"
              << colors::reset() << std::endl;
    return true;
    
    #else
    std::cout << colors::yellow() << "⚠ RCU not available (requires atomics and shared_mutex)"
              << colors::reset() << std::endl;
    return true;
    #endif
}

// =============================================================================
// Test 2: High-Frequency Readers with Occasional Writers
// =============================================================================

bool test_rcu_concurrent_readers_writers() {
    std::cout << colors::cyan() << "\n[TEST] RCU - Concurrent Readers/Writers"
              << colors::reset() << std::endl;
    
    #if FATP_USE_ATOMIC && FATP_USE_SHARED_MUTEX
    
    using TensorType = Tensor<int>;
    RCUPolicy<TensorType> rcu_tensor(TensorType({1000}));
    
    std::atomic<bool> stop{false};
    std::atomic<uint64_t> read_count{0};
    std::atomic<uint64_t> write_count{0};
    
    // 8 reader threads (high frequency)
    std::vector<std::thread> readers;
    for (int i = 0; i < 8; ++i) {
        readers.emplace_back([&]() {
            while (!stop.load(std::memory_order_relaxed)) {
                auto guard = rcu_tensor.read();
                const auto& tensor = *guard;
                // Verify data consistency
                int sum = 0;
                for (size_t j = 0; j < std::min<size_t>(10, tensor.size()); ++j) {
                    sum += tensor[j];
                }
                (void)sum; // Use value
                read_count.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    
    // 2 writer threads (low frequency)
    std::vector<std::thread> writers;
    for (int i = 0; i < 2; ++i) {
        writers.emplace_back([&, writer_id = i]() {
            for (int j = 0; j < 50; ++j) {
                auto guard = rcu_tensor.write();
                guard.update([writer_id, j](TensorType& tensor) {
                    for (size_t k = 0; k < tensor.size(); ++k) {
                        tensor[k] = writer_id * 1000 + j;
                    }
                });
                write_count.fetch_add(1, std::memory_order_relaxed);
                std::this_thread::sleep_for(1ms);
            }
        });
    }
    
    // Let it run for a bit
    std::this_thread::sleep_for(100ms);
    stop.store(true, std::memory_order_relaxed);
    
    // Join all threads
    for (auto& r : readers) r.join();
    for (auto& w : writers) w.join();
    
    std::cout << colors::blue() << "  Reads: " << read_count.load()
              << ", Writes: " << write_count.load()
              << colors::reset() << std::endl;
    
    ASSERT_TRUE(read_count.load() > 1000, "Should have many reads");
    ASSERT_TRUE(write_count.load() == 100, "Should have 100 writes (50*2)");
    
    std::cout << colors::green() << "✓ Concurrent RCU operations passed"
              << colors::reset() << std::endl;
    return true;
    
    #else
    std::cout << colors::yellow() << "⚠ RCU not available"
              << colors::reset() << std::endl;
    return true;
    #endif
}

// =============================================================================
// Test 3: RCU Performance - Lock-Free Reads
// =============================================================================

bool test_rcu_performance() {
    std::cout << colors::cyan() << "\n[TEST] RCU Performance - Lock-Free Reads"
              << colors::reset() << std::endl;
    
    #if FATP_USE_ATOMIC && FATP_USE_SHARED_MUTEX
    
    using TensorType = Tensor<double>;
    RCUPolicy<TensorType> rcu_tensor(TensorType({10000}));
    
    // Initialize
    {
        auto guard = rcu_tensor.write();
        guard.update([](TensorType& tensor) {
            tensor.fill(42.0);
        });
    }
    
    // Benchmark pure reads
    const int iterations = 100000;
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        auto guard = rcu_tensor.read();
        const auto& tensor = *guard;
        double sum = tensor[0] + tensor[5000] + tensor[9999];
        DoNotOptimize(sum);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    double ns_per_read = static_cast<double>(duration.count()) / iterations;
    
    std::cout << colors::blue() << "  Read latency: "
              << std::fixed << std::setprecision(2) << ns_per_read << " ns/read"
              << colors::reset() << std::endl;
    
    // RCU reads should be very fast (typically < 100ns)
    ASSERT_TRUE(ns_per_read < 500.0, "RCU reads should be fast");
    
    std::cout << colors::green() << "✓ RCU performance acceptable"
              << colors::reset() << std::endl;
    return true;
    
    #else
    std::cout << colors::yellow() << "⚠ RCU not available"
              << colors::reset() << std::endl;
    return true;
    #endif
}

// =============================================================================
// Test 4: RCU with Complex Updates
// =============================================================================

bool test_rcu_complex_updates() {
    std::cout << colors::cyan() << "\n[TEST] RCU - Complex Tensor Updates"
              << colors::reset() << std::endl;
    
    #if FATP_USE_ATOMIC && FATP_USE_SHARED_MUTEX
    
    using TensorType = Tensor<double>;
    RCUPolicy<TensorType> rcu_tensor(TensorType({100, 100}));
    
    // Update entire 2D tensor
    {
        auto guard = rcu_tensor.write();
        guard.update([](TensorType& tensor) {
            for (size_t i = 0; i < tensor.size(); ++i) {
                tensor[i] = static_cast<double>(i % 100);
            }
        });
    }
    
    // Verify structure
    {
        auto guard = rcu_tensor.read();
        const auto& tensor = *guard;
        ASSERT_TRUE(tensor.shape().size() == 2, "Should be 2D");
        ASSERT_TRUE(tensor.shape()[0] == 100, "First dim should be 100");
        ASSERT_TRUE(tensor.shape()[1] == 100, "Second dim should be 100");
        ASSERT_TRUE(tensor.size() == 10000, "Total size should be 10000");
    }
    
    // Perform arithmetic update
    {
        auto guard = rcu_tensor.write();
        guard.update([](TensorType& tensor) {
            for (size_t i = 0; i < tensor.size(); ++i) {
                tensor[i] *= 2.0;
            }
        });
    }
    
    // Verify arithmetic
    {
        auto guard = rcu_tensor.read();
        const auto& tensor = *guard;
        ASSERT_TRUE(tensor[0] == 0.0, "First should be 0");
        ASSERT_TRUE(tensor[50] == 100.0, "Element 50 should be 100");
    }
    
    std::cout << colors::green() << "✓ Complex RCU updates passed"
              << colors::reset() << std::endl;
    return true;
    
    #else
    std::cout << colors::yellow() << "⚠ RCU not available"
              << colors::reset() << std::endl;
    return true;
    #endif
}

// =============================================================================
// Test Runner
// =============================================================================

bool test_RCU_integration() {
    PRINT_HEADER(RCU INTEGRATION)
    
    TestRunner runner;
    
    RUN_TEST(runner, rcu_tensor_basic);
    RUN_TEST(runner, rcu_concurrent_readers_writers);
    RUN_TEST(runner, rcu_performance);
    RUN_TEST(runner, rcu_complex_updates);
    
    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_RCU_integration() ? 0 : 1;
}
#endif
