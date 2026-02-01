
/*
FATP_META:
  meta_version: 1
  component: StateMachine
  file_role: test
  path: components/StateMachine/tests/compile_fail/compile_fail_StateMachine_DuplicateStates.cpp
  layer: Testing
  namespace: fat_p
  summary: "test file for StateMachine"
  api_stability: in_work
  related:
    docs_search: "StateMachine"
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

namespace fat_p::testing::compile_fail::statemachine_duplicate_states_1
{

struct Ctx
{
    int mCounter = 0;
};

struct A
{
    void on_entry(Ctx&) noexcept
    {
    }

    void on_exit(Ctx&) noexcept
    {
    }
};

using TransitionList = std::tuple<std::pair<A, A>>;

using SM = StateMachine<Ctx,
                        TransitionList,
                        AnyToAnyTransitionPolicy,
                        NoExceptActionPolicy,
                        0,
                        A,
                        A>;

inline void force_instantiate()
{
    Ctx ctx;
    SM sm(ctx);
    (void)sm;
}

} // namespace fat_p::testing::compile_fail::statemachine_duplicate_states_1
