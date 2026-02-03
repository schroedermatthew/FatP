/**
 * @file test_ScopeGuard.cpp
 * @brief Comprehensive unit tests for ScopeGuard.h and ScopeGuardPolicies.h
 *
 * @details Complete test suite for:
 * - Basic RAII functionality
 * - All 4 exception policies (Nothrow, Terminate, LogAndSwallow, Rethrow)
 * - Move semantics (construction and assignment)
 * - Dismiss functionality (dismiss, dismiss_if)
 * - Macro convenience helpers (FATP_SCOPE_GUARD, FATP_SCOPE_GUARD_EX)
 * - Resource management patterns
 * - Performance characteristics
 *
 * Note: ScopeGuard is designed for single-threaded use only.
 */
/*
FATP_META:
  meta_version: 1
  component: ScopeGuard
  file_role: test
  path: components/ScopeGuard/tests/test_ScopeGuard.cpp
  layer: Testing
  namespace: fat_p
  summary: "Unit tests for ScopeGuard."
  api_stability: in_work
  related:
    docs_search: "ScopeGuard"
    headers:
      - include/fat_p/ScopeGuard.h
      - include/fat_p/ScopeGuardPolicies.h
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

#include <cassert>
#include <chrono>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "FatPConcepts.h"
#include "FatPTest.h"
#include "ScopeGuard.h"
#include "ScopeGuardPolicies.h"

using namespace fat_p::testing;
using namespace fat_p;

namespace fat_p::testing::scopeguard
{

class TestResource
{
public:
    static inline int construction_count{0};
    static inline int destruction_count{0};
    static inline int cleanup_count{0};

    int id;
    bool cleaned = false;

    explicit TestResource(int i = 0)
        : id(i)
    {
        ++construction_count;
    }

    ~TestResource()
    {
        ++destruction_count;
    }

    void cleanup()
    {
        cleaned = true;
        ++cleanup_count;
    }

    static void reset_counters()
    {
        construction_count = 0;
        destruction_count = 0;
        cleanup_count = 0;
    }
};

class ThrowingAction
{
public:
    bool should_throw;
    bool was_called = false;

    explicit ThrowingAction(bool throw_on_call = false)
        : should_throw(throw_on_call)
    {
    }

    void operator()()
    {
        was_called = true;
        if (should_throw)
        {
            throw std::runtime_error("Intentional test exception");
        }
    }
};

FATP_TEST_CASE(basic)
{
    std::cout << colors::cyan() << "\nTesting Basic ScopeGuard Functionality..." << colors::reset() << std::endl;

    // Test 1: Basic RAII cleanup
    {
        std::cout << colors::blue() << "  [TEST] Basic RAII cleanup" << colors::reset() << std::endl;

        int cleanup_count = 0;
        {
            auto guard = makeScopeGuard([&cleanup_count]() {
                ++cleanup_count;
            });
            FATP_ASSERT_EQ(cleanup_count, 0, "Cleanup should not run yet");
        }
        FATP_ASSERT_EQ(cleanup_count, 1, "Cleanup should have run once");
    }

    // Test 2: Resource cleanup
    {
        std::cout << colors::blue() << "  [TEST] Resource cleanup" << colors::reset() << std::endl;

        TestResource::reset_counters();
        {
            auto* resource = new TestResource(42);
            auto guard = makeScopeGuard([resource]() {
                resource->cleanup();
                delete resource;
            });
        }
        FATP_ASSERT_EQ(TestResource::construction_count, 1, "Should construct once");
        FATP_ASSERT_EQ(TestResource::destruction_count, 1, "Should destruct once");
        FATP_ASSERT_EQ(TestResource::cleanup_count, 1, "Should cleanup once");
    }

    // Test 3: Multiple guards in scope
    {
        std::cout << colors::blue() << "  [TEST] Multiple guards in scope" << colors::reset() << std::endl;

        int count1 = 0, count2 = 0, count3 = 0;
        {
            auto guard1 = makeScopeGuard([&count1]() {
                ++count1;
            });
            auto guard2 = makeScopeGuard([&count2]() {
                ++count2;
            });
            auto guard3 = makeScopeGuard([&count3]() {
                ++count3;
            });
        }
        // Guards execute in reverse order of construction (stack unwinding)
        FATP_ASSERT_EQ(count1, 1, "Guard1 should execute");
        FATP_ASSERT_EQ(count2, 1, "Guard2 should execute");
        FATP_ASSERT_EQ(count3, 1, "Guard3 should execute");
    }

    // Test 4: Nested scopes
    {
        std::cout << colors::blue() << "  [TEST] Nested scopes" << colors::reset() << std::endl;

        int outer = 0, inner = 0;
        {
            auto outer_guard = makeScopeGuard([&outer]() {
                ++outer;
            });
            FATP_ASSERT_EQ(outer, 0, "Outer guard not executed yet");
            {
                auto inner_guard = makeScopeGuard([&inner]() {
                    ++inner;
                });
                FATP_ASSERT_EQ(inner, 0, "Inner guard not executed yet");
            }
            FATP_ASSERT_EQ(inner, 1, "Inner guard should have executed");
            FATP_ASSERT_EQ(outer, 0, "Outer guard still not executed");
        }
        FATP_ASSERT_EQ(outer, 1, "Outer guard should have executed");
    }

    std::cout << colors::green() << "Basic ScopeGuard: Tests passed." << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// II. Dismiss Functionality Tests
// =============================================================================

FATP_TEST_CASE(dismiss)
{
    std::cout << colors::cyan() << "\nTesting Dismiss Functionality..." << colors::reset() << std::endl;

    // Test 1: Basic dismiss
    {
        std::cout << colors::blue() << "  [TEST] Basic dismiss" << colors::reset() << std::endl;

        int cleanup_count = 0;
        {
            auto guard = makeScopeGuard([&cleanup_count]() {
                ++cleanup_count;
            });
            FATP_ASSERT_TRUE(guard.is_active(), "Guard should be active");
            guard.dismiss();
            FATP_ASSERT_FALSE(guard.is_active(), "Guard should be dismissed");
        }
        FATP_ASSERT_EQ(cleanup_count, 0, "Dismissed guard should not execute");
    }

    // Test 2: Conditional dismiss (true)
    {
        std::cout << colors::blue() << "  [TEST] Conditional dismiss (true)" << colors::reset() << std::endl;

        int cleanup_count = 0;
        bool success = true;
        {
            auto guard = makeScopeGuard([&cleanup_count]() {
                ++cleanup_count;
            });
            guard.dismiss_if(success);
        }
        FATP_ASSERT_EQ(cleanup_count, 0, "Should not execute when dismissed");
    }

    // Test 3: Conditional dismiss (false)
    {
        std::cout << colors::blue() << "  [TEST] Conditional dismiss (false)" << colors::reset() << std::endl;

        int cleanup_count = 0;
        bool success = false;
        {
            auto guard = makeScopeGuard([&cleanup_count]() {
                ++cleanup_count;
            });
            guard.dismiss_if(success);
        }
        FATP_ASSERT_EQ(cleanup_count, 1, "Should execute when not dismissed");
    }

    // Test 4: Dismiss pattern for successful operations
    {
        std::cout << colors::blue() << "  [TEST] Dismiss pattern for successful operations" << colors::reset()
                  << std::endl;

        TestResource::reset_counters();
        auto* resource = new TestResource(42);
        bool operation_succeeded = false;

        {
            auto guard = makeScopeGuard([resource]() {
                resource->cleanup();
                delete resource;
            });

            // Simulate an operation that might fail
            try
            {
                // Operation succeeds
                operation_succeeded = true;
                guard.dismiss(); // Don't cleanup on success
            }
            catch (...)
            {
                // If operation fails, guard will clean up
            }
        }

        FATP_ASSERT_TRUE(operation_succeeded, "Operation should succeed");
        FATP_ASSERT_EQ(TestResource::cleanup_count, 0, "Should not cleanup on success");

        // Manual cleanup
        delete resource;
    }

    std::cout << colors::green() << "Dismiss Functionality: Tests passed." << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// III. Move Semantics Tests
// =============================================================================

FATP_TEST_CASE(move_semantics)
{
    std::cout << colors::cyan() << "\nTesting Move Semantics..." << colors::reset() << std::endl;

    // Test 1: Move construction
    {
        std::cout << colors::blue() << "  [TEST] Move construction" << colors::reset() << std::endl;

        int cleanup_count = 0;
        {
            auto guard1 = makeScopeGuard([&cleanup_count]() {
                ++cleanup_count;
            });
            FATP_ASSERT_TRUE(guard1.is_active(), "guard1 should be active");

            auto guard2 = std::move(guard1);
            FATP_ASSERT_TRUE(guard2.is_active(), "guard2 should be active");
            FATP_ASSERT_FALSE(guard1.is_active(), "guard1 should be inactive after move");
        }
        FATP_ASSERT_EQ(cleanup_count, 1, "Cleanup should execute once from guard2");
    }

    // Test 2: Move assignment
    {
        std::cout << colors::blue() << "  [TEST] Move assignment" << colors::reset() << std::endl;

        int cleanup1 = 0, cleanup2 = 0;
        {
            // Use std::function to ensure same type for move assignment
            ScopeGuard<std::function<void()>> guard1(std::function<void()>([&cleanup1]() {
                ++cleanup1;
            }));
            ScopeGuard<std::function<void()>> guard2(std::function<void()>([&cleanup2]() {
                ++cleanup2;
            }));

            // Move assign guard1 to guard2
            // guard2's original action should execute immediately
            guard2 = std::move(guard1);

            FATP_ASSERT_EQ(cleanup2, 1, "guard2's original action should execute");
            FATP_ASSERT_EQ(cleanup1, 0, "guard1's action not executed yet");
            FATP_ASSERT_FALSE(guard1.is_active(), "guard1 should be inactive");
            FATP_ASSERT_TRUE(guard2.is_active(), "guard2 should be active");
        }
        FATP_ASSERT_EQ(cleanup1, 1, "guard1's action should execute from guard2");
    }

    // Test 3: Moving into function and back
    {
        std::cout << colors::blue() << "  [TEST] Moving into function and back" << colors::reset() << std::endl;

        int cleanup_count = 0;

        auto create_guard = [&cleanup_count]() {
            return makeScopeGuard([&cleanup_count]() {
                ++cleanup_count;
            });
        };

        {
            auto guard = create_guard();
            FATP_ASSERT_TRUE(guard.is_active(), "Returned guard should be active");
        }
        FATP_ASSERT_EQ(cleanup_count, 1, "Cleanup should execute");
    }

    // Test 4: Storing in container (requires move)
    {
        std::cout << colors::blue() << "  [TEST] Storing in container" << colors::reset() << std::endl;

        int cleanup_count = 0;

        // C++17 compatible: Use std::function to avoid lambda in decltype
        {
            std::vector<ScopeGuard<std::function<void()>>> guards;

            // Create guards with std::function wrappers
            guards.push_back(ScopeGuard<std::function<void()>>(std::function<void()>([&cleanup_count]() {
                ++cleanup_count;
            })));
            guards.push_back(ScopeGuard<std::function<void()>>(std::function<void()>([&cleanup_count]() {
                ++cleanup_count;
            })));

            FATP_ASSERT_EQ(cleanup_count, 0, "No cleanup yet");
        }
        FATP_ASSERT_EQ(cleanup_count, 2, "All guards should execute");
    }

    std::cout << colors::green() << "Move Semantics: Tests passed." << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// IV. Exception Policy Tests
// =============================================================================

FATP_TEST_CASE(nothrow_policy)
{
    std::cout << colors::cyan() << "\nTesting ScopeGuardNothrowPolicy..." << colors::reset() << std::endl;

    // Test 1: Noexcept lambda compiles
    {
        std::cout << colors::blue() << "  [TEST] Noexcept lambda compiles" << colors::reset() << std::endl;

        int cleanup_count = 0;
        {
            auto guard = makeScopeGuard<ScopeGuardNothrowPolicy>([&cleanup_count]() noexcept {
                ++cleanup_count;
            });
        }
        FATP_ASSERT_EQ(cleanup_count, 1, "Cleanup should execute");
    }

    // Note: We cannot test a throwing lambda with NothrowPolicy
    // because it would fail to compile (static_assert)

    std::cout << colors::green() << "ScopeGuardNothrowPolicy: Tests passed." << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(terminate_policy)
{
    std::cout << colors::cyan() << "\nTesting ScopeGuardTerminatePolicy..." << colors::reset() << std::endl;

    // Test 1: Non-throwing action
    {
        std::cout << colors::blue() << "  [TEST] Non-throwing action" << colors::reset() << std::endl;

        int cleanup_count = 0;
        {
            auto guard = makeScopeGuard<ScopeGuardTerminatePolicy>([&cleanup_count]() {
                ++cleanup_count;
            });
        }
        FATP_ASSERT_EQ(cleanup_count, 1, "Cleanup should execute");
    }

    // Test 2: Dismissed guard doesn't execute
    {
        std::cout << colors::blue() << "  [TEST] Dismissed guard doesn't execute" << colors::reset() << std::endl;

        int cleanup_count = 0;
        {
            auto guard = makeScopeGuard<ScopeGuardTerminatePolicy>([&cleanup_count]() {
                ++cleanup_count;
            });
            guard.dismiss();
        }
        FATP_ASSERT_EQ(cleanup_count, 0, "Dismissed guard should not execute");
    }

    // Note: We cannot directly test the terminate behavior in a unit test
    // as it would terminate the test process. This would require a
    // separate test harness that spawns processes.

    std::cout << colors::blue() << "  [INFO] Cannot test std::terminate() in unit test (would abort)" << colors::reset()
              << std::endl;

    std::cout << colors::green() << "ScopeGuardTerminatePolicy: Tests passed." << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(log_and_swallow_policy)
{
    std::cout << colors::cyan() << "\nTesting ScopeGuardLogAndSwallowPolicy..." << colors::reset() << std::endl;

    // Test 1: Non-throwing action
    {
        std::cout << colors::blue() << "  [TEST] Non-throwing action" << colors::reset() << std::endl;

        int cleanup_count = 0;
        {
            auto guard = makeScopeGuard<ScopeGuardLogAndSwallowPolicy>([&cleanup_count]() {
                ++cleanup_count;
            });
        }
        FATP_ASSERT_EQ(cleanup_count, 1, "Cleanup should execute");
    }

    // Test 2: Throwing action (exception swallowed)
    {
        std::cout << colors::blue() << "  [TEST] Throwing action (exception swallowed)" << colors::reset() << std::endl;

        bool exception_caught_outside = false;
        try
        {
            {
                auto guard = makeScopeGuard<ScopeGuardLogAndSwallowPolicy>([]() {
                    throw std::runtime_error("Test exception");
                });
                // Exception will be caught and logged in destructor
            }
            // We should reach here (exception was swallowed)
        }
        catch (...)
        {
            exception_caught_outside = true;
        }

        FATP_ASSERT_FALSE(exception_caught_outside, "Exception should be swallowed, not propagated");
    }

    // Test 3: std::exception derived
    {
        std::cout << colors::blue() << "  [TEST] std::exception derived" << colors::reset() << std::endl;

        try
        {
            {
                auto guard = makeScopeGuard<ScopeGuardLogAndSwallowPolicy>([]() {
                    throw std::logic_error("Logic error in cleanup");
                });
            }
            FATP_ASSERT_TRUE(true, "Should reach here (exception swallowed)");
        }
        catch (...)
        {
            FATP_ASSERT_TRUE(false, "Exception should not propagate");
        }
    }

    // Test 4: Verify logging output goes to stderr
    {
        std::cout << colors::blue() << "  [TEST] Logging to stderr" << colors::reset() << std::endl;

        // Redirect stderr to a stringstream
        std::stringstream captured;
        std::streambuf* original_stderr = std::cerr.rdbuf(captured.rdbuf());

        {
            auto guard = makeScopeGuard<ScopeGuardLogAndSwallowPolicy>([]() {
                throw std::runtime_error("Captured error message");
            });
        }

        // Restore stderr
        std::cerr.rdbuf(original_stderr);

        std::string output = captured.str();
        FATP_ASSERT_NE(output.find("Captured error message"),
                       std::string::npos,
                       "Error message should be logged to stderr");
        FATP_ASSERT_TRUE(output.find("ScopeGuard") != std::string::npos ||
                             output.find("swallowed") != std::string::npos,
                         "Log should indicate source and that exception was handled");
    }

    // Note: To test FATP_SCOPE_GUARD_LOG_ERRORS=0, compile a separate test file with:
    //   #define FATP_SCOPE_GUARD_LOG_ERRORS 0
    //   #include "ScopeGuardPolicies.h"
    // Then verify captured.str() is empty after triggering an exception.
    // This cannot be tested in the same translation unit due to macro evaluation order.

    std::cout << colors::green() << "ScopeGuardLogAndSwallowPolicy: Tests passed." << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(rethrow_policy)
{
    std::cout << colors::cyan() << "\nTesting ScopeGuardRethrowPolicy..." << colors::reset() << std::endl;

    // Test 1: Non-throwing action
    {
        std::cout << colors::blue() << "  [TEST] Non-throwing action" << colors::reset() << std::endl;

        int cleanup_count = 0;
        {
            auto guard = makeScopeGuard<ScopeGuardRethrowPolicy>([&cleanup_count]() {
                ++cleanup_count;
            });
        }
        FATP_ASSERT_EQ(cleanup_count, 1, "Cleanup should execute");
    }

    // Test 2: Throwing action (exception propagates)
    {
        std::cout << colors::blue() << "  [TEST] Throwing action (exception propagates)" << colors::reset()
                  << std::endl;

        bool exception_caught = false;
        try
        {
            {
                auto guard = makeScopeGuard<ScopeGuardRethrowPolicy>([]() {
                    throw std::runtime_error("Test exception");
                });
            }
        }
        catch (const std::runtime_error& e)
        {
            exception_caught = true;
            FATP_ASSERT_EQ(std::string(e.what()), "Test exception", "Should catch the correct exception");
        }

        FATP_ASSERT_TRUE(exception_caught, "Exception should propagate");
    }

    // Test 3: Different exception types
    {
        std::cout << colors::blue() << "  [TEST] Different exception types" << colors::reset() << std::endl;

        bool caught_logic_error = false;
        try
        {
            {
                auto guard = makeScopeGuard<ScopeGuardRethrowPolicy>([]() {
                    throw std::logic_error("Logic error");
                });
            }
        }
        catch (const std::logic_error&)
        {
            caught_logic_error = true;
        }
        FATP_ASSERT_TRUE(caught_logic_error, "Should catch logic_error");
    }

    std::cout << colors::yellow() << "  [WARNING] RethrowPolicy makes destructors throwing - use with caution!"
              << colors::reset() << std::endl;

    std::cout << colors::green() << "ScopeGuardRethrowPolicy: Tests passed." << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// V. Macro Convenience Tests
// =============================================================================

FATP_TEST_CASE(macro_convenience)
{
    std::cout << colors::cyan() << "\nTesting Macro Convenience..." << colors::reset() << std::endl;

    // Test 1: FATP_SCOPE_GUARD macro
    {
        std::cout << colors::blue() << "  [TEST] FATP_SCOPE_GUARD macro" << colors::reset() << std::endl;

        int cleanup_count = 0;
        {
            FATP_SCOPE_GUARD
            {
                ++cleanup_count;
            };
        }
        FATP_ASSERT_EQ(cleanup_count, 1, "FATP_SCOPE_GUARD should execute");
    }

    // Test 2: Multiple FATP_SCOPE_GUARD on same line (unique naming)
    {
        std::cout << colors::blue() << "  [TEST] Multiple FATP_SCOPE_GUARD macros" << colors::reset() << std::endl;

        int count1 = 0, count2 = 0;
        {
            FATP_SCOPE_GUARD
            {
                ++count1;
            };
            FATP_SCOPE_GUARD
            {
                ++count2;
            };
        }
        FATP_ASSERT_EQ(count1, 1, "First guard should execute");
        FATP_ASSERT_EQ(count2, 1, "Second guard should execute");
    }

    // Test 3: FATP_SCOPE_GUARD_EX with policy
    {
        std::cout << colors::blue() << "  [TEST] FATP_SCOPE_GUARD_EX macro" << colors::reset() << std::endl;

        int cleanup_count = 0;
        {
            // Note: Don't add 'noexcept' - the macro already adds it for NothrowPolicy
            FATP_SCOPE_GUARD_EX(ScopeGuardNothrowPolicy)
            {
                ++cleanup_count;
            };
        }
        FATP_ASSERT_EQ(cleanup_count, 1, "FATP_SCOPE_GUARD_EX should execute");
    }

    // Test 4: Capturing local variables
    {
        std::cout << colors::blue() << "  [TEST] Capturing local variables" << colors::reset() << std::endl;

        int value = 10;
        {
            FATP_SCOPE_GUARD
            {
                value *= 2;
            };
        }
        FATP_ASSERT_EQ(value, 20, "Should capture and modify local variable");
    }

    std::cout << colors::green() << "Macro Convenience: Tests passed." << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// VI. Thread-Safety Tests
// =============================================================================

FATP_TEST_CASE(complex_resource_management)
{
    std::cout << colors::cyan() << "\nTesting Complex Resource Management..." << colors::reset() << std::endl;

    // Test 1: File handle pattern
    {
        std::cout << colors::blue() << "  [TEST] File handle pattern" << colors::reset() << std::endl;

        bool file_closed = false;

        // Simulate file operations
        auto simulate_file_operations = [&file_closed]() {
            // FILE* file = fopen("test.txt", "w");
            void* file = (void*)0x1234; // Simulated file handle

            auto guard = makeScopeGuard([&file_closed, file]() {
                if (file)
                {
                    // fclose(file);
                    file_closed = true;
                }
            });

            // Do operations...
            // If exception thrown, file will be closed automatically
        };

        simulate_file_operations();
        FATP_ASSERT_TRUE(file_closed, "File should be closed");
    }

    // Test 2: Transaction rollback pattern
    {
        std::cout << colors::blue() << "  [TEST] Transaction rollback pattern" << colors::reset() << std::endl;

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

            if (success)
            {
                committed = true;
                rollback_guard.dismiss(); // Don't rollback on success
            }
        };

        simulate_transaction();
        FATP_ASSERT_TRUE(committed, "Transaction should commit");
        FATP_ASSERT_FALSE(rolled_back, "Should not rollback on success");
    }

    // Test 3: Multiple resource cleanup
    {
        std::cout << colors::blue() << "  [TEST] Multiple resource cleanup" << colors::reset() << std::endl;

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

        FATP_ASSERT_EQ(cleanup1, 1, "Resource1 should be cleaned");
        FATP_ASSERT_EQ(cleanup2, 1, "Resource2 should be cleaned");
        FATP_ASSERT_EQ(cleanup3, 1, "Resource3 should be cleaned");
    }

    std::cout << colors::green() << "Complex Resource Management: Tests passed." << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// VIII. Performance Benchmarks
// =============================================================================
// =============================================================================
// Type Trait Tests
// =============================================================================

FATP_TEST_CASE(type_traits)
{
    std::cout << colors::cyan() << "\nTesting Type Traits..." << colors::reset() << std::endl;

    // Test 1: ScopeGuard detection
    {
        std::cout << colors::blue() << "  [TEST] is_scope_guard detection" << colors::reset() << std::endl;

        using BasicGuard = ScopeGuard<std::function<void()>>;
        using PolicyGuard = ScopeGuard<std::function<void()>, ScopeGuardNothrowPolicy>;

        static_assert(concepts::scope_guard_type<BasicGuard>, "ScopeGuard should be detected");
        static_assert(concepts::scope_guard_type<PolicyGuard>, "ScopeGuard with policy should be detected");
        static_assert(!concepts::scope_guard_type<int>, "int should not be detected as ScopeGuard");
        static_assert(!concepts::scope_guard_type<std::function<void()>>,
                      "std::function should not be detected as ScopeGuard");

        FATP_ASSERT_TRUE(concepts::scope_guard_type<BasicGuard>, "Runtime check: BasicGuard");
        FATP_ASSERT_FALSE(concepts::scope_guard_type<double>, "Runtime check: double");
    }

    // Test 2: ScopeGuardOnFail detection
    {
        std::cout << colors::blue() << "  [TEST] is_scope_guard detection for OnFail" << colors::reset() << std::endl;

        using FailGuard = ScopeGuardOnFail<std::function<void()>>;

        static_assert(concepts::scope_guard_type<FailGuard>, "ScopeGuardOnFail should be detected as scope guard");

        FATP_ASSERT_TRUE(concepts::scope_guard_type<FailGuard>, "Runtime check: FailGuard");
    }

    // Test 3: ScopeGuardOnSuccess detection
    {
        std::cout << colors::blue() << "  [TEST] is_scope_guard detection for OnSuccess" << colors::reset()
                  << std::endl;

        using SuccessGuard = ScopeGuardOnSuccess<std::function<void()>>;

        static_assert(concepts::scope_guard_type<SuccessGuard>,
                      "ScopeGuardOnSuccess should be detected as scope guard");

        FATP_ASSERT_TRUE(concepts::scope_guard_type<SuccessGuard>, "Runtime check: SuccessGuard");
    }

    std::cout << colors::green() << "Type Traits: Tests passed." << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// FATP_SCOPE_FAIL / FATP_SCOPE_SUCCESS Tests
// =============================================================================

FATP_TEST_CASE(scope_fail)
{
    std::cout << colors::cyan() << "\nTesting FATP_SCOPE_FAIL..." << colors::reset() << std::endl;

    // Test 1: FATP_SCOPE_FAIL executes on exception
    {
        std::cout << colors::blue() << "  [TEST] FATP_SCOPE_FAIL executes on exception" << colors::reset() << std::endl;

        int rollback_count = 0;

        try
        {
            FATP_SCOPE_FAIL
            {
                ++rollback_count;
            };
            throw std::runtime_error("test exception");
        }
        catch (...)
        {
            // Expected
        }

        FATP_ASSERT_EQ(rollback_count, 1, "FATP_SCOPE_FAIL should execute on exception");
    }

    // Test 2: FATP_SCOPE_FAIL does NOT execute on normal exit
    {
        std::cout << colors::blue() << "  [TEST] FATP_SCOPE_FAIL skipped on normal exit" << colors::reset()
                  << std::endl;

        int rollback_count = 0;

        {
            FATP_SCOPE_FAIL
            {
                ++rollback_count;
            };
            // Normal exit - no exception
        }

        FATP_ASSERT_EQ(rollback_count, 0, "FATP_SCOPE_FAIL should not execute on normal exit");
    }

    // Test 3: Multiple FATP_SCOPE_FAIL in same scope
    {
        std::cout << colors::blue() << "  [TEST] Multiple FATP_SCOPE_FAIL guards" << colors::reset() << std::endl;

        int count1 = 0, count2 = 0;

        try
        {
            FATP_SCOPE_FAIL
            {
                ++count1;
            };
            FATP_SCOPE_FAIL
            {
                ++count2;
            };
            throw std::runtime_error("test");
        }
        catch (...)
        {
        }

        FATP_ASSERT_EQ(count1, 1, "First FATP_SCOPE_FAIL should execute");
        FATP_ASSERT_EQ(count2, 1, "Second FATP_SCOPE_FAIL should execute");
    }

    // Test 4: FATP_SCOPE_FAIL can be dismissed
    {
        std::cout << colors::blue() << "  [TEST] FATP_SCOPE_FAIL dismiss" << colors::reset() << std::endl;

        int rollback_count = 0;

        try
        {
            auto guard = makeScopeGuardOnFail([&] {
                ++rollback_count;
            });
            guard.dismiss();
            throw std::runtime_error("test");
        }
        catch (...)
        {
        }

        FATP_ASSERT_EQ(rollback_count, 0, "Dismissed FATP_SCOPE_FAIL should not execute");
    }

    // Test 5: FATP_SCOPE_FAIL swallows exceptions from cleanup (doesn't call terminate)
    {
        std::cout << colors::blue() << "  [TEST] FATP_SCOPE_FAIL swallows cleanup exceptions" << colors::reset()
                  << std::endl;

        int rollback_count = 0;
        bool outer_exception_caught = false;

        try
        {
            FATP_SCOPE_FAIL
            {
                ++rollback_count;
                throw std::logic_error("cleanup exception"); // This should be swallowed
            };
            throw std::runtime_error("original exception");
        }
        catch (const std::runtime_error&)
        {
            outer_exception_caught = true;
            // Should catch the ORIGINAL exception, not the cleanup exception
        }
        catch (...)
        {
            FATP_ASSERT_TRUE(false, "Should not catch cleanup exception");
        }

        FATP_ASSERT_EQ(rollback_count, 1, "FATP_SCOPE_FAIL should have executed");
        FATP_ASSERT_TRUE(outer_exception_caught, "Original exception should propagate");
    }

    std::cout << colors::green() << "FATP_SCOPE_FAIL: Tests passed." << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(scope_success)
{
    std::cout << colors::cyan() << "\nTesting FATP_SCOPE_SUCCESS..." << colors::reset() << std::endl;

    // Test 1: FATP_SCOPE_SUCCESS executes on normal exit
    {
        std::cout << colors::blue() << "  [TEST] FATP_SCOPE_SUCCESS executes on normal exit" << colors::reset()
                  << std::endl;

        int commit_count = 0;

        {
            FATP_SCOPE_SUCCESS
            {
                ++commit_count;
            };
            // Normal exit
        }

        FATP_ASSERT_EQ(commit_count, 1, "FATP_SCOPE_SUCCESS should execute on normal exit");
    }

    // Test 2: FATP_SCOPE_SUCCESS does NOT execute on exception
    {
        std::cout << colors::blue() << "  [TEST] FATP_SCOPE_SUCCESS skipped on exception" << colors::reset()
                  << std::endl;

        int commit_count = 0;

        try
        {
            FATP_SCOPE_SUCCESS
            {
                ++commit_count;
            };
            throw std::runtime_error("test exception");
        }
        catch (...)
        {
            // Expected
        }

        FATP_ASSERT_EQ(commit_count, 0, "FATP_SCOPE_SUCCESS should not execute on exception");
    }

    // Test 3: Combined FATP_SCOPE_SUCCESS and FATP_SCOPE_FAIL
    {
        std::cout << colors::blue() << "  [TEST] Combined SUCCESS and FAIL - normal exit" << colors::reset()
                  << std::endl;

        int commits = 0, rollbacks = 0;

        {
            FATP_SCOPE_SUCCESS
            {
                ++commits;
            };
            FATP_SCOPE_FAIL
            {
                ++rollbacks;
            };
            // Normal exit
        }

        FATP_ASSERT_EQ(commits, 1, "FATP_SCOPE_SUCCESS should execute");
        FATP_ASSERT_EQ(rollbacks, 0, "FATP_SCOPE_FAIL should not execute");
    }

    // Test 4: Combined FATP_SCOPE_SUCCESS and FATP_SCOPE_FAIL on exception
    {
        std::cout << colors::blue() << "  [TEST] Combined SUCCESS and FAIL - exception" << colors::reset() << std::endl;

        int commits = 0, rollbacks = 0;

        try
        {
            FATP_SCOPE_SUCCESS
            {
                ++commits;
            };
            FATP_SCOPE_FAIL
            {
                ++rollbacks;
            };
            throw std::runtime_error("test");
        }
        catch (...)
        {
        }

        FATP_ASSERT_EQ(commits, 0, "FATP_SCOPE_SUCCESS should not execute on exception");
        FATP_ASSERT_EQ(rollbacks, 1, "FATP_SCOPE_FAIL should execute on exception");
    }

    std::cout << colors::green() << "FATP_SCOPE_SUCCESS: Tests passed." << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// Conditional Guard Move Assignment Tests
// =============================================================================

FATP_TEST_CASE(conditional_move_assignment)
{
    std::cout << colors::cyan() << "\nTesting Conditional Guard Move Assignment..." << colors::reset() << std::endl;

    // Test 1: ScopeGuardOnFail move assignment during normal operation
    // Note: We use std::function because each lambda has a unique type
    {
        std::cout << colors::blue() << "  [TEST] ScopeGuardOnFail move assignment (normal)" << colors::reset()
                  << std::endl;

        int count1 = 0;
        int count2 = 0;

        try
        {
            std::function<void()> action1 = [&] {
                ++count1;
            };
            std::function<void()> action2 = [&] {
                ++count2;
            };

            ScopeGuardOnFail<std::function<void()>> guard1(std::move(action1));
            ScopeGuardOnFail<std::function<void()>> guard2(std::move(action2));

            // Move assign: guard2 gets guard1's action
            // Since we are NOT throwing yet, guard2's old action should NOT execute
            guard2 = std::move(guard1);

            throw std::runtime_error("Trigger");
        }
        catch (...)
        {
        }

        FATP_ASSERT_EQ(count1, 1, "Moved action should execute on exception");
        FATP_ASSERT_EQ(count2, 0, "Overwritten action should NOT execute (no exception when assigned)");
    }

    // Test 2: ScopeGuardOnSuccess move assignment during normal operation
    {
        std::cout << colors::blue() << "  [TEST] ScopeGuardOnSuccess move assignment (normal)" << colors::reset()
                  << std::endl;

        int count1 = 0;
        int count2 = 0;

        {
            std::function<void()> action1 = [&] {
                ++count1;
            };
            std::function<void()> action2 = [&] {
                ++count2;
            };

            ScopeGuardOnSuccess<std::function<void()>> guard1(std::move(action1));
            ScopeGuardOnSuccess<std::function<void()>> guard2(std::move(action2));

            // Move assign: guard2 gets guard1's action
            // Since we ARE in normal operation, guard2's old action SHOULD execute
            guard2 = std::move(guard1);
        }

        FATP_ASSERT_EQ(count1, 1, "Moved action should execute on success");
        FATP_ASSERT_EQ(count2, 1, "Overwritten action SHOULD execute (was in success state when assigned)");
    }

    // Test 3: ScopeGuardOnFail move assignment preserves baseline
    {
        std::cout << colors::blue() << "  [TEST] ScopeGuardOnFail baseline preservation" << colors::reset()
                  << std::endl;

        int count = 0;

        {
            std::function<void()> action1 = [&] {
                ++count;
            };
            std::function<void()> action2 = [] {};

            ScopeGuardOnFail<std::function<void()>> guard1(std::move(action1));
            ScopeGuardOnFail<std::function<void()>> guard2(std::move(action2));

            guard2 = std::move(guard1);
            // Normal exit - guard2 (with guard1's action) should NOT execute
        }

        FATP_ASSERT_EQ(count, 0, "Moved OnFail guard should not execute on normal exit");
    }

    std::cout << colors::green() << "Conditional Guard Move Assignment: Tests passed." << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// FATP_SCOPE_EXIT Alias Test
// =============================================================================

FATP_TEST_CASE(scope_exit_alias)
{
    std::cout << colors::cyan() << "\nTesting FATP_SCOPE_EXIT alias..." << colors::reset() << std::endl;

    // Test 1: FATP_SCOPE_EXIT works same as FATP_SCOPE_GUARD
    {
        std::cout << colors::blue() << "  [TEST] FATP_SCOPE_EXIT basic functionality" << colors::reset() << std::endl;

        int cleanup_count = 0;
        {
            FATP_SCOPE_EXIT
            {
                ++cleanup_count;
            };
        }
        FATP_ASSERT_EQ(cleanup_count, 1, "FATP_SCOPE_EXIT should execute on scope exit");
    }

    // Test 2: FATP_SCOPE_EXIT executes on exception too
    {
        std::cout << colors::blue() << "  [TEST] FATP_SCOPE_EXIT on exception" << colors::reset() << std::endl;

        int cleanup_count = 0;
        try
        {
            FATP_SCOPE_EXIT
            {
                ++cleanup_count;
            };
            throw std::runtime_error("test");
        }
        catch (...)
        {
        }
        FATP_ASSERT_EQ(cleanup_count, 1, "FATP_SCOPE_EXIT should execute on exception");
    }

    std::cout << colors::green() << "FATP_SCOPE_EXIT: Tests passed." << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// Noexcept Propagation Tests
// =============================================================================

FATP_TEST_CASE(noexcept_propagation)
{
    std::cout << colors::cyan() << "\nTesting Noexcept Propagation..." << colors::reset() << std::endl;

    // Test 1: Noexcept lambda results in noexcept move
    {
        std::cout << colors::blue() << "  [TEST] Noexcept action -> noexcept move" << colors::reset() << std::endl;

        auto guard = makeScopeGuard([]() noexcept {});
        using GuardType = decltype(guard);

        static_assert(std::is_nothrow_move_constructible_v<GuardType>,
                      "Guard with noexcept action should be nothrow move constructible");

        FATP_ASSERT_TRUE((std::is_nothrow_move_constructible_v<GuardType>), "Runtime verification of noexcept move");
    }

    std::cout << colors::green() << "Noexcept Propagation: Tests passed." << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// Main Test Function
// =============================================================================

} // namespace fat_p::testing::scopeguard

namespace fat_p::testing
{


void run_benchmarks()
{
    using namespace fat_p::testing;

    auto& out = *get_test_config().output;
    out << "\n" << colors::cyan() << "Sanity Benchmark:" << colors::reset() << "\n";
    out << "  (benchmarks are provided by benchmark executables; unit tests do not run full benchmarks)\n\n";
}

bool test_ScopeGuard()
{
    FATP_PRINT_HEADER(SCOPE GUARD)

    TestRunner runner;

    // Basic functionality
    FATP_RUN_TEST_NS(runner, scopeguard, basic);
    FATP_RUN_TEST_NS(runner, scopeguard, dismiss);
    FATP_RUN_TEST_NS(runner, scopeguard, move_semantics);

    // Policies
    FATP_RUN_TEST_NS(runner, scopeguard, nothrow_policy);
    FATP_RUN_TEST_NS(runner, scopeguard, terminate_policy);
    FATP_RUN_TEST_NS(runner, scopeguard, log_and_swallow_policy);
    FATP_RUN_TEST_NS(runner, scopeguard, rethrow_policy);

    // Macros
    FATP_RUN_TEST_NS(runner, scopeguard, macro_convenience);
    FATP_RUN_TEST_NS(runner, scopeguard, scope_exit_alias);
    FATP_RUN_TEST_NS(runner, scopeguard, scope_fail);
    FATP_RUN_TEST_NS(runner, scopeguard, scope_success);
    FATP_RUN_TEST_NS(runner, scopeguard, conditional_move_assignment);

    // Advanced
    FATP_RUN_TEST_NS(runner, scopeguard, complex_resource_management);
    FATP_RUN_TEST_NS(runner, scopeguard, type_traits);
    FATP_RUN_TEST_NS(runner, scopeguard, noexcept_propagation);

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
    return fat_p::testing::test_ScopeGuard() ? 0 : 1;
}
#endif
