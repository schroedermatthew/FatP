/**
 * @file EqualityComparisons.h
 * @brief Recursive equality comparison framework for containers and nested types.
 *
 * @layer Domain
 *
 * Builds on FloatingPointComparison.h to provide recursive comparison for
 * containers, pairs, tuples, and user-defined types. Includes diagnostic
 * logging on mismatch with element indices and values.
 */
#pragma once

/*
FATP_META:
  meta_version: 1
  component: EqualityComparisons
  file_role: public_header
  path: include/fat_p/EqualityComparisons.h
  namespace: fat_p
  layer: Foundation
  summary: "Public header for EqualityComparisons."
  api_stability: in_work
  related:
    docs_search: "EqualityComparisons"
    tests:
      - components/Equality/tests/test_EqualityComparisons.cpp
      - components/Tensor/tests/test_TensorComparison.cpp
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
#include <cstddef>
#include <functional>
#include <iterator>
#include <string>
#include <tuple>
#include <type_traits>
#include <typeinfo>
#include <unordered_set>
#include <utility>
#include <vector>

#include "DiagnosticLogger_Core.h"
#include "FloatingPointComparison.h"
#include "Stringify.h"

namespace fat_p
{

// ============================================================================
// Configuration Constants
// ============================================================================

/**
 * @brief Determines if testing containers should stop on the first error
 * or test the entire container and print error messages for each error.
 *
 * - true:  Stop at first mismatch
 * - false: Check all elements and report all differences
 */
inline constexpr bool kStopOnFirstError = false;

// ============================================================================
// Forward Declarations and Traits
// ============================================================================

/**
 * @brief Forward declaration of the dispatcher struct.
 * @tparam T The type being compared.
 * @tparam Policy The comparison policy for floating-point types.
 */
template <typename T, typename Policy>
struct EqualDispatcher;

/**
 * @brief Trait to check if a type has `operator==`.
 * @tparam T The type to check.
 */
template <typename T, typename = void>
struct HasEqual : std::false_type
{
};

template <typename T>
struct HasEqual<
    T,
    std::enable_if_t<std::is_convertible_v<decltype(std::declval<const T&>() == std::declval<const T&>()), bool>>>
    : std::true_type
{
};

/**
 * @brief Trait to check if a type is a `std::pair`.
 * @tparam T The type to check.
 */
template <typename T>
struct IsPair : std::false_type
{
};

template <typename T1, typename T2>
struct IsPair<std::pair<T1, T2>> : std::true_type
{
};

/**
 * @brief Trait to check if a type is a `std::tuple`.
 * @tparam T The type to check.
 */
template <typename T>
struct IsTuple : std::false_type
{
};

template <typename... Ts>
struct IsTuple<std::tuple<Ts...>> : std::true_type
{
};

/**
 * @brief Trait to check if a type is iterable (has const begin/end).
 * @tparam T The type to check.
 */
template <typename T, typename = void>
struct IsIterable : std::false_type
{
};

template <typename T>
struct IsIterable<
    T,
    std::void_t<decltype(std::begin(std::declval<const T&>())), decltype(std::end(std::declval<const T&>()))>>
    : std::true_type
{
};

/**
 * @brief Trait to check if a type has a size() member.
 * @tparam T The type to check.
 */
template <typename T, typename = void>
struct HasSize : std::false_type
{
};

template <typename T>
struct HasSize<T, std::void_t<decltype(std::declval<const T&>().size())>> : std::true_type
{
};

/**
 * @brief Trait to check if a type has mapped_type (maps vs sets).
 * @tparam T The type to check.
 */
template <typename T, typename = void>
struct HasMappedType : std::false_type
{
};

template <typename T>
struct HasMappedType<T, std::void_t<typename T::mapped_type>> : std::true_type
{
};

/**
 * @brief Trait to check if toString(const T&) is available and returns std::string.
 *
 * Requires the return type to be convertible to std::string to prevent
 * compile errors when toString returns a non-string type.
 *
 * @tparam T The type to check.
 */
template <typename T, typename = void>
struct HasToString : std::false_type
{
};

template <typename T>
struct HasToString<T,
                   std::enable_if_t<std::is_convertible_v<decltype(toString(std::declval<const T&>())), std::string>>>
    : std::true_type
{
};

/**
 * @brief Robust trait to check if a type is an unordered associative container.
 *
 * Validates the full API surface required for unordered container comparison:
 * - key_type exists
 * - find(key) exists and returns an iterator
 * - end() exists
 * - find() result is comparable to end()
 *
 * This is stronger than just checking for key_equal, which could exist on
 * types that don't support the full associative container interface.
 *
 * @tparam T The type to check.
 */
template <typename T, typename = void>
struct IsUnorderedAssociative : std::false_type
{
};

template <typename T>
struct IsUnorderedAssociative<
    T,
    std::void_t<typename T::key_type,
                typename T::key_equal,
                decltype(std::declval<const T&>().find(std::declval<const typename T::key_type&>())),
                decltype(std::declval<const T&>().end()),
                decltype(std::declval<const T&>().find(std::declval<const typename T::key_type&>()) ==
                         std::declval<const T&>().end())>> : std::true_type
{
};

/**
 * @brief Trait to detect unordered *multi* associative containers.
 *
 * Distinguishes mUnordered{set,map} (unique) from mUnordered{multiset,multimap} (multi)
 * using the return type of insert(value_type):
 * - unique: std::pair<iterator, bool>
 * - multi:  iterator
 *
 * Also requires equal_range/count so we can validate multiplicity and match values,
 * and hash_function()/key_eq() for building the visited-keys set.
 *
 * @tparam T The type to check.
 */
template <typename T, typename = void>
struct IsUnorderedMultiAssociative : std::false_type
{
};

template <typename T>
struct IsUnorderedMultiAssociative<
    T,
    std::void_t<typename T::key_type,
                typename T::key_equal,
                decltype(std::declval<const T&>().equal_range(std::declval<const typename T::key_type&>())),
                decltype(std::declval<const T&>().count(std::declval<const typename T::key_type&>())),
                decltype(std::declval<const T&>().hash_function()),
                decltype(std::declval<const T&>().key_eq())>>
    : std::bool_constant<!std::is_same_v<decltype(std::declval<T&>().insert(std::declval<typename T::value_type>())),
                                         std::pair<typename T::iterator, bool>>>
{
};

/**
 * @brief Alias to get the `value_type` of a container.
 * @tparam Container The container type.
 */
template <typename Container>
using ContainerValueT = typename Container::value_type;

/**
 * @brief Truncates a string for safe logging, showing head and tail.
 *
 * Prevents OOM and log flooding when comparing large strings (XML, JSON, etc.).
 * Shows the beginning and end of long strings with total size.
 *
 * @param s The string to truncate.
 * @param headLen Number of characters to show from the beginning.
 * @param tailLen Number of characters to show from the end.
 * @return Truncated string with size annotation, or original if short enough.
 */
inline std::string truncateForLog(const std::string& s, std::size_t headLen = 32, std::size_t tailLen = 16)
{
    constexpr std::size_t kEllipsisLen = 5; // " ... "
    if (s.size() <= headLen + tailLen + kEllipsisLen)
    {
        return s;
    }
    return s.substr(0, headLen) + " ... " + s.substr(s.size() - tailLen) + " (" + std::to_string(s.size()) + " chars)";
}

/**
 * @brief Formats a value for diagnostics; falls back to typeid name if toString unavailable.
 *
 * For std::string values, applies truncation to prevent OOM and log flooding
 * when comparing containers of large strings.
 *
 * @tparam T The value type.
 * @param value The value to format.
 * @return A diagnostic string representation.
 */
template <typename T>
inline std::string safeToString(const T& value)
{
    // Special case: truncate std::string to prevent OOM in container logs
    if constexpr (std::is_same_v<std::decay_t<T>, std::string>)
    {
        return truncateForLog(value);
    }
    else if constexpr (HasToString<T>::value)
    {
        return toString(value);
    }
    else
    {
        return std::string("[") + typeid(value).name() + "]";
    }
}

// ============================================================================
// Tuple Comparison (respects kStopOnFirstError)
// ============================================================================

namespace detail
{

/**
 * @brief Helper to compare tuple elements with proper diagnostics and
 * respect for kStopOnFirstError setting.
 *
 * Unlike a simple fold expression, this implementation:
 * - Reports which tuple index failed
 * - Shows the differing values
 * - Continues checking all elements when kStopOnFirstError is false
 */
template <typename Policy, typename Tuple, std::size_t... I, typename... EpsParams>
bool tupleAreEqualImpl(const Tuple& a, const Tuple& b, std::index_sequence<I...>, EpsParams... eps)
{
    bool success = true;

    (
        [&]() {
            using ElemT = std::decay_t<std::tuple_element_t<I, Tuple>>;
            bool result = EqualDispatcher<ElemT, Policy>::compare(std::get<I>(a), std::get<I>(b), eps...);
            if (!result)
            {
                success = false;
                FATP_LOG_ERROR(std::string("Tuple element mismatch at index ") + std::to_string(I) + ": " +
                               safeToString(std::get<I>(a)) + " != " + safeToString(std::get<I>(b)));
            }
        }(),
        ...);

    return success;
}

/**
 * @brief Short-circuit tuple comparison using && fold expression.
 *
 * Stops at first mismatch. Uses consistent element typing with tupleAreEqualImpl.
 */
template <typename Policy, typename Tuple, std::size_t... I, typename... EpsParams>
bool tupleAreEqualStopFirstImpl(const Tuple& a, const Tuple& b, std::index_sequence<I...>, EpsParams... eps)
{
    return ([&]() -> bool {
        using ElemT = std::decay_t<std::tuple_element_t<I, Tuple>>;
        bool result = EqualDispatcher<ElemT, Policy>::compare(std::get<I>(a), std::get<I>(b), eps...);
        if (!result)
        {
            FATP_LOG_ERROR(std::string("Tuple element mismatch at index ") + std::to_string(I) + ": " +
                           safeToString(std::get<I>(a)) + " != " + safeToString(std::get<I>(b)));
        }
        return result;
    }() && ...);
}

/**
 * @brief Tuple comparison that respects kStopOnFirstError.
 *
 * When kStopOnFirstError is true, uses short-circuit evaluation.
 * When false, checks all elements and reports all mismatches.
 */
template <typename Policy, typename Tuple, typename... EpsParams>
bool compareTuple(const Tuple& a, const Tuple& b, EpsParams... eps)
{
    if constexpr (kStopOnFirstError)
    {
        return tupleAreEqualStopFirstImpl<Policy>(a, b, std::make_index_sequence<std::tuple_size_v<Tuple>>{}, eps...);
    }
    else
    {
        return tupleAreEqualImpl<Policy>(a, b, std::make_index_sequence<std::tuple_size_v<Tuple>>{}, eps...);
    }
}

} // namespace detail

// ============================================================================
// EqualDispatcher - Central Dispatching Mechanism
// ============================================================================

/**
 * @brief Dispatcher for equality comparison based on type and policy.
 *
 * Uses template specialization and SFINAE to route comparison requests
 * to the appropriate comparison method based on the type:
 * - Pairs: Compare first and second separately
 * - Tuples: Compare each element recursively (respects kStopOnFirstError)
 * - Unordered associative: Key-based lookup comparison
 * - Sequential containers: Element-wise comparison with index tracking
 * - Floating-point: Delegate to Policy
 * - Other types: Use operator==
 *
 * @tparam T The type being compared.
 * @tparam Policy The comparison policy for floating-point types.
 */
template <typename T, typename Policy>
struct EqualDispatcher
{
    /**
     * @brief Recursively compares two objects.
     * @tparam EpsParams Optional epsilon parameters for floating-point comparison.
     * @param a The first object.
     * @param b The second object.
     * @param eps Optional epsilon parameters.
     * @return True if objects are equal according to the policy.
     *
     * @note Complexity: O(N) where N is the total number of elements in nested structures.
     *       For unordered multi-containers, worst-case matching may be O(N*M)
     *       where M is the maximum multiplicity of a single key.
     * @note Thread-safety: Comparison logic is thread-safe for concurrent reads.
     *       Mismatch paths call FATP_LOG_ERROR, so overall thread-safety depends on
     *       the logging subsystem.
     */
    template <typename... EpsParams>
    static bool compare(const T& a, const T& b, EpsParams... eps)
    {
        // === PAIR COMPARISON ===
        if constexpr (IsPair<T>::value)
        {
            bool firstOk = EqualDispatcher<typename T::first_type, Policy>::compare(a.first, b.first, eps...);
            if (!firstOk)
            {
                FATP_LOG_ERROR(std::string("Pair.first mismatch: ") + safeToString(a.first) +
                               " != " + safeToString(b.first));
                if (kStopOnFirstError)
                {
                    return false;
                }
            }

            bool secondOk = EqualDispatcher<typename T::second_type, Policy>::compare(a.second, b.second, eps...);
            if (!secondOk)
            {
                FATP_LOG_ERROR(std::string("Pair.second mismatch: ") + safeToString(a.second) +
                               " != " + safeToString(b.second));
                if (kStopOnFirstError)
                {
                    return false;
                }
            }

            return firstOk && secondOk;
        }

        // === TUPLE COMPARISON ===
        else if constexpr (IsTuple<T>::value)
        {
            return detail::compareTuple<Policy>(a, b, eps...);
        }

        // === STRING FAST PATH ===
        // Strings are iterable but operator== is more efficient than character-by-character
        // recursive dispatch. This avoids per-character EqualDispatcher overhead and index tracking.
        // No logging here -- outer context (pair/tuple/container) provides structural info.
        // Use decay_t to handle const std::string (e.g., map keys are std::pair<const K, V>).
        else if constexpr (std::is_same_v<std::decay_t<T>, std::string>)
        {
            return a == b;
        }

        // === CONTAINER COMPARISON ===
        else if constexpr (IsIterable<T>::value)
        {
            // Size check (only when .size() is available)
            if constexpr (HasSize<T>::value)
            {
                if (a.size() != b.size())
                {
                    FATP_LOG_ERROR(std::string("Container size mismatch: ") + std::to_string(a.size()) + " vs " +
                                   std::to_string(b.size()));
                    return false;
                }
            }

            bool success = true;

            // --- UNORDERED ASSOCIATIVE CONTAINERS ---
            // Uses the robust IsUnorderedAssociative trait that validates the full API.
            //
            // SEMANTICS NOTE: This comparator enforces VALUE EQUALITY, not just key equivalence.
            // Key lookup uses the container's key_equal to find corresponding entries, but the
            // actual values (or elements in sets) are then compared for exact/epsilon equality.
            //
            // SET SEMANTICS: For unordered_set/unordered_multiset, elements are compared for value
            // equality after lookup. With non-standard key_equal (e.g., case-insensitive), two sets
            // may compare UNEQUAL even if key-equivalent: {"Hello"} vs {"hello"} are NOT EQUAL
            // because the string values differ.
            //
            // MAP SEMANTICS: For unordered_map/unordered_multimap, keys are matched by key_eq for
            // correspondence, then MAPPED VALUES are compared for equality. Key representations are
            // NOT compared. With case-insensitive key_equal, {("Hello",1)} vs {("hello",1)} ARE EQUAL
            // because the mapped values match (key representations are considered equivalent by key_eq).
            //
            // NOTE: Key matching uses each container's own hash_function()/key_eq() (e.g., searching
            // b uses b's functors). If a and b were constructed with different stateful hash/key_equal
            // instances, lookups may fail unexpectedly.
            //
            // For unordered maps, epsilon applies only to mapped values (after successful key
            // lookup). Epsilon cannot be used to "find" approximate keys in unordered containers.
            if constexpr (IsUnorderedAssociative<T>::value)
            {
                using KeyT = typename T::key_type;

                // Multi-containers require multiplicity validation and per-key matching.
                if constexpr (IsUnorderedMultiAssociative<T>::value)
                {
                    // Use the container's actual hasher and key_equal instances.
                    // Required for stateful functors (seeded hashers, custom equals, etc.).
                    auto keyEq = a.key_eq();
                    auto keyHash = a.hash_function();

                    struct KeyHasher
                    {
                        decltype(keyHash) mHasher;

                        std::size_t operator()(const KeyT& key) const
                        {
                            return mHasher(key);
                        }
                    };

                    struct KeyEqual
                    {
                        decltype(keyEq) mEqual;

                        bool operator()(const KeyT& aKey, const KeyT& bKey) const
                        {
                            return mEqual(aKey, bKey);
                        }
                    };

                    std::unordered_set<KeyT, KeyHasher, KeyEqual> visitedKeys(0, KeyHasher{keyHash}, KeyEqual{keyEq});

                    if constexpr (HasSize<T>::value)
                    {
                        visitedKeys.reserve(a.size());
                    }

                    for (const auto& elemA : a)
                    {
                        const KeyT& keyA = [&]() -> const KeyT& {
                            if constexpr (HasMappedType<T>::value)
                            {
                                return elemA.first;
                            }
                            else
                            {
                                return elemA;
                            }
                        }();

                        auto insertResult = visitedKeys.insert(keyA);
                        if (!insertResult.second)
                        {
                            continue;
                        }

                        const auto countA = a.count(keyA);
                        const auto countB = b.count(keyA);
                        if (countA != countB)
                        {
                            FATP_LOG_ERROR(std::string("Multi-container multiplicity mismatch for key ") +
                                           safeToString(keyA) + ": " + std::to_string(countA) +
                                           " != " + std::to_string(countB));
                            if (kStopOnFirstError)
                            {
                                return false;
                            }
                            success = false;
                            continue;
                        }

                        auto rangeA = a.equal_range(keyA);
                        auto rangeB = b.equal_range(keyA);

                        // Materialize B range iterators so we can "consume" matches.
                        std::vector<decltype(rangeB.first)> bIters;
                        bIters.reserve(static_cast<std::size_t>(countB));
                        for (auto it = rangeB.first; it != rangeB.second; ++it)
                        {
                            bIters.push_back(it);
                        }
                        std::vector<bool> used(bIters.size(), false);

                        if constexpr (HasMappedType<T>::value)
                        {
                            // unordered_multimap: match values within the equal_range bucket.
                            for (auto itA = rangeA.first; itA != rangeA.second; ++itA)
                            {
                                bool matched = false;
                                using ValT = std::decay_t<decltype(itA->second)>;

                                for (std::size_t i = 0; i < bIters.size(); ++i)
                                {
                                    if (used[i])
                                    {
                                        continue;
                                    }

                                    if (EqualDispatcher<ValT, Policy>::compare(itA->second, bIters[i]->second, eps...))
                                    {
                                        used[i] = true;
                                        matched = true;
                                        break;
                                    }
                                }

                                if (!matched)
                                {
                                    FATP_LOG_ERROR(std::string("Multimap: no matching value for key ") +
                                                   safeToString(keyA) + " and value " + safeToString(itA->second));
                                    if (kStopOnFirstError)
                                    {
                                        return false;
                                    }
                                    success = false;
                                }
                            }
                        }
                        else
                        {
                            // unordered_multiset: match elements within the equal_range bucket.
                            for (auto itA = rangeA.first; itA != rangeA.second; ++itA)
                            {
                                bool matched = false;
                                using ValT = std::decay_t<decltype(*itA)>;

                                for (std::size_t i = 0; i < bIters.size(); ++i)
                                {
                                    if (used[i])
                                    {
                                        continue;
                                    }

                                    if (EqualDispatcher<ValT, Policy>::compare(*itA, *bIters[i], eps...))
                                    {
                                        used[i] = true;
                                        matched = true;
                                        break;
                                    }
                                }

                                if (!matched)
                                {
                                    FATP_LOG_ERROR(std::string("Multiset: no matching element for key ") +
                                                   safeToString(keyA) + " and element " + safeToString(*itA));
                                    if (kStopOnFirstError)
                                    {
                                        return false;
                                    }
                                    success = false;
                                }
                            }
                        }
                    }

                    // Reverse check for containers without .size()
                    // Detects keys in b that are not in a
                    if constexpr (!HasSize<T>::value)
                    {
                        for (const auto& elemB : b)
                        {
                            const KeyT& keyB = [&]() -> const KeyT& {
                                if constexpr (HasMappedType<T>::value)
                                {
                                    return elemB.first;
                                }
                                else
                                {
                                    return elemB;
                                }
                            }();

                            // Dedupe: skip keys already seen in forward scan or earlier in this loop
                            auto insertResult = visitedKeys.insert(keyB);
                            if (!insertResult.second)
                            {
                                continue;
                            }

                            if (a.count(keyB) == 0)
                            {
                                FATP_LOG_ERROR(std::string("Extra key present only in second container: ") +
                                               safeToString(keyB));
                                if (kStopOnFirstError)
                                {
                                    return false;
                                }
                                success = false;
                            }
                        }
                    }

                    return success;
                }

                // Unique mUnordered{set,map}:
                for (const auto& elemA : a)
                {
                    // Handle maps (have mapped_type)
                    if constexpr (HasMappedType<T>::value)
                    {
                        const KeyT& keyA = elemA.first;
                        auto itB = b.find(keyA);
                        if (itB == b.end())
                        {
                            FATP_LOG_ERROR(std::string("Key not found in second container: ") + safeToString(keyA));
                            if (kStopOnFirstError)
                            {
                                return false;
                            }
                            success = false;
                            continue;
                        }
                        const auto& valA = elemA.second;
                        const auto& valB = itB->second;
                        using ValT = std::decay_t<decltype(valA)>;
                        bool result = EqualDispatcher<ValT, Policy>::compare(valA, valB, eps...);
                        if (!result)
                        {
                            success = false;
                            FATP_LOG_ERROR(std::string("Value mismatch for key '") + safeToString(keyA) +
                                           "': " + safeToString(valA) + " != " + safeToString(valB));
                            if (kStopOnFirstError)
                            {
                                return false;
                            }
                        }
                    }
                    // Handle sets (no mapped_type)
                    else
                    {
                        const KeyT& keyA = elemA;
                        auto itB = b.find(keyA);
                        if (itB == b.end())
                        {
                            FATP_LOG_ERROR(std::string("Element not found in second set: ") + safeToString(keyA));
                            if (kStopOnFirstError)
                            {
                                return false;
                            }
                            success = false;
                            continue;
                        }
                        // For sets, also compare the values (for floating-point tolerance)
                        const auto& valA = elemA;
                        const auto& valB = *itB;
                        using ValT = std::decay_t<decltype(valA)>;
                        bool result = EqualDispatcher<ValT, Policy>::compare(valA, valB, eps...);
                        if (!result)
                        {
                            success = false;
                            FATP_LOG_ERROR(std::string("Set element value mismatch: ") + safeToString(valA) +
                                           " != " + safeToString(valB));
                            if (kStopOnFirstError)
                            {
                                return false;
                            }
                        }
                    }
                }
                if constexpr (!HasSize<T>::value)
                {
                    for (const auto& elemB : b)
                    {
                        const KeyT& keyB = [&]() -> const KeyT& {
                            if constexpr (HasMappedType<T>::value)
                            {
                                return elemB.first;
                            }
                            else
                            {
                                return elemB;
                            }
                        }();

                        if (a.find(keyB) == a.end())
                        {
                            FATP_LOG_ERROR(std::string("Extra key present only in second container: ") +
                                           safeToString(keyB));
                            if (kStopOnFirstError)
                            {
                                return false;
                            }
                            success = false;
                        }
                    }
                }

                return success;
            }

            // --- ORDERED ASSOCIATIVE OR SEQUENTIAL CONTAINERS ---
            else
            {
                auto it1 = std::begin(a);
                auto it2 = std::begin(b);
                std::size_t index = 0;

                while (it1 != std::end(a))
                {
                    // Early-end guard for non-sized containers (prevents UB)
                    if constexpr (!HasSize<T>::value)
                    {
                        if (it2 == std::end(b))
                        {
                            FATP_LOG_ERROR(std::string("Container b ended early at index ") + std::to_string(index));
                            return false;
                        }
                    }

                    using ElemT = std::decay_t<decltype(*it1)>;
                    bool result = EqualDispatcher<ElemT, Policy>::compare(*it1, *it2, eps...);
                    if (!result)
                    {
                        success = false;
                        FATP_LOG_ERROR(std::string("Element mismatch at index ") + std::to_string(index) + ": " +
                                       safeToString(*it1) + " != " + safeToString(*it2));
                        if (kStopOnFirstError)
                        {
                            return false;
                        }
                    }
                    ++it1;
                    ++it2;
                    ++index;
                }

                // Post-loop check: b longer than a (non-sized containers only)
                if constexpr (!HasSize<T>::value)
                {
                    if (it2 != std::end(b))
                    {
                        FATP_LOG_ERROR(std::string("Container b has extra elements starting at index ") +
                                       std::to_string(index));
                        return false;
                    }
                }

                return success;
            }
        }

        // === FLOATING-POINT COMPARISON ===
        else if constexpr (std::is_floating_point_v<T>)
        {
            return Policy::epsilonMatch(a, b, eps...);
        }

        // === TYPES WITH operator== ===
        else if constexpr (HasEqual<T>::value)
        {
            // No logging here - outer context (pair/tuple/container) provides
            // structural information with values. Top-level comparisons don't
            // need logging since caller receives the boolean result directly.
            return a == b;
        }

        // === UNSUPPORTED TYPES ===
        else
        {
            FATP_LOG_ERROR(std::string("Unsupported type for equality comparison: ") + typeid(T).name());
            return false;
        }
    }
};

// ============================================================================
// Public Interface
// ============================================================================

/**
 * @brief Public interface for equality comparison.
 *
 * Main entry point for comparing any two objects of the same type.
 * Supports:
 * - Floating-point numbers (with epsilon comparison)
 * - Integers and other built-in types
 * - Containers (vector, array, map, set, unordered_map, unordered_set, etc.)
 * - Pairs and tuples
 * - Nested containers
 * - Any type with operator==
 *
 * @tparam T The type being compared.
 * @tparam Policy The comparison policy for floating-point types
 *         (default: StandardComparisonPolicy).
 * @tparam EpsParams Optional epsilon parameters for floating-point comparison.
 * @param a The first object.
 * @param b The second object.
 * @param eps Optional epsilon parameters (policy-dependent).
 * @return True if objects are equal according to the policy.
 *
 * @note Complexity: O(N) where N is the total number of elements in nested structures.
 *       For unordered multi-containers, worst-case matching may be O(N*M)
 *       where M is the maximum multiplicity of a single key.
 * @note Thread-safety: Comparison logic is thread-safe for concurrent reads.
 *       Mismatch paths call LOG_ERROR, so overall thread-safety depends on
 *       the logging subsystem.
 * @note Diagnostics: LOG_ERROR is emitted only by structural comparisons (containers,
 *       pairs, tuples) to provide context (index, key, position). Leaf comparisons
 *       (strings, integers, floats, and other operator== types) return only a boolean
 *       without logging. Top-level scalar comparisons produce no diagnostics.
 *
 * @code
 * // Basic comparison
 * bool eq1 = areEqual(1.0, 1.0 + 1e-10); // Uses StandardComparisonPolicy
 *
 * // With custom epsilon
 * bool eq2 = areEqual(1.0, 1.1, 0.2); // Within 0.2
 *
 * // With specific policy
 * bool eq3 = areEqual<double, UlpComparisonPolicy>(1.0, nextafter(1.0, 2.0));
 *
 * // Container comparison
 * std::vector<double> v1 = {1.0, 2.0, 3.0};
 * std::vector<double> v2 = {1.0, 2.0, 3.0 + 1e-10};
 * bool eq4 = areEqual(v1, v2); // Compares element-wise with epsilon
 *
 * // Nested containers
 * std::map<std::string, std::vector<double>> m1, m2;
 * bool eq5 = areEqual(m1, m2);
 * @endcode
 */
template <typename T, typename Policy = StandardComparisonPolicy, typename... EpsParams>
[[nodiscard]] bool areEqual(const T& a, const T& b, EpsParams... eps)
{
    return EqualDispatcher<T, Policy>::compare(a, b, eps...);
}

} // namespace fat_p
