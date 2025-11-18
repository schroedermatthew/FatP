/**
 * @file FloatingPointComparison.h
 * @brief Robust floating-point comparison utilities
 * 
 * QUICK START:
 * - For most cases: Use approximateEqual() with HybridComparisonPolicy (default)
 * - For exact bit-level testing: Use floatEqual<T, UlpComparisonPolicy>()
 * - For simple absolute tolerance: Use floatEqual() with StandardComparisonPolicy (default)
 * 
 * POLICY SELECTION GUIDE:
 * - StandardComparisonPolicy: Absolute difference |a - b| <= epsilon
 *   → Best for: Values near a known scale, simple use cases
 * 
 * - RelativeComparisonPolicy: |a - b| <= epsilon * max(|a|, |b|)
 *   → Best for: Values spanning many orders of magnitude (WARNING: fails near zero)
 * 
 * - UlpComparisonPolicy: Unit in Last Place (bit-exact) comparison
 *   → Best for: Testing numerical algorithms, bit-exact requirements
 *   → Limitations: float/double only (no long double), complex handling near zero
 * 
 * - HybridComparisonPolicy: Combines absolute AND relative (RECOMMENDED)
 *   → Best for: Production code requiring robustness across all scales
 *   → Handles both near-zero and large magnitude values correctly
 */

#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>
#include <algorithm> // For std::max
#include <optional>  // For std::optional in detail::handleSpecialValues

// For C++20 support for type-punning without UB
#if __cplusplus >= 202002L
#include <bit>
#endif

#include "ComparisonTolerances.h" // kDefaultDoubleEpsilon, kDefaultFloatEpsilon
#include "DiagnosticLogger.h"     // For conditionalPrintError
#include "Stringify.h"            // For toString

namespace fat_p {

    // ====================================================================
    // Helper Functions
    // ====================================================================

    /**
     * @brief Helper to get the correct default epsilon for a floating-point type.
     * @tparam T The floating-point type (float, double, or long double).
     * @return The appropriate default epsilon value, correctly typed as T.
     */
    template <typename T>
    constexpr T getDefaultEpsilon() {
        static_assert(std::is_floating_point_v<T>, "T must be a floating-point type.");
        if constexpr (std::is_same_v<T, float>) {
            return static_cast<T>(kDefaultFloatEpsilon);
        }
        else if constexpr (std::is_same_v<T, double>) {
            return static_cast<T>(kDefaultDoubleEpsilon);
        }
        else {
            // long double: compute appropriate epsilon
            return static_cast<T>(
                std::numeric_limits<long double>::epsilon() * 100.0L
            );
        }
    }

    namespace detail {
        /**
         * @brief Centralized handling for special floating-point values (NaN, Inf).
         * @return std::optional<bool>: 
         * - value if special values determine equality (true/false).
         * - nullopt if comparison should proceed using tolerance logic.
         */
        template <typename T>
        std::optional<bool> handleSpecialValues(T a, T b) {
            // NaN is never equal to anything
            if (std::isnan(a) || std::isnan(b)) return false;

            // Same-sign infinities are equal
            if (std::isinf(a) && std::isinf(b)) {
                return (a > 0) == (b > 0);
            }

            // One infinity, one finite: not equal
            if (std::isinf(a) || std::isinf(b)) return false;

            // Normal values: proceed with comparison
            return std::nullopt;
        }

        /**
         * @brief Check if values have consistent signs.
         * 
         * Ensures that values with opposite signs (excluding ±0) are never
         * considered equal, regardless of magnitude. This is a critical
         * correctness check for all comparison policies.
         * 
         * @return std::optional<bool>:
         * - true if both are zero (including -0.0 and +0.0)
         * - false if opposite signs (and not both zero)
         * - nullopt if same sign (continue with tolerance checks)
         */
        template <typename T>
        std::optional<bool> handleSignConsistency(T a, T b) {
            // Both zero (including -0.0 and +0.0) are considered equal
            if (a == static_cast<T>(0.0) && b == static_cast<T>(0.0)) {
                return true;
            }
            
            // Opposite signs (non-zero) should never be equal
            if (std::signbit(a) != std::signbit(b)) {
                return false;
            }
            
            // Same sign, continue with tolerance checks
            return std::nullopt;
        }
    } // namespace detail


    // ====================================================================
    // Comparison Policies
    // ====================================================================

    /**
     * @brief Standard absolute tolerance comparison policy for floating-point types.
     * 
     * Uses absolute difference comparison: |a - b| <= epsilon
     * Best for: Most use cases, values near a known scale
     * 
     * Features:
     * - Uses type-specific defaults if no epsilon is supplied
     * - Handles NaN, infinity, and subnormal numbers correctly
     * - Enforces sign consistency (opposite signs are never equal)
     * - Simple and predictable behavior
     * 
     * @note This is the default policy for floatEqual().
     * 
     * @example
     * @code
     * double a = 1.0, b = 1.0 + 1e-10;
     * bool eq = floatEqual(a, b);  // Uses StandardComparisonPolicy by default
     * bool eq2 = floatEqual(a, b, 1e-8);  // Custom epsilon
     * @endcode
     */
    struct StandardComparisonPolicy {
        template <typename T, typename... EpsParams>
        static bool epsilonMatch(T a, T b, EpsParams... eps) {
            static_assert(std::is_floating_point_v<T>,
                "Policy only for floating-point types");

            // Handle NaN and Infinity
            if (auto result = detail::handleSpecialValues(a, b)) {
                return *result;
            }

            // Handle sign consistency
            if (auto sign_result = detail::handleSignConsistency(a, b)) {
                return *sign_result;
            }

            T actualEps;
            if constexpr (sizeof...(eps) > 0) {
                // Use the provided epsilon
                actualEps = std::get<0>(std::forward_as_tuple(eps...));
            }
            else {
                // Use the type-specific default epsilon
                actualEps = getDefaultEpsilon<T>();
            }

            // Standard absolute difference comparison
            if (std::fabs(a - b) > actualEps) {
                diagnostic::conditionalPrintError([&]() -> std::string {
                    return std::string("Equality check failed: ") +
                        toString(a) + " and " + toString(b) +
                        " differ by more than " + toString(actualEps);
                    });
                return false;
            }
            return true;
        }
    };

    /**
     * @brief Units in the Last Place (ULP) comparison policy for floating-point types.
     * 
     * Compares values based on their binary representation distance.
     * Best for: Bit-exact comparisons, testing numerical algorithms
     * 
     * Features:
     * - Considers floating-point representation directly
     * - Default tolerance: 4 ULPs
     * - Hybrid handling of subnormals using absolute tolerance
     * - Enforces sign consistency (opposite signs are never equal)
     * - C++20: Uses std::bit_cast (standard-compliant)
     * - Pre-C++20: Uses union type-punning (widely supported)
     * 
     * Limitations:
     * - Only supports float (4 bytes) and double (8 bytes)
     * - Not available for long double (platform-dependent size)
     * 
     * @note For long double, use StandardComparisonPolicy or HybridComparisonPolicy.
     * 
     * @example
     * @code
     * double a = 1.0;
     * double b = std::nextafter(1.0, 2.0);  // Next representable value
     * bool eq = floatEqual<double, UlpComparisonPolicy>(a, b, 1.0);  // Within 1 ULP
     * @endcode
     */
    struct UlpComparisonPolicy {
        template <typename T, typename... EpsParams>
        static bool epsilonMatch(T a, T b, EpsParams... eps) {
            static_assert(std::is_floating_point_v<T>,
                "Policy only for floating-point types");

            // ULP comparison only supports float (4 bytes) and double (8 bytes)
            static_assert(sizeof(T) == 4 || sizeof(T) == 8,
                "ULP comparison only supports float (4 bytes) and double (8 bytes). "
                "For long double, use StandardComparisonPolicy or HybridComparisonPolicy.");

            // Handle NaN and Infinity
            if (auto result = detail::handleSpecialValues(a, b)) {
                return *result;
            }

            T actualEps = static_cast<T>(4.0); // Default ULP tolerance (4 ULPs)
            if constexpr (sizeof...(eps) > 0) {
                actualEps = std::get<0>(std::forward_as_tuple(eps...));
            }
            
            // Optimization: exact match is always true (handles zero)
            if (a == b) return true;

            // Use a fixed absolute tolerance for the subnormal range (where ULP breaks down)
            // Rationale: In the subnormal range, ULP distance becomes unreliable because
            // the exponent is fixed at the minimum value and only the mantissa varies.
            // These thresholds (1e-6 for float, 1e-12 for double) are chosen to be:
            // - Large enough to span the entire subnormal range robustly
            // - Small enough to not cause false positives for normalized values near zero
            constexpr T AbsSubnormalTolerance = [] {
                if constexpr (std::is_same_v<T, float>) {
                    // 1e-6 ~= 10 * FLT_EPSILON, covers float subnormals (min: ~1.4e-45)
                    return static_cast<T>(1.0e-6f);
                } else {
                    // 1e-12 ~= 5000 * DBL_EPSILON, covers double subnormals (min: ~5e-324)
                    return static_cast<T>(1.0e-12);
                }
            }();

            // Handle signs (must have the same sign for ULP to make sense unless both are zero)
            if (std::signbit(a) != std::signbit(b)) return false;

            // --- CORRECTED SUBNORMAL HANDLING ---
            if (std::fpclassify(a) == FP_SUBNORMAL ||
                std::fpclassify(b) == FP_SUBNORMAL)
            {
                // If either is subnormal, use a fixed, robust absolute tolerance check.
                return std::fabs(a - b) <= AbsSubnormalTolerance;
            }

            // --- STANDARD ULP COMPARISON using Type-Punning ---
            using IntType = std::conditional_t<sizeof(T) == 4, int32_t, int64_t>;
            IntType int_a, int_b;

            // Use std::bit_cast (C++20 compliant) or union (pre-C++20, common UB but widely supported)
            #if __cplusplus >= 202002L
                int_a = std::bit_cast<IntType>(a);
                int_b = std::bit_cast<IntType>(b);
            #else
                union {
                    T mF;
                    IntType mI;
                } ua = { a }, ub = { b };
                int_a = ua.mI;
                int_b = ub.mI;
            #endif

            // Convert signed magnitude to unsigned integer representation for distance calculation
            if (int_a < 0) {
                if constexpr (sizeof(T) == 4) {
                    int_a = INT32_MIN - int_a;
                }
                else {
                    int_a = INT64_MIN - int_a;
                }
            }
            if (int_b < 0) {
                if constexpr (sizeof(T) == 4) {
                    int_b = INT32_MIN - int_b;
                }
                else {
                    int_b = INT64_MIN - int_b;
                }
            }

            // Calculate the absolute difference in the integer representation (ULP count)
            auto diff = std::abs(static_cast<IntType>(int_a - int_b));

            if (diff > static_cast<decltype(diff)>(actualEps)) {
                // Logging failure message if enabled
                diagnostic::conditionalPrintError([&]() -> std::string {
                    return std::string("Equality check failed: ") +
                        toString(a) + " and " + toString(b) +
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
     * 
     * Compares values based on their relative difference: |a - b| <= epsilon * max(|a|, |b|)
     * Best for: Values spanning many orders of magnitude but bounded away from zero
     * 
     * Features:
     * - Scale-independent comparison
     * - Works well for large and small values (if not too close to zero)
     * - Enforces sign consistency (opposite signs are never equal)
     * 
     * Limitations:
     * - Poor behavior near zero (relative error becomes meaningless)
     * - Requires explicit epsilon parameter for best results
     * 
     * @warning This policy is unreliable for values near zero (relative error becomes
     *          meaningless as magnitude approaches zero). For robust comparison across
     *          all scales, use HybridComparisonPolicy instead.
     * 
     * @note For robust comparison near zero, use HybridComparisonPolicy instead.
     * 
     * @example
     * @code
     * // Good use case: comparing large values
     * double a = 1e6, b = 1e6 + 0.01;
     * bool eq = floatEqual<double, RelativeComparisonPolicy>(a, b, 1e-5);
     * 
     * // Bad use case: comparing near-zero values
     * double c = 1e-15, d = 2e-15;  // Use HybridComparisonPolicy instead!
     * @endcode
     */
    struct RelativeComparisonPolicy {
        template <typename T, typename... EpsParams>
        static bool epsilonMatch(T a, T b, EpsParams... eps) {
            static_assert(std::is_floating_point_v<T>,
                "Policy only for floating-point types");

            // Handle NaN and Infinity
            if (auto result = detail::handleSpecialValues(a, b)) {
                return *result;
            }

            // Handle sign consistency
            if (auto sign_result = detail::handleSignConsistency(a, b)) {
                return *sign_result;
            }

            T relEps;
            if constexpr (sizeof...(eps) > 0) {
                relEps = std::get<0>(std::forward_as_tuple(eps...));
            }
            else {
                relEps = getDefaultEpsilon<T>();
            }

            T maxAbs = std::max(std::fabs(a), std::fabs(b));

            // Avoid division by zero and handle exact equality at zero
            if (maxAbs == static_cast<T>(0.0)) return true; // a == b == 0.0

            if (std::fabs(a - b) > relEps * maxAbs) {
                diagnostic::conditionalPrintError([&]() -> std::string {
                    return std::string("Equality check failed: ") +
                        toString(a) + " and " + toString(b) +
                        " differ by more than the relative tolerance " +
                        toString(relEps);
                    });
                return false;
            }
            return true;
        }
    };

    /**
     * @brief Hybrid (relative and absolute) tolerance comparison policy.
     * 
     * Combines relative and absolute tolerances: passes if either condition is met
     * - Absolute: |a - b| <= absEps
     * - Relative: |a - b| <= relEps * max(|a|, |b|)
     * 
     * Best for: Robust comparison across all scales (RECOMMENDED for production code)
     * 
     * Features:
     * - Handles both near-zero and large values correctly
     * - Combines benefits of absolute and relative comparison
     * - Enforces sign consistency (opposite signs are never equal)
     * - Most robust policy available
     * 
     * @note This is the policy used by approximateEqual().
     * 
     * @example
     * @code
     * // Near zero - absolute tolerance dominates
     * double a = 1e-10, b = 2e-10;
     * bool eq1 = floatEqual<double, HybridComparisonPolicy>(a, b, 1e-5, 1e-8);
     * 
     * // Large values - relative tolerance dominates
     * double c = 1e6, d = 1e6 + 0.01;
     * bool eq2 = floatEqual<double, HybridComparisonPolicy>(c, d, 1e-5, 1e-8);
     * @endcode
     */
    struct HybridComparisonPolicy {
        template <typename T, typename... EpsParams>
        static bool epsilonMatch(T a, T b, EpsParams... eps) {
            static_assert(std::is_floating_point_v<T>,
                "Policy only for floating-point types");

            // Handle NaN and Infinity
            if (auto result = detail::handleSpecialValues(a, b)) {
                return *result;
            }

            // Handle sign consistency
            if (auto sign_result = detail::handleSignConsistency(a, b)) {
                return *sign_result;
            }

            // Extract parameters with defaults
            auto eps_tuple = std::forward_as_tuple(eps...);
            T relEps;
            T absEps;

            if constexpr (sizeof...(eps) >= 2) {
                relEps = std::get<0>(eps_tuple);
                absEps = std::get<1>(eps_tuple);
            }
            else if constexpr (sizeof...(eps) == 1) {
                // Use the provided single value for both tolerances
                relEps = std::get<0>(eps_tuple);
                absEps = relEps;
            }
            else {
                // Use type-specific defaults for both tolerances
                relEps = getDefaultEpsilon<T>();
                absEps = getDefaultEpsilon<T>();
            }

            T diff = std::fabs(a - b);

            // 1. Pass if absolute tolerance is met (handles near-zero)
            if (diff <= absEps) return true;

            // 2. Otherwise check relative tolerance (handles large magnitude)
            T maxAbs = std::max(std::fabs(a), std::fabs(b));
            if (diff > relEps * maxAbs) {
                diagnostic::conditionalPrintError([&]() -> std::string {
                    return std::string("Equality check failed: ") +
                        toString(a) + " and " + toString(b) +
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
    // Convenience Functions
    // ====================================================================

    /**
     * @brief Simplified interface for robust approximate floating-point comparison.
     * 
     * Uses the HybridComparisonPolicy with sensible defaults.
     * This is the recommended function for most floating-point comparisons.
     * 
     * @tparam T The type being compared (must be float, double, or long double).
     * @param a The first value.
     * @param b The second value.
     * @param relEps The relative tolerance (default: type-specific epsilon).
     * @param absEps The absolute tolerance (default: type-specific epsilon).
     * @return True if the values are approximately equal according to the Hybrid policy.
     * 
     * @example
     * @code
     * double x = 1.0;
     * double y = 1.0 + 1e-10;
     * bool equal = approximateEqual(x, y); // Uses defaults
     * 
     * // With custom tolerances
     * bool equal2 = approximateEqual(x, y, 1e-5, 1e-8);
     * @endcode
     */
    template <typename T>
    [[nodiscard]]
    std::enable_if_t<std::is_floating_point_v<T>, bool>
    approximateEqual(const T& a, const T& b,
                     T relEps = getDefaultEpsilon<T>(), // Use T for precision
                     T absEps = getDefaultEpsilon<T>()) { // absEps defaults to relEps
        return HybridComparisonPolicy::epsilonMatch(a, b, relEps, absEps);
    }

    /**
     * @brief Basic floating-point equality comparison with a single policy.
     * 
     * This is a simplified version that works directly with floating-point types
     * without the full EqualDispatcher machinery. Allows explicit selection of
     * the comparison policy.
     * 
     * @tparam T The floating-point type.
     * @tparam Policy The comparison policy (default: StandardComparisonPolicy).
     * @param a The first value.
     * @param b The second value.
     * @param eps Optional epsilon parameter(s) for the policy (should be of type T).
     * @return True if values are equal according to the policy.
     * 
     * @example
     * @code
     * // Standard absolute comparison
     * bool eq1 = floatEqual(1.0, 1.0 + 1e-10);
     * 
     * // With custom epsilon
     * bool eq2 = floatEqual(1.0, 1.1, 0.2);
     * 
     * // With ULP policy
     * bool eq3 = floatEqual<double, UlpComparisonPolicy>(1.0, nextafter(1.0, 2.0));
     * @endcode
     */
    template <typename T, typename Policy = StandardComparisonPolicy, typename... EpsParams>
    [[nodiscard]]
    std::enable_if_t<std::is_floating_point_v<T>, bool>
    floatEqual(const T& a, const T& b, EpsParams... eps) {
        return Policy::epsilonMatch(a, b, eps...);
    }

} // namespace fat_p
