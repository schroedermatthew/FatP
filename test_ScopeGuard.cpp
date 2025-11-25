/**
 * @file test_ScopeGuard.cpp
 * @brief Comprehensive unit tests for ScopeGuard.h and ScopeGuardPolicies.h
 *
 * @details Complete test suite for:
 * - Basic RAII functionality
 * - All 4 exception policies (Nothrow, Terminate, LogAndSwallow, Rethrow)
 * - Move semantics (construction and assignment)
 * - Dismiss functionality (dismiss, dismiss_if)
 * - Macro convenience helpers (SCOPE_GUARD, SCOPE_GUARD_EX)
 * - Resource management patterns
 * - Performance characteristics
 * 
 * Note: ScopeGuard is designed for single-threaded use only.
 */

#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <stdexcept>
#include <chrono>
#include <cassert>

#include "ScopeGuard.h"
#include "ScopeGuardPolicies.h"
#include "test_ScopeGuard.h"
#include "FatPTest.h"

using namespace fat_p::testing;
using namespace fat_p;

namespace fat_p::testing
{

class TestResource {
public:
    static inline int construction_count{0};
    static inline int destruction_count{0};
    static inline int cleanup_count{0};
    
    int id;
    bool cleaned = false;
    
    explicit TestResource(int i = 0) : id(i) {
        ++construction_count;
    }
    
    ~TestResource() {
        ++destruction_count;
    }
    
    void cleanup() {
        cleaned = true;
        ++cleanup_count;
    }
    
    static void reset_counters() {
        construction_count = 0;
        destruction_count = 0;
        cleanup_count = 0;
    }
};

class ThrowingAction {
public:
    bool should_throw;
    bool was_called = false;
    
    explicit ThrowingAction(bool throw_on_call = false) 
        : should_throw(throw_on_call) {}
    
    void operator()() {
        was_called = true;
        if (should_throw) {
            throw std::runtime_error("Intentional test exception");
        }
    }
};

bool test_BasicScopeGuard() {
    std::cout << colors::cyan() << "\nTesting Basic ScopeGuard Functionality..."
              << colors::reset() << std::endl;
    
    // Test 1: Basic RAII cleanup
    {
        std::cout << colors::blue() << "  [TEST] Basic RAII cleanup"
                  << colors::reset() << std::endl;
        
        int cleanup_count = 0;
        {
            auto guard = makeScopeGuard([&cleanup_count]() {
                ++cleanup_count;
            });
            ASSERT_EQ(cleanup_count, 0, "Cleanup should not run yet");
        }
        ASSERT_EQ(cleanup_count, 1, "Cleanup should have run once");
    }
    
    // Test 2: Resource cleanup
    {
        std::cout << colors::blue() << "  [TEST] Resource cleanup"
                  << colors::reset() << std::endl;
        
        TestResource::reset_counters();
        {
            auto* resource = new TestResource(42);
            auto guard = makeScopeGuard([resource]() {
                resource->cleanup();
                delete resource;
            });
        }
        ASSERT_EQ(TestResource::construction_count, 1, "Should construct once");
        ASSERT_EQ(TestResource::destruction_count, 1, "Should destruct once");
        ASSERT_EQ(TestResource::cleanup_count, 1, "Should cleanup once");
    }
    
    // Test 3: Multiple guards in scope
    {
        std::cout << colors::blue() << "  [TEST] Multiple guards in scope"
                  << colors::reset() << std::endl;
        
        int count1 = 0, count2 = 0, count3 = 0;
        {
            auto guard1 = makeScopeGuard([&count1]() { ++count1; });
            auto guard2 = makeScopeGuard([&count2]() { ++count2; });
            auto guard3 = makeScopeGuard([&count3]() { ++count3; });
        }
        // Guards execute in reverse order of construction (stack unwinding)
        ASSERT_EQ(count1, 1, "Guard1 should execute");
        ASSERT_EQ(count2, 1, "Guard2 should execute");
        ASSERT_EQ(count3, 1, "Guard3 should execute");
    }
    
    // Test 4: Nested scopes
    {
        std::cout << colors::blue() << "  [TEST] Nested scopes"
                  << colors::reset() << std::endl;
        
        int outer = 0, inner = 0;
        {
            auto outer_guard = makeScopeGuard([&outer]() { ++outer; });
            ASSERT_EQ(outer, 0, "Outer guard not executed yet");
            {
                auto inner_guard = makeScopeGuard([&inner]() { ++inner; });
                ASSERT_EQ(inner, 0, "Inner guard not executed yet");
            }
            ASSERT_EQ(inner, 1, "Inner guard should have executed");
            ASSERT_EQ(outer, 0, "Outer guard still not executed");
        }
        ASSERT_EQ(outer, 1, "Outer guard should have executed");
    }
    
    std::cout << colors::green() << "Basic ScopeGuard: Tests passed."
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// II. Dismiss Functionality Tests
// =============================================================================

bool test_DismissFunctionality() {
    std::cout << colors::cyan() << "\nTesting Dismiss Functionality..."
              << colors::reset() << std::endl;
    
    // Test 1: Basic dismiss
    {
        std::cout << colors::blue() << "  [TEST] Basic dismiss"
                  << colors::reset() << std::endl;
        
        int cleanup_count = 0;
        {
            auto guard = makeScopeGuard([&cleanup_count]() { ++cleanup_count; });
            SIMPLE_ASSERT(guard.is_active(), "Guard should be active");
            guard.dismiss();
            SIMPLE_ASSERT(!guard.is_active(), "Guard should be dismissed");
        }
        ASSERT_EQ(cleanup_count, 0, "Dismissed guard should not execute");
    }
    
    // Test 2: Conditional dismiss (true)
    {
        std::cout << colors::blue() << "  [TEST] Conditional dismiss (true)"
                  << colors::reset() << std::endl;
        
        int cleanup_count = 0;
        bool success = true;
        {
            auto guard = makeScopeGuard([&cleanup_count]() { ++cleanup_count; });
            guard.dismiss_if(success);
        }
        ASSERT_EQ(cleanup_count, 0, "Should not execute when dismissed");
    }
    
    // Test 3: Conditional dismiss (false)
    {
        std::cout << colors::blue() << "  [TEST] Conditional dismiss (false)"
                  << colors::reset() << std::endl;
        
        int cleanup_count = 0;
        bool success = false;
        {
            auto guard = makeScopeGuard([&cleanup_count]() { ++cleanup_count; });
            guard.dismiss_if(success);
        }
        ASSERT_EQ(cleanup_count, 1, "Should execute when not dismissed");
    }
    
    // Test 4: Dismiss pattern for successful operations
    {
        std::cout << colors::blue() << "  [TEST] Dismiss pattern for successful operations"
                  << colors::reset() << std::endl;
        
        TestResource::reset_counters();
        auto* resource = new TestResource(42);
        bool operation_succeeded = false;
        
        {
            auto guard = makeScopeGuard([resource]() {
                resource->cleanup();
                delete resource;
            });
            
            // Simulate an operation that might fail
            try {
                // Operation succeeds
                operation_succeeded = true;
                guard.dismiss(); // Don't cleanup on success
            } catch (...) {
                // If operation fails, guard will clean up
            }
        }
        
        SIMPLE_ASSERT(operation_succeeded, "Operation should succeed");
        ASSERT_EQ(TestResource::cleanup_count, 0, "Should not cleanup on success");
        
        // Manual cleanup
        delete resource;
    }
    
    std::cout << colors::green() << "Dismiss Functionality: Tests passed."
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// III. Move Semantics Tests
// =============================================================================

bool test_MoveSemantics() {
    std::cout << colors::cyan() << "\nTesting Move Semantics..."
              << colors::reset() << std::endl;
    
    // Test 1: Move construction
    {
        std::cout << colors::blue() << "  [TEST] Move construction"
                  << colors::reset() << std::endl;
        
        int cleanup_count = 0;
        {
            auto guard1 = makeScopeGuard([&cleanup_count]() { ++cleanup_count; });
            SIMPLE_ASSERT(guard1.is_active(), "guard1 should be active");
            
            auto guard2 = std::move(guard1);
            SIMPLE_ASSERT(guard2.is_active(), "guard2 should be active");
            SIMPLE_ASSERT(!guard1.is_active(), "guard1 should be inactive after move");
        }
        ASSERT_EQ(cleanup_count, 1, "Cleanup should execute once from guard2");
    }
    
    // Test 2: Move assignment
    {
        std::cout << colors::blue() << "  [TEST] Move assignment"
                  << colors::reset() << std::endl;
        
        int cleanup1 = 0, cleanup2 = 0;
        {
            // Use std::function to ensure same type for move assignment
            ScopeGuard<std::function<void()>> guard1(
                std::function<void()>([&cleanup1]() { ++cleanup1; }));
            ScopeGuard<std::function<void()>> guard2(
                std::function<void()>([&cleanup2]() { ++cleanup2; }));
            
            // Move assign guard1 to guard2
            // guard2's original action should execute immediately
            guard2 = std::move(guard1);
            
            ASSERT_EQ(cleanup2, 1, "guard2's original action should execute");
            ASSERT_EQ(cleanup1, 0, "guard1's action not executed yet");
            SIMPLE_ASSERT(!guard1.is_active(), "guard1 should be inactive");
            SIMPLE_ASSERT(guard2.is_active(), "guard2 should be active");
        }
        ASSERT_EQ(cleanup1, 1, "guard1's action should execute from guard2");
    }
    
    // Test 3: Moving into function and back
    {
        std::cout << colors::blue() << "  [TEST] Moving into function and back"
                  << colors::reset() << std::endl;
        
        int cleanup_count = 0;
        
        auto create_guard = [&cleanup_count]() {
            return makeScopeGuard([&cleanup_count]() { ++cleanup_count; });
        };
        
        {
            auto guard = create_guard();
            SIMPLE_ASSERT(guard.is_active(), "Returned guard should be active");
        }
        ASSERT_EQ(cleanup_count, 1, "Cleanup should execute");
    }
    
    // Test 4: Storing in container (requires move)
    {
        std::cout << colors::blue() << "  [TEST] Storing in container"
                  << colors::reset() << std::endl;
        
        int cleanup_count = 0;
        
        // C++17 compatible: Use std::function to avoid lambda in decltype
        {
            std::vector<ScopeGuard<std::function<void()>>> guards;
            
            // Create guards with std::function wrappers
            guards.push_back(ScopeGuard<std::function<void()>>(
                std::function<void()>([&cleanup_count]() { ++cleanup_count; })));
            guards.push_back(ScopeGuard<std::function<void()>>(
                std::function<void()>([&cleanup_count]() { ++cleanup_count; })));
            
            ASSERT_EQ(cleanup_count, 0, "No cleanup yet");
        }
        ASSERT_EQ(cleanup_count, 2, "All guards should execute");
    }
    
    std::cout << colors::green() << "Move Semantics: Tests passed."
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// IV. Exception Policy Tests
// =============================================================================

bool test_NothrowPolicy() {
    std::cout << colors::cyan() << "\nTesting ScopeGuardNothrowPolicy..."
              << colors::reset() << std::endl;
    
    // Test 1: Noexcept lambda compiles
    {
        std::cout << colors::blue() << "  [TEST] Noexcept lambda compiles"
                  << colors::reset() << std::endl;
        
        int cleanup_count = 0;
        {
            auto guard = makeScopeGuard<ScopeGuardNothrowPolicy>(
                [&cleanup_count]() noexcept { ++cleanup_count; });
        }
        ASSERT_EQ(cleanup_count, 1, "Cleanup should execute");
    }
    
    // Note: We cannot test a throwing lambda with NothrowPolicy
    // because it would fail to compile (static_assert)
    
    std::cout << colors::green() << "ScopeGuardNothrowPolicy: Tests passed."
              << colors::reset() << std::endl;
    return true;
}

bool test_TerminatePolicy() {
    std::cout << colors::cyan() << "\nTesting ScopeGuardTerminatePolicy..."
              << colors::reset() << std::endl;
    
    // Test 1: Non-throwing action
    {
        std::cout << colors::blue() << "  [TEST] Non-throwing action"
                  << colors::reset() << std::endl;
        
        int cleanup_count = 0;
        {
            auto guard = makeScopeGuard<ScopeGuardTerminatePolicy>(
                [&cleanup_count]() { ++cleanup_count; });
        }
        ASSERT_EQ(cleanup_count, 1, "Cleanup should execute");
    }
    
    // Test 2: Dismissed guard doesn't execute
    {
        std::cout << colors::blue() << "  [TEST] Dismissed guard doesn't execute"
                  << colors::reset() << std::endl;
        
        int cleanup_count = 0;
        {
            auto guard = makeScopeGuard<ScopeGuardTerminatePolicy>(
                [&cleanup_count]() { ++cleanup_count; });
            guard.dismiss();
        }
        ASSERT_EQ(cleanup_count, 0, "Dismissed guard should not execute");
    }
    
    // Note: We cannot directly test the terminate behavior in a unit test
    // as it would terminate the test process. This would require a
    // separate test harness that spawns processes.
    
    std::cout << colors::blue() 
              << "  [INFO] Cannot test std::terminate() in unit test (would abort)"
              << colors::reset() << std::endl;
    
    std::cout << colors::green() << "ScopeGuardTerminatePolicy: Tests passed."
              << colors::reset() << std::endl;
    return true;
}

bool test_LogAndSwallowPolicy() {
    std::cout << colors::cyan() << "\nTesting ScopeGuardLogAndSwallowPolicy..."
              << colors::reset() << std::endl;
    
    // Test 1: Non-throwing action
    {
        std::cout << colors::blue() << "  [TEST] Non-throwing action"
                  << colors::reset() << std::endl;
        
        int cleanup_count = 0;
        {
            auto guard = makeScopeGuard<ScopeGuardLogAndSwallowPolicy>(
                [&cleanup_count]() { ++cleanup_count; });
        }
        ASSERT_EQ(cleanup_count, 1, "Cleanup should execute");
    }
    
    // Test 2: Throwing action (exception swallowed)
    {
        std::cout << colors::blue() << "  [TEST] Throwing action (exception swallowed)"
                  << colors::reset() << std::endl;
        
        bool exception_caught_outside = false;
        try {
            {
                auto guard = makeScopeGuard<ScopeGuardLogAndSwallowPolicy>([]() {
                    throw std::runtime_error("Test exception");
                });
                // Exception will be caught and logged in destructor
            }
            // We should reach here (exception was swallowed)
        } catch (...) {
            exception_caught_outside = true;
        }
        
        SIMPLE_ASSERT(!exception_caught_outside, 
            "Exception should be swallowed, not propagated");
    }
    
    // Test 3: std::exception derived
    {
        std::cout << colors::blue() << "  [TEST] std::exception derived"
                  << colors::reset() << std::endl;
        
        try {
            {
                auto guard = makeScopeGuard<ScopeGuardLogAndSwallowPolicy>([]() {
                    throw std::logic_error("Logic error in cleanup");
                });
            }
            SIMPLE_ASSERT(true, "Should reach here (exception swallowed)");
        } catch (...) {
            SIMPLE_ASSERT(false, "Exception should not propagate");
        }
    }
    
    std::cout << colors::green() << "ScopeGuardLogAndSwallowPolicy: Tests passed."
              << colors::reset() << std::endl;
    return true;
}

bool test_RethrowPolicy() {
    std::cout << colors::cyan() << "\nTesting ScopeGuardRethrowPolicy..."
              << colors::reset() << std::endl;
    
    // Test 1: Non-throwing action
    {
        std::cout << colors::blue() << "  [TEST] Non-throwing action"
                  << colors::reset() << std::endl;
        
        int cleanup_count = 0;
        {
            auto guard = makeScopeGuard<ScopeGuardRethrowPolicy>(
                [&cleanup_count]() { ++cleanup_count; });
        }
        ASSERT_EQ(cleanup_count, 1, "Cleanup should execute");
    }
    
    // Test 2: Throwing action (exception propagates)
    {
        std::cout << colors::blue() << "  [TEST] Throwing action (exception propagates)"
                  << colors::reset() << std::endl;
        
        bool exception_caught = false;
        try {
            {
                auto guard = makeScopeGuard<ScopeGuardRethrowPolicy>([]() {
                    throw std::runtime_error("Test exception");
                });
            }
        } catch (const std::runtime_error& e) {
            exception_caught = true;
            SIMPLE_ASSERT(std::string(e.what()) == "Test exception", 
                "Should catch the correct exception");
        }
        
        SIMPLE_ASSERT(exception_caught, "Exception should propagate");
    }
    
    // Test 3: Different exception types
    {
        std::cout << colors::blue() << "  [TEST] Different exception types"
                  << colors::reset() << std::endl;
        
        bool caught_logic_error = false;
        try {
            {
                auto guard = makeScopeGuard<ScopeGuardRethrowPolicy>([]() {
                    throw std::logic_error("Logic error");
                });
            }
        } catch (const std::logic_error&) {
            caught_logic_error = true;
        }
        SIMPLE_ASSERT(caught_logic_error, "Should catch logic_error");
    }
    
    std::cout << colors::yellow() 
              << "  [WARNING] RethrowPolicy makes destructors throwing - use with caution!"
              << colors::reset() << std::endl;
    
    std::cout << colors::green() << "ScopeGuardRethrowPolicy: Tests passed."
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// V. Macro Convenience Tests
// =============================================================================

bool test_MacroConvenience() {
    std::cout << colors::cyan() << "\nTesting Macro Convenience..."
              << colors::reset() << std::endl;
    
    // Test 1: SCOPE_GUARD macro
    {
        std::cout << colors::blue() << "  [TEST] SCOPE_GUARD macro"
                  << colors::reset() << std::endl;
        
        int cleanup_count = 0;
        {
            SCOPE_GUARD { ++cleanup_count; };
        }
        ASSERT_EQ(cleanup_count, 1, "SCOPE_GUARD should execute");
    }
    
    // Test 2: Multiple SCOPE_GUARD on same line (unique naming)
    {
        std::cout << colors::blue() << "  [TEST] Multiple SCOPE_GUARD macros"
                  << colors::reset() << std::endl;
        
        int count1 = 0, count2 = 0;
        {
            SCOPE_GUARD { ++count1; };
            SCOPE_GUARD { ++count2; };
        }
        ASSERT_EQ(count1, 1, "First guard should execute");
        ASSERT_EQ(count2, 1, "Second guard should execute");
    }
    
    // Test 3: SCOPE_GUARD_EX with policy
    {
        std::cout << colors::blue() << "  [TEST] SCOPE_GUARD_EX macro"
                  << colors::reset() << std::endl;
        
        int cleanup_count = 0;
        {
            // Note: Don't add 'noexcept' - the macro already adds it for NothrowPolicy
            SCOPE_GUARD_EX(ScopeGuardNothrowPolicy) { 
                ++cleanup_count; 
            };
        }
        ASSERT_EQ(cleanup_count, 1, "SCOPE_GUARD_EX should execute");
    }
    
    // Test 4: Capturing local variables
    {
        std::cout << colors::blue() << "  [TEST] Capturing local variables"
                  << colors::reset() << std::endl;
        
        int value = 10;
        {
            SCOPE_GUARD { value *= 2; };
        }
        ASSERT_EQ(value, 20, "Should capture and modify local variable");
    }
    
    std::cout << colors::green() << "Macro Convenience: Tests passed."
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// VI. Thread-Safety Tests
// =============================================================================

bool test_ComplexResourceManagement() {
    std::cout << colors::cyan() << "\nTesting Complex Resource Management..."
              << colors::reset() << std::endl;
    
    // Test 1: File handle pattern
    {
        std::cout << colors::blue() << "  [TEST] File handle pattern"
                  << colors::reset() << std::endl;
        
        bool file_closed = false;
        
        // Simulate file operations
        auto simulate_file_operations = [&file_closed]() {
            // FILE* file = fopen("test.txt", "w");
            void* file = (void*)0x1234; // Simulated file handle
            
            auto guard = makeScopeGuard([&file_closed, file]() {
                if (file) {
                    // fclose(file);
                    file_closed = true;
                }
            });
            
            // Do operations...
            // If exception thrown, file will be closed automatically
        };
        
        simulate_file_operations();
        SIMPLE_ASSERT(file_closed, "File should be closed");
    }
    
    // Test 2: Transaction rollback pattern
    {
        std::cout << colors::blue() << "  [TEST] Transaction rollback pattern"
                  << colors::reset() << std::endl;
        
        bool rolled_back = false;
        bool committed = false;
        
        auto simulate_transaction = [&]() {
            // Begin transaction
            
            auto rollback_guard = makeScopeGuard([&rolled_back]() {
                // Rollback on failure
                rolled_back = true;
            });
            
            // Do transactional work...
            bool success = true;
            
            if (success) {
                committed = true;
                rollback_guard.dismiss(); // Don't rollback on success
            }
        };
        
        simulate_transaction();
        SIMPLE_ASSERT(committed, "Transaction should commit");
        SIMPLE_ASSERT(!rolled_back, "Should not rollback on success");
    }
    
    // Test 3: Multiple resource cleanup
    {
        std::cout << colors::blue() << "  [TEST] Multiple resource cleanup"
                  << colors::reset() << std::endl;
        
        int cleanup1 = 0, cleanup2 = 0, cleanup3 = 0;
        
        {
            auto* resource1 = new int(1);
            auto guard1 = makeScopeGuard([&cleanup1, resource1]() {
                ++cleanup1;
                delete resource1;
            });
            
            auto* resource2 = new int(2);
            auto guard2 = makeScopeGuard([&cleanup2, resource2]() {
                ++cleanup2;
                delete resource2;
            });
            
            auto* resource3 = new int(3);
            auto guard3 = makeScopeGuard([&cleanup3, resource3]() {
                ++cleanup3;
                delete resource3;
            });
            
            // All resources will be cleaned up in reverse order
        }
        
        ASSERT_EQ(cleanup1, 1, "Resource1 should be cleaned");
        ASSERT_EQ(cleanup2, 1, "Resource2 should be cleaned");
        ASSERT_EQ(cleanup3, 1, "Resource3 should be cleaned");
    }
    
    std::cout << colors::green() << "Complex Resource Management: Tests passed."
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// VIII. Performance Benchmarks
// =============================================================================

void run_scope_guard_benchmarks() {
    std::cout << "\n" << colors::bold() << colors::cyan()
              << "=== Performance Benchmarks ==="
              << colors::reset() << std::endl;
    
    const size_t ITERATIONS = 1000000;
    
    // Benchmark 1: ScopeGuard overhead
    {
        int counter = 0;
        benchmark("ScopeGuard creation and execution", [&]() {
            auto guard = makeScopeGuard([&counter]() { ++counter; });
        }, ITERATIONS);
    }
    
    // Benchmark 2: Dismissed guard overhead
    {
        int counter = 0;
        benchmark("ScopeGuard with dismiss", [&]() {
            auto guard = makeScopeGuard([&counter]() { ++counter; });
            guard.dismiss();
        }, ITERATIONS);
    }
    
    // Benchmark 3: Manual cleanup (baseline)
    {
        int counter = 0;
        benchmark("Manual cleanup (baseline)", [&]() {
            ++counter;
        }, ITERATIONS);
    }
    
    // Benchmark 4: Different policies
    {
        int counter = 0;
        benchmark("ScopeGuardTerminatePolicy", [&]() {
            auto guard = makeScopeGuard<ScopeGuardTerminatePolicy>(
                [&counter]() { ++counter; });
        }, ITERATIONS / 10);
    }
    
    {
        int counter = 0;
        benchmark("ScopeGuardLogAndSwallowPolicy", [&]() {
            auto guard = makeScopeGuard<ScopeGuardLogAndSwallowPolicy>(
                [&counter]() { ++counter; });
        }, ITERATIONS / 10);
    }
    
    std::cout << "\n" << colors::blue()
              << "[NOTE] Benchmarks show RAII overhead vs manual cleanup"
              << colors::reset() << std::endl;
}

// =============================================================================
// Main Test Function
// =============================================================================


bool test_ScopeGuard() {

    PRINT_HEADER(SCOPE GUARD)

    TestRunner runner;
    
    runner.run_test("BasicScopeGuard", test_BasicScopeGuard);
    runner.run_test("DismissFunctionality", test_DismissFunctionality);
    runner.run_test("MoveSemantics", test_MoveSemantics);
    
    runner.run_test("NothrowPolicy", test_NothrowPolicy);
    runner.run_test("TerminatePolicy", test_TerminatePolicy);
    runner.run_test("LogAndSwallowPolicy", test_LogAndSwallowPolicy);
    runner.run_test("RethrowPolicy", test_RethrowPolicy);
    
    runner.run_test("MacroConvenience", test_MacroConvenience);
    runner.run_test("ComplexResourceManagement", test_ComplexResourceManagement);
    
    int failed = runner.print_summary();
    
    if (failed == 0) {
        run_scope_guard_benchmarks();
    }
    
    return failed == 0;
}

}
