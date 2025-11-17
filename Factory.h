/**
 * @file Factory.h (v3.1 - Hybrid Compatibility)
 * @brief Factory with both modern policies and legacy variadic parameter support
 * 
 * @details Two Factory implementations:
 * 1. Factory - Modern policy-based factory (v3.0 style) with variadic parameters
 * 2. SimpleVariadicFactory - Lightweight factory matching old Factory.hpp for EqualityAny
 * 
 * @version 3.1
 * @note Use SimpleVariadicFactory for EqualityAny.h compatibility
 * @note Use Factory for new code with full policy customization
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
#include <shared_mutex>

namespace fat_p {

// ============================================================================
// Error Types (shared by both implementations)
// ============================================================================

enum class FactoryError {
    KeyNotFound,
    KeyAlreadyExists,
    CreationFailed,
    InvalidKey,
    InvalidCreator,
    RegistryFull
};

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

inline std::ostream& operator<<(std::ostream& os, FactoryError error) {
    return os << toString(error);
}

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
// Statistics
// ============================================================================

struct FactoryStats {
    std::atomic<size_t> registrations{0};
    std::atomic<size_t> registration_failures{0};
    std::atomic<size_t> resolutions{0};
    std::atomic<size_t> resolution_failures{0};
    std::atomic<size_t> unregistrations{0};
    std::atomic<size_t> lookups{0};
    
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
// PART 1: Simple Variadic Factory (for EqualityAny.h compatibility)
// ============================================================================

/**
 * @brief Default fallback policy: returns default-constructed object
 */
template <typename T>
struct DefaultFallbackPolicy {
    static T get() { return T{}; }
};

/**
 * @brief Throwing fallback policy: throws on key not found
 */
template <typename T>
struct ThrowingFallbackPolicy {
    static T get() { throw std::runtime_error("Factory key not found."); }
};

/**
 * @brief Simplified factory matching old Factory.hpp for EqualityAny compatibility
 * 
 * @tparam K Key type
 * @tparam T Return type
 * @tparam ThreadSafe Enable thread safety with shared_mutex
 * @tparam FallbackPolicy Policy for missing keys
 * @tparam Params Variadic parameter types for creator functions
 * 
 * @note This matches the old Factory.hpp signature exactly
 * @note All creators must have signature: T(Params...)
 * @note Thread-safe operations use shared_mutex when ThreadSafe=true
 * 
 * @example EqualityAny usage
 * @code
 * using AnyRegistryFactory = SimpleVariadicFactory<
 *     std::pair<std::type_index, std::type_index>,
 *     bool,
 *     true,  // Thread-safe
 *     AnyFallbackPolicy,
 *     const std::any&, const std::any&, double, double
 * >;
 * 
 * factory.registerType(key, [](const std::any& a, const std::any& b, double e1, double e2) {
 *     return compare(a, b, e1, e2);
 * });
 * 
 * bool result = factory.create(key, anyA, anyB, 0.001, 0.001);
 * @endcode
 */
template <
    typename K,
    typename T,
    bool ThreadSafe = false,
    typename FallbackPolicy = DefaultFallbackPolicy<T>,
    typename... Params
>
class SimpleVariadicFactory {
public:
    using CreatorFunction = std::function<T(Params...)>;
    
private:
    using Registry = std::map<K, CreatorFunction>;
    
    // Null mutex for non-thread-safe mode
    struct NullMutex {
        void lock() const {}
        void unlock() const {}
        bool try_lock() const { return true; }
        void lock_shared() const {}
        void unlock_shared() const {}
        bool try_lock_shared() const { return true; }
    };
    
    using MutexType = std::conditional_t<ThreadSafe, std::shared_mutex, NullMutex>;
    
    Registry mRegistry;
    mutable MutexType mMutex;
    
public:
    /**
     * @brief Returns the singleton instance
     */
    static SimpleVariadicFactory& instance() {
        static SimpleVariadicFactory inst;
        return inst;
    }
    
    /**
     * @brief Registers a creator function for the given key
     * 
     * @tparam Callable Callable type (function, lambda, etc.)
     * @param key The key to associate with the creator
     * @param creator The callable with signature T(Params...)
     * @return true if registration succeeded, false if key already exists
     */
    template <typename Callable>
    bool registerType(const K& key, Callable&& creator) {
        std::unique_lock<MutexType> lock(mMutex);
        return mRegistry.try_emplace(key, std::forward<Callable>(creator)).second;
    }
    
    /**
     * @brief Creates an object using the registered creator
     * 
     * @param key The key to look up
     * @param params Parameters to forward to the creator
     * @return The created object or fallback value if key not found
     */
    T create(const K& key, Params... params) const {
        std::shared_lock<MutexType> lock(mMutex);
        auto it = mRegistry.find(key);
        if (it != mRegistry.end()) {
            return (it->second)(std::forward<Params>(params)...);
        } else {
            return FallbackPolicy::get();
        }
    }
    
    /**
     * @brief Checks if a creator is registered for the given key
     */
    bool hasType(const K& key) const {
        std::shared_lock<MutexType> lock(mMutex);
        return mRegistry.count(key) > 0;
    }
    
    /**
     * @brief Get number of registered creators
     */
    size_t size() const {
        std::shared_lock<MutexType> lock(mMutex);
        return mRegistry.size();
    }
    
    /**
     * @brief Check if factory is empty
     */
    bool empty() const {
        return size() == 0;
    }
    
    /**
     * @brief Clear all registrations
     */
    void clear() {
        std::unique_lock<MutexType> lock(mMutex);
        mRegistry.clear();
    }
    
private:
    SimpleVariadicFactory() = default;
};

// ============================================================================
// Legacy name for OLD Factory.hpp (EqualityAny compatibility)
// ============================================================================

/**
 * @brief Old-style Factory for EqualityAny.h compatibility
 * @note Use this ONLY for code that needs the old Factory.hpp interface
 * @note For new code, use Factory or SimpleFactory aliases
 */
template <typename K, typename T, bool ThreadSafe = false,
          typename FallbackPolicy = DefaultFallbackPolicy<T>,
          typename... Params>
using LegacyVariadicFactory = SimpleVariadicFactory<K, T, ThreadSafe, FallbackPolicy, Params...>;

// ============================================================================
// PART 2: Modern Policy-Based Factory (v3.0 style) - DEFAULT "Factory"
// ============================================================================

// Storage Policies
template<typename K, typename V>
struct MapStoragePolicy {
    using StorageType = std::map<K, V>;
    static constexpr bool is_ordered = true;
};

template<typename K, typename V>
struct UnorderedMapStoragePolicy {
    using StorageType = std::unordered_map<K, V>;
    static constexpr bool is_ordered = false;
};

// Registration Policies
struct AllowOverwritePolicy {
    template<typename Registry, typename K, typename V>
    static bool insert(Registry& reg, const K& key, V&& value, FactoryStats& stats) {
        bool existed = reg.count(key) > 0;
        reg[key] = std::forward<V>(value);
        if (!existed) ++stats.registrations;
        return !existed;
    }
};

struct PreventOverwritePolicy {
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

// Error Handling Policies
template<typename T, typename K>
struct ExpectedErrorPolicy {
    using ErrorType = FactoryErrorInfo<K>;
    using ReturnType = Expected<T, ErrorType>;
    
    static ReturnType handle_not_found(const K& key) {
        return unexpected{ErrorType{FactoryError::KeyNotFound, "No creator registered", &key}};
    }
    
    static ReturnType handle_creation_failed(const K& key, const std::string& reason) {
        return unexpected{ErrorType{FactoryError::CreationFailed, reason, &key}};
    }
};

template<typename T, typename K>
struct ThrowingErrorPolicy {
    using ReturnType = T;
    
    [[noreturn]] static T handle_not_found(const K&) {
        throw std::runtime_error("Factory key not found");
    }
    
    [[noreturn]] static T handle_creation_failed(const K&, const std::string& reason) {
        throw std::runtime_error("Factory creation failed: " + reason);
    }
};

template<typename T, typename K>
struct DefaultErrorPolicy {
    using ReturnType = T;
    static T handle_not_found(const K&) { return T{}; }
    static T handle_creation_failed(const K&, const std::string&) { return T{}; }
};

// Lifetime Policies
struct InstanceLifetimePolicy {
    static constexpr bool is_singleton = false;
};

struct SingletonLifetimePolicy {
    static constexpr bool is_singleton = true;
};

/**
 * @brief Policy-based Factory with variadic parameters (v3.0+)
 * 
 * @tparam K Key type
 * @tparam T Product type
 * @tparam ConcurrencyPolicy Thread-safety policy
 * @tparam ErrorHandlingPolicy Error handling strategy
 * @tparam RegistrationPolicy Overwrite behavior
 * @tparam StoragePolicy Container type
 * @tparam LifetimePolicy Instance vs Singleton
 * @tparam Params Variadic parameter types
 * 
 * @note This is the main Factory for v3.0+ code with full policy support
 * @note Provides full policy customization and Expected<T,E> support
 * @note For EqualityAny.h, use LegacyVariadicFactory instead
 */
template <
    typename K,
    typename T,
    typename ConcurrencyPolicy = SingleThreadedPolicy,
    typename ErrorHandlingPolicy = ExpectedErrorPolicy<T, K>,
    typename RegistrationPolicy = PreventOverwritePolicy,
    typename StoragePolicy = MapStoragePolicy<K, std::function<T()>>,
    typename LifetimePolicy = InstanceLifetimePolicy,
    typename... Params
>
class Factory : private ConcurrencyPolicy {
public:
    using KeyType = K;
    using ProductType = T;
    using CreatorFunction = std::function<T(Params...)>;
    using ErrorType = typename ErrorHandlingPolicy::ErrorType;
    using ReturnType = typename ErrorHandlingPolicy::ReturnType;
    using StorageType = typename StoragePolicy::StorageType;
    
private:
    StorageType registry_;
    mutable FactoryStats stats_;
    
public:
    Factory() = default;
    
    template<typename L = LifetimePolicy>
    static std::enable_if_t<L::is_singleton, Factory&> instance() {
        static Factory inst;
        return inst;
    }
    
    template<typename Callable>
    [[nodiscard]] bool registerType(const K& key, Callable&& creator) {
        typename ConcurrencyPolicy::LockGuard lock(this->getLock());
        if constexpr (std::is_pointer_v<K>) {
            debug_enforce(key != nullptr, "Factory: null key");
        }
        return RegistrationPolicy::insert(
            registry_, key, CreatorFunction(std::forward<Callable>(creator)), stats_);
    }
    
    /**
     * @brief Register multiple types at once
     */
    size_t registerTypes(std::initializer_list<std::pair<K, CreatorFunction>> registrations) {
        typename ConcurrencyPolicy::LockGuard lock(this->getLock());
        
        size_t success_count = 0;
        for (const auto& [key, creator] : registrations) {
            if (RegistrationPolicy::insert(registry_, key, creator, stats_)) {
                ++success_count;
            }
        }
        return success_count;
    }
    
    /**
     * @brief Unregister a type
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
    
    [[nodiscard]] ReturnType make(const K& key, Params... params) const {
        auto* self = const_cast<Factory*>(this);
        if constexpr (is_shared_policy<ConcurrencyPolicy>::value) {
            typename ConcurrencyPolicy::SharedGuard lock(self->getLock());
            return makeImpl(key, std::forward<Params>(params)...);
        } else {
            typename ConcurrencyPolicy::LockGuard lock(self->getLock());
            return makeImpl(key, std::forward<Params>(params)...);
        }
    }
    
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
    
    [[nodiscard]] bool empty() const noexcept { return size() == 0; }
    
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
     * @brief Reset statistics
     */
    void resetStats() noexcept {
        typename ConcurrencyPolicy::LockGuard lock(this->getLock());
        stats_.reset();
    }
    
    void clear() noexcept {
        typename ConcurrencyPolicy::LockGuard lock(this->getLock());
        registry_.clear();
        stats_.reset();
    }
    
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
    
private:
    ReturnType makeImpl(const K& key, Params... params) const {
        ++stats_.lookups;
        auto it = registry_.find(key);
        
        if (it == registry_.end()) {
            ++stats_.resolution_failures;
            return ErrorHandlingPolicy::handle_not_found(key);
        }
        
        try {
            ++stats_.resolutions;
            return it->second(std::forward<Params>(params)...);
        } catch (const std::exception& e) {
            ++stats_.resolution_failures;
            --stats_.resolutions;
            if constexpr (std::is_same_v<ReturnType, T>) {
                throw;
            } else {
                return ErrorHandlingPolicy::handle_creation_failed(key, e.what());
            }
        }
    }
};

// ============================================================================
// Type Aliases (for code not needing parameters)
// ============================================================================

/**
 * @brief Simple factory with Expected return (no parameters)
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
 * @brief Thread-safe factory with mutex (no parameters)
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
 * @brief Fast factory with unordered_map (no parameters)
 */
template<typename K, typename T>
using FastFactory = Factory<K, T,
    SingleThreadedPolicy,
    ExpectedErrorPolicy<T, K>,
    PreventOverwritePolicy,
    UnorderedMapStoragePolicy<K, std::function<T()>>,
    InstanceLifetimePolicy
>;

} // namespace fat_p
