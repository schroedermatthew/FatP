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

#include <iostream>
#include <vector>
#include <limits>
#include <cmath>
#include <chrono>
#include <cstdint>
#include <random>
#include <utility>

#include "HpcVector.h"
#include "FatPTest.h"

#ifndef ENABLE_TEST_APPLICATION
#include "test_HpcVector.h"
#endif

namespace fat_p::testing {

// =============================================================================
// Test Helpers
// =============================================================================

template<typename T>
bool is_aligned(const T* ptr, std::size_t alignment) noexcept {
    return ptr == nullptr || 
           (reinterpret_cast<std::uintptr_t>(ptr) % alignment) == 0;
}

// =============================================================================
// NumaAlignedAllocator Tests
// =============================================================================

bool test_hpc_allocator_basic()
{
    std::cout << colors::cyan() << "\n[HpcVector] NumaAlignedAllocator basic test..."
              << colors::reset() << std::endl;

    memory::NumaLocalAllocator<int, 64> alloc;
    
    // Allocate
    int* ptr = alloc.allocate(16);
    SIMPLE_ASSERT(ptr != nullptr, "Allocation should succeed");
    SIMPLE_ASSERT(is_aligned(ptr, 64), "Pointer should be 64-byte aligned");
    
    // Use memory
    for (int i = 0; i < 16; ++i) {
        ptr[i] = i * 10;
    }
    
    for (int i = 0; i < 16; ++i) {
        ASSERT_EQ(ptr[i], i * 10, "Value should match");
    }
    
    // Deallocate
    alloc.deallocate(ptr, 16);

    std::cout << colors::green() << "  NumaAlignedAllocator basic: PASSED" 
              << colors::reset() << std::endl;
    return true;
}

bool test_hpc_allocator_numa_info()
{
    std::cout << colors::cyan() << "\n[HpcVector] NUMA info query..."
              << colors::reset() << std::endl;

    bool numa_available = memory::NumaInfo::is_available();
    int num_nodes = memory::NumaInfo::num_nodes();
    int current_node = memory::NumaInfo::current_node();

    std::cout << "  NUMA available: " << (numa_available ? "yes" : "no") << "\n";
    std::cout << "  Number of nodes: " << num_nodes << "\n";
    std::cout << "  Current node: " << current_node << "\n";

    // Sanity checks
    SIMPLE_ASSERT(num_nodes >= 1, "Should have at least 1 node");
    SIMPLE_ASSERT(current_node >= 0 && current_node < num_nodes, 
                  "Current node should be valid");

    std::cout << colors::green() << "  NUMA info: PASSED" 
              << colors::reset() << std::endl;
    return true;
}

bool test_hpc_allocator_policies()
{
    std::cout << colors::cyan() << "\n[HpcVector] Allocator policies..."
              << colors::reset() << std::endl;

    // Local policy (default)
    {
        memory::NumaLocalAllocator<double, 64> alloc;
        double* ptr = alloc.allocate(8);
        SIMPLE_ASSERT(is_aligned(ptr, 64), "Local alloc should be aligned");
        alloc.deallocate(ptr, 8);
    }

    // Preferred policy (specific node)
    {
        memory::NumaPreferredAllocator<double, 64> alloc(
            memory::NumaPreferredPolicy{0});
        double* ptr = alloc.allocate(8);
        SIMPLE_ASSERT(is_aligned(ptr, 64), "Preferred alloc should be aligned");
        alloc.deallocate(ptr, 8);
    }

    // Interleaved policy
    {
        memory::NumaInterleavedAllocator<double, 64> alloc;
        double* ptr = alloc.allocate(8);
        SIMPLE_ASSERT(is_aligned(ptr, 64), "Interleaved alloc should be aligned");
        alloc.deallocate(ptr, 8);
    }

    std::cout << colors::green() << "  Allocator policies: PASSED" 
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// HpcVector Basic Tests
// =============================================================================

bool test_hpc_vector_construction()
{
    std::cout << colors::cyan() << "\n[HpcVector] Construction tests..."
              << colors::reset() << std::endl;

    // Default construction
    {
        HpcVector<int> v;
        ASSERT_EQ(v.size(), 0u, "Default should be empty");
        ASSERT_EQ(v.capacity(), 0u, "Default should have no capacity");
        SIMPLE_ASSERT(v.is_aligned(), "Empty vector should be aligned");
    }

    // Size construction
    {
        HpcVector<int> v(100);
        ASSERT_EQ(v.size(), 100u, "Should have 100 elements");
        SIMPLE_ASSERT(v.is_aligned(), "Should be aligned");
    }

    // Size + value construction
    {
        HpcVector<int> v(50, 42);
        ASSERT_EQ(v.size(), 50u, "Should have 50 elements");
        for (size_t i = 0; i < v.size(); ++i) {
            ASSERT_EQ(v[i], 42, "All elements should be 42");
        }
    }

    // Initializer list
    {
        HpcVector<int> v{1, 2, 3, 4, 5};
        ASSERT_EQ(v.size(), 5u, "Should have 5 elements");
        ASSERT_EQ(v[0], 1, "First element should be 1");
        ASSERT_EQ(v[4], 5, "Last element should be 5");
    }

    // Iterator construction
    {
        std::vector<int> src{10, 20, 30, 40};
        HpcVector<int> v(src.begin(), src.end());
        ASSERT_EQ(v.size(), src.size(), "Should match source size");
        for (size_t i = 0; i < v.size(); ++i) {
            ASSERT_EQ(v[i], src[i], "Elements should match");
        }
    }

    std::cout << colors::green() << "  Construction tests: PASSED" 
              << colors::reset() << std::endl;
    return true;
}

bool test_hpc_vector_alignment()
{
    std::cout << colors::cyan() << "\n[HpcVector] Alignment verification..."
              << colors::reset() << std::endl;

    // Test various sizes
    for (size_t n : {1, 7, 16, 63, 64, 65, 100, 1000, 10000}) {
        HpcVector<float, 64> v(n);
        
        SIMPLE_ASSERT(v.is_aligned(), "Vector should be aligned");
        
        std::uintptr_t addr = reinterpret_cast<std::uintptr_t>(v.data());
        SIMPLE_ASSERT(addr % 64 == 0, "Address should be 64-byte aligned");
    }

    // Test different alignments
    {
        HpcVector<double, 32> v32(100);
        SIMPLE_ASSERT(reinterpret_cast<std::uintptr_t>(v32.data()) % 32 == 0,
                      "Should be 32-byte aligned");
    }

    std::cout << colors::green() << "  Alignment verification: PASSED" 
              << colors::reset() << std::endl;
    return true;
}

bool test_hpc_vector_assume_aligned()
{
    std::cout << colors::cyan() << "\n[HpcVector] assume_aligned() test..."
              << colors::reset() << std::endl;

    HpcVector<float, 64> v(256, 1.0f);
    
    // Get aligned pointer
    float* ptr = v.assume_aligned();
    const float* cptr = std::as_const(v).assume_aligned();
    
    SIMPLE_ASSERT(ptr == v.data(), "assume_aligned should return data()");
    SIMPLE_ASSERT(cptr == v.data(), "const assume_aligned should return data()");
    
    // Use it in a loop (compiler should be able to optimize)
    float sum = 0.0f;
    for (size_t i = 0; i < v.size(); ++i) {
        sum += ptr[i];
    }
    
    SIMPLE_ASSERT(std::abs(sum - 256.0f) < 0.001f, "Sum should be 256");

    std::cout << colors::green() << "  assume_aligned(): PASSED" 
              << colors::reset() << std::endl;
    return true;
}

bool test_hpc_vector_copy_move()
{
    std::cout << colors::cyan() << "\n[HpcVector] Copy/move semantics..."
              << colors::reset() << std::endl;

    // Copy construction
    {
        HpcVector<int> orig{1, 2, 3, 4, 5};
        HpcVector<int> copy(orig);
        
        ASSERT_EQ(copy.size(), orig.size(), "Copy should have same size");
        for (size_t i = 0; i < orig.size(); ++i) {
            ASSERT_EQ(copy[i], orig[i], "Elements should match");
        }
        SIMPLE_ASSERT(copy.data() != orig.data(), "Should be separate memory");
    }

    // Move construction
    {
        HpcVector<int> orig{10, 20, 30};
        int* orig_data = orig.data();
        HpcVector<int> moved(std::move(orig));
        
        ASSERT_EQ(moved.size(), 3u, "Moved should have elements");
        ASSERT_EQ(moved.data(), orig_data, "Should take ownership");
        ASSERT_EQ(orig.size(), 0u, "Original should be empty");
    }

    // Copy assignment
    {
        HpcVector<int> a{1, 2, 3};
        HpcVector<int> b{4, 5};
        b = a;
        
        ASSERT_EQ(b.size(), a.size(), "Should have same size");
        SIMPLE_ASSERT(b.data() != a.data(), "Should be separate memory");
    }

    // Move assignment
    {
        HpcVector<int> a{1, 2, 3};
        HpcVector<int> b{4, 5};
        int* a_data = a.data();
        b = std::move(a);
        
        ASSERT_EQ(b.data(), a_data, "Should take ownership");
    }

    std::cout << colors::green() << "  Copy/move semantics: PASSED" 
              << colors::reset() << std::endl;
    return true;
}

bool test_hpc_vector_modifiers()
{
    std::cout << colors::cyan() << "\n[HpcVector] Modifiers..."
              << colors::reset() << std::endl;

    HpcVector<int> v;

    // push_back
    for (int i = 0; i < 100; ++i) {
        v.push_back(i);
    }
    ASSERT_EQ(v.size(), 100u, "Should have 100 elements");
    
    // Verify alignment maintained after growth
    SIMPLE_ASSERT(v.is_aligned(), "Should remain aligned after push_back");

    // pop_back
    v.pop_back();
    ASSERT_EQ(v.size(), 99u, "Should have 99 elements");
    ASSERT_EQ(v.back(), 98, "Last element should be 98");

    // clear
    v.clear();
    ASSERT_EQ(v.size(), 0u, "Should be empty after clear");
    SIMPLE_ASSERT(v.capacity() > 0, "Capacity should remain");

    // resize
    v.resize(50, 7);
    ASSERT_EQ(v.size(), 50u, "Should have 50 elements");
    ASSERT_EQ(v[25], 7, "Elements should be 7");

    // emplace_back
    v.emplace_back(999);
    ASSERT_EQ(v.back(), 999, "Last element should be 999");

    std::cout << colors::green() << "  Modifiers: PASSED" 
              << colors::reset() << std::endl;
    return true;
}

bool test_hpc_vector_iterators()
{
    std::cout << colors::cyan() << "\n[HpcVector] Iterators..."
              << colors::reset() << std::endl;

    HpcVector<int> v{1, 2, 3, 4, 5};

    // Range-based for
    int sum = 0;
    for (int x : v) {
        sum += x;
    }
    ASSERT_EQ(sum, 15, "Sum should be 15");

    // std::accumulate
    sum = 0;
    for (auto it = v.begin(); it != v.end(); ++it) {
        sum += *it;
    }
    ASSERT_EQ(sum, 15, "Iterator sum should be 15");

    // Reverse iteration
    std::vector<int> reversed;
    for (auto it = v.rbegin(); it != v.rend(); ++it) {
        reversed.push_back(*it);
    }
    ASSERT_EQ(reversed[0], 5, "First reversed should be 5");
    ASSERT_EQ(reversed[4], 1, "Last reversed should be 1");

    std::cout << colors::green() << "  Iterators: PASSED" 
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// SIMD Integration Tests
// =============================================================================

bool test_hpc_vector_simd_compatibility()
{
    std::cout << colors::cyan() << "\n[HpcVector] SIMD compatibility..."
              << colors::reset() << std::endl;

    const size_t n = 1024;
    HpcVector<float, 64> a(n), b(n), c(n);

    // Initialize
    for (size_t i = 0; i < n; ++i) {
        a[i] = static_cast<float>(i);
        b[i] = static_cast<float>(i * 2);
    }

    // Simulate SIMD-style operation using aligned pointers
    float* pa = a.assume_aligned();
    float* pb = b.assume_aligned();
    float* pc = c.assume_aligned();

    for (size_t i = 0; i < n; ++i) {
        pc[i] = pa[i] + pb[i];
    }

    // Verify
    for (size_t i = 0; i < n; ++i) {
        float expected = static_cast<float>(i) + static_cast<float>(i * 2);
        SIMPLE_ASSERT(std::abs(c[i] - expected) < 0.001f, 
                      "SIMD result should match");
    }

    std::cout << colors::green() << "  SIMD compatibility: PASSED" 
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// Performance Benchmarks
// =============================================================================

void run_hpc_vector_benchmarks()
{
    auto& out = *get_test_config().output;
    out << "\n" << colors::cyan() << colors::bold()
        << "=== HpcVector vs std::vector Performance ===" 
        << colors::reset() << "\n\n";

    const size_t N = 1000000;

    out << colors::blue() << "--- Allocation (1M elements) ---" 
        << colors::reset() << "\n";

    // std::vector allocation
    benchmark("std::vector<float> alloc", [&]() {
        std::vector<float> v(N);
        DoNotOptimize(v.data());
    }, 1000);

    // HpcVector allocation
    benchmark("HpcVector<float> alloc", [&]() {
        HpcVector<float> v(N);
        DoNotOptimize(v.data());
    }, 1000);

    out << "\n" << colors::blue() << "--- Sequential Write (1M elements) ---"
        << colors::reset() << "\n";

    std::vector<float> std_v(N);
    HpcVector<float> hpc_v(N);

    benchmark("std::vector write", [&]() {
        for (size_t i = 0; i < N; ++i) {
            std_v[i] = static_cast<float>(i);
        }
        DoNotOptimize(std_v.data());
    }, 100);

    benchmark("HpcVector write", [&]() {
        float* ptr = hpc_v.assume_aligned();
        for (size_t i = 0; i < N; ++i) {
            ptr[i] = static_cast<float>(i);
        }
        DoNotOptimize(hpc_v.data());
    }, 100);

    out << "\n" << colors::blue() << "--- Sum Reduction (1M elements) ---"
        << colors::reset() << "\n";

    // Initialize
    for (size_t i = 0; i < N; ++i) {
        std_v[i] = 1.0f;
        hpc_v[i] = 1.0f;
    }

    volatile float result = 0;

    benchmark("std::vector sum", [&]() {
        float sum = 0;
        for (size_t i = 0; i < N; ++i) {
            sum += std_v[i];
        }
        result = sum;
    }, 100);

    benchmark("HpcVector sum (aligned)", [&]() {
        float sum = 0;
        const float* ptr = hpc_v.assume_aligned();
        for (size_t i = 0; i < N; ++i) {
            sum += ptr[i];
        }
        result = sum;
    }, 100);

    out << "\n" << colors::blue() << "--- Info ---" << colors::reset() << "\n";
    out << "  HpcVector alignment: " << HpcVector<float>::alignment << " bytes\n";
    out << "  NUMA available: " 
        << (memory::NumaInfo::is_available() ? "yes" : "no") << "\n";
    out << "  NUMA nodes: " << memory::NumaInfo::num_nodes() << "\n";

    out << "\n";
}

// =============================================================================
// Test Registration
// =============================================================================

void register_hpc_vector_tests(TestRunner& runner)
{
    auto& out = *get_test_config().output;
    
    out << "\n" << colors::blue() << "--- NumaAlignedAllocator Tests ---" 
        << colors::reset() << "\n";
    RUN_TEST(runner, hpc_allocator_basic);
    RUN_TEST(runner, hpc_allocator_numa_info);
    RUN_TEST(runner, hpc_allocator_policies);
    
    out << "\n" << colors::blue() << "--- HpcVector Basic Tests ---" 
        << colors::reset() << "\n";
    RUN_TEST(runner, hpc_vector_construction);
    RUN_TEST(runner, hpc_vector_alignment);
    RUN_TEST(runner, hpc_vector_assume_aligned);
    RUN_TEST(runner, hpc_vector_copy_move);
    RUN_TEST(runner, hpc_vector_modifiers);
    RUN_TEST(runner, hpc_vector_iterators);
    
    out << "\n" << colors::blue() << "--- HpcVector SIMD Integration ---" 
        << colors::reset() << "\n";
    RUN_TEST(runner, hpc_vector_simd_compatibility);
}

// =============================================================================
// Main Test Entry Point
// =============================================================================

bool test_HpcVector()
{
    PRINT_HEADER(HPC VECTOR)

    TestRunner runner;
    register_hpc_vector_tests(runner);
    run_hpc_vector_benchmarks();

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Main (Standalone Test Application)
// =============================================================================

#ifdef ENABLE_TEST_APPLICATION

int main()
{
    return fat_p::testing::test_HpcVector() ? 0 : 1;
}

#endif // ENABLE_TEST_APPLICATION
