#include "StateMachine.h"

namespace fat_p::testing::compile_fail::statemachine_bad_initial_index_1
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

using TransitionList = std::tuple<std::pair<A, B>>;

// InitialIndex=2 is out of range for 2 states (valid: 0..1).
using SM = StateMachine<Ctx,
                        TransitionList,
                        AnyToAnyTransitionPolicy,
                        NoExceptActionPolicy,
                        2,
                        A,
                        B>;

inline void force_instantiate()
{
    Ctx ctx;
    SM sm(ctx);
    (void)sm;
}

} // namespace fat_p::testing::compile_fail::statemachine_bad_initial_index_1
