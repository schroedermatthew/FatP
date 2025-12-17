// DebugOnly.h
// Zero-overhead debug-only storage for C++17/20
//
// Stores values in debug builds, compiles to nothing in release.
// Use for debug labels, performance counters, creation tracking, and invariant checks.

#ifndef FATP_DEBUG_ONLY_H
#define FATP_DEBUG_ONLY_H

#include "CppStandardDetection.h"

#include <functional>
#include <ostream>
#include <type_traits>
#include <utility>

// ============================================================================
// Helper macro for [[no_unique_address]] attribute (C++20)
// ============================================================================
#if FATP_HAS_CPP20
    #define FATP_NO_UNIQUE_ADDRESS [[no_unique_address]]
#else
    #define FATP_NO_UNIQUE_ADDRESS
#endif

namespace fat_p {

// Forward declaration
template <typename T>
struct DebugOnly;

namespace detail {

// Helper to detect if T is streamable
template <typename T, typename = void>
struct is_streamable : std::false_type {};

template <typename T>
struct is_streamable<T, std::void_t<decltype(std::declval<std::ostream&>() << std::declval<T>())>>
    : std::true_type {};

template <typename T>
inline constexpr bool is_streamable_v = is_streamable<T>::value;

// Helper to detect if T is hashable
template <typename T, typename = void>
struct is_hashable : std::false_type {};

template <typename T>
struct is_hashable<T, std::void_t<decltype(std::hash<T>{}(std::declval<T>()))>>
    : std::true_type {};

template <typename T>
inline constexpr bool is_hashable_v = is_hashable<T>::value;

// Helper to detect if T is equality comparable
template <typename T, typename = void>
struct is_equality_comparable : std::false_type {};

template <typename T>
struct is_equality_comparable<T, std::void_t<decltype(std::declval<T>() == std::declval<T>())>>
    : std::true_type {};

template <typename T>
inline constexpr bool is_equality_comparable_v = is_equality_comparable<T>::value;

// Helper to detect if T is less-than comparable
template <typename T, typename = void>
struct is_less_comparable : std::false_type {};

template <typename T>
struct is_less_comparable<T, std::void_t<decltype(std::declval<T>() < std::declval<T>())>>
    : std::true_type {};

template <typename T>
inline constexpr bool is_less_comparable_v = is_less_comparable<T>::value;

} // namespace detail


// ============================================================================
// DebugOnly<T> - Debug Build Implementation (NDEBUG not defined)
// ============================================================================

#ifndef NDEBUG

template <typename T>
struct DebugOnly
{
    // Type aliases for generic code
    using value_type = T;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;

    // Compile-time query: is storage active?
    static constexpr bool is_active = true;

private:
    T value_;

public:
    // Default constructor - value-initializes T
    constexpr DebugOnly() noexcept(std::is_nothrow_default_constructible_v<T>)
        : value_()
    {}

    // Value constructors
    constexpr DebugOnly(const T& val) noexcept(std::is_nothrow_copy_constructible_v<T>)
        : value_(val)
    {}

    constexpr DebugOnly(T&& val) noexcept(std::is_nothrow_move_constructible_v<T>)
        : value_(std::move(val))
    {}

    // In-place construction
    template <typename... Args>
    constexpr explicit DebugOnly(std::in_place_t, Args&&... args)
        noexcept(std::is_nothrow_constructible_v<T, Args...>)
        : value_(std::forward<Args>(args)...)
    {}

    // Copy/move constructors
    constexpr DebugOnly(const DebugOnly& other)
        noexcept(std::is_nothrow_copy_constructible_v<T>)
        : value_(other.value_)
    {}

    constexpr DebugOnly(DebugOnly&& other)
        noexcept(std::is_nothrow_move_constructible_v<T>)
        : value_(std::move(other.value_))
    {}

    // Assignment operators
    constexpr DebugOnly& operator=(const T& val)
        noexcept(std::is_nothrow_copy_assignable_v<T>)
    {
        value_ = val;
        return *this;
    }

    constexpr DebugOnly& operator=(T&& val)
        noexcept(std::is_nothrow_move_assignable_v<T>)
    {
        value_ = std::move(val);
        return *this;
    }

    constexpr DebugOnly& operator=(const DebugOnly& other)
        noexcept(std::is_nothrow_copy_assignable_v<T>)
    {
        if (this != &other)
        {
            value_ = other.value_;
        }
        return *this;
    }

    constexpr DebugOnly& operator=(DebugOnly&& other)
        noexcept(std::is_nothrow_move_assignable_v<T>)
    {
        if (this != &other)
        {
            value_ = std::move(other.value_);
        }
        return *this;
    }

    ~DebugOnly() = default;

    // ========================================================================
    // Accessors (Debug Mode Only)
    // These do not exist in release mode. Use if_debug() or value_or()
    // for cross-mode compatible code.
    // ========================================================================

    [[nodiscard]] constexpr T& get() & noexcept { return value_; }
    [[nodiscard]] constexpr const T& get() const& noexcept { return value_; }
    [[nodiscard]] constexpr T&& get() && noexcept { return std::move(value_); }
    [[nodiscard]] constexpr const T&& get() const&& noexcept { return std::move(value_); }

    [[nodiscard]] constexpr T* operator->() noexcept { return &value_; }
    [[nodiscard]] constexpr const T* operator->() const noexcept { return &value_; }

    [[nodiscard]] constexpr T& operator*() & noexcept { return value_; }
    [[nodiscard]] constexpr const T& operator*() const& noexcept { return value_; }
    [[nodiscard]] constexpr T&& operator*() && noexcept { return std::move(value_); }
    [[nodiscard]] constexpr const T&& operator*() const&& noexcept { return std::move(value_); }

    // Implicit conversion to T& (convenient but be aware of accidental copies)
    operator T&() & noexcept { return value_; }
    operator const T&() const& noexcept { return value_; }

    // Raw pointer access
    [[nodiscard]] constexpr T* data() noexcept { return &value_; }
    [[nodiscard]] constexpr const T* data() const noexcept { return &value_; }

    // ========================================================================
    // Modifiers
    // ========================================================================

    // In-place construction of new value
    template <typename... Args>
    constexpr T& emplace(Args&&... args)
        noexcept(std::is_nothrow_constructible_v<T, Args...>)
    {
        value_.~T();
        new (&value_) T(std::forward<Args>(args)...);
        return value_;
    }

    // Reset to default value
    constexpr void reset() noexcept(std::is_nothrow_default_constructible_v<T>)
    {
        value_ = T();
    }

    // Swap
    constexpr void swap(DebugOnly& other)
        noexcept(std::is_nothrow_swappable_v<T>)
    {
        using std::swap;
        swap(value_, other.value_);
    }

    // ========================================================================
    // Conditional Execution (cross-mode safe)
    // ========================================================================

    // Execute function with value if in debug mode
    // Note: Return value should be used or intentionally discarded
    template <typename Func>
    [[nodiscard]] constexpr decltype(auto) if_debug(Func&& func)
    {
        return std::forward<Func>(func)(value_);
    }

    template <typename Func>
    [[nodiscard]] constexpr decltype(auto) if_debug(Func&& func) const
    {
        return std::forward<Func>(func)(value_);
    }

    // Modify value in-place with function
    template <typename Func>
    constexpr void modify(Func&& func)
    {
        std::forward<Func>(func)(value_);
    }

    // Get value or default (for cross-mode compatibility)
    // Note: In debug mode, default_value is not evaluated to avoid side effects
    template <typename U>
    [[nodiscard]] constexpr T value_or(U&&) const&
        noexcept(std::is_nothrow_copy_constructible_v<T>)
    {
        return value_;
    }

    template <typename U>
    [[nodiscard]] constexpr T value_or(U&&) &&
        noexcept(std::is_nothrow_move_constructible_v<T>)
    {
        return std::move(value_);
    }

    // ========================================================================
    // Comparison Operators
    // Note: Comparisons with DebugOnly<T> work in both modes
    // Comparisons with raw T are deleted in release to prevent control flow bugs
    // ========================================================================

    template <typename U = T>
    [[nodiscard]] constexpr auto operator==(const DebugOnly& other) const
        noexcept(noexcept(std::declval<U>() == std::declval<U>()))
        -> std::enable_if_t<detail::is_equality_comparable_v<U>, bool>
    {
        return value_ == other.value_;
    }

    template <typename U = T>
    [[nodiscard]] constexpr auto operator!=(const DebugOnly& other) const
        noexcept(noexcept(std::declval<U>() == std::declval<U>()))
        -> std::enable_if_t<detail::is_equality_comparable_v<U>, bool>
    {
        return value_ != other.value_;
    }

    template <typename U = T>
    [[nodiscard]] constexpr auto operator<(const DebugOnly& other) const
        noexcept(noexcept(std::declval<U>() < std::declval<U>()))
        -> std::enable_if_t<detail::is_less_comparable_v<U>, bool>
    {
        return value_ < other.value_;
    }

    template <typename U = T>
    [[nodiscard]] constexpr auto operator<=(const DebugOnly& other) const
        noexcept(noexcept(std::declval<U>() < std::declval<U>()))
        -> std::enable_if_t<detail::is_less_comparable_v<U>, bool>
    {
        return value_ <= other.value_;
    }

    template <typename U = T>
    [[nodiscard]] constexpr auto operator>(const DebugOnly& other) const
        noexcept(noexcept(std::declval<U>() < std::declval<U>()))
        -> std::enable_if_t<detail::is_less_comparable_v<U>, bool>
    {
        return value_ > other.value_;
    }

    template <typename U = T>
    [[nodiscard]] constexpr auto operator>=(const DebugOnly& other) const
        noexcept(noexcept(std::declval<U>() < std::declval<U>()))
        -> std::enable_if_t<detail::is_less_comparable_v<U>, bool>
    {
        return value_ >= other.value_;
    }

    // Comparison with raw T (debug mode only - deleted in release)
    template <typename U = T>
    [[nodiscard]] constexpr auto operator==(const T& other) const
        noexcept(noexcept(std::declval<U>() == std::declval<U>()))
        -> std::enable_if_t<detail::is_equality_comparable_v<U>, bool>
    {
        return value_ == other;
    }

    template <typename U = T>
    [[nodiscard]] constexpr auto operator!=(const T& other) const
        noexcept(noexcept(std::declval<U>() == std::declval<U>()))
        -> std::enable_if_t<detail::is_equality_comparable_v<U>, bool>
    {
        return value_ != other;
    }

    // ========================================================================
    // Increment/Decrement (for counters)
    // Only meaningful for integral types; no-op for others.
    // Use if constexpr to avoid compilation errors for non-integral T.
    // ========================================================================

    constexpr DebugOnly& operator++() noexcept
    {
        if constexpr (std::is_integral_v<T>)
        {
            ++value_;
        }
        return *this;
    }

    constexpr T operator++(int) noexcept
    {
        if constexpr (std::is_integral_v<T>)
        {
            return value_++;
        }
        else
        {
            return T{};
        }
    }

    constexpr DebugOnly& operator--() noexcept
    {
        if constexpr (std::is_integral_v<T>)
        {
            --value_;
        }
        return *this;
    }

    constexpr T operator--(int) noexcept
    {
        if constexpr (std::is_integral_v<T>)
        {
            return value_--;
        }
        else
        {
            return T{};
        }
    }

    // Compound assignment for arithmetic types
    template <typename U>
    constexpr DebugOnly& operator+=(const U& rhs) noexcept
    {
        if constexpr (std::is_arithmetic_v<T>)
        {
            value_ += rhs;
        }
        return *this;
    }

    template <typename U>
    constexpr DebugOnly& operator-=(const U& rhs) noexcept
    {
        if constexpr (std::is_arithmetic_v<T>)
        {
            value_ -= rhs;
        }
        return *this;
    }
};

// Stream output (debug mode)
template <typename T>
auto operator<<(std::ostream& os, const DebugOnly<T>& val)
    -> std::enable_if_t<detail::is_streamable_v<T>, std::ostream&>
{
    return os << val.get();
}

// Free swap (debug mode)
template <typename T>
constexpr void swap(DebugOnly<T>& a, DebugOnly<T>& b)
    noexcept(noexcept(a.swap(b)))
{
    a.swap(b);
}

#else // NDEBUG defined

// ============================================================================
// DebugOnly<T> - Release Build Implementation (NDEBUG defined)
// ============================================================================

template <typename T>
struct DebugOnly
{
    // Type aliases for generic code
    using value_type = T;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;

    // Compile-time query: is storage active?
    static constexpr bool is_active = false;

    // All constructors are no-ops
    constexpr DebugOnly() noexcept = default;
    constexpr DebugOnly(const T&) noexcept {}
    constexpr DebugOnly(T&&) noexcept {}

    template <typename... Args>
    constexpr explicit DebugOnly(std::in_place_t, Args&&...) noexcept {}

    constexpr DebugOnly(const DebugOnly&) noexcept = default;
    constexpr DebugOnly(DebugOnly&&) noexcept = default;

    // All assignments are no-ops
    constexpr DebugOnly& operator=(const T&) noexcept { return *this; }
    constexpr DebugOnly& operator=(T&&) noexcept { return *this; }
    constexpr DebugOnly& operator=(const DebugOnly&) noexcept = default;
    constexpr DebugOnly& operator=(DebugOnly&&) noexcept = default;

    ~DebugOnly() = default;

    // ========================================================================
    // Stub Accessors (for template compilation)
    // These exist so that template code that calls get() will compile,
    // but the calls should be guarded with #ifndef NDEBUG or if constexpr.
    // In optimized builds, these will be eliminated if not called.
    // ========================================================================

    // Modifiers (all no-ops)
    template <typename... Args>
    constexpr void emplace(Args&&...) noexcept {}

    constexpr void reset() noexcept {}

    constexpr void swap(DebugOnly&) noexcept {}

    // ========================================================================
    // Conditional Execution (release: does nothing)
    // ========================================================================

    // Execute function - does nothing in release
    template <typename Func>
    constexpr void if_debug(Func&&) const noexcept {}

    // Modify - does nothing in release
    template <typename Func>
    constexpr void modify(Func&&) const noexcept {}

    // Get value or default - always returns default in release
    template <typename U>
    [[nodiscard]] constexpr T value_or(U&& default_value) const
        noexcept(std::is_nothrow_constructible_v<T, U>)
    {
        return T(std::forward<U>(default_value));
    }

    // ========================================================================
    // Comparison Operators
    // DebugOnly-to-DebugOnly comparisons are no-ops (always equal)
    // DebugOnly-to-T comparisons are DELETED to prevent control flow bugs
    // ========================================================================

    [[nodiscard]] constexpr bool operator==(const DebugOnly&) const noexcept { return true; }
    [[nodiscard]] constexpr bool operator!=(const DebugOnly&) const noexcept { return false; }
    [[nodiscard]] constexpr bool operator<(const DebugOnly&) const noexcept { return false; }
    [[nodiscard]] constexpr bool operator<=(const DebugOnly&) const noexcept { return true; }
    [[nodiscard]] constexpr bool operator>(const DebugOnly&) const noexcept { return false; }
    [[nodiscard]] constexpr bool operator>=(const DebugOnly&) const noexcept { return true; }

    // CRITICAL: Comparisons with raw T are DELETED in release mode
    // This prevents silent control flow changes between debug and release:
    //   if (debug_val == 5) { ... }  // Would ALWAYS be true in release!
    // Use value_or() or if_debug() instead for cross-mode safe code.
    [[nodiscard]] bool operator==(const T&) const = delete;
    [[nodiscard]] bool operator!=(const T&) const = delete;

    // ========================================================================
    // Increment/Decrement (no-ops in release)
    // Use if constexpr to match debug mode signature
    // ========================================================================

    constexpr DebugOnly& operator++() noexcept
    {
        return *this;
    }

    constexpr T operator++(int) noexcept
    {
        if constexpr (std::is_default_constructible_v<T>)
        {
            return T{};
        }
        else
        {
            // For non-default-constructible types, this branch is never taken
            // in well-formed code (increment only makes sense for integral types)
            return *reinterpret_cast<T*>(nullptr);
        }
    }

    constexpr DebugOnly& operator--() noexcept
    {
        return *this;
    }

    constexpr T operator--(int) noexcept
    {
        if constexpr (std::is_default_constructible_v<T>)
        {
            return T{};
        }
        else
        {
            return *reinterpret_cast<T*>(nullptr);
        }
    }

    template <typename U>
    constexpr DebugOnly& operator+=(const U&) noexcept
    {
        return *this;
    }

    template <typename U>
    constexpr DebugOnly& operator-=(const U&) noexcept
    {
        return *this;
    }
};

// Stream output (release mode - outputs nothing)
template <typename T>
std::ostream& operator<<(std::ostream& os, const DebugOnly<T>&)
{
    return os;
}

// Free swap (release mode - no-op)
template <typename T>
constexpr void swap(DebugOnly<T>&, DebugOnly<T>&) noexcept {}

#endif // NDEBUG


// ============================================================================
// Helper Macros for Safe Cross-Mode Access
// ============================================================================

// Execute code only in debug mode
#ifndef NDEBUG
    #define DEBUG_ONLY_EXEC(code) do { code; } while(0)
#else
    #define DEBUG_ONLY_EXEC(code) do { (void)0; } while(0)
#endif

// Increment a debug counter
#ifndef NDEBUG
    #define DEBUG_ONLY_INCREMENT(counter) ++(counter)
#else
    #define DEBUG_ONLY_INCREMENT(counter) ((void)0)
#endif

// Log with a debug value
#ifndef NDEBUG
    #define DEBUG_ONLY_LOG(stream, val) ((stream) << (val).get())
#else
    #define DEBUG_ONLY_LOG(stream, val) ((void)0)
#endif


// ============================================================================
// std::hash specialization
// ============================================================================

} // namespace fat_p

namespace std {

template <typename T>
struct hash<fat_p::DebugOnly<T>>
{
    constexpr size_t operator()(const fat_p::DebugOnly<T>& val) const
        noexcept(noexcept(std::hash<T>{}(std::declval<T>())))
    {
#ifndef NDEBUG
        if constexpr (fat_p::detail::is_hashable_v<T>)
        {
            return std::hash<T>{}(val.get());
        }
        else
        {
            return 0;
        }
#else
        (void)val;
        return 0;  // All release DebugOnly hash to same value
#endif
    }
};

} // namespace std

#endif // FATP_DEBUG_ONLY_H
