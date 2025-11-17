// CircularBuffer.h
#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <utility>

#include "FatPTypeTraits.h"

namespace fat_p {

/**
 * @brief Lock-free single-producer single-consumer (SPSC) circular buffer
 * @tparam T Element type
 * @tparam Capacity Maximum number of elements (actual capacity is Capacity)
 * 
 * @details This is a wait-free SPSC queue using atomics with memory ordering.
 * - One producer thread can push
 * - One consumer thread can pop
 * - Cache-line alignment prevents false sharing
 * - Uses +1 slot internally to distinguish full from empty
 * 
 * Performance: ~10-20ns per operation on modern CPUs
 */
template <typename T, size_t Capacity>
class CircularBuffer {
private:
    alignas(64) std::unique_ptr<T[]> buffer_;  // Cache-line aligned
    alignas(64) std::atomic<size_t> read_idx_{0};
    alignas(64) std::atomic<size_t> write_idx_{0};

public:
    CircularBuffer() : buffer_(std::make_unique<T[]>(Capacity + 1)) {}  // +1 for full/empty distinction

    /**
     * @brief Push element by copy
     * @param value Element to push
     * @return true if successful, false if buffer is full
     */
    bool push(const T& value) {
        size_t write = write_idx_.load(std::memory_order_relaxed);
        size_t next_write = (write + 1) % (Capacity + 1);
        if (next_write == read_idx_.load(std::memory_order_acquire)) {
            return false;  // Full
        }
        buffer_[write] = value;
        write_idx_.store(next_write, std::memory_order_release);
        return true;
    }

    /**
     * @brief Push element by move
     * @param value Element to move
     * @return true if successful, false if buffer is full
     */
    bool push(T&& value) {
        size_t write = write_idx_.load(std::memory_order_relaxed);
        size_t next_write = (write + 1) % (Capacity + 1);
        if (next_write == read_idx_.load(std::memory_order_acquire)) {
            return false;  // Full
        }
        buffer_[write] = std::move(value);
        write_idx_.store(next_write, std::memory_order_release);
        return true;
    }

    /**
     * @brief Pop element
     * @param value Output parameter to receive popped element
     * @return true if successful, false if buffer is empty
     */
    bool pop(T& value) {
        size_t read = read_idx_.load(std::memory_order_relaxed);
        if (read == write_idx_.load(std::memory_order_acquire)) {
            return false;  // Empty
        }
        value = std::move(buffer_[read]);
        size_t next_read = (read + 1) % (Capacity + 1);
        read_idx_.store(next_read, std::memory_order_release);
        return true;
    }

    /**
     * @brief Get current number of elements
     * @return Number of elements in buffer
     */
    size_t size() const {
        size_t write = write_idx_.load(std::memory_order_acquire);
        size_t read = read_idx_.load(std::memory_order_acquire);
        return (write >= read) ? (write - read) : (Capacity + 1 - read + write);
    }

    /**
     * @brief Check if buffer is empty
     * @return true if empty
     */
    bool empty() const {
        return read_idx_.load(std::memory_order_acquire) == write_idx_.load(std::memory_order_acquire);
    }

    /**
     * @brief Check if buffer is full
     * @return true if full
     */
    bool full() const {
        size_t write = write_idx_.load(std::memory_order_acquire);
        size_t read = read_idx_.load(std::memory_order_acquire);
        return ((write + 1) % (Capacity + 1)) == read;
    }

    /**
     * @brief Get the capacity of the buffer
     * @return Maximum number of elements
     */
    static constexpr size_t capacity() { return Capacity; }
};

template <typename T, size_t N>
struct is_circular_buffer<CircularBuffer<T, N>> : std::true_type {};

}  // namespace fay_p
