/**
 * @file CheckedArithmetic_IntSimd_NEON.h
 * @brief NEON (128-bit) integer SIMD acceleration for checked arithmetic
 * @version 1.1 (Fixed brace style)
 *
 * Provides 4-wide int32 and 2-wide int64 (AArch64 only) acceleration.
 *
 * Key Technique: Differential Saturation
 * - Compute both wrapping and saturating results
 * - If they differ, overflow occurred
 * - This is uniquely efficient on NEON which has native saturating arithmetic
 *
 * SaturatingPolicy Optimization:
 * - Maps directly to hardware vqadd/vqsub instructions (zero overhead)
 */

#pragma once

#include "CheckedArithmetic_IntSimd_Common.h"

#if defined(FATP_INT_SIMD_NEON)

namespace fat_p {
namespace int_simd {

// =============================================================================
// NEON Differential Saturation: int32_t (4-wide)
// =============================================================================

/**
 * @brief SIMD accelerated checked add for int32_t vectors using NEON
 *
 * Uses differential saturation: compare wrapping vs saturating result.
 * If they differ, overflow occurred in that lane.
 */
template<typename Policy, typename ScalarAddFn>
size_t neon_checked_add_i32(
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
        int32x4_t va = vld1q_s32(a + i);
        int32x4_t vb = vld1q_s32(b + i);

        // SaturatingPolicy fast path: use hardware saturation directly
        if constexpr (std::is_same_v<Policy, fat_p::SaturatingPolicy>)
        {
            int32x4_t vs = vqaddq_s32(va, vb);  // Hardware saturating add
            vst1q_s32(result + i, vs);
            continue;
        }

        // Other policies: detect overflow via differential saturation
        int32x4_t vr = vaddq_s32(va, vb);       // Wrapping add
        int32x4_t vs = vqaddq_s32(va, vb);      // Saturating add

        if (neon_any_differ_i32(vr, vs))
        {
            // Overflow detected - scalar fallback for error classification
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
            vst1q_s32(result + i, vr);
        }
    }

    return i;
}

/**
 * @brief SIMD accelerated checked add for uint32_t vectors using NEON
 */
template<typename Policy, typename ScalarAddFn>
size_t neon_checked_add_u32(
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
        uint32x4_t va = vld1q_u32(a + i);
        uint32x4_t vb = vld1q_u32(b + i);

        if constexpr (std::is_same_v<Policy, fat_p::SaturatingPolicy>)
        {
            uint32x4_t vs = vqaddq_u32(va, vb);
            vst1q_u32(result + i, vs);
            continue;
        }

        uint32x4_t vr = vaddq_u32(va, vb);
        uint32x4_t vs = vqaddq_u32(va, vb);

        if (neon_any_differ_u32(vr, vs))
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
            vst1q_u32(result + i, vr);
        }
    }

    return i;
}

// =============================================================================
// NEON Differential Saturation: int32_t Sub
// =============================================================================

template<typename Policy, typename ScalarSubFn>
size_t neon_checked_sub_i32(
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
        int32x4_t va = vld1q_s32(a + i);
        int32x4_t vb = vld1q_s32(b + i);

        if constexpr (std::is_same_v<Policy, fat_p::SaturatingPolicy>)
        {
            int32x4_t vs = vqsubq_s32(va, vb);
            vst1q_s32(result + i, vs);
            continue;
        }

        int32x4_t vr = vsubq_s32(va, vb);
        int32x4_t vs = vqsubq_s32(va, vb);

        if (neon_any_differ_i32(vr, vs))
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
            vst1q_s32(result + i, vr);
        }
    }

    return i;
}

template<typename Policy, typename ScalarSubFn>
size_t neon_checked_sub_u32(
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
        uint32x4_t va = vld1q_u32(a + i);
        uint32x4_t vb = vld1q_u32(b + i);

        if constexpr (std::is_same_v<Policy, fat_p::SaturatingPolicy>)
        {
            uint32x4_t vs = vqsubq_u32(va, vb);
            vst1q_u32(result + i, vs);
            continue;
        }

        uint32x4_t vr = vsubq_u32(va, vb);
        uint32x4_t vs = vqsubq_u32(va, vb);

        if (neon_any_differ_u32(vr, vs))
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
            vst1q_u32(result + i, vr);
        }
    }

    return i;
}

// =============================================================================
// NEON AArch64: int64_t (2-wide)
// =============================================================================

#if FATP_INT_SIMD_NEON_AARCH64

template<typename Policy, typename ScalarAddFn>
size_t neon_checked_add_i64(
    const int64_t* a,
    const int64_t* b,
    int64_t* result,
    size_t n,
    size_t start_idx,
    ScalarAddFn checked_add_scalar)
{
    constexpr size_t LANES = 2;
    size_t i = start_idx;

    for (; i + LANES <= n; i += LANES)
    {
        int64x2_t va = vld1q_s64(a + i);
        int64x2_t vb = vld1q_s64(b + i);

        if constexpr (std::is_same_v<Policy, fat_p::SaturatingPolicy>)
        {
            int64x2_t vs = vqaddq_s64(va, vb);
            vst1q_s64(result + i, vs);
            continue;
        }

        int64x2_t vr = vaddq_s64(va, vb);
        int64x2_t vs = vqaddq_s64(va, vb);

        if (neon_any_differ_i64(vr, vs))
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
            vst1q_s64(result + i, vr);
        }
    }

    return i;
}

template<typename Policy, typename ScalarSubFn>
size_t neon_checked_sub_i64(
    const int64_t* a,
    const int64_t* b,
    int64_t* result,
    size_t n,
    size_t start_idx,
    ScalarSubFn checked_sub_scalar)
{
    constexpr size_t LANES = 2;
    size_t i = start_idx;

    for (; i + LANES <= n; i += LANES)
    {
        int64x2_t va = vld1q_s64(a + i);
        int64x2_t vb = vld1q_s64(b + i);

        if constexpr (std::is_same_v<Policy, fat_p::SaturatingPolicy>)
        {
            int64x2_t vs = vqsubq_s64(va, vb);
            vst1q_s64(result + i, vs);
            continue;
        }

        int64x2_t vr = vsubq_s64(va, vb);
        int64x2_t vs = vqsubq_s64(va, vb);

        if (neon_any_differ_i64(vr, vs))
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
            vst1q_s64(result + i, vr);
        }
    }

    return i;
}

#endif // FATP_INT_SIMD_NEON_AARCH64

// =============================================================================
// NEON Vector Mul (int32_t - 4-wide) using Wide Multiply
// =============================================================================

/**
 * @brief Check if any 64-bit lane exceeds int32 range
 */
inline bool neon_mul_overflow_i32(int64x2_t wide_lo, int64x2_t wide_hi) noexcept
{
    int64x2_t max32 = vdupq_n_s64(INT32_MAX);
    int64x2_t min32 = vdupq_n_s64(INT32_MIN);

    // Check lo lanes
    uint64x2_t lo_too_big = vcgtq_s64(wide_lo, max32);
    uint64x2_t lo_too_small = vcltq_s64(wide_lo, min32);
    uint64x2_t lo_overflow = vorrq_u64(lo_too_big, lo_too_small);

    // Check hi lanes
    uint64x2_t hi_too_big = vcgtq_s64(wide_hi, max32);
    uint64x2_t hi_too_small = vcltq_s64(wide_hi, min32);
    uint64x2_t hi_overflow = vorrq_u64(hi_too_big, hi_too_small);

    uint64x2_t any_overflow = vorrq_u64(lo_overflow, hi_overflow);
    return (vgetq_lane_u64(any_overflow, 0) != 0) || (vgetq_lane_u64(any_overflow, 1) != 0);
}

/**
 * @brief Check if any 64-bit lane exceeds uint32 range
 */
inline bool neon_mul_overflow_u32(uint64x2_t wide_lo, uint64x2_t wide_hi) noexcept
{
    uint64x2_t max32 = vdupq_n_u64(UINT32_MAX);

    uint64x2_t lo_overflow = vcgtq_u64(wide_lo, max32);
    uint64x2_t hi_overflow = vcgtq_u64(wide_hi, max32);

    uint64x2_t any_overflow = vorrq_u64(lo_overflow, hi_overflow);
    return (vgetq_lane_u64(any_overflow, 0) != 0) || (vgetq_lane_u64(any_overflow, 1) != 0);
}

template<typename Policy, typename ScalarMulFn>
size_t neon_checked_mul_i32(
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
        int32x4_t va = vld1q_s32(a + i);
        int32x4_t vb = vld1q_s32(b + i);

        // Wide multiply: int32 * int32 -> int64
        // vmull_s32 multiplies low 2 lanes, vmull_high_s32 multiplies high 2 lanes
        int64x2_t wide_lo = vmull_s32(vget_low_s32(va), vget_low_s32(vb));
        int64x2_t wide_hi = vmull_high_s32(va, vb);

        if constexpr (std::is_same_v<Policy, fat_p::SaturatingPolicy>)
        {
            // Clamp to int32 range and narrow
            int64x2_t max32 = vdupq_n_s64(INT32_MAX);
            int64x2_t min32 = vdupq_n_s64(INT32_MIN);

            wide_lo = vminq_s64(vmaxq_s64(wide_lo, min32), max32);
            wide_hi = vminq_s64(vmaxq_s64(wide_hi, min32), max32);

            // Narrow to 32-bit
            int32x2_t narrow_lo = vmovn_s64(wide_lo);
            int32x2_t narrow_hi = vmovn_s64(wide_hi);
            int32x4_t vr = vcombine_s32(narrow_lo, narrow_hi);

            vst1q_s32(result + i, vr);
            continue;
        }

        if (neon_mul_overflow_i32(wide_lo, wide_hi))
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
            // No overflow - narrow and store
            int32x2_t narrow_lo = vmovn_s64(wide_lo);
            int32x2_t narrow_hi = vmovn_s64(wide_hi);
            int32x4_t vr = vcombine_s32(narrow_lo, narrow_hi);
            vst1q_s32(result + i, vr);
        }
    }

    return i;
}

template<typename Policy, typename ScalarMulFn>
size_t neon_checked_mul_u32(
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
        uint32x4_t va = vld1q_u32(a + i);
        uint32x4_t vb = vld1q_u32(b + i);

        // Wide multiply: uint32 * uint32 -> uint64
        uint64x2_t wide_lo = vmull_u32(vget_low_u32(va), vget_low_u32(vb));
        uint64x2_t wide_hi = vmull_high_u32(va, vb);

        if constexpr (std::is_same_v<Policy, fat_p::SaturatingPolicy>)
        {
            // Clamp to uint32 range and narrow
            uint64x2_t max32 = vdupq_n_u64(UINT32_MAX);

            wide_lo = vminq_u64(wide_lo, max32);
            wide_hi = vminq_u64(wide_hi, max32);

            uint32x2_t narrow_lo = vmovn_u64(wide_lo);
            uint32x2_t narrow_hi = vmovn_u64(wide_hi);
            uint32x4_t vr = vcombine_u32(narrow_lo, narrow_hi);

            vst1q_u32(result + i, vr);
            continue;
        }

        if (neon_mul_overflow_u32(wide_lo, wide_hi))
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
            uint32x2_t narrow_lo = vmovn_u64(wide_lo);
            uint32x2_t narrow_hi = vmovn_u64(wide_hi);
            uint32x4_t vr = vcombine_u32(narrow_lo, narrow_hi);
            vst1q_u32(result + i, vr);
        }
    }

    return i;
}

} // namespace int_simd
} // namespace fat_p

#endif // FATP_INT_SIMD_NEON
