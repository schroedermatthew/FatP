// PipeOperator.h (User-defined pipe operator overload)
#ifndef CPP_UTILITIES_PIPE_OPERATOR_H
#define CPP_UTILITIES_PIPE_OPERATOR_H

#include "Expected.h"
#include <type_traits>

namespace cpp_utilities {

// =============================================================================
// Type Traits for Detecting Expected
// =============================================================================

template <typename T>
struct is_expected : std::false_type {};

template <typename T, typename E, template <typename, typename> class Storage>
struct is_expected<ExpectedImpl<T, E, Storage>> : std::true_type {};

template <typename T>
inline constexpr bool is_expected_v = is_expected<std::decay_t<T>>::value;

// =============================================================================
// General Pipe Operator for Non-Expected Types
// =============================================================================

/**
 * @brief Pipe operator for regular values
 * @tparam T Value type
 * @tparam Func Function type
 * @param value Input value
 * @param func Function to apply
 * @return Result of func(value)
 * 
 * @details This overload is only enabled when T is not an Expected type
 * and the function doesn't return an Expected.
 */
template <typename T, typename Func>
auto operator|(T&& value, Func&& func) 
    -> std::enable_if_t<!is_expected_v<T>, decltype(func(std::forward<T>(value)))>
{
    return func(std::forward<T>(value));
}

// =============================================================================
// Pipe Operator for Expected Types
// =============================================================================

/**
 * @brief Pipe operator for Expected with function returning non-Expected
 * @tparam T Value type
 * @tparam E Error type
 * @tparam Storage Storage policy template
 * @tparam Func Function type
 * @param exp Expected value
 * @param func Function to apply to the value
 * @return Expected containing the result or the original error
 * 
 * @details If Expected has a value, applies func and wraps result in Expected.
 * If Expected has an error, propagates the error.
 * This overload is for functions that return plain values.
 */
template <typename T, typename E, template <typename, typename> class Storage, typename Func>
auto operator|(ExpectedImpl<T, E, Storage>&& exp, Func&& func)
    -> std::enable_if_t<
        !is_expected_v<decltype(func(*std::move(exp)))>,
        ExpectedImpl<decltype(func(*std::move(exp))), E, Storage>
    >
{
    using ResultType = decltype(func(*std::move(exp)));
    using ReturnType = ExpectedImpl<ResultType, E, Storage>;
    
    if (exp.has_value()) {
        return ReturnType(func(*std::move(exp)));
    }
    return ReturnType(unexpected<E>(std::move(exp.error())));
}

/**
 * @brief Pipe operator for Expected with function returning Expected
 * @tparam T Value type
 * @tparam E Error type
 * @tparam Storage Storage policy template
 * @tparam Func Function type
 * @param exp Expected value
 * @param func Function to apply (returns Expected)
 * @return Expected from func or the original error
 * 
 * @details This is the monadic bind operation (flatMap/and_then).
 * If Expected has a value, applies func which returns another Expected.
 * If Expected has an error, propagates the error.
 * No double-wrapping occurs.
 */
template <typename T, typename E, template <typename, typename> class Storage, typename Func>
auto operator|(ExpectedImpl<T, E, Storage>&& exp, Func&& func)
    -> std::enable_if_t<
        is_expected_v<decltype(func(*std::move(exp)))>,
        decltype(func(*std::move(exp)))
    >
{
    using ReturnType = decltype(func(*std::move(exp)));
    
    if (exp.has_value()) {
        return func(*std::move(exp));
    }
    return ReturnType(unexpected<E>(std::move(exp.error())));
}

/**
 * @brief Pipe operator for const lvalue Expected with function returning non-Expected
 */
template <typename T, typename E, template <typename, typename> class Storage, typename Func>
auto operator|(const ExpectedImpl<T, E, Storage>& exp, Func&& func)
    -> std::enable_if_t<
        !is_expected_v<decltype(func(*exp))>,
        ExpectedImpl<decltype(func(*exp)), E, Storage>
    >
{
    using ResultType = decltype(func(*exp));
    using ReturnType = ExpectedImpl<ResultType, E, Storage>;
    
    if (exp.has_value()) {
        return ReturnType(func(*exp));
    }
    return ReturnType(unexpected<E>(exp.error()));
}

/**
 * @brief Pipe operator for const lvalue Expected with function returning Expected
 */
template <typename T, typename E, template <typename, typename> class Storage, typename Func>
auto operator|(const ExpectedImpl<T, E, Storage>& exp, Func&& func)
    -> std::enable_if_t<
        is_expected_v<decltype(func(*exp))>,
        decltype(func(*exp))
    >
{
    using ReturnType = decltype(func(*exp));
    
    if (exp.has_value()) {
        return func(*exp);
    }
    return ReturnType(unexpected<E>(exp.error()));
}

}  // namespace cpp_utilities

#endif  // CPP_UTILITIES_PIPE_OPERATOR_H
