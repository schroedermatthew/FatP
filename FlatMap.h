// FlatMap.h
#pragma once

#include <algorithm>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include "enforce.h"
#include "FatPTypeTraits.h"

namespace fat_p
{

// Forward declaration
template <typename Key,
          typename T,
          typename Compare,
          typename Allocator>
class FlatMap;

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

    struct arrow_proxy
    {
        reference ref;

        reference* operator->()
        {
            return &ref;
        }
    };

    using pointer = arrow_proxy;

private:
    BaseIterator base_;

public:
    FlatMapIterator() = default;

    explicit FlatMapIterator(BaseIterator it)
        : base_(it)
    {
    }

    reference operator*() const
    {
        return reference(base_->first, base_->second);
    }

    pointer operator->() const
    {
        return pointer{reference(base_->first, base_->second)};
    }

    FlatMapIterator& operator++()
    {
        ++base_;
        return *this;
    }

    FlatMapIterator operator++(int)
    {
        FlatMapIterator tmp = *this;
        ++base_;
        return tmp;
    }

    FlatMapIterator& operator--()
    {
        --base_;
        return *this;
    }

    FlatMapIterator operator--(int)
    {
        FlatMapIterator tmp = *this;
        --base_;
        return tmp;
    }

    FlatMapIterator& operator+=(difference_type n)
    {
        base_ += n;
        return *this;
    }

    FlatMapIterator& operator-=(difference_type n)
    {
        base_ -= n;
        return *this;
    }

    FlatMapIterator operator+(difference_type n) const
    {
        return FlatMapIterator(base_ + n);
    }

    FlatMapIterator operator-(difference_type n) const
    {
        return FlatMapIterator(base_ - n);
    }

    difference_type operator-(const FlatMapIterator& other) const
    {
        return base_ - other.base_;
    }

    reference operator[](difference_type n) const
    {
        return *(*this + n);
    }

    bool operator==(const FlatMapIterator& other) const
    {
        return base_ == other.base_;
    }

    bool operator!=(const FlatMapIterator& other) const
    {
        return base_ != other.base_;
    }

    bool operator<(const FlatMapIterator& other) const
    {
        return base_ < other.base_;
    }

    bool operator<=(const FlatMapIterator& other) const
    {
        return base_ <= other.base_;
    }

    bool operator>(const FlatMapIterator& other) const
    {
        return base_ > other.base_;
    }

    bool operator>=(const FlatMapIterator& other) const
    {
        return base_ >= other.base_;
    }

    BaseIterator base() const
    {
        return base_;
    }
};

template <typename BaseIterator, typename Key, typename T>
FlatMapIterator<BaseIterator, Key, T> operator+(
    typename FlatMapIterator<BaseIterator, Key, T>::difference_type n,
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

    struct arrow_proxy
    {
        reference ref;

        const reference* operator->() const
        {
            return &ref;
        }
    };

    using pointer = arrow_proxy;

private:
    BaseIterator base_;

public:
    FlatMapConstIterator() = default;

    explicit FlatMapConstIterator(BaseIterator it)
        : base_(it)
    {
    }

    template <typename OtherBase>
    FlatMapConstIterator(const FlatMapIterator<OtherBase, Key, T>& other)
        : base_(other.base())
    {
    }

    reference operator*() const
    {
        return reference(base_->first, base_->second);
    }

    pointer operator->() const
    {
        return pointer{reference(base_->first, base_->second)};
    }

    FlatMapConstIterator& operator++()
    {
        ++base_;
        return *this;
    }

    FlatMapConstIterator operator++(int)
    {
        FlatMapConstIterator tmp = *this;
        ++base_;
        return tmp;
    }

    FlatMapConstIterator& operator--()
    {
        --base_;
        return *this;
    }

    FlatMapConstIterator operator--(int)
    {
        FlatMapConstIterator tmp = *this;
        --base_;
        return tmp;
    }

    FlatMapConstIterator& operator+=(difference_type n)
    {
        base_ += n;
        return *this;
    }

    FlatMapConstIterator& operator-=(difference_type n)
    {
        base_ -= n;
        return *this;
    }

    FlatMapConstIterator operator+(difference_type n) const
    {
        return FlatMapConstIterator(base_ + n);
    }

    FlatMapConstIterator operator-(difference_type n) const
    {
        return FlatMapConstIterator(base_ - n);
    }

    difference_type operator-(const FlatMapConstIterator& other) const
    {
        return base_ - other.base_;
    }

    reference operator[](difference_type n) const
    {
        return *(*this + n);
    }

    bool operator==(const FlatMapConstIterator& other) const
    {
        return base_ == other.base_;
    }

    bool operator!=(const FlatMapConstIterator& other) const
    {
        return base_ != other.base_;
    }

    bool operator<(const FlatMapConstIterator& other) const
    {
        return base_ < other.base_;
    }

    bool operator<=(const FlatMapConstIterator& other) const
    {
        return base_ <= other.base_;
    }

    bool operator>(const FlatMapConstIterator& other) const
    {
        return base_ > other.base_;
    }

    bool operator>=(const FlatMapConstIterator& other) const
    {
        return base_ >= other.base_;
    }

    BaseIterator base() const
    {
        return base_;
    }
};

template <typename BaseIterator, typename Key, typename T>
FlatMapConstIterator<BaseIterator, Key, T> operator+(
    typename FlatMapConstIterator<BaseIterator, Key, T>::difference_type n,
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
    using InternalAllocator =
        typename std::allocator_traits<Allocator>::template rebind_alloc<InternalPair>;
    using Storage = std::vector<InternalPair, InternalAllocator>;

    Storage data_;
    Compare comp_;

    // Helper to detect if Compare has is_transparent
    template <typename C, typename = void>
    struct has_is_transparent : std::false_type {};
    
    template <typename C>
    struct has_is_transparent<C, std::void_t<typename C::is_transparent>> : std::true_type {};

    struct KeyCompare
    {
        Compare comp;
        
        // Enable is_transparent if the underlying comparator has it
        // This allows heterogeneous lookup (e.g., find("literal") without creating std::string)
        template <typename C = Compare, 
                  typename = std::enable_if_t<has_is_transparent<C>::value>>
        using is_transparent = typename C::is_transparent;

        // Standard overloads for InternalPair comparisons
        bool operator()(const InternalPair& a, const InternalPair& b) const
        {
            return comp(a.first, b.first);
        }

        bool operator()(const InternalPair& a, const Key& b) const
        {
            return comp(a.first, b);
        }

        bool operator()(const Key& a, const InternalPair& b) const
        {
            return comp(a, b.first);
        }
        
        // Heterogeneous lookup overloads - enabled when Compare has is_transparent
        template <typename K, 
                  typename C = Compare,
                  typename = std::enable_if_t<has_is_transparent<C>::value>>
        bool operator()(const InternalPair& a, const K& b) const
        {
            return comp(a.first, b);
        }
        
        template <typename K,
                  typename C = Compare,
                  typename = std::enable_if_t<has_is_transparent<C>::value>>
        bool operator()(const K& a, const InternalPair& b) const
        {
            return comp(a, b.first);
        }
    };

    KeyCompare key_value_comp() const
    {
        return KeyCompare{comp_};
    }

    bool keys_equivalent(const Key& a, const Key& b) const
    {
        return !comp_(a, b) && !comp_(b, a);
    }

public:
    using key_type = Key;
    using mapped_type = T;
    using value_type = std::pair<const Key, T>;
    using size_type = typename Storage::size_type;
    using difference_type = typename Storage::difference_type;
    using key_compare = Compare;
    using allocator_type = Allocator;
    using reference = value_type&;
    using const_reference = const value_type&;
    using iterator = FlatMapIterator<typename Storage::iterator, Key, T>;
    using const_iterator = FlatMapConstIterator<typename Storage::const_iterator, Key, T>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    // =========================================================================
    // Constructors
    // =========================================================================

    FlatMap() = default;

    explicit FlatMap(const Compare& comp, const Allocator& alloc = Allocator())
        : data_(InternalAllocator(alloc))
        , comp_(comp)
    {
    }

    explicit FlatMap(const Allocator& alloc)
        : data_(InternalAllocator(alloc))
    {
    }

    template <class InputIt>
    FlatMap(InputIt first,
            InputIt last,
            const Compare& comp = Compare(),
            const Allocator& alloc = Allocator())
        : data_(InternalAllocator(alloc))
        , comp_(comp)
    {
        for (; first != last; ++first)
        {
            data_.emplace_back(first->first, first->second);
        }
        std::sort(data_.begin(), data_.end(), key_value_comp());
        auto last_unique = std::unique(data_.begin(),
                                       data_.end(),
                                       [this](const InternalPair& a, const InternalPair& b) {
                                           return keys_equivalent(a.first, b.first);
                                       });
        data_.erase(last_unique, data_.end());
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

    FlatMap(FlatMap&& other) noexcept(
        std::is_nothrow_move_constructible_v<Storage> &&
        std::is_nothrow_move_constructible_v<Compare>)
        : data_(std::move(other.data_))
        , comp_(std::move(other.comp_))
    {
    }

    FlatMap& operator=(FlatMap&& other) noexcept(
        std::is_nothrow_move_assignable_v<Storage> &&
        std::is_nothrow_move_assignable_v<Compare>)
    {
        data_ = std::move(other.data_);
        comp_ = std::move(other.comp_);
        return *this;
    }

    // =========================================================================
    // Iterators
    // =========================================================================

    iterator begin() noexcept
    {
        return iterator(data_.begin());
    }

    const_iterator begin() const noexcept
    {
        return const_iterator(data_.begin());
    }

    iterator end() noexcept
    {
        return iterator(data_.end());
    }

    const_iterator end() const noexcept
    {
        return const_iterator(data_.end());
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
        return const_iterator(data_.cbegin());
    }

    const_iterator cend() const noexcept
    {
        return const_iterator(data_.cend());
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
        return data_.empty();
    }

    [[nodiscard]] size_type size() const noexcept
    {
        return data_.size();
    }

    [[nodiscard]] size_type max_size() const noexcept
    {
        return data_.max_size();
    }

    [[nodiscard]] size_type capacity() const noexcept
    {
        return data_.capacity();
    }

    void reserve(size_type n)
    {
        data_.reserve(n);
    }

    void shrink_to_fit()
    {
        data_.shrink_to_fit();
    }

    // =========================================================================
    // Element access
    // =========================================================================

    [[nodiscard]] T& at(const Key& key)
    {
        auto it = lower_bound_internal(key);
        if (it == data_.end() || comp_(key, it->first))
        {
            throw std::out_of_range("FlatMap::at: key not found");
        }
        return it->second;
    }

    [[nodiscard]] const T& at(const Key& key) const
    {
        auto it = lower_bound_internal(key);
        if (it == data_.end() || comp_(key, it->first))
        {
            throw std::out_of_range("FlatMap::at: key not found");
        }
        return it->second;
    }

    T& operator[](const Key& key)
    {
        auto it = lower_bound_internal(key);
        if (it == data_.end() || comp_(key, it->first))
        {
            it = data_.insert(it, InternalPair(key, T()));
        }
        return it->second;
    }

    T& operator[](Key&& key)
    {
        auto it = lower_bound_internal(key);
        if (it == data_.end() || comp_(key, it->first))
        {
            it = data_.insert(it, InternalPair(std::move(key), T()));
        }
        return it->second;
    }

    // =========================================================================
    // Modifiers
    // =========================================================================

    void clear() noexcept
    {
        data_.clear();
    }

    std::pair<iterator, bool> insert(const value_type& value)
    {
        auto it = lower_bound_internal(value.first);
        if (it != data_.end() && !comp_(value.first, it->first))
        {
            return {iterator(it), false};
        }
        return {iterator(data_.insert(it, InternalPair(value.first, value.second))), true};
    }

    std::pair<iterator, bool> insert(value_type&& value)
    {
        auto it = lower_bound_internal(value.first);
        if (it != data_.end() && !comp_(value.first, it->first))
        {
            return {iterator(it), false};
        }
        auto inserted =
            data_.insert(it, InternalPair(std::move(value.first), std::move(value.second)));
        return {iterator(inserted), true};
    }

    iterator insert(const_iterator hint, const value_type& value)
    {
        (void)hint;
        return insert(value).first;
    }

    iterator insert(const_iterator hint, value_type&& value)
    {
        (void)hint;
        return insert(std::move(value)).first;
    }

    template <class InputIt>
    void insert(InputIt first, InputIt last)
    {
        if (first == last)
        {
            return;
        }

        size_type old_size = data_.size();

        for (; first != last; ++first)
        {
            data_.emplace_back(first->first, first->second);
        }

        auto mid = data_.begin() + static_cast<difference_type>(old_size);
        std::sort(mid, data_.end(), key_value_comp());
        std::inplace_merge(data_.begin(), mid, data_.end(), key_value_comp());

        auto last_unique = std::unique(data_.begin(),
                                       data_.end(),
                                       [this](const InternalPair& a, const InternalPair& b) {
                                           return keys_equivalent(a.first, b.first);
                                       });
        data_.erase(last_unique, data_.end());
    }

    void insert(std::initializer_list<value_type> ilist)
    {
        insert(ilist.begin(), ilist.end());
    }

    template <class M>
    std::pair<iterator, bool> insert_or_assign(const Key& k, M&& obj)
    {
        auto it = lower_bound_internal(k);
        if (it != data_.end() && !comp_(k, it->first))
        {
            it->second = std::forward<M>(obj);
            return {iterator(it), false};
        }
        return {iterator(data_.insert(it, InternalPair(k, std::forward<M>(obj)))), true};
    }

    template <class M>
    std::pair<iterator, bool> insert_or_assign(Key&& k, M&& obj)
    {
        auto it = lower_bound_internal(k);
        if (it != data_.end() && !comp_(k, it->first))
        {
            it->second = std::forward<M>(obj);
            return {iterator(it), false};
        }
        return {iterator(data_.insert(it, InternalPair(std::move(k), std::forward<M>(obj)))), true};
    }

    template <class... Args>
    std::pair<iterator, bool> emplace(Args&&... args)
    {
        InternalPair temp(std::forward<Args>(args)...);
        auto it = lower_bound_internal(temp.first);
        if (it != data_.end() && !comp_(temp.first, it->first))
        {
            return {iterator(it), false};
        }
        return {iterator(data_.insert(it, std::move(temp))), true};
    }

    template <class... Args>
    iterator emplace_hint(const_iterator hint, Args&&... args)
    {
        (void)hint;
        return emplace(std::forward<Args>(args)...).first;
    }

    template <class... Args>
    std::pair<iterator, bool> try_emplace(const Key& k, Args&&... args)
    {
        auto it = lower_bound_internal(k);
        if (it != data_.end() && !comp_(k, it->first))
        {
            return {iterator(it), false};
        }
        auto inserted = data_.insert(it,
                                     InternalPair(std::piecewise_construct,
                                                  std::forward_as_tuple(k),
                                                  std::forward_as_tuple(std::forward<Args>(args)...)));
        return {iterator(inserted), true};
    }

    template <class... Args>
    std::pair<iterator, bool> try_emplace(Key&& k, Args&&... args)
    {
        auto it = lower_bound_internal(k);
        if (it != data_.end() && !comp_(k, it->first))
        {
            return {iterator(it), false};
        }
        auto inserted = data_.insert(it,
                                     InternalPair(std::piecewise_construct,
                                                  std::forward_as_tuple(std::move(k)),
                                                  std::forward_as_tuple(std::forward<Args>(args)...)));
        return {iterator(inserted), true};
    }

    iterator erase(iterator pos)
    {
        enforce(pos.base() >= data_.begin() && pos.base() < data_.end(),
                "FlatMap::erase: invalid iterator");
        return iterator(data_.erase(pos.base()));
    }

    iterator erase(const_iterator pos)
    {
        enforce(pos.base() >= data_.cbegin() && pos.base() < data_.cend(),
                "FlatMap::erase: invalid iterator");
        return iterator(data_.erase(pos.base()));
    }

    iterator erase(const_iterator first, const_iterator last)
    {
        enforce(first.base() >= data_.cbegin() && first.base() <= data_.cend(),
                "FlatMap::erase: invalid first iterator");
        enforce(last.base() >= data_.cbegin() && last.base() <= data_.cend(),
                "FlatMap::erase: invalid last iterator");
        enforce(first.base() <= last.base(), "FlatMap::erase: invalid iterator range");
        return iterator(data_.erase(first.base(), last.base()));
    }

    size_type erase(const Key& key)
    {
        auto it = lower_bound_internal(key);
        if (it == data_.end() || comp_(key, it->first))
        {
            return 0;
        }
        data_.erase(it);
        return 1;
    }

    /// @brief Extracts an element from the container
    /// @param pos Iterator to the element to extract
    /// @return The extracted key-value pair (moved from container)
    /// @note The element is removed from the container after extraction
    value_type extract(const_iterator pos)
    {
        enforce(pos >= cbegin() && pos < cend(), "FlatMap::extract: invalid iterator");
        auto internal_it = data_.begin() + (pos.base() - data_.cbegin());
        value_type result(std::move(internal_it->first), std::move(internal_it->second));
        data_.erase(internal_it);
        return result;
    }

    void swap(FlatMap& other) noexcept(std::is_nothrow_swappable_v<Compare>)
    {
        data_.swap(other.data_);
        std::swap(comp_, other.comp_);
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
        if (source.empty()) return;
        if (empty())
        {
            swap(source);
            return;
        }
        
        // Both containers are sorted - use merge algorithm for O(n + m) complexity
        Storage merged;
        merged.reserve(size() + source.size());
        
        auto it1 = data_.begin();
        auto it2 = source.data_.begin();
        
        while (it1 != data_.end() && it2 != source.data_.end())
        {
            if (comp_(it1->first, it2->first))
            {
                merged.push_back(std::move(*it1));
                ++it1;
            }
            else if (comp_(it2->first, it1->first))
            {
                merged.push_back(std::move(*it2));
                ++it2;
            }
            else
            {
                // Keys are equivalent - keep ours, skip theirs
                merged.push_back(std::move(*it1));
                ++it1;
                ++it2;
            }
        }
        
        // Append remaining elements
        while (it1 != data_.end())
        {
            merged.push_back(std::move(*it1));
            ++it1;
        }
        while (it2 != source.data_.end())
        {
            merged.push_back(std::move(*it2));
            ++it2;
        }
        
        data_ = std::move(merged);
        source.data_.clear();
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
        auto it = lower_bound_internal(key);
        if (it != data_.end() && !comp_(key, it->first))
        {
            return iterator(it);
        }
        return end();
    }

    [[nodiscard]] const_iterator find(const Key& key) const
    {
        auto it = lower_bound_internal(key);
        if (it != data_.end() && !comp_(key, it->first))
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
        return iterator(lower_bound_internal(key));
    }

    const_iterator lower_bound(const Key& key) const
    {
        return const_iterator(lower_bound_internal(key));
    }

    iterator upper_bound(const Key& key)
    {
        return iterator(std::upper_bound(data_.begin(), data_.end(), key, key_value_comp()));
    }

    const_iterator upper_bound(const Key& key) const
    {
        return const_iterator(std::upper_bound(data_.begin(), data_.end(), key, key_value_comp()));
    }

    // =========================================================================
    // Heterogeneous Lookup (requires Compare with is_transparent)
    // =========================================================================
    // These overloads are only enabled when the comparator has is_transparent,
    // allowing lookups without constructing a Key object (e.g., find("literal")
    // on FlatMap<std::string, T, std::less<>> avoids std::string construction).

    template <typename K, typename C = Compare,
              typename = std::enable_if_t<has_is_transparent<C>::value>>
    [[nodiscard]] size_type count(const K& key) const
    {
        return find(key) != end() ? 1 : 0;
    }

    template <typename K, typename C = Compare,
              typename = std::enable_if_t<has_is_transparent<C>::value>>
    [[nodiscard]] iterator find(const K& key)
    {
        auto it = std::lower_bound(data_.begin(), data_.end(), key, key_value_comp());
        if (it != data_.end() && !comp_(key, it->first) && !comp_(it->first, key))
        {
            return iterator(it);
        }
        return end();
    }

    template <typename K, typename C = Compare,
              typename = std::enable_if_t<has_is_transparent<C>::value>>
    [[nodiscard]] const_iterator find(const K& key) const
    {
        auto it = std::lower_bound(data_.begin(), data_.end(), key, key_value_comp());
        if (it != data_.end() && !comp_(key, it->first) && !comp_(it->first, key))
        {
            return const_iterator(it);
        }
        return end();
    }

    template <typename K, typename C = Compare,
              typename = std::enable_if_t<has_is_transparent<C>::value>>
    [[nodiscard]] bool contains(const K& key) const
    {
        return find(key) != end();
    }

    template <typename K, typename C = Compare,
              typename = std::enable_if_t<has_is_transparent<C>::value>>
    std::pair<iterator, iterator> equal_range(const K& key)
    {
        return {lower_bound(key), upper_bound(key)};
    }

    template <typename K, typename C = Compare,
              typename = std::enable_if_t<has_is_transparent<C>::value>>
    std::pair<const_iterator, const_iterator> equal_range(const K& key) const
    {
        return {lower_bound(key), upper_bound(key)};
    }

    template <typename K, typename C = Compare,
              typename = std::enable_if_t<has_is_transparent<C>::value>>
    iterator lower_bound(const K& key)
    {
        return iterator(std::lower_bound(data_.begin(), data_.end(), key, key_value_comp()));
    }

    template <typename K, typename C = Compare,
              typename = std::enable_if_t<has_is_transparent<C>::value>>
    const_iterator lower_bound(const K& key) const
    {
        return const_iterator(std::lower_bound(data_.begin(), data_.end(), key, key_value_comp()));
    }

    template <typename K, typename C = Compare,
              typename = std::enable_if_t<has_is_transparent<C>::value>>
    iterator upper_bound(const K& key)
    {
        return iterator(std::upper_bound(data_.begin(), data_.end(), key, key_value_comp()));
    }

    template <typename K, typename C = Compare,
              typename = std::enable_if_t<has_is_transparent<C>::value>>
    const_iterator upper_bound(const K& key) const
    {
        return const_iterator(std::upper_bound(data_.begin(), data_.end(), key, key_value_comp()));
    }

    // =========================================================================
    // Observers
    // =========================================================================

    key_compare key_comp() const
    {
        return comp_;
    }

    struct value_compare
    {
        Compare comp;

        bool operator()(const value_type& lhs, const value_type& rhs) const
        {
            return comp(lhs.first, rhs.first);
        }
    };

    value_compare value_comp() const
    {
        return value_compare{comp_};
    }

    allocator_type get_allocator() const noexcept
    {
        return allocator_type(data_.get_allocator());
    }

private:
    typename Storage::iterator lower_bound_internal(const Key& key)
    {
        return std::lower_bound(data_.begin(), data_.end(), key, key_value_comp());
    }

    typename Storage::const_iterator lower_bound_internal(const Key& key) const
    {
        return std::lower_bound(data_.begin(), data_.end(), key, key_value_comp());
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
void swap(FlatMap<K, V, C, A>& lhs,
          FlatMap<K, V, C, A>& rhs) noexcept(noexcept(lhs.swap(rhs)))
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
