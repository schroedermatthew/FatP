// JsonLite.h
#pragma once

#include <algorithm>
#include <any>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <variant>
#include <string>
#include <string_view>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <stdexcept>

#include "EqualityComparisons.h"  // Assuming this exists; if not, remove or define.

namespace cpp_utilities {

// ====================================================================
// JSON Value Type (for Parsing)
// ====================================================================

using JsonObject = std::map<std::string, struct JsonValue>;
using JsonArray = std::vector<struct JsonValue>;

struct JsonValue : std::variant<std::nullptr_t, bool, double, std::string, JsonArray, JsonObject> {
    using variant::variant;
    
    // Helper methods for type checking
    [[nodiscard]] bool is_null() const noexcept { return std::holds_alternative<std::nullptr_t>(*this); }
    [[nodiscard]] bool is_bool() const noexcept { return std::holds_alternative<bool>(*this); }
    [[nodiscard]] bool is_number() const noexcept { return std::holds_alternative<double>(*this); }
    [[nodiscard]] bool is_string() const noexcept { return std::holds_alternative<std::string>(*this); }
    [[nodiscard]] bool is_array() const noexcept { return std::holds_alternative<JsonArray>(*this); }
    [[nodiscard]] bool is_object() const noexcept { return std::holds_alternative<JsonObject>(*this); }
};

// ====================================================================
// JSON Policies
// ====================================================================

struct StandardJsonPolicy {
    static constexpr bool pretty_print = false;
    static constexpr int numeric_precision = 6;
    static constexpr int indent_step = 4;
    static constexpr bool allow_nan_inf = false;
    static constexpr bool escape_unicode = true;
};

struct PrettyJsonPolicy : StandardJsonPolicy {
    static constexpr bool pretty_print = true;
};

struct CompatJsonPolicy : StandardJsonPolicy {
    static constexpr bool allow_nan_inf = true;
    static constexpr bool escape_unicode = false;
};

// ====================================================================
// Detail Helpers
// ====================================================================

namespace detail {
    // Depth limit to prevent stack overflow
    static constexpr size_t MAX_PARSE_DEPTH = 512;
    static constexpr size_t MAX_DUMP_DEPTH = 512;
    
    template <typename Os>
    void output_indent(Os& os, int indent) {
        for (int i = 0; i < indent; ++i) {
            os << ' ';
        }
    }

    template <typename Os, typename Policy>
    void escape_string(Os& os, std::string_view s) {
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
                    } else if (Policy::escape_unicode && static_cast<unsigned char>(c) > 0x7F) {
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
        } else if constexpr (std::is_arithmetic_v<T>) {
            if constexpr (std::is_floating_point_v<T>) {
                if (std::isnan(obj)) {
                    if constexpr (Policy::allow_nan_inf) {
                        os << "NaN";
                    } else {
                        os << "null";
                    }
                } else if (std::isinf(obj)) {
                    if constexpr (Policy::allow_nan_inf) {
                        os << (obj > 0 ? "Infinity" : "-Infinity");
                    } else {
                        os << "null";
                    }
                } else {
                    double intpart;
                    if (std::modf(obj, &intpart) == 0.0) {
                        // Print as integer if no fractional part
                        os << static_cast<long long>(intpart);
                    } else {
                        os << std::fixed << std::setprecision(Policy::numeric_precision) << obj;
                    }
                }
            } else {
                os << obj;
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
    inline void check_dump_depth(int indent, int indent_step) {
        size_t depth = static_cast<size_t>(indent / (indent_step > 0 ? indent_step : 1));
        if (depth > MAX_DUMP_DEPTH) {
            throw std::runtime_error("JSON dump error: maximum nesting depth (" + 
                                   std::to_string(MAX_DUMP_DEPTH) + ") exceeded");
        }
    }

}  // namespace detail

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

// Scalar (non-iterable or string)
template <typename T, typename Policy>
struct JsonDispatcher<T, Policy, std::enable_if_t<!IsIterable<T>::value || std::is_same_v<std::decay_t<T>, std::string>>> {
    template <typename Os>
    static void dump(Os& os, const T& obj, bool pretty = Policy::pretty_print, int indent = 0) {
        if constexpr (std::is_same_v<T, std::nullptr_t> || std::is_null_pointer_v<T>) {
            os << "null";
        } else if constexpr (!std::is_arithmetic_v<T> && !std::is_same_v<std::decay_t<T>, std::string> && 
                             !std::is_convertible_v<T, std::string_view> && detail::has_to_json_v<T>) {
            // User-defined type with custom to_json - convert to JsonValue first
            JsonValue j;
            to_json(j, obj);
            JsonDispatcher<JsonValue, Policy>::dump(os, j, pretty, indent);
        } else {
            detail::dump_scalar<Os, T, Policy>(os, obj);
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

// Pair (as array, corrected from original which dumped as object)
template <typename T1, typename T2, typename Policy>
struct JsonDispatcher<std::pair<T1, T2>, Policy> {
    template <typename Os>
    static void dump(Os& os, const std::pair<T1, T2>& p, bool pretty = Policy::pretty_print, int indent = 0) {
        os << '[';
        if (pretty) os << '\n';
        if (pretty) detail::output_indent(os, indent + Policy::indent_step);
        JsonDispatcher<T1, Policy>::dump(os, p.first, pretty, indent + Policy::indent_step);
        os << (pretty ? ", " : ",");
        JsonDispatcher<T2, Policy>::dump(os, p.second, pretty, indent + Policy::indent_step);
        if (pretty) os << '\n';
        if (pretty) detail::output_indent(os, indent);
        os << ']';
    }
};

// Tuple (as array)
template <typename... Ts, typename Policy>
struct JsonDispatcher<std::tuple<Ts...>, Policy> {
    template <typename Os>
    static void dump(Os& os, const std::tuple<Ts...>& tup, bool pretty = Policy::pretty_print, int indent = 0) {
        detail::dump_tuple_impl<Os, Policy>(os, tup, std::make_index_sequence<sizeof...(Ts)>(), pretty, indent);
    }
};

// Iterable non-associative (as array)
template <typename T, typename Policy>
struct JsonDispatcher<T, Policy, std::enable_if_t<IsIterable<T>::value && !std::is_same_v<std::decay_t<T>, std::string> && !HasMappedType<T>::value>> {
    template <typename Os>
    static void dump(Os& os, const T& cont, bool pretty = Policy::pretty_print, int indent = 0) {
        detail::check_dump_depth(indent, Policy::indent_step);
        os << '[';
        if (pretty && !cont.empty()) os << '\n';
        bool first = true;
        for (const auto& elem : cont) {
            if (!first) os << (pretty ? ",\n" : ",");
            first = false;
            if (pretty) detail::output_indent(os, indent + Policy::indent_step);
            JsonDispatcher<ContainerValueT<T>, Policy>::dump(os, elem, pretty, indent + Policy::indent_step);
        }
        if (pretty && !cont.empty()) os << '\n';
        if (pretty) detail::output_indent(os, indent);
        os << ']';
    }
};

// Associative (as object)
template <typename T, typename Policy>
struct JsonDispatcher<T, Policy, std::enable_if_t<IsIterable<T>::value && !std::is_same_v<std::decay_t<T>, std::string> && HasMappedType<T>::value>> {
    template <typename Os>
    static void dump(Os& os, const T& cont, bool pretty = Policy::pretty_print, int indent = 0) {
        detail::check_dump_depth(indent, Policy::indent_step);
        os << '{';
        if (pretty && !cont.empty()) os << '\n';
        bool first = true;
        for (const auto& elem : cont) {
            if (!first) os << (pretty ? ",\n" : ",");
            first = false;
            if (pretty) detail::output_indent(os, indent + Policy::indent_step);
            detail::escape_string<Os, Policy>(os, elem.first);
            os << (pretty ? " : " : ":");
            JsonDispatcher<typename T::mapped_type, Policy>::dump(os, elem.second, pretty, indent + Policy::indent_step);
        }
        if (pretty && !cont.empty()) os << '\n';
        if (pretty) detail::output_indent(os, indent);
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
namespace detail {
    template <typename Os, typename Policy, typename Tuple, std::size_t... I>
    void dump_tuple_impl(Os& os, const Tuple& tup, std::index_sequence<I...>, bool pretty, int indent) {
        os << '[';
        if (pretty) os << '\n';
        bool first = true;
        (..., ([&]() {
            if (!first) {
                os << (pretty ? ",\n" : ",");
            }
            first = false;
            if (pretty) output_indent(os, indent + Policy::indent_step);
            JsonDispatcher<std::tuple_element_t<I, Tuple>, Policy>::dump(os, std::get<I>(tup), pretty, indent + Policy::indent_step);
        })());
        if (pretty && sizeof...(I) > 0) os << '\n';
        if (pretty) output_indent(os, indent);
        os << ']';
    }
}  // namespace detail

// ====================================================================
// High-Level API Functions
// ====================================================================

template <typename T, typename Policy = StandardJsonPolicy>
std::string to_json_string(const T& obj, bool pretty = Policy::pretty_print) {
    std::ostringstream oss;
    JsonDispatcher<T, Policy>::dump(oss, obj, pretty);
    return oss.str();
}

template <typename T, typename Policy = StandardJsonPolicy, typename Os>
void to_json_stream(Os& os, const T& obj, bool pretty = Policy::pretty_print) {
    JsonDispatcher<T, Policy>::dump(os, obj, pretty);
}

// Specialization for already JsonValue
inline JsonValue to_json(const JsonValue& value) {
    return value;
}

// ====================================================================
// C++17-Compatible Macros (NO __VA_OPT__)
// ====================================================================

// MSVC workaround: needs extra expansion layer
#define CPP_JSON_EXPAND(x) x

#define CPP_JSON_ARG_COUNT_IMPL(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,N,...) N
#define CPP_JSON_ARG_COUNT(...) CPP_JSON_EXPAND(CPP_JSON_ARG_COUNT_IMPL(__VA_ARGS__,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1))

#define CPP_JSON_CAT(a, b) CPP_JSON_CAT_IMPL(a, b)
#define CPP_JSON_CAT_IMPL(a, b) a##b

#define CPP_JSON_TO_FIELD(field) \
    obj[#field] = to_json(value.field);

#define CPP_JSON_FROM_FIELD(field) \
    if (auto it = obj.find(#field); it != obj.end()) { \
        try { \
            from_json(it->second, value.field); \
        } catch (const std::exception& e) { \
            throw std::runtime_error(std::string("Error deserializing field '") + #field + "': " + e.what()); \
        } \
    } else if constexpr (!detail::is_optional_v<decltype((value.field))>) { \
        throw std::runtime_error("Required field missing: '" #field "'"); \
    }

#define CPP_JSON_FROM_FIELD_OPT(field) \
    if (auto it = obj.find(#field); it != obj.end()) { \
        from_json(it->second, value.field); \
    }

#define CPP_JSON_APPLY_1(macro, x1) macro(x1)
#define CPP_JSON_APPLY_2(macro, x1, x2) macro(x1) macro(x2)
#define CPP_JSON_APPLY_3(macro, x1, x2, x3) macro(x1) macro(x2) macro(x3)
#define CPP_JSON_APPLY_4(macro, x1, x2, x3, x4) macro(x1) macro(x2) macro(x3) macro(x4)
#define CPP_JSON_APPLY_5(macro, x1, x2, x3, x4, x5) macro(x1) macro(x2) macro(x3) macro(x4) macro(x5)
#define CPP_JSON_APPLY_6(macro, x1, x2, x3, x4, x5, x6) macro(x1) macro(x2) macro(x3) macro(x4) macro(x5) macro(x6)
#define CPP_JSON_APPLY_7(macro, x1, x2, x3, x4, x5, x6, x7) macro(x1) macro(x2) macro(x3) macro(x4) macro(x5) macro(x6) macro(x7)
#define CPP_JSON_APPLY_8(macro, x1, x2, x3, x4, x5, x6, x7, x8) macro(x1) macro(x2) macro(x3) macro(x4) macro(x5) macro(x6) macro(x7) macro(x8)
#define CPP_JSON_APPLY_9(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9) macro(x1) macro(x2) macro(x3) macro(x4) macro(x5) macro(x6) macro(x7) macro(x8) macro(x9)
#define CPP_JSON_APPLY_10(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10) macro(x1) macro(x2) macro(x3) macro(x4) macro(x5) macro(x6) macro(x7) macro(x8) macro(x9) macro(x10)
#define CPP_JSON_APPLY_11(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11) macro(x1) macro(x2) macro(x3) macro(x4) macro(x5) macro(x6) macro(x7) macro(x8) macro(x9) macro(x10) macro(x11)
#define CPP_JSON_APPLY_12(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12) macro(x1) macro(x2) macro(x3) macro(x4) macro(x5) macro(x6) macro(x7) macro(x8) macro(x9) macro(x10) macro(x11) macro(x12)
#define CPP_JSON_APPLY_13(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13) macro(x1) macro(x2) macro(x3) macro(x4) macro(x5) macro(x6) macro(x7) macro(x8) macro(x9) macro(x10) macro(x11) macro(x12) macro(x13)
#define CPP_JSON_APPLY_14(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14) macro(x1) macro(x2) macro(x3) macro(x4) macro(x5) macro(x6) macro(x7) macro(x8) macro(x9) macro(x10) macro(x11) macro(x12) macro(x13) macro(x14)
#define CPP_JSON_APPLY_15(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15) macro(x1) macro(x2) macro(x3) macro(x4) macro(x5) macro(x6) macro(x7) macro(x8) macro(x9) macro(x10) macro(x11) macro(x12) macro(x13) macro(x14) macro(x15)
#define CPP_JSON_APPLY_16(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16) macro(x1) macro(x2) macro(x3) macro(x4) macro(x5) macro(x6) macro(x7) macro(x8) macro(x9) macro(x10) macro(x11) macro(x12) macro(x13) macro(x14) macro(x15) macro(x16)
#define CPP_JSON_APPLY_17(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17) macro(x1) macro(x2) macro(x3) macro(x4) macro(x5) macro(x6) macro(x7) macro(x8) macro(x9) macro(x10) macro(x11) macro(x12) macro(x13) macro(x14) macro(x15) macro(x16) macro(x17)
#define CPP_JSON_APPLY_18(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18) macro(x1) macro(x2) macro(x3) macro(x4) macro(x5) macro(x6) macro(x7) macro(x8) macro(x9) macro(x10) macro(x11) macro(x12) macro(x13) macro(x14) macro(x15) macro(x16) macro(x17) macro(x18)
#define CPP_JSON_APPLY_19(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19) macro(x1) macro(x2) macro(x3) macro(x4) macro(x5) macro(x6) macro(x7) macro(x8) macro(x9) macro(x10) macro(x11) macro(x12) macro(x13) macro(x14) macro(x15) macro(x16) macro(x17) macro(x18) macro(x19)
#define CPP_JSON_APPLY_20(macro, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20) macro(x1) macro(x2) macro(x3) macro(x4) macro(x5) macro(x6) macro(x7) macro(x8) macro(x9) macro(x10) macro(x11) macro(x12) macro(x13) macro(x14) macro(x15) macro(x16) macro(x17) macro(x18) macro(x19) macro(x20)

#define CPP_JSON_FOR_EACH(macro, ...) \
    CPP_JSON_EXPAND(CPP_JSON_CAT(CPP_JSON_APPLY_, CPP_JSON_ARG_COUNT(__VA_ARGS__))(macro, __VA_ARGS__))

#define CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Type, ...)                                           \
inline void to_json(JsonValue& j, const Type& value) {                                          \
    JsonObject obj;                                                                             \
    CPP_JSON_FOR_EACH(CPP_JSON_TO_FIELD, __VA_ARGS__)                                           \
    j = std::move(obj);                                                                         \
}                                                                                               \
inline void from_json(const JsonValue& j, Type& value) {                                        \
    const auto& obj = std::get<JsonObject>(j);                                                  \
    CPP_JSON_FOR_EACH(CPP_JSON_FROM_FIELD, __VA_ARGS__)                                         \
}

#define CPP_JSON_DEFINE_TYPE_OPTIONAL(Type, ...)                                                \
inline void to_json(JsonValue& j, const Type& value) {                                          \
    JsonObject obj;                                                                             \
    CPP_JSON_FOR_EACH(CPP_JSON_TO_FIELD, __VA_ARGS__)                                           \
    j = std::move(obj);                                                                         \
}                                                                                               \
inline void from_json(const JsonValue& j, Type& value) {                                        \
    const auto& obj = std::get<JsonObject>(j);                                                  \
    CPP_JSON_FOR_EACH(CPP_JSON_FROM_FIELD_OPT, __VA_ARGS__)                                     \
}

#define CPP_JSON_DEFINE_TYPE_INTRUSIVE(Type, ...)                                               \
friend void to_json(JsonValue& j, const Type& value) {                                          \
    JsonObject obj;                                                                             \
    CPP_JSON_FOR_EACH(CPP_JSON_TO_FIELD, __VA_ARGS__)                                           \
    j = std::move(obj);                                                                         \
}                                                                                               \
friend void from_json(const JsonValue& j, Type& value) {                                        \
    const auto& obj = std::get<JsonObject>(j);                                                  \
    CPP_JSON_FOR_EACH(CPP_JSON_FROM_FIELD, __VA_ARGS__)                                         \
}

// ====================================================================
// JSON Parser
// ====================================================================

namespace detail {
    inline void skip_whitespace(std::string_view s, size_t& pos) {
        while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos]))) ++pos;
    }

    inline std::string parse_string(std::string_view s, size_t& pos) {
        if (s[pos] != '"') throw std::runtime_error("JSON parse error: expected string");
        ++pos;
        std::string res;
        while (pos < s.size() && s[pos] != '"') {
            if (s[pos] == '\\') {
                ++pos;
                if (pos >= s.size()) throw std::runtime_error("JSON parse error: invalid escape sequence");
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
                        if (pos + 4 >= s.size()) 
                            throw std::runtime_error("JSON parse error: invalid unicode escape");
                        std::string hex = std::string(s.substr(pos + 1, 4));
                        uint32_t codepoint = static_cast<uint32_t>(std::stoul(hex, nullptr, 16));
                        pos += 4;
                        // Append UTF-8 encoded codepoint (basic, no surrogate handling)
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
                        }
                        break;
                    }
                    default: throw std::runtime_error("JSON parse error: invalid escape character");
                }
            } else {
                res += s[pos];
            }
            ++pos;
        }
        if (pos >= s.size() || s[pos] != '"') 
            throw std::runtime_error("JSON parse error: unterminated string");
        ++pos;
        return res;
    }

    inline double parse_number(std::string_view s, size_t& pos) {
        size_t start = pos;
        if (s[pos] == '-') ++pos;
        
        if (s.substr(pos, 3) == "NaN") {
            pos += 3;
            return std::numeric_limits<double>::quiet_NaN();
        }
        if (s.substr(pos, 8) == "Infinity") {
            pos += 8;
            return start < pos && s[start] == '-' ? -std::numeric_limits<double>::infinity() 
                                                   : std::numeric_limits<double>::infinity();
        }
        
        while (pos < s.size() && (std::isdigit(s[pos]) || s[pos] == '.' || 
               s[pos] == 'e' || s[pos] == 'E' || s[pos] == '+' || s[pos] == '-')) {
            ++pos;
        }
        
        std::string num_str = std::string(s.substr(start, pos - start));
        try {
            return std::stod(num_str);
        } catch (...) {
            throw std::runtime_error("JSON parse error: invalid number '" + num_str + "'");
        }
    }

    JsonValue parse_value(std::string_view s, size_t& pos, size_t depth = 0);

    inline JsonArray parse_array(std::string_view s, size_t& pos, size_t depth) {
        if (depth > MAX_PARSE_DEPTH) {
            throw std::runtime_error("JSON parse error: maximum nesting depth (" + 
                                   std::to_string(MAX_PARSE_DEPTH) + ") exceeded");
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

    inline JsonObject parse_object(std::string_view s, size_t& pos, size_t depth) {
        if (depth > MAX_PARSE_DEPTH) {
            throw std::runtime_error("JSON parse error: maximum nesting depth (" + 
                                   std::to_string(MAX_PARSE_DEPTH) + ") exceeded");
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
            std::string key = parse_string(s, pos);
            skip_whitespace(s, pos);
            if (pos >= s.size() || s[pos] != ':') 
                throw std::runtime_error("JSON parse error: expected ':' after object key '" + key + "' at position " + std::to_string(pos));
            ++pos;
            skip_whitespace(s, pos);
            obj[key] = parse_value(s, pos, depth + 1);
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
        if (s.substr(pos, 4) == "true") { pos += 4; return true; }
        if (s.substr(pos, 5) == "false") { pos += 5; return false; }
        if (s.substr(pos, 4) == "null") { pos += 4; return nullptr; }
        if (std::isdigit(c) || c == '-' || c == '.') return parse_number(s, pos);
        
        if (s.substr(pos, 3) == "NaN") { pos += 3; return std::numeric_limits<double>::quiet_NaN(); }
        if (s.substr(pos, 8) == "Infinity") { pos += 8; return std::numeric_limits<double>::infinity(); }
        if (s.substr(pos, 9) == "-Infinity") { pos += 9; return -std::numeric_limits<double>::infinity(); }
        
        throw std::runtime_error("JSON parse error: invalid value at position " + std::to_string(pos));
    }
}  // namespace detail

[[nodiscard]] inline JsonValue parse_json(std::string_view json) {
    size_t pos = 0;
    JsonValue val = detail::parse_value(json, pos);
    detail::skip_whitespace(json, pos);
    if (pos != json.size()) 
        throw std::runtime_error("JSON parse error: extra data after JSON value");
    return val;
}

// ====================================================================
// to_json and from_json implementations
// ====================================================================

// Forward declare for use in map deserialization
template <typename T>
[[nodiscard]] T from_json_string(std::string_view json_str);

// Basic types - to_json (output parameter version for consistency with macros)
inline void to_json(JsonValue& j, std::nullptr_t) { j = nullptr; }
inline void to_json(JsonValue& j, bool value) { j = value; }
inline void to_json(JsonValue& j, int value) { j = static_cast<double>(value); }
inline void to_json(JsonValue& j, unsigned int value) { j = static_cast<double>(value); }
inline void to_json(JsonValue& j, long value) { j = static_cast<double>(value); }
inline void to_json(JsonValue& j, unsigned long value) { j = static_cast<double>(value); }
inline void to_json(JsonValue& j, long long value) { j = static_cast<double>(value); }
inline void to_json(JsonValue& j, unsigned long long value) { j = static_cast<double>(value); }
inline void to_json(JsonValue& j, float value) { j = static_cast<double>(value); }
inline void to_json(JsonValue& j, double value) { j = value; }
inline void to_json(JsonValue& j, const std::string& value) { j = value; }
inline void to_json(JsonValue& j, const char* value) { j = std::string(value); }

// Return-value versions for backward compatibility and direct usage
inline JsonValue to_json(std::nullptr_t) { return nullptr; }
inline JsonValue to_json(bool value) { return value; }
inline JsonValue to_json(int value) { return static_cast<double>(value); }
inline JsonValue to_json(unsigned int value) { return static_cast<double>(value); }
inline JsonValue to_json(long value) { return static_cast<double>(value); }
inline JsonValue to_json(unsigned long value) { return static_cast<double>(value); }
inline JsonValue to_json(long long value) { return static_cast<double>(value); }
inline JsonValue to_json(unsigned long long value) { return static_cast<double>(value); }
inline JsonValue to_json(float value) { return static_cast<double>(value); }
inline JsonValue to_json(double value) { return value; }
inline JsonValue to_json(const std::string& value) { return value; }
inline JsonValue to_json(const char* value) { return std::string(value); }

// Basic types - from_json (output parameter version)
inline void from_json(const JsonValue& j, bool& value) {
    if (!j.is_bool()) throw std::runtime_error("JSON type mismatch: expected bool");
    value = std::get<bool>(j);
}

inline void from_json(const JsonValue& j, int& value) {
    if (!j.is_number()) throw std::runtime_error("JSON type mismatch: expected number");
    double d = std::get<double>(j);
    if (d < static_cast<double>(std::numeric_limits<int>::min()) || 
        d > static_cast<double>(std::numeric_limits<int>::max())) {
        throw std::runtime_error("JSON number out of range for int");
    }
    value = static_cast<int>(d);
}

inline void from_json(const JsonValue& j, long& value) {
    if (!j.is_number()) throw std::runtime_error("JSON type mismatch: expected number");
    double d = std::get<double>(j);
    if (d < static_cast<double>(std::numeric_limits<long>::min()) || 
        d > static_cast<double>(std::numeric_limits<long>::max())) {
        throw std::runtime_error("JSON number out of range for long");
    }
    value = static_cast<long>(d);
}

inline void from_json(const JsonValue& j, long long& value) {
    if (!j.is_number()) throw std::runtime_error("JSON type mismatch: expected number");
    double d = std::get<double>(j);
    // Note: double can't precisely represent all long long values
    if (d < static_cast<double>(std::numeric_limits<long long>::min()) || 
        d > static_cast<double>(std::numeric_limits<long long>::max())) {
        throw std::runtime_error("JSON number out of range for long long");
    }
    value = static_cast<long long>(d);
}

inline void from_json(const JsonValue& j, unsigned int& value) {
    if (!j.is_number()) throw std::runtime_error("JSON type mismatch: expected number");
    double d = std::get<double>(j);
    if (d < 0.0 || d > static_cast<double>(std::numeric_limits<unsigned int>::max())) {
        throw std::runtime_error("JSON number out of range for unsigned int");
    }
    value = static_cast<unsigned int>(d);
}

inline void from_json(const JsonValue& j, unsigned long& value) {
    if (!j.is_number()) throw std::runtime_error("JSON type mismatch: expected number");
    double d = std::get<double>(j);
    if (d < 0.0 || d > static_cast<double>(std::numeric_limits<unsigned long>::max())) {
        throw std::runtime_error("JSON number out of range for unsigned long");
    }
    value = static_cast<unsigned long>(d);
}

inline void from_json(const JsonValue& j, unsigned long long& value) {
    if (!j.is_number()) throw std::runtime_error("JSON type mismatch: expected number");
    double d = std::get<double>(j);
    if (d < 0.0 || d > static_cast<double>(std::numeric_limits<unsigned long long>::max())) {
        throw std::runtime_error("JSON number out of range for unsigned long long");
    }
    value = static_cast<unsigned long long>(d);
}

inline void from_json(const JsonValue& j, double& value) {
    if (!j.is_number()) throw std::runtime_error("JSON type mismatch: expected number");
    value = std::get<double>(j);
}

inline void from_json(const JsonValue& j, float& value) {
    if (!j.is_number()) throw std::runtime_error("JSON type mismatch: expected number");
    value = static_cast<float>(std::get<double>(j));
}

inline void from_json(const JsonValue& j, std::string& value) {
    if (!j.is_string()) throw std::runtime_error("JSON type mismatch: expected string");
    value = std::get<std::string>(j);
}

// Containers - to_json (output parameter versions)
template <typename T>
void to_json(JsonValue& j, const std::vector<T>& vec) {
    JsonArray arr;
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

template <typename K, typename V>
void to_json(JsonValue& j, const std::map<K, V>& m) {
    JsonObject obj;
    for (const auto& [key, val] : m) {
        if constexpr (std::is_convertible_v<K, std::string>) {
            obj[key] = to_json(val);
        } else {
            obj[to_json_string(key)] = to_json(val);
        }
    }
    j = std::move(obj);
}

template <typename T>
void to_json(JsonValue& j, const std::optional<T>& opt) {
    if (opt.has_value()) {
        j = to_json(*opt);
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
template <typename T>
JsonValue to_json(const std::vector<T>& vec) {
    JsonArray arr;
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

template <typename K, typename V>
JsonValue to_json(const std::map<K, V>& m) {
    JsonObject obj;
    for (const auto& [key, val] : m) {
        if constexpr (std::is_convertible_v<K, std::string>) {
            obj[key] = to_json(val);
        } else {
            obj[to_json_string(key)] = to_json(val);
        }
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
        } else {
            m[from_json_string<K>(key)] = std::move(value);
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
    if (!j.is_array()) throw std::runtime_error("JSON type mismatch: expected array");
    const auto& arr = std::get<JsonArray>(j);
    if (arr.size() >= 2) {
        from_json(arr[0], p.first);
        from_json(arr[1], p.second);
    }
}

template <typename... Ts>
void from_json(const JsonValue& j, std::tuple<Ts...>& tup) {
    if (!j.is_array()) throw std::runtime_error("JSON type mismatch: expected array");
    const auto& arr = std::get<JsonArray>(j);
    from_json_tuple_impl(arr, tup, std::index_sequence_for<Ts...>{});
}

// Helper for tuple deserialization
template <typename Tuple, std::size_t... I>
void from_json_tuple_impl(const JsonArray& arr, Tuple& tup, std::index_sequence<I...>) {
    (..., (I < arr.size() ? from_json(arr[I], std::get<I>(tup)) : void()));
}

// Template return-value from_json for backward compatibility
template <typename T>
T from_json(const JsonValue& j) {
    T result;
    from_json(j, result);
    return result;
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

// ====================================================================
// File I/O
// ====================================================================

[[nodiscard]] inline JsonValue load_json_from_file(const std::string& filename) {
    std::ifstream ifs(filename, std::ios::binary);
    if (!ifs) 
        throw std::runtime_error("Failed to open file: " + filename);
    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    return parse_json(content);
}

template <typename Policy = StandardJsonPolicy>
inline void save_json_to_file(const std::string& filename, const JsonValue& val, bool pretty = Policy::pretty_print) {
    std::ofstream ofs(filename);
    if (!ofs) 
        throw std::runtime_error("Failed to open file: " + filename);
    to_json_stream<JsonValue, Policy>(ofs, val, pretty);
}

// ====================================================================
// Convenience
// ====================================================================

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
 * @tparam T The parameter struct type
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
 * @tparam T The parameter struct type
 * @param filename Path to the input file
 * @return The deserialized parameter struct
 * @throws std::runtime_error if file cannot be opened or JSON is invalid
 */
template<typename T>
[[nodiscard]] inline T load_params(const std::string& filename) {
    return from_json<T>(load_json_from_file(filename));
}

/**
 * @brief Save application parameters with a backup of the old file
 * @tparam T The parameter struct type
 * @tparam Policy The JSON formatting policy
 * @param filename Path to the output file
 * @param params The parameter struct to save
 * @param backup_suffix Suffix for backup file (default: ".bak")
 */
template<typename T, typename Policy = PrettyJsonPolicy>
inline void save_params_with_backup(const std::string& filename, const T& params, 
                                   const std::string& backup_suffix = ".bak") {
    // If file exists, create backup
    std::ifstream test(filename);
    if (test.good()) {
        test.close();
        std::string backup_name = filename + backup_suffix;
        std::ifstream src(filename, std::ios::binary);
        std::ofstream dst(backup_name, std::ios::binary);
        dst << src.rdbuf();
    }
    save_params<T, Policy>(filename, params);
}

}  // namespace cpp_utilities