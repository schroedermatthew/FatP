/**
 * @file test_AllocationStrategies.cpp
 * @brief Test suite for fat_p::AllocationStrategies
 * 
 * Tests the three allocator policies:
 * - NewDeleteAllocator: Standard new/delete per object
 * - BlockAllocator: Contiguous block allocation with bump pointer
 * - PoolAllocator: Fixed-size pre-allocated pool
 * 
 * @version 2.0
 */

#include <iostream>
#include <string>
#include <vector>
#include <array>
#include <atomic>
#include <cstdint>

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
    static inline std::atomic<int> construct_count{0};
    static inline std::atomic<int> destruct_count{0};
    static inline std::atomic<int> copy_count{0};
    static inline std::atomic<int> move_count{0};

    int64_t value;  // Ensures type is at least pointer-sized

    explicit LifecycleTracker(int v = 0) : value(v) 
    { 
        construct_count.fetch_add(1, std::memory_order_relaxed);
    }

    LifecycleTracker(const LifecycleTracker& other) : value(other.value) 
    { 
        copy_count.fetch_add(1, std::memory_order_relaxed);
    }

    LifecycleTracker(LifecycleTracker&& other) noexcept : value(other.value) 
    { 
        move_count.fetch_add(1, std::memory_order_relaxed);
    }

    LifecycleTracker& operator=(const LifecycleTracker& other)
    {
        if (this != &other)
        {
            value = other.value;
            copy_count.fetch_add(1, std::memory_order_relaxed);
        }
        return *this;
    }

    LifecycleTracker& operator=(LifecycleTracker&& other) noexcept
    {
        if (this != &other)
        {
            value = other.value;
            move_count.fetch_add(1, std::memory_order_relaxed);
        }
        return *this;
    }

    ~LifecycleTracker() 
    { 
        destruct_count.fetch_add(1, std::memory_order_relaxed);
    }

    static void reset() noexcept
    {
        construct_count.store(0, std::memory_order_relaxed);
        destruct_count.store(0, std::memory_order_relaxed);
        copy_count.store(0, std::memory_order_relaxed);
        move_count.store(0, std::memory_order_relaxed);
    }
};

/**
 * @brief Trivially copyable type for PoolAllocator testing
 */
struct TrivialNode
{
    int key;
    int value;
    
    TrivialNode() : key(0), value(0) {}
    TrivialNode(int k, int v) : key(k), value(v) {}
};
static_assert(std::is_trivially_copyable_v<TrivialNode>);

/**
 * @brief Over-aligned type for alignment testing
 */
struct alignas(64) CacheAligned
{
    char data[64];
    
    CacheAligned() { data[0] = 0; }
    explicit CacheAligned(char c) { data[0] = c; }
};

// ============================================================================
// Test Suite 1: NewDeleteAllocator
// ============================================================================

TEST_CASE(newdelete_basic)
{
    NewDeleteAllocator<int> alloc;
    
    int* ptr = alloc.allocate(42);
    ASSERT_NOT_NULLPTR(ptr, "Allocation should succeed");
    ASSERT_EQ(*ptr, 42, "Value should be constructed in-place");
    
    alloc.deallocate(ptr);
    return true;
}

TEST_CASE(newdelete_multiple_allocations)
{
    NewDeleteAllocator<int> alloc;
    std::vector<int*> ptrs;
    
    for (int i = 0; i < 100; ++i)
    {
        int* ptr = alloc.allocate(i * 10);
        ASSERT_NOT_NULLPTR(ptr, "Allocation should succeed");
        ASSERT_EQ(*ptr, i * 10, "Value should match");
        ptrs.push_back(ptr);
    }
    
    for (auto ptr : ptrs)
    {
        alloc.deallocate(ptr);
    }
    
    return true;
}

TEST_CASE(newdelete_lifecycle_tracking)
{
    LifecycleTracker::reset();
    
    NewDeleteAllocator<LifecycleTracker> alloc;
    
    LifecycleTracker* ptr = alloc.allocate(42);
    ASSERT_EQ(LifecycleTracker::construct_count.load(), 1, "One construction");
    ASSERT_EQ(ptr->value, 42, "Value should be set");
    
    alloc.deallocate(ptr);
    ASSERT_EQ(LifecycleTracker::destruct_count.load(), 1, "One destruction");
    
    return true;
}

TEST_CASE(newdelete_over_aligned)
{
    NewDeleteAllocator<CacheAligned> alloc;
    
    CacheAligned* ptr = alloc.allocate('X');
    ASSERT_NOT_NULLPTR(ptr, "Aligned allocation should succeed");
    
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    ASSERT_EQ(addr % 64, size_t(0), "Should be 64-byte aligned");
    ASSERT_EQ(ptr->data[0], 'X', "Value should be constructed");
    
    alloc.deallocate(ptr);
    return true;
}

TEST_CASE(newdelete_copy_move)
{
    NewDeleteAllocator<int> alloc1;
    
    // Copy construction
    NewDeleteAllocator<int> alloc2(alloc1);
    int* ptr = alloc2.allocate(123);
    ASSERT_EQ(*ptr, 123, "Copied allocator should work");
    alloc2.deallocate(ptr);
    
    // Move construction
    NewDeleteAllocator<int> alloc3(std::move(alloc1));
    ptr = alloc3.allocate(456);
    ASSERT_EQ(*ptr, 456, "Moved allocator should work");
    alloc3.deallocate(ptr);
    
    return true;
}

// ============================================================================
// Test Suite 2: BlockAllocator
// ============================================================================

TEST_CASE(block_basic)
{
    BlockAllocator<LifecycleTracker> alloc;
    LifecycleTracker::reset();
    
    LifecycleTracker* ptr = alloc.allocate(42);
    ASSERT_NOT_NULLPTR(ptr, "Block allocation should succeed");
    ASSERT_EQ(ptr->value, 42, "Value should be constructed");
    ASSERT_EQ(LifecycleTracker::construct_count.load(), 1, "One construction");
    
    alloc.deallocate(ptr);
    ASSERT_EQ(LifecycleTracker::destruct_count.load(), 1, "One destruction");
    
    return true;
}

TEST_CASE(block_multiple_allocations)
{
    BlockAllocator<LifecycleTracker> alloc;
    LifecycleTracker::reset();
    std::vector<LifecycleTracker*> ptrs;
    
    constexpr int count = 500;  // More than one block (256 per block)
    
    for (int i = 0; i < count; ++i)
    {
        LifecycleTracker* ptr = alloc.allocate(i);
        ASSERT_NOT_NULLPTR(ptr, "Allocation should succeed");
        ASSERT_EQ(ptr->value, i, "Value should match");
        ptrs.push_back(ptr);
    }
    
    ASSERT_EQ(LifecycleTracker::construct_count.load(), count, "All constructions");
    
    for (auto ptr : ptrs)
    {
        alloc.deallocate(ptr);
    }
    
    ASSERT_EQ(LifecycleTracker::destruct_count.load(), count, "All destructions");
    
    return true;
}

TEST_CASE(block_free_list_reuse)
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
    ASSERT_EQ(ptr1, ptr2, "Should reuse deallocated memory");
    ASSERT_EQ(ptr2->value, 2, "Value should be newly constructed");
    
    alloc.deallocate(ptr2);
    return true;
}

TEST_CASE(block_move_semantics)
{
    BlockAllocator<LifecycleTracker> alloc1;
    LifecycleTracker::reset();
    
    // Allocate from first allocator
    LifecycleTracker* ptr = alloc1.allocate(42);
    ASSERT_EQ(ptr->value, 42, "Initial allocation");
    
    // Move construct - transfers ownership
    BlockAllocator<LifecycleTracker> alloc2(std::move(alloc1));
    
    // Can still use the pointer through moved allocator
    ASSERT_EQ(ptr->value, 42, "Value preserved after move");
    
    // Deallocate through new owner
    alloc2.deallocate(ptr);
    ASSERT_EQ(LifecycleTracker::destruct_count.load(), 1, "Destruction via new owner");
    
    return true;
}

TEST_CASE(block_alignment)
{
    BlockAllocator<CacheAligned> alloc;
    
    CacheAligned* ptr = alloc.allocate('A');
    ASSERT_NOT_NULLPTR(ptr, "Aligned allocation should succeed");
    
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    ASSERT_EQ(addr % 64, size_t(0), "Should be 64-byte aligned");
    
    alloc.deallocate(ptr);
    return true;
}

// ============================================================================
// Test Suite 3: PoolAllocator
// ============================================================================

TEST_CASE(pool_basic)
{
    PoolAllocator<100>::Allocator<TrivialNode> alloc;
    
    ASSERT_EQ(alloc.capacity(), size_t(100), "Capacity should match template");
    ASSERT_EQ(alloc.allocated(), size_t(0), "Initially none allocated");
    ASSERT_EQ(alloc.available(), size_t(100), "All available initially");
    ASSERT_FALSE(alloc.full(), "Should not be full");
    
    TrivialNode* ptr = alloc.allocate(1, 100);
    ASSERT_NOT_NULLPTR(ptr, "Allocation should succeed");
    ASSERT_EQ(ptr->key, 1, "Key should match");
    ASSERT_EQ(ptr->value, 100, "Value should match");
    
    ASSERT_EQ(alloc.allocated(), size_t(1), "One allocated");
    ASSERT_EQ(alloc.available(), size_t(99), "99 remaining");
    
    alloc.deallocate(ptr);
    ASSERT_EQ(alloc.allocated(), size_t(0), "None allocated after dealloc");
    
    return true;
}

TEST_CASE(pool_free_list_reuse)
{
    PoolAllocator<10>::Allocator<TrivialNode> alloc;
    
    TrivialNode* ptr1 = alloc.allocate(1, 10);
    alloc.deallocate(ptr1);
    
    TrivialNode* ptr2 = alloc.allocate(2, 20);
    
    // Free list LIFO: should get same memory back
    ASSERT_EQ(ptr1, ptr2, "Should reuse deallocated slot");
    ASSERT_EQ(ptr2->key, 2, "New value constructed");
    
    alloc.deallocate(ptr2);
    return true;
}

TEST_CASE(pool_exhaustion)
{
    PoolAllocator<5>::Allocator<TrivialNode> alloc;
    std::vector<TrivialNode*> ptrs;
    
    // Allocate all 5 slots
    for (int i = 0; i < 5; ++i)
    {
        TrivialNode* ptr = alloc.allocate(i, i * 10);
        ASSERT_NOT_NULLPTR(ptr, "Allocation should succeed while pool has space");
        ptrs.push_back(ptr);
    }
    
    ASSERT_TRUE(alloc.full(), "Pool should be full");
    ASSERT_EQ(alloc.available(), size_t(0), "No slots available");
    
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
    ASSERT_TRUE(threw, "Should throw std::bad_alloc when exhausted");
    
    // Cleanup
    for (auto ptr : ptrs)
    {
        alloc.deallocate(ptr);
    }
    
    ASSERT_FALSE(alloc.full(), "Pool no longer full after deallocation");
    
    return true;
}

TEST_CASE(pool_full_capacity_cycle)
{
    PoolAllocator<100>::Allocator<TrivialNode> alloc;
    std::vector<TrivialNode*> ptrs;
    
    // Fill the pool
    for (int i = 0; i < 100; ++i)
    {
        ptrs.push_back(alloc.allocate(i, i));
    }
    ASSERT_EQ(alloc.allocated(), size_t(100), "All allocated");
    
    // Empty the pool
    for (auto ptr : ptrs)
    {
        alloc.deallocate(ptr);
    }
    ASSERT_EQ(alloc.allocated(), size_t(0), "All deallocated");
    
    // Refill - should work with free list
    ptrs.clear();
    for (int i = 0; i < 100; ++i)
    {
        TrivialNode* ptr = alloc.allocate(i + 100, i + 100);
        ASSERT_NOT_NULLPTR(ptr, "Re-allocation should succeed");
        ptrs.push_back(ptr);
    }
    
    for (auto ptr : ptrs)
    {
        alloc.deallocate(ptr);
    }
    
    return true;
}

TEST_CASE(pool_move_semantics)
{
    PoolAllocator<10>::Allocator<TrivialNode> alloc1;
    
    // Allocate from first
    TrivialNode* ptr1 = alloc1.allocate(1, 10);
    TrivialNode* ptr2 = alloc1.allocate(2, 20);
    ASSERT_EQ(alloc1.allocated(), size_t(2), "Two allocated");
    
    // Move construct
    PoolAllocator<10>::Allocator<TrivialNode> alloc2(std::move(alloc1));
    
    // State transferred
    ASSERT_EQ(alloc2.allocated(), size_t(2), "Allocation count transferred");
    
    // Source should be reset
    ASSERT_EQ(alloc1.allocated(), size_t(0), "Source reset after move");
    
    // Can continue using moved allocator
    TrivialNode* ptr3 = alloc2.allocate(3, 30);
    ASSERT_EQ(ptr3->key, 3, "New allocation works");
    
    alloc2.deallocate(ptr3);
    alloc2.deallocate(ptr2);
    alloc2.deallocate(ptr1);
    
    return true;
}

// ============================================================================
// Test Suite 4: Edge Cases
// ============================================================================

TEST_CASE(edge_default_construction)
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

TEST_CASE(edge_single_element_type)
{
    // Smallest valid type that can hold a pointer
    struct SmallType
    {
        void* data;
        SmallType() : data(nullptr) {}
        explicit SmallType(void* p) : data(p) {}
    };
    static_assert(sizeof(SmallType) >= sizeof(void*));
    
    BlockAllocator<SmallType> alloc;
    
    SmallType* ptr = alloc.allocate(nullptr);
    ASSERT_NOT_NULLPTR(ptr, "Should allocate small type");
    ASSERT_NULLPTR(ptr->data, "Value should be default");
    
    alloc.deallocate(ptr);
    return true;
}

TEST_CASE(edge_destructor_cleanup)
{
    LifecycleTracker::reset();
    
    {
        BlockAllocator<LifecycleTracker> alloc;
        
        // Allocate several objects
        for (int i = 0; i < 10; ++i)
        {
            alloc.allocate(i);
        }
        
        ASSERT_EQ(LifecycleTracker::construct_count.load(), 10, "All constructed");
        // Note: We intentionally don't deallocate - allocator destructor 
        // will free the blocks but not call destructors on live objects
        // This is by design - allocator manages memory, not object lifetime
    }
    
    // Destructor freed blocks but didn't call object destructors
    // This is expected behavior - caller must deallocate to invoke destructors
    
    return true;
}

// ============================================================================
// Benchmarks
// ============================================================================

void run_benchmarks()
{
    auto& out = *get_test_config().output;
    out << "\n" << colors::cyan() << "Allocation Strategies Benchmarks:" 
        << colors::reset() << "\n\n";

    constexpr size_t iterations = 100000;
    constexpr size_t warmup = 1000;

    // Benchmark 1: NewDeleteAllocator single allocation
    {
        out << colors::yellow() << "Single Allocation/Deallocation:" << colors::reset() << "\n";
        
        double nd_time = measure_perf([]() {
            NewDeleteAllocator<int> alloc;
            int* ptr = alloc.allocate(42);
            DoNotOptimize(ptr);
            alloc.deallocate(ptr);
        }, iterations, warmup);
        
        out << "  NewDeleteAllocator: " << format_time(nd_time) << "\n";
    }

    // Benchmark 2: BlockAllocator with reuse
    {
        // Use int64_t instead of int (must be at least pointer-sized)
        BlockAllocator<int64_t> alloc;
        
        double block_time = measure_perf([&alloc]() {
            int64_t* ptr = alloc.allocate(42);
            DoNotOptimize(ptr);
            alloc.deallocate(ptr);  // Goes to free list
        }, iterations, warmup);
        
        out << "  BlockAllocator:     " << format_time(block_time) << "\n";
    }

    // Benchmark 3: PoolAllocator
    {
        PoolAllocator<1000>::Allocator<TrivialNode> alloc;
        
        double pool_time = measure_perf([&alloc]() {
            TrivialNode* ptr = alloc.allocate(1, 2);
            DoNotOptimize(ptr);
            alloc.deallocate(ptr);
        }, iterations, warmup);
        
        out << "  PoolAllocator:      " << format_time(pool_time) << "\n";
    }

    // Benchmark 4: Burst allocation pattern
    {
        out << "\n" << colors::yellow() << "Burst Allocation (100 objects):" 
            << colors::reset() << "\n";
        
        double nd_burst = measure_perf([]() {
            NewDeleteAllocator<int> alloc;
            std::array<int*, 100> ptrs;
            for (int i = 0; i < 100; ++i)
            {
                ptrs[i] = alloc.allocate(i);
            }
            for (auto ptr : ptrs)
            {
                alloc.deallocate(ptr);
            }
        }, iterations / 100, warmup / 10);
        
        out << "  NewDeleteAllocator: " << format_time(nd_burst) << "\n";
        
        double block_burst = measure_perf([]() {
            BlockAllocator<int64_t> alloc;
            std::array<int64_t*, 100> ptrs;
            for (int i = 0; i < 100; ++i)
            {
                ptrs[i] = alloc.allocate(i);
            }
            for (auto ptr : ptrs)
            {
                alloc.deallocate(ptr);
            }
        }, iterations / 100, warmup / 10);
        
        out << "  BlockAllocator:     " << format_time(block_burst) << "\n";
        
        double pool_burst = measure_perf([]() {
            PoolAllocator<100>::Allocator<TrivialNode> alloc;
            std::array<TrivialNode*, 100> ptrs;
            for (int i = 0; i < 100; ++i)
            {
                ptrs[i] = alloc.allocate(i, i);
            }
            for (auto ptr : ptrs)
            {
                alloc.deallocate(ptr);
            }
        }, iterations / 100, warmup / 10);
        
        out << "  PoolAllocator:      " << format_time(pool_burst) << "\n";
    }

    out << "\n";
}

} // namespace fat_p::testing::allocationns

// ============================================================================
// Public Interface
// ============================================================================

namespace fat_p::testing
{

bool test_AllocationStrategies()
{
    PRINT_HEADER(ALLOCATION STRATEGIES)
    
    TestRunner runner;
    auto& out = *get_test_config().output;
    
    // Test Suite 1: NewDeleteAllocator
    out << colors::blue() << "--- NewDeleteAllocator ---" << colors::reset() << "\n";
    RUN_TEST_NS(runner, allocationns, newdelete_basic);
    RUN_TEST_NS(runner, allocationns, newdelete_multiple_allocations);
    RUN_TEST_NS(runner, allocationns, newdelete_lifecycle_tracking);
    RUN_TEST_NS(runner, allocationns, newdelete_over_aligned);
    RUN_TEST_NS(runner, allocationns, newdelete_copy_move);
    
    // Test Suite 2: BlockAllocator
    out << "\n" << colors::blue() << "--- BlockAllocator ---" << colors::reset() << "\n";
    RUN_TEST_NS(runner, allocationns, block_basic);
    RUN_TEST_NS(runner, allocationns, block_multiple_allocations);
    RUN_TEST_NS(runner, allocationns, block_free_list_reuse);
    RUN_TEST_NS(runner, allocationns, block_move_semantics);
    RUN_TEST_NS(runner, allocationns, block_alignment);
    
    // Test Suite 3: PoolAllocator
    out << "\n" << colors::blue() << "--- PoolAllocator ---" << colors::reset() << "\n";
    RUN_TEST_NS(runner, allocationns, pool_basic);
    RUN_TEST_NS(runner, allocationns, pool_free_list_reuse);
    RUN_TEST_NS(runner, allocationns, pool_exhaustion);
    RUN_TEST_NS(runner, allocationns, pool_full_capacity_cycle);
    RUN_TEST_NS(runner, allocationns, pool_move_semantics);
    
    // Test Suite 4: Edge Cases
    out << "\n" << colors::blue() << "--- Edge Cases ---" << colors::reset() << "\n";
    RUN_TEST_NS(runner, allocationns, edge_default_construction);
    RUN_TEST_NS(runner, allocationns, edge_single_element_type);
    RUN_TEST_NS(runner, allocationns, edge_destructor_cleanup);
    
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
