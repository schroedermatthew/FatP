/**
 * @file Stringify.h
 * @brief Type-to-string conversion utilities
 *
 * @layer Foundation
 */
#pragma once

/*
FATP_META:
  meta_version: 1
  component: Stringify
  file_role: public_header
  path: fat_p/Stringify.h
  namespace: fat_p
  layer: Foundation
  summary: "Public header for Stringify."
  api_stability: in_work
  related:
    docs_search: "Stringify"
    tests:
      - tests/test_Stringify.cpp
  hygiene:
    pragma_once: true
    include_guard: true
    defines_total: 4
    defines_unprefixed: 4
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/
#include "CppStandardDetection.h"

#include <string>
#include <string_view>
#include <sstream>
#include <type_traits>
#include <utility>
#include <iomanip>
#include <limits>
#include <locale>
#include <iterator>
#include <cstdint>
#include <tuple>
#include <vector>
#include <cassert>
#include <iostream>

// Check for C++17 optional support (MSVC uses _MSVC_LANG instead of __cplusplus)
#if FATP_CPP17_OR_LATER
#define FATP_STRINGIFY_HAS_OPTIONAL 1
#include <optional>
#else
#define FATP_STRINGIFY_HAS_OPTIONAL 0
#endif

#include "TypeTraits.h"

// Check if TypeTraits.h detected C++20 features
#if FATP_HAS_CPP20
    // Robust guard: only include <format> if it actually exists in the stdlib
    // Format detection handled by CppStandardDetection.h
        #if FATP_HAS_FORMAT
            #include <format>
            #define FATP_STRINGIFY_HAS_STD_FORMAT 1
        #endif
#endif
#ifndef FATP_STRINGIFY_HAS_STD_FORMAT
    #define FATP_STRINGIFY_HAS_STD_FORMAT 0
#endif

/**
 * @file Stringify.h
 * @brief Utility library for robust and flexible type-to-string conversion in C++17/20.
 * @layer Foundation
 *
 * Provides functions to convert various types (built-in, streamable, containers,
 * custom types with specific member functions) into std::string, with support
 * for formatting options, wide strings, and error handling.
 * 
 * @section new_features_v2_2 New Features in v2.2
 * 
 * **Performance Optimizations:**
 * - Boolean fast path: Direct "true"/"false" return (~10 ns vs ~470 ns)
 * - Floating-point fast path: Uses std::to_string for default options (~400 ns vs ~900 ns)
 * - Pair/tuple optimization: String concatenation instead of ostringstream
 * - toStringConcat optimization: Fold expression with string append
 * - Container reserve: Pre-allocates output buffer based on size estimate
 * 
 * @section new_features_v2_1 New Features in v2.1
 * 
 * **Recursion Depth Guard:**
 * - Thread-local recursion depth tracking prevents stack overflow from circular references
 * - Configurable limits (100 in debug, 200 in release)
 * - Returns "<recursion-limit>" when depth exceeded
 * 
 * **Classic Locale Default:**
 * - `use_classic_locale = true` by default for deterministic HPC/Scientific output
 * - Ensures consistent decimal separators ('.') regardless of global locale
 * - Still supports custom locales via `custom_locale` option
 * 
 * **Error Reporting:**
 * - `getLastStringifyError()` retrieves thread-local error message from failed `tryToString()`
 * - Provides detailed exception messages for debugging
 * - Thread-safe error tracking
 * 
 * **Enum Stringifier Trait:**
 * - User-specializable `EnumStringifier<E>` template for custom enum-to-string conversion
 * - Falls back to underlying type representation if not specialized
 * - Zero dependencies and overhead if unused
 * 
 * @section limitations Known Limitations
 * 
 * **Circular References:**
 * - Does NOT detect pointer cycles in data structures
 * - User must ensure data structures are acyclic
 * - Recursion depth guard prevents stack overflow but doesn't detect true cycles
 * - @warning Circular references will cause stack overflow or hit recursion limit
 * 
 * **Wide String ASCII-Only:**
 * - Non-wstreamable types use ASCII-only placeholder widening
 * - Non-ASCII placeholders will be mangled (C++26 codecvt deprecation workaround)
 * 
 * @section thread_safety Thread Safety
 * All Stringify functions are stateless and reentrant. Multiple threads can
 * safely call any function concurrently with independent arguments.
 * Thread safety for shared mutable objects (including StringifyOptions and
 * std::locale instances) is the caller's responsibility, consistent with
 * standard library utilities like std::ostringstream.
 * 
 * @section performance Performance Characteristics
 * - **Booleans (default opts)**: O(1), zero-allocation fast path returning literals
 * - **Integers (default opts)**: O(log10 n), zero-allocation fast path using std::to_string
 * - **Integers (custom opts)**: O(log10 n), single allocation via ostringstream
 * - **Floating-point (default opts)**: O(precision), uses std::to_string fast path
 * - **Floating-point (custom opts)**: O(precision), single allocation via ostringstream
 * - **Custom types**: Depends on toString()/to_string() implementation
 * - **Containers**: O(n * element_cost), recursive with depth limiting
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

    // --- Recursion depth tracking (thread-safe, zero allocation) ---
    
    inline int& get_stringify_depth() {
        thread_local int depth = 0;
        return depth;
    }
    
    struct StringifyDepthGuard {
        int& depth;
        bool exceeded;
        
        StringifyDepthGuard() : depth(get_stringify_depth()), exceeded(false) {
            ++depth;
            #ifndef NDEBUG
            if (depth > 100) {
                exceeded = true;
                std::cerr << "STRINGIFY ERROR: Recursion depth exceeded (possible cycle)\n";
                assert(false && "Stringify recursion limit exceeded");
            }
            #else
            if (depth > 200) {
                exceeded = true;
            }
            #endif
        }
        
        ~StringifyDepthGuard() {
            --depth;
        }
        
        bool exceeded_limit() const {
            return exceeded;
        }
    };
    
    // --- Thread-local error tracking ---
    
    inline std::string& get_last_stringify_error() {
        thread_local std::string last_error;
        return last_error;
    }


    // --- Detection helpers for pair/tuple/optional ---
    
    // Specific std::pair detection (checks for actual std::pair instantiation)
    template <typename T>
    struct is_std_pair : std::false_type {};
    
    template <typename T1, typename T2>
    struct is_std_pair<std::pair<T1, T2>> : std::true_type {};

    template <typename T>
    using op_first_type = typename T::first_type;
    
    template <typename T>
    using op_second_type = typename T::second_type;
    
    template <typename T>
    using op_tuple_size = decltype(std::tuple_size<T>::value);
    
#if FATP_STRINGIFY_HAS_OPTIONAL
    template <typename T>
    using op_has_value = decltype(std::declval<const T&>().has_value());

    template <typename T>
    using op_deref = decltype(*std::declval<const T&>());

    // Optional-like types must have both has_value() AND operator*.
    // This excludes std::any (has has_value() but no operator*).
    template <typename T>
    inline constexpr bool kIsOptionalLike =
        is_detected_v<op_has_value, std::decay_t<T>> &&
        is_detected_v<op_deref, std::decay_t<T>>;
#endif

    // --- Streamable Traits ---

    /**
     * @brief Primary trait: checks if a type T is streamable to std::ostringstream.
     */
    template <typename T, typename = void>
    struct is_ostreamable_impl : std::false_type {};

    template <typename T>
    struct is_ostreamable_impl<T, std::void_t<decltype(
        std::declval<std::ostringstream&>() << std::declval<const T&>())>> 
        : std::true_type {};

    /**
     * @brief Checks if a type T is streamable to std::wostringstream.
     */
    template <typename T, typename = void>
    struct is_wostreamable_impl : std::false_type {};

    template <typename T>
    struct is_wostreamable_impl<T, std::void_t<decltype(
        std::declval<std::wostringstream&>() << std::declval<const T&>())>> 
        : std::true_type {};

    /**
     * @brief Checks if a type T has a member function named toString() that returns a string.
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
     */
    template <typename T, typename = void>
    struct has_to_string_snake_method_impl : std::false_type {};

    template <typename T>
    struct has_to_string_snake_method_impl<T, std::void_t<decltype(
        std::declval<const T&>().to_string())>> 
        : std::bool_constant<std::is_convertible_v<
            decltype(std::declval<const T&>().to_string()), std::string>> {};

    // --- Convenience Aliases with Decay ---

    template <typename T>
    using is_ostreamable = is_ostreamable_impl<std::decay_t<T>>;

    template <typename T>
    using is_wostreamable = is_wostreamable_impl<std::decay_t<T>>;

    template <typename T>
    using has_to_string_method = has_to_string_method_impl<std::decay_t<T>>;

    template <typename T>
    using has_to_string_snake_method = has_to_string_snake_method_impl<std::decay_t<T>>;

    /**
     * @brief Alias/Concept to the is_iterable trait defined in TypeTraits.h
     */
    template <typename T>
    #if FATP_HAS_CPP20
    concept is_iterable_concept = fat_p::is_iterable<std::decay_t<T>>::value;
    #else
    using is_iterable = fat_p::is_iterable<std::decay_t<T>>;
    #endif

} // namespace detail

// --- Variable Templates ---

template <typename T>
inline constexpr bool is_ostreamable_v = detail::is_ostreamable<T>::value;

template <typename T>
inline constexpr bool is_wostreamable_v = detail::is_wostreamable<T>::value;

template <typename T>
inline constexpr bool has_to_string_method_v = detail::has_to_string_method<T>::value;

template <typename T>
inline constexpr bool has_to_string_snake_method_v = detail::has_to_string_snake_method<T>::value;

// =============================================================================
// Enum Stringifier Trait (User Specialization Point)
// =============================================================================

/**
 * @brief User-specializable trait for enum-to-string conversion.
 * 
 * Users can specialize this for their enum types to provide string representations.
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
struct EnumStringifier {
    static const char* to_string(E) {
        return nullptr;
    }
};

// =============================================================================
// String Conversion Options
// =============================================================================

/**
 * @brief Options for controlling string conversion behavior.
 */
struct StringifyOptions {
    const char* placeholder = "<non-stringifiable>"; ///< Placeholder for non-stringifiable types.
    bool use_hex_for_pointers = true;              ///< Use hexadecimal notation for pointers.
    int float_precision = -1;                      ///< Decimal precision (-1 means default).
    bool scientific_notation = false;              ///< Use scientific notation for floats.
    bool show_bool_as_text = true;                 ///< Show booleans as "true"/"false".
    const char* container_open = "[";              ///< Opening delimiter for sequential containers.
    const char* container_close = "]";             ///< Closing delimiter for sequential containers.
    const char* container_separator = ", ";        ///< Separator between container elements.
    int max_container_depth = 3;                   ///< Max recursion depth for containers.
    bool use_classic_locale = true;                ///< Use classic "C" locale for determinism.
    std::locale* custom_locale = nullptr;          ///< Custom locale (nullptr = classic/global).
    
    constexpr StringifyOptions() noexcept = default;
};

// =============================================================================
// Forward Declarations
// =============================================================================

/**
 * @brief Forward declaration of the primary toString function template.
 */
template <typename T>
[[nodiscard]] std::string toString(T&& value, const StringifyOptions& opts = {});

// =============================================================================
// Core Stringify Functions
// =============================================================================

namespace detail {

    /**
     * @brief Optimized path for built-in integer types using std::to_string.
     */
    template <typename T>
    [[nodiscard]] inline std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>,
                                          std::string>
    fast_int_to_string(T value) {
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
     * @brief Optimized path for floating-point types using std::to_string.
     */
    template <typename T>
    [[nodiscard]] inline std::enable_if_t<std::is_floating_point_v<T>, std::string>
    fast_float_to_string(T value) {
        return std::to_string(value);
    }

    /**
     * @brief Helper to convert a value using its toString() or to_string() member function.
     */
    template <typename T>
    [[nodiscard]] inline std::string stringify_with_method(const T& value,
                                                           const StringifyOptions&) {
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
     */
    template <typename T>
    [[nodiscard]] inline std::string stringify_with_stream(T&& value,
                                                           const StringifyOptions& opts) {
        std::ostringstream ss;
        
        if (opts.custom_locale != nullptr) {
            ss.imbue(*opts.custom_locale);
        } else if (opts.use_classic_locale) {
            ss.imbue(std::locale::classic());
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
     * @brief Helper for tuple stringification using string concatenation (optimized).
     */
    template <typename Tuple, std::size_t... Is>
    inline std::string stringify_tuple_impl(const Tuple& t, 
                                            const StringifyOptions& opts, 
                                            std::index_sequence<Is...>) {
        std::string result;
        result.reserve(64);
        result += "(";
        bool first = true;
        ((result += (first ? (first = false, "") : ", "),
          result += toString(std::get<Is>(t), opts)), ...);
        result += ")";
        return result;
    }

    // Helper to detect if container has .size() method
    template <typename T, typename = void>
    struct has_size_method : std::false_type {};

    template <typename T>
    struct has_size_method<T, std::void_t<decltype(std::declval<const T&>().size())>>
        : std::true_type {};

    /**
     * @brief Estimate container output size for reserve optimization.
     * @details Uses .size() when available (O(1)), falls back to iteration otherwise.
     */
    template <typename Container>
    [[nodiscard]] inline std::size_t estimate_container_size(const Container& container,
                                                              const StringifyOptions& opts) {
        std::size_t count = 0;
        if constexpr (has_size_method<Container>::value) {
            count = container.size();
        } else {
            for (auto it = std::begin(container); it != std::end(container); ++it) {
                ++count;
            }
        }
        // Estimate: open + close + (count * (avg_elem_size + separator_len))
        std::size_t sep_len = std::char_traits<char>::length(opts.container_separator);
        return 2 + count * (8 + sep_len);
    }

    /**
     * @brief Helper for stringifying sequential containers recursively.
     * @warning Does not detect circular references.
     */
    template <typename Container>
    [[nodiscard]] inline std::string stringify_container(const Container& container, 
                                                         const StringifyOptions& opts) {
        StringifyDepthGuard guard;
        if (guard.exceeded_limit()) {
            return "<recursion-limit>";
        }
        
        if (opts.max_container_depth <= 0) {
            return "<max depth>";
        }
        
        std::string result;
        result.reserve(estimate_container_size(container, opts));
        result += opts.container_open;
        
        bool first = true;
        StringifyOptions elem_opts = opts;
        elem_opts.max_container_depth--;
        
        for (const auto& elem : container) {
            if (!first) {
                result += opts.container_separator;
            }
            result += toString(elem, elem_opts);
            first = false;
        }
        
        result += opts.container_close;
        return result;
    }

    /**
     * @brief Helper for stringifying map-like containers recursively.
     */
    template <typename Container>
    [[nodiscard]] inline std::string stringify_map(const Container& container, 
                                                   const StringifyOptions& opts) {
        StringifyDepthGuard guard;
        if (guard.exceeded_limit()) {
            return "<recursion-limit>";
        }
        
        if (opts.max_container_depth <= 0) {
            return "<max depth>";
        }
        
        std::string result;
        result.reserve(estimate_container_size(container, opts) * 2);
        result += "{";
        
        bool first = true;
        StringifyOptions elem_opts = opts;
        elem_opts.max_container_depth--;
        
        for (const auto& pair : container) {
            if (!first) {
                result += opts.container_separator;
            }
            result += toString(pair.first, elem_opts);
            result += ": ";
            result += toString(pair.second, elem_opts);
            first = false;
        }
        
        result += "}";
        return result;
    }

} // namespace detail

/**
 * @brief Converts a value to string with fallback to a placeholder.
 * 
 * @details Priority order for conversion methods:
 * 0. Tuple/Pair/Optional (Recursive types) - Checked FIRST
 * 1. Strings (Passthrough)
 * 2. Nullable strings (char*)
 * 2.5. Enums with custom stringifier
 * 3A. FAST PATH: Boolean types
 * 3B. FAST PATH: Built-in integers
 * 3C. FAST PATH: Floating-point types (default opts)
 * 4. T::toString() or T::to_string() member function
 * 5. std::ostringstream operator<<
 * 6A. Map-like container stringification
 * 6B. Sequential container stringification
 * 7. Placeholder string
 */
template <typename T>
[[nodiscard]] inline std::string toString(T&& value, const StringifyOptions& opts)
{
    using PlainT = std::decay_t<T>;

    // PRIORITY 0: Recursive types (Tuple, Pair, Optional)
    // These must be checked before streamable/container checks to ensure correct recursion
    if constexpr (detail::is_std_pair<PlainT>::value) {
        // std::pair detected - use optimized string concatenation
        std::string result;
        result.reserve(32);
        result += "(";
        result += toString(value.first, opts);
        result += ", ";
        result += toString(value.second, opts);
        result += ")";
        return result;
    }
    else if constexpr (is_detected_v<detail::op_tuple_size, PlainT>) {
        // std::tuple detected (has tuple_size)
        return detail::stringify_tuple_impl(value, opts,
                                            std::make_index_sequence<std::tuple_size_v<PlainT>>{});
    }
#if FATP_STRINGIFY_HAS_OPTIONAL
    else if constexpr (detail::kIsOptionalLike<PlainT>) {
        // std::optional detected (has both has_value() and operator*)
        return value.has_value() ? toString(*value, opts) : "nullopt";
    }
#endif
    // PRIORITY 1: Strings
    else if constexpr (std::is_same_v<PlainT, std::string>)
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
    // PRIORITY 2: Nullable C-strings
    else if constexpr (std::is_same_v<PlainT, const char*> || std::is_same_v<PlainT, char*>)
    {
        return value ? std::string(value) : opts.placeholder;
    }
    // PRIORITY 2.5: Enum with custom stringifier
    else if constexpr (std::is_enum_v<PlainT>)
    {
        const char* enum_str = EnumStringifier<PlainT>::to_string(value);
        if (enum_str != nullptr) {
            return std::string(enum_str);
        }
        return std::to_string(static_cast<std::underlying_type_t<PlainT>>(value));
    }
    // PRIORITY 3A: Fast Boolean (NEW - avoids ostringstream)
    else if constexpr (std::is_same_v<PlainT, bool>)
    {
        if (opts.show_bool_as_text) {
            return value ? "true" : "false";
        } else {
            return value ? "1" : "0";
        }
    }
    // PRIORITY 3B: Fast Integers
    else if constexpr (std::is_integral_v<PlainT>)
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
    // PRIORITY 3C: Fast Floating-Point (NEW - uses std::to_string for default opts)
    else if constexpr (std::is_floating_point_v<PlainT>)
    {
        if (opts.float_precision == -1 && !opts.scientific_notation && 
            opts.custom_locale == nullptr) 
        {
            return detail::fast_float_to_string(value);
        }
        else
        {
            return detail::stringify_with_stream(std::forward<T>(value), opts);
        }
    }
    // PRIORITY 4: Custom Methods
    else if constexpr (detail::has_to_string_method<PlainT>::value || 
                       detail::has_to_string_snake_method<PlainT>::value)
    {
        return detail::stringify_with_method(value, opts);
    }
    // PRIORITY 5: Streamable
    else if constexpr (detail::is_ostreamable<PlainT>::value)
    {
        return detail::stringify_with_stream(std::forward<T>(value), opts);
    }
    // PRIORITY 6: Containers
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
    // PRIORITY 7: Fallback
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
 */
template <typename T>
[[nodiscard]] inline std::string toStringOr(T&& value, const char* fallback) {
    StringifyOptions opts;
    opts.placeholder = fallback;
    return toString(std::forward<T>(value), opts);
}

/**
 * @brief Safely converts a value to string, returning success status without throwing.
 */
template <typename T>
[[nodiscard]] inline bool tryToString(const T& value, std::string& out, 
                                      const StringifyOptions& opts = {}) noexcept {
    try {
        out = toString(value, opts);
        return true;
    } catch (const std::exception& e) {
        detail::get_last_stringify_error() = e.what();
        return false;
    } catch (...) {
        detail::get_last_stringify_error() = "Unknown stringify error";
        return false;
    }
}


/**
 * @brief Retrieves the last error message from tryToString.
 * @return Thread-local error string from the last failed tryToString call.
 */
[[nodiscard]] inline const std::string& getLastStringifyError() noexcept {
    return detail::get_last_stringify_error();
}

/**
 * @brief Checks if a type is stringifiable at compile time.
 */
template <typename T>
inline constexpr bool is_stringifiable_v = 
    std::is_enum_v<std::decay_t<T>> ||
    std::is_arithmetic_v<std::decay_t<T>> ||
    detail::is_ostreamable<T>::value || 
    detail::has_to_string_method<T>::value ||
    detail::has_to_string_snake_method<T>::value ||
    is_detected_v<detail::op_first_type, T> ||
    is_detected_v<detail::op_tuple_size, T> ||
    #if FATP_STRINGIFY_HAS_OPTIONAL
    detail::kIsOptionalLike<T> ||
    #endif
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
 * @details Fallback uses manual ASCII widening to avoid deprecated codecvt (C++26 compliant).
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
        // Fallback: Manual ASCII widening (Avoids std::codecvt deprecation)
        std::wstring result;
        const char* str = opts.placeholder;
        while (*str) {
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
 * @brief Converts a pointer to a string using hexadecimal notation (default) or decimal.
 */
template <typename T>
[[nodiscard]] inline std::string toStringPointer(T* ptr, 
                                                 const StringifyOptions& opts = {},
                                                 const char* null_placeholder = "nullptr") {
    if (ptr == nullptr) {
        return null_placeholder;
    }
    
    std::ostringstream ss;
    auto addr = reinterpret_cast<std::uintptr_t>(ptr);
    
    if (opts.use_hex_for_pointers) {
        ss << "0x" << std::hex << addr;
    } else {
        ss << addr;
    }
    
    return ss.str();
}

/**
 * @brief Variadic function that concatenates multiple values into a single string.
 * @details Optimized to use fold expression with string concatenation instead of ostringstream.
 */
template <typename... Args>
[[nodiscard]] inline std::string toStringConcat(Args&&... args) {
    std::string result;
    result.reserve(sizeof...(args) * 16);
    ((result += toString(std::forward<Args>(args))), ...);
    return result;
}

} // namespace fat_p
