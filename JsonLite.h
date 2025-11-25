/**
 * @file JsonLite.h
 * @brief Lightweight JSON library for C++ configuration and parameter management
 * 
 * @section overview Overview
 * JsonLite is a modern C++17 header-only JSON library designed specifically for
 * application configuration files, parameter persistence, and structured data
 * serialization where simplicity and minimal external dependencies are priorities.
 * 
 * This is the STANDALONE version with ZERO external dependencies.
 * For fat_p component integration, use FatPJsonLite.h instead.
 * 
 * @section features Features
 * - C++17 header-only library (single file, no build configuration)
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
 * USING_JSON_LITE()
 *
 * struct Config {
 *     int port;
 *     std::string host;
 *     std::optional<int> timeout;
 * };
 * CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Config, port, host, timeout)
 * 
 * int main() {
 *     Config cfg{8080, "localhost", 30};
 *     save_params("config.json", cfg);
 *     
 *     auto loaded = load_params<Config>("config.json");
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

#pragma once

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <list>
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
namespace fat_p {

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
namespace json_detail {
    /**
     * @struct SourceLocation
     * @brief Lightweight structure for capturing source code location information
     * 
     * @details Stores file name, line number, and function name for error reporting.
     * This is a simple alternative to std::source_location (C++20) that works in C++17.
     * Used by json_enforce macro to provide detailed error messages with context.
     * 
     * @see json_enforce
     * @see JSON_LOCUS
     */
    struct SourceLocation 
    {
        const char* file;       ///< Source file name from __FILE__
        int line;               ///< Source line number from __LINE__
        const char* function;   ///< Function name from __func__
    };
    
    /**
     * @def JSON_LOCUS
     * @brief Macro to capture current source location
     * 
     * @details Expands to a SourceLocation structure initialized with the current
     * file, line, and function. Used internally by json_enforce for error reporting.
     * 
     * @code{.cpp}
     * SourceLocation loc = JSON_LOCUS;
     * std::cout << "Error at " << loc.file << ":" << loc.line << std::endl;
     * @endcode
     * 
     * @see SourceLocation
     * @see json_enforce
     */
    #define JSON_LOCUS ::fat_p::json_detail::SourceLocation{__FILE__, __LINE__, __func__}
    
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
    template<typename T>
    inline void append_to_stream(std::ostringstream& oss, const T& value) 
    {
        oss << value;
    }
    
    /**
     * @brief Variadic template to append multiple values to string stream
     * 
     * @details Recursively appends values to the output string stream, separated by
     * spaces. This is used internally by json_enforce to build error messages from
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
     * @see json_enforce_impl
     */
    template<typename T, typename... Args>
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
     * @brief Internal implementation of json_enforce macro
     * 
     * @details This function is called by the json_enforce macro when a condition fails.
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
     * @warning This function should not be called directly; use the json_enforce macro instead
     * 
     * @see json_enforce
     * @see SourceLocation
     */
    template<typename... Args>
    [[noreturn]] inline void json_enforce_impl(bool condition, SourceLocation loc, Args&&... args) 
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
}

/**
 * @def json_enforce
 * @brief Contract enforcement macro with source location tracking
 * 
 * @details This macro implements a Design-by-Contract (DbC) style assertion that throws
 * std::runtime_error with detailed context when the condition fails. It automatically
 * captures source location (file, line, function) and supports optional name-value pairs
 * for additional context.
 * 
 * The macro expands to a do-while(0) block for safe use in all control flow contexts.
 * 
 * @section usage Usage
 * @code{.cpp}
 * // Basic usage
 * json_enforce(value >= 0, "Value must be non-negative");
 * 
 * // With context (name-value pairs)
 * json_enforce(port >= 1024, 
 *     "Invalid port number",
 *     "port", port,
 *     "min", 1024);
 * 
 * // Type checking
 * json_enforce(j.is_int(), 
 *     "Type mismatch",
 *     "expected", "int",
 *     "got", type_name(j));
 * @endcode
 * 
 * @section error_messages Error Messages
 * When a check fails, the error message includes:
 * - Source file, line number, and function name
 * - The failed condition (stringified)
 * - Any additional name-value pairs provided
 * 
 * Example error:
 * @code
 * JSON Error at JsonLite.h:1234 in from_json - condition: port >= 1024 port 80 min 1024
 * @endcode
 * 
 * @param condition Boolean expression to check (must evaluate to true)
 * @param ... Optional name-value pairs for error context (variadic)
 * 
 * @throws std::runtime_error When condition is false
 * 
 * @note The condition is stringified and included in the error message
 * @note This macro is safe to use in headers (no ODR violations)
 * 
 * @see json_enforce_impl
 * @see JSON_LOCUS
 */
#define json_enforce(condition, ...) \
    do { \
        if (!(condition)) { \
            ::fat_p::json_detail::json_enforce_impl(false, JSON_LOCUS, \
                "condition: ", #condition, ##__VA_ARGS__); \
        } \
    } while(0)

/**
 * @brief Safe numeric type conversion with overflow and range checking
 * 
 * @details This template function provides checked numeric conversions between
 * integral types, preventing silent overflow, truncation, and sign-related bugs.
 * It performs compile-time and runtime checks to ensure the source value fits
 * within the target type's representable range.
 * 
 * The function handles all combinations of signed/unsigned conversions:
 * - Signed ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€š ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¾ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ Signed: Checks both min and max bounds
 * - Unsigned ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€š ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¾ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ Signed: Checks maximum bound and signedness
 * - Signed ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€š ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¾ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ Unsigned: Checks for negativity and maximum bound
 * - Unsigned ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€š ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¾ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ Unsigned: Checks maximum bound
 * 
 * @section behavior Special Behaviors
 * - If source and target types are identical, returns value directly (zero overhead)
 * - Uses constexpr if for compile-time optimization
 * - Throws with detailed error messages including value and type information
 * 
 * @section examples Examples
 * @code{.cpp}
 * // Safe conversions
 * int64_t big = 1000;
 * int small = checked_cast<int>(big);  // OK
 * 
 * // Overflow detection
 * int64_t too_big = INT_MAX + 1LL;
 * int fail = checked_cast<int>(too_big);  // Throws!
 * 
 * // Negativity detection
 * int negative = -1;
 * unsigned int ufail = checked_cast<unsigned int>(negative);  // Throws!
 * 
 * // Different sized unsigned types
 * uint64_t u64 = 1ULL << 40;
 * uint32_t u32 = checked_cast<uint32_t>(u64);  // Throws!
 * @endcode
 * 
 * @tparam To Target numeric type (integral)
 * @tparam From Source numeric type (integral, deduced from value)
 * @param value The value to convert
 * @return To The safely converted value
 * 
 * @throws std::runtime_error via json_enforce when:
 *         - Value exceeds target type's maximum
 *         - Value is below target type's minimum (signed types)
 *         - Negative value cast to unsigned type
 * 
 * @note This is a foundational utility used extensively in JsonLite's from_json conversions
 * @note Zero runtime overhead when To == From (optimized away by compiler)
 * @note Error messages include source value, target type name, and valid range
 * 
 * @warning This function is designed for integral types only
 * 
 * @see from_json
 * @see json_enforce
 */
template<typename To, typename From>
inline To checked_cast(From value) 
{
    if constexpr (std::is_same_v<To, From>) 
    {
        return value;
    }
    else if constexpr (std::is_signed_v<From> && std::is_signed_v<To>) 
    {
        if (value < static_cast<From>(std::numeric_limits<To>::min()) ||
            value > static_cast<From>(std::numeric_limits<To>::max())) 
        {
            json_enforce(false, 
                "Numeric cast out of range: value=", value,
                " target_type=", typeid(To).name(),
                " min=", std::numeric_limits<To>::min(),
                " max=", std::numeric_limits<To>::max());
        }
    }
    else if constexpr (!std::is_signed_v<From> && std::is_signed_v<To>) 
    {
        if (value > static_cast<typename std::make_unsigned<To>::type>(std::numeric_limits<To>::max())) 
        {
            json_enforce(false,
                "Numeric cast out of range: value=", value,
                " target_type=", typeid(To).name(),
                " max=", std::numeric_limits<To>::max());
        }
    }
    else if constexpr (std::is_signed_v<From> && !std::is_signed_v<To>) 
    {
        if (value < 0) 
        {
            json_enforce(false,
                "Numeric cast: negative value for unsigned type: value=", value,
                " target_type=", typeid(To).name());
        }
        if (static_cast<typename std::make_unsigned<From>::type>(value) > std::numeric_limits<To>::max()) 
        {
            json_enforce(false,
                "Numeric cast out of range: value=", value,
                " target_type=", typeid(To).name(),
                " max=", std::numeric_limits<To>::max());
        }
    }
    else 
    {
        if (value > std::numeric_limits<To>::max()) 
        {
            json_enforce(false,
                "Numeric cast out of range: value=", value,
                " target_type=", typeid(To).name(),
                " max=", std::numeric_limits<To>::max());
        }
    }
    return static_cast<To>(value);
}

/**
 * @namespace fat_p::json_detail
 * @brief Extended implementation details for JsonLite internal use
 */
namespace json_detail {
    /**
     * @var double_epsilon
     * @brief Tolerance for floating-point fractional part detection
     * 
     * @details Used when converting doubles to integers to determine if a fractional
     * part exists. Set to 100 times the machine epsilon to handle rounding errors
     * in floating-point arithmetic.
     * 
     * Example: 3.0000000001 would be considered an integer, but 3.1 would not.
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
     * @param type_name Type name for error messages
     * 
     * @details Validates that:
     * - Value has no fractional part (within epsilon tolerance)
     * - For unsigned types, value is non-negative
     * - Value is within the target type's range
     * 
     * @throws std::runtime_error if validation fails
     */
    template <typename IntType>
    inline void convert_double_to_int(double d, IntType& result, const char* type_name) 
    {
        static_assert(std::is_integral_v<IntType>, "IntType must be an integral type");
        
        double intpart;
        json_enforce(fabs(std::modf(d, &intpart)) <= double_epsilon,
                "JSON conversion error: fractional part detected",
                "value", d,
                "target_type", type_name);
        
        if constexpr (std::is_unsigned_v<IntType>) 
        {
            json_enforce(d >= 0, 
                    "JSON conversion error: negative value for unsigned type", 
                    "value", d,
                    "target_type", type_name);
        }
        
        if constexpr (std::is_signed_v<IntType> && sizeof(IntType) >= 8) 
        {
            constexpr double type_min = static_cast<double>(std::numeric_limits<IntType>::min());
            constexpr double type_max = static_cast<double>(std::numeric_limits<IntType>::max());
            
            if (intpart < type_min || intpart > type_max)
            {
                json_enforce(false,
                        "JSON conversion error: value out of range for 64-bit signed integer",
                        "value", d,
                        "target_type", type_name);
            }
            
            IntType candidate = static_cast<IntType>(intpart);
            
            if (intpart < 0.0 && candidate > 0)
            {
                json_enforce(false,
                        "JSON conversion error: underflow detected",
                        "value", d,
                        "converted", candidate,
                        "target_type", type_name);
            }
            
            if (intpart > 0.0 && candidate < 0)
            {
                json_enforce(false,
                        "JSON conversion error: overflow detected",
                        "value", d,
                        "converted", candidate,
                        "target_type", type_name);
            }
            
            result = candidate;
        }
        else
        {
            constexpr double type_min = static_cast<double>(std::numeric_limits<IntType>::min());
            constexpr double type_max = static_cast<double>(std::numeric_limits<IntType>::max());
            
            json_enforce(intpart >= type_min && intpart <= type_max,
                    "JSON conversion error: value out of range",
                    "value", d,
                    "target_type", type_name);
            
            IntType candidate = static_cast<IntType>(intpart);
            
            if constexpr (sizeof(IntType) >= 8) 
            {
                double roundtrip = static_cast<double>(candidate);
                json_enforce(roundtrip == intpart,
                        "JSON conversion error: precision loss or overflow detected",
                        "value", d,
                        "converted", candidate,
                        "target_type", type_name);
            }
            
            result = candidate;
        }
    }

    /**
     * @brief Unreachable code marker after json_enforce
     * 
     * @details This function is marked [[noreturn]] and calls std::abort().
     * It serves as a termination point for code paths that should never execute
     * after a failed json_enforce check. The compiler can use this for optimization.
     * 
     * @note This function exists to satisfy compilers that require explicit
     *       termination after noreturn paths
     * 
     * @see json_enforce
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
    struct IsIterable : std::false_type {};
    
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
    struct IsIterable<T, std::void_t<decltype(std::begin(std::declval<T&>())), 
                                      decltype(std::end(std::declval<T&>()))>> : std::true_type {};
    
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
    struct HasMappedType : std::false_type {};
    
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
    struct HasMappedType<T, std::void_t<typename T::mapped_type>> : std::true_type {};
    
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
}

/**
 * @typedef JsonObject
 * @brief JSON object representation as string-to-JsonValue map
 * 
 * @details Uses std::map with std::string keys and JsonValue values to represent
 * JSON objects. Provides ordered key iteration (lexicographic order) which aids
 * in deterministic testing and debugging.
 * 
 * @section why_map Why std::map Instead of std::unordered_map?
 * - Deterministic iteration order for testing and debugging
 * - Better performance for small objects (< 20 keys), which are common in config files
 * - Simpler memory layout and fewer allocations
 * - More predictable behavior across platforms
 * 
 * @section usage Usage
 * @code{.cpp}
 * JsonObject obj;
 * obj["name"] = "Alice";
 * obj["age"] = 30;
 * obj["active"] = true;
 * 
 * // Iteration is ordered by key
 * for (const auto& [key, value] : obj) {
 *     std::cout << key << ": " << value << "\n";
 * }
 * @endcode
 * 
 * @note Keys are always std::string (not string_view or const char*)
 * @note Ordered iteration enables stable JSON output
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
 * @details JsonValue is the core type in JsonLite, representing any JSON value.
 * It inherits from std::variant and can hold:
 * - null (std::nullptr_t)
 * - boolean (bool)
 * - integer (int64_t)
 * - floating-point (double)
 * - string (std::string)
 * - array (JsonArray = std::vector<JsonValue>)
 * - object (JsonObject = std::map<std::string, JsonValue>)
 * 
 * @section design Design Rationale
 * 1. **Type Safety**: std::variant provides compile-time type safety
 * 2. **Integer Precision**: int64_t preserves all JSON number integers exactly
 * 3. **Simplicity**: Direct inheritance from std::variant (no wrapper overhead)
 * 4. **Modern C++**: Leverages C++17 features (variant, structured bindings)
 * 
 * @section type_checking Type Checking
 * JsonValue provides type checking methods:
 * - is_null() - Check for null
 * - is_bool() - Check for boolean
 * - is_int() - Check for integer (int64_t)
 * - is_number() - Check for any numeric type (int or double)
 * - is_double() - Check for floating-point
 * - is_string() - Check for string
 * - is_array() - Check for array
 * - is_object() - Check for object
 * 
 * @section access Value Access
 * Use std::get<T> or std::get_if<T> to extract values:
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
 * @section examples Common Usage Examples
 * @code{.cpp}
 * // Null
 * JsonValue null_val = nullptr;
 * 
 * // Boolean
 * JsonValue bool_val = true;
 * 
 * // Integer
 * JsonValue int_val = 42;
 * 
 * // Float
 * JsonValue float_val = 3.14;
 * 
 * // String
 * JsonValue str_val = std::string("hello");
 * 
 * // Array
 * JsonArray arr;
 * arr.push_back(1);
 * arr.push_back(2);
 * JsonValue arr_val = arr;
 * 
 * // Object
 * JsonObject obj;
 * obj["key"] = "value";
 * JsonValue obj_val = obj;
 * @endcode
 * 
 * @note Inheriting from std::variant allows using visitor patterns and std::visit
 * @note All numeric JSON values are parsed as either int64_t or double
 * @note String values use std::string (not string_view) for value semantics
 * 
 * @see JsonObject
 * @see JsonArray
 * @see to_json
 * @see from_json
 */
struct JsonValue : std::variant<std::nullptr_t, bool, int64_t, double, std::string, 
                                 JsonArray, JsonObject>
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

namespace json_detail {
    /**
     * @brief Get human-readable type name from JsonValue
     * 
     * @param j JsonValue to get type name from
     * @return std::string_view Type name ("null", "boolean", "integer", "number", "string", "array", "object", or "unknown")
     * 
     * @details Returns a string view representing the actual type held by the JsonValue.
     * This is primarily used for error messages and debugging.
     * 
     * @code{.cpp}
     * JsonValue j1 = 42;
     * JsonValue j2 = "hello";
     * JsonValue j3 = true;
     * 
     * assert(type_name(j1) == "integer");
     * assert(type_name(j2) == "string");
     * assert(type_name(j3) == "boolean");
     * @endcode
     * 
     * @note Returns "number" for any numeric type, "integer" specifically for int64_t
     * @note This is a noexcept function safe to call in any context
     * 
     * @see JsonValue
     */
    inline std::string_view type_name(const JsonValue& j) noexcept 
    {
        if (j.is_null()) return "null";
        if (j.is_bool()) return "boolean";
        if (j.is_int()) return "integer";
        if (j.is_number()) return "number";
        if (j.is_string()) return "string";
        if (j.is_array()) return "array";
        if (j.is_object()) return "object";
        return "unknown";
    }
}

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
enum class NumberFormat {
    Auto,         ///< Automatic formatting (default): shortest representation
    Fixed,        ///< Fixed-point notation (e.g., "123.456")
    Scientific,   ///< Scientific notation (e.g., "1.23e+02")
    Shortest      ///< Shortest representation between fixed and scientific
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
 * - **max_parse_depth**: Maximum nesting depth during parsing (default: 512)
 * - **max_dump_depth**: Maximum nesting depth during serialization (default: 512)
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
struct StandardJsonPolicy {
    static constexpr bool pretty_print = true;              ///< Enable indentation and newlines
    static constexpr int numeric_precision = 16;            ///< Decimal precision for floats
    static constexpr int indent_step = 4;                   ///< Spaces per indent level
    static constexpr bool allow_nan_inf = false;            ///< Allow non-standard NaN/Infinity
    static constexpr bool escape_unicode = true;            ///< Escape non-ASCII characters
    static constexpr bool allow_comments = false;           ///< Allow C-style comments (// and /* */)
    static constexpr size_t max_parse_depth = 512;          ///< Max parsing recursion depth
    static constexpr size_t max_dump_depth = 512;           ///< Max serialization recursion depth
    static constexpr NumberFormat number_format = NumberFormat::Auto;  ///< Number output format
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
struct PrettyJsonPolicy : StandardJsonPolicy {
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
struct FixedFormatPolicy : StandardJsonPolicy {
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
struct ScientificFormatPolicy : StandardJsonPolicy {
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
struct CompatJsonPolicy : StandardJsonPolicy {
    static constexpr bool allow_nan_inf = true;     ///< Allow NaN and Infinity
    static constexpr bool escape_unicode = false;   ///< Output raw UTF-8
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
struct ConfigJsonPolicy : StandardJsonPolicy {
    static constexpr bool allow_comments = true;
};

namespace json_detail {
    struct PolicyContext {
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
    struct PolicyScope {
        std::unique_ptr<PolicyContext> prev_ctx;
        
        PolicyScope() : prev_ctx(std::move(current_policy_context))
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

}

namespace json_detail {
    template <typename Os>
    inline void output_indent(Os& os, int indent) noexcept 
    {
        for (int i = 0; i < indent; ++i) 
        {
            os << ' ';
        }
    }

    template <typename Os, typename Policy>
    void escape_string(Os& os, std::string_view s) 
    {
        bool escape_unicode = get_effective_escape_unicode<Policy>();
        os << '"';
        
        for (size_t i = 0; i < s.size(); ) 
        {
            unsigned char c = static_cast<unsigned char>(s[i]);
            
            switch (c) 
            {
                case '"': os << R"(\")"; ++i; continue;
                case '\\': os << R"(\\)"; ++i; continue;
                case '\b': os << R"(\b)"; ++i; continue;
                case '\f': os << R"(\f)"; ++i; continue;
                case '\n': os << R"(\n)"; ++i; continue;
                case '\r': os << R"(\r)"; ++i; continue;
                case '\t': os << R"(\t)"; ++i; continue;
                default: break;
            }
            
            if (c < 0x20) 
            {
                os << "\\u" << std::hex << std::setw(4) << std::setfill('0') 
                   << static_cast<int>(c) << std::dec;
                ++i;
                continue;
            }
            
            if (c < 0x80) 
            {
                os << static_cast<char>(c);
                ++i;
                continue;
            }
            
            if (!escape_unicode) 
            {
                os << static_cast<char>(c);
                ++i;
                continue;
            }
            
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
                os << "\\u" << std::hex << std::setw(4) << std::setfill('0') 
                   << static_cast<int>(c) << std::dec;
                ++i;
                continue;
            }
            
            if (i + bytes > s.size()) 
            {
                os << "\\u" << std::hex << std::setw(4) << std::setfill('0') 
                   << static_cast<int>(c) << std::dec;
                ++i;
                continue;
            }
            
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
                os << "\\u" << std::hex << std::setw(4) << std::setfill('0') 
                   << static_cast<int>(c) << std::dec;
                ++i;
                continue;
            }
            
            if (codepoint <= 0xFFFF) 
            {
                os << "\\u" << std::hex << std::setw(4) << std::setfill('0') 
                   << codepoint << std::dec;
            } 
            else 
            {
                codepoint -= 0x10000;
                uint16_t high = 0xD800 + ((codepoint >> 10) & 0x3FF);
                uint16_t low = 0xDC00 + (codepoint & 0x3FF);
                os << "\\u" << std::hex << std::setw(4) << std::setfill('0') 
                   << high << "\\u" << std::setw(4) << std::setfill('0') 
                   << low << std::dec;
            }
            
            i += bytes;
        }
        
        os << '"';
    }

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
                os << static_cast<int64_t>(obj);
            }
        } 
        else if constexpr (std::is_convertible_v<T, std::string_view>) 
        {
            escape_string<Os, Policy>(os, obj);
        }
    }

    template <typename Os, typename Policy, typename Tuple, std::size_t... I>
    void dump_tuple_impl(Os& os, const Tuple& tup, std::index_sequence<I...>, 
                        bool pretty, int indent);

    template <typename T>
    struct is_optional : std::false_type {};
    
    template <typename T>
    struct is_optional<std::optional<T>> : std::true_type {};
    
    template <typename T>
    inline constexpr bool is_optional_v = is_optional<T>::value;
    
    template <typename T>
    struct is_pair : std::false_type {};
    
    template <typename T1, typename T2>
    struct is_pair<std::pair<T1, T2>> : std::true_type {};
    
    template <typename T>
    inline constexpr bool is_pair_v = is_pair<T>::value;
    
    template <typename T>
    struct is_tuple : std::false_type {};
    
    template <typename... Ts>
    struct is_tuple<std::tuple<Ts...>> : std::true_type {};
    
    template <typename T>
    inline constexpr bool is_tuple_v = is_tuple<T>::value;
    
    template <typename T>
    struct optional_value_type { using type = T; };
    
    template <typename T>
    struct optional_value_type<std::optional<T>> { using type = T; };
    
    template <typename T>
    using optional_value_type_t = typename optional_value_type<T>::type;

    template <typename T, typename = void>
    struct has_to_json : std::false_type {};
    
    template <typename T>
    struct has_to_json<T, std::void_t<decltype(to_json(std::declval<JsonValue&>(), 
                                                        std::declval<const T&>()))>> 
        : std::true_type {};
    
    template <typename T>
    inline constexpr bool has_to_json_v = has_to_json<T>::value;

    template <typename T>
    struct is_sequence_container : std::false_type {};
    
    template <typename T>
    struct is_sequence_container<std::vector<T>> : std::true_type {};
    
    template <typename T>
    struct is_sequence_container<std::deque<T>> : std::true_type {};
    
    template <typename T>
    struct is_sequence_container<std::list<T>> : std::true_type {};
    
    template <typename T>
    inline constexpr bool is_sequence_container_v = is_sequence_container<T>::value;

    template <typename Policy>
    inline void check_dump_depth(int indent) 
    {
        size_t depth = static_cast<size_t>(indent / 
                                           (Policy::indent_step > 0 ? Policy::indent_step : 1));
        json_enforce(depth <= Policy::max_dump_depth,
                "JSON dump error: maximum nesting depth exceeded",
                "max_depth", Policy::max_dump_depth,
                "current_depth", depth);
    }
}

template <typename T, typename Policy, typename = void>
struct JsonDispatcher;

template <typename Policy>
struct JsonDispatcher<std::nullptr_t, Policy> {
    template <typename Os>
    static void dump(Os& os, std::nullptr_t, bool = Policy::pretty_print, int = 0) 
    {
        os << "null";
    }
};

template <typename T, typename Policy>
struct JsonDispatcher<T, Policy, std::enable_if_t<
    (!json_detail::IsIterable<T>::value || std::is_same_v<std::decay_t<T>, std::string>) &&
    !json_detail::is_optional_v<T> && 
    !json_detail::is_pair_v<T> && 
    !json_detail::is_tuple_v<T>>>
{
    template <typename Os>
    static void dump(Os& os, const T& obj, bool pretty = Policy::pretty_print, int indent = 0) 
    {
        if constexpr (std::is_same_v<T, std::nullptr_t> || std::is_null_pointer_v<T>) 
        {
            os << "null";
        } 
        else if constexpr (!std::is_arithmetic_v<T> && !std::is_same_v<std::decay_t<T>, std::string> 
                         && !std::is_convertible_v<T, std::string_view> 
                         && json_detail::has_to_json_v<T>) 
        {
            JsonValue j;
            to_json(j, obj);
            JsonDispatcher<JsonValue, Policy>::dump(os, j, pretty, indent);
        } 
        else 
        {
            json_detail::dump_scalar<Os, T, Policy>(os, obj);
        }
    }
};

template <typename T, typename Policy>
struct JsonDispatcher<std::optional<T>, Policy> {
    template <typename Os>
    static void dump(Os& os, const std::optional<T>& opt, bool pretty = Policy::pretty_print, 
                    int indent = 0) 
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

template <typename T1, typename T2, typename Policy>
struct JsonDispatcher<std::pair<T1, T2>, Policy> {
    template <typename Os>
    static void dump(Os& os, const std::pair<T1, T2>& p, bool pretty = Policy::pretty_print, 
                    int indent = 0) 
    {
        os << '[';
        if (pretty) os << '\n';
        if (pretty) json_detail::output_indent(os, indent + Policy::indent_step);
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

template <typename... Ts, typename Policy>
struct JsonDispatcher<std::tuple<Ts...>, Policy> {
    template <typename Os>
    static void dump(Os& os, const std::tuple<Ts...>& tup, bool pretty = Policy::pretty_print, 
                    int indent = 0) 
    {
        json_detail::dump_tuple_impl<Os, Policy>(os, tup, 
                                                  std::make_index_sequence<sizeof...(Ts)>(), 
                                                  pretty, indent);
    }
};

template <typename T, typename Policy>
struct JsonDispatcher<T, Policy, std::enable_if_t<json_detail::IsIterable<T>::value && 
                                                   !std::is_same_v<std::decay_t<T>, std::string> && 
                                                   !json_detail::HasMappedType<T>::value>> 
{
    template <typename Os>
    static void dump(Os& os, const T& cont, bool pretty = Policy::pretty_print, int indent = 0) 
    {
        json_detail::check_dump_depth<Policy>(indent);
        os << '[';
        if (pretty && !cont.empty()) os << '\n';
        bool first = true;
        for (const auto& elem : cont) 
        {
            if (!first) os << ',';
            if (pretty) 
            { 
                os << '\n'; 
                json_detail::output_indent(os, indent + Policy::indent_step); 
            }
            first = false;
            JsonDispatcher<json_detail::ContainerValueT<T>, Policy>::dump(os, elem, pretty, 
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

template <typename T, typename Policy>
struct JsonDispatcher<T, Policy, std::enable_if_t<json_detail::IsIterable<T>::value && 
                                                   !std::is_same_v<std::decay_t<T>, std::string> && 
                                                   json_detail::HasMappedType<T>::value>> 
{
    template <typename Os>
    static void dump(Os& os, const T& cont, bool pretty = Policy::pretty_print, int indent = 0) 
    {
        json_detail::check_dump_depth<Policy>(indent);
        os << '{';
        if (pretty && !cont.empty()) os << '\n';
        bool first = true;
        for (const auto& elem : cont) 
        {
            if (!first) os << ',';
            if (pretty) 
            { 
                os << '\n'; 
                json_detail::output_indent(os, indent + Policy::indent_step); 
            }
            first = false;
            if constexpr (std::is_convertible_v<typename T::key_type, std::string_view>) 
            {
                json_detail::escape_string<Os, Policy>(os, elem.first);
            } 
            else 
            {
                std::ostringstream key_stream;
                key_stream << elem.first;
                json_detail::escape_string<Os, Policy>(os, key_stream.str());
            }
            os << (pretty ? " : " : ":");
            JsonDispatcher<typename T::mapped_type, Policy>::dump(os, elem.second, pretty, 
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

template <typename Policy>
struct JsonDispatcher<JsonValue, Policy> {
    template <typename Os>
    static void dump(Os& os, const JsonValue& val, bool pretty = Policy::pretty_print, 
                    int indent = 0) 
    {
        std::visit([&](auto&& arg) 
        {
            JsonDispatcher<std::decay_t<decltype(arg)>, Policy>::dump(os, arg, pretty, indent);
        }, val);
    }
};

namespace json_detail {
    template <typename Os, typename Policy, typename Tuple, std::size_t... I>
    void dump_tuple_impl(Os& os, const Tuple& tup, std::index_sequence<I...>, 
                        bool pretty, int indent) 
    {
        os << '[';
        if (pretty && sizeof...(I) > 0) os << '\n';
        bool first = true;
        (..., ([&]() 
        {
            if (!first) os << ',';
            if (pretty) 
            { 
                os << '\n'; 
                output_indent(os, indent + Policy::indent_step); 
            }
            first = false;
            JsonDispatcher<std::tuple_element_t<I, Tuple>, Policy>::dump(os, std::get<I>(tup), 
                                                                          pretty, indent + 
                                                                          Policy::indent_step);
        })());
        if (pretty && sizeof...(I) > 0) 
        { 
            os << '\n'; 
            output_indent(os, indent); 
        }
        os << ']';
    }
}

template <typename T, typename Policy = StandardJsonPolicy>
std::string to_json_string(const T& obj, bool pretty = Policy::pretty_print) 
{
    json_detail::PolicyScope<Policy> scope;
    std::ostringstream oss;
    JsonDispatcher<T, Policy>::dump(oss, obj, pretty);
    return oss.str();
}

template <typename T, typename Policy = StandardJsonPolicy, typename Os>
void to_json_stream(Os& os, const T& obj, bool pretty = Policy::pretty_print) 
{
    json_detail::PolicyScope<Policy> scope;
    JsonDispatcher<T, Policy>::dump(os, obj, pretty);
}

inline JsonValue to_json(const JsonValue& value) 
{
    return value;
}

/**
 * @def USING_JSON_LITE
 * @brief Convenience macro to bring JsonLite symbols into scope
 * 
 * @details This macro provides a clean way to import all necessary JsonLite functionality
 * into your namespace. It expands to:
 * - `using namespace fat_p` - Imports JsonValue, JsonObject, JsonArray, etc.
 * - `using fat_p::to_json` - Makes to_json available for ADL
 * - `using fat_p::from_json` - Makes from_json available for ADL
 * 
 * @section usage Recommended Usage
 * Place at the top of your implementation files after including JsonLite.h:
 * @code{.cpp}
 * #include "JsonLite.h"
 * USING_JSON_LITE()
 * 
 * struct Config {
 *     int port;
 *     std::string host;
 * };
 * CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Config, port, host)
 * 
 * int main() {
 *     Config cfg{8080, "localhost"};
 *     save_params("config.json", cfg);  // Functions now in scope
 * }
 * @endcode
 * 
 * @section alternatives Alternatives
 * If you prefer explicit qualification or want to avoid namespace pollution:
 * @code{.cpp}
 * #include "JsonLite.h"
 * // Don't use USING_JSON_LITE()
 * 
 * // Use explicit qualification
 * fat_p::save_params("config.json", cfg);
 * 
 * // Or selective using declarations
 * using fat_p::JsonValue;
 * using fat_p::save_params;
 * using fat_p::load_params;
 * @endcode
 * 
 * @note This macro is safe to use in .cpp files but avoid in header files
 * @note It enables ADL for to_json/from_json, which is required for the macros to work
 * 
 * @see CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE
 * @see to_json
 * @see from_json
 */
#define USING_JSON_LITE() \
    using namespace fat_p; \
    using fat_p::to_json; \
    using fat_p::from_json;

#define CPP_JSON_EXPAND(x) x

#define CPP_JSON_ARG_COUNT_IMPL(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32,_33,_34,_35,_36,_37,_38,_39,_40,_41,_42,_43,_44,_45,_46,_47,_48,_49,_50,N,...) N
#define CPP_JSON_ARG_COUNT(...) CPP_JSON_EXPAND(CPP_JSON_ARG_COUNT_IMPL(__VA_ARGS__,50,49,48,47,46,45,44,43,42,41,40,39,38,37,36,35,34,33,32,31,30,29,28,27,26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1))

#define CPP_JSON_CAT(a, b) CPP_JSON_CAT_IMPL(a, b)
#define CPP_JSON_CAT_IMPL(a, b) a##b

#define CPP_JSON_TO_FIELD(field) \
    do { obj[#field] = json_detail::to_json_value(value.field); } while(0)

#define CPP_JSON_FROM_FIELD(field) \
    do { \
        if (auto it = obj.find(#field); it != obj.end()) { \
            try { \
                from_json(it->second, value.field); \
            } catch (const std::exception& e) { \
                json_enforce(false, \
                        "Error deserializing field", \
                        "field", #field, \
                        "error", e.what()); \
            } \
        } else if constexpr (!fat_p::json_detail::is_optional_v<decltype((value.field))>) { \
            json_enforce(false, "Required field missing", "field", #field); \
        } \
    } while(0)

#define CPP_JSON_FROM_FIELD_OPT(field) \
    do { \
        if (auto it = obj.find(#field); it != obj.end()) { \
            from_json(it->second, value.field); \
        } \
    } while(0)

/**
 * @section macro_system Macro-Based Serialization System
 * 
 * @details The following macros (CPP_JSON_APPLY_1 through CPP_JSON_APPLY_50) implement
 * variadic macro expansion for C++17. This is necessary because C++17 lacks reflection
 * capabilities, requiring compile-time field iteration via preprocessor metaprogramming.
 * 
 * @subsection limitations Current Limitations
 * 
 * **Maximum Field Count**: 50 fields per struct
 * - Structs with more than 50 fields will trigger CPP_JSON_APPLY_51 static_assert
 * - This is a fundamental limitation of the preprocessor-based approach
 * 
 * **Compilation Performance**:
 * - Deep macro expansion (50 levels) increases compilation time
 * - Each additional field adds marginally to compile time
 * - For large projects with many serialized structs, consider:
 *   * Using precompiled headers
 *   * Splitting large structs into nested sub-structures
 *   * Limiting the number of fields in frequently-used types
 * 
 * **Error Messages**:
 * - Macro expansion errors can be difficult to debug
 * - Use -fmacro-backtrace-limit=0 (GCC/Clang) for full macro expansion traces
 * - Common error: forgetting to add a field to the macro invocation
 * 
 * @subsection workarounds Workarounds for Exceeding Limits
 * 
 * If you have a struct with more than 50 fields, use one of these approaches:
 * 
 * **1. Nested Structures** (Recommended):
 * @code{.cpp}
 * struct Address {
 *     std::string street, city, state, zip;
 * };
 * CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Address, street, city, state, zip)
 * 
 * struct Person {
 *     std::string name;
 *     Address home_address;
 *     Address work_address;
 *     // ... more fields
 * };
 * CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Person, name, home_address, work_address, ...)
 * @endcode
 * 
 * **2. Manual to_json/from_json Functions**:
 * @code{.cpp}
 * struct LargeStruct {
 *     // 100+ fields
 * };
 * 
 * void to_json(JsonValue& j, const LargeStruct& value) {
 *     JsonObject obj;
 *     obj["field1"] = to_json(value.field1);
 *     obj["field2"] = to_json(value.field2);
 *     // ... manual implementation for all fields
 *     j = std::move(obj);
 * }
 * 
 * void from_json(const JsonValue& j, LargeStruct& value) {
 *     const auto& obj = std::get<JsonObject>(j);
 *     from_json(obj.at("field1"), value.field1);
 *     from_json(obj.at("field2"), value.field2);
 *     // ... manual implementation for all fields
 * }
 * @endcode
 * 
 * **3. Partial Macro Usage**:
 * @code{.cpp}
 * // Use macros for frequently-accessed fields, manual for others
 * struct Config {
 *     int critical_field1;
 *     int critical_field2;
 *     // ... 48 more critical fields
 *     std::map<std::string, std::string> extended_properties;  // catchall
 * };
 * CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Config, critical_field1, ..., extended_properties)
 * @endcode
 * 
 * @subsection future_plans Future Enhancement Plans
 * 
 * **C++20 Migration**:
 * - Could use `__VA_OPT__` for cleaner macro implementation
 * - Still limited by preprocessor, but improved error messages
 * 
 * **C++23/26 Reflection** (Future):
 * - Static reflection (P2996) would eliminate macros entirely
 * - Automatic field iteration without preprocessor metaprogramming
 * - This represents the long-term solution for removing these limitations
 * 
 * @note Until static reflection is available in a widely-adopted C++ standard,
 *       the macro approach represents the best balance of functionality,
 *       portability, and zero-dependency design for C++17 projects.
 */

#define CPP_JSON_APPLY_1(macro, x1) macro(x1);
#define CPP_JSON_APPLY_2(macro, x1, x2) macro(x1); macro(x2);
#define CPP_JSON_APPLY_3(macro, x1, x2, x3) macro(x1); macro(x2); macro(x3);
#define CPP_JSON_APPLY_4(macro, x1, x2, x3, x4) macro(x1); macro(x2); macro(x3); macro(x4);
#define CPP_JSON_APPLY_5(macro, x1, x2, x3, x4, x5) macro(x1); macro(x2); macro(x3); macro(x4); macro(x5);
#define CPP_JSON_APPLY_6(macro, x1, x2, x3, x4, x5, x6) macro(x1); macro(x2); macro(x3); macro(x4); macro(x5); macro(x6);
#define CPP_JSON_APPLY_7(macro, x1, x2, x3, x4, x5, x6, x7) macro(x1); macro(x2); macro(x3); macro(x4); macro(x5); macro(x6); macro(x7);
#define CPP_JSON_APPLY_8(macro, x1, x2, x3, x4, x5, x6, x7, x8) macro(x1); macro(x2); macro(x3); macro(x4); macro(x5); macro(x6); macro(x7); macro(x8);
#define CPP_JSON_APPLY_9(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9) macro(x1); macro(x2); macro(x3); macro(x4); macro(x5); macro(x6); macro(x7); macro(x8); macro(x9);
#define CPP_JSON_APPLY_10(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10) macro(x1); macro(x2); macro(x3); macro(x4); macro(x5); macro(x6); macro(x7); macro(x8); macro(x9); macro(x10);
#define CPP_JSON_APPLY_11(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11) macro(x1); macro(x2); macro(x3); macro(x4); macro(x5); macro(x6); macro(x7); macro(x8); macro(x9); macro(x10); macro(x11);
#define CPP_JSON_APPLY_12(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12) macro(x1); macro(x2); macro(x3); macro(x4); macro(x5); macro(x6); macro(x7); macro(x8); macro(x9); macro(x10); macro(x11); macro(x12);
#define CPP_JSON_APPLY_13(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13) macro(x1); macro(x2); macro(x3); macro(x4); macro(x5); macro(x6); macro(x7); macro(x8); macro(x9); macro(x10); macro(x11); macro(x12); macro(x13);
#define CPP_JSON_APPLY_14(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14) macro(x1); macro(x2); macro(x3); macro(x4); macro(x5); macro(x6); macro(x7); macro(x8); macro(x9); macro(x10); macro(x11); macro(x12); macro(x13); macro(x14);
#define CPP_JSON_APPLY_15(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15) macro(x1); macro(x2); macro(x3); macro(x4); macro(x5); macro(x6); macro(x7); macro(x8); macro(x9); macro(x10); macro(x11); macro(x12); macro(x13); macro(x14); macro(x15);
#define CPP_JSON_APPLY_16(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16) macro(x1); macro(x2); macro(x3); macro(x4); macro(x5); macro(x6); macro(x7); macro(x8); macro(x9); macro(x10); macro(x11); macro(x12); macro(x13); macro(x14); macro(x15); macro(x16);
#define CPP_JSON_APPLY_17(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17) macro(x1); macro(x2); macro(x3); macro(x4); macro(x5); macro(x6); macro(x7); macro(x8); macro(x9); macro(x10); macro(x11); macro(x12); macro(x13); macro(x14); macro(x15); macro(x16); macro(x17);
#define CPP_JSON_APPLY_18(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18) macro(x1); macro(x2); macro(x3); macro(x4); macro(x5); macro(x6); macro(x7); macro(x8); macro(x9); macro(x10); macro(x11); macro(x12); macro(x13); macro(x14); macro(x15); macro(x16); macro(x17); macro(x18);
#define CPP_JSON_APPLY_19(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19) macro(x1); macro(x2); macro(x3); macro(x4); macro(x5); macro(x6); macro(x7); macro(x8); macro(x9); macro(x10); macro(x11); macro(x12); macro(x13); macro(x14); macro(x15); macro(x16); macro(x17); macro(x18); macro(x19);
#define CPP_JSON_APPLY_20(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20) macro(x1); macro(x2); macro(x3); macro(x4); macro(x5); macro(x6); macro(x7); macro(x8); macro(x9); macro(x10); macro(x11); macro(x12); macro(x13); macro(x14); macro(x15); macro(x16); macro(x17); macro(x18); macro(x19); macro(x20);
#define CPP_JSON_APPLY_21(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21) macro(x1); macro(x2); macro(x3); macro(x4); macro(x5); macro(x6); macro(x7); macro(x8); macro(x9); macro(x10); macro(x11); macro(x12); macro(x13); macro(x14); macro(x15); macro(x16); macro(x17); macro(x18); macro(x19); macro(x20); macro(x21);
#define CPP_JSON_APPLY_22(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22) macro(x1); macro(x2); macro(x3); macro(x4); macro(x5); macro(x6); macro(x7); macro(x8); macro(x9); macro(x10); macro(x11); macro(x12); macro(x13); macro(x14); macro(x15); macro(x16); macro(x17); macro(x18); macro(x19); macro(x20); macro(x21); macro(x22);
#define CPP_JSON_APPLY_23(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23) macro(x1); macro(x2); macro(x3); macro(x4); macro(x5); macro(x6); macro(x7); macro(x8); macro(x9); macro(x10); macro(x11); macro(x12); macro(x13); macro(x14); macro(x15); macro(x16); macro(x17); macro(x18); macro(x19); macro(x20); macro(x21); macro(x22); macro(x23);
#define CPP_JSON_APPLY_24(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24) macro(x1); macro(x2); macro(x3); macro(x4); macro(x5); macro(x6); macro(x7); macro(x8); macro(x9); macro(x10); macro(x11); macro(x12); macro(x13); macro(x14); macro(x15); macro(x16); macro(x17); macro(x18); macro(x19); macro(x20); macro(x21); macro(x22); macro(x23); macro(x24);
#define CPP_JSON_APPLY_25(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25) macro(x1); macro(x2); macro(x3); macro(x4); macro(x5); macro(x6); macro(x7); macro(x8); macro(x9); macro(x10); macro(x11); macro(x12); macro(x13); macro(x14); macro(x15); macro(x16); macro(x17); macro(x18); macro(x19); macro(x20); macro(x21); macro(x22); macro(x23); macro(x24); macro(x25);
#define CPP_JSON_APPLY_26(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26) macro(x1); macro(x2); macro(x3); macro(x4); macro(x5); macro(x6); macro(x7); macro(x8); macro(x9); macro(x10); macro(x11); macro(x12); macro(x13); macro(x14); macro(x15); macro(x16); macro(x17); macro(x18); macro(x19); macro(x20); macro(x21); macro(x22); macro(x23); macro(x24); macro(x25); macro(x26);
#define CPP_JSON_APPLY_27(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27) macro(x1); macro(x2); macro(x3); macro(x4); macro(x5); macro(x6); macro(x7); macro(x8); macro(x9); macro(x10); macro(x11); macro(x12); macro(x13); macro(x14); macro(x15); macro(x16); macro(x17); macro(x18); macro(x19); macro(x20); macro(x21); macro(x22); macro(x23); macro(x24); macro(x25); macro(x26); macro(x27);
#define CPP_JSON_APPLY_28(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28) macro(x1); macro(x2); macro(x3); macro(x4); macro(x5); macro(x6); macro(x7); macro(x8); macro(x9); macro(x10); macro(x11); macro(x12); macro(x13); macro(x14); macro(x15); macro(x16); macro(x17); macro(x18); macro(x19); macro(x20); macro(x21); macro(x22); macro(x23); macro(x24); macro(x25); macro(x26); macro(x27); macro(x28);
#define CPP_JSON_APPLY_29(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29) macro(x1); macro(x2); macro(x3); macro(x4); macro(x5); macro(x6); macro(x7); macro(x8); macro(x9); macro(x10); macro(x11); macro(x12); macro(x13); macro(x14); macro(x15); macro(x16); macro(x17); macro(x18); macro(x19); macro(x20); macro(x21); macro(x22); macro(x23); macro(x24); macro(x25); macro(x26); macro(x27); macro(x28); macro(x29);
#define CPP_JSON_APPLY_30(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30) macro(x1); macro(x2); macro(x3); macro(x4); macro(x5); macro(x6); macro(x7); macro(x8); macro(x9); macro(x10); macro(x11); macro(x12); macro(x13); macro(x14); macro(x15); macro(x16); macro(x17); macro(x18); macro(x19); macro(x20); macro(x21); macro(x22); macro(x23); macro(x24); macro(x25); macro(x26); macro(x27); macro(x28); macro(x29); macro(x30);
#define CPP_JSON_APPLY_31(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31) macro(x1); macro(x2); macro(x3); macro(x4); macro(x5); macro(x6); macro(x7); macro(x8); macro(x9); macro(x10); macro(x11); macro(x12); macro(x13); macro(x14); macro(x15); macro(x16); macro(x17); macro(x18); macro(x19); macro(x20); macro(x21); macro(x22); macro(x23); macro(x24); macro(x25); macro(x26); macro(x27); macro(x28); macro(x29); macro(x30); macro(x31);
#define CPP_JSON_APPLY_32(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31, x32) macro(x1); macro(x2); macro(x3); macro(x4); macro(x5); macro(x6); macro(x7); macro(x8); macro(x9); macro(x10); macro(x11); macro(x12); macro(x13); macro(x14); macro(x15); macro(x16); macro(x17); macro(x18); macro(x19); macro(x20); macro(x21); macro(x22); macro(x23); macro(x24); macro(x25); macro(x26); macro(x27); macro(x28); macro(x29); macro(x30); macro(x31); macro(x32);
#define CPP_JSON_APPLY_33(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31, x32, x33) macro(x1); macro(x2); macro(x3); macro(x4); macro(x5); macro(x6); macro(x7); macro(x8); macro(x9); macro(x10); macro(x11); macro(x12); macro(x13); macro(x14); macro(x15); macro(x16); macro(x17); macro(x18); macro(x19); macro(x20); macro(x21); macro(x22); macro(x23); macro(x24); macro(x25); macro(x26); macro(x27); macro(x28); macro(x29); macro(x30); macro(x31); macro(x32); macro(x33);
#define CPP_JSON_APPLY_34(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31, x32, x33, x34) macro(x1); macro(x2); macro(x3); macro(x4); macro(x5); macro(x6); macro(x7); macro(x8); macro(x9); macro(x10); macro(x11); macro(x12); macro(x13); macro(x14); macro(x15); macro(x16); macro(x17); macro(x18); macro(x19); macro(x20); macro(x21); macro(x22); macro(x23); macro(x24); macro(x25); macro(x26); macro(x27); macro(x28); macro(x29); macro(x30); macro(x31); macro(x32); macro(x33); macro(x34);
#define CPP_JSON_APPLY_35(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31, x32, x33, x34, x35) macro(x1); macro(x2); macro(x3); macro(x4); macro(x5); macro(x6); macro(x7); macro(x8); macro(x9); macro(x10); macro(x11); macro(x12); macro(x13); macro(x14); macro(x15); macro(x16); macro(x17); macro(x18); macro(x19); macro(x20); macro(x21); macro(x22); macro(x23); macro(x24); macro(x25); macro(x26); macro(x27); macro(x28); macro(x29); macro(x30); macro(x31); macro(x32); macro(x33); macro(x34); macro(x35);
#define CPP_JSON_APPLY_36(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31, x32, x33, x34, x35, x36) macro(x1); macro(x2); macro(x3); macro(x4); macro(x5); macro(x6); macro(x7); macro(x8); macro(x9); macro(x10); macro(x11); macro(x12); macro(x13); macro(x14); macro(x15); macro(x16); macro(x17); macro(x18); macro(x19); macro(x20); macro(x21); macro(x22); macro(x23); macro(x24); macro(x25); macro(x26); macro(x27); macro(x28); macro(x29); macro(x30); macro(x31); macro(x32); macro(x33); macro(x34); macro(x35); macro(x36);
#define CPP_JSON_APPLY_37(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31, x32, x33, x34, x35, x36, x37) macro(x1); macro(x2); macro(x3); macro(x4); macro(x5); macro(x6); macro(x7); macro(x8); macro(x9); macro(x10); macro(x11); macro(x12); macro(x13); macro(x14); macro(x15); macro(x16); macro(x17); macro(x18); macro(x19); macro(x20); macro(x21); macro(x22); macro(x23); macro(x24); macro(x25); macro(x26); macro(x27); macro(x28); macro(x29); macro(x30); macro(x31); macro(x32); macro(x33); macro(x34); macro(x35); macro(x36); macro(x37);
#define CPP_JSON_APPLY_38(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31, x32, x33, x34, x35, x36, x37, x38) macro(x1); macro(x2); macro(x3); macro(x4); macro(x5); macro(x6); macro(x7); macro(x8); macro(x9); macro(x10); macro(x11); macro(x12); macro(x13); macro(x14); macro(x15); macro(x16); macro(x17); macro(x18); macro(x19); macro(x20); macro(x21); macro(x22); macro(x23); macro(x24); macro(x25); macro(x26); macro(x27); macro(x28); macro(x29); macro(x30); macro(x31); macro(x32); macro(x33); macro(x34); macro(x35); macro(x36); macro(x37); macro(x38);
#define CPP_JSON_APPLY_39(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31, x32, x33, x34, x35, x36, x37, x38, x39) macro(x1); macro(x2); macro(x3); macro(x4); macro(x5); macro(x6); macro(x7); macro(x8); macro(x9); macro(x10); macro(x11); macro(x12); macro(x13); macro(x14); macro(x15); macro(x16); macro(x17); macro(x18); macro(x19); macro(x20); macro(x21); macro(x22); macro(x23); macro(x24); macro(x25); macro(x26); macro(x27); macro(x28); macro(x29); macro(x30); macro(x31); macro(x32); macro(x33); macro(x34); macro(x35); macro(x36); macro(x37); macro(x38); macro(x39);
#define CPP_JSON_APPLY_40(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31, x32, x33, x34, x35, x36, x37, x38, x39, x40) macro(x1); macro(x2); macro(x3); macro(x4); macro(x5); macro(x6); macro(x7); macro(x8); macro(x9); macro(x10); macro(x11); macro(x12); macro(x13); macro(x14); macro(x15); macro(x16); macro(x17); macro(x18); macro(x19); macro(x20); macro(x21); macro(x22); macro(x23); macro(x24); macro(x25); macro(x26); macro(x27); macro(x28); macro(x29); macro(x30); macro(x31); macro(x32); macro(x33); macro(x34); macro(x35); macro(x36); macro(x37); macro(x38); macro(x39); macro(x40);
#define CPP_JSON_APPLY_41(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31, x32, x33, x34, x35, x36, x37, x38, x39, x40, x41) macro(x1); macro(x2); macro(x3); macro(x4); macro(x5); macro(x6); macro(x7); macro(x8); macro(x9); macro(x10); macro(x11); macro(x12); macro(x13); macro(x14); macro(x15); macro(x16); macro(x17); macro(x18); macro(x19); macro(x20); macro(x21); macro(x22); macro(x23); macro(x24); macro(x25); macro(x26); macro(x27); macro(x28); macro(x29); macro(x30); macro(x31); macro(x32); macro(x33); macro(x34); macro(x35); macro(x36); macro(x37); macro(x38); macro(x39); macro(x40); macro(x41);
#define CPP_JSON_APPLY_42(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31, x32, x33, x34, x35, x36, x37, x38, x39, x40, x41, x42) macro(x1); macro(x2); macro(x3); macro(x4); macro(x5); macro(x6); macro(x7); macro(x8); macro(x9); macro(x10); macro(x11); macro(x12); macro(x13); macro(x14); macro(x15); macro(x16); macro(x17); macro(x18); macro(x19); macro(x20); macro(x21); macro(x22); macro(x23); macro(x24); macro(x25); macro(x26); macro(x27); macro(x28); macro(x29); macro(x30); macro(x31); macro(x32); macro(x33); macro(x34); macro(x35); macro(x36); macro(x37); macro(x38); macro(x39); macro(x40); macro(x41); macro(x42);
#define CPP_JSON_APPLY_43(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31, x32, x33, x34, x35, x36, x37, x38, x39, x40, x41, x42, x43) macro(x1); macro(x2); macro(x3); macro(x4); macro(x5); macro(x6); macro(x7); macro(x8); macro(x9); macro(x10); macro(x11); macro(x12); macro(x13); macro(x14); macro(x15); macro(x16); macro(x17); macro(x18); macro(x19); macro(x20); macro(x21); macro(x22); macro(x23); macro(x24); macro(x25); macro(x26); macro(x27); macro(x28); macro(x29); macro(x30); macro(x31); macro(x32); macro(x33); macro(x34); macro(x35); macro(x36); macro(x37); macro(x38); macro(x39); macro(x40); macro(x41); macro(x42); macro(x43);
#define CPP_JSON_APPLY_44(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31, x32, x33, x34, x35, x36, x37, x38, x39, x40, x41, x42, x43, x44) macro(x1); macro(x2); macro(x3); macro(x4); macro(x5); macro(x6); macro(x7); macro(x8); macro(x9); macro(x10); macro(x11); macro(x12); macro(x13); macro(x14); macro(x15); macro(x16); macro(x17); macro(x18); macro(x19); macro(x20); macro(x21); macro(x22); macro(x23); macro(x24); macro(x25); macro(x26); macro(x27); macro(x28); macro(x29); macro(x30); macro(x31); macro(x32); macro(x33); macro(x34); macro(x35); macro(x36); macro(x37); macro(x38); macro(x39); macro(x40); macro(x41); macro(x42); macro(x43); macro(x44);
#define CPP_JSON_APPLY_45(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31, x32, x33, x34, x35, x36, x37, x38, x39, x40, x41, x42, x43, x44, x45) macro(x1); macro(x2); macro(x3); macro(x4); macro(x5); macro(x6); macro(x7); macro(x8); macro(x9); macro(x10); macro(x11); macro(x12); macro(x13); macro(x14); macro(x15); macro(x16); macro(x17); macro(x18); macro(x19); macro(x20); macro(x21); macro(x22); macro(x23); macro(x24); macro(x25); macro(x26); macro(x27); macro(x28); macro(x29); macro(x30); macro(x31); macro(x32); macro(x33); macro(x34); macro(x35); macro(x36); macro(x37); macro(x38); macro(x39); macro(x40); macro(x41); macro(x42); macro(x43); macro(x44); macro(x45);
#define CPP_JSON_APPLY_46(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31, x32, x33, x34, x35, x36, x37, x38, x39, x40, x41, x42, x43, x44, x45, x46) macro(x1); macro(x2); macro(x3); macro(x4); macro(x5); macro(x6); macro(x7); macro(x8); macro(x9); macro(x10); macro(x11); macro(x12); macro(x13); macro(x14); macro(x15); macro(x16); macro(x17); macro(x18); macro(x19); macro(x20); macro(x21); macro(x22); macro(x23); macro(x24); macro(x25); macro(x26); macro(x27); macro(x28); macro(x29); macro(x30); macro(x31); macro(x32); macro(x33); macro(x34); macro(x35); macro(x36); macro(x37); macro(x38); macro(x39); macro(x40); macro(x41); macro(x42); macro(x43); macro(x44); macro(x45); macro(x46);
#define CPP_JSON_APPLY_47(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31, x32, x33, x34, x35, x36, x37, x38, x39, x40, x41, x42, x43, x44, x45, x46, x47) macro(x1); macro(x2); macro(x3); macro(x4); macro(x5); macro(x6); macro(x7); macro(x8); macro(x9); macro(x10); macro(x11); macro(x12); macro(x13); macro(x14); macro(x15); macro(x16); macro(x17); macro(x18); macro(x19); macro(x20); macro(x21); macro(x22); macro(x23); macro(x24); macro(x25); macro(x26); macro(x27); macro(x28); macro(x29); macro(x30); macro(x31); macro(x32); macro(x33); macro(x34); macro(x35); macro(x36); macro(x37); macro(x38); macro(x39); macro(x40); macro(x41); macro(x42); macro(x43); macro(x44); macro(x45); macro(x46); macro(x47);
#define CPP_JSON_APPLY_48(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31, x32, x33, x34, x35, x36, x37, x38, x39, x40, x41, x42, x43, x44, x45, x46, x47, x48) macro(x1); macro(x2); macro(x3); macro(x4); macro(x5); macro(x6); macro(x7); macro(x8); macro(x9); macro(x10); macro(x11); macro(x12); macro(x13); macro(x14); macro(x15); macro(x16); macro(x17); macro(x18); macro(x19); macro(x20); macro(x21); macro(x22); macro(x23); macro(x24); macro(x25); macro(x26); macro(x27); macro(x28); macro(x29); macro(x30); macro(x31); macro(x32); macro(x33); macro(x34); macro(x35); macro(x36); macro(x37); macro(x38); macro(x39); macro(x40); macro(x41); macro(x42); macro(x43); macro(x44); macro(x45); macro(x46); macro(x47); macro(x48);
#define CPP_JSON_APPLY_49(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31, x32, x33, x34, x35, x36, x37, x38, x39, x40, x41, x42, x43, x44, x45, x46, x47, x48, x49) macro(x1); macro(x2); macro(x3); macro(x4); macro(x5); macro(x6); macro(x7); macro(x8); macro(x9); macro(x10); macro(x11); macro(x12); macro(x13); macro(x14); macro(x15); macro(x16); macro(x17); macro(x18); macro(x19); macro(x20); macro(x21); macro(x22); macro(x23); macro(x24); macro(x25); macro(x26); macro(x27); macro(x28); macro(x29); macro(x30); macro(x31); macro(x32); macro(x33); macro(x34); macro(x35); macro(x36); macro(x37); macro(x38); macro(x39); macro(x40); macro(x41); macro(x42); macro(x43); macro(x44); macro(x45); macro(x46); macro(x47); macro(x48); macro(x49);
#define CPP_JSON_APPLY_50(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31, x32, x33, x34, x35, x36, x37, x38, x39, x40, x41, x42, x43, x44, x45, x46, x47, x48, x49, x50) macro(x1); macro(x2); macro(x3); macro(x4); macro(x5); macro(x6); macro(x7); macro(x8); macro(x9); macro(x10); macro(x11); macro(x12); macro(x13); macro(x14); macro(x15); macro(x16); macro(x17); macro(x18); macro(x19); macro(x20); macro(x21); macro(x22); macro(x23); macro(x24); macro(x25); macro(x26); macro(x27); macro(x28); macro(x29); macro(x30); macro(x31); macro(x32); macro(x33); macro(x34); macro(x35); macro(x36); macro(x37); macro(x38); macro(x39); macro(x40); macro(x41); macro(x42); macro(x43); macro(x44); macro(x45); macro(x46); macro(x47); macro(x48); macro(x49); macro(x50);

#define CPP_JSON_APPLY_51(...) \
    static_assert(false, \
        "CPP_JSON_DEFINE_TYPE_* macros support a maximum of 50 fields. " \
        "Your struct has 51 or more fields. Solutions: " \
        "1) Use nested structs to group related fields, " \
        "2) Write custom to_json/from_json functions, " \
        "3) Split into multiple smaller structs.")

#define CPP_JSON_FOR_EACH(macro, ...) \
    CPP_JSON_EXPAND(CPP_JSON_CAT(CPP_JSON_APPLY_, CPP_JSON_ARG_COUNT(__VA_ARGS__))(macro, __VA_ARGS__))

/**
 * @def CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE
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
 * CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Config, port, host, enabled)
 * 
 * // Now you can:
 * Config cfg{8080, "localhost", true};
 * JsonValue j = to_json(cfg);           // Serialize
 * Config loaded = from_json<Config>(j);  // Deserialize
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
 * CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Address, city, zip)
 * 
 * struct Person {
 *     std::string name;
 *     Address address;  // Nested struct
 * };
 * CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Person, name, address)
 * @endcode
 * 
 * @param Type The type name (struct or class)
 * @param ... Field names (up to 50 fields supported)
 * 
 * @note Place macro call in the same namespace as Type (for ADL)
 * @note All fields must be public or the macro must be a friend
 * @note For private fields, use CPP_JSON_DEFINE_TYPE_INTRUSIVE instead
 * 
 * @warning Fields must be listed in the exact order you want them serialized
 * 
 * @see CPP_JSON_DEFINE_TYPE_INTRUSIVE
 * @see CPP_JSON_DEFINE_TYPE_OPTIONAL
 * @see to_json
 * @see from_json
 */
#define CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Type, ...)                                       \
[[maybe_unused]] inline void to_json(fat_p::JsonValue& j, const Type& value) {             \
    fat_p::JsonObject obj;                                                                  \
    CPP_JSON_FOR_EACH(CPP_JSON_TO_FIELD, __VA_ARGS__)                                       \
    j = std::move(obj);                                                                     \
}                                                                                           \
[[maybe_unused]] inline void from_json(const fat_p::JsonValue& j, Type& value) {           \
    json_enforce(j.is_object(), "JSON type mismatch", "expected", "object",                      \
            "got", fat_p::json_detail::type_name(j));                                       \
    const auto& obj = std::get<fat_p::JsonObject>(j);                                       \
    CPP_JSON_FOR_EACH(CPP_JSON_FROM_FIELD, __VA_ARGS__)                                     \
}

/**
 * @def CPP_JSON_DEFINE_TYPE_OPTIONAL
 * @brief Define JSON serialization with optional fields
 * 
 * @details Similar to CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE, but makes all fields optional
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
 * CPP_JSON_DEFINE_TYPE_OPTIONAL(Settings, port, host, debug)
 * 
 * // This JSON is valid (only some fields present)
 * Settings s = from_json<Settings>(parse_json(R"({"port": 9000})"));
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
 * @see CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE
 * @see std::optional
 */
#define CPP_JSON_DEFINE_TYPE_OPTIONAL(Type, ...)                                                \
[[maybe_unused]] inline void to_json(fat_p::JsonValue& j, const Type& value) {                 \
    fat_p::JsonObject obj;                                                                      \
    CPP_JSON_FOR_EACH(CPP_JSON_TO_FIELD, __VA_ARGS__)                                           \
    j = std::move(obj);                                                                         \
}                                                                                               \
[[maybe_unused]] inline void from_json(const fat_p::JsonValue& j, Type& value) {               \
    json_enforce(j.is_object(), "JSON type mismatch", "expected", "object",                          \
            "got", fat_p::json_detail::type_name(j));                                           \
    const auto& obj = std::get<fat_p::JsonObject>(j);                                           \
    CPP_JSON_FOR_EACH(CPP_JSON_FROM_FIELD_OPT, __VA_ARGS__)                                     \
}

/**
 * @def CPP_JSON_DEFINE_TYPE_INTRUSIVE
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
 *     std::string username_;
 *     std::string password_hash_;
 *     int user_id_;
 *     
 * public:
 *     User() = default;
 *     User(std::string name, std::string hash, int id) 
 *         : username_(std::move(name))
 *         , password_hash_(std::move(hash))
 *         , user_id_(id) {}
 *     
 *     // Define serialization with access to private members
 *     CPP_JSON_DEFINE_TYPE_INTRUSIVE(User, username_, password_hash_, user_id_)
 * };
 * 
 * // Usage is the same
 * User user{"alice", "hash123", 42};
 * JsonValue j = to_json(user);
 * User loaded = from_json<User>(j);
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
 * @see CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE
 * @see CPP_JSON_DEFINE_TYPE_OPTIONAL
 */
#define CPP_JSON_DEFINE_TYPE_INTRUSIVE(Type, ...)                                               \
[[maybe_unused]] friend void to_json(fat_p::JsonValue& j, const Type& value) {                 \
    fat_p::JsonObject obj;                                                                      \
    CPP_JSON_FOR_EACH(CPP_JSON_TO_FIELD, __VA_ARGS__)                                           \
    j = std::move(obj);                                                                         \
}                                                                                               \
[[maybe_unused]] friend void from_json(const fat_p::JsonValue& j, Type& value) {               \
    json_enforce(j.is_object(), "JSON type mismatch", "expected", "object",                          \
            "got", fat_p::json_detail::type_name(j));                                           \
    const auto& obj = std::get<fat_p::JsonObject>(j);                                           \
    CPP_JSON_FOR_EACH(CPP_JSON_FROM_FIELD, __VA_ARGS__)                                         \
}


namespace json_detail {
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
            
            if constexpr (Policy::allow_comments)
            {
                if (s.size() - pos >= 2 && s[pos] == '/' && s[pos + 1] == '/')
                {
                    pos += 2;
                    while (pos < s.size() && s[pos] != '\n') { ++pos; }
                    continue;
                }
                
                if (s.size() - pos >= 2 && s[pos] == '/' && s[pos + 1] == '*')
                {
                    pos += 2;
                    while (pos + 1 < s.size() && !(s[pos] == '*' && s[pos + 1] == '/')) { ++pos; }
                    if (pos + 1 < s.size()) { pos += 2; }
                    continue;
                }
            }
            
            break;
        }
    }

    inline std::string parse_string(std::string_view s, size_t& pos) 
    {
        json_enforce(pos < s.size() && s[pos] == '"',
                "JSON parse error: expected string",
                "position", pos);
        ++pos;
        std::string res;
        res.reserve(64);
        
        while (pos < s.size() && s[pos] != '"') 
        {
            if (s[pos] == '\\') 
            {
                ++pos;
                json_enforce(pos < s.size(),
                        "JSON parse error: invalid escape sequence",
                        "position", pos);
                switch (s[pos]) 
                {
                    case '"': res += '"'; break;
                    case '\\': res += '\\'; break;
                    case '/': res += '/'; break;
                    case 'b': res += '\b'; break;
                    case 'f': res += '\f'; break;
                    case 'n': res += '\n'; break;
                    case 'r': res += '\r'; break;
                    case 't': res += '\t'; break;
                    case 'u': 
                    {
                        json_enforce(pos + 4 < s.size(),
                                "JSON parse error: invalid unicode escape",
                                "position", pos);
                        std::string hex = std::string(s.substr(pos + 1, 4));
                        uint32_t codepoint;
                        try 
                        {
                            codepoint = static_cast<uint32_t>(std::stoul(hex, nullptr, 16));
                        } 
                        catch (...) 
                        {
                            json_enforce(false,
                                    "JSON parse error: invalid unicode hex",
                                    "position", pos,
                                    "hex", hex);
                        }
                        pos += 4;
                        
                        if (codepoint >= 0xD800 && codepoint <= 0xDBFF) 
                        {
                            json_enforce(pos + 6 < s.size() && s[pos + 1] == '\\' && s[pos + 2] == 'u',
                                    "JSON parse error: incomplete surrogate pair",
                                    "position", pos);
                            std::string low_hex = std::string(s.substr(pos + 3, 4));
                            uint32_t low_surrogate;
                            try 
                            {
                                low_surrogate = static_cast<uint32_t>(std::stoul(low_hex, nullptr, 16));
                            } 
                            catch (...) 
                            {
                                json_enforce(false,
                                        "JSON parse error: invalid low surrogate",
                                        "position", pos,
                                        "hex", low_hex);
                            }
                            json_enforce(low_surrogate >= 0xDC00 && low_surrogate <= 0xDFFF,
                                    "JSON parse error: invalid low surrogate value",
                                    "position", pos,
                                    "value", low_surrogate);
                            pos += 6;
                            codepoint = 0x10000 + ((codepoint & 0x3FF) << 10) + (low_surrogate & 0x3FF);
                        } 
                        else if (codepoint >= 0xDC00 && codepoint <= 0xDFFF) 
                        {
                            json_enforce(false,
                                    "JSON parse error: unexpected low surrogate",
                                    "position", pos,
                                    "codepoint", codepoint);
                        }
                        
                        if (codepoint <= 0x7F) 
                        {
                            res += static_cast<char>(codepoint);
                        } 
                        else if (codepoint <= 0x7FF) 
                        {
                            res += static_cast<char>(0xC0 | (codepoint >> 6));
                            res += static_cast<char>(0x80 | (codepoint & 0x3F));
                        } 
                        else if (codepoint <= 0xFFFF) 
                        {
                            res += static_cast<char>(0xE0 | (codepoint >> 12));
                            res += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                            res += static_cast<char>(0x80 | (codepoint & 0x3F));
                        } 
                        else if (codepoint <= 0x10FFFF) 
                        {
                            res += static_cast<char>(0xF0 | (codepoint >> 18));
                            res += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
                            res += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                            res += static_cast<char>(0x80 | (codepoint & 0x3F));
                        } 
                        else 
                        {
                            json_enforce(false,
                                    "JSON parse error: invalid unicode codepoint",
                                    "position", pos,
                                    "codepoint", codepoint);
                        }
                        break;
                    }
                    default: 
                        json_enforce(false,
                                "JSON parse error: invalid escape character",
                                "position", pos,
                                "character", s[pos]);
                }
            } 
            else 
            {
                res += s[pos];
            }
            ++pos;
        }
        json_enforce(pos < s.size() && s[pos] == '"',
                "JSON parse error: unterminated string",
                "position", pos);
        ++pos;
        return res;
    }

    template <typename Policy = StandardJsonPolicy>
    inline JsonValue parse_number(std::string_view s, size_t& pos) 
    {
        size_t start = pos;
        
        if (s.substr(pos, 3) == "NaN") 
        {
            json_enforce(Policy::allow_nan_inf,
                    "JSON parse error: NaN is not valid JSON (use allow_nan_inf policy to enable)",
                    "position", pos);
            pos += 3;
            return std::numeric_limits<double>::quiet_NaN();
        }
        if (s.substr(pos, 8) == "Infinity") 
        {
            json_enforce(Policy::allow_nan_inf,
                    "JSON parse error: Infinity is not valid JSON (use allow_nan_inf policy to enable)",
                    "position", pos);
            pos += 8;
            return std::numeric_limits<double>::infinity();
        }
        if (s.substr(pos, 9) == "-Infinity") 
        {
            json_enforce(Policy::allow_nan_inf,
                    "JSON parse error: -Infinity is not valid JSON (use allow_nan_inf policy to enable)",
                    "position", pos);
            pos += 9;
            return -std::numeric_limits<double>::infinity();
        }
        
        json_enforce(pos < s.size() && (std::isdigit(s[pos]) || s[pos] == '-'),
                "JSON parse error: invalid number",
                "position", pos);
        
        if (s[pos] == '-') 
        {
            ++pos;
            json_enforce(pos < s.size() && std::isdigit(s[pos]),
                    "JSON parse error: invalid number after '-'",
                    "position", pos);
        }
        
        bool has_decimal = false;
        bool has_exponent = false;
        size_t scan_pos = pos;
        
        while (scan_pos < s.size()) 
        {
            char c = s[scan_pos];
            if (c == '.') 
            {
                has_decimal = true;
            } 
            else if (c == 'e' || c == 'E') 
            {
                has_exponent = true;
            } 
            else if (!std::isdigit(c) && c != '+' && c != '-') 
            {
                break;
            }
            ++scan_pos;
        }
        
        std::string num_str = std::string(s.substr(start, scan_pos - start));
        pos = scan_pos;
        
        if (!has_decimal && !has_exponent) 
        {
            try 
            {
                int64_t int_val = std::stoll(num_str);
                return int_val;
            } 
            catch (const std::out_of_range&) 
            {
                try 
                {
                    return std::stod(num_str);
                } 
                catch (...) 
                {
                    json_enforce(false,
                            "JSON parse error: invalid number",
                            "position", start,
                            "value", num_str);
                    unreachable_after_enforce();
                }
            } 
            catch (...) 
            {
                json_enforce(false,
                        "JSON parse error: invalid number",
                        "position", start,
                        "value", num_str);
                unreachable_after_enforce();
            }
        }
        
        try 
        {
            return std::stod(num_str);
        } 
        catch (...) 
        {
            json_enforce(false,
                    "JSON parse error: invalid number",
                    "position", start,
                    "value", num_str);
            unreachable_after_enforce();
        }
    }

    template <typename Policy>
    JsonValue parse_value(std::string_view s, size_t& pos, size_t depth = 0);

    template <typename Policy = StandardJsonPolicy>
    inline JsonArray parse_array(std::string_view s, size_t& pos, size_t depth) 
    {
        json_enforce(depth <= Policy::max_parse_depth,
                "JSON parse error: maximum nesting depth exceeded",
                "position", pos,
                "max_depth", Policy::max_parse_depth);
        JsonArray arr;
        ++pos;
        skip_whitespace<Policy>(s, pos);
        if (pos < s.size() && s[pos] == ']') 
        {
            ++pos;
            return arr;
        }
        while (pos < s.size()) 
        {
            arr.push_back(parse_value<Policy>(s, pos, depth + 1));
            skip_whitespace<Policy>(s, pos);
            json_enforce(pos < s.size(),
                    "JSON parse error: unterminated array",
                    "position", pos);
            if (s[pos] == ']') 
            {
                ++pos;
                return arr;
            }
            json_enforce(s[pos] == ',',
                    "JSON parse error: expected ',' or ']' in array",
                    "position", pos);
            ++pos;
            skip_whitespace<Policy>(s, pos);
        }
        json_enforce(false, "JSON parse error: unterminated array");
        unreachable_after_enforce();
    }

    template <typename Policy = StandardJsonPolicy>
    inline JsonObject parse_object(std::string_view s, size_t& pos, size_t depth) 
    {
        json_enforce(depth <= Policy::max_parse_depth,
                "JSON parse error: maximum nesting depth exceeded",
                "position", pos,
                "max_depth", Policy::max_parse_depth);
        JsonObject obj;
        ++pos;
        skip_whitespace<Policy>(s, pos);
        if (pos < s.size() && s[pos] == '}') 
        {
            ++pos;
            return obj;
        }
        while (pos < s.size()) 
        {
            skip_whitespace<Policy>(s, pos);
            json_enforce(pos < s.size() && s[pos] == '"',
                    "JSON parse error: expected string key",
                    "position", pos);
            std::string key = parse_string(s, pos);
            skip_whitespace<Policy>(s, pos);
            json_enforce(pos < s.size() && s[pos] == ':',
                    "JSON parse error: expected ':' after object key",
                    "position", pos,
                    "key", key);
            ++pos;
            skip_whitespace<Policy>(s, pos);
            obj[std::move(key)] = parse_value<Policy>(s, pos, depth + 1);
            skip_whitespace<Policy>(s, pos);
            json_enforce(pos < s.size(),
                    "JSON parse error: unterminated object",
                    "position", pos);
            if (s[pos] == '}') 
            {
                ++pos;
                return obj;
            }
            json_enforce(s[pos] == ',',
                    "JSON parse error: expected ',' or '}' in object",
                    "position", pos);
            ++pos;
        }
        json_enforce(false, "JSON parse error: unterminated object");
        unreachable_after_enforce();
    }

    template <typename Policy = StandardJsonPolicy>
    inline JsonValue parse_value(std::string_view s, size_t& pos, size_t depth) 
    {
        skip_whitespace<Policy>(s, pos);
        json_enforce(pos < s.size(),
                "JSON parse error: unexpected end of input");
        
        char c = s[pos];
        if (c == '{') 
        {
            return parse_object<Policy>(s, pos, depth);
        }
        if (c == '[') 
        {
            return parse_array<Policy>(s, pos, depth);
        }
        if (c == '"') 
        {
            return parse_string(s, pos);
        }

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
        
        if (std::isdigit(c) || c == '-' || s.substr(pos, 3) == "NaN" || 
            s.substr(pos, 8) == "Infinity" || s.substr(pos, 9) == "-Infinity") 
        {
            return parse_number<Policy>(s, pos);
        }
        
        json_enforce(false,
                "JSON parse error: invalid value",
                "position", pos);
        unreachable_after_enforce();
    }
}

template <typename Policy = StandardJsonPolicy>
[[nodiscard]] inline JsonValue parse_json(std::string_view json) 
{
    size_t pos = 0;
    JsonValue val = json_detail::parse_value<Policy>(json, pos);
    json_detail::skip_whitespace<Policy>(json, pos);
    json_enforce(pos == json.size(),
            "JSON parse error: extra data after JSON value",
            "position", pos);
    return val;
}

inline void to_json(JsonValue& j, std::nullptr_t) noexcept { j = nullptr; }
inline void to_json(JsonValue& j, bool value) noexcept { j = value; }
inline void to_json(JsonValue& j, int value) noexcept { j = static_cast<int64_t>(value); }
inline void to_json(JsonValue& j, unsigned int value) noexcept 
{ 
    j = static_cast<int64_t>(value); 
}
inline void to_json(JsonValue& j, long value) noexcept { j = static_cast<int64_t>(value); }
inline void to_json(JsonValue& j, unsigned long value) noexcept 
{ 
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
inline void to_json(JsonValue& j, double value) noexcept { j = value; }
inline void to_json(JsonValue& j, const std::string& value) { j = value; }
inline void to_json(JsonValue& j, const char* value) { j = std::string(value); }
inline void to_json(JsonValue& j, std::string_view value) { j = std::string(value); }
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

inline JsonValue to_json(std::nullptr_t) noexcept { return nullptr; }
inline JsonValue to_json(bool value) noexcept { return value; }
inline JsonValue to_json(int value) noexcept { return static_cast<int64_t>(value); }
inline JsonValue to_json(unsigned int value) noexcept 
{ 
    return static_cast<int64_t>(value); 
}
inline JsonValue to_json(long value) noexcept { return static_cast<int64_t>(value); }
inline JsonValue to_json(unsigned long value) noexcept 
{ 
    if (value > static_cast<unsigned long>(std::numeric_limits<int64_t>::max())) 
    {
        return static_cast<double>(value);
    }
    return static_cast<int64_t>(value);
}
inline JsonValue to_json(long long value) noexcept { return static_cast<int64_t>(value); }
inline JsonValue to_json(unsigned long long value) noexcept 
{ 
    if (value > static_cast<unsigned long long>(std::numeric_limits<int64_t>::max())) 
    {
        return static_cast<double>(value);
    }
    return static_cast<int64_t>(value);
}
inline JsonValue to_json(float value) noexcept { return static_cast<double>(value); }
inline JsonValue to_json(double value) noexcept { return value; }
inline JsonValue to_json(const std::string& value) { return value; }
inline JsonValue to_json(const char* value) { return std::string(value); }
inline JsonValue to_json(std::string_view value) { return std::string(value); }
inline JsonValue to_json(signed char value) noexcept { return static_cast<int64_t>(value); }
inline JsonValue to_json(unsigned char value) noexcept { return static_cast<int64_t>(value); }
inline JsonValue to_json(short value) noexcept { return static_cast<int64_t>(value); }
inline JsonValue to_json(unsigned short value) noexcept { return static_cast<int64_t>(value); }

inline void from_json(const JsonValue& j, bool& value) 
{
    json_enforce(j.is_bool(),
            "JSON type mismatch",
            "expected", "boolean",
            "got", json_detail::type_name(j));
    value = std::get<bool>(j);
}

inline void from_json(const JsonValue& j, int& value) 
{
    if (j.is_int()) 
    {
        int64_t i64 = std::get<int64_t>(j);
        value = checked_cast<int>(i64);
    } 
    else if (j.is_number()) 
    {
        json_detail::convert_double_to_int(std::get<double>(j), value, "int");
    } 
    else 
    {
        json_enforce(false,
                "JSON type mismatch",
                "expected", "number",
                "got", json_detail::type_name(j));
    }
}

inline void from_json(const JsonValue& j, unsigned int& value) 
{
    if (j.is_int()) 
    {
        int64_t i64 = std::get<int64_t>(j);
        value = checked_cast<unsigned int>(i64);
    } 
    else if (j.is_number()) 
    {
        json_detail::convert_double_to_int(std::get<double>(j), value, "unsigned int");
    } 
    else 
    {
        json_enforce(false, "JSON type mismatch", "expected", "number", 
                "got", json_detail::type_name(j));
    }
}

inline void from_json(const JsonValue& j, long& value) 
{
    if (j.is_int()) 
    {
        int64_t i64 = std::get<int64_t>(j);
        value = checked_cast<long>(i64);
    } 
    else if (j.is_number()) 
    {
        json_detail::convert_double_to_int(std::get<double>(j), value, "long");
    } 
    else 
    {
        json_enforce(false, "JSON type mismatch", "expected", "number", 
                "got", json_detail::type_name(j));
    }
}

inline void from_json(const JsonValue& j, unsigned long& value) 
{
    if (j.is_int()) 
    {
        int64_t i64 = std::get<int64_t>(j);
        value = checked_cast<unsigned long>(i64);
    } 
    else if (j.is_number()) 
    {
        json_detail::convert_double_to_int(std::get<double>(j), value, "unsigned long");
    } 
    else 
    {
        json_enforce(false, "JSON type mismatch", "expected", "number", 
                "got", json_detail::type_name(j));
    }
}

inline void from_json(const JsonValue& j, long long& value) 
{
    if (j.is_int()) 
    {
        value = std::get<int64_t>(j);
    } 
    else if (j.is_number()) 
    {
        json_detail::convert_double_to_int(std::get<double>(j), value, "long long");
    } 
    else 
    {
        json_enforce(false, "JSON type mismatch", "expected", "number", 
                "got", json_detail::type_name(j));
    }
}

inline void from_json(const JsonValue& j, unsigned long long& value) 
{
    if (j.is_int()) 
    {
        int64_t i64 = std::get<int64_t>(j);
        value = checked_cast<unsigned long long>(i64);
    } 
    else if (j.is_number()) 
    {
        json_detail::convert_double_to_int(std::get<double>(j), value, "unsigned long long");
    } 
    else 
    {
        json_enforce(false, "JSON type mismatch", "expected", "number", 
                "got", json_detail::type_name(j));
    }
}

inline void from_json(const JsonValue& j, float& value) 
{
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
        json_enforce(false, "JSON type mismatch", "expected", "number", 
                "got", json_detail::type_name(j));
    }
}

inline void from_json(const JsonValue& j, double& value) 
{
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
        json_enforce(false, "JSON type mismatch", "expected", "number", 
                "got", json_detail::type_name(j));
    }
}

inline void from_json(const JsonValue& j, std::string& value) 
{
    json_enforce(j.is_string(), "JSON type mismatch", "expected", "string", 
            "got", json_detail::type_name(j));
    value = std::get<std::string>(j);
}

inline void from_json(const JsonValue& j, signed char& value) 
{
    if (j.is_int()) 
    {
        int64_t i64 = std::get<int64_t>(j);
        value = checked_cast<signed char>(i64);
    } 
    else if (j.is_number()) 
    {
        double d = std::get<double>(j);
        double intpart;
        json_enforce(fabs(std::modf(d, &intpart)) <= json_detail::double_epsilon,
                "JSON conversion error: fractional part detected",
                "value", d,
                "target_type", "signed char");
        json_enforce(intpart >= static_cast<double>(std::numeric_limits<signed char>::min()) &&
                intpart <= static_cast<double>(std::numeric_limits<signed char>::max()),
                "JSON conversion error: value out of range",
                "value", d,
                "target_type", "signed char");
        value = static_cast<signed char>(intpart);
    } 
    else 
    {
        json_enforce(false, "JSON type mismatch", "expected", "number", 
                "got", json_detail::type_name(j));
    }
}

inline void from_json(const JsonValue& j, unsigned char& value) 
{
    if (j.is_int()) 
    {
        int64_t i64 = std::get<int64_t>(j);
        value = checked_cast<unsigned char>(i64);
    } 
    else if (j.is_number()) 
    {
        double d = std::get<double>(j);
        double intpart;
        json_enforce(fabs(std::modf(d, &intpart)) <= json_detail::double_epsilon,
                "JSON conversion error: fractional part detected",
                "value", d,
                "target_type", "unsigned char");
        json_enforce(intpart >= 0.0 && 
                intpart <= static_cast<double>(std::numeric_limits<unsigned char>::max()),
                "JSON conversion error: value out of range",
                "value", d,
                "target_type", "unsigned char");
        value = static_cast<unsigned char>(intpart);
    } 
    else 
    {
        json_enforce(false, "JSON type mismatch", "expected", "number", 
                "got", json_detail::type_name(j));
    }
}

inline void from_json(const JsonValue& j, short& value) 
{
    if (j.is_int()) 
    {
        int64_t i64 = std::get<int64_t>(j);
        value = checked_cast<short>(i64);
    } 
    else if (j.is_number()) 
    {
        double d = std::get<double>(j);
        double intpart;
        json_enforce(fabs(std::modf(d, &intpart)) <= json_detail::double_epsilon,
                "JSON conversion error: fractional part detected",
                "value", d,
                "target_type", "short");
        json_enforce(intpart >= static_cast<double>(std::numeric_limits<short>::min()) &&
                intpart <= static_cast<double>(std::numeric_limits<short>::max()),
                "JSON conversion error: value out of range",
                "value", d,
                "target_type", "short");
        value = static_cast<short>(intpart);
    } 
    else 
    {
        json_enforce(false, "JSON type mismatch", "expected", "number", 
                "got", json_detail::type_name(j));
    }
}

inline void from_json(const JsonValue& j, unsigned short& value) 
{
    if (j.is_int()) 
    {
        int64_t i64 = std::get<int64_t>(j);
        value = checked_cast<unsigned short>(i64);
    } 
    else if (j.is_number()) 
    {
        double d = std::get<double>(j);
        double intpart;
        json_enforce(fabs(std::modf(d, &intpart)) <= json_detail::double_epsilon,
                "JSON conversion error: fractional part detected",
                "value", d,
                "target_type", "unsigned short");
        json_enforce(intpart >= 0.0 && 
                intpart <= static_cast<double>(std::numeric_limits<unsigned short>::max()),
                "JSON conversion error: value out of range",
                "value", d,
                "target_type", "unsigned short");
        value = static_cast<unsigned short>(intpart);
    } 
    else 
    {
        json_enforce(false, "JSON type mismatch", "expected", "number", 
                "got", json_detail::type_name(j));
    }
}


namespace json_detail 
{
    template <typename T>
    JsonValue to_json_value(const T& value) 
    {
        JsonValue result;
        to_json(result, value);
        return result;
    }
}

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
            std::ostringstream oss;
            oss << key;
            key_str = oss.str();
        }
        obj[std::move(key_str)] = json_detail::to_json_value(val);
    }
    j = std::move(obj);
}

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

template <typename T1, typename T2>
void to_json(JsonValue& j, const std::pair<T1, T2>& p) 
{
    JsonArray arr;
    arr.push_back(json_detail::to_json_value(p.first));
    arr.push_back(json_detail::to_json_value(p.second));
    j = std::move(arr);
}

template <typename Tuple, std::size_t... I>
JsonArray tuple_to_json_impl(const Tuple& tup, std::index_sequence<I...>) 
{
    JsonArray arr;
    (..., arr.push_back(json_detail::to_json_value(std::get<I>(tup))));
    return arr;
}

template <typename... Ts>
void to_json(JsonValue& j, const std::tuple<Ts...>& tup) 
{
    j = tuple_to_json_impl(tup, std::index_sequence_for<Ts...>{});
}

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

template <typename T, size_t N>
void from_json(const JsonValue& j, std::array<T, N>& arr) 
{
    json_enforce(j.is_array(), "JSON type mismatch", "expected", "array for std::array", 
            "got", json_detail::type_name(j));
    const auto& json_arr = std::get<JsonArray>(j);
    json_enforce(json_arr.size() == N,
            "JSON array size mismatch",
            "expected", N,
            "got", json_arr.size());
    for (size_t i = 0; i < N; ++i) 
    {
        from_json(json_arr[i], arr[i]);
    }
}

template <typename T>
void from_json(const JsonValue& j, std::vector<T>& vec) 
{
    json_enforce(j.is_array(), "JSON type mismatch", "expected", "array", 
            "got", json_detail::type_name(j));
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

template <typename T>
void from_json(const JsonValue& j, std::set<T>& s) 
{
    json_enforce(j.is_array(), "JSON type mismatch", "expected", "array for std::set", 
            "got", json_detail::type_name(j));
    const auto& arr = std::get<JsonArray>(j);
    s.clear();
    for (const auto& elem : arr) 
    {
        T value;
        from_json(elem, value);
        s.insert(std::move(value));
    }
}

template <typename T>
void from_json(const JsonValue& j, std::unordered_set<T>& s) 
{
    json_enforce(j.is_array(), "JSON type mismatch", "expected", "array for std::unordered_set", 
            "got", json_detail::type_name(j));
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

template <typename T>
void from_json(const JsonValue& j, std::deque<T>& d) 
{
    json_enforce(j.is_array(), "JSON type mismatch", "expected", "array for std::deque", 
            "got", json_detail::type_name(j));
    const auto& arr = std::get<JsonArray>(j);
    d.clear();
    for (const auto& elem : arr) 
    {
        T value;
        from_json(elem, value);
        d.push_back(std::move(value));
    }
}

template <typename T>
void from_json(const JsonValue& j, std::list<T>& lst) 
{
    json_enforce(j.is_array(), "JSON type mismatch", "expected", "array for std::list", 
            "got", json_detail::type_name(j));
    const auto& arr = std::get<JsonArray>(j);
    lst.clear();
    for (const auto& elem : arr) 
    {
        T value;
        from_json(elem, value);
        lst.push_back(std::move(value));
    }
}

template <typename K, typename V>
void from_json(const JsonValue& j, std::map<K, V>& m) 
{
    json_enforce(j.is_object(), "JSON type mismatch", "expected", "object", 
            "got", json_detail::type_name(j));
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
            K converted_key;
            std::istringstream iss(key);
            iss.imbue(std::locale::classic());
            iss >> converted_key;
            json_enforce(!iss.fail() && iss.eof(), 
                    "Failed to convert map key", 
                    "key", key,
                    "target_type", typeid(K).name());
            m[converted_key] = std::move(value);
        } 
        else 
        {
            json_enforce(false, "Unsupported map key type for deserialization");
        }
    }
}

template <typename K, typename V>
void from_json(const JsonValue& j, std::unordered_map<K, V>& m) 
{
    json_enforce(j.is_object(), "JSON type mismatch", "expected", "object for std::unordered_map", 
            "got", json_detail::type_name(j));
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
            K converted_key;
            std::istringstream iss(key);
            iss.imbue(std::locale::classic());
            iss >> converted_key;
            json_enforce(!iss.fail() && iss.eof(), 
                    "Failed to convert map key", 
                    "key", key,
                    "target_type", typeid(K).name());
            m[converted_key] = std::move(value);
        } 
        else 
        {
            json_enforce(false, "Unsupported map key type for deserialization");
        }
    }
}

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

template <typename T1, typename T2>
void from_json(const JsonValue& j, std::pair<T1, T2>& p) 
{
    json_enforce(j.is_array(), "JSON type mismatch", "expected", "array for pair", 
            "got", json_detail::type_name(j));
    const auto& arr = std::get<JsonArray>(j);
    json_enforce(arr.size() == 2,
            "JSON array size mismatch for pair",
            "expected", 2,
            "got", arr.size());
    from_json(arr[0], p.first);
    from_json(arr[1], p.second);
}

template <typename... Ts>
void from_json(const JsonValue& j, std::tuple<Ts...>& tup) 
{
    json_enforce(j.is_array(), "JSON type mismatch", "expected", "array for tuple", 
            "got", json_detail::type_name(j));
    const auto& arr = std::get<JsonArray>(j);
    constexpr size_t expected_size = sizeof...(Ts);
    json_enforce(arr.size() == expected_size,
            "JSON array size mismatch for tuple",
            "expected", expected_size,
            "got", arr.size());
    from_json_tuple_impl(arr, tup, std::index_sequence_for<Ts...>{});
}

template <typename Tuple, std::size_t... I>
void from_json_tuple_impl(const JsonArray& arr, Tuple& tup, std::index_sequence<I...>) 
{
    (..., from_json(arr[I], std::get<I>(tup)));
}

/**
 * @brief Value-returning from_json overload for convenient single-line conversions
 * 
 * @details This template provides a more ergonomic alternative to the reference-based
 * from_json(const JsonValue&, T&) overloads. It enables direct initialization and
 * const-correctness while maintaining the same type safety and error handling.
 * 
 * @section usage Usage Examples
 * @code{.cpp}
 * JsonObject obj;
 * obj["port"] = 8080;
 * obj["host"] = "localhost";
 * obj["timeout"] = 30;
 * 
 * // Reference-based API (traditional, efficient for large objects)
 * int port;
 * from_json(obj["port"], port);
 * 
 * // Value-returning API (convenient, enables const and single-line init)
 * const int port = from_json<int>(obj["port"]);
 * const auto host = from_json<std::string>(obj["host"]);
 * const auto timeout = from_json<int>(obj["timeout"]);
 * 
 * // Even more convenient: direct key access
 * const int port = from_json<int>(obj, "port");
 * const auto host = from_json<std::string>(obj, "host");
 * const auto timeout = from_json<int>(obj, "timeout");
 * 
 * // Works with complex types too
 * auto vec = from_json<std::vector<int>>(obj, "numbers");
 * auto opt = from_json<std::optional<std::string>>(obj, "maybe");
 * @endcode
 * 
 * @note The compiler will optimize away copies via NRVO (Named Return Value Optimization)
 * making this zero-overhead in release builds
 * 
 * @tparam T The target type to convert to (must be explicitly specified)
 * @param j The JSON value to convert from
 * @return T The converted value
 * @throws std::runtime_error via json_enforce on type mismatch or conversion failure
 */
template <typename T>
T from_json(const JsonValue& j) 
{
    if constexpr (std::is_same_v<T, JsonValue>) 
    {
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
 * @brief Convenience overload for extracting values from JsonObject by key
 * 
 * @details This overload eliminates the need to call obj.at("key") explicitly,
 * making the API more concise and readable.
 * 
 * @code{.cpp}
 * JsonObject obj;
 * obj["port"] = 8080;
 * obj["host"] = "localhost";
 * 
 * // Instead of: from_json<int>(obj.at("port"))
 * const int port = from_json<int>(obj, "port");
 * const auto host = from_json<std::string>(obj, "host");
 * @endcode
 * 
 * @tparam T The target type to convert to
 * @param obj The JSON object to extract from
 * @param key The key to lookup in the object
 * @return T The converted value
 * @throws std::runtime_error if key doesn't exist or type mismatch
 */
template <typename T>
T from_json(const JsonObject& obj, const std::string& key)
{
    return from_json<T>(obj.at(key));
}

/**
 * @brief Convenience overload for extracting values from JsonValue (object) by key
 * 
 * @details This overload works with JsonValue that holds a JsonObject, automatically
 * checking the type and extracting the value by key.
 * 
 * @code{.cpp}
 * JsonValue j = parse_json(R"({"port": 8080, "host": "localhost"})");
 * 
 * const int port = from_json<int>(j, "port");
 * const auto host = from_json<std::string>(j, "host");
 * @endcode
 * 
 * @tparam T The target type to convert to
 * @param j The JSON value (must hold an object)
 * @param key The key to lookup in the object
 * @return T The converted value
 * @throws std::runtime_error if j is not an object, key doesn't exist, or type mismatch
 */
template <typename T>
T from_json(const JsonValue& j, const std::string& key)
{
    json_enforce(j.is_object(), 
            "JSON type mismatch: expected object for key access", 
            "expected", "object", 
            "got", json_detail::type_name(j),
            "key", key);
    const auto& obj = std::get<JsonObject>(j);
    return from_json<T>(obj.at(key));
}

template <typename T>
auto to_json(const T& value) -> decltype(to_json(std::declval<JsonValue&>(), value), JsonValue{}) 
{
    JsonValue j;
    to_json(j, value);
    return j;
}

[[nodiscard]] inline JsonValue load_json_from_file(const std::string& filename) 
{
    std::ifstream ifs(filename, std::ios::binary);
    json_enforce(ifs.is_open(), "Failed to open file for reading", "filename", filename);
    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    json_enforce(!ifs.bad(), "Error reading file", "filename", filename);
    return parse_json(content);
}

template <typename Policy = StandardJsonPolicy>
inline void save_json_to_file(const std::string& filename, const JsonValue& val, 
                              bool pretty = Policy::pretty_print) 
{
    std::ofstream ofs(filename);
    json_enforce(ofs.is_open(), "Failed to open file for writing", "filename", filename);
    to_json_stream<JsonValue, Policy>(ofs, val, pretty);
    json_enforce(ofs.good(), "Error writing to file", "filename", filename);
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
 * @brief Save a C++ object to JSON file
 * 
 * @details High-level convenience function for persisting C++ objects as JSON files.
 * This is the recommended way to save configuration data, parameters, or any
 * serializable object to disk.
 * 
 * The function:
 * 1. Converts the object to JsonValue via to_json()
 * 2. Serializes to pretty-printed JSON (by default)
 * 3. Writes to the specified file
 * 4. Validates file operations (throws on error)
 * 
 * @tparam T Type of object to save (must have to_json overload or macro)
 * @tparam Policy Formatting policy (default: PrettyJsonPolicy for readable output)
 * @param filename Path to output file (will be created or overwritten)
 * @param params Object to serialize
 * 
 * @throws std::runtime_error If file cannot be opened or write fails
 * 
 * @section examples Examples
 * @code{.cpp}
 * struct Config {
 *     int port = 8080;
 *     std::string host = "localhost";
 * };
 * CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Config, port, host)
 * 
 * Config cfg{9000, "192.168.1.1"};
 * 
 * // Save with pretty printing (default)
 * save_params("config.json", cfg);
 * 
 * // Save with compact output
 * save_params<Config, StandardJsonPolicy>("config.json", cfg);
 * 
 * // Save with specific numeric format
 * save_params<Config, FixedFormatPolicy>("config.json", cfg);
 * @endcode
 * 
 * @note File is written atomically - if write fails, file may be partially written
 * @note Consider save_params_with_backup() for critical data
 * 
 * @see load_params
 * @see save_params_with_backup
 * @see PrettyJsonPolicy
 * @see StandardJsonPolicy
 */
template<typename T, typename Policy = PrettyJsonPolicy>
inline void save_params(const std::string& filename, const T& params) 
{
    save_json_to_file<Policy>(filename, to_json(params), true);
}

/**
 * @brief Load a C++ object from JSON file (value-returning)
 * 
 * @details Convenience function for loading objects from JSON files. Returns the
 * deserialized object directly, enabling const-correctness and single-line initialization.
 * 
 * The function:
 * 1. Reads the file into a string
 * 2. Parses the JSON
 * 3. Converts to the target type via from_json()
 * 4. Returns the object
 * 
 * @tparam T Type of object to load (must have from_json overload or macro)
 * @param filename Path to JSON file
 * @return T The deserialized object
 * 
 * @throws std::runtime_error If:
 *         - File cannot be opened
 *         - JSON parsing fails
 *         - Type conversion fails (missing fields, type mismatch, etc.)
 * 
 * @section examples Examples
 * @code{.cpp}
 * // Value-returning version (preferred)
 * const Config cfg = load_params<Config>("config.json");
 * 
 * // Direct initialization
 * auto settings = load_params<Settings>("settings.json");
 * 
 * // With error handling
 * try {
 *     auto cfg = load_params<Config>("config.json");
 *     std::cout << "Port: " << cfg.port << "\n";
 * } catch (const std::exception& e) {
 *     std::cerr << "Failed to load config: " << e.what() << "\n";
 * }
 * @endcode
 * 
 * @note Use this version for new code (enables const)
 * @note For existing references, use the overload: load_params(filename, obj)
 * 
 * @see save_params
 * @see load_params(const std::string&, T&)
 * @see from_json
 */
template<typename T>
[[nodiscard]] inline T load_params(const std::string& filename) 
{
    return from_json<T>(load_json_from_file(filename));
}

/**
 * @brief Load a C++ object from JSON file (reference-based)
 * 
 * @details Convenience function that loads data into an existing object reference.
 * This overload is provided for compatibility with existing code that uses references.
 * 
 * @tparam T Type of object to load (deduced from params)
 * @param filename Path to JSON file
 * @param params Reference to object that will receive the loaded data
 * 
 * @throws std::runtime_error If file read, parsing, or conversion fails
 * 
 * @code{.cpp}
 * Config cfg;
 * load_params("config.json", cfg);
 * // cfg now contains the loaded data
 * @endcode
 * 
 * @note Consider using the value-returning overload for new code
 * 
 * @see load_params(const std::string&)
 * @see save_params
 */
template<typename T>
inline void load_params(const std::string& filename, T& params) 
{
    params = from_json<T>(load_json_from_file(filename));
}

/**
 * @section json_pointer JSON Pointer (RFC 6901)
 * 
 * @details JSON Pointer provides a standardized way to navigate JSON documents
 * using path-like syntax. This is part of the JSON standard (RFC 6901) and provides
 * type-safe, exception-based navigation through JSON structures.
 * 
 * Features:
 * - RFC 6901 compliant implementation
 * - Escape sequence support (~0 for ~, ~1 for /)
 * - Array index navigation with bounds checking
 * - Object key navigation with existence checking
 * - Type-safe conversion with query_json_as<T>()
 * 
 * @code{.cpp}
 * JsonValue config = load_json_from_file("config.json");
 * 
 * // Navigate to nested value
 * const JsonValue& port = query_json_pointer(config, "/database/port");
 * int port_num = from_json<int>(port);
 * 
 * // Type-safe shorthand
 * int timeout = query_json_as<int>(config, "/database/timeout");
 * 
 * // Array access
 * const JsonValue& first_host = query_json_pointer(config, "/servers/0/host");
 * @endcode
 */

namespace json_detail {

/**
 * @brief Decode JSON Pointer token per RFC 6901
 * 
 * @details Decodes escape sequences in JSON Pointer tokens:
 * - ~0 becomes ~ (tilde)
 * - ~1 becomes / (forward slash)
 * 
 * These escapes are necessary because ~ and / have special meaning in JSON Pointer
 * syntax. The tilde is the escape character, and forward slash is the path separator.
 * 
 * @param token The token to decode (string between '/' separators)
 * @return std::string Decoded token
 * 
 * @throws std::runtime_error If escape sequence is incomplete or invalid
 * 
 * @code{.cpp}
 * decode_json_pointer_token("foo~1bar");  // Returns "foo/bar"
 * decode_json_pointer_token("a~0b");      // Returns "a~b"
 * decode_json_pointer_token("plain");     // Returns "plain"
 * @endcode
 * 
 * @note This function is for internal use by query_json_pointer
 * @see query_json_pointer
 */
[[nodiscard]] inline std::string decode_json_pointer_token(std::string_view token)
{
    std::string result;
    result.reserve(token.size());
    
    for (size_t i = 0; i < token.size(); ++i) {
        if (token[i] == '~') {
            json_enforce(i + 1 < token.size(), 
                "JSON Pointer: incomplete escape sequence at end of token",
                "position", i,
                "token", std::string(token));
            
            if (token[i + 1] == '0') {
                result += '~';
                ++i;
            } else if (token[i + 1] == '1') {
                result += '/';
                ++i;
            } else {
                json_enforce(false,
                    "JSON Pointer: invalid escape sequence (only ~0 and ~1 allowed)",
                    "sequence", std::string("~") + std::string(1, token[i + 1]),
                    "position", i);
            }
        } else {
            result += token[i];
        }
    }
    
    return result;
}

/**
 * @brief Navigate one level in JSON structure
 * 
 * @details Performs single-level navigation in a JSON value:
 * - For objects: looks up the key
 * - For arrays: parses index and validates bounds
 * - For scalars: throws error (cannot navigate into primitives)
 * 
 * @param current The current JSON value to navigate from
 * @param decoded_token The decoded token (object key or array index)
 * @return const JsonValue& Reference to the target value
 * 
 * @throws std::runtime_error If:
 *         - Object key not found
 *         - Array index invalid or out of bounds
 *         - Attempting to navigate into scalar value
 *         - Using "-" index (reserved for append operations)
 * 
 * @code{.cpp}
 * JsonValue obj = JsonObject{{"key", std::string("value")}};
 * const JsonValue& val = navigate_json_level(obj, "key");
 * 
 * JsonValue arr = JsonArray{1, 2, 3};
 * const JsonValue& elem = navigate_json_level(arr, "1");  // Gets second element
 * @endcode
 * 
 * @note This function is for internal use by query_json_pointer
 * @see query_json_pointer
 */
[[nodiscard]] inline const JsonValue& navigate_json_level(
    const JsonValue& current,
    std::string_view decoded_token)
{
    if (current.is_object()) {
        const auto& obj = std::get<JsonObject>(current);
        std::string key(decoded_token);
        
        auto it = obj.find(key);
        json_enforce(it != obj.end(),
            "JSON Pointer: object key not found",
            "key", key);
        
        return it->second;
    }
    else if (current.is_array()) {
        const auto& arr = std::get<JsonArray>(current);
        
        json_enforce(!decoded_token.empty(),
            "JSON Pointer: empty array index");
        
        if (decoded_token == "-") {
            json_enforce(false,
                "JSON Pointer: array index '-' not valid for query (use for append only)");
        }
        
        json_enforce(decoded_token[0] != '0' || decoded_token.size() == 1,
            "JSON Pointer: array index has leading zero (not allowed per RFC 6901)",
            "token", std::string(decoded_token));
        
        size_t index = 0;
        auto [ptr, ec] = std::from_chars(
            decoded_token.data(),
            decoded_token.data() + decoded_token.size(),
            index);
        
        json_enforce(ec == std::errc{} && ptr == decoded_token.data() + decoded_token.size(),
            "JSON Pointer: invalid array index (must be non-negative integer)",
            "token", std::string(decoded_token));
        
        json_enforce(index < arr.size(),
            "JSON Pointer: array index out of bounds",
            "index", index,
            "size", arr.size());
        
        return arr[index];
    }
    else {
        std::string type_str;
        if (current.is_null()) type_str = "null";
        else if (current.is_bool()) type_str = "boolean";
        else if (current.is_number()) type_str = "number";
        else if (current.is_string()) type_str = "string";
        else type_str = "unknown";
        
        json_enforce(false,
            "JSON Pointer: cannot navigate into scalar value",
            "type", type_str,
            "token", std::string(decoded_token));
        
        return current;
    }
}

} // namespace json_detail

/**
 * @brief Query JSON using RFC 6901 JSON Pointer (const version)
 * 
 * @details Navigates a JSON document using JSON Pointer path syntax per RFC 6901.
 * A JSON Pointer is a string containing a sequence of zero or more reference tokens,
 * each prefixed by a '/' character.
 * 
 * Syntax:
 * - Empty string "" refers to the whole document
 * - "/foo" refers to the value of the "foo" key in the root object
 * - "/foo/0" refers to the first element of the "foo" array
 * - "/foo/bar" refers to nested object navigation
 * 
 * Escape sequences:
 * - ~0 represents ~ (tilde)
 * - ~1 represents / (forward slash)
 * 
 * @param root The root JSON value to query
 * @param pointer JSON Pointer path (must start with '/' or be empty)
 * @return const JsonValue& Reference to the target value
 * 
 * @throws std::runtime_error If:
 *         - Pointer doesn't start with '/' (unless empty)
 *         - Path component not found
 *         - Array index invalid or out of bounds
 *         - Attempting to navigate into scalar
 *         - Invalid escape sequences
 * 
 * @section examples Examples
 * @code{.cpp}
 * JsonValue doc = parse_json(R"({
 *     "database": {
 *         "host": "localhost",
 *         "port": 5432,
 *         "servers": ["primary", "backup"]
 *     }
 * })");
 * 
 * // Object navigation
 * const JsonValue& port = query_json_pointer(doc, "/database/port");
 * int port_num = from_json<int>(port);  // 5432
 * 
 * // Array navigation
 * const JsonValue& server = query_json_pointer(doc, "/database/servers/0");
 * std::string server_name = from_json<std::string>(server);  // "primary"
 * 
 * // Root document
 * const JsonValue& root = query_json_pointer(doc, "");  // Returns doc itself
 * 
 * // Escaped characters
 * JsonValue doc2 = parse_json(R"({"foo/bar": "baz", "a~b": "c"})");
 * const JsonValue& val1 = query_json_pointer(doc2, "/foo~1bar");  // "baz"
 * const JsonValue& val2 = query_json_pointer(doc2, "/a~0b");      // "c"
 * @endcode
 * 
 * @note Returns reference - lifetime is tied to the root JsonValue
 * @note This is the const version - cannot modify the result
 * 
 * @see query_json_pointer(JsonValue&, std::string_view)
 * @see query_json_as
 */
[[nodiscard]] inline const JsonValue& query_json_pointer(
    const JsonValue& root, 
    std::string_view pointer)
{
    if (pointer.empty()) {
        return root;
    }
    
    json_enforce(pointer.front() == '/',
        "JSON Pointer must start with '/' or be empty",
        "pointer", std::string(pointer));
    
    const JsonValue* current = &root;
    size_t start = 1;
    
    while (start <= pointer.size()) {
        size_t next_slash = pointer.find('/', start);
        size_t end = (next_slash == std::string_view::npos) 
            ? pointer.size() 
            : next_slash;
        
        std::string_view token = pointer.substr(start, end - start);
        std::string decoded = json_detail::decode_json_pointer_token(token);
        
        current = &json_detail::navigate_json_level(*current, decoded);
        
        if (next_slash == std::string_view::npos) {
            break;
        }
        start = next_slash + 1;
    }
    
    return *current;
}

/**
 * @brief Query JSON using RFC 6901 JSON Pointer (mutable version)
 * 
 * @details Mutable version of query_json_pointer that allows modification of the
 * target value. All navigation rules and error handling are the same as the const
 * version.
 * 
 * @param root The root JSON value to query
 * @param pointer JSON Pointer path (must start with '/' or be empty)
 * @return JsonValue& Mutable reference to the target value
 * 
 * @throws std::runtime_error Same conditions as const version
 * 
 * @code{.cpp}
 * JsonValue config = load_json_from_file("config.json");
 * 
 * // Modify nested value
 * JsonValue& port = query_json_pointer(config, "/database/port");
 * port = 9000;  // Change port
 * 
 * // Modify array element
 * JsonValue& server = query_json_pointer(config, "/servers/0");
 * server = std::string("new-server.com");
 * 
 * save_json_to_file("config.json", config);
 * @endcode
 * 
 * @note Returns mutable reference - can modify the result
 * @note Lifetime is tied to the root JsonValue
 * 
 * @see query_json_pointer(const JsonValue&, std::string_view)
 * @see query_json_as
 */
[[nodiscard]] inline JsonValue& query_json_pointer(
    JsonValue& root, 
    std::string_view pointer)
{
    return const_cast<JsonValue&>(
        query_json_pointer(const_cast<const JsonValue&>(root), pointer)
    );
}

/**
 * @brief Type-safe JSON Pointer query with automatic conversion
 * 
 * @details Combines query_json_pointer() and from_json() into a single operation.
 * This is the most convenient way to extract typed values from JSON documents.
 * 
 * The function:
 * 1. Navigates to the pointer location
 * 2. Converts the value to the target type
 * 3. Returns the converted value
 * 
 * @tparam T Target type for conversion
 * @param root The root JSON value to query
 * @param pointer JSON Pointer path
 * @return T The converted value
 * 
 * @throws std::runtime_error If:
 *         - Navigation fails (invalid path, not found, etc.)
 *         - Type conversion fails (wrong type, missing fields, etc.)
 * 
 * @section examples Examples
 * @code{.cpp}
 * JsonValue config = load_json_from_file("config.json");
 * 
 * // Extract primitives
 * int port = query_json_as<int>(config, "/database/port");
 * std::string host = query_json_as<std::string>(config, "/database/host");
 * bool enabled = query_json_as<bool>(config, "/features/logging");
 * 
 * // Extract optionals
 * auto timeout = query_json_as<std::optional<int>>(config, "/database/timeout");
 * if (timeout) {
 *     std::cout << "Timeout: " << *timeout << "\n";
 * }
 * 
 * // Extract arrays
 * auto hosts = query_json_as<std::vector<std::string>>(config, "/servers");
 * 
 * // Extract custom types (with macro)
 * struct DatabaseConfig {
 *     std::string host;
 *     int port;
 * };
 * CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(DatabaseConfig, host, port)
 * 
 * auto db = query_json_as<DatabaseConfig>(config, "/database");
 * @endcode
 * 
 * @note Combines navigation and conversion in one call
 * @note Uses from_json<T> for type conversion
 * @note All from_json conversion rules apply
 * 
 * @see query_json_pointer
 * @see from_json
 */
template<typename T>
[[nodiscard]] inline T query_json_as(
    const JsonValue& root, 
    std::string_view pointer)
{
    const JsonValue& value = query_json_pointer(root, pointer);
    return from_json<T>(value);
}

/**
 * @brief Save object to JSON file with automatic backup
 * 
 * @details Enhanced version of save_params() that creates a backup copy before
 * writing. This provides safety against data loss if the save operation fails
 * or if the new data is corrupted.
 * 
 * Backup behavior:
 * 1. If the target file exists, create a backup copy first
 * 2. Save the new data to the target file
 * 3. If save fails, the backup remains intact
 * 4. If file doesn't exist, no backup is created (first save)
 * 
 * @tparam T Type of object to save
 * @tparam Policy Formatting policy (default: PrettyJsonPolicy)
 * @param filename Path to output file
 * @param params Object to serialize
 * @param backup_suffix Suffix for backup file (default: ".bak")
 * 
 * @throws std::runtime_error If:
 *         - Backup creation fails
 *         - File write fails
 *         - Serialization fails
 * 
 * @section examples Examples
 * @code{.cpp}
 * Config cfg{8080, "localhost"};
 * 
 * // Creates config.json and config.json.bak (if config.json exists)
 * save_params_with_backup("config.json", cfg);
 * 
 * // Custom backup suffix
 * save_params_with_backup("config.json", cfg, ".backup");
 * 
 * // With different timestamp suffixes
 * auto timestamp = std::to_string(std::time(nullptr));
 * save_params_with_backup("config.json", cfg, "." + timestamp);
 * @endcode
 * 
 * @section use_cases When to Use
 * - Critical configuration files
 * - User preferences that shouldn't be lost
 * - Data that changes frequently
 * - Production environments where reliability is paramount
 * 
 * @note The backup file is created by copying, not moving, so both files exist after save
 * @note Previous backup is overwritten each time (keeps only one backup)
 * @note For versioned backups, change backup_suffix on each call
 * 
 * @warning This is not atomic - if the program crashes during save, you may have
 *          partial data in the main file, but the backup will be intact
 * 
 * @see save_params
 * @see load_params
 */
template<typename T, typename Policy = PrettyJsonPolicy>
inline void save_params_with_backup(const std::string& filename, const T& params, 
                                    const std::string& backup_suffix = ".bak") 
{
    std::ifstream test(filename);
    if (test.good()) 
    {
        test.close();
        std::string backup_name = filename + backup_suffix;
        
        std::ifstream src(filename, std::ios::binary);
        json_enforce(src.is_open(), "Failed to open source file for backup", "filename", filename);
        
        std::ofstream dst(backup_name, std::ios::binary);
        json_enforce(dst.is_open(), "Failed to create backup file", "backup_file", backup_name);
        
        dst << src.rdbuf();
        json_enforce(dst.good(), "Error writing backup file", "backup_file", backup_name);
        
        dst.close();
        src.close();
    }
    
    save_params<T, Policy>(filename, params);
}

} // namespace fat_p
