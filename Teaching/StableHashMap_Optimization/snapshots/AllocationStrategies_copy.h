// AllocationStrategies.h - Simple allocator policies with zero dependencies
//
// Provides lightweight allocator policies for containers like StableHashMap.
// For full-featured allocators with DbC, thread-safety wrappers, diagnostics,
// and Expected-based error handling, see FatPAllocationStrategies.h.
//
// Allocator Policies:
//
//   NewDeleteAllocator (recommended default)
//     - Standard new/delete per object
//     - Best cache locality for lookups (malloc's allocation patterns)
//     - Simple, well-understood behavior
//     - Best for: Lookup-heavy workloads, general use
//
//   BlockAllocator
//     - Allocates objects in contiguous 256-object blocks
//     - Fast allocation (bump pointer) and deallocation (free list)
//     - May hurt lookup cache locality (objects scattered across blocks)
//     - Best for: Insert/erase-heavy workloads, churn
//
//   PoolAllocator<MaxObjects>
//     - Pre-allocated contiguous pool with free list
//     - Fastest allocation after warmup (just pointer ops)
//     - Excellent cache locality
//     - Fixed maximum capacity (throws std::bad_alloc if exceeded)
//     - Best for: Fixed-size containers with known bounds
//
// Usage with StableHashMap:
//
//   // Default (NewDeleteAllocator) - best for most use cases
//   fat_p::StableHashMap<K, V> map1;
//
//   // BlockAllocator for insert-heavy workloads
//   fat_p::StableHashMap<K, V, std::hash<K>, std::equal_to<K>,
//                        fat_p::BlockAllocator> map2;
//
//   // PoolAllocator for fixed-size, maximum performance
//   fat_p::StableHashMap<K, V, std::hash<K>, std::equal_to<K>,
//                        fat_p::PoolAllocator<10000>::template Allocator> map3;
//
// Allocator Concept Requirements:
//   - Allocator<T> is default constructible
//   - T* allocate(Args&&... args) - allocate and construct in-place
//   - void deallocate(T* ptr) - destroy and recycle memory
//   - Move constructible and move assignable
//   - Copy operations deleted (stateful allocators)
//
// Thread Safety:
//   These allocators are NOT thread-safe. For thread-safe allocators,
//   see FatPAllocationStrategies.h which provides SynchronizedWrapper
//   and LockFreeWrapper policies.
//
#pragma once

#include <cstddef>
#include <cstring>
#include <new>
#include <type_traits>
#include <utility>

namespace fat_p
{

// ============================================================================
// NewDeleteAllocator - Standard new/delete per object
// ============================================================================
//
// Simple wrapper around operator new/delete. Each object is individually
// allocated from the heap. This often provides better cache locality for
// lookups due to malloc's allocation patterns.
//
// Supports over-aligned types (alignof(T) > alignof(std::max_align_t)) via
// C++17 aligned new/delete. Examples: SIMD types, cache-line aligned structs.
//
// Complexity:
//   allocate:   O(1) amortized (malloc)
//   deallocate: O(1) amortized (free)
//
// Memory overhead: malloc metadata per object (~16-32 bytes typical)
//
template <typename T>
class NewDeleteAllocator
{
    static constexpr bool is_overaligned = alignof(T) > alignof(std::max_align_t);

public:
    NewDeleteAllocator() = default;
    NewDeleteAllocator(const NewDeleteAllocator&) = default;
    NewDeleteAllocator& operator=(const NewDeleteAllocator&) = default;
    NewDeleteAllocator(NewDeleteAllocator&&) noexcept = default;
    NewDeleteAllocator& operator=(NewDeleteAllocator&&) noexcept = default;
    ~NewDeleteAllocator() = default;

    template <typename... Args>
    T* allocate(Args&&... args)
    {
        if constexpr (is_overaligned)
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

    void deallocate(T* ptr)
    {
        if constexpr (is_overaligned)
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

// ============================================================================
// BlockAllocator - Allocates objects in contiguous blocks
// ============================================================================
//
// Allocates objects from contiguous blocks of memory. New allocations use
// a bump pointer within the current block. Deallocated objects go to a
// free list for reuse.
//
// Supports over-aligned types via alignas propagation - the Block struct
// inherits T's alignment, and C++17 new handles over-aligned structs.
//
// Complexity:
//   allocate:   O(1) (bump pointer or free list pop)
//   deallocate: O(1) (free list push)
//
// Memory overhead: ~8 bytes per block for linked list pointer
//
// Trade-offs vs NewDeleteAllocator:
//   + Faster allocation (no malloc per object)
//   + Faster deallocation (no free per object)
//   + Less memory fragmentation
//   - Objects scattered across blocks may hurt lookup cache locality
//   - Memory not returned to OS until allocator destroyed
//
template <typename T>
class BlockAllocator
{
    static constexpr size_t kBlockSize = 256; // Objects per block

    struct FreeNode
    {
        FreeNode* next;
    };

    // T must be at least as large as a pointer for free list to work
    static_assert(sizeof(T) >= sizeof(FreeNode*), "BlockAllocator<T>: T is too small (must be at least pointer-sized)");

    struct Block
    {
        alignas(alignof(T)) char data[kBlockSize * sizeof(T)];
        Block* next = nullptr;
    };

    Block* head_block_ = nullptr;
    FreeNode* free_list_ = nullptr;
    size_t current_offset_ = kBlockSize; // Forces new block on first alloc

public:
    BlockAllocator() = default;

    // Non-copyable (stateful)
    BlockAllocator(const BlockAllocator&) = delete;
    BlockAllocator& operator=(const BlockAllocator&) = delete;

    BlockAllocator(BlockAllocator&& other) noexcept
        : head_block_(other.head_block_)
        , free_list_(other.free_list_)
        , current_offset_(other.current_offset_)
    {
        other.head_block_ = nullptr;
        other.free_list_ = nullptr;
        other.current_offset_ = kBlockSize;
    }

    BlockAllocator& operator=(BlockAllocator&& other) noexcept
    {
        if (this != &other)
        {
            destroy_all_blocks();
            head_block_ = other.head_block_;
            free_list_ = other.free_list_;
            current_offset_ = other.current_offset_;
            other.head_block_ = nullptr;
            other.free_list_ = nullptr;
            other.current_offset_ = kBlockSize;
        }
        return *this;
    }

    ~BlockAllocator()
    {
        destroy_all_blocks();
    }

    template <typename... Args>
    T* allocate(Args&&... args)
    {
        T* ptr = allocate_raw();
        new (ptr) T(std::forward<Args>(args)...);
        return ptr;
    }

    void deallocate(T* ptr)
    {
        ptr->~T();
        FreeNode* fn = reinterpret_cast<FreeNode*>(ptr);
        fn->next = free_list_;
        free_list_ = fn;
    }

private:
    T* allocate_raw()
    {
        // Fast path: reuse from free list
        if (free_list_)
        {
            FreeNode* fn = free_list_;
            free_list_ = fn->next;
            return reinterpret_cast<T*>(fn);
        }

        // Allocate new block if needed
        if (current_offset_ >= kBlockSize)
        {
            Block* new_block = new Block();
            new_block->next = head_block_;
            head_block_ = new_block;
            current_offset_ = 0;
        }

        T* ptr = reinterpret_cast<T*>(head_block_->data + current_offset_ * sizeof(T));
        ++current_offset_;
        return ptr;
    }

    void destroy_all_blocks()
    {
        while (head_block_)
        {
            Block* next = head_block_->next;
            delete head_block_;
            head_block_ = next;
        }
        free_list_ = nullptr;
        current_offset_ = kBlockSize;
    }
};

// ============================================================================
// PoolAllocator - Fixed-size pre-allocated pool
// ============================================================================
//
// Pre-allocates a contiguous array of objects. Allocation pops from free list,
// deallocation pushes to free list. Zero heap allocations after construction.
//
// Complexity:
//   allocate:   O(1) (free list pop)
//   deallocate: O(1) (free list push)
//
// Memory overhead: None beyond the pre-allocated pool
//
// Trade-offs:
//   + Fastest possible allocation
//   + Best cache locality (contiguous memory)
//   + No fragmentation
//   + Deterministic performance (no malloc calls)
//   - Fixed maximum capacity
//   - Memory allocated upfront even if unused
//   - Throws std::bad_alloc if pool exhausted
//
// Usage:
//   fat_p::PoolAllocator<1024>::Allocator<MyNode> alloc;
//   MyNode* p = alloc.allocate(args...);
//   alloc.deallocate(p);
//
template <size_t MaxObjects>
struct PoolAllocator
{
    template <typename T>
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
        // instead
        static_assert(std::is_trivially_copyable_v<T> || MaxObjects == 0,
                      "PoolAllocator<T>: T must be trivially copyable for move operations. "
                      "Use NewDeleteAllocator or BlockAllocator for non-trivial types.");

        alignas(alignof(T)) char storage_[MaxObjects * sizeof(T)];
        FreeNode* free_list_ = nullptr;
        size_t allocated_ = 0;
        bool initialized_ = false;

        void initialize()
        {
            if (initialized_)
            {
                return;
            }
            // Build free list in reverse (LIFO order)
            for (size_t i = MaxObjects; i > 0; --i)
            {
                FreeNode* fn = reinterpret_cast<FreeNode*>(storage_ + (i - 1) * sizeof(T));
                fn->next = free_list_;
                free_list_ = fn;
            }
            initialized_ = true;
        }

    public:
        Allocator() = default;

        // Non-copyable (stateful with embedded storage)
        Allocator(const Allocator&) = delete;
        Allocator& operator=(const Allocator&) = delete;

        Allocator(Allocator&& other) noexcept
            : free_list_(nullptr)
            , allocated_(other.allocated_)
            , initialized_(other.initialized_)
        {
            if (initialized_)
            {
                std::memcpy(storage_, other.storage_, MaxObjects * sizeof(T));
                // Rebuild free list for new storage location
                free_list_ = nullptr;
                FreeNode* other_current = other.free_list_;
                FreeNode** my_tail = &free_list_;
                while (other_current)
                {
                    size_t offset = reinterpret_cast<char*>(other_current) - other.storage_;
                    FreeNode* my_node = reinterpret_cast<FreeNode*>(storage_ + offset);
                    *my_tail = my_node;
                    my_tail = &my_node->next;
                    other_current = other_current->next;
                }
                *my_tail = nullptr;
            }
            other.free_list_ = nullptr;
            other.allocated_ = 0;
            other.initialized_ = false;
        }

        Allocator& operator=(Allocator&& other) noexcept
        {
            if (this != &other)
            {
                allocated_ = other.allocated_;
                initialized_ = other.initialized_;
                if (initialized_)
                {
                    std::memcpy(storage_, other.storage_, MaxObjects * sizeof(T));
                    // Rebuild free list
                    free_list_ = nullptr;
                    FreeNode* other_current = other.free_list_;
                    FreeNode** my_tail = &free_list_;
                    while (other_current)
                    {
                        size_t offset = reinterpret_cast<char*>(other_current) - other.storage_;
                        FreeNode* my_node = reinterpret_cast<FreeNode*>(storage_ + offset);
                        *my_tail = my_node;
                        my_tail = &my_node->next;
                        other_current = other_current->next;
                    }
                    *my_tail = nullptr;
                }
                other.free_list_ = nullptr;
                other.allocated_ = 0;
                other.initialized_ = false;
            }
            return *this;
        }

        ~Allocator() = default;

        template <typename... Args>
        T* allocate(Args&&... args)
        {
            initialize();
            if (!free_list_)
            {
                throw std::bad_alloc(); // Pool exhausted
            }
            FreeNode* fn = free_list_;
            free_list_ = fn->next;
            ++allocated_;
            T* ptr = reinterpret_cast<T*>(fn);
            new (ptr) T(std::forward<Args>(args)...);
            return ptr;
        }

        void deallocate(T* ptr)
        {
            ptr->~T();
            FreeNode* fn = reinterpret_cast<FreeNode*>(ptr);
            fn->next = free_list_;
            free_list_ = fn;
            --allocated_;
        }

        // Pool status queries
        static constexpr size_t capacity()
        {
            return MaxObjects;
        }
        size_t allocated() const
        {
            return allocated_;
        }
        size_t available() const
        {
            return MaxObjects - allocated_;
        }
        bool full() const
        {
            return allocated_ >= MaxObjects;
        }
    };
};

} // namespace fat_p
