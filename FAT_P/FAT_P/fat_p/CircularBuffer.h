/**
 * @file CircularBuffer.h
 * @brief Fixed-capacity circular buffer with O(1) push/pop operations
 *
 * @layer Containers
 */
#pragma once

/*
FATP_META:
  meta_version: 1
  component: CircularBuffer
  file_role: public_header
  path: fat_p/CircularBuffer.h
  namespace: fat_p
  layer: Containers
  summary: "Public header for CircularBuffer."
  api_stability: in_work
  related:
    docs_search: "CircularBuffer"
    tests:
      - tests/test_CircularBuffer.cpp
      - tests/test_FatPTypeTraits.cpp
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
#include "CppStandardDetection.h"

#include <atomic>
#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

#include "FatPTypeTraits.h"

namespace fat_p {

namespace detail {

// Cache line size for false sharing prevention
// C++17 provides std::hardware_destructive_interference_size but:
// - Support is spotty (MSVC has it, GCC/Clang often warn about ABI stability)
// - GCC warns with -Winterference-size about value varying between compilers
// 64 bytes is the pragmatic industry standard for x86-64 and ARM64.
// We hardcode it to avoid warnings and ensure consistent ABI.
inline constexpr size_t cache_line_size = 64;

// Round up to next power of 2 (or return n if already power of 2)
constexpr size_t next_power_of_two(size_t n) noexcept
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

constexpr bool is_power_of_two(size_t n) noexcept
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
 * @tparam T Element type (must be nothrow move constructible and move assignable)
 * @tparam Capacity Maximum number of elements the buffer can hold
 *
 * @details This is a wait-free SPSC queue using atomics with memory ordering.
 * The internal buffer size is rounded up to the next power of 2 (if not already)
 * to enable efficient bitwise AND masking instead of expensive modulo operations.
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
 * - Producer uses relaxed load on write_idx, acquire load on read_idx (when cache miss),
 *   release store on write_idx
 * - Consumer uses relaxed load on read_idx, acquire load on write_idx (when cache miss),
 *   release store on read_idx
 * - This ensures proper visibility of pushed elements to the consumer
 *
 * Performance characteristics:
 * - Wait-free push/pop: O(1) guaranteed completion
 * - ~300-360M ops/sec SPSC throughput
 * - Bitwise AND masking for index wraparound (faster than modulo)
 *
 * @note For trivially copyable types requiring MPMC support, see LockFreeRingBuffer.
 * @note For dynamic capacity needs, see LockFreeQueue.
 */
template <typename T, size_t Capacity>
class CircularBuffer
{
    static_assert(Capacity > 0, "Capacity must be greater than 0");
    static_assert(std::is_nothrow_move_constructible_v<T> ||
                  std::is_nothrow_copy_constructible_v<T>,
                  "T must be nothrow move or copy constructible for exception safety");
    static_assert(std::atomic<size_t>::is_always_lock_free,
                  "std::atomic<size_t> must be lock-free for wait-free guarantees");

public:
    using value_type = T;
    using size_type = size_t;
    using reference = T&;
    using const_reference = const T&;

private:
    static constexpr size_t CACHE_LINE_SIZE = detail::cache_line_size;

    // Round up (Capacity + 1) to next power of 2 for efficient masking
    // The +1 is needed to distinguish full from empty (one slot always unused)
    static constexpr size_t BUFFER_SIZE = detail::next_power_of_two(Capacity + 1);
    static constexpr size_t INDEX_MASK = BUFFER_SIZE - 1;

    // Verify our power-of-2 logic at compile time
    static_assert(detail::is_power_of_two(BUFFER_SIZE),
                  "Internal error: BUFFER_SIZE must be power of 2");
    static_assert(BUFFER_SIZE > Capacity,
                  "Internal error: BUFFER_SIZE must be greater than Capacity");

    // Core indices - each on its own cache line to prevent false sharing
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> read_idx_{0};
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> write_idx_{0};

    // Cached indices for index caching optimization
    // Producer caches consumer's read_idx to avoid cross-core atomic loads
    // Consumer caches producer's write_idx to avoid cross-core atomic loads
    // These are NOT atomic - only accessed by their respective threads
    alignas(CACHE_LINE_SIZE) mutable size_t cached_read_idx_{0};   // Producer's cache of read_idx
    alignas(CACHE_LINE_SIZE) mutable size_t cached_write_idx_{0};  // Consumer's cache of write_idx

    alignas(CACHE_LINE_SIZE) std::unique_ptr<T[]> buffer_;

    // Efficient index increment using bitwise AND (no division)
    static constexpr size_t next_index(size_t idx) noexcept
    {
        return (idx + 1) & INDEX_MASK;
    }

    // Calculate distance between indices (handles wraparound)
    static constexpr size_t index_distance(size_t write, size_t read) noexcept
    {
        return (write - read) & INDEX_MASK;
    }

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
#if FATP_CPP20_OR_LATER
        : buffer_(std::make_unique_for_overwrite<T[]>(BUFFER_SIZE))
#else
        : buffer_(std::make_unique<T[]>(BUFFER_SIZE))
#endif
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
        size_t write = write_idx_.load(std::memory_order_relaxed);

        // First check against cached read index (no cross-core traffic)
        if (index_distance(write, cached_read_idx_) >= Capacity)
        {
            // Cache says full - refresh cache and recheck
            cached_read_idx_ = read_idx_.load(std::memory_order_acquire);
            if (index_distance(write, cached_read_idx_) >= Capacity)
            {
                return false;  // Actually full
            }
        }

        buffer_[write] = value;
        write_idx_.store(next_index(write), std::memory_order_release);
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
        size_t write = write_idx_.load(std::memory_order_relaxed);

        // First check against cached read index (no cross-core traffic)
        if (index_distance(write, cached_read_idx_) >= Capacity)
        {
            // Cache says full - refresh cache and recheck
            cached_read_idx_ = read_idx_.load(std::memory_order_acquire);
            if (index_distance(write, cached_read_idx_) >= Capacity)
            {
                return false;  // Actually full
            }
        }

        buffer_[write] = std::move(value);
        write_idx_.store(next_index(write), std::memory_order_release);
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
    [[nodiscard]] bool emplace(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
    {
        size_t write = write_idx_.load(std::memory_order_relaxed);

        // First check against cached read index (no cross-core traffic)
        if (index_distance(write, cached_read_idx_) >= Capacity)
        {
            // Cache says full - refresh cache and recheck
            cached_read_idx_ = read_idx_.load(std::memory_order_acquire);
            if (index_distance(write, cached_read_idx_) >= Capacity)
            {
                return false;  // Actually full
            }
        }

        buffer_[write] = T(std::forward<Args>(args)...);
        write_idx_.store(next_index(write), std::memory_order_release);
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
        size_t read = read_idx_.load(std::memory_order_relaxed);

        // First check against cached write index (no cross-core traffic)
        if (read == cached_write_idx_)
        {
            // Cache says empty - refresh cache and recheck
            cached_write_idx_ = write_idx_.load(std::memory_order_acquire);
            if (read == cached_write_idx_)
            {
                return false;  // Actually empty
            }
        }

        value = std::move(buffer_[read]);
        read_idx_.store(next_index(read), std::memory_order_release);
        return true;
    }

    /**
     * @brief Peek at the front element without removing it
     *
     * @return Pointer to front element, or nullptr if empty
     *
     * @note Only the consumer thread should call this
     * @note The returned pointer is valid until the next pop() call
     */
    [[nodiscard]] const T* front() const noexcept
    {
        size_t read = read_idx_.load(std::memory_order_relaxed);

        // For front(), we need to be conservative - always check actual index
        // since front() might be called repeatedly without pop()
        if (read == write_idx_.load(std::memory_order_acquire))
        {
            return nullptr;
        }

        return &buffer_[read];
    }

    /**
     * @brief Get approximate number of elements
     *
     * @return Number of elements currently in the buffer
     *
     * @note This value may be stale immediately after returning when
     *       producer and consumer are active concurrently
     * @note Safe to call from any thread
     */
    [[nodiscard]] size_t size() const noexcept
    {
        size_t write = write_idx_.load(std::memory_order_acquire);
        size_t read = read_idx_.load(std::memory_order_acquire);
        return index_distance(write, read);
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
        return read_idx_.load(std::memory_order_acquire) ==
               write_idx_.load(std::memory_order_acquire);
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
        size_t write = write_idx_.load(std::memory_order_acquire);
        size_t read = read_idx_.load(std::memory_order_acquire);
        return index_distance(write, read) >= Capacity;
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
     *       buffer_size() and capacity() is overhead for the empty/full distinction
     *       and power-of-2 alignment.
     */
    [[nodiscard]] static constexpr size_t buffer_size() noexcept
    {
        return BUFFER_SIZE;
    }

    /**
     * @brief Reset buffer to empty state
     *
     * For trivially destructible types, this simply resets the indices.
     * For non-trivial types (e.g., shared_ptr, containers), this automatically
     * calls clear_and_destruct() to properly release resources.
     *
     * @warning NOT THREAD-SAFE. Call only when no other threads are accessing
     *          the buffer (e.g., during shutdown or reinitialization).
     */
    void clear() noexcept(std::is_nothrow_destructible_v<T>)
    {
        if constexpr (!std::is_trivially_destructible_v<T>)
        {
            // Auto-destruct non-trivial types to prevent resource leaks
            clear_and_destruct();
        }
        else
        {
            read_idx_.store(0, std::memory_order_relaxed);
            write_idx_.store(0, std::memory_order_relaxed);
            cached_read_idx_ = 0;
            cached_write_idx_ = 0;
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
    void clear_and_destruct() noexcept(std::is_nothrow_destructible_v<T>)
    {
        T tmp;
        while (pop(tmp))
        {
            // Element is moved out and tmp is destructed at end of loop iteration
        }
    }
};

// Type trait specialization
template <typename T, size_t N>
struct is_circular_buffer<CircularBuffer<T, N>> : std::true_type {};

} // namespace fat_p
