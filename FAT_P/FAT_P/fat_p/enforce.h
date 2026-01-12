/**
 * @file enforce.h
 * @brief Provides macro implementations for contract enforcement using
 * predefined and generic predicates with various policies.
 * 
 * @section zero_cost_guarantee Zero-Cost Guarantee
 * 
 * All `FATP_DEBUG_ENFORCE*` and `FATP_ENFORCE` macros are GUARANTEED to generate zero code
 * in release builds (when NDEBUG is defined). This is achieved via `if constexpr`
 * which ensures the enforcement code is never instantiated, not merely "optimized away".
 * 
 * The `FATP_ALWAYS_ENFORCE*` macros always generate code regardless of build mode.
 */
#pragma once
/*
FATP_META:
  meta_version: 1
  component: enforce
  file_role: public_header
  path: fat_p/enforce.h
  namespace: fat_p
  summary: "Public header for enforce."
  api_stability: in_work
  related:
    docs_search: "enforce"
    tests:
      - tests/test_Enforce.cpp
  hygiene:
    pragma_once: true
    include_guard: true
    defines_total: 90
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/
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

// FATP_ENFORCE() - Debug-only contract enforcement
// GUARANTEE: Zero codegen in release builds (NDEBUG defined).
// Uses preprocessor elimination for absolute guarantee - the macro expands to
// nothing, so no condition evaluation, no function calls, no string literals.
#ifdef NDEBUG
#define FATP_ENFORCE(condition, ...) ((void)0)
#else
#define FATP_ENFORCE(condition, ...)                                                               \
    fat_p::debug_enforce_impl((condition), #condition, FATP_LOCUS, ##__VA_ARGS__)
#endif

#define FATP_ALWAYS_ENFORCE(condition, ...)                                                        \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::AlwaysEnforcePolicy>(                    \
            (condition), #condition, FATP_LOCUS);                                                  \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#define FATP_ENFORCE_WARN(condition, ...)                                                          \
    do                                                                                             \
    {                                                                                              \
        auto enforcer =                                                                            \
            fat_p::enforce_policy_impl<fat_p::WarningPolicy>((condition), #condition, FATP_LOCUS); \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#define FATP_NOEXCEPT_ENFORCE(condition, ...)                                                      \
    do                                                                                             \
    {                                                                                              \
        auto enforcer =                                                                            \
            fat_p::enforce_policy_impl<fat_p::NoThrowPolicy>((condition), #condition, FATP_LOCUS); \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#define FATP_ABORT_ENFORCE(condition, ...)                                                         \
    do                                                                                             \
    {                                                                                              \
        auto enforcer =                                                                            \
            fat_p::enforce_policy_impl<fat_p::AbortPolicy>((condition), #condition, FATP_LOCUS);   \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

// ============================================================================
// Expected Integration Macros
// ============================================================================

#define FATP_ENFORCE_EXPECTED(condition, ...)                                                      \
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

#define FATP_ALWAYS_ENFORCE_EXPECTED(condition, ...)                                               \
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

#define FATP_ENFORCE_PREDICATE_EXPECTED(PredicateType, target, ...)                                \
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

#define FATP_ALWAYS_ENFORCE_1(PredicateType, target, ...)                                          \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::AlwaysEnforcePolicy>(                    \
            fat_p::PredicateType::check(target), #PredicateType "(" #target ")", FATP_LOCUS);      \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#define FATP_ALWAYS_ENFORCE_2(PredicateType, target1, target2, ...)                                \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::AlwaysEnforcePolicy>(                    \
            fat_p::PredicateType::check(target1, target2),                                         \
            #PredicateType "(" #target1 ", " #target2 ")",                                         \
            FATP_LOCUS);                                                                           \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#define FATP_ALWAYS_ENFORCE_3(PredicateType, target1, target2, target3, ...)                       \
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
#define FATP_DEBUG_ENFORCE_1(PredicateType, target, ...) ((void)0)
#define FATP_DEBUG_ENFORCE_2(PredicateType, target1, target2, ...) ((void)0)
#define FATP_DEBUG_ENFORCE_3(PredicateType, target1, target2, target3, ...) ((void)0)
#else
#define FATP_DEBUG_ENFORCE_1(PredicateType, target, ...)                                           \
    fat_p::debug_enforce_predicate_1<fat_p::PredicateType>(                                        \
        target, #PredicateType "(" #target ")", FATP_LOCUS, ##__VA_ARGS__)

#define FATP_DEBUG_ENFORCE_2(PredicateType, target1, target2, ...)                                 \
    fat_p::debug_enforce_predicate_2<fat_p::PredicateType>(                                        \
        target1, target2, #PredicateType "(" #target1 ", " #target2 ")", FATP_LOCUS, ##__VA_ARGS__)

#define FATP_DEBUG_ENFORCE_3(PredicateType, target1, target2, target3, ...)                        \
    fat_p::debug_enforce_predicate_3<fat_p::PredicateType>(                                        \
        target1, target2, target3,                                                                 \
        #PredicateType "(" #target1 ", " #target2 ", " #target3 ")", FATP_LOCUS, ##__VA_ARGS__)
#endif

// Aliases for FATP_ENFORCE_N (debug-only variants)
#define FATP_ENFORCE_1 FATP_DEBUG_ENFORCE_1
#define FATP_ENFORCE_2 FATP_DEBUG_ENFORCE_2
#define FATP_ENFORCE_3 FATP_DEBUG_ENFORCE_3

// ============================================================================
// Generic Arity-Based Macros - NoThrow
// ============================================================================

#define FATP_NOEXCEPT_ENFORCE_1(PredicateType, target, ...)                                        \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::NoThrowPolicy>(                          \
            fat_p::PredicateType::check(target), #PredicateType "(" #target ")", FATP_LOCUS);      \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#define FATP_NOEXCEPT_ENFORCE_2(PredicateType, target1, target2, ...)                              \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::NoThrowPolicy>(                          \
            fat_p::PredicateType::check(target1, target2),                                         \
            #PredicateType "(" #target1 ", " #target2 ")",                                         \
            FATP_LOCUS);                                                                           \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#define FATP_NOEXCEPT_ENFORCE_3(PredicateType, target1, target2, target3, ...)                     \
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

#define FATP_ABORT_ENFORCE_1(PredicateType, target, ...)                                           \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::AbortPolicy>(                            \
            fat_p::PredicateType::check(target), #PredicateType "(" #target ")", FATP_LOCUS);      \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#define FATP_ABORT_ENFORCE_2(PredicateType, target1, target2, ...)                                 \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::AbortPolicy>(                            \
            fat_p::PredicateType::check(target1, target2),                                         \
            #PredicateType "(" #target1 ", " #target2 ")",                                         \
            FATP_LOCUS);                                                                           \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#define FATP_ABORT_ENFORCE_3(PredicateType, target1, target2, target3, ...)                        \
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

#define FATP_WARN_ENFORCE_1(PredicateType, target, ...)                                            \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::WarningPolicy>(                          \
            fat_p::PredicateType::check(target), #PredicateType "(" #target ")", FATP_LOCUS);      \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#define FATP_WARN_ENFORCE_2(PredicateType, target1, target2, ...)                                  \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::WarningPolicy>(                          \
            fat_p::PredicateType::check(target1, target2),                                         \
            #PredicateType "(" #target1 ", " #target2 ")",                                         \
            FATP_LOCUS);                                                                           \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#define FATP_WARN_ENFORCE_3(PredicateType, target1, target2, target3, ...)                         \
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

// Release build: All FATP_DEBUG_ENFORCE_* macros expand to nothing
#ifdef NDEBUG
#define FATP_DEBUG_ENFORCE_ALL_SATISFY(pred, container, ...) ((void)0)
#define FATP_DEBUG_ENFORCE_ANY_SATISFY(pred, container, ...) ((void)0)
#define FATP_DEBUG_ENFORCE_HAS_SIZE(expected_size, container, ...) ((void)0)
#define FATP_DEBUG_ENFORCE_IS_UNIQUE(container, ...) ((void)0)
#define FATP_DEBUG_ENFORCE_APPROX_EQUAL(epsilon, a, b, ...) ((void)0)
#define FATP_DEBUG_ENFORCE_IN_RANGE(min, max, value, ...) ((void)0)
#define FATP_DEBUG_ENFORCE_VALID_INDEX(idx, container, ...) ((void)0)
#define FATP_DEBUG_ENFORCE_IS_INTEGRAL(value, ...) ((void)0)
#define FATP_DEBUG_ENFORCE_IS_NON_NEGATIVE(value, ...) ((void)0)
#define FATP_DEBUG_ENFORCE_IS_POSITIVE(value, ...) ((void)0)
#define FATP_DEBUG_ENFORCE_IS_POWER_OF_TWO(value, ...) ((void)0)
#define FATP_DEBUG_ENFORCE_IS_SORTED(container, ...) ((void)0)
#define FATP_DEBUG_ENFORCE_IS_SORTED_WITH(comp, container, ...) ((void)0)
#define FATP_DEBUG_ENFORCE_IS_VALID_ITERATOR(it, end, ...) ((void)0)
#define FATP_DEBUG_ENFORCE_NOT_EMPTY(value, ...) ((void)0)
#define FATP_DEBUG_ENFORCE_NOT_NULL(ptr, ...) ((void)0)
#define FATP_DEBUG_ENFORCE_IS_FINITE(value, ...) ((void)0)
#define FATP_DEBUG_ENFORCE_IS_NORMAL(value, ...) ((void)0)
#endif

#define FATP_ALWAYS_ENFORCE_ALL_SATISFY(pred, container, ...)                                      \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::AlwaysEnforcePolicy>(                    \
            fat_p::AllSatisfyPredicate::check(pred, container),                                    \
            "all_satisfy(" #pred ", " #container ")",                                              \
            FATP_LOCUS);                                                                           \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#ifndef NDEBUG
#define FATP_DEBUG_ENFORCE_ALL_SATISFY(pred, container, ...)                                       \
    fat_p::debug_enforce_predicate_2<fat_p::AllSatisfyPredicate>(                                  \
        pred, container, "all_satisfy(" #pred ", " #container ")", FATP_LOCUS, ##__VA_ARGS__)
#endif

// ============================================================================
// Specific Predicate Convenience Macros - AnySatisfy
// ============================================================================

#define FATP_ALWAYS_ENFORCE_ANY_SATISFY(pred, container, ...)                                      \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::AlwaysEnforcePolicy>(                    \
            fat_p::AnySatisfyPredicate::check(pred, container),                                    \
            "any_satisfy(" #pred ", " #container ")",                                              \
            FATP_LOCUS);                                                                           \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#ifndef NDEBUG
#define FATP_DEBUG_ENFORCE_ANY_SATISFY(pred, container, ...)                                       \
    fat_p::debug_enforce_predicate_2<fat_p::AnySatisfyPredicate>(                                  \
        pred, container, "any_satisfy(" #pred ", " #container ")", FATP_LOCUS, ##__VA_ARGS__)
#endif

// ============================================================================
// Specific Predicate Convenience Macros - HasSize
// ============================================================================

#define FATP_ALWAYS_ENFORCE_HAS_SIZE(expected_size, container, ...)                                \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::AlwaysEnforcePolicy>(                    \
            fat_p::HasSizePredicate::check(expected_size, container),                              \
            "has_size(" #expected_size ", " #container ")",                                        \
            FATP_LOCUS);                                                                           \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#ifndef NDEBUG
#define FATP_DEBUG_ENFORCE_HAS_SIZE(expected_size, container, ...)                                 \
    fat_p::debug_enforce_predicate_2<fat_p::HasSizePredicate>(                                     \
        expected_size, container, "has_size(" #expected_size ", " #container ")",                  \
        FATP_LOCUS, ##__VA_ARGS__)
#endif

// ============================================================================
// Specific Predicate Convenience Macros - ContainerIsUnique
// ============================================================================

#define FATP_ALWAYS_ENFORCE_IS_UNIQUE(container, ...)                                              \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::AlwaysEnforcePolicy>(                    \
            fat_p::ContainerIsUniquePredicate::check(container),                                   \
            "is_unique(" #container ")",                                                           \
            FATP_LOCUS);                                                                           \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#ifndef NDEBUG
#define FATP_DEBUG_ENFORCE_IS_UNIQUE(container, ...)                                               \
    fat_p::debug_enforce_predicate_1<fat_p::ContainerIsUniquePredicate>(                           \
        container, "is_unique(" #container ")", FATP_LOCUS, ##__VA_ARGS__)
#endif

// ============================================================================
// Specific Predicate Convenience Macros - ApproxEqual
// ============================================================================

#define FATP_ALWAYS_ENFORCE_APPROX_EQUAL(epsilon, a, b, ...)                                       \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::AlwaysEnforcePolicy>(                    \
            fat_p::ApproxEqualPredicate::check(epsilon, a, b),                                     \
            "approx_equal(" #epsilon ", " #a ", " #b ")",                                          \
            FATP_LOCUS);                                                                           \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#ifndef NDEBUG
#define FATP_DEBUG_ENFORCE_APPROX_EQUAL(epsilon, a, b, ...)                                        \
    fat_p::debug_enforce_predicate_3<fat_p::ApproxEqualPredicate>(                                 \
        epsilon, a, b, "approx_equal(" #epsilon ", " #a ", " #b ")", FATP_LOCUS, ##__VA_ARGS__)
#endif

// ============================================================================
// Specific Predicate Convenience Macros - InRange
// ============================================================================

#define FATP_ALWAYS_ENFORCE_IN_RANGE(min, max, value, ...)                                         \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::AlwaysEnforcePolicy>(                    \
            fat_p::InRangePredicate::check(value, min, max),                                       \
            "in_range(" #min ", " #max ", " #value ")",                                            \
            FATP_LOCUS);                                                                           \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#ifndef NDEBUG
#define FATP_DEBUG_ENFORCE_IN_RANGE(min, max, value, ...)                                          \
    fat_p::debug_enforce_predicate_3<fat_p::InRangePredicate>(                                     \
        value, min, max, "in_range(" #min ", " #max ", " #value ")", FATP_LOCUS, ##__VA_ARGS__)
#endif

// ============================================================================
// Specific Predicate Convenience Macros - ValidIndex
// ============================================================================

#define FATP_ALWAYS_ENFORCE_VALID_INDEX(idx, container, ...)                                       \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::AlwaysEnforcePolicy>(                    \
            fat_p::ValidIndexPredicate::check(idx, container),                                     \
            "valid_index(" #idx ", " #container ")",                                               \
            FATP_LOCUS);                                                                           \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#ifndef NDEBUG
#define FATP_DEBUG_ENFORCE_VALID_INDEX(idx, container, ...)                                        \
    fat_p::debug_enforce_predicate_2<fat_p::ValidIndexPredicate>(                                  \
        idx, container, "valid_index(" #idx ", " #container ")", FATP_LOCUS, ##__VA_ARGS__)
#endif

// ============================================================================
// Specific Predicate Convenience Macros - IsIntegral
// ============================================================================

#define FATP_ALWAYS_ENFORCE_IS_INTEGRAL(value, ...)                                                \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::AlwaysEnforcePolicy>(                    \
            fat_p::IsIntegralPredicate::check(value), "is_integral(" #value ")", FATP_LOCUS);      \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#ifndef NDEBUG
#define FATP_DEBUG_ENFORCE_IS_INTEGRAL(value, ...)                                                 \
    fat_p::debug_enforce_predicate_1<fat_p::IsIntegralPredicate>(                                  \
        value, "is_integral(" #value ")", FATP_LOCUS, ##__VA_ARGS__)
#endif

// ============================================================================
// Specific Predicate Convenience Macros - IsNonNegative
// ============================================================================

#define FATP_ALWAYS_ENFORCE_IS_NON_NEGATIVE(value, ...)                                            \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::AlwaysEnforcePolicy>(                    \
            fat_p::IsNonNegativePredicate::check(value),                                           \
            "is_non_negative(" #value ")",                                                         \
            FATP_LOCUS);                                                                           \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#ifndef NDEBUG
#define FATP_DEBUG_ENFORCE_IS_NON_NEGATIVE(value, ...)                                             \
    fat_p::debug_enforce_predicate_1<fat_p::IsNonNegativePredicate>(                               \
        value, "is_non_negative(" #value ")", FATP_LOCUS, ##__VA_ARGS__)
#endif

// ============================================================================
// Specific Predicate Convenience Macros - IsPositive
// ============================================================================

#define FATP_ALWAYS_ENFORCE_IS_POSITIVE(value, ...)                                                \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::AlwaysEnforcePolicy>(                    \
            fat_p::IsPositivePredicate::check(value), "is_positive(" #value ")", FATP_LOCUS);      \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#ifndef NDEBUG
#define FATP_DEBUG_ENFORCE_IS_POSITIVE(value, ...)                                                 \
    fat_p::debug_enforce_predicate_1<fat_p::IsPositivePredicate>(                                  \
        value, "is_positive(" #value ")", FATP_LOCUS, ##__VA_ARGS__)
#endif

// ============================================================================
// Specific Predicate Convenience Macros - IsPowerOfTwo
// ============================================================================

#define FATP_ALWAYS_ENFORCE_IS_POWER_OF_TWO(value, ...)                                            \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::AlwaysEnforcePolicy>(                    \
            fat_p::IsPowerOfTwoPredicate::check(value),                                            \
            "is_power_of_two(" #value ")",                                                         \
            FATP_LOCUS);                                                                           \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#ifndef NDEBUG
#define FATP_DEBUG_ENFORCE_IS_POWER_OF_TWO(value, ...)                                             \
    fat_p::debug_enforce_predicate_1<fat_p::IsPowerOfTwoPredicate>(                                \
        value, "is_power_of_two(" #value ")", FATP_LOCUS, ##__VA_ARGS__)
#endif

// ============================================================================
// Specific Predicate Convenience Macros - IsSorted
// ============================================================================

#define FATP_ALWAYS_ENFORCE_IS_SORTED(container, ...)                                              \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::AlwaysEnforcePolicy>(                    \
            fat_p::IsSortedPredicate::check(container), "is_sorted(" #container ")", FATP_LOCUS);  \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#ifndef NDEBUG
#define FATP_DEBUG_ENFORCE_IS_SORTED(container, ...)                                               \
    fat_p::debug_enforce_predicate_1<fat_p::IsSortedPredicate>(                                    \
        container, "is_sorted(" #container ")", FATP_LOCUS, ##__VA_ARGS__)
#endif

#define FATP_ALWAYS_ENFORCE_IS_SORTED_WITH(comp, container, ...)                                   \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::AlwaysEnforcePolicy>(                    \
            fat_p::IsSortedPredicate::check(container, comp),                                      \
            "is_sorted_with(" #comp ", " #container ")",                                           \
            FATP_LOCUS);                                                                           \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#ifndef NDEBUG
#define FATP_DEBUG_ENFORCE_IS_SORTED_WITH(comp, container, ...)                                    \
    fat_p::debug_enforce_sorted_with_impl(                                                         \
        comp, container, "is_sorted_with(" #comp ", " #container ")", FATP_LOCUS, ##__VA_ARGS__)
#endif

// ============================================================================
// Specific Predicate Convenience Macros - IsValidIterator
// ============================================================================

#define FATP_ALWAYS_ENFORCE_IS_VALID_ITERATOR(it, end, ...)                                        \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::AlwaysEnforcePolicy>(                    \
            fat_p::IsValidIteratorPredicate::check(it, end),                                       \
            "is_valid_iterator(" #it ", " #end ")",                                                \
            FATP_LOCUS);                                                                           \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#ifndef NDEBUG
#define FATP_DEBUG_ENFORCE_IS_VALID_ITERATOR(it, end, ...)                                         \
    fat_p::debug_enforce_predicate_2<fat_p::IsValidIteratorPredicate>(                             \
        it, end, "is_valid_iterator(" #it ", " #end ")", FATP_LOCUS, ##__VA_ARGS__)
#endif

// ============================================================================
// Specific Predicate Convenience Macros - NotEmpty
// ============================================================================

#define FATP_ALWAYS_ENFORCE_NOT_EMPTY(value, ...)                                                  \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::AlwaysEnforcePolicy>(                    \
            fat_p::NotEmptyPredicate::check(value), "not_empty(" #value ")", FATP_LOCUS);          \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#ifndef NDEBUG
#define FATP_DEBUG_ENFORCE_NOT_EMPTY(value, ...)                                                   \
    fat_p::debug_enforce_predicate_1<fat_p::NotEmptyPredicate>(                                    \
        value, "not_empty(" #value ")", FATP_LOCUS, ##__VA_ARGS__)
#endif

// ============================================================================
// Specific Predicate Convenience Macros - NotNull (All Policies)
// ============================================================================

#define FATP_ALWAYS_ENFORCE_NOT_NULL(ptr, ...)                                                     \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::AlwaysEnforcePolicy>(                    \
            fat_p::NotNullPredicate::check(ptr), "not_null(" #ptr ")", FATP_LOCUS);                \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#ifndef NDEBUG
#define FATP_DEBUG_ENFORCE_NOT_NULL(ptr, ...)                                                      \
    fat_p::debug_enforce_predicate_1<fat_p::NotNullPredicate>(                                     \
        ptr, "not_null(" #ptr ")", FATP_LOCUS, ##__VA_ARGS__)
#endif

#define FATP_WARN_ENFORCE_NOT_NULL(ptr, ...)                                                       \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::WarningPolicy>(                          \
            fat_p::NotNullPredicate::check(ptr), "not_null(" #ptr ")", FATP_LOCUS);                \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#define FATP_NOEXCEPT_ENFORCE_NOT_NULL(ptr, ...)                                                   \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::NoThrowPolicy>(                          \
            fat_p::NotNullPredicate::check(ptr), "not_null(" #ptr ")", FATP_LOCUS);                \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#define FATP_ABORT_ENFORCE_NOT_NULL(ptr, ...)                                                      \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::AbortPolicy>(                            \
            fat_p::NotNullPredicate::check(ptr), "not_null(" #ptr ")", FATP_LOCUS);                \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

// ============================================================================
// Specific Predicate Convenience Macros - IsFinite
// ============================================================================

#define FATP_ALWAYS_ENFORCE_IS_FINITE(value, ...)                                                  \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::AlwaysEnforcePolicy>(                    \
            fat_p::IsFinitePredicate::check(value), "is_finite(" #value ")", FATP_LOCUS);          \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#ifndef NDEBUG
#define FATP_DEBUG_ENFORCE_IS_FINITE(value, ...)                                                   \
    fat_p::debug_enforce_predicate_1<fat_p::IsFinitePredicate>(                                    \
        value, "is_finite(" #value ")", FATP_LOCUS, ##__VA_ARGS__)
#endif

// ============================================================================
// Specific Predicate Convenience Macros - IsNormal
// ============================================================================

#define FATP_ALWAYS_ENFORCE_IS_NORMAL(value, ...)                                                  \
    do                                                                                             \
    {                                                                                              \
        auto enforcer = fat_p::enforce_policy_impl<fat_p::AlwaysEnforcePolicy>(                    \
            fat_p::IsNormalPredicate::check(value), "is_normal(" #value ")", FATP_LOCUS);          \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#ifndef NDEBUG
#define FATP_DEBUG_ENFORCE_IS_NORMAL(value, ...)                                                   \
    fat_p::debug_enforce_predicate_1<fat_p::IsNormalPredicate>(                                    \
        value, "is_normal(" #value ")", FATP_LOCUS, ##__VA_ARGS__)
#endif

} // namespace fat_p
