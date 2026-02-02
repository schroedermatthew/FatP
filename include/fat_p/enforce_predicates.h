#pragma once

/*
FATP_META:
  meta_version: 1
  component: Enforce
  file_role: public_header
  path: include/fat_p/enforce_predicates.h
  namespace: fat_p
  layer: Foundation
  summary: "Public header for enforce_predicates."
  api_stability: in_work
  related:
    docs_search: "enforce_predicates"
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

/**
 * @file enforce_predicates.h
 * @brief Defines the Predicate policy structs used by contract macros to
 * perform specialized, type-safe checks on various C++ types (pointers,
 * containers, arithmetic values).
 *
 *
 *
 * @details Each predicate provides a static `check` method that returns a
 * boolean result. They use C++20 concepts and `if constexpr` to offer
 * zero-overhead checks whenever possible.
 */

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <ranges>
#include <stdexcept>
#include <type_traits>
#include <unordered_set>
#include <utility>

#include "Concepts.h"
#include "ConstexprUtilities.h"

namespace fat_p
{

// ===================================================================
// 1. Core Predicates
// ===================================================================

struct BooleanPredicate
{
    static constexpr bool check(bool condition) noexcept
    {
        return condition;
    }

    constexpr bool operator()(bool condition) const noexcept
    {
        return check(condition);
    }

    static bool check_runtime(bool condition) noexcept
    {
        return condition;
    }
};

struct NotNullPredicate
{
    template <typename T>
    static constexpr bool check(const T ptr) noexcept
    {
        return ptr != nullptr;
    }

    template <typename T>
    constexpr bool operator()(const T ptr) const noexcept
    {
        return check(ptr);
    }

    template <typename T>
    static bool check_runtime(const T ptr) noexcept
    {
        return ptr != nullptr;
    }
};

struct IsNullPredicate
{
    template <typename T>
    static constexpr bool check(const T ptr) noexcept
    {
        return ptr == nullptr;
    }

    template <typename T>
    constexpr bool operator()(const T ptr) const noexcept
    {
        return check(ptr);
    }

    template <typename T>
    static bool check_runtime(const T ptr) noexcept
    {
        return ptr == nullptr;
    }
};

struct NotEmptyPredicate
{
    template <typename T>
        requires concepts::has_empty<T>
    static constexpr bool check(const T& value) noexcept
    {
        return !value.empty();
    }

    template <typename T>
        requires concepts::has_empty<T>
    constexpr bool operator()(const T& value) const noexcept
    {
        return check(value);
    }

    template <typename T>
        requires concepts::has_empty<T>
    static bool check_runtime(const T& value) noexcept
    {
        return !value.empty();
    }
};

struct IsPositivePredicate
{
    template <typename T>
        requires std::is_arithmetic_v<T>
    static constexpr bool check(T value) noexcept
    {
        return value > T{0};
    }

    template <typename T>
        requires std::is_arithmetic_v<T>
    constexpr bool operator()(T value) const noexcept
    {
        return check(value);
    }

    template <typename T>
        requires std::is_arithmetic_v<T>
    static bool check_runtime(T value) noexcept
    {
        return value > T{0};
    }
};

struct IsNonNegativePredicate
{
    template <typename T>
        requires std::is_arithmetic_v<T>
    static constexpr bool check(T value) noexcept
    {
        return value >= T{0};
    }

    template <typename T>
        requires std::is_arithmetic_v<T>
    constexpr bool operator()(T value) const noexcept
    {
        return check(value);
    }

    template <typename T>
        requires std::is_arithmetic_v<T>
    static bool check_runtime(T value) noexcept
    {
        return value >= T{0};
    }
};

struct IsIntegralPredicate
{
    template <typename T>
    static constexpr bool check(const T&) noexcept
    {
        return std::is_integral_v<T>;
    }

    template <typename T>
    constexpr bool operator()(const T&) const noexcept
    {
        return check<T>(T{});
    }

    template <typename T>
    static bool check_runtime(const T&) noexcept
    {
        return std::is_integral_v<T>;
    }
};

// ===================================================================
// 2. Container Predicates
// ===================================================================

struct ContainerIsUniquePredicate
{
    template <typename Container>
        requires std::ranges::range<Container>
    static bool check(const Container& container) noexcept(false)
    {
        using value_type = std::ranges::range_value_t<Container>;
        if constexpr (concepts::hashable<value_type>)
        {
            std::unordered_set<value_type> unique_set;
            for (const auto& elem : container)
            {
                if (!unique_set.insert(elem).second)
                {
                    return false;
                }
            }
            return true;
        }
        else
        {
            auto begin = std::ranges::begin(container);
            auto end = std::ranges::end(container);
            for (auto it = begin; it != end; ++it)
            {
                for (auto jt = std::next(it); jt != end; ++jt)
                {
                    if (*it == *jt)
                    {
                        return false;
                    }
                }
            }
            return true;
        }
    }

    template <typename Container>
        requires std::ranges::range<Container>
    static bool check_runtime(const Container& container) noexcept(false)
    {
        return check(container);
    }
};

struct HasNoNullElementsPredicate
{
    template <typename Container>
        requires std::ranges::range<Container>
    static constexpr bool check(const Container& container) noexcept
    {
        for (const auto& element : container)
        {
            if (element == nullptr)
            {
                return false;
            }
        }
        return true;
    }

    template <typename Container>
        requires std::ranges::range<Container>
    static bool check_runtime(const Container& container) noexcept
    {
        for (const auto& element : container)
        {
            if (element == nullptr)
            {
                return false;
            }
        }
        return true;
    }
};

struct HasSizePredicate
{
    template <typename Size, typename Container>
        requires concepts::sized<Container>
    static constexpr bool check(Size expected, const Container& container) noexcept
    {
        return container.size() == static_cast<typename Container::size_type>(expected);
    }

    template <typename Size, typename Container>
        requires concepts::sized<Container>
    static constexpr bool check(const Container& container) noexcept
    {
        return container.size() == Size::value;
    }

    template <typename Size, typename Container>
        requires concepts::sized<Container>
    static bool check_runtime(Size expected, const Container& container) noexcept
    {
        return container.size() == static_cast<typename Container::size_type>(expected);
    }

    template <typename Size, typename Container>
        requires concepts::sized<Container>
    static bool check_runtime(const Container& container) noexcept
    {
        return container.size() == Size::value;
    }
};

struct IsSortedPredicate
{
    template <typename Container, typename Compare = std::less<>>
        requires std::ranges::range<Container>
    static bool check(const Container& container)
    {
        return std::is_sorted(std::begin(container), std::end(container), Compare{});
    }

    template <typename Container, typename CompareFunc>
        requires std::ranges::range<Container>
    static bool check(const Container& container, CompareFunc comp)
    {
        return std::is_sorted(std::begin(container), std::end(container), comp);
    }

    template <typename Container, typename Compare = std::less<>>
        requires std::ranges::range<Container>
    static bool check_runtime(const Container& container)
    {
        return std::is_sorted(std::begin(container), std::end(container), Compare{});
    }

    template <typename Container, typename CompareFunc>
        requires std::ranges::range<Container>
    static bool check_runtime(const Container& container, CompareFunc comp)
    {
        return std::is_sorted(std::begin(container), std::end(container), comp);
    }
};

using ContainerIsSortedPredicate = IsSortedPredicate;

struct AllSatisfyPredicate
{
    template <typename PredFunc, typename Container>
        requires std::ranges::range<Container>
    static bool check(PredFunc pred, const Container& container)
    {
        return std::all_of(std::begin(container), std::end(container), pred);
    }

    template <typename PredFunc, typename Container>
        requires std::ranges::range<Container>
    static bool check_runtime(PredFunc pred, const Container& container)
    {
        return std::all_of(std::begin(container), std::end(container), pred);
    }
};

struct AnySatisfyPredicate
{
    template <typename PredFunc, typename Container>
        requires std::ranges::range<Container>
    static bool check(PredFunc pred, const Container& container)
    {
        return std::any_of(std::begin(container), std::end(container), pred);
    }

    template <typename PredFunc, typename Container>
        requires std::ranges::range<Container>
    static bool check_runtime(PredFunc pred, const Container& container)
    {
        return std::any_of(std::begin(container), std::end(container), pred);
    }
};

struct ContainerHasElementPredicate
{
    template <typename Container, typename Element>
        requires std::ranges::range<Container>
    static bool check(const Container& container, const Element& element)
    {
        return std::find(std::begin(container), std::end(container), element) != std::end(container);
    }

    template <typename Container, typename Element>
        requires std::ranges::range<Container>
    static bool check_runtime(const Container& container, const Element& element)
    {
        return check(container, element);
    }
};

// ===================================================================
// 3. Arithmetic and Range Predicates
// ===================================================================

struct InRangePredicate
{
    template <typename T, typename U = T, typename V = T>
        requires std::is_arithmetic_v<T> && std::is_arithmetic_v<U> && std::is_arithmetic_v<V>
    static constexpr bool check(T value, U min, V max) noexcept
    {
        return value >= min && value <= max;
    }

    template <typename MinType, typename MaxType, typename T>
        requires std::is_arithmetic_v<T>
    static constexpr bool check(T value) noexcept
    {
        static_assert(std::is_arithmetic_v<decltype(MinType::value)> && std::is_arithmetic_v<decltype(MaxType::value)>,
                      "MinType and MaxType must have arithmetic ::value members.");
        return value >= MinType::value && value <= MaxType::value;
    }

    template <typename T, typename U = T, typename V = T>
        requires std::is_arithmetic_v<T> && std::is_arithmetic_v<U> && std::is_arithmetic_v<V>
    static bool check_runtime(T value, U min, V max) noexcept
    {
        return value >= min && value <= max;
    }

    template <typename MinType, typename MaxType, typename T>
        requires std::is_arithmetic_v<T>
    static bool check_runtime(T value) noexcept
    {
        static_assert(std::is_arithmetic_v<decltype(MinType::value)> && std::is_arithmetic_v<decltype(MaxType::value)>,
                      "MinType and MaxType must have arithmetic ::value members.");
        return value >= MinType::value && value <= MaxType::value;
    }
};

struct InExclusiveRangePredicate
{
    template <typename T, typename U = T, typename V = T>
        requires std::is_arithmetic_v<T> && std::is_arithmetic_v<U> && std::is_arithmetic_v<V>
    static constexpr bool check(T value, U min, V max) noexcept
    {
        return value > min && value < max;
    }

    template <typename MinType, typename MaxType, typename T>
        requires std::is_arithmetic_v<T>
    static constexpr bool check(T value) noexcept
    {
        static_assert(std::is_arithmetic_v<decltype(MinType::value)> && std::is_arithmetic_v<decltype(MaxType::value)>,
                      "MinType and MaxType must have arithmetic ::value members.");
        return value > MinType::value && value < MaxType::value;
    }

    template <typename T, typename U = T, typename V = T>
        requires std::is_arithmetic_v<T> && std::is_arithmetic_v<U> && std::is_arithmetic_v<V>
    static bool check_runtime(T value, U min, V max) noexcept
    {
        return value > min && value < max;
    }
};

struct ValidIndexPredicate
{
    template <typename Index, typename Container>
        requires concepts::sized<Container>
    static constexpr bool check(Index idx, const Container& container) noexcept
    {
        if constexpr (std::is_signed_v<Index>)
        {
            return idx >= 0 && static_cast<size_t>(idx) < container.size();
        }
        else
        {
            return static_cast<size_t>(idx) < container.size();
        }
    }

    template <typename Index, typename Container>
        requires concepts::sized<Container>
    static bool check_runtime(Index idx, const Container& container) noexcept
    {
        return check(idx, container);
    }
};

struct IsPowerOfTwoPredicate
{
    template <typename T>
        requires std::is_integral_v<T>
    static constexpr bool check(T value) noexcept
    {
        return value > 0 && (value & (value - 1)) == 0;
    }

    template <typename ValueType>
        requires std::is_integral_v<decltype(ValueType::value)>
    static constexpr bool check() noexcept
    {
        return ValueType::value > 0 && (ValueType::value & (ValueType::value - 1)) == 0;
    }

    template <typename T>
        requires std::is_integral_v<T>
    static bool check_runtime(T value) noexcept
    {
        return value > 0 && (value & (value - 1)) == 0;
    }

    template <typename ValueType>
        requires std::is_integral_v<decltype(ValueType::value)>
    static bool check_runtime() noexcept
    {
        return ValueType::value > 0 && (ValueType::value & (ValueType::value - 1)) == 0;
    }
};

struct ApproxEqualPredicate
{
    template <typename Eps, typename T, typename U = T>
        requires std::is_floating_point_v<T> && std::is_floating_point_v<U> && std::is_floating_point_v<Eps>
    static constexpr bool check(Eps epsilon, T a, U b) noexcept
    {
        return std::abs(a - b) <= epsilon;
    }

    template <typename EpsilonType, typename T, typename U = T>
        requires std::is_floating_point_v<T> && std::is_floating_point_v<U>
    static constexpr bool check(T a, U b) noexcept
    {
        static_assert(std::is_floating_point_v<decltype(EpsilonType::value)>,
                      "EpsilonType must have floating-point ::value member.");
        return std::abs(a - b) <= EpsilonType::value;
    }

    template <typename Eps, typename T, typename U = T>
        requires std::is_floating_point_v<T> && std::is_floating_point_v<U> && std::is_floating_point_v<Eps>
    static bool check_runtime(Eps epsilon, T a, U b) noexcept
    {
        return std::abs(a - b) <= epsilon;
    }

    template <typename EpsilonType, typename T, typename U = T>
        requires std::is_floating_point_v<T> && std::is_floating_point_v<U>
    static bool check_runtime(T a, U b) noexcept
    {
        static_assert(std::is_floating_point_v<decltype(EpsilonType::value)>,
                      "EpsilonType must have floating-point ::value member.");
        return std::abs(a - b) <= EpsilonType::value;
    }
};

struct IsLessThanPredicate
{
    template <typename Lhs, typename Rhs>
    static constexpr bool check(Lhs lhs, Rhs rhs) noexcept
    {
        return lhs < rhs;
    }

    template <typename Lhs, typename Rhs>
    static bool check_runtime(Lhs lhs, Rhs rhs) noexcept
    {
        return lhs < rhs;
    }
};

struct IsGreaterThanPredicate
{
    template <typename Lhs, typename Rhs>
    static constexpr bool check(Lhs lhs, Rhs rhs) noexcept
    {
        return lhs > rhs;
    }

    template <typename Lhs, typename Rhs>
    static bool check_runtime(Lhs lhs, Rhs rhs) noexcept
    {
        return lhs > rhs;
    }
};

struct IsLessThanOrEqualPredicate
{
    template <typename Lhs, typename Rhs>
    static constexpr bool check(Lhs lhs, Rhs rhs) noexcept
    {
        return lhs <= rhs;
    }

    template <typename Lhs, typename Rhs>
    static bool check_runtime(Lhs lhs, Rhs rhs) noexcept
    {
        return lhs <= rhs;
    }
};

struct IsGreaterThanOrEqualPredicate
{
    template <typename Lhs, typename Rhs>
    static constexpr bool check(Lhs lhs, Rhs rhs) noexcept
    {
        return lhs >= rhs;
    }

    template <typename Lhs, typename Rhs>
    static bool check_runtime(Lhs lhs, Rhs rhs) noexcept
    {
        return lhs >= rhs;
    }
};

// ===================================================================
// 4. Floating-Point Predicates
// ===================================================================

struct IsFinitePredicate
{
    template <typename T>
        requires std::is_floating_point_v<T>
    static bool check(T value) noexcept
    {
        return std::isfinite(value);
    }

    template <typename T>
        requires std::is_floating_point_v<T>
    static bool check_runtime(T value) noexcept
    {
        return std::isfinite(value);
    }
};

struct IsNormalPredicate
{
    template <typename T>
        requires std::is_floating_point_v<T>
    static bool check(T value) noexcept
    {
        return std::isnormal(value) || value == T{0};
    }

    template <typename T>
        requires std::is_floating_point_v<T>
    static bool check_runtime(T value) noexcept
    {
        return std::isnormal(value) || value == T{0};
    }
};

struct IsNotNaNPredicate
{
    template <typename T>
        requires std::is_floating_point_v<T>
    static bool check(T value) noexcept
    {
        return !std::isnan(value);
    }

    template <typename T>
        requires std::is_floating_point_v<T>
    static bool check_runtime(T value) noexcept
    {
        return !std::isnan(value);
    }
};

struct IsNotInfPredicate
{
    template <typename T>
        requires std::is_floating_point_v<T>
    static bool check(T value) noexcept
    {
        return !std::isinf(value);
    }

    template <typename T>
        requires std::is_floating_point_v<T>
    static bool check_runtime(T value) noexcept
    {
        return !std::isinf(value);
    }
};

// ===================================================================
// 5. Iterator Predicates
// ===================================================================

struct IsValidIteratorPredicate
{
    template <typename It, typename End>
        requires std::input_or_output_iterator<It>
    static constexpr bool check(It it, End end) noexcept
    {
        return it != end;
    }

    template <typename ItType>
        requires std::input_or_output_iterator<ItType>
    static constexpr bool check() noexcept
    {
        return true;
    }

    template <typename It, typename End>
        requires std::input_or_output_iterator<It>
    static bool check_runtime(It it, End end) noexcept
    {
        return it != end;
    }

    template <typename ItType>
        requires std::input_or_output_iterator<ItType>
    static bool check_runtime() noexcept
    {
        return true;
    }
};

} // namespace fat_p
