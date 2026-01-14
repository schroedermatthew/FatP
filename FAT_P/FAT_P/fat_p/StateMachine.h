/**
 * @file StateMachine.h
 * @brief Type-safe finite state machine with compile-time validation
 *
 * @layer Domain
 */
#pragma once
/*
FATP_META:
  meta_version: 1
  component: StateMachine
  file_role: public_header
  path: fat_p/StateMachine.h
  namespace: fat_p
  layer: Domain
  summary: "Public header for StateMachine."
  api_stability: in_work
  related:
    docs_search: "StateMachine"
    tests:
      - tests/test_StateMachine.cpp
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/
#include "enforce.h"
#include <array>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>

namespace fat_p
{
// ====================================================================
// 1. Policies and Checker Traits
// ====================================================================

// --- Transition Policies ---
/** @brief Enforces only explicitly defined transitions in the TransitionList. */
struct StrictTransitionPolicy
{
};

/** @brief Allows any state to transition to any other state. */
struct AnyToAnyTransitionPolicy
{
};

// --- Action Policies ---
/** @brief Uses static_assert to ensure all State::on_entry/on_exit actions are noexcept. */
struct NoExceptActionPolicy
{
};

/** @brief Default policy allowing actions to throw. */
struct ThrowingActionPolicy
{
};

// --- Action Policy Checker (For NoExceptActionPolicy) ---
// SFINAE concepts to check for noexcept guarantee on state actions.
template <typename State, typename Context>
using HasNoexceptEntry =
    std::integral_constant<bool, noexcept(std::declval<State>().on_entry(std::declval<Context&>()))>;

template <typename State, typename Context>
using HasNoexceptExit = std::integral_constant<bool, noexcept(std::declval<State>().on_exit(std::declval<Context&>()))>;

// Policy enforcer utility
template <typename Policy, typename State, typename Context>
struct ActionPolicyEnforcer
{
    static constexpr void validate() noexcept
    {
    }
};

// Specialization that activates static_assert for NoExceptActionPolicy
template <typename State, typename Context>
struct ActionPolicyEnforcer<NoExceptActionPolicy, State, Context>
{
    static constexpr void validate() noexcept
    {
        static_assert(HasNoexceptEntry<State, Context>::value,
                      "CTSM Error: NoExceptActionPolicy requires State::on_entry() to be noexcept.");
        static_assert(HasNoexceptExit<State, Context>::value,
                      "CTSM Error: NoExceptActionPolicy requires State::on_exit() to be noexcept.");
    }
};

// --- Unique States Checker ---
template <typename... Ts>
struct are_unique;

template <>
struct are_unique<> : std::true_type
{
};

template <typename T, typename... Ts>
struct are_unique<T, Ts...>
    : std::conjunction<std::negation<std::disjunction<std::is_same<T, Ts>...>>, are_unique<Ts...>>
{
};

// ====================================================================
// 2. Main State Machine Class (Base Template)
// ====================================================================

/**
 * @brief A zero-overhead, compile-time defined state machine based on policies.
 *
 * @tparam Context The mutable data object shared by all states.
 * @tparam TransitionList A std::tuple of std::pair<FromState, ToState> for allowed transitions.
 * @tparam TTransitionPolicy (StrictTransitionPolicy or AnyToAnyTransitionPolicy).
 * @tparam TActionPolicy (NoExceptActionPolicy or ThrowingActionPolicy).
 * @tparam InitialIndex The index of the initial state (default: 0).
 * @tparam States A variadic pack of state types.
 */
template <typename Context,
          typename TransitionList,
          typename TTransitionPolicy,
          typename TActionPolicy,
          size_t InitialIndex = 0,
          typename... States>
class StateMachine
{
    using StateTuple = std::tuple<States...>;
    static constexpr size_t NumStates = sizeof...(States);

    // Compile-time validation
    static_assert(NumStates > 0, "StateMachine must have at least one state");
    static_assert(InitialIndex < NumStates, "InitialIndex must be within 0 to NumStates-1");
    static_assert(are_unique<States...>::value, "State types must be unique in the variadic pack");

    // --- State Storage ---
    Context& mContext;
    int mCurrentStateIndex = InitialIndex;

    // --- Compile-Time Index Finder (Non-Recursive) ---
    template <typename TState, std::size_t... I>
    static constexpr int find_index_impl(std::index_sequence<I...>)
    {
        constexpr bool matches[NumStates] = {std::is_same_v<TState, std::tuple_element_t<I, StateTuple>>...};
        for (int j = 0; j < NumStates; ++j)
        {
            if (matches[j])
            {
                return j;
            }
        }
        return -1;
    }

    template <typename TState>
    static constexpr int get_state_index()
    {
        return find_index_impl<TState>(std::make_index_sequence<NumStates>{});
    }

    // --- Recursive Dispatchers ---
    template <int I = 0>
    void dispatch_exit_rec(int targetIndex)
    {
        if constexpr (I < NumStates)
        {
            if (targetIndex == I)
            {
                std::tuple_element_t<I, StateTuple>{}.on_exit(mContext);
            }
            dispatch_exit_rec<I + 1>(targetIndex);
        }
    }

    void dispatch_exit_action(int targetIndex)
    {
        dispatch_exit_rec(targetIndex);
    }

    template <int I = 0>
    void dispatch_entry_rec(int targetIndex)
    {
        if constexpr (I < NumStates)
        {
            if (targetIndex == I)
            {
                std::tuple_element_t<I, StateTuple>{}.on_entry(mContext);
            }
            dispatch_entry_rec<I + 1>(targetIndex);
        }
    }

    void dispatch_entry_action(int targetIndex)
    {
        dispatch_entry_rec(targetIndex);
    }

public:
    StateMachine(Context& context)
        : mContext(context)
        , mCurrentStateIndex(InitialIndex)
    {
        // Enforce action policies on all states at compile time
        (ActionPolicyEnforcer<TActionPolicy, States, Context>::validate(), ...);
        // Enter the initial state
        dispatch_entry_action(InitialIndex);
    }

    /**
     * @brief Initiates a transition to the specified state type.
     *
     * @tparam TNextState The target state type (must be in the States pack).
     */
    template <typename TNextState>
    void transition() noexcept(std::is_same_v<TActionPolicy, NoExceptActionPolicy> &&
                               std::is_same_v<TTransitionPolicy, AnyToAnyTransitionPolicy>)
    {
        constexpr int nextIndex = get_state_index<TNextState>();
        const int currentIndex = mCurrentStateIndex;

        static_assert(nextIndex != -1, "Target state type not found in the StateMachine definition");

        if (currentIndex == nextIndex)
        {
            // No action needed for self-transition
            return;
        }

        // Exit Action
        dispatch_exit_action(currentIndex);

        // Update State
        mCurrentStateIndex = nextIndex;

        // Entry Action
        dispatch_entry_action(nextIndex);
    }

    /** @brief Gets the runtime index of the current state. */
    int current_state_index() const noexcept
    {
        return mCurrentStateIndex;
    }

    /** @brief Checks if the state machine is currently in the specified state type. */
    template <typename TState>
    bool is_in_state() const noexcept
    {
        static_assert(get_state_index<TState>() != -1, "Queried state type not found in StateMachine");
        return mCurrentStateIndex == get_state_index<TState>();
    }
};

// ====================================================================
// 3. Specialization for StrictTransitionPolicy
// ====================================================================

/**
 * @brief StateMachine specialization with strict transition validation.
 *
 * Only transitions explicitly listed in TransitionList are allowed at runtime.
 */
template <typename Context, typename TransitionList, typename TActionPolicy, size_t InitialIndex, typename... States>
class StateMachine<Context, TransitionList, StrictTransitionPolicy, TActionPolicy, InitialIndex, States...>
{
    using StateTuple = std::tuple<States...>;
    static constexpr size_t NumStates = sizeof...(States);

    // Compile-time validation
    static_assert(NumStates > 0, "StateMachine must have at least one state");
    static_assert(InitialIndex < NumStates, "InitialIndex must be within 0 to NumStates-1");
    static_assert(are_unique<States...>::value, "State types must be unique in the variadic pack");

    // --- State Storage ---
    Context& mContext;
    int mCurrentStateIndex = InitialIndex;

    // --- Compile-Time Index Finder ---
    template <typename TState, std::size_t... I>
    static constexpr int find_index_impl(std::index_sequence<I...>)
    {
        constexpr bool matches[NumStates] = {std::is_same_v<TState, std::tuple_element_t<I, StateTuple>>...};
        for (int j = 0; j < NumStates; ++j)
        {
            if (matches[j])
            {
                return j;
            }
        }
        return -1;
    }

    template <typename TState>
    static constexpr int get_state_index()
    {
        return find_index_impl<TState>(std::make_index_sequence<NumStates>{});
    }

    // --- Transition Matrix Building (Optimized with single index sequence) ---
    template <typename List, std::size_t... Idx>
    static constexpr auto build_transition_matrix_impl(std::index_sequence<Idx...>)
    {
        std::array<std::array<bool, NumStates>, NumStates> matrix{};
        (void((matrix[get_state_index<typename std::tuple_element_t<Idx, List>::first_type>()]
                     [get_state_index<typename std::tuple_element_t<Idx, List>::second_type>()] = true)),
         ...);
        return matrix;
    }

    template <typename List>
    static constexpr auto build_transition_matrix()
    {
        return build_transition_matrix_impl<List>(std::make_index_sequence<std::tuple_size_v<List>>{});
    }

    static constexpr auto transition_matrix = build_transition_matrix<TransitionList>();

    // --- Recursive Dispatchers ---
    template <int I = 0>
    void dispatch_exit_rec(int targetIndex)
    {
        if constexpr (I < NumStates)
        {
            if (targetIndex == I)
            {
                std::tuple_element_t<I, StateTuple>{}.on_exit(mContext);
            }
            dispatch_exit_rec<I + 1>(targetIndex);
        }
    }

    void dispatch_exit_action(int targetIndex)
    {
        dispatch_exit_rec(targetIndex);
    }

    template <int I = 0>
    void dispatch_entry_rec(int targetIndex)
    {
        if constexpr (I < NumStates)
        {
            if (targetIndex == I)
            {
                std::tuple_element_t<I, StateTuple>{}.on_entry(mContext);
            }
            dispatch_entry_rec<I + 1>(targetIndex);
        }
    }

    void dispatch_entry_action(int targetIndex)
    {
        dispatch_entry_rec(targetIndex);
    }

public:
    StateMachine(Context& context)
        : mContext(context)
        , mCurrentStateIndex(InitialIndex)
    {
        // Enforce action policies on all states at compile time
        (ActionPolicyEnforcer<TActionPolicy, States, Context>::validate(), ...);
        // Enter the initial state
        dispatch_entry_action(InitialIndex);
    }

    /**
     * @brief Initiates a transition to the specified state type.
     *
     * @tparam TNextState The target state type (must be in the States pack).
     * @throws std::runtime_error if transition is not in TransitionList.
     */
    template <typename TNextState>
    void transition()
    {
        constexpr int nextIndex = get_state_index<TNextState>();
        const int currentIndex = mCurrentStateIndex;

        static_assert(nextIndex != -1, "Target state type not found in the StateMachine definition");

        if (currentIndex == nextIndex)
        {
            // No action needed for self-transition
            return;
        }

        // Runtime Transition Validation
        if (!transition_matrix[currentIndex][nextIndex])
        {
            throw std::runtime_error("CTSM Error: Transition is not valid under StrictTransitionPolicy");
        }

        // Exit Action
        dispatch_exit_action(currentIndex);

        // Update State
        mCurrentStateIndex = nextIndex;

        // Entry Action
        dispatch_entry_action(nextIndex);
    }

    /** @brief Gets the runtime index of the current state. */
    int current_state_index() const noexcept
    {
        return mCurrentStateIndex;
    }

    /** @brief Checks if the state machine is currently in the specified state type. */
    template <typename TState>
    bool is_in_state() const noexcept
    {
        static_assert(get_state_index<TState>() != -1, "Queried state type not found in StateMachine");
        return mCurrentStateIndex == get_state_index<TState>();
    }
};

} // namespace fat_p
