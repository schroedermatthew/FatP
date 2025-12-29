/**
 * @file test_AtomicSharedPtr.cpp
 * @brief Test suite for fat_p::AtomicSharedPtr
 *
 * @author Fat-P Library
 * @version 1.0
 * @date 2025
 */

#include <atomic>
#include <iostream>
#include <thread>
#include <vector>

#include "AtomicSharedPtr.h"
#include "FatPTest.h"

namespace fat_p::testing::atomicsharedptr
{

using namespace fat_p;

struct TestData
{
    int value;
    explicit TestData(int v = 0) : value(v) {}
};

struct LifetimeTracker
{
    inline static std::atomic<int> alive{0};
    int id;

    explicit LifetimeTracker(int i = 0) : id(i) { alive++; }
    ~LifetimeTracker() { alive--; }
    LifetimeTracker(const LifetimeTracker& o) : id(o.id) { alive++; }
};

// ============================================================================
// Tests
// ============================================================================

TEST_CASE(basic_construction)
{
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Basic Construction ===" << colors::reset() << "\n";

    AtomicSharedPtr<TestData> ref1;
    ASSERT_TRUE(!ref1.raw_load(), "Default constructed should be null");

    auto sp = std::make_shared<TestData>(42);
    AtomicSharedPtr<TestData> ref2(sp);
    ASSERT_EQ(ref2.load()->value, 42, "Value should match");

    out << colors::green() << "OK" << colors::reset() << "\n";
    return true;
}

TEST_CASE(load_store)
{
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Load/Store ===" << colors::reset() << "\n";

    AtomicSharedPtr<TestData> ref;

    ref.store(std::make_shared<TestData>(100));
    ASSERT_EQ(ref.load()->value, 100, "Stored value should match");

    ref.store(std::make_shared<TestData>(200));
    ASSERT_EQ(ref.load()->value, 200, "Updated value should match");

    out << colors::green() << "OK" << colors::reset() << "\n";
    return true;
}

TEST_CASE(exchange)
{
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Exchange ===" << colors::reset() << "\n";

    AtomicSharedPtr<TestData> ref(std::make_shared<TestData>(10));

    auto old = ref.exchange(std::make_shared<TestData>(20));
    ASSERT_EQ(old->value, 10, "Exchange should return old value");
    ASSERT_EQ(ref.load()->value, 20, "New value should be stored");

    out << colors::green() << "OK" << colors::reset() << "\n";
    return true;
}

TEST_CASE(compare_exchange_weak)
{
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Compare-Exchange Weak ===" << colors::reset() << "\n";

    AtomicSharedPtr<TestData> ref(std::make_shared<TestData>(30));

    auto expected = ref.load();
    bool success = ref.compare_exchange_weak(expected, std::make_shared<TestData>(40));
    ASSERT_TRUE(success, "CAS should succeed when expected matches");
    ASSERT_EQ(ref.load()->value, 40, "Value should be updated");

    expected = std::make_shared<TestData>(999);
    success = ref.compare_exchange_weak(expected, std::make_shared<TestData>(50));
    ASSERT_TRUE(!success, "CAS should fail when expected doesn't match");
    ASSERT_EQ(ref.load()->value, 40, "Value should not change on failed CAS");

    out << colors::green() << "OK" << colors::reset() << "\n";
    return true;
}

TEST_CASE(compare_exchange_strong)
{
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Compare-Exchange Strong ===" << colors::reset() << "\n";

    AtomicSharedPtr<TestData> ref(std::make_shared<TestData>(60));

    auto expected = ref.load();
    bool success = ref.compare_exchange_strong(expected, std::make_shared<TestData>(70));
    ASSERT_TRUE(success, "Strong CAS should succeed");
    ASSERT_EQ(ref.load()->value, 70, "Value should be updated");

    out << colors::green() << "OK" << colors::reset() << "\n";
    return true;
}

TEST_CASE(throw_on_null)
{
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== ThrowOnNull ===" << colors::reset() << "\n";

    AtomicSharedPtr<TestData, true> throwing_ref;

    bool caught = false;
    try
    {
        auto val = throwing_ref.load();
    }
    catch (const std::runtime_error&)
    {
        caught = true;
    }
    ASSERT_TRUE(caught, "Should throw on null load");

    AtomicSharedPtr<TestData, false> non_throwing_ref;
    auto val = non_throwing_ref.load();
    ASSERT_TRUE(!val, "Should return null without throwing");

    out << colors::green() << "OK" << colors::reset() << "\n";
    return true;
}

TEST_CASE(concurrent_loads)
{
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Concurrent Loads ===" << colors::reset() << "\n";

    AtomicSharedPtr<TestData> ref(std::make_shared<TestData>(270));
    constexpr int num_threads = 8;
    std::atomic<int> success_count{0};

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i)
    {
        threads.emplace_back([&]()
        {
            for (int j = 0; j < 1000; ++j)
            {
                auto val = ref.load();
                if (val && val->value == 270)
                {
                    success_count++;
                }
            }
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    ASSERT_EQ(success_count.load(), num_threads * 1000, "All loads should succeed");

    out << colors::green() << "OK" << colors::reset() << "\n";
    return true;
}

TEST_CASE(concurrent_stores)
{
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Concurrent Stores ===" << colors::reset() << "\n";

    AtomicSharedPtr<TestData> ref(std::make_shared<TestData>(0));
    constexpr int num_threads = 4;
    std::atomic<int> completed{0};

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i)
    {
        threads.emplace_back([&, i]()
        {
            for (int j = 0; j < 100; ++j)
            {
                ref.store(std::make_shared<TestData>(i * 100 + j));
            }
            completed++;
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    ASSERT_EQ(completed.load(), num_threads, "All threads should complete");
    ASSERT_TRUE(ref.load(), "Final value should exist");

    out << colors::green() << "OK" << colors::reset() << "\n";
    return true;
}

TEST_CASE(no_memory_leaks)
{
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== No Memory Leaks ===" << colors::reset() << "\n";

    ASSERT_EQ(LifetimeTracker::alive.load(), 0, "Should start with 0 alive");

    {
        AtomicSharedPtr<LifetimeTracker> ref(std::make_shared<LifetimeTracker>(1));
        ASSERT_EQ(LifetimeTracker::alive.load(), 1, "Should have 1 alive");

        ref.store(std::make_shared<LifetimeTracker>(2));
        ASSERT_EQ(LifetimeTracker::alive.load(), 1, "Old object destroyed after store");
    }

    ASSERT_EQ(LifetimeTracker::alive.load(), 0, "Should have 0 alive after scope");

    out << colors::green() << "OK" << colors::reset() << "\n";
    return true;
}

TEST_CASE(factory_function)
{
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Factory Function ===" << colors::reset() << "\n";

    auto ref = make_atomic_shared_ptr<TestData>(42);
    ASSERT_EQ(ref.load()->value, 42, "Factory should construct with args");

    auto throwing_ref = make_atomic_shared_ptr<TestData, true>(100);
    ASSERT_EQ(throwing_ref.load()->value, 100, "Throwing variant should work");

    out << colors::green() << "OK" << colors::reset() << "\n";
    return true;
}

TEST_CASE(type_trait)
{
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Type Trait ===" << colors::reset() << "\n";

    static_assert(is_atomic_shared_ptr_v<AtomicSharedPtr<TestData>>,
                  "Should be true for AtomicSharedPtr");
    static_assert(is_atomic_shared_ptr_v<AtomicSharedPtr<TestData, true>>,
                  "Should be true for throwing variant");
    static_assert(!is_atomic_shared_ptr_v<int>,
                  "Should be false for int");
    static_assert(!is_atomic_shared_ptr_v<std::shared_ptr<TestData>>,
                  "Should be false for shared_ptr");

    out << colors::green() << "OK" << colors::reset() << "\n";
    return true;
}

TEST_CASE(lock_free_query)
{
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Lock-Free Query ===" << colors::reset() << "\n";

    AtomicSharedPtr<TestData> ref;
    
    // Runtime query
    bool lf = ref.is_lock_free();
    out << "  is_lock_free(): " << (lf ? "true" : "false") << "\n";

    // Compile-time query
    constexpr bool always_lf = AtomicSharedPtr<TestData>::is_always_lock_free();
    out << "  is_always_lock_free(): " << (always_lf ? "true" : "false") << "\n";

    // On C++17, both should be false (no lock-free query available)
    // On C++20, is_always_lock_free implies is_lock_free
#if FATP_HAS_CPP20_ATOMIC_SHARED_PTR
    if (always_lf)
    {
        ASSERT_TRUE(lf, "is_always_lock_free implies is_lock_free");
    }
#else
    ASSERT_TRUE(!lf, "C++17 path always returns false for is_lock_free");
    ASSERT_TRUE(!always_lf, "C++17 path always returns false for is_always_lock_free");
#endif

    out << colors::green() << "OK" << colors::reset() << "\n";
    return true;
}

TEST_CASE(bool_conversion)
{
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Bool Conversion ===" << colors::reset() << "\n";

    AtomicSharedPtr<TestData> empty_ref;
    ASSERT_TRUE(!empty_ref, "Empty ref should be false");

    AtomicSharedPtr<TestData> valid_ref(std::make_shared<TestData>(1));
    ASSERT_TRUE(static_cast<bool>(valid_ref), "Valid ref should be true");

    out << colors::green() << "OK" << colors::reset() << "\n";
    return true;
}

} // namespace fat_p::testing::atomicsharedptr

namespace fat_p::testing
{

bool test_AtomicSharedPtr()
{
    PRINT_HEADER(ATOMIC SHARED PTR)

    TestRunner runner;

    RUN_TEST_NS(runner, atomicsharedptr, basic_construction);
    RUN_TEST_NS(runner, atomicsharedptr, load_store);
    RUN_TEST_NS(runner, atomicsharedptr, exchange);
    RUN_TEST_NS(runner, atomicsharedptr, compare_exchange_weak);
    RUN_TEST_NS(runner, atomicsharedptr, compare_exchange_strong);
    RUN_TEST_NS(runner, atomicsharedptr, throw_on_null);
    RUN_TEST_NS(runner, atomicsharedptr, concurrent_loads);
    RUN_TEST_NS(runner, atomicsharedptr, concurrent_stores);
    RUN_TEST_NS(runner, atomicsharedptr, no_memory_leaks);
    RUN_TEST_NS(runner, atomicsharedptr, factory_function);
    RUN_TEST_NS(runner, atomicsharedptr, type_trait);
    RUN_TEST_NS(runner, atomicsharedptr, lock_free_query);
    RUN_TEST_NS(runner, atomicsharedptr, bool_conversion);

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_AtomicSharedPtr() ? 0 : 1;
}
#endif
