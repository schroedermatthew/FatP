/**
 * @file ServiceLocator.h
 * @brief Policy-based service locator with scoped overrides.
 *
 * @layer Domain
 *
 * A ServiceLocator provides type-safe registration and resolution of services.
 * It supports:
 *   - non-owning instance registration
 *   - owning shared_ptr registration
 *   - factory registration (singleton or transient)
 *   - parent/child layering (scoped overrides)
 *
 * Complexity:
 *   - register/unregister: Average O(1) + potential allocation (unordered_map growth)
 *   - tryResolve/resolve: Average O(1)
 *   - createExpected (factory): Average O(1) + factory cost
 *
 * Thread Safety:
 *   - Defined by ConcurrencyPolicy
 *   - With SharedMutexPolicy: concurrent resolves are permitted; registration is exclusive
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

#pragma once

/*
FATP_META:
  meta_version: 1
  component: ServiceLocator
  file_role: public_header
  path: fat_p/ServiceLocator.h
  namespace: fat_p
  layer: Domain
  summary: "Public header for ServiceLocator."
  api_stability: in_work
  related:
    docs_search: "ServiceLocator"
    tests:
      - tests/test_ServiceLocator.cpp
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

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_map>
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
    CircularDependency ///< Factory tried to resolve itself (directly or indirectly)
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

struct NoServiceLocatorStatisticsPolicy
{
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

        void incrementRegistrations() noexcept
        {
        }
        void incrementRegistrationFailures() noexcept
        {
        }
        void incrementResolutions() noexcept
        {
        }
        void incrementResolutionFailures() noexcept
        {
        }
        void incrementCreations() noexcept
        {
        }
        void incrementCreationFailures() noexcept
        {
        }
        void incrementUnregistrations() noexcept
        {
        }
        void reset() noexcept
        {
        }
        [[nodiscard]] Snapshot snapshot() const noexcept
        {
            return {};
        }
    };
};

struct AtomicServiceLocatorStatisticsPolicy
{
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
            return Snapshot{mRegistrations.load(std::memory_order_relaxed),
                            mRegistrationFailures.load(std::memory_order_relaxed),
                            mResolutions.load(std::memory_order_relaxed),
                            mResolutionFailures.load(std::memory_order_relaxed),
                            mCreations.load(std::memory_order_relaxed),
                            mCreationFailures.load(std::memory_order_relaxed),
                            mUnregistrations.load(std::memory_order_relaxed)};
        }

        void incrementRegistrations() noexcept
        {
            mRegistrations.fetch_add(1, std::memory_order_relaxed);
        }
        void incrementRegistrationFailures() noexcept
        {
            mRegistrationFailures.fetch_add(1, std::memory_order_relaxed);
        }
        void incrementResolutions() noexcept
        {
            mResolutions.fetch_add(1, std::memory_order_relaxed);
        }
        void incrementResolutionFailures() noexcept
        {
            mResolutionFailures.fetch_add(1, std::memory_order_relaxed);
        }
        void incrementCreations() noexcept
        {
            mCreations.fetch_add(1, std::memory_order_relaxed);
        }
        void incrementCreationFailures() noexcept
        {
            mCreationFailures.fetch_add(1, std::memory_order_relaxed);
        }
        void incrementUnregistrations() noexcept
        {
            mUnregistrations.fetch_add(1, std::memory_order_relaxed);
        }
    };
};

struct ServicePreventOverwritePolicy
{
    template <typename Registry, typename Key, typename Value, typename Stats>
    static bool insert(Registry& registry, const Key& key, Value&& value, Stats& stats)
    {
        auto [it, inserted] = registry.try_emplace(key, std::forward<Value>(value));
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
        (void)registry.insert_or_assign(key, std::forward<Value>(value));
        stats.incrementRegistrations();
        return true;
    }
};

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
struct is_shared_ptr : std::false_type
{
};

template <typename U>
struct is_shared_ptr<std::shared_ptr<U>> : std::true_type
{
};

template <typename T>
inline constexpr bool is_shared_ptr_v = is_shared_ptr<T>::value;

template <typename T>
struct is_unique_ptr : std::false_type
{
};

template <typename U, typename D>
struct is_unique_ptr<std::unique_ptr<U, D>> : std::true_type
{
};

template <typename T>
inline constexpr bool is_unique_ptr_v = is_unique_ptr<T>::value;

template <typename T, typename Factory>
std::shared_ptr<T> invokeFactoryToShared(Factory& factory)
{
    using Ret = std::invoke_result_t<Factory&>;

    if constexpr (detail::is_shared_ptr_v<Ret>)
    {
        return factory();
    }
    else if constexpr (detail::is_unique_ptr_v<Ret>)
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
          typename TypeKeyPolicy = detail::DefaultServiceTypeKeyPolicy>
class ServiceLocator : private ConcurrencyPolicy
{
public:
    using StatsType = typename StatisticsPolicy::Stats;
    using RegisterResult = Expected<void, ServiceErrorInfo>;

    class Scope;
    class Registration;

    ServiceLocator() = default;

    explicit ServiceLocator(const ServiceLocator* parent)
        : mParent(parent)
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

    [[nodiscard]] Scope makeScope() const
    {
        return Scope(*this);
    }


    [[nodiscard]] ServiceLocator makeChild() const&
    {
        return ServiceLocator(this);
    }

    ServiceLocator makeChild() const&& = delete;

    template <typename T>
    [[nodiscard]] RegisterResult registerInstance(T& instance, std::string_view name = {})
    {
        assertRegistrableServiceType<T>();
        ServiceKey key = makeKey<T>(name);
        ServiceEntry entry;
        entry.mKind = ServiceEntryKind::Instance;
        entry.mLifetime = ServiceLifetime::Singleton;
        entry.mInstance = std::addressof(instance);

        auto lock = writeLock();
        if (!RegistrationPolicy::insert(mRegistry, key, std::move(entry), mStats))
        {
            return unexpected{ServiceErrorInfo{ServiceError::ServiceAlreadyExists,
                                               "Instance registration rejected by RegistrationPolicy",
                                               std::string(name)}};
        }
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

        ServiceKey key = makeKey<T>(name);
        ServiceEntry entry;
        entry.mKind = ServiceEntryKind::Shared;
        entry.mLifetime = ServiceLifetime::Singleton;
        entry.mShared = std::move(instance);

        auto lock = writeLock();
        if (!RegistrationPolicy::insert(mRegistry, key, std::move(entry), mStats))
        {
            return unexpected{ServiceErrorInfo{ServiceError::ServiceAlreadyExists,
                                               "Shared registration rejected by RegistrationPolicy",
                                               std::string(name)}};
        }
        return {};
    }

    template <typename T, typename Factory>
    [[nodiscard]] RegisterResult registerFactory(Factory factory, ServiceLifetime lifetime, std::string_view name = {})
    {
        assertRegistrableServiceType<T>();

        if (lifetime != ServiceLifetime::Singleton && lifetime != ServiceLifetime::Transient)
        {
            return unexpected{ServiceErrorInfo{ServiceError::InvalidLifetime,
                                               "Only Singleton and Transient lifetimes are valid for factories",
                                               std::string(name)}};
        }

        ServiceKey key = makeKey<T>(name);

        ServiceEntry entry;
        entry.mKind = ServiceEntryKind::Factory;
        entry.mLifetime = lifetime;
        entry.mFactory = [factory = std::move(factory)]() mutable -> std::shared_ptr<void> {
            auto sp = detail::invokeFactoryToShared<T>(factory);
            return sp;
        };

        auto lock = writeLock();
        if (!RegistrationPolicy::insert(mRegistry, key, std::move(entry), mStats))
        {
            return unexpected{ServiceErrorInfo{ServiceError::ServiceAlreadyExists,
                                               "Factory registration rejected by RegistrationPolicy",
                                               std::string(name)}};
        }

        return {};
    }

    template <typename T>
    [[nodiscard]] bool unregister(std::string_view name = {})
    {
        ServiceKey key = makeKey<T>(name);
        auto lock = writeLock();
        const size_t removed = mRegistry.erase(key);
        if (removed != 0)
        {
            mStats.incrementUnregistrations();
        }
        return removed != 0;
    }

    void clear()
    {
        auto lock = writeLock();
        mRegistry.clear();
    }

    [[nodiscard]] size_t size() const
    {
        auto lock = readLock();
        return mRegistry.size();
    }

    [[nodiscard]] bool empty() const
    {
        return size() == 0;
    }

    [[nodiscard]] StatsType& stats() noexcept
    {
        return mStats;
    }

    [[nodiscard]] const StatsType& stats() const noexcept
    {
        return mStats;
    }

    template <typename T>
    [[nodiscard]] T* tryResolve(std::string_view name = {}) const
    {
        auto expected = resolveExpected<T>(name);
        if (!expected.has_value())
        {
            return nullptr;
        }
        return std::addressof(expected.value().get());
    }

    template <typename T>
    [[nodiscard]] Expected<std::reference_wrapper<T>, ServiceErrorInfo>
    resolveExpected(std::string_view name = {}) const
    {
        const ServiceKey key = makeKey<T>(name);
        auto local = resolveEntryForRead(key);
        if (!local.has_value())
        {
            if (mParent != nullptr)
            {
                return mParent->template resolveExpected<T>(name);
            }
            mStats.incrementResolutionFailures();
            return unexpected{
                ServiceErrorInfo{ServiceError::ServiceNotFound, "No matching service registration", std::string(name)}};
        }

        const ServiceEntrySnapshot snap = local.value();
        if (snap.mKind == ServiceEntryKind::Instance)
        {
            mStats.incrementResolutions();
            return std::ref(*static_cast<T*>(snap.mInstance));
        }
        if (snap.mKind == ServiceEntryKind::Shared)
        {
            if (!snap.mShared)
            {
                mStats.incrementResolutionFailures();
                return unexpected{ServiceErrorInfo{ServiceError::NullSharedInstance,
                                                   "Shared registration holds an empty shared_ptr",
                                                   std::string(name)}};
            }
            mStats.incrementResolutions();
            return std::ref(*static_cast<T*>(snap.mShared.get()));
        }
        if (snap.mKind == ServiceEntryKind::Factory)
        {
            if (snap.mLifetime == ServiceLifetime::Transient)
            {
                mStats.incrementResolutionFailures();
                return unexpected{ServiceErrorInfo{ServiceError::FactoryNotRegistered,
                                                   "Transient services require createExpected<T>()",
                                                   std::string(name)}};
            }

            auto cached = resolveOrCreateSingleton(key, snap, std::string(name));
            if (!cached.has_value())
            {
                mStats.incrementResolutionFailures();
                return unexpected{cached.error()};
            }
            mStats.incrementResolutions();
            return std::ref(*static_cast<T*>(cached.value().get()));
        }

        mStats.incrementResolutionFailures();
        return unexpected{
            ServiceErrorInfo{ServiceError::ServiceNotFound, "Entry kind not recognized", std::string(name)}};
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
        const ServiceKey key = makeKey<T>(name);
        auto local = resolveEntryForRead(key);
        if (!local.has_value())
        {
            if (mParent != nullptr)
            {
                return mParent->template createExpected<T>(name);
            }
            mStats.incrementCreationFailures();
            return unexpected{
                ServiceErrorInfo{ServiceError::ServiceNotFound, "No matching service registration", std::string(name)}};
        }

        const ServiceEntrySnapshot snap = local.value();
        if (snap.mKind != ServiceEntryKind::Factory)
        {
            mStats.incrementCreationFailures();
            return unexpected{ServiceErrorInfo{ServiceError::FactoryNotRegistered,
                                               "No factory registered for this service",
                                               std::string(name)}};
        }

        if (snap.mLifetime == ServiceLifetime::Singleton)
        {
            auto cached = resolveOrCreateSingleton(key, snap, std::string(name));
            if (!cached.has_value())
            {
                mStats.incrementCreationFailures();
                return unexpected{cached.error()};
            }
            mStats.incrementCreations();
            return std::static_pointer_cast<T>(cached.value());
        }

        auto created = invokeFactory(snap, std::string(name));
        if (!created.has_value())
        {
            mStats.incrementCreationFailures();
            return unexpected{created.error()};
        }
        mStats.incrementCreations();
        return std::static_pointer_cast<T>(created.value());
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
    };

    struct ServiceKeyHash
    {
        [[nodiscard]] size_t operator()(const ServiceKey& key) const noexcept
        {
            const size_t h1 = std::hash<const void*>{}(key.mTypeId);
            const size_t h2 = std::hash<std::string>{}(key.mName);
            constexpr size_t kMagic = static_cast<size_t>(0x9e3779b97f4a7c15ULL);
            return h1 ^ (h2 + kMagic + (h1 << 6U) + (h1 >> 2U));
        }
    };

    struct ServiceKeyEq
    {
        [[nodiscard]] bool operator()(const ServiceKey& a, const ServiceKey& b) const noexcept
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

        /// Per-entry state for singleton creation coordination.
        ///
        /// Design rationale: We use shared_ptr to a separate state object because:
        /// 1. std::mutex/condition_variable are not movable, but ServiceEntry must be
        ///    movable for unordered_map storage
        /// 2. After releasing the registry lock, another thread could unregister/clear
        ///    the entry, making any raw pointers to ServiceEntry dangling. By holding
        ///    a shared_ptr<SingletonState>, the coordination state survives even if
        ///    the registry entry is deleted.
        /// 3. We store mValue in SingletonState (not just ServiceEntry) so that
        ///    threads blocked on mCv can safely read the result without re-acquiring
        ///    the registry lock.
        /// 4. After successful creation, we re-acquire the registry lock to publish
        ///    mValue to ServiceEntry::mShared (if the entry still exists).
        struct SingletonState
        {
            std::mutex mMutex{};
            std::condition_variable mCv{};
            bool mCreating = false;
            std::thread::id mCreatingThread{};
            std::shared_ptr<void> mValue{}; // Result stored here for lifetime safety
        };
        mutable std::shared_ptr<SingletonState> mSingletonState{};

        /// Ensures the singleton state exists. Called while holding registry lock.
        std::shared_ptr<SingletonState> ensureSingletonState() const
        {
            if (!mSingletonState)
            {
                mSingletonState = std::make_shared<SingletonState>();
            }
            return mSingletonState;
        }
    };

    struct ServiceEntrySnapshot
    {
        ServiceEntryKind mKind = ServiceEntryKind::None;
        ServiceLifetime mLifetime = ServiceLifetime::Singleton;
        void* mInstance = nullptr;
        std::shared_ptr<void> mShared{};
        std::function<std::shared_ptr<void>()> mFactory{};
    };

    using Registry = std::unordered_map<ServiceKey, ServiceEntry, ServiceKeyHash, ServiceKeyEq>;

    const ServiceLocator* mParent = nullptr;
    mutable Registry mRegistry{};
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

    template <typename T>
    [[nodiscard]] static ServiceKey makeKey(std::string_view name)
    {
        ServiceKey key;
        key.mTypeId = TypeKeyPolicy::template typeId<T>();
        key.mName = std::string(name);
        return key;
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
        ServiceKey key;
        key.mTypeId = typeId;
        key.mName = std::string(name);

        auto lock = writeLock();
        const size_t removed = mRegistry.erase(key);
        if (removed != 0)
        {
            mStats.incrementUnregistrations();
        }
        return removed != 0;
    }

    [[nodiscard]] std::optional<ServiceEntrySnapshot> resolveEntryForRead(const ServiceKey& key) const
    {
        auto lock = readLock();
        auto it = mRegistry.find(key);
        if (it == mRegistry.end())
        {
            return std::nullopt;
        }

        ServiceEntrySnapshot snap;
        snap.mKind = it->second.mKind;
        snap.mLifetime = it->second.mLifetime;
        snap.mInstance = it->second.mInstance;
        snap.mShared = it->second.mShared;
        snap.mFactory = it->second.mFactory;
        return snap;
    }

    [[nodiscard]] Expected<std::shared_ptr<void>, ServiceErrorInfo> invokeFactory(const ServiceEntrySnapshot& snap,
                                                                                  std::string name) const
    {
        return invokeFactoryImpl(snap.mFactory, std::move(name));
    }

private:
    [[nodiscard]] Expected<std::shared_ptr<void>, ServiceErrorInfo>
    invokeFactoryImpl(const std::function<std::shared_ptr<void>()>& factory, std::string name) const
    {
        try
        {
            std::shared_ptr<void> created = factory ? factory() : nullptr;
            if (!created)
            {
                return unexpected{ServiceErrorInfo{ServiceError::FactoryReturnedNull,
                                                   "Factory returned an empty shared_ptr",
                                                   std::move(name)}};
            }
            return created;
        }
        catch (const std::exception& e)
        {
            return unexpected{ServiceErrorInfo{ServiceError::FactoryThrew,
                                               std::string("Factory threw: ") + e.what(),
                                               std::move(name)}};
        }
        catch (...)
        {
            return unexpected{
                ServiceErrorInfo{ServiceError::FactoryThrew, "Factory threw: non-std exception", std::move(name)}};
        }
    }

    [[nodiscard]] Expected<std::shared_ptr<void>, ServiceErrorInfo>
    resolveOrCreateSingleton(const ServiceKey& key, const ServiceEntrySnapshot& snap, std::string name) const
    {
        if (snap.mLifetime != ServiceLifetime::Singleton)
        {
            return unexpected{ServiceErrorInfo{ServiceError::InvalidLifetime,
                                               "resolveOrCreateSingleton requires Singleton lifetime",
                                               std::move(name)}};
        }

        // Fast path: already created (from snapshot taken under read lock)
        if (snap.mShared)
        {
            return snap.mShared;
        }

        // Slow path: coordinate singleton creation.
        //
        // CRITICAL LIFETIME SAFETY: After releasing the registry lock, another thread
        // could unregister/clear the entry. We must NOT access any ServiceEntry* after
        // the lock is released. Instead:
        //   1. Copy shared_ptr<SingletonState> out while holding lock (stays alive)
        //   2. Store result in state->mValue (not ServiceEntry::mShared)
        //   3. Re-acquire lock to publish to registry IF entry still exists

        // Step 1: Get SingletonState under registry lock
        std::shared_ptr<typename ServiceEntry::SingletonState> state;
        {
            auto lock = writeLock();
            auto it = mRegistry.find(key);
            if (it == mRegistry.end())
            {
                return unexpected{ServiceErrorInfo{ServiceError::ServiceNotFound,
                                                   "Singleton factory removed during resolution",
                                                   std::move(name)}};
            }

            // Quick check: maybe another thread already created it
            if (it->second.mShared)
            {
                return it->second.mShared;
            }

            // Get/create the SingletonState (returned as shared_ptr)
            state = it->second.ensureSingletonState();
        }
        // Registry lock released - we now only interact with `state` and `snap`
        // Both are safe: state is shared_ptr, snap is a copy

        auto publishAndGet = [&](const std::shared_ptr<typename ServiceEntry::SingletonState>& localState)
            -> Expected<std::shared_ptr<void>, ServiceErrorInfo> {
            auto lock = writeLock();
            auto it = mRegistry.find(key);
            if (it == mRegistry.end())
            {
                return unexpected{ServiceErrorInfo{ServiceError::ServiceNotFound,
                                                   "Singleton entry removed during creation",
                                                   std::string(name)}};
            }

            if (it->second.mKind != ServiceEntryKind::Factory || it->second.mLifetime != ServiceLifetime::Singleton ||
                it->second.mSingletonState != localState)
            {
                return unexpected{ServiceErrorInfo{ServiceError::ServiceNotFound,
                                                   "Singleton entry replaced during creation",
                                                   std::string(name)}};
            }

            if (it->second.mShared)
            {
                return it->second.mShared;
            }

            if (!localState->mValue)
            {
                return unexpected{ServiceErrorInfo{ServiceError::ServiceNotFound,
                                                   "Singleton value not available after creation",
                                                   std::string(name)}};
            }

            it->second.mShared = localState->mValue;
            return it->second.mShared;
        };

        // Step 2: Coordinate via SingletonState (no registry lock held)
        const auto thisThread = std::this_thread::get_id();
        std::unique_lock<std::mutex> stateLock(state->mMutex);

        // Check if already created (stored in state->mValue for lifetime safety)
        if (state->mValue)
        {
            stateLock.unlock();
            return publishAndGet(state);
        }

        // Check if another thread is currently creating
        if (state->mCreating)
        {
            // Circular dependency detection: same thread trying to create again
            if (state->mCreatingThread == thisThread)
            {
                return unexpected{
                    ServiceErrorInfo{ServiceError::CircularDependency,
                                     "Singleton factory attempted to resolve itself (circular dependency)",
                                     std::move(name)}};
            }

            // Different thread is creating - wait for it to finish
            state->mCv.wait(stateLock, [&state]() {
                return !state->mCreating;
            });

            // Check result after waking - creator may have succeeded or failed
            if (state->mValue)
            {
                stateLock.unlock();
                return publishAndGet(state);
            }

            // Creator failed. Retry-on-failure semantics: we try creating ourselves.
            // (Alternative: cache-failure semantics would return error here)
        }

        // Step 3: We are the creator
        state->mCreating = true;
        state->mCreatingThread = thisThread;
        stateLock.unlock();

        // Step 4: Invoke factory with NO locks held
        // Factory CAN safely resolve OTHER services (reentrancy supported)
        // We use snap.mFactory (copied earlier), NOT a dangling entry pointer
        auto created = invokeFactory(snap, name);

        // Step 5: Update state and notify waiters
        stateLock.lock();
        state->mCreating = false;
        state->mCreatingThread = std::thread::id{};

        if (!created.has_value())
        {
            // Factory failed. Wake waiters so they can retry.
            stateLock.unlock();
            state->mCv.notify_all();
            return unexpected{created.error()};
        }

        // Success - store in SingletonState (lifetime-safe location)
        state->mValue = created.value();
        stateLock.unlock();
        state->mCv.notify_all();

        // Step 6: Publish to registry and return the registry-held shared_ptr.
        // If the entry was removed or replaced during creation, return an error
        // rather than returning a shared_ptr that is not owned by the registry,
        // because resolveExpected() returns references.
        return publishAndGet(state);
    }
};

template <typename ConcurrencyPolicy, typename RegistrationPolicy, typename StatisticsPolicy, typename TypeKeyPolicy>
class ServiceLocator<ConcurrencyPolicy, RegistrationPolicy, StatisticsPolicy, TypeKeyPolicy>::Scope
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

template <typename ConcurrencyPolicy, typename RegistrationPolicy, typename StatisticsPolicy, typename TypeKeyPolicy>
class ServiceLocator<ConcurrencyPolicy, RegistrationPolicy, StatisticsPolicy, TypeKeyPolicy>::Registration
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

} // namespace fat_p
