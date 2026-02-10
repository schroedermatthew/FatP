#pragma once

/*
FATP_META:
  meta_version: 1
  component: Stringify
  file_role: public_header
  path: include/fat_p/Stringify.h
  namespace: fat_p
  layer: Foundation
  summary: "Type-to-string conversion utilities using C++20 concepts."
  api_stability: stable
  related:
    docs_search: "Stringify"
    tests:
      - components/Stringify/tests/test_Stringify.cpp
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
*/

/**
 * @file Stringify.h
 * @brief Type-to-string conversion utilities using C++20 concepts.
 *
 * Provides functions to convert various types (built-in, streamable, containers,
 * custom types with specific member functions) into std::string, with support
 * for formatting options, wide strings, and error handling.
 *
 * @section features Key Features
 *
 * - C++20 concepts for compile-time type detection (via Concepts.h)
 * - std::format when available (FATP_HAS_FORMAT), ostringstream fallback
 * - Fast paths for booleans, integers, and floats
 * - Recursive container/tuple/optional stringification
 * - Thread-safe error tracking and recursion depth limiting
 * - Classic locale default for deterministic HPC/Scientific output
 * - User-specializable EnumStringifier trait
 *
 * @section performance Performance Characteristics
 * - **Booleans**: O(1), zero-allocation fast path returning literals
 * - **Integers**: O(log10 n), uses std::format or std::to_string
 * - **Floating-point**: O(precision), uses std::format or std::to_string
 * - **Custom types**: Depends on toString()/to_string() implementation
 * - **Containers**: O(n * element_cost), recursive with depth limiting
 * - **Strings**: O(1) passthrough, zero additional allocations
 *
 * @section thread_safety Thread Safety
 * All Stringify functions are stateless and reentrant. Multiple threads can
 * safely call any function concurrently with independent arguments.
 *
 * @section limitations Known Limitations
 * - Does NOT detect pointer cycles in data structures
 * - Wide string fallback uses ASCII-only placeholder widening
 */

#include "CppFeatureDetection.h"
#include "Concepts.h"

#include <cassert>
#include <charconv>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <locale>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#if FATP_HAS_FORMAT
#include <format>
#endif

namespace fat_p
{

// =============================================================================
// Enum Stringifier Trait (User Specialization Point)
// =============================================================================

/**
 * @brief User-specializable trait for enum-to-string conversion.
 *
 * @example
 * enum class Color { Red, Green, Blue };
 *
 * template <>
 * struct EnumStringifier<Color> {
 *     static const char* to_string(Color c) {
 *         switch (c) {
 *             case Color::Red: return "Red";
 *             case Color::Green: return "Green";
 *             case Color::Blue: return "Blue";
 *         }
 *         return "<unknown Color>";
 *     }
 * };
 */
template <typename E>
struct EnumStringifier
{
    static const char* to_string(E)
    {
        return nullptr;
    }
};

// =============================================================================
// String Conversion Options
// =============================================================================

/**
 * @brief Options for controlling string conversion behavior.
 */
struct StringifyOptions
{
    const char* placeholder = "<non-stringifiable>"; ///< Placeholder for non-stringifiable types.
    bool use_hex_for_pointers = true;                ///< Use hexadecimal notation for pointers.
    int float_precision = -1;                        ///< Decimal precision (-1 means default).
    bool scientific_notation = false;                ///< Use scientific notation for floats.
    bool show_bool_as_text = true;                   ///< Show booleans as "true"/"false".
    const char* container_open = "[";                ///< Opening delimiter for sequential containers.
    const char* container_close = "]";               ///< Closing delimiter for sequential containers.
    const char* container_separator = ", ";          ///< Separator between container elements.
    int max_container_depth = 3;                     ///< Max recursion depth for containers.
    bool use_classic_locale = true;                  ///< Use classic "C" locale for determinism.
    std::locale* custom_locale = nullptr;            ///< Custom locale (nullptr = classic/global).

    constexpr StringifyOptions() noexcept = default;
};

// =============================================================================
// Forward Declarations
// =============================================================================

template <typename T>
[[nodiscard]] std::string toString(T&& value, const StringifyOptions& opts = {});

template <typename T>
[[nodiscard]] std::string
toStringPointer(T* ptr, const StringifyOptions& opts = {}, const char* nullPlaceholder = "nullptr");

// =============================================================================
// Implementation Detail
// =============================================================================

namespace detail
{

// --- Recursion depth tracking (thread-safe) ---

inline int& getStringifyDepth() noexcept
{
    thread_local int depth = 0;
    return depth;
}

struct StringifyDepthGuard
{
    int& mDepth;
    bool mExceeded;

    StringifyDepthGuard()
        : mDepth(getStringifyDepth())
        , mExceeded(false)
    {
        ++mDepth;
#ifndef NDEBUG
        if (mDepth > 100)
        {
            mExceeded = true;
            std::cerr << "STRINGIFY ERROR: Recursion depth exceeded (possible cycle)\n";
            assert(false && "Stringify recursion limit exceeded");
        }
#else
        if (mDepth > 200)
        {
            mExceeded = true;
        }
#endif
    }

    ~StringifyDepthGuard()
    {
        --mDepth;
    }

    [[nodiscard]] bool exceededLimit() const noexcept
    {
        return mExceeded;
    }
};

// --- Thread-local error tracking ---

inline std::string& getLastStringifyError() noexcept
{
    thread_local std::string lastError;
    return lastError;
}

// --- std::pair detection ---

template <typename T>
struct is_std_pair_impl : std::false_type
{
};

template <typename T1, typename T2>
struct is_std_pair_impl<std::pair<T1, T2>> : std::true_type
{
};

template <typename T>
concept std_pair = is_std_pair_impl<std::decay_t<T>>::value;

// --- std::array detection (should be treated as container, not tuple) ---

template <typename T>
struct is_std_array_impl : std::false_type
{
};

template <typename T, std::size_t N>
struct is_std_array_impl<std::array<T, N>> : std::true_type
{
};

template <typename T>
concept std_array = is_std_array_impl<std::decay_t<T>>::value;

// --- Optional-like detection (has_value + operator*) ---

template <typename T>
concept dereferenceable_optional = requires(const T& val) {
    { val.has_value() } -> std::convertible_to<bool>;
    { *val };
};

// --- Fast paths for built-in types ---

/**
 * @brief Fast path for integers using std::to_string.
 * @note std::to_string is faster than std::format for integers on most implementations.
 */
template <std::integral T>
    requires(!std::same_as<T, bool>)
[[nodiscard]] inline std::string fastIntToString(T value) noexcept
{
    return std::to_string(value);
}

/**
 * @brief Fast path for floating-point using std::format when available.
 * @note Uses std::to_chars as fallback for deterministic, minimal output.
 */
template <std::floating_point T>
[[nodiscard]] inline std::string fastFloatToString(T value)
{
#if FATP_HAS_FORMAT
    return std::format("{}", value);
#else
    // Use to_chars for deterministic output (no trailing zeros like to_string)
    // Buffer size 128: safe for 128-bit long double platforms
    char buffer[128];
    auto result = std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::general);
    if (result.ec == std::errc{})
    {
        return std::string(buffer, result.ptr);
    }
    // Fallback to ostringstream if to_chars fails (shouldn't happen for valid floats)
    std::ostringstream ss;
    ss.imbue(std::locale::classic());
    ss << value;
    return ss.str();
#endif
}

// --- Custom method stringification ---

template <typename T>
    requires concepts::has_custom_string_method<T>
[[nodiscard]] inline std::string stringifyWithMethod(const T& value)
{
    if constexpr (concepts::has_to_string_method<T>)
    {
        return value.toString();
    }
    else
    {
        return value.to_string();
    }
}

// --- Stream-based stringification ---

template <typename T>
    requires concepts::streamable<std::decay_t<T>>
[[nodiscard]] inline std::string stringifyWithStream(T&& value, const StringifyOptions& opts)
{
    std::ostringstream ss;

    if (opts.custom_locale != nullptr)
    {
        ss.imbue(*opts.custom_locale);
    }
    else if (opts.use_classic_locale)
    {
        ss.imbue(std::locale::classic());
    }

    if (opts.float_precision >= 0)
    {
        ss << std::setprecision(opts.float_precision);
        if (!opts.scientific_notation)
        {
            ss << std::fixed;
        }
    }
    if (opts.scientific_notation)
    {
        ss << std::scientific;
    }

    if constexpr (std::same_as<std::decay_t<T>, bool>)
    {
        if (opts.show_bool_as_text)
        {
            ss << std::boolalpha;
        }
    }

    if constexpr (std::is_pointer_v<std::decay_t<T>>)
    {
        if (opts.use_hex_for_pointers)
        {
            ss << std::hex << std::showbase;
        }
    }

    ss << std::forward<T>(value);
    return ss.str();
}

// --- Tuple stringification ---

template <typename Tuple, std::size_t... Is>
[[nodiscard]] inline std::string stringifyTupleImpl(
    const Tuple& t, const StringifyOptions& opts, std::index_sequence<Is...>)
{
    std::string result;
    result.reserve(64);
    result += "(";
    [[maybe_unused]] bool first = true;
    ((result += (first ? (first = false, "") : ", "), result += toString(std::get<Is>(t), opts)), ...);
    result += ")";
    return result;
}

// --- Container size estimation ---

template <typename Container>
[[nodiscard]] inline std::size_t estimateContainerSize(
    const Container& container, const StringifyOptions& opts) noexcept
{
    std::size_t count = 0;
    if constexpr (concepts::sized<Container>)
    {
        count = container.size();
    }
    else
    {
        // For non-sized containers (e.g., forward_list), use a reasonable default
        // to avoid iterating twice. Most small containers have <16 elements.
        count = 16;
    }
    std::size_t sepLen = std::char_traits<char>::length(opts.container_separator);
    return 2 + count * (8 + sepLen);
}

// --- Container stringification ---

template <concepts::printable_range Container>
[[nodiscard]] inline std::string stringifyContainer(const Container& container, const StringifyOptions& opts)
{
    StringifyDepthGuard guard;
    if (guard.exceededLimit())
    {
        return "<recursion-limit>";
    }

    if (opts.max_container_depth <= 0)
    {
        return "<max depth>";
    }

    std::string result;
    result.reserve(estimateContainerSize(container, opts));
    result += opts.container_open;

    bool first = true;
    StringifyOptions elemOpts = opts;
    elemOpts.max_container_depth--;

    for (const auto& elem : container)
    {
        if (!first)
        {
            result += opts.container_separator;
        }
        result += toString(elem, elemOpts);
        first = false;
    }

    result += opts.container_close;
    return result;
}

// --- Map stringification ---

template <concepts::map_like Container>
[[nodiscard]] inline std::string stringifyMap(const Container& container, const StringifyOptions& opts)
{
    StringifyDepthGuard guard;
    if (guard.exceededLimit())
    {
        return "<recursion-limit>";
    }

    if (opts.max_container_depth <= 0)
    {
        return "<max depth>";
    }

    std::string result;
    result.reserve(estimateContainerSize(container, opts) * 2);
    result += "{";

    bool first = true;
    StringifyOptions elemOpts = opts;
    elemOpts.max_container_depth--;

    for (const auto& pair : container)
    {
        if (!first)
        {
            result += opts.container_separator;
        }
        result += toString(pair.first, elemOpts);
        result += ": ";
        result += toString(pair.second, elemOpts);
        first = false;
    }

    result += "}";
    return result;
}

} // namespace detail

// =============================================================================
// Primary toString Implementation
// =============================================================================

/**
 * @brief Converts a value to string with fallback to a placeholder.
 *
 * @details Priority order:
 * 1. Pair/Tuple/Optional (recursive types)
 * 2. Strings (passthrough)
 * 3. Nullable C-strings
 * 4. Enums with custom stringifier
 * 5. Booleans (fast path)
 * 6. Integers (fast path with std::format or std::to_string)
 * 7. Floating-point (fast path with std::format or std::to_string)
 * 8. Custom toString()/to_string() methods
 * 9. Streamable types (operator<<)
 * 10. Map-like containers
 * 11. Printable ranges
 * 12. Placeholder fallback
 */
template <typename T>
[[nodiscard]] inline std::string toString(T&& value, const StringifyOptions& opts)
{
    using PlainT = std::decay_t<T>;

    // Priority 1: Recursive types (but not std::array - that goes to containers)
    if constexpr (detail::std_pair<PlainT>)
    {
        std::string result;
        result.reserve(32);
        result += "(";
        result += toString(value.first, opts);
        result += ", ";
        result += toString(value.second, opts);
        result += ")";
        return result;
    }
    else if constexpr (concepts::tuple_like<PlainT> && !detail::std_pair<PlainT> && !detail::std_array<PlainT>)
    {
        return detail::stringifyTupleImpl(value, opts, std::make_index_sequence<std::tuple_size_v<PlainT>>{});
    }
    else if constexpr (detail::dereferenceable_optional<PlainT>)
    {
        return value.has_value() ? toString(*value, opts) : "nullopt";
    }
    // Priority 2: Strings (with move semantics for rvalues)
    else if constexpr (std::same_as<PlainT, std::string>)
    {
        return std::forward<T>(value);
    }
    else if constexpr (std::same_as<PlainT, std::string_view>)
    {
        return std::string(value);
    }
    else if constexpr (std::is_array_v<std::remove_reference_t<T>> &&
                       std::same_as<std::remove_extent_t<std::remove_reference_t<T>>, char>)
    {
        return std::string(value);
    }
    else if constexpr (std::is_array_v<std::remove_reference_t<T>> &&
                       std::same_as<std::remove_extent_t<std::remove_reference_t<T>>, const char>)
    {
        return std::string(value);
    }
    // Priority 3: Nullable C-strings
    else if constexpr (std::same_as<PlainT, const char*> || std::same_as<PlainT, char*>)
    {
        return value ? std::string(value) : opts.placeholder;
    }
    // Priority 3b: Raw pointers (non-char) route to toStringPointer
    else if constexpr (std::is_pointer_v<PlainT> &&
                       !std::same_as<PlainT, const char*> &&
                       !std::same_as<PlainT, char*> &&
                       std::is_object_v<std::remove_pointer_t<PlainT>>)
    {
        return toStringPointer(static_cast<const void*>(value), opts);
    }
    // Priority 4: Enums
    else if constexpr (std::is_enum_v<PlainT>)
    {
        const char* enumStr = EnumStringifier<PlainT>::to_string(value);
        if (enumStr != nullptr)
        {
            return std::string(enumStr);
        }
        return std::to_string(static_cast<std::underlying_type_t<PlainT>>(value));
    }
    // Priority 5: Booleans
    else if constexpr (std::same_as<PlainT, bool>)
    {
        if (opts.show_bool_as_text)
        {
            return value ? "true" : "false";
        }
        else
        {
            return value ? "1" : "0";
        }
    }
    // Priority 6: Integers
    else if constexpr (std::integral<PlainT>)
    {
        // Fast path when no custom locale is needed
        if (opts.custom_locale == nullptr)
        {
            return detail::fastIntToString(value);
        }
        else
        {
            return detail::stringifyWithStream(std::forward<T>(value), opts);
        }
    }
    // Priority 7: Floating-point
    else if constexpr (std::floating_point<PlainT>)
    {
        if (opts.float_precision == -1 && !opts.scientific_notation && opts.custom_locale == nullptr)
        {
            return detail::fastFloatToString(value);
        }
        else
        {
            return detail::stringifyWithStream(std::forward<T>(value), opts);
        }
    }
    // Priority 8: Custom methods
    else if constexpr (concepts::has_custom_string_method<PlainT>)
    {
        return detail::stringifyWithMethod(value);
    }
    // Priority 9: Streamable
    else if constexpr (concepts::streamable<PlainT>)
    {
        return detail::stringifyWithStream(std::forward<T>(value), opts);
    }
    // Priority 10: Maps (map_like implies pair value_type, so never a string)
    else if constexpr (concepts::map_like<PlainT>)
    {
        return detail::stringifyMap(value, opts);
    }
    // Priority 11: Ranges
    else if constexpr (concepts::printable_range<PlainT>)
    {
        return detail::stringifyContainer(value, opts);
    }
    // Priority 12: Fallback
    else
    {
        return opts.placeholder;
    }
}

// =============================================================================
// Convenience Functions
// =============================================================================

/**
 * @brief Converts a value to string using a custom fallback placeholder.
 */
template <typename T>
[[nodiscard]] inline std::string toStringOr(T&& value, const char* fallback)
{
    StringifyOptions opts;
    opts.placeholder = fallback;
    return toString(std::forward<T>(value), opts);
}

/**
 * @brief Safely converts a value to string without throwing.
 */
template <typename T>
[[nodiscard]] inline bool tryToString(const T& value, std::string& out, const StringifyOptions& opts = {}) noexcept
{
    try
    {
        out = toString(value, opts);
        return true;
    }
    catch (const std::exception& e)
    {
        detail::getLastStringifyError() = e.what();
        return false;
    }
    catch (...)
    {
        detail::getLastStringifyError() = "Unknown stringify error";
        return false;
    }
}

/**
 * @brief Retrieves the last error message from tryToString.
 */
[[nodiscard]] inline const std::string& getLastStringifyError() noexcept
{
    return detail::getLastStringifyError();
}

// =============================================================================
// Wide String Support
// =============================================================================

/**
 * @brief Converts value to wide string (std::wstring).
 * @details Fallback uses manual ASCII widening to avoid deprecated codecvt.
 */
template <typename T>
[[nodiscard]] inline std::wstring toWString(T&& value, const StringifyOptions& opts = {})
{
    using PlainT = std::decay_t<T>;

    if constexpr (concepts::wstreamable<PlainT>)
    {
        std::wostringstream wss;

        if (opts.custom_locale != nullptr)
        {
            wss.imbue(*opts.custom_locale);
        }
        else if (opts.use_classic_locale)
        {
            wss.imbue(std::locale::classic());
        }

        if (opts.float_precision >= 0)
        {
            wss << std::setprecision(opts.float_precision);
            if (!opts.scientific_notation)
            {
                wss << std::fixed;
            }
        }
        if (opts.scientific_notation)
        {
            wss << std::scientific;
        }

        if constexpr (std::same_as<PlainT, bool>)
        {
            if (opts.show_bool_as_text)
            {
                wss << std::boolalpha;
            }
        }

        if constexpr (std::is_pointer_v<PlainT>)
        {
            if (opts.use_hex_for_pointers)
            {
                wss << std::hex << std::showbase;
            }
        }

        wss << std::forward<T>(value);
        return wss.str();
    }
    else
    {
        // Fallback: Manual ASCII widening
        std::wstring result;
        const char* str = opts.placeholder;
        while (*str)
        {
            result.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*str)));
            ++str;
        }
        return result;
    }
}

// =============================================================================
// Advanced Conversions
// =============================================================================

/**
 * @brief Converts value to string with alignment and padding.
 */
template <typename T>
[[nodiscard]] inline std::string toStringPadded(T&& value, std::size_t width, char align = '>', char padChar = ' ')
{
    auto str = toString(std::forward<T>(value));
    if (str.length() >= width)
    {
        return str;
    }

    std::size_t padding = width - str.length();

    switch (align)
    {
        case '<':
            str.append(padding, padChar);
            break;
        case '^':
            str.insert(0, padding / 2, padChar);
            str.append(padding - padding / 2, padChar);
            break;
        case '>':
        default:
            str.insert(0, padding, padChar);
            break;
    }

    return str;
}

/**
 * @brief Converts a numeric value with specified precision.
 */
template <typename T>
    requires(std::integral<T> || std::floating_point<T>)
[[nodiscard]] inline std::string toStringFormatted(T value, int precision = 6, bool fixed = true)
{
    StringifyOptions opts;
    opts.float_precision = precision;
    opts.scientific_notation = !fixed;
    return toString(value, opts);
}

/**
 * @brief Converts a pointer to string.
 */
template <typename T>
[[nodiscard]] inline std::string
toStringPointer(T* ptr, const StringifyOptions& opts, const char* nullPlaceholder)
{
    if (ptr == nullptr)
    {
        return nullPlaceholder;
    }

    std::ostringstream ss;
    auto addr = reinterpret_cast<std::uintptr_t>(ptr);

    if (opts.use_hex_for_pointers)
    {
        ss << "0x" << std::hex << addr;
    }
    else
    {
        ss << addr;
    }

    return ss.str();
}

/**
 * @brief Concatenates multiple values into a single string.
 */
template <typename... Args>
[[nodiscard]] inline std::string toStringConcat(Args&&... args)
{
    std::string result;
    result.reserve(sizeof...(args) * 16);
    ((result += toString(std::forward<Args>(args))), ...);
    return result;
}

} // namespace fat_p
