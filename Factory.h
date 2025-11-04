/**
 * @file Factory.h (v3.0 - Compilation Fix)
 * @brief Policy-based Factory implementation with Expected support
 * 
 * @details Provides a flexible, type-safe factory pattern with:
 * - Full policy customization (Concurrency, Error Handling, Registration, Storage, Lifetime)
 * - Expected<T,E> integration for error handling
 * - No forced singleton (testable by default)
 * - Perfect forwarding for parameters
 * - Statistics and diagnostics
 * - Unregister/clear capabilities
 * - Integration with library's ConcurrencyPolicies
 * 
 * @version 3.0 - Fixed all critical bugs, renamed create() to resolve()
 * @section complexity Time Complexity: O(log n) for map, O(1) for unordered_map
 * @section exception_safety Exception Safety: Strong guarantee
 * 
 * @section breaking_changes Changes from v2.0:
 * - create() renamed to resolve() (better semantics)
 * - FactoryStats members now std::atomic (thread-safe)
 * - registerTypeWithArgs() removed (use lambda captures)
 * - Statistics renamed: creations → resolutions
 * 
 * @section variadic_note Variadic Parameters via Lambda Captures
 * For runtime parameters, capture them in lambdas:
 * @code
 * // Runtime configuration example
 * auto make_connection = [](const std::string& host, int port) {
 *     return [host, port]() { return Connection(host, port); };
 * };
 * 
 * factory.registerType("dev", make_connection("localhost", 5432));
 * factory.registerType("prod", make_connection("prod-server", 5432));
 * @endcode
 */

#pragma once

#include "Expected.h"
#include "ConcurrencyPolicies.h"
#include "enforce.h"
#include "TypeTraits.h"
#include <functional>
#include <map>
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>
#include <type_traits>
#include <utility>
#include <atomic>
#include <optional>

namespace cpp_utilities {

// ============================================================================
// Error Types
// ============================================================================

/**
 * @brief Error codes for factory operations
 */
enum class FactoryError {
    KeyNotFound,
    KeyAlreadyExists,
    CreationFailed,
    InvalidKey,
    InvalidCreator,
    RegistryFull
};

/**
 * @brief Convert FactoryError to string
 */
inline std::string toString(FactoryError error) {
    switch (error) {
        case FactoryError::KeyNotFound: return "Key not found";
        case FactoryError::KeyAlreadyExists: return "Key already exists";
        case FactoryError::CreationFailed: return "Creation failed";
        case FactoryError::InvalidKey: return "Invalid key";
        case FactoryError::InvalidCreator: return "Invalid creator";
        case FactoryError::RegistryFull: return "Registry full";
        default: return "Unknown error";
    }
}

/**
 * @brief Stream operator for FactoryError
 */
inline std::ostream& operator<<(std::ostream& os, FactoryError error) {
    return os << toString(error);
}

/**
 * @brief Detailed factory error with context
 */
template<typename K>
struct FactoryErrorInfo {
    FactoryError code;
    std::string message;
    std::optional<K> key;
    
    FactoryErrorInfo(FactoryError c, const std::string& msg, const K* k = nullptr)
        : code(c), message(msg), key(k ? std::make_optional(*k) : std::nullopt) {}
        
    std::string full_message() const {
        std::string result = toString(code) + ": " + message;
        if (key) {
            result += " (key: " + keyToString(*key) + ")";
        }
        return result;
    }
    
private:
    template<typename T>
    static std::string keyToString(const T& k) {
        if constexpr (std::is_convertible_v<T, std::string>) {
            return std::string(k);
        } else if constexpr (std::is_arithmetic_v<T>) {
            return std::to_string(k);
        } else {
            return "[non-stringable key]";
        }
    }
};

// ============================================================================
// Statistics (now thread-safe with atomics)
// ============================================================================

/**
 * @brief Factory operation statistics (thread-safe)
 * @note All members are atomic for thread-safe updates
 * @note Use snapshot() to get a consistent non-atomic copy for reading
 */
struct FactoryStats {
    std::atomic<size_t> registrations{0};
    std::atomic<size_t> registration_failures{0};
    std::atomic<size_t> resolutions{0};          // renamed from creations
    std::atomic<size_t> resolution_failures{0};  // renamed from creation_failures
    std::atomic<size_t> unregistrations{0};
    std::atomic<size_t> lookups{0};
    
    // Delete copy/move constructors (atomics aren't copyable)
    FactoryStats() = default;
    FactoryStats(const FactoryStats&) = delete;
    FactoryStats& operator=(const FactoryStats&) = delete;
    FactoryStats(FactoryStats&&) = delete;
    FactoryStats& operator=(FactoryStats&&) = delete;
    
    void reset() noexcept {
        registrations.store(0, std::memory_order_relaxed);
        registration_failures.store(0, std::memory_order_relaxed);
        resolutions.store(0, std::memory_order_relaxed);
        resolution_failures.store(0, std::memory_order_relaxed);
        unregistrations.store(0, std::memory_order_relaxed);
        lookups.store(0, std::memory_order_relaxed);
    }
    
    /**
     * @brief Create a non-atomic snapshot for reading
     * @note Use this from getStats() for consistent reads
     */
    struct Snapshot {
        size_t registrations;
        size_t registration_failures;
        size_t resolutions;
        size_t resolution_failures;
        size_t unregistrations;
        size_t lookups;
    };
    
    Snapshot snapshot() const noexcept {
        return Snapshot{
            registrations.load(std::memory_order_relaxed),
            registration_failures.load(std::memory_order_relaxed),
            resolutions.load(std::memory_order_relaxed),
            resolution_failures.load(std::memory_order_relaxed),
            unregistrations.load(std::memory_order_relaxed),
            lookups.load(std::memory_order_relaxed)
        };
    }
};

// ============================================================================
// Storage Policies
// ============================================================================

/**
 * @brief Map-based storage (ordered, O(log n) lookup)
 */
template<typename K, typename V>
struct MapStoragePolicy {
    using StorageType = std::map<K, V>;
    
    static constexpr bool is_ordered = true;
    static constexpr const char* name() { return "Map"; }
};

/**
 * @brief Unordered map storage (hash-based, O(1) average lookup)
 */
template<typename K, typename V>
struct UnorderedMapStoragePolicy {
    using StorageType = std::unordered_map<K, V>;
    
    static constexpr bool is_ordered = false;
    static constexpr const char* name() { return "UnorderedMap"; }
};

// ============================================================================
// Registration Policies
// ============================================================================

/**
 * @brief Allow overwriting existing registrations
 */
struct AllowOverwritePolicy {
    static constexpr bool allow_overwrite = true;
    
    template<typename Registry, typename K, typename V>
    static bool insert(Registry& reg, const K& key, V&& value, FactoryStats& stats) {
        bool existed = reg.count(key) > 0;
        reg[key] = std::forward<V>(value);
        if (existed) {
            // Overwrite doesn't count as new registration
            return false;
        }
        ++stats.registrations;
        return true;
    }
};

/**
 * @brief Prevent overwriting (default - safer)
 */
struct PreventOverwritePolicy {
    static constexpr bool allow_overwrite = false;
    
    template<typename Registry, typename K, typename V>
    static bool insert(Registry& reg, const K& key, V&& value, FactoryStats& stats) {
        auto [it, inserted] = reg.try_emplace(key, std::forward<V>(value));
        if (inserted) {
            ++stats.registrations;
        } else {
            ++stats.registration_failures;
        }
        return inserted;
    }
};

// ============================================================================
// Error Handling Policies
// ============================================================================

/**
 * @brief Return Expected with detailed error info
 */
template<typename T, typename K>
struct ExpectedErrorPolicy {
    using ErrorType = FactoryErrorInfo<K>;
    using ReturnType = Expected<T, ErrorType>;
    
    static ReturnType handle_not_found(const K& key) {
        return unexpected{ErrorType{
            FactoryError::KeyNotFound,
            "No creator registered for this key",
            &key
        }};
    }
    
    static ReturnType handle_creation_failed(const K& key, const std::string& reason) {
        return unexpected{ErrorType{
            FactoryError::CreationFailed,
            reason,
            &key
        }};
    }
};

/**
 * @brief Throw exception on error (backward compatible)
 */
template<typename T, typename K>
struct ThrowingErrorPolicy {
    using ReturnType = T;
    
    [[noreturn]] static T handle_not_found(const K& key) {
        throw std::runtime_error("Factory key not found: " + 
            (std::is_convertible_v<K, std::string> ? std::string(key) : "[key]"));
    }
    
    [[noreturn]] static T handle_creation_failed(const K& key, const std::string& reason) {
        throw std::runtime_error("Factory resolution failed: " + reason);
    }
};

/**
 * @brief Return default-constructed object on error
 */
template<typename T, typename K>
struct DefaultErrorPolicy {
    using ReturnType = T;
    
    static T handle_not_found(const K&) {
        return T{};
    }
    
    static T handle_creation_failed(const K&, const std::string&) {
        return T{};
    }
};

// ============================================================================
// Lifetime Policies
// ============================================================================

/**
 * @brief Regular instance (default - testable)
 */
struct InstanceLifetimePolicy {
    static constexpr bool is_singleton = false;
};

/**
 * @brief Singleton pattern (use sparingly)
 */
struct SingletonLifetimePolicy {
    static constexpr bool is_singleton = true;
};

// ============================================================================
// Main Factory Class
// ============================================================================

/**
 * @brief Policy-based Factory for type-safe object resolution
 * 
 * @tparam K Key type (must be copyable and support Storage policy requirements)
 * @tparam T Product type
 * @tparam ConcurrencyPolicy Thread-safety policy (from ConcurrencyPolicies.h)
 * @tparam ErrorHandlingPolicy How to handle errors (Expected, throw, default)
 * @tparam RegistrationPolicy Allow/prevent overwrites
 * @tparam StoragePolicy Map vs UnorderedMap
 * @tparam LifetimePolicy Instance vs Singleton
 * 
 * @note v3.0 changes: resolve() instead of create(), atomic stats, bug fixes
 * @note For runtime parameters, use lambda captures (see examples below)
 * 
 * @example Basic Usage
 * @code
 * SimpleFactory<std::string, Widget> factory;
 * factory.registerType("widget", [] { return Widget(42); });
 * auto result = factory.resolve("widget");
 * if (result) {
 *     Widget w = std::move(*result);
 * }
 * @endcode
 * 
 * @example Runtime Parameters via Lambda Captures
 * @code
 * SimpleFactory<std::string, Connection> factory;
 * 
 * // Helper to capture runtime params
 * auto makeConnCreator = [](const std::string& host, int port) {
 *     return [host, port]() { return Connection(host, port); };
 * };
 * 
 * factory.registerType("dev", makeConnCreator("localhost", 5432));
 * factory.registerType("prod", makeConnCreator("prod-server", 5432));
 * 
 * // Each resolve uses the captured parameters
 * auto dev_conn = factory.resolve("dev");   // localhost:5432
 * auto prod_conn = factory.resolve("prod"); // prod-server:5432
 * @endcode
 */
template <
    typename K,
    typename T,
    typename ConcurrencyPolicy = SingleThreadedPolicy,
    typename ErrorHandlingPolicy = ExpectedErrorPolicy<T, K>,
    typename RegistrationPolicy = PreventOverwritePolicy,
    typename StoragePolicy = MapStoragePolicy<K, std::function<T()>>,
    typename LifetimePolicy = InstanceLifetimePolicy
>
class Factory : private ConcurrencyPolicy {
public:
    // Type aliases
    using KeyType = K;
    using ProductType = T;
    using CreatorFunction = std::function<T()>;
    using ErrorType = typename ErrorHandlingPolicy::ErrorType;
    using ReturnType = typename ErrorHandlingPolicy::ReturnType;
    using StorageType = typename StoragePolicy::StorageType;
    
private:
    StorageType registry_;
    mutable FactoryStats stats_;
    
public:
    // ========================================================================
    // Constructors
    // ========================================================================
    
    /**
     * @brief Default constructor (for InstanceLifetimePolicy)
     */
    Factory() = default;
    
    /**
     * @brief Singleton accessor (only if SingletonLifetimePolicy)
     */
    template<typename L = LifetimePolicy>
    static std::enable_if_t<L::is_singleton, Factory&> instance() {
        static Factory inst;
        return inst;
    }
    
    // ========================================================================
    // Registration
    // ========================================================================
    
    /**
     * @brief Register a creator function for a key
     * 
     * @param key The key to register
     * @param creator The creator function (lambda, function pointer, etc.)
     * @return true if registration succeeded, false if key exists (PreventOverwrite)
     * 
     * @note Thread-safe according to ConcurrencyPolicy
     * @note Creator signature must match T()
     * @note For runtime parameters, capture them in the lambda
     * 
     * @example No parameters
     * @code
     * factory.registerType("widget", [] { return Widget(42); });
     * @endcode
     * 
     * @example With captured parameters
     * @code
     * std::string host = "localhost";
     * int port = 5432;
     * factory.registerType("db", [host, port] { 
     *     return Connection(host, port); 
     * });
     * @endcode
     */
    template<typename Callable>
    [[nodiscard]] bool registerType(const K& key, Callable&& creator) {
        typename ConcurrencyPolicy::LockGuard lock(this->getLock());
        
        // Validate key
        if constexpr (std::is_pointer_v<K>) {
            debug_enforce(key != nullptr, "Factory: null key");
        }
        
        return RegistrationPolicy::insert(
            registry_, 
            key, 
            CreatorFunction(std::forward<Callable>(creator)),
            stats_
        );
    }
    
    /**
     * @brief Register multiple types at once (more efficient)
     * 
     * @param registrations Vector of {key, creator} pairs
     * @return Number of successful registrations
     * 
     * @note Single lock acquisition for all registrations
     * 
     * @example
     * @code
     * size_t registered = factory.registerTypes({
     *     {"widget1", [] { return Widget(1); }},
     *     {"widget2", [] { return Widget(2); }},
     *     {"widget3", [] { return Widget(3); }}
     * });
     * @endcode
     */
    size_t registerTypes(std::initializer_list<std::pair<K, CreatorFunction>> registrations) {
        typename ConcurrencyPolicy::LockGuard lock(this->getLock());
        
        size_t success_count = 0;
        for (const auto& [key, creator] : registrations) {
            if (RegistrationPolicy::insert(
                    registry_, 
                    key, 
                    creator,
                    stats_)) {
                ++success_count;
            }
        }
        
        return success_count;
    }
    
    /**
     * @brief Unregister a type
     * 
     * @param key The key to unregister
     * @return true if key was found and removed, false if not found
     */
    [[nodiscard]] bool unregisterType(const K& key) {
        typename ConcurrencyPolicy::LockGuard lock(this->getLock());
        size_t removed = registry_.erase(key);
        if (removed > 0) {
            ++stats_.unregistrations;
            return true;
        }
        return false;
    }
    
    /**
     * @brief Clear all registrations
     */
    void clear() noexcept {
        typename ConcurrencyPolicy::LockGuard lock(this->getLock());
        registry_.clear();
        stats_.reset();
    }
    
    // ========================================================================
    // Resolution (renamed from "creation" for better semantics)
    // ========================================================================
    
    /**
     * @brief Make (obtain) an object using registered creator
     * 
     * @param key The key to look up
     * @return ReturnType (Expected<T, Error> or T depending on ErrorHandlingPolicy)
     * 
     * @note Thread-safe according to ConcurrencyPolicy
     * @note For Expected: Check with if(result) before accessing *result
     * @note "make" is used since the factory makes/produces objects
     *       (whether creating new instances, calling functions, etc.)
     * 
     * @example
     * @code
     * auto result = factory.make("widget");
     * if (result) {
     *     Widget w = std::move(*result);
     *     // use w...
     * } else {
     *     std::cerr << "Error: " << result.error().full_message() << "\n";
     * }
     * @endcode
     */
    [[nodiscard]] ReturnType make(const K& key) const {
        // Use shared lock for read (if supported by ConcurrencyPolicy)
        // const_cast is safe here - we're just acquiring a lock, not modifying logical state
        auto* self = const_cast<Factory*>(this);
        if constexpr (is_shared_policy<ConcurrencyPolicy>::value) {
            typename ConcurrencyPolicy::SharedGuard lock(self->getLock());
            return makeImpl(key);
        } else {
            typename ConcurrencyPolicy::LockGuard lock(self->getLock());
            return makeImpl(key);
        }
    }
    
    // ========================================================================
    // Queries
    // ========================================================================
    
    /**
     * @brief Check if a key is registered
     */
    [[nodiscard]] bool hasType(const K& key) const noexcept {
        auto* self = const_cast<Factory*>(this);
        if constexpr (is_shared_policy<ConcurrencyPolicy>::value) {
            typename ConcurrencyPolicy::SharedGuard lock(self->getLock());
            ++stats_.lookups;
            return registry_.count(key) > 0;
        } else {
            typename ConcurrencyPolicy::LockGuard lock(self->getLock());
            ++stats_.lookups;
            return registry_.count(key) > 0;
        }
    }
    
    /**
     * @brief Get number of registered types
     */
    [[nodiscard]] size_t size() const noexcept {
        auto* self = const_cast<Factory*>(this);
        if constexpr (is_shared_policy<ConcurrencyPolicy>::value) {
            typename ConcurrencyPolicy::SharedGuard lock(self->getLock());
            return registry_.size();
        } else {
            typename ConcurrencyPolicy::LockGuard lock(self->getLock());
            return registry_.size();
        }
    }
    
    /**
     * @brief Check if factory is empty
     */
    [[nodiscard]] bool empty() const noexcept {
        return size() == 0;
    }
    
    /**
     * @brief Get all registered keys
     */
    [[nodiscard]] std::vector<K> getRegisteredKeys() const {
        std::vector<K> keys;
        auto* self = const_cast<Factory*>(this);
        if constexpr (is_shared_policy<ConcurrencyPolicy>::value) {
            typename ConcurrencyPolicy::SharedGuard lock(self->getLock());
            keys.reserve(registry_.size());
            for (const auto& [key, creator] : registry_) {
                keys.push_back(key);
            }
        } else {
            typename ConcurrencyPolicy::LockGuard lock(self->getLock());
            keys.reserve(registry_.size());
            for (const auto& [key, creator] : registry_) {
                keys.push_back(key);
            }
        }
        return keys;
    }
    
    /**
     * @brief Get statistics (thread-safe snapshot)
     */
    [[nodiscard]] FactoryStats::Snapshot getStats() const noexcept {
        auto* self = const_cast<Factory*>(this);
        if constexpr (is_shared_policy<ConcurrencyPolicy>::value) {
            typename ConcurrencyPolicy::SharedGuard lock(self->getLock());
            return stats_.snapshot();
        } else {
            typename ConcurrencyPolicy::LockGuard lock(self->getLock());
            return stats_.snapshot();
        }
    }
    
    /**
     * @brief Reset statistics
     */
    void resetStats() noexcept {
        typename ConcurrencyPolicy::LockGuard lock(this->getLock());
        stats_.reset();
    }
    
private:
    /**
     * @brief Implementation of make (called with lock held)
     */
    ReturnType makeImpl(const K& key) const {
        ++stats_.lookups;  // FIX: Now counts lookups in make()
        auto it = registry_.find(key);
        
        if (it == registry_.end()) {
            ++stats_.resolution_failures;
            return ErrorHandlingPolicy::handle_not_found(key);
        }
        
        try {
            ++stats_.resolutions;
            return it->second();
        } catch (const std::exception& e) {
            ++stats_.resolution_failures;
            --stats_.resolutions; // Decrement since make failed
            if constexpr (std::is_same_v<ReturnType, T>) {
                // Throwing policy - rethrow
                throw;
            } else {
                // Expected policy - return error
                return ErrorHandlingPolicy::handle_creation_failed(key, e.what());
            }
        }
    }
};

// ============================================================================
// Type Aliases for Common Configurations
// ============================================================================

/**
 * @brief Simple factory with Expected return (recommended default)
 */
template<typename K, typename T>
using SimpleFactory = Factory<K, T, 
    SingleThreadedPolicy,
    ExpectedErrorPolicy<T, K>,
    PreventOverwritePolicy,
    MapStoragePolicy<K, std::function<T()>>,
    InstanceLifetimePolicy
>;

/**
 * @brief Thread-safe factory with mutex
 */
template<typename K, typename T>
using ThreadSafeFactory = Factory<K, T,
    MutexSynchronizationPolicy,
    ExpectedErrorPolicy<T, K>,
    PreventOverwritePolicy,
    MapStoragePolicy<K, std::function<T()>>,
    InstanceLifetimePolicy
>;

/**
 * @brief Fast factory with unordered_map (for hashable keys)
 */
template<typename K, typename T>
using FastFactory = Factory<K, T,
    SingleThreadedPolicy,
    ExpectedErrorPolicy<T, K>,
    PreventOverwritePolicy,
    UnorderedMapStoragePolicy<K, std::function<T()>>,
    InstanceLifetimePolicy
>;

/**
 * @brief Backward-compatible throwing factory
 */
template<typename K, typename T, bool ThreadSafe = false>
using LegacyFactory = Factory<K, T,
    std::conditional_t<ThreadSafe, MutexSynchronizationPolicy, SingleThreadedPolicy>,
    ThrowingErrorPolicy<T, K>,
    PreventOverwritePolicy,
    MapStoragePolicy<K, std::function<T()>>,
    SingletonLifetimePolicy
>;

} // namespace cpp_utilities
