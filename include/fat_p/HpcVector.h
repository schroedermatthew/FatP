#pragma once

/*
FATP_META:
  meta_version: 1
  component: HpcVector
  file_role: public_header
  path: include/fat_p/HpcVector.h
  namespace: fat_p
  layer: Containers
  summary: "Public header for HpcVector."
  api_stability: in_work
  related:
    docs_search: "HpcVector"
    tests:
      - components/HpcVector/tests/test_HpcVector.cpp
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
 * @file HpcVector.h
 * @brief The "Holy Grail" HPC container: cache-aligned, NUMA-local, SIMD-ready
 *
 *
 * @version 1.2
 *
 * Version History:
 * - 1.2: Allocator refactored to leverage NumaAllocator.h (no duplication)
 * - 1.1: Renamed is_numa_allocated() to isNumaAvailable() for clarity
 * - 1.0: Initial release
 *
 * @details This header provides HpcVector, a high-performance container that
 * combines the benefits of AlignedVector and NumaAllocator:
 *
 * 1. Cache-Line Alignment: Data starts on 64-byte boundaries, enabling
 *    aligned SIMD loads/stores and preventing false sharing.
 *
 * 2. NUMA Locality: Memory is allocated on the current thread's NUMA node,
 *    avoiding expensive cross-socket memory accesses on multi-socket systems.
 *
 * 3. SIMD Optimization: assume_aligned() method provides compiler hints
 *    for auto-vectorization.
 *
 * 4. STL Compatibility: Full std::vector-like interface with iterators,
 *    capacity management, and exception-safe operations.
 *
 * Architecture:
 *
 *   +-------------------------------------------------------------+
 *   |                    HpcVector<T>                             |
 *   |  +---------------------------------------------------------+|
 *   |  |              HpcAllocator<T, 64, Policy>                ||
 *   |  |  +--------------+    +--------------------------------+ ||
 *   |  |  | NUMA Path    | OR | Aligned Fallback Path          | ||
 *   |  |  | (page-aligned|    | (posix_memalign/_aligned_malloc)| ||
 *   |  |  | >= 4KB)      |    |                                | ||
 *   |  |  +--------------+    +--------------------------------+ ||
 *   |  +---------------------------------------------------------+|
 *   |                                                             |
 *   |  Features:                                                  |
 *   |  - assume_aligned() for compiler optimization hints         |
 *   |  - isAligned() runtime verification                        |
 *   |  - get_numa_node() for debugging/verification               |
 *   |  - All std::vector operations                               |
 *   +-------------------------------------------------------------+
 *
 * Performance Impact:
 * - NUMA: Reduces memory latency from ~150ns to ~60ns on multi-socket
 * - Alignment: Enables vmovaps vs vmovups (10-30% faster SIMD)
 * - Combined: Up to 2x throughput for memory-bound HPC workloads
 *
 * Usage:
 *   // Basic usage (local NUMA node, 64-byte aligned)
 *   HpcVector<float> data(1000000);
 *
 *   // Use assume_aligned() for SIMD loops
 *   float* ptr = data.assume_aligned();
 *   for (size_t i = 0; i < data.size(); i += 8) {
 *       __m256 v = _mm256_load_ps(ptr + i);  // Aligned load!
 *       // ...
 *   }
 *
 *   // Specific NUMA node
 *   HpcVector<double, 64, NumaPreferredPolicy> gpu_adjacent(1000, NumaPreferredPolicy{1});
 *
 *   // Interleaved across all nodes (for large shared arrays)
 *   HpcVector<int, 64, NumaInterleavedPolicy> shared_data(10000000);
 *
 * @see NumaAlignedAllocator.h for allocator details
 * @see CheckedArithmetic.h for integration with checked vector operations
 *
 * Requires: C++17
 */

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>
#include <type_traits>

#include "NumaAlignedAllocator.h"

namespace fat_p
{

// =============================================================================
// HpcVector Implementation
// =============================================================================

/**
 * @brief High-performance vector with NUMA locality and cache alignment
 *
 * @tparam T Element type
 * @tparam Alignment Memory alignment in bytes (default 64 for cache lines)
 * @tparam Policy NUMA allocation policy (default NumaLocalPolicy)
 *
 * This is the recommended container for HPC workloads in the Fat-P library.
 * It combines the benefits of AlignedVector (SIMD alignment) with
 * NumaAllocator (memory locality) in a single, easy-to-use container.
 */
template <typename T, std::size_t Alignment = 64, typename Policy = memory::NumaLocalPolicy>
class HpcVector
{
public:
    // Type definitions
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using iterator = T*;
    using const_iterator = const T*;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    using allocator_type = memory::NumaAlignedAllocator<T, Alignment, Policy>;

    static constexpr std::size_t alignment = Alignment;

private:
    allocator_type mAllocator;
    pointer data_;
    size_type mSize;
    size_type mCapacity;

    // Internal helpers
    void destroy_range(pointer first, pointer last) noexcept
    {
        if constexpr (!std::is_trivially_destructible_v<T>)
        {
            for (; first != last; ++first)
            {
                first->~T();
            }
        }
    }

    void construct_range_copy(pointer dest, const_pointer src, size_type count)
    {
        if constexpr (std::is_trivially_copyable_v<T>)
        {
            if (count > 0)
            {
                assert(count <= std::numeric_limits<size_t>::max() / sizeof(T));
                std::memcpy(dest, src, count * sizeof(T));
            }
        }
        else
        {
            for (size_type i = 0; i < count; ++i)
            {
                new (dest + i) T(src[i]);
            }
        }
    }

    void construct_range_move(pointer dest, pointer src, size_type count)
    {
        if constexpr (std::is_trivially_copyable_v<T>)
        {
            if (count > 0)
            {
                assert(count <= std::numeric_limits<size_t>::max() / sizeof(T));
                std::memcpy(dest, src, count * sizeof(T));
            }
        }
        else
        {
            for (size_type i = 0; i < count; ++i)
            {
                new (dest + i) T(std::move(src[i]));
            }
        }
    }

    /**
     * @brief Exception-safe reallocation
     *
     * If move construction throws at element N, elements 0..N-1 are properly
     * destroyed before re-throwing. This prevents resource leaks for types
     * with non-trivial destructors (e.g., std::string, containers).
     */
    void reallocate(size_type new_capacity)
    {
        pointer new_data = mAllocator.allocate(new_capacity);
        size_type constructed_count = 0;

        try
        {
            if constexpr (std::is_trivially_copyable_v<T>)
            {
                // Fast path: memcpy is noexcept, no partial construction possible
                if (mSize > 0)
                {
                    std::memcpy(new_data, data_, mSize * sizeof(T));
                }
                constructed_count = mSize;
            }
            else
            {
                // Slow path: track progress for exception safety
                for (; constructed_count < mSize; ++constructed_count)
                {
                    new (new_data + constructed_count) T(std::move(data_[constructed_count]));
                }
            }
        }
        catch (...)
        {
            // Destroy successfully constructed elements before freeing memory
            if constexpr (!std::is_trivially_destructible_v<T>)
            {
                for (size_type i = 0; i < constructed_count; ++i)
                {
                    new_data[i].~T();
                }
            }
            mAllocator.deallocate(new_data, new_capacity);
            throw;
        }

        destroy_range(data_, data_ + mSize);
        mAllocator.deallocate(data_, mCapacity);

        data_ = new_data;
        mCapacity = new_capacity;
    }

public:
    // =========================================================================
    // Constructors
    // =========================================================================

    HpcVector() noexcept
        : mAllocator()
        , data_(nullptr)
        , mSize(0)
        , mCapacity(0)
    {
    }

    explicit HpcVector(const allocator_type& alloc) noexcept
        : mAllocator(alloc)
        , data_(nullptr)
        , mSize(0)
        , mCapacity(0)
    {
    }

    explicit HpcVector(const Policy& policy) noexcept
        : mAllocator(policy)
        , data_(nullptr)
        , mSize(0)
        , mCapacity(0)
    {
    }

    explicit HpcVector(size_type count, const allocator_type& alloc = allocator_type())
        : mAllocator(alloc)
        , data_(nullptr)
        , mSize(0)
        , mCapacity(0)
    {
        if (count > 0)
        {
            data_ = mAllocator.allocate(count);
            mCapacity = count;

            if constexpr (std::is_trivially_default_constructible_v<T>)
            {
                assert(count <= std::numeric_limits<size_t>::max() / sizeof(T));
                std::memset(data_, 0, count * sizeof(T));
                mSize = count;
            }
            else
            {
                for (mSize = 0; mSize < count; ++mSize)
                {
                    new (data_ + mSize) T();
                }
            }
        }
    }

    HpcVector(size_type count, const T& value, const allocator_type& alloc = allocator_type())
        : mAllocator(alloc)
        , data_(nullptr)
        , mSize(0)
        , mCapacity(0)
    {
        if (count > 0)
        {
            data_ = mAllocator.allocate(count);
            mCapacity = count;

            for (mSize = 0; mSize < count; ++mSize)
            {
                new (data_ + mSize) T(value);
            }
        }
    }

    template <std::input_iterator InputIt>
    HpcVector(InputIt first, InputIt last, const allocator_type& alloc = allocator_type())
        : mAllocator(alloc)
        , data_(nullptr)
        , mSize(0)
        , mCapacity(0)
    {
        size_type count = static_cast<size_type>(std::distance(first, last));
        if (count > 0)
        {
            data_ = mAllocator.allocate(count);
            mCapacity = count;

            for (; first != last; ++first, ++mSize)
            {
                new (data_ + mSize) T(*first);
            }
        }
    }

    HpcVector(std::initializer_list<T> init, const allocator_type& alloc = allocator_type())
        : HpcVector(init.begin(), init.end(), alloc)
    {
    }

    // Copy constructor
    HpcVector(const HpcVector& other)
        : mAllocator(other.mAllocator)
        , data_(nullptr)
        , mSize(0)
        , mCapacity(0)
    {
        if (other.mSize > 0)
        {
            data_ = mAllocator.allocate(other.mSize);
            mCapacity = other.mSize;
            construct_range_copy(data_, other.data_, other.mSize);
            mSize = other.mSize;
        }
    }

    // Move constructor
    HpcVector(HpcVector&& other) noexcept
        : mAllocator(std::move(other.mAllocator))
        , data_(other.data_)
        , mSize(other.mSize)
        , mCapacity(other.mCapacity)
    {
        other.data_ = nullptr;
        other.mSize = 0;
        other.mCapacity = 0;
    }

    // Destructor
    ~HpcVector()
    {
        destroy_range(data_, data_ + mSize);
        mAllocator.deallocate(data_, mCapacity);
    }

    // =========================================================================
    // Assignment
    // =========================================================================

    HpcVector& operator=(const HpcVector& other)
    {
        if (this != &other)
        {
            HpcVector tmp(other);
            swap(tmp);
        }
        return *this;
    }

    HpcVector& operator=(HpcVector&& other) noexcept
    {
        if (this != &other)
        {
            destroy_range(data_, data_ + mSize);
            mAllocator.deallocate(data_, mCapacity);

            mAllocator = std::move(other.mAllocator);
            data_ = other.data_;
            mSize = other.mSize;
            mCapacity = other.mCapacity;

            other.data_ = nullptr;
            other.mSize = 0;
            other.mCapacity = 0;
        }
        return *this;
    }

    HpcVector& operator=(std::initializer_list<T> init)
    {
        HpcVector tmp(init, mAllocator);
        swap(tmp);
        return *this;
    }

    // =========================================================================
    // Element Access
    // =========================================================================

    [[nodiscard]] reference operator[](size_type pos) noexcept
    {
        return data_[pos];
    }

    [[nodiscard]] const_reference operator[](size_type pos) const noexcept
    {
        return data_[pos];
    }

    [[nodiscard]] reference at(size_type pos)
    {
        if (pos >= mSize)
        {
            throw std::out_of_range("HpcVector::at: index out of range");
        }
        return data_[pos];
    }

    [[nodiscard]] const_reference at(size_type pos) const
    {
        if (pos >= mSize)
        {
            throw std::out_of_range("HpcVector::at: index out of range");
        }
        return data_[pos];
    }

    [[nodiscard]] reference front() noexcept
    {
        return data_[0];
    }
    [[nodiscard]] const_reference front() const noexcept
    {
        return data_[0];
    }
    [[nodiscard]] reference back() noexcept
    {
        return data_[mSize - 1];
    }
    [[nodiscard]] const_reference back() const noexcept
    {
        return data_[mSize - 1];
    }

    [[nodiscard]] pointer data() noexcept
    {
        return data_;
    }
    [[nodiscard]] const_pointer data() const noexcept
    {
        return data_;
    }

    // =========================================================================
    // SIMD / HPC Specific Methods
    // =========================================================================

    /**
     * @brief Get pointer with compiler alignment hint
     *
     * Returns a pointer with __builtin_assume_aligned hint for the compiler,
     * enabling better auto-vectorization.
     *
     * @return Pointer to data with alignment assumption
     *
     * @note Only call this when you know the data is aligned (always true
     *       for HpcVector unless you've done something strange).
     */
    [[nodiscard]] pointer assume_aligned() noexcept
    {
#if defined(__GNUC__) || defined(__clang__)
        return static_cast<pointer>(__builtin_assume_aligned(data_, Alignment));
#else
        return data_;
#endif
    }

    [[nodiscard]] const_pointer assume_aligned() const noexcept
    {
#if defined(__GNUC__) || defined(__clang__)
        return static_cast<const_pointer>(__builtin_assume_aligned(data_, Alignment));
#else
        return data_;
#endif
    }

    /**
     * @brief Check if data is properly aligned
     *
     * @return true if data pointer is aligned to Alignment bytes
     */
    [[nodiscard]] bool isAligned() const noexcept
    {
        if (!data_)
        {
            return true; // Empty vector is trivially aligned
        }
        return (reinterpret_cast<std::uintptr_t>(data_) % Alignment) == 0;
    }

    /**
     * @brief Check if NUMA support is available
     *
     * @return true if this system supports NUMA and allocations use NUMA APIs
     *
     * @note This indicates whether the allocator uses NUMA APIs, not whether
     *       this specific buffer is on a particular NUMA node. When true,
     *       all allocations from this vector use NUMA (no fallback mixing).
     */
    [[nodiscard]] bool isNumaAvailable() const noexcept
    {
        return mAllocator.numa_available();
    }

    /**
     * @brief Get the alignment of this vector
     *
     * @return Alignment in bytes
     */
    [[nodiscard]] static constexpr std::size_t get_alignment() noexcept
    {
        return Alignment;
    }

    // =========================================================================
    // Iterators
    // =========================================================================

    [[nodiscard]] iterator begin() noexcept
    {
        return data_;
    }
    [[nodiscard]] const_iterator begin() const noexcept
    {
        return data_;
    }
    [[nodiscard]] const_iterator cbegin() const noexcept
    {
        return data_;
    }
    [[nodiscard]] iterator end() noexcept
    {
        return data_ + mSize;
    }
    [[nodiscard]] const_iterator end() const noexcept
    {
        return data_ + mSize;
    }
    [[nodiscard]] const_iterator cend() const noexcept
    {
        return data_ + mSize;
    }
    [[nodiscard]] reverse_iterator rbegin() noexcept
    {
        return reverse_iterator(end());
    }
    [[nodiscard]] const_reverse_iterator rbegin() const noexcept
    {
        return const_reverse_iterator(end());
    }
    [[nodiscard]] const_reverse_iterator crbegin() const noexcept
    {
        return const_reverse_iterator(end());
    }
    [[nodiscard]] reverse_iterator rend() noexcept
    {
        return reverse_iterator(begin());
    }
    [[nodiscard]] const_reverse_iterator rend() const noexcept
    {
        return const_reverse_iterator(begin());
    }
    [[nodiscard]] const_reverse_iterator crend() const noexcept
    {
        return const_reverse_iterator(begin());
    }

    // =========================================================================
    // Capacity
    // =========================================================================

    [[nodiscard]] bool empty() const noexcept
    {
        return mSize == 0;
    }
    [[nodiscard]] size_type size() const noexcept
    {
        return mSize;
    }
    [[nodiscard]] size_type capacity() const noexcept
    {
        return mCapacity;
    }

    [[nodiscard]] size_type max_size() const noexcept
    {
        return std::numeric_limits<size_type>::max() / sizeof(T);
    }

    void reserve(size_type new_cap)
    {
        if (new_cap > mCapacity)
        {
            reallocate(new_cap);
        }
    }

    void shrink_to_fit()
    {
        if (mCapacity > mSize && mSize > 0)
        {
            reallocate(mSize);
        }
    }

    // =========================================================================
    // Modifiers
    // =========================================================================

    void clear() noexcept
    {
        destroy_range(data_, data_ + mSize);
        mSize = 0;
    }

    void push_back(const T& value)
    {
        if (mSize >= mCapacity)
        {
            size_type new_cap = mCapacity == 0 ? 1 : mCapacity * 2;
            reallocate(new_cap);
        }
        new (data_ + mSize) T(value);
        ++mSize;
    }

    void push_back(T&& value)
    {
        if (mSize >= mCapacity)
        {
            size_type new_cap = mCapacity == 0 ? 1 : mCapacity * 2;
            reallocate(new_cap);
        }
        new (data_ + mSize) T(std::move(value));
        ++mSize;
    }

    template <typename... Args>
    reference emplace_back(Args&&... args)
    {
        if (mSize >= mCapacity)
        {
            size_type new_cap = mCapacity == 0 ? 1 : mCapacity * 2;
            reallocate(new_cap);
        }
        new (data_ + mSize) T(std::forward<Args>(args)...);
        return data_[mSize++];
    }

    void pop_back() noexcept
    {
        if (mSize > 0)
        {
            --mSize;
            data_[mSize].~T();
        }
    }

    void resize(size_type count)
    {
        if (count > mSize)
        {
            if (count > mCapacity)
            {
                reallocate(count);
            }
            if constexpr (std::is_trivially_default_constructible_v<T>)
            {
                std::memset(data_ + mSize, 0, (count - mSize) * sizeof(T));
            }
            else
            {
                for (size_type i = mSize; i < count; ++i)
                {
                    new (data_ + i) T();
                }
            }
        }
        else if (count < mSize)
        {
            destroy_range(data_ + count, data_ + mSize);
        }
        mSize = count;
    }

    void resize(size_type count, const T& value)
    {
        if (count > mSize)
        {
            if (count > mCapacity)
            {
                reallocate(count);
            }
            for (size_type i = mSize; i < count; ++i)
            {
                new (data_ + i) T(value);
            }
        }
        else if (count < mSize)
        {
            destroy_range(data_ + count, data_ + mSize);
        }
        mSize = count;
    }

    // =========================================================================
    // Insert Operations
    // =========================================================================

    /**
     * @brief Insert a single element at the specified position
     * @param pos Iterator to the position before which the element will be inserted
     * @param value Element value to insert
     * @return Iterator to the inserted element
     */
    iterator insert(const_iterator pos, const T& value)
    {
        size_type index = static_cast<size_type>(pos - data_);
        if (mSize >= mCapacity)
        {
            size_type new_cap = mCapacity == 0 ? 1 : mCapacity * 2;
            reallocate(new_cap);
        }

        // Shift elements to make room
        if (index < mSize)
        {
            if constexpr (std::is_trivially_copyable_v<T>)
            {
                std::memmove(data_ + index + 1, data_ + index, (mSize - index) * sizeof(T));
            }
            else
            {
                for (size_type i = mSize; i > index; --i)
                {
                    new (data_ + i) T(std::move(data_[i - 1]));
                    data_[i - 1].~T();
                }
            }
        }

        new (data_ + index) T(value);
        ++mSize;
        return data_ + index;
    }

    /**
     * @brief Insert a single element (move) at the specified position
     * @param pos Iterator to the position before which the element will be inserted
     * @param value Element value to insert (moved)
     * @return Iterator to the inserted element
     */
    iterator insert(const_iterator pos, T&& value)
    {
        size_type index = static_cast<size_type>(pos - data_);
        if (mSize >= mCapacity)
        {
            size_type new_cap = mCapacity == 0 ? 1 : mCapacity * 2;
            reallocate(new_cap);
        }

        // Shift elements to make room
        if (index < mSize)
        {
            if constexpr (std::is_trivially_copyable_v<T>)
            {
                std::memmove(data_ + index + 1, data_ + index, (mSize - index) * sizeof(T));
            }
            else
            {
                for (size_type i = mSize; i > index; --i)
                {
                    new (data_ + i) T(std::move(data_[i - 1]));
                    data_[i - 1].~T();
                }
            }
        }

        new (data_ + index) T(std::move(value));
        ++mSize;
        return data_ + index;
    }

    /**
     * @brief Insert elements from a range at the specified position
     * @tparam InputIt Input iterator type
     * @param pos Iterator to the position before which the elements will be inserted
     * @param first Iterator to the first element to insert
     * @param last Iterator past the last element to insert
     * @return Iterator to the first inserted element, or pos if first == last
     */
    template <std::input_iterator InputIt>
    iterator insert(const_iterator pos, InputIt first, InputIt last)
    {
        size_type index = static_cast<size_type>(pos - data_);
        size_type count = static_cast<size_type>(std::distance(first, last));

        if (count == 0)
        {
            return data_ + index;
        }

        // Ensure capacity
        if (mSize + count > mCapacity)
        {
            size_type new_cap = std::max(mCapacity * 2, mSize + count);
            reallocate(new_cap);
        }

        // Shift existing elements to make room
        if (index < mSize)
        {
            if constexpr (std::is_trivially_copyable_v<T>)
            {
                std::memmove(data_ + index + count, data_ + index, (mSize - index) * sizeof(T));
            }
            else
            {
                // Move from end to avoid overwriting
                for (size_type i = mSize; i > index; --i)
                {
                    new (data_ + i - 1 + count) T(std::move(data_[i - 1]));
                    data_[i - 1].~T();
                }
            }
        }

        // Copy new elements into the gap
        size_type insert_idx = index;
        for (InputIt it = first; it != last; ++it, ++insert_idx)
        {
            new (data_ + insert_idx) T(*it);
        }

        mSize += count;
        return data_ + index;
    }

    /**
     * @brief Insert count copies of value at the specified position
     * @param pos Iterator to the position before which the elements will be inserted
     * @param count Number of elements to insert
     * @param value Element value to insert
     * @return Iterator to the first inserted element, or pos if count == 0
     */
    iterator insert(const_iterator pos, size_type count, const T& value)
    {
        size_type index = static_cast<size_type>(pos - data_);

        if (count == 0)
        {
            return data_ + index;
        }

        // Ensure capacity
        if (mSize + count > mCapacity)
        {
            size_type new_cap = std::max(mCapacity * 2, mSize + count);
            reallocate(new_cap);
        }

        // Shift existing elements
        if (index < mSize)
        {
            if constexpr (std::is_trivially_copyable_v<T>)
            {
                std::memmove(data_ + index + count, data_ + index, (mSize - index) * sizeof(T));
            }
            else
            {
                for (size_type i = mSize; i > index; --i)
                {
                    new (data_ + i - 1 + count) T(std::move(data_[i - 1]));
                    data_[i - 1].~T();
                }
            }
        }

        // Fill with value
        for (size_type i = 0; i < count; ++i)
        {
            new (data_ + index + i) T(value);
        }

        mSize += count;
        return data_ + index;
    }

    /**
     * @brief Insert elements from initializer list at the specified position
     * @param pos Iterator to the position before which the elements will be inserted
     * @param ilist Initializer list with elements to insert
     * @return Iterator to the first inserted element
     */
    iterator insert(const_iterator pos, std::initializer_list<T> ilist)
    {
        return insert(pos, ilist.begin(), ilist.end());
    }

    void swap(HpcVector& other) noexcept
    {
        std::swap(mAllocator, other.mAllocator);
        std::swap(data_, other.data_);
        std::swap(mSize, other.mSize);
        std::swap(mCapacity, other.mCapacity);
    }

    // =========================================================================
    // Allocator Access
    // =========================================================================

    [[nodiscard]] allocator_type get_allocator() const noexcept
    {
        return mAllocator;
    }
};

// Non-member swap
template <typename T, std::size_t A, typename P>
void swap(HpcVector<T, A, P>& lhs, HpcVector<T, A, P>& rhs) noexcept
{
    lhs.swap(rhs);
}

// Comparison operators
template <typename T, std::size_t A, typename P>
bool operator==(const HpcVector<T, A, P>& lhs, const HpcVector<T, A, P>& rhs)
{
    if (lhs.size() != rhs.size())
    {
        return false;
    }
    return std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

template <typename T, std::size_t A, typename P>
bool operator!=(const HpcVector<T, A, P>& lhs, const HpcVector<T, A, P>& rhs)
{
    return !(lhs == rhs);
}

template <typename T, std::size_t A, typename P>
bool operator<(const HpcVector<T, A, P>& lhs, const HpcVector<T, A, P>& rhs)
{
    return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

template <typename T, std::size_t A, typename P>
bool operator<=(const HpcVector<T, A, P>& lhs, const HpcVector<T, A, P>& rhs)
{
    return !(rhs < lhs);
}

template <typename T, std::size_t A, typename P>
bool operator>(const HpcVector<T, A, P>& lhs, const HpcVector<T, A, P>& rhs)
{
    return rhs < lhs;
}

template <typename T, std::size_t A, typename P>
bool operator>=(const HpcVector<T, A, P>& lhs, const HpcVector<T, A, P>& rhs)
{
    return !(lhs < rhs);
}

// =============================================================================
// Type Traits
// =============================================================================

/// Detect if a type is an HpcVector
template <typename T>
struct is_hpc_vector : std::false_type
{
};

template <typename T, std::size_t A, typename P>
struct is_hpc_vector<HpcVector<T, A, P>> : std::true_type
{
};

template <typename T>
inline constexpr bool is_hpc_vector_v = is_hpc_vector<T>::value;

// =============================================================================
// Convenience Aliases
// =============================================================================

/// HpcVector with local NUMA allocation (default)
template <typename T, std::size_t Alignment = 64>
using HpcLocalVector = HpcVector<T, Alignment, memory::NumaLocalPolicy>;

/// HpcVector with specific NUMA node allocation
template <typename T, std::size_t Alignment = 64>
using HpcPreferredVector = HpcVector<T, Alignment, memory::NumaPreferredPolicy>;

/// HpcVector with interleaved NUMA allocation
template <typename T, std::size_t Alignment = 64>
using HpcInterleavedVector = HpcVector<T, Alignment, memory::NumaInterleavedPolicy>;

} // namespace fat_p
