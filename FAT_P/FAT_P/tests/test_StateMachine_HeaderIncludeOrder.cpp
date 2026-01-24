/**
 * @file test_StateMachine_HeaderIncludeOrder.cpp
 * @brief Include-order hygiene checks for StateMachine.h.
 */
/*
FATP_META:
  meta_version: 1
  component: StateMachine
  file_role: test
  path: tests/test_StateMachine_HeaderIncludeOrder.cpp
  namespace: fat_p
  summary: "StateMachine include-order hygiene checks (included after other headers)."
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

// This TU intentionally includes common standard headers and FatPTest.h BEFORE
// StateMachine.h. The goal is to detect include-order hazards and macro/
// namespace collisions.

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <tuple>
#include <vector>

#include "FatPTest.h"
#include "StateMachine.h"

namespace fat_p::testing::statemachine_header_include_order
{

struct Ctx
{
    int mCounter = 0;
    std::string mLog;
};

struct A
{
    void on_entry(Ctx& ctx) noexcept
    {
        ++ctx.mCounter;
        ctx.mLog += "A+";
    }

    void on_exit(Ctx& ctx) noexcept
    {
        ++ctx.mCounter;
        ctx.mLog += "A-";
    }
};

struct B
{
    void on_entry(Ctx& ctx) noexcept
    {
        ++ctx.mCounter;
        ctx.mLog += "B+";
    }

    void on_exit(Ctx& ctx) noexcept
    {
        ++ctx.mCounter;
        ctx.mLog += "B-";
    }
};

FATP_TEST_CASE(header_include_order_compiles_and_runs)
{
    Ctx ctx;

    using TransitionList = std::tuple<std::pair<A, B>>;

    StateMachine<Ctx,
                 TransitionList,
                 StrictTransitionPolicy,
                 NoExceptActionPolicy,
                 0,
                 A,
                 B>
        sm(ctx);

    FATP_ASSERT_TRUE(sm.isInState<A>(), "Starts in A");
    sm.transition<B>();
    FATP_ASSERT_TRUE(sm.isInState<B>(), "A->B allowed");
    FATP_ASSERT_EQ(ctx.mCounter, 3, "Initial entry + exit + entry");
    FATP_ASSERT_EQ(ctx.mLog, std::string("A+A-B+"), "Hook order / log");

    return true;
}

} // namespace fat_p::testing::statemachine_header_include_order

namespace fat_p::testing
{

bool test_StateMachine_HeaderIncludeOrder()
{
    FATP_PRINT_HEADER(STATE MACHINE HEADER INCLUDE ORDER)

    TestRunner runner;
    std::cout << "Suite: StateMachine Header Include-Order\n";
    FATP_RUN_TEST_NS(runner,
                     statemachine_header_include_order,
                     header_include_order_compiles_and_runs);

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_StateMachine_HeaderIncludeOrder() ? 0 : 1;
}
#endif
