// FlatSet.h
#ifndef CPP_UTILITIES_FLAT_SET_H
#define CPP_UTILITIES_FLAT_SET_H

#include <algorithm>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <utility>
#include <vector>

namespace cpp_utilities {

template <typename T, typename Compare = std::less<T>, typename Allocator = std::allocator<T>>
class FlatSet {
private:
    using Storage = std::vector<T, Allocator>;
    
    Storage data_;
    Compare comp_;

public:
    using key_type = T;
    using value_type = T;
    using size_type = typename Storage::size_type;
    using difference_type = typename Storage::difference_type;
    using key_compare = Compare;
    using value_compare = Compare;
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
    FlatSet() = default;

    explicit FlatSet(const Compare& comp, const Allocator& alloc = Allocator())
        : data_(alloc), comp_(comp) {}

    explicit FlatSet(const Allocator& alloc) : data_(alloc) {}

    template <class InputIt>
    FlatSet(InputIt first, InputIt last, const Compare& comp = Compare(), 
            const Allocator& alloc = Allocator())
        : data_(first, last, alloc), comp_(comp) {
        std::sort(data_.begin(), data_.end(), comp_);
        data_.erase(std::unique(data_.begin(), data_.end()), data_.end());
    }

    FlatSet(std::initializer_list<value_type> init, const Compare& comp = Compare(), 
            const Allocator& alloc = Allocator())
        : FlatSet(init.begin(), init.end(), comp, alloc) {}

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

    // Modifiers
    void clear() noexcept { data_.clear(); }

    std::pair<iterator, bool> insert(const value_type& value) {
        auto it = lower_bound(value);
        if (it != end() && !comp_(value, *it)) {
            return {it, false};
        }
        return {data_.insert(it, value), true};
    }

    std::pair<iterator, bool> insert(value_type&& value) {
        auto it = lower_bound(value);
        if (it != end() && !comp_(value, *it)) {
            return {it, false};
        }
        return {data_.insert(it, std::move(value)), true};
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

    template <class... Args>
    std::pair<iterator, bool> emplace(Args&&... args) {
        return insert(value_type(std::forward<Args>(args)...));
    }

    template <class... Args>
    iterator emplace_hint(const_iterator hint, Args&&... args) {
        (void)hint;
        return emplace(std::forward<Args>(args)...).first;
    }

    iterator erase(iterator pos) { return data_.erase(pos); }
    iterator erase(const_iterator pos) { return data_.erase(pos); }
    iterator erase(const_iterator first, const_iterator last) { return data_.erase(first, last); }
    
    size_type erase(const key_type& key) {
        auto it = find(key);
        if (it == end()) return 0;
        data_.erase(it);
        return 1;
    }

    void swap(FlatSet& other) noexcept {
        data_.swap(other.data_);
        std::swap(comp_, other.comp_);
    }

    // Lookup
    size_type count(const key_type& key) const { return find(key) != end() ? 1 : 0; }
    
    iterator find(const key_type& key) {
        auto it = lower_bound(key);
        return (it != end() && !comp_(key, *it)) ? it : end();
    }
    
    const_iterator find(const key_type& key) const {
        auto it = lower_bound(key);
        return (it != end() && !comp_(key, *it)) ? it : end();
    }
    
    bool contains(const key_type& key) const { return find(key) != end(); }

    std::pair<iterator, iterator> equal_range(const key_type& key) {
        return {lower_bound(key), upper_bound(key)};
    }

    std::pair<const_iterator, const_iterator> equal_range(const key_type& key) const {
        return {lower_bound(key), upper_bound(key)};
    }

    iterator lower_bound(const key_type& key) {
        return std::lower_bound(data_.begin(), data_.end(), key, comp_);
    }

    const_iterator lower_bound(const key_type& key) const {
        return std::lower_bound(data_.begin(), data_.end(), key, comp_);
    }

    iterator upper_bound(const key_type& key) {
        return std::upper_bound(data_.begin(), data_.end(), key, comp_);
    }

    const_iterator upper_bound(const key_type& key) const {
        return std::upper_bound(data_.begin(), data_.end(), key, comp_);
    }

    // Observers
    key_compare key_comp() const { return comp_; }
    value_compare value_comp() const { return comp_; }
    allocator_type get_allocator() const noexcept { return data_.get_allocator(); }
};

}  // namespace cpp_utilities

#endif  // CPP_UTILITIES_FLAT_SET_H
