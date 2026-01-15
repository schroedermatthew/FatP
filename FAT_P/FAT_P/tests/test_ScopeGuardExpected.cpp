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
/*
FATP_META:
  meta_version: 1
  component: ScopeGuardExpected
  file_role: test
  path: tests/test_ScopeGuardExpected.cpp
  namespace: fat_p
  summary: "Unit tests for ScopeGuardExpected."
  related:
    docs_search: "ScopeGuardExpected"
    headers:
      - fat_p/ScopeGuardExpected.h
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

#include <iostream>
#include <stdexcept>
#include <string>

#include "FatPTest.h"
#include "ScopeGuardExpected.h"

using namespace fat_p::testing;
using namespace fat_p;

namespace fat_p::testing::scopeguardexpected
{

// =============================================================================
// make_rollback_guard Tests
// =============================================================================

FATP_TEST_CASE(make_rollback_guard)
{
    std::cout << colors::cyan() << "\nTesting make_rollback_guard..." << colors::reset() << std::endl;

    // Test 1: Rollback executes on error
    {
        std::cout << colors::blue() << "  [TEST] Rollback on error" << colors::reset() << std::endl;

        int rollback_count = 0;
        {
            Expected<int, std::string> result = make_unexpected(std::string("error"));
            auto guard = make_rollback_guard(result,
                                             [&]
                                             {
                                                 ++rollback_count;
                                             });
        }
        FATP_ASSERT_EQ(rollback_count, 1, "Rollback should execute on error");
    }

    // Test 2: Rollback skipped on success
    {
        std::cout << colors::blue() << "  [TEST] Rollback skipped on success" << colors::reset() << std::endl;

        int rollback_count = 0;
        {
            Expected<int, std::string> result = 42;
            auto guard = make_rollback_guard(result,
                                             [&]
                                             {
                                                 ++rollback_count;
                                             });
        }
        FATP_ASSERT_EQ(rollback_count, 0, "Rollback should not execute on success");
    }

    // Test 3: Rollback with void Expected
    {
        std::cout << colors::blue() << "  [TEST] Rollback with void Expected" << colors::reset() << std::endl;

        int rollback_count = 0;
        {
            Expected<void, std::string> result = make_unexpected(std::string("failed"));
            auto guard = make_rollback_guard(result,
                                             [&]
                                             {
                                                 ++rollback_count;
                                             });
        }
        FATP_ASSERT_EQ(rollback_count, 1, "Rollback should execute on void error");
    }

    // Test 4: Rollback can be dismissed
    {
        std::cout << colors::blue() << "  [TEST] Rollback can be dismissed" << colors::reset() << std::endl;

        int rollback_count = 0;
        {
            Expected<int, std::string> result = make_unexpected(std::string("error"));
            auto guard = make_rollback_guard(result,
                                             [&]
                                             {
                                                 ++rollback_count;
                                             });
            guard.dismiss();
        }
        FATP_ASSERT_EQ(rollback_count, 0, "Dismissed rollback should not execute");
    }

    std::cout << colors::green() << "make_rollback_guard: Tests passed." << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// make_success_guard Tests
// =============================================================================

FATP_TEST_CASE(make_success_guard)
{
    std::cout << colors::cyan() << "\nTesting make_success_guard..." << colors::reset() << std::endl;

    // Test 1: Success action executes on value
    {
        std::cout << colors::blue() << "  [TEST] Success action on value" << colors::reset() << std::endl;

        int commit_count = 0;
        {
            Expected<int, std::string> result = 42;
            auto guard = make_success_guard(result,
                                            [&]
                                            {
                                                ++commit_count;
                                            });
        }
        FATP_ASSERT_EQ(commit_count, 1, "Success action should execute on value");
    }

    // Test 2: Success action skipped on error
    {
        std::cout << colors::blue() << "  [TEST] Success action skipped on error" << colors::reset() << std::endl;

        int commit_count = 0;
        {
            Expected<int, std::string> result = make_unexpected(std::string("error"));
            auto guard = make_success_guard(result,
                                            [&]
                                            {
                                                ++commit_count;
                                            });
        }
        FATP_ASSERT_EQ(commit_count, 0, "Success action should not execute on error");
    }

    // Test 3: Success with void Expected
    {
        std::cout << colors::blue() << "  [TEST] Success with void Expected" << colors::reset() << std::endl;

        int commit_count = 0;
        {
            Expected<void, std::string> result; // Default constructs as success
            auto guard = make_success_guard(result,
                                            [&]
                                            {
                                                ++commit_count;
                                            });
        }
        FATP_ASSERT_EQ(commit_count, 1, "Success action should execute on void success");
    }

    std::cout << colors::green() << "make_success_guard: Tests passed." << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// Transaction Pattern Tests
// =============================================================================

FATP_TEST_CASE(transaction_pattern)
{
    std::cout << colors::cyan() << "\nTesting Transaction Pattern..." << colors::reset() << std::endl;

    // Test 1: Combined commit/rollback on success
    {
        std::cout << colors::blue() << "  [TEST] Combined guards - success path" << colors::reset() << std::endl;

        int commits = 0;
        int rollbacks = 0;

        {
            Expected<void, std::string> result;
            auto commit_guard = make_success_guard(result,
                                                   [&]
                                                   {
                                                       ++commits;
                                                   });
            auto rollback_guard = make_rollback_guard(result,
                                                      [&]
                                                      {
                                                          ++rollbacks;
                                                      });

            // Simulate success
            result = Expected<void, std::string>{};
        }

        FATP_ASSERT_EQ(commits, 1, "Should commit on success");
        FATP_ASSERT_EQ(rollbacks, 0, "Should not rollback on success");
    }

    // Test 2: Combined commit/rollback on failure
    {
        std::cout << colors::blue() << "  [TEST] Combined guards - failure path" << colors::reset() << std::endl;

        int commits = 0;
        int rollbacks = 0;

        {
            Expected<void, std::string> result;
            auto commit_guard = make_success_guard(result,
                                                   [&]
                                                   {
                                                       ++commits;
                                                   });
            auto rollback_guard = make_rollback_guard(result,
                                                      [&]
                                                      {
                                                          ++rollbacks;
                                                      });

            // Simulate failure
            result = make_unexpected(std::string("operation failed"));
        }

        FATP_ASSERT_EQ(commits, 0, "Should not commit on failure");
        FATP_ASSERT_EQ(rollbacks, 1, "Should rollback on failure");
    }

    // Test 3: Multi-step transaction
    {
        std::cout << colors::blue() << "  [TEST] Multi-step transaction" << colors::reset() << std::endl;

        int step1_rollback = 0;
        int step2_rollback = 0;
        int final_commit = 0;

        auto do_transaction = [&](bool step1_ok, bool step2_ok) -> Expected<int, std::string>
        {
            Expected<int, std::string> result;

            // Step 1
            if (!step1_ok)
            {
                return make_unexpected(std::string("step1 failed"));
            }
            auto guard1 = make_rollback_guard(result,
                                              [&]
                                              {
                                                  ++step1_rollback;
                                              });

            // Step 2
            if (!step2_ok)
            {
                result = make_unexpected(std::string("step2 failed"));
                return result;
            }
            auto guard2 = make_rollback_guard(result,
                                              [&]
                                              {
                                                  ++step2_rollback;
                                              });

            // Success
            result = 100;
            auto commit_guard = make_success_guard(result,
                                                   [&]
                                                   {
                                                       ++final_commit;
                                                   });

            return result;
        };

        // All steps succeed
        auto r1 = do_transaction(true, true);
        FATP_ASSERT_TRUE(r1.has_value(), "Full success should return value");
        FATP_ASSERT_EQ(final_commit, 1, "Should commit");
        FATP_ASSERT_EQ(step1_rollback, 0, "Should not rollback step1");
        FATP_ASSERT_EQ(step2_rollback, 0, "Should not rollback step2");

        // Reset and test failure
        step1_rollback = 0;
        step2_rollback = 0;
        final_commit = 0;

        auto r2 = do_transaction(true, false);
        FATP_ASSERT_FALSE(r2.has_value(), "Step2 failure should return error");
        FATP_ASSERT_EQ(final_commit, 0, "Should not commit on failure");
    }

    std::cout << colors::green() << "Transaction Pattern: Tests passed." << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// with_resource Tests
// =============================================================================

FATP_TEST_CASE(with_resource)
{
    std::cout << colors::cyan() << "\nTesting with_resource..." << colors::reset() << std::endl;

    // Test 1: Successful resource use
    {
        std::cout << colors::blue() << "  [TEST] Successful resource use" << colors::reset() << std::endl;

        bool cleaned_up = false;
        auto result = with_resource<int, std::string>(
            []
            {
                return 10;
            },
            [](int& val)
            {
                return val * 2;
            },
            [&](int&)
            {
                cleaned_up = true;
            });

        FATP_ASSERT_TRUE(result.has_value(), "Should succeed");
        FATP_ASSERT_EQ(*result, 20, "Should return correct value");
        FATP_ASSERT_TRUE(cleaned_up, "Should cleanup");
    }

    // Test 2: Exception during action
    {
        std::cout << colors::blue() << "  [TEST] Exception during action" << colors::reset() << std::endl;

        bool cleaned_up = false;
        auto result = with_resource<int, std::string>(
            []
            {
                return 10;
            },
            [](int&) -> int
            {
                throw std::runtime_error("action failed");
            },
            [&](int&)
            {
                cleaned_up = true;
            });

        FATP_ASSERT_FALSE(result.has_value(), "Should fail");
        FATP_ASSERT_TRUE(cleaned_up, "Should still cleanup on exception");
        FATP_ASSERT_NE(result.error().find("action failed"),
                       std::string::npos,
                       "Error should contain exception message");
    }

    // Test 3: Void return type
    {
        std::cout << colors::blue() << "  [TEST] Void return type" << colors::reset() << std::endl;

        bool action_ran = false;
        bool cleaned_up = false;

        auto result = with_resource<void, std::string>(
            []
            {
                return 42;
            },
            [&](int& val)
            {
                action_ran = (val == 42);
            },
            [&](int&)
            {
                cleaned_up = true;
            });

        FATP_ASSERT_TRUE(result.has_value(), "Should succeed");
        FATP_ASSERT_TRUE(action_ran, "Action should run with resource");
        FATP_ASSERT_TRUE(cleaned_up, "Should cleanup");
    }

    // Test 4: Unknown exception
    {
        std::cout << colors::blue() << "  [TEST] Unknown exception handling" << colors::reset() << std::endl;

        bool cleaned_up = false;
        auto result = with_resource<int, std::string>(
            []
            {
                return 10;
            },
            [](int&) -> int
            {
                throw 42;
            }, // Non-std::exception
            [&](int&)
            {
                cleaned_up = true;
            });

        FATP_ASSERT_FALSE(result.has_value(), "Should fail");
        FATP_ASSERT_TRUE(cleaned_up, "Should cleanup on unknown exception");
        FATP_ASSERT_EQ(result.error(), "Unknown exception", "Should report unknown exception");
    }

    // Test 5: Double exception safety (action throws AND cleanup throws)
    {
        std::cout << colors::blue() << "  [TEST] Double exception safety" << colors::reset() << std::endl;

        bool cleanup_ran = false;

        // This must NOT call std::terminate. If action throws and cleanup also throws,
        // the cleanup exception should be swallowed and the action exception propagated.
        auto result = with_resource<int, std::string>(
            []
            {
                return 10;
            },
            [](int&) -> int
            {
                throw std::runtime_error("Primary Error");
            },
            [&](int&)
            {
                cleanup_ran = true;
                throw std::runtime_error("Cleanup Error"); // Should be swallowed
            });

        FATP_ASSERT_TRUE(cleanup_ran, "Cleanup should have run");
        FATP_ASSERT_FALSE(result.has_value(), "Result should be error");
        FATP_ASSERT_EQ(result.error(),
                       "Primary Error",
                       "Should return the PRIMARY exception, not the cleanup exception");
    }

    std::cout << colors::green() << "with_resource: Tests passed." << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// with_expected_resource Tests
// =============================================================================

FATP_TEST_CASE(with_expected_resource)
{
    std::cout << colors::cyan() << "\nTesting with_expected_resource..." << colors::reset() << std::endl;

    // Test 1: Successful resource acquisition and use
    {
        std::cout << colors::blue() << "  [TEST] Successful acquisition and use" << colors::reset() << std::endl;

        bool cleaned_up = false;
        Expected<int, std::string> resource_result = 10;

        auto result = with_expected_resource<int, std::string, int>(
            std::move(resource_result),
            [](int& val) -> Expected<int, std::string>
            {
                return val * 3;
            },
            [&](int&)
            {
                cleaned_up = true;
            });

        FATP_ASSERT_TRUE(result.has_value(), "Should succeed");
        FATP_ASSERT_EQ(*result, 30, "Should return correct value");
        FATP_ASSERT_TRUE(cleaned_up, "Should cleanup");
    }

    // Test 2: Failed resource acquisition
    {
        std::cout << colors::blue() << "  [TEST] Failed resource acquisition" << colors::reset() << std::endl;

        bool action_ran = false;
        bool cleaned_up = false;
        Expected<int, std::string> resource_result = make_unexpected(std::string("acquisition failed"));

        auto result = with_expected_resource<int, std::string, int>(
            std::move(resource_result),
            [&](int& val) -> Expected<int, std::string>
            {
                action_ran = true;
                return val * 3;
            },
            [&](int&)
            {
                cleaned_up = true;
            });

        FATP_ASSERT_FALSE(result.has_value(), "Should fail");
        FATP_ASSERT_FALSE(action_ran, "Action should not run if acquisition failed");
        FATP_ASSERT_FALSE(cleaned_up, "Should not cleanup if resource not acquired");
        FATP_ASSERT_EQ(result.error(), "acquisition failed", "Should propagate acquisition error");
    }

    // Test 3: Action returns error
    {
        std::cout << colors::blue() << "  [TEST] Action returns error" << colors::reset() << std::endl;

        bool cleaned_up = false;
        Expected<int, std::string> resource_result = 10;

        auto result = with_expected_resource<int, std::string, int>(
            std::move(resource_result),
            [](int&) -> Expected<int, std::string>
            {
                return make_unexpected(std::string("action error"));
            },
            [&](int&)
            {
                cleaned_up = true;
            });

        FATP_ASSERT_FALSE(result.has_value(), "Should fail");
        FATP_ASSERT_TRUE(cleaned_up, "Should still cleanup even on action error");
        FATP_ASSERT_EQ(result.error(), "action error", "Should return action error");
    }

    std::cout << colors::green() << "with_expected_resource: Tests passed." << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// make_capturing_guard Tests
// =============================================================================

FATP_TEST_CASE(make_capturing_guard)
{
    std::cout << colors::cyan() << "\nTesting make_capturing_guard..." << colors::reset() << std::endl;

    // Test 1: Successful cleanup (no exception)
    {
        std::cout << colors::blue() << "  [TEST] Successful cleanup" << colors::reset() << std::endl;

        Expected<void, std::string> cleanup_result;
        bool cleanup_ran = false;

        {
            auto guard = make_capturing_guard(cleanup_result,
                                              [&]
                                              {
                                                  cleanup_ran = true;
                                              });
        }

        FATP_ASSERT_TRUE(cleanup_ran, "Cleanup should run");
        FATP_ASSERT_TRUE(cleanup_result.has_value(), "Should indicate success");
    }

    // Test 2: Exception during cleanup is captured (no stderr logging)
    {
        std::cout << colors::blue() << "  [TEST] Exception captured without logging" << colors::reset() << std::endl;

        Expected<void, std::string> cleanup_result;

        {
            // This should NOT produce any stderr output since we use NothrowPolicy
            // and swallow the exception internally after capturing it
            auto guard = make_capturing_guard(cleanup_result,
                                              []
                                              {
                                                  throw std::runtime_error("cleanup failed");
                                              });
        }

        FATP_ASSERT_FALSE(cleanup_result.has_value(), "Should indicate failure");
        FATP_ASSERT_NE(cleanup_result.error().find("cleanup failed"),
                       std::string::npos,
                       "Should capture exception message");
    }

    // Test 3: Unknown exception captured
    {
        std::cout << colors::blue() << "  [TEST] Unknown exception captured" << colors::reset() << std::endl;

        Expected<void, std::string> cleanup_result;

        {
            auto guard = make_capturing_guard(cleanup_result,
                                              []
                                              {
                                                  throw 42; // Non-std::exception
                                              });
        }

        FATP_ASSERT_FALSE(cleanup_result.has_value(), "Should indicate failure");
        FATP_ASSERT_NE(cleanup_result.error().find("Unknown"),
                       std::string::npos,
                       "Should capture unknown exception message");
    }

    std::cout << colors::green() << "make_capturing_guard: Tests passed." << colors::reset() << std::endl;
    return true;
}

} // namespace fat_p::testing::scopeguardexpected

// =============================================================================
// Main Test Function
// =============================================================================

namespace fat_p::testing
{

bool test_ScopeGuardExpected()
{
    FATP_PRINT_HEADER(SCOPE GUARD EXPECTED BRIDGE)

    TestRunner runner;

    FATP_RUN_TEST_NS(runner, scopeguardexpected, make_rollback_guard);
    FATP_RUN_TEST_NS(runner, scopeguardexpected, make_success_guard);
    FATP_RUN_TEST_NS(runner, scopeguardexpected, transaction_pattern);
    FATP_RUN_TEST_NS(runner, scopeguardexpected, with_resource);
    FATP_RUN_TEST_NS(runner, scopeguardexpected, with_expected_resource);
    FATP_RUN_TEST_NS(runner, scopeguardexpected, make_capturing_guard);

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_ScopeGuardExpected() ? 0 : 1;
}
#endif
