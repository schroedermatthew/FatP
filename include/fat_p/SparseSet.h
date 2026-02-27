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
 * SparseSet stores a set of keys with O(1) average-case insert, erase,
 * and contains operations. Active keys are stored densely for
 * cache-friendly iteration.
 *
 * An IndexPolicy template parameter controls how keys are mapped to
 * sparse-array indices. The default IdentityIndex requires unsigned
 * integer keys and uses them directly as indices. Custom policies
 * enable composite key types (e.g., entity handles that pack an index
 * and a generation counter) by extracting the sparse index from the key.
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
// Index Policies
// ============================================================================

/**
 * @brief Default index policy: the key is its own sparse-array index.
 *
 * @tparam T Unsigned integral key type.
 *
 * This is the identity mapping. The key type must be an unsigned integer.
 * The sparse array is indexed directly by the key value.
 */
template <typename T>
struct IdentityIndex
{
    static_assert(std::is_unsigned_v<T>,
                  "IdentityIndex requires an unsigned integral key type");

    using sparse_index_type = T;

    [[nodiscard]] static constexpr T index(const T& key) noexcept
    {
        return key;
    }
};

// ============================================================================
// SparseSet
// ============================================================================

/**
 * @brief Sparse set for keys with dense iteration.
 *
 * @tparam T           Key type stored in the dense array.
 * @tparam IndexPolicy Policy that extracts a sparse-array index from a key.
 *                     Must provide:
 *                     - `using sparse_index_type = <unsigned integral type>`
 *                     - `static constexpr sparse_index_type index(const T&) noexcept`
 *
 * With the default IdentityIndex, the key type must be an unsigned integer
 * and is used directly as the sparse-array index. A custom IndexPolicy
 * enables composite key types whose sparse index is a subset of the key.
 * Identity is determined by the extracted index: two keys with the same
 * extracted index are considered the same element.
 *
 * @note Thread-safety: NOT thread-safe for concurrent writes. Caller must
 *       synchronize. Concurrent reads are safe.
 */
template <typename T = uint32_t, typename IndexPolicy = IdentityIndex<T>>
class SparseSet
{
    using sparse_type = typename IndexPolicy::sparse_index_type;
    static_assert(std::is_unsigned_v<sparse_type>,
                  "IndexPolicy::sparse_index_type must be unsigned");

public:
    using value_type = T;
    using index_policy = IndexPolicy;
    using size_type = size_t;
    using iterator = typename std::vector<T>::iterator;
    using const_iterator = typename std::vector<T>::const_iterator;

    /**
     * @brief Constructs an empty set.
     *
     * @param maxValue Optional maximum supported sparse index (inclusive).
     *                 This pre-sizes the sparse mapping.
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
     * @brief Inserts key if it is not present.
     *
     * @param value Key to insert.
     * @return true if insertion occurred; false if key was already present.
     * @throws std::length_error if the extracted index cannot be represented.
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
        mSparse[sparseIndex] = static_cast<sparse_type>(denseIndex);
        return true;
    }

    /**
     * @brief Erases key if it is present.
     *
     * @param value Key to erase.
     * @return true if key was erased; false if it was not present.
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
            mSparse[static_cast<size_type>(IndexPolicy::index(lastValue))] =
                static_cast<sparse_type>(denseIndex);
        }

        mDense.pop_back();
        return true;
    }

    /**
     * @brief Returns true if key is present.
     *
     * @param value Key to check.
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
     * @param value Key to find.
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
     * @param value Key to find.
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
     * @brief Returns the dense-array index for a given key.
     *
     * @param value Key to look up.
     * @return Dense index, or size() if key is not present.
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
     * @brief Ensures the sparse mapping supports indices up to maxValue.
     *
     * @param maxValue Maximum supported sparse index (inclusive).
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
     * @note Complexity: O(n) where n is number of elements (to find max index).
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
            size_type maxIdx = 0;
            for (const T& val : mDense)
            {
                const size_type idx = static_cast<size_type>(IndexPolicy::index(val));
                if (idx > maxIdx)
                {
                    maxIdx = idx;
                }
            }
            const size_type required = maxIdx + 1;
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
     * @note Complexity: O(1).
     * @note Thread-safety: Thread-safe for concurrent reads.
     */
    [[nodiscard]] size_type capacity() const noexcept
    {
        return mSparse.size();
    }

    /**
     * @brief Returns the key stored at dense index.
     *
     * @note Complexity: O(1).
     * @note Thread-safety: Thread-safe for concurrent reads.
     */
    [[nodiscard]] T operator[](size_type index) const noexcept
    {
        return mDense[index];
    }

    /**
     * @brief Returns the key stored at dense index with bounds checking.
     *
     * @throws std::out_of_range if index is out of range.
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

    iterator begin() noexcept { return mDense.begin(); }
    iterator end() noexcept { return mDense.end(); }

    /// @note Thread-safety: Thread-safe for concurrent reads.
    [[nodiscard]] const_iterator begin() const noexcept { return mDense.begin(); }
    /// @note Thread-safety: Thread-safe for concurrent reads.
    [[nodiscard]] const_iterator end() const noexcept { return mDense.end(); }
    /// @note Thread-safety: Thread-safe for concurrent reads.
    [[nodiscard]] const_iterator cbegin() const noexcept { return mDense.cbegin(); }
    /// @note Thread-safety: Thread-safe for concurrent reads.
    [[nodiscard]] const_iterator cend() const noexcept { return mDense.cend(); }

    // -------------------------------------------------------------------------
    // Accessors
    // -------------------------------------------------------------------------

    /// @brief Returns the underlying dense storage.
    /// @note Thread-safety: Thread-safe for concurrent reads.
    [[nodiscard]] const std::vector<T>& dense() const noexcept { return mDense; }

    /// @brief Returns the underlying sparse mapping.
    /// @note Thread-safety: Thread-safe for concurrent reads.
    [[nodiscard]] const std::vector<sparse_type>& sparse() const noexcept { return mSparse; }

    // -------------------------------------------------------------------------
    // Swap
    // -------------------------------------------------------------------------

    /// @brief Swaps contents with another SparseSet.
    /// @note Complexity: O(1).
    void swap(SparseSet& other) noexcept(
        std::is_nothrow_swappable_v<std::vector<T>> &&
        std::is_nothrow_swappable_v<std::vector<sparse_type>>)
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

    static void enforceDenseIndexFits(size_type denseIndex, const char* context)
    {
        const size_type maxIndex = static_cast<size_type>(
            std::numeric_limits<sparse_type>::max());
        if (denseIndex > maxIndex)
        {
            throw std::length_error(context);
        }
    }

    static bool tryToSparseIndex(const T& value, size_type& sparseIndex) noexcept
    {
        const sparse_type idx = IndexPolicy::index(value);
        if constexpr (sizeof(sparse_type) > sizeof(size_type))
        {
            if (idx > static_cast<sparse_type>(std::numeric_limits<size_type>::max()))
            {
                return false;
            }
        }
        sparseIndex = static_cast<size_type>(idx);
        return true;
    }

    static size_type toSparseIndexOrThrow(const T& value, const char* context)
    {
        const sparse_type idx = IndexPolicy::index(value);
        if constexpr (sizeof(sparse_type) > sizeof(size_type))
        {
            if (idx > static_cast<sparse_type>(std::numeric_limits<size_type>::max()))
            {
                throw std::length_error(context);
            }
        }
        return static_cast<size_type>(idx);
    }

    bool containsAtIndex(const T& /*value*/, size_type sparseIndex) const noexcept
    {
        if (sparseIndex >= mSparse.size())
        {
            return false;
        }
        const size_type denseIndex = static_cast<size_type>(mSparse[sparseIndex]);
        return (denseIndex < mDense.size()) &&
               (static_cast<size_type>(IndexPolicy::index(mDense[denseIndex])) == sparseIndex);
    }

    size_type ensureSparseCapacity(const T& value)
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
            // Geometric growth: double from current capacity until sufficient.
            // Avoids O(N^2) reallocation cost when inserting N sequential keys.
            // reserve() (public API) retains exact sizing for known-bound pre-allocation.
            size_type newCapacity = mSparse.size() < 64 ? 64 : mSparse.size();
            while (newCapacity < required)
                newCapacity *= 2;
            mSparse.resize(newCapacity);
        }

        return sparseIndex;
    }

    std::vector<T> mDense;
    std::vector<sparse_type> mSparse;
};

// ============================================================================
// SparseSetWithData
// ============================================================================

/**
 * @brief Sparse set for keys with dense associated data.
 *
 * @tparam T           Key type stored in the dense array.
 * @tparam Data        Associated data stored for each active key.
 * @tparam IndexPolicy Policy that extracts a sparse-array index from a key.
 *                     Must provide:
 *                     - `using sparse_index_type = <unsigned integral type>`
 *                     - `static constexpr sparse_index_type index(const T&) noexcept`
 *
 * With the default IdentityIndex, the key type must be an unsigned integer
 * and is used directly as the sparse-array index. A custom IndexPolicy
 * enables composite key types whose sparse index is a subset of the key.
 * Identity is determined by the extracted index: two keys with the same
 * extracted index are considered the same element.
 *
 * @note Thread-safety: NOT thread-safe for concurrent writes. Caller must
 *       synchronize. Concurrent reads are safe.
 */
template <typename T, typename Data, typename IndexPolicy = IdentityIndex<T>,
          template <typename, typename...> class DataContainer = std::vector>
class SparseSetWithData
{
    using sparse_type = typename IndexPolicy::sparse_index_type;
    static_assert(std::is_unsigned_v<sparse_type>,
                  "IndexPolicy::sparse_index_type must be unsigned");

public:
    using value_type = T;
    using data_type = Data;
    using index_policy = IndexPolicy;
    using size_type = size_t;
    using iterator = typename std::vector<T>::iterator;
    using const_iterator = typename std::vector<T>::const_iterator;

    /**
     * @brief Constructs an empty set.
     *
     * @param maxValue Optional maximum supported sparse index (inclusive).
     *                 This pre-sizes the sparse mapping.
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
     * @brief Inserts key with associated data if not present.
     *
     * @param value Key to insert.
     * @param data  Data to associate.
     * @return true if insertion occurred; false if key was already present.
     * @throws std::length_error if the extracted index cannot be represented.
     *
     * @note Complexity: O(1) amortized.
     * @note Exception-safety: Strong guarantee.
     */
    bool insert(T value, const Data& data)
    {
        return insertImpl(value, data);
    }

    /**
     * @brief Inserts key with associated data if not present (move).
     *
     * @param value Key to insert.
     * @param data  Data to associate (moved).
     * @return true if insertion occurred; false if key was already present.
     * @throws std::length_error if the extracted index cannot be represented.
     *
     * @note Complexity: O(1) amortized.
     * @note Exception-safety: Strong guarantee.
     */
    bool insert(T value, Data&& data)
    {
        return insertImpl(value, std::move(data));
    }

    /**
     * @brief Constructs Data in-place and inserts if key is not present.
     *
     * @tparam Args Constructor argument types for Data.
     * @param value Key to insert.
     * @param args  Arguments forwarded to Data constructor.
     * @return true if insertion occurred; false if key was already present.
     * @throws std::length_error if the extracted index cannot be represented.
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
     * @brief Constructs Data in-place if key is not present; returns pointer to data.
     *
     * Unlike emplace(), which returns bool, tryEmplace() returns a pointer to
     * the newly inserted data on success, or nullptr if the key was already
     * present. This avoids a redundant lookup when the caller needs a reference
     * to the inserted data immediately after insertion.
     *
     * @tparam Args Constructor argument types for Data.
     * @param value Key to insert.
     * @param args  Arguments forwarded to Data constructor.
     * @return Pointer to inserted data, or nullptr if key was already present.
     * @throws std::length_error if the extracted index cannot be represented.
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
        enforceDenseIndexFits(denseIndex,
                              "SparseSetWithData::tryEmplace: dense index overflow");

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

        mSparse[sparseIndex] = static_cast<sparse_type>(denseIndex);
        return &mData.back();
    }

    /**
     * @brief Erases key if present.
     *
     * @param value Key to erase.
     * @return true if key was erased; false if it was not present.
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
            mSparse[static_cast<size_type>(IndexPolicy::index(lastValue))] =
                static_cast<sparse_type>(denseIndex);
        }

        mDense.pop_back();
        mData.pop_back();

        // Mark the erased slot as absent so raw reads don't see a stale dense
        // index. containsAtIndex() uses the round-trip check and remains
        // correct, but callers can now fast-path on the sentinel.
        mSparse[sparseIndex] = std::numeric_limits<sparse_type>::max();

        return true;
    }

    /**
     * @brief Returns true if key is present.
     *
     * @param value Key to check.
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
        const size_type denseIndex = static_cast<size_type>(mSparse[sparseIndex]);
        return &mData[denseIndex];
    }

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
        const size_type denseIndex = static_cast<size_type>(mSparse[sparseIndex]);
        return &mData[denseIndex];
    }

    [[nodiscard]] Data& get(T value)
    {
        Data* ptr = tryGet(value);
        if (ptr == nullptr)
        {
            throw std::out_of_range("SparseSetWithData::get: value not found");
        }
        return *ptr;
    }

    [[nodiscard]] const Data& get(T value) const
    {
        const Data* ptr = tryGet(value);
        if (ptr == nullptr)
        {
            throw std::out_of_range("SparseSetWithData::get: value not found");
        }
        return *ptr;
    }

    [[nodiscard]] Data& dataAt(size_type index)
    {
        return mData.at(index);
    }

    [[nodiscard]] const Data& dataAt(size_type index) const
    {
        return mData.at(index);
    }

    /// @brief Unchecked access to data by dense index. Use only when the
    ///        index is known to be valid (e.g. during iteration over the
    ///        dense array). No bounds check is performed.
    [[nodiscard]] Data& dataAtUnchecked(size_type index) noexcept
    {
        return mData[index];
    }

    [[nodiscard]] const Data& dataAtUnchecked(size_type index) const noexcept
    {
        return mData[index];
    }

    [[nodiscard]] size_type size() const noexcept { return mDense.size(); }
    [[nodiscard]] bool empty() const noexcept { return mDense.empty(); }

    void clear() noexcept
    {
        mDense.clear();
        mData.clear();
    }

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

    void shrink_to_fit()
    {
        if (mDense.empty())
        {
            mSparse.clear();
            mSparse.shrink_to_fit();
        }
        else
        {
            size_type maxIdx = 0;
            for (const T& val : mDense)
            {
                const size_type idx = static_cast<size_type>(IndexPolicy::index(val));
                if (idx > maxIdx)
                {
                    maxIdx = idx;
                }
            }
            const size_type required = maxIdx + 1;
            if (required < mSparse.size())
            {
                mSparse.resize(required);
            }
            mSparse.shrink_to_fit();
        }
        mDense.shrink_to_fit();
        if constexpr (requires { mData.shrink_to_fit(); })
            mData.shrink_to_fit();
    }

    [[nodiscard]] size_type capacity() const noexcept { return mSparse.size(); }
    [[nodiscard]] const std::vector<T>& dense() const noexcept { return mDense; }
    [[nodiscard]] const DataContainer<Data>& data() const noexcept { return mData; }
    [[nodiscard]] const std::vector<sparse_type>& sparse() const noexcept { return mSparse; }

    /**
     * @brief Swap two elements by dense index, maintaining all invariants.
     *
     * Exchanges the keys, data, and sparse back-pointers for the elements at
     * dense positions @p i and @p j. Both indices must be less than size().
     * No-op when i == j.
     *
     * Use this instead of writing to the dense/data/sparse arrays directly.
     * It is the only correct way to reorder elements in place — for example,
     * when sorting components by entity index or maintaining a contiguous
     * group prefix.
     *
     * @param i Dense index of the first element.
     * @param j Dense index of the second element.
     *
     * @note Complexity: O(1).
     * @note noexcept when Data is nothrow-swappable.
     * @note Precondition: i < size() && j < size() (asserted in debug builds).
     */
    void swapDenseEntries(size_type i, size_type j) noexcept(
        std::is_nothrow_swappable_v<T> &&
        std::is_nothrow_swappable_v<Data>)
    {
        if (i == j)
            return;

        using std::swap;
        swap(mDense[i], mDense[j]);
        swap(mData[i],  mData[j]);
        mSparse[static_cast<size_type>(IndexPolicy::index(mDense[i]))] =
            static_cast<sparse_type>(i);
        mSparse[static_cast<size_type>(IndexPolicy::index(mDense[j]))] =
            static_cast<sparse_type>(j);
    }

    // -------------------------------------------------------------------------
    // Iterators
    // -------------------------------------------------------------------------

    iterator begin() noexcept { return mDense.begin(); }
    iterator end() noexcept { return mDense.end(); }
    [[nodiscard]] const_iterator begin() const noexcept { return mDense.begin(); }
    [[nodiscard]] const_iterator end() const noexcept { return mDense.end(); }
    [[nodiscard]] const_iterator cbegin() const noexcept { return mDense.cbegin(); }
    [[nodiscard]] const_iterator cend() const noexcept { return mDense.cend(); }

    // -------------------------------------------------------------------------
    // Swap
    // -------------------------------------------------------------------------

    void swap(SparseSetWithData& other) noexcept(
        std::is_nothrow_swappable_v<std::vector<T>> &&
        std::is_nothrow_swappable_v<DataContainer<Data>> &&
        std::is_nothrow_swappable_v<std::vector<sparse_type>>)
    {
        using std::swap;
        swap(mDense, other.mDense);
        swap(mData, other.mData);
        swap(mSparse, other.mSparse);
    }

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
        const size_type maxIndex = static_cast<size_type>(
            std::numeric_limits<sparse_type>::max());
        if (denseIndex > maxIndex)
        {
            throw std::length_error(context);
        }
    }

    static bool tryToSparseIndex(const T& value, size_type& sparseIndex) noexcept
    {
        const sparse_type idx = IndexPolicy::index(value);
        if constexpr (sizeof(sparse_type) > sizeof(size_type))
        {
            if (idx > static_cast<sparse_type>(std::numeric_limits<size_type>::max()))
            {
                return false;
            }
        }
        sparseIndex = static_cast<size_type>(idx);
        return true;
    }

    static size_type toSparseIndexOrThrow(const T& value, const char* context)
    {
        const sparse_type idx = IndexPolicy::index(value);
        if constexpr (sizeof(sparse_type) > sizeof(size_type))
        {
            if (idx > static_cast<sparse_type>(std::numeric_limits<size_type>::max()))
            {
                throw std::length_error(context);
            }
        }
        return static_cast<size_type>(idx);
    }

    bool containsAtIndex(const T& /*value*/, size_type sparseIndex) const noexcept
    {
        if (sparseIndex >= mSparse.size())
        {
            return false;
        }
        const size_type denseIndex = static_cast<size_type>(mSparse[sparseIndex]);
        return (denseIndex < mDense.size()) &&
               (static_cast<size_type>(IndexPolicy::index(mDense[denseIndex])) == sparseIndex);
    }

    size_type ensureSparseCapacity(const T& value)
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
            // Geometric growth: double from current capacity until sufficient.
            // Avoids O(N^2) reallocation cost when inserting N sequential keys.
            // reserve() (public API) retains exact sizing for known-bound pre-allocation.
            size_type newCapacity = mSparse.size() < 64 ? 64 : mSparse.size();
            while (newCapacity < required)
                newCapacity *= 2;
            mSparse.resize(newCapacity);
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
        enforceDenseIndexFits(denseIndex,
                              "SparseSetWithData::insert: dense index overflow");

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

        mSparse[sparseIndex] = static_cast<sparse_type>(denseIndex);
        return true;
    }

    std::vector<T> mDense;
    DataContainer<Data> mData;
    std::vector<sparse_type> mSparse;
};

} // namespace fat_p
