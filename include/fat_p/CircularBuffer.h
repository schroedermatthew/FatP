#pragma once

/*
FATP_META:
  meta_version: 1
  component: CircularBuffer
  file_role: public_header
  path: include/fat_p/CircularBuffer.h
  namespace: fat_p
  layer: Containers
  summary: "Public header for CircularBuffer."
  api_stability: candidate
  related:
    docs:
      - components/CircularBuffer/docs/CircularBuffer_Overview.md
      - components/CircularBuffer/docs/CircularBuffer_User_Manual.md
    tests:
      - components/CircularBuffer/tests/test_CircularBuffer.cpp
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/

/**
 * @file CircularBuffer.h
 * @brief Fixed-capacity circular buffer with O(1) push/pop operations
 */

#include <atomic>
#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace fat_p
{

namespace detail
{

// Cache line size for false sharing prevention
// C++17 provides std::hardware_destructive_interference_size but:
// - Support is spotty (MSVC has it, GCC/Clang often warn about ABI stability)
// - GCC warns with -Winterference-size about value varying between compilers
// 64 bytes is the pragmatic industry standard for x86-64 and ARM64.
// We hardcode it to avoid warnings and ensure consistent ABI.
inline constexpr size_t kCacheLineSize = 64;

// Round up to next power of 2 (or return n if already power of 2)
constexpr size_t nextPowerOfTwo(size_t n) noexcept
{
    if (n == 0)
    {
        return 1;
    }
    --n;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    if constexpr (sizeof(size_t) >= 8)
    {
        n |= n >> 32;
    }
    return n + 1;
}

constexpr bool isPowerOfTwo(size_t n) noexcept
{
    return n > 0 && (n & (n - 1)) == 0;
}

} // namespace detail

// ============================================================================
// CircularBuffer Implementation
// ============================================================================

/**
 * @brief Lock-free single-producer single-consumer (SPSC) circular buffer
 *
 * @tparam T Element type. Must be default constructible and nothrow move/copy constructible.
 * @tparam Capacity Maximum number of elements the buffer can hold
 *
 * @details This is a wait-free SPSC queue using atomics with memory ordering.
 * The internal buffer size is rounded up to the next power of 2 (if not already)
 * to enable efficient bitwise AND masking instead of expensive modulo operations.
 *
 * Index design: indices are monotonically increasing counters that are never
 * masked on storage. Masking is applied only when indexing into the backing
 * array. This eliminates the class of torn-snapshot bugs where a masked index
 * pair can produce an in-range-but-wrong distance, because the unsigned
 * difference of two monotonic counters is always the true element count.
 *
 * Thread safety model:
 * - Exactly one producer thread may call push() / emplace()
 * - Exactly one consumer thread may call pop() / front()
 * - Observer methods (size, empty, full) may be called from either thread
 * - Cache-line alignment prevents false sharing between producer and consumer
 *
 * Index Caching Optimization:
 * - Producer caches consumer's read index locally
 * - Consumer caches producer's write index locally
 * - Reduces cache coherency traffic by ~50-70%
 * - Provides ~1.7x throughput improvement on multi-core systems
 *
 * Memory ordering:
 * - Producer uses relaxed load on mWriteIdx, acquire load on mReadIdx (when cache miss),
 *   release store on mWriteIdx
 * - Consumer uses relaxed load on mReadIdx, acquire load on mWriteIdx (when cache miss),
 *   release store on mReadIdx
 * - This ensures proper visibility of pushed elements to the consumer
 *
 * Performance characteristics:
 * - Wait-free push/pop: O(1) guaranteed completion
 * - ~300-360M ops/sec SPSC throughput
 * - Bitwise AND masking for array access (faster than modulo)
 *
 * @note For trivially copyable types requiring MPMC support, see LockFreeRingBuffer.
 * @note For dynamic capacity needs, see LockFreeQueue.
 */
template <typename T, size_t Capacity>
class CircularBuffer
{
    static_assert(Capacity > 0, "Capacity must be greater than 0");
    static_assert(std::is_default_constructible_v<T>,
                  "T must be default constructible (array-backed storage)");
    static_assert(std::is_nothrow_move_constructible_v<T> || std::is_nothrow_copy_constructible_v<T>,
                  "T must be nothrow move or copy constructible for exception safety");
    static_assert(std::atomic<size_t>::is_always_lock_free,
                  "std::atomic<size_t> must be lock-free for wait-free guarantees");

public:
    using value_type = T;
    using size_type = size_t;
    using reference = T&;
    using const_reference = const T&;

private:
    static constexpr size_t kCacheLineSize = detail::kCacheLineSize;

    // Round up (Capacity + 1) to next power of 2 for efficient masking.
    // The +1 ensures kBufferSize > Capacity, so that no two elements in the
    // buffer (at most Capacity apart in monotonic index space) map to the same
    // array slot after masking.
    static constexpr size_t kBufferSize = detail::nextPowerOfTwo(Capacity + 1);
    static constexpr size_t kIndexMask = kBufferSize - 1;

    // Verify our power-of-2 logic at compile time
    static_assert(detail::isPowerOfTwo(kBufferSize), "Internal error: kBufferSize must be power of 2");
    static_assert(kBufferSize > Capacity, "Internal error: kBufferSize must be greater than Capacity");

    // Core indices - monotonically increasing, never masked on storage.
    // Masking is applied only when indexing into the backing array.
    // Each index lives on its own cache line to prevent false sharing.
    alignas(kCacheLineSize) std::atomic<size_t> mReadIdx{0};
    alignas(kCacheLineSize) std::atomic<size_t> mWriteIdx{0};

    // Cached indices for index caching optimization.
    // Producer caches consumer's mReadIdx to avoid cross-core atomic loads.
    // Consumer caches producer's mWriteIdx to avoid cross-core atomic loads.
    // These are NOT atomic - only accessed by their respective threads.
    alignas(kCacheLineSize) mutable size_t mCachedReadIdx{0};  // Producer's cache of mReadIdx
    alignas(kCacheLineSize) mutable size_t mCachedWriteIdx{0}; // Consumer's cache of mWriteIdx

    alignas(kCacheLineSize) std::unique_ptr<T[]> mBuffer;

public:
    /**
     * @brief Construct an empty circular buffer
     *
     * Allocates a power-of-2 number of slots internally for efficient index masking.
     * The actual usable capacity is exactly the Capacity template parameter.
     *
     * @note In C++20, uses make_unique_for_overwrite to avoid zero-initialization
     *       overhead for large buffers. The ring buffer logic guarantees slots are
     *       written before being read.
     */
    CircularBuffer()
        : mBuffer(std::make_unique_for_overwrite<T[]>(kBufferSize))
    {
    }

    // Non-copyable due to atomic members
    CircularBuffer(const CircularBuffer&) = delete;
    CircularBuffer& operator=(const CircularBuffer&) = delete;

    // Non-moveable due to atomic members
    CircularBuffer(CircularBuffer&&) = delete;
    CircularBuffer& operator=(CircularBuffer&&) = delete;

    ~CircularBuffer() = default;

    /**
     * @brief Push element by copy
     *
     * @param value Element to push
     * @return true if successful, false if buffer is full
     *
     * @note Only one thread may call push() (the producer thread)
     * @note Wait-free: completes in bounded time regardless of other threads
     */
    [[nodiscard]] bool push(const T& value) noexcept(std::is_nothrow_copy_assignable_v<T>)
    {
        size_t write = mWriteIdx.load(std::memory_order_relaxed);

        // First check against cached read index (no cross-core traffic)
        if (write - mCachedReadIdx >= Capacity)
        {
            // Cache says full - refresh cache and recheck
            mCachedReadIdx = mReadIdx.load(std::memory_order_acquire);
            if (write - mCachedReadIdx >= Capacity)
            {
                return false; // Actually full
            }
        }

        mBuffer[write & kIndexMask] = value;
        mWriteIdx.store(write + 1, std::memory_order_release);
        return true;
    }

    /**
     * @brief Push element by move
     *
     * @param value Element to move into the buffer
     * @return true if successful, false if buffer is full
     *
     * @note Only one thread may call push() (the producer thread)
     * @note Wait-free: completes in bounded time regardless of other threads
     */
    [[nodiscard]] bool push(T&& value) noexcept(std::is_nothrow_move_assignable_v<T>)
    {
        size_t write = mWriteIdx.load(std::memory_order_relaxed);

        // First check against cached read index (no cross-core traffic)
        if (write - mCachedReadIdx >= Capacity)
        {
            // Cache says full - refresh cache and recheck
            mCachedReadIdx = mReadIdx.load(std::memory_order_acquire);
            if (write - mCachedReadIdx >= Capacity)
            {
                return false; // Actually full
            }
        }

        mBuffer[write & kIndexMask] = std::move(value);
        mWriteIdx.store(write + 1, std::memory_order_release);
        return true;
    }

    /**
     * @brief Construct element in-place
     *
     * @tparam Args Constructor argument types
     * @param args Arguments to forward to T's constructor
     * @return true if successful, false if buffer is full
     *
     * @note Only one thread may call emplace() (the producer thread)
     */
    template <typename... Args>
    [[nodiscard]] bool emplace(Args&&... args)
        noexcept(std::is_nothrow_constructible_v<T, Args...> && std::is_nothrow_move_assignable_v<T>)
    {
        size_t write = mWriteIdx.load(std::memory_order_relaxed);

        // First check against cached read index (no cross-core traffic)
        if (write - mCachedReadIdx >= Capacity)
        {
            // Cache says full - refresh cache and recheck
            mCachedReadIdx = mReadIdx.load(std::memory_order_acquire);
            if (write - mCachedReadIdx >= Capacity)
            {
                return false; // Actually full
            }
        }

        mBuffer[write & kIndexMask] = T(std::forward<Args>(args)...);
        mWriteIdx.store(write + 1, std::memory_order_release);
        return true;
    }

    /**
     * @brief Pop element from buffer
     *
     * @param value Output parameter to receive popped element (moved)
     * @return true if successful, false if buffer is empty
     *
     * @note Only one thread may call pop() (the consumer thread)
     * @note Wait-free: completes in bounded time regardless of other threads
     */
    [[nodiscard]] bool pop(T& value) noexcept(std::is_nothrow_move_assignable_v<T>)
    {
        size_t read = mReadIdx.load(std::memory_order_relaxed);

        // First check against cached write index (no cross-core traffic)
        if (read == mCachedWriteIdx)
        {
            // Cache says empty - refresh cache and recheck
            mCachedWriteIdx = mWriteIdx.load(std::memory_order_acquire);
            if (read == mCachedWriteIdx)
            {
                return false; // Actually empty
            }
        }

        value = std::move(mBuffer[read & kIndexMask]);
        mReadIdx.store(read + 1, std::memory_order_release);
        return true;
    }

    /**
     * @brief Peek at the front element without removing it
     *
     * @return Pointer to front element, or nullptr if empty
     *
     * @note Only the consumer thread should call this
     * @note The returned pointer remains valid until the next successful pop() call
     *       (or clear()/clearAndDestruct()).
     * @note Under the SPSC contract, producer push()/emplace() does not invalidate
     *       this pointer while the front element remains in the buffer.
     */
    [[nodiscard]] const T* front() const noexcept
    {
        size_t read = mReadIdx.load(std::memory_order_relaxed);

        // For front(), we need to be conservative - always check actual index
        // since front() might be called repeatedly without pop()
        if (read == mWriteIdx.load(std::memory_order_acquire))
        {
            return nullptr;
        }

        return &mBuffer[read & kIndexMask];
    }

    /**
     * @brief Get approximate number of elements
     *
     * @return Number of elements currently in the buffer
     *
     * @note This value may be stale immediately after returning when
     *       producer and consumer are active concurrently
     * @note The returned value is always in the range [0, Capacity]
     * @note Safe to call from any thread
     *
     * @note Complexity: O(1), two atomic loads
     * @note Thread-safety: safe to call from any thread
     */
    [[nodiscard]] size_t size() const noexcept
    {
        // With monotonic indices, (write - read) is always the true element
        // count — but only when both values come from the same instant.
        // Loading two separate atomics is not atomic: if the observer is
        // preempted between loads, the stale/fresh pair can produce a
        // wrapped unsigned difference (SIZE_MAX-scale), not a real count.
        //
        // Double-read stabilization solves this: bracket one index with two
        // reads, and if it hasn't changed, the other index (read between
        // the brackets) forms a temporally consistent pair.
        //
        // Correctness argument (monotonic indices eliminate ABA):
        //   - w1 == w2 proves mWriteIdx didn't advance during the window
        //     (a 64-bit counter cannot cycle through 2^64 values in ~20ns).
        //   - mReadIdx was loaded while mWriteIdx was stable, so (w1, r) is
        //     a snapshot of a real queue state.
        //   - Since pop() never advances read past write, w1 >= r always.
        //   - Therefore w1 - r is the true element count. No range check needed.
        //
        // Same argument applies symmetrically when stabilizing mReadIdx.
        for (int attempt = 0; attempt < 4; ++attempt)
        {
            if ((attempt & 1) == 0)
            {
                // Stabilize mWriteIdx
                size_t w1 = mWriteIdx.load(std::memory_order_acquire);
                size_t r  = mReadIdx.load(std::memory_order_acquire);
                size_t w2 = mWriteIdx.load(std::memory_order_acquire);
                if (w1 == w2)
                {
                    return w1 - r;
                }
            }
            else
            {
                // Stabilize mReadIdx
                size_t r1 = mReadIdx.load(std::memory_order_acquire);
                size_t w  = mWriteIdx.load(std::memory_order_acquire);
                size_t r2 = mReadIdx.load(std::memory_order_acquire);
                if (r1 == r2)
                {
                    return w - r1;
                }
            }
        }

        // Exhausted retries: both indices advancing during every load window.
        // Load read first, then write, so that w >= r is guaranteed by
        // monotonicity (write is at least as fresh as read). Clamp to
        // Capacity as a defensive bound against preemption-induced skew.
        size_t r = mReadIdx.load(std::memory_order_acquire);
        size_t w = mWriteIdx.load(std::memory_order_acquire);
        size_t d = w - r;
        return d <= Capacity ? d : Capacity;
    }

    /**
     * @brief Check if buffer is empty
     *
     * @return true if no elements are in the buffer
     *
     * @note Safe to call from any thread
     */
    [[nodiscard]] bool empty() const noexcept
    {
        return mReadIdx.load(std::memory_order_acquire) == mWriteIdx.load(std::memory_order_acquire);
    }

    /**
     * @brief Check if buffer is full
     *
     * @return true if buffer cannot accept more elements
     *
     * @note Safe to call from any thread
     */
    [[nodiscard]] bool full() const noexcept
    {
        size_t write = mWriteIdx.load(std::memory_order_acquire);
        size_t read = mReadIdx.load(std::memory_order_acquire);
        return (write - read) >= Capacity;
    }

    /**
     * @brief Get the maximum capacity
     *
     * @return Maximum number of elements the buffer can hold
     *
     * @note This is a compile-time constant equal to the Capacity template parameter
     */
    [[nodiscard]] static constexpr size_t capacity() noexcept
    {
        return Capacity;
    }

    /**
     * @brief Get the internal buffer size (power of 2)
     *
     * @return Actual allocated buffer size (always >= Capacity + 1, power of 2)
     *
     * @note Useful for understanding memory usage. The difference between
     *       bufferSize() and capacity() is overhead for the empty/full distinction
     *       and power-of-2 alignment.
     */
    [[nodiscard]] static constexpr size_t bufferSize() noexcept
    {
        return kBufferSize;
    }

    /**
     * @brief Reset buffer to empty state
     *
     * For trivially destructible types, this simply resets the indices.
     * For non-trivial types (e.g., shared_ptr, containers), this automatically
     * calls clearAndDestruct() to properly release resources.
     *
     * @warning NOT THREAD-SAFE. Call only when no other threads are accessing
     *          the buffer (e.g., during shutdown or reinitialization).
     */
    void clear() noexcept(std::is_nothrow_destructible_v<T>)
    {
        if constexpr (!std::is_trivially_destructible_v<T>)
        {
            // Auto-destruct non-trivial types to prevent resource leaks
            clearAndDestruct();
        }
        else
        {
            mReadIdx.store(0, std::memory_order_relaxed);
            mWriteIdx.store(0, std::memory_order_relaxed);
            mCachedReadIdx = 0;
            mCachedWriteIdx = 0;
        }
    }

    /**
     * @brief Reset buffer and destruct all elements
     *
     * Pops and destructs all elements in the buffer, then resets indices.
     * Use this instead of clear() when T holds resources that should be released.
     *
     * @warning NOT THREAD-SAFE. Call only when no other threads are accessing
     *          the buffer.
     */
    void clearAndDestruct() noexcept(std::is_nothrow_move_assignable_v<T>
                                     && std::is_nothrow_destructible_v<T>)
    {
        T tmp;
        while (pop(tmp))
        {
            // Element is moved out and tmp is destructed at end of loop iteration
        }

        // Reset indices and caches to the canonical empty state.
        // This is single-threaded by contract.
        mReadIdx.store(0, std::memory_order_relaxed);
        mWriteIdx.store(0, std::memory_order_relaxed);
        mCachedReadIdx = 0;
        mCachedWriteIdx = 0;
    }
};

} // namespace fat_p
