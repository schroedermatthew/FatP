/**
 * @file test_ValueGuard.cpp
 * @brief Comprehensive unit tests for ValueGuard.h v2.0
 * 
 * Tests all features:
 * - Basic RAII value restoration (copy and move policies)
 * - Policy extensibility (NoRestore, Conditional, Custom)
 * - Early release functionality
 * - State introspection (is_active, original, current)
 * - Move semantics (construction and assignment)
 * - Support for move-only types
 * - Deduction guides
 * - Exception safety
 * - Performance characteristics
 * - Integration patterns
 * 
 * Test Configuration:
 * - Processor: Intel Core i7-8850H @ 2.60GHz
 * - RAM: 32GB
 * - C++ Standard: C++17
 * - Build Modes: Debug and Release
 * 
 * Compilation:
 * - Debug:   g++ -std=c++17 -g test_ValueGuard.cpp -o test_value_guard_debug
 * - Release: g++ -std=c++17 -O3 -DNDEBUG test_ValueGuard.cpp -o test_value_guard_release
 * 
 * @version 2.0
 * @date 2025
 */

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <thread>
#include <atomic>
#include <sstream>

// Include the updated ValueGuard header
#include "ValueGuard.h"
#include "FatPTest.h"

namespace fat_p::testing {

using namespace fat_p;

// =============================================================================
// Helper Classes
// =============================================================================

/**
 * @brief Test object that tracks lifecycle events
 */
class LifecycleTracker {
public:
    static inline std::atomic<int> construct_count{0};
    static inline std::atomic<int> copy_construct_count{0};
    static inline std::atomic<int> move_construct_count{0};
    static inline std::atomic<int> copy_assign_count{0};
    static inline std::atomic<int> move_assign_count{0};
    static inline std::atomic<int> destruct_count{0};
    
    int value;
    
    explicit LifecycleTracker(int v = 0) : value(v) {
        ++construct_count;
    }
    
    LifecycleTracker(const LifecycleTracker& other) : value(other.value) {
        ++copy_construct_count;
    }
    
    LifecycleTracker(LifecycleTracker&& other) noexcept : value(other.value) {
        ++move_construct_count;
        other.value = -1;
    }
    
    LifecycleTracker& operator=(const LifecycleTracker& other) {
        value = other.value;
        ++copy_assign_count;
        return *this;
    }
    
    LifecycleTracker& operator=(LifecycleTracker&& other) noexcept {
        value = other.value;
        ++move_assign_count;
        other.value = -1;
        return *this;
    }
    
    ~LifecycleTracker() {
        ++destruct_count;
    }
    
    static void reset_counters() {
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
class MoveOnly {
public:
    static inline std::atomic<int> construct_count{0};
    static inline std::atomic<int> move_construct_count{0};
    static inline std::atomic<int> move_assign_count{0};
    static inline std::atomic<int> destruct_count{0};
    
    int value;
    
    explicit MoveOnly(int v = 0) : value(v) {
        ++construct_count;
    }
    
    MoveOnly(const MoveOnly&) = delete;
    MoveOnly& operator=(const MoveOnly&) = delete;
    
    MoveOnly(MoveOnly&& other) noexcept : value(other.value) {
        ++move_construct_count;
        other.value = -1;
    }
    
    MoveOnly& operator=(MoveOnly&& other) noexcept {
        value = other.value;
        ++move_assign_count;
        other.value = -1;
        return *this;
    }
    
    ~MoveOnly() {
        ++destruct_count;
    }
    
    static void reset_counters() {
        construct_count = 0;
        move_construct_count = 0;
        move_assign_count = 0;
        destruct_count = 0;
    }
};

// =============================================================================
// Test Suite 1: Basic Functionality - Copy Policy
// =============================================================================

bool test_basic_copy_restoration() {
    int value = 42;
    
    {
        ValueGuard guard(value, 100);
        ASSERT_EQ(value, 100, "Value changed to new value");
    }
    
    ASSERT_EQ(value, 42, "Value restored to original");
    return true;
}

bool test_basic_copy_with_string() {
    std::string str = "original";
    
    {
        ValueGuard guard(str, std::string("temporary"));
        ASSERT_EQ(str, "temporary", "String changed to new value");
    }
    
    ASSERT_EQ(str, "original", "String restored to original");
    return true;
}

bool test_copy_policy_explicit() {
    double value = 3.14;
    
    {
        ValueGuard<double, ValueGuardCopyPolicy<double>> guard(value, 2.71);
        ASSERT_EQ(value, 2.71, "Value changed");
    }
    
    ASSERT_EQ(value, 3.14, "Value restored");
    return true;
}

bool test_introspection_original() {
    int value = 42;
    
    ValueGuard guard(value, 100);
    ASSERT_EQ(guard.original(), 42, "Original value accessible");
    ASSERT_EQ(guard.current(), 100, "Current value accessible");
    ASSERT_TRUE(guard.is_active(), "Guard is active");
    
    return true;
}

// =============================================================================
// Test Suite 2: Move Policy
// =============================================================================

bool test_basic_move_restoration() {
    std::string str = "original";
    
    {
        ValueGuard guard(str, std::string("temporary"));
        ASSERT_EQ(str, "temporary", "Value moved to new value");
    }
    
    ASSERT_EQ(str, "original", "Value restored");
    return true;
}

bool test_move_policy_explicit() {
    std::unique_ptr<int> ptr = std::make_unique<int>(42);
    int* original_addr = ptr.get();
    
    {
        ValueGuard<std::unique_ptr<int>, ValueGuardMovePolicy<std::unique_ptr<int>>> 
            guard(ptr, std::make_unique<int>(100));
        
        ASSERT_TRUE(ptr != nullptr, "Ptr has new value");
        ASSERT_EQ(*ptr, 100, "New value is 100");
        ASSERT_NE(ptr.get(), original_addr, "Pointer changed");
    }
    
    ASSERT_TRUE(ptr != nullptr, "Ptr restored");
    ASSERT_EQ(ptr.get(), original_addr, "Original pointer restored");
    ASSERT_EQ(*ptr, 42, "Original value restored");
    
    return true;
}

bool test_move_only_type() {
    MoveOnly::reset_counters();
    
    MoveOnly obj(42);
    
    {
        ValueGuard<MoveOnly, ValueGuardMovePolicy<MoveOnly>> 
            guard(obj, MoveOnly(100));
        
        ASSERT_EQ(obj.value, 100, "Value changed");
    }
    
    ASSERT_EQ(obj.value, 42, "Value restored for move-only type");
    
    return true;
}

bool test_factory_move() {
    std::string str = "original";
    
    {
        auto guard = make_value_guard_move(str, std::string("temporary"));
        ASSERT_EQ(str, "temporary", "Factory move worked");
    }
    
    ASSERT_EQ(str, "original", "Value restored via factory");
    return true;
}

// =============================================================================
// Test Suite 3: NoRestore Policy
// =============================================================================

bool test_no_restore_policy() {
    int value = 42;
    
    {
        ValueGuard<int, ValueGuardNoRestorePolicy<int>> guard(value, 100);
        ASSERT_EQ(value, 100, "Value changed");
    }
    
    ASSERT_EQ(value, 100, "Value NOT restored (no-restore policy)");
    return true;
}

bool test_no_restore_factory() {
    std::string str = "original";
    
    {
        auto guard = make_value_guard_no_restore(str, std::string("permanent"));
        ASSERT_EQ(str, "permanent", "Value changed");
    }
    
    ASSERT_EQ(str, "permanent", "Value NOT restored");
    return true;
}

// =============================================================================
// Test Suite 4: Conditional Policy
// =============================================================================

bool test_conditional_restore_true() {
    int value = 42;
    bool should_restore = true;
    
    {
        auto guard = make_value_guard_conditional(
            value, 
            std::move(100), 
            [&should_restore]() { return should_restore; }
        );
        ASSERT_EQ(value, 100, "Value changed");
    }
    
    ASSERT_EQ(value, 42, "Value restored (condition true)");
    return true;
}

bool test_conditional_restore_false() {
    int value = 42;
    bool should_restore = false;
    
    {
        auto guard = make_value_guard_conditional(
            value, 
            std::move(100), 
            [&should_restore]() { return should_restore; }
        );
        ASSERT_EQ(value, 100, "Value changed");
    }
    
    ASSERT_EQ(value, 100, "Value NOT restored (condition false)");
    return true;
}

bool test_conditional_dynamic_decision() {
    int value = 42;
    int counter = 0;
    
    {
        auto guard = make_value_guard_conditional(
            value, 
            std::move(100), 
            [&counter]() { return ++counter > 0; }
        );
        ASSERT_EQ(value, 100, "Value changed");
        // Condition will be true when destructor runs
    }
    
    ASSERT_EQ(value, 42, "Value restored (condition evaluated at destruction)");
    ASSERT_EQ(counter, 1, "Condition invoked once");
    
    return true;
}

// =============================================================================
// Test Suite 5: Custom Policy
// =============================================================================

bool test_custom_restorer() {
    int value = 42;
    int custom_restore_value = 999;
    
    {
        auto guard = make_value_guard_custom(
            value,
            100,
            [custom_restore_value](int& target, int&&) noexcept {
                target = custom_restore_value;
            }
        );
        ASSERT_EQ(value, 100, "Value changed");
    }
    
    ASSERT_EQ(value, 999, "Custom restorer used");
    return true;
}

bool test_custom_restorer_with_logging() {
    std::string value = "original";
    std::string log;
    
    {
        auto guard = make_value_guard_custom(
            value,
            std::string("temp"),
            [&log](std::string& target, std::string&& orig) noexcept {
                log = "Restoring from " + target + " to " + orig;
                target = std::move(orig);
            }
        );
        ASSERT_EQ(value, "temp", "Value changed");
    }
    
    ASSERT_EQ(value, "original", "Value restored");
    ASSERT_TRUE(log.find("Restoring") != std::string::npos, "Custom logic executed");
    
    return true;
}

bool test_custom_restorer_incremental() {
    int value = 10;
    
    {
        auto guard = make_value_guard_custom(
            value,
            50,
            [](int& target, int&& original) noexcept {
                // Restore with increment
                target = original + 5;
            }
        );
        ASSERT_EQ(value, 50, "Value changed");
    }
    
    ASSERT_EQ(value, 15, "Custom restore with modification");
    return true;
}

// =============================================================================
// Test Suite 6: Early Release
// =============================================================================

bool test_release_basic() {
    int value = 42;
    
    {
        ValueGuard guard(value, 100);
        ASSERT_EQ(value, 100, "Value changed");
        ASSERT_TRUE(guard.is_active(), "Guard active before release");
        
        guard.release();
        
        ASSERT_FALSE(guard.is_active(), "Guard inactive after release");
    }
    
    ASSERT_EQ(value, 100, "Value NOT restored after release");
    return true;
}

bool test_release_conditional() {
    int value = 42;
    bool commit = true;
    
    {
        ValueGuard guard(value, 100);
        ASSERT_EQ(value, 100, "Value changed");
        
        if (commit) {
            guard.release();
        }
    }
    
    ASSERT_EQ(value, 100, "Value committed (not restored)");
    
    // Test without commit
    value = 42;
    commit = false;
    
    {
        ValueGuard guard(value, 100);
        if (commit) {
            guard.release();
        }
    }
    
    ASSERT_EQ(value, 42, "Value restored (not committed)");
    return true;
}

// =============================================================================
// Test Suite 7: Move Semantics of Guard
// =============================================================================

bool test_guard_move_constructor() {
    int value = 42;
    
    {
        ValueGuard guard1(value, 100);
        ASSERT_TRUE(guard1.is_active(), "Guard1 active");
        
        ValueGuard guard2(std::move(guard1));
        ASSERT_FALSE(guard1.is_active(), "Guard1 inactive after move");
        ASSERT_TRUE(guard2.is_active(), "Guard2 active after move");
        
        ASSERT_EQ(value, 100, "Value still changed");
    }
    
    ASSERT_EQ(value, 42, "Value restored by moved guard");
    return true;
}

bool test_guard_move_assignment() {
    int value1 = 42;
    int value2 = 99;
    
    {
        ValueGuard guard1(value1, 100);
        ValueGuard guard2(value2, 200);
        
        guard2 = std::move(guard1);
        
        ASSERT_FALSE(guard1.is_active(), "Guard1 inactive after move assign");
        ASSERT_TRUE(guard2.is_active(), "Guard2 active after move assign");
    }
    
    ASSERT_EQ(value1, 42, "Value1 restored");
    ASSERT_EQ(value2, 200, "Value2 stays at modified (guard1 took over)");
    
    return true;
}

bool test_guard_in_vector() {
    int value = 42;
    
    std::vector<ValueGuard<int, ValueGuardCopyPolicy<int>>> guards;
    guards.reserve(10);
    
    {
        ValueGuard<int, ValueGuardCopyPolicy<int>> guard(value, 100);
        ASSERT_EQ(value, 100, "Value changed");
        
        guards.push_back(std::move(guard));
        ASSERT_FALSE(guard.is_active(), "Original guard inactive");
    }
    
    ASSERT_EQ(value, 100, "Value still changed (guard in vector)");
    
    guards.clear();
    
    ASSERT_EQ(value, 42, "Value restored after vector cleared");
    return true;
}

// =============================================================================
// Test Suite 8: Swap Operations
// =============================================================================

bool test_swap_member() {
    int value1 = 42;
    int value2 = 99;
    
    {
        ValueGuard guard1(value1, 100);
        ValueGuard guard2(value2, 200);
        
        guard1.swap(guard2);
        
        ASSERT_EQ(value1, 100, "Value1 still modified");
        ASSERT_EQ(value2, 200, "Value2 still modified");
    }
    
    // After swap, guard1 now manages value2 (restores to 99), guard2 manages value1 (restores to 42)
    ASSERT_EQ(value1, 42, "Value1 restored by swapped guard2");
    ASSERT_EQ(value2, 99, "Value2 restored by swapped guard1");
    
    return true;
}

bool test_swap_std() {
    int value1 = 42;
    int value2 = 99;
    
    {
        ValueGuard guard1(value1, 100);
        ValueGuard guard2(value2, 200);
        
        using std::swap;
        swap(guard1, guard2);
        
        ASSERT_EQ(value1, 100, "Value1 still modified");
        ASSERT_EQ(value2, 200, "Value2 still modified");
    }
    
    ASSERT_EQ(value1, 42, "Value1 restored by swapped guard");
    ASSERT_EQ(value2, 99, "Value2 restored by swapped guard");
    
    return true;
}

// =============================================================================
// Test Suite 9: Exception Safety
// =============================================================================

bool test_exception_during_mutation() {
    int value = 42;
    bool exception_caught = false;
    
    try {
        ValueGuard guard(value, 100);
        ASSERT_EQ(value, 100, "Value changed");
        
        throw std::runtime_error("Test exception");
    } catch (const std::exception&) {
        exception_caught = true;
    }
    
    ASSERT_TRUE(exception_caught, "Exception caught");
    ASSERT_EQ(value, 42, "Value restored despite exception");
    
    return true;
}

bool test_exception_safety_with_move() {
    std::unique_ptr<int> ptr = std::make_unique<int>(42);
    int* original_addr = ptr.get();
    bool exception_caught = false;
    
    try {
        ValueGuard<std::unique_ptr<int>, ValueGuardMovePolicy<std::unique_ptr<int>>>
            guard(ptr, std::make_unique<int>(100));
        
        ASSERT_TRUE(ptr != nullptr, "Ptr changed");
        throw std::runtime_error("Test exception");
    } catch (const std::exception&) {
        exception_caught = true;
    }
    
    ASSERT_TRUE(exception_caught, "Exception caught");
    ASSERT_TRUE(ptr != nullptr, "Ptr restored");
    ASSERT_EQ(ptr.get(), original_addr, "Original pointer restored");
    
    return true;
}

bool test_no_throw_guarantee_copy() {
    static_assert(noexcept(ValueGuardCopyPolicy<int>().execute(
        std::declval<int&>(), std::declval<const int&>())),
        "Copy policy execute should be noexcept for int");
    
    return true;
}

bool test_no_throw_guarantee_move() {
    static_assert(noexcept(ValueGuardMovePolicy<std::string>().execute(
        std::declval<std::string&>(), std::declval<std::string&&>())),
        "Move policy execute should be noexcept for string");
    
    return true;
}

// =============================================================================
// Test Suite 10: Deduction Guides
// =============================================================================

bool test_deduction_guide_copy() {
    int value = 42;
    
    {
        ValueGuard guard(value, 100);  // Should deduce ValueGuardCopyPolicy
        ASSERT_EQ(value, 100, "Value changed via deduction");
    }
    
    ASSERT_EQ(value, 42, "Value restored");
    return true;
}

bool test_deduction_guide_move() {
    std::string str = "original";
    
    {
        ValueGuard guard(str, std::string("temp"));  // Should deduce MovePolicy
        ASSERT_EQ(str, "temp", "Value changed via deduction");
    }
    
    ASSERT_EQ(str, "original", "Value restored");
    return true;
}

bool test_deduction_guide_custom() {
    int value = 42;
    
    {
        ValueGuard guard(value, 100, [](int& t, int&& o) noexcept { t = o + 1; });
        ASSERT_EQ(value, 100, "Value changed");
    }
    
    ASSERT_EQ(value, 43, "Custom restorer via deduction");
    return true;
}

// =============================================================================
// Test Suite 11: Integration Patterns
// =============================================================================

bool test_nested_guards() {
    int value = 0;
    
    {
        ValueGuard guard1(value, 10);
        ASSERT_EQ(value, 10, "First guard applied");
        
        {
            ValueGuard guard2(value, 20);
            ASSERT_EQ(value, 20, "Second guard applied");
        }
        
        ASSERT_EQ(value, 10, "Value restored to first guard");
    }
    
    ASSERT_EQ(value, 0, "Value fully restored");
    return true;
}

bool test_multiple_guards_same_value() {
    int value = 42;
    
    {
        ValueGuard guard1(value, 100);
        ASSERT_EQ(value, 100, "First modification");
        
        // This creates a second guard on the already modified value
        {
            ValueGuard guard2(value, 200);
            ASSERT_EQ(value, 200, "Second modification");
        }
        
        ASSERT_EQ(value, 100, "Restored to first guard's value");
    }
    
    ASSERT_EQ(value, 42, "Fully restored");
    return true;
}

bool test_guard_with_flag_toggle() {
    bool flag = false;
    
    {
        ValueGuard guard(flag, true);
        ASSERT_TRUE(flag, "Flag toggled on");
        
        // Simulate work with flag enabled
    }
    
    ASSERT_FALSE(flag, "Flag toggled off");
    return true;
}

bool test_guard_with_counter() {
    int counter = 0;
    
    {
        ValueGuard guard(counter, counter + 1);
        ASSERT_EQ(counter, 1, "Counter incremented");
        
        {
            ValueGuard guard2(counter, counter + 1);
            ASSERT_EQ(counter, 2, "Counter incremented again");
        }
        
        ASSERT_EQ(counter, 1, "Counter decremented");
    }
    
    ASSERT_EQ(counter, 0, "Counter fully restored");
    return true;
}

bool test_raii_pattern_file_mode() {
    std::string mode = "read";
    
    {
        ValueGuard guard(mode, std::string("write"));
        ASSERT_EQ(mode, "write", "Mode changed to write");
        
        // Simulate file operations in write mode
    }
    
    ASSERT_EQ(mode, "read", "Mode restored to read");
    return true;
}

// =============================================================================
// Test Suite 12: Performance
// =============================================================================

bool test_performance_copy_policy() {
    int value = 42;
    
    auto time_ns = measure_perf([&value]() {
        ValueGuard guard(value, 100);
    });
    
    std::cout << "  Copy policy: " << time_ns << " ns per guard\n";
    
    // Should be very fast (< 100ns on target hardware)
    ASSERT_TRUE(time_ns < 1000.0, "Copy policy should be fast");
    
    return true;
}

bool test_performance_move_policy() {
    std::string value = "original string value for testing";
    
    auto time_ns = measure_perf([&value]() {
        ValueGuard<std::string, ValueGuardMovePolicy<std::string>> 
            guard(value, std::string("temporary"));
    });
    
    std::cout << "  Move policy: " << time_ns << " ns per guard\n";
    
    ASSERT_TRUE(time_ns < 10000.0, "Move policy should be reasonably fast");
    
    return true;
}

bool test_performance_custom_policy() {
    int value = 42;
    
    auto time_ns = measure_perf([&value]() {
        auto guard = make_value_guard_custom(
            value, 100,
            [](int& t, int&& o) noexcept { t = o; }
        );
    });
    
    std::cout << "  Custom policy: " << time_ns << " ns per guard\n";
    
    ASSERT_TRUE(time_ns < 5000.0, "Custom policy should be fast");
    
    return true;
}

// =============================================================================
// Test Suite 13: Edge Cases
// =============================================================================

bool test_self_assignment_prevention() {
    int value = 42;
    
    {
        ValueGuard guard(value, value);  // Same value
        ASSERT_EQ(value, 42, "Value unchanged");
    }
    
    ASSERT_EQ(value, 42, "Value still correct after self-guard");
    return true;
}

bool test_multiple_release_calls() {
    int value = 42;
    
    {
        ValueGuard guard(value, 100);
        guard.release();
        guard.release();  // Second release should be safe
        guard.release();  // Third release should be safe
        
        ASSERT_FALSE(guard.is_active(), "Guard inactive");
    }
    
    ASSERT_EQ(value, 100, "Value not restored");
    return true;
}

bool test_zero_sized_type() {
    struct Empty {};
    Empty e;
    
    {
        ValueGuard guard(e, Empty{});
        // Just verify it compiles and runs
    }
    
    return true;
}

bool test_large_type() {
    struct Large {
        char data[1024];
        int value;
        
        Large(int v = 0) : value(v) {
            std::fill(std::begin(data), std::end(data), 0);
        }
        
        bool operator==(const Large& other) const {
            return value == other.value;
        }
    };
    
    Large large(42);
    
    {
        ValueGuard guard(large, Large(100));
        ASSERT_EQ(large.value, 100, "Large type modified");
    }
    
    ASSERT_EQ(large.value, 42, "Large type restored");
    return true;
}

// =============================================================================
// Test Suite 14: Lifecycle Tracking
// =============================================================================

bool test_lifecycle_copy_counts() {
    LifecycleTracker::reset_counters();
    
    LifecycleTracker obj(42);
    
    {
        LifecycleTracker new_val(100);
        ValueGuard guard(obj, new_val);
        ASSERT_EQ(obj.value, 100, "Value changed");
    }
    
    ASSERT_EQ(obj.value, 42, "Value restored");
    
    // Verify lifecycle events
    ASSERT_TRUE(LifecycleTracker::copy_construct_count > 0, 
                "Copy construction occurred");
    ASSERT_TRUE(LifecycleTracker::copy_assign_count > 0, 
                "Copy assignment occurred");
    
    return true;
}

bool test_lifecycle_move_counts() {
    LifecycleTracker::reset_counters();
    
    LifecycleTracker obj(42);
    
    {
        ValueGuard<LifecycleTracker, ValueGuardMovePolicy<LifecycleTracker>> 
            guard(obj, LifecycleTracker(100));
        ASSERT_EQ(obj.value, 100, "Value changed");
    }
    
    ASSERT_EQ(obj.value, 42, "Value restored");
    
    // Verify move operations were used
    ASSERT_TRUE(LifecycleTracker::move_assign_count > 0, 
                "Move assignment occurred");
    
    return true;
}

// =============================================================================
// Main Test Runner
// =============================================================================

bool test_ValueGuard() {

    PRINT_HEADER(VALUE GUARD)

    TestRunner runner;
    auto& out = *get_test_config().output; 

    // Test Suite 1: Basic Copy Policy
    out << colors::bold() << "=== Test Suite 1: Basic Copy Policy ===" 
        << colors::reset() << "\n";
    runner.run_test("basic_copy_restoration", test_basic_copy_restoration);
    runner.run_test("basic_copy_with_string", test_basic_copy_with_string);
    runner.run_test("copy_policy_explicit", test_copy_policy_explicit);
    runner.run_test("introspection_original", test_introspection_original);
    
    // Test Suite 2: Move Policy
    out << "\n" << colors::bold() << "=== Test Suite 2: Move Policy ===" 
        << colors::reset() << "\n";
    runner.run_test("basic_move_restoration", test_basic_move_restoration);
    runner.run_test("move_policy_explicit", test_move_policy_explicit);
    runner.run_test("move_only_type", test_move_only_type);
    runner.run_test("factory_move", test_factory_move);
    
    // Test Suite 3: NoRestore Policy
    out << "\n" << colors::bold() << "=== Test Suite 3: NoRestore Policy ===" 
        << colors::reset() << "\n";
    runner.run_test("no_restore_policy", test_no_restore_policy);
    runner.run_test("no_restore_factory", test_no_restore_factory);
    
    // Test Suite 4: Conditional Policy
    out << "\n" << colors::bold() << "=== Test Suite 4: Conditional Policy ===" 
        << colors::reset() << "\n";
    runner.run_test("conditional_restore_true", test_conditional_restore_true);
    runner.run_test("conditional_restore_false", test_conditional_restore_false);
    runner.run_test("conditional_dynamic_decision", test_conditional_dynamic_decision);
    
    // Test Suite 5: Custom Policy
    out << "\n" << colors::bold() << "=== Test Suite 5: Custom Policy ===" 
        << colors::reset() << "\n";
    runner.run_test("custom_restorer", test_custom_restorer);
    runner.run_test("custom_restorer_with_logging", test_custom_restorer_with_logging);
    runner.run_test("custom_restorer_incremental", test_custom_restorer_incremental);
    
    // Test Suite 6: Early Release
    out << "\n" << colors::bold() << "=== Test Suite 6: Early Release ===" 
        << colors::reset() << "\n";
    runner.run_test("release_basic", test_release_basic);
    runner.run_test("release_conditional", test_release_conditional);
    
    // Test Suite 7: Move Semantics of Guard
    out << "\n" << colors::bold() << "=== Test Suite 7: Guard Move Semantics ===" 
        << colors::reset() << "\n";
    runner.run_test("guard_move_constructor", test_guard_move_constructor);
    runner.run_test("guard_move_assignment", test_guard_move_assignment);
    runner.run_test("guard_in_vector", test_guard_in_vector);
    
    // Test Suite 8: Swap Operations
    out << "\n" << colors::bold() << "=== Test Suite 8: Swap Operations ===" 
        << colors::reset() << "\n";
    runner.run_test("swap_member", test_swap_member);
    runner.run_test("swap_std", test_swap_std);
    
    // Test Suite 9: Exception Safety
    out << "\n" << colors::bold() << "=== Test Suite 9: Exception Safety ===" 
        << colors::reset() << "\n";
    runner.run_test("exception_during_mutation", test_exception_during_mutation);
    runner.run_test("exception_safety_with_move", test_exception_safety_with_move);
    runner.run_test("no_throw_guarantee_copy", test_no_throw_guarantee_copy);
    runner.run_test("no_throw_guarantee_move", test_no_throw_guarantee_move);
    
    // Test Suite 10: Deduction Guides
    out << "\n" << colors::bold() << "=== Test Suite 10: Deduction Guides ===" 
        << colors::reset() << "\n";
    runner.run_test("deduction_guide_copy", test_deduction_guide_copy);
    runner.run_test("deduction_guide_move", test_deduction_guide_move);
    runner.run_test("deduction_guide_custom", test_deduction_guide_custom);
    
    // Test Suite 11: Integration Patterns
    out << "\n" << colors::bold() << "=== Test Suite 11: Integration Patterns ===" 
        << colors::reset() << "\n";
    runner.run_test("nested_guards", test_nested_guards);
    runner.run_test("multiple_guards_same_value", test_multiple_guards_same_value);
    runner.run_test("guard_with_flag_toggle", test_guard_with_flag_toggle);
    runner.run_test("guard_with_counter", test_guard_with_counter);
    runner.run_test("raii_pattern_file_mode", test_raii_pattern_file_mode);
    
    // Test Suite 12: Performance
    out << "\n" << colors::bold() << "=== Test Suite 12: Performance ===" 
        << colors::reset() << "\n";
    runner.run_test("performance_copy_policy", test_performance_copy_policy);
    runner.run_test("performance_move_policy", test_performance_move_policy);
    runner.run_test("performance_custom_policy", test_performance_custom_policy);
    
    // Test Suite 13: Edge Cases
    out << "\n" << colors::bold() << "=== Test Suite 13: Edge Cases ===" 
        << colors::reset() << "\n";
    runner.run_test("self_assignment_prevention", test_self_assignment_prevention);
    runner.run_test("multiple_release_calls", test_multiple_release_calls);
    runner.run_test("zero_sized_type", test_zero_sized_type);
    runner.run_test("large_type", test_large_type);
    
    // Test Suite 14: Lifecycle Tracking
    out << "\n" << colors::bold() << "=== Test Suite 14: Lifecycle Tracking ===" 
        << colors::reset() << "\n";
    runner.run_test("lifecycle_copy_counts", test_lifecycle_copy_counts);
    runner.run_test("lifecycle_move_counts", test_lifecycle_move_counts);
    
    // Print summary
    int failed = runner.print_summary();
    
    return failed == 0;
}

} // namespace fat_p::testing
