/**
 * @file test_TensorStorage.cpp
 * @brief Comprehensive unit tests for TensorStorage.h
 */

#include "TensorStorage.h"
#include "FatPTest.h"
#include "AlignedVector.h"
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>

namespace fat_p::testing::tensorstorage
{

// =============================================================================
// Basic Tests
// =============================================================================

TEST_CASE(construction) {
    using Alloc = AlignedAllocator<float, 64>;
    Alloc alloc;
    
    // Default construction
    TensorStorage<float, Alloc> storage1;
    ASSERT_NULLPTR(storage1.get(), "Default storage should be null");
    ASSERT_EQ(storage1.use_count(), 0L, "Default storage should have 0 ref count");
    ASSERT_FALSE(static_cast<bool>(storage1), "Default storage should be falsy");
    
    // Construction with data
    float* data = alloc.allocate(10);
    for (int i = 0; i < 10; ++i) data[i] = static_cast<float>(i);
    
    TensorStorage<float, Alloc> storage2(data, 10, alloc);
    ASSERT_NOT_NULLPTR(storage2.get(), "Storage should not be null");
    ASSERT_EQ(storage2.use_count(), 1L, "Initial ref count should be 1");
    ASSERT_TRUE(static_cast<bool>(storage2), "Storage should be truthy");
    ASSERT_EQ(storage2[0], 0.0f, "First element should be 0");
    ASSERT_EQ(storage2[9], 9.0f, "Last element should be 9");
    
    return true;
}

TEST_CASE(copy) {
    using Alloc = AlignedAllocator<double, 64>;
    Alloc alloc;
    
    double* data = alloc.allocate(5);
    for (int i = 0; i < 5; ++i) data[i] = i * 2.0;
    
    TensorStorage<double, Alloc> storage1(data, 5, alloc);
    ASSERT_EQ(storage1.use_count(), 1L, "Initial ref count should be 1");
    
    // Copy construction
    TensorStorage<double, Alloc> storage2(storage1);
    ASSERT_EQ(storage1.use_count(), 2L, "Ref count should be 2 after copy");
    ASSERT_EQ(storage2.use_count(), 2L, "Copy should share ref count");
    ASSERT_EQ(storage1.get(), storage2.get(), "Pointers should be same");
    ASSERT_FALSE(storage1.unique(), "Storage1 should not be unique");
    ASSERT_FALSE(storage2.unique(), "Storage2 should not be unique");
    
    // Copy assignment
    TensorStorage<double, Alloc> storage3;
    storage3 = storage1;
    ASSERT_EQ(storage1.use_count(), 3L, "Ref count should be 3 after assignment");
    ASSERT_EQ(storage3.get(), storage1.get(), "Pointers should match");
    
    // Verify data integrity
    ASSERT_EQ(storage2[2], 4.0, "Data should be accessible through all copies");
    ASSERT_EQ(storage3[4], 8.0, "Data should be accessible through all copies");
    
    return true;
}

TEST_CASE(move) {
    using Alloc = AlignedAllocator<int, 32>;
    Alloc alloc;
    
    int* data = alloc.allocate(3);
    data[0] = 100;
    data[1] = 200;
    data[2] = 300;
    
    TensorStorage<int, Alloc> storage1(data, 3, alloc);
    int* original_ptr = storage1.get();
    ASSERT_EQ(storage1.use_count(), 1L, "Initial ref count should be 1");
    
    // Move construction
    TensorStorage<int, Alloc> storage2(std::move(storage1));
    ASSERT_NULLPTR(storage1.get(), "Moved-from storage should be null");
    ASSERT_EQ(storage1.use_count(), 0L, "Moved-from should have 0 ref count");
    ASSERT_EQ(storage2.get(), original_ptr, "Moved-to should have original pointer");
    ASSERT_EQ(storage2.use_count(), 1L, "Moved-to should have ref count 1");
    ASSERT_TRUE(storage2.unique(), "Moved-to should be unique");
    
    // Move assignment
    TensorStorage<int, Alloc> storage3;
    storage3 = std::move(storage2);
    ASSERT_NULLPTR(storage2.get(), "Moved-from storage should be null");
    ASSERT_EQ(storage3.get(), original_ptr, "Moved-to should have original pointer");
    ASSERT_EQ(storage3[1], 200, "Data should be intact after move");
    
    return true;
}

TEST_CASE(reset) {
    using Alloc = AlignedAllocator<float, 64>;
    Alloc alloc;
    
    float* data1 = alloc.allocate(4);
    for (int i = 0; i < 4; ++i) data1[i] = static_cast<float>(i);
    
    TensorStorage<float, Alloc> storage(data1, 4, alloc);
    ASSERT_EQ(storage.use_count(), 1L, "Initial ref count should be 1");
    
    // Reset to null
    storage.reset();
    ASSERT_NULLPTR(storage.get(), "Reset should null the pointer");
    ASSERT_EQ(storage.use_count(), 0L, "Reset should have 0 ref count");
    
    // Reset with new data
    float* data2 = alloc.allocate(3);
    data2[0] = 10.0f;
    data2[1] = 20.0f;
    data2[2] = 30.0f;
    
    storage.reset(data2, 3, alloc);
    ASSERT_NOT_NULLPTR(storage.get(), "Reset with data should set pointer");
    ASSERT_EQ(storage.use_count(), 1L, "Reset should have ref count 1");
    ASSERT_EQ(storage[1], 20.0f, "New data should be accessible");
    
    return true;
}

TEST_CASE(unique) {
    using Alloc = AlignedAllocator<double, 64>;
    Alloc alloc;
    
    double* data = alloc.allocate(2);
    data[0] = 1.5;
    data[1] = 2.5;
    
    TensorStorage<double, Alloc> storage1(data, 2, alloc);
    ASSERT_TRUE(storage1.unique(), "Single owner should be unique");
    
    TensorStorage<double, Alloc> storage2 = storage1;
    ASSERT_FALSE(storage1.unique(), "Multiple owners should not be unique");
    ASSERT_FALSE(storage2.unique(), "Multiple owners should not be unique");
    
    storage2.reset();
    ASSERT_TRUE(storage1.unique(), "After release, should be unique again");
    
    return true;
}

// =============================================================================
// Multi-threaded Tests
// =============================================================================

TEST_CASE(concurrent_copy) {
    using Alloc = AlignedAllocator<int, 64>;
    Alloc alloc;
    
    int* data = alloc.allocate(1000);
    for (int i = 0; i < 1000; ++i) data[i] = i;
    
    TensorStorage<int, Alloc> original(data, 1000, alloc);
    
    constexpr int num_threads = 8;
    constexpr int copies_per_thread = 100;
    
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    
    // Each thread creates and destroys many copies
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&original, &success_count]() {
            for (int i = 0; i < copies_per_thread; ++i) {
                TensorStorage<int, Alloc> copy = original;
                
                // Verify data integrity
                if (copy[500] == 500 && copy.get() == original.get()) {
                    success_count.fetch_add(1, std::memory_order_relaxed);
                }
                
                // Random delay to increase contention
                if (i % 10 == 0) {
                    std::this_thread::yield();
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    int expected = num_threads * copies_per_thread;
    ASSERT_EQ(success_count.load(), expected, 
                 "All concurrent copies should succeed");
    
    // Original should still be valid
    ASSERT_EQ(original.use_count(), 1L, "Original should be sole owner after threads finish");
    ASSERT_EQ(original[999], 999, "Original data should be intact");
    
    return true;
}

TEST_CASE(concurrent_mixed_ops) {
    using Alloc = AlignedAllocator<float, 64>;
    Alloc alloc;
    
    float* data = alloc.allocate(100);
    for (int i = 0; i < 100; ++i) data[i] = static_cast<float>(i);
    
    TensorStorage<float, Alloc> shared(data, 100, alloc);
    
    constexpr int num_threads = 4;
    std::vector<std::thread> threads;
    std::atomic<bool> error_flag{false};
    
    // Mix of copy, move, and access operations
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&shared, &error_flag, t]() {
            for (int i = 0; i < 50; ++i) {
                // Create copy
                TensorStorage<float, Alloc> copy1 = shared;
                
                // Verify access
                if (copy1[t] != static_cast<float>(t)) {
                    error_flag.store(true, std::memory_order_relaxed);
                }
                
                // Create another copy
                TensorStorage<float, Alloc> copy2 = copy1;
                
                // Move operation
                TensorStorage<float, Alloc> copy3 = std::move(copy2);
                
                // Verify again
                if (copy3[t * 10 % 100] != static_cast<float>(t * 10 % 100)) {
                    error_flag.store(true, std::memory_order_relaxed);
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    ASSERT_FALSE(error_flag.load(), "No data corruption should occur");
    ASSERT_EQ(shared.use_count(), 1L, "Shared should be sole owner after test");
    
    return true;
}

// =============================================================================
// Performance Benchmarks
// =============================================================================

void benchmark_tensor_storage() {
    std::cout << "\n" << colors::cyan() << "TensorStorage Benchmarks:" << colors::reset() << "\n\n";
    
    using Alloc = AlignedAllocator<double, 64>;
    Alloc alloc;
    
    constexpr size_t size = 1000;
    double* data = alloc.allocate(size);
    for (size_t i = 0; i < size; ++i) data[i] = static_cast<double>(i);
    
    // Benchmark copy construction
    {
        TensorStorage<double, Alloc> original(data, size, alloc);
        
        double copy_time = measure_perf([&original]() {
            TensorStorage<double, Alloc> copy = original;
            DoNotOptimize(copy);
        }, 1000000, 100);
        
        std::cout << "Copy construction: " << format_time(copy_time) << "\n";
    }
    
    // Benchmark access
    {
        double* new_data = alloc.allocate(size);
        for (size_t i = 0; i < size; ++i) new_data[i] = static_cast<double>(i);
        
        TensorStorage<double, Alloc> storage(new_data, size, alloc);
        
        double access_time = measure_perf([&storage, i=0]() mutable {
            double val = storage[i % 1000];
            DoNotOptimize(val);
            ++i;
        }, 1000000, 100);
        
        std::cout << "Element access: " << format_time(access_time) << "\n";
    }
    
    // Benchmark ref count operations
    {
        double* data3 = alloc.allocate(size);
        TensorStorage<double, Alloc> original(data3, size, alloc);
        
        double ref_time = measure_perf([&original]() {
            TensorStorage<double, Alloc> copy = original;
            long count = copy.use_count();
            DoNotOptimize(count);
        }, 100000, 100);
        
        std::cout << "Copy + use_count: " << format_time(ref_time) << "\n";
    }
}

} // namespace fat_p::testing::tensorstorage

// =============================================================================
// Public Interface
// =============================================================================

namespace fat_p::testing
{

bool test_TensorStorage() {
    PRINT_HEADER(TENSOR STORAGE)
    
    TestRunner runner;
    
    RUN_TEST_NS(runner, tensorstorage, construction);
    RUN_TEST_NS(runner, tensorstorage, copy);
    RUN_TEST_NS(runner, tensorstorage, move);
    RUN_TEST_NS(runner, tensorstorage, reset);
    RUN_TEST_NS(runner, tensorstorage, unique);
    
    // Multi-threaded tests
    RUN_TEST_NS(runner, tensorstorage, concurrent_copy);
    RUN_TEST_NS(runner, tensorstorage, concurrent_mixed_ops);
    
    tensorstorage::benchmark_tensor_storage();
    
    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_TensorStorage() ? 0 : 1;
}
#endif
