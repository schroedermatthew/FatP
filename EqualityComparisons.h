// EqualityComparisons.h
// Container and complex type equality comparison framework
// Builds on FloatingPointComparison.h to provide recursive comparison for
// containers, pairs, tuples, and user-defined types
#pragma once

#include <algorithm>
#include <any>
#include <array>
#include <cstddef>
#include <iterator>
#include <map>
#include <set>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "FloatingPointComparison.h" // Provides all comparison policies
#include "DiagnosticLogger.h"        // Dependency for conditionalPrintError and toString
#include "Stringify.h"               // For toString

namespace cpp_utilities {
    
    // ====================================================================
    // Configuration Constants
    // ====================================================================
    
    /**
     * @brief Determines if testing containers should stop on the first error
     * or test the entire container and print error messages for each error.
     * 
     * Constexpr allows compile-time use.
     * 
     * - true:  Stop at first mismatch (fail-fast)
     * - false: Check all elements and report all differences
     */
    inline constexpr bool kStopOnFirstError = false;

    // ====================================================================
    // Forward Declarations and Traits
    // ====================================================================
    
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
    struct HasEqual : std::false_type {};
    
    template <typename T>
    struct HasEqual<
        T, std::void_t<decltype(std::declval<const T&>() ==
            std::declval<const T&>())>>
        : std::true_type {};

    /**
     * @brief Trait to check if a type is a `std::pair`.
     * @tparam T The type to check.
     */
    template <typename T>
    struct IsPair : std::false_type {};
    
    template <typename T1, typename T2>
    struct IsPair<std::pair<T1, T2>> : std::true_type {};

    /**
     * @brief Trait to check if a type is a `std::tuple`.
     * @tparam T The type to check.
     */
    template <typename T>
    struct IsTuple : std::false_type {};
    
    template <typename... Ts>
    struct IsTuple<std::tuple<Ts...>> : std::true_type {};

    /**
     * @brief Trait to check if a type is iterable (has begin/end).
     * @tparam T The type to check.
     */
    template <typename T, typename = void>
    struct IsIterable : std::false_type {};
    
    template <typename T>
    struct IsIterable<T, std::void_t<
        decltype(std::begin(std::declval<T&>())), 
        decltype(std::end(std::declval<T&>()))>>
        : std::true_type {};

    /**
     * @brief Trait to check if a type has key_compare (ordered associative containers).
     * @tparam T The type to check.
     */
    template <typename T, typename = void>
    struct HasKeyCompare : std::false_type {};
    
    template <typename T>
    struct HasKeyCompare<T, std::void_t<typename T::key_compare>> : std::true_type {};

    /**
     * @brief Trait to check if a type has key_equal (unordered associative containers).
     * @tparam T The type to check.
     */
    template <typename T, typename = void>
    struct HasKeyEqual : std::false_type {};
    
    template <typename T>
    struct HasKeyEqual<T, std::void_t<typename T::key_equal>> : std::true_type {};

    /**
     * @brief Trait to check if a type has mapped_type (maps vs sets).
     * @tparam T The type to check.
     */
    template <typename T, typename = void>
    struct HasMappedType : std::false_type {};
    
    template <typename T>
    struct HasMappedType<T, std::void_t<typename T::mapped_type>> : std::true_type {};

    /**
     * @brief Alias to get the `value_type` of a container.
     * @tparam Container The container type.
     */
    template <typename Container>
    using ContainerValueT = typename Container::value_type;

    /**
     * @brief Forward declaration for recursive tuple comparison.
     * @tparam Policy The comparison policy.
     * @tparam Tuple The tuple type.
     * @tparam I An `std::index_sequence` for iterating tuple elements.
     * @tparam EpsParams Parameter pack for epsilon values.
     * @param a The first tuple.
     * @param b The second tuple.
     * @param indices The index sequence for tuple elements.
     * @param eps Parameters for epsilon comparison.
     * @return `true` if tuples are equal, `false` otherwise.
     */
    template <typename Policy, typename Tuple, std::size_t... I,
        typename... EpsParams>
    bool tupleAreEqualImpl(const Tuple& a, const Tuple& b,
        std::index_sequence<I...>, EpsParams... eps);

    // ====================================================================
    // EqualDispatcher - Central Dispatching Mechanism
    // ====================================================================
    
    /**
     * @brief Dispatcher for equality comparison based on type and policy.
     * 
     * This struct uses template specialization and SFINAE to route comparison
     * requests to the appropriate comparison method based on the type:
     * - Pairs: Compare first and second separately
     * - Tuples: Compare each element recursively
     * - Iterable containers: Compare sizes, then elements
     * - Floating-point: Delegate to Policy
     * - Other types: Use operator==
     * 
     * @tparam T The type being compared.
     * @tparam Policy The comparison policy for floating-point types.
     */
    template <typename T, typename Policy>
    struct EqualDispatcher {
        /**
         * @brief Recursively compares two objects.
         * @tparam EpsParams Optional epsilon parameters for floating-point comparison.
         * @param a The first object.
         * @param b The second object.
         * @param eps Optional epsilon parameters.
         * @return True if objects are equal according to the policy.
         */
        template <typename... EpsParams>
        static bool compare(const T& a, const T& b, EpsParams... eps) {
            
            // === PAIR COMPARISON ===
            if constexpr (IsPair<T>::value) {
                bool first = EqualDispatcher<typename T::first_type, Policy>::compare(
                    a.first, b.first, eps...);
                bool second = EqualDispatcher<typename T::second_type, Policy>::compare(
                    a.second, b.second, eps...);
                return first && second;
            }
            
            // === TUPLE COMPARISON ===
            else if constexpr (IsTuple<T>::value) {
                return tupleAreEqualImpl<Policy>(
                    a, b, std::make_index_sequence<std::tuple_size_v<T>>{},
                    eps...);
            }
            
            // === CONTAINER COMPARISON ===
            else if constexpr (IsIterable<T>::value) {
                // Check sizes first
                if (a.size() != b.size()) {
                    diagnostic::conditionalPrintError([&]() -> std::string {
                        return std::string("Containers have different sizes: ") +
                            std::to_string(a.size()) + " vs " +
                            std::to_string(b.size());
                        });
                    return false;
                }
                
                bool success = true;
                
                // --- UNORDERED ASSOCIATIVE CONTAINERS ---
                if constexpr (HasKeyEqual<T>::value) {
                    for (const auto& elemA : a) {
                        using KeyT = typename T::key_type;
                        
                        // Handle maps (have mapped_type)
                        if constexpr (HasMappedType<T>::value) {
                            const KeyT& keyA = elemA.first;
                            auto itB = b.find(keyA);
                            if (itB == b.end()) {
                                diagnostic::conditionalPrintError([&]() -> std::string {
                                    return std::string("Key not found in second container: ") + 
                                           toString(keyA);
                                    });
                                return false;
                            }
                            const auto& valA = elemA.second;
                            const auto& valB = itB->second;
                            bool result = EqualDispatcher<std::decay_t<decltype(valA)>, Policy>::compare(
                                valA, valB, eps...);
                            if (!result) {
                                success = false;
                                diagnostic::conditionalPrintError([&]() -> std::string {
                                    return std::string("Values for key '") + 
                                           toString(keyA) + "' differ.";
                                    });
                                if (kStopOnFirstError) {
                                    return false;
                                }
                            }
                        }
                        // Handle sets (no mapped_type)
                        else {
                            const KeyT& keyA = elemA;
                            auto itB = b.find(keyA);
                            if (itB == b.end()) {
                                diagnostic::conditionalPrintError([&]() -> std::string {
                                    return std::string("Key not found in second container: ") + 
                                           toString(keyA);
                                    });
                                return false;
                            }
                            const auto& valA = elemA;
                            const auto& valB = *itB;
                            bool result = EqualDispatcher<std::decay_t<decltype(valA)>, Policy>::compare(
                                valA, valB, eps...);
                            if (!result) {
                                success = false;
                                diagnostic::conditionalPrintError([&]() -> std::string {
                                    return std::string("Values for key '") + 
                                           toString(keyA) + "' differ.";
                                    });
                                if (kStopOnFirstError) {
                                    return false;
                                }
                            }
                        }
                    }
                    return success;
                }
                
                // --- ORDERED ASSOCIATIVE OR SEQUENTIAL CONTAINERS ---
                else {
                    auto it1 = std::begin(a);
                    auto it2 = std::begin(b);
                    while (it1 != std::end(a)) {
                        bool result = EqualDispatcher<ContainerValueT<T>, Policy>::compare(
                            *it1, *it2, eps...);
                        if (!result) {
                            success = false;
                            diagnostic::conditionalPrintError([&]() -> std::string {
                                return std::string("Container elements differ.");
                                });
                            if (kStopOnFirstError) {
                                return false;
                            }
                        }
                        ++it1;
                        ++it2;
                    }
                    return success;
                }
            }
            
            // === FLOATING-POINT COMPARISON ===
            else if constexpr (std::is_floating_point_v<T>) {
                return Policy::epsilonMatch(a, b, eps...);
            }
            
            // === TYPES WITH operator== ===
            else if constexpr (HasEqual<T>::value) {
                if (a == b) {
                    return true;
                }
                else {
                    diagnostic::conditionalPrintError([&]() -> std::string {
                        return std::string("Equality check failed: ") +
                            toString(a) + " and " +
                            toString(b) + " are not equal.";
                        });
                    return false;
                }
            }
            
            // === UNSUPPORTED TYPES ===
            else {
                diagnostic::conditionalPrintError([&]() -> std::string {
                    return std::string("Unsupported type for equality comparison: ") +
                        typeid(T).name();
                    });
                return false;
            }
        }
    };

    /**
     * @brief Helper for comparing tuples using index sequence and fold expression.
     * 
     * This function uses C++17 fold expressions to compare each element of a tuple
     * with the corresponding element in another tuple, using the appropriate policy.
     * 
     * @tparam Policy The comparison policy.
     * @tparam Tuple The tuple type.
     * @tparam I Index sequence for tuple elements.
     * @tparam EpsParams Optional epsilon parameters.
     * @param a The first tuple.
     * @param b The second tuple.
     * @param indices The index sequence.
     * @param eps Optional epsilon parameters.
     * @return True if all tuple elements are equal.
     */
    template <typename Policy, typename Tuple, std::size_t... I,
        typename... EpsParams>
    bool tupleAreEqualImpl(const Tuple& a, const Tuple& b,
        std::index_sequence<I...>, EpsParams... eps) {
        return (EqualDispatcher<std::tuple_element_t<I, Tuple>, Policy>::compare(
            std::get<I>(a), std::get<I>(b), eps...) &&
            ...);
    }

    // ====================================================================
    // Public Interface
    // ====================================================================
    
    /**
     * @brief Public interface for equality comparison.
     * 
     * This is the main entry point for comparing any two objects of the same type.
     * It supports:
     * - Floating-point numbers (with epsilon comparison)
     * - Integers and other built-in types
     * - Strings
     * - Containers (vector, array, map, set, unordered_map, unordered_set, etc.)
     * - Pairs and tuples
     * - Nested containers
     * - Any type with operator==
     * 
     * @tparam T The type being compared.
     * @tparam Policy The comparison policy for floating-point types (default: StandardComparisonPolicy).
     * @tparam EpsParams Optional epsilon parameters for floating-point comparison.
     * @param a The first object.
     * @param b The second object.
     * @param eps Optional epsilon parameters (policy-dependent).
     * @return True if objects are equal according to the policy.
     * 
     * @example
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
    [[nodiscard]]
    bool areEqual(const T& a, const T& b, EpsParams... eps) {
        return EqualDispatcher<T, Policy>::compare(a, b, eps...);
    }

} // namespace cpp_utilities
