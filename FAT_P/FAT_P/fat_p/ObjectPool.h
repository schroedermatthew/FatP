/**
 * @file ObjectPool.h
 * @brief High-performance object pool with concurrency policy support
 *
 *
 *
 * @layer Domain
 *
 * @details Object pool that integrates with fat_p concurrency infrastructure.
 *
 * Critical fixes implemented:
 * - CRITICAL-1: Deleted move operations (prevents use-after-free)
 * - CRITICAL-2: static_assert for Node layout (prevents fragile cast)
 * - CRITICAL-3: [[nodiscard]] on acquire() (prevents silent leaks)
 * - CRITICAL-4: Debug assertion in destructor (detects unreleased objects)
 * - CRITICAL-5: reserve() before free_list_ modification (exception safety)
 * - CRITICAL-6: try-catch in acquire() (constructor exception safety)
 *
 * @version 3.2
 */

#pragma once

/*
FATP_META:
  meta_version: 1
  component: ObjectPool
  file_role: public_header
  path: fat_p/ObjectPool.h
  namespace: fat_p
  layer: Containers
  summary: "Public header for ObjectPool."
  api_stability: in_work
  related:
    docs_search: "ObjectPool"
    tests:
      - tests/test_ObjectPool.cpp
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>
#include <type_traits>
#include <vector>

#include "ConcurrencyPolicies.h"
#include "FatPTypeTraits.h"

namespace fat_p
{

/**
 * @brief High-performance object pool with configurable synchronization
 *
 * @tparam T          Object type to pool
 * @tparam SyncPolicy Concurrency policy (default: SingleThreadedPolicy)
 *
 * @note Non-movable to prevent dangling pointer bugs
 * @note Template signature matches fat_p forward declaration
 * @note Blocks are never reclaimed until pool destruction (monotonic growth)
 */
template <typename T, typename SyncPolicy = SingleThreadedPolicy>
class [[nodiscard]] ObjectPool
{
public:
    // ========================================================================
    // Type Definitions
    // ========================================================================

    using value_type = T;
    using sync_policy_type = SyncPolicy;

private:
    // ========================================================================
    // Internal Node Structure
    // ========================================================================

    struct Node
    {
        alignas(T) std::byte storage[sizeof(T)];
        Node* next = nullptr;
    };

    // CRITICAL-2 FIX: Compile-time layout verification
    // Ensures reinterpret_cast<Node*>(obj) is valid because storage is at offset 0
    static_assert(offsetof(Node, storage) == 0,
                  "Node layout assumption violated: storage must be at offset 0");

    // Compile-time alignment verification
    static_assert(alignof(Node) >= alignof(T), "Node alignment must be sufficient for T");
    static_assert(offsetof(Node, storage) % alignof(T) == 0,
                  "Storage must be correctly aligned for T");

    // Compile-time type requirements
    static_assert(std::is_destructible_v<T>, "T must be destructible");

    // ========================================================================
    // Member Variables
    // ========================================================================

    Node* free_list_ = nullptr;
    size_t free_count_ = 0;
    std::vector<std::unique_ptr<Node[]>> mBlocks;
    size_t block_size_;
    mutable SyncPolicy sync_policy_; // mutable for const methods (ALL FOUR agreed)

#ifndef NDEBUG
    size_t acquired_count_ = 0; // Debug tracking for leak detection
    size_t total_acquires_ = 0; // Lifetime statistics
    size_t total_releases_ = 0;
#endif

    // ========================================================================
    // Block Management
    // ========================================================================

    void allocate_block()
    {
        auto block = std::make_unique<Node[]>(block_size_);
        Node* raw_block = block.get();

        // CRITICAL-5 FIX: Exception safety - reserve before modifying free_list_
        // If reserve throws, unique_ptr cleans up block and free_list_ remains valid
        mBlocks.reserve(mBlocks.size() + 1);

        // Initialize nodes and weave into free list (no-throw after reserve)
        for (size_t i = 0; i < block_size_; ++i)
        {
            Node* node = &raw_block[i];
            node->next = free_list_;
            free_list_ = node;
        }

        free_count_ += block_size_;

        // No-throw guarantee after reserve succeeded
        mBlocks.push_back(std::move(block));
    }

#ifndef NDEBUG
    static Node* acquired_sentinel() noexcept
    {
        return reinterpret_cast<Node*>(static_cast<std::uintptr_t>(1));
    }

    // Debug helper: check if pointer belongs to this pool
    bool is_from_pool(const T* obj) const noexcept
    {
        if (!obj)
        {
            return false;
        }

        const std::uintptr_t addr = reinterpret_cast<std::uintptr_t>(obj);
        for (const auto& block : mBlocks)
        {
            const std::uintptr_t begin = reinterpret_cast<std::uintptr_t>(block.get());
            const std::uintptr_t end = begin + (block_size_ * sizeof(Node));

            if (addr >= begin && addr < end)
            {
                return ((addr - begin) % sizeof(Node)) == 0;
            }
        }
        return false;
    }
#endif

public:
    // ========================================================================
    // Constructors and Destructor
    // ========================================================================

    /**
     * @brief Construct object pool
     * @param initial_block_size Number of objects per block (must be > 0)
     */
    explicit ObjectPool(size_t initial_block_size = 64)
        : block_size_(initial_block_size)
    {
        assert(block_size_ > 0 && "Block size must be positive");
        allocate_block();
    }

    // CRITICAL-1 FIX: Non-copyable and non-movable
    // Moving a pool while objects are acquired invalidates all outstanding pointers
    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;
    ObjectPool(ObjectPool&&) = delete;
    ObjectPool& operator=(ObjectPool&&) = delete;

    /**
     * @brief Destructor - destroys all blocks
     * @warning In debug mode, asserts if objects are still acquired
     */
    ~ObjectPool()
    {
#ifndef NDEBUG
        // CRITICAL-4 FIX: Debug assertion for unreleased objects
        assert(acquired_count_ == 0 &&
               "ObjectPool destroyed with unreleased objects - resource leak!");
#endif
        // unique_ptr handles block deallocation automatically
    }

    // ========================================================================
    // Core Operations
    // ========================================================================

    /**
     * @brief Acquires an object from the pool
     * @tparam Args Constructor argument types
     * @param args Arguments forwarded to T's constructor
     * @return Pointer to constructed object (never null)
     *
     * @throws std::bad_alloc if block allocation fails
     * @throws Any exception thrown by T's constructor (node is restored)
     *
     * @note If pool is empty, allocates a new block
     * @note Return value MUST be released back to pool or wrapped in PooledObject
     */
    template <typename... Args>
    [[nodiscard]] T* acquire(Args&&... args) // CRITICAL-3 FIX: [[nodiscard]]
    {
        auto guard = sync_policy_.lock();

        if (!free_list_)
        {
            allocate_block();
        }

        Node* node = free_list_;
        free_list_ = node->next;
        --free_count_;

#ifndef NDEBUG
        node->next = acquired_sentinel();
#endif

        // CRITICAL-6 FIX: Exception-safe construction
        try
        {
            T* obj = new (node->storage) T(std::forward<Args>(args)...);
#ifndef NDEBUG
            ++acquired_count_;
            ++total_acquires_;
#endif
            return obj;
        }
        catch (...)
        {
            // Restore node to free list before rethrowing
            node->next = free_list_;
            free_list_ = node;
            ++free_count_;
            throw;
        }
    }

    /**
     * @brief Tries to acquire without allocating new blocks
     * @tparam Args Constructor argument types
     * @param args Arguments forwarded to T's constructor
     * @return Pointer to constructed object, or nullptr if pool is empty
     *
     * @throws Any exception thrown by T's constructor (node is restored)
     *
     * @note Never allocates new blocks - useful for HPC where memory growth is forbidden
     * @note Conditionally noexcept based on T's constructor
     */
    template <typename... Args>
    [[nodiscard]] T* try_acquire(Args&&... args)
        noexcept(std::is_nothrow_constructible_v<T, Args...>)
    {
        auto guard = sync_policy_.lock();

        if (!free_list_)
        {
            return nullptr; // Pool empty, don't allocate
        }

        Node* node = free_list_;
        free_list_ = node->next;
        --free_count_;

#ifndef NDEBUG
        node->next = acquired_sentinel();
#endif

        // Optimization: Skip try-catch for noexcept constructors
        if constexpr (std::is_nothrow_constructible_v<T, Args...>)
        {
            T* obj = new (node->storage) T(std::forward<Args>(args)...);
#ifndef NDEBUG
            ++acquired_count_;
            ++total_acquires_;
#endif
            return obj;
        }
        else
        {
            try
            {
                T* obj = new (node->storage) T(std::forward<Args>(args)...);
#ifndef NDEBUG
                ++acquired_count_;
                ++total_acquires_;
#endif
                return obj;
            }
            catch (...)
            {
                node->next = free_list_;
                free_list_ = node;
                ++free_count_;
                throw;
            }
        }
    }

    /**
     * @brief Acquires raw storage suitable for one T (no object constructed)
     * @return Pointer to storage; a T object does not exist yet
     *
     * @note Caller MUST construct T in-place (placement-new) before any access.
     * @note Caller MUST destroy the object and then call release() exactly once.
     * @note Only enabled for trivially destructible T.
     */
    template <typename U = T>
    [[nodiscard]] std::enable_if_t<std::is_trivially_destructible_v<U>, T*> acquire_uninitialized()
    {
        auto guard = sync_policy_.lock();

        if (!free_list_)
        {
            allocate_block();
        }

        Node* node = free_list_;
        free_list_ = node->next;
        --free_count_;

#ifndef NDEBUG
        node->next = acquired_sentinel();
#endif

#ifndef NDEBUG
        ++acquired_count_;
        ++total_acquires_;
#endif
        return reinterpret_cast<T*>(node->storage);
    }

    /**
     * @brief Acquires raw storage whose bytes are zero-filled (no object constructed)
     * @return Pointer to storage; a T object does not exist yet
     *
     * @note Caller MUST construct T in-place (placement-new) before any access.
     * @note Only enabled for trivially constructible and trivially destructible T.
     * @note This zeroes bytes; it is not equivalent to value-initialization for all T.
     */
    template <typename U = T>
    [[nodiscard]] std::enable_if_t<std::is_trivially_constructible_v<U> &&
                                   std::is_trivially_destructible_v<U>,
                                   T*>
    acquire_zeroed()
    {
        auto guard = sync_policy_.lock();

        if (!free_list_)
        {
            allocate_block();
        }

        Node* node = free_list_;
        free_list_ = node->next;
        --free_count_;

#ifndef NDEBUG
        node->next = acquired_sentinel();
#endif

        std::memset(node->storage, 0, sizeof(T));

#ifndef NDEBUG
        ++acquired_count_;
        ++total_acquires_;
#endif
        return reinterpret_cast<T*>(node->storage);
    }

    /**
     * @brief Releases an object back to the pool
     * @param obj Pointer previously obtained from acquire()
     *
     * @note Calls destructor and returns memory to free list
     * @warning UB if obj was not acquired from this pool or already released
     * @note In debug builds, these conditions trigger assertion failures
     */
    void release(T* obj)
    {
        if (!obj)
        {
            return;
        }

        auto guard = sync_policy_.lock();

#ifndef NDEBUG
        // Double-release and foreign pointer detection
        assert(is_from_pool(obj) && "ObjectPool::release: pointer not from this pool");
        Node* node = reinterpret_cast<Node*>(obj);
        assert(node->next == acquired_sentinel() && "ObjectPool::release: double release detected");
        --acquired_count_;
        ++total_releases_;
#endif

        // Destroy object
        obj->~T();

        // Return node to free list
        // Valid cast due to static_assert checking offsetof(Node, storage) == 0
#ifndef NDEBUG
        // (node already computed above)
#else
        Node* node = reinterpret_cast<Node*>(obj);
#endif
        node->next = free_list_;
        free_list_ = node;
        ++free_count_;
    }

    // ========================================================================
    // Capacity Management
    // ========================================================================

    /**
     * @brief Pre-allocate blocks upfront
     * @param n Minimum number of blocks to have allocated
     *
     * @note Useful when workload size is known beforehand
     * @note No-op if already have >= n blocks
     */
    void reserve_blocks(size_t n)
    {
        auto guard = sync_policy_.lock();

        while (mBlocks.size() < n)
        {
            allocate_block();
        }
    }

    /**
     * @brief Rebuild the free list in address order to restore allocation locality
     * @return true if compaction was performed, false if pool has acquired objects
     *
     * @details After many random-order releases, the LIFO free list can become
     * scattered across memory, causing cache-unfriendly pointer chasing on
     * subsequent acquires. This function rebuilds the free list so that
     * consecutive acquires return memory in sequential address order.
     *
     * @note Only compacts when pool is fully free (no outstanding acquired objects)
     * @note Complexity: O(capacity) when compaction occurs
     * @note Call this at natural idle points (end of frame, connection pool drain)
     *
     * @warning This is an optimization for specific workloads with "flush and refill"
     * patterns. Most applications do not need to call this.
     */
    bool try_compact_free_list()
    {
        auto guard = sync_policy_.lock();

        // Only compact when fully free
        const size_t total_capacity = mBlocks.size() * block_size_;
        if (free_count_ != total_capacity)
        {
            return false; // Objects still acquired
        }

#ifndef NDEBUG
        assert(acquired_count_ == 0 && "Counter mismatch: free_count_ == capacity but acquired_count_ != 0");
#endif

        // Rebuild free list in address order
        // We want acquires to return memory sequentially, so build list in reverse
        // (LIFO means first node in list is returned first)
        free_list_ = nullptr;

        // Collect and sort block pointers by address
        std::vector<Node*> blocks;
        blocks.reserve(mBlocks.size());
        for (const auto& block : mBlocks)
        {
            blocks.push_back(block.get());
        }

        std::sort(blocks.begin(), blocks.end());

        // Process blocks in reverse address order so lower-address blocks are at front of list
        for (auto it = blocks.rbegin(); it != blocks.rend(); ++it)
        {
            Node* block_start = *it;
            // Link nodes in reverse order within block
            for (size_t i = block_size_; i-- > 0;)
            {
                Node* node = &block_start[i];
                node->next = free_list_;
                free_list_ = node;
            }
        }

        return true;
    }

    // ========================================================================
    // Diagnostics
    // ========================================================================

    /**
     * @brief Pool statistics for monitoring and debugging
     */
    struct Stats
    {
        size_t total_capacity; ///< Total objects pool can hold
        size_t available;      ///< Objects currently in free list
        size_t acquired;       ///< Objects currently in use
        size_t num_blocks;     ///< Number of allocated blocks
        size_t block_size;     ///< Objects per block
        size_t lifetime_acquires; ///< Total acquire() calls (0 in release builds)
        size_t lifetime_releases; ///< Total release() calls (0 in release builds)
    };

    /**
     * @brief Returns current pool statistics
     * @return Stats structure with current state
     */
    Stats stats() const
    {
        auto guard = sync_policy_.lock();

        const size_t total = mBlocks.size() * block_size_;
        const size_t avail = free_count_;

#ifndef NDEBUG
        assert((acquired_count_ + free_count_) == total && "ObjectPool: counter mismatch");
#endif

#ifndef NDEBUG
        return Stats{total,
                     avail,
                     acquired_count_,
                     mBlocks.size(),
                     block_size_,
                     total_acquires_,
                     total_releases_};
#else
        return Stats{total,
                     avail,
                     total - avail,
                     mBlocks.size(),
                     block_size_,
                     0u,
                     0u};
#endif
    }

    /// @brief Get the block size
    size_t block_size() const noexcept
    {
        return block_size_;
    }

    /// @brief Get total number of blocks allocated
    size_t num_blocks() const
    {
        auto guard = sync_policy_.lock();
        return mBlocks.size();
    }

    /// @brief Get total capacity of the pool
    size_t capacity() const
    {
        auto guard = sync_policy_.lock();
        return mBlocks.size() * block_size_;
    }

    /**
     * @brief Get number of objects available for acquire()
     * @return Number of free objects
     * @note O(1) operation
     */
    size_t available() const
    {
        auto guard = sync_policy_.lock();

        return free_count_;
    }

    /**
     * @brief Get number of currently acquired objects
     * @return Number of objects in use (debug mode only, 0 in release)
     */
    size_t active_count() const
    {
#ifndef NDEBUG
        auto guard = sync_policy_.lock();
        return acquired_count_;
#else
        return 0; // Not tracked in release mode
#endif
    }
};

// ============================================================================
// RAII Wrapper
// ============================================================================

/**
 * @brief RAII wrapper for object pool objects
 * @tparam T Object type
 * @tparam SyncPolicy Synchronization policy
 *
 * @details Automatically releases object back to pool on destruction.
 * Move-only semantics prevent accidental copies.
 */
template <typename T, typename SyncPolicy = SingleThreadedPolicy>
class PooledObject
{
public:
    using pool_type = ObjectPool<T, SyncPolicy>;

private:
    pool_type* mPool = nullptr;
    T* mObj = nullptr;

public:
    /// @brief Default constructor - creates empty wrapper
    PooledObject() = default;

    /**
     * @brief Construct from pool and object
     * @param pool Pool that owns the object
     * @param obj Object pointer (may be nullptr)
     */
    PooledObject(pool_type* pool, T* obj)
        : mPool(pool)
        , mObj(obj)
    {
    }

    // Non-copyable
    PooledObject(const PooledObject&) = delete;
    PooledObject& operator=(const PooledObject&) = delete;

    // Movable
    PooledObject(PooledObject&& other) noexcept
        : mPool(other.mPool)
        , mObj(other.mObj)
    {
        other.mPool = nullptr;
        other.mObj = nullptr;
    }

    PooledObject& operator=(PooledObject&& other) noexcept
    {
        if (this != &other)
        {
            reset();
            mPool = other.mPool;
            mObj = other.mObj;
            other.mPool = nullptr;
            other.mObj = nullptr;
        }
        return *this;
    }

    /// @brief Destructor - releases object back to pool
    ~PooledObject()
    {
        reset();
    }

    /// @brief Release object back to pool and clear wrapper
    void reset()
    {
        if (mObj && mPool)
        {
            mPool->release(mObj);
        }
        mObj = nullptr;
        mPool = nullptr;
    }

    /**
     * @brief Release ownership without returning to pool
     * @return Raw pointer (caller takes ownership)
     * @note Caller is responsible for calling pool->release()
     */
    [[nodiscard]] T* release()
    {
        T* tmp = mObj;
        mObj = nullptr;
        mPool = nullptr;
        return tmp;
    }

    // Accessors with null checks
    T* operator->()
    {
        assert(mObj != nullptr && "Dereferencing null PooledObject");
        return mObj;
    }

    const T* operator->() const
    {
        assert(mObj != nullptr && "Dereferencing null PooledObject");
        return mObj;
    }

    T& operator*()
    {
        assert(mObj != nullptr && "Dereferencing null PooledObject");
        return *mObj;
    }

    const T& operator*() const
    {
        assert(mObj != nullptr && "Dereferencing null PooledObject");
        return *mObj;
    }

    /// @brief Get raw pointer (may be nullptr)
    T* get() noexcept
    {
        return mObj;
    }

    /// @brief Get raw pointer (may be nullptr)
    const T* get() const noexcept
    {
        return mObj;
    }

    /// @brief Check if wrapper holds a valid object
    explicit operator bool() const noexcept
    {
        return mObj != nullptr;
    }

    /// @brief Get owning pool
    pool_type* get_pool() const noexcept
    {
        return mPool;
    }
};

// ============================================================================
// Factory Function
// ============================================================================

/**
 * @brief Helper to create RAII-wrapped pooled object
 * @tparam T Object type
 * @tparam SyncPolicy Synchronization policy
 * @tparam Args Constructor argument types
 * @param pool Object pool
 * @param args Constructor arguments
 * @return RAII wrapper holding newly acquired object
 */
template <typename T, typename SyncPolicy, typename... Args>
[[nodiscard]] PooledObject<T, SyncPolicy>
make_pooled(ObjectPool<T, SyncPolicy>& pool, Args&&... args)
{
    return PooledObject<T, SyncPolicy>(&pool, pool.acquire(std::forward<Args>(args)...));
}

// ============================================================================
// Type Traits (matches FatPTypeTraits.h forward declaration)
// ============================================================================

template <typename T, typename SyncPolicy>
struct is_object_pool<ObjectPool<T, SyncPolicy>> : std::true_type
{
};

// ============================================================================
// Convenience Aliases
// ============================================================================

/// @brief Single-threaded object pool (zero synchronization overhead)
template <typename T>
using SimpleObjectPool = ObjectPool<T, SingleThreadedPolicy>;

/// @brief Thread-safe object pool with mutex synchronization
template <typename T>
using ThreadSafeObjectPool = ObjectPool<T, MutexSynchronizationPolicy>;

} // namespace fat_p
