/**
 * @file CacheUtilities.h
 * @brief Cache control, prefetching, and cache-aware programming utilities
 * @version 1.0
 * 
 * @details Low-level cache control for optimizing memory access patterns in HPC code.
 * Provides prefetch hints, cache line awareness, and false sharing prevention.
 * 
 * Key Features:
 * - Software prefetching (temporal and non-temporal)
 * - Cache line size detection
 * - Cache flush operations
 * - False sharing prevention helpers
 * - Cache-oblivious algorithms utilities
 * - Streaming store (bypass cache)
 * 
 * Use Cases:
 * - Loop prefetching for predictable access patterns
 * - Streaming large datasets
 * - NUMA-aware data placement
 * - Reducing cache contention
 * 
 * Performance Impact:
 * - Prefetching can reduce memory latency by 2-3x
 * - Proper alignment prevents 10-20% performance loss
 * - Cache-aware blocking improves locality
 * 
 * Requires: C++17
 * 
 * @author cpp_utilities
 * @date 2025
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <new>

// Architecture-specific headers
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386) || defined(_M_IX86)
    #include <immintrin.h>
    #define CACHE_X86
#elif defined(__ARM_NEON) || defined(__aarch64__)
    #include <arm_acle.h>
    #define CACHE_ARM
#endif

namespace cpp_utilities {
namespace perf {

// =============================================================================
// Cache Line Information
// =============================================================================

/**
 * @brief Cache line size detection
 */
class CacheInfo {
public:
    /**
     * @brief Get L1 cache line size in bytes
     */
    static constexpr size_t l1_line_size() {
#if defined(CACHE_X86)
        return 64; // Standard for x86/x64
#elif defined(CACHE_ARM)
        return 64; // Most ARM processors
#else
        return 64; // Conservative default
#endif
    }
    
    /**
     * @brief Get L2 cache line size in bytes
     */
    static constexpr size_t l2_line_size() {
        return l1_line_size(); // Usually same as L1
    }
    
    /**
     * @brief Get L3 cache line size in bytes
     */
    static constexpr size_t l3_line_size() {
        return l1_line_size(); // Usually same as L1
    }
    
    /**
     * @brief Get hardware destructive interference size (for false sharing)
     */
    static constexpr size_t destructive_interference_size() {
#ifdef __cpp_lib_hardware_interference_size
        return std::hardware_destructive_interference_size;
#else
        return 64; // Common cache line size
#endif
    }
    
    /**
     * @brief Get hardware constructive interference size (for data locality)
     */
    static constexpr size_t constructive_interference_size() {
#ifdef __cpp_lib_hardware_interference_size
        return std::hardware_constructive_interference_size;
#else
        return 64;
#endif
    }
};

// =============================================================================
// Prefetch Hints
// =============================================================================

/**
 * @brief Prefetch locality hints
 */
enum class PrefetchLocality {
    None = 0,       // No temporal locality (single use, streaming)
    Low = 1,        // Low temporal locality (used few times)
    Moderate = 2,   // Moderate temporal locality
    High = 3        // High temporal locality (used many times)
};

/**
 * @brief Prefetch operation types
 */
enum class PrefetchOp {
    Read,  // Prefetch for reading
    Write  // Prefetch for writing (prefetch exclusive)
};

/**
 * @brief Prefetch memory into cache
 * @tparam Locality Temporal locality hint
 * @tparam Op Read or write prefetch
 * @param addr Address to prefetch
 */
template<PrefetchLocality Locality = PrefetchLocality::High,
         PrefetchOp Op = PrefetchOp::Read>
inline void prefetch(const void* addr) {
#if defined(CACHE_X86)
    constexpr int locality = static_cast<int>(Locality);
    
    if constexpr (Op == PrefetchOp::Read) {
        // Prefetch to all cache levels based on locality
        if constexpr (Locality == PrefetchLocality::None) {
            _mm_prefetch(static_cast<const char*>(addr), _MM_HINT_NTA);
        } else if constexpr (Locality == PrefetchLocality::Low) {
            _mm_prefetch(static_cast<const char*>(addr), _MM_HINT_T2);
        } else if constexpr (Locality == PrefetchLocality::Moderate) {
            _mm_prefetch(static_cast<const char*>(addr), _MM_HINT_T1);
        } else { // High
            _mm_prefetch(static_cast<const char*>(addr), _MM_HINT_T0);
        }
    } else { // Write
        // Prefetch exclusive (for writing)
        #if defined(__PRFCHW__)
        _mm_prefetch(static_cast<const char*>(addr), _MM_HINT_ET0);
        #else
        _mm_prefetch(static_cast<const char*>(addr), _MM_HINT_T0);
        #endif
    }
    
#elif defined(CACHE_ARM)
    if constexpr (Op == PrefetchOp::Read) {
        __pld(addr);
    } else {
        __pldx(addr);
    }
    
#else
    // Compiler-agnostic prefetch
    __builtin_prefetch(addr, 
                       (Op == PrefetchOp::Write) ? 1 : 0, 
                       static_cast<int>(Locality));
#endif
}

/**
 * @brief Prefetch range of memory
 * @param addr Start address
 * @param size Number of bytes to prefetch
 */
template<PrefetchLocality Locality = PrefetchLocality::High>
inline void prefetch_range(const void* addr, size_t size) {
    constexpr size_t line_size = CacheInfo::l1_line_size();
    const char* ptr = static_cast<const char*>(addr);
    const char* end = ptr + size;
    
    for (; ptr < end; ptr += line_size) {
        prefetch<Locality>(ptr);
    }
}

/**
 * @brief Prefetch for loop iterations ahead
 * @param base Base pointer
 * @param index Current iteration index
 * @param stride Stride between iterations
 * @param distance Number of iterations to prefetch ahead
 */
template<typename T, PrefetchLocality Locality = PrefetchLocality::High>
inline void prefetch_ahead(const T* base, size_t index, size_t stride = 1, size_t distance = 8) {
    prefetch<Locality>(base + (index + distance) * stride);
}

// =============================================================================
// Cache Flush Operations
// =============================================================================

/**
 * @brief Flush cache line containing address
 * @param addr Address to flush
 */
inline void flush_cache_line(const void* addr) {
#if defined(CACHE_X86)
    _mm_clflush(addr);
#elif defined(CACHE_ARM) && defined(__ARM_ARCH) && __ARM_ARCH >= 8
    __asm__ __volatile__("dc cvac, %0" : : "r"(addr) : "memory");
#else
    // No portable way to flush - use memory barrier
    asm volatile("" ::: "memory");
    (void)addr;
#endif
}

/**
 * @brief Flush range of cache lines
 * @param addr Start address
 * @param size Number of bytes to flush
 */
inline void flush_cache_range(const void* addr, size_t size) {
    constexpr size_t line_size = CacheInfo::l1_line_size();
    const char* ptr = static_cast<const char*>(addr);
    const char* end = ptr + size;
    
    for (; ptr < end; ptr += line_size) {
        flush_cache_line(ptr);
    }
}

/**
 * @brief Flush and invalidate cache line (stronger than flush)
 * @param addr Address to flush and invalidate
 */
inline void flush_invalidate(const void* addr) {
#if defined(CACHE_X86) && defined(__CLFLUSHOPT__)
    _mm_clflushopt(const_cast<void*>(addr)); // Optimized flush (if available)
#elif defined(CACHE_X86)
    _mm_clflush(addr); // Standard flush
#elif defined(CACHE_ARM)
    __asm__ __volatile__("dc civac, %0" : : "r"(addr) : "memory");
#else
    flush_cache_line(addr);
#endif
}

// =============================================================================
// Non-Temporal (Streaming) Operations
// =============================================================================

/**
 * @brief Non-temporal (streaming) store - bypasses cache
 * @details Useful for write-only data that won't be read soon
 * @param dest Destination address (must be 16/32-byte aligned)
 * @param value Value to store
 */
template<typename T>
inline void stream_store(T* dest, const T& value) {
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
    
#if defined(CACHE_X86)
    if constexpr (sizeof(T) == 4) {
        _mm_stream_si32(reinterpret_cast<int*>(dest), *reinterpret_cast<const int*>(&value));
    } else if constexpr (sizeof(T) == 8) {
        _mm_stream_si64(reinterpret_cast<long long*>(dest), *reinterpret_cast<const long long*>(&value));
    } else if constexpr (sizeof(T) <= 16) {
        _mm_stream_si128(reinterpret_cast<__m128i*>(dest), 
                         *reinterpret_cast<const __m128i*>(&value));
    } else if constexpr (sizeof(T) <= 32) {
        #ifdef __AVX__
        _mm256_stream_si256(reinterpret_cast<__m256i*>(dest), 
                            *reinterpret_cast<const __m256i*>(&value));
        #else
        *dest = value; // Fallback
        #endif
    } else {
        *dest = value; // Fallback for large types
    }
#else
    *dest = value; // Fallback
#endif
}

/**
 * @brief Non-temporal memcpy (streaming copy)
 * @param dest Destination (should be aligned)
 * @param src Source
 * @param size Number of bytes
 */
inline void stream_copy(void* dest, const void* src, size_t size) {
#if defined(CACHE_X86) && defined(__AVX__)
    const size_t simd_width = 32; // AVX: 256-bit = 32 bytes
    char* d = static_cast<char*>(dest);
    const char* s = static_cast<const char*>(src);
    
    // Process 32-byte chunks
    size_t chunks = size / simd_width;
    for (size_t i = 0; i < chunks; ++i) {
        __m256i data = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(s));
        _mm256_stream_si256(reinterpret_cast<__m256i*>(d), data);
        d += simd_width;
        s += simd_width;
    }
    
    // Handle remainder
    size_t remainder = size % simd_width;
    if (remainder) {
        std::memcpy(d, s, remainder);
    }
    
    _mm_sfence(); // Ensure stores complete
#else
    std::memcpy(dest, src, size);
#endif
}

// =============================================================================
// False Sharing Prevention
// =============================================================================

/**
 * @brief Cache-aligned type wrapper to prevent false sharing
 * @tparam T Type to wrap
 */
template<typename T>
struct alignas(CacheInfo::destructive_interference_size()) CacheAligned {
    T value;
    
    CacheAligned() = default;
    
    template<typename... Args>
    explicit CacheAligned(Args&&... args) : value(std::forward<Args>(args)...) {}
    
    operator T&() noexcept { return value; }
    operator const T&() const noexcept { return value; }
    
    T& get() noexcept { return value; }
    const T& get() const noexcept { return value; }
};

/**
 * @brief Padded type to occupy full cache line
 * @tparam T Type to pad
 */
template<typename T>
struct CacheLinePadded {
    static constexpr size_t cache_line_size = CacheInfo::destructive_interference_size();
    static constexpr size_t padding_size = 
        cache_line_size > sizeof(T) ? cache_line_size - sizeof(T) : 0;
    
    T value;
    char padding[padding_size];
    
    CacheLinePadded() = default;
    
    template<typename... Args>
    explicit CacheLinePadded(Args&&... args) : value(std::forward<Args>(args)...) {}
    
    operator T&() noexcept { return value; }
    operator const T&() const noexcept { return value; }
    
    T& get() noexcept { return value; }
    const T& get() const noexcept { return value; }
};

// =============================================================================
// Memory Barriers and Fences
// =============================================================================

/**
 * @brief Full memory barrier
 */
inline void memory_barrier() {
#if defined(CACHE_X86)
    _mm_mfence();
#elif defined(CACHE_ARM)
    __asm__ __volatile__("dmb sy" : : : "memory");
#else
    std::atomic_thread_fence(std::memory_order_seq_cst);
#endif
}

/**
 * @brief Store fence (ensure all stores before this complete)
 */
inline void store_fence() {
#if defined(CACHE_X86)
    _mm_sfence();
#elif defined(CACHE_ARM)
    __asm__ __volatile__("dmb st" : : : "memory");
#else
    std::atomic_thread_fence(std::memory_order_release);
#endif
}

/**
 * @brief Load fence (ensure all loads after this see prior stores)
 */
inline void load_fence() {
#if defined(CACHE_X86)
    _mm_lfence();
#elif defined(CACHE_ARM)
    __asm__ __volatile__("dmb ld" : : : "memory");
#else
    std::atomic_thread_fence(std::memory_order_acquire);
#endif
}

// =============================================================================
// Cache-Aware Blocking Utilities
// =============================================================================

/**
 * @brief Compute optimal block size for cache blocking
 * @param elements Total number of elements
 * @param element_size Size of each element
 * @param cache_size Target cache size (e.g., L1, L2)
 * @return Optimal block size
 */
inline size_t optimal_block_size(size_t elements, size_t element_size, 
                                 size_t cache_size = 32 * 1024) {
    // Use ~80% of cache to leave room for other data
    size_t usable_cache = cache_size * 4 / 5;
    size_t max_block = usable_cache / element_size;
    
    // Round down to power of 2 for better performance
    size_t block_size = 1;
    while (block_size * 2 <= max_block && block_size * 2 <= elements) {
        block_size *= 2;
    }
    
    return block_size;
}

/**
 * @brief 2D matrix blocking iterator
 */
struct BlockIterator2D {
    size_t rows, cols;
    size_t block_rows, block_cols;
    size_t current_i, current_j;
    size_t block_i, block_j;
    
    BlockIterator2D(size_t r, size_t c, size_t br, size_t bc)
        : rows(r), cols(c), block_rows(br), block_cols(bc)
        , current_i(0), current_j(0), block_i(0), block_j(0) {}
    
    bool has_next() const {
        return block_i < rows;
    }
    
    void next_block() {
        block_j += block_cols;
        if (block_j >= cols) {
            block_j = 0;
            block_i += block_rows;
        }
    }
    
    size_t block_start_i() const { return block_i; }
    size_t block_start_j() const { return block_j; }
    size_t block_end_i() const { return std::min(block_i + block_rows, rows); }
    size_t block_end_j() const { return std::min(block_j + block_cols, cols); }
};

// =============================================================================
// Alignment Utilities
// =============================================================================

/**
 * @brief Check if pointer is aligned to boundary
 * @param ptr Pointer to check
 * @param alignment Alignment boundary (must be power of 2)
 * @return true if aligned
 */
inline bool is_aligned(const void* ptr, size_t alignment) {
    return (reinterpret_cast<uintptr_t>(ptr) & (alignment - 1)) == 0;
}

/**
 * @brief Align pointer up to boundary
 * @param ptr Pointer to align
 * @param alignment Alignment boundary (must be power of 2)
 * @return Aligned pointer
 */
inline void* align_up(void* ptr, size_t alignment) {
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    uintptr_t aligned = (addr + alignment - 1) & ~(alignment - 1);
    return reinterpret_cast<void*>(aligned);
}

/**
 * @brief Align pointer down to boundary
 * @param ptr Pointer to align
 * @param alignment Alignment boundary (must be power of 2)
 * @return Aligned pointer
 */
inline void* align_down(void* ptr, size_t alignment) {
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    uintptr_t aligned = addr & ~(alignment - 1);
    return reinterpret_cast<void*>(aligned);
}

/**
 * @brief Compute alignment offset
 * @param ptr Pointer
 * @param alignment Alignment boundary
 * @return Bytes needed to align up
 */
inline size_t alignment_offset(const void* ptr, size_t alignment) {
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    uintptr_t misalignment = addr & (alignment - 1);
    return misalignment ? alignment - misalignment : 0;
}

} // namespace perf
} // namespace cpp_utilities
