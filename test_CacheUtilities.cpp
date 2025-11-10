#include <iostream>
#include <vector>
#include <numeric>
#include <cstring>

#include "CacheUtilities.h"
#include "test_CacheUtilities.h"
#include "test_Utilities.h"

namespace cpp_utilities::testing
{

using namespace perf;

bool test_cache_info() {
    std::cout << "L1 cache line size: " << CacheInfo::l1_line_size() << " bytes\n";
    std::cout << "Destructive interference size: " << CacheInfo::destructive_interference_size() << " bytes\n";
    std::cout << "Constructive interference size: " << CacheInfo::constructive_interference_size() << " bytes\n";
    
    SIMPLE_ASSERT(CacheInfo::l1_line_size() >= 16, "Cache line should be at least 16 bytes");
    SIMPLE_ASSERT(CacheInfo::l1_line_size() <= 256, "Cache line should be at most 256 bytes");
    
    return true;
}

bool test_prefetch_operations() {
    alignas(64) int data[1024];
    for (int i = 0; i < 1024; ++i) {
        data[i] = i;
    }
    
    // Test various prefetch operations
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
    
    SIMPLE_ASSERT(sum == (1023 * 1024) / 2, "Data should be accessible after prefetch");
    
    return true;
}

bool test_prefetch_ahead() {
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
    
    SIMPLE_ASSERT(sum > 0.0f, "Prefetch ahead should work");
    
    return true;
}

bool test_cache_flush() {
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
    
    // Verify data is still accessible
    SIMPLE_ASSERT(data[0] == 0, "Data should still be correct after flush");
    
    return true;
}

bool test_stream_store() {
    alignas(64) int data[64];
    
    for (int i = 0; i < 64; ++i) {
        stream_store(&data[i], i);
    }
    
    store_fence(); // Ensure stores complete
    
    for (int i = 0; i < 64; ++i) {
        SIMPLE_ASSERT(data[i] == i, "Stream store should write correct values");
    }
    
    return true;
}

bool test_stream_copy() {
    alignas(64) char src[1024];
    alignas(64) char dest[1024];
    
    for (int i = 0; i < 1024; ++i) {
        src[i] = static_cast<char>(i % 256);
    }
    
    stream_copy(dest, src, 1024);
    
    for (int i = 0; i < 1024; ++i) {
        SIMPLE_ASSERT(dest[i] == src[i], "Stream copy should copy correctly");
    }
    
    return true;
}

bool test_cache_aligned() {
    CacheAligned<int> val1(42);
    SIMPLE_ASSERT(val1.get() == 42, "CacheAligned should store value");
    
    CacheAligned<double> val2;
    val2.get() = 3.14;
    SIMPLE_ASSERT(val2.get() == 3.14, "CacheAligned should be modifiable");
    
    // Check alignment
    SIMPLE_ASSERT(reinterpret_cast<uintptr_t>(&val1) % CacheInfo::destructive_interference_size() == 0,
                  "CacheAligned should be properly aligned");
    
    return true;
}

bool test_cache_line_padded() {
    CacheLinePadded<int> val1(100);
    SIMPLE_ASSERT(val1.get() == 100, "CacheLinePadded should store value");
    
    SIMPLE_ASSERT(sizeof(CacheLinePadded<int>) >= CacheInfo::destructive_interference_size(),
                  "CacheLinePadded should be at least cache line size");
    
    return true;
}

bool test_memory_barriers() {
    alignas(64) int data[2] = {0, 0};
    
    data[0] = 1;
    memory_barrier();
    data[1] = 2;
    
    SIMPLE_ASSERT(data[0] == 1 && data[1] == 2, "Memory barrier should not corrupt data");
    
    data[0] = 3;
    store_fence();
    
    load_fence();
    int val = data[0];
    
    SIMPLE_ASSERT(val == 3, "Fences should not corrupt data");
    
    return true;
}

bool test_optimal_block_size() {
    size_t block_size = optimal_block_size(10000, sizeof(double), 32768);
    
    SIMPLE_ASSERT(block_size > 0, "Block size should be positive");
    SIMPLE_ASSERT(block_size * sizeof(double) <= 32768, "Block should fit in cache");
    
    return true;
}

bool test_block_iterator_2d() {
    BlockIterator2D iter(100, 100, 16, 16);
    
    size_t block_count = 0;
    while (iter.has_next()) {
        size_t si = iter.block_start_i();
        size_t sj = iter.block_start_j();
        size_t ei = iter.block_end_i();
        size_t ej = iter.block_end_j();
        
        SIMPLE_ASSERT(ei > si, "Block should have positive height");
        SIMPLE_ASSERT(ej > sj, "Block should have positive width");
        
        iter.next_block();
        block_count++;
    }
    
    // 100x100 divided into 16x16 blocks = 7x7 = 49 blocks (with remainder)
    SIMPLE_ASSERT(block_count > 0, "Should have at least one block");
    
    return true;
}

bool test_alignment_utilities() {
    alignas(64) char buffer[128];
    
    // Test is_aligned
    SIMPLE_ASSERT(is_aligned(buffer, 64), "Buffer should be 64-byte aligned");
    SIMPLE_ASSERT(is_aligned(buffer, 32), "64-byte aligned is also 32-byte aligned");
    SIMPLE_ASSERT(is_aligned(buffer, 16), "64-byte aligned is also 16-byte aligned");
    
    // Test align_up
    char* unaligned = buffer + 1;
    void* aligned_up = align_up(unaligned, 64);
    SIMPLE_ASSERT(is_aligned(aligned_up, 64), "align_up should produce aligned pointer");
    
    // Test align_down
    void* aligned_down = align_down(unaligned, 64);
    SIMPLE_ASSERT(is_aligned(aligned_down, 64), "align_down should produce aligned pointer");
    
    // Test alignment_offset
    size_t offset = alignment_offset(unaligned, 64);
    SIMPLE_ASSERT(offset == 63, "Offset should be 63 bytes");
    
    return true;
}

void benchmark_cache_utilities() {
    std::cout << "\n" << colors::cyan() << "CacheUtilities Benchmarks:" << colors::reset() << "\n\n";
    
    constexpr size_t N = 10000;
    alignas(64) float data[N];
    std::iota(data, data + N, 0.0f);
    
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
    
    // Benchmark memcpy vs stream_copy
    alignas(64) char src[8192], dest[8192];
    
    double memcpy_time = measure_perf([&]() {
        std::memcpy(dest, src, 8192);
        DoNotOptimize(dest);
    }, 100000, 100);
    
    std::cout << "memcpy (8KB): " << format_time(memcpy_time) << "\n";
    
    double stream_time = measure_perf([&]() {
        stream_copy(dest, src, 8192);
        DoNotOptimize(dest);
    }, 100000, 100);
    
    std::cout << "stream_copy (8KB): " << format_time(stream_time) << "\n";
}

bool test_CacheUtilities() {

    PRINT_HEADER(CACHE UTILITIES)

    TestRunner runner;

    RUN_TEST(runner, cache_info);
    RUN_TEST(runner, prefetch_operations);
    RUN_TEST(runner, prefetch_ahead);
    RUN_TEST(runner, cache_flush);
    RUN_TEST(runner, stream_store);
    RUN_TEST(runner, stream_copy);
    RUN_TEST(runner, cache_aligned);
    RUN_TEST(runner, cache_line_padded);
    RUN_TEST(runner, memory_barriers);
    RUN_TEST(runner, optimal_block_size);
    RUN_TEST(runner, block_iterator_2d);
    RUN_TEST(runner, alignment_utilities);

    benchmark_cache_utilities();

    return 0 == runner.print_summary();
}

} // namespace cpp_utilities::testing
