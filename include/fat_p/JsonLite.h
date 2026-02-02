#pragma once

/*
FATP_META:
  meta_version: 1
  component: JsonLite
  file_role: public_header
  path: include/fat_p/JsonLite.h
  namespace: fat_p
  layer: Domain
  summary: "Public header for JsonLite."
  api_stability: in_work
  related:
    docs_search: "JsonLite"
    tests:
      - components/DiagnosticLogger/tests/test_DiagnosticLogger_Json.cpp
      - components/Json/tests/test_JsonLite.cpp
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 64
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/

/**
 * @file JsonLite.h
 * @brief Lightweight JSON library for C++ configuration and parameter management
 *
 *
 *
 * @section overview Overview
 * JsonLite is a modern C++20 header-only JSON library designed specifically for
 * application configuration files, parameter persistence, and structured data
 * serialization where simplicity and minimal external dependencies are priorities.
 *
 * This is the STANDALONE version with ZERO external dependencies.
 * For fat_p component integration, use FatPJson.h instead.
 *
 * @section features Features
 * - C++20 header-only library (single file, no build configuration)
 * - Zero external dependencies
 * - Policy-based design for compile-time customization
 * - Type-safe variant-based JSON value representation
 * - Macro-based automatic struct serialization
 * - Comprehensive error messages with position information
 * - Support for std::optional, std::vector, std::map, std::tuple, std::pair
 * - Integer precision preservation using int64_t
 * - Safe numeric conversions with overflow checking
 * - Enhanced error reporting with source location tracking
 * - Both reference and value-returning conversion APIs
 *
 * @section example Basic Example
 * @code{.cpp}
 * #include "JsonLite.h"
 *
 * struct Config {
 *     int port;
 *     std::string host;
 *     std::optional<int> timeout;
 * };
 * FATP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Config, port, host, timeout)
 *
 * int main() {
 *     Config cfg{8080, "localhost", 30};
 *     fat_p::save_params("config.json", cfg);
 *
 *     auto loaded = fat_p::load_params<Config>("config.json");
 *     return 0;
 * }
 * @endcode
 *
 * @section conversion_api Two-Style Conversion API
 * @code{.cpp}
 * JsonObject obj;
 * obj["port"] = 8080;
 * obj["host"] = "localhost";
 *
 * // Reference-based API (efficient for large objects)
 * int port;
 * from_json(obj["port"], port);
 *
 * // Value-returning API (convenient, enables const)
 * const int port = from_json<int>(obj["port"]);
 * const auto host = from_json<std::string>(obj["host"]);
 * @endcode
 *
 */

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib> // std::abort (used in unreachable_after_enforce)
#include <deque>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <list>
#include <locale>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <typeinfo> // typeid (used in checked_cast error messages)
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

/**
 * @namespace fat_p
 * @brief Main namespace for the fat_p library components
 *
 * @details This namespace contains all fat_p library utilities including JsonLite.
 * The library follows modern C++17 design patterns with emphasis on type safety,
 * compile-time configuration, and zero-cost abstractions.
 */
namespace fat_p
{

/**
 * @namespace fat_p::json_detail
 * @brief Internal implementation details for JsonLite
 *
 * @details This namespace contains implementation helpers, type traits, and utility
 * functions used internally by JsonLite. Users should not directly depend on anything
 * in this namespace as it is subject to change without notice.
 *
 * @warning This is an internal namespace. Do not use directly in client code.
 */
namespace json_detail
{
/**
 * @struct SourceLocation
 * @brief Lightweight structure for capturing source code location information
 *
 * @details Stores file name, line number, and function name for error reporting.
 * This is a simple alternative to std::source_location (C++20) that works in C++17.
 * Used by FATP_JSON_ENFORCE macro to provide detailed error messages with context.
 *
 * @see FATP_JSON_ENFORCE
 * @see FATP_JSON_LOCUS
 */
struct SourceLocation
{
    const char* file;     ///< Source file name from __FILE__
    int line;             ///< Source line number from __LINE__
    const char* function; ///< Function name from __func__
};

/**
 * @def FATP_JSON_LOCUS
 * @brief Macro to capture current source location
 *
 * @details Expands to a SourceLocation structure initialized with the current
 * file, line, and function. Used internally by FATP_JSON_ENFORCE for error reporting.
 *
 * @code{.cpp}
 * SourceLocation loc = FATP_JSON_LOCUS;
 * std::cout << "Error at " << loc.file << ":" << loc.line << std::endl;
 * @endcode
 *
 * @see SourceLocation
 * @see FATP_JSON_ENFORCE
 */
#define FATP_JSON_LOCUS                  \
    ::fat_p::json_detail::SourceLocation \
    {                                    \
        __FILE__, __LINE__, __func__     \
    }

/**
 * @brief Appends a single value to an output string stream
 *
 * @details Base case for the variadic append_to_stream template. Simply streams
 * the value to the output string stream using operator<<.
 *
 * @tparam T Type of value to append (must support operator<<)
 * @param oss Output string stream to append to
 * @param value Value to append
 *
 * @see append_to_stream(std::ostringstream&, const T&, const Args&...)
 */
template <typename T>
inline void append_to_stream(std::ostringstream& oss, const T& value)
{
    oss << value;
}

/**
 * @brief Variadic template to append multiple values to string stream
 *
 * @details Recursively appends values to the output string stream, separated by
 * spaces. This is used internally by FATP_JSON_ENFORCE to build error messages from
 * multiple arguments.
 *
 * @code{.cpp}
 * std::ostringstream oss;
 * append_to_stream(oss, "Error:", 42, "value", 3.14);
 * // Result: "Error: 42 value 3.14 "
 * @endcode
 *
 * @tparam T Type of first value
 * @tparam Args Types of remaining values
 * @param oss Output string stream to append to
 * @param first First value to append
 * @param rest Remaining values to append
 *
 * @see FATP_JSON_ENFORCE_impl
 */
template <typename T, typename... Args>
inline void append_to_stream(std::ostringstream& oss, const T& first, const Args&... rest)
{
    oss << first << " ";
    append_to_stream(oss, rest...);
}

/**
 * @brief Base case for variadic append_to_stream (no arguments)
 *
 * @details Terminal case of the recursive template that does nothing.
 * This is called when all arguments have been processed.
 *
 * @param oss Output string stream (unused in base case)
 */
inline void append_to_stream(std::ostringstream&)
{
}

/**
 * @brief Internal implementation of FATP_JSON_ENFORCE macro
 *
 * @details This function is called by the FATP_JSON_ENFORCE macro when a condition fails.
 * It builds a comprehensive error message including source location and any additional
 * context provided via variadic arguments, then throws std::runtime_error.
 *
 * The function is marked [[noreturn]] because it always throws or terminates.
 *
 * @tparam Args Types of additional context arguments
 * @param condition The condition that was checked (should be false when called)
 * @param loc Source location where the check failed
 * @param args Additional context to include in error message (name-value pairs)
 *
 * @throws std::runtime_error Always throws with detailed error message
 *
 * @warning This function should not be called directly; use the FATP_JSON_ENFORCE macro instead
 *
 * @see FATP_JSON_ENFORCE
 * @see SourceLocation
 */
template <typename... Args>
[[noreturn]] inline void FATP_JSON_ENFORCE_impl(bool condition, SourceLocation loc, Args&&... args)
{
    if (!condition)
    {
        std::ostringstream oss;
        oss << "JSON Error at " << loc.file << ":" << loc.line << " in " << loc.function;

        if constexpr (sizeof...(args) > 0)
        {
            oss << " - ";
            append_to_stream(oss, std::forward<Args>(args)...);
        }

        throw std::runtime_error(oss.str());
    }
    std::terminate();
}
} // namespace json_detail

/**
 * @brief Contract enforcement macro with source location tracking.
 *
 * Throws std::runtime_error with file, line, and function context when
 * the condition fails. Supports optional name-value pairs for diagnostics.
 *
 * @code{.cpp}
 * // Basic usage
 * FATP_JSON_ENFORCE(value >= 0, "Value must be non-negative");
 *
 * // With context (name-value pairs)
 * FATP_JSON_ENFORCE(port >= 1024,
 *     "Invalid port number",
 *     "port", port,
 *     "min", 1024);
 * @endcode
 *
 * @param condition Boolean expression to check (must evaluate to true)
 * @param ... Optional name-value pairs for error context
 *
 * @throws std::runtime_error when condition is false
 */
// Implementation notes:
// - do-while(0) wrapper ensures safe use in all control flow contexts (if/else, etc.)
// - Condition is stringified (#condition) and included in error message
// - FATP_JSON_LOCUS captures __FILE__, __LINE__, __func__ at the call site
// - ##__VA_ARGS__ handles the zero-args case (GCC/Clang extension, C++20 standard)
// - No ODR issues: macro expands inline, FATP_JSON_ENFORCE_impl is in detail namespace
#define FATP_JSON_ENFORCE(condition, ...)                                 \
    do                                                                    \
    {                                                                     \
        if (!(condition))                                                 \
        {                                                                 \
            ::fat_p::json_detail::FATP_JSON_ENFORCE_impl(false,           \
                                                         FATP_JSON_LOCUS, \
                                                         "condition: ",   \
                                                         #condition,      \
                                                         ##__VA_ARGS__);  \
        }                                                                 \
    } while (0)

// ============================================================================
// json_lite namespace - Standalone utilities for JsonLite
// ============================================================================
//
// JsonLite is designed to be usable without any other Fat-P components.
// These utilities live in fat_p::json_lite to avoid collision with the
// full Fat-P versions (e.g., checked_cast in CheckedArithmetic.h).
//
// Use fat_p::jl:: as a short alias.
//

namespace json_lite
{

/**
 * @brief Safe numeric type conversion with overflow and range checking.
 *
 * Converts between integral types, throwing if the value cannot be represented
 * in the target type. Zero overhead when source and target types are identical.
 *
 * @code{.cpp}
 * int64_t big = 1000;
 * int small = json_lite::checked_cast<int>(big);  // OK
 *
 * int64_t too_big = INT_MAX + 1LL;
 * int fail = json_lite::checked_cast<int>(too_big);  // Throws!
 *
 * int negative = -1;
 * unsigned int ufail = json_lite::checked_cast<unsigned int>(negative);  // Throws!
 * @endcode
 *
 * @tparam To   Target integral type
 * @tparam From Source integral type (deduced)
 * @param value The value to convert
 * @return The converted value
 *
 * @throws std::runtime_error if value is outside target type's representable range
 *
 * @see from_json
 */
template <typename To, typename From>
inline To checked_cast(From value)
{
    // Case 1: Same type - no conversion needed, zero overhead
    if constexpr (std::is_same_v<To, From>)
    {
        return value;
    }
    // Case 2: Signed -> Signed
    // Risk: Value could exceed target's min OR max
    // Example: int64_t -> int8_t where value = 1000
    else if constexpr (std::is_signed_v<From> && std::is_signed_v<To>)
    {
        if (value < static_cast<From>(std::numeric_limits<To>::min()) ||
            value > static_cast<From>(std::numeric_limits<To>::max()))
        {
            FATP_JSON_ENFORCE(false,
                              "Numeric cast out of range: value=",
                              value,
                              " target_type=",
                              typeid(To).name(),
                              " min=",
                              std::numeric_limits<To>::min(),
                              " max=",
                              std::numeric_limits<To>::max());
        }
    }
    // Case 3: Unsigned -> Signed
    // Risk: Large unsigned value exceeds signed max (no min check needed - unsigned >= 0)
    // Example: uint64_t -> int32_t where value = 3 billion
    // Note: Cast To's max to unsigned to avoid signed/unsigned comparison warnings
    else if constexpr (!std::is_signed_v<From> && std::is_signed_v<To>)
    {
        if (value > static_cast<typename std::make_unsigned<To>::type>(std::numeric_limits<To>::max()))
        {
            FATP_JSON_ENFORCE(false,
                              "Numeric cast out of range: value=",
                              value,
                              " target_type=",
                              typeid(To).name(),
                              " max=",
                              std::numeric_limits<To>::max());
        }
    }
    // Case 4: Signed -> Unsigned
    // Risk 1: Negative values have no unsigned representation
    // Risk 2: Even positive values might exceed a smaller unsigned max
    // Example: int64_t -> uint8_t where value = -5 or value = 300
    else if constexpr (std::is_signed_v<From> && !std::is_signed_v<To>)
    {
        // First check: reject negative values outright
        if (value < 0)
        {
            FATP_JSON_ENFORCE(false,
                              "Numeric cast: negative value for unsigned type: value=",
                              value,
                              " target_type=",
                              typeid(To).name());
        }
        // Second check: verify positive value fits in target range
        // Cast to unsigned for safe comparison (value is known non-negative here)
        if (static_cast<typename std::make_unsigned<From>::type>(value) > std::numeric_limits<To>::max())
        {
            FATP_JSON_ENFORCE(false,
                              "Numeric cast out of range: value=",
                              value,
                              " target_type=",
                              typeid(To).name(),
                              " max=",
                              std::numeric_limits<To>::max());
        }
    }
    // Case 5: Unsigned -> Unsigned
    // Risk: Only overflow above max (no sign issues, no underflow possible)
    // Example: uint64_t -> uint8_t where value = 1000
    else
    {
        if (value > std::numeric_limits<To>::max())
        {
            FATP_JSON_ENFORCE(false,
                              "Numeric cast out of range: value=",
                              value,
                              " target_type=",
                              typeid(To).name(),
                              " max=",
                              std::numeric_limits<To>::max());
        }
    }

    // All checks passed - perform the actual cast
    return static_cast<To>(value);
}

} // namespace json_lite

// Short namespace alias for convenience
namespace jl = json_lite;

/**
 * @namespace fat_p::json_detail
 * @brief Extended implementation details for JsonLite internal use
 */
namespace json_detail
{
/**
 * @var double_epsilon
 * @brief Tolerance for floating-point fractional part detection
 *
 * @details Used when converting doubles to integers to determine if a fractional
 * part exists. Value: 100 * machine epsilon.
 *
 * @see from_json(const JsonValue&, int&)
 */
inline constexpr double double_epsilon = std::numeric_limits<double>::epsilon() * 100.0;

/*
 * NOTE: Numeric margin constants were removed (formerly signed_64bit_margin = 2048,
 * unsigned_64bit_margin = 4096, signed_32bit_margin = 0).
 *
 * Rationale: These margins artificially restricted the valid conversion range without
 * mathematical justification. Direct bounds checking against std::numeric_limits is
 * sufficient and allows all valid conversions within double's precision limits.
 *
 * For integers beyond 2^53, double precision loss is a fundamental IEEE 754 limitation
 * that cannot be solved with arbitrary margins at 2^63.
 */

/**
 * @brief Convert double to integral type with validation
 *
 * @tparam IntType Target integral type
 * @param d Double value to convert
 * @param result Reference to store the converted value
 * @param typeName Type name for error messages
 *
 * @details Validates that:
 * - Value has no fractional part (within epsilon tolerance)
 * - For unsigned types, value is non-negative
 * - Value is within the target type's range
 *
 * @throws std::runtime_error if validation fails
 */
template <typename IntType>
inline void convert_double_to_int(double d, IntType& result, const char* typeName)
{
    static_assert(std::is_integral_v<IntType>, "IntType must be an integral type");

    double intpart;
    FATP_JSON_ENFORCE(fabs(std::modf(d, &intpart)) <= double_epsilon,
                      "JSON conversion error: fractional part detected",
                      "value",
                      d,
                      "target_type",
                      typeName);

    if constexpr (std::is_unsigned_v<IntType>)
    {
        FATP_JSON_ENFORCE(d >= 0,
                          "JSON conversion error: negative value for unsigned type",
                          "value",
                          d,
                          "target_type",
                          typeName);
    }

    if constexpr (std::is_signed_v<IntType> && sizeof(IntType) >= 8)
    {
        constexpr double type_min = static_cast<double>(std::numeric_limits<IntType>::min());
        constexpr double type_max = static_cast<double>(std::numeric_limits<IntType>::max());

        if (intpart < type_min || intpart > type_max)
        {
            FATP_JSON_ENFORCE(false,
                              "JSON conversion error: value out of range for 64-bit signed integer",
                              "value",
                              d,
                              "target_type",
                              typeName);
        }

        IntType candidate = static_cast<IntType>(intpart);

        if (intpart < 0.0 && candidate > 0)
        {
            FATP_JSON_ENFORCE(false,
                              "JSON conversion error: underflow detected",
                              "value",
                              d,
                              "converted",
                              candidate,
                              "target_type",
                              typeName);
        }

        if (intpart > 0.0 && candidate < 0)
        {
            FATP_JSON_ENFORCE(false,
                              "JSON conversion error: overflow detected",
                              "value",
                              d,
                              "converted",
                              candidate,
                              "target_type",
                              typeName);
        }

        result = candidate;
    }
    else
    {
        constexpr double type_min = static_cast<double>(std::numeric_limits<IntType>::min());
        constexpr double type_max = static_cast<double>(std::numeric_limits<IntType>::max());

        FATP_JSON_ENFORCE(intpart >= type_min && intpart <= type_max,
                          "JSON conversion error: value out of range",
                          "value",
                          d,
                          "target_type",
                          typeName);

        IntType candidate = static_cast<IntType>(intpart);

        if constexpr (sizeof(IntType) >= 8)
        {
            double roundtrip = static_cast<double>(candidate);
            FATP_JSON_ENFORCE(roundtrip == intpart,
                              "JSON conversion error: precision loss or overflow detected",
                              "value",
                              d,
                              "converted",
                              candidate,
                              "target_type",
                              typeName);
        }

        result = candidate;
    }
}

/**
 * @brief Marks code path as unreachable after FATP_JSON_ENFORCE failure
 *
 * @details Calls std::abort(). Placed after FATP_JSON_ENFORCE in functions
 * that require a return value on all paths.
 *
 * @see FATP_JSON_ENFORCE
 */
[[noreturn]] inline void unreachable_after_enforce()
{
    std::abort();
}

/**
 * @struct IsIterable
 * @brief Type trait to detect if a type supports iteration
 *
 * @details Uses SFINAE to check if a type has begin() and end() members
 * (or free functions). This is used to distinguish between container types
 * and non-container types during JSON serialization.
 *
 * @tparam T Type to check for iterability
 *
 * @code{.cpp}
 * static_assert(IsIterable<std::vector<int>>::value);  // true
 * static_assert(IsIterable<std::map<int,int>>::value); // true
 * static_assert(!IsIterable<int>::value);              // false
 * @endcode
 *
 * @see to_json
 */
template <typename T, typename = void>
struct IsIterable : std::false_type
{
};

/**
 * @struct IsIterable
 * @brief Specialization for types with begin()/end()
 *
 * @details SFINAE-friendly specialization that inherits from std::true_type
 * when T supports std::begin() and std::end().
 *
 * @tparam T Type to check (must have begin/end)
 */
template <typename T>
struct IsIterable<T, std::void_t<decltype(std::begin(std::declval<T&>())), decltype(std::end(std::declval<T&>()))>>
    : std::true_type
{
};

/**
 * @struct HasMappedType
 * @brief Type trait to detect associative containers (maps)
 *
 * @details Uses SFINAE to check if a type has a nested mapped_type typedef,
 * which distinguishes maps from sequences. Used to serialize maps as JSON
 * objects versus arrays.
 *
 * @tparam T Type to check for mapped_type
 *
 * @code{.cpp}
 * static_assert(HasMappedType<std::map<int,int>>::value);        // true
 * static_assert(HasMappedType<std::unordered_map<int,int>>::value); // true
 * static_assert(!HasMappedType<std::vector<int>>::value);        // false
 * @endcode
 *
 * @see to_json
 */
template <typename T, typename = void>
struct HasMappedType : std::false_type
{
};

/**
 * @struct HasMappedType
 * @brief Specialization for types with mapped_type
 *
 * @details SFINAE-friendly specialization that inherits from std::true_type
 * when T::mapped_type exists.
 *
 * @tparam T Type to check (must have mapped_type)
 */
template <typename T>
struct HasMappedType<T, std::void_t<typename T::mapped_type>> : std::true_type
{
};

/**
 * @typedef ContainerValueT
 * @brief Extracts the value_type from a container
 *
 * @details Alias template that retrieves Container::value_type. Used during
 * generic container serialization to determine element type.
 *
 * @tparam Container Container type with value_type member
 *
 * @code{.cpp}
 * using IntVecValue = ContainerValueT<std::vector<int>>;  // int
 * using MapValue = ContainerValueT<std::map<int, std::string>>;  // std::pair<const int, std::string>
 * @endcode
 *
 * @see to_json
 * @see from_json
 */
template <typename Container>
using ContainerValueT = typename Container::value_type;
} // namespace json_detail

/**
 * @typedef JsonObject
 * @brief JSON object representation as string-to-JsonValue map
 *
 * @details Uses std::map with std::string keys and JsonValue values.
 * Iteration order is lexicographic by key.
 *
 * @code{.cpp}
 * JsonObject obj;
 * obj["name"] = "Alice";
 * obj["age"] = 30;
 * @endcode
 *
 * @see JsonValue
 * @see JsonArray
 */
using JsonObject = std::map<std::string, struct JsonValue>;

/**
 * @typedef JsonArray
 * @brief JSON array representation as vector of JsonValue
 *
 * @details Uses std::vector<JsonValue> to represent JSON arrays. Provides
 * efficient random access, contiguous storage, and automatic growth.
 *
 * @section usage Usage
 * @code{.cpp}
 * JsonArray arr;
 * arr.push_back(1);
 * arr.push_back("hello");
 * arr.push_back(true);
 *
 * // Type-heterogeneous elements
 * JsonValue first = arr[0];  // int64_t
 * JsonValue second = arr[1]; // std::string
 * JsonValue third = arr[2];  // bool
 * @endcode
 *
 * @note Elements can be of any JsonValue type (heterogeneous)
 * @note Uses std::vector for O(1) random access and efficient memory usage
 *
 * @see JsonValue
 * @see JsonObject
 */
using JsonArray = std::vector<struct JsonValue>;

/**
 * @struct JsonValue
 * @brief Variant-based JSON value type supporting all JSON types
 *
 * @details JsonValue can hold:
 * - null (std::nullptr_t)
 * - boolean (bool)
 * - integer (int64_t)
 * - floating-point (double)
 * - string (std::string)
 * - array (JsonArray)
 * - object (JsonObject)
 *
 * @section type_checking Type Checking
 * - is_null(), is_bool(), is_int(), is_number(), is_string(), is_array(), is_object()
 *
 * @section access Value Access
 * @code{.cpp}
 * JsonValue j = 42;
 * if (j.is_int()) {
 *     int64_t value = std::get<int64_t>(j);
 * }
 *
 * // Or with get_if for exception-free access
 * if (auto* p = std::get_if<int64_t>(&j)) {
 *     int64_t value = *p;
 * }
 * @endcode
 *
 * @see JsonObject
 * @see JsonArray
 */
struct JsonValue : std::variant<std::nullptr_t, bool, int64_t, double, std::string, JsonArray, JsonObject>
{
    using variant::variant;

    /**
     * @brief Check if value is null
     *
     * @return true if value holds nullptr_t, false otherwise
     *
     * @note This is a const noexcept method for safe use in constexpr contexts
     * @see is_bool, is_int, is_number
     */
    [[nodiscard]] bool is_null() const noexcept
    {
        return std::holds_alternative<std::nullptr_t>(*this);
    }

    /**
     * @brief Check if value is a boolean
     *
     * @return true if value holds bool, false otherwise
     *
     * @code{.cpp}
     * JsonValue j = true;
     * assert(j.is_bool());  // true
     * @endcode
     *
     * @note This is a const noexcept method for safe use in constexpr contexts
     * @see is_null, is_int, is_string
     */
    [[nodiscard]] bool is_bool() const noexcept
    {
        return std::holds_alternative<bool>(*this);
    }

    /**
     * @brief Check if value is an integer
     *
     * @return true if value holds int64_t, false otherwise
     *
     * @details Returns true only for values explicitly stored as int64_t.
     * Use is_number() to check for any numeric type (int or double).
     *
     * @code{.cpp}
     * JsonValue j1 = 42;      // int64_t
     * JsonValue j2 = 3.14;    // double
     * assert(j1.is_int());    // true
     * assert(!j2.is_int());   // false
     * @endcode
     *
     * @note This is a const noexcept method
     * @see is_number, is_double
     */
    [[nodiscard]] bool is_int() const noexcept
    {
        return std::holds_alternative<int64_t>(*this);
    }

    /**
     * @brief Check if value is any numeric type (int or double)
     *
     * @return true if value holds int64_t or double, false otherwise
     *
     * @details This method returns true for both integer and floating-point values.
     * Use is_int() or is_double() to distinguish between them.
     *
     * @code{.cpp}
     * JsonValue j1 = 42;      // int64_t
     * JsonValue j2 = 3.14;    // double
     * JsonValue j3 = "text";  // string
     * assert(j1.is_number()); // true
     * assert(j2.is_number()); // true
     * assert(!j3.is_number()); // false
     * @endcode
     *
     * @note This is a const noexcept method
     * @see is_int, is_double
     */
    [[nodiscard]] bool is_number() const noexcept
    {
        return std::holds_alternative<int64_t>(*this) || std::holds_alternative<double>(*this);
    }

    /**
     * @brief Check if value is a string
     *
     * @return true if value holds std::string, false otherwise
     *
     * @code{.cpp}
     * JsonValue j = std::string("hello");
     * assert(j.is_string());  // true
     * @endcode
     *
     * @note This is a const noexcept method
     * @see is_bool, is_int, is_array
     */
    [[nodiscard]] bool is_string() const noexcept
    {
        return std::holds_alternative<std::string>(*this);
    }

    /**
     * @brief Check if value is an array
     *
     * @return true if value holds JsonArray, false otherwise
     *
     * @details JsonArray is std::vector<JsonValue>, allowing heterogeneous elements.
     *
     * @code{.cpp}
     * JsonArray arr;
     * arr.push_back(1);
     * arr.push_back("text");
     * JsonValue j = arr;
     * assert(j.is_array());  // true
     * @endcode
     *
     * @note This is a const noexcept method
     * @see is_object, JsonArray
     */
    [[nodiscard]] bool is_array() const noexcept
    {
        return std::holds_alternative<JsonArray>(*this);
    }

    /**
     * @brief Check if value is an object
     *
     * @return true if value holds JsonObject, false otherwise
     *
     * @details JsonObject is std::map<std::string, JsonValue>, providing ordered
     * key-value pairs.
     *
     * @code{.cpp}
     * JsonObject obj;
     * obj["key"] = "value";
     * JsonValue j = obj;
     * assert(j.is_object());  // true
     * @endcode
     *
     * @note This is a const noexcept method
     * @see is_array, JsonObject
     */
    [[nodiscard]] bool is_object() const noexcept
    {
        return std::holds_alternative<JsonObject>(*this);
    }
};

namespace json_detail
{
/**
 * @brief Get human-readable type name from JsonValue
 *
 * @param j JsonValue to get type name from
 * @return std::string_view Type name ("null", "boolean", "integer", "number", "string", "array", "object", or
 * "unknown")
 *
 * @details Returns a string view representing the actual type held by the JsonValue.
 * This is primarily used for error messages and debugging.
 *
 * @code{.cpp}
 * JsonValue j1 = 42;
 * JsonValue j2 = "hello";
 * JsonValue j3 = true;
 *
 * assert(typeName(j1) == "integer");
 * assert(typeName(j2) == "string");
 * assert(typeName(j3) == "boolean");
 * @endcode
 *
 * @note Returns "number" for any numeric type, "integer" specifically for int64_t
 * @note This is a noexcept function safe to call in any context
 *
 * @see JsonValue
 */
inline std::string_view typeName(const JsonValue& j) noexcept
{
    if (j.is_null())
    {
        return "null";
    }
    if (j.is_bool())
    {
        return "boolean";
    }
    if (j.is_int())
    {
        return "integer";
    }
    if (j.is_number())
    {
        return "number";
    }
    if (j.is_string())
    {
        return "string";
    }
    if (j.is_array())
    {
        return "array";
    }
    if (j.is_object())
    {
        return "object";
    }
    return "unknown";
}
} // namespace json_detail

/**
 * @enum NumberFormat
 * @brief Floating-point number formatting options for JSON output
 *
 * @details Controls how floating-point numbers are formatted when serializing to JSON.
 * Can be specified per-policy to customize numeric output formatting.
 *
 * @see StandardJsonPolicy
 * @see FixedFormatPolicy
 * @see ScientificFormatPolicy
 */
enum class NumberFormat
{
    Auto,       ///< Automatic formatting (default): shortest representation
    Fixed,      ///< Fixed-point notation (e.g., "123.456")
    Scientific, ///< Scientific notation (e.g., "1.23e+02")
    Shortest    ///< Shortest representation between fixed and scientific
};

/**
 * @struct StandardJsonPolicy
 * @brief Default policy for JSON serialization and parsing
 *
 * @details Provides standard JSON processing with reasonable defaults suitable for
 * most use cases. All other policies inherit from this and override specific settings.
 *
 * @section settings Configuration Settings
 * - **pretty_print**: Enable pretty-printing with indentation (default: true)
 * - **numeric_precision**: Floating-point precision for output (default: 16)
 * - **indent_step**: Spaces per indentation level (default: 4)
 * - **allow_nan_inf**: Allow NaN and Infinity values (default: false, strict JSON)
 * - **escape_unicode**: Escape Unicode characters above ASCII (default: true)
 * - **max_parse_depth**: Maximum nesting depth during parsing (default: 64)
 * - **max_dump_depth**: Maximum nesting depth during serialization (default: 64)
 * - **number_format**: Numeric formatting style (default: Auto)
 *
 * @code{.cpp}
 * // Use standard policy
 * save_json_to_file<StandardJsonPolicy>("config.json", json_value);
 *
 * // Create custom policy
 * struct MyPolicy : StandardJsonPolicy {
 *     static constexpr bool pretty_print = false;
 *     static constexpr int numeric_precision = 6;
 * };
 * @endcode
 *
 * @note This policy follows strict JSON spec (no NaN/Inf, escaped Unicode)
 * @see PrettyJsonPolicy
 * @see CompatJsonPolicy
 */
struct StandardJsonPolicy
{
    static constexpr bool pretty_print = true;    ///< Enable indentation and newlines
    static constexpr int numeric_precision = 16;  ///< Decimal precision for floats
    static constexpr int indent_step = 4;         ///< Spaces per indent level
    static constexpr bool allow_nan_inf = false;  ///< Allow non-standard NaN/Infinity
    static constexpr bool escape_unicode = true;  ///< Escape non-ASCII characters
    static constexpr bool allow_comments = false; ///< Allow C-style comments (// and /* */)
    static constexpr size_t max_parse_depth = 64; ///< Max parsing recursion depth (conservative for stack safety)
    static constexpr size_t max_dump_depth = 64;  ///< Max serialization recursion depth
    static constexpr NumberFormat number_format = NumberFormat::Auto; ///< Number output format
};

/**
 * @struct PrettyJsonPolicy
 * @brief Policy explicitly enabling pretty-printing
 *
 * @details Identical to StandardJsonPolicy but makes the intent explicit.
 * Use this when you want to emphasize that pretty-printing is enabled.
 *
 * @code{.cpp}
 * save_params<Config, PrettyJsonPolicy>("config.json", cfg);
 * @endcode
 *
 * @see StandardJsonPolicy
 */
struct PrettyJsonPolicy : StandardJsonPolicy
{
    static constexpr bool pretty_print = true;
};

/**
 * @struct FixedFormatPolicy
 * @brief Policy for fixed-point number formatting
 *
 * @details Forces all floating-point numbers to use fixed-point notation
 * instead of scientific notation. Useful when you want consistent decimal
 * representation regardless of magnitude.
 *
 * @code{.cpp}
 * // Outputs: 123.456 instead of 1.23e+02
 * save_json_to_file<FixedFormatPolicy>("data.json", value);
 * @endcode
 *
 * @see ScientificFormatPolicy
 * @see StandardJsonPolicy
 */
struct FixedFormatPolicy : StandardJsonPolicy
{
    static constexpr NumberFormat number_format = NumberFormat::Fixed;
};

/**
 * @struct ScientificFormatPolicy
 * @brief Policy for scientific notation formatting
 *
 * @details Forces all floating-point numbers to use scientific notation.
 * Useful for very large or very small numbers, or when you need consistent
 * exponential representation.
 *
 * @code{.cpp}
 * // Outputs: 1.23e+02 instead of 123.0
 * save_json_to_file<ScientificFormatPolicy>("data.json", value);
 * @endcode
 *
 * @see FixedFormatPolicy
 * @see StandardJsonPolicy
 */
struct ScientificFormatPolicy : StandardJsonPolicy
{
    static constexpr NumberFormat number_format = NumberFormat::Scientific;
};

/**
 * @struct CompatJsonPolicy
 * @brief Policy for relaxed JSON compatibility
 *
 * @details Enables non-standard JSON extensions for better compatibility
 * with JavaScript and other systems:
 * - Allows NaN and Infinity values
 * - Does not escape Unicode characters (outputs UTF-8 directly)
 *
 * @warning This produces non-standard JSON that may not parse correctly
 *          in strict JSON parsers
 *
 * @code{.cpp}
 * // Allow NaN/Infinity and raw Unicode
 * save_json_to_file<CompatJsonPolicy>("compat.json", value);
 * @endcode
 *
 * @note Use this when interfacing with JavaScript or systems that support extended JSON
 * @see StandardJsonPolicy
 */
struct CompatJsonPolicy : StandardJsonPolicy
{
    static constexpr bool allow_nan_inf = true;   ///< Allow NaN and Infinity
    static constexpr bool escape_unicode = false; ///< Output raw UTF-8
};

/**
 * @struct ConfigJsonPolicy
 * @brief Policy for human-editable configuration files
 *
 * @details Enables comment support for configuration files while maintaining
 * other standard JSON behaviors. Supports C-style comments:
 * - Line comments: // comment text
 * - Block comments: slash-star comment text star-slash
 *
 * @warning This produces/accepts non-standard JSON with comments
 *
 * @code{.cpp}
 * auto config = parse_json<ConfigJsonPolicy>(R"(
 * {
 *     "port": 8080,
 *     "db_host": "localhost"
 * }
 * )");
 * @endcode
 *
 * @note Comments are stripped during parsing and not preserved in output
 * @see StandardJsonPolicy
 */
struct ConfigJsonPolicy : StandardJsonPolicy
{
    static constexpr bool allow_comments = true;
};

namespace json_detail
{
struct PolicyContext
{
    int numeric_precision = 16;
    bool allow_nan_inf = false;
    bool escape_unicode = true;

    template <typename Policy>
    static PolicyContext from_policy()
    {
        PolicyContext ctx;
        ctx.numeric_precision = Policy::numeric_precision;
        ctx.allow_nan_inf = Policy::allow_nan_inf;
        ctx.escape_unicode = Policy::escape_unicode;
        return ctx;
    }
};

inline thread_local std::unique_ptr<PolicyContext> current_policy_context = nullptr;

template <typename Policy>
struct PolicyScope
{
    std::unique_ptr<PolicyContext> prev_ctx;

    PolicyScope()
        : prev_ctx(std::move(current_policy_context))
    {
        current_policy_context = std::make_unique<PolicyContext>(PolicyContext::from_policy<Policy>());
    }

    ~PolicyScope()
    {
        current_policy_context = std::move(prev_ctx);
    }

    PolicyScope(const PolicyScope&) = delete;
    PolicyScope& operator=(const PolicyScope&) = delete;
    PolicyScope(PolicyScope&&) = delete;
    PolicyScope& operator=(PolicyScope&&) = delete;
};

template <typename Policy>
inline int get_effective_numeric_precision()
{
    return current_policy_context ? current_policy_context->numeric_precision : Policy::numeric_precision;
}

template <typename Policy>
inline bool get_effective_allow_nan_inf()
{
    return current_policy_context ? current_policy_context->allow_nan_inf : Policy::allow_nan_inf;
}

template <typename Policy>
inline bool get_effective_escape_unicode()
{
    return current_policy_context ? current_policy_context->escape_unicode : Policy::escape_unicode;
}

} // namespace json_detail

namespace json_detail
{
/**
 * @brief Write indentation spaces to output stream
 *
 * @tparam Os Output stream type (must support operator<<)
 * @param os Output stream to write to
 * @param indent Number of space characters to output
 */
template <typename Os>
inline void output_indent(Os& os, int indent)
{
    for (int i = 0; i < indent; ++i)
    {
        os << ' ';
    }
}

/**
 * @brief Write JSON unicode escape sequence to output stream
 *
 * @details Outputs \uXXXX for BMP codepoints (U+0000 to U+FFFF).
 * Outputs surrogate pair \uXXXX\uXXXX for supplementary plane codepoints.
 *
 * @tparam Os Output stream type (must support operator<<)
 * @param os Output stream to write to
 * @param codepoint Unicode codepoint to escape (0x000000 to 0x10FFFF)
 */
template <typename Os>
inline void output_unicode_escape(Os& os, uint32_t codepoint)
{
    if (codepoint <= 0xFFFF)
    {
        os << "\\u" << std::hex << std::setw(4) << std::setfill('0') << codepoint << std::dec;
    }
    else
    {
        // Surrogate pair for supplementary plane
        codepoint -= 0x10000;
        uint16_t high = 0xD800 + ((codepoint >> 10) & 0x3FF);
        uint16_t low = 0xDC00 + (codepoint & 0x3FF);
        os << "\\u" << std::hex << std::setw(4) << std::setfill('0') << high << "\\u" << std::setw(4)
           << std::setfill('0') << low << std::dec;
    }
}

/**
 * @brief Write JSON-escaped string to output stream
 *
 * @details Outputs a quoted string with proper JSON escaping:
 * - Control characters (U+0000 to U+001F): \uXXXX
 * - Quote, backslash, and whitespace: \", \\, \b, \f, \n, \r, \t
 * - Non-ASCII (when Policy::escape_unicode is true): \uXXXX or surrogate pairs
 * - Invalid UTF-8 sequences: escaped byte-by-byte
 *
 * @tparam Os Output stream type (must support operator<<)
 * @tparam Policy JSON policy controlling escape_unicode behavior
 * @param os Output stream to write to
 * @param s String view to escape and output
 *
 * @see output_unicode_escape
 * @see StandardJsonPolicy::escape_unicode
 */
template <typename Os, typename Policy>
void escape_string(Os& os, std::string_view s)
{
    bool escape_unicode = get_effective_escape_unicode<Policy>();
    os << '"';

    for (size_t i = 0; i < s.size();)
    {
        unsigned char c = static_cast<unsigned char>(s[i]);

        switch (c)
        {
            case '"':
                os << R"(\")";
                ++i;
                continue;
            case '\\':
                os << R"(\\)";
                ++i;
                continue;
            case '\b':
                os << R"(\b)";
                ++i;
                continue;
            case '\f':
                os << R"(\f)";
                ++i;
                continue;
            case '\n':
                os << R"(\n)";
                ++i;
                continue;
            case '\r':
                os << R"(\r)";
                ++i;
                continue;
            case '\t':
                os << R"(\t)";
                ++i;
                continue;
            default:
                break;
        }

        // Control characters (0x00-0x1F)
        if (c < 0x20)
        {
            output_unicode_escape(os, c);
            ++i;
            continue;
        }

        // ASCII printable (0x20-0x7F)
        if (c < 0x80)
        {
            os << static_cast<char>(c);
            ++i;
            continue;
        }

        // Non-ASCII: pass through if not escaping unicode
        if (!escape_unicode)
        {
            os << static_cast<char>(c);
            ++i;
            continue;
        }

        // Decode UTF-8 sequence
        uint32_t codepoint = 0;
        size_t bytes = 0;

        if ((c & 0xE0) == 0xC0)
        {
            bytes = 2;
            codepoint = c & 0x1F;
        }
        else if ((c & 0xF0) == 0xE0)
        {
            bytes = 3;
            codepoint = c & 0x0F;
        }
        else if ((c & 0xF8) == 0xF0)
        {
            bytes = 4;
            codepoint = c & 0x07;
        }
        else
        {
            // Invalid lead byte - escape as-is
            output_unicode_escape(os, c);
            ++i;
            continue;
        }

        // Incomplete sequence
        if (i + bytes > s.size())
        {
            output_unicode_escape(os, c);
            ++i;
            continue;
        }

        // Validate and decode continuation bytes
        bool valid = true;
        for (size_t j = 1; j < bytes; ++j)
        {
            unsigned char cont = static_cast<unsigned char>(s[i + j]);
            if ((cont & 0xC0) != 0x80)
            {
                valid = false;
                break;
            }
            codepoint = (codepoint << 6) | (cont & 0x3F);
        }

        if (!valid)
        {
            output_unicode_escape(os, c);
            ++i;
            continue;
        }

        output_unicode_escape(os, codepoint);
        i += bytes;
    }

    os << '"';
}

/**
 * @brief Write JSON scalar value to output stream
 *
 * @details Handles serialization of primitive JSON types:
 * - bool: outputs "true" or "false"
 * - int64_t: outputs decimal integer
 * - floating-point: outputs number with policy-controlled formatting
 * - string-convertible: outputs JSON-escaped quoted string
 *
 * @tparam Os Output stream type (must support operator<<)
 * @tparam T Scalar type to serialize
 * @tparam Policy JSON policy controlling numeric formatting and NaN/Inf handling
 * @param os Output stream to write to
 * @param obj Value to serialize
 *
 * @see StandardJsonPolicy::allow_nan_inf
 * @see StandardJsonPolicy::numeric_precision
 * @see StandardJsonPolicy::number_format
 */
template <typename Os, typename T, typename Policy>
void dump_scalar(Os& os, const T& obj)
{
    if constexpr (std::is_same_v<T, bool>)
    {
        os << (obj ? "true" : "false");
    }
    else if constexpr (std::is_same_v<T, int64_t>)
    {
        os << obj;
    }
    else if constexpr (std::is_arithmetic_v<T>)
    {
        if constexpr (std::is_floating_point_v<T>)
        {
            // NaN handling: JSON has no NaN literal, so either use extension or null
            if (std::isnan(obj))
            {
                bool allow_nan_inf = get_effective_allow_nan_inf<Policy>();
                if (allow_nan_inf)
                {
                    os << "NaN";
                }
                else
                {
                    os << "null";
                }
            }
            // Infinity handling: same rationale as NaN
            else if (std::isinf(obj))
            {
                bool allow_nan_inf = get_effective_allow_nan_inf<Policy>();
                if (allow_nan_inf)
                {
                    os << (obj > 0 ? "Infinity" : "-Infinity");
                }
                else
                {
                    os << "null";
                }
            }
            else
            {
                double intpart;
                // Whole-number check: output "42" instead of "42.0000000000000000"
                // The +/-1000 margin avoids edge cases near int64_t bounds where
                // double precision loss could cause incorrect round-trip conversion
                if (fabs(std::modf(obj, &intpart)) < json_detail::double_epsilon &&
                    intpart >= static_cast<double>(std::numeric_limits<int64_t>::min() + 1000) &&
                    intpart <= static_cast<double>(std::numeric_limits<int64_t>::max()) - 1000)
                {
                    os << static_cast<int64_t>(intpart);
                }
                else
                {
                    int numeric_precision = get_effective_numeric_precision<Policy>();
                    constexpr auto format = Policy::number_format;

                    if constexpr (format == NumberFormat::Fixed)
                    {
                        os << std::fixed << std::setprecision(numeric_precision) << obj;
                    }
                    else if constexpr (format == NumberFormat::Scientific)
                    {
                        os << std::scientific << std::setprecision(numeric_precision) << obj;
                    }
                    else if constexpr (format == NumberFormat::Shortest)
                    {
                        os << std::setprecision(numeric_precision) << obj;
                    }
                    else
                    {
                        // NumberFormat::Auto: use scientific for very large/small values
                        // Thresholds chosen for human readability:
                        // - Below 1e-6: scientific avoids "0.000000123456..."
                        // - Above 1e15: scientific avoids "1234567890123456.0..."
                        double abs_val = std::abs(obj);
                        if (abs_val != 0.0 && (abs_val < 1e-6 || abs_val > 1e15))
                        {
                            os << std::scientific << std::setprecision(numeric_precision) << obj;
                        }
                        else
                        {
                            os << std::fixed << std::setprecision(numeric_precision) << obj;
                        }
                    }
                }
            }
        }
        else
        {
            // Non-floating arithmetic (int, short, char, etc.): widen to int64_t
            os << static_cast<int64_t>(obj);
        }
    }
    else if constexpr (std::is_convertible_v<T, std::string_view>)
    {
        escape_string<Os, Policy>(os, obj);
    }
}

/**
 * @brief Serialize tuple elements to JSON array (forward declaration)
 *
 * @tparam Os Output stream type
 * @tparam Policy JSON serialization policy
 * @tparam Tuple Tuple type to serialize
 * @tparam I Index sequence for tuple elements
 */
template <typename Os, typename Policy, typename Tuple, std::size_t... I>
void dump_tuple_impl(Os& os, const Tuple& tup, std::index_sequence<I...>, bool pretty, int indent);

// ============================================================================
// Type Traits for JSON Serialization
// ============================================================================

/**
 * @brief Detect std::optional types
 * @tparam T Type to check
 */
template <typename T>
struct is_optional : std::false_type
{
};

template <typename T>
struct is_optional<std::optional<T>> : std::true_type
{
};

template <typename T>
inline constexpr bool is_optional_v = is_optional<T>::value;

/**
 * @brief Detect std::pair types
 * @tparam T Type to check
 */
template <typename T>
struct is_pair : std::false_type
{
};

template <typename T1, typename T2>
struct is_pair<std::pair<T1, T2>> : std::true_type
{
};

template <typename T>
inline constexpr bool is_pair_v = is_pair<T>::value;

/**
 * @brief Detect std::tuple types
 * @tparam T Type to check
 */
template <typename T>
struct is_tuple : std::false_type
{
};

template <typename... Ts>
struct is_tuple<std::tuple<Ts...>> : std::true_type
{
};

template <typename T>
inline constexpr bool is_tuple_v = is_tuple<T>::value;

/**
 * @brief Extract value type from std::optional, or return T unchanged
 * @tparam T Type to unwrap (may or may not be std::optional)
 */
template <typename T>
struct optional_value_type
{
    using type = T;
};

template <typename T>
struct optional_value_type<std::optional<T>>
{
    using type = T;
};

template <typename T>
using optional_value_type_t = typename optional_value_type<T>::type;

/**
 * @brief Detect if type has ADL-discoverable to_json(JsonValue&, const T&)
 * @tparam T Type to check for to_json support
 */
template <typename T, typename = void>
struct has_to_json : std::false_type
{
};

// SFINAE: succeeds only if to_json(JsonValue&, const T&) is valid
template <typename T>
struct has_to_json<T, std::void_t<decltype(to_json(std::declval<JsonValue&>(), std::declval<const T&>()))>>
    : std::true_type
{
};

template <typename T>
inline constexpr bool has_to_json_v = has_to_json<T>::value;

/**
 * @brief Detect sequence containers (vector, deque, list)
 * @tparam T Type to check
 */
template <typename T>
struct is_sequence_container : std::false_type
{
};

template <typename T>
struct is_sequence_container<std::vector<T>> : std::true_type
{
};

template <typename T>
struct is_sequence_container<std::deque<T>> : std::true_type
{
};

template <typename T>
struct is_sequence_container<std::list<T>> : std::true_type
{
};

template <typename T>
inline constexpr bool is_sequence_container_v = is_sequence_container<T>::value;

/**
 * @brief Verify serialization depth does not exceed policy limit
 *
 * @tparam Policy JSON policy with max_dump_depth setting
 * @param indent Current indentation level (used to calculate depth)
 * @throws std::runtime_error if depth exceeds Policy::max_dump_depth
 */
template <typename Policy>
inline void check_dump_depth(int indent)
{
    // Derive nesting depth from indentation level
    // Guard against indent_step == 0 to prevent division by zero
    size_t depth = static_cast<size_t>(indent / (Policy::indent_step > 0 ? Policy::indent_step : 1));
    FATP_JSON_ENFORCE(depth <= Policy::max_dump_depth,
                      "JSON dump error: maximum nesting depth exceeded",
                      "max_depth",
                      Policy::max_dump_depth,
                      "current_depth",
                      depth);
}
} // namespace json_detail

// ============================================================================
// JsonDispatcher - Type-Based Serialization Dispatch
// ============================================================================

/**
 * @brief Primary template for JSON serialization dispatch
 *
 * @details Specializations handle different C++ types and serialize them
 * to appropriate JSON representations. Uses SFINAE to select the correct
 * specialization based on type traits.
 *
 * @tparam T Type to serialize
 * @tparam Policy JSON serialization policy
 */
template <typename T, typename Policy, typename = void>
struct JsonDispatcher;

/**
 * @brief JsonDispatcher specialization for nullptr_t
 * @details Outputs JSON "null" literal
 */
template <typename Policy>
struct JsonDispatcher<std::nullptr_t, Policy>
{
    template <typename Os>
    static void dump(Os& os, std::nullptr_t, bool = Policy::pretty_print, int = 0)
    {
        os << "null";
    }
};

/**
 * @brief JsonDispatcher specialization for scalar and user-defined types
 *
 * @details Handles types that are:
 * - Not iterable (or are std::string, which is iterable but treated as scalar)
 * - Not std::optional, std::pair, or std::tuple
 *
 * User-defined types with to_json overloads are converted via that overload.
 */
template <typename T, typename Policy>
struct JsonDispatcher<
    T,
    Policy,
    std::enable_if_t<(!json_detail::IsIterable<T>::value || std::is_same_v<std::decay_t<T>, std::string>) &&
                     !json_detail::is_optional_v<T> && !json_detail::is_pair_v<T> && !json_detail::is_tuple_v<T>>>
{
    template <typename Os>
    static void dump(Os& os, const T& obj, bool pretty = Policy::pretty_print, int indent = 0)
    {
        if constexpr (std::is_same_v<T, std::nullptr_t> || std::is_null_pointer_v<T>)
        {
            os << "null";
        }
        else if constexpr (!std::is_arithmetic_v<T> && !std::is_same_v<std::decay_t<T>, std::string> &&
                           !std::is_convertible_v<T, std::string_view> && json_detail::has_to_json_v<T>)
        {
            // User-defined type: delegate to ADL-found to_json, then serialize result
            JsonValue j;
            to_json(j, obj);
            JsonDispatcher<JsonValue, Policy>::dump(os, j, pretty, indent);
        }
        else
        {
            // Built-in scalar: bool, integer, float, or string
            json_detail::dump_scalar<Os, T, Policy>(os, obj);
        }
    }
};

/**
 * @brief JsonDispatcher specialization for std::optional
 * @details Outputs contained value if present, "null" otherwise
 */
template <typename T, typename Policy>
struct JsonDispatcher<std::optional<T>, Policy>
{
    template <typename Os>
    static void dump(Os& os, const std::optional<T>& opt, bool pretty = Policy::pretty_print, int indent = 0)
    {
        if (opt.has_value())
        {
            JsonDispatcher<T, Policy>::dump(os, *opt, pretty, indent);
        }
        else
        {
            os << "null";
        }
    }
};

/**
 * @brief JsonDispatcher specialization for std::pair
 * @details Outputs as JSON array with two elements: [first, second]
 */
template <typename T1, typename T2, typename Policy>
struct JsonDispatcher<std::pair<T1, T2>, Policy>
{
    template <typename Os>
    static void dump(Os& os, const std::pair<T1, T2>& p, bool pretty = Policy::pretty_print, int indent = 0)
    {
        os << '[';
        if (pretty)
        {
            os << '\n';
        }
        if (pretty)
        {
            json_detail::output_indent(os, indent + Policy::indent_step);
        }
        JsonDispatcher<T1, Policy>::dump(os, p.first, pretty, indent + Policy::indent_step);
        os << ',';
        if (pretty)
        {
            os << '\n';
            json_detail::output_indent(os, indent + Policy::indent_step);
        }
        JsonDispatcher<T2, Policy>::dump(os, p.second, pretty, indent + Policy::indent_step);
        if (pretty)
        {
            os << '\n';
            json_detail::output_indent(os, indent);
        }
        os << ']';
    }
};

/**
 * @brief JsonDispatcher specialization for std::tuple
 * @details Outputs as JSON array with one element per tuple element
 */
template <typename... Ts, typename Policy>
struct JsonDispatcher<std::tuple<Ts...>, Policy>
{
    template <typename Os>
    static void dump(Os& os, const std::tuple<Ts...>& tup, bool pretty = Policy::pretty_print, int indent = 0)
    {
        json_detail::dump_tuple_impl<Os, Policy>(os, tup, std::make_index_sequence<sizeof...(Ts)>(), pretty, indent);
    }
};

/**
 * @brief JsonDispatcher specialization for sequence containers
 *
 * @details Handles iterable types without mapped_type (vector, set, deque, list, etc.).
 * Outputs as JSON array.
 *
 * @note std::string is excluded (handled as scalar despite being iterable)
 */
template <typename T, typename Policy>
struct JsonDispatcher<
    T,
    Policy,
    std::enable_if_t<json_detail::IsIterable<T>::value && !std::is_same_v<std::decay_t<T>, std::string> &&
                     !json_detail::HasMappedType<T>::value>>
{
    template <typename Os>
    static void dump(Os& os, const T& cont, bool pretty = Policy::pretty_print, int indent = 0)
    {
        json_detail::check_dump_depth<Policy>(indent);
        os << '[';
        if (pretty && !cont.empty())
        {
            os << '\n';
        }
        bool first = true;
        for (const auto& elem : cont)
        {
            // Comma before all elements except first
            if (!first)
            {
                os << ',';
            }
            if (pretty)
            {
                os << '\n';
                json_detail::output_indent(os, indent + Policy::indent_step);
            }
            first = false;
            JsonDispatcher<json_detail::ContainerValueT<T>, Policy>::dump(os,
                                                                          elem,
                                                                          pretty,
                                                                          indent + Policy::indent_step);
        }
        if (pretty && !cont.empty())
        {
            os << '\n';
            json_detail::output_indent(os, indent);
        }
        os << ']';
    }
};

/**
 * @brief JsonDispatcher specialization for associative containers
 *
 * @details Handles iterable types with mapped_type (map, unordered_map).
 * Outputs as JSON object. Non-string keys are converted via operator<<.
 */
template <typename T, typename Policy>
struct JsonDispatcher<
    T,
    Policy,
    std::enable_if_t<json_detail::IsIterable<T>::value && !std::is_same_v<std::decay_t<T>, std::string> &&
                     json_detail::HasMappedType<T>::value>>
{
    template <typename Os>
    static void dump(Os& os, const T& cont, bool pretty = Policy::pretty_print, int indent = 0)
    {
        json_detail::check_dump_depth<Policy>(indent);
        os << '{';
        if (pretty && !cont.empty())
        {
            os << '\n';
        }
        bool first = true;
        for (const auto& elem : cont)
        {
            if (!first)
            {
                os << ',';
            }
            if (pretty)
            {
                os << '\n';
                json_detail::output_indent(os, indent + Policy::indent_step);
            }
            first = false;

            // String-convertible keys: use directly
            if constexpr (std::is_convertible_v<typename T::key_type, std::string_view>)
            {
                json_detail::escape_string<Os, Policy>(os, elem.first);
            }
            else
            {
                // Non-string keys (int, enum, etc.): stringify via operator
                std::ostringstream key_stream;
                key_stream.imbue(std::locale::classic());
                key_stream << elem.first;
                json_detail::escape_string<Os, Policy>(os, key_stream.str());
            }

            os << (pretty ? " : " : ":");
            JsonDispatcher<typename T::mapped_type, Policy>::dump(os,
                                                                  elem.second,
                                                                  pretty,
                                                                  indent + Policy::indent_step);
        }
        if (pretty && !cont.empty())
        {
            os << '\n';
            json_detail::output_indent(os, indent);
        }
        os << '}';
    }
};

/**
 * @brief JsonDispatcher specialization for JsonValue
 * @details Uses std::visit to dispatch to the appropriate type handler
 */
template <typename Policy>
struct JsonDispatcher<JsonValue, Policy>
{
    template <typename Os>
    static void dump(Os& os, const JsonValue& val, bool pretty = Policy::pretty_print, int indent = 0)
    {
        std::visit(
            [&](auto&& arg) {
                JsonDispatcher<std::decay_t<decltype(arg)>, Policy>::dump(os, arg, pretty, indent);
            },
            val);
    }
};

namespace json_detail
{

/**
 * @brief Serialize tuple elements to JSON array
 *
 * @tparam Os Output stream type
 * @tparam Policy JSON serialization policy
 * @tparam Tuple Tuple type
 * @tparam I Index sequence matching tuple size
 * @param os Output stream
 * @param tup Tuple to serialize
 * @param pretty Enable pretty-printing
 * @param indent Current indentation level
 */
template <typename Os, typename Policy, typename Tuple, std::size_t... I>
void dump_tuple_impl(Os& os, const Tuple& tup, std::index_sequence<I...>, bool pretty, int indent)
{
    os << '[';
    if (pretty && sizeof...(I) > 0)
    {
        os << '\n';
    }
    bool first = true;
    // Fold expression: invoke lambda for each index in sequence
    (..., ([&]() {
         if (!first)
         {
             os << ',';
         }
         if (pretty)
         {
             os << '\n';
             output_indent(os, indent + Policy::indent_step);
         }
         first = false;
         JsonDispatcher<std::tuple_element_t<I, Tuple>, Policy>::dump(os,
                                                                      std::get<I>(tup),
                                                                      pretty,
                                                                      indent + Policy::indent_step);
     })());
    if (pretty && sizeof...(I) > 0)
    {
        os << '\n';
        output_indent(os, indent);
    }
    os << ']';
}
} // namespace json_detail

// ============================================================================
// High-Level Serialization API
// ============================================================================

/**
 * @brief Serialize object to JSON string
 *
 * @tparam T Type to serialize (must have JsonDispatcher support)
 * @tparam Policy JSON serialization policy (default: StandardJsonPolicy)
 * @param obj Object to serialize
 * @param pretty Enable pretty-printing with indentation
 * @return std::string JSON representation
 *
 * @code{.cpp}
 * Config cfg{8080, "localhost"};
 * std::string json = to_json_string(cfg);
 * std::string compact = to_json_string<Config, StandardJsonPolicy>(cfg, false);
 * @endcode
 */
template <typename T, typename Policy = StandardJsonPolicy>
std::string to_json_string(const T& obj, bool pretty = Policy::pretty_print)
{
    json_detail::PolicyScope<Policy> scope;
    std::ostringstream oss;
    oss.imbue(std::locale::classic()); // Locale-independent numeric formatting
    JsonDispatcher<T, Policy>::dump(oss, obj, pretty);
    return oss.str();
}

/**
 * @brief Serialize object to output stream
 *
 * @tparam T Type to serialize
 * @tparam Policy JSON serialization policy
 * @tparam Os Output stream type
 * @param os Output stream to write to
 * @param obj Object to serialize
 * @param pretty Enable pretty-printing
 *
 * @code{.cpp}
 * std::ofstream file("config.json");
 * to_json_stream(file, cfg);
 * @endcode
 */
template <typename T, typename Policy = StandardJsonPolicy, typename Os>
void to_json_stream(Os& os, const T& obj, bool pretty = Policy::pretty_print)
{
    json_detail::PolicyScope<Policy> scope;
    JsonDispatcher<T, Policy>::dump(os, obj, pretty);
}

/**
 * @brief Identity conversion for JsonValue
 * @param value JsonValue to return unchanged
 * @return Copy of input value
 */
inline JsonValue to_json(const JsonValue& value)
{
    return value;
}

#define FATP_JSON_EXPAND(x) x

#define FATP_JSON_ARG_COUNT_IMPL(_1,  \
                                 _2,  \
                                 _3,  \
                                 _4,  \
                                 _5,  \
                                 _6,  \
                                 _7,  \
                                 _8,  \
                                 _9,  \
                                 _10, \
                                 _11, \
                                 _12, \
                                 _13, \
                                 _14, \
                                 _15, \
                                 _16, \
                                 _17, \
                                 _18, \
                                 _19, \
                                 _20, \
                                 _21, \
                                 _22, \
                                 _23, \
                                 _24, \
                                 _25, \
                                 _26, \
                                 _27, \
                                 _28, \
                                 _29, \
                                 _30, \
                                 _31, \
                                 _32, \
                                 _33, \
                                 _34, \
                                 _35, \
                                 _36, \
                                 _37, \
                                 _38, \
                                 _39, \
                                 _40, \
                                 _41, \
                                 _42, \
                                 _43, \
                                 _44, \
                                 _45, \
                                 _46, \
                                 _47, \
                                 _48, \
                                 _49, \
                                 _50, \
                                 N,   \
                                 ...) \
    N
#define FATP_JSON_ARG_COUNT(...)                           \
    FATP_JSON_EXPAND(FATP_JSON_ARG_COUNT_IMPL(__VA_ARGS__, \
                                              50,          \
                                              49,          \
                                              48,          \
                                              47,          \
                                              46,          \
                                              45,          \
                                              44,          \
                                              43,          \
                                              42,          \
                                              41,          \
                                              40,          \
                                              39,          \
                                              38,          \
                                              37,          \
                                              36,          \
                                              35,          \
                                              34,          \
                                              33,          \
                                              32,          \
                                              31,          \
                                              30,          \
                                              29,          \
                                              28,          \
                                              27,          \
                                              26,          \
                                              25,          \
                                              24,          \
                                              23,          \
                                              22,          \
                                              21,          \
                                              20,          \
                                              19,          \
                                              18,          \
                                              17,          \
                                              16,          \
                                              15,          \
                                              14,          \
                                              13,          \
                                              12,          \
                                              11,          \
                                              10,          \
                                              9,           \
                                              8,           \
                                              7,           \
                                              6,           \
                                              5,           \
                                              4,           \
                                              3,           \
                                              2,           \
                                              1))

#define FATP_JSON_CAT(a, b) FATP_JSON_CAT_IMPL(a, b)
#define FATP_JSON_CAT_IMPL(a, b) a##b

#define FATP_JSON_TO_FIELD(field)                              \
    do                                                         \
    {                                                          \
        obj[#field] = json_detail::to_json_value(value.field); \
    } while (0)

#define FATP_JSON_FROM_FIELD(field)                                                                        \
    do                                                                                                     \
    {                                                                                                      \
        if (auto it = obj.find(#field); it != obj.end())                                                   \
        {                                                                                                  \
            try                                                                                            \
            {                                                                                              \
                from_json(it->second, value.field);                                                        \
            }                                                                                              \
            catch (const std::exception& e)                                                                \
            {                                                                                              \
                FATP_JSON_ENFORCE(false, "Error deserializing field", "field", #field, "error", e.what()); \
            }                                                                                              \
        }                                                                                                  \
        else if constexpr (!fat_p::json_detail::is_optional_v<decltype((value.field))>)                    \
        {                                                                                                  \
            FATP_JSON_ENFORCE(false, "Required field missing", "field", #field);                           \
        }                                                                                                  \
    } while (0)

#define FATP_JSON_FROM_FIELD_OPT(field)                  \
    do                                                   \
    {                                                    \
        if (auto it = obj.find(#field); it != obj.end()) \
        {                                                \
            from_json(it->second, value.field);          \
        }                                                \
    } while (0)

/**
 * @section macro_system Macro-Based Serialization System
 *
 * @details Variadic macros for automatic struct serialization.
 *
 * **Limits**: Maximum 50 fields per struct. Exceeding triggers static_assert.
 *
 * **Workarounds for >50 fields**:
 * - Nest structs to group related fields
 * - Write manual to_json/from_json functions
 * - Split into multiple structs
 */

#define FATP_JSON_APPLY_1(macro, x1) macro(x1);
#define FATP_JSON_APPLY_2(macro, x1, x2) \
    macro(x1);                           \
    macro(x2);
#define FATP_JSON_APPLY_3(macro, x1, x2, x3) \
    macro(x1);                               \
    macro(x2);                               \
    macro(x3);
#define FATP_JSON_APPLY_4(macro, x1, x2, x3, x4) \
    macro(x1);                                   \
    macro(x2);                                   \
    macro(x3);                                   \
    macro(x4);
#define FATP_JSON_APPLY_5(macro, x1, x2, x3, x4, x5) \
    macro(x1);                                       \
    macro(x2);                                       \
    macro(x3);                                       \
    macro(x4);                                       \
    macro(x5);
#define FATP_JSON_APPLY_6(macro, x1, x2, x3, x4, x5, x6) \
    macro(x1);                                           \
    macro(x2);                                           \
    macro(x3);                                           \
    macro(x4);                                           \
    macro(x5);                                           \
    macro(x6);
#define FATP_JSON_APPLY_7(macro, x1, x2, x3, x4, x5, x6, x7) \
    macro(x1);                                               \
    macro(x2);                                               \
    macro(x3);                                               \
    macro(x4);                                               \
    macro(x5);                                               \
    macro(x6);                                               \
    macro(x7);
#define FATP_JSON_APPLY_8(macro, x1, x2, x3, x4, x5, x6, x7, x8) \
    macro(x1);                                                   \
    macro(x2);                                                   \
    macro(x3);                                                   \
    macro(x4);                                                   \
    macro(x5);                                                   \
    macro(x6);                                                   \
    macro(x7);                                                   \
    macro(x8);
#define FATP_JSON_APPLY_9(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9) \
    macro(x1);                                                       \
    macro(x2);                                                       \
    macro(x3);                                                       \
    macro(x4);                                                       \
    macro(x5);                                                       \
    macro(x6);                                                       \
    macro(x7);                                                       \
    macro(x8);                                                       \
    macro(x9);
#define FATP_JSON_APPLY_10(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10) \
    macro(x1);                                                             \
    macro(x2);                                                             \
    macro(x3);                                                             \
    macro(x4);                                                             \
    macro(x5);                                                             \
    macro(x6);                                                             \
    macro(x7);                                                             \
    macro(x8);                                                             \
    macro(x9);                                                             \
    macro(x10);
#define FATP_JSON_APPLY_11(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11) \
    macro(x1);                                                                  \
    macro(x2);                                                                  \
    macro(x3);                                                                  \
    macro(x4);                                                                  \
    macro(x5);                                                                  \
    macro(x6);                                                                  \
    macro(x7);                                                                  \
    macro(x8);                                                                  \
    macro(x9);                                                                  \
    macro(x10);                                                                 \
    macro(x11);
#define FATP_JSON_APPLY_12(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12) \
    macro(x1);                                                                       \
    macro(x2);                                                                       \
    macro(x3);                                                                       \
    macro(x4);                                                                       \
    macro(x5);                                                                       \
    macro(x6);                                                                       \
    macro(x7);                                                                       \
    macro(x8);                                                                       \
    macro(x9);                                                                       \
    macro(x10);                                                                      \
    macro(x11);                                                                      \
    macro(x12);
#define FATP_JSON_APPLY_13(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13) \
    macro(x1);                                                                            \
    macro(x2);                                                                            \
    macro(x3);                                                                            \
    macro(x4);                                                                            \
    macro(x5);                                                                            \
    macro(x6);                                                                            \
    macro(x7);                                                                            \
    macro(x8);                                                                            \
    macro(x9);                                                                            \
    macro(x10);                                                                           \
    macro(x11);                                                                           \
    macro(x12);                                                                           \
    macro(x13);
#define FATP_JSON_APPLY_14(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14) \
    macro(x1);                                                                                 \
    macro(x2);                                                                                 \
    macro(x3);                                                                                 \
    macro(x4);                                                                                 \
    macro(x5);                                                                                 \
    macro(x6);                                                                                 \
    macro(x7);                                                                                 \
    macro(x8);                                                                                 \
    macro(x9);                                                                                 \
    macro(x10);                                                                                \
    macro(x11);                                                                                \
    macro(x12);                                                                                \
    macro(x13);                                                                                \
    macro(x14);
#define FATP_JSON_APPLY_15(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15) \
    macro(x1);                                                                                      \
    macro(x2);                                                                                      \
    macro(x3);                                                                                      \
    macro(x4);                                                                                      \
    macro(x5);                                                                                      \
    macro(x6);                                                                                      \
    macro(x7);                                                                                      \
    macro(x8);                                                                                      \
    macro(x9);                                                                                      \
    macro(x10);                                                                                     \
    macro(x11);                                                                                     \
    macro(x12);                                                                                     \
    macro(x13);                                                                                     \
    macro(x14);                                                                                     \
    macro(x15);
#define FATP_JSON_APPLY_16(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16) \
    macro(x1);                                                                                           \
    macro(x2);                                                                                           \
    macro(x3);                                                                                           \
    macro(x4);                                                                                           \
    macro(x5);                                                                                           \
    macro(x6);                                                                                           \
    macro(x7);                                                                                           \
    macro(x8);                                                                                           \
    macro(x9);                                                                                           \
    macro(x10);                                                                                          \
    macro(x11);                                                                                          \
    macro(x12);                                                                                          \
    macro(x13);                                                                                          \
    macro(x14);                                                                                          \
    macro(x15);                                                                                          \
    macro(x16);
#define FATP_JSON_APPLY_17(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17) \
    macro(x1);                                                                                                \
    macro(x2);                                                                                                \
    macro(x3);                                                                                                \
    macro(x4);                                                                                                \
    macro(x5);                                                                                                \
    macro(x6);                                                                                                \
    macro(x7);                                                                                                \
    macro(x8);                                                                                                \
    macro(x9);                                                                                                \
    macro(x10);                                                                                               \
    macro(x11);                                                                                               \
    macro(x12);                                                                                               \
    macro(x13);                                                                                               \
    macro(x14);                                                                                               \
    macro(x15);                                                                                               \
    macro(x16);                                                                                               \
    macro(x17);
#define FATP_JSON_APPLY_18(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18) \
    macro(x1);                                                                                                     \
    macro(x2);                                                                                                     \
    macro(x3);                                                                                                     \
    macro(x4);                                                                                                     \
    macro(x5);                                                                                                     \
    macro(x6);                                                                                                     \
    macro(x7);                                                                                                     \
    macro(x8);                                                                                                     \
    macro(x9);                                                                                                     \
    macro(x10);                                                                                                    \
    macro(x11);                                                                                                    \
    macro(x12);                                                                                                    \
    macro(x13);                                                                                                    \
    macro(x14);                                                                                                    \
    macro(x15);                                                                                                    \
    macro(x16);                                                                                                    \
    macro(x17);                                                                                                    \
    macro(x18);
#define FATP_JSON_APPLY_19(macro, \
                           x1,    \
                           x2,    \
                           x3,    \
                           x4,    \
                           x5,    \
                           x6,    \
                           x7,    \
                           x8,    \
                           x9,    \
                           x10,   \
                           x11,   \
                           x12,   \
                           x13,   \
                           x14,   \
                           x15,   \
                           x16,   \
                           x17,   \
                           x18,   \
                           x19)   \
    macro(x1);                    \
    macro(x2);                    \
    macro(x3);                    \
    macro(x4);                    \
    macro(x5);                    \
    macro(x6);                    \
    macro(x7);                    \
    macro(x8);                    \
    macro(x9);                    \
    macro(x10);                   \
    macro(x11);                   \
    macro(x12);                   \
    macro(x13);                   \
    macro(x14);                   \
    macro(x15);                   \
    macro(x16);                   \
    macro(x17);                   \
    macro(x18);                   \
    macro(x19);
#define FATP_JSON_APPLY_20(macro, \
                           x1,    \
                           x2,    \
                           x3,    \
                           x4,    \
                           x5,    \
                           x6,    \
                           x7,    \
                           x8,    \
                           x9,    \
                           x10,   \
                           x11,   \
                           x12,   \
                           x13,   \
                           x14,   \
                           x15,   \
                           x16,   \
                           x17,   \
                           x18,   \
                           x19,   \
                           x20)   \
    macro(x1);                    \
    macro(x2);                    \
    macro(x3);                    \
    macro(x4);                    \
    macro(x5);                    \
    macro(x6);                    \
    macro(x7);                    \
    macro(x8);                    \
    macro(x9);                    \
    macro(x10);                   \
    macro(x11);                   \
    macro(x12);                   \
    macro(x13);                   \
    macro(x14);                   \
    macro(x15);                   \
    macro(x16);                   \
    macro(x17);                   \
    macro(x18);                   \
    macro(x19);                   \
    macro(x20);
#define FATP_JSON_APPLY_21(macro, \
                           x1,    \
                           x2,    \
                           x3,    \
                           x4,    \
                           x5,    \
                           x6,    \
                           x7,    \
                           x8,    \
                           x9,    \
                           x10,   \
                           x11,   \
                           x12,   \
                           x13,   \
                           x14,   \
                           x15,   \
                           x16,   \
                           x17,   \
                           x18,   \
                           x19,   \
                           x20,   \
                           x21)   \
    macro(x1);                    \
    macro(x2);                    \
    macro(x3);                    \
    macro(x4);                    \
    macro(x5);                    \
    macro(x6);                    \
    macro(x7);                    \
    macro(x8);                    \
    macro(x9);                    \
    macro(x10);                   \
    macro(x11);                   \
    macro(x12);                   \
    macro(x13);                   \
    macro(x14);                   \
    macro(x15);                   \
    macro(x16);                   \
    macro(x17);                   \
    macro(x18);                   \
    macro(x19);                   \
    macro(x20);                   \
    macro(x21);
#define FATP_JSON_APPLY_22(macro, \
                           x1,    \
                           x2,    \
                           x3,    \
                           x4,    \
                           x5,    \
                           x6,    \
                           x7,    \
                           x8,    \
                           x9,    \
                           x10,   \
                           x11,   \
                           x12,   \
                           x13,   \
                           x14,   \
                           x15,   \
                           x16,   \
                           x17,   \
                           x18,   \
                           x19,   \
                           x20,   \
                           x21,   \
                           x22)   \
    macro(x1);                    \
    macro(x2);                    \
    macro(x3);                    \
    macro(x4);                    \
    macro(x5);                    \
    macro(x6);                    \
    macro(x7);                    \
    macro(x8);                    \
    macro(x9);                    \
    macro(x10);                   \
    macro(x11);                   \
    macro(x12);                   \
    macro(x13);                   \
    macro(x14);                   \
    macro(x15);                   \
    macro(x16);                   \
    macro(x17);                   \
    macro(x18);                   \
    macro(x19);                   \
    macro(x20);                   \
    macro(x21);                   \
    macro(x22);
#define FATP_JSON_APPLY_23(macro, \
                           x1,    \
                           x2,    \
                           x3,    \
                           x4,    \
                           x5,    \
                           x6,    \
                           x7,    \
                           x8,    \
                           x9,    \
                           x10,   \
                           x11,   \
                           x12,   \
                           x13,   \
                           x14,   \
                           x15,   \
                           x16,   \
                           x17,   \
                           x18,   \
                           x19,   \
                           x20,   \
                           x21,   \
                           x22,   \
                           x23)   \
    macro(x1);                    \
    macro(x2);                    \
    macro(x3);                    \
    macro(x4);                    \
    macro(x5);                    \
    macro(x6);                    \
    macro(x7);                    \
    macro(x8);                    \
    macro(x9);                    \
    macro(x10);                   \
    macro(x11);                   \
    macro(x12);                   \
    macro(x13);                   \
    macro(x14);                   \
    macro(x15);                   \
    macro(x16);                   \
    macro(x17);                   \
    macro(x18);                   \
    macro(x19);                   \
    macro(x20);                   \
    macro(x21);                   \
    macro(x22);                   \
    macro(x23);
#define FATP_JSON_APPLY_24(macro, \
                           x1,    \
                           x2,    \
                           x3,    \
                           x4,    \
                           x5,    \
                           x6,    \
                           x7,    \
                           x8,    \
                           x9,    \
                           x10,   \
                           x11,   \
                           x12,   \
                           x13,   \
                           x14,   \
                           x15,   \
                           x16,   \
                           x17,   \
                           x18,   \
                           x19,   \
                           x20,   \
                           x21,   \
                           x22,   \
                           x23,   \
                           x24)   \
    macro(x1);                    \
    macro(x2);                    \
    macro(x3);                    \
    macro(x4);                    \
    macro(x5);                    \
    macro(x6);                    \
    macro(x7);                    \
    macro(x8);                    \
    macro(x9);                    \
    macro(x10);                   \
    macro(x11);                   \
    macro(x12);                   \
    macro(x13);                   \
    macro(x14);                   \
    macro(x15);                   \
    macro(x16);                   \
    macro(x17);                   \
    macro(x18);                   \
    macro(x19);                   \
    macro(x20);                   \
    macro(x21);                   \
    macro(x22);                   \
    macro(x23);                   \
    macro(x24);
#define FATP_JSON_APPLY_25(macro, \
                           x1,    \
                           x2,    \
                           x3,    \
                           x4,    \
                           x5,    \
                           x6,    \
                           x7,    \
                           x8,    \
                           x9,    \
                           x10,   \
                           x11,   \
                           x12,   \
                           x13,   \
                           x14,   \
                           x15,   \
                           x16,   \
                           x17,   \
                           x18,   \
                           x19,   \
                           x20,   \
                           x21,   \
                           x22,   \
                           x23,   \
                           x24,   \
                           x25)   \
    macro(x1);                    \
    macro(x2);                    \
    macro(x3);                    \
    macro(x4);                    \
    macro(x5);                    \
    macro(x6);                    \
    macro(x7);                    \
    macro(x8);                    \
    macro(x9);                    \
    macro(x10);                   \
    macro(x11);                   \
    macro(x12);                   \
    macro(x13);                   \
    macro(x14);                   \
    macro(x15);                   \
    macro(x16);                   \
    macro(x17);                   \
    macro(x18);                   \
    macro(x19);                   \
    macro(x20);                   \
    macro(x21);                   \
    macro(x22);                   \
    macro(x23);                   \
    macro(x24);                   \
    macro(x25);
#define FATP_JSON_APPLY_26(macro, \
                           x1,    \
                           x2,    \
                           x3,    \
                           x4,    \
                           x5,    \
                           x6,    \
                           x7,    \
                           x8,    \
                           x9,    \
                           x10,   \
                           x11,   \
                           x12,   \
                           x13,   \
                           x14,   \
                           x15,   \
                           x16,   \
                           x17,   \
                           x18,   \
                           x19,   \
                           x20,   \
                           x21,   \
                           x22,   \
                           x23,   \
                           x24,   \
                           x25,   \
                           x26)   \
    macro(x1);                    \
    macro(x2);                    \
    macro(x3);                    \
    macro(x4);                    \
    macro(x5);                    \
    macro(x6);                    \
    macro(x7);                    \
    macro(x8);                    \
    macro(x9);                    \
    macro(x10);                   \
    macro(x11);                   \
    macro(x12);                   \
    macro(x13);                   \
    macro(x14);                   \
    macro(x15);                   \
    macro(x16);                   \
    macro(x17);                   \
    macro(x18);                   \
    macro(x19);                   \
    macro(x20);                   \
    macro(x21);                   \
    macro(x22);                   \
    macro(x23);                   \
    macro(x24);                   \
    macro(x25);                   \
    macro(x26);
#define FATP_JSON_APPLY_27(macro, \
                           x1,    \
                           x2,    \
                           x3,    \
                           x4,    \
                           x5,    \
                           x6,    \
                           x7,    \
                           x8,    \
                           x9,    \
                           x10,   \
                           x11,   \
                           x12,   \
                           x13,   \
                           x14,   \
                           x15,   \
                           x16,   \
                           x17,   \
                           x18,   \
                           x19,   \
                           x20,   \
                           x21,   \
                           x22,   \
                           x23,   \
                           x24,   \
                           x25,   \
                           x26,   \
                           x27)   \
    macro(x1);                    \
    macro(x2);                    \
    macro(x3);                    \
    macro(x4);                    \
    macro(x5);                    \
    macro(x6);                    \
    macro(x7);                    \
    macro(x8);                    \
    macro(x9);                    \
    macro(x10);                   \
    macro(x11);                   \
    macro(x12);                   \
    macro(x13);                   \
    macro(x14);                   \
    macro(x15);                   \
    macro(x16);                   \
    macro(x17);                   \
    macro(x18);                   \
    macro(x19);                   \
    macro(x20);                   \
    macro(x21);                   \
    macro(x22);                   \
    macro(x23);                   \
    macro(x24);                   \
    macro(x25);                   \
    macro(x26);                   \
    macro(x27);
#define FATP_JSON_APPLY_28(macro, \
                           x1,    \
                           x2,    \
                           x3,    \
                           x4,    \
                           x5,    \
                           x6,    \
                           x7,    \
                           x8,    \
                           x9,    \
                           x10,   \
                           x11,   \
                           x12,   \
                           x13,   \
                           x14,   \
                           x15,   \
                           x16,   \
                           x17,   \
                           x18,   \
                           x19,   \
                           x20,   \
                           x21,   \
                           x22,   \
                           x23,   \
                           x24,   \
                           x25,   \
                           x26,   \
                           x27,   \
                           x28)   \
    macro(x1);                    \
    macro(x2);                    \
    macro(x3);                    \
    macro(x4);                    \
    macro(x5);                    \
    macro(x6);                    \
    macro(x7);                    \
    macro(x8);                    \
    macro(x9);                    \
    macro(x10);                   \
    macro(x11);                   \
    macro(x12);                   \
    macro(x13);                   \
    macro(x14);                   \
    macro(x15);                   \
    macro(x16);                   \
    macro(x17);                   \
    macro(x18);                   \
    macro(x19);                   \
    macro(x20);                   \
    macro(x21);                   \
    macro(x22);                   \
    macro(x23);                   \
    macro(x24);                   \
    macro(x25);                   \
    macro(x26);                   \
    macro(x27);                   \
    macro(x28);
#define FATP_JSON_APPLY_29(macro, \
                           x1,    \
                           x2,    \
                           x3,    \
                           x4,    \
                           x5,    \
                           x6,    \
                           x7,    \
                           x8,    \
                           x9,    \
                           x10,   \
                           x11,   \
                           x12,   \
                           x13,   \
                           x14,   \
                           x15,   \
                           x16,   \
                           x17,   \
                           x18,   \
                           x19,   \
                           x20,   \
                           x21,   \
                           x22,   \
                           x23,   \
                           x24,   \
                           x25,   \
                           x26,   \
                           x27,   \
                           x28,   \
                           x29)   \
    macro(x1);                    \
    macro(x2);                    \
    macro(x3);                    \
    macro(x4);                    \
    macro(x5);                    \
    macro(x6);                    \
    macro(x7);                    \
    macro(x8);                    \
    macro(x9);                    \
    macro(x10);                   \
    macro(x11);                   \
    macro(x12);                   \
    macro(x13);                   \
    macro(x14);                   \
    macro(x15);                   \
    macro(x16);                   \
    macro(x17);                   \
    macro(x18);                   \
    macro(x19);                   \
    macro(x20);                   \
    macro(x21);                   \
    macro(x22);                   \
    macro(x23);                   \
    macro(x24);                   \
    macro(x25);                   \
    macro(x26);                   \
    macro(x27);                   \
    macro(x28);                   \
    macro(x29);
#define FATP_JSON_APPLY_30(macro, \
                           x1,    \
                           x2,    \
                           x3,    \
                           x4,    \
                           x5,    \
                           x6,    \
                           x7,    \
                           x8,    \
                           x9,    \
                           x10,   \
                           x11,   \
                           x12,   \
                           x13,   \
                           x14,   \
                           x15,   \
                           x16,   \
                           x17,   \
                           x18,   \
                           x19,   \
                           x20,   \
                           x21,   \
                           x22,   \
                           x23,   \
                           x24,   \
                           x25,   \
                           x26,   \
                           x27,   \
                           x28,   \
                           x29,   \
                           x30)   \
    macro(x1);                    \
    macro(x2);                    \
    macro(x3);                    \
    macro(x4);                    \
    macro(x5);                    \
    macro(x6);                    \
    macro(x7);                    \
    macro(x8);                    \
    macro(x9);                    \
    macro(x10);                   \
    macro(x11);                   \
    macro(x12);                   \
    macro(x13);                   \
    macro(x14);                   \
    macro(x15);                   \
    macro(x16);                   \
    macro(x17);                   \
    macro(x18);                   \
    macro(x19);                   \
    macro(x20);                   \
    macro(x21);                   \
    macro(x22);                   \
    macro(x23);                   \
    macro(x24);                   \
    macro(x25);                   \
    macro(x26);                   \
    macro(x27);                   \
    macro(x28);                   \
    macro(x29);                   \
    macro(x30);
#define FATP_JSON_APPLY_31(macro, \
                           x1,    \
                           x2,    \
                           x3,    \
                           x4,    \
                           x5,    \
                           x6,    \
                           x7,    \
                           x8,    \
                           x9,    \
                           x10,   \
                           x11,   \
                           x12,   \
                           x13,   \
                           x14,   \
                           x15,   \
                           x16,   \
                           x17,   \
                           x18,   \
                           x19,   \
                           x20,   \
                           x21,   \
                           x22,   \
                           x23,   \
                           x24,   \
                           x25,   \
                           x26,   \
                           x27,   \
                           x28,   \
                           x29,   \
                           x30,   \
                           x31)   \
    macro(x1);                    \
    macro(x2);                    \
    macro(x3);                    \
    macro(x4);                    \
    macro(x5);                    \
    macro(x6);                    \
    macro(x7);                    \
    macro(x8);                    \
    macro(x9);                    \
    macro(x10);                   \
    macro(x11);                   \
    macro(x12);                   \
    macro(x13);                   \
    macro(x14);                   \
    macro(x15);                   \
    macro(x16);                   \
    macro(x17);                   \
    macro(x18);                   \
    macro(x19);                   \
    macro(x20);                   \
    macro(x21);                   \
    macro(x22);                   \
    macro(x23);                   \
    macro(x24);                   \
    macro(x25);                   \
    macro(x26);                   \
    macro(x27);                   \
    macro(x28);                   \
    macro(x29);                   \
    macro(x30);                   \
    macro(x31);
#define FATP_JSON_APPLY_32(macro, \
                           x1,    \
                           x2,    \
                           x3,    \
                           x4,    \
                           x5,    \
                           x6,    \
                           x7,    \
                           x8,    \
                           x9,    \
                           x10,   \
                           x11,   \
                           x12,   \
                           x13,   \
                           x14,   \
                           x15,   \
                           x16,   \
                           x17,   \
                           x18,   \
                           x19,   \
                           x20,   \
                           x21,   \
                           x22,   \
                           x23,   \
                           x24,   \
                           x25,   \
                           x26,   \
                           x27,   \
                           x28,   \
                           x29,   \
                           x30,   \
                           x31,   \
                           x32)   \
    macro(x1);                    \
    macro(x2);                    \
    macro(x3);                    \
    macro(x4);                    \
    macro(x5);                    \
    macro(x6);                    \
    macro(x7);                    \
    macro(x8);                    \
    macro(x9);                    \
    macro(x10);                   \
    macro(x11);                   \
    macro(x12);                   \
    macro(x13);                   \
    macro(x14);                   \
    macro(x15);                   \
    macro(x16);                   \
    macro(x17);                   \
    macro(x18);                   \
    macro(x19);                   \
    macro(x20);                   \
    macro(x21);                   \
    macro(x22);                   \
    macro(x23);                   \
    macro(x24);                   \
    macro(x25);                   \
    macro(x26);                   \
    macro(x27);                   \
    macro(x28);                   \
    macro(x29);                   \
    macro(x30);                   \
    macro(x31);                   \
    macro(x32);
#define FATP_JSON_APPLY_33(macro, \
                           x1,    \
                           x2,    \
                           x3,    \
                           x4,    \
                           x5,    \
                           x6,    \
                           x7,    \
                           x8,    \
                           x9,    \
                           x10,   \
                           x11,   \
                           x12,   \
                           x13,   \
                           x14,   \
                           x15,   \
                           x16,   \
                           x17,   \
                           x18,   \
                           x19,   \
                           x20,   \
                           x21,   \
                           x22,   \
                           x23,   \
                           x24,   \
                           x25,   \
                           x26,   \
                           x27,   \
                           x28,   \
                           x29,   \
                           x30,   \
                           x31,   \
                           x32,   \
                           x33)   \
    macro(x1);                    \
    macro(x2);                    \
    macro(x3);                    \
    macro(x4);                    \
    macro(x5);                    \
    macro(x6);                    \
    macro(x7);                    \
    macro(x8);                    \
    macro(x9);                    \
    macro(x10);                   \
    macro(x11);                   \
    macro(x12);                   \
    macro(x13);                   \
    macro(x14);                   \
    macro(x15);                   \
    macro(x16);                   \
    macro(x17);                   \
    macro(x18);                   \
    macro(x19);                   \
    macro(x20);                   \
    macro(x21);                   \
    macro(x22);                   \
    macro(x23);                   \
    macro(x24);                   \
    macro(x25);                   \
    macro(x26);                   \
    macro(x27);                   \
    macro(x28);                   \
    macro(x29);                   \
    macro(x30);                   \
    macro(x31);                   \
    macro(x32);                   \
    macro(x33);
#define FATP_JSON_APPLY_34(macro, \
                           x1,    \
                           x2,    \
                           x3,    \
                           x4,    \
                           x5,    \
                           x6,    \
                           x7,    \
                           x8,    \
                           x9,    \
                           x10,   \
                           x11,   \
                           x12,   \
                           x13,   \
                           x14,   \
                           x15,   \
                           x16,   \
                           x17,   \
                           x18,   \
                           x19,   \
                           x20,   \
                           x21,   \
                           x22,   \
                           x23,   \
                           x24,   \
                           x25,   \
                           x26,   \
                           x27,   \
                           x28,   \
                           x29,   \
                           x30,   \
                           x31,   \
                           x32,   \
                           x33,   \
                           x34)   \
    macro(x1);                    \
    macro(x2);                    \
    macro(x3);                    \
    macro(x4);                    \
    macro(x5);                    \
    macro(x6);                    \
    macro(x7);                    \
    macro(x8);                    \
    macro(x9);                    \
    macro(x10);                   \
    macro(x11);                   \
    macro(x12);                   \
    macro(x13);                   \
    macro(x14);                   \
    macro(x15);                   \
    macro(x16);                   \
    macro(x17);                   \
    macro(x18);                   \
    macro(x19);                   \
    macro(x20);                   \
    macro(x21);                   \
    macro(x22);                   \
    macro(x23);                   \
    macro(x24);                   \
    macro(x25);                   \
    macro(x26);                   \
    macro(x27);                   \
    macro(x28);                   \
    macro(x29);                   \
    macro(x30);                   \
    macro(x31);                   \
    macro(x32);                   \
    macro(x33);                   \
    macro(x34);
#define FATP_JSON_APPLY_35(macro, \
                           x1,    \
                           x2,    \
                           x3,    \
                           x4,    \
                           x5,    \
                           x6,    \
                           x7,    \
                           x8,    \
                           x9,    \
                           x10,   \
                           x11,   \
                           x12,   \
                           x13,   \
                           x14,   \
                           x15,   \
                           x16,   \
                           x17,   \
                           x18,   \
                           x19,   \
                           x20,   \
                           x21,   \
                           x22,   \
                           x23,   \
                           x24,   \
                           x25,   \
                           x26,   \
                           x27,   \
                           x28,   \
                           x29,   \
                           x30,   \
                           x31,   \
                           x32,   \
                           x33,   \
                           x34,   \
                           x35)   \
    macro(x1);                    \
    macro(x2);                    \
    macro(x3);                    \
    macro(x4);                    \
    macro(x5);                    \
    macro(x6);                    \
    macro(x7);                    \
    macro(x8);                    \
    macro(x9);                    \
    macro(x10);                   \
    macro(x11);                   \
    macro(x12);                   \
    macro(x13);                   \
    macro(x14);                   \
    macro(x15);                   \
    macro(x16);                   \
    macro(x17);                   \
    macro(x18);                   \
    macro(x19);                   \
    macro(x20);                   \
    macro(x21);                   \
    macro(x22);                   \
    macro(x23);                   \
    macro(x24);                   \
    macro(x25);                   \
    macro(x26);                   \
    macro(x27);                   \
    macro(x28);                   \
    macro(x29);                   \
    macro(x30);                   \
    macro(x31);                   \
    macro(x32);                   \
    macro(x33);                   \
    macro(x34);                   \
    macro(x35);
#define FATP_JSON_APPLY_36(macro, \
                           x1,    \
                           x2,    \
                           x3,    \
                           x4,    \
                           x5,    \
                           x6,    \
                           x7,    \
                           x8,    \
                           x9,    \
                           x10,   \
                           x11,   \
                           x12,   \
                           x13,   \
                           x14,   \
                           x15,   \
                           x16,   \
                           x17,   \
                           x18,   \
                           x19,   \
                           x20,   \
                           x21,   \
                           x22,   \
                           x23,   \
                           x24,   \
                           x25,   \
                           x26,   \
                           x27,   \
                           x28,   \
                           x29,   \
                           x30,   \
                           x31,   \
                           x32,   \
                           x33,   \
                           x34,   \
                           x35,   \
                           x36)   \
    macro(x1);                    \
    macro(x2);                    \
    macro(x3);                    \
    macro(x4);                    \
    macro(x5);                    \
    macro(x6);                    \
    macro(x7);                    \
    macro(x8);                    \
    macro(x9);                    \
    macro(x10);                   \
    macro(x11);                   \
    macro(x12);                   \
    macro(x13);                   \
    macro(x14);                   \
    macro(x15);                   \
    macro(x16);                   \
    macro(x17);                   \
    macro(x18);                   \
    macro(x19);                   \
    macro(x20);                   \
    macro(x21);                   \
    macro(x22);                   \
    macro(x23);                   \
    macro(x24);                   \
    macro(x25);                   \
    macro(x26);                   \
    macro(x27);                   \
    macro(x28);                   \
    macro(x29);                   \
    macro(x30);                   \
    macro(x31);                   \
    macro(x32);                   \
    macro(x33);                   \
    macro(x34);                   \
    macro(x35);                   \
    macro(x36);
#define FATP_JSON_APPLY_37(macro, \
                           x1,    \
                           x2,    \
                           x3,    \
                           x4,    \
                           x5,    \
                           x6,    \
                           x7,    \
                           x8,    \
                           x9,    \
                           x10,   \
                           x11,   \
                           x12,   \
                           x13,   \
                           x14,   \
                           x15,   \
                           x16,   \
                           x17,   \
                           x18,   \
                           x19,   \
                           x20,   \
                           x21,   \
                           x22,   \
                           x23,   \
                           x24,   \
                           x25,   \
                           x26,   \
                           x27,   \
                           x28,   \
                           x29,   \
                           x30,   \
                           x31,   \
                           x32,   \
                           x33,   \
                           x34,   \
                           x35,   \
                           x36,   \
                           x37)   \
    macro(x1);                    \
    macro(x2);                    \
    macro(x3);                    \
    macro(x4);                    \
    macro(x5);                    \
    macro(x6);                    \
    macro(x7);                    \
    macro(x8);                    \
    macro(x9);                    \
    macro(x10);                   \
    macro(x11);                   \
    macro(x12);                   \
    macro(x13);                   \
    macro(x14);                   \
    macro(x15);                   \
    macro(x16);                   \
    macro(x17);                   \
    macro(x18);                   \
    macro(x19);                   \
    macro(x20);                   \
    macro(x21);                   \
    macro(x22);                   \
    macro(x23);                   \
    macro(x24);                   \
    macro(x25);                   \
    macro(x26);                   \
    macro(x27);                   \
    macro(x28);                   \
    macro(x29);                   \
    macro(x30);                   \
    macro(x31);                   \
    macro(x32);                   \
    macro(x33);                   \
    macro(x34);                   \
    macro(x35);                   \
    macro(x36);                   \
    macro(x37);
#define FATP_JSON_APPLY_38(macro, \
                           x1,    \
                           x2,    \
                           x3,    \
                           x4,    \
                           x5,    \
                           x6,    \
                           x7,    \
                           x8,    \
                           x9,    \
                           x10,   \
                           x11,   \
                           x12,   \
                           x13,   \
                           x14,   \
                           x15,   \
                           x16,   \
                           x17,   \
                           x18,   \
                           x19,   \
                           x20,   \
                           x21,   \
                           x22,   \
                           x23,   \
                           x24,   \
                           x25,   \
                           x26,   \
                           x27,   \
                           x28,   \
                           x29,   \
                           x30,   \
                           x31,   \
                           x32,   \
                           x33,   \
                           x34,   \
                           x35,   \
                           x36,   \
                           x37,   \
                           x38)   \
    macro(x1);                    \
    macro(x2);                    \
    macro(x3);                    \
    macro(x4);                    \
    macro(x5);                    \
    macro(x6);                    \
    macro(x7);                    \
    macro(x8);                    \
    macro(x9);                    \
    macro(x10);                   \
    macro(x11);                   \
    macro(x12);                   \
    macro(x13);                   \
    macro(x14);                   \
    macro(x15);                   \
    macro(x16);                   \
    macro(x17);                   \
    macro(x18);                   \
    macro(x19);                   \
    macro(x20);                   \
    macro(x21);                   \
    macro(x22);                   \
    macro(x23);                   \
    macro(x24);                   \
    macro(x25);                   \
    macro(x26);                   \
    macro(x27);                   \
    macro(x28);                   \
    macro(x29);                   \
    macro(x30);                   \
    macro(x31);                   \
    macro(x32);                   \
    macro(x33);                   \
    macro(x34);                   \
    macro(x35);                   \
    macro(x36);                   \
    macro(x37);                   \
    macro(x38);
#define FATP_JSON_APPLY_39(macro, \
                           x1,    \
                           x2,    \
                           x3,    \
                           x4,    \
                           x5,    \
                           x6,    \
                           x7,    \
                           x8,    \
                           x9,    \
                           x10,   \
                           x11,   \
                           x12,   \
                           x13,   \
                           x14,   \
                           x15,   \
                           x16,   \
                           x17,   \
                           x18,   \
                           x19,   \
                           x20,   \
                           x21,   \
                           x22,   \
                           x23,   \
                           x24,   \
                           x25,   \
                           x26,   \
                           x27,   \
                           x28,   \
                           x29,   \
                           x30,   \
                           x31,   \
                           x32,   \
                           x33,   \
                           x34,   \
                           x35,   \
                           x36,   \
                           x37,   \
                           x38,   \
                           x39)   \
    macro(x1);                    \
    macro(x2);                    \
    macro(x3);                    \
    macro(x4);                    \
    macro(x5);                    \
    macro(x6);                    \
    macro(x7);                    \
    macro(x8);                    \
    macro(x9);                    \
    macro(x10);                   \
    macro(x11);                   \
    macro(x12);                   \
    macro(x13);                   \
    macro(x14);                   \
    macro(x15);                   \
    macro(x16);                   \
    macro(x17);                   \
    macro(x18);                   \
    macro(x19);                   \
    macro(x20);                   \
    macro(x21);                   \
    macro(x22);                   \
    macro(x23);                   \
    macro(x24);                   \
    macro(x25);                   \
    macro(x26);                   \
    macro(x27);                   \
    macro(x28);                   \
    macro(x29);                   \
    macro(x30);                   \
    macro(x31);                   \
    macro(x32);                   \
    macro(x33);                   \
    macro(x34);                   \
    macro(x35);                   \
    macro(x36);                   \
    macro(x37);                   \
    macro(x38);                   \
    macro(x39);
#define FATP_JSON_APPLY_40(macro, \
                           x1,    \
                           x2,    \
                           x3,    \
                           x4,    \
                           x5,    \
                           x6,    \
                           x7,    \
                           x8,    \
                           x9,    \
                           x10,   \
                           x11,   \
                           x12,   \
                           x13,   \
                           x14,   \
                           x15,   \
                           x16,   \
                           x17,   \
                           x18,   \
                           x19,   \
                           x20,   \
                           x21,   \
                           x22,   \
                           x23,   \
                           x24,   \
                           x25,   \
                           x26,   \
                           x27,   \
                           x28,   \
                           x29,   \
                           x30,   \
                           x31,   \
                           x32,   \
                           x33,   \
                           x34,   \
                           x35,   \
                           x36,   \
                           x37,   \
                           x38,   \
                           x39,   \
                           x40)   \
    macro(x1);                    \
    macro(x2);                    \
    macro(x3);                    \
    macro(x4);                    \
    macro(x5);                    \
    macro(x6);                    \
    macro(x7);                    \
    macro(x8);                    \
    macro(x9);                    \
    macro(x10);                   \
    macro(x11);                   \
    macro(x12);                   \
    macro(x13);                   \
    macro(x14);                   \
    macro(x15);                   \
    macro(x16);                   \
    macro(x17);                   \
    macro(x18);                   \
    macro(x19);                   \
    macro(x20);                   \
    macro(x21);                   \
    macro(x22);                   \
    macro(x23);                   \
    macro(x24);                   \
    macro(x25);                   \
    macro(x26);                   \
    macro(x27);                   \
    macro(x28);                   \
    macro(x29);                   \
    macro(x30);                   \
    macro(x31);                   \
    macro(x32);                   \
    macro(x33);                   \
    macro(x34);                   \
    macro(x35);                   \
    macro(x36);                   \
    macro(x37);                   \
    macro(x38);                   \
    macro(x39);                   \
    macro(x40);
#define FATP_JSON_APPLY_41(macro, \
                           x1,    \
                           x2,    \
                           x3,    \
                           x4,    \
                           x5,    \
                           x6,    \
                           x7,    \
                           x8,    \
                           x9,    \
                           x10,   \
                           x11,   \
                           x12,   \
                           x13,   \
                           x14,   \
                           x15,   \
                           x16,   \
                           x17,   \
                           x18,   \
                           x19,   \
                           x20,   \
                           x21,   \
                           x22,   \
                           x23,   \
                           x24,   \
                           x25,   \
                           x26,   \
                           x27,   \
                           x28,   \
                           x29,   \
                           x30,   \
                           x31,   \
                           x32,   \
                           x33,   \
                           x34,   \
                           x35,   \
                           x36,   \
                           x37,   \
                           x38,   \
                           x39,   \
                           x40,   \
                           x41)   \
    macro(x1);                    \
    macro(x2);                    \
    macro(x3);                    \
    macro(x4);                    \
    macro(x5);                    \
    macro(x6);                    \
    macro(x7);                    \
    macro(x8);                    \
    macro(x9);                    \
    macro(x10);                   \
    macro(x11);                   \
    macro(x12);                   \
    macro(x13);                   \
    macro(x14);                   \
    macro(x15);                   \
    macro(x16);                   \
    macro(x17);                   \
    macro(x18);                   \
    macro(x19);                   \
    macro(x20);                   \
    macro(x21);                   \
    macro(x22);                   \
    macro(x23);                   \
    macro(x24);                   \
    macro(x25);                   \
    macro(x26);                   \
    macro(x27);                   \
    macro(x28);                   \
    macro(x29);                   \
    macro(x30);                   \
    macro(x31);                   \
    macro(x32);                   \
    macro(x33);                   \
    macro(x34);                   \
    macro(x35);                   \
    macro(x36);                   \
    macro(x37);                   \
    macro(x38);                   \
    macro(x39);                   \
    macro(x40);                   \
    macro(x41);
#define FATP_JSON_APPLY_42(macro, \
                           x1,    \
                           x2,    \
                           x3,    \
                           x4,    \
                           x5,    \
                           x6,    \
                           x7,    \
                           x8,    \
                           x9,    \
                           x10,   \
                           x11,   \
                           x12,   \
                           x13,   \
                           x14,   \
                           x15,   \
                           x16,   \
                           x17,   \
                           x18,   \
                           x19,   \
                           x20,   \
                           x21,   \
                           x22,   \
                           x23,   \
                           x24,   \
                           x25,   \
                           x26,   \
                           x27,   \
                           x28,   \
                           x29,   \
                           x30,   \
                           x31,   \
                           x32,   \
                           x33,   \
                           x34,   \
                           x35,   \
                           x36,   \
                           x37,   \
                           x38,   \
                           x39,   \
                           x40,   \
                           x41,   \
                           x42)   \
    macro(x1);                    \
    macro(x2);                    \
    macro(x3);                    \
    macro(x4);                    \
    macro(x5);                    \
    macro(x6);                    \
    macro(x7);                    \
    macro(x8);                    \
    macro(x9);                    \
    macro(x10);                   \
    macro(x11);                   \
    macro(x12);                   \
    macro(x13);                   \
    macro(x14);                   \
    macro(x15);                   \
    macro(x16);                   \
    macro(x17);                   \
    macro(x18);                   \
    macro(x19);                   \
    macro(x20);                   \
    macro(x21);                   \
    macro(x22);                   \
    macro(x23);                   \
    macro(x24);                   \
    macro(x25);                   \
    macro(x26);                   \
    macro(x27);                   \
    macro(x28);                   \
    macro(x29);                   \
    macro(x30);                   \
    macro(x31);                   \
    macro(x32);                   \
    macro(x33);                   \
    macro(x34);                   \
    macro(x35);                   \
    macro(x36);                   \
    macro(x37);                   \
    macro(x38);                   \
    macro(x39);                   \
    macro(x40);                   \
    macro(x41);                   \
    macro(x42);
#define FATP_JSON_APPLY_43(macro, \
                           x1,    \
                           x2,    \
                           x3,    \
                           x4,    \
                           x5,    \
                           x6,    \
                           x7,    \
                           x8,    \
                           x9,    \
                           x10,   \
                           x11,   \
                           x12,   \
                           x13,   \
                           x14,   \
                           x15,   \
                           x16,   \
                           x17,   \
                           x18,   \
                           x19,   \
                           x20,   \
                           x21,   \
                           x22,   \
                           x23,   \
                           x24,   \
                           x25,   \
                           x26,   \
                           x27,   \
                           x28,   \
                           x29,   \
                           x30,   \
                           x31,   \
                           x32,   \
                           x33,   \
                           x34,   \
                           x35,   \
                           x36,   \
                           x37,   \
                           x38,   \
                           x39,   \
                           x40,   \
                           x41,   \
                           x42,   \
                           x43)   \
    macro(x1);                    \
    macro(x2);                    \
    macro(x3);                    \
    macro(x4);                    \
    macro(x5);                    \
    macro(x6);                    \
    macro(x7);                    \
    macro(x8);                    \
    macro(x9);                    \
    macro(x10);                   \
    macro(x11);                   \
    macro(x12);                   \
    macro(x13);                   \
    macro(x14);                   \
    macro(x15);                   \
    macro(x16);                   \
    macro(x17);                   \
    macro(x18);                   \
    macro(x19);                   \
    macro(x20);                   \
    macro(x21);                   \
    macro(x22);                   \
    macro(x23);                   \
    macro(x24);                   \
    macro(x25);                   \
    macro(x26);                   \
    macro(x27);                   \
    macro(x28);                   \
    macro(x29);                   \
    macro(x30);                   \
    macro(x31);                   \
    macro(x32);                   \
    macro(x33);                   \
    macro(x34);                   \
    macro(x35);                   \
    macro(x36);                   \
    macro(x37);                   \
    macro(x38);                   \
    macro(x39);                   \
    macro(x40);                   \
    macro(x41);                   \
    macro(x42);                   \
    macro(x43);
#define FATP_JSON_APPLY_44(macro, \
                           x1,    \
                           x2,    \
                           x3,    \
                           x4,    \
                           x5,    \
                           x6,    \
                           x7,    \
                           x8,    \
                           x9,    \
                           x10,   \
                           x11,   \
                           x12,   \
                           x13,   \
                           x14,   \
                           x15,   \
                           x16,   \
                           x17,   \
                           x18,   \
                           x19,   \
                           x20,   \
                           x21,   \
                           x22,   \
                           x23,   \
                           x24,   \
                           x25,   \
                           x26,   \
                           x27,   \
                           x28,   \
                           x29,   \
                           x30,   \
                           x31,   \
                           x32,   \
                           x33,   \
                           x34,   \
                           x35,   \
                           x36,   \
                           x37,   \
                           x38,   \
                           x39,   \
                           x40,   \
                           x41,   \
                           x42,   \
                           x43,   \
                           x44)   \
    macro(x1);                    \
    macro(x2);                    \
    macro(x3);                    \
    macro(x4);                    \
    macro(x5);                    \
    macro(x6);                    \
    macro(x7);                    \
    macro(x8);                    \
    macro(x9);                    \
    macro(x10);                   \
    macro(x11);                   \
    macro(x12);                   \
    macro(x13);                   \
    macro(x14);                   \
    macro(x15);                   \
    macro(x16);                   \
    macro(x17);                   \
    macro(x18);                   \
    macro(x19);                   \
    macro(x20);                   \
    macro(x21);                   \
    macro(x22);                   \
    macro(x23);                   \
    macro(x24);                   \
    macro(x25);                   \
    macro(x26);                   \
    macro(x27);                   \
    macro(x28);                   \
    macro(x29);                   \
    macro(x30);                   \
    macro(x31);                   \
    macro(x32);                   \
    macro(x33);                   \
    macro(x34);                   \
    macro(x35);                   \
    macro(x36);                   \
    macro(x37);                   \
    macro(x38);                   \
    macro(x39);                   \
    macro(x40);                   \
    macro(x41);                   \
    macro(x42);                   \
    macro(x43);                   \
    macro(x44);
#define FATP_JSON_APPLY_45(macro, \
                           x1,    \
                           x2,    \
                           x3,    \
                           x4,    \
                           x5,    \
                           x6,    \
                           x7,    \
                           x8,    \
                           x9,    \
                           x10,   \
                           x11,   \
                           x12,   \
                           x13,   \
                           x14,   \
                           x15,   \
                           x16,   \
                           x17,   \
                           x18,   \
                           x19,   \
                           x20,   \
                           x21,   \
                           x22,   \
                           x23,   \
                           x24,   \
                           x25,   \
                           x26,   \
                           x27,   \
                           x28,   \
                           x29,   \
                           x30,   \
                           x31,   \
                           x32,   \
                           x33,   \
                           x34,   \
                           x35,   \
                           x36,   \
                           x37,   \
                           x38,   \
                           x39,   \
                           x40,   \
                           x41,   \
                           x42,   \
                           x43,   \
                           x44,   \
                           x45)   \
    macro(x1);                    \
    macro(x2);                    \
    macro(x3);                    \
    macro(x4);                    \
    macro(x5);                    \
    macro(x6);                    \
    macro(x7);                    \
    macro(x8);                    \
    macro(x9);                    \
    macro(x10);                   \
    macro(x11);                   \
    macro(x12);                   \
    macro(x13);                   \
    macro(x14);                   \
    macro(x15);                   \
    macro(x16);                   \
    macro(x17);                   \
    macro(x18);                   \
    macro(x19);                   \
    macro(x20);                   \
    macro(x21);                   \
    macro(x22);                   \
    macro(x23);                   \
    macro(x24);                   \
    macro(x25);                   \
    macro(x26);                   \
    macro(x27);                   \
    macro(x28);                   \
    macro(x29);                   \
    macro(x30);                   \
    macro(x31);                   \
    macro(x32);                   \
    macro(x33);                   \
    macro(x34);                   \
    macro(x35);                   \
    macro(x36);                   \
    macro(x37);                   \
    macro(x38);                   \
    macro(x39);                   \
    macro(x40);                   \
    macro(x41);                   \
    macro(x42);                   \
    macro(x43);                   \
    macro(x44);                   \
    macro(x45);
#define FATP_JSON_APPLY_46(macro, \
                           x1,    \
                           x2,    \
                           x3,    \
                           x4,    \
                           x5,    \
                           x6,    \
                           x7,    \
                           x8,    \
                           x9,    \
                           x10,   \
                           x11,   \
                           x12,   \
                           x13,   \
                           x14,   \
                           x15,   \
                           x16,   \
                           x17,   \
                           x18,   \
                           x19,   \
                           x20,   \
                           x21,   \
                           x22,   \
                           x23,   \
                           x24,   \
                           x25,   \
                           x26,   \
                           x27,   \
                           x28,   \
                           x29,   \
                           x30,   \
                           x31,   \
                           x32,   \
                           x33,   \
                           x34,   \
                           x35,   \
                           x36,   \
                           x37,   \
                           x38,   \
                           x39,   \
                           x40,   \
                           x41,   \
                           x42,   \
                           x43,   \
                           x44,   \
                           x45,   \
                           x46)   \
    macro(x1);                    \
    macro(x2);                    \
    macro(x3);                    \
    macro(x4);                    \
    macro(x5);                    \
    macro(x6);                    \
    macro(x7);                    \
    macro(x8);                    \
    macro(x9);                    \
    macro(x10);                   \
    macro(x11);                   \
    macro(x12);                   \
    macro(x13);                   \
    macro(x14);                   \
    macro(x15);                   \
    macro(x16);                   \
    macro(x17);                   \
    macro(x18);                   \
    macro(x19);                   \
    macro(x20);                   \
    macro(x21);                   \
    macro(x22);                   \
    macro(x23);                   \
    macro(x24);                   \
    macro(x25);                   \
    macro(x26);                   \
    macro(x27);                   \
    macro(x28);                   \
    macro(x29);                   \
    macro(x30);                   \
    macro(x31);                   \
    macro(x32);                   \
    macro(x33);                   \
    macro(x34);                   \
    macro(x35);                   \
    macro(x36);                   \
    macro(x37);                   \
    macro(x38);                   \
    macro(x39);                   \
    macro(x40);                   \
    macro(x41);                   \
    macro(x42);                   \
    macro(x43);                   \
    macro(x44);                   \
    macro(x45);                   \
    macro(x46);
#define FATP_JSON_APPLY_47(macro, \
                           x1,    \
                           x2,    \
                           x3,    \
                           x4,    \
                           x5,    \
                           x6,    \
                           x7,    \
                           x8,    \
                           x9,    \
                           x10,   \
                           x11,   \
                           x12,   \
                           x13,   \
                           x14,   \
                           x15,   \
                           x16,   \
                           x17,   \
                           x18,   \
                           x19,   \
                           x20,   \
                           x21,   \
                           x22,   \
                           x23,   \
                           x24,   \
                           x25,   \
                           x26,   \
                           x27,   \
                           x28,   \
                           x29,   \
                           x30,   \
                           x31,   \
                           x32,   \
                           x33,   \
                           x34,   \
                           x35,   \
                           x36,   \
                           x37,   \
                           x38,   \
                           x39,   \
                           x40,   \
                           x41,   \
                           x42,   \
                           x43,   \
                           x44,   \
                           x45,   \
                           x46,   \
                           x47)   \
    macro(x1);                    \
    macro(x2);                    \
    macro(x3);                    \
    macro(x4);                    \
    macro(x5);                    \
    macro(x6);                    \
    macro(x7);                    \
    macro(x8);                    \
    macro(x9);                    \
    macro(x10);                   \
    macro(x11);                   \
    macro(x12);                   \
    macro(x13);                   \
    macro(x14);                   \
    macro(x15);                   \
    macro(x16);                   \
    macro(x17);                   \
    macro(x18);                   \
    macro(x19);                   \
    macro(x20);                   \
    macro(x21);                   \
    macro(x22);                   \
    macro(x23);                   \
    macro(x24);                   \
    macro(x25);                   \
    macro(x26);                   \
    macro(x27);                   \
    macro(x28);                   \
    macro(x29);                   \
    macro(x30);                   \
    macro(x31);                   \
    macro(x32);                   \
    macro(x33);                   \
    macro(x34);                   \
    macro(x35);                   \
    macro(x36);                   \
    macro(x37);                   \
    macro(x38);                   \
    macro(x39);                   \
    macro(x40);                   \
    macro(x41);                   \
    macro(x42);                   \
    macro(x43);                   \
    macro(x44);                   \
    macro(x45);                   \
    macro(x46);                   \
    macro(x47);
#define FATP_JSON_APPLY_48(macro, \
                           x1,    \
                           x2,    \
                           x3,    \
                           x4,    \
                           x5,    \
                           x6,    \
                           x7,    \
                           x8,    \
                           x9,    \
                           x10,   \
                           x11,   \
                           x12,   \
                           x13,   \
                           x14,   \
                           x15,   \
                           x16,   \
                           x17,   \
                           x18,   \
                           x19,   \
                           x20,   \
                           x21,   \
                           x22,   \
                           x23,   \
                           x24,   \
                           x25,   \
                           x26,   \
                           x27,   \
                           x28,   \
                           x29,   \
                           x30,   \
                           x31,   \
                           x32,   \
                           x33,   \
                           x34,   \
                           x35,   \
                           x36,   \
                           x37,   \
                           x38,   \
                           x39,   \
                           x40,   \
                           x41,   \
                           x42,   \
                           x43,   \
                           x44,   \
                           x45,   \
                           x46,   \
                           x47,   \
                           x48)   \
    macro(x1);                    \
    macro(x2);                    \
    macro(x3);                    \
    macro(x4);                    \
    macro(x5);                    \
    macro(x6);                    \
    macro(x7);                    \
    macro(x8);                    \
    macro(x9);                    \
    macro(x10);                   \
    macro(x11);                   \
    macro(x12);                   \
    macro(x13);                   \
    macro(x14);                   \
    macro(x15);                   \
    macro(x16);                   \
    macro(x17);                   \
    macro(x18);                   \
    macro(x19);                   \
    macro(x20);                   \
    macro(x21);                   \
    macro(x22);                   \
    macro(x23);                   \
    macro(x24);                   \
    macro(x25);                   \
    macro(x26);                   \
    macro(x27);                   \
    macro(x28);                   \
    macro(x29);                   \
    macro(x30);                   \
    macro(x31);                   \
    macro(x32);                   \
    macro(x33);                   \
    macro(x34);                   \
    macro(x35);                   \
    macro(x36);                   \
    macro(x37);                   \
    macro(x38);                   \
    macro(x39);                   \
    macro(x40);                   \
    macro(x41);                   \
    macro(x42);                   \
    macro(x43);                   \
    macro(x44);                   \
    macro(x45);                   \
    macro(x46);                   \
    macro(x47);                   \
    macro(x48);
#define FATP_JSON_APPLY_49(macro, \
                           x1,    \
                           x2,    \
                           x3,    \
                           x4,    \
                           x5,    \
                           x6,    \
                           x7,    \
                           x8,    \
                           x9,    \
                           x10,   \
                           x11,   \
                           x12,   \
                           x13,   \
                           x14,   \
                           x15,   \
                           x16,   \
                           x17,   \
                           x18,   \
                           x19,   \
                           x20,   \
                           x21,   \
                           x22,   \
                           x23,   \
                           x24,   \
                           x25,   \
                           x26,   \
                           x27,   \
                           x28,   \
                           x29,   \
                           x30,   \
                           x31,   \
                           x32,   \
                           x33,   \
                           x34,   \
                           x35,   \
                           x36,   \
                           x37,   \
                           x38,   \
                           x39,   \
                           x40,   \
                           x41,   \
                           x42,   \
                           x43,   \
                           x44,   \
                           x45,   \
                           x46,   \
                           x47,   \
                           x48,   \
                           x49)   \
    macro(x1);                    \
    macro(x2);                    \
    macro(x3);                    \
    macro(x4);                    \
    macro(x5);                    \
    macro(x6);                    \
    macro(x7);                    \
    macro(x8);                    \
    macro(x9);                    \
    macro(x10);                   \
    macro(x11);                   \
    macro(x12);                   \
    macro(x13);                   \
    macro(x14);                   \
    macro(x15);                   \
    macro(x16);                   \
    macro(x17);                   \
    macro(x18);                   \
    macro(x19);                   \
    macro(x20);                   \
    macro(x21);                   \
    macro(x22);                   \
    macro(x23);                   \
    macro(x24);                   \
    macro(x25);                   \
    macro(x26);                   \
    macro(x27);                   \
    macro(x28);                   \
    macro(x29);                   \
    macro(x30);                   \
    macro(x31);                   \
    macro(x32);                   \
    macro(x33);                   \
    macro(x34);                   \
    macro(x35);                   \
    macro(x36);                   \
    macro(x37);                   \
    macro(x38);                   \
    macro(x39);                   \
    macro(x40);                   \
    macro(x41);                   \
    macro(x42);                   \
    macro(x43);                   \
    macro(x44);                   \
    macro(x45);                   \
    macro(x46);                   \
    macro(x47);                   \
    macro(x48);                   \
    macro(x49);
#define FATP_JSON_APPLY_50(macro, \
                           x1,    \
                           x2,    \
                           x3,    \
                           x4,    \
                           x5,    \
                           x6,    \
                           x7,    \
                           x8,    \
                           x9,    \
                           x10,   \
                           x11,   \
                           x12,   \
                           x13,   \
                           x14,   \
                           x15,   \
                           x16,   \
                           x17,   \
                           x18,   \
                           x19,   \
                           x20,   \
                           x21,   \
                           x22,   \
                           x23,   \
                           x24,   \
                           x25,   \
                           x26,   \
                           x27,   \
                           x28,   \
                           x29,   \
                           x30,   \
                           x31,   \
                           x32,   \
                           x33,   \
                           x34,   \
                           x35,   \
                           x36,   \
                           x37,   \
                           x38,   \
                           x39,   \
                           x40,   \
                           x41,   \
                           x42,   \
                           x43,   \
                           x44,   \
                           x45,   \
                           x46,   \
                           x47,   \
                           x48,   \
                           x49,   \
                           x50)   \
    macro(x1);                    \
    macro(x2);                    \
    macro(x3);                    \
    macro(x4);                    \
    macro(x5);                    \
    macro(x6);                    \
    macro(x7);                    \
    macro(x8);                    \
    macro(x9);                    \
    macro(x10);                   \
    macro(x11);                   \
    macro(x12);                   \
    macro(x13);                   \
    macro(x14);                   \
    macro(x15);                   \
    macro(x16);                   \
    macro(x17);                   \
    macro(x18);                   \
    macro(x19);                   \
    macro(x20);                   \
    macro(x21);                   \
    macro(x22);                   \
    macro(x23);                   \
    macro(x24);                   \
    macro(x25);                   \
    macro(x26);                   \
    macro(x27);                   \
    macro(x28);                   \
    macro(x29);                   \
    macro(x30);                   \
    macro(x31);                   \
    macro(x32);                   \
    macro(x33);                   \
    macro(x34);                   \
    macro(x35);                   \
    macro(x36);                   \
    macro(x37);                   \
    macro(x38);                   \
    macro(x39);                   \
    macro(x40);                   \
    macro(x41);                   \
    macro(x42);                   \
    macro(x43);                   \
    macro(x44);                   \
    macro(x45);                   \
    macro(x46);                   \
    macro(x47);                   \
    macro(x48);                   \
    macro(x49);                   \
    macro(x50);

#define FATP_JSON_APPLY_51(...)                                                     \
    static_assert(false,                                                            \
                  "FATP_JSON_DEFINE_TYPE_* macros support a maximum of 50 fields. " \
                  "Your struct has 51 or more fields. Solutions: "                  \
                  "1) Use nested structs to group related fields, "                 \
                  "2) Write custom to_json/from_json functions, "                   \
                  "3) Split into multiple smaller structs.")

#define FATP_JSON_FOR_EACH(macro, ...) \
    FATP_JSON_EXPAND(FATP_JSON_CAT(FATP_JSON_APPLY_, FATP_JSON_ARG_COUNT(__VA_ARGS__))(macro, __VA_ARGS__))

/**
 * @def FATP_JSON_DEFINE_TYPE_NON_INTRUSIVE
 * @brief Define JSON serialization for a struct/class (non-intrusive)
 *
 * @details This macro generates to_json() and from_json() functions for a user-defined
 * type without modifying the type itself. The functions are defined in the same namespace
 * as the type and are found via ADL (Argument Dependent Lookup).
 *
 * This is the most common macro for adding JSON support to your types.
 *
 * @section usage Usage
 * @code{.cpp}
 * struct Config {
 *     int port;
 *     std::string host;
 *     bool enabled;
 * };
 *
 * // Define serialization (outside the struct, same namespace)
 * FATP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Config, port, host, enabled)
 *
 * // Now you can:
 * Config cfg{8080, "localhost", true};
 * JsonValue j = json_encode(cfg);           // Serialize
 * Config loaded = json_decode<Config>(j);   // Deserialize
 * @endcode
 *
 * @section behavior Behavior
 * - **Serialization**: Creates a JsonObject with field names as keys
 * - **Deserialization**: Expects a JsonObject with matching field names
 * - **Required Fields**: All fields must be present in JSON (throws if missing)
 * - **Extra Fields**: Extra JSON keys are ignored
 * - **Type Safety**: Full compile-time type checking via from_json overloads
 *
 * @section nested Nested Types
 * @code{.cpp}
 * struct Address {
 *     std::string city;
 *     int zip;
 * };
 * FATP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Address, city, zip)
 *
 * struct Person {
 *     std::string name;
 *     Address address;  // Nested struct
 * };
 * FATP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Person, name, address)
 * @endcode
 *
 * @param Type The type name (struct or class)
 * @param ... Field names (up to 50 fields supported)
 *
 * @note Place macro call in the same namespace as Type (for ADL)
 * @note All fields must be public or the macro must be a friend
 * @note For private fields, use FATP_JSON_DEFINE_TYPE_INTRUSIVE instead
 *
 * @warning Fields must be listed in the exact order you want them serialized
 *
 * @see FATP_JSON_DEFINE_TYPE_INTRUSIVE
 * @see FATP_JSON_DEFINE_TYPE_OPTIONAL
 * @see to_json
 * @see from_json
 */
#define FATP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Type, ...)                             \
    [[maybe_unused]] inline void to_json(fat_p::JsonValue& j, const Type& value)   \
    {                                                                              \
        fat_p::JsonObject obj;                                                     \
        FATP_JSON_FOR_EACH(FATP_JSON_TO_FIELD, __VA_ARGS__)                        \
        j = std::move(obj);                                                        \
    }                                                                              \
    [[maybe_unused]] inline void from_json(const fat_p::JsonValue& j, Type& value) \
    {                                                                              \
        FATP_JSON_ENFORCE(j.is_object(),                                           \
                          "JSON type mismatch",                                    \
                          "expected",                                              \
                          "object",                                                \
                          "got",                                                   \
                          fat_p::json_detail::typeName(j));                       \
        const auto& obj = std::get<fat_p::JsonObject>(j);                          \
        FATP_JSON_FOR_EACH(FATP_JSON_FROM_FIELD, __VA_ARGS__)                      \
    }

/**
 * @def FATP_JSON_DEFINE_TYPE_OPTIONAL
 * @brief Define JSON serialization with optional fields
 *
 * @details Similar to FATP_JSON_DEFINE_TYPE_NON_INTRUSIVE, but makes all fields optional
 * during deserialization. Missing fields in the JSON are silently skipped rather than
 * causing an error.
 *
 * This is useful for configuration files where not all fields need to be present, or
 * when dealing with partial updates.
 *
 * @section usage Usage
 * @code{.cpp}
 * struct Settings {
 *     int port = 8080;           // Default value
 *     std::string host = "localhost";
 *     bool debug = false;
 * };
 *
 * // All fields are optional during deserialization
 * FATP_JSON_DEFINE_TYPE_OPTIONAL(Settings, port, host, debug)
 *
 * // This JSON is valid (only some fields present)
 * Settings s = json_decode<Settings>(parse_json(R"({"port": 9000})"));
 * // s.port == 9000, s.host == "localhost", s.debug == false
 * @endcode
 *
 * @section best_practices Best Practices
 * - Always provide default values for all fields
 * - Consider using std::optional for truly optional data
 * - Document which fields are required vs optional
 *
 * @param Type The type name (struct or class)
 * @param ... Field names (up to 50 fields supported)
 *
 * @note All fields must have default-constructible values or default member initializers
 * @note Serialization still writes all fields (regardless of value)
 *
 * @see FATP_JSON_DEFINE_TYPE_NON_INTRUSIVE
 * @see std::optional
 */
#define FATP_JSON_DEFINE_TYPE_OPTIONAL(Type, ...)                                  \
    [[maybe_unused]] inline void to_json(fat_p::JsonValue& j, const Type& value)   \
    {                                                                              \
        fat_p::JsonObject obj;                                                     \
        FATP_JSON_FOR_EACH(FATP_JSON_TO_FIELD, __VA_ARGS__)                        \
        j = std::move(obj);                                                        \
    }                                                                              \
    [[maybe_unused]] inline void from_json(const fat_p::JsonValue& j, Type& value) \
    {                                                                              \
        FATP_JSON_ENFORCE(j.is_object(),                                           \
                          "JSON type mismatch",                                    \
                          "expected",                                              \
                          "object",                                                \
                          "got",                                                   \
                          fat_p::json_detail::typeName(j));                       \
        const auto& obj = std::get<fat_p::JsonObject>(j);                          \
        FATP_JSON_FOR_EACH(FATP_JSON_FROM_FIELD_OPT, __VA_ARGS__)                  \
    }

/**
 * @def FATP_JSON_DEFINE_TYPE_INTRUSIVE
 * @brief Define JSON serialization inside a class (intrusive)
 *
 * @details This macro generates to_json() and from_json() as friend functions declared
 * inside the class definition. This allows access to private/protected members.
 *
 * Use this when you need to serialize private fields or when you want to keep
 * serialization logic close to the type definition.
 *
 * @section usage Usage
 * @code{.cpp}
 * class User {
 * private:
 *     std::string mUsername;
 *     std::string password_hash_;
 *     int user_id_;
 *
 * public:
 *     User() = default;
 *     User(std::string name, std::string hash, int id)
 *         : mUsername(std::move(name))
 *         , password_hash_(std::move(hash))
 *         , user_id_(id) {}
 *
 *     // Define serialization with access to private members
 *     FATP_JSON_DEFINE_TYPE_INTRUSIVE(User, mUsername, password_hash_, user_id_)
 * };
 *
 * // Usage is the same
 * User user{"alice", "hash123", 42};
 * JsonValue j = json_encode(user);
 * User loaded = json_decode<User>(j);
 * @endcode
 *
 * @section friend_functions Friend Functions
 * The macro generates friend functions that have access to private/protected members:
 * - `friend void to_json(JsonValue& j, const Type& value)`
 * - `friend void from_json(const JsonValue& j, Type& value)`
 *
 * @param Type The type name (usually use 'User' not 'class User')
 * @param ... Field names (up to 50 fields supported)
 *
 * @note Place macro call inside the class definition (public/private doesn't matter)
 * @note Friend functions are defined inline (no separate .cpp file needed)
 * @note The generated functions can access all private/protected members
 *
 * @warning Type must be default-constructible for deserialization
 *
 * @see FATP_JSON_DEFINE_TYPE_NON_INTRUSIVE
 * @see FATP_JSON_DEFINE_TYPE_OPTIONAL
 */
#define FATP_JSON_DEFINE_TYPE_INTRUSIVE(Type, ...)                                 \
    [[maybe_unused]] friend void to_json(fat_p::JsonValue& j, const Type& value)   \
    {                                                                              \
        fat_p::JsonObject obj;                                                     \
        FATP_JSON_FOR_EACH(FATP_JSON_TO_FIELD, __VA_ARGS__)                        \
        j = std::move(obj);                                                        \
    }                                                                              \
    [[maybe_unused]] friend void from_json(const fat_p::JsonValue& j, Type& value) \
    {                                                                              \
        FATP_JSON_ENFORCE(j.is_object(),                                           \
                          "JSON type mismatch",                                    \
                          "expected",                                              \
                          "object",                                                \
                          "got",                                                   \
                          fat_p::json_detail::typeName(j));                       \
        const auto& obj = std::get<fat_p::JsonObject>(j);                          \
        FATP_JSON_FOR_EACH(FATP_JSON_FROM_FIELD, __VA_ARGS__)                      \
    }

namespace json_detail
{

/**
 * @brief Advance position past whitespace and optional comments
 *
 * @tparam Policy JSON policy (controls comment support via allow_comments)
 * @param s Input string view
 * @param pos Current position (modified in-place)
 *
 * @details When Policy::allow_comments is true, also skips:
 * - Line comments: // to end of line
 * - Block comments: slash-star to star-slash
 */
template <typename Policy = StandardJsonPolicy>
inline void skip_whitespace(std::string_view s, size_t& pos) noexcept
{
    while (pos < s.size())
    {
        if (std::isspace(static_cast<unsigned char>(s[pos])))
        {
            ++pos;
            continue;
        }

        // Comment handling only when policy enables it (compile-time branch)
        if constexpr (Policy::allow_comments)
        {
            // Line comment: skip from // to newline
            if (s.size() - pos >= 2 && s[pos] == '/' && s[pos + 1] == '/')
            {
                pos += 2;
                while (pos < s.size() && s[pos] != '\n')
                {
                    ++pos;
                }
                continue;
            }

            // Block comment: skip from /* to */
            if (s.size() - pos >= 2 && s[pos] == '/' && s[pos + 1] == '*')
            {
                pos += 2;
                while (pos + 1 < s.size() && !(s[pos] == '*' && s[pos + 1] == '/'))
                {
                    ++pos;
                }
                // Skip closing */ if found (otherwise we're at EOF, which is fine)
                if (pos + 1 < s.size())
                {
                    pos += 2;
                }
                continue;
            }
        }

        // Non-whitespace, non-comment character - stop here
        break;
    }
}

/**
 * @brief Parse JSON string literal with escape sequence handling
 *
 * @param s Input string view (must start with opening quote at pos)
 * @param pos Current position (modified to point past closing quote)
 * @return std::string Unescaped string content
 *
 * @throws std::runtime_error on unterminated string or invalid escape
 *
 * @details Handles all JSON escape sequences:
 * - Basic: \", \\, \/, \b, \f, \n, \r, \t
 * - Unicode: \uXXXX (BMP) and \uXXXX\uXXXX (surrogate pairs)
 */
inline std::string parse_string(std::string_view s, size_t& pos)
{
    FATP_JSON_ENFORCE(pos < s.size() && s[pos] == '"', "JSON parse error: expected mString_LIT_1__position", pos);
    ++pos; // Skip opening quote

    std::string res;
    res.reserve(64); // Reasonable default to reduce reallocations

    while (pos < s.size() && s[pos] != '"')
    {
        if (s[pos] == '\\')
        {
            ++pos; // Skip backslash
            FATP_JSON_ENFORCE(pos < s.size(), "JSON parse error: invalid escape sequence", "position", pos);

            switch (s[pos])
            {
                    // Simple escape sequences - direct character mapping
                case '"':
                    res += '"';
                    break;
                case '\\':
                    res += '\\';
                    break;
                case '/':
                    res += '/';
                    break;
                case 'b':
                    res += '\b';
                    break;
                case 'f':
                    res += '\f';
                    break;
                case 'n':
                    res += '\n';
                    break;
                case 'r':
                    res += '\r';
                    break;
                case 't':
                    res += '\t';
                    break;

                    // Unicode escape: \uXXXX
                case 'u':
                {
                    FATP_JSON_ENFORCE(pos + 5 <= s.size(), "JSON parse error: invalid unicode escape", "position", pos);
                    std::string hex = std::string(s.substr(pos + 1, 4));
                    uint32_t codepoint;
                    try
                    {
                        codepoint = static_cast<uint32_t>(std::stoul(hex, nullptr, 16));
                    }
                    catch (...)
                    {
                        FATP_JSON_ENFORCE(false, "JSON parse error: invalid unicode hex", "position", pos, "hex", hex);
                    }
                    pos += 4;

                    // Surrogate pair handling for characters outside BMP (U+10000+)
                    // High surrogate: D800-DBFF, must be followed by low surrogate
                    if (codepoint >= 0xD800 && codepoint <= 0xDBFF)
                    {
                        FATP_JSON_ENFORCE(pos + 6 < s.size() && s[pos + 1] == '\\' && s[pos + 2] == 'u',
                                          "JSON parse error: incomplete surrogate pair",
                                          "position",
                                          pos);
                        std::string low_hex = std::string(s.substr(pos + 3, 4));
                        uint32_t low_surrogate;
                        try
                        {
                            low_surrogate = static_cast<uint32_t>(std::stoul(low_hex, nullptr, 16));
                        }
                        catch (...)
                        {
                            FATP_JSON_ENFORCE(false,
                                              "JSON parse error: invalid low surrogate",
                                              "position",
                                              pos,
                                              "hex",
                                              low_hex);
                        }
                        // Low surrogate must be in DC00-DFFF range
                        FATP_JSON_ENFORCE(low_surrogate >= 0xDC00 && low_surrogate <= 0xDFFF,
                                          "JSON parse error: invalid low surrogate value",
                                          "position",
                                          pos,
                                          "value",
                                          low_surrogate);
                        pos += 6;
                        // Decode surrogate pair: ((high - D800) << 10) + (low - DC00) + 10000
                        codepoint = 0x10000 + ((codepoint & 0x3FF) << 10) + (low_surrogate & 0x3FF);
                    }
                    // Lone low surrogate is invalid
                    else if (codepoint >= 0xDC00 && codepoint <= 0xDFFF)
                    {
                        FATP_JSON_ENFORCE(false,
                                          "JSON parse error: unexpected low surrogate",
                                          "position",
                                          pos,
                                          "codepoint",
                                          codepoint);
                    }

                    // Encode codepoint as UTF-8
                    // 1-byte: 0xxxxxxx (U+0000 to U+007F)
                    if (codepoint <= 0x7F)
                    {
                        res += static_cast<char>(codepoint);
                    }
                    // 2-byte: 110xxxxx 10xxxxxx (U+0080 to U+07FF)
                    else if (codepoint <= 0x7FF)
                    {
                        res += static_cast<char>(0xC0 | (codepoint >> 6));
                        res += static_cast<char>(0x80 | (codepoint & 0x3F));
                    }
                    // 3-byte: 1110xxxx 10xxxxxx 10xxxxxx (U+0800 to U+FFFF)
                    else if (codepoint <= 0xFFFF)
                    {
                        res += static_cast<char>(0xE0 | (codepoint >> 12));
                        res += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                        res += static_cast<char>(0x80 | (codepoint & 0x3F));
                    }
                    // 4-byte: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx (U+10000 to U+10FFFF)
                    else if (codepoint <= 0x10FFFF)
                    {
                        res += static_cast<char>(0xF0 | (codepoint >> 18));
                        res += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
                        res += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                        res += static_cast<char>(0x80 | (codepoint & 0x3F));
                    }
                    else
                    {
                        FATP_JSON_ENFORCE(false,
                                          "JSON parse error: invalid unicode codepoint",
                                          "position",
                                          pos,
                                          "codepoint",
                                          codepoint);
                    }
                    break;
                }
                default:
                    FATP_JSON_ENFORCE(false,
                                      "JSON parse error: invalid escape character",
                                      "position",
                                      pos,
                                      "character",
                                      s[pos]);
            }
        }
        else
        {
            // Regular character - copy as-is
            res += s[pos];
        }
        ++pos;
    }

    FATP_JSON_ENFORCE(pos < s.size() && s[pos] == '"', "JSON parse error: unterminated mString_LIT_1__position", pos);
    ++pos; // Skip closing quote
    return res;
}

/**
 * @brief Parse JSON number (integer or floating-point)
 *
 * @tparam Policy JSON policy (controls NaN/Infinity support via allow_nan_inf)
 * @param s Input string view
 * @param pos Current position (modified to point past number)
 * @return JsonValue containing int64_t or double
 *
 * @throws std::runtime_error on invalid number format
 *
 * @details Parsing strategy:
 * - Uses std::from_chars for locale-independent, zero-allocation parsing
 * - Prefers int64_t when number has no decimal or exponent
 * - Falls back to double for fractional values or on integer overflow
 * - Optionally accepts NaN, Infinity, -Infinity when policy allows
 */
template <typename Policy = StandardJsonPolicy>
inline JsonValue parse_number(std::string_view s, size_t& pos)
{
    size_t start = pos;

    // Non-standard extensions: NaN and Infinity (only when policy allows)
    if (s.substr(pos, 3) == "NaN")
    {
        FATP_JSON_ENFORCE(Policy::allow_nan_inf,
                          "JSON parse error: NaN is not valid JSON (use allow_nan_inf policy to enable)",
                          "position",
                          pos);
        pos += 3;
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (s.substr(pos, 8) == "Infinity")
    {
        FATP_JSON_ENFORCE(Policy::allow_nan_inf,
                          "JSON parse error: Infinity is not valid JSON (use allow_nan_inf policy to enable)",
                          "position",
                          pos);
        pos += 8;
        return std::numeric_limits<double>::infinity();
    }
    if (s.substr(pos, 9) == "-Infinity")
    {
        FATP_JSON_ENFORCE(Policy::allow_nan_inf,
                          "JSON parse error: -Infinity is not valid JSON (use allow_nan_inf policy to enable)",
                          "position",
                          pos);
        pos += 9;
        return -std::numeric_limits<double>::infinity();
    }

    // Standard JSON number: must start with digit or minus
    FATP_JSON_ENFORCE(pos < s.size() && (std::isdigit(s[pos]) || s[pos] == '-'),
                      "JSON parse error: invalid number",
                      "position",
                      pos);

    // Validate minus is followed by digit
    if (s[pos] == '-')
    {
        ++pos;
        FATP_JSON_ENFORCE(pos < s.size() && std::isdigit(s[pos]),
                          "JSON parse error: invalid number after '-'",
                          "position",
                          pos);
    }

    // RFC 8259: Leading zeros are not allowed (except for 0 itself or 0.xxx)
    // Valid: 0, 0.5, -0, -0.5
    // Invalid: 01, 007, -01, 00
    if (s[pos] == '0' && pos + 1 < s.size() && std::isdigit(s[pos + 1]))
    {
        FATP_JSON_ENFORCE(false, "JSON parse error: leading zeros are not allowed in numbers", "position", pos);
    }

    // RFC 8259 compliant number scanning with validation
    // number = [ minus ] int [ frac ] [ exp ]
    // int    = zero / ( digit1-9 *DIGIT )
    // frac   = decimal-point 1*DIGIT
    // exp    = e [ minus / plus ] 1*DIGIT
    bool has_decimal = false;
    bool has_exponent = false;
    size_t scan_pos = pos;

    // Scan integer part (we already validated first digit exists)
    while (scan_pos < s.size() && std::isdigit(s[scan_pos]))
    {
        ++scan_pos;
    }

    // Optional fractional part
    if (scan_pos < s.size() && s[scan_pos] == '.')
    {
        has_decimal = true;
        ++scan_pos;
        // RFC 8259: frac = decimal-point 1*DIGIT (at least one digit required)
        FATP_JSON_ENFORCE(scan_pos < s.size() && std::isdigit(s[scan_pos]),
                          "JSON parse error: decimal point must be followed by at least one digit",
                          "position",
                          scan_pos);
        while (scan_pos < s.size() && std::isdigit(s[scan_pos]))
        {
            ++scan_pos;
        }
    }

    // Optional exponent part
    if (scan_pos < s.size() && (s[scan_pos] == 'e' || s[scan_pos] == 'E'))
    {
        has_exponent = true;
        ++scan_pos;
        // Optional sign
        if (scan_pos < s.size() && (s[scan_pos] == '+' || s[scan_pos] == '-'))
        {
            ++scan_pos;
        }
        // RFC 8259: exp = e [ minus / plus ] 1*DIGIT (at least one digit required)
        FATP_JSON_ENFORCE(scan_pos < s.size() && std::isdigit(s[scan_pos]),
                          "JSON parse error: exponent must be followed by at least one digit",
                          "position",
                          scan_pos);
        while (scan_pos < s.size() && std::isdigit(s[scan_pos]))
        {
            ++scan_pos;
        }
    }

    const char* start_ptr = s.data() + start;
    const char* end_ptr = s.data() + scan_pos;

    // Prefer integer when no decimal point or exponent
    if (!has_decimal && !has_exponent)
    {
        int64_t int_val;
        auto result = std::from_chars(start_ptr, end_ptr, int_val);

        if (result.ec == std::errc() && result.ptr == end_ptr)
        {
            pos = scan_pos;
            return int_val;
        }
        // Integer overflow - fall through to double parsing
    }

    // Parse as double (handles decimals, exponents, and integer overflow)
    double dbl_val;
    auto result = std::from_chars(start_ptr, end_ptr, dbl_val);

    if (result.ec == std::errc() && result.ptr == end_ptr)
    {
        pos = scan_pos;
        return dbl_val;
    }

    // from_chars failed entirely - invalid number format
    FATP_JSON_ENFORCE(false,
                      "JSON parse error: invalid number",
                      "position",
                      start,
                      "value",
                      std::string(s.substr(start, scan_pos - start)));
    unreachable_after_enforce();
}

/**
 * @brief Parse any JSON value (forward declaration)
 *
 * @tparam Policy JSON parsing policy
 * @param s Input string view
 * @param pos Current position
 * @param depth Current nesting depth for recursion limit
 * @return JsonValue The parsed value
 */
template <typename Policy>
JsonValue parse_value(std::string_view s, size_t& pos, size_t depth = 0);

/**
 * @brief Parse JSON array
 *
 * @tparam Policy JSON policy (controls max_parse_depth)
 * @param s Input string view (pos should point to opening '[')
 * @param pos Current position (modified to point past closing ']')
 * @param depth Current nesting depth
 * @return JsonArray Parsed array
 *
 * @throws std::runtime_error on syntax error or depth exceeded
 */
template <typename Policy = StandardJsonPolicy>
inline JsonArray parse_array(std::string_view s, size_t& pos, size_t depth)
{
    FATP_JSON_ENFORCE(depth <= Policy::max_parse_depth,
                      "JSON parse error: maximum nesting depth exceeded",
                      "position",
                      pos,
                      "max_depth",
                      Policy::max_parse_depth);

    JsonArray arr;
    ++pos; // Skip opening '['
    skip_whitespace<Policy>(s, pos);

    // Empty array fast path
    if (pos < s.size() && s[pos] == ']')
    {
        ++pos;
        return arr;
    }

    // Parse comma-separated elements
    while (pos < s.size())
    {
        arr.push_back(parse_value<Policy>(s, pos, depth + 1));
        skip_whitespace<Policy>(s, pos);

        FATP_JSON_ENFORCE(pos < s.size(), "JSON parse error: unterminated array", "position", pos);

        // End of array
        if (s[pos] == ']')
        {
            ++pos;
            return arr;
        }

        // Expect comma before next element
        FATP_JSON_ENFORCE(s[pos] == ',', "JSON parse error: expected ',' or ']' in array", "position", pos);
        ++pos;
        skip_whitespace<Policy>(s, pos);
    }

    FATP_JSON_ENFORCE(false, "JSON parse error: unterminated array");
    unreachable_after_enforce();
}

/**
 * @brief Parse JSON object
 *
 * @tparam Policy JSON policy (controls max_parse_depth)
 * @param s Input string view (pos should point to opening '{')
 * @param pos Current position (modified to point past closing '}')
 * @param depth Current nesting depth
 * @return JsonObject Parsed object
 *
 * @throws std::runtime_error on syntax error or depth exceeded
 *
 * @note Duplicate keys: later values overwrite earlier ones (std::map behavior)
 */
template <typename Policy = StandardJsonPolicy>
inline JsonObject parse_object(std::string_view s, size_t& pos, size_t depth)
{
    FATP_JSON_ENFORCE(depth <= Policy::max_parse_depth,
                      "JSON parse error: maximum nesting depth exceeded",
                      "position",
                      pos,
                      "max_depth",
                      Policy::max_parse_depth);

    JsonObject obj;
    ++pos; // Skip opening '{'
    skip_whitespace<Policy>(s, pos);

    // Empty object fast path
    if (pos < s.size() && s[pos] == '}')
    {
        ++pos;
        return obj;
    }

    // Parse comma-separated key:value pairs
    while (pos < s.size())
    {
        skip_whitespace<Policy>(s, pos);

        // Key must be a string
        FATP_JSON_ENFORCE(pos < s.size() && s[pos] == '"',
                          "JSON parse error: expected string mKey_LIT_1__position",
                          pos);
        std::string key = parse_string(s, pos);

        skip_whitespace<Policy>(s, pos);

        // Expect colon after key
        FATP_JSON_ENFORCE(pos < s.size() && s[pos] == ':',
                          "JSON parse error: expected ':' after object key",
                          "position",
                          pos,
                          "key",
                          key);
        ++pos;

        skip_whitespace<Policy>(s, pos);

        // Parse value and store in map (move key to avoid copy)
        obj[std::move(key)] = parse_value<Policy>(s, pos, depth + 1);

        skip_whitespace<Policy>(s, pos);

        FATP_JSON_ENFORCE(pos < s.size(), "JSON parse error: unterminated object", "position", pos);

        // End of object
        if (s[pos] == '}')
        {
            ++pos;
            return obj;
        }

        // Expect comma before next pair
        FATP_JSON_ENFORCE(s[pos] == ',', "JSON parse error: expected ',' or '}' in object", "position", pos);
        ++pos;
    }

    FATP_JSON_ENFORCE(false, "JSON parse error: unterminated object");
    unreachable_after_enforce();
}

/**
 * @brief Parse any JSON value based on leading character
 *
 * @tparam Policy JSON parsing policy
 * @param s Input string view
 * @param pos Current position (modified to point past parsed value)
 * @param depth Current nesting depth
 * @return JsonValue The parsed value
 *
 * @throws std::runtime_error on invalid JSON syntax
 *
 * @details Dispatches to type-specific parsers based on first character:
 * - '{': object
 * - '[': array
 * - '"': string
 * - 't'/'f': boolean
 * - 'n': null
 * - digit/'-'/NaN/Infinity: number
 */
template <typename Policy = StandardJsonPolicy>
inline JsonValue parse_value(std::string_view s, size_t& pos, size_t depth)
{
    skip_whitespace<Policy>(s, pos);
    FATP_JSON_ENFORCE(pos < s.size(), "JSON parse error: unexpected end of input");

    char c = s[pos];

    // Compound types: object and array
    if (c == '{')
    {
        return parse_object<Policy>(s, pos, depth);
    }
    if (c == '[')
    {
        return parse_array<Policy>(s, pos, depth);
    }

    // String
    if (c == '"')
    {
        return parse_string(s, pos);
    }

    // Literals: true, false, null
    // Using substr comparison for simplicity; performance is fine for short literals
    if (s.substr(pos, 4) == "true")
    {
        pos += 4;
        return true;
    }
    if (s.substr(pos, 5) == "false")
    {
        pos += 5;
        return false;
    }
    if (s.substr(pos, 4) == "null")
    {
        pos += 4;
        return nullptr;
    }

    // Number (including non-standard NaN/Infinity when allowed)
    if (std::isdigit(c) || c == '-' || s.substr(pos, 3) == "NaN" || s.substr(pos, 8) == "Infinity" ||
        s.substr(pos, 9) == "-Infinity")
    {
        return parse_number<Policy>(s, pos);
    }

    FATP_JSON_ENFORCE(false, "JSON parse error: invalid value", "position", pos);
    unreachable_after_enforce();
}

} // namespace json_detail

/**
 * @brief Parse JSON string into JsonValue
 *
 * @tparam Policy JSON parsing policy (default: StandardJsonPolicy)
 * @param json JSON string to parse
 * @return JsonValue Parsed JSON value
 *
 * @throws std::runtime_error on:
 * - Invalid JSON syntax
 * - Nesting depth exceeded
 * - Extra data after root value
 *
 * @code{.cpp}
 * // Standard parsing
 * auto val = parse_json(R"({"key": [1, 2, 3]})");
 *
 * // With comments enabled
 * auto cfg = parse_json<ConfigJsonPolicy>(R"({
 *     // Server settings
 *     "port": 8080
 * })");
 *
 * // With NaN/Infinity support
 * auto data = parse_json<CompatJsonPolicy>(R"({"value": NaN})");
 * @endcode
 *
 * @see StandardJsonPolicy
 * @see ConfigJsonPolicy
 * @see CompatJsonPolicy
 */
template <typename Policy = StandardJsonPolicy>
[[nodiscard]] inline JsonValue parse_json(std::string_view json)
{
    size_t pos = 0;
    JsonValue val = json_detail::parse_value<Policy>(json, pos);

    // Verify no trailing content after root value
    json_detail::skip_whitespace<Policy>(json, pos);
    FATP_JSON_ENFORCE(pos == json.size(), "JSON parse error: extra data after JSON value", "position", pos);

    return val;
}

// ============================================================================
// Primitive Type Serialization (to_json)
// ============================================================================

/**
 * @name Primitive to_json Overloads (Reference-Based)
 * @brief Convert C++ primitives to JsonValue via output reference
 *
 * @details These overloads store the result in a JsonValue reference parameter.
 * Integer types are widened to int64_t. Large unsigned values that exceed
 * int64_t range are stored as double to preserve magnitude.
 *
 * @param j Output JsonValue reference
 * @param value Value to convert
 *
 * @{
 */
inline void to_json(JsonValue& j, std::nullptr_t) noexcept
{
    j = nullptr;
}
inline void to_json(JsonValue& j, bool value) noexcept
{
    j = value;
}
inline void to_json(JsonValue& j, int value) noexcept
{
    j = static_cast<int64_t>(value);
}
inline void to_json(JsonValue& j, unsigned int value) noexcept
{
    j = static_cast<int64_t>(value);
}
inline void to_json(JsonValue& j, long value) noexcept
{
    j = static_cast<int64_t>(value);
}
inline void to_json(JsonValue& j, unsigned long value) noexcept
{
    // Values exceeding int64_t max stored as double to preserve magnitude
    if (value > static_cast<unsigned long>(std::numeric_limits<int64_t>::max()))
    {
        j = static_cast<double>(value);
    }
    else
    {
        j = static_cast<int64_t>(value);
    }
}
inline void to_json(JsonValue& j, long long value) noexcept
{
    j = static_cast<int64_t>(value);
}
inline void to_json(JsonValue& j, unsigned long long value) noexcept
{
    // Values exceeding int64_t max stored as double to preserve magnitude
    if (value > static_cast<unsigned long long>(std::numeric_limits<int64_t>::max()))
    {
        j = static_cast<double>(value);
    }
    else
    {
        j = static_cast<int64_t>(value);
    }
}
inline void to_json(JsonValue& j, float value) noexcept
{
    j = static_cast<double>(value);
}
inline void to_json(JsonValue& j, double value) noexcept
{
    j = value;
}
inline void to_json(JsonValue& j, const std::string& value)
{
    j = value;
}
inline void to_json(JsonValue& j, const char* value)
{
    j = std::string(value);
}
inline void to_json(JsonValue& j, std::string_view value)
{
    j = std::string(value);
}
inline void to_json(JsonValue& j, signed char value) noexcept
{
    j = static_cast<int64_t>(value);
}
inline void to_json(JsonValue& j, unsigned char value) noexcept
{
    j = static_cast<int64_t>(value);
}
inline void to_json(JsonValue& j, short value) noexcept
{
    j = static_cast<int64_t>(value);
}
inline void to_json(JsonValue& j, unsigned short value) noexcept
{
    j = static_cast<int64_t>(value);
}
/** @} */

/**
 * @name Primitive to_json Overloads (Value-Returning)
 * @brief Convert C++ primitives to JsonValue, returning the result
 *
 * @details Same conversion rules as reference-based overloads but returns
 * the JsonValue directly. Enables single-line initialization and const usage.
 *
 * @param value Value to convert
 * @return JsonValue The converted value
 *
 * @{
 */
inline JsonValue to_json(std::nullptr_t) noexcept
{
    return nullptr;
}
inline JsonValue to_json(bool value) noexcept
{
    return value;
}
inline JsonValue to_json(int value) noexcept
{
    return static_cast<int64_t>(value);
}
inline JsonValue to_json(unsigned int value) noexcept
{
    return static_cast<int64_t>(value);
}
inline JsonValue to_json(long value) noexcept
{
    return static_cast<int64_t>(value);
}
inline JsonValue to_json(unsigned long value) noexcept
{
    if (value > static_cast<unsigned long>(std::numeric_limits<int64_t>::max()))
    {
        return static_cast<double>(value);
    }
    return static_cast<int64_t>(value);
}
inline JsonValue to_json(long long value) noexcept
{
    return static_cast<int64_t>(value);
}
inline JsonValue to_json(unsigned long long value) noexcept
{
    if (value > static_cast<unsigned long long>(std::numeric_limits<int64_t>::max()))
    {
        return static_cast<double>(value);
    }
    return static_cast<int64_t>(value);
}
inline JsonValue to_json(float value) noexcept
{
    return static_cast<double>(value);
}
inline JsonValue to_json(double value) noexcept
{
    return value;
}
inline JsonValue to_json(const std::string& value)
{
    return value;
}
inline JsonValue to_json(const char* value)
{
    return std::string(value);
}
inline JsonValue to_json(std::string_view value)
{
    return std::string(value);
}
inline JsonValue to_json(signed char value) noexcept
{
    return static_cast<int64_t>(value);
}
inline JsonValue to_json(unsigned char value) noexcept
{
    return static_cast<int64_t>(value);
}
inline JsonValue to_json(short value) noexcept
{
    return static_cast<int64_t>(value);
}
inline JsonValue to_json(unsigned short value) noexcept
{
    return static_cast<int64_t>(value);
}
/** @} */

// ============================================================================
// Value-Returning Convenience API (json_encode / json_decode)
// ============================================================================

/**
 * @name json_encode / json_decode
 * @brief Value-returning JSON conversion functions
 *
 * @details These functions provide convenient value-returning semantics for
 * JSON serialization and deserialization. They use ADL internally to find
 * the appropriate to_json/from_json overloads, avoiding C++ name hiding
 * issues that can occur with using declarations.
 *
 * Unlike to_json/from_json which use output parameters, these return values
 * directly, enabling more natural expression syntax:
 *
 * @code{.cpp}
 * struct Config { int port; std::string host; };
 * FATP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Config, port, host)
 *
 * // Encoding (object -> JSON)
 * Config cfg{8080, "localhost"};
 * JsonValue j = json_encode(cfg);
 *
 * // Decoding (JSON -> object)
 * Config cfg2 = json_decode<Config>(j);
 *
 * // Also works with primitives
 * JsonValue num = json_encode(42);
 * int val = json_decode<int>(num);
 * @endcode
 *
 * @note These functions are the recommended API for value-returning conversions.
 * The two-argument to_json/from_json remain available for cases where output
 * parameters are preferred or for implementing custom serializers.
 *
 * @{
 */

/**
 * @brief Encode a value to JSON (value-returning)
 *
 * @tparam T Type to encode (must have to_json overload via macro or custom)
 * @param value The value to serialize
 * @return JsonValue The JSON representation
 */
template <typename T>
inline JsonValue json_encode(const T& value)
{
    JsonValue j;
    to_json(j, value); // ADL finds the correct overload
    return j;
}

/**
 * @brief Decode JSON to a value (value-returning)
 *
 * @tparam T Target type (must have from_json overload via macro or custom)
 * @param j The JSON value to deserialize
 * @return T The deserialized object
 * @throws std::runtime_error on type mismatch or conversion failure
 */
template <typename T>
inline T json_decode(const JsonValue& j)
{
    T result{};
    from_json(j, result); // ADL finds the correct overload
    return result;
}

/** @} */

// ============================================================================
// Primitive Type Deserialization (from_json)
// ============================================================================

/**
 * @name Primitive from_json Overloads
 * @brief Convert JsonValue to C++ primitives via output reference
 *
 * @details These overloads extract values from JsonValue into C++ types.
 * Numeric conversions use checked_cast for overflow detection. Double values
 * are validated for fractional parts when converting to integers.
 *
 * @param j Input JsonValue
 * @param value Output reference to store result
 * @throws std::runtime_error on type mismatch or range overflow
 *
 * @{
 */
inline void from_json(const JsonValue& j, bool& value)
{
    FATP_JSON_ENFORCE(j.is_bool(), "JSON type mismatch", "expected", "boolean", "got", json_detail::typeName(j));
    value = std::get<bool>(j);
}

inline void from_json(const JsonValue& j, int& value)
{
    if (j.is_int())
    {
        int64_t i64 = std::get<int64_t>(j);
        value = json_lite::checked_cast<int>(i64);
    }
    else if (j.is_number())
    {
        // Double path: validate no fractional part, then range check
        json_detail::convert_double_to_int(std::get<double>(j), value, "int");
    }
    else
    {
        FATP_JSON_ENFORCE(false, "JSON type mismatch", "expected", "number", "got", json_detail::typeName(j));
    }
}

inline void from_json(const JsonValue& j, unsigned int& value)
{
    if (j.is_int())
    {
        int64_t i64 = std::get<int64_t>(j);
        value = json_lite::checked_cast<unsigned int>(i64);
    }
    else if (j.is_number())
    {
        json_detail::convert_double_to_int(std::get<double>(j), value, "unsigned int");
    }
    else
    {
        FATP_JSON_ENFORCE(false, "JSON type mismatch", "expected", "number", "got", json_detail::typeName(j));
    }
}

inline void from_json(const JsonValue& j, long& value)
{
    if (j.is_int())
    {
        int64_t i64 = std::get<int64_t>(j);
        value = json_lite::checked_cast<long>(i64);
    }
    else if (j.is_number())
    {
        json_detail::convert_double_to_int(std::get<double>(j), value, "long");
    }
    else
    {
        FATP_JSON_ENFORCE(false, "JSON type mismatch", "expected", "number", "got", json_detail::typeName(j));
    }
}

inline void from_json(const JsonValue& j, unsigned long& value)
{
    if (j.is_int())
    {
        int64_t i64 = std::get<int64_t>(j);
        value = json_lite::checked_cast<unsigned long>(i64);
    }
    else if (j.is_number())
    {
        json_detail::convert_double_to_int(std::get<double>(j), value, "unsigned long");
    }
    else
    {
        FATP_JSON_ENFORCE(false, "JSON type mismatch", "expected", "number", "got", json_detail::typeName(j));
    }
}

inline void from_json(const JsonValue& j, long long& value)
{
    if (j.is_int())
    {
        // Direct assignment - same underlying type (int64_t)
        value = std::get<int64_t>(j);
    }
    else if (j.is_number())
    {
        json_detail::convert_double_to_int(std::get<double>(j), value, "long long");
    }
    else
    {
        FATP_JSON_ENFORCE(false, "JSON type mismatch", "expected", "number", "got", json_detail::typeName(j));
    }
}

inline void from_json(const JsonValue& j, unsigned long long& value)
{
    if (j.is_int())
    {
        int64_t i64 = std::get<int64_t>(j);
        value = json_lite::checked_cast<unsigned long long>(i64);
    }
    else if (j.is_number())
    {
        json_detail::convert_double_to_int(std::get<double>(j), value, "unsigned long long");
    }
    else
    {
        FATP_JSON_ENFORCE(false, "JSON type mismatch", "expected", "number", "got", json_detail::typeName(j));
    }
}

inline void from_json(const JsonValue& j, float& value)
{
    // Float accepts both int64_t and double sources (precision loss possible)
    if (j.is_int())
    {
        value = static_cast<float>(std::get<int64_t>(j));
    }
    else if (j.is_number())
    {
        value = static_cast<float>(std::get<double>(j));
    }
    else
    {
        FATP_JSON_ENFORCE(false, "JSON type mismatch", "expected", "number", "got", json_detail::typeName(j));
    }
}

inline void from_json(const JsonValue& j, double& value)
{
    // Double accepts both int64_t and double sources
    if (j.is_int())
    {
        value = static_cast<double>(std::get<int64_t>(j));
    }
    else if (j.is_number())
    {
        value = std::get<double>(j);
    }
    else
    {
        FATP_JSON_ENFORCE(false, "JSON type mismatch", "expected", "number", "got", json_detail::typeName(j));
    }
}

inline void from_json(const JsonValue& j, std::string& value)
{
    FATP_JSON_ENFORCE(j.is_string(), "JSON type mismatch", "expected", "string", "got", json_detail::typeName(j));
    value = std::get<std::string>(j);
}

inline void from_json(const JsonValue& j, signed char& value)
{
    if (j.is_int())
    {
        int64_t i64 = std::get<int64_t>(j);
        value = json_lite::checked_cast<signed char>(i64);
    }
    else if (j.is_number())
    {
        // Inline validation for small types (convert_double_to_int not specialized)
        double d = std::get<double>(j);
        double intpart;
        FATP_JSON_ENFORCE(fabs(std::modf(d, &intpart)) <= json_detail::double_epsilon,
                          "JSON conversion error: fractional part detected",
                          "value",
                          d,
                          "target_type",
                          "signed char");
        FATP_JSON_ENFORCE(intpart >= static_cast<double>(std::numeric_limits<signed char>::min()) &&
                              intpart <= static_cast<double>(std::numeric_limits<signed char>::max()),
                          "JSON conversion error: value out of range",
                          "value",
                          d,
                          "target_type",
                          "signed char");
        value = static_cast<signed char>(intpart);
    }
    else
    {
        FATP_JSON_ENFORCE(false, "JSON type mismatch", "expected", "number", "got", json_detail::typeName(j));
    }
}

inline void from_json(const JsonValue& j, unsigned char& value)
{
    if (j.is_int())
    {
        int64_t i64 = std::get<int64_t>(j);
        value = json_lite::checked_cast<unsigned char>(i64);
    }
    else if (j.is_number())
    {
        double d = std::get<double>(j);
        double intpart;
        FATP_JSON_ENFORCE(fabs(std::modf(d, &intpart)) <= json_detail::double_epsilon,
                          "JSON conversion error: fractional part detected",
                          "value",
                          d,
                          "target_type",
                          "unsigned char");
        FATP_JSON_ENFORCE(intpart >= 0.0 && intpart <= static_cast<double>(std::numeric_limits<unsigned char>::max()),
                          "JSON conversion error: value out of range",
                          "value",
                          d,
                          "target_type",
                          "unsigned char");
        value = static_cast<unsigned char>(intpart);
    }
    else
    {
        FATP_JSON_ENFORCE(false, "JSON type mismatch", "expected", "number", "got", json_detail::typeName(j));
    }
}

inline void from_json(const JsonValue& j, short& value)
{
    if (j.is_int())
    {
        int64_t i64 = std::get<int64_t>(j);
        value = json_lite::checked_cast<short>(i64);
    }
    else if (j.is_number())
    {
        double d = std::get<double>(j);
        double intpart;
        FATP_JSON_ENFORCE(fabs(std::modf(d, &intpart)) <= json_detail::double_epsilon,
                          "JSON conversion error: fractional part detected",
                          "value",
                          d,
                          "target_type",
                          "short");
        FATP_JSON_ENFORCE(intpart >= static_cast<double>(std::numeric_limits<short>::min()) &&
                              intpart <= static_cast<double>(std::numeric_limits<short>::max()),
                          "JSON conversion error: value out of range",
                          "value",
                          d,
                          "target_type",
                          "short");
        value = static_cast<short>(intpart);
    }
    else
    {
        FATP_JSON_ENFORCE(false, "JSON type mismatch", "expected", "number", "got", json_detail::typeName(j));
    }
}

inline void from_json(const JsonValue& j, unsigned short& value)
{
    if (j.is_int())
    {
        int64_t i64 = std::get<int64_t>(j);
        value = json_lite::checked_cast<unsigned short>(i64);
    }
    else if (j.is_number())
    {
        double d = std::get<double>(j);
        double intpart;
        FATP_JSON_ENFORCE(fabs(std::modf(d, &intpart)) <= json_detail::double_epsilon,
                          "JSON conversion error: fractional part detected",
                          "value",
                          d,
                          "target_type",
                          "unsigned short");
        FATP_JSON_ENFORCE(intpart >= 0.0 && intpart <= static_cast<double>(std::numeric_limits<unsigned short>::max()),
                          "JSON conversion error: value out of range",
                          "value",
                          d,
                          "target_type",
                          "unsigned short");
        value = static_cast<unsigned short>(intpart);
    }
    else
    {
        FATP_JSON_ENFORCE(false, "JSON type mismatch", "expected", "number", "got", json_detail::typeName(j));
    }
}
/** @} */

namespace json_detail
{
/**
 * @brief Helper to convert any type to JsonValue via ADL to_json
 *
 * @tparam T Type with to_json overload
 * @param value Value to convert
 * @return JsonValue Converted value
 */
template <typename T>
JsonValue to_json_value(const T& value)
{
    JsonValue result;
    to_json(result, value);
    return result;
}
} // namespace json_detail

// ============================================================================
// Container Serialization (to_json)
// ============================================================================

/**
 * @name Sequence Container to_json Overloads
 * @brief Serialize sequence containers to JSON arrays
 *
 * @tparam T Element type (must have to_json overload)
 * @param j Output JsonValue (will hold JsonArray)
 * @param container Container to serialize
 *
 * @{
 */

/// @brief Serialize std::array to JSON array (fixed size)
template <typename T, size_t N>
void to_json(JsonValue& j, const std::array<T, N>& arr)
{
    JsonArray json_arr;
    json_arr.reserve(N);
    for (const auto& elem : arr)
    {
        json_arr.push_back(json_detail::to_json_value(elem));
    }
    j = std::move(json_arr);
}

/// @brief Serialize std::vector to JSON array
template <typename T>
void to_json(JsonValue& j, const std::vector<T>& vec)
{
    JsonArray arr;
    arr.reserve(vec.size());
    for (const auto& elem : vec)
    {
        arr.push_back(json_detail::to_json_value(elem));
    }
    j = std::move(arr);
}

/// @brief Serialize std::set to JSON array (iteration order is sorted)
template <typename T>
void to_json(JsonValue& j, const std::set<T>& s)
{
    JsonArray arr;
    for (const auto& elem : s)
    {
        arr.push_back(json_detail::to_json_value(elem));
    }
    j = std::move(arr);
}

/// @brief Serialize std::unordered_set to JSON array (order undefined)
template <typename T>
void to_json(JsonValue& j, const std::unordered_set<T>& s)
{
    JsonArray arr;
    arr.reserve(s.size());
    for (const auto& elem : s)
    {
        arr.push_back(json_detail::to_json_value(elem));
    }
    j = std::move(arr);
}

/// @brief Serialize std::deque to JSON array
template <typename T>
void to_json(JsonValue& j, const std::deque<T>& d)
{
    JsonArray arr;
    arr.reserve(d.size());
    for (const auto& elem : d)
    {
        arr.push_back(json_detail::to_json_value(elem));
    }
    j = std::move(arr);
}

/// @brief Serialize std::list to JSON array
template <typename T>
void to_json(JsonValue& j, const std::list<T>& lst)
{
    JsonArray arr;
    arr.reserve(lst.size());
    for (const auto& elem : lst)
    {
        arr.push_back(json_detail::to_json_value(elem));
    }
    j = std::move(arr);
}
/** @} */

/**
 * @name Associative Container to_json Overloads
 * @brief Serialize associative containers to JSON objects
 *
 * @details Keys are converted to strings:
 * - String keys: used directly
 * - Arithmetic keys: converted via std::to_string
 * - Other keys: converted via operator
 *
 * @tparam K Key type
 * @tparam V Value type (must have to_json overload)
 * @param j Output JsonValue (will hold JsonObject)
 * @param m Map to serialize
 *
 * @{
 */

/// @brief Serialize std::map to JSON object (lexicographic key order)
template <typename K, typename V>
void to_json(JsonValue& j, const std::map<K, V>& m)
{
    JsonObject obj;
    for (const auto& [key, val] : m)
    {
        std::string key_str;
        if constexpr (std::is_convertible_v<K, std::string>)
        {
            key_str = key;
        }
        else if constexpr (std::is_arithmetic_v<K>)
        {
            key_str = std::to_string(key);
        }
        else
        {
            // Fallback: use operator<< for custom types
            std::ostringstream oss;
            oss << key;
            key_str = oss.str();
        }
        obj[std::move(key_str)] = json_detail::to_json_value(val);
    }
    j = std::move(obj);
}

/// @brief Serialize std::unordered_map to JSON object (order undefined)
template <typename K, typename V>
void to_json(JsonValue& j, const std::unordered_map<K, V>& m)
{
    JsonObject obj;
    for (const auto& [key, val] : m)
    {
        std::string key_str;
        if constexpr (std::is_convertible_v<K, std::string>)
        {
            key_str = key;
        }
        else if constexpr (std::is_arithmetic_v<K>)
        {
            key_str = std::to_string(key);
        }
        else
        {
            std::ostringstream oss;
            oss << key;
            key_str = oss.str();
        }
        obj[std::move(key_str)] = json_detail::to_json_value(val);
    }
    j = std::move(obj);
}
/** @} */

/**
 * @name Utility Type to_json Overloads
 * @brief Serialize std::optional, std::pair, and std::tuple
 * @{
 */

/// @brief Serialize std::optional (null if empty, value otherwise)
template <typename T>
void to_json(JsonValue& j, const std::optional<T>& opt)
{
    if (opt.has_value())
    {
        to_json(j, *opt);
    }
    else
    {
        j = nullptr;
    }
}

/// @brief Serialize std::pair as two-element JSON array
template <typename T1, typename T2>
void to_json(JsonValue& j, const std::pair<T1, T2>& p)
{
    JsonArray arr;
    arr.push_back(json_detail::to_json_value(p.first));
    arr.push_back(json_detail::to_json_value(p.second));
    j = std::move(arr);
}

/// @brief Implementation helper for tuple serialization
template <typename Tuple, std::size_t... I>
JsonArray tuple_to_json_impl(const Tuple& tup, std::index_sequence<I...>)
{
    JsonArray arr;
    // Fold expression: serialize each tuple element in order
    (..., arr.push_back(json_detail::to_json_value(std::get<I>(tup))));
    return arr;
}

/// @brief Serialize std::tuple as JSON array with one element per tuple element
template <typename... Ts>
void to_json(JsonValue& j, const std::tuple<Ts...>& tup)
{
    j = tuple_to_json_impl(tup, std::index_sequence_for<Ts...>{});
}
/** @} */

/**
 * @name Value-Returning Container to_json Overloads
 * @brief Same as reference-based overloads but return JsonValue directly
 * @{
 */

template <typename T, size_t N>
JsonValue to_json(const std::array<T, N>& arr)
{
    JsonArray json_arr;
    json_arr.reserve(N);
    for (const auto& elem : arr)
    {
        json_arr.push_back(json_detail::to_json_value(elem));
    }
    return json_arr;
}

template <typename T>
JsonValue to_json(const std::vector<T>& vec)
{
    JsonArray arr;
    arr.reserve(vec.size());
    for (const auto& elem : vec)
    {
        arr.push_back(json_detail::to_json_value(elem));
    }
    return arr;
}

template <typename T>
JsonValue to_json(const std::set<T>& s)
{
    JsonArray arr;
    for (const auto& elem : s)
    {
        arr.push_back(json_detail::to_json_value(elem));
    }
    return arr;
}

template <typename T>
JsonValue to_json(const std::unordered_set<T>& s)
{
    JsonArray arr;
    arr.reserve(s.size());
    for (const auto& elem : s)
    {
        arr.push_back(json_detail::to_json_value(elem));
    }
    return arr;
}

template <typename T>
JsonValue to_json(const std::deque<T>& d)
{
    JsonArray arr;
    for (const auto& elem : d)
    {
        arr.push_back(json_detail::to_json_value(elem));
    }
    return arr;
}

template <typename T>
JsonValue to_json(const std::list<T>& lst)
{
    JsonArray arr;
    arr.reserve(lst.size());
    for (const auto& elem : lst)
    {
        arr.push_back(json_detail::to_json_value(elem));
    }
    return arr;
}

template <typename K, typename V>
JsonValue to_json(const std::map<K, V>& m)
{
    JsonObject obj;
    for (const auto& [key, val] : m)
    {
        std::string key_str;
        if constexpr (std::is_convertible_v<K, std::string>)
        {
            key_str = key;
        }
        else if constexpr (std::is_arithmetic_v<K>)
        {
            key_str = std::to_string(key);
        }
        else
        {
            std::ostringstream oss;
            oss << key;
            key_str = oss.str();
        }
        obj[std::move(key_str)] = json_detail::to_json_value(val);
    }
    return obj;
}

template <typename K, typename V>
JsonValue to_json(const std::unordered_map<K, V>& m)
{
    JsonObject obj;
    for (const auto& [key, val] : m)
    {
        std::string key_str;
        if constexpr (std::is_convertible_v<K, std::string>)
        {
            key_str = key;
        }
        else if constexpr (std::is_arithmetic_v<K>)
        {
            key_str = std::to_string(key);
        }
        else
        {
            std::ostringstream oss;
            oss << key;
            key_str = oss.str();
        }
        obj[std::move(key_str)] = json_detail::to_json_value(val);
    }
    return obj;
}

template <typename T>
JsonValue to_json(const std::optional<T>& opt)
{
    if (opt.has_value())
    {
        return to_json(*opt);
    }
    return nullptr;
}

template <typename T1, typename T2>
JsonValue to_json(const std::pair<T1, T2>& p)
{
    JsonArray arr;
    arr.push_back(json_detail::to_json_value(p.first));
    arr.push_back(json_detail::to_json_value(p.second));
    return arr;
}
/** @} */

// ============================================================================
// Container Deserialization (from_json)
// ============================================================================

/**
 * @name Sequence Container from_json Overloads
 * @brief Deserialize JSON arrays to sequence containers
 *
 * @tparam T Element type (must have from_json overload)
 * @param j Input JsonValue (must hold JsonArray)
 * @param container Output container (cleared before populating)
 * @throws std::runtime_error if j is not an array
 *
 * @{
 */

/// @brief Deserialize JSON array to std::array (size must match exactly)
template <typename T, size_t N>
void from_json(const JsonValue& j, std::array<T, N>& arr)
{
    FATP_JSON_ENFORCE(j.is_array(),
                      "JSON type mismatch",
                      "expected",
                      "array for std::array",
                      "got",
                      json_detail::typeName(j));
    const auto& json_arr = std::get<JsonArray>(j);
    FATP_JSON_ENFORCE(json_arr.size() == N, "JSON array size mismatch", "expected", N, "got", json_arr.size());
    for (size_t i = 0; i < N; ++i)
    {
        from_json(json_arr[i], arr[i]);
    }
}

/// @brief Deserialize JSON array to std::vector
template <typename T>
void from_json(const JsonValue& j, std::vector<T>& vec)
{
    FATP_JSON_ENFORCE(j.is_array(), "JSON type mismatch", "expected", "array", "got", json_detail::typeName(j));
    const auto& arr = std::get<JsonArray>(j);
    vec.clear();
    vec.reserve(arr.size());
    for (const auto& elem : arr)
    {
        T value;
        from_json(elem, value);
        vec.push_back(std::move(value));
    }
}

/// @brief Deserialize JSON array to std::set (duplicates removed)
template <typename T>
void from_json(const JsonValue& j, std::set<T>& s)
{
    FATP_JSON_ENFORCE(j.is_array(),
                      "JSON type mismatch",
                      "expected",
                      "array for std::set",
                      "got",
                      json_detail::typeName(j));
    const auto& arr = std::get<JsonArray>(j);
    s.clear();
    for (const auto& elem : arr)
    {
        T value;
        from_json(elem, value);
        s.insert(std::move(value));
    }
}

/// @brief Deserialize JSON array to std::unordered_set (duplicates removed)
template <typename T>
void from_json(const JsonValue& j, std::unordered_set<T>& s)
{
    FATP_JSON_ENFORCE(j.is_array(),
                      "JSON type mismatch",
                      "expected",
                      "array for std::unordered_set",
                      "got",
                      json_detail::typeName(j));
    const auto& arr = std::get<JsonArray>(j);
    s.clear();
    s.reserve(arr.size());
    for (const auto& elem : arr)
    {
        T value;
        from_json(elem, value);
        s.insert(std::move(value));
    }
}

/// @brief Deserialize JSON array to std::deque
template <typename T>
void from_json(const JsonValue& j, std::deque<T>& d)
{
    FATP_JSON_ENFORCE(j.is_array(),
                      "JSON type mismatch",
                      "expected",
                      "array for std::deque",
                      "got",
                      json_detail::typeName(j));
    const auto& arr = std::get<JsonArray>(j);
    d.clear();
    for (const auto& elem : arr)
    {
        T value;
        from_json(elem, value);
        d.push_back(std::move(value));
    }
}

/// @brief Deserialize JSON array to std::list
template <typename T>
void from_json(const JsonValue& j, std::list<T>& lst)
{
    FATP_JSON_ENFORCE(j.is_array(),
                      "JSON type mismatch",
                      "expected",
                      "array for std::list",
                      "got",
                      json_detail::typeName(j));
    const auto& arr = std::get<JsonArray>(j);
    lst.clear();
    for (const auto& elem : arr)
    {
        T value;
        from_json(elem, value);
        lst.push_back(std::move(value));
    }
}
/** @} */

namespace json_detail
{
/**
 * @brief Convert string key to arithmetic type
 *
 * @details Uses std::from_chars for locale-independent parsing.
 *
 * @tparam K Target arithmetic type
 * @param key String key from JSON object
 * @return K Converted key value
 * @throws std::runtime_error if conversion fails
 */
template <typename K>
K convert_map_key(const std::string& key)
{
    K result{};
    const char* start = key.data();
    const char* end = key.data() + key.size();

    auto conv_result = std::from_chars(start, end, result);

    FATP_JSON_ENFORCE(conv_result.ec == std::errc() && conv_result.ptr == end,
                      "Failed to convert map key",
                      "key",
                      key,
                      "target_type",
                      typeid(K).name());

    return result;
}
} // namespace json_detail

/**
 * @name Associative Container from_json Overloads
 * @brief Deserialize JSON objects to associative containers
 *
 * @details String keys are used directly. Arithmetic keys are parsed from strings.
 * Other key types are not supported and will fail at compile time.
 *
 * @tparam K Key type (string or arithmetic)
 * @tparam V Value type (must have from_json overload)
 * @param j Input JsonValue (must hold JsonObject)
 * @param m Output map (cleared before populating)
 * @throws std::runtime_error if j is not an object
 *
 * @{
 */

/// @brief Deserialize JSON object to std::map
template <typename K, typename V>
void from_json(const JsonValue& j, std::map<K, V>& m)
{
    FATP_JSON_ENFORCE(j.is_object(), "JSON type mismatch", "expected", "object", "got", json_detail::typeName(j));
    const auto& obj = std::get<JsonObject>(j);
    m.clear();
    for (const auto& [key, val] : obj)
    {
        V value;
        from_json(val, value);
        if constexpr (std::is_same_v<K, std::string>)
        {
            m[key] = std::move(value);
        }
        else if constexpr (std::is_arithmetic_v<K>)
        {
            K converted_key = json_detail::convert_map_key<K>(key);
            m[converted_key] = std::move(value);
        }
        else
        {
            FATP_JSON_ENFORCE(false, "Unsupported map key type for deserialization");
        }
    }
}

/// @brief Deserialize JSON object to std::unordered_map
template <typename K, typename V>
void from_json(const JsonValue& j, std::unordered_map<K, V>& m)
{
    FATP_JSON_ENFORCE(j.is_object(),
                      "JSON type mismatch",
                      "expected",
                      "object for std::unordered_map",
                      "got",
                      json_detail::typeName(j));
    const auto& obj = std::get<JsonObject>(j);
    m.clear();
    m.reserve(obj.size());
    for (const auto& [key, val] : obj)
    {
        V value;
        from_json(val, value);
        if constexpr (std::is_same_v<K, std::string>)
        {
            m[key] = std::move(value);
        }
        else if constexpr (std::is_arithmetic_v<K>)
        {
            K converted_key = json_detail::convert_map_key<K>(key);
            m[converted_key] = std::move(value);
        }
        else
        {
            FATP_JSON_ENFORCE(false, "Unsupported map key type for deserialization");
        }
    }
}
/** @} */

/**
 * @name Utility Type from_json Overloads
 * @brief Deserialize to std::optional, std::pair, and std::tuple
 * @{
 */

/// @brief Deserialize to std::optional (null becomes std::nullopt)
template <typename T>
void from_json(const JsonValue& j, std::optional<T>& opt)
{
    if (j.is_null())
    {
        opt = std::nullopt;
    }
    else
    {
        T value;
        from_json(j, value);
        opt = std::move(value);
    }
}

/// @brief Deserialize two-element JSON array to std::pair
template <typename T1, typename T2>
void from_json(const JsonValue& j, std::pair<T1, T2>& p)
{
    FATP_JSON_ENFORCE(j.is_array(),
                      "JSON type mismatch",
                      "expected",
                      "array for pair",
                      "got",
                      json_detail::typeName(j));
    const auto& arr = std::get<JsonArray>(j);
    FATP_JSON_ENFORCE(arr.size() == 2, "JSON array size mismatch for pair", "expected", 2, "got", arr.size());
    from_json(arr[0], p.first);
    from_json(arr[1], p.second);
}

/// @brief Deserialize JSON array to std::tuple (size must match tuple arity)
template <typename... Ts>
void from_json(const JsonValue& j, std::tuple<Ts...>& tup)
{
    FATP_JSON_ENFORCE(j.is_array(),
                      "JSON type mismatch",
                      "expected",
                      "array for tuple",
                      "got",
                      json_detail::typeName(j));
    const auto& arr = std::get<JsonArray>(j);
    constexpr size_t expected_size = sizeof...(Ts);
    FATP_JSON_ENFORCE(arr.size() == expected_size,
                      "JSON array size mismatch for tuple",
                      "expected",
                      expected_size,
                      "got",
                      arr.size());
    from_json_tuple_impl(arr, tup, std::index_sequence_for<Ts...>{});
}

/// @brief Implementation helper for tuple deserialization
template <typename Tuple, std::size_t... I>
void from_json_tuple_impl(const JsonArray& arr, Tuple& tup, std::index_sequence<I...>)
{
    // Fold expression: deserialize each element by index
    (..., from_json(arr[I], std::get<I>(tup)));
}
/** @} */

/**
 * @brief Value-returning from_json for convenient single-line conversions
 *
 * @tparam T Target type (must be explicitly specified)
 * @param j Input JsonValue
 * @return T Converted value
 * @throws std::runtime_error on type mismatch or conversion failure
 *
 * @code{.cpp}
 * const int port = from_json<int>(obj["port"]);
 * const auto vec = from_json<std::vector<int>>(obj["data"]);
 * @endcode
 */
template <typename T>
T from_json(const JsonValue& j)
{
    if constexpr (std::is_same_v<T, JsonValue>)
    {
        // Identity case: return copy
        return j;
    }
    else
    {
        T result;
        from_json(j, result);
        return result;
    }
}

/**
 * @brief Extract value from JsonObject by key
 *
 * @tparam T Target type
 * @param obj JSON object
 * @param key Key to lookup
 * @return T Converted value
 * @throws std::out_of_range if key not found
 * @throws std::runtime_error on type mismatch
 */
template <typename T>
T from_json(const JsonObject& obj, const std::string& key)
{
    return from_json<T>(obj.at(key));
}

/**
 * @brief Extract value from JsonValue (object) by key
 *
 * @tparam T Target type
 * @param j JSON value (must hold object)
 * @param key Key to lookup
 * @return T Converted value
 * @throws std::runtime_error if not object, key missing, or type mismatch
 */
template <typename T>
T from_json(const JsonValue& j, const std::string& key)
{
    FATP_JSON_ENFORCE(j.is_object(),
                      "JSON type mismatch: expected object for key access",
                      "expected",
                      "object",
                      "got",
                      json_detail::typeName(j),
                      "key",
                      key);
    const auto& obj = std::get<JsonObject>(j);
    return from_json<T>(obj.at(key));
}

/**
 * @brief Generic to_json for user-defined types with ADL overload
 *
 * @details Enables value-returning syntax for any type that has a
 * to_json(JsonValue&, const T&) overload defined.
 *
 * @tparam T Type with to_json overload
 * @param value Value to convert
 * @return JsonValue Converted value
 */
template <typename T>
auto to_json(const T& value) -> decltype(to_json(std::declval<JsonValue&>(), value), JsonValue{})
{
    JsonValue j;
    to_json(j, value);
    return j;
}

// ============================================================================
// File I/O
// ============================================================================

/**
 * @brief Load JSON from file
 *
 * @param filename Path to JSON file
 * @return JsonValue Parsed JSON content
 * @throws std::runtime_error if file cannot be read or JSON is invalid
 */
[[nodiscard]] inline JsonValue load_json_from_file(const std::string& filename)
{
    std::ifstream ifs(filename, std::ios::binary);
    FATP_JSON_ENFORCE(ifs.is_open(), "Failed to open file for reading", "filename", filename);
    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    FATP_JSON_ENFORCE(!ifs.bad(), "Error reading file", "filename", filename);
    return parse_json(content);
}

/**
 * @brief Save JSON to file
 *
 * @tparam Policy JSON serialization policy
 * @param filename Path to output file
 * @param val JSON value to save
 * @param pretty Enable pretty-printing
 * @throws std::runtime_error if file cannot be written
 */
template <typename Policy = StandardJsonPolicy>
inline void save_json_to_file(const std::string& filename, const JsonValue& val, bool pretty = Policy::pretty_print)
{
    std::ofstream ofs(filename);
    FATP_JSON_ENFORCE(ofs.is_open(), "Failed to open file for writing", "filename", filename);
    ofs.imbue(std::locale::classic()); // Locale-independent numeric output
    to_json_stream<JsonValue, Policy>(ofs, val, pretty);
    FATP_JSON_ENFORCE(ofs.good(), "Error writing to file", "filename", filename);
}

/**
 * @brief Convert JSON string directly to a C++ object
 *
 * @details Convenience function that combines parse_json() and from_json() into a
 * single call. Useful for testing or when working with JSON strings directly.
 *
 * @tparam T Target type to deserialize to
 * @param json_str JSON string to parse and convert
 * @return T The deserialized object
 *
 * @throws std::runtime_error If JSON is malformed or type conversion fails
 *
 * @code{.cpp}
 * auto cfg = from_json_string<Config>(R"({"port": 8080, "host": "localhost"})");
 * @endcode
 *
 * @see parse_json
 * @see from_json
 */
template <typename T>
[[nodiscard]] T from_json_string(std::string_view json_str)
{
    JsonValue val = parse_json(json_str);
    return from_json<T>(val);
}

/**
 * @brief Save a C++ object to JSON file.
 *
 * @code{.cpp}
 * save_params("config.json", cfg);  // Pretty printed
 * save_params<Config, StandardJsonPolicy>("config.json", cfg);  // Compact
 * @endcode
 *
 * @throws std::runtime_error if file cannot be opened or write fails
 */
template <typename T, typename Policy = PrettyJsonPolicy>
inline void save_params(const std::string& filename, const T& params)
{
    save_json_to_file<Policy>(filename, to_json(params), true);
}

/**
 * @brief Load a C++ object from JSON file (value-returning).
 *
 * @code{.cpp}
 * const Config cfg = load_params<Config>("config.json");
 * @endcode
 *
 * @throws std::runtime_error if file read, parse, or conversion fails
 */
template <typename T>
[[nodiscard]] inline T load_params(const std::string& filename)
{
    return from_json<T>(load_json_from_file(filename));
}

/// @brief Load into existing object reference.
template <typename T>
inline void load_params(const std::string& filename, T& params)
{
    params = from_json<T>(load_json_from_file(filename));
}

// ============================================================================
// JSON Pointer (RFC 6901)
// ============================================================================

namespace json_detail
{

// Decode escape sequences per RFC 6901: ~0 -> ~, ~1 -> /
[[nodiscard]] inline std::string decode_json_pointer_token(std::string_view token)
{
    std::string result;
    result.reserve(token.size());

    for (size_t i = 0; i < token.size(); ++i)
    {
        if (token[i] == '~')
        {
            FATP_JSON_ENFORCE(i + 1 < token.size(),
                              "JSON Pointer: incomplete escape sequence at end of token",
                              "position",
                              i,
                              "token",
                              std::string(token));

            if (token[i + 1] == '0')
            {
                result += '~';
                ++i;
            }
            else if (token[i + 1] == '1')
            {
                result += '/';
                ++i;
            }
            else
            {
                FATP_JSON_ENFORCE(false,
                                  "JSON Pointer: invalid escape sequence (only ~0 and ~1 allowed)",
                                  "sequence",
                                  std::string("~") + std::string(1, token[i + 1]),
                                  "position",
                                  i);
            }
        }
        else
        {
            result += token[i];
        }
    }

    return result;
}

// Navigate one level: object key lookup or array index
[[nodiscard]] inline const JsonValue& navigate_json_level(const JsonValue& current, std::string_view decoded_token)
{
    if (current.is_object())
    {
        const auto& obj = std::get<JsonObject>(current);
        std::string key(decoded_token);

        auto it = obj.find(key);
        FATP_JSON_ENFORCE(it != obj.end(), "JSON Pointer: object key not found", "key", key);

        return it->second;
    }
    else if (current.is_array())
    {
        const auto& arr = std::get<JsonArray>(current);

        FATP_JSON_ENFORCE(!decoded_token.empty(), "JSON Pointer: empty array index");

        if (decoded_token == "-")
        {
            FATP_JSON_ENFORCE(false, "JSON Pointer: array index '-' not valid for query (use for append only)");
        }

        FATP_JSON_ENFORCE(decoded_token[0] != '0' || decoded_token.size() == 1,
                          "JSON Pointer: array index has leading zero (not allowed per RFC 6901)",
                          "token",
                          std::string(decoded_token));

        size_t index = 0;
        auto [ptr, ec] = std::from_chars(decoded_token.data(), decoded_token.data() + decoded_token.size(), index);

        FATP_JSON_ENFORCE(ec == std::errc{} && ptr == decoded_token.data() + decoded_token.size(),
                          "JSON Pointer: invalid array index (must be non-negative integer)",
                          "token",
                          std::string(decoded_token));

        FATP_JSON_ENFORCE(index < arr.size(),
                          "JSON Pointer: array index out of bounds",
                          "index",
                          index,
                          "size",
                          arr.size());

        return arr[index];
    }
    else
    {
        std::string type_str;
        if (current.is_null())
        {
            type_str = "null";
        }
        else if (current.is_bool())
        {
            type_str = "boolean";
        }
        else if (current.is_number())
        {
            type_str = "number";
        }
        else if (current.is_string())
        {
            type_str = "string";
        }
        else
        {
            type_str = "unknown";
        }

        FATP_JSON_ENFORCE(false,
                          "JSON Pointer: cannot navigate into scalar value",
                          "type",
                          type_str,
                          "token",
                          std::string(decoded_token));

        return current;
    }
}

} // namespace json_detail

/**
 * @brief Query JSON using RFC 6901 JSON Pointer.
 *
 * @code{.cpp}
 * const JsonValue& port = query_json_pointer(config, "/database/port");
 * const JsonValue& elem = query_json_pointer(config, "/servers/0");
 * @endcode
 *
 * Escape sequences: ~0 -> ~, ~1 -> /
 *
 * @throws std::runtime_error if path invalid or not found
 */
[[nodiscard]] inline const JsonValue& query_json_pointer(const JsonValue& root, std::string_view pointer)
{
    if (pointer.empty())
    {
        return root;
    }

    FATP_JSON_ENFORCE(pointer.front() == '/',
                      "JSON Pointer must start with '/' or be empty",
                      "pointer",
                      std::string(pointer));

    const JsonValue* current = &root;
    size_t start = 1;

    while (start <= pointer.size())
    {
        size_t next_slash = pointer.find('/', start);
        size_t end = (next_slash == std::string_view::npos) ? pointer.size() : next_slash;

        std::string_view token = pointer.substr(start, end - start);
        std::string decoded = json_detail::decode_json_pointer_token(token);

        current = &json_detail::navigate_json_level(*current, decoded);

        if (next_slash == std::string_view::npos)
        {
            break;
        }
        start = next_slash + 1;
    }

    return *current;
}

/// @brief Mutable version of query_json_pointer.
[[nodiscard]] inline JsonValue& query_json_pointer(JsonValue& root, std::string_view pointer)
{
    return const_cast<JsonValue&>(query_json_pointer(const_cast<const JsonValue&>(root), pointer));
}

/**
 * @brief Type-safe JSON Pointer query with automatic conversion.
 *
 * @code{.cpp}
 * int port = query_json_as<int>(config, "/database/port");
 * @endcode
 *
 * @throws std::runtime_error if navigation or conversion fails
 */
template <typename T>
[[nodiscard]] inline T query_json_as(const JsonValue& root, std::string_view pointer)
{
    const JsonValue& value = query_json_pointer(root, pointer);
    return from_json<T>(value);
}

/**
 * @brief Save object to JSON file with automatic backup.
 *
 * Creates backup before writing. If save fails, backup remains intact.
 *
 * @param backup_suffix Suffix for backup file (default: ".bak")
 * @throws std::runtime_error if backup or write fails
 */
template <typename T, typename Policy = PrettyJsonPolicy>
inline void
save_params_with_backup(const std::string& filename, const T& params, const std::string& backup_suffix = ".bak")
{
    std::ifstream test(filename);
    if (test.good())
    {
        test.close();
        std::string backup_name = filename + backup_suffix;

        std::ifstream src(filename, std::ios::binary);
        FATP_JSON_ENFORCE(src.is_open(), "Failed to open source file for backup", "filename", filename);

        std::ofstream dst(backup_name, std::ios::binary);
        FATP_JSON_ENFORCE(dst.is_open(), "Failed to create backup file", "backup_file", backup_name);

        dst << src.rdbuf();
        FATP_JSON_ENFORCE(dst.good(), "Error writing backup file", "backup_file", backup_name);

        dst.close();
        src.close();
    }

    save_params<T, Policy>(filename, params);
}

} // namespace fat_p
