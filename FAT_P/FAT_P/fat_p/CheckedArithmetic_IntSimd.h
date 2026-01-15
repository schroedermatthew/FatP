/**
 * @file CheckedArithmetic_IntSimd.h
 * @brief Unified integer SIMD dispatch for checked arithmetic operations
 *
 *
 * @layer Foundation
 *
 * @version 1.2 (Added uint64_t dispatch, fixed documentation)
 *
 * This header provides the unified API for integer SIMD acceleration.
 * It automatically selects the best available implementation:
 * - AVX2 (8-wide int32, 4-wide int64) on x86-64
 * - SSE2 (4-wide int32, 2-wide int64) on x86/x86-64 fallback
 * - NEON (4-wide int32, 2-wide int64 on AArch64) on ARM
 * - Scalar fallback on other platforms
 *
 * Supported Operations:
 * - Add: Overflow via XOR sign-bit check or differential saturation
 * - Sub: Overflow via XOR sign-bit check or differential saturation
 * - Mul: Wide-multiply (int32*int32 -> int64) with bounds checking
 *
 * Supported Types:
 * - int32_t, uint32_t: Full SIMD acceleration (add/sub/mul)
 * - int64_t: Partial acceleration (add on AVX2/NEON, sub on NEON)
 * - uint64_t: Limited acceleration (add on AVX2)
 *
 * Intentionally NOT Supported:
 * - int8_t, uint8_t, int16_t, uint16_t
 *   Rationale:
 *   1. Wide-multiply explosion: int8*int8 -> int16 requires unpack/repack stages
 *   2. No _mm256_mul_epi8: Must unpack to 16-bit, multiply, repack
 *   3. C++ promotion semantics: int8*int8 promotes to int, overflow on narrowing
 *   4. Saturation usually desired: Image/audio wants clamp-to-max, not errors
 *   5. Native saturation exists: vqaddq_u8 (NEON), _mm_adds_epu8 (SSE)
 *   6. Minimal HPC demand: Scientific computing uses int32/int64/float/double
 *
 * Design Pattern: detect -> scalar fallback
 * - SIMD fast path for bulk processing
 * - Overflow detection using architecture-specific techniques
 * - Scalar fallback for proper error classification per policy
 *
 * Architecture Highlights:
 * - SSE2/AVX2: Overflow via (a^r)&(b^r) sign-bit check
 * - NEON: Differential saturation (wrapping vs saturating compare)
 * - SaturatingPolicy on NEON: Zero overhead via hardware vqadd/vqsub
 * - Mul: vmull_s32/vmull_high_s32 (NEON), _mm256_mul_epi32 (AVX2)
 */

#pragma once

/*
FATP_META:
  meta_version: 1
  component: CheckedArithmetic_IntSimd
  file_role: public_header
  path: fat_p/CheckedArithmetic_IntSimd.h
  namespace: fat_p
  layer: Foundation
  summary: "Public header for CheckedArithmetic_IntSimd."
  api_stability: in_work
  related:
    docs_search: "CheckedArithmetic_IntSimd"
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
#include "CheckedArithmetic_IntSimd_AVX2.h"
#endif

#if defined(FATP_INT_SIMD_SSE2)
#include "CheckedArithmetic_IntSimd_SSE2.h"
#endif

#if defined(FATP_INT_SIMD_NEON)
#include "CheckedArithmetic_IntSimd_NEON.h"
#endif

namespace fat_p
{
namespace int_simd
{

// =============================================================================
// Unified Dispatch: checked_add_vec_simd
// =============================================================================

/**
 * @brief SIMD-accelerated checked vector addition
 *
 * @tparam T Integer type (int32_t, uint32_t, int64_t, uint64_t)
 * @tparam Policy Error handling policy
 * @tparam ScalarAddFn Scalar checked_add function type
 * @param a First operand array
 * @param b Second operand array
 * @param result Output array (must be pre-allocated with size n)
 * @param n Number of elements
 * @param checked_add_scalar Scalar fallback function
 * @return true on success, false if error detected (for ReturnExpectedPolicy)
 *
 * Usage:
 *   std::vector<int32_t> a(1000), b(1000), result(1000);
 *   bool ok = checked_add_vec_simd<int32_t, ReturnExpectedPolicy>(
 *       a.data(), b.data(), result.data(), 1000,
 *       [](int32_t x, int32_t y) { return checked_add<ReturnExpectedPolicy>(x, y); }
 *   );
 */
template <typename T, typename Policy, typename ScalarAddFn>
bool checked_add_vec_simd(const T* a, const T* b, T* result, size_t n, ScalarAddFn checked_add_scalar)
{
    size_t i = 0;

    // Architecture dispatch for int32_t
    if constexpr (std::is_same_v<T, int32_t>)
    {
#if defined(FATP_INT_SIMD_AVX2)
        i = avx2_checked_add_i32<Policy>(a, b, result, n, i, checked_add_scalar);
#elif defined(FATP_INT_SIMD_SSE2)
        i = sse2_checked_add_i32<Policy>(a, b, result, n, i, checked_add_scalar);
#elif defined(FATP_INT_SIMD_NEON)
        i = neon_checked_add_i32<Policy>(a, b, result, n, i, checked_add_scalar);
#endif
    }
    else if constexpr (std::is_same_v<T, uint32_t>)
    {
#if defined(FATP_INT_SIMD_AVX2)
        i = avx2_checked_add_u32<Policy>(a, b, result, n, i, checked_add_scalar);
#elif defined(FATP_INT_SIMD_SSE2)
        i = sse2_checked_add_u32<Policy>(a, b, result, n, i, checked_add_scalar);
#elif defined(FATP_INT_SIMD_NEON)
        i = neon_checked_add_u32<Policy>(a, b, result, n, i, checked_add_scalar);
#endif
    }
    else if constexpr (std::is_same_v<T, int64_t>)
    {
#if defined(FATP_INT_SIMD_AVX2)
        i = avx2_checked_add_i64<Policy>(a, b, result, n, i, checked_add_scalar);
#elif defined(FATP_INT_SIMD_NEON) && FATP_INT_SIMD_NEON_AARCH64
        i = neon_checked_add_i64<Policy>(a, b, result, n, i, checked_add_scalar);
#endif
        // SSE2 int64 falls through to scalar (no efficient cmpgt)
    }
    else if constexpr (std::is_same_v<T, uint64_t>)
    {
#if defined(FATP_INT_SIMD_AVX2)
        i = avx2_checked_add_u64<Policy>(a, b, result, n, i, checked_add_scalar);
#endif
        // SSE2/NEON uint64 falls through to scalar
    }

    // Check for error signal from SIMD path
    if (i == static_cast<size_t>(-1))
    {
        return false; // Error occurred
    }

    // Tail processing (scalar)
    for (; i < n; ++i)
    {
        auto r = checked_add_scalar(a[i], b[i]);
        if constexpr (std::is_same_v<Policy, fat_p::ReturnExpectedPolicy>)
        {
            if (!r.has_value())
            {
                return false;
            }
            result[i] = r.value();
        }
        else
        {
            result[i] = r;
        }
    }

    return true;
}

// =============================================================================
// Unified Dispatch: checked_sub_vec_simd
// =============================================================================

template <typename T, typename Policy, typename ScalarSubFn>
bool checked_sub_vec_simd(const T* a, const T* b, T* result, size_t n, ScalarSubFn checked_sub_scalar)
{
    size_t i = 0;

    if constexpr (std::is_same_v<T, int32_t>)
    {
#if defined(FATP_INT_SIMD_AVX2)
        i = avx2_checked_sub_i32<Policy>(a, b, result, n, i, checked_sub_scalar);
#elif defined(FATP_INT_SIMD_SSE2)
        i = sse2_checked_sub_i32<Policy>(a, b, result, n, i, checked_sub_scalar);
#elif defined(FATP_INT_SIMD_NEON)
        i = neon_checked_sub_i32<Policy>(a, b, result, n, i, checked_sub_scalar);
#endif
    }
    else if constexpr (std::is_same_v<T, uint32_t>)
    {
#if defined(FATP_INT_SIMD_AVX2)
        i = avx2_checked_sub_u32<Policy>(a, b, result, n, i, checked_sub_scalar);
#elif defined(FATP_INT_SIMD_SSE2)
        i = sse2_checked_sub_u32<Policy>(a, b, result, n, i, checked_sub_scalar);
#elif defined(FATP_INT_SIMD_NEON)
        i = neon_checked_sub_u32<Policy>(a, b, result, n, i, checked_sub_scalar);
#endif
    }
    else if constexpr (std::is_same_v<T, int64_t>)
    {
#if defined(FATP_INT_SIMD_NEON) && FATP_INT_SIMD_NEON_AARCH64
        i = neon_checked_sub_i64<Policy>(a, b, result, n, i, checked_sub_scalar);
#endif
        // AVX2/SSE2 int64 sub falls through to scalar for now
    }
    // uint64_t sub falls through to scalar (no SIMD implementation)

    if (i == static_cast<size_t>(-1))
    {
        return false;
    }

    // Tail processing
    for (; i < n; ++i)
    {
        auto r = checked_sub_scalar(a[i], b[i]);
        if constexpr (std::is_same_v<Policy, fat_p::ReturnExpectedPolicy>)
        {
            if (!r.has_value())
            {
                return false;
            }
            result[i] = r.value();
        }
        else
        {
            result[i] = r;
        }
    }

    return true;
}

// =============================================================================
// Unified Dispatch: checked_mul_vec_simd
// =============================================================================

/**
 * @brief SIMD-accelerated checked vector multiplication
 *
 * Uses wide-multiply approach: int32*int32 -> int64, then check bounds.
 * This guarantees no intermediate overflow and correct detection.
 *
 * @tparam T Integer type (int32_t, uint32_t supported; int64_t scalar only)
 * @tparam Policy Error handling policy
 * @tparam ScalarMulFn Scalar checked_mul function type
 * @return true on success, false if error detected (for ReturnExpectedPolicy)
 */
template <typename T, typename Policy, typename ScalarMulFn>
bool checked_mul_vec_simd(const T* a, const T* b, T* result, size_t n, ScalarMulFn checked_mul_scalar)
{
    size_t i = 0;

    // Architecture dispatch for int32_t
    if constexpr (std::is_same_v<T, int32_t>)
    {
#if defined(FATP_INT_SIMD_AVX2)
        i = avx2_checked_mul_i32<Policy>(a, b, result, n, i, checked_mul_scalar);
#elif defined(FATP_INT_SIMD_SSE2)
        i = sse2_checked_mul_i32<Policy>(a, b, result, n, i, checked_mul_scalar);
#elif defined(FATP_INT_SIMD_NEON)
        i = neon_checked_mul_i32<Policy>(a, b, result, n, i, checked_mul_scalar);
#endif
    }
    else if constexpr (std::is_same_v<T, uint32_t>)
    {
#if defined(FATP_INT_SIMD_AVX2)
        i = avx2_checked_mul_u32<Policy>(a, b, result, n, i, checked_mul_scalar);
#elif defined(FATP_INT_SIMD_SSE2)
        i = sse2_checked_mul_u32<Policy>(a, b, result, n, i, checked_mul_scalar);
#elif defined(FATP_INT_SIMD_NEON)
        i = neon_checked_mul_u32<Policy>(a, b, result, n, i, checked_mul_scalar);
#endif
    }
    // int64_t multiplication: no efficient SIMD (would need __int128)
    // Falls through to scalar tail processing

    // Check for error signal from SIMD path
    if (i == static_cast<size_t>(-1))
    {
        return false; // Error occurred
    }

    // Tail processing (scalar)
    for (; i < n; ++i)
    {
        auto r = checked_mul_scalar(a[i], b[i]);
        if constexpr (std::is_same_v<Policy, fat_p::ReturnExpectedPolicy>)
        {
            if (!r.has_value())
            {
                return false;
            }
            result[i] = r.value();
        }
        else
        {
            result[i] = r;
        }
    }

    return true;
}

// =============================================================================
// Architecture Info (for debugging/logging)
// =============================================================================

struct IntSimdInfo
{
    static constexpr const char* architecture()
    {
#if defined(FATP_INT_SIMD_AVX2)
        return "AVX2";
#elif defined(FATP_INT_SIMD_SSE2)
        return "SSE2";
#elif defined(FATP_INT_SIMD_NEON) && FATP_INT_SIMD_NEON_AARCH64
        return "NEON-AArch64";
#elif defined(FATP_INT_SIMD_NEON)
        return "NEON-ARM32";
#else
        return "Scalar";
#endif
    }

    static constexpr size_t int32_width()
    {
#if defined(FATP_INT_SIMD_AVX2)
        return 8;
#elif defined(FATP_INT_SIMD_SSE2) || defined(FATP_INT_SIMD_NEON)
        return 4;
#else
        return 1;
#endif
    }

    static constexpr size_t int64_width()
    {
#if defined(FATP_INT_SIMD_AVX2)
        return 4;
#elif defined(FATP_INT_SIMD_NEON) && FATP_INT_SIMD_NEON_AARCH64
        return 2;
#else
        return 1;
#endif
    }

    static constexpr bool has_saturating_hardware()
    {
#if defined(FATP_INT_SIMD_NEON)
        return true; // NEON has vqadd/vqsub
#else
        return false;
#endif
    }
};

} // namespace int_simd
} // namespace fat_p
