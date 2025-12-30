/**
 * @file AllocationStrategies.h
 * @brief Lightweight allocator policies for Fat-P containers.
 *
 * @layer Infrastructure
 *
 * Provides three allocator policies for use with policy-based containers
 * like StableHashMap:
 *
 * - NewDeleteAllocator: Standard new/delete per object (recommended default)
 * - BlockAllocator: Contiguous block allocation with bump pointer
 * - PoolAllocator: Fixed-size pre-allocated pool
 *
 * These allocators are intentionally placed in the root `fat_p` namespace
 * as they are cross-cutting primitives used by multiple container components.
 *
 * For full-featured allocators with DbC, thread-safety wrappers, diagnostics,
 * and Expected-based error handling, see FatPAllocationStrategies.h.
 *
 * @note Thread-safety: NOT thread-safe. All allocators require external
 *       synchronization for concurrent access. For thread-safe allocators,
 *       see FatPAllocationStrategies.h which provides SynchronizedWrapper
 *       and LockFreeWrapper policies.
 *
 * @see StableHashMap for primary usage with hash containers.
 * @see FatPAllocationStrategies.h for thread-safe and diagnostic variants.
 */

#pragma once

#include <cstddef>
#include <cstring>
#include <new>
#include <type_traits>
#include <utility>

namespace fat_p {

/**
 * @brief Standard new/delete allocator with per-object heap allocation.
 *
 * Simple wrapper around operator new/delete. Each object is individually
 * allocated from the heap. This often provides better cache locality for
 * lookups due to malloc's allocation patterns.
 *
 * Supports over-aligned types (alignof(T) > alignof(std::max_align_t)) via
 * C++17 aligned new/delete. Examples: SIMD types, cache-line aligned structs.
 *
 * @tparam T Element type to allocate.
 *
 * @note Complexity: allocate() O(1) amortized, deallocate() O(1) amortized.
 * @note Memory overhead: malloc metadata per object (~16-32 bytes typical).
 * @note Thread-safety: NOT thread-safe. Caller must synchronize.
 *
 * @par Usage
 * @code
 * fat_p::NewDeleteAllocator<MyNode> alloc;
 * MyNode* p = alloc.allocate(arg1, arg2);  // Constructs in-place
 * alloc.deallocate(p);                      // Destroys and frees
 * @endcode
 */
template<typename T>
class NewDeleteAllocator
{
    static constexpr bool kIsOveraligned = alignof(T) > alignof(std::max_align_t);

public:
    NewDeleteAllocator() = default;
    NewDeleteAllocator(const NewDeleteAllocator&) = default;
    NewDeleteAllocator& operator=(const NewDeleteAllocator&) = default;
    NewDeleteAllocator(NewDeleteAllocator&&) noexcept = default;
    NewDeleteAllocator& operator=(NewDeleteAllocator&&) noexcept = default;
    ~NewDeleteAllocator() = default;

    /**
     * @brief Allocates and constructs an object.
     *
     * @tparam Args Constructor argument types.
     * @param args Arguments forwarded to T's constructor.
     * @return Pointer to newly constructed object.
     *
     * @throws std::bad_alloc if allocation fails.
     * @throws Any exception thrown by T's constructor.
     */
    template<typename... Args>
    T* allocate(Args&&... args)
    {
        if constexpr (kIsOveraligned)
        {
            // Over-aligned: use aligned allocation + placement new
            void* mem = ::operator new(sizeof(T), std::align_val_t{alignof(T)});
            return new (mem) T(std::forward<Args>(args)...);
        }
        else
        {
            // Normal alignment: standard new handles it
            return new T(std::forward<Args>(args)...);
        }
    }

    /**
     * @brief Destroys and deallocates an object.
     *
     * @param ptr Pointer to object previously returned by allocate().
     *
     * @pre ptr was returned by allocate() on this allocator.
     * @pre ptr has not already been deallocated.
     */
    void deallocate(T* ptr)
    {
        if constexpr (kIsOveraligned)
        {
            // Over-aligned: explicit destructor + aligned delete
            ptr->~T();
            ::operator delete(ptr, std::align_val_t{alignof(T)});
        }
        else
        {
            delete ptr;
        }
    }
};

/**
 * @brief Block allocator with contiguous storage and free list recycling.
 *
 * Allocates objects from contiguous blocks of memory. New allocations use
 * a bump pointer within the current block. Deallocated objects go to a
 * free list for reuse.
 *
 * Supports over-aligned types via alignas propagation - the Block struct
 * inherits T's alignment, and C++17 new handles over-aligned structs.
 *
 * @tparam T Element type to allocate. Must be at least sizeof(void*).
 *
 * @note Complexity: allocate() O(1), deallocate() O(1).
 * @note Memory overhead: ~8 bytes per block for linked list pointer.
 * @note Thread-safety: NOT thread-safe. Caller must synchronize.
 *
 * @par Trade-offs vs NewDeleteAllocator
 * - Faster allocation (no malloc per object)
 * - Faster deallocation (no free per object)
 * - Less memory fragmentation
 * - Objects scattered across blocks may hurt lookup cache locality
 * - Memory not returned to OS until allocator destroyed
 *
 * @par Usage
 * @code
 * fat_p::BlockAllocator<MyNode> alloc;
 * MyNode* p = alloc.allocate(arg1, arg2);
 * alloc.deallocate(p);
 * @endcode
 */
template<typename T>
class BlockAllocator
{
    static constexpr size_t kBlockSize = 256;  // Objects per block

    struct FreeNode
    {
        FreeNode* next;
    };

    // T must be at least as large as a pointer for free list to work
    static_assert(sizeof(T) >= sizeof(FreeNode*),
        "BlockAllocator<T>: T is too small (must be at least pointer-sized)");

    struct Block
    {
        alignas(alignof(T)) char data[kBlockSize * sizeof(T)];
        Block* next = nullptr;
    };

    Block* mHeadBlock = nullptr;
    FreeNode* mFreeList = nullptr;
    size_t mCurrentOffset = kBlockSize;  // Forces new block on first alloc

public:
    BlockAllocator() = default;

    // Non-copyable (stateful)
    BlockAllocator(const BlockAllocator&) = delete;
    BlockAllocator& operator=(const BlockAllocator&) = delete;

    /**
     * @brief Move constructor. Transfers ownership of all blocks.
     * @param other Source allocator (left empty after move).
     */
    BlockAllocator(BlockAllocator&& other) noexcept
        : mHeadBlock(other.mHeadBlock)
        , mFreeList(other.mFreeList)
        , mCurrentOffset(other.mCurrentOffset)
    {
        other.mHeadBlock = nullptr;
        other.mFreeList = nullptr;
        other.mCurrentOffset = kBlockSize;
    }

    /**
     * @brief Move assignment. Transfers ownership of all blocks.
     * @param other Source allocator (left empty after move).
     * @return Reference to this allocator.
     */
    BlockAllocator& operator=(BlockAllocator&& other) noexcept
    {
        if (this != &other)
        {
            destroyAllBlocks();
            mHeadBlock = other.mHeadBlock;
            mFreeList = other.mFreeList;
            mCurrentOffset = other.mCurrentOffset;
            other.mHeadBlock = nullptr;
            other.mFreeList = nullptr;
            other.mCurrentOffset = kBlockSize;
        }
        return *this;
    }

    ~BlockAllocator()
    {
        destroyAllBlocks();
    }

    /**
     * @brief Allocates and constructs an object.
     *
     * @tparam Args Constructor argument types.
     * @param args Arguments forwarded to T's constructor.
     * @return Pointer to newly constructed object.
     *
     * @throws std::bad_alloc if block allocation fails.
     * @throws Any exception thrown by T's constructor.
     */
    template<typename... Args>
    T* allocate(Args&&... args)
    {
        T* ptr = allocateRaw();
        new (ptr) T(std::forward<Args>(args)...);
        return ptr;
    }

    /**
     * @brief Destroys and recycles an object to the free list.
     *
     * @param ptr Pointer to object previously returned by allocate().
     *
     * @pre ptr was returned by allocate() on this allocator.
     * @pre ptr has not already been deallocated.
     */
    void deallocate(T* ptr)
    {
        ptr->~T();
        FreeNode* fn = reinterpret_cast<FreeNode*>(ptr);
        fn->next = mFreeList;
        mFreeList = fn;
    }

private:
    T* allocateRaw()
    {
        // Fast path: reuse from free list
        if (mFreeList)
        {
            FreeNode* fn = mFreeList;
            mFreeList = fn->next;
            return reinterpret_cast<T*>(fn);
        }

        // Allocate new block if needed
        if (mCurrentOffset >= kBlockSize)
        {
            Block* newBlock = new Block();
            newBlock->next = mHeadBlock;
            mHeadBlock = newBlock;
            mCurrentOffset = 0;
        }

        T* ptr = reinterpret_cast<T*>(
            mHeadBlock->data + mCurrentOffset * sizeof(T));
        ++mCurrentOffset;
        return ptr;
    }

    void destroyAllBlocks()
    {
        while (mHeadBlock)
        {
            Block* next = mHeadBlock->next;
            delete mHeadBlock;
            mHeadBlock = next;
        }
        mFreeList = nullptr;
        mCurrentOffset = kBlockSize;
    }
};

/**
 * @brief Fixed-size pre-allocated pool allocator.
 *
 * Pre-allocates a contiguous array of objects. Allocation pops from free list,
 * deallocation pushes to free list. Zero heap allocations after construction.
 *
 * @tparam MaxObjects Maximum number of objects the pool can hold.
 *
 * @note Complexity: allocate() O(1), deallocate() O(1).
 * @note Memory overhead: None beyond the pre-allocated pool.
 * @note Thread-safety: NOT thread-safe. Caller must synchronize.
 *
 * @par Trade-offs
 * - Fastest possible allocation (just pointer operations)
 * - Best cache locality (contiguous memory)
 * - No fragmentation
 * - Deterministic performance (no malloc calls)
 * - Fixed maximum capacity
 * - Memory allocated upfront even if unused
 * - Throws std::bad_alloc if pool exhausted
 *
 * @par Usage
 * @code
 * fat_p::PoolAllocator<1024>::Allocator<MyNode> alloc;
 * MyNode* p = alloc.allocate(args...);
 * alloc.deallocate(p);
 *
 * // Query pool status
 * size_t remaining = alloc.available();
 * bool isFull = alloc.full();
 * @endcode
 */
template<size_t MaxObjects>
struct PoolAllocator
{
    /**
     * @brief The actual allocator type for a specific element type.
     *
     * @tparam T Element type to allocate. Must be trivially copyable and
     *         at least sizeof(void*).
     */
    template<typename T>
    class Allocator
    {
        struct FreeNode
        {
            FreeNode* next;
        };

        // T must be at least as large as a pointer for free list to work
        static_assert(sizeof(T) >= sizeof(FreeNode*),
            "PoolAllocator<T>: T is too small (must be at least pointer-sized)");

        // Move operations use memcpy, which requires trivially copyable T
        // If you need non-trivial types, use NewDeleteAllocator or BlockAllocator
        static_assert(std::is_trivially_copyable_v<T> || MaxObjects == 0,
            "PoolAllocator<T>: T must be trivially copyable for move operations. "
            "Use NewDeleteAllocator or BlockAllocator for non-trivial types.");

        alignas(alignof(T)) char mStorage[MaxObjects * sizeof(T)];
        FreeNode* mFreeList = nullptr;
        size_t mAllocated = 0;
        bool mInitialized = false;

        void initialize()
        {
            if (mInitialized)
            {
                return;
            }
            // Build free list in reverse (LIFO order)
            for (size_t i = MaxObjects; i > 0; --i)
            {
                FreeNode* fn = reinterpret_cast<FreeNode*>(
                    mStorage + (i - 1) * sizeof(T));
                fn->next = mFreeList;
                mFreeList = fn;
            }
            mInitialized = true;
        }

    public:
        Allocator() = default;

        // Non-copyable (stateful with embedded storage)
        Allocator(const Allocator&) = delete;
        Allocator& operator=(const Allocator&) = delete;

        /**
         * @brief Move constructor. Copies storage and rebuilds free list.
         * @param other Source allocator (left empty after move).
         */
        Allocator(Allocator&& other) noexcept
            : mFreeList(nullptr)
            , mAllocated(other.mAllocated)
            , mInitialized(other.mInitialized)
        {
            if (mInitialized)
            {
                std::memcpy(mStorage, other.mStorage, MaxObjects * sizeof(T));
                // Rebuild free list for new storage location
                mFreeList = nullptr;
                FreeNode* otherCurrent = other.mFreeList;
                FreeNode** myTail = &mFreeList;
                while (otherCurrent)
                {
                    ptrdiff_t diff = reinterpret_cast<char*>(otherCurrent) - other.mStorage;
                    size_t offset = static_cast<size_t>(diff);
                    FreeNode* myNode = reinterpret_cast<FreeNode*>(mStorage + offset);
                    *myTail = myNode;
                    myTail = &myNode->next;
                    otherCurrent = otherCurrent->next;
                }
                *myTail = nullptr;
            }
            other.mFreeList = nullptr;
            other.mAllocated = 0;
            other.mInitialized = false;
        }

        /**
         * @brief Move assignment. Copies storage and rebuilds free list.
         * @param other Source allocator (left empty after move).
         * @return Reference to this allocator.
         */
        Allocator& operator=(Allocator&& other) noexcept
        {
            if (this != &other)
            {
                mAllocated = other.mAllocated;
                mInitialized = other.mInitialized;
                if (mInitialized)
                {
                    std::memcpy(mStorage, other.mStorage, MaxObjects * sizeof(T));
                    // Rebuild free list
                    mFreeList = nullptr;
                    FreeNode* otherCurrent = other.mFreeList;
                    FreeNode** myTail = &mFreeList;
                    while (otherCurrent)
                    {
                        ptrdiff_t diff = reinterpret_cast<char*>(otherCurrent) - other.mStorage;
                        size_t offset = static_cast<size_t>(diff);
                        FreeNode* myNode = reinterpret_cast<FreeNode*>(mStorage + offset);
                        *myTail = myNode;
                        myTail = &myNode->next;
                        otherCurrent = otherCurrent->next;
                    }
                    *myTail = nullptr;
                }
                other.mFreeList = nullptr;
                other.mAllocated = 0;
                other.mInitialized = false;
            }
            return *this;
        }

        ~Allocator() = default;

        /**
         * @brief Allocates and constructs an object from the pool.
         *
         * @tparam Args Constructor argument types.
         * @param args Arguments forwarded to T's constructor.
         * @return Pointer to newly constructed object.
         *
         * @throws std::bad_alloc if pool is exhausted.
         * @throws Any exception thrown by T's constructor.
         */
        template<typename... Args>
        T* allocate(Args&&... args)
        {
            initialize();
            if (!mFreeList)
            {
                throw std::bad_alloc();  // Pool exhausted
            }
            FreeNode* fn = mFreeList;
            mFreeList = fn->next;
            ++mAllocated;
            T* ptr = reinterpret_cast<T*>(fn);
            new (ptr) T(std::forward<Args>(args)...);
            return ptr;
        }

        /**
         * @brief Destroys and returns an object to the pool.
         *
         * @param ptr Pointer to object previously returned by allocate().
         *
         * @pre ptr was returned by allocate() on this allocator.
         * @pre ptr has not already been deallocated.
         */
        void deallocate(T* ptr)
        {
            ptr->~T();
            FreeNode* fn = reinterpret_cast<FreeNode*>(ptr);
            fn->next = mFreeList;
            mFreeList = fn;
            --mAllocated;
        }

        /// @brief Returns the maximum number of objects the pool can hold.
        [[nodiscard]] static constexpr size_t capacity() { return MaxObjects; }

        /// @brief Returns the number of currently allocated objects.
        [[nodiscard]] size_t allocated() const { return mAllocated; }

        /// @brief Returns the number of available slots in the pool.
        [[nodiscard]] size_t available() const { return MaxObjects - mAllocated; }

        /// @brief Returns true if the pool is fully allocated.
        [[nodiscard]] bool full() const { return mAllocated >= MaxObjects; }
    };
};

} // namespace fat_p
