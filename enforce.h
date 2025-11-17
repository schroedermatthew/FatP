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
namespace fat_p {
    // --- Locus Macros ---
#ifndef FATP_LOCUS
#define FATP_LOCUS __FILE__ ":" FATP_STRINGIFY(__LINE__)
#define FATP_STRINGIFY(x) FATP_TOSTRING(x)
#define FATP_TOSTRING(x) #x
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
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::DebugOnlyPolicy>( \
        (condition), #condition, FATP_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define always_enforce(condition, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::AlwaysEnforcePolicy>( \
        (condition), #condition, FATP_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define enforce_warn(condition, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::WarningPolicy>( \
        (condition), #condition, FATP_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define noexcept_enforce(condition, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::NoThrowPolicy>( \
        (condition), #condition, FATP_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define abort_enforce(condition, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::AbortPolicy>( \
        (condition), #condition, FATP_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
// Returns Expected<void, std::string> on failure
#define enforce_expected(condition, ...) ([&]() -> Expected<void, std::string> { \
    if (!(condition)) { \
        fat_p::MessageBuilder mb; \
        mb.format(__VA_ARGS__); \
        std::string msg = mb.get_message(FATP_LOCUS, #condition); \
        diagnostic::conditionalPrintError([&]() { return "Expected Failure: " + msg; }); \
        return fat_p::make_unexpected(msg); \
    } \
    return {}; \
})()
// Similar for always_enforce_expected
#define always_enforce_expected(condition, ...) ([&]() -> Expected<void, std::string> { \
    if (!(condition)) { \
        fat_p::MessageBuilder mb; \
        mb.format(__VA_ARGS__); \
        std::string msg = mb.get_message(FATP_LOCUS, #condition); \
        diagnostic::conditionalPrintError([&]() { return "Expected Failure: " + msg; }); \
        return fat_p::make_unexpected(msg); \
    } \
    return {}; \
})()
// Predicate version (returns Expected<bool, E> where bool is predicate result)
#define enforce_predicate_expected(PredicateType, target, ...) ([&]() -> Expected<bool, std::string> { \
    auto result = PredicateType::check(target); \
    if (!result) { \
        fat_p::MessageBuilder mb; \
        mb.format(__VA_ARGS__); \
        std::string msg = mb.get_message(FATP_LOCUS, #PredicateType "(" #target ")"); \
        diagnostic::conditionalPrintError([&]() { return "Expected Failure: " + msg; }); \
        return fat_p::make_unexpected(msg); \
    } \
    return result; \
})()
// --- Generic Arity-Based Macros ---
#define always_enforce_1(PredicateType, target, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::AlwaysEnforcePolicy>( \
        fat_p::PredicateType::check(target), \
        #PredicateType "(" #target ")", \
        FATP_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
#define always_enforce_2(PredicateType, target1, target2, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::AlwaysEnforcePolicy>( \
        fat_p::PredicateType::check(target1, target2), \
        #PredicateType "(" #target1 ", " #target2 ")", \
        FATP_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
#define always_enforce_3(PredicateType, target1, target2, target3, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::AlwaysEnforcePolicy>( \
        fat_p::PredicateType::check(target1, target2, target3), \
        #PredicateType "(" #target1 ", " #target2 ", " #target3 ")", \
        FATP_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
#define debug_enforce_1(PredicateType, target, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::DebugOnlyPolicy>( \
        fat_p::PredicateType::check(target), \
        #PredicateType "(" #target ")", \
        FATP_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
#define debug_enforce_2(PredicateType, target1, target2, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::DebugOnlyPolicy>( \
        fat_p::PredicateType::check(target1, target2), \
        #PredicateType "(" #target1 ", " #target2 ")", \
        FATP_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
#define debug_enforce_3(PredicateType, target1, target2, target3, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::DebugOnlyPolicy>( \
        fat_p::PredicateType::check(target1, target2, target3), \
        #PredicateType "(" #target1 ", " #target2 ", " #target3 ")", \
        FATP_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
#define noexcept_enforce_1(PredicateType, target, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::NoThrowPolicy>( \
        fat_p::PredicateType::check(target), \
        #PredicateType "(" #target ")", \
        FATP_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
#define noexcept_enforce_2(PredicateType, target1, target2, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::NoThrowPolicy>( \
        fat_p::PredicateType::check(target1, target2), \
        #PredicateType "(" #target1 ", " #target2 ")", \
        FATP_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
#define noexcept_enforce_3(PredicateType, target1, target2, target3, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::NoThrowPolicy>( \
        fat_p::PredicateType::check(target1, target2, target3), \
        #PredicateType "(" #target1 ", " #target2 ", " #target3 ")", \
        FATP_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
#define abort_enforce_1(PredicateType, target, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::AbortPolicy>( \
        fat_p::PredicateType::check(target), \
        #PredicateType "(" #target ")", \
        FATP_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
#define abort_enforce_2(PredicateType, target1, target2, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::AbortPolicy>( \
        fat_p::PredicateType::check(target1, target2), \
        #PredicateType "(" #target1 ", " #target2 ")", \
        FATP_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
#define abort_enforce_3(PredicateType, target1, target2, target3, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::AbortPolicy>( \
        fat_p::PredicateType::check(target1, target2, target3), \
        #PredicateType "(" #target1 ", " #target2 ", " #target3 ")", \
        FATP_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
#define warn_enforce_1(PredicateType, target, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::WarningPolicy>( \
        fat_p::PredicateType::check(target), \
        #PredicateType "(" #target ")", \
        FATP_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
#define warn_enforce_2(PredicateType, target1, target2, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::WarningPolicy>( \
        fat_p::PredicateType::check(target1, target2), \
        #PredicateType "(" #target1 ", " #target2 ")", \
        FATP_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
#define warn_enforce_3(PredicateType, target1, target2, target3, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::WarningPolicy>( \
        fat_p::PredicateType::check(target1, target2, target3), \
        #PredicateType "(" #target1 ", " #target2 ", " #target3 ")", \
        FATP_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
// --- Specific Predicate-Based Macros ---
// AllSatisfyPredicate Group
#define always_enforce_all_satisfy(pred, container, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::AlwaysEnforcePolicy>( \
        fat_p::AllSatisfyPredicate::check(pred, container), \
        "all_satisfy(" #pred ", " #container ")", \
        FATP_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
#define debug_enforce_all_satisfy(pred, container, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::DebugOnlyPolicy>( \
        fat_p::AllSatisfyPredicate::check(pred, container), \
        "all_satisfy(" #pred ", " #container ")", \
        FATP_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
#define always_enforce_all_satisfy_static(PredType, container, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::AlwaysEnforcePolicy>( \
        fat_p::AllSatisfyPredicate::check(PredType{}, container), \
        "all_satisfy_static<" #PredType ">(" #container ")", \
        FATP_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
#define debug_enforce_all_satisfy_static(PredType, container, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::DebugOnlyPolicy>( \
        fat_p::AllSatisfyPredicate::check<PredType>(container), \
        "all_satisfy_static<" #PredType ">(" #container ")", \
        FATP_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
// AnySatisfyPredicate Group
#define always_enforce_any_satisfy(pred, container, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::AlwaysEnforcePolicy>( \
        fat_p::AnySatisfyPredicate::check(pred, container), \
        "any_satisfy(" #pred ", " #container ")", \
        FATP_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
#define debug_enforce_any_satisfy(pred, container, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::DebugOnlyPolicy>( \
        fat_p::AnySatisfyPredicate::check(pred, container), \
        "any_satisfy(" #pred ", " #container ")", \
        FATP_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
#define always_enforce_any_satisfy_static(PredType, container, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::AlwaysEnforcePolicy>( \
        fat_p::AnySatisfyPredicate::check<PredType>(container), \
        "any_satisfy_static<" #PredType ">(" #container ")", \
        FATP_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
#define debug_enforce_any_satisfy_static(PredType, container, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::DebugOnlyPolicy>( \
        fat_p::AnySatisfyPredicate::check<PredType>(container), \
        "any_satisfy_static<" #PredType ">(" #container ")", \
        FATP_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
// HasSizePredicate Group
#define always_enforce_has_size(expected_size, container, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::AlwaysEnforcePolicy>( \
        fat_p::HasSizePredicate::check(expected_size, container), \
        "has_size(" #expected_size ", " #container ")", FATP_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define debug_enforce_has_size(expected_size, container, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::DebugOnlyPolicy>( \
        fat_p::HasSizePredicate::check(expected_size, container), \
        "has_size(" #expected_size ", " #container ")", FATP_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
// ContainerIsUniquePredicate Group
#define always_enforce_is_unique(container, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::AlwaysEnforcePolicy>( \
        fat_p::ContainerIsUniquePredicate::check(container), \
        "is_unique(" #container ")", FATP_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define debug_enforce_is_unique(container, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::DebugOnlyPolicy>( \
        fat_p::ContainerIsUniquePredicate::check(container), \
        "is_unique(" #container ")", FATP_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
// ApproxEqualPredicate Group
#define always_enforce_approx_equal(epsilon, a, b, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::AlwaysEnforcePolicy>( \
        fat_p::ApproxEqualPredicate::check(epsilon, a, b), \
        "approx_equal(" #epsilon ", " #a ", " #b ")", FATP_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define debug_enforce_approx_equal(epsilon, a, b, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::DebugOnlyPolicy>( \
        fat_p::ApproxEqualPredicate::check(epsilon, a, b), \
        "approx_equal(" #epsilon ", " #a ", " #b ")", FATP_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define always_enforce_approx_equal_static(EpsilonType, a, b, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::AlwaysEnforcePolicy>( \
        fat_p::ApproxEqualPredicate::check<EpsilonType>(a, b), \
        "approx_equal_static<" #EpsilonType ">(" #a ", " #b ")", \
        FATP_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define debug_enforce_approx_equal_static(EpsilonType, a, b, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::DebugOnlyPolicy>( \
        fat_p::ApproxEqualPredicate::check<EpsilonType>(a, b), \
        "approx_equal_static<" #EpsilonType ">(" #a ", " #b ")", \
        FATP_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
// InRangePredicate Group
#define always_enforce_in_range(min, max, value, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::AlwaysEnforcePolicy>( \
        fat_p::InRangePredicate::check(value, min, max), \
        "in_range(" #min ", " #max ", " #value ")", FATP_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define debug_enforce_in_range(min, max, value, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::DebugOnlyPolicy>( \
        fat_p::InRangePredicate::check(value, min, max), \
        "in_range(" #min ", " #max ", " #value ")", FATP_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define always_enforce_in_range_static(MinType, MaxType, value, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::AlwaysEnforcePolicy>( \
        fat_p::InRangePredicate::check<MinType, MaxType>(value), \
        "in_range_static<" #MinType ", " #MaxType ">(" #value ")", \
        FATP_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define debug_enforce_in_range_static(MinType, MaxType, value, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::DebugOnlyPolicy>( \
        fat_p::InRangePredicate::check<MinType, MaxType>(value), \
        "in_range_static<" #MinType ", " #MaxType ">(" #value ")", \
        FATP_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
// IsIntegralPredicate Group
#define always_enforce_is_integral(value, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::AlwaysEnforcePolicy>( \
        fat_p::IsIntegralPredicate::check(value), \
        "is_integral(" #value ")", FATP_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define debug_enforce_is_integral(value, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::DebugOnlyPolicy>( \
        fat_p::IsIntegralPredicate::check(value), \
        "is_integral(" #value ")", FATP_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
// IsNonNegativePredicate Group
#define always_enforce_is_non_negative(value, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::AlwaysEnforcePolicy>( \
        fat_p::IsNonNegativePredicate::check(value), \
        "is_non_negative(" #value ")", FATP_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define debug_enforce_is_non_negative(value, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::DebugOnlyPolicy>( \
        fat_p::IsNonNegativePredicate::check(value), \
        "is_non_negative(" #value ")", FATP_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
// IsPositivePredicate Group
#define always_enforce_is_positive(value, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::AlwaysEnforcePolicy>( \
        fat_p::IsPositivePredicate::check(value), \
        "is_positive(" #value ")", FATP_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define debug_enforce_is_positive(value, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::DebugOnlyPolicy>( \
        fat_p::IsPositivePredicate::check(value), \
        "is_positive(" #value ")", FATP_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
// IsPowerOfTwoPredicate Group
#define always_enforce_is_power_of_two(value, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::AlwaysEnforcePolicy>( \
        fat_p::IsPowerOfTwoPredicate::check(value), \
        "is_power_of_two(" #value ")", FATP_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define debug_enforce_is_power_of_two(value, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::DebugOnlyPolicy>( \
        fat_p::IsPowerOfTwoPredicate::check(value), \
        "is_power_of_two(" #value ")", FATP_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define always_enforce_is_power_of_two_static(ValueType, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::AlwaysEnforcePolicy>( \
        fat_p::IsPowerOfTwoPredicate::check<ValueType>(), \
        "is_power_of_two_static<" #ValueType ">()", FATP_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define debug_enforce_is_power_of_two_static(ValueType, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::DebugOnlyPolicy>( \
        fat_p::IsPowerOfTwoPredicate::check<ValueType>(), \
        "is_power_of_two_static<" #ValueType ">()", FATP_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
// IsSortedPredicate Group
#define always_enforce_is_sorted(container, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::AlwaysEnforcePolicy>( \
        fat_p::IsSortedPredicate::check(container), \
        "is_sorted(" #container ")", FATP_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define debug_enforce_is_sorted(container, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::DebugOnlyPolicy>( \
        fat_p::IsSortedPredicate::check(container), \
        "is_sorted(" #container ")", FATP_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define always_enforce_is_sorted_with(comp, container, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::AlwaysEnforcePolicy>( \
        fat_p::IsSortedPredicate::check(container, comp), \
        "is_sorted_with(" #comp ", " #container ")", FATP_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define debug_enforce_is_sorted_with(comp, container, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::DebugOnlyPolicy>( \
        fat_p::IsSortedPredicate::check(container, comp), \
        "is_sorted_with(" #comp ", " #container ")", FATP_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
// IsValidIteratorPredicate Group
#define always_enforce_is_valid_iterator(it, end, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::AlwaysEnforcePolicy>( \
        fat_p::IsValidIteratorPredicate::check(it, end), \
        "is_valid_iterator(" #it ", " #end ")", \
        FATP_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
#define debug_enforce_is_valid_iterator(it, end, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::DebugOnlyPolicy>( \
        fat_p::IsValidIteratorPredicate::check(it, end), \
        "is_valid_iterator(" #it ", " #end ")", \
        FATP_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
#define always_enforce_is_valid_iterator_static(ItType, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::AlwaysEnforcePolicy>( \
        fat_p::IsValidIteratorPredicate::check<ItType>(), \
        "is_valid_iterator_static<" #ItType ">()", \
        FATP_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
#define debug_enforce_is_valid_iterator_static(ItType, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::DebugOnlyPolicy>( \
        fat_p::IsValidIteratorPredicate::check<ItType>(), \
        "is_valid_iterator_static<" #ItType ">()", \
        FATP_LOCUS \
    ); \
    enforcer(__VA_ARGS__); \
} while (0)
// NotEmptyPredicate Group
#define always_enforce_not_empty(value, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::AlwaysEnforcePolicy>( \
        fat_p::NotEmptyPredicate::check(value), \
        "not_empty(" #value ")", FATP_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define debug_enforce_not_empty(value, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::DebugOnlyPolicy>( \
        fat_p::NotEmptyPredicate::check(value), \
        "not_empty(" #value ")", FATP_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
// NotNullPredicate Group
#define always_enforce_not_null(ptr, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::AlwaysEnforcePolicy>( \
        fat_p::NotNullPredicate::check(ptr), \
        "not_null(" #ptr ")", FATP_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define debug_enforce_not_null(ptr, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::DebugOnlyPolicy>( \
        fat_p::NotNullPredicate::check(ptr), \
        "not_null(" #ptr ")", FATP_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define warn_enforce_not_null(ptr, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::WarningPolicy>( \
        fat_p::NotNullPredicate::check(ptr), \
        "not_null(" #ptr ")", FATP_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define noexcept_enforce_not_null(ptr, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::NoThrowPolicy>( \
        fat_p::NotNullPredicate::check(ptr), \
        "not_null(" #ptr ")", FATP_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
#define abort_enforce_not_null(ptr, ...) do { \
    auto enforcer = fat_p::enforce_policy_impl< \
        fat_p::AbortPolicy>( \
        fat_p::NotNullPredicate::check(ptr), \
        "not_null(" #ptr ")", FATP_LOCUS); \
    enforcer(__VA_ARGS__); \
} while (0)
} // namespace fat_p