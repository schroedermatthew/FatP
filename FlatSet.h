// FlatSet.h
#pragma once

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

template <typename T, typename Compare = std::less<T>, typename Allocator = std::allocator<T>>
class FlatSet
{
private:
    using Storage = std::vector<T, Allocator>;
    using internal_iterator = typename Storage::iterator;

    Storage data_;
    Compare comp_;

    // Helper to detect if Compare has is_transparent
    template <typename C, typename = void>
    struct has_is_transparent : std::false_type {};
    
    template <typename C>
    struct has_is_transparent<C, std::void_t<typename C::is_transparent>> : std::true_type {};

    bool elements_equivalent(const T& a, const T& b) const
    {
        return !comp_(a, b) && !comp_(b, a);
    }
    
    // Heterogeneous equivalence check
    template <typename K>
    bool elements_equivalent_hetero(const T& a, const K& b) const
    {
        return !comp_(a, b) && !comp_(b, a);
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
    internal_iterator to_internal(const_iterator it)
    {
        return data_.begin() + (it - data_.cbegin());
    }

public:

    // =========================================================================
    // Constructors
    // =========================================================================

    FlatSet() = default;

    explicit FlatSet(const Compare& comp, const Allocator& alloc = Allocator())
        : data_(alloc)
        , comp_(comp)
    {
    }

    explicit FlatSet(const Allocator& alloc)
        : data_(alloc)
    {
    }

    template <class InputIt>
    FlatSet(InputIt first,
            InputIt last,
            const Compare& comp = Compare(),
            const Allocator& alloc = Allocator())
        : data_(first, last, alloc)
        , comp_(comp)
    {
        std::sort(data_.begin(), data_.end(), comp_);
        auto last_unique =
            std::unique(data_.begin(), data_.end(), [this](const T& a, const T& b) {
                return elements_equivalent(a, b);
            });
        data_.erase(last_unique, data_.end());
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
        : data_(std::move(other.data_))
        , comp_(std::move(other.comp_))
    {
    }

    FlatSet& operator=(FlatSet&& other) noexcept(
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
        return data_.begin();
    }

    const_iterator begin() const noexcept
    {
        return data_.begin();
    }

    iterator end() noexcept
    {
        return data_.end();
    }

    const_iterator end() const noexcept
    {
        return data_.end();
    }

    reverse_iterator rbegin() noexcept
    {
        return data_.rbegin();
    }

    const_reverse_iterator rbegin() const noexcept
    {
        return data_.rbegin();
    }

    reverse_iterator rend() noexcept
    {
        return data_.rend();
    }

    const_reverse_iterator rend() const noexcept
    {
        return data_.rend();
    }

    const_iterator cbegin() const noexcept
    {
        return data_.cbegin();
    }

    const_iterator cend() const noexcept
    {
        return data_.cend();
    }

    const_reverse_iterator crbegin() const noexcept
    {
        return data_.crbegin();
    }

    const_reverse_iterator crend() const noexcept
    {
        return data_.crend();
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
    // Modifiers
    // =========================================================================

    void clear() noexcept
    {
        data_.clear();
    }

    std::pair<iterator, bool> insert(const value_type& value)
    {
        auto it = std::lower_bound(data_.begin(), data_.end(), value, comp_);
        if (it != data_.end() && !comp_(value, *it))
        {
            return {it, false};
        }
        return {data_.insert(it, value), true};
    }

    std::pair<iterator, bool> insert(value_type&& value)
    {
        auto it = std::lower_bound(data_.begin(), data_.end(), value, comp_);
        if (it != data_.end() && !comp_(value, *it))
        {
            return {it, false};
        }
        return {data_.insert(it, std::move(value)), true};
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
            data_.push_back(*first);
        }

        auto mid = data_.begin() + static_cast<difference_type>(old_size);
        std::sort(mid, data_.end(), comp_);
        std::inplace_merge(data_.begin(), mid, data_.end(), comp_);

        auto last_unique =
            std::unique(data_.begin(), data_.end(), [this](const T& a, const T& b) {
                return elements_equivalent(a, b);
            });
        data_.erase(last_unique, data_.end());
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
        (void)hint;
        return emplace(std::forward<Args>(args)...).first;
    }

    iterator erase(const_iterator pos)
    {
        enforce(pos >= data_.cbegin() && pos < data_.cend(), "FlatSet::erase: invalid iterator");
        return data_.erase(to_internal(pos));
    }

    iterator erase(const_iterator first, const_iterator last)
    {
        enforce(first >= data_.cbegin() && first <= data_.cend(),
                "FlatSet::erase: invalid first iterator");
        enforce(last >= data_.cbegin() && last <= data_.cend(),
                "FlatSet::erase: invalid last iterator");
        enforce(first <= last, "FlatSet::erase: invalid iterator range");
        return data_.erase(to_internal(first), to_internal(last));
    }

    size_type erase(const key_type& key)
    {
        auto it = std::lower_bound(data_.begin(), data_.end(), key, comp_);
        if (it == data_.end() || comp_(key, *it))
        {
            return 0;
        }
        data_.erase(it);
        return 1;
    }

    /// @brief Extracts an element from the container
    /// @param pos Iterator to the element to extract
    /// @return The extracted value (moved from container)
    /// @note The element is removed from the container after extraction
    value_type extract(const_iterator pos)
    {
        enforce(pos >= cbegin() && pos < cend(), "FlatSet::extract: invalid iterator");
        auto internal_it = to_internal(pos);
        value_type result(std::move(*internal_it));
        data_.erase(internal_it);
        return result;
    }

    void swap(FlatSet& other) noexcept(std::is_nothrow_swappable_v<Compare>)
    {
        data_.swap(other.data_);
        std::swap(comp_, other.comp_);
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
            if (comp_(*it1, *it2))
            {
                merged.push_back(std::move(*it1));
                ++it1;
            }
            else if (comp_(*it2, *it1))
            {
                merged.push_back(std::move(*it2));
                ++it2;
            }
            else
            {
                // Elements are equivalent - keep ours, skip theirs
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
        return (it != end() && !comp_(key, *it)) ? it : end();
    }

    [[nodiscard]] const_iterator find(const key_type& key) const
    {
        auto it = lower_bound(key);
        return (it != end() && !comp_(key, *it)) ? it : end();
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
        return std::lower_bound(data_.begin(), data_.end(), key, comp_);
    }

    const_iterator lower_bound(const key_type& key) const
    {
        return std::lower_bound(data_.begin(), data_.end(), key, comp_);
    }

    iterator upper_bound(const key_type& key)
    {
        return std::upper_bound(data_.begin(), data_.end(), key, comp_);
    }

    const_iterator upper_bound(const key_type& key) const
    {
        return std::upper_bound(data_.begin(), data_.end(), key, comp_);
    }

    // =========================================================================
    // Heterogeneous Lookup (requires Compare with is_transparent)
    // =========================================================================
    // These overloads are only enabled when the comparator has is_transparent,
    // allowing lookups without constructing a key_type object.

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
        auto it = lower_bound(key);
        return (it != end() && elements_equivalent_hetero(*it, key)) ? it : end();
    }

    template <typename K, typename C = Compare,
              typename = std::enable_if_t<has_is_transparent<C>::value>>
    [[nodiscard]] const_iterator find(const K& key) const
    {
        auto it = lower_bound(key);
        return (it != end() && elements_equivalent_hetero(*it, key)) ? it : end();
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
        return std::lower_bound(data_.begin(), data_.end(), key, comp_);
    }

    template <typename K, typename C = Compare,
              typename = std::enable_if_t<has_is_transparent<C>::value>>
    const_iterator lower_bound(const K& key) const
    {
        return std::lower_bound(data_.begin(), data_.end(), key, comp_);
    }

    template <typename K, typename C = Compare,
              typename = std::enable_if_t<has_is_transparent<C>::value>>
    iterator upper_bound(const K& key)
    {
        return std::upper_bound(data_.begin(), data_.end(), key, comp_);
    }

    template <typename K, typename C = Compare,
              typename = std::enable_if_t<has_is_transparent<C>::value>>
    const_iterator upper_bound(const K& key) const
    {
        return std::upper_bound(data_.begin(), data_.end(), key, comp_);
    }

    // =========================================================================
    // Observers
    // =========================================================================

    key_compare key_comp() const
    {
        return comp_;
    }

    value_compare value_comp() const
    {
        return comp_;
    }

    allocator_type get_allocator() const noexcept
    {
        return data_.get_allocator();
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
