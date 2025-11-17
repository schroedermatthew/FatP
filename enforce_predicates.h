/**
 * @file enforce_predicates.h
 * @brief Defines the Predicate policy structs used by contract macros to
 * perform specialized, type-safe checks on various C++ types (pointers,
 * containers, arithmetic values).
 *
 * @details Each predicate provides a static `check` method that returns a
 * boolean result. They are heavily templated, using SFINAE and C++17
 * `if constexpr` to offer zero-overhead checks whenever possible.
 */
#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <stdexcept>
#include <type_traits>
#include <unordered_set>
#include <utility>

#include "ConstexprUtilities.h"
#include "TypeTraits.h"

namespace fat_p {

    // ===================================================================
    // 1. Core Predicates
    // ===================================================================
    struct BooleanPredicate {
        static constexpr bool check(bool condition) noexcept {
            return condition;
        }
        constexpr bool operator()(bool condition) const noexcept {
            return check(condition);
        }
        // Non-constexpr version (for runtime-specific logic if needed)
        static bool check_runtime(bool condition) noexcept {
            return condition;
            // Add runtime-only code here if needed
        }
    };
    struct NotNullPredicate {
        template <typename T>
        static constexpr bool check(const T ptr) noexcept {
            return ptr != nullptr;
        }
        template <typename T>
        constexpr bool operator()(const T ptr) const noexcept {
            return check(ptr);
        }
        // Non-constexpr version
        template <typename T>
        static bool check_runtime(const T ptr) noexcept {
            return ptr != nullptr;
            // Add runtime-only code here if needed
        }
    };
    struct NotEmptyPredicate {
        template <typename T,
            std::enable_if_t<has_empty<T>::value, int> = 0>
        static constexpr bool check(const T& value) noexcept {
            return !value.empty();
        }
        template <typename T,
            std::enable_if_t<has_empty<T>::value, int> = 0>
        constexpr bool operator()(const T& value) const noexcept {
            return check(value);
        }
        // Non-constexpr version
        template <typename T,
            std::enable_if_t<has_empty<T>::value, int> = 0>
        static bool check_runtime(const T& value) noexcept {
            return !value.empty();
            // Add runtime-only code here if needed
        }
    };
    struct IsPositivePredicate {
        template <typename T,
            std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
        static constexpr bool check(T value) noexcept {
            return value > T{ 0 };
        }
        template <typename T,
            std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
        constexpr bool operator()(T value) const noexcept {
            return check(value);
        }
        // Non-constexpr version
        template <typename T,
            std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
        static bool check_runtime(T value) noexcept {
            return value > T {
        0
    };
            // Add runtime-only code here if needed
        }
    };
    struct IsNonNegativePredicate {
        template <typename T,
            std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
        static constexpr bool check(T value) noexcept {
            return value >= T{ 0 };
        }
        template <typename T,
            std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
        constexpr bool operator()(T value) const noexcept {
            return check(value);
        }
        // Non-constexpr version
        template <typename T,
            std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
        static bool check_runtime(T value) noexcept {
            return value >= T{ 0 };
            // Add runtime-only code here if needed
        }
    };
    struct IsIntegralPredicate {
        template <typename T>
        static constexpr bool check(const T&) noexcept {
            return std::is_integral_v<T>;
        }
        template <typename T>
        constexpr bool operator()(const T&) const noexcept {
            return check<T>(T{});
        }
        // Non-constexpr version
        template <typename T>
        static bool check_runtime(const T&) noexcept {
            return std::is_integral_v<T>;
            // Add runtime-only code here if needed
        }
    };
    // ===================================================================
    // 2. Container Predicates
    // ===================================================================
    struct ContainerIsUniquePredicate {
        template <typename Container,
            std::enable_if_t<has_begin<Container>::value&&
            has_end<Container>::value, int> = 0>
        static bool check(const Container& container) noexcept(!is_hashable<typename std::iterator_traits<decltype(std::begin(std::declval<const Container&>()))>::value_type>::value) {
            using value_type = typename std::iterator_traits<decltype(std::begin(container))>::value_type;
            if constexpr (is_hashable<value_type>::value) {
                std::unordered_set<value_type> unique_set;
                for (const auto& elem : container) {
                    if (!unique_set.insert(elem).second) {
                        return false;
                    }
                }
                return true;
            }
            else {
                auto begin = std::begin(container);
                auto end = std::end(container);
                for (auto it = begin; it != end; ++it) {
                    for (auto jt = std::next(it); jt != end; ++jt) {
                        if (*it == *jt) return false;
                    }
                }
                return true;
            }
        }
        // Non-constexpr version (main is already non-constexpr due to loops/allocations)
        template <typename Container,
            std::enable_if_t<has_begin<Container>::value&&
            has_end<Container>::value, int> = 0>
        static bool check_runtime(const Container& container) noexcept(!is_hashable<typename std::iterator_traits<decltype(std::begin(std::declval<const Container&>()))>::value_type>::value) {
            return check(container);
            // Add runtime-only code here if needed
        }
    };
    struct HasNoNullElementsPredicate {
        template <typename Container,
            std::enable_if_t<has_begin<Container>::value&&
            has_end<Container>::value, int> = 0>
        static constexpr bool check(const Container& container) noexcept {
            for (const auto& element : container) {
                if (element == nullptr) {
                    return false;
                }
            }
            return true;
        }
        // Non-constexpr version
        template <typename Container,
            std::enable_if_t<has_begin<Container>::value&&
            has_end<Container>::value, int> = 0>
        static bool check_runtime(const Container& container) noexcept {
            for (const auto& element : container) {
                if (element == nullptr) {
                    return false;
                }
            }
            return true;
            // Add runtime-only code here if needed
        }
    };
    struct HasSizePredicate {
        // constexpr for flexibility in both runtime and compile-time if possible
        template <typename Size, typename Container,
            std::enable_if_t<has_size<Container>::value, int> = 0>
        static constexpr bool check(Size expected, const Container& container) noexcept {
            return container.size() == expected;
        }
        // New constexpr overload uses Size::value for compile-time
        template <typename Size, typename Container,
            std::enable_if_t<has_size<Container>::value, int> = 0>
        static constexpr bool check(const Container& container) noexcept {
            return container.size() == Size::value;
        }
        // Non-constexpr version (for runtime; can add non-constexpr logic)
        template <typename Size, typename Container,
            std::enable_if_t<has_size<Container>::value, int> = 0>
        static bool check_runtime(Size expected, const Container& container) noexcept {
            return container.size() == expected;
            // Add runtime-only code here if needed
        }
        // Non-constexpr overload for Size::value
        template <typename Size, typename Container,
            std::enable_if_t<has_size<Container>::value, int> = 0>
        static bool check_runtime(const Container& container) noexcept {
            return container.size() == Size::value;
            // Add runtime-only code here if needed
        }
    };
    struct IsSortedPredicate {
        template <typename Container, typename Compare = std::less<>,
            std::enable_if_t<has_begin<Container>::value&&
            has_end<Container>::value&&
            is_valid_comparator<Compare, typename Container::value_type>::value,
            int> = 0>
        static constexpr bool check(const Container& container) {
            return std::is_sorted(std::begin(container), std::end(container), Compare{});
        }
        template <typename Container, typename CompareFunc,
            std::enable_if_t<has_begin<Container>::value&&
            has_end<Container>::value, int> = 0>
        static constexpr bool check(const Container& container,
            CompareFunc comp) {
            return std::is_sorted(std::begin(container),
                std::end(container), comp);
        }
        // Non-constexpr versions
        template <typename Container, typename Compare = std::less<>,
            std::enable_if_t<has_begin<Container>::value&&
            has_end<Container>::value&&
            is_valid_comparator<Compare, typename Container::value_type>::value,
            int> = 0>
        static bool check_runtime(const Container& container) {
            return std::is_sorted(std::begin(container), std::end(container), Compare{});
            // Add runtime-only code here if needed
        }
        template <typename Container, typename CompareFunc,
            std::enable_if_t<has_begin<Container>::value&&
            has_end<Container>::value, int> = 0>
        static bool check_runtime(const Container& container,
            CompareFunc comp) {
            return std::is_sorted(std::begin(container),
                std::end(container), comp);
            // Add runtime-only code here if needed
        }
    };
    struct AllSatisfyPredicate {
        template <typename PredFunc, typename Container,
            std::enable_if_t<has_begin<Container>::value&&
            has_end<Container>::value, int> = 0>
        static constexpr bool check(PredFunc pred, const Container& container) {
            return std::all_of(std::begin(container), std::end(container),
                pred);
        }
        // Non-constexpr version
        template <typename PredFunc, typename Container,
            std::enable_if_t<has_begin<Container>::value&&
            has_end<Container>::value, int> = 0>
        static bool check_runtime(PredFunc pred, const Container& container) {
            return std::all_of(std::begin(container), std::end(container),
                pred);
            // Add runtime-only code here if needed
        }
    };
    struct AnySatisfyPredicate {
        template <typename PredFunc, typename Container,
            std::enable_if_t<has_begin<Container>::value&&
            has_end<Container>::value, int> = 0>
        static constexpr bool check(PredFunc pred, const Container& container) {
            return std::any_of(std::begin(container), std::end(container),
                pred);
        }
        // Non-constexpr version
        template <typename PredFunc, typename Container,
            std::enable_if_t<has_begin<Container>::value&&
            has_end<Container>::value, int> = 0>
        static bool check_runtime(PredFunc pred, const Container& container) {
            return std::any_of(std::begin(container), std::end(container),
                pred);
            // Add runtime-only code here if needed
        }
    };
    // ===================================================================
    // 3. Arithmetic and Range Predicates
    // ===================================================================
    struct InRangePredicate {
        template <typename T, typename U = T, typename V = T,
            std::enable_if_t<std::is_arithmetic_v<T>&&
            std::is_arithmetic_v<U>&&
            std::is_arithmetic_v<V>, int> = 0>
        static constexpr bool check(T value, U min, V max) noexcept {
            return value >= min && value <= max;
        }
        template <typename MinType, typename MaxType, typename T,
            std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
        static constexpr bool check(T value) noexcept {
            static_assert(std::is_arithmetic_v<decltype(MinType::value)> &&
                std::is_arithmetic_v<decltype(MaxType::value)>,
                "MinType and MaxType must have arithmetic ::value members.");
            return value >= MinType::value && value <= MaxType::value;
        }
        // Non-constexpr versions
        template <typename T, typename U = T, typename V = T,
            std::enable_if_t<std::is_arithmetic_v<T>&&
            std::is_arithmetic_v<U>&&
            std::is_arithmetic_v<V>, int> = 0>
        static bool check_runtime(T value, U min, V max) noexcept {
            return value >= min && value <= max;
            // Add runtime-only code here if needed
        }
        template <typename MinType, typename MaxType, typename T,
            std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
        static bool check_runtime(T value) noexcept {
            static_assert(std::is_arithmetic_v<decltype(MinType::value)> &&
                std::is_arithmetic_v<decltype(MaxType::value)>,
                "MinType and MaxType must have arithmetic ::value members.");
            return value >= MinType::value && value <= MaxType::value;
            // Add runtime-only code here if needed
        }
    };
    struct IsPowerOfTwoPredicate {
        template <typename T,
            std::enable_if_t<std::is_integral_v<T>, int> = 0>
        static constexpr bool check(T value) noexcept {
            return value > 0 && (value & (value - 1)) == 0;
        }
        template <typename ValueType,
            std::enable_if_t<std::is_integral_v<
            decltype(ValueType::value)>, int> = 0>
        static constexpr bool check() noexcept {
            return ValueType::value > 0 &&
                (ValueType::value & (ValueType::value - 1)) == 0;
        }
        // Non-constexpr versions
        template <typename T,
            std::enable_if_t<std::is_integral_v<T>, int> = 0>
        static bool check_runtime(T value) noexcept {
            return value > 0 && (value & (value - 1)) == 0;
            // Add runtime-only code here if needed
        }
        template <typename ValueType,
            std::enable_if_t<std::is_integral_v<
            decltype(ValueType::value)>, int> = 0>
        static bool check_runtime() noexcept {
            return ValueType::value > 0 &&
                (ValueType::value & (ValueType::value - 1)) == 0;
            // Add runtime-only code here if needed
        }
    };
    struct ApproxEqualPredicate {
        // Original version with epsilon as parameter (constexpr, using custom abs)
        template <typename Eps, typename T, typename U = T,
            std::enable_if_t<std::is_floating_point_v<T>&&
            std::is_floating_point_v<U>&&
            std::is_floating_point_v<Eps>, int> = 0>
        static constexpr bool check(Eps epsilon, T a, U b) noexcept {
            return std::abs(a - b) <= epsilon; // Use custom constexpr abs
        }
        // Compile-time version with EpsilonType (constexpr, using custom abs)
        template <typename EpsilonType, typename T, typename U = T,
            std::enable_if_t<std::is_floating_point_v<T>&&
            std::is_floating_point_v<U>, int> = 0>
        static constexpr bool check(T a, U b) noexcept {
            static_assert(std::is_floating_point_v<decltype(EpsilonType::value)>,
                "EpsilonType must have floating-point ::value member.");
            return std::abs(a - b) <= EpsilonType::value; // Use custom constexpr abs
        }
        // Non-constexpr version as requested (for runtime; can add non-constexpr logic)
        template <typename Eps, typename T, typename U = T,
            std::enable_if_t<std::is_floating_point_v<T>&&
            std::is_floating_point_v<U>&&
            std::is_floating_point_v<Eps>, int> = 0>
        static bool check_runtime(Eps epsilon, T a, U b) noexcept {
            return std::abs(a - b) <= epsilon; // Can use std::abs here since not constexpr
            // Add runtime-only code if needed, e.g., std::cout << "Runtime check\n";
        }
        // Optional: Non-constexpr version for EpsilonType
        template <typename EpsilonType, typename T, typename U = T,
            std::enable_if_t<std::is_floating_point_v<T>&&
            std::is_floating_point_v<U>, int> = 0>
        static bool check_runtime(T a, U b) noexcept {
            static_assert(std::is_floating_point_v<decltype(EpsilonType::value)>,
                "EpsilonType must have floating-point ::value member.");
            return std::abs(a - b) <= EpsilonType::value;
            // Add runtime-only code if needed
        }
    };
    struct IsLessThanPredicate {
        template <typename Lhs, typename Rhs>
        static constexpr bool check(Lhs lhs, Rhs rhs) noexcept {
            return lhs < rhs;
        }
        // Non-constexpr version
        template <typename Lhs, typename Rhs>
        static bool check_runtime(Lhs lhs, Rhs rhs) noexcept {
            return lhs < rhs;
            // Add runtime-only code here if needed
        }
    };
    struct IsGreaterThanPredicate {
        template <typename Lhs, typename Rhs>
        static constexpr bool check(Lhs lhs, Rhs rhs) noexcept {
            return lhs > rhs;
        }
        // Non-constexpr version
        template <typename Lhs, typename Rhs>
        static bool check_runtime(Lhs lhs, Rhs rhs) noexcept {
            return lhs > rhs;
            // Add runtime-only code here if needed
        }
    };
    struct IsLessThanOrEqualPredicate {
        template <typename Lhs, typename Rhs>
        static constexpr bool check(Lhs lhs, Rhs rhs) noexcept {
            return lhs <= rhs;
        }
        // Non-constexpr version
        template <typename Lhs, typename Rhs>
        static bool check_runtime(Lhs lhs, Rhs rhs) noexcept {
            return lhs <= rhs;
            // Add runtime-only code here if needed
        }
    };
    struct IsGreaterThanOrEqualPredicate {
        template <typename Lhs, typename Rhs>
        static constexpr bool check(Lhs lhs, Rhs rhs) noexcept {
            return lhs >= rhs;
        }
        // Non-constexpr version
        template <typename Lhs, typename Rhs>
        static bool check_runtime(Lhs lhs, Rhs rhs) noexcept {
            return lhs >= rhs;
            // Add runtime-only code here if needed
        }
    };
    // ===================================================================
    // 4. Iterator Predicates
    // ===================================================================
    struct IsValidIteratorPredicate {
        template <typename It, typename End,
            std::enable_if_t<
            std::is_base_of_v<std::input_iterator_tag,
            typename std::iterator_traits<It>::
            iterator_category>, int> = 0>
        static constexpr bool check(It it, End end) noexcept {
            return it != end;
        }
        template <typename ItType,
            std::enable_if_t<
            std::is_base_of_v<std::input_iterator_tag,
            typename std::iterator_traits<ItType>::
            iterator_category>, int> = 0>
        static constexpr bool check() noexcept {
            return true;
        }
        // Non-constexpr versions
        template <typename It, typename End,
            std::enable_if_t<
            std::is_base_of_v<std::input_iterator_tag,
            typename std::iterator_traits<It>::
            iterator_category>, int> = 0>
        static bool check_runtime(It it, End end) noexcept {
            return it != end;
            // Add runtime-only code here if needed
        }
        template <typename ItType,
            std::enable_if_t<
            std::is_base_of_v<std::input_iterator_tag,
            typename std::iterator_traits<ItType>::
            iterator_category>, int> = 0>
        static bool check_runtime() noexcept {
            return true;
            // Add runtime-only code here if needed
        }
    };
} // namespace fat_p