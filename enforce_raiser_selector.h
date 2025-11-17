/**
 * @file enforce_raiser_selector.h
 * @brief Defines the policy-to-raiser mapping and meta-logic required for the
 * policy-based contract system, including contextual enforcement.
 *
 * @details This header is central to the system, providing three key roles:
 * 1. Mapping explicit Policy tags (e.g., AbortPolicy) to concrete Raisers.
 * 2. Mapping Predicates to their default exception Raisers (e.g.,
 * InRangePredicate -> OutOfRangeRaiser).
 * 3. Defining the ContextualRaiserResolver metafunction to guarantee
 * noexcept safety.
 */
#pragma once

#include "enforce_raisers.h" // Provides all concrete Raiser implementations
#include "enforce_contextual_policies.h" // Provides policy tags and traits
#include "enforce_predicates.h" // For predicate forward declarations

namespace fat_p {

    // --- 1. Policy Tags ---

    struct AlwaysEnforcePolicy {};
    struct DebugOnlyPolicy {};
    struct WarningPolicy {};
    struct NoThrowPolicy {};
    struct IgnorePolicy {};
    struct AbortPolicy {};
    struct ExpectedPolicy {};

    // --- 2. Predicate-to-Raiser Mapping ---

    // Forward declarations for all predicates
    struct BooleanPredicate;
    struct NotNullPredicate;
    struct NotEmptyPredicate;
    struct IsPositivePredicate;
    struct IsNonNegativePredicate;
    struct IsIntegralPredicate;
    struct ContainerIsUniquePredicate;
    struct HasNoNullElementsPredicate;
    struct HasSizePredicate;
    struct IsSortedPredicate;
    struct IsSortedWithPredicate; 
    struct AllSatisfyPredicate;
    struct AnySatisfyPredicate;
    struct InRangePredicate;
    struct IsPowerOfTwoPredicate;
    struct ApproxEqualPredicate;
    struct IsValidIteratorPredicate;
    struct IsLessThanPredicate;
    struct IsGreaterThanPredicate;
    struct IsLessThanOrEqualPredicate;
    struct IsGreaterThanOrEqualPredicate;

    struct LogicRaiser;
    struct LengthErrorRaiser;
    struct OutOfRangeRaiser;
    struct DomainErrorRaiser;
    struct ThrowingRaiser;
    struct NoThrowRaiser;
    struct NoOpRaiser;
    struct WarningToCerrRaiser;
    struct AbortRaiser;
    template<typename E>
    struct ExpectedRaiser;

    template <typename Predicate>
    struct PredicateToRaiser;

    template <>
    struct PredicateToRaiser<BooleanPredicate> {
        using type = LogicRaiser;
    };

    template <>
    struct PredicateToRaiser<NotNullPredicate> {
        using type = LogicRaiser;
    };

    template <>
    struct PredicateToRaiser<NotEmptyPredicate> {
        using type = LengthErrorRaiser;
    };

    template <>
    struct PredicateToRaiser<IsPositivePredicate> {
        using type = LogicRaiser;
    };

    template <>
    struct PredicateToRaiser<IsNonNegativePredicate> {
        using type = LogicRaiser;
    };

    template <>
    struct PredicateToRaiser<IsIntegralPredicate> {
        using type = LogicRaiser;
    };

    template <>
    struct PredicateToRaiser<ContainerIsUniquePredicate> {
        using type = LogicRaiser;
    };

    template <>
    struct PredicateToRaiser<HasNoNullElementsPredicate> {
        using type = LogicRaiser;
    };

    template <>
    struct PredicateToRaiser<HasSizePredicate> {
        using type = LengthErrorRaiser;
    };

    template <>
    struct PredicateToRaiser<IsSortedPredicate> {
        using type = LogicRaiser;
    };

    template <>
    struct PredicateToRaiser<IsSortedWithPredicate> {
        using type = LogicRaiser;
    };

    template <>
    struct PredicateToRaiser<AllSatisfyPredicate> {
        using type = LogicRaiser;
    };

    template <>
    struct PredicateToRaiser<AnySatisfyPredicate> {
        using type = LogicRaiser;
    };

    template <>
    struct PredicateToRaiser<InRangePredicate> {
        using type = OutOfRangeRaiser;
    };

    template <>
    struct PredicateToRaiser<IsPowerOfTwoPredicate> {
        using type = LogicRaiser;
    };

    template <>
    struct PredicateToRaiser<ApproxEqualPredicate> {
        using type = DomainErrorRaiser;
    };

    template <>
    struct PredicateToRaiser<IsValidIteratorPredicate> {
        using type = OutOfRangeRaiser;
    };

    template <>
    struct PredicateToRaiser<IsLessThanPredicate> {
        using type = LogicRaiser;
    };

    template <>
    struct PredicateToRaiser<IsGreaterThanPredicate> {
        using type = LogicRaiser;
    };

    template <>
    struct PredicateToRaiser<IsLessThanOrEqualPredicate> {
        using type = LogicRaiser;
    };

    template <>
    struct PredicateToRaiser<IsGreaterThanOrEqualPredicate> {
        using type = LogicRaiser;
    };


    // --- 4. RaiserSelector ---

    template <typename Policy>
    struct RaiserSelector;

    template <>
    struct RaiserSelector<DebugOnlyPolicy> {
#ifdef NDEBUG
        using type = NoOpRaiser;
#else
        using type = LogicRaiser;
#endif
    };

    template <>
    struct RaiserSelector<AlwaysEnforcePolicy> {  
        using type = LogicRaiser;                 
    };                                            

    template <>
    struct RaiserSelector<WarningPolicy> {
        using type = WarningToCerrRaiser;
    };

    template <>
    struct RaiserSelector<NoThrowPolicy> {
        using type = NoThrowRaiser;
    };

    template <>
    struct RaiserSelector<IgnorePolicy> {
        using type = NoOpRaiser;
    };

    template <>
    struct RaiserSelector<AbortPolicy> {
        using type = AbortRaiser;
    };

    template <>
    struct RaiserSelector<NoexceptFunctionPolicy> {
        using type = NoThrowRaiser;
    };

    template <>
    struct RaiserSelector<ThrowingFunctionPolicy> {
        using type = LogicRaiser;
    };

    template <>
    struct RaiserSelector<ExpectedPolicy> {
        using type = ExpectedRaiser<std::string>;  // Default to string error
    };

} // namespace fat_p