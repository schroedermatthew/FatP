/**
 * @file SmallVector.h
 * @brief Small-size optimized vector with inline storage for zero-allocation small collections
 *
 * @details
 * SmallVector optimizes the common case of small collections by storing up to InlineCapacity
 * elements directly within the object, avoiding heap allocations entirely. When the size exceeds
 * InlineCapacity, the container automatically transitions to heap-allocated storage.
 *
 * Key Features:
 * - Zero heap allocations for small sizes (size <= InlineCapacity)
 * - Seamless automatic transition between inline and heap storage
 * - Full C++17 allocator model support
 * - STRONG exception safety guarantee for all reallocations
 * - Compatible with standard algorithms
 *
 * Performance:
 * - Small sizes: Zero allocations, optimal cache locality
 * - Large sizes: Standard vector performance with 2x geometric growth
 * - Move operations: O(1) for heap storage with equal allocators
 *
 * Implementation Notes:
 * - Uses pointer-based storage discrimination (LLVM-style) for optimal hot-path performance
 * - data_ pointer always valid - points to inline_buffer_ or heap allocation
 * - begin()/data() are simple pointer returns with no branching
 *
 * @example Basic Usage
 * @code
 * SmallVector<int, 8> vec;
 * vec.push_back(1);  // Uses inline storage
 * for (int i = 0; i < 100; ++i) {
 *     vec.push_back(i);  // Automatically transitions to heap at element 9
 * }
 *
 * SmallVector<std::string, 4> strings = {"a", "b", "c"};
 * strings.emplace_back("d");
 * @endcode
 *
 * @warning NOT thread-safe. Users must provide external synchronization for concurrent access.
 *
 * @see std::vector for standard vector interface comparison
 */

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <type_traits>
#include <utility>

#include "enforce.h"
#include "CheckedArithmetic.h"
#include "ScopeGuard.h"

namespace fat_p {

// Forward declaration for type trait
template <typename T, size_t C, typename A>
class SmallVector;

/**
 * @brief Small-size optimized vector with inline storage
 *
 * @details
 * Provides complete std::vector interface with optimization for small collections. Elements up to
 * InlineCapacity are stored directly in the object (no heap allocation). Automatically promotes to
 * heap storage when capacity is exceeded. shrink_to_fit() can demote back to inline storage.
 *
 * @tparam T Element type (must be MoveConstructible and Destructible)
 * @tparam InlineCapacity Number of elements to store inline before heap allocation (default: 8)
 * @tparam Allocator Allocator type for heap storage (default: std::allocator<T>)
 *
 * @invariant size_ <= capacity_
 * @invariant data_ points to either inline_buffer_ or heap allocation
 * @invariant Elements in [data_, data_ + size_) are constructed
 * @invariant Elements in [data_ + size_, data_ + capacity_) are uninitialized
 */
template <typename T, size_t InlineCapacity = 8, typename Allocator = std::allocator<T>>
class SmallVector {
private:
    using AllocTraits = std::allocator_traits<Allocator>;
    
    // Inline buffer for small element storage - no heap allocation needed
    alignas(T) std::byte inline_buffer_[InlineCapacity * sizeof(T)];
    
    // Always-valid pointer to current storage (inline or heap)
    // Hot path optimization: begin()/data() just return this pointer
    T* data_;
    
    // Current number of constructed elements
    size_t size_ = 0;
    
    // Current capacity (InlineCapacity when inline, heap capacity otherwise)
    size_t capacity_ = InlineCapacity;

#if FATP_HAS_CPP20
    // EBO allows zero-size allocators to occupy no space
    [[no_unique_address]] Allocator allocator_;
#else
    Allocator allocator_;
#endif 

    // Returns pointer to inline buffer as T*
    T* inline_ptr() noexcept {
        return reinterpret_cast<T*>(inline_buffer_);
    }
    
    const T* inline_ptr() const noexcept {
        return reinterpret_cast<const T*>(inline_buffer_);
    }

    // Runtime check for which storage mode is active
    // Simple pointer comparison - very fast
    bool is_inline() const noexcept {
        return data_ == inline_ptr();
    }

    /**
     * @brief Reallocates storage to accommodate new capacity
     *
     * @details
     * Core reallocation with STRONG exception safety. Uses nested ScopeGuards to track resource
     * allocation and element construction. If any operation throws, guards automatically clean up
     * and leave original buffer untouched.
     *
     * Growth strategy: 2x geometric growth for amortized O(1) push_back.
     * Uses CheckedArithmetic to detect capacity overflow safely.
     *
     * @param min_capacity Minimum required capacity after reallocation
     * @throws std::bad_alloc If allocation fails
     * @throws Any exception from T's move constructor
     */
    void grow(size_t min_capacity) {
        // Overflow-safe capacity calculation using CheckedArithmetic
        auto new_cap_result = checked_mul<ReturnExpectedPolicy>(capacity_, size_t(2));
        
        size_t new_cap;
        if (new_cap_result.has_value()) {
            new_cap = std::max(min_capacity, *new_cap_result);
        } else {
            new_cap = min_capacity;
            always_enforce(new_cap <= max_size(), "Requested capacity exceeds max_size");
        }
        
        if (new_cap == 0) {
            new_cap = std::max(min_capacity, size_t(1));
        }
        
        // Allocate new storage
        T* new_data = AllocTraits::allocate(allocator_, new_cap);
        
        // RAII guard to deallocate on exception
        auto cleanup_guard = makeScopeGuard([&]() noexcept {
            AllocTraits::deallocate(allocator_, new_data, new_cap);
        });
        
        // Track construction progress for exception rollback
        size_t constructed = 0;
        auto element_guard = makeScopeGuard([&]() noexcept {
            std::destroy_n(new_data, constructed);
        });
        
        // Move-construct elements (may throw)
        for (size_t i = 0; i < size_; ++i) {
            AllocTraits::construct(allocator_, new_data + i, std::move(data_[i]));
            ++constructed;
        }
        
        // Success - dismiss guards and commit changes
        element_guard.dismiss();
        cleanup_guard.dismiss();
        
        // Now safe to destroy old elements
        std::destroy_n(data_, size_);
        
        // Deallocate old heap storage if not inline
        if (!is_inline()) {
            AllocTraits::deallocate(allocator_, data_, capacity_);
        }
        
        // Update to new heap storage
        data_ = new_data;
        capacity_ = new_cap;
    }

public:
    // Standard container type aliases
    using value_type = T;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using iterator = T*;
    using const_iterator = const T*;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    // ==================================================================================
    // Constructors and Destructor
    // ==================================================================================

    /** @brief Default constructor creating empty vector with inline storage */
    SmallVector() noexcept 
        : data_(inline_ptr()), size_(0), capacity_(InlineCapacity), allocator_() {}

    /** @brief Constructor with explicit allocator */
    explicit SmallVector(const Allocator& alloc) noexcept
        : data_(inline_ptr()), size_(0), capacity_(InlineCapacity), allocator_(alloc) {}

    /** @brief Constructs vector with count default-constructed elements */
    explicit SmallVector(size_type count, const Allocator& alloc = Allocator()) 
        : SmallVector(alloc) {
        resize(count);
    }

    /** @brief Constructs vector with count copies of value */
    SmallVector(size_type count, const T& value, const Allocator& alloc = Allocator())
        : SmallVector(alloc) {
        assign(count, value);
    }

    /** @brief Range constructor from [first, last) */
    template <class InputIt, std::enable_if_t<!std::is_integral_v<InputIt>, int> = 0>
    SmallVector(InputIt first, InputIt last, const Allocator& alloc = Allocator()) 
        : SmallVector(alloc) {
        assign(first, last);
    }

    /** @brief Constructs from initializer list */
    SmallVector(std::initializer_list<T> ilist, const Allocator& alloc = Allocator())
        : SmallVector(ilist.begin(), ilist.end(), alloc) {}

    /**
     * @brief Copy constructor
     * @note Allocator selected via select_on_container_copy_construction per C++17 standard
     */
    SmallVector(const SmallVector& other)
        : SmallVector(AllocTraits::select_on_container_copy_construction(other.allocator_)) {
        assign(other.begin(), other.end());
    }

    /** @brief Copy constructor with explicit allocator */
    SmallVector(const SmallVector& other, const Allocator& alloc) : SmallVector(alloc) {
        assign(other.begin(), other.end());
    }

    /**
     * @brief Move constructor
     * @note Steals heap storage in O(1), inline storage requires O(N) element-wise move
     */
    SmallVector(SmallVector&& other) noexcept
        : data_(inline_ptr()), size_(0), capacity_(InlineCapacity),
          allocator_(std::move(other.allocator_)) {
        if (other.is_inline()) {
            // Inline storage requires element-wise move
            std::uninitialized_move_n(other.data_, other.size_, data_);
            size_ = other.size_;
            std::destroy_n(other.data_, other.size_);
        } else {
            // Heap storage: O(1) pointer steal
            data_ = other.data_;
            size_ = other.size_;
            capacity_ = other.capacity_;
            // Reset other to inline state
            other.data_ = other.inline_ptr();
            other.capacity_ = InlineCapacity;
        }
        other.size_ = 0;
    }

    /**
     * @brief Move constructor with explicit allocator
     * @note If allocators unequal, performs element-wise move instead of pointer steal
     */
    SmallVector(SmallVector&& other, const Allocator& alloc) : SmallVector(alloc) {
        if (allocator_ == other.allocator_) {
            // Equal allocators: can steal resources
            if (other.is_inline()) {
                std::uninitialized_move_n(other.data_, other.size_, data_);
                size_ = other.size_;
                std::destroy_n(other.data_, other.size_);
            } else {
                data_ = other.data_;
                size_ = other.size_;
                capacity_ = other.capacity_;
                other.data_ = other.inline_ptr();
                other.capacity_ = InlineCapacity;
            }
            other.size_ = 0;
        } else {
            // Unequal allocators: must perform element-wise move
            assign(std::make_move_iterator(other.begin()), 
                   std::make_move_iterator(other.end()));
        }
    }

    /** @brief Destructor */
    ~SmallVector() noexcept {
        std::destroy_n(data_, size_);
        if (!is_inline()) {
            AllocTraits::deallocate(allocator_, data_, capacity_);
        }
    }

    /**
     * @brief Copy assignment
     * @note Implements POCCA (Propagate On Container Copy Assignment) semantics
     */
    SmallVector& operator=(const SmallVector& other) {
        if (this == &other) return *this;
        
        constexpr bool Pocca = AllocTraits::propagate_on_container_copy_assignment::value;
        
        if constexpr (Pocca) {
            if (allocator_ != other.allocator_) {
                // Allocator will change - must deallocate with old allocator first
                clear();
                if (!is_inline()) {
                    AllocTraits::deallocate(allocator_, data_, capacity_);
                    data_ = inline_ptr();
                    capacity_ = InlineCapacity;
                }
                allocator_ = other.allocator_;
            }
        }
        
        assign(other.begin(), other.end());
        return *this;
    }

    /**
     * @brief Move assignment
     * @note Implements POCMA (Propagate On Container Move Assignment) semantics
     */
    SmallVector& operator=(SmallVector&& other) noexcept(
        AllocTraits::is_always_equal::value || 
        AllocTraits::propagate_on_container_move_assignment::value
    ) {
        if (this == &other) return *this;

        // Clean up existing resources
        std::destroy_n(data_, size_);
        if (!is_inline()) {
            AllocTraits::deallocate(allocator_, data_, capacity_);
        }
        
        // Reset to inline state
        data_ = inline_ptr();
        capacity_ = InlineCapacity;
        size_ = 0;

        constexpr bool Pocma = AllocTraits::propagate_on_container_move_assignment::value;

        if constexpr (Pocma) {
            // POCMA=true: always propagate allocator and steal resources
            allocator_ = std::move(other.allocator_);
        } else if (allocator_ != other.allocator_) {
            // POCMA=false and allocators unequal: element-wise move required
            assign(std::make_move_iterator(other.begin()), 
                   std::make_move_iterator(other.end()));
            other.clear();
            return *this;
        }
        // POCMA=false and allocators equal: can steal without propagation

        // Perform efficient resource transfer
        if (other.is_inline()) {
            std::uninitialized_move_n(other.data_, other.size_, data_);
            std::destroy_n(other.data_, other.size_);
        } else {
            data_ = other.data_;
            capacity_ = other.capacity_;
            other.data_ = other.inline_ptr();
            other.capacity_ = InlineCapacity;
        }
        size_ = other.size_;

        // Leave other in valid empty state
        other.size_ = 0;
        
        return *this;
    }

    /** @brief Replaces contents with initializer list */
    SmallVector& operator=(std::initializer_list<T> ilist) {
        assign(ilist);
        return *this;
    }

    // ==================================================================================
    // Assign - Replace Container Contents
    // ==================================================================================

    /** @brief Replaces contents with count copies of value */
    void assign(size_type count, const T& value) {
        clear();
        if (count > capacity_) {
            reserve(count);
        }
        std::uninitialized_fill_n(data_, count, value);
        size_ = count;
    }

    /** @brief Replaces contents with elements from range [first, last) */
    template <class InputIt, std::enable_if_t<!std::is_integral_v<InputIt>, int> = 0>
    void assign(InputIt first, InputIt last) {
        clear();
        size_t count = std::distance(first, last);
        if (count > capacity_) {
            reserve(count);
        }
        std::uninitialized_copy(first, last, data_);
        size_ = count;
    }

    /** @brief Replaces contents with elements from initializer list */
    void assign(std::initializer_list<T> ilist) {
        assign(ilist.begin(), ilist.end());
    }

    // ==================================================================================
    // Iterators - HOT PATH - Simple pointer returns, no branching
    // ==================================================================================

    iterator begin() noexcept { return data_; }
    const_iterator begin() const noexcept { return data_; }
    const_iterator cbegin() const noexcept { return data_; }
    
    iterator end() noexcept { return data_ + size_; }
    const_iterator end() const noexcept { return data_ + size_; }
    const_iterator cend() const noexcept { return data_ + size_; }
    
    reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
    const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    const_reverse_iterator crbegin() const noexcept { return rbegin(); }
    
    reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
    const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
    const_reverse_iterator crend() const noexcept { return rend(); }

    // ==================================================================================
    // Capacity and Size
    // ==================================================================================

    /** @brief Returns number of elements */
    size_type size() const noexcept { return size_; }

    /** @brief Returns maximum possible number of elements */
    size_type max_size() const noexcept {
        return std::min(AllocTraits::max_size(allocator_),
                        std::numeric_limits<size_type>::max() / sizeof(T));
    }

    /**
     * @brief Returns current capacity
     * @note For inline storage returns InlineCapacity, for heap returns allocated capacity
     */
    size_type capacity() const noexcept { return capacity_; }

    /** @brief Checks if container is empty */
    bool empty() const noexcept { return size_ == 0; }

    /**
     * @brief Increases capacity to at least new_cap
     * @note Does nothing if new_cap <= current capacity
     * @throws std::bad_alloc or exception from T's move constructor
     */
    void reserve(size_type new_cap) {
        if (new_cap <= capacity_) {
            return;
        }
        grow(new_cap);
    }

    /**
     * @brief Reduces capacity to fit current size or InlineCapacity
     * @note Transitions from heap to inline storage if size <= InlineCapacity
     */
    void shrink_to_fit() {
        if (is_inline() || size_ > InlineCapacity) {
            return;
        }
        
        T* old_data = data_;
        size_t old_capacity = capacity_;
        
        // Move back to inline storage
        data_ = inline_ptr();
        capacity_ = InlineCapacity;
        
        std::uninitialized_move_n(old_data, size_, data_);
        std::destroy_n(old_data, size_);
        AllocTraits::deallocate(allocator_, old_data, old_capacity);
    }

    // ==================================================================================
    // Element Access
    // ==================================================================================

    /**
     * @brief Access element with bounds checking
     * @throws always_enforce exception if pos >= size()
     */
    reference at(size_type pos) {
        always_enforce(pos < size_, "Index ", pos, " out of bounds (size=", size_, ")");
        return data_[pos];
    }

    /** @brief Access element with bounds checking (const) */
    const_reference at(size_type pos) const {
        always_enforce(pos < size_, "Index ", pos, " out of bounds (size=", size_, ")");
        return data_[pos];
    }

    /**
     * @brief Access element without bounds checking
     * @warning Undefined behavior if pos >= size() in release builds
     * @note Debug builds check bounds via enforce()
     */
    reference operator[](size_type pos) {
        enforce(pos < size_, "Index out of bounds");
        return data_[pos];
    }

    /** @brief Access element without bounds checking (const) */
    const_reference operator[](size_type pos) const {
        enforce(pos < size_, "Index out of bounds");
        return data_[pos];
    }

    /** @brief Access first element */
    reference front() {
        enforce(size_ > 0, "Cannot access front of empty vector");
        return data_[0];
    }

    /** @brief Access first element (const) */
    const_reference front() const {
        enforce(size_ > 0, "Cannot access front of empty vector");
        return data_[0];
    }

    /** @brief Access last element */
    reference back() {
        enforce(size_ > 0, "Cannot access back of empty vector");
        return data_[size_ - 1];
    }

    /** @brief Access last element (const) */
    const_reference back() const {
        enforce(size_ > 0, "Cannot access back of empty vector");
        return data_[size_ - 1];
    }

    /**
     * @brief Returns pointer to underlying element storage
     * @note Pointer valid until reallocation. May point to inline or heap storage.
     */
    T* data() noexcept { return data_; }

    /** @brief Returns const pointer to underlying element storage */
    const T* data() const noexcept { return data_; }
    
    // ==================================================================================
    // Modifiers
    // ==================================================================================

    /**
     * @brief Removes all elements
     * @note Preserves current capacity. Storage mode unchanged.
     */
    void clear() noexcept {
        std::destroy_n(data_, size_);
        size_ = 0;
    }

    /** @brief Inserts copy of value before pos */
    iterator insert(const_iterator pos, const T& value) {
        return emplace(pos, value);
    }

    /** @brief Inserts value by move before pos */
    iterator insert(const_iterator pos, T&& value) {
        return emplace(pos, std::move(value));
    }

    /**
     * @brief Inserts count copies of value before pos
     * @note Provides STRONG exception safety
     * @throws std::bad_alloc or exception from T's copy constructor
     */
    iterator insert(const_iterator pos, size_type count, const T& value) {
        size_t idx = pos - data_;
        enforce(idx <= size_, "Insert position out of range");
        
        if (count == 0) {
            return data_ + idx;
        }
        
        // Verify new size won't overflow
        auto new_size_result = checked_add<ReturnExpectedPolicy>(size_, count);
        always_enforce(new_size_result.has_value(), "Insert would exceed max_size");
        size_t new_size = *new_size_result;
        
        if (new_size > capacity_) {
            // Growth path with STRONG exception safety
            auto new_cap_result = checked_mul<ReturnExpectedPolicy>(capacity_, size_t(2));
            size_t new_cap = new_cap_result.has_value() ? 
                std::max(new_size, *new_cap_result) : new_size;
            
            T* new_data = AllocTraits::allocate(allocator_, new_cap);
            
            auto cleanup_guard = makeScopeGuard([&]() noexcept {
                AllocTraits::deallocate(allocator_, new_data, new_cap);
            });
            
            size_t constructed = 0;
            auto element_guard = makeScopeGuard([&]() noexcept {
                std::destroy_n(new_data, constructed);
            });
            
            // Move prefix
            for (size_t i = 0; i < idx; ++i) {
                AllocTraits::construct(allocator_, new_data + i, std::move(data_[i]));
                ++constructed;
            }
            
            // Insert count copies
            for (size_t i = 0; i < count; ++i) {
                AllocTraits::construct(allocator_, new_data + idx + i, value);
                ++constructed;
            }
            
            // Move suffix
            for (size_t i = idx; i < size_; ++i) {
                AllocTraits::construct(allocator_, new_data + i + count, std::move(data_[i]));
                ++constructed;
            }
            
            element_guard.dismiss();
            cleanup_guard.dismiss();
            
            // Clean up old storage and commit
            std::destroy_n(data_, size_);
            if (!is_inline()) {
                AllocTraits::deallocate(allocator_, data_, capacity_);
            }
            
            data_ = new_data;
            capacity_ = new_cap;
            size_ = new_size;
            return data_ + idx;
        }
        
        // In-place insertion without reallocation
        iterator insert_pos = data_ + idx;
        
        if (idx < size_) {
            size_t tail = size_ - idx;
            // Two cases: tail fits entirely in uninit space, or tail extends beyond insertion
            if (tail <= count) {
                std::uninitialized_move_n(insert_pos, tail, insert_pos + count);
                std::fill_n(insert_pos, count, value);
            } else {
                std::uninitialized_move_n(end() - count, count, end());
                std::move_backward(insert_pos, end() - count, end());
                std::fill_n(insert_pos, count, value);
            }
        } else {
            // Inserting at end into uninitialized space
            std::uninitialized_fill_n(insert_pos, count, value);
        }
        
        size_ = new_size;
        return insert_pos;
    }

    /**
     * @brief Inserts elements from range [first, last) before pos
     * @note Provides STRONG exception safety
     */
    template <class InputIt, std::enable_if_t<!std::is_integral_v<InputIt>, int> = 0>
    iterator insert(const_iterator pos, InputIt first, InputIt last) {
        size_t idx = pos - data_;
        enforce(idx <= size_, "Insert position out of range");
        
        size_t count = std::distance(first, last);
        if (count == 0) {
            return data_ + idx;
        }
        
        auto new_size_result = checked_add<ReturnExpectedPolicy>(size_, count);
        always_enforce(new_size_result.has_value(), "Insert would exceed max_size");
        size_t new_size = *new_size_result;
        
        if (new_size > capacity_) {
            // Growth path with STRONG exception safety
            auto new_cap_result = checked_mul<ReturnExpectedPolicy>(capacity_, size_t(2));
            size_t new_cap = new_cap_result.has_value() ? 
                std::max(new_size, *new_cap_result) : new_size;
            
            T* new_data = AllocTraits::allocate(allocator_, new_cap);
            
            auto cleanup_guard = makeScopeGuard([&]() noexcept {
                AllocTraits::deallocate(allocator_, new_data, new_cap);
            });
            
            size_t constructed = 0;
            auto element_guard = makeScopeGuard([&]() noexcept {
                std::destroy_n(new_data, constructed);
            });
            
            // Move prefix
            for (size_t i = 0; i < idx; ++i) {
                AllocTraits::construct(allocator_, new_data + i, std::move(data_[i]));
                ++constructed;
            }
            
            // Copy inserted range
            for (auto it = first; it != last; ++it) {
                AllocTraits::construct(allocator_, new_data + constructed, *it);
                ++constructed;
            }
            
            // Move suffix
            for (size_t i = idx; i < size_; ++i) {
                AllocTraits::construct(allocator_, new_data + i + count, std::move(data_[i]));
                ++constructed;
            }
            
            element_guard.dismiss();
            cleanup_guard.dismiss();
            
            std::destroy_n(data_, size_);
            if (!is_inline()) {
                AllocTraits::deallocate(allocator_, data_, capacity_);
            }
            
            data_ = new_data;
            capacity_ = new_cap;
            size_ = new_size;
            return data_ + idx;
        }
        
        // In-place insertion
        iterator insert_pos = data_ + idx;
        
        if (idx < size_) {
            size_t tail = size_ - idx;
            if (tail <= count) {
                std::uninitialized_move_n(insert_pos, tail, insert_pos + count);
                std::copy(first, last, insert_pos);
            } else {
                std::uninitialized_move_n(end() - count, count, end());
                std::move_backward(insert_pos, end() - count, end());
                std::copy(first, last, insert_pos);
            }
        } else {
            std::uninitialized_copy(first, last, insert_pos);
        }
        
        size_ = new_size;
        return insert_pos;
    }

    /** @brief Inserts elements from initializer list before pos */
    iterator insert(const_iterator pos, std::initializer_list<T> ilist) {
        return insert(pos, ilist.begin(), ilist.end());
    }

    /**
     * @brief Constructs element in-place before pos
     *
     * @details
     * Provides STRONG exception safety during reallocation, basic guarantee for in-place
     * construction. For trivially moveable and destructible types, uses memmove optimization
     * to shift elements efficiently.
     *
     * @param pos Iterator before which element will be constructed
     * @param args Arguments forwarded to T's constructor
     * @return Iterator pointing to constructed element
     * @throws Exception from T's constructor or move constructor
     */
    template <class... Args>
    iterator emplace(const_iterator pos, Args&&... args) {
        size_t idx = pos - data_;
        enforce(idx <= size_, "Emplace position out of range");
        
        if (size_ >= capacity_) {
            // Growth path with strong exception safety
            size_t new_cap = capacity_ == 0 ? 1 : capacity_ * 2;
            
            T* new_data = AllocTraits::allocate(allocator_, new_cap);
            
            auto cleanup_guard = makeScopeGuard([&]() noexcept {
                AllocTraits::deallocate(allocator_, new_data, new_cap);
            });
            
            size_t constructed = 0;
            auto element_guard = makeScopeGuard([&]() noexcept {
                std::destroy_n(new_data, constructed);
            });
            
            // Move prefix
            for (size_t i = 0; i < idx; ++i) {
                AllocTraits::construct(allocator_, new_data + i, std::move(data_[i]));
                ++constructed;
            }
            
            // Construct new element
            AllocTraits::construct(allocator_, new_data + idx, std::forward<Args>(args)...);
            ++constructed;
            
            // Move suffix
            for (size_t i = idx; i < size_; ++i) {
                AllocTraits::construct(allocator_, new_data + i + 1, std::move(data_[i]));
                ++constructed;
            }
            
            element_guard.dismiss();
            cleanup_guard.dismiss();
            
            // Cleanup old storage
            std::destroy_n(data_, size_);
            if (!is_inline()) {
                AllocTraits::deallocate(allocator_, data_, capacity_);
            }
            
            data_ = new_data;
            capacity_ = new_cap;
            ++size_;
            return data_ + idx;
        }
        
        // Non-growing path: size_ < capacity_
        iterator insert_pos = data_ + idx;
        
        if (idx < size_) {
            // HPC optimization: use memmove for trivially moveable types
            if constexpr (std::is_trivially_move_constructible_v<T> && 
                          std::is_trivially_move_assignable_v<T> && 
                          std::is_trivially_destructible_v<T>) {
                size_t tail_len_bytes = (size_ - idx) * sizeof(T);
                std::memmove(insert_pos + 1, insert_pos, tail_len_bytes);
            } else {
                // Standard path for non-trivial types
                AllocTraits::construct(allocator_, end(), std::move(*(end() - 1)));
                std::move_backward(insert_pos, end() - 1, end());
                std::destroy_at(insert_pos);
            }
            AllocTraits::construct(allocator_, insert_pos, std::forward<Args>(args)...);
        } else {
            // Inserting at end
            AllocTraits::construct(allocator_, insert_pos, std::forward<Args>(args)...);
        }
        
        ++size_;
        return insert_pos;
    }

    /**
     * @brief Erases element at pos
     * @param pos Iterator to element to erase
     * @return Iterator to element following erased element
     */
    iterator erase(const_iterator pos) {
        size_t idx = pos - data_;
        enforce(idx < size_, "Erase position out of range");
        
        iterator it = data_ + idx;
        std::move(it + 1, end(), it);
        std::destroy_at(end() - 1);
        --size_;
        return it;
    }

    /**
     * @brief Erases range [first, last)
     * @return Iterator to element following erased range
     */
    iterator erase(const_iterator first, const_iterator last) {
        if (first == last) {
            return const_cast<iterator>(first);
        }
        
        size_t first_idx = first - data_;
        size_t last_idx = last - data_;
        enforce(first_idx <= last_idx && last_idx <= size_, "Erase range invalid");
        
        iterator f = data_ + first_idx;
        iterator l = data_ + last_idx;
        std::move(l, end(), f);
        size_t count = last_idx - first_idx;
        std::destroy_n(end() - count, count);
        size_ -= count;
        return f;
    }

    /** @brief Appends copy of value */
    void push_back(const T& value) {
        emplace_back(value);
    }

    /** @brief Appends value by move */
    void push_back(T&& value) {
        emplace_back(std::move(value));
    }

    /**
     * @brief Constructs element in-place at end
     * @return Reference to newly constructed element
     * @note Provides STRONG exception safety
     */
    template <class... Args>
    reference emplace_back(Args&&... args) {
        if (size_ >= capacity_) {
            size_t new_cap = capacity_ == 0 ? 1 : capacity_ * 2;
            reserve(new_cap);
        }
        
        AllocTraits::construct(allocator_, data_ + size_, std::forward<Args>(args)...);
        ++size_;
        return data_[size_ - 1];
    }

    /** @brief Removes last element */
    void pop_back() {
        always_enforce(size_ > 0, "Cannot pop from empty vector");
        --size_;
        std::destroy_at(data_ + size_);
    }

    /**
     * @brief Resizes container to count elements
     * @note If count > size, default-constructs new elements. If count < size, destroys excess.
     */
    void resize(size_type count) {
        if (count < size_) {
            std::destroy_n(data_ + count, size_ - count);
        } else if (count > size_) {
            if (count > capacity_) {
                reserve(count);
            }
            std::uninitialized_value_construct_n(data_ + size_, count - size_);
        }
        size_ = count;
    }

    /** @brief Resizes container, initializing new elements with value */
    void resize(size_type count, const T& value) {
        if (count < size_) {
            std::destroy_n(data_ + count, size_ - count);
        } else if (count > size_) {
            if (count > capacity_) {
                reserve(count);
            }
            std::uninitialized_fill_n(data_ + size_, count - size_, value);
        }
        size_ = count;
    }

    /**
     * @brief Swaps contents with another SmallVector
     * @note Implements POCS (Propagate On Container Swap) semantics
     * @warning If POCS is false, allocators must be equal (undefined behavior otherwise)
     */
    void swap(SmallVector& other) noexcept(
        AllocTraits::is_always_equal::value || 
        AllocTraits::propagate_on_container_swap::value) {
        if (this == &other) {
            return;
        }
        
        constexpr bool Pocs = AllocTraits::propagate_on_container_swap::value;
        
        if constexpr (Pocs) {
            using std::swap;
            swap(allocator_, other.allocator_);
        } else {
            always_enforce(allocator_ == other.allocator_, 
                          "Cannot swap containers with unequal allocators when POCS is false");
        }
        
        // Both inline: element-wise swap
        if (is_inline() && other.is_inline()) {
            size_t min_size = std::min(size_, other.size_);
            size_t max_size = std::max(size_, other.size_);
            
            // Swap common elements
            for (size_t i = 0; i < min_size; ++i) {
                using std::swap;
                swap(data_[i], other.data_[i]);
            }
            
            // Move excess elements
            if (size_ > other.size_) {
                std::uninitialized_move_n(data_ + min_size, size_ - min_size, 
                                          other.data_ + min_size);
                std::destroy_n(data_ + min_size, size_ - min_size);
            } else if (other.size_ > size_) {
                std::uninitialized_move_n(other.data_ + min_size, other.size_ - min_size,
                                          data_ + min_size);
                std::destroy_n(other.data_ + min_size, other.size_ - min_size);
            }
            
            using std::swap;
            swap(size_, other.size_);
            return;
        }
        
        // Both heap: just swap pointers
        if (!is_inline() && !other.is_inline()) {
            using std::swap;
            swap(data_, other.data_);
            swap(size_, other.size_);
            swap(capacity_, other.capacity_);
            return;
        }
        
        // One inline, one heap: move inline to heap's inline, steal heap pointer
        SmallVector* inline_vec = is_inline() ? this : &other;
        SmallVector* heap_vec = is_inline() ? &other : this;
        
        // Save heap info
        T* heap_data = heap_vec->data_;
        size_t heap_size = heap_vec->size_;
        size_t heap_cap = heap_vec->capacity_;
        
        // Move inline elements to heap_vec's inline buffer
        heap_vec->data_ = heap_vec->inline_ptr();
        heap_vec->capacity_ = InlineCapacity;
        std::uninitialized_move_n(inline_vec->data_, inline_vec->size_, heap_vec->data_);
        heap_vec->size_ = inline_vec->size_;
        
        // Destroy inline elements and give heap to inline_vec
        std::destroy_n(inline_vec->data_, inline_vec->size_);
        inline_vec->data_ = heap_data;
        inline_vec->size_ = heap_size;
        inline_vec->capacity_ = heap_cap;
    }

    /** @brief Returns copy of allocator */
    Allocator get_allocator() const noexcept {
        return allocator_;
    }
};

// ==================================================================================
// Non-member Comparison Operators
// ==================================================================================

template <class T, size_t N, class Alloc = std::allocator<T>>
bool operator==(const SmallVector<T, N, Alloc>& lhs, const SmallVector<T, N, Alloc>& rhs) {
    return lhs.size() == rhs.size() && std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

template <class T, size_t N, class Alloc = std::allocator<T>>
bool operator!=(const SmallVector<T, N, Alloc>& lhs, const SmallVector<T, N, Alloc>& rhs) {
    return !(lhs == rhs);
}

template <class T, size_t N, class Alloc = std::allocator<T>>
bool operator<(const SmallVector<T, N, Alloc>& lhs, const SmallVector<T, N, Alloc>& rhs) {
    return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

template <class T, size_t N, class Alloc = std::allocator<T>>
bool operator<=(const SmallVector<T, N, Alloc>& lhs, const SmallVector<T, N, Alloc>& rhs) {
    return !(rhs < lhs);
}

template <class T, size_t N, class Alloc = std::allocator<T>>
bool operator>(const SmallVector<T, N, Alloc>& lhs, const SmallVector<T, N, Alloc>& rhs) {
    return rhs < lhs;
}

template <class T, size_t N, class Alloc = std::allocator<T>>
bool operator>=(const SmallVector<T, N, Alloc>& lhs, const SmallVector<T, N, Alloc>& rhs) {
    return !(lhs < rhs);
}

// ==================================================================================
// Type Traits
// ==================================================================================

// Specialization for SmallVector types
template <typename T, size_t C, typename A>
struct is_small_vector<SmallVector<T, C, A>> : std::true_type {};

} // namespace fat_p

// ==================================================================================
// C++17 PMR Support
// ==================================================================================

#if __cplusplus >= 201703L && __has_include(<memory_resource>)
#include <memory_resource>
namespace fat_p {
    /**
     * @brief PMR alias for SmallVector using polymorphic allocator
     * @note Allows runtime polymorphic allocator selection
     */
    template <typename T, size_t N>
    using PMRSmallVector = SmallVector<T, N, std::pmr::polymorphic_allocator<T>>;
}
#endif
