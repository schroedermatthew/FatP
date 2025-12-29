// ConstexprUtilities.h
// Compile-time utilities for hashing, arithmetic, and string operations
// C++17 header-only library
#pragma once

#include <cmath>
#include <cstdint>
#include <iosfwd>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace fat_p
{

// =============================================================================
// C++20 Detection and Macros
// =============================================================================

#if __cplusplus >= 202002L
    #define FATP_CONSTEVAL consteval
#else
    #define FATP_CONSTEVAL constexpr
#endif

// =============================================================================
// Hashing Utilities
// =============================================================================

/**
 * @brief Compile-time FNV-1a hash function for strings (32-bit).
 *
 * FNV-1a is a non-cryptographic hash with good avalanche properties.
 * Useful for compile-time string switches and hash-based dispatch tables.
 *
 * @param s The string view to hash.
 * @return uint32_t The 32-bit hash value.
 *
 * @note Not suitable for cryptographic purposes.
 * @note Empty string returns the FNV offset basis (2166136261).
 */
[[nodiscard]] FATP_CONSTEVAL uint32_t constexpr_hash(std::string_view s) noexcept
{
    constexpr uint32_t FNV_PRIME = 16777619U;
    constexpr uint32_t FNV_OFFSET_BASIS = 2166136261U;

    uint32_t hash = FNV_OFFSET_BASIS;
    for (char c : s)
    {
        hash ^= static_cast<uint32_t>(static_cast<unsigned char>(c));
        hash *= FNV_PRIME;
    }
    return hash;
}

/**
 * @brief Compile-time FNV-1a hash function for strings (64-bit).
 *
 * 64-bit variant provides better collision resistance for large hash tables.
 *
 * @param s The string view to hash.
 * @return uint64_t The 64-bit hash value.
 *
 * @note Empty string returns the FNV offset basis (14695981039346656037).
 */
[[nodiscard]] FATP_CONSTEVAL uint64_t constexpr_hash64(std::string_view s) noexcept
{
    constexpr uint64_t FNV_PRIME = 1099511628211ULL;
    constexpr uint64_t FNV_OFFSET_BASIS = 14695981039346656037ULL;

    uint64_t hash = FNV_OFFSET_BASIS;
    for (char c : s)
    {
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
        hash *= FNV_PRIME;
    }
    return hash;
}

/**
 * @brief Combine two hash values into one (for hashing composite types).
 *
 * Uses the Boost-style hash combine algorithm with the golden ratio constant.
 *
 * @param seed The existing hash seed.
 * @param value The new hash value to combine.
 * @return uint64_t The combined hash.
 *
 * @example
 *   uint64_t h = hash_combine(constexpr_hash64("key"), constexpr_hash64("value"));
 */
[[nodiscard]] constexpr uint64_t hash_combine(uint64_t seed, uint64_t value) noexcept
{
    return seed ^ (value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2));
}

/**
 * @brief Hash multiple string values into a single hash.
 *
 * @param args String views to hash and combine.
 * @return uint64_t The combined hash of all inputs.
 *
 * @example
 *   constexpr auto h = hash_values("namespace", "class", "method");
 */
template <typename... Args>
[[nodiscard]] constexpr uint64_t hash_values(const Args&... args) noexcept
{
    uint64_t seed = 0;
    ((seed = hash_combine(seed, constexpr_hash64(args))), ...);
    return seed;
}

// =============================================================================
// Arithmetic Utilities
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
template <typename T>
[[nodiscard]] constexpr bool is_power_of_two(T n) noexcept
{
    static_assert(std::is_integral_v<T>, "is_power_of_two requires an integral type");
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
template <typename T>
[[nodiscard]] constexpr T next_power_of_two(T n) noexcept
{
    static_assert(std::is_integral_v<T>, "next_power_of_two requires an integral type");
    static_assert(std::is_unsigned_v<T>, "next_power_of_two requires an unsigned type");

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
        return 0;  // Overflow: result would exceed type max
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
template <typename T>
[[nodiscard]] constexpr int log2_floor(T n) noexcept
{
    static_assert(std::is_integral_v<T>, "log2_floor requires an integral type");
    static_assert(std::is_unsigned_v<T>, "log2_floor requires an unsigned type");

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
template <typename T>
[[nodiscard]] constexpr int log2_ceil(T n) noexcept
{
    static_assert(std::is_integral_v<T>, "log2_ceil requires an integral type");
    static_assert(std::is_unsigned_v<T>, "log2_ceil requires an unsigned type");

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
template <typename T>
[[nodiscard]] constexpr int count_digits(T n) noexcept
{
    static_assert(std::is_integral_v<T>, "count_digits requires an integral type");

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
        digits = (abs_n < 10) ? 1 :
                 (abs_n < 100) ? 2 :
                 (abs_n < 1000) ? 3 :
                 (abs_n < 10000) ? 4 :
                 (abs_n < 100000) ? 5 :
                 (abs_n < 1000000) ? 6 :
                 (abs_n < 10000000) ? 7 :
                 (abs_n < 100000000) ? 8 :
                 (abs_n < 1000000000) ? 9 : 10;
    }
    else
    {
        // 64-bit path
        digits = (abs_n < 10ULL) ? 1 :
                 (abs_n < 100ULL) ? 2 :
                 (abs_n < 1000ULL) ? 3 :
                 (abs_n < 10000ULL) ? 4 :
                 (abs_n < 100000ULL) ? 5 :
                 (abs_n < 1000000ULL) ? 6 :
                 (abs_n < 10000000ULL) ? 7 :
                 (abs_n < 100000000ULL) ? 8 :
                 (abs_n < 1000000000ULL) ? 9 :
                 (abs_n < 10000000000ULL) ? 10 :
                 (abs_n < 100000000000ULL) ? 11 :
                 (abs_n < 1000000000000ULL) ? 12 :
                 (abs_n < 10000000000000ULL) ? 13 :
                 (abs_n < 100000000000000ULL) ? 14 :
                 (abs_n < 1000000000000000ULL) ? 15 :
                 (abs_n < 10000000000000000ULL) ? 16 :
                 (abs_n < 100000000000000000ULL) ? 17 :
                 (abs_n < 1000000000000000000ULL) ? 18 :
                 (abs_n < 10000000000000000000ULL) ? 19 : 20;
    }
    return sign_chars + digits;
}

/**
 * @brief Population count - counts the number of set bits.
 *
 * Uses compiler intrinsics (GCC/Clang) when available for 10-50x speedup.
 * Falls back to portable loop for MSVC and other compilers.
 *
 * @tparam T An unsigned integral type.
 * @param n The value to count bits in.
 * @return The number of 1 bits in n.
 *
 * @example
 *   static_assert(popcount(0b1010u) == 2);
 *   static_assert(popcount(0xFFu) == 8);
 */
template <typename T>
[[nodiscard]] constexpr int popcount(T n) noexcept
{
    static_assert(std::is_integral_v<T>, "popcount requires an integral type");
    static_assert(std::is_unsigned_v<T>, "popcount requires an unsigned type");

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
#else
    // Portable fallback (MSVC intrinsics are not constexpr)
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
 * Uses compiler intrinsics (GCC/Clang) when available for significant speedup.
 *
 * @tparam T An unsigned integral type.
 * @param n The value to analyze.
 * @return Number of leading zero bits. Returns sizeof(T)*8 if n == 0.
 *
 * @example
 *   static_assert(clz(uint8_t(0b00001000)) == 4);
 */
template <typename T>
[[nodiscard]] constexpr int clz(T n) noexcept
{
    static_assert(std::is_integral_v<T>, "clz requires an integral type");
    static_assert(std::is_unsigned_v<T>, "clz requires an unsigned type");

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
 * Uses compiler intrinsics (GCC/Clang) when available for significant speedup.
 *
 * @tparam T An unsigned integral type.
 * @param n The value to analyze.
 * @return Number of trailing zero bits. Returns sizeof(T)*8 if n == 0.
 *
 * @example
 *   static_assert(ctz(uint8_t(0b00001000)) == 3);
 */
template <typename T>
[[nodiscard]] constexpr int ctz(T n) noexcept
{
    static_assert(std::is_integral_v<T>, "ctz requires an integral type");
    static_assert(std::is_unsigned_v<T>, "ctz requires an unsigned type");

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

// =============================================================================
// String Conversion Utilities
// =============================================================================

// Buffer pool size for thread-local converters
// Set to 16 to safely handle deep nested logging calls without buffer aliasing
inline constexpr std::size_t STRING_POOL_SIZE = 16;

/**
 * @brief Compile-time integer-to-string conversion.
 *
 * Converts integers to their string representation at compile time.
 * The result is stored in an internal buffer accessible via view().
 *
 * @tparam T Integral type (any size, signed or unsigned).
 *
 * @example
 *   constexpr constexpr_to_string_t<int> conv{42};
 *   constexpr auto view = conv.view();  // "42"
 */
template <typename T>
struct constexpr_to_string_t
{
    static_assert(std::is_integral_v<T>, "constexpr_to_string_t requires integral type");
    static constexpr std::size_t MaxSize = 32;
    char buffer[MaxSize] = {};
    std::size_t length = 0;

    constexpr constexpr_to_string_t() noexcept = default;

    constexpr constexpr_to_string_t(T value) noexcept
    {
        if (value == 0)
        {
            buffer[0] = '0';
            length = 1;
            return;
        }

        bool negative = false;
        using UnsignedT = std::make_unsigned_t<T>;
        UnsignedT abs_value;

        if constexpr (std::is_signed_v<T>)
        {
            if (value < 0)
            {
                negative = true;
                abs_value = static_cast<UnsignedT>(-(value + 1)) + 1;
            }
            else
            {
                abs_value = static_cast<UnsignedT>(value);
            }
        }
        else
        {
            abs_value = value;
        }

        char temp_buffer[MaxSize] = {};
        std::size_t temp_pos = 0;

        while (abs_value > 0)
        {
            temp_buffer[temp_pos++] = '0' + static_cast<char>(abs_value % 10);
            abs_value /= 10;
        }

        std::size_t write_pos = 0;
        if (negative)
        {
            buffer[write_pos++] = '-';
        }

        for (std::size_t i = temp_pos; i > 0; --i)
        {
            buffer[write_pos++] = temp_buffer[i - 1];
        }

        length = write_pos;
    }

    [[nodiscard]] constexpr std::string_view view() const noexcept
    {
        return {buffer, length};
    }
};

/**
 * @brief Convert integral value to string_view using thread-local buffer pool.
 *
 * Uses a rotating pool of 16 thread-local buffers to support multiple calls
 * in the same expression without interference, including nested logging calls.
 *
 * @tparam T Integral type (any size, signed or unsigned).
 * @param value Value to convert.
 * @return std::string_view String representation (valid until 16 more calls).
 *
 * @warning The returned view is invalidated after 16 subsequent calls.
 * @note Thread-safe via thread-local storage.
 *
 * @example
 *   std::cout << to_string_view(10) << " + " << to_string_view(20);  // Safe
 */
template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
[[nodiscard]] inline std::string_view to_string_view(T value) noexcept
{
    thread_local constexpr_to_string_t<T> converter_pool[STRING_POOL_SIZE] = {};
    thread_local std::size_t pool_index = 0;

    auto& converter = converter_pool[pool_index];
    pool_index = (pool_index + 1) % STRING_POOL_SIZE;

    converter = constexpr_to_string_t<T>{value};
    return converter.view();
}

/**
 * @brief Pass-through for C-string literals.
 *
 * @param s The C-string to wrap.
 * @return std::string_view wrapping the input.
 */
[[nodiscard]] constexpr std::string_view to_string_view(const char* s) noexcept
{
    return s;
}

// =============================================================================
// Floating-Point String Conversion
// =============================================================================

namespace detail
{

// Maximum value that can be safely cast to long long
inline constexpr double MAX_SAFE_INT_DOUBLE = 9007199254740992.0;  // 2^53

}  // namespace detail

/**
 * @brief Floating-point-to-string conversion helper.
 *
 * Converts floating-point values to string representation with configurable
 * precision. Handles special values (NaN, Inf) correctly.
 *
 * @tparam T Floating-point type (float, double, long double).
 *
 * @note This is a runtime-only operation in C++17 (std::isnan not constexpr).
 * @note Large values (> 2^53) may lose precision or produce "overflow".
 * @note Does not support scientific notation. Values beyond safe range
 *       will display "overflow" rather than attempting lossy conversion.
 */
template <typename T>
struct float_to_string_t
{
    static_assert(std::is_floating_point_v<T>, "float_to_string_t requires floating-point type");
    static constexpr std::size_t MaxSize = 64;
    char buffer[MaxSize] = {};
    std::size_t length = 0;

    float_to_string_t() noexcept = default;

    float_to_string_t(T val, int prec) noexcept
    {
        if (std::isnan(val))
        {
            buffer[0] = 'n';
            buffer[1] = 'a';
            buffer[2] = 'n';
            length = 3;
            return;
        }

        if (std::isinf(val))
        {
            if (std::signbit(val))
            {
                buffer[length++] = '-';
            }
            buffer[length++] = 'i';
            buffer[length++] = 'n';
            buffer[length++] = 'f';
            return;
        }

        bool negative = std::signbit(val);
        T abs_val = negative ? -val : val;

        // Normalize -0.0 to 0.0 for cleaner output
        if (abs_val == T(0))
        {
            negative = false;
        }

        // Clamp precision to valid range
        if (prec < 0)
        {
            prec = 0;
        }
        if (prec > 15)
        {
            prec = 15;
        }

        // Check for overflow before conversion
        if (abs_val > detail::MAX_SAFE_INT_DOUBLE)
        {
            if (negative)
            {
                buffer[length++] = '-';
            }
            const char* overflow_str = "overflow";
            for (int i = 0; overflow_str[i] != '\0'; ++i)
            {
                buffer[length++] = overflow_str[i];
            }
            return;
        }

        if (negative)
        {
            buffer[length++] = '-';
        }

        // Integer part
        auto int_part = static_cast<long long>(abs_val);
        T frac_part = abs_val - static_cast<T>(int_part);

        // Convert integer part
        if (int_part == 0)
        {
            buffer[length++] = '0';
        }
        else
        {
            char temp[32] = {};
            std::size_t temp_len = 0;
            while (int_part > 0)
            {
                temp[temp_len++] = '0' + static_cast<char>(int_part % 10);
                int_part /= 10;
            }
            for (std::size_t i = temp_len; i > 0; --i)
            {
                buffer[length++] = temp[i - 1];
            }
        }

        // Decimal point
        buffer[length++] = '.';

        // Fractional part
        for (int i = 0; i < prec; ++i)
        {
            frac_part *= T(10);
            int digit = static_cast<int>(frac_part);
            buffer[length++] = '0' + static_cast<char>(digit);
            frac_part -= static_cast<T>(digit);
        }
    }

    [[nodiscard]] std::string_view view() const noexcept
    {
        return {buffer, length};
    }
};

/**
 * @brief Convert floating-point value to string_view.
 *
 * @tparam T Floating-point type (float, double, long double).
 * @param value Value to convert.
 * @param precision Decimal places (0-15, default: 6).
 * @return std::string_view String representation.
 *
 * @note Returns "nan", "inf", "-inf" for special values.
 * @note Returns "overflow" or "-overflow" for values > 2^53.
 * @warning The returned view is invalidated after 16 subsequent calls.
 */
template <typename T, typename = std::enable_if_t<std::is_floating_point_v<T>>>
[[nodiscard]] inline std::string_view to_string_view(T value, int precision = 6) noexcept
{
    thread_local float_to_string_t<T> converter_pool[STRING_POOL_SIZE] = {};
    thread_local std::size_t pool_index = 0;

    auto& converter = converter_pool[pool_index];
    pool_index = (pool_index + 1) % STRING_POOL_SIZE;

    converter = float_to_string_t<T>{value, precision};
    return converter.view();
}

// =============================================================================
// Hexadecimal String Conversion
// =============================================================================

/**
 * @brief Integer-to-hexadecimal string conversion.
 *
 * @tparam T Unsigned integral type.
 */
template <typename T>
struct to_hex_string_t
{
    static_assert(std::is_integral_v<T>, "to_hex_string_t requires integral type");
    static_assert(std::is_unsigned_v<T>, "to_hex_string_t requires unsigned type");
    static constexpr std::size_t MaxSize = sizeof(T) * 2 + 3;  // "0x" + digits + null
    char buffer[MaxSize] = {};
    std::size_t length = 0;

    constexpr to_hex_string_t() noexcept = default;

    constexpr to_hex_string_t(T value, bool prefix, bool uppercase) noexcept
    {
        const char* hex_lower = "0123456789abcdef";
        const char* hex_upper = "0123456789ABCDEF";
        const char* hex_chars = uppercase ? hex_upper : hex_lower;

        if (prefix)
        {
            buffer[length++] = '0';
            buffer[length++] = uppercase ? 'X' : 'x';
        }

        if (value == 0)
        {
            buffer[length++] = '0';
            return;
        }

        char temp[sizeof(T) * 2] = {};
        std::size_t temp_len = 0;

        while (value > 0)
        {
            temp[temp_len++] = hex_chars[value & 0xF];
            value >>= 4;
        }

        for (std::size_t i = temp_len; i > 0; --i)
        {
            buffer[length++] = temp[i - 1];
        }
    }

    // Backward-compatible constructor
    constexpr to_hex_string_t(T value, bool prefix = true) noexcept
        : to_hex_string_t(value, prefix, false)
    {
    }

    [[nodiscard]] constexpr std::string_view view() const noexcept
    {
        return {buffer, length};
    }
};

/**
 * @brief Convert unsigned integer to hexadecimal string_view.
 *
 * @tparam T Unsigned integral type.
 * @param value Value to convert.
 * @param prefix If true, prepend "0x" or "0X" (default: true).
 * @param uppercase If true, use A-F instead of a-f (default: false).
 * @return std::string_view Hexadecimal representation.
 *
 * @warning The returned view is invalidated after 16 subsequent calls.
 *
 * @example
 *   to_hex_string_view(255u);              // "0xff"
 *   to_hex_string_view(255u, true, true);  // "0XFF"
 *   to_hex_string_view(255u, false);       // "ff"
 */
template <typename T, typename = std::enable_if_t<std::is_integral_v<T> && std::is_unsigned_v<T>>>
[[nodiscard]] inline std::string_view to_hex_string_view(T value,
                                                          bool prefix = true,
                                                          bool uppercase = false) noexcept
{
    thread_local to_hex_string_t<T> converter_pool[STRING_POOL_SIZE] = {};
    thread_local std::size_t pool_index = 0;

    auto& converter = converter_pool[pool_index];
    pool_index = (pool_index + 1) % STRING_POOL_SIZE;

    converter = to_hex_string_t<T>{value, prefix, uppercase};
    return converter.view();
}

// =============================================================================
// String Concatenation
// =============================================================================

/**
 * @brief Zero-allocation string view concatenation container.
 *
 * Holds references to multiple string views for deferred concatenation.
 * No memory allocation occurs until to_string() is called.
 *
 * @tparam N The number of views to concatenate.
 *
 * @warning Views must remain valid until consumption. Passing temporaries
 *          (e.g., std::string rvalues) results in dangling references.
 */
template <std::size_t N>
struct ConstexprString
{
    std::string_view views[N];

    /**
     * @brief Calculate total size of concatenated string.
     */
    [[nodiscard]] constexpr std::size_t size() const noexcept
    {
        std::size_t s = 0;
        for (std::size_t i = 0; i < N; ++i)
        {
            s += views[i].size();
        }
        return s;
    }

    /**
     * @brief Check if the concatenated string is empty.
     */
    [[nodiscard]] constexpr bool empty() const noexcept
    {
        return size() == 0;
    }

    /**
     * @brief Write concatenated string to a character buffer.
     *
     * @param target_buffer Destination buffer.
     * @param target_size Buffer size (must include space for null terminator).
     * @return Number of characters written (excluding null terminator).
     *
     * @note Truncates if buffer is too small. Always null-terminates.
     */
    [[nodiscard]] constexpr std::size_t to_array(char* target_buffer,
                                                  std::size_t target_size) const
    {
        if (target_size == 0)
        {
            return 0;
        }

        std::size_t current_pos = 0;
        for (std::size_t i = 0; i < N; ++i)
        {
            for (char c : views[i])
            {
                if (current_pos < target_size - 1)
                {
                    target_buffer[current_pos++] = c;
                }
            }
        }
        target_buffer[current_pos] = '\0';
        return current_pos;
    }

    /**
     * @brief Convert to std::string (allocates memory).
     *
     * @return std::string Concatenated result.
     */
    [[nodiscard]] std::string to_string() const
    {
        std::string result;
        result.reserve(size());
        for (std::size_t i = 0; i < N; ++i)
        {
            result.append(views[i]);
        }
        return result;
    }
};

/**
 * @brief Stream output operator for ConstexprString.
 *
 * Writes the concatenated string directly to the stream without allocation.
 */
template <std::size_t N>
inline std::ostream& operator<<(std::ostream& os, const ConstexprString<N>& cs)
{
    for (std::size_t i = 0; i < N; ++i)
    {
        os << cs.views[i];
    }
    return os;
}

/**
 * @brief Create a ConstexprString from multiple string views.
 *
 * @param v String views or values convertible to string_view.
 * @return ConstexprString holding references to the inputs.
 *
 * @warning Do not pass temporaries. The returned object holds views
 *          that must remain valid until the result is consumed.
 *
 * @example
 *   auto msg = constexpr_concat("Error: ", to_string_view(42), " occurred");
 *   std::string s = msg.to_string();  // "Error: 42 occurred"
 *   std::cout << msg;                 // Direct streaming (no allocation)
 */
template <typename... Args>
[[nodiscard]] constexpr auto constexpr_concat(Args&&... v)
{
    return ConstexprString<sizeof...(Args)>{{std::forward<Args>(v)...}};
}

// =============================================================================
// Compile-Time String Utilities
// =============================================================================

/**
 * @brief Compile-time string length.
 *
 * @param s Null-terminated C-string.
 * @return Length excluding null terminator.
 */
[[nodiscard]] constexpr std::size_t constexpr_strlen(const char* s) noexcept
{
    std::size_t len = 0;
    while (s[len] != '\0')
    {
        ++len;
    }
    return len;
}

/**
 * @brief Compile-time string comparison.
 *
 * @param a First string.
 * @param b Second string.
 * @return <0 if a < b, 0 if a == b, >0 if a > b.
 */
[[nodiscard]] constexpr int constexpr_strcmp(const char* a, const char* b) noexcept
{
    while (*a && (*a == *b))
    {
        ++a;
        ++b;
    }
    return static_cast<unsigned char>(*a) - static_cast<unsigned char>(*b);
}

/**
 * @brief Compile-time string equality check.
 *
 * @param a First string view.
 * @param b Second string view.
 * @return true if strings are equal.
 */
[[nodiscard]] constexpr bool constexpr_streq(std::string_view a, std::string_view b) noexcept
{
    if (a.size() != b.size())
    {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        if (a[i] != b[i])
        {
            return false;
        }
    }
    return true;
}

}  // namespace fat_p
