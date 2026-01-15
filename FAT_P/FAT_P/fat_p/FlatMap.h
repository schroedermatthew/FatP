/**
 * @file FlatMap.h
 * @brief Sorted vector-backed associative container with contiguous storage
 *
 * @layer Containers
 *
 * FlatMap provides an ordered key-value container backed by a sorted vector,
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
  component: FlatMap
  file_role: public_header
  path: fat_p/FlatMap.h
  namespace: fat_p
  layer: Containers
  summary: "Public header for FlatMap."
  api_stability: in_work
  related:
    docs_search: "FlatMap"
    tests:
      - tests/test_FatPTypeTraits.cpp
      - tests/test_FlatMap.cpp
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
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "enforce.h"
#include "FatPTypeTraits.h"

namespace fat_p
{

// =============================================================================
// Tag types for sorted input optimization (shared with FlatSet)
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
 *
 * @example
 *   std::vector<std::pair<int, std::string>> sorted_data = ...;
 *   FlatMap<int, std::string> map(ordered_unique_range, sorted_data.begin(), sorted_data.end());
 */
struct ordered_unique_range_t
{
    explicit ordered_unique_range_t() = default;
};
inline constexpr ordered_unique_range_t ordered_unique_range{};

/**
 * @brief Tag to indicate input range is already sorted but may have duplicates.
 *
 * Pass this tag when input is sorted but may contain duplicate keys.
 * This skips sorting but still runs deduplication (first key wins).
 */
struct ordered_range_t
{
    explicit ordered_range_t() = default;
};
inline constexpr ordered_range_t ordered_range{};

#endif // FATP_ORDERED_RANGE_TAGS_DEFINED

// Forward declaration
template <typename Key, typename T, typename Compare, typename Allocator>
class FlatMap;

template <typename BaseIterator, typename Key, typename T>
class FlatMapConstIterator;

// =============================================================================
// FlatMapIterator - Custom iterator that protects key immutability
// =============================================================================

template <typename BaseIterator, typename Key, typename T>
class FlatMapIterator
{
public:
    using iterator_category = std::random_access_iterator_tag;
    using difference_type = typename std::iterator_traits<BaseIterator>::difference_type;
    using value_type = std::pair<const Key, T>;
    using reference = std::pair<const Key&, T&>;

    struct ArrowProxy
    {
        reference mRef;

        reference* operator->()
        {
            return &mRef;
        }
    };

    using pointer = ArrowProxy;

private:
    BaseIterator mBase;

    template <typename, typename, typename, typename>
    friend class FlatMap;

    template <typename, typename, typename>
    friend class FlatMapConstIterator;

    BaseIterator base() const
    {
        return mBase;
    }

public:
    FlatMapIterator() = default;

    explicit FlatMapIterator(BaseIterator it)
        : mBase(it)
    {
    }

    reference operator*() const
    {
        return reference(mBase->first, mBase->second);
    }

    pointer operator->() const
    {
        return pointer{reference(mBase->first, mBase->second)};
    }

    FlatMapIterator& operator++()
    {
        ++mBase;
        return *this;
    }

    FlatMapIterator operator++(int)
    {
        FlatMapIterator tmp = *this;
        ++mBase;
        return tmp;
    }

    FlatMapIterator& operator--()
    {
        --mBase;
        return *this;
    }

    FlatMapIterator operator--(int)
    {
        FlatMapIterator tmp = *this;
        --mBase;
        return tmp;
    }

    FlatMapIterator& operator+=(difference_type n)
    {
        mBase += n;
        return *this;
    }

    FlatMapIterator& operator-=(difference_type n)
    {
        mBase -= n;
        return *this;
    }

    FlatMapIterator operator+(difference_type n) const
    {
        return FlatMapIterator(mBase + n);
    }

    FlatMapIterator operator-(difference_type n) const
    {
        return FlatMapIterator(mBase - n);
    }

    difference_type operator-(const FlatMapIterator& other) const
    {
        return mBase - other.mBase;
    }

    reference operator[](difference_type n) const
    {
        return *(*this + n);
    }

    bool operator==(const FlatMapIterator& other) const
    {
        return mBase == other.mBase;
    }

    bool operator!=(const FlatMapIterator& other) const
    {
        return mBase != other.mBase;
    }

    bool operator<(const FlatMapIterator& other) const
    {
        return mBase < other.mBase;
    }

    bool operator<=(const FlatMapIterator& other) const
    {
        return mBase <= other.mBase;
    }

    bool operator>(const FlatMapIterator& other) const
    {
        return mBase > other.mBase;
    }

    bool operator>=(const FlatMapIterator& other) const
    {
        return mBase >= other.mBase;
    }
};

template <typename BaseIterator, typename Key, typename T>
FlatMapIterator<BaseIterator, Key, T> operator+(typename FlatMapIterator<BaseIterator, Key, T>::difference_type n,
                                                const FlatMapIterator<BaseIterator, Key, T>& it)
{
    return it + n;
}

// =============================================================================
// FlatMapConstIterator - Const version of the custom iterator
// =============================================================================

template <typename BaseIterator, typename Key, typename T>
class FlatMapConstIterator
{
public:
    using iterator_category = std::random_access_iterator_tag;
    using difference_type = typename std::iterator_traits<BaseIterator>::difference_type;
    using value_type = std::pair<const Key, T>;
    using reference = std::pair<const Key&, const T&>;

    struct ArrowProxy
    {
        reference mRef;

        const reference* operator->() const
        {
            return &mRef;
        }
    };

    using pointer = ArrowProxy;

private:
    BaseIterator mBase;

    template <typename, typename, typename, typename>
    friend class FlatMap;

    BaseIterator base() const
    {
        return mBase;
    }

public:
    FlatMapConstIterator() = default;

    explicit FlatMapConstIterator(BaseIterator it)
        : mBase(it)
    {
    }

    template <typename OtherBase>
    FlatMapConstIterator(const FlatMapIterator<OtherBase, Key, T>& other)
        : mBase(other.base())
    {
    }

    reference operator*() const
    {
        return reference(mBase->first, mBase->second);
    }

    pointer operator->() const
    {
        return pointer{reference(mBase->first, mBase->second)};
    }

    FlatMapConstIterator& operator++()
    {
        ++mBase;
        return *this;
    }

    FlatMapConstIterator operator++(int)
    {
        FlatMapConstIterator tmp = *this;
        ++mBase;
        return tmp;
    }

    FlatMapConstIterator& operator--()
    {
        --mBase;
        return *this;
    }

    FlatMapConstIterator operator--(int)
    {
        FlatMapConstIterator tmp = *this;
        --mBase;
        return tmp;
    }

    FlatMapConstIterator& operator+=(difference_type n)
    {
        mBase += n;
        return *this;
    }

    FlatMapConstIterator& operator-=(difference_type n)
    {
        mBase -= n;
        return *this;
    }

    FlatMapConstIterator operator+(difference_type n) const
    {
        return FlatMapConstIterator(mBase + n);
    }

    FlatMapConstIterator operator-(difference_type n) const
    {
        return FlatMapConstIterator(mBase - n);
    }

    difference_type operator-(const FlatMapConstIterator& other) const
    {
        return mBase - other.mBase;
    }

    reference operator[](difference_type n) const
    {
        return *(*this + n);
    }

    bool operator==(const FlatMapConstIterator& other) const
    {
        return mBase == other.mBase;
    }

    bool operator!=(const FlatMapConstIterator& other) const
    {
        return mBase != other.mBase;
    }

    bool operator<(const FlatMapConstIterator& other) const
    {
        return mBase < other.mBase;
    }

    bool operator<=(const FlatMapConstIterator& other) const
    {
        return mBase <= other.mBase;
    }

    bool operator>(const FlatMapConstIterator& other) const
    {
        return mBase > other.mBase;
    }

    bool operator>=(const FlatMapConstIterator& other) const
    {
        return mBase >= other.mBase;
    }
};

template <typename BaseIterator, typename Key, typename T>
FlatMapConstIterator<BaseIterator, Key, T>
operator+(typename FlatMapConstIterator<BaseIterator, Key, T>::difference_type n,
          const FlatMapConstIterator<BaseIterator, Key, T>& it)
{
    return it + n;
}

// =============================================================================
// FlatMap - Sorted vector-backed associative container
// =============================================================================

template <typename Key,
          typename T,
          typename Compare = std::less<Key>,
          typename Allocator = std::allocator<std::pair<const Key, T>>>
class FlatMap
{
private:
    using InternalPair = std::pair<Key, T>;
    using InternalAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<InternalPair>;
    using Storage = std::vector<InternalPair, InternalAllocator>;

    Storage mData;
    Compare mComp;

    // Helper to detect if Compare has is_transparent
    template <typename C, typename = void>
    struct HasIsTransparent : std::false_type
    {
    };

    template <typename C>
    struct HasIsTransparent<C, std::void_t<typename C::is_transparent>> : std::true_type
    {
    };

    struct KeyCompare
    {
        Compare mComp;

        // Enable is_transparent if the underlying comparator has it
        // This allows heterogeneous lookup (e.g., find("literal") without creating std::string)
        template <typename C = Compare, typename = std::enable_if_t<HasIsTransparent<C>::value>>
        using is_transparent = typename C::is_transparent;

        // Standard overloads for InternalPair comparisons
        bool operator()(const InternalPair& a, const InternalPair& b) const
        {
            return mComp(a.first, b.first);
        }

        bool operator()(const InternalPair& a, const Key& b) const
        {
            return mComp(a.first, b);
        }

        bool operator()(const Key& a, const InternalPair& b) const
        {
            return mComp(a, b.first);
        }

        // Heterogeneous lookup overloads - enabled when Compare has is_transparent
        template <typename K, typename C = Compare, typename = std::enable_if_t<HasIsTransparent<C>::value>>
        bool operator()(const InternalPair& a, const K& b) const
        {
            return mComp(a.first, b);
        }

        template <typename K, typename C = Compare, typename = std::enable_if_t<HasIsTransparent<C>::value>>
        bool operator()(const K& a, const InternalPair& b) const
        {
            return mComp(a, b.first);
        }
    };

    KeyCompare keyValueComp() const
    {
        return KeyCompare{mComp};
    }

    bool keysEquivalent(const Key& a, const Key& b) const
    {
        return !mComp(a, b) && !mComp(b, a);
    }

public:
    using key_type = Key;
    using mapped_type = T;
    using value_type = std::pair<const Key, T>;
    using size_type = typename Storage::size_type;
    using difference_type = typename Storage::difference_type;
    using key_compare = Compare;
    using allocator_type = Allocator;
    using iterator = FlatMapIterator<typename Storage::iterator, Key, T>;
    using const_iterator = FlatMapConstIterator<typename Storage::const_iterator, Key, T>;
    // Note: iterator returns proxy pair std::pair<const Key&, T&>, not value_type&
    using reference = typename iterator::reference;
    using const_reference = typename const_iterator::reference;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    // =========================================================================
    // Constructors
    // =========================================================================

    FlatMap() = default;

    explicit FlatMap(const Compare& comp, const Allocator& alloc = Allocator())
        : mData(InternalAllocator(alloc))
        , mComp(comp)
    {
    }

    explicit FlatMap(const Allocator& alloc)
        : mData(InternalAllocator(alloc))
    {
    }

    template <class InputIt>
    FlatMap(InputIt first, InputIt last, const Compare& comp = Compare(), const Allocator& alloc = Allocator())
        : mData(InternalAllocator(alloc))
        , mComp(comp)
    {
        for (; first != last; ++first)
        {
            mData.emplace_back(first->first, first->second);
        }
        std::stable_sort(mData.begin(), mData.end(), keyValueComp());
        auto lastUnique = std::unique(mData.begin(), mData.end(), [this](const InternalPair& a, const InternalPair& b) {
            return keysEquivalent(a.first, b.first);
        });
        mData.erase(lastUnique, mData.end());
    }

    /**
     * @brief Construct from pre-sorted, unique range (skips sort and dedup).
     *
     * @warning Caller guarantees range is sorted by comp and has no duplicate keys.
     *          Passing unsorted or non-unique data is undefined behavior.
     */
    template <class InputIt>
    FlatMap(ordered_unique_range_t,
            InputIt first,
            InputIt last,
            const Compare& comp = Compare(),
            const Allocator& alloc = Allocator())
        : mData(InternalAllocator(alloc))
        , mComp(comp)
    {
        for (; first != last; ++first)
        {
            mData.emplace_back(first->first, first->second);
        }
        // Trust caller - no sort, no dedup
        FATP_ENFORCE(std::is_sorted(mData.begin(), mData.end(), keyValueComp()),
                     "FlatMap: ordered_unique_range input was not sorted");
    }

    /**
     * @brief Construct from pre-sorted range that may have duplicates (skips sort only).
     *
     * @warning Caller guarantees range is sorted by comp. First duplicate wins.
     */
    template <class InputIt>
    FlatMap(ordered_range_t,
            InputIt first,
            InputIt last,
            const Compare& comp = Compare(),
            const Allocator& alloc = Allocator())
        : mData(InternalAllocator(alloc))
        , mComp(comp)
    {
        for (; first != last; ++first)
        {
            mData.emplace_back(first->first, first->second);
        }
        // Trust caller on sorting - only dedup
        FATP_ENFORCE(std::is_sorted(mData.begin(), mData.end(), keyValueComp()),
                     "FlatMap: ordered_range input was not sorted");
        auto lastUnique = std::unique(mData.begin(), mData.end(), [this](const InternalPair& a, const InternalPair& b) {
            return keysEquivalent(a.first, b.first);
        });
        mData.erase(lastUnique, mData.end());
    }

    FlatMap(std::initializer_list<value_type> init,
            const Compare& comp = Compare(),
            const Allocator& alloc = Allocator())
        : FlatMap(init.begin(), init.end(), comp, alloc)
    {
    }

    // Explicitly defaulted copy/move operations with noexcept where possible
    FlatMap(const FlatMap&) = default;
    FlatMap& operator=(const FlatMap&) = default;

    FlatMap(FlatMap&& other) noexcept(std::is_nothrow_move_constructible_v<Storage> &&
                                      std::is_nothrow_move_constructible_v<Compare>)
        : mData(std::move(other.mData))
        , mComp(std::move(other.mComp))
    {
    }

    FlatMap& operator=(FlatMap&& other) noexcept(std::is_nothrow_move_assignable_v<Storage> &&
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
        return iterator(mData.begin());
    }

    const_iterator begin() const noexcept
    {
        return const_iterator(mData.begin());
    }

    iterator end() noexcept
    {
        return iterator(mData.end());
    }

    const_iterator end() const noexcept
    {
        return const_iterator(mData.end());
    }

    reverse_iterator rbegin() noexcept
    {
        return reverse_iterator(end());
    }

    const_reverse_iterator rbegin() const noexcept
    {
        return const_reverse_iterator(end());
    }

    reverse_iterator rend() noexcept
    {
        return reverse_iterator(begin());
    }

    const_reverse_iterator rend() const noexcept
    {
        return const_reverse_iterator(begin());
    }

    const_iterator cbegin() const noexcept
    {
        return const_iterator(mData.cbegin());
    }

    const_iterator cend() const noexcept
    {
        return const_iterator(mData.cend());
    }

    const_reverse_iterator crbegin() const noexcept
    {
        return const_reverse_iterator(cend());
    }

    const_reverse_iterator crend() const noexcept
    {
        return const_reverse_iterator(cbegin());
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
    // Element access
    // =========================================================================

    [[nodiscard]] T& at(const Key& key)
    {
        auto it = lowerBoundInternal(key);
        if (it == mData.end() || mComp(key, it->first))
        {
            throw std::out_of_range("FlatMap::at: key not found");
        }
        return it->second;
    }

    [[nodiscard]] const T& at(const Key& key) const
    {
        auto it = lowerBoundInternal(key);
        if (it == mData.end() || mComp(key, it->first))
        {
            throw std::out_of_range("FlatMap::at: key not found");
        }
        return it->second;
    }

    T& operator[](const Key& key)
    {
        auto it = lowerBoundInternal(key);
        if (it == mData.end() || mComp(key, it->first))
        {
            it = mData.insert(it, InternalPair(key, T()));
        }
        return it->second;
    }

    T& operator[](Key&& key)
    {
        auto it = lowerBoundInternal(key);
        if (it == mData.end() || mComp(key, it->first))
        {
            it = mData.insert(it, InternalPair(std::move(key), T()));
        }
        return it->second;
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
        auto it = lowerBoundInternal(value.first);
        if (it != mData.end() && !mComp(value.first, it->first))
        {
            return {iterator(it), false};
        }
        return {iterator(mData.insert(it, InternalPair(value.first, value.second))), true};
    }

    std::pair<iterator, bool> insert(value_type&& value)
    {
        auto it = lowerBoundInternal(value.first);
        if (it != mData.end() && !mComp(value.first, it->first))
        {
            return {iterator(it), false};
        }
        auto inserted = mData.insert(it, InternalPair(std::move(value.first), std::move(value.second)));
        return {iterator(inserted), true};
    }

    iterator insert(const_iterator hint, const value_type& value)
    {
        return insertWithHint(hint, value.first, value.second);
    }

    iterator insert(const_iterator hint, value_type&& value)
    {
        return insertWithHint(hint, std::move(value.first), std::move(value.second));
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
            mData.emplace_back(first->first, first->second);
        }

        auto mid = mData.begin() + static_cast<difference_type>(oldSize);
        std::stable_sort(mid, mData.end(), keyValueComp());
        std::inplace_merge(mData.begin(), mid, mData.end(), keyValueComp());

        auto lastUnique = std::unique(mData.begin(), mData.end(), [this](const InternalPair& a, const InternalPair& b) {
            return keysEquivalent(a.first, b.first);
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
            mData.emplace_back(first->first, first->second);
        }

        auto mid = mData.begin() + static_cast<difference_type>(oldSize);

        // Debug check: verify input was actually sorted
        FATP_ENFORCE(std::is_sorted(mid, mData.end(), keyValueComp()),
                     "FlatMap::insert(ordered_unique_range): input was not sorted");

        // Skip sorting new elements - just merge
        std::inplace_merge(mData.begin(), mid, mData.end(), keyValueComp());

        // Still need to dedup against existing elements
        auto lastUnique = std::unique(mData.begin(), mData.end(), [this](const InternalPair& a, const InternalPair& b) {
            return keysEquivalent(a.first, b.first);
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

    template <class M>
    std::pair<iterator, bool> insert_or_assign(const Key& k, M&& obj)
    {
        auto it = lowerBoundInternal(k);
        if (it != mData.end() && !mComp(k, it->first))
        {
            it->second = std::forward<M>(obj);
            return {iterator(it), false};
        }
        return {iterator(mData.insert(it, InternalPair(k, std::forward<M>(obj)))), true};
    }

    template <class M>
    std::pair<iterator, bool> insert_or_assign(Key&& k, M&& obj)
    {
        auto it = lowerBoundInternal(k);
        if (it != mData.end() && !mComp(k, it->first))
        {
            it->second = std::forward<M>(obj);
            return {iterator(it), false};
        }
        return {iterator(mData.insert(it, InternalPair(std::move(k), std::forward<M>(obj)))), true};
    }

    template <class... Args>
    std::pair<iterator, bool> emplace(Args&&... args)
    {
        InternalPair temp(std::forward<Args>(args)...);
        auto it = lowerBoundInternal(temp.first);
        if (it != mData.end() && !mComp(temp.first, it->first))
        {
            return {iterator(it), false};
        }
        return {iterator(mData.insert(it, std::move(temp))), true};
    }

    template <class... Args>
    iterator emplace_hint(const_iterator hint, Args&&... args)
    {
        // Construct the pair first to get the key
        InternalPair temp(std::forward<Args>(args)...);
        return insertWithHint(hint, std::move(temp.first), std::move(temp.second));
    }

    template <class... Args>
    std::pair<iterator, bool> try_emplace(const Key& k, Args&&... args)
    {
        auto it = lowerBoundInternal(k);
        if (it != mData.end() && !mComp(k, it->first))
        {
            return {iterator(it), false};
        }
        auto inserted = mData.insert(it,
                                     InternalPair(std::piecewise_construct,
                                                  std::forward_as_tuple(k),
                                                  std::forward_as_tuple(std::forward<Args>(args)...)));
        return {iterator(inserted), true};
    }

    template <class... Args>
    std::pair<iterator, bool> try_emplace(Key&& k, Args&&... args)
    {
        auto it = lowerBoundInternal(k);
        if (it != mData.end() && !mComp(k, it->first))
        {
            return {iterator(it), false};
        }
        auto inserted = mData.insert(it,
                                     InternalPair(std::piecewise_construct,
                                                  std::forward_as_tuple(std::move(k)),
                                                  std::forward_as_tuple(std::forward<Args>(args)...)));
        return {iterator(inserted), true};
    }

    iterator erase(iterator pos)
    {
        FATP_ENFORCE(pos.base() >= mData.begin() && pos.base() < mData.end(), "FlatMap::erase: invalid iterator");
        return iterator(mData.erase(pos.base()));
    }

    iterator erase(const_iterator pos)
    {
        FATP_ENFORCE(pos.base() >= mData.cbegin() && pos.base() < mData.cend(), "FlatMap::erase: invalid iterator");
        return iterator(mData.erase(pos.base()));
    }

    iterator erase(const_iterator first, const_iterator last)
    {
        FATP_ENFORCE(first.base() >= mData.cbegin() && first.base() <= mData.cend(),
                     "FlatMap::erase: invalid first iterator");
        FATP_ENFORCE(last.base() >= mData.cbegin() && last.base() <= mData.cend(),
                     "FlatMap::erase: invalid last iterator");
        FATP_ENFORCE(first.base() <= last.base(), "FlatMap::erase: invalid iterator range");
        return iterator(mData.erase(first.base(), last.base()));
    }

    size_type erase(const Key& key)
    {
        auto it = lowerBoundInternal(key);
        if (it == mData.end() || mComp(key, it->first))
        {
            return 0;
        }
        mData.erase(it);
        return 1;
    }

    /// @brief Extracts an element from the container
    /// @param pos Iterator to the element to extract
    /// @return The extracted key-value pair (moved from container)
    /// @note The element is removed from the container after extraction
    value_type extract(const_iterator pos)
    {
        FATP_ENFORCE(pos >= cbegin() && pos < cend(), "FlatMap::extract: invalid iterator");
        auto internalIt = mData.begin() + (pos.base() - mData.cbegin());
        value_type result(std::move(internalIt->first), std::move(internalIt->second));
        mData.erase(internalIt);
        return result;
    }

    void swap(FlatMap& other) noexcept(std::is_nothrow_swappable_v<Storage> && std::is_nothrow_swappable_v<Compare>)
    {
        mData.swap(other.mData);
        std::swap(mComp, other.mComp);
    }

    /**
     * @brief Merge elements from another FlatMap
     *
     * Attempts to extract each element from source and insert it into *this.
     * If a key already exists in *this, the element is left in source.
     * Uses O(n + m) merge algorithm since both containers are sorted.
     *
     * @param source The FlatMap to merge from (will be modified)
     */
    void merge(FlatMap& source)
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
        Storage remaining(source.mData.get_allocator()); // Elements that stay in source (duplicates)
        merged.reserve(size() + source.size());
        remaining.reserve(source.size());

        auto it1 = mData.begin();
        auto it2 = source.mData.begin();

        while (it1 != mData.end() && it2 != source.mData.end())
        {
            if (mComp(it1->first, it2->first))
            {
                merged.push_back(std::move(*it1));
                ++it1;
            }
            else if (mComp(it2->first, it1->first))
            {
                merged.push_back(std::move(*it2));
                ++it2;
            }
            else
            {
                // Keys are equivalent - keep ours, leave theirs in source
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
     * @brief Merge elements from another FlatMap (rvalue overload)
     */
    void merge(FlatMap&& source)
    {
        merge(source);
    }

    // =========================================================================
    // Lookup
    // =========================================================================

    [[nodiscard]] size_type count(const Key& key) const
    {
        return find(key) != end() ? 1 : 0;
    }

    [[nodiscard]] iterator find(const Key& key)
    {
        auto it = lowerBoundInternal(key);
        if (it != mData.end() && !mComp(key, it->first))
        {
            return iterator(it);
        }
        return end();
    }

    [[nodiscard]] const_iterator find(const Key& key) const
    {
        auto it = lowerBoundInternal(key);
        if (it != mData.end() && !mComp(key, it->first))
        {
            return const_iterator(it);
        }
        return end();
    }

    [[nodiscard]] bool contains(const Key& key) const
    {
        return find(key) != end();
    }

    std::pair<iterator, iterator> equal_range(const Key& key)
    {
        return {lower_bound(key), upper_bound(key)};
    }

    std::pair<const_iterator, const_iterator> equal_range(const Key& key) const
    {
        return {lower_bound(key), upper_bound(key)};
    }

    iterator lower_bound(const Key& key)
    {
        return iterator(lowerBoundInternal(key));
    }

    const_iterator lower_bound(const Key& key) const
    {
        return const_iterator(lowerBoundInternal(key));
    }

    iterator upper_bound(const Key& key)
    {
        return iterator(std::upper_bound(mData.begin(), mData.end(), key, keyValueComp()));
    }

    const_iterator upper_bound(const Key& key) const
    {
        return const_iterator(std::upper_bound(mData.begin(), mData.end(), key, keyValueComp()));
    }

    // =========================================================================
    // Heterogeneous Lookup (requires Compare with is_transparent)
    // =========================================================================
    // These overloads are only enabled when the comparator has is_transparent,
    // allowing lookups without constructing a Key object (e.g., find("literal")
    // on FlatMap<std::string, T, std::less<>> avoids std::string construction).

    template <typename K, typename C = Compare, typename = std::enable_if_t<HasIsTransparent<C>::value>>
    [[nodiscard]] size_type count(const K& key) const
    {
        return find(key) != end() ? 1 : 0;
    }

    template <typename K, typename C = Compare, typename = std::enable_if_t<HasIsTransparent<C>::value>>
    [[nodiscard]] iterator find(const K& key)
    {
        auto it = std::lower_bound(mData.begin(), mData.end(), key, keyValueComp());
        if (it != mData.end() && !mComp(key, it->first) && !mComp(it->first, key))
        {
            return iterator(it);
        }
        return end();
    }

    template <typename K, typename C = Compare, typename = std::enable_if_t<HasIsTransparent<C>::value>>
    [[nodiscard]] const_iterator find(const K& key) const
    {
        auto it = std::lower_bound(mData.begin(), mData.end(), key, keyValueComp());
        if (it != mData.end() && !mComp(key, it->first) && !mComp(it->first, key))
        {
            return const_iterator(it);
        }
        return end();
    }

    template <typename K, typename C = Compare, typename = std::enable_if_t<HasIsTransparent<C>::value>>
    [[nodiscard]] bool contains(const K& key) const
    {
        return find(key) != end();
    }

    template <typename K, typename C = Compare, typename = std::enable_if_t<HasIsTransparent<C>::value>>
    std::pair<iterator, iterator> equal_range(const K& key)
    {
        return {lower_bound(key), upper_bound(key)};
    }

    template <typename K, typename C = Compare, typename = std::enable_if_t<HasIsTransparent<C>::value>>
    std::pair<const_iterator, const_iterator> equal_range(const K& key) const
    {
        return {lower_bound(key), upper_bound(key)};
    }

    template <typename K, typename C = Compare, typename = std::enable_if_t<HasIsTransparent<C>::value>>
    iterator lower_bound(const K& key)
    {
        return iterator(std::lower_bound(mData.begin(), mData.end(), key, keyValueComp()));
    }

    template <typename K, typename C = Compare, typename = std::enable_if_t<HasIsTransparent<C>::value>>
    const_iterator lower_bound(const K& key) const
    {
        return const_iterator(std::lower_bound(mData.begin(), mData.end(), key, keyValueComp()));
    }

    template <typename K, typename C = Compare, typename = std::enable_if_t<HasIsTransparent<C>::value>>
    iterator upper_bound(const K& key)
    {
        return iterator(std::upper_bound(mData.begin(), mData.end(), key, keyValueComp()));
    }

    template <typename K, typename C = Compare, typename = std::enable_if_t<HasIsTransparent<C>::value>>
    const_iterator upper_bound(const K& key) const
    {
        return const_iterator(std::upper_bound(mData.begin(), mData.end(), key, keyValueComp()));
    }

    // =========================================================================
    // Observers
    // =========================================================================

    key_compare key_comp() const
    {
        return mComp;
    }

    struct value_compare
    {
        Compare mComp;

        bool operator()(const value_type& lhs, const value_type& rhs) const
        {
            return mComp(lhs.first, rhs.first);
        }
    };

    value_compare value_comp() const
    {
        return value_compare{mComp};
    }

    allocator_type get_allocator() const noexcept
    {
        return allocator_type(mData.get_allocator());
    }

private:
    typename Storage::iterator lowerBoundInternal(const Key& key)
    {
        return std::lower_bound(mData.begin(), mData.end(), key, keyValueComp());
    }

    typename Storage::const_iterator lowerBoundInternal(const Key& key) const
    {
        return std::lower_bound(mData.begin(), mData.end(), key, keyValueComp());
    }

    /**
     * @brief Insert with hint optimization
     *
     * If the hint is correct (key belongs at hint position), this is O(1) for
     * position finding + O(n) for the actual vector insert.
     * If hint is wrong, falls back to O(log n) binary search.
     *
     * Hint is considered correct if:
     * - hint == end() and (empty or key > back)
     * - hint == begin() and (empty or key < front)
     * - Otherwise: prev(hint)->first < key < hint->first
     */
    template <typename K, typename V>
    iterator insertWithHint(const_iterator hint, K&& key, V&& value)
    {
        // Fast path: hint at end and key > last element (common for sorted insertion)
        if (hint == cend())
        {
            if (mData.empty() || mComp(mData.back().first, key))
            {
                mData.emplace_back(std::forward<K>(key), std::forward<V>(value));
                return iterator(mData.end() - 1);
            }
            // Check if it's a duplicate of the last element
            if (!mComp(key, mData.back().first))
            {
                return iterator(mData.end() - 1); // Key equivalent to last, return existing
            }
        }
        // Fast path: hint at begin and key < first element
        else if (hint == cbegin())
        {
            if (mData.empty())
            {
                mData.emplace_back(std::forward<K>(key), std::forward<V>(value));
                return iterator(mData.begin());
            }
            if (mComp(key, mData.front().first))
            {
                auto it = mData.insert(mData.begin(), InternalPair(std::forward<K>(key), std::forward<V>(value)));
                return iterator(it);
            }
            // Check if it's a duplicate of the first element
            if (!mComp(mData.front().first, key))
            {
                return iterator(mData.begin()); // Key equivalent to first, return existing
            }
        }
        // Check if hint is valid for middle positions
        else
        {
            auto hintBase = hint.base();
            auto prevIt = hintBase - 1;

            // Valid hint: prev < key < hint
            if (mComp(prevIt->first, key) && mComp(key, hintBase->first))
            {
                auto insertPos = mData.begin() + (hintBase - mData.cbegin());
                auto it = mData.insert(insertPos, InternalPair(std::forward<K>(key), std::forward<V>(value)));
                return iterator(it);
            }
            // Check if key is equivalent to prev (duplicate)
            if (!mComp(prevIt->first, key) && !mComp(key, prevIt->first))
            {
                return iterator(mData.begin() + (prevIt - mData.cbegin()));
            }
            // Check if key is equivalent to hint (duplicate)
            if (!mComp(hintBase->first, key) && !mComp(key, hintBase->first))
            {
                return iterator(mData.begin() + (hintBase - mData.cbegin()));
            }
        }

        // Hint was wrong - fall back to binary search
        auto it = lowerBoundInternal(key);
        if (it != mData.end() && !mComp(key, it->first))
        {
            return iterator(it); // Key already exists
        }
        return iterator(mData.insert(it, InternalPair(std::forward<K>(key), std::forward<V>(value))));
    }
};

// =============================================================================
// Comparison operators
// =============================================================================

template <typename K, typename V, typename C, typename A>
bool operator==(const FlatMap<K, V, C, A>& lhs, const FlatMap<K, V, C, A>& rhs)
{
    if (lhs.size() != rhs.size())
    {
        return false;
    }
    auto lit = lhs.begin();
    auto rit = rhs.begin();
    for (; lit != lhs.end(); ++lit, ++rit)
    {
        if ((*lit).first != (*rit).first || (*lit).second != (*rit).second)
        {
            return false;
        }
    }
    return true;
}

template <typename K, typename V, typename C, typename A>
bool operator!=(const FlatMap<K, V, C, A>& lhs, const FlatMap<K, V, C, A>& rhs)
{
    return !(lhs == rhs);
}

// =============================================================================
// Swap
// =============================================================================

template <typename K, typename V, typename C, typename A>
void swap(FlatMap<K, V, C, A>& lhs, FlatMap<K, V, C, A>& rhs) noexcept(noexcept(lhs.swap(rhs)))
{
    lhs.swap(rhs);
}

// =============================================================================
// Type trait specialization
// =============================================================================

template <typename K, typename V, typename C, typename A>
struct is_flat_map<FlatMap<K, V, C, A>> : std::true_type
{
};

} // namespace fat_p
