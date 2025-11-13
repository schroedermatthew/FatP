/**
 * @file JsonLite.h
 * @brief Lightweight JSON library for C++ configuration and parameter management
 * 
 * @section overview Overview
 * JsonLite is a modern C++17 header-only JSON library designed specifically for
 * application configuration files, parameter persistence, and structured data
 * serialization where simplicity and zero dependencies are priorities.
 * 
 * @section features Features
 * - C++17 header-only library (single file, no build configuration)
 * - Zero external dependencies (pure standard library)
 * - Policy-based design for compile-time customization
 * - Type-safe variant-based JSON value representation
 * - Macro-based automatic struct serialization
 * - Comprehensive error messages with position information
 * - Support for std::optional, std::vector, std::map, std::tuple, std::pair
 * - Integer precision preservation using int64_t
 * 
 * @section example Basic Example
 * @code{.cpp}
 * #include "JsonLite.h"
 * using namespace cpp_utilities;
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
 */

#pragma once

#include <algorithm>
#include <array>
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

namespace cpp_utilities {

// ====================================================================
// Type Traits
// ====================================================================

namespace json_detail {
    // Check if type is iterable (has begin/end)
    template <typename T, typename = void>
    struct IsIterable : std::false_type {};
    
    template <typename T>
    struct IsIterable<T, std::void_t<
        decltype(std::begin(std::declval<T&>())),
        decltype(std::end(std::declval<T&>()))
    >> : std::true_type {};
    
    // Check if type has mapped_type (is associative container)
    template <typename T, typename = void>
    struct HasMappedType : std::false_type {};
    
    template <typename T>
    struct HasMappedType<T, std::void_t<typename T::mapped_type>> : std::true_type {};
    
    // Extract value_type from container
    template <typename Container>
    using ContainerValueT = typename Container::value_type;
} // namespace json_detail

// ====================================================================
// JSON Value Type (for Parsing)
// ====================================================================

/** @brief Type alias for JSON object representation using ordered map */
using JsonObject = std::map<std::string, struct JsonValue>;

/** @brief Type alias for JSON array representation */
using JsonArray = std::vector<struct JsonValue>;

/**
 * @brief Type-safe JSON value container using std::variant
 * 
 * Represents any valid JSON value type. Uses int64_t for integers to preserve
 * precision up to ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â±2^63, and std::map for objects to maintain deterministic
 * iteration order.
 * 
 * Supported types:
 * - null (std::nullptr_t)
 * - boolean (bool)
 * - integer (int64_t)
 * - floating-point (double)
 * - string (std::string)
 * - array (JsonArray)
 * - object (JsonObject)
 */
struct JsonValue : std::variant<std::nullptr_t, bool, int64_t, double, std::string, JsonArray, JsonObject> {
    using variant::variant;
    
    /** @brief Check if value is null */
    [[nodiscard]] bool is_null() const noexcept { return std::holds_alternative<std::nullptr_t>(*this); }
    
    /** @brief Check if value is boolean */
    [[nodiscard]] bool is_bool() const noexcept { return std::holds_alternative<bool>(*this); }
    
    /** @brief Check if value is integer (int64_t) */
    [[nodiscard]] bool is_int() const noexcept { return std::holds_alternative<int64_t>(*this); }
    
    /** @brief Check if value is numeric (int64_t or double) */
    [[nodiscard]] bool is_number() const noexcept { return std::holds_alternative<int64_t>(*this) || std::holds_alternative<double>(*this); }
    
    /** @brief Check if value is string */
    [[nodiscard]] bool is_string() const noexcept { return std::holds_alternative<std::string>(*this); }
    
    /** @brief Check if value is array */
    [[nodiscard]] bool is_array() const noexcept { return std::holds_alternative<JsonArray>(*this); }
    
    /** @brief Check if value is object */
    [[nodiscard]] bool is_object() const noexcept { return std::holds_alternative<JsonObject>(*this); }
};

// ====================================================================
// JSON Policies
// ====================================================================

/**
 * @brief Number formatting strategy for floating-point values
 * 
 * Controls how double values are serialized to JSON:
 * - Auto: Automatically choose fixed or scientific based on magnitude
 * - Fixed: Always use fixed-point notation (e.g., 123.456)
 * - Scientific: Always use scientific notation (e.g., 1.234e+2)
 * - Shortest: Use default formatting (shortest representation)
 */
enum class NumberFormat {
    Auto,        ///< Smart selection: fixed for normal range, scientific for extreme values
    Fixed,       ///< Always use fixed-point notation (original behavior)
    Scientific,  ///< Always use scientific notation
    Shortest     ///< Use default formatting (no std::fixed or std::scientific)
};

/**
 * @brief Standard JSON formatting policy
 * 
 * Default policy for strict JSON compliance:
 * - Compact output (no pretty printing)
 * - 16 decimal places for floating-point precision
 * - Auto number formatting (smart fixed/scientific selection)
 * - No NaN/Infinity support (outputs null instead)
 * - Unicode characters escaped to \uXXXX sequences
 * - Maximum nesting depth of 512 levels
 */
struct StandardJsonPolicy {
    static constexpr bool pretty_print = true;         ///< Enable pretty printing
    static constexpr int numeric_precision = 16;       ///< Decimal places for floating-point
    static constexpr int indent_step = 4;              ///< Spaces per indentation level
    static constexpr bool allow_nan_inf = false;       ///< Reject NaN/Infinity (output null)
    static constexpr bool escape_unicode = true;       ///< Escape non-ASCII as \uXXXX
    static constexpr size_t max_parse_depth = 512;     ///< Maximum parsing depth
    static constexpr size_t max_dump_depth = 512;      ///< Maximum serialization depth
    static constexpr NumberFormat number_format = NumberFormat::Auto;  ///< Number formatting strategy
};

/**
 * @brief Pretty-printing JSON policy
 * 
 * Extends StandardJsonPolicy with human-readable formatting:
 * - Multi-line output with indentation
 * - Suitable for configuration files and debugging
 */
struct PrettyJsonPolicy : StandardJsonPolicy {
    static constexpr bool pretty_print = true;         ///< Enable pretty printing
};

/**
 * @brief Fixed-format JSON policy (legacy behavior)
 * 
 * Always uses fixed-point notation for numbers:
 * - Maintains backward compatibility with previous versions
 * - Good for values in range Â±1e-15 to Â±1e+15
 * - May produce very long strings for extreme values
 */
struct FixedFormatPolicy : StandardJsonPolicy {
    static constexpr NumberFormat number_format = NumberFormat::Fixed;  ///< Always fixed-point
};

/**
 * @brief Scientific notation JSON policy
 * 
 * Always uses scientific notation for floating-point numbers:
 * - Compact representation for all magnitudes
 * - Consistent format regardless of value range
 * - Ideal for scientific data
 */
struct ScientificFormatPolicy : StandardJsonPolicy {
    static constexpr NumberFormat number_format = NumberFormat::Scientific;  ///< Always scientific
};

/**
 * @brief Compatibility JSON policy
 * 
 * Relaxed policy for compatibility with non-standard JSON:
 * - Allows NaN and Infinity values
 * - Preserves Unicode characters in output
 * - Useful for JavaScript interoperability
 */
struct CompatJsonPolicy : StandardJsonPolicy {
    static constexpr bool allow_nan_inf = true;        ///< Allow NaN/Infinity values
    static constexpr bool escape_unicode = false;      ///< Output Unicode directly
};

// ====================================================================
// Policy Context System (Thread-Local Policy Stack)
// ====================================================================

namespace json_detail {
    /**
     * @brief Runtime policy context for thread-local policy settings
     * 
     * Allows macros to respect custom policies by storing policy configuration
     * in thread-local storage. This is necessary because macros cannot take
     * template parameters directly.
     */
    struct PolicyContext {
        int numeric_precision = 16;
        bool allow_nan_inf = false;
        bool escape_unicode = true;
        
        template <typename Policy>
        static PolicyContext from_policy() {
            PolicyContext ctx;
            ctx.numeric_precision = Policy::numeric_precision;
            ctx.allow_nan_inf = Policy::allow_nan_inf;
            ctx.escape_unicode = Policy::escape_unicode;
            return ctx;
        }
    };
    
    /** @brief Thread-local policy context stack pointer */
    inline thread_local PolicyContext* current_policy_context = nullptr;
    
    /**
     * @brief RAII guard for setting active policy context
     * 
     * Sets the current policy context for the duration of its lifetime.
     * Automatically restores the previous context on destruction.
     */
    template <typename Policy>
    struct PolicyScope {
        PolicyContext ctx;
        PolicyContext* prev;
        
        PolicyScope() : ctx(PolicyContext::from_policy<Policy>()), prev(current_policy_context) {
            current_policy_context = &ctx;
        }
        
        ~PolicyScope() { 
            current_policy_context = prev; 
        }
        
        // Non-copyable, non-movable
        PolicyScope(const PolicyScope&) = delete;
        PolicyScope& operator=(const PolicyScope&) = delete;
        PolicyScope(PolicyScope&&) = delete;
        PolicyScope& operator=(PolicyScope&&) = delete;
    };
    
    /** @brief Get the effective numeric precision (from context or default) */
    template <typename Policy>
    inline int get_effective_numeric_precision() {
        return current_policy_context ? current_policy_context->numeric_precision : Policy::numeric_precision;
    }
    
    /** @brief Get the effective allow_nan_inf setting (from context or default) */
    template <typename Policy>
    inline bool get_effective_allow_nan_inf() {
        return current_policy_context ? current_policy_context->allow_nan_inf : Policy::allow_nan_inf;
    }
    
    /** @brief Get the effective escape_unicode setting (from context or default) */
    template <typename Policy>
    inline bool get_effective_escape_unicode() {
        return current_policy_context ? current_policy_context->escape_unicode : Policy::escape_unicode;
    }
} // json_detail

// ====================================================================
// Detail Helpers
// ====================================================================

namespace json_detail {
    template <typename Os>
    inline void output_indent(Os& os, int indent) noexcept {
        for (int i = 0; i < indent; ++i) {
            os << ' ';
        }
    }

    template <typename Os, typename Policy>
    void escape_string(Os& os, std::string_view s) {
        bool escape_unicode = get_effective_escape_unicode<Policy>();
        os << '"';
        for (char c : s) {
            switch (c) {
                case '"': os << R"(\")"; break;
                case '\\': os << R"(\\)"; break;
                case '\b': os << R"(\b)"; break;
                case '\f': os << R"(\f)"; break;
                case '\n': os << R"(\n)"; break;
                case '\r': os << R"(\r)"; break;
                case '\t': os << R"(\t)"; break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        // Control characters MUST always be escaped per JSON spec (RFC 8259)
                        os << "\\u" << std::hex << std::setw(4) << std::setfill('0') 
                           << static_cast<int>(static_cast<unsigned char>(c)) << std::dec;
                    } else if (escape_unicode && static_cast<unsigned char>(c) > 0x7F) {
                        // Non-ASCII: escape if policy requires it
                        os << "\\u" << std::hex << std::setw(4) << std::setfill('0') 
                           << static_cast<int>(static_cast<unsigned char>(c)) << std::dec;
                    } else {
                        os << c;
                    }
                    break;
            }
        }
        os << '"';
    }

    template <typename Os, typename T, typename Policy>
    void dump_scalar(Os& os, const T& obj) {
        if constexpr (std::is_same_v<T, bool>) {
            os << (obj ? "true" : "false");
        } else if constexpr (std::is_same_v<T, int64_t>) {
            os << obj;
        } else if constexpr (std::is_arithmetic_v<T>) {
            if constexpr (std::is_floating_point_v<T>) {
                if (std::isnan(obj)) {
                    bool allow_nan_inf = get_effective_allow_nan_inf<Policy>();
                    if (allow_nan_inf) {
                        os << "NaN";
                    } else {
                        os << "null";
                    }
                } else if (std::isinf(obj)) {
                    bool allow_nan_inf = get_effective_allow_nan_inf<Policy>();
                    if (allow_nan_inf) {
                        os << (obj > 0 ? "Infinity" : "-Infinity");
                    } else {
                        os << "null";
                    }
                } else {
                    double intpart;
                    if (std::modf(obj, &intpart) == 0.0 && 
                        intpart >= std::numeric_limits<int64_t>::min() &&
                        intpart <= std::numeric_limits<int64_t>::max()) {
                        // Output as integer if no fractional part and fits in int64_t
                        os << static_cast<int64_t>(intpart);
                    } else {
                        // Floating-point output with fixed precision
                        // Note: std::fixed with precision=16 is optimal for typical values
                        // in the range ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â±1e-15 to ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â±1e+15. Values outside this range
                        // (e.g., Planck constant 6.626e-34, Avogadro's number 6.022e23)
                        // may produce very long strings or lose precision. For scientific
                        // data with extreme exponents, consider storing as strings or
                        // pre-formatting with std::scientific before serialization.
                        int numeric_precision = get_effective_numeric_precision<Policy>();
                        constexpr auto format = Policy::number_format;
                        
                        if constexpr (format == NumberFormat::Fixed) {
                            // Legacy behavior: always fixed-point notation
                            os << std::fixed << std::setprecision(numeric_precision) << obj;
                        } 
                        else if constexpr (format == NumberFormat::Scientific) {
                            // Always use scientific notation
                            os << std::scientific << std::setprecision(numeric_precision) << obj;
                        }
                        else if constexpr (format == NumberFormat::Shortest) {
                            // Default formatting (shortest representation)
                            os << std::setprecision(numeric_precision) << obj;
                        }
                        else { // NumberFormat::Auto
                            // Smart selection based on magnitude
                            double abs_val = std::abs(obj);
                            
                            // Use scientific notation for very small or very large numbers
                            // Range: values outside [1e-6, 1e15] use scientific notation
                            // This handles Planck constant (6.626e-34), Avogadro (6.022e23), etc.
                            if (abs_val != 0.0 && (abs_val < 1e-6 || abs_val > 1e15)) {
                                os << std::scientific << std::setprecision(numeric_precision) << obj;
                            } else {
                                os << std::fixed << std::setprecision(numeric_precision) << obj;
                            }
                        }
                    }
                }
            } else {
                os << static_cast<int64_t>(obj);
            }
        } else if constexpr (std::is_convertible_v<T, std::string_view>) {
            escape_string<Os, Policy>(os, obj);
        }
    }

    // Tuple dump implementation using index sequence
    template <typename Os, typename Policy, typename Tuple, std::size_t... I>
    void dump_tuple_impl(Os& os, const Tuple& tup, std::index_sequence<I...>, bool pretty, int indent);

    // Type trait to detect std::optional
    template <typename T>
    struct is_optional : std::false_type {};
    
    template <typename T>
    struct is_optional<std::optional<T>> : std::true_type {};
    
    template <typename T>
    inline constexpr bool is_optional_v = is_optional<T>::value;
    
    // Type trait to detect std::pair
    template <typename T>
    struct is_pair : std::false_type {};
    
    template <typename T1, typename T2>
    struct is_pair<std::pair<T1, T2>> : std::true_type {};
    
    template <typename T>
    inline constexpr bool is_pair_v = is_pair<T>::value;
    
    // Type trait to detect std::tuple
    template <typename T>
    struct is_tuple : std::false_type {};
    
    template <typename... Ts>
    struct is_tuple<std::tuple<Ts...>> : std::true_type {};
    
    template <typename T>
    inline constexpr bool is_tuple_v = is_tuple<T>::value;
    
    // Extract inner type from optional
    template <typename T>
    struct optional_value_type { using type = T; };
    
    template <typename T>
    struct optional_value_type<std::optional<T>> { using type = T; };
    
    template <typename T>
    using optional_value_type_t = typename optional_value_type<T>::type;

    // Trait to check if to_json(JsonValue&, const T&) exists
    template <typename T, typename = void>
    struct has_to_json : std::false_type {};
    
    template <typename T>
    struct has_to_json<T, std::void_t<decltype(to_json(std::declval<JsonValue&>(), std::declval<const T&>()))>> : std::true_type {};
    
    template <typename T>
    inline constexpr bool has_to_json_v = has_to_json<T>::value;

    // Helper to check dump depth
    template <typename Policy>
    inline void check_dump_depth(int indent) {
        size_t depth = static_cast<size_t>(indent / (Policy::indent_step > 0 ? Policy::indent_step : 1));
        if (depth > Policy::max_dump_depth) {
            throw std::runtime_error("JSON dump error: maximum nesting depth (" + 
                                   std::to_string(Policy::max_dump_depth) + ") exceeded");
        }
    }

}  // namespace json_detail

// ====================================================================
// JsonDispatcher Primary and Specializations
// ====================================================================

// Primary declaration
template <typename T, typename Policy, typename = void>
struct JsonDispatcher;

// Null pointer
template <typename Policy>
struct JsonDispatcher<std::nullptr_t, Policy> {
    template <typename Os>
    static void dump(Os& os, std::nullptr_t, bool = Policy::pretty_print, int = 0) {
        os << "null";
    }
};

// Scalar (non-iterable or string, excluding optional/pair/tuple)
template <typename T, typename Policy>
struct JsonDispatcher<T, Policy, std::enable_if_t<
    (!json_detail::IsIterable<T>::value || std::is_same_v<std::decay_t<T>, std::string>) &&
    !json_detail::is_optional_v<T> && 
    !json_detail::is_pair_v<T> && 
    !json_detail::is_tuple_v<T>
>> {
    template <typename Os>
    static void dump(Os& os, const T& obj, bool pretty = Policy::pretty_print, int indent = 0) {
        if constexpr (std::is_same_v<T, std::nullptr_t> || std::is_null_pointer_v<T>) {
            os << "null";
        } else if constexpr (!std::is_arithmetic_v<T> && !std::is_same_v<std::decay_t<T>, std::string> && 
                             !std::is_convertible_v<T, std::string_view> && json_detail::has_to_json_v<T>) {
            // User-defined type with custom to_json - convert to JsonValue first
            JsonValue j;
            to_json(j, obj);
            JsonDispatcher<JsonValue, Policy>::dump(os, j, pretty, indent);
        } else {
            json_detail::dump_scalar<Os, T, Policy>(os, obj);
        }
    }
};

// std::optional support
template <typename T, typename Policy>
struct JsonDispatcher<std::optional<T>, Policy> {
    template <typename Os>
    static void dump(Os& os, const std::optional<T>& opt, bool pretty = Policy::pretty_print, int indent = 0) {
        if (opt.has_value()) {
            JsonDispatcher<T, Policy>::dump(os, *opt, pretty, indent);
        } else {
            os << "null";
        }
    }
};

// Pair (as array)
template <typename T1, typename T2, typename Policy>
struct JsonDispatcher<std::pair<T1, T2>, Policy> {
    template <typename Os>
    static void dump(Os& os, const std::pair<T1, T2>& p, bool pretty = Policy::pretty_print, int indent = 0) {
        os << '[';
        if (pretty) os << '\n';
        if (pretty) json_detail::output_indent(os, indent + Policy::indent_step);
        JsonDispatcher<T1, Policy>::dump(os, p.first, pretty, indent + Policy::indent_step);
        os << ',';
        if (pretty) { os << '\n'; json_detail::output_indent(os, indent + Policy::indent_step); }
        JsonDispatcher<T2, Policy>::dump(os, p.second, pretty, indent + Policy::indent_step);
        if (pretty) { os << '\n'; json_detail::output_indent(os, indent); }
        os << ']';
    }
};

// Tuple (as array)
template <typename... Ts, typename Policy>
struct JsonDispatcher<std::tuple<Ts...>, Policy> {
    template <typename Os>
    static void dump(Os& os, const std::tuple<Ts...>& tup, bool pretty = Policy::pretty_print, int indent = 0) {
        json_detail::dump_tuple_impl<Os, Policy>(os, tup, std::make_index_sequence<sizeof...(Ts)>(), pretty, indent);
    }
};

// Iterable non-associative (as array)
template <typename T, typename Policy>
struct JsonDispatcher<T, Policy, std::enable_if_t<json_detail::IsIterable<T>::value && !std::is_same_v<std::decay_t<T>, std::string> && !json_detail::HasMappedType<T>::value>> {
    template <typename Os>
    static void dump(Os& os, const T& cont, bool pretty = Policy::pretty_print, int indent = 0) {
        json_detail::check_dump_depth<Policy>(indent);
        os << '[';
        if (pretty && !cont.empty()) os << '\n';
        bool first = true;
        for (const auto& elem : cont) {
            if (!first) os << ',';
            if (pretty) { os << '\n'; json_detail::output_indent(os, indent + Policy::indent_step); }
            first = false;
            JsonDispatcher<json_detail::ContainerValueT<T>, Policy>::dump(os, elem, pretty, indent + Policy::indent_step);
        }
        if (pretty && !cont.empty()) { os << '\n'; json_detail::output_indent(os, indent); }
        os << ']';
    }
};

// Associative (as object)
template <typename T, typename Policy>
struct JsonDispatcher<T, Policy, std::enable_if_t<json_detail::IsIterable<T>::value && !std::is_same_v<std::decay_t<T>, std::string> && json_detail::HasMappedType<T>::value>> {
    template <typename Os>
    static void dump(Os& os, const T& cont, bool pretty = Policy::pretty_print, int indent = 0) {
        json_detail::check_dump_depth<Policy>(indent);
        os << '{';
        if (pretty && !cont.empty()) os << '\n';
        bool first = true;
        for (const auto& elem : cont) {
            if (!first) os << ',';
            if (pretty) { os << '\n'; json_detail::output_indent(os, indent + Policy::indent_step); }
            first = false;
            // Key must be convertible to string
            if constexpr (std::is_convertible_v<typename T::key_type, std::string_view>) {
                json_detail::escape_string<Os, Policy>(os, elem.first);
            } else {
                std::ostringstream key_stream;
                key_stream << elem.first;
                json_detail::escape_string<Os, Policy>(os, key_stream.str());
            }
            os << (pretty ? " : " : ":");
            JsonDispatcher<typename T::mapped_type, Policy>::dump(os, elem.second, pretty, indent + Policy::indent_step);
        }
        if (pretty && !cont.empty()) { os << '\n'; json_detail::output_indent(os, indent); }
        os << '}';
    }
};

// JsonValue (using variant visit)
template <typename Policy>
struct JsonDispatcher<JsonValue, Policy> {
    template <typename Os>
    static void dump(Os& os, const JsonValue& val, bool pretty = Policy::pretty_print, int indent = 0) {
        std::visit([&](auto&& arg) {
            JsonDispatcher<std::decay_t<decltype(arg)>, Policy>::dump(os, arg, pretty, indent);
        }, val);
    }
};

// Now define dump_tuple_impl after JsonDispatcher is fully declared
namespace json_detail {
    template <typename Os, typename Policy, typename Tuple, std::size_t... I>
    void dump_tuple_impl(Os& os, const Tuple& tup, std::index_sequence<I...>, bool pretty, int indent) {
        os << '[';
        if (pretty && sizeof...(I) > 0) os << '\n';
        bool first = true;
        (..., ([&]() {
            if (!first) os << ',';
            if (pretty) { os << '\n'; output_indent(os, indent + Policy::indent_step); }
            first = false;
            JsonDispatcher<std::tuple_element_t<I, Tuple>, Policy>::dump(os, std::get<I>(tup), pretty, indent + Policy::indent_step);
        })());
        if (pretty && sizeof...(I) > 0) { os << '\n'; output_indent(os, indent); }
        os << ']';
    }
}  // namespace json_detail

// ====================================================================
// High-Level API Functions
// ====================================================================

/**
 * @brief Serialize an object to a JSON string
 * 
 * @tparam T Type of object to serialize (must have to_json defined or be a
 *      basic type)
 * @tparam Policy Formatting policy (default: StandardJsonPolicy)
 * @param obj Object to serialize
 * @param pretty Enable pretty-printing (default: from policy)
 * @return std::string JSON representation
 * 
 * @note For custom types, use CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE or
 *      implement to_json
 */
template <typename T, typename Policy = StandardJsonPolicy>
std::string to_json_string(const T& obj, bool pretty = Policy::pretty_print) {
    json_detail::PolicyScope<Policy> scope;  // Set policy context for macro-generated functions
    std::ostringstream oss;
    JsonDispatcher<T, Policy>::dump(oss, obj, pretty);
    return oss.str();
}

/**
 * @brief Serialize an object to an output stream
 * 
 * @tparam T Type of object to serialize
 * @tparam Policy Formatting policy (default: StandardJsonPolicy)
 * @tparam Os Output stream type
 * @param os Output stream
 * @param obj Object to serialize
 * @param pretty Enable pretty-printing (default: from policy)
 */
template <typename T, typename Policy = StandardJsonPolicy, typename Os>
void to_json_stream(Os& os, const T& obj, bool pretty = Policy::pretty_print) {
    json_detail::PolicyScope<Policy> scope;  // Set policy context for macro-generated functions
    JsonDispatcher<T, Policy>::dump(os, obj, pretty);
}

/**
 * @brief Identity function for JsonValue
 * @param value JsonValue to return
 * @return JsonValue Copy of input value
 */
inline JsonValue to_json(const JsonValue& value) {
    return value;
}

// ====================================================================
// Automatic Serialization Macros
// ====================================================================

#define USING_JSON_LITE() \
    using namespace cpp_utilities; \
    using cpp_utilities::to_json; \
    using cpp_utilities::from_json;


/**
 * @defgroup macros Automatic Serialization Macros
 * @brief Macros for automatic generation of to_json/from_json functions
 * 
 * These macros generate serialization functions for custom structs, supporting
 * up to 50 fields per struct. Three variants are provided:
 * 
 * - CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE: Define outside class (most common)
 * - CPP_JSON_DEFINE_TYPE_OPTIONAL: All fields optional (no "missing field" errors)
 * - CPP_JSON_DEFINE_TYPE_INTRUSIVE: Define inside class (for private members)
 * 
 * @{
 */

// MSVC workaround: needs extra expansion layer
#define CPP_JSON_EXPAND(x) x

#define CPP_JSON_ARG_COUNT_IMPL(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32,_33,_34,_35,_36,_37,_38,_39,_40,_41,_42,_43,_44,_45,_46,_47,_48,_49,_50,N,...) N
#define CPP_JSON_ARG_COUNT(...) CPP_JSON_EXPAND(CPP_JSON_ARG_COUNT_IMPL(__VA_ARGS__,50,49,48,47,46,45,44,43,42,41,40,39,38,37,36,35,34,33,32,31,30,29,28,27,26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1))

#define CPP_JSON_CAT(a, b) CPP_JSON_CAT_IMPL(a, b)
#define CPP_JSON_CAT_IMPL(a, b) a##b

// Macro hygiene: wrapped in do-while(0) to behave like statements
#define CPP_JSON_TO_FIELD(field) \
    do { obj[#field] = to_json(value.field); } while(0)

#define CPP_JSON_FROM_FIELD(field) \
    do { \
        if (auto it = obj.find(#field); it != obj.end()) { \
            try { \
                from_json(it->second, value.field); \
            } catch (const std::exception& e) { \
                throw std::runtime_error(std::string("Error deserializing field '") + #field + "': " + e.what()); \
            } \
        } else if constexpr (!cpp_utilities::json_detail::is_optional_v<decltype((value.field))>) { \
            throw std::runtime_error("Required field missing: '" #field "'"); \
        } \
    } while(0)

#define CPP_JSON_FROM_FIELD_OPT(field) \
    do { \
        if (auto it = obj.find(#field); it != obj.end()) { \
            from_json(it->second, value.field); \
        } \
    } while(0)

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

// Improved error handling for 51+ fields
// This will trigger a better compile-time error message
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
 * @brief Define JSON serialization for a struct (non-intrusive)
 * 
 * Use this macro outside the struct definition to generate to_json and from_json
 * functions. Supports up to 50 fields. All fields are required unless declared
 * as std::optional.
 * 
 * Uses [[maybe_unused]] attribute to minimize warnings. For types in custom
 * namespaces, invoke the macro inside the namespace for proper ADL.
 * 
 * @param Type The struct type name
 * @param ... Field names (up to 50 fields)
 * 
 * Example:
 * @code
 * struct Point { int x, y; };
 * CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Point, x, y)
 * 
 * // For namespaced types, invoke inside the namespace:
 * namespace myapp {
 *     struct Config { int port; };
 *     CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Config, port)
 * }
 * @endcode
 * 
 * @note If you need custom serialization logic, implement to_json/from_json
 *       manually instead of using this macro.
 */
#define CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Type, ...)                                           \
[[maybe_unused]] inline void to_json(cpp_utilities::JsonValue& j, const Type& value) {         \
    cpp_utilities::JsonObject obj;                                                              \
    CPP_JSON_FOR_EACH(CPP_JSON_TO_FIELD, __VA_ARGS__)                                           \
    j = std::move(obj);                                                                         \
}                                                                                               \
[[maybe_unused]] inline void from_json(const cpp_utilities::JsonValue& j, Type& value) {       \
    if (!j.is_object()) throw std::runtime_error("JSON type mismatch: expected object");       \
    const auto& obj = std::get<cpp_utilities::JsonObject>(j);                                  \
    CPP_JSON_FOR_EACH(CPP_JSON_FROM_FIELD, __VA_ARGS__)                                         \
}

/**
 * @brief Define JSON serialization with all optional fields
 * 
 * Similar to CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE but treats all fields as optional.
 * Missing fields in JSON will not cause errors; struct fields retain their
 * existing values.
 * 
 * Uses [[maybe_unused]] attribute to minimize warnings about unused functions.
 * 
 * @param Type The struct type name
 * @param ... Field names (up to 50 fields)
 * 
 * Example:
 * @code
 * struct Config { int port = 8080; std::string host = "localhost"; };
 * CPP_JSON_DEFINE_TYPE_OPTIONAL(Config, port, host)
 * @endcode
 */
#define CPP_JSON_DEFINE_TYPE_OPTIONAL(Type, ...)                                                \
[[maybe_unused]] inline void to_json(cpp_utilities::JsonValue& j, const Type& value) {         \
    cpp_utilities::JsonObject obj;                                                              \
    CPP_JSON_FOR_EACH(CPP_JSON_TO_FIELD, __VA_ARGS__)                                           \
    j = std::move(obj);                                                                         \
}                                                                                               \
[[maybe_unused]] inline void from_json(const cpp_utilities::JsonValue& j, Type& value) {       \
    if (!j.is_object()) {                                                                       \
        throw std::runtime_error("JSON type mismatch: expected object");                        \
    }                                                                                           \
    const auto& obj = std::get<cpp_utilities::JsonObject>(j);                                  \
    CPP_JSON_FOR_EACH(CPP_JSON_FROM_FIELD_OPT, __VA_ARGS__)                                     \
}

/**
 * @brief Define JSON serialization within a class (intrusive)
 * 
 * Use this macro inside a class definition to enable serialization of private
 * members. The generated functions are declared as friends and injected into
 * the enclosing namespace via ADL (Argument Dependent Lookup).
 * 
 * @param Type The class type name
 * @param ... Field names, including private members (up to 50 fields)
 * 
 * Example:
 * @code
 * class Secret {
 *     int value_;
 * public:
 *     CPP_JSON_DEFINE_TYPE_INTRUSIVE(Secret, value_)
 * };
 * @endcode
 * 
 * @note Friend functions are injected into the enclosing namespace and found
 *       via ADL. Collisions are rare since the functions are only visible when
 *       the class type is involved in the call.
 */
#define CPP_JSON_DEFINE_TYPE_INTRUSIVE(Type, ...)                                               \
[[maybe_unused]] friend void to_json(cpp_utilities::JsonValue& j, const Type& value) {          \
    cpp_utilities::JsonObject obj;                                                              \
    CPP_JSON_FOR_EACH(CPP_JSON_TO_FIELD, __VA_ARGS__)                                           \
    j = std::move(obj);                                                                         \
}                                                                                               \
[[maybe_unused]] friend void from_json(const cpp_utilities::JsonValue& j, Type& value) {        \
    if (!j.is_object()){                                                                        \
        throw std::runtime_error("JSON type mismatch: expected object");                        \
    }                                                                                           \
    const auto& obj = std::get<cpp_utilities::JsonObject>(j);                                   \
    CPP_JSON_FOR_EACH(CPP_JSON_FROM_FIELD, __VA_ARGS__)                                         \
}

/** @} */ // end of macros group

// ====================================================================
// JSON Parser
// ====================================================================

/**
 * @defgroup parser JSON Parser Functions
 * @brief Recursive descent parser for JSON strings
 * 
 * Implements a single-pass recursive descent parser with:
 * - Full UTF-16 surrogate pair support
 * - Configurable depth limits (default: 512 levels)
 * - Position information in all error messages
 * - Automatic int64_t vs double selection for numbers
 * - Optional NaN/Infinity support (policy-controlled)
 * 
 * @{
 */

namespace json_detail {
    inline void skip_whitespace(std::string_view s, size_t& pos) noexcept {
        while (pos < s.size() &&
                        std::isspace(static_cast<unsigned char>(s[pos]))){
            ++pos;
        }
    }

    inline std::string parse_string(std::string_view s, size_t& pos) {
        if (pos >= s.size() || s[pos] != '"') {
            throw std::runtime_error("JSON parse error: expected string at position " + std::to_string(pos));
        }
        ++pos;
        std::string res;
        res.reserve(64);  // Pre-allocate typical string size
        
        while (pos < s.size() && s[pos] != '"') {
            if (s[pos] == '\\') {
                ++pos;
                if (pos >= s.size()) {
                    throw std::runtime_error("JSON parse error: invalid escape sequence at position " + std::to_string(pos));
                }
                switch (s[pos]) {
                    case '"': res += '"'; break;
                    case '\\': res += '\\'; break;
                    case '/': res += '/'; break;
                    case 'b': res += '\b'; break;
                    case 'f': res += '\f'; break;
                    case 'n': res += '\n'; break;
                    case 'r': res += '\r'; break;
                    case 't': res += '\t'; break;
                    case 'u': {
                        // Unicode escape: \uXXXX with UTF-16 surrogate pair support
                        if (pos + 4 >= s.size()) 
                            throw std::runtime_error("JSON parse error: invalid unicode escape at position " + std::to_string(pos));
                        std::string hex = std::string(s.substr(pos + 1, 4));
                        uint32_t codepoint;
                        try {
                            codepoint = static_cast<uint32_t>(std::stoul(hex, nullptr, 16));
                        } catch (...) {
                            throw std::runtime_error("JSON parse error: invalid unicode hex at position " + std::to_string(pos));
                        }
                        pos += 4;
                        
                        // Handle UTF-16 surrogate pairs
                        if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
                            // High surrogate - need low surrogate
                            if (pos + 6 >= s.size() || s[pos + 1] != '\\' || s[pos + 2] != 'u') {
                                throw std::runtime_error("JSON parse error: incomplete surrogate pair at position " + std::to_string(pos));
                            }
                            std::string low_hex = std::string(s.substr(pos + 3, 4));
                            uint32_t low_surrogate;
                            try {
                                low_surrogate = static_cast<uint32_t>(std::stoul(low_hex, nullptr, 16));
                            } catch (...) {
                                throw std::runtime_error("JSON parse error: invalid low surrogate at position " + std::to_string(pos));
                            }
                            if (low_surrogate < 0xDC00 || low_surrogate > 0xDFFF) {
                                throw std::runtime_error("JSON parse error: invalid low surrogate value at position " + std::to_string(pos));
                            }
                            pos += 6;
                            // Combine surrogates
                            codepoint = 0x10000 + ((codepoint & 0x3FF) << 10) + (low_surrogate & 0x3FF);
                        } else if (codepoint >= 0xDC00 && codepoint <= 0xDFFF) {
                            throw std::runtime_error("JSON parse error: unexpected low surrogate at position " + std::to_string(pos));
                        }
                        
                        // Encode as UTF-8
                        if (codepoint <= 0x7F) {
                            res += static_cast<char>(codepoint);
                        } else if (codepoint <= 0x7FF) {
                            res += static_cast<char>(0xC0 | (codepoint >> 6));
                            res += static_cast<char>(0x80 | (codepoint & 0x3F));
                        } else if (codepoint <= 0xFFFF) {
                            res += static_cast<char>(0xE0 | (codepoint >> 12));
                            res += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                            res += static_cast<char>(0x80 | (codepoint & 0x3F));
                        } else if (codepoint <= 0x10FFFF) {
                            res += static_cast<char>(0xF0 | (codepoint >> 18));
                            res += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
                            res += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                            res += static_cast<char>(0x80 | (codepoint & 0x3F));
                        } else {
                            throw std::runtime_error("JSON parse error: invalid unicode codepoint at position " + std::to_string(pos));
                        }
                        break;
                    }
                    default: 
                        throw std::runtime_error(std::string("JSON parse error: invalid escape character '") + s[pos] + "' at position " + std::to_string(pos));
                }
            } else {
                res += s[pos];
            }
            ++pos;
        }
        if (pos >= s.size() || s[pos] != '"') 
            throw std::runtime_error("JSON parse error: unterminated string at position " + std::to_string(pos));
        ++pos;
        return res;
    }

    inline JsonValue parse_number(std::string_view s, size_t& pos) {
        size_t start = pos;
        
        // Handle NaN and Infinity (non-standard JSON, policy-controlled)
        if (s.substr(pos, 3) == "NaN") {
            pos += 3;
            return std::numeric_limits<double>::quiet_NaN();
        }
        if (s.substr(pos, 8) == "Infinity") {
            pos += 8;
            return std::numeric_limits<double>::infinity();
        }
        if (s.substr(pos, 9) == "-Infinity") {
            pos += 9;
            return -std::numeric_limits<double>::infinity();
        }
        
        // Numbers must start with digit or minus sign
        if (pos >= s.size() || (!std::isdigit(s[pos]) && s[pos] != '-')) {
            throw std::runtime_error("JSON parse error: invalid number at position " + std::to_string(pos));
        }
        
        if (s[pos] == '-') {
            ++pos;
            if (pos >= s.size() || !std::isdigit(s[pos])) {
                throw std::runtime_error("JSON parse error: invalid number after '-' at position " + std::to_string(pos));
            }
        }
        
        // Detect if number has decimal point or exponent
        bool has_decimal = false;
        bool has_exponent = false;
        size_t scan_pos = pos;
        
        while (scan_pos < s.size()) {
            char c = s[scan_pos];
            if (c == '.') has_decimal = true;
            else if (c == 'e' || c == 'E') has_exponent = true;
            else if (!std::isdigit(c) && c != '+' && c != '-') break;
            ++scan_pos;
        }
        
        std::string num_str = std::string(s.substr(start, scan_pos - start));
        pos = scan_pos;
        
        // Parse as int64_t if no decimal/exponent to preserve integer precision
        if (!has_decimal && !has_exponent) {
            try {
                int64_t int_val = std::stoll(num_str);
                return int_val;
            } catch (const std::out_of_range&) {
                // Fallback to double if out of int64 range
                try {
                    return std::stod(num_str);
                } catch (...) {
                    throw std::runtime_error("JSON parse error: invalid number '" + num_str + "' at position " + std::to_string(start));
                }
            } catch (...) {
                throw std::runtime_error("JSON parse error: invalid number '" + num_str + "' at position " + std::to_string(start));
            }
        }
        
        // Has decimal or exponent - parse as double
        try {
            return std::stod(num_str);
        } catch (...) {
            throw std::runtime_error("JSON parse error: invalid number '" + num_str + "' at position " + std::to_string(start));
        }
    }

    JsonValue parse_value(std::string_view s, size_t& pos, size_t depth = 0);

    template <typename Policy = StandardJsonPolicy>
    inline JsonArray parse_array(std::string_view s, size_t& pos, size_t depth) {
        if (depth > Policy::max_parse_depth) {
            throw std::runtime_error("JSON parse error: maximum nesting depth (" + 
                                   std::to_string(Policy::max_parse_depth) + ") exceeded at position " + std::to_string(pos));
        }
        JsonArray arr;
        ++pos;
        skip_whitespace(s, pos);
        if (pos < s.size() && s[pos] == ']') {
            ++pos;
            return arr;
        }
        while (pos < s.size()) {
            arr.push_back(parse_value(s, pos, depth + 1));
            skip_whitespace(s, pos);
            if (pos >= s.size()) 
                throw std::runtime_error("JSON parse error: unterminated array at position " + std::to_string(pos));
            if (s[pos] == ']') {
                ++pos;
                return arr;
            }
            if (s[pos] != ',') 
                throw std::runtime_error("JSON parse error: expected ',' or ']' in array at position " + std::to_string(pos));
            ++pos;
            skip_whitespace(s, pos);
        }
        throw std::runtime_error("JSON parse error: unterminated array");
    }

    template <typename Policy = StandardJsonPolicy>
    inline JsonObject parse_object(std::string_view s, size_t& pos, size_t depth) {
        if (depth > Policy::max_parse_depth) {
            throw std::runtime_error("JSON parse error: maximum nesting depth (" + 
                                   std::to_string(Policy::max_parse_depth) + ") exceeded at position " + std::to_string(pos));
        }
        JsonObject obj;
        ++pos;
        skip_whitespace(s, pos);
        if (pos < s.size() && s[pos] == '}') {
            ++pos;
            return obj;
        }
        while (pos < s.size()) {
            skip_whitespace(s, pos);
            if (pos >= s.size() || s[pos] != '"') 
                throw std::runtime_error("JSON parse error: expected string key at position " + std::to_string(pos));
            std::string key = parse_string(s, pos);
            skip_whitespace(s, pos);
            if (pos >= s.size() || s[pos] != ':') 
                throw std::runtime_error("JSON parse error: expected ':' after object key '" + key + "' at position " + std::to_string(pos));
            ++pos;
            skip_whitespace(s, pos);
            obj[std::move(key)] = parse_value(s, pos, depth + 1);
            skip_whitespace(s, pos);
            if (pos >= s.size()) 
                throw std::runtime_error("JSON parse error: unterminated object at position " + std::to_string(pos));
            if (s[pos] == '}') {
                ++pos;
                return obj;
            }
            if (s[pos] != ',') 
                throw std::runtime_error("JSON parse error: expected ',' or '}' in object at position " + std::to_string(pos));
            ++pos;
        }
        throw std::runtime_error("JSON parse error: unterminated object");
    }

    inline JsonValue parse_value(std::string_view s, size_t& pos, size_t depth) {
        skip_whitespace(s, pos);
        if (pos >= s.size()) 
            throw std::runtime_error("JSON parse error: unexpected end of input");
        
        char c = s[pos];
        if (c == '{') return parse_object(s, pos, depth);
        if (c == '[') return parse_array(s, pos, depth);
        if (c == '"') return parse_string(s, pos);
        
        // Check for literals
        if (s.substr(pos, 4) == "true") { pos += 4; return true; }
        if (s.substr(pos, 5) == "false") { pos += 5; return false; }
        if (s.substr(pos, 4) == "null") { pos += 4; return nullptr; }
        
        // Numbers (including optional NaN/Infinity)
        if (std::isdigit(c) || c == '-' || s.substr(pos, 3) == "NaN" || 
            s.substr(pos, 8) == "Infinity" || s.substr(pos, 9) == "-Infinity") {
            return parse_number(s, pos);
        }
        
        throw std::runtime_error("JSON parse error: invalid value at position " + std::to_string(pos));
    }
}  // namespace json_detail

/**
 * @brief Parse a JSON string into a JsonValue
 * 
 * Parses a complete JSON value from a string_view. The input must contain
 * exactly one JSON value with no trailing data.
 * 
 * @tparam Policy Parsing policy (default: StandardJsonPolicy)
 * @param json JSON string to parse
 * @return JsonValue Parsed JSON value
 * @throws std::runtime_error On parse errors with position information
 * 
 * @note Entire string must be consumed. Trailing whitespace is allowed but
 *       trailing non-whitespace causes an error.
 */
template <typename Policy = StandardJsonPolicy>
[[nodiscard]] inline JsonValue parse_json(std::string_view json) {
    size_t pos = 0;
    JsonValue val = json_detail::parse_value(json, pos);
    json_detail::skip_whitespace(json, pos);
    if (pos != json.size()) 
        throw std::runtime_error("JSON parse error: extra data after JSON value at position " + std::to_string(pos));
    return val;
}

/** @} */ // end of parser group

// ====================================================================
// Type Conversion Functions
// ====================================================================

/**
 * @defgroup conversion Type Conversion Functions
 * @brief Functions to convert between C++ types and JSON
 * 
 * Provides bidirectional conversion between C++ types and JsonValue:
 * - to_json: Convert C++ type to JsonValue
 * - from_json: Extract C++ type from JsonValue with validation
 * 
 * All numeric conversions include range checking and fractional part
 * validation to prevent silent data loss.
 * 
 * @{
 */

// Forward declare for use in map deserialization
template <typename T>
[[nodiscard]] T from_json_string(std::string_view json_str);

// Basic types - to_json (output parameter version for consistency with macros)
inline void to_json(JsonValue& j, std::nullptr_t) noexcept { j = nullptr; }
inline void to_json(JsonValue& j, bool value) noexcept { j = value; }
inline void to_json(JsonValue& j, int value) noexcept { j = static_cast<int64_t>(value); }
inline void to_json(JsonValue& j, unsigned int value) noexcept { j = static_cast<int64_t>(value); }
inline void to_json(JsonValue& j, long value) noexcept { j = static_cast<int64_t>(value); }
inline void to_json(JsonValue& j, unsigned long value) noexcept { 
    if (value > static_cast<unsigned long>(std::numeric_limits<int64_t>::max())) {
        j = static_cast<double>(value);  // Fallback to double for large unsigned
    } else {
        j = static_cast<int64_t>(value);
    }
}
inline void to_json(JsonValue& j, long long value) noexcept { j = static_cast<int64_t>(value); }
inline void to_json(JsonValue& j, unsigned long long value) noexcept { 
    if (value > static_cast<unsigned long long>(std::numeric_limits<int64_t>::max())) {
        j = static_cast<double>(value);  // Fallback to double for large unsigned
    } else {
        j = static_cast<int64_t>(value);
    }
}
inline void to_json(JsonValue& j, float value) noexcept { j = static_cast<double>(value); }
inline void to_json(JsonValue& j, double value) noexcept { j = value; }
inline void to_json(JsonValue& j, const std::string& value) { j = value; }
inline void to_json(JsonValue& j, const char* value) { j = std::string(value); }
inline void to_json(JsonValue& j, std::string_view value) { j = std::string(value); }

// Additional fixed-width integer types not covered by standard types
// (signed char, short, unsigned char, unsigned short)
inline void to_json(JsonValue& j, signed char value) noexcept { j = static_cast<int64_t>(value); }
inline void to_json(JsonValue& j, unsigned char value) noexcept { j = static_cast<int64_t>(value); }
inline void to_json(JsonValue& j, short value) noexcept { j = static_cast<int64_t>(value); }
inline void to_json(JsonValue& j, unsigned short value) noexcept { j = static_cast<int64_t>(value); }

// Return-value versions for backward compatibility and direct usage
inline JsonValue to_json(std::nullptr_t) noexcept { return nullptr; }
inline JsonValue to_json(bool value) noexcept { return value; }
inline JsonValue to_json(int value) noexcept { return static_cast<int64_t>(value); }
inline JsonValue to_json(unsigned int value) noexcept { return static_cast<int64_t>(value); }
inline JsonValue to_json(long value) noexcept { return static_cast<int64_t>(value); }
inline JsonValue to_json(unsigned long value) noexcept { 
    if (value > static_cast<unsigned long>(std::numeric_limits<int64_t>::max())) {
        return static_cast<double>(value);
    }
    return static_cast<int64_t>(value);
}
inline JsonValue to_json(long long value) noexcept { return static_cast<int64_t>(value); }
inline JsonValue to_json(unsigned long long value) noexcept { 
    if (value > static_cast<unsigned long long>(std::numeric_limits<int64_t>::max())) {
        return static_cast<double>(value);
    }
    return static_cast<int64_t>(value);
}
inline JsonValue to_json(float value) noexcept { return static_cast<double>(value); }
inline JsonValue to_json(double value) noexcept { return value; }
inline JsonValue to_json(const std::string& value) { return value; }
inline JsonValue to_json(const char* value) { return std::string(value); }
inline JsonValue to_json(std::string_view value) { return std::string(value); }

// Additional fixed-width integer types not covered by standard types
// (signed char, short, unsigned char, unsigned short)
inline JsonValue to_json(signed char value) noexcept { return static_cast<int64_t>(value); }
inline JsonValue to_json(unsigned char value) noexcept { return static_cast<int64_t>(value); }
inline JsonValue to_json(short value) noexcept { return static_cast<int64_t>(value); }
inline JsonValue to_json(unsigned short value) noexcept { return static_cast<int64_t>(value); }

// Basic types - from_json (output parameter version with validation)
inline void from_json(const JsonValue& j, bool& value) {
    if (!j.is_bool()) throw std::runtime_error("JSON type mismatch: expected bool");
    value = std::get<bool>(j);
}

inline void from_json(const JsonValue& j, int& value) {
    if (j.is_int()) {
        int64_t i64 = std::get<int64_t>(j);
        if (i64 < std::numeric_limits<int>::min() || i64 > std::numeric_limits<int>::max()) {
            throw std::runtime_error("JSON value out of range for int: " + std::to_string(i64));
        }
        value = static_cast<int>(i64);
    } else if (j.is_number()) {
        double d = std::get<double>(j);
        double intpart;
        if (std::modf(d, &intpart) != 0.0) {
            throw std::runtime_error("JSON value has fractional part, cannot convert to int: " + std::to_string(d));
        }
        if (intpart < std::numeric_limits<int>::min() || intpart > std::numeric_limits<int>::max()) {
            throw std::runtime_error("JSON value out of range for int: " + std::to_string(d));
        }
        value = static_cast<int>(intpart);
    } else {
        throw std::runtime_error("JSON type mismatch: expected number");
    }
}

inline void from_json(const JsonValue& j, unsigned int& value) {
    if (j.is_int()) {
        int64_t i64 = std::get<int64_t>(j);
        if (i64 < 0 || i64 > std::numeric_limits<unsigned int>::max()) {
            throw std::runtime_error("JSON value out of range for unsigned int: " + std::to_string(i64));
        }
        value = static_cast<unsigned int>(i64);
    } else if (j.is_number()) {
        double d = std::get<double>(j);
        double intpart;
        if (std::modf(d, &intpart) != 0.0) {
            throw std::runtime_error("JSON value has fractional part, cannot convert to unsigned int: " + std::to_string(d));
        }
        if (intpart < 0 || intpart > std::numeric_limits<unsigned int>::max()) {
            throw std::runtime_error("JSON value out of range for unsigned int: " + std::to_string(d));
        }
        value = static_cast<unsigned int>(intpart);
    } else {
        throw std::runtime_error("JSON type mismatch: expected number");
    }
}

inline void from_json(const JsonValue& j, long& value) {
    if (j.is_int()) {
        int64_t i64 = std::get<int64_t>(j);
        if (i64 < std::numeric_limits<long>::min() || i64 > std::numeric_limits<long>::max()) {
            throw std::runtime_error("JSON value out of range for long: " + std::to_string(i64));
        }
        value = static_cast<long>(i64);
    } else if (j.is_number()) {
        double d = std::get<double>(j);
        double intpart;
        if (std::modf(d, &intpart) != 0.0) {
            throw std::runtime_error("JSON value has fractional part, cannot convert to long: " + std::to_string(d));
        }
        if (intpart < std::numeric_limits<long>::min() || intpart > std::numeric_limits<long>::max()) {
            throw std::runtime_error("JSON value out of range for long: " + std::to_string(d));
        }
        value = static_cast<long>(intpart);
    } else {
        throw std::runtime_error("JSON type mismatch: expected number");
    }
}

inline void from_json(const JsonValue& j, unsigned long& value) {
    if (j.is_int()) {
        int64_t i64 = std::get<int64_t>(j);
        if (i64 < 0) {
            throw std::runtime_error("JSON value is negative, cannot convert to unsigned long: " + std::to_string(i64));
        }
        value = static_cast<unsigned long>(i64);
    } else if (j.is_number()) {
        double d = std::get<double>(j);
        if (d < 0) {
            throw std::runtime_error("JSON value is negative, cannot convert to unsigned long");
        }
        value = static_cast<unsigned long>(d);
    } else {
        throw std::runtime_error("JSON type mismatch: expected number");
    }
}

inline void from_json(const JsonValue& j, long long& value) {
    if (j.is_int()) {
        int64_t i64 = std::get<int64_t>(j);
        if (i64 < std::numeric_limits<long long>::min() || i64 > std::numeric_limits<long long>::max()) {
            throw std::runtime_error("JSON value out of range for long long: " + std::to_string(i64));
        }
        value = static_cast<long long>(i64);
    } else if (j.is_number()) {
        double d = std::get<double>(j);
        double intpart;
        if (std::modf(d, &intpart) != 0.0) {
            throw std::runtime_error("JSON value has fractional part, cannot convert to long long: " + std::to_string(d));
        }
        if (intpart < std::numeric_limits<long long>::min() || intpart > std::numeric_limits<long long>::max()) {
            throw std::runtime_error("JSON value out of range for long long: " + std::to_string(d));
        }
        value = static_cast<long long>(intpart);
    } else {
        throw std::runtime_error("JSON type mismatch: expected number");
    }
}

inline void from_json(const JsonValue& j, unsigned long long& value) {
    if (j.is_int()) {
        int64_t i64 = std::get<int64_t>(j);
        if (i64 < 0) {
            throw std::runtime_error("JSON value is negative, cannot convert to unsigned long long: " + std::to_string(i64));
        }
        value = static_cast<unsigned long long>(i64);
    } else if (j.is_number()) {
        double d = std::get<double>(j);
        if (d < 0) {
            throw std::runtime_error("JSON value is negative, cannot convert to unsigned long long");
        }
        value = static_cast<unsigned long long>(d);
    } else {
        throw std::runtime_error("JSON type mismatch: expected number");
    }
}

inline void from_json(const JsonValue& j, float& value) {
    if (j.is_int()) {
        value = static_cast<float>(std::get<int64_t>(j));
    } else if (j.is_number()) {
        value = static_cast<float>(std::get<double>(j));
    } else {
        throw std::runtime_error("JSON type mismatch: expected number");
    }
}

inline void from_json(const JsonValue& j, double& value) {
    if (j.is_int()) {
        value = static_cast<double>(std::get<int64_t>(j));
    } else if (j.is_number()) {
        value = std::get<double>(j);
    } else {
        throw std::runtime_error("JSON type mismatch: expected number");
    }
}

inline void from_json(const JsonValue& j, std::string& value) {
    if (!j.is_string()) throw std::runtime_error("JSON type mismatch: expected string");
    value = std::get<std::string>(j);
}

// Additional fixed-width integer types not covered by standard types
// (signed char, short, unsigned char, unsigned short)
inline void from_json(const JsonValue& j, signed char& value) {
    if (j.is_int()) {
        int64_t i64 = std::get<int64_t>(j);
        if (i64 < std::numeric_limits<signed char>::min() || i64 > std::numeric_limits<signed char>::max()) {
            throw std::runtime_error("JSON value out of range for signed char: " + std::to_string(i64));
        }
        value = static_cast<signed char>(i64);
    } else if (j.is_number()) {
        double d = std::get<double>(j);
        double intpart;
        if (std::modf(d, &intpart) != 0.0) {
            throw std::runtime_error("JSON value has fractional part, cannot convert to signed char: " + std::to_string(d));
        }
        if (intpart < std::numeric_limits<signed char>::min() || intpart > std::numeric_limits<signed char>::max()) {
            throw std::runtime_error("JSON value out of range for signed char: " + std::to_string(d));
        }
        value = static_cast<signed char>(intpart);
    } else {
        throw std::runtime_error("JSON type mismatch: expected number");
    }
}

inline void from_json(const JsonValue& j, unsigned char& value) {
    if (j.is_int()) {
        int64_t i64 = std::get<int64_t>(j);
        if (i64 < 0 || i64 > std::numeric_limits<unsigned char>::max()) {
            throw std::runtime_error("JSON value out of range for unsigned char: " + std::to_string(i64));
        }
        value = static_cast<unsigned char>(i64);
    } else if (j.is_number()) {
        double d = std::get<double>(j);
        double intpart;
        if (std::modf(d, &intpart) != 0.0) {
            throw std::runtime_error("JSON value has fractional part, cannot convert to unsigned char: " + std::to_string(d));
        }
        if (intpart < 0 || intpart > std::numeric_limits<unsigned char>::max()) {
            throw std::runtime_error("JSON value out of range for unsigned char: " + std::to_string(d));
        }
        value = static_cast<unsigned char>(intpart);
    } else {
        throw std::runtime_error("JSON type mismatch: expected number");
    }
}

inline void from_json(const JsonValue& j, short& value) {
    if (j.is_int()) {
        int64_t i64 = std::get<int64_t>(j);
        if (i64 < std::numeric_limits<short>::min() || i64 > std::numeric_limits<short>::max()) {
            throw std::runtime_error("JSON value out of range for short: " + std::to_string(i64));
        }
        value = static_cast<short>(i64);
    } else if (j.is_number()) {
        double d = std::get<double>(j);
        double intpart;
        if (std::modf(d, &intpart) != 0.0) {
            throw std::runtime_error("JSON value has fractional part, cannot convert to short: " + std::to_string(d));
        }
        if (intpart < std::numeric_limits<short>::min() || intpart > std::numeric_limits<short>::max()) {
            throw std::runtime_error("JSON value out of range for short: " + std::to_string(d));
        }
        value = static_cast<short>(intpart);
    } else {
        throw std::runtime_error("JSON type mismatch: expected number");
    }
}

inline void from_json(const JsonValue& j, unsigned short& value) {
    if (j.is_int()) {
        int64_t i64 = std::get<int64_t>(j);
        if (i64 < 0 || i64 > std::numeric_limits<unsigned short>::max()) {
            throw std::runtime_error("JSON value out of range for unsigned short: " + std::to_string(i64));
        }
        value = static_cast<unsigned short>(i64);
    } else if (j.is_number()) {
        double d = std::get<double>(j);
        double intpart;
        if (std::modf(d, &intpart) != 0.0) {
            throw std::runtime_error("JSON value has fractional part, cannot convert to unsigned short: " + std::to_string(d));
        }
        if (intpart < 0 || intpart > std::numeric_limits<unsigned short>::max()) {
            throw std::runtime_error("JSON value out of range for unsigned short: " + std::to_string(d));
        }
        value = static_cast<unsigned short>(intpart);
    } else {
        throw std::runtime_error("JSON type mismatch: expected number");
    }
}

// Containers - to_json (output parameter version)
template <typename T, size_t N>
void to_json(JsonValue& j, const std::array<T, N>& arr) {
    JsonArray json_arr;
    json_arr.reserve(N);

    for (const auto& elem : arr) {
        json_arr.push_back(to_json(elem));
    }
    j = std::move(json_arr);
}
template <typename T>
void to_json(JsonValue& j, const std::vector<T>& vec) {
    JsonArray arr;
    arr.reserve(vec.size());
    for (const auto& elem : vec) {
        arr.push_back(to_json(elem));
    }
    j = std::move(arr);
}

template <typename T>
void to_json(JsonValue& j, const std::set<T>& s) {
    JsonArray arr;
    for (const auto& elem : s) {
        arr.push_back(to_json(elem));
    }
    j = std::move(arr);
}

template <typename T>
void to_json(JsonValue& j, const std::unordered_set<T>& s) {
    JsonArray arr;
    for (const auto& elem : s) {
        arr.push_back(to_json(elem));
    }
    j = std::move(arr);
}

template <typename T>
void to_json(JsonValue& j, const std::deque<T>& d) {
    JsonArray arr;
    arr.reserve(d.size());
    for (const auto& elem : d) {
        arr.push_back(to_json(elem));
    }
    j = std::move(arr);
}

template <typename T>
void to_json(JsonValue& j, const std::list<T>& lst) {
    JsonArray arr;
    for (const auto& elem : lst) {
        arr.push_back(to_json(elem));
    }
    j = std::move(arr);
}

template <typename K, typename V>
void to_json(JsonValue& j, const std::map<K, V>& m) {
    JsonObject obj;
    for (const auto& [key, val] : m) {
        std::string key_str;
        if constexpr (std::is_convertible_v<K, std::string>) {
            key_str = key;
        } else if constexpr (std::is_arithmetic_v<K>) {
            key_str = std::to_string(key);
        } else {
            std::ostringstream oss;
            oss << key;
            key_str = oss.str();
        }
        obj[std::move(key_str)] = to_json(val);
    }
    j = std::move(obj);
}

template <typename K, typename V>
void to_json(JsonValue& j, const std::unordered_map<K, V>& m) {
    JsonObject obj;
    for (const auto& [key, val] : m) {
        std::string key_str;
        if constexpr (std::is_convertible_v<K, std::string>) {
            key_str = key;
        } else if constexpr (std::is_arithmetic_v<K>) {
            key_str = std::to_string(key);
        } else {
            std::ostringstream oss;
            oss << key;
            key_str = oss.str();
        }
        obj[std::move(key_str)] = to_json(val);
    }
    j = std::move(obj);
}

template <typename T>
void to_json(JsonValue& j, const std::optional<T>& opt) {
    if (opt.has_value()) {
        to_json(j, *opt);
    } else {
        j = nullptr;
    }
}

template <typename T1, typename T2>
void to_json(JsonValue& j, const std::pair<T1, T2>& p) {
    JsonArray arr;
    arr.push_back(to_json(p.first));
    arr.push_back(to_json(p.second));
    j = std::move(arr);
}

// Helper for tuple serialization
template <typename Tuple, std::size_t... I>
JsonArray tuple_to_json_impl(const Tuple& tup, std::index_sequence<I...>) {
    JsonArray arr;
    (..., arr.push_back(to_json(std::get<I>(tup))));
    return arr;
}

template <typename... Ts>
void to_json(JsonValue& j, const std::tuple<Ts...>& tup) {
    j = tuple_to_json_impl(tup, std::index_sequence_for<Ts...>{});
}

// Containers - to_json (return-value versions for backward compatibility)
template <typename T, size_t N>
JsonValue to_json(const std::array<T, N>& arr) {
    JsonArray json_arr;
    json_arr.reserve(N); // std::array size is fixed, reserve capacity in the JsonArray

    for (const auto& elem : arr) {
        // We rely on the existing 'to_json(const T&)' overload for T
        json_arr.push_back(to_json(elem));
    }
    return json_arr; // Returns JsonValue implicitly holding the JsonArray
}

template <typename T>
JsonValue to_json(const std::vector<T>& vec) {
    JsonArray arr;
    arr.reserve(vec.size());
    for (const auto& elem : vec) {
        arr.push_back(to_json(elem));
    }
    return arr;
}

template <typename T>
JsonValue to_json(const std::set<T>& s) {
    JsonArray arr;
    for (const auto& elem : s) {
        arr.push_back(to_json(elem));
    }
    return arr;
}

template <typename T>
JsonValue to_json(const std::unordered_set<T>& s) {
    JsonArray arr;
    for (const auto& elem : s) {
        arr.push_back(to_json(elem));
    }
    return arr;
}

template <typename T>
JsonValue to_json(const std::deque<T>& d) {
    JsonArray arr;
    for (const auto& elem : d) {
        arr.push_back(to_json(elem));
    }
    return arr;
}

template <typename T>
JsonValue to_json(const std::list<T>& lst) {
    JsonArray arr;
    for (const auto& elem : lst) {
        arr.push_back(to_json(elem));
    }
    return arr;
}

template <typename K, typename V>
JsonValue to_json(const std::map<K, V>& m) {
    JsonObject obj;
    for (const auto& [key, val] : m) {
        std::string key_str;
        if constexpr (std::is_convertible_v<K, std::string>) {
            key_str = key;
        } else if constexpr (std::is_arithmetic_v<K>) {
            key_str = std::to_string(key);
        } else {
            std::ostringstream oss;
            oss << key;
            key_str = oss.str();
        }
        obj[std::move(key_str)] = to_json(val);
    }
    return obj;
}

template <typename K, typename V>
JsonValue to_json(const std::unordered_map<K, V>& m) {
    JsonObject obj;
    for (const auto& [key, val] : m) {
        std::string key_str;
        if constexpr (std::is_convertible_v<K, std::string>) {
            key_str = key;
        } else if constexpr (std::is_arithmetic_v<K>) {
            key_str = std::to_string(key);
        } else {
            std::ostringstream oss;
            oss << key;
            key_str = oss.str();
        }
        obj[std::move(key_str)] = to_json(val);
    }
    return obj;
}

template <typename T>
JsonValue to_json(const std::optional<T>& opt) {
    if (opt.has_value()) {
        return to_json(*opt);
    }
    return nullptr;
}

template <typename T1, typename T2>
JsonValue to_json(const std::pair<T1, T2>& p) {
    JsonArray arr;
    arr.push_back(to_json(p.first));
    arr.push_back(to_json(p.second));
    return arr;
}

// Containers - from_json (output parameter version)
template <typename T, size_t N>
void from_json(const JsonValue& j, std::array<T, N>& arr) {
    if (!j.is_array()) {
        throw std::runtime_error("JSON type mismatch: expected array for std::array");
    }
    const auto& json_arr = std::get<JsonArray>(j);

    if (json_arr.size() != N) {
        // You may choose to throw an error here if the sizes MUST match exactly
        throw std::runtime_error("JSON array size mismatch: expected size " +
            std::to_string(N) + ", got " + std::to_string(json_arr.size()));
    }

    // Use std::copy for efficient iteration and deserialization
    for (size_t i = 0; i < N; ++i) {
        // We rely on the existing 'from_json(JsonValue, T&)' overload for T
        from_json(json_arr[i], arr[i]);
    }
}

template <typename T>
void from_json(const JsonValue& j, std::vector<T>& vec) {
    if (!j.is_array()) throw std::runtime_error("JSON type mismatch: expected array");
    const auto& arr = std::get<JsonArray>(j);
    vec.clear();
    vec.reserve(arr.size());
    for (const auto& elem : arr) {
        T value;
        from_json(elem, value);
        vec.push_back(std::move(value));
    }
}

template <typename T>
void from_json(const JsonValue& j, std::set<T>& s) {
    if (!j.is_array()) throw std::runtime_error("JSON type mismatch: expected array for std::set");
    const auto& arr = std::get<JsonArray>(j);
    s.clear();
    for (const auto& elem : arr) {
        T value;
        from_json(elem, value);
        s.insert(std::move(value));
    }
}

template <typename T>
void from_json(const JsonValue& j, std::unordered_set<T>& s) {
    if (!j.is_array()) throw std::runtime_error("JSON type mismatch: expected array for std::unordered_set");
    const auto& arr = std::get<JsonArray>(j);
    s.clear();
    s.reserve(arr.size());
    for (const auto& elem : arr) {
        T value;
        from_json(elem, value);
        s.insert(std::move(value));
    }
}

template <typename T>
void from_json(const JsonValue& j, std::deque<T>& d) {
    if (!j.is_array()) throw std::runtime_error("JSON type mismatch: expected array for std::deque");
    const auto& arr = std::get<JsonArray>(j);
    d.clear();
    for (const auto& elem : arr) {
        T value;
        from_json(elem, value);
        d.push_back(std::move(value));
    }
}

template <typename T>
void from_json(const JsonValue& j, std::list<T>& lst) {
    if (!j.is_array()) throw std::runtime_error("JSON type mismatch: expected array for std::list");
    const auto& arr = std::get<JsonArray>(j);
    lst.clear();
    for (const auto& elem : arr) {
        T value;
        from_json(elem, value);
        lst.push_back(std::move(value));
    }
}

template <typename K, typename V>
void from_json(const JsonValue& j, std::map<K, V>& m) {
    if (!j.is_object()) throw std::runtime_error("JSON type mismatch: expected object");
    const auto& obj = std::get<JsonObject>(j);
    m.clear();
    for (const auto& [key, val] : obj) {
        V value;
        from_json(val, value);
        if constexpr (std::is_same_v<K, std::string>) {
            m[key] = std::move(value);
        } else if constexpr (std::is_arithmetic_v<K>) {
            K converted_key;
            std::istringstream iss(key);
            if (!(iss >> converted_key)) {
                throw std::runtime_error("Failed to convert map key: " + key);
            }
            m[converted_key] = std::move(value);
        } else {
            throw std::runtime_error("Unsupported map key type for deserialization");
        }
    }
}

template <typename K, typename V>
void from_json(const JsonValue& j, std::unordered_map<K, V>& m) {
    if (!j.is_object()) throw std::runtime_error("JSON type mismatch: expected object for std::unordered_map");
    const auto& obj = std::get<JsonObject>(j);
    m.clear();
    m.reserve(obj.size());
    for (const auto& [key, val] : obj) {
        V value;
        from_json(val, value);
        if constexpr (std::is_same_v<K, std::string>) {
            m[key] = std::move(value);
        } else if constexpr (std::is_arithmetic_v<K>) {
            K converted_key;
            std::istringstream iss(key);
            if (!(iss >> converted_key)) {
                throw std::runtime_error("Failed to convert map key: " + key);
            }
            m[converted_key] = std::move(value);
        } else {
            throw std::runtime_error("Unsupported map key type for deserialization");
        }
    }
}

template <typename T>
void from_json(const JsonValue& j, std::optional<T>& opt) {
    if (j.is_null()) {
        opt = std::nullopt;
    } else {
        T value;
        from_json(j, value);
        opt = std::move(value);
    }
}

template <typename T1, typename T2>
void from_json(const JsonValue& j, std::pair<T1, T2>& p) {
    if (!j.is_array()) throw std::runtime_error("JSON type mismatch: expected array for pair");
    const auto& arr = std::get<JsonArray>(j);
    if (arr.size() != 2) {
        throw std::runtime_error("JSON array size mismatch: expected 2 elements for pair, got " + std::to_string(arr.size()));
    }
    from_json(arr[0], p.first);
    from_json(arr[1], p.second);
}

template <typename... Ts>
void from_json(const JsonValue& j, std::tuple<Ts...>& tup) {
    if (!j.is_array()) throw std::runtime_error("JSON type mismatch: expected array for tuple");
    const auto& arr = std::get<JsonArray>(j);
    constexpr size_t expected_size = sizeof...(Ts);
    if (arr.size() != expected_size) {
        throw std::runtime_error("JSON array size mismatch: expected " + std::to_string(expected_size) + 
                               " elements for tuple, got " + std::to_string(arr.size()));
    }
    from_json_tuple_impl(arr, tup, std::index_sequence_for<Ts...>{});
}

// Helper for tuple deserialization
template <typename Tuple, std::size_t... I>
void from_json_tuple_impl(const JsonArray& arr, Tuple& tup, std::index_sequence<I...>) {
    (..., from_json(arr[I], std::get<I>(tup)));
}

// Template return-value from_json for backward compatibility
template <typename T>
T from_json(const JsonValue& j) {
    if constexpr (std::is_same_v<T, JsonValue>) {
        return j;
    }
    else {
        T result;
        from_json(j, result);
        return result;
    }
}

// Generic template to_json that forwards to output-parameter version
// This allows user-defined types to work with return-value API
// Requires that void to_json(JsonValue&, const T&) exists
template <typename T>
auto to_json(const T& value) -> decltype(to_json(std::declval<JsonValue&>(), value), JsonValue{}) {
    JsonValue j;
    to_json(j, value);
    return j;
}

/** @} */ // end of conversion group

// ====================================================================
// File I/O Functions
// ====================================================================

/**
 * @defgroup fileio File I/O Functions
 * @brief Functions for reading and writing JSON files
 * @{
 */

/**
 * @brief Load and parse a JSON file
 * 
 * @param filename Path to JSON file
 * @return JsonValue Parsed JSON content
 * @throws std::runtime_error If file cannot be opened or read, or JSON is invalid
 */
[[nodiscard]] inline JsonValue load_json_from_file(const std::string& filename) {
    std::ifstream ifs(filename, std::ios::binary);
    if (!ifs.is_open()) 
        throw std::runtime_error("Failed to open file for reading: " + filename);
    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    if (ifs.bad()) 
        throw std::runtime_error("Error reading file: " + filename);
    return parse_json(content);
}

/**
 * @brief Save a JsonValue to a file
 * 
 * @tparam Policy Formatting policy (default: StandardJsonPolicy)
 * @param filename Path to output file
 * @param val JsonValue to save
 * @param pretty Enable pretty printing (default: from policy)
 * @throws std::runtime_error If file cannot be opened or written
 */
template <typename Policy = StandardJsonPolicy>
inline void save_json_to_file(const std::string& filename, const JsonValue& val, bool pretty = Policy::pretty_print) {
    std::ofstream ofs(filename);
    if (!ofs.is_open()) 
        throw std::runtime_error("Failed to open file for writing: " + filename);
    to_json_stream<JsonValue, Policy>(ofs, val, pretty);
    if (!ofs.good()) 
        throw std::runtime_error("Error writing to file: " + filename);
}

/** @} */ // end of fileio group

// ====================================================================
// Convenience Functions
// ====================================================================

/**
 * @brief Parse JSON string and convert to type T
 * 
 * @tparam T Target type
 * @param json_str JSON string
 * @return T Deserialized value
 * @throws std::runtime_error On parse or conversion errors
 */
template <typename T>
[[nodiscard]] T from_json_string(std::string_view json_str) {
    JsonValue val = parse_json(json_str);
    return from_json<T>(val);
}

// ====================================================================
// Param-Specific Helpers (convenience functions for app parameters)
// ====================================================================

/**
 * @brief Save application parameters to a JSON file with pretty printing by default
 * @tparam T The parameter struct type (must have to_json defined)
 * @tparam Policy The JSON formatting policy (default: PrettyJsonPolicy for readable configs)
 * @param filename Path to the output file
 * @param params The parameter struct to save
 */
template<typename T, typename Policy = PrettyJsonPolicy>
inline void save_params(const std::string& filename, const T& params) {
    save_json_to_file<Policy>(filename, to_json(params), true);
}

/**
 * @brief Load application parameters from a JSON file
 * @tparam T The parameter struct type (must have from_json defined)
 * @param filename Path to the input file
 * @return The deserialized parameter struct
 * @throws std::runtime_error if file cannot be opened or JSON is invalid
 */
template<typename T>
[[nodiscard]] inline T load_params(const std::string& filename) {
    return from_json<T>(load_json_from_file(filename));
}

/**
 * @brief Load application parameters from a JSON file
 * @tparam T The parameter struct type (must have from_json defined)
 * @param filename Path to the input file
 * @param params The parameter struct to fill
 * @throws std::runtime_error if file cannot be opened or JSON is invalid
 */
template<typename T>
inline void load_params(const std::string& filename, T& params) {
    params = from_json<T>(load_json_from_file(filename));
}

/**
 * @brief Save application parameters with a backup of the old file
 * @tparam T The parameter struct type (must have to_json defined)
 * @tparam Policy The JSON formatting policy (default: PrettyJsonPolicy)
 * @param filename Path to the output file
 * @param params The parameter struct to save
 * @param backup_suffix Suffix for backup file (default: ".bak")
 */
template<typename T, typename Policy = PrettyJsonPolicy>
inline void save_params_with_backup(const std::string& filename, const T& params, 
                                   const std::string& backup_suffix = ".bak") {
    // Check if file exists and create backup if so
    std::ifstream test(filename);
    if (test.good()) {
        test.close();
        std::string backup_name = filename + backup_suffix;
        
        // Create backup
        std::ifstream src(filename, std::ios::binary);
        if (!src.is_open()) {
            throw std::runtime_error("Failed to open source file for backup: " + filename);
        }
        
        std::ofstream dst(backup_name, std::ios::binary);
        if (!dst.is_open()) {
            throw std::runtime_error("Failed to create backup file: " + backup_name);
        }
        
        dst << src.rdbuf();
        
        if (!dst.good()) {
            throw std::runtime_error("Error writing backup file: " + backup_name);
        }
        
        dst.close();
        src.close();
    }
    
    // Save new params
    save_params<T, Policy>(filename, params);
}

}  // namespace cpp_utilities