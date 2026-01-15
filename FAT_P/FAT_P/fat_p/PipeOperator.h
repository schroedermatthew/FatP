/**
 * @file PipeOperator.h
 * @brief Functional pipe operator for value and Expected type composition
 *
 * Provides Unix-style pipe operator (|) for composing functions with values
 * and Expected types. Enables Rust-like functional pipelines in C++17.
 *
 * Key Features:
 * - General value piping: `value | func1 | func2`
 * - Expected map: `Expected<T> | (T -> U)` -> `Expected<U>`
 * - Expected bind: `Expected<T> | (T -> Expected<U>)` -> `Expected<U>`
 * - Full Expected<void> support for status-only operations
 * - Automatic error propagation through pipelines
 * - Zero-overhead abstraction (compiles to direct calls)
 * - Full const correctness (lvalue and rvalue overloads)
 * - std::invoke support for member pointers and functors
 * - Conditional noexcept for HPC optimization
 * - Smart storage policy handling:
 *   - Preserves TrivialStorage when result is trivially copyable
 *   - Falls back to UnionStorage for void or non-trivial results
 *
 * Storage Policy Behavior:
 * - TrivialExpected<T> | (T -> trivial U) -> TrivialExpected<U> (preserved)
 * - TrivialExpected<T> | (T -> non-trivial U) -> Expected<U, E, UnionStorage> (fallback)
 * - TrivialExpected<T> | (T -> void) -> Expected<void, E, UnionStorage> (fallback)
 *
 * C++20 Ranges Compatibility:
 * The general pipe operator `T | Func` can conflict with C++20 Ranges. To avoid:
 * - Use explicit `using fat_p::operator|;` in limited scope
 * - Or use the `pipe()` wrapper function
 * - Or use method syntax: `exp.map(f).and_then(g)`
 *
 * Thread Safety:
 * Pipe operations are thread-safe for distinct Expected objects.
 * Concurrent piping of the same Expected requires external synchronization.
 *
 * @layer Domain
 */

#pragma once

/*
FATP_META:
  meta_version: 1
  component: PipeOperator
  file_role: public_header
  path: fat_p/PipeOperator.h
  namespace: fat_p
  layer: Foundation
  summary: "Public header for PipeOperator."
  api_stability: in_work
  related:
    docs_search: "PipeOperator"
    tests:
      - tests/test_PipeOperator.cpp
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
#include <functional>
#include <type_traits>
#include <utility>

#include "Expected.h"
#include "FatPTypeTraits.h"

namespace fat_p
{

// =============================================================================
// Internal Helpers
// =============================================================================

namespace pipe_detail
{

template <typename Func, typename... Args>
using invoke_result_t = std::invoke_result_t<Func, Args...>;

template <typename Func, typename... Args>
inline constexpr bool is_nothrow_invocable_v = std::is_nothrow_invocable_v<Func, Args...>;

/**
 * @brief Trait to detect if a storage policy is TrivialStorage
 *
 * We need to detect TrivialStorage specifically because it has strict requirements:
 * - T must be trivially copyable
 * - T must not be void
 *
 * Other storage policies (UnionStorage, VariantStorage) work with any type.
 */
template <template <typename, typename> class Storage>
struct is_trivial_storage : std::false_type
{
};

template <>
struct is_trivial_storage<TrivialStorage> : std::true_type
{
};

template <template <typename, typename> class Storage>
inline constexpr bool is_trivial_storage_v = is_trivial_storage<Storage>::value;

/**
 * @brief Selects appropriate storage policy for pipe result type
 *
 * TrivialStorage has strict requirements:
 * - T must be trivially copyable (static_assert in TrivialStorage)
 * - T must not be void
 *
 * This metafunction ensures we fall back to UnionStorage when the result type
 * doesn't meet TrivialStorage requirements, preventing compilation errors.
 *
 * Logic:
 * - If CurrentStorage is NOT TrivialStorage: always preserve it (UnionStorage/VariantStorage
 *   work with any type)
 * - If CurrentStorage IS TrivialStorage AND ResultType is trivially copyable AND not void:
 *   preserve TrivialStorage
 * - Otherwise: fall back to UnionStorage
 */
template <typename ResultType,
          typename E,
          template <typename, typename> class CurrentStorage,
          bool IsTrivialStorage = is_trivial_storage_v<CurrentStorage>>
struct SelectResultStorage
{
    // Non-TrivialStorage: always preserve the current storage policy
    using type = ExpectedImpl<ResultType, E, CurrentStorage>;
};

// Specialization for TrivialStorage: check if ResultType is compatible
template <typename ResultType, typename E, template <typename, typename> class CurrentStorage>
struct SelectResultStorage<ResultType, E, CurrentStorage, true>
{
    // TrivialStorage requires: trivially copyable AND not void
    static constexpr bool can_use_trivial = std::is_trivially_copyable_v<ResultType> && !std::is_void_v<ResultType>;

    using type = std::conditional_t<can_use_trivial,
                                    ExpectedImpl<ResultType, E, TrivialStorage>,
                                    ExpectedImpl<ResultType, E, UnionStorage>>;
};

/**
 * @brief Helper alias for selecting the correct ExpectedImpl type
 */
template <typename ResultType, typename E, template <typename, typename> class CurrentStorage>
using select_result_t = typename SelectResultStorage<ResultType, E, CurrentStorage>::type;

} // namespace pipe_detail

// =============================================================================
// General Pipe Operator for Non-Expected Types
// =============================================================================

/**
 * @brief Pipe operator for regular values (functional composition)
 * @tparam T Value type (must not be Expected)
 * @tparam Func Callable type
 * @param value Input value to transform
 * @param func Function to apply
 * @return Result of std::invoke(func, value)
 *
 * @warning This overload can conflict with C++20 Ranges. Use scoped
 * `using fat_p::operator|;` or the `pipe()` wrapper function.
 */
template <typename T, typename Func>
auto operator|(T&& value, Func&& func) noexcept(pipe_detail::is_nothrow_invocable_v<Func, T>)
    -> std::enable_if_t<!is_expected_v<std::decay_t<T>>, pipe_detail::invoke_result_t<Func, T>>
{
    return std::invoke(std::forward<Func>(func), std::forward<T>(value));
}

// =============================================================================
// Pipe Operator for Expected<T> (Value Types)
// =============================================================================

/**
 * @brief Map operation: Expected<T>&& | (T -> U) -> Expected<U>
 *
 * Applies function to contained value if present, wraps result.
 * If Expected contains error, propagates error without calling function.
 *
 * Storage policy behavior:
 * - If U is trivially copyable: preserves TrivialStorage
 * - If U is void or non-trivial: falls back to UnionStorage
 */
template <typename T, typename E, template <typename, typename> class Storage, typename Func>
auto operator|(ExpectedImpl<T, E, Storage>&& exp,
               Func&& func) noexcept(pipe_detail::is_nothrow_invocable_v<Func, T&&> &&
                                     std::is_nothrow_move_constructible_v<E>)
    -> std::enable_if_t<!std::is_void_v<T> && !is_expected_v<std::decay_t<pipe_detail::invoke_result_t<Func, T&&>>>,
                        pipe_detail::select_result_t<pipe_detail::invoke_result_t<Func, T&&>, E, Storage>>
{
    using ResultType = pipe_detail::invoke_result_t<Func, T&&>;
    using ReturnType = pipe_detail::select_result_t<ResultType, E, Storage>;

    if (exp.has_value())
    {
        if constexpr (std::is_void_v<ResultType>)
        {
            std::invoke(std::forward<Func>(func), *std::move(exp));
            return ReturnType();
        }
        else
        {
            return ReturnType(std::in_place, std::invoke(std::forward<Func>(func), *std::move(exp)));
        }
    }
    return ReturnType(unexpected<E>(std::move(exp.error())));
}

/**
 * @brief Bind operation: Expected<T>&& | (T -> Expected<U>) -> Expected<U>
 *
 * Monadic bind (flatMap). Applies function returning Expected,
 * flattens result to avoid Expected<Expected<U>>.
 *
 * Note: The storage policy of the result is determined by the function's
 * return type, not the input's storage policy.
 */
template <typename T, typename E, template <typename, typename> class Storage, typename Func>
auto operator|(ExpectedImpl<T, E, Storage>&& exp,
               Func&& func) noexcept(pipe_detail::is_nothrow_invocable_v<Func, T&&> &&
                                     std::is_nothrow_move_constructible_v<E>)
    -> std::enable_if_t<!std::is_void_v<T> && is_expected_v<std::decay_t<pipe_detail::invoke_result_t<Func, T&&>>>,
                        std::decay_t<pipe_detail::invoke_result_t<Func, T&&>>>
{
    using ReturnType = std::decay_t<pipe_detail::invoke_result_t<Func, T&&>>;

    if (exp.has_value())
    {
        return std::invoke(std::forward<Func>(func), *std::move(exp));
    }
    return ReturnType(unexpected<E>(std::move(exp.error())));
}

/**
 * @brief Const map: const Expected<T>& | (T -> U) -> Expected<U>
 */
template <typename T, typename E, template <typename, typename> class Storage, typename Func>
auto operator|(const ExpectedImpl<T, E, Storage>& exp,
               Func&& func) noexcept(pipe_detail::is_nothrow_invocable_v<Func, const T&> &&
                                     std::is_nothrow_copy_constructible_v<E>)
    -> std::enable_if_t<!std::is_void_v<T> &&
                            !is_expected_v<std::decay_t<pipe_detail::invoke_result_t<Func, const T&>>>,
                        pipe_detail::select_result_t<pipe_detail::invoke_result_t<Func, const T&>, E, Storage>>
{
    using ResultType = pipe_detail::invoke_result_t<Func, const T&>;
    using ReturnType = pipe_detail::select_result_t<ResultType, E, Storage>;

    if (exp.has_value())
    {
        if constexpr (std::is_void_v<ResultType>)
        {
            std::invoke(std::forward<Func>(func), *exp);
            return ReturnType();
        }
        else
        {
            return ReturnType(std::in_place, std::invoke(std::forward<Func>(func), *exp));
        }
    }
    return ReturnType(unexpected<E>(exp.error()));
}

/**
 * @brief Const bind: const Expected<T>& | (T -> Expected<U>) -> Expected<U>
 */
template <typename T, typename E, template <typename, typename> class Storage, typename Func>
auto operator|(const ExpectedImpl<T, E, Storage>& exp,
               Func&& func) noexcept(pipe_detail::is_nothrow_invocable_v<Func, const T&> &&
                                     std::is_nothrow_copy_constructible_v<E>)
    -> std::enable_if_t<!std::is_void_v<T> && is_expected_v<std::decay_t<pipe_detail::invoke_result_t<Func, const T&>>>,
                        std::decay_t<pipe_detail::invoke_result_t<Func, const T&>>>
{
    using ReturnType = std::decay_t<pipe_detail::invoke_result_t<Func, const T&>>;

    if (exp.has_value())
    {
        return std::invoke(std::forward<Func>(func), *exp);
    }
    return ReturnType(unexpected<E>(exp.error()));
}

// =============================================================================
// Pipe Operator for Expected<void> (Void Specializations)
// =============================================================================

/**
 * @brief Void map: Expected<void>&& | (() -> U) -> Expected<U>
 *
 * Applies zero-argument function when Expected<void> has value.
 */
template <typename E, template <typename, typename> class Storage, typename Func>
auto operator|(ExpectedImpl<void, E, Storage>&& exp, Func&& func) noexcept(pipe_detail::is_nothrow_invocable_v<Func> &&
                                                                           std::is_nothrow_move_constructible_v<E>)
    -> std::enable_if_t<!is_expected_v<std::decay_t<pipe_detail::invoke_result_t<Func>>>,
                        pipe_detail::select_result_t<pipe_detail::invoke_result_t<Func>, E, Storage>>
{
    using ResultType = pipe_detail::invoke_result_t<Func>;
    using ReturnType = pipe_detail::select_result_t<ResultType, E, Storage>;

    if (exp.has_value())
    {
        if constexpr (std::is_void_v<ResultType>)
        {
            std::invoke(std::forward<Func>(func));
            return ReturnType();
        }
        else
        {
            return ReturnType(std::in_place, std::invoke(std::forward<Func>(func)));
        }
    }
    return ReturnType(unexpected<E>(std::move(exp.error())));
}

/**
 * @brief Void bind: Expected<void>&& | (() -> Expected<U>) -> Expected<U>
 */
template <typename E, template <typename, typename> class Storage, typename Func>
auto operator|(ExpectedImpl<void, E, Storage>&& exp, Func&& func) noexcept(pipe_detail::is_nothrow_invocable_v<Func> &&
                                                                           std::is_nothrow_move_constructible_v<E>)
    -> std::enable_if_t<is_expected_v<std::decay_t<pipe_detail::invoke_result_t<Func>>>,
                        std::decay_t<pipe_detail::invoke_result_t<Func>>>
{
    using ReturnType = std::decay_t<pipe_detail::invoke_result_t<Func>>;

    if (exp.has_value())
    {
        return std::invoke(std::forward<Func>(func));
    }
    return ReturnType(unexpected<E>(std::move(exp.error())));
}

/**
 * @brief Const void map: const Expected<void>& | (() -> U) -> Expected<U>
 */
template <typename E, template <typename, typename> class Storage, typename Func>
auto operator|(const ExpectedImpl<void, E, Storage>& exp,
               Func&& func) noexcept(pipe_detail::is_nothrow_invocable_v<Func> &&
                                     std::is_nothrow_copy_constructible_v<E>)
    -> std::enable_if_t<!is_expected_v<std::decay_t<pipe_detail::invoke_result_t<Func>>>,
                        pipe_detail::select_result_t<pipe_detail::invoke_result_t<Func>, E, Storage>>
{
    using ResultType = pipe_detail::invoke_result_t<Func>;
    using ReturnType = pipe_detail::select_result_t<ResultType, E, Storage>;

    if (exp.has_value())
    {
        if constexpr (std::is_void_v<ResultType>)
        {
            std::invoke(std::forward<Func>(func));
            return ReturnType();
        }
        else
        {
            return ReturnType(std::in_place, std::invoke(std::forward<Func>(func)));
        }
    }
    return ReturnType(unexpected<E>(exp.error()));
}

/**
 * @brief Const void bind: const Expected<void>& | (() -> Expected<U>) -> Expected<U>
 */
template <typename E, template <typename, typename> class Storage, typename Func>
auto operator|(const ExpectedImpl<void, E, Storage>& exp,
               Func&& func) noexcept(pipe_detail::is_nothrow_invocable_v<Func> &&
                                     std::is_nothrow_copy_constructible_v<E>)
    -> std::enable_if_t<is_expected_v<std::decay_t<pipe_detail::invoke_result_t<Func>>>,
                        std::decay_t<pipe_detail::invoke_result_t<Func>>>
{
    using ReturnType = std::decay_t<pipe_detail::invoke_result_t<Func>>;

    if (exp.has_value())
    {
        return std::invoke(std::forward<Func>(func));
    }
    return ReturnType(unexpected<E>(exp.error()));
}

// =============================================================================
// Pipeline Builder (Explicit Syntax for Disambiguation)
// =============================================================================

/**
 * @brief Wrapper type for explicit pipeline construction
 *
 * Use when you need to disambiguate from C++20 Ranges or other
 * libraries that overload operator|.
 *
 * Uses forwarding reference semantics to preserve value category.
 */
template <typename T>
struct PipeWrapper
{
    T value;

    template <typename Func>
    auto operator|(Func&& func) && noexcept(noexcept(std::forward<T>(value) | std::forward<Func>(func)))
        -> decltype(std::forward<T>(value) | std::forward<Func>(func))
    {
        return std::forward<T>(value) | std::forward<Func>(func);
    }
};

/**
 * @brief Create explicit pipe wrapper to avoid operator| ambiguity
 * @param value Value to wrap (can be any type including Expected)
 * @return PipeWrapper that forwards the value correctly
 *
 * Usage:
 *   auto result = fat_p::pipe(my_expected) | transform_func;
 */
template <typename T>
auto pipe(T&& value) -> PipeWrapper<T&&>
{
    return PipeWrapper<T&&>{std::forward<T>(value)};
}

} // namespace fat_p
