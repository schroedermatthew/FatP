/**
 * @file TypeTraits.h
 * @brief Comprehensive type trait utilities for C++17/20/23
 * @layer CoreUtility
 * 
 * @details Provides extensive compile-time type introspection including:
 * - Detection idiom (is_detected, detected_or, detected_t)
 * - Container traits (has_begin, has_end, has_size, has_empty, has_reserve, has_data)
 * - Composite traits (is_iterable, is_sized, is_container, is_reservable)
 * - Comparison traits (is_hashable, is_equality_comparable, is_fully_ordered)
 * - Library type detection (is_atomic, is_scoped_enum, is_transparent)
 * - Policy detection (has_validate, has_shared_locking, is_lock_free_policy)
 * - Range traits (has_rbegin, has_rend, is_reverse_iterable)
 * - Access traits (has_subscript, has_at, is_random_accessible)
 * - Serialization traits (has_serialize, has_deserialize, is_serializable)
 * - Allocator traits (is_allocator, has_allocator_type, has_rebind)
 * - Callable traits (is_invocable, is_invocable_r, is_nothrow_invocable)
 * - Container operations (has_clear, has_push_back, has_emplace_back)
 * - Advanced traits (is_contiguous_container, is_trivially_relocatable)
 * - Trait composition (all_of, any_of, none_of)
 * - Diagnostic helpers (why_not_container, why_not_hashable, etc.)
 * 
 * @note All traits are constexpr and have zero runtime overhead
 * @note Compatible with C++17 and later (conditional C++20/23 support)
 * @note Thread-safe: All operations are compile-time only
 * @note Header-only, no dependencies beyond standard library
 * 
 * @section quick_ref Quick Reference
 * 
 * @subsection container_checks Container Checks
 * @code
 * is_iterable_v<T>          // Has begin()/end()
 * is_sized_v<T>             // Has size()
 * is_container_v<T>         // Iterable + sized
 * is_contiguous_container_v<T>  // Has data()
 * is_reservable_v<T>        // Has reserve()
 * @endcode
 * 
 * @subsection comparison_checks Comparison Checks
 * @code
 * is_hashable_v<T>          // std::hash<T> exists
 * has_less_than_v<T>        // Has operator<
 * is_fully_ordered_v<T>     // Has <, <=, >, >=
 * is_equality_comparable_v<T>  // Has operator==
 * @endcode
 * 
 * @subsection callable_checks Callable Checks
 * @code
 * is_invocable_v<F, Args...>      // Can call F(args...)
 * is_invocable_r_v<R, F, Args...> // F(args...) returns R
 * is_function_object_v<F>         // Has operator()
 * @endcode
 * 
 * @subsection trait_composition Trait Composition
 * @code
 * all_of_v<T, Trait1, Trait2>  // All traits pass
 * any_of_v<T, Trait1, Trait2>  // Any trait passes
 * none_of_v<T, Trait>          // Trait fails
 * @endcode
 * 
 * @subsection diagnostics Diagnostics
 * @code
 * why_not_container<T>::reason    // Explains failure
 * why_not_hashable<T>::reason     // Explains failure
 * diagnose_container<T>()         // Returns reason string
 * @endcode
 */

#pragma once

#include <type_traits>
#include <functional>
#include <atomic>
#include <iterator>
#include <cstddef>

#include "CppStandardDetection.h"

#if FATP_HAS_CPP20
    #include <concepts>
#endif

namespace fat_p {

namespace detail {
    /**
     * @brief void_t helper for detection idiom (C++17 compatible)
     */
    template <typename...>
    using void_t = void;

    /**
     * @brief nonesuch type for detection failures
     */
    struct nonesuch {
        ~nonesuch() = delete;
        nonesuch(nonesuch const&) = delete;
        void operator=(nonesuch const&) = delete;
    };

    /**
     * @brief detector helper - detects if Op<Args...> is well-formed
     * @tparam Default Fallback type if detection fails
     * @tparam AlwaysVoid void or substitution failure
     * @tparam Op Template template to test
     * @tparam Args Arguments to Op
     */
    template <typename Default, typename AlwaysVoid,
              template <typename...> class Op, typename... Args>
    struct detector {
        using value_t = std::false_type;
        using type = Default;
    };

    template <typename Default, template <typename...> class Op, typename... Args>
    struct detector<Default, void_t<Op<Args...>>, Op, Args...> {
        using value_t = std::true_type;
        using type = Op<Args...>;
    };

    /**
     * @brief Helper for op_value_type detection
     */
    template <typename T>
    using op_value_type = typename T::value_type;

} // namespace detail

/**
 * @brief is_detected - checks if Op<Args...> is a valid expression
 * @tparam Op Template template to test
 * @tparam Args Arguments to Op
 */
template <template <typename...> class Op, typename... Args>
using is_detected = typename detail::detector<detail::nonesuch, void, Op, Args...>::value_t;

/**
 * @brief is_detected_v - checks if Op<Args...> is a valid expression (C++17 variable template)
 * @tparam Op Template template to test
 * @tparam Args Arguments to Op
 */
template <template <typename...> class Op, typename... Args>
inline constexpr bool is_detected_v = is_detected<Op, Args...>::value;

/**
 * @brief detected_t - gets the type if detected, nonesuch otherwise
 * @tparam Op Template template to test
 * @tparam Args Arguments to Op
 */
template <template <typename...> class Op, typename... Args>
using detected_t = typename detail::detector<detail::nonesuch, void, Op, Args...>::type;

/**
 * @brief detected_or - gets the type if detected, Default otherwise
 * @tparam Default The fallback type
 * @tparam Op Template template to test
 * @tparam Args Arguments to Op
 */
template <typename Default, template <typename...> class Op, typename... Args>
using detected_or = typename detail::detector<Default, void, Op, Args...>::type;

/**
 * @brief is_detected_exact - checks if detected type matches Expected
 * @tparam Expected The expected type
 * @tparam Op Template template to test
 * @tparam Args Arguments to Op
 */
template <typename Expected, template <typename...> class Op, typename... Args>
using is_detected_exact = std::is_same<Expected, detected_t<Op, Args...>>;

/**
 * @brief is_detected_exact_v - checks if detected type matches Expected (C++17 variable template)
 * @tparam Expected The expected type
 * @tparam Op Template template to test
 * @tparam Args Arguments to Op
 */
template <typename Expected, template <typename...> class Op, typename... Args>
inline constexpr bool is_detected_exact_v = is_detected_exact<Expected, Op, Args...>::value;

/**
 * @brief is_detected_convertible - checks if detected type converts to To
 * @tparam To The target type for conversion
 * @tparam Op Template template to test
 * @tparam Args Arguments to Op
 */
template <typename To, template <typename...> class Op, typename... Args>
using is_detected_convertible = std::is_convertible<detected_t<Op, Args...>, To>;

/**
 * @brief is_detected_convertible_v - checks if detected type converts to To (C++17 variable
 *        template)
 * @tparam To The target type for conversion
 * @tparam Op Template template to test
 * @tparam Args Arguments to Op
 */
template <typename To, template <typename...> class Op, typename... Args>
inline constexpr bool is_detected_convertible_v = is_detected_convertible<To, Op, Args...>::value;

/**
 * @brief has_begin - detects if T has begin() method or std::begin works
 * @tparam T The type to check
 */
template <typename T, typename = void>
struct has_begin : std::false_type {};

template <typename T>
struct has_begin<T, detail::void_t<decltype(std::begin(std::declval<T&>()))>> 
    : std::true_type {};

/**
 * @brief has_begin_v - variable template for has_begin
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool has_begin_v = has_begin<T>::value;

/**
 * @brief has_end - detects if T has end() method or std::end works
 * @tparam T The type to check
 */
template <typename T, typename = void>
struct has_end : std::false_type {};

template <typename T>
struct has_end<T, detail::void_t<decltype(std::end(std::declval<T&>()))>> 
    : std::true_type {};

/**
 * @brief has_end_v - variable template for has_end
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool has_end_v = has_end<T>::value;

/**
 * @brief has_size - detects if T has size() method
 * @tparam T The type to check
 */
template <typename T, typename = void>
struct has_size : std::false_type {};

template <typename T>
struct has_size<T, detail::void_t<decltype(std::declval<T&>().size())>> 
    : std::true_type {};

/**
 * @brief has_size_v - variable template for has_size
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool has_size_v = has_size<T>::value;

/**
 * @brief has_data - detects if T has data() method
 * @tparam T The type to check
 */
template <typename T, typename = void>
struct has_data : std::false_type {};

template <typename T>
struct has_data<T, detail::void_t<decltype(std::declval<T&>().data())>> 
    : std::true_type {};

/**
 * @brief has_data_v - variable template for has_data
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool has_data_v = has_data<T>::value;

/**
 * @brief has_empty - detects if T has empty() method
 * @tparam T The type to check
 */
template <typename T, typename = void>
struct has_empty : std::false_type {};

template <typename T>
struct has_empty<T, detail::void_t<decltype(std::declval<T&>().empty())>> 
    : std::true_type {};

/**
 * @brief has_empty_v - variable template for has_empty
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool has_empty_v = has_empty<T>::value;

/**
 * @brief has_reserve - detects if T has reserve() method
 * @tparam T The type to check
 */
template <typename T, typename = void>
struct has_reserve : std::false_type {};

template <typename T>
struct has_reserve<T, detail::void_t<decltype(std::declval<T&>().reserve(
    std::declval<std::size_t>()))>> : std::true_type {};

/**
 * @brief has_reserve_v - variable template for has_reserve
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool has_reserve_v = has_reserve<T>::value;

/**
 * @brief has_rbegin - detects if T supports rbegin()
 * @tparam T The type to check
 */
template <typename T, typename = void>
struct has_rbegin : std::false_type {};

template <typename T>
struct has_rbegin<T, detail::void_t<decltype(std::rbegin(std::declval<T&>()))>> 
    : std::true_type {};

/**
 * @brief has_rbegin_v - variable template for has_rbegin
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool has_rbegin_v = has_rbegin<T>::value;

/**
 * @brief has_rend - detects if T supports rend()
 * @tparam T The type to check
 */
template <typename T, typename = void>
struct has_rend : std::false_type {};

template <typename T>
struct has_rend<T, detail::void_t<decltype(std::rend(std::declval<T&>()))>> 
    : std::true_type {};

/**
 * @brief has_rend_v - variable template for has_rend
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool has_rend_v = has_rend<T>::value;

/**
 * @brief is_iterable - checks if T can be iterated (has begin and end)
 * @tparam T The type to check
 */
template <typename T>
struct is_iterable : std::conjunction<has_begin<T>, has_end<T>> {};

/**
 * @brief is_iterable_v - variable template for is_iterable
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool is_iterable_v = is_iterable<T>::value;

/**
 * @brief is_sized - checks if T has size() method
 * @tparam T The type to check
 */
template <typename T>
struct is_sized : has_size<T> {};

/**
 * @brief is_sized_v - variable template for is_sized
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool is_sized_v = is_sized<T>::value;

/**
 * @brief is_container - checks if T is a container (iterable + sized)
 * @tparam T The type to check
 */
template <typename T>
struct is_container : std::conjunction<is_iterable<T>, is_sized<T>> {};

/**
 * @brief is_container_v - variable template for is_container
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool is_container_v = is_container<T>::value;

/**
 * @brief is_reservable - checks if T has reserve() (e.g., vector, string)
 * @tparam T The type to check
 */
template <typename T>
struct is_reservable : has_reserve<T> {};

/**
 * @brief is_reservable_v - variable template for is_reservable
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool is_reservable_v = is_reservable<T>::value;

/**
 * @brief is_reverse_iterable - checks if T supports reverse iteration
 * @tparam T The type to check
 */
template <typename T>
struct is_reverse_iterable : std::conjunction<has_rbegin<T>, has_rend<T>> {};

/**
 * @brief is_reverse_iterable_v - variable template for is_reverse_iterable
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool is_reverse_iterable_v = is_reverse_iterable<T>::value;

#if FATP_HAS_CPP20

/**
 * @namespace concepts
 * @brief C++20 concepts for type constraints
 * 
 * @details These concepts provide cleaner syntax for template constraints
 * while the struct-based traits remain available for use with std::conjunction
 * and other metaprogramming utilities.
 */
namespace concepts {

/**
 * @brief C++20 concept: checks if T can be iterated (has begin and end returning iterators)
 * @tparam T The type to check
 */
template<typename T>
concept Iterable = requires(T val) {
    { std::begin(val) } -> std::input_or_output_iterator;
    { std::end(val) } -> std::input_or_output_iterator;
};

/**
 * @brief C++20 concept: checks if T has a size() method returning an integral
 * @tparam T The type to check
 */
template<typename T>
concept Sized = requires(T val) {
    { val.size() } -> std::integral;
};

/**
 * @brief C++20 concept: checks if T is a container (iterable + sized)
 * @tparam T The type to check
 */
template<typename T>
concept Container = Iterable<T> && Sized<T>;

/**
 * @brief C++20 concept: checks if T supports reverse iteration
 * @tparam T The type to check
 */
template<typename T>
concept ReverseIterable = requires(T val) {
    { std::rbegin(val) } -> std::input_or_output_iterator;
    { std::rend(val) } -> std::input_or_output_iterator;
};

/**
 * @brief C++20 concept: checks if T has reserve() method
 * @tparam T The type to check
 */
template<typename T>
concept Reservable = requires(T val) {
    val.reserve(std::size_t{});
};

/**
 * @brief C++20 concept: checks if T is a contiguous container
 * @tparam T The type to check
 */
template<typename T>
concept ContiguousContainer = Container<T> && requires(T val) {
    { val.data() };
};

/**
 * @brief C++20 concept: checks if T is hashable
 * @tparam T The type to check
 */
template<typename T>
concept Hashable = requires(T val) {
    { std::hash<std::remove_cv_t<T>>{}(val) } -> std::convertible_to<std::size_t>;
};

/**
 * @brief C++20 concept: checks if T is equality comparable
 * @tparam T The type to check
 */
template<typename T>
concept EqualityComparable = requires(T a, T b) {
    { a == b } -> std::convertible_to<bool>;
};

/**
 * @brief C++20 concept: checks if T is fully ordered
 * @tparam T The type to check
 */
template<typename T>
concept FullyOrdered = requires(T a, T b) {
    { a < b } -> std::convertible_to<bool>;
    { a <= b } -> std::convertible_to<bool>;
    { a > b } -> std::convertible_to<bool>;
    { a >= b } -> std::convertible_to<bool>;
};

} // namespace concepts

#endif // FATP_HAS_CPP20

/**
 * @brief has_subscript - detects if T supports operator[]
 * @tparam T The type to check
 */
template <typename T, typename = void>
struct has_subscript : std::false_type {};

template <typename T>
struct has_subscript<T, detail::void_t<decltype(std::declval<T&>()[std::declval<std::size_t>()])>> 
    : std::true_type {};

/**
 * @brief has_subscript_v - variable template for has_subscript
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool has_subscript_v = has_subscript<T>::value;

/**
 * @brief has_at - detects if T has at() method
 * @tparam T The type to check
 */
template <typename T, typename = void>
struct has_at : std::false_type {};

template <typename T>
struct has_at<T, detail::void_t<decltype(std::declval<T&>().at(std::declval<std::size_t>()))>> 
    : std::true_type {};

/**
 * @brief has_at_v - variable template for has_at
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool has_at_v = has_at<T>::value;

/**
 * @brief is_random_accessible - checks if T supports random access (subscript + size)
 * @tparam T The type to check
 */
template <typename T>
struct is_random_accessible : std::conjunction<has_subscript<T>, has_size<T>> {};

/**
 * @brief is_random_accessible_v - variable template for is_random_accessible
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool is_random_accessible_v = is_random_accessible<T>::value;

/**
 * @brief has_clear - detects if T has clear() method
 * @tparam T The type to check
 */
template <typename T, typename = void>
struct has_clear : std::false_type {};

template <typename T>
struct has_clear<T, detail::void_t<decltype(std::declval<T&>().clear())>> 
    : std::true_type {};

/**
 * @brief has_clear_v - variable template for has_clear
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool has_clear_v = has_clear<T>::value;

/**
 * @brief has_push_back - detects if T has push_back() method
 * @tparam T The type to check
 */
template <typename T, typename = void>
struct has_push_back : std::false_type {};

template <typename T>
struct has_push_back<T, detail::void_t<decltype(
    std::declval<T&>().push_back(std::declval<typename T::value_type>()))>>
    : std::true_type {};

/**
 * @brief has_push_back_v - variable template for has_push_back
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool has_push_back_v = has_push_back<T>::value;

/**
 * @brief has_emplace_back - detects if T has emplace_back() method
 * @tparam T The type to check
 */
template <typename T, typename = void>
struct has_emplace_back : std::false_type {};

template <typename T>
struct has_emplace_back<T, detail::void_t<decltype(
    std::declval<T&>().emplace_back())>>
    : std::true_type {};

/**
 * @brief has_emplace_back_v - variable template for has_emplace_back
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool has_emplace_back_v = has_emplace_back<T>::value;

/**
 * @brief has_push_front - detects if T has push_front() method
 * @tparam T The type to check
 */
template <typename T, typename = void>
struct has_push_front : std::false_type {};

template <typename T>
struct has_push_front<T, detail::void_t<decltype(
    std::declval<T&>().push_front(std::declval<typename T::value_type>()))>>
    : std::true_type {};

/**
 * @brief has_push_front_v - variable template for has_push_front
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool has_push_front_v = has_push_front<T>::value;

/**
 * @brief is_contiguous_container - checks if T is a contiguous container (has data())
 * @tparam T The type to check
 */
template <typename T>
struct is_contiguous_container : std::conjunction<
    is_container<T>,
    has_data<T>
> {};

/**
 * @brief is_contiguous_container_v - variable template for is_contiguous_container
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool is_contiguous_container_v = is_contiguous_container<T>::value;

namespace detail {
    template <typename T>
    struct is_pair_impl : std::false_type {};

    template <typename T1, typename T2>
    struct is_pair_impl<std::pair<T1, T2>> : std::true_type {};

    template <typename T>
    using is_pair = is_pair_impl<std::decay_t<T>>;

    template <typename T>
    struct has_pair_value_type {
        static constexpr bool value = is_detected_v<op_value_type, T> &&
                                      is_pair<detected_t<op_value_type, T>>::value;
    };

} // namespace detail

/**
 * @brief is_map_like - checks if T is a map (iterable with pair value_type)
 * @tparam T The type to check
 */
template <typename T>
struct is_map_like : std::conjunction<
    is_iterable<T>,
    detail::has_pair_value_type<std::decay_t<T>>
> {};

/**
 * @brief is_map_like_v - variable template for is_map_like
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool is_map_like_v = is_map_like<T>::value;

/**
 * @brief is_hashable - detects if std::hash<T> is defined
 * @tparam T The type to check
 */
template <typename T, typename = void>
struct is_hashable : std::false_type {};

template <typename T>
struct is_hashable<T, detail::void_t<decltype(
    std::hash<std::remove_cv_t<T>>{}(std::declval<T>()))>> : std::true_type {};

/**
 * @brief is_hashable_v - variable template for is_hashable
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool is_hashable_v = is_hashable<T>::value;

/**
 * @brief is_equality_comparable - detects if T has operator==
 * @tparam T The type to check
 */
template <typename T, typename = void>
struct is_equality_comparable : std::false_type {};

template <typename T>
struct is_equality_comparable<T, detail::void_t<decltype(
    std::declval<T>() == std::declval<T>())>> : std::true_type {};

/**
 * @brief is_equality_comparable_v - variable template for is_equality_comparable
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool is_equality_comparable_v = is_equality_comparable<T>::value;

/**
 * @brief is_inequality_comparable - detects if T has operator!=
 * @tparam T The type to check
 */
template <typename T, typename = void>
struct is_inequality_comparable : std::false_type {};

template <typename T>
struct is_inequality_comparable<T, detail::void_t<decltype(
    std::declval<T>() != std::declval<T>())>> : std::true_type {};

/**
 * @brief is_inequality_comparable_v - variable template for is_inequality_comparable
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool is_inequality_comparable_v = is_inequality_comparable<T>::value;

/**
 * @brief has_less - detects if T has operator<
 * @tparam T The type to check
 */
template <typename T, typename = void>
struct has_less : std::false_type {};

template <typename T>
struct has_less<T, detail::void_t<decltype(
    std::declval<T>() < std::declval<T>())>> : std::true_type {};

/**
 * @brief has_less_v - variable template for has_less
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool has_less_v = has_less<T>::value;

/**
 * @brief has_less_equal - detects if T has operator<=
 * @tparam T The type to check
 */
template <typename T, typename = void>
struct has_less_equal : std::false_type {};

template <typename T>
struct has_less_equal<T, detail::void_t<decltype(
    std::declval<T>() <= std::declval<T>())>> : std::true_type {};

/**
 * @brief has_less_equal_v - variable template for has_less_equal
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool has_less_equal_v = has_less_equal<T>::value;

/**
 * @brief has_greater - detects if T has operator>
 * @tparam T The type to check
 */
template <typename T, typename = void>
struct has_greater : std::false_type {};

template <typename T>
struct has_greater<T, detail::void_t<decltype(
    std::declval<T>() > std::declval<T>())>> : std::true_type {};

/**
 * @brief has_greater_v - variable template for has_greater
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool has_greater_v = has_greater<T>::value;

/**
 * @brief has_greater_equal - detects if T has operator>=
 * @tparam T The type to check
 */
template <typename T, typename = void>
struct has_greater_equal : std::false_type {};

template <typename T>
struct has_greater_equal<T, detail::void_t<decltype(
    std::declval<T>() >= std::declval<T>())>> : std::true_type {};

/**
 * @brief has_greater_equal_v - variable template for has_greater_equal
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool has_greater_equal_v = has_greater_equal<T>::value;

/**
 * @brief is_fully_ordered - checks if T has all comparison operators (<, <=, >, >=)
 * @tparam T The type to check
 */
template <typename T>
struct is_fully_ordered : std::conjunction<
    has_less<T>,
    has_less_equal<T>,
    has_greater<T>,
    has_greater_equal<T>
> {};

/**
 * @brief is_fully_ordered_v - variable template for is_fully_ordered
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool is_fully_ordered_v = is_fully_ordered<T>::value;

/**
 * @brief has_less_than - detects if T has operator< (for ordering)
 * @tparam T The type to check
 */
template <typename T, typename = void>
struct has_less_than : std::false_type {};

template <typename T>
struct has_less_than<T, detail::void_t<decltype(
    std::declval<T>() < std::declval<T>())>> : std::true_type {};

/**
 * @brief has_less_than_v - variable template for has_less_than
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool has_less_than_v = has_less_than<T>::value;

#if FATP_HAS_CPP20
/**
 * @brief is_three_way_comparable - detects if T has operator<=> (C++20)
 * @tparam T The type to check
 */
template <typename T, typename = void>
struct is_three_way_comparable : std::false_type {};

template <typename T>
struct is_three_way_comparable<T, detail::void_t<decltype(
    std::declval<T>() <=> std::declval<T>())>> : std::true_type {};

/**
 * @brief is_three_way_comparable_v - variable template for is_three_way_comparable
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool is_three_way_comparable_v = is_three_way_comparable<T>::value;
#endif

/**
 * @brief is_valid_comparator - checks if Comp can compare T objects
 * @tparam Comp The comparator type
 * @tparam T The type being compared
 */
template <typename Comp, typename T, typename = void>
struct is_valid_comparator : std::false_type {};

template <typename Comp, typename T>
struct is_valid_comparator<Comp, T, detail::void_t<
    decltype(std::declval<Comp>()(std::declval<T>(), std::declval<T>()))
>> : std::true_type {};

/**
 * @brief is_valid_comparator_v - variable template for is_valid_comparator
 * @tparam Comp The comparator type
 * @tparam T The type being compared
 */
template <typename Comp, typename T>
inline constexpr bool is_valid_comparator_v = is_valid_comparator<Comp, T>::value;

/**
 * @brief is_transparent - detects if T has is_transparent tag (for heterogeneous lookup)
 * @tparam T The type to check
 */
template <typename T, typename = void>
struct is_transparent : std::false_type {};

template <typename T>
struct is_transparent<T, detail::void_t<typename T::is_transparent>> 
    : std::true_type {};

/**
 * @brief is_transparent_v - variable template for is_transparent
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool is_transparent_v = is_transparent<T>::value;

/**
 * @brief is_atomic - detects if T is std::atomic<U> for some U
 * @tparam T The type to check
 */
template <typename T>
struct is_atomic : std::false_type {};

template <typename T>
struct is_atomic<std::atomic<T>> : std::true_type {};

/**
 * @brief is_atomic_v - variable template for is_atomic
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool is_atomic_v = is_atomic<T>::value;

/**
 * @brief is_scoped_enum - detects if T is a scoped enum (enum class)
 * @tparam T The type to check
 */
template <typename T>
struct is_scoped_enum : std::conjunction<
    std::is_enum<T>,
    std::negation<std::is_convertible<T, int>>
> {};

/**
 * @brief is_scoped_enum_v - variable template for is_scoped_enum
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool is_scoped_enum_v = is_scoped_enum<T>::value;

/**
 * @brief has_validate_t - detection helper for validate() method
 * @tparam T The policy type
 */
template <typename T>
using has_validate_t = decltype(std::declval<T&>().validate());

/**
 * @brief has_validate - detects validate() method in policies
 * @tparam T The policy type
 */
template <typename T>
struct has_validate : is_detected<has_validate_t, T> {};

/**
 * @brief has_validate_v - variable template for has_validate
 * @tparam T The policy type
 */
template <typename T>
inline constexpr bool has_validate_v = has_validate<T>::value;

/**
 * @brief has_shared_locking - detects SharedGuard type in concurrency policies
 * @tparam T The policy type
 */
template <typename T, typename = void>
struct has_shared_locking : std::false_type {};

template <typename T>
struct has_shared_locking<T, detail::void_t<typename T::SharedGuard>> : std::true_type {};

/**
 * @brief has_shared_locking_v - variable template for has_shared_locking
 * @tparam T The policy type
 */
template <typename T>
inline constexpr bool has_shared_locking_v = has_shared_locking<T>::value;

/**
 * @brief is_lock_free_policy - detects LockFreeTag in policies
 * @tparam T The policy type
 */
template <typename T, typename = void>
struct is_lock_free_policy : std::false_type {};

template <typename T>
struct is_lock_free_policy<T, detail::void_t<typename T::LockFreeTag>> : std::true_type {};

/**
 * @brief is_lock_free_policy_v - variable template for is_lock_free_policy
 * @tparam T The policy type
 */
template <typename T>
inline constexpr bool is_lock_free_policy_v = is_lock_free_policy<T>::value;

/**
 * @brief has_serialize_t - detection helper for serialize() method
 * @tparam T The type to check
 */
template <typename T>
using has_serialize_t = decltype(std::declval<T&>().serialize(
    std::declval<std::ostream&>()));

/**
 * @brief has_serialize - detects if T has serialize() method
 * @tparam T The type to check
 */
template <typename T>
struct has_serialize : is_detected<has_serialize_t, T> {};

/**
 * @brief has_serialize_v - variable template for has_serialize
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool has_serialize_v = has_serialize<T>::value;

/**
 * @brief has_deserialize_t - detection helper for deserialize() static method
 * @tparam T The type to check
 */
template <typename T>
using has_deserialize_t = decltype(T::deserialize(std::declval<std::istream&>()));

/**
 * @brief has_deserialize - detects if T has static deserialize() method
 * @tparam T The type to check
 */
template <typename T>
struct has_deserialize : is_detected<has_deserialize_t, T> {};

/**
 * @brief has_deserialize_v - variable template for has_deserialize
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool has_deserialize_v = has_deserialize<T>::value;

/**
 * @brief is_serializable - checks if T supports both serialize and deserialize
 * @tparam T The type to check
 */
template <typename T>
struct is_serializable : std::conjunction<has_serialize<T>, has_deserialize<T>> {};

/**
 * @brief is_serializable_v - variable template for is_serializable
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool is_serializable_v = is_serializable<T>::value;

/**
 * @brief has_allocator_type - detects if T has allocator_type member
 * @tparam T The type to check
 */
template <typename T, typename = void>
struct has_allocator_type : std::false_type {};

template <typename T>
struct has_allocator_type<T, detail::void_t<typename T::allocator_type>> : std::true_type {};

/**
 * @brief has_allocator_type_v - variable template for has_allocator_type
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool has_allocator_type_v = has_allocator_type<T>::value;

/**
 * @brief is_allocator - detects if T satisfies allocator requirements
 * @tparam T The type to check
 */
template <typename T, typename = void>
struct is_allocator : std::false_type {};

template <typename T>
struct is_allocator<T, detail::void_t<
    typename T::value_type,
    decltype(std::declval<T&>().allocate(std::declval<std::size_t>())),
    decltype(std::declval<T&>().deallocate(
        std::declval<typename T::value_type*>(), std::declval<std::size_t>()))
>> : std::true_type {};

/**
 * @brief is_allocator_v - variable template for is_allocator
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool is_allocator_v = is_allocator<T>::value;

/**
 * @brief has_rebind - detects if allocator has rebind member template
 * @tparam T The allocator type to check
 */
template <typename T, typename = void>
struct has_rebind : std::false_type {};

template <typename T>
struct has_rebind<T, detail::void_t<typename T::template rebind<int>>> : std::true_type {};

/**
 * @brief has_rebind_v - variable template for has_rebind
 * @tparam T The allocator type to check
 */
template <typename T>
inline constexpr bool has_rebind_v = has_rebind<T>::value;

/**
 * @brief is_invocable - checks if F can be invoked with Args
 * @tparam F The callable type
 * @tparam Args The argument types
 */
template <typename F, typename... Args>
struct is_invocable : std::is_invocable<F, Args...> {};

/**
 * @brief is_invocable_v - variable template for is_invocable
 * @tparam F The callable type
 * @tparam Args The argument types
 */
template <typename F, typename... Args>
inline constexpr bool is_invocable_v = is_invocable<F, Args...>::value;

/**
 * @brief is_invocable_r - checks if F returns R when invoked with Args
 * @tparam R The expected return type
 * @tparam F The callable type
 * @tparam Args The argument types
 */
template <typename R, typename F, typename... Args>
struct is_invocable_r : std::is_invocable_r<R, F, Args...> {};

/**
 * @brief is_invocable_r_v - variable template for is_invocable_r
 * @tparam R The expected return type
 * @tparam F The callable type
 * @tparam Args The argument types
 */
template <typename R, typename F, typename... Args>
inline constexpr bool is_invocable_r_v = is_invocable_r<R, F, Args...>::value;

/**
 * @brief is_nothrow_invocable - checks if F can be invoked with Args without throwing
 * @tparam F The callable type
 * @tparam Args The argument types
 */
template <typename F, typename... Args>
struct is_nothrow_invocable : std::is_nothrow_invocable<F, Args...> {};

/**
 * @brief is_nothrow_invocable_v - variable template for is_nothrow_invocable
 * @tparam F The callable type
 * @tparam Args The argument types
 */
template <typename F, typename... Args>
inline constexpr bool is_nothrow_invocable_v = is_nothrow_invocable<F, Args...>::value;

/**
 * @brief is_function_object - detects if T is a function object (has operator())
 * @tparam T The type to check
 */
template <typename T, typename = void>
struct is_function_object : std::false_type {};

template <typename T>
struct is_function_object<T, detail::void_t<decltype(&T::operator())>> : std::true_type {};

/**
 * @brief is_function_object_v - variable template for is_function_object
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool is_function_object_v = is_function_object<T>::value;

#if FATP_HAS_CPP17
/**
 * @brief is_aggregate - checks if T is an aggregate type (C++17)
 * @tparam T The type to check
 */
template <typename T>
struct is_aggregate : std::is_aggregate<T> {};

/**
 * @brief is_aggregate_v - variable template for is_aggregate
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool is_aggregate_v = is_aggregate<T>::value;
#endif

/**
 * @brief is_bounded_array - checks if T is an array with known bound
 * @tparam T The type to check
 */
template <typename T>
struct is_bounded_array : std::is_array<T> {
    static constexpr bool value = std::is_array_v<T> && std::extent_v<T> > 0;
};

/**
 * @brief is_bounded_array_v - variable template for is_bounded_array
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool is_bounded_array_v = is_bounded_array<T>::value;

/**
 * @brief is_unbounded_array - checks if T is an array with unknown bound
 * @tparam T The type to check
 */
template <typename T>
struct is_unbounded_array : std::is_array<T> {
    static constexpr bool value = std::is_array_v<T> && std::extent_v<T> == 0;
};

/**
 * @brief is_unbounded_array_v - variable template for is_unbounded_array
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool is_unbounded_array_v = is_unbounded_array<T>::value;

/**
 * @brief has_tuple_size - detects if std::tuple_size works on T
 * @tparam T The type to check
 */
template <typename T, typename = void>
struct has_tuple_size : std::false_type {};

template <typename T>
struct has_tuple_size<T, detail::void_t<decltype(std::tuple_size<T>::value)>> : std::true_type {};

/**
 * @brief has_tuple_size_v - variable template for has_tuple_size
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool has_tuple_size_v = has_tuple_size<T>::value;

/**
 * @brief has_tuple_element - detects if std::tuple_element works on T
 * @tparam T The type to check
 */
template <typename T, typename = void>
struct has_tuple_element : std::false_type {};

template <typename T>
struct has_tuple_element<T, detail::void_t<typename std::tuple_element<0, T>::type>> 
    : std::true_type {};

/**
 * @brief has_tuple_element_v - variable template for has_tuple_element
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool has_tuple_element_v = has_tuple_element<T>::value;

/**
 * @brief has_get - detects if std::get works on T
 * @tparam T The type to check
 */
template <typename T, typename = void>
struct has_get : std::false_type {};

template <typename T>
struct has_get<T, detail::void_t<decltype(std::get<0>(std::declval<T>()))>> : std::true_type {};

/**
 * @brief has_get_v - variable template for has_get
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool has_get_v = has_get<T>::value;

/**
 * @brief is_tuple_like - checks if T supports tuple protocol (tuple_size, tuple_element, get)
 * @tparam T The type to check
 */
template <typename T>
struct is_tuple_like : std::conjunction<
    has_tuple_size<T>,
    has_tuple_element<T>,
    has_get<T>
> {};

/**
 * @brief is_tuple_like_v - variable template for is_tuple_like
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool is_tuple_like_v = is_tuple_like<T>::value;

/**
 * @brief has_iterator_category - detects if T has iterator traits
 * @tparam T The type to check
 */
template <typename T, typename = void>
struct has_iterator_category : std::false_type {};

template <typename T>
struct has_iterator_category<T, detail::void_t<
    typename std::iterator_traits<T>::iterator_category>> : std::true_type {};

/**
 * @brief has_iterator_category_v - variable template for has_iterator_category
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool has_iterator_category_v = has_iterator_category<T>::value;

/**
 * @brief has_c_str - detects if T has c_str() method
 * @tparam T The type to check
 */
template <typename T, typename = void>
struct has_c_str : std::false_type {};

template <typename T>
struct has_c_str<T, detail::void_t<decltype(std::declval<T&>().c_str())>> : std::true_type {};

/**
 * @brief has_c_str_v - variable template for has_c_str
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool has_c_str_v = has_c_str<T>::value;

/**
 * @brief is_string_like - checks if T behaves like a string (iterable + c_str)
 * @tparam T The type to check
 */
template <typename T>
struct is_string_like : std::conjunction<
    is_iterable<T>,
    has_c_str<T>
> {};

/**
 * @brief is_string_like_v - variable template for is_string_like
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool is_string_like_v = is_string_like<T>::value;

/**
 * @brief has_has_value - detects if T has has_value() method
 * @tparam T The type to check
 */
template <typename T, typename = void>
struct has_has_value : std::false_type {};

template <typename T>
struct has_has_value<T, detail::void_t<decltype(std::declval<T&>().has_value())>> 
    : std::true_type {};

/**
 * @brief has_has_value_v - variable template for has_has_value
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool has_has_value_v = has_has_value<T>::value;

/**
 * @brief has_value_method - detects if T has value() method
 * @tparam T The type to check
 */
template <typename T, typename = void>
struct has_value_method : std::false_type {};

template <typename T>
struct has_value_method<T, detail::void_t<decltype(std::declval<T&>().value())>> 
    : std::true_type {};

/**
 * @brief has_value_method_v - variable template for has_value_method
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool has_value_method_v = has_value_method<T>::value;

/**
 * @brief is_optional_like - checks if T behaves like std::optional
 * @tparam T The type to check
 */
template <typename T>
struct is_optional_like : std::conjunction<
    has_has_value<T>,
    has_value_method<T>
> {};

/**
 * @brief is_optional_like_v - variable template for is_optional_like
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool is_optional_like_v = is_optional_like<T>::value;

/**
 * @brief has_index_method - detects if T has index() method
 * @tparam T The type to check
 */
template <typename T, typename = void>
struct has_index_method : std::false_type {};

template <typename T>
struct has_index_method<T, detail::void_t<decltype(std::declval<T&>().index())>> 
    : std::true_type {};

/**
 * @brief has_index_method_v - variable template for has_index_method
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool has_index_method_v = has_index_method<T>::value;

/**
 * @brief is_variant_like - checks if T behaves like std::variant
 * @tparam T The type to check
 */
template <typename T>
struct is_variant_like : has_index_method<T> {};

/**
 * @brief is_variant_like_v - variable template for is_variant_like
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool is_variant_like_v = is_variant_like<T>::value;

/**
 * @brief is_range - checks if T can be used as a range (has begin/end)
 * @tparam T The type to check
 */
template <typename T>
struct is_range : is_iterable<T> {};

/**
 * @brief is_range_v - variable template for is_range
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool is_range_v = is_range<T>::value;

/**
 * @brief is_sized_range - checks if T is a range with size()
 * @tparam T The type to check
 */
template <typename T>
struct is_sized_range : std::conjunction<is_iterable<T>, has_size<T>> {};

/**
 * @brief is_sized_range_v - variable template for is_sized_range
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool is_sized_range_v = is_sized_range<T>::value;

/**
 * @brief is_trivially_relocatable - checks if T can be relocated with memcpy
 * @tparam T The type to check
 * 
 * @note Uses std::is_trivially_copyable as the implementation, which guarantees
 * that T has a trivial destructor, trivial copy constructor, and trivial move
 * constructor (if applicable). This is a safe and conservative check for types
 * that can be relocated by simply copying their byte representation.
 * 
 * @note For C++23 and later, this may be replaced with std::is_trivially_relocatable
 * when that proposal is standardized.
 */
template<typename T>
struct is_trivially_relocatable : std::is_trivially_copyable<T> {};

/**
 * @brief is_trivially_relocatable_v - variable template for is_trivially_relocatable
 * @tparam T The type to check
 */
template<typename T>
inline constexpr bool is_trivially_relocatable_v = is_trivially_relocatable<T>::value;

/**
 * @namespace trait_ops
 * @brief Trait composition helpers for combining multiple traits
 */
namespace trait_ops {

/**
 * @brief all_of - checks if all traits are satisfied
 * @tparam Traits... The traits to check
 */
template<template<typename> class... Traits>
struct all_of {
    template<typename T>
    struct apply : std::conjunction<Traits<T>...> {};
};

/**
 * @brief all_of_v - variable template for all_of
 * @tparam T The type to check
 * @tparam Traits... The traits to check
 */
template<typename T, template<typename> class... Traits>
inline constexpr bool all_of_v = all_of<Traits...>::template apply<T>::value;

/**
 * @brief any_of - checks if any trait is satisfied
 * @tparam Traits... The traits to check
 */
template<template<typename> class... Traits>
struct any_of {
    template<typename T>
    struct apply : std::disjunction<Traits<T>...> {};
};

/**
 * @brief any_of_v - variable template for any_of
 * @tparam T The type to check
 * @tparam Traits... The traits to check
 */
template<typename T, template<typename> class... Traits>
inline constexpr bool any_of_v = any_of<Traits...>::template apply<T>::value;

/**
 * @brief none_of - checks if no traits are satisfied
 * @tparam Trait The trait to negate
 */
template<template<typename> class Trait>
struct none_of {
    template<typename T>
    struct apply : std::negation<Trait<T>> {};
};

/**
 * @brief none_of_v - variable template for none_of
 * @tparam T The type to check
 * @tparam Trait The trait to negate
 */
template<typename T, template<typename> class Trait>
inline constexpr bool none_of_v = none_of<Trait>::template apply<T>::value;

} // namespace trait_ops

/**
 * @namespace diagnostics
 * @brief Diagnostic helpers that explain why type traits fail
 */
namespace diagnostics {

/**
 * @brief why_not_container - Explains why T is not a container
 * @tparam T The type to diagnose
 * 
 * @details Provides a human-readable reason for container trait failure
 * 
 * Example:
 * @code
 * static_assert(is_container_v<MyType>, why_not_container<MyType>::reason);
 * @endcode
 */
template<typename T>
struct why_not_container {
    static constexpr const char* reason =
        !has_begin_v<T> ? "Type lacks begin() method or std::begin support" :
        !has_end_v<T> ? "Type lacks end() method or std::end support" :
        !has_size_v<T> ? "Type lacks size() method" :
        "Type satisfies container requirements";
};

/**
 * @brief why_not_hashable - Explains why T is not hashable
 * @tparam T The type to diagnose
 */
template<typename T>
struct why_not_hashable {
    static constexpr const char* reason =
        !is_hashable_v<T> ? "std::hash<T> specialization not found or not valid" :
        "Type is hashable";
};

/**
 * @brief why_not_serializable - Explains why T is not serializable
 * @tparam T The type to diagnose
 */
template<typename T>
struct why_not_serializable {
    static constexpr const char* reason =
        !has_serialize_v<T> ? "Type lacks serialize(std::ostream&) method" :
        !has_deserialize_v<T> ? "Type lacks static deserialize(std::istream&) method" :
        "Type is serializable";
};

/**
 * @brief why_not_comparable - Explains why T is not comparable
 * @tparam T The type to diagnose
 */
template<typename T>
struct why_not_comparable {
    static constexpr const char* reason =
        !has_less_v<T> ? "Type lacks operator<" :
        "Type is comparable";
};

/**
 * @brief diagnose_container - Runtime diagnostic function
 * @tparam T The type to diagnose
 * @return const char* Reason string
 * 
 * @details Can be used at compile-time or runtime for diagnostics
 * 
 * Example:
 * @code
 * std::cout << "Why not container: " << diagnose_container<int>() << "\n";
 * @endcode
 */
template<typename T>
constexpr const char* diagnose_container() {
    return why_not_container<T>::reason;
}

/**
 * @brief diagnose_hashable - Runtime diagnostic function
 * @tparam T The type to diagnose
 * @return const char* Reason string
 */
template<typename T>
constexpr const char* diagnose_hashable() {
    return why_not_hashable<T>::reason;
}

/**
 * @brief diagnose_serializable - Runtime diagnostic function
 * @tparam T The type to diagnose
 * @return const char* Reason string
 */
template<typename T>
constexpr const char* diagnose_serializable() {
    return why_not_serializable<T>::reason;
}

/**
 * @brief diagnose_comparable - Runtime diagnostic function
 * @tparam T The type to diagnose
 * @return const char* Reason string
 */
template<typename T>
constexpr const char* diagnose_comparable() {
    return why_not_comparable<T>::reason;
}

} // namespace diagnostics

/**
 * @namespace extension_points
 * @brief Extension points for user-defined type traits
 */
namespace extension_points {
    
    /**
     * @brief custom_traits - specialization point for user-defined traits
     * @tparam T The type to provide custom traits for
     * 
     * @details Users can specialize this template to provide custom trait information
     * 
     * Example:
     * @code
     * namespace fat_p::extension_points {
     *     template<>
     *     struct custom_traits<MyType> {
     *         static constexpr bool is_relocatable = true;
     *         static constexpr bool has_custom_hash = true;
     *     };
     * }
     * @endcode
     */
    template<typename T>
    struct custom_traits {
    };
    
} // namespace extension_points

/**
 * @brief requires_iterable - DbC helper that enforces iterability at compile time
 * @tparam T The type to check
 */
template<typename T>
constexpr void requires_iterable() {
    static_assert(is_iterable_v<T>, 
        "[CONTRACT VIOLATION] Type must be iterable (have begin/end)");
}

/**
 * @brief requires_sized - DbC helper that enforces size() method at compile time
 * @tparam T The type to check
 */
template<typename T>
constexpr void requires_sized() {
    static_assert(is_sized_v<T>,
        "[CONTRACT VIOLATION] Type must have size() method");
}

/**
 * @brief requires_container - DbC helper that enforces container requirements at compile time
 * @tparam T The type to check
 */
template<typename T>
constexpr void requires_container() {
    static_assert(is_container_v<T>,
        "[CONTRACT VIOLATION] Type must be a container (iterable + sized)");
}

/**
 * @brief requires_hashable - DbC helper that enforces hashability at compile time
 * @tparam T The type to check
 */
template<typename T>
constexpr void requires_hashable() {
    static_assert(is_hashable_v<T>,
        "[CONTRACT VIOLATION] Type must be hashable (std::hash<T> must exist)");
}

/**
 * @brief requires_comparable - DbC helper that enforces comparability at compile time
 * @tparam T The type to check
 */
template<typename T>
constexpr void requires_comparable() {
    static_assert(has_less_than_v<T>,
        "[CONTRACT VIOLATION] Type must be comparable (operator< must exist)");
}

/**
 * @brief requires_invocable - DbC helper that enforces invocability at compile time
 * @tparam F The callable type
 * @tparam Args The argument types
 */
template<typename F, typename... Args>
constexpr void requires_invocable() {
    static_assert(is_invocable_v<F, Args...>,
        "[CONTRACT VIOLATION] Function must be invocable with given arguments");
}

/**
 * @brief requires_allocator - DbC helper that enforces allocator requirements at compile time
 * @tparam T The allocator type
 */
template<typename T>
constexpr void requires_allocator() {
    static_assert(is_allocator_v<T>,
        "[CONTRACT VIOLATION] Type must be an allocator");
}

/**
 * @brief requires_serializable - DbC helper that enforces serializability at compile time
 * @tparam T The type to check
 */
template<typename T>
constexpr void requires_serializable() {
    static_assert(is_serializable_v<T>,
        "[CONTRACT VIOLATION] Type must be serializable (have serialize/deserialize)");
}

/**
 * @brief requires_contiguous - DbC helper that enforces contiguous container at compile time
 * @tparam T The type to check
 */
template<typename T>
constexpr void requires_contiguous() {
    static_assert(is_contiguous_container_v<T>,
        "[CONTRACT VIOLATION] Type must be a contiguous container (have data())");
}

} // namespace fat_p
