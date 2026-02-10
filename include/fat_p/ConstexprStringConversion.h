#pragma once

/*
FATP_META:
  meta_version: 1
  component: ConstexprStringConversion
  file_role: public_header
  path: include/fat_p/ConstexprStringConversion.h
  namespace: fat_p
  layer: Foundation
  summary: Thread-safe integer, float, and hex to-string-view converters with zero-alloc concat.
  api_stability: candidate
  related:
    docs_search: "ConstexprStringConversion"
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
 * @file ConstexprStringConversion.h
 * @brief Thread-safe integer/float/hex to-string-view converters and constexpr_concat.
 *
 * Each to_string_view overload maintains a rotating pool of thread-local
 * converter buffers, allowing multiple calls in the same expression (including
 * nested logging and constexpr_concat chains) without interference.
 *
 * Also provides ConstexprString<N> for zero-allocation string view concatenation,
 * and compile-time string utilities (constexpr_strlen, constexpr_strcmp, constexpr_streq).
 *
 * Previously part of ConstexprUtilities.h. Extracted because string conversion
 * is independent of hashing and bit manipulation.
 */

#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace fat_p
{

// =============================================================================
// String Conversion Utilities
// =============================================================================

// Buffer pool size for thread-local converters.
// Each to_string_view overload maintains its own rotating pool of this many
// slots. A returned string_view is invalidated after STRING_POOL_SIZE more
// calls to the *same* overload on the same thread. 64 slots provides ample
// headroom for complex expressions, nested logging, and constexpr_concat
// chains. The previous value of 16 was too low for real-world usage patterns
// involving multiple conversions in a single statement or call tree.
//
// If you need a value to survive beyond one expression, copy to std::string.
inline constexpr std::size_t STRING_POOL_SIZE = 64;

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
template <std::integral T>
struct constexpr_to_string_t
{
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
 * Uses a rotating pool of STRING_POOL_SIZE thread-local buffers to support
 * multiple calls in the same expression without interference, including
 * nested logging calls and constexpr_concat chains.
 *
 * @tparam T Integral type (any size, signed or unsigned).
 * @param value Value to convert.
 * @return std::string_view String representation. Valid until STRING_POOL_SIZE
 *         more calls to this overload on the same thread.
 *
 * @warning The returned view is invalidated after STRING_POOL_SIZE subsequent
 *          calls. For persistent storage, copy to std::string immediately.
 * @note Thread-safe via thread-local storage.
 *
 * @example
 *   std::cout << to_string_view(10) << " + " << to_string_view(20);  // Safe
 *   std::string saved(to_string_view(42));  // Safe — copies into owned string
 */
template <std::integral T>
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
inline constexpr double MAX_SAFE_INT_DOUBLE = 9007199254740992.0; // 2^53

} // namespace detail

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
template <std::floating_point T>
struct float_to_string_t
{
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
 * @brief Convert floating-point value to string_view using thread-local buffer pool.
 *
 * @tparam T Floating-point type (float, double, long double).
 * @param value Value to convert.
 * @param precision Decimal places (0-15, default: 6).
 * @return std::string_view String representation. Valid until STRING_POOL_SIZE
 *         more calls to this overload on the same thread.
 *
 * @note Returns "nan", "inf", "-inf" for special values.
 * @note Returns "overflow" or "-overflow" for values > 2^53.
 * @warning The returned view is invalidated after STRING_POOL_SIZE subsequent calls.
 */
template <std::floating_point T>
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
template <std::unsigned_integral T>
struct to_hex_string_t
{
    static constexpr std::size_t MaxSize = sizeof(T) * 2 + 3; // "0x" + digits + null
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
 * @brief Convert unsigned integer to hexadecimal string_view using thread-local buffer pool.
 *
 * @tparam T Unsigned integral type.
 * @param value Value to convert.
 * @param prefix If true, prepend "0x" or "0X" (default: true).
 * @param uppercase If true, use A-F instead of a-f (default: false).
 * @return std::string_view Hexadecimal representation. Valid until STRING_POOL_SIZE
 *         more calls to this overload on the same thread.
 *
 * @warning The returned view is invalidated after STRING_POOL_SIZE subsequent calls.
 *
 * @example
 *   to_hex_string_view(255u);              // "0xff"
 *   to_hex_string_view(255u, true, true);  // "0XFF"
 *   to_hex_string_view(255u, false);       // "ff"
 */
template <std::unsigned_integral T>
[[nodiscard]] inline std::string_view to_hex_string_view(T value, bool prefix = true, bool uppercase = false) noexcept
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
    [[nodiscard]] constexpr std::size_t to_array(char* target_buffer, std::size_t target_size) const
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

} // namespace fat_p
