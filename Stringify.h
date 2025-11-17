// Stringify.h
#pragma once

#include <string>
#include <sstream>
#include <type_traits>
#include <utility>
#include <iomanip>
#include <limits>
#include <locale>
#include <iterator>
#include <cstdint>
#include <codecvt>
#include <locale>

#include "TypeTraits.h"

// Check if TypeTraits.h detected C++20 features
#if FATP_HAS_CPP20
    #include <format>
    #include <string_view>
#endif

/**
 * @file Stringify.h
 * @brief Utility library for robust and flexible type-to-string conversion in C++17/20.
 *
 * Provides functions to convert various types (built-in, streamable, containers,
 * custom types with specific member functions) into std::string, with support
 * for formatting options, wide strings, and error handling.
 * 
 * @section thread_safety Thread Safety
 * 
 * All functions in this library are thread-safe for concurrent calls with different
 * arguments, as they are stateless and have no mutable shared state.
 * 
 * **Safe Operations:**
 * - Concurrent calls to toString() from multiple threads
 * - Passing different StringifyOptions to different threads
 * - Converting different values simultaneously
 * 
 * **Unsafe Operations:**
 * - Sharing non-const StringifyOptions across threads (use thread_local or const)
 * - Using custom_locale pointer that points to shared mutable std::locale
 * - Global locale changes during toString() calls (std::locale::global())
 * 
 * **Recommendations for Multithreaded Code:**
 * - Use const StringifyOptions or create per-thread copies
 * - If using custom locales, ensure they are immutable or use thread_local
 * - Avoid std::locale::global() calls in concurrent contexts
 * 
 * @section performance Performance Characteristics
 * 
 * - **Integers (default opts)**: O(log₁₀ n), zero-allocation fast path using std::to_string
 * - **Integers (custom opts)**: O(log₁₀ n), single allocation via ostringstream
 * - **Floating-point**: O(precision), single allocation via ostringstream
 * - **Custom types**: Depends on toString()/to_string() implementation
 * - **Containers**: O(n × element_cost), recursive with depth limiting
 * - **Strings**: O(1) passthrough, zero additional allocations
 * 
 * @section complexity Complexity Guarantees
 * - compile-time dispatch via if constexpr (zero runtime overhead)
 * - Type trait resolution at compile time
 * - Minimal template instantiations through careful SFINAE design
 */

namespace fat_p {

// =============================================================================
// SFINAE Traits for Streamability Detection
// =============================================================================

namespace detail {

    /**
     * @brief Primary trait: checks if a type T is streamable to std::ostringstream.
     * @tparam T The type to check.
     */
    template <typename T, typename = void>
    struct is_ostreamable_impl : std::false_type {};

    template <typename T>
    struct is_ostreamable_impl<T, std::void_t<decltype(
        std::declval<std::ostringstream&>() << std::declval<const T&>())>> 
        : std::true_type {};

    /**
     * @brief Checks if a type T is streamable to std::wostringstream.
     * @tparam T The type to check.
     */
    template <typename T, typename = void>
    struct is_wostreamable_impl : std::false_type {};

    template <typename T>
    struct is_wostreamable_impl<T, std::void_t<decltype(
        std::declval<std::wostringstream&>() << std::declval<const T&>())>> 
        : std::true_type {};

    /**
     * @brief Checks if a type T has a member function named toString() that returns a string.
     * @tparam T The type to check.
     */
    template <typename T, typename = void>
    struct has_to_string_method_impl : std::false_type {};

    template <typename T>
    struct has_to_string_method_impl<T, std::void_t<decltype(
        std::declval<const T&>().toString())>> 
        : std::bool_constant<std::is_convertible_v<
            decltype(std::declval<const T&>().toString()), std::string>> {};

    /**
     * @brief Checks if a type T has a member function named to_string() that returns a string.
     * @tparam T The type to check.
     */
    template <typename T, typename = void>
    struct has_to_string_snake_method_impl : std::false_type {};

    template <typename T>
    struct has_to_string_snake_method_impl<T, std::void_t<decltype(
        std::declval<const T&>().to_string())>> 
        : std::bool_constant<std::is_convertible_v<
            decltype(std::declval<const T&>().to_string()), std::string>> {};

    /**
     * @brief Convenience alias for is_ostreamable_impl with type decay.
     * @tparam T The type to check.
     */
    template <typename T>
    using is_ostreamable = is_ostreamable_impl<std::decay_t<T>>;

    /**
     * @brief Convenience alias for is_wostreamable_impl with type decay.
     * @tparam T The type to check.
     */
    template <typename T>
    using is_wostreamable = is_wostreamable_impl<std::decay_t<T>>;

    /**
     * @brief Convenience alias for has_to_string_method_impl with type decay.
     * @tparam T The type to check.
     */
    template <typename T>
    using has_to_string_method = has_to_string_method_impl<std::decay_t<T>>;

    /**
     * @brief Convenience alias for has_to_string_snake_method_impl with type decay.
     * @tparam T The type to check.
     */
    template <typename T>
    using has_to_string_snake_method = has_to_string_snake_method_impl<std::decay_t<T>>;

    /**
     * @brief Alias/Concept to the is_iterable trait defined in TypeTraits.h, using type decay.
     * @tparam T The type to check.
     */
    template <typename T>
    #if FATP_HAS_CPP20
    concept is_iterable_concept = fat_p::is_iterable<std::decay_t<T>>;
    #else
    using is_iterable = fat_p::is_iterable<std::decay_t<T>>;
    #endif

} // namespace detail

/**
 * @brief Checks if a type T is streamable to std::ostringstream (C++17 variable template).
 * @tparam T The type to check.
 */
template <typename T>
inline constexpr bool is_ostreamable_v = detail::is_ostreamable<T>::value;

/**
 * @brief Checks if a type T is streamable to std::wostringstream (C++17 variable template).
 * @tparam T The type to check.
 */
template <typename T>
inline constexpr bool is_wostreamable_v = detail::is_wostreamable<T>::value;

/**
 * @brief Checks if a type T has a string-returning toString() member (C++17 variable template).
 * @tparam T The type to check.
 */
template <typename T>
inline constexpr bool has_to_string_method_v = detail::has_to_string_method<T>::value;

/**
 * @brief Checks if a type T has a string-returning to_string() member (C++17 variable template).
 * @tparam T The type to check.
 */
template <typename T>
inline constexpr bool has_to_string_snake_method_v = detail::has_to_string_snake_method<T>::value;

// =============================================================================
// String Conversion Options
// =============================================================================

/**
 * @brief Options for controlling string conversion behavior.
 */
struct StringifyOptions {
    const char* placeholder = "<non-stringifiable>"; ///< Placeholder string for non-stringifiable types.
    bool use_hex_for_pointers = true;              ///< Use hexadecimal notation for pointers.
    int float_precision = -1;                      ///< Decimal precision (-1 means default).
    bool scientific_notation = false;              ///< Use scientific notation for floats.
    bool show_bool_as_text = true;                 ///< Show booleans as "true"/"false" instead of "1"/"0".
    const char* container_open = "[";              ///< Opening delimiter for sequential containers.
    const char* container_close = "]";             ///< Closing delimiter for sequential containers.
    const char* container_separator = ", ";        ///< Separator between container elements.
    int max_container_depth = 3;                   ///< Max recursion depth for containers.
    std::locale* custom_locale = nullptr;         ///< Custom locale to use (nullptr means global locale).
    
    constexpr StringifyOptions() noexcept = default;
};

// =============================================================================
// Forward Declarations
// =============================================================================

/**
 * @brief Forward declaration of the primary toString function template.
 * @tparam T The type of value to convert.
 * @param value The value.
 * @param opts Formatting options.
 * @return std::string The string representation.
 */
template <typename T>
[[nodiscard]] std::string toString(T&& value, const StringifyOptions& opts = {});

// =============================================================================
// Core Stringify Functions
// =============================================================================

namespace detail {

    /**
     * @brief Optimized path for built-in integer types using std::to_string.
     * @tparam T An integral type (excluding bool).
     * @param value The integer value.
     * @return std::string The string representation.
     */
    template <typename T>
    [[nodiscard]] inline std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>, std::string>
    fast_int_to_string(T value) noexcept {
        if constexpr (std::is_same_v<T, int>) {
            return std::to_string(value);
        } else if constexpr (std::is_same_v<T, long>) {
            return std::to_string(value);
        } else if constexpr (std::is_same_v<T, long long>) {
            return std::to_string(value);
        } else if constexpr (std::is_same_v<T, unsigned>) {
            return std::to_string(value);
        } else if constexpr (std::is_same_v<T, unsigned long>) {
            return std::to_string(value);
        } else if constexpr (std::is_same_v<T, unsigned long long>) {
            return std::to_string(value);
        } else {
            if constexpr (std::is_unsigned_v<T>) {
                return std::to_string(static_cast<unsigned long long>(value));
            } else {
                return std::to_string(static_cast<long long>(value));
            }
        }
    }

    /**
     * @brief Helper to convert a value using its toString() or to_string() member function.
     * @tparam T The type of value.
     * @param value The value.
     * @param opts Formatting options (ignored by this helper, but kept for consistent interface).
     * @return std::string The string representation from the member function.
     */
    template <typename T>
    [[nodiscard]] inline std::string stringify_with_method(const T& value, const StringifyOptions&) {
        if constexpr (has_to_string_method<T>::value) {
            return value.toString();
        } else if constexpr (has_to_string_snake_method<T>::value) {
            return value.to_string();
        } else {
            static_assert(!std::is_same_v<T, T>, "Type should have toString method");
            return "";
        }
    }

    /**
     * @brief Helper to convert a value using the std::ostringstream operator<<.
     *
     * @details This function is used for both C++17 fallback and the primary implementation,
     * as std::format currently lacks full standard support for locale and boolalpha flags.
     * 
     * @tparam T The type of value.
     * @param value The value.
     * @param opts Formatting options to apply to the stream.
     * @return std::string The string representation from the stream.
     */
    template <typename T>
    [[nodiscard]] inline std::string stringify_with_stream(T&& value, const StringifyOptions& opts) {
        std::ostringstream ss;
        
        if (opts.custom_locale != nullptr) {
            ss.imbue(*opts.custom_locale);
        }
        
        if (opts.float_precision >= 0) {
            ss << std::setprecision(opts.float_precision);
            if (!opts.scientific_notation) {
                ss << std::fixed;
            }
        }
        if (opts.scientific_notation) {
            ss << std::scientific;
        }
        
        if constexpr (std::is_same_v<std::decay_t<T>, bool>) {
            if (opts.show_bool_as_text) {
                ss << std::boolalpha;
            }
        }
        
        if constexpr (std::is_pointer_v<std::decay_t<T>>) {
            if (opts.use_hex_for_pointers) {
                ss << std::hex << std::showbase;
            }
        }
        
        ss << std::forward<T>(value);
        return ss.str();
    }


    /**
     * @brief Helper for stringifying sequential containers recursively.
     * @tparam Container The container type (must be iterable).
     * @param container The container instance.
     * @param opts Formatting options (depth is reduced for recursion).
     * @return std::string The string representation of the container contents.
     */
    template <typename Container>
    [[nodiscard]] inline std::string stringify_container(const Container& container, 
                                                         const StringifyOptions& opts) {
        if (opts.max_container_depth <= 0) {
            return "<max depth>";
        }
        
        std::ostringstream ss;
        ss << opts.container_open;
        
        bool first = true;
        StringifyOptions elem_opts = opts;
        elem_opts.max_container_depth--;
        
        for (const auto& elem : container) {
            if (!first) {
                ss << opts.container_separator;
            }
            
            ss << toString(elem, elem_opts);
            first = false;
        }
        
        ss << opts.container_close;
        return ss.str();
    }

    /**
     * @brief Helper for stringifying map-like containers recursively (uses braces {}).
     * @tparam Container The map-like container type (e.g., std::map, std::unordered_map).
     * @param container The container instance.
     * @param opts Formatting options (depth is reduced for recursion).
     * @return std::string The string representation of the map contents.
     */
    template <typename Container>
    [[nodiscard]] inline std::string stringify_map(const Container& container, 
                                                   const StringifyOptions& opts) {
        if (opts.max_container_depth <= 0) {
            return "<max depth>";
        }
        
        std::ostringstream ss;
        ss << "{";
        
        bool first = true;
        StringifyOptions elem_opts = opts;
        elem_opts.max_container_depth--;
        
        for (const auto& pair : container) {
            if (!first) {
                ss << opts.container_separator;
            }
            
            ss << toString(pair.first, elem_opts) << ": " << toString(pair.second, elem_opts);
            first = false;
        }
        
        ss << "}";
        return ss.str();
    }

} // namespace detail

/**
 * @brief Converts a value to string with fallback to a placeholder.
 * 
 * @details Priority order for conversion methods:
 *   0. FAST PATH: Built-in integers (delegates to std::to_string)
 *   1. T::toString() or T::to_string() member function (with return type check)
 *   2. std::ostringstream operator<<
 *   3A. Map-like container stringification (if is_map_like_v is true)
 *   3B. Sequential container stringification (if is_iterable_v is true)
 *   4. Placeholder string (from options)
 * 
 * @tparam T Type of value to convert.
 * @param value Value to convert (forwarded).
 * @param opts Formatting options.
 * @return std::string The string representation.
 * 
 * @complexity O(1) for built-in types, O(N) for containers where N is size.
 * @exception noexcept(false) May throw if underlying stream operations throw.
 * 
 * @example
 * int x = 42;
 * auto str = toString(x); // "42" - uses fast path
 * 
 * std::vector<int> v = {1, 2, 3};
 * auto str2 = toString(v); // ""
 * 
 * std::map<std::string, int> m = {{"a", 1}, {"b", 2}};
 * auto str3 = toString(m); // "{"a": 1, "b": 2}"
 */
template <typename T>
[[nodiscard]] inline std::string toString(T&& value, const StringifyOptions& opts)
{
    using PlainT = std::decay_t<T>;
    
    if constexpr (std::is_same_v<PlainT, std::string>)
    {
        return value;
    }
    else if constexpr (std::is_array_v<std::remove_reference_t<T>> && 
                       std::is_same_v<std::remove_extent_t<std::remove_reference_t<T>>, char>)
    {
        return std::string(value);
    }
    else if constexpr (std::is_array_v<std::remove_reference_t<T>> && 
                       std::is_same_v<std::remove_extent_t<std::remove_reference_t<T>>, const char>)
    {
        return std::string(value);
    }
    else if constexpr (std::is_same_v<PlainT, const char*>)
    {
        return value ? std::string(value) : std::string();
    }
    else if constexpr (std::is_same_v<PlainT, char*>)
    {
        return value ? std::string(value) : std::string();
    }
    else if constexpr (std::is_integral_v<PlainT> && !std::is_same_v<PlainT, bool>)
    {
        if (opts.float_precision == -1 && !opts.scientific_notation && 
            opts.custom_locale == nullptr) 
        {
            return detail::fast_int_to_string(value);
        }
        else
        {
            return detail::stringify_with_stream(std::forward<T>(value), opts);
        }
    }
    else if constexpr (detail::has_to_string_method<PlainT>::value || 
                       detail::has_to_string_snake_method<PlainT>::value)
    {
        return detail::stringify_with_method(value, opts);
    }
    else if constexpr (detail::is_ostreamable<PlainT>::value)
    {
        return detail::stringify_with_stream(std::forward<T>(value), opts);
    }
    else if constexpr (is_map_like_v<PlainT> &&
                       !std::is_convertible_v<PlainT, std::string> &&
                       !std::is_convertible_v<PlainT, const char*>)
    {
        return detail::stringify_map(value, opts);
    }
    #if FATP_HAS_CPP20
    else if constexpr (detail::is_iterable_concept<PlainT> && 
                       !std::is_convertible_v<PlainT, std::string_view> &&
                       !std::is_convertible_v<PlainT, const char*>) 
    {
        return detail::stringify_container(value, opts);
    }
    #else
    else if constexpr (detail::is_iterable<PlainT>::value && 
                       !std::is_convertible_v<PlainT, std::string> &&
                       !std::is_convertible_v<PlainT, const char*>) 
    {
        return detail::stringify_container(value, opts);
    }
    #endif
    else
    {
        return opts.placeholder;
    }
}

/**
 * @brief Converts a value to string using a custom fallback placeholder.
 * @tparam T Type of value to convert.
 * @param value Value to convert.
 * @param fallback Custom placeholder if not stringifiable.
 * @return std::string The string representation or the fallback.
 * 
 * @example
 * struct NonStreamable {};
 * auto str = toStringOr(NonStreamable{}, "N/A"); // "N/A"
 */
template <typename T>
[[nodiscard]] inline std::string toStringOr(T&& value, const char* fallback) {
    StringifyOptions opts;
    opts.placeholder = fallback;
    return toString(std::forward<T>(value), opts);
}

/**
 * @brief Safely converts a value to string, returning success status without throwing.
 * @tparam T Type of value to convert.
 * @param value Value to convert.
 * @param out [out] Output string (only modified on success).
 * @param opts Formatting options.
 * @return true if conversion succeeded (via stream, method, or container logic), false otherwise (fell to placeholder).
 * 
 * @complexity O(1) for built-in types.
 * @exception noexcept Strong exception safety guarantee.
 * 
 * @example
 * std::string result;
 * if (tryToString(42, result)) {
 *     // result is "42"
 * }
 */
template <typename T>
[[nodiscard]] inline bool tryToString(const T& value, std::string& out, 
                                      const StringifyOptions& opts = {}) noexcept {
    using PlainT = std::decay_t<T>;
    
    #if FATP_HAS_CPP20
    constexpr bool is_stringifiable = detail::is_ostreamable<PlainT>::value || 
                                      detail::has_to_string_method<PlainT>::value ||
                                      detail::has_to_string_snake_method<PlainT>::value ||
                                      detail::is_iterable_concept<PlainT>;
    if constexpr (!is_stringifiable) {
        return false;
    }
    #else
    if constexpr (!detail::is_ostreamable<PlainT>::value && 
                  !detail::has_to_string_method<PlainT>::value &&
                  !detail::has_to_string_snake_method<PlainT>::value &&
                  !detail::is_iterable<PlainT>::value) {
        return false;
    } 
    #endif
    else {
        try {
            out = toString(value, opts);
            return true;
        } catch (...) {
            return false;
        }
    }
}

/**
 * @brief Checks if a type is stringifiable at compile time (C++17/20 variable template).
 * @tparam T Type to check.
 * @return true if type can be converted to string by any mechanism.
 */
template <typename T>
inline constexpr bool is_stringifiable_v = 
    detail::is_ostreamable<T>::value || 
    detail::has_to_string_method<T>::value ||
    detail::has_to_string_snake_method<T>::value ||
    #if FATP_HAS_CPP20
    (detail::is_iterable_concept<T> && 
     !std::is_convertible_v<T, std::string_view> &&
     !std::is_convertible_v<T, const char*>);
    #else
    (detail::is_iterable<T>::value && 
     !std::is_convertible_v<T, std::string> &&
     !std::is_convertible_v<T, const char*>);
    #endif


// =============================================================================
// Wide String Support
// =============================================================================

/**
 * @brief Converts value to wide string (std::wstring) using std::wostringstream.
 * @tparam T Type of value to convert.
 * @param value Value to convert.
 * @param opts Formatting options.
 * @return std::wstring Wide string representation.
 */
template <typename T>
[[nodiscard]] inline std::wstring toWString(T&& value, const StringifyOptions& opts = {}) {
    using PlainT = std::decay_t<T>;
    
    if constexpr (detail::is_wostreamable<PlainT>::value) {
        std::wostringstream wss;
        
        if (opts.custom_locale != nullptr) {
            wss.imbue(*opts.custom_locale);
        }
        
        if (opts.float_precision >= 0) {
            wss << std::setprecision(opts.float_precision);
            if (!opts.scientific_notation) {
                wss << std::fixed;
            }
        }
        if (opts.scientific_notation) {
            wss << std::scientific;
        }
        
        if constexpr (std::is_same_v<PlainT, bool>) {
            if (opts.show_bool_as_text) {
                wss << std::boolalpha;
            }
        }
        
        if constexpr (std::is_pointer_v<PlainT>) {
            if (opts.use_hex_for_pointers) {
                wss << std::hex << std::showbase;
            }
        }
        
        wss << std::forward<T>(value);
        return wss.str();
    } else {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        return converter.from_bytes(opts.placeholder);
    }
}

// =============================================================================
// Advanced Conversions
// =============================================================================

/**
 * @brief Converts value to string with alignment and padding.
 * @tparam T Type of value to convert.
 * @param value Value to convert.
 * @param width Minimum total width of the output string.
 * @param align Alignment character: '<' for left, '>' for right, '^' for center.
 * @param pad_char Character to use for padding.
 * @return std::string The padded and aligned string.
 * 
 * @example
 * toStringPadded(42, 5, '>', '0'); // "00042"
 * toStringPadded("hi", 5, '<');    // "hi   "
 */
template <typename T>
[[nodiscard]] inline std::string toStringPadded(T&& value, std::size_t width, 
                                                char align = '>', char pad_char = ' ') {
    auto str = toString(std::forward<T>(value));
    if (str.length() >= width) {
        return str;
    }
    
    std::size_t padding = width - str.length();
    
    switch (align) {
        case '<':
            str.append(padding, pad_char);
            break;
        case '^':
            str.insert(0, padding / 2, pad_char);
            str.append(padding - padding / 2, pad_char);
            break;
        case '>':
        default:
            str.insert(0, padding, pad_char);
            break;
    }
    
    return str;
}

/**
 * @brief Converts a numeric value to a string with specified floating-point format.
 * @tparam T Numeric type (must be std::is_arithmetic_v).
 * @param value Value to convert.
 * @param precision Decimal precision.
 * @param fixed Use fixed notation (if true) or scientific (if false).
 * @return std::string The formatted string.
 */
template <typename T>
[[nodiscard]] inline std::enable_if_t<std::is_arithmetic_v<T>, std::string>
toStringFormatted(T value, int precision = 6, bool fixed = true) {
    StringifyOptions opts;
    opts.float_precision = precision;
    opts.scientific_notation = !fixed;
    return toString(value, opts);
}

/**
 * @brief Converts a pointer to a string using hexadecimal notation.
 * @tparam T The type pointed to.
 * @param ptr Pointer value.
 * @param null_placeholder String to use for nullptr.
 * @return std::string Pointer as hex string or placeholder.
 */
template <typename T>
[[nodiscard]] inline std::string toStringPointer(T* ptr, const char* null_placeholder = "nullptr") {
    if (ptr == nullptr) {
        return null_placeholder;
    }
    
    std::ostringstream ss;
    ss << "0x" << std::hex << reinterpret_cast<std::uintptr_t>(ptr);
    return ss.str();
}

/**
 * @brief Variadic function that concatenates multiple values into a single string.
 * @tparam Args Types of arguments to concatenate.
 * @param args Values to concatenate.
 * @return std::string Concatenated string.
 * 
 * @example
 * toStringConcat("Value: ", 42, ", Status: ", true); 
 * // "Value: 42, Status: true"
 */
template <typename... Args>
[[nodiscard]] inline std::string toStringConcat(Args&&... args) {
    std::ostringstream ss;
    (ss << ... << toString(std::forward<Args>(args)));
    return ss.str();
}

} // namespace fat_p
