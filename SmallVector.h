// SmallVector.h - FIXED: Move constructor properly clears source state
#ifndef CPP_UTILITIES_SMALL_VECTOR_H
#define CPP_UTILITIES_SMALL_VECTOR_H

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>
#include <stdexcept>

namespace cpp_utilities {

/**
 * @brief Small vector optimization: stores elements inline for small sizes, heap for large
 * @tparam T Element type
 * @tparam InlineCapacity Number of elements to store inline before heap allocation
 * @tparam Allocator Allocator type (default: std::allocator<T>)
 * 
 * @details Performance characteristics:
 * - Small sizes (≤ InlineCapacity): No heap allocation, cache-friendly
 * - Large sizes: Automatic promotion to heap storage
 * - Move operations: O(1) for heap, O(N) for inline
 * 
 * @note This implementation has been FIXED to properly handle move semantics:
 *       - Move constructor now correctly clears source size
 *       - Move assignment properly resets source state
 */
template <typename T, size_t InlineCapacity = 16, typename Allocator = std::allocator<T>>
class SmallVector {
private:
    using AllocTraits = std::allocator_traits<Allocator>;

    union {
        alignas(T) std::byte inline_storage[InlineCapacity * sizeof(T)];
        struct {
            T* data_;
            size_t capacity_;
            Allocator allocator_;
        } heap_;
    };
    size_t size_ = 0;
    bool is_inline_ = true;

    void switch_to_heap(size_t new_capacity) {
        Allocator alloc = get_allocator();
        T* new_data = AllocTraits::allocate(alloc, new_capacity);
        try {
            std::uninitialized_move(begin(), end(), new_data);
        } catch (...) {
            AllocTraits::deallocate(alloc, new_data, new_capacity);
            throw;
        }
        destroy_inline();
        heap_.data_ = new_data;
        heap_.capacity_ = new_capacity;
        heap_.allocator_ = std::move(alloc);
        is_inline_ = false;
    }

    void destroy_inline() {
        if (is_inline_) {
            std::destroy_n(reinterpret_cast<T*>(inline_storage), size_);
        }
    }

    // Helper: uninitialized_move_backward (not in standard library)
    template<typename InputIt, typename ForwardIt>
    static ForwardIt uninitialized_move_backward(InputIt first, InputIt last, ForwardIt result) {
        using value_type = typename std::iterator_traits<ForwardIt>::value_type;
        ForwardIt current = result;
        try {
            while (last != first) {
                --last;
                --current;
                ::new (static_cast<void*>(std::addressof(*current))) value_type(std::move(*last));
            }
            return current;
        } catch (...) {
            while (current != result) {
                current->~value_type();
                ++current;
            }
            throw;
        }
    }

public:
    using value_type = T;
    using allocator_type = Allocator;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = typename AllocTraits::pointer;
    using const_pointer = typename AllocTraits::const_pointer;
    using iterator = pointer;
    using const_iterator = const_pointer;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    SmallVector() noexcept : size_(0), is_inline_(true) {}

    explicit SmallVector(const Allocator& alloc) noexcept
        : heap_{nullptr, 0, alloc}, is_inline_(true), size_(0) {}

    explicit SmallVector(size_type count, const T& value, const Allocator& alloc = Allocator())
        : SmallVector(alloc) {
        assign(count, value);
    }

    explicit SmallVector(size_type count, const Allocator& alloc = Allocator())
        : SmallVector(alloc) {
        resize(count);
    }

    template <class InputIt>
    SmallVector(InputIt first, InputIt last, const Allocator& alloc = Allocator())
        : SmallVector(alloc) {
        assign(first, last);
    }

    SmallVector(std::initializer_list<T> ilist, const Allocator& alloc = Allocator())
        : SmallVector(ilist.begin(), ilist.end(), alloc) {}

    SmallVector(const SmallVector& other)
        : SmallVector(other.begin(), other.end(), 
                      AllocTraits::select_on_container_copy_construction(other.get_allocator())) {}

    SmallVector(const SmallVector& other, const Allocator& alloc)
        : SmallVector(other.begin(), other.end(), alloc) {}

    // FIXED: Move constructor now properly clears source state
    SmallVector(SmallVector&& other) noexcept
        : size_(other.size_), is_inline_(other.is_inline_) {
        if (other.is_inline_) {
            // Move inline elements
            std::uninitialized_move(other.begin(), other.end(), begin());
            // Clear source inline elements
            std::destroy_n(other.begin(), other.size_);
        } else {
            // Steal heap allocation
            heap_ = other.heap_;
            // Reset other to empty inline state
            other.heap_.data_ = nullptr;
            other.heap_.capacity_ = 0;
        }
        // CRITICAL FIX: Always reset source to empty state
        other.is_inline_ = true;
        other.size_ = 0;
    }

    SmallVector(SmallVector&& other, const Allocator& alloc)
        : SmallVector(alloc) {
        if (get_allocator() == other.get_allocator()) {
            *this = std::move(other);
        } else {
            assign(std::make_move_iterator(other.begin()), 
                   std::make_move_iterator(other.end()));
        }
    }

    ~SmallVector() {
        clear();
        if (!is_inline_) {
            AllocTraits::deallocate(heap_.allocator_, heap_.data_, heap_.capacity_);
        }
    }

    SmallVector& operator=(const SmallVector& other) {
        if (this != &other) {
            assign(other.begin(), other.end());
        }
        return *this;
    }

    // FIXED: Move assignment now properly clears source state
    SmallVector& operator=(SmallVector&& other) noexcept {
        if (this != &other) {
            // Clean up current state
            clear();
            if (!is_inline_) {
                AllocTraits::deallocate(heap_.allocator_, heap_.data_, heap_.capacity_);
            }
            
            // Move from other
            size_ = other.size_;
            is_inline_ = other.is_inline_;
            
            if (other.is_inline_) {
                // Move inline elements
                std::uninitialized_move(other.begin(), other.end(), begin());
                // Destroy source elements
                std::destroy_n(other.begin(), other.size_);
            } else {
                // Steal heap allocation
                heap_ = other.heap_;
                other.heap_.data_ = nullptr;
                other.heap_.capacity_ = 0;
            }
            
            // CRITICAL FIX: Always reset source to empty state
            other.is_inline_ = true;
            other.size_ = 0;
        }
        return *this;
    }

    SmallVector& operator=(std::initializer_list<T> ilist) {
        assign(ilist);
        return *this;
    }

    allocator_type get_allocator() const noexcept {
        return is_inline_ ? allocator_type() : heap_.allocator_;
    }

    // Iterators
    iterator begin() noexcept {
        return is_inline_ ? reinterpret_cast<T*>(inline_storage) : heap_.data_;
    }

    const_iterator begin() const noexcept {
        return is_inline_ ? reinterpret_cast<const T*>(inline_storage) : heap_.data_;
    }

    iterator end() noexcept {
        return begin() + size_;
    }

    const_iterator end() const noexcept {
        return begin() + size_;
    }

    reverse_iterator rbegin() noexcept {
        return reverse_iterator(end());
    }

    const_reverse_iterator rbegin() const noexcept {
        return const_reverse_iterator(end());
    }

    reverse_iterator rend() noexcept {
        return reverse_iterator(begin());
    }

    const_reverse_iterator rend() const noexcept {
        return const_reverse_iterator(begin());
    }

    const_iterator cbegin() const noexcept {
        return begin();
    }

    const_iterator cend() const noexcept {
        return end();
    }

    const_reverse_iterator crbegin() const noexcept {
        return rbegin();
    }

    const_reverse_iterator crend() const noexcept {
        return rend();
    }

    // Capacity
    bool empty() const noexcept {
        return size_ == 0;
    }

    size_type size() const noexcept {
        return size_;
    }

    size_type max_size() const noexcept {
        return AllocTraits::max_size(get_allocator());
    }

    void reserve(size_type new_capacity) {
        if (new_capacity > capacity()) {
            if (is_inline_) {
                switch_to_heap(new_capacity);
            } else {
                Allocator alloc = heap_.allocator_;
                T* new_data = AllocTraits::allocate(alloc, new_capacity);
                try {
                    std::uninitialized_move(begin(), end(), new_data);
                } catch (...) {
                    AllocTraits::deallocate(alloc, new_data, new_capacity);
                    throw;
                }
                std::destroy_n(heap_.data_, size_);
                AllocTraits::deallocate(alloc, heap_.data_, heap_.capacity_);
                heap_.data_ = new_data;
                heap_.capacity_ = new_capacity;
            }
        }
    }

    size_type capacity() const noexcept {
        return is_inline_ ? InlineCapacity : heap_.capacity_;
    }

    void shrink_to_fit() {
        if (!is_inline_ && size_ <= InlineCapacity) {
            // Move back to inline storage
            T temp_storage alignas(T)[InlineCapacity];
            std::uninitialized_move(begin(), end(), reinterpret_cast<T*>(temp_storage));
            
            Allocator alloc = heap_.allocator_;
            T* old_data = heap_.data_;
            size_t old_cap = heap_.capacity_;
            
            std::destroy_n(old_data, size_);
            AllocTraits::deallocate(alloc, old_data, old_cap);
            
            is_inline_ = true;
            std::uninitialized_move(reinterpret_cast<T*>(temp_storage), 
                                   reinterpret_cast<T*>(temp_storage) + size_, 
                                   begin());
            std::destroy_n(reinterpret_cast<T*>(temp_storage), size_);
        }
    }

    // Modifiers
    void clear() noexcept {
        std::destroy_n(begin(), size_);
        size_ = 0;
    }

    iterator insert(const_iterator pos, const T& value) {
        return emplace(pos, value);
    }

    iterator insert(const_iterator pos, T&& value) {
        return emplace(pos, std::move(value));
    }

    iterator insert(const_iterator pos, size_type count, const T& value) {
        if (count == 0) return const_cast<iterator>(pos);
        size_t idx = pos - begin();
        if (size_ + count > capacity()) {
            reserve(size_ + count);
        }
        iterator insert_pos = begin() + idx;
        uninitialized_move_backward(insert_pos, end(), end() + count);
        std::uninitialized_fill_n(insert_pos, count, value);
        size_ += count;
        return insert_pos;
    }

    template <class InputIt>
    iterator insert(const_iterator pos, InputIt first, InputIt last) {
        if (first == last) return const_cast<iterator>(pos);
        size_t count = std::distance(first, last);
        size_t idx = pos - begin();
        if (size_ + count > capacity()) {
            reserve(size_ + count);
        }
        iterator insert_pos = begin() + idx;
        uninitialized_move_backward(insert_pos, end(), end() + count);
        std::uninitialized_copy(first, last, insert_pos);
        size_ += count;
        return insert_pos;
    }

    iterator insert(const_iterator pos, std::initializer_list<T> ilist) {
        return insert(pos, ilist.begin(), ilist.end());
    }

    template <class... Args>
    iterator emplace(const_iterator pos, Args&&... args) {
        size_t idx = pos - begin();
        if (size_ >= capacity()) {
            reserve(capacity() == 0 ? 1 : capacity() * 2);
        }
        iterator insert_pos = begin() + idx;
        uninitialized_move_backward(insert_pos, end(), end() + 1);
        Allocator alloc = get_allocator();
        AllocTraits::construct(alloc, insert_pos, std::forward<Args>(args)...);
        ++size_;
        return insert_pos;
    }

    iterator erase(const_iterator pos) {
        iterator it = const_cast<iterator>(pos);
        std::move(it + 1, end(), it);
        --size_;
        Allocator alloc = get_allocator();
        AllocTraits::destroy(alloc, end());
        return it;
    }

    iterator erase(const_iterator first, const_iterator last) {
        if (first == last) return const_cast<iterator>(first);
        iterator f = const_cast<iterator>(first);
        iterator l = const_cast<iterator>(last);
        std::move(l, end(), f);
        size_t count = last - first;
        std::destroy_n(end() - count, count);
        size_ -= count;
        return f;
    }

    void push_back(const T& value) {
        emplace_back(value);
    }

    void push_back(T&& value) {
        emplace_back(std::move(value));
    }

    template <class... Args>
    reference emplace_back(Args&&... args) {
        if (size_ >= capacity()) {
            reserve(capacity() == 0 ? 1 : capacity() * 2);
        }
        Allocator alloc = get_allocator();
        AllocTraits::construct(alloc, end(), std::forward<Args>(args)...);
        ++size_;
        return back();
    }

    void pop_back() {
        --size_;
        Allocator alloc = get_allocator();
        AllocTraits::destroy(alloc, end());
    }

    void resize(size_type count) {
        if (count < size_) {
            std::destroy_n(begin() + count, size_ - count);
        } else if (count > size_) {
            if (count > capacity()) reserve(count);
            std::uninitialized_value_construct_n(begin() + size_, count - size_);
        }
        size_ = count;
    }

    void resize(size_type count, const T& value) {
        if (count < size_) {
            std::destroy_n(begin() + count, size_ - count);
        } else if (count > size_) {
            if (count > capacity()) reserve(count);
            std::uninitialized_fill_n(begin() + size_, count - size_, value);
        }
        size_ = count;
    }

    void swap(SmallVector& other) noexcept {
        using std::swap;
        if (this == &other) return;

        if (is_inline_ && other.is_inline_) {
            // Both inline, swap contents element-wise
            size_t max_size = std::max(size_, other.size_);
            size_t min_size = std::min(size_, other.size_);
            
            for (size_t i = 0; i < min_size; ++i) {
                swap(operator[](i), other[i]);
            }
            
            if (size_ > min_size) {
                for (size_t i = min_size; i < size_; ++i) {
                    new (&other.begin()[i]) T(std::move(operator[](i)));
                    Allocator alloc = get_allocator();
                    AllocTraits::destroy(alloc, begin() + i);
                }
            } else if (other.size_ > min_size) {
                for (size_t i = min_size; i < other.size_; ++i) {
                    new (&begin()[i]) T(std::move(other[i]));
                    Allocator other_alloc = other.get_allocator();
                    AllocTraits::destroy(other_alloc, other.begin() + i);
                }
            }
            
            swap(size_, other.size_);
        } else if (!is_inline_ && !other.is_inline_) {
            // Both heap, swap heap data
            swap(heap_, other.heap_);
            swap(size_, other.size_);
        } else {
            // One inline, one heap - complex swap
            SmallVector* inline_vec = is_inline_ ? this : &other;
            SmallVector* heap_vec = is_inline_ ? &other : this;
            
            // Save heap data
            auto temp_data = heap_vec->heap_.data_;
            auto temp_capacity = heap_vec->heap_.capacity_;
            auto temp_alloc = std::move(heap_vec->heap_.allocator_);
            auto temp_size = heap_vec->size_;
            
            // Copy inline to heap_vec
            heap_vec->is_inline_ = true;
            heap_vec->size_ = inline_vec->size_;
            std::uninitialized_move(inline_vec->begin(), inline_vec->end(), heap_vec->begin());
            inline_vec->destroy_inline();
            
            // Move heap to inline_vec
            inline_vec->is_inline_ = false;
            inline_vec->heap_.data_ = temp_data;
            inline_vec->heap_.capacity_ = temp_capacity;
            inline_vec->heap_.allocator_ = std::move(temp_alloc);
            inline_vec->size_ = temp_size;
        }
    }

    // Element access
    reference operator[](size_type pos) {
        return begin()[pos];
    }

    const_reference operator[](size_type pos) const {
        return begin()[pos];
    }

    reference at(size_type pos) {
        if (pos >= size_) throw std::out_of_range("SmallVector::at");
        return operator[](pos);
    }

    const_reference at(size_type pos) const {
        if (pos >= size_) throw std::out_of_range("SmallVector::at");
        return operator[](pos);
    }

    reference front() {
        return operator[](0);
    }

    const_reference front() const {
        return operator[](0);
    }

    reference back() {
        return operator[](size_ - 1);
    }

    const_reference back() const {
        return operator[](size_ - 1);
    }

    T* data() noexcept {
        return begin();
    }

    const T* data() const noexcept {
        return begin();
    }

    // Assign
    void assign(size_type count, const T& value) {
        clear();
        if (count > capacity()) reserve(count);
        std::uninitialized_fill_n(begin(), count, value);
        size_ = count;
    }

    template <class InputIt>
    void assign(InputIt first, InputIt last) {
        clear();
        size_t count = std::distance(first, last);
        if (count > capacity()) reserve(count);
        std::uninitialized_copy(first, last, begin());
        size_ = count;
    }

    void assign(std::initializer_list<T> ilist) {
        assign(ilist.begin(), ilist.end());
    }
};

template <class T, size_t N, class Alloc>
bool operator==(const SmallVector<T, N, Alloc>& lhs, const SmallVector<T, N, Alloc>& rhs) {
    return lhs.size() == rhs.size() && std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

template <class T, size_t N, class Alloc>
bool operator!=(const SmallVector<T, N, Alloc>& lhs, const SmallVector<T, N, Alloc>& rhs) {
    return !(lhs == rhs);
}

}  // namespace cpp_utilities

#endif  // CPP_UTILITIES_SMALL_VECTOR_H
