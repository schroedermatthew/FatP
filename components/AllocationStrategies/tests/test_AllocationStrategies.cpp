/**
 * @file test_AllocationStrategies.cpp
 * @brief Comprehensive unit tests for fat_p::AllocationStrategies
 *
 * Tests the three allocator policies:
 * - NewDeleteAllocator: Standard new/delete per object
 * - BlockAllocator: Contiguous block allocation with bump pointer
 * - PoolAllocator: Fixed-size pre-allocated pool
 *
 * @version 2.1
 */
/*
FATP_META:
  meta_version: 1
  component: AllocationStrategies
  file_role: test
  path: components/AllocationStrategies/tests/test_AllocationStrategies.cpp
  layer: Testing
  namespace: fat_p
  summary: "Unit tests for AllocationStrategies."
  api_stability: in_work
  related:
    docs_search: "AllocationStrategies"
    headers:
      - include/fat_p/AllocationStrategies.h
      - include/fat_p/FatPTest.h
  hygiene:
    pragma_once: false
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
#include <array>
#include <atomic>
#include <cstdint>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "AllocationStrategies.h"
#include "FatPTest.h"

namespace fat_p::testing::allocationns
{

// ============================================================================
// Helper Types
// ============================================================================

/**
 * @brief Test class for tracking construction/destruction lifecycle
 */
class LifecycleTracker
{
public:
    static inline std::atomic<int> sConstructCount{0};
    static inline std::atomic<int> sDestructCount{0};
    static inline std::atomic<int> sCopyCount{0};
    static inline std::atomic<int> sMoveCount{0};

    int64_t mValue; // Ensures type is at least pointer-sized

    explicit LifecycleTracker(int v = 0)
        : mValue(v)
    {
        sConstructCount.fetch_add(1, std::memory_order_relaxed);
    }

    LifecycleTracker(const LifecycleTracker& other)
        : mValue(other.mValue)
    {
        sCopyCount.fetch_add(1, std::memory_order_relaxed);
    }

    LifecycleTracker(LifecycleTracker&& other) noexcept
        : mValue(other.mValue)
    {
        sMoveCount.fetch_add(1, std::memory_order_relaxed);
    }

    LifecycleTracker& operator=(const LifecycleTracker& other)
    {
        if (this != &other)
        {
            mValue = other.mValue;
            sCopyCount.fetch_add(1, std::memory_order_relaxed);
        }
        return *this;
    }

    LifecycleTracker& operator=(LifecycleTracker&& other) noexcept
    {
        if (this != &other)
        {
            mValue = other.mValue;
            sMoveCount.fetch_add(1, std::memory_order_relaxed);
        }
        return *this;
    }

    ~LifecycleTracker()
    {
        sDestructCount.fetch_add(1, std::memory_order_relaxed);
    }

    static void reset() noexcept
    {
        sConstructCount.store(0, std::memory_order_relaxed);
        sDestructCount.store(0, std::memory_order_relaxed);
        sCopyCount.store(0, std::memory_order_relaxed);
        sMoveCount.store(0, std::memory_order_relaxed);
    }
};

/**
 * @brief Trivially copyable type for PoolAllocator testing
 */
struct TrivialNode
{
    int mKey;
    int mValue;

    TrivialNode()
        : mKey(0)
        , mValue(0)
    {
    }
    TrivialNode(int k, int v)
        : mKey(k)
        , mValue(v)
    {
    }
};
static_assert(std::is_trivially_copyable_v<TrivialNode>);

/**
 * @brief Over-aligned type for alignment testing
 */
struct alignas(64) CacheAligned
{
    char mData[64];

    CacheAligned()
    {
        mData[0] = 0;
    }
    explicit CacheAligned(char c)
    {
        mData[0] = c;
    }
};

/**
 * @brief Type whose constructor conditionally throws, for exception safety testing.
 *
 * When sThrowOnNext is true, the constructor throws before completing.
 * Tracks construction/destruction counts so tests can verify no leak.
 */
struct ThrowingType
{
    static inline std::atomic<int> sConstructCount{0};
    static inline std::atomic<int> sDestructCount{0};
    static inline bool sThrowOnNext{false};

    int64_t mValue;

    explicit ThrowingType(int v = 0)
        : mValue(v)
    {
        if (sThrowOnNext)
        {
            throw std::runtime_error("ThrowingType: constructor throw");
        }
        sConstructCount.fetch_add(1, std::memory_order_relaxed);
    }

    ~ThrowingType()
    {
        sDestructCount.fetch_add(1, std::memory_order_relaxed);
    }

    ThrowingType(const ThrowingType&) = delete;
    ThrowingType& operator=(const ThrowingType&) = delete;

    static void reset() noexcept
    {
        sConstructCount.store(0, std::memory_order_relaxed);
        sDestructCount.store(0, std::memory_order_relaxed);
        sThrowOnNext = false;
    }
};

/**
 * @brief Trivially copyable type whose constructor conditionally throws.
 *
 * PoolAllocator requires trivially copyable types, so this type has no
 * user-defined destructor/copy/move. Construction tracking uses a static
 * counter; no per-instance destruction tracking is possible.
 */
struct PoolThrowingType
{
    static inline std::atomic<int> sConstructCount{0};
    static inline bool sThrowOnNext{false};

    int64_t mValue;

    explicit PoolThrowingType(int v = 0)
        : mValue(v)
    {
        if (sThrowOnNext)
        {
            throw std::runtime_error("PoolThrowingType: constructor throw");
        }
        sConstructCount.fetch_add(1, std::memory_order_relaxed);
    }

    static void reset() noexcept
    {
        sConstructCount.store(0, std::memory_order_relaxed);
        sThrowOnNext = false;
    }
};
static_assert(std::is_trivially_copyable_v<PoolThrowingType>);

// ============================================================================
// Test Suite 1: NewDeleteAllocator
// ============================================================================

FATP_TEST_CASE(newdelete_basic)
{
    NewDeleteAllocator<int> alloc;

    int* ptr = alloc.allocate(42);
    FATP_ASSERT_NOT_NULLPTR(ptr, "Allocation should succeed");
    FATP_ASSERT_EQ(*ptr, 42, "Value should be constructed in-place");

    alloc.deallocate(ptr);
    return true;
}

FATP_TEST_CASE(newdelete_multiple_allocations)
{
    NewDeleteAllocator<int> alloc;
    std::vector<int*> ptrs;

    constexpr int kCount = 100;
    for (int i = 0; i < kCount; ++i)
    {
        int* ptr = alloc.allocate(i * 10);
        FATP_ASSERT_NOT_NULLPTR(ptr, "Allocation should succeed");
        FATP_ASSERT_EQ(*ptr, i * 10, "Value should match");
        ptrs.push_back(ptr);
    }

    for (auto ptr : ptrs)
    {
        alloc.deallocate(ptr);
    }

    return true;
}

FATP_TEST_CASE(newdelete_lifecycle_tracking)
{
    LifecycleTracker::reset();

    NewDeleteAllocator<LifecycleTracker> alloc;

    LifecycleTracker* ptr = alloc.allocate(42);
    FATP_ASSERT_EQ(LifecycleTracker::sConstructCount.load(), 1, "One construction");
    FATP_ASSERT_EQ(ptr->mValue, 42, "Value should be set");

    alloc.deallocate(ptr);
    FATP_ASSERT_EQ(LifecycleTracker::sDestructCount.load(), 1, "One destruction");

    return true;
}

FATP_TEST_CASE(newdelete_over_aligned)
{
    NewDeleteAllocator<CacheAligned> alloc;

    CacheAligned* ptr = alloc.allocate('X');
    FATP_ASSERT_NOT_NULLPTR(ptr, "Aligned allocation should succeed");

    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    FATP_ASSERT_EQ(addr % 64, size_t(0), "Should be 64-byte aligned");
    FATP_ASSERT_EQ(ptr->mData[0], 'X', "Value should be constructed");

    alloc.deallocate(ptr);
    return true;
}

FATP_TEST_CASE(newdelete_copy_move)
{
    NewDeleteAllocator<int> alloc1;

    // Copy construction
    NewDeleteAllocator<int> alloc2(alloc1);
    int* ptr = alloc2.allocate(123);
    FATP_ASSERT_EQ(*ptr, 123, "Copied allocator should work");
    alloc2.deallocate(ptr);

    // Move construction
    NewDeleteAllocator<int> alloc3(std::move(alloc1));
    ptr = alloc3.allocate(456);
    FATP_ASSERT_EQ(*ptr, 456, "Moved allocator should work");
    alloc3.deallocate(ptr);

    return true;
}

// ============================================================================
// Test Suite 2: BlockAllocator
// ============================================================================

FATP_TEST_CASE(block_basic)
{
    BlockAllocator<LifecycleTracker> alloc;
    LifecycleTracker::reset();

    LifecycleTracker* ptr = alloc.allocate(42);
    FATP_ASSERT_NOT_NULLPTR(ptr, "Block allocation should succeed");
    FATP_ASSERT_EQ(ptr->mValue, 42, "Value should be constructed");
    FATP_ASSERT_EQ(LifecycleTracker::sConstructCount.load(), 1, "One construction");

    alloc.deallocate(ptr);
    FATP_ASSERT_EQ(LifecycleTracker::sDestructCount.load(), 1, "One destruction");

    return true;
}

FATP_TEST_CASE(block_multiple_allocations)
{
    BlockAllocator<LifecycleTracker> alloc;
    LifecycleTracker::reset();
    std::vector<LifecycleTracker*> ptrs;

    constexpr int kCount = 500; // More than one block (256 per block)

    for (int i = 0; i < kCount; ++i)
    {
        LifecycleTracker* ptr = alloc.allocate(i);
        FATP_ASSERT_NOT_NULLPTR(ptr, "Allocation should succeed");
        FATP_ASSERT_EQ(ptr->mValue, i, "Value should match");
        ptrs.push_back(ptr);
    }

    FATP_ASSERT_EQ(LifecycleTracker::sConstructCount.load(), kCount, "All constructions");

    for (auto ptr : ptrs)
    {
        alloc.deallocate(ptr);
    }

    FATP_ASSERT_EQ(LifecycleTracker::sDestructCount.load(), kCount, "All destructions");

    return true;
}

FATP_TEST_CASE(block_free_list_reuse)
{
    BlockAllocator<LifecycleTracker> alloc;
    LifecycleTracker::reset();

    // Allocate
    LifecycleTracker* ptr1 = alloc.allocate(1);

    // Deallocate - goes to free list
    alloc.deallocate(ptr1);

    // Allocate again - should reuse from free list
    LifecycleTracker* ptr2 = alloc.allocate(2);

    // Due to LIFO free list, should get same memory back
    FATP_ASSERT_EQ(ptr1, ptr2, "Should reuse deallocated memory");
    FATP_ASSERT_EQ(ptr2->mValue, 2, "Value should be newly constructed");

    alloc.deallocate(ptr2);
    return true;
}

FATP_TEST_CASE(block_move_semantics)
{
    BlockAllocator<LifecycleTracker> alloc1;
    LifecycleTracker::reset();

    // Allocate from first allocator
    LifecycleTracker* ptr = alloc1.allocate(42);
    FATP_ASSERT_EQ(ptr->mValue, 42, "Initial allocation");

    // Move construct - transfers ownership
    BlockAllocator<LifecycleTracker> alloc2(std::move(alloc1));

    // Can still use the pointer through moved allocator
    FATP_ASSERT_EQ(ptr->mValue, 42, "Value preserved after move");

    // Deallocate through new owner
    alloc2.deallocate(ptr);
    FATP_ASSERT_EQ(LifecycleTracker::sDestructCount.load(), 1, "Destruction via new owner");

    return true;
}

FATP_TEST_CASE(block_alignment)
{
    BlockAllocator<CacheAligned> alloc;

    CacheAligned* ptr = alloc.allocate('A');
    FATP_ASSERT_NOT_NULLPTR(ptr, "Aligned allocation should succeed");

    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    FATP_ASSERT_EQ(addr % 64, size_t(0), "Should be 64-byte aligned");

    alloc.deallocate(ptr);
    return true;
}

// ============================================================================
// Test Suite 3: PoolAllocator
// ============================================================================

FATP_TEST_CASE(pool_basic)
{
    PoolAllocator<100>::Allocator<TrivialNode> alloc;

    FATP_ASSERT_EQ(alloc.capacity(), size_t(100), "Capacity should match template");
    FATP_ASSERT_EQ(alloc.allocated(), size_t(0), "Initially none allocated");
    FATP_ASSERT_EQ(alloc.available(), size_t(100), "All available initially");
    FATP_ASSERT_FALSE(alloc.full(), "Should not be full");

    TrivialNode* ptr = alloc.allocate(1, 100);
    FATP_ASSERT_NOT_NULLPTR(ptr, "Allocation should succeed");
    FATP_ASSERT_EQ(ptr->mKey, 1, "Key should match");
    FATP_ASSERT_EQ(ptr->mValue, 100, "Value should match");

    FATP_ASSERT_EQ(alloc.allocated(), size_t(1), "One allocated");
    FATP_ASSERT_EQ(alloc.available(), size_t(99), "99 remaining");

    alloc.deallocate(ptr);
    FATP_ASSERT_EQ(alloc.allocated(), size_t(0), "None allocated after dealloc");

    return true;
}

FATP_TEST_CASE(pool_free_list_reuse)
{
    PoolAllocator<10>::Allocator<TrivialNode> alloc;

    TrivialNode* ptr1 = alloc.allocate(1, 10);
    alloc.deallocate(ptr1);

    TrivialNode* ptr2 = alloc.allocate(2, 20);

    // Free list LIFO: should get same memory back
    FATP_ASSERT_EQ(ptr1, ptr2, "Should reuse deallocated slot");
    FATP_ASSERT_EQ(ptr2->mKey, 2, "New value constructed");

    alloc.deallocate(ptr2);
    return true;
}

FATP_TEST_CASE(pool_exhaustion)
{
    PoolAllocator<5>::Allocator<TrivialNode> alloc;
    std::vector<TrivialNode*> ptrs;

    // Allocate all 5 slots
    for (int i = 0; i < 5; ++i)
    {
        TrivialNode* ptr = alloc.allocate(i, i * 10);
        FATP_ASSERT_NOT_NULLPTR(ptr, "Allocation should succeed while pool has space");
        ptrs.push_back(ptr);
    }

    FATP_ASSERT_TRUE(alloc.full(), "Pool should be full");
    FATP_ASSERT_EQ(alloc.available(), size_t(0), "No slots available");

    // Next allocation should throw
    bool threw = false;
    try
    {
        alloc.allocate(99, 999);
    }
    catch (const std::bad_alloc&)
    {
        threw = true;
    }
    FATP_ASSERT_TRUE(threw, "Should throw std::bad_alloc when exhausted");

    // Cleanup
    for (auto ptr : ptrs)
    {
        alloc.deallocate(ptr);
    }

    FATP_ASSERT_FALSE(alloc.full(), "Pool no longer full after deallocation");

    return true;
}

FATP_TEST_CASE(pool_full_capacity_cycle)
{
    PoolAllocator<100>::Allocator<TrivialNode> alloc;
    std::vector<TrivialNode*> ptrs;

    // Fill the pool
    for (int i = 0; i < 100; ++i)
    {
        ptrs.push_back(alloc.allocate(i, i));
    }
    FATP_ASSERT_EQ(alloc.allocated(), size_t(100), "All allocated");

    // Empty the pool
    for (auto ptr : ptrs)
    {
        alloc.deallocate(ptr);
    }
    FATP_ASSERT_EQ(alloc.allocated(), size_t(0), "All deallocated");

    // Refill - should work with free list
    ptrs.clear();
    for (int i = 0; i < 100; ++i)
    {
        TrivialNode* ptr = alloc.allocate(i + 100, i + 100);
        FATP_ASSERT_NOT_NULLPTR(ptr, "Re-allocation should succeed");
        ptrs.push_back(ptr);
    }

    for (auto ptr : ptrs)
    {
        alloc.deallocate(ptr);
    }

    return true;
}

FATP_TEST_CASE(pool_move_semantics)
{
    PoolAllocator<10>::Allocator<TrivialNode> alloc1;

    // Allocate from first
    TrivialNode* ptr1 = alloc1.allocate(1, 10);
    TrivialNode* ptr2 = alloc1.allocate(2, 20);
    FATP_ASSERT_EQ(alloc1.allocated(), size_t(2), "Two allocated");

    // Move construct
    PoolAllocator<10>::Allocator<TrivialNode> alloc2(std::move(alloc1));

    // State transferred
    FATP_ASSERT_EQ(alloc2.allocated(), size_t(2), "Allocation count transferred");

    // Source should be reset
    FATP_ASSERT_EQ(alloc1.allocated(), size_t(0), "Source reset after move");

    // Can continue using moved allocator
    TrivialNode* ptr3 = alloc2.allocate(3, 30);
    FATP_ASSERT_EQ(ptr3->mKey, 3, "New allocation works");

    alloc2.deallocate(ptr3);
    alloc2.deallocate(ptr2);
    alloc2.deallocate(ptr1);

    return true;
}

// ============================================================================
// Test Suite 4: Edge Cases
// ============================================================================

FATP_TEST_CASE(edge_default_construction)
{
    // All allocators should be default constructible
    NewDeleteAllocator<int> nda;
    BlockAllocator<LifecycleTracker> ba;
    PoolAllocator<10>::Allocator<TrivialNode> pa;

    // Should be usable immediately
    int* p1 = nda.allocate(1);
    nda.deallocate(p1);

    LifecycleTracker::reset();
    LifecycleTracker* p2 = ba.allocate(2);
    ba.deallocate(p2);

    TrivialNode* p3 = pa.allocate(3, 30);
    pa.deallocate(p3);

    return true;
}

FATP_TEST_CASE(edge_single_element_type)
{
    // Smallest valid type that can hold a pointer
    struct SmallType
    {
        void* mData;
        SmallType()
            : mData(nullptr)
        {
        }
        explicit SmallType(void* p)
            : mData(p)
        {
        }
    };
    static_assert(sizeof(SmallType) >= sizeof(void*));

    BlockAllocator<SmallType> alloc;

    SmallType* ptr = alloc.allocate(nullptr);
    FATP_ASSERT_NOT_NULLPTR(ptr, "Should allocate small type");
    FATP_ASSERT_NULLPTR(ptr->mData, "Value should be default");

    alloc.deallocate(ptr);
    return true;
}

FATP_TEST_CASE(edge_destructor_cleanup)
{
    LifecycleTracker::reset();

    {
        BlockAllocator<LifecycleTracker> alloc;

        // Allocate several objects
        for (int i = 0; i < 10; ++i)
        {
            alloc.allocate(i);
        }

        FATP_ASSERT_EQ(LifecycleTracker::sConstructCount.load(), 10, "All constructed");
        // Note: We intentionally don't deallocate - allocator destructor
        // will free the blocks but not call destructors on live objects
        // This is by design - allocator manages memory, not object lifetime
    }

    // Destructor freed blocks but didn't call object destructors
    // This is expected behavior - caller must deallocate to invoke destructors

    return true;
}

// ============================================================================
// Test Suite 5: Exception Safety (Regression for placement-new leak fix)
// ============================================================================
// BlockAllocator::allocate() and PoolAllocator::Allocator::allocate() previously
// leaked the raw memory slot when T's constructor threw: allocateRaw() succeeded
// but placement-new threw, and the slot was never returned to the free list.
// PoolAllocator additionally corrupted mAllocated (incremented before construction).
// The fix wraps placement-new in try/catch and moves ++mAllocated after construction.

FATP_TEST_CASE(block_exception_safety_slot_recovery)
{
    ThrowingType::reset();
    BlockAllocator<ThrowingType> alloc;

    // Allocate one object successfully
    ThrowingType* p1 = alloc.allocate(1);
    FATP_ASSERT_NOT_NULLPTR(p1, "First allocation must succeed");
    FATP_ASSERT_EQ(ThrowingType::sConstructCount.load(), 1, "One successful construction");

    // Now make the constructor throw
    ThrowingType::sThrowOnNext = true;
    bool threw = false;
    try
    {
        alloc.allocate(2);
    }
    catch (const std::runtime_error&)
    {
        threw = true;
    }
    FATP_ASSERT_TRUE(threw, "Constructor throw must propagate");
    FATP_ASSERT_EQ(ThrowingType::sConstructCount.load(), 1, "Failed construction must not increment count");

    // The raw slot from the failed allocation must have been returned to the
    // free list. Verify by allocating again — this must succeed and reuse the slot.
    ThrowingType::sThrowOnNext = false;
    ThrowingType* p2 = alloc.allocate(3);
    FATP_ASSERT_NOT_NULLPTR(p2, "Post-throw allocation must succeed (slot recovered)");
    FATP_ASSERT_EQ(ThrowingType::sConstructCount.load(), 2, "Second successful construction");

    alloc.deallocate(p2);
    alloc.deallocate(p1);

    FATP_ASSERT_EQ(ThrowingType::sDestructCount.load(), 2, "Both objects destroyed cleanly");

    return true;
}

FATP_TEST_CASE(block_exception_safety_repeated_throws)
{
    ThrowingType::reset();
    BlockAllocator<ThrowingType> alloc;

    // Allocate a few objects, then throw repeatedly, then allocate again.
    // Every failed slot must be recoverable.
    constexpr int kGoodAllocs = 5;
    constexpr int kFailedAllocs = 10;

    std::vector<ThrowingType*> good;
    for (int i = 0; i < kGoodAllocs; ++i)
    {
        good.push_back(alloc.allocate(i));
    }

    // Throw 10 times — each should return its slot
    ThrowingType::sThrowOnNext = true;
    for (int i = 0; i < kFailedAllocs; ++i)
    {
        try
        {
            alloc.allocate(100 + i);
        }
        catch (const std::runtime_error&)
        {
        }
    }
    FATP_ASSERT_EQ(ThrowingType::sConstructCount.load(), kGoodAllocs,
        "Only successful constructions counted");

    // Now allocate again — all 10 recovered slots plus fresh ones must work
    ThrowingType::sThrowOnNext = false;
    for (int i = 0; i < kFailedAllocs + 5; ++i)
    {
        good.push_back(alloc.allocate(200 + i));
    }
    FATP_ASSERT_EQ(ThrowingType::sConstructCount.load(), kGoodAllocs + kFailedAllocs + 5,
        "All post-recovery allocations must succeed");

    for (auto* p : good)
    {
        alloc.deallocate(p);
    }
    return true;
}

FATP_TEST_CASE(pool_exception_safety_slot_recovery)
{
    PoolThrowingType::reset();
    PoolAllocator<16>::Allocator<PoolThrowingType> alloc;

    // Allocate one object successfully
    PoolThrowingType* p1 = alloc.allocate(1);
    FATP_ASSERT_NOT_NULLPTR(p1, "First allocation must succeed");
    FATP_ASSERT_EQ(alloc.allocated(), size_t(1), "allocated() must be 1");

    // Make the constructor throw
    PoolThrowingType::sThrowOnNext = true;
    bool threw = false;
    try
    {
        alloc.allocate(2);
    }
    catch (const std::runtime_error&)
    {
        threw = true;
    }
    FATP_ASSERT_TRUE(threw, "Constructor throw must propagate");
    FATP_ASSERT_EQ(alloc.allocated(), size_t(1),
        "allocated() must remain 1 after failed construction (not incremented before construction)");

    // The slot must have been returned — pool should still have capacity - 1 free slots
    PoolThrowingType::sThrowOnNext = false;
    PoolThrowingType* p2 = alloc.allocate(3);
    FATP_ASSERT_NOT_NULLPTR(p2, "Post-throw allocation must succeed (slot recovered)");
    FATP_ASSERT_EQ(alloc.allocated(), size_t(2), "allocated() must be 2 after recovery");

    alloc.deallocate(p2);
    alloc.deallocate(p1);

    FATP_ASSERT_EQ(alloc.allocated(), size_t(0), "allocated() must be 0 after full cleanup");
    return true;
}

FATP_TEST_CASE(pool_exception_safety_capacity_preserved)
{
    PoolThrowingType::reset();
    constexpr size_t kPoolSize = 4;
    PoolAllocator<kPoolSize>::Allocator<PoolThrowingType> alloc;

    // Throw on every allocation — pool capacity must not be consumed
    PoolThrowingType::sThrowOnNext = true;
    for (size_t i = 0; i < kPoolSize * 3; ++i)
    {
        try
        {
            alloc.allocate(static_cast<int>(i));
        }
        catch (const std::runtime_error&)
        {
        }
    }
    FATP_ASSERT_EQ(alloc.allocated(), size_t(0), "No slots consumed by failed constructions");

    // Now allocate all slots successfully — must still have full capacity
    PoolThrowingType::sThrowOnNext = false;
    std::vector<PoolThrowingType*> ptrs;
    for (size_t i = 0; i < kPoolSize; ++i)
    {
        ptrs.push_back(alloc.allocate(static_cast<int>(i)));
    }
    FATP_ASSERT_EQ(alloc.allocated(), kPoolSize, "Full pool capacity available after failed throws");

    // Pool is now exhausted
    bool exhausted = false;
    try
    {
        alloc.allocate(999);
    }
    catch (const std::bad_alloc&)
    {
        exhausted = true;
    }
    FATP_ASSERT_TRUE(exhausted, "Pool must be exhausted at capacity");

    for (auto* p : ptrs)
    {
        alloc.deallocate(p);
    }
    return true;
}

// ============================================================================
// Test Suite 6: Stress/Fuzz Tests
// ============================================================================

FATP_TEST_CASE(stress_block_random_operations)
{
    BlockAllocator<LifecycleTracker> alloc;
    LifecycleTracker::reset();
    std::vector<LifecycleTracker*> live;

    std::mt19937 rng(42); // Fixed seed for reproducibility
    constexpr int kIterations = 10000;

    for (int i = 0; i < kIterations; ++i)
    {
        // 70% allocate, 30% deallocate (when possible)
        if (live.empty() || rng() % 10 < 7)
        {
            live.push_back(alloc.allocate(i));
        }
        else
        {
            std::uniform_int_distribution<size_t> dist(0, live.size() - 1);
            size_t idx = dist(rng);
            alloc.deallocate(live[idx]);
            live.erase(live.begin() + static_cast<ptrdiff_t>(idx));
        }
    }

    // Verify remaining pointers are valid by accessing values
    for (auto* ptr : live)
    {
        FATP_ASSERT_GE(ptr->mValue, 0, "Values should be valid");
    }

    // Cleanup
    for (auto* ptr : live)
    {
        alloc.deallocate(ptr);
    }

    // Verify lifecycle counts
    int constructs = LifecycleTracker::sConstructCount.load();
    int destructs = LifecycleTracker::sDestructCount.load();
    FATP_ASSERT_EQ(constructs, destructs, "All constructions should have matching destructions");

    return true;
}

FATP_TEST_CASE(stress_pool_fill_empty_cycles)
{
    constexpr size_t kPoolSize = 100;
    constexpr size_t kCycles = 50;
    PoolAllocator<kPoolSize>::Allocator<TrivialNode> alloc;

    for (size_t cycle = 0; cycle < kCycles; ++cycle)
    {
        std::vector<TrivialNode*> ptrs;

        // Fill pool
        for (size_t i = 0; i < kPoolSize; ++i)
        {
            int key = static_cast<int>(cycle * kPoolSize + i);
            int value = static_cast<int>(i);
            TrivialNode* ptr = alloc.allocate(key, value);
            FATP_ASSERT_NOT_NULLPTR(ptr, "Allocation should succeed");
            ptrs.push_back(ptr);
        }

        FATP_ASSERT_TRUE(alloc.full(), "Pool should be full");

        // Verify all values
        for (size_t i = 0; i < kPoolSize; ++i)
        {
            int expectedKey = static_cast<int>(cycle * kPoolSize + i);
            int expectedValue = static_cast<int>(i);
            FATP_ASSERT_EQ(ptrs[i]->mKey, expectedKey, "Key should match");
            FATP_ASSERT_EQ(ptrs[i]->mValue, expectedValue, "Value should match");
        }

        // Empty pool in random order
        std::mt19937 rng(static_cast<unsigned>(cycle));
        std::shuffle(ptrs.begin(), ptrs.end(), rng);
        for (auto* ptr : ptrs)
        {
            alloc.deallocate(ptr);
        }

        FATP_ASSERT_EQ(alloc.allocated(), size_t(0), "Pool should be empty");
    }

    return true;
}

FATP_TEST_CASE(stress_newdelete_interleaved)
{
    NewDeleteAllocator<LifecycleTracker> alloc;
    LifecycleTracker::reset();

    std::vector<LifecycleTracker*> ptrs;
    std::mt19937 rng(12345);
    constexpr int kIterations = 5000;

    for (int i = 0; i < kIterations; ++i)
    {
        if (ptrs.empty() || rng() % 2 == 0)
        {
            ptrs.push_back(alloc.allocate(i));
        }
        else
        {
            std::uniform_int_distribution<size_t> dist(0, ptrs.size() - 1);
            size_t idx = dist(rng);
            alloc.deallocate(ptrs[idx]);
            ptrs.erase(ptrs.begin() + static_cast<ptrdiff_t>(idx));
        }
    }

    for (auto* ptr : ptrs)
    {
        alloc.deallocate(ptr);
    }

    FATP_ASSERT_EQ(LifecycleTracker::sConstructCount.load(),
                   LifecycleTracker::sDestructCount.load(),
                   "All allocations should be freed");

    return true;
}

// ============================================================================
// Benchmarks
// ============================================================================

inline void run_benchmarks()
{
    using namespace fat_p::testing;

    auto& out = *get_test_config().output;
    out << "\n" << colors::cyan() << "Sanity Benchmark:" << colors::reset() << "\n";
    out << "  (benchmarks are provided by benchmark executables; unit tests do not run full benchmarks)\n\n";
}


} // namespace fat_p::testing::allocationns

// ============================================================================
// Public Interface
// ============================================================================

namespace fat_p::testing
{

bool test_AllocationStrategies()
{
    FATP_PRINT_HEADER(ALLOCATION STRATEGIES)

    TestRunner runner;
    auto& out = *get_test_config().output;

    // Test Suite 1: NewDeleteAllocator
    out << colors::blue() << "--- NewDeleteAllocator ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, allocationns, newdelete_basic);
    FATP_RUN_TEST_NS(runner, allocationns, newdelete_multiple_allocations);
    FATP_RUN_TEST_NS(runner, allocationns, newdelete_lifecycle_tracking);
    FATP_RUN_TEST_NS(runner, allocationns, newdelete_over_aligned);
    FATP_RUN_TEST_NS(runner, allocationns, newdelete_copy_move);

    // Test Suite 2: BlockAllocator
    out << "\n" << colors::blue() << "--- BlockAllocator ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, allocationns, block_basic);
    FATP_RUN_TEST_NS(runner, allocationns, block_multiple_allocations);
    FATP_RUN_TEST_NS(runner, allocationns, block_free_list_reuse);
    FATP_RUN_TEST_NS(runner, allocationns, block_move_semantics);
    FATP_RUN_TEST_NS(runner, allocationns, block_alignment);

    // Test Suite 3: PoolAllocator
    out << "\n" << colors::blue() << "--- PoolAllocator ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, allocationns, pool_basic);
    FATP_RUN_TEST_NS(runner, allocationns, pool_free_list_reuse);
    FATP_RUN_TEST_NS(runner, allocationns, pool_exhaustion);
    FATP_RUN_TEST_NS(runner, allocationns, pool_full_capacity_cycle);
    FATP_RUN_TEST_NS(runner, allocationns, pool_move_semantics);

    // Test Suite 4: Edge Cases
    out << "\n" << colors::blue() << "--- Edge Cases ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, allocationns, edge_default_construction);
    FATP_RUN_TEST_NS(runner, allocationns, edge_single_element_type);
    FATP_RUN_TEST_NS(runner, allocationns, edge_destructor_cleanup);

    // Test Suite 5: Exception Safety (placement-new leak regression)
    out << "\n" << colors::blue() << "--- Exception Safety ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, allocationns, block_exception_safety_slot_recovery);
    FATP_RUN_TEST_NS(runner, allocationns, block_exception_safety_repeated_throws);
    FATP_RUN_TEST_NS(runner, allocationns, pool_exception_safety_slot_recovery);
    FATP_RUN_TEST_NS(runner, allocationns, pool_exception_safety_capacity_preserved);

    // Test Suite 6: Stress Tests
    out << "\n" << colors::blue() << "--- Stress Tests ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, allocationns, stress_block_random_operations);
    FATP_RUN_TEST_NS(runner, allocationns, stress_pool_fill_empty_cycles);
    FATP_RUN_TEST_NS(runner, allocationns, stress_newdelete_interleaved);

    // Benchmarks
#ifndef NDEBUG
    out << "\n[Debug build - skipping benchmarks]\n";
#else
    allocationns::run_benchmarks();
#endif

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_AllocationStrategies() ? 0 : 1;
}
#endif
