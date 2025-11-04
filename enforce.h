/**
 * @file enforce.h
 * @brief Provides macro implementations for contract enforcement using
 * predefined and generic predicates with various policies.
 */
#pragma once
#include <type_traits>
#include <utility>

#include "enforce_enforcers.h"
#include "enforce_predicates.h"
#include "enforce_raisers.h"
#include "enforce_raiser_selector.h"
 /**
  * @file enforce.h
  * @brief Provides macro implementations for contract enforcement using
  * predefined and generic predicates with various policies.
  */
namespace cpp_utilities {
    // --- Locus Macros ---
#ifndef CPP_UTILITIES_LOCUS
#define CPP_UTILITIES_LOCUS __FILE__ ":" CPP_UTILITIES_STRINGIFY(__LINE__)
#define CPP_UTILITIES_STRINGIFY(x) CPP_UTILITIES_TOSTRING(x)
#define CPP_UTILITIES_TOSTRING(x) #x
#endif
// --- Core Enforcement Function ---
    template <typename Policy>
    [[nodiscard]] auto enforce_policy_impl(
        bool passed,
        const char* expression_str,
        const char* locus)
    {
        using Raiser = typename RaiserSelector<Policy>::type;
        return MakeEnforcer<Raiser>(passed, expression_str, locus);
    }
    // --- General Condition Macros ---
#define enforce(condition, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::DebugOnlyPolicy>( \
        (condition), #condition, CPP_UTILITIES_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define always_enforce(condition, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::AlwaysEnforcePolicy>( \
        (condition), #condition, CPP_UTILITIES_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define enforce_warn(condition, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::WarningPolicy>( \
        (condition), #condition, CPP_UTILITIES_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define noexcept_enforce(condition, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::NoThrowPolicy>( \
        (condition), #condition, CPP_UTILITIES_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define abort_enforce(condition, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::AbortPolicy>( \
        (condition), #condition, CPP_UTILITIES_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
// Returns Expected<void, std::string> on failure
#define enforce_expected(condition, ...) ([&]() -> Expected<void, std::string> { \
    if (!(condition)) { \
        cpp_utilities::MessageBuilder mb; \
        mb.format(__VA_ARGS__); \
        std::string msg = mb.get_message(CPP_UTILITIES_LOCUS, #condition); \
        diagnostic::conditionalPrintError([&]() { return "Expected Failure: " + msg; }); \
        return cpp_utilities::make_unexpected(msg); \
    } \
    return {}; \
})()
// Similar for always_enforce_expected
#define always_enforce_expected(condition, ...) ([&]() -> Expected<void, std::string> { \
    if (!(condition)) { \
        cpp_utilities::MessageBuilder mb; \
        mb.format(__VA_ARGS__); \
        std::string msg = mb.get_message(CPP_UTILITIES_LOCUS, #condition); \
        diagnostic::conditionalPrintError([&]() { return "Expected Failure: " + msg; }); \
        return cpp_utilities::make_unexpected(msg); \
    } \
    return {}; \
})()
// Predicate version (returns Expected<bool, E> where bool is predicate result)
#define enforce_predicate_expected(PredicateType, target, ...) ([&]() -> Expected<bool, std::string> { \
    auto result = PredicateType::check(target); \
    if (!result) { \
        cpp_utilities::MessageBuilder mb; \
        mb.format(__VA_ARGS__); \
        std::string msg = mb.get_message(CPP_UTILITIES_LOCUS, #PredicateType "(" #target ")"); \
        diagnostic::conditionalPrintError([&]() { return "Expected Failure: " + msg; }); \
        return cpp_utilities::make_unexpected(msg); \
    } \
    return result; \
})()
// --- Generic Arity-Based Macros ---
#define always_enforce_1(PredicateType, target, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::AlwaysEnforcePolicy>( \
        cpp_utilities::PredicateType::check(target), \
        #PredicateType "(" #target ")", \
        CPP_UTILITIES_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
#define always_enforce_2(PredicateType, target1, target2, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::AlwaysEnforcePolicy>( \
        cpp_utilities::PredicateType::check(target1, target2), \
        #PredicateType "(" #target1 ", " #target2 ")", \
        CPP_UTILITIES_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
#define always_enforce_3(PredicateType, target1, target2, target3, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::AlwaysEnforcePolicy>( \
        cpp_utilities::PredicateType::check(target1, target2, target3), \
        #PredicateType "(" #target1 ", " #target2 ", " #target3 ")", \
        CPP_UTILITIES_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
#define debug_enforce_1(PredicateType, target, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::DebugOnlyPolicy>( \
        cpp_utilities::PredicateType::check(target), \
        #PredicateType "(" #target ")", \
        CPP_UTILITIES_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
#define debug_enforce_2(PredicateType, target1, target2, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::DebugOnlyPolicy>( \
        cpp_utilities::PredicateType::check(target1, target2), \
        #PredicateType "(" #target1 ", " #target2 ")", \
        CPP_UTILITIES_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
#define debug_enforce_3(PredicateType, target1, target2, target3, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::DebugOnlyPolicy>( \
        cpp_utilities::PredicateType::check(target1, target2, target3), \
        #PredicateType "(" #target1 ", " #target2 ", " #target3 ")", \
        CPP_UTILITIES_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
#define noexcept_enforce_1(PredicateType, target, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::NoThrowPolicy>( \
        cpp_utilities::PredicateType::check(target), \
        #PredicateType "(" #target ")", \
        CPP_UTILITIES_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
#define noexcept_enforce_2(PredicateType, target1, target2, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::NoThrowPolicy>( \
        cpp_utilities::PredicateType::check(target1, target2), \
        #PredicateType "(" #target1 ", " #target2 ")", \
        CPP_UTILITIES_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
#define noexcept_enforce_3(PredicateType, target1, target2, target3, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::NoThrowPolicy>( \
        cpp_utilities::PredicateType::check(target1, target2, target3), \
        #PredicateType "(" #target1 ", " #target2 ", " #target3 ")", \
        CPP_UTILITIES_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
#define abort_enforce_1(PredicateType, target, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::AbortPolicy>( \
        cpp_utilities::PredicateType::check(target), \
        #PredicateType "(" #target ")", \
        CPP_UTILITIES_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
#define abort_enforce_2(PredicateType, target1, target2, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::AbortPolicy>( \
        cpp_utilities::PredicateType::check(target1, target2), \
        #PredicateType "(" #target1 ", " #target2 ")", \
        CPP_UTILITIES_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
#define abort_enforce_3(PredicateType, target1, target2, target3, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::AbortPolicy>( \
        cpp_utilities::PredicateType::check(target1, target2, target3), \
        #PredicateType "(" #target1 ", " #target2 ", " #target3 ")", \
        CPP_UTILITIES_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
#define warn_enforce_1(PredicateType, target, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::WarningPolicy>( \
        cpp_utilities::PredicateType::check(target), \
        #PredicateType "(" #target ")", \
        CPP_UTILITIES_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
#define warn_enforce_2(PredicateType, target1, target2, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::WarningPolicy>( \
        cpp_utilities::PredicateType::check(target1, target2), \
        #PredicateType "(" #target1 ", " #target2 ")", \
        CPP_UTILITIES_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
#define warn_enforce_3(PredicateType, target1, target2, target3, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::WarningPolicy>( \
        cpp_utilities::PredicateType::check(target1, target2, target3), \
        #PredicateType "(" #target1 ", " #target2 ", " #target3 ")", \
        CPP_UTILITIES_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
// --- Specific Predicate-Based Macros ---
// AllSatisfyPredicate Group
#define always_enforce_all_satisfy(pred, container, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::AlwaysEnforcePolicy>( \
        cpp_utilities::AllSatisfyPredicate::check(pred, container), \
        "all_satisfy(" #pred ", " #container ")", \
        CPP_UTILITIES_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
#define debug_enforce_all_satisfy(pred, container, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::DebugOnlyPolicy>( \
        cpp_utilities::AllSatisfyPredicate::check(pred, container), \
        "all_satisfy(" #pred ", " #container ")", \
        CPP_UTILITIES_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
#define always_enforce_all_satisfy_static(PredType, container, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::AlwaysEnforcePolicy>( \
        cpp_utilities::AllSatisfyPredicate::check(PredType{}, container), \
        "all_satisfy_static<" #PredType ">(" #container ")", \
        CPP_UTILITIES_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
#define debug_enforce_all_satisfy_static(PredType, container, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::DebugOnlyPolicy>( \
        cpp_utilities::AllSatisfyPredicate::check<PredType>(container), \
        "all_satisfy_static<" #PredType ">(" #container ")", \
        CPP_UTILITIES_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
// AnySatisfyPredicate Group
#define always_enforce_any_satisfy(pred, container, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::AlwaysEnforcePolicy>( \
        cpp_utilities::AnySatisfyPredicate::check(pred, container), \
        "any_satisfy(" #pred ", " #container ")", \
        CPP_UTILITIES_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
#define debug_enforce_any_satisfy(pred, container, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::DebugOnlyPolicy>( \
        cpp_utilities::AnySatisfyPredicate::check(pred, container), \
        "any_satisfy(" #pred ", " #container ")", \
        CPP_UTILITIES_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
#define always_enforce_any_satisfy_static(PredType, container, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::AlwaysEnforcePolicy>( \
        cpp_utilities::AnySatisfyPredicate::check<PredType>(container), \
        "any_satisfy_static<" #PredType ">(" #container ")", \
        CPP_UTILITIES_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
#define debug_enforce_any_satisfy_static(PredType, container, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::DebugOnlyPolicy>( \
        cpp_utilities::AnySatisfyPredicate::check<PredType>(container), \
        "any_satisfy_static<" #PredType ">(" #container ")", \
        CPP_UTILITIES_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
// HasSizePredicate Group
#define always_enforce_has_size(expected_size, container, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::AlwaysEnforcePolicy>( \
        cpp_utilities::HasSizePredicate::check(expected_size, container), \
        "has_size(" #expected_size ", " #container ")", CPP_UTILITIES_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define debug_enforce_has_size(expected_size, container, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::DebugOnlyPolicy>( \
        cpp_utilities::HasSizePredicate::check(expected_size, container), \
        "has_size(" #expected_size ", " #container ")", CPP_UTILITIES_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
// ContainerIsUniquePredicate Group
#define always_enforce_is_unique(container, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::AlwaysEnforcePolicy>( \
        cpp_utilities::ContainerIsUniquePredicate::check(container), \
        "is_unique(" #container ")", CPP_UTILITIES_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define debug_enforce_is_unique(container, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::DebugOnlyPolicy>( \
        cpp_utilities::ContainerIsUniquePredicate::check(container), \
        "is_unique(" #container ")", CPP_UTILITIES_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
// ApproxEqualPredicate Group
#define always_enforce_approx_equal(epsilon, a, b, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::AlwaysEnforcePolicy>( \
        cpp_utilities::ApproxEqualPredicate::check(epsilon, a, b), \
        "approx_equal(" #epsilon ", " #a ", " #b ")", CPP_UTILITIES_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define debug_enforce_approx_equal(epsilon, a, b, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::DebugOnlyPolicy>( \
        cpp_utilities::ApproxEqualPredicate::check(epsilon, a, b), \
        "approx_equal(" #epsilon ", " #a ", " #b ")", CPP_UTILITIES_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define always_enforce_approx_equal_static(EpsilonType, a, b, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::AlwaysEnforcePolicy>( \
        cpp_utilities::ApproxEqualPredicate::check<EpsilonType>(a, b), \
        "approx_equal_static<" #EpsilonType ">(" #a ", " #b ")", \
        CPP_UTILITIES_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define debug_enforce_approx_equal_static(EpsilonType, a, b, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::DebugOnlyPolicy>( \
        cpp_utilities::ApproxEqualPredicate::check<EpsilonType>(a, b), \
        "approx_equal_static<" #EpsilonType ">(" #a ", " #b ")", \
        CPP_UTILITIES_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
// InRangePredicate Group
#define always_enforce_in_range(min, max, value, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::AlwaysEnforcePolicy>( \
        cpp_utilities::InRangePredicate::check(value, min, max), \
        "in_range(" #min ", " #max ", " #value ")", CPP_UTILITIES_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define debug_enforce_in_range(min, max, value, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::DebugOnlyPolicy>( \
        cpp_utilities::InRangePredicate::check(value, min, max), \
        "in_range(" #min ", " #max ", " #value ")", CPP_UTILITIES_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define always_enforce_in_range_static(MinType, MaxType, value, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::AlwaysEnforcePolicy>( \
        cpp_utilities::InRangePredicate::check<MinType, MaxType>(value), \
        "in_range_static<" #MinType ", " #MaxType ">(" #value ")", \
        CPP_UTILITIES_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define debug_enforce_in_range_static(MinType, MaxType, value, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::DebugOnlyPolicy>( \
        cpp_utilities::InRangePredicate::check<MinType, MaxType>(value), \
        "in_range_static<" #MinType ", " #MaxType ">(" #value ")", \
        CPP_UTILITIES_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
// IsIntegralPredicate Group
#define always_enforce_is_integral(value, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::AlwaysEnforcePolicy>( \
        cpp_utilities::IsIntegralPredicate::check(value), \
        "is_integral(" #value ")", CPP_UTILITIES_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define debug_enforce_is_integral(value, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::DebugOnlyPolicy>( \
        cpp_utilities::IsIntegralPredicate::check(value), \
        "is_integral(" #value ")", CPP_UTILITIES_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
// IsNonNegativePredicate Group
#define always_enforce_is_non_negative(value, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::AlwaysEnforcePolicy>( \
        cpp_utilities::IsNonNegativePredicate::check(value), \
        "is_non_negative(" #value ")", CPP_UTILITIES_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define debug_enforce_is_non_negative(value, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::DebugOnlyPolicy>( \
        cpp_utilities::IsNonNegativePredicate::check(value), \
        "is_non_negative(" #value ")", CPP_UTILITIES_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
// IsPositivePredicate Group
#define always_enforce_is_positive(value, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::AlwaysEnforcePolicy>( \
        cpp_utilities::IsPositivePredicate::check(value), \
        "is_positive(" #value ")", CPP_UTILITIES_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define debug_enforce_is_positive(value, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::DebugOnlyPolicy>( \
        cpp_utilities::IsPositivePredicate::check(value), \
        "is_positive(" #value ")", CPP_UTILITIES_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
// IsPowerOfTwoPredicate Group
#define always_enforce_is_power_of_two(value, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::AlwaysEnforcePolicy>( \
        cpp_utilities::IsPowerOfTwoPredicate::check(value), \
        "is_power_of_two(" #value ")", CPP_UTILITIES_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define debug_enforce_is_power_of_two(value, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::DebugOnlyPolicy>( \
        cpp_utilities::IsPowerOfTwoPredicate::check(value), \
        "is_power_of_two(" #value ")", CPP_UTILITIES_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define always_enforce_is_power_of_two_static(ValueType, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::AlwaysEnforcePolicy>( \
        cpp_utilities::IsPowerOfTwoPredicate::check<ValueType>(), \
        "is_power_of_two_static<" #ValueType ">()", CPP_UTILITIES_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define debug_enforce_is_power_of_two_static(ValueType, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::DebugOnlyPolicy>( \
        cpp_utilities::IsPowerOfTwoPredicate::check<ValueType>(), \
        "is_power_of_two_static<" #ValueType ">()", CPP_UTILITIES_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
// IsSortedPredicate Group
#define always_enforce_is_sorted(container, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::AlwaysEnforcePolicy>( \
        cpp_utilities::IsSortedPredicate::check(container), \
        "is_sorted(" #container ")", CPP_UTILITIES_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define debug_enforce_is_sorted(container, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::DebugOnlyPolicy>( \
        cpp_utilities::IsSortedPredicate::check(container), \
        "is_sorted(" #container ")", CPP_UTILITIES_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define always_enforce_is_sorted_with(comp, container, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::AlwaysEnforcePolicy>( \
        cpp_utilities::IsSortedPredicate::check(container, comp), \
        "is_sorted_with(" #comp ", " #container ")", CPP_UTILITIES_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define debug_enforce_is_sorted_with(comp, container, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::DebugOnlyPolicy>( \
        cpp_utilities::IsSortedPredicate::check(container, comp), \
        "is_sorted_with(" #comp ", " #container ")", CPP_UTILITIES_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
// IsValidIteratorPredicate Group
#define always_enforce_is_valid_iterator(it, end, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::AlwaysEnforcePolicy>( \
        cpp_utilities::IsValidIteratorPredicate::check(it, end), \
        "is_valid_iterator(" #it ", " #end ")", \
        CPP_UTILITIES_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
#define debug_enforce_is_valid_iterator(it, end, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::DebugOnlyPolicy>( \
        cpp_utilities::IsValidIteratorPredicate::check(it, end), \
        "is_valid_iterator(" #it ", " #end ")", \
        CPP_UTILITIES_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
#define always_enforce_is_valid_iterator_static(ItType, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::AlwaysEnforcePolicy>( \
        cpp_utilities::IsValidIteratorPredicate::check<ItType>(), \
        "is_valid_iterator_static<" #ItType ">()", \
        CPP_UTILITIES_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
#define debug_enforce_is_valid_iterator_static(ItType, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::DebugOnlyPolicy>( \
        cpp_utilities::IsValidIteratorPredicate::check<ItType>(), \
        "is_valid_iterator_static<" #ItType ">()", \
        CPP_UTILITIES_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
// NotEmptyPredicate Group
#define always_enforce_not_empty(value, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::AlwaysEnforcePolicy>( \
        cpp_utilities::NotEmptyPredicate::check(value), \
        "not_empty(" #value ")", CPP_UTILITIES_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define debug_enforce_not_empty(value, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::DebugOnlyPolicy>( \
        cpp_utilities::NotEmptyPredicate::check(value), \
        "not_empty(" #value ")", CPP_UTILITIES_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
// NotNullPredicate Group
#define always_enforce_not_null(ptr, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::AlwaysEnforcePolicy>( \
        cpp_utilities::NotNullPredicate::check(ptr), \
        "not_null(" #ptr ")", CPP_UTILITIES_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define debug_enforce_not_null(ptr, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::DebugOnlyPolicy>( \
        cpp_utilities::NotNullPredicate::check(ptr), \
        "not_null(" #ptr ")", CPP_UTILITIES_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define warn_enforce_not_null(ptr, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::WarningPolicy>( \
        cpp_utilities::NotNullPredicate::check(ptr), \
        "not_null(" #ptr ")", CPP_UTILITIES_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define noexcept_enforce_not_null(ptr, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::NoThrowPolicy>( \
        cpp_utilities::NotNullPredicate::check(ptr), \
        "not_null(" #ptr ")", CPP_UTILITIES_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define abort_enforce_not_null(ptr, ...) do { \
    auto enforcer = cpp_utilities::enforce_policy_impl< \
        cpp_utilities::AbortPolicy>( \
        cpp_utilities::NotNullPredicate::check(ptr), \
        "not_null(" #ptr ")", CPP_UTILITIES_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
} // namespace cpp_utilities