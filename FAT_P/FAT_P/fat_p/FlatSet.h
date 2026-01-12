/**
 * @file FlatSet.h
 * @brief Sorted vector-backed set container with contiguous storage
 *
 * @layer Enforcement
 *
 * FlatSet provides an ordered set container backed by a sorted vector,
 * offering cache-friendly iteration and O(log n) lookup. Suitable for small
 * to medium-sized collections where insertion/deletion frequency is low
 * relative to lookup frequency.
 *
 * @note Thread-safety: NOT thread-safe. Caller must synchronize.
 */
#pragma once

/*
FATP_META:
  meta_version: 1
  component: FlatSet
  file_role: public_header
  path: fat_p/FlatSet.h
  namespace: fat_p
  summary: "Public header for FlatSet."
  api_stability: in_work
  related:
    docs_search: "FlatSet"
    tests:
      - tests/test_FatPTypeTraits.cpp
      - tests/test_FlatSet.cpp
    benchmarks:
      - benchmarks/benchmark_FlatMapSet.cpp
  hygiene:
    pragma_once: true
    include_guard: true
    defines_total: 1
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/
#include <algorithm>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <type_traits>
#include <utility>
#include <vector>

#include "enforce.h"
#include "FatPTypeTraits.h"

namespace fat_p
{

// =============================================================================
// Tag types for sorted input optimization (shared with FlatMap)
// =============================================================================

#ifndef FATP_ORDERED_RANGE_TAGS_DEFINED
#define FATP_ORDERED_RANGE_TAGS_DEFINED

/**
 * @brief Tag to indicate input range is already sorted and unique.
 * 
 * Pass this tag to constructors/insert functions when you can guarantee
 * the input is already sorted according to the container's comparator
 * and contains no duplicate keys. This skips O(N log N) sorting.
 * 
 * @warning Passing unsorted or non-unique data with this tag is undefined behavior.
 */
struct ordered_unique_range_t { explicit ordered_unique_range_t() = default; };
inline constexpr ordered_unique_range_t ordered_unique_range{};

/**
 * @brief Tag to indicate input range is already sorted but may have duplicates.
 * 
 * Pass this tag when input is sorted but may contain duplicate keys.
 * This skips sorting but still runs deduplication (first key wins).
 */
struct ordered_range_t { explicit ordered_range_t() = default; };
inline constexpr ordered_range_t ordered_range{};

#endif // FATP_ORDERED_RANGE_TAGS_DEFINED

template <typename T, typename Compare = std::less<T>, typename Allocator = std::allocator<T>>
class FlatSet
{
private:
    using Storage = std::vector<T, Allocator>;
    using InternalIterator = typename Storage::iterator;

    Storage mData;
    Compare mComp;

    // Helper to detect if Compare has is_transparent
    template <typename C, typename = void>
    struct HasIsTransparent : std::false_type {};
    
    template <typename C>
    struct HasIsTransparent<C, std::void_t<typename C::is_transparent>> : std::true_type {};

    bool elementsEquivalent(const T& a, const T& b) const
    {
        return !mComp(a, b) && !mComp(b, a);
    }
    
    // Heterogeneous equivalence check
    template <typename K>
    bool elementsEquivalentHetero(const T& a, const K& b) const
    {
        return !mComp(a, b) && !mComp(b, a);
    }

public:
    using key_type = T;
    using value_type = T;
    using size_type = typename Storage::size_type;
    using difference_type = typename Storage::difference_type;
    using key_compare = Compare;
    using value_compare = Compare;
    using allocator_type = Allocator;
    using reference = const value_type&;  // const - set elements are immutable
    using const_reference = const value_type&;
    using pointer = typename std::allocator_traits<Allocator>::const_pointer;
    using const_pointer = typename std::allocator_traits<Allocator>::const_pointer;
    // Both iterator types are const - set elements must not be modified through iterators
    // (modifying would break the sorted invariant)
    using iterator = typename Storage::const_iterator;
    using const_iterator = typename Storage::const_iterator;
    using reverse_iterator = typename Storage::const_reverse_iterator;
    using const_reverse_iterator = typename Storage::const_reverse_iterator;

private:
    // Convert const_iterator to internal mutable iterator for modifications
    InternalIterator toInternal(const_iterator it)
    {
        return mData.begin() + (it - mData.cbegin());
    }

public:

    // =========================================================================
    // Constructors
    // =========================================================================

    FlatSet() = default;

    explicit FlatSet(const Compare& comp, const Allocator& alloc = Allocator())
        : mData(alloc)
        , mComp(comp)
    {
    }

    explicit FlatSet(const Allocator& alloc)
        : mData(alloc)
    {
    }

    template <class InputIt>
    FlatSet(InputIt first,
            InputIt last,
            const Compare& comp = Compare(),
            const Allocator& alloc = Allocator())
        : mData(first, last, alloc)
        , mComp(comp)
    {
        std::stable_sort(mData.begin(), mData.end(), mComp);
        auto lastUnique =
            std::unique(mData.begin(), mData.end(), [this](const T& a, const T& b) {
                return elementsEquivalent(a, b);
            });
        mData.erase(lastUnique, mData.end());
    }

    /**
     * @brief Construct from pre-sorted, unique range (skips sort and dedup).
     * 
     * @warning Caller guarantees range is sorted by comp and has no duplicates.
     *          Passing unsorted or non-unique data is undefined behavior.
     */
    template <class InputIt>
    FlatSet(ordered_unique_range_t,
            InputIt first,
            InputIt last,
            const Compare& comp = Compare(),
            const Allocator& alloc = Allocator())
        : mData(first, last, alloc)
        , mComp(comp)
    {
        // Trust caller - no sort, no dedup
        FATP_ENFORCE(std::is_sorted(mData.begin(), mData.end(), mComp),
                "FlatSet: ordered_unique_range input was not sorted");
    }

    /**
     * @brief Construct from pre-sorted range that may have duplicates (skips sort only).
     * 
     * @warning Caller guarantees range is sorted by comp. First duplicate wins.
     */
    template <class InputIt>
    FlatSet(ordered_range_t,
            InputIt first,
            InputIt last,
            const Compare& comp = Compare(),
            const Allocator& alloc = Allocator())
        : mData(first, last, alloc)
        , mComp(comp)
    {
        // Trust caller on sorting - only dedup
        FATP_ENFORCE(std::is_sorted(mData.begin(), mData.end(), mComp),
                "FlatSet: ordered_range input was not sorted");
        auto lastUnique =
            std::unique(mData.begin(), mData.end(), [this](const T& a, const T& b) {
                return elementsEquivalent(a, b);
            });
        mData.erase(lastUnique, mData.end());
    }

    FlatSet(std::initializer_list<value_type> init,
            const Compare& comp = Compare(),
            const Allocator& alloc = Allocator())
        : FlatSet(init.begin(), init.end(), comp, alloc)
    {
    }

    // Explicitly defaulted copy/move operations with noexcept where possible
    FlatSet(const FlatSet&) = default;
    FlatSet& operator=(const FlatSet&) = default;

    FlatSet(FlatSet&& other) noexcept(
        std::is_nothrow_move_constructible_v<Storage> &&
        std::is_nothrow_move_constructible_v<Compare>)
        : mData(std::move(other.mData))
        , mComp(std::move(other.mComp))
    {
    }

    FlatSet& operator=(FlatSet&& other) noexcept(
        std::is_nothrow_move_assignable_v<Storage> &&
        std::is_nothrow_move_assignable_v<Compare>)
    {
        mData = std::move(other.mData);
        mComp = std::move(other.mComp);
        return *this;
    }

    // =========================================================================
    // Iterators
    // =========================================================================

    iterator begin() noexcept
    {
        return mData.begin();
    }

    const_iterator begin() const noexcept
    {
        return mData.begin();
    }

    iterator end() noexcept
    {
        return mData.end();
    }

    const_iterator end() const noexcept
    {
        return mData.end();
    }

    reverse_iterator rbegin() noexcept
    {
        return mData.rbegin();
    }

    const_reverse_iterator rbegin() const noexcept
    {
        return mData.rbegin();
    }

    reverse_iterator rend() noexcept
    {
        return mData.rend();
    }

    const_reverse_iterator rend() const noexcept
    {
        return mData.rend();
    }

    const_iterator cbegin() const noexcept
    {
        return mData.cbegin();
    }

    const_iterator cend() const noexcept
    {
        return mData.cend();
    }

    const_reverse_iterator crbegin() const noexcept
    {
        return mData.crbegin();
    }

    const_reverse_iterator crend() const noexcept
    {
        return mData.crend();
    }

    // =========================================================================
    // Capacity
    // =========================================================================

    [[nodiscard]] bool empty() const noexcept
    {
        return mData.empty();
    }

    [[nodiscard]] size_type size() const noexcept
    {
        return mData.size();
    }

    [[nodiscard]] size_type max_size() const noexcept
    {
        return mData.max_size();
    }

    [[nodiscard]] size_type capacity() const noexcept
    {
        return mData.capacity();
    }

    void reserve(size_type n)
    {
        mData.reserve(n);
    }

    void shrink_to_fit()
    {
        mData.shrink_to_fit();
    }

    // =========================================================================
    // Modifiers
    // =========================================================================

    void clear() noexcept
    {
        mData.clear();
    }

    std::pair<iterator, bool> insert(const value_type& value)
    {
        auto it = std::lower_bound(mData.begin(), mData.end(), value, mComp);
        if (it != mData.end() && !mComp(value, *it))
        {
            return {it, false};
        }
        return {mData.insert(it, value), true};
    }

    std::pair<iterator, bool> insert(value_type&& value)
    {
        auto it = std::lower_bound(mData.begin(), mData.end(), value, mComp);
        if (it != mData.end() && !mComp(value, *it))
        {
            return {it, false};
        }
        return {mData.insert(it, std::move(value)), true};
    }

    iterator insert(const_iterator hint, const value_type& value)
    {
        return insertWithHint(hint, value);
    }

    iterator insert(const_iterator hint, value_type&& value)
    {
        return insertWithHint(hint, std::move(value));
    }

    template <class InputIt>
    void insert(InputIt first, InputIt last)
    {
        if (first == last)
        {
            return;
        }

        size_type oldSize = mData.size();

        for (; first != last; ++first)
        {
            mData.push_back(*first);
        }

        auto mid = mData.begin() + static_cast<difference_type>(oldSize);
        std::stable_sort(mid, mData.end(), mComp);
        std::inplace_merge(mData.begin(), mid, mData.end(), mComp);

        auto lastUnique =
            std::unique(mData.begin(), mData.end(), [this](const T& a, const T& b) {
                return elementsEquivalent(a, b);
            });
        mData.erase(lastUnique, mData.end());
    }

    /**
     * @brief Insert from pre-sorted, unique range (skips sort of new elements).
     * 
     * @warning Caller guarantees range is sorted and unique. UB otherwise.
     */
    template <class InputIt>
    void insert(ordered_unique_range_t, InputIt first, InputIt last)
    {
        if (first == last)
        {
            return;
        }

        size_type oldSize = mData.size();

        for (; first != last; ++first)
        {
            mData.push_back(*first);
        }

        auto mid = mData.begin() + static_cast<difference_type>(oldSize);
        
        // Debug check: verify input was actually sorted
        FATP_ENFORCE(std::is_sorted(mid, mData.end(), mComp),
                "FlatSet::insert(ordered_unique_range): input was not sorted");

        // Skip sorting new elements - just merge
        std::inplace_merge(mData.begin(), mid, mData.end(), mComp);

        // Still need to dedup against existing elements
        auto lastUnique =
            std::unique(mData.begin(), mData.end(), [this](const T& a, const T& b) {
                return elementsEquivalent(a, b);
            });
        mData.erase(lastUnique, mData.end());
    }

    /**
     * @brief Insert from pre-sorted range that may have duplicates.
     */
    template <class InputIt>
    void insert(ordered_range_t, InputIt first, InputIt last)
    {
        // Same as ordered_unique_range - we dedup anyway
        insert(ordered_unique_range, first, last);
    }

    void insert(std::initializer_list<value_type> ilist)
    {
        insert(ilist.begin(), ilist.end());
    }

    template <class... Args>
    std::pair<iterator, bool> emplace(Args&&... args)
    {
        return insert(value_type(std::forward<Args>(args)...));
    }

    template <class... Args>
    iterator emplace_hint(const_iterator hint, Args&&... args)
    {
        return insertWithHint(hint, value_type(std::forward<Args>(args)...));
    }

    iterator erase(const_iterator pos)
    {
        FATP_ENFORCE(pos >= mData.cbegin() && pos < mData.cend(), "FlatSet::erase: invalid iterator");
        return mData.erase(toInternal(pos));
    }

    iterator erase(const_iterator first, const_iterator last)
    {
        FATP_ENFORCE(first >= mData.cbegin() && first <= mData.cend(),
                "FlatSet::erase: invalid first iterator");
        FATP_ENFORCE(last >= mData.cbegin() && last <= mData.cend(),
                "FlatSet::erase: invalid last iterator");
        FATP_ENFORCE(first <= last, "FlatSet::erase: invalid iterator range");
        return mData.erase(toInternal(first), toInternal(last));
    }

    size_type erase(const key_type& key)
    {
        auto it = std::lower_bound(mData.begin(), mData.end(), key, mComp);
        if (it == mData.end() || mComp(key, *it))
        {
            return 0;
        }
        mData.erase(it);
        return 1;
    }

    /// @brief Extracts an element from the container
    /// @param pos Iterator to the element to extract
    /// @return The extracted value (moved from container)
    /// @note The element is removed from the container after extraction
    value_type extract(const_iterator pos)
    {
        FATP_ENFORCE(pos >= cbegin() && pos < cend(), "FlatSet::extract: invalid iterator");
        auto internalIt = toInternal(pos);
        value_type result(std::move(*internalIt));
        mData.erase(internalIt);
        return result;
    }

    void swap(FlatSet& other) noexcept(std::is_nothrow_swappable_v<Storage> &&
                                       std::is_nothrow_swappable_v<Compare>)
    {
        mData.swap(other.mData);
        std::swap(mComp, other.mComp);
    }

    /**
     * @brief Merge elements from another FlatSet
     * 
     * Attempts to extract each element from source and insert it into *this.
     * If an element already exists in *this, it is left in source.
     * Uses O(n + m) merge algorithm since both containers are sorted.
     * 
     * @param source The FlatSet to merge from (will be modified)
     */
    void merge(FlatSet& source)
    {
        if (this == &source)
        {
            return;
        }
        if (source.empty())
        {
            return;
        }
        if (empty())
        {
            // Swap both data and comparator - correct for stateful comparators
            swap(source);
            return;
        }
        
        // Both containers are sorted - use merge algorithm for O(n + m) complexity
        Storage merged(mData.get_allocator());
        Storage remaining(source.mData.get_allocator());  // Elements that stay in source (duplicates)
        merged.reserve(size() + source.size());
        remaining.reserve(source.size());
        
        auto it1 = mData.begin();
        auto it2 = source.mData.begin();
        
        while (it1 != mData.end() && it2 != source.mData.end())
        {
            if (mComp(*it1, *it2))
            {
                merged.push_back(std::move(*it1));
                ++it1;
            }
            else if (mComp(*it2, *it1))
            {
                merged.push_back(std::move(*it2));
                ++it2;
            }
            else
            {
                // Elements are equivalent - keep ours, leave theirs in source
                merged.push_back(std::move(*it1));
                remaining.push_back(std::move(*it2));
                ++it1;
                ++it2;
            }
        }
        
        // Append remaining elements from this
        while (it1 != mData.end())
        {
            merged.push_back(std::move(*it1));
            ++it1;
        }
        
        // Append remaining elements from source (these get merged)
        while (it2 != source.mData.end())
        {
            merged.push_back(std::move(*it2));
            ++it2;
        }
        
        mData = std::move(merged);
        source.mData = std::move(remaining);
    }

    /**
     * @brief Merge elements from another FlatSet (rvalue overload)
     */
    void merge(FlatSet&& source)
    {
        merge(source);
    }

    // =========================================================================
    // Lookup
    // =========================================================================

    [[nodiscard]] size_type count(const key_type& key) const
    {
        return find(key) != end() ? 1 : 0;
    }

    [[nodiscard]] iterator find(const key_type& key)
    {
        auto it = lower_bound(key);
        return (it != end() && !mComp(key, *it)) ? it : end();
    }

    [[nodiscard]] const_iterator find(const key_type& key) const
    {
        auto it = lower_bound(key);
        return (it != end() && !mComp(key, *it)) ? it : end();
    }

    [[nodiscard]] bool contains(const key_type& key) const
    {
        return find(key) != end();
    }

    std::pair<iterator, iterator> equal_range(const key_type& key)
    {
        return {lower_bound(key), upper_bound(key)};
    }

    std::pair<const_iterator, const_iterator> equal_range(const key_type& key) const
    {
        return {lower_bound(key), upper_bound(key)};
    }

    iterator lower_bound(const key_type& key)
    {
        return std::lower_bound(mData.begin(), mData.end(), key, mComp);
    }

    const_iterator lower_bound(const key_type& key) const
    {
        return std::lower_bound(mData.begin(), mData.end(), key, mComp);
    }

    iterator upper_bound(const key_type& key)
    {
        return std::upper_bound(mData.begin(), mData.end(), key, mComp);
    }

    const_iterator upper_bound(const key_type& key) const
    {
        return std::upper_bound(mData.begin(), mData.end(), key, mComp);
    }

    // =========================================================================
    // Heterogeneous Lookup (requires Compare with is_transparent)
    // =========================================================================
    // These overloads are only enabled when the comparator has is_transparent,
    // allowing lookups without constructing a key_type object.

    template <typename K, typename C = Compare,
              typename = std::enable_if_t<HasIsTransparent<C>::value>>
    [[nodiscard]] size_type count(const K& key) const
    {
        return find(key) != end() ? 1 : 0;
    }

    template <typename K, typename C = Compare,
              typename = std::enable_if_t<HasIsTransparent<C>::value>>
    [[nodiscard]] iterator find(const K& key)
    {
        auto it = lower_bound(key);
        return (it != end() && elementsEquivalentHetero(*it, key)) ? it : end();
    }

    template <typename K, typename C = Compare,
              typename = std::enable_if_t<HasIsTransparent<C>::value>>
    [[nodiscard]] const_iterator find(const K& key) const
    {
        auto it = lower_bound(key);
        return (it != end() && elementsEquivalentHetero(*it, key)) ? it : end();
    }

    template <typename K, typename C = Compare,
              typename = std::enable_if_t<HasIsTransparent<C>::value>>
    [[nodiscard]] bool contains(const K& key) const
    {
        return find(key) != end();
    }

    template <typename K, typename C = Compare,
              typename = std::enable_if_t<HasIsTransparent<C>::value>>
    std::pair<iterator, iterator> equal_range(const K& key)
    {
        return {lower_bound(key), upper_bound(key)};
    }

    template <typename K, typename C = Compare,
              typename = std::enable_if_t<HasIsTransparent<C>::value>>
    std::pair<const_iterator, const_iterator> equal_range(const K& key) const
    {
        return {lower_bound(key), upper_bound(key)};
    }

    template <typename K, typename C = Compare,
              typename = std::enable_if_t<HasIsTransparent<C>::value>>
    iterator lower_bound(const K& key)
    {
        return std::lower_bound(mData.begin(), mData.end(), key, mComp);
    }

    template <typename K, typename C = Compare,
              typename = std::enable_if_t<HasIsTransparent<C>::value>>
    const_iterator lower_bound(const K& key) const
    {
        return std::lower_bound(mData.begin(), mData.end(), key, mComp);
    }

    template <typename K, typename C = Compare,
              typename = std::enable_if_t<HasIsTransparent<C>::value>>
    iterator upper_bound(const K& key)
    {
        return std::upper_bound(mData.begin(), mData.end(), key, mComp);
    }

    template <typename K, typename C = Compare,
              typename = std::enable_if_t<HasIsTransparent<C>::value>>
    const_iterator upper_bound(const K& key) const
    {
        return std::upper_bound(mData.begin(), mData.end(), key, mComp);
    }

    // =========================================================================
    // Observers
    // =========================================================================

    key_compare key_comp() const
    {
        return mComp;
    }

    value_compare value_comp() const
    {
        return mComp;
    }

    allocator_type get_allocator() const noexcept
    {
        return mData.get_allocator();
    }

private:
    /**
     * @brief Insert with hint optimization
     * 
     * If the hint is correct (value belongs at hint position), this is O(1) for
     * position finding + O(n) for the actual vector insert.
     * If hint is wrong, falls back to O(log n) binary search.
     */
    template <typename V>
    iterator insertWithHint(const_iterator hint, V&& value)
    {
        // Fast path: hint at end and value > last element (common for sorted insertion)
        if (hint == cend())
        {
            if (mData.empty() || mComp(mData.back(), value))
            {
                mData.push_back(std::forward<V>(value));
                return mData.end() - 1;
            }
            // Check if it's a duplicate of the last element
            if (!mComp(value, mData.back()))
            {
                return mData.end() - 1;  // Value equivalent to last, return existing
            }
        }
        // Fast path: hint at begin and value < first element
        else if (hint == cbegin())
        {
            if (mData.empty())
            {
                mData.push_back(std::forward<V>(value));
                return mData.begin();
            }
            if (mComp(value, mData.front()))
            {
                return mData.insert(mData.begin(), std::forward<V>(value));
            }
            // Check if it's a duplicate of the first element
            if (!mComp(mData.front(), value))
            {
                return mData.begin();  // Value equivalent to first, return existing
            }
        }
        // Check if hint is valid for middle positions
        else
        {
            auto hintIt = toInternal(hint);
            auto prevIt = hintIt - 1;
            
            // Valid hint: prev < value < hint
            if (mComp(*prevIt, value) && mComp(value, *hintIt))
            {
                return mData.insert(hintIt, std::forward<V>(value));
            }
            // Check if value is equivalent to prev (duplicate)
            if (!mComp(*prevIt, value) && !mComp(value, *prevIt))
            {
                return prevIt;
            }
            // Check if value is equivalent to hint (duplicate)
            if (!mComp(*hintIt, value) && !mComp(value, *hintIt))
            {
                return hintIt;
            }
        }
        
        // Hint was wrong - fall back to binary search
        auto it = std::lower_bound(mData.begin(), mData.end(), value, mComp);
        if (it != mData.end() && !mComp(value, *it))
        {
            return it;  // Value already exists
        }
        return mData.insert(it, std::forward<V>(value));
    }
};

// =============================================================================
// Comparison operators
// =============================================================================

template <typename T, typename C, typename A>
bool operator==(const FlatSet<T, C, A>& lhs, const FlatSet<T, C, A>& rhs)
{
    if (lhs.size() != rhs.size())
    {
        return false;
    }
    return std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

template <typename T, typename C, typename A>
bool operator!=(const FlatSet<T, C, A>& lhs, const FlatSet<T, C, A>& rhs)
{
    return !(lhs == rhs);
}

// =============================================================================
// Swap
// =============================================================================

template <typename T, typename C, typename A>
void swap(FlatSet<T, C, A>& lhs, FlatSet<T, C, A>& rhs) noexcept(noexcept(lhs.swap(rhs)))
{
    lhs.swap(rhs);
}

// =============================================================================
// Type trait specialization
// =============================================================================

template <typename T, typename C, typename A>
struct is_flat_set<FlatSet<T, C, A>> : std::true_type
{
};

} // namespace fat_p
