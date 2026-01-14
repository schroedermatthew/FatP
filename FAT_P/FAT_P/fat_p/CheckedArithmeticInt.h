/**
 * @file CheckedArithmeticInt.h
 * @brief Checked arithmetic operations for integer types
 * @layer Foundation
 *
 * This header provides overflow-checked arithmetic operations for integers:
 *
 * Scalar Operations:
 * - checked_add, checked_sub, checked_mul, checked_div, checked_mod
 * - checked_negate, checked_abs, checked_inc, checked_dec
 *
 * Bitwise Operations:
 * - checked_and, checked_or, checked_xor
 * - checked_left_shift, checked_right_shift (with UB prevention)
 *
 * Pointer Arithmetic:
 * - checked_add(ptr, offset), checked_sub(ptr, offset)
 * - checked_inc(ptr), checked_dec(ptr)
 *
 * Vector Operations (SIMD-accelerated):
 * - checked_add_vec, checked_sub_vec, checked_mul_vec
 * - Accelerated on: AVX2, SSE2, NEON
 * - Supported types: int32_t, uint32_t, int64_t
 *
 * SIMD Architecture Support:
 * - AVX2: 8-wide int32, 4-wide int64
 * - SSE2: 4-wide int32, 2-wide int64
 * - NEON: 4-wide int32, 2-wide int64 (AArch64)
 * - SaturatingPolicy on NEON: Zero overhead via hardware vqadd/vqsub
 *
 * Why int8/int16 SIMD is NOT implemented:
 * - Wide-multiply explosion (int8*int8->int16 requires unpack/repack)
 * - No _mm256_mul_epi8 intrinsic exists
 * - C++ promotes int8*int8 to int (overflow on narrowing, not operation)
 * - Image/audio usually wants saturation, not error detection
 *
 * Dependency: CheckedArithmeticPolicies.h
 *
 * Part of the CheckedArithmetic split architecture:
 *   CheckedArithmeticBase.h     <- Foundation
 *   CheckedArithmeticPolicies.h <- Policies
 *   CheckedArithmeticInt.h      <- This file (integer ops)
 *   CheckedArithmeticFP.h       <- Floating-point ops
 *   CheckedArithmetic.h         <- Umbrella
 */
#pragma once

/*
FATP_META:
  meta_version: 1
  component: CheckedArithmeticInt
  file_role: public_header
  path: fat_p/CheckedArithmeticInt.h
  namespace: fat_p
  layer: Foundation
  summary: "Public header for CheckedArithmeticInt."
  api_stability: in_work
  related:
    docs_search: "CheckedArithmeticInt"
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
#include "CheckedArithmeticPolicies.h"
#include "enforce.h"

#include <algorithm>   // std::swap
#include <cstddef>     // std::ptrdiff_t
#include <cstdint>     // std::int32_t, etc.
#include <limits>      // std::numeric_limits
#include <type_traits> // std::is_signed_v, etc.
#include <utility>     // std::forward
#include <vector>

// Integer SIMD acceleration (SSE2, AVX2, NEON)
// Note: SIMD intrinsics headers are included by CheckedArithmetic_IntSimd_Common.h
#include "CheckedArithmetic_IntSimd.h"

namespace fat_p
{

// =============================================================================
// Internal Helper: Non-constexpr Error Path
// =============================================================================
// Rationale: C++17 constexpr functions may not contain non-literal locals.
// FATP_ALWAYS_ENFORCE() constructs an RAII enforcer (non-literal type), so we route
// the failure path through a non-constexpr helper while preserving the caller's
// FATP_LOCUS for accurate error reporting.
//
// This helper is intentionally [[noreturn]] to enable dead-code elimination
// and prevent "control reaches end of non-void function" warnings.

namespace detail
{

template <typename... Msgs>
inline void checked_arithmetic_fail(const char* locus, Msgs&&... msgs)
{
    auto enforcer = ::fat_p::enforce_policy_impl<::fat_p::AlwaysEnforcePolicy>(false, "false", locus);
    enforcer(std::forward<Msgs>(msgs)...);
    // RAII destructor throws on scope exit
}

} // namespace detail

// =============================================================================
// Scalar Integer Operations
// =============================================================================

/**
 * @brief Checked addition for integers with overflow detection
 *
 * @tparam Policy Error handling policy (default: ThrowOnErrorPolicy)
 * @tparam T Integral type (auto-deduced)
 * @param a First operand
 * @param b Second operand
 * @return Sum according to policy, or error if overflow
 *
 * Overflow detection:
 * - GCC/Clang: Uses __builtin_add_overflow (optimal codegen)
 * - Fallback: Manual bounds checking
 *
 * @example
 *   auto sum = checked_add(100, 200);  // Throws on overflow
 *   auto safe = checked_add<ReturnExpectedPolicy>(a, b);  // Returns Expected
 */
template <typename Policy = ThrowOnErrorPolicy, FATP_ENABLE_IF_INTEGRAL>
[[nodiscard]] constexpr PolicyReturnType<Policy, T>
checked_add(T a, T b) noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
#if FATP_HAS_BUILTIN_OVERFLOW
    T result;
    if (__builtin_add_overflow(a, b, &result))
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            detail::checked_arithmetic_fail(FATP_LOCUS, "Addition overflow:", a, "+", b);
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            if constexpr (std::is_signed_v<T>)
            {
                MathError code = (b > 0) ? MathError::Overflow : MathError::Underflow;
                return Expected<T, MathError>(unexpect, code);
            }
            else
            {
                return Expected<T, MathError>(unexpect, MathError::Overflow);
            }
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy> || std::is_same_v<Policy, InfTolerantPolicy>)
        {
            if constexpr (std::is_signed_v<T>)
            {
                return (b > 0) ? std::numeric_limits<T>::max() : std::numeric_limits<T>::lowest();
            }
            else
            {
                return std::numeric_limits<T>::max();
            }
        }
    }
#else
    // Signed overflow is UB: check bounds before evaluating the expression.
    // For unsigned types, overflow is well-defined (wraps), but we check first
    // for consistency.
    bool overflow;

    if constexpr (std::is_signed_v<T>)
    {
        overflow = (b > 0 && a > std::numeric_limits<T>::max() - b) || (b < 0 && a < std::numeric_limits<T>::min() - b);
    }
    else
    {
        overflow = (a > std::numeric_limits<T>::max() - b);
    }

    if (overflow)
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            detail::checked_arithmetic_fail(FATP_LOCUS, "Addition overflow:", a, "+", b);
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            if constexpr (std::is_signed_v<T>)
            {
                MathError code = (b > 0) ? MathError::Overflow : MathError::Underflow;
                return Expected<T, MathError>(unexpect, code);
            }
            else
            {
                return Expected<T, MathError>(unexpect, MathError::Overflow);
            }
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy> || std::is_same_v<Policy, InfTolerantPolicy>)
        {
            if constexpr (std::is_signed_v<T>)
            {
                return (b > 0) ? std::numeric_limits<T>::max() : std::numeric_limits<T>::lowest();
            }
            else
            {
                return std::numeric_limits<T>::max();
            }
        }
    }

    // No overflow: safe to compute
    T result = a + b;
#endif

    return result;
}

/**
 * @brief Checked subtraction for integers with overflow detection
 */
template <typename Policy = ThrowOnErrorPolicy, FATP_ENABLE_IF_INTEGRAL>
[[nodiscard]] constexpr PolicyReturnType<Policy, T>
checked_sub(T a, T b) noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
#if FATP_HAS_BUILTIN_OVERFLOW
    T result;
    if (__builtin_sub_overflow(a, b, &result))
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            detail::checked_arithmetic_fail(FATP_LOCUS, "Subtraction overflow:", a, "-", b);
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            if constexpr (std::is_signed_v<T>)
            {
                MathError code = (b < 0) ? MathError::Overflow : MathError::Underflow;
                return Expected<T, MathError>(unexpect, code);
            }
            else
            {
                return Expected<T, MathError>(unexpect, MathError::Underflow);
            }
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy> || std::is_same_v<Policy, InfTolerantPolicy>)
        {
            if constexpr (std::is_signed_v<T>)
            {
                return (b < 0) ? std::numeric_limits<T>::max() : std::numeric_limits<T>::lowest();
            }
            else
            {
                return std::numeric_limits<T>::lowest();
            }
        }
    }
#else
    // Signed overflow is UB: check bounds before evaluating the expression.
    bool overflow;

    if constexpr (std::is_signed_v<T>)
    {
        overflow = (b < 0 && a > std::numeric_limits<T>::max() + b) || (b > 0 && a < std::numeric_limits<T>::min() + b);
    }
    else
    {
        overflow = (a < b);
    }

    if (overflow)
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            detail::checked_arithmetic_fail(FATP_LOCUS, "Subtraction overflow:", a, "-", b);
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            if constexpr (std::is_signed_v<T>)
            {
                MathError code = (b < 0) ? MathError::Overflow : MathError::Underflow;
                return Expected<T, MathError>(unexpect, code);
            }
            else
            {
                return Expected<T, MathError>(unexpect, MathError::Underflow);
            }
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy> || std::is_same_v<Policy, InfTolerantPolicy>)
        {
            if constexpr (std::is_signed_v<T>)
            {
                return (b < 0) ? std::numeric_limits<T>::max() : std::numeric_limits<T>::lowest();
            }
            else
            {
                return std::numeric_limits<T>::lowest();
            }
        }
    }

    // No overflow: safe to compute
    T result = a - b;
#endif

    return result;
}

/**
 * @brief Checked multiplication for integers with overflow detection
 */
template <typename Policy = ThrowOnErrorPolicy, FATP_ENABLE_IF_INTEGRAL>
[[nodiscard]] constexpr PolicyReturnType<Policy, T>
checked_mul(T a, T b) noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
#if FATP_HAS_BUILTIN_OVERFLOW
    T result;
    if (__builtin_mul_overflow(a, b, &result))
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            detail::checked_arithmetic_fail(FATP_LOCUS, "Multiplication overflow:", a, "*", b);
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<T, MathError>(unexpect, MathError::Overflow);
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy> || std::is_same_v<Policy, InfTolerantPolicy>)
        {
            if constexpr (std::is_signed_v<T>)
            {
                bool negative = (a < 0) != (b < 0);
                return negative ? std::numeric_limits<T>::lowest() : std::numeric_limits<T>::max();
            }
            else
            {
                return std::numeric_limits<T>::max();
            }
        }
    }
#else
    if (a == 0 || b == 0)
    {
        return T{0};
    }

    // Signed overflow is UB: check bounds before evaluating the expression.
    bool overflow;

    if constexpr (std::is_signed_v<T>)
    {
        overflow = (a > 0 && b > 0 && a > std::numeric_limits<T>::max() / b) ||
                   (a < 0 && b < 0 && a < std::numeric_limits<T>::max() / b) ||
                   (a > 0 && b < 0 && b < std::numeric_limits<T>::min() / a) ||
                   (a < 0 && b > 0 && a < std::numeric_limits<T>::min() / b);
    }
    else
    {
        overflow = (a > std::numeric_limits<T>::max() / b);
    }

    if (overflow)
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            detail::checked_arithmetic_fail(FATP_LOCUS, "Multiplication overflow:", a, "*", b);
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<T, MathError>(unexpect, MathError::Overflow);
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy> || std::is_same_v<Policy, InfTolerantPolicy>)
        {
            if constexpr (std::is_signed_v<T>)
            {
                bool negative = (a < 0) != (b < 0);
                return negative ? std::numeric_limits<T>::lowest() : std::numeric_limits<T>::max();
            }
            else
            {
                return std::numeric_limits<T>::max();
            }
        }
    }

    // No overflow: safe to compute
    T result = a * b;
#endif

    return result;
}

/**
 * @brief Checked division for integers with zero and overflow detection
 *
 * Detects:
 * - Division by zero
 * - MIN / -1 overflow (for signed types)
 */
template <typename Policy = ThrowOnErrorPolicy, FATP_ENABLE_IF_INTEGRAL>
[[nodiscard]] constexpr PolicyReturnType<Policy, T>
checked_div(T a, T b) noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    if (b == 0)
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            detail::checked_arithmetic_fail(FATP_LOCUS, "Division by zero:", a, "/", b);
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<T, MathError>(unexpect, MathError::DivByZero);
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy> || std::is_same_v<Policy, InfTolerantPolicy>)
        {
            if (a == 0)
            {
                return T{0};
            }
            return (a > 0) ? std::numeric_limits<T>::max() : std::numeric_limits<T>::lowest();
        }
    }

    if constexpr (std::is_signed_v<T>)
    {
        bool overflow = (a == std::numeric_limits<T>::min() && b == -1);
        if (overflow)
        {
            if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
            {
                detail::checked_arithmetic_fail(FATP_LOCUS, "Overflow in division (min/-1):", a, "/", b);
            }
            else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
            {
                return Expected<T, MathError>(unexpect, MathError::Overflow);
            }
            else if constexpr (std::is_same_v<Policy, SaturatingPolicy> || std::is_same_v<Policy, InfTolerantPolicy>)
            {
                return std::numeric_limits<T>::max();
            }
        }
    }

    T result = a / b;
    return result;
}

/**
 * @brief Checked modulo for integers with zero and overflow detection
 */
template <typename Policy = ThrowOnErrorPolicy, FATP_ENABLE_IF_INTEGRAL>
[[nodiscard]] constexpr PolicyReturnType<Policy, T>
checked_mod(T a, T b) noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    if (b == 0)
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            detail::checked_arithmetic_fail(FATP_LOCUS, "Modulo by zero:", a, "%", b);
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<T, MathError>(unexpect, MathError::DivByZero);
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy> || std::is_same_v<Policy, InfTolerantPolicy>)
        {
            return T{0};
        }
    }

    if constexpr (std::is_signed_v<T>)
    {
        bool overflow = (a == std::numeric_limits<T>::min() && b == -1);
        if (overflow)
        {
            if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
            {
                detail::checked_arithmetic_fail(FATP_LOCUS, "Overflow in mod (min%-1):", a, "%", b);
            }
            else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
            {
                return Expected<T, MathError>(unexpect, MathError::Overflow);
            }
            else if constexpr (std::is_same_v<Policy, SaturatingPolicy> || std::is_same_v<Policy, InfTolerantPolicy>)
            {
                return T{0};
            }
        }
    }

    T result = a % b;
    return result;
}

// =============================================================================
// Bitwise Operations
// =============================================================================

/**
 * @brief Checked bitwise AND (always safe, no overflow possible)
 */
template <typename Policy = ThrowOnErrorPolicy, FATP_ENABLE_IF_INTEGRAL>
[[nodiscard]] constexpr PolicyReturnType<Policy, T>
checked_and(T a, T b) noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    T result = a & b;
    return result;
}

/**
 * @brief Checked bitwise OR (always safe, no overflow possible)
 */
template <typename Policy = ThrowOnErrorPolicy, FATP_ENABLE_IF_INTEGRAL>
[[nodiscard]] constexpr PolicyReturnType<Policy, T>
checked_or(T a, T b) noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    T result = a | b;
    return result;
}

/**
 * @brief Checked bitwise XOR (always safe, no overflow possible)
 */
template <typename Policy = ThrowOnErrorPolicy, FATP_ENABLE_IF_INTEGRAL>
[[nodiscard]] constexpr PolicyReturnType<Policy, T>
checked_xor(T a, T b) noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    T result = a ^ b;
    return result;
}

/**
 * @brief Checked left shift with UB prevention
 *
 * Prevents undefined behavior from:
 * - Negative or too-large shift amounts
 * - Left-shifting negative signed values (C++ UB)
 * - Left-shifting positive signed values through the sign bit (C++ UB)
 */
template <typename Policy = ThrowOnErrorPolicy, FATP_ENABLE_IF_INTEGRAL, typename S>
[[nodiscard]] constexpr PolicyReturnType<Policy, T>
checked_left_shift(T a, S shift) noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    static_assert(std::is_integral_v<S>, "Shift must be integral");

    constexpr auto max_shift = static_cast<S>(sizeof(T) * 8);

    // Check for invalid shift amount
    if (shift < 0 || shift >= max_shift)
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            detail::checked_arithmetic_fail(FATP_LOCUS, "Invalid left shift amount:", shift);
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<T, MathError>(unexpect, MathError::InvalidArgument);
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy> || std::is_same_v<Policy, InfTolerantPolicy>)
        {
            return T{0};
        }
    }

    if constexpr (std::is_signed_v<T>)
    {
        // Left-shifting a negative signed value is undefined behavior in C++
        if (a < 0)
        {
            if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
            {
                detail::checked_arithmetic_fail(FATP_LOCUS,
                                                "Left shift of negative value is undefined:",
                                                a,
                                                "<<",
                                                shift);
            }
            else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
            {
                return Expected<T, MathError>(unexpect, MathError::InvalidArgument);
            }
            else if constexpr (std::is_same_v<Policy, SaturatingPolicy> || std::is_same_v<Policy, InfTolerantPolicy>)
            {
                return T{0};
            }
        }

        // Left-shifting a positive value into/through the sign bit is also UB.
        // Check: a << shift overflows if a > (MAX >> shift)
        using UT = std::make_unsigned_t<T>;
        UT max_safe = static_cast<UT>(std::numeric_limits<T>::max()) >> shift;
        if (static_cast<UT>(a) > max_safe)
        {
            if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
            {
                detail::checked_arithmetic_fail(FATP_LOCUS, "Left shift overflow:", a, "<<", shift);
            }
            else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
            {
                return Expected<T, MathError>(unexpect, MathError::Overflow);
            }
            else if constexpr (std::is_same_v<Policy, SaturatingPolicy> || std::is_same_v<Policy, InfTolerantPolicy>)
            {
                return std::numeric_limits<T>::max();
            }
        }
    }

    // No UB: safe to compute
    T result = a << shift;
    return result;
}

/**
 * @brief Checked right shift with invalid shift detection
 */
template <typename Policy = ThrowOnErrorPolicy, FATP_ENABLE_IF_INTEGRAL, typename S>
[[nodiscard]] constexpr PolicyReturnType<Policy, T>
checked_right_shift(T a, S shift) noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    static_assert(std::is_integral_v<S>, "Shift must be integral");

    constexpr auto max_shift = static_cast<S>(sizeof(T) * 8);

    if (shift < 0 || shift >= max_shift)
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            detail::checked_arithmetic_fail(FATP_LOCUS, "Invalid right shift amount:", shift);
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<T, MathError>(unexpect, MathError::InvalidArgument);
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy> || std::is_same_v<Policy, InfTolerantPolicy>)
        {
            if constexpr (std::is_signed_v<T>)
            {
                return (a < 0) ? T{-1} : T{0};
            }
            else
            {
                return T{0};
            }
        }
    }

    T result = a >> shift;
    return result;
}

// =============================================================================
// Unary Integer Operations
// =============================================================================

/**
 * @brief Checked negation for integers with overflow detection
 *
 * For signed types, -MIN overflows because |MIN| > MAX in two's complement.
 */
template <typename Policy = ThrowOnErrorPolicy, FATP_ENABLE_IF_INTEGRAL>
[[nodiscard]] constexpr PolicyReturnType<Policy, T>
checked_negate(T a) noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    if constexpr (std::is_signed_v<T>)
    {
        if (a == std::numeric_limits<T>::min())
        {
            if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
            {
                detail::checked_arithmetic_fail(FATP_LOCUS, "Overflow in negation (min):", a);
            }
            else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
            {
                return Expected<T, MathError>(unexpect, MathError::Overflow);
            }
            else if constexpr (std::is_same_v<Policy, SaturatingPolicy> || std::is_same_v<Policy, InfTolerantPolicy>)
            {
                return std::numeric_limits<T>::max();
            }
        }
    }

    T result = -a;
    return result;
}

/**
 * @brief Checked absolute value for integers with overflow detection
 *
 * For signed types, abs(MIN) overflows because |MIN| > MAX.
 * For unsigned types, this is a no-op.
 */
template <typename Policy = ThrowOnErrorPolicy, FATP_ENABLE_IF_INTEGRAL>
[[nodiscard]] constexpr PolicyReturnType<Policy, T>
checked_abs(T a) noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    if constexpr (std::is_unsigned_v<T>)
    {
        return a;
    }
    else
    {
        if (a == std::numeric_limits<T>::min())
        {
            if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
            {
                detail::checked_arithmetic_fail(FATP_LOCUS, "Overflow in abs (min):", a);
            }
            else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
            {
                return Expected<T, MathError>(unexpect, MathError::Overflow);
            }
            else if constexpr (std::is_same_v<Policy, SaturatingPolicy> || std::is_same_v<Policy, InfTolerantPolicy>)
            {
                return std::numeric_limits<T>::max();
            }
        }

        return (a < 0) ? -a : a;
    }
}

/**
 * @brief Checked increment (a + 1)
 */
template <typename Policy = ThrowOnErrorPolicy, FATP_ENABLE_IF_INTEGRAL>
[[nodiscard]] constexpr PolicyReturnType<Policy, T>
checked_inc(T a) noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    return checked_add<Policy>(a, T{1});
}

/**
 * @brief Checked decrement (a - 1)
 */
template <typename Policy = ThrowOnErrorPolicy, FATP_ENABLE_IF_INTEGRAL>
[[nodiscard]] constexpr PolicyReturnType<Policy, T>
checked_dec(T a) noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    return checked_sub<Policy>(a, T{1});
}

// =============================================================================
// Pointer Arithmetic
// =============================================================================

/**
 * @brief Checked pointer arithmetic with address-space overflow detection
 *
 * @warning Guarantees ADDRESS-SPACE overflow safety but CANNOT guarantee
 *          object bounds. Ensure resulting pointer is within same allocation.
 *
 * @note Offset is in elements (like standard ptr arithmetic), not bytes.
 */
template <typename P, typename Policy = ReturnExpectedPolicy>
[[nodiscard]] PolicyReturnType<Policy, P*>
checked_add(P* ptr, std::ptrdiff_t offset) noexcept(PolicyTraits<Policy>::template is_noexcept<std::ptrdiff_t>)
{
    static_assert(!std::is_void_v<P>, "Cannot perform pointer arithmetic on void*");

    auto addr = reinterpret_cast<std::uintptr_t>(ptr);
    constexpr std::size_t elem_size = sizeof(P);

    if (offset >= 0)
    {
        auto byte_offset_result =
            checked_mul<Policy>(static_cast<std::uintptr_t>(offset), static_cast<std::uintptr_t>(elem_size));
        if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            if (!byte_offset_result.has_value())
            {
                return unexpected(byte_offset_result.error());
            }
            auto res = checked_add<Policy>(addr, byte_offset_result.value());
            if (!res.has_value())
            {
                return unexpected(res.error());
            }
            return reinterpret_cast<P*>(res.value());
        }
        else
        {
            auto res = checked_add<Policy>(addr, byte_offset_result);
            return reinterpret_cast<P*>(res);
        }
    }
    else
    {
        if (offset == std::numeric_limits<std::ptrdiff_t>::min())
        {
            if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
            {
                detail::checked_arithmetic_fail(FATP_LOCUS, "Pointer arithmetic overflow: offset == PTRDIFF_MIN");
            }
            else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
            {
                return unexpected(MathError::Overflow);
            }
            else
            {
                return nullptr;
            }
        }

        auto byte_offset_result =
            checked_mul<Policy>(static_cast<std::uintptr_t>(-offset), static_cast<std::uintptr_t>(elem_size));
        if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            if (!byte_offset_result.has_value())
            {
                return unexpected(byte_offset_result.error());
            }
            auto res = checked_sub<Policy>(addr, byte_offset_result.value());
            if (!res.has_value())
            {
                return unexpected(res.error());
            }
            return reinterpret_cast<P*>(res.value());
        }
        else
        {
            auto res = checked_sub<Policy>(addr, byte_offset_result);
            return reinterpret_cast<P*>(res);
        }
    }
}

/**
 * @brief Checked pointer subtraction
 */
template <typename P, typename Policy = ReturnExpectedPolicy>
[[nodiscard]] PolicyReturnType<Policy, P*>
checked_sub(P* ptr, std::ptrdiff_t offset) noexcept(PolicyTraits<Policy>::template is_noexcept<std::ptrdiff_t>)
{
    static_assert(!std::is_void_v<P>, "Cannot perform pointer arithmetic on void*");

    auto addr = reinterpret_cast<std::uintptr_t>(ptr);
    constexpr std::size_t elem_size = sizeof(P);

    if (offset >= 0)
    {
        auto byte_offset_result =
            checked_mul<Policy>(static_cast<std::uintptr_t>(offset), static_cast<std::uintptr_t>(elem_size));
        if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            if (!byte_offset_result.has_value())
            {
                return unexpected(byte_offset_result.error());
            }
            auto res = checked_sub<Policy>(addr, byte_offset_result.value());
            if (!res.has_value())
            {
                return unexpected(res.error());
            }
            return reinterpret_cast<P*>(res.value());
        }
        else
        {
            auto res = checked_sub<Policy>(addr, byte_offset_result);
            return reinterpret_cast<P*>(res);
        }
    }
    else
    {
        if (offset == std::numeric_limits<std::ptrdiff_t>::min())
        {
            if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
            {
                detail::checked_arithmetic_fail(FATP_LOCUS, "Pointer arithmetic overflow: offset == PTRDIFF_MIN");
            }
            else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
            {
                return unexpected(MathError::Overflow);
            }
            else
            {
                return nullptr;
            }
        }

        auto byte_offset_result =
            checked_mul<Policy>(static_cast<std::uintptr_t>(-offset), static_cast<std::uintptr_t>(elem_size));
        if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            if (!byte_offset_result.has_value())
            {
                return unexpected(byte_offset_result.error());
            }
            auto res = checked_add<Policy>(addr, byte_offset_result.value());
            if (!res.has_value())
            {
                return unexpected(res.error());
            }
            return reinterpret_cast<P*>(res.value());
        }
        else
        {
            auto res = checked_add<Policy>(addr, byte_offset_result);
            return reinterpret_cast<P*>(res);
        }
    }
}

/**
 * @brief Checked pointer increment
 */
template <typename P, typename Policy = ReturnExpectedPolicy>
[[nodiscard]] PolicyReturnType<Policy, P*>
checked_inc(P* ptr) noexcept(PolicyTraits<Policy>::template is_noexcept<std::ptrdiff_t>)
{
    return checked_add<P, Policy>(ptr, std::ptrdiff_t{1});
}

/**
 * @brief Checked pointer decrement
 */
template <typename P, typename Policy = ReturnExpectedPolicy>
[[nodiscard]] PolicyReturnType<Policy, P*>
checked_dec(P* ptr) noexcept(PolicyTraits<Policy>::template is_noexcept<std::ptrdiff_t>)
{
    return checked_sub<P, Policy>(ptr, std::ptrdiff_t{1});
}

// =============================================================================
// Vector Operations (SIMD-Accelerated)
// =============================================================================

/**
 * @brief SIMD-accelerated checked vector addition for integers
 *
 * Architecture acceleration:
 * - AVX2: 8-wide for int32, 4-wide for int64
 * - SSE2: 4-wide for int32, 2-wide for int64
 * - NEON: 4-wide for int32, 2-wide for int64 (AArch64)
 *
 * @param vec_a First operand vector
 * @param vec_b Second operand vector (must match size)
 * @return Result vector according to policy
 */
template <typename Policy = ThrowOnErrorPolicy, typename T>
[[nodiscard]] PolicyReturnType<Policy, std::vector<T>>
checked_add_vec(const std::vector<T>& vec_a,
                const std::vector<T>& vec_b) noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    static_assert(std::is_integral_v<T>, "checked_add_vec requires integral types");

    if (vec_a.size() != vec_b.size())
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            detail::checked_arithmetic_fail(FATP_LOCUS, "Vector size mismatch in addition");
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<std::vector<T>, MathError>(unexpect, MathError::InvalidArgument);
        }
        else
        {
            return std::vector<T>();
        }
    }

    size_t n = vec_a.size();
    std::vector<T> result(n);

#if FATP_HAS_INT_SIMD
    if constexpr (std::is_same_v<T, int32_t> || std::is_same_v<T, uint32_t> || std::is_same_v<T, int64_t>)
    {
        auto scalar_fn = [](T a, T b)
        {
            return checked_add<Policy>(a, b);
        };

        bool success =
            int_simd::checked_add_vec_simd<T, Policy>(vec_a.data(), vec_b.data(), result.data(), n, scalar_fn);

        if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            if (!success)
            {
                for (size_t i = 0; i < n; ++i)
                {
                    auto temp = checked_add<Policy>(vec_a[i], vec_b[i]);
                    if (!temp.has_value())
                    {
                        return Expected<std::vector<T>, MathError>(unexpect, temp.error());
                    }
                }
            }
        }

        return result;
    }
#endif

    // Scalar fallback
    for (size_t i = 0; i < n; ++i)
    {
        auto temp = checked_add<Policy>(vec_a[i], vec_b[i]);
        if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            if (!temp.has_value())
            {
                return Expected<std::vector<T>, MathError>(unexpect, temp.error());
            }
            result[i] = temp.value();
        }
        else
        {
            result[i] = temp;
        }
    }

    return result;
}

/**
 * @brief SIMD-accelerated checked vector subtraction for integers
 */
template <typename Policy = ThrowOnErrorPolicy, typename T>
[[nodiscard]] PolicyReturnType<Policy, std::vector<T>>
checked_sub_vec(const std::vector<T>& vec_a,
                const std::vector<T>& vec_b) noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    static_assert(std::is_integral_v<T>, "checked_sub_vec requires integral types");

    if (vec_a.size() != vec_b.size())
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            detail::checked_arithmetic_fail(FATP_LOCUS, "Vector size mismatch in subtraction");
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<std::vector<T>, MathError>(unexpect, MathError::InvalidArgument);
        }
        else
        {
            return std::vector<T>();
        }
    }

    size_t n = vec_a.size();
    std::vector<T> result(n);

#if FATP_HAS_INT_SIMD
    if constexpr (std::is_same_v<T, int32_t> || std::is_same_v<T, uint32_t> || std::is_same_v<T, int64_t>)
    {
        auto scalar_fn = [](T a, T b)
        {
            return checked_sub<Policy>(a, b);
        };

        bool success =
            int_simd::checked_sub_vec_simd<T, Policy>(vec_a.data(), vec_b.data(), result.data(), n, scalar_fn);

        if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            if (!success)
            {
                for (size_t i = 0; i < n; ++i)
                {
                    auto temp = checked_sub<Policy>(vec_a[i], vec_b[i]);
                    if (!temp.has_value())
                    {
                        return Expected<std::vector<T>, MathError>(unexpect, temp.error());
                    }
                }
            }
        }

        return result;
    }
#endif

    // Scalar fallback
    for (size_t i = 0; i < n; ++i)
    {
        auto temp = checked_sub<Policy>(vec_a[i], vec_b[i]);
        if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            if (!temp.has_value())
            {
                return Expected<std::vector<T>, MathError>(unexpect, temp.error());
            }
            result[i] = temp.value();
        }
        else
        {
            result[i] = temp;
        }
    }

    return result;
}

/**
 * @brief SIMD-accelerated checked vector multiplication for integers
 *
 * Uses wide-multiply technique: int32*int32->int64 with bounds check.
 * Only available for int32_t/uint32_t (int64 would need int128).
 */
template <typename Policy = ThrowOnErrorPolicy, typename T>
[[nodiscard]] PolicyReturnType<Policy, std::vector<T>>
checked_mul_vec(const std::vector<T>& vec_a,
                const std::vector<T>& vec_b) noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    static_assert(std::is_integral_v<T>, "checked_mul_vec requires integral types");

    if (vec_a.size() != vec_b.size())
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            detail::checked_arithmetic_fail(FATP_LOCUS, "Vector size mismatch in multiplication");
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<std::vector<T>, MathError>(unexpect, MathError::InvalidArgument);
        }
        else
        {
            return std::vector<T>();
        }
    }

    size_t n = vec_a.size();
    std::vector<T> result(n);

#if FATP_HAS_INT_SIMD
    if constexpr (std::is_same_v<T, int32_t> || std::is_same_v<T, uint32_t>)
    {
        auto scalar_fn = [](T a, T b)
        {
            return checked_mul<Policy>(a, b);
        };

        bool success =
            int_simd::checked_mul_vec_simd<T, Policy>(vec_a.data(), vec_b.data(), result.data(), n, scalar_fn);

        if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            if (!success)
            {
                for (size_t i = 0; i < n; ++i)
                {
                    auto temp = checked_mul<Policy>(vec_a[i], vec_b[i]);
                    if (!temp.has_value())
                    {
                        return Expected<std::vector<T>, MathError>(unexpect, temp.error());
                    }
                }
            }
        }

        return result;
    }
#endif

    // Scalar fallback (also for int64 which has no SIMD mul path)
    for (size_t i = 0; i < n; ++i)
    {
        auto temp = checked_mul<Policy>(vec_a[i], vec_b[i]);
        if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            if (!temp.has_value())
            {
                return Expected<std::vector<T>, MathError>(unexpect, temp.error());
            }
            result[i] = temp.value();
        }
        else
        {
            result[i] = temp;
        }
    }

    return result;
}

} // namespace fat_p
