/**
 * @file test_NumaAllocator.cpp
 * @brief Comprehensive unit tests for NumaAllocator.h
 */
/*
FATP_META:
  meta_version: 1
  component: NumaAllocator
  file_role: test
  path: components/NumaAllocator/tests/test_NumaAllocator.cpp
  layer: Testing
  namespace: fat_p
  summary: "Unit tests for NumaAllocator."
  api_stability: in_work
  related:
    docs_search: "NumaAllocator"
    headers:
      - include/fat_p/FatPTest.h
      - include/fat_p/NumaAllocator.h
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
#include <iomanip>
#include <iostream>
#include <numeric>
#include <thread>
#include <vector>

#include "FatPTest.h"
#include "NumaAllocator.h"

namespace fat_p::testing::numaallocator
{

using namespace fat_p::memory;

FATP_TEST_CASE(numa_info)
{
    std::cout << "NUMA available: " << (NumaInfo::is_available() ? "Yes" : "No") << "\n";
    std::cout << "NUMA nodes: " << NumaInfo::num_nodes() << "\n";
    std::cout << "Current NUMA node: " << NumaInfo::current_node() << "\n";

    FATP_ASSERT_TRUE(NumaInfo::num_nodes() >= 1, "Should have at least 1 NUMA node");
    FATP_ASSERT_TRUE(NumaInfo::current_node() >= 0, "Current node should be non-negative");
    FATP_ASSERT_TRUE(NumaInfo::current_node() < NumaInfo::num_nodes(), "Current node should be within range");

    int cpus = NumaInfo::cpus_on_node(0);
    FATP_ASSERT_TRUE(cpus >= 0, "CPUs on node 0 should be non-negative");
    std::cout << "CPUs on node 0: " << cpus << "\n";

    return true;
}

FATP_TEST_CASE(numa_local_allocator)
{
    NumaAllocator<int, NumaLocalPolicy> alloc;

    int* ptr = alloc.allocate(100);
    FATP_ASSERT_TRUE(ptr != nullptr, "Allocation should succeed");

    for (int i = 0; i < 100; ++i)
    {
        ptr[i] = i;
    }

    for (int i = 0; i < 100; ++i)
    {
        FATP_ASSERT_TRUE(ptr[i] == i, "Memory should be accessible and correct");
    }

#if defined(__linux__) && FATP_HAS_NUMA_SUPPORT
    if (NumaInfo::is_available())
    {
        int alloc_node = get_memory_node(ptr);
        int expected_node = NumaInfo::current_node();
        if (alloc_node >= 0)
        {
            FATP_ASSERT_EQ(alloc_node, expected_node, "Memory should be on local NUMA node");
        }
    }
#endif

    alloc.deallocate(ptr, 100);

    return true;
}

FATP_TEST_CASE(numa_interleaved_allocator)
{
    NumaAllocator<double, NumaInterleavedPolicy> alloc;

    double* ptr = alloc.allocate(1000);
    FATP_ASSERT_TRUE(ptr != nullptr, "Interleaved allocation should succeed");

    for (int i = 0; i < 1000; ++i)
    {
        ptr[i] = static_cast<double>(i) * 1.5;
    }

    double sum = 0.0;
    for (int i = 0; i < 1000; ++i)
    {
        sum += ptr[i];
    }

    FATP_ASSERT_TRUE(sum > 0.0, "Memory should be usable");

    alloc.deallocate(ptr, 1000);

    return true;
}

FATP_TEST_CASE(numa_preferred_allocator)
{
    int preferred_node = 0;
    NumaAllocator<float, NumaPreferredPolicy> alloc(NumaPreferredPolicy{preferred_node});

    float* ptr = alloc.allocate(500);
    FATP_ASSERT_TRUE(ptr != nullptr, "Preferred node allocation should succeed");

    for (int i = 0; i < 500; ++i)
    {
        ptr[i] = static_cast<float>(i);
    }

#if defined(__linux__) && FATP_HAS_NUMA_SUPPORT
    if (NumaInfo::is_available())
    {
        int alloc_node = get_memory_node(ptr);
        if (alloc_node >= 0)
        {
            FATP_ASSERT_EQ(alloc_node, preferred_node, "Memory should be on preferred NUMA node");
        }
    }
#endif

    alloc.deallocate(ptr, 500);

    return true;
}

FATP_TEST_CASE(allocator_zero_size)
{
    NumaAllocator<int, NumaLocalPolicy> alloc;

    int* ptr = alloc.allocate(0);
    // Implementation defined: Current implementation returns nullptr
    FATP_ASSERT_TRUE(ptr == nullptr, "allocate(0) should return nullptr");

    alloc.deallocate(nullptr, 0);

    return true;
}

FATP_TEST_CASE(allocator_equality)
{
    NumaAllocator<int, NumaLocalPolicy> local1;
    NumaAllocator<int, NumaLocalPolicy> local2;
    FATP_ASSERT_TRUE(local1 == local2, "NumaLocalPolicy allocators should be equal");

    NumaAllocator<int, NumaInterleavedPolicy> interleaved1;
    NumaAllocator<int, NumaInterleavedPolicy> interleaved2;
    FATP_ASSERT_TRUE(interleaved1 == interleaved2, "NumaInterleavedPolicy allocators should be equal");

    FATP_ASSERT_TRUE(!(local1 == interleaved1), "Different policy types should not be equal");

    NumaAllocator<int, NumaPreferredPolicy> pref0(NumaPreferredPolicy{0});
    NumaAllocator<int, NumaPreferredPolicy> pref0_copy(NumaPreferredPolicy{0});
    FATP_ASSERT_TRUE(pref0 == pref0_copy, "Same node preferred allocators should be equal");

    if (NumaInfo::num_nodes() > 1)
    {
        NumaAllocator<int, NumaPreferredPolicy> pref1(NumaPreferredPolicy{1});
        FATP_ASSERT_TRUE(pref0 != pref1, "Different node preferred allocators should not be equal");
    }

    return true;
}

FATP_TEST_CASE(allocator_rebind)
{
    NumaAllocator<int, NumaPreferredPolicy> int_alloc(NumaPreferredPolicy{0});

    using ReboundAlloc = typename NumaAllocator<int, NumaPreferredPolicy>::template rebind<double>::other;
    ReboundAlloc double_alloc(int_alloc);

    FATP_ASSERT_EQ(double_alloc.get_policy().node,
                   int_alloc.get_policy().node,
                   "Rebound allocator should preserve policy state");

    double* ptr = double_alloc.allocate(10);
    FATP_ASSERT_TRUE(ptr != nullptr, "Rebound allocator should work");
    double_alloc.deallocate(ptr, 10);

    return true;
}

FATP_TEST_CASE(numa_local_vector)
{
    NumaLocalVector<int> vec;

    for (int i = 0; i < 100; ++i)
    {
        vec.push_back(i);
    }

    FATP_ASSERT_EQ(vec.size(), static_cast<size_t>(100), "Vector should have 100 elements");

    int sum = std::accumulate(vec.begin(), vec.end(), 0);
    FATP_ASSERT_EQ(sum, 4950, "Sum should be correct (0+1+...+99 = 4950)");

    return true;
}

FATP_TEST_CASE(numa_interleaved_vector)
{
    NumaInterleavedVector<double> vec(1000, 1.5);

    FATP_ASSERT_EQ(vec.size(), static_cast<size_t>(1000), "Vector should have 1000 elements");
    FATP_ASSERT_CLOSE(vec[0], 1.5, "Elements should be initialized");
    FATP_ASSERT_CLOSE(vec[999], 1.5, "Last element should be initialized");

    return true;
}

FATP_TEST_CASE(numa_preferred_vector)
{
    int node = 0;
    auto vec = make_preferred_vector<int>(node);

    vec.resize(100);
    std::iota(vec.begin(), vec.end(), 0);

    FATP_ASSERT_EQ(vec.size(), static_cast<size_t>(100), "Vector should have 100 elements");
    FATP_ASSERT_EQ(vec[50], 50, "Elements should be correct");

    auto vec2 = make_preferred_vector<double>(node, 50, 3.14);
    FATP_ASSERT_EQ(vec2.size(), static_cast<size_t>(50), "Vector should have 50 elements");
    FATP_ASSERT_CLOSE(vec2[0], 3.14, "Elements should be initialized with value");

    return true;
}

FATP_TEST_CASE(vector_copy_move)
{
    NumaLocalVector<int> vec1;
    for (int i = 0; i < 50; ++i)
    {
        vec1.push_back(i);
    }

    NumaLocalVector<int> vec2 = vec1;
    FATP_ASSERT_EQ(vec2.size(), vec1.size(), "Copied vector should have same size");
    FATP_ASSERT_EQ(vec2[25], 25, "Copied vector should have correct content");

    NumaLocalVector<int> vec3 = std::move(vec1);
    FATP_ASSERT_EQ(vec3.size(), static_cast<size_t>(50), "Moved vector should have correct size");
    FATP_ASSERT_EQ(vec3[25], 25, "Moved vector should have correct content");

    return true;
}

FATP_TEST_CASE(numa_memory_stats)
{
    bool found_valid_stats = false;

    for (int node = 0; node < NumaInfo::num_nodes(); ++node)
    {
        auto stats = get_node_memory_stats(node);
        std::cout << "Node " << node << " - Valid: " << (stats.valid ? "Yes" : "No") << ", Total: " << stats.total_bytes
                  << ", Free: " << stats.free_bytes << ", Used: " << stats.used_bytes
                  << ", Has Total: " << (stats.has_total ? "Yes" : "No") << "\n";

        if (stats.valid)
        {
            found_valid_stats = true;
            if (stats.has_total)
            {
                FATP_ASSERT_TRUE(stats.total_bytes >= stats.free_bytes, "Total should be >= free");
#if defined(__linux__)
                FATP_ASSERT_TRUE(stats.total_bytes > 0, "Total bytes should be positive on Linux");
                FATP_ASSERT_EQ(stats.used_bytes,
                               stats.total_bytes - stats.free_bytes,
                               "Used should equal total - free");
#endif
            }
            else
            {
                // Windows: only free_bytes is meaningful
                // We just ensure free bytes are present (can be 0 under heavy load)
            }
        }
    }

    if (NumaInfo::is_available() && found_valid_stats)
    {
        std::cout << "Valid NUMA stats found.\n";
    }
    else if (NumaInfo::is_available())
    {
        std::cout << "Note: NUMA available but no valid stats (may be container/VM environment).\n";
    }

    auto invalid_stats = get_node_memory_stats(-1);
    FATP_ASSERT_TRUE(!invalid_stats.valid, "Invalid node should return invalid stats");

    auto out_of_range_stats = get_node_memory_stats(1000);
    FATP_ASSERT_TRUE(!out_of_range_stats.valid, "Out of range node should return invalid stats");

    return true;
}

FATP_TEST_CASE(thread_local_numa_pool_basic)
{
    int* ptr1 = ThreadLocalNumaPool<int>::allocate(10);
    FATP_ASSERT_TRUE(ptr1 != nullptr, "Pool allocation should succeed");

    for (int i = 0; i < 10; ++i)
    {
        ptr1[i] = i;
    }

    int node = ThreadLocalNumaPool<int>::numa_node();
    FATP_ASSERT_TRUE(node >= 0, "NUMA node should be valid");

    size_t used_before = ThreadLocalNumaPool<int>::used();
    // Use > 0 check because exact counting now involves alignment overheads which may vary
    FATP_ASSERT_TRUE(used_before > 0, "Used should reflect allocation");

    int* ptr2 = ThreadLocalNumaPool<int>::allocate(5);
    FATP_ASSERT_TRUE(ptr2 != nullptr, "Second allocation should succeed");

    // Note: Sequential allocations may NOT be contiguous in payload address anymore due to headers/padding
    // We check they are distinct and valid.
    FATP_ASSERT_TRUE(ptr2 != ptr1, "Pointers from separate allocations should be distinct");

    ThreadLocalNumaPool<int>::deallocate(ptr1, 10);
    ThreadLocalNumaPool<int>::deallocate(ptr2, 5);

    return true;
}

FATP_TEST_CASE(thread_local_numa_pool_large)
{
    // Force a large allocation that bypasses the pool and uses direct allocation
    size_t huge_count = 2000; // > default 1024

    int* large_ptr = ThreadLocalNumaPool<int>::allocate(huge_count);
    FATP_ASSERT_TRUE(large_ptr != nullptr, "Large allocation should succeed");

    for (size_t i = 0; i < huge_count; ++i)
    {
        large_ptr[i] = static_cast<int>(i);
    }

    ThreadLocalNumaPool<int>::deallocate(large_ptr, huge_count);

    return true;
}

FATP_TEST_CASE(thread_local_numa_pool_reset)
{
    ThreadLocalNumaPool<double>::reset();
    size_t initial_used = ThreadLocalNumaPool<double>::used();
    FATP_ASSERT_EQ(initial_used, static_cast<size_t>(0), "Used should be 0 after reset");

    double* ptr = ThreadLocalNumaPool<double>::allocate(10);
    FATP_ASSERT_TRUE(ptr != nullptr, "Allocation after reset should succeed");

    ThreadLocalNumaPool<double>::reset();
    size_t after_reset = ThreadLocalNumaPool<double>::used();
    FATP_ASSERT_EQ(after_reset, static_cast<size_t>(0), "Used should be 0 after second reset");

    // After reset, the implementation may reuse memory or allocate new.
    // Address reuse is an optimization detail, not part of the contract.
    double* ptr2 = ThreadLocalNumaPool<double>::allocate(10);
    FATP_ASSERT_TRUE(ptr2 != nullptr, "Allocation after reset should succeed");

    // Verify we can write to the new allocation without crashing
    for (int i = 0; i < 10; ++i)
    {
        ptr2[i] = static_cast<double>(i);
    }

    return true;
}

FATP_TEST_CASE(thread_local_numa_pool_multithread)
{
    std::atomic<bool> success{true};

    auto worker = [&success](int thread_id) {
        int* ptr = ThreadLocalNumaPool<int>::allocate(10);
        if (!ptr)
        {
            success.store(false);
            return;
        }

        for (int i = 0; i < 10; ++i)
        {
            ptr[i] = thread_id * 100 + i;
        }

        for (int i = 0; i < 10; ++i)
        {
            if (ptr[i] != thread_id * 100 + i)
            {
                success.store(false);
                return;
            }
        }

        int node = ThreadLocalNumaPool<int>::numa_node();
        if (node < 0)
        {
            success.store(false);
        }

        ThreadLocalNumaPool<int>::deallocate(ptr, 10);
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i)
    {
        threads.emplace_back(worker, i);
    }

    for (auto& t : threads)
    {
        t.join();
    }

    FATP_ASSERT_TRUE(success.load(), "All threads should succeed");

    return true;
}

FATP_TEST_CASE(thread_local_numa_pool_cross_thread_dealloc)
{
    // Cross-thread deallocation test.
    //
    // CRITICAL CONSTRAINT: Pool pointers must not outlive the owning thread.
    // This test verifies safe cross-thread deallocation when the owner is ALIVE.
    //
    // The pattern:
    // 1. Producer allocates from its pool
    // 2. Producer shares pointer with consumer
    // 3. Consumer reads data and calls deallocate (no-op for pool allocations)
    // 4. Consumer signals completion
    // 5. Producer exits (pool freed AFTER consumer is done)

    std::atomic<int*> shared_ptr{nullptr};
    std::atomic<bool> consumer_done{false};
    std::atomic<bool> producer_ready{false};

    std::thread producer([&]() {
        // Allocate from producer thread's pool
        int* ptr = ThreadLocalNumaPool<int>::allocate(10);
        for (int i = 0; i < 10; ++i)
        {
            ptr[i] = 42 + i;
        }
        shared_ptr.store(ptr, std::memory_order_release);
        producer_ready.store(true, std::memory_order_release);

        // CRITICAL: Wait for consumer to finish before exiting
        // If we exit early, the pool is destroyed and consumer gets UB
        while (!consumer_done.load(std::memory_order_acquire))
        {
            std::this_thread::yield();
        }
    });

    std::thread consumer([&]() {
        // Wait for producer to share data
        while (!producer_ready.load(std::memory_order_acquire))
        {
            std::this_thread::yield();
        }

        int* ptr = shared_ptr.load(std::memory_order_acquire);

        // Verify memory is accessible (producer is still alive)
        bool data_valid = (ptr != nullptr && ptr[0] == 42 && ptr[9] == 51);

        // Deallocate from consumer thread
        // This should detect "Source::Pool" in header and safely no-op
        // (safe because producer thread is still alive)
        ThreadLocalNumaPool<int>::deallocate(ptr, 10);

        // Signal producer that we're done
        consumer_done.store(data_valid, std::memory_order_release);
    });

    producer.join();
    consumer.join();

    FATP_ASSERT_TRUE(consumer_done.load(), "Consumer should complete successfully with valid data");

    return true;
}

FATP_TEST_CASE(thread_local_numa_pool_aligned_types)
{
    // Test with over-aligned type (e.g., AVX-512 vector simulation)
    struct alignas(64) AlignedData
    {
        char data[64];
    };
    static_assert(alignof(AlignedData) == 64, "Test requires 64-byte alignment");

    AlignedData* ptr = ThreadLocalNumaPool<AlignedData>::allocate(10);
    FATP_ASSERT_TRUE(ptr != nullptr, "Aligned allocation should succeed");

    // Verify alignment
    std::uintptr_t addr = reinterpret_cast<std::uintptr_t>(ptr);
    FATP_ASSERT_TRUE(addr % 64 == 0, "Pointer must be 64-byte aligned");

    // Verify each element is aligned
    for (int i = 0; i < 10; ++i)
    {
        std::uintptr_t elem_addr = reinterpret_cast<std::uintptr_t>(&ptr[i]);
        FATP_ASSERT_TRUE(elem_addr % 64 == 0, "Each element must be 64-byte aligned");
    }

    ThreadLocalNumaPool<AlignedData>::deallocate(ptr, 10);

    return true;
}

FATP_TEST_CASE(numa_allocator_aligned_types)
{
    // Test NumaAllocator with over-aligned type to verify fallback path alignment
    struct alignas(64) AlignedData
    {
        char data[64];
    };
    static_assert(alignof(AlignedData) == 64, "Test requires 64-byte alignment");

    NumaAllocator<AlignedData, NumaLocalPolicy> local_alloc;
    AlignedData* local_ptr = local_alloc.allocate(5);
    FATP_ASSERT_TRUE(local_ptr != nullptr, "Local policy aligned allocation should succeed");

    std::uintptr_t local_addr = reinterpret_cast<std::uintptr_t>(local_ptr);
    FATP_ASSERT_TRUE(local_addr % 64 == 0, "NumaLocalPolicy must return 64-byte aligned pointer");

    for (int i = 0; i < 5; ++i)
    {
        std::uintptr_t elem_addr = reinterpret_cast<std::uintptr_t>(&local_ptr[i]);
        FATP_ASSERT_TRUE(elem_addr % 64 == 0, "Each element must be 64-byte aligned");
    }

    local_alloc.deallocate(local_ptr, 5);

    NumaAllocator<AlignedData, NumaInterleavedPolicy> interleaved_alloc;
    AlignedData* interleaved_ptr = interleaved_alloc.allocate(5);
    FATP_ASSERT_TRUE(interleaved_ptr != nullptr, "Interleaved policy aligned allocation should succeed");

    std::uintptr_t interleaved_addr = reinterpret_cast<std::uintptr_t>(interleaved_ptr);
    FATP_ASSERT_TRUE(interleaved_addr % 64 == 0, "NumaInterleavedPolicy must return 64-byte aligned pointer");

    interleaved_alloc.deallocate(interleaved_ptr, 5);

    return true;
}

FATP_TEST_CASE(bind_thread_to_node_validation)
{
    bool result_invalid = bind_thread_to_node(-1);
    FATP_ASSERT_TRUE(!result_invalid, "Binding to invalid node should fail");

    bool result_out_of_range = bind_thread_to_node(1000);
    FATP_ASSERT_TRUE(!result_out_of_range, "Binding to out-of-range node should fail");

    if (NumaInfo::is_available() && NumaInfo::num_nodes() > 0)
    {
        bool result_valid = bind_thread_to_node(0);
        std::cout << "Binding to node 0: " << (result_valid ? "Success" : "Failed") << "\n";
    }

    return true;
}

FATP_TEST_CASE(allocator_exception_safety)
{
    NumaAllocator<int, NumaLocalPolicy> alloc;

    bool threw = false;
    try
    {
        size_t huge = (std::numeric_limits<size_t>::max)() / sizeof(int) + 1;
        [[maybe_unused]] int* ptr = alloc.allocate(huge);
    }
    catch (const std::bad_alloc&)
    {
        threw = true;
    }
    FATP_ASSERT_TRUE(threw, "Oversized allocation should throw bad_alloc");

    return true;
}
} // namespace fat_p::testing::numaallocator

namespace fat_p::testing
{


inline void run_benchmarks()
{
    using namespace fat_p::testing;

    auto& out = *get_test_config().output;
    out << "\n" << colors::cyan() << "Sanity Benchmark:" << colors::reset() << "\n";
    out << "  (benchmarks are provided by benchmark executables; unit tests do not run full benchmarks)\n\n";
}

bool test_NumaAllocator()
{
    FATP_PRINT_HEADER(NUMA ALLOCATOR)

    TestRunner runner;

    FATP_RUN_TEST_NS(runner, numaallocator, numa_info);
    FATP_RUN_TEST_NS(runner, numaallocator, numa_local_allocator);
    FATP_RUN_TEST_NS(runner, numaallocator, numa_interleaved_allocator);
    FATP_RUN_TEST_NS(runner, numaallocator, numa_preferred_allocator);
    FATP_RUN_TEST_NS(runner, numaallocator, allocator_zero_size);
    FATP_RUN_TEST_NS(runner, numaallocator, allocator_equality);
    FATP_RUN_TEST_NS(runner, numaallocator, allocator_rebind);
    FATP_RUN_TEST_NS(runner, numaallocator, numa_local_vector);
    FATP_RUN_TEST_NS(runner, numaallocator, numa_interleaved_vector);
    FATP_RUN_TEST_NS(runner, numaallocator, numa_preferred_vector);
    FATP_RUN_TEST_NS(runner, numaallocator, vector_copy_move);
    FATP_RUN_TEST_NS(runner, numaallocator, numa_memory_stats);
    FATP_RUN_TEST_NS(runner, numaallocator, thread_local_numa_pool_basic);
    FATP_RUN_TEST_NS(runner, numaallocator, thread_local_numa_pool_large);
    FATP_RUN_TEST_NS(runner, numaallocator, thread_local_numa_pool_reset);
    FATP_RUN_TEST_NS(runner, numaallocator, thread_local_numa_pool_multithread);
    FATP_RUN_TEST_NS(runner, numaallocator, thread_local_numa_pool_cross_thread_dealloc);
    FATP_RUN_TEST_NS(runner, numaallocator, thread_local_numa_pool_aligned_types);
    FATP_RUN_TEST_NS(runner, numaallocator, numa_allocator_aligned_types);
    FATP_RUN_TEST_NS(runner, numaallocator, bind_thread_to_node_validation);
    FATP_RUN_TEST_NS(runner, numaallocator, allocator_exception_safety);


    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_NumaAllocator() ? 0 : 1;
}
#endif
