#pragma once

/*
FATP_META:
  meta_version: 1
  component: LockFreeQueue
  file_role: public_header
  path: include/fat_p/LockFreeQueue.h
  namespace: fat_p
  layer: Concurrency
  summary: "Lock-free MPMC queue with sequence-number ABA prevention."
  api_stability: stable
  related:
    docs_search: "LockFreeQueue"
    tests:
      - components/LockFreeContainers/tests/test_LockFreeQueue.cpp
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
 * @file LockFreeQueue.h
 * @brief Lock-free MPMC queue with sequence-number-based ABA prevention
 *
 * @details Lock-free multi-producer multi-consumer queue using atomic operations
 * with per-slot sequence numbers for ABA problem prevention. Provides O(1)
 * amortized enqueue and dequeue operations with strong progress guarantees.
 *
 * Features:
 * - True lock-free MPMC semantics
 * - Fixed capacity (power of 2 for fast modulo)
 * - Wait-free for single producer/consumer
 * - ABA-problem resistant via monotonic sequence numbers
 * - Cache-line aligned to prevent false sharing
 * - Bounded memory usage
 * - Optional statistics tracking (compile-time configurable)
 *
 * Thread-safety: Full MPMC lock-free
 * Exception-safety: Strong guarantee (operations are atomic)
 *
 * @section usage Usage Example
 * @code
 * LockFreeQueue<int, 1024> queue;
 *
 * // Producer thread
 * queue.enqueue(42);
 *
 * // Consumer thread
 * int value;
 * if (queue.dequeue(value))
 * {
 *     // Got value
 * }
 *
 * // Check stats (if enabled)
 * auto stats = queue.stats();
 * @endcode
 */

#include "FatPConfig.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <type_traits>
#include <utility>

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
#include <intrin.h>
#endif

namespace fat_p
{

// ============================================================================
// Queue Statistics
// ============================================================================

namespace lockfree_queue
{

/**
 * @brief Statistics for lock-free queue performance monitoring
 */
struct Stats
{
    uint64_t totalEnqueues = 0;
    uint64_t totalDequeues = 0;
    uint64_t failedEnqueues = 0;
    uint64_t failedDequeues = 0;
    size_t currentSize = 0;
    size_t capacity = 0;
};

} // namespace lockfree_queue

// ============================================================================
// CPU Pause Intrinsic
// ============================================================================

namespace detail
{

/**
 * @brief CPU pause hint for spin-wait loops
 *
 * Reduces power consumption and improves performance on hyperthreaded cores
 * by hinting to the processor that this is a spin-wait loop.
 */
inline void cpuPause() noexcept
{
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
    _mm_pause();
#elif defined(__x86_64__) || defined(__i386__)
    __builtin_ia32_pause();
#elif defined(__aarch64__)
    __asm__ __volatile__("yield" ::: "memory");
#elif defined(__arm__)
    __asm__ __volatile__("yield" ::: "memory");
#else
    std::this_thread::yield();
#endif
}

} // namespace detail

// ============================================================================
// Lock-Free Queue Implementation
// ============================================================================

/**
 * @brief Lock-free MPMC queue with fixed capacity
 *
 * @tparam T Element type (must be trivially copyable)
 * @tparam MaxSize Maximum queue capacity (must be power of 2)
 * @tparam EnableStats Enable statistics tracking (slight overhead when true)
 *
 * Algorithm: Uses per-slot sequence numbers to track slot state:
 * - Slot ready to write: sequence == enqueue position
 * - Slot ready to read: sequence == enqueue position + 1
 * - After read: sequence == enqueue position + MaxSize
 *
 * This prevents ABA problems by ensuring each slot cycles through
 * monotonically increasing sequence numbers.
 */
template <typename T, size_t MaxSize = 1024, bool EnableStats = false>
class LockFreeQueue
{
    static_assert(std::is_trivially_copyable_v<T>,
                  "T must be trivially copyable for lock-free operations");
    static_assert((MaxSize & (MaxSize - 1)) == 0,
                  "MaxSize must be power of 2");
    static_assert(MaxSize > 0,
                  "MaxSize must be positive");
    static_assert(std::atomic<uint64_t>::is_always_lock_free,
                  "uint64_t atomics must be lock-free for this implementation");

public:
    using value_type = T;
    using size_type = size_t;
    using stats_type = lockfree_queue::Stats;

private:
    static constexpr size_t kMask = MaxSize - 1;

    // Slot with sequence number for ABA prevention
    struct alignas(FATP_CACHE_LINE_SIZE) Slot
    {
        std::atomic<uint64_t> sequence;
        T data;

        Slot() noexcept
            : sequence(0)
        {
        }
    };

    // Statistics storage (conditionally compiled)
    struct StatsStorage
    {
        alignas(FATP_CACHE_LINE_SIZE) std::atomic<uint64_t> totalEnqueues{0};
        alignas(FATP_CACHE_LINE_SIZE) std::atomic<uint64_t> totalDequeues{0};
        alignas(FATP_CACHE_LINE_SIZE) std::atomic<uint64_t> failedEnqueues{0};
        alignas(FATP_CACHE_LINE_SIZE) std::atomic<uint64_t> failedDequeues{0};
    };

    struct NoStatsStorage
    {
    };

    using StatsType = std::conditional_t<EnableStats, StatsStorage, NoStatsStorage>;

public:
    /**
     * @brief Construct queue
     *
     * Initializes all slot sequence numbers for correct initial state.
     */
    LockFreeQueue() noexcept
    {
        for (size_t i = 0; i < MaxSize; ++i)
        {
            mSlots[i].sequence.store(i, std::memory_order_relaxed);
        }
        mEnqueuePos.store(0, std::memory_order_relaxed);
        mDequeuePos.store(0, std::memory_order_relaxed);
    }

    ~LockFreeQueue() = default;

    // Non-copyable (contains atomics)
    LockFreeQueue(const LockFreeQueue&) = delete;
    LockFreeQueue& operator=(const LockFreeQueue&) = delete;

    // Non-movable (contains atomics)
    LockFreeQueue(LockFreeQueue&&) = delete;
    LockFreeQueue& operator=(LockFreeQueue&&) = delete;

    /**
     * @brief Enqueue an element (copy)
     * @param item Element to enqueue
     * @return true if enqueued, false if queue is full
     */
    [[nodiscard]] bool enqueue(const T& item) noexcept
    {
        return enqueueImpl(item);
    }

    /**
     * @brief Enqueue an element (move)
     * @param item Element to enqueue
     * @return true if enqueued, false if queue is full
     */
    [[nodiscard]] bool enqueue(T&& item) noexcept
    {
        return enqueueImpl(std::move(item));
    }

    /**
     * @brief Dequeue an element
     * @param item Output parameter for dequeued element
     * @return true if dequeued, false if queue is empty
     */
    [[nodiscard]] bool dequeue(T& item) noexcept
    {
        uint64_t pos = mDequeuePos.load(std::memory_order_relaxed);

        for (;;)
        {
            Slot* slot = &mSlots[pos & kMask];
            uint64_t seq = slot->sequence.load(std::memory_order_acquire);
            auto diff = static_cast<int64_t>(seq) - static_cast<int64_t>(pos + 1);

            if (diff == 0)
            {
                // Slot is ready to dequeue
                if (mDequeuePos.compare_exchange_weak(pos,
                                                      pos + 1,
                                                      std::memory_order_relaxed))
                {
                    item = std::move(slot->data);
                    slot->sequence.store(pos + MaxSize, std::memory_order_release);
                    incrementStat<&StatsStorage::totalDequeues>();
                    return true;
                }
            }
            else if (diff < 0)
            {
                // Queue is empty
                incrementStat<&StatsStorage::failedDequeues>();
                return false;
            }
            else
            {
                // Another thread dequeued, retry
                pos = mDequeuePos.load(std::memory_order_relaxed);
            }
        }
    }

    /**
     * @brief Try to dequeue with spin-wait
     * @param item Output parameter
     * @param maxAttempts Maximum retry attempts before giving up
     * @return true if dequeued
     */
    [[nodiscard]] bool tryDequeue(T& item, size_t maxAttempts = 100) noexcept
    {
        for (size_t i = 0; i < maxAttempts; ++i)
        {
            if (dequeue(item))
            {
                return true;
            }
            detail::cpuPause();
        }
        return false;
    }

    /**
     * @brief Check if queue is empty
     * @note Snapshot only - state may change immediately after return
     */
    [[nodiscard]] bool empty() const noexcept
    {
        uint64_t enq = mEnqueuePos.load(std::memory_order_acquire);
        uint64_t deq = mDequeuePos.load(std::memory_order_acquire);
        return enq == deq;
    }

    /**
     * @brief Get approximate queue size
     * @note Snapshot only - may not be exact under high contention
     */
    [[nodiscard]] size_t size() const noexcept
    {
        uint64_t enq = mEnqueuePos.load(std::memory_order_acquire);
        uint64_t deq = mDequeuePos.load(std::memory_order_acquire);
        return static_cast<size_t>(enq - deq);
    }

    /**
     * @brief Get queue capacity
     */
    [[nodiscard]] static constexpr size_t capacity() noexcept
    {
        return MaxSize;
    }

    /**
     * @brief Get queue statistics
     * @note Only available when EnableStats = true
     */
    template <bool E = EnableStats>
        requires E
    [[nodiscard]] stats_type stats() const noexcept
    {
        stats_type result;
        result.totalEnqueues = mStats.totalEnqueues.load(std::memory_order_relaxed);
        result.totalDequeues = mStats.totalDequeues.load(std::memory_order_relaxed);
        result.failedEnqueues = mStats.failedEnqueues.load(std::memory_order_relaxed);
        result.failedDequeues = mStats.failedDequeues.load(std::memory_order_relaxed);
        result.currentSize = size();
        result.capacity = MaxSize;
        return result;
    }

    /**
     * @brief Reset statistics
     * @note Only available when EnableStats = true
     */
    template <bool E = EnableStats>
        requires E
    void resetStats() noexcept
    {
        mStats.totalEnqueues.store(0, std::memory_order_relaxed);
        mStats.totalDequeues.store(0, std::memory_order_relaxed);
        mStats.failedEnqueues.store(0, std::memory_order_relaxed);
        mStats.failedDequeues.store(0, std::memory_order_relaxed);
    }

private:
    /**
     * @brief Internal enqueue implementation
     */
    template <typename U>
    [[nodiscard]] bool enqueueImpl(U&& item) noexcept
    {
        uint64_t pos = mEnqueuePos.load(std::memory_order_relaxed);

        for (;;)
        {
            Slot* slot = &mSlots[pos & kMask];
            uint64_t seq = slot->sequence.load(std::memory_order_acquire);
            auto diff = static_cast<int64_t>(seq) - static_cast<int64_t>(pos);

            if (diff == 0)
            {
                // Slot is ready to enqueue
                if (mEnqueuePos.compare_exchange_weak(pos,
                                                      pos + 1,
                                                      std::memory_order_relaxed))
                {
                    slot->data = std::forward<U>(item);
                    slot->sequence.store(pos + 1, std::memory_order_release);
                    incrementStat<&StatsStorage::totalEnqueues>();
                    return true;
                }
            }
            else if (diff < 0)
            {
                // Queue is full
                incrementStat<&StatsStorage::failedEnqueues>();
                return false;
            }
            else
            {
                // Another thread enqueued, retry
                pos = mEnqueuePos.load(std::memory_order_relaxed);
            }
        }
    }

    /**
     * @brief Conditionally increment a statistic counter
     */
    template <std::atomic<uint64_t> StatsStorage::*Counter>
    void incrementStat() noexcept
    {
        if constexpr (EnableStats)
        {
            (mStats.*Counter).fetch_add(1, std::memory_order_relaxed);
        }
    }

    // Data members - carefully ordered for cache efficiency
    alignas(FATP_CACHE_LINE_SIZE) std::atomic<uint64_t> mEnqueuePos;
    alignas(FATP_CACHE_LINE_SIZE) std::atomic<uint64_t> mDequeuePos;
    alignas(FATP_CACHE_LINE_SIZE) StatsType mStats;
    alignas(FATP_CACHE_LINE_SIZE) std::array<Slot, MaxSize> mSlots;
};

} // namespace fat_p
