#pragma once

/*
FATP_META:
  meta_version: 1
  component: ConstexprBitOps
  file_role: public_header
  path: include/fat_p/ConstexprBitOps.h
  namespace: fat_p
  layer: Foundation
  summary: Constexpr bit manipulation utilities with platform-specific intrinsics.
  api_stability: candidate
  related:
    docs_search: "ConstexprBitOps"
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

/**
 * @file ConstexprBitOps.h
 * @brief Constexpr bit manipulation: power-of-two, log2, popcount, clz, ctz.
 *
 * All functions are constexpr and use compiler intrinsics when available:
 *   - GCC/Clang: __builtin_popcount, __builtin_clz, __builtin_ctz (constexpr)
 *   - MSVC: __popcnt, _BitScanForward, _BitScanReverse (runtime fast path
 *     gated by std::is_constant_evaluated())
 *
 * Previously part of ConstexprUtilities.h. Extracted because bit manipulation
 * is an independent concern from hashing and string conversion, and multiple
 * components (CircularBuffer, BitSet) were duplicating these operations locally.
 *
 * This is the canonical definition — do not redefine these operations in
 * component-local detail namespaces (Systemic Hygiene Policy Rule E).
 */

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace fat_p
{

// =============================================================================
// Power-of-Two Utilities
// =============================================================================

/**
 * @brief Checks if an integer is a power of two at compile-time.
 *
 * @tparam T An integral type (signed or unsigned).
 * @param n The value to check.
 * @return true if n is a power of two (1, 2, 4, 8, ...), false otherwise.
 *
 * @note Zero and negative values return false.
 */
template <std::integral T>
[[nodiscard]] constexpr bool is_power_of_two(T n) noexcept
{
    return (n > 0) && ((n & (n - 1)) == 0);
}

/**
 * @brief Returns the next power of two greater than or equal to n.
 *
 * @tparam T An unsigned integral type.
 * @param n The input value.
 * @return The smallest power of two >= n. Returns 1 for n == 0.
 *
 * @warning Returns 0 on overflow (when result would exceed type max).
 *          Caller should check result != 0 when n is near type maximum.
 *
 * @example
 *   static_assert(next_power_of_two(5u) == 8u);
 *   static_assert(next_power_of_two(8u) == 8u);
 *   static_assert(next_power_of_two(0u) == 1u);
 */
template <std::unsigned_integral T>
[[nodiscard]] constexpr T next_power_of_two(T n) noexcept
{

    if (n == 0)
    {
        return 1;
    }
    if (is_power_of_two(n))
    {
        return n;
    }

    // Check for potential overflow before computing
    constexpr T high_bit = T(1) << (sizeof(T) * 8 - 1);
    if (n > high_bit)
    {
        return 0; // Overflow: result would exceed type max
    }

    --n;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    if constexpr (sizeof(T) > 1)
    {
        n |= n >> 8;
    }
    if constexpr (sizeof(T) > 2)
    {
        n |= n >> 16;
    }
    if constexpr (sizeof(T) > 4)
    {
        n |= n >> 32;
    }
    return n + 1;
}

// =============================================================================
// Logarithm Utilities
// =============================================================================

/**
 * @brief Returns floor(log2(n)) for positive integers.
 *
 * @tparam T An unsigned integral type.
 * @param n The input value (must be > 0).
 * @return The position of the highest set bit (0-indexed), or -1 if n == 0.
 *
 * @example
 *   static_assert(log2_floor(8u) == 3);   // 2^3 = 8
 *   static_assert(log2_floor(15u) == 3);  // floor(log2(15)) = 3
 *   static_assert(log2_floor(1u) == 0);   // 2^0 = 1
 */
template <std::unsigned_integral T>
[[nodiscard]] constexpr int log2_floor(T n) noexcept
{

    if (n == 0)
    {
        return -1;
    }
    int log = 0;
    while (n >>= 1)
    {
        ++log;
    }
    return log;
}

/**
 * @brief Returns ceil(log2(n)) for positive integers.
 *
 * @tparam T An unsigned integral type.
 * @param n The input value (must be > 0).
 * @return The smallest k such that 2^k >= n, or -1 if n == 0.
 *
 * @example
 *   static_assert(log2_ceil(8u) == 3);   // 2^3 = 8
 *   static_assert(log2_ceil(9u) == 4);   // 2^4 = 16 >= 9
 *   static_assert(log2_ceil(1u) == 0);   // 2^0 = 1
 */
template <std::unsigned_integral T>
[[nodiscard]] constexpr int log2_ceil(T n) noexcept
{

    if (n == 0)
    {
        return -1;
    }
    if (n == 1)
    {
        return 0;
    }
    int floor = log2_floor(n);
    return is_power_of_two(n) ? floor : floor + 1;
}

// =============================================================================
// Digit Counting
// =============================================================================

/**
 * @brief Counts the number of decimal digits in an integer.
 *
 * Optimized implementation using comparison ladders for 32-bit and 64-bit types.
 *
 * @tparam T An integral type.
 * @param n The value to count digits for.
 * @return The number of digits (including minus sign for negative values).
 *
 * @example
 *   static_assert(count_digits(0) == 1);
 *   static_assert(count_digits(42) == 2);
 *   static_assert(count_digits(-42) == 3);  // '-', '4', '2'
 */
template <std::integral T>
[[nodiscard]] constexpr int count_digits(T n) noexcept
{

    if (n == 0)
    {
        return 1;
    }

    int sign_chars = 0;
    using UnsignedT = std::make_unsigned_t<T>;
    UnsignedT abs_n = 0;

    if constexpr (std::is_signed_v<T>)
    {
        if (n < 0)
        {
            sign_chars = 1;
            abs_n = static_cast<UnsignedT>(-(n + 1)) + 1;
        }
        else
        {
            abs_n = static_cast<UnsignedT>(n);
        }
    }
    else
    {
        abs_n = n;
    }

    // Optimized comparison ladder
    int digits = 0;
    if constexpr (sizeof(T) <= 4)
    {
        // 32-bit path
        digits = (abs_n < 10)           ? 1
                 : (abs_n < 100)        ? 2
                 : (abs_n < 1000)       ? 3
                 : (abs_n < 10000)      ? 4
                 : (abs_n < 100000)     ? 5
                 : (abs_n < 1000000)    ? 6
                 : (abs_n < 10000000)   ? 7
                 : (abs_n < 100000000)  ? 8
                 : (abs_n < 1000000000) ? 9
                                        : 10;
    }
    else
    {
        // 64-bit path
        digits = (abs_n < 10ULL)                     ? 1
                 : (abs_n < 100ULL)                  ? 2
                 : (abs_n < 1000ULL)                 ? 3
                 : (abs_n < 10000ULL)                ? 4
                 : (abs_n < 100000ULL)               ? 5
                 : (abs_n < 1000000ULL)              ? 6
                 : (abs_n < 10000000ULL)             ? 7
                 : (abs_n < 100000000ULL)            ? 8
                 : (abs_n < 1000000000ULL)           ? 9
                 : (abs_n < 10000000000ULL)          ? 10
                 : (abs_n < 100000000000ULL)         ? 11
                 : (abs_n < 1000000000000ULL)        ? 12
                 : (abs_n < 10000000000000ULL)       ? 13
                 : (abs_n < 100000000000000ULL)      ? 14
                 : (abs_n < 1000000000000000ULL)     ? 15
                 : (abs_n < 10000000000000000ULL)    ? 16
                 : (abs_n < 100000000000000000ULL)   ? 17
                 : (abs_n < 1000000000000000000ULL)  ? 18
                 : (abs_n < 10000000000000000000ULL) ? 19
                                                     : 20;
    }
    return sign_chars + digits;
}

// =============================================================================
// Bit Counting and Scanning
// =============================================================================

/**
 * @brief Population count — counts the number of set bits.
 *
 * Uses compiler intrinsics when available:
 *   - GCC/Clang: __builtin_popcount (constexpr, used in all contexts)
 *   - MSVC: __popcnt/__popcnt64 at runtime, portable fallback at compile time
 *
 * @tparam T An unsigned integral type.
 * @param n The value to count bits in.
 * @return The number of 1 bits in n.
 *
 * @example
 *   static_assert(popcount(0b1010u) == 2);
 *   static_assert(popcount(0xFFu) == 8);
 */
template <std::unsigned_integral T>
[[nodiscard]] constexpr int popcount(T n) noexcept
{

#if defined(__GNUC__) || defined(__clang__)
    if constexpr (sizeof(T) <= sizeof(unsigned int))
    {
        return __builtin_popcount(static_cast<unsigned int>(n));
    }
    else if constexpr (sizeof(T) <= sizeof(unsigned long))
    {
        return __builtin_popcountl(static_cast<unsigned long>(n));
    }
    else
    {
        return __builtin_popcountll(static_cast<unsigned long long>(n));
    }
#elif defined(_MSC_VER)
    if (!std::is_constant_evaluated())
    {
        if constexpr (sizeof(T) <= 4)
        {
            return static_cast<int>(__popcnt(static_cast<unsigned int>(n)));
        }
        else
        {
#if defined(_M_X64) || defined(_M_AMD64)
            return static_cast<int>(__popcnt64(static_cast<unsigned __int64>(n)));
#else
            const auto lo = static_cast<unsigned int>(n);
            const auto hi = static_cast<unsigned int>(static_cast<unsigned __int64>(n) >> 32);
            return static_cast<int>(__popcnt(lo) + __popcnt(hi));
#endif
        }
    }
    // Constexpr fallback for MSVC
    int count = 0;
    while (n)
    {
        n &= (n - 1);
        ++count;
    }
    return count;
#else
    // Portable fallback (no intrinsics available)
    int count = 0;
    while (n)
    {
        n &= (n - 1);
        ++count;
    }
    return count;
#endif
}

/**
 * @brief Count leading zeros.
 *
 * Uses compiler intrinsics when available:
 *   - GCC/Clang: __builtin_clz (constexpr)
 *   - MSVC: _BitScanReverse at runtime, portable fallback at compile time
 *
 * @tparam T An unsigned integral type.
 * @param n The value to analyze.
 * @return Number of leading zero bits. Returns sizeof(T)*8 if n == 0.
 *
 * @example
 *   static_assert(clz(uint8_t(0b00001000)) == 4);
 */
template <std::unsigned_integral T>
[[nodiscard]] constexpr int clz(T n) noexcept
{

    constexpr int bits = sizeof(T) * 8;
    if (n == 0)
    {
        return bits;
    }

#if defined(__GNUC__) || defined(__clang__)
    if constexpr (sizeof(T) <= sizeof(unsigned int))
    {
        constexpr int uint_bits = sizeof(unsigned int) * 8;
        return __builtin_clz(static_cast<unsigned int>(n)) - (uint_bits - bits);
    }
    else if constexpr (sizeof(T) <= sizeof(unsigned long))
    {
        constexpr int ulong_bits = sizeof(unsigned long) * 8;
        return __builtin_clzl(static_cast<unsigned long>(n)) - (ulong_bits - bits);
    }
    else
    {
        constexpr int ullong_bits = sizeof(unsigned long long) * 8;
        return __builtin_clzll(static_cast<unsigned long long>(n)) - (ullong_bits - bits);
    }
#elif defined(_MSC_VER)
    if (!std::is_constant_evaluated())
    {
        unsigned long index = 0;
        if constexpr (sizeof(T) <= 4)
        {
            _BitScanReverse(&index, static_cast<unsigned long>(n));
            return bits - 1 - static_cast<int>(index);
        }
        else
        {
#if defined(_M_X64) || defined(_M_AMD64)
            _BitScanReverse64(&index, static_cast<unsigned __int64>(n));
            return 63 - static_cast<int>(index);
#else
            const auto hi = static_cast<unsigned long>(static_cast<unsigned __int64>(n) >> 32);
            if (hi != 0)
            {
                _BitScanReverse(&index, hi);
                return 31 - static_cast<int>(index);
            }
            const auto lo = static_cast<unsigned long>(n);
            _BitScanReverse(&index, lo);
            return 63 - static_cast<int>(index);
#endif
        }
    }
    // Constexpr fallback for MSVC
    int count = 0;
    T mask = T(1) << (bits - 1);
    while ((n & mask) == 0)
    {
        ++count;
        mask >>= 1;
    }
    return count;
#else
    // Portable fallback
    int count = 0;
    T mask = T(1) << (bits - 1);
    while ((n & mask) == 0)
    {
        ++count;
        mask >>= 1;
    }
    return count;
#endif
}

/**
 * @brief Count trailing zeros.
 *
 * Uses compiler intrinsics when available:
 *   - GCC/Clang: __builtin_ctz (constexpr)
 *   - MSVC: _BitScanForward at runtime, portable fallback at compile time
 *
 * @tparam T An unsigned integral type.
 * @param n The value to analyze.
 * @return Number of trailing zero bits. Returns sizeof(T)*8 if n == 0.
 *
 * @example
 *   static_assert(ctz(uint8_t(0b00001000)) == 3);
 */
template <std::unsigned_integral T>
[[nodiscard]] constexpr int ctz(T n) noexcept
{

    constexpr int bits = sizeof(T) * 8;
    if (n == 0)
    {
        return bits;
    }

#if defined(__GNUC__) || defined(__clang__)
    if constexpr (sizeof(T) <= sizeof(unsigned int))
    {
        return __builtin_ctz(static_cast<unsigned int>(n));
    }
    else if constexpr (sizeof(T) <= sizeof(unsigned long))
    {
        return __builtin_ctzl(static_cast<unsigned long>(n));
    }
    else
    {
        return __builtin_ctzll(static_cast<unsigned long long>(n));
    }
#elif defined(_MSC_VER)
    if (!std::is_constant_evaluated())
    {
        unsigned long index = 0;
        if constexpr (sizeof(T) <= 4)
        {
            _BitScanForward(&index, static_cast<unsigned long>(n));
            return static_cast<int>(index);
        }
        else
        {
#if defined(_M_X64) || defined(_M_AMD64)
            _BitScanForward64(&index, static_cast<unsigned __int64>(n));
            return static_cast<int>(index);
#else
            const auto lo = static_cast<unsigned long>(n);
            if (_BitScanForward(&index, lo) != 0)
            {
                return static_cast<int>(index);
            }
            const auto hi = static_cast<unsigned long>(static_cast<unsigned __int64>(n) >> 32);
            _BitScanForward(&index, hi);
            return static_cast<int>(index) + 32;
#endif
        }
    }
    // Constexpr fallback for MSVC
    int count = 0;
    while ((n & 1) == 0)
    {
        ++count;
        n >>= 1;
    }
    return count;
#else
    // Portable fallback
    int count = 0;
    while ((n & 1) == 0)
    {
        ++count;
        n >>= 1;
    }
    return count;
#endif
}

} // namespace fat_p
