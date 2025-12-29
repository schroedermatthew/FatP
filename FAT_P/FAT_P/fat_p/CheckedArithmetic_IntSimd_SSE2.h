/**
 * @file CheckedArithmetic_IntSimd_SSE2.h
 * @brief SSE2 (128-bit) integer SIMD acceleration for checked arithmetic
 * @version 1.1 (Fixed brace style)
 *
 * Provides 4-wide int32 and 2-wide int64 acceleration for add/sub operations.
 * Uses detect -> scalar fallback pattern for proper error classification.
 *
 * Overflow Detection:
 * - Signed: (a^r) & (b^r) with sign bit check
 * - Unsigned: result < operand (via bias trick for SIMD compare)
 */

#pragma once

#include "CheckedArithmetic_IntSimd_Common.h"

#if defined(FATP_INT_SIMD_SSE2)

namespace fat_p {
namespace int_simd {

// =============================================================================
// SSE2 Signed Overflow Detection
// =============================================================================

/**
 * @brief Compute signed add overflow mask (32-bit lanes)
 *
 * Overflow occurs when:
 * - pos + pos = neg (positive overflow)
 * - neg + neg = pos (negative overflow)
 *
 * Equivalent to: (a^r) & (b^r) has sign bit set
 */
inline __m128i sse2_add_overflow_mask_i32(__m128i a, __m128i b, __m128i r) noexcept
{
    __m128i xor_ar = _mm_xor_si128(a, r);
    __m128i xor_br = _mm_xor_si128(b, r);
    return _mm_and_si128(xor_ar, xor_br);
    // Caller checks sign bits via sse2_any_overflow_i32()
}

/**
 * @brief Compute signed sub overflow mask (32-bit lanes)
 *
 * Overflow occurs when a and b have different signs AND result sign differs from a.
 * Equivalent to: (a^b) & (a^r) has sign bit set
 */
inline __m128i sse2_sub_overflow_mask_i32(__m128i a, __m128i b, __m128i r) noexcept
{
    __m128i xor_ab = _mm_xor_si128(a, b);
    __m128i xor_ar = _mm_xor_si128(a, r);
    return _mm_and_si128(xor_ab, xor_ar);
}

// =============================================================================
// SSE2 Unsigned Overflow Detection (via bias trick)
// =============================================================================

/**
 * @brief Compute unsigned add overflow mask (32-bit lanes)
 *
 * Unsigned overflow: result < a
 * SSE2 only has signed compare, so we bias by 0x80000000 to convert to signed domain.
 */
inline __m128i sse2_add_overflow_mask_u32(__m128i a, __m128i /*b*/, __m128i r) noexcept
{
    __m128i bias = _mm_set1_epi32(static_cast<int>(0x80000000u));
    __m128i a_biased = _mm_xor_si128(a, bias);
    __m128i r_biased = _mm_xor_si128(r, bias);
    // r < a in unsigned domain == r_biased < a_biased in signed domain
    return _mm_cmpgt_epi32(a_biased, r_biased);
}

/**
 * @brief Compute unsigned sub overflow mask (32-bit lanes)
 *
 * Unsigned underflow: a < b
 */
inline __m128i sse2_sub_overflow_mask_u32(__m128i a, __m128i b, __m128i /*r*/) noexcept
{
    __m128i bias = _mm_set1_epi32(static_cast<int>(0x80000000u));
    __m128i a_biased = _mm_xor_si128(a, bias);
    __m128i b_biased = _mm_xor_si128(b, bias);
    return _mm_cmpgt_epi32(b_biased, a_biased);
}

// =============================================================================
// SSE2 Vector Add (int32_t)
// =============================================================================

/**
 * @brief SIMD accelerated checked add for int32_t vectors
 *
 * @tparam Policy Error handling policy (ThrowOnError, ReturnExpected, Saturating)
 * @tparam ScalarAddFn Scalar checked_add function type
 * @param vec_a First operand vector
 * @param vec_b Second operand vector
 * @param checked_add_scalar Scalar fallback function
 * @param result Output vector (pre-sized)
 * @param start_idx Starting index for processing
 * @return Index after last processed element, or negative if error detected
 *
 * Pattern: Process SIMD chunks, detect overflow, fall back to scalar on error.
 */
template<typename Policy, typename ScalarAddFn>
size_t sse2_checked_add_i32(
    const int32_t* a,
    const int32_t* b,
    int32_t* result,
    size_t n,
    size_t start_idx,
    ScalarAddFn checked_add_scalar)
{
    constexpr size_t LANES = 4;
    size_t i = start_idx;

    for (; i + LANES <= n; i += LANES)
    {
        __m128i va = _mm_loadu_si128(reinterpret_cast<const __m128i*>(a + i));
        __m128i vb = _mm_loadu_si128(reinterpret_cast<const __m128i*>(b + i));
        __m128i vr = _mm_add_epi32(va, vb);

        // Compute overflow mask
        __m128i overflow_mask = sse2_add_overflow_mask_i32(va, vb, vr);

        if (sse2_any_overflow_i32(overflow_mask))
        {
            // Overflow detected - process this chunk with scalar for proper error handling
            for (size_t j = i; j < i + LANES; ++j)
            {
                auto r = checked_add_scalar(a[j], b[j]);
                if constexpr (std::is_same_v<Policy, fat_p::ReturnExpectedPolicy>)
                {
                    if (!r.has_value())
                    {
                        return static_cast<size_t>(-1);  // Signal error
                    }
                    result[j] = r.value();
                }
                else
                {
                    result[j] = r;
                }
            }
        }
        else
        {
            // No overflow - store SIMD result
            _mm_storeu_si128(reinterpret_cast<__m128i*>(result + i), vr);
        }
    }

    return i;  // Return index for tail processing
}

/**
 * @brief SIMD accelerated checked add for uint32_t vectors
 */
template<typename Policy, typename ScalarAddFn>
size_t sse2_checked_add_u32(
    const uint32_t* a,
    const uint32_t* b,
    uint32_t* result,
    size_t n,
    size_t start_idx,
    ScalarAddFn checked_add_scalar)
{
    constexpr size_t LANES = 4;
    size_t i = start_idx;

    for (; i + LANES <= n; i += LANES)
    {
        __m128i va = _mm_loadu_si128(reinterpret_cast<const __m128i*>(a + i));
        __m128i vb = _mm_loadu_si128(reinterpret_cast<const __m128i*>(b + i));
        __m128i vr = _mm_add_epi32(va, vb);  // Same instruction for signed/unsigned

        __m128i overflow_mask = sse2_add_overflow_mask_u32(va, vb, vr);

        if (_mm_movemask_epi8(overflow_mask) != 0)
        {
            // Scalar fallback
            for (size_t j = i; j < i + LANES; ++j)
            {
                auto r = checked_add_scalar(a[j], b[j]);
                if constexpr (std::is_same_v<Policy, fat_p::ReturnExpectedPolicy>)
                {
                    if (!r.has_value())
                    {
                        return static_cast<size_t>(-1);
                    }
                    result[j] = r.value();
                }
                else
                {
                    result[j] = r;
                }
            }
        }
        else
        {
            _mm_storeu_si128(reinterpret_cast<__m128i*>(result + i), vr);
        }
    }

    return i;
}

// =============================================================================
// SSE2 Vector Sub (int32_t)
// =============================================================================

template<typename Policy, typename ScalarSubFn>
size_t sse2_checked_sub_i32(
    const int32_t* a,
    const int32_t* b,
    int32_t* result,
    size_t n,
    size_t start_idx,
    ScalarSubFn checked_sub_scalar)
{
    constexpr size_t LANES = 4;
    size_t i = start_idx;

    for (; i + LANES <= n; i += LANES)
    {
        __m128i va = _mm_loadu_si128(reinterpret_cast<const __m128i*>(a + i));
        __m128i vb = _mm_loadu_si128(reinterpret_cast<const __m128i*>(b + i));
        __m128i vr = _mm_sub_epi32(va, vb);

        __m128i overflow_mask = sse2_sub_overflow_mask_i32(va, vb, vr);

        if (sse2_any_overflow_i32(overflow_mask))
        {
            for (size_t j = i; j < i + LANES; ++j)
            {
                auto r = checked_sub_scalar(a[j], b[j]);
                if constexpr (std::is_same_v<Policy, fat_p::ReturnExpectedPolicy>)
                {
                    if (!r.has_value())
                    {
                        return static_cast<size_t>(-1);
                    }
                    result[j] = r.value();
                }
                else
                {
                    result[j] = r;
                }
            }
        }
        else
        {
            _mm_storeu_si128(reinterpret_cast<__m128i*>(result + i), vr);
        }
    }

    return i;
}

template<typename Policy, typename ScalarSubFn>
size_t sse2_checked_sub_u32(
    const uint32_t* a,
    const uint32_t* b,
    uint32_t* result,
    size_t n,
    size_t start_idx,
    ScalarSubFn checked_sub_scalar)
{
    constexpr size_t LANES = 4;
    size_t i = start_idx;

    for (; i + LANES <= n; i += LANES)
    {
        __m128i va = _mm_loadu_si128(reinterpret_cast<const __m128i*>(a + i));
        __m128i vb = _mm_loadu_si128(reinterpret_cast<const __m128i*>(b + i));
        __m128i vr = _mm_sub_epi32(va, vb);

        __m128i overflow_mask = sse2_sub_overflow_mask_u32(va, vb, vr);

        if (_mm_movemask_epi8(overflow_mask) != 0)
        {
            for (size_t j = i; j < i + LANES; ++j)
            {
                auto r = checked_sub_scalar(a[j], b[j]);
                if constexpr (std::is_same_v<Policy, fat_p::ReturnExpectedPolicy>)
                {
                    if (!r.has_value())
                    {
                        return static_cast<size_t>(-1);
                    }
                    result[j] = r.value();
                }
                else
                {
                    result[j] = r;
                }
            }
        }
        else
        {
            _mm_storeu_si128(reinterpret_cast<__m128i*>(result + i), vr);
        }
    }

    return i;
}

// =============================================================================
// SSE2 Vector Mul (int32_t - 4-wide) using Wide Multiply
// =============================================================================

/**
 * @brief Check if any 64-bit lane exceeds int32 range (SSE2)
 * @note SSE2 lacks _mm_cmpgt_epi64, so we use scalar check after extraction
 */
inline bool sse2_mul_overflow_i32(__m128i wide_even, __m128i wide_odd) noexcept
{
    // Extract 64-bit values and check bounds
    int64_t e0 = _mm_cvtsi128_si64(wide_even);
    int64_t e1 = _mm_cvtsi128_si64(_mm_srli_si128(wide_even, 8));
    int64_t o0 = _mm_cvtsi128_si64(wide_odd);
    int64_t o1 = _mm_cvtsi128_si64(_mm_srli_si128(wide_odd, 8));

    return (e0 > INT32_MAX || e0 < INT32_MIN ||
            e1 > INT32_MAX || e1 < INT32_MIN ||
            o0 > INT32_MAX || o0 < INT32_MIN ||
            o1 > INT32_MAX || o1 < INT32_MIN);
}

/**
 * @brief Check if any 64-bit lane exceeds uint32 range (SSE2)
 */
inline bool sse2_mul_overflow_u32(__m128i wide_even, __m128i wide_odd) noexcept
{
    uint64_t e0 = static_cast<uint64_t>(_mm_cvtsi128_si64(wide_even));
    uint64_t e1 = static_cast<uint64_t>(_mm_cvtsi128_si64(_mm_srli_si128(wide_even, 8)));
    uint64_t o0 = static_cast<uint64_t>(_mm_cvtsi128_si64(wide_odd));
    uint64_t o1 = static_cast<uint64_t>(_mm_cvtsi128_si64(_mm_srli_si128(wide_odd, 8)));

    return (e0 > UINT32_MAX || e1 > UINT32_MAX || o0 > UINT32_MAX || o1 > UINT32_MAX);
}

/**
 * @brief Narrow 64-bit products to 32-bit and interleave (SSE2)
 * @param wide_even [e0, e1] as 64-bit (products for lanes 0, 2)
 * @param wide_odd [o0, o1] as 64-bit (products for lanes 1, 3)
 * @return [e0_lo, o0_lo, e1_lo, o1_lo] as 32-bit
 */
inline __m128i sse2_narrow_and_interleave(__m128i wide_even, __m128i wide_odd) noexcept
{
    // Interleave at 64-bit granularity: [e0, o0]
    __m128i lo = _mm_unpacklo_epi64(wide_even, wide_odd);
    // hi would be [e1, o1]
    __m128i hi = _mm_unpackhi_epi64(wide_even, wide_odd);

    // Use shuffle_ps to pick low 32 bits
    // lo as 32-bit: [e0_lo, e0_hi, o0_lo, o0_hi]
    // hi as 32-bit: [e1_lo, e1_hi, o1_lo, o1_hi]
    __m128 lo_f = _mm_castsi128_ps(lo);
    __m128 hi_f = _mm_castsi128_ps(hi);
    __m128 result_f = _mm_shuffle_ps(lo_f, hi_f, _MM_SHUFFLE(2, 0, 2, 0));
    // Result: [e0_lo, o0_lo, e1_lo, o1_lo]

    return _mm_castps_si128(result_f);
}

/**
 * @brief SSE2 signed 32-bit multiply with overflow detection
 *
 * @note SSE2 LIMITATION: SSE2 lacks signed 32->64 multiply (_mm_mul_epi32 is SSE4.1).
 *       _mm_mul_epu32 only handles unsigned and only lanes 0,2.
 *       For signed int32 with correct overflow detection, we must use scalar
 *       widening multiplication. This function processes 4 elements per iteration
 *       for cache efficiency, but computation is scalar.
 *
 * @note For true SIMD multiplication, compile with SSE4.1 or AVX2 flags.
 */
template<typename Policy, typename ScalarMulFn>
size_t sse2_checked_mul_i32(
    const int32_t* a,
    const int32_t* b,
    int32_t* result,
    size_t n,
    size_t start_idx,
    ScalarMulFn checked_mul_scalar)
{
    constexpr size_t LANES = 4;
    size_t i = start_idx;

    for (; i + LANES <= n; i += LANES)
    {
        // Scalar widening multiply - SSE2 lacks signed 32->64 multiply
        // Process 4 elements per iteration for cache/prefetch efficiency
        int64_t p0 = static_cast<int64_t>(a[i])   * static_cast<int64_t>(b[i]);
        int64_t p1 = static_cast<int64_t>(a[i+1]) * static_cast<int64_t>(b[i+1]);
        int64_t p2 = static_cast<int64_t>(a[i+2]) * static_cast<int64_t>(b[i+2]);
        int64_t p3 = static_cast<int64_t>(a[i+3]) * static_cast<int64_t>(b[i+3]);

        bool overflow = (p0 > INT32_MAX || p0 < INT32_MIN ||
                         p1 > INT32_MAX || p1 < INT32_MIN ||
                         p2 > INT32_MAX || p2 < INT32_MIN ||
                         p3 > INT32_MAX || p3 < INT32_MIN);

        if (overflow)
        {
            for (size_t j = i; j < i + LANES; ++j)
            {
                auto r = checked_mul_scalar(a[j], b[j]);
                if constexpr (std::is_same_v<Policy, fat_p::ReturnExpectedPolicy>)
                {
                    if (!r.has_value())
                    {
                        return static_cast<size_t>(-1);
                    }
                    result[j] = r.value();
                }
                else
                {
                    result[j] = r;
                }
            }
        }
        else
        {
            result[i]   = static_cast<int32_t>(p0);
            result[i+1] = static_cast<int32_t>(p1);
            result[i+2] = static_cast<int32_t>(p2);
            result[i+3] = static_cast<int32_t>(p3);
        }
    }

    return i;
}

template<typename Policy, typename ScalarMulFn>
size_t sse2_checked_mul_u32(
    const uint32_t* a,
    const uint32_t* b,
    uint32_t* result,
    size_t n,
    size_t start_idx,
    ScalarMulFn checked_mul_scalar)
{
    constexpr size_t LANES = 4;
    size_t i = start_idx;

    for (; i + LANES <= n; i += LANES)
    {
        __m128i va = _mm_loadu_si128(reinterpret_cast<const __m128i*>(a + i));
        __m128i vb = _mm_loadu_si128(reinterpret_cast<const __m128i*>(b + i));

        // Wide multiply: uint32 * uint32 -> uint64
        __m128i wide_even = _mm_mul_epu32(va, vb);

        __m128i va_odd = _mm_srli_epi64(va, 32);
        __m128i vb_odd = _mm_srli_epi64(vb, 32);
        __m128i wide_odd = _mm_mul_epu32(va_odd, vb_odd);

        if (sse2_mul_overflow_u32(wide_even, wide_odd))
        {
            for (size_t j = i; j < i + LANES; ++j)
            {
                auto r = checked_mul_scalar(a[j], b[j]);
                if constexpr (std::is_same_v<Policy, fat_p::ReturnExpectedPolicy>)
                {
                    if (!r.has_value())
                    {
                        return static_cast<size_t>(-1);
                    }
                    result[j] = r.value();
                }
                else
                {
                    result[j] = r;
                }
            }
        }
        else
        {
            __m128i vr = sse2_narrow_and_interleave(wide_even, wide_odd);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(result + i), vr);
        }
    }

    return i;
}

} // namespace int_simd
} // namespace fat_p

#endif // FATP_INT_SIMD_SSE2
