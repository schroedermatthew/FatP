#pragma once

/*
FATP_META:
  meta_version: 1
  component: ServiceLocator
  file_role: public_header
  path: include/fat_p/ServiceLocator.h
  namespace: fat_p
  layer: Domain
  summary: "Policy-based service locator with scoped overrides."
  api_stability: in_work
  api_stability_notes: "Core API is stable. 'in_work' reflects: (1) TypeKeyPolicy DSO stability not yet addressed, (2) potential future cache policy extensions for named services."
  related:
    docs:
      - Documentation/ServiceLocator/Overview - ServiceLocator.md
      - Documentation/ServiceLocator/User Manual - ServiceLocator.md
    tests:
      - components/ServiceLocator/tests/test_ServiceLocator.cpp
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

/**
 * @file ServiceLocator.h
 * @brief Policy-based service locator with scoped overrides.
 *
 * A ServiceLocator provides type-safe registration and resolution of services.
 * It supports:
 *   - non-owning instance registration
 *   - owning shared_ptr registration
 *   - factory registration (singleton or transient)
 *   - parent/child layering (scoped overrides)
 *
 * Implementation:
 *   Uses StableHashMap for reference stability (pointers remain valid across
 *   insert/rehash). Two-level storage provides fast path for unnamed services.
 *
 * Complexity:
 *   - register/unregister: Average O(1)
 *   - tryResolve/resolve: Average O(1), ~4.5 ns for unnamed services
 *   - createExpected (factory): Average O(1) + factory cost
 *
 * Thread Safety:
 *   - Defined by ConcurrencyPolicy
 *   - With SharedMutexPolicy: concurrent resolves are permitted; registration is exclusive
 *
 * Thread Safety Matrix (common aliases):
 *   - DefaultServiceLocator: No internal synchronization; callers must externally synchronize access.
 *   - ThreadSafeServiceLocator: concurrent resolves are supported; register/unregister/clear are exclusive.
 *   - HotLoopServiceLocator: DefaultServiceLocator + an optional thread-local MRU(2) cache for unnamed
 *     resolves.
 *   - ThreadSafeHotLoopServiceLocator: ThreadSafeServiceLocator + the MRU(2) cache for unnamed resolves.
 *
 *   Notes:
 *     - The MRU resolve cache is type-only and applies only when resolving with an empty name.
 *     - For cache-enabled thread-safe locators (ThreadSafeHotLoopServiceLocator), cache hits do not
 *       take the shared mutex. Do not call register/unregister/clear concurrently with resolve/tryResolve
 *       on other threads. Treat registration as a startup/shutdown operation or quiesce threads first.
 *     - Pointer/reference results become invalid after unregister/overwrite; use resolveSharedExpected()
 *       or tryResolveShared() when you need lifetime via shared_ptr.
 *     - global() is per-instantiation: use ThreadSafeServiceLocator::global() if you need a globally
 *       accessible locator that supports concurrent resolves.
 *
 *   Logical Constness:
 *     Methods marked `const` (tryResolve, resolveExpected, etc.) do not modify the registry contents
 *     (the set of registered services). However, they may mutate internal coordination state:
 *     - Statistics counters (if enabled)
 *     - Singleton factory creation state (one-time initialization)
 *     - MRU cache entries (if enabled)
 *     This is standard "logical constness" for thread-safe containers.
 *
 *   @note When using SharedMutexPolicy (thread-safe locator), the StatisticsPolicy
 *         MUST provide thread-safe increment operations. Use AtomicServiceLocatorStatisticsPolicy
 *         or implement a custom policy with atomic counters. NoServiceLocatorStatisticsPolicy
 *         is always safe (no-op). If you provide a custom StatisticsPolicy and use SharedMutexPolicy,
 *         your policy must be thread-safe; the kThreadSafe trait is the enforcement mechanism.
 *
 * Singleton Factory Semantics:
 *   Factory execution is guaranteed exactly once per registration, even under
 *   concurrent resolution. If multiple threads attempt to resolve an uninitialized
 *   singleton simultaneously, one thread executes the factory while others wait.
 *
 *   Reentrancy: The factory MAY safely resolve other services from this locator.
 *   If a factory attempts to resolve its own service (circular dependency), the
 *   locator detects this and returns ServiceError::CircularDependency instead
 *   of deadlocking.
 *
 *   Cross-Thread Semantics: To prevent cross-thread deadlocks during singleton
 *   construction, the locator serializes singleton factory execution (per
 *   locator family). At most one singleton factory executes at a time.
 *   Cycles are still detected only within a single call chain and are
 *   reported as ServiceError::CircularDependency.
 *
 *   Design Note: This serialization is implemented via a re-entrant "singleton
 *   factory gate" shared across parent/child scopes. A thread only marks a
 *   singleton as "creating" after it owns the gate, preventing cross-thread
 *   cyclic wait patterns that would otherwise deadlock.
 *
 *   Failure: If the factory throws or returns null, the error propagates to the
 *   caller. The singleton remains uninitialized, and subsequent resolve attempts
 *   will retry the factory. To cache failures, wrap your factory with try/catch
 *   and store a sentinel value.
 *
 * Lifetime:
 *   - registerInstance stores a raw pointer; caller owns the instance lifetime
 *   - registerShared stores a shared_ptr; locator shares ownership
 *   - singleton factory caches the created shared_ptr in the registry
 *   - transient factory creates a new shared_ptr per createExpected() call
 *
 * Limitations:
 *   - Factories must be CopyConstructible (stored in std::function)
 *   - Default type-key policy uses address identity; not stable across DSO/plugin boundaries
 *   - Returning references (not shared_ptr) means callers must not hold references
 *     across unregister/overwrite operations
 */

#include "ConcurrencyPolicies.h"
#include "enforce.h"
#include "Expected.h"
#include "StableHashMap.h"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <ostream>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>

namespace fat_p
{

enum class ServiceLifetime
{
    Singleton,
    Transient
};

enum class ServiceError
{
    ServiceNotFound,
    ServiceAlreadyExists,
    InvalidLifetime,
    NullSharedInstance,
    FactoryReturnedNull,
    FactoryThrew,
    FactoryNotRegistered,
    CircularDependency,
    TransientRequiresCreate
};

inline std::string toString(ServiceError error)
{
    switch (error)
    {
        case ServiceError::ServiceNotFound:
            return "Service not found";
        case ServiceError::ServiceAlreadyExists:
            return "Service already exists";
        case ServiceError::InvalidLifetime:
            return "Invalid lifetime";
        case ServiceError::NullSharedInstance:
            return "Shared instance is null";
        case ServiceError::FactoryReturnedNull:
            return "Factory returned null";
        case ServiceError::FactoryThrew:
            return "Factory threw";
        case ServiceError::FactoryNotRegistered:
            return "Factory not registered";
        case ServiceError::CircularDependency:
            return "Circular dependency detected";
        case ServiceError::TransientRequiresCreate:
            return "Transient requires create";
        default:
            return "Unknown error";
    }
}

inline std::ostream& operator<<(std::ostream& os, ServiceError error)
{
    return os << toString(error);
}

struct ServiceErrorInfo
{
    ServiceError mCode{};
    std::string mMessage{};
    std::string mName{};

    ServiceErrorInfo() = default;

    ServiceErrorInfo(ServiceError code, std::string message, std::string name)
        : mCode(code)
        , mMessage(std::move(message))
        , mName(std::move(name))
    {
    }

    [[nodiscard]] std::string fullMessage() const
    {
        if (mName.empty())
        {
            return fat_p::toString(mCode) + ": " + mMessage;
        }
        return fat_p::toString(mCode) + ": " + mMessage + " (name: " + mName + ")";
    }
};

inline std::ostream& operator<<(std::ostream& os, const ServiceErrorInfo& info)
{
    return os << info.fullMessage();
}

struct NoServiceLocatorStatisticsPolicy
{
    static constexpr bool kThreadSafe = true;

    struct Stats
    {
        struct Snapshot
        {
            size_t mRegistrations = 0;
            size_t mRegistrationFailures = 0;
            size_t mResolutions = 0;
            size_t mResolutionFailures = 0;
            size_t mCreations = 0;
            size_t mCreationFailures = 0;
            size_t mUnregistrations = 0;
        };

        void incrementRegistrations() noexcept {}
        void incrementRegistrationFailures() noexcept {}
        void incrementResolutions() noexcept {}
        void incrementResolutionFailures() noexcept {}
        void incrementCreations() noexcept {}
        void incrementCreationFailures() noexcept {}
        void incrementUnregistrations() noexcept {}
        void incrementUnregistrations(size_t) noexcept {}
        void reset() noexcept {}
        [[nodiscard]] Snapshot snapshot() const noexcept { return {}; }
    };
};

struct AtomicServiceLocatorStatisticsPolicy
{
    static constexpr bool kThreadSafe = true;

    struct Stats
    {
        std::atomic<size_t> mRegistrations{0};
        std::atomic<size_t> mRegistrationFailures{0};
        std::atomic<size_t> mResolutions{0};
        std::atomic<size_t> mResolutionFailures{0};
        std::atomic<size_t> mCreations{0};
        std::atomic<size_t> mCreationFailures{0};
        std::atomic<size_t> mUnregistrations{0};

        Stats() = default;
        Stats(const Stats&) = delete;
        Stats& operator=(const Stats&) = delete;
        Stats(Stats&&) = delete;
        Stats& operator=(Stats&&) = delete;

        void reset() noexcept
        {
            mRegistrations.store(0, std::memory_order_relaxed);
            mRegistrationFailures.store(0, std::memory_order_relaxed);
            mResolutions.store(0, std::memory_order_relaxed);
            mResolutionFailures.store(0, std::memory_order_relaxed);
            mCreations.store(0, std::memory_order_relaxed);
            mCreationFailures.store(0, std::memory_order_relaxed);
            mUnregistrations.store(0, std::memory_order_relaxed);
        }

        struct Snapshot
        {
            size_t mRegistrations;
            size_t mRegistrationFailures;
            size_t mResolutions;
            size_t mResolutionFailures;
            size_t mCreations;
            size_t mCreationFailures;
            size_t mUnregistrations;
        };

        [[nodiscard]] Snapshot snapshot() const noexcept
        {
            return Snapshot{
                mRegistrations.load(std::memory_order_relaxed),
                mRegistrationFailures.load(std::memory_order_relaxed),
                mResolutions.load(std::memory_order_relaxed),
                mResolutionFailures.load(std::memory_order_relaxed),
                mCreations.load(std::memory_order_relaxed),
                mCreationFailures.load(std::memory_order_relaxed),
                mUnregistrations.load(std::memory_order_relaxed)
            };
        }

        void incrementRegistrations() noexcept { mRegistrations.fetch_add(1, std::memory_order_relaxed); }
        void incrementRegistrationFailures() noexcept { mRegistrationFailures.fetch_add(1, std::memory_order_relaxed); }
        void incrementResolutions() noexcept { mResolutions.fetch_add(1, std::memory_order_relaxed); }
        void incrementResolutionFailures() noexcept { mResolutionFailures.fetch_add(1, std::memory_order_relaxed); }
        void incrementCreations() noexcept { mCreations.fetch_add(1, std::memory_order_relaxed); }
        void incrementCreationFailures() noexcept { mCreationFailures.fetch_add(1, std::memory_order_relaxed); }
        void incrementUnregistrations() noexcept { mUnregistrations.fetch_add(1, std::memory_order_relaxed); }
        void incrementUnregistrations(size_t count) noexcept
        {
            mUnregistrations.fetch_add(count, std::memory_order_relaxed);
        }
    };
};

struct ServicePreventOverwritePolicy
{
    template <typename Registry, typename Key, typename Value, typename Stats>
    static bool insert(Registry& registry, const Key& key, Value&& value, Stats& stats)
    {
        auto [ptr, inserted] = registry.insert(key, std::forward<Value>(value));
        if (inserted)
        {
            stats.incrementRegistrations();
            return true;
        }
        stats.incrementRegistrationFailures();
        return false;
    }
};

struct ServiceAllowOverwritePolicy
{
    template <typename Registry, typename Key, typename Value, typename Stats>
    static bool insert(Registry& registry, const Key& key, Value&& value, Stats& stats)
    {
        registry.insert_or_assign(key, std::forward<Value>(value));
        stats.incrementRegistrations();
        return true;
    }
};

// ============================================================================
// Resolve Cache Policies
// ============================================================================

// Default: no caching.
struct NoServiceLocatorResolveCachePolicy
{
    static constexpr bool kEnabled = false;

    template <typename T, typename Locator>
    static T* tryGet(const Locator&, const void*, std::uint64_t) noexcept
    {
        return nullptr;
    }

    template <typename T, typename Locator>
    static void put(const Locator&, const void*, std::uint64_t, T*) noexcept
    {
        // no-op
    }
};

// Thread-local MRU cache.
//
// Notes:
//  - This is intentionally tiny: 1-entry catches "same service repeatedly".
//    2-entry catches common alternation patterns (e.g., read+write services).
//  - Cache entries are invalidated by a monotonically increasing registry epoch.
//    The epoch is bumped on successful register/unregister/clear operations.
template <std::size_t Slots>
struct ThreadLocalMruServiceLocatorResolveCachePolicy
{
    static_assert(Slots == 1 || Slots == 2, "Slots must be 1 or 2");
    static constexpr bool kEnabled = true;

    struct Entry
    {
        const void* owner = nullptr;
        const void* typeId = nullptr;
        const void* service = nullptr;
        std::uint64_t epoch = 0;
    };

    struct Cache
    {
        Entry e0{};
        Entry e1{};
        std::uint8_t mruIndex = 0; // 0 => e0 is MRU, 1 => e1 is MRU (Slots==2)
    };

#if defined(__cpp_constinit) && (__cpp_constinit >= 201907L)
    static inline thread_local constinit Cache tlsCache{};
#else
    static inline thread_local Cache tlsCache{};
#endif

    static Cache& cache() noexcept { return tlsCache; }

    template <typename T, typename Locator>
    static T* tryGet(const Locator& locator, const void* typeId, std::uint64_t epoch) noexcept
    {
        Cache& c = cache();
        const void* owner = &locator;

        if constexpr (Slots == 1)
        {
            if (c.e0.owner == owner && c.e0.typeId == typeId && c.e0.epoch == epoch)
            {
                return const_cast<T*>(static_cast<const T*>(c.e0.service));
            }
            return nullptr;
        }
        else
        {
            const std::uint8_t mru = static_cast<std::uint8_t>(c.mruIndex & 1U);
            Entry& first = (mru == 0) ? c.e0 : c.e1;
            Entry& second = (mru == 0) ? c.e1 : c.e0;

            if (first.owner == owner && first.typeId == typeId && first.epoch == epoch)
            {
                return const_cast<T*>(static_cast<const T*>(first.service));
            }

            if (second.owner == owner && second.typeId == typeId && second.epoch == epoch)
            {
                // ABAB-friendly: flip MRU index without swapping full entries.
                c.mruIndex = static_cast<std::uint8_t>(1U - mru);
                return const_cast<T*>(static_cast<const T*>(second.service));
            }

            return nullptr;
        }
    }

    template <typename T, typename Locator>
    static void put(const Locator& locator, const void* typeId, std::uint64_t epoch, T* service) noexcept
    {
        if (service == nullptr)
        {
            return;
        }

        Cache& c = cache();
        const void* owner = &locator;

        if constexpr (Slots == 1)
        {
            c.e0 = Entry{owner, typeId, static_cast<const void*>(service), epoch};
            return;
        }
        else
        {
            // Update if present.
            if (c.e0.owner == owner && c.e0.typeId == typeId)
            {
                c.e0.service = static_cast<const void*>(service);
                c.e0.epoch = epoch;
                c.mruIndex = 0;
                return;
            }

            if (c.e1.owner == owner && c.e1.typeId == typeId)
            {
                c.e1.service = static_cast<const void*>(service);
                c.e1.epoch = epoch;
                c.mruIndex = 1;
                return;
            }

            // Insert into LRU slot (only when we have a real service to store).
            const std::uint8_t mru = static_cast<std::uint8_t>(c.mruIndex & 1U);
            const std::uint8_t lru = static_cast<std::uint8_t>(1U - mru);

            if (lru == 0)
            {
                c.e0 = Entry{owner, typeId, static_cast<const void*>(service), epoch};
            }
            else
            {
                c.e1 = Entry{owner, typeId, static_cast<const void*>(service), epoch};
            }

            c.mruIndex = lru;
        }
    }
};

using OneEntryServiceLocatorResolveCachePolicy = ThreadLocalMruServiceLocatorResolveCachePolicy<1>;
using TwoEntryServiceLocatorResolveCachePolicy = ThreadLocalMruServiceLocatorResolveCachePolicy<2>;

namespace detail
{

template <typename T>
inline constexpr unsigned char kServiceTypeToken = 0;

struct DefaultServiceTypeKeyPolicy
{
    template <typename T>
    static const void* typeId() noexcept
    {
        using U = std::remove_cv_t<std::remove_reference_t<T>>;
        return &kServiceTypeToken<U>;
    }
};

template <typename T>
struct is_shared_ptr : std::false_type {};

template <typename U>
struct is_shared_ptr<std::shared_ptr<U>> : std::true_type {};

template <typename T>
inline constexpr bool is_shared_ptr_v = is_shared_ptr<T>::value;

template <typename T>
struct is_unique_ptr : std::false_type {};

template <typename U, typename D>
struct is_unique_ptr<std::unique_ptr<U, D>> : std::true_type {};

template <typename T>
inline constexpr bool is_unique_ptr_v = is_unique_ptr<T>::value;

template <typename T, typename Factory>
std::shared_ptr<T> invokeFactoryToShared(Factory& factory)
{
    using Ret = std::invoke_result_t<Factory&>;

    if constexpr (is_shared_ptr_v<Ret>)
    {
        return factory();
    }
    else if constexpr (is_unique_ptr_v<Ret>)
    {
        auto up = factory();
        return std::shared_ptr<T>(std::move(up));
    }
    else
    {
        static_assert(std::is_same_v<Ret, std::shared_ptr<T>> || std::is_same_v<Ret, std::unique_ptr<T>>,
                      "Factory must return std::shared_ptr<T> or std::unique_ptr<T>.");
        return {};
    }
}

} // namespace detail

template <typename ConcurrencyPolicy = SingleThreadedPolicy,
          typename RegistrationPolicy = ServicePreventOverwritePolicy,
          typename StatisticsPolicy = NoServiceLocatorStatisticsPolicy,
          typename TypeKeyPolicy = detail::DefaultServiceTypeKeyPolicy,
          typename ResolveCachePolicy = NoServiceLocatorResolveCachePolicy>
class ServiceLocator : private ConcurrencyPolicy
{
private:
    struct SingletonFactoryGate
    {
        class Guard
        {
        public:
            explicit Guard(SingletonFactoryGate& gate)
                : mGate(gate)
            {
                mGate.enter();
            }

            Guard(const Guard&) = delete;
            Guard& operator=(const Guard&) = delete;
            Guard(Guard&&) = delete;
            Guard& operator=(Guard&&) = delete;

            ~Guard()
            {
                mGate.exit();
            }

        private:
            SingletonFactoryGate& mGate;
        };

        void enter()
        {
            std::unique_lock<std::mutex> lock(mMutex);
            const std::thread::id thisThread = std::this_thread::get_id();

            mCv.wait(lock, [this, &thisThread]() { return (mDepth == 0) || (mOwnerThread == thisThread); });

            mOwnerThread = thisThread;
            ++mDepth;
        }

        void exit()
        {
            std::unique_lock<std::mutex> lock(mMutex);

            // Debug: fail-fast via FATP_ENFORCE (terminates on violation).
            // Release: FATP_ENFORCE is no-op; graceful early return provides safety.
            FATP_ENFORCE(mDepth > 0, "SingletonFactoryGate exit without enter");
            FATP_ENFORCE(mOwnerThread == std::this_thread::get_id(),
                         "SingletonFactoryGate exit from non-owner thread");

            if (mDepth == 0)
            {
                return;
            }

            if (--mDepth == 0)
            {
                mOwnerThread = std::thread::id{};
                lock.unlock();
                mCv.notify_all();
            }
        }

    private:
        std::mutex mMutex{};
        std::condition_variable mCv{};
        std::thread::id mOwnerThread{};
        size_t mDepth = 0;
    };

    struct RootState
    {
        // Bumps whenever the registry is mutated (registration / unregistration / clear).
        // Used as a cheap cache invalidation signal for optional resolve caches.
        std::atomic<std::uint64_t> mRegistryEpoch{0};

        SingletonFactoryGate mSingletonFactoryGate{};
    };

public:
    // Enforce that thread-safe concurrency policies require thread-safe statistics policies.
    // Use AtomicServiceLocatorStatisticsPolicy or NoServiceLocatorStatisticsPolicy with
    // SharedMutexPolicy or other thread-safe concurrency policies.
    static_assert(
        std::is_same_v<ConcurrencyPolicy, SingleThreadedPolicy> || StatisticsPolicy::kThreadSafe,
        "Thread-safe ConcurrencyPolicy requires a thread-safe StatisticsPolicy. "
        "Use AtomicServiceLocatorStatisticsPolicy or NoServiceLocatorStatisticsPolicy.");

    using StatsType = typename StatisticsPolicy::Stats;
    using RegisterResult = Expected<void, ServiceErrorInfo>;

    class Scope;
    class Registration;

    ServiceLocator()
        : mRootState(std::make_shared<RootState>())
    {
    }

    explicit ServiceLocator(const ServiceLocator* parent)
        : mParent(parent)
        , mRootState(parent != nullptr ? parent->mRootState : std::make_shared<RootState>())
    {
    }

    ServiceLocator(const ServiceLocator&) = delete;
    ServiceLocator& operator=(const ServiceLocator&) = delete;
    ServiceLocator(ServiceLocator&&) = delete;
    ServiceLocator& operator=(ServiceLocator&&) = delete;

    ~ServiceLocator() = default;

    [[nodiscard]] static ServiceLocator& global()
    {
        static ServiceLocator sLocator{};
        return sLocator;
    }

    [[nodiscard]] Scope makeScope() const&
    {
        return Scope(*this);
    }

    Scope makeScope() const&& = delete;

    [[nodiscard]] ServiceLocator makeChild() const&
    {
        return ServiceLocator(this);
    }

    ServiceLocator makeChild() const&& = delete;

    // ========================================================================
    // Registration
    // ========================================================================

    template <typename T>
    [[nodiscard]] RegisterResult registerInstance(T& instance, std::string_view name = {})
    {
        assertRegistrableServiceType<T>();
        const void* typeId = TypeKeyPolicy::template typeId<T>();

        ServiceEntry entry;
        entry.mKind = ServiceEntryKind::Instance;
        entry.mLifetime = ServiceLifetime::Singleton;
        entry.mInstance = std::addressof(instance);

        auto lock = writeLock();

        if (name.empty())
        {
            // Fast path: unnamed service - use RegistrationPolicy
            if (!RegistrationPolicy::insert(mUnnamedRegistry, typeId, std::move(entry), mStats))
            {
                return unexpected{ServiceErrorInfo{ServiceError::ServiceAlreadyExists,
                                                   "Instance registration rejected",
                                                   std::string(name)}};
            }
        }
        else
        {
            // Named service path
            ServiceKey key{typeId, std::string(name)};
            if (!RegistrationPolicy::insert(mNamedRegistry, key, std::move(entry), mStats))
            {
                return unexpected{ServiceErrorInfo{ServiceError::ServiceAlreadyExists,
                                                   "Instance registration rejected by RegistrationPolicy",
                                                   std::string(name)}};
            }
        }
        bumpRegistryEpoch();
        return {};
    }

    template <typename T>
    [[nodiscard]] RegisterResult registerShared(std::shared_ptr<T> instance, std::string_view name = {})
    {
        assertRegistrableServiceType<T>();

        if (!instance)
        {
            mStats.incrementRegistrationFailures();
            return unexpected{ServiceErrorInfo{ServiceError::NullSharedInstance,
                                               "Shared registration received an empty shared_ptr",
                                               std::string(name)}};
        }

        const void* typeId = TypeKeyPolicy::template typeId<T>();

        ServiceEntry entry;
        entry.mKind = ServiceEntryKind::Shared;
        entry.mLifetime = ServiceLifetime::Singleton;
        entry.mShared = std::move(instance);

        auto lock = writeLock();

        if (name.empty())
        {
            // Fast path: unnamed service - use RegistrationPolicy
            if (!RegistrationPolicy::insert(mUnnamedRegistry, typeId, std::move(entry), mStats))
            {
                return unexpected{ServiceErrorInfo{ServiceError::ServiceAlreadyExists,
                                                   "Shared registration rejected",
                                                   std::string(name)}};
            }
        }
        else
        {
            ServiceKey key{typeId, std::string(name)};
            if (!RegistrationPolicy::insert(mNamedRegistry, key, std::move(entry), mStats))
            {
                return unexpected{ServiceErrorInfo{ServiceError::ServiceAlreadyExists,
                                                   "Shared registration rejected by RegistrationPolicy",
                                                   std::string(name)}};
            }
        }
        bumpRegistryEpoch();
        return {};
    }

    template <typename T, typename Factory>
    [[nodiscard]] RegisterResult registerFactory(Factory factory, ServiceLifetime lifetime, std::string_view name = {})
    {
        assertRegistrableServiceType<T>();

        if (lifetime != ServiceLifetime::Singleton && lifetime != ServiceLifetime::Transient)
        {
            mStats.incrementRegistrationFailures();
            return unexpected{ServiceErrorInfo{ServiceError::InvalidLifetime,
                                               "Only Singleton and Transient lifetimes are valid for factories",
                                               std::string(name)}};
        }

        const void* typeId = TypeKeyPolicy::template typeId<T>();

        ServiceEntry entry;
        entry.mKind = ServiceEntryKind::Factory;
        entry.mLifetime = lifetime;
        entry.mFactory = [factory = std::move(factory)]() mutable -> std::shared_ptr<void> {
            auto sp = detail::invokeFactoryToShared<T>(factory);
            return sp;
        };

        auto lock = writeLock();

        if (name.empty())
        {
            // Fast path: unnamed service - use RegistrationPolicy
            if (!RegistrationPolicy::insert(mUnnamedRegistry, typeId, std::move(entry), mStats))
            {
                return unexpected{ServiceErrorInfo{ServiceError::ServiceAlreadyExists,
                                                   "Factory registration rejected",
                                                   std::string(name)}};
            }
        }
        else
        {
            ServiceKey key{typeId, std::string(name)};
            if (!RegistrationPolicy::insert(mNamedRegistry, key, std::move(entry), mStats))
            {
                return unexpected{ServiceErrorInfo{ServiceError::ServiceAlreadyExists,
                                                   "Factory registration rejected by RegistrationPolicy",
                                                   std::string(name)}};
            }
        }

        bumpRegistryEpoch();
        return {};
    }

    template <typename T>
    [[nodiscard]] bool unregister(std::string_view name = {})
    {
        const void* typeId = TypeKeyPolicy::template typeId<T>();
        auto lock = writeLock();

        bool removed = false;
        if (name.empty())
        {
            removed = mUnnamedRegistry.erase(typeId);
        }
        else
        {
            ServiceKeyView key{typeId, name};
            removed = mNamedRegistry.erase(key);
        }

        if (removed)
        {
            bumpRegistryEpoch();
            mStats.incrementUnregistrations();
        }
        return removed;
    }

    void clear()
    {
        auto lock = writeLock();
        const size_t count = mUnnamedRegistry.size() + mNamedRegistry.size();
        mUnnamedRegistry.clear();
        mNamedRegistry.clear();
        if (count > 0)
        {
            bumpRegistryEpoch();
            mStats.incrementUnregistrations(count);
        }
    }

    [[nodiscard]] size_t size() const
    {
        auto lock = readLock();
        return mUnnamedRegistry.size() + mNamedRegistry.size();
    }

    [[nodiscard]] bool empty() const
    {
        return size() == 0;
    }

    template <typename T>
    [[nodiscard]] bool isRegistered(std::string_view name = {}) const
    {
        const void* typeId = TypeKeyPolicy::template typeId<T>();

        {
            auto lock = readLock();

            if (name.empty())
            {
                if (mUnnamedRegistry.find(typeId) != nullptr)
                {
                    return true;
                }
            }
            else
            {
                ServiceKeyView key{typeId, name};
                if (mNamedRegistry.find(key) != nullptr)
                {
                    return true;
                }
            }
        }
        // Lock released before parent traversal (consistent with tryResolve pattern)
        return mParent != nullptr && mParent->template isRegistered<T>(name);
    }

    [[nodiscard]] StatsType& stats() noexcept
    {
        return mStats;
    }

    [[nodiscard]] const StatsType& stats() const noexcept
    {
        return mStats;
    }

    // ========================================================================
    // Resolution - OPTIMIZED HOT PATH
    // ========================================================================

    template <typename T>
    [[nodiscard]] T* tryResolve(std::string_view name = {}) const
    {
        const void* typeId = TypeKeyPolicy::template typeId<T>();

        // Track if we need singleton factory creation (must happen outside lock)
        bool needsFactoryCreation = false;

        {
            auto lock = readLock();

            // Find entry (two-level lookup)
            ServiceEntry* entry = nullptr;
            const bool cacheable = name.empty();
            std::uint64_t epoch = 0;
            if (cacheable)
            {
                epoch = registryEpoch();

                if constexpr (ResolveCachePolicy::kEnabled)
                {
                    if (auto* cached = ResolveCachePolicy::template tryGet<T>(*this, typeId, epoch))
                    {
                        mStats.incrementResolutions();
                        return cached;
                    }
                }

                entry = mUnnamedRegistry.find(typeId);
            }
            else
            {
                ServiceKeyView key{typeId, name};
                entry = mNamedRegistry.find(key);
            }

            if (entry != nullptr)
            {
                if (entry->mKind == ServiceEntryKind::Instance)
                {
                    auto* resolved = static_cast<T*>(entry->mInstance);
                    if constexpr (ResolveCachePolicy::kEnabled)
                    {
                        if (cacheable)
                        {
                            ResolveCachePolicy::template put<T>(*this, typeId, epoch, resolved);
                        }
                    }

                    mStats.incrementResolutions();
                    return resolved;
                }
                if (entry->mKind == ServiceEntryKind::Shared)
                {
                    if (!entry->mShared)
                    {
                        mStats.incrementResolutionFailures();
                        return nullptr;
                    }
                    auto* resolved = static_cast<T*>(entry->mShared.get());
                    if constexpr (ResolveCachePolicy::kEnabled)
                    {
                        if (cacheable)
                        {
                            ResolveCachePolicy::template put<T>(*this, typeId, epoch, resolved);
                        }
                    }
                    mStats.incrementResolutions();
                    return resolved;
                }
                if (entry->mKind == ServiceEntryKind::Factory)
                {
                    if (entry->mLifetime == ServiceLifetime::Transient)
                    {
                        mStats.incrementResolutionFailures();
                        return nullptr;
                    }

                    // Singleton factory - check if already cached
                    if (entry->mShared)
                    {
                        auto* resolved = static_cast<T*>(entry->mShared.get());
                        if constexpr (ResolveCachePolicy::kEnabled)
                        {
                            if (cacheable)
                            {
                                ResolveCachePolicy::template put<T>(*this, typeId, epoch, resolved);
                            }
                        }
                        mStats.incrementResolutions();
                        return resolved;
                    }

                    // Need factory creation - must release lock first
                    needsFactoryCreation = true;
                }
                else if (!needsFactoryCreation)
                {
                    mStats.incrementResolutionFailures();
                    return nullptr;
                }
            }
        }
        // Lock released here

        // Handle factory creation outside lock
        if (needsFactoryCreation)
        {
            auto cached = resolveOrCreateSingleton<T>(typeId, name);
            if (!cached.has_value())
            {
                mStats.incrementResolutionFailures();
                return nullptr;
            }
            auto* resolved = static_cast<T*>(cached.value().get());
            if constexpr (ResolveCachePolicy::kEnabled)
            {
                if (name.empty())
                {
                    ResolveCachePolicy::template put<T>(*this, typeId, registryEpoch(), resolved);
                }
            }
            mStats.incrementResolutions();
            return resolved;
        }

        // Not found locally - try parent
        if (mParent != nullptr)
        {
            return mParent->template tryResolve<T>(name);
        }

        mStats.incrementResolutionFailures();
        return nullptr;
    }


    template <typename T>
    [[nodiscard]] Expected<std::reference_wrapper<T>, ServiceErrorInfo>
    resolveExpected(std::string_view name = {}) const
    {
        const void* typeId = TypeKeyPolicy::template typeId<T>();

        // Track if we need factory creation (must happen outside lock)
        bool needsFactoryCreation = false;

        {
            auto lock = readLock();

            // Find entry (two-level lookup)
            ServiceEntry* entry = nullptr;
            const bool cacheable = name.empty();
            std::uint64_t epoch = 0;
            if (cacheable)
            {
                epoch = registryEpoch();

                if constexpr (ResolveCachePolicy::kEnabled)
                {
                    if (auto* cached = ResolveCachePolicy::template tryGet<T>(*this, typeId, epoch))
                    {
                        mStats.incrementResolutions();
                        return std::ref(*cached);
                    }
                }

                entry = mUnnamedRegistry.find(typeId);
            }
            else
            {
                ServiceKeyView key{typeId, name};
                entry = mNamedRegistry.find(key);
            }

            if (entry != nullptr)
            {
                // Resolve based on entry kind
                if (entry->mKind == ServiceEntryKind::Instance)
                {
                    auto* resolved = static_cast<T*>(entry->mInstance);
                    if constexpr (ResolveCachePolicy::kEnabled)
                    {
                        if (cacheable)
                        {
                            ResolveCachePolicy::template put<T>(*this, typeId, epoch, resolved);
                        }
                    }
                    mStats.incrementResolutions();
                    return std::ref(*resolved);
                }
                if (entry->mKind == ServiceEntryKind::Shared)
                {
                    if (!entry->mShared)
                    {
                        mStats.incrementResolutionFailures();
                        return unexpected{ServiceErrorInfo{ServiceError::NullSharedInstance,
                                                           "Shared registration holds an empty shared_ptr",
                                                           std::string(name)}};
                    }
                    auto* resolved = static_cast<T*>(entry->mShared.get());
                    if constexpr (ResolveCachePolicy::kEnabled)
                    {
                        if (cacheable)
                        {
                            ResolveCachePolicy::template put<T>(*this, typeId, epoch, resolved);
                        }
                    }
                    mStats.incrementResolutions();
                    return std::ref(*resolved);
                }
                if (entry->mKind == ServiceEntryKind::Factory)
                {
                    if (entry->mLifetime == ServiceLifetime::Transient)
                    {
                        mStats.incrementResolutionFailures();
                        return unexpected{ServiceErrorInfo{ServiceError::TransientRequiresCreate,
                                                           "Transient services require createExpected<T>()",
                                                           std::string(name)}};
                    }

                    // Singleton factory - check if already cached
                    if (entry->mShared)
                    {
                        auto* resolved = static_cast<T*>(entry->mShared.get());
                        if constexpr (ResolveCachePolicy::kEnabled)
                        {
                            if (cacheable)
                            {
                                ResolveCachePolicy::template put<T>(*this, typeId, epoch, resolved);
                            }
                        }
                        mStats.incrementResolutions();
                        return std::ref(*resolved);
                    }

                    // Need factory creation - must release lock first
                    needsFactoryCreation = true;
                }
                else if (!needsFactoryCreation)
                {
                    mStats.incrementResolutionFailures();
                    return unexpected{ServiceErrorInfo{ServiceError::ServiceNotFound,
                                                       "Entry kind not recognized",
                                                       std::string(name)}};
                }
            }
        }
        // Lock released here

        // Handle factory creation outside lock
        if (needsFactoryCreation)
        {
            auto cached = resolveOrCreateSingleton<T>(typeId, name);
            if (!cached.has_value())
            {
                mStats.incrementResolutionFailures();
                return unexpected{cached.error()};
            }
            auto* resolved = static_cast<T*>(cached.value().get());
            if constexpr (ResolveCachePolicy::kEnabled)
            {
                if (name.empty())
                {
                    ResolveCachePolicy::template put<T>(*this, typeId, registryEpoch(), resolved);
                }
            }
            mStats.incrementResolutions();
            return std::ref(*resolved);
        }

        // Not found locally - try parent
        if (mParent != nullptr)
        {
            return mParent->template resolveExpected<T>(name);
        }
        mStats.incrementResolutionFailures();
        return unexpected{
            ServiceErrorInfo{ServiceError::ServiceNotFound, "No matching service registration", std::string(name)}};
    }

    template <typename T>
    [[nodiscard]] T& resolve(std::string_view name = {}) const
    {
        auto expected = resolveExpected<T>(name);
        if (!expected.has_value())
        {
            FATP_ALWAYS_ENFORCE(false, "ServiceLocator::resolve failed: {}", expected.error().fullMessage());
        }
        return expected.value().get();
    }

    template <typename T>
    [[nodiscard]] Expected<std::shared_ptr<T>, ServiceErrorInfo> createExpected(std::string_view name = {}) const
    {
        const void* typeId = TypeKeyPolicy::template typeId<T>();

        // Track what we need to do outside lock
        bool needsSingletonCreation = false;
        bool needsTransientCreation = false;
        std::function<std::shared_ptr<void>()> factoryCopy;

        {
            auto lock = readLock();

            ServiceEntry* entry = nullptr;
            if (name.empty())
            {
                entry = mUnnamedRegistry.find(typeId);
            }
            else
            {
                ServiceKeyView key{typeId, name};
                entry = mNamedRegistry.find(key);
            }

            if (entry != nullptr)
            {
                if (entry->mKind != ServiceEntryKind::Factory)
                {
                    mStats.incrementCreationFailures();
                    return unexpected{ServiceErrorInfo{ServiceError::FactoryNotRegistered,
                                                       "No factory registered for this service",
                                                       std::string(name)}};
                }

                if (entry->mLifetime == ServiceLifetime::Singleton)
                {
                    // Check if already cached
                    if (entry->mShared)
                    {
                        mStats.incrementCreations();
                        return std::static_pointer_cast<T>(entry->mShared);
                    }
                    needsSingletonCreation = true;
                }
                else
                {
                    // Transient - copy factory for use outside lock
                    factoryCopy = entry->mFactory;
                    needsTransientCreation = true;
                }
            }
        }
        // Lock released here

        if (needsSingletonCreation)
        {
            auto cached = resolveOrCreateSingleton<T>(typeId, name);
            if (!cached.has_value())
            {
                mStats.incrementCreationFailures();
                return unexpected{cached.error()};
            }
            mStats.incrementCreations();
            return std::static_pointer_cast<T>(cached.value());
        }

        if (needsTransientCreation)
        {
            // Transient - create new instance outside lock
            try
            {
                auto created = factoryCopy ? factoryCopy() : nullptr;
                if (!created)
                {
                    mStats.incrementCreationFailures();
                    return unexpected{ServiceErrorInfo{ServiceError::FactoryReturnedNull,
                                                       "Factory returned an empty shared_ptr",
                                                       std::string(name)}};
                }
                mStats.incrementCreations();
                return std::static_pointer_cast<T>(created);
            }
            catch (const std::exception& e)
            {
                mStats.incrementCreationFailures();
                return unexpected{ServiceErrorInfo{ServiceError::FactoryThrew,
                                                   std::string("Factory threw: ") + e.what(),
                                                   std::string(name)}};
            }
            catch (...)
            {
                mStats.incrementCreationFailures();
                return unexpected{
                    ServiceErrorInfo{ServiceError::FactoryThrew,
                                               "Factory threw: non-std exception",
                                               std::string(name)}};
            }
        }

        // Not found locally - try parent
        if (mParent != nullptr)
        {
            return mParent->template createExpected<T>(name);
        }
        mStats.incrementCreationFailures();
        return unexpected{
            ServiceErrorInfo{ServiceError::ServiceNotFound, "No matching service registration", std::string(name)}};
    }

    template <typename T>
    [[nodiscard]] std::shared_ptr<T> create(std::string_view name = {}) const
    {
        auto expected = createExpected<T>(name);
        if (!expected.has_value())
        {
            FATP_ALWAYS_ENFORCE(false, "ServiceLocator::create failed: {}", expected.error().fullMessage());
        }
        return expected.value();
    }

    template <typename T>
    [[nodiscard]] Expected<std::shared_ptr<T>, ServiceErrorInfo>
    resolveSharedExpected(std::string_view name = {}) const
    {
        const void* typeId = TypeKeyPolicy::template typeId<T>();

        bool needsFactoryCreation = false;

        {
            auto lock = readLock();

            ServiceEntry* entry = nullptr;
            if (name.empty())
            {
                entry = mUnnamedRegistry.find(typeId);
            }
            else
            {
                ServiceKeyView key{typeId, name};
                entry = mNamedRegistry.find(key);
            }

            if (entry != nullptr)
            {
                if (entry->mKind == ServiceEntryKind::Instance)
                {
                    mStats.incrementResolutionFailures();
                    return unexpected{ServiceErrorInfo{ServiceError::ServiceNotFound,
                                                       "Cannot get shared_ptr for instance-registered service",
                                                       std::string(name)}};
                }

                if (entry->mKind == ServiceEntryKind::Shared)
                {
                    if (!entry->mShared)
                    {
                        mStats.incrementResolutionFailures();
                        return unexpected{ServiceErrorInfo{ServiceError::NullSharedInstance,
                                                           "Shared registration holds an empty shared_ptr",
                                                           std::string(name)}};
                    }
                    mStats.incrementResolutions();
                    return std::static_pointer_cast<T>(entry->mShared);
                }

                if (entry->mKind == ServiceEntryKind::Factory)
                {
                    if (entry->mLifetime == ServiceLifetime::Transient)
                    {
                        mStats.incrementResolutionFailures();
                        return unexpected{ServiceErrorInfo{ServiceError::TransientRequiresCreate,
                                                           "Transient services require createExpected<T>()",
                                                           std::string(name)}};
                    }

                    // Check if already cached
                    if (entry->mShared)
                    {
                        mStats.incrementResolutions();
                        return std::static_pointer_cast<T>(entry->mShared);
                    }

                    needsFactoryCreation = true;
                }
                else if (!needsFactoryCreation)
                {
                    mStats.incrementResolutionFailures();
                    return unexpected{ServiceErrorInfo{ServiceError::ServiceNotFound,
                                                       "Entry kind not recognized",
                                                       std::string(name)}};
                }
            }
        }
        // Lock released here

        if (needsFactoryCreation)
        {
            auto cached = resolveOrCreateSingleton<T>(typeId, name);
            if (!cached.has_value())
            {
                mStats.incrementResolutionFailures();
                return unexpected{cached.error()};
            }
            mStats.incrementResolutions();
            return std::static_pointer_cast<T>(cached.value());
        }

        // Not found locally - try parent
        if (mParent != nullptr)
        {
            return mParent->template resolveSharedExpected<T>(name);
        }
        mStats.incrementResolutionFailures();
        return unexpected{
            ServiceErrorInfo{ServiceError::ServiceNotFound, "No matching service registration", std::string(name)}};
    }

    /// Resolve a service as shared_ptr, returning empty shared_ptr on failure.
    ///
    /// This is a convenience wrapper around resolveSharedExpected that returns
    /// an empty shared_ptr instead of an error. Use this when you want lifetime
    /// safety without explicit error handling.
    ///
    /// @note For hot-path resolution where lifetime management is not needed,
    ///       prefer tryResolve<T>() which returns a raw pointer with lower overhead.
    ///
    /// @return shared_ptr to the service, or empty shared_ptr if not found or
    ///         if the service is instance-registered (no shared ownership) or transient.
    template <typename T>
    [[nodiscard]] std::shared_ptr<T> tryResolveShared(std::string_view name = {}) const
    {
        auto result = resolveSharedExpected<T>(name);
        return result.has_value() ? result.value() : std::shared_ptr<T>{};
    }

private:
    enum class ServiceEntryKind
    {
        None,
        Instance,
        Shared,
        Factory
    };

    struct ServiceKey
    {
        const void* mTypeId = nullptr;
        std::string mName{};

        bool operator==(const ServiceKey& other) const noexcept
        {
            return mTypeId == other.mTypeId && mName == other.mName;
        }
    };

    struct ServiceKeyView
    {
        const void* mTypeId = nullptr;
        std::string_view mName{};
    };

    struct TypeIdHash
    {
        // Marker to indicate we already produce a high-quality, avalanching hash.
        // This allows StableHashMap to skip its built-in mixer (avoids double-mixing).
        using is_avalanching = void;

        [[nodiscard]] size_t operator()(const void* p) const noexcept
        {
            return hashPtr(p);
        }

    private:
        [[nodiscard]] static size_t hashPtr(const void* p) noexcept
        {
            const uintptr_t x = reinterpret_cast<uintptr_t>(p);

            if constexpr (sizeof(size_t) >= 8)
            {
                // SplitMix64 finalizer (same mixer used in FatPHashMap benchmarks).
                uint64_t z = static_cast<uint64_t>(x);
                z = (z ^ (z >> 30U)) * 0xBF58476D1CE4E5B9ULL;
                z = (z ^ (z >> 27U)) * 0x94D049BB133111EBULL;
                z = z ^ (z >> 31U);
                return static_cast<size_t>(z);
            }
            else
            {
                // MurmurHash3 32-bit finalizer
                uint32_t h = static_cast<uint32_t>(x);
                h ^= h >> 16U;
                h *= 0x85ebca6bU;
                h ^= h >> 13U;
                h *= 0xc2b2ae35U;
                h ^= h >> 16U;
                return static_cast<size_t>(h);
            }
        }
    };

    struct ServiceKeyHash
    {
        using is_avalanching = void;

        [[nodiscard]] size_t operator()(const ServiceKey& key) const noexcept
        {
            return hashImpl(key.mTypeId, key.mName);
        }

        [[nodiscard]] size_t operator()(const ServiceKeyView& key) const noexcept
        {
            return hashImpl(key.mTypeId, key.mName);
        }

    private:
        [[nodiscard]] static size_t hashImpl(const void* typeId, std::string_view name) noexcept
        {
            // Combine the type-id and name hashes, then apply an explicit finalizer so
            // StableHashMap can treat this hash as avalanching (see is_avalanching).
            size_t h = TypeIdHash{}(typeId);
            const size_t hName = std::hash<std::string_view>{}(name);

            constexpr size_t kMagic = (sizeof(size_t) >= 8)
                ? static_cast<size_t>(0x9e3779b97f4a7c15ULL)
                : static_cast<size_t>(0x9e3779b9U);

            h ^= (hName + kMagic + (h << 6U) + (h >> 2U));

            if constexpr (sizeof(size_t) >= 8)
            {
                // MurmurHash3 finalizer (64-bit)
                h ^= h >> 33U;
                h *= 0xff51afd7ed558ccdULL;
                h ^= h >> 33U;
                h *= 0xc4ceb9fe1a85ec53ULL;
                h ^= h >> 33U;
            }
            else
            {
                // MurmurHash3 finalizer (32-bit)
                h ^= h >> 16U;
                h *= 0x85ebca6bU;
                h ^= h >> 13U;
                h *= 0xc2b2ae35U;
                h ^= h >> 16U;
            }

            return h;
        }
    };

    struct ServiceKeyEq
    {
        [[nodiscard]] bool operator()(const ServiceKey& a, const ServiceKey& b) const noexcept
        {
            return a.mTypeId == b.mTypeId && a.mName == b.mName;
        }

        [[nodiscard]] bool operator()(const ServiceKey& a, const ServiceKeyView& b) const noexcept
        {
            return a.mTypeId == b.mTypeId && a.mName == b.mName;
        }

        [[nodiscard]] bool operator()(const ServiceKeyView& a, const ServiceKey& b) const noexcept
        {
            return a.mTypeId == b.mTypeId && a.mName == b.mName;
        }
    };

    struct ServiceEntry
    {
        ServiceEntryKind mKind = ServiceEntryKind::None;
        ServiceLifetime mLifetime = ServiceLifetime::Singleton;
        void* mInstance = nullptr;
        std::shared_ptr<void> mShared{};
        std::function<std::shared_ptr<void>()> mFactory{};

        // Per-entry state for singleton creation coordination
        struct SingletonState
        {
            std::mutex mMutex{};
            std::condition_variable mCv{};
            bool mCreating = false;
            std::thread::id mCreatingThread{};
            std::shared_ptr<void> mValue{};
        };
        mutable std::shared_ptr<SingletonState> mSingletonState{};
    };

    // Two-level storage for optimal performance:
    // Level 1: Unnamed services (fast path) - void* key only, no string allocation
    // Level 2: Named services - full ServiceKey with string name
    //
    // StableHashMap provides reference stability: pointers to entries remain valid
    // across insert/rehash operations. This eliminates the need for snapshot copies.
    using UnnamedRegistry = StableHashMap<const void*, ServiceEntry, TypeIdHash>;
    using NamedRegistry = StableHashMap<ServiceKey, ServiceEntry, ServiceKeyHash, ServiceKeyEq>;

    const ServiceLocator* mParent = nullptr;
    std::shared_ptr<RootState> mRootState{};
    mutable UnnamedRegistry mUnnamedRegistry{};
    mutable NamedRegistry mNamedRegistry{};
    mutable StatsType mStats{};

    [[nodiscard]] auto readLock() const
    {
        return static_cast<const ConcurrencyPolicy&>(*this).lock_shared();
    }

    [[nodiscard]] auto writeLock()
    {
        return static_cast<ConcurrencyPolicy&>(*this).lock();
    }

    [[nodiscard]] auto writeLock() const
    {
        auto& policy = const_cast<ConcurrencyPolicy&>(static_cast<const ConcurrencyPolicy&>(*this));
        return policy.lock();
    }

    [[nodiscard]] std::uint64_t registryEpoch() const noexcept
    {
        // RootState always exists for all scopes produced from a locator.
        return mRootState->mRegistryEpoch.load(std::memory_order_relaxed);
    }

    void bumpRegistryEpoch() noexcept
    {
        mRootState->mRegistryEpoch.fetch_add(1, std::memory_order_relaxed);
    }

    template <typename T>
    static void assertRegistrableServiceType()
    {
        using U = std::remove_reference_t<T>;
        static_assert(!std::is_const_v<U> && !std::is_volatile_v<U>,
                      "ServiceLocator does not permit registration of cv-qualified service types.");
    }

    [[nodiscard]] bool unregisterUntyped(const void* typeId, std::string_view name)
    {
        auto lock = writeLock();

        bool removed = false;
        if (name.empty())
        {
            removed = mUnnamedRegistry.erase(typeId);
        }
        else
        {
            ServiceKeyView key{typeId, name};
            removed = mNamedRegistry.erase(key);
        }

        if (removed)
        {
            bumpRegistryEpoch();
            mStats.incrementUnregistrations();
        }
        return removed;
    }

    template <typename T>
    [[nodiscard]] Expected<std::shared_ptr<void>, ServiceErrorInfo>
    resolveOrCreateSingleton(const void* typeId, std::string_view name) const
    {
        // Get SingletonState under write lock
        std::shared_ptr<typename ServiceEntry::SingletonState> state;
        std::function<std::shared_ptr<void>()> factoryCopy;
        {
            auto lock = writeLock();

            // Find entry
            ServiceEntry* entry = nullptr;
            if (name.empty())
            {
                entry = mUnnamedRegistry.find(typeId);
            }
            else
            {
                ServiceKeyView key{typeId, name};
                entry = mNamedRegistry.find(key);
            }

            if (entry == nullptr)
            {
                return unexpected{ServiceErrorInfo{ServiceError::ServiceNotFound,
                                                   "Singleton factory removed during resolution",
                                                   std::string(name)}};
            }

            if (entry->mLifetime != ServiceLifetime::Singleton)
            {
                return unexpected{ServiceErrorInfo{ServiceError::InvalidLifetime,
                                                   "resolveOrCreateSingleton requires Singleton lifetime",
                                                   std::string(name)}};
            }

            // Fast path: already created
            if (entry->mShared)
            {
                return entry->mShared;
            }

            if (!entry->mSingletonState)
            {
                entry->mSingletonState = std::make_shared<typename ServiceEntry::SingletonState>();
            }
            state = entry->mSingletonState;
            factoryCopy = entry->mFactory;  // Copy factory for use outside lock
        }


        // Coordinate via SingletonState (no registry lock held)
        const std::thread::id thisThread = std::this_thread::get_id();
        std::unique_lock<std::mutex> stateLock(state->mMutex);

        if (state->mValue)
        {
            stateLock.unlock();
            return publishSingleton(typeId, name, state);
        }

        if (state->mCreating)
        {
            if (state->mCreatingThread == thisThread)
            {
                return unexpected{ServiceErrorInfo{ServiceError::CircularDependency,
                                                   "Singleton factory attempted to resolve itself "
                                                   "(circular dependency)",
                                                   std::string(name)}};
            }

            state->mCv.wait(stateLock, [&state]() { return !state->mCreating; });

            if (state->mValue)
            {
                stateLock.unlock();
                return publishSingleton(typeId, name, state);
            }
        }

        stateLock.unlock();

        // Serialize singleton factory execution to prevent cross-thread deadlocks.
        // The gate is re-entrant per thread, allowing factories to resolve other
        // singletons safely within the same call chain.
        FATP_ENFORCE(mRootState != nullptr, "ServiceLocator RootState must be initialized");
        typename SingletonFactoryGate::Guard gate(mRootState->mSingletonFactoryGate);

        stateLock.lock();

        if (state->mValue)
        {
            stateLock.unlock();
            return publishSingleton(typeId, name, state);
        }

        if (state->mCreating)
        {
            // Under the gate, only this thread should be able to be creating.
            if (state->mCreatingThread == thisThread)
            {
                return unexpected{ServiceErrorInfo{ServiceError::CircularDependency,
                                                   "Singleton factory attempted to resolve itself "
                                                   "(circular dependency)",
                                                   std::string(name)}};
            }

            state->mCv.wait(stateLock, [&state]() { return !state->mCreating; });

            if (state->mValue)
            {
                stateLock.unlock();
                return publishSingleton(typeId, name, state);
            }
        }

        // We are the creator (and gate owner)
        state->mCreating = true;
        state->mCreatingThread = thisThread;
        stateLock.unlock();

        // Invoke factory with NO locks held (gate is held)
        std::shared_ptr<void> created;
        try
        {
            created = factoryCopy ? factoryCopy() : nullptr;
            if (!created)
            {
                stateLock.lock();
                state->mCreating = false;
                state->mCreatingThread = std::thread::id{};
                stateLock.unlock();
                state->mCv.notify_all();
                return unexpected{ServiceErrorInfo{ServiceError::FactoryReturnedNull,
                                                   "Factory returned an empty shared_ptr",
                                                   std::string(name)}};
            }
        }
        catch (const std::exception& e)
        {
            stateLock.lock();
            state->mCreating = false;
            state->mCreatingThread = std::thread::id{};
            stateLock.unlock();
            state->mCv.notify_all();
            return unexpected{ServiceErrorInfo{ServiceError::FactoryThrew,
                                               std::string("Factory threw: ") + e.what(),
                                               std::string(name)}};
        }
        catch (...)
        {
            stateLock.lock();
            state->mCreating = false;
            state->mCreatingThread = std::thread::id{};
            stateLock.unlock();
            state->mCv.notify_all();
            return unexpected{ServiceErrorInfo{ServiceError::FactoryThrew,
                                               "Factory threw: non-std exception",
                                               std::string(name)}};
        }

        stateLock.lock();
        state->mCreating = false;
        state->mCreatingThread = std::thread::id{};
        state->mValue = created;
        stateLock.unlock();
        state->mCv.notify_all();

        return publishSingleton(typeId, name, state);
    }

    [[nodiscard]] Expected<std::shared_ptr<void>, ServiceErrorInfo>
    publishSingleton(const void* typeId, std::string_view name,
                     const std::shared_ptr<typename ServiceEntry::SingletonState>& state) const
    {
        auto lock = writeLock();

        ServiceEntry* entry = nullptr;
        if (name.empty())
        {
            entry = mUnnamedRegistry.find(typeId);
        }
        else
        {
            ServiceKeyView key{typeId, name};
            entry = mNamedRegistry.find(key);
        }

        if (entry == nullptr)
        {
            return unexpected{ServiceErrorInfo{ServiceError::ServiceNotFound,
                                               "Singleton entry removed during creation",
                                               std::string(name)}};
        }

        if (entry->mKind != ServiceEntryKind::Factory ||
            entry->mLifetime != ServiceLifetime::Singleton ||
            entry->mSingletonState != state)
        {
            return unexpected{ServiceErrorInfo{ServiceError::ServiceNotFound,
                                               "Singleton entry replaced during creation",
                                               std::string(name)}};
        }

        if (entry->mShared)
        {
            return entry->mShared;
        }

        if (!state->mValue)
        {
            return unexpected{ServiceErrorInfo{ServiceError::ServiceNotFound,
                                               "Singleton value not available after creation",
                                               std::string(name)}};
        }

        entry->mShared = state->mValue;
        return entry->mShared;
    }
};

// ============================================================================
// Scope RAII Helper
// ============================================================================

template <typename ConcurrencyPolicy,
          typename RegistrationPolicy,
          typename StatisticsPolicy,
          typename TypeKeyPolicy,
          typename ResolveCachePolicy>
class ServiceLocator<ConcurrencyPolicy,
                     RegistrationPolicy,
                     StatisticsPolicy,
                     TypeKeyPolicy,
                     ResolveCachePolicy>::Scope
{
public:
    explicit Scope(const ServiceLocator& parent)
        : mChild(&parent)
    {
    }

    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
    Scope(Scope&&) = delete;
    Scope& operator=(Scope&&) = delete;

    ~Scope() = default;

    [[nodiscard]] ServiceLocator& locator() noexcept
    {
        return mChild;
    }

    [[nodiscard]] const ServiceLocator& locator() const noexcept
    {
        return mChild;
    }

private:
    ServiceLocator mChild;
};

// ============================================================================
// Registration RAII Helper
// ============================================================================

template <typename ConcurrencyPolicy,
          typename RegistrationPolicy,
          typename StatisticsPolicy,
          typename TypeKeyPolicy,
          typename ResolveCachePolicy>
class ServiceLocator<ConcurrencyPolicy,
                     RegistrationPolicy,
                     StatisticsPolicy,
                     TypeKeyPolicy,
                     ResolveCachePolicy>::Registration
{
public:
    Registration() = default;

    template <typename T>
    static Expected<Registration, ServiceErrorInfo>
    registerInstanceExpected(ServiceLocator& locator, T& instance, std::string_view name = {})
    {
        auto result = locator.registerInstance<T>(instance, name);
        if (!result.has_value())
        {
            return unexpected{result.error()};
        }
        return Registration(locator, TypeKeyPolicy::template typeId<T>(), std::string(name));
    }

    template <typename T>
    static Expected<Registration, ServiceErrorInfo>
    registerSharedExpected(ServiceLocator& locator, std::shared_ptr<T> instance, std::string_view name = {})
    {
        auto result = locator.registerShared<T>(std::move(instance), name);
        if (!result.has_value())
        {
            return unexpected{result.error()};
        }
        return Registration(locator, TypeKeyPolicy::template typeId<T>(), std::string(name));
    }

    template <typename T, typename Factory>
    static Expected<Registration, ServiceErrorInfo>
    registerFactoryExpected(ServiceLocator& locator,
                            Factory&& factory,
                            ServiceLifetime lifetime,
                            std::string_view name = {})
    {
        auto result = locator.template registerFactory<T>(std::forward<Factory>(factory), lifetime, name);
        if (!result.has_value())
        {
            return unexpected{result.error()};
        }
        return Registration(locator, TypeKeyPolicy::template typeId<T>(), std::string(name));
    }

    Registration(const Registration&) = delete;
    Registration& operator=(const Registration&) = delete;

    Registration(Registration&& other) noexcept
        : mLocator(other.mLocator)
        , mTypeId(other.mTypeId)
        , mName(std::move(other.mName))
    {
        other.mLocator = nullptr;
        other.mTypeId = nullptr;
        other.mName.clear();
    }

    Registration& operator=(Registration&& other) noexcept
    {
        if (this != &other)
        {
            reset();
            mLocator = other.mLocator;
            mTypeId = other.mTypeId;
            mName = std::move(other.mName);
            other.mLocator = nullptr;
            other.mTypeId = nullptr;
            other.mName.clear();
        }
        return *this;
    }

    ~Registration()
    {
        reset();
    }

    void reset()
    {
        if (mLocator != nullptr && mTypeId != nullptr)
        {
            (void)mLocator->unregisterUntyped(mTypeId, mName);
        }
        mLocator = nullptr;
        mTypeId = nullptr;
        mName.clear();
    }

    /// Returns true if this Registration is managing an active service registration.
    [[nodiscard]] explicit operator bool() const noexcept
    {
        return mLocator != nullptr && mTypeId != nullptr;
    }

private:
    Registration(ServiceLocator& locator, const void* typeId, std::string name)
        : mLocator(&locator)
        , mTypeId(typeId)
        , mName(std::move(name))
    {
    }

    ServiceLocator* mLocator = nullptr;
    const void* mTypeId = nullptr;
    std::string mName{};
};

// Convenience aliases
using DefaultServiceLocator = ServiceLocator<SingleThreadedPolicy>;
using ThreadSafeServiceLocator = ServiceLocator<SharedMutexPolicy>;

// Opt-in: type-only resolve cache (thread-local 2-entry MRU). Best for hot loops.
using HotLoopServiceLocator = ServiceLocator<SingleThreadedPolicy,
                                            ServicePreventOverwritePolicy,
                                            NoServiceLocatorStatisticsPolicy,
                                            detail::DefaultServiceTypeKeyPolicy,
                                            TwoEntryServiceLocatorResolveCachePolicy>;

using ThreadSafeHotLoopServiceLocator = ServiceLocator<SharedMutexPolicy,
                                                      ServicePreventOverwritePolicy,
                                                      NoServiceLocatorStatisticsPolicy,
                                                      detail::DefaultServiceTypeKeyPolicy,
                                                      TwoEntryServiceLocatorResolveCachePolicy>;

} // namespace fat_p
