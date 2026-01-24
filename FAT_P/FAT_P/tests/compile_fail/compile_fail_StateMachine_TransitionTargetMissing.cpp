#include "StateMachine.h"

namespace fat_p::testing::compile_fail::statemachine_transition_target_missing_1
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

using TransitionList = std::tuple<std::pair<A, B>>;

using SM = StateMachine<Ctx,
                        TransitionList,
                        AnyToAnyTransitionPolicy,
                        NoExceptActionPolicy,
                        0,
                        A,
                        B>;

inline void force_instantiate()
{
    Ctx ctx;
    SM sm(ctx);
    // Must fail: Missing is not in the machine's state pack.
    sm.template transition<Missing>();
}

} // namespace fat_p::testing::compile_fail::statemachine_transition_target_missing_1
