/**
 * @file CheckedArithmetic.h
 * @brief Unified checked arithmetic operations for integers and floating-point
 * @version 3.1 (Fixed static_math::mul for unsigned types)
 *
 * @details This is the umbrella header that provides the complete checked
 * arithmetic API. It includes both integer and floating-point operations
 * plus mixed-type operations that work on both:
 *
 * From CheckedArithmeticInt.h:
 * - checked_add, checked_sub, checked_mul, checked_div, checked_mod (integers)
 * - Bitwise operations, pointer arithmetic
 * - SIMD-accelerated integer vector ops (AVX2, SSE2, NEON)
 *
 * From CheckedArithmeticFP.h:
 * - checked_add_fp, checked_sub_fp, checked_mul_fp, checked_div_fp
 * - checked_sqrt_fp, checked_floor_fp, etc.
 * - SIMD-accelerated FP vector ops via SimdVector
 *
 * Mixed operations (defined in this file):
 * - checked_clamp: Clamp value to range (int and FP)
 * - checked_in_range: Test if value is in range (int and FP)
 * - checked_cast: Safe type conversion with overflow detection
 * - static_checked_cast: Compile-time checked cast
 *
 * Version History:
 * - 3.1: Fixed static_math::mul for unsigned types
 * - 3.0: Split architecture (Base, Policies, Int, FP, Umbrella)
 * - 2.2: SIMD multiplication via wide-multiply
 * - 2.1: Modular integer SIMD (SSE2, AVX2, NEON)
 * - 2.0: SimdVector integration for FP
 * - 1.x: Initial checked arithmetic
 *
 * @note For minimal includes, use specific headers:
 *   - CheckedArithmeticInt.h for integer-only (no SimdVector dependency)
 *   - CheckedArithmeticFP.h for floating-point-only
 *
 * @performance
 * - Builtin path (GCC/Clang): ~2-5 ns per scalar op
 * - SIMD vectors (int32, AVX2): ~0.5-1 ns per element
 * - SIMD vectors (float/double): ~0.5-1 ns per element
 */
#pragma once

/*
FATP_META:
  meta_version: 1
  component: CheckedArithmetic
  file_role: public_header
  path: fat_p/CheckedArithmetic.h
  namespace: fat_p
  summary: "Public header for CheckedArithmetic."
  api_stability: in_work
  related:
    docs_search: "CheckedArithmetic"
    tests:
      - tests/test_CheckedArithmetic.cpp
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 1
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/
// Include the split components
#include "CheckedArithmeticInt.h"
#include "CheckedArithmeticFP.h"

#include <algorithm>  // for std::swap
#include <cstdint>    // Explicit include for INT32_MAX etc.

namespace fat_p {

// =============================================================================
// Mixed Operations (Integer + Floating-Point)
// =============================================================================

/**
 * @brief Checked clamping for any arithmetic type
 *
 * Clamps value to [min_val, max_val] range with validation.
 * Works for both integers and floating-point types.
 *
 * @param value Value to clamp
 * @param min_val Minimum of range
 * @param max_val Maximum of range
 * @return Clamped value according to policy
 *
 * Detects:
 * - Invalid range (min > max)
 * - NaN inputs (for floating-point)
 */
template <typename Policy = ThrowOnErrorPolicy, typename T>
[[nodiscard]] constexpr PolicyReturnType<Policy, T> checked_clamp(T value, T min_val, T max_val)
    noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    static_assert(std::is_arithmetic_v<T>, "checked_clamp requires arithmetic types");

    if (min_val > max_val)
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            detail::checked_arithmetic_fail(FATP_LOCUS, "Invalid range: min > max");
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<T, MathError>(unexpect, MathError::InvalidArgument);
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy> ||
                          std::is_same_v<Policy, InfTolerantPolicy>)
        {
            std::swap(min_val, max_val);
        }
    }

    if constexpr (std::is_floating_point_v<T>)
    {
        if (std::isnan(value) || std::isnan(min_val) || std::isnan(max_val))
        {
            if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
            {
                detail::checked_arithmetic_fail(FATP_LOCUS, "NaN in clamp");
            }
            else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
            {
                return Expected<T, MathError>(unexpect, MathError::NaN);
            }
            else if constexpr (std::is_same_v<Policy, SaturatingPolicy> ||
                              std::is_same_v<Policy, InfTolerantPolicy>)
            {
                return std::numeric_limits<T>::quiet_NaN();
            }
        }
    }

    T result = (value < min_val) ? min_val : (value > max_val) ? max_val : value;

    return result;
}

/**
 * @brief Checked range test for any arithmetic type
 *
 * Tests if value is in [min_val, max_val] range with validation.
 *
 * @param value Value to test
 * @param min_val Minimum of range
 * @param max_val Maximum of range
 * @return true if in range, or error according to policy
 */
template <typename Policy = ReturnExpectedPolicy, typename T>
[[nodiscard]] constexpr PolicyReturnType<Policy, bool> checked_in_range(T value, T min_val, T max_val)
    noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    static_assert(std::is_arithmetic_v<T>, "checked_in_range requires arithmetic types");

    if (min_val > max_val)
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            detail::checked_arithmetic_fail(FATP_LOCUS, "Invalid range: min > max");
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<bool, MathError>(unexpect, MathError::InvalidArgument);
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy> ||
                          std::is_same_v<Policy, InfTolerantPolicy>)
        {
            std::swap(min_val, max_val);
        }
    }

    if constexpr (std::is_floating_point_v<T>)
    {
        if (std::isnan(value) || std::isnan(min_val) || std::isnan(max_val))
        {
            if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
            {
                detail::checked_arithmetic_fail(FATP_LOCUS, "NaN in range check");
            }
            else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
            {
                return Expected<bool, MathError>(unexpect, MathError::NaN);
            }
            else if constexpr (std::is_same_v<Policy, SaturatingPolicy> ||
                              std::is_same_v<Policy, InfTolerantPolicy>)
            {
                return false;
            }
        }
    }

    bool result = (value >= min_val) && (value <= max_val);

    return result;
}

// =============================================================================
// Type Conversion with Overflow Detection
// =============================================================================

namespace detail {

/**
 * @brief Helper to detect if a cast would overflow
 */
template <typename To, typename From>
struct CastOverflowCheck
{
    using ToLimits = std::numeric_limits<To>;

    static constexpr bool would_overflow(From value) noexcept
    {
        if constexpr (std::is_floating_point_v<From> && std::is_integral_v<To>)
        {
            // FP to integer: check truncated value fits
            if (std::isnan(value) || std::isinf(value))
            {
                return true;
            }
            return value < static_cast<From>(ToLimits::lowest()) ||
                   value > static_cast<From>(ToLimits::max());
        }
        else if constexpr (std::is_integral_v<From> && std::is_floating_point_v<To>)
        {
            // Integer to FP: always fits (may lose precision but not overflow)
            return false;
        }
        else if constexpr (std::is_signed_v<From> == std::is_signed_v<To>)
        {
            // Same signedness
            if constexpr (sizeof(From) <= sizeof(To))
            {
                return false;  // Widening
            }
            else
            {
                // Narrowing
                return value < static_cast<From>(ToLimits::lowest()) ||
                       value > static_cast<From>(ToLimits::max());
            }
        }
        else if constexpr (std::is_signed_v<From> && !std::is_signed_v<To>)
        {
            // Signed to unsigned
            if (value < 0)
            {
                return true;
            }
            using UnsignedFrom = std::make_unsigned_t<From>;
            return static_cast<UnsignedFrom>(value) > ToLimits::max();
        }
        else
        {
            // Unsigned to signed
            if constexpr (sizeof(From) < sizeof(To))
            {
                return false;  // Always fits
            }
            else
            {
                return value > static_cast<From>(ToLimits::max());
            }
        }
    }
};

} // namespace detail

/**
 * @brief Runtime checked type conversion
 *
 * Converts value to target type with overflow detection.
 * Works for numeric types (int, float, etc.).
 *
 * @tparam To Target type
 * @tparam Policy Error handling policy
 * @tparam From Source type (auto-deduced)
 * @param value Value to convert
 * @return Converted value according to policy
 *
 * Detects:
 * - Overflow (value too large for target)
 * - Underflow (negative value to unsigned)
 * - NaN/Inf (for floating-point sources)
 */
template <typename To, typename Policy = ThrowOnErrorPolicy, typename From>
[[nodiscard]] constexpr PolicyReturnType<Policy, To> checked_cast(From value)
    noexcept(PolicyTraits<Policy>::template is_noexcept<To>)
{
    static_assert(std::is_arithmetic_v<From>, "checked_cast requires arithmetic source");
    static_assert(std::is_arithmetic_v<To>, "checked_cast requires arithmetic target");
    static_assert(!std::is_same_v<To, bool>, "checked_cast does not support bool target");

    // Same type: no check needed
    if constexpr (std::is_same_v<To, From>)
    {
        return static_cast<To>(value);
    }

    // FP source: check for NaN/Inf before numeric overflow
    if constexpr (std::is_floating_point_v<From>)
    {
        if (std::isnan(value))
        {
            if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
            {
                detail::checked_arithmetic_fail(FATP_LOCUS, "checked_cast: NaN cannot be converted");
            }
            else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
            {
                return Expected<To, MathError>(unexpect, MathError::NaN);
            }
            else if constexpr (std::is_same_v<Policy, SaturatingPolicy> ||
                              std::is_same_v<Policy, InfTolerantPolicy>)
            {
                if constexpr (std::is_floating_point_v<To>)
                {
                    return std::numeric_limits<To>::quiet_NaN();
                }
                else
                {
                    return To{0};
                }
            }
        }

        if (std::isinf(value) && std::is_integral_v<To>)
        {
            if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
            {
                detail::checked_arithmetic_fail(FATP_LOCUS, "checked_cast: Inf cannot be converted to integer");
            }
            else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
            {
                return Expected<To, MathError>(unexpect, MathError::Inf);
            }
            else if constexpr (std::is_same_v<Policy, SaturatingPolicy> ||
                              std::is_same_v<Policy, InfTolerantPolicy>)
            {
                return (value > 0) ? std::numeric_limits<To>::max() :
                                     std::numeric_limits<To>::lowest();
            }
        }
    }

    // Check for overflow
    bool overflow = detail::CastOverflowCheck<To, From>::would_overflow(value);

    if (overflow)
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            detail::checked_arithmetic_fail(FATP_LOCUS, "checked_cast overflow");
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            if constexpr (std::is_signed_v<From> && !std::is_floating_point_v<From>)
            {
                if (value < 0)
                {
                    return Expected<To, MathError>(unexpect, MathError::Underflow);
                }
            }
            return Expected<To, MathError>(unexpect, MathError::Overflow);
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy> ||
                          std::is_same_v<Policy, InfTolerantPolicy>)
        {
            if constexpr (std::is_signed_v<From> && !std::is_floating_point_v<From>)
            {
                if (value < 0)
                {
                    return std::numeric_limits<To>::lowest();
                }
            }
            return std::numeric_limits<To>::max();
        }
    }

    return static_cast<To>(value);
}

/**
 * @brief Compile-time checked cast using static_assert
 *
 * Fails to compile if the conversion would overflow.
 *
 * @tparam To Target type
 * @tparam From Source type
 * @tparam value Value to convert (must be constexpr)
 * @return Converted value
 *
 * @example
 *   constexpr int8_t x = static_checked_cast<int8_t, int, 100>();  // OK
 *   constexpr int8_t y = static_checked_cast<int8_t, int, 200>();  // Compile error!
 */
template <typename To, typename From, From value>
constexpr To static_checked_cast()
{
    static_assert(std::is_arithmetic_v<From>, "static_checked_cast requires arithmetic source");
    static_assert(std::is_arithmetic_v<To>, "static_checked_cast requires arithmetic target");
    static_assert(!detail::CastOverflowCheck<To, From>::would_overflow(value),
                  "static_checked_cast: value would overflow target type");
    return static_cast<To>(value);
}

// =============================================================================
// AVX2 Availability Flag
// =============================================================================

#ifdef __AVX2__
constexpr bool has_avx2 = true;
#else
constexpr bool has_avx2 = false;
#endif

} // namespace fat_p

// =============================================================================
// Compile-Time Checked Arithmetic (static_math namespace)
// =============================================================================

namespace static_math {

/**
 * @brief Compile-time checked addition
 * @note Fails to compile if overflow would occur
 */
template <typename T, T a, T b>
constexpr T add()
{
    static_assert(b >= 0 ? a <= std::numeric_limits<T>::max() - b :
                            a >= std::numeric_limits<T>::min() - b,
                  "Overflow in static_math::add");
    return a + b;
}

/**
 * @brief Compile-time checked subtraction
 */
template <typename T, T a, T b>
constexpr T sub()
{
    static_assert(b >= 0 ? a >= std::numeric_limits<T>::min() + b :
                            a <= std::numeric_limits<T>::max() + b,
                  "Overflow in static_math::sub");
    return a - b;
}

/**
 * @brief Compile-time checked multiplication
 *
 * Properly handles both signed and unsigned types.
 * For unsigned types, only positive overflow is possible.
 */
template <typename T, T a, T b>
constexpr T mul()
{
    if constexpr (std::is_unsigned_v<T>)
    {
        // Unsigned: only need to check a * b <= max
        static_assert(a == 0 || b == 0 || a <= std::numeric_limits<T>::max() / b,
                      "Overflow in static_math::mul");
    }
    else
    {
        // Signed: need to handle all four sign combinations
        static_assert(a == 0 || b == 0 ||
            (a > 0 && b > 0 ? a <= std::numeric_limits<T>::max() / b :
             a < 0 && b < 0 ? a >= std::numeric_limits<T>::max() / b :
             (a > 0 ? b >= std::numeric_limits<T>::min() / a :
                      a >= std::numeric_limits<T>::min() / b)),
                      "Overflow in static_math::mul");
    }
    return a * b;
}

/**
 * @brief Compile-time checked division
 */
template <typename T, T a, T b>
constexpr T div()
{
    static_assert(b != 0, "Division by zero in static_math::div");
    if constexpr (std::is_signed_v<T>)
    {
        static_assert(!(a == std::numeric_limits<T>::min() && b == static_cast<T>(-1)),
                      "Overflow in static_math::div (min / -1)");
    }
    return a / b;
}

/**
 * @brief Compile-time checked modulo
 */
template <typename T, T a, T b>
constexpr T mod()
{
    static_assert(b != 0, "Modulo by zero in static_math::mod");
    if constexpr (std::is_signed_v<T>)
    {
        static_assert(!(a == std::numeric_limits<T>::min() && b == static_cast<T>(-1)),
                      "Overflow in static_math::mod (min % -1)");
    }
    return a % b;
}

/**
 * @brief Compile-time checked left shift
 */
template <typename T, T a, int shift>
constexpr T left_shift()
{
    static_assert(shift >= 0 && shift < static_cast<int>(sizeof(T) * 8),
                  "Invalid shift amount in static_math::left_shift");
    return a << shift;
}

/**
 * @brief Compile-time checked right shift
 */
template <typename T, T a, int shift>
constexpr T right_shift()
{
    static_assert(shift >= 0 && shift < static_cast<int>(sizeof(T) * 8),
                  "Invalid shift amount in static_math::right_shift");
    return a >> shift;
}

template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
constexpr T and_op(T a, T b)
{
    return a & b;
}

template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
constexpr T or_op(T a, T b)
{
    return a | b;
}

template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
constexpr T xor_op(T a, T b)
{
    return a ^ b;
}

} // namespace static_math

// =============================================================================
// Macro Cleanup
// =============================================================================

// Note: HAS_BUILTIN_OVERFLOW is defined in Policies.h and used by Int.h
// We clean it up here at the umbrella level
#undef HAS_BUILTIN_OVERFLOW
