#pragma once

/*
FATP_META:
  meta_version: 1
  component: Concepts
  file_role: public_header
  path: include/fat_p/Concepts.h
  namespace: fat_p::concepts
  layer: Foundation
  summary: C++20 concepts for type constraints.
  api_stability: stable
  related:
    docs_search: "Concepts"
    tests:
      - components/Concepts/tests/test_Concepts.cpp
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
*/

/**
 * @file Concepts.h
 * @brief C++20 concepts for type constraints.
 *
 * @layer Foundation
 *
 * @details Provides C++20 concepts for compile-time type introspection.
 *
 * All concepts use snake_case naming for STL compatibility.
 *
 * @section categories Concept Categories
 *
 * @subsection container_concepts Container Concepts
 * - `iterable<T>` - Has begin()/end()
 * - `sized<T>` - Has size()
 * - `container<T>` - Iterable + sized
 * - `contiguous_container<T>` - Container + data()
 * - `reservable<T>` - Has reserve()
 * - `reverse_iterable<T>` - Has rbegin()/rend()
 * - `random_accessible<T>` - Has operator[] + size()
 * - `map_like<T>` - Iterable with pair value_type
 *
 * @subsection comparison_concepts Comparison Concepts
 * - `hashable<T>` - std::hash<T> exists
 * - `equality_comparable<T>` - Has operator==
 * - `totally_ordered<T>` - Has <, <=, >, >=
 * - `three_way_comparable<T>` - Has operator<=>
 * - `valid_comparator<Comp, T>` - Comp can compare T objects
 *
 * @subsection callable_concepts Callable Concepts
 * - `invocable<F, Args...>` - F(args...) is valid
 * - `invocable_r<R, F, Args...>` - F(args...) returns R
 * - `nothrow_invocable<F, Args...>` - F(args...) is noexcept
 * - `function_object<T>` - Has operator()
 *
 * @subsection type_concepts Type Classification Concepts
 * - `streamable<T>` - Supports operator<<
 * - `allocator<T>` - Satisfies allocator requirements
 * - `serializable<T>` - Has serialize/deserialize
 * - `scoped_enum<T>` - Is an enum class
 * - `atomic_type<T>` - Is std::atomic<U>
 * - `transparent<T>` - Has is_transparent tag
 *
 * @note All concepts have zero runtime overhead (compile-time only).
 * @note Thread-safe: All operations are compile-time only.
 */

#include <atomic>
#include <concepts>
#include <cstddef>
#include <functional>
#include <istream>
#include <iterator>
#include <ostream>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>

#include "CppFeatureDetection.h"

namespace fat_p::concepts
{

// =============================================================================
// Container Concepts
// =============================================================================

/**
 * @brief Checks if T can be iterated (has begin and end returning iterators).
 * @tparam T The type to check
 *
 * @details Equivalent to std::ranges::range but with explicit iterator requirements.
 *
 * Example:
 * @code
 * template <iterable T>
 * void process(const T& container);
 * @endcode
 */
template <typename T>
concept iterable = requires(T& val) {
    { std::begin(val) } -> std::input_or_output_iterator;
    { std::end(val) } -> std::input_or_output_iterator;
};

/**
 * @brief Checks if T has a size() method returning an integral type.
 * @tparam T The type to check
 */
template <typename T>
concept sized = requires(const T& val) {
    { val.size() } -> std::integral;
};

/**
 * @brief Checks if T is a container (iterable + sized).
 * @tparam T The type to check
 *
 * @details This is the Fat-P definition of container: iterable with known size.
 * Use std::ranges::sized_range for the standard library equivalent.
 */
template <typename T>
concept container = iterable<T> && sized<T>;

/**
 * @brief Checks if T has a data() method (contiguous storage).
 * @tparam T The type to check
 */
template <typename T>
concept has_data = requires(T& val) {
    { val.data() };
};

/**
 * @brief Checks if T is a contiguous container (container + data()).
 * @tparam T The type to check
 *
 * @details Use std::ranges::contiguous_range for the standard library equivalent.
 */
template <typename T>
concept contiguous_container = container<T> && has_data<T>;

/**
 * @brief Checks if T has a reserve() method.
 * @tparam T The type to check
 */
template <typename T>
concept reservable = requires(T& val) {
    { val.reserve(std::size_t{}) };
};

/**
 * @brief Checks if T supports reverse iteration (rbegin/rend).
 * @tparam T The type to check
 */
template <typename T>
concept reverse_iterable = requires(T& val) {
    { std::rbegin(val) } -> std::input_or_output_iterator;
    { std::rend(val) } -> std::input_or_output_iterator;
};

/**
 * @brief Checks if T has an empty() method.
 * @tparam T The type to check
 */
template <typename T>
concept has_empty = requires(const T& val) {
    { val.empty() } -> std::convertible_to<bool>;
};

/**
 * @brief Checks if T supports operator[] with size_t index.
 * @tparam T The type to check
 */
template <typename T>
concept subscriptable = requires(T& val, std::size_t idx) {
    { val[idx] };
};

/**
 * @brief Checks if T has an at() method with bounds checking.
 * @tparam T The type to check
 */
template <typename T>
concept has_at = requires(T& val, std::size_t idx) {
    { val.at(idx) };
};

/**
 * @brief Checks if T supports random access (subscript + size).
 * @tparam T The type to check
 */
template <typename T>
concept random_accessible = subscriptable<T> && sized<T>;

/**
 * @brief Checks if T has a clear() method.
 * @tparam T The type to check
 */
template <typename T>
concept has_clear = requires(T& val) {
    { val.clear() };
};

/**
 * @brief Checks if T has push_back() method.
 * @tparam T The type to check
 */
template <typename T>
concept has_push_back = requires(T& val, typename T::value_type elem) {
    { val.push_back(elem) };
};

/**
 * @brief Checks if T has emplace_back() method.
 * @tparam T The type to check
 */
template <typename T>
concept has_emplace_back = requires(T& val) {
    { val.emplace_back() };
};

/**
 * @brief Checks if T has push_front() method.
 * @tparam T The type to check
 */
template <typename T>
concept has_push_front = requires(T& val, typename T::value_type elem) {
    { val.push_front(elem) };
};

/**
 * @brief Helper to detect pair value_type.
 */
namespace detail
{
template <typename T>
struct is_pair : std::false_type
{
};

template <typename T1, typename T2>
struct is_pair<std::pair<T1, T2>> : std::true_type
{
};

template <typename T>
concept has_pair_value_type = requires {
    typename T::value_type;
} && is_pair<typename std::decay_t<T>::value_type>::value;

template <typename T>
concept has_key_mapped_types = requires {
    typename T::key_type;
    typename T::mapped_type;
};
} // namespace detail

/**
 * @brief Checks if T is map-like (iterable with pair value_type AND key_type/mapped_type).
 * @details Requires key_type and mapped_type to distinguish actual maps from vector<pair>.
 * @tparam T The type to check
 */
template <typename T>
concept map_like = iterable<T> && detail::has_pair_value_type<T> && detail::has_key_mapped_types<T>;

// =============================================================================
// Comparison Concepts
// =============================================================================

/**
 * @brief Checks if T is hashable via std::hash.
 * @tparam T The type to check
 */
template <typename T>
concept hashable = requires(const T& val) {
    { std::hash<std::remove_cv_t<T>>{}(val) } -> std::convertible_to<std::size_t>;
};

/**
 * @brief Checks if T has operator==.
 * @tparam T The type to check
 *
 * @note Prefer std::equality_comparable from <concepts> for standard conformance.
 */
template <typename T>
concept equality_comparable = requires(const T& a, const T& b) {
    { a == b } -> std::convertible_to<bool>;
};

/**
 * @brief Checks if T has operator!=.
 * @tparam T The type to check
 */
template <typename T>
concept inequality_comparable = requires(const T& a, const T& b) {
    { a != b } -> std::convertible_to<bool>;
};

/**
 * @brief Checks if T has all relational operators (<, <=, >, >=).
 * @tparam T The type to check
 *
 * @note Prefer std::totally_ordered from <concepts> for standard conformance.
 */
template <typename T>
concept totally_ordered = requires(const T& a, const T& b) {
    { a < b } -> std::convertible_to<bool>;
    { a <= b } -> std::convertible_to<bool>;
    { a > b } -> std::convertible_to<bool>;
    { a >= b } -> std::convertible_to<bool>;
};

/**
 * @brief Checks if T has operator<.
 * @tparam T The type to check
 */
template <typename T>
concept less_than_comparable = requires(const T& a, const T& b) {
    { a < b } -> std::convertible_to<bool>;
};

/**
 * @brief Checks if T has operator<=> (three-way comparison).
 * @tparam T The type to check
 *
 * @note Prefer std::three_way_comparable from <compare> for standard conformance.
 */
template <typename T>
concept three_way_comparable = requires(const T& a, const T& b) {
    { a <=> b };
};

/**
 * @brief Checks if Comp can compare T objects.
 * @tparam Comp The comparator type
 * @tparam T The type being compared
 */
template <typename Comp, typename T>
concept valid_comparator = requires(Comp comp, const T& a, const T& b) {
    { comp(a, b) } -> std::convertible_to<bool>;
};

/**
 * @brief Checks if T has is_transparent tag (for heterogeneous lookup).
 * @tparam T The type to check
 */
template <typename T>
concept transparent = requires {
    typename T::is_transparent;
};

// =============================================================================
// Stream Concepts
// =============================================================================

/**
 * @brief Checks if T supports stream insertion (operator<<).
 * @tparam T The type to check
 */
template <typename T>
concept streamable = requires(std::ostream& os, const T& val) {
    { os << val } -> std::convertible_to<std::ostream&>;
};

/**
 * @brief Checks if T supports wide stream insertion (operator<<).
 * @tparam T The type to check
 */
template <typename T>
concept wstreamable = requires(std::wostream& os, const T& val) {
    { os << val } -> std::convertible_to<std::wostream&>;
};

/**
 * @brief Checks if T supports stream extraction (operator>>).
 * @tparam T The type to check
 */
template <typename T>
concept input_streamable = requires(std::istream& is, T& val) {
    { is >> val } -> std::convertible_to<std::istream&>;
};

// =============================================================================
// Custom String Method Concepts
// =============================================================================

/**
 * @brief Checks if T has a member function toString() returning something convertible to string.
 * @tparam T The type to check
 */
template <typename T>
concept has_to_string_method = requires(const T& val) {
    { val.toString() } -> std::convertible_to<std::string>;
};

/**
 * @brief Checks if T has a member function to_string() returning something convertible to string.
 * @tparam T The type to check
 */
template <typename T>
concept has_to_string_snake_method = requires(const T& val) {
    { val.to_string() } -> std::convertible_to<std::string>;
};

/**
 * @brief Checks if T has any custom toString method (camelCase or snake_case).
 * @tparam T The type to check
 */
template <typename T>
concept has_custom_string_method = has_to_string_method<T> || has_to_string_snake_method<T>;

// =============================================================================
// Printable Range Concepts
// =============================================================================

/**
 * @brief Checks if T is a standard string type that should not be iterated as a container.
 * @tparam T The type to check
 * @note Used to exclude strings from range-based stringification.
 */
template <typename T>
concept std_string_type = std::same_as<std::remove_cvref_t<T>, std::string> ||
                          std::same_as<std::remove_cvref_t<T>, std::string_view> ||
                          std::same_as<std::remove_cvref_t<T>, const char*> ||
                          std::same_as<std::remove_cvref_t<T>, char*>;

/**
 * @brief Checks if T is a range that should be printed as a container.
 * @tparam T The type to check
 * @note Excludes standard string types which have their own handling.
 */
template <typename T>
concept printable_range = std::ranges::range<T> && (!std_string_type<T>);

// =============================================================================
// Callable Concepts
// =============================================================================

/**
 * @brief Checks if F can be invoked with Args.
 * @tparam F The callable type
 * @tparam Args The argument types
 *
 * @note Equivalent to std::invocable from <concepts>.
 */
template <typename F, typename... Args>
concept invocable = std::invocable<F, Args...>;

/**
 * @brief Checks if F returns R when invoked with Args.
 * @tparam R The expected return type
 * @tparam F The callable type
 * @tparam Args The argument types
 */
template <typename R, typename F, typename... Args>
concept invocable_r = std::is_invocable_r_v<R, F, Args...>;

/**
 * @brief Checks if F can be invoked with Args without throwing.
 * @tparam F The callable type
 * @tparam Args The argument types
 */
template <typename F, typename... Args>
concept nothrow_invocable = std::is_nothrow_invocable_v<F, Args...>;

/**
 * @brief Checks if T is a function object (has operator()).
 * @tparam T The type to check
 */
template <typename T>
concept function_object = requires {
    &T::operator();
};

// =============================================================================
// Type Classification Concepts
// =============================================================================

/**
 * @brief Checks if T is std::atomic<U> for some U.
 * @tparam T The type to check
 */
namespace detail
{
template <typename T>
struct is_atomic_impl : std::false_type
{
};

template <typename T>
struct is_atomic_impl<std::atomic<T>> : std::true_type
{
};
} // namespace detail

template <typename T>
concept atomic_type = detail::is_atomic_impl<T>::value;

/**
 * @brief Checks if T is a scoped enum (enum class).
 * @tparam T The type to check
 */
template <typename T>
concept scoped_enum = std::is_enum_v<T> && !std::is_convertible_v<T, int>;

/**
 * @brief Checks if T satisfies allocator requirements.
 * @tparam T The type to check
 */
template <typename T>
concept allocator = requires(T& alloc, std::size_t n) {
    typename T::value_type;
    { alloc.allocate(n) } -> std::same_as<typename T::value_type*>;
    { alloc.deallocate(std::declval<typename T::value_type*>(), n) };
};

/**
 * @brief Checks if T has allocator_type member.
 * @tparam T The type to check
 */
template <typename T>
concept has_allocator_type = requires {
    typename T::allocator_type;
};

/**
 * @brief Checks if allocator T has rebind member template.
 * @tparam T The allocator type to check
 */
template <typename T>
concept has_rebind = requires {
    typename T::template rebind<int>;
};

// =============================================================================
// Serialization Concepts
// =============================================================================

/**
 * @brief Checks if T has serialize(ostream&) method.
 * @tparam T The type to check
 */
template <typename T>
concept has_serialize = requires(T& val, std::ostream& os) {
    { val.serialize(os) };
};

/**
 * @brief Checks if T has static deserialize(istream&) method.
 * @tparam T The type to check
 */
template <typename T>
concept has_deserialize = requires(std::istream& is) {
    { T::deserialize(is) };
};

/**
 * @brief Checks if T supports both serialize and deserialize.
 * @tparam T The type to check
 */
template <typename T>
concept serializable = has_serialize<T> && has_deserialize<T>;

// =============================================================================
// Policy Detection Concepts
// =============================================================================

/**
 * @brief Checks if T has validate() method (for policies).
 * @tparam T The policy type
 */
template <typename T>
concept has_validate = requires(T& val) {
    { val.validate() };
};

/**
 * @brief Checks if T has SharedGuard type (for concurrency policies).
 * @tparam T The policy type
 */
template <typename T>
concept has_shared_locking = requires {
    typename T::SharedGuard;
};

/**
 * @brief Checks if T has LockFreeTag (for lock-free policies).
 * @tparam T The policy type
 */
template <typename T>
concept lock_free_policy = requires {
    typename T::LockFreeTag;
};

// =============================================================================
// Tuple Concepts
// =============================================================================

/**
 * @brief Checks if std::tuple_size works on T.
 * @tparam T The type to check
 */
template <typename T>
concept tuple_like = requires {
    { std::tuple_size<T>::value } -> std::convertible_to<std::size_t>;
};

/**
 * @brief Checks if T has first_type and second_type (pair-like).
 * @tparam T The type to check
 */
template <typename T>
concept pair_like = requires {
    typename T::first_type;
    typename T::second_type;
};

// =============================================================================
// Array Concepts
// =============================================================================

/**
 * @brief Checks if T is an array with known bound.
 * @tparam T The type to check
 */
template <typename T>
concept bounded_array = std::is_bounded_array_v<T>;

/**
 * @brief Checks if T is an array with unknown bound.
 * @tparam T The type to check
 */
template <typename T>
concept unbounded_array = std::is_unbounded_array_v<T>;

// =============================================================================
// Aggregate Concepts
// =============================================================================

/**
 * @brief Checks if T is an aggregate type.
 * @tparam T The type to check
 */
template <typename T>
concept aggregate = std::is_aggregate_v<T>;

// =============================================================================
// String-like Concepts
// =============================================================================

/**
 * @brief Checks if T has c_str() method.
 * @tparam T The type to check
 */
template <typename T>
concept has_c_str = requires(const T& val) {
    { val.c_str() };
};

/**
 * @brief Checks if T behaves like a string (iterable + c_str).
 * @tparam T The type to check
 */
template <typename T>
concept string_like = iterable<T> && has_c_str<T>;

// =============================================================================
// Optional-like Concepts
// =============================================================================

/**
 * @brief Checks if T has has_value() method.
 * @tparam T The type to check
 */
template <typename T>
concept has_has_value = requires(const T& val) {
    { val.has_value() } -> std::convertible_to<bool>;
};

/**
 * @brief Checks if T has value() method.
 * @tparam T The type to check
 */
template <typename T>
concept has_value_method = requires(T& val) {
    { val.value() };
};

/**
 * @brief Checks if T behaves like std::optional.
 * @tparam T The type to check
 */
template <typename T>
concept optional_like = has_has_value<T> && has_value_method<T>;

// =============================================================================
// Variant-like Concepts
// =============================================================================

/**
 * @brief Checks if T has index() method.
 * @tparam T The type to check
 */
template <typename T>
concept has_index_method = requires(const T& val) {
    { val.index() };
};

/**
 * @brief Checks if T behaves like std::variant.
 * @tparam T The type to check
 */
template <typename T>
concept variant_like = has_index_method<T>;

// =============================================================================
// Range Concepts
// =============================================================================

/**
 * @brief Checks if T can be used as a range (has begin/end).
 * @tparam T The type to check
 * @note Equivalent to iterable concept.
 */
template <typename T>
concept range = iterable<T>;

/**
 * @brief Checks if T is a range with size().
 * @tparam T The type to check
 */
template <typename T>
concept sized_range = iterable<T> && sized<T>;

// =============================================================================
// Iterator Concepts
// =============================================================================

/**
 * @brief Checks if T has iterator traits.
 * @tparam T The type to check
 */
template <typename T>
concept has_iterator_category = requires {
    typename std::iterator_traits<T>::iterator_category;
};

// =============================================================================
// Tuple-like Concepts
// =============================================================================

/**
 * @brief Checks if std::get works on T.
 * @tparam T The type to check
 */
template <typename T>
concept has_get = requires(T& val) {
    { std::get<0>(val) };
};

/**
 * @brief Checks if T supports tuple protocol (tuple_size, tuple_element, get).
 * @tparam T The type to check
 */
template <typename T>
concept is_tuple_like = tuple_like<T> && has_get<T>;

// =============================================================================
// Relocatable Concepts
// =============================================================================

/**
 * @brief Checks if T can be relocated with memcpy.
 * @tparam T The type to check
 */
template <typename T>
concept trivially_relocatable = std::is_trivially_copyable_v<T>;

// =============================================================================
// Stringifiable Concept
// =============================================================================

/**
 * @brief Checks if T can be converted to a string via fat_p::toString.
 * @tparam T The type to check
 *
 * @details A type is stringifiable if it is:
 * - An enum type
 * - An arithmetic type (integers, floats, bool)
 * - Streamable via operator<<
 * - Has a custom toString() or to_string() method
 * - A pair-like type (has first_type/second_type)
 * - A tuple-like type (has std::tuple_size)
 * - An optional-like type (has has_value() and value())
 * - A printable range (iterable, non-string)
 */
template <typename T>
concept stringifiable = std::is_enum_v<std::decay_t<T>> ||
                        std::is_arithmetic_v<std::decay_t<T>> ||
                        streamable<std::decay_t<T>> ||
                        has_custom_string_method<std::decay_t<T>> ||
                        pair_like<std::decay_t<T>> ||
                        tuple_like<std::decay_t<T>> ||
                        optional_like<std::decay_t<T>> ||
                        printable_range<std::decay_t<T>>;

} // namespace fat_p::concepts
