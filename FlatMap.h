// FlatMap.h
#pragma once

#include <algorithm>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <stdexcept>
#include <utility>
#include <vector>

namespace cpp_utilities {

template <typename Key, typename T, typename Compare = std::less<Key>, 
          typename Allocator = std::allocator<std::pair<const Key, T>>>
class FlatMap {
private:
    // Internal storage uses non-const Key for assignability
    using InternalPair = std::pair<Key, T>;
    using InternalAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<InternalPair>;
    using Storage = std::vector<InternalPair, InternalAllocator>;
    using Pair = std::pair<const Key, T>;  // Exposed to user
    
    Storage data_;
    Compare comp_;
    
    struct KeyCompare {
        Compare comp;
        bool operator()(const InternalPair& a, const InternalPair& b) const {
            return comp(a.first, b.first);
        }
        bool operator()(const InternalPair& a, const Key& b) const {
            return comp(a.first, b);
        }
        bool operator()(const Key& a, const InternalPair& b) const {
            return comp(a, b.first);
        }
    };
    
    KeyCompare key_value_comp() const { return KeyCompare{comp_}; }

public:
    using key_type = Key;
    using mapped_type = T;
    using value_type = Pair;
    using size_type = typename Storage::size_type;
    using difference_type = typename Storage::difference_type;
    using key_compare = Compare;
    using allocator_type = Allocator;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = typename Storage::pointer;
    using const_pointer = typename Storage::const_pointer;
    using iterator = typename Storage::iterator;
    using const_iterator = typename Storage::const_iterator;
    using reverse_iterator = typename Storage::reverse_iterator;
    using const_reverse_iterator = typename Storage::const_reverse_iterator;

    // Constructors
    FlatMap() = default;
    
    explicit FlatMap(const Compare& comp, const Allocator& alloc = Allocator())
        : data_(alloc), comp_(comp) {}
    
    explicit FlatMap(const Allocator& alloc) : data_(alloc) {}
    
    template <class InputIt>
    FlatMap(InputIt first, InputIt last, const Compare& comp = Compare(), 
            const Allocator& alloc = Allocator())
        : data_(alloc), comp_(comp) {
        for (; first != last; ++first) {
            data_.emplace_back(first->first, first->second);
        }
        std::sort(data_.begin(), data_.end(), key_value_comp());
        data_.erase(std::unique(data_.begin(), data_.end(),
            [](const InternalPair& a, const InternalPair& b) { return a.first == b.first; }), data_.end());
    }
    
    FlatMap(std::initializer_list<value_type> init, const Compare& comp = Compare(), 
            const Allocator& alloc = Allocator())
        : FlatMap(init.begin(), init.end(), comp, alloc) {}

    // Iterators
    iterator begin() noexcept { return data_.begin(); }
    const_iterator begin() const noexcept { return data_.begin(); }
    iterator end() noexcept { return data_.end(); }
    const_iterator end() const noexcept { return data_.end(); }
    reverse_iterator rbegin() noexcept { return data_.rbegin(); }
    const_reverse_iterator rbegin() const noexcept { return data_.rbegin(); }
    reverse_iterator rend() noexcept { return data_.rend(); }
    const_reverse_iterator rend() const noexcept { return data_.rend(); }
    const_iterator cbegin() const noexcept { return data_.cbegin(); }
    const_iterator cend() const noexcept { return data_.cend(); }
    const_reverse_iterator crbegin() const noexcept { return data_.crbegin(); }
    const_reverse_iterator crend() const noexcept { return data_.crend(); }

    // Capacity
    bool empty() const noexcept { return data_.empty(); }
    size_type size() const noexcept { return data_.size(); }
    size_type max_size() const noexcept { return data_.max_size(); }

    // Element access
    T& at(const Key& key) {
        auto it = find(key);
        if (it == end()) {
            throw std::out_of_range("FlatMap::at");
        }
        return it->second;
    }

    const T& at(const Key& key) const {
        auto it = find(key);
        if (it == end()) {
            throw std::out_of_range("FlatMap::at");
        }
        return it->second;
    }

    T& operator[](const Key& key) {
        auto it = lower_bound(key);
        if (it == end() || comp_(key, it->first)) {
            it = data_.insert(it, InternalPair(key, T()));
        }
        return it->second;
    }

    T& operator[](Key&& key) {
        auto it = lower_bound(key);
        if (it == end() || comp_(key, it->first)) {
            it = data_.insert(it, InternalPair(std::move(key), T()));
        }
        return it->second;
    }

    // Modifiers
    void clear() noexcept { data_.clear(); }

    std::pair<iterator, bool> insert(const value_type& value) {
        auto it = lower_bound(value.first);
        if (it != end() && !comp_(value.first, it->first)) {
            return {it, false};
        }
        return {data_.insert(it, InternalPair(value.first, value.second)), true};
    }

    std::pair<iterator, bool> insert(value_type&& value) {
        auto it = lower_bound(value.first);
        if (it != end() && !comp_(value.first, it->first)) {
            return {it, false};
        }
        return {data_.insert(it, InternalPair(std::move(value.first), std::move(value.second))), true};
    }

    iterator insert(const_iterator hint, const value_type& value) {
        (void)hint;
        return insert(value).first;
    }

    iterator insert(const_iterator hint, value_type&& value) {
        (void)hint;
        return insert(std::move(value)).first;
    }

    template <class InputIt>
    void insert(InputIt first, InputIt last) {
        for (; first != last; ++first) {
            insert(*first);
        }
    }

    void insert(std::initializer_list<value_type> ilist) {
        insert(ilist.begin(), ilist.end());
    }

    template <class M>
    std::pair<iterator, bool> insert_or_assign(const Key& k, M&& obj) {
        auto it = lower_bound(k);
        if (it != end() && !comp_(k, it->first)) {
            it->second = std::forward<M>(obj);
            return {it, false};
        }
        return {data_.insert(it, InternalPair(k, std::forward<M>(obj))), true};
    }

    template <class M>
    std::pair<iterator, bool> insert_or_assign(Key&& k, M&& obj) {
        auto it = lower_bound(k);
        if (it != end() && !comp_(k, it->first)) {
            it->second = std::forward<M>(obj);
            return {it, false};
        }
        return {data_.insert(it, InternalPair(std::move(k), std::forward<M>(obj))), true};
    }

    template <class... Args>
    std::pair<iterator, bool> emplace(Args&&... args) {
        InternalPair temp(std::forward<Args>(args)...);
        return insert(std::move(temp));
    }

    template <class... Args>
    std::pair<iterator, bool> try_emplace(const Key& k, Args&&... args) {
        auto it = lower_bound(k);
        if (it != end() && !comp_(k, it->first)) {
            return {it, false};
        }
        return {data_.insert(it, InternalPair(std::piecewise_construct,
                                               std::forward_as_tuple(k),
                                               std::forward_as_tuple(std::forward<Args>(args)...))), true};
    }

    template <class... Args>
    std::pair<iterator, bool> try_emplace(Key&& k, Args&&... args) {
        auto it = lower_bound(k);
        if (it != end() && !comp_(k, it->first)) {
            return {it, false};
        }
        return {data_.insert(it, InternalPair(std::piecewise_construct,
                                               std::forward_as_tuple(std::move(k)),
                                               std::forward_as_tuple(std::forward<Args>(args)...))), true};
    }

    iterator erase(iterator pos) { return data_.erase(pos); }
    iterator erase(const_iterator pos) { return data_.erase(pos); }
    iterator erase(const_iterator first, const_iterator last) { return data_.erase(first, last); }
    
    size_type erase(const Key& key) {
        auto it = find(key);
        if (it == end()) return 0;
        data_.erase(it);
        return 1;
    }

    void swap(FlatMap& other) noexcept {
        data_.swap(other.data_);
        std::swap(comp_, other.comp_);
    }

    // Lookup
    size_type count(const Key& key) const { return find(key) != end() ? 1 : 0; }
    
    iterator find(const Key& key) {
        auto it = lower_bound(key);
        return (it != end() && !comp_(key, it->first)) ? it : end();
    }

    const_iterator find(const Key& key) const {
        auto it = lower_bound(key);
        return (it != end() && !comp_(key, it->first)) ? it : end();
    }

    bool contains(const Key& key) const { return find(key) != end(); }

    std::pair<iterator, iterator> equal_range(const Key& key) {
        return {lower_bound(key), upper_bound(key)};
    }

    std::pair<const_iterator, const_iterator> equal_range(const Key& key) const {
        return {lower_bound(key), upper_bound(key)};
    }

    iterator lower_bound(const Key& key) {
        return std::lower_bound(data_.begin(), data_.end(), key, key_value_comp());
    }

    const_iterator lower_bound(const Key& key) const {
        return std::lower_bound(data_.begin(), data_.end(), key, key_value_comp());
    }

    iterator upper_bound(const Key& key) {
        return std::upper_bound(data_.begin(), data_.end(), key, key_value_comp());
    }

    const_iterator upper_bound(const Key& key) const {
        return std::upper_bound(data_.begin(), data_.end(), key, key_value_comp());
    }

    // Observers
    key_compare key_comp() const { return comp_; }
    
    struct value_compare {
        Compare comp;
        bool operator()(const value_type& lhs, const value_type& rhs) const {
            return comp(lhs.first, rhs.first);
        }
    };
    
    value_compare value_comp() const { return value_compare{comp_}; }
    allocator_type get_allocator() const noexcept { return data_.get_allocator(); }
};

}  // namespace cpp_utilities
