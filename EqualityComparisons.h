// EqualityComparisons.h
#pragma once
#include <algorithm>
#include <any>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <map>
#include <set>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "DiagnosticLogger.h" // Dependency for conditionalPrintError and toString
#include "ComparisonTolerances.h" // NEW: Holds kDefaultDoubleEpsilon, kDefaultFloatEpsilon
#include "Stringify.h"

namespace cpp_utilities {
    // ====================================================================
    // Forward Declarations and Traits
    // ====================================================================
    // REMOVED: inline constexpr double kEqualityTol = 1e-9;
    // The default tolerances are now provided by ComparisonTolerances.h
    /** @brief Determines if testing containers should stop on the first error
     * or test the entire container and print error messages for each error.
     * Constexpr allows compile-time use.
     */
    inline constexpr bool kStopOnFirstError = false;
    /** @brief Forward declaration of the dispatcher struct. */
    template <typename T, typename Policy>
    struct EqualDispatcher;
    // --- Helper to get the correct default epsilon (Used by policies) ---
    template <typename T>
    constexpr double getDefaultEpsilon() {
        static_assert(std::is_floating_point_v<T>, "T must be a floating-point type.");
        if constexpr (std::is_same_v<T, float>) {
            return kDefaultFloatEpsilon;
        }
        else if constexpr (std::is_same_v<T, double>) {
            return kDefaultDoubleEpsilon;
        }
        else {
            // long double: compute appropriate epsilon
            return static_cast<double>(
                std::numeric_limits<long double>::epsilon() * 100.0L
            );
        }
    }
    /**
     * @brief Trait to check if a type has `operator==`.
     * @tparam T The type to check.
     */
    template <typename T, typename = void>
    struct HasEqual : std::false_type {};
    template <typename T>
    struct HasEqual<
        T, std::void_t<decltype(std::declval<const T&>() ==
            std::declval<const T&>())>>
        : std::true_type {};
    /**
     * @brief Trait to check if a type is a `std::pair`.
     * @tparam T The type to check.
     */
    template <typename T>
    struct IsPair : std::false_type {};
    template <typename T1, typename T2>
    struct IsPair<std::pair<T1, T2>> : std::true_type {};
    /**
     * @brief Trait to check if a type is a `std::tuple`.
     * @tparam T The type to check.
     */
    template <typename T>
    struct IsTuple : std::false_type {};
    template <typename... Ts>
    struct IsTuple<std::tuple<Ts...>> : std::true_type {};
    // Fixed: Used standard SFINAE pattern without separate function.
    template <typename T, typename = void>
    struct IsIterable : std::false_type {};
    template <typename T>
    struct IsIterable<T, std::void_t<decltype(std::begin(std::declval<T&>())), decltype(std::end(std::declval<T&>()))>>
        : std::true_type {
    };
    // Removed redundant IsContainer; use IsIterable instead.
    // Fixed: Used SFINAE for IsAssociative, IsOrdered, HasMappedType.
    template <typename T, typename = void>
    struct HasKeyCompare : std::false_type {};
    template <typename T>
    struct HasKeyCompare<T, std::void_t<typename T::key_compare>> : std::true_type {}; // For ordered map/set
    template <typename T, typename = void>
    struct HasKeyEqual : std::false_type {};
    template <typename T>
    struct HasKeyEqual<T, std::void_t<typename T::key_equal>> : std::true_type {}; // For unordered map/set
    template <typename T, typename = void>
    struct HasMappedType : std::false_type {};
    template <typename T>
    struct HasMappedType<T, std::void_t<typename T::mapped_type>> : std::true_type {};
    /**
     * @brief Alias to get the `value_type` of a container.
     * @tparam Container The container type.
     */
    template <typename Container>
    using ContainerValueT = typename Container::value_type;
    /**
     * @brief Forward declaration for recursive tuple comparison.
     * @tparam Policy The comparison policy.
     * @tparam Tuple The tuple type.
     * @tparam I An `std::index_sequence` for iterating tuple elements.
     * @tparam EpsParams Parameter pack for epsilon values.
     * @param a The first tuple.
     * @param b The second tuple.
     * @param indices The index sequence for tuple elements.
     * @param eps Parameters for epsilon comparison.
     * @return `true` if tuples are equal, `false` otherwise.
     */
    template <typename Policy, typename Tuple, std::size_t... I,
        typename... EpsParams>
    bool tupleAreEqualImpl(const Tuple& a, const Tuple& b,
        std::index_sequence<I...>, EpsParams... eps);
    // ====================================================================
    // Comparison Policies
    // ====================================================================
    /**
     * @brief Standard absolute tolerance comparison policy for floating-point
     * types. Uses type-specific defaults if no epsilon is supplied.
     */
    struct StandardComparisonPolicy {
        /**
         * @brief Performs an epsilon-based comparison.
         * @tparam T Floating-point type.
         * @param a The first value.
         * @param b The second value.
         * @param eps The absolute tolerance (optional).
         * @return `true` if values are equal within tolerance, `false` otherwise.
         */
        template <typename T, typename... EpsParams>
        static bool epsilonMatch(T a, T b, EpsParams... eps) {
            static_assert(std::is_floating_point_v<T>,
                "Policy only for floating-point types");
            double actualEps;
            if constexpr (sizeof...(eps) > 0) {
                // Use the provided epsilon (must be the first parameter in the pack)
                actualEps = std::get<0>(std::forward_as_tuple(eps...));
            }
            else {
                // Use the type-specific default epsilon
                actualEps = getDefaultEpsilon<T>();
            }
            if (std::isnan(a) || std::isnan(b)) return false;
            if (std::isinf(a) && std::isinf(b) && (a > 0) == (b > 0)) {
                return true;
            }
            if (std::isinf(a) || std::isinf(b)) return false;

            if (std::fabs(a - b) > actualEps) {
                diagnostic::conditionalPrintError([&]() -> std::string {
                    return std::string("Equality check failed: ") +
                        toString(a) +
                        " and " + toString(b) +
                        " differ by more than " + toString(actualEps);
                    });
                return false;
            }
            return true;
        }
    };
    /**
     * @brief Units in the Last Place (ULP) comparison policy for floating-point
     * types.
     */
    struct UlpComparisonPolicy {
        /**
         * @brief Performs a ULP-based comparison with a hybrid check for subnormals.
         * @tparam T Floating-point type (e.g., float, double).
         * @param a The first value.
         * @param b The second value.
         * @param eps The ULP tolerance (if provided). Defaults to 4.0.
         * @return `true` if values are equal within tolerance, `false` otherwise.
         */
        template <typename T, typename... EpsParams>
        static bool epsilonMatch(T a, T b, EpsParams... eps) {
            static_assert(std::is_floating_point_v<T>,
                "Policy only for floating-point types");
            
            // ULP comparison uses union type-punning which only works for
            // float (4 bytes) and double (8 bytes). Long double is platform-dependent
            // (10/12/16 bytes) and would cause undefined behavior.
            static_assert(sizeof(T) == 4 || sizeof(T) == 8,
                "ULP comparison only supports float (4 bytes) and double (8 bytes). "
                "For long double, use StandardComparisonPolicy or HybridComparisonPolicy.");

            double actualEps = 4.0; // Default ULP tolerance
            if constexpr (sizeof...(eps) > 0) {
                actualEps = std::get<0>(std::forward_as_tuple(eps...));
            }

            // Use a static absolute tolerance for the subnormal range
            // This is necessary because ULP definition breaks down near zero.
            // Type-specific: 1e-6 for float, 1e-12 for double
            constexpr T AbsSubnormalTolerance = [] {
                if constexpr (std::is_same_v<T, float>) {
                    return static_cast<T>(1.0e-6f);   // ~10x float epsilon
                } else {
                    return static_cast<T>(1.0e-12);   // ~5e4x double epsilon
                }
            }();

            if (std::isnan(a) || std::isnan(b)) return false;
            if (a == b) return true;

            // Handle signs (must have the same sign for ULP to make sense unless both are zero)
            if (std::signbit(a) != std::signbit(b)) return false;

            // --- CORRECTED SUBNORMAL HANDLING ---
            if (std::fpclassify(a) == FP_SUBNORMAL ||
                std::fpclassify(b) == FP_SUBNORMAL)
            {
                // If either is subnormal, use a fixed, robust absolute tolerance check.
                return std::fabs(a - b) <= AbsSubnormalTolerance;
            }

            // --- STANDARD ULP COMPARISON ---

            // Union for type-punning: allows interpreting bits as an integer.
            using IntType = std::conditional_t<sizeof(T) == 4, int32_t, int64_t>;

            union {
                T mF;
                IntType mI;
            } ua = { a }, ub = { b };

            // Convert signed magnitude to unsigned integer representation for distance calculation
            // Negative zero is handled here if it wasn't caught by a == b check.
            if (ua.mI < 0) {
                if constexpr (sizeof(T) == 4) {
                    ua.mI = INT32_MIN - ua.mI;
                }
                else {
                    ua.mI = INT64_MIN - ua.mI;
                }
            }
            if (ub.mI < 0) {
                if constexpr (sizeof(T) == 4) {
                    ub.mI = INT32_MIN - ub.mI;
                }
                else {
                    ub.mI = INT64_MIN - ub.mI;
                }
            }

            // Calculate the absolute difference in the integer representation
            auto diff = std::abs(static_cast<IntType>(ua.mI - ub.mI));

            if (diff > static_cast<decltype(diff)>(actualEps)) {
                // Logging failure message if enabled
                diagnostic::conditionalPrintError([&]() -> std::string {
                    return std::string("Equality check failed: ") +
                        toString(a) +
                        " and " + toString(b) +
                        " differ by more than " + toString(actualEps) +
                        " ULPs";
                    });
                return false;
            }

            return true;
        }
    };

    /**
     * @brief Relative tolerance comparison policy for floating-point types.
     */
    struct RelativeComparisonPolicy {
        /**
         * @brief Performs a relative tolerance-based comparison.
         * @tparam T Floating-point type.
         * @param a The first value.
         * @param b The second value.
         * @param eps The relative tolerance.
         * @return `true` if values are equal within tolerance, `false` otherwise.
         */
        template <typename T, typename... EpsParams>
        static bool epsilonMatch(T a, T b, EpsParams... eps) {
            static_assert(std::is_floating_point_v<T>,
                "Policy only for floating-point types");

            double relEps = std::get<0>(std::forward_as_tuple(eps...)); // Requires one param
            if (std::isnan(a) || std::isnan(b)) return false;
            if (std::isinf(a) && std::isinf(b) && (a > 0) == (b > 0)) {
                return true;
            }
            if (std::isinf(a) || std::isinf(b)) return false;
            double maxAbs = std::max(std::fabs(a), std::fabs(b));
            if (maxAbs <= relEps * 0.5) return true;
            if (std::fabs(a - b) > relEps * maxAbs) {
                diagnostic::conditionalPrintError([&]() -> std::string {
                    return std::string("Equality check failed: ") +
                        toString(a) +
                        " and " + toString(b) +
                        " differ by more than the relative tolerance " +
                        toString(relEps);
                    });
                return false;
            }
            return true;
        }
    };
    /**
     * @brief Hybrid (relative and absolute) tolerance comparison policy for
     * floating-point types.
     */
    struct HybridComparisonPolicy {
        /**
         * @brief Performs a hybrid tolerance-based comparison.
         * @tparam T Floating-point type.
         * @param a The first value.
         * @param b The second value.
         * @param relEps The relative tolerance.
         * @param absEps The absolute tolerance.
         * @return `true` if values are equal within tolerance, `false` otherwise.
         */
        template <typename T, typename... EpsParams>
        static bool epsilonMatch(T a, T b, EpsParams... eps) {
            static_assert(std::is_floating_point_v<T>,
                "Policy only for floating-point types");
            // Requires two parameters: relEps and absEps
            auto eps_tuple = std::forward_as_tuple(eps...);
            double relEps = std::get<0>(eps_tuple);
            double absEps = std::get<1>(eps_tuple);
            if (std::isnan(a) || std::isnan(b)) return false;
            if (std::isinf(a) && std::isinf(b) && (a > 0) == (b > 0)) {
                return true;
            }
            if (std::isinf(a) || std::isinf(b)) return false;
            double diff = std::fabs(a - b);
            if (diff <= absEps) return true;
            double maxAbs = std::max(std::fabs(a), std::fabs(b));
            if (diff > relEps * maxAbs) {
                diagnostic::conditionalPrintError([&]() -> std::string {
                    return std::string("Equality check failed: ") +
                        toString(a) +
                        " and " + toString(b) +
                        " differ by more than rel=" +
                        toString(relEps) + " abs=" +
                        toString(absEps);
                    });
                return false;
            }
            return true;
        }
    };
    // ====================================================================
    // EqualDispatcher - Central Dispatching Mechanism (Unchanged)
    // ====================================================================
    /**
     * @brief Dispatcher for equality comparison based on type and policy.
     */
    template <typename T, typename Policy>
    struct EqualDispatcher {
        /**
         * @brief Recursively compares two objects.
         */
        template <typename... EpsParams>
        static bool compare(const T& a, const T& b, EpsParams... eps) {
            if constexpr (IsPair<T>::value) {
                bool first = EqualDispatcher<typename T::first_type, Policy>::compare(
                    a.first, b.first, eps...);
                bool second = EqualDispatcher<typename T::second_type, Policy>::compare(
                    a.second, b.second, eps...);
                return first && second;
            }
            else if constexpr (IsTuple<T>::value) {
                return tupleAreEqualImpl<Policy>(
                    a, b, std::make_index_sequence<std::tuple_size_v<T>>{},
                    eps...);
            }
            else if constexpr (IsIterable<T>::value) {
                if (a.size() != b.size()) {
                    diagnostic::conditionalPrintError([&]() -> std::string {
                        return std::string("Containers have different sizes: ") +
                            std::to_string(a.size()) + " vs " +
                            std::to_string(b.size());
                        });
                    return false;
                }
                bool success = true;
                if constexpr (HasKeyEqual<T>::value) { // unordered assoc
                    for (const auto& elemA : a) {
                        using KeyT = typename T::key_type;
                        if constexpr (HasMappedType<T>::value) {
                            const KeyT& keyA = elemA.first;
                            auto itB = b.find(keyA);
                            if (itB == b.end()) {
                                conditionalPrintError([&]() -> std::string {
                                    return std::string("Key not found in second container: ") + toString(keyA);
                                    });
                                return false;
                            }
                            const auto& valA = elemA.second;
                            const auto& valB = itB->second;
                            bool result = EqualDispatcher<std::decay_t<decltype(valA)>, Policy>::compare(valA, valB, eps...);
                            if (!result) {
                                success = false;
                                conditionalPrintError([&]() -> std::string {
                                    return std::string("Values for key '") + toString(keyA) + "' differ.";
                                    });
                                if (kStopOnFirstError) {
                                    return false;
                                }
                            }
                        }
                        else { // unordered_set
                            const KeyT& keyA = elemA;
                            auto itB = b.find(keyA);
                            if (itB == b.end()) {
                                conditionalPrintError([&]() -> std::string {
                                    return std::string("Key not found in second container: ") + toString(keyA);
                                    });
                                return false;
                            }
                            const auto& valA = elemA;
                            const auto& valB = *itB;
                            bool result = EqualDispatcher<std::decay_t<decltype(valA)>, Policy>::compare(valA, valB, eps...);
                            if (!result) {
                                success = false;
                                conditionalPrintError([&]() -> std::string {
                                    return std::string("Values for key '") + toString(keyA) + "' differ.";
                                    });
                                if (kStopOnFirstError) {
                                    return false;
                                }
                            }
                        }
                    }
                    return success;
                }
                else { // ordered assoc or sequential
                    auto it1 = std::begin(a);
                    auto it2 = std::begin(b);
                    while (it1 != std::end(a)) {
                        bool result = EqualDispatcher<ContainerValueT<T>, Policy>::compare(
                            *it1, *it2, eps...);
                        if (!result) {
                            success = false;
                            diagnostic::conditionalPrintError([&]() -> std::string {
                                return std::string("Container elements differ.");
                                });
                            if (kStopOnFirstError) {
                                return false;
                            }
                        }
                        ++it1;
                        ++it2;
                    }
                    return success;
                }
            }
            else if constexpr (std::is_floating_point_v<T>) {
                return Policy::epsilonMatch(a, b, eps...);
            }
            else if constexpr (HasEqual<T>::value) {
                if (a == b) {
                    return true;
                }
                else {
                    diagnostic::conditionalPrintError([&]() -> std::string {
                        return std::string("Equality check failed: ") +
                            toString(a) + " and " +
                            toString(b) + " are not equal.";
                        });
                    return false;
                }
            }
            else {
                conditionalPrintError([&]() -> std::string {
                    return std::string("Unsupported type for equality comparison: ") +
                        typeid(T).name();
                    });
                return false;
            }
        }
    };
    /**
     * @brief Helper for comparing tuples using index sequence and fold
     * expression.
     */
    template <typename Policy, typename Tuple, std::size_t... I,
        typename... EpsParams>
    bool tupleAreEqualImpl(const Tuple& a, const Tuple& b,
        std::index_sequence<I...>, EpsParams... eps) {
        return (EqualDispatcher<std::tuple_element_t<I, Tuple>, Policy>::compare(
            std::get<I>(a), std::get<I>(b), eps...) &&
            ...);
    }
    /**
     * @brief Public interface for equality comparison.
     */
    template <typename T, typename Policy = StandardComparisonPolicy, typename... EpsParams>
    bool areEqual(const T& a, const T& b, EpsParams... eps) {
        return EqualDispatcher<T, Policy>::compare(a, b, eps...);
    }
    // --- NEW: Simplified Interface for Robust Approximate Comparison ---
    /**
     * @brief Simplified interface for robust approximate floating-point comparison.
     * * Uses the HybridComparisonPolicy and type-appropriate default epsilon/tolerances
     * from ComparisonTolerances.h.
     *
     * @tparam T The type being compared (must be float or double).
     * @param a The first value.
     * @param b The second value.
     * @return True if the values are approximately equal according to the Hybrid policy.
     */
    template <typename T>
    [[nodiscard]] // Applied [[nodiscard]] attribute for safety
    std::enable_if_t<std::is_floating_point_v<T>, bool>
        approximateEqual(const T& a, const T& b, double relEps = getDefaultEpsilon<T>(), double absEps = getDefaultEpsilon<T>()) {

        // C++17 'if constexpr' used for compile-time type selection
        if constexpr (std::is_same_v<T, double>) {
            // Calls areEqual, forcing the Hybrid policy and using double defaults
            return areEqual<T, HybridComparisonPolicy>(
                a, b, relEps, absEps);
        }
        else { // float
            // Calls areEqual, forcing the Hybrid policy and using float defaults
            return areEqual<T, HybridComparisonPolicy>(
                a, b, relEps, absEps);
        }
    }
} // namespace cpp_utilities