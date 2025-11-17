// ConstexprUtilities.h
#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits> 
#include <utility> // For std::forward
#include <cmath>   // Added for std::isnan, std::isinf, std::signbit, std::abs

namespace fat_p
{

// --- Hashing Utilities ---

/**
 * @brief Compile-time FNV-1a hash function for strings (32-bit).
 * @param s The string view to hash (must be a literal or const expression).
 * @return uint32_t The 32-bit hash value.
 */
[[nodiscard]] constexpr uint32_t constexpr_hash(std::string_view s) noexcept {
    // FNV-1a constants for 32-bit
    constexpr uint32_t FNV_PRIME = 16777619U;
    constexpr uint32_t FNV_OFFSET_BASIS = 2166136261U;

    uint32_t hash = FNV_OFFSET_BASIS;

    for (char c : s) {
        hash ^= static_cast<uint32_t>(static_cast<unsigned char>(c));
        hash *= FNV_PRIME;
    }
    return hash;
}

/**
 * @brief Compile-time FNV-1a hash function for strings (64-bit).
 * @param s The string view to hash (must be a literal or const expression).
 * @return uint64_t The 64-bit hash value.
 */
[[nodiscard]] constexpr uint64_t constexpr_hash64(std::string_view s) noexcept {
    // FNV-1a constants for 64-bit
    constexpr uint64_t FNV_PRIME = 1099511628211ULL;
    constexpr uint64_t FNV_OFFSET_BASIS = 14695981039346656037ULL;

    uint64_t hash = FNV_OFFSET_BASIS;
    for (char c : s) {
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
        hash *= FNV_PRIME;
    }
    return hash;
}

// --- Arithmetic Utilities ---

/**
 * @brief Checks if an integer is a power of two at compile-time.
 * @tparam T An integral type (signed or unsigned).
 * @param n The value to check.
 * @return true if n is a power of two (1, 2, 4, 8, ...), false otherwise.
 */
template <typename T>
[[nodiscard]] constexpr bool is_power_of_two(T n) noexcept {
    static_assert(std::is_integral_v<T>, "is_power_of_two requires an integral type");
    
    // Direct check: excludes negatives and zero, no casting needed
    return (n > 0) && ((n & (n - 1)) == 0);
}


// --- String Conversion & Concatenation Utilities ---

/**
 * @brief Constexpr lookup table for string representations of single digits (0-9).
 */
static constexpr std::string_view constexpr_digits[] = {
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9"
};

/**
 * @brief Compile-time integer-to-string conversion (runtime for multi-digit).
 * @tparam T Integral type
 * @note Full range support but constexpr evaluation limited to simple cases in C++17
 */
template <typename T>
struct constexpr_to_string_t {
    static constexpr std::size_t MaxSize = 32;
    char buffer[MaxSize] = {};
    std::size_t length = 0;

    // Default constructor for array initialization (MSVC compatibility)
    constexpr constexpr_to_string_t() noexcept = default;

    constexpr constexpr_to_string_t(T value) noexcept {
        if (value == 0) {
            buffer[0] = '0';
            length = 1;
            return;
        }

        bool negative = false;
        using UnsignedT = std::make_unsigned_t<T>;
        UnsignedT abs_value;
        
        if constexpr (std::is_signed_v<T>) {
            if (value < 0) {
                negative = true;
                abs_value = static_cast<UnsignedT>(-(value + 1)) + 1;
            } else {
                abs_value = static_cast<UnsignedT>(value);
            }
        } else {
            abs_value = value;
        }

        // Build digits from right to left into temp buffer
        char temp_buffer[MaxSize] = {};
        std::size_t temp_pos = 0;
        
        while (abs_value > 0) {
            temp_buffer[temp_pos++] = '0' + (abs_value % 10);
            abs_value /= 10;
        }
        
        // Now reverse into actual buffer
        std::size_t write_pos = 0;
        if (negative) {
            buffer[write_pos++] = '-';
        }
        
        // Copy digits in reverse order
        for (std::size_t i = temp_pos; i > 0; --i) {
            buffer[write_pos++] = temp_buffer[i - 1];
        }
        
        length = write_pos;
    }

    [[nodiscard]] constexpr std::string_view view() const noexcept {
        return {buffer, length};
    }
};

/**
 * @brief Convert integral value to string_view.
 * @tparam T Integral type (any size, signed or unsigned)
 * @param value Value to convert
 * @return std::string_view String representation
 * 
 * @note Uses thread-local rotating buffer pool to support multiple calls
 *       in the same expression (e.g., concat with multiple integers).
 *       Safe for multi-threaded use and supports up to 8 simultaneous values.
 * @warning The returned string_view is valid until 8 more calls to to_string_view
 *          have been made in the same thread.
 */
template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
[[nodiscard]] inline std::string_view to_string_view(T value) noexcept {
    // Use a pool of 8 buffers to support multiple calls in same expression
    constexpr std::size_t POOL_SIZE = 8;
    thread_local constexpr_to_string_t<T> converter_pool[POOL_SIZE] = {};
    thread_local std::size_t pool_index = 0;
    
    // Rotate through buffer pool
    auto& converter = converter_pool[pool_index];
    pool_index = (pool_index + 1) % POOL_SIZE;
    
    converter = constexpr_to_string_t<T>{value};
    return converter.view();
}

/**
 * @brief Specialization to return a string_view directly from a C-string literal.
 */
[[nodiscard]] constexpr std::string_view to_string_view(const char* s) noexcept {
    return s;
}


/**
 * @brief A constexpr struct to hold concatenated string views without allocation.
 * @tparam N The number of views to concatenate.
 */
template <std::size_t N>
struct ConstexprString
{
    std::string_view views[N];

    [[nodiscard]] constexpr std::size_t size() const noexcept {
        std::size_t s = 0;
        for (std::size_t i = 0; i < N; ++i) {
            s += views[i].size();
        }
        return s;
    }

    /**
     * @brief Writes the concatenated string to a character array.
     * @param target_buffer The destination array (must be large enough).
     * @param target_size The size of the target buffer (including null terminator).
     */
    constexpr void to_array(char* target_buffer, std::size_t target_size) const {
        std::size_t current_pos = 0;
        for (std::size_t i = 0; i < N; ++i) {
            for (char c : views[i]) {
                if (current_pos < target_size - 1) { // -1 for null terminator
                    target_buffer[current_pos++] = c;
                }
            }
        }
        target_buffer[current_pos] = '\0';
    }

    /**
     * @brief Convert to std::string at runtime (not constexpr).
     * @return std::string Allocated string containing concatenated views
     */
    [[nodiscard]] std::string to_string() const {
        std::string result;
        result.reserve(size()); // Single allocation
        for (std::size_t i = 0; i < N; ++i) {
            result.append(views[i]);
        }
        return result;
    }
};

/**
 * @brief Creates a ConstexprString holding multiple string views.
 * @param v The string views to combine (forwarded perfectly).
 */
template <typename... Args>
[[nodiscard]] constexpr auto constexpr_concat(Args&&... v) {
    return ConstexprString<sizeof...(Args)>{ { std::forward<Args>(v)... } };
}

// --- Floating-Point Utilities ---

/**
 * @brief Floating-point-to-string conversion helper (runtime only).
 * @tparam T Floating-point type
 * @note While marked constexpr, floating-point string conversion is runtime-only in C++17
 */
template <typename T>
struct constexpr_float_to_string_t {
    static constexpr std::size_t MaxSize = 64;
    char buffer[MaxSize] = {};
    std::size_t length = 0;

    // Default constructor for array initialization (MSVC compatibility)
    constexpr constexpr_float_to_string_t() noexcept = default;

    constexpr constexpr_float_to_string_t(T val, int prec) noexcept {
        // CRITICAL: Detect NaN BEFORE any other operations
        bool is_nan = std::isnan(val);
        
        if (is_nan) {
            buffer[0] = 'n'; buffer[1] = 'a'; buffer[2] = 'n';
            length = 3;
            return;
        }
        
        // Check for infinity
        bool is_inf = std::isinf(val);
        if (is_inf) {
            if (std::signbit(val)) buffer[length++] = '-';
            buffer[length++] = 'i'; buffer[length++] = 'n'; buffer[length++] = 'f';
            return;
        }

        // Now safe to check sign (after confirming not NaN or inf)
        bool negative = std::signbit(val);
        T abs_val = std::abs(val);
        
        // Integer part
        auto int_part = static_cast<long long>(abs_val);
        T frac_part = abs_val - static_cast<T>(int_part);

        // Build integer part
        if (int_part == 0) {
            buffer[length++] = '0';
        } else {
            std::size_t temp_pos = MaxSize - 1;
            auto temp = int_part;
            while (temp > 0) {
                buffer[temp_pos--] = '0' + (temp % 10);
                temp /= 10;
            }
            std::size_t int_len = MaxSize - 1 - temp_pos;
            for (std::size_t i = 0; i < int_len; ++i) {
                buffer[length++] = buffer[temp_pos + 1 + i];
            }
        }

        // Decimal point
        buffer[length++] = '.';

        // Fractional part
        for (int i = 0; i < prec && i < 15; ++i) {
            frac_part *= T(10);
            int digit = static_cast<int>(frac_part);
            buffer[length++] = '0' + digit;
            frac_part -= static_cast<T>(digit);
        }

        // Add sign if negative
        if (negative) {
            for (std::size_t i = length; i > 0; --i) {
                buffer[i] = buffer[i-1];
            }
            buffer[0] = '-';
            ++length;
        }
    }

    constexpr std::string_view view() const noexcept {
        return {buffer, length};
    }
};

/**
 * @brief Convert floating-point to string_view (runtime evaluation only).
 * @tparam T Floating-point type (float, double, long double)
 * @param value Value to convert
 * @param precision Decimal places (default: 6)
 * @return std::string_view String representation
 * 
 * @note While marked constexpr, this function performs runtime-only evaluation in C++17.
 *       Use for runtime string generation, not compile-time constants.
 *       Uses thread-local rotating buffer pool to support multiple calls
 *       in the same expression.
 * @warning Limited precision due to constexpr math constraints
 * @warning The returned string_view is valid until 8 more calls to to_string_view
 *          have been made in the same thread.
 */
template <typename T, typename = std::enable_if_t<std::is_floating_point_v<T>>>
[[nodiscard]] inline std::string_view to_string_view(T value, int precision = 6) noexcept {
    // Use a pool of 8 buffers to support multiple calls in same expression
    constexpr std::size_t POOL_SIZE = 8;
    thread_local constexpr_float_to_string_t<T> converter_pool[POOL_SIZE] = {};
    thread_local std::size_t pool_index = 0;
    
    // Rotate through buffer pool
    auto& converter = converter_pool[pool_index];
    pool_index = (pool_index + 1) % POOL_SIZE;
    
    converter = constexpr_float_to_string_t<T>{value, precision};
    return converter.view();
}

} // namespace fat_p
