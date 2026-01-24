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
