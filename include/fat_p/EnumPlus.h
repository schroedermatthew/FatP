#pragma once

/*
FATP_META:
  meta_version: 1
  component: EnumPlus
  file_role: public_header
  path: include/fat_p/EnumPlus.h
  namespace: fat_p
  layer: Foundation
  summary: "Public header for EnumPlus."
  api_stability: in_work
  related:
    docs_search: "EnumPlus"
    tests:
      - components/EnumPlus/tests/test_EnumPlus.cpp
  hygiene:
    pragma_once: false
    include_guard: true
    defines_total: 1
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/

/**
 * @file EnumPlus.h
 * @brief Enhanced enum utilities with string conversion and iteration
 *
 */

#include <algorithm>
#include <array>
#include <concepts>
#include <functional>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

namespace fat_p
{

// ============================================================================
// Forward Declarations
// ============================================================================

template <typename E>
struct EnumSizeTrait;

template <typename E, bool Enable = false>
struct EnableOverloadedOperators : std::bool_constant<Enable> {};

/// Concept: E has opted into bitwise operators via EnableOverloadedOperators
template <typename E>
concept enum_with_operators = EnableOverloadedOperators<E>::value;

template <typename E>
struct EnumStringPolicy;

/// Concept: E is an enum with a specialized EnumStringPolicy providing to_string
template <typename E>
concept named_enum = std::is_enum_v<E> && requires(E e) {
    { EnumStringPolicy<E>::to_string(e) } -> std::convertible_to<std::string_view>;
};

// ============================================================================
// Bounds Check Policies
// ============================================================================

struct DefaultBoundsCheckPolicy
{
    template <typename E>
    static void check_bounds(std::size_t index, std::size_t size)
    {
        if (index >= size)
        {
            throw std::out_of_range("EnumPlusMap index out of bounds");
        }
    }
};

struct NoBoundsCheckPolicy
{
    template <typename E>
    static void check_bounds(std::size_t, std::size_t) noexcept
    {
    }
};

// ============================================================================
// Core Utility: to_underlying (needed by other components)
// ============================================================================

template <typename E>
    requires std::is_enum_v<E>
constexpr std::underlying_type_t<E> to_underlying(E value) noexcept
{
    return static_cast<std::underlying_type_t<E>>(value);
}

// ============================================================================
// EnumPlusWrapper - Wraps an enum to enable operator overloading
// ============================================================================

template <typename E>
class EnumPlusWrapper
{
    static_assert(std::is_enum_v<E>, "E must be an enum type");

public:
    using enum_type = E;
    using underlying_type = std::underlying_type_t<E>;

    constexpr EnumPlusWrapper() noexcept
        : mValue(static_cast<E>(0))
    {
    }
    constexpr explicit EnumPlusWrapper(E value) noexcept
        : mValue(value)
    {
    }

    // DELETE construction from underlying type -- enforces strong typing
    EnumPlusWrapper(underlying_type) = delete;
    EnumPlusWrapper& operator=(underlying_type) = delete;

    constexpr E value() const noexcept
    {
        return mValue;
    }
    constexpr underlying_type underlying() const noexcept
    {
        return static_cast<underlying_type>(mValue);
    }

    constexpr operator E() const noexcept
    {
        return mValue;
    }

    constexpr bool operator==(const EnumPlusWrapper& other) const noexcept
    {
        return mValue == other.mValue;
    }

    constexpr bool operator!=(const EnumPlusWrapper& other) const noexcept
    {
        return mValue != other.mValue;
    }

    constexpr bool operator==(E other) const noexcept
    {
        return mValue == other;
    }

    constexpr bool operator!=(E other) const noexcept
    {
        return mValue != other;
    }

private:
    E mValue;
};

// ============================================================================
// Bitwise Operators (enabled via EnableOverloadedOperators)
// ============================================================================

template <typename E>
    requires enum_with_operators<E>
constexpr EnumPlusWrapper<E> operator|(EnumPlusWrapper<E> lhs, EnumPlusWrapper<E> rhs) noexcept
{
    return EnumPlusWrapper<E>(static_cast<E>(lhs.underlying() | rhs.underlying()));
}

template <typename E>
    requires enum_with_operators<E>
constexpr E operator|(E lhs, E rhs) noexcept
{
    using U = std::underlying_type_t<E>;
    return static_cast<E>(static_cast<U>(lhs) | static_cast<U>(rhs));
}

template <typename E>
    requires enum_with_operators<E>
constexpr EnumPlusWrapper<E> operator|(EnumPlusWrapper<E> lhs, E rhs) noexcept
{
    return EnumPlusWrapper<E>(static_cast<E>(lhs.underlying() | static_cast<std::underlying_type_t<E>>(rhs)));
}

template <typename E>
    requires enum_with_operators<E>
constexpr EnumPlusWrapper<E> operator|(E lhs, EnumPlusWrapper<E> rhs) noexcept
{
    return EnumPlusWrapper<E>(static_cast<E>(static_cast<std::underlying_type_t<E>>(lhs) | rhs.underlying()));
}

template <typename E>
    requires enum_with_operators<E>
constexpr EnumPlusWrapper<E> operator&(EnumPlusWrapper<E> lhs, EnumPlusWrapper<E> rhs) noexcept
{
    return EnumPlusWrapper<E>(static_cast<E>(lhs.underlying() & rhs.underlying()));
}

template <typename E>
    requires enum_with_operators<E>
constexpr E operator&(E lhs, E rhs) noexcept
{
    using U = std::underlying_type_t<E>;
    return static_cast<E>(static_cast<U>(lhs) & static_cast<U>(rhs));
}

template <typename E>
    requires enum_with_operators<E>
constexpr EnumPlusWrapper<E> operator&(EnumPlusWrapper<E> lhs, E rhs) noexcept
{
    return EnumPlusWrapper<E>(static_cast<E>(lhs.underlying() & static_cast<std::underlying_type_t<E>>(rhs)));
}

template <typename E>
    requires enum_with_operators<E>
constexpr EnumPlusWrapper<E> operator&(E lhs, EnumPlusWrapper<E> rhs) noexcept
{
    return EnumPlusWrapper<E>(static_cast<E>(static_cast<std::underlying_type_t<E>>(lhs) & rhs.underlying()));
}

template <typename E>
    requires enum_with_operators<E>
constexpr EnumPlusWrapper<E> operator^(EnumPlusWrapper<E> lhs, EnumPlusWrapper<E> rhs) noexcept
{
    return EnumPlusWrapper<E>(static_cast<E>(lhs.underlying() ^ rhs.underlying()));
}

template <typename E>
    requires enum_with_operators<E>
constexpr E operator^(E lhs, E rhs) noexcept
{
    using U = std::underlying_type_t<E>;
    return static_cast<E>(static_cast<U>(lhs) ^ static_cast<U>(rhs));
}

template <typename E>
    requires enum_with_operators<E>
constexpr EnumPlusWrapper<E> operator^(EnumPlusWrapper<E> lhs, E rhs) noexcept
{
    return EnumPlusWrapper<E>(static_cast<E>(lhs.underlying() ^ static_cast<std::underlying_type_t<E>>(rhs)));
}

template <typename E>
    requires enum_with_operators<E>
constexpr EnumPlusWrapper<E> operator^(E lhs, EnumPlusWrapper<E> rhs) noexcept
{
    return EnumPlusWrapper<E>(static_cast<E>(static_cast<std::underlying_type_t<E>>(lhs) ^ rhs.underlying()));
}

template <typename E>
    requires enum_with_operators<E>
constexpr EnumPlusWrapper<E> operator~(EnumPlusWrapper<E> value) noexcept
{
    return EnumPlusWrapper<E>(static_cast<E>(~value.underlying()));
}

template <typename E>
    requires enum_with_operators<E>
constexpr E operator~(E value) noexcept
{
    using U = std::underlying_type_t<E>;
    return static_cast<E>(~static_cast<U>(value));
}

// Compound assignment operators
template <typename E>
    requires enum_with_operators<E>
EnumPlusWrapper<E>& operator|=(EnumPlusWrapper<E>& lhs, EnumPlusWrapper<E> rhs) noexcept
{
    lhs = lhs | rhs;
    return lhs;
}

template <typename E>
    requires enum_with_operators<E>
E& operator|=(E& lhs, E rhs) noexcept
{
    lhs = lhs | rhs;
    return lhs;
}

template <typename E>
    requires enum_with_operators<E>
EnumPlusWrapper<E>& operator|=(EnumPlusWrapper<E>& lhs, E rhs) noexcept
{
    lhs = lhs | rhs;
    return lhs;
}

template <typename E>
    requires enum_with_operators<E>
EnumPlusWrapper<E>& operator&=(EnumPlusWrapper<E>& lhs, EnumPlusWrapper<E> rhs) noexcept
{
    lhs = lhs & rhs;
    return lhs;
}

template <typename E>
    requires enum_with_operators<E>
E& operator&=(E& lhs, E rhs) noexcept
{
    lhs = lhs & rhs;
    return lhs;
}

template <typename E>
    requires enum_with_operators<E>
EnumPlusWrapper<E>& operator&=(EnumPlusWrapper<E>& lhs, E rhs) noexcept
{
    lhs = lhs & rhs;
    return lhs;
}

template <typename E>
    requires enum_with_operators<E>
EnumPlusWrapper<E>& operator^=(EnumPlusWrapper<E>& lhs, EnumPlusWrapper<E> rhs) noexcept
{
    lhs = lhs ^ rhs;
    return lhs;
}

template <typename E>
    requires enum_with_operators<E>
E& operator^=(E& lhs, E rhs) noexcept
{
    lhs = lhs ^ rhs;
    return lhs;
}

template <typename E>
    requires enum_with_operators<E>
EnumPlusWrapper<E>& operator^=(EnumPlusWrapper<E>& lhs, E rhs) noexcept
{
    lhs = lhs ^ rhs;
    return lhs;
}

// ============================================================================
// Bitwise Shift Operators (for flag manipulation)
// ============================================================================

template <typename E>
    requires enum_with_operators<E>
constexpr EnumPlusWrapper<E> operator<<(EnumPlusWrapper<E> lhs, int rhs) noexcept
{
    return EnumPlusWrapper<E>(static_cast<E>(lhs.underlying() << rhs));
}

template <typename E>
    requires enum_with_operators<E>
constexpr E operator<<(E lhs, int rhs) noexcept
{
    using U = std::underlying_type_t<E>;
    return static_cast<E>(static_cast<U>(lhs) << rhs);
}

template <typename E>
    requires enum_with_operators<E>
EnumPlusWrapper<E>& operator<<=(EnumPlusWrapper<E>& lhs, int rhs) noexcept
{
    lhs = lhs << rhs;
    return lhs;
}

template <typename E>
    requires enum_with_operators<E>
E& operator<<=(E& lhs, int rhs) noexcept
{
    lhs = lhs << rhs;
    return lhs;
}

template <typename E>
    requires enum_with_operators<E>
constexpr EnumPlusWrapper<E> operator>>(EnumPlusWrapper<E> lhs, int rhs) noexcept
{
    return EnumPlusWrapper<E>(static_cast<E>(lhs.underlying() >> rhs));
}

template <typename E>
    requires enum_with_operators<E>
constexpr E operator>>(E lhs, int rhs) noexcept
{
    using U = std::underlying_type_t<E>;
    return static_cast<E>(static_cast<U>(lhs) >> rhs);
}

template <typename E>
    requires enum_with_operators<E>
EnumPlusWrapper<E>& operator>>=(EnumPlusWrapper<E>& lhs, int rhs) noexcept
{
    lhs = lhs >> rhs;
    return lhs;
}

template <typename E>
    requires enum_with_operators<E>
E& operator>>=(E& lhs, int rhs) noexcept
{
    lhs = lhs >> rhs;
    return lhs;
}

// ============================================================================
// Stream Operators
// ============================================================================

template <named_enum E>
std::ostream& operator<<(std::ostream& os, E value)
{
    os << EnumStringPolicy<E>::to_string(value);
    return os;
}

template <typename E>
std::ostream& operator<<(std::ostream& os, EnumPlusWrapper<E> value)
{
    if constexpr (named_enum<E>)
    {
        os << EnumStringPolicy<E>::to_string(value.value());
    }
    else
    {
        os << static_cast<std::underlying_type_t<E>>(value.value());
    }
    return os;
}

// ============================================================================
// EnumPlusMap - Type-safe array-based mapping from enum to values
// ============================================================================

template <typename E, typename T, typename BoundsPolicy = DefaultBoundsCheckPolicy>
class EnumPlusMap
{
    static_assert(std::is_enum_v<E>, "E must be an enum type");

public:
    using enum_type = E;
    using value_type = T;
    using size_type = std::size_t;
    using reference = T&;
    using const_reference = const T&;
    using iterator = typename std::array<T, EnumSizeTrait<E>::size>::iterator;
    using const_iterator = typename std::array<T, EnumSizeTrait<E>::size>::const_iterator;

    static constexpr size_type enum_size = EnumSizeTrait<E>::size;

    // Default constructor
    constexpr EnumPlusMap()
        : mData{}
    {
    }

    // Constructor from initializer list (variadic)
    // Disabled when:
    // 1. Single argument of type T or convertible to T (use fill constructor)
    // 2. Single callable argument (use generator constructor)
    template <typename Arg1,
              typename... Args>
        requires ((sizeof...(Args) > 0) ||                  // Multiple args: always enabled
                  (!std::is_convertible_v<Arg1, T> &&        // Single arg: not convertible to T
                   !std::is_invocable_r_v<T, Arg1, E>))     // and not a generator function
    constexpr EnumPlusMap(Arg1&& arg1, Args&&... args)
        : mData{std::forward<Arg1>(arg1), std::forward<Args>(args)...}
    {
    }

    // Constructor with fill value
    constexpr explicit EnumPlusMap(const T& fill_value)
    {
        mData.fill(fill_value);
    }

    // Constructor with generator function
    template <typename Func>
        requires std::is_invocable_r_v<T, Func, E>
    constexpr explicit EnumPlusMap(Func&& generator)
    {
        for (size_type i = 0; i < enum_size; ++i)
        {
            mData[i] = generator(static_cast<E>(i));
        }
    }

    // Element access
    constexpr reference operator[](E key) noexcept
    {
        return mData[to_index(key)];
    }

    constexpr const_reference operator[](E key) const noexcept
    {
        return mData[to_index(key)];
    }

    constexpr reference at(E key)
    {
        const auto idx = to_index(key);
        BoundsPolicy::template check_bounds<E>(idx, enum_size);
        return mData[idx];
    }

    constexpr const_reference at(E key) const
    {
        const auto idx = to_index(key);
        BoundsPolicy::template check_bounds<E>(idx, enum_size);
        return mData[idx];
    }

    // Capacity
    constexpr size_type size() const noexcept
    {
        return enum_size;
    }
    constexpr bool empty() const noexcept
    {
        return enum_size == 0;
    }

    // Iterators
    constexpr iterator begin() noexcept
    {
        return mData.begin();
    }
    constexpr const_iterator begin() const noexcept
    {
        return mData.begin();
    }
    constexpr const_iterator cbegin() const noexcept
    {
        return mData.cbegin();
    }

    constexpr iterator end() noexcept
    {
        return mData.end();
    }
    constexpr const_iterator end() const noexcept
    {
        return mData.end();
    }
    constexpr const_iterator cend() const noexcept
    {
        return mData.cend();
    }

    // Fill
    constexpr void fill(const T& value)
    {
        mData.fill(value);
    }

    // Apply function to all elements
    template <typename Func>
    constexpr void for_each(Func&& func)
    {
        for (size_type i = 0; i < enum_size; ++i)
        {
            func(mData[i]);
        }
    }

    template <typename Func>
    constexpr void for_each(Func&& func) const
    {
        for (size_type i = 0; i < enum_size; ++i)
        {
            func(mData[i]);
        }
    }

    // Data access
    constexpr T* data() noexcept
    {
        return mData.data();
    }
    constexpr const T* data() const noexcept
    {
        return mData.data();
    }

private:
    std::array<T, enum_size> mData;

    static constexpr size_type to_index(E key) noexcept
    {
        return static_cast<size_type>(to_underlying(key));
    }
};

// ============================================================================
// Utility Functions
// ============================================================================

// Check if an integer value is a valid enum value
// Requires EnumSizeTrait<E> to be specialized (assumes contiguous 0-based enum)
template <typename E>
    requires std::is_enum_v<E>
constexpr bool is_valid_enum(std::underlying_type_t<E> value) noexcept
{
    return value >= 0 && static_cast<std::size_t>(value) < EnumSizeTrait<E>::size;
}

template <typename E>
    requires std::is_enum_v<E>
constexpr bool is_valid_enum(E value) noexcept
{
    return is_valid_enum<E>(to_underlying(value));
}

// Safe conversion to underlying - returns nullopt if invalid
template <typename E>
    requires std::is_enum_v<E>
constexpr std::optional<std::underlying_type_t<E>> safe_to_underlying(E value) noexcept
{
    if (is_valid_enum(value))
    {
        return to_underlying(value);
    }
    return std::nullopt;
}

// Safe cast from underlying type to enum
template <typename E>
    requires std::is_enum_v<E>
constexpr std::optional<E> safe_enum_cast(std::underlying_type_t<E> value) noexcept
{
    if (is_valid_enum<E>(value))
    {
        return static_cast<E>(value);
    }
    return std::nullopt;
}

// Get the 0-based index of an enum value
// Returns nullopt if the value is invalid
template <typename E>
    requires std::is_enum_v<E>
constexpr std::optional<std::size_t> enum_index(E value) noexcept
{
    auto underlying = to_underlying(value);
    if (is_valid_enum<E>(underlying))
    {
        return static_cast<std::size_t>(underlying);
    }
    return std::nullopt;
}

// Get the enum value at a given index
// Returns nullopt if the index is out of range
template <typename E>
    requires std::is_enum_v<E>
constexpr std::optional<E> enum_value(std::size_t index) noexcept
{
    if (index < EnumSizeTrait<E>::size)
    {
        return static_cast<E>(index);
    }
    return std::nullopt;
}

// Check if an enum value is valid (more explicit alias for is_valid_enum)
template <typename E>
    requires std::is_enum_v<E>
constexpr bool enum_contains(E value) noexcept
{
    return is_valid_enum(value);
}

// Compile-time enum count (alias for EnumSizeTrait<E>::size)
template <typename E>
inline constexpr std::size_t enum_count = EnumSizeTrait<E>::size;

// Get all enum values as an array
template <typename E>
    requires std::is_enum_v<E>
constexpr std::array<E, EnumSizeTrait<E>::size> enum_values() noexcept
{
    std::array<E, EnumSizeTrait<E>::size> values{};
    for (std::size_t i = 0; i < EnumSizeTrait<E>::size; ++i)
    {
        values[i] = static_cast<E>(i);
    }
    return values;
}

// Iterate over all enum values with a callable
template <typename E, typename Func>
constexpr void for_each_enum(Func&& func)
{
    static_assert(std::is_enum_v<E>, "E must be an enum type");
    constexpr auto values = enum_values<E>();
    for (auto v : values)
    {
        func(v);
    }
}

// Convert enum to string (requires EnumStringPolicy specialization)
template <named_enum E>
std::string_view to_string(E value)
{
    return EnumStringPolicy<E>::to_string(value);
}

// Convert string to enum (requires EnumStringPolicy specialization)
template <named_enum E>
E from_string(std::string_view str)
{
    return EnumStringPolicy<E>::from_string(str);
}

namespace detail
{

// Case-insensitive character comparison
inline constexpr bool char_eq_icase(char a, char b) noexcept
{
    auto to_lower = [](char c) -> char {
        return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c;
    };
    return to_lower(a) == to_lower(b);
}

// Case-insensitive string comparison
inline bool string_eq_icase(std::string_view a, std::string_view b) noexcept
{
    if (a.size() != b.size())
    {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        if (!char_eq_icase(a[i], b[i]))
        {
            return false;
        }
    }
    return true;
}

} // namespace detail

// Convert string to enum case-insensitively (requires EnumStringPolicy specialization)
template <named_enum E>
std::optional<E> from_string_icase(std::string_view str) noexcept
{
    for (std::size_t i = 0; i < EnumSizeTrait<E>::size; ++i)
    {
        E candidate = static_cast<E>(i);
        if (detail::string_eq_icase(str, EnumStringPolicy<E>::to_string(candidate)))
        {
            return candidate;
        }
    }
    return std::nullopt;
}

// Convert string to enum case-insensitively with default value
template <named_enum E>
E from_string_icase_or(std::string_view str, E default_value) noexcept
{
    auto result = from_string_icase<E>(str);
    return result ? *result : default_value;
}

// Entry type for enum_entries
template <typename E>
struct EnumEntry
{
    std::string_view name;
    E value;
};

// Get array of name-value pairs for enum reflection
// Requires EnumStringPolicy<E> specialization
template <named_enum E>
constexpr std::array<EnumEntry<E>, EnumSizeTrait<E>::size> enum_entries() noexcept
{
    std::array<EnumEntry<E>, EnumSizeTrait<E>::size> entries{};
    for (std::size_t i = 0; i < EnumSizeTrait<E>::size; ++i)
    {
        E val = static_cast<E>(i);
        entries[i] = {EnumStringPolicy<E>::to_string(val), val};
    }
    return entries;
}

// Check if enum has flag(s) set
template <typename E>
constexpr bool has_flag(E value, E flag) noexcept
{
    using U = std::underlying_type_t<E>;
    return (static_cast<U>(value) & static_cast<U>(flag)) == static_cast<U>(flag);
}

template <typename E>
constexpr bool has_flag(EnumPlusWrapper<E> value, E flag) noexcept
{
    return (value.underlying() & to_underlying(flag)) == to_underlying(flag);
}

template <typename E>
constexpr bool has_flag(EnumPlusWrapper<E> value, EnumPlusWrapper<E> flag) noexcept
{
    return (value.underlying() & flag.underlying()) == flag.underlying();
}

} // namespace fat_p
