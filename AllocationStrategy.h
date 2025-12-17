/**
 * @file AllocationStrategy.h
 * @brief Provides the AllocationStrategy template for policy-based allocators with zero runtime overhead.
 *
 * @details The AllocationStrategy template allows custom allocation policies (e.g., standard heap, fixed stack, pool)
 * with conditional thread-safety via ConcurrencyPolicies, DbC via enforce, safe ops via CheckedArithmetic/Expected,
 * RAII via ScopeGuard, type-safe sizes via StrongId, enforced init via EnforcedInit, logging via DiagnosticLogger,
 * and traits via TypeTraits. Supports policy composition (e.g., Synchronized<PoolImpl>).
 * Wish-List: More policies (e.g., arena), atomic for lock-free stateful, SIMD for bulk alloc if applicable.
 * Perf: Zero-overhead for stateless; constexpr where possible.
 * Extensible: Impl template for custom policies; composition via wrappers.
 * C++17 compliant; header-only; no external deps (guards optional <mutex>/<atomic>).
 *
 * @note Checks are debug-only (via enforce); release zero-overhead.
 * @note Supports movable-only T via forward.
 */
#pragma once
#if !defined(FATP_USE_MUTEX)
#define FATP_USE_MUTEX 1 // Enable by default; undef to disable <mutex>
#endif
#if !defined(FATP_USE_ATOMIC)
#define FATP_USE_ATOMIC 1 // Enable by default
#endif
#include <mutex> // for std::mutex if enabled
#include <atomic> // for std::atomic if enabled
#include <memory> // for std::shared_ptr and memory utilities
#include <new> // for std::align_val_t, operator new with alignment
#include <stdexcept> // for std::runtime_error
#include <algorithm> // for std::exchange in move constructor
#include <cstddef> // for std::byte, size_t
#include <string> // for std::string, std::to_string
#include <utility> // for std::exchange
#include <type_traits> // for std::is_same_v
#include <limits> // for numeric_limits
#include <array> // for std::array

#include "enforce.h" // For DbC enforces in alloc/dealloc
#include "Expected.h" // For safe alloc returns (e.g., Expected<T*, std::string>)
#include "ConcurrencyPolicies.h" // For conditional thread-safety in wrappers
#include "CheckedArithmetic.h" // For safe mul in alloc (n * sizeof(T))
#include "ScopeGuard.h" // For RAII guards in alloc to ensure dealloc on failure
#include "StrongId.h" // For type-safe sizes (e.g., StrongId<size_t> for n)
#include "EnforcedInit.h" // For enforced init in stateful policies (e.g., buffer)
#include "DiagnosticLogger_Core.h" // For logging allocation failures

namespace fat_p {
    // Forward declaration of the AllocationStrategy needed for policy
    // constructors and rebind operations.
    template <typename T, typename Impl>
    class AllocationStrategy;
    // ====================================================================
    // 1. Core Allocation Policy Implementations (Zero-Overhead)
    // ====================================================================
    /**
     * @brief Custom exception for memory allocation failures within policies.
     */
    class AllocationFailure : public std::runtime_error {
    public:
        explicit AllocationFailure(const std::string& message)
            : std::runtime_error("Allocation Failure: " + message) {
            // Integration: DiagnosticLogger for logging failures
            LOG_ERROR(message);
        }
    };
    // --- Standard Allocator Policy ---
    /** @brief Tag used for comparing StandardAllocatorImpl instances. */
    struct StandardAllocatorTag {};
    /**
     * @brief Implements standard heap allocation using global operators.
     *
     * This policy is stateless, relying on the C++ standard library's heap
     * management (via ::operator new/delete), which is already thread-safe.
     *
     * @tparam T The type of object to allocate.
     */
    template <typename T>
    struct StandardAllocatorImpl {
        using value_type = T;
        using Tag = StandardAllocatorTag; // Crucial for synchronization check
        /** @brief Default constructor. */
        constexpr StandardAllocatorImpl() noexcept = default;
        /**
         * @brief Templated copy constructor for rebinding/cross-type conversion.
         */
        template <typename OtherT, typename OtherImpl>
        constexpr StandardAllocatorImpl(
            const AllocationStrategy<OtherT, OtherImpl>&) noexcept {
        }
        /**
         * @brief Templated move constructor for rebinding/cross-type conversion.
         */
        template <typename OtherT, typename OtherImpl>
        constexpr StandardAllocatorImpl(
            AllocationStrategy<OtherT, OtherImpl>&&) noexcept {
        }
        /** @brief Rebind struct for converting to an allocator for a different type. */
        template <class U>
        struct rebind_impl {
            using type = StandardAllocatorImpl<U>;
        };
        /**
         * @brief Allocates raw memory using global operator new.
         * @param n The number of objects of type T to allocate.
         * @return A pointer to the allocated memory.
         * @throw std::bad_alloc if allocation fails.
         */
        T* allocate(StrongId<size_t, struct AllocSizeTag> n) {
            enforce(n.get() > 0, "Allocate with n=0");
            size_t bytes = checked_mul<ThrowOnErrorPolicy>(n.get(), sizeof(T));
            // FIX: operator new throws bad_alloc on failure, never returns nullptr
            // For over-aligned types (alignment > alignof(std::max_align_t)),
            // use aligned new (C++17)
            void* p;
            constexpr size_t default_align = alignof(std::max_align_t);
            if constexpr (alignof(T) > default_align) {
                p = ::operator new(bytes, std::align_val_t(alignof(T)));
            } else {
                p = ::operator new(bytes);
            }
            return static_cast<T*>(p);
        }
        /**
         * @brief Deallocates memory using global operator delete.
         * @param p Pointer to the memory.
         * @param n Number of objects (ignored for standard deallocation).
         */
        void deallocate(T* p, StrongId<size_t, struct AllocSizeTag> /*n*/) noexcept {
            enforce(p != nullptr, "Deallocate null ptr");
            // Match allocation - use aligned delete for over-aligned types
            constexpr size_t default_align = alignof(std::max_align_t);
            if constexpr (alignof(T) > default_align) {
                ::operator delete(p, std::align_val_t(alignof(T)));
            } else {
                ::operator delete(p);
            }
        }
        /**
         * @brief Checks if two standard allocators are equal (always true).
         */
        friend bool operator==(const StandardAllocatorImpl&,
            const StandardAllocatorImpl&) noexcept {
            return true;
        }
        /**
         * @brief Checks if two standard allocators are unequal (always false).
         */
        friend bool operator!=(const StandardAllocatorImpl& a,
            const StandardAllocatorImpl& b) noexcept {
            return !(a == b);
        }
    };
    // --- Stack Allocator Policy (Stateful with EnforcedInit) ---
    /** @brief Tag used for comparing StackAllocatorImpl instances. */
    struct StackAllocatorTag {};
    /**
     * @brief Implements a fixed-size, stack-based memory allocation strategy.
     *
     * This allocator manages a local, fixed-size byte buffer. It is a
     * non-propagating stateful allocator.
     *
     * @tparam T The type of object to allocate.
     * @tparam N The size of the internal stack buffer in bytes.
     */
    template <typename T, size_t N>
    struct StackAllocatorImpl {
        using value_type = T;
        using Tag = StackAllocatorTag; // Crucial for synchronization check
        /** @brief Rebind struct for converting to an allocator for a different type. */
        template <class U>
        struct rebind_impl {
            using type = StackAllocatorImpl<U, N>;
        };
        /** @brief Global atomic counter to generate unique IDs. */
        // FIX: Properly initialize atomic with explicit constructor
        static inline std::atomic<size_t> next_id{0};
        
        /// @brief Properly aligned storage for the buffer
        struct alignas(alignof(std::max_align_t)) AlignedBuffer {
            std::byte data[N];
        };
        
        /// @brief The internal fixed-size memory buffer (enforced init).
        EnforcedInit<AlignedBuffer> buffer_;
        /// @brief Current offset into the buffer for the next allocation (atomic for lock-free).
        std::atomic<size_t> offset_{ 0 };
        /// @brief Unique identifier for this specific allocator instance.
        const size_t id_;
        
        /** @brief Default constructor. */
        StackAllocatorImpl() 
            : id_(next_id.fetch_add(1, std::memory_order_relaxed)) {
            // Initialize the buffer
            (void)buffer_.init(AlignedBuffer{});
        }
        
        /**
         * @brief Copy constructor. Creates a new, unique instance.
         */
        StackAllocatorImpl(const StackAllocatorImpl& /*other*/) noexcept
            : id_(next_id.fetch_add(1, std::memory_order_relaxed)) {
            // New instance, reset offset and initialize buffer
            (void)buffer_.init(AlignedBuffer{});
        }
        
        /**
         * @brief Move constructor. Transfers the allocation progress.
         * FIX: Properly handle move semantics with atomic operations
         */
        StackAllocatorImpl(StackAllocatorImpl&& other) noexcept
            : id_(next_id.fetch_add(1, std::memory_order_relaxed)) {
            // Initialize buffer
            (void)buffer_.init(AlignedBuffer{});
            // Transfer offset atomically
            offset_.store(other.offset_.load(std::memory_order_acquire), 
                         std::memory_order_release);
            // Reset the moved-from object
            other.offset_.store(0, std::memory_order_release);
        }
        
        /**
         * @brief Templated constructor for rebind support.
         */
        template <typename U, size_t M>
        StackAllocatorImpl(const StackAllocatorImpl<U, M>&) noexcept
            : id_(next_id.fetch_add(1, std::memory_order_relaxed)) {
            (void)buffer_.init(AlignedBuffer{});
        }
        
        /**
         * @brief Allocates memory from the stack buffer.
         * @param n Number of elements to allocate.
         * @return Pointer to allocated memory or throws on failure.
         * @throw AllocationFailure if insufficient space or overflow.
         */
        T* allocate(StrongId<size_t, struct AllocSizeTag> n) {
            enforce(n.get() > 0, "Allocate with n=0");
            
            // FIX: Proper overflow checking with safe arithmetic
            size_t size = checked_mul<ThrowOnErrorPolicy>(n.get(), sizeof(T));
            
            // Calculate alignment
            const size_t alignment = alignof(T);
            size_t current = offset_.load(std::memory_order_acquire);
            
            // FIX: Proper alignment calculation with bounds checking
            size_t aligned_offset = (current + alignment - 1) & ~(alignment - 1);
            
            // Check for alignment overflow
            if (aligned_offset < current) {
                throw AllocationFailure("Alignment overflow in stack allocator");
            }
            
            // FIX: Check bounds before allocation
            if (aligned_offset > N || size > N - aligned_offset) {
                throw AllocationFailure(
                    "Stack allocator out of memory: requested " + 
                    std::to_string(size) + " bytes, available " + 
                    std::to_string(N > aligned_offset ? N - aligned_offset : 0) + " bytes");
            }
            
            size_t new_offset = aligned_offset + size;
            
            // FIX: Use atomic compare-exchange for thread-safe allocation
            while (!offset_.compare_exchange_weak(current, new_offset,
                                                  std::memory_order_release,
                                                  std::memory_order_acquire)) {
                // Recalculate with updated current value
                aligned_offset = (current + alignment - 1) & ~(alignment - 1);
                
                if (aligned_offset < current || aligned_offset > N || 
                    size > N - aligned_offset) {
                    throw AllocationFailure("Stack allocator out of memory (concurrent)");
                }
                
                new_offset = aligned_offset + size;
            }
            
            // Return pointer to allocated space
            auto& buf = buffer_.get();
            return reinterpret_cast<T*>(&buf.data[aligned_offset]);
        }
        
        /**
         * @brief Deallocates memory (no-op for stack allocator).
         * @note Stack allocator uses bump-pointer allocation; individual deallocation is not supported.
         */
        void deallocate(T* p, StrongId<size_t, struct AllocSizeTag> /*n*/) noexcept {
            enforce(p != nullptr, "Deallocate null ptr");
            // Stack allocator does not support individual deallocation
            // Memory is reclaimed when allocator is destroyed or reset
        }
        
        /**
         * @brief Resets the allocator, reclaiming all allocated memory.
         */
        void reset() noexcept {
            offset_.store(0, std::memory_order_release);
        }
        
        /**
         * @brief Returns the current offset in the buffer.
         */
        size_t get_offset() const noexcept {
            return offset_.load(std::memory_order_acquire);
        }
        
        /**
         * @brief Returns the total buffer size.
         */
        constexpr size_t capacity() const noexcept {
            return N;
        }
        
        /**
         * @brief Returns the available space in bytes.
         */
        size_t available() const noexcept {
            size_t current = offset_.load(std::memory_order_acquire);
            return current < N ? N - current : 0;
        }
        
        /**
         * @brief Checks if two stack allocators are equal (same instance).
         * FIX: Use unique ID for proper comparison
         */
        friend bool operator==(const StackAllocatorImpl& a,
            const StackAllocatorImpl& b) noexcept {
            return a.id_ == b.id_;
        }
        
        /**
         * @brief Checks if two stack allocators are unequal.
         */
        friend bool operator!=(const StackAllocatorImpl& a,
            const StackAllocatorImpl& b) noexcept {
            return !(a == b);
        }
    };
    
    // --- Pool Allocator Policy (Free-list based) ---
    /** @brief Tag used for comparing PoolAllocatorImpl instances. */
    struct PoolAllocatorTag {};
    
    /**
     * @brief Implements a fixed-size pool allocation strategy.
     *
     * This allocator pre-allocates a pool of fixed-size blocks and manages
     * them via a free list for O(1) allocation/deallocation.
     *
     * @tparam T The type of object to allocate.
     * @tparam N The number of objects in the pool.
     */
    template <typename T, size_t N>
    struct PoolAllocatorImpl {
        using value_type = T;
        using Tag = PoolAllocatorTag;
        
        /** @brief Rebind struct for converting to an allocator for a different type. */
        template <class U>
        struct rebind_impl {
            using type = PoolAllocatorImpl<U, N>;
        };
        
        /** @brief Global atomic counter to generate unique IDs. */
        static inline std::atomic<size_t> next_id{0};
        
        /** @brief Node structure for the free list. */
        union Node {
            alignas(T) std::byte storage[sizeof(T)];
            Node* next;
        };
        
        /// @brief The pool of nodes
        EnforcedInit<std::array<Node, N>> pool_;
        /// @brief Head of the free list (atomic for lock-free access)
        std::atomic<Node*> free_list_;
        /// @brief Unique identifier for this specific allocator instance
        const size_t id_;
        
        /** @brief Default constructor - initializes the free list. */
        PoolAllocatorImpl() 
            : free_list_(nullptr)
            , id_(next_id.fetch_add(1, std::memory_order_relaxed)) {
            // Initialize pool array
            (void)pool_.init(std::array<Node, N>{});
            // Initialize free list
            auto& pool = pool_.get();
            for (size_t i = 0; i < N - 1; ++i) {
                pool[i].next = &pool[i + 1];
            }
            pool[N - 1].next = nullptr;
            free_list_.store(&pool[0], std::memory_order_release);
        }
        
        /**
         * @brief Copy constructor. Creates a new, unique instance.
         */
        PoolAllocatorImpl(const PoolAllocatorImpl& /*other*/) noexcept
            : free_list_(nullptr)
            , id_(next_id.fetch_add(1, std::memory_order_relaxed)) {
            // Initialize new pool
            (void)pool_.init(std::array<Node, N>{});
            auto& pool = pool_.get();
            for (size_t i = 0; i < N - 1; ++i) {
                pool[i].next = &pool[i + 1];
            }
            pool[N - 1].next = nullptr;
            free_list_.store(&pool[0], std::memory_order_release);
        }
        
        /**
         * @brief Move constructor.
         */
        PoolAllocatorImpl(PoolAllocatorImpl&& other) noexcept
            : free_list_(nullptr)
            , id_(next_id.fetch_add(1, std::memory_order_relaxed)) {
            // Initialize pool  
            (void)pool_.init(std::array<Node, N>{});
            // Move the free list pointer
            free_list_.store(other.free_list_.load(std::memory_order_acquire),
                           std::memory_order_release);
            other.free_list_.store(nullptr, std::memory_order_release);
        }
        
        /**
         * @brief Templated constructor for rebind support.
         */
        template <typename U, size_t M>
        PoolAllocatorImpl(const PoolAllocatorImpl<U, M>&) noexcept
            : free_list_(nullptr)
            , id_(next_id.fetch_add(1, std::memory_order_relaxed)) {
            (void)pool_.init(std::array<Node, N>{});
            auto& pool = pool_.get();
            for (size_t i = 0; i < N - 1; ++i) {
                pool[i].next = &pool[i + 1];
            }
            pool[N - 1].next = nullptr;
            free_list_.store(&pool[0], std::memory_order_release);
        }
        
        /**
         * @brief Allocates a single object from the pool.
         * @param n Number of objects (must be 1 for pool allocator).
         * @return Pointer to allocated memory.
         * @throw AllocationFailure if pool is exhausted or n != 1.
         */
        T* allocate(StrongId<size_t, struct AllocSizeTag> n) {
            enforce(n.get() == 1, "Pool allocator can only allocate single objects");
            
            // Lock-free pop from free list
            Node* head = free_list_.load(std::memory_order_acquire);
            while (head != nullptr) {
                Node* next = head->next;
                if (free_list_.compare_exchange_weak(head, next,
                                                    std::memory_order_release,
                                                    std::memory_order_acquire)) {
                    return reinterpret_cast<T*>(head->storage);
                }
            }
            
            throw AllocationFailure("Pool allocator exhausted");
        }
        
        /**
         * @brief Deallocates an object back to the pool.
         * @param p Pointer to the memory.
         * @param n Number of objects (must be 1).
         */
        void deallocate(T* p, StrongId<size_t, struct AllocSizeTag> n) noexcept {
            enforce(p != nullptr, "Deallocate null ptr");
            enforce(n.get() == 1, "Pool allocator can only deallocate single objects");
            
            // Lock-free push to free list
            Node* node = reinterpret_cast<Node*>(p);
            Node* head = free_list_.load(std::memory_order_acquire);
            do {
                node->next = head;
            } while (!free_list_.compare_exchange_weak(head, node,
                                                      std::memory_order_release,
                                                      std::memory_order_acquire));
        }
        
        /**
         * @brief Returns the pool capacity.
         */
        constexpr size_t capacity() const noexcept {
            return N;
        }
        
        /**
         * @brief Checks if two pool allocators are equal (same instance).
         */
        friend bool operator==(const PoolAllocatorImpl& a,
            const PoolAllocatorImpl& b) noexcept {
            return a.id_ == b.id_;
        }
        
        /**
         * @brief Checks if two pool allocators are unequal.
         */
        friend bool operator!=(const PoolAllocatorImpl& a,
            const PoolAllocatorImpl& b) noexcept {
            return !(a == b);
        }
    };
    
    // ====================================================================
    // Synchronization Wrappers
    // ====================================================================
    
    /**
     * @brief Wrapper adding thread-safety to stateless allocators.
     * FIX: Proper handling of Expected return types
     */
    template <typename T, typename BaseImpl,
              typename SyncPolicy = MutexSynchronizationPolicy>
    class SynchronizedWrapper : public BaseImpl, protected SyncPolicy {
    public:
        using value_type = T;
        template <class U>
        struct rebind_impl {
            using type = SynchronizedWrapper<U,
                typename BaseImpl::template rebind_impl<U>::type, SyncPolicy>;
        };
        
        SynchronizedWrapper() = default;
        SynchronizedWrapper(const SynchronizedWrapper&) = default;
        SynchronizedWrapper(SynchronizedWrapper&&) noexcept = default;
        
        template <typename OtherT, typename OtherImpl>
        SynchronizedWrapper(
            const AllocationStrategy<OtherT, OtherImpl>& other) noexcept
            : BaseImpl(other) {
        }
        
        template <typename OtherT, typename OtherImpl>
        SynchronizedWrapper(
            AllocationStrategy<OtherT, OtherImpl>&& other) noexcept
            : BaseImpl(std::move(other)) {
        }
        
        /**
         * @brief Thread-safe allocation.
         * FIX: Return raw pointer, let AllocationStrategy wrap in Expected
         */
        T* allocate(StrongId<size_t, struct AllocSizeTag> n) {
            typename SyncPolicy::LockGuard lock(this->getLock());
            return BaseImpl::allocate(n);
        }
        
        void deallocate(T* p, StrongId<size_t, struct AllocSizeTag> n) noexcept {
            typename SyncPolicy::LockGuard lock(this->getLock());
            BaseImpl::deallocate(p, n);
        }
        
        friend bool operator==(const SynchronizedWrapper& a,
            const SynchronizedWrapper& b) noexcept {
            return static_cast<const BaseImpl&>(a) ==
                static_cast<const BaseImpl&>(b);
        }
        
        friend bool operator!=(const SynchronizedWrapper& a,
            const SynchronizedWrapper& b) noexcept {
            return !(a == b);
        }
    };
    
    // --- LockFreeWrapper using Atomic (Updates: Lock-Free for Stateful) ---
#if FATP_USE_ATOMIC
    /**
     * @brief Wrapper for lock-free operations on stateful allocators.
     * @note Requires BaseImpl to use atomic operations internally.
     */
    template <typename T, typename BaseImpl>
    class LockFreeWrapper : public BaseImpl {
    public:
        using value_type = T;
        template <class U>
        struct rebind_impl { 
            using type = LockFreeWrapper<U, 
                typename BaseImpl::template rebind_impl<U>::type>; 
        };
        
        LockFreeWrapper() = default;
        LockFreeWrapper(const LockFreeWrapper&) = default;
        LockFreeWrapper(LockFreeWrapper&&) noexcept = default;
        
        T* allocate(StrongId<size_t, struct AllocSizeTag> n) {
            // Delegate to base impl which should use atomic operations
            return BaseImpl::allocate(n);
        }
        
        void deallocate(T* p, StrongId<size_t, struct AllocSizeTag> n) noexcept {
            BaseImpl::deallocate(p, n);
        }
    };
#endif
    
    // ====================================================================
    // 2. The AllocationStrategy (Public Interface)
    // ====================================================================
    /**
     * @brief The main allocator interface compliant with C++ standards.
     *
     * This class uses the Empty Base Class Optimization (EBCO) by inheriting
     * from the Impl, ensuring no overhead for stateless allocators.
     *
     * @tparam T The type of object to allocate.
     * @tparam Impl The memory allocation policy (e.g., StandardAllocatorImpl).
     */
    template <typename T, typename Impl>
    class AllocationStrategy : public Impl {
    public:
        using value_type = T;
        using ImplType = Impl;
        /** @brief Rebind struct required by the C++ standard. */
        template <class U>
        struct rebind {
            using other = AllocationStrategy<U,
                typename Impl::template rebind_impl<U>::type>;
        };
        /** @brief Default constructor. */
        constexpr AllocationStrategy() = default;
        /** @brief Copy constructor. */
        constexpr AllocationStrategy(const AllocationStrategy&) noexcept = default;
        /** @brief Assignment operator. */
        AllocationStrategy& operator=(
            const AllocationStrategy&) noexcept = default;
        /**
         * @brief Templated constructor for cross-type and rebind conversion.
         * @tparam U The value type of the other allocator.
         * @tparam UImpl The implementation type of the other allocator.
         * @param other The other allocator instance.
         */
        template <typename U, typename UImpl>
        constexpr AllocationStrategy(const AllocationStrategy<U, UImpl>& other) noexcept
            : Impl(other) {
        }
        /**
         * @brief Allocates raw memory by delegating to the policy.
         * @param n The number of elements to allocate.
         * @return Expected<T*, std::string> Pointer on success, error on failure.
         * FIX: Proper exception handling and Expected construction
         */
        Expected<T*, std::string> allocate(StrongId<size_t, struct AllocSizeTag> n) {
            try {
                T* ptr = Impl::allocate(n);
                return ptr;
            }
            catch (const std::bad_alloc& e) {
                return make_unexpected(std::string("Bad alloc: ") + e.what());
            }
            catch (const AllocationFailure& e) {
                return make_unexpected(std::string("Allocation failure: ") + e.what());
            }
            catch (const std::exception& e) {
                return make_unexpected(std::string("Exception: ") + e.what());
            }
            catch (...) {
                return make_unexpected(std::string("Unknown allocation error"));
            }
        }
        /**
         * @brief Deallocates memory by delegating to the policy.
         * @param p Pointer to the memory to deallocate.
         * @param n The number of elements being deallocated.
         */
        void deallocate(T* p, StrongId<size_t, struct AllocSizeTag> n) noexcept {
            Impl::deallocate(p, n);
        }
        /**
         * @brief Constructs an object in pre-allocated memory.
         * @tparam U The type of object to construct.
         * @tparam Args The constructor arguments.
         * @param p Pointer to the memory.
         * @param args Arguments forwarded to the constructor.
         */
        template <class U, class... Args>
        void construct(U* p, Args&&... args) {
            ::new ((void*)p) U(std::forward<Args>(args)...);
        }
        /**
         * @brief Destroys an object.
         * @tparam U The type of object to destroy.
         * @param p Pointer to the object.
         */
        template <class U>
        void destroy(U* p) {
            p->~U();
        }
        /**
         * @brief Checks if two AllocationStrategy are equal.
         * @note Delegates equality check to the underlying implementation policy.
         */
        friend bool operator==(const AllocationStrategy& a,
            const AllocationStrategy& b) noexcept {
            return static_cast<const Impl&>(a) == static_cast<const Impl&>(b);
        }
        /**
         * @brief Checks if two AllocationStrategy are unequal.
         */
        friend bool operator!=(const AllocationStrategy& a,
            const AllocationStrategy& b) noexcept {
            return !(a == b);
        }
    };
    
    // ====================================================================
    // 3. Public Type Aliases
    // ====================================================================
    /**
     * @brief Type alias for the standard, heap-based allocator.
     * @tparam T The value type.
     */
    template <typename T>
    using StandardAllocator =
        AllocationStrategy<T, StandardAllocatorImpl<T>>;
    /**
     * @brief Type alias for a thread-safe, heap-based allocator.
     * @tparam T The value type.
     */
    template <typename T>
    using SynchronizedAllocator =
        AllocationStrategy<T,
        SynchronizedWrapper<T, StandardAllocatorImpl<T>>>;
    /**
     * @brief Type alias for a fast, fixed-size stack allocator (4KB).
     * @tparam T The value type.
     */
    template <typename T>
    using FastStackAllocator =
        AllocationStrategy<T, StackAllocatorImpl<T, 4096>>;
    /**
     * @brief Type alias for a pool allocator.
     * @tparam T The value type.
     */
    template <typename T>
    using PoolAllocator =
        AllocationStrategy<T, PoolAllocatorImpl<T, 1024>>;
    
    // LockFreeStackAllocator using LockFreeWrapper
#if FATP_USE_ATOMIC
    template <typename T>
    using LockFreeStackAllocator =
        AllocationStrategy<T, LockFreeWrapper<T, StackAllocatorImpl<T, 4096>>>;
#endif
} // namespace fat_p
