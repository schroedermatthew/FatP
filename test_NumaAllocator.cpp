#include <iostream>
#include <thread>
#include <numeric>

#include "NumaAllocator.h"
#include "test_NumaAllocator.h"
#include "test_Utilities.h"

namespace cpp_utilities::testing
{

using namespace memory;

bool test_numa_info() {
    std::cout << "NUMA available: " << (NumaInfo::is_available() ? "Yes" : "No") << "\n";
    std::cout << "NUMA nodes: " << NumaInfo::num_nodes() << "\n";
    std::cout << "Current NUMA node: " << NumaInfo::current_node() << "\n";
    
    SIMPLE_ASSERT(NumaInfo::num_nodes() >= 1, "Should have at least 1 NUMA node");
    SIMPLE_ASSERT(NumaInfo::current_node() >= 0, "Current node should be valid");
    
    return true;
}

bool test_numa_local_allocator() {
    NumaAllocator<int, NumaLocalPolicy> alloc;
    
    int* ptr = alloc.allocate(100);
    SIMPLE_ASSERT(ptr != nullptr, "Allocation should succeed");
    
    // Initialize memory
    for (int i = 0; i < 100; ++i) {
        ptr[i] = i;
    }
    
    // Verify
    for (int i = 0; i < 100; ++i) {
        SIMPLE_ASSERT(ptr[i] == i, "Memory should be accessible and correct");
    }
    
    alloc.deallocate(ptr, 100);
    
    return true;
}

bool test_numa_interleaved_allocator() {
    NumaAllocator<double, NumaInterleavedPolicy> alloc;
    
    double* ptr = alloc.allocate(1000);
    SIMPLE_ASSERT(ptr != nullptr, "Interleaved allocation should succeed");
    
    for (int i = 0; i < 1000; ++i) {
        ptr[i] = static_cast<double>(i) * 1.5;
    }
    
    double sum = 0.0;
    for (int i = 0; i < 1000; ++i) {
        sum += ptr[i];
    }
    
    SIMPLE_ASSERT(sum > 0.0, "Memory should be usable");
    
    alloc.deallocate(ptr, 1000);
    
    return true;
}

bool test_numa_preferred_allocator() {
    int preferred_node = 0; // First node
    NumaAllocator<float, NumaPreferredPolicy> alloc(NumaPreferredPolicy{preferred_node});
    
    float* ptr = alloc.allocate(500);
    SIMPLE_ASSERT(ptr != nullptr, "Preferred node allocation should succeed");
    
    for (int i = 0; i < 500; ++i) {
        ptr[i] = static_cast<float>(i);
    }
    
    alloc.deallocate(ptr, 500);
    
    return true;
}

bool test_numa_local_vector() {
    NumaLocalVector<int> vec;
    
    for (int i = 0; i < 100; ++i) {
        vec.push_back(i);
    }
    
    SIMPLE_ASSERT(vec.size() == 100, "Vector should have 100 elements");
    
    int sum = std::accumulate(vec.begin(), vec.end(), 0);
    SIMPLE_ASSERT(sum == 4950, "Sum should be correct (0+1+...+99 = 4950)");
    
    return true;
}

bool test_numa_interleaved_vector() {
    NumaInterleavedVector<double> vec(1000, 1.5);
    
    SIMPLE_ASSERT(vec.size() == 1000, "Vector should have 1000 elements");
    SIMPLE_ASSERT(vec[0] == 1.5, "Elements should be initialized");
    
    return true;
}

bool test_numa_preferred_vector() {
    int node = 0;
    NumaPreferredVector<int> vec(node);
    
    vec.resize(100);
    std::iota(vec.begin(), vec.end(), 0);
    
    SIMPLE_ASSERT(vec.size() == 100, "Vector should have 100 elements");
    SIMPLE_ASSERT(vec[50] == 50, "Elements should be correct");
    
    return true;
}

bool test_numa_memory_stats() {
    for (int node = 0; node < NumaInfo::num_nodes(); ++node) {
        auto stats = get_node_memory_stats(node);
        std::cout << "Node " << node << " - Total: " << stats.total_bytes 
                  << ", Free: " << stats.free_bytes 
                  << ", Used: " << stats.used_bytes << "\n";
    }
    
    return true;
}

bool test_thread_local_numa_pool() {
    // Test basic allocation
    int* ptr1 = ThreadLocalNumaPool<int>::allocate(10);
    SIMPLE_ASSERT(ptr1 != nullptr, "Pool allocation should succeed");
    
    for (int i = 0; i < 10; ++i) {
        ptr1[i] = i;
    }
    
    int node = ThreadLocalNumaPool<int>::numa_node();
    SIMPLE_ASSERT(node >= 0, "NUMA node should be valid");
    
    // Test across multiple threads
    std::thread t1([]() {
        int* ptr = ThreadLocalNumaPool<int>::allocate(5);
        int node = ThreadLocalNumaPool<int>::numa_node();
        DoNotOptimize(ptr);
        DoNotOptimize(node);
    });
    
    std::thread t2([]() {
        int* ptr = ThreadLocalNumaPool<int>::allocate(5);
        int node = ThreadLocalNumaPool<int>::numa_node();
        DoNotOptimize(ptr);
        DoNotOptimize(node);
    });
    
    t1.join();
    t2.join();
    
    return true;
}

void benchmark_numa_allocator() {
    std::cout << "\n" << colors::cyan() << "NumaAllocator Benchmarks:" << colors::reset() << "\n\n";
    
    constexpr size_t N = 10000;
    
    // Benchmark NUMA-local allocation
    double local_time = measure_perf([]() {
        NumaAllocator<int, NumaLocalPolicy> alloc;
        int* ptr = alloc.allocate(N);
        for (size_t i = 0; i < N; ++i) {
            ptr[i] = static_cast<int>(i);
        }
        alloc.deallocate(ptr, N);
    }, 1000, 10);
    
    std::cout << "NUMA-local allocation (" << N << " ints): " << format_time(local_time) << "\n";
    
    // Benchmark vector operations
    double vec_time = measure_perf([]() {
        NumaLocalVector<int> vec;
        for (int i = 0; i < 1000; ++i) {
            vec.push_back(i);
        }
        DoNotOptimize(vec);
    }, 10000, 100);
    
    std::cout << "NumaLocalVector push_back (1000 elements): " << format_time(vec_time) << "\n";
}

bool test_NumaAllocator() {

    PRINT_HEADER(NUMA ALLOCATOR)

    TestRunner runner;

    RUN_TEST(runner, numa_info);
    RUN_TEST(runner, numa_local_allocator);
    RUN_TEST(runner, numa_interleaved_allocator);
    RUN_TEST(runner, numa_preferred_allocator);
    RUN_TEST(runner, numa_local_vector);
    RUN_TEST(runner, numa_interleaved_vector);
    RUN_TEST(runner, numa_preferred_vector);
    RUN_TEST(runner, numa_memory_stats);
    RUN_TEST(runner, thread_local_numa_pool);

    benchmark_numa_allocator();

    return 0 == runner.print_summary();
}

} // namespace cpp_utilities::testing
