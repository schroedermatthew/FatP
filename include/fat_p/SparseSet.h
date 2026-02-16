#pragma once

/*
FATP_META:
  meta_version: 1
  component: SparseSet
  file_role: public_header
  path: include/fat_p/SparseSet.h
  namespace: fat_p
  layer: Containers
  summary: Sparse set with dense iteration and O(1) average operations.
  api_stability: in_work
  related:
    docs_search: "SparseSet"
    tests:
      - components/SparseSet/tests/test_SparseSet.cpp
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
 * @file SparseSet.h
 * @brief Sparse set with dense iteration.
 *
 * SparseSet stores a set of integer indices with O(1) average-case insert,
 * erase, and contains operations. Active indices are stored densely for
 * cache-friendly iteration.
 *
 * Erase uses swap-with-back. Dense iteration order is not stable.
 */

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace fat_p
{

// ============================================================================
// SparseSet
// ============================================================================

/**
 * @brief Sparse set for integer indices with dense iteration.
 *
 * @tparam T Unsigned index type.
 *
 * @note Thread-safety: NOT thread-safe for concurrent writes. Caller must
 *       synchronize. Concurrent reads are safe.
 */
template <typename T = uint32_t>
class SparseSet
{
    static_assert(std::is_unsigned_v<T>, "T must be an unsigned integral type");

public:
    using value_type = T;
    using size_type = size_t;
    using iterator = typename std::vector<T>::iterator;
    using const_iterator = typename std::vector<T>::const_iterator;

    /**
     * @brief Constructs an empty set.
     *
     * @param maxValue Optional maximum supported value (inclusive). This
     *                 pre-sizes the sparse mapping.
     *
     * @note Complexity: O(maxValue) when growth is required; otherwise O(1).
     */
    explicit SparseSet(size_type maxValue = 0)
    {
        if (maxValue != 0)
        {
            reserve(maxValue);
        }
    }

    /**
     * @brief Inserts value if it is not present.
     *
     * @param value Value to insert.
     * @return true if insertion occurred; false if value was already present.
     * @throws std::length_error if value cannot be represented as an index.
     *
     * @note Complexity: O(1) amortized.
     * @note Exception-safety: Strong guarantee.
     */
    bool insert(T value)
    {
        const size_type sparseIndex = ensureSparseCapacity(value);

        if (containsAtIndex(value, sparseIndex))
        {
            return false;
        }

        const size_type denseIndex = mDense.size();
        enforceDenseIndexFits(denseIndex, "SparseSet::insert: dense index overflow");

        mDense.push_back(value);
        mSparse[sparseIndex] = static_cast<T>(denseIndex);
        return true;
    }

    /**
     * @brief Erases value if it is present.
     *
     * @param value Value to erase.
     * @return true if value was erased; false if it was not present.
     *
     * @note Complexity: O(1).
     */
    bool erase(T value) noexcept
    {
        size_type sparseIndex = 0;
        if (!tryToSparseIndex(value, sparseIndex))
        {
            return false;
        }
        if (!containsAtIndex(value, sparseIndex))
        {
            return false;
        }

        const size_type denseIndex = static_cast<size_type>(mSparse[sparseIndex]);
        const size_type lastIndex = mDense.size() - 1;

        if (denseIndex != lastIndex)
        {
            const T lastValue = mDense[lastIndex];
            mDense[denseIndex] = lastValue;
            mSparse[static_cast<size_type>(lastValue)] = static_cast<T>(denseIndex);
        }

        mDense.pop_back();
        return true;
    }

    /**
     * @brief Returns true if value is present.
     *
     * @param value Value to check.
     * @return true if present; false otherwise.
     *
     * @note Complexity: O(1).
     * @note Thread-safety: Thread-safe for concurrent reads.
     */
    [[nodiscard]] bool contains(T value) const noexcept
    {
        size_type sparseIndex = 0;
        if (!tryToSparseIndex(value, sparseIndex))
        {
            return false;
        }
        return containsAtIndex(value, sparseIndex);
    }

    /**
     * @brief Finds an element in the set.
     *
     * @param value Value to find.
     * @return Iterator to the element if found; end() otherwise.
     *
     * @note Complexity: O(1).
     * @note Thread-safety: Thread-safe for concurrent reads.
     */
    [[nodiscard]] iterator find(T value) noexcept
    {
        size_type sparseIndex = 0;
        if (!tryToSparseIndex(value, sparseIndex))
        {
            return mDense.end();
        }
        if (!containsAtIndex(value, sparseIndex))
        {
            return mDense.end();
        }
        const size_type denseIndex = static_cast<size_type>(mSparse[sparseIndex]);
        return mDense.begin() + static_cast<typename iterator::difference_type>(denseIndex);
    }

    /**
     * @brief Finds an element in the set (const).
     *
     * @param value Value to find.
     * @return Const iterator to the element if found; end() otherwise.
     *
     * @note Complexity: O(1).
     * @note Thread-safety: Thread-safe for concurrent reads.
     */
    [[nodiscard]] const_iterator find(T value) const noexcept
    {
        size_type sparseIndex = 0;
        if (!tryToSparseIndex(value, sparseIndex))
        {
            return mDense.end();
        }
        if (!containsAtIndex(value, sparseIndex))
        {
            return mDense.end();
        }
        const size_type denseIndex = static_cast<size_type>(mSparse[sparseIndex]);
        return mDense.begin() + static_cast<typename const_iterator::difference_type>(denseIndex);
    }

    /**
     * @brief Returns the dense-array index for a given sparse value.
     *
     * @param value Sparse value to look up.
     * @return Dense index, or size() if value is not present.
     *
     * @details
     * This is useful when the caller maintains parallel arrays that must
     * stay synchronized with the dense array (e.g., an ECS component store
     * that keeps a side-vector of full entity IDs alongside the sparse-key
     * dense array). Knowing the dense index allows the caller to mirror
     * swap-with-back erasure in its own parallel storage.
     *
     * @note Complexity: O(1).
     * @note Thread-safety: Thread-safe for concurrent reads.
     */
    [[nodiscard]] size_type indexOf(T value) const noexcept
    {
        size_type sparseIndex = 0;
        if (!tryToSparseIndex(value, sparseIndex))
        {
            return mDense.size();
        }
        if (!containsAtIndex(value, sparseIndex))
        {
            return mDense.size();
        }
        return static_cast<size_type>(mSparse[sparseIndex]);
    }

    /// @brief Returns the number of elements.
    /// @note Complexity: O(1).
    /// @note Thread-safety: Thread-safe for concurrent reads.
    [[nodiscard]] size_type size() const noexcept
    {
        return mDense.size();
    }

    /// @brief Returns true if the set is empty.
    /// @note Complexity: O(1).
    /// @note Thread-safety: Thread-safe for concurrent reads.
    [[nodiscard]] bool empty() const noexcept
    {
        return mDense.empty();
    }

    /// @brief Removes all elements.
    /// @note Complexity: O(1). Does not shrink sparse mapping.
    void clear() noexcept
    {
        mDense.clear();
    }

    /**
     * @brief Ensures the sparse mapping supports values up to maxValue.
     *
     * @param maxValue Maximum supported value (inclusive).
     * @throws std::length_error if maxValue is too large.
     *
     * @note Complexity: O(maxValue) when growth is required.
     */
    void reserve(size_type maxValue)
    {
        if (maxValue == std::numeric_limits<size_type>::max())
        {
            throw std::length_error("SparseSet::reserve: maxValue too large");
        }

        const size_type required = maxValue + 1;
        if (required > mSparse.size())
        {
            mSparse.resize(required);
        }
    }

    /**
     * @brief Shrinks sparse mapping to minimum required size.
     *
     * Reduces memory usage by shrinking the sparse mapping to fit only the
     * maximum value currently in the set. Also shrinks dense storage.
     *
     * @note Complexity: O(n) where n is number of elements (to find max value).
     * @note If the set is empty, both sparse and dense storage are fully released.
     */
    void shrink_to_fit()
    {
        if (mDense.empty())
        {
            mSparse.clear();
            mSparse.shrink_to_fit();
        }
        else
        {
            const T maxVal = *std::max_element(mDense.begin(), mDense.end());
            const size_type required = static_cast<size_type>(maxVal) + 1;
            if (required < mSparse.size())
            {
                mSparse.resize(required);
            }
            mSparse.shrink_to_fit();
        }
        mDense.shrink_to_fit();
    }

    /**
     * @brief Returns the sparse mapping size.
     *
     * This is the smallest value X such that all values < X can be inserted
     * without resizing the sparse mapping.
     *
     * @note Complexity: O(1).
     * @note Thread-safety: Thread-safe for concurrent reads.
     */
    [[nodiscard]] size_type capacity() const noexcept
    {
        return mSparse.size();
    }

    /**
     * @brief Returns the value stored at dense index.
     *
     * @param index Dense index.
     * @return Value at index.
     *
     * @note Complexity: O(1).
     * @note Thread-safety: Thread-safe for concurrent reads.
     */
    [[nodiscard]] T operator[](size_type index) const noexcept
    {
        return mDense[index];
    }

    /**
     * @brief Returns the value stored at dense index with bounds checking.
     *
     * @param index Dense index.
     * @return Value at index.
     * @throws std::out_of_range if index is out of range.
     *
     * @note Complexity: O(1).
     * @note Thread-safety: Thread-safe for concurrent reads.
     */
    [[nodiscard]] T at(size_type index) const
    {
        if (index >= mDense.size())
        {
            throw std::out_of_range("SparseSet::at: index out of range");
        }
        return mDense[index];
    }

    // -------------------------------------------------------------------------
    // Iterators
    // -------------------------------------------------------------------------

    iterator begin() noexcept
    {
        return mDense.begin();
    }

    iterator end() noexcept
    {
        return mDense.end();
    }

    /// @note Thread-safety: Thread-safe for concurrent reads.
    [[nodiscard]] const_iterator begin() const noexcept
    {
        return mDense.begin();
    }

    /// @note Thread-safety: Thread-safe for concurrent reads.
    [[nodiscard]] const_iterator end() const noexcept
    {
        return mDense.end();
    }

    /// @note Thread-safety: Thread-safe for concurrent reads.
    [[nodiscard]] const_iterator cbegin() const noexcept
    {
        return mDense.cbegin();
    }

    /// @note Thread-safety: Thread-safe for concurrent reads.
    [[nodiscard]] const_iterator cend() const noexcept
    {
        return mDense.cend();
    }

    // -------------------------------------------------------------------------
    // Accessors
    // -------------------------------------------------------------------------

    /// @brief Returns the underlying dense storage.
    /// @note Thread-safety: Thread-safe for concurrent reads.
    [[nodiscard]] const std::vector<T>& dense() const noexcept
    {
        return mDense;
    }

    /// @brief Returns the underlying sparse mapping.
    /// @note Thread-safety: Thread-safe for concurrent reads.
    [[nodiscard]] const std::vector<T>& sparse() const noexcept
    {
        return mSparse;
    }

    // -------------------------------------------------------------------------
    // Swap
    // -------------------------------------------------------------------------

    /**
     * @brief Swaps contents with another SparseSet.
     * @param other SparseSet to swap with.
     * @note Complexity: O(1).
     */
    void swap(SparseSet& other) noexcept(std::is_nothrow_swappable_v<std::vector<T>>)
    {
        using std::swap;
        swap(mDense, other.mDense);
        swap(mSparse, other.mSparse);
    }

    /// @brief ADL-enabled swap.
    friend void swap(SparseSet& a, SparseSet& b) noexcept(noexcept(a.swap(b)))
    {
        a.swap(b);
    }

private:
    // -------------------------------------------------------------------------
    // Private Helpers
    // -------------------------------------------------------------------------

    /// @brief Checks if dense index fits in T without truncation.
    static void enforceDenseIndexFits(size_type denseIndex, const char* context)
    {
        const size_type maxIndex = static_cast<size_type>(std::numeric_limits<T>::max());
        if (denseIndex > maxIndex)
        {
            throw std::length_error(context);
        }
    }

    /// @brief Converts value to sparse index without throwing. Returns false if out of range.
    static bool tryToSparseIndex(T value, size_type& sparseIndex) noexcept
    {
        if constexpr (sizeof(T) > sizeof(size_type))
        {
            if (value > static_cast<T>(std::numeric_limits<size_type>::max()))
            {
                return false;
            }
        }
        sparseIndex = static_cast<size_type>(value);
        return true;
    }

    /// @brief Converts value to sparse index, throwing on overflow.
    static size_type toSparseIndexOrThrow(T value, const char* context)
    {
        if constexpr (sizeof(T) > sizeof(size_type))
        {
            if (value > static_cast<T>(std::numeric_limits<size_type>::max()))
            {
                throw std::length_error(context);
            }
        }
        return static_cast<size_type>(value);
    }

    /// @brief Checks containment given pre-validated sparse index.
    bool containsAtIndex(T value, size_type sparseIndex) const noexcept
    {
        if (sparseIndex >= mSparse.size())
        {
            return false;
        }
        const size_type denseIndex = static_cast<size_type>(mSparse[sparseIndex]);
        return (denseIndex < mDense.size()) && (mDense[denseIndex] == value);
    }

    /// @brief Ensures sparse array can hold value, returns sparse index.
    size_type ensureSparseCapacity(T value)
    {
        const size_type sparseIndex = toSparseIndexOrThrow(
            value, "SparseSet::insert: value must fit in size_type");

        if (sparseIndex == std::numeric_limits<size_type>::max())
        {
            throw std::length_error("SparseSet::insert: value too large");
        }

        const size_type required = sparseIndex + 1;
        if (required > mSparse.size())
        {
            mSparse.resize(required);
        }

        return sparseIndex;
    }

    std::vector<T> mDense;
    std::vector<T> mSparse;
};

// ============================================================================
// SparseSetWithData
// ============================================================================

/**
 * @brief Sparse set for indices with dense associated data.
 *
 * @tparam T    Unsigned index type.
 * @tparam Data Associated data stored for each active index.
 *
 * @note Thread-safety: NOT thread-safe for concurrent writes. Caller must
 *       synchronize. Concurrent reads are safe.
 */
template <typename T, typename Data>
class SparseSetWithData
{
    static_assert(std::is_unsigned_v<T>, "T must be an unsigned integral type");

public:
    using value_type = T;
    using data_type = Data;
    using size_type = size_t;
    using iterator = typename std::vector<T>::iterator;
    using const_iterator = typename std::vector<T>::const_iterator;

    /**
     * @brief Constructs an empty set.
     *
     * @param maxValue Optional maximum supported value (inclusive). This
     *                 pre-sizes the sparse mapping.
     *
     * @note Complexity: O(maxValue) when growth is required; otherwise O(1).
     */
    explicit SparseSetWithData(size_type maxValue = 0)
    {
        if (maxValue != 0)
        {
            reserve(maxValue);
        }
    }

    /**
     * @brief Inserts value with associated data if not present.
     *
     * @param value Value to insert.
     * @param data  Data to associate.
     * @return true if insertion occurred; false if value was already present.
     * @throws std::length_error if value cannot be represented as an index.
     *
     * @note Complexity: O(1) amortized.
     * @note Exception-safety: Strong guarantee.
     */
    bool insert(T value, const Data& data)
    {
        return insertImpl(value, data);
    }

    /**
     * @brief Inserts value with associated data if not present (move).
     *
     * @param value Value to insert.
     * @param data  Data to associate (moved).
     * @return true if insertion occurred; false if value was already present.
     * @throws std::length_error if value cannot be represented as an index.
     *
     * @note Complexity: O(1) amortized.
     * @note Exception-safety: Strong guarantee.
     */
    bool insert(T value, Data&& data)
    {
        return insertImpl(value, std::move(data));
    }

    /**
     * @brief Constructs Data in-place and inserts if value is not present.
     *
     * @tparam Args Constructor argument types for Data.
     * @param value Value to insert.
     * @param args  Arguments forwarded to Data constructor.
     * @return true if insertion occurred; false if value was already present.
     * @throws std::length_error if value cannot be represented as an index.
     *
     * @note Complexity: O(1) amortized.
     * @note Exception-safety: Strong guarantee.
     */
    template <typename... Args>
    bool emplace(T value, Args&&... args)
    {
        return tryEmplace(value, std::forward<Args>(args)...) != nullptr;
    }

    /**
     * @brief Constructs Data in-place if value is not present; returns pointer to data.
     *
     * Unlike emplace(), which returns bool, tryEmplace() returns a pointer to
     * the newly inserted data on success, or nullptr if the value was already
     * present. This avoids a redundant lookup when the caller needs a reference
     * to the inserted data immediately after insertion.
     *
     * @tparam Args Constructor argument types for Data.
     * @param value Value to insert.
     * @param args  Arguments forwarded to Data constructor.
     * @return Pointer to inserted data, or nullptr if value was already present.
     * @throws std::length_error if value cannot be represented as an index.
     *
     * @note Complexity: O(1) amortized.
     * @note Exception-safety: Strong guarantee.
     */
    template <typename... Args>
    Data* tryEmplace(T value, Args&&... args)
    {
        const size_type sparseIndex = ensureSparseCapacity(value);

        if (containsAtIndex(value, sparseIndex))
        {
            return nullptr;
        }

        const size_type denseIndex = mDense.size();
        enforceDenseIndexFits(denseIndex, "SparseSetWithData::tryEmplace: dense index overflow");

        mDense.push_back(value);
        try
        {
            mData.emplace_back(std::forward<Args>(args)...);
        }
        catch (...)
        {
            mDense.pop_back();
            throw;
        }

        mSparse[sparseIndex] = static_cast<T>(denseIndex);
        return &mData.back();
    }

    /**
     * @brief Erases value if present.
     *
     * @param value Value to erase.
     * @return true if value was erased; false if it was not present.
     *
     * @note Complexity: O(1).
     * @note Exception-safety: Basic guarantee if Data move-assignment can throw.
     */
    bool erase(T value)
    {
        size_type sparseIndex = 0;
        if (!tryToSparseIndex(value, sparseIndex))
        {
            return false;
        }
        if (!containsAtIndex(value, sparseIndex))
        {
            return false;
        }

        const size_type denseIndex = static_cast<size_type>(mSparse[sparseIndex]);
        const size_type lastIndex = mDense.size() - 1;

        if (denseIndex != lastIndex)
        {
            const T lastValue = mDense[lastIndex];

            // Move data first. If this throws, dense/sparse mapping is unchanged.
            mData[denseIndex] = std::move(mData[lastIndex]);

            mDense[denseIndex] = lastValue;
            mSparse[static_cast<size_type>(lastValue)] = static_cast<T>(denseIndex);
        }

        mDense.pop_back();
        mData.pop_back();
        return true;
    }

    /**
     * @brief Returns true if value is present.
     *
     * @param value Value to check.
     * @return true if present; false otherwise.
     *
     * @note Complexity: O(1).
     * @note Thread-safety: Thread-safe for concurrent reads.
     */
    [[nodiscard]] bool contains(T value) const noexcept
    {
        size_type sparseIndex = 0;
        if (!tryToSparseIndex(value, sparseIndex))
        {
            return false;
        }
        return containsAtIndex(value, sparseIndex);
    }

    /**
     * @brief Finds an element in the set.
     *
     * @param value Value to find.
     * @return Iterator to the element if found; end() otherwise.
     *
     * @note Complexity: O(1).
     * @note Thread-safety: Thread-safe for concurrent reads.
     */
    [[nodiscard]] iterator find(T value) noexcept
    {
        size_type sparseIndex = 0;
        if (!tryToSparseIndex(value, sparseIndex))
        {
            return mDense.end();
        }
        if (!containsAtIndex(value, sparseIndex))
        {
            return mDense.end();
        }
        const size_type denseIndex = static_cast<size_type>(mSparse[sparseIndex]);
        return mDense.begin() + static_cast<typename iterator::difference_type>(denseIndex);
    }

    /**
     * @brief Finds an element in the set (const).
     *
     * @param value Value to find.
     * @return Const iterator to the element if found; end() otherwise.
     *
     * @note Complexity: O(1).
     * @note Thread-safety: Thread-safe for concurrent reads.
     */
    [[nodiscard]] const_iterator find(T value) const noexcept
    {
        size_type sparseIndex = 0;
        if (!tryToSparseIndex(value, sparseIndex))
        {
            return mDense.end();
        }
        if (!containsAtIndex(value, sparseIndex))
        {
            return mDense.end();
        }
        const size_type denseIndex = static_cast<size_type>(mSparse[sparseIndex]);
        return mDense.begin() + static_cast<typename const_iterator::difference_type>(denseIndex);
    }

    /**
     * @brief Returns the dense-array index for a given sparse value.
     *
     * @param value Sparse value to look up.
     * @return Dense index, or size() if value is not present.
     *
     * @details
     * This is useful when the caller maintains parallel arrays that must
     * stay synchronized with the dense array (e.g., an ECS component store
     * that keeps a side-vector of full entity IDs alongside the sparse-key
     * dense array). Knowing the dense index allows the caller to mirror
     * swap-with-back erasure in its own parallel storage.
     *
     * @note Complexity: O(1).
     * @note Thread-safety: Thread-safe for concurrent reads.
     */
    [[nodiscard]] size_type indexOf(T value) const noexcept
    {
        size_type sparseIndex = 0;
        if (!tryToSparseIndex(value, sparseIndex))
        {
            return mDense.size();
        }
        if (!containsAtIndex(value, sparseIndex))
        {
            return mDense.size();
        }
        return static_cast<size_type>(mSparse[sparseIndex]);
    }

    /**
     * @brief Returns a pointer to data for value, or nullptr if absent.
     *
     * @param value Value to look up.
     * @return Pointer to associated data, or nullptr.
     *
     * @note Complexity: O(1).
     */
    [[nodiscard]] Data* tryGet(T value) noexcept
    {
        size_type sparseIndex = 0;
        if (!tryToSparseIndex(value, sparseIndex))
        {
            return nullptr;
        }
        if (!containsAtIndex(value, sparseIndex))
        {
            return nullptr;
        }
        return &mData[static_cast<size_type>(mSparse[sparseIndex])];
    }

    /**
     * @brief Returns a const pointer to data for value, or nullptr if absent.
     *
     * @param value Value to look up.
     * @return Const pointer to associated data, or nullptr.
     *
     * @note Complexity: O(1).
     * @note Thread-safety: Thread-safe for concurrent reads.
     */
    [[nodiscard]] const Data* tryGet(T value) const noexcept
    {
        size_type sparseIndex = 0;
        if (!tryToSparseIndex(value, sparseIndex))
        {
            return nullptr;
        }
        if (!containsAtIndex(value, sparseIndex))
        {
            return nullptr;
        }
        return &mData[static_cast<size_type>(mSparse[sparseIndex])];
    }

    /**
     * @brief Returns a reference to data for value.
     *
     * @param value Value to look up.
     * @return Reference to associated data.
     * @throws std::out_of_range if value is not present.
     *
     * @note Complexity: O(1).
     */
    [[nodiscard]] Data& get(T value)
    {
        Data* dataPtr = tryGet(value);
        if (dataPtr == nullptr)
        {
            throw std::out_of_range("SparseSetWithData::get: element not present");
        }
        return *dataPtr;
    }

    /**
     * @brief Returns a const reference to data for value.
     *
     * @param value Value to look up.
     * @return Const reference to associated data.
     * @throws std::out_of_range if value is not present.
     *
     * @note Complexity: O(1).
     * @note Thread-safety: Thread-safe for concurrent reads.
     */
    [[nodiscard]] const Data& get(T value) const
    {
        const Data* dataPtr = tryGet(value);
        if (dataPtr == nullptr)
        {
            throw std::out_of_range("SparseSetWithData::get: element not present");
        }
        return *dataPtr;
    }

    /**
     * @brief Returns a reference to data at dense index.
     *
     * @param index Dense index.
     * @return Reference to data.
     * @throws std::out_of_range if index is out of range.
     *
     * @note Complexity: O(1).
     */
    [[nodiscard]] Data& dataAt(size_type index)
    {
        return mData.at(index);
    }

    /**
     * @brief Returns a const reference to data at dense index.
     *
     * @param index Dense index.
     * @return Const reference to data.
     * @throws std::out_of_range if index is out of range.
     *
     * @note Complexity: O(1).
     * @note Thread-safety: Thread-safe for concurrent reads.
     */
    [[nodiscard]] const Data& dataAt(size_type index) const
    {
        return mData.at(index);
    }

    /// @brief Returns the number of elements.
    /// @note Thread-safety: Thread-safe for concurrent reads.
    [[nodiscard]] size_type size() const noexcept
    {
        return mDense.size();
    }

    /// @brief Returns true if the set is empty.
    /// @note Thread-safety: Thread-safe for concurrent reads.
    [[nodiscard]] bool empty() const noexcept
    {
        return mDense.empty();
    }

    /// @brief Removes all elements.
    /// @note Complexity: O(n) where n is number of elements (destructor calls).
    void clear() noexcept
    {
        mDense.clear();
        mData.clear();
    }

    /**
     * @brief Ensures the sparse mapping supports values up to maxValue.
     *
     * @param maxValue Maximum supported value (inclusive).
     * @throws std::length_error if maxValue is too large.
     *
     * @note Complexity: O(maxValue) when growth is required.
     */
    void reserve(size_type maxValue)
    {
        if (maxValue == std::numeric_limits<size_type>::max())
        {
            throw std::length_error("SparseSetWithData::reserve: maxValue too large");
        }

        const size_type required = maxValue + 1;
        if (required > mSparse.size())
        {
            mSparse.resize(required);
        }
    }

    /**
     * @brief Shrinks sparse mapping to minimum required size.
     *
     * Reduces memory usage by shrinking the sparse mapping to fit only the
     * maximum value currently in the set. Also shrinks dense and data storage.
     *
     * @note Complexity: O(n) where n is number of elements (to find max value).
     * @note If the set is empty, all storage is fully released.
     */
    void shrink_to_fit()
    {
        if (mDense.empty())
        {
            mSparse.clear();
            mSparse.shrink_to_fit();
        }
        else
        {
            const T maxVal = *std::max_element(mDense.begin(), mDense.end());
            const size_type required = static_cast<size_type>(maxVal) + 1;
            if (required < mSparse.size())
            {
                mSparse.resize(required);
            }
            mSparse.shrink_to_fit();
        }
        mDense.shrink_to_fit();
        mData.shrink_to_fit();
    }

    /// @brief Returns the sparse mapping size.
    /// @note Thread-safety: Thread-safe for concurrent reads.
    [[nodiscard]] size_type capacity() const noexcept
    {
        return mSparse.size();
    }

    /// @brief Returns the underlying dense index storage.
    /// @note Thread-safety: Thread-safe for concurrent reads.
    [[nodiscard]] const std::vector<T>& dense() const noexcept
    {
        return mDense;
    }

    /// @brief Returns the underlying dense data storage.
    /// @note Thread-safety: Thread-safe for concurrent reads.
    [[nodiscard]] const std::vector<Data>& data() const noexcept
    {
        return mData;
    }

    // -------------------------------------------------------------------------
    // Iterators
    // -------------------------------------------------------------------------

    iterator begin() noexcept
    {
        return mDense.begin();
    }

    iterator end() noexcept
    {
        return mDense.end();
    }

    /// @note Thread-safety: Thread-safe for concurrent reads.
    [[nodiscard]] const_iterator begin() const noexcept
    {
        return mDense.begin();
    }

    /// @note Thread-safety: Thread-safe for concurrent reads.
    [[nodiscard]] const_iterator end() const noexcept
    {
        return mDense.end();
    }

    /// @note Thread-safety: Thread-safe for concurrent reads.
    [[nodiscard]] const_iterator cbegin() const noexcept
    {
        return mDense.cbegin();
    }

    /// @note Thread-safety: Thread-safe for concurrent reads.
    [[nodiscard]] const_iterator cend() const noexcept
    {
        return mDense.cend();
    }

    // -------------------------------------------------------------------------
    // Swap
    // -------------------------------------------------------------------------

    /**
     * @brief Swaps contents with another SparseSetWithData.
     * @param other SparseSetWithData to swap with.
     * @note Complexity: O(1).
     */
    void swap(SparseSetWithData& other) noexcept(
        std::is_nothrow_swappable_v<std::vector<T>> &&
        std::is_nothrow_swappable_v<std::vector<Data>>)
    {
        using std::swap;
        swap(mDense, other.mDense);
        swap(mData, other.mData);
        swap(mSparse, other.mSparse);
    }

    /// @brief ADL-enabled swap.
    friend void swap(SparseSetWithData& a, SparseSetWithData& b) noexcept(noexcept(a.swap(b)))
    {
        a.swap(b);
    }

private:
    // -------------------------------------------------------------------------
    // Private Helpers
    // -------------------------------------------------------------------------

    static void enforceDenseIndexFits(size_type denseIndex, const char* context)
    {
        const size_type maxIndex = static_cast<size_type>(std::numeric_limits<T>::max());
        if (denseIndex > maxIndex)
        {
            throw std::length_error(context);
        }
    }

    static bool tryToSparseIndex(T value, size_type& sparseIndex) noexcept
    {
        if constexpr (sizeof(T) > sizeof(size_type))
        {
            if (value > static_cast<T>(std::numeric_limits<size_type>::max()))
            {
                return false;
            }
        }
        sparseIndex = static_cast<size_type>(value);
        return true;
    }

    static size_type toSparseIndexOrThrow(T value, const char* context)
    {
        if constexpr (sizeof(T) > sizeof(size_type))
        {
            if (value > static_cast<T>(std::numeric_limits<size_type>::max()))
            {
                throw std::length_error(context);
            }
        }
        return static_cast<size_type>(value);
    }

    bool containsAtIndex(T value, size_type sparseIndex) const noexcept
    {
        if (sparseIndex >= mSparse.size())
        {
            return false;
        }
        const size_type denseIndex = static_cast<size_type>(mSparse[sparseIndex]);
        return (denseIndex < mDense.size()) && (mDense[denseIndex] == value);
    }

    size_type ensureSparseCapacity(T value)
    {
        const size_type sparseIndex = toSparseIndexOrThrow(
            value, "SparseSetWithData: value must fit in size_type");

        if (sparseIndex == std::numeric_limits<size_type>::max())
        {
            throw std::length_error("SparseSetWithData: value too large");
        }

        const size_type required = sparseIndex + 1;
        if (required > mSparse.size())
        {
            mSparse.resize(required);
        }

        return sparseIndex;
    }

    template <typename DataArg>
    bool insertImpl(T value, DataArg&& data)
    {
        const size_type sparseIndex = ensureSparseCapacity(value);

        if (containsAtIndex(value, sparseIndex))
        {
            return false;
        }

        const size_type denseIndex = mDense.size();
        enforceDenseIndexFits(denseIndex, "SparseSetWithData::insert: dense index overflow");

        mDense.push_back(value);
        try
        {
            mData.push_back(std::forward<DataArg>(data));
        }
        catch (...)
        {
            mDense.pop_back();
            throw;
        }

        mSparse[sparseIndex] = static_cast<T>(denseIndex);
        return true;
    }

    std::vector<T> mDense;
    std::vector<Data> mData;
    std::vector<T> mSparse;
};

} // namespace fat_p
