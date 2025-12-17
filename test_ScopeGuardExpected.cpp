/**
 * @file test_ScopeGuardExpected.cpp
 * @brief Unit tests for ScopeGuardExpected.h bridge utilities
 *
 * @details Complete test suite for:
 * - make_rollback_guard: Conditional cleanup on Expected error
 * - make_success_guard: Conditional cleanup on Expected success
 * - make_capturing_guard: Exception capture into Expected
 * - with_resource: RAII wrapper returning Expected
 * - with_expected_resource: RAII wrapper with Expected resource
 * - Combined transaction patterns
 */

#include <iostream>
#include <string>
#include <stdexcept>

#include "ScopeGuardExpected.h"
#include "test_ScopeGuardExpected.h"
#include "FatPTest.h"

using namespace fat_p::testing;
using namespace fat_p;

namespace fat_p::testing
{

// =============================================================================
// make_rollback_guard Tests
// =============================================================================

bool test_MakeRollbackGuard()
{
    std::cout << colors::cyan() << "\nTesting make_rollback_guard..."
              << colors::reset() << std::endl;
    
    // Test 1: Rollback executes on error
    {
        std::cout << colors::blue() << "  [TEST] Rollback on error"
                  << colors::reset() << std::endl;
        
        int rollback_count = 0;
        {
            Expected<int, std::string> result = make_unexpected(std::string("error"));
            auto guard = make_rollback_guard(result, [&] { ++rollback_count; });
        }
        ASSERT_EQ(rollback_count, 1, "Rollback should execute on error");
    }
    
    // Test 2: Rollback skipped on success
    {
        std::cout << colors::blue() << "  [TEST] Rollback skipped on success"
                  << colors::reset() << std::endl;
        
        int rollback_count = 0;
        {
            Expected<int, std::string> result = 42;
            auto guard = make_rollback_guard(result, [&] { ++rollback_count; });
        }
        ASSERT_EQ(rollback_count, 0, "Rollback should not execute on success");
    }
    
    // Test 3: Rollback with void Expected
    {
        std::cout << colors::blue() << "  [TEST] Rollback with void Expected"
                  << colors::reset() << std::endl;
        
        int rollback_count = 0;
        {
            Expected<void, std::string> result = make_unexpected(std::string("failed"));
            auto guard = make_rollback_guard(result, [&] { ++rollback_count; });
        }
        ASSERT_EQ(rollback_count, 1, "Rollback should execute on void error");
    }
    
    // Test 4: Rollback can be dismissed
    {
        std::cout << colors::blue() << "  [TEST] Rollback can be dismissed"
                  << colors::reset() << std::endl;
        
        int rollback_count = 0;
        {
            Expected<int, std::string> result = make_unexpected(std::string("error"));
            auto guard = make_rollback_guard(result, [&] { ++rollback_count; });
            guard.dismiss();
        }
        ASSERT_EQ(rollback_count, 0, "Dismissed rollback should not execute");
    }
    
    std::cout << colors::green() << "make_rollback_guard: Tests passed."
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// make_success_guard Tests
// =============================================================================

bool test_MakeSuccessGuard()
{
    std::cout << colors::cyan() << "\nTesting make_success_guard..."
              << colors::reset() << std::endl;
    
    // Test 1: Success action executes on value
    {
        std::cout << colors::blue() << "  [TEST] Success action on value"
                  << colors::reset() << std::endl;
        
        int commit_count = 0;
        {
            Expected<int, std::string> result = 42;
            auto guard = make_success_guard(result, [&] { ++commit_count; });
        }
        ASSERT_EQ(commit_count, 1, "Success action should execute on value");
    }
    
    // Test 2: Success action skipped on error
    {
        std::cout << colors::blue() << "  [TEST] Success action skipped on error"
                  << colors::reset() << std::endl;
        
        int commit_count = 0;
        {
            Expected<int, std::string> result = make_unexpected(std::string("error"));
            auto guard = make_success_guard(result, [&] { ++commit_count; });
        }
        ASSERT_EQ(commit_count, 0, "Success action should not execute on error");
    }
    
    // Test 3: Success with void Expected
    {
        std::cout << colors::blue() << "  [TEST] Success with void Expected"
                  << colors::reset() << std::endl;
        
        int commit_count = 0;
        {
            Expected<void, std::string> result;  // Default constructs as success
            auto guard = make_success_guard(result, [&] { ++commit_count; });
        }
        ASSERT_EQ(commit_count, 1, "Success action should execute on void success");
    }
    
    std::cout << colors::green() << "make_success_guard: Tests passed."
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// Transaction Pattern Tests
// =============================================================================

bool test_TransactionPattern()
{
    std::cout << colors::cyan() << "\nTesting Transaction Pattern..."
              << colors::reset() << std::endl;
    
    // Test 1: Combined commit/rollback on success
    {
        std::cout << colors::blue() << "  [TEST] Combined guards - success path"
                  << colors::reset() << std::endl;
        
        int commits = 0;
        int rollbacks = 0;
        
        {
            Expected<void, std::string> result;
            auto commit_guard = make_success_guard(result, [&] { ++commits; });
            auto rollback_guard = make_rollback_guard(result, [&] { ++rollbacks; });
            
            // Simulate success
            result = Expected<void, std::string>{};
        }
        
        ASSERT_EQ(commits, 1, "Should commit on success");
        ASSERT_EQ(rollbacks, 0, "Should not rollback on success");
    }
    
    // Test 2: Combined commit/rollback on failure
    {
        std::cout << colors::blue() << "  [TEST] Combined guards - failure path"
                  << colors::reset() << std::endl;
        
        int commits = 0;
        int rollbacks = 0;
        
        {
            Expected<void, std::string> result;
            auto commit_guard = make_success_guard(result, [&] { ++commits; });
            auto rollback_guard = make_rollback_guard(result, [&] { ++rollbacks; });
            
            // Simulate failure
            result = make_unexpected(std::string("operation failed"));
        }
        
        ASSERT_EQ(commits, 0, "Should not commit on failure");
        ASSERT_EQ(rollbacks, 1, "Should rollback on failure");
    }
    
    // Test 3: Multi-step transaction
    {
        std::cout << colors::blue() << "  [TEST] Multi-step transaction"
                  << colors::reset() << std::endl;
        
        int step1_rollback = 0;
        int step2_rollback = 0;
        int final_commit = 0;
        
        auto do_transaction = [&](bool step1_ok, bool step2_ok) -> Expected<int, std::string> {
            Expected<int, std::string> result;
            
            // Step 1
            if (!step1_ok)
            {
                return make_unexpected(std::string("step1 failed"));
            }
            auto guard1 = make_rollback_guard(result, [&] { ++step1_rollback; });
            
            // Step 2
            if (!step2_ok)
            {
                result = make_unexpected(std::string("step2 failed"));
                return result;
            }
            auto guard2 = make_rollback_guard(result, [&] { ++step2_rollback; });
            
            // Success
            result = 100;
            auto commit_guard = make_success_guard(result, [&] { ++final_commit; });
            
            return result;
        };
        
        // All steps succeed
        auto r1 = do_transaction(true, true);
        SIMPLE_ASSERT(r1.has_value(), "Full success should return value");
        ASSERT_EQ(final_commit, 1, "Should commit");
        ASSERT_EQ(step1_rollback, 0, "Should not rollback step1");
        ASSERT_EQ(step2_rollback, 0, "Should not rollback step2");
        
        // Reset and test failure
        step1_rollback = 0;
        step2_rollback = 0;
        final_commit = 0;
        
        auto r2 = do_transaction(true, false);
        SIMPLE_ASSERT(!r2.has_value(), "Step2 failure should return error");
        ASSERT_EQ(final_commit, 0, "Should not commit on failure");
    }
    
    std::cout << colors::green() << "Transaction Pattern: Tests passed."
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// with_resource Tests
// =============================================================================

bool test_WithResource()
{
    std::cout << colors::cyan() << "\nTesting with_resource..."
              << colors::reset() << std::endl;
    
    // Test 1: Successful resource use
    {
        std::cout << colors::blue() << "  [TEST] Successful resource use"
                  << colors::reset() << std::endl;
        
        bool cleaned_up = false;
        auto result = with_resource<int, std::string>(
            [] { return 10; },
            [](int& val) { return val * 2; },
            [&](int&) { cleaned_up = true; }
        );
        
        SIMPLE_ASSERT(result.has_value(), "Should succeed");
        ASSERT_EQ(*result, 20, "Should return correct value");
        SIMPLE_ASSERT(cleaned_up, "Should cleanup");
    }
    
    // Test 2: Exception during action
    {
        std::cout << colors::blue() << "  [TEST] Exception during action"
                  << colors::reset() << std::endl;
        
        bool cleaned_up = false;
        auto result = with_resource<int, std::string>(
            [] { return 10; },
            [](int&) -> int { throw std::runtime_error("action failed"); },
            [&](int&) { cleaned_up = true; }
        );
        
        SIMPLE_ASSERT(!result.has_value(), "Should fail");
        SIMPLE_ASSERT(cleaned_up, "Should still cleanup on exception");
        SIMPLE_ASSERT(result.error().find("action failed") != std::string::npos,
            "Error should contain exception message");
    }
    
    // Test 3: Void return type
    {
        std::cout << colors::blue() << "  [TEST] Void return type"
                  << colors::reset() << std::endl;
        
        bool action_ran = false;
        bool cleaned_up = false;
        
        auto result = with_resource<void, std::string>(
            [] { return 42; },
            [&](int& val) { action_ran = (val == 42); },
            [&](int&) { cleaned_up = true; }
        );
        
        SIMPLE_ASSERT(result.has_value(), "Should succeed");
        SIMPLE_ASSERT(action_ran, "Action should run with resource");
        SIMPLE_ASSERT(cleaned_up, "Should cleanup");
    }
    
    // Test 4: Unknown exception
    {
        std::cout << colors::blue() << "  [TEST] Unknown exception handling"
                  << colors::reset() << std::endl;
        
        bool cleaned_up = false;
        auto result = with_resource<int, std::string>(
            [] { return 10; },
            [](int&) -> int { throw 42; },  // Non-std::exception
            [&](int&) { cleaned_up = true; }
        );
        
        SIMPLE_ASSERT(!result.has_value(), "Should fail");
        SIMPLE_ASSERT(cleaned_up, "Should cleanup on unknown exception");
        ASSERT_EQ(result.error(), "Unknown exception", "Should report unknown exception");
    }
    
    // Test 5: Double exception safety (action throws AND cleanup throws)
    {
        std::cout << colors::blue() << "  [TEST] Double exception safety"
                  << colors::reset() << std::endl;
        
        bool cleanup_ran = false;
        
        // This must NOT call std::terminate. If action throws and cleanup also throws,
        // the cleanup exception should be swallowed and the action exception propagated.
        auto result = with_resource<int, std::string>(
            [] { return 10; },
            [](int&) -> int { throw std::runtime_error("Primary Error"); },
            [&](int&) { 
                cleanup_ran = true;
                throw std::runtime_error("Cleanup Error");  // Should be swallowed
            }
        );
        
        SIMPLE_ASSERT(cleanup_ran, "Cleanup should have run");
        SIMPLE_ASSERT(!result.has_value(), "Result should be error");
        ASSERT_EQ(result.error(), "Primary Error", 
            "Should return the PRIMARY exception, not the cleanup exception");
    }
    
    std::cout << colors::green() << "with_resource: Tests passed."
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// with_expected_resource Tests
// =============================================================================

bool test_WithExpectedResource()
{
    std::cout << colors::cyan() << "\nTesting with_expected_resource..."
              << colors::reset() << std::endl;
    
    // Test 1: Successful resource acquisition and use
    {
        std::cout << colors::blue() << "  [TEST] Successful acquisition and use"
                  << colors::reset() << std::endl;
        
        bool cleaned_up = false;
        Expected<int, std::string> resource_result = 10;
        
        auto result = with_expected_resource<int, std::string, int>(
            std::move(resource_result),
            [](int& val) -> Expected<int, std::string> { return val * 3; },
            [&](int&) { cleaned_up = true; }
        );
        
        SIMPLE_ASSERT(result.has_value(), "Should succeed");
        ASSERT_EQ(*result, 30, "Should return correct value");
        SIMPLE_ASSERT(cleaned_up, "Should cleanup");
    }
    
    // Test 2: Failed resource acquisition
    {
        std::cout << colors::blue() << "  [TEST] Failed resource acquisition"
                  << colors::reset() << std::endl;
        
        bool action_ran = false;
        bool cleaned_up = false;
        Expected<int, std::string> resource_result = 
            make_unexpected(std::string("acquisition failed"));
        
        auto result = with_expected_resource<int, std::string, int>(
            std::move(resource_result),
            [&](int& val) -> Expected<int, std::string> { 
                action_ran = true;
                return val * 3; 
            },
            [&](int&) { cleaned_up = true; }
        );
        
        SIMPLE_ASSERT(!result.has_value(), "Should fail");
        SIMPLE_ASSERT(!action_ran, "Action should not run if acquisition failed");
        SIMPLE_ASSERT(!cleaned_up, "Should not cleanup if resource not acquired");
        ASSERT_EQ(result.error(), "acquisition failed", "Should propagate acquisition error");
    }
    
    // Test 3: Action returns error
    {
        std::cout << colors::blue() << "  [TEST] Action returns error"
                  << colors::reset() << std::endl;
        
        bool cleaned_up = false;
        Expected<int, std::string> resource_result = 10;
        
        auto result = with_expected_resource<int, std::string, int>(
            std::move(resource_result),
            [](int&) -> Expected<int, std::string> { 
                return make_unexpected(std::string("action error")); 
            },
            [&](int&) { cleaned_up = true; }
        );
        
        SIMPLE_ASSERT(!result.has_value(), "Should fail");
        SIMPLE_ASSERT(cleaned_up, "Should still cleanup even on action error");
        ASSERT_EQ(result.error(), "action error", "Should return action error");
    }
    
    std::cout << colors::green() << "with_expected_resource: Tests passed."
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// make_capturing_guard Tests
// =============================================================================

bool test_MakeCapturingGuard()
{
    std::cout << colors::cyan() << "\nTesting make_capturing_guard..."
              << colors::reset() << std::endl;
    
    // Test 1: Successful cleanup (no exception)
    {
        std::cout << colors::blue() << "  [TEST] Successful cleanup"
                  << colors::reset() << std::endl;
        
        Expected<void, std::string> cleanup_result;
        bool cleanup_ran = false;
        
        {
            auto guard = make_capturing_guard(cleanup_result, [&] { 
                cleanup_ran = true; 
            });
        }
        
        SIMPLE_ASSERT(cleanup_ran, "Cleanup should run");
        SIMPLE_ASSERT(cleanup_result.has_value(), "Should indicate success");
    }
    
    // Test 2: Exception during cleanup is captured (no stderr logging)
    {
        std::cout << colors::blue() << "  [TEST] Exception captured without logging"
                  << colors::reset() << std::endl;
        
        Expected<void, std::string> cleanup_result;
        
        {
            // This should NOT produce any stderr output since we use NothrowPolicy
            // and swallow the exception internally after capturing it
            auto guard = make_capturing_guard(cleanup_result, [] { 
                throw std::runtime_error("cleanup failed"); 
            });
        }
        
        SIMPLE_ASSERT(!cleanup_result.has_value(), "Should indicate failure");
        SIMPLE_ASSERT(cleanup_result.error().find("cleanup failed") != std::string::npos,
            "Should capture exception message");
    }
    
    // Test 3: Unknown exception captured
    {
        std::cout << colors::blue() << "  [TEST] Unknown exception captured"
                  << colors::reset() << std::endl;
        
        Expected<void, std::string> cleanup_result;
        
        {
            auto guard = make_capturing_guard(cleanup_result, [] { 
                throw 42;  // Non-std::exception
            });
        }
        
        SIMPLE_ASSERT(!cleanup_result.has_value(), "Should indicate failure");
        SIMPLE_ASSERT(cleanup_result.error().find("Unknown") != std::string::npos,
            "Should capture unknown exception message");
    }
    
    std::cout << colors::green() << "make_capturing_guard: Tests passed."
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// Main Test Function
// =============================================================================

bool test_ScopeGuardExpected()
{
    PRINT_HEADER(SCOPE GUARD EXPECTED BRIDGE)
    
    TestRunner runner;
    
    runner.run_test("MakeRollbackGuard", test_MakeRollbackGuard);
    runner.run_test("MakeSuccessGuard", test_MakeSuccessGuard);
    runner.run_test("TransactionPattern", test_TransactionPattern);
    runner.run_test("WithResource", test_WithResource);
    runner.run_test("WithExpectedResource", test_WithExpectedResource);
    runner.run_test("MakeCapturingGuard", test_MakeCapturingGuard);
    
    int failed = runner.print_summary();
    return failed == 0;
}

}  // namespace fat_p::testing
