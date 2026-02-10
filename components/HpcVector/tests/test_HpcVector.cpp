/**
 * @file test_HpcVector.cpp
 * @brief Comprehensive tests for HpcVector and NumaAlignedAllocator
 *
 * Tests cover:
 * - Memory alignment verification
 * - NUMA availability detection
 * - Basic vector operations
 * - SIMD compatibility (assume_aligned)
 * - Copy/move semantics
 * - Exception safety
 * - Performance benchmarks vs std::vector
 */
/*
FATP_META:
  meta_version: 1
  component: HpcVector
  file_role: test
  path: components/HpcVector/tests/test_HpcVector.cpp
  layer: Testing
  namespace: fat_p::testing::hpcvector
  summary: "Unit tests for HpcVector."
  api_stability: in_work
  related:
    docs_search: "HpcVector"
    headers:
      - include/fat_p/HpcVector.h
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

#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include <utility>
#include <vector>

#include "FatPTest.h"
#include "HpcVector.h"

namespace fat_p::testing::hpcvector
{

// =============================================================================
// Test Helpers
// =============================================================================

template <typename T>
bool isAligned(const T* ptr, std::size_t alignment) noexcept
{
    return ptr == nullptr || (reinterpret_cast<std::uintptr_t>(ptr) % alignment) == 0;
}

// =============================================================================
// NumaAlignedAllocator Tests
// =============================================================================

FATP_TEST_CASE(allocator_basic)
{
    std::cout << colors::cyan() << "\n[HpcVector] NumaAlignedAllocator basic test..." << colors::reset() << std::endl;

    memory::NumaLocalAllocator<int, 64> alloc;

    // Allocate
    int* ptr = alloc.allocate(16);
    FATP_ASSERT_TRUE(ptr != nullptr, "Allocation should succeed");
    FATP_ASSERT_TRUE(isAligned(ptr, 64), "Pointer should be 64-byte aligned");

    // Use memory
    for (int i = 0; i < 16; ++i)
    {
        ptr[i] = i * 10;
    }

    for (int i = 0; i < 16; ++i)
    {
        FATP_ASSERT_EQ(ptr[i], i * 10, "Value should match");
    }

    // Deallocate
    alloc.deallocate(ptr, 16);

    std::cout << colors::green() << "  NumaAlignedAllocator basic: PASSED" << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(allocator_numa_info)
{
    std::cout << colors::cyan() << "\n[HpcVector] NUMA info query..." << colors::reset() << std::endl;

    bool numa_available = memory::NumaInfo::is_available();
    int num_nodes = memory::NumaInfo::num_nodes();
    int current_node = memory::NumaInfo::current_node();

    std::cout << "  NUMA available: " << (numa_available ? "yes" : "no") << "\n";
    std::cout << "  Number of nodes: " << num_nodes << "\n";
    std::cout << "  Current node: " << current_node << "\n";

    // Sanity checks
    FATP_ASSERT_TRUE(num_nodes >= 1, "Should have at least 1 node");
    FATP_ASSERT_TRUE(current_node >= 0 && current_node < num_nodes, "Current node should be valid");

    std::cout << colors::green() << "  NUMA info: PASSED" << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(allocator_policies)
{
    std::cout << colors::cyan() << "\n[HpcVector] Allocator policies..." << colors::reset() << std::endl;

    // Local policy (default)
    {
        memory::NumaLocalAllocator<double, 64> alloc;
        double* ptr = alloc.allocate(8);
        FATP_ASSERT_TRUE(isAligned(ptr, 64), "Local alloc should be aligned");
        alloc.deallocate(ptr, 8);
    }

    // Preferred policy (specific node)
    {
        memory::NumaPreferredAllocator<double, 64> alloc(memory::NumaPreferredPolicy{0});
        double* ptr = alloc.allocate(8);
        FATP_ASSERT_TRUE(isAligned(ptr, 64), "Preferred alloc should be aligned");
        alloc.deallocate(ptr, 8);
    }

    // Interleaved policy
    {
        memory::NumaInterleavedAllocator<double, 64> alloc;
        double* ptr = alloc.allocate(8);
        FATP_ASSERT_TRUE(isAligned(ptr, 64), "Interleaved alloc should be aligned");
        alloc.deallocate(ptr, 8);
    }

    std::cout << colors::green() << "  Allocator policies: PASSED" << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// HpcVector Basic Tests
// =============================================================================

FATP_TEST_CASE(vector_construction)
{
    std::cout << colors::cyan() << "\n[HpcVector] Construction tests..." << colors::reset() << std::endl;

    // Default construction
    {
        HpcVector<int> v;
        FATP_ASSERT_EQ(v.size(), 0u, "Default should be empty");
        FATP_ASSERT_EQ(v.capacity(), 0u, "Default should have no capacity");
        FATP_ASSERT_TRUE(v.isAligned(), "Empty vector should be aligned");
    }

    // Size construction
    {
        HpcVector<int> v(100);
        FATP_ASSERT_EQ(v.size(), 100u, "Should have 100 elements");
        FATP_ASSERT_TRUE(v.isAligned(), "Should be aligned");
    }

    // Size + value construction
    {
        HpcVector<int> v(50, 42);
        FATP_ASSERT_EQ(v.size(), 50u, "Should have 50 elements");
        for (size_t i = 0; i < v.size(); ++i)
        {
            FATP_ASSERT_EQ(v[i], 42, "All elements should be 42");
        }
    }

    // Initializer list
    {
        HpcVector<int> v{1, 2, 3, 4, 5};
        FATP_ASSERT_EQ(v.size(), 5u, "Should have 5 elements");
        FATP_ASSERT_EQ(v[0], 1, "First element should be 1");
        FATP_ASSERT_EQ(v[4], 5, "Last element should be 5");
    }

    // Iterator construction
    {
        std::vector<int> src{10, 20, 30, 40};
        HpcVector<int> v(src.begin(), src.end());
        FATP_ASSERT_EQ(v.size(), src.size(), "Should match source size");
        for (size_t i = 0; i < v.size(); ++i)
        {
            FATP_ASSERT_EQ(v[i], src[i], "Elements should match");
        }
    }

    std::cout << colors::green() << "  Construction tests: PASSED" << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(vector_alignment)
{
    std::cout << colors::cyan() << "\n[HpcVector] Alignment verification..." << colors::reset() << std::endl;

    // Test various sizes
    for (size_t n : {size_t(1), size_t(7), size_t(16), size_t(63), size_t(64), size_t(65), size_t(100), size_t(1000), size_t(10000)})
    {
        HpcVector<float, 64> v(n);

        FATP_ASSERT_TRUE(v.isAligned(), "Vector should be aligned");

        std::uintptr_t addr = reinterpret_cast<std::uintptr_t>(v.data());
        FATP_ASSERT_TRUE(addr % 64 == 0, "Address should be 64-byte aligned");
    }

    // Test different alignments
    {
        HpcVector<double, 32> v32(100);
        FATP_ASSERT_TRUE(reinterpret_cast<std::uintptr_t>(v32.data()) % 32 == 0, "Should be 32-byte aligned");
    }

    std::cout << colors::green() << "  Alignment verification: PASSED" << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(vector_assume_aligned)
{
    std::cout << colors::cyan() << "\n[HpcVector] assume_aligned() test..." << colors::reset() << std::endl;

    HpcVector<float, 64> v(256, 1.0f);

    // Get aligned pointer
    float* ptr = v.assume_aligned();
    const float* cptr = std::as_const(v).assume_aligned();

    FATP_ASSERT_TRUE(ptr == v.data(), "assume_aligned should return data()");
    FATP_ASSERT_TRUE(cptr == v.data(), "const assume_aligned should return data()");

    // Use it in a loop (compiler should be able to optimize)
    float sum = 0.0f;
    for (size_t i = 0; i < v.size(); ++i)
    {
        sum += ptr[i];
    }

    FATP_ASSERT_TRUE(std::abs(sum - 256.0f) < 0.001f, "Sum should be 256");

    std::cout << colors::green() << "  assume_aligned(): PASSED" << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(vector_copy_move)
{
    std::cout << colors::cyan() << "\n[HpcVector] Copy/move semantics..." << colors::reset() << std::endl;

    // Copy construction
    {
        HpcVector<int> orig{1, 2, 3, 4, 5};
        HpcVector<int> copy(orig);

        FATP_ASSERT_EQ(copy.size(), orig.size(), "Copy should have same size");
        for (size_t i = 0; i < orig.size(); ++i)
        {
            FATP_ASSERT_EQ(copy[i], orig[i], "Elements should match");
        }
        FATP_ASSERT_TRUE(copy.data() != orig.data(), "Should be separate memory");
    }

    // Move construction
    {
        HpcVector<int> orig{10, 20, 30};
        int* orig_data = orig.data();
        HpcVector<int> moved(std::move(orig));

        FATP_ASSERT_EQ(moved.size(), 3u, "Moved should have elements");
        FATP_ASSERT_EQ(moved.data(), orig_data, "Should take ownership");
        FATP_ASSERT_EQ(orig.size(), 0u, "Original should be empty");
    }

    // Copy assignment
    {
        HpcVector<int> a{1, 2, 3};
        HpcVector<int> b{4, 5};
        b = a;

        FATP_ASSERT_EQ(b.size(), a.size(), "Should have same size");
        FATP_ASSERT_TRUE(b.data() != a.data(), "Should be separate memory");
    }

    // Move assignment
    {
        HpcVector<int> a{1, 2, 3};
        HpcVector<int> b{4, 5};
        int* a_data = a.data();
        b = std::move(a);

        FATP_ASSERT_EQ(b.data(), a_data, "Should take ownership");
    }

    std::cout << colors::green() << "  Copy/move semantics: PASSED" << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(vector_modifiers)
{
    std::cout << colors::cyan() << "\n[HpcVector] Modifiers..." << colors::reset() << std::endl;

    HpcVector<int> v;

    // push_back
    for (int i = 0; i < 100; ++i)
    {
        v.push_back(i);
    }
    FATP_ASSERT_EQ(v.size(), 100u, "Should have 100 elements");

    // Verify alignment maintained after growth
    FATP_ASSERT_TRUE(v.isAligned(), "Should remain aligned after push_back");

    // pop_back
    v.pop_back();
    FATP_ASSERT_EQ(v.size(), 99u, "Should have 99 elements");
    FATP_ASSERT_EQ(v.back(), 98, "Last element should be 98");

    // clear
    v.clear();
    FATP_ASSERT_EQ(v.size(), 0u, "Should be empty after clear");
    FATP_ASSERT_TRUE(v.capacity() > 0, "Capacity should remain");

    // resize
    v.resize(50, 7);
    FATP_ASSERT_EQ(v.size(), 50u, "Should have 50 elements");
    FATP_ASSERT_EQ(v[25], 7, "Elements should be 7");

    // emplace_back
    v.emplace_back(999);
    FATP_ASSERT_EQ(v.back(), 999, "Last element should be 999");

    std::cout << colors::green() << "  Modifiers: PASSED" << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(vector_iterators)
{
    std::cout << colors::cyan() << "\n[HpcVector] Iterators..." << colors::reset() << std::endl;

    HpcVector<int> v{1, 2, 3, 4, 5};

    // Range-based for
    int sum = 0;
    for (int x : v)
    {
        sum += x;
    }
    FATP_ASSERT_EQ(sum, 15, "Sum should be 15");

    // std::accumulate
    sum = 0;
    for (auto it = v.begin(); it != v.end(); ++it)
    {
        sum += *it;
    }
    FATP_ASSERT_EQ(sum, 15, "Iterator sum should be 15");

    // Reverse iteration
    std::vector<int> reversed;
    for (auto it = v.rbegin(); it != v.rend(); ++it)
    {
        reversed.push_back(*it);
    }
    FATP_ASSERT_EQ(reversed[0], 5, "First reversed should be 5");
    FATP_ASSERT_EQ(reversed[4], 1, "Last reversed should be 1");

    std::cout << colors::green() << "  Iterators: PASSED" << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// SIMD Integration Tests
// =============================================================================

FATP_TEST_CASE(vector_simd_compatibility)
{
    std::cout << colors::cyan() << "\n[HpcVector] SIMD compatibility..." << colors::reset() << std::endl;

    const size_t n = 1024;
    HpcVector<float, 64> a(n), b(n), c(n);

    // Initialize
    for (size_t i = 0; i < n; ++i)
    {
        a[i] = static_cast<float>(i);
        b[i] = static_cast<float>(i * 2);
    }

    // Simulate SIMD-style operation using aligned pointers
    float* pa = a.assume_aligned();
    float* pb = b.assume_aligned();
    float* pc = c.assume_aligned();

    for (size_t i = 0; i < n; ++i)
    {
        pc[i] = pa[i] + pb[i];
    }

    // Verify
    for (size_t i = 0; i < n; ++i)
    {
        float expected = static_cast<float>(i) + static_cast<float>(i * 2);
        FATP_ASSERT_TRUE(std::abs(c[i] - expected) < 0.001f, "SIMD result should match");
    }

    std::cout << colors::green() << "  SIMD compatibility: PASSED" << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// Performance Benchmarks
// =============================================================================
// =============================================================================
// Sanity Benchmark (kept fast; full benchmarks remain available via code inspection)
// =============================================================================
// =============================================================================
// Test Registration
// =============================================================================

void register_hpc_vector_tests(TestRunner& runner)
{
    auto& out = *get_test_config().output;

    out << "\n" << colors::blue() << "--- NumaAlignedAllocator Tests ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, hpcvector, allocator_basic);
    FATP_RUN_TEST_NS(runner, hpcvector, allocator_numa_info);
    FATP_RUN_TEST_NS(runner, hpcvector, allocator_policies);

    out << "\n" << colors::blue() << "--- HpcVector Basic Tests ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, hpcvector, vector_construction);
    FATP_RUN_TEST_NS(runner, hpcvector, vector_alignment);
    FATP_RUN_TEST_NS(runner, hpcvector, vector_assume_aligned);
    FATP_RUN_TEST_NS(runner, hpcvector, vector_copy_move);
    FATP_RUN_TEST_NS(runner, hpcvector, vector_modifiers);
    FATP_RUN_TEST_NS(runner, hpcvector, vector_iterators);

    out << "\n" << colors::blue() << "--- HpcVector SIMD Integration ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, hpcvector, vector_simd_compatibility);
}

// =============================================================================
// Main Test Entry Point
// =============================================================================

} // namespace fat_p::testing::hpcvector

namespace fat_p::testing
{


void run_benchmarks()
{
    using namespace fat_p::testing;

    auto& out = *get_test_config().output;
    out << "\n" << colors::cyan() << "Sanity Benchmark:" << colors::reset() << "\n";
    out << "  (benchmarks are provided by benchmark executables; unit tests do not run full benchmarks)\n\n";
}

bool test_HpcVector()
{
    FATP_PRINT_HEADER(HPC VECTOR)

    TestRunner runner;
    hpcvector::register_hpc_vector_tests(runner);

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Main (Standalone Test Application)
// =============================================================================

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_HpcVector() ? 0 : 1;
}
#endif // ENABLE_TEST_APPLICATION
