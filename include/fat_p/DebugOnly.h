#pragma once

/*
FATP_META:
  meta_version: 1
  component: DebugOnly
  file_role: public_header
  path: include/fat_p/DebugOnly.h
  namespace: fat_p
  layer: Foundation
  summary: "Public header for DebugOnly."
  api_stability: in_work
  related:
    docs_search: "DebugOnly"
    tests:
      - components/DebugOnly/tests/test_DebugOnly.cpp
  hygiene:
    pragma_once: false
    include_guard: true
    defines_total: 7
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/

/**
 * @file DebugOnly.h
 * @brief Debug-only utilities that compile to nothing in release builds
 *
 * @details
 * Zero-overhead debug-only storage for C++20
 * Stores values in debug builds, compiles to nothing in release.
 * Use for debug labels, performance counters, creation tracking, and invariant checks.
 */

#include "Concepts.h"
#include "FatPConfig.h"

#include <functional>
#include <ostream>
#include <type_traits>
#include <utility>

namespace fat_p
{

// Forward declaration
template <typename T>
struct DebugOnly;

// NOTE: Type traits (streamable, hashable, equality_comparable, totally_ordered)
// are provided by Concepts.h. DebugOnly uses fat_p::concepts::* from that header
// to avoid duplication and ensure composability per Systemic Hygiene Policy.

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
    T mValue;

public:
    // Default constructor - value-initializes T
    constexpr DebugOnly() noexcept(std::is_nothrow_default_constructible_v<T>)
        : mValue()
    {
    }

    // Value constructors
    constexpr DebugOnly(const T& val) noexcept(std::is_nothrow_copy_constructible_v<T>)
        : mValue(val)
    {
    }

    constexpr DebugOnly(T&& val) noexcept(std::is_nothrow_move_constructible_v<T>)
        : mValue(std::move(val))
    {
    }

    // In-place construction
    template <typename... Args>
    constexpr explicit DebugOnly(std::in_place_t, Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
        : mValue(std::forward<Args>(args)...)
    {
    }

    // Copy/move constructors
    constexpr DebugOnly(const DebugOnly& other) noexcept(std::is_nothrow_copy_constructible_v<T>)
        : mValue(other.mValue)
    {
    }

    constexpr DebugOnly(DebugOnly&& other) noexcept(std::is_nothrow_move_constructible_v<T>)
        : mValue(std::move(other.mValue))
    {
    }

    // Assignment operators
    constexpr DebugOnly& operator=(const T& val) noexcept(std::is_nothrow_copy_assignable_v<T>)
    {
        mValue = val;
        return *this;
    }

    constexpr DebugOnly& operator=(T&& val) noexcept(std::is_nothrow_move_assignable_v<T>)
    {
        mValue = std::move(val);
        return *this;
    }

    constexpr DebugOnly& operator=(const DebugOnly& other) noexcept(std::is_nothrow_copy_assignable_v<T>)
    {
        if (this != &other)
        {
            mValue = other.mValue;
        }
        return *this;
    }

    constexpr DebugOnly& operator=(DebugOnly&& other) noexcept(std::is_nothrow_move_assignable_v<T>)
    {
        if (this != &other)
        {
            mValue = std::move(other.mValue);
        }
        return *this;
    }

    ~DebugOnly() = default;

    // ========================================================================
    // Accessors (Debug Mode Only)
    // These do not exist in release mode. Use if_debug() or value_or()
    // for cross-mode compatible code.
    // ========================================================================

    [[nodiscard]] constexpr T& get() & noexcept
    {
        return mValue;
    }
    [[nodiscard]] constexpr const T& get() const& noexcept
    {
        return mValue;
    }
    [[nodiscard]] constexpr T&& get() && noexcept
    {
        return std::move(mValue);
    }
    [[nodiscard]] constexpr const T&& get() const&& noexcept
    {
        return std::move(mValue);
    }

    [[nodiscard]] constexpr T* operator->() noexcept
    {
        return &mValue;
    }
    [[nodiscard]] constexpr const T* operator->() const noexcept
    {
        return &mValue;
    }

    [[nodiscard]] constexpr T& operator*() & noexcept
    {
        return mValue;
    }
    [[nodiscard]] constexpr const T& operator*() const& noexcept
    {
        return mValue;
    }
    [[nodiscard]] constexpr T&& operator*() && noexcept
    {
        return std::move(mValue);
    }
    [[nodiscard]] constexpr const T&& operator*() const&& noexcept
    {
        return std::move(mValue);
    }

    // Implicit conversion to T& (convenient but be aware of accidental copies)
    operator T&() & noexcept
    {
        return mValue;
    }
    operator const T&() const& noexcept
    {
        return mValue;
    }

    // Raw pointer access
    [[nodiscard]] constexpr T* data() noexcept
    {
        return &mValue;
    }
    [[nodiscard]] constexpr const T* data() const noexcept
    {
        return &mValue;
    }

    // ========================================================================
    // Modifiers
    // ========================================================================

    // In-place construction of new value
    template <typename... Args>
    constexpr T& emplace(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
    {
        mValue.~T();
        new (&mValue) T(std::forward<Args>(args)...);
        return mValue;
    }

    // Reset to default value
    constexpr void reset() noexcept(std::is_nothrow_default_constructible_v<T>)
    {
        mValue = T();
    }

    // Swap
    constexpr void swap(DebugOnly& other) noexcept(std::is_nothrow_swappable_v<T>)
    {
        using std::swap;
        swap(mValue, other.mValue);
    }

    // ========================================================================
    // Conditional Execution (cross-mode safe)
    // ========================================================================

    // Execute function with value if in debug mode
    // Note: Return value should be used or intentionally discarded
    template <typename Func>
    [[nodiscard]] constexpr decltype(auto) if_debug(Func&& func)
    {
        return std::forward<Func>(func)(mValue);
    }

    template <typename Func>
    [[nodiscard]] constexpr decltype(auto) if_debug(Func&& func) const
    {
        return std::forward<Func>(func)(mValue);
    }

    // Modify value in-place with function
    template <typename Func>
    constexpr void modify(Func&& func)
    {
        std::forward<Func>(func)(mValue);
    }

    // Get value or default (for cross-mode compatibility)
    // Note: In debug mode, default_value is not evaluated to avoid side effects
    template <typename U>
    [[nodiscard]] constexpr T value_or(U&&) const& noexcept(std::is_nothrow_copy_constructible_v<T>)
    {
        return mValue;
    }

    template <typename U>
    [[nodiscard]] constexpr T value_or(U&&) && noexcept(std::is_nothrow_move_constructible_v<T>)
    {
        return std::move(mValue);
    }

    // ========================================================================
    // Comparison Operators
    // Note: Comparisons with DebugOnly<T> work in both modes
    // Comparisons with raw T are deleted in release to prevent control flow bugs
    // ========================================================================

    template <typename U = T>
        requires concepts::equality_comparable<U>
    [[nodiscard]] constexpr auto operator==(const DebugOnly& other) const
        noexcept(noexcept(std::declval<U>() == std::declval<U>())) -> bool
    {
        return mValue == other.mValue;
    }

    template <typename U = T>
        requires concepts::equality_comparable<U>
    [[nodiscard]] constexpr auto operator!=(const DebugOnly& other) const
        noexcept(noexcept(std::declval<U>() == std::declval<U>())) -> bool
    {
        return mValue != other.mValue;
    }

    template <typename U = T>
        requires concepts::totally_ordered<U>
    [[nodiscard]] constexpr auto operator<(const DebugOnly& other) const
        noexcept(noexcept(std::declval<U>() < std::declval<U>())) -> bool
    {
        return mValue < other.mValue;
    }

    template <typename U = T>
        requires concepts::totally_ordered<U>
    [[nodiscard]] constexpr auto operator<=(const DebugOnly& other) const
        noexcept(noexcept(std::declval<U>() < std::declval<U>())) -> bool
    {
        return mValue <= other.mValue;
    }

    template <typename U = T>
        requires concepts::totally_ordered<U>
    [[nodiscard]] constexpr auto operator>(const DebugOnly& other) const
        noexcept(noexcept(std::declval<U>() < std::declval<U>())) -> bool
    {
        return mValue > other.mValue;
    }

    template <typename U = T>
        requires concepts::totally_ordered<U>
    [[nodiscard]] constexpr auto operator>=(const DebugOnly& other) const
        noexcept(noexcept(std::declval<U>() < std::declval<U>())) -> bool
    {
        return mValue >= other.mValue;
    }

    // Comparison with raw T (debug mode only - deleted in release)
    template <typename U = T>
        requires concepts::equality_comparable<U>
    [[nodiscard]] constexpr auto operator==(const T& other) const
        noexcept(noexcept(std::declval<U>() == std::declval<U>())) -> bool
    {
        return mValue == other;
    }

    template <typename U = T>
        requires concepts::equality_comparable<U>
    [[nodiscard]] constexpr auto operator!=(const T& other) const
        noexcept(noexcept(std::declval<U>() == std::declval<U>())) -> bool
    {
        return mValue != other;
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
            ++mValue;
        }
        return *this;
    }

    constexpr T operator++(int) noexcept
    {
        if constexpr (std::is_integral_v<T>)
        {
            return mValue++;
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
            --mValue;
        }
        return *this;
    }

    constexpr T operator--(int) noexcept
    {
        if constexpr (std::is_integral_v<T>)
        {
            return mValue--;
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
            mValue += rhs;
        }
        return *this;
    }

    template <typename U>
    constexpr DebugOnly& operator-=(const U& rhs) noexcept
    {
        if constexpr (std::is_arithmetic_v<T>)
        {
            mValue -= rhs;
        }
        return *this;
    }
};

// Stream output (debug mode)
template <typename T>
    requires concepts::streamable<T>
auto operator<<(std::ostream& os, const DebugOnly<T>& val) -> std::ostream&
{
    return os << val.get();
}

// Free swap (debug mode)
template <typename T>
constexpr void swap(DebugOnly<T>& a, DebugOnly<T>& b) noexcept(noexcept(a.swap(b)))
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
    constexpr DebugOnly(const T&) noexcept
    {
    }
    constexpr DebugOnly(T&&) noexcept
    {
    }

    template <typename... Args>
    constexpr explicit DebugOnly(std::in_place_t, Args&&...) noexcept
    {
    }

    constexpr DebugOnly(const DebugOnly&) noexcept = default;
    constexpr DebugOnly(DebugOnly&&) noexcept = default;

    // All assignments are no-ops
    constexpr DebugOnly& operator=(const T&) noexcept
    {
        return *this;
    }
    constexpr DebugOnly& operator=(T&&) noexcept
    {
        return *this;
    }
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
    constexpr void emplace(Args&&...) noexcept
    {
    }

    constexpr void reset() noexcept
    {
    }

    constexpr void swap(DebugOnly&) noexcept
    {
    }

    // ========================================================================
    // Conditional Execution (release: does nothing)
    // ========================================================================

    // Execute function - does nothing in release
    template <typename Func>
    constexpr void if_debug(Func&&) const noexcept
    {
    }

    // Modify - does nothing in release
    template <typename Func>
    constexpr void modify(Func&&) const noexcept
    {
    }

    // Get value or default - always returns default in release
    template <typename U>
    [[nodiscard]] constexpr T value_or(U&& default_value) const noexcept(std::is_nothrow_constructible_v<T, U>)
    {
        return T(std::forward<U>(default_value));
    }

    // ========================================================================
    // Comparison Operators
    // DebugOnly-to-DebugOnly comparisons are no-ops (always equal)
    // DebugOnly-to-T comparisons are DELETED to prevent control flow bugs
    // ========================================================================

    [[nodiscard]] constexpr bool operator==(const DebugOnly&) const noexcept
    {
        return true;
    }
    [[nodiscard]] constexpr bool operator!=(const DebugOnly&) const noexcept
    {
        return false;
    }
    [[nodiscard]] constexpr bool operator<(const DebugOnly&) const noexcept
    {
        return false;
    }
    [[nodiscard]] constexpr bool operator<=(const DebugOnly&) const noexcept
    {
        return true;
    }
    [[nodiscard]] constexpr bool operator>(const DebugOnly&) const noexcept
    {
        return false;
    }
    [[nodiscard]] constexpr bool operator>=(const DebugOnly&) const noexcept
    {
        return true;
    }

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
constexpr void swap(DebugOnly<T>&, DebugOnly<T>&) noexcept
{
}

#endif // NDEBUG

// ============================================================================
// Helper Macros for Safe Cross-Mode Access
// ============================================================================

// Execute code only in debug mode
#ifndef NDEBUG
#define FATP_DEBUG_ONLY_EXEC(code) \
    do                             \
    {                              \
        code;                      \
    } while (0)
#else
#define FATP_DEBUG_ONLY_EXEC(code) \
    do                             \
    {                              \
        (void)0;                   \
    } while (0)
#endif

// Increment a debug counter
#ifndef NDEBUG
#define FATP_DEBUG_ONLY_INCREMENT(counter) ++(counter)
#else
#define FATP_DEBUG_ONLY_INCREMENT(counter) ((void)0)
#endif

// Log with a debug value
#ifndef NDEBUG
#define FATP_DEBUG_ONLY_LOG(stream, val) ((stream) << (val).get())
#else
#define FATP_DEBUG_ONLY_LOG(stream, val) ((void)0)
#endif

// ============================================================================
// std::hash specialization
// ============================================================================

} // namespace fat_p

namespace std
{

template <typename T>
struct hash<fat_p::DebugOnly<T>>
{
    constexpr size_t operator()(const fat_p::DebugOnly<T>& val) const
        noexcept(noexcept(std::hash<T>{}(std::declval<T>())))
    {
#ifndef NDEBUG
        if constexpr (fat_p::concepts::hashable<T>)
        {
            return std::hash<T>{}(val.get());
        }
        else
        {
            return 0;
        }
#else
        (void)val;
        return 0; // All release DebugOnly hash to same value
#endif
    }
};

} // namespace std
