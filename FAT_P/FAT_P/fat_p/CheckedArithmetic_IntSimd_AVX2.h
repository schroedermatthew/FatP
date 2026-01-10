/**
 * @file CheckedArithmetic_IntSimd_AVX2.h
 * @brief AVX2 (256-bit) integer SIMD acceleration for checked arithmetic
 * @version 1.2 (Added uint64_t add, fixed brace style)
 *
 * Provides 8-wide int32 and 4-wide int64 acceleration for add/sub/mul operations.
 * Uses detect -> scalar fallback pattern for proper error classification.
 *
 * Multiplication uses wide-multiply approach:
 * - int32 * int32 -> int64 intermediate
 * - Bounds check against INT32_MIN/MAX
 * - No intermediate overflow possible
 */

#pragma once

/*
FATP_META:
  meta_version: 1
  component: CheckedArithmetic_IntSimd_AVX2
  file_role: public_header
  path: fat_p/CheckedArithmetic_IntSimd_AVX2.h
  namespace: fat_p
  summary: "Public header for CheckedArithmetic_IntSimd_AVX2."
  api_stability: in_work
  related:
    docs_search: "CheckedArithmetic_IntSimd_AVX2"
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
#include "CheckedArithmetic_IntSimd_Common.h"

#if defined(FATP_INT_SIMD_AVX2)

namespace fat_p {
namespace int_simd {

// =============================================================================
// AVX2 Signed Overflow Detection
// =============================================================================

/**
 * @brief Compute signed add overflow mask (32-bit lanes, 8-wide)
 */
inline __m256i avx2_add_overflow_mask_i32(__m256i a, __m256i b, __m256i r) noexcept
{
    __m256i xor_ar = _mm256_xor_si256(a, r);
    __m256i xor_br = _mm256_xor_si256(b, r);
    return _mm256_and_si256(xor_ar, xor_br);
}

/**
 * @brief Compute signed sub overflow mask (32-bit lanes, 8-wide)
 */
inline __m256i avx2_sub_overflow_mask_i32(__m256i a, __m256i b, __m256i r) noexcept
{
    __m256i xor_ab = _mm256_xor_si256(a, b);
    __m256i xor_ar = _mm256_xor_si256(a, r);
    return _mm256_and_si256(xor_ab, xor_ar);
}

// =============================================================================
// AVX2 Unsigned Overflow Detection
// =============================================================================

/**
 * @brief Compute unsigned add overflow mask (32-bit lanes, 8-wide)
 */
inline __m256i avx2_add_overflow_mask_u32(__m256i a, __m256i /*b*/, __m256i r) noexcept
{
    __m256i bias = _mm256_set1_epi32(static_cast<int>(0x80000000u));
    __m256i a_biased = _mm256_xor_si256(a, bias);
    __m256i r_biased = _mm256_xor_si256(r, bias);
    return _mm256_cmpgt_epi32(a_biased, r_biased);
}

/**
 * @brief Compute unsigned sub overflow mask (32-bit lanes, 8-wide)
 */
inline __m256i avx2_sub_overflow_mask_u32(__m256i a, __m256i b, __m256i /*r*/) noexcept
{
    __m256i bias = _mm256_set1_epi32(static_cast<int>(0x80000000u));
    __m256i a_biased = _mm256_xor_si256(a, bias);
    __m256i b_biased = _mm256_xor_si256(b, bias);
    return _mm256_cmpgt_epi32(b_biased, a_biased);
}

// =============================================================================
// AVX2 64-bit Overflow Detection
// =============================================================================

inline __m256i avx2_add_overflow_mask_i64(__m256i a, __m256i b, __m256i r) noexcept
{
    __m256i xor_ar = _mm256_xor_si256(a, r);
    __m256i xor_br = _mm256_xor_si256(b, r);
    return _mm256_and_si256(xor_ar, xor_br);
}

inline __m256i avx2_sub_overflow_mask_i64(__m256i a, __m256i b, __m256i r) noexcept
{
    __m256i xor_ab = _mm256_xor_si256(a, b);
    __m256i xor_ar = _mm256_xor_si256(a, r);
    return _mm256_and_si256(xor_ab, xor_ar);
}

inline __m256i avx2_add_overflow_mask_u64(__m256i a, __m256i /*b*/, __m256i r) noexcept
{
    __m256i bias = _mm256_set1_epi64x(static_cast<int64_t>(0x8000000000000000ULL));
    __m256i a_biased = _mm256_xor_si256(a, bias);
    __m256i r_biased = _mm256_xor_si256(r, bias);
    return _mm256_cmpgt_epi64(a_biased, r_biased);
}

inline __m256i avx2_sub_overflow_mask_u64(__m256i a, __m256i b, __m256i /*r*/) noexcept
{
    __m256i bias = _mm256_set1_epi64x(static_cast<int64_t>(0x8000000000000000ULL));
    __m256i a_biased = _mm256_xor_si256(a, bias);
    __m256i b_biased = _mm256_xor_si256(b, bias);
    return _mm256_cmpgt_epi64(b_biased, a_biased);
}

// =============================================================================
// AVX2 Vector Add (int32_t - 8-wide)
// =============================================================================

template<typename Policy, typename ScalarAddFn>
size_t avx2_checked_add_i32(
    const int32_t* a,
    const int32_t* b,
    int32_t* result,
    size_t n,
    size_t start_idx,
    ScalarAddFn checked_add_scalar)
{
    constexpr size_t LANES = 8;
    size_t i = start_idx;

    for (; i + LANES <= n; i += LANES)
    {
        __m256i va = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(a + i));
        __m256i vb = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(b + i));
        __m256i vr = _mm256_add_epi32(va, vb);

        __m256i overflow_mask = avx2_add_overflow_mask_i32(va, vb, vr);

        if (avx2_any_overflow_i32(overflow_mask))
        {
            // Scalar fallback for this chunk
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
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(result + i), vr);
        }
    }

    return i;
}

template<typename Policy, typename ScalarAddFn>
size_t avx2_checked_add_u32(
    const uint32_t* a,
    const uint32_t* b,
    uint32_t* result,
    size_t n,
    size_t start_idx,
    ScalarAddFn checked_add_scalar)
{
    constexpr size_t LANES = 8;
    size_t i = start_idx;

    for (; i + LANES <= n; i += LANES)
    {
        __m256i va = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(a + i));
        __m256i vb = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(b + i));
        __m256i vr = _mm256_add_epi32(va, vb);

        __m256i overflow_mask = avx2_add_overflow_mask_u32(va, vb, vr);

        if (_mm256_movemask_epi8(overflow_mask) != 0)
        {
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
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(result + i), vr);
        }
    }

    return i;
}

// =============================================================================
// AVX2 Vector Add (int64_t - 4-wide)
// =============================================================================

template<typename Policy, typename ScalarAddFn>
size_t avx2_checked_add_i64(
    const int64_t* a,
    const int64_t* b,
    int64_t* result,
    size_t n,
    size_t start_idx,
    ScalarAddFn checked_add_scalar)
{
    constexpr size_t LANES = 4;
    size_t i = start_idx;

    for (; i + LANES <= n; i += LANES)
    {
        __m256i va = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(a + i));
        __m256i vb = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(b + i));
        __m256i vr = _mm256_add_epi64(va, vb);

        __m256i overflow_mask = avx2_add_overflow_mask_i64(va, vb, vr);

        if (avx2_any_overflow_i64(overflow_mask))
        {
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
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(result + i), vr);
        }
    }

    return i;
}

// =============================================================================
// AVX2 Vector Add (uint64_t - 4-wide)
// =============================================================================

template<typename Policy, typename ScalarAddFn>
size_t avx2_checked_add_u64(
    const uint64_t* a,
    const uint64_t* b,
    uint64_t* result,
    size_t n,
    size_t start_idx,
    ScalarAddFn checked_add_scalar)
{
    constexpr size_t LANES = 4;
    size_t i = start_idx;

    for (; i + LANES <= n; i += LANES)
    {
        __m256i va = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(a + i));
        __m256i vb = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(b + i));
        __m256i vr = _mm256_add_epi64(va, vb);

        __m256i overflow_mask = avx2_add_overflow_mask_u64(va, vb, vr);

        if (_mm256_movemask_epi8(overflow_mask) != 0)
        {
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
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(result + i), vr);
        }
    }

    return i;
}

// =============================================================================
// AVX2 Vector Sub (int32_t - 8-wide)
// =============================================================================

template<typename Policy, typename ScalarSubFn>
size_t avx2_checked_sub_i32(
    const int32_t* a,
    const int32_t* b,
    int32_t* result,
    size_t n,
    size_t start_idx,
    ScalarSubFn checked_sub_scalar)
{
    constexpr size_t LANES = 8;
    size_t i = start_idx;

    for (; i + LANES <= n; i += LANES)
    {
        __m256i va = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(a + i));
        __m256i vb = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(b + i));
        __m256i vr = _mm256_sub_epi32(va, vb);

        __m256i overflow_mask = avx2_sub_overflow_mask_i32(va, vb, vr);

        if (avx2_any_overflow_i32(overflow_mask))
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
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(result + i), vr);
        }
    }

    return i;
}

template<typename Policy, typename ScalarSubFn>
size_t avx2_checked_sub_u32(
    const uint32_t* a,
    const uint32_t* b,
    uint32_t* result,
    size_t n,
    size_t start_idx,
    ScalarSubFn checked_sub_scalar)
{
    constexpr size_t LANES = 8;
    size_t i = start_idx;

    for (; i + LANES <= n; i += LANES)
    {
        __m256i va = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(a + i));
        __m256i vb = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(b + i));
        __m256i vr = _mm256_sub_epi32(va, vb);

        __m256i overflow_mask = avx2_sub_overflow_mask_u32(va, vb, vr);

        if (_mm256_movemask_epi8(overflow_mask) != 0)
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
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(result + i), vr);
        }
    }

    return i;
}

// =============================================================================
// AVX2 Vector Mul (int32_t - 8-wide) using Wide Multiply
// =============================================================================

/**
 * @brief Check if any 64-bit lane exceeds int32 range
 * @param wide_even 64-bit products for even lanes (0,2,4,6)
 * @param wide_odd 64-bit products for odd lanes (1,3,5,7)
 * @return true if any overflow detected
 *
 * Wide multiply approach: int32 * int32 -> int64, then check bounds.
 */
inline bool avx2_mul_overflow_i32(__m256i wide_even, __m256i wide_odd) noexcept
{
    __m256i max32 = _mm256_set1_epi64x(INT32_MAX);
    __m256i min32 = _mm256_set1_epi64x(INT32_MIN);

    // Check even lanes
    __m256i even_too_big = _mm256_cmpgt_epi64(wide_even, max32);
    __m256i even_too_small = _mm256_cmpgt_epi64(min32, wide_even);
    __m256i even_overflow = _mm256_or_si256(even_too_big, even_too_small);

    // Check odd lanes
    __m256i odd_too_big = _mm256_cmpgt_epi64(wide_odd, max32);
    __m256i odd_too_small = _mm256_cmpgt_epi64(min32, wide_odd);
    __m256i odd_overflow = _mm256_or_si256(odd_too_big, odd_too_small);

    __m256i any_overflow = _mm256_or_si256(even_overflow, odd_overflow);
    return _mm256_movemask_epi8(any_overflow) != 0;
}

/**
 * @brief Check if any 64-bit lane exceeds uint32 range
 */
inline bool avx2_mul_overflow_u32(__m256i wide_even, __m256i wide_odd) noexcept
{
    __m256i max32 = _mm256_set1_epi64x(static_cast<int64_t>(UINT32_MAX));

    // For unsigned, use signed compare with biased values
    // Since UINT32_MAX = 0xFFFFFFFF fits in int64, we can use signed compare
    // But we need to check if wide > UINT32_MAX
    // Using unsigned comparison via bias trick for 64-bit
    __m256i bias = _mm256_set1_epi64x(static_cast<int64_t>(0x8000000000000000ULL));
    __m256i max32_biased = _mm256_xor_si256(max32, bias);

    __m256i even_biased = _mm256_xor_si256(wide_even, bias);
    __m256i odd_biased = _mm256_xor_si256(wide_odd, bias);

    __m256i even_overflow = _mm256_cmpgt_epi64(even_biased, max32_biased);
    __m256i odd_overflow = _mm256_cmpgt_epi64(odd_biased, max32_biased);

    __m256i any_overflow = _mm256_or_si256(even_overflow, odd_overflow);
    return _mm256_movemask_epi8(any_overflow) != 0;
}

/**
 * @brief Narrow 64-bit products to 32-bit and interleave even/odd lanes
 * @param wide_even 64-bit products for lanes 0,2,4,6
 * @param wide_odd 64-bit products for lanes 1,3,5,7
 * @return Interleaved 32-bit results [r0,r1,r2,r3,r4,r5,r6,r7]
 *
 * Layout:
 *   wide_even = [e0, e1, e2, e3] as int64 (e0=a0*b0, e1=a2*b2, e2=a4*b4, e3=a6*b6)
 *   wide_odd  = [o0, o1, o2, o3] as int64 (o0=a1*b1, o1=a3*b3, o2=a5*b5, o3=a7*b7)
 *   Result needs: [e0_lo, o0_lo, e1_lo, o1_lo, e2_lo, o2_lo, e3_lo, o3_lo]
 */
inline __m256i avx2_narrow_and_interleave(__m256i wide_even, __m256i wide_odd) noexcept
{
    // Step 1: Interleave at 64-bit granularity within each 128-bit lane
    // unpacklo: [e0, o0 | e2, o2] as 64-bit
    // unpackhi: [e1, o1 | e3, o3] as 64-bit
    __m256i lo = _mm256_unpacklo_epi64(wide_even, wide_odd);
    __m256i hi = _mm256_unpackhi_epi64(wide_even, wide_odd);

    // Step 2: Use shuffle_ps to pick low 32 bits from each 64-bit slot
    // lo as 32-bit: [e0_lo, e0_hi, o0_lo, o0_hi | e2_lo, e2_hi, o2_lo, o2_hi]
    // hi as 32-bit: [e1_lo, e1_hi, o1_lo, o1_hi | e3_lo, e3_hi, o3_lo, o3_hi]
    // shuffle_ps(lo, hi, _MM_SHUFFLE(2,0,2,0)) per 128-bit lane:
    //   result = [lo[0], lo[2], hi[0], hi[2]] = [e0_lo, o0_lo, e1_lo, o1_lo]
    __m256 lo_f = _mm256_castsi256_ps(lo);
    __m256 hi_f = _mm256_castsi256_ps(hi);
    __m256 result_f = _mm256_shuffle_ps(lo_f, hi_f, _MM_SHUFFLE(2, 0, 2, 0));

    return _mm256_castps_si256(result_f);
}

template<typename Policy, typename ScalarMulFn>
size_t avx2_checked_mul_i32(
    const int32_t* a,
    const int32_t* b,
    int32_t* result,
    size_t n,
    size_t start_idx,
    ScalarMulFn checked_mul_scalar)
{
    constexpr size_t LANES = 8;
    size_t i = start_idx;

    for (; i + LANES <= n; i += LANES)
    {
        __m256i va = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(a + i));
        __m256i vb = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(b + i));

        // Wide multiply: int32 * int32 -> int64
        // _mm256_mul_epi32 multiplies low 32-bits of each 64-bit lane
        __m256i wide_even = _mm256_mul_epi32(va, vb);  // lanes 0, 2, 4, 6

        // Shift to get odd lanes into position, then multiply
        __m256i va_odd = _mm256_srli_epi64(va, 32);
        __m256i vb_odd = _mm256_srli_epi64(vb, 32);
        __m256i wide_odd = _mm256_mul_epi32(va_odd, vb_odd);  // lanes 1, 3, 5, 7

        if (avx2_mul_overflow_i32(wide_even, wide_odd))
        {
            // Scalar fallback for this chunk
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
            // Narrow and store
            __m256i vr = avx2_narrow_and_interleave(wide_even, wide_odd);
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(result + i), vr);
        }
    }

    return i;
}

template<typename Policy, typename ScalarMulFn>
size_t avx2_checked_mul_u32(
    const uint32_t* a,
    const uint32_t* b,
    uint32_t* result,
    size_t n,
    size_t start_idx,
    ScalarMulFn checked_mul_scalar)
{
    constexpr size_t LANES = 8;
    size_t i = start_idx;

    for (; i + LANES <= n; i += LANES)
    {
        __m256i va = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(a + i));
        __m256i vb = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(b + i));

        // Wide multiply: uint32 * uint32 -> uint64
        __m256i wide_even = _mm256_mul_epu32(va, vb);  // lanes 0, 2, 4, 6

        __m256i va_odd = _mm256_srli_epi64(va, 32);
        __m256i vb_odd = _mm256_srli_epi64(vb, 32);
        __m256i wide_odd = _mm256_mul_epu32(va_odd, vb_odd);  // lanes 1, 3, 5, 7

        if (avx2_mul_overflow_u32(wide_even, wide_odd))
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
            __m256i vr = avx2_narrow_and_interleave(wide_even, wide_odd);
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(result + i), vr);
        }
    }

    return i;
}

} // namespace int_simd
} // namespace fat_p

#endif // FATP_INT_SIMD_AVX2
