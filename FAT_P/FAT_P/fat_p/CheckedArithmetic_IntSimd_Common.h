/**
 * @file CheckedArithmetic_IntSimd_Common.h
 * @brief Common traits and helpers for integer SIMD acceleration
 *
 *
 * @layer Foundation
 *
 * @version 1.0
 *
 * This provides the foundational infrastructure for checked integer SIMD
 * operations across SSE2, AVX2, and NEON architectures.
 *
 * Design: detect -> scalar fallback
 * - Perform SIMD arithmetic (fast path)
 * - Check for overflow using architecture-specific masks
 * - If overflow detected, fall back to scalar mChecked* for proper error handling
 */

#pragma once

/*
FATP_META:
  meta_version: 1
  component: CheckedArithmetic_IntSimd_Common
  file_role: public_header
  path: fat_p/CheckedArithmetic_IntSimd_Common.h
  namespace: fat_p
  layer: Foundation
  summary: "Public header for CheckedArithmetic_IntSimd_Common."
  api_stability: in_work
  related:
    docs_search: "CheckedArithmetic_IntSimd_Common"
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 9
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

// =============================================================================
// Architecture Detection (MUST be at global scope before any namespace)
// =============================================================================

#if defined(__AVX2__)
#define FATP_INT_SIMD_AVX2 1
#define FATP_INT_SIMD_SSE2 1
#include <immintrin.h>
#elif defined(__SSE2__) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2) || defined(_M_X64)
#define FATP_INT_SIMD_SSE2 1
#include <emmintrin.h>
#endif

#if defined(__aarch64__)
#define FATP_INT_SIMD_NEON 1
#define FATP_INT_SIMD_NEON_AARCH64 1
#include <arm_neon.h>
#elif defined(__ARM_NEON)
#define FATP_INT_SIMD_NEON 1
#define FATP_INT_SIMD_NEON_AARCH64 0
#include <arm_neon.h>
#endif

// Unified availability check
#if defined(FATP_INT_SIMD_AVX2) || defined(FATP_INT_SIMD_SSE2) || defined(FATP_INT_SIMD_NEON)
#define FATP_HAS_INT_SIMD 1
#else
#define FATP_HAS_INT_SIMD 0
#endif

namespace fat_p
{
namespace int_simd
{

// =============================================================================
// Lane Width Constants
// =============================================================================

#if defined(FATP_INT_SIMD_AVX2)
constexpr size_t INT32_SIMD_WIDTH = 8; // 256-bit / 32-bit
constexpr size_t INT64_SIMD_WIDTH = 4; // 256-bit / 64-bit
#elif defined(FATP_INT_SIMD_SSE2) || defined(FATP_INT_SIMD_NEON)
constexpr size_t INT32_SIMD_WIDTH = 4; // 128-bit / 32-bit
constexpr size_t INT64_SIMD_WIDTH = 2; // 128-bit / 64-bit
#else
constexpr size_t INT32_SIMD_WIDTH = 1;
constexpr size_t INT64_SIMD_WIDTH = 1;
#endif

// =============================================================================
// Overflow Classification (for scalar fallback)
// =============================================================================

enum class OverflowType
{
    None,
    Overflow, // Positive overflow
    Underflow // Negative overflow (signed) or wrap-around (unsigned)
};

/**
 * @brief Detect signed addition overflow
 * @return OverflowType indicating the nature of any overflow
 */
template <typename T>
constexpr OverflowType detect_add_overflow(T a, T b, T result) noexcept
{
    static_assert(std::is_integral_v<T>, "Integral type required");

    if constexpr (std::is_signed_v<T>)
    {
        // Positive overflow: pos + pos = neg
        if (b > 0 && a > std::numeric_limits<T>::max() - b)
        {
            return OverflowType::Overflow;
        }
        // Negative overflow: neg + neg = pos
        if (b < 0 && a < std::numeric_limits<T>::min() - b)
        {
            return OverflowType::Underflow;
        }
        return OverflowType::None;
    }
    else
    {
        // Unsigned wrap: result < operand
        return (result < a) ? OverflowType::Overflow : OverflowType::None;
    }
}

/**
 * @brief Detect signed subtraction overflow
 */
template <typename T>
constexpr OverflowType detect_sub_overflow(T a, T b, [[maybe_unused]] T result) noexcept
{
    static_assert(std::is_integral_v<T>, "Integral type required");

    if constexpr (std::is_signed_v<T>)
    {
        // Positive overflow: pos - neg = neg (shouldn't be neg)
        if (b < 0 && a > std::numeric_limits<T>::max() + b)
        {
            return OverflowType::Overflow;
        }
        // Negative overflow: neg - pos = pos (shouldn't be pos)
        if (b > 0 && a < std::numeric_limits<T>::min() + b)
        {
            return OverflowType::Underflow;
        }
        return OverflowType::None;
    }
    else
    {
        // Unsigned underflow: a < b
        return (a < b) ? OverflowType::Underflow : OverflowType::None;
    }
}

/**
 * @brief Detect multiplication overflow
 */
template <typename T>
constexpr OverflowType detect_mul_overflow(T a, T b, T result) noexcept
{
    static_assert(std::is_integral_v<T>, "Integral type required");

    if (a == 0 || b == 0)
    {
        return OverflowType::None;
    }

    if constexpr (std::is_signed_v<T>)
    {
        // Special case: INT_MIN * -1
        if ((a == static_cast<T>(-1) && b == std::numeric_limits<T>::min()) ||
            (b == static_cast<T>(-1) && a == std::numeric_limits<T>::min()))
        {
            return OverflowType::Overflow;
        }

        // Check via division
        if (b != 0 && result / b != a)
        {
            // Determine overflow direction
            bool same_sign = (a > 0) == (b > 0);
            return same_sign ? OverflowType::Overflow : OverflowType::Underflow;
        }
        return OverflowType::None;
    }
    else
    {
        // Unsigned: check via division
        if (result / b != a)
        {
            return OverflowType::Overflow;
        }
        return OverflowType::None;
    }
}

// =============================================================================
// SIMD Mask Checking Utilities
// =============================================================================

#if defined(FATP_INT_SIMD_AVX2)

/**
 * @brief Check if any lane in AVX2 mask is set (32-bit lanes)
 *
 * @note CRITICAL: This helper masks specific sign bits (0x88888888) and is
 *       intended ONLY for SIGNED overflow detection where the sign bit
 *       indicates the error state.
 *
 *       For UNSIGNED overflow detection, the comparison itself yields
 *       0xFF or 0x00 per lane, so check _mm256_movemask_epi8(mask) != 0 directly.
 *       DO NOT use this helper for unsigned masks - it will miss overflows.
 *
 * movemask_epi8 returns bit per byte; sign bits at byte positions 3,7,11,15,19,23,27,31
 */
inline bool avx2_any_overflow_i32(__m256i mask) noexcept
{
    // Sign bit of each 32-bit element is at byte positions 3,7,11,15,19,23,27,31
    // In movemask result, these are bits 3,7,11,15,19,23,27,31
    return (_mm256_movemask_epi8(mask) & 0x88888888) != 0;
}

/**
 * @brief Check if any lane in AVX2 mask is set (64-bit lanes)
 */
inline bool avx2_any_overflow_i64(__m256i mask) noexcept
{
    // Sign bit of each 64-bit element is at byte positions 7,15,23,31
    return (_mm256_movemask_epi8(mask) & 0x80808080) != 0;
}

#endif // FATP_INT_SIMD_AVX2

#if defined(FATP_INT_SIMD_SSE2)

/**
 * @brief Check if any lane in SSE2 mask is set (32-bit lanes)
 *
 * @note CRITICAL: This helper masks specific sign bits (0x8888) and is
 *       intended ONLY for SIGNED overflow detection.
 *       For UNSIGNED overflow, use _mm_movemask_epi8(mask) != 0 directly.
 */
inline bool sse2_any_overflow_i32(__m128i mask) noexcept
{
    // Sign bit of each 32-bit element is at byte positions 3,7,11,15
    return (_mm_movemask_epi8(mask) & 0x8888) != 0;
}

/**
 * @brief Check if any lane in SSE2 mask is set (64-bit lanes)
 */
inline bool sse2_any_overflow_i64(__m128i mask) noexcept
{
    // Sign bit of each 64-bit element is at byte positions 7,15
    return (_mm_movemask_epi8(mask) & 0x8080) != 0;
}

#endif // FATP_INT_SIMD_SSE2

#if defined(FATP_INT_SIMD_NEON)

/**
 * @brief Check if any lane in NEON mask differs (for differential saturation)
 * @param wrap Wrapping result
 * @param sat Saturating result
 * @return true if any lane differs (overflow occurred)
 */
inline bool neon_any_differ_i32(int32x4_t wrap, int32x4_t sat) noexcept
{
    uint32x4_t eq = vceqq_s32(wrap, sat);
    // All lanes equal = all 1s; any differ = not all 1s
#if FATP_INT_SIMD_NEON_AARCH64
    // AArch64: check 64-bit chunks
    uint64x2_t eq64 = vreinterpretq_u64_u32(eq);
    uint64_t m0 = vgetq_lane_u64(eq64, 0);
    uint64_t m1 = vgetq_lane_u64(eq64, 1);
    return (m0 != std::numeric_limits<uint64_t>::max()) || (m1 != std::numeric_limits<uint64_t>::max());
#else
    // AArch32: check 32-bit lanes individually
    return (vgetq_lane_u32(eq, 0) != ~0U) || (vgetq_lane_u32(eq, 1) != ~0U) || (vgetq_lane_u32(eq, 2) != ~0U) ||
           (vgetq_lane_u32(eq, 3) != ~0U);
#endif
}

inline bool neon_any_differ_u32(uint32x4_t wrap, uint32x4_t sat) noexcept
{
    uint32x4_t eq = vceqq_u32(wrap, sat);
#if FATP_INT_SIMD_NEON_AARCH64
    // AArch64: check 64-bit chunks
    uint64x2_t eq64 = vreinterpretq_u64_u32(eq);
    uint64_t m0 = vgetq_lane_u64(eq64, 0);
    uint64_t m1 = vgetq_lane_u64(eq64, 1);
    return (m0 != std::numeric_limits<uint64_t>::max()) || (m1 != std::numeric_limits<uint64_t>::max());
#else
    // AArch32: check 32-bit lanes individually
    return (vgetq_lane_u32(eq, 0) != ~0U) || (vgetq_lane_u32(eq, 1) != ~0U) || (vgetq_lane_u32(eq, 2) != ~0U) ||
           (vgetq_lane_u32(eq, 3) != ~0U);
#endif
}

#if FATP_INT_SIMD_NEON_AARCH64
inline bool neon_any_differ_i64(int64x2_t wrap, int64x2_t sat) noexcept
{
    uint64x2_t eq = vceqq_s64(wrap, sat);
    uint64_t m0 = vgetq_lane_u64(eq, 0);
    uint64_t m1 = vgetq_lane_u64(eq, 1);
    return (m0 != std::numeric_limits<uint64_t>::max()) || (m1 != std::numeric_limits<uint64_t>::max());
}

inline bool neon_any_differ_u64(uint64x2_t wrap, uint64x2_t sat) noexcept
{
    uint64x2_t eq = vceqq_u64(wrap, sat);
    uint64_t m0 = vgetq_lane_u64(eq, 0);
    uint64_t m1 = vgetq_lane_u64(eq, 1);
    return (m0 != std::numeric_limits<uint64_t>::max()) || (m1 != std::numeric_limits<uint64_t>::max());
}
#endif

#endif // FATP_INT_SIMD_NEON

} // namespace int_simd
} // namespace fat_p
