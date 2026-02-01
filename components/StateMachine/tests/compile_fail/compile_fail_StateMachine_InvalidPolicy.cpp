/**
 * @file compile_fail_StateMachine_InvalidPolicy.cpp
 * @brief Compile-fail test: Invalid policy types must be rejected.
 *
 * @details
 * This test verifies that StateMachine rejects invalid policy types at
 * compile time with a clear error message. Only the following policies
 * are valid:
 *   - TTransitionPolicy: StrictTransitionPolicy or AnyToAnyTransitionPolicy
 *   - TActionPolicy: NoExceptActionPolicy or ThrowingActionPolicy
 *
 * Expected error:
 *   "TTransitionPolicy must be StrictTransitionPolicy or AnyToAnyTransitionPolicy"
 *   OR
 *   "TActionPolicy must be NoExceptActionPolicy or ThrowingActionPolicy"
 */
/*
FATP_META:
  meta_version: 1
  component: StateMachine
  file_role: test
  path: components/StateMachine/tests/compile_fail/compile_fail_StateMachine_InvalidPolicy.cpp
  layer: Testing
  namespace: N/A
  summary: "Compile-fail test: Invalid policy types are rejected."
  api_stability: stable
  related:
    docs_search: "StateMachine"
    headers:
      - include/fat_p/StateMachine.h
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

#include "StateMachine.h"

#include <tuple>

namespace
{

struct BadTransitionPolicy
{
};

struct BadActionPolicy
{
};

struct TestContext
{
    int mValue = 0;
};

struct StateA
{
    void on_entry(TestContext&) {}
    void on_exit(TestContext&) {}
};

// COMPILE-FAIL TEST 1: Invalid TTransitionPolicy
// Uncomment the line below to test:
// using SM_BadTransition = fat_p::StateMachine<TestContext, std::tuple<>,
//     BadTransitionPolicy, fat_p::ThrowingActionPolicy, 0, StateA>;

// COMPILE-FAIL TEST 2: Invalid TActionPolicy
// Uncomment the line below to test:
using SM_BadAction = fat_p::StateMachine<TestContext, std::tuple<>,
    fat_p::AnyToAnyTransitionPolicy, BadActionPolicy, 0, StateA>;

} // anonymous namespace

int main()
{
    TestContext ctx;

    // Instantiate to trigger static_assert
    SM_BadAction sm(ctx);
    (void)sm;

    return 0;
}
