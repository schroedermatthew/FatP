/**
 * @file CheckedArithmeticBase.h
 * @brief Core types, enums, and detection traits for checked arithmetic
 *
 *
 * @layer Foundation
 *
 * @version 3.1
 *
 * This is the foundational header of the CheckedArithmetic module.
 * It contains only the basic types that all other components depend on:
 * - MathError enum for error classification
 * - Formatting utilities for error messages
 * - Forward declarations of policy types (for minimal coupling)
 * - HPC container detection traits (for aligned stores)
 *
 * Dependency: None (self-contained)
 *
 * Part of the CheckedArithmetic split architecture:
 *   CheckedArithmeticBase.h     <- This file (foundation)
 *   CheckedArithmeticPolicies.h <- Depends on Base
 *   CheckedArithmeticInt.h      <- Depends on Policies
 *   CheckedArithmeticFP.h       <- Depends on Policies
 *   CheckedArithmetic.h         <- Umbrella (includes all)
 *
 * =============================================================================
 * DESIGN PATTERN: Forward Declarations for Minimal Coupling
 * =============================================================================
 *
 * This header uses forward declarations for policy types:
 *
 *   struct ThrowOnErrorPolicy;
 *   struct ReturnExpectedPolicy;
 *   // etc.
 *
 * This works because:
 * 1. PolicyReturnType uses std::is_same_v<Policy, X> which operates on
 *    incomplete types - it only compares type identity, not type contents.
 *
 * 2. The actual policy definitions in CheckedArithmeticPolicies.h complete
 *    these forward declarations.
 *
 * Benefits:
 * - Base.h has zero dependencies on Policies.h
 * - Policies.h can include Base.h without circularity
 * - PolicyReturnType<> can be defined here and used everywhere
 *
 * This pattern is recommended for any library with policy-based design
 * where return types depend on policy selection.
 * =============================================================================
 */
#pragma once

/*
FATP_META:
  meta_version: 1
  component: CheckedArithmeticBase
  file_role: public_header
  path: fat_p/CheckedArithmeticBase.h
  namespace: fat_p
  layer: Foundation
  summary: "Public header for CheckedArithmeticBase."
  api_stability: in_work
  related:
    docs_search: "CheckedArithmeticBase"
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/
#include <cstdint>
#include <ostream>
#include <type_traits>
#include <utility> // for std::declval

#include "Expected.h" // Required for PolicyReturnType

namespace fat_p
{

// =============================================================================
// Forward Declarations (Policy Types)
// =============================================================================
// These are defined in CheckedArithmeticPolicies.h but forward-declared here
// so that PolicyReturnType<> can be defined without including Policies.h.
// This breaks what would otherwise be a circular dependency.
//
// Note: std::is_same_v works with incomplete (forward-declared) types because
// it only checks type identity, not type contents.

struct ThrowOnErrorPolicy;
struct ReturnExpectedPolicy;
struct SaturatingPolicy;
struct InfTolerantPolicy;

// =============================================================================
// Error Classification
// =============================================================================

/**
 * @brief Error codes for checked arithmetic operations
 *
 * Used by both integer and floating-point checked operations to classify
 * the type of error that occurred.
 */
enum class MathError
{
    Overflow,       ///< Result exceeded maximum representable value
    Underflow,      ///< Result fell below minimum representable value
    DivByZero,      ///< Division or modulo by zero attempted
    NaN,            ///< Result is Not-a-Number (FP only)
    Inf,            ///< Result is infinity from finite inputs (FP only)
    InvalidArgument ///< Invalid input (e.g., bad shift amount, NaN input)
};

/**
 * @brief Stream insertion operator for MathError
 *
 * Enables human-readable error output in diagnostics and exceptions.
 */
inline std::ostream& operator<<(std::ostream& os, MathError err)
{
    switch (err)
    {
        case MathError::Overflow:
            return os << "Overflow";
        case MathError::Underflow:
            return os << "Underflow";
        case MathError::DivByZero:
            return os << "DivByZero";
        case MathError::NaN:
            return os << "NaN";
        case MathError::Inf:
            return os << "Inf";
        case MathError::InvalidArgument:
            return os << "InvalidArgument";
        default:
            return os << "Unknown";
    }
}

// =============================================================================
// Policy Return Type (uses forward-declared policies)
// =============================================================================

/**
 * @brief Computes the return type for a checked operation based on policy
 *
 * This template alias works with forward-declared policy types because
 * std::is_same_v only checks type identity, not type completeness.
 *
 * - ThrowOnErrorPolicy, SaturatingPolicy, InfTolerantPolicy: Returns R
 * - ReturnExpectedPolicy: Returns Expected<R, MathError>
 */
template <typename Policy, typename R>
using PolicyReturnType =
    std::conditional_t<std::is_same_v<Policy, ReturnExpectedPolicy>, fat_p::Expected<R, fat_p::MathError>, R>;

// =============================================================================
// SIMD Configuration Constants
// =============================================================================

/// Number of 32-bit integers per AVX2 register
constexpr std::size_t AVX2_INT32_PER_REG = 8;

/// Number of doubles per AVX2 register
constexpr std::size_t AVX2_DOUBLES_PER_REG = 4;

// =============================================================================
// HPC Container Detection Traits
// =============================================================================
// These traits enable optimized SIMD stores when using HPC-aware containers
// like AlignedVector or HpcVector that provide alignment guarantees.

/**
 * @brief SFINAE detector for containers with assume_aligned() method
 *
 * HPC containers like AlignedVector provide an assume_aligned() method
 * that returns a pointer with compiler alignment hints. This trait
 * detects such containers so vector operations can use aligned stores.
 *
 * Usage in vector operations:
 * @code
 *   if constexpr (has_assume_aligned_v<ResultVec>) {
 *       T* ptr = result.assume_aligned();
 *       vr.store_aligned(ptr + i);
 *   } else {
 *       vr.store_unaligned(result.data() + i);
 *   }
 * @endcode
 *
 * @tparam C Container type to test
 */
template <typename C, typename = void>
struct has_assume_aligned : std::false_type
{
};

template <typename C>
struct has_assume_aligned<C, std::void_t<decltype(std::declval<C>().assume_aligned())>> : std::true_type
{
};

/**
 * @brief Helper variable template for has_assume_aligned
 */
template <typename C>
inline constexpr bool has_assume_aligned_v = has_assume_aligned<C>::value;

/**
 * @brief SFINAE detector for containers with aligned data() method
 *
 * Some containers guarantee their data() pointer is aligned even without
 * a separate assume_aligned() method. This checks for an aligned_data()
 * method as an alternative.
 */
template <typename C, typename = void>
struct has_aligned_data : std::false_type
{
};

template <typename C>
struct has_aligned_data<C, std::void_t<decltype(std::declval<C>().aligned_data())>> : std::true_type
{
};

template <typename C>
inline constexpr bool has_aligned_data_v = has_aligned_data<C>::value;

/**
 * @brief Unified check for alignment-aware containers
 *
 * Returns true if the container provides any form of aligned pointer access.
 */
template <typename C>
inline constexpr bool is_alignment_aware_v = has_assume_aligned_v<C> || has_aligned_data_v<C>;

/**
 * @brief Get aligned pointer from container (with SFINAE dispatch)
 *
 * Returns a pointer suitable for aligned SIMD operations:
 * - If container has assume_aligned(): calls it
 * - If container has aligned_data(): calls it
 * - Otherwise: returns data() (unaligned fallback)
 *
 * @tparam C Container type
 * @param container The container to get pointer from
 * @return Pointer to container's data (aligned if supported)
 */
template <typename C>
auto get_aligned_ptr(C& container) noexcept
{
    if constexpr (has_assume_aligned_v<C>)
    {
        return container.assume_aligned();
    }
    else if constexpr (has_aligned_data_v<C>)
    {
        return container.aligned_data();
    }
    else
    {
        return container.data();
    }
}

/**
 * @brief Const version of get_aligned_ptr
 */
template <typename C>
auto get_aligned_ptr(const C& container) noexcept
{
    if constexpr (has_assume_aligned_v<std::remove_const_t<C>>)
    {
        return container.assume_aligned();
    }
    else if constexpr (has_aligned_data_v<std::remove_const_t<C>>)
    {
        return container.aligned_data();
    }
    else
    {
        return container.data();
    }
}

} // namespace fat_p
