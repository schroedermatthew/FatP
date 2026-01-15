/**
 * @file CheckedArithmeticFP.h
 * @brief Checked arithmetic operations for floating-point types
 *
 *
 * @layer Foundation
 *
 * @version 3.0
 *
 * This header provides validated arithmetic operations for float/double:
 *
 * Scalar Operations:
 * - checked_add_fp, checked_sub_fp, checked_mul_fp, checked_div_fp
 * - checked_mod_fp, checked_abs_fp, checked_sqrt_fp
 * - checked_floor_fp, checked_ceil_fp, checked_trunc_fp, checked_round_fp
 *
 * Vector Operations (SIMD-accelerated via SimdVector):
 * - checked_add_vec_fp, checked_sub_vec_fp, checked_mul_vec_fp, checked_div_vec_fp
 * - Accelerated on: SSE2, AVX, AVX-512, NEON (AArch64)
 * - Automatic width selection (4-16 elements per operation)
 *
 * Validation covers:
 * - NaN input detection
 * - Infinity from finite inputs (overflow)
 * - Division by zero
 * - Invalid operations (e.g., sqrt of negative)
 *
 * Policy handling:
 * - ThrowOnErrorPolicy: Throws on NaN/Inf/error
 * - ReturnExpectedPolicy: Returns Expected<T, MathError>
 * - SaturatingPolicy: Clamps overflow to finite extremes
 * - InfTolerantPolicy: Allows infinity through (only NaN is error)
 *
 * Dependency: CheckedArithmeticPolicies.h, SimdVector.h
 *
 * Part of the CheckedArithmetic split architecture:
 *   CheckedArithmeticBase.h     <- Foundation
 *   CheckedArithmeticPolicies.h <- Policies
 *   CheckedArithmeticInt.h      <- Integer ops
 *   CheckedArithmeticFP.h       <- This file (FP ops)
 *   CheckedArithmetic.h         <- Umbrella
 */
#pragma once

/*
FATP_META:
  meta_version: 1
  component: CheckedArithmeticFP
  file_role: public_header
  path: fat_p/CheckedArithmeticFP.h
  namespace: fat_p
  layer: Foundation
  summary: "Public header for CheckedArithmeticFP."
  api_stability: in_work
  related:
    docs_search: "CheckedArithmeticFP"
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 1
    defines_unprefixed: 1
    undefs_total: 1
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/
#include "CheckedArithmeticPolicies.h"
#include "enforce.h"
#include "SimdVector.h"

#include <cmath>
#include <vector>

#if defined(__AVX2__)
#include <immintrin.h>
#endif

namespace fat_p
{

// =============================================================================
// Internal Macros
// =============================================================================

/**
 * @brief Validates FP inputs for NaN and problematic Inf combinations
 *
 * This macro performs common input validation for binary FP operations:
 * - Detects NaN in either input
 * - Detects Inf-Inf or Inf+(-Inf) which produce NaN
 *
 * Must be followed by the actual operation.
 */
#define FATP_VALIDATE_FP_INPUTS(a, b, op_name)                                          \
    do                                                                                  \
    {                                                                                   \
        if (std::isnan(a) || std::isnan(b))                                             \
        {                                                                               \
            if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)                   \
            {                                                                           \
                FATP_ALWAYS_ENFORCE(false, "FP input contains NaN:", a, op_name, b);    \
            }                                                                           \
            else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)            \
            {                                                                           \
                return Expected<T, MathError>(unexpect, MathError::NaN);                \
            }                                                                           \
            else if constexpr (std::is_same_v<Policy, SaturatingPolicy>)                \
            {                                                                           \
                return std::numeric_limits<T>::quiet_NaN();                             \
            }                                                                           \
            else if constexpr (std::is_same_v<Policy, InfTolerantPolicy>)               \
            {                                                                           \
                return std::numeric_limits<T>::quiet_NaN();                             \
            }                                                                           \
        }                                                                               \
        if (std::isinf(a) && std::isinf(b))                                             \
        {                                                                               \
            constexpr bool is_subtraction = (op_name[0] == '-');                        \
            bool same_sign = (a > 0) == (b > 0);                                        \
            if ((!same_sign && op_name[0] == '+') || (same_sign && is_subtraction))     \
            {                                                                           \
                if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)               \
                {                                                                       \
                    FATP_ALWAYS_ENFORCE(false, "FP Inf-Inf undefined:", a, op_name, b); \
                }                                                                       \
                else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)        \
                {                                                                       \
                    return Expected<T, MathError>(unexpect, MathError::NaN);            \
                }                                                                       \
                else if constexpr (std::is_same_v<Policy, SaturatingPolicy>)            \
                {                                                                       \
                    return std::numeric_limits<T>::quiet_NaN();                         \
                }                                                                       \
                else if constexpr (std::is_same_v<Policy, InfTolerantPolicy>)           \
                {                                                                       \
                    return std::numeric_limits<T>::quiet_NaN();                         \
                }                                                                       \
            }                                                                           \
        }                                                                               \
    } while (0)

// =============================================================================
// Scalar Floating-Point Operations
// =============================================================================

/**
 * @brief Checked floating-point addition
 *
 * Validates inputs and detects overflow (finite->infinite).
 */
template <typename Policy = ThrowOnErrorPolicy, FATP_ENABLE_IF_FLOATING>
[[nodiscard]] PolicyReturnType<Policy, T> checked_add_fp(T a,
                                                         T b) noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    FATP_VALIDATE_FP_INPUTS(a, b, "+");

    T result = a + b;
    bool is_nan = std::isnan(result);
    bool is_inf = std::isinf(result);

    if (is_nan || (is_inf && std::isfinite(a) && std::isfinite(b)))
    {
        MathError code = is_nan ? MathError::NaN : MathError::Inf;
        if constexpr (std::is_same_v<Policy, InfTolerantPolicy>)
        {
            if (is_inf)
            {
                return result;
            }
            return std::numeric_limits<T>::quiet_NaN();
        }
        else if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            FATP_ALWAYS_ENFORCE(false, "Addition error (NaN/Inf):", a, "+", b);
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<T, MathError>(unexpect, code);
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy>)
        {
            if (is_nan)
            {
                return std::numeric_limits<T>::quiet_NaN();
            }
            return (result > 0) ? std::numeric_limits<T>::max() : std::numeric_limits<T>::lowest();
        }
    }

    return result;
}

/**
 * @brief Checked floating-point subtraction
 */
template <typename Policy = ThrowOnErrorPolicy, FATP_ENABLE_IF_FLOATING>
[[nodiscard]] PolicyReturnType<Policy, T> checked_sub_fp(T a,
                                                         T b) noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    FATP_VALIDATE_FP_INPUTS(a, b, "-");

    T result = a - b;
    bool is_nan = std::isnan(result);
    bool is_inf = std::isinf(result);

    if (is_nan || (is_inf && std::isfinite(a) && std::isfinite(b)))
    {
        MathError code = is_nan ? MathError::NaN : MathError::Inf;
        if constexpr (std::is_same_v<Policy, InfTolerantPolicy>)
        {
            if (is_inf)
            {
                return result;
            }
            return std::numeric_limits<T>::quiet_NaN();
        }
        else if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            FATP_ALWAYS_ENFORCE(false, "Subtraction error (NaN/Inf):", a, "-", b);
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<T, MathError>(unexpect, code);
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy>)
        {
            if (is_nan)
            {
                return std::numeric_limits<T>::quiet_NaN();
            }
            return (result > 0) ? std::numeric_limits<T>::max() : std::numeric_limits<T>::lowest();
        }
    }

    return result;
}

/**
 * @brief Checked floating-point multiplication
 */
template <typename Policy = ThrowOnErrorPolicy, FATP_ENABLE_IF_FLOATING>
[[nodiscard]] PolicyReturnType<Policy, T> checked_mul_fp(T a,
                                                         T b) noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    FATP_VALIDATE_FP_INPUTS(a, b, "*");

    T result = a * b;
    bool is_nan = std::isnan(result);
    bool is_inf = std::isinf(result);

    if (is_nan || (is_inf && std::isfinite(a) && std::isfinite(b)))
    {
        MathError code = is_nan ? MathError::NaN : MathError::Inf;
        if constexpr (std::is_same_v<Policy, InfTolerantPolicy>)
        {
            if (is_inf)
            {
                return result;
            }
            return std::numeric_limits<T>::quiet_NaN();
        }
        else if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            FATP_ALWAYS_ENFORCE(false, "Multiplication error (NaN/Inf):", a, "*", b);
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<T, MathError>(unexpect, code);
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy>)
        {
            if (is_nan)
            {
                return std::numeric_limits<T>::quiet_NaN();
            }
            return (result > 0) ? std::numeric_limits<T>::max() : std::numeric_limits<T>::lowest();
        }
    }

    return result;
}

/**
 * @brief Checked floating-point division
 *
 * Additionally handles division by zero specially.
 */
template <typename Policy = ThrowOnErrorPolicy, FATP_ENABLE_IF_FLOATING>
[[nodiscard]] PolicyReturnType<Policy, T> checked_div_fp(T a,
                                                         T b) noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    FATP_VALIDATE_FP_INPUTS(a, b, "/");

    if (b == T{0})
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            FATP_ALWAYS_ENFORCE(false, "Division by zero in div_fp:", a, "/", b);
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<T, MathError>(unexpect, MathError::DivByZero);
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy>)
        {
            if (a == T{0})
            {
                return T{0};
            }
            return (a > T{0}) ? std::numeric_limits<T>::max() : std::numeric_limits<T>::lowest();
        }
        else if constexpr (std::is_same_v<Policy, InfTolerantPolicy>)
        {
            if (a == T{0})
            {
                return std::numeric_limits<T>::quiet_NaN();
            }
            return (a > T{0}) ? std::numeric_limits<T>::infinity() : -std::numeric_limits<T>::infinity();
        }
    }

    T result = a / b;
    bool is_nan = std::isnan(result);
    bool is_inf = std::isinf(result);

    if (is_nan || (is_inf && std::isfinite(a) && std::isfinite(b)))
    {
        MathError code = is_nan ? MathError::NaN : MathError::Inf;
        if constexpr (std::is_same_v<Policy, InfTolerantPolicy>)
        {
            if (is_inf)
            {
                return result;
            }
            return std::numeric_limits<T>::quiet_NaN();
        }
        else if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            FATP_ALWAYS_ENFORCE(false, "Division error (NaN/Inf):", a, "/", b);
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<T, MathError>(unexpect, code);
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy>)
        {
            if (is_nan)
            {
                return std::numeric_limits<T>::quiet_NaN();
            }
            return (result > 0) ? std::numeric_limits<T>::max() : std::numeric_limits<T>::lowest();
        }
    }

    return result;
}

/**
 * @brief Checked floating-point modulo (fmod)
 */
template <typename Policy = ThrowOnErrorPolicy, FATP_ENABLE_IF_FLOATING>
[[nodiscard]] PolicyReturnType<Policy, T> checked_mod_fp(T a,
                                                         T b) noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    FATP_VALIDATE_FP_INPUTS(a, b, "%");

    if (b == T{0})
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            FATP_ALWAYS_ENFORCE(false, "Division by zero in mod_fp:", a, "%", b);
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<T, MathError>(unexpect, MathError::DivByZero);
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy>)
        {
            return T{0};
        }
        else if constexpr (std::is_same_v<Policy, InfTolerantPolicy>)
        {
            return std::numeric_limits<T>::quiet_NaN();
        }
    }

    T result = std::fmod(a, b);

    if (std::isnan(result))
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            FATP_ALWAYS_ENFORCE(false, "Modulo error (NaN):", a, "%", b);
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<T, MathError>(unexpect, MathError::NaN);
        }
        else
        {
            return std::numeric_limits<T>::quiet_NaN();
        }
    }

    return result;
}

/**
 * @brief Checked floating-point absolute value
 */
template <typename Policy = ThrowOnErrorPolicy, FATP_ENABLE_IF_FLOATING>
[[nodiscard]] PolicyReturnType<Policy, T> checked_abs_fp(T a) noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    if (std::isnan(a))
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            FATP_ALWAYS_ENFORCE(false, "NaN input in abs_fp:", a);
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<T, MathError>(unexpect, MathError::NaN);
        }
        else
        {
            return std::numeric_limits<T>::quiet_NaN();
        }
    }

    return std::abs(a);
}

/**
 * @brief Checked floating-point square root
 *
 * Detects NaN input and negative input (invalid domain).
 */
template <typename Policy = ThrowOnErrorPolicy, FATP_ENABLE_IF_FLOATING>
[[nodiscard]] PolicyReturnType<Policy, T> checked_sqrt_fp(T a) noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    if (std::isnan(a))
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            FATP_ALWAYS_ENFORCE(false, "NaN input in sqrt_fp:", a);
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<T, MathError>(unexpect, MathError::NaN);
        }
        else
        {
            return std::numeric_limits<T>::quiet_NaN();
        }
    }

    if (a < 0)
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            FATP_ALWAYS_ENFORCE(false, "Negative input in sqrt_fp:", a);
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<T, MathError>(unexpect, MathError::InvalidArgument);
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy>)
        {
            return T{0};
        }
        else if constexpr (std::is_same_v<Policy, InfTolerantPolicy>)
        {
            return std::numeric_limits<T>::quiet_NaN();
        }
    }

    return std::sqrt(a);
}

/**
 * @brief Checked floating-point floor
 */
template <typename Policy = ThrowOnErrorPolicy, FATP_ENABLE_IF_FLOATING>
[[nodiscard]] PolicyReturnType<Policy, T> checked_floor_fp(T a) noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    if (std::isnan(a))
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            FATP_ALWAYS_ENFORCE(false, "NaN input in floor_fp:", a);
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<T, MathError>(unexpect, MathError::NaN);
        }
        else
        {
            return std::numeric_limits<T>::quiet_NaN();
        }
    }

    return std::floor(a);
}

/**
 * @brief Checked floating-point ceiling
 */
template <typename Policy = ThrowOnErrorPolicy, FATP_ENABLE_IF_FLOATING>
[[nodiscard]] PolicyReturnType<Policy, T> checked_ceil_fp(T a) noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    if (std::isnan(a))
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            FATP_ALWAYS_ENFORCE(false, "NaN input in ceil_fp:", a);
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<T, MathError>(unexpect, MathError::NaN);
        }
        else
        {
            return std::numeric_limits<T>::quiet_NaN();
        }
    }

    return std::ceil(a);
}

/**
 * @brief Checked floating-point truncation
 */
template <typename Policy = ThrowOnErrorPolicy, FATP_ENABLE_IF_FLOATING>
[[nodiscard]] PolicyReturnType<Policy, T> checked_trunc_fp(T a) noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    if (std::isnan(a))
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            FATP_ALWAYS_ENFORCE(false, "NaN input in trunc_fp:", a);
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<T, MathError>(unexpect, MathError::NaN);
        }
        else
        {
            return std::numeric_limits<T>::quiet_NaN();
        }
    }

    return std::trunc(a);
}

/**
 * @brief Checked floating-point rounding
 */
template <typename Policy = ThrowOnErrorPolicy, FATP_ENABLE_IF_FLOATING>
[[nodiscard]] PolicyReturnType<Policy, T> checked_round_fp(T a) noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    if (std::isnan(a))
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            FATP_ALWAYS_ENFORCE(false, "NaN input in round_fp:", a);
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<T, MathError>(unexpect, MathError::NaN);
        }
        else
        {
            return std::numeric_limits<T>::quiet_NaN();
        }
    }

    return std::round(a);
}

// =============================================================================
// SIMD Error Detection Helpers
// =============================================================================

namespace detail
{

#if defined(__AVX2__) && (defined(__x86_64__) || defined(_M_X64))
/**
 * @brief Detects NaN or overflow-to-Inf in AVX2 double vector results
 */
template <typename Policy>
[[nodiscard]] inline bool detect_fp_simd_error(const __m256d& va, const __m256d& vb, const __m256d& vr) noexcept
{
    // Check for NaN in result
    __m256d nan_mask = _mm256_cmp_pd(vr, vr, _CMP_UNORD_Q);
    if (_mm256_movemask_pd(nan_mask) != 0)
    {
        return true;
    }

    // For non-InfTolerant policies, check for overflow
    if constexpr (!std::is_same_v<Policy, InfTolerantPolicy>)
    {
        __m256d inf_pos = _mm256_set1_pd(std::numeric_limits<double>::infinity());
        __m256d inf_neg = _mm256_set1_pd(-std::numeric_limits<double>::infinity());
        __m256d result_is_inf =
            _mm256_or_pd(_mm256_cmp_pd(vr, inf_pos, _CMP_EQ_OQ), _mm256_cmp_pd(vr, inf_neg, _CMP_EQ_OQ));

        __m256d a_is_finite = _mm256_and_pd(
            _mm256_cmp_pd(va, va, _CMP_ORD_Q),
            _mm256_and_pd(_mm256_cmp_pd(va, inf_pos, _CMP_NEQ_OQ), _mm256_cmp_pd(va, inf_neg, _CMP_NEQ_OQ)));

        __m256d b_is_finite = _mm256_and_pd(
            _mm256_cmp_pd(vb, vb, _CMP_ORD_Q),
            _mm256_and_pd(_mm256_cmp_pd(vb, inf_pos, _CMP_NEQ_OQ), _mm256_cmp_pd(vb, inf_neg, _CMP_NEQ_OQ)));

        __m256d overflow = _mm256_and_pd(result_is_inf, _mm256_and_pd(a_is_finite, b_is_finite));

        if (_mm256_movemask_pd(overflow) != 0)
        {
            return true;
        }
    }
    return false;
}
#endif

/**
 * @brief Architecture-agnostic FP error detection using SimdVector
 *
 * Works on SSE, AVX, AVX-512, NEON via SimdVector abstraction.
 */
template <typename Policy, typename T>
[[nodiscard]] inline bool
detect_fp_simd_error_generic(const SimdVector<T>& va, const SimdVector<T>& vb, const SimdVector<T>& vr) noexcept
{
    // Check for NaN in result
    if (vr.has_nan())
    {
        return true;
    }

    // For non-InfTolerant policies, check for overflow (finite->Inf)
    if constexpr (!std::is_same_v<Policy, InfTolerantPolicy>)
    {
        if (vr.has_inf())
        {
            if (va.all_finite() && vb.all_finite())
            {
                return true;
            }
        }
    }
    return false;
}

} // namespace detail

// =============================================================================
// Vector Floating-Point Operations (SIMD-Accelerated)
// =============================================================================

/**
 * @brief SIMD-accelerated checked vector addition for floating-point
 *
 * Uses SimdVector for portable SIMD across SSE, AVX, AVX-512, NEON.
 */
template <typename Policy = ThrowOnErrorPolicy, typename T>
[[nodiscard]] PolicyReturnType<Policy, std::vector<T>>
checked_add_vec_fp(const std::vector<T>& vec_a,
                   const std::vector<T>& vec_b) noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    static_assert(std::is_floating_point_v<T>, "checked_add_vec_fp requires floating-point types");

    if (vec_a.size() != vec_b.size())
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            FATP_ALWAYS_ENFORCE(false, "Vector size mismatch in FP addition");
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

    using VecT = SimdVector<T>;
    constexpr size_t vec_width = VecT::width;

    size_t i = 0;

    // SIMD main loop
    for (; i + vec_width <= n; i += vec_width)
    {
        auto va = VecT::load_unaligned(&vec_a[i]);
        auto vb = VecT::load_unaligned(&vec_b[i]);
        auto vr = va + vb;

        bool has_error = detail::detect_fp_simd_error_generic<Policy>(va, vb, vr);

        if (has_error)
        {
            // Scalar fallback for error classification
            for (size_t j = i; j < i + vec_width && j < n; ++j)
            {
                auto temp = checked_add_fp<Policy>(vec_a[j], vec_b[j]);
                if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
                {
                    if (!temp.has_value())
                    {
                        return Expected<std::vector<T>, MathError>(unexpect, temp.error());
                    }
                    result[j] = temp.value();
                }
                else
                {
                    result[j] = temp;
                }
            }
        }
        else
        {
            vr.store_unaligned(&result[i]);
        }
    }

    // Scalar cleanup
    for (; i < n; ++i)
    {
        auto temp = checked_add_fp<Policy>(vec_a[i], vec_b[i]);
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
 * @brief SIMD-accelerated checked vector subtraction for floating-point
 */
template <typename Policy = ThrowOnErrorPolicy, typename T>
[[nodiscard]] PolicyReturnType<Policy, std::vector<T>>
checked_sub_vec_fp(const std::vector<T>& vec_a,
                   const std::vector<T>& vec_b) noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    static_assert(std::is_floating_point_v<T>, "checked_sub_vec_fp requires floating-point types");

    if (vec_a.size() != vec_b.size())
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            FATP_ALWAYS_ENFORCE(false, "Vector size mismatch in FP subtraction");
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

    using VecT = SimdVector<T>;
    constexpr size_t vec_width = VecT::width;

    size_t i = 0;

    for (; i + vec_width <= n; i += vec_width)
    {
        auto va = VecT::load_unaligned(&vec_a[i]);
        auto vb = VecT::load_unaligned(&vec_b[i]);
        auto vr = va - vb;

        bool has_error = detail::detect_fp_simd_error_generic<Policy>(va, vb, vr);

        if (has_error)
        {
            for (size_t j = i; j < i + vec_width && j < n; ++j)
            {
                auto temp = checked_sub_fp<Policy>(vec_a[j], vec_b[j]);
                if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
                {
                    if (!temp.has_value())
                    {
                        return Expected<std::vector<T>, MathError>(unexpect, temp.error());
                    }
                    result[j] = temp.value();
                }
                else
                {
                    result[j] = temp;
                }
            }
        }
        else
        {
            vr.store_unaligned(&result[i]);
        }
    }

    for (; i < n; ++i)
    {
        auto temp = checked_sub_fp<Policy>(vec_a[i], vec_b[i]);
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
 * @brief SIMD-accelerated checked vector multiplication for floating-point
 */
template <typename Policy = ThrowOnErrorPolicy, typename T>
[[nodiscard]] PolicyReturnType<Policy, std::vector<T>>
checked_mul_vec_fp(const std::vector<T>& vec_a,
                   const std::vector<T>& vec_b) noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    static_assert(std::is_floating_point_v<T>, "checked_mul_vec_fp requires floating-point types");

    if (vec_a.size() != vec_b.size())
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            FATP_ALWAYS_ENFORCE(false, "Vector size mismatch in FP multiplication");
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

    using VecT = SimdVector<T>;
    constexpr size_t vec_width = VecT::width;

    size_t i = 0;

    for (; i + vec_width <= n; i += vec_width)
    {
        auto va = VecT::load_unaligned(&vec_a[i]);
        auto vb = VecT::load_unaligned(&vec_b[i]);
        auto vr = va * vb;

        bool has_error = detail::detect_fp_simd_error_generic<Policy>(va, vb, vr);

        if (has_error)
        {
            for (size_t j = i; j < i + vec_width && j < n; ++j)
            {
                auto temp = checked_mul_fp<Policy>(vec_a[j], vec_b[j]);
                if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
                {
                    if (!temp.has_value())
                    {
                        return Expected<std::vector<T>, MathError>(unexpect, temp.error());
                    }
                    result[j] = temp.value();
                }
                else
                {
                    result[j] = temp;
                }
            }
        }
        else
        {
            vr.store_unaligned(&result[i]);
        }
    }

    for (; i < n; ++i)
    {
        auto temp = checked_mul_fp<Policy>(vec_a[i], vec_b[i]);
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
 * @brief SIMD-accelerated checked vector division for floating-point
 */
template <typename Policy = ThrowOnErrorPolicy, typename T>
[[nodiscard]] PolicyReturnType<Policy, std::vector<T>>
checked_div_vec_fp(const std::vector<T>& vec_a,
                   const std::vector<T>& vec_b) noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    static_assert(std::is_floating_point_v<T>, "checked_div_vec_fp requires floating-point types");

    if (vec_a.size() != vec_b.size())
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            FATP_ALWAYS_ENFORCE(false, "Vector size mismatch in FP division");
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

    using VecT = SimdVector<T>;
    constexpr size_t vec_width = VecT::width;

    size_t i = 0;

    for (; i + vec_width <= n; i += vec_width)
    {
        auto va = VecT::load_unaligned(&vec_a[i]);
        auto vb = VecT::load_unaligned(&vec_b[i]);
        auto vr = va / vb;

        bool has_error = detail::detect_fp_simd_error_generic<Policy>(va, vb, vr);

        if (has_error)
        {
            for (size_t j = i; j < i + vec_width && j < n; ++j)
            {
                auto temp = checked_div_fp<Policy>(vec_a[j], vec_b[j]);
                if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
                {
                    if (!temp.has_value())
                    {
                        return Expected<std::vector<T>, MathError>(unexpect, temp.error());
                    }
                    result[j] = temp.value();
                }
                else
                {
                    result[j] = temp;
                }
            }
        }
        else
        {
            vr.store_unaligned(&result[i]);
        }
    }

    for (; i < n; ++i)
    {
        auto temp = checked_div_fp<Policy>(vec_a[i], vec_b[i]);
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

// =============================================================================
// Alignment-Aware Vector Operations (HPC Container Support)
// =============================================================================
// These overloads accept a result container template parameter, enabling
// use of HPC containers like AlignedVector for optimized aligned stores.
// Uses the has_assume_aligned_v trait from CheckedArithmeticBase.h.

/**
 * @brief Alignment-aware checked vector addition for floating-point
 *
 * When ResultVec provides assume_aligned() (e.g., AlignedVector), uses
 * aligned SIMD stores for better performance. Falls back to unaligned
 * stores for std::vector.
 *
 * @tparam Policy Error handling policy
 * @tparam T Floating-point element type (float or double)
 * @tparam ResultVec Output container type (default: std::vector<T>)
 * @param vec_a First input vector
 * @param vec_b Second input vector
 * @return ResultVec with element-wise sum, or Expected if Policy requires
 *
 * Example with AlignedVector:
 * @code
 *   AlignedVector<double> a(1000), b(1000);
 *   // ... fill a and b ...
 *   auto result = checked_add_vec_fp_aligned<SaturatingPolicy, double,
 *                                            AlignedVector<double>>(a, b);
 *   // Uses aligned stores internally
 * @endcode
 */
template <typename Policy = ThrowOnErrorPolicy,
          typename T,
          typename ResultVec = std::vector<T>,
          typename InputVec = std::vector<T>>
[[nodiscard]] PolicyReturnType<Policy, ResultVec>
checked_add_vec_fp_aligned(const InputVec& vec_a,
                           const InputVec& vec_b) noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    static_assert(std::is_floating_point_v<T>, "checked_add_vec_fp_aligned requires floating-point types");

    if (vec_a.size() != vec_b.size())
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            FATP_ALWAYS_ENFORCE(false, "Vector size mismatch in FP addition");
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<ResultVec, MathError>(unexpect, MathError::InvalidArgument);
        }
        else
        {
            return ResultVec();
        }
    }

    size_t n = vec_a.size();
    ResultVec result(n);

    // Get appropriately aligned pointer for stores
    T* res_ptr = get_aligned_ptr(result);

    using VecT = SimdVector<T>;
    constexpr size_t vec_width = VecT::width;

    size_t i = 0;

    // SIMD main loop with alignment-aware stores
    for (; i + vec_width <= n; i += vec_width)
    {
        auto va = VecT::load_unaligned(&vec_a[i]);
        auto vb = VecT::load_unaligned(&vec_b[i]);
        auto vr = va + vb;

        bool has_error = detail::detect_fp_simd_error_generic<Policy>(va, vb, vr);

        if (has_error)
        {
            for (size_t j = i; j < i + vec_width && j < n; ++j)
            {
                auto temp = checked_add_fp<Policy>(vec_a[j], vec_b[j]);
                if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
                {
                    if (!temp.has_value())
                    {
                        return Expected<ResultVec, MathError>(unexpect, temp.error());
                    }
                    res_ptr[j] = temp.value();
                }
                else
                {
                    res_ptr[j] = temp;
                }
            }
        }
        else
        {
            // Use aligned store if container supports it
            if constexpr (is_alignment_aware_v<ResultVec>)
            {
                vr.store_aligned(res_ptr + i);
            }
            else
            {
                vr.store_unaligned(res_ptr + i);
            }
        }
    }

    // Scalar cleanup
    for (; i < n; ++i)
    {
        auto temp = checked_add_fp<Policy>(vec_a[i], vec_b[i]);
        if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            if (!temp.has_value())
            {
                return Expected<ResultVec, MathError>(unexpect, temp.error());
            }
            res_ptr[i] = temp.value();
        }
        else
        {
            res_ptr[i] = temp;
        }
    }

    return result;
}

/**
 * @brief Alignment-aware checked vector subtraction for floating-point
 */
template <typename Policy = ThrowOnErrorPolicy,
          typename T,
          typename ResultVec = std::vector<T>,
          typename InputVec = std::vector<T>>
[[nodiscard]] PolicyReturnType<Policy, ResultVec>
checked_sub_vec_fp_aligned(const InputVec& vec_a,
                           const InputVec& vec_b) noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    static_assert(std::is_floating_point_v<T>, "checked_sub_vec_fp_aligned requires floating-point types");

    if (vec_a.size() != vec_b.size())
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            FATP_ALWAYS_ENFORCE(false, "Vector size mismatch in FP subtraction");
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<ResultVec, MathError>(unexpect, MathError::InvalidArgument);
        }
        else
        {
            return ResultVec();
        }
    }

    size_t n = vec_a.size();
    ResultVec result(n);
    T* res_ptr = get_aligned_ptr(result);

    using VecT = SimdVector<T>;
    constexpr size_t vec_width = VecT::width;

    size_t i = 0;

    for (; i + vec_width <= n; i += vec_width)
    {
        auto va = VecT::load_unaligned(&vec_a[i]);
        auto vb = VecT::load_unaligned(&vec_b[i]);
        auto vr = va - vb;

        bool has_error = detail::detect_fp_simd_error_generic<Policy>(va, vb, vr);

        if (has_error)
        {
            for (size_t j = i; j < i + vec_width && j < n; ++j)
            {
                auto temp = checked_sub_fp<Policy>(vec_a[j], vec_b[j]);
                if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
                {
                    if (!temp.has_value())
                    {
                        return Expected<ResultVec, MathError>(unexpect, temp.error());
                    }
                    res_ptr[j] = temp.value();
                }
                else
                {
                    res_ptr[j] = temp;
                }
            }
        }
        else
        {
            if constexpr (is_alignment_aware_v<ResultVec>)
            {
                vr.store_aligned(res_ptr + i);
            }
            else
            {
                vr.store_unaligned(res_ptr + i);
            }
        }
    }

    for (; i < n; ++i)
    {
        auto temp = checked_sub_fp<Policy>(vec_a[i], vec_b[i]);
        if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            if (!temp.has_value())
            {
                return Expected<ResultVec, MathError>(unexpect, temp.error());
            }
            res_ptr[i] = temp.value();
        }
        else
        {
            res_ptr[i] = temp;
        }
    }

    return result;
}

/**
 * @brief Alignment-aware checked vector multiplication for floating-point
 */
template <typename Policy = ThrowOnErrorPolicy,
          typename T,
          typename ResultVec = std::vector<T>,
          typename InputVec = std::vector<T>>
[[nodiscard]] PolicyReturnType<Policy, ResultVec>
checked_mul_vec_fp_aligned(const InputVec& vec_a,
                           const InputVec& vec_b) noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    static_assert(std::is_floating_point_v<T>, "checked_mul_vec_fp_aligned requires floating-point types");

    if (vec_a.size() != vec_b.size())
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            FATP_ALWAYS_ENFORCE(false, "Vector size mismatch in FP multiplication");
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<ResultVec, MathError>(unexpect, MathError::InvalidArgument);
        }
        else
        {
            return ResultVec();
        }
    }

    size_t n = vec_a.size();
    ResultVec result(n);
    T* res_ptr = get_aligned_ptr(result);

    using VecT = SimdVector<T>;
    constexpr size_t vec_width = VecT::width;

    size_t i = 0;

    for (; i + vec_width <= n; i += vec_width)
    {
        auto va = VecT::load_unaligned(&vec_a[i]);
        auto vb = VecT::load_unaligned(&vec_b[i]);
        auto vr = va * vb;

        bool has_error = detail::detect_fp_simd_error_generic<Policy>(va, vb, vr);

        if (has_error)
        {
            for (size_t j = i; j < i + vec_width && j < n; ++j)
            {
                auto temp = checked_mul_fp<Policy>(vec_a[j], vec_b[j]);
                if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
                {
                    if (!temp.has_value())
                    {
                        return Expected<ResultVec, MathError>(unexpect, temp.error());
                    }
                    res_ptr[j] = temp.value();
                }
                else
                {
                    res_ptr[j] = temp;
                }
            }
        }
        else
        {
            if constexpr (is_alignment_aware_v<ResultVec>)
            {
                vr.store_aligned(res_ptr + i);
            }
            else
            {
                vr.store_unaligned(res_ptr + i);
            }
        }
    }

    for (; i < n; ++i)
    {
        auto temp = checked_mul_fp<Policy>(vec_a[i], vec_b[i]);
        if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            if (!temp.has_value())
            {
                return Expected<ResultVec, MathError>(unexpect, temp.error());
            }
            res_ptr[i] = temp.value();
        }
        else
        {
            res_ptr[i] = temp;
        }
    }

    return result;
}

/**
 * @brief Alignment-aware checked vector division for floating-point
 */
template <typename Policy = ThrowOnErrorPolicy,
          typename T,
          typename ResultVec = std::vector<T>,
          typename InputVec = std::vector<T>>
[[nodiscard]] PolicyReturnType<Policy, ResultVec>
checked_div_vec_fp_aligned(const InputVec& vec_a,
                           const InputVec& vec_b) noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    static_assert(std::is_floating_point_v<T>, "checked_div_vec_fp_aligned requires floating-point types");

    if (vec_a.size() != vec_b.size())
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            FATP_ALWAYS_ENFORCE(false, "Vector size mismatch in FP division");
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<ResultVec, MathError>(unexpect, MathError::InvalidArgument);
        }
        else
        {
            return ResultVec();
        }
    }

    size_t n = vec_a.size();
    ResultVec result(n);
    T* res_ptr = get_aligned_ptr(result);

    using VecT = SimdVector<T>;
    constexpr size_t vec_width = VecT::width;

    size_t i = 0;

    for (; i + vec_width <= n; i += vec_width)
    {
        auto va = VecT::load_unaligned(&vec_a[i]);
        auto vb = VecT::load_unaligned(&vec_b[i]);
        auto vr = va / vb;

        bool has_error = detail::detect_fp_simd_error_generic<Policy>(va, vb, vr);

        if (has_error)
        {
            for (size_t j = i; j < i + vec_width && j < n; ++j)
            {
                auto temp = checked_div_fp<Policy>(vec_a[j], vec_b[j]);
                if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
                {
                    if (!temp.has_value())
                    {
                        return Expected<ResultVec, MathError>(unexpect, temp.error());
                    }
                    res_ptr[j] = temp.value();
                }
                else
                {
                    res_ptr[j] = temp;
                }
            }
        }
        else
        {
            if constexpr (is_alignment_aware_v<ResultVec>)
            {
                vr.store_aligned(res_ptr + i);
            }
            else
            {
                vr.store_unaligned(res_ptr + i);
            }
        }
    }

    for (; i < n; ++i)
    {
        auto temp = checked_div_fp<Policy>(vec_a[i], vec_b[i]);
        if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            if (!temp.has_value())
            {
                return Expected<ResultVec, MathError>(unexpect, temp.error());
            }
            res_ptr[i] = temp.value();
        }
        else
        {
            res_ptr[i] = temp;
        }
    }

    return result;
}

// =============================================================================
// Macro Cleanup
// =============================================================================

#undef FATP_VALIDATE_FP_INPUTS

} // namespace fat_p
