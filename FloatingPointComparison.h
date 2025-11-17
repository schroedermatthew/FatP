// FloatingPointComparison.h
// Core floating-point comparison policies and utilities
#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>

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
     * @return The appropriate default epsilon value.
     */
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
     * - Simple and predictable behavior
     * 
     * @note This is the default policy for areEqual().
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
            
            // Handle NaN
            if (std::isnan(a) || std::isnan(b)) return false;
            
            // Handle infinity (same sign infinities are equal)
            if (std::isinf(a) && std::isinf(b) && (a > 0) == (b > 0)) {
                return true;
            }
            if (std::isinf(a) || std::isinf(b)) return false;

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
     * 
     * Limitations:
     * - Only supports float (4 bytes) and double (8 bytes)
     * - Not available for long double (platform-dependent size)
     * 
     * @note For long double, use StandardComparisonPolicy or HybridComparisonPolicy.
     */
    struct UlpComparisonPolicy {
        /**
         * @brief Performs a ULP-based comparison with a hybrid check for subnormals.
         * @tparam T Floating-point type (float or double only).
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
            // Note: Union type-punning is technically undefined behavior in C++ standard
            // but is widely supported by all major compilers (GCC, Clang, MSVC).
            // For C++20+, consider using std::bit_cast instead.
            
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
     * Best for: Values spanning many orders of magnitude
     * 
     * Features:
     * - Scale-independent comparison
     * - Works well for large and small values
     * 
     * Limitations:
     * - Poor behavior near zero (relative error becomes meaningless)
     * - Requires explicit epsilon parameter
     * 
     * @note For robust comparison near zero, use HybridComparisonPolicy instead.
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

            double relEps;
            if constexpr (sizeof...(eps) > 0) {
                relEps = std::get<0>(std::forward_as_tuple(eps...));
            }
            else {
                // Use default epsilon if none provided
                relEps = getDefaultEpsilon<T>();
            }
            
            if (std::isnan(a) || std::isnan(b)) return false;
            if (std::isinf(a) && std::isinf(b) && (a > 0) == (b > 0)) {
                return true;
            }
            if (std::isinf(a) || std::isinf(b)) return false;
            
            double maxAbs = std::max(std::fabs(a), std::fabs(b));
            
            // Handle near-zero case
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
     * - Most robust policy available
     * 
     * @note This is the policy used by approximateEqual().
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
            
            // Extract parameters with defaults
            auto eps_tuple = std::forward_as_tuple(eps...);
            double relEps;
            double absEps;
            
            if constexpr (sizeof...(eps) >= 2) {
                relEps = std::get<0>(eps_tuple);
                absEps = std::get<1>(eps_tuple);
            }
            else if constexpr (sizeof...(eps) == 1) {
                relEps = std::get<0>(eps_tuple);
                absEps = relEps; // Use same value for both if only one provided
            }
            else {
                // Use defaults
                relEps = getDefaultEpsilon<T>();
                absEps = getDefaultEpsilon<T>();
            }
            
            if (std::isnan(a) || std::isnan(b)) return false;
            if (std::isinf(a) && std::isinf(b) && (a > 0) == (b > 0)) {
                return true;
            }
            if (std::isinf(a) || std::isinf(b)) return false;
            
            double diff = std::fabs(a - b);
            
            // Pass if absolute tolerance is met
            if (diff <= absEps) return true;
            
            // Otherwise check relative tolerance
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
     * @param absEps The absolute tolerance (default: type-specific epsilon * 0.01).
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
                     double relEps = getDefaultEpsilon<T>(), 
                     double absEps = getDefaultEpsilon<T>() * 0.01) {
        return HybridComparisonPolicy::epsilonMatch(a, b, relEps, absEps);
    }

    /**
     * @brief Basic floating-point equality comparison with a single policy.
     * 
     * This is a simplified version that works directly with floating-point types
     * without the full EqualDispatcher machinery.
     * 
     * @tparam T The floating-point type.
     * @tparam Policy The comparison policy (default: StandardComparisonPolicy).
     * @param a The first value.
     * @param b The second value.
     * @param eps Optional epsilon parameter(s) for the policy.
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
