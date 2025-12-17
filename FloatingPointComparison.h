/**
 * @file FloatingPointComparison.h
 * @brief Robust floating-point comparison utilities
 * * CHANGE LOG:
 * - Added <cstdlib> for std::abs integer support.
 * - OPTIMIZED LOGIC (Standard/Hybrid): Absolute Tolerance check now runs BEFORE
 * Sign Consistency check. This creates a stable "noise floor" around zero,
 * treating +noise and -noise as equal if within absolute epsilon.
 * - UlpComparisonPolicy and RelativeComparisonPolicy remain strict regarding signs.
 */
#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>
#include <algorithm> // For std::max
#include <cstdlib>   // For std::abs (integer)
#include <cstring>   // For std::memcpy (C++17 type-punning)
#include <optional>  // For std::optional in detail::handleSpecialValues

 // For C++20 support for type-punning without UB
#if __cplusplus >= 202002L
#include <bit>
#endif

#include "ComparisonTolerances.h"
#include "DiagnosticLogger_Core.h"
#include "Stringify.h"

namespace fat_p {

    // ====================================================================
    // Helper Functions
    // ====================================================================

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
            return static_cast<T>(std::numeric_limits<long double>::epsilon() * 100.0L);
        }
    }

    namespace detail {
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
            return std::nullopt;
        }

        /**
         * @brief Checks if signs differ.
         * Note: This returns true for +0.0 and -0.0, but that case is usually
         * caught by equality checks or absolute tolerance checks before this is called.
         */
        template <typename T>
        bool areSignsDifferent(T a, T b) {
            return std::signbit(a) != std::signbit(b);
        }
    } // namespace detail

    // ====================================================================
    // Comparison Policies
    // ====================================================================

    /**
     * @brief Standard absolute tolerance comparison policy.
     * * LOGIC ORDER:
     * 1. Check Special Values (NaN/Inf)
     * 2. Check Absolute Tolerance (Priority 1: Handles noise around zero)
     * 3. Check Sign Consistency (Priority 2: Enforces direction for larger values)
     */
    struct StandardComparisonPolicy {
        template <typename T, typename... EpsParams>
        [[nodiscard]] static bool epsilonMatch(T a, T b, EpsParams... eps) {
            static_assert(std::is_floating_point_v<T>, "Policy only for floating-point types");

            if (auto result = detail::handleSpecialValues(a, b)) return *result;

            T actualEps;
            if constexpr (sizeof...(eps) > 0) {
                actualEps = std::get<0>(std::forward_as_tuple(eps...));
            }
            else {
                actualEps = getDefaultEpsilon<T>();
            }

            // 1. ABSOLUTE TOLERANCE CHECK (Priority #1)
            // Allows crossing zero if within noise floor.
            if (std::fabs(a - b) <= actualEps) {
                return true;
            }

            // 2. SIGN CONSISTENCY CHECK (Priority #2)
            // If outside noise floor, opposite signs are never equal.
            if (detail::areSignsDifferent(a, b)) {
                return false;
            }

            // If we get here: Signs are same, but difference > epsilon.
            #ifndef NDEBUG
                LOG_ERROR(std::string("Equality check failed: ") +
                          toString(std::fabs(a - b)) + " > " + toString(actualEps));
#endif
            return false;
        }
    };

    /**
     * @brief Units in the Last Place (ULP) comparison policy.
     * * LOGIC ORDER:
     * 1. Check Special Values
     * 2. Exact Equality Optimization
     * 3. STRICT SIGN CHECK (ULP distance across zero is not linear)
     * 4. Subnormal Fallback
     * 5. ULP Calculation
     */
    struct UlpComparisonPolicy {
        template <typename T, typename... EpsParams>
        [[nodiscard]] static bool epsilonMatch(T a, T b, EpsParams... eps) {
            static_assert(std::is_floating_point_v<T>, "Policy only for floating-point types");
            static_assert(sizeof(T) == 4 || sizeof(T) == 8,
                "ULP comparison only supports float (4 bytes) and double (8 bytes).");

            if (auto result = detail::handleSpecialValues(a, b)) return *result;

            // Optimization: exact match
            if (a == b) return true;

            // STRICT SIGN CHECK
            // ULP distance is meaningless across zero (jumping from +0 to -0 involves flipping the sign bit, huge integer diff).
            if (detail::areSignsDifferent(a, b)) return false;

            T actualEps = static_cast<T>(4.0);
            if constexpr (sizeof...(eps) > 0) {
                actualEps = std::get<0>(std::forward_as_tuple(eps...));
            }

            // Subnormal handling
            constexpr T AbsSubnormalTolerance = [] {
                if constexpr (std::is_same_v<T, float>) return static_cast<T>(1.0e-6f);
                else return static_cast<T>(1.0e-12);
                }();

            if (std::fpclassify(a) == FP_SUBNORMAL || std::fpclassify(b) == FP_SUBNORMAL) {
                return std::fabs(a - b) <= AbsSubnormalTolerance;
            }

            using IntType = std::conditional_t<sizeof(T) == 4, int32_t, int64_t>;
            IntType int_a, int_b;

#if __cplusplus >= 202002L
            int_a = std::bit_cast<IntType>(a);
            int_b = std::bit_cast<IntType>(b);
#else
            // Strict C++17 compliant type-punning using std::memcpy
            std::memcpy(&int_a, &a, sizeof(T));
            std::memcpy(&int_b, &b, sizeof(T));
#endif

            // Convert negative values using unsigned arithmetic to avoid UB
            if (int_a < 0) {
                if constexpr (sizeof(T) == 4) {
                    int_a = static_cast<int32_t>(0x80000000u - static_cast<uint32_t>(int_a));
                }
                else {
                    int_a = static_cast<int64_t>(0x8000000000000000ull - static_cast<uint64_t>(int_a));
                }
            }
            if (int_b < 0) {
                if constexpr (sizeof(T) == 4) {
                    int_b = static_cast<int32_t>(0x80000000u - static_cast<uint32_t>(int_b));
                }
                else {
                    int_b = static_cast<int64_t>(0x8000000000000000ull - static_cast<uint64_t>(int_b));
                }
            }

            auto diff = std::abs(static_cast<IntType>(int_a - int_b));
            if (diff > static_cast<decltype(diff)>(actualEps)) {
                #ifndef NDEBUG
                LOG_ERROR(std::string("Equality check failed: ") + toString(diff) + " ULPs" );
                #endif
                return false;
            }
            return true;
        }
    };

    /**
     * @brief Relative tolerance comparison policy.
     * * LOGIC ORDER:
     * 1. Check Special Values
     * 2. STRICT SIGN CHECK (Relative error across zero is undefined/infinite)
     * 3. Relative Tolerance Calculation
     */
    struct RelativeComparisonPolicy {
        template <typename T, typename... EpsParams>
        [[nodiscard]] static bool epsilonMatch(T a, T b, EpsParams... eps) {
            static_assert(std::is_floating_point_v<T>, "Policy only for floating-point types");

            if (auto result = detail::handleSpecialValues(a, b)) return *result;

            // Handle signed zeros (+0.0 == -0.0 in IEEE 754)
            if (a == static_cast<T>(0.0) && b == static_cast<T>(0.0)) return true;

            // STRICT SIGN CHECK
            if (detail::areSignsDifferent(a, b)) return false;

            T relEps;
            if constexpr (sizeof...(eps) > 0) {
                relEps = std::get<0>(std::forward_as_tuple(eps...));
            }
            else {
                relEps = getDefaultEpsilon<T>();
            }

            T maxAbs = std::max(std::fabs(a), std::fabs(b));
            if (maxAbs == static_cast<T>(0.0)) return true;

            if (std::fabs(a - b) > relEps * maxAbs) {
                #ifndef NDEBUG
                LOG_ERROR(std::string("Equality check failed: relative error"));
                #endif
                return false;
            }
            return true;
        }
    };

    /**
     * @brief Hybrid (relative and absolute) tolerance comparison policy.
     * * LOGIC ORDER:
     * 1. Check Special Values
     * 2. Check Absolute Tolerance (Priority 1: Handles noise around zero)
     * 3. Check Sign Consistency (Priority 2: Enforces direction for larger values)
     * 4. Check Relative Tolerance (Priority 3: Handles large scaling)
     */
    struct HybridComparisonPolicy {
        template <typename T, typename... EpsParams>
        [[nodiscard]] static bool epsilonMatch(T a, T b, EpsParams... eps) {
            static_assert(std::is_floating_point_v<T>, "Policy only for floating-point types");

            if (auto result = detail::handleSpecialValues(a, b)) return *result;

            auto eps_tuple = std::forward_as_tuple(eps...);
            T relEps, absEps;

            if constexpr (sizeof...(eps) >= 2) {
                relEps = std::get<0>(eps_tuple);
                absEps = std::get<1>(eps_tuple);
            }
            else if constexpr (sizeof...(eps) == 1) {
                relEps = std::get<0>(eps_tuple);
                absEps = relEps;
            }
            else {
                relEps = getDefaultEpsilon<T>();
                absEps = getDefaultEpsilon<T>();
            }

            T diff = std::fabs(a - b);

            // 1. ABSOLUTE CHECK (Priority #1)
            // Allows crossing zero if within noise floor.
            if (diff <= absEps) return true;

            // 2. SIGN CONSISTENCY CHECK (Priority #2)
            // If failed absolute, we must strictly enforce signs before checking relative.
            if (detail::areSignsDifferent(a, b)) return false;

            // 3. RELATIVE CHECK (Priority #3)
            T maxAbs = std::max(std::fabs(a), std::fabs(b));
            if (diff > relEps * maxAbs) {
                #ifndef NDEBUG
                LOG_ERROR(std::string("Equality check failed: Hybrid"));
                #endif
                return false;
            }
            return true;
        }
    };

    // ====================================================================
    // Convenience Functions
    // ====================================================================

    template <typename T>
    [[nodiscard]]
    std::enable_if_t<std::is_floating_point_v<T>, bool>
        approximateEqual(const T& a, const T& b, T relEps = getDefaultEpsilon<T>(), T absEps = getDefaultEpsilon<T>()) {
        return HybridComparisonPolicy::epsilonMatch(a, b, relEps, absEps);
    }

    template <typename T, typename Policy = StandardComparisonPolicy, typename... EpsParams>
    [[nodiscard]]
    std::enable_if_t<std::is_floating_point_v<T>, bool>
        floatEqual(const T& a, const T& b, EpsParams... eps) {
        return Policy::epsilonMatch(a, b, eps...);
    }

} // namespace fat_p