/**
 * @file enforce.h
 * @brief Provides macro implementations for contract enforcement using
 * predefined and generic predicates with various policies.
 * 
 * @section zero_cost_guarantee Zero-Cost Guarantee
 * 
 * All `debug_enforce*` and `enforce` macros are GUARANTEED to generate zero code
 * in release builds (when NDEBUG is defined). This is achieved via `if constexpr`
 * which ensures the enforcement code is never instantiated, not merely "optimized away".
 * 
 * The `always_enforce*` macros always generate code regardless of build mode.
 */
#pragma once
#include <type_traits>
#include <utility>

#include "enforce_enforcers.h"
#include "enforce_predicates.h"
#include "enforce_raiser_selector.h"
#include "enforce_raisers.h"

namespace fat_p {

// --- Locus Macros ---
#ifndef FATP_LOCUS
#define FATP_LOCUS __FILE__ ":" FATP_STRINGIFY(__LINE__)
#define FATP_STRINGIFY(x) FATP_TOSTRING(x)
#define FATP_TOSTRING(x) #x
#endif

// ============================================================================
// Compile-Time Enforcement Control
// ============================================================================
// This flag determines whether debug-only enforcement is instantiated at all.
// Using if constexpr with this flag GUARANTEES zero codegen in release builds -
// the enforcement code is never instantiated, not just "optimized away".
// This is a language-level guarantee, not compiler-dependent optimization.

#ifdef NDEBUG
inline constexpr bool FATP_DEBUG_ENFORCE_ENABLED = false;
#else
inline constexpr bool FATP_DEBUG_ENFORCE_ENABLED = true;
#endif

// ============================================================================
// Core Enforcement Functions
// ============================================================================

template <typename Policy>
[[nodiscard]] auto enforce_policy_impl(bool passed, const char* expression_str, const char* locus)
{
    using Raiser = typename RaiserSelector<Policy>::type;
    return MakeEnforcer<Raiser>(passed, expression_str, locus);
}

// ============================================================================
// Debug-Only Enforcement Helpers (Guaranteed Zero-Cost in Release)
// ============================================================================
// These templates use if constexpr to ensure NO instantiation occurs in release.
// The discarded branch is never compiled - this is mandated by the C++ standard.

// Basic condition enforcement
template <typename... Msgs>
inline void debug_enforce_impl(
    [[maybe_unused]] bool condition,
    [[maybe_unused]] const char* expression_str,
    [[maybe_unused]] const char* locus,
    [[maybe_unused]] Msgs&&... msgs)
{
    if constexpr (FATP_DEBUG_ENFORCE_ENABLED)
    {
        auto enforcer = enforce_policy_impl<DebugOnlyPolicy>(condition, expression_str, locus);
        enforcer(std::forward<Msgs>(msgs)...);
    }
}

// 1-argument predicate enforcement
template <typename Predicate, typename T, typename... Msgs>
inline void debug_enforce_predicate_1(
    [[maybe_unused]] T&& target,
    [[maybe_unused]] const char* expression_str,
    [[maybe_unused]] const char* locus,
    [[maybe_unused]] Msgs&&... msgs)
{
    if constexpr (FATP_DEBUG_ENFORCE_ENABLED)
    {
        auto enforcer = enforce_policy_impl<DebugOnlyPolicy>(
            Predicate::check(std::forward<T>(target)), expression_str, locus);
        enforcer(std::forward<Msgs>(msgs)...);
    }
}

// 2-argument predicate enforcement
template <typename Predicate, typename T1, typename T2, typename... Msgs>
inline void debug_enforce_predicate_2(
    [[maybe_unused]] T1&& target1,
    [[maybe_unused]] T2&& target2,
    [[maybe_unused]] const char* expression_str,
    [[maybe_unused]] const char* locus,
    [[maybe_unused]] Msgs&&... msgs)
{
    if constexpr (FATP_DEBUG_ENFORCE_ENABLED)
    {
        auto enforcer = enforce_policy_impl<DebugOnlyPolicy>(
            Predicate::check(std::forward<T1>(target1), std::forward<T2>(target2)),
            expression_str, locus);
        enforcer(std::forward<Msgs>(msgs)...);
    }
}

// 3-argument predicate enforcement
template <typename Predicate, typename T1, typename T2, typename T3, typename... Msgs>
inline void debug_enforce_predicate_3(
    [[maybe_unused]] T1&& target1,
    [[maybe_unused]] T2&& target2,
    [[maybe_unused]] T3&& target3,
    [[maybe_unused]] const char* expression_str,
    [[maybe_unused]] const char* locus,
    [[maybe_unused]] Msgs&&... msgs)
{
    if constexpr (FATP_DEBUG_ENFORCE_ENABLED)
    {
        auto enforcer = enforce_policy_impl<DebugOnlyPolicy>(
            Predicate::check(std::forward<T1>(target1), std::forward<T2>(target2),
                             std::forward<T3>(target3)),
            expression_str, locus);
        enforcer(std::forward<Msgs>(msgs)...);
    }
}

// Sorted container with comparator enforcement
template <typename Comp, typename Container, typename... Msgs>
inline void debug_enforce_sorted_with_impl(
    [[maybe_unused]] Comp&& comp,
    [[maybe_unused]] Container&& container,
    [[maybe_unused]] const char* expression_str,
    [[maybe_unused]] const char* locus,
    [[maybe_unused]] Msgs&&... msgs)
{
    if constexpr (FATP_DEBUG_ENFORCE_ENABLED)
    {
        auto enforcer = enforce_policy_impl<DebugOnlyPolicy>(
            IsSortedPredicate::check(std::forward<Container>(container), std::forward<Comp>(comp)),
            expression_str, locus);
        enforcer(std::forward<Msgs>(msgs)...);
    }
}

// ============================================================================
// General Condition Macros
// ============================================================================

// enforce() - Debug-only contract enforcement
// GUARANTEE: Zero codegen in release builds (NDEBUG defined).
// Uses preprocessor elimination for absolute guarantee - the macro expands to
// nothing, so no condition evaluation, no function calls, no string literals.
#ifdef NDEBUG
#define enforce(condition, ...) ((void)0)
#else
#define enforce(condition, ...)                                                                    \
    fat_p::debug_enforce_impl((condition), #condition, FATP_LOCUS, ##__VA_ARGS__)
#endif

#define always_enforce(condition, ...)                                                             \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::AlwaysEnforcePolicy>(                    \
            (condition), #condition, FATP_LOCUS);                                                  \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#define enforce_warn(condition, ...)                                                               \
    do                                                                                             \
    {                                                                                              \
        auto enforcer =                                                                            \
            fat_p::enforce_policy_impl<fat_p::WarningPolicy>((condition), #condition, FATP_LOCUS); \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#define noexcept_enforce(condition, ...)                                                           \
    do                                                                                             \
    {                                                                                              \
        auto enforcer =                                                                            \
            fat_p::enforce_policy_impl<fat_p::NoThrowPolicy>((condition), #condition, FATP_LOCUS); \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#define abort_enforce(condition, ...)                                                              \
    do                                                                                             \
    {                                                                                              \
        auto enforcer =                                                                            \
            fat_p::enforce_policy_impl<fat_p::AbortPolicy>((condition), #condition, FATP_LOCUS);   \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

// ============================================================================
// Expected Integration Macros
// ============================================================================

#define enforce_expected(condition, ...)                                                           \
    ([&]() -> fat_p::Expected<void, std::string> {                                                 \
        if (!(condition))                                                                          \
        {                                                                                          \
            fat_p::MessageBuilder mb;                                                              \
            mb.format(__VA_ARGS__);                                                                \
            std::string msg = mb.get_message(FATP_LOCUS, #condition);                              \
            fat_p::detail::writeToStderr("Expected Failure: ", msg);                               \
            return fat_p::make_unexpected(msg);                                                    \
        }                                                                                          \
        return {};                                                                                 \
    })()

#define always_enforce_expected(condition, ...)                                                    \
    ([&]() -> fat_p::Expected<void, std::string> {                                                 \
        if (!(condition))                                                                          \
        {                                                                                          \
            fat_p::MessageBuilder mb;                                                              \
            mb.format(__VA_ARGS__);                                                                \
            std::string msg = mb.get_message(FATP_LOCUS, #condition);                              \
            fat_p::detail::writeToStderr("Expected Failure: ", msg);                               \
            return fat_p::make_unexpected(msg);                                                    \
        }                                                                                          \
        return {};                                                                                 \
    })()

#define enforce_predicate_expected(PredicateType, target, ...)                                     \
    ([&]() -> fat_p::Expected<bool, std::string> {                                                 \
        auto result = PredicateType::check(target);                                                \
        if (!result)                                                                               \
        {                                                                                          \
            fat_p::MessageBuilder mb;                                                              \
            mb.format(__VA_ARGS__);                                                                \
            std::string msg = mb.get_message(FATP_LOCUS, #PredicateType "(" #target ")");          \
            fat_p::detail::writeToStderr("Expected Failure: ", msg);                               \
            return fat_p::make_unexpected(msg);                                                    \
        }                                                                                          \
        return result;                                                                             \
    })()

// ============================================================================
// Generic Arity-Based Macros - Always Enforce
// ============================================================================

#define always_enforce_1(PredicateType, target, ...)                                               \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::AlwaysEnforcePolicy>(                    \
            fat_p::PredicateType::check(target), #PredicateType "(" #target ")", FATP_LOCUS);      \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#define always_enforce_2(PredicateType, target1, target2, ...)                                     \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::AlwaysEnforcePolicy>(                    \
            fat_p::PredicateType::check(target1, target2),                                         \
            #PredicateType "(" #target1 ", " #target2 ")",                                         \
            FATP_LOCUS);                                                                           \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#define always_enforce_3(PredicateType, target1, target2, target3, ...)                            \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::AlwaysEnforcePolicy>(                    \
            fat_p::PredicateType::check(target1, target2, target3),                                \
            #PredicateType "(" #target1 ", " #target2 ", " #target3 ")",                           \
            FATP_LOCUS);                                                                           \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

// ============================================================================
// Generic Arity-Based Macros - Debug Only (with aliases)
// GUARANTEE: Zero codegen in release builds (NDEBUG defined).
// Uses preprocessor elimination for absolute guarantee.
// ============================================================================

#ifdef NDEBUG
#define debug_enforce_1(PredicateType, target, ...) ((void)0)
#define debug_enforce_2(PredicateType, target1, target2, ...) ((void)0)
#define debug_enforce_3(PredicateType, target1, target2, target3, ...) ((void)0)
#else
#define debug_enforce_1(PredicateType, target, ...)                                                \
    fat_p::debug_enforce_predicate_1<fat_p::PredicateType>(                                        \
        target, #PredicateType "(" #target ")", FATP_LOCUS, ##__VA_ARGS__)

#define debug_enforce_2(PredicateType, target1, target2, ...)                                      \
    fat_p::debug_enforce_predicate_2<fat_p::PredicateType>(                                        \
        target1, target2, #PredicateType "(" #target1 ", " #target2 ")", FATP_LOCUS, ##__VA_ARGS__)

#define debug_enforce_3(PredicateType, target1, target2, target3, ...)                             \
    fat_p::debug_enforce_predicate_3<fat_p::PredicateType>(                                        \
        target1, target2, target3,                                                                 \
        #PredicateType "(" #target1 ", " #target2 ", " #target3 ")", FATP_LOCUS, ##__VA_ARGS__)
#endif

// Aliases for enforce_N (debug-only variants)
#define enforce_1 debug_enforce_1
#define enforce_2 debug_enforce_2
#define enforce_3 debug_enforce_3

// ============================================================================
// Generic Arity-Based Macros - NoThrow
// ============================================================================

#define noexcept_enforce_1(PredicateType, target, ...)                                             \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::NoThrowPolicy>(                          \
            fat_p::PredicateType::check(target), #PredicateType "(" #target ")", FATP_LOCUS);      \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#define noexcept_enforce_2(PredicateType, target1, target2, ...)                                   \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::NoThrowPolicy>(                          \
            fat_p::PredicateType::check(target1, target2),                                         \
            #PredicateType "(" #target1 ", " #target2 ")",                                         \
            FATP_LOCUS);                                                                           \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#define noexcept_enforce_3(PredicateType, target1, target2, target3, ...)                          \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::NoThrowPolicy>(                          \
            fat_p::PredicateType::check(target1, target2, target3),                                \
            #PredicateType "(" #target1 ", " #target2 ", " #target3 ")",                           \
            FATP_LOCUS);                                                                           \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

// ============================================================================
// Generic Arity-Based Macros - Abort
// ============================================================================

#define abort_enforce_1(PredicateType, target, ...)                                                \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::AbortPolicy>(                            \
            fat_p::PredicateType::check(target), #PredicateType "(" #target ")", FATP_LOCUS);      \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#define abort_enforce_2(PredicateType, target1, target2, ...)                                      \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::AbortPolicy>(                            \
            fat_p::PredicateType::check(target1, target2),                                         \
            #PredicateType "(" #target1 ", " #target2 ")",                                         \
            FATP_LOCUS);                                                                           \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#define abort_enforce_3(PredicateType, target1, target2, target3, ...)                             \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::AbortPolicy>(                            \
            fat_p::PredicateType::check(target1, target2, target3),                                \
            #PredicateType "(" #target1 ", " #target2 ", " #target3 ")",                           \
            FATP_LOCUS);                                                                           \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

// ============================================================================
// Generic Arity-Based Macros - Warning
// ============================================================================

#define warn_enforce_1(PredicateType, target, ...)                                                 \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::WarningPolicy>(                          \
            fat_p::PredicateType::check(target), #PredicateType "(" #target ")", FATP_LOCUS);      \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#define warn_enforce_2(PredicateType, target1, target2, ...)                                       \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::WarningPolicy>(                          \
            fat_p::PredicateType::check(target1, target2),                                         \
            #PredicateType "(" #target1 ", " #target2 ")",                                         \
            FATP_LOCUS);                                                                           \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#define warn_enforce_3(PredicateType, target1, target2, target3, ...)                              \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::WarningPolicy>(                          \
            fat_p::PredicateType::check(target1, target2, target3),                                \
            #PredicateType "(" #target1 ", " #target2 ", " #target3 ")",                           \
            FATP_LOCUS);                                                                           \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

// ============================================================================
// Specific Predicate Convenience Macros - AllSatisfy
// ============================================================================

// Release build: All debug_enforce_* macros expand to nothing
#ifdef NDEBUG
#define debug_enforce_all_satisfy(pred, container, ...) ((void)0)
#define debug_enforce_any_satisfy(pred, container, ...) ((void)0)
#define debug_enforce_has_size(expected_size, container, ...) ((void)0)
#define debug_enforce_is_unique(container, ...) ((void)0)
#define debug_enforce_approx_equal(epsilon, a, b, ...) ((void)0)
#define debug_enforce_in_range(min, max, value, ...) ((void)0)
#define debug_enforce_valid_index(idx, container, ...) ((void)0)
#define debug_enforce_is_integral(value, ...) ((void)0)
#define debug_enforce_is_non_negative(value, ...) ((void)0)
#define debug_enforce_is_positive(value, ...) ((void)0)
#define debug_enforce_is_power_of_two(value, ...) ((void)0)
#define debug_enforce_is_sorted(container, ...) ((void)0)
#define debug_enforce_is_sorted_with(comp, container, ...) ((void)0)
#define debug_enforce_is_valid_iterator(it, end, ...) ((void)0)
#define debug_enforce_not_empty(value, ...) ((void)0)
#define debug_enforce_not_null(ptr, ...) ((void)0)
#define debug_enforce_is_finite(value, ...) ((void)0)
#define debug_enforce_is_normal(value, ...) ((void)0)
#endif

#define always_enforce_all_satisfy(pred, container, ...)                                           \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::AlwaysEnforcePolicy>(                    \
            fat_p::AllSatisfyPredicate::check(pred, container),                                    \
            "all_satisfy(" #pred ", " #container ")",                                              \
            FATP_LOCUS);                                                                           \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#ifndef NDEBUG
#define debug_enforce_all_satisfy(pred, container, ...)                                            \
    fat_p::debug_enforce_predicate_2<fat_p::AllSatisfyPredicate>(                                  \
        pred, container, "all_satisfy(" #pred ", " #container ")", FATP_LOCUS, ##__VA_ARGS__)
#endif

// ============================================================================
// Specific Predicate Convenience Macros - AnySatisfy
// ============================================================================

#define always_enforce_any_satisfy(pred, container, ...)                                           \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::AlwaysEnforcePolicy>(                    \
            fat_p::AnySatisfyPredicate::check(pred, container),                                    \
            "any_satisfy(" #pred ", " #container ")",                                              \
            FATP_LOCUS);                                                                           \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#ifndef NDEBUG
#define debug_enforce_any_satisfy(pred, container, ...)                                            \
    fat_p::debug_enforce_predicate_2<fat_p::AnySatisfyPredicate>(                                  \
        pred, container, "any_satisfy(" #pred ", " #container ")", FATP_LOCUS, ##__VA_ARGS__)
#endif

// ============================================================================
// Specific Predicate Convenience Macros - HasSize
// ============================================================================

#define always_enforce_has_size(expected_size, container, ...)                                     \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::AlwaysEnforcePolicy>(                    \
            fat_p::HasSizePredicate::check(expected_size, container),                              \
            "has_size(" #expected_size ", " #container ")",                                        \
            FATP_LOCUS);                                                                           \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#ifndef NDEBUG
#define debug_enforce_has_size(expected_size, container, ...)                                      \
    fat_p::debug_enforce_predicate_2<fat_p::HasSizePredicate>(                                     \
        expected_size, container, "has_size(" #expected_size ", " #container ")",                  \
        FATP_LOCUS, ##__VA_ARGS__)
#endif

// ============================================================================
// Specific Predicate Convenience Macros - ContainerIsUnique
// ============================================================================

#define always_enforce_is_unique(container, ...)                                                   \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::AlwaysEnforcePolicy>(                    \
            fat_p::ContainerIsUniquePredicate::check(container),                                   \
            "is_unique(" #container ")",                                                           \
            FATP_LOCUS);                                                                           \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#ifndef NDEBUG
#define debug_enforce_is_unique(container, ...)                                                    \
    fat_p::debug_enforce_predicate_1<fat_p::ContainerIsUniquePredicate>(                           \
        container, "is_unique(" #container ")", FATP_LOCUS, ##__VA_ARGS__)
#endif

// ============================================================================
// Specific Predicate Convenience Macros - ApproxEqual
// ============================================================================

#define always_enforce_approx_equal(epsilon, a, b, ...)                                            \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::AlwaysEnforcePolicy>(                    \
            fat_p::ApproxEqualPredicate::check(epsilon, a, b),                                     \
            "approx_equal(" #epsilon ", " #a ", " #b ")",                                          \
            FATP_LOCUS);                                                                           \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#ifndef NDEBUG
#define debug_enforce_approx_equal(epsilon, a, b, ...)                                             \
    fat_p::debug_enforce_predicate_3<fat_p::ApproxEqualPredicate>(                                 \
        epsilon, a, b, "approx_equal(" #epsilon ", " #a ", " #b ")", FATP_LOCUS, ##__VA_ARGS__)
#endif

// ============================================================================
// Specific Predicate Convenience Macros - InRange
// ============================================================================

#define always_enforce_in_range(min, max, value, ...)                                              \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::AlwaysEnforcePolicy>(                    \
            fat_p::InRangePredicate::check(value, min, max),                                       \
            "in_range(" #min ", " #max ", " #value ")",                                            \
            FATP_LOCUS);                                                                           \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#ifndef NDEBUG
#define debug_enforce_in_range(min, max, value, ...)                                               \
    fat_p::debug_enforce_predicate_3<fat_p::InRangePredicate>(                                     \
        value, min, max, "in_range(" #min ", " #max ", " #value ")", FATP_LOCUS, ##__VA_ARGS__)
#endif

// ============================================================================
// Specific Predicate Convenience Macros - ValidIndex
// ============================================================================

#define always_enforce_valid_index(idx, container, ...)                                            \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::AlwaysEnforcePolicy>(                    \
            fat_p::ValidIndexPredicate::check(idx, container),                                     \
            "valid_index(" #idx ", " #container ")",                                               \
            FATP_LOCUS);                                                                           \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#ifndef NDEBUG
#define debug_enforce_valid_index(idx, container, ...)                                             \
    fat_p::debug_enforce_predicate_2<fat_p::ValidIndexPredicate>(                                  \
        idx, container, "valid_index(" #idx ", " #container ")", FATP_LOCUS, ##__VA_ARGS__)
#endif

// ============================================================================
// Specific Predicate Convenience Macros - IsIntegral
// ============================================================================

#define always_enforce_is_integral(value, ...)                                                     \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::AlwaysEnforcePolicy>(                    \
            fat_p::IsIntegralPredicate::check(value), "is_integral(" #value ")", FATP_LOCUS);      \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#ifndef NDEBUG
#define debug_enforce_is_integral(value, ...)                                                      \
    fat_p::debug_enforce_predicate_1<fat_p::IsIntegralPredicate>(                                  \
        value, "is_integral(" #value ")", FATP_LOCUS, ##__VA_ARGS__)
#endif

// ============================================================================
// Specific Predicate Convenience Macros - IsNonNegative
// ============================================================================

#define always_enforce_is_non_negative(value, ...)                                                 \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::AlwaysEnforcePolicy>(                    \
            fat_p::IsNonNegativePredicate::check(value),                                           \
            "is_non_negative(" #value ")",                                                         \
            FATP_LOCUS);                                                                           \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#ifndef NDEBUG
#define debug_enforce_is_non_negative(value, ...)                                                  \
    fat_p::debug_enforce_predicate_1<fat_p::IsNonNegativePredicate>(                               \
        value, "is_non_negative(" #value ")", FATP_LOCUS, ##__VA_ARGS__)
#endif

// ============================================================================
// Specific Predicate Convenience Macros - IsPositive
// ============================================================================

#define always_enforce_is_positive(value, ...)                                                     \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::AlwaysEnforcePolicy>(                    \
            fat_p::IsPositivePredicate::check(value), "is_positive(" #value ")", FATP_LOCUS);      \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#ifndef NDEBUG
#define debug_enforce_is_positive(value, ...)                                                      \
    fat_p::debug_enforce_predicate_1<fat_p::IsPositivePredicate>(                                  \
        value, "is_positive(" #value ")", FATP_LOCUS, ##__VA_ARGS__)
#endif

// ============================================================================
// Specific Predicate Convenience Macros - IsPowerOfTwo
// ============================================================================

#define always_enforce_is_power_of_two(value, ...)                                                 \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::AlwaysEnforcePolicy>(                    \
            fat_p::IsPowerOfTwoPredicate::check(value),                                            \
            "is_power_of_two(" #value ")",                                                         \
            FATP_LOCUS);                                                                           \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#ifndef NDEBUG
#define debug_enforce_is_power_of_two(value, ...)                                                  \
    fat_p::debug_enforce_predicate_1<fat_p::IsPowerOfTwoPredicate>(                                \
        value, "is_power_of_two(" #value ")", FATP_LOCUS, ##__VA_ARGS__)
#endif

// ============================================================================
// Specific Predicate Convenience Macros - IsSorted
// ============================================================================

#define always_enforce_is_sorted(container, ...)                                                   \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::AlwaysEnforcePolicy>(                    \
            fat_p::IsSortedPredicate::check(container), "is_sorted(" #container ")", FATP_LOCUS);  \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#ifndef NDEBUG
#define debug_enforce_is_sorted(container, ...)                                                    \
    fat_p::debug_enforce_predicate_1<fat_p::IsSortedPredicate>(                                    \
        container, "is_sorted(" #container ")", FATP_LOCUS, ##__VA_ARGS__)
#endif

#define always_enforce_is_sorted_with(comp, container, ...)                                        \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::AlwaysEnforcePolicy>(                    \
            fat_p::IsSortedPredicate::check(container, comp),                                      \
            "is_sorted_with(" #comp ", " #container ")",                                           \
            FATP_LOCUS);                                                                           \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#ifndef NDEBUG
#define debug_enforce_is_sorted_with(comp, container, ...)                                         \
    fat_p::debug_enforce_sorted_with_impl(                                                         \
        comp, container, "is_sorted_with(" #comp ", " #container ")", FATP_LOCUS, ##__VA_ARGS__)
#endif

// ============================================================================
// Specific Predicate Convenience Macros - IsValidIterator
// ============================================================================

#define always_enforce_is_valid_iterator(it, end, ...)                                             \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::AlwaysEnforcePolicy>(                    \
            fat_p::IsValidIteratorPredicate::check(it, end),                                       \
            "is_valid_iterator(" #it ", " #end ")",                                                \
            FATP_LOCUS);                                                                           \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#ifndef NDEBUG
#define debug_enforce_is_valid_iterator(it, end, ...)                                              \
    fat_p::debug_enforce_predicate_2<fat_p::IsValidIteratorPredicate>(                             \
        it, end, "is_valid_iterator(" #it ", " #end ")", FATP_LOCUS, ##__VA_ARGS__)
#endif

// ============================================================================
// Specific Predicate Convenience Macros - NotEmpty
// ============================================================================

#define always_enforce_not_empty(value, ...)                                                       \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::AlwaysEnforcePolicy>(                    \
            fat_p::NotEmptyPredicate::check(value), "not_empty(" #value ")", FATP_LOCUS);          \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#ifndef NDEBUG
#define debug_enforce_not_empty(value, ...)                                                        \
    fat_p::debug_enforce_predicate_1<fat_p::NotEmptyPredicate>(                                    \
        value, "not_empty(" #value ")", FATP_LOCUS, ##__VA_ARGS__)
#endif

// ============================================================================
// Specific Predicate Convenience Macros - NotNull (All Policies)
// ============================================================================

#define always_enforce_not_null(ptr, ...)                                                          \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::AlwaysEnforcePolicy>(                    \
            fat_p::NotNullPredicate::check(ptr), "not_null(" #ptr ")", FATP_LOCUS);                \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#ifndef NDEBUG
#define debug_enforce_not_null(ptr, ...)                                                           \
    fat_p::debug_enforce_predicate_1<fat_p::NotNullPredicate>(                                     \
        ptr, "not_null(" #ptr ")", FATP_LOCUS, ##__VA_ARGS__)
#endif

#define warn_enforce_not_null(ptr, ...)                                                            \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::WarningPolicy>(                          \
            fat_p::NotNullPredicate::check(ptr), "not_null(" #ptr ")", FATP_LOCUS);                \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#define noexcept_enforce_not_null(ptr, ...)                                                        \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::NoThrowPolicy>(                          \
            fat_p::NotNullPredicate::check(ptr), "not_null(" #ptr ")", FATP_LOCUS);                \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#define abort_enforce_not_null(ptr, ...)                                                           \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::AbortPolicy>(                            \
            fat_p::NotNullPredicate::check(ptr), "not_null(" #ptr ")", FATP_LOCUS);                \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

// ============================================================================
// Specific Predicate Convenience Macros - IsFinite
// ============================================================================

#define always_enforce_is_finite(value, ...)                                                       \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::AlwaysEnforcePolicy>(                    \
            fat_p::IsFinitePredicate::check(value), "is_finite(" #value ")", FATP_LOCUS);          \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#ifndef NDEBUG
#define debug_enforce_is_finite(value, ...)                                                        \
    fat_p::debug_enforce_predicate_1<fat_p::IsFinitePredicate>(                                    \
        value, "is_finite(" #value ")", FATP_LOCUS, ##__VA_ARGS__)
#endif

// ============================================================================
// Specific Predicate Convenience Macros - IsNormal
// ============================================================================

#define always_enforce_is_normal(value, ...)                                                       \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::AlwaysEnforcePolicy>(                    \
            fat_p::IsNormalPredicate::check(value), "is_normal(" #value ")", FATP_LOCUS);          \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#ifndef NDEBUG
#define debug_enforce_is_normal(value, ...)                                                        \
    fat_p::debug_enforce_predicate_1<fat_p::IsNormalPredicate>(                                    \
        value, "is_normal(" #value ")", FATP_LOCUS, ##__VA_ARGS__)
#endif

} // namespace fat_p
