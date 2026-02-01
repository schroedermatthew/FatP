
/*
FATP_META:
  meta_version: 1
  component: StateMachine
  file_role: test
  path: components/StateMachine/tests/compile_fail/compile_fail_StateMachine_TransitionListMissingState.cpp
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

namespace fat_p::testing::compile_fail::statemachine_transition_list_missing_state_1
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

struct B
{
    void on_entry(Ctx&) noexcept
    {
    }

    void on_exit(Ctx&) noexcept
    {
    }
};

struct Missing
{
    void on_entry(Ctx&) noexcept
    {
    }

    void on_exit(Ctx&) noexcept
    {
    }
};

using TransitionList = std::tuple<std::pair<A, Missing>>;

// StrictTransitionPolicy should reject TransitionList pairs that reference
// states not present in the machine's States... pack.
using SM = StateMachine<Ctx,
                        TransitionList,
                        StrictTransitionPolicy,
                        NoExceptActionPolicy,
                        0,
                        A,
                        B>;

inline void force_instantiate()
{
    Ctx ctx;
    SM sm(ctx);
    (void)sm;
}

} // namespace fat_p::testing::compile_fail::statemachine_transition_list_missing_state_1
