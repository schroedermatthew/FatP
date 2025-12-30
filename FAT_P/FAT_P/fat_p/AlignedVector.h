/**
 * @file AlignedVector.h
 * @brief Cache-aware aligned vector container for HPC workloads
 *
 * @layer Infrastructure
 *
 * @details Drop-in replacement for std::vector with explicit memory alignment control.
 * Optimized for SIMD operations and cache-line awareness to prevent false sharing.
 *
 * Key Features:
 * - Configurable memory alignment (16, 32, 64, 128 bytes)
 * - SIMD-friendly data layout
 * - Cache-line aligned allocations
 * - Compatible with std::vector interface
 * - Move semantics support
 * - Exception-safe operations
 * - Zero-overhead when alignment == alignof(T)
 *
 * @note Thread-safety: NOT thread-safe. Caller must synchronize for concurrent access.
 *
 * @see HpcVector.h for NUMA-aware variant
 * @see CheckedArithmetic.h for integration with checked vector operations
 */

#pragma once

// MSVC warning C4702: unreachable code
// This is a false positive triggered by if constexpr discarded branches.
// GCC/Clang don't warn about this (correctly).
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4702)
#endif

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <algorithm>
#include <iterator>
#include <type_traits>
#include <initializer_list>
#include <cstring>
#include <limits>

#include "FatPTypeTraits.h"

namespace fat_p {

// =============================================================================
// Aligned Allocator
// =============================================================================

/**
 * @brief STL-compatible allocator with configurable alignment
 *
 * @tparam T Value type
 * @tparam Alignment Memory alignment in bytes (must be power of 2)
 *
 * @note Thread-safety: NOT thread-safe. Each container should have its own allocator.
 */
template<typename T, size_t Alignment = 64>
class AlignedAllocator
{
public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = T*;
    using const_pointer = const T*;

    static_assert((Alignment & (Alignment - 1)) == 0, "Alignment must be power of 2");
    static_assert(Alignment >= alignof(T), "Alignment must be at least alignof(T)");
    static_assert(Alignment >= alignof(void*),
                  "Alignment must be at least alignof(void*) for posix_memalign/_aligned_malloc");

    static constexpr size_t alignment = Alignment;

    AlignedAllocator() noexcept = default;

    template<typename U>
    AlignedAllocator(const AlignedAllocator<U, Alignment>&) noexcept
    {
    }

    /**
     * @brief Allocate aligned memory
     *
     * @param n Number of elements to allocate
     * @return Pointer to aligned memory block
     * @throws std::bad_alloc if allocation fails or overflow detected
     *
     * @note Complexity: O(1)
     */
    [[nodiscard]] T* allocate(size_t n)
    {
        if (n == 0)
        {
            return nullptr;
        }

        if (n > std::numeric_limits<size_t>::max() / sizeof(T))
        {
            throw std::bad_alloc();
        }

        size_t size = n * sizeof(T);
        void* ptr = nullptr;

#if defined(_MSC_VER)
        ptr = _aligned_malloc(size, Alignment);
        if (!ptr)
        {
            throw std::bad_alloc();
        }
#else
        if (posix_memalign(&ptr, Alignment, size) != 0)
        {
            throw std::bad_alloc();
        }
#endif

        return static_cast<T*>(ptr);
    }

    /**
     * @brief Deallocate aligned memory
     *
     * @param ptr Pointer to memory block (may be nullptr)
     *
     * @note Complexity: O(1)
     */
    void deallocate(T* ptr, size_t) noexcept
    {
        if (!ptr)
        {
            return;
        }

#if defined(_MSC_VER)
        _aligned_free(ptr);
#else
        free(ptr);
#endif
    }

    template<typename U>
    struct rebind
    {
        using other = AlignedAllocator<U, Alignment>;
    };
};

template<typename T1, size_t A1, typename T2, size_t A2>
bool operator==(const AlignedAllocator<T1, A1>&, const AlignedAllocator<T2, A2>&) noexcept
{
    return A1 == A2;
}

template<typename T1, size_t A1, typename T2, size_t A2>
bool operator!=(const AlignedAllocator<T1, A1>&, const AlignedAllocator<T2, A2>&) noexcept
{
    return A1 != A2;
}

// =============================================================================
// Aligned Vector
// =============================================================================

/**
 * @brief Cache-aware vector with configurable alignment
 *
 * @tparam T Element type
 * @tparam Alignment Memory alignment in bytes (default: 64 for cache line)
 *
 * @note Thread-safety: NOT thread-safe. Caller must synchronize for concurrent access.
 *
 * @see AlignedAllocator for allocation details
 */
template<typename T, size_t Alignment = 64>
class AlignedVector
{
public:
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
    using allocator_type = AlignedAllocator<T, Alignment>;

    static constexpr size_t alignment = Alignment;

private:
    allocator_type mAllocator;
    pointer mData;
    size_type mSize;
    size_type mCapacity;

    /**
     * @brief Calculate safe growth capacity with overflow check
     */
    [[nodiscard]] size_type safeGrowCapacity(size_type minCapacity) const
    {
        constexpr size_type maxCap = std::numeric_limits<size_type>::max() / sizeof(T);
        if (minCapacity > maxCap)
        {
            throw std::length_error("AlignedVector: capacity overflow");
        }
        size_type newCap = (mCapacity == 0) ? 1 : mCapacity;
        if (newCap <= maxCap / 2)
        {
            newCap *= 2;
        }
        else
        {
            newCap = maxCap;
        }
        return std::max(newCap, minCapacity);
    }

    /**
     * @brief Destroy elements in range [first, last)
     */
    void destroyRange(pointer first, pointer last) noexcept
    {
        if constexpr (!std::is_trivially_destructible_v<T>)
        {
            for (; first != last; ++first)
            {
                first->~T();
            }
        }
    }

    /**
     * @brief Construct elements by copying with exception safety
     *
     * If copy construction throws at element N, elements 0..N-1 are destroyed.
     */
    void constructRangeCopy(pointer dest, const_pointer src, size_type count)
    {
        if constexpr (std::is_trivially_copyable_v<T>)
        {
            std::memcpy(dest, src, count * sizeof(T));
        }
        else
        {
            size_type constructed = 0;
            try
            {
                for (; constructed < count; ++constructed)
                {
                    new (dest + constructed) T(src[constructed]);
                }
            }
            catch (...)
            {
                destroyRange(dest, dest + constructed);
                throw;
            }
        }
    }

    /**
     * @brief Construct elements by moving with exception safety
     *
     * If move construction throws at element N, elements 0..N-1 are destroyed.
     */
    void constructRangeMove(pointer dest, pointer src, size_type count)
    {
        if constexpr (std::is_trivially_copyable_v<T>)
        {
            std::memcpy(dest, src, count * sizeof(T));
        }
        else
        {
            size_type constructed = 0;
            try
            {
                for (; constructed < count; ++constructed)
                {
                    new (dest + constructed) T(std::move(src[constructed]));
                }
            }
            catch (...)
            {
                destroyRange(dest, dest + constructed);
                throw;
            }
        }
    }

    /**
     * @brief Check if pointer references element within this vector
     */
    [[nodiscard]] bool isInternalReference(const T* ptr) const noexcept
    {
        return ptr >= mData && ptr < mData + mSize;
    }

    /**
     * @brief Exception-safe reallocation
     *
     * If move construction throws at element N, elements 0..N-1 are properly
     * destroyed before re-throwing. Prevents resource leaks for non-trivial types.
     */
    void reallocate(size_type newCapacity)
    {
        pointer newData = mAllocator.allocate(newCapacity);
        size_type constructedCount = 0;

        try
        {
            if constexpr (std::is_trivially_copyable_v<T>)
            {
                std::memcpy(newData, mData, mSize * sizeof(T));
                constructedCount = mSize;
            }
            else
            {
                for (; constructedCount < mSize; ++constructedCount)
                {
                    if constexpr (std::is_nothrow_move_constructible_v<T> ||
                                  !std::is_copy_constructible_v<T>)
                    {
                        new (newData + constructedCount) T(std::move(mData[constructedCount]));
                    }
                    else
                    {
                        new (newData + constructedCount) T(mData[constructedCount]);
                    }
                }
            }
        }
        catch (...)
        {
            destroyRange(newData, newData + constructedCount);
            mAllocator.deallocate(newData, newCapacity);
            throw;
        }

        destroyRange(mData, mData + mSize);
        mAllocator.deallocate(mData, mCapacity);

        mData = newData;
        mCapacity = newCapacity;
    }

public:
    // =========================================================================
    // Constructors
    // =========================================================================

    /**
     * @brief Default constructor - creates empty vector
     *
     * @note Complexity: O(1)
     */
    AlignedVector() noexcept
        : mAllocator()
        , mData(nullptr)
        , mSize(0)
        , mCapacity(0)
    {
    }

    /**
     * @brief Construct vector with count value-initialized elements
     *
     * @param count Number of elements
     * @throws std::bad_alloc if allocation fails
     *
     * @note Complexity: O(count)
     */
    explicit AlignedVector(size_type count)
        : mAllocator()
        , mData(nullptr)
        , mSize(0)
        , mCapacity(0)
    {
        if (count > 0)
        {
            mData = mAllocator.allocate(count);
            mCapacity = count;

            if constexpr (std::is_trivially_default_constructible_v<T>)
            {
                std::memset(mData, 0, count * sizeof(T));
                mSize = count;
            }
            else
            {
                try
                {
                    for (mSize = 0; mSize < count; ++mSize)
                    {
                        new (mData + mSize) T();
                    }
                }
                catch (...)
                {
                    destroyRange(mData, mData + mSize);
                    mAllocator.deallocate(mData, mCapacity);
                    throw;
                }
            }
        }
    }

    /**
     * @brief Construct vector with count copies of value
     *
     * @param count Number of elements
     * @param value Value to copy
     * @throws std::bad_alloc if allocation fails
     *
     * @note Complexity: O(count)
     */
    AlignedVector(size_type count, const T& value)
        : mAllocator()
        , mData(nullptr)
        , mSize(0)
        , mCapacity(0)
    {
        if (count > 0)
        {
            mData = mAllocator.allocate(count);
            mCapacity = count;

            try
            {
                for (mSize = 0; mSize < count; ++mSize)
                {
                    new (mData + mSize) T(value);
                }
            }
            catch (...)
            {
                destroyRange(mData, mData + mSize);
                mAllocator.deallocate(mData, mCapacity);
                throw;
            }
        }
    }

    /**
     * @brief Construct from initializer list
     *
     * @param init Initializer list
     * @throws std::bad_alloc if allocation fails
     *
     * @note Complexity: O(init.size())
     */
    AlignedVector(std::initializer_list<T> init)
        : mAllocator()
        , mData(nullptr)
        , mSize(0)
        , mCapacity(0)
    {
        if (init.size() > 0)
        {
            mData = mAllocator.allocate(init.size());
            mCapacity = init.size();

            try
            {
                for (const auto& value : init)
                {
                    new (mData + mSize) T(value);
                    ++mSize;
                }
            }
            catch (...)
            {
                destroyRange(mData, mData + mSize);
                mAllocator.deallocate(mData, mCapacity);
                throw;
            }
        }
    }

    /**
     * @brief Construct from iterator range
     *
     * @tparam InputIt Iterator type
     * @param first Iterator to first element
     * @param last Iterator past last element
     * @throws std::bad_alloc if allocation fails
     *
     * @note Complexity: O(distance(first, last))
     */
    template<typename InputIt,
             typename = std::enable_if_t<!std::is_integral_v<InputIt>>>
    AlignedVector(InputIt first, InputIt last)
        : mAllocator()
        , mData(nullptr)
        , mSize(0)
        , mCapacity(0)
    {
        if constexpr (std::is_base_of_v<std::random_access_iterator_tag,
                      typename std::iterator_traits<InputIt>::iterator_category>)
        {
            auto count = static_cast<size_type>(std::distance(first, last));
            if (count > 0)
            {
                mData = mAllocator.allocate(count);
                mCapacity = count;
                try
                {
                    for (; first != last; ++first, ++mSize)
                    {
                        new (mData + mSize) T(*first);
                    }
                }
                catch (...)
                {
                    destroyRange(mData, mData + mSize);
                    mAllocator.deallocate(mData, mCapacity);
                    throw;
                }
            }
        }
        else
        {
            for (; first != last; ++first)
            {
                push_back(*first);
            }
        }
    }

    /**
     * @brief Copy constructor
     *
     * @param other Vector to copy
     * @throws std::bad_alloc if allocation fails
     *
     * @note Complexity: O(other.size())
     */
    AlignedVector(const AlignedVector& other)
        : mAllocator()
        , mData(nullptr)
        , mSize(0)
        , mCapacity(0)
    {
        if (other.mSize > 0)
        {
            mData = mAllocator.allocate(other.mSize);
            mCapacity = other.mSize;
            try
            {
                constructRangeCopy(mData, other.mData, other.mSize);
                mSize = other.mSize;
            }
            catch (...)
            {
                mAllocator.deallocate(mData, mCapacity);
                throw;
            }
        }
    }

    /**
     * @brief Move constructor
     *
     * @param other Vector to move from (left empty)
     *
     * @note Complexity: O(1)
     */
    AlignedVector(AlignedVector&& other) noexcept
        : mAllocator(std::move(other.mAllocator))
        , mData(other.mData)
        , mSize(other.mSize)
        , mCapacity(other.mCapacity)
    {
        other.mData = nullptr;
        other.mSize = 0;
        other.mCapacity = 0;
    }

    /**
     * @brief Destructor
     *
     * @note Complexity: O(size())
     */
    ~AlignedVector()
    {
        destroyRange(mData, mData + mSize);
        mAllocator.deallocate(mData, mCapacity);
    }

    // =========================================================================
    // Assignment Operators
    // =========================================================================

    /**
     * @brief Copy assignment operator
     *
     * @param other Vector to copy
     * @return Reference to this
     *
     * @note Complexity: O(size() + other.size())
     * @note Exception safety: Strong guarantee (copy-and-swap)
     */
    AlignedVector& operator=(const AlignedVector& other)
    {
        if (this != &other)
        {
            AlignedVector temp(other);
            swap(temp);
        }
        return *this;
    }

    /**
     * @brief Move assignment operator
     *
     * @param other Vector to move from
     * @return Reference to this
     *
     * @note Complexity: O(size())
     */
    AlignedVector& operator=(AlignedVector&& other) noexcept
    {
        if (this != &other)
        {
            destroyRange(mData, mData + mSize);
            mAllocator.deallocate(mData, mCapacity);

            mAllocator = std::move(other.mAllocator);
            mData = other.mData;
            mSize = other.mSize;
            mCapacity = other.mCapacity;

            other.mData = nullptr;
            other.mSize = 0;
            other.mCapacity = 0;
        }
        return *this;
    }

    /**
     * @brief Initializer list assignment
     *
     * @param ilist Initializer list
     * @return Reference to this
     *
     * @note Complexity: O(size() + ilist.size())
     */
    AlignedVector& operator=(std::initializer_list<T> ilist)
    {
        assign(ilist.begin(), ilist.end());
        return *this;
    }

    // =========================================================================
    // Element Access
    // =========================================================================

    /**
     * @brief Access element by index (unchecked)
     *
     * @param pos Element index
     * @return Reference to element
     *
     * @pre pos < size()
     * @note Complexity: O(1)
     */
    reference operator[](size_type pos) noexcept
    {
        return mData[pos];
    }

    const_reference operator[](size_type pos) const noexcept
    {
        return mData[pos];
    }

    /**
     * @brief Access element by index (bounds-checked)
     *
     * @param pos Element index
     * @return Reference to element
     * @throws std::out_of_range if pos >= size()
     *
     * @note Complexity: O(1)
     */
    reference at(size_type pos)
    {
        if (pos >= mSize)
        {
            throw std::out_of_range("AlignedVector::at");
        }
        return mData[pos];
    }

    const_reference at(size_type pos) const
    {
        if (pos >= mSize)
        {
            throw std::out_of_range("AlignedVector::at");
        }
        return mData[pos];
    }

    /**
     * @brief Access first element
     *
     * @return Reference to first element
     *
     * @pre size() > 0
     * @note Complexity: O(1)
     */
    reference front() noexcept
    {
        assert(mSize > 0 && "front() called on empty AlignedVector");
        return mData[0];
    }

    const_reference front() const noexcept
    {
        assert(mSize > 0 && "front() called on empty AlignedVector");
        return mData[0];
    }

    /**
     * @brief Access last element
     *
     * @return Reference to last element
     *
     * @pre size() > 0
     * @note Complexity: O(1)
     */
    reference back() noexcept
    {
        assert(mSize > 0 && "back() called on empty AlignedVector");
        return mData[mSize - 1];
    }

    const_reference back() const noexcept
    {
        assert(mSize > 0 && "back() called on empty AlignedVector");
        return mData[mSize - 1];
    }

    /**
     * @brief Direct access to underlying array
     *
     * @return Pointer to data (nullptr if empty)
     *
     * @note Complexity: O(1)
     */
    [[nodiscard]] pointer data() noexcept
    {
        return mData;
    }

    [[nodiscard]] const_pointer data() const noexcept
    {
        return mData;
    }

    /**
     * @brief Get pointer with compiler alignment hint
     *
     * Returns a pointer with __builtin_assume_aligned hint, enabling
     * the compiler to generate aligned SIMD instructions.
     *
     * This method is detected by has_assume_aligned_v<AlignedVector<T>>
     * from CheckedArithmeticBase.h, enabling optimized stores in
     * checked vector operations.
     *
     * @return Pointer with Alignment hint for compiler optimization
     *
     * @note Complexity: O(1)
     */
    [[nodiscard]] pointer assume_aligned() noexcept
    {
#if defined(__GNUC__) || defined(__clang__)
        return static_cast<pointer>(__builtin_assume_aligned(mData, Alignment));
#elif defined(_MSC_VER)
        __assume((reinterpret_cast<std::uintptr_t>(mData) % Alignment) == 0);
        return mData;
#else
        return mData;
#endif
    }

    [[nodiscard]] const_pointer assume_aligned() const noexcept
    {
#if defined(__GNUC__) || defined(__clang__)
        return static_cast<const_pointer>(__builtin_assume_aligned(mData, Alignment));
#elif defined(_MSC_VER)
        __assume((reinterpret_cast<std::uintptr_t>(mData) % Alignment) == 0);
        return mData;
#else
        return mData;
#endif
    }

    // =========================================================================
    // Iterators
    // =========================================================================

    iterator begin() noexcept { return mData; }
    const_iterator begin() const noexcept { return mData; }
    const_iterator cbegin() const noexcept { return mData; }

    iterator end() noexcept { return mData + mSize; }
    const_iterator end() const noexcept { return mData + mSize; }
    const_iterator cend() const noexcept { return mData + mSize; }

    reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
    const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(end()); }

    reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
    const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
    const_reverse_iterator crend() const noexcept { return const_reverse_iterator(begin()); }

    // =========================================================================
    // Capacity
    // =========================================================================

    /**
     * @brief Check if vector is empty
     * @return true if size() == 0
     * @note Complexity: O(1)
     */
    [[nodiscard]] bool empty() const noexcept
    {
        return mSize == 0;
    }

    /**
     * @brief Get number of elements
     * @return Current element count
     * @note Complexity: O(1)
     */
    [[nodiscard]] size_type size() const noexcept
    {
        return mSize;
    }

    /**
     * @brief Get current capacity
     * @return Number of elements that can be held without reallocation
     * @note Complexity: O(1)
     */
    [[nodiscard]] size_type capacity() const noexcept
    {
        return mCapacity;
    }

    /**
     * @brief Get maximum possible size
     * @return Maximum number of elements
     * @note Complexity: O(1)
     */
    [[nodiscard]] size_type max_size() const noexcept
    {
        return std::numeric_limits<size_type>::max() / sizeof(T);
    }

    /**
     * @brief Get allocator
     * @return Copy of allocator
     * @note Complexity: O(1)
     */
    [[nodiscard]] allocator_type get_allocator() const noexcept
    {
        return mAllocator;
    }

    /**
     * @brief Reserve capacity
     *
     * @param newCapacity Minimum capacity to reserve
     * @throws std::bad_alloc if allocation fails
     *
     * @note Complexity: O(size()) if reallocation occurs, O(1) otherwise
     */
    void reserve(size_type newCapacity)
    {
        if (newCapacity > mCapacity)
        {
            reallocate(newCapacity);
        }
    }

    /**
     * @brief Shrink capacity to fit size
     *
     * @note Complexity: O(size()) if reallocation occurs, O(1) otherwise
     */
    void shrink_to_fit()
    {
        if (mSize < mCapacity)
        {
            if (mSize == 0)
            {
                mAllocator.deallocate(mData, mCapacity);
                mData = nullptr;
                mCapacity = 0;
            }
            else
            {
                reallocate(mSize);
            }
        }
    }

    // =========================================================================
    // Modifiers
    // =========================================================================

    /**
     * @brief Clear all elements
     *
     * @note Complexity: O(size())
     * @note Does not deallocate memory
     */
    void clear() noexcept
    {
        destroyRange(mData, mData + mSize);
        mSize = 0;
    }

    /**
     * @brief Assign count copies of value
     *
     * @param count Number of elements
     * @param value Value to copy
     *
     * @note Complexity: O(size() + count)
     * @note Exception safety: Strong guarantee
     */
    void assign(size_type count, const T& value)
    {
        // Handle aliasing: if value is inside this vector, copy first
        const T* valPtr = std::addressof(value);
        if (isInternalReference(valPtr))
        {
            T valueCopy = value;
            assign(count, valueCopy);
            return;
        }

        // Build new vector, then swap (strong exception guarantee)
        AlignedVector temp;
        if (count > 0)
        {
            temp.mData = temp.mAllocator.allocate(count);
            temp.mCapacity = count;
            try
            {
                for (temp.mSize = 0; temp.mSize < count; ++temp.mSize)
                {
                    new (temp.mData + temp.mSize) T(value);
                }
            }
            catch (...)
            {
                temp.destroyRange(temp.mData, temp.mData + temp.mSize);
                temp.mAllocator.deallocate(temp.mData, temp.mCapacity);
                temp.mData = nullptr;
                temp.mSize = 0;
                temp.mCapacity = 0;
                throw;
            }
        }
        swap(temp);
    }

    /**
     * @brief Assign from iterator range
     *
     * @tparam InputIt Iterator type
     * @param first Iterator to first element
     * @param last Iterator past last element
     *
     * @note Complexity: O(size() + distance(first, last))
     * @note Exception safety: Strong guarantee
     */
    template<typename InputIt,
             typename = std::enable_if_t<!std::is_integral_v<InputIt>>>
    void assign(InputIt first, InputIt last)
    {
        // Build new vector, then swap (strong exception guarantee)
        AlignedVector temp(first, last);
        swap(temp);
    }

    /**
     * @brief Assign from initializer list
     *
     * @param ilist Initializer list
     *
     * @note Complexity: O(size() + ilist.size())
     */
    void assign(std::initializer_list<T> ilist)
    {
        assign(ilist.begin(), ilist.end());
    }

    /**
     * @brief Insert single element at position (lvalue)
     *
     * @param pos Position to insert at
     * @param value Value to insert
     * @return Iterator to inserted element
     *
     * @note Complexity: O(size())
     * @note Exception safety: Basic guarantee (leak-free even if move assignment throws)
     */
    iterator insert(const_iterator pos, const T& value)
    {
        size_type index = pos - mData;

        // Handle aliasing: if value is inside this vector, copy first
        const T* valPtr = std::addressof(value);
        if (isInternalReference(valPtr))
        {
            T valueCopy = value;
            return insert(mData + index, std::move(valueCopy));
        }

        if (mSize == mCapacity)
        {
            size_type newCapacity = safeGrowCapacity(mSize + 1);
            reallocate(newCapacity);
        }

        if (index < mSize)
        {
            // Construct tail element in uninitialized memory
            new (mData + mSize) T(std::move(mData[mSize - 1]));
            try
            {
                // Shift elements (may throw)
                for (size_type i = mSize - 1; i > index; --i)
                {
                    mData[i] = std::move(mData[i - 1]);
                }
                mData[index] = value;
            }
            catch (...)
            {
                // Destroy the tail element we constructed before rethrowing
                destroyRange(mData + mSize, mData + mSize + 1);
                throw;
            }
        }
        else
        {
            new (mData + mSize) T(value);
        }
        ++mSize;
        return mData + index;
    }

    /**
     * @brief Insert single element at position (rvalue)
     *
     * @param pos Position to insert at
     * @param value Value to move-insert
     * @return Iterator to inserted element
     *
     * @note Complexity: O(size())
     * @note Exception safety: Basic guarantee (leak-free even if move assignment throws)
     */
    iterator insert(const_iterator pos, T&& value)
    {
        size_type index = pos - mData;

        // Handle aliasing
        const T* valPtr = std::addressof(value);
        if (isInternalReference(valPtr))
        {
            T valueCopy = std::move(value);
            return insert(mData + index, std::move(valueCopy));
        }

        if (mSize == mCapacity)
        {
            size_type newCapacity = safeGrowCapacity(mSize + 1);
            reallocate(newCapacity);
        }

        if (index < mSize)
        {
            // Construct tail element in uninitialized memory
            new (mData + mSize) T(std::move(mData[mSize - 1]));
            try
            {
                // Shift elements (may throw)
                for (size_type i = mSize - 1; i > index; --i)
                {
                    mData[i] = std::move(mData[i - 1]);
                }
                mData[index] = std::move(value);
            }
            catch (...)
            {
                // Destroy the tail element we constructed before rethrowing
                destroyRange(mData + mSize, mData + mSize + 1);
                throw;
            }
        }
        else
        {
            new (mData + mSize) T(std::move(value));
        }
        ++mSize;
        return mData + index;
    }

    /**
     * @brief Insert count copies of value at position
     *
     * @param pos Position to insert at
     * @param count Number of copies
     * @param value Value to copy
     * @return Iterator to first inserted element
     *
     * @note Complexity: O(size() + count)
     * @note Exception safety: Basic guarantee
     */
    iterator insert(const_iterator pos, size_type count, const T& value)
    {
        if (count == 0)
        {
            return const_cast<iterator>(pos);
        }

        size_type index = pos - mData;

        // Handle aliasing: if value is inside this vector, copy first
        const T* valPtr = std::addressof(value);
        if (isInternalReference(valPtr))
        {
            T valueCopy = value;
            return insert(mData + index, count, valueCopy);
        }

        if (mSize + count > mCapacity)
        {
            size_type newCapacity = safeGrowCapacity(mSize + count);
            reallocate(newCapacity);
        }

        if (index < mSize)
        {
            // Track what we construct in uninitialized memory for cleanup
            size_type tailConstructed = 0;
            try
            {
                // Phase 1: Move tail elements into uninitialized space
                size_type toConstruct = std::min(count, mSize - index);
                for (size_type i = 0; i < toConstruct; ++i)
                {
                    new (mData + mSize + count - 1 - i) T(std::move(mData[mSize - 1 - i]));
                    ++tailConstructed;
                }

                // Phase 2: Shift remaining elements within initialized space
                for (size_type i = mSize - toConstruct; i > index; --i)
                {
                    mData[i + count - 1] = std::move(mData[i - 1]);
                }

                // Phase 3: Fill the gap
                size_type gapConstructed = 0;
                try
                {
                    for (size_type i = 0; i < count; ++i)
                    {
                        if (index + i < mSize)
                        {
                            mData[index + i] = value;
                        }
                        else
                        {
                            new (mData + index + i) T(value);
                            ++gapConstructed;
                        }
                    }
                }
                catch (...)
                {
                    // Destroy gap elements we just constructed
                    destroyRange(mData + mSize, mData + mSize + gapConstructed);
                    throw;
                }
            }
            catch (...)
            {
                // Destroy tail elements we constructed
                destroyRange(mData + mSize + count - tailConstructed,
                             mData + mSize + count);
                throw;
            }
        }
        else
        {
            // Appending at end
            size_type constructed = 0;
            try
            {
                for (size_type i = 0; i < count; ++i)
                {
                    new (mData + mSize + i) T(value);
                    ++constructed;
                }
            }
            catch (...)
            {
                destroyRange(mData + mSize, mData + mSize + constructed);
                throw;
            }
        }
        mSize += count;
        return mData + index;
    }

    /**
     * @brief Insert from iterator range
     *
     * @tparam InputIt Iterator type
     * @param pos Position to insert at
     * @param first Iterator to first element
     * @param last Iterator past last element
     * @return Iterator to first inserted element
     *
     * @note Complexity: O(size() + distance(first, last))
     * @note Exception safety: Basic guarantee
     * @note Handles self-insertion safely
     */
    template<typename InputIt,
             typename = std::enable_if_t<!std::is_integral_v<InputIt>>>
    iterator insert(const_iterator pos, InputIt first, InputIt last)
    {
        size_type index = pos - mData;

        if constexpr (std::is_base_of_v<std::random_access_iterator_tag,
                      typename std::iterator_traits<InputIt>::iterator_category>)
        {
            size_type count = static_cast<size_type>(std::distance(first, last));
            if (count == 0)
            {
                return mData + index;
            }

            // Self-insertion check: if iterators point into this vector,
            // copy to temporary to avoid use-after-free on reallocation
            if constexpr (std::is_same_v<InputIt, iterator> ||
                          std::is_same_v<InputIt, const_iterator>)
            {
                if (isInternalReference(first))
                {
                    AlignedVector temp(first, last);
                    return insert(mData + index, temp.begin(), temp.end());
                }
            }

            if (mSize + count > mCapacity)
            {
                size_type newCapacity = safeGrowCapacity(mSize + count);
                reallocate(newCapacity);
            }

            if (index < mSize)
            {
                size_type tailConstructed = 0;
                try
                {
                    // Phase 1: Move tail into uninitialized space
                    size_type toConstruct = std::min(count, mSize - index);
                    for (size_type i = 0; i < toConstruct; ++i)
                    {
                        new (mData + mSize + count - 1 - i) T(std::move(mData[mSize - 1 - i]));
                        ++tailConstructed;
                    }

                    // Phase 2: Shift remaining elements
                    for (size_type i = mSize - toConstruct; i > index; --i)
                    {
                        mData[i + count - 1] = std::move(mData[i - 1]);
                    }

                    // Phase 3: Copy input range into gap
                    size_type gapConstructed = 0;
                    try
                    {
                        size_type i = 0;
                        for (; first != last; ++first, ++i)
                        {
                            if (index + i < mSize)
                            {
                                mData[index + i] = *first;
                            }
                            else
                            {
                                new (mData + index + i) T(*first);
                                ++gapConstructed;
                            }
                        }
                    }
                    catch (...)
                    {
                        destroyRange(mData + mSize, mData + mSize + gapConstructed);
                        throw;
                    }
                }
                catch (...)
                {
                    destroyRange(mData + mSize + count - tailConstructed,
                                 mData + mSize + count);
                    throw;
                }
            }
            else
            {
                // Appending at end
                size_type constructed = 0;
                try
                {
                    for (; first != last; ++first)
                    {
                        new (mData + mSize + constructed) T(*first);
                        ++constructed;
                    }
                }
                catch (...)
                {
                    destroyRange(mData + mSize, mData + mSize + constructed);
                    throw;
                }
            }
            mSize += count;
            return mData + index;
        }
        else
        {
            // For forward iterators, insert one at a time
            size_type inserted = 0;
            for (; first != last; ++first, ++inserted)
            {
                insert(mData + index + inserted, *first);
            }
            return mData + index;
        }
    }

    /**
     * @brief Insert from initializer list
     *
     * @param pos Position to insert at
     * @param ilist Initializer list
     * @return Iterator to first inserted element
     *
     * @note Complexity: O(size() + ilist.size())
     */
    iterator insert(const_iterator pos, std::initializer_list<T> ilist)
    {
        return insert(pos, ilist.begin(), ilist.end());
    }

    /**
     * @brief Emplace element at position
     *
     * @tparam Args Constructor argument types
     * @param pos Position to emplace at
     * @param args Constructor arguments
     * @return Iterator to emplaced element
     *
     * @note Complexity: O(size())
     * @note Exception safety: Basic guarantee (leak-free even if move assignment throws)
     */
    template<typename... Args>
    iterator emplace(const_iterator pos, Args&&... args)
    {
        size_type index = pos - mData;

        if (mSize == mCapacity)
        {
            size_type newCapacity = safeGrowCapacity(mSize + 1);
            reallocate(newCapacity);
        }

        if (index < mSize)
        {
            // Construct the new element first to ensure exception safety.
            // If construction throws, vector state is unchanged.
            T temp(std::forward<Args>(args)...);

            // Construct tail element in uninitialized memory
            new (mData + mSize) T(std::move(mData[mSize - 1]));
            try
            {
                // Shift elements (may throw)
                for (size_type i = mSize - 1; i > index; --i)
                {
                    mData[i] = std::move(mData[i - 1]);
                }

                // Move temp into position (may throw)
                mData[index] = std::move(temp);
            }
            catch (...)
            {
                // Destroy the tail element we constructed before rethrowing
                destroyRange(mData + mSize, mData + mSize + 1);
                throw;
            }
        }
        else
        {
            new (mData + mSize) T(std::forward<Args>(args)...);
        }
        ++mSize;
        return mData + index;
    }

    /**
     * @brief Erase element at position
     *
     * @param pos Position to erase
     * @return Iterator to element after erased
     *
     * @note Complexity: O(size())
     */
    iterator erase(const_iterator pos)
    {
        size_type index = pos - mData;
        if (index >= mSize)
        {
            return end();
        }

        for (size_type i = index; i < mSize - 1; ++i)
        {
            mData[i] = std::move(mData[i + 1]);
        }
        --mSize;
        mData[mSize].~T();
        return mData + index;
    }

    /**
     * @brief Erase range [first, last)
     *
     * @param first Iterator to first element to erase
     * @param last Iterator past last element to erase
     * @return Iterator to element after erased range
     *
     * @note Complexity: O(size())
     */
    iterator erase(const_iterator first, const_iterator last)
    {
        size_type startIndex = first - mData;
        size_type endIndex = last - mData;

        if (startIndex >= mSize || startIndex >= endIndex)
        {
            return mData + startIndex;
        }

        size_type count = endIndex - startIndex;

        for (size_type i = startIndex; i + count < mSize; ++i)
        {
            mData[i] = std::move(mData[i + count]);
        }

        destroyRange(mData + mSize - count, mData + mSize);
        mSize -= count;
        return mData + startIndex;
    }

    /**
     * @brief Push element to back (lvalue)
     *
     * @param value Value to copy
     *
     * @note Complexity: O(1) amortized, O(size()) worst-case
     * @note Handles aliasing when value references internal element
     */
    void push_back(const T& value)
    {
        if (mSize == mCapacity)
        {
            // Handle aliasing: value might be inside this vector
            const T* valPtr = std::addressof(value);
            if (isInternalReference(valPtr))
            {
                T temp = value;
                size_type newCapacity = safeGrowCapacity(mSize + 1);
                reallocate(newCapacity);
                new (mData + mSize) T(std::move(temp));
                ++mSize;
                return;
            }
            size_type newCapacity = safeGrowCapacity(mSize + 1);
            reallocate(newCapacity);
        }
        new (mData + mSize) T(value);
        ++mSize;
    }

    /**
     * @brief Push element to back (rvalue)
     *
     * @param value Value to move
     *
     * @note Complexity: O(1) amortized, O(size()) worst-case
     */
    void push_back(T&& value)
    {
        if (mSize == mCapacity)
        {
            // Handle aliasing
            const T* valPtr = std::addressof(value);
            if (isInternalReference(valPtr))
            {
                T temp = std::move(value);
                size_type newCapacity = safeGrowCapacity(mSize + 1);
                reallocate(newCapacity);
                new (mData + mSize) T(std::move(temp));
                ++mSize;
                return;
            }
            size_type newCapacity = safeGrowCapacity(mSize + 1);
            reallocate(newCapacity);
        }
        new (mData + mSize) T(std::move(value));
        ++mSize;
    }

    /**
     * @brief Emplace element at back
     *
     * @tparam Args Constructor argument types
     * @param args Constructor arguments
     * @return Reference to emplaced element
     *
     * @note Complexity: O(1) amortized, O(size()) worst-case
     */
    template<typename... Args>
    reference emplace_back(Args&&... args)
    {
        if (mSize == mCapacity)
        {
            size_type newCapacity = safeGrowCapacity(mSize + 1);
            reallocate(newCapacity);
        }
        new (mData + mSize) T(std::forward<Args>(args)...);
        ++mSize;
        return mData[mSize - 1];
    }

    /**
     * @brief Remove last element
     *
     * @pre size() > 0
     * @note Complexity: O(1)
     */
    void pop_back() noexcept
    {
        if (mSize > 0)
        {
            --mSize;
            mData[mSize].~T();
        }
    }

    /**
     * @brief Resize to count elements (default-constructed)
     *
     * @param count New size
     *
     * @note Complexity: O(|size() - count|)
     * @note Exception safety: Basic guarantee
     */
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
                std::memset(mData + mSize, 0, (count - mSize) * sizeof(T));
                mSize = count;
            }
            else
            {
                size_type constructed = mSize;
                try
                {
                    for (; constructed < count; ++constructed)
                    {
                        new (mData + constructed) T();
                    }
                    mSize = count;
                }
                catch (...)
                {
                    destroyRange(mData + mSize, mData + constructed);
                    throw;
                }
            }
        }
        else
        {
            destroyRange(mData + count, mData + mSize);
            mSize = count;
        }
    }

    /**
     * @brief Resize to count elements (copy-constructed from value)
     *
     * @param count New size
     * @param value Value to copy for new elements
     *
     * @note Complexity: O(|size() - count|)
     * @note Exception safety: Basic guarantee
     */
    void resize(size_type count, const T& value)
    {
        if (count > mSize)
        {
            if (count > mCapacity)
            {
                // Handle aliasing
                const T* valPtr = std::addressof(value);
                if (isInternalReference(valPtr))
                {
                    T valueCopy = value;
                    resize(count, valueCopy);
                    return;
                }
                reallocate(count);
            }
            size_type constructed = mSize;
            try
            {
                for (; constructed < count; ++constructed)
                {
                    new (mData + constructed) T(value);
                }
                mSize = count;
            }
            catch (...)
            {
                destroyRange(mData + mSize, mData + constructed);
                throw;
            }
        }
        else
        {
            destroyRange(mData + count, mData + mSize);
            mSize = count;
        }
    }

    /**
     * @brief Swap contents with another vector
     *
     * @param other Vector to swap with
     *
     * @note Complexity: O(1)
     */
    void swap(AlignedVector& other) noexcept
    {
        std::swap(mAllocator, other.mAllocator);
        std::swap(mData, other.mData);
        std::swap(mSize, other.mSize);
        std::swap(mCapacity, other.mCapacity);
    }

    // =========================================================================
    // Alignment Information
    // =========================================================================

    /**
     * @brief Get alignment value
     * @return Alignment in bytes
     * @note Complexity: O(1)
     */
    static constexpr size_t get_alignment() noexcept
    {
        return Alignment;
    }

    /**
     * @brief Check if data pointer is properly aligned
     *
     * @return true if data() is aligned to Alignment bytes
     *
     * @note PORTABILITY: Uses pointer->uintptr_t conversion for alignment check.
     *       This assumes a flat address space with conventional pointer representation.
     *       Not intended for CHERI, capability-based, or exotic pointer models.
     *       On standard x86_64/AArch64 HPC targets, this is the idiomatic approach.
     *
     * @note Complexity: O(1)
     */
    [[nodiscard]] bool is_aligned() const noexcept
    {
        return mData == nullptr || (reinterpret_cast<uintptr_t>(mData) % Alignment) == 0;
    }
};

// =============================================================================
// Non-member Functions
// =============================================================================

template<typename T, size_t A>
void swap(AlignedVector<T, A>& lhs, AlignedVector<T, A>& rhs) noexcept
{
    lhs.swap(rhs);
}

template<typename T, size_t A>
bool operator==(const AlignedVector<T, A>& lhs, const AlignedVector<T, A>& rhs)
{
    return lhs.size() == rhs.size() && std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

template<typename T, size_t A>
bool operator!=(const AlignedVector<T, A>& lhs, const AlignedVector<T, A>& rhs)
{
    return !(lhs == rhs);
}

template<typename T, size_t A>
bool operator<(const AlignedVector<T, A>& lhs, const AlignedVector<T, A>& rhs)
{
    return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

template<typename T, size_t A>
bool operator<=(const AlignedVector<T, A>& lhs, const AlignedVector<T, A>& rhs)
{
    return !(rhs < lhs);
}

template<typename T, size_t A>
bool operator>(const AlignedVector<T, A>& lhs, const AlignedVector<T, A>& rhs)
{
    return rhs < lhs;
}

template<typename T, size_t A>
bool operator>=(const AlignedVector<T, A>& lhs, const AlignedVector<T, A>& rhs)
{
    return !(lhs < rhs);
}

// Type trait specialization
template<typename T, size_t Alignment>
struct is_aligned_vector<AlignedVector<T, Alignment>> : std::true_type {};

} // namespace fat_p

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
