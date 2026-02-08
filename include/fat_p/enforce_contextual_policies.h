#pragma once

/*
FATP_META:
  meta_version: 1
  component: Enforce
  file_role: public_header
  path: include/fat_p/enforce_contextual_policies.h
  namespace: fat_p
  layer: Foundation
  summary: "Public header for enforce_contextual_policies."
  api_stability: in_work
  related:
    docs_search: "enforce_contextual_policies"
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

/**
 * @file enforce_contextual_policies.h
 * @brief Defines type traits and policy tags essential for contextual contract
 * enforcement.
 *
 * @details The core component is the is_noexcept_function_ptr trait, which
 * detects the 'noexcept' specification on function and method pointers at
 * compile time. This information is used to select the correct Raiser
 * (throwing or non-throwing) via ContextualRaiserResolver.
 */

#include <type_traits>

namespace fat_p
{

struct NoThrowRaiser;

// --- Policy Tags ---

/// @brief Policy tag selected when the function is marked 'noexcept'.
/// Directs the system to use NoThrowRaiser, adhering to the function's
/// exception safety guarantee.
struct NoexceptFunctionPolicy
{
};

/// @brief Policy tag selected when the function is capable of throwing.
/// Directs the system to use the default throwing raiser (mapped from
/// PredicateType or explicitly specified) unless explicitly overridden.
struct ThrowingFunctionPolicy
{
};

// --- Function Trait: noexcept Detection ---
//
// Detects whether a function pointer or member function pointer type carries
// the noexcept specification.  C++17 made noexcept part of the type system,
// so each cv/ref-qualified member function pointer is a distinct type that
// requires its own partial specialization.
//
// Layout:
//   - Primary template ............ false (non-function or non-noexcept)
//   - Free function pointer ....... R(*)(Args...) noexcept
//   - Member function pointers .... all 12 cv/ref-qualifier combinations

// Primary template: not a noexcept function pointer.
template <typename T>
inline constexpr bool is_noexcept_function_ptr_v = false;

// -----------------------------------------------------------------
// Free function pointers
// -----------------------------------------------------------------
template <typename R, typename... Args>
inline constexpr bool is_noexcept_function_ptr_v<R (*)(Args...) noexcept> = true;

// -----------------------------------------------------------------
// Member function pointers — all cv/ref-qualifier × noexcept combinations
// -----------------------------------------------------------------
template <typename R, typename C, typename... Args>
inline constexpr bool is_noexcept_function_ptr_v<R (C::*)(Args...) noexcept> = true;

template <typename R, typename C, typename... Args>
inline constexpr bool is_noexcept_function_ptr_v<R (C::*)(Args...) const noexcept> = true;

template <typename R, typename C, typename... Args>
inline constexpr bool is_noexcept_function_ptr_v<R (C::*)(Args...) volatile noexcept> = true;

template <typename R, typename C, typename... Args>
inline constexpr bool is_noexcept_function_ptr_v<R (C::*)(Args...) const volatile noexcept> = true;

template <typename R, typename C, typename... Args>
inline constexpr bool is_noexcept_function_ptr_v<R (C::*)(Args...) & noexcept> = true;

template <typename R, typename C, typename... Args>
inline constexpr bool is_noexcept_function_ptr_v<R (C::*)(Args...) && noexcept> = true;

template <typename R, typename C, typename... Args>
inline constexpr bool is_noexcept_function_ptr_v<R (C::*)(Args...) const & noexcept> = true;

template <typename R, typename C, typename... Args>
inline constexpr bool is_noexcept_function_ptr_v<R (C::*)(Args...) const && noexcept> = true;

template <typename R, typename C, typename... Args>
inline constexpr bool is_noexcept_function_ptr_v<R (C::*)(Args...) volatile & noexcept> = true;

template <typename R, typename C, typename... Args>
inline constexpr bool is_noexcept_function_ptr_v<R (C::*)(Args...) volatile && noexcept> = true;

template <typename R, typename C, typename... Args>
inline constexpr bool is_noexcept_function_ptr_v<R (C::*)(Args...) const volatile & noexcept> = true;

template <typename R, typename C, typename... Args>
inline constexpr bool is_noexcept_function_ptr_v<R (C::*)(Args...) const volatile && noexcept> = true;

/// @brief Struct wrapper for is_noexcept_function_ptr_v.
/// Provided for backward compatibility and use in template-template contexts.
template <typename T>
struct is_noexcept_function_ptr : std::bool_constant<is_noexcept_function_ptr_v<T>>
{
};

// --- Contextual Raiser Resolver ---

template <typename NoexceptPolicy, typename ThrowingRaiser>
struct ContextualRaiserResolver
{
    using type = ThrowingRaiser;
};

template <typename ThrowingRaiser>
struct ContextualRaiserResolver<NoexceptFunctionPolicy, ThrowingRaiser>
{
    using type = NoThrowRaiser;
};

} // namespace fat_p
