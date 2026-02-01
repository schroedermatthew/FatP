#pragma once

/*
FATP_META:
  meta_version: 1
  component: StrongId
  file_role: public_header
  path: include/fat_p/StrongId.h
  namespace: [fat_p, fat_p::strong_id]
  layer: Foundation
  summary: Type-safe ID wrapper with zero runtime overhead and policy-based validation.
  api_stability: candidate
  related:
    docs:
      - Documentation/IN WORK/Overview - StrongId.md
      - Documentation/IN WORK/User Manual - StrongId.md
      - Documentation/IN WORK/Companion Guide - StrongId.md
    tests:
      - components/StrongId/tests/test_StrongId.cpp
    benchmarks:
      - components/StrongId/benchmarks/benchmark_StrongId.cpp
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 1
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/

/**
 * @file StrongId.h
 * @brief Provides the StrongId template for creating strong, type-safe ID
 * wrappers with zero runtime overhead.
 *
 * @layer Foundation
 *
 * @details The StrongId template wraps an underlying integral type (T) and
 * uses a unique Tag struct to create a distinct, compile-time type. This
 * prevents accidental mixing of semantically different ID types (e.g.,
 * UserId vs ProductId) while maintaining the same performance as raw integers.
 *
 * Key Features:
 * - Zero-overhead abstraction (optimizes away completely)
 * - Type-safe: distinct tag types create incompatible ID types
 * - Policy-based validation (NoCheckPolicy, PositiveCheckPolicy, etc.)
 * - Policy-based arithmetic (DefaultOpPolicy with overflow checks, UncheckedOpPolicy)
 * - Full arithmetic and bitwise operator support
 * - std::hash specialization for use in unordered containers
 * - AtomicStrongId wrapper for thread-safe atomic operations
 * - Expected-based safe factory method
 *
 * @example Basic Usage
 * @code
 * struct UserIdTag {};
 * struct ProductIdTag {};
 *
 * using UserId = fat_p::StrongId<int, UserIdTag>;
 * using ProductId = fat_p::StrongId<int, ProductIdTag>;
 *
 * UserId user(42);
 * ProductId product(42);
 * // user == product;  // Compile error: different types!
 *
 * std::unordered_map<UserId, std::string> names;
 * names[user] = "Alice";
 * @endcode
 *
 * @example With Validation Policy
 * @code
 * using PositiveId = fat_p::StrongId<int, MyTag, fat_p::strong_id::PositiveCheckPolicy>;
 * PositiveId id(42);   // OK
 * PositiveId bad(-1);  // throws std::invalid_argument
 *
 * // Safe factory:
 * auto result = PositiveId::create(-1);
 * if (!result) { std::cerr << result.error(); }
 * @endcode
 *
 * @example Atomic Operations
 * @code
 * struct MyTag {};
 * using MyId = fat_p::StrongId<int, MyTag>;
 *
 * fat_p::AtomicStrongId<int, MyTag> atomicId(MyId(0));
 * atomicId.store(MyId(42));
 * MyId val = atomicId.load();
 * @endcode
 *
 * @contract
 * - T must be an integral type (static_assert enforced)
 * - Tag must be a unique type (typically an empty struct)
 * - CheckPolicy::check(T) is called on construction and after arithmetic
 * - Arithmetic operations use OpPolicy for overflow detection
 *
 * @performance
 * - With UncheckedOpPolicy, StrongId is designed to compile down to the same
 *   machine code as the underlying integral type in optimized builds.
 * - With DefaultOpPolicy, arithmetic routes through checked operations and can
 *   add measurable overhead in hot paths.
 *
 * See benchmarks/benchmark_StrongId.cpp for measured data.
 *
 * @thread_safety
 * - StrongId itself is not thread-safe (same as raw integers)
 * - Use AtomicStrongId for thread-safe atomic operations
 * - Lock-freedom depends on the standard library implementation
 *
 * @version 1.0
 *
 * @see fat_p::strong_id::NoCheckPolicy
 * @see fat_p::strong_id::PositiveCheckPolicy
 * @see fat_p::strong_id::NonZeroCheckPolicy
 * @see fat_p::strong_id::DefaultOpPolicy
 * @see fat_p::strong_id::UncheckedOpPolicy
 */

#if FATP_ENABLE_IOSTREAM
#include <iostream>
#endif
#include <atomic>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <compare>

#include "CheckedArithmetic.h"
#include "CppFeatureDetection.h"
#include "Expected.h"
#include "FatPConfig.h"

namespace fat_p
{

// =============================================================================
// Module Namespace: strong_id
// =============================================================================
// Per Systemic Hygiene Policy Rules B and C, policies and helpers live in a
// module namespace to avoid root namespace pollution.

namespace strong_id
{

// =============================================================================
// Check Policies
// =============================================================================

/**
 * @brief Default check policy: no additional validation.
 *
 * Use when any integral value is valid for the ID type.
 */
struct NoCheckPolicy
{
    template <typename T>
    static constexpr void check(T) noexcept
    {
    }
};

/**
 * @brief Check policy requiring non-negative values.
 *
 * For signed types, throws std::invalid_argument if value < 0.
 * For unsigned types, this is a no-op (always valid).
 */
struct PositiveCheckPolicy
{
    template <typename T>
    static constexpr void check(T value)
    {
        if constexpr (std::is_signed_v<T>)
        {
            if (value < 0)
            {
                throw std::invalid_argument("Negative ID value not allowed");
            }
        }
    }
};

/**
 * @brief Check policy requiring non-zero values.
 *
 * Throws std::invalid_argument if value == 0.
 * Useful when 0 is reserved as an invalid/sentinel value.
 */
struct NonZeroCheckPolicy
{
    template <typename T>
    static constexpr void check(T value)
    {
        if (value == T{0})
        {
            throw std::invalid_argument("Zero ID value not allowed");
        }
    }
};

/**
 * @brief Check policy requiring strictly positive values (> 0).
 *
 * Combines NonZero and Positive constraints.
 * For signed types: throws if value <= 0
 * For unsigned types: throws if value == 0
 */
struct StrictlyPositiveCheckPolicy
{
    template <typename T>
    static constexpr void check(T value)
    {
        if constexpr (std::is_signed_v<T>)
        {
            if (value <= 0)
            {
                throw std::invalid_argument("ID value must be strictly positive");
            }
        }
        else
        {
            if (value == T{0})
            {
                throw std::invalid_argument("ID value must be strictly positive");
            }
        }
    }
};

/**
 * @brief Check policy requiring value within a compile-time range.
 *
 * @tparam Min Minimum allowed value (inclusive)
 * @tparam Max Maximum allowed value (inclusive)
 *
 * @example
 * @code
 * using BoundedId = StrongId<int, MyTag, RangeCheckPolicy<1, 1000>>;
 * BoundedId id(500);   // OK
 * BoundedId bad(0);    // throws std::out_of_range
 * BoundedId bad2(1001); // throws std::out_of_range
 * @endcode
 */
template <auto Min, auto Max>
struct RangeCheckPolicy
{
    static_assert(Min <= Max, "RangeCheckPolicy: Min must be <= Max");

    template <typename T>
    static constexpr void check(T value)
    {
        if (value < static_cast<T>(Min) || value > static_cast<T>(Max))
        {
            throw std::out_of_range("ID value out of allowed range");
        }
    }
};

// =============================================================================
// Operation Policies
// =============================================================================

/**
 * @brief Default operation policy: arithmetic with overflow checking.
 *
 * Uses Fat-P checked arithmetic functions which throw on overflow.
 * Provides safety at the cost of some performance overhead.
 */
template <typename U>
struct DefaultOpPolicy
{
    static constexpr U add(U lhs, U rhs)
    {
        return ::fat_p::checked_add(lhs, rhs);
    }
    static constexpr U sub(U lhs, U rhs)
    {
        return ::fat_p::checked_sub(lhs, rhs);
    }
    static constexpr U mul(U lhs, U rhs)
    {
        return ::fat_p::checked_mul(lhs, rhs);
    }
    static constexpr U div(U lhs, U rhs)
    {
        return ::fat_p::checked_div(lhs, rhs);
    }
    static constexpr U mod(U lhs, U rhs)
    {
        return ::fat_p::checked_mod(lhs, rhs);
    }
    static constexpr U neg(U val)
    {
        if constexpr (std::is_signed_v<U>)
        {
            if (val == std::numeric_limits<U>::min())
            {
                throw std::overflow_error("Negation overflow");
            }
        }
        return static_cast<U>(-val);
    }
    static constexpr U bit_and(U lhs, U rhs)
    {
        return ::fat_p::checked_and(lhs, rhs);
    }
    static constexpr U bit_or(U lhs, U rhs)
    {
        return ::fat_p::checked_or(lhs, rhs);
    }
    static constexpr U bit_xor(U lhs, U rhs)
    {
        return ::fat_p::checked_xor(lhs, rhs);
    }
    static constexpr U bit_not(U val)
    {
        return static_cast<U>(~val);
    }
    static constexpr U left_shift(U lhs, U rhs)
    {
        return ::fat_p::checked_left_shift(lhs, rhs);
    }
    static constexpr U right_shift(U lhs, U rhs)
    {
        return ::fat_p::checked_right_shift(lhs, rhs);
    }
};

/**
 * @brief Unchecked operation policy: raw arithmetic without overflow checks.
 *
 * Use when maximum performance is required and inputs are known to be safe.
 * Behavior on overflow is undefined (same as raw integer arithmetic).
 * Benchmarks show this achieves identical performance to raw integers.
 */
template <typename U>
struct UncheckedOpPolicy
{
    static constexpr U add(U lhs, U rhs) noexcept
    {
        return static_cast<U>(lhs + rhs);
    }
    static constexpr U sub(U lhs, U rhs) noexcept
    {
        return static_cast<U>(lhs - rhs);
    }
    static constexpr U mul(U lhs, U rhs) noexcept
    {
        return static_cast<U>(lhs * rhs);
    }
    static constexpr U div(U lhs, U rhs) noexcept
    {
        return static_cast<U>(lhs / rhs);
    }
    static constexpr U mod(U lhs, U rhs) noexcept
    {
        return static_cast<U>(lhs % rhs);
    }
    static constexpr U neg(U val) noexcept
    {
        return static_cast<U>(-val);
    }
    static constexpr U bit_and(U lhs, U rhs) noexcept
    {
        return static_cast<U>(lhs & rhs);
    }
    static constexpr U bit_or(U lhs, U rhs) noexcept
    {
        return static_cast<U>(lhs | rhs);
    }
    static constexpr U bit_xor(U lhs, U rhs) noexcept
    {
        return static_cast<U>(lhs ^ rhs);
    }
    static constexpr U bit_not(U val) noexcept
    {
        return static_cast<U>(~val);
    }
    static constexpr U left_shift(U lhs, U rhs) noexcept
    {
        return static_cast<U>(lhs << rhs);
    }
    static constexpr U right_shift(U lhs, U rhs) noexcept
    {
        return static_cast<U>(lhs >> rhs);
    }
};

} // namespace strong_id

// =============================================================================
// Convenience Macro for Local Namespace Import
// =============================================================================
// Per Systemic Hygiene Policy Rule A.3, provides opt-in local import.
// Users expand this in their .cpp files to import policy types.

/**
 * @brief Convenience macro to import StrongId policies into local scope.
 *
 * @example
 * @code
 * #include "StrongId.h"
 *
 * void myFunction() {
 *     FATP_USING_STRONG_ID_POLICIES();
 *     // Now can use NoCheckPolicy, PositiveCheckPolicy, etc. directly
 *     using MyId = fat_p::StrongId<int, MyTag, PositiveCheckPolicy>;
 * }
 * @endcode
 */
#define FATP_USING_STRONG_ID_POLICIES()                                                                               \
    using ::fat_p::strong_id::NoCheckPolicy;                                                                           \
    using ::fat_p::strong_id::PositiveCheckPolicy;                                                                     \
    using ::fat_p::strong_id::NonZeroCheckPolicy;                                                                      \
    using ::fat_p::strong_id::StrictlyPositiveCheckPolicy;                                                             \
    using ::fat_p::strong_id::RangeCheckPolicy;                                                                        \
    using ::fat_p::strong_id::DefaultOpPolicy;                                                                         \
    using ::fat_p::strong_id::UncheckedOpPolicy

// =============================================================================
// StrongId Class Template
// =============================================================================

/**
 * @brief Strongly typed ID wrapper with zero runtime overhead.
 *
 * @tparam T Underlying integral type (int, long, uint64_t, etc.)
 * @tparam Tag Unique type tag to distinguish different ID types
 * @tparam CheckPolicy Validation policy applied on construction and after arithmetic
 * @tparam OpPolicy Arithmetic operation policy (checked or unchecked)
 */
template <typename T,
          typename Tag,
          typename CheckPolicy = strong_id::NoCheckPolicy,
          template <typename> class OpPolicy = strong_id::DefaultOpPolicy>
class StrongId
{
    static_assert(std::is_integral_v<T>, "StrongId requires an integral underlying type");

public:
    using value_type = T;
    using tag_type = Tag;
    using check_policy = CheckPolicy;
    template <typename U>
    using op_policy = OpPolicy<U>;

    // =========================================================================
    // Constructors
    // =========================================================================

    /** @brief Default constructor. Initializes to 0 and validates. */
    constexpr StrongId()
        : mValue{}
    {
        CheckPolicy::check(mValue);
    }

    /** @brief Explicit constructor from value. Validates via CheckPolicy. */
    explicit constexpr StrongId(T value)
        : mValue(value)
    {
        CheckPolicy::check(mValue);
    }

    // Default copy/move (trivial, zero overhead)
    constexpr StrongId(const StrongId&) = default;
    constexpr StrongId(StrongId&&) noexcept = default;
    constexpr StrongId& operator=(const StrongId&) = default;
    constexpr StrongId& operator=(StrongId&&) noexcept = default;
    ~StrongId() = default;

    // =========================================================================
    // Factory Methods
    // =========================================================================

    /**
     * @brief Safe factory method returning Expected.
     *
     * @param value The value to wrap
     * @return Expected<StrongId, std::string> with the ID or error message
     *
     * @example
     * @code
     * auto result = PositiveId::create(-1);
     * if (result) {
     *     use(*result);
     * } else {
     *     log_error(result.error());
     * }
     * @endcode
     */
    [[nodiscard]] static Expected<StrongId, std::string> create(T value)
    {
        try
        {
            return StrongId(value);
        }
        catch (const std::exception& e)
        {
            return make_unexpected(std::string(e.what()));
        }
    }

    // =========================================================================
    // Sentinel/Validity Methods
    // =========================================================================

    /**
     * @brief Returns an invalid/sentinel StrongId value.
     *
     * Uses std::numeric_limits<T>::max() as the invalid sentinel.
     * This is a common pattern for ID types where max value is reserved.
     *
     * @note This does NOT apply CheckPolicy validation.
     */
    [[nodiscard]] static constexpr StrongId invalid() noexcept
    {
        return StrongId(NoValidateTag{}, kInvalidValue);
    }

    /**
     * @brief Checks if this ID is valid (not the invalid sentinel).
     * @return true if this ID is not equal to invalid()
     */
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return mValue != kInvalidValue;
    }

    /**
     * @brief Returns the minimum possible StrongId value.
     * @note Does NOT apply CheckPolicy validation.
     */
    [[nodiscard]] static constexpr StrongId min() noexcept
    {
        return StrongId(NoValidateTag{}, std::numeric_limits<T>::min());
    }

    /**
     * @brief Returns the maximum non-sentinel StrongId value.
     * @note Does NOT apply CheckPolicy validation.
     *
     * @details The invalid sentinel reserves std::numeric_limits<T>::max(), so
     * this returns one less.
     */
    [[nodiscard]] static constexpr StrongId max() noexcept
    {
        return StrongId(NoValidateTag{}, static_cast<T>(kInvalidValue - T{1}));
    }

    // =========================================================================
    // Accessors
    // =========================================================================

    /** @brief Get underlying value. */
    [[nodiscard]] constexpr T get() const noexcept
    {
        return mValue;
    }

    /** @brief Get underlying value (alias for get()). */
    [[nodiscard]] constexpr T value() const noexcept
    {
        return mValue;
    }

    /** @brief Explicit conversion to underlying type. */
    [[nodiscard]] explicit constexpr operator T() const noexcept
    {
        return mValue;
    }

    // =========================================================================
    // Comparison Operators
    // =========================================================================

    [[nodiscard]] friend constexpr bool operator==(const StrongId& lhs, const StrongId& rhs) noexcept
    {
        return lhs.mValue == rhs.mValue;
    }

    [[nodiscard]] friend constexpr bool operator!=(const StrongId& lhs, const StrongId& rhs) noexcept
    {
        return lhs.mValue != rhs.mValue;
    }

    [[nodiscard]] friend constexpr bool operator<(const StrongId& lhs, const StrongId& rhs) noexcept
    {
        return lhs.mValue < rhs.mValue;
    }

    [[nodiscard]] friend constexpr bool operator<=(const StrongId& lhs, const StrongId& rhs) noexcept
    {
        return lhs.mValue <= rhs.mValue;
    }

    [[nodiscard]] friend constexpr bool operator>(const StrongId& lhs, const StrongId& rhs) noexcept
    {
        return lhs.mValue > rhs.mValue;
    }

    [[nodiscard]] friend constexpr bool operator>=(const StrongId& lhs, const StrongId& rhs) noexcept
    {
        return lhs.mValue >= rhs.mValue;
    }

    [[nodiscard]] friend constexpr auto operator<=>(const StrongId& lhs, const StrongId& rhs) noexcept
    {
        return lhs.mValue <=> rhs.mValue;
    }

    // =========================================================================
    // Increment/Decrement Operators
    // =========================================================================

    constexpr StrongId& operator++()
    {
        mValue = OpPolicy<T>::add(mValue, T(1));
        CheckPolicy::check(mValue);
        return *this;
    }

    constexpr StrongId operator++(int)
    {
        StrongId temp = *this;
        ++(*this);
        return temp;
    }

    constexpr StrongId& operator--()
    {
        mValue = OpPolicy<T>::sub(mValue, T(1));
        CheckPolicy::check(mValue);
        return *this;
    }

    constexpr StrongId operator--(int)
    {
        StrongId temp = *this;
        --(*this);
        return temp;
    }

    // =========================================================================
    // Compound Assignment Operators (StrongId and scalar)
    // =========================================================================

    constexpr StrongId& operator+=(const StrongId& rhs)
    {
        mValue = OpPolicy<T>::add(mValue, rhs.mValue);
        CheckPolicy::check(mValue);
        return *this;
    }

    constexpr StrongId& operator+=(T rhs)
    {
        mValue = OpPolicy<T>::add(mValue, rhs);
        CheckPolicy::check(mValue);
        return *this;
    }

    constexpr StrongId& operator-=(const StrongId& rhs)
    {
        mValue = OpPolicy<T>::sub(mValue, rhs.mValue);
        CheckPolicy::check(mValue);
        return *this;
    }

    constexpr StrongId& operator-=(T rhs)
    {
        mValue = OpPolicy<T>::sub(mValue, rhs);
        CheckPolicy::check(mValue);
        return *this;
    }

    constexpr StrongId& operator*=(const StrongId& rhs)
    {
        mValue = OpPolicy<T>::mul(mValue, rhs.mValue);
        CheckPolicy::check(mValue);
        return *this;
    }

    constexpr StrongId& operator*=(T rhs)
    {
        mValue = OpPolicy<T>::mul(mValue, rhs);
        CheckPolicy::check(mValue);
        return *this;
    }

    constexpr StrongId& operator/=(const StrongId& rhs)
    {
        mValue = OpPolicy<T>::div(mValue, rhs.mValue);
        CheckPolicy::check(mValue);
        return *this;
    }

    constexpr StrongId& operator/=(T rhs)
    {
        mValue = OpPolicy<T>::div(mValue, rhs);
        CheckPolicy::check(mValue);
        return *this;
    }

    constexpr StrongId& operator%=(const StrongId& rhs)
    {
        mValue = OpPolicy<T>::mod(mValue, rhs.mValue);
        CheckPolicy::check(mValue);
        return *this;
    }

    constexpr StrongId& operator%=(T rhs)
    {
        mValue = OpPolicy<T>::mod(mValue, rhs);
        CheckPolicy::check(mValue);
        return *this;
    }

    // =========================================================================
    // Binary Arithmetic Operators
    // =========================================================================

    [[nodiscard]] friend constexpr StrongId operator+(StrongId lhs, const StrongId& rhs)
    {
        return lhs += rhs;
    }

    [[nodiscard]] friend constexpr StrongId operator+(StrongId lhs, T rhs)
    {
        return lhs += rhs;
    }

    [[nodiscard]] friend constexpr StrongId operator-(StrongId lhs, const StrongId& rhs)
    {
        return lhs -= rhs;
    }

    [[nodiscard]] friend constexpr StrongId operator-(StrongId lhs, T rhs)
    {
        return lhs -= rhs;
    }

    [[nodiscard]] friend constexpr StrongId operator*(StrongId lhs, const StrongId& rhs)
    {
        return lhs *= rhs;
    }

    [[nodiscard]] friend constexpr StrongId operator*(StrongId lhs, T rhs)
    {
        return lhs *= rhs;
    }

    [[nodiscard]] friend constexpr StrongId operator/(StrongId lhs, const StrongId& rhs)
    {
        return lhs /= rhs;
    }

    [[nodiscard]] friend constexpr StrongId operator/(StrongId lhs, T rhs)
    {
        return lhs /= rhs;
    }

    [[nodiscard]] friend constexpr StrongId operator%(StrongId lhs, const StrongId& rhs)
    {
        return lhs %= rhs;
    }

    [[nodiscard]] friend constexpr StrongId operator%(StrongId lhs, T rhs)
    {
        return lhs %= rhs;
    }

    // =========================================================================
    // Unary Operators
    // =========================================================================

    [[nodiscard]] constexpr StrongId operator-() const
    {
        return StrongId(OpPolicy<T>::neg(mValue));
    }

    [[nodiscard]] constexpr StrongId operator+() const noexcept
    {
        return *this;
    }

    // =========================================================================
    // Bitwise Compound Assignment (StrongId and scalar)
    // =========================================================================

    constexpr StrongId& operator&=(const StrongId& rhs)
    {
        mValue = OpPolicy<T>::bit_and(mValue, rhs.mValue);
        CheckPolicy::check(mValue);
        return *this;
    }

    constexpr StrongId& operator&=(T rhs)
    {
        mValue = OpPolicy<T>::bit_and(mValue, rhs);
        CheckPolicy::check(mValue);
        return *this;
    }

    constexpr StrongId& operator|=(const StrongId& rhs)
    {
        mValue = OpPolicy<T>::bit_or(mValue, rhs.mValue);
        CheckPolicy::check(mValue);
        return *this;
    }

    constexpr StrongId& operator|=(T rhs)
    {
        mValue = OpPolicy<T>::bit_or(mValue, rhs);
        CheckPolicy::check(mValue);
        return *this;
    }

    constexpr StrongId& operator^=(const StrongId& rhs)
    {
        mValue = OpPolicy<T>::bit_xor(mValue, rhs.mValue);
        CheckPolicy::check(mValue);
        return *this;
    }

    constexpr StrongId& operator^=(T rhs)
    {
        mValue = OpPolicy<T>::bit_xor(mValue, rhs);
        CheckPolicy::check(mValue);
        return *this;
    }

    constexpr StrongId& operator<<=(T rhs)
    {
        mValue = OpPolicy<T>::left_shift(mValue, rhs);
        CheckPolicy::check(mValue);
        return *this;
    }

    constexpr StrongId& operator>>=(T rhs)
    {
        mValue = OpPolicy<T>::right_shift(mValue, rhs);
        CheckPolicy::check(mValue);
        return *this;
    }

    // =========================================================================
    // Binary Bitwise Operators
    // =========================================================================

    [[nodiscard]] friend constexpr StrongId operator&(StrongId lhs, const StrongId& rhs)
    {
        return lhs &= rhs;
    }

    [[nodiscard]] friend constexpr StrongId operator&(StrongId lhs, T rhs)
    {
        return lhs &= rhs;
    }

    [[nodiscard]] friend constexpr StrongId operator|(StrongId lhs, const StrongId& rhs)
    {
        return lhs |= rhs;
    }

    [[nodiscard]] friend constexpr StrongId operator|(StrongId lhs, T rhs)
    {
        return lhs |= rhs;
    }

    [[nodiscard]] friend constexpr StrongId operator^(StrongId lhs, const StrongId& rhs)
    {
        return lhs ^= rhs;
    }

    [[nodiscard]] friend constexpr StrongId operator^(StrongId lhs, T rhs)
    {
        return lhs ^= rhs;
    }

    [[nodiscard]] friend constexpr StrongId operator<<(StrongId lhs, T rhs)
    {
        return lhs <<= rhs;
    }

    [[nodiscard]] friend constexpr StrongId operator>>(StrongId lhs, T rhs)
    {
        return lhs >>= rhs;
    }

    [[nodiscard]] constexpr StrongId operator~() const
    {
        return StrongId(OpPolicy<T>::bit_not(mValue));
    }

    // =========================================================================
    // Swap
    // =========================================================================

    constexpr void swap(StrongId& other) noexcept
    {
        using std::swap;
        swap(mValue, other.mValue);
    }

    friend constexpr void swap(StrongId& lhs, StrongId& rhs) noexcept
    {
        lhs.swap(rhs);
    }

private:
    struct NoValidateTag
    {
        explicit constexpr NoValidateTag() noexcept = default;
    };

    static constexpr T kInvalidValue = std::numeric_limits<T>::max();

    explicit constexpr StrongId(NoValidateTag, T value) noexcept
        : mValue(value)
    {
    }

    T mValue{};
};

// =============================================================================
// Stream Output Operator
// =============================================================================

#if FATP_ENABLE_IOSTREAM
template <typename T, typename Tag, typename Check, template <typename> class Op>
std::ostream& operator<<(std::ostream& os, const StrongId<T, Tag, Check, Op>& id)
{
    return os << id.get();
}
#endif

// =============================================================================
// AtomicStrongId Wrapper
// =============================================================================

/**
 * @brief Atomic wrapper for StrongId providing thread-safe operations.
 *
 * @details
 * std::atomic only provides arithmetic RMW operations (fetch_add, etc.) for
 * built-in arithmetic types. Since StrongId is a user-defined type, the
 * std::atomic<StrongId<...>> specialization only offers load/store/exchange/
 * compare_exchange operations.
 *
 * AtomicStrongId wraps std::atomic<StrongId<...>> and adds fetch_add/fetch_sub
 * operations implemented via compare_exchange loops, respecting the OpPolicy
 * overflow checks.
 *
 * Lock-freedom depends on the underlying std::atomic<StrongId<...>> provided
 * by the standard library.
 *
 * @example
 * @code
 * AtomicStrongId<int, MyTag> counter(MyId(0));
 *
 * // Thread-safe operations:
 * counter.store(MyId(42));
 * MyId val = counter.load();
 * MyId old = counter.exchange(MyId(100));
 *
 * // Atomic increment (not available on std::atomic<StrongId>):
 * MyId prev = counter.fetch_add(1);
 *
 * // Compare-exchange:
 * MyId expected(42);
 * counter.compare_exchange_strong(expected, MyId(50));
 * @endcode
 */
template <typename T,
          typename Tag,
          typename CheckPolicy = strong_id::NoCheckPolicy,
          template <typename> class OpPolicy = strong_id::DefaultOpPolicy>
class AtomicStrongId
{
public:
    using strong_id_type = StrongId<T, Tag, CheckPolicy, OpPolicy>;
    using value_type = strong_id_type;

    static constexpr bool is_always_lock_free = std::atomic<strong_id_type>::is_always_lock_free;

    AtomicStrongId() noexcept = default;

    explicit AtomicStrongId(strong_id_type desired) noexcept
        : mAtomic(desired)
    {
    }

    AtomicStrongId(const AtomicStrongId&) = delete;
    AtomicStrongId& operator=(const AtomicStrongId&) = delete;

    strong_id_type operator=(strong_id_type desired) noexcept
    {
        store(desired);
        return desired;
    }

    operator strong_id_type() const noexcept
    {
        return load();
    }

    [[nodiscard]] bool is_lock_free() const noexcept
    {
        return mAtomic.is_lock_free();
    }

    void store(strong_id_type desired,
               std::memory_order order = std::memory_order_seq_cst) noexcept
    {
        mAtomic.store(desired, order);
    }

    [[nodiscard]] strong_id_type load(
        std::memory_order order = std::memory_order_seq_cst) const noexcept
    {
        return mAtomic.load(order);
    }

    [[nodiscard]] strong_id_type exchange(
        strong_id_type desired,
        std::memory_order order = std::memory_order_seq_cst) noexcept
    {
        return mAtomic.exchange(desired, order);
    }

    bool compare_exchange_weak(strong_id_type& expected,
                               strong_id_type desired,
                               std::memory_order success,
                               std::memory_order failure) noexcept
    {
        return mAtomic.compare_exchange_weak(expected, desired, success, failure);
    }

    bool compare_exchange_weak(strong_id_type& expected,
                               strong_id_type desired,
                               std::memory_order order = std::memory_order_seq_cst) noexcept
    {
        return mAtomic.compare_exchange_weak(expected, desired, order);
    }

    bool compare_exchange_strong(strong_id_type& expected,
                                 strong_id_type desired,
                                 std::memory_order success,
                                 std::memory_order failure) noexcept
    {
        return mAtomic.compare_exchange_strong(expected, desired, success, failure);
    }

    bool compare_exchange_strong(strong_id_type& expected,
                                 strong_id_type desired,
                                 std::memory_order order = std::memory_order_seq_cst) noexcept
    {
        return mAtomic.compare_exchange_strong(expected, desired, order);
    }

    // =========================================================================
    // Atomic RMW Operations (via CAS loop)
    // =========================================================================

    /**
     * @brief Atomically adds arg to the stored value and returns the old value.
     * @param arg Value to add (as underlying type T)
     * @param order Memory order for the operation
     * @return The value before the addition
     */
    strong_id_type fetch_add(T arg,
                             std::memory_order order = std::memory_order_seq_cst)
    {
        return fetch_add_impl(arg, order);
    }

    /**
     * @brief Atomically adds arg to the stored value and returns the old value.
     * @param arg Value to add (as StrongId)
     * @param order Memory order for the operation
     * @return The value before the addition
     */
    strong_id_type fetch_add(strong_id_type arg,
                             std::memory_order order = std::memory_order_seq_cst)
    {
        return fetch_add_impl(arg.get(), order);
    }

    /**
     * @brief Atomically subtracts arg from the stored value and returns the old value.
     * @param arg Value to subtract (as underlying type T)
     * @param order Memory order for the operation
     * @return The value before the subtraction
     */
    strong_id_type fetch_sub(T arg,
                             std::memory_order order = std::memory_order_seq_cst)
    {
        return fetch_sub_impl(arg, order);
    }

    /**
     * @brief Atomically subtracts arg from the stored value and returns the old value.
     * @param arg Value to subtract (as StrongId)
     * @param order Memory order for the operation
     * @return The value before the subtraction
     */
    strong_id_type fetch_sub(strong_id_type arg,
                             std::memory_order order = std::memory_order_seq_cst)
    {
        return fetch_sub_impl(arg.get(), order);
    }

private:
    strong_id_type fetch_add_impl(T arg, std::memory_order order)
    {
        strong_id_type expected = mAtomic.load(std::memory_order_relaxed);

        for (;;)
        {
            strong_id_type old = expected;
            strong_id_type desired = old + arg;

            if (mAtomic.compare_exchange_weak(expected,
                                              desired,
                                              order,
                                              std::memory_order_relaxed))
            {
                return old;
            }
        }
    }

    strong_id_type fetch_sub_impl(T arg, std::memory_order order)
    {
        strong_id_type expected = mAtomic.load(std::memory_order_relaxed);

        for (;;)
        {
            strong_id_type old = expected;
            strong_id_type desired = old - arg;

            if (mAtomic.compare_exchange_weak(expected,
                                              desired,
                                              order,
                                              std::memory_order_relaxed))
            {
                return old;
            }
        }
    }

    std::atomic<strong_id_type> mAtomic{};
};

} // namespace fat_p

// =============================================================================
// std::hash Specialization
// =============================================================================

namespace std
{
template <typename T, typename Tag, typename Check, template <typename> class Op>
struct hash<fat_p::StrongId<T, Tag, Check, Op>>
{
    std::size_t operator()(const fat_p::StrongId<T, Tag, Check, Op>& id) const noexcept
    {
        return std::hash<T>{}(id.get());
    }
};
} // namespace std
