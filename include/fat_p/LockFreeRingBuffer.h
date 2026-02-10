#pragma once

/*
FATP_META:
  meta_version: 1
  component: LockFreeRingBuffer
  file_role: public_header
  path: include/fat_p/LockFreeRingBuffer.h
  namespace: fat_p
  layer: Concurrency
  summary: "Lock-free ring buffers for SPSC and MPMC scenarios."
  api_stability: stable
  related:
    docs_search: "LockFreeRingBuffer"
    tests:
      - components/LockFreeContainers/tests/test_LockFreeRingBuffer.cpp
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
 * @file LockFreeRingBuffer.h
 * @brief Lock-free ring buffers for SPSC and MPMC scenarios
 *
 * @details Provides two lock-free ring buffer implementations:
 *
 * - LockFreeRingBuffer<T>: Wait-free SPSC (single-producer single-consumer)
 *   Optimal for audio/video pipelines, logging, and inter-thread communication.
 *
 * - LockFreeRingBufferMPMC<T>: Lock-free MPMC (multi-producer multi-consumer)
 *   Uses per-slot sequence numbers for correctness under concurrent access.
 *
 * Thread-safety:
 * - SPSC: Exactly ONE producer thread and ONE consumer thread
 * - MPMC: Any number of producers and consumers
 *
 * @section usage Usage Example
 * @code
 * // SPSC usage
 * LockFreeRingBuffer<int> spsc(1024);
 * spsc.push(42);              // Producer thread only
 * auto val = spsc.pop();      // Consumer thread only
 *
 * // MPMC usage
 * LockFreeRingBufferMPMC<int> mpmc(1024);
 * mpmc.push(42);              // Any thread
 * auto val = mpmc.pop();      // Any thread
 * @endcode
 */

#include "FatPConfig.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <new>
#include <optional>
#include <type_traits>
#include <utility>

#ifdef _WIN32
#include <malloc.h>
#endif

namespace fat_p
{

// ============================================================================
// Internal Utilities
// ============================================================================

namespace lockfree_ringbuffer_detail
{

/**
 * @brief Round up to next power of two
 * @note Returns SIZE_MAX/2 + 1 (largest power of 2) if input would overflow
 */
inline constexpr size_t roundUpPowerOfTwo(size_t n) noexcept
{
    if (n == 0)
    {
        return 1;
    }

    // Guard against overflow: if n is already greater than max power of 2,
    // return the largest representable power of 2
    constexpr size_t kMaxPowerOf2 = static_cast<size_t>(1) << (sizeof(size_t) * 8 - 1);
    if (n > kMaxPowerOf2)
    {
        return kMaxPowerOf2;
    }

    --n;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    if constexpr (sizeof(size_t) == 8)
    {
        n |= n >> 32;
    }
    return n + 1;
}

/**
 * @brief Allocate cache-line aligned memory
 * @param count Number of elements to allocate
 * @throws std::bad_alloc on allocation failure or size overflow
 */
template <typename T>
[[nodiscard]] T* allocateAligned(size_t count)
{
    // Overflow check: ensure count * sizeof(T) doesn't wrap
    if (sizeof(T) > 0)
    {
        constexpr size_t kMaxCount = std::numeric_limits<size_t>::max() / sizeof(T);
        if (count > kMaxCount)
        {
            throw std::bad_alloc();
        }
    }

    void* ptr = nullptr;
    size_t bytes = count * sizeof(T);

#ifdef _WIN32
    ptr = _aligned_malloc(bytes, FATP_CACHE_LINE_SIZE);
    if (!ptr)
    {
        throw std::bad_alloc();
    }
#else
    if (posix_memalign(&ptr, FATP_CACHE_LINE_SIZE, bytes) != 0)
    {
        throw std::bad_alloc();
    }
#endif

    return static_cast<T*>(ptr);
}

/**
 * @brief Free cache-line aligned memory
 */
inline void freeAligned(void* ptr) noexcept
{
    if (ptr)
    {
#ifdef _WIN32
        _aligned_free(ptr);
#else
        std::free(ptr);
#endif
    }
}

} // namespace lockfree_ringbuffer_detail

// ============================================================================
// LockFreeRingBuffer - Single Producer Single Consumer (SPSC)
// ============================================================================

/**
 * @brief Wait-free ring buffer for single-producer single-consumer scenarios
 *
 * @tparam T Element type (must be trivially copyable)
 *
 * Thread-safety: Exactly ONE producer thread and ONE consumer thread.
 * Violating this constraint causes undefined behavior.
 *
 * Operations:
 * - push(): Producer thread only, O(1), wait-free
 * - pop(): Consumer thread only, O(1), wait-free
 * - peek(): Consumer thread only, O(1), wait-free
 * - empty(): Consumer thread (producer thread use is racy but benign)
 * - full(): Producer thread (consumer thread use is racy but benign)
 * - size(): Either thread (approximate, may be stale)
 */
template <typename T>
class LockFreeRingBuffer
{
    static_assert(std::is_trivially_copyable_v<T>,
                  "T must be trivially copyable for lock-free operations");
    static_assert(std::atomic<size_t>::is_always_lock_free,
                  "size_t atomics must be lock-free for this implementation");

public:
    using value_type = T;
    using size_type = size_t;

    /**
     * @brief Construct ring buffer with specified capacity
     * @param capacity Requested capacity (rounded up to power of 2)
     * @throws std::bad_alloc if allocation fails
     */
    explicit LockFreeRingBuffer(size_t capacity)
        : mCapacity(lockfree_ringbuffer_detail::roundUpPowerOfTwo(capacity))
        , mMask(mCapacity - 1)
        , mBuffer(lockfree_ringbuffer_detail::allocateAligned<T>(mCapacity))
        , mWritePos(0)
        , mReadPos(0)
    {
        // C++20 implicit-lifetime rules: trivially copyable types (enforced by
        // static_assert above) are implicitly created in raw allocated storage.
        // No placement-new needed.
    }

    ~LockFreeRingBuffer()
    {
        lockfree_ringbuffer_detail::freeAligned(mBuffer);
    }

    // Non-copyable
    LockFreeRingBuffer(const LockFreeRingBuffer&) = delete;
    LockFreeRingBuffer& operator=(const LockFreeRingBuffer&) = delete;

    // Non-moveable (contains atomic members)
    LockFreeRingBuffer(LockFreeRingBuffer&&) = delete;
    LockFreeRingBuffer& operator=(LockFreeRingBuffer&&) = delete;

    /**
     * @brief Push element (producer thread only)
     * @param value Element to push
     * @return true if pushed, false if buffer is full
     *
     * @note Uses cached read position for fast-path to avoid cross-cache-line
     *       reads on every operation. Only refreshes cache when buffer appears full.
     */
    [[nodiscard]] bool push(const T& value) noexcept
    {
        size_t write = mWritePos.load(std::memory_order_relaxed);

        // Fast path: check against cached read position (no cross-cache-line read)
        if (isFull(write, mCachedRead))
        {
            // Slow path: refresh cache from actual consumer position
            mCachedRead = mReadPos.load(std::memory_order_acquire);
            if (isFull(write, mCachedRead))
            {
                return false;
            }
        }

        mBuffer[write & mMask] = value;
        mWritePos.store(write + 1, std::memory_order_release);

        return true;
    }

    /**
     * @brief Push element (producer thread only, move version)
     * @param value Element to push
     * @return true if pushed, false if buffer is full
     */
    [[nodiscard]] bool push(T&& value) noexcept
    {
        size_t write = mWritePos.load(std::memory_order_relaxed);

        // Fast path: check against cached read position (no cross-cache-line read)
        if (isFull(write, mCachedRead))
        {
            // Slow path: refresh cache from actual consumer position
            mCachedRead = mReadPos.load(std::memory_order_acquire);
            if (isFull(write, mCachedRead))
            {
                return false;
            }
        }

        mBuffer[write & mMask] = std::move(value);
        mWritePos.store(write + 1, std::memory_order_release);

        return true;
    }

    /**
     * @brief Pop element (consumer thread only)
     * @return Element if available, std::nullopt if buffer is empty
     *
     * @note Uses cached write position for fast-path to avoid cross-cache-line
     *       reads on every operation. Only refreshes cache when buffer appears empty.
     */
    [[nodiscard]] std::optional<T> pop() noexcept
    {
        size_t read = mReadPos.load(std::memory_order_relaxed);

        // Fast path: check against cached write position (no cross-cache-line read)
        if (isEmpty(mCachedWrite, read))
        {
            // Slow path: refresh cache from actual producer position
            mCachedWrite = mWritePos.load(std::memory_order_acquire);
            if (isEmpty(mCachedWrite, read))
            {
                return std::nullopt;
            }
        }

        T value = mBuffer[read & mMask];
        mReadPos.store(read + 1, std::memory_order_release);

        return value;
    }

    /**
     * @brief Pop element with out-parameter (consumer thread only)
     * @param[out] value Output parameter for popped element
     * @return true if element was popped, false if buffer is empty
     *
     * @note More efficient than optional-returning pop() for trivial types.
     *       Uses cached write position for fast-path.
     */
    [[nodiscard]] bool pop(T& value) noexcept
    {
        size_t read = mReadPos.load(std::memory_order_relaxed);

        // Fast path: check against cached write position (no cross-cache-line read)
        if (isEmpty(mCachedWrite, read))
        {
            // Slow path: refresh cache from actual producer position
            mCachedWrite = mWritePos.load(std::memory_order_acquire);
            if (isEmpty(mCachedWrite, read))
            {
                return false;
            }
        }

        value = mBuffer[read & mMask];
        mReadPos.store(read + 1, std::memory_order_release);

        return true;
    }

    /**
     * @brief Peek at front without removing (consumer thread only)
     * @return Element if available, std::nullopt if buffer is empty
     *
     * @note Uses cached write position for fast-path.
     */
    [[nodiscard]] std::optional<T> peek() const noexcept
    {
        size_t read = mReadPos.load(std::memory_order_relaxed);

        // Fast path: check against cached write position
        if (isEmpty(mCachedWrite, read))
        {
            // Slow path: refresh cache
            mCachedWrite = mWritePos.load(std::memory_order_acquire);
            if (isEmpty(mCachedWrite, read))
            {
                return std::nullopt;
            }
        }

        return mBuffer[read & mMask];
    }

    /**
     * @brief Check if empty
     * @note Best called from consumer thread; producer thread sees stale reads
     */
    [[nodiscard]] bool empty() const noexcept
    {
        size_t write = mWritePos.load(std::memory_order_acquire);
        size_t read = mReadPos.load(std::memory_order_acquire);
        return isEmpty(write, read);
    }

    /**
     * @brief Check if full
     * @note Best called from producer thread; consumer thread sees stale reads
     */
    [[nodiscard]] bool full() const noexcept
    {
        size_t write = mWritePos.load(std::memory_order_acquire);
        size_t read = mReadPos.load(std::memory_order_acquire);
        return isFull(write, read);
    }

    /**
     * @brief Get approximate size (snapshot, may be stale)
     */
    [[nodiscard]] size_t size() const noexcept
    {
        size_t write = mWritePos.load(std::memory_order_acquire);
        size_t read = mReadPos.load(std::memory_order_acquire);
        return write - read;
    }

    /**
     * @brief Get capacity
     */
    [[nodiscard]] size_t capacity() const noexcept
    {
        return mCapacity;
    }

private:
    [[nodiscard]] bool isEmpty(size_t write, size_t read) const noexcept
    {
        return write == read;
    }

    [[nodiscard]] bool isFull(size_t write, size_t read) const noexcept
    {
        return (write - read) >= mCapacity;
    }

    const size_t mCapacity;
    const size_t mMask;
    T* mBuffer;

    // Positions - cache-line aligned to prevent false sharing
    alignas(FATP_CACHE_LINE_SIZE) std::atomic<size_t> mWritePos;
    alignas(FATP_CACHE_LINE_SIZE) std::atomic<size_t> mReadPos;

    // Cached positions for fast-path (avoids cross-cache-line reads)
    // Producer caches consumer's read position
    // Consumer caches producer's write position
    // These are thread-local to their respective threads, so no synchronization needed
    alignas(FATP_CACHE_LINE_SIZE) mutable size_t mCachedRead{0};
    alignas(FATP_CACHE_LINE_SIZE) mutable size_t mCachedWrite{0};
};

// ============================================================================
// LockFreeRingBufferMPMC - Multi Producer Multi Consumer
// ============================================================================

/**
 * @brief Lock-free ring buffer for multi-producer multi-consumer scenarios
 *
 * @tparam T Element type (must be trivially copyable)
 *
 * Thread-safety: Any number of producer and consumer threads.
 *
 * Algorithm: Uses per-slot sequence numbers to ensure data is fully written
 * before being visible to consumers, and fully read before being reusable
 * by producers. This prevents the data races that occur in naive MPMC
 * ring buffer implementations.
 *
 * State machine per slot:
 * - sequence == 2*N*capacity + slot_index: Ready for producer at position N
 * - sequence == 2*N*capacity + slot_index + 1: Ready for consumer at position N
 */
template <typename T>
class LockFreeRingBufferMPMC
{
    static_assert(std::is_trivially_copyable_v<T>,
                  "T must be trivially copyable for lock-free operations");
    static_assert(std::atomic<size_t>::is_always_lock_free,
                  "size_t atomics must be lock-free for this implementation");

public:
    using value_type = T;
    using size_type = size_t;

private:
    struct alignas(FATP_CACHE_LINE_SIZE) Slot
    {
        std::atomic<size_t> sequence;
        T data;
    };

public:
    /**
     * @brief Construct ring buffer with specified capacity
     * @param capacity Requested capacity (rounded up to power of 2)
     * @throws std::bad_alloc if allocation fails
     */
    explicit LockFreeRingBufferMPMC(size_t capacity)
        : mCapacity(lockfree_ringbuffer_detail::roundUpPowerOfTwo(capacity))
        , mMask(mCapacity - 1)
        , mSlots(lockfree_ringbuffer_detail::allocateAligned<Slot>(mCapacity))
        , mEnqueuePos(0)
        , mDequeuePos(0)
    {
        // Construct each Slot using placement-new for proper object lifetime
        // This is required because Slot contains std::atomic which needs construction
        for (size_t i = 0; i < mCapacity; ++i)
        {
            new (&mSlots[i]) Slot();
            mSlots[i].sequence.store(i, std::memory_order_relaxed);
        }
    }

    ~LockFreeRingBufferMPMC()
    {
        // Explicitly destruct each Slot before freeing raw memory
        for (size_t i = 0; i < mCapacity; ++i)
        {
            mSlots[i].~Slot();
        }
        lockfree_ringbuffer_detail::freeAligned(mSlots);
    }

    // Non-copyable, non-moveable
    LockFreeRingBufferMPMC(const LockFreeRingBufferMPMC&) = delete;
    LockFreeRingBufferMPMC& operator=(const LockFreeRingBufferMPMC&) = delete;
    LockFreeRingBufferMPMC(LockFreeRingBufferMPMC&&) = delete;
    LockFreeRingBufferMPMC& operator=(LockFreeRingBufferMPMC&&) = delete;

    /**
     * @brief Push element (thread-safe for multiple producers)
     * @param value Element to push
     * @return true if pushed, false if buffer is full
     */
    [[nodiscard]] bool push(const T& value) noexcept
    {
        size_t pos = mEnqueuePos.load(std::memory_order_relaxed);

        for (;;)
        {
            Slot& slot = mSlots[pos & mMask];
            size_t seq = slot.sequence.load(std::memory_order_acquire);
            auto diff = static_cast<int64_t>(seq) - static_cast<int64_t>(pos);

            if (diff == 0)
            {
                // Slot is ready for this position - try to claim it
                if (mEnqueuePos.compare_exchange_weak(pos,
                                                      pos + 1,
                                                      std::memory_order_relaxed))
                {
                    // Claimed! Write data, then publish
                    slot.data = value;
                    slot.sequence.store(pos + 1, std::memory_order_release);
                    return true;
                }
                // CAS failed, pos updated, retry
            }
            else if (diff < 0)
            {
                // Buffer is full
                return false;
            }
            else
            {
                // Slot not ready yet, reload position
                pos = mEnqueuePos.load(std::memory_order_relaxed);
            }
        }
    }

    /**
     * @brief Pop element (thread-safe for multiple consumers)
     * @return Element if available, std::nullopt if buffer is empty
     */
    [[nodiscard]] std::optional<T> pop() noexcept
    {
        size_t pos = mDequeuePos.load(std::memory_order_relaxed);

        for (;;)
        {
            Slot& slot = mSlots[pos & mMask];
            size_t seq = slot.sequence.load(std::memory_order_acquire);
            auto diff = static_cast<int64_t>(seq) - static_cast<int64_t>(pos + 1);

            if (diff == 0)
            {
                // Slot has data for this position - try to claim it
                if (mDequeuePos.compare_exchange_weak(pos,
                                                      pos + 1,
                                                      std::memory_order_relaxed))
                {
                    // Claimed! Read data, then mark slot as free
                    T value = slot.data;
                    slot.sequence.store(pos + mCapacity, std::memory_order_release);
                    return value;
                }
                // CAS failed, pos updated, retry
            }
            else if (diff < 0)
            {
                // Buffer is empty
                return std::nullopt;
            }
            else
            {
                // Slot not ready yet, reload position
                pos = mDequeuePos.load(std::memory_order_relaxed);
            }
        }
    }

    /**
     * @brief Check if empty (snapshot, may be stale)
     */
    [[nodiscard]] bool empty() const noexcept
    {
        size_t enq = mEnqueuePos.load(std::memory_order_acquire);
        size_t deq = mDequeuePos.load(std::memory_order_acquire);
        return enq == deq;
    }

    /**
     * @brief Check if full (snapshot, may be stale)
     */
    [[nodiscard]] bool full() const noexcept
    {
        size_t enq = mEnqueuePos.load(std::memory_order_acquire);
        size_t deq = mDequeuePos.load(std::memory_order_acquire);
        return (enq - deq) >= mCapacity;
    }

    /**
     * @brief Get approximate size (snapshot, may be stale)
     */
    [[nodiscard]] size_t size() const noexcept
    {
        size_t enq = mEnqueuePos.load(std::memory_order_acquire);
        size_t deq = mDequeuePos.load(std::memory_order_acquire);
        return enq - deq;
    }

    /**
     * @brief Get capacity
     */
    [[nodiscard]] size_t capacity() const noexcept
    {
        return mCapacity;
    }

private:
    const size_t mCapacity;
    const size_t mMask;
    Slot* mSlots;

    alignas(FATP_CACHE_LINE_SIZE) std::atomic<size_t> mEnqueuePos;
    alignas(FATP_CACHE_LINE_SIZE) std::atomic<size_t> mDequeuePos;
};

} // namespace fat_p
