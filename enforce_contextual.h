/**
 * @file enforce_contextual.h
 * @brief Defines macros and template functions for contextual contract enforcement that automatically
 * adapt failure behavior based on the function's 'noexcept' specification.
 *
 * @details This system uses the FATP_CONTEXTUAL_RESOLVER metafunction
 * to select the appropriate raiser: NoThrowRaiser if the function is noexcept,
 * or the default throwing raiser (mapped from PredicateType or explicitly
 * specified) unless explicitly overridden. The convenience predicate macros (formerly Section III)
 * have been moved to the end as type-safe inline template functions.
 */
#pragma once
#include <type_traits>
#include <utility> // For std::forward

#include "enforce_contextual_policies.h" // Provides is_noexcept_function_ptr
#include "enforce_enforcers.h" // Provides enforce_policy_impl
#include "enforce_predicates.h" // Provides NotNullPredicate, IsPositivePredicate, etc.
#include "Expected.h" // For Expected integration
#include "ScopeGuard.h" // For RAII in contextual if needed

namespace fat_p {

#ifndef FATP_LOCUS
#define FATP_LOCUS __FILE__ ":" FATP_STRINGIFY(__LINE__)
#define FATP_STRINGIFY(x) FATP_TOSTRING(x)
#define FATP_TOSTRING(x) #x
#endif

    // --- Internal Contextual Resolver Factory ---
#define FATP_CONTEXTUAL_RESOLVER(FunctionPtr, ThrowingRaiserType) \
    typename fat_p::ContextualRaiserResolver< \
        std::conditional_t< \
            fat_p::is_noexcept_function_ptr< \
                decltype((FunctionPtr)) \
            >::value, \
            fat_p::NoexceptFunctionPolicy, \
            fat_p::ThrowingFunctionPolicy \
        >, \
        ThrowingRaiserType \
    >::type
// --- Internal Predicate Enforcement Helper (N-argument) ---
#define FATP_CONTEXTUAL_PREDICATE_N(FunctionPtr, PredicateType, N, Targets, ...) \
        do { \
            using DefaultThrowingRaiser = \
                typename fat_p::PredicateToRaiser<PredicateType>::type; \
            \
            using FinalRaiser = \
                FATP_CONTEXTUAL_RESOLVER(FunctionPtr, DefaultThrowingRaiser); \
            \
            auto enforcer = fat_p::enforce_policy_impl<FinalRaiser>( \
                PredicateType::check(Targets), \
                #PredicateType "(" #Targets ")", \
                FATP_LOCUS \
            ); \
            enforcer(__VA_ARGS__); \
        } while(0)
// --- Internal Helper for Explicit Policy (Abort/Debug) with Predicates ---
#define FATP_CONTEXTUAL_ABORT_N_IMPL(FunctionPtr, PredicateType, Targets, ...) \
        do { \
            using FinalRaiser = FATP_CONTEXTUAL_RESOLVER(FunctionPtr, fat_p::AbortRaiser); \
            \
            auto enforcer = fat_p::enforce_policy_impl<FinalRaiser>( \
                PredicateType::check(Targets), \
                #PredicateType "(" #Targets ")", \
                FATP_LOCUS \
            ); \
            enforcer(__VA_ARGS__); \
        } while(0)
#define FATP_CONTEXTUAL_DEBUG_N_IMPL(FunctionPtr, PredicateType, Targets, ...) do { \
        if constexpr (!std::is_same_v<fat_p::RaiserSelector<fat_p::DebugOnlyPolicy>::type, fat_p::NoOpRaiser>) \
        { \
            using ThrowingRaiser = fat_p::RaiserSelector<fat_p::DebugOnlyPolicy>::type; \
            using FinalRaiser = FATP_CONTEXTUAL_RESOLVER(FunctionPtr, ThrowingRaiser); \
            \
            auto enforcer = fat_p::enforce_policy_impl<FinalRaiser>( \
                PredicateType::check(Targets), \
                #PredicateType "(" #Targets ")", \
                FATP_LOCUS \
            ); \
            enforcer(__VA_ARGS__); \
        } \
    } while(0)
// --- I. Contextual Simple Condition Checks ---
#define contextual_enforce(func_ptr, condition, ...) do { \
    using FinalRaiser = \
        FATP_CONTEXTUAL_RESOLVER(func_ptr, fat_p::LogicRaiser); \
    auto enforcer = fat_p::enforce_policy_impl<FinalRaiser>( \
        (condition), #condition, FATP_LOCUS); \
    enforcer(__VA_ARGS__); \
} while(0)
#define contextual_enforce_invalid_arg(func_ptr, condition, ...) do { \
    using FinalRaiser = \
        FATP_CONTEXTUAL_RESOLVER(func_ptr, fat_p::InvalidArgumentRaiser); \
    auto enforcer = fat_p::enforce_policy_impl<FinalRaiser>( \
        (condition), #condition, FATP_LOCUS); \
    enforcer(__VA_ARGS__); \
} while(0)
// --- II. Contextual Generic Predicate Checks ---
#define contextual_enforce_1(func_ptr, PredicateType, target, ...) \
    FATP_CONTEXTUAL_PREDICATE_N( \
        func_ptr, PredicateType, 1, (target), __VA_ARGS__ \
    )
#define contextual_enforce_2(func_ptr, PredicateType, target1, target2, ...) \
    FATP_CONTEXTUAL_PREDICATE_N( \
        func_ptr, PredicateType, 2, (target1, target2), __VA_ARGS__ \
    )
#define contextual_enforce_3(func_ptr, PredicateType, target1, target2, target3, ...) \
    FATP_CONTEXTUAL_PREDICATE_N( \
        func_ptr, PredicateType, 3, (target1, target2, target3), __VA_ARGS__ \
    )
#define contextual_enforce_4(func_ptr, PredicateType, target1, target2, target3, target4, ...) \
    FATP_CONTEXTUAL_PREDICATE_N( \
        func_ptr, PredicateType, 4, (target1, target2, target3, target4), __VA_ARGS__ \
    )
#define contextual_enforce_5(func_ptr, PredicateType, target1, target2, target3, target4, target5, ...) \
    FATP_CONTEXTUAL_PREDICATE_N( \
        func_ptr, PredicateType, 5, (target1, target2, target3, target4, target5), __VA_ARGS__ \
    )
// --- III. Contextual Abort Policy Checks ---
#define contextual_abort(func_ptr, condition, ...) do { \
    using FinalRaiser = FATP_CONTEXTUAL_RESOLVER(func_ptr, fat_p::AbortRaiser); \
    auto enforcer = fat_p::enforce_policy_impl<FinalRaiser>( \
        (condition), #condition, FATP_LOCUS); \
    enforcer(__VA_ARGS__); \
} while(0)
#define contextual_abort_1(func_ptr, PredicateType, target, ...) \
    FATP_CONTEXTUAL_ABORT_N_IMPL(func_ptr, PredicateType, (target), __VA_ARGS__)
#define contextual_abort_2(func_ptr, PredicateType, target1, target2, ...) \
    FATP_CONTEXTUAL_ABORT_N_IMPL(func_ptr, PredicateType, (target1, target2), __VA_ARGS__)
#define contextual_abort_3(func_ptr, PredicateType, target1, target2, target3, ...) \
    FATP_CONTEXTUAL_ABORT_N_IMPL(func_ptr, PredicateType, (target1, target2, target3), __VA_ARGS__)
#define contextual_abort_4(func_ptr, PredicateType, target1, target2, target3, target4, ...) \
    FATP_CONTEXTUAL_ABORT_N_IMPL(func_ptr, PredicateType, (target1, target2, target3, target4), __VA_ARGS__)
#define contextual_abort_5(func_ptr, PredicateType, target1, target2, target3, target4, target5, ...) \
    FATP_CONTEXTUAL_ABORT_N_IMPL(func_ptr, PredicateType, (target1, target2, target3, target4, target5), __VA_ARGS__)
// --- IV. Contextual Debug Policy Checks ---
#define contextual_debug(func_ptr, condition, ...) do { \
    if constexpr (!std::is_same_v<fat_p::RaiserSelector<fat_p::DebugOnlyPolicy>::type, fat_p::NoOpRaiser>) { \
        using ThrowingRaiser = fat_p::RaiserSelector<fat_p::DebugOnlyPolicy>::type; \
        using FinalRaiser = FATP_CONTEXTUAL_RESOLVER(func_ptr, ThrowingRaiser); \
        auto enforcer = fat_p::enforce_policy_impl<FinalRaiser>( \
            (condition), #condition, FATP_LOCUS); \
        enforcer(__VA_ARGS__); \
    } \
} while(0)
#define contextual_debug_1(func_ptr, PredicateType, target, ...) \
    FATP_CONTEXTUAL_DEBUG_N_IMPL(func_ptr, PredicateType, (target), __VA_ARGS__)
#define contextual_debug_2(func_ptr, PredicateType, target1, target2, ...) \
    FATP_CONTEXTUAL_DEBUG_N_IMPL(func_ptr, PredicateType, (target1, target2), __VA_ARGS__)
#define contextual_debug_3(func_ptr, PredicateType, target1, target2, target3, ...) \
    FATP_CONTEXTUAL_DEBUG_N_IMPL(func_ptr, PredicateType, (target1, target2, target3), __VA_ARGS__)
#define contextual_debug_4(func_ptr, PredicateType, target1, target2, target3, target4, ...) \
    FATP_CONTEXTUAL_DEBUG_N_IMPL(func_ptr, PredicateType, (target1, target2, target3, target4), __VA_ARGS__)
#define contextual_debug_5(func_ptr, PredicateType, target1, target2, target3, target4, target5, ...) \
    FATP_CONTEXTUAL_DEBUG_N_IMPL(func_ptr, PredicateType, (target1, target2, target3, target4, target5), __VA_ARGS__)
// --- V. Contextual Convenience Predicate Checks ---
#define contextual_enforce_not_null(func_ptr, ptr, ...) \
    contextual_enforce_1(func_ptr, NotNullPredicate, ptr, __VA_ARGS__)
#define contextual_abort_not_null(func_ptr, ptr, ...) \
    contextual_abort_1(func_ptr, NotNullPredicate, ptr, __VA_ARGS__)
#define contextual_debug_not_null(func_ptr, ptr, ...) \
    contextual_debug_1(func_ptr, NotNullPredicate, ptr, __VA_ARGS__)
#define contextual_enforce_is_positive(func_ptr, value, ...) \
    contextual_enforce_1(func_ptr, IsPositivePredicate, value, __VA_ARGS__)
#define contextual_abort_is_positive(func_ptr, value, ...) \
    contextual_abort_1(func_ptr, IsPositivePredicate, value, __VA_ARGS__)
#define contextual_debug_is_positive(func_ptr, value, ...) \
    contextual_debug_1(func_ptr, IsPositivePredicate, value, __VA_ARGS__)
#define contextual_enforce_is_non_negative(func_ptr, value, ...) \
    contextual_enforce_1(func_ptr, IsNonNegativePredicate, value, __VA_ARGS__)
#define contextual_abort_is_non_negative(func_ptr, value, ...) \
    contextual_abort_1(func_ptr, IsNonNegativePredicate, value, __VA_ARGS__)
#define contextual_debug_is_non_negative(func_ptr, value, ...) \
    contextual_debug_1(func_ptr, IsNonNegativePredicate, value, __VA_ARGS__)
#define contextual_enforce_is_power_of_two(func_ptr, value, ...) \
    contextual_enforce_1(func_ptr, IsPowerOfTwoPredicate, value, __VA_ARGS__)
#define contextual_abort_is_power_of_two(func_ptr, value, ...) \
    contextual_abort_1(func_ptr, IsPowerOfTwoPredicate, value, __VA_ARGS__)
#define contextual_debug_is_power_of_two(func_ptr, value, ...) \
    contextual_debug_1(func_ptr, IsPowerOfTwoPredicate, value, __VA_ARGS__)
#define contextual_enforce_is_unique(func_ptr, container, ...) \
    contextual_enforce_1(func_ptr, ContainerIsUniquePredicate, container, __VA_ARGS__)
#define contextual_abort_is_unique(func_ptr, container, ...) \
    contextual_abort_1(func_ptr, ContainerIsUniquePredicate, container, __VA_ARGS__)
#define contextual_debug_is_unique(func_ptr, container, ...) \
    contextual_debug_1(func_ptr, ContainerIsUniquePredicate, container, __VA_ARGS__)
#define contextual_enforce_all_satisfy(func_ptr, pred, range, ...) \
    contextual_enforce_2(func_ptr, AllSatisfyPredicate<decltype(pred)>, pred, range, __VA_ARGS__)
#define contextual_abort_all_satisfy(func_ptr, pred, range, ...) \
    contextual_abort_2(func_ptr, AllSatisfyPredicate<decltype(pred)>, pred, range, __VA_ARGS__)
#define contextual_debug_all_satisfy(func_ptr, pred, range, ...) \
    contextual_debug_2(func_ptr, AllSatisfyPredicate<decltype(pred)>, pred, range, __VA_ARGS__)
#define contextual_enforce_has_size(func_ptr, expected_size, container, ...) \
    contextual_enforce_2(func_ptr, HasSizePredicate, expected_size, container, __VA_ARGS__)
#define contextual_abort_has_size(func_ptr, expected_size, container, ...) \
    contextual_abort_2(func_ptr, HasSizePredicate, expected_size, container, __VA_ARGS__)
#define contextual_debug_has_size(func_ptr, expected_size, container, ...) \
    contextual_debug_2(func_ptr, HasSizePredicate, expected_size, container, __VA_ARGS__)
#define contextual_enforce_is_less_than(func_ptr, lhs, rhs, ...) \
    contextual_enforce_2(func_ptr, IsLessThanPredicate, lhs, rhs, __VA_ARGS__)
#define contextual_abort_is_less_than(func_ptr, lhs, rhs, ...) \
    contextual_abort_2(func_ptr, IsLessThanPredicate, lhs, rhs, __VA_ARGS__)
#define contextual_debug_is_less_than(func_ptr, lhs, rhs, ...) \
    contextual_debug_2(func_ptr, IsLessThanPredicate, lhs, rhs, __VA_ARGS__)
#define contextual_enforce_is_greater_than(func_ptr, lhs, rhs, ...) \
    contextual_enforce_2(func_ptr, IsGreaterThanPredicate, lhs, rhs, __VA_ARGS__)
#define contextual_abort_is_greater_than(func_ptr, lhs, rhs, ...) \
    contextual_abort_2(func_ptr, IsGreaterThanPredicate, lhs, rhs, __VA_ARGS__)
#define contextual_debug_is_greater_than(func_ptr, lhs, rhs, ...) \
    contextual_debug_2(func_ptr, IsGreaterThanPredicate, lhs, rhs, __VA_ARGS__)
#define contextual_enforce_is_less_than_or_equal(func_ptr, lhs, rhs, ...) \
    contextual_enforce_2(func_ptr, IsLessThanOrEqualPredicate, lhs, rhs, __VA_ARGS__)
#define contextual_abort_is_less_than_or_equal(func_ptr, lhs, rhs, ...) \
    contextual_abort_2(func_ptr, IsLessThanOrEqualPredicate, lhs, rhs, __VA_ARGS__)
#define contextual_debug_is_less_than_or_equal(func_ptr, lhs, rhs, ...) \
    contextual_debug_2(func_ptr, IsLessThanOrEqualPredicate, lhs, rhs, __VA_ARGS__)
#define contextual_enforce_is_greater_than_or_equal(func_ptr, lhs, rhs, ...) \
    contextual_enforce_2(func_ptr, IsGreaterThanOrEqualPredicate, lhs, rhs, __VA_ARGS__)
#define contextual_abort_is_greater_than_or_equal(func_ptr, lhs, rhs, ...) \
    contextual_abort_2(func_ptr, IsGreaterThanOrEqualPredicate, lhs, rhs, __VA_ARGS__)
#define contextual_debug_is_greater_than_or_equal(func_ptr, lhs, rhs, ...) \
    contextual_debug_2(func_ptr, IsGreaterThanOrEqualPredicate, lhs, rhs, __VA_ARGS__)
#define contextual_enforce_between(func_ptr, value, min, max, ...) \
    contextual_enforce_3(func_ptr, InRangePredicate, value, min, max, __VA_ARGS__)
#define contextual_abort_between(func_ptr, value, min, max, ...) \
    contextual_abort_3(func_ptr, InRangePredicate, value, min, max, __VA_ARGS__)
#define contextual_debug_between(func_ptr, value, min, max, ...) \
    contextual_debug_3(func_ptr, InRangePredicate, value, min, max, __VA_ARGS__)
#define contextual_enforce_approx_equal(func_ptr, tolerance, lhs, rhs, ...) \
    contextual_enforce_3(func_ptr, ApproxEqualPredicate, tolerance, lhs, rhs, __VA_ARGS__)
#define contextual_abort_approx_equal(func_ptr, tolerance, lhs, rhs, ...) \
    contextual_abort_3(func_ptr, ApproxEqualPredicate, tolerance, lhs, rhs, __VA_ARGS__)
#define contextual_debug_approx_equal(func_ptr, tolerance, lhs, rhs, ...) \
    contextual_debug_3(func_ptr, ApproxEqualPredicate, tolerance, lhs, rhs, __VA_ARGS__)
// --- VI. Contextual Expected Integration Checks ---
#define contextual_enforce_expected(func_ptr, condition, ...) ([&]() -> Expected<void, std::string> { \
    if (!(condition)) { \
        fat_p::MessageBuilder mb; \
        mb.format(__VA_ARGS__); \
        std::string msg = mb.get_message(FATP_LOCUS, #condition); \
        fat_p::detail::writeToStderr("Expected Failure: ", msg); \
        return fat_p::make_unexpected(msg); \
    } \
    return {}; \
})()
#define contextual_enforce_expected_1(func_ptr, PredicateType, target, ...) ([&]() -> Expected<bool, std::string> { \
    auto result = PredicateType::check(target); \
    if (!result) { \
        fat_p::MessageBuilder mb; \
        mb.format(__VA_ARGS__); \
        std::string msg = mb.get_message(FATP_LOCUS, #PredicateType "(" #target ")"); \
        fat_p::detail::writeToStderr("Expected Failure: ", msg); \
        return fat_p::make_unexpected(msg); \
    } \
    return result; \
})()
#define contextual_enforce_expected_2(func_ptr, PredicateType, target1, target2, ...) ([&]() -> Expected<bool, std::string> { \
    auto result = PredicateType::check(target1, target2); \
    if (!result) { \
        fat_p::MessageBuilder mb; \
        mb.format(__VA_ARGS__); \
        std::string msg = mb.get_message(FATP_LOCUS, #PredicateType "(" #target1 ", " #target2 ")"); \
        fat_p::detail::writeToStderr("Expected Failure: ", msg); \
        return fat_p::make_unexpected(msg); \
    } \
    return result; \
})()
#define contextual_enforce_expected_3(func_ptr, PredicateType, target1, target2, target3, ...) ([&]() -> Expected<bool, std::string> { \
    auto result = PredicateType::check(target1, target2, target3); \
    if (!result) { \
        fat_p::MessageBuilder mb; \
        mb.format(__VA_ARGS__); \
        std::string msg = mb.get_message(FATP_LOCUS, #PredicateType "(" #target1 ", " #target2 ", " #target3 ")"); \
        fat_p::detail::writeToStderr("Expected Failure: ", msg); \
        return fat_p::make_unexpected(msg); \
    } \
    return result; \
})()
#define contextual_enforce_expected_4(func_ptr, PredicateType, target1, target2, target3, target4, ...) ([&]() -> Expected<bool, std::string> { \
    auto result = PredicateType::check(target1, target2, target3, target4); \
    if (!result) { \
        fat_p::MessageBuilder mb; \
        mb.format(__VA_ARGS__); \
        std::string msg = mb.get_message(FATP_LOCUS, #PredicateType "(" #target1 ", " #target2 ", " #target3 ", " #target4 ")"); \
        fat_p::detail::writeToStderr("Expected Failure: ", msg); \
        return fat_p::make_unexpected(msg); \
    } \
    return result; \
})()
#define contextual_enforce_expected_5(func_ptr, PredicateType, target1, target2, target3, target4, target5, ...) ([&]() -> Expected<bool, std::string> { \
    auto result = PredicateType::check(target1, target2, target3, target4, target5); \
    if (!result) { \
        fat_p::MessageBuilder mb; \
        mb.format(__VA_ARGS__); \
        std::string msg = mb.get_message(FATP_LOCUS, #PredicateType "(" #target1 ", " #target2 ", " #target3 ", " #target4 ", " #target5 ")"); \
        fat_p::detail::writeToStderr("Expected Failure: ", msg); \
        return fat_p::make_unexpected(msg); \
    } \
    return result; \
})()
// Convenience for not_null with Expected
#define contextual_enforce_expected_not_null(func_ptr, ptr, ...) \
    contextual_enforce_expected_1(func_ptr, NotNullPredicate, ptr, __VA_ARGS__)
// Add similar for other predicates as needed
} // namespace fat_p