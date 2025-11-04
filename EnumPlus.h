#ifndef CPP_UTILITIES_ENUM_PLUS_H
#define CPP_UTILITIES_ENUM_PLUS_H

#include <array>
#include <type_traits>
#include <string>
#include <string_view>
#include <stdexcept>
#include <ostream>
#include <algorithm>
#include <functional>

namespace cpp_utilities {

// ============================================================================
// Forward Declarations
// ============================================================================

template<typename E>
struct EnumSizeTrait;

template<typename E, bool Enable = false>
struct EnableOverloadedOperators;

template<typename E>
struct EnumStringPolicy;

// ============================================================================
// Bounds Check Policies
// ============================================================================

struct DefaultBoundsCheckPolicy {
    template<typename E>
    static void check_bounds(std::size_t index, std::size_t size) {
        if (index >= size) {
            throw std::out_of_range("EnumPlusMap index out of bounds");
        }
    }
};

struct NoBoundsCheckPolicy {
    template<typename E>
    static void check_bounds(std::size_t, std::size_t) noexcept {}
};

// ============================================================================
// EnumPlusWrapper - Wraps an enum to enable operator overloading
// ============================================================================

template<typename E>
class EnumPlusWrapper {
    static_assert(std::is_enum_v<E>, "E must be an enum type");
    
public:
    using enum_type = E;
    using underlying_type = std::underlying_type_t<E>;
    
    constexpr EnumPlusWrapper() noexcept : value_(static_cast<E>(0)) {}
    constexpr explicit EnumPlusWrapper(E value) noexcept : value_(value) {}
    constexpr explicit EnumPlusWrapper(underlying_type value) noexcept 
        : value_(static_cast<E>(value)) {}
    
    constexpr E value() const noexcept { return value_; }
    constexpr underlying_type underlying() const noexcept { 
        return static_cast<underlying_type>(value_); 
    }
    
    constexpr operator E() const noexcept { return value_; }
    
    constexpr bool operator==(const EnumPlusWrapper& other) const noexcept {
        return value_ == other.value_;
    }
    
    constexpr bool operator!=(const EnumPlusWrapper& other) const noexcept {
        return value_ != other.value_;
    }
    
    constexpr bool operator==(E other) const noexcept {
        return value_ == other;
    }
    
    constexpr bool operator!=(E other) const noexcept {
        return value_ != other;
    }
    
private:
    E value_;
};

// ============================================================================
// Bitwise Operators (enabled via EnableOverloadedOperators)
// ============================================================================

template<typename E>
constexpr auto operator|(EnumPlusWrapper<E> lhs, EnumPlusWrapper<E> rhs) noexcept
    -> std::enable_if_t<EnableOverloadedOperators<E>::value, EnumPlusWrapper<E>> {
    return EnumPlusWrapper<E>(lhs.underlying() | rhs.underlying());
}

template<typename E>
constexpr auto operator|(E lhs, E rhs) noexcept
    -> std::enable_if_t<EnableOverloadedOperators<E>::value, E> {
    using U = std::underlying_type_t<E>;
    return static_cast<E>(static_cast<U>(lhs) | static_cast<U>(rhs));
}

template<typename E>
constexpr auto operator|(EnumPlusWrapper<E> lhs, E rhs) noexcept
    -> std::enable_if_t<EnableOverloadedOperators<E>::value, EnumPlusWrapper<E>> {
    return EnumPlusWrapper<E>(lhs.underlying() | static_cast<std::underlying_type_t<E>>(rhs));
}

template<typename E>
constexpr auto operator|(E lhs, EnumPlusWrapper<E> rhs) noexcept
    -> std::enable_if_t<EnableOverloadedOperators<E>::value, EnumPlusWrapper<E>> {
    return EnumPlusWrapper<E>(static_cast<std::underlying_type_t<E>>(lhs) | rhs.underlying());
}

template<typename E>
constexpr auto operator&(EnumPlusWrapper<E> lhs, EnumPlusWrapper<E> rhs) noexcept
    -> std::enable_if_t<EnableOverloadedOperators<E>::value, EnumPlusWrapper<E>> {
    return EnumPlusWrapper<E>(lhs.underlying() & rhs.underlying());
}

template<typename E>
constexpr auto operator&(E lhs, E rhs) noexcept
    -> std::enable_if_t<EnableOverloadedOperators<E>::value, E> {
    using U = std::underlying_type_t<E>;
    return static_cast<E>(static_cast<U>(lhs) & static_cast<U>(rhs));
}

template<typename E>
constexpr auto operator&(EnumPlusWrapper<E> lhs, E rhs) noexcept
    -> std::enable_if_t<EnableOverloadedOperators<E>::value, EnumPlusWrapper<E>> {
    return EnumPlusWrapper<E>(lhs.underlying() & static_cast<std::underlying_type_t<E>>(rhs));
}

template<typename E>
constexpr auto operator&(E lhs, EnumPlusWrapper<E> rhs) noexcept
    -> std::enable_if_t<EnableOverloadedOperators<E>::value, EnumPlusWrapper<E>> {
    return EnumPlusWrapper<E>(static_cast<std::underlying_type_t<E>>(lhs) & rhs.underlying());
}

template<typename E>
constexpr auto operator^(EnumPlusWrapper<E> lhs, EnumPlusWrapper<E> rhs) noexcept
    -> std::enable_if_t<EnableOverloadedOperators<E>::value, EnumPlusWrapper<E>> {
    return EnumPlusWrapper<E>(lhs.underlying() ^ rhs.underlying());
}

template<typename E>
constexpr auto operator^(E lhs, E rhs) noexcept
    -> std::enable_if_t<EnableOverloadedOperators<E>::value, E> {
    using U = std::underlying_type_t<E>;
    return static_cast<E>(static_cast<U>(lhs) ^ static_cast<U>(rhs));
}

template<typename E>
constexpr auto operator^(EnumPlusWrapper<E> lhs, E rhs) noexcept
    -> std::enable_if_t<EnableOverloadedOperators<E>::value, EnumPlusWrapper<E>> {
    return EnumPlusWrapper<E>(lhs.underlying() ^ static_cast<std::underlying_type_t<E>>(rhs));
}

template<typename E>
constexpr auto operator^(E lhs, EnumPlusWrapper<E> rhs) noexcept
    -> std::enable_if_t<EnableOverloadedOperators<E>::value, EnumPlusWrapper<E>> {
    return EnumPlusWrapper<E>(static_cast<std::underlying_type_t<E>>(lhs) ^ rhs.underlying());
}

template<typename E>
constexpr auto operator~(EnumPlusWrapper<E> value) noexcept
    -> std::enable_if_t<EnableOverloadedOperators<E>::value, EnumPlusWrapper<E>> {
    return EnumPlusWrapper<E>(~value.underlying());
}

template<typename E>
constexpr auto operator~(E value) noexcept
    -> std::enable_if_t<EnableOverloadedOperators<E>::value, E> {
    using U = std::underlying_type_t<E>;
    return static_cast<E>(~static_cast<U>(value));
}

// Compound assignment operators
template<typename E>
auto operator|=(EnumPlusWrapper<E>& lhs, EnumPlusWrapper<E> rhs) noexcept
    -> std::enable_if_t<EnableOverloadedOperators<E>::value, EnumPlusWrapper<E>&> {
    lhs = lhs | rhs;
    return lhs;
}

template<typename E>
auto operator|=(E& lhs, E rhs) noexcept
    -> std::enable_if_t<EnableOverloadedOperators<E>::value, E&> {
    lhs = lhs | rhs;
    return lhs;
}

template<typename E>
auto operator|=(EnumPlusWrapper<E>& lhs, E rhs) noexcept
    -> std::enable_if_t<EnableOverloadedOperators<E>::value, EnumPlusWrapper<E>&> {
    lhs = lhs | rhs;
    return lhs;
}

template<typename E>
auto operator&=(EnumPlusWrapper<E>& lhs, EnumPlusWrapper<E> rhs) noexcept
    -> std::enable_if_t<EnableOverloadedOperators<E>::value, EnumPlusWrapper<E>&> {
    lhs = lhs & rhs;
    return lhs;
}

template<typename E>
auto operator&=(E& lhs, E rhs) noexcept
    -> std::enable_if_t<EnableOverloadedOperators<E>::value, E&> {
    lhs = lhs & rhs;
    return lhs;
}

template<typename E>
auto operator&=(EnumPlusWrapper<E>& lhs, E rhs) noexcept
    -> std::enable_if_t<EnableOverloadedOperators<E>::value, EnumPlusWrapper<E>&> {
    lhs = lhs & rhs;
    return lhs;
}

template<typename E>
auto operator^=(EnumPlusWrapper<E>& lhs, EnumPlusWrapper<E> rhs) noexcept
    -> std::enable_if_t<EnableOverloadedOperators<E>::value, EnumPlusWrapper<E>&> {
    lhs = lhs ^ rhs;
    return lhs;
}

template<typename E>
auto operator^=(E& lhs, E rhs) noexcept
    -> std::enable_if_t<EnableOverloadedOperators<E>::value, E&> {
    lhs = lhs ^ rhs;
    return lhs;
}

template<typename E>
auto operator^=(EnumPlusWrapper<E>& lhs, E rhs) noexcept
    -> std::enable_if_t<EnableOverloadedOperators<E>::value, EnumPlusWrapper<E>&> {
    lhs = lhs ^ rhs;
    return lhs;
}

// ============================================================================
// Stream Operators
// ============================================================================

template<typename E>
auto operator<<(std::ostream& os, E value)
    -> std::enable_if_t<std::is_enum_v<E> && EnumStringPolicy<E>::has_names, std::ostream&> {
    os << EnumStringPolicy<E>::to_string(value);
    return os;
}

template<typename E>
std::ostream& operator<<(std::ostream& os, EnumPlusWrapper<E> value) {
    if constexpr (EnumStringPolicy<E>::has_names) {
        os << EnumStringPolicy<E>::to_string(value.value());
    } else {
        os << static_cast<std::underlying_type_t<E>>(value.value());
    }
    return os;
}

// ============================================================================
// EnumPlusMap - Type-safe array-based mapping from enum to values
// ============================================================================

template<typename E, typename T, typename BoundsPolicy = DefaultBoundsCheckPolicy>
class EnumPlusMap {
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
    constexpr EnumPlusMap() : data_{} {}
    
    // Constructor from initializer list (variadic)
    // Disabled when:
    // 1. Single argument of type T or convertible to T (use fill constructor)
    // 2. Single callable argument (use generator constructor)
    template<typename Arg1, typename... Args,
             std::enable_if_t<
                 (sizeof...(Args) > 0) ||  // Multiple args: always enabled
                 (!std::is_convertible_v<Arg1, T> &&  // Single arg: not convertible to T
                  !std::is_invocable_r_v<T, Arg1, E>), // and not a generator function
                 int> = 0>
    constexpr EnumPlusMap(Arg1&& arg1, Args&&... args) 
        : data_{std::forward<Arg1>(arg1), std::forward<Args>(args)...} {}
    
    // Constructor with fill value
    constexpr explicit EnumPlusMap(const T& fill_value) {
        data_.fill(fill_value);
    }
    
    // Constructor with generator function
    template<typename Func>
    constexpr explicit EnumPlusMap(Func&& generator,
        std::enable_if_t<std::is_invocable_r_v<T, Func, E>>* = nullptr) {
        for (size_type i = 0; i < enum_size; ++i) {
            data_[i] = generator(static_cast<E>(i));
        }
    }
    
    // Element access
    constexpr reference operator[](E key) noexcept {
        return data_[to_index(key)];
    }
    
    constexpr const_reference operator[](E key) const noexcept {
        return data_[to_index(key)];
    }
    
    constexpr reference at(E key) {
        const auto idx = to_index(key);
        BoundsPolicy::template check_bounds<E>(idx, enum_size);
        return data_[idx];
    }
    
    constexpr const_reference at(E key) const {
        const auto idx = to_index(key);
        BoundsPolicy::template check_bounds<E>(idx, enum_size);
        return data_[idx];
    }
    
    // Capacity
    constexpr size_type size() const noexcept { return enum_size; }
    constexpr bool empty() const noexcept { return enum_size == 0; }
    
    // Iterators
    constexpr iterator begin() noexcept { return data_.begin(); }
    constexpr const_iterator begin() const noexcept { return data_.begin(); }
    constexpr const_iterator cbegin() const noexcept { return data_.cbegin(); }
    
    constexpr iterator end() noexcept { return data_.end(); }
    constexpr const_iterator end() const noexcept { return data_.end(); }
    constexpr const_iterator cend() const noexcept { return data_.cend(); }
    
    // Fill
    constexpr void fill(const T& value) {
        data_.fill(value);
    }
    
    // Apply function to all elements
    template<typename Func>
    constexpr void for_each(Func&& func) {
        for (size_type i = 0; i < enum_size; ++i) {
            func(data_[i]);
        }
    }
    
    template<typename Func>
    constexpr void for_each(Func&& func) const {
        for (size_type i = 0; i < enum_size; ++i) {
            func(data_[i]);
        }
    }
    
    // Data access
    constexpr T* data() noexcept { return data_.data(); }
    constexpr const T* data() const noexcept { return data_.data(); }
    
private:
    std::array<T, enum_size> data_;
    
    static constexpr size_type to_index(E key) noexcept {
        return static_cast<size_type>(static_cast<std::underlying_type_t<E>>(key));
    }
};

// ============================================================================
// Utility Functions
// ============================================================================

// Convert enum to underlying type
template<typename E>
constexpr auto to_underlying(E value) noexcept
    -> std::enable_if_t<std::is_enum_v<E>, std::underlying_type_t<E>> {
    return static_cast<std::underlying_type_t<E>>(value);
}

// Convert enum to string (requires EnumStringPolicy specialization)
template<typename E>
std::string_view to_string(E value) {
    static_assert(EnumStringPolicy<E>::has_names, 
                  "EnumStringPolicy<E> must be specialized with names");
    return EnumStringPolicy<E>::to_string(value);
}

// Convert string to enum (requires EnumStringPolicy specialization)
template<typename E>
E from_string(std::string_view str) {
    static_assert(EnumStringPolicy<E>::has_names, 
                  "EnumStringPolicy<E> must be specialized with names");
    return EnumStringPolicy<E>::from_string(str);
}

// Check if enum has flag(s) set
template<typename E>
constexpr bool has_flag(E value, E flag) noexcept {
    using U = std::underlying_type_t<E>;
    return (static_cast<U>(value) & static_cast<U>(flag)) == static_cast<U>(flag);
}

template<typename E>
constexpr bool has_flag(EnumPlusWrapper<E> value, E flag) noexcept {
    return (value.underlying() & to_underlying(flag)) == to_underlying(flag);
}

template<typename E>
constexpr bool has_flag(EnumPlusWrapper<E> value, EnumPlusWrapper<E> flag) noexcept {
    return (value.underlying() & flag.underlying()) == flag.underlying();
}

// Get all enum values
template<typename E>
constexpr auto enum_values() noexcept 
    -> std::enable_if_t<std::is_enum_v<E>, std::array<E, EnumSizeTrait<E>::size>> {
    std::array<E, EnumSizeTrait<E>::size> values{};
    for (std::size_t i = 0; i < EnumSizeTrait<E>::size; ++i) {
        values[i] = static_cast<E>(i);
    }
    return values;
}

} // namespace cpp_utilities

#endif // CPP_UTILITIES_ENUM_PLUS_H
