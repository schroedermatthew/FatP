/**
 * @file CacheUtilities.h
 * @brief Cache control, prefetching, and cache-aware programming utilities
 *
 *
 * @layer Domain
 *
 * @version 1.1
 *
 * Version History:
 * - 1.1: Fixed critical bugs identified in code review:
 *        - CacheLinePadded: Added alignas, fixed zero-length array, added static_assert
 *        - stream_store: Added alignment checks, fixed UB for partial-size types
 *        - stream_copy: Added alignment check with fallback
 *        - ARM prefetch: Use __builtin_prefetch for portability (Apple Silicon)
 *        - BlockIterator2D: Fixed has_next() logic
 *        - Added noexcept throughout
 *        - Added const overloads for align_up/align_down
 *        - Apple Silicon: 128-byte destructive interference size
 * - 1.0: Initial release
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

/*
FATP_META:
  meta_version: 1
  component: CacheUtilities
  file_role: public_header
  path: fat_p/CacheUtilities.h
  namespace: fat_p
  layer: Domain
  summary: "Public header for CacheUtilities."
  api_stability: in_work
  related:
    docs_search: "CacheUtilities"
    tests:
      - tests/test_CacheUtilities.cpp
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 5
    defines_unprefixed: 5
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/
#include "FatPConfig.h"
#include <algorithm>
#include <atomic> // for std::atomic_thread_fence
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <type_traits>
#include <utility> // for std::forward

// Architecture-specific headers
#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#define FATP_CACHE_X86
#define FATP_CACHE_X86_64
#elif defined(__i386) || defined(_M_IX86)
#include <immintrin.h>
#define FATP_CACHE_X86
#define FATP_CACHE_X86_32
#elif defined(__ARM_NEON) || defined(__aarch64__)
// Use __builtin_prefetch for better portability (Apple Silicon, Android, Linux ARM)
// arm_acle.h has inconsistent support across compilers
#define FATP_CACHE_ARM
#endif

namespace fat_p
{
namespace perf
{

// =============================================================================
// Cache Line Information
// =============================================================================

/**
 * @brief Compile-time cache line size constants
 *
 * These are constexpr values that can be used in alignas() and template parameters.
 * For runtime detection, see CacheInfo::mDetect*() functions (if implemented).
 */
namespace cache_constants
{

/**
 * @brief L1 cache line size in bytes
 */
inline constexpr size_t l1_line_size_v =
#if defined(FATP_CACHE_X86)
    64 // Standard for all x86/x64
#elif defined(__APPLE__) && defined(__aarch64__)
    128 // Apple Silicon (M1/M2/M3) uses 128-byte lines
#elif defined(FATP_CACHE_ARM)
    64 // Most ARM processors (Cortex-A series)
#else
    64 // Conservative default
#endif
    ;

/**
 * @brief Hardware destructive interference size (minimum spacing to avoid false sharing)
 *
 * On Apple Silicon, the coherency granule is 128 bytes, meaning two variables
 * within 128 bytes of each other can cause false sharing.
 */
inline constexpr size_t destructive_interference_size_v = l1_line_size_v;

/**
 * @brief Hardware constructive interference size (maximum size for co-located data)
 */
inline constexpr size_t constructive_interference_size_v = l1_line_size_v;

} // namespace cache_constants

/**
 * @brief Cache line size detection (legacy interface, uses constexpr values)
 */
class CacheInfo
{
public:
    static constexpr size_t l1_line_size() noexcept
    {
        return cache_constants::l1_line_size_v;
    }

    static constexpr size_t l2_line_size() noexcept
    {
        return cache_constants::l1_line_size_v; // Usually same as L1
    }

    static constexpr size_t l3_line_size() noexcept
    {
        return cache_constants::l1_line_size_v; // Usually same as L1
    }

    static constexpr size_t destructive_interference_size() noexcept
    {
        return cache_constants::destructive_interference_size_v;
    }

    static constexpr size_t constructive_interference_size() noexcept
    {
        return cache_constants::constructive_interference_size_v;
    }
};

// =============================================================================
// Forward Declarations (alignment utilities needed by stream_store)
// =============================================================================

inline bool is_aligned(const void* ptr, size_t alignment) noexcept;

// =============================================================================
// Prefetch Hints
// =============================================================================

/**
 * @brief Prefetch locality hints
 */
enum class PrefetchLocality
{
    None = 0,     // No temporal locality (single use, streaming)
    Low = 1,      // Low temporal locality (used few times)
    Moderate = 2, // Moderate temporal locality
    High = 3      // High temporal locality (used many times)
};

/**
 * @brief Prefetch operation types
 */
enum class PrefetchOp
{
    Read, // Prefetch for reading
    Write // Prefetch for writing (prefetch exclusive)
};

/**
 * @brief Prefetch memory into cache
 * @tparam Locality Temporal locality hint
 * @tparam Op Read or write prefetch
 * @param addr Address to prefetch
 */
template <PrefetchLocality Locality = PrefetchLocality::High, PrefetchOp Op = PrefetchOp::Read>
inline void prefetch(const void* addr) noexcept
{
#if defined(FATP_CACHE_X86)
    if constexpr (Op == PrefetchOp::Read)
    {
        // Prefetch to cache levels based on locality hint
        if constexpr (Locality == PrefetchLocality::None)
        {
            _mm_prefetch(static_cast<const char*>(addr), _MM_HINT_NTA);
        }
        else if constexpr (Locality == PrefetchLocality::Low)
        {
            _mm_prefetch(static_cast<const char*>(addr), _MM_HINT_T2);
        }
        else if constexpr (Locality == PrefetchLocality::Moderate)
        {
            _mm_prefetch(static_cast<const char*>(addr), _MM_HINT_T1);
        }
        else
        { // High
            _mm_prefetch(static_cast<const char*>(addr), _MM_HINT_T0);
        }
    }
    else
    { // Write
        // Write prefetch - use T0 hint (exclusive state)
        // Note: _MM_HINT_ET0 is not portable across compilers
        _mm_prefetch(static_cast<const char*>(addr), _MM_HINT_T0);
    }

#else
    // Portable path for ARM and other architectures
    // __builtin_prefetch works on GCC, Clang, and Apple Clang
    // Parameters: (addr, rw, locality) where rw: 0=read, 1=write
    __builtin_prefetch(addr, (Op == PrefetchOp::Write) ? 1 : 0, static_cast<int>(Locality));
#endif
}

/**
 * @brief Prefetch range of memory
 * @param addr Start address
 * @param size Number of bytes to prefetch (0 is valid, does nothing)
 */
template <PrefetchLocality Locality = PrefetchLocality::High>
inline void prefetch_range(const void* addr, size_t size) noexcept
{
    if (size == 0)
    {
        return;
    }

    constexpr size_t line_size = cache_constants::l1_line_size_v;
    const char* ptr = static_cast<const char*>(addr);
    const char* end = ptr + size;

    for (; ptr < end; ptr += line_size)
    {
        prefetch<Locality>(ptr);
    }
}

/**
 * @brief Prefetch for loop iterations ahead
 * @param base Base pointer
 * @param index Current iteration index
 * @param stride Stride between iterations (elements, not bytes)
 * @param distance Number of iterations to prefetch ahead
 *
 * @note Safe against overflow: if index + distance wraps, no prefetch is issued
 */
template <typename T, PrefetchLocality Locality = PrefetchLocality::High>
inline void prefetch_ahead(const T* base, size_t index, size_t stride = 1, size_t distance = 8) noexcept
{
    // Check for overflow before computing target
    size_t target_index = index + distance;
    if (target_index < index)
    {
        return; // Overflow detected, skip prefetch
    }

    // Additional overflow check for stride multiplication
    size_t offset = target_index * stride;
    if (stride != 0 && offset / stride != target_index)
    {
        return; // Overflow in multiplication
    }

    prefetch<Locality>(base + offset);
}

// =============================================================================
// Cache Flush Operations
// =============================================================================

/**
 * @brief Flush cache line containing address
 * @param addr Address to flush
 */
inline void flush_cache_line(const void* addr) noexcept
{
#if defined(FATP_CACHE_X86)
    _mm_clflush(addr);
#elif defined(FATP_CACHE_ARM) && defined(__aarch64__)
    __asm__ __volatile__("dc cvac, %0" : : "r"(addr) : "memory");
#else
    // No portable way to flush - use compiler memory barrier
    asm volatile("" ::: "memory");
    (void)addr;
#endif
}

/**
 * @brief Flush range of cache lines
 * @param addr Start address
 * @param size Number of bytes to flush (0 is valid, does nothing)
 *
 * @note Flushes all cache lines that overlap with [addr, addr+size)
 */
inline void flush_cache_range(const void* addr, size_t size) noexcept
{
    if (size == 0)
    {
        return;
    }

    constexpr size_t line_size = cache_constants::l1_line_size_v;
    const char* ptr = static_cast<const char*>(addr);
    const char* end = ptr + size;

    // Flush all lines including the one containing the last byte
    for (; ptr < end; ptr += line_size)
    {
        flush_cache_line(ptr);
    }
}

/**
 * @brief Flush and invalidate cache line (stronger than flush)
 * @param addr Address to flush and invalidate
 */
inline void flush_invalidate(const void* addr) noexcept
{
#if defined(FATP_CACHE_X86) && defined(__CLFLUSHOPT__)
    _mm_clflushopt(const_cast<void*>(addr));
#elif defined(FATP_CACHE_X86)
    _mm_clflush(addr);
#elif defined(FATP_CACHE_ARM) && defined(__aarch64__)
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
 * @param dest Destination address
 * @param value Value to store
 *
 * @note Falls back to memcpy if alignment requirements aren't met.
 *       For best performance, ensure dest is aligned to sizeof(T) or better.
 *
 * Alignment requirements for streaming intrinsics:
 * - 4-byte types: 4-byte alignment (natural)
 * - 8-byte types: 8-byte alignment (natural)
 * - 9-16 byte types: 16-byte alignment
 * - 17-32 byte types: 32-byte alignment
 */
template <typename T>
inline void stream_store(T* dest, const T& value) noexcept
{
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");

#if defined(FATP_CACHE_X86)
    if constexpr (sizeof(T) == 4)
    {
        // 4-byte stream - use memcpy to avoid strict-aliasing UB
        if (is_aligned(dest, 4))
        {
            int tmp;
            std::memcpy(&tmp, &value, sizeof(T));
            _mm_stream_si32(reinterpret_cast<int*>(dest), tmp);
        }
        else
        {
            // Fallback: use memcpy for unaligned (safe on all architectures)
            std::memcpy(dest, &value, sizeof(T));
        }
    }
    else if constexpr (sizeof(T) == 8)
    {
// 8-byte stream - use memcpy to avoid strict-aliasing UB
#if defined(FATP_CACHE_X86_64)
        if (is_aligned(dest, 8))
        {
            long long tmp;
            std::memcpy(&tmp, &value, sizeof(T));
            _mm_stream_si64(reinterpret_cast<long long*>(dest), tmp);
        }
        else
        {
            std::memcpy(dest, &value, sizeof(T));
        }
#else
        // x86-32: no _mm_stream_si64, use memcpy
        std::memcpy(dest, &value, sizeof(T));
#endif
    }
    else if constexpr (sizeof(T) <= 16)
    {
        // 16-byte stream
        if (is_aligned(dest, 16))
        {
            __m128i tmp{};
            std::memcpy(&tmp, &value, sizeof(T));
            _mm_stream_si128(reinterpret_cast<__m128i*>(dest), tmp);
        }
        else
        {
            std::memcpy(dest, &value, sizeof(T));
        }
    }
    else if constexpr (sizeof(T) <= 32)
    {
#ifdef __AVX__
        // 32-byte stream
        if (is_aligned(dest, 32))
        {
            __m256i tmp{};
            std::memcpy(&tmp, &value, sizeof(T));
            _mm256_stream_si256(reinterpret_cast<__m256i*>(dest), tmp);
        }
        else
        {
            std::memcpy(dest, &value, sizeof(T));
        }
#else
        std::memcpy(dest, &value, sizeof(T));
#endif
    }
    else
    {
        std::memcpy(dest, &value, sizeof(T));
    }
#else
    // Non-x86: use memcpy (safe and portable)
    std::memcpy(dest, &value, sizeof(T));
#endif
}

/**
 * @brief Non-temporal memcpy (streaming copy)
 * @param dest Destination (best if 32-byte aligned)
 * @param src Source
 * @param size Number of bytes
 *
 * @note Falls back to std::memcpy if dest is not 32-byte aligned.
 *       Always call store_fence() after if you need ordering guarantees.
 */
inline void stream_copy(void* dest, const void* src, size_t size) noexcept
{
    if (size == 0)
    {
        return;
    }

#if defined(FATP_CACHE_X86) && defined(__AVX__)
    // _mm256_stream_si256 requires 32-byte aligned destination
    if (!is_aligned(dest, 32))
    {
        std::memcpy(dest, src, size);
        return;
    }

    constexpr size_t simd_width = 32; // AVX: 256-bit = 32 bytes
    char* d = static_cast<char*>(dest);
    const char* s = static_cast<const char*>(src);

    // Process 32-byte chunks
    size_t chunks = size / simd_width;
    for (size_t i = 0; i < chunks; ++i)
    {
        // Use unaligned load (src may not be aligned)
        __m256i data = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(s));
        // Aligned streaming store (dest is verified aligned)
        _mm256_stream_si256(reinterpret_cast<__m256i*>(d), data);
        d += simd_width;
        s += simd_width;
    }

    // Handle remainder with regular memcpy
    size_t remainder = size % simd_width;
    if (remainder)
    {
        std::memcpy(d, s, remainder);
    }

    _mm_sfence(); // Ensure streaming stores complete before returning
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
 *
 * Places T at the start of a cache line. Multiple CacheAligned<T> objects
 * in an array are guaranteed not to share cache lines.
 */
template <typename T>
struct alignas(cache_constants::destructive_interference_size_v) CacheAligned
{
    T value;

    CacheAligned() = default;

    template <typename... Args>
    explicit CacheAligned(Args&&... args)
        : value(std::forward<Args>(args)...)
    {
    }

    operator T&() noexcept
    {
        return value;
    }
    operator const T&() const noexcept
    {
        return value;
    }

    T& get() noexcept
    {
        return value;
    }
    const T& get() const noexcept
    {
        return value;
    }
};

/**
 * @brief Padded type to occupy exactly one cache line
 * @tparam T Type to pad (must fit within a cache line)
 *
 * Unlike CacheAligned, this struct is padded to exactly cache_line_size bytes,
 * preventing any sharing even with adjacent different types.
 *
 * Uses template specialization for C++17 compatibility (avoids [[no_unique_address]]
 * which is C++20 only).
 */
namespace detail
{

// Primary template: T is smaller than cache line, needs padding
template <typename T, bool IsExactFit = (sizeof(T) == cache_constants::destructive_interference_size_v)>
struct CacheLinePaddedImpl
{
    static constexpr size_t cache_line_size = cache_constants::destructive_interference_size_v;

    static_assert(sizeof(T) < cache_line_size,
                  "CacheLinePadded<T> requires sizeof(T) <= cache line size. "
                  "Use CacheAligned<T> for larger types.");

    T value;
    char mPadding[cache_line_size - sizeof(T)]{}; // Zero-initialize padding

    CacheLinePaddedImpl() = default;

    template <typename... Args>
    explicit CacheLinePaddedImpl(Args&&... args)
        : value(std::forward<Args>(args)...)
        , mPadding{}
    {
    }

    operator T&() noexcept
    {
        return value;
    }
    operator const T&() const noexcept
    {
        return value;
    }

    T& get() noexcept
    {
        return value;
    }
    const T& get() const noexcept
    {
        return value;
    }
};

// Specialization: T is exactly cache line size, no padding needed
template <typename T>
struct CacheLinePaddedImpl<T, true>
{
    static constexpr size_t cache_line_size = cache_constants::destructive_interference_size_v;

    static_assert(sizeof(T) == cache_line_size, "Internal error: exact-fit specialization used for non-exact type");

    T value;

    CacheLinePaddedImpl() = default;

    template <typename... Args>
    explicit CacheLinePaddedImpl(Args&&... args)
        : value(std::forward<Args>(args)...)
    {
    }

    operator T&() noexcept
    {
        return value;
    }
    operator const T&() const noexcept
    {
        return value;
    }

    T& get() noexcept
    {
        return value;
    }
    const T& get() const noexcept
    {
        return value;
    }
};

} // namespace detail

// User-facing alias with proper alignment
// Note: Explicitly declare default constructor for MSVC compatibility
// (using Base::Base doesn't always inherit default ctor correctly on MSVC)
template <typename T>
struct alignas(cache_constants::destructive_interference_size_v) CacheLinePadded : detail::CacheLinePaddedImpl<T>
{
    using Base = detail::CacheLinePaddedImpl<T>;

    CacheLinePadded() = default;

    template <typename... Args>
    explicit CacheLinePadded(Args&&... args)
        : Base(std::forward<Args>(args)...)
    {
    }

    // Inherit accessors from base
    using Base::get;
    using Base::operator T&;
    using Base::operator const T&;
};

// =============================================================================
// Memory Barriers and Fences
// =============================================================================

/**
 * @brief Full memory barrier (serializes all memory operations)
 */
inline void memory_barrier() noexcept
{
#if defined(FATP_CACHE_X86)
    _mm_mfence();
#elif defined(FATP_CACHE_ARM)
    __asm__ __volatile__("dmb sy" : : : "memory");
#else
    std::atomic_thread_fence(std::memory_order_seq_cst);
#endif
}

/**
 * @brief Store fence (ensure all stores before this complete before any after)
 */
inline void store_fence() noexcept
{
#if defined(FATP_CACHE_X86)
    _mm_sfence();
#elif defined(FATP_CACHE_ARM)
    __asm__ __volatile__("dmb st" : : : "memory");
#else
    std::atomic_thread_fence(std::memory_order_release);
#endif
}

/**
 * @brief Load fence (ensure all loads after this see stores completed before)
 */
inline void load_fence() noexcept
{
#if defined(FATP_CACHE_X86)
    _mm_lfence();
#elif defined(FATP_CACHE_ARM)
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
 * @param element_size Size of each element in bytes
 * @param cache_size Target cache size in bytes (default: 32KB L1)
 * @return Optimal block size in elements
 *
 * @note Returns at least 1 even for degenerate inputs.
 *       Uses ~80% of cache to leave room for other data.
 */
inline size_t optimal_block_size(size_t elements, size_t element_size, size_t cache_size = 32 * 1024) noexcept
{
    if (elements == 0 || element_size == 0)
    {
        return 1;
    }

    // Use ~80% of cache to leave room for other data
    size_t usable_cache = cache_size * 4 / 5;
    size_t max_block = usable_cache / element_size;

    if (max_block == 0)
    {
        return 1;
    }

    // Round down to power of 2 for better loop performance
    size_t block_size = 1;
    while (block_size * 2 <= max_block && block_size * 2 <= elements)
    {
        block_size *= 2;
    }

    return block_size;
}

/**
 * @brief 2D matrix blocking iterator
 *
 * Iterates over a matrix in cache-friendly blocks, processing all blocks
 * in row-major order within the block grid.
 *
 * @note Block sizes are clamped to minimum 1 to prevent infinite loops.
 */
struct BlockIterator2D
{
    size_t rows, cols;
    size_t block_rows, block_cols;
    size_t block_i, block_j;

    BlockIterator2D(size_t r, size_t c, size_t br, size_t bc) noexcept
        : rows(r)
        , cols(c)
        , block_rows(std::max(br, size_t{1})) // Prevent zero (infinite loop)
        , block_cols(std::max(bc, size_t{1})) // Prevent zero (infinite loop)
        , block_i(0)
        , block_j(0)
    {
    }

    /**
     * @brief Check if there are more blocks to process
     */
    bool has_next() const noexcept
    {
        return block_i < rows && cols > 0;
    }

    /**
     * @brief Advance to next block
     */
    void next_block() noexcept
    {
        block_j += block_cols;
        if (block_j >= cols)
        {
            block_j = 0;
            block_i += block_rows;
        }
    }

    size_t block_start_i() const noexcept
    {
        return block_i;
    }
    size_t block_start_j() const noexcept
    {
        return block_j;
    }
    size_t block_end_i() const noexcept
    {
        return std::min(block_i + block_rows, rows);
    }
    size_t block_end_j() const noexcept
    {
        return std::min(block_j + block_cols, cols);
    }
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
inline bool is_aligned(const void* ptr, size_t alignment) noexcept
{
    assert((alignment & (alignment - 1)) == 0 && "alignment must be power of 2");
    return (reinterpret_cast<uintptr_t>(ptr) & (alignment - 1)) == 0;
}

/**
 * @brief Align pointer up to boundary
 * @param ptr Pointer to align
 * @param alignment Alignment boundary (must be power of 2)
 * @return Aligned pointer >= ptr
 */
inline void* align_up(void* ptr, size_t alignment) noexcept
{
    assert((alignment & (alignment - 1)) == 0 && "alignment must be power of 2");
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    uintptr_t aligned = (addr + alignment - 1) & ~(alignment - 1);
    return reinterpret_cast<void*>(aligned);
}

/**
 * @brief Align const pointer up to boundary
 */
inline const void* align_up(const void* ptr, size_t alignment) noexcept
{
    return align_up(const_cast<void*>(ptr), alignment);
}

/**
 * @brief Align pointer down to boundary
 * @param ptr Pointer to align
 * @param alignment Alignment boundary (must be power of 2)
 * @return Aligned pointer <= ptr
 */
inline void* align_down(void* ptr, size_t alignment) noexcept
{
    assert((alignment & (alignment - 1)) == 0 && "alignment must be power of 2");
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    uintptr_t aligned = addr & ~(alignment - 1);
    return reinterpret_cast<void*>(aligned);
}

/**
 * @brief Align const pointer down to boundary
 */
inline const void* align_down(const void* ptr, size_t alignment) noexcept
{
    return align_down(const_cast<void*>(ptr), alignment);
}

/**
 * @brief Compute alignment offset
 * @param ptr Pointer
 * @param alignment Alignment boundary (must be power of 2)
 * @return Bytes needed to align ptr up (0 if already aligned)
 */
inline size_t alignment_offset(const void* ptr, size_t alignment) noexcept
{
    assert((alignment & (alignment - 1)) == 0 && "alignment must be power of 2");
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    uintptr_t misalignment = addr & (alignment - 1);
    return misalignment ? alignment - misalignment : 0;
}

} // namespace perf
} // namespace fat_p
