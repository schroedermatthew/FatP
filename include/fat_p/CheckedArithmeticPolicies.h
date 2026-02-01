/**
 * @file CheckedArithmeticPolicies.h
 * @brief Policy classes and type traits for checked arithmetic
 *
 *
 * @layer Foundation
 *
 * @version 3.1
 *
 * This header defines the error handling policies that control how
 * checked arithmetic operations respond to errors (overflow, NaN, etc.).
 *
 * Policies:
 * - ThrowOnErrorPolicy: Throws exception on error (default)
 * - ReturnExpectedPolicy: Returns Expected<T, MathError> for noexcept safety
 * - SaturatingPolicy: Clamps to min/max on overflow
 * - InfTolerantPolicy: Like Saturating but allows infinity through for FP
 *
 * Also provides:
 * - PolicyTraits<Policy>: Query policy properties via full specializations
 * - C++20 concepts (conditional) and SFINAE helpers
 *
 * =============================================================================
 * DESIGN PATTERN: Full Specialization for Extensibility
 * =============================================================================
 *
 * PolicyTraits uses full template specialization rather than conditional logic:
 *
 *   template <> struct PolicyTraits<ThrowOnErrorPolicy> {
 *       template <typename T> static constexpr bool is_noexcept = false;
 *       // ... other traits
 *   };
 *
 * Benefits over conditional approach:
 * 1. Adding new policies requires only a new specialization, no changes to
 *    existing code (Open/Closed Principle)
 * 2. Each policy's traits are self-contained and easy to audit
 * 3. Compile-time errors if a policy lacks required traits
 * 4. Enables policy-specific type aliases (ReturnType) cleanly
 *
 * To add a custom policy:
 * 1. Define your policy tag: struct MyPolicy {};
 * 2. Specialize PolicyTraits<MyPolicy> with required members
 * 3. (Optional) Add to PolicyReturnType if needed
 * =============================================================================
 *
 * Dependency: CheckedArithmeticBase.h
 *
 * Part of the CheckedArithmetic split architecture:
 *   CheckedArithmeticBase.h     <- Foundation
 *   CheckedArithmeticPolicies.h <- This file
 *   CheckedArithmeticInt.h      <- Depends on this
 *   CheckedArithmeticFP.h       <- Depends on this
 *   CheckedArithmetic.h         <- Umbrella
 */
#pragma once

/*
FATP_META:
  meta_version: 1
  component: CheckedArithmeticPolicies
  file_role: public_header
  path: include/fat_p/CheckedArithmeticPolicies.h
  namespace: fat_p
  layer: Foundation
  summary: "Public header for CheckedArithmeticPolicies."
  api_stability: in_work
  related:
    docs_search: "CheckedArithmeticPolicies"
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 10
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/
#include "CheckedArithmeticBase.h"
#include "CppFeatureDetection.h"
#include "Expected.h"

#include <limits>
#include <type_traits>

namespace fat_p
{

// =============================================================================
// Policy Tag Classes (complete the forward declarations from Base.h)
// =============================================================================

/**
 * @brief Policy that throws exceptions on arithmetic errors
 *
 * This is the default policy. When an error is detected (overflow, etc.),
 * the operation throws an exception via FATP_ALWAYS_ENFORCE().
 *
 * Use this when:
 * - Arithmetic errors are truly exceptional
 * - You want to fail fast and loud
 * - Performance of the error path doesn't matter
 *
 * Properties:
 * - is_noexcept: false
 * - Returns: T directly
 */
struct ThrowOnErrorPolicy
{
};

/**
 * @brief Policy that returns Expected<T, MathError> instead of throwing
 *
 * All operations become noexcept and return an Expected type that must
 * be checked by the caller.
 *
 * Use this when:
 * - You need noexcept guarantees
 * - Error handling must be explicit
 * - You want monadic error composition
 *
 * Properties:
 * - is_noexcept: true
 * - Returns: Expected<T, MathError>
 */
struct ReturnExpectedPolicy
{
};

/**
 * @brief Policy that saturates to min/max on overflow
 *
 * Instead of throwing or returning an error, saturating operations
 * clamp the result to the representable range.
 *
 * Use this when:
 * - You want deterministic behavior without exceptions
 * - Saturation semantics are appropriate (DSP, image processing)
 * - You need noexcept guarantees
 *
 * Properties:
 * - is_noexcept: true
 * - Returns: T directly (clamped)
 *
 * Note: On NEON, SaturatingPolicy can use hardware saturation (vqadd/vqsub)
 * for zero-overhead implementation.
 */
struct SaturatingPolicy
{
};

/**
 * @brief Policy like Saturating but allows infinity for FP operations
 *
 * For floating-point operations, infinity from finite inputs is allowed
 * through rather than being treated as an error. NaN is still handled
 * as an error.
 *
 * Use this when:
 * - You're doing FP computations where infinity is valid
 * - You want to distinguish "true infinity" from overflow
 *
 * Properties:
 * - is_noexcept: true
 * - Returns: T directly
 * - allows_infinity: true (unique to this policy)
 */
struct InfTolerantPolicy
{
};

// =============================================================================
// Policy Traits - Primary Template (undefined)
// =============================================================================
// The primary template is intentionally left undefined. Each policy must
// provide a full specialization. This ensures compile-time errors if
// someone uses an unsupported policy type.

/**
 * @brief Traits class for querying policy properties
 *
 * Each policy must specialize this template with:
 * - template<T> static constexpr bool is_noexcept
 * - template<T> using ReturnType = ...
 * - static constexpr bool allows_infinity (optional, for FP policies)
 *
 * @tparam Policy The policy type to query
 */
template <typename Policy>
struct PolicyTraits;
// No definition - requires specialization

// =============================================================================
// PolicyTraits Specializations
// =============================================================================

/**
 * @brief Traits for ThrowOnErrorPolicy
 */
template <>
struct PolicyTraits<ThrowOnErrorPolicy>
{
    /// Operations with this policy may throw
    template <typename T>
    static constexpr bool is_noexcept = false;

    /// Return type is T directly
    template <typename T>
    using ReturnType = T;

    /// This policy does not allow infinity (FP operations check for it)
    static constexpr bool allows_infinity = false;

    /// Human-readable policy name for diagnostics
    static constexpr const char* name = "ThrowOnError";
};

/**
 * @brief Traits for ReturnExpectedPolicy
 */
template <>
struct PolicyTraits<ReturnExpectedPolicy>
{
    /// Operations with this policy are noexcept
    template <typename T>
    static constexpr bool is_noexcept = true;

    /// Return type is Expected<T, MathError>
    template <typename T>
    using ReturnType = Expected<T, MathError>;

    /// This policy does not allow infinity (returns error instead)
    static constexpr bool allows_infinity = false;

    /// Human-readable policy name for diagnostics
    static constexpr const char* name = "ReturnExpected";
};

/**
 * @brief Traits for SaturatingPolicy
 */
template <>
struct PolicyTraits<SaturatingPolicy>
{
    /// Operations with this policy are noexcept
    template <typename T>
    static constexpr bool is_noexcept = true;

    /// Return type is T directly (clamped to range)
    template <typename T>
    using ReturnType = T;

    /// This policy does not allow infinity (saturates to max instead)
    static constexpr bool allows_infinity = false;

    /// Human-readable policy name for diagnostics
    static constexpr const char* name = "Saturating";
};

/**
 * @brief Traits for InfTolerantPolicy
 */
template <>
struct PolicyTraits<InfTolerantPolicy>
{
    /// Operations with this policy are noexcept
    template <typename T>
    static constexpr bool is_noexcept = true;

    /// Return type is T directly
    template <typename T>
    using ReturnType = T;

    /// This policy DOES allow infinity through (unique among policies)
    static constexpr bool allows_infinity = true;

    /// Human-readable policy name for diagnostics
    static constexpr const char* name = "InfTolerant";
};

// =============================================================================
// Policy Helper Functions
// =============================================================================

/**
 * @brief Check if a policy allows infinity through for FP operations
 *
 * @tparam Policy The policy to check
 * @return true if infinity is tolerated, false otherwise
 */
template <typename Policy>
inline constexpr bool policy_allows_infinity_v = PolicyTraits<Policy>::allows_infinity;

/**
 * @brief Check if operations with this policy are noexcept
 *
 * @tparam Policy The policy to check
 * @tparam T The value type (usually not needed but kept for consistency)
 */
template <typename Policy, typename T = void>
inline constexpr bool policy_is_noexcept_v = PolicyTraits<Policy>::template is_noexcept<T>;

// =============================================================================
// Type Constraints (C++20 Concepts)
// =============================================================================

/**
 * @brief Concept for integral types excluding bool
 */
template <typename T>
concept IntegralNonBool = std::integral<T> && !std::same_as<T, bool>;

/**
 * @brief Concept for floating-point types
 */
template <typename T>
concept FloatingPoint = std::floating_point<T>;

/**
 * @brief Concept for all arithmetic types (int or FP, excluding bool)
 */
template <typename T>
concept Arithmetic = IntegralNonBool<T> || FloatingPoint<T>;

/**
 * @brief Concept for valid checked arithmetic policies
 *
 * Ensures the policy has required traits defined.
 */
template <typename P>
concept ValidPolicy = requires
{
    {PolicyTraits<P>::template is_noexcept<int>}->std::convertible_to<bool>;
    {PolicyTraits<P>::allows_infinity}->std::convertible_to<bool>;
};

// Macro versions for template parameter lists
#define FATP_ENABLE_IF_INTEGRAL IntegralNonBool T
#define FATP_ENABLE_IF_FLOATING FloatingPoint T
#define FATP_ENABLE_IF_ARITHMETIC Arithmetic T

// =============================================================================
// Builtin Overflow Detection
// =============================================================================

/**
 * @brief Compile-time detection of __builtin_*_overflow intrinsics
 *
 * GCC 5+ and Clang provide these builtins for efficient overflow checking.
 * When available, they generate optimal code (using carry flag, etc.).
 */
#if defined(__has_builtin)
#if __has_builtin(__builtin_add_overflow) && __has_builtin(__builtin_sub_overflow) && \
    __has_builtin(__builtin_mul_overflow)
#define FATP_HAS_BUILTIN_OVERFLOW 1
#else
#define FATP_HAS_BUILTIN_OVERFLOW 0
#endif
#elif defined(__GNUC__) && (__GNUC__ >= 5)
#define FATP_HAS_BUILTIN_OVERFLOW 1
#else
#define FATP_HAS_BUILTIN_OVERFLOW 0
#endif

} // namespace fat_p
