#pragma once

/*
FATP_META:
  meta_version: 1
  component: enforce_contextual
  file_role: public_header
  path: include/fat_p/enforce_contextual.h
  namespace: fat_p
  layer: Foundation
  summary: "Public header for enforce_contextual."
  api_stability: in_work
  related:
    docs_search: "enforce_contextual"
    tests:
      - components/Enforce/tests/test_Enforce.cpp
  hygiene:
    pragma_once: true
    include_guard: true
    defines_total: 72
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/

/**
 * @file enforce_contextual.h
 * @brief Defines macros and template functions for contextual contract enforcement that
 * automatically adapt failure behavior based on the function's noexcept specification.
 *
 * @details This system uses the FATP_CONTEXTUAL_RESOLVER metafunction to select the
 * appropriate raiser: NoThrowRaiser if the function is noexcept, or the default throwing
 * raiser (mapped from PredicateType or explicitly specified) unless explicitly overridden.
 */

#include <source_location>
#include <type_traits>
#include <utility>

#include "enforce_contextual_policies.h"
#include "enforce_enforcers.h"
#include "enforce_predicates.h"
#include "enforce_raiser_selector.h"
#include "Expected.h"

namespace fat_p
{

// ============================================================================
// Internal Implementation Macros
// ============================================================================
// These are not part of the public API. Use the FATP_CONTEXTUAL_* macros below.

// Resolves the final raiser type based on whether FunctionPtr is noexcept.
#define FATP_CONTEXTUAL_RESOLVER(FunctionPtr, ThrowingRaiserType)                           \
    typename fat_p::ContextualRaiserResolver<                                               \
        std::conditional_t<fat_p::is_noexcept_function_ptr_v<decltype((FunctionPtr))>,      \
                           fat_p::NoexceptFunctionPolicy,                                   \
                           fat_p::ThrowingFunctionPolicy>,                                  \
        ThrowingRaiserType>::type

// Predicate enforcement with automatic raiser selection from PredicateToRaiser.
#define FATP_CONTEXTUAL_PREDICATE_N(FunctionPtr, PredicateType, N, Targets, ...)              \
    do                                                                                        \
    {                                                                                         \
        if (!PredicateType::check Targets) [[unlikely]]                                       \
        {                                                                                     \
            using DefaultThrowingRaiser = typename fat_p::PredicateToRaiser<PredicateType>::type; \
            using FinalRaiser = FATP_CONTEXTUAL_RESOLVER(FunctionPtr, DefaultThrowingRaiser); \
            auto enforcer = fat_p::MakeEnforcer<FinalRaiser>(false,                           \
                                                             #PredicateType "(" #Targets ")", \
                                                             std::source_location::current()); \
            enforcer(__VA_ARGS__);                                                            \
        }                                                                                     \
    } while (0)

// Predicate enforcement with explicit abort raiser.
#define FATP_CONTEXTUAL_ABORT_N_IMPL(FunctionPtr, PredicateType, Targets, ...)            \
    do                                                                                    \
    {                                                                                     \
        if (!PredicateType::check Targets) [[unlikely]]                                   \
        {                                                                                 \
            using FinalRaiser = FATP_CONTEXTUAL_RESOLVER(FunctionPtr, fat_p::AbortRaiser); \
            auto enforcer = fat_p::MakeEnforcer<FinalRaiser>(false,                       \
                                                             #PredicateType "(" #Targets ")", \
                                                             std::source_location::current()); \
            enforcer(__VA_ARGS__);                                                        \
        }                                                                                 \
    } while (0)

// Predicate enforcement with debug-only policy (zero-cost when NoOpRaiser).
#define FATP_CONTEXTUAL_DEBUG_N_IMPL(FunctionPtr, PredicateType, Targets, ...)                                 \
    do                                                                                                         \
    {                                                                                                          \
        if constexpr (!std::is_same_v<fat_p::RaiserSelector<fat_p::DebugOnlyPolicy>::type, fat_p::NoOpRaiser>) \
        {                                                                                                      \
            if (!PredicateType::check Targets) [[unlikely]]                                                    \
            {                                                                                                  \
                using ThrowingRaiser = fat_p::RaiserSelector<fat_p::DebugOnlyPolicy>::type;                    \
                using FinalRaiser = FATP_CONTEXTUAL_RESOLVER(FunctionPtr, ThrowingRaiser);                     \
                auto enforcer = fat_p::MakeEnforcer<FinalRaiser>(false,                                        \
                                                                 #PredicateType "(" #Targets ")",              \
                                                                 std::source_location::current());             \
                enforcer(__VA_ARGS__);                                                                         \
            }                                                                                                  \
        }                                                                                                      \
    } while (0)

// Simple condition enforcement with explicit raiser type.
#define FATP_CONTEXTUAL_CONDITION_IMPL_(FunctionPtr, RaiserType, condition, ...)                      \
    do                                                                                               \
    {                                                                                                \
        if (!(condition)) [[unlikely]]                                                               \
        {                                                                                            \
            using FinalRaiser = FATP_CONTEXTUAL_RESOLVER(FunctionPtr, RaiserType);                   \
            auto enforcer = fat_p::MakeEnforcer<FinalRaiser>(false, #condition,                      \
                                                             std::source_location::current());       \
            enforcer(__VA_ARGS__);                                                                   \
        }                                                                                            \
    } while (0)

// Expected integration with predicate check â€” returns Expected<bool, string>.
#define FATP_CONTEXTUAL_EXPECTED_PRED_IMPL_(func_ptr, PredicateType, check_call, expr_str, ...) \
    ([&]() -> fat_p::Expected<bool, std::string> {                                              \
        auto fatp_pred_result_ = check_call;                                                    \
        if (!fatp_pred_result_)                                                                 \
        {                                                                                       \
            fat_p::MessageBuilder mb;                                                           \
            mb.format(__VA_ARGS__);                                                             \
            std::string msg = mb.get_message(std::source_location::current(), expr_str);        \
            fat_p::detail::writeToStderr("Expected Failure: ", msg);                            \
            return fat_p::unexpected(msg);                                                      \
        }                                                                                       \
        return fatp_pred_result_;                                                               \
    })()

// ============================================================================
// I. Simple Condition Checks
// ============================================================================

#define FATP_CONTEXTUAL_ENFORCE(func_ptr, condition, ...) \
    FATP_CONTEXTUAL_CONDITION_IMPL_(func_ptr, fat_p::LogicRaiser, condition, __VA_ARGS__)

#define FATP_CONTEXTUAL_ENFORCE_INVALID_ARG(func_ptr, condition, ...) \
    FATP_CONTEXTUAL_CONDITION_IMPL_(func_ptr, fat_p::InvalidArgumentRaiser, condition, __VA_ARGS__)

#define FATP_CONTEXTUAL_ABORT(func_ptr, condition, ...) \
    FATP_CONTEXTUAL_CONDITION_IMPL_(func_ptr, fat_p::AbortRaiser, condition, __VA_ARGS__)

#define FATP_CONTEXTUAL_DEBUG(func_ptr, condition, ...)                                                        \
    do                                                                                                         \
    {                                                                                                          \
        if constexpr (!std::is_same_v<fat_p::RaiserSelector<fat_p::DebugOnlyPolicy>::type, fat_p::NoOpRaiser>) \
        {                                                                                                      \
            if (!(condition)) [[unlikely]]                                                                     \
            {                                                                                                  \
                using ThrowingRaiser = fat_p::RaiserSelector<fat_p::DebugOnlyPolicy>::type;                    \
                using FinalRaiser = FATP_CONTEXTUAL_RESOLVER(func_ptr, ThrowingRaiser);                        \
                auto enforcer = fat_p::MakeEnforcer<FinalRaiser>(false, #condition,                            \
                                                                 std::source_location::current());             \
                enforcer(__VA_ARGS__);                                                                         \
            }                                                                                                  \
        }                                                                                                      \
    } while (0)

// ============================================================================
// II. Generic Predicate Checks (by arity)
// ============================================================================

// --- Enforce (auto-selects raiser from PredicateToRaiser) ---
#define FATP_CONTEXTUAL_ENFORCE_1(fp, Pred, t, ...) \
    FATP_CONTEXTUAL_PREDICATE_N(fp, Pred, 1, (t), __VA_ARGS__)
#define FATP_CONTEXTUAL_ENFORCE_2(fp, Pred, t1, t2, ...) \
    FATP_CONTEXTUAL_PREDICATE_N(fp, Pred, 2, (t1, t2), __VA_ARGS__)
#define FATP_CONTEXTUAL_ENFORCE_3(fp, Pred, t1, t2, t3, ...) \
    FATP_CONTEXTUAL_PREDICATE_N(fp, Pred, 3, (t1, t2, t3), __VA_ARGS__)
#define FATP_CONTEXTUAL_ENFORCE_4(fp, Pred, t1, t2, t3, t4, ...) \
    FATP_CONTEXTUAL_PREDICATE_N(fp, Pred, 4, (t1, t2, t3, t4), __VA_ARGS__)
#define FATP_CONTEXTUAL_ENFORCE_5(fp, Pred, t1, t2, t3, t4, t5, ...) \
    FATP_CONTEXTUAL_PREDICATE_N(fp, Pred, 5, (t1, t2, t3, t4, t5), __VA_ARGS__)

// --- Abort ---
#define FATP_CONTEXTUAL_ABORT_1(fp, Pred, t, ...) \
    FATP_CONTEXTUAL_ABORT_N_IMPL(fp, Pred, (t), __VA_ARGS__)
#define FATP_CONTEXTUAL_ABORT_2(fp, Pred, t1, t2, ...) \
    FATP_CONTEXTUAL_ABORT_N_IMPL(fp, Pred, (t1, t2), __VA_ARGS__)
#define FATP_CONTEXTUAL_ABORT_3(fp, Pred, t1, t2, t3, ...) \
    FATP_CONTEXTUAL_ABORT_N_IMPL(fp, Pred, (t1, t2, t3), __VA_ARGS__)
#define FATP_CONTEXTUAL_ABORT_4(fp, Pred, t1, t2, t3, t4, ...) \
    FATP_CONTEXTUAL_ABORT_N_IMPL(fp, Pred, (t1, t2, t3, t4), __VA_ARGS__)
#define FATP_CONTEXTUAL_ABORT_5(fp, Pred, t1, t2, t3, t4, t5, ...) \
    FATP_CONTEXTUAL_ABORT_N_IMPL(fp, Pred, (t1, t2, t3, t4, t5), __VA_ARGS__)

// --- Debug ---
#define FATP_CONTEXTUAL_DEBUG_1(fp, Pred, t, ...) \
    FATP_CONTEXTUAL_DEBUG_N_IMPL(fp, Pred, (t), __VA_ARGS__)
#define FATP_CONTEXTUAL_DEBUG_2(fp, Pred, t1, t2, ...) \
    FATP_CONTEXTUAL_DEBUG_N_IMPL(fp, Pred, (t1, t2), __VA_ARGS__)
#define FATP_CONTEXTUAL_DEBUG_3(fp, Pred, t1, t2, t3, ...) \
    FATP_CONTEXTUAL_DEBUG_N_IMPL(fp, Pred, (t1, t2, t3), __VA_ARGS__)
#define FATP_CONTEXTUAL_DEBUG_4(fp, Pred, t1, t2, t3, t4, ...) \
    FATP_CONTEXTUAL_DEBUG_N_IMPL(fp, Pred, (t1, t2, t3, t4), __VA_ARGS__)
#define FATP_CONTEXTUAL_DEBUG_5(fp, Pred, t1, t2, t3, t4, t5, ...) \
    FATP_CONTEXTUAL_DEBUG_N_IMPL(fp, Pred, (t1, t2, t3, t4, t5), __VA_ARGS__)

// ============================================================================
// III. Per-Predicate Convenience Checks
// ============================================================================
// Thin wrappers over the generic arity macros above. Grouped by policy tier.
// For predicates not listed here, use FATP_CONTEXTUAL_{ENFORCE|ABORT|DEBUG}_N.

// --- Enforce ---
#define FATP_CONTEXTUAL_ENFORCE_NOT_NULL(fp, ptr, ...)                  FATP_CONTEXTUAL_ENFORCE_1(fp, fat_p::NotNullPredicate, ptr, __VA_ARGS__)
#define FATP_CONTEXTUAL_ENFORCE_IS_POSITIVE(fp, val, ...)               FATP_CONTEXTUAL_ENFORCE_1(fp, fat_p::IsPositivePredicate, val, __VA_ARGS__)
#define FATP_CONTEXTUAL_ENFORCE_IS_NON_NEGATIVE(fp, val, ...)           FATP_CONTEXTUAL_ENFORCE_1(fp, fat_p::IsNonNegativePredicate, val, __VA_ARGS__)
#define FATP_CONTEXTUAL_ENFORCE_IS_POWER_OF_TWO(fp, val, ...)           FATP_CONTEXTUAL_ENFORCE_1(fp, fat_p::IsPowerOfTwoPredicate, val, __VA_ARGS__)
#define FATP_CONTEXTUAL_ENFORCE_IS_UNIQUE(fp, ctr, ...)                 FATP_CONTEXTUAL_ENFORCE_1(fp, fat_p::ContainerIsUniquePredicate, ctr, __VA_ARGS__)
#define FATP_CONTEXTUAL_ENFORCE_ALL_SATISFY(fp, pred, range, ...)       FATP_CONTEXTUAL_ENFORCE_2(fp, fat_p::AllSatisfyPredicate, pred, range, __VA_ARGS__)
#define FATP_CONTEXTUAL_ENFORCE_HAS_SIZE(fp, sz, ctr, ...)              FATP_CONTEXTUAL_ENFORCE_2(fp, fat_p::HasSizePredicate, sz, ctr, __VA_ARGS__)
#define FATP_CONTEXTUAL_ENFORCE_IS_LESS_THAN(fp, lhs, rhs, ...)        FATP_CONTEXTUAL_ENFORCE_2(fp, fat_p::IsLessThanPredicate, lhs, rhs, __VA_ARGS__)
#define FATP_CONTEXTUAL_ENFORCE_IS_GREATER_THAN(fp, lhs, rhs, ...)     FATP_CONTEXTUAL_ENFORCE_2(fp, fat_p::IsGreaterThanPredicate, lhs, rhs, __VA_ARGS__)
#define FATP_CONTEXTUAL_ENFORCE_IS_LESS_THAN_OR_EQUAL(fp, lhs, rhs, ...)    FATP_CONTEXTUAL_ENFORCE_2(fp, fat_p::IsLessThanOrEqualPredicate, lhs, rhs, __VA_ARGS__)
#define FATP_CONTEXTUAL_ENFORCE_IS_GREATER_THAN_OR_EQUAL(fp, lhs, rhs, ...) FATP_CONTEXTUAL_ENFORCE_2(fp, fat_p::IsGreaterThanOrEqualPredicate, lhs, rhs, __VA_ARGS__)
#define FATP_CONTEXTUAL_ENFORCE_BETWEEN(fp, val, min, max, ...)         FATP_CONTEXTUAL_ENFORCE_3(fp, fat_p::InRangePredicate, val, min, max, __VA_ARGS__)
#define FATP_CONTEXTUAL_ENFORCE_APPROX_EQUAL(fp, tol, lhs, rhs, ...)   FATP_CONTEXTUAL_ENFORCE_3(fp, fat_p::ApproxEqualPredicate, tol, lhs, rhs, __VA_ARGS__)

// --- Abort ---
#define FATP_CONTEXTUAL_ABORT_NOT_NULL(fp, ptr, ...)                    FATP_CONTEXTUAL_ABORT_1(fp, fat_p::NotNullPredicate, ptr, __VA_ARGS__)
#define FATP_CONTEXTUAL_ABORT_IS_POSITIVE(fp, val, ...)                 FATP_CONTEXTUAL_ABORT_1(fp, fat_p::IsPositivePredicate, val, __VA_ARGS__)
#define FATP_CONTEXTUAL_ABORT_IS_NON_NEGATIVE(fp, val, ...)             FATP_CONTEXTUAL_ABORT_1(fp, fat_p::IsNonNegativePredicate, val, __VA_ARGS__)
#define FATP_CONTEXTUAL_ABORT_IS_POWER_OF_TWO(fp, val, ...)             FATP_CONTEXTUAL_ABORT_1(fp, fat_p::IsPowerOfTwoPredicate, val, __VA_ARGS__)
#define FATP_CONTEXTUAL_ABORT_IS_UNIQUE(fp, ctr, ...)                   FATP_CONTEXTUAL_ABORT_1(fp, fat_p::ContainerIsUniquePredicate, ctr, __VA_ARGS__)
#define FATP_CONTEXTUAL_ABORT_ALL_SATISFY(fp, pred, range, ...)         FATP_CONTEXTUAL_ABORT_2(fp, fat_p::AllSatisfyPredicate, pred, range, __VA_ARGS__)
#define FATP_CONTEXTUAL_ABORT_HAS_SIZE(fp, sz, ctr, ...)                FATP_CONTEXTUAL_ABORT_2(fp, fat_p::HasSizePredicate, sz, ctr, __VA_ARGS__)
#define FATP_CONTEXTUAL_ABORT_IS_LESS_THAN(fp, lhs, rhs, ...)          FATP_CONTEXTUAL_ABORT_2(fp, fat_p::IsLessThanPredicate, lhs, rhs, __VA_ARGS__)
#define FATP_CONTEXTUAL_ABORT_IS_GREATER_THAN(fp, lhs, rhs, ...)       FATP_CONTEXTUAL_ABORT_2(fp, fat_p::IsGreaterThanPredicate, lhs, rhs, __VA_ARGS__)
#define FATP_CONTEXTUAL_ABORT_IS_LESS_THAN_OR_EQUAL(fp, lhs, rhs, ...) FATP_CONTEXTUAL_ABORT_2(fp, fat_p::IsLessThanOrEqualPredicate, lhs, rhs, __VA_ARGS__)
#define FATP_CONTEXTUAL_ABORT_IS_GREATER_THAN_OR_EQUAL(fp, lhs, rhs, ...) FATP_CONTEXTUAL_ABORT_2(fp, fat_p::IsGreaterThanOrEqualPredicate, lhs, rhs, __VA_ARGS__)
#define FATP_CONTEXTUAL_ABORT_BETWEEN(fp, val, min, max, ...)           FATP_CONTEXTUAL_ABORT_3(fp, fat_p::InRangePredicate, val, min, max, __VA_ARGS__)
#define FATP_CONTEXTUAL_ABORT_APPROX_EQUAL(fp, tol, lhs, rhs, ...)     FATP_CONTEXTUAL_ABORT_3(fp, fat_p::ApproxEqualPredicate, tol, lhs, rhs, __VA_ARGS__)

// --- Debug ---
#define FATP_CONTEXTUAL_DEBUG_NOT_NULL(fp, ptr, ...)                    FATP_CONTEXTUAL_DEBUG_1(fp, fat_p::NotNullPredicate, ptr, __VA_ARGS__)
#define FATP_CONTEXTUAL_DEBUG_IS_POSITIVE(fp, val, ...)                 FATP_CONTEXTUAL_DEBUG_1(fp, fat_p::IsPositivePredicate, val, __VA_ARGS__)
#define FATP_CONTEXTUAL_DEBUG_IS_NON_NEGATIVE(fp, val, ...)             FATP_CONTEXTUAL_DEBUG_1(fp, fat_p::IsNonNegativePredicate, val, __VA_ARGS__)
#define FATP_CONTEXTUAL_DEBUG_IS_POWER_OF_TWO(fp, val, ...)             FATP_CONTEXTUAL_DEBUG_1(fp, fat_p::IsPowerOfTwoPredicate, val, __VA_ARGS__)
#define FATP_CONTEXTUAL_DEBUG_IS_UNIQUE(fp, ctr, ...)                   FATP_CONTEXTUAL_DEBUG_1(fp, fat_p::ContainerIsUniquePredicate, ctr, __VA_ARGS__)
#define FATP_CONTEXTUAL_DEBUG_ALL_SATISFY(fp, pred, range, ...)         FATP_CONTEXTUAL_DEBUG_2(fp, fat_p::AllSatisfyPredicate, pred, range, __VA_ARGS__)
#define FATP_CONTEXTUAL_DEBUG_HAS_SIZE(fp, sz, ctr, ...)                FATP_CONTEXTUAL_DEBUG_2(fp, fat_p::HasSizePredicate, sz, ctr, __VA_ARGS__)
#define FATP_CONTEXTUAL_DEBUG_IS_LESS_THAN(fp, lhs, rhs, ...)          FATP_CONTEXTUAL_DEBUG_2(fp, fat_p::IsLessThanPredicate, lhs, rhs, __VA_ARGS__)
#define FATP_CONTEXTUAL_DEBUG_IS_GREATER_THAN(fp, lhs, rhs, ...)       FATP_CONTEXTUAL_DEBUG_2(fp, fat_p::IsGreaterThanPredicate, lhs, rhs, __VA_ARGS__)
#define FATP_CONTEXTUAL_DEBUG_IS_LESS_THAN_OR_EQUAL(fp, lhs, rhs, ...) FATP_CONTEXTUAL_DEBUG_2(fp, fat_p::IsLessThanOrEqualPredicate, lhs, rhs, __VA_ARGS__)
#define FATP_CONTEXTUAL_DEBUG_IS_GREATER_THAN_OR_EQUAL(fp, lhs, rhs, ...) FATP_CONTEXTUAL_DEBUG_2(fp, fat_p::IsGreaterThanOrEqualPredicate, lhs, rhs, __VA_ARGS__)
#define FATP_CONTEXTUAL_DEBUG_BETWEEN(fp, val, min, max, ...)           FATP_CONTEXTUAL_DEBUG_3(fp, fat_p::InRangePredicate, val, min, max, __VA_ARGS__)
#define FATP_CONTEXTUAL_DEBUG_APPROX_EQUAL(fp, tol, lhs, rhs, ...)     FATP_CONTEXTUAL_DEBUG_3(fp, fat_p::ApproxEqualPredicate, tol, lhs, rhs, __VA_ARGS__)

// ============================================================================
// IV. Expected Integration Checks
// ============================================================================

#define FATP_CONTEXTUAL_ENFORCE_EXPECTED(func_ptr, condition, ...)              \
    ([&]() -> fat_p::Expected<void, std::string> {                              \
        if (!(condition))                                                       \
        {                                                                       \
            fat_p::MessageBuilder mb;                                           \
            mb.format(__VA_ARGS__);                                             \
            std::string msg = mb.get_message(std::source_location::current(),   \
                                             #condition);                       \
            fat_p::detail::writeToStderr("Expected Failure: ", msg);            \
            return fat_p::unexpected(msg);                                      \
        }                                                                       \
        return {};                                                              \
    })()

#define FATP_CONTEXTUAL_ENFORCE_EXPECTED_1(fp, Pred, t, ...) \
    FATP_CONTEXTUAL_EXPECTED_PRED_IMPL_(fp, Pred, Pred::check(t), #Pred "(" #t ")", __VA_ARGS__)

#define FATP_CONTEXTUAL_ENFORCE_EXPECTED_2(fp, Pred, t1, t2, ...) \
    FATP_CONTEXTUAL_EXPECTED_PRED_IMPL_(fp, Pred, Pred::check(t1, t2), #Pred "(" #t1 ", " #t2 ")", __VA_ARGS__)

#define FATP_CONTEXTUAL_ENFORCE_EXPECTED_3(fp, Pred, t1, t2, t3, ...) \
    FATP_CONTEXTUAL_EXPECTED_PRED_IMPL_(fp, Pred, Pred::check(t1, t2, t3), #Pred "(" #t1 ", " #t2 ", " #t3 ")", __VA_ARGS__)

#define FATP_CONTEXTUAL_ENFORCE_EXPECTED_4(fp, Pred, t1, t2, t3, t4, ...) \
    FATP_CONTEXTUAL_EXPECTED_PRED_IMPL_(fp, Pred, Pred::check(t1, t2, t3, t4), #Pred "(" #t1 ", " #t2 ", " #t3 ", " #t4 ")", __VA_ARGS__)

#define FATP_CONTEXTUAL_ENFORCE_EXPECTED_5(fp, Pred, t1, t2, t3, t4, t5, ...) \
    FATP_CONTEXTUAL_EXPECTED_PRED_IMPL_(fp, Pred, Pred::check(t1, t2, t3, t4, t5), #Pred "(" #t1 ", " #t2 ", " #t3 ", " #t4 ", " #t5 ")", __VA_ARGS__)

#define FATP_CONTEXTUAL_ENFORCE_EXPECTED_NOT_NULL(func_ptr, ptr, ...) \
    FATP_CONTEXTUAL_ENFORCE_EXPECTED_1(func_ptr, fat_p::NotNullPredicate, ptr, __VA_ARGS__)

} // namespace fat_p
