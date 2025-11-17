// LockFreeRingBuffer.h
#pragma once

#include <atomic>
#include <cstddef>
#include <cstring>
#include <memory>
#include <optional>
#include <type_traits>
#include <malloc.h> // For _aligned_malloc and _aligned_free on Windows

#include "FatPTypeTraits.h"

namespace fat_p {

// ============================================================================
// LockFreeRingBuffer - Single Producer Single Consumer Queue
// ============================================================================
//
// A wait-free ring buffer for single-producer single-consumer scenarios.
// Uses atomic operations for synchronization without locks.
//
// Perfect for:
// - Audio/video processing pipelines
// - Inter-thread communication
// - Producer-consumer patterns
// - Real-time systems
// - Logging from hot paths
//
// Performance characteristics:
// - Wait-free push/pop (no blocking)
// - Cache-line aligned to avoid false sharing
// - Pre-allocated memory (no dynamic allocation after construction)
// - Extremely fast: ~5-20ns per operation
//
// Example:
//   LockFreeRingBuffer<int> buffer(1024);
//   
//   // Producer thread
//   buffer.push(42);
//   
//   // Consumer thread
//   if (auto val = buffer.pop()) {
//       process(*val);
//   }
//
// Thread safety:
// - ONE producer thread
// - ONE consumer thread
// - No locks, wait-free operations
// ============================================================================

template<typename T>
class LockFreeRingBuffer {
public:
    static_assert(std::is_trivially_copyable_v<T>, 
                  "T must be trivially copyable for lock-free operations");
    
    explicit LockFreeRingBuffer(size_t capacity)
        : capacity_(round_up_power_of_two(capacity))
        , mask_(capacity_ - 1)
        , buffer_(allocate_aligned(capacity_))
        , write_pos_(0)
        , read_pos_(0) {
    }
    
    // LockFreeRingBuffer.h (updated destructor)
    ~LockFreeRingBuffer() {
        if (buffer_) {
#ifdef _WIN32
            // Use _aligned_free for memory allocated with _aligned_malloc
            _aligned_free(buffer_);
#else
            // Use std::free for memory allocated with posix_memalign
            // Note: On POSIX systems, posix_memalign memory is freed with 'free', 
            // which is equivalent to std::free.
            std::free(buffer_);
#endif
        }
    }
    // Non-copyable
    LockFreeRingBuffer(const LockFreeRingBuffer&) = delete;
    LockFreeRingBuffer& operator=(const LockFreeRingBuffer&) = delete;
    
    // Non-moveable (contains atomic members)
    LockFreeRingBuffer(LockFreeRingBuffer&&) = delete;
    LockFreeRingBuffer& operator=(LockFreeRingBuffer&&) = delete;
    
    // Push element (producer only)
    // Returns false if buffer is full
    [[nodiscard]] bool push(const T& value) {
        const size_t write = write_pos_.load(std::memory_order_relaxed);
        const size_t read = read_pos_.load(std::memory_order_acquire);
        
        if (is_full(write, read)) {
            return false;
        }
        
        buffer_[write & mask_] = value;
        
        // Release write to make data visible to consumer
        write_pos_.store(write + 1, std::memory_order_release);
        
        return true;
    }
    
    // Move version
    [[nodiscard]] bool push(T&& value) {
        const size_t write = write_pos_.load(std::memory_order_relaxed);
        const size_t read = read_pos_.load(std::memory_order_acquire);
        
        if (is_full(write, read)) {
            return false;
        }
        
        buffer_[write & mask_] = std::move(value);
        
        write_pos_.store(write + 1, std::memory_order_release);
        
        return true;
    }
    
    // Pop element (consumer only)
    // Returns std::nullopt if buffer is empty
    [[nodiscard]] std::optional<T> pop() {
        const size_t read = read_pos_.load(std::memory_order_relaxed);
        const size_t write = write_pos_.load(std::memory_order_acquire);
        
        if (is_empty(write, read)) {
            return std::nullopt;
        }
        
        T value = buffer_[read & mask_];
        
        // Release read to make space visible to producer
        read_pos_.store(read + 1, std::memory_order_release);
        
        return value;
    }
    
    // Peek at front without removing (consumer only)
    [[nodiscard]] std::optional<T> peek() const {
        const size_t read = read_pos_.load(std::memory_order_relaxed);
        const size_t write = write_pos_.load(std::memory_order_acquire);
        
        if (is_empty(write, read)) {
            return std::nullopt;
        }
        
        return buffer_[read & mask_];
    }
    
    // Check if empty (can be called by consumer)
    [[nodiscard]] bool empty() const {
        const size_t read = read_pos_.load(std::memory_order_relaxed);
        const size_t write = write_pos_.load(std::memory_order_acquire);
        return is_empty(write, read);
    }
    
    // Check if full (can be called by producer)
    [[nodiscard]] bool full() const {
        const size_t write = write_pos_.load(std::memory_order_relaxed);
        const size_t read = read_pos_.load(std::memory_order_acquire);
        return is_full(write, read);
    }
    
    // Approximate size (may be stale)
    [[nodiscard]] size_t size() const {
        const size_t write = write_pos_.load(std::memory_order_acquire);
        const size_t read = read_pos_.load(std::memory_order_acquire);
        return write - read;
    }
    
    // Capacity
    [[nodiscard]] size_t capacity() const {
        return capacity_;
    }
    
private:
    static constexpr size_t CACHE_LINE_SIZE = 64;
    
    // Round up to next power of two for efficient masking
    static size_t round_up_power_of_two(size_t n) {
        if (n == 0) return 1;
        --n;
        n |= n >> 1;
        n |= n >> 2;
        n |= n >> 4;
        n |= n >> 8;
        n |= n >> 16;
        if constexpr (sizeof(size_t) == 8) {
            n |= n >> 32;
        }
        return n + 1;
    }
    
    // Allocate cache-line aligned memory
    static T* allocate_aligned(size_t capacity) {
        void* ptr = nullptr;
        #ifdef _WIN32
        ptr = _aligned_malloc(capacity * sizeof(T), CACHE_LINE_SIZE);
        #else
        if (posix_memalign(&ptr, CACHE_LINE_SIZE, capacity * sizeof(T)) != 0) {
            throw std::bad_alloc();
        }
        #endif
        return static_cast<T*>(ptr);
    }
    
    bool is_empty(size_t write, size_t read) const {
        return write == read;
    }
    
    bool is_full(size_t write, size_t read) const {
        return (write - read) >= capacity_;
    }
    
    const size_t capacity_;
    const size_t mask_;
    T* buffer_;
    
    // Pad to separate cache lines to avoid false sharing
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> write_pos_;
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> read_pos_;
};

// ============================================================================
// LockFreeRingBufferMPMC - Multi Producer Multi Consumer Queue
// ============================================================================
//
// A lock-free ring buffer supporting multiple producers and consumers.
// Uses CAS operations for thread-safe access.
//
// Performance characteristics:
// - Lock-free (may retry on contention)
// - Slower than SPSC but still very fast
// - Good for general-purpose queues
//
// Example:
//   LockFreeRingBufferMPMC<int> buffer(1024);
//   
//   // Any thread can push
//   buffer.push(42);
//   
//   // Any thread can pop
//   if (auto val = buffer.pop()) {
//       process(*val);
//   }
// ============================================================================

template<typename T>
class LockFreeRingBufferMPMC {
public:
    static_assert(std::is_trivially_copyable_v<T>, 
                  "T must be trivially copyable for lock-free operations");
    
    explicit LockFreeRingBufferMPMC(size_t capacity)
        : capacity_(round_up_power_of_two(capacity))
        , mask_(capacity_ - 1)
        , buffer_(allocate_aligned(capacity_))
        , write_pos_(0)
        , read_pos_(0) {
    }
    
    ~LockFreeRingBufferMPMC() {
        if (buffer_) {
#ifdef _WIN32
            // Use _aligned_free for memory allocated with _aligned_malloc
            _aligned_free(buffer_);
#else
            // Use std::free for memory allocated with posix_memalign
            // Note: On POSIX systems, posix_memalign memory is freed with 'free', 
            // which is equivalent to std::free.
            std::free(buffer_);
#endif
        }
    }

    // Non-copyable, non-moveable
    LockFreeRingBufferMPMC(const LockFreeRingBufferMPMC&) = delete;
    LockFreeRingBufferMPMC& operator=(const LockFreeRingBufferMPMC&) = delete;
    LockFreeRingBufferMPMC(LockFreeRingBufferMPMC&&) = delete;
    LockFreeRingBufferMPMC& operator=(LockFreeRingBufferMPMC&&) = delete;
    
    // Push element (thread-safe for multiple producers)
    [[nodiscard]] bool push(const T& value) {
        size_t write;
        size_t next_write;
        
        do {
            write = write_pos_.load(std::memory_order_acquire);
            const size_t read = read_pos_.load(std::memory_order_acquire);
            
            if (is_full(write, read)) {
                return false;
            }
            
            next_write = write + 1;
            
        } while (!write_pos_.compare_exchange_weak(write, next_write,
                                                    std::memory_order_release,
                                                    std::memory_order_acquire));
        
        buffer_[write & mask_] = value;
        
        return true;
    }
    
    // Pop element (thread-safe for multiple consumers)
    [[nodiscard]] std::optional<T> pop() {
        size_t read;
        size_t next_read;
        
        do {
            read = read_pos_.load(std::memory_order_acquire);
            const size_t write = write_pos_.load(std::memory_order_acquire);
            
            if (is_empty(write, read)) {
                return std::nullopt;
            }
            
            next_read = read + 1;
            
        } while (!read_pos_.compare_exchange_weak(read, next_read,
                                                   std::memory_order_release,
                                                   std::memory_order_acquire));
        
        return buffer_[read & mask_];
    }
    
    [[nodiscard]] bool empty() const {
        const size_t read = read_pos_.load(std::memory_order_acquire);
        const size_t write = write_pos_.load(std::memory_order_acquire);
        return is_empty(write, read);
    }
    
    [[nodiscard]] bool full() const {
        const size_t write = write_pos_.load(std::memory_order_acquire);
        const size_t read = read_pos_.load(std::memory_order_acquire);
        return is_full(write, read);
    }
    
    [[nodiscard]] size_t size() const {
        const size_t write = write_pos_.load(std::memory_order_acquire);
        const size_t read = read_pos_.load(std::memory_order_acquire);
        return write - read;
    }
    
    [[nodiscard]] size_t capacity() const {
        return capacity_;
    }
    
private:
    static constexpr size_t CACHE_LINE_SIZE = 64;
    
    static size_t round_up_power_of_two(size_t n) {
        if (n == 0) return 1;
        --n;
        n |= n >> 1;
        n |= n >> 2;
        n |= n >> 4;
        n |= n >> 8;
        n |= n >> 16;
        if constexpr (sizeof(size_t) == 8) {
            n |= n >> 32;
        }
        return n + 1;
    }
    
    static T* allocate_aligned(size_t capacity) {
        void* ptr = nullptr;
        #ifdef _WIN32
        ptr = _aligned_malloc(capacity * sizeof(T), CACHE_LINE_SIZE);
        #else
        if (posix_memalign(&ptr, CACHE_LINE_SIZE, capacity * sizeof(T)) != 0) {
            throw std::bad_alloc();
        }
        #endif
        return static_cast<T*>(ptr);
    }
    
    bool is_empty(size_t write, size_t read) const {
        return write == read;
    }
    
    bool is_full(size_t write, size_t read) const {
        return (write - read) >= capacity_;
    }
    
    const size_t capacity_;
    const size_t mask_;
    T* buffer_;
    
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> write_pos_;
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> read_pos_;
};


template <typename T>
struct is_lock_free_ring_buffer<LockFreeRingBuffer<T>> : std::true_type {};

} // namespace fat_p