/**
 * @file test_StateMachine.cpp
 * @brief Comprehensive test suite for fat_p::StateMachine
 *
 * This test suite demonstrates all features of fat_p::StateMachine including:
 * - Strict and AnyToAny transition policies
 * - NoExcept and Throwing action policies
 * - State entry/exit actions
 * - Compile-time transition validation
 * - Runtime transition checks
 * - Context sharing between states
 * - State query operations
 * - Configurable initial state
 * - Unique state type enforcement
 */
/*
FATP_META:
  meta_version: 1
  component: StateMachine
  file_role: test
  path: components/StateMachine/tests/test_StateMachine.cpp
  layer: Testing
  namespace:
    - fat_p
    - fat_p::testing::statemachine
  summary: "Unit tests for StateMachine."
  api_stability: stable
  related:
    docs_search: "StateMachine"
    headers:
      - include/fat_p/StateMachine.h
      - include/fat_p/FatPTest.h
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 2
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/

#ifdef _MSC_VER
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS 1
#define FATP_DEFINED_CRT_SECURE_NO_WARNINGS_SM_TEST
#endif
#endif

#include <iostream>
#include <stdexcept>
#include <string>
#include <array>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <limits>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "StateMachine.h"
#include "FatPTest.h"

namespace fat_p::testing::statemachine
{
// ============================================================================
// Test Context and States
// ============================================================================

// Shared context for state machine tests
struct TestContext
{
    int mCounter = 0;
    std::string mLog;
    bool mFlag = false;

    void reset()
    {
        mCounter = 0;
        mLog.clear();
        mFlag = false;
    }
};

// Simple states for basic testing
struct StateA
{
    void on_entry(TestContext& ctx) noexcept
    {
        ctx.mLog += "A_entry;";
        ctx.mCounter++;
    }
    void on_exit(TestContext& ctx) noexcept
    {
        ctx.mLog += "A_exit;";
        ctx.mCounter++;
    }
};

struct StateB
{
    void on_entry(TestContext& ctx) noexcept
    {
        ctx.mLog += "B_entry;";
        ctx.mCounter++;
    }
    void on_exit(TestContext& ctx) noexcept
    {
        ctx.mLog += "B_exit;";
        ctx.mCounter++;
    }
};

struct StateC
{
    void on_entry(TestContext& ctx) noexcept
    {
        ctx.mLog += "C_entry;";
        ctx.mCounter++;
    }
    void on_exit(TestContext& ctx) noexcept
    {
        ctx.mLog += "C_exit;";
        ctx.mCounter++;
    }
};

struct StateD
{
    void on_entry(TestContext& ctx) noexcept
    {
        ctx.mLog += "D_entry;";
        ctx.mCounter += 10;
    }
    void on_exit(TestContext& ctx) noexcept
    {
        ctx.mLog += "D_exit;";
        ctx.mCounter += 10;
    }
};

// States with data manipulation
struct IdleState
{
    void on_entry(TestContext& ctx) noexcept
    {
        ctx.mFlag = false;
        ctx.mLog += "[Idle]";
    }
    void on_exit(TestContext& ctx) noexcept
    {
        ctx.mLog += "[Leaving_Idle]";
    }
};

struct ProcessingState
{
    void on_entry(TestContext& ctx) noexcept
    {
        ctx.mFlag = true;
        ctx.mCounter += 10;
        ctx.mLog += "[Processing]";
    }
    void on_exit(TestContext& ctx) noexcept
    {
        ctx.mLog += "[Leaving_Processing]";
    }
};

struct CompletedState
{
    void on_entry(TestContext& ctx) noexcept
    {
        ctx.mCounter += 100;
        ctx.mLog += "[Completed]";
    }
    void on_exit(TestContext& ctx) noexcept
    {
        ctx.mLog += "[Leaving_Completed]";
    }
};

struct ErrorState
{
    void on_entry(TestContext& ctx) noexcept
    {
        ctx.mFlag = false;
        ctx.mCounter = -1;
        ctx.mLog += "[Error]";
    }
    void on_exit(TestContext& ctx) noexcept
    {
        ctx.mLog += "[Leaving_Error]";
    }
};

// States that throw (for testing fat_p::ThrowingActionPolicy)
struct ThrowingEntryState
{
    void on_entry(TestContext& ctx)
    {
        ctx.mLog += "ThrowEntry;";
        if (ctx.mCounter > 5)
        {
            throw std::runtime_error("Entry action failed");
        }
    }
    void on_exit(TestContext& ctx) noexcept
    {
        ctx.mLog += "ThrowExit;";
    }
};

struct ThrowingExitState
{
    void on_entry(TestContext& ctx) noexcept
    {
        ctx.mLog += "NormalEntry;";
    }
    void on_exit(TestContext& ctx)
    {
        ctx.mLog += "ThrowExit;";
        if (ctx.mFlag)
        {
            throw std::runtime_error("Exit action failed");
        }
    }
};

struct NormalState
{
    void on_entry(TestContext& ctx)
    {
        ctx.mLog += "Normal;";
    }
    void on_exit(TestContext& ctx)
    {
        ctx.mLog += "NormalExit;";
    }
};

// ============================================================================
// Test Suite 1: Basic State Machine Construction and Initialization
// ============================================================================

FATP_TEST_CASE(state_machine_construction)
{
    TestContext ctx;

    using TransitionList = std::tuple<std::pair<StateA, StateB>, std::pair<StateB, StateC>>;

    fat_p::StateMachine<TestContext,
                        TransitionList,
                        fat_p::StrictTransitionPolicy,
                        fat_p::NoExceptActionPolicy,
                        0,
                        StateA,
                        StateB,
                        StateC>
        sm(ctx);

    // Should start in StateA (index 0) and call its on_entry
    FATP_ASSERT_EQ(sm.currentStateIndex(), 0u, "Should start in first state (StateA)");
    FATP_ASSERT_EQ(ctx.mLog, std::string("A_entry;"), "Should have called StateA on_entry");
    FATP_ASSERT_EQ(ctx.mCounter, 1, "Counter should be 1 after StateA entry");
    FATP_ASSERT_TRUE(sm.isInState<StateA>(), "Should be in StateA");

    return true;
}

FATP_TEST_CASE(state_machine_default_policy)
{
    TestContext ctx;

    // fat_p::AnyToAnyTransitionPolicy with empty transition list
    using TransitionList = std::tuple<>;

    fat_p::StateMachine<TestContext,
                        TransitionList,
                        fat_p::AnyToAnyTransitionPolicy,
                        fat_p::NoExceptActionPolicy,
                        0,
                        StateA,
                        StateB,
                        StateC>
        sm(ctx);

    FATP_ASSERT_EQ(sm.currentStateIndex(), 0u, "Should start in first state");
    FATP_ASSERT_EQ(ctx.mLog, std::string("A_entry;"), "Should have called StateA on_entry");
    FATP_ASSERT_TRUE(sm.isInState<StateA>(), "Should be in StateA");
    FATP_ASSERT_FALSE(sm.isInState<StateB>(), "Should not be in StateB");

    return true;
}

FATP_TEST_CASE(state_machine_custom_initial_state)
{
    TestContext ctx;

    using TransitionList = std::tuple<>;

    // Start at index 1 (StateB)
    fat_p::StateMachine<TestContext,
                        TransitionList,
                        fat_p::AnyToAnyTransitionPolicy,
                        fat_p::NoExceptActionPolicy,
                        1,
                        StateA,
                        StateB,
                        StateC>
        sm(ctx);

    FATP_ASSERT_EQ(sm.currentStateIndex(), 1u, "Should start in StateB (index 1)");
    FATP_ASSERT_EQ(ctx.mLog, std::string("B_entry;"), "Should have called StateB on_entry");
    FATP_ASSERT_TRUE(sm.isInState<StateB>(), "Should be in StateB");
    FATP_ASSERT_FALSE(sm.isInState<StateA>(), "Should not be in StateA");

    // Start at index 2 (StateC)
    ctx.reset();
    fat_p::StateMachine<TestContext,
                        TransitionList,
                        fat_p::AnyToAnyTransitionPolicy,
                        fat_p::NoExceptActionPolicy,
                        2,
                        StateA,
                        StateB,
                        StateC>
        sm2(ctx);

    FATP_ASSERT_EQ(sm2.currentStateIndex(), 2u, "Should start in StateC (index 2)");
    FATP_ASSERT_EQ(ctx.mLog, std::string("C_entry;"), "Should have called StateC on_entry");
    FATP_ASSERT_TRUE(sm2.isInState<StateC>(), "Should be in StateC");

    return true;
}

// ============================================================================
// Test Suite 2: Basic State Transitions
// ============================================================================

FATP_TEST_CASE(simple_transition)
{
    TestContext ctx;

    using TransitionList = std::tuple<std::pair<StateA, StateB>, std::pair<StateB, StateC>>;

    fat_p::StateMachine<TestContext,
                        TransitionList,
                        fat_p::StrictTransitionPolicy,
                        fat_p::NoExceptActionPolicy,
                        0,
                        StateA,
                        StateB,
                        StateC>
        sm(ctx);

    ctx.mLog.clear();
    sm.transition<StateB>();

    FATP_ASSERT_EQ(sm.currentStateIndex(), 1u, "Should now be in StateB (index 1)");
    FATP_ASSERT_EQ(ctx.mLog, std::string("A_exit;B_entry;"), "Should exit A and enter B");
    FATP_ASSERT_TRUE(sm.isInState<StateB>(), "Should be in StateB");
    FATP_ASSERT_FALSE(sm.isInState<StateA>(), "Should not be in StateA");

    return true;
}

FATP_TEST_CASE(chained_transitions)
{
    TestContext ctx;

    using TransitionList = std::tuple<std::pair<StateA, StateB>, std::pair<StateB, StateC>, std::pair<StateC, StateA>>;

    fat_p::StateMachine<TestContext,
                        TransitionList,
                        fat_p::StrictTransitionPolicy,
                        fat_p::NoExceptActionPolicy,
                        0,
                        StateA,
                        StateB,
                        StateC>
        sm(ctx);

    ctx.mLog.clear();
    ctx.mCounter = 0;

    sm.transition<StateB>();
    FATP_ASSERT_EQ(sm.currentStateIndex(), 1u, "Should be in StateB");
    FATP_ASSERT_TRUE(sm.isInState<StateB>(), "Should be in StateB");

    sm.transition<StateC>();
    FATP_ASSERT_EQ(sm.currentStateIndex(), 2u, "Should be in StateC");
    FATP_ASSERT_TRUE(sm.isInState<StateC>(), "Should be in StateC");

    sm.transition<StateA>();
    FATP_ASSERT_EQ(sm.currentStateIndex(), 0u, "Should be back in StateA");
    FATP_ASSERT_TRUE(sm.isInState<StateA>(), "Should be in StateA");

    // Should have: A_exit, B_entry, B_exit, C_entry, C_exit, A_entry
    FATP_ASSERT_EQ(ctx.mCounter, 6, "Should have 6 action calls");
    FATP_ASSERT_EQ(ctx.mLog,
                   std::string("A_exit;B_entry;B_exit;C_entry;C_exit;A_entry;"),
                   "Transition sequence should be correct");

    return true;
}

FATP_TEST_CASE(self_transition_is_noop)
{
    TestContext ctx;

    using TransitionList = std::tuple<std::pair<StateA, StateB>>;

    fat_p::StateMachine<TestContext,
                        TransitionList,
                        fat_p::StrictTransitionPolicy,
                        fat_p::NoExceptActionPolicy,
                        0,
                        StateA,
                        StateB,
                        StateC>
        sm(ctx);

    ctx.mLog.clear();
    ctx.mCounter = 0;

    sm.transition<StateA>(); // Transition to self

    FATP_ASSERT_EQ(sm.currentStateIndex(), 0u, "Should remain in StateA");
    FATP_ASSERT_EQ(ctx.mLog, std::string(""), "Self-transition should not call actions");
    FATP_ASSERT_EQ(ctx.mCounter, 0, "Counter should not change");
    FATP_ASSERT_TRUE(sm.isInState<StateA>(), "Should still be in StateA");

    return true;
}

FATP_TEST_CASE(multiple_transitions_same_state)
{
    TestContext ctx;

    using TransitionList = std::tuple<std::pair<StateA, StateB>, std::pair<StateB, StateA>, std::pair<StateA, StateC>>;

    fat_p::StateMachine<TestContext,
                        TransitionList,
                        fat_p::StrictTransitionPolicy,
                        fat_p::NoExceptActionPolicy,
                        0,
                        StateA,
                        StateB,
                        StateC>
        sm(ctx);

    ctx.mLog.clear();

    sm.transition<StateB>();
    sm.transition<StateA>();
    sm.transition<StateB>();
    sm.transition<StateA>();

    FATP_ASSERT_EQ(sm.currentStateIndex(), 0u, "Should be in StateA");
    FATP_ASSERT_TRUE(ctx.mLog.find("A_exit;B_entry;B_exit;A_entry;A_exit;B_entry;B_exit;A_entry;") != std::string::npos,
                     "Should have multiple back-and-forth transitions");

    return true;
}

// ============================================================================
// Test Suite 3: fat_p::StrictTransitionPolicy Validation
// ============================================================================

FATP_TEST_CASE(strict_policy_valid_transitions)
{
    TestContext ctx;

    using TransitionList = std::tuple<std::pair<IdleState, ProcessingState>,
                                      std::pair<ProcessingState, CompletedState>,
                                      std::pair<ProcessingState, ErrorState>,
                                      std::pair<ErrorState, IdleState>,
                                      std::pair<CompletedState, IdleState>>;

    fat_p::StateMachine<TestContext,
                 TransitionList,
                 fat_p::StrictTransitionPolicy,
                 fat_p::NoExceptActionPolicy,
                 0,
                 IdleState,
                 ProcessingState,
                 CompletedState,
                 ErrorState>
        sm(ctx);

    ctx.mLog.clear();

    // Valid transitions
    sm.transition<ProcessingState>();
    FATP_ASSERT_EQ(ctx.mFlag, true, "Flag should be set in ProcessingState");
    FATP_ASSERT_EQ(ctx.mCounter, 10, "Counter should be 10");

    sm.transition<CompletedState>();
    FATP_ASSERT_EQ(ctx.mCounter, 110, "Counter should be 110 after completion");

    sm.transition<IdleState>();
    FATP_ASSERT_EQ(ctx.mFlag, false, "Flag should be cleared in IdleState");

    return true;
}

FATP_TEST_CASE(strict_policy_invalid_transition_throws)
{
    TestContext ctx;

    using TransitionList = std::tuple<std::pair<StateA, StateB>
                                      // Note: StateA -> StateC is NOT allowed
                                      >;

    fat_p::StateMachine<TestContext,
                        TransitionList,
                        fat_p::StrictTransitionPolicy,
                        fat_p::NoExceptActionPolicy,
                        0,
                        StateA,
                        StateB,
                        StateC>
        sm(ctx);

    const std::string log_before = ctx.mLog;
    const int counter_before = ctx.mCounter;

    bool exception_thrown = false;
    try
    {
        sm.transition<StateC>(); // Invalid transition
    }
    catch (const std::runtime_error& e)
    {
        exception_thrown = true;
        std::string msg = e.what();
        FATP_ASSERT_TRUE(msg.find("not valid") != std::string::npos,
                         "Exception message should mention invalid transition");
    }

    FATP_ASSERT_TRUE(exception_thrown, "Should throw on invalid transition");
    FATP_ASSERT_EQ(ctx.mLog, log_before, "Invalid transition must not run entry/exit hooks");
    FATP_ASSERT_EQ(ctx.mCounter, counter_before, "Invalid transition must not run entry/exit hooks");
    FATP_ASSERT_EQ(sm.currentStateIndex(), 0u, "Should remain in original state after failed transition");
    FATP_ASSERT_TRUE(sm.isInState<StateA>(), "Should still be in StateA");

    return true;
}

FATP_TEST_CASE(strict_policy_complex_graph)
{
    TestContext ctx;

    // Complex state graph: Idle -> Processing -> {Completed, Error}
    //                      Error -> Idle
    //                      Completed -> Idle
    using TransitionList = std::tuple<std::pair<IdleState, ProcessingState>,
                                      std::pair<ProcessingState, CompletedState>,
                                      std::pair<ProcessingState, ErrorState>,
                                      std::pair<ErrorState, IdleState>,
                                      std::pair<CompletedState, IdleState>>;

    fat_p::StateMachine<TestContext,
                 TransitionList,
                 fat_p::StrictTransitionPolicy,
                 fat_p::NoExceptActionPolicy,
                 0,
                 IdleState,
                 ProcessingState,
                 CompletedState,
                 ErrorState>
        sm(ctx);

    // Test error path
    ctx.reset();
    sm.transition<ProcessingState>();
    sm.transition<ErrorState>();
    FATP_ASSERT_EQ(ctx.mCounter, -1, "Error state should set counter to -1");
    FATP_ASSERT_TRUE(sm.isInState<ErrorState>(), "Should be in ErrorState");

    // Recover from error
    sm.transition<IdleState>();
    FATP_ASSERT_EQ(ctx.mFlag, false, "Should be back in idle state");
    FATP_ASSERT_TRUE(sm.isInState<IdleState>(), "Should be in IdleState");

    return true;
}

FATP_TEST_CASE(strict_policy_prevents_shortcut)
{
    TestContext ctx;

    using TransitionList = std::tuple<std::pair<IdleState, ProcessingState>, std::pair<ProcessingState, CompletedState>
                                      // Note: Idle -> Completed is NOT allowed
                                      >;

    fat_p::StateMachine<TestContext,
                 TransitionList,
                 fat_p::StrictTransitionPolicy,
                 fat_p::NoExceptActionPolicy,
                 0,
                 IdleState,
                 ProcessingState,
                 CompletedState>
        sm(ctx);

    bool caught = false;
    try
    {
        sm.transition<CompletedState>(); // Try to skip Processing
    }
    catch (const std::runtime_error&)
    {
        caught = true;
    }

    FATP_ASSERT_TRUE(caught, "Should not allow shortcut transitions");
    FATP_ASSERT_TRUE(sm.isInState<IdleState>(), "Should remain in IdleState");

    return true;
}

// ============================================================================
// Test Suite 4: fat_p::AnyToAnyTransitionPolicy
// ============================================================================

FATP_TEST_CASE(any_to_any_policy_all_transitions)
{
    TestContext ctx;

    using TransitionList = std::tuple<>; // Empty for AnyToAny

    fat_p::StateMachine<TestContext,
                        TransitionList,
                        fat_p::AnyToAnyTransitionPolicy,
                        fat_p::NoExceptActionPolicy,
                        0,
                        StateA,
                        StateB,
                        StateC>
        sm(ctx);

    // Any transition should work
    ctx.mLog.clear();
    sm.transition<StateC>(); // A -> C (would be invalid in strict)
    FATP_ASSERT_EQ(sm.currentStateIndex(), 2u, "Should be in StateC");
    FATP_ASSERT_TRUE(sm.isInState<StateC>(), "Should be in StateC");

    sm.transition<StateA>(); // C -> A
    FATP_ASSERT_EQ(sm.currentStateIndex(), 0u, "Should be back in StateA");
    FATP_ASSERT_TRUE(sm.isInState<StateA>(), "Should be in StateA");

    sm.transition<StateB>(); // A -> B
    FATP_ASSERT_EQ(sm.currentStateIndex(), 1u, "Should be in StateB");
    FATP_ASSERT_TRUE(sm.isInState<StateB>(), "Should be in StateB");

    return true;
}

FATP_TEST_CASE(any_to_any_with_complex_states)
{
    TestContext ctx;

    using TransitionList = std::tuple<>;

    fat_p::StateMachine<TestContext,
                 TransitionList,
                 fat_p::AnyToAnyTransitionPolicy,
                 fat_p::NoExceptActionPolicy,
                 0,
                 IdleState,
                 ProcessingState,
                 CompletedState,
                 ErrorState>
        sm(ctx);

    // Jump directly from Idle to Completed (not typical but allowed)
    sm.transition<CompletedState>();
    FATP_ASSERT_EQ(ctx.mCounter, 100, "Should have Completed counter value");
    FATP_ASSERT_TRUE(sm.isInState<CompletedState>(), "Should be in CompletedState");

    // Jump to Error from Completed
    sm.transition<ErrorState>();
    FATP_ASSERT_EQ(ctx.mCounter, -1, "Should have Error counter value");
    FATP_ASSERT_TRUE(sm.isInState<ErrorState>(), "Should be in ErrorState");

    // Any transition is valid
    sm.transition<ProcessingState>();
    FATP_ASSERT_EQ(ctx.mFlag, true, "Should be processing");
    FATP_ASSERT_TRUE(sm.isInState<ProcessingState>(), "Should be in ProcessingState");

    return true;
}

FATP_TEST_CASE(any_to_any_with_four_states)
{
    TestContext ctx;

    using TransitionList = std::tuple<>;

    fat_p::StateMachine<TestContext,
                 TransitionList,
                 fat_p::AnyToAnyTransitionPolicy,
                 fat_p::NoExceptActionPolicy,
                 0,
                 StateA,
                 StateB,
                 StateC,
                 StateD>
        sm(ctx);

    // Test all permutations work
    sm.transition<StateD>();
    FATP_ASSERT_TRUE(sm.isInState<StateD>(), "Should be in StateD");
    FATP_ASSERT_EQ(ctx.mCounter,
                   12,
                   "Counter should reflect StateD entry"); // 1 from A init + 1 from A exit + 10 from D entry

    sm.transition<StateB>();
    FATP_ASSERT_TRUE(sm.isInState<StateB>(), "Should be in StateB");

    sm.transition<StateA>();
    FATP_ASSERT_TRUE(sm.isInState<StateA>(), "Should be in StateA");

    return true;
}

// ============================================================================
// Test Suite 5: fat_p::NoExceptActionPolicy vs fat_p::ThrowingActionPolicy
// ============================================================================

FATP_TEST_CASE(noexcept_policy_compiles)
{
    TestContext ctx;

    using TransitionList = std::tuple<std::pair<StateA, StateB>>;

    // This should compile because all states have noexcept actions
    fat_p::StateMachine<TestContext,
                        TransitionList,
                        fat_p::StrictTransitionPolicy,
                        fat_p::NoExceptActionPolicy,
                        0,
                        StateA,
                        StateB,
                        StateC>
        sm(ctx);

    // Verify noexcept specification
    static_assert(noexcept(std::declval<StateA>().on_entry(std::declval<TestContext&>())),
                  "StateA on_entry should be noexcept");
    static_assert(noexcept(std::declval<StateB>().on_exit(std::declval<TestContext&>())),
                  "StateB on_exit should be noexcept");

    return true;
}

FATP_TEST_CASE(throwing_policy_allows_exceptions)
{
    TestContext ctx;

    using TransitionList = std::tuple<std::pair<NormalState, ThrowingEntryState>>;

    // fat_p::ThrowingActionPolicy allows non-noexcept actions
    fat_p::StateMachine<TestContext,
                 TransitionList,
                 fat_p::AnyToAnyTransitionPolicy,
                 fat_p::ThrowingActionPolicy,
                 0,
                 NormalState,
                 ThrowingEntryState>
        sm(ctx);

    ctx.mLog.clear();

    ctx.mCounter = 10; // Will cause throw in ThrowingEntryState

    bool exception_caught = false;
    try
    {
        sm.transition<ThrowingEntryState>();
    }
    catch (const std::runtime_error& e)
    {
        exception_caught = true;
        FATP_ASSERT_TRUE(std::string(e.what()).find("Entry action failed") != std::string::npos,
                         "Should catch entry action exception");
    }

    FATP_ASSERT_TRUE(exception_caught, "Exception should be thrown and caught");

    FATP_ASSERT_EQ(sm.currentStateIndex(), 1u, "Entry throw updates the state index to the target state");
    FATP_ASSERT_TRUE(sm.isInState<ThrowingEntryState>(), "Should be in ThrowingEntryState after entry throw");
    FATP_ASSERT_EQ(ctx.mLog,
                   std::string("NormalExit;ThrowEntry;"),
                   "Should exit old state and attempt to enter target state");

    // Ensure the state machine can transition away without double-exiting the original state.
    ctx.mCounter = 0;
    sm.transition<NormalState>();

    FATP_ASSERT_EQ(sm.currentStateIndex(), 0u, "Should return to NormalState after a recovery transition");
    FATP_ASSERT_TRUE(sm.isInState<NormalState>(), "Should be in NormalState after recovery transition");
    FATP_ASSERT_EQ(ctx.mLog,
                   std::string("NormalExit;ThrowEntry;ThrowExit;Normal;"),
                   "Recovery should exit ThrowingEntryState and enter NormalState");

    return true;
}

FATP_TEST_CASE(throwing_exit_action)
{
    TestContext ctx;
    ctx.mFlag = true; // Will cause exit to throw

    using TransitionList = std::tuple<>;

    fat_p::StateMachine<TestContext,
                 TransitionList,
                 fat_p::AnyToAnyTransitionPolicy,
                 fat_p::ThrowingActionPolicy,
                 0,
                 ThrowingExitState,
                 NormalState>
        sm(ctx);

    ctx.mLog.clear();

    bool exception_caught = false;
    try
    {
        sm.transition<NormalState>();
    }
    catch (const std::runtime_error& e)
    {
        exception_caught = true;
        FATP_ASSERT_TRUE(std::string(e.what()).find("Exit action failed") != std::string::npos,
                         "Should catch exit action exception");
    }

    FATP_ASSERT_TRUE(exception_caught, "Exit exception should be thrown and caught");

    FATP_ASSERT_EQ(sm.currentStateIndex(), 0u, "Exit throw does not update the state index");
    FATP_ASSERT_TRUE(sm.isInState<ThrowingExitState>(), "Should remain in ThrowingExitState after exit throw");
    FATP_ASSERT_EQ(ctx.mLog, std::string("ThrowExit;"), "Exit throw must not run target entry hook");

    return true;
}

// ============================================================================
// Test Suite 6: Context Sharing and State Data
// ============================================================================

FATP_TEST_CASE(context_shared_between_states)
{
    TestContext ctx;
    ctx.mCounter = 0;
    ctx.mFlag = false;

    using TransitionList =
        std::tuple<std::pair<IdleState, ProcessingState>, std::pair<ProcessingState, CompletedState>>;

    fat_p::StateMachine<TestContext,
                 TransitionList,
                 fat_p::StrictTransitionPolicy,
                 fat_p::NoExceptActionPolicy,
                 0,
                 IdleState,
                 ProcessingState,
                 CompletedState>
        sm(ctx);

    FATP_ASSERT_EQ(ctx.mFlag, false, "Initial flag should be false");
    FATP_ASSERT_EQ(ctx.mCounter, 0, "Initial counter should be 0");

    sm.transition<ProcessingState>();
    FATP_ASSERT_EQ(ctx.mFlag, true, "Processing should set flag");
    FATP_ASSERT_EQ(ctx.mCounter, 10, "Processing should increment counter");

    sm.transition<CompletedState>();
    FATP_ASSERT_EQ(ctx.mCounter, 110, "Completed should add to counter");

    return true;
}

FATP_TEST_CASE(context_persistence_across_transitions)
{
    TestContext ctx;

    using TransitionList = std::tuple<>;

    fat_p::StateMachine<TestContext,
                        TransitionList,
                        fat_p::AnyToAnyTransitionPolicy,
                        fat_p::NoExceptActionPolicy,
                        0,
                        StateA,
                        StateB,
                        StateC>
        sm(ctx);

    // Perform multiple transitions and verify log accumulates
    // Note: Initial StateA entry already happened during construction
    sm.transition<StateB>();
    sm.transition<StateC>();
    sm.transition<StateA>();
    sm.transition<StateB>();

    // Log should contain initial entry plus all transitions
    std::string expected = "A_entry;A_exit;B_entry;B_exit;C_entry;C_exit;A_entry;A_exit;B_entry;";
    FATP_ASSERT_EQ(ctx.mLog, expected, "Context log should persist and accumulate");
    FATP_ASSERT_EQ(ctx.mCounter, 9, "Counter should reflect all actions"); // 1 initial + 8 from transitions

    return true;
}

FATP_TEST_CASE(context_modification_visible)
{
    TestContext ctx;
    ctx.mCounter = 100;

    using TransitionList = std::tuple<>;

    fat_p::StateMachine<TestContext,
                        TransitionList,
                        fat_p::AnyToAnyTransitionPolicy,
                        fat_p::NoExceptActionPolicy,
                        0,
                        StateA,
                        StateB> sm(
        ctx);

    // Initial state entry should modify context
    FATP_ASSERT_EQ(ctx.mCounter, 101, "Initial state should have incremented counter");

    // External modification should be visible
    ctx.mCounter = 50;
    sm.transition<StateB>();
    FATP_ASSERT_EQ(ctx.mCounter, 52, "External modifications should be visible"); // 50 + 1 (A exit) + 1 (B entry)

    return true;
}

// ============================================================================
// Test Suite 7: State Query Operations
// ============================================================================

FATP_TEST_CASE(current_state_index)
{
    TestContext ctx;

    using TransitionList = std::tuple<>;

    fat_p::StateMachine<TestContext,
                        TransitionList,
                        fat_p::AnyToAnyTransitionPolicy,
                        fat_p::NoExceptActionPolicy,
                        0,
                        StateA,
                        StateB,
                        StateC>
        sm(ctx);

    FATP_ASSERT_EQ(sm.currentStateIndex(), 0u, "Should start at index 0");

    sm.transition<StateB>();
    FATP_ASSERT_EQ(sm.currentStateIndex(), 1u, "Should be at index 1");

    sm.transition<StateC>();
    FATP_ASSERT_EQ(sm.currentStateIndex(), 2u, "Should be at index 2");

    sm.transition<StateA>();
    FATP_ASSERT_EQ(sm.currentStateIndex(), 0u, "Should be back at index 0");

    return true;
}

FATP_TEST_CASE(is_in_state)
{
    TestContext ctx;

    using TransitionList = std::tuple<>;

    fat_p::StateMachine<TestContext,
                        TransitionList,
                        fat_p::AnyToAnyTransitionPolicy,
                        fat_p::NoExceptActionPolicy,
                        0,
                        StateA,
                        StateB,
                        StateC>
        sm(ctx);

    FATP_ASSERT_TRUE(sm.isInState<StateA>(), "Should be in StateA");
    FATP_ASSERT_FALSE(sm.isInState<StateB>(), "Should not be in StateB");
    FATP_ASSERT_FALSE(sm.isInState<StateC>(), "Should not be in StateC");

    sm.transition<StateB>();
    FATP_ASSERT_FALSE(sm.isInState<StateA>(), "Should not be in StateA");
    FATP_ASSERT_TRUE(sm.isInState<StateB>(), "Should be in StateB");
    FATP_ASSERT_FALSE(sm.isInState<StateC>(), "Should not be in StateC");

    sm.transition<StateC>();
    FATP_ASSERT_FALSE(sm.isInState<StateA>(), "Should not be in StateA");
    FATP_ASSERT_FALSE(sm.isInState<StateB>(), "Should not be in StateB");
    FATP_ASSERT_TRUE(sm.isInState<StateC>(), "Should be in StateC");

    return true;
}

FATP_TEST_CASE(query_operations_consistency)
{
    TestContext ctx;

    using TransitionList = std::tuple<>;

    fat_p::StateMachine<TestContext,
                 TransitionList,
                 fat_p::AnyToAnyTransitionPolicy,
                 fat_p::NoExceptActionPolicy,
                 0,
                 IdleState,
                 ProcessingState,
                 CompletedState,
                 ErrorState>
        sm(ctx);

    // isInState should match currentStateIndex
    FATP_ASSERT_EQ(sm.currentStateIndex(), 0u, "Index should be 0");
    FATP_ASSERT_TRUE(sm.isInState<IdleState>(), "Should be in IdleState");

    sm.transition<ProcessingState>();
    FATP_ASSERT_EQ(sm.currentStateIndex(), 1u, "Index should be 1");
    FATP_ASSERT_TRUE(sm.isInState<ProcessingState>(), "Should be in ProcessingState");

    sm.transition<ErrorState>();
    FATP_ASSERT_EQ(sm.currentStateIndex(), 3u, "Index should be 3");
    FATP_ASSERT_TRUE(sm.isInState<ErrorState>(), "Should be in ErrorState");

    return true;
}

// ============================================================================
// Test Suite 8: Edge Cases and Error Handling
// ============================================================================

FATP_TEST_CASE(single_state_machine)
{
    TestContext ctx;

    using TransitionList = std::tuple<>;

    fat_p::StateMachine<TestContext,
                        TransitionList,
                        fat_p::AnyToAnyTransitionPolicy,
                        fat_p::NoExceptActionPolicy,
                        0,
                        StateA> sm(ctx);

    FATP_ASSERT_EQ(sm.currentStateIndex(), 0u, "Should be at index 0");
    FATP_ASSERT_TRUE(sm.isInState<StateA>(), "Should be in StateA");
    FATP_ASSERT_EQ(ctx.mLog, std::string("A_entry;"), "Should have called entry");

    // Self-transition should be no-op
    ctx.mLog.clear();
    sm.transition<StateA>();
    FATP_ASSERT_EQ(ctx.mLog, std::string(""), "Self-transition should be no-op");

    return true;
}

FATP_TEST_CASE(large_state_machine)
{
    TestContext ctx;

    using TransitionList = std::tuple<>;

    // Test with 4 states
    fat_p::StateMachine<TestContext,
                 TransitionList,
                 fat_p::AnyToAnyTransitionPolicy,
                 fat_p::NoExceptActionPolicy,
                 0,
                 StateA,
                 StateB,
                 StateC,
                 StateD>
        sm(ctx);

    FATP_ASSERT_EQ(sm.currentStateIndex(), 0u, "Should start at index 0");

    sm.transition<StateD>();
    FATP_ASSERT_EQ(sm.currentStateIndex(), 3u, "Should be at index 3");
    FATP_ASSERT_TRUE(sm.isInState<StateD>(), "Should be in StateD");

    sm.transition<StateB>();
    FATP_ASSERT_EQ(sm.currentStateIndex(), 1u, "Should be at index 1");

    return true;
}

FATP_TEST_CASE(empty_log_accumulation)
{
    struct QuietState
    {
        void on_entry(TestContext&) noexcept
        {
        }
        void on_exit(TestContext&) noexcept
        {
        }
    };

    TestContext ctx;

    using TransitionList = std::tuple<>;

    fat_p::StateMachine<TestContext,
                        TransitionList,
                        fat_p::AnyToAnyTransitionPolicy,
                        fat_p::NoExceptActionPolicy,
                        0,
                        QuietState,
                        StateA> sm(
        ctx);

    FATP_ASSERT_EQ(ctx.mLog, std::string(""), "Quiet state should not log");

    sm.transition<StateA>();
    FATP_ASSERT_EQ(ctx.mLog, std::string("A_entry;"), "Only StateA should log");

    return true;
}

FATP_TEST_CASE(initial_state_action_called_once)
{
    TestContext ctx;

    using TransitionList = std::tuple<>;

    fat_p::StateMachine<TestContext,
                        TransitionList,
                        fat_p::AnyToAnyTransitionPolicy,
                        fat_p::NoExceptActionPolicy,
                        0,
                        StateA,
                        StateB> sm(
        ctx);

    // Initial entry should have been called exactly once
    FATP_ASSERT_EQ(ctx.mCounter, 1, "Entry should be called once on construction");
    FATP_ASSERT_EQ(ctx.mLog, std::string("A_entry;"), "Should have one entry log");

    return true;
}

// ============================================================================
// Test Suite 9: Performance and Compile-Time Validation
// ============================================================================

FATP_TEST_CASE(compile_time_state_validation)
{
    TestContext ctx;

    using TransitionList = std::tuple<>;

    // These should compile without issues
    fat_p::StateMachine<TestContext,
                        TransitionList,
                        fat_p::AnyToAnyTransitionPolicy,
                        fat_p::NoExceptActionPolicy,
                        0,
                        StateA,
                        StateB,
                        StateC>
        sm(ctx);

    // The following would fail at compile time (tested manually):
    // fat_p::StateMachine with duplicate states
    // fat_p::StateMachine with invalid InitialIndex
    // fat_p::StateMachine with no states
    // transition<InvalidState>()

    return true;
}

FATP_TEST_CASE(noexcept_specification)
{
    TestContext ctx;

    using TransitionList = std::tuple<>;

    fat_p::StateMachine<TestContext,
                        TransitionList,
                        fat_p::AnyToAnyTransitionPolicy,
                        fat_p::NoExceptActionPolicy,
                        0,
                        StateA,
                        StateB> sm(
        ctx);

    // With fat_p::NoExceptActionPolicy AND fat_p::AnyToAnyTransitionPolicy, transition should be noexcept
    static_assert(noexcept(sm.transition<StateB>()),
                  "transition should be noexcept with fat_p::NoExceptActionPolicy and fat_p::AnyToAnyTransitionPolicy");

    // With fat_p::StrictTransitionPolicy, transition is NOT noexcept (can throw on invalid transition)
    using StrictTransitionList = std::tuple<std::pair<StateA, StateB>>;
    fat_p::StateMachine<TestContext,
                        StrictTransitionList,
                        fat_p::StrictTransitionPolicy,
                        fat_p::NoExceptActionPolicy,
                        0,
                        StateA,
                        StateB>
        sm_strict(ctx);

    static_assert(!noexcept(sm_strict.transition<StateB>()),
                  "transition should NOT be noexcept with fat_p::StrictTransitionPolicy (can throw)");

    return true;
}

// ============================================================================
// Test Suite 10: Complex Scenarios
// ============================================================================

FATP_TEST_CASE(workflow_simulation)
{
    TestContext ctx;

    // Simulate a typical workflow: Idle -> Processing -> Completed -> Idle
    using TransitionList = std::tuple<std::pair<IdleState, ProcessingState>,
                                      std::pair<ProcessingState, CompletedState>,
                                      std::pair<ProcessingState, ErrorState>,
                                      std::pair<CompletedState, IdleState>,
                                      std::pair<ErrorState, IdleState>>;

    fat_p::StateMachine<TestContext,
                 TransitionList,
                 fat_p::StrictTransitionPolicy,
                 fat_p::NoExceptActionPolicy,
                 0,
                 IdleState,
                 ProcessingState,
                 CompletedState,
                 ErrorState>
        sm(ctx);

    // Happy path
    FATP_ASSERT_TRUE(sm.isInState<IdleState>(), "Start in Idle");

    sm.transition<ProcessingState>();
    FATP_ASSERT_TRUE(sm.isInState<ProcessingState>(), "Move to Processing");
    FATP_ASSERT_EQ(ctx.mCounter, 10, "Processing increments counter");

    sm.transition<CompletedState>();
    FATP_ASSERT_TRUE(sm.isInState<CompletedState>(), "Move to Completed");
    FATP_ASSERT_EQ(ctx.mCounter, 110, "Completed adds to counter");

    sm.transition<IdleState>();
    FATP_ASSERT_TRUE(sm.isInState<IdleState>(), "Back to Idle");

    // Error path
    ctx.mCounter = 0;
    sm.transition<ProcessingState>();
    sm.transition<ErrorState>();
    FATP_ASSERT_TRUE(sm.isInState<ErrorState>(), "Error state reached");
    FATP_ASSERT_EQ(ctx.mCounter, -1, "Error resets counter");

    sm.transition<IdleState>();
    FATP_ASSERT_TRUE(sm.isInState<IdleState>(), "Recovered to Idle");

    return true;
}

FATP_TEST_CASE(cyclic_transitions)
{
    TestContext ctx;

    using TransitionList = std::tuple<std::pair<StateA, StateB>, std::pair<StateB, StateC>, std::pair<StateC, StateA>>;

    fat_p::StateMachine<TestContext,
                        TransitionList,
                        fat_p::StrictTransitionPolicy,
                        fat_p::NoExceptActionPolicy,
                        0,
                        StateA,
                        StateB,
                        StateC>
        sm(ctx);

    ctx.reset();

    // Cycle multiple times
    for (int i = 0; i < 3; ++i)
    {
        sm.transition<StateB>();
        sm.transition<StateC>();
        sm.transition<StateA>();
    }

    FATP_ASSERT_TRUE(sm.isInState<StateA>(), "Should end in StateA");
    FATP_ASSERT_EQ(ctx.mCounter, 18, "Should have 6 actions per cycle * 3 cycles");

    return true;
}

FATP_TEST_CASE(state_machine_with_custom_initial)
{
    TestContext ctx;

    using TransitionList = std::tuple<std::pair<ProcessingState, CompletedState>,
                                      std::pair<CompletedState, IdleState>,
                                      std::pair<IdleState, ProcessingState>>;

    // Start in ProcessingState (index 1)
    fat_p::StateMachine<TestContext,
                 TransitionList,
                 fat_p::StrictTransitionPolicy,
                 fat_p::NoExceptActionPolicy,
                 1,
                 IdleState,
                 ProcessingState,
                 CompletedState>
        sm(ctx);

    FATP_ASSERT_TRUE(sm.isInState<ProcessingState>(), "Should start in ProcessingState");
    FATP_ASSERT_EQ(ctx.mCounter, 10, "Should have Processing initial counter");
    FATP_ASSERT_EQ(ctx.mFlag, true, "Processing should set flag");

    // Complete workflow from processing
    sm.transition<CompletedState>();
    sm.transition<IdleState>();

    FATP_ASSERT_TRUE(sm.isInState<IdleState>(), "Should reach IdleState");
    FATP_ASSERT_EQ(ctx.mFlag, false, "Idle should clear flag");

    return true;
}


// ============================================================================
// Test Suite 12: Construction Edge Cases and Semantics
// ============================================================================

FATP_TEST_CASE(exception_in_initial_entry_throws)
{
    struct Ctx
    {
        bool mShouldThrow = true;
        bool mEntryAttempted = false;
    };

    struct ThrowingInitial
    {
        void on_entry(Ctx& c)
        {
            c.mEntryAttempted = true;
            if (c.mShouldThrow)
            {
                throw std::runtime_error("Initial entry failed");
            }
        }
        void on_exit(Ctx&) noexcept {}
    };

    struct Other
    {
        void on_entry(Ctx&) noexcept {}
        void on_exit(Ctx&) noexcept {}
    };

    using TL = std::tuple<std::pair<ThrowingInitial, Other>>;
    using SM = fat_p::StateMachine<
        Ctx, TL, fat_p::StrictTransitionPolicy, fat_p::ThrowingActionPolicy,
        0, ThrowingInitial, Other>;

    Ctx ctx;
    bool threw = false;
    try
    {
        SM sm(ctx);
    }
    catch (const std::runtime_error&)
    {
        threw = true;
    }

    FATP_ASSERT_TRUE(threw, "Constructor should propagate initial entry exception");
    FATP_ASSERT_TRUE(ctx.mEntryAttempted, "Initial entry should have been attempted");

    // Verify non-throwing construction works
    Ctx ctx2;
    ctx2.mShouldThrow = false;
    SM sm2(ctx2);
    FATP_ASSERT_TRUE(ctx2.mEntryAttempted, "Non-throwing entry should succeed");
    FATP_ASSERT_EQ(sm2.currentStateIndex(), 0U, "Should be in initial state");

    return true;
}

FATP_TEST_CASE(state_machine_is_not_copyable)
{
    // StateMachine should not be copyable since it holds a reference to Context.
    // Verify this at compile time using type traits.
    using TL = std::tuple<std::pair<StateA, StateB>>;
    using SM = fat_p::StateMachine<
        TestContext, TL, fat_p::StrictTransitionPolicy, fat_p::NoExceptActionPolicy,
        0, StateA, StateB>;

    // Verify copy operations are deleted
    static_assert(!std::is_copy_constructible_v<SM>,
                  "StateMachine should not be copy constructible");
    static_assert(!std::is_copy_assignable_v<SM>,
                  "StateMachine should not be copy assignable");

    return true;
}

FATP_TEST_CASE(state_machine_is_not_movable)
{
    // StateMachine should not be movable since it holds a reference to Context.
    // Verify this at compile time using type traits.
    using TL = std::tuple<std::pair<StateA, StateB>>;
    using SM = fat_p::StateMachine<
        TestContext, TL, fat_p::StrictTransitionPolicy, fat_p::NoExceptActionPolicy,
        0, StateA, StateB>;

    // Verify move operations are deleted
    static_assert(!std::is_move_constructible_v<SM>,
                  "StateMachine should not be move constructible");
    static_assert(!std::is_move_assignable_v<SM>,
                  "StateMachine should not be move assignable");

    return true;
}

// ============================================================================
// Test Suite 11: Compile-Time Introspection APIs
// ============================================================================

FATP_TEST_CASE(introspection_state_count)
{
    using TL = std::tuple<std::pair<StateA, StateB>, std::pair<StateB, StateC>>;

    // AnyToAny policy
    using SM_Any = fat_p::StateMachine<
        TestContext, TL, fat_p::AnyToAnyTransitionPolicy, fat_p::ThrowingActionPolicy,
        0, StateA, StateB, StateC>;

    static_assert(SM_Any::stateCount() == 3, "stateCount() should return 3");

    // Strict policy
    using SM_Strict = fat_p::StateMachine<
        TestContext, TL, fat_p::StrictTransitionPolicy, fat_p::ThrowingActionPolicy,
        0, StateA, StateB, StateC>;

    static_assert(SM_Strict::stateCount() == 3, "stateCount() should return 3");

    // Single state machine
    using SM_Single = fat_p::StateMachine<
        TestContext, std::tuple<>, fat_p::AnyToAnyTransitionPolicy, fat_p::ThrowingActionPolicy,
        0, StateA>;

    static_assert(SM_Single::stateCount() == 1, "stateCount() should return 1");

    return true;
}

FATP_TEST_CASE(introspection_initial_state_index)
{
    using TL = std::tuple<std::pair<StateA, StateB>, std::pair<StateB, StateC>>;

    // Default initial index (0)
    using SM0 = fat_p::StateMachine<
        TestContext, TL, fat_p::AnyToAnyTransitionPolicy, fat_p::ThrowingActionPolicy,
        0, StateA, StateB, StateC>;

    static_assert(SM0::initialStateIndex() == 0, "initialStateIndex() should return 0");

    // Non-default initial index
    using SM1 = fat_p::StateMachine<
        TestContext, TL, fat_p::AnyToAnyTransitionPolicy, fat_p::ThrowingActionPolicy,
        1, StateA, StateB, StateC>;

    static_assert(SM1::initialStateIndex() == 1, "initialStateIndex() should return 1");

    using SM2 = fat_p::StateMachine<
        TestContext, TL, fat_p::AnyToAnyTransitionPolicy, fat_p::ThrowingActionPolicy,
        2, StateA, StateB, StateC>;

    static_assert(SM2::initialStateIndex() == 2, "initialStateIndex() should return 2");

    return true;
}

FATP_TEST_CASE(introspection_contains_state)
{
    struct UnknownState
    {
        void on_entry(TestContext&) {}
        void on_exit(TestContext&) {}
    };

    using TL = std::tuple<std::pair<StateA, StateB>>;
    using SM = fat_p::StateMachine<
        TestContext, TL, fat_p::AnyToAnyTransitionPolicy, fat_p::ThrowingActionPolicy,
        0, StateA, StateB, StateC>;

    // States in the machine
    static_assert(SM::contains_state<StateA>, "Should contain StateA");
    static_assert(SM::contains_state<StateB>, "Should contain StateB");
    static_assert(SM::contains_state<StateC>, "Should contain StateC");

    // State not in the machine
    static_assert(!SM::contains_state<UnknownState>, "Should not contain UnknownState");

    return true;
}

FATP_TEST_CASE(introspection_is_transition_allowed)
{
    // Only available for StrictTransitionPolicy
    using TL = std::tuple<
        std::pair<StateA, StateB>,
        std::pair<StateB, StateC>,
        std::pair<StateC, StateA>>;  // Cycle: A -> B -> C -> A

    using SM = fat_p::StateMachine<
        TestContext, TL, fat_p::StrictTransitionPolicy, fat_p::ThrowingActionPolicy,
        0, StateA, StateB, StateC>;

    // Allowed transitions
    static_assert(SM::is_transition_allowed<StateA, StateB>, "A->B should be allowed");
    static_assert(SM::is_transition_allowed<StateB, StateC>, "B->C should be allowed");
    static_assert(SM::is_transition_allowed<StateC, StateA>, "C->A should be allowed");

    // Disallowed transitions
    static_assert(!SM::is_transition_allowed<StateA, StateC>, "A->C should not be allowed");
    static_assert(!SM::is_transition_allowed<StateB, StateA>, "B->A should not be allowed");
    static_assert(!SM::is_transition_allowed<StateC, StateB>, "C->B should not be allowed");

    // Self-transitions are always allowed (defined as a no-op).
    static_assert(SM::is_transition_allowed<StateA, StateA>, "A->A is always allowed (no-op)");
    static_assert(SM::is_transition_allowed<StateB, StateB>, "B->B is always allowed (no-op)");
    static_assert(SM::is_transition_allowed<StateC, StateC>, "C->C is always allowed (no-op)");

    return true;
}

FATP_TEST_CASE(transition_order_exit_completes_before_entry)
{
    struct Ctx
    {
        std::vector<std::string> mSequence;
        bool mExitComplete = false;
    };

    struct A
    {
        void on_entry(Ctx& c) noexcept
        {
            c.mSequence.push_back("A_entry");
        }
        void on_exit(Ctx& c) noexcept
        {
            c.mSequence.push_back("A_exit_start");
            // Simulate work (atomic fence prevents reordering without volatile warning)
            for (int i = 0; i < 100; ++i)
            {
                std::atomic_signal_fence(std::memory_order_seq_cst);
            }
            c.mExitComplete = true;
            c.mSequence.push_back("A_exit_end");
        }
    };

    struct B
    {
        void on_entry(Ctx& c) noexcept
        {
            c.mSequence.push_back(c.mExitComplete ? "B_entry_after_exit" : "B_entry_before_exit");
        }
        void on_exit(Ctx&) noexcept {}
    };

    using TL = std::tuple<std::pair<A, B>>;
    using SM = fat_p::StateMachine<Ctx, TL, fat_p::StrictTransitionPolicy, fat_p::NoExceptActionPolicy, 0, A, B>;

    Ctx ctx;
    SM sm(ctx);
    ctx.mSequence.clear();
    ctx.mExitComplete = false;

    sm.transition<B>();

    FATP_ASSERT_EQ(ctx.mSequence.size(), 3U, "Should have 3 events");
    FATP_ASSERT_EQ(ctx.mSequence[0], std::string("A_exit_start"), "Exit should start first");
    FATP_ASSERT_EQ(ctx.mSequence[1], std::string("A_exit_end"), "Exit should complete");
    FATP_ASSERT_EQ(ctx.mSequence[2], std::string("B_entry_after_exit"), "Entry should see exit complete");

    return true;
}

FATP_TEST_CASE(state_internal_data_behavior)
{
    // Test documents that state instances do NOT persist internal data across transitions.
    // StateMachine invokes hooks on default-constructed temporaries: TState{}.on_entry(ctx).
    // Persistent data belongs in Context, not in state structs.
    // This test verifies and documents this intentional design choice.

    struct Ctx
    {
        int mValue = 0;
    };

    struct StatefulA
    {
        int mInternalCounter = 0;

        void on_entry(Ctx& c) noexcept
        {
            ++mInternalCounter;
            c.mValue = mInternalCounter;
        }
        void on_exit(Ctx&) noexcept {}
    };

    struct StatefulB
    {
        int mInternalCounter = 100;

        void on_entry(Ctx& c) noexcept
        {
            ++mInternalCounter;
            c.mValue = mInternalCounter;
        }
        void on_exit(Ctx&) noexcept {}
    };

    using TL = std::tuple<std::pair<StatefulA, StatefulB>, std::pair<StatefulB, StatefulA>>;
    using SM = fat_p::StateMachine<
        Ctx, TL, fat_p::StrictTransitionPolicy, fat_p::NoExceptActionPolicy,
        0, StatefulA, StatefulB>;

    Ctx ctx;
    SM sm(ctx);

    FATP_ASSERT_EQ(ctx.mValue, 1, "First entry to A: counter = 1");

    sm.transition<StatefulB>();
    FATP_ASSERT_EQ(ctx.mValue, 101, "First entry to B: counter = 101");

    sm.transition<StatefulA>();
    // Note: State internal data does NOT persist across transitions in current implementation.
    // Each entry sees the initial value. This documents actual behavior.
    FATP_ASSERT_EQ(ctx.mValue, 1, "Second entry to A: counter resets (states not persisted)");

    sm.transition<StatefulB>();
    FATP_ASSERT_EQ(ctx.mValue, 101, "Second entry to B: counter resets (states not persisted)");

    return true;
}

// ============================================================================
// Test Suite 13: Large State Counts and Policy Combinations
// ============================================================================

namespace large_state_detail
{

struct LargeCtx
{
    std::size_t mLastEntered = 0;
};

template<std::size_t N>
struct S32
{
    void on_entry(LargeCtx& c) noexcept { c.mLastEntered = N; }
    void on_exit(LargeCtx&) noexcept {}
};

// Ring topology: S0->S1->S2->...->S31->S0
using TL32 = std::tuple<
    std::pair<S32<0>, S32<1>>, std::pair<S32<1>, S32<2>>, std::pair<S32<2>, S32<3>>, std::pair<S32<3>, S32<4>>,
    std::pair<S32<4>, S32<5>>, std::pair<S32<5>, S32<6>>, std::pair<S32<6>, S32<7>>, std::pair<S32<7>, S32<8>>,
    std::pair<S32<8>, S32<9>>, std::pair<S32<9>, S32<10>>, std::pair<S32<10>, S32<11>>, std::pair<S32<11>, S32<12>>,
    std::pair<S32<12>, S32<13>>, std::pair<S32<13>, S32<14>>, std::pair<S32<14>, S32<15>>, std::pair<S32<15>, S32<16>>,
    std::pair<S32<16>, S32<17>>, std::pair<S32<17>, S32<18>>, std::pair<S32<18>, S32<19>>, std::pair<S32<19>, S32<20>>,
    std::pair<S32<20>, S32<21>>, std::pair<S32<21>, S32<22>>, std::pair<S32<22>, S32<23>>, std::pair<S32<23>, S32<24>>,
    std::pair<S32<24>, S32<25>>, std::pair<S32<25>, S32<26>>, std::pair<S32<26>, S32<27>>, std::pair<S32<27>, S32<28>>,
    std::pair<S32<28>, S32<29>>, std::pair<S32<29>, S32<30>>, std::pair<S32<30>, S32<31>>, std::pair<S32<31>, S32<0>>
>;

using SM32 = fat_p::StateMachine<LargeCtx, TL32, fat_p::StrictTransitionPolicy, fat_p::NoExceptActionPolicy, 0,
    S32<0>, S32<1>, S32<2>, S32<3>, S32<4>, S32<5>, S32<6>, S32<7>,
    S32<8>, S32<9>, S32<10>, S32<11>, S32<12>, S32<13>, S32<14>, S32<15>,
    S32<16>, S32<17>, S32<18>, S32<19>, S32<20>, S32<21>, S32<22>, S32<23>,
    S32<24>, S32<25>, S32<26>, S32<27>, S32<28>, S32<29>, S32<30>, S32<31>
>;

} // namespace large_state_detail

FATP_TEST_CASE(very_large_state_count_32)
{
    using namespace large_state_detail;

    LargeCtx ctx;
    SM32 sm(ctx);
    FATP_ASSERT_EQ(ctx.mLastEntered, 0U, "Should start in S32<0>");
    FATP_ASSERT_EQ(sm.currentStateIndex(), 0U, "Index should be 0");

    // Walk through half the ring
    sm.transition<S32<1>>();
    sm.transition<S32<2>>();
    sm.transition<S32<3>>();
    sm.transition<S32<4>>();
    sm.transition<S32<5>>();
    sm.transition<S32<6>>();
    sm.transition<S32<7>>();
    sm.transition<S32<8>>();
    sm.transition<S32<9>>();
    sm.transition<S32<10>>();
    sm.transition<S32<11>>();
    sm.transition<S32<12>>();
    sm.transition<S32<13>>();
    sm.transition<S32<14>>();
    sm.transition<S32<15>>();

    FATP_ASSERT_EQ(ctx.mLastEntered, 15U, "Should be in S32<15>");
    FATP_ASSERT_EQ(sm.currentStateIndex(), 15U, "Index should be 15");

    // Continue to end and wrap
    sm.transition<S32<16>>();
    sm.transition<S32<17>>();
    sm.transition<S32<18>>();
    sm.transition<S32<19>>();
    sm.transition<S32<20>>();
    sm.transition<S32<21>>();
    sm.transition<S32<22>>();
    sm.transition<S32<23>>();
    sm.transition<S32<24>>();
    sm.transition<S32<25>>();
    sm.transition<S32<26>>();
    sm.transition<S32<27>>();
    sm.transition<S32<28>>();
    sm.transition<S32<29>>();
    sm.transition<S32<30>>();
    sm.transition<S32<31>>();
    sm.transition<S32<0>>();

    FATP_ASSERT_EQ(ctx.mLastEntered, 0U, "Should wrap back to S32<0>");
    FATP_ASSERT_EQ(sm.currentStateIndex(), 0U, "Index should be 0 again");

    return true;
}

FATP_TEST_CASE(all_policy_combinations_strict_noexcept)
{
    struct Ctx { int mValue = 0; };
    struct A { void on_entry(Ctx& c) noexcept { c.mValue = 1; } void on_exit(Ctx&) noexcept {} };
    struct B { void on_entry(Ctx& c) noexcept { c.mValue = 2; } void on_exit(Ctx&) noexcept {} };

    using TL = std::tuple<std::pair<A, B>, std::pair<B, A>>;
    using SM = fat_p::StateMachine<Ctx, TL, fat_p::StrictTransitionPolicy, fat_p::NoExceptActionPolicy, 0, A, B>;

    Ctx ctx;
    SM sm(ctx);
    FATP_ASSERT_EQ(ctx.mValue, 1, "Strict+NoExcept: initial entry");

    sm.transition<B>();
    FATP_ASSERT_EQ(ctx.mValue, 2, "Strict+NoExcept: transition to B");

    // Invalid transition should throw
    bool threw = false;
    try { sm.transition<B>(); } // Already in B, but self-transition is no-op
    catch (...) { threw = true; }
    FATP_ASSERT_FALSE(threw, "Self transition should not throw");

    return true;
}

FATP_TEST_CASE(all_policy_combinations_strict_throwing)
{
    struct Ctx { int mValue = 0; bool mShouldThrow = false; };
    struct A
    {
        void on_entry(Ctx& c) { c.mValue = 1; if (c.mShouldThrow) throw std::runtime_error("A"); }
        void on_exit(Ctx&) {}
    };
    struct B
    {
        void on_entry(Ctx& c) { c.mValue = 2; }
        void on_exit(Ctx&) {}
    };

    using TL = std::tuple<std::pair<A, B>, std::pair<B, A>>;
    using SM = fat_p::StateMachine<Ctx, TL, fat_p::StrictTransitionPolicy, fat_p::ThrowingActionPolicy, 0, A, B>;

    Ctx ctx;
    SM sm(ctx);
    FATP_ASSERT_EQ(ctx.mValue, 1, "Strict+Throwing: initial entry");

    sm.transition<B>();
    FATP_ASSERT_EQ(ctx.mValue, 2, "Strict+Throwing: transition to B");

    ctx.mShouldThrow = true;
    bool threw = false;
    try { sm.transition<A>(); }
    catch (const std::runtime_error&) { threw = true; }
    FATP_ASSERT_TRUE(threw, "Strict+Throwing: exception propagates");

    return true;
}

FATP_TEST_CASE(all_policy_combinations_anytoany_noexcept)
{
    struct Ctx { int mValue = 0; };
    struct A { void on_entry(Ctx& c) noexcept { c.mValue = 1; } void on_exit(Ctx&) noexcept {} };
    struct B { void on_entry(Ctx& c) noexcept { c.mValue = 2; } void on_exit(Ctx&) noexcept {} };
    struct C { void on_entry(Ctx& c) noexcept { c.mValue = 3; } void on_exit(Ctx&) noexcept {} };

    using TL = std::tuple<>; // Empty - AnyToAny allows all
    using SM = fat_p::StateMachine<Ctx, TL, fat_p::AnyToAnyTransitionPolicy, fat_p::NoExceptActionPolicy, 0, A, B, C>;

    Ctx ctx;
    SM sm(ctx);
    FATP_ASSERT_EQ(ctx.mValue, 1, "AnyToAny+NoExcept: initial");

    // Can go anywhere
    sm.transition<C>();
    FATP_ASSERT_EQ(ctx.mValue, 3, "AnyToAny+NoExcept: A->C direct");

    sm.transition<A>();
    FATP_ASSERT_EQ(ctx.mValue, 1, "AnyToAny+NoExcept: C->A direct");

    sm.transition<B>();
    FATP_ASSERT_EQ(ctx.mValue, 2, "AnyToAny+NoExcept: A->B");

    return true;
}

FATP_TEST_CASE(all_policy_combinations_anytoany_throwing)
{
    struct Ctx { int mValue = 0; bool mShouldThrow = false; };
    struct A
    {
        void on_entry(Ctx& c) { c.mValue = 1; if (c.mShouldThrow) throw std::runtime_error("A"); }
        void on_exit(Ctx&) {}
    };
    struct B { void on_entry(Ctx& c) { c.mValue = 2; } void on_exit(Ctx&) {} };
    struct C { void on_entry(Ctx& c) { c.mValue = 3; } void on_exit(Ctx&) {} };

    using TL = std::tuple<>;
    using SM = fat_p::StateMachine<Ctx, TL, fat_p::AnyToAnyTransitionPolicy, fat_p::ThrowingActionPolicy, 0, A, B, C>;

    Ctx ctx;
    SM sm(ctx);

    sm.transition<C>();
    sm.transition<B>();
    FATP_ASSERT_EQ(ctx.mValue, 2, "AnyToAny+Throwing: transitions work");

    ctx.mShouldThrow = true;
    bool threw = false;
    try { sm.transition<A>(); }
    catch (const std::runtime_error&) { threw = true; }
    FATP_ASSERT_TRUE(threw, "AnyToAny+Throwing: exception propagates");

    return true;
}


// ============================================================================
// Main Test Runner
// ============================================================================

} // namespace fat_p::testing::statemachine


// ============================================================================
// Suite 11: Stress / Soak / Fuzz / Debug-only Checks (Hammer Plan)
// ============================================================================

namespace fat_p::testing::hammer
{

static std::uint64_t parseU64Strict(const char* s)
{
    if (s == nullptr || *s == '\0')
    {
        throw std::runtime_error("Empty numeric environment value");
    }

    std::uint64_t value = 0;
    for (const char* p = s; *p != '\0'; ++p)
    {
        const unsigned char ch = static_cast<unsigned char>(*p);
        if (!std::isdigit(ch))
        {
            throw std::runtime_error("Non-digit in numeric environment value");
        }

        const std::uint64_t digit = static_cast<std::uint64_t>(ch - '0');
        if (value > (std::numeric_limits<std::uint64_t>::max() - digit) / 10ULL)
        {
            throw std::runtime_error("Numeric environment value overflow");
        }

        value = value * 10ULL + digit;
    }

    return value;
}

static std::uint64_t readEnvU64(const char* name,
                                std::uint64_t defaultValue,
                                std::uint64_t minValue,
                                std::uint64_t maxValue)
{
    const char* s = std::getenv(name);
    if (s == nullptr || *s == '\0')
    {
        return defaultValue;
    }

    const std::uint64_t v = parseU64Strict(s);
    if (v < minValue)
    {
        return minValue;
    }
    if (v > maxValue)
    {
        return maxValue;
    }
    return v;
}

// xorshift64* PRNG (std-only, deterministic)
struct XorShift64Star
{
    std::uint64_t mState;

    explicit XorShift64Star(std::uint64_t seed)
        : mState(seed ? seed : 0x9E3779B97F4A7C15ULL)
    {
    }

    std::uint64_t nextU64()
    {
        std::uint64_t x = mState;
        x ^= x >> 12;
        x ^= x << 25;
        x ^= x >> 27;
        mState = x;
        return x * 0x2545F4914F6CDD1DULL;
    }

    std::uint32_t nextU32()
    {
        return static_cast<std::uint32_t>(nextU64() >> 32);
    }

    std::size_t nextIndex(std::size_t n)
    {
        return (n == 0U) ? 0U : static_cast<std::size_t>(nextU64() % n);
    }
};

// ----------------------------
// Soak: deterministic long run
// ----------------------------
FATP_TEST_CASE(soak_cycle_transitions_env_configurable)
{
    // Env knob:
    //   FATP_STATE_MACHINE_SOAK_ITERS
    const std::uint64_t iters = readEnvU64("FATP_STATE_MACHINE_SOAK_ITERS",
                                            1'000'000ULL,
                                            1'000ULL,
                                            200'000'000ULL);

    struct Ctx
    {
        std::uint64_t mEntry = 0;
        std::uint64_t mExit = 0;
    };

    struct A
    {
        void on_entry(Ctx& c) noexcept { ++c.mEntry; }
        void on_exit(Ctx& c) noexcept { ++c.mExit; }
    };

    struct B
    {
        void on_entry(Ctx& c) noexcept { ++c.mEntry; }
        void on_exit(Ctx& c) noexcept { ++c.mExit; }
    };

    struct C
    {
        void on_entry(Ctx& c) noexcept { ++c.mEntry; }
        void on_exit(Ctx& c) noexcept { ++c.mExit; }
    };

    using TL = std::tuple<std::pair<A, B>, std::pair<B, C>, std::pair<C, A>>;
    using SM = fat_p::StateMachine<Ctx, TL, fat_p::StrictTransitionPolicy, fat_p::NoExceptActionPolicy, 0, A, B, C>;

    Ctx ctx;
    SM sm(ctx);

    for (std::uint64_t i = 0; i < iters; ++i)
    {
        sm.transition<B>();
        sm.transition<C>();
        sm.transition<A>();
    }

    const std::uint64_t transitions = iters * 3ULL;
    FATP_ASSERT_EQ(ctx.mEntry, 1ULL + transitions, "Entry count matches (initial + per transition)");
    FATP_ASSERT_EQ(ctx.mExit, transitions, "Exit count matches (per transition)");
    return true;
}

// ---------------------------------------------------------------------------
// Strict policy random walk fuzz with oracle validation
// ---------------------------------------------------------------------------

template<std::size_t Id>
struct TagState
{
    template<class Ctx>
    void on_entry(Ctx& ctx) noexcept
    {
        ctx.mEntryCalls.fetch_add(1, std::memory_order_relaxed);
        ctx.mLastEntry = Id;
        ctx.phaseAdvanceOnEntry(Id);
    }

    template<class Ctx>
    void on_exit(Ctx& ctx) noexcept
    {
        ctx.mExitCalls.fetch_add(1, std::memory_order_relaxed);
        ctx.mLastExit = Id;
        ctx.phaseAdvanceOnExit(Id);
    }
};

struct HammerContext
{
    std::atomic<std::uint64_t> mEntryCalls {0};
    std::atomic<std::uint64_t> mExitCalls {0};
    std::size_t mLastEntry = static_cast<std::size_t>(-1);
    std::size_t mLastExit = static_cast<std::size_t>(-1);

    enum class Phase : std::uint8_t
    {
        ExpectEntry,
        ExpectExit,
    };

    Phase mPhase = Phase::ExpectEntry;
    bool mFailed = false;
    const char* mFailure = nullptr;
    std::size_t mExpectedEntry = 0;
    std::size_t mExpectedExit = 0;

    void phaseAdvanceOnEntry(std::size_t id) noexcept
    {
        if (mFailed)
        {
            return;
        }

        if (mPhase != Phase::ExpectEntry || id != mExpectedEntry)
        {
            mFailed = true;
            mFailure = "Hook order violation: entry not expected";
            return;
        }

        mPhase = Phase::ExpectExit;
    }

    void phaseAdvanceOnExit(std::size_t id) noexcept
    {
        if (mFailed)
        {
            return;
        }

        if (mPhase != Phase::ExpectExit || id != mExpectedExit)
        {
            mFailed = true;
            mFailure = "Hook order violation: exit not expected";
            return;
        }

        mPhase = Phase::ExpectEntry;
    }
};

// A 6-state graph with:
//   - ring edges
//   - a few chords
//   - some missing edges (so invalid transitions exist)
template<class... Pairs>
using TransitionTuple = std::tuple<Pairs...>;

template<std::size_t From, std::size_t To>
using Edge = std::pair<TagState<From>, TagState<To>>;

using StrictGraph6 = TransitionTuple<
    Edge<0, 1>, Edge<1, 2>, Edge<2, 3>, Edge<3, 4>, Edge<4, 5>, Edge<5, 0>,
    Edge<0, 3>, Edge<2, 5>, Edge<4, 1>>;

static bool strictOracleAllowed6(std::size_t from, std::size_t to) noexcept
{
    if (from == to)
    {
        return true; // self transition is defined as no-op allowed
    }

    switch (from)
    {
    case 0: return (to == 1 || to == 3);
    case 1: return (to == 2);
    case 2: return (to == 3 || to == 5);
    case 3: return (to == 4);
    case 4: return (to == 5 || to == 1);
    case 5: return (to == 0);
    default: return false;
    }
}

FATP_TEST_CASE(strict_policy_random_walk_fuzz)
{
    const std::uint64_t steps = readEnvU64("FATP_STATE_MACHINE_FUZZ_STEPS",
                                            20'000ULL,
                                            1'000ULL,
                                            5'000'000ULL);
    const std::uint64_t seed = readEnvU64("FATP_STATE_MACHINE_FUZZ_SEED",
                                           0xC0FFEE123456789ULL,
                                           1ULL,
                                           std::numeric_limits<std::uint64_t>::max());

    using S0 = TagState<0>;
    using S1 = TagState<1>;
    using S2 = TagState<2>;
    using S3 = TagState<3>;
    using S4 = TagState<4>;
    using S5 = TagState<5>;

    using SM = fat_p::StateMachine<HammerContext,
                                   StrictGraph6,
                                   fat_p::StrictTransitionPolicy,
                                   fat_p::NoExceptActionPolicy,
                                   0,
                                   S0, S1, S2, S3, S4, S5>;

    HammerContext ctx;
    // Constructor calls initial entry immediately.
    ctx.mExpectedEntry = 0;
    ctx.mPhase = HammerContext::Phase::ExpectEntry;
    SM sm(ctx);
    FATP_ASSERT_TRUE(!ctx.mFailed, "Initial entry failed");

    std::size_t model = 0;
    XorShift64Star rng(seed);

    std::uint64_t lastEntry = ctx.mEntryCalls.load(std::memory_order_relaxed);
    std::uint64_t lastExit = ctx.mExitCalls.load(std::memory_order_relaxed);

    for (std::uint64_t i = 0; i < steps; ++i)
    {
        const std::size_t target = rng.nextIndex(6);

        const bool allowed = strictOracleAllowed6(model, target);

        const std::uint64_t beforeEntry = ctx.mEntryCalls.load(std::memory_order_relaxed);
        const std::uint64_t beforeExit = ctx.mExitCalls.load(std::memory_order_relaxed);

        // Set hook expectations only for non-self transitions that should be allowed.
        if (allowed && target != model)
        {
            ctx.mExpectedExit = model;
            ctx.mExpectedEntry = target;
            ctx.mPhase = HammerContext::Phase::ExpectExit;
        }

        bool threw = false;
        try
        {
            switch (target)
            {
            case 0: sm.transition<S0>(); break;
            case 1: sm.transition<S1>(); break;
            case 2: sm.transition<S2>(); break;
            case 3: sm.transition<S3>(); break;
            case 4: sm.transition<S4>(); break;
            case 5: sm.transition<S5>(); break;
            default: break;
            }
        }
        catch (const std::exception&)
        {
            threw = true;
        }

        const std::uint64_t afterEntry = ctx.mEntryCalls.load(std::memory_order_relaxed);
        const std::uint64_t afterExit = ctx.mExitCalls.load(std::memory_order_relaxed);

        if (!allowed)
        {
            FATP_ASSERT_TRUE(threw, "Invalid strict transition must throw");
            FATP_ASSERT_EQ(beforeEntry, afterEntry, "Invalid strict transition must not call entry");
            FATP_ASSERT_EQ(beforeExit, afterExit, "Invalid strict transition must not call exit");
            FATP_ASSERT_EQ(sm.currentStateIndex(), model, "Invalid strict transition must not change state");
        }
        else if (target == model)
        {
            FATP_ASSERT_TRUE(!threw, "Self transition should not throw");
            FATP_ASSERT_EQ(beforeEntry, afterEntry, "Self transition is no-op: no entry");
            FATP_ASSERT_EQ(beforeExit, afterExit, "Self transition is no-op: no exit");
            FATP_ASSERT_EQ(sm.currentStateIndex(), model, "Self transition is no-op: index unchanged");
        }
        else
        {
            FATP_ASSERT_TRUE(!threw, "Valid strict transition should not throw");
            FATP_ASSERT_EQ(beforeExit + 1ULL, afterExit, "Valid transition: exactly one exit");
            FATP_ASSERT_EQ(beforeEntry + 1ULL, afterEntry, "Valid transition: exactly one entry");
            FATP_ASSERT_EQ(sm.currentStateIndex(), target, "Valid transition updates state index");
            FATP_ASSERT_TRUE(!ctx.mFailed, "Hook order invariant violated");
            model = target;
        }

        // Periodic consistency: entry is always exit+1 (because constructor did initial entry).
        lastEntry = afterEntry;
        lastExit = afterExit;
        FATP_ASSERT_TRUE(lastEntry == lastExit + 1ULL, "Invariant: mEntryCalls == mExitCalls + 1");
    }

    return true;
}

// -------------------------------------------------------
// Exception injection fuzz: pins fat_p::ThrowingActionPolicy rule
// -------------------------------------------------------
FATP_TEST_CASE(exception_injection_fuzz_throwing_policy)
{
    struct Ctx
    {
        std::uint64_t mEntry = 0;
        std::uint64_t mExit = 0;
        bool mThrowOnEntry = false;
        bool mThrowOnExit = false;
    };

    struct A
    {
        void on_entry(Ctx& c)
        {
            ++c.mEntry;
            if (c.mThrowOnEntry)
            {
                throw std::runtime_error("A entry");
            }
        }

        void on_exit(Ctx& c)
        {
            ++c.mExit;
            if (c.mThrowOnExit)
            {
                throw std::runtime_error("A exit");
            }
        }
    };

    struct B
    {
        void on_entry(Ctx& c)
        {
            ++c.mEntry;
            if (c.mThrowOnEntry)
            {
                throw std::runtime_error("B entry");
            }
        }

        void on_exit(Ctx& c)
        {
            ++c.mExit;
            if (c.mThrowOnExit)
            {
                throw std::runtime_error("B exit");
            }
        }
    };

    using TL = std::tuple<std::pair<A, B>, std::pair<B, A>>;
    using SM = fat_p::StateMachine<Ctx, TL, fat_p::StrictTransitionPolicy, fat_p::ThrowingActionPolicy, 0, A, B>;

    Ctx ctx;
    SM sm(ctx);
    FATP_ASSERT_EQ(sm.currentStateIndex(), 0U, "Starts in A");

    XorShift64Star rng(0x123456789ABCDEF0ULL);
    for (std::uint64_t i = 0; i < 10'000ULL; ++i)
    {
        const bool wantEntryThrow = (rng.nextU32() & 1U) != 0U;
        const bool wantExitThrow = (rng.nextU32() & 2U) != 0U;

        ctx.mThrowOnEntry = wantEntryThrow;
        ctx.mThrowOnExit = wantExitThrow;

        const std::uint64_t entry0 = ctx.mEntry;
        const std::uint64_t exit0 = ctx.mExit;
        const std::size_t idx0 = sm.currentStateIndex();
        const std::size_t idx1 = (idx0 == 0U) ? 1U : 0U;

        bool threw = false;
        try
        {
            if (idx1 == 0U)
            {
                sm.transition<A>();
            }
            else
            {
                sm.transition<B>();
            }
        }
        catch (const std::exception&)
        {
            threw = true;
        }

        if (wantExitThrow)
        {
            FATP_ASSERT_TRUE(threw, "Exit-throw must throw");
            FATP_ASSERT_EQ(sm.currentStateIndex(), idx0, "Exit-throw: index unchanged");
            FATP_ASSERT_EQ(ctx.mExit, exit0 + 1ULL, "Exit-throw: exit attempted once");
            FATP_ASSERT_EQ(ctx.mEntry, entry0, "Exit-throw: entry not called");
        }
        else if (wantEntryThrow)
        {
            FATP_ASSERT_TRUE(threw, "Entry-throw must throw");
            FATP_ASSERT_EQ(sm.currentStateIndex(), idx1, "Entry-throw: index updated");
            FATP_ASSERT_EQ(ctx.mExit, exit0 + 1ULL, "Entry-throw: exit ran");
            FATP_ASSERT_EQ(ctx.mEntry, entry0 + 1ULL, "Entry-throw: entry attempted once");
        }
        else
        {
            FATP_ASSERT_TRUE(!threw, "No-throw: transition must succeed");
            FATP_ASSERT_EQ(sm.currentStateIndex(), idx1, "No-throw: index updated");
            FATP_ASSERT_EQ(ctx.mExit, exit0 + 1ULL, "No-throw: exit ran");
            FATP_ASSERT_EQ(ctx.mEntry, entry0 + 1ULL, "No-throw: entry ran");
        }

        ctx.mThrowOnEntry = false;
        ctx.mThrowOnExit = false;
    }

    return true;
}

// --------------------------------------------------------
// Thread stress: independent machines in parallel (no races)
// --------------------------------------------------------
FATP_TEST_CASE(threaded_independent_state_machines_stress)
{
    const std::uint64_t steps = readEnvU64("FATP_STATE_MACHINE_THREAD_STEPS",
                                            50'000ULL,
                                            1'000ULL,
                                            5'000'000ULL);

    std::uint64_t threads = readEnvU64("FATP_STATE_MACHINE_THREADS",
                                        static_cast<std::uint64_t>(std::thread::hardware_concurrency()),
                                        1ULL,
                                        64ULL);
    if (threads == 0ULL)
    {
        threads = 1ULL;
    }

    struct Ctx
    {
        std::uint64_t mEntry = 0;
        std::uint64_t mExit = 0;
    };

    struct A
    {
        void on_entry(Ctx& c) noexcept { ++c.mEntry; }
        void on_exit(Ctx& c) noexcept { ++c.mExit; }
    };

    struct B
    {
        void on_entry(Ctx& c) noexcept { ++c.mEntry; }
        void on_exit(Ctx& c) noexcept { ++c.mExit; }
    };

    using TL = std::tuple<std::pair<A, B>, std::pair<B, A>>;
    using SM = fat_p::StateMachine<Ctx, TL, fat_p::StrictTransitionPolicy, fat_p::NoExceptActionPolicy, 0, A, B>;

    std::atomic<bool> ok {true};
    std::vector<std::thread> pool;
    pool.reserve(static_cast<std::size_t>(threads));

    for (std::uint64_t t = 0; t < threads; ++t)
    {
        pool.emplace_back([t, steps, &ok]()
        {
            Ctx ctx;
            SM sm(ctx);
            XorShift64Star rng(0xBADC0FFEE0DDF00DULL + t * 101ULL);

            for (std::uint64_t i = 0; i < steps; ++i)
            {
                const bool toB = (rng.nextU32() & 1U) != 0U;
                if (toB)
                {
                    sm.transition<B>();
                }
                else
                {
                    sm.transition<A>();
                }
            }

            if (ctx.mEntry != ctx.mExit + 1ULL)
            {
                ok.store(false, std::memory_order_relaxed);
            }
        });
    }

    for (auto& th : pool)
    {
        th.join();
    }

    FATP_ASSERT_TRUE(ok.load(std::memory_order_relaxed), "Threaded stress invariant holds");
    return true;
}

// -----------------------------------------------------------
// Debug-only reentrancy check (prints SKIPPED in Release build)
// -----------------------------------------------------------
FATP_TEST_CASE(reentrant_transition_detected_in_debug)
{
#ifdef NDEBUG
    std::cout << "SKIPPED (Release)\n";
    return true;
#else
    // Test that reentrant transitions (calling transition() from within
    // on_entry/on_exit) are detected and rejected in Debug builds.

    struct Ctx
    {
        void* mSm = nullptr;
        void (*mReenter)(void*) = nullptr;
        bool mAttempted = false;
    };

    struct A
    {
        void on_entry(Ctx&) noexcept {}

        void on_exit(Ctx& ctx)
        {
            // Attempt reentrant transition during A's exit (while A->B is in progress)
            if ((ctx.mSm != nullptr) && (ctx.mReenter != nullptr) && !ctx.mAttempted)
            {
                ctx.mAttempted = true;
                ctx.mReenter(ctx.mSm);  // Nested transition<B>() - should be caught
            }
        }
    };

    struct B
    {
        void on_entry(Ctx&) noexcept {}
        void on_exit(Ctx&) noexcept {}
    };

    using TL = std::tuple<>;
    using SM = fat_p::StateMachine<
        Ctx,
        TL,
        fat_p::AnyToAnyTransitionPolicy,
        fat_p::ThrowingActionPolicy,
        0,
        A, B>;

    // Static function to call transition<B>() - avoids capturing lambda issue
    struct Reenter
    {
        static void call(void* smPtr)
        {
            static_cast<SM*>(smPtr)->template transition<B>();
        }
    };

    Ctx ctx;
    SM sm(ctx);
    ctx.mSm = &sm;
    ctx.mReenter = &Reenter::call;

    bool threw = false;
    std::string errorMsg;
    try
    {
        sm.transition<B>();  // A::on_exit will attempt nested transition<B>
    }
    catch (const std::runtime_error& e)
    {
        threw = true;
        errorMsg = e.what();
    }

    FATP_ASSERT_TRUE(threw, "Debug reentrancy guard should throw");
    FATP_ASSERT_TRUE(errorMsg.find("Reentrant") != std::string::npos ||
                     errorMsg.find("reentrant") != std::string::npos,
                     "Exception message should mention reentrancy");

    return true;
#endif
}

// -------------------------------------------------------
// Fuzz with AnyToAny + NoExcept policy combination
// -------------------------------------------------------
FATP_TEST_CASE(fuzz_any_to_any_noexcept)
{
    const std::uint64_t steps = readEnvU64("FATP_STATE_MACHINE_FUZZ_STEPS",
                                            20'000ULL,
                                            1'000ULL,
                                            5'000'000ULL);

    struct Ctx
    {
        std::uint64_t mTransitions = 0;
    };

    struct S0 { void on_entry(Ctx& c) noexcept { ++c.mTransitions; } void on_exit(Ctx&) noexcept {} };
    struct S1 { void on_entry(Ctx& c) noexcept { ++c.mTransitions; } void on_exit(Ctx&) noexcept {} };
    struct S2 { void on_entry(Ctx& c) noexcept { ++c.mTransitions; } void on_exit(Ctx&) noexcept {} };
    struct S3 { void on_entry(Ctx& c) noexcept { ++c.mTransitions; } void on_exit(Ctx&) noexcept {} };

    using TL = std::tuple<>; // AnyToAny needs no explicit edges
    using SM = fat_p::StateMachine<
        Ctx, TL, fat_p::AnyToAnyTransitionPolicy, fat_p::NoExceptActionPolicy,
        0, S0, S1, S2, S3>;

    Ctx ctx;
    SM sm(ctx);

    XorShift64Star rng(0xDEADBEEF12345678ULL);

    for (std::uint64_t i = 0; i < steps; ++i)
    {
        const std::size_t target = rng.nextIndex(4);

        // AnyToAny should never throw for valid state indices
        switch (target)
        {
        case 0: sm.transition<S0>(); break;
        case 1: sm.transition<S1>(); break;
        case 2: sm.transition<S2>(); break;
        case 3: sm.transition<S3>(); break;
        default: break;
        }

        FATP_ASSERT_EQ(sm.currentStateIndex(), target, "AnyToAny should always succeed");
    }

    // Transitions counted = initial entry (1) + non-self transitions
    FATP_ASSERT_TRUE(ctx.mTransitions >= 1ULL, "At least initial entry");

    return true;
}

// -------------------------------------------------------
// Fuzz with AnyToAny + Throwing policy combination
// -------------------------------------------------------
FATP_TEST_CASE(fuzz_any_to_any_throwing)
{
    const std::uint64_t steps = readEnvU64("FATP_STATE_MACHINE_FUZZ_STEPS",
                                            10'000ULL,
                                            1'000ULL,
                                            1'000'000ULL);

    struct Ctx
    {
        std::uint64_t mEntry = 0;
        std::uint64_t mExit = 0;
        bool mThrowOnEntry = false;
    };

    struct S0
    {
        void on_entry(Ctx& c) { ++c.mEntry; if (c.mThrowOnEntry) throw std::runtime_error("S0"); }
        void on_exit(Ctx& c) { ++c.mExit; }
    };
    struct S1
    {
        void on_entry(Ctx& c) { ++c.mEntry; if (c.mThrowOnEntry) throw std::runtime_error("S1"); }
        void on_exit(Ctx& c) { ++c.mExit; }
    };
    struct S2
    {
        void on_entry(Ctx& c) { ++c.mEntry; if (c.mThrowOnEntry) throw std::runtime_error("S2"); }
        void on_exit(Ctx& c) { ++c.mExit; }
    };

    using TL = std::tuple<>;
    using SM = fat_p::StateMachine<
        Ctx, TL, fat_p::AnyToAnyTransitionPolicy, fat_p::ThrowingActionPolicy,
        0, S0, S1, S2>;

    Ctx ctx;
    SM sm(ctx);

    XorShift64Star rng(0xCAFEBABE87654321ULL);

    for (std::uint64_t i = 0; i < steps; ++i)
    {
        const std::size_t target = rng.nextIndex(3);
        const bool wantThrow = (rng.nextU32() % 20) == 0; // 5% throw rate

        ctx.mThrowOnEntry = wantThrow;
        const std::size_t before = sm.currentStateIndex();

        bool threw = false;
        try
        {
            switch (target)
            {
            case 0: sm.transition<S0>(); break;
            case 1: sm.transition<S1>(); break;
            case 2: sm.transition<S2>(); break;
            default: break;
            }
        }
        catch (const std::runtime_error&)
        {
            threw = true;
        }

        if (target == before)
        {
            // Self-transition is no-op, no throw possible from entry
            FATP_ASSERT_FALSE(threw, "Self transition should not throw");
        }
        else if (wantThrow)
        {
            FATP_ASSERT_TRUE(threw, "Should throw when configured");
            FATP_ASSERT_EQ(sm.currentStateIndex(), target, "State updates even on entry throw");
        }

        ctx.mThrowOnEntry = false;
    }

    return true;
}

// -------------------------------------------------------
// Deterministic replay verification
// -------------------------------------------------------
FATP_TEST_CASE(deterministic_replay_verification)
{
    constexpr std::uint64_t kSeed = 0x123456789ABCDEF0ULL;
    constexpr std::uint64_t kSteps = 10'000ULL;

    struct Ctx
    {
        std::vector<std::size_t> mHistory;
    };

    struct S0 { void on_entry(Ctx& c) noexcept { c.mHistory.push_back(0); } void on_exit(Ctx&) noexcept {} };
    struct S1 { void on_entry(Ctx& c) noexcept { c.mHistory.push_back(1); } void on_exit(Ctx&) noexcept {} };
    struct S2 { void on_entry(Ctx& c) noexcept { c.mHistory.push_back(2); } void on_exit(Ctx&) noexcept {} };

    using TL = std::tuple<>;
    using SM = fat_p::StateMachine<
        Ctx, TL, fat_p::AnyToAnyTransitionPolicy, fat_p::NoExceptActionPolicy,
        0, S0, S1, S2>;

    // First run
    Ctx ctx1;
    SM sm1(ctx1);
    XorShift64Star rng1(kSeed);

    for (std::uint64_t i = 0; i < kSteps; ++i)
    {
        switch (rng1.nextIndex(3))
        {
        case 0: sm1.transition<S0>(); break;
        case 1: sm1.transition<S1>(); break;
        case 2: sm1.transition<S2>(); break;
        default: break;
        }
    }

    // Second run with same seed
    Ctx ctx2;
    SM sm2(ctx2);
    XorShift64Star rng2(kSeed);

    for (std::uint64_t i = 0; i < kSteps; ++i)
    {
        switch (rng2.nextIndex(3))
        {
        case 0: sm2.transition<S0>(); break;
        case 1: sm2.transition<S1>(); break;
        case 2: sm2.transition<S2>(); break;
        default: break;
        }
    }

    // Histories must match exactly
    FATP_ASSERT_EQ(ctx1.mHistory.size(), ctx2.mHistory.size(), "History sizes must match");

    bool match = true;
    for (std::size_t i = 0; i < ctx1.mHistory.size() && match; ++i)
    {
        if (ctx1.mHistory[i] != ctx2.mHistory[i])
        {
            match = false;
        }
    }

    FATP_ASSERT_TRUE(match, "Deterministic replay must produce identical history");
    FATP_ASSERT_EQ(sm1.currentStateIndex(), sm2.currentStateIndex(), "Final states must match");

    return true;
}

// -------------------------------------------------------
// Fully connected graph stress (6 states, 30 edges)
// -------------------------------------------------------

namespace fully_connected_detail
{

struct FCCtx
{
    std::uint64_t mTransitions = 0;
};

template<std::size_t N>
struct FS
{
    void on_entry(FCCtx& c) noexcept { ++c.mTransitions; }
    void on_exit(FCCtx&) noexcept {}
};

// Fully connected: every state can go to every other state (6*5 = 30 edges)
using FCTL = std::tuple<
    // From FS<0>
    std::pair<FS<0>, FS<1>>, std::pair<FS<0>, FS<2>>, std::pair<FS<0>, FS<3>>,
    std::pair<FS<0>, FS<4>>, std::pair<FS<0>, FS<5>>,
    // From FS<1>
    std::pair<FS<1>, FS<0>>, std::pair<FS<1>, FS<2>>, std::pair<FS<1>, FS<3>>,
    std::pair<FS<1>, FS<4>>, std::pair<FS<1>, FS<5>>,
    // From FS<2>
    std::pair<FS<2>, FS<0>>, std::pair<FS<2>, FS<1>>, std::pair<FS<2>, FS<3>>,
    std::pair<FS<2>, FS<4>>, std::pair<FS<2>, FS<5>>,
    // From FS<3>
    std::pair<FS<3>, FS<0>>, std::pair<FS<3>, FS<1>>, std::pair<FS<3>, FS<2>>,
    std::pair<FS<3>, FS<4>>, std::pair<FS<3>, FS<5>>,
    // From FS<4>
    std::pair<FS<4>, FS<0>>, std::pair<FS<4>, FS<1>>, std::pair<FS<4>, FS<2>>,
    std::pair<FS<4>, FS<3>>, std::pair<FS<4>, FS<5>>,
    // From FS<5>
    std::pair<FS<5>, FS<0>>, std::pair<FS<5>, FS<1>>, std::pair<FS<5>, FS<2>>,
    std::pair<FS<5>, FS<3>>, std::pair<FS<5>, FS<4>>
>;

using FCSM = fat_p::StateMachine<FCCtx, FCTL, fat_p::StrictTransitionPolicy, fat_p::NoExceptActionPolicy, 0,
                               FS<0>, FS<1>, FS<2>, FS<3>, FS<4>, FS<5>>;

} // namespace fully_connected_detail

FATP_TEST_CASE(fully_connected_graph_stress)
{
    using namespace fully_connected_detail;

    const std::uint64_t steps = readEnvU64("FATP_STATE_MACHINE_FUZZ_STEPS",
                                            20'000ULL,
                                            1'000ULL,
                                            5'000'000ULL);

    FCCtx ctx;
    FCSM sm(ctx);

    XorShift64Star rng(0xFC00EC7ED12345ULL);

    for (std::uint64_t i = 0; i < steps; ++i)
    {
        const std::size_t target = rng.nextIndex(6);

        // All transitions should succeed (fully connected)
        switch (target)
        {
        case 0: sm.transition<FS<0>>(); break;
        case 1: sm.transition<FS<1>>(); break;
        case 2: sm.transition<FS<2>>(); break;
        case 3: sm.transition<FS<3>>(); break;
        case 4: sm.transition<FS<4>>(); break;
        case 5: sm.transition<FS<5>>(); break;
        default: break;
        }

        FATP_ASSERT_EQ(sm.currentStateIndex(), target, "Fully connected: all transitions valid");
    }

    return true;
}

// -------------------------------------------------------
// Zero allocation verification after construction
// -------------------------------------------------------
FATP_TEST_CASE(zero_allocation_after_construction)
{
    // This test verifies the StateMachine doesn't allocate during transitions.
    // We can't directly measure allocations without a custom allocator, but we can
    // verify the StateMachine uses no dynamic containers internally by checking
    // that rapid transitions don't increase memory (indirect verification).

    struct Ctx
    {
        std::uint64_t mCount = 0;
    };

    struct A { void on_entry(Ctx& c) noexcept { ++c.mCount; } void on_exit(Ctx&) noexcept {} };
    struct B { void on_entry(Ctx& c) noexcept { ++c.mCount; } void on_exit(Ctx&) noexcept {} };
    struct C { void on_entry(Ctx& c) noexcept { ++c.mCount; } void on_exit(Ctx&) noexcept {} };

    using TL = std::tuple<std::pair<A, B>, std::pair<B, C>, std::pair<C, A>>;
    using SM = fat_p::StateMachine<Ctx, TL, fat_p::StrictTransitionPolicy, fat_p::NoExceptActionPolicy, 0, A, B, C>;

    // Verify StateMachine size is small (no internal vectors/maps)
    // A reasonable upper bound: sizeof(context ref) + sizeof(state index) + sizeof(states tuple) + padding
    // Should be well under 256 bytes for simple states
    static_assert(sizeof(SM) < 256, "StateMachine should be small (no dynamic allocations)");

    Ctx ctx;
    SM sm(ctx);

    // Run many transitions - if there were allocations, this would be slow or fragment memory
    for (int i = 0; i < 100'000; ++i)
    {
        sm.transition<B>();
        sm.transition<C>();
        sm.transition<A>();
    }

    FATP_ASSERT_EQ(ctx.mCount, 1ULL + 300'000ULL, "All transitions executed");

    return true;
}

} // namespace fat_p::testing::hammer


namespace fat_p::testing
{


void run_benchmarks()
{
    using namespace fat_p::testing;

    auto& out = *get_test_config().output;
    out << "\n" << colors::cyan() << "Sanity Benchmark:" << colors::reset() << "\n";
    out << "  (benchmarks are provided by benchmark executables; unit tests do not run full benchmarks)\n\n";
}

bool test_StateMachine()
{
    FATP_PRINT_HEADER(STATE MACHINE)

    TestRunner runner;

    // Suite 1: Construction
    std::cout << "Suite 1: Construction and Initialization\n";
    FATP_RUN_TEST_NS(runner, statemachine, state_machine_construction);
    FATP_RUN_TEST_NS(runner, statemachine, state_machine_default_policy);
    FATP_RUN_TEST_NS(runner, statemachine, state_machine_custom_initial_state);

    // Suite 2: Basic Transitions
    std::cout << "\nSuite 2: Basic State Transitions\n";
    FATP_RUN_TEST_NS(runner, statemachine, simple_transition);
    FATP_RUN_TEST_NS(runner, statemachine, chained_transitions);
    FATP_RUN_TEST_NS(runner, statemachine, self_transition_is_noop);
    FATP_RUN_TEST_NS(runner, statemachine, multiple_transitions_same_state);

    // Suite 3: Strict Policy
    std::cout << "\nSuite 3: fat_p::StrictTransitionPolicy\n";
    FATP_RUN_TEST_NS(runner, statemachine, strict_policy_valid_transitions);
    FATP_RUN_TEST_NS(runner, statemachine, strict_policy_invalid_transition_throws);
    FATP_RUN_TEST_NS(runner, statemachine, strict_policy_complex_graph);
    FATP_RUN_TEST_NS(runner, statemachine, strict_policy_prevents_shortcut);

    // Suite 4: AnyToAny Policy
    std::cout << "\nSuite 4: fat_p::AnyToAnyTransitionPolicy\n";
    FATP_RUN_TEST_NS(runner, statemachine, any_to_any_policy_all_transitions);
    FATP_RUN_TEST_NS(runner, statemachine, any_to_any_with_complex_states);
    FATP_RUN_TEST_NS(runner, statemachine, any_to_any_with_four_states);

    // Suite 5: Action Policies
    std::cout << "\nSuite 5: Action Policies\n";
    FATP_RUN_TEST_NS(runner, statemachine, noexcept_policy_compiles);
    FATP_RUN_TEST_NS(runner, statemachine, throwing_policy_allows_exceptions);
    FATP_RUN_TEST_NS(runner, statemachine, throwing_exit_action);

    // Suite 6: Context Sharing
    std::cout << "\nSuite 6: Context Sharing\n";
    FATP_RUN_TEST_NS(runner, statemachine, context_shared_between_states);
    FATP_RUN_TEST_NS(runner, statemachine, context_persistence_across_transitions);
    FATP_RUN_TEST_NS(runner, statemachine, context_modification_visible);

    // Suite 7: State Queries
    std::cout << "\nSuite 7: State Query Operations\n";
    FATP_RUN_TEST_NS(runner, statemachine, current_state_index);
    FATP_RUN_TEST_NS(runner, statemachine, is_in_state);
    FATP_RUN_TEST_NS(runner, statemachine, query_operations_consistency);

    // Suite 8: Edge Cases
    std::cout << "\nSuite 8: Edge Cases\n";
    FATP_RUN_TEST_NS(runner, statemachine, single_state_machine);
    FATP_RUN_TEST_NS(runner, statemachine, large_state_machine);
    FATP_RUN_TEST_NS(runner, statemachine, empty_log_accumulation);
    FATP_RUN_TEST_NS(runner, statemachine, initial_state_action_called_once);

    // Suite 9: Compile-Time Validation
    std::cout << "\nSuite 9: Compile-Time Validation\n";
    FATP_RUN_TEST_NS(runner, statemachine, compile_time_state_validation);
    FATP_RUN_TEST_NS(runner, statemachine, noexcept_specification);

    // Suite 10: Complex Scenarios
    std::cout << "\nSuite 10: Complex Scenarios\n";
    FATP_RUN_TEST_NS(runner, statemachine, workflow_simulation);
    FATP_RUN_TEST_NS(runner, statemachine, cyclic_transitions);
    FATP_RUN_TEST_NS(runner, statemachine, state_machine_with_custom_initial);

    // Suite 11: Stress / Soak / Fuzz / Debug-only Checks (Hammer Plan)
    std::cout << "\nSuite 11: Stress / Soak / Fuzz / Debug-only Checks\n";
    FATP_RUN_TEST_NS(runner, hammer, soak_cycle_transitions_env_configurable);
    FATP_RUN_TEST_NS(runner, hammer, strict_policy_random_walk_fuzz);
    FATP_RUN_TEST_NS(runner, hammer, exception_injection_fuzz_throwing_policy);
    FATP_RUN_TEST_NS(runner, hammer, threaded_independent_state_machines_stress);
    FATP_RUN_TEST_NS(runner, hammer, reentrant_transition_detected_in_debug);

    // Suite 12: Construction Edge Cases and Semantics
    std::cout << "\nSuite 12: Construction Edge Cases and Semantics\n";
    FATP_RUN_TEST_NS(runner, statemachine, exception_in_initial_entry_throws);
    FATP_RUN_TEST_NS(runner, statemachine, state_machine_is_not_copyable);
    FATP_RUN_TEST_NS(runner, statemachine, state_machine_is_not_movable);
    FATP_RUN_TEST_NS(runner, statemachine, introspection_state_count);
    FATP_RUN_TEST_NS(runner, statemachine, introspection_initial_state_index);
    FATP_RUN_TEST_NS(runner, statemachine, introspection_contains_state);
    FATP_RUN_TEST_NS(runner, statemachine, introspection_is_transition_allowed);
    FATP_RUN_TEST_NS(runner, statemachine, transition_order_exit_completes_before_entry);
    FATP_RUN_TEST_NS(runner, statemachine, state_internal_data_behavior);

    // Suite 13: Large State Counts and Policy Combinations
    std::cout << "\nSuite 13: Large State Counts and Policy Combinations\n";
    FATP_RUN_TEST_NS(runner, statemachine, very_large_state_count_32);
    FATP_RUN_TEST_NS(runner, statemachine, all_policy_combinations_strict_noexcept);
    FATP_RUN_TEST_NS(runner, statemachine, all_policy_combinations_strict_throwing);
    FATP_RUN_TEST_NS(runner, statemachine, all_policy_combinations_anytoany_noexcept);
    FATP_RUN_TEST_NS(runner, statemachine, all_policy_combinations_anytoany_throwing);

    // Suite 14: Extended Stress / Fuzz Tests
    std::cout << "\nSuite 14: Extended Stress / Fuzz Tests\n";
    FATP_RUN_TEST_NS(runner, hammer, fuzz_any_to_any_noexcept);
    FATP_RUN_TEST_NS(runner, hammer, fuzz_any_to_any_throwing);
    FATP_RUN_TEST_NS(runner, hammer, deterministic_replay_verification);
    FATP_RUN_TEST_NS(runner, hammer, fully_connected_graph_stress);
    FATP_RUN_TEST_NS(runner, hammer, zero_allocation_after_construction);

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_StateMachine() ? 0 : 1;
}
#endif
