/**
 * @file enforce_contextual.h
 * @brief Defines macros and template functions for contextual contract enforcement that
 * automatically adapt failure behavior based on the function's noexcept specification.
 * @layer Foundation
 *
 * @details This system uses the FATP_CONTEXTUAL_RESOLVER metafunction to select the
 * appropriate raiser: NoThrowRaiser if the function is noexcept, or the default throwing
 * raiser (mapped from PredicateType or explicitly specified) unless explicitly overridden.
 */
#pragma once

/*
FATP_META:
  meta_version: 1
  component: enforce_contextual
  file_role: public_header
  path: fat_p/enforce_contextual.h
  namespace: fat_p
  layer: Foundation
  summary: "Public header for enforce_contextual."
  api_stability: in_work
  related:
    docs_search: "enforce_contextual"
    tests:
      - tests/test_Enforce.cpp
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
#include <type_traits>
#include <utility>

#include "enforce_contextual_policies.h"
#include "enforce_enforcers.h"
#include "enforce_predicates.h"
#include "enforce_raiser_selector.h"
#include "Expected.h"

namespace fat_p {

#ifndef FATP_LOCUS
#define FATP_LOCUS __FILE__ ":" FATP_STRINGIFY(__LINE__)
#define FATP_STRINGIFY(x) FATP_TOSTRING(x)
#define FATP_TOSTRING(x) #x
#endif

// --- Internal Contextual Resolver Factory ---
#define FATP_CONTEXTUAL_RESOLVER(FunctionPtr, ThrowingRaiserType)                                  \
    typename fat_p::ContextualRaiserResolver<                                                      \
        std::conditional_t<fat_p::is_noexcept_function_ptr<decltype((FunctionPtr))>::value,        \
                           fat_p::NoexceptFunctionPolicy,                                          \
                           fat_p::ThrowingFunctionPolicy>,                                         \
        ThrowingRaiserType>::type

// --- Internal Predicate Enforcement Helper (N-argument) ---
#define FATP_CONTEXTUAL_PREDICATE_N(FunctionPtr, PredicateType, N, Targets, ...)                   \
    do                                                                                             \
    {                                                                                              \
        using DefaultThrowingRaiser = typename fat_p::PredicateToRaiser<PredicateType>::type;      \
                                                                                                   \
        using FinalRaiser = FATP_CONTEXTUAL_RESOLVER(FunctionPtr, DefaultThrowingRaiser);          \
                                                                                                   \
        auto enforcer = fat_p::MakeEnforcer<FinalRaiser>(                                          \
            PredicateType::check Targets, #PredicateType "(" #Targets ")", FATP_LOCUS);            \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

// --- Internal Helper for Explicit Policy (Abort) with Predicates ---
#define FATP_CONTEXTUAL_ABORT_N_IMPL(FunctionPtr, PredicateType, Targets, ...)                     \
    do                                                                                             \
    {                                                                                              \
        using FinalRaiser = FATP_CONTEXTUAL_RESOLVER(FunctionPtr, fat_p::AbortRaiser);             \
                                                                                                   \
        auto enforcer = fat_p::MakeEnforcer<FinalRaiser>(                                          \
            PredicateType::check Targets, #PredicateType "(" #Targets ")", FATP_LOCUS);            \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

// --- Internal Helper for Debug Policy with Predicates ---
#define FATP_CONTEXTUAL_DEBUG_N_IMPL(FunctionPtr, PredicateType, Targets, ...)                     \
    do                                                                                             \
    {                                                                                              \
        if constexpr (!std::is_same_v<fat_p::RaiserSelector<fat_p::DebugOnlyPolicy>::type,         \
                                      fat_p::NoOpRaiser>)                                          \
        {                                                                                          \
            using ThrowingRaiser = fat_p::RaiserSelector<fat_p::DebugOnlyPolicy>::type;            \
            using FinalRaiser = FATP_CONTEXTUAL_RESOLVER(FunctionPtr, ThrowingRaiser);             \
                                                                                                   \
            auto enforcer = fat_p::MakeEnforcer<FinalRaiser>(                                      \
                PredicateType::check Targets, #PredicateType "(" #Targets ")", FATP_LOCUS);        \
            enforcer(__VA_ARGS__);                                                                 \
        }                                                                                          \
    } while (0)

// ============================================================================
// I. Contextual Simple Condition Checks
// ============================================================================

#define FATP_CONTEXTUAL_ENFORCE(func_ptr, condition, ...)                                          \
    do                                                                                             \
    {                                                                                              \
        using FinalRaiser = FATP_CONTEXTUAL_RESOLVER(func_ptr, fat_p::LogicRaiser);                \
        auto enforcer =                                                                            \
            fat_p::MakeEnforcer<FinalRaiser>((condition), #condition, FATP_LOCUS);                 \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#define FATP_CONTEXTUAL_ENFORCE_INVALID_ARG(func_ptr, condition, ...)                              \
    do                                                                                             \
    {                                                                                              \
        using FinalRaiser = FATP_CONTEXTUAL_RESOLVER(func_ptr, fat_p::InvalidArgumentRaiser);      \
        auto enforcer =                                                                            \
            fat_p::MakeEnforcer<FinalRaiser>((condition), #condition, FATP_LOCUS);                 \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

// ============================================================================
// II. Contextual Generic Predicate Checks
// ============================================================================

#define FATP_CONTEXTUAL_ENFORCE_1(func_ptr, PredicateType, target, ...)                            \
    FATP_CONTEXTUAL_PREDICATE_N(func_ptr, PredicateType, 1, (target), __VA_ARGS__)

#define FATP_CONTEXTUAL_ENFORCE_2(func_ptr, PredicateType, target1, target2, ...)                  \
    FATP_CONTEXTUAL_PREDICATE_N(func_ptr, PredicateType, 2, (target1, target2), __VA_ARGS__)

#define FATP_CONTEXTUAL_ENFORCE_3(func_ptr, PredicateType, target1, target2, target3, ...)         \
    FATP_CONTEXTUAL_PREDICATE_N(                                                                   \
        func_ptr, PredicateType, 3, (target1, target2, target3), __VA_ARGS__)

#define FATP_CONTEXTUAL_ENFORCE_4(func_ptr, PredicateType, target1, target2, target3, target4, ...)\
    FATP_CONTEXTUAL_PREDICATE_N(                                                                   \
        func_ptr, PredicateType, 4, (target1, target2, target3, target4), __VA_ARGS__)

#define FATP_CONTEXTUAL_ENFORCE_5(                                                                 \
    func_ptr, PredicateType, target1, target2, target3, target4, target5, ...)                     \
    FATP_CONTEXTUAL_PREDICATE_N(                                                                   \
        func_ptr, PredicateType, 5, (target1, target2, target3, target4, target5), __VA_ARGS__)

// ============================================================================
// III. Contextual Abort Policy Checks
// ============================================================================

#define FATP_CONTEXTUAL_ABORT(func_ptr, condition, ...)                                            \
    do                                                                                             \
    {                                                                                              \
        using FinalRaiser = FATP_CONTEXTUAL_RESOLVER(func_ptr, fat_p::AbortRaiser);                \
        auto enforcer =                                                                            \
            fat_p::MakeEnforcer<FinalRaiser>((condition), #condition, FATP_LOCUS);                 \
        enforcer(__VA_ARGS__);                                                                     \
    } while (0)

#define FATP_CONTEXTUAL_ABORT_1(func_ptr, PredicateType, target, ...)                              \
    FATP_CONTEXTUAL_ABORT_N_IMPL(func_ptr, PredicateType, (target), __VA_ARGS__)

#define FATP_CONTEXTUAL_ABORT_2(func_ptr, PredicateType, target1, target2, ...)                    \
    FATP_CONTEXTUAL_ABORT_N_IMPL(func_ptr, PredicateType, (target1, target2), __VA_ARGS__)

#define FATP_CONTEXTUAL_ABORT_3(func_ptr, PredicateType, target1, target2, target3, ...)           \
    FATP_CONTEXTUAL_ABORT_N_IMPL(func_ptr, PredicateType, (target1, target2, target3), __VA_ARGS__)

#define FATP_CONTEXTUAL_ABORT_4(func_ptr, PredicateType, target1, target2, target3, target4, ...)  \
    FATP_CONTEXTUAL_ABORT_N_IMPL(                                                                  \
        func_ptr, PredicateType, (target1, target2, target3, target4), __VA_ARGS__)

#define FATP_CONTEXTUAL_ABORT_5(                                                                   \
    func_ptr, PredicateType, target1, target2, target3, target4, target5, ...)                     \
    FATP_CONTEXTUAL_ABORT_N_IMPL(                                                                  \
        func_ptr, PredicateType, (target1, target2, target3, target4, target5), __VA_ARGS__)

// ============================================================================
// IV. Contextual Debug Policy Checks
// ============================================================================

#define FATP_CONTEXTUAL_DEBUG(func_ptr, condition, ...)                                            \
    do                                                                                             \
    {                                                                                              \
        if constexpr (!std::is_same_v<fat_p::RaiserSelector<fat_p::DebugOnlyPolicy>::type,         \
                                      fat_p::NoOpRaiser>)                                          \
        {                                                                                          \
            using ThrowingRaiser = fat_p::RaiserSelector<fat_p::DebugOnlyPolicy>::type;            \
            using FinalRaiser = FATP_CONTEXTUAL_RESOLVER(func_ptr, ThrowingRaiser);                \
            auto enforcer =                                                                        \
                fat_p::MakeEnforcer<FinalRaiser>((condition), #condition, FATP_LOCUS);             \
            enforcer(__VA_ARGS__);                                                                 \
        }                                                                                          \
    } while (0)

#define FATP_CONTEXTUAL_DEBUG_1(func_ptr, PredicateType, target, ...)                              \
    FATP_CONTEXTUAL_DEBUG_N_IMPL(func_ptr, PredicateType, (target), __VA_ARGS__)

#define FATP_CONTEXTUAL_DEBUG_2(func_ptr, PredicateType, target1, target2, ...)                    \
    FATP_CONTEXTUAL_DEBUG_N_IMPL(func_ptr, PredicateType, (target1, target2), __VA_ARGS__)

#define FATP_CONTEXTUAL_DEBUG_3(func_ptr, PredicateType, target1, target2, target3, ...)           \
    FATP_CONTEXTUAL_DEBUG_N_IMPL(func_ptr, PredicateType, (target1, target2, target3), __VA_ARGS__)

#define FATP_CONTEXTUAL_DEBUG_4(func_ptr, PredicateType, target1, target2, target3, target4, ...)  \
    FATP_CONTEXTUAL_DEBUG_N_IMPL(                                                                  \
        func_ptr, PredicateType, (target1, target2, target3, target4), __VA_ARGS__)

#define FATP_CONTEXTUAL_DEBUG_5(                                                                   \
    func_ptr, PredicateType, target1, target2, target3, target4, target5, ...)                     \
    FATP_CONTEXTUAL_DEBUG_N_IMPL(                                                                  \
        func_ptr, PredicateType, (target1, target2, target3, target4, target5), __VA_ARGS__)

// ============================================================================
// V. Contextual Convenience Predicate Checks
// ============================================================================

// NotNull
#define FATP_CONTEXTUAL_ENFORCE_NOT_NULL(func_ptr, ptr, ...)                                       \
    FATP_CONTEXTUAL_ENFORCE_1(func_ptr, fat_p::NotNullPredicate, ptr, __VA_ARGS__)
#define FATP_CONTEXTUAL_ABORT_NOT_NULL(func_ptr, ptr, ...)                                         \
    FATP_CONTEXTUAL_ABORT_1(func_ptr, fat_p::NotNullPredicate, ptr, __VA_ARGS__)
#define FATP_CONTEXTUAL_DEBUG_NOT_NULL(func_ptr, ptr, ...)                                         \
    FATP_CONTEXTUAL_DEBUG_1(func_ptr, fat_p::NotNullPredicate, ptr, __VA_ARGS__)

// IsPositive
#define FATP_CONTEXTUAL_ENFORCE_IS_POSITIVE(func_ptr, value, ...)                                  \
    FATP_CONTEXTUAL_ENFORCE_1(func_ptr, fat_p::IsPositivePredicate, value, __VA_ARGS__)
#define FATP_CONTEXTUAL_ABORT_IS_POSITIVE(func_ptr, value, ...)                                    \
    FATP_CONTEXTUAL_ABORT_1(func_ptr, fat_p::IsPositivePredicate, value, __VA_ARGS__)
#define FATP_CONTEXTUAL_DEBUG_IS_POSITIVE(func_ptr, value, ...)                                    \
    FATP_CONTEXTUAL_DEBUG_1(func_ptr, fat_p::IsPositivePredicate, value, __VA_ARGS__)

// IsNonNegative
#define FATP_CONTEXTUAL_ENFORCE_IS_NON_NEGATIVE(func_ptr, value, ...)                              \
    FATP_CONTEXTUAL_ENFORCE_1(func_ptr, fat_p::IsNonNegativePredicate, value, __VA_ARGS__)
#define FATP_CONTEXTUAL_ABORT_IS_NON_NEGATIVE(func_ptr, value, ...)                                \
    FATP_CONTEXTUAL_ABORT_1(func_ptr, fat_p::IsNonNegativePredicate, value, __VA_ARGS__)
#define FATP_CONTEXTUAL_DEBUG_IS_NON_NEGATIVE(func_ptr, value, ...)                                \
    FATP_CONTEXTUAL_DEBUG_1(func_ptr, fat_p::IsNonNegativePredicate, value, __VA_ARGS__)

// IsPowerOfTwo
#define FATP_CONTEXTUAL_ENFORCE_IS_POWER_OF_TWO(func_ptr, value, ...)                              \
    FATP_CONTEXTUAL_ENFORCE_1(func_ptr, fat_p::IsPowerOfTwoPredicate, value, __VA_ARGS__)
#define FATP_CONTEXTUAL_ABORT_IS_POWER_OF_TWO(func_ptr, value, ...)                                \
    FATP_CONTEXTUAL_ABORT_1(func_ptr, fat_p::IsPowerOfTwoPredicate, value, __VA_ARGS__)
#define FATP_CONTEXTUAL_DEBUG_IS_POWER_OF_TWO(func_ptr, value, ...)                                \
    FATP_CONTEXTUAL_DEBUG_1(func_ptr, fat_p::IsPowerOfTwoPredicate, value, __VA_ARGS__)

// ContainerIsUnique
#define FATP_CONTEXTUAL_ENFORCE_IS_UNIQUE(func_ptr, container, ...)                                \
    FATP_CONTEXTUAL_ENFORCE_1(func_ptr, fat_p::ContainerIsUniquePredicate, container, __VA_ARGS__)
#define FATP_CONTEXTUAL_ABORT_IS_UNIQUE(func_ptr, container, ...)                                  \
    FATP_CONTEXTUAL_ABORT_1(func_ptr, fat_p::ContainerIsUniquePredicate, container, __VA_ARGS__)
#define FATP_CONTEXTUAL_DEBUG_IS_UNIQUE(func_ptr, container, ...)                                  \
    FATP_CONTEXTUAL_DEBUG_1(func_ptr, fat_p::ContainerIsUniquePredicate, container, __VA_ARGS__)

// AllSatisfy
#define FATP_CONTEXTUAL_ENFORCE_ALL_SATISFY(func_ptr, pred, range, ...)                            \
    FATP_CONTEXTUAL_ENFORCE_2(func_ptr, fat_p::AllSatisfyPredicate, pred, range, __VA_ARGS__)
#define FATP_CONTEXTUAL_ABORT_ALL_SATISFY(func_ptr, pred, range, ...)                              \
    FATP_CONTEXTUAL_ABORT_2(func_ptr, fat_p::AllSatisfyPredicate, pred, range, __VA_ARGS__)
#define FATP_CONTEXTUAL_DEBUG_ALL_SATISFY(func_ptr, pred, range, ...)                              \
    FATP_CONTEXTUAL_DEBUG_2(func_ptr, fat_p::AllSatisfyPredicate, pred, range, __VA_ARGS__)

// HasSize
#define FATP_CONTEXTUAL_ENFORCE_HAS_SIZE(func_ptr, expected_size, container, ...)                  \
    FATP_CONTEXTUAL_ENFORCE_2(func_ptr, fat_p::HasSizePredicate, expected_size, container, __VA_ARGS__)
#define FATP_CONTEXTUAL_ABORT_HAS_SIZE(func_ptr, expected_size, container, ...)                    \
    FATP_CONTEXTUAL_ABORT_2(func_ptr, fat_p::HasSizePredicate, expected_size, container, __VA_ARGS__)
#define FATP_CONTEXTUAL_DEBUG_HAS_SIZE(func_ptr, expected_size, container, ...)                    \
    FATP_CONTEXTUAL_DEBUG_2(func_ptr, fat_p::HasSizePredicate, expected_size, container, __VA_ARGS__)

// IsLessThan
#define FATP_CONTEXTUAL_ENFORCE_IS_LESS_THAN(func_ptr, lhs, rhs, ...)                              \
    FATP_CONTEXTUAL_ENFORCE_2(func_ptr, fat_p::IsLessThanPredicate, lhs, rhs, __VA_ARGS__)
#define FATP_CONTEXTUAL_ABORT_IS_LESS_THAN(func_ptr, lhs, rhs, ...)                                \
    FATP_CONTEXTUAL_ABORT_2(func_ptr, fat_p::IsLessThanPredicate, lhs, rhs, __VA_ARGS__)
#define FATP_CONTEXTUAL_DEBUG_IS_LESS_THAN(func_ptr, lhs, rhs, ...)                                \
    FATP_CONTEXTUAL_DEBUG_2(func_ptr, fat_p::IsLessThanPredicate, lhs, rhs, __VA_ARGS__)

// IsGreaterThan
#define FATP_CONTEXTUAL_ENFORCE_IS_GREATER_THAN(func_ptr, lhs, rhs, ...)                           \
    FATP_CONTEXTUAL_ENFORCE_2(func_ptr, fat_p::IsGreaterThanPredicate, lhs, rhs, __VA_ARGS__)
#define FATP_CONTEXTUAL_ABORT_IS_GREATER_THAN(func_ptr, lhs, rhs, ...)                             \
    FATP_CONTEXTUAL_ABORT_2(func_ptr, fat_p::IsGreaterThanPredicate, lhs, rhs, __VA_ARGS__)
#define FATP_CONTEXTUAL_DEBUG_IS_GREATER_THAN(func_ptr, lhs, rhs, ...)                             \
    FATP_CONTEXTUAL_DEBUG_2(func_ptr, fat_p::IsGreaterThanPredicate, lhs, rhs, __VA_ARGS__)

// IsLessThanOrEqual
#define FATP_CONTEXTUAL_ENFORCE_IS_LESS_THAN_OR_EQUAL(func_ptr, lhs, rhs, ...)                     \
    FATP_CONTEXTUAL_ENFORCE_2(func_ptr, fat_p::IsLessThanOrEqualPredicate, lhs, rhs, __VA_ARGS__)
#define FATP_CONTEXTUAL_ABORT_IS_LESS_THAN_OR_EQUAL(func_ptr, lhs, rhs, ...)                       \
    FATP_CONTEXTUAL_ABORT_2(func_ptr, fat_p::IsLessThanOrEqualPredicate, lhs, rhs, __VA_ARGS__)
#define FATP_CONTEXTUAL_DEBUG_IS_LESS_THAN_OR_EQUAL(func_ptr, lhs, rhs, ...)                       \
    FATP_CONTEXTUAL_DEBUG_2(func_ptr, fat_p::IsLessThanOrEqualPredicate, lhs, rhs, __VA_ARGS__)

// IsGreaterThanOrEqual
#define FATP_CONTEXTUAL_ENFORCE_IS_GREATER_THAN_OR_EQUAL(func_ptr, lhs, rhs, ...)                  \
    FATP_CONTEXTUAL_ENFORCE_2(func_ptr, fat_p::IsGreaterThanOrEqualPredicate, lhs, rhs, __VA_ARGS__)
#define FATP_CONTEXTUAL_ABORT_IS_GREATER_THAN_OR_EQUAL(func_ptr, lhs, rhs, ...)                    \
    FATP_CONTEXTUAL_ABORT_2(func_ptr, fat_p::IsGreaterThanOrEqualPredicate, lhs, rhs, __VA_ARGS__)
#define FATP_CONTEXTUAL_DEBUG_IS_GREATER_THAN_OR_EQUAL(func_ptr, lhs, rhs, ...)                    \
    FATP_CONTEXTUAL_DEBUG_2(func_ptr, fat_p::IsGreaterThanOrEqualPredicate, lhs, rhs, __VA_ARGS__)

// InRange (between)
#define FATP_CONTEXTUAL_ENFORCE_BETWEEN(func_ptr, value, min, max, ...)                            \
    FATP_CONTEXTUAL_ENFORCE_3(func_ptr, fat_p::InRangePredicate, value, min, max, __VA_ARGS__)
#define FATP_CONTEXTUAL_ABORT_BETWEEN(func_ptr, value, min, max, ...)                              \
    FATP_CONTEXTUAL_ABORT_3(func_ptr, fat_p::InRangePredicate, value, min, max, __VA_ARGS__)
#define FATP_CONTEXTUAL_DEBUG_BETWEEN(func_ptr, value, min, max, ...)                              \
    FATP_CONTEXTUAL_DEBUG_3(func_ptr, fat_p::InRangePredicate, value, min, max, __VA_ARGS__)

// ApproxEqual
#define FATP_CONTEXTUAL_ENFORCE_APPROX_EQUAL(func_ptr, tolerance, lhs, rhs, ...)                   \
    FATP_CONTEXTUAL_ENFORCE_3(func_ptr, fat_p::ApproxEqualPredicate, tolerance, lhs, rhs, __VA_ARGS__)
#define FATP_CONTEXTUAL_ABORT_APPROX_EQUAL(func_ptr, tolerance, lhs, rhs, ...)                     \
    FATP_CONTEXTUAL_ABORT_3(func_ptr, fat_p::ApproxEqualPredicate, tolerance, lhs, rhs, __VA_ARGS__)
#define FATP_CONTEXTUAL_DEBUG_APPROX_EQUAL(func_ptr, tolerance, lhs, rhs, ...)                     \
    FATP_CONTEXTUAL_DEBUG_3(func_ptr, fat_p::ApproxEqualPredicate, tolerance, lhs, rhs, __VA_ARGS__)

// ============================================================================
// VI. Contextual Expected Integration Checks
// ============================================================================

#define FATP_CONTEXTUAL_ENFORCE_EXPECTED(func_ptr, condition, ...)                                 \
    ([&]() -> fat_p::Expected<void, std::string> {                                                 \
        if (!(condition))                                                                          \
        {                                                                                          \
            fat_p::MessageBuilder mb;                                                              \
            mb.format(__VA_ARGS__);                                                                \
            std::string msg = mb.get_message(FATP_LOCUS, #condition);                              \
            fat_p::detail::writeToStderr("Expected Failure: ", msg);                               \
            return fat_p::unexpected(msg);                                                         \
        }                                                                                          \
        return {};                                                                                 \
    })()

#define FATP_CONTEXTUAL_ENFORCE_EXPECTED_1(func_ptr, PredicateType, target, ...)                   \
    ([&]() -> fat_p::Expected<bool, std::string> {                                                 \
        auto result = PredicateType::check(target);                                                \
        if (!result)                                                                               \
        {                                                                                          \
            fat_p::MessageBuilder mb;                                                              \
            mb.format(__VA_ARGS__);                                                                \
            std::string msg = mb.get_message(FATP_LOCUS, #PredicateType "(" #target ")");          \
            fat_p::detail::writeToStderr("Expected Failure: ", msg);                               \
            return fat_p::unexpected(msg);                                                         \
        }                                                                                          \
        return result;                                                                             \
    })()

#define FATP_CONTEXTUAL_ENFORCE_EXPECTED_2(func_ptr, PredicateType, target1, target2, ...)         \
    ([&]() -> fat_p::Expected<bool, std::string> {                                                 \
        auto result = PredicateType::check(target1, target2);                                      \
        if (!result)                                                                               \
        {                                                                                          \
            fat_p::MessageBuilder mb;                                                              \
            mb.format(__VA_ARGS__);                                                                \
            std::string msg =                                                                      \
                mb.get_message(FATP_LOCUS, #PredicateType "(" #target1 ", " #target2 ")");         \
            fat_p::detail::writeToStderr("Expected Failure: ", msg);                               \
            return fat_p::unexpected(msg);                                                         \
        }                                                                                          \
        return result;                                                                             \
    })()

#define FATP_CONTEXTUAL_ENFORCE_EXPECTED_3(func_ptr, PredicateType, target1, target2, target3, ...)\
    ([&]() -> fat_p::Expected<bool, std::string> {                                                 \
        auto result = PredicateType::check(target1, target2, target3);                             \
        if (!result)                                                                               \
        {                                                                                          \
            fat_p::MessageBuilder mb;                                                              \
            mb.format(__VA_ARGS__);                                                                \
            std::string msg = mb.get_message(                                                      \
                FATP_LOCUS, #PredicateType "(" #target1 ", " #target2 ", " #target3 ")");          \
            fat_p::detail::writeToStderr("Expected Failure: ", msg);                               \
            return fat_p::unexpected(msg);                                                         \
        }                                                                                          \
        return result;                                                                             \
    })()

#define FATP_CONTEXTUAL_ENFORCE_EXPECTED_4(                                                        \
    func_ptr, PredicateType, target1, target2, target3, target4, ...)                              \
    ([&]() -> fat_p::Expected<bool, std::string> {                                                 \
        auto result = PredicateType::check(target1, target2, target3, target4);                    \
        if (!result)                                                                               \
        {                                                                                          \
            fat_p::MessageBuilder mb;                                                              \
            mb.format(__VA_ARGS__);                                                                \
            std::string msg = mb.get_message(                                                      \
                FATP_LOCUS,                                                                        \
                #PredicateType "(" #target1 ", " #target2 ", " #target3 ", " #target4 ")");        \
            fat_p::detail::writeToStderr("Expected Failure: ", msg);                               \
            return fat_p::unexpected(msg);                                                         \
        }                                                                                          \
        return result;                                                                             \
    })()

#define FATP_CONTEXTUAL_ENFORCE_EXPECTED_5(                                                        \
    func_ptr, PredicateType, target1, target2, target3, target4, target5, ...)                     \
    ([&]() -> fat_p::Expected<bool, std::string> {                                                 \
        auto result = PredicateType::check(target1, target2, target3, target4, target5);           \
        if (!result)                                                                               \
        {                                                                                          \
            fat_p::MessageBuilder mb;                                                              \
            mb.format(__VA_ARGS__);                                                                \
            std::string msg = mb.get_message(                                                      \
                FATP_LOCUS,                                                                        \
                #PredicateType "(" #target1 ", " #target2 ", " #target3 ", " #target4              \
                              ", " #target5 ")");                                                  \
            fat_p::detail::writeToStderr("Expected Failure: ", msg);                               \
            return fat_p::unexpected(msg);                                                         \
        }                                                                                          \
        return result;                                                                             \
    })()

#define FATP_CONTEXTUAL_ENFORCE_EXPECTED_NOT_NULL(func_ptr, ptr, ...)                              \
    FATP_CONTEXTUAL_ENFORCE_EXPECTED_1(func_ptr, fat_p::NotNullPredicate, ptr, __VA_ARGS__)

} // namespace fat_p
