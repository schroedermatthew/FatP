#pragma once

/*
FATP_META:
  meta_version: 1
  component: StateMachine
  file_role: public_header
  path: include/fat_p/StateMachine.h
  namespace: fat_p
  layer: Domain
  summary: "Public header for StateMachine."
  api_stability: stable
  related:
    docs_search: "StateMachine"
    tests:
      - components/StateMachine/tests/test_StateMachine.cpp
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

/**
 * @file StateMachine.h
 * @brief Type-safe finite state machine with compile-time validation.
 *
 * @details
 * This state machine uses a runtime state index and a compile-time state set.
 * Entry/exit hooks are invoked on default-constructed temporary state objects.
 * Store persistent data in Context.
 *
 * Self-transition requests (transitioning to the current state type) are a
 * no-op: no validation is performed and no hooks are invoked.
 *
 * @note Complexity: transition() is O(1) for both AnyToAny and Strict policies.
 * @note Thread-safety: Not thread-safe. All access must be externally
 *       synchronized, including access to the Context object.
 * @note Reentrancy: Do not call transition() from within on_entry/on_exit hooks.
 *       Debug builds detect reentrant calls and fail-fast (throw or terminate).
 *       Release builds have undefined behavior for reentrant transitions.
 * @note Destruction: The destructor does NOT invoke the current state's
 *       on_exit() hook. If cleanup is required, explicitly transition to a
 *       terminal state before destruction, or perform cleanup in Context's
 *       destructor.
 * @note Exception safety (ThrowingActionPolicy):
 *       - If the exit hook throws, the state index remains unchanged.
 *       - If the entry hook throws, the state index is updated to the target
 *         index. The machine is considered to be in the target state; entry
 *         may be retried by user logic.
 */

#include <array>
#include <concepts>
#include <cstddef>
#include <exception>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>

namespace fat_p
{
// ============================================================================
// 1. Policies
// ============================================================================

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

namespace detail
{

// ============================================================================
// 2. Concepts and Type Traits
// ============================================================================

// --- State Contract Concepts (C++20) ---
template <typename TState, typename TContext>
concept HasEntryExit = requires(TState& s, TContext& ctx)
{
    { s.on_entry(ctx) } -> std::same_as<void>;
    { s.on_exit(ctx) } -> std::same_as<void>;
};

template <typename TState, typename TContext>
concept HasNoexceptEntryExit = requires(TState& s, TContext& ctx)
{
    { s.on_entry(ctx) } noexcept -> std::same_as<void>;
    { s.on_exit(ctx) } noexcept -> std::same_as<void>;
};

// --- Unique States Checker ---
template <typename... Ts>
struct AreUnique;

template <>
struct AreUnique<> : std::true_type
{
};

template <typename T, typename... Ts>
struct AreUnique<T, Ts...>
    : std::conjunction<std::negation<std::disjunction<std::is_same<T, Ts>...>>, AreUnique<Ts...>>
{
};

// --- Policy detection ---
template <typename T>
inline constexpr bool is_strict_policy_v = std::is_same_v<T, StrictTransitionPolicy>;

template <typename T>
inline constexpr bool is_any_to_any_policy_v = std::is_same_v<T, AnyToAnyTransitionPolicy>;

// ============================================================================
// 3. Transition Matrix Builder (Only instantiated for StrictTransitionPolicy)
// ============================================================================

template <typename StateTuple, std::size_t NumStates>
struct TransitionMatrixBuilder
{
    template <typename TState, std::size_t... I>
    static consteval int findStateIndexImpl(std::index_sequence<I...>)
    {
        constexpr std::array<bool, NumStates> kMatches =
            {std::is_same_v<TState, std::tuple_element_t<I, StateTuple>>...};
        for (std::size_t j = 0; j < NumStates; ++j)
        {
            if (kMatches[j])
            {
                return static_cast<int>(j);
            }
        }
        return -1;
    }

    template <typename TState>
    static consteval int getStateIndex()
    {
        return findStateIndexImpl<TState>(std::make_index_sequence<NumStates>{});
    }

    template <typename TPair>
    static consteval bool isTransitionPairValid()
    {
        using From = typename TPair::first_type;
        using To = typename TPair::second_type;
        return (getStateIndex<From>() != -1) && (getStateIndex<To>() != -1);
    }

    template <typename List, std::size_t... Idx>
    static consteval bool areTransitionTypesValidImpl(std::index_sequence<Idx...>)
    {
        return (isTransitionPairValid<std::tuple_element_t<Idx, List>>() && ...);
    }

    template <typename TransitionList>
    static consteval bool areTransitionTypesValid()
    {
        return areTransitionTypesValidImpl<TransitionList>(
            std::make_index_sequence<std::tuple_size_v<TransitionList>>{});
    }

    template <typename TPair>
    static consteval void setTransition(std::array<std::array<bool, NumStates>, NumStates>& matrix)
    {
        constexpr std::size_t from_index =
            static_cast<std::size_t>(getStateIndex<typename TPair::first_type>());
        constexpr std::size_t to_index =
            static_cast<std::size_t>(getStateIndex<typename TPair::second_type>());
        matrix[from_index][to_index] = true;
    }

    template <typename List, std::size_t... Idx>
    static consteval auto buildMatrixImpl(std::index_sequence<Idx...>)
    {
        std::array<std::array<bool, NumStates>, NumStates> matrix{};
        (setTransition<std::tuple_element_t<Idx, List>>(matrix), ...);
        return matrix;
    }

    template <typename TransitionList>
    static consteval auto buildMatrix()
    {
        return buildMatrixImpl<TransitionList>(
            std::make_index_sequence<std::tuple_size_v<TransitionList>>{});
    }
};

} // namespace detail

// ============================================================================
// 4. StateMachine Class Template (Unified)
// ============================================================================

/**
 * @brief A zero-overhead, compile-time defined state machine based on policies.
 *
 * @tparam Context The mutable data object shared by all states.
 * @tparam TransitionList A std::tuple of std::pair<FromState, ToState> for allowed transitions.
 *         Used by StrictTransitionPolicy; ignored by AnyToAnyTransitionPolicy.
 * @tparam TTransitionPolicy StrictTransitionPolicy or AnyToAnyTransitionPolicy.
 * @tparam TActionPolicy NoExceptActionPolicy or ThrowingActionPolicy.
 * @tparam InitialIndex The index of the initial state (default: 0).
 * @tparam States A variadic pack of state types.
 *
 * @note Memory (StrictTransitionPolicy): Uses a compile-time transition matrix of size
 *       O(N^2) where N = sizeof...(States). For 32 states, this is 1024 bytes. Consider
 *       AnyToAnyTransitionPolicy if memory is constrained and all transitions are valid
 *       by design.
 */
template <typename Context,
          typename TransitionList,
          typename TTransitionPolicy,
          typename TActionPolicy,
          std::size_t InitialIndex = 0,
          typename... States>
class StateMachine
{
    // --- Type aliases ---
    using StateTuple = std::tuple<States...>;
    static constexpr std::size_t kNumStates = sizeof...(States);
    static constexpr bool kNoexceptActions = std::is_same_v<TActionPolicy, NoExceptActionPolicy>;
    static constexpr bool kStrictPolicy = detail::is_strict_policy_v<TTransitionPolicy>;

    using ActionFn = std::conditional_t<kNoexceptActions, void (*)(Context&) noexcept, void (*)(Context&)>;
    using MatrixBuilder = detail::TransitionMatrixBuilder<StateTuple, kNumStates>;

    // --- Compile-time policy validation ---
    static_assert(detail::is_any_to_any_policy_v<TTransitionPolicy> ||
                      detail::is_strict_policy_v<TTransitionPolicy>,
                  "TTransitionPolicy must be StrictTransitionPolicy or AnyToAnyTransitionPolicy");
    static_assert(std::is_same_v<TActionPolicy, NoExceptActionPolicy> ||
                      std::is_same_v<TActionPolicy, ThrowingActionPolicy>,
                  "TActionPolicy must be NoExceptActionPolicy or ThrowingActionPolicy");

    // --- Compile-time state validation ---
    static_assert(kNumStates > 0, "StateMachine must have at least one state");
    static_assert(InitialIndex < kNumStates, "InitialIndex must be within 0 to kNumStates-1");
    static_assert(detail::AreUnique<States...>::value, "State types must be unique in the variadic pack");
    static_assert((std::is_default_constructible_v<States> && ...),
                  "CTSM Error: All state types must be default-constructible "
                  "(hooks are invoked on TState{})");
    static_assert(!kNoexceptActions || (std::is_nothrow_default_constructible_v<States> && ...),
                  "CTSM Error: NoExceptActionPolicy requires all state types to be "
                  "nothrow default-constructible (hooks are invoked on TState{} inside "
                  "noexcept wrappers)");
    static_assert((detail::HasEntryExit<States, Context> && ...),
                  "CTSM Error: All state types must provide "
                  "void on_entry(Context&) and void on_exit(Context&)");
    static_assert(!kNoexceptActions || (detail::HasNoexceptEntryExit<States, Context> && ...),
                  "CTSM Error: NoExceptActionPolicy requires "
                  "on_entry and on_exit to be noexcept");

    // --- Transition list validation (StrictTransitionPolicy only) ---
    static_assert(!kStrictPolicy || MatrixBuilder::template areTransitionTypesValid<TransitionList>(),
                  "TransitionList contains a state type not present in the StateMachine definition");

    // --- State storage ---
    Context& mContext;
    std::size_t mCurrentStateIndex = InitialIndex;

#ifndef NDEBUG
    bool mInTransition = false;
#endif

    // --- Compile-time index finder ---
    template <typename TState, std::size_t... I>
    static consteval int findStateIndexImpl(std::index_sequence<I...>)
    {
        constexpr std::array<bool, kNumStates> kMatches =
            {std::is_same_v<TState, std::tuple_element_t<I, StateTuple>>...};
        for (std::size_t j = 0; j < kNumStates; ++j)
        {
            if (kMatches[j])
            {
                return static_cast<int>(j);
            }
        }
        return -1;
    }

    template <typename TState>
    static consteval int getStateIndex()
    {
        return findStateIndexImpl<TState>(std::make_index_sequence<kNumStates>{});
    }

    // --- Action dispatch tables ---
    template <typename TState>
    static void entryAction(Context& ctx) noexcept(kNoexceptActions)
    {
        TState{}.on_entry(ctx);
    }

    template <typename TState>
    static void exitAction(Context& ctx) noexcept(kNoexceptActions)
    {
        TState{}.on_exit(ctx);
    }

    static constexpr std::array<ActionFn, kNumStates> kEntryActions = {&entryAction<States>...};
    static constexpr std::array<ActionFn, kNumStates> kExitActions = {&exitAction<States>...};

    void dispatchExitAction(std::size_t stateIndex) noexcept(kNoexceptActions)
    {
        kExitActions[stateIndex](mContext);
    }

    void dispatchEntryAction(std::size_t stateIndex) noexcept(kNoexceptActions)
    {
        kEntryActions[stateIndex](mContext);
    }

    // --- Transition matrix (only instantiated for StrictTransitionPolicy) ---
    struct TransitionMatrixHolder
    {
        static constexpr auto kMatrix = MatrixBuilder::template buildMatrix<TransitionList>();
    };

    // Dummy for AnyToAny to avoid unnecessary matrix instantiation
    struct NoMatrixHolder
    {
    };

    using MatrixHolder = std::conditional_t<kStrictPolicy, TransitionMatrixHolder, NoMatrixHolder>;

    // Helper to check if transition is allowed at compile time
    template <typename TFrom, typename TTo>
    static consteval bool checkTransitionAllowed()
    {
        constexpr int kFromIndex = getStateIndex<TFrom>();
        constexpr int kToIndex = getStateIndex<TTo>();

        static_assert(kFromIndex != -1, "Source state type not found in the StateMachine definition");
        static_assert(kToIndex != -1, "Target state type not found in the StateMachine definition");

        if constexpr (std::is_same_v<TFrom, TTo>)
        {
            return true; // Self-transitions always allowed (no-op)
        }
        else if constexpr (kStrictPolicy)
        {
            return TransitionMatrixHolder::kMatrix[static_cast<std::size_t>(kFromIndex)]
                                                  [static_cast<std::size_t>(kToIndex)];
        }
        else
        {
            return true; // AnyToAny allows all transitions
        }
    }

public:
    // --- Constructor ---
    explicit StateMachine(Context& context)
        : mContext(context)
        , mCurrentStateIndex(InitialIndex)
    {
        dispatchEntryAction(InitialIndex);
    }

    ~StateMachine() = default;

    StateMachine(const StateMachine&) = delete;
    StateMachine& operator=(const StateMachine&) = delete;
    StateMachine(StateMachine&&) = delete;
    StateMachine& operator=(StateMachine&&) = delete;

    // --- Compile-Time Introspection ---

    /**
     * @brief Checks at compile time whether TState is in the state machine.
     * @tparam TState The state type to check.
     */
    template <typename TState>
    static constexpr bool contains_state = (getStateIndex<TState>() != -1);

    /**
     * @brief Checks at compile time whether a transition is allowed.
     * @tparam TFrom The source state type.
     * @tparam TTo The target state type.
     *
     * For StrictTransitionPolicy, returns true only if the transition is in TransitionList.
     * For AnyToAnyTransitionPolicy, always returns true.
     * Self-transitions are always allowed (they are defined as a no-op).
     */
    template <typename TFrom, typename TTo>
    static constexpr bool is_transition_allowed = checkTransitionAllowed<TFrom, TTo>();

    /**
     * @brief Gets the number of states in this state machine.
     * @note Complexity: O(1).
     */
    [[nodiscard]] static constexpr std::size_t stateCount() noexcept
    {
        return kNumStates;
    }

    /**
     * @brief Gets the index of the initial state.
     * @note Complexity: O(1).
     */
    [[nodiscard]] static constexpr std::size_t initialStateIndex() noexcept
    {
        return InitialIndex;
    }

    /**
     * @brief Initiates a transition to the specified state type.
     *
     * @tparam TNextState The target state type (must be in the States pack).
     * @throws std::runtime_error (StrictTransitionPolicy only) if transition is not in TransitionList.
     *
     * @note Complexity: O(1).
     * @note Thread-safety: Not thread-safe.
     * @note Reentrancy: Do not call from within on_entry/on_exit hooks.
     *       Debug builds detect and fail-fast; Release behavior is undefined.
     * @note Invalid transitions (StrictTransitionPolicy) are rejected before running any hooks.
     */
    template <typename TNextState>
    void transition() noexcept(kNoexceptActions && !kStrictPolicy)
    {
        constexpr int kNextIndex = getStateIndex<TNextState>();
        static_assert(kNextIndex != -1, "Target state type not found in the StateMachine definition");

        const std::size_t currentIndex = mCurrentStateIndex;
        const std::size_t nextStateIndex = static_cast<std::size_t>(kNextIndex);

#ifndef NDEBUG
        // Reentrancy guard - BEFORE self-transition check
        // Even self-transitions from within hooks indicate a logic error
        struct ReentrancyGuard
        {
            bool& mFlag;

            explicit ReentrancyGuard(bool& flag)
                : mFlag(flag)
            {
                if (mFlag)
                {
                    if constexpr (kNoexceptActions)
                    {
                        std::terminate();
                    }
                    else
                    {
                        throw std::runtime_error(
                            "CTSM Error: Reentrant transition detected - "
                            "do not call transition() from within on_entry/on_exit");
                    }
                }
                mFlag = true;
            }

            ~ReentrancyGuard()
            {
                mFlag = false;
            }
        };

        [[maybe_unused]] ReentrancyGuard guard(mInTransition);
#endif

        // Self-transition is a no-op
        if (currentIndex == nextStateIndex)
        {
            return;
        }

        // Runtime transition validation (StrictTransitionPolicy only)
        if constexpr (kStrictPolicy)
        {
            if (!TransitionMatrixHolder::kMatrix[currentIndex][nextStateIndex])
            {
                throw std::runtime_error("CTSM Error: Transition is not valid under StrictTransitionPolicy");
            }
        }

        // Exit current state
        dispatchExitAction(currentIndex);

        // Update state index
        mCurrentStateIndex = nextStateIndex;

        // Enter new state
        dispatchEntryAction(nextStateIndex);
    }

    /**
     * @brief Gets the runtime index of the current state.
     *
     * @note Complexity: O(1).
     * @note Thread-safety: Not thread-safe.
     */
    [[nodiscard]] std::size_t currentStateIndex() const noexcept
    {
        return mCurrentStateIndex;
    }

    /**
     * @brief Checks whether the state machine is currently in the specified state type.
     *
     * @note Complexity: O(1).
     * @note Thread-safety: Not thread-safe.
     */
    template <typename TState>
    [[nodiscard]] bool isInState() const noexcept
    {
        constexpr int kIndex = getStateIndex<TState>();
        static_assert(kIndex != -1, "Queried state type not found in StateMachine");
        return mCurrentStateIndex == static_cast<std::size_t>(kIndex);
    }
};

} // namespace fat_p
