/**
 * @file compile_fail_StateMachine_ZeroStates.cpp
 * @brief Expected-fail: StateMachine requires at least one state.
 *
 * @details
 * This translation unit must FAIL to compile. It verifies the static_assert
 * that enforces: kNumStates > 0.
 *
 * Expected error: "StateMachine must have at least one state"
 */
/*
FATP_META:
  meta_version: 1
  component: StateMachine
  file_role: compile_fail_test
  path: tests/compile_fail/compile_fail_StateMachine_ZeroStates.cpp
  namespace: fat_p::testing::compile_fail
  summary: "Compile-fail test: zero states rejected"
  contract_tested: "kNumStates > 0"
  expected_error: "at least one state"
  related:
    headers:
      - fat_p/StateMachine.h
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
*/

#include "StateMachine.h"

namespace fat_p::testing::compile_fail::statemachine_zero_states_1
{

struct Ctx
{
    int mCounter = 0;
};

using TransitionList = std::tuple<>;

// Must fail: StateMachine requires at least one state type.
using SM = fat_p::StateMachine<Ctx,
                               TransitionList,
                               fat_p::AnyToAnyTransitionPolicy,
                               fat_p::ThrowingActionPolicy,
                               0>;

// Force instantiation
static_assert(sizeof(SM) > 0, "SM must be a complete type");

} // namespace fat_p::testing::compile_fail::statemachine_zero_states_1
