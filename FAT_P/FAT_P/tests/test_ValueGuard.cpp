/**
 * @file test_ValueGuard.cpp
 * @brief Comprehensive unit tests for ValueGuard.h v2.4
 *
 * Tests all features:
 * - Basic RAII value restoration (copy and move policies)
 * - Policy extensibility (NoRestore, Conditional, Custom)
 * - Early release functionality
 * - State introspection (is_active, original, current)
 * - Move semantics (construction and assignment)
 * - Support for move-only types
 * - Deduction guides (with disambiguation)
 * - Exception safety (strong guarantee)
 * - Performance characteristics
 * - Integration patterns
 *
 * Test Configuration:
 * - C++ Standard: C++17
 * - Build Modes: Debug and Release
 *
 * Compilation:
 * - Debug:   g++ -std=c++17 -g test_ValueGuard.cpp -o test_value_guard_debug
 * - Release: g++ -std=c++17 -O3 -DNDEBUG test_ValueGuard.cpp -o test_value_guard_release
 *
 * @version 2.4
 * @date 2025
 */
/*
FATP_META:
  meta_version: 1
  component: ValueGuard
  file_role: test
  path: tests/test_ValueGuard.cpp
  namespace: fat_p
  summary: "Unit tests for ValueGuard."
  related:
    docs_search: "ValueGuard"
    headers:
      - fat_p/ValueGuard.h
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

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "FatPTest.h"
#include "ValueGuard.h"

namespace fat_p::testing::value_guard
{

using namespace fat_p;

// =============================================================================
// Helper Classes
// =============================================================================

/**
 * @brief Test object that tracks lifecycle events
 */
class LifecycleTracker
{
public:
    static inline std::atomic<int> construct_count{0};
    static inline std::atomic<int> copy_construct_count{0};
    static inline std::atomic<int> move_construct_count{0};
    static inline std::atomic<int> copy_assign_count{0};
    static inline std::atomic<int> move_assign_count{0};
    static inline std::atomic<int> destruct_count{0};

    int value;

    explicit LifecycleTracker(int v = 0)
        : value(v)
    {
        ++construct_count;
    }

    LifecycleTracker(const LifecycleTracker& other)
        : value(other.value)
    {
        ++copy_construct_count;
    }

    LifecycleTracker(LifecycleTracker&& other) noexcept
        : value(other.value)
    {
        ++move_construct_count;
        other.value = -1;
    }

    LifecycleTracker& operator=(const LifecycleTracker& other)
    {
        value = other.value;
        ++copy_assign_count;
        return *this;
    }

    LifecycleTracker& operator=(LifecycleTracker&& other) noexcept
    {
        value = other.value;
        ++move_assign_count;
        other.value = -1;
        return *this;
    }

    ~LifecycleTracker()
    {
        ++destruct_count;
    }

    static void reset_counters()
    {
        construct_count = 0;
        copy_construct_count = 0;
        move_construct_count = 0;
        copy_assign_count = 0;
        move_assign_count = 0;
        destruct_count = 0;
    }
};

/**
 * @brief Move-only type for testing move semantics
 */
class MoveOnly
{
public:
    static inline std::atomic<int> construct_count{0};
    static inline std::atomic<int> move_construct_count{0};
    static inline std::atomic<int> move_assign_count{0};
    static inline std::atomic<int> destruct_count{0};

    int value;

    explicit MoveOnly(int v = 0)
        : value(v)
    {
        ++construct_count;
    }

    MoveOnly(const MoveOnly&) = delete;
    MoveOnly& operator=(const MoveOnly&) = delete;

    MoveOnly(MoveOnly&& other) noexcept
        : value(other.value)
    {
        ++move_construct_count;
        other.value = -1;
    }

    MoveOnly& operator=(MoveOnly&& other) noexcept
    {
        value = other.value;
        ++move_assign_count;
        other.value = -1;
        return *this;
    }

    ~MoveOnly()
    {
        ++destruct_count;
    }

    static void reset_counters()
    {
        construct_count = 0;
        move_construct_count = 0;
        move_assign_count = 0;
        destruct_count = 0;
    }
};

/**
 * @brief Type that throws on assignment (for exception safety tests)
 */
class ThrowOnAssign
{
public:
    int value;
    mutable int throw_countdown; // Throws when > 0, decrements each attempt

    explicit ThrowOnAssign(int v = 0, int throws_remaining = 0)
        : value(v)
        , throw_countdown(throws_remaining)
    {
    }

    ThrowOnAssign(const ThrowOnAssign& other)
        : value(other.value)
        , throw_countdown(other.throw_countdown)
    {
    }

    ThrowOnAssign(ThrowOnAssign&& other) noexcept
        : value(other.value)
        , throw_countdown(other.throw_countdown)
    {
        other.value = -1;
    }

    ThrowOnAssign& operator=(const ThrowOnAssign& other)
    {
        if (throw_countdown > 0)
        {
            --throw_countdown;
            throw std::runtime_error("ThrowOnAssign: copy assignment threw");
        }
        value = other.value;
        throw_countdown = other.throw_countdown;
        return *this;
    }

    ThrowOnAssign& operator=(ThrowOnAssign&& other)
    {
        if (throw_countdown > 0)
        {
            --throw_countdown;
            throw std::runtime_error("ThrowOnAssign: move assignment threw");
        }
        value = other.value;
        throw_countdown = other.throw_countdown;
        other.value = -1;
        return *this;
    }
};

// =============================================================================
// Test Suite 1: Basic Functionality - Copy Policy
// =============================================================================

FATP_TEST_CASE(basic_copy_restoration)
{
    int value = 42;

    {
        ValueGuard guard(value, 100);
        FATP_ASSERT_EQ(value, 100, "Value changed to new value");
    }

    FATP_ASSERT_EQ(value, 42, "Value restored to original");
    return true;
}

FATP_TEST_CASE(basic_copy_with_string)
{
    std::string str = "original";

    {
        ValueGuard guard(str, std::string("temporary"));
        FATP_ASSERT_EQ(str, "temporary", "String changed to new value");
    }

    FATP_ASSERT_EQ(str, "original", "String restored to original");
    return true;
}

FATP_TEST_CASE(copy_policy_explicit)
{
    double value = 3.14;

    {
        ValueGuard<double, ValueGuardCopyPolicy<double>> guard(value, 2.71);
        FATP_ASSERT_EQ(value, 2.71, "Value changed");
    }

    FATP_ASSERT_EQ(value, 3.14, "Value restored");
    return true;
}

FATP_TEST_CASE(introspection_original)
{
    int value = 42;

    ValueGuard guard(value, 100);
    FATP_ASSERT_EQ(guard.original(), 42, "Original value accessible");
    FATP_ASSERT_EQ(guard.current(), 100, "Current value accessible");
    FATP_ASSERT_TRUE(guard.is_active(), "Guard is active");

    return true;
}

// =============================================================================
// Test Suite 2: Move Policy
// =============================================================================

FATP_TEST_CASE(basic_move_restoration)
{
    std::string str = "original";

    {
        ValueGuard guard(str, std::string("temporary"));
        FATP_ASSERT_EQ(str, "temporary", "Value moved to new value");
    }

    FATP_ASSERT_EQ(str, "original", "Value restored");
    return true;
}

FATP_TEST_CASE(move_policy_explicit)
{
    std::unique_ptr<int> ptr = std::make_unique<int>(42);
    int* original_addr = ptr.get();

    {
        ValueGuard<std::unique_ptr<int>, ValueGuardMovePolicy<std::unique_ptr<int>>> guard(ptr,
                                                                                           std::make_unique<int>(100));

        FATP_ASSERT_TRUE(ptr != nullptr, "Ptr has new value");
        FATP_ASSERT_EQ(*ptr, 100, "New value is 100");
        FATP_ASSERT_NE(ptr.get(), original_addr, "Pointer changed");
    }

    FATP_ASSERT_TRUE(ptr != nullptr, "Ptr restored");
    FATP_ASSERT_EQ(ptr.get(), original_addr, "Original pointer restored");
    FATP_ASSERT_EQ(*ptr, 42, "Original value restored");

    return true;
}

FATP_TEST_CASE(move_only_type)
{
    MoveOnly::reset_counters();

    MoveOnly obj(42);

    {
        ValueGuard<MoveOnly, ValueGuardMovePolicy<MoveOnly>> guard(obj, MoveOnly(100));

        FATP_ASSERT_EQ(obj.value, 100, "Value changed");
    }

    FATP_ASSERT_EQ(obj.value, 42, "Value restored for move-only type");
    return true;
}

FATP_TEST_CASE(factory_move)
{
    std::string str = "original";

    {
        auto guard = make_value_guard_move(str, std::string("temporary"));
        FATP_ASSERT_EQ(str, "temporary", "Value moved to new value");
    }

    FATP_ASSERT_EQ(str, "original", "Value restored");
    return true;
}

// =============================================================================
// Test Suite 3: NoRestore Policy
// =============================================================================

FATP_TEST_CASE(no_restore_policy)
{
    int value = 42;

    {
        ValueGuard<int, ValueGuardNoRestorePolicy<int>> guard(value, 100);
        FATP_ASSERT_EQ(value, 100, "Value changed");
    }

    FATP_ASSERT_EQ(value, 100, "Value NOT restored (NoRestore policy)");
    return true;
}

FATP_TEST_CASE(no_restore_factory)
{
    int value = 42;

    {
        auto guard = make_value_guard_no_restore(value, 100);
        FATP_ASSERT_EQ(value, 100, "Value changed");
    }

    FATP_ASSERT_EQ(value, 100, "Value NOT restored (NoRestore policy via factory)");
    return true;
}

// =============================================================================
// Test Suite 4: Conditional Policy
// =============================================================================

FATP_TEST_CASE(conditional_restore_true)
{
    int value = 42;

    {
        auto guard = make_value_guard_conditional(value,
                                                  100,
                                                  []()
                                                  {
                                                      return true;
                                                  });
        FATP_ASSERT_EQ(value, 100, "Value changed");
    }

    FATP_ASSERT_EQ(value, 42, "Value restored (condition true)");
    return true;
}

FATP_TEST_CASE(conditional_restore_false)
{
    int value = 42;

    {
        auto guard = make_value_guard_conditional(value,
                                                  100,
                                                  []()
                                                  {
                                                      return false;
                                                  });
        FATP_ASSERT_EQ(value, 100, "Value changed");
    }

    FATP_ASSERT_EQ(value, 100, "Value NOT restored (condition false)");
    return true;
}

FATP_TEST_CASE(conditional_dynamic_decision)
{
    int value = 42;
    bool should_restore = true;

    {
        auto guard = make_value_guard_conditional(value,
                                                  100,
                                                  [&should_restore]()
                                                  {
                                                      return should_restore;
                                                  });
        FATP_ASSERT_EQ(value, 100, "Value changed");

        // Decide not to restore mid-scope
        should_restore = false;
    }

    FATP_ASSERT_EQ(value, 100, "Value NOT restored (dynamic decision)");
    return true;
}

// =============================================================================
// Test Suite 5: Custom Policy
// =============================================================================

FATP_TEST_CASE(custom_restorer)
{
    int value = 42;
    bool restorer_called = false;

    {
        auto guard = make_value_guard_custom(value,
                                             100,
                                             [&restorer_called](int& target, int&& original)
                                             {
                                                 restorer_called = true;
                                                 target = original + 1; // Custom logic: restore + 1
                                             });
        FATP_ASSERT_EQ(value, 100, "Value changed");
    }

    FATP_ASSERT_TRUE(restorer_called, "Custom restorer was called");
    FATP_ASSERT_EQ(value, 43, "Value restored with custom logic (+1)");
    return true;
}

FATP_TEST_CASE(custom_restorer_with_logging)
{
    std::string value = "original";
    std::vector<std::string> log;

    {
        auto guard = make_value_guard_custom(value,
                                             std::string("temporary"),
                                             [&log](std::string& target, std::string&& original)
                                             {
                                                 log.push_back("Restoring from '" + target + "' to '" + original + "'");
                                                 target = std::move(original);
                                             });
        FATP_ASSERT_EQ(value, "temporary", "Value changed");
    }

    FATP_ASSERT_EQ(value, "original", "Value restored");
    FATP_ASSERT_EQ(log.size(), 1u, "One log entry");
    FATP_ASSERT_TRUE(log[0].find("Restoring from") != std::string::npos, "Log entry correct");
    return true;
}

FATP_TEST_CASE(custom_restorer_incremental)
{
    int counter = 0;

    {
        auto guard = make_value_guard_custom(counter,
                                             10,
                                             [](int& target, int&& /*original*/)
                                             {
                                                 target--; // Decrement instead of full restore
                                             });
        FATP_ASSERT_EQ(counter, 10, "Counter set to 10");
    }

    FATP_ASSERT_EQ(counter, 9, "Counter decremented by custom restorer");
    return true;
}

// =============================================================================
// Test Suite 6: Early Release
// =============================================================================

FATP_TEST_CASE(release_basic)
{
    int value = 42;

    {
        ValueGuard guard(value, 100);
        FATP_ASSERT_EQ(value, 100, "Value changed");

        guard.release();
        FATP_ASSERT_FALSE(guard.is_active(), "Guard released");
    }

    FATP_ASSERT_EQ(value, 100, "Value NOT restored (released)");
    return true;
}

FATP_TEST_CASE(release_conditional)
{
    int value = 42;
    bool operation_succeeded = false;

    {
        ValueGuard guard(value, 100);
        FATP_ASSERT_EQ(value, 100, "Value changed");

        // Simulate successful operation
        operation_succeeded = true;

        if (operation_succeeded)
        {
            guard.release(); // Commit the change
        }
    }

    FATP_ASSERT_EQ(value, 100, "Value kept (operation succeeded, released)");
    return true;
}

// =============================================================================
// Test Suite 7: Move Semantics of Guard
// =============================================================================

FATP_TEST_CASE(guard_move_constructor)
{
    int value = 42;

    {
        ValueGuard guard1(value, 100);
        FATP_ASSERT_EQ(value, 100, "Value changed");

        ValueGuard guard2(std::move(guard1));

        FATP_ASSERT_FALSE(guard1.is_active(), "Guard1 inactive after move");
        FATP_ASSERT_TRUE(guard2.is_active(), "Guard2 active after move");

        FATP_ASSERT_EQ(value, 100, "Value still changed");
    }

    FATP_ASSERT_EQ(value, 42, "Value restored by moved guard");
    return true;
}

/**
 * @brief CRITICAL TEST: Verifies move assignment restores the current target
 *
 * This test validates the fix for CRITICAL-1: Move assignment must restore
 * the current mGuard_LIT_0__s target.
 *
 * Expected behavior (v2.4):
 * - guard2 = std::move(guard1) first restores value2 to 99
 * - Then guard2 takes ownership of value1
 * - On scope exit, guard2 restores value1 to 42
 */
FATP_TEST_CASE(guard_move_assignment)
{
    int value1 = 42;
    int value2 = 99;

    {
        ValueGuard guard1(value1, 100); // value1: 42 -> 100
        ValueGuard guard2(value2, 200); // value2: 99 -> 200

        // CRITICAL: This must restore value2 to 99 before taking value1
        guard2 = std::move(guard1);

        FATP_ASSERT_FALSE(guard1.is_active(), "Guard1 inactive after move assign");
        FATP_ASSERT_TRUE(guard2.is_active(), "Guard2 active after move assign");

        // value2 should be IMMEDIATELY restored when guard2 is reassigned
        FATP_ASSERT_EQ(value2, 99, "value2 restored immediately on reassignment");
    }

    // Final state: value1 restored by guard2, value2 already restored
    FATP_ASSERT_EQ(value1, 42, "value1 restored by guard2");
    FATP_ASSERT_EQ(value2, 99, "value2 was restored during reassignment");

    return true;
}

/**
 * @brief Tests that self-move-assignment is a no-op
 */
FATP_TEST_CASE(self_move_assignment)
{
    int value = 42;

    {
        ValueGuard guard(value, 100);
        FATP_ASSERT_EQ(value, 100, "Value changed");

        // Self-move-assignment should be a no-op
        guard = std::move(guard);

        FATP_ASSERT_TRUE(guard.is_active(), "Guard still active after self-move");
        FATP_ASSERT_EQ(value, 100, "Value unchanged after self-move");
    }

    FATP_ASSERT_EQ(value, 42, "Value restored normally");
    return true;
}

FATP_TEST_CASE(guard_in_vector)
{
    int value = 42;

    std::vector<ValueGuard<int, ValueGuardCopyPolicy<int>>> guards;
    guards.reserve(10);

    {
        ValueGuard<int, ValueGuardCopyPolicy<int>> guard(value, 100);
        FATP_ASSERT_EQ(value, 100, "Value changed");

        guards.push_back(std::move(guard));
        FATP_ASSERT_FALSE(guard.is_active(), "Original guard inactive");
    }

    FATP_ASSERT_EQ(value, 100, "Value still changed (guard in vector)");

    guards.clear();

    FATP_ASSERT_EQ(value, 42, "Value restored after vector cleared");
    return true;
}

// =============================================================================
// Test Suite 8: Swap Operations
// =============================================================================

FATP_TEST_CASE(swap_member)
{
    int value1 = 42;
    int value2 = 99;

    {
        ValueGuard guard1(value1, 100);
        ValueGuard guard2(value2, 200);

        guard1.swap(guard2);

        FATP_ASSERT_EQ(value1, 100, "Value1 still modified");
        FATP_ASSERT_EQ(value2, 200, "Value2 still modified");
    }

    // After swap: guard1 manages value2 (restores to 99), guard2 manages value1 (restores to 42)
    FATP_ASSERT_EQ(value1, 42, "Value1 restored by swapped guard2");
    FATP_ASSERT_EQ(value2, 99, "Value2 restored by swapped guard1");

    return true;
}

FATP_TEST_CASE(swap_std)
{
    int value1 = 42;
    int value2 = 99;

    {
        ValueGuard guard1(value1, 100);
        ValueGuard guard2(value2, 200);

        using std::swap;
        swap(guard1, guard2);

        FATP_ASSERT_EQ(value1, 100, "Value1 still modified");
        FATP_ASSERT_EQ(value2, 200, "Value2 still modified");
    }

    FATP_ASSERT_EQ(value1, 42, "Value1 restored by swapped guard");
    FATP_ASSERT_EQ(value2, 99, "Value2 restored by swapped guard");

    return true;
}

/**
 * @brief Tests swap with a released (inactive) guard
 *
 * Verifies that swap exchanges ALL state including active flag.
 * After swap:
 * - g1 (originally active for b) becomes inactive (was g2's state)
 * - g2 (originally inactive) becomes active for a
 */
FATP_TEST_CASE(swap_with_released)
{
    int a = 10;
    int b = 20;

    {
        ValueGuard g1(a, 100); // a: 10 -> 100, active
        ValueGuard g2(b, 200); // b: 20 -> 200, active

        g2.release(); // g2 now inactive
        FATP_ASSERT_FALSE(g2.is_active(), "g2 released");

        g1.swap(g2); // Exchange ALL state

        // After swap:
        // g1: manages b, but is now INACTIVE (was g2's state)
        // g2: manages a, and is ACTIVE (was g1's state)
        FATP_ASSERT_FALSE(g1.is_active(), "g1 inactive after swap (was g2's state)");
        FATP_ASSERT_TRUE(g2.is_active(), "g2 active after swap (was g1's state)");
    }

    // g1 destructs: inactive, no restore -> b stays at 200
    // g2 destructs: active, restores a -> a = 10
    FATP_ASSERT_EQ(a, 10, "a restored (g2 was active after swap)");
    FATP_ASSERT_EQ(b, 200, "b NOT restored (g1 was inactive after swap)");

    return true;
}

// =============================================================================
// Test Suite 9: Exception Safety
// =============================================================================

FATP_TEST_CASE(exception_during_mutation)
{
    int value = 42;
    bool exception_caught = false;

    try
    {
        ValueGuard guard(value, 100);
        FATP_ASSERT_EQ(value, 100, "Value changed");

        throw std::runtime_error("Test exception");
    }
    catch (const std::exception&)
    {
        exception_caught = true;
    }

    FATP_ASSERT_TRUE(exception_caught, "Exception caught");
    FATP_ASSERT_EQ(value, 42, "Value restored despite exception");

    return true;
}

FATP_TEST_CASE(exception_safety_with_move)
{
    std::string str = "original";
    bool exception_caught = false;

    try
    {
        ValueGuard guard(str, std::string("temporary"));
        FATP_ASSERT_EQ(str, "temporary", "String changed");

        throw std::runtime_error("Test exception");
    }
    catch (const std::exception&)
    {
        exception_caught = true;
    }

    FATP_ASSERT_TRUE(exception_caught, "Exception caught");
    FATP_ASSERT_EQ(str, "original", "String restored despite exception");

    return true;
}

/**
 * @brief Tests constructor exception safety (strong guarantee)
 *
 * If assignment to target throws during construction, the target
 * should be restored to its original state (not left moved-from).
 */
FATP_TEST_CASE(constructor_exception_safety)
{
    // throw_countdown=1: throws on first assignment, allows recovery assignment
    ThrowOnAssign target(42, 1);
    bool exception_caught = false;

    try
    {
        // This should throw during *mTarget = std::move(new_value)
        // The catch block should restore target to original state
        ValueGuard<ThrowOnAssign, ValueGuardMovePolicy<ThrowOnAssign>> guard(target, ThrowOnAssign(100, 0));

        FATP_ASSERT_TRUE(false, "Should have thrown");
    }
    catch (const std::runtime_error&)
    {
        exception_caught = true;
    }

    FATP_ASSERT_TRUE(exception_caught, "Exception caught from constructor");
    // Target should be restored, not left in moved-from state
    FATP_ASSERT_EQ(target.value, 42, "Target restored after constructor exception");

    return true;
}

FATP_TEST_CASE(no_throw_guarantee_copy)
{
    // CopyPolicy with nothrow copy-assignable type
    static_assert(ValueGuardCopyPolicy<int>::is_nothrow_restore, "CopyPolicy<int> should be nothrow");
    return true;
}

FATP_TEST_CASE(no_throw_guarantee_move)
{
    // MovePolicy with nothrow move-assignable type
    static_assert(ValueGuardMovePolicy<int>::is_nothrow_restore, "MovePolicy<int> should be nothrow");
    return true;
}

// =============================================================================
// Test Suite 10: Deduction Guides
// =============================================================================

FATP_TEST_CASE(deduction_guide_copy)
{
    int value = 42;
    const int new_val = 100;

    {
        ValueGuard guard(value, new_val); // Should use CopyPolicy
        FATP_ASSERT_EQ(value, 100, "Value changed");
    }

    FATP_ASSERT_EQ(value, 42, "Value restored");
    return true;
}

FATP_TEST_CASE(deduction_guide_move)
{
    std::string str = "original";

    {
        ValueGuard guard(str, std::string("temporary")); // Should use MovePolicy
        FATP_ASSERT_EQ(str, "temporary", "String changed");
    }

    FATP_ASSERT_EQ(str, "original", "String restored");
    return true;
}

FATP_TEST_CASE(deduction_guide_custom)
{
    int value = 42;

    {
        // 2-arg callable -> CustomPolicy (NOT ConditionalPolicy)
        ValueGuard guard(value,
                         100,
                         [](int& t, int&& o)
                         {
                             t = o;
                         });
        FATP_ASSERT_EQ(value, 100, "Value changed");
    }

    FATP_ASSERT_EQ(value, 42, "Value restored");
    return true;
}

/**
 * @brief Tests deduction guide disambiguation between Custom and Conditional
 *
 * Verifies that:
 * - 2-arg callable (T&, T&&) -> CustomPolicy
 * - 0-arg callable () -> bool -> ConditionalPolicy
 */
FATP_TEST_CASE(deduction_guide_disambiguation)
{
    int value = 42;

    // Test 1: 2-arg callable should select CustomPolicy
    {
        ValueGuard guard1(value,
                          100,
                          [](int& target, int&& original)
                          {
                              target = original + 1; // Custom restore logic
                          });
        FATP_ASSERT_EQ(value, 100, "Custom: value changed");
    }
    FATP_ASSERT_EQ(value, 43, "Custom: restored with +1");

    // Reset
    value = 42;

    // Test 2: 0-arg callable should select ConditionalPolicy
    {
        bool do_restore = true;
        ValueGuard guard2(value,
                          100,
                          [&do_restore]()
                          {
                              return do_restore;
                          });
        FATP_ASSERT_EQ(value, 100, "Conditional: value changed");
    }
    FATP_ASSERT_EQ(value, 42, "Conditional: restored (condition true)");

    return true;
}

// =============================================================================
// Test Suite 11: Integration Patterns
// =============================================================================

FATP_TEST_CASE(nested_guards)
{
    int outer = 1;
    int inner = 2;

    {
        ValueGuard guard_outer(outer, 10);
        FATP_ASSERT_EQ(outer, 10, "Outer changed");

        {
            ValueGuard guard_inner(inner, 20);
            FATP_ASSERT_EQ(inner, 20, "Inner changed");
        }

        FATP_ASSERT_EQ(inner, 2, "Inner restored");
        FATP_ASSERT_EQ(outer, 10, "Outer still changed");
    }

    FATP_ASSERT_EQ(outer, 1, "Outer restored");
    FATP_ASSERT_EQ(inner, 2, "Inner still restored");

    return true;
}

FATP_TEST_CASE(multiple_guards_same_value)
{
    int value = 1;

    {
        ValueGuard guard1(value, 10); // 1 -> 10, will restore to 1
        FATP_ASSERT_EQ(value, 10, "First guard changed");

        {
            ValueGuard guard2(value, 100); // 10 -> 100, will restore to 10
            FATP_ASSERT_EQ(value, 100, "Second guard changed");
        }

        FATP_ASSERT_EQ(value, 10, "Second guard restored to 10");
    }

    FATP_ASSERT_EQ(value, 1, "First guard restored to 1");
    return true;
}

FATP_TEST_CASE(guard_with_flag_toggle)
{
    bool debug_mode = false;

    {
        ValueGuard guard(debug_mode, true);
        FATP_ASSERT_TRUE(debug_mode, "Debug mode enabled");

        // ... do debug work ...
    }

    FATP_ASSERT_FALSE(debug_mode, "Debug mode disabled");
    return true;
}

FATP_TEST_CASE(guard_with_counter)
{
    int recursion_depth = 0;

    auto recursive_func = [&recursion_depth](auto& self, int n) -> int
    {
        ValueGuard guard(recursion_depth, recursion_depth + 1);

        if (n <= 1)
        {
            return 1;
        }
        return n * self(self, n - 1);
    };

    int result = recursive_func(recursive_func, 5);

    FATP_ASSERT_EQ(result, 120, "Factorial calculated correctly");
    FATP_ASSERT_EQ(recursion_depth, 0, "Recursion depth restored");

    return true;
}

FATP_TEST_CASE(raii_pattern_file_mode)
{
    enum class FileMode
    {
        Read,
        Write,
        Append
    };
    FileMode mode = FileMode::Read;

    {
        ValueGuard guard(mode, FileMode::Write);
        FATP_ASSERT_TRUE(mode == FileMode::Write, "Mode changed to Write");

        // ... write operations ...
    }

    FATP_ASSERT_TRUE(mode == FileMode::Read, "Mode restored to Read");
    return true;
}

// =============================================================================
// Test Suite 12: Edge Cases
// =============================================================================

FATP_TEST_CASE(self_assignment_prevention)
{
    int value = 42;

    {
        ValueGuard guard(value, 100);

        // Self-assignment should be a no-op
        guard = std::move(guard);

        FATP_ASSERT_TRUE(guard.is_active(), "Guard still active");
        FATP_ASSERT_EQ(value, 100, "Value unchanged");
    }

    FATP_ASSERT_EQ(value, 42, "Value restored");
    return true;
}

FATP_TEST_CASE(multiple_release_calls)
{
    int value = 42;

    {
        ValueGuard guard(value, 100);

        guard.release();
        FATP_ASSERT_FALSE(guard.is_active(), "Guard inactive");

        guard.release(); // Should be safe to call again
        FATP_ASSERT_FALSE(guard.is_active(), "Guard still inactive");
    }

    FATP_ASSERT_EQ(value, 100, "Value not restored (released)");
    return true;
}

struct EmptyStruct
{
};

FATP_TEST_CASE(zero_sized_type)
{
    EmptyStruct obj;

    {
        ValueGuard<EmptyStruct, ValueGuardCopyPolicy<EmptyStruct>> guard(obj, EmptyStruct{});
        FATP_ASSERT_TRUE(guard.is_active(), "Guard active for empty struct");
    }

    return true;
}

FATP_TEST_CASE(large_type)
{
    std::array<int, 1000> large_array;
    large_array.fill(42);

    {
        std::array<int, 1000> new_array;
        new_array.fill(100);

        ValueGuard guard(large_array, new_array);
        FATP_ASSERT_EQ(large_array[0], 100, "Large array changed");
        FATP_ASSERT_EQ(large_array[999], 100, "Large array end changed");
    }

    FATP_ASSERT_EQ(large_array[0], 42, "Large array restored");
    FATP_ASSERT_EQ(large_array[999], 42, "Large array end restored");

    return true;
}

// =============================================================================
// Test Suite 13: Lifecycle Tracking
// =============================================================================

FATP_TEST_CASE(lifecycle_copy_counts)
{
    LifecycleTracker::reset_counters();

    LifecycleTracker obj(42);
    LifecycleTracker new_val(100);

    {
        ValueGuard guard(obj, new_val);
        FATP_ASSERT_EQ(obj.value, 100, "Value changed");
    }

    FATP_ASSERT_EQ(obj.value, 42, "Value restored");

    // Verify lifecycle events
    FATP_ASSERT_TRUE(LifecycleTracker::copy_construct_count > 0, "Copy construction occurred");
    FATP_ASSERT_TRUE(LifecycleTracker::copy_assign_count > 0, "Copy assignment occurred");

    return true;
}

FATP_TEST_CASE(lifecycle_move_counts)
{
    LifecycleTracker::reset_counters();

    LifecycleTracker obj(42);

    {
        ValueGuard<LifecycleTracker, ValueGuardMovePolicy<LifecycleTracker>> guard(obj, LifecycleTracker(100));
        FATP_ASSERT_EQ(obj.value, 100, "Value changed");
    }

    FATP_ASSERT_EQ(obj.value, 42, "Value restored");

    // Verify move operations were used
    FATP_ASSERT_TRUE(LifecycleTracker::move_assign_count > 0, "Move assignment occurred");

    return true;
}

// =============================================================================
// Benchmark Functions (Separated from tests)
// =============================================================================

/**
 * @brief Prevents compiler from optimizing away the result
 */
template <typename T>
void DoNotOptimize(T&& value)
{
#if defined(_MSC_VER)
    // MSVC: Use volatile to prevent optimization
    static volatile char sink;
    sink = static_cast<char>(reinterpret_cast<uintptr_t>(&value) & 0xFF);
    (void)sink;
#else
    // GCC/Clang: Use inline assembly
    asm volatile("" : : "r,m"(value) : "memory");
#endif
}

/**
 * @brief Measures execution time of a function
 */
template <typename Func>
double measure_perf(Func&& func, int iterations)
{
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i)
    {
        func();
    }
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::nano>(end - start).count() / iterations;
}

void benchmark_value_guard()
{
    constexpr int ITERATIONS = 100000;
    auto& out = *get_test_config().output;

    out << "\n=== ValueGuard Benchmarks ===\n";

    // Benchmark 1: Copy Policy
    {
        int value = 42;
        double ns = measure_perf(
            [&value]()
            {
                ValueGuard guard(value, 100);
                DoNotOptimize(value);
            },
            ITERATIONS);
        out << "Copy Policy (int):     " << ns << " ns/op\n";
    }

    // Benchmark 2: Move Policy
    {
        std::string str = "original_value_for_testing";
        double ns = measure_perf(
            [&str]()
            {
                ValueGuard guard(str, std::string("temporary_value"));
                DoNotOptimize(str);
            },
            ITERATIONS);
        out << "Move Policy (string):  " << ns << " ns/op\n";
    }

    // Benchmark 3: Custom Policy
    {
        int value = 42;
        double ns = measure_perf(
            [&value]()
            {
                auto guard = make_value_guard_custom(value,
                                                     100,
                                                     [](int& t, int&& o) noexcept
                                                     {
                                                         t = o;
                                                     });
                DoNotOptimize(value);
            },
            ITERATIONS);
        out << "Custom Policy (int):   " << ns << " ns/op\n";
    }

    // Benchmark 4: Conditional Policy
    {
        int value = 42;
        double ns = measure_perf(
            [&value]()
            {
                auto guard = make_value_guard_conditional(value,
                                                          100,
                                                          []() noexcept
                                                          {
                                                              return true;
                                                          });
                DoNotOptimize(value);
            },
            ITERATIONS);
        out << "Conditional Policy:    " << ns << " ns/op\n";
    }

    // Benchmark 5: Guard move construction
    {
        int value = 42;
        double ns = measure_perf(
            [&value]()
            {
                ValueGuard guard1(value, 100);
                ValueGuard guard2(std::move(guard1));
                DoNotOptimize(guard2);
            },
            ITERATIONS);
        out << "Move construction:     " << ns << " ns/op\n";
    }

    out << "=== Benchmarks Complete ===\n";
}

// =============================================================================
// Performance Tests (validation, not benchmarking)
// =============================================================================

FATP_TEST_CASE(performance_copy_policy)
{
    int value = 42;
    constexpr int ITERATIONS = 10000;

    for (int i = 0; i < ITERATIONS; ++i)
    {
        ValueGuard guard(value, i);
        DoNotOptimize(value);
    }

    FATP_ASSERT_EQ(value, 42, "Value restored after many iterations");
    return true;
}

FATP_TEST_CASE(performance_move_policy)
{
    std::string str = "original";
    constexpr int ITERATIONS = 10000;

    for (int i = 0; i < ITERATIONS; ++i)
    {
        ValueGuard guard(str, std::string("temp"));
        DoNotOptimize(str);
    }

    FATP_ASSERT_EQ(str, "original", "String restored after many iterations");
    return true;
}

FATP_TEST_CASE(performance_custom_policy)
{
    int value = 42;
    constexpr int ITERATIONS = 10000;

    for (int i = 0; i < ITERATIONS; ++i)
    {
        auto guard = make_value_guard_custom(value,
                                             i,
                                             [](int& t, int&& o) noexcept
                                             {
                                                 t = o;
                                             });
        DoNotOptimize(value);
    }

    FATP_ASSERT_EQ(value, 42, "Value restored after many custom iterations");
    return true;
}

} // namespace fat_p::testing::value_guard

// =============================================================================
// Main Test Runner
// =============================================================================

namespace fat_p::testing
{

bool test_ValueGuard()
{
    FATP_PRINT_HEADER(VALUE GUARD v2.4)

    TestRunner runner;
    auto& out = *get_test_config().output;

    // Test Suite 1: Basic Copy Policy
    out << colors::bold() << "=== Test Suite 1: Basic Copy Policy ===" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, value_guard, basic_copy_restoration);
    FATP_RUN_TEST_NS(runner, value_guard, basic_copy_with_string);
    FATP_RUN_TEST_NS(runner, value_guard, copy_policy_explicit);
    FATP_RUN_TEST_NS(runner, value_guard, introspection_original);

    // Test Suite 2: Move Policy
    out << "\n" << colors::bold() << "=== Test Suite 2: Move Policy ===" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, value_guard, basic_move_restoration);
    FATP_RUN_TEST_NS(runner, value_guard, move_policy_explicit);
    FATP_RUN_TEST_NS(runner, value_guard, move_only_type);
    FATP_RUN_TEST_NS(runner, value_guard, factory_move);

    // Test Suite 3: NoRestore Policy
    out << "\n" << colors::bold() << "=== Test Suite 3: NoRestore Policy ===" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, value_guard, no_restore_policy);
    FATP_RUN_TEST_NS(runner, value_guard, no_restore_factory);

    // Test Suite 4: Conditional Policy
    out << "\n" << colors::bold() << "=== Test Suite 4: Conditional Policy ===" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, value_guard, conditional_restore_true);
    FATP_RUN_TEST_NS(runner, value_guard, conditional_restore_false);
    FATP_RUN_TEST_NS(runner, value_guard, conditional_dynamic_decision);

    // Test Suite 5: Custom Policy
    out << "\n" << colors::bold() << "=== Test Suite 5: Custom Policy ===" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, value_guard, custom_restorer);
    FATP_RUN_TEST_NS(runner, value_guard, custom_restorer_with_logging);
    FATP_RUN_TEST_NS(runner, value_guard, custom_restorer_incremental);

    // Test Suite 6: Early Release
    out << "\n" << colors::bold() << "=== Test Suite 6: Early Release ===" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, value_guard, release_basic);
    FATP_RUN_TEST_NS(runner, value_guard, release_conditional);

    // Test Suite 7: Move Semantics of Guard
    out << "\n" << colors::bold() << "=== Test Suite 7: Guard Move Semantics ===" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, value_guard, guard_move_constructor);
    FATP_RUN_TEST_NS(runner, value_guard, guard_move_assignment);
    FATP_RUN_TEST_NS(runner, value_guard, self_move_assignment);
    FATP_RUN_TEST_NS(runner, value_guard, guard_in_vector);

    // Test Suite 8: Swap Operations
    out << "\n" << colors::bold() << "=== Test Suite 8: Swap Operations ===" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, value_guard, swap_member);
    FATP_RUN_TEST_NS(runner, value_guard, swap_std);
    FATP_RUN_TEST_NS(runner, value_guard, swap_with_released);

    // Test Suite 9: Exception Safety
    out << "\n" << colors::bold() << "=== Test Suite 9: Exception Safety ===" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, value_guard, exception_during_mutation);
    FATP_RUN_TEST_NS(runner, value_guard, exception_safety_with_move);
    FATP_RUN_TEST_NS(runner, value_guard, constructor_exception_safety);
    FATP_RUN_TEST_NS(runner, value_guard, no_throw_guarantee_copy);
    FATP_RUN_TEST_NS(runner, value_guard, no_throw_guarantee_move);

    // Test Suite 10: Deduction Guides
    out << "\n" << colors::bold() << "=== Test Suite 10: Deduction Guides ===" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, value_guard, deduction_guide_copy);
    FATP_RUN_TEST_NS(runner, value_guard, deduction_guide_move);
    FATP_RUN_TEST_NS(runner, value_guard, deduction_guide_custom);
    FATP_RUN_TEST_NS(runner, value_guard, deduction_guide_disambiguation);

    // Test Suite 11: Integration Patterns
    out << "\n" << colors::bold() << "=== Test Suite 11: Integration Patterns ===" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, value_guard, nested_guards);
    FATP_RUN_TEST_NS(runner, value_guard, multiple_guards_same_value);
    FATP_RUN_TEST_NS(runner, value_guard, guard_with_flag_toggle);
    FATP_RUN_TEST_NS(runner, value_guard, guard_with_counter);
    FATP_RUN_TEST_NS(runner, value_guard, raii_pattern_file_mode);

    // Test Suite 12: Edge Cases
    out << "\n" << colors::bold() << "=== Test Suite 12: Edge Cases ===" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, value_guard, self_assignment_prevention);
    FATP_RUN_TEST_NS(runner, value_guard, multiple_release_calls);
    FATP_RUN_TEST_NS(runner, value_guard, zero_sized_type);
    FATP_RUN_TEST_NS(runner, value_guard, large_type);

    // Test Suite 13: Lifecycle Tracking
    out << "\n" << colors::bold() << "=== Test Suite 13: Lifecycle Tracking ===" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, value_guard, lifecycle_copy_counts);
    FATP_RUN_TEST_NS(runner, value_guard, lifecycle_move_counts);

    // Test Suite 14: Performance Validation
    out << "\n" << colors::bold() << "=== Test Suite 14: Performance Validation ===" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, value_guard, performance_copy_policy);
    FATP_RUN_TEST_NS(runner, value_guard, performance_move_policy);
    FATP_RUN_TEST_NS(runner, value_guard, performance_custom_policy);

    // Optionally run benchmarks
#ifndef NDEBUG
    std::cout << "\n[Debug build - skipping benchmarks]\n";
#else
    fat_p::testing::value_guard::benchmark_value_guard();
#endif

    // Print summary
    int failed = runner.print_summary();


    return failed == 0;
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_ValueGuard() ? 0 : 1;
}
#endif
