/**
 * @file EqualityAny.h
 * @brief Runtime equality comparison for std::any via type registry.
 *
 * @layer Domain
 *
 * Extends EqualityComparisons.h to support runtime type comparison of std::any
 * values using a thread-safe registry of comparison functions.
 *
 * @note Thread-safety: Registry operations are thread-safe. However, the caller
 *       must ensure the std::any objects being compared are not concurrently
 *       modified during comparison.
 */
#pragma once

/*
FATP_META:
  meta_version: 1
  component: EqualityAny
  file_role: public_header
  path: include/fat_p/EqualityAny.h
  namespace: fat_p
  layer: Foundation
  summary: "Public header for EqualityAny."
  api_stability: in_work
  related:
    docs_search: "EqualityAny"
    tests:
      - components/Equality/tests/test_EqualityAny.cpp
    benchmarks:
      - benchmarks/benchmark_EqualityComparisonsAny.cpp
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
#include <any>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <string>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>

#include "DiagnosticLogger_Core.h"
#include "EqualityComparisons.h"
#include "Factory.h"

namespace fat_p
{

// ============================================================================
// Configuration Constants
// ============================================================================

/**
 * @brief Maximum recursion depth for nested std::any comparison.
 *
 * Prevents stack overflow when comparing deeply nested std::any structures,
 * including nesting through containers (e.g., any -> vector<any> -> any).
 * If this depth is exceeded, comparison returns false and logs an error.
 *
 * Depth counts the number of concurrent areEqual(std::any, ...) calls on the
 * current thread's call stack (tracked by a thread-local counter).
 * A value of 10 allows up to 10 concurrent std::any comparison frames; the
 * 11th nested std::any comparison fails with a recursion-depth error.
 */
inline constexpr std::size_t kMaxAnyRecursionDepth = 10;

/**
 * @brief Controls whether to fall back to StandardComparisonPolicy when
 * the requested policy is not registered for a type.
 *
 * - true:  Fall back to StandardComparisonPolicy (logs warning)
 * - false: Return false for unregistered policy/type combinations
 */
inline constexpr bool kAllowPolicyFallback = false;

// ============================================================================
// Registry Types
// ============================================================================

/**
 * @brief Custom fallback policy for unregistered type/policy (returns false).
 */
struct AnyFallbackPolicy
{
    static bool get()
    {
        return false;
    }
};

/**
 * @brief Registry for comparisons using type-appropriate default epsilon.
 *
 * Registered functions take only (any&, any&) and call areEqual<T, Policy>
 * without epsilon parameters, allowing the policy to use type-specific defaults.
 */
using AnyDefaultRegistryFactory = LegacyVariadicFactory<std::pair<std::type_index, std::type_index>,
                                                        bool,
                                                        true,
                                                        AnyFallbackPolicy,
                                                        const std::any&,
                                                        const std::any&>;

/**
 * @brief Registry for comparisons with explicit epsilon values.
 *
 * Registered functions take (any&, any&, double, double) and forward the
 * epsilon values to areEqual<T, Policy>.
 */
using AnyExplicitRegistryFactory = LegacyVariadicFactory<std::pair<std::type_index, std::type_index>,
                                                         bool,
                                                         true,
                                                         AnyFallbackPolicy,
                                                         const std::any&,
                                                         const std::any&,
                                                         double,
                                                         double>;

/**
 * @brief Returns the singleton registry for default-epsilon comparisons.
 * @return Reference to the global AnyDefaultRegistryFactory.
 */
inline AnyDefaultRegistryFactory& getAnyDefaultRegistry()
{
    return AnyDefaultRegistryFactory::instance();
}

/**
 * @brief Returns the singleton registry for explicit-epsilon comparisons.
 * @return Reference to the global AnyExplicitRegistryFactory.
 */
inline AnyExplicitRegistryFactory& getAnyExplicitRegistry()
{
    return AnyExplicitRegistryFactory::instance();
}

// ============================================================================
// Registration Functions
// ============================================================================

/**
 * @brief Registers comparison functions for a specific type and policy.
 *
 * Registers in both the default-epsilon and explicit-epsilon registries.
 *
 * @tparam T The type to register.
 * @tparam Policy The policy to use for comparison (default StandardComparisonPolicy).
 *
 * @note The lambda captures no state and may throw std::bad_any_cast if the
 *       std::any objects do not contain type T.
 */
template <typename T, typename Policy = StandardComparisonPolicy>
void registerAnyType()
{
    auto key = std::make_pair(std::type_index(typeid(T)), std::type_index(typeid(Policy)));

    // Register default-epsilon version (uses policy/type defaults)
    (void)getAnyDefaultRegistry().registerType(key, [](const std::any& a, const std::any& b) {
        return areEqual<T, Policy>(std::any_cast<const T&>(a), std::any_cast<const T&>(b));
    });

    // Register explicit-epsilon version
    if constexpr (std::is_same_v<Policy, HybridComparisonPolicy>)
    {
        (void)getAnyExplicitRegistry().registerType(
            key,
            [](const std::any& a, const std::any& b, double relEps, double absEps) {
                return areEqual<T, Policy>(std::any_cast<const T&>(a), std::any_cast<const T&>(b), relEps, absEps);
            });
    }
    else
    {
        (void)getAnyExplicitRegistry().registerType(
            key,
            [](const std::any& a, const std::any& b, double eps, double /*unused*/) {
                return areEqual<T, Policy>(std::any_cast<const T&>(a), std::any_cast<const T&>(b), eps);
            });
    }
}

/**
 * @brief Registers a comparison function for a type that doesn't need epsilon.
 *
 * Handles integral types, strings, and other types where exact equality is used.
 * Uses operator== directly, which means composite types (pairs, tuples, containers)
 * registered via this function will NOT emit structural mismatch diagnostics
 * (no index/key information on failure). For structural diagnostics on composite
 * types, use registerAnyType<T, Policy>() instead.
 *
 * @tparam T The type to register.
 */
template <typename T>
void registerAnyTypeWithoutEpsilon()
{
    auto registerForPolicy = [](auto policyTag) {
        using Policy = std::decay_t<decltype(policyTag)>;

        auto key = std::make_pair(std::type_index(typeid(T)), std::type_index(typeid(Policy)));

        // Both registries use exact equality for non-epsilon types
        (void)getAnyDefaultRegistry().registerType(key, [](const std::any& a, const std::any& b) {
            return std::any_cast<const T&>(a) == std::any_cast<const T&>(b);
        });

        (void)getAnyExplicitRegistry().registerType(
            key,
            [](const std::any& a, const std::any& b, double /*unused*/, double /*unused*/) {
                return std::any_cast<const T&>(a) == std::any_cast<const T&>(b);
            });
    };

    registerForPolicy(StandardComparisonPolicy{});
    registerForPolicy(RelativeComparisonPolicy{});
    registerForPolicy(HybridComparisonPolicy{});
    registerForPolicy(UlpComparisonPolicy{});
}

/**
 * @brief Registers a given type with all available comparison policies.
 *
 * @tparam T The type to register with all policies.
 */
template <typename T>
void registerWithAllPolicies()
{
    registerAnyType<T, StandardComparisonPolicy>();
    registerAnyType<T, RelativeComparisonPolicy>();
    registerAnyType<T, HybridComparisonPolicy>();
    registerAnyType<T, UlpComparisonPolicy>();
}

// ============================================================================
// Depth Tracking
// ============================================================================

namespace detail
{

/**
 * @brief Thread-local recursion depth tracker for std::any comparison.
 *
 * Encapsulates the depth counter and provides safe increment/decrement
 * operations with underflow detection. Tracks depth across all entry points
 * into std::any comparison, including through container element comparisons.
 */
struct AnyDepthTracker
{
    inline static thread_local std::size_t sDepth = 0;

    static void increment() noexcept
    {
        ++sDepth;
    }

    static void decrement() noexcept
    {
        // Underflow indicates a bug in guard pairing. Debug-detectable via assert;
        // release builds clamp to zero to prevent wrap-around corruption.
        assert(sDepth > 0 && "AnyDepthTracker: depth counter underflow");
        if (sDepth > 0)
        {
            --sDepth;
        }
    }

    static std::size_t current() noexcept
    {
        return sDepth;
    }
};

/**
 * @brief RAII guard for incrementing/decrementing the recursion depth counter.
 *
 * Ensures proper cleanup on all exit paths including exceptions.
 */
struct AnyDepthGuard
{
    AnyDepthGuard() noexcept
    {
        AnyDepthTracker::increment();
    }

    ~AnyDepthGuard() noexcept
    {
        AnyDepthTracker::decrement();
    }

    AnyDepthGuard(const AnyDepthGuard&) = delete;
    AnyDepthGuard& operator=(const AnyDepthGuard&) = delete;
    AnyDepthGuard(AnyDepthGuard&&) = delete;
    AnyDepthGuard& operator=(AnyDepthGuard&&) = delete;
};

} // namespace detail

// ============================================================================
// Lazy Initialization
// ============================================================================

namespace detail
{

/**
 * @brief Ensures the default types are registered in the comparison registry.
 *
 * Uses a function-local static for thread-safe, first-use initialization.
 * This avoids static initialization order issues.
 *
 * @note This function is called automatically by areEqual for std::any.
 *       Users can also call it explicitly to ensure registration before
 *       first comparison.
 */
inline void ensureAnyEqualityRegistered()
{
    static const bool initialized = []() {
        // Fixed-width integer types (no epsilon needed)
        // Note: We register both fixed-width and fundamental types to ensure
        // portability. On platforms where int == int32_t, duplicate registration
        // is harmless (try_emplace ignores duplicates).
        registerAnyTypeWithoutEpsilon<bool>();
        registerAnyTypeWithoutEpsilon<int8_t>();
        registerAnyTypeWithoutEpsilon<int16_t>();
        registerAnyTypeWithoutEpsilon<int32_t>();
        registerAnyTypeWithoutEpsilon<int64_t>();
        registerAnyTypeWithoutEpsilon<uint8_t>();
        registerAnyTypeWithoutEpsilon<uint16_t>();
        registerAnyTypeWithoutEpsilon<uint32_t>();
        registerAnyTypeWithoutEpsilon<uint64_t>();

        // Fundamental integer types (may overlap with fixed-width on some platforms)
        registerAnyTypeWithoutEpsilon<char>();
        registerAnyTypeWithoutEpsilon<signed char>();
        registerAnyTypeWithoutEpsilon<unsigned char>();
        registerAnyTypeWithoutEpsilon<short>();
        registerAnyTypeWithoutEpsilon<unsigned short>();
        registerAnyTypeWithoutEpsilon<int>();
        registerAnyTypeWithoutEpsilon<unsigned int>();
        registerAnyTypeWithoutEpsilon<long>();
        registerAnyTypeWithoutEpsilon<unsigned long>();
        registerAnyTypeWithoutEpsilon<long long>();
        registerAnyTypeWithoutEpsilon<unsigned long long>();

        // Floating-point types (all policies)
        registerWithAllPolicies<float>();
        registerWithAllPolicies<double>();
        // Note: long double is not supported by UlpComparisonPolicy (only float/double)
        registerAnyType<long double, StandardComparisonPolicy>();
        registerAnyType<long double, RelativeComparisonPolicy>();
        registerAnyType<long double, HybridComparisonPolicy>();

        // Container types
        registerWithAllPolicies<std::vector<float>>();
        registerWithAllPolicies<std::vector<double>>();
        registerWithAllPolicies<std::vector<int>>();
        registerWithAllPolicies<std::deque<double>>();

        // Pair types
        registerWithAllPolicies<std::pair<double, double>>();
        registerWithAllPolicies<std::pair<int, double>>();
        registerWithAllPolicies<std::pair<int, int>>();
        registerAnyTypeWithoutEpsilon<std::pair<std::string, std::string>>();
        registerAnyTypeWithoutEpsilon<std::pair<std::string, int>>();

        // Tuple types
        registerWithAllPolicies<std::tuple<double>>();
        registerWithAllPolicies<std::tuple<double, double>>();
        registerWithAllPolicies<std::tuple<double, double, double>>();
        registerAnyTypeWithoutEpsilon<std::tuple<int, int>>();
        registerAnyTypeWithoutEpsilon<std::tuple<std::string, int>>();

        // Array types (common sizes)
        registerWithAllPolicies<std::array<double, 2>>();
        registerWithAllPolicies<std::array<double, 3>>();
        registerWithAllPolicies<std::array<double, 4>>();
        registerWithAllPolicies<std::array<float, 3>>();
        registerWithAllPolicies<std::array<float, 4>>();

        // Nested container types
        registerWithAllPolicies<std::vector<std::pair<double, double>>>();
        // Note: std::vector<std::any> is not pre-registered because comparing
        // containers of std::any requires careful consideration of recursion
        // limits and diagnostic output. Users can register custom types if needed.

        // String types (no epsilon needed)
        registerAnyTypeWithoutEpsilon<std::string>();
        registerAnyTypeWithoutEpsilon<std::vector<std::string>>();

        // Map types
        // Note: std::unordered_map<std::string, std::any> is not pre-registered
        // for the same reasons as std::vector<std::any>.
        registerWithAllPolicies<std::unordered_map<std::string, double>>();
        registerWithAllPolicies<std::unordered_map<std::string, int>>();

        return true;
    }();
    (void)initialized;
}

} // namespace detail

// ============================================================================
// Forward Declarations
// ============================================================================

/**
 * @brief Compares two std::any values using type-appropriate default epsilon.
 *
 * Uses the underlying type's policy-specific default tolerance. For example,
 * float values use kDefaultFloatEpsilon, double values use kDefaultDoubleEpsilon.
 */
template <typename Policy = StandardComparisonPolicy>
bool areEqual(const std::any& a, const std::any& b);

/**
 * @brief Compares two std::any values using explicit epsilon.
 *
 * @param eps The tolerance to use. For HybridComparisonPolicy, this is the
 *            relative tolerance.
 */
template <typename Policy = StandardComparisonPolicy>
bool areEqual(const std::any& a, const std::any& b, double eps);

/**
 * @brief Compares two std::any values using two explicit epsilon values.
 *
 * @param relEps For HybridComparisonPolicy, the relative tolerance.
 * @param absEps For HybridComparisonPolicy, the absolute tolerance.
 */
template <typename Policy = StandardComparisonPolicy>
bool areEqual(const std::any& a, const std::any& b, double relEps, double absEps);

// ============================================================================
// EqualDispatcher Specialization for std::any
// ============================================================================

/**
 * @brief Specialization for std::any to delegate to runtime areEqual.
 *
 * @note Recursion depth is tracked via thread-local counter across all
 *       entry points, including container element comparisons.
 */
template <typename Policy>
struct EqualDispatcher<std::any, Policy>
{
    template <typename... EpsParams>
    static bool compare(const std::any& a, const std::any& b, EpsParams... eps)
    {
        static_assert(sizeof...(EpsParams) <= 2, "std::any comparison accepts at most 2 epsilon parameters");
        return areEqual<Policy>(a, b, eps...);
    }
};

// ============================================================================
// Implementation Details
// ============================================================================

namespace detail
{

/**
 * @brief Common validation and nested-any handling for std::any comparison.
 *
 * @return true if comparison should proceed, false if result is already determined.
 * @param[out] result The comparison result if returning false.
 * @param eps Optional epsilon parameters to forward to nested std::any comparisons.
 */
template <typename Policy, typename... EpsParams>
bool validateAndHandleNested(const std::any& a, const std::any& b, bool& result, EpsParams... eps)
{
    const std::size_t depth = AnyDepthTracker::current();
    if (depth > kMaxAnyRecursionDepth)
    {
        FATP_LOG_ERROR(std::string("Recursion depth exceeded (") + std::to_string(depth) + " > " +
                       std::to_string(kMaxAnyRecursionDepth) + ") in std::any comparison. Possible excessive nesting.");
        result = false;
        return false;
    }

    // Both empty is equal
    if (!a.has_value() && !b.has_value())
    {
        result = true;
        return false;
    }

    // One empty, one not, or type mismatch
    if (a.has_value() != b.has_value() || a.type() != b.type())
    {
        FATP_LOG_ERROR(std::string("Type mismatch in std::any comparison. a: ") +
                       (a.has_value() ? a.type().name() : "(empty)") +
                       ", b: " + (b.has_value() ? b.type().name() : "(empty)"));
        result = false;
        return false;
    }

    // Handle nested std::any
    auto valueType = std::type_index(a.type());
    if (valueType == std::type_index(typeid(std::any)))
    {
        try
        {
            result = areEqual<Policy>(std::any_cast<const std::any&>(a), std::any_cast<const std::any&>(b), eps...);
            return false;
        }
        catch (const std::bad_any_cast& e)
        {
            FATP_LOG_ERROR(std::string("Bad any_cast for nested std::any: ") + e.what());
            result = false;
            return false;
        }
    }

    return true; // Proceed with normal comparison
}

/**
 * @brief Logs error for unregistered type/policy combination.
 */
inline void logUnregisteredType(std::type_index valueType, std::type_index policyType)
{
    FATP_LOG_ERROR(std::string("No comparison handler registered for type '") + valueType.name() + "' with policy '" +
                   policyType.name() + "'. Use registerAnyType<T, Policy>() to register.");
}

} // namespace detail

// ============================================================================
// Implementation: Default Epsilon
// ============================================================================

/**
 * @brief Compares two std::any objects using type-appropriate default tolerance.
 *
 * @tparam Policy The comparison policy (default StandardComparisonPolicy).
 * @param a The first any.
 * @param b The second any.
 * @return True if equal (or both empty), false otherwise.
 *
 * @note Complexity: O(1) registry lookup + O(N) for type comparison.
 * @note Thread-safety: Registry lookup is thread-safe. Caller must ensure
 *       the std::any objects are not modified during comparison.
 *
 * @warning Returns false if recursion depth exceeds kMaxAnyRecursionDepth.
 */
template <typename Policy>
bool areEqual(const std::any& a, const std::any& b)
{
    detail::ensureAnyEqualityRegistered();
    detail::AnyDepthGuard depthGuard;

    bool result;
    if (!detail::validateAndHandleNested<Policy>(a, b, result))
    {
        return result;
    }

    auto valueType = std::type_index(a.type());
    auto policyType = std::type_index(typeid(Policy));
    auto key = std::make_pair(valueType, policyType);

    if (getAnyDefaultRegistry().hasType(key))
    {
        return getAnyDefaultRegistry().create(key, a, b);
    }

    // Optionally fall back to StandardComparisonPolicy
    if constexpr (kAllowPolicyFallback)
    {
        if constexpr (!std::is_same_v<Policy, StandardComparisonPolicy>)
        {
            auto fallbackPolicyType = std::type_index(typeid(StandardComparisonPolicy));
            auto fallbackKey = std::make_pair(valueType, fallbackPolicyType);

            if (getAnyDefaultRegistry().hasType(fallbackKey))
            {
                FATP_LOG_WARNING(std::string("Policy '") + policyType.name() + "' not registered for type '" +
                                 valueType.name() + "'; falling back to StandardComparisonPolicy.");
                return getAnyDefaultRegistry().create(fallbackKey, a, b);
            }
        }
    }

    detail::logUnregisteredType(valueType, policyType);
    return false;
}

// ============================================================================
// Implementation: Single Explicit Epsilon
// ============================================================================

/**
 * @brief Compares two std::any objects using explicit epsilon.
 *
 * @tparam Policy The comparison policy (default StandardComparisonPolicy).
 * @param a The first any.
 * @param b The second any.
 * @param eps Tolerance value. For HybridComparisonPolicy, this value is used
 *        as both relative and absolute tolerance (forwards as eps, eps).
 * @return True if equal (or both empty), false otherwise.
 */
template <typename Policy>
bool areEqual(const std::any& a, const std::any& b, double eps)
{
    return areEqual<Policy>(a, b, eps, eps);
}

// ============================================================================
// Implementation: Two Explicit Epsilons
// ============================================================================

/**
 * @brief Compares two std::any objects using two explicit epsilon values.
 *
 * @tparam Policy The comparison policy (default StandardComparisonPolicy).
 * @param a The first any.
 * @param b The second any.
 * @param relEps Tolerance value (relative for HybridComparisonPolicy).
 * @param absEps Second tolerance (absolute for HybridComparisonPolicy).
 * @return True if equal (or both empty), false otherwise.
 *
 * @note Complexity: O(1) registry lookup + O(N) for type comparison.
 * @note Thread-safety: Registry lookup is thread-safe. Caller must ensure
 *       the std::any objects are not modified during comparison.
 *
 * @warning Returns false if recursion depth exceeds kMaxAnyRecursionDepth.
 */
template <typename Policy>
bool areEqual(const std::any& a, const std::any& b, double relEps, double absEps)
{
    detail::ensureAnyEqualityRegistered();
    detail::AnyDepthGuard depthGuard;

    bool result;
    if (!detail::validateAndHandleNested<Policy>(a, b, result, relEps, absEps))
    {
        return result;
    }

    auto valueType = std::type_index(a.type());
    auto policyType = std::type_index(typeid(Policy));
    auto key = std::make_pair(valueType, policyType);

    if (getAnyExplicitRegistry().hasType(key))
    {
        return getAnyExplicitRegistry().create(key, a, b, relEps, absEps);
    }

    // Optionally fall back to StandardComparisonPolicy
    if constexpr (kAllowPolicyFallback)
    {
        if constexpr (!std::is_same_v<Policy, StandardComparisonPolicy>)
        {
            auto fallbackPolicyType = std::type_index(typeid(StandardComparisonPolicy));
            auto fallbackKey = std::make_pair(valueType, fallbackPolicyType);

            if (getAnyExplicitRegistry().hasType(fallbackKey))
            {
                FATP_LOG_WARNING(std::string("Policy '") + policyType.name() + "' not registered for type '" +
                                 valueType.name() + "'; falling back to StandardComparisonPolicy.");
                return getAnyExplicitRegistry().create(fallbackKey, a, b, relEps, absEps);
            }
        }
    }

    detail::logUnregisteredType(valueType, policyType);
    return false;
}

} // namespace fat_p
