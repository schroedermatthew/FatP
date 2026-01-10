/**
 * @file test_CacheUtilities.cpp
 * @brief Comprehensive unit tests for CacheUtilities.h
 */
/*
FATP_META:
  meta_version: 1
  component: CacheUtilities
  file_role: test
  path: tests/test_CacheUtilities.cpp
  namespace: fat_p::testing::cacheutilities::testing::cacheutilities
  summary: "Unit tests for CacheUtilities."
  related:
    docs_search: "CacheUtilities"
    headers:
      - fat_p/CacheUtilities.h
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
#include <vector>
#include <numeric>
#include <cstring>
#include <atomic>
#include <array>
#include <memory>

#include "CacheUtilities.h"
#include "FatPTest.h"

namespace fat_p::testing::cacheutilities
{

using namespace perf;

// =============================================================================
// CacheInfo Tests
// =============================================================================

FATP_TEST_CASE(cache_info) {
    std::cout << "  Cache line sizes:\n";
    std::cout << "    L1 line size: " << CacheInfo::l1_line_size() << " bytes\n";
    std::cout << "    Destructive interference: " << CacheInfo::destructive_interference_size() << " bytes\n";
    std::cout << "    Constructive interference: " << CacheInfo::constructive_interference_size() << " bytes\n";
    
    // Sanity checks
    FATP_ASSERT_TRUE(CacheInfo::l1_line_size() >= 32, "Cache line should be at least 32 bytes");
    FATP_ASSERT_TRUE(CacheInfo::l1_line_size() <= 256, "Cache line should be at most 256 bytes");
    
    // Power of 2 check
    constexpr size_t line_size = CacheInfo::l1_line_size();
    FATP_ASSERT_TRUE((line_size & (line_size - 1)) == 0, "Cache line size should be power of 2");
    
    // Destructive >= constructive (typically equal)
    FATP_ASSERT_TRUE(CacheInfo::destructive_interference_size() >= CacheInfo::constructive_interference_size(),
                  "Destructive interference should be >= constructive");
    
    // Verify constexpr values match function returns
    FATP_ASSERT_TRUE(cache_constants::l1_line_size_v == CacheInfo::l1_line_size(),
                  "Constexpr value should match function");
    FATP_ASSERT_TRUE(cache_constants::destructive_interference_size_v == CacheInfo::destructive_interference_size(),
                  "Constexpr value should match function");
    
    return true;
}

// =============================================================================
// Prefetch Tests
// =============================================================================

FATP_TEST_CASE(prefetch_operations) {
    alignas(64) int data[1024];
    for (int i = 0; i < 1024; ++i) {
        data[i] = i;
    }
    
    // Test various prefetch operations (these should not crash)
    prefetch<PrefetchLocality::High>(data);
    prefetch<PrefetchLocality::Moderate>(data + 64);
    prefetch<PrefetchLocality::Low>(data + 128);
    prefetch<PrefetchLocality::None>(data + 192);
    
    prefetch<PrefetchLocality::High, PrefetchOp::Write>(data + 256);
    
    // Prefetch range
    prefetch_range<PrefetchLocality::High>(data, sizeof(data));
    
    // Use the data to ensure prefetch did something
    long long sum = 0;
    for (int i = 0; i < 1024; ++i) {
        sum += data[i];
    }
    
    long long expected = static_cast<long long>(1023) * 1024 / 2;
    FATP_ASSERT_TRUE(sum == expected, "Data should be accessible after prefetch");
    
    return true;
}

FATP_TEST_CASE(prefetch_range_zero_size) {
    alignas(64) int data[64];
    
    // Zero-size prefetch should not crash or do anything
    prefetch_range(data, 0);
    prefetch_range(nullptr, 0);  // Even nullptr with size 0 should be safe
    
    return true;
}

FATP_TEST_CASE(prefetch_ahead) {
    constexpr size_t N = 1000;
    alignas(64) float data[N];
    for (size_t i = 0; i < N; ++i) {
        data[i] = static_cast<float>(i);
    }
    
    float sum = 0.0f;
    for (size_t i = 0; i < N - 8; ++i) {
        prefetch_ahead<float>(data, i);
        sum += data[i];
    }
    
    FATP_ASSERT_TRUE(sum > 0.0f, "Prefetch ahead should work");
    
    return true;
}

FATP_TEST_CASE(prefetch_ahead_overflow) {
    alignas(64) int data[64];
    
    // Test with values that would overflow if not checked
    // SIZE_MAX + 8 would wrap around to a small value
    size_t large_index = SIZE_MAX - 4;
    
    // This should NOT crash - the overflow check should prevent bad prefetch
    prefetch_ahead<int>(data, large_index, 1, 8);
    
    // Also test stride overflow
    prefetch_ahead<int>(data, 10, SIZE_MAX / 2, 4);
    
    return true;
}

// =============================================================================
// Cache Flush Tests
// =============================================================================

FATP_TEST_CASE(cache_flush) {
    alignas(64) int data[64];
    for (int i = 0; i < 64; ++i) {
        data[i] = i;
    }
    
    // Flush single cache line
    flush_cache_line(data);
    
    // Flush range
    flush_cache_range(data, sizeof(data));
    
    // Flush and invalidate
    flush_invalidate(data);
    
    // Verify data is still accessible (flush doesn't destroy data)
    FATP_ASSERT_TRUE(data[0] == 0, "Data should still be correct after flush");
    FATP_ASSERT_TRUE(data[63] == 63, "Last element should be correct after flush");
    
    return true;
}

FATP_TEST_CASE(flush_cache_range_zero_size) {
    alignas(64) int data[64];
    
    // Zero-size flush should not crash
    flush_cache_range(data, 0);
    flush_cache_range(nullptr, 0);
    
    return true;
}

// =============================================================================
// Stream Store Tests
// =============================================================================

FATP_TEST_CASE(stream_store) {
    alignas(64) int data[64];
    
    for (int i = 0; i < 64; ++i) {
        stream_store(&data[i], i);
    }
    
    store_fence(); // Ensure stores complete
    
    for (int i = 0; i < 64; ++i) {
        FATP_ASSERT_TRUE(data[i] == i, "Stream store should write correct values");
    }
    
    return true;
}

FATP_TEST_CASE(stream_store_8byte) {
    alignas(64) double data[32];
    
    for (int i = 0; i < 32; ++i) {
        double val = static_cast<double>(i) * 1.5;
        stream_store(&data[i], val);
    }
    
    store_fence();
    
    for (int i = 0; i < 32; ++i) {
        double expected = static_cast<double>(i) * 1.5;
        FATP_ASSERT_TRUE(data[i] == expected, "Stream store should work for 8-byte types");
    }
    
    return true;
}

FATP_TEST_CASE(stream_store_unaligned) {
    // Test that unaligned stream_store falls back gracefully (no crash)
    alignas(64) char buffer[128];
    std::memset(buffer, 0, sizeof(buffer));
    
    // Create unaligned int pointer
    int* unaligned_int = reinterpret_cast<int*>(buffer + 1);
    stream_store(unaligned_int, 42);
    store_fence();
    
    // Read back using memcpy to avoid UB on architectures requiring alignment
    int loaded_int = 0;
    std::memcpy(&loaded_int, unaligned_int, sizeof(loaded_int));
    FATP_ASSERT_TRUE(loaded_int == 42, "Unaligned stream_store should work via fallback");
    
    // Create unaligned double pointer
    double* unaligned_double = reinterpret_cast<double*>(buffer + 3);
    stream_store(unaligned_double, 3.14);
    store_fence();
    
    // Read back using memcpy
    double loaded_double = 0.0;
    std::memcpy(&loaded_double, unaligned_double, sizeof(loaded_double));
    FATP_ASSERT_TRUE(loaded_double == 3.14, "Unaligned double stream_store should work");
    
    return true;
}

// =============================================================================
// Stream Copy Tests
// =============================================================================

FATP_TEST_CASE(stream_copy) {
    alignas(64) char src[1024];
    alignas(64) char dest[1024];
    
    for (int i = 0; i < 1024; ++i) {
        src[i] = static_cast<char>(i % 256);
    }
    
    stream_copy(dest, src, 1024);
    
    for (int i = 0; i < 1024; ++i) {
        FATP_ASSERT_TRUE(dest[i] == src[i], "Stream copy should copy correctly");
    }
    
    return true;
}

FATP_TEST_CASE(stream_copy_unaligned) {
    alignas(64) char src[1024];
    alignas(64) char dest[1024];
    
    for (int i = 0; i < 1024; ++i) {
        src[i] = static_cast<char>(i % 256);
    }
    
    // Unaligned destination should fall back to memcpy (no crash)
    stream_copy(dest + 1, src + 1, 1022);
    
    for (int i = 1; i < 1023; ++i) {
        FATP_ASSERT_TRUE(dest[i] == src[i], "Unaligned stream copy should work via fallback");
    }
    
    return true;
}

FATP_TEST_CASE(stream_copy_zero_size) {
    alignas(64) char src[64];
    alignas(64) char dest[64];
    
    // Zero-size copy should not crash
    stream_copy(dest, src, 0);
    
    return true;
}

// =============================================================================
// CacheAligned Tests
// =============================================================================

FATP_TEST_CASE(cache_aligned) {
    CacheAligned<int> val1(42);
    FATP_ASSERT_TRUE(val1.get() == 42, "CacheAligned should store value");
    
    CacheAligned<double> val2;
    val2.get() = 3.14;
    FATP_ASSERT_TRUE(val2.get() == 3.14, "CacheAligned should be modifiable");
    
    // Check alignment
    FATP_ASSERT_TRUE(reinterpret_cast<uintptr_t>(&val1) % CacheInfo::destructive_interference_size() == 0,
                  "CacheAligned should be properly aligned");
    
    return true;
}

FATP_TEST_CASE(cache_aligned_array) {
    // Array of CacheAligned should have each element on separate cache lines
    CacheAligned<int> arr[4];
    
    for (int i = 0; i < 4; ++i) {
        arr[i].get() = i * 10;
    }
    
    // Verify values
    for (int i = 0; i < 4; ++i) {
        FATP_ASSERT_TRUE(arr[i].get() == i * 10, "Array element should have correct value");
    }
    
    // Verify each element is on its own cache line
    uintptr_t addr0 = reinterpret_cast<uintptr_t>(&arr[0]);
    uintptr_t addr1 = reinterpret_cast<uintptr_t>(&arr[1]);
    
    size_t spacing = addr1 - addr0;
    FATP_ASSERT_TRUE(spacing >= CacheInfo::destructive_interference_size(),
                  "CacheAligned array elements should be spaced by at least cache line size");
    
    return true;
}

// =============================================================================
// CacheLinePadded Tests
// =============================================================================

FATP_TEST_CASE(cache_line_padded) {
    CacheLinePadded<int> val1(100);
    FATP_ASSERT_TRUE(val1.get() == 100, "CacheLinePadded should store value");
    
    // Size should be at least cache line size
    FATP_ASSERT_TRUE(sizeof(CacheLinePadded<int>) >= CacheInfo::destructive_interference_size(),
                  "CacheLinePadded should be at least cache line size");
    
    // Size should be exactly cache line size (for types smaller than cache line)
    FATP_ASSERT_TRUE(sizeof(CacheLinePadded<int>) == CacheInfo::destructive_interference_size(),
                  "CacheLinePadded<int> should be exactly cache line size");
    
    return true;
}

FATP_TEST_CASE(cache_line_padded_alignment) {
    // Array of 2 elements - key test for the alignas fix
    CacheLinePadded<int> arr[2];
    arr[0].get() = 111;
    arr[1].get() = 222;
    
    uintptr_t addr0 = reinterpret_cast<uintptr_t>(&arr[0]);
    uintptr_t addr1 = reinterpret_cast<uintptr_t>(&arr[1]);
    
    // Verify base alignment
    FATP_ASSERT_TRUE(addr0 % CacheInfo::destructive_interference_size() == 0,
                  "CacheLinePadded should be cache-line aligned");
    
    // Verify spacing is exactly cache line size
    size_t spacing = addr1 - addr0;
    FATP_ASSERT_TRUE(spacing == CacheInfo::destructive_interference_size(),
                  "CacheLinePadded array elements should be spaced by exactly cache line size");
    
    // Verify values are intact
    FATP_ASSERT_TRUE(arr[0].get() == 111, "First element should be correct");
    FATP_ASSERT_TRUE(arr[1].get() == 222, "Second element should be correct");
    
    return true;
}

FATP_TEST_CASE(cache_line_padded_various_sizes) {
    // Test with various small types
    CacheLinePadded<char> c(42);
    FATP_ASSERT_TRUE(sizeof(CacheLinePadded<char>) == CacheInfo::destructive_interference_size(),
                  "CacheLinePadded<char> should be cache line size");
    FATP_ASSERT_TRUE(c.get() == 42, "char value should be correct");
    
    CacheLinePadded<short> s(1000);
    FATP_ASSERT_TRUE(sizeof(CacheLinePadded<short>) == CacheInfo::destructive_interference_size(),
                  "CacheLinePadded<short> should be cache line size");
    FATP_ASSERT_TRUE(s.get() == 1000, "short value should be correct");
    
    CacheLinePadded<double> d(3.14159);
    FATP_ASSERT_TRUE(sizeof(CacheLinePadded<double>) == CacheInfo::destructive_interference_size(),
                  "CacheLinePadded<double> should be cache line size");
    FATP_ASSERT_TRUE(d.get() == 3.14159, "double value should be correct");
    
    return true;
}

FATP_TEST_CASE(cache_line_padded_exact_size) {
    // Test with a type that is exactly cache line size (no padding needed)
    struct ExactSize {
        char data[CacheInfo::destructive_interference_size()];
    };
    
    CacheLinePadded<ExactSize> exact;
    (void)exact;  // Suppress unused variable warning
    
    FATP_ASSERT_TRUE(sizeof(CacheLinePadded<ExactSize>) == CacheInfo::destructive_interference_size(),
                  "CacheLinePadded of exact-size type should work");
    
    return true;
}

// =============================================================================
// Memory Barrier Tests
// =============================================================================

FATP_TEST_CASE(memory_barriers) {
    alignas(64) int data[2] = {0, 0};
    
    data[0] = 1;
    memory_barrier();
    data[1] = 2;
    
    FATP_ASSERT_TRUE(data[0] == 1 && data[1] == 2, "Memory barrier should not corrupt data");
    
    data[0] = 3;
    store_fence();
    
    load_fence();
    int val = data[0];
    
    FATP_ASSERT_TRUE(val == 3, "Fences should not corrupt data");
    
    return true;
}

// =============================================================================
// Blocking Utilities Tests
// =============================================================================

FATP_TEST_CASE(optimal_block_size) {
    size_t block_size = optimal_block_size(10000, sizeof(double), 32768);
    
    FATP_ASSERT_TRUE(block_size > 0, "Block size should be positive");
    FATP_ASSERT_TRUE(block_size * sizeof(double) <= 32768, "Block should fit in cache");
    
    // Power of 2 check
    FATP_ASSERT_TRUE((block_size & (block_size - 1)) == 0, "Block size should be power of 2");
    
    return true;
}

FATP_TEST_CASE(optimal_block_size_edge_cases) {
    // Zero elements
    size_t block = optimal_block_size(0, sizeof(int), 32768);
    FATP_ASSERT_TRUE(block >= 1, "Block size for 0 elements should be at least 1");
    
    // Zero element size
    block = optimal_block_size(1000, 0, 32768);
    FATP_ASSERT_TRUE(block >= 1, "Block size for 0-byte elements should be at least 1");
    
    // Huge elements that don't fit in cache
    block = optimal_block_size(1000, 1024 * 1024, 32768);
    FATP_ASSERT_TRUE(block >= 1, "Block size should be at least 1 even for huge elements");
    
    return true;
}

FATP_TEST_CASE(block_iterator_2d) {
    BlockIterator2D iter(100, 100, 16, 16);
    
    size_t block_count = 0;
    while (iter.has_next()) {
        size_t si = iter.block_start_i();
        size_t sj = iter.block_start_j();
        size_t ei = iter.block_end_i();
        size_t ej = iter.block_end_j();
        
        FATP_ASSERT_TRUE(ei > si, "Block should have positive height");
        FATP_ASSERT_TRUE(ej > sj, "Block should have positive width");
        FATP_ASSERT_TRUE(ei <= 100, "Block end_i should not exceed rows");
        FATP_ASSERT_TRUE(ej <= 100, "Block end_j should not exceed cols");
        
        iter.next_block();
        block_count++;
    }
    
    // 100x100 divided into 16x16 blocks = ceil(100/16) * ceil(100/16) = 7 * 7 = 49 blocks
    FATP_ASSERT_TRUE(block_count == 49, "Should have exactly 49 blocks for 100x100/16x16");
    
    return true;
}

FATP_TEST_CASE(block_iterator_2d_non_divisible) {
    // Test with sizes that don't divide evenly
    BlockIterator2D iter(50, 70, 16, 16);
    
    size_t block_count = 0;
    while (iter.has_next()) {
        iter.next_block();
        block_count++;
    }
    
    // ceil(50/16) * ceil(70/16) = 4 * 5 = 20 blocks
    FATP_ASSERT_TRUE(block_count == 20, "Should have exactly 20 blocks for 50x70/16x16");
    
    return true;
}

FATP_TEST_CASE(block_iterator_2d_zero_blocks) {
    // Zero block sizes should be clamped to 1 (prevent infinite loop)
    BlockIterator2D iter_zero_rows(10, 10, 0, 5);
    size_t count1 = 0;
    while (iter_zero_rows.has_next() && count1 < 1000) {
        iter_zero_rows.next_block();
        count1++;
    }
    FATP_ASSERT_TRUE(count1 < 1000, "Zero block_rows should be clamped, no infinite loop");
    FATP_ASSERT_TRUE(count1 == 20, "Should iterate correctly with clamped block_rows=1");
    
    BlockIterator2D iter_zero_cols(10, 10, 5, 0);
    size_t count2 = 0;
    while (iter_zero_cols.has_next() && count2 < 1000) {
        iter_zero_cols.next_block();
        count2++;
    }
    FATP_ASSERT_TRUE(count2 < 1000, "Zero block_cols should be clamped, no infinite loop");
    FATP_ASSERT_TRUE(count2 == 20, "Should iterate correctly with clamped block_cols=1");
    
    // Zero dimensions should terminate immediately
    BlockIterator2D iter_zero_dim(0, 10, 5, 5);
    FATP_ASSERT_TRUE(!iter_zero_dim.has_next(), "Zero rows should have no blocks");
    
    BlockIterator2D iter_zero_cols_dim(10, 0, 5, 5);
    FATP_ASSERT_TRUE(!iter_zero_cols_dim.has_next(), "Zero cols should have no blocks");
    
    return true;
}

// =============================================================================
// Alignment Utilities Tests
// =============================================================================

FATP_TEST_CASE(alignment_utilities) {
    alignas(64) char buffer[128];
    
    // Test is_aligned
    FATP_ASSERT_TRUE(is_aligned(buffer, 64), "Buffer should be 64-byte aligned");
    FATP_ASSERT_TRUE(is_aligned(buffer, 32), "64-byte aligned is also 32-byte aligned");
    FATP_ASSERT_TRUE(is_aligned(buffer, 16), "64-byte aligned is also 16-byte aligned");
    FATP_ASSERT_TRUE(is_aligned(buffer, 1), "Any pointer is 1-byte aligned");
    
    // Test unaligned
    char* unaligned = buffer + 1;
    FATP_ASSERT_TRUE(!is_aligned(unaligned, 64), "buffer+1 should not be 64-byte aligned");
    FATP_ASSERT_TRUE(!is_aligned(unaligned, 2), "buffer+1 should not be 2-byte aligned");
    
    // Test align_up
    void* aligned_up = align_up(unaligned, 64);
    FATP_ASSERT_TRUE(is_aligned(aligned_up, 64), "align_up should produce aligned pointer");
    FATP_ASSERT_TRUE(aligned_up >= unaligned, "align_up should not go backward");
    
    // Test align_down
    void* aligned_down = align_down(unaligned, 64);
    FATP_ASSERT_TRUE(is_aligned(aligned_down, 64), "align_down should produce aligned pointer");
    FATP_ASSERT_TRUE(aligned_down <= unaligned, "align_down should not go forward");
    
    // Test alignment_offset
    size_t offset = alignment_offset(unaligned, 64);
    FATP_ASSERT_TRUE(offset == 63, "Offset should be 63 bytes");
    
    offset = alignment_offset(buffer, 64);
    FATP_ASSERT_TRUE(offset == 0, "Offset for aligned pointer should be 0");
    
    return true;
}

FATP_TEST_CASE(alignment_utilities_const) {
    alignas(64) const char buffer[128] = {};
    const char* unaligned = buffer + 1;
    
    // Test const overloads
    const void* aligned_up = align_up(unaligned, 64);
    FATP_ASSERT_TRUE(is_aligned(aligned_up, 64), "const align_up should work");
    
    const void* aligned_down = align_down(unaligned, 64);
    FATP_ASSERT_TRUE(is_aligned(aligned_down, 64), "const align_down should work");
    
    return true;
}

// =============================================================================
// Benchmarks
// =============================================================================

void benchmark_cache_utilities() {
    std::cout << "\n" << colors::cyan() << "CacheUtilities Benchmarks:" << colors::reset() << "\n\n";
    
    constexpr size_t N = 10000;
    
    // Use heap allocation to avoid stack overflow warning (C6262)
    auto data_storage = std::make_unique<float[]>(N + 16);  // Extra for alignment
    float* data = reinterpret_cast<float*>(
        align_up(data_storage.get(), 64));
    std::iota(data, data + N, 0.0f);
    
    // Flush cache before benchmarking
    flush_cache_range(data, N * sizeof(float));
    store_fence();
    
    // Benchmark without prefetch
    double no_prefetch_time = measure_perf([&]() {
        float sum = 0.0f;
        for (size_t i = 0; i < N; ++i) {
            sum += data[i];
        }
        DoNotOptimize(sum);
    }, 100000, 100);
    
    std::cout << "Sum without prefetch: " << format_time(no_prefetch_time) << "\n";
    
    // Benchmark with prefetch
    double prefetch_time = measure_perf([&]() {
        float sum = 0.0f;
        for (size_t i = 0; i < N - 8; ++i) {
            prefetch_ahead<float>(data, i);
            sum += data[i];
        }
        DoNotOptimize(sum);
    }, 100000, 100);
    
    std::cout << "Sum with prefetch: " << format_time(prefetch_time) << "\n";
    
    // Benchmark memcpy vs stream_copy (aligned)
    constexpr size_t COPY_SIZE = 8192;
    auto src_storage = std::make_unique<char[]>(COPY_SIZE + 64);
    auto dest_storage = std::make_unique<char[]>(COPY_SIZE + 64);
    char* src = reinterpret_cast<char*>(align_up(src_storage.get(), 64));
    char* dest = reinterpret_cast<char*>(align_up(dest_storage.get(), 64));
    std::memset(src, 0xAB, COPY_SIZE);
    
    double memcpy_time = measure_perf([&]() {
        std::memcpy(dest, src, COPY_SIZE);
        DoNotOptimize(dest);
    }, 100000, 100);
    
    std::cout << "memcpy (8KB aligned): " << format_time(memcpy_time) << "\n";
    
    double stream_time = measure_perf([&]() {
        stream_copy(dest, src, COPY_SIZE);
        DoNotOptimize(dest);
    }, 100000, 100);
    
    std::cout << "stream_copy (8KB aligned): " << format_time(stream_time) << "\n";
    
    // Benchmark false sharing prevention
    std::cout << "\nFalse sharing test (informational - may not show difference in single-threaded test):\n";
    std::cout << "  sizeof(int): " << sizeof(int) << " bytes\n";
    std::cout << "  sizeof(CacheAligned<int>): " << sizeof(CacheAligned<int>) << " bytes\n";
    std::cout << "  sizeof(CacheLinePadded<int>): " << sizeof(CacheLinePadded<int>) << " bytes\n";
}

} // namespace fat_p::testing::cacheutilities

namespace fat_p::testing
{

// =============================================================================
// Test Runner
// =============================================================================

bool test_CacheUtilities() {

    FATP_PRINT_HEADER(CACHE UTILITIES)

    // Enable verbose output to see individual test results
    get_test_config().verbose = true;

    TestRunner runner;

    // CacheInfo tests
    FATP_RUN_TEST_NS(runner, cacheutilities, cache_info);
    
    // Prefetch tests
    FATP_RUN_TEST_NS(runner, cacheutilities, prefetch_operations);
    FATP_RUN_TEST_NS(runner, cacheutilities, prefetch_range_zero_size);
    FATP_RUN_TEST_NS(runner, cacheutilities, prefetch_ahead);
    FATP_RUN_TEST_NS(runner, cacheutilities, prefetch_ahead_overflow);
    
    // Cache flush tests
    FATP_RUN_TEST_NS(runner, cacheutilities, cache_flush);
    FATP_RUN_TEST_NS(runner, cacheutilities, flush_cache_range_zero_size);
    
    // Stream store tests
    FATP_RUN_TEST_NS(runner, cacheutilities, stream_store);
    FATP_RUN_TEST_NS(runner, cacheutilities, stream_store_8byte);
    FATP_RUN_TEST_NS(runner, cacheutilities, stream_store_unaligned);
    
    // Stream copy tests
    FATP_RUN_TEST_NS(runner, cacheutilities, stream_copy);
    FATP_RUN_TEST_NS(runner, cacheutilities, stream_copy_unaligned);
    FATP_RUN_TEST_NS(runner, cacheutilities, stream_copy_zero_size);
    
    // CacheAligned tests
    FATP_RUN_TEST_NS(runner, cacheutilities, cache_aligned);
    FATP_RUN_TEST_NS(runner, cacheutilities, cache_aligned_array);
    
    // CacheLinePadded tests
    FATP_RUN_TEST_NS(runner, cacheutilities, cache_line_padded);
    FATP_RUN_TEST_NS(runner, cacheutilities, cache_line_padded_alignment);
    FATP_RUN_TEST_NS(runner, cacheutilities, cache_line_padded_various_sizes);
    FATP_RUN_TEST_NS(runner, cacheutilities, cache_line_padded_exact_size);
    
    // Memory barrier tests
    FATP_RUN_TEST_NS(runner, cacheutilities, memory_barriers);
    
    // Blocking utilities tests
    FATP_RUN_TEST_NS(runner, cacheutilities, optimal_block_size);
    FATP_RUN_TEST_NS(runner, cacheutilities, optimal_block_size_edge_cases);
    FATP_RUN_TEST_NS(runner, cacheutilities, block_iterator_2d);
    FATP_RUN_TEST_NS(runner, cacheutilities, block_iterator_2d_non_divisible);
    FATP_RUN_TEST_NS(runner, cacheutilities, block_iterator_2d_zero_blocks);
    
    // Alignment utilities tests
    FATP_RUN_TEST_NS(runner, cacheutilities, alignment_utilities);
    FATP_RUN_TEST_NS(runner, cacheutilities, alignment_utilities_const);

    cacheutilities::benchmark_cache_utilities();

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_CacheUtilities() ? 0 : 1;
}
#endif
