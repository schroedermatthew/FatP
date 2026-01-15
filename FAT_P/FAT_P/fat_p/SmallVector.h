/**
 * @file SmallVector.h
 * @brief Small-size optimized vector with inline storage for zero-allocation small collections
 *
 * 
 *
 * @layer Containers
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
 * - Strong exception safety for reallocations (when T is nothrow-movable or copyable)
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
 * @section contract_qoi Contract vs Quality-of-Implementation (QoI)
 *
 * SmallVector mirrors the formal contract of std::vector. Behavior is classified as:
 *
 * - **Contract**: Required by the C++ standard; guaranteed to work.
 * - **QoI**: Matches major std::vector implementations but is NOT required by the standard.
 * - **UB**: Undefined behavior per the standard; no guarantees even if it appears to work.
 *
 * @subsection aliasing_note Aliasing-Safe Growth (QoI)
 *
 * This implementation orders growth operations to tolerate self-referential insertion:
 * @code
 * SmallVector<int, 4> v = {1, 2, 3, 4};
 * v.push_back(v[0]);  // Works in this implementation (QoI)
 * @endcode
 *
 * **Important**: Aliasing-safe growth ordering is a Quality-of-Implementation improvement.
 * It matches the behavior of major std::vector implementations (libstdc++, libc++, MSVC),
 * but is NOT required by the C++ standard and is NOT part of this container's formal contract.
 * Code relying on this behavior is technically non-portable.
 *
 * @subsection type_requirements Type Requirements by Operation
 *
 * While the primary template requires T to be MoveConstructible and Destructible,
 * some operations impose additional requirements, mirroring std::vector:
 *
 * | Operation                  | Additional Requirements                              |
 * |----------------------------|------------------------------------------------------|
 * | insert(pos, count, value)  | CopyInsertable and CopyAssignable                    |
 * | insert(pos, first, last)   | EmplaceConstructible; CopyAssignable for in-place    |
 * | resize(count)              | DefaultInsertable (default-constructible via alloc)  |
 * | resize(count, value)       | CopyInsertable                                       |
 * | push_back(const T&)        | CopyInsertable                                       |
 * | emplace(pos, args...)      | EmplaceConstructible; MoveAssignable for in-place    |
 *
 * Note: In-place insertion paths (no reallocation) may use assignment into
 * moved-from elements. This is standard-conforming per [sequence.reqmts].
 *
 * @subsection guarantee_matrix Exception Safety & Behavior Guarantees
 *
 * | Operation                    | Guarantee    | Notes                                      |
 * |------------------------------|--------------|------------------------------------------- |
 * | reserve() / grow()           | Strong*      | Original state preserved on exception      |
 * | push_back() with realloc     | Strong*      | Container preserved; aliasing is QoI       |
 * | push_back() no realloc       | Strong       | Element constructed at end                 |
 * | emplace_back() with realloc  | Strong*      | Original state preserved on exception      |
 * | emplace_back() no realloc    | Strong       | Element constructed at end                 |
 * | emplace() with realloc       | Strong*      | Original state preserved on exception      |
 * | emplace() no realloc         | Basic        | Container valid; value at insertion point  |
 * |                              |              | may be modified or replaced on exception   |
 * | insert(pos, value)           | Strong*      | With realloc; Basic without                |
 * | insert(pos, count, value)    | Strong*      | With realloc; Basic without                |
 * | insert(pos, first, last)     | Strong*      | With realloc for forward+ iters; Basic for |
 * |                              |              | input iters or without realloc             |
 * | erase()                      | Basic        | Elements shifted, no reallocation          |
 * | shrink_to_fit()              | Strong*      | Original state preserved on exception      |
 * | swap()                       | Basic        | If move can throw; no resource leaks;      |
 * |                              |              | containers remain valid; noexcept otherwise|
 * | clear()                      | No-throw     | Always succeeds                            |
 * | Self-range insert            | **UB**       | Applies to all iterator categories;        |
 * |                              |              | e.g., v.insert(pos, v.begin(), v.end())    |
 * | Self-reference push_back     | QoI          | Works but not guaranteed by standard       |
 *
 * *Strong guarantee requires T to be nothrow_move_constructible OR CopyConstructible.
 *  When T has only a throwing move constructor and no copy constructor, reallocation
 *  operations provide only the Basic guarantee (container remains valid but may be
 *  in a modified state). This matches std::vector behavior via std::move_if_noexcept.
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
 *
 * // CTAD (C++17): deduces SmallVector<int, 8>
 * SmallVector v = {1, 2, 3, 4, 5};
 *
 * // Cross-capacity comparison
 * SmallVector<int, 4> small = {1, 2, 3};
 * SmallVector<int, 16> large = {1, 2, 3};
 * assert(small == large);  // Works!
 * @endcode
 *
 * @warning NOT thread-safe. Users must provide external synchronization for concurrent access.
 *
 * @see std::vector for standard vector interface comparison
 */

#pragma once

/*
FATP_META:
  meta_version: 1
  component: SmallVector
  file_role: public_header
  path: fat_p/SmallVector.h
  namespace: fat_p
  layer: Containers
  summary: "Public header for SmallVector."
  api_stability: in_work
  related:
    docs_search: "SmallVector"
    tests:
      - tests/test_FatPTypeTraits.cpp
      - tests/test_SmallVector.cpp
    benchmarks:
      - benchmarks/benchmark_SmallVector.cpp
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 4
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>

#include "enforce.h"
#include "CheckedArithmetic.h"
#include "ScopeGuard.h"
#include "FatPTypeTraits.h"  // For is_small_vector primary template

#if FATP_HAS_CPP20
#include <compare>
#endif

// ==================================================================================
// Portable Compiler Intrinsics
// ==================================================================================

// Branch prediction hints - help optimizer but not required for correctness
#if defined(__GNUC__) || defined(__clang__)
    #define FATP_LIKELY(x)   __builtin_expect(!!(x), 1)
    #define FATP_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    // MSVC and others: no-op, optimizer handles it
    #define FATP_LIKELY(x)   (x)
    #define FATP_UNLIKELY(x) (x)
#endif

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
 * @tparam T Element type (must be MoveConstructible and Destructible;
 *           see @ref type_requirements for operation-specific requirements)
 * @tparam InlineCapacity Number of elements to store inline before heap allocation (default: 8)
 * @tparam Allocator Allocator type for heap storage (default: std::allocator<T>).
 *                   Must satisfy alignment requirements for T per the C++ allocator model.
 *
 * @invariant size_ <= mCapacity
 * @invariant data_ points to either inline_buffer_ or heap allocation
 * @invariant Elements in [data_, data_ + size_) are constructed
 * @invariant Elements in [data_ + size_, data_ + mCapacity) are uninitialized
 */
template <typename T, size_t InlineCapacity = 8, typename Allocator = std::allocator<T>>
class SmallVector {
    static_assert(InlineCapacity > 0,
        "SmallVector requires InlineCapacity > 0. "
        "Use std::vector if no inline storage is desired, or std::max(N, 1) in generic code.");

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
    size_t mCapacity = InlineCapacity;

    // EBO allows zero-size allocators to occupy no space
    // [[no_unique_address]] is C++20 but GCC/Clang support it in C++17 mode
#if __has_cpp_attribute(no_unique_address)
    [[no_unique_address]] Allocator mAllocator;
#else
    Allocator mAllocator;
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
     * @brief Debug-only invariant verification
     * 
     * Validates internal consistency at mutation boundaries.
     * Zero cost in release builds.
     */
    void assert_invariants() const noexcept {
#ifndef NDEBUG
        FATP_ENFORCE(data_ != nullptr, "data_ is null");
        FATP_ENFORCE(size_ <= mCapacity, "size_ > capacity_");
        if (data_ == inline_ptr()) {
            FATP_ENFORCE(mCapacity == InlineCapacity, 
                    "inline storage but capacity != InlineCapacity");
        } else {
            FATP_ENFORCE(mCapacity > InlineCapacity, 
                    "heap storage but capacity <= InlineCapacity");
        }
#endif
    }

    // ==================================================================================
    // Debug Aliasing Detection (debug-only diagnostics)
    // ==================================================================================
    
    /**
     * @brief Checks if pointer points into this container's data
     * @note Used by debug aliasing detection
     */
    bool aliases_this(const T* p) const noexcept {
        return p >= data_ && p < data_ + size_;
    }

    /**
     * @brief Returns index of pointer within container
     * @pre aliases_this(p) must be true
     */
    size_t index_of_ptr(const T* p) const noexcept {
        return static_cast<size_t>(p - data_);
    }

    /**
     * @brief Debug check for self-referential push_back
     * 
     * Detects when push_back argument references an element inside this container
     * AND reallocation might occur (size == capacity).
     * 
     * @note Debug-only diagnostic. No-op in release builds.
     * @note This is a QoI check; the implementation handles aliasing safely,
     *       but users shouldn't rely on it.
     */
    void debug_check_self_ref_push_back([[maybe_unused]] const T* value_ptr) const {
#ifndef NDEBUG
        if (size_ == mCapacity && aliases_this(value_ptr)) {
            FATP_ENFORCE(false,
                "push_back: argument aliases this container and reallocation may occur "
                "(debug diagnostic; behavior is QoI, not guaranteed by standard)");
        }
#endif
    }

    /**
     * @brief Debug check for self-referential insert
     * 
     * Detects two dangerous aliasing scenarios:
     * 1. Reallocation risk: size == capacity AND value aliases container
     * 2. Shift corruption: value aliases element at/after insertion point
     *    (even without realloc, shifting moves the referenced element before
     *    its value is consumed)
     * 
     * @param idx Insertion index
     * @param value_ptr Pointer to value being inserted
     * 
     * @note Debug-only diagnostic. No-op in release builds.
     * @note This is a QoI check; users shouldn't rely on aliasing behavior.
     */
    void debug_check_self_ref_insert([[maybe_unused]] size_t idx,
                                     [[maybe_unused]] const T* value_ptr) const {
#ifndef NDEBUG
        if (!aliases_this(value_ptr)) {
            return;
        }

        const size_t src = index_of_ptr(value_ptr);

        // Case 1: Reallocation might occur
        if (size_ == mCapacity) {
            FATP_ENFORCE(false,
                "insert: argument aliases this container and reallocation may occur "
                "(debug diagnostic; behavior is QoI, not guaranteed by standard)");
        }

        // Case 2: Insertion at/before source element shifts it before value is consumed
        if (idx <= src) {
            FATP_ENFORCE(false,
                "insert: argument aliases element at/after insertion point; "
                "shifting may invalidate the reference (debug diagnostic)");
        }
#endif
    }

    /**
     * @brief Reallocates storage to accommodate new capacity
     *
     * @details
     * Core reallocation with exception safety via nested ScopeGuards to track resource
     * allocation and element construction. If any operation throws, guards automatically clean up.
     *
     * Exception safety: Strong* (see class documentation). Uses std::move_if_noexcept:
     * - If T is nothrow_move_constructible: moves elements, Strong guarantee
     * - If T is CopyConstructible: copies elements, Strong guarantee  
     * - If T has only throwing move: moves elements, Basic guarantee only
     *
     * Growth strategy: 2x geometric growth for amortized O(1) push_back.
     * Uses CheckedArithmetic to detect capacity overflow safely.
     *
     * @param min_capacity Minimum required capacity after reallocation
     * @throws std::bad_alloc If allocation fails
     * @throws Any exception from T's move/copy constructor
     */
    void grow(size_t min_capacity) {
        // Overflow-safe capacity calculation using CheckedArithmetic
        auto new_cap_result = checked_mul<ReturnExpectedPolicy>(mCapacity, size_t(2));
        
        size_t new_cap;
        if (new_cap_result.has_value()) {
            new_cap = std::max(min_capacity, *new_cap_result);
        } else {
            new_cap = min_capacity;
            FATP_ALWAYS_ENFORCE(new_cap <= max_size(), "Requested capacity exceeds max_size");
        }
        
        // Defensive: ensure non-zero capacity even if overflow arithmetic produces 0
        if (new_cap == 0) {
            new_cap = std::max(min_capacity, size_t(1));
        }
        
        // Allocate new storage
        T* new_data = AllocTraits::allocate(mAllocator, new_cap);
        
        // RAII guard to deallocate on exception
        auto cleanup_guard = makeScopeGuard([&]() noexcept {
            AllocTraits::deallocate(mAllocator, new_data, new_cap);
        });
        
        // Track construction progress for exception rollback
        size_t constructed = 0;
        auto element_guard = makeScopeGuard([&]() noexcept {
            std::destroy_n(new_data, constructed);
        });
        
        // Move-construct elements using move_if_noexcept for strong exception safety.
        // If T's move ctor can throw and T is copyable, we copy instead to preserve
        // the original elements in case of exception.
        for (size_t i = 0; i < size_; ++i) {
            AllocTraits::construct(mAllocator, new_data + i, std::move_if_noexcept(data_[i]));
            ++constructed;
        }
        
        // Success - dismiss guards and commit changes
        element_guard.dismiss();
        cleanup_guard.dismiss();
        
        // Now safe to destroy old elements
        std::destroy_n(data_, size_);
        
        // Deallocate old heap storage if not inline
        if (!is_inline()) {
            AllocTraits::deallocate(mAllocator, data_, mCapacity);
        }
        
        // Update to new heap storage
        data_ = new_data;
        mCapacity = new_cap;
        assert_invariants();
    }

public:
    // Standard container type aliases
    using value_type = T;
    using allocator_type = Allocator;
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
        : data_(inline_ptr()), size_(0), mCapacity(InlineCapacity), mAllocator() {
        assert_invariants();
    }

    /** @brief Constructor with explicit allocator */
    explicit SmallVector(const Allocator& alloc) noexcept
        : data_(inline_ptr()), size_(0), mCapacity(InlineCapacity), mAllocator(alloc) {
        assert_invariants();
    }

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
        : SmallVector(AllocTraits::select_on_container_copy_construction(other.mAllocator)) {
        assign(other.begin(), other.end());
    }

    /** @brief Copy constructor with explicit allocator */
    SmallVector(const SmallVector& other, const Allocator& alloc) : SmallVector(alloc) {
        assign(other.begin(), other.end());
    }

    /**
     * @brief Move constructor
     * @note Steals heap storage in O(1), inline storage requires O(N) element-wise move
     * @note noexcept only if T is nothrow move constructible (for inline path)
     */
    SmallVector(SmallVector&& other) noexcept(std::is_nothrow_move_constructible_v<T>)
        : data_(inline_ptr()), size_(0), mCapacity(InlineCapacity),
          mAllocator(std::move(other.mAllocator)) {
        if (other.is_inline()) {
            // Inline storage requires element-wise move
            std::uninitialized_move_n(other.data_, other.size_, data_);
            size_ = other.size_;
            std::destroy_n(other.data_, other.size_);
        } else {
            // Heap storage: O(1) pointer steal
            data_ = other.data_;
            size_ = other.size_;
            mCapacity = other.mCapacity;
            // Reset other to inline state
            other.data_ = other.inline_ptr();
            other.mCapacity = InlineCapacity;
        }
        other.size_ = 0;
        assert_invariants();
    }

    /**
     * @brief Move constructor with explicit allocator
     * @note If allocators unequal, performs element-wise move instead of pointer steal
     */
    SmallVector(SmallVector&& other, const Allocator& alloc) : SmallVector(alloc) {
        if (mAllocator == other.mAllocator) {
            // Equal allocators: can steal resources
            if (other.is_inline()) {
                std::uninitialized_move_n(other.data_, other.size_, data_);
                size_ = other.size_;
                std::destroy_n(other.data_, other.size_);
            } else {
                data_ = other.data_;
                size_ = other.size_;
                mCapacity = other.mCapacity;
                other.data_ = other.inline_ptr();
                other.mCapacity = InlineCapacity;
            }
            other.size_ = 0;
        } else {
            // Unequal allocators: must perform element-wise move
            assign(std::make_move_iterator(other.begin()), 
                   std::make_move_iterator(other.end()));
        }
        assert_invariants();
    }

    /** @brief Destructor */
    ~SmallVector() noexcept {
        std::destroy_n(data_, size_);
        if (!is_inline()) {
            AllocTraits::deallocate(mAllocator, data_, mCapacity);
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
            if (mAllocator != other.mAllocator) {
                // Allocator will change - must deallocate with old allocator first
                clear();
                if (!is_inline()) {
                    AllocTraits::deallocate(mAllocator, data_, mCapacity);
                    data_ = inline_ptr();
                    mCapacity = InlineCapacity;
                }
                mAllocator = other.mAllocator;
            }
        }
        
        assign(other.begin(), other.end());
        assert_invariants();
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
            AllocTraits::deallocate(mAllocator, data_, mCapacity);
        }
        
        // Reset to inline state
        data_ = inline_ptr();
        mCapacity = InlineCapacity;
        size_ = 0;

        constexpr bool Pocma = AllocTraits::propagate_on_container_move_assignment::value;

        if constexpr (Pocma) {
            // POCMA=true: always propagate allocator and steal resources
            mAllocator = std::move(other.mAllocator);
        } else if (mAllocator != other.mAllocator) {
            // POCMA=false and allocators unequal: element-wise move required
            assign(std::make_move_iterator(other.begin()), 
                   std::make_move_iterator(other.end()));
            other.clear();
            assert_invariants();
            return *this;
        }
        // POCMA=false and allocators equal: can steal without propagation

        // Perform efficient resource transfer
        if (other.is_inline()) {
            std::uninitialized_move_n(other.data_, other.size_, data_);
            std::destroy_n(other.data_, other.size_);
        } else {
            data_ = other.data_;
            mCapacity = other.mCapacity;
            other.data_ = other.inline_ptr();
            other.mCapacity = InlineCapacity;
        }
        size_ = other.size_;

        // Leave other in valid empty state
        other.size_ = 0;
        
        assert_invariants();
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

    /**
     * @brief Replaces contents with count copies of value
     * @note Provides Strong* exception safety (see class documentation)
     */
    void assign(size_type count, const T& value) {
        // Stabilize value before clear() in case it aliases this container
        T stable_value = value;
        
        clear();
        if (count > mCapacity) {
            reserve(count);
        }
        
        // Exception-safe construction with guard
        size_t constructed = 0;
        auto guard = makeScopeGuard([&]() noexcept {
            std::destroy_n(data_, constructed);
            size_ = 0;
        });
        
        for (size_t i = 0; i < count; ++i) {
            AllocTraits::construct(mAllocator, data_ + i, stable_value);
            ++constructed;
        }
        
        guard.dismiss();
        size_ = count;
        assert_invariants();
    }

    /**
     * @brief Replaces contents with elements from range [first, last)
     * @note Supports input iterators (single-pass) as well as forward iterators
     * @note Provides Strong* exception safety for forward+ iterators (see class documentation)
     */
    template <class InputIt, std::enable_if_t<!std::is_integral_v<InputIt>, int> = 0>
    void assign(InputIt first, InputIt last) {
        using IterCategory = typename std::iterator_traits<InputIt>::iterator_category;
        
        if constexpr (std::is_base_of_v<std::forward_iterator_tag, IterCategory>) {
            // Forward+ iterators: can compute distance and traverse twice
            clear();
            auto dist = std::distance(first, last);
            FATP_ENFORCE(dist >= 0, "Negative iterator distance");
            size_t count = static_cast<size_t>(dist);
            if (count > mCapacity) {
                reserve(count);
            }
            
            // Exception-safe construction with guard
            size_t constructed = 0;
            auto guard = makeScopeGuard([&]() noexcept {
                std::destroy_n(data_, constructed);
                size_ = 0;
            });
            
            for (auto it = first; it != last; ++it) {
                AllocTraits::construct(mAllocator, data_ + constructed, *it);
                ++constructed;
            }
            
            guard.dismiss();
            size_ = count;
            assert_invariants();
        } else {
            // Input iterators: single-pass only, cannot call distance()
            clear();
            for (; first != last; ++first) {
                emplace_back(*first);
            }
            assert_invariants();
        }
    }

    /** @brief Replaces contents with elements from initializer list */
    void assign(std::initializer_list<T> ilist) {
        assign(ilist.begin(), ilist.end());
    }

    // ==================================================================================
    // Iterators - HOT PATH - Simple pointer returns, no branching
    // ==================================================================================

    [[nodiscard]] iterator begin() noexcept { return data_; }
    [[nodiscard]] const_iterator begin() const noexcept { return data_; }
    [[nodiscard]] const_iterator cbegin() const noexcept { return data_; }
    
    [[nodiscard]] iterator end() noexcept { return data_ + size_; }
    [[nodiscard]] const_iterator end() const noexcept { return data_ + size_; }
    [[nodiscard]] const_iterator cend() const noexcept { return data_ + size_; }
    
    [[nodiscard]] reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
    [[nodiscard]] const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    [[nodiscard]] const_reverse_iterator crbegin() const noexcept { return rbegin(); }
    
    [[nodiscard]] reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
    [[nodiscard]] const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
    [[nodiscard]] const_reverse_iterator crend() const noexcept { return rend(); }

    // ==================================================================================
    // Capacity and Size
    // ==================================================================================

    /** @brief Returns number of elements */
    [[nodiscard]] size_type size() const noexcept { return size_; }

    /** @brief Returns maximum possible number of elements */
    [[nodiscard]] size_type max_size() const noexcept {
        return AllocTraits::max_size(mAllocator);
    }

    /**
     * @brief Returns current capacity
     * @note For inline storage returns InlineCapacity, for heap returns allocated capacity
     */
    [[nodiscard]] size_type capacity() const noexcept { return mCapacity; }

    /** @brief Checks if container is empty */
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }

    /**
     * @brief Increases capacity to at least new_cap
     * @note Does nothing if new_cap <= current capacity
     * @throws std::bad_alloc or exception from T's move constructor
     */
    void reserve(size_type new_cap) {
        if (new_cap <= mCapacity) {
            return;
        }
        grow(new_cap);
    }

    /**
     * @brief Reduces capacity to fit current size or InlineCapacity
     * @note Transitions from heap to inline storage if size <= InlineCapacity
     * @note Shrinks heap allocation if size < capacity and size > InlineCapacity
     * @note Provides Strong* exception safety (see class documentation)
     */
    void shrink_to_fit() {
        if (is_inline()) {
            return;
        }
        
        if (size_ <= InlineCapacity) {
            // Demote from heap to inline storage
            T* old_data = data_;
            size_t old_capacity = mCapacity;
            T* new_inline = inline_ptr();
            
            // Track construction progress for exception rollback
            size_t constructed = 0;
            auto element_guard = makeScopeGuard([&]() noexcept {
                // If we fail, destroy any partially constructed inline elements
                std::destroy_n(new_inline, constructed);
                // State unchanged: data_ still points to heap, which is intact
            });
            
            // Move elements to inline buffer (may throw)
            for (size_t i = 0; i < size_; ++i) {
                AllocTraits::construct(mAllocator, new_inline + i, 
                                       std::move_if_noexcept(old_data[i]));
                ++constructed;
            }
            
            // Success - dismiss guard and commit
            element_guard.dismiss();
            
            // Now safe to update state and cleanup
            data_ = new_inline;
            mCapacity = InlineCapacity;
            std::destroy_n(old_data, size_);
            AllocTraits::deallocate(mAllocator, old_data, old_capacity);
        } else if (size_ < mCapacity) {
            // Shrink heap allocation to exactly size_
            T* new_data = AllocTraits::allocate(mAllocator, size_);
            
            auto cleanup_guard = makeScopeGuard([&]() noexcept {
                AllocTraits::deallocate(mAllocator, new_data, size_);
            });
            
            size_t constructed = 0;
            auto element_guard = makeScopeGuard([&]() noexcept {
                std::destroy_n(new_data, constructed);
            });
            
            for (size_t i = 0; i < size_; ++i) {
                AllocTraits::construct(mAllocator, new_data + i, 
                                       std::move_if_noexcept(data_[i]));
                ++constructed;
            }
            
            element_guard.dismiss();
            cleanup_guard.dismiss();
            
            std::destroy_n(data_, size_);
            AllocTraits::deallocate(mAllocator, data_, mCapacity);
            
            data_ = new_data;
            mCapacity = size_;
        }
        assert_invariants();
    }

    // ==================================================================================
    // Element Access
    // ==================================================================================

    /**
     * @brief Access element with bounds checking
     * @throws always_enforce exception if pos >= size()
     */
    [[nodiscard]] reference at(size_type pos) {
        FATP_ALWAYS_ENFORCE(pos < size_, "Index ", pos, " out of bounds (size=", size_, ")");
        return data_[pos];
    }

    /** @brief Access element with bounds checking (const) */
    [[nodiscard]] const_reference at(size_type pos) const {
        FATP_ALWAYS_ENFORCE(pos < size_, "Index ", pos, " out of bounds (size=", size_, ")");
        return data_[pos];
    }

    /**
     * @brief Access element without bounds checking
     * @warning Undefined behavior if pos >= size() in release builds
     * @note Debug builds check bounds via FATP_ENFORCE()
     */
    [[nodiscard]] reference operator[](size_type pos) {
        FATP_ENFORCE(pos < size_, "Index out of bounds");
        return data_[pos];
    }

    /** @brief Access element without bounds checking (const) */
    [[nodiscard]] const_reference operator[](size_type pos) const {
        FATP_ENFORCE(pos < size_, "Index out of bounds");
        return data_[pos];
    }

    /** @brief Access first element */
    [[nodiscard]] reference front() {
        FATP_ENFORCE(size_ > 0, "Cannot access front of empty vector");
        return data_[0];
    }

    /** @brief Access first element (const) */
    [[nodiscard]] const_reference front() const {
        FATP_ENFORCE(size_ > 0, "Cannot access front of empty vector");
        return data_[0];
    }

    /** @brief Access last element */
    [[nodiscard]] reference back() {
        FATP_ENFORCE(size_ > 0, "Cannot access back of empty vector");
        return data_[size_ - 1];
    }

    /** @brief Access last element (const) */
    [[nodiscard]] const_reference back() const {
        FATP_ENFORCE(size_ > 0, "Cannot access back of empty vector");
        return data_[size_ - 1];
    }

    /**
     * @brief Returns pointer to underlying element storage
     * @note Pointer valid until reallocation. May point to inline or heap storage.
     */
    [[nodiscard]] T* data() noexcept { return data_; }

    /** @brief Returns const pointer to underlying element storage */
    [[nodiscard]] const T* data() const noexcept { return data_; }
    
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
        assert_invariants();
    }

    /** @brief Inserts copy of value before pos */
    iterator insert(const_iterator pos, const T& value) {
        size_t idx = static_cast<size_t>(pos - data_);
        debug_check_self_ref_insert(idx, std::addressof(value));
        return emplace(pos, value);
    }

    /** @brief Inserts value by move before pos */
    iterator insert(const_iterator pos, T&& value) {
        size_t idx = static_cast<size_t>(pos - data_);
        debug_check_self_ref_insert(idx, std::addressof(value));
        return emplace(pos, std::move(value));
    }

    /**
     * @brief Inserts count copies of value before pos
     * @note Requires T to be CopyInsertable and CopyAssignable (per [sequence.reqmts])
     * @note In-place path (no reallocation) uses assignment into moved-from elements
     * @note Provides Strong* exception safety with reallocation; Basic without
     *       (see class documentation for Strong* condition)
     * @throws std::bad_alloc or exception from T's copy constructor/assignment
     */
    iterator insert(const_iterator pos, size_type count, const T& value) {
        size_t idx = static_cast<size_t>(pos - data_);
        FATP_ENFORCE(idx <= size_, "Insert position out of range");
        debug_check_self_ref_insert(idx, std::addressof(value));
        
        if (count == 0) {
            return data_ + idx;
        }
        
        // Verify new size won't overflow
        auto new_size_result = checked_add<ReturnExpectedPolicy>(size_, count);
        FATP_ALWAYS_ENFORCE(new_size_result.has_value(), "Insert would exceed max_size");
        size_t new_size = *new_size_result;
        
        if (new_size > mCapacity) {
            // Growth path with STRONG exception safety
            auto new_cap_result = checked_mul<ReturnExpectedPolicy>(mCapacity, size_t(2));
            size_t new_cap = new_cap_result.has_value() ? 
                std::max(new_size, *new_cap_result) : new_size;
            
            T* new_data = AllocTraits::allocate(mAllocator, new_cap);
            
            auto cleanup_guard = makeScopeGuard([&]() noexcept {
                AllocTraits::deallocate(mAllocator, new_data, new_cap);
            });
            
            // Insert count copies FIRST while old data_ is still valid
            // This handles aliasing (e.g., v.insert(pos, 5, v[0])) - no stabilization
            // copy needed since all constructions from value complete before any moves
            size_t inserted_constructed = 0;
            auto inserted_guard = makeScopeGuard([&]() noexcept {
                std::destroy_n(new_data + idx, inserted_constructed);
            });
            
            for (size_t i = 0; i < count; ++i) {
                AllocTraits::construct(mAllocator, new_data + idx + i, value);
                ++inserted_constructed;
            }
            
            // Move prefix [0, idx) to new_data[0, idx)
            size_t prefix_constructed = 0;
            auto prefix_guard = makeScopeGuard([&]() noexcept {
                std::destroy_n(new_data, prefix_constructed);
            });
            
            for (size_t i = 0; i < idx; ++i) {
                AllocTraits::construct(mAllocator, new_data + i, 
                                       std::move_if_noexcept(data_[i]));
                ++prefix_constructed;
            }
            
            // Move suffix [idx, size_) to new_data[idx+count, ...)
            size_t suffix_constructed = 0;
            auto suffix_guard = makeScopeGuard([&]() noexcept {
                std::destroy_n(new_data + idx + count, suffix_constructed);
            });
            
            for (size_t i = idx; i < size_; ++i) {
                AllocTraits::construct(mAllocator, new_data + i + count, 
                                       std::move_if_noexcept(data_[i]));
                ++suffix_constructed;
            }
            
            // Success - dismiss all guards
            suffix_guard.dismiss();
            prefix_guard.dismiss();
            inserted_guard.dismiss();
            cleanup_guard.dismiss();
            
            // Clean up old storage and commit
            std::destroy_n(data_, size_);
            if (!is_inline()) {
                AllocTraits::deallocate(mAllocator, data_, mCapacity);
            }
            
            data_ = new_data;
            mCapacity = new_cap;
            size_ = new_size;
            assert_invariants();
            return data_ + idx;
        }
        
        // In-place insertion without reallocation
        // Stabilize value in case it aliases an element that will be moved
        T stable_value = value;
        iterator insert_pos = data_ + idx;
        
        if (idx < size_) {
            size_t tail = size_ - idx;
            // Two cases: tail fits entirely in uninit space, or tail extends beyond insertion
            if (tail <= count) {
                // Move existing tail elements to their new positions (in uninitialized space)
                size_t moved_tail = 0;
                auto moved_tail_guard = makeScopeGuard([&]() noexcept {
                    std::destroy_n(insert_pos + count, moved_tail);
                });
                for (size_t i = 0; i < tail; ++i) {
                    AllocTraits::construct(mAllocator,
                                           insert_pos + count + i,
                                           std::move_if_noexcept(insert_pos[i]));
                    ++moved_tail;
                }
                // First 'tail' positions are moved-from but constructed - use assignment
                std::fill_n(insert_pos, tail, stable_value);
                // Remaining 'count - tail' positions are uninitialized - use placement new
                size_t filled = 0;
                auto fill_guard = makeScopeGuard([&]() noexcept {
                    std::destroy_n(insert_pos + tail, filled);
                });
                for (size_t i = 0; i < (count - tail); ++i) {
                    AllocTraits::construct(mAllocator, insert_pos + tail + i, stable_value);
                    ++filled;
                }
                fill_guard.dismiss();
                moved_tail_guard.dismiss();
            } else {
                // First create 'count' new elements at the end in uninitialized storage
                size_t moved_suffix = 0;
                auto moved_suffix_guard = makeScopeGuard([&]() noexcept {
                    std::destroy_n(end(), moved_suffix);
                });
                for (size_t i = 0; i < count; ++i) {
                    AllocTraits::construct(mAllocator,
                                           end() + i,
                                           std::move_if_noexcept(*(end() - count + i)));
                    ++moved_suffix;
                }
                std::move_backward(insert_pos, end() - count, end());
                std::fill_n(insert_pos, count, stable_value);
                moved_suffix_guard.dismiss();
            }
        } else {
            // Inserting at end into uninitialized space
            size_t filled = 0;
            auto fill_guard = makeScopeGuard([&]() noexcept {
                std::destroy_n(insert_pos, filled);
            });
            for (size_t i = 0; i < count; ++i) {
                AllocTraits::construct(mAllocator, insert_pos + i, stable_value);
                ++filled;
            }
            fill_guard.dismiss();
        }
        
        size_ = new_size;
        assert_invariants();
        return insert_pos;
    }

    /**
     * @brief Inserts elements from range [first, last) before pos
     * @note Provides Strong* exception safety with realloc for forward+ iters;
     *       Basic for input iters or in-place insertion (see class documentation)
     */
    template <class InputIt, std::enable_if_t<!std::is_integral_v<InputIt>, int> = 0>
    iterator insert(const_iterator pos, InputIt first, InputIt last) {
        size_t idx = static_cast<size_t>(pos - data_);
        FATP_ENFORCE(idx <= size_, "Insert position out of range");
        
        using IterCategory = typename std::iterator_traits<InputIt>::iterator_category;
        return insert_range_impl(idx, first, last, IterCategory{});
    }

private:
    /**
     * @brief Input iterator insert implementation
     * @note For single-pass iterators, we append then rotate into position
     */
    template <class InputIt>
    iterator insert_range_impl(size_t idx, InputIt first, InputIt last, std::input_iterator_tag) {
        if (first == last) {
            return data_ + idx;
        }
        
        // For input iterators, we can't know count upfront.
        // Strategy: append all elements to end, then rotate into position.
        size_t original_size = size_;
        
        // Append elements
        for (; first != last; ++first) {
            emplace_back(*first);
        }
        
        // Rotate the appended elements into position
        // [0..idx)[idx..original_size)[original_size..size_)
        //    ^           ^                    ^
        //  prefix    existing tail      newly inserted
        // After rotate: [0..idx)[newly inserted)[existing tail)
        std::rotate(data_ + idx, data_ + original_size, data_ + size_);
        
        assert_invariants();
        return data_ + idx;
    }
    
    /**
     * @brief Forward+ iterator insert implementation
     * @note Can compute distance and use optimized paths
     */
    template <class ForwardIt>
    iterator insert_range_impl(size_t idx,
                               ForwardIt first,
                               ForwardIt last,
                               std::forward_iterator_tag) {
        auto dist = std::distance(first, last);
        FATP_ENFORCE(dist >= 0, "Negative iterator distance");
        size_t count = static_cast<size_t>(dist);
        if (count == 0) {
            return data_ + idx;
        }
        
        auto new_size_result = checked_add<ReturnExpectedPolicy>(size_, count);
        FATP_ALWAYS_ENFORCE(new_size_result.has_value(), "Insert would exceed max_size");
        size_t new_size = *new_size_result;
        
        if (new_size > mCapacity) {
            // Growth path with STRONG exception safety
            // Must handle self-referential insertion (iterators into *this)
            auto new_cap_result = checked_mul<ReturnExpectedPolicy>(mCapacity, size_t(2));
            size_t new_cap = new_cap_result.has_value() ? 
                std::max(new_size, *new_cap_result) : new_size;
            
            T* new_data = AllocTraits::allocate(mAllocator, new_cap);
            
            auto cleanup_guard = makeScopeGuard([&]() noexcept {
                AllocTraits::deallocate(mAllocator, new_data, new_cap);
            });
            
            // FIX #1: Use separate guards for each disjoint region
            // Previously: single guard destroyed [0, constructed) but elements were at
            // different locations. Now: each region has its own guard.
            
            // 1. Construct the INSERTED elements first (at offset idx)
            // This handles aliasing if first/last point into the old buffer
            size_t constructed_inserted = 0;
            auto inserted_guard = makeScopeGuard([&]() noexcept {
                std::destroy_n(new_data + idx, constructed_inserted);
            });
            
            for (auto it = first; it != last; ++it) {
                AllocTraits::construct(mAllocator, new_data + idx + constructed_inserted, *it);
                ++constructed_inserted;
            }
            
            // 2. Move prefix [0, idx)
            size_t constructed_prefix = 0;
            auto prefix_guard = makeScopeGuard([&]() noexcept {
                std::destroy_n(new_data, constructed_prefix);
            });
            
            for (size_t i = 0; i < idx; ++i) {
                AllocTraits::construct(mAllocator, new_data + i, 
                                       std::move_if_noexcept(data_[i]));
                ++constructed_prefix;
            }
            
            // 3. Move suffix [idx, size_) to [idx + count, ...)
            size_t constructed_suffix = 0;
            auto suffix_guard = makeScopeGuard([&]() noexcept {
                std::destroy_n(new_data + idx + count, constructed_suffix);
            });
            
            for (size_t i = idx; i < size_; ++i) {
                AllocTraits::construct(mAllocator, new_data + i + count, 
                                       std::move_if_noexcept(data_[i]));
                ++constructed_suffix;
            }
            
            // Success - dismiss all guards
            suffix_guard.dismiss();
            prefix_guard.dismiss();
            inserted_guard.dismiss();
            cleanup_guard.dismiss();
            
            std::destroy_n(data_, size_);
            if (!is_inline()) {
                AllocTraits::deallocate(mAllocator, data_, mCapacity);
            }
            
            data_ = new_data;
            mCapacity = new_cap;
            size_ = new_size;
            assert_invariants();
            return data_ + idx;
        }
        
        // In-place insertion must handle self-referential ranges.
        // If [first, last) aliases this container, move_backward can mutate the
        // source range before std::copy reads from it. std::copy is not overlap-safe.
        // 
        // Detection: Check if first's underlying pointer is within [data_, data_+size_).
        // This only applies to pointer-like iterators (raw pointers, our own iterator type).
        // For wrapped iterators like move_iterator, we can't reliably detect aliasing,
        // but that's acceptable since the user explicitly requested move semantics.
        //
        // Fix: Materialize the input range to a temporary buffer, then insert from that.
        if constexpr (std::is_pointer_v<ForwardIt> || 
                      std::is_same_v<ForwardIt, iterator> ||
                      std::is_same_v<ForwardIt, const_iterator>) {
            // Get the address of the first element (if range is non-empty)
            const T* first_addr = std::addressof(*first);
            const bool aliases_this = (first_addr >= data_) && (first_addr < data_ + size_);
            
            if (aliases_this) {
                // Materialize to temp buffer to break aliasing
                T* tmp = AllocTraits::allocate(mAllocator, count);
                size_t constructed = 0;
                auto tmp_guard = makeScopeGuard([&]() noexcept {
                    std::destroy_n(tmp, constructed);
                    AllocTraits::deallocate(mAllocator, tmp, count);
                });
                
                for (auto it = first; it != last; ++it) {
                    AllocTraits::construct(mAllocator, tmp + constructed, *it);
                    ++constructed;
                }
                
                // Recurse with non-aliasing pointer range
                // tmp_guard will clean up after recursive call returns
                iterator result = insert_range_impl(idx, tmp, tmp + count, std::forward_iterator_tag{});
                
                // Success - destroy temp (guard handles this)
                return result;
            }
        }
        
        // In-place insertion (non-aliasing case)
        iterator insert_pos = data_ + idx;
        
        if (idx < size_) {
            size_t tail = size_ - idx;
            if (tail <= count) {
                // Move existing tail elements to their new positions (in uninitialized space)
                size_t moved_tail = 0;
                auto moved_tail_guard = makeScopeGuard([&]() noexcept {
                    std::destroy_n(insert_pos + count, moved_tail);
                });
                for (size_t i = 0; i < tail; ++i) {
                    AllocTraits::construct(mAllocator,
                                           insert_pos + count + i,
                                           std::move_if_noexcept(insert_pos[i]));
                    ++moved_tail;
                }
                // Split the input range at 'tail' elements
                auto mid = first;
                std::advance(mid, tail);
                // First 'tail' positions are moved-from but constructed - use copy assignment
                std::copy(first, mid, insert_pos);
                // Remaining 'count - tail' positions are uninitialized - use placement new
                size_t copied = 0;
                auto copy_guard = makeScopeGuard([&]() noexcept {
                    std::destroy_n(insert_pos + tail, copied);
                });
                for (auto it = mid; it != last; ++it) {
                    AllocTraits::construct(mAllocator, insert_pos + tail + copied, *it);
                    ++copied;
                }
                copy_guard.dismiss();
                moved_tail_guard.dismiss();
            } else {
                size_t moved_suffix = 0;
                auto moved_suffix_guard = makeScopeGuard([&]() noexcept {
                    std::destroy_n(end(), moved_suffix);
                });
                for (size_t i = 0; i < count; ++i) {
                    AllocTraits::construct(mAllocator,
                                           end() + i,
                                           std::move_if_noexcept(*(end() - count + i)));
                    ++moved_suffix;
                }
                std::move_backward(insert_pos, end() - count, end());
                std::copy(first, last, insert_pos);
                moved_suffix_guard.dismiss();
            }
        } else {
            size_t copied = 0;
            auto copy_guard = makeScopeGuard([&]() noexcept {
                std::destroy_n(insert_pos, copied);
            });
            for (auto it = first; it != last; ++it) {
                AllocTraits::construct(mAllocator, insert_pos + copied, *it);
                ++copied;
            }
            copy_guard.dismiss();
        }
        
        size_ = new_size;
        assert_invariants();
        return insert_pos;
    }

public:

    /** @brief Inserts elements from initializer list before pos */
    iterator insert(const_iterator pos, std::initializer_list<T> ilist) {
        return insert(pos, ilist.begin(), ilist.end());
    }

    /**
     * @brief Constructs element in-place before pos
     *
     * @details
     * Provides Strong* exception safety during reallocation (see class documentation),
     * Basic guarantee for in-place construction. For trivially moveable and destructible
     * types, uses memmove optimization to shift elements efficiently.
     *
     * @param pos Iterator before which element will be constructed
     * @param args Arguments forwarded to T's constructor
     * @return Iterator pointing to constructed element
     * @throws Exception from T's constructor or move constructor
     */
    template <class... Args>
    iterator emplace(const_iterator pos, Args&&... args) {
        size_t idx = static_cast<size_t>(pos - data_);
        FATP_ENFORCE(idx <= size_, "Emplace position out of range");
        
        if (size_ >= mCapacity) {
            // Growth path with strong exception safety
            auto new_cap_result = checked_mul<ReturnExpectedPolicy>(mCapacity, size_t(2));
            size_t new_cap;
            if (new_cap_result.has_value()) {
                new_cap = std::max(size_ + 1, *new_cap_result);
            } else {
                new_cap = size_ + 1;
                FATP_ALWAYS_ENFORCE(new_cap <= max_size(), "Capacity overflow");
            }
            // Defensive: ensure non-zero capacity even if overflow arithmetic produces 0
            if (new_cap == 0) {
                new_cap = 1;
            }
            
            T* new_data = AllocTraits::allocate(mAllocator, new_cap);
            
            auto cleanup_guard = makeScopeGuard([&]() noexcept {
                AllocTraits::deallocate(mAllocator, new_data, new_cap);
            });
            
            // Construct new element FIRST while old data_ is still valid
            // This handles aliasing (e.g., v.emplace(pos, v[0]))
            AllocTraits::construct(mAllocator, new_data + idx, std::forward<Args>(args)...);
            
            auto new_elem_guard = makeScopeGuard([&]() noexcept {
                AllocTraits::destroy(mAllocator, new_data + idx);
            });
            
            // Move prefix [0, idx) to new_data[0, idx)
            size_t prefix_constructed = 0;
            auto prefix_guard = makeScopeGuard([&]() noexcept {
                std::destroy_n(new_data, prefix_constructed);
            });
            
            for (size_t i = 0; i < idx; ++i) {
                AllocTraits::construct(mAllocator, new_data + i, 
                                       std::move_if_noexcept(data_[i]));
                ++prefix_constructed;
            }
            
            // Move suffix [idx, size_) to new_data[idx+1, size_+1)
            size_t suffix_constructed = 0;
            auto suffix_guard = makeScopeGuard([&]() noexcept {
                std::destroy_n(new_data + idx + 1, suffix_constructed);
            });
            
            for (size_t i = idx; i < size_; ++i) {
                AllocTraits::construct(mAllocator, new_data + i + 1, 
                                       std::move_if_noexcept(data_[i]));
                ++suffix_constructed;
            }
            
            // Success - dismiss all guards
            suffix_guard.dismiss();
            prefix_guard.dismiss();
            new_elem_guard.dismiss();
            cleanup_guard.dismiss();
            
            // Cleanup old storage
            std::destroy_n(data_, size_);
            if (!is_inline()) {
                AllocTraits::deallocate(mAllocator, data_, mCapacity);
            }
            
            data_ = new_data;
            mCapacity = new_cap;
            ++size_;
            assert_invariants();
            return data_ + idx;
        }
        
        // Non-growing path: size_ < mCapacity
        iterator insert_pos = data_ + idx;
        
        if (idx < size_) {
            // HPC optimization: use memmove for trivially copyable types
            // Note: is_trivially_copyable is the only portable, standard-blessed
            // condition that guarantees memcpy/memmove safety (implies trivial
            // copy/move ctors, assignment ops, and destructor)
            if constexpr (std::is_trivially_copyable_v<T>) {
                size_t tail_len_bytes = (size_ - idx) * sizeof(T);
                std::memmove(insert_pos + 1, insert_pos, tail_len_bytes);
                AllocTraits::construct(mAllocator, insert_pos, std::forward<Args>(args)...);
            } else {
                // Standard path for non-trivial types
                // Move last element into uninitialized space at end
                AllocTraits::construct(mAllocator, end(), std::move(*(end() - 1)));
                // Guard: if shifting or assignment throws, destroy the element we just
                // constructed beyond size_ to maintain invariant
                auto tail_guard = makeScopeGuard([&]() noexcept {
                    std::destroy_at(end());
                });
                // Shift elements backward
                std::move_backward(insert_pos, end() - 1, end());
                // Assign to the moved-from slot (NOT destroy+construct - that breaks
                // exception safety if construction throws)
                *insert_pos = T(std::forward<Args>(args)...);
                tail_guard.dismiss();
            }
        } else {
            // Inserting at end
            AllocTraits::construct(mAllocator, insert_pos, std::forward<Args>(args)...);
        }
        
        ++size_;
        assert_invariants();
        return insert_pos;
    }

    /**
     * @brief Erases element at pos
     * @param pos Iterator to element to erase
     * @return Iterator to element following erased element
     */
    iterator erase(const_iterator pos) {
        size_t idx = static_cast<size_t>(pos - data_);
        FATP_ENFORCE(idx < size_, "Erase position out of range");
        
        iterator it = data_ + idx;
        std::move(it + 1, end(), it);
        std::destroy_at(end() - 1);
        --size_;
        assert_invariants();
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
        
        size_t first_idx = static_cast<size_t>(first - data_);
        size_t last_idx = static_cast<size_t>(last - data_);
        FATP_ENFORCE(first_idx <= last_idx && last_idx <= size_, "Erase range invalid");
        
        iterator f = data_ + first_idx;
        iterator l = data_ + last_idx;
        std::move(l, end(), f);
        size_t count = last_idx - first_idx;
        std::destroy_n(end() - count, count);
        size_ -= count;
        assert_invariants();
        return f;
    }

    /** @brief Appends copy of value */
    void push_back(const T& value) {
        debug_check_self_ref_push_back(std::addressof(value));
        (void)emplace_back(value);
    }

    /** @brief Appends value by move */
    void push_back(T&& value) {
        debug_check_self_ref_push_back(std::addressof(value));
        (void)emplace_back(std::move(value));
    }

    /**
     * @brief Constructs element in-place at end
     * @return Reference to newly constructed element
     * @note Strong exception safety without reallocation; Strong* with reallocation
     *       (see class documentation)
     * @note Handles self-referential insertion (e.g., v.push_back(v[0]))
     */
    template <class... Args>
    reference emplace_back(Args&&... args) {
        if (FATP_LIKELY(size_ < mCapacity)) {
            // Fast path: no reallocation needed
            AllocTraits::construct(mAllocator, data_ + size_, std::forward<Args>(args)...);
            ++size_;
            assert_invariants();
            return data_[size_ - 1];
        }
        
        // Slow path: reallocation required
        // Must construct new element BEFORE moving old elements to handle
        // self-referential insertion (e.g., v.push_back(v[0]))
        auto new_cap_result = checked_mul<ReturnExpectedPolicy>(mCapacity, size_t(2));
        size_t new_cap;
        if (new_cap_result.has_value()) {
            new_cap = std::max(size_ + 1, *new_cap_result);
        } else {
            new_cap = size_ + 1;
            FATP_ALWAYS_ENFORCE(new_cap <= max_size(), "Capacity overflow");
        }
        // Defensive: ensure non-zero capacity even if overflow arithmetic produces 0
        if (new_cap == 0) {
            new_cap = 1;
        }
        
        T* new_data = AllocTraits::allocate(mAllocator, new_cap);
        
        auto cleanup_guard = makeScopeGuard([&]() noexcept {
            AllocTraits::deallocate(mAllocator, new_data, new_cap);
        });
        
        // Construct new element FIRST while old data_ is still valid
        AllocTraits::construct(mAllocator, new_data + size_, std::forward<Args>(args)...);
        
        auto new_elem_guard = makeScopeGuard([&]() noexcept {
            AllocTraits::destroy(mAllocator, new_data + size_);
        });
        
        // Now move existing elements
        size_t constructed = 0;
        auto element_guard = makeScopeGuard([&]() noexcept {
            std::destroy_n(new_data, constructed);
        });
        
        for (size_t i = 0; i < size_; ++i) {
            AllocTraits::construct(mAllocator, new_data + i, 
                                   std::move_if_noexcept(data_[i]));
            ++constructed;
        }
        
        // Success - commit
        element_guard.dismiss();
        new_elem_guard.dismiss();
        cleanup_guard.dismiss();
        
        std::destroy_n(data_, size_);
        if (!is_inline()) {
            AllocTraits::deallocate(mAllocator, data_, mCapacity);
        }
        
        data_ = new_data;
        mCapacity = new_cap;
        ++size_;
        assert_invariants();
        return data_[size_ - 1];
    }

    /** @brief Removes last element */
    void pop_back() {
        FATP_ALWAYS_ENFORCE(size_ > 0, "Cannot pop from empty vector");
        --size_;
        std::destroy_at(data_ + size_);
        assert_invariants();
    }

    /**
     * @brief Resizes container to count elements
     * @note Requires T to be DefaultInsertable (default-constructible via allocator)
     * @note If count > size, default-constructs new elements. If count < size, destroys excess.
     * @note Strong* exception safety if reallocation needed (see class documentation)
     */
    void resize(size_type count) {
        if (count < size_) {
            std::destroy_n(data_ + count, size_ - count);
        } else if (count > size_) {
            if (count > mCapacity) {
                reserve(count);
            }
            // FIX #2: Guard partial construction to prevent leaks on exception
            size_t constructed = 0;
            auto guard = makeScopeGuard([&]() noexcept {
                std::destroy_n(data_ + size_, constructed);
            });
            
            for (size_t i = size_; i < count; ++i) {
                AllocTraits::construct(mAllocator, data_ + i);
                ++constructed;
            }
            
            guard.dismiss();
        }
        size_ = count;
        assert_invariants();
    }

    /** 
     * @brief Resizes container, initializing new elements with value 
     * @note Requires T to be CopyInsertable
     * @note Strong* exception safety if reallocation needed (see class documentation)
     */
    void resize(size_type count, const T& value) {
        if (count < size_) {
            std::destroy_n(data_ + count, size_ - count);
            size_ = count;
            assert_invariants();
        } else if (count > size_) {
            // Stabilize value before potential reallocation
            T stable_value = value;
            
            if (count > mCapacity) {
                reserve(count);
            }
            // FIX #2: Guard partial construction to prevent leaks on exception
            size_t constructed = 0;
            auto guard = makeScopeGuard([&]() noexcept {
                std::destroy_n(data_ + size_, constructed);
            });
            
            for (size_t i = size_; i < count; ++i) {
                AllocTraits::construct(mAllocator, data_ + i, stable_value);
                ++constructed;
            }
            
            guard.dismiss();
            size_ = count;
            assert_invariants();
        }
    }

    /**
     * @brief Swaps contents with another SmallVector
     * @note Implements POCS (Propagate On Container Swap) semantics
     * @note Exception safety: noexcept if moves are noexcept; Basic guarantee otherwise
     *       (containers remain valid, no resource leaks, but may be partially swapped)
     * @warning If POCS is false, allocators must be equal (undefined behavior otherwise)
     */
    void swap(SmallVector& other) noexcept(
        (AllocTraits::is_always_equal::value || 
         AllocTraits::propagate_on_container_swap::value) &&
        std::is_nothrow_move_constructible_v<T> &&
        std::is_nothrow_swappable_v<T>) {
        if (this == &other) {
            return;
        }
        
        constexpr bool Pocs = AllocTraits::propagate_on_container_swap::value;
        
        if constexpr (Pocs) {
            using std::swap;
            swap(mAllocator, other.mAllocator);
        } else {
            FATP_ALWAYS_ENFORCE(mAllocator == other.mAllocator, 
                          "Cannot swap containers with unequal allocators when POCS is false");
        }
        
        // Both inline: element-wise swap
        if (is_inline() && other.is_inline()) {
            size_t min_size = std::min(size_, other.size_);
            
            // Swap common elements
            for (size_t i = 0; i < min_size; ++i) {
                using std::swap;
                swap(data_[i], other.data_[i]);
            }
            
            // Move excess elements
            if (size_ > other.size_) {
                size_t constructed = 0;
                auto guard = makeScopeGuard([&]() noexcept {
                    std::destroy_n(other.data_ + min_size, constructed);
                });
                for (size_t i = 0; i < (size_ - min_size); ++i) {
                    AllocTraits::construct(other.mAllocator,
                                           other.data_ + min_size + i,
                                           std::move_if_noexcept(data_[min_size + i]));
                    ++constructed;
                }
                guard.dismiss();
                std::destroy_n(data_ + min_size, size_ - min_size);
            } else if (other.size_ > size_) {
                size_t constructed = 0;
                auto guard = makeScopeGuard([&]() noexcept {
                    std::destroy_n(data_ + min_size, constructed);
                });
                for (size_t i = 0; i < (other.size_ - min_size); ++i) {
                    AllocTraits::construct(mAllocator,
                                           data_ + min_size + i,
                                           std::move_if_noexcept(other.data_[min_size + i]));
                    ++constructed;
                }
                guard.dismiss();
                std::destroy_n(other.data_ + min_size, other.size_ - min_size);
            }
            
            using std::swap;
            swap(size_, other.size_);
            assert_invariants();
            other.assert_invariants();
            return;
        }
        
        // Both heap: just swap pointers
        if (!is_inline() && !other.is_inline()) {
            using std::swap;
            swap(data_, other.data_);
            swap(size_, other.size_);
            swap(mCapacity, other.mCapacity);
            assert_invariants();
            other.assert_invariants();
            return;
        }
        
        // FIX #3: One inline, one heap - with proper exception safety
        // Previously: state was mutated before potentially-throwing move, causing
        // heap leak and corruption if move threw. Now: move first, then mutate state.
        SmallVector* inline_vec = is_inline() ? this : &other;
        SmallVector* heap_vec = is_inline() ? &other : this;
        
        // Save heap info
        T* heap_data = heap_vec->data_;
        size_t heap_size = heap_vec->size_;
        size_t heap_cap = heap_vec->mCapacity;
        size_t inline_size = inline_vec->size_;
        
        // Rollback guard restores heap_vec state if move throws
        auto rollback = makeScopeGuard([&]() noexcept {
            heap_vec->data_ = heap_data;
            heap_vec->size_ = heap_size;
            heap_vec->mCapacity = heap_cap;
        });
        
        // Move inline elements to heap_vec's inline buffer FIRST (may throw).
        // Must not leave partially-constructed elements beyond heap_vec->size_ on exception.
        size_t constructed = 0;
        auto inline_construct_guard = makeScopeGuard([&]() noexcept {
            std::destroy_n(heap_vec->inline_ptr(), constructed);
        });
        for (size_t i = 0; i < inline_size; ++i) {
            AllocTraits::construct(heap_vec->mAllocator,
                                   heap_vec->inline_ptr() + i,
                                   std::move_if_noexcept(inline_vec->data_[i]));
            ++constructed;
        }
        
        // Now safe to mutate state
        heap_vec->data_ = heap_vec->inline_ptr();
        heap_vec->mCapacity = InlineCapacity;
        heap_vec->size_ = inline_size;
        inline_construct_guard.dismiss();
        
        // Destroy inline elements and give heap to inline_vec
        std::destroy_n(inline_vec->data_, inline_vec->size_);
        inline_vec->data_ = heap_data;
        inline_vec->size_ = heap_size;
        inline_vec->mCapacity = heap_cap;
        
        rollback.dismiss();
        assert_invariants();
        other.assert_invariants();
    }

    /** @brief Returns copy of allocator */
    [[nodiscard]] Allocator get_allocator() const noexcept {
        return mAllocator;
    }

#if FATP_HAS_CPP20
    /**
     * @brief Checks if container contains an element equal to value
     * @param value Value to search for
     * @return true if found, false otherwise
     * @note C++20 feature
     */
    [[nodiscard]] bool contains(const T& value) const {
        return std::find(begin(), end(), value) != end();
    }
#endif
};

// ==================================================================================
// Non-member swap (ADL)
// ==================================================================================

template <class T, size_t N, class Alloc>
void swap(SmallVector<T, N, Alloc>& lhs, SmallVector<T, N, Alloc>& rhs)
    noexcept(noexcept(lhs.swap(rhs))) {
    lhs.swap(rhs);
}

// ==================================================================================
// Non-member Comparison Operators (Cross-Capacity)
// ==================================================================================

// These operators allow comparison between SmallVectors with different InlineCapacity.
// E.g., SmallVector<int,8> can be compared with SmallVector<int,16>.

template <class T, size_t N1, size_t N2, class Alloc>
bool operator==(const SmallVector<T, N1, Alloc>& lhs, const SmallVector<T, N2, Alloc>& rhs) {
    return lhs.size() == rhs.size() && std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

template <class T, size_t N1, size_t N2, class Alloc>
bool operator!=(const SmallVector<T, N1, Alloc>& lhs, const SmallVector<T, N2, Alloc>& rhs) {
    return !(lhs == rhs);
}

template <class T, size_t N1, size_t N2, class Alloc>
bool operator<(const SmallVector<T, N1, Alloc>& lhs, const SmallVector<T, N2, Alloc>& rhs) {
    return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

template <class T, size_t N1, size_t N2, class Alloc>
bool operator<=(const SmallVector<T, N1, Alloc>& lhs, const SmallVector<T, N2, Alloc>& rhs) {
    return !(rhs < lhs);
}

template <class T, size_t N1, size_t N2, class Alloc>
bool operator>(const SmallVector<T, N1, Alloc>& lhs, const SmallVector<T, N2, Alloc>& rhs) {
    return rhs < lhs;
}

template <class T, size_t N1, size_t N2, class Alloc>
bool operator>=(const SmallVector<T, N1, Alloc>& lhs, const SmallVector<T, N2, Alloc>& rhs) {
    return !(lhs < rhs);
}

#if FATP_HAS_CPP20

/**
 * @brief Three-way comparison operator (spaceship)
 * @note C++20 feature. Enables all comparison operators via rewriting.
 * @note Cross-capacity: can compare SmallVector<T,N1> with SmallVector<T,N2>
 */
template <class T, size_t N1, size_t N2, class Alloc>
auto operator<=>(const SmallVector<T, N1, Alloc>& lhs, const SmallVector<T, N2, Alloc>& rhs) {
    return std::lexicographical_compare_three_way(
        lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}
#endif

// ==================================================================================
// Type Traits
// ==================================================================================

// Specialization for SmallVector types
template <typename T, size_t C, typename A>
struct is_small_vector<SmallVector<T, C, A>> : std::true_type {};

// ==================================================================================
// CTAD Deduction Guides (C++17)
// ==================================================================================

// Allows: SmallVector v = {1, 2, 3};  -> SmallVector<int, 8>
template <class T, class Alloc = std::allocator<T>>
SmallVector(std::initializer_list<T>, Alloc = Alloc()) -> SmallVector<T, 8, Alloc>;

// Allows: SmallVector v(iter, iter);  -> SmallVector<iter::value_type, 8>
template <class InputIt,
          class Alloc = std::allocator<typename std::iterator_traits<InputIt>::value_type>,
          std::enable_if_t<!std::is_integral_v<InputIt>, int> = 0>
SmallVector(InputIt, InputIt, Alloc = Alloc()) 
    -> SmallVector<typename std::iterator_traits<InputIt>::value_type, 8, Alloc>;

} // namespace fat_p

// ==================================================================================
// C++17 PMR Support
// ==================================================================================

#if FATP_HAS_CPP17 && __has_include(<memory_resource>)
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
