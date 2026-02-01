
/*
FATP_META:
  meta_version: 1
  component: StateMachine
  file_role: test
  path: components/StateMachine/tests/compile_fail/compile_fail_StateMachine_NoExceptPolicyRequiresNoexceptCtor.cpp
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

#include <tuple>

namespace fat_p::testing::compile_fail::statemachine_noexcept_policy_requires_noexcept_ctor
{

struct Ctx
{
    int mCounter = 0;
};

struct BadCtorState
{
    BadCtorState() noexcept(false)
    {
    }

    void on_entry(Ctx&) noexcept
    {
    }

    void on_exit(Ctx&) noexcept
    {
    }
};

// NoExceptActionPolicy wraps hooks in noexcept functions. Since hooks are
// invoked on a temporary (TState{}), a throwing default constructor would
// terminate the program. The StateMachine contract requires nothrow default
// construction under NoExceptActionPolicy.
using SM = StateMachine<Ctx,
                        std::tuple<>,
                        AnyToAnyTransitionPolicy,
                        NoExceptActionPolicy,
                        0,
                        BadCtorState>;

inline void force_instantiate()
{
    Ctx ctx;
    SM sm(ctx);
    (void)sm;
}

} // namespace fat_p::testing::compile_fail::statemachine_noexcept_policy_requires_noexcept_ctor
