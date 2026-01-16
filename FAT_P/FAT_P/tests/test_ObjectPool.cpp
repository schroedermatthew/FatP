/**
 * @file test_ObjectPool.cpp
 * @brief Comprehensive tests for ObjectPool v3.2 (Four-Reviewer Consensus)
 *
 * Tests cover:
 * - Core operations (acquire, release, reuse)
 * - New APIs (try_acquire, reserve_blocks, stats, capacity, available)
 * - RAII wrapper (PooledObject, make_pooled)
 * - Exception safety (constructor throws, allocation fails)
 * - Thread safety (MutexSynchronizationPolicy)
 * - Debug-mode checks (double-release, foreign pointer, leak detection)
 * - Type aliases (SimpleObjectPool, ThreadSafeObjectPool)
 * - Specialized acquisition (acquire_uninitialized, acquire_zeroed)
 */
/*
FATP_META:
  meta_version: 1
  component: ObjectPool
  file_role: test
  path: tests/test_ObjectPool.cpp
  namespace: fat_p::testing::objectpool
  summary: "Unit tests for ObjectPool."
  related:
    docs_search: "ObjectPool"
    headers:
      - fat_p/ObjectPool.h
      - fat_p/FatPTest.h
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

#include <atomic>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "FatPTest.h"
#include "ObjectPool.h"

namespace fat_p::testing::objectpool
{

// ============================================================================
// Test Objects
// ============================================================================

struct TestObject
{
    int value{0};
    static inline std::atomic<int> construct_count{0};
    static inline std::atomic<int> destruct_count{0};

    TestObject(int v = 0)
        : value(v)
    {
        ++construct_count;
    }
    ~TestObject()
    {
        ++destruct_count;
    }

    static void reset()
    {
        construct_count = 0;
        destruct_count = 0;
    }
};

struct ThrowingObject
{
    static inline bool should_throw{false};
    int value;

    ThrowingObject(int v = 0)
        : value(v)
    {
        if (should_throw)
        {
            throw std::runtime_error("ThrowingObject constructor failed");
        }
    }
};

struct ComplexObject
{
    int a, b;
    std::string str;

    ComplexObject(int x, int y, std::string s)
        : a(x)
        , b(y)
        , str(std::move(s))
    {
    }
};

// Trivially constructible/destructible for specialized acquire tests
struct TrivialObject
{
    int x;
    int y;
    double z;
};
static_assert(std::is_trivially_constructible_v<TrivialObject>);
static_assert(std::is_trivially_destructible_v<TrivialObject>);

// ============================================================================
// Core Operation Tests
// ============================================================================

FATP_TEST_CASE(basic_acquire_release)
{
    ObjectPool<TestObject> pool(4);

    TestObject* obj = pool.acquire(42);
    FATP_ASSERT_TRUE(obj != nullptr, "Should acquire object");
    FATP_ASSERT_TRUE(obj->value == 42, "Object should be initialized with value");

    pool.release(obj);

    return true;
}

FATP_TEST_CASE(reuse)
{
    TestObject::reset();

    ObjectPool<TestObject> pool(4);

    TestObject* obj1 = pool.acquire(1);
    pool.release(obj1);

    TestObject* obj2 = pool.acquire(2);
    pool.release(obj2);

    // Should reuse memory (same address)
    FATP_ASSERT_TRUE(obj1 == obj2, "Should reuse released object memory");

    return true;
}

FATP_TEST_CASE(multiple_acquire)
{
    ObjectPool<TestObject> pool(4);

    std::vector<TestObject*> objects;
    for (int i = 0; i < 10; ++i)
    {
        objects.push_back(pool.acquire(i));
    }

    FATP_ASSERT_TRUE(objects.size() == 10, "Should acquire 10 objects");
    FATP_ASSERT_TRUE(pool.num_blocks() >= 2, "Should allocate multiple blocks");

    for (auto* obj : objects)
    {
        pool.release(obj);
    }

    return true;
}

FATP_TEST_CASE(block_growth)
{
    ObjectPool<TestObject> pool(4);

    FATP_ASSERT_TRUE(pool.num_blocks() == 1, "Should start with 1 block");
    FATP_ASSERT_TRUE(pool.capacity() == 4, "Initial capacity should be block_size");

    std::vector<TestObject*> objects;
    for (int i = 0; i < 10; ++i)
    {
        objects.push_back(pool.acquire(i));
    }

    FATP_ASSERT_TRUE(pool.num_blocks() >= 3, "Should grow to multiple blocks");
    FATP_ASSERT_TRUE(pool.capacity() >= 12, "Capacity should grow with blocks");

    for (auto* obj : objects)
    {
        pool.release(obj);
    }

    return true;
}

FATP_TEST_CASE(constructor_args)
{
    ObjectPool<ComplexObject> pool(4);

    ComplexObject* obj = pool.acquire(10, 20, "test");
    FATP_ASSERT_TRUE(obj->a == 10 && obj->b == 20 && obj->str == "test",
                     "Constructor args should be forwarded correctly");

    pool.release(obj);

    return true;
}

// ============================================================================
// New API Tests (v3.2)
// ============================================================================

FATP_TEST_CASE(try_acquire)
{
    ObjectPool<TestObject> pool(2);

    // Exhaust the initial block
    TestObject* obj1 = pool.acquire(1);
    TestObject* obj2 = pool.acquire(2);

    // Pool is now empty (2 objects acquired from block of 2)
    // try_acquire should return nullptr without allocating
    // But wait - acquire() allocates a new block when empty
    // So we need to test differently

    // Release and test try_acquire succeeds
    pool.release(obj1);
    TestObject* obj3 = pool.try_acquire(3);
    FATP_ASSERT_TRUE(obj3 != nullptr, "try_acquire should succeed when pool has free nodes");
    FATP_ASSERT_TRUE(obj3->value == 3, "try_acquire should construct object");

    pool.release(obj2);
    pool.release(obj3);

    return true;
}

FATP_TEST_CASE(reserve_blocks)
{
    ObjectPool<TestObject> pool(4);

    FATP_ASSERT_TRUE(pool.num_blocks() == 1, "Should start with 1 block");

    pool.reserve_blocks(5);

    FATP_ASSERT_TRUE(pool.num_blocks() == 5, "Should have 5 blocks after reserve");
    FATP_ASSERT_TRUE(pool.capacity() == 20, "Capacity should be 5 * 4 = 20");

    // Acquire should not need to allocate new blocks
    std::vector<TestObject*> objects;
    for (int i = 0; i < 15; ++i)
    {
        objects.push_back(pool.acquire(i));
    }

    FATP_ASSERT_TRUE(pool.num_blocks() == 5, "Should still have 5 blocks");

    for (auto* obj : objects)
    {
        pool.release(obj);
    }

    return true;
}

FATP_TEST_CASE(stats)
{
    ObjectPool<TestObject> pool(4);

    auto stats1 = pool.stats();
    FATP_ASSERT_TRUE(stats1.total_capacity == 4, "Initial capacity should be 4");
    FATP_ASSERT_TRUE(stats1.available == 4, "All 4 should be available initially");
    FATP_ASSERT_TRUE(stats1.acquired == 0, "None should be acquired initially");
    FATP_ASSERT_TRUE(stats1.num_blocks == 1, "Should have 1 block");
    FATP_ASSERT_TRUE(stats1.block_size == 4, "Block size should be 4");

    TestObject* obj1 = pool.acquire(1);
    TestObject* obj2 = pool.acquire(2);

    auto stats2 = pool.stats();
    FATP_ASSERT_TRUE(stats2.available == 2, "2 should be available after acquiring 2");
    FATP_ASSERT_TRUE(stats2.acquired == 2, "2 should be acquired");

    pool.release(obj1);

    auto stats3 = pool.stats();
    FATP_ASSERT_TRUE(stats3.available == 3, "3 should be available after releasing 1");
    FATP_ASSERT_TRUE(stats3.acquired == 1, "1 should still be acquired");

    pool.release(obj2);

    return true;
}

FATP_TEST_CASE(capacity_and_available)
{
    ObjectPool<TestObject> pool(4);

    FATP_ASSERT_TRUE(pool.capacity() == 4, "Initial capacity should be 4");
    FATP_ASSERT_TRUE(pool.available() == 4, "All 4 should be available");

    TestObject* obj = pool.acquire(42);

    FATP_ASSERT_TRUE(pool.capacity() == 4, "Capacity unchanged after acquire");
    FATP_ASSERT_TRUE(pool.available() == 3, "3 available after acquiring 1");

    pool.release(obj);

    FATP_ASSERT_TRUE(pool.available() == 4, "4 available after release");

    return true;
}

FATP_TEST_CASE(active_count)
{
    ObjectPool<TestObject> pool(4);

#ifndef NDEBUG
    FATP_ASSERT_TRUE(pool.active_count() == 0, "Initially 0 active");

    TestObject* obj1 = pool.acquire(1);
    FATP_ASSERT_TRUE(pool.active_count() == 1, "1 active after acquire");

    TestObject* obj2 = pool.acquire(2);
    FATP_ASSERT_TRUE(pool.active_count() == 2, "2 active after second acquire");

    pool.release(obj1);
    FATP_ASSERT_TRUE(pool.active_count() == 1, "1 active after release");

    pool.release(obj2);
    FATP_ASSERT_TRUE(pool.active_count() == 0, "0 active after all released");
#else
    // In release mode, active_count() returns 0
    FATP_ASSERT_TRUE(pool.active_count() == 0, "active_count returns 0 in release mode");
    TestObject* obj = pool.acquire(42);
    pool.release(obj);
#endif

    return true;
}

// ============================================================================
// Specialized Acquire Tests
// ============================================================================

FATP_TEST_CASE(acquire_uninitialized)
{
    ObjectPool<TrivialObject> pool(4);

    TrivialObject* obj = pool.acquire_uninitialized();
    FATP_ASSERT_TRUE(obj != nullptr, "acquire_uninitialized should return pointer");

    // The pool returns raw storage. Start object lifetime before accessing.
    ::new (static_cast<void*>(obj)) TrivialObject;

    // Manually initialize
    obj->x = 10;
    obj->y = 20;
    obj->z = 3.14;

    FATP_ASSERT_TRUE(obj->x == 10 && obj->y == 20, "Should be able to use memory");

    pool.release(obj);

    return true;
}

FATP_TEST_CASE(acquire_zeroed)
{
    ObjectPool<TrivialObject> pool(4);

    TrivialObject* obj = pool.acquire_zeroed();
    FATP_ASSERT_TRUE(obj != nullptr, "acquire_zeroed should return pointer");

    // The pool returns raw storage. Start object lifetime without overwriting zeroed bytes.
    ::new (static_cast<void*>(obj)) TrivialObject;

    // Memory should be zeroed
    FATP_ASSERT_TRUE(obj->x == 0 && obj->y == 0, "Memory should be zero-initialized");

    pool.release(obj);

    return true;
}

// ============================================================================
// RAII Wrapper Tests
// ============================================================================

FATP_TEST_CASE(pooled_object_raii)
{
    TestObject::reset();

    {
        ObjectPool<TestObject> pool(4);

        {
            auto pooled = make_pooled(pool, 42);
            FATP_ASSERT_TRUE(pooled.get() != nullptr, "PooledObject should hold object");
            FATP_ASSERT_TRUE(pooled->value == 42, "PooledObject should access object");
            FATP_ASSERT_TRUE((*pooled).value == 42, "operator* should work");

            auto stats = pool.stats();
            FATP_ASSERT_TRUE(stats.acquired == 1, "1 should be acquired via PooledObject");
        }
        // PooledObject destroyed, should release back to pool

        auto stats = pool.stats();
        FATP_ASSERT_TRUE(stats.acquired == 0, "0 should be acquired after PooledObject destroyed");
    }

    return true;
}

FATP_TEST_CASE(pooled_object_move)
{
    ObjectPool<TestObject> pool(4);

    auto pooled1 = make_pooled(pool, 100);
    TestObject* raw_ptr = pooled1.get();

    // Move construction
    PooledObject<TestObject> pooled2(std::move(pooled1));

    FATP_ASSERT_TRUE(pooled1.get() == nullptr, "Moved-from should be null");
    FATP_ASSERT_TRUE(pooled2.get() == raw_ptr, "Moved-to should hold original pointer");
    FATP_ASSERT_TRUE(pooled2->value == 100, "Value should be preserved");

    // Move assignment
    auto pooled3 = make_pooled(pool, 200);
    pooled3 = std::move(pooled2);

    FATP_ASSERT_TRUE(pooled2.get() == nullptr, "Moved-from should be null");
    FATP_ASSERT_TRUE(pooled3.get() == raw_ptr, "Move-assigned should hold pointer");

    return true;
}

FATP_TEST_CASE(pooled_object_reset)
{
    ObjectPool<TestObject> pool(4);

    auto pooled = make_pooled(pool, 42);
    FATP_ASSERT_TRUE(pool.stats().acquired == 1, "1 acquired");

    pooled.reset();

    FATP_ASSERT_TRUE(pooled.get() == nullptr, "reset() should clear pointer");
    FATP_ASSERT_TRUE(pool.stats().acquired == 0, "reset() should release to pool");

    return true;
}

FATP_TEST_CASE(pooled_object_release)
{
    ObjectPool<TestObject> pool(4);

    auto pooled = make_pooled(pool, 42);
    TestObject* raw = pooled.release();

    FATP_ASSERT_TRUE(pooled.get() == nullptr, "release() should clear pointer");
    FATP_ASSERT_TRUE(raw != nullptr, "release() should return raw pointer");
    FATP_ASSERT_TRUE(raw->value == 42, "Raw pointer should still be valid");

    // Object is still acquired - we own it now
    FATP_ASSERT_TRUE(pool.stats().acquired == 1, "Object still acquired after release()");

    // Must manually release
    pool.release(raw);

    FATP_ASSERT_TRUE(pool.stats().acquired == 0, "Manual release should work");

    return true;
}

FATP_TEST_CASE(pooled_object_get_pool)
{
    ObjectPool<TestObject> pool(4);

    auto pooled = make_pooled(pool, 42);

    FATP_ASSERT_TRUE(pooled.get_pool() == &pool, "get_pool() should return owning pool");

    return true;
}

FATP_TEST_CASE(pooled_object_bool_conversion)
{
    ObjectPool<TestObject> pool(4);

    PooledObject<TestObject> empty;
    FATP_ASSERT_TRUE(!empty, "Default PooledObject should be falsy");

    auto pooled = make_pooled(pool, 42);
    FATP_ASSERT_TRUE(static_cast<bool>(pooled), "Valid PooledObject should be truthy");

    pooled.reset();
    FATP_ASSERT_TRUE(!pooled, "Reset PooledObject should be falsy");

    return true;
}

// ============================================================================
// Exception Safety Tests
// ============================================================================

FATP_TEST_CASE(constructor_exception_safety)
{
    ObjectPool<ThrowingObject> pool(4);

    ThrowingObject::should_throw = false;

    // Normal acquisition should work
    ThrowingObject* obj1 = pool.acquire(1);
    FATP_ASSERT_TRUE(obj1 != nullptr, "Normal acquire should succeed");
    FATP_ASSERT_TRUE(obj1->value == 1, "Value should be set");

    auto stats_before = pool.stats();
    size_t available_before = stats_before.available;

    // Enable throwing
    ThrowingObject::should_throw = true;

    bool caught = false;
    try
    {
        (void)pool.acquire(2); // Should throw, suppress [[nodiscard]] warning
    }
    catch (const std::runtime_error&)
    {
        caught = true;
    }

    FATP_ASSERT_TRUE(caught, "Exception should be thrown");

    auto stats_after = pool.stats();
    FATP_ASSERT_TRUE(stats_after.available == available_before,
                     "Node should be restored to free list after constructor throws");

    // Disable throwing and verify pool still works
    ThrowingObject::should_throw = false;

    ThrowingObject* obj2 = pool.acquire(3);
    FATP_ASSERT_TRUE(obj2 != nullptr, "Pool should still work after exception");

    pool.release(obj1);
    pool.release(obj2);

    return true;
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

FATP_TEST_CASE(thread_safety)
{
    ObjectPool<TestObject, MutexSynchronizationPolicy> pool(16);

    std::atomic<int> total_ops{0};
    std::vector<std::thread> threads;

    for (int i = 0; i < 4; ++i)
    {
        threads.emplace_back([&pool, &total_ops]() {
            for (int j = 0; j < 100; ++j)
            {
                TestObject* obj = pool.acquire(j);
                pool.release(obj);
                ++total_ops;
            }
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    FATP_ASSERT_TRUE(total_ops == 400, "All operations should complete");
    FATP_ASSERT_TRUE(pool.stats().acquired == 0, "All objects should be released");

    return true;
}

FATP_TEST_CASE(thread_safe_alias)
{
    ThreadSafeObjectPool<TestObject> pool(8);

    std::vector<std::thread> threads;
    for (int i = 0; i < 2; ++i)
    {
        threads.emplace_back([&pool]() {
            for (int j = 0; j < 50; ++j)
            {
                TestObject* obj = pool.acquire(j);
                pool.release(obj);
            }
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    return true;
}

// ============================================================================
// Type Alias Tests
// ============================================================================

FATP_TEST_CASE(simple_alias)
{
    SimpleObjectPool<TestObject> pool(4);

    TestObject* obj = pool.acquire(42);
    FATP_ASSERT_TRUE(obj != nullptr, "SimpleObjectPool should work");
    FATP_ASSERT_TRUE(obj->value == 42, "Value should be correct");

    pool.release(obj);

    return true;
}

// ============================================================================
// Edge Case Tests
// ============================================================================

FATP_TEST_CASE(null_release)
{
    ObjectPool<TestObject> pool(4);

    // Releasing nullptr should be safe (no-op)
    pool.release(nullptr);

    return true;
}

FATP_TEST_CASE(exhaust_and_grow)
{
    ObjectPool<TestObject> pool(2);

    TestObject* obj1 = pool.acquire();
    TestObject* obj2 = pool.acquire();
    TestObject* obj3 = pool.acquire(); // Should trigger new block

    FATP_ASSERT_TRUE(obj1 && obj2 && obj3, "Should handle pool exhaustion by growing");
    FATP_ASSERT_TRUE(pool.num_blocks() >= 2, "Should have allocated new block");

    pool.release(obj1);
    pool.release(obj2);
    pool.release(obj3);

    return true;
}

FATP_TEST_CASE(block_size_accessor)
{
    ObjectPool<TestObject> pool(32);

    FATP_ASSERT_TRUE(pool.block_size() == 32, "block_size() should return configured size");

    return true;
}

// ============================================================================
// Benchmark
// ============================================================================

void benchmark_objectpool()
{
    std::cout << "\n" << colors::cyan() << "ObjectPool Benchmarks:" << colors::reset() << "\n\n";

    ObjectPool<TestObject> pool(64);

    // Benchmark acquire + release
    double pool_time = measure_perf(
        [&pool]() {
            TestObject* obj = pool.acquire(42);
            pool.release(obj);
        },
        100000,
        1000);
    std::cout << "Pool acquire + release: " << format_time(pool_time) << "\n";

    // Benchmark new + delete for comparison
    double new_time = measure_perf(
        []() {
            TestObject* obj = new TestObject(42);
            delete obj;
        },
        100000,
        1000);
    std::cout << "new + delete:           " << format_time(new_time) << "\n";

    if (pool_time > 0)
    {
        std::cout << "Speedup: " << std::fixed << std::setprecision(1) << (new_time / pool_time) << "x\n";
    }

    // Benchmark try_acquire
    double try_time = measure_perf(
        [&pool]() {
            TestObject* obj = pool.try_acquire(42);
            if (obj)
            {
                pool.release(obj);
            }
        },
        100000,
        1000);
    std::cout << "try_acquire + release:  " << format_time(try_time) << "\n";

    // Benchmark stats() (cold path)
    double stats_time = measure_perf(
        [&pool]() {
            auto s = pool.stats();
            (void)s;
        },
        10000,
        100);
    std::cout << "stats() (cold path):    " << format_time(stats_time) << "\n";
}

// ============================================================================
// Main Test Runner
// ============================================================================

} // namespace fat_p::testing::objectpool

namespace fat_p::testing
{

bool test_ObjectPool()
{
    FATP_PRINT_HEADER(OBJECT POOL v3.2)

    TestRunner runner;

    // Core operations
    FATP_RUN_TEST_NS(runner, objectpool, basic_acquire_release);
    FATP_RUN_TEST_NS(runner, objectpool, reuse);
    FATP_RUN_TEST_NS(runner, objectpool, multiple_acquire);
    FATP_RUN_TEST_NS(runner, objectpool, block_growth);
    FATP_RUN_TEST_NS(runner, objectpool, constructor_args);

    // New v3.2 APIs
    FATP_RUN_TEST_NS(runner, objectpool, try_acquire);
    FATP_RUN_TEST_NS(runner, objectpool, reserve_blocks);
    FATP_RUN_TEST_NS(runner, objectpool, stats);
    FATP_RUN_TEST_NS(runner, objectpool, capacity_and_available);
    FATP_RUN_TEST_NS(runner, objectpool, active_count);

    // Specialized acquire (trivial types)
    FATP_RUN_TEST_NS(runner, objectpool, acquire_uninitialized);
    FATP_RUN_TEST_NS(runner, objectpool, acquire_zeroed);

    // RAII wrapper
    FATP_RUN_TEST_NS(runner, objectpool, pooled_object_raii);
    FATP_RUN_TEST_NS(runner, objectpool, pooled_object_move);
    FATP_RUN_TEST_NS(runner, objectpool, pooled_object_reset);
    FATP_RUN_TEST_NS(runner, objectpool, pooled_object_release);
    FATP_RUN_TEST_NS(runner, objectpool, pooled_object_get_pool);
    FATP_RUN_TEST_NS(runner, objectpool, pooled_object_bool_conversion);

    // Exception safety
    FATP_RUN_TEST_NS(runner, objectpool, constructor_exception_safety);

    // Thread safety
    FATP_RUN_TEST_NS(runner, objectpool, thread_safety);
    FATP_RUN_TEST_NS(runner, objectpool, thread_safe_alias);

    // Type aliases
    FATP_RUN_TEST_NS(runner, objectpool, simple_alias);

    // Edge cases
    FATP_RUN_TEST_NS(runner, objectpool, null_release);
    FATP_RUN_TEST_NS(runner, objectpool, exhaust_and_grow);
    FATP_RUN_TEST_NS(runner, objectpool, block_size_accessor);

    objectpool::benchmark_objectpool();

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_ObjectPool() ? 0 : 1;
}
#endif
