/**
 * @file Factory.h
 * @brief Policy-based factory with compile-time customization.
 *
 * @layer Domain
 *
 * Provides two implementations:
 * 1. Factory - Modern policy-based factory with full customization
 * 2. SimpleVariadicFactory - Legacy-compatible lightweight factory
 *
 * @note Use SimpleVariadicFactory for EqualityAny.h compatibility
 * @note Use Factory for new code with full policy customization
 * @note Creators are invoked outside locks to allow reentrancy (e.g., nested factory calls)
 */

#pragma once

/*
FATP_META:
  meta_version: 1
  component: Factory
  file_role: public_header
  path: fat_p/Factory.h
  namespace: fat_p
  layer: Domain
  summary: "Public header for Factory."
  api_stability: in_work
  related:
    docs_search: "Factory"
    tests:
      - tests/test_Factory.cpp
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
#include "ConcurrencyPolicies.h"
#include "enforce.h"
#include "Expected.h"
#include "Stringify.h"
#include "TypeTraits.h"
#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fat_p
{

// ============================================================================
// Error Types (shared by both implementations)
// ============================================================================

enum class FactoryError
{
    KeyNotFound,
    KeyAlreadyExists,
    CreationFailed,
    InvalidKey,
    InvalidCreator,
    RegistryFull
};

inline std::string toString(FactoryError error)
{
    switch (error)
    {
        case FactoryError::KeyNotFound:
            return "Key not found";
        case FactoryError::KeyAlreadyExists:
            return "Key already exists";
        case FactoryError::CreationFailed:
            return "Creation failed";
        case FactoryError::InvalidKey:
            return "Invalid key";
        case FactoryError::InvalidCreator:
            return "Invalid creator";
        case FactoryError::RegistryFull:
            return "Registry full";
        default:
            return "Unknown error";
    }
}

inline std::ostream& operator<<(std::ostream& os, FactoryError error)
{
    return os << toString(error);
}

/**
 * @brief Error information for factory operations
 *
 * @tparam K Key type
 *
 * @note Uses fat_p::toString() for key stringification, supporting string_view,
 *       custom types with toString() methods, containers, and more.
 */
template <typename K>
struct FactoryErrorInfo
{
    FactoryError code;
    std::string message;
    std::optional<K> key;

    FactoryErrorInfo(FactoryError c, const std::string& msg, const K* k = nullptr)
        : code(c)
        , message(msg)
        , key(k ? std::make_optional(*k) : std::nullopt)
    {
    }

    std::string full_message() const
    {
        std::string result = fat_p::toString(code) + ": " + message;
        if (key)
        {
            result += " (key: " + fat_p::toString(*key) + ")";
        }
        return result;
    }
};

// ============================================================================
// Statistics Types and Policies
// ============================================================================

/**
 * @brief Atomic statistics with thread-safe increments
 *
 * @note Non-copyable and non-movable: atomics represent shared mutable state;
 *       copying would create incoherent statistical snapshots. Moving would
 *       transfer ownership of counters without transferring the associated registry.
 */
struct FactoryStats
{
    std::atomic<size_t> registrations{0};
    std::atomic<size_t> registration_failures{0};
    std::atomic<size_t> resolutions{0};
    std::atomic<size_t> resolution_failures{0};
    std::atomic<size_t> unregistrations{0};
    std::atomic<size_t> lookups{0};

    FactoryStats() = default;

    // Non-copyable: atomics represent shared mutable state
    FactoryStats(const FactoryStats&) = delete;
    FactoryStats& operator=(const FactoryStats&) = delete;

    // Non-movable: stats are logically bound to a specific factory instance
    FactoryStats(FactoryStats&&) = delete;
    FactoryStats& operator=(FactoryStats&&) = delete;

    void reset() noexcept
    {
        registrations.store(0, std::memory_order_relaxed);
        registration_failures.store(0, std::memory_order_relaxed);
        resolutions.store(0, std::memory_order_relaxed);
        resolution_failures.store(0, std::memory_order_relaxed);
        unregistrations.store(0, std::memory_order_relaxed);
        lookups.store(0, std::memory_order_relaxed);
    }

    struct Snapshot
    {
        size_t registrations;
        size_t registration_failures;
        size_t resolutions;
        size_t resolution_failures;
        size_t unregistrations;
        size_t lookups;
    };

    Snapshot snapshot() const noexcept
    {
        return Snapshot{registrations.load(std::memory_order_relaxed),
                        registration_failures.load(std::memory_order_relaxed),
                        resolutions.load(std::memory_order_relaxed),
                        resolution_failures.load(std::memory_order_relaxed),
                        unregistrations.load(std::memory_order_relaxed),
                        lookups.load(std::memory_order_relaxed)};
    }

    // Increment methods using relaxed memory ordering for performance
    void increment_registrations() noexcept
    {
        registrations.fetch_add(1, std::memory_order_relaxed);
    }
    void increment_registration_failures() noexcept
    {
        registration_failures.fetch_add(1, std::memory_order_relaxed);
    }
    void increment_resolutions() noexcept
    {
        resolutions.fetch_add(1, std::memory_order_relaxed);
    }
    void increment_resolution_failures() noexcept
    {
        resolution_failures.fetch_add(1, std::memory_order_relaxed);
    }
    void increment_unregistrations() noexcept
    {
        unregistrations.fetch_add(1, std::memory_order_relaxed);
    }
    void increment_lookups() noexcept
    {
        lookups.fetch_add(1, std::memory_order_relaxed);
    }
};

/**
 * @brief Zero-overhead statistics policy for HPC scenarios
 *
 * All operations are no-ops, allowing the compiler to optimize them away entirely.
 */
struct NoStatisticsPolicy
{
    struct Stats
    {
        struct Snapshot
        {
            size_t registrations = 0;
            size_t registration_failures = 0;
            size_t resolutions = 0;
            size_t resolution_failures = 0;
            size_t unregistrations = 0;
            size_t lookups = 0;
        };

        void increment_registrations() noexcept
        {
        }
        void increment_registration_failures() noexcept
        {
        }
        void increment_resolutions() noexcept
        {
        }
        void increment_resolution_failures() noexcept
        {
        }
        void increment_unregistrations() noexcept
        {
        }
        void increment_lookups() noexcept
        {
        }
        void reset() noexcept
        {
        }
        Snapshot snapshot() const noexcept
        {
            return {};
        }
    };
};

/**
 * @brief Standard atomic statistics policy
 */
struct AtomicStatisticsPolicy
{
    using Stats = FactoryStats;
};

// ============================================================================
// PART 1: Simple Variadic Factory (for EqualityAny.h compatibility)
// ============================================================================

/**
 * @brief Default fallback policy: returns default-constructed object
 */
template <typename T>
struct DefaultFallbackPolicy
{
    static T get()
    {
        return T{};
    }
};

/**
 * @brief Throwing fallback policy: throws on key not found
 */
template <typename T>
struct ThrowingFallbackPolicy
{
    static T get()
    {
        throw std::runtime_error("Factory key not found.");
    }
};

/**
 * @brief Null mutex for non-thread-safe mode
 *
 * All operations are no-ops with noexcept for optimal codegen.
 */
struct NullMutex
{
    void lock() const noexcept
    {
    }
    void unlock() const noexcept
    {
    }
    bool try_lock() const noexcept
    {
        return true;
    }
    void lock_shared() const noexcept
    {
    }
    void unlock_shared() const noexcept
    {
    }
    bool try_lock_shared() const noexcept
    {
        return true;
    }
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
 * @note Creators are invoked outside locks to allow reentrancy
 */
template <typename K,
          typename T,
          bool ThreadSafe = false,
          typename FallbackPolicy = DefaultFallbackPolicy<T>,
          typename... Params>
class SimpleVariadicFactory
{
public:
    using CreatorFunction = std::function<T(Params...)>;

private:
    using Registry = std::map<K, CreatorFunction, std::less<>>;
    using MutexType = std::conditional_t<ThreadSafe, std::shared_mutex, NullMutex>;

    Registry mRegistry;
    mutable MutexType mMutex;

    // Private constructor enforces singleton pattern.
    // Use SimpleVariadicFactory::instance() to access.
    SimpleVariadicFactory() = default;

public:
    /**
     * @brief Returns the singleton instance
     */
    static SimpleVariadicFactory& instance()
    {
        static SimpleVariadicFactory inst;
        return inst;
    }

    /**
     * @brief Registers a creator function for the given key
     */
    template <typename Callable>
    [[nodiscard]] bool registerType(const K& key, Callable&& creator)
    {
        std::unique_lock<MutexType> lock(mMutex);
        return mRegistry.try_emplace(key, std::forward<Callable>(creator)).second;
    }

    /**
     * @brief Creates an object using the registered creator
     *
     * @note Creator is invoked outside the lock to allow re-entrant factory access.
     */
    T create(const K& key, Params... params) const
    {
        CreatorFunction creator;

        // Phase 1: Lookup under lock
        {
            std::shared_lock<MutexType> lock(mMutex);
            auto it = mRegistry.find(key);
            if (it == mRegistry.end())
            {
                return FallbackPolicy::get();
            }
            creator = it->second; // Copy the std::function
        }
        // Lock released here

        // Phase 2: Execute outside lock (allows re-entrant factory access)
        return creator(std::forward<Params>(params)...);
    }

    [[nodiscard]] bool hasType(const K& key) const
    {
        std::shared_lock<MutexType> lock(mMutex);
        return mRegistry.count(key) > 0;
    }

    [[nodiscard]] size_t size() const
    {
        std::shared_lock<MutexType> lock(mMutex);
        return mRegistry.size();
    }

    [[nodiscard]] bool empty() const
    {
        return size() == 0;
    }

    void clear()
    {
        std::unique_lock<MutexType> lock(mMutex);
        mRegistry.clear();
    }
};

/**
 * @brief Old-style Factory for EqualityAny.h compatibility
 */
template <typename K,
          typename T,
          bool ThreadSafe = false,
          typename FallbackPolicy = DefaultFallbackPolicy<T>,
          typename... Params>
using LegacyVariadicFactory = SimpleVariadicFactory<K, T, ThreadSafe, FallbackPolicy, Params...>;

// ============================================================================
// PART 2: Modern Policy-Based Factory
// ============================================================================

// Storage Policies with Transparent Comparators

/**
 * @brief Transparent hash for heterogeneous string key lookup
 *
 * Enables lookup with std::string_view or const char* without allocating
 * a temporary std::string. Used by UnorderedMapStoragePolicy when K=std::string.
 *
 * @note The is_transparent tag enables std::unordered_map::find() overloads
 *       that accept any type convertible to string_view.
 * @note Current Factory::make() takes const K&, so caller-side conversion
 *       still occurs. The benefit is in internal container operations.
 *       Future enhancement: template make() on KeyArg for full benefit.
 */
struct TransparentStringHash
{
    using is_transparent = void;

    size_t operator()(std::string_view sv) const noexcept
    {
        return std::hash<std::string_view>{}(sv);
    }

    size_t operator()(const std::string& s) const noexcept
    {
        return std::hash<std::string>{}(s);
    }

    size_t operator()(const char* s) const noexcept
    {
        return std::hash<std::string_view>{}(std::string_view(s));
    }
};

struct TransparentEqual
{
    using is_transparent = void;

    template <typename T, typename U>
    bool operator()(const T& lhs, const U& rhs) const
    {
        return lhs == rhs;
    }
};

template <typename K, typename V>
struct MapStoragePolicy
{
    using StorageType = std::map<K, V, std::less<>>;
    static constexpr bool is_ordered = true;
};

template <typename K, typename V>
struct UnorderedMapStoragePolicy
{
private:
    static constexpr bool is_string_key = std::is_same_v<K, std::string>;

    using HashType = std::conditional_t<is_string_key, TransparentStringHash, std::hash<K>>;
    using EqualType = std::conditional_t<is_string_key, TransparentEqual, std::equal_to<K>>;

public:
    using StorageType = std::unordered_map<K, V, HashType, EqualType>;
    static constexpr bool is_ordered = false;
};

// Registration Policies

struct AllowOverwritePolicy
{
    template <typename Registry, typename K, typename V, typename Stats>
    static bool insert(Registry& reg, const K& key, V&& value, Stats& stats)
    {
        auto [it, inserted] = reg.insert_or_assign(key, std::forward<V>(value));
        if (inserted)
        {
            stats.increment_registrations();
        }
        return inserted;
    }
};

struct PreventOverwritePolicy
{
    template <typename Registry, typename K, typename V, typename Stats>
    static bool insert(Registry& reg, const K& key, V&& value, Stats& stats)
    {
        auto [it, inserted] = reg.try_emplace(key, std::forward<V>(value));
        if (inserted)
        {
            stats.increment_registrations();
        }
        else
        {
            stats.increment_registration_failures();
        }
        return inserted;
    }
};

// Error Handling Policies

namespace factory
{

template <typename T, typename K>
struct ExpectedErrorPolicy
{
    using ErrorType = FactoryErrorInfo<K>;
    using ReturnType = Expected<T, ErrorType>;

    static ReturnType handle_not_found(const K& key)
    {
        return unexpected{ErrorType{FactoryError::KeyNotFound, "No creator registered", &key}};
    }

    static ReturnType handle_creation_failed(const K& key, const std::string& reason)
    {
        return unexpected{ErrorType{FactoryError::CreationFailed, reason, &key}};
    }
};

template <typename T, typename K>
struct ThrowingErrorPolicy
{
    using ErrorType = FactoryError;
    using ReturnType = T;

    [[noreturn]] static T handle_not_found(const K&)
    {
        throw std::runtime_error("Factory key not found");
    }

    [[noreturn]] static T handle_creation_failed(const K&, const std::string& reason)
    {
        throw std::runtime_error("Factory creation failed: " + reason);
    }
};

template <typename T, typename K>
struct DefaultErrorPolicy
{
    using ErrorType = FactoryError;
    using ReturnType = T;

    static T handle_not_found(const K&)
    {
        return T{};
    }
    static T handle_creation_failed(const K&, const std::string&)
    {
        return T{};
    }
};

} // namespace factory

// Convenience alias for verbose module namespace.
namespace fac = factory;

// Lifetime Policies
struct InstanceLifetimePolicy
{
    static constexpr bool is_singleton = false;
};

struct SingletonLifetimePolicy
{
    static constexpr bool is_singleton = true;
};

/**
 * @brief Policy-based Factory with variadic parameters
 *
 * @note Creators are invoked outside locks to allow re-entrant factory access.
 * @note Statistics are updated using relaxed memory ordering for performance.
 */
template <typename K,
          typename T,
          typename ConcurrencyPolicy = SingleThreadedPolicy,
          typename ErrorHandlingPolicy = factory::ExpectedErrorPolicy<T, K>,
          typename RegistrationPolicy = PreventOverwritePolicy,
          typename StoragePolicy = MapStoragePolicy<K, std::function<T()>>,
          typename LifetimePolicy = InstanceLifetimePolicy,
          typename StatisticsPolicy = AtomicStatisticsPolicy,
          typename... Params>
class Factory : private ConcurrencyPolicy
{
public:
    using KeyType = K;
    using ProductType = T;
    using CreatorFunction = std::function<T(Params...)>;
    using ErrorType = typename ErrorHandlingPolicy::ErrorType;
    using ReturnType = typename ErrorHandlingPolicy::ReturnType;
    using StorageType = typename StoragePolicy::StorageType;
    using StatsType = typename StatisticsPolicy::Stats;

private:
    StorageType mRegistry;
    mutable StatsType mStats;

    auto& getLockForConst() const
    {
        return const_cast<Factory*>(this)->ConcurrencyPolicy::getLock();
    }

public:
    Factory() = default;

    template <typename L = LifetimePolicy>
    static std::enable_if_t<L::is_singleton, Factory&> instance()
    {
        static Factory inst;
        return inst;
    }

    template <typename Callable>
    [[nodiscard]] bool registerType(const K& key, Callable&& creator)
    {
        typename ConcurrencyPolicy::LockGuard lock(this->getLock());
        if constexpr (std::is_pointer_v<K>)
        {
            FATP_DEBUG_ENFORCE(key != nullptr, "Factory: null key");
        }
        return RegistrationPolicy::insert(mRegistry, key, CreatorFunction(std::forward<Callable>(creator)), mStats);
    }

    size_t registerTypes(std::initializer_list<std::pair<K, CreatorFunction>> registrations)
    {
        typename ConcurrencyPolicy::LockGuard lock(this->getLock());

        size_t success_count = 0;
        for (const auto& [key, creator] : registrations)
        {
            if (RegistrationPolicy::insert(mRegistry, key, creator, mStats))
            {
                ++success_count;
            }
        }
        return success_count;
    }

    [[nodiscard]] bool unregisterType(const K& key)
    {
        typename ConcurrencyPolicy::LockGuard lock(this->getLock());
        size_t removed = mRegistry.erase(key);
        if (removed > 0)
        {
            mStats.increment_unregistrations();
            return true;
        }
        return false;
    }

    /**
     * @brief Create an object using the registered creator
     *
     * @note Creator is invoked outside the lock to allow re-entrant factory access.
     */
    [[nodiscard]] ReturnType make(const K& key, Params... params) const
    {
        CreatorFunction creator;

        // Phase 1: Lookup under lock
        {
            if constexpr (is_shared_policy<ConcurrencyPolicy>::value)
            {
                typename ConcurrencyPolicy::SharedGuard lock(getLockForConst());
                mStats.increment_lookups();
                auto it = mRegistry.find(key);
                if (it == mRegistry.end())
                {
                    mStats.increment_resolution_failures();
                    return ErrorHandlingPolicy::handle_not_found(key);
                }
                creator = it->second;
            }
            else
            {
                typename ConcurrencyPolicy::LockGuard lock(getLockForConst());
                mStats.increment_lookups();
                auto it = mRegistry.find(key);
                if (it == mRegistry.end())
                {
                    mStats.increment_resolution_failures();
                    return ErrorHandlingPolicy::handle_not_found(key);
                }
                creator = it->second;
            }
        }
        // Lock released here

        // Phase 2: Execute outside lock
        try
        {
            auto result = creator(std::forward<Params>(params)...);
            mStats.increment_resolutions();
            return result;
        }
        catch (const std::exception& e)
        {
            mStats.increment_resolution_failures();
            if constexpr (std::is_same_v<ReturnType, T>)
            {
                throw;
            }
            else
            {
                return ErrorHandlingPolicy::handle_creation_failed(key, e.what());
            }
        }
    }

    /**
     * @brief Check if a creator is registered for the given key
     *
     * @note This method increments the lookups statistic (mutable stats by design).
     */
    [[nodiscard]] bool hasType(const K& key) const noexcept
    {
        if constexpr (is_shared_policy<ConcurrencyPolicy>::value)
        {
            typename ConcurrencyPolicy::SharedGuard lock(getLockForConst());
            mStats.increment_lookups();
            return mRegistry.count(key) > 0;
        }
        else
        {
            typename ConcurrencyPolicy::LockGuard lock(getLockForConst());
            mStats.increment_lookups();
            return mRegistry.count(key) > 0;
        }
    }

    [[nodiscard]] size_t size() const noexcept
    {
        if constexpr (is_shared_policy<ConcurrencyPolicy>::value)
        {
            typename ConcurrencyPolicy::SharedGuard lock(getLockForConst());
            return mRegistry.size();
        }
        else
        {
            typename ConcurrencyPolicy::LockGuard lock(getLockForConst());
            return mRegistry.size();
        }
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return size() == 0;
    }

    [[nodiscard]] std::vector<K> getRegisteredKeys() const
    {
        std::vector<K> keys;
        if constexpr (is_shared_policy<ConcurrencyPolicy>::value)
        {
            typename ConcurrencyPolicy::SharedGuard lock(getLockForConst());
            keys.reserve(mRegistry.size());
            for (const auto& [key, creator] : mRegistry)
            {
                keys.push_back(key);
            }
        }
        else
        {
            typename ConcurrencyPolicy::LockGuard lock(getLockForConst());
            keys.reserve(mRegistry.size());
            for (const auto& [key, creator] : mRegistry)
            {
                keys.push_back(key);
            }
        }
        return keys;
    }

    void resetStats() noexcept
    {
        typename ConcurrencyPolicy::LockGuard lock(this->getLock());
        mStats.reset();
    }

    void clear() noexcept
    {
        typename ConcurrencyPolicy::LockGuard lock(this->getLock());
        mRegistry.clear();
        mStats.reset();
    }

    [[nodiscard]] typename StatsType::Snapshot getStats() const noexcept
    {
        if constexpr (is_shared_policy<ConcurrencyPolicy>::value)
        {
            typename ConcurrencyPolicy::SharedGuard lock(getLockForConst());
            return mStats.snapshot();
        }
        else
        {
            typename ConcurrencyPolicy::LockGuard lock(getLockForConst());
            return mStats.snapshot();
        }
    }
};

// ============================================================================
// Type Aliases
// ============================================================================

template <typename K, typename T>
using SimpleFactory = Factory<K,
                              T,
                              SingleThreadedPolicy,
                              factory::ExpectedErrorPolicy<T, K>,
                              PreventOverwritePolicy,
                              MapStoragePolicy<K, std::function<T()>>,
                              InstanceLifetimePolicy,
                              AtomicStatisticsPolicy>;

template <typename K, typename T>
using ThreadSafeFactory = Factory<K,
                                  T,
                                  MutexSynchronizationPolicy,
                                  factory::ExpectedErrorPolicy<T, K>,
                                  PreventOverwritePolicy,
                                  MapStoragePolicy<K, std::function<T()>>,
                                  InstanceLifetimePolicy,
                                  AtomicStatisticsPolicy>;

template <typename K, typename T>
using FastFactory = Factory<K,
                            T,
                            SingleThreadedPolicy,
                            factory::ExpectedErrorPolicy<T, K>,
                            PreventOverwritePolicy,
                            UnorderedMapStoragePolicy<K, std::function<T()>>,
                            InstanceLifetimePolicy,
                            AtomicStatisticsPolicy>;

/**
 * @brief Recommended factory for string keys (2x+ faster lookups)
 */
template <typename T>
using StringKeyFactory = FastFactory<std::string, T>;

/**
 * @brief HPC-optimized factory with zero statistics overhead
 */
template <typename K, typename T>
using HPCFactory = Factory<K,
                           T,
                           SingleThreadedPolicy,
                           factory::ThrowingErrorPolicy<T, K>,
                           PreventOverwritePolicy,
                           UnorderedMapStoragePolicy<K, std::function<T()>>,
                           InstanceLifetimePolicy,
                           NoStatisticsPolicy>;

} // namespace fat_p
