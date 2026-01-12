/**
 * @file enforce_raiser_selector.h
 * @brief Defines the policy-to-raiser mapping and meta-logic required for the
 * policy-based contract system, including contextual enforcement.
 *
 * 
 *
 * @layer Foundation
 *
 * @details This header is central to the system, providing three key roles:
 * 1. Mapping explicit Policy tags (e.g., AbortPolicy) to concrete Raisers.
 * 2. Mapping Predicates to their default exception Raisers (e.g.,
 *    InRangePredicate -> OutOfRangeRaiser).
 * 3. Defining the ContextualRaiserResolver metafunction to guarantee
 *    noexcept safety.
 */
#pragma once

/*
FATP_META:
  meta_version: 1
  component: enforce_raiser_selector
  file_role: public_header
  path: fat_p/enforce_raiser_selector.h
  namespace: fat_p
  layer: Foundation
  summary: "Public header for enforce_raiser_selector."
  api_stability: in_work
  related:
    docs_search: "enforce_raiser_selector"
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
#include "enforce_contextual_policies.h"
#include "enforce_predicates.h"
#include "enforce_raisers.h"

namespace fat_p {

// --- 1. Policy Tags ---

struct AlwaysEnforcePolicy
{
};
struct DebugOnlyPolicy
{
};
struct WarningPolicy
{
};
struct NoThrowPolicy
{
};
struct IgnorePolicy
{
};
struct AbortPolicy
{
};
struct ExpectedPolicy
{
};

// --- 2. Predicate-to-Raiser Mapping ---

template <typename Predicate>
struct PredicateToRaiser;

template <>
struct PredicateToRaiser<BooleanPredicate>
{
    using type = LogicRaiser;
};

template <>
struct PredicateToRaiser<NotNullPredicate>
{
    using type = LogicRaiser;
};

template <>
struct PredicateToRaiser<IsNullPredicate>
{
    using type = LogicRaiser;
};

template <>
struct PredicateToRaiser<NotEmptyPredicate>
{
    using type = LengthErrorRaiser;
};

template <>
struct PredicateToRaiser<IsPositivePredicate>
{
    using type = LogicRaiser;
};

template <>
struct PredicateToRaiser<IsNonNegativePredicate>
{
    using type = LogicRaiser;
};

template <>
struct PredicateToRaiser<IsIntegralPredicate>
{
    using type = LogicRaiser;
};

template <>
struct PredicateToRaiser<ContainerIsUniquePredicate>
{
    using type = LogicRaiser;
};

template <>
struct PredicateToRaiser<HasNoNullElementsPredicate>
{
    using type = LogicRaiser;
};

template <>
struct PredicateToRaiser<HasSizePredicate>
{
    using type = LengthErrorRaiser;
};

template <>
struct PredicateToRaiser<IsSortedPredicate>
{
    using type = LogicRaiser;
};

template <>
struct PredicateToRaiser<AllSatisfyPredicate>
{
    using type = LogicRaiser;
};

template <>
struct PredicateToRaiser<AnySatisfyPredicate>
{
    using type = LogicRaiser;
};

template <>
struct PredicateToRaiser<ContainerHasElementPredicate>
{
    using type = LogicRaiser;
};

template <>
struct PredicateToRaiser<InRangePredicate>
{
    using type = OutOfRangeRaiser;
};

template <>
struct PredicateToRaiser<InExclusiveRangePredicate>
{
    using type = OutOfRangeRaiser;
};

template <>
struct PredicateToRaiser<ValidIndexPredicate>
{
    using type = OutOfRangeRaiser;
};

template <>
struct PredicateToRaiser<IsPowerOfTwoPredicate>
{
    using type = LogicRaiser;
};

template <>
struct PredicateToRaiser<ApproxEqualPredicate>
{
    using type = DomainErrorRaiser;
};

template <>
struct PredicateToRaiser<IsValidIteratorPredicate>
{
    using type = OutOfRangeRaiser;
};

template <>
struct PredicateToRaiser<IsLessThanPredicate>
{
    using type = LogicRaiser;
};

template <>
struct PredicateToRaiser<IsGreaterThanPredicate>
{
    using type = LogicRaiser;
};

template <>
struct PredicateToRaiser<IsLessThanOrEqualPredicate>
{
    using type = LogicRaiser;
};

template <>
struct PredicateToRaiser<IsGreaterThanOrEqualPredicate>
{
    using type = LogicRaiser;
};

template <>
struct PredicateToRaiser<IsFinitePredicate>
{
    using type = DomainErrorRaiser;
};

template <>
struct PredicateToRaiser<IsNormalPredicate>
{
    using type = DomainErrorRaiser;
};

template <>
struct PredicateToRaiser<IsNotNaNPredicate>
{
    using type = DomainErrorRaiser;
};

template <>
struct PredicateToRaiser<IsNotInfPredicate>
{
    using type = DomainErrorRaiser;
};

// --- 3. RaiserSelector ---

template <typename Policy>
struct RaiserSelector;

template <>
struct RaiserSelector<DebugOnlyPolicy>
{
#ifdef NDEBUG
    using type = NoOpRaiser;
#else
    using type = LogicRaiser;
#endif
};

template <>
struct RaiserSelector<AlwaysEnforcePolicy>
{
    using type = LogicRaiser;
};

template <>
struct RaiserSelector<WarningPolicy>
{
    using type = WarningToCerrRaiser;
};

template <>
struct RaiserSelector<NoThrowPolicy>
{
    using type = NoThrowRaiser;
};

template <>
struct RaiserSelector<IgnorePolicy>
{
    using type = NoOpRaiser;
};

template <>
struct RaiserSelector<AbortPolicy>
{
    using type = AbortRaiser;
};

template <>
struct RaiserSelector<NoexceptFunctionPolicy>
{
    using type = NoThrowRaiser;
};

template <>
struct RaiserSelector<ThrowingFunctionPolicy>
{
    using type = LogicRaiser;
};

template <>
struct RaiserSelector<ExpectedPolicy>
{
    using type = ExpectedRaiser<std::string>;
};

} // namespace fat_p
