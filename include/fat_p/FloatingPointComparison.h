#pragma once

/*
FATP_META:
  meta_version: 1
  component: FloatingPointComparison
  file_role: public_header
  path: include/fat_p/FloatingPointComparison.h
  namespace: fat_p
  layer: Foundation
  summary: "Public header for FloatingPointComparison."
  api_stability: in_work
  related:
    docs_search: "FloatingPointComparison"
    tests:
      - components/FloatingPointComparison/tests/test_FloatingPointComparison.cpp
    benchmarks:
      - components/FloatingPointComparison/benchmarks/benchmark_FloatingPointComparison.cpp
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 2
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/

/**
 * @file FloatingPointComparison.h
 * @brief Robust floating-point comparison utilities.
 *
 * Provides policy-based floating-point comparison with support for absolute,
 * relative, ULP, and hybrid tolerance strategies. Handles special cases
 * (NaN, infinity, signed zero) correctly.
 *
 * CHANGE LOG:
 * - Added <cstdlib> for std::abs integer support.
 * - OPTIMIZED LOGIC (Standard/Hybrid): Absolute Tolerance check now runs BEFORE
 *   Sign Consistency check. This creates a stable "noise floor" around zero,
 *   treating +noise and -noise as equal if within absolute epsilon.
 * - UlpComparisonPolicy and RelativeComparisonPolicy remain strict regarding signs.
 */

#include "CppFeatureDetection.h"

#include <algorithm> // For std::max
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib> // For std::abs (integer)
#include <cstring> // For std::memcpy (C++17 type-punning)
#include <limits>
#include <optional> // For std::optional in detail::handleSpecialValues
#include <string>
#include <tuple> // For std::forward_as_tuple, std::get
#include <type_traits>

// For C++20 support for type-punning without UB
#if FATP_CPP20_OR_LATER
#include <bit>
#define FATP_FPC_HAS_BIT_CAST 1
#else
#define FATP_FPC_HAS_BIT_CAST 0
#endif

#include "ComparisonTolerances.h"
#include "DiagnosticLogger_Core.h"
#include "Stringify.h"

namespace fat_p
{

// ====================================================================
// Helper Functions
// ====================================================================

/**
 * @brief Returns the default comparison epsilon for a floating-point type.
 *
 * Default is approximately 100× machine epsilon, suitable for typical
 * accumulated floating-point error from multiple operations.
 *
 * @tparam T Floating-point type (float, double, long double)
 * @return Type-appropriate default epsilon:
 *         - float:  ~1.19e-5 (100× FLT_EPSILON)
 *         - double: ~2.22e-14 (100× DBL_EPSILON)
 *         - long double: 100× LDBL_EPSILON
 *
 * @note Complexity: O(1), compile-time evaluation (constexpr)
 * @note Thread-safety: Thread-safe (stateless, no side effects)
 */
template <typename T>
constexpr T getDefaultEpsilon()
{
    static_assert(std::is_floating_point_v<T>, "T must be a floating-point type.");
    if constexpr (std::is_same_v<T, float>)
    {
        return static_cast<T>(kDefaultFloatEpsilon);
    }
    else if constexpr (std::is_same_v<T, double>)
    {
        return static_cast<T>(kDefaultDoubleEpsilon);
    }
    else
    {
        return static_cast<T>(std::numeric_limits<long double>::epsilon() * 100.0L);
    }
}

namespace detail
{
/**
 * @brief Handles IEEE 754 special values (NaN, infinity).
 * @return std::nullopt if values are normal, bool result if special case handled.
 * @note noexcept: only performs simple FP classifications and comparisons.
 */
template <typename T>
std::optional<bool> handleSpecialValues(T a, T b) noexcept
{
    // NaN is never equal to anything
    if (std::isnan(a) || std::isnan(b))
    {
        return false;
    }
    // Same-sign infinities are equal
    if (std::isinf(a) && std::isinf(b))
    {
        return (a > 0) == (b > 0);
    }
    // One infinity, one finite: not equal
    if (std::isinf(a) || std::isinf(b))
    {
        return false;
    }
    return std::nullopt;
}

/**
 * @brief Checks if signs differ.
 * Note: This returns true for +0.0 and -0.0, but that case is usually
 * caught by equality checks or absolute tolerance checks before this is called.
 * @note noexcept: only accesses the sign bit.
 */
template <typename T>
bool areSignsDifferent(T a, T b) noexcept
{
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
struct StandardComparisonPolicy
{
    template <typename T, typename... EpsParams>
    [[nodiscard]] static bool epsilonMatch(T a, T b, EpsParams... eps)
    {
        static_assert(std::is_floating_point_v<T>, "Policy only for floating-point types");
        static_assert(sizeof...(EpsParams) <= 1,
                      "StandardComparisonPolicy accepts at most one epsilon parameter (absolute tolerance).");

        if (auto result = detail::handleSpecialValues(a, b))
        {
            return *result;
        }

        T actualEps;
        if constexpr (sizeof...(eps) > 0)
        {
            actualEps = static_cast<T>(std::get<0>(std::forward_as_tuple(eps...)));
        }
        else
        {
            actualEps = getDefaultEpsilon<T>();
        }

        // 1. ABSOLUTE TOLERANCE CHECK (Priority #1)
        // Allows crossing zero if within noise floor.
        // NOTE: Inclusive boundary (<=): values exactly epsilon apart ARE considered equal.
        // This matches common library behavior (Google Test EXPECT_NEAR, etc.) and avoids
        // surprising boundary failures from floating-point representation error.
        if (std::fabs(a - b) <= actualEps)
        {
            return true;
        }

        // 2. SIGN CONSISTENCY CHECK (Priority #2)
        // If outside noise floor, opposite signs are never equal.
        if (detail::areSignsDifferent(a, b))
        {
            return false;
        }

// If we get here: Signs are same, but difference > epsilon.
#ifndef NDEBUG
        FATP_LOG_ERROR(std::string("Equality check failed: ") + toString(std::fabs(a - b)) + " > " +
                       toString(actualEps));
#endif
        return false;
    }
};

/**
 * @brief Units in Last Place (ULP) comparison policy.
 *
 * Compares the bit-level distance between floating-point representations.
 * This is the most precise comparison method, measuring how many representable
 * floating-point values exist between two numbers.
 *
 * @note ULP tolerance is an integer count. Floating-point values are
 *       truncated (e.g., 4.9 → 4 ULPs). Default: 4 ULPs.
 * @note Sign-strict: opposite signs always compare unequal (ULP distance
 *       across zero is not meaningful due to IEEE 754 representation).
 * @note Subnormal fallback: uses absolute tolerance (1e-6f for float,
 *       1e-12 for double) since ULP distance is unreliable for subnormals.
 * @note Only supports float (4 bytes) and double (8 bytes), not long double.
 *
 * LOGIC ORDER:
 * 1. Check Special Values
 * 2. Exact Equality Optimization
 * 3. STRICT SIGN CHECK (ULP distance across zero is not linear)
 * 4. Subnormal Fallback
 * 5. ULP Calculation (using unsigned ordered-space arithmetic)
 */
struct UlpComparisonPolicy
{
    template <typename T, typename... EpsParams>
    [[nodiscard]] static bool epsilonMatch(T a, T b, EpsParams... eps)
    {
        static_assert(std::is_floating_point_v<T>, "Policy only for floating-point types");
        static_assert(sizeof...(EpsParams) <= 1,
                      "UlpComparisonPolicy accepts at most one epsilon parameter (max ULP count).");
        static_assert(sizeof(T) == 4 || sizeof(T) == 8,
                      "ULP comparison only supports float (4 bytes) and double (8 bytes).");

        if (auto result = detail::handleSpecialValues(a, b))
        {
            return *result;
        }

        // Optimization: exact match
        if (a == b)
        {
            return true;
        }

        // STRICT SIGN CHECK
        // ULP distance is meaningless across zero (jumping from +0 to -0 involves flipping the sign bit, huge integer
        // diff).
        if (detail::areSignsDifferent(a, b))
        {
            return false;
        }

        T actualEps = static_cast<T>(4.0);
        if constexpr (sizeof...(eps) > 0)
        {
            actualEps = static_cast<T>(std::get<0>(std::forward_as_tuple(eps...)));
        }

        // Validate epsilon: must be finite and non-negative
        // Fractional values are truncated to integer ULP count
        if (!std::isfinite(actualEps) || actualEps < static_cast<T>(0.0))
        {
#ifndef NDEBUG
            FATP_LOG_ERROR("ULP epsilon must be finite and non-negative.");
#endif
            return false;
        }

        // Subnormal handling: ULP distance unreliable, use absolute tolerance
        constexpr T AbsSubnormalTolerance = [] {
            if constexpr (std::is_same_v<T, float>)
            {
                return static_cast<T>(1.0e-6f);
            }
            else
            {
                return static_cast<T>(1.0e-12);
            }
        }();

        if (std::fpclassify(a) == FP_SUBNORMAL || std::fpclassify(b) == FP_SUBNORMAL)
        {
            return std::fabs(a - b) <= AbsSubnormalTolerance;
        }

        // Use unsigned type for bit manipulation (cleaner, no signed overflow concerns)
        using BitsType = std::conditional_t<sizeof(T) == 4, uint32_t, uint64_t>;
        BitsType bits_a = 0, bits_b = 0;

#if FATP_FPC_HAS_BIT_CAST
        bits_a = std::bit_cast<BitsType>(a);
        bits_b = std::bit_cast<BitsType>(b);
#else
        // Strict C++17 compliant type-punning using std::memcpy
        std::memcpy(&bits_a, &a, sizeof(T));
        std::memcpy(&bits_b, &b, sizeof(T));
#endif

        // Map IEEE 754 representation to ordered unsigned space:
        // - Positive floats: set sign bit (maps 0x00... to 0x80...)
        // - Negative floats: invert all bits (maps 0xFF... to 0x00...)
        // This creates a linear ordering where subtraction gives ULP distance
        auto to_ordered = [](BitsType bits) -> BitsType {
            constexpr BitsType sign_mask = BitsType(1) << (sizeof(BitsType) * 8 - 1);
            return (bits & sign_mask) ? ~bits : (bits | sign_mask);
        };

        const BitsType ordered_a = to_ordered(bits_a);
        const BitsType ordered_b = to_ordered(bits_b);

        // Safe unsigned subtraction (no overflow possible)
        const BitsType ulp_diff = (ordered_a > ordered_b) ? (ordered_a - ordered_b) : (ordered_b - ordered_a);

        // Truncate floating-point epsilon to integer ULP count
        const BitsType max_ulps = static_cast<BitsType>(actualEps);

        if (ulp_diff > max_ulps)
        {
#ifndef NDEBUG
            FATP_LOG_ERROR(std::string("Equality check failed: ") + toString(ulp_diff) + " ULPs");
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
struct RelativeComparisonPolicy
{
    template <typename T, typename... EpsParams>
    [[nodiscard]] static bool epsilonMatch(T a, T b, EpsParams... eps)
    {
        static_assert(std::is_floating_point_v<T>, "Policy only for floating-point types");
        static_assert(sizeof...(EpsParams) <= 1,
                      "RelativeComparisonPolicy accepts at most one epsilon parameter (relative tolerance).");

        if (auto result = detail::handleSpecialValues(a, b))
        {
            return *result;
        }

        // Handle signed zeros (+0.0 == -0.0 in IEEE 754)
        if (a == static_cast<T>(0.0) && b == static_cast<T>(0.0))
        {
            return true;
        }

        // STRICT SIGN CHECK
        if (detail::areSignsDifferent(a, b))
        {
            return false;
        }

        T relEps;
        if constexpr (sizeof...(eps) > 0)
        {
            relEps = static_cast<T>(std::get<0>(std::forward_as_tuple(eps...)));
        }
        else
        {
            relEps = getDefaultEpsilon<T>();
        }

        T maxAbs = std::max(std::fabs(a), std::fabs(b));
        if (maxAbs == static_cast<T>(0.0))
        {
            return true;
        }

        if (std::fabs(a - b) > relEps * maxAbs)
        {
#ifndef NDEBUG
            FATP_LOG_ERROR(std::string("Equality check failed: relative error"));
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
struct HybridComparisonPolicy
{
    template <typename T, typename... EpsParams>
    [[nodiscard]] static bool epsilonMatch(T a, T b, EpsParams... eps)
    {
        static_assert(std::is_floating_point_v<T>, "Policy only for floating-point types");
        static_assert(sizeof...(EpsParams) <= 2,
                      "HybridComparisonPolicy accepts at most two epsilon parameters (relative, absolute).");

        if (auto result = detail::handleSpecialValues(a, b))
        {
            return *result;
        }

        auto eps_tuple = std::forward_as_tuple(eps...);
        T relEps, absEps;

        if constexpr (sizeof...(eps) >= 2)
        {
            relEps = static_cast<T>(std::get<0>(eps_tuple));
            absEps = static_cast<T>(std::get<1>(eps_tuple));
        }
        else if constexpr (sizeof...(eps) == 1)
        {
            relEps = static_cast<T>(std::get<0>(eps_tuple));
            absEps = relEps;
        }
        else
        {
            relEps = getDefaultEpsilon<T>();
            absEps = getDefaultEpsilon<T>();
        }

        T diff = std::fabs(a - b);

        // 1. ABSOLUTE CHECK (Priority #1)
        // Allows crossing zero if within noise floor.
        if (diff <= absEps)
        {
            return true;
        }

        // 2. SIGN CONSISTENCY CHECK (Priority #2)
        // If failed absolute, we must strictly enforce signs before checking relative.
        if (detail::areSignsDifferent(a, b))
        {
            return false;
        }

        // 3. RELATIVE CHECK (Priority #3)
        T maxAbs = std::max(std::fabs(a), std::fabs(b));
        if (diff > relEps * maxAbs)
        {
#ifndef NDEBUG
            FATP_LOG_ERROR(std::string("Equality check failed: Hybrid"));
#endif
            return false;
        }
        return true;
    }
};

// ====================================================================
// Convenience Functions
// ====================================================================

/**
 * @brief Compares floating-point values using hybrid tolerance (recommended default).
 *
 * This is the recommended comparison function for most applications. It uses
 * absolute tolerance first (noise floor around zero), then relative tolerance
 * for larger values, handling the full floating-point range correctly.
 *
 * @tparam T Floating-point type (float, double, long double)
 * @param a First value to compare
 * @param b Second value to compare
 * @param relEps Relative tolerance (default: ~100× machine epsilon)
 * @param absEps Absolute tolerance / noise floor (default: same as relEps)
 * @return true if the values are approximately equal within tolerance
 *
 * @note Complexity: O(1)
 * @note Thread-safety: Thread-safe (stateless, no side effects)
 *
 * @code
 * // Basic usage with defaults
 * fat_p::approximateEqual(0.1 + 0.2, 0.3);  // true
 *
 * // Custom tolerances
 * fat_p::approximateEqual(a, b, 1e-9, 1e-12);
 * @endcode
 */
template <typename T>
[[nodiscard]]
std::enable_if_t<std::is_floating_point_v<T>, bool>
approximateEqual(const T& a, const T& b, T relEps = getDefaultEpsilon<T>(), T absEps = getDefaultEpsilon<T>())
{
    return HybridComparisonPolicy::epsilonMatch(a, b, relEps, absEps);
}

/**
 * @brief Compares floating-point values using a compile-time selected policy.
 *
 * Allows explicit selection of comparison strategy for domain-specific needs.
 *
 * @tparam T Floating-point type (float, double, long double)
 * @tparam Policy Comparison policy (default: StandardComparisonPolicy)
 *         - StandardComparisonPolicy: absolute tolerance
 *         - RelativeComparisonPolicy: relative tolerance
 *         - UlpComparisonPolicy: bit-level ULP distance
 *         - HybridComparisonPolicy: combined absolute + relative
 * @tparam EpsParams Variadic epsilon parameter types
 * @param a First value to compare
 * @param b Second value to compare
 * @param eps Policy-specific tolerance parameter(s):
 *         - Standard: 1 param (absolute epsilon)
 *         - Relative: 1 param (relative epsilon)
 *         - ULP: 1 param (max ULP count, truncated to integer)
 *         - Hybrid: 1-2 params (relative epsilon, absolute epsilon)
 * @return true if equal according to the selected policy
 *
 * @note Complexity: O(1)
 * @note Thread-safety: Thread-safe (stateless, no side effects)
 *
 * @code
 * // Absolute tolerance
 * fat_p::floatEqual<double, fat_p::StandardComparisonPolicy>(a, b, 1e-9);
 *
 * // ULP comparison (bit-level)
 * fat_p::floatEqual<float, fat_p::UlpComparisonPolicy>(a, b, 4.0f);
 * @endcode
 */
template <typename T, typename Policy = StandardComparisonPolicy, typename... EpsParams>
[[nodiscard]]
std::enable_if_t<std::is_floating_point_v<T>, bool> floatEqual(const T& a, const T& b, EpsParams... eps)
{
    return Policy::epsilonMatch(a, b, eps...);
}

} // namespace fat_p