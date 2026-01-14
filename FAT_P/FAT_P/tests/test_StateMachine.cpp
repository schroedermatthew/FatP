/**
 * @file test_StateMachine.cpp
 * @brief Comprehensive test suite for fat_p::StateMachine
 *
 * This test suite demonstrates all features of StateMachine including:
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
  path: tests/test_StateMachine.cpp
  namespace: fat_p
  summary: "Unit tests for StateMachine."
  related:
    docs_search: "StateMachine"
    headers:
      - fat_p/StateMachine.h
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
#include <sstream>
#include <stdexcept>
#include <string>

#include "FatPTest.h"
#include "StateMachine.h"

using namespace fat_p;
using namespace fat_p::testing;

namespace fat_p::testing::statemachine
{
// ============================================================================
// Test Context and States
// ============================================================================

// Shared context for state machine tests
struct TestContext
{
    int counter = 0;
    std::string log;
    bool flag = false;

    void reset()
    {
        counter = 0;
        log.clear();
        flag = false;
    }
};

// Simple states for basic testing
struct StateA
{
    void on_entry(TestContext& ctx) noexcept
    {
        ctx.log += "A_entry;";
        ctx.counter++;
    }
    void on_exit(TestContext& ctx) noexcept
    {
        ctx.log += "A_exit;";
        ctx.counter++;
    }
};

struct StateB
{
    void on_entry(TestContext& ctx) noexcept
    {
        ctx.log += "B_entry;";
        ctx.counter++;
    }
    void on_exit(TestContext& ctx) noexcept
    {
        ctx.log += "B_exit;";
        ctx.counter++;
    }
};

struct StateC
{
    void on_entry(TestContext& ctx) noexcept
    {
        ctx.log += "C_entry;";
        ctx.counter++;
    }
    void on_exit(TestContext& ctx) noexcept
    {
        ctx.log += "C_exit;";
        ctx.counter++;
    }
};

struct StateD
{
    void on_entry(TestContext& ctx) noexcept
    {
        ctx.log += "D_entry;";
        ctx.counter += 10;
    }
    void on_exit(TestContext& ctx) noexcept
    {
        ctx.log += "D_exit;";
        ctx.counter += 10;
    }
};

// States with data manipulation
struct IdleState
{
    void on_entry(TestContext& ctx) noexcept
    {
        ctx.flag = false;
        ctx.log += "[Idle]";
    }
    void on_exit(TestContext& ctx) noexcept
    {
        ctx.log += "[Leaving_Idle]";
    }
};

struct ProcessingState
{
    void on_entry(TestContext& ctx) noexcept
    {
        ctx.flag = true;
        ctx.counter += 10;
        ctx.log += "[Processing]";
    }
    void on_exit(TestContext& ctx) noexcept
    {
        ctx.log += "[Leaving_Processing]";
    }
};

struct CompletedState
{
    void on_entry(TestContext& ctx) noexcept
    {
        ctx.counter += 100;
        ctx.log += "[Completed]";
    }
    void on_exit(TestContext& ctx) noexcept
    {
        ctx.log += "[Leaving_Completed]";
    }
};

struct ErrorState
{
    void on_entry(TestContext& ctx) noexcept
    {
        ctx.flag = false;
        ctx.counter = -1;
        ctx.log += "[Error]";
    }
    void on_exit(TestContext& ctx) noexcept
    {
        ctx.log += "[Leaving_Error]";
    }
};

// States that throw (for testing ThrowingActionPolicy)
struct ThrowingEntryState
{
    void on_entry(TestContext& ctx)
    {
        ctx.log += "ThrowEntry;";
        if (ctx.counter > 5)
        {
            throw std::runtime_error("Entry action failed");
        }
    }
    void on_exit(TestContext& ctx) noexcept
    {
        ctx.log += "ThrowExit;";
    }
};

struct ThrowingExitState
{
    void on_entry(TestContext& ctx) noexcept
    {
        ctx.log += "NormalEntry;";
    }
    void on_exit(TestContext& ctx)
    {
        ctx.log += "ThrowExit;";
        if (ctx.flag)
        {
            throw std::runtime_error("Exit action failed");
        }
    }
};

struct NormalState
{
    void on_entry(TestContext& ctx)
    {
        ctx.log += "Normal;";
    }
    void on_exit(TestContext& ctx)
    {
        ctx.log += "NormalExit;";
    }
};

// ============================================================================
// Test Suite 1: Basic State Machine Construction and Initialization
// ============================================================================

FATP_TEST_CASE(state_machine_construction)
{
    TestContext ctx;

    using TransitionList = std::tuple<std::pair<StateA, StateB>, std::pair<StateB, StateC>>;

    StateMachine<TestContext, TransitionList, StrictTransitionPolicy, NoExceptActionPolicy, 0, StateA, StateB, StateC>
        sm(ctx);

    // Should start in StateA (index 0) and call its on_entry
    FATP_ASSERT_EQ(sm.current_state_index(), 0, "Should start in first state (StateA)");
    FATP_ASSERT_EQ(ctx.log, std::string("A_entry;"), "Should have called StateA on_entry");
    FATP_ASSERT_EQ(ctx.counter, 1, "Counter should be 1 after StateA entry");
    FATP_ASSERT_TRUE(sm.is_in_state<StateA>(), "Should be in StateA");

    return true;
}

FATP_TEST_CASE(state_machine_default_policy)
{
    TestContext ctx;

    // AnyToAnyTransitionPolicy with empty transition list
    using TransitionList = std::tuple<>;

    StateMachine<TestContext, TransitionList, AnyToAnyTransitionPolicy, NoExceptActionPolicy, 0, StateA, StateB, StateC>
        sm(ctx);

    FATP_ASSERT_EQ(sm.current_state_index(), 0, "Should start in first state");
    FATP_ASSERT_EQ(ctx.log, std::string("A_entry;"), "Should have called StateA on_entry");
    FATP_ASSERT_TRUE(sm.is_in_state<StateA>(), "Should be in StateA");
    FATP_ASSERT_FALSE(sm.is_in_state<StateB>(), "Should not be in StateB");

    return true;
}

FATP_TEST_CASE(state_machine_custom_initial_state)
{
    TestContext ctx;

    using TransitionList = std::tuple<>;

    // Start at index 1 (StateB)
    StateMachine<TestContext, TransitionList, AnyToAnyTransitionPolicy, NoExceptActionPolicy, 1, StateA, StateB, StateC>
        sm(ctx);

    FATP_ASSERT_EQ(sm.current_state_index(), 1, "Should start in StateB (index 1)");
    FATP_ASSERT_EQ(ctx.log, std::string("B_entry;"), "Should have called StateB on_entry");
    FATP_ASSERT_TRUE(sm.is_in_state<StateB>(), "Should be in StateB");
    FATP_ASSERT_FALSE(sm.is_in_state<StateA>(), "Should not be in StateA");

    // Start at index 2 (StateC)
    ctx.reset();
    StateMachine<TestContext, TransitionList, AnyToAnyTransitionPolicy, NoExceptActionPolicy, 2, StateA, StateB, StateC>
        sm2(ctx);

    FATP_ASSERT_EQ(sm2.current_state_index(), 2, "Should start in StateC (index 2)");
    FATP_ASSERT_EQ(ctx.log, std::string("C_entry;"), "Should have called StateC on_entry");
    FATP_ASSERT_TRUE(sm2.is_in_state<StateC>(), "Should be in StateC");

    return true;
}

// ============================================================================
// Test Suite 2: Basic State Transitions
// ============================================================================

FATP_TEST_CASE(simple_transition)
{
    TestContext ctx;

    using TransitionList = std::tuple<std::pair<StateA, StateB>, std::pair<StateB, StateC>>;

    StateMachine<TestContext, TransitionList, StrictTransitionPolicy, NoExceptActionPolicy, 0, StateA, StateB, StateC>
        sm(ctx);

    ctx.log.clear();
    sm.transition<StateB>();

    FATP_ASSERT_EQ(sm.current_state_index(), 1, "Should now be in StateB (index 1)");
    FATP_ASSERT_EQ(ctx.log, std::string("A_exit;B_entry;"), "Should exit A and enter B");
    FATP_ASSERT_TRUE(sm.is_in_state<StateB>(), "Should be in StateB");
    FATP_ASSERT_FALSE(sm.is_in_state<StateA>(), "Should not be in StateA");

    return true;
}

FATP_TEST_CASE(chained_transitions)
{
    TestContext ctx;

    using TransitionList = std::tuple<std::pair<StateA, StateB>, std::pair<StateB, StateC>, std::pair<StateC, StateA>>;

    StateMachine<TestContext, TransitionList, StrictTransitionPolicy, NoExceptActionPolicy, 0, StateA, StateB, StateC>
        sm(ctx);

    ctx.log.clear();
    ctx.counter = 0;

    sm.transition<StateB>();
    FATP_ASSERT_EQ(sm.current_state_index(), 1, "Should be in StateB");
    FATP_ASSERT_TRUE(sm.is_in_state<StateB>(), "Should be in StateB");

    sm.transition<StateC>();
    FATP_ASSERT_EQ(sm.current_state_index(), 2, "Should be in StateC");
    FATP_ASSERT_TRUE(sm.is_in_state<StateC>(), "Should be in StateC");

    sm.transition<StateA>();
    FATP_ASSERT_EQ(sm.current_state_index(), 0, "Should be back in StateA");
    FATP_ASSERT_TRUE(sm.is_in_state<StateA>(), "Should be in StateA");

    // Should have: A_exit, B_entry, B_exit, C_entry, C_exit, A_entry
    FATP_ASSERT_EQ(ctx.counter, 6, "Should have 6 action calls");
    FATP_ASSERT_EQ(ctx.log,
                   std::string("A_exit;B_entry;B_exit;C_entry;C_exit;A_entry;"),
                   "Transition sequence should be correct");

    return true;
}

FATP_TEST_CASE(self_transition_is_noop)
{
    TestContext ctx;

    using TransitionList = std::tuple<std::pair<StateA, StateA>, // Self-transition allowed
                                      std::pair<StateA, StateB>>;

    StateMachine<TestContext, TransitionList, StrictTransitionPolicy, NoExceptActionPolicy, 0, StateA, StateB, StateC>
        sm(ctx);

    ctx.log.clear();
    ctx.counter = 0;

    sm.transition<StateA>(); // Transition to self

    FATP_ASSERT_EQ(sm.current_state_index(), 0, "Should remain in StateA");
    FATP_ASSERT_EQ(ctx.log, std::string(""), "Self-transition should not call actions");
    FATP_ASSERT_EQ(ctx.counter, 0, "Counter should not change");
    FATP_ASSERT_TRUE(sm.is_in_state<StateA>(), "Should still be in StateA");

    return true;
}

FATP_TEST_CASE(multiple_transitions_same_state)
{
    TestContext ctx;

    using TransitionList = std::tuple<std::pair<StateA, StateB>, std::pair<StateB, StateA>, std::pair<StateA, StateC>>;

    StateMachine<TestContext, TransitionList, StrictTransitionPolicy, NoExceptActionPolicy, 0, StateA, StateB, StateC>
        sm(ctx);

    ctx.log.clear();

    sm.transition<StateB>();
    sm.transition<StateA>();
    sm.transition<StateB>();
    sm.transition<StateA>();

    FATP_ASSERT_EQ(sm.current_state_index(), 0, "Should be in StateA");
    FATP_ASSERT_TRUE(ctx.log.find("A_exit;B_entry;B_exit;A_entry;A_exit;B_entry;B_exit;A_entry;") != std::string::npos,
                     "Should have multiple back-and-forth transitions");

    return true;
}

// ============================================================================
// Test Suite 3: StrictTransitionPolicy Validation
// ============================================================================

FATP_TEST_CASE(strict_policy_valid_transitions)
{
    TestContext ctx;

    using TransitionList = std::tuple<std::pair<IdleState, ProcessingState>,
                                      std::pair<ProcessingState, CompletedState>,
                                      std::pair<ProcessingState, ErrorState>,
                                      std::pair<ErrorState, IdleState>,
                                      std::pair<CompletedState, IdleState>>;

    StateMachine<TestContext,
                 TransitionList,
                 StrictTransitionPolicy,
                 NoExceptActionPolicy,
                 0,
                 IdleState,
                 ProcessingState,
                 CompletedState,
                 ErrorState>
        sm(ctx);

    ctx.log.clear();

    // Valid transitions
    sm.transition<ProcessingState>();
    FATP_ASSERT_EQ(ctx.flag, true, "Flag should be set in ProcessingState");
    FATP_ASSERT_EQ(ctx.counter, 10, "Counter should be 10");

    sm.transition<CompletedState>();
    FATP_ASSERT_EQ(ctx.counter, 110, "Counter should be 110 after completion");

    sm.transition<IdleState>();
    FATP_ASSERT_EQ(ctx.flag, false, "Flag should be cleared in IdleState");

    return true;
}

FATP_TEST_CASE(strict_policy_invalid_transition_throws)
{
    TestContext ctx;

    using TransitionList = std::tuple<std::pair<StateA, StateB>
                                      // Note: StateA -> StateC is NOT allowed
                                      >;

    StateMachine<TestContext, TransitionList, StrictTransitionPolicy, NoExceptActionPolicy, 0, StateA, StateB, StateC>
        sm(ctx);

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
    FATP_ASSERT_EQ(sm.current_state_index(), 0, "Should remain in original state after failed transition");
    FATP_ASSERT_TRUE(sm.is_in_state<StateA>(), "Should still be in StateA");

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

    StateMachine<TestContext,
                 TransitionList,
                 StrictTransitionPolicy,
                 NoExceptActionPolicy,
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
    FATP_ASSERT_EQ(ctx.counter, -1, "Error state should set counter to -1");
    FATP_ASSERT_TRUE(sm.is_in_state<ErrorState>(), "Should be in ErrorState");

    // Recover from error
    sm.transition<IdleState>();
    FATP_ASSERT_EQ(ctx.flag, false, "Should be back in idle state");
    FATP_ASSERT_TRUE(sm.is_in_state<IdleState>(), "Should be in IdleState");

    return true;
}

bool test_strict_policy_prevents_shortcut()
{
    TestContext ctx;

    using TransitionList = std::tuple<std::pair<IdleState, ProcessingState>, std::pair<ProcessingState, CompletedState>
                                      // Note: Idle -> Completed is NOT allowed
                                      >;

    StateMachine<TestContext,
                 TransitionList,
                 StrictTransitionPolicy,
                 NoExceptActionPolicy,
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
    FATP_ASSERT_TRUE(sm.is_in_state<IdleState>(), "Should remain in IdleState");

    return true;
}

// ============================================================================
// Test Suite 4: AnyToAnyTransitionPolicy
// ============================================================================

bool test_any_to_any_policy_all_transitions()
{
    TestContext ctx;

    using TransitionList = std::tuple<>; // Empty for AnyToAny

    StateMachine<TestContext, TransitionList, AnyToAnyTransitionPolicy, NoExceptActionPolicy, 0, StateA, StateB, StateC>
        sm(ctx);

    // Any transition should work
    ctx.log.clear();
    sm.transition<StateC>(); // A -> C (would be invalid in strict)
    FATP_ASSERT_EQ(sm.current_state_index(), 2, "Should be in StateC");
    FATP_ASSERT_TRUE(sm.is_in_state<StateC>(), "Should be in StateC");

    sm.transition<StateA>(); // C -> A
    FATP_ASSERT_EQ(sm.current_state_index(), 0, "Should be back in StateA");
    FATP_ASSERT_TRUE(sm.is_in_state<StateA>(), "Should be in StateA");

    sm.transition<StateB>(); // A -> B
    FATP_ASSERT_EQ(sm.current_state_index(), 1, "Should be in StateB");
    FATP_ASSERT_TRUE(sm.is_in_state<StateB>(), "Should be in StateB");

    return true;
}

bool test_any_to_any_with_complex_states()
{
    TestContext ctx;

    using TransitionList = std::tuple<>;

    StateMachine<TestContext,
                 TransitionList,
                 AnyToAnyTransitionPolicy,
                 NoExceptActionPolicy,
                 0,
                 IdleState,
                 ProcessingState,
                 CompletedState,
                 ErrorState>
        sm(ctx);

    // Jump directly from Idle to Completed (not typical but allowed)
    sm.transition<CompletedState>();
    FATP_ASSERT_EQ(ctx.counter, 100, "Should have Completed counter value");
    FATP_ASSERT_TRUE(sm.is_in_state<CompletedState>(), "Should be in CompletedState");

    // Jump to Error from Completed
    sm.transition<ErrorState>();
    FATP_ASSERT_EQ(ctx.counter, -1, "Should have Error counter value");
    FATP_ASSERT_TRUE(sm.is_in_state<ErrorState>(), "Should be in ErrorState");

    // Any transition is valid
    sm.transition<ProcessingState>();
    FATP_ASSERT_EQ(ctx.flag, true, "Should be processing");
    FATP_ASSERT_TRUE(sm.is_in_state<ProcessingState>(), "Should be in ProcessingState");

    return true;
}

bool test_any_to_any_with_four_states()
{
    TestContext ctx;

    using TransitionList = std::tuple<>;

    StateMachine<TestContext,
                 TransitionList,
                 AnyToAnyTransitionPolicy,
                 NoExceptActionPolicy,
                 0,
                 StateA,
                 StateB,
                 StateC,
                 StateD>
        sm(ctx);

    // Test all permutations work
    sm.transition<StateD>();
    FATP_ASSERT_TRUE(sm.is_in_state<StateD>(), "Should be in StateD");
    FATP_ASSERT_EQ(ctx.counter,
                   12,
                   "Counter should reflect StateD entry"); // 1 from A init + 1 from A exit + 10 from D entry

    sm.transition<StateB>();
    FATP_ASSERT_TRUE(sm.is_in_state<StateB>(), "Should be in StateB");

    sm.transition<StateA>();
    FATP_ASSERT_TRUE(sm.is_in_state<StateA>(), "Should be in StateA");

    return true;
}

// ============================================================================
// Test Suite 5: NoExceptActionPolicy vs ThrowingActionPolicy
// ============================================================================

bool test_noexcept_policy_compiles()
{
    TestContext ctx;

    using TransitionList = std::tuple<std::pair<StateA, StateB>>;

    // This should compile because all states have noexcept actions
    StateMachine<TestContext, TransitionList, StrictTransitionPolicy, NoExceptActionPolicy, 0, StateA, StateB, StateC>
        sm(ctx);

    // Verify noexcept specification
    static_assert(noexcept(std::declval<StateA>().on_entry(std::declval<TestContext&>())),
                  "StateA on_entry should be noexcept");
    static_assert(noexcept(std::declval<StateB>().on_exit(std::declval<TestContext&>())),
                  "StateB on_exit should be noexcept");

    return true;
}

bool test_throwing_policy_allows_exceptions()
{
    TestContext ctx;

    using TransitionList = std::tuple<std::pair<NormalState, ThrowingEntryState>>;

    // ThrowingActionPolicy allows non-noexcept actions
    StateMachine<TestContext,
                 TransitionList,
                 AnyToAnyTransitionPolicy,
                 ThrowingActionPolicy,
                 0,
                 NormalState,
                 ThrowingEntryState>
        sm(ctx);

    ctx.counter = 10; // Will cause throw in ThrowingEntryState

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

    return true;
}

bool test_throwing_exit_action()
{
    TestContext ctx;
    ctx.flag = true; // Will cause exit to throw

    using TransitionList = std::tuple<>;

    StateMachine<TestContext,
                 TransitionList,
                 AnyToAnyTransitionPolicy,
                 ThrowingActionPolicy,
                 0,
                 ThrowingExitState,
                 NormalState>
        sm(ctx);

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

    return true;
}

// ============================================================================
// Test Suite 6: Context Sharing and State Data
// ============================================================================

bool test_context_shared_between_states()
{
    TestContext ctx;
    ctx.counter = 0;
    ctx.flag = false;

    using TransitionList =
        std::tuple<std::pair<IdleState, ProcessingState>, std::pair<ProcessingState, CompletedState>>;

    StateMachine<TestContext,
                 TransitionList,
                 StrictTransitionPolicy,
                 NoExceptActionPolicy,
                 0,
                 IdleState,
                 ProcessingState,
                 CompletedState>
        sm(ctx);

    FATP_ASSERT_EQ(ctx.flag, false, "Initial flag should be false");
    FATP_ASSERT_EQ(ctx.counter, 0, "Initial counter should be 0");

    sm.transition<ProcessingState>();
    FATP_ASSERT_EQ(ctx.flag, true, "Processing should set flag");
    FATP_ASSERT_EQ(ctx.counter, 10, "Processing should increment counter");

    sm.transition<CompletedState>();
    FATP_ASSERT_EQ(ctx.counter, 110, "Completed should add to counter");

    return true;
}

bool test_context_persistence_across_transitions()
{
    TestContext ctx;

    using TransitionList = std::tuple<>;

    StateMachine<TestContext, TransitionList, AnyToAnyTransitionPolicy, NoExceptActionPolicy, 0, StateA, StateB, StateC>
        sm(ctx);

    // Perform multiple transitions and verify log accumulates
    // Note: Initial StateA entry already happened during construction
    sm.transition<StateB>();
    sm.transition<StateC>();
    sm.transition<StateA>();
    sm.transition<StateB>();

    // Log should contain initial entry plus all transitions
    std::string expected = "A_entry;A_exit;B_entry;B_exit;C_entry;C_exit;A_entry;A_exit;B_entry;";
    FATP_ASSERT_EQ(ctx.log, expected, "Context log should persist and accumulate");
    FATP_ASSERT_EQ(ctx.counter, 9, "Counter should reflect all actions"); // 1 initial + 8 from transitions

    return true;
}

bool test_context_modification_visible()
{
    TestContext ctx;
    ctx.counter = 100;

    using TransitionList = std::tuple<>;

    StateMachine<TestContext, TransitionList, AnyToAnyTransitionPolicy, NoExceptActionPolicy, 0, StateA, StateB> sm(
        ctx);

    // Initial state entry should modify context
    FATP_ASSERT_EQ(ctx.counter, 101, "Initial state should have incremented counter");

    // External modification should be visible
    ctx.counter = 50;
    sm.transition<StateB>();
    FATP_ASSERT_EQ(ctx.counter, 52, "External modifications should be visible"); // 50 + 1 (A exit) + 1 (B entry)

    return true;
}

// ============================================================================
// Test Suite 7: State Query Operations
// ============================================================================

bool test_current_state_index()
{
    TestContext ctx;

    using TransitionList = std::tuple<>;

    StateMachine<TestContext, TransitionList, AnyToAnyTransitionPolicy, NoExceptActionPolicy, 0, StateA, StateB, StateC>
        sm(ctx);

    FATP_ASSERT_EQ(sm.current_state_index(), 0, "Should start at index 0");

    sm.transition<StateB>();
    FATP_ASSERT_EQ(sm.current_state_index(), 1, "Should be at index 1");

    sm.transition<StateC>();
    FATP_ASSERT_EQ(sm.current_state_index(), 2, "Should be at index 2");

    sm.transition<StateA>();
    FATP_ASSERT_EQ(sm.current_state_index(), 0, "Should be back at index 0");

    return true;
}

bool test_is_in_state()
{
    TestContext ctx;

    using TransitionList = std::tuple<>;

    StateMachine<TestContext, TransitionList, AnyToAnyTransitionPolicy, NoExceptActionPolicy, 0, StateA, StateB, StateC>
        sm(ctx);

    FATP_ASSERT_TRUE(sm.is_in_state<StateA>(), "Should be in StateA");
    FATP_ASSERT_FALSE(sm.is_in_state<StateB>(), "Should not be in StateB");
    FATP_ASSERT_FALSE(sm.is_in_state<StateC>(), "Should not be in StateC");

    sm.transition<StateB>();
    FATP_ASSERT_FALSE(sm.is_in_state<StateA>(), "Should not be in StateA");
    FATP_ASSERT_TRUE(sm.is_in_state<StateB>(), "Should be in StateB");
    FATP_ASSERT_FALSE(sm.is_in_state<StateC>(), "Should not be in StateC");

    sm.transition<StateC>();
    FATP_ASSERT_FALSE(sm.is_in_state<StateA>(), "Should not be in StateA");
    FATP_ASSERT_FALSE(sm.is_in_state<StateB>(), "Should not be in StateB");
    FATP_ASSERT_TRUE(sm.is_in_state<StateC>(), "Should be in StateC");

    return true;
}

bool test_query_operations_consistency()
{
    TestContext ctx;

    using TransitionList = std::tuple<>;

    StateMachine<TestContext,
                 TransitionList,
                 AnyToAnyTransitionPolicy,
                 NoExceptActionPolicy,
                 0,
                 IdleState,
                 ProcessingState,
                 CompletedState,
                 ErrorState>
        sm(ctx);

    // is_in_state should match current_state_index
    FATP_ASSERT_EQ(sm.current_state_index(), 0, "Index should be 0");
    FATP_ASSERT_TRUE(sm.is_in_state<IdleState>(), "Should be in IdleState");

    sm.transition<ProcessingState>();
    FATP_ASSERT_EQ(sm.current_state_index(), 1, "Index should be 1");
    FATP_ASSERT_TRUE(sm.is_in_state<ProcessingState>(), "Should be in ProcessingState");

    sm.transition<ErrorState>();
    FATP_ASSERT_EQ(sm.current_state_index(), 3, "Index should be 3");
    FATP_ASSERT_TRUE(sm.is_in_state<ErrorState>(), "Should be in ErrorState");

    return true;
}

// ============================================================================
// Test Suite 8: Edge Cases and Error Handling
// ============================================================================

bool test_single_state_machine()
{
    TestContext ctx;

    using TransitionList = std::tuple<>;

    StateMachine<TestContext, TransitionList, AnyToAnyTransitionPolicy, NoExceptActionPolicy, 0, StateA> sm(ctx);

    FATP_ASSERT_EQ(sm.current_state_index(), 0, "Should be at index 0");
    FATP_ASSERT_TRUE(sm.is_in_state<StateA>(), "Should be in StateA");
    FATP_ASSERT_EQ(ctx.log, std::string("A_entry;"), "Should have called entry");

    // Self-transition should be no-op
    ctx.log.clear();
    sm.transition<StateA>();
    FATP_ASSERT_EQ(ctx.log, std::string(""), "Self-transition should be no-op");

    return true;
}

bool test_large_state_machine()
{
    TestContext ctx;

    using TransitionList = std::tuple<>;

    // Test with 4 states
    StateMachine<TestContext,
                 TransitionList,
                 AnyToAnyTransitionPolicy,
                 NoExceptActionPolicy,
                 0,
                 StateA,
                 StateB,
                 StateC,
                 StateD>
        sm(ctx);

    FATP_ASSERT_EQ(sm.current_state_index(), 0, "Should start at index 0");

    sm.transition<StateD>();
    FATP_ASSERT_EQ(sm.current_state_index(), 3, "Should be at index 3");
    FATP_ASSERT_TRUE(sm.is_in_state<StateD>(), "Should be in StateD");

    sm.transition<StateB>();
    FATP_ASSERT_EQ(sm.current_state_index(), 1, "Should be at index 1");

    return true;
}

bool test_empty_log_accumulation()
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

    StateMachine<TestContext, TransitionList, AnyToAnyTransitionPolicy, NoExceptActionPolicy, 0, QuietState, StateA> sm(
        ctx);

    FATP_ASSERT_EQ(ctx.log, std::string(""), "Quiet state should not log");

    sm.transition<StateA>();
    FATP_ASSERT_EQ(ctx.log, std::string("A_entry;"), "Only StateA should log");

    return true;
}

bool test_initial_state_action_called_once()
{
    TestContext ctx;

    using TransitionList = std::tuple<>;

    StateMachine<TestContext, TransitionList, AnyToAnyTransitionPolicy, NoExceptActionPolicy, 0, StateA, StateB> sm(
        ctx);

    // Initial entry should have been called exactly once
    FATP_ASSERT_EQ(ctx.counter, 1, "Entry should be called once on construction");
    FATP_ASSERT_EQ(ctx.log, std::string("A_entry;"), "Should have one entry log");

    return true;
}

// ============================================================================
// Test Suite 9: Performance and Compile-Time Validation
// ============================================================================

bool test_compile_time_state_validation()
{
    TestContext ctx;

    using TransitionList = std::tuple<>;

    // These should compile without issues
    StateMachine<TestContext, TransitionList, AnyToAnyTransitionPolicy, NoExceptActionPolicy, 0, StateA, StateB, StateC>
        sm(ctx);

    // The following would fail at compile time (tested manually):
    // StateMachine with duplicate states
    // StateMachine with invalid InitialIndex
    // StateMachine with no states
    // transition<InvalidState>()

    return true;
}

bool test_noexcept_specification()
{
    TestContext ctx;

    using TransitionList = std::tuple<>;

    StateMachine<TestContext, TransitionList, AnyToAnyTransitionPolicy, NoExceptActionPolicy, 0, StateA, StateB> sm(
        ctx);

    // With NoExceptActionPolicy AND AnyToAnyTransitionPolicy, transition should be noexcept
    static_assert(noexcept(sm.template transition<StateB>()),
                  "transition should be noexcept with NoExceptActionPolicy and AnyToAnyTransitionPolicy");

    // With StrictTransitionPolicy, transition is NOT noexcept (can throw on invalid transition)
    using StrictTransitionList = std::tuple<std::pair<StateA, StateB>>;
    StateMachine<TestContext, StrictTransitionList, StrictTransitionPolicy, NoExceptActionPolicy, 0, StateA, StateB>
        sm_strict(ctx);

    static_assert(!noexcept(sm_strict.template transition<StateB>()),
                  "transition should NOT be noexcept with StrictTransitionPolicy (can throw)");

    return true;
}

// ============================================================================
// Test Suite 10: Complex Scenarios
// ============================================================================

bool test_workflow_simulation()
{
    TestContext ctx;

    // Simulate a typical workflow: Idle -> Processing -> Completed -> Idle
    using TransitionList = std::tuple<std::pair<IdleState, ProcessingState>,
                                      std::pair<ProcessingState, CompletedState>,
                                      std::pair<ProcessingState, ErrorState>,
                                      std::pair<CompletedState, IdleState>,
                                      std::pair<ErrorState, IdleState>>;

    StateMachine<TestContext,
                 TransitionList,
                 StrictTransitionPolicy,
                 NoExceptActionPolicy,
                 0,
                 IdleState,
                 ProcessingState,
                 CompletedState,
                 ErrorState>
        sm(ctx);

    // Happy path
    FATP_ASSERT_TRUE(sm.is_in_state<IdleState>(), "Start in Idle");

    sm.transition<ProcessingState>();
    FATP_ASSERT_TRUE(sm.is_in_state<ProcessingState>(), "Move to Processing");
    FATP_ASSERT_EQ(ctx.counter, 10, "Processing increments counter");

    sm.transition<CompletedState>();
    FATP_ASSERT_TRUE(sm.is_in_state<CompletedState>(), "Move to Completed");
    FATP_ASSERT_EQ(ctx.counter, 110, "Completed adds to counter");

    sm.transition<IdleState>();
    FATP_ASSERT_TRUE(sm.is_in_state<IdleState>(), "Back to Idle");

    // Error path
    ctx.counter = 0;
    sm.transition<ProcessingState>();
    sm.transition<ErrorState>();
    FATP_ASSERT_TRUE(sm.is_in_state<ErrorState>(), "Error state reached");
    FATP_ASSERT_EQ(ctx.counter, -1, "Error resets counter");

    sm.transition<IdleState>();
    FATP_ASSERT_TRUE(sm.is_in_state<IdleState>(), "Recovered to Idle");

    return true;
}

bool test_cyclic_transitions()
{
    TestContext ctx;

    using TransitionList = std::tuple<std::pair<StateA, StateB>, std::pair<StateB, StateC>, std::pair<StateC, StateA>>;

    StateMachine<TestContext, TransitionList, StrictTransitionPolicy, NoExceptActionPolicy, 0, StateA, StateB, StateC>
        sm(ctx);

    ctx.reset();

    // Cycle multiple times
    for (int i = 0; i < 3; ++i)
    {
        sm.transition<StateB>();
        sm.transition<StateC>();
        sm.transition<StateA>();
    }

    FATP_ASSERT_TRUE(sm.is_in_state<StateA>(), "Should end in StateA");
    FATP_ASSERT_EQ(ctx.counter, 18, "Should have 6 actions per cycle * 3 cycles");

    return true;
}

bool test_state_machine_with_custom_initial()
{
    TestContext ctx;

    using TransitionList = std::tuple<std::pair<ProcessingState, CompletedState>,
                                      std::pair<CompletedState, IdleState>,
                                      std::pair<IdleState, ProcessingState>>;

    // Start in ProcessingState (index 1)
    StateMachine<TestContext,
                 TransitionList,
                 StrictTransitionPolicy,
                 NoExceptActionPolicy,
                 1,
                 IdleState,
                 ProcessingState,
                 CompletedState>
        sm(ctx);

    FATP_ASSERT_TRUE(sm.is_in_state<ProcessingState>(), "Should start in ProcessingState");
    FATP_ASSERT_EQ(ctx.counter, 10, "Should have Processing initial counter");
    FATP_ASSERT_EQ(ctx.flag, true, "Processing should set flag");

    // Complete workflow from processing
    sm.transition<CompletedState>();
    sm.transition<IdleState>();

    FATP_ASSERT_TRUE(sm.is_in_state<IdleState>(), "Should reach IdleState");
    FATP_ASSERT_EQ(ctx.flag, false, "Idle should clear flag");

    return true;
}


// ============================================================================
// Main Test Runner
// ============================================================================

} // namespace fat_p::testing::statemachine

namespace fat_p::testing
{

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
    std::cout << "\nSuite 3: StrictTransitionPolicy\n";
    FATP_RUN_TEST_NS(runner, statemachine, strict_policy_valid_transitions);
    FATP_RUN_TEST_NS(runner, statemachine, strict_policy_invalid_transition_throws);
    FATP_RUN_TEST_NS(runner, statemachine, strict_policy_complex_graph);
    FATP_RUN_TEST_NS(runner, statemachine, strict_policy_prevents_shortcut);

    // Suite 4: AnyToAny Policy
    std::cout << "\nSuite 4: AnyToAnyTransitionPolicy\n";
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
