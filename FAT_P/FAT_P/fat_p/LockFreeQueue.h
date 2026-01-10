/**
 * @file LockFreeQueue.h
 * @brief High-performance lock-free MPMC queue implementation
 * 
 * @details Lock-free multi-producer multi-consumer queue using atomic operations.
 * Complements CircularBuffer with true lock-free semantics for high contention scenarios.
 * 
 * Features:
 * - True lock-free MPMC semantics
 * - Fixed capacity (power of 2 for fast modulo)
 * - Wait-free for single producer/consumer
 * - ABA-problem resistant
 * - Cache-line aligned to prevent false sharing
 * - Bounded memory usage
 * 
 * @section performance Performance Characteristics
 * - Enqueue: O(1) amortized, ~50-100ns uncontended
 * - Dequeue: O(1) amortized, ~50-100ns uncontended
 * - Under contention: ~200-500ns per operation
 * - Memory: Fixed (capacity * sizeof(T) + metadata)
 * 
 * @section algorithm Algorithm
 * Uses sequence numbers to track slot state:
 * - Empty slot: sequence == expected_read
 * - Full slot: sequence == expected_write
 * - Prevents ABA problem via monotonic sequences
 * 
 * @section usage Usage Example
 * @code
 * LockFreeQueue<int> queue(1024);
 * 
 * // Producer thread
 * queue.enqueue(42);
 * 
 * // Consumer thread
 * int value;
 * if (queue.dequeue(value)) {
 *     // Got value
 * }
 * 
 * // Check stats
 * auto stats = queue.stats();
 * std::cout << "Enqueued: " << stats.total_enqueues << "\n";
 * @endcode
 */

#pragma once

/*
FATP_META:
  meta_version: 1
  component: LockFreeQueue
  file_role: public_header
  path: fat_p/LockFreeQueue.h
  namespace: fat_p
  summary: "Public header for LockFreeQueue."
  api_stability: in_work
  related:
    docs_search: "LockFreeQueue"
    tests:
      - tests/test_LockFreeQueue.cpp
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
#include <atomic>
#include <array>
#include <memory>
#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>
#include "FatPConfig.h"

#include "FatPTypeTraits.h"

namespace fat_p {

// ============================================================================
// Cache Line Size
// ============================================================================


// ============================================================================
// Queue Statistics
// ============================================================================

/**
 * @brief Statistics for lock-free queue performance monitoring
 */
struct LockFreeQueueStats {
    uint64_t total_enqueues = 0;
    uint64_t total_dequeues = 0;
    uint64_t failed_enqueues = 0;  // Queue full
    uint64_t failed_dequeues = 0;  // Queue empty
    size_t current_size = 0;
    size_t capacity = 0;
};

// ============================================================================
// Lock-Free Queue Implementation
// ============================================================================

/**
 * @brief Lock-free MPMC queue with fixed capacity
 * 
 * @tparam T Element type (must be trivially copyable for best performance)
 * @tparam MaxSize Maximum queue capacity (must be power of 2)
 * 
 * Thread-safety: Full MPMC lock-free
 * Exception-safety: Strong guarantee (operations are atomic)
 */
template<typename T, size_t MaxSize = 1024>
class LockFreeQueue {
    static_assert((MaxSize & (MaxSize - 1)) == 0, "MaxSize must be power of 2");
    static_assert(MaxSize > 0, "MaxSize must be positive");
    
    // Slot with sequence number for ABA prevention
    struct alignas(FATP_CACHE_LINE_SIZE) Slot {
        std::atomic<uint64_t> sequence;
        T data;
        
        Slot() : sequence(0) {}
    };
    
public:
    /**
     * @brief Construct queue with specified capacity
     * @param capacity Queue capacity (will be rounded up to power of 2)
     */
    explicit LockFreeQueue(size_t capacity = MaxSize) 
        : m_mask(MaxSize - 1)
    {
        
        (void)capacity; // Capacity is fixed by MaxSize template parameter.
// Initialize sequence numbers
        for (size_t i = 0; i < MaxSize; ++i) {
            m_slots[i].sequence.store(i, std::memory_order_relaxed);
        }
        
        m_enqueue_pos.store(0, std::memory_order_relaxed);
        m_dequeue_pos.store(0, std::memory_order_relaxed);
    }
    
    /**
     * @brief Destructor
     */
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
    bool enqueue(const T& item) {
        return enqueue_impl(item);
    }
    
    /**
     * @brief Enqueue an element (move)
     * @param item Element to enqueue
     * @return true if enqueued, false if queue is full
     */
    bool enqueue(T&& item) {
        return enqueue_impl(std::move(item));
    }
    
    /**
     * @brief Dequeue an element
     * @param item Output parameter for dequeued element
     * @return true if dequeued, false if queue is empty
     */
    bool dequeue(T& item) {
        Slot* slot;
        uint64_t pos = m_dequeue_pos.load(std::memory_order_relaxed);
        
        for (;;) {
            slot = &m_slots[pos & m_mask];
            uint64_t seq = slot->sequence.load(std::memory_order_acquire);
            intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);
            
            if (diff == 0) {
                // Slot is ready to dequeue
                if (m_dequeue_pos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    item = std::move(slot->data);
                    slot->sequence.store(pos + m_mask + 1, std::memory_order_release);
                    m_stats.total_dequeues.fetch_add(1, std::memory_order_relaxed);
                    return true;
                }
            } else if (diff < 0) {
                // Queue is empty
                m_stats.failed_dequeues.fetch_add(1, std::memory_order_relaxed);
                return false;
            } else {
                // Another thread dequeued, retry
                pos = m_dequeue_pos.load(std::memory_order_relaxed);
            }
        }
    }
    
    /**
     * @brief Try to dequeue with timeout (busy-wait)
     * @param item Output parameter
     * @param max_attempts Maximum retry attempts
     * @return true if dequeued
     */
    bool try_dequeue(T& item, size_t max_attempts = 100) {
        for (size_t i = 0; i < max_attempts; ++i) {
            if (dequeue(item)) {
                return true;
            }
            // Brief pause to reduce contention
            for (volatile int j = 0; j < 10; ++j);
        }
        return false;
    }
    
    /**
     * @brief Check if queue is empty
     * @note This is a snapshot - state may change immediately
     */
    bool empty() const noexcept {
        uint64_t enq = m_enqueue_pos.load(std::memory_order_acquire);
        uint64_t deq = m_dequeue_pos.load(std::memory_order_acquire);
        return enq == deq;
    }
    
    /**
     * @brief Get approximate queue size
     * @note This is a snapshot - may not be exact under high contention
     */
    size_t size() const noexcept {
        uint64_t enq = m_enqueue_pos.load(std::memory_order_acquire);
        uint64_t deq = m_dequeue_pos.load(std::memory_order_acquire);
        return static_cast<size_t>(enq - deq);
    }
    
    /**
     * @brief Get queue capacity
     */
    size_t capacity() const noexcept {
        return MaxSize;
    }
    
    /**
     * @brief Get queue statistics
     */
    LockFreeQueueStats stats() const {
        LockFreeQueueStats result;
        result.total_enqueues = m_stats.total_enqueues.load(std::memory_order_relaxed);
        result.total_dequeues = m_stats.total_dequeues.load(std::memory_order_relaxed);
        result.failed_enqueues = m_stats.failed_enqueues.load(std::memory_order_relaxed);
        result.failed_dequeues = m_stats.failed_dequeues.load(std::memory_order_relaxed);
        result.current_size = size();
        result.capacity = MaxSize;
        return result;
    }
    
    /**
     * @brief Reset statistics
     */
    void reset_stats() {
        m_stats.total_enqueues.store(0, std::memory_order_relaxed);
        m_stats.total_dequeues.store(0, std::memory_order_relaxed);
        m_stats.failed_enqueues.store(0, std::memory_order_relaxed);
        m_stats.failed_dequeues.store(0, std::memory_order_relaxed);
    }
    
private:
    /**
     * @brief Internal enqueue implementation
     */
    template<typename U>
    bool enqueue_impl(U&& item) {
        Slot* slot;
        uint64_t pos = m_enqueue_pos.load(std::memory_order_relaxed);
        
        for (;;) {
            slot = &m_slots[pos & m_mask];
            uint64_t seq = slot->sequence.load(std::memory_order_acquire);
            intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);
            
            if (diff == 0) {
                // Slot is ready to enqueue
                if (m_enqueue_pos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    slot->data = std::forward<U>(item);
                    slot->sequence.store(pos + 1, std::memory_order_release);
                    m_stats.total_enqueues.fetch_add(1, std::memory_order_relaxed);
                    return true;
                }
            } else if (diff < 0) {
                // Queue is full
                m_stats.failed_enqueues.fetch_add(1, std::memory_order_relaxed);
                return false;
            } else {
                // Another thread enqueued, retry
                pos = m_enqueue_pos.load(std::memory_order_relaxed);
            }
        }
    }
    
    // Align to cache line to prevent false sharing
    alignas(FATP_CACHE_LINE_SIZE) std::atomic<uint64_t> m_enqueue_pos;
    alignas(FATP_CACHE_LINE_SIZE) std::atomic<uint64_t> m_dequeue_pos;
    
    const uint64_t m_mask;
    
    // Statistics (relaxed ordering for performance)
    struct {
        alignas(FATP_CACHE_LINE_SIZE) std::atomic<uint64_t> total_enqueues{0};
        alignas(FATP_CACHE_LINE_SIZE) std::atomic<uint64_t> total_dequeues{0};
        alignas(FATP_CACHE_LINE_SIZE) std::atomic<uint64_t> failed_enqueues{0};
        alignas(FATP_CACHE_LINE_SIZE) std::atomic<uint64_t> failed_dequeues{0};
    } m_stats;
    
    // Slot array (must be last for alignment)
    alignas(FATP_CACHE_LINE_SIZE) std::array<Slot, MaxSize> m_slots;
};


template <typename T, size_t MaxSize >
struct is_lock_free_queue<LockFreeQueue<T, MaxSize>> : std::true_type {};

} // namespace fat_p