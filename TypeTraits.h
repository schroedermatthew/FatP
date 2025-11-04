/**
 * @file TypeTraits.h
 * @brief Comprehensive type trait utilities for C++17/20/23
 * @version 1.1 - Complete Production Implementation
 * 
 * @details Provides:
 * - Detection idiom (is_detected, detected_or)
 * - Container traits (has_begin, has_end, has_size, has_empty, has_reserve)
 * - Composite traits (is_iterable, is_sized, is_container, is_reservable)
 * - Comparability traits (is_hashable, is_comparable, is_valid_comparator)
 * - Library type detection (is_atomic, is_expected, is_strong_id)
 * - Policy detection (has_validate, has_shared_locking, is_lock_free_policy)
 * 
 * @note All traits are constexpr and have zero runtime overhead
 * @note Compatible with C++17 and later
 */

#pragma once

#include <type_traits>
#include <functional>
#include <atomic>
#include <iterator>

namespace cpp_utilities {

// =============================================================================
// Detection Idiom Implementation
// =============================================================================

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

} // namespace detail

/**
 * @brief is_detected - checks if Op<Args...> is a valid expression
 * @tparam Op Template template to test
 * @tparam Args Arguments to Op
 */
template <template <typename...> class Op, typename... Args>
using is_detected = typename detail::detector<detail::nonesuch, void, Op, Args...>::value_t;

template <template <typename...> class Op, typename... Args>
inline constexpr bool is_detected_v = is_detected<Op, Args...>::value;

/**
 * @brief detected_t - gets the type if detected, nonesuch otherwise
 */
template <template <typename...> class Op, typename... Args>
using detected_t = typename detail::detector<detail::nonesuch, void, Op, Args...>::type;

/**
 * @brief detected_or - gets the type if detected, Default otherwise
 */
template <typename Default, template <typename...> class Op, typename... Args>
using detected_or = typename detail::detector<Default, void, Op, Args...>::type;

/**
 * @brief is_detected_exact - checks if detected type matches Expected
 */
template <typename Expected, template <typename...> class Op, typename... Args>
using is_detected_exact = std::is_same<Expected, detected_t<Op, Args...>>;

template <typename Expected, template <typename...> class Op, typename... Args>
inline constexpr bool is_detected_exact_v = is_detected_exact<Expected, Op, Args...>::value;

/**
 * @brief is_detected_convertible - checks if detected type converts to To
 */
template <typename To, template <typename...> class Op, typename... Args>
using is_detected_convertible = std::is_convertible<detected_t<Op, Args...>, To>;

template <typename To, template <typename...> class Op, typename... Args>
inline constexpr bool is_detected_convertible_v = is_detected_convertible<To, Op, Args...>::value;

// =============================================================================
// Container Property Traits
// =============================================================================

/**
 * @brief has_begin - detects if T has begin() method or std::begin works
 */
template <typename T, typename = void>
struct has_begin : std::false_type {};

template <typename T>
struct has_begin<T, detail::void_t<decltype(std::begin(std::declval<T&>()))>> : std::true_type {};

template <typename T>
inline constexpr bool has_begin_v = has_begin<T>::value;

/**
 * @brief has_end - detects if T has end() method or std::end works
 */
template <typename T, typename = void>
struct has_end : std::false_type {};

template <typename T>
struct has_end<T, detail::void_t<decltype(std::end(std::declval<T&>()))>> : std::true_type {};

template <typename T>
inline constexpr bool has_end_v = has_end<T>::value;

/**
 * @brief has_size - detects if T has size() method
 */
template <typename T, typename = void>
struct has_size : std::false_type {};

template <typename T>
struct has_size<T, detail::void_t<decltype(std::declval<T&>().size())>> : std::true_type {};

template <typename T>
inline constexpr bool has_size_v = has_size<T>::value;

/**
 * @brief has_empty - detects if T has empty() method
 */
template <typename T, typename = void>
struct has_empty : std::false_type {};

template <typename T>
struct has_empty<T, detail::void_t<decltype(std::declval<T&>().empty())>> : std::true_type {};

template <typename T>
inline constexpr bool has_empty_v = has_empty<T>::value;

/**
 * @brief has_reserve - detects if T has reserve() method
 */
template <typename T, typename = void>
struct has_reserve : std::false_type {};

template <typename T>
struct has_reserve<T, detail::void_t<decltype(std::declval<T&>().reserve(std::declval<std::size_t>()))>> : std::true_type {};

template <typename T>
inline constexpr bool has_reserve_v = has_reserve<T>::value;

// =============================================================================
// Composite Traits
// =============================================================================

/**
 * @brief is_iterable - checks if T can be iterated (has begin and end)
 */
template <typename T>
struct is_iterable : std::conjunction<has_begin<T>, has_end<T>> {};

template <typename T>
inline constexpr bool is_iterable_v = is_iterable<T>::value;

/**
 * @brief is_sized - checks if T has size() method
 */
template <typename T>
struct is_sized : has_size<T> {};

template <typename T>
inline constexpr bool is_sized_v = is_sized<T>::value;

/**
 * @brief is_container - checks if T is a container (iterable + sized)
 */
template <typename T>
struct is_container : std::conjunction<is_iterable<T>, is_sized<T>> {};

template <typename T>
inline constexpr bool is_container_v = is_container<T>::value;

/**
 * @brief is_reservable - checks if T has reserve() (e.g., vector, string)
 */
template <typename T>
struct is_reservable : has_reserve<T> {};

template <typename T>
inline constexpr bool is_reservable_v = is_reservable<T>::value;

// =============================================================================
// Comparability Traits
// =============================================================================

/**
 * @brief is_hashable - detects if std::hash<T> is defined
 */
template <typename T, typename = void>
struct is_hashable : std::false_type {};

template <typename T>
struct is_hashable<T, detail::void_t<decltype(std::hash<std::remove_cv_t<T>>{}(std::declval<T>()))>> : std::true_type {};

template <typename T>
inline constexpr bool is_hashable_v = is_hashable<T>::value;

/**
 * @brief is_comparable - detects if T has operator< (for ordering)
 */
template <typename T, typename = void>
struct is_comparable : std::false_type {};

template <typename T>
struct is_comparable<T, detail::void_t<decltype(std::declval<T>() < std::declval<T>())>> : std::true_type {};

template <typename T>
inline constexpr bool is_comparable_v = is_comparable<T>::value;

/**
 * @brief is_valid_comparator - checks if Comp can compare T objects
 */
template <typename Comp, typename T, typename = void>
struct is_valid_comparator : std::false_type {};

template <typename Comp, typename T>
struct is_valid_comparator<Comp, T, detail::void_t<
    decltype(std::declval<Comp>()(std::declval<T>(), std::declval<T>()))
>> : std::true_type {};

template <typename Comp, typename T>
inline constexpr bool is_valid_comparator_v = is_valid_comparator<Comp, T>::value;

// =============================================================================
// Library Type Detection
// =============================================================================

/**
 * @brief is_atomic - detects if T is std::atomic<U> for some U
 */
template <typename T>
struct is_atomic : std::false_type {};

template <typename T>
struct is_atomic<std::atomic<T>> : std::true_type {};

template <typename T>
inline constexpr bool is_atomic_v = is_atomic<T>::value;

/**
 * @brief is_expected - detects Expected types (forward declaration compatible)
 */
template <typename T>
struct is_expected : std::false_type {};

template <typename T>
inline constexpr bool is_expected_v = is_expected<T>::value;

/**
 * @brief is_strong_id - detects StrongId types (forward declaration compatible)
 */
template <typename T>
struct is_strong_id : std::false_type {};

template <typename T>
inline constexpr bool is_strong_id_v = is_strong_id<T>::value;

// =============================================================================
// Policy Detection Traits
// =============================================================================

/**
 * @brief has_validate_t - detects validate() method in policies
 */
template <typename T>
using has_validate_t = decltype(std::declval<T&>().validate());

/**
 * @brief has_shared_locking - detects SharedGuard type in concurrency policies
 */
template <typename T, typename = void>
struct has_shared_locking : std::false_type {};

template <typename T>
struct has_shared_locking<T, detail::void_t<typename T::SharedGuard>> : std::true_type {};

template <typename T>
inline constexpr bool has_shared_locking_v = has_shared_locking<T>::value;

/**
 * @brief is_lock_free_policy - detects LockFreeTag in policies
 */
template <typename T, typename = void>
struct is_lock_free_policy : std::false_type {};

template <typename T>
struct is_lock_free_policy<T, detail::void_t<typename T::LockFreeTag>> : std::true_type {};

template <typename T>
inline constexpr bool is_lock_free_policy_v = is_lock_free_policy<T>::value;

} // namespace cpp_utilities
