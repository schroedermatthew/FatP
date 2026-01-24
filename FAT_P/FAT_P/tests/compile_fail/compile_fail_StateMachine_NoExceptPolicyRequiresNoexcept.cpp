#include "StateMachine.h"

namespace fat_p::testing::compile_fail::statemachine_noexcept_requires_noexcept_1
{

struct Ctx
{
    int mCounter = 0;
};

struct NotNoexcept
{
    void on_entry(Ctx&)
    {
    }

    void on_exit(Ctx&) noexcept
    {
    }
};

using TransitionList = std::tuple<>;

using SM = StateMachine<Ctx,
                        TransitionList,
                        AnyToAnyTransitionPolicy,
                        NoExceptActionPolicy,
                        0,
                        NotNoexcept>;

inline void force_instantiate()
{
    Ctx ctx;
    // Constructor calls the NoExceptActionPolicy validation.
    SM sm(ctx);
    (void)sm;
}

} // namespace fat_p::testing::compile_fail::statemachine_noexcept_requires_noexcept_1
