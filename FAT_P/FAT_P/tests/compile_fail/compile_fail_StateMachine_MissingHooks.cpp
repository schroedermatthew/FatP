#include "StateMachine.h"

namespace fat_p::testing::compile_fail::statemachine_missing_hooks_1
{

struct Ctx
{
    int mCounter = 0;
};

struct MissingExit
{
    void on_entry(Ctx&) noexcept
    {
    }

    // on_exit intentionally missing
};

using TransitionList = std::tuple<>;

using SM = StateMachine<Ctx,
                        TransitionList,
                        AnyToAnyTransitionPolicy,
                        NoExceptActionPolicy,
                        0,
                        MissingExit>;

inline void force_instantiate()
{
    Ctx ctx;
    SM sm(ctx);
    (void)sm;
}

} // namespace fat_p::testing::compile_fail::statemachine_missing_hooks_1
