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
