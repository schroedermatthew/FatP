/**
 * @file test_StateMachine_HeaderSelfContained.cpp
 * @brief Header self-containment test for StateMachine.h
 *
 * @details
 * Verifies that StateMachine.h is self-contained: it compiles when included
 * first (and only) in an otherwise empty TU. Double-include validates
 * #pragma once / idempotence.
 *
 * This file exists primarily to COMPILE. Runtime checks are minimal.
 */
/*
FATP_META:
  meta_version: 1
  component: StateMachine
  file_role: header_self_contained_test
  path: tests/test_StateMachine_HeaderSelfContained.cpp
  namespace: fat_p::testing
  summary: "Compile-only self-containment check for StateMachine.h"
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

// CRITICAL: StateMachine.h MUST be the first include (no FatPTest.h!)
#include "StateMachine.h"
#include "StateMachine.h"  // Validate idempotence

#include <iostream>
#include <tuple>
#include <utility>

namespace fat_p::testing::statemachine_header_self_contained
{

struct Ctx
{
    int mCounter = 0;
};

struct A
{
    void on_entry(Ctx& ctx) noexcept { ++ctx.mCounter; }
    void on_exit(Ctx&) noexcept {}
};

struct B
{
    void on_entry(Ctx&) noexcept {}
    void on_exit(Ctx&) noexcept {}
};

} // namespace fat_p::testing::statemachine_header_self_contained

namespace fat_p::testing
{

bool test_StateMachine_HeaderSelfContained()
{
    std::cout << "==========================================================\n";
    std::cout << "STATE MACHINE HEADER UNIT TESTS\n";
    std::cout << "==========================================================\n\n";
    std::cout << "Suite: StateMachine Header Self-Containment\n";

    using namespace statemachine_header_self_contained;

    // Minimal instantiation to force template compilation
    using TL = std::tuple<std::pair<A, B>>;
    using SM = fat_p::StateMachine<
        Ctx,
        TL,
        fat_p::StrictTransitionPolicy,
        fat_p::NoExceptActionPolicy,
        0,
        A, B>;

    Ctx ctx;
    SM sm(ctx);
    sm.transition<B>();

    const bool passed = (ctx.mCounter == 1) && sm.isInState<B>();

    std::cout << "[COMPILE] Running: header_self_contained_includes ... ";
    std::cout << (passed ? "PASSED" : "FAILED") << " (0.00 ms)\n\n";
    std::cout << "=== Test Summary ===\n";
    std::cout << "Passed: " << (passed ? 1 : 0) << "\n";
    std::cout << "Failed: " << (passed ? 0 : 1) << "\n";
    std::cout << "Total:  1\n";

    return passed;
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_StateMachine_HeaderSelfContained() ? 0 : 1;
}
#endif
