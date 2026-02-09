#pragma once

/*
FATP_META:
  meta_version: 1
  component: enforce
  file_role: public_header
  path: include/fat_p/enforce.h
  namespace: fat_p
  layer: Foundation
  summary: "Public header for enforce."
  api_stability: in_work
  related:
    docs_search: "enforce"
    tests:
      - components/Enforce/tests/test_Enforce.cpp
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 88
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/

/**
 * @file enforce.h
 * @brief Provides macro implementations for contract enforcement using
 * predefined and generic predicates with various policies.
 *
 *
 *
 * @section zero_cost_guarantee Zero-Cost Guarantee
 *
 * All `FATP_DEBUG_ENFORCE*` and `FATP_ENFORCE` macros are GUARANTEED to generate zero code
 * in release builds (when NDEBUG is defined). This is achieved via `if constexpr`
 * which ensures the enforcement code is never instantiated, not merely "optimized away".
 *
 * The `FATP_ALWAYS_ENFORCE*` macros always generate code regardless of build mode.
 */

#include <source_location>
#include <type_traits>
#include <utility>

#include "enforce_enforcers.h"
#include "enforce_predicates.h"
#include "enforce_raiser_selector.h"
#include "enforce_raisers.h"

namespace fat_p
{

// ============================================================================
// Source Location
// ============================================================================
// All enforcement functions use std::source_location for call-site metadata.
// Macros pass std::source_location::current() which captures the expansion site.

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
[[nodiscard]] constexpr auto enforce_policy_impl(bool passed, const char* expression_str, std::source_location loc)
{
    using Raiser = typename RaiserSelector<Policy>::type;
    return MakeEnforcer<Raiser>(passed, expression_str, loc);
}

// ============================================================================
// Debug-Only Enforcement Helpers (Guaranteed Zero-Cost in Release)
// ============================================================================
// These templates use if constexpr to ensure NO instantiation occurs in release.
// The discarded branch is never compiled - this is mandated by the C++ standard.

// Basic condition enforcement
template <typename... Msgs>
inline void debug_enforce_impl([[maybe_unused]] bool condition,
                               [[maybe_unused]] const char* expression_str,
                               [[maybe_unused]] std::source_location loc,
                               [[maybe_unused]] Msgs&&... msgs)
{
    if constexpr (FATP_DEBUG_ENFORCE_ENABLED)
    {
        auto enforcer = enforce_policy_impl<DebugOnlyPolicy>(condition, expression_str, loc);
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
#define FATP_ENFORCE(condition, ...)                                       \
    do                                                                     \
    {                                                                      \
        if (!(condition)) [[unlikely]]                                     \
            fat_p::debug_enforce_impl(false, #condition,                   \
                std::source_location::current() __VA_OPT__(,) __VA_ARGS__); \
    } while (0)
#endif

#define FATP_ALWAYS_ENFORCE(condition, ...)                                          \
    do                                                                               \
    {                                                                                \
        if (!(condition)) [[unlikely]]                                               \
        {                                                                            \
            auto enforcer = fat_p::enforce_policy_impl<fat_p::AlwaysEnforcePolicy>(  \
                false, #condition, std::source_location::current());                 \
            enforcer(__VA_ARGS__);                                                   \
        }                                                                            \
    } while (0)

#define FATP_ENFORCE_WARN(condition, ...)                                         \
    do                                                                            \
    {                                                                             \
        if (!(condition)) [[unlikely]]                                            \
        {                                                                         \
            auto enforcer = fat_p::enforce_policy_impl<fat_p::WarningPolicy>(     \
                false, #condition, std::source_location::current());              \
            enforcer(__VA_ARGS__);                                                \
        }                                                                         \
    } while (0)

#define FATP_NOEXCEPT_ENFORCE(condition, ...)                                      \
    do                                                                              \
    {                                                                               \
        if (!(condition)) [[unlikely]]                                              \
        {                                                                           \
            auto enforcer = fat_p::enforce_policy_impl<fat_p::NoThrowPolicy>(       \
                false, #condition, std::source_location::current());                \
            enforcer(__VA_ARGS__);                                                  \
        }                                                                           \
    } while (0)

#define FATP_ABORT_ENFORCE(condition, ...)                                        \
    do                                                                             \
    {                                                                              \
        if (!(condition)) [[unlikely]]                                             \
        {                                                                          \
            auto enforcer = fat_p::enforce_policy_impl<fat_p::AbortPolicy>(        \
                false, #condition, std::source_location::current());               \
            enforcer(__VA_ARGS__);                                                 \
        }                                                                          \
    } while (0)

// ============================================================================
// Expected Integration Macros
// ============================================================================

#define FATP_ENFORCE_EXPECTED(condition, ...)                         \
    ([&]() -> fat_p::Expected<void, std::string> {                    \
        if (!(condition))                                             \
        {                                                             \
            fat_p::MessageBuilder mb;                                 \
            mb.format(__VA_ARGS__);                                   \
            std::string msg = mb.get_message(std::source_location::current(), #condition); \
            fat_p::detail::writeToStderr("Expected Failure: ", msg);  \
            return fat_p::make_unexpected(msg);                       \
        }                                                             \
        return {};                                                    \
    })()

#define FATP_ALWAYS_ENFORCE_EXPECTED(condition, ...)                  \
    ([&]() -> fat_p::Expected<void, std::string> {                    \
        if (!(condition))                                             \
        {                                                             \
            fat_p::MessageBuilder mb;                                 \
            mb.format(__VA_ARGS__);                                   \
            std::string msg = mb.get_message(std::source_location::current(), #condition); \
            fat_p::detail::writeToStderr("Expected Failure: ", msg);  \
            return fat_p::make_unexpected(msg);                       \
        }                                                             \
        return {};                                                    \
    })()

#define FATP_ENFORCE_PREDICATE_EXPECTED(PredicateType, target, ...)                       \
    ([&]() -> fat_p::Expected<bool, std::string> {                                        \
        auto fatp_pred_result_ = PredicateType::check(target);                            \
        if (!fatp_pred_result_)                                                           \
        {                                                                                 \
            fat_p::MessageBuilder mb;                                                     \
            mb.format(__VA_ARGS__);                                                       \
            std::string msg = mb.get_message(std::source_location::current(), #PredicateType "(" #target ")"); \
            fat_p::detail::writeToStderr("Expected Failure: ", msg);                      \
            return fat_p::make_unexpected(msg);                                           \
        }                                                                                 \
        return fatp_pred_result_;                                                         \
    })()

// ============================================================================
// Internal Implementation Macro
// ============================================================================
// Single pattern used by all active (non-debug) predicate enforcement macros.
// Reduces every ALWAYS/WARN/NOEXCEPT/ABORT predicate macro to a one-liner.
// Do NOT call directly â€” use the public FATP_*_ENFORCE_* macros.

#define FATP_ENFORCE_PRED_IMPL_(Policy, check_expr, expr_str, ...)        \
    do                                                                     \
    {                                                                      \
        if (!(check_expr)) [[unlikely]]                                    \
        {                                                                  \
            auto enforcer = fat_p::enforce_policy_impl<fat_p::Policy>(     \
                false, (expr_str), std::source_location::current());       \
            enforcer(__VA_ARGS__);                                         \
        }                                                                  \
    } while (0)

// ============================================================================
// Generic Arity-Based Macros - Always Enforce
// ============================================================================

#define FATP_ALWAYS_ENFORCE_1(PredicateType, target, ...) \
    FATP_ENFORCE_PRED_IMPL_(AlwaysEnforcePolicy, fat_p::PredicateType::check(target), #PredicateType "(" #target ")", __VA_ARGS__)

#define FATP_ALWAYS_ENFORCE_2(PredicateType, target1, target2, ...) \
    FATP_ENFORCE_PRED_IMPL_(AlwaysEnforcePolicy, fat_p::PredicateType::check(target1, target2), #PredicateType "(" #target1 ", " #target2 ")", __VA_ARGS__)

#define FATP_ALWAYS_ENFORCE_3(PredicateType, target1, target2, target3, ...) \
    FATP_ENFORCE_PRED_IMPL_(AlwaysEnforcePolicy, fat_p::PredicateType::check(target1, target2, target3), #PredicateType "(" #target1 ", " #target2 ", " #target3 ")", __VA_ARGS__)

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
#define FATP_DEBUG_ENFORCE_1(PredicateType, target, ...)                                        \
    do { if (!fat_p::PredicateType::check(target)) [[unlikely]]                                 \
        fat_p::debug_enforce_impl(false, #PredicateType "(" #target ")",                        \
            std::source_location::current() __VA_OPT__(,) __VA_ARGS__); } while (0)

#define FATP_DEBUG_ENFORCE_2(PredicateType, target1, target2, ...)                              \
    do { if (!fat_p::PredicateType::check(target1, target2)) [[unlikely]]                       \
        fat_p::debug_enforce_impl(false, #PredicateType "(" #target1 ", " #target2 ")",         \
            std::source_location::current() __VA_OPT__(,) __VA_ARGS__); } while (0)

#define FATP_DEBUG_ENFORCE_3(PredicateType, target1, target2, target3, ...)                     \
    do { if (!fat_p::PredicateType::check(target1, target2, target3)) [[unlikely]]              \
        fat_p::debug_enforce_impl(false, #PredicateType "(" #target1 ", " #target2 ", " #target3 ")", \
            std::source_location::current() __VA_OPT__(,) __VA_ARGS__); } while (0)
#endif

// Aliases for FATP_ENFORCE_N (debug-only variants)
#define FATP_ENFORCE_1 FATP_DEBUG_ENFORCE_1
#define FATP_ENFORCE_2 FATP_DEBUG_ENFORCE_2
#define FATP_ENFORCE_3 FATP_DEBUG_ENFORCE_3

// ============================================================================
// Generic Arity-Based Macros - NoThrow
// ============================================================================

#define FATP_NOEXCEPT_ENFORCE_1(PredicateType, target, ...) \
    FATP_ENFORCE_PRED_IMPL_(NoThrowPolicy, fat_p::PredicateType::check(target), #PredicateType "(" #target ")", __VA_ARGS__)

#define FATP_NOEXCEPT_ENFORCE_2(PredicateType, target1, target2, ...) \
    FATP_ENFORCE_PRED_IMPL_(NoThrowPolicy, fat_p::PredicateType::check(target1, target2), #PredicateType "(" #target1 ", " #target2 ")", __VA_ARGS__)

#define FATP_NOEXCEPT_ENFORCE_3(PredicateType, target1, target2, target3, ...) \
    FATP_ENFORCE_PRED_IMPL_(NoThrowPolicy, fat_p::PredicateType::check(target1, target2, target3), #PredicateType "(" #target1 ", " #target2 ", " #target3 ")", __VA_ARGS__)

// ============================================================================
// Generic Arity-Based Macros - Abort
// ============================================================================

#define FATP_ABORT_ENFORCE_1(PredicateType, target, ...) \
    FATP_ENFORCE_PRED_IMPL_(AbortPolicy, fat_p::PredicateType::check(target), #PredicateType "(" #target ")", __VA_ARGS__)

#define FATP_ABORT_ENFORCE_2(PredicateType, target1, target2, ...) \
    FATP_ENFORCE_PRED_IMPL_(AbortPolicy, fat_p::PredicateType::check(target1, target2), #PredicateType "(" #target1 ", " #target2 ")", __VA_ARGS__)

#define FATP_ABORT_ENFORCE_3(PredicateType, target1, target2, target3, ...) \
    FATP_ENFORCE_PRED_IMPL_(AbortPolicy, fat_p::PredicateType::check(target1, target2, target3), #PredicateType "(" #target1 ", " #target2 ", " #target3 ")", __VA_ARGS__)

// ============================================================================
// Generic Arity-Based Macros - Warning
// ============================================================================

#define FATP_WARN_ENFORCE_1(PredicateType, target, ...) \
    FATP_ENFORCE_PRED_IMPL_(WarningPolicy, fat_p::PredicateType::check(target), #PredicateType "(" #target ")", __VA_ARGS__)

#define FATP_WARN_ENFORCE_2(PredicateType, target1, target2, ...) \
    FATP_ENFORCE_PRED_IMPL_(WarningPolicy, fat_p::PredicateType::check(target1, target2), #PredicateType "(" #target1 ", " #target2 ")", __VA_ARGS__)

#define FATP_WARN_ENFORCE_3(PredicateType, target1, target2, target3, ...) \
    FATP_ENFORCE_PRED_IMPL_(WarningPolicy, fat_p::PredicateType::check(target1, target2, target3), #PredicateType "(" #target1 ", " #target2 ", " #target3 ")", __VA_ARGS__)


// ============================================================================
// Per-Predicate Convenience Macros
// ============================================================================
// These provide human-readable expression strings in diagnostics (e.g.,
// "not_null(ptr)" instead of "NotNullPredicate(ptr)"). Each is a thin
// wrapper around FATP_ENFORCE_PRED_IMPL_ or debug_enforce_impl.
//
// For predicates not listed here, use the generic arity macros above:
//   FATP_ALWAYS_ENFORCE_1(PredicateType, target, ...)
//   FATP_DEBUG_ENFORCE_1(PredicateType, target, ...)
//   etc.

// --- Always-Enforce (active in all builds) --------------------------------

#define FATP_ALWAYS_ENFORCE_ALL_SATISFY(pred, container, ...) \
    FATP_ENFORCE_PRED_IMPL_(AlwaysEnforcePolicy, fat_p::AllSatisfyPredicate::check(pred, container), "all_satisfy(" #pred ", " #container ")", __VA_ARGS__)
#define FATP_ALWAYS_ENFORCE_ANY_SATISFY(pred, container, ...) \
    FATP_ENFORCE_PRED_IMPL_(AlwaysEnforcePolicy, fat_p::AnySatisfyPredicate::check(pred, container), "any_satisfy(" #pred ", " #container ")", __VA_ARGS__)
#define FATP_ALWAYS_ENFORCE_HAS_SIZE(expected_size, container, ...) \
    FATP_ENFORCE_PRED_IMPL_(AlwaysEnforcePolicy, fat_p::HasSizePredicate::check(expected_size, container), "has_size(" #expected_size ", " #container ")", __VA_ARGS__)
#define FATP_ALWAYS_ENFORCE_IS_UNIQUE(container, ...) \
    FATP_ENFORCE_PRED_IMPL_(AlwaysEnforcePolicy, fat_p::ContainerIsUniquePredicate::check(container), "is_unique(" #container ")", __VA_ARGS__)
#define FATP_ALWAYS_ENFORCE_APPROX_EQUAL(epsilon, a, b, ...) \
    FATP_ENFORCE_PRED_IMPL_(AlwaysEnforcePolicy, fat_p::ApproxEqualPredicate::check(epsilon, a, b), "approx_equal(" #epsilon ", " #a ", " #b ")", __VA_ARGS__)
#define FATP_ALWAYS_ENFORCE_IN_RANGE(min, max, value, ...) \
    FATP_ENFORCE_PRED_IMPL_(AlwaysEnforcePolicy, fat_p::InRangePredicate::check(value, min, max), "in_range(" #min ", " #max ", " #value ")", __VA_ARGS__)
#define FATP_ALWAYS_ENFORCE_VALID_INDEX(idx, container, ...) \
    FATP_ENFORCE_PRED_IMPL_(AlwaysEnforcePolicy, fat_p::ValidIndexPredicate::check(idx, container), "valid_index(" #idx ", " #container ")", __VA_ARGS__)
#define FATP_ALWAYS_ENFORCE_IS_INTEGRAL(value, ...) \
    FATP_ENFORCE_PRED_IMPL_(AlwaysEnforcePolicy, fat_p::IsIntegralPredicate::check(value), "is_integral(" #value ")", __VA_ARGS__)
#define FATP_ALWAYS_ENFORCE_IS_NON_NEGATIVE(value, ...) \
    FATP_ENFORCE_PRED_IMPL_(AlwaysEnforcePolicy, fat_p::IsNonNegativePredicate::check(value), "is_non_negative(" #value ")", __VA_ARGS__)
#define FATP_ALWAYS_ENFORCE_IS_POSITIVE(value, ...) \
    FATP_ENFORCE_PRED_IMPL_(AlwaysEnforcePolicy, fat_p::IsPositivePredicate::check(value), "is_positive(" #value ")", __VA_ARGS__)
#define FATP_ALWAYS_ENFORCE_IS_POWER_OF_TWO(value, ...) \
    FATP_ENFORCE_PRED_IMPL_(AlwaysEnforcePolicy, fat_p::IsPowerOfTwoPredicate::check(value), "is_power_of_two(" #value ")", __VA_ARGS__)
#define FATP_ALWAYS_ENFORCE_IS_SORTED(container, ...) \
    FATP_ENFORCE_PRED_IMPL_(AlwaysEnforcePolicy, fat_p::IsSortedPredicate::check(container), "is_sorted(" #container ")", __VA_ARGS__)
#define FATP_ALWAYS_ENFORCE_IS_SORTED_WITH(comp, container, ...) \
    FATP_ENFORCE_PRED_IMPL_(AlwaysEnforcePolicy, fat_p::IsSortedPredicate::check(container, comp), "is_sorted_with(" #comp ", " #container ")", __VA_ARGS__)
#define FATP_ALWAYS_ENFORCE_IS_VALID_ITERATOR(it, end, ...) \
    FATP_ENFORCE_PRED_IMPL_(AlwaysEnforcePolicy, fat_p::IsValidIteratorPredicate::check(it, end), "is_valid_iterator(" #it ", " #end ")", __VA_ARGS__)
#define FATP_ALWAYS_ENFORCE_NOT_EMPTY(value, ...) \
    FATP_ENFORCE_PRED_IMPL_(AlwaysEnforcePolicy, fat_p::NotEmptyPredicate::check(value), "not_empty(" #value ")", __VA_ARGS__)
#define FATP_ALWAYS_ENFORCE_NOT_NULL(ptr, ...) \
    FATP_ENFORCE_PRED_IMPL_(AlwaysEnforcePolicy, fat_p::NotNullPredicate::check(ptr), "not_null(" #ptr ")", __VA_ARGS__)
#define FATP_ALWAYS_ENFORCE_IS_FINITE(value, ...) \
    FATP_ENFORCE_PRED_IMPL_(AlwaysEnforcePolicy, fat_p::IsFinitePredicate::check(value), "is_finite(" #value ")", __VA_ARGS__)
#define FATP_ALWAYS_ENFORCE_IS_NORMAL(value, ...) \
    FATP_ENFORCE_PRED_IMPL_(AlwaysEnforcePolicy, fat_p::IsNormalPredicate::check(value), "is_normal(" #value ")", __VA_ARGS__)

// --- Debug-Only (zero codegen in release) ---------------------------------

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
#else
#define FATP_DEBUG_ENFORCE_ALL_SATISFY(pred, container, ...)      \
    do { if (!fat_p::AllSatisfyPredicate::check(pred, container)) [[unlikely]]  \
        fat_p::debug_enforce_impl(false, "all_satisfy(" #pred ", " #container ")", \
            std::source_location::current() __VA_OPT__(,) __VA_ARGS__); } while (0)
#define FATP_DEBUG_ENFORCE_ANY_SATISFY(pred, container, ...)      \
    do { if (!fat_p::AnySatisfyPredicate::check(pred, container)) [[unlikely]]  \
        fat_p::debug_enforce_impl(false, "any_satisfy(" #pred ", " #container ")", \
            std::source_location::current() __VA_OPT__(,) __VA_ARGS__); } while (0)
#define FATP_DEBUG_ENFORCE_HAS_SIZE(expected_size, container, ...) \
    do { if (!fat_p::HasSizePredicate::check(expected_size, container)) [[unlikely]] \
        fat_p::debug_enforce_impl(false, "has_size(" #expected_size ", " #container ")", \
            std::source_location::current() __VA_OPT__(,) __VA_ARGS__); } while (0)
#define FATP_DEBUG_ENFORCE_IS_UNIQUE(container, ...)              \
    do { if (!fat_p::ContainerIsUniquePredicate::check(container)) [[unlikely]] \
        fat_p::debug_enforce_impl(false, "is_unique(" #container ")", \
            std::source_location::current() __VA_OPT__(,) __VA_ARGS__); } while (0)
#define FATP_DEBUG_ENFORCE_APPROX_EQUAL(epsilon, a, b, ...)       \
    do { if (!fat_p::ApproxEqualPredicate::check(epsilon, a, b)) [[unlikely]] \
        fat_p::debug_enforce_impl(false, "approx_equal(" #epsilon ", " #a ", " #b ")", \
            std::source_location::current() __VA_OPT__(,) __VA_ARGS__); } while (0)
#define FATP_DEBUG_ENFORCE_IN_RANGE(min, max, value, ...)         \
    do { if (!fat_p::InRangePredicate::check(value, min, max)) [[unlikely]] \
        fat_p::debug_enforce_impl(false, "in_range(" #min ", " #max ", " #value ")", \
            std::source_location::current() __VA_OPT__(,) __VA_ARGS__); } while (0)
#define FATP_DEBUG_ENFORCE_VALID_INDEX(idx, container, ...)       \
    do { if (!fat_p::ValidIndexPredicate::check(idx, container)) [[unlikely]] \
        fat_p::debug_enforce_impl(false, "valid_index(" #idx ", " #container ")", \
            std::source_location::current() __VA_OPT__(,) __VA_ARGS__); } while (0)
#define FATP_DEBUG_ENFORCE_IS_INTEGRAL(value, ...)                \
    do { if (!fat_p::IsIntegralPredicate::check(value)) [[unlikely]] \
        fat_p::debug_enforce_impl(false, "is_integral(" #value ")", \
            std::source_location::current() __VA_OPT__(,) __VA_ARGS__); } while (0)
#define FATP_DEBUG_ENFORCE_IS_NON_NEGATIVE(value, ...)            \
    do { if (!fat_p::IsNonNegativePredicate::check(value)) [[unlikely]] \
        fat_p::debug_enforce_impl(false, "is_non_negative(" #value ")", \
            std::source_location::current() __VA_OPT__(,) __VA_ARGS__); } while (0)
#define FATP_DEBUG_ENFORCE_IS_POSITIVE(value, ...)                \
    do { if (!fat_p::IsPositivePredicate::check(value)) [[unlikely]] \
        fat_p::debug_enforce_impl(false, "is_positive(" #value ")", \
            std::source_location::current() __VA_OPT__(,) __VA_ARGS__); } while (0)
#define FATP_DEBUG_ENFORCE_IS_POWER_OF_TWO(value, ...)            \
    do { if (!fat_p::IsPowerOfTwoPredicate::check(value)) [[unlikely]] \
        fat_p::debug_enforce_impl(false, "is_power_of_two(" #value ")", \
            std::source_location::current() __VA_OPT__(,) __VA_ARGS__); } while (0)
#define FATP_DEBUG_ENFORCE_IS_SORTED(container, ...)              \
    do { if (!fat_p::IsSortedPredicate::check(container)) [[unlikely]] \
        fat_p::debug_enforce_impl(false, "is_sorted(" #container ")", \
            std::source_location::current() __VA_OPT__(,) __VA_ARGS__); } while (0)
#define FATP_DEBUG_ENFORCE_IS_SORTED_WITH(comp, container, ...)   \
    do { if (!fat_p::IsSortedPredicate::check(container, comp)) [[unlikely]] \
        fat_p::debug_enforce_impl(false, "is_sorted_with(" #comp ", " #container ")", \
            std::source_location::current() __VA_OPT__(,) __VA_ARGS__); } while (0)
#define FATP_DEBUG_ENFORCE_IS_VALID_ITERATOR(it, end, ...)        \
    do { if (!fat_p::IsValidIteratorPredicate::check(it, end)) [[unlikely]] \
        fat_p::debug_enforce_impl(false, "is_valid_iterator(" #it ", " #end ")", \
            std::source_location::current() __VA_OPT__(,) __VA_ARGS__); } while (0)
#define FATP_DEBUG_ENFORCE_NOT_EMPTY(value, ...)                  \
    do { if (!fat_p::NotEmptyPredicate::check(value)) [[unlikely]] \
        fat_p::debug_enforce_impl(false, "not_empty(" #value ")", \
            std::source_location::current() __VA_OPT__(,) __VA_ARGS__); } while (0)
#define FATP_DEBUG_ENFORCE_NOT_NULL(ptr, ...)                     \
    do { if (!fat_p::NotNullPredicate::check(ptr)) [[unlikely]] \
        fat_p::debug_enforce_impl(false, "not_null(" #ptr ")", \
            std::source_location::current() __VA_OPT__(,) __VA_ARGS__); } while (0)
#define FATP_DEBUG_ENFORCE_IS_FINITE(value, ...)                  \
    do { if (!fat_p::IsFinitePredicate::check(value)) [[unlikely]] \
        fat_p::debug_enforce_impl(false, "is_finite(" #value ")", \
            std::source_location::current() __VA_OPT__(,) __VA_ARGS__); } while (0)
#define FATP_DEBUG_ENFORCE_IS_NORMAL(value, ...)                  \
    do { if (!fat_p::IsNormalPredicate::check(value)) [[unlikely]] \
        fat_p::debug_enforce_impl(false, "is_normal(" #value ")", \
            std::source_location::current() __VA_OPT__(,) __VA_ARGS__); } while (0)
#endif

// --- NotNull Multi-Policy Variants ----------------------------------------

#define FATP_WARN_ENFORCE_NOT_NULL(ptr, ...) \
    FATP_ENFORCE_PRED_IMPL_(WarningPolicy, fat_p::NotNullPredicate::check(ptr), "not_null(" #ptr ")", __VA_ARGS__)
#define FATP_NOEXCEPT_ENFORCE_NOT_NULL(ptr, ...) \
    FATP_ENFORCE_PRED_IMPL_(NoThrowPolicy, fat_p::NotNullPredicate::check(ptr), "not_null(" #ptr ")", __VA_ARGS__)
#define FATP_ABORT_ENFORCE_NOT_NULL(ptr, ...) \
    FATP_ENFORCE_PRED_IMPL_(AbortPolicy, fat_p::NotNullPredicate::check(ptr), "not_null(" #ptr ")", __VA_ARGS__)

} // namespace fat_p
