// EqualityAny.h
#pragma once

#include <any>
#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "EqualityComparisons.h"
#include "Factory.h"

namespace fat_p {

 /**
 * @brief Function type for comparing std::any values, with two tolerance
 * doubles.
 */
 using AnyCompareFunc =
     std::function<bool(const std::any&, const std::any&, double, double)>;

 /**
 * @brief Custom fallback policy for unregistered type/policy (returns false).
 */
 struct AnyFallbackPolicy {
     static bool get() { return false; }
 };

 /**
 * @brief Type alias for the Factory used as the registry (thread-safe). */
 using AnyRegistryFactory = LegacyVariadicFactory<std::pair<std::type_index, std::type_index>,
     bool, true, AnyFallbackPolicy,
     const std::any&, const std::any&, double, double>;

 // Helper to provide the registry instance.
 inline AnyRegistryFactory& getAnyCompareRegistry() {
     return AnyRegistryFactory::instance();
 }

/**
 * @brief Registers a comparison function for a specific type and policy.
 *
 * @tparam T The type to register.
 * @tparam Policy The policy to use for comparison
 * (default StandardComparisonPolicy).
 */
template <typename T, typename Policy = StandardComparisonPolicy>
void registerAnyType() {
    auto key = std::make_pair(std::type_index(typeid(T)), std::type_index(typeid(Policy)));

    if constexpr (std::is_same_v<Policy, HybridComparisonPolicy>) {
        getAnyCompareRegistry().registerType(key,
            [](const std::any& a, const std::any& b, double relEps, double absEps) {
                return areEqual<T, Policy>(std::any_cast<const T&>(a), std::any_cast<const T&>(b), relEps, absEps);
            });
    }
    else {
        getAnyCompareRegistry().registerType(key,
            [](const std::any& a, const std::any& b, double eps, double /*unused*/) {
                return areEqual<T, Policy>(std::any_cast<const T&>(a), std::any_cast<const T&>(b), eps);
            });
    }
}


/**
 * @brief Registers a comparison function for a type that doesn't need epsilon.
 *
 * Handles non-floating-point types like integers and strings.
 *
 * @tparam T The non-numeric type to register.
 */
template <typename T>
void registerAnyTypeWithoutEpsilon() {
    auto key = std::make_pair(std::type_index(typeid(T)),
                            std::type_index(typeid(StandardComparisonPolicy)));
    getAnyCompareRegistry().registerType(key,
        [](const std::any& a, const std::any& b, 
        double /*unused*/, double /*unused*/) {
            return areEqual<T>(std::any_cast<const T&>(a),
                               std::any_cast<const T&>(b));
        });
}


/**
 * @brief Registers a given type with all available comparison policies.
 *
 * @tparam T The type to register with all policies.
 */
template <typename T>
void registerWithAllPolicies() {
    registerAnyType<T, StandardComparisonPolicy>();
    registerAnyType<T, RelativeComparisonPolicy>();
    registerAnyType<T, HybridComparisonPolicy>();
    registerAnyType<T, UlpComparisonPolicy>();
}

/**
* @brief Namespace for internal detail implementations (not for direct use).
*/
namespace detail {

static bool s_AnyEquality_Initialized = []() {
    registerAnyTypeWithoutEpsilon<bool>();
    registerAnyTypeWithoutEpsilon<int8_t>();
    registerAnyTypeWithoutEpsilon<int16_t>();
    registerAnyTypeWithoutEpsilon<int32_t>();
    registerAnyTypeWithoutEpsilon<int64_t>();
    registerAnyTypeWithoutEpsilon<uint8_t>();
    registerAnyTypeWithoutEpsilon<uint16_t>();
    registerAnyTypeWithoutEpsilon<uint32_t>();
    registerAnyTypeWithoutEpsilon<uint64_t>();
    registerAnyTypeWithoutEpsilon<int>();

    registerWithAllPolicies<float>();
    registerWithAllPolicies<double>();
    registerWithAllPolicies<::std::vector<float>>();
    registerWithAllPolicies<::std::vector<double>>();
    registerWithAllPolicies<::std::pair<double, double>>();
    registerWithAllPolicies<::std::pair<int, double>>();
    registerWithAllPolicies<::std::array<double, 3>>();
    registerWithAllPolicies<::std::vector<::std::pair<double, double>>>();
    registerAnyTypeWithoutEpsilon<::std::string>();
    registerAnyTypeWithoutEpsilon<::std::vector<::std::string>>();
    registerWithAllPolicies<::std::vector<::std::any>>();  
    registerWithAllPolicies<::std::deque<double>>(); 

    using MapType = ::std::unordered_map<::std::string, ::std::any>;
    registerWithAllPolicies<MapType>();

    return true;
    }();
} // namespace detail

 // Forward declaration with defaults and depth limit
 template <typename Policy = StandardComparisonPolicy>
 bool areEqual(const ::std::any& a, const std::any& b,
     double eps = kDefaultDoubleEpsilon, double eps2 = kDefaultDoubleEpsilon, int depth = 0);

 /**
  * @brief Specialization for std::any to delegate to dynamic areEqual.
  */
 template <typename Policy>
 struct EqualDispatcher<std::any, Policy> {
     template <typename... EpsParams>
     static bool compare(const std::any& a, const std::any& b, EpsParams... eps) {
         return areEqual<Policy>(a, b, eps...);
     }
 };

/**
 * @brief Compares two std::any objects for equality using the registered
 * comparison functions.
 *
 * @tparam Policy The comparison policy (default StandardComparisonPolicy).
 * @param a The first any.
 * @param b The second any.
 * @param eps A tolerance, or relative/absolute tolerances for hybrid.
 * @param depth Recursion depth for nested any (to prevent stack overflow).
 * @return True if equal (or both empty), false otherwise.
 */
template <typename Policy>
bool areEqual(const std::any& a, const std::any& b,
    double eps, double eps2, int depth) {
    if (depth > 10) {  // Prevent infinite recursion
        LOG_ERROR("Recursion depth exceeded in any comparison.");
        return false;
    }
    if (!a.has_value() && !b.has_value()) {
        return true;
    }
    if (a.has_value() != b.has_value() || a.type() != b.type()) {
        LOG_ERROR(std::string("Type mismatch or empty value in any object. a.type(): ") +
                (a.has_value() ? a.type().name() : "empty") +
                ", b.type(): " +
                (b.has_value() ? b.type().name() : "empty"));
        return false;
    }

    auto valueType = std::type_index(a.type());
    if (valueType == std::type_index(typeid(std::any))) {
        try {
            bool result = areEqual<Policy>(std::any_cast<const std::any&>(a),
                std::any_cast<const std::any&>(b), eps, eps2, depth + 1);
            return result;
        }
        catch (const std::bad_any_cast&) {
            LOG_ERROR(std::string("Bad any_cast for nested std::any."));
            return false;
        }
    }
    auto policyType = std::type_index(typeid(Policy));
    auto key = std::make_pair(valueType, policyType);

    if (getAnyCompareRegistry().hasType(key)) {
        bool result = getAnyCompareRegistry().create(key, a, b, eps, eps2);
        return result;
    }
    else {
        auto fallbackPolicyType = std::type_index(typeid(StandardComparisonPolicy));
        auto fallbackKey = std::make_pair(valueType, fallbackPolicyType);
        if (getAnyCompareRegistry().hasType(fallbackKey)) {
            LOG_ERROR(std::string("Requested policy not registered for type ") + std::string(valueType.name()) +
                    "; falling back to StandardComparisonPolicy.");
            bool result = getAnyCompareRegistry().create(fallbackKey, a, b, eps, eps2);
            return result;
        }
        else {
            LOG_ERROR(std::string("Unsupported type/policy in any for comparison (not registered): ") +
                    std::string(valueType.name()) + " with policy " + std::string(policyType.name()));
            return false;
        }
    }
}

} // namespace fat_p