/**
 * @file compile_fail_StateMachine_NonDefaultConstructible.cpp
 * @brief Expected-fail: State types must be default-constructible.
 *
 * @details
 * This translation unit must FAIL to compile. It verifies the static_assert
 * that enforces: std::is_default_constructible_v<States>.
 *
 * Expected error: "default-constructible"
 */
/*
FATP_META:
  meta_version: 1
  component: StateMachine
  file_role: compile_fail_test
  path: tests/compile_fail/compile_fail_StateMachine_NonDefaultConstructible.cpp
  namespace: fat_p::testing::compile_fail
  summary: "Compile-fail test: non-default-constructible state rejected"
  contract_tested: "is_default_constructible_v<States>"
  expected_error: "default-constructible"
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

namespace fat_p::testing::compile_fail::statemachine_non_default_constructible
{

struct Ctx
{
    int mCounter = 0;
};

struct NonDefaultState
{
    int mValue;

    // No default constructor - requires argument
    explicit NonDefaultState(int v) : mValue(v) {}

    void on_entry(Ctx&) noexcept {}
    void on_exit(Ctx&) noexcept {}
};

struct OkState
{
    void on_entry(Ctx&) noexcept {}
    void on_exit(Ctx&) noexcept {}
};

// IMPORTANT: Use fully-qualified names to avoid lookup issues
using TL = std::tuple<std::pair<NonDefaultState, OkState>>;

// Must fail: NonDefaultState is not default-constructible
using SM = fat_p::StateMachine<
    Ctx,
    TL,
    fat_p::AnyToAnyTransitionPolicy,
    fat_p::NoExceptActionPolicy,
    0,
    NonDefaultState,
    OkState>;

// Force instantiation
static_assert(sizeof(SM) > 0, "Force complete type instantiation");

} // namespace fat_p::testing::compile_fail::statemachine_non_default_constructible
