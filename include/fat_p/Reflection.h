#pragma once

/*
FATP_META:
  meta_version: 1
  component: Reflection
  file_role: public_header
  path: include/fat_p/Reflection.h
  namespace: fat_p
  layer: Foundation
  summary: "Public header for Reflection."
  api_stability: in_work
  related:
    docs_search: "Reflection"
    tests:
      - components/Reflection/tests/test_Reflection.cpp
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 54
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/

/**
 * @file Reflection.h
 * @brief Advanced compile-time reflection system with unified C++17/C++20 macro syntax
 *
 *
 * @layer Domain
 *
 * @version 3.0.1 - Unified REFLECT_REGISTER for both C++17 and C++20 (MSVC-compatible)
 *
 * C++20: NTTP-based field names, compile-time strings
 * C++17: Constructor-based field names, minimal boilerplate
 * Both: Same FATP_REFLECT_REGISTER(Type, field1, field2, ...) syntax
 *
 * Features:
 * - Unified macro interface across C++17 and C++20
 * - Two-stage registration (declare in namespace, register at global scope)
 * - Linear O(N) field lookup (both versions)
 * - Zero runtime overhead for indexed field access
 * - Future-proof for C++26 native reflection
 * - MSVC-compatible macro expansion (v3.0.1)
 *
 * Changes in v3.0.1:
 * - Added MSVC workaround for __VA_ARGS__ expansion in nested macros
 * - Uses FATP_REFLECT_MAP_EXPAND on MSVC to force proper macro expansion
 * - GCC/Clang behavior unchanged
 *
 * Changes in v3.0:
 * - Eliminated REFLECT_MANUAL (legacy) - now both C++17 and C++20 use REFLECT_REGISTER
 * - C++17 uses Field constructor with #field for names (no boilerplate)
 * - Macro expansion fixed for proper FATP_REFLECT_COUNT evaluation
 * - Extended to support up to 32 fields
 */

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include "ConstexprUtilities.h"
#include "CppFeatureDetection.h"

namespace fat_p
{

// ============================================================================
// C++26 Reflection Detection (P2996)
// ============================================================================

#if FATP_HAS_REFLECTION
#define FATP_HAS_CPP26_REFLECTION 1
#else
#define FATP_HAS_CPP26_REFLECTION 0
#endif

// ============================================================================
// MSVC Workarounds for Macro Expansion
// ============================================================================

// MSVC has a known bug where __VA_ARGS__ is not expanded properly
// when passed to another macro. We need multiple expansion layers.
#if defined(_MSC_VER)
#define FATP_EXPAND(...) __VA_ARGS__
#define FATP_EXPAND1(...) FATP_EXPAND(__VA_ARGS__)
#define FATP_EXPAND2(...) FATP_EXPAND1(FATP_EXPAND1(__VA_ARGS__))
#define FATP_EXPAND3(...) FATP_EXPAND2(FATP_EXPAND2(__VA_ARGS__))
#else
#define FATP_EXPAND(...) __VA_ARGS__
#define FATP_EXPAND1(...) __VA_ARGS__
#define FATP_EXPAND2(...) __VA_ARGS__
#define FATP_EXPAND3(...) __VA_ARGS__
#endif

// Concatenation macros
#define FATP_CONCAT_IMPL(x, y) x##y
#define FATP_CONCAT(x, y) FATP_CONCAT_IMPL(x, y)

// ============================================================================
// C++26: Native Reflection (P2996) - Experimental (Stub)
// ============================================================================

#if FATP_HAS_CPP26_REFLECTION
// Future: Native reflection support when compilers implement P2996
// For now, fall through to C++20 implementation
#endif

// ============================================================================
// Fixed String for NTTP (C++20)
// ============================================================================

namespace detail
{

/**
 * @brief Compile-time string for use as non-type template parameter
 */
template <size_t N>
struct fixed_string
{
    char data[N];

    constexpr fixed_string(const char (&str)[N])
    {
        for (size_t i = 0; i < N; ++i)
        {
            data[i] = str[i];
        }
    }

    constexpr std::string_view view() const
    {
        return std::string_view(data, N - 1);
    }

    constexpr operator std::string_view() const
    {
        return view();
    }

    constexpr size_t size() const
    {
        return N - 1;
    }
    constexpr const char* c_str() const
    {
        return data;
    }
};

} // namespace detail

// ============================================================================
// Forward Declarations
// ============================================================================

template <typename T>
struct Reflectable;

// Forward declarations of reflection interface functions
template <typename T>
constexpr size_t field_count();

template <size_t I, typename T>
constexpr decltype(auto) get_field(T& obj);

template <size_t I, typename T>
constexpr decltype(auto) get_field(const T& obj);

template <size_t I, typename T>
constexpr std::string_view get_field_name();

namespace detail
{

/**
 * @brief SFINAE helper to detect if a type has reflection metadata
 */
template <typename T, typename = void>
struct has_reflection : std::false_type
{
};

template <typename T>
struct has_reflection<T, std::void_t<decltype(Reflectable<T>::field_count)>> : std::true_type
{
};

template <typename T>
inline constexpr bool is_reflectable_v = has_reflection<T>::value;

} // namespace detail

// ============================================================================
// Field Information Storage
// ============================================================================

/**
 * @brief Field metadata with compile-time name (C++20 NTTP)
 */
template <typename Class, typename FieldType, auto Ptr, detail::fixed_string Name>
struct Field
{
    using class_type = Class;
    using field_type = FieldType;

    static constexpr auto pointer = Ptr;
    static constexpr auto name = Name;

    static constexpr std::string_view get_name()
    {
        return name.view();
    }

    static constexpr uint64_t get_hash()
    {
        return constexpr_hash64(get_name());
    }

    static constexpr decltype(auto) get(Class& obj)
    {
        return obj.*pointer;
    }

    static constexpr decltype(auto) get(const Class& obj)
    {
        return obj.*pointer;
    }

    template <typename U>
    static constexpr void set(Class& obj, U&& value)
    {
        obj.*pointer = std::forward<U>(value);
    }
};

// ============================================================================
// Reflection Helpers
// ============================================================================

namespace detail
{

// Visit fields helper (fold expression for unpacking)
template <typename T, typename Visitor, size_t... Is>
constexpr void visit_fields_impl(T& obj, Visitor&& visitor, std::index_sequence<Is...>)
{
    (visitor(get_field_name<Is, T>(), get_field<Is>(obj)), ...);
}

template <typename T, typename Visitor, size_t... Is>
constexpr void visit_fields_impl(const T& obj, Visitor&& visitor, std::index_sequence<Is...>)
{
    (visitor(get_field_name<Is, T>(), get_field<Is>(obj)), ...);
}

// Get field names helper
template <typename T, size_t... Is>
constexpr auto get_field_names_impl(std::index_sequence<Is...>)
{
    return std::array<std::string_view, sizeof...(Is)>{get_field_name<Is, T>()...};
}

// To tuple helper
template <typename T, size_t... Is>
constexpr auto to_tuple_impl(T& obj, std::index_sequence<Is...>)
{
    return std::tie(get_field<Is>(obj)...);
}

template <typename T, size_t... Is>
constexpr auto to_tuple_impl(const T& obj, std::index_sequence<Is...>)
{
    return std::tie(get_field<Is>(obj)...);
}

// Linear has_field
template <typename T, size_t I = 0>
constexpr bool has_field_impl(std::string_view name)
{
    if constexpr (I >= field_count<T>())
    {
        return false;
    }
    else
    {
        return (get_field_name<I, T>() == name) || has_field_impl<T, I + 1>(name);
    }
}

// Linear visit_field
template <typename T, size_t I = 0, typename Func>
constexpr bool visit_field_impl(T& obj, std::string_view name, Func&& func)
{
    if constexpr (I >= field_count<T>())
    {
        return false;
    }
    else if (get_field_name<I, T>() == name)
    {
        func(get_field<I>(obj));
        return true;
    }
    else
    {
        return visit_field_impl<T, I + 1>(obj, name, std::forward<Func>(func));
    }
}

template <typename T, size_t I = 0, typename Func>
constexpr bool visit_field_impl(const T& obj, std::string_view name, Func&& func)
{
    if constexpr (I >= field_count<T>())
    {
        return false;
    }
    else if (get_field_name<I, T>() == name)
    {
        func(get_field<I>(obj));
        return true;
    }
    else
    {
        return visit_field_impl<T, I + 1>(obj, name, std::forward<Func>(func));
    }
}

} // namespace detail

// ============================================================================
// Reflection Interface
// ============================================================================

template <typename T>
constexpr size_t field_count()
{
    static_assert(detail::is_reflectable_v<T>, "Type must be registered with REFLECT_REGISTER or REFLECT_MANUAL");
    return Reflectable<T>::field_count;
}

template <size_t I, typename T>
constexpr decltype(auto) get_field(T& obj)
{
    static_assert(detail::is_reflectable_v<T>, "Type must be registered with REFLECT_REGISTER or REFLECT_MANUAL");
    static_assert(I < field_count<T>(), "Field index out of bounds");
    return Reflectable<T>::template get_field<I>(obj);
}

template <size_t I, typename T>
constexpr decltype(auto) get_field(const T& obj)
{
    static_assert(detail::is_reflectable_v<T>, "Type must be registered with REFLECT_REGISTER or REFLECT_MANUAL");
    static_assert(I < field_count<T>(), "Field index out of bounds");
    return Reflectable<T>::template get_field<I>(obj);
}

template <size_t I, typename T>
constexpr std::string_view get_field_name()
{
    static_assert(detail::is_reflectable_v<T>, "Type must be registered with REFLECT_REGISTER or REFLECT_MANUAL");
    static_assert(I < field_count<T>(), "Field index out of bounds");
    return Reflectable<T>::template field_name<I>();
}

template <size_t I, typename T>
using field_type_t = typename Reflectable<T>::template field_type<I>;

template <typename T>
constexpr bool is_reflectable()
{
    return detail::is_reflectable_v<T>;
}

template <typename T>
class FieldAccessor
{
public:
    static constexpr bool has_field(std::string_view name)
    {
        return detail::has_field_impl<T>(name);
    }

    template <typename Func>
    static constexpr bool visit_field(T& obj, std::string_view name, Func&& func)
    {
        return detail::visit_field_impl(obj, name, std::forward<Func>(func));
    }

    template <typename Func>
    static constexpr bool visit_field(const T& obj, std::string_view name, Func&& func)
    {
        return detail::visit_field_impl(obj, name, std::forward<Func>(func));
    }
};

/**
 * @brief Visit all fields with a visitor function
 */
template <typename T, typename Visitor>
constexpr void visit_fields(T& obj, Visitor&& visitor)
{
    static_assert(detail::is_reflectable_v<T>, "Type must be registered with REFLECT_REGISTER or REFLECT_MANUAL");

    detail::visit_fields_impl(obj, std::forward<Visitor>(visitor), std::make_index_sequence<field_count<T>()>{});
}

template <typename T, typename Visitor>
constexpr void visit_fields(const T& obj, Visitor&& visitor)
{
    static_assert(detail::is_reflectable_v<T>, "Type must be registered with REFLECT_REGISTER or REFLECT_MANUAL");

    detail::visit_fields_impl(obj, std::forward<Visitor>(visitor), std::make_index_sequence<field_count<T>()>{});
}

/**
 * @brief Get all field names as a compile-time array
 */
template <typename T>
constexpr auto get_field_names()
{
    return detail::get_field_names_impl<T>(std::make_index_sequence<field_count<T>()>{});
}

/**
 * @brief Convert reflectable object to tuple
 */
template <typename T>
constexpr auto to_tuple(T& obj)
{
    return detail::to_tuple_impl(obj, std::make_index_sequence<field_count<T>()>{});
}

template <typename T>
constexpr auto to_tuple(const T& obj)
{
    return detail::to_tuple_impl(obj, std::make_index_sequence<field_count<T>()>{});
}

/**
 * @brief Get type name
 */
template <typename T>
constexpr std::string_view type_name()
{
#if defined(__clang__) || defined(__GNUC__)
    constexpr std::string_view full_name = __PRETTY_FUNCTION__;
    constexpr auto start = full_name.find("T = ") + 4;
    constexpr auto end = full_name.find_first_of("];", start);
    return full_name.substr(start, end - start);
#elif defined(_MSC_VER)
    constexpr std::string_view full_name = __FUNCSIG__;
    constexpr auto start = full_name.find("type_name<") + 10;
    constexpr auto end = full_name.find_first_of(">(", start);
    return full_name.substr(start, end - start);
#else
    return "unknown";
#endif
}

/**
 * @brief Simple debug string representation
 */
template <typename T>
std::string to_debug_string(const T& obj)
{
    std::string result = std::string(type_name<T>()) + " { ";

    bool first = true;
    visit_fields(obj, [&](std::string_view name, const auto& value) {
        if (!first)
        {
            result += ", ";
        }
        first = false;

        result += std::string(name) + ": ";

        using ValueType = std::decay_t<decltype(value)>;
        if constexpr (std::is_integral_v<ValueType>)
        {
            result += std::to_string(value);
        }
        else if constexpr (std::is_floating_point_v<ValueType>)
        {
            result += std::to_string(value);
        }
        else if constexpr (std::is_same_v<ValueType, std::string>)
        {
            result += "\"" + value + "\"";
        }
        else if constexpr (std::is_same_v<ValueType, std::string_view>)
        {
            result += "\"" + std::string(value) + "\"";
        }
        else if constexpr (std::is_pointer_v<ValueType>)
        {
            result += value ? "non-null" : "null";
        }
        else
        {
            result += "?";
        }
    });

    result += " }";
    return result;
}

// ============================================================================
// Registration Macros (Unified for C++17/C++20)
// ============================================================================

/**
 * @brief Declare reflection intent (optional, for documentation)
 * Usage inside namespace: FATP_REFLECT_DECLARE(Point, x, y)
 */
#define FATP_REFLECT_DECLARE(Type, ...) static_assert(sizeof(Type), "Type must be complete for reflection declaration")

/**
 * @brief Register a type for reflection (unified version)
 * IMPORTANT: Must be called at GLOBAL SCOPE with qualified Type
 *
 * Usage:
 *   FATP_REFLECT_REGISTER(::fat_p::testing::Point, x, y)
 */
#define FATP_REFLECT_REGISTER(Type, ...)                                                                         \
    template <>                                                                                                  \
    struct fat_p::Reflectable<Type>                                                                              \
    {                                                                                                            \
        static constexpr auto fields = std::make_tuple(FATP_REFLECT_MAP(FATP_REFLECT_FIELD, Type, __VA_ARGS__)); \
        static constexpr size_t field_count = FATP_REFLECT_COUNT(__VA_ARGS__);                                   \
                                                                                                                 \
        template <size_t I>                                                                                      \
        using field_type = typename std::tuple_element_t<I, decltype(fields)>::field_type;                       \
                                                                                                                 \
        template <size_t I>                                                                                      \
        static constexpr std::string_view field_name()                                                           \
        {                                                                                                        \
            return std::get<I>(fields).get_name();                                                               \
        }                                                                                                        \
                                                                                                                 \
        template <size_t I>                                                                                      \
        static constexpr uint64_t field_hash()                                                                   \
        {                                                                                                        \
            return std::get<I>(fields).get_hash();                                                               \
        }                                                                                                        \
                                                                                                                 \
        template <size_t I>                                                                                      \
        static constexpr decltype(auto) get_field(Type& obj)                                                     \
        {                                                                                                        \
            return std::get<I>(fields).get(obj);                                                                 \
        }                                                                                                        \
                                                                                                                 \
        template <size_t I>                                                                                      \
        static constexpr decltype(auto) get_field(const Type& obj)                                               \
        {                                                                                                        \
            return std::get<I>(fields).get(obj);                                                                 \
        }                                                                                                        \
    };

// Field macro (C++20 NTTP version)
#define FATP_REFLECT_FIELD(Type, field)                      \
    Field<Type, decltype(Type::field), &Type::field, #field> \
    {                                                        \
    }

// Count macro
#define FATP_REFLECT_COUNT(...)                       \
    FATP_EXPAND3(FATP_REFLECT_COUNT_IMPL(__VA_ARGS__, \
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

#define FATP_REFLECT_COUNT_IMPL(_1,  \
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
                                N,   \
                                ...) \
    N

// Map macro - applies macro m to each field with Type as first arg
#define FATP_REFLECT_MAP(m, Type, ...) FATP_EXPAND3(FATP_REFLECT_MAP_IMPL(m, Type, __VA_ARGS__))
#define FATP_REFLECT_MAP_IMPL(m, Type, ...) \
    FATP_EXPAND2(FATP_CONCAT(FATP_REFLECT_MAP_, FATP_REFLECT_COUNT(__VA_ARGS__))(m, Type, __VA_ARGS__))

// Define REFLECT_MAP_0 to REFLECT_MAP_32
#define FATP_REFLECT_MAP_0(m, T)

#define FATP_REFLECT_MAP_1(m, T, a) m(T, a)

#define FATP_REFLECT_MAP_2(m, T, a, b) m(T, a), m(T, b)

#define FATP_REFLECT_MAP_3(m, T, a, b, c) m(T, a), m(T, b), m(T, c)

#define FATP_REFLECT_MAP_4(m, T, a, b, c, d) m(T, a), m(T, b), m(T, c), m(T, d)

#define FATP_REFLECT_MAP_5(m, T, a, b, c, d, e) m(T, a), m(T, b), m(T, c), m(T, d), m(T, e)

#define FATP_REFLECT_MAP_6(m, T, a, b, c, d, e, f) m(T, a), m(T, b), m(T, c), m(T, d), m(T, e), m(T, f)

#define FATP_REFLECT_MAP_7(m, T, a, b, c, d, e, f, g) m(T, a), m(T, b), m(T, c), m(T, d), m(T, e), m(T, f), m(T, g)

#define FATP_REFLECT_MAP_8(m, T, a, b, c, d, e, f, g, h) \
    m(T, a), m(T, b), m(T, c), m(T, d), m(T, e), m(T, f), m(T, g), m(T, h)

#define FATP_REFLECT_MAP_9(m, T, a, b, c, d, e, f, g, h, i) \
    m(T, a), m(T, b), m(T, c), m(T, d), m(T, e), m(T, f), m(T, g), m(T, h), m(T, i)

#define FATP_REFLECT_MAP_10(m, T, a, b, c, d, e, f, g, h, i, j) \
    m(T, a), m(T, b), m(T, c), m(T, d), m(T, e), m(T, f), m(T, g), m(T, h), m(T, i), m(T, j)

#define FATP_REFLECT_MAP_11(m, T, a, b, c, d, e, f, g, h, i, j, k) \
    m(T, a), m(T, b), m(T, c), m(T, d), m(T, e), m(T, f), m(T, g), m(T, h), m(T, i), m(T, j), m(T, k)

#define FATP_REFLECT_MAP_12(m, T, a, b, c, d, e, f, g, h, i, j, k, l) \
    m(T, a), m(T, b), m(T, c), m(T, d), m(T, e), m(T, f), m(T, g), m(T, h), m(T, i), m(T, j), m(T, k), m(T, l)

#define FATP_REFLECT_MAP_13(m, T, a, b, c, d, e, f, g, h, i, j, k, l, m1) \
    m(T, a), m(T, b), m(T, c), m(T, d), m(T, e), m(T, f), m(T, g), m(T, h), m(T, i), m(T, j), m(T, k), m(T, l), m(T, m1)

#define FATP_REFLECT_MAP_14(m, T, a, b, c, d, e, f, g, h, i, j, k, l, m1, n)                                    \
    m(T, a), m(T, b), m(T, c), m(T, d), m(T, e), m(T, f), m(T, g), m(T, h), m(T, i), m(T, j), m(T, k), m(T, l), \
        m(T, m1), m(T, n)

#define FATP_REFLECT_MAP_15(m, T, a, b, c, d, e, f, g, h, i, j, k, l, m1, n, o)                                 \
    m(T, a), m(T, b), m(T, c), m(T, d), m(T, e), m(T, f), m(T, g), m(T, h), m(T, i), m(T, j), m(T, k), m(T, l), \
        m(T, m1), m(T, n), m(T, o)

#define FATP_REFLECT_MAP_16(m, T, a, b, c, d, e, f, g, h, i, j, k, l, m1, n, o, p)                              \
    m(T, a), m(T, b), m(T, c), m(T, d), m(T, e), m(T, f), m(T, g), m(T, h), m(T, i), m(T, j), m(T, k), m(T, l), \
        m(T, m1), m(T, n), m(T, o), m(T, p)

#define FATP_REFLECT_MAP_17(m, T, a, b, c, d, e, f, g, h, i, j, k, l, m1, n, o, p, q)                           \
    m(T, a), m(T, b), m(T, c), m(T, d), m(T, e), m(T, f), m(T, g), m(T, h), m(T, i), m(T, j), m(T, k), m(T, l), \
        m(T, m1), m(T, n), m(T, o), m(T, p), m(T, q)

#define FATP_REFLECT_MAP_18(m, T, a, b, c, d, e, f, g, h, i, j, k, l, m1, n, o, p, q, r)                        \
    m(T, a), m(T, b), m(T, c), m(T, d), m(T, e), m(T, f), m(T, g), m(T, h), m(T, i), m(T, j), m(T, k), m(T, l), \
        m(T, m1), m(T, n), m(T, o), m(T, p), m(T, q), m(T, r)

#define FATP_REFLECT_MAP_19(m, T, a, b, c, d, e, f, g, h, i, j, k, l, m1, n, o, p, q, r, s)                     \
    m(T, a), m(T, b), m(T, c), m(T, d), m(T, e), m(T, f), m(T, g), m(T, h), m(T, i), m(T, j), m(T, k), m(T, l), \
        m(T, m1), m(T, n), m(T, o), m(T, p), m(T, q), m(T, r), m(T, s)

#define FATP_REFLECT_MAP_20(m, T, a, b, c, d, e, f, g, h, i, j, k, l, m1, n, o, p, q, r, s, t)                  \
    m(T, a), m(T, b), m(T, c), m(T, d), m(T, e), m(T, f), m(T, g), m(T, h), m(T, i), m(T, j), m(T, k), m(T, l), \
        m(T, m1), m(T, n), m(T, o), m(T, p), m(T, q), m(T, r), m(T, s), m(T, t)

#define FATP_REFLECT_MAP_21(m, T, a, b, c, d, e, f, g, h, i, j, k, l, m1, n, o, p, q, r, s, t, u)               \
    m(T, a), m(T, b), m(T, c), m(T, d), m(T, e), m(T, f), m(T, g), m(T, h), m(T, i), m(T, j), m(T, k), m(T, l), \
        m(T, m1), m(T, n), m(T, o), m(T, p), m(T, q), m(T, r), m(T, s), m(T, t), m(T, u)

#define FATP_REFLECT_MAP_22(m, T, a, b, c, d, e, f, g, h, i, j, k, l, m1, n, o, p, q, r, s, t, u, v)            \
    m(T, a), m(T, b), m(T, c), m(T, d), m(T, e), m(T, f), m(T, g), m(T, h), m(T, i), m(T, j), m(T, k), m(T, l), \
        m(T, m1), m(T, n), m(T, o), m(T, p), m(T, q), m(T, r), m(T, s), m(T, t), m(T, u), m(T, v)

#define FATP_REFLECT_MAP_23(m, T, a, b, c, d, e, f, g, h, i, j, k, l, m1, n, o, p, q, r, s, t, u, v, w)         \
    m(T, a), m(T, b), m(T, c), m(T, d), m(T, e), m(T, f), m(T, g), m(T, h), m(T, i), m(T, j), m(T, k), m(T, l), \
        m(T, m1), m(T, n), m(T, o), m(T, p), m(T, q), m(T, r), m(T, s), m(T, t), m(T, u), m(T, v), m(T, w)

#define FATP_REFLECT_MAP_24(m, T, a, b, c, d, e, f, g, h, i, j, k, l, m1, n, o, p, q, r, s, t, u, v, w, x)      \
    m(T, a), m(T, b), m(T, c), m(T, d), m(T, e), m(T, f), m(T, g), m(T, h), m(T, i), m(T, j), m(T, k), m(T, l), \
        m(T, m1), m(T, n), m(T, o), m(T, p), m(T, q), m(T, r), m(T, s), m(T, t), m(T, u), m(T, v), m(T, w), m(T, x)

#define FATP_REFLECT_MAP_25(m, T, a, b, c, d, e, f, g, h, i, j, k, l, m1, n, o, p, q, r, s, t, u, v, w, x, y)        \
    m(T, a), m(T, b), m(T, c), m(T, d), m(T, e), m(T, f), m(T, g), m(T, h), m(T, i), m(T, j), m(T, k), m(T, l),      \
        m(T, m1), m(T, n), m(T, o), m(T, p), m(T, q), m(T, r), m(T, s), m(T, t), m(T, u), m(T, v), m(T, w), m(T, x), \
        m(T, y)

#define FATP_REFLECT_MAP_26(m, T, a, b, c, d, e, f, g, h, i, j, k, l, m1, n, o, p, q, r, s, t, u, v, w, x, y, z)     \
    m(T, a), m(T, b), m(T, c), m(T, d), m(T, e), m(T, f), m(T, g), m(T, h), m(T, i), m(T, j), m(T, k), m(T, l),      \
        m(T, m1), m(T, n), m(T, o), m(T, p), m(T, q), m(T, r), m(T, s), m(T, t), m(T, u), m(T, v), m(T, w), m(T, x), \
        m(T, y), m(T, z)

#define FATP_REFLECT_MAP_27(m, T, a, b, c, d, e, f, g, h, i, j, k, l, m1, n, o, p, q, r, s, t, u, v, w, x, y, z, aa) \
    m(T, a), m(T, b), m(T, c), m(T, d), m(T, e), m(T, f), m(T, g), m(T, h), m(T, i), m(T, j), m(T, k), m(T, l),      \
        m(T, m1), m(T, n), m(T, o), m(T, p), m(T, q), m(T, r), m(T, s), m(T, t), m(T, u), m(T, v), m(T, w), m(T, x), \
        m(T, y), m(T, z), m(T, aa)

#define FATP_REFLECT_MAP_28(m,                                                                                       \
                            T,                                                                                       \
                            a,                                                                                       \
                            b,                                                                                       \
                            c,                                                                                       \
                            d,                                                                                       \
                            e,                                                                                       \
                            f,                                                                                       \
                            g,                                                                                       \
                            h,                                                                                       \
                            i,                                                                                       \
                            j,                                                                                       \
                            k,                                                                                       \
                            l,                                                                                       \
                            m1,                                                                                      \
                            n,                                                                                       \
                            o,                                                                                       \
                            p,                                                                                       \
                            q,                                                                                       \
                            r,                                                                                       \
                            s,                                                                                       \
                            t,                                                                                       \
                            u,                                                                                       \
                            v,                                                                                       \
                            w,                                                                                       \
                            x,                                                                                       \
                            y,                                                                                       \
                            z,                                                                                       \
                            aa,                                                                                      \
                            ab)                                                                                      \
    m(T, a), m(T, b), m(T, c), m(T, d), m(T, e), m(T, f), m(T, g), m(T, h), m(T, i), m(T, j), m(T, k), m(T, l),      \
        m(T, m1), m(T, n), m(T, o), m(T, p), m(T, q), m(T, r), m(T, s), m(T, t), m(T, u), m(T, v), m(T, w), m(T, x), \
        m(T, y), m(T, z), m(T, aa), m(T, ab)

#define FATP_REFLECT_MAP_29(m,                                                                                       \
                            T,                                                                                       \
                            a,                                                                                       \
                            b,                                                                                       \
                            c,                                                                                       \
                            d,                                                                                       \
                            e,                                                                                       \
                            f,                                                                                       \
                            g,                                                                                       \
                            h,                                                                                       \
                            i,                                                                                       \
                            j,                                                                                       \
                            k,                                                                                       \
                            l,                                                                                       \
                            m1,                                                                                      \
                            n,                                                                                       \
                            o,                                                                                       \
                            p,                                                                                       \
                            q,                                                                                       \
                            r,                                                                                       \
                            s,                                                                                       \
                            t,                                                                                       \
                            u,                                                                                       \
                            v,                                                                                       \
                            w,                                                                                       \
                            x,                                                                                       \
                            y,                                                                                       \
                            z,                                                                                       \
                            aa,                                                                                      \
                            ab,                                                                                      \
                            ac)                                                                                      \
    m(T, a), m(T, b), m(T, c), m(T, d), m(T, e), m(T, f), m(T, g), m(T, h), m(T, i), m(T, j), m(T, k), m(T, l),      \
        m(T, m1), m(T, n), m(T, o), m(T, p), m(T, q), m(T, r), m(T, s), m(T, t), m(T, u), m(T, v), m(T, w), m(T, x), \
        m(T, y), m(T, z), m(T, aa), m(T, ab), m(T, ac)

#define FATP_REFLECT_MAP_30(m,                                                                                       \
                            T,                                                                                       \
                            a,                                                                                       \
                            b,                                                                                       \
                            c,                                                                                       \
                            d,                                                                                       \
                            e,                                                                                       \
                            f,                                                                                       \
                            g,                                                                                       \
                            h,                                                                                       \
                            i,                                                                                       \
                            j,                                                                                       \
                            k,                                                                                       \
                            l,                                                                                       \
                            m1,                                                                                      \
                            n,                                                                                       \
                            o,                                                                                       \
                            p,                                                                                       \
                            q,                                                                                       \
                            r,                                                                                       \
                            s,                                                                                       \
                            t,                                                                                       \
                            u,                                                                                       \
                            v,                                                                                       \
                            w,                                                                                       \
                            x,                                                                                       \
                            y,                                                                                       \
                            z,                                                                                       \
                            aa,                                                                                      \
                            ab,                                                                                      \
                            ac,                                                                                      \
                            ad)                                                                                      \
    m(T, a), m(T, b), m(T, c), m(T, d), m(T, e), m(T, f), m(T, g), m(T, h), m(T, i), m(T, j), m(T, k), m(T, l),      \
        m(T, m1), m(T, n), m(T, o), m(T, p), m(T, q), m(T, r), m(T, s), m(T, t), m(T, u), m(T, v), m(T, w), m(T, x), \
        m(T, y), m(T, z), m(T, aa), m(T, ab), m(T, ac), m(T, ad)

#define FATP_REFLECT_MAP_31(m,                                                                                       \
                            T,                                                                                       \
                            a,                                                                                       \
                            b,                                                                                       \
                            c,                                                                                       \
                            d,                                                                                       \
                            e,                                                                                       \
                            f,                                                                                       \
                            g,                                                                                       \
                            h,                                                                                       \
                            i,                                                                                       \
                            j,                                                                                       \
                            k,                                                                                       \
                            l,                                                                                       \
                            m1,                                                                                      \
                            n,                                                                                       \
                            o,                                                                                       \
                            p,                                                                                       \
                            q,                                                                                       \
                            r,                                                                                       \
                            s,                                                                                       \
                            t,                                                                                       \
                            u,                                                                                       \
                            v,                                                                                       \
                            w,                                                                                       \
                            x,                                                                                       \
                            y,                                                                                       \
                            z,                                                                                       \
                            aa,                                                                                      \
                            ab,                                                                                      \
                            ac,                                                                                      \
                            ad,                                                                                      \
                            ae)                                                                                      \
    m(T, a), m(T, b), m(T, c), m(T, d), m(T, e), m(T, f), m(T, g), m(T, h), m(T, i), m(T, j), m(T, k), m(T, l),      \
        m(T, m1), m(T, n), m(T, o), m(T, p), m(T, q), m(T, r), m(T, s), m(T, t), m(T, u), m(T, v), m(T, w), m(T, x), \
        m(T, y), m(T, z), m(T, aa), m(T, ab), m(T, ac), m(T, ad), m(T, ae)

#define FATP_REFLECT_MAP_32(m,                                                                                       \
                            T,                                                                                       \
                            a,                                                                                       \
                            b,                                                                                       \
                            c,                                                                                       \
                            d,                                                                                       \
                            e,                                                                                       \
                            f,                                                                                       \
                            g,                                                                                       \
                            h,                                                                                       \
                            i,                                                                                       \
                            j,                                                                                       \
                            k,                                                                                       \
                            l,                                                                                       \
                            m1,                                                                                      \
                            n,                                                                                       \
                            o,                                                                                       \
                            p,                                                                                       \
                            q,                                                                                       \
                            r,                                                                                       \
                            s,                                                                                       \
                            t,                                                                                       \
                            u,                                                                                       \
                            v,                                                                                       \
                            w,                                                                                       \
                            x,                                                                                       \
                            y,                                                                                       \
                            z,                                                                                       \
                            aa,                                                                                      \
                            ab,                                                                                      \
                            ac,                                                                                      \
                            ad,                                                                                      \
                            ae,                                                                                      \
                            af)                                                                                      \
    m(T, a), m(T, b), m(T, c), m(T, d), m(T, e), m(T, f), m(T, g), m(T, h), m(T, i), m(T, j), m(T, k), m(T, l),      \
        m(T, m1), m(T, n), m(T, o), m(T, p), m(T, q), m(T, r), m(T, s), m(T, t), m(T, u), m(T, v), m(T, w), m(T, x), \
        m(T, y), m(T, z), m(T, aa), m(T, ab), m(T, ac), m(T, ad), m(T, ae), m(T, af)

// Note: Internal helper macros (FATP_EXPAND*, FATP_CONCAT*, FATP_REFLECT_MAP_IMPL,
// FATP_REFLECT_COUNT_IMPL, FATP_REFLECT_MAP_0..32) are intentionally NOT #undef'd
// because they are required for macro expansion at user call sites when
// FATP_REFLECT_REGISTER is invoked. Since all macros now use the FATP_ prefix,
// namespace collision risk is eliminated per Rule F of the Systemic Hygiene Policy.

} // namespace fat_p
