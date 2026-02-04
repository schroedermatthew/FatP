#pragma once

// A std-only adaptation of fat_p::ServiceLocator.h
//
// Goals:
//  - No dependencies on FAT-P headers (ConcurrencyPolicies, Expected, StableHashMap, enforce).
//  - Preserve the original class's *behavioral contract* as closely as practical:
//      * instance/shared/factory registration
//      * singleton vs transient factories
//      * parent/child scopes (overrides)
//      * thread-safety selectable via policy
//      * optional tiny thread-local MRU cache for unnamed resolves
//      * circular-dependency detection during singleton construction
//      * cross-thread singleton construction serialized via a re-entrant gate
//
// Notes:
//  - Where FAT-P used StableHashMap for reference stability, this implementation uses
//    std::unordered_map<Key, std::unique_ptr<Entry>> to keep Entry addresses stable
//    across rehash, while still using only std.

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <ostream>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <variant>
#include <unordered_map>

namespace fat_p::std_only
{

// ============================================================================
// Minimal Expected<T, E>
// ============================================================================

template <typename E>
struct unexpected
{
    E mError;
};

template <typename E>
unexpected(E) -> unexpected<E>;

template <typename T, typename E>
class Expected
{
public:
    Expected() = delete;

    Expected(const T& v)
        : mStorage(v)
    {
    }

    Expected(T&& v)
        : mStorage(std::move(v))
    {
    }

    Expected(unexpected<E> u)
        : mStorage(std::move(u.mError))
    {
    }

    [[nodiscard]] bool has_value() const noexcept
    {
        return std::holds_alternative<T>(mStorage);
    }

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return has_value();
    }

    [[nodiscard]] T& value() &
    {
        assert(has_value());
        return std::get<T>(mStorage);
    }

    [[nodiscard]] const T& value() const &
    {
        assert(has_value());
        return std::get<T>(mStorage);
    }

    [[nodiscard]] T&& value() &&
    {
        assert(has_value());
        return std::get<T>(std::move(mStorage));
    }

    [[nodiscard]] E& error() &
    {
        assert(!has_value());
        return std::get<E>(mStorage);
    }

    [[nodiscard]] const E& error() const &
    {
        assert(!has_value());
        return std::get<E>(mStorage);
    }

private:
    std::variant<T, E> mStorage;
};

// void specialization
template <typename E>
class Expected<void, E>
{
public:
    Expected() = default;

    Expected(unexpected<E> u)
        : mError(std::move(u.mError))
    {
    }

    [[nodiscard]] bool has_value() const noexcept
    {
        return !mError.has_value();
    }

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return has_value();
    }

    void value() const
    {
        assert(has_value());
    }

    [[nodiscard]] E& error() &
    {
        assert(!has_value());
        return *mError;
    }

    [[nodiscard]] const E& error() const &
    {
        assert(!has_value());
        return *mError;
    }

private:
    std::optional<E> mError;
};

// ============================================================================
// Core enums + error info
// ============================================================================

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
            return toString(mCode) + ": " + mMessage;
        }
        return toString(mCode) + ": " + mMessage + " (name: " + mName + ")";
    }
};

inline std::ostream& operator<<(std::ostream& os, const ServiceErrorInfo& info)
{
    return os << info.fullMessage();
}

// ============================================================================
// Concurrency policies (std-only)
// ============================================================================

struct SingleThreadedPolicy
{
    struct NoLock
    {
    };

    [[nodiscard]] NoLock lock() noexcept { return {}; }
    [[nodiscard]] NoLock lock_shared() const noexcept { return {}; }
};

struct SharedMutexPolicy
{
    [[nodiscard]] std::unique_lock<std::shared_mutex> lock()
    {
        return std::unique_lock<std::shared_mutex>(mMutex);
    }

    [[nodiscard]] std::shared_lock<std::shared_mutex> lock_shared() const
    {
        return std::shared_lock<std::shared_mutex>(mMutex);
    }

private:
    mutable std::shared_mutex mMutex{};
};

// ============================================================================
// Statistics policies (copied conceptually; std-only)
// ============================================================================

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
                mUnregistrations.load(std::memory_order_relaxed)};
        }

        void incrementRegistrations() noexcept { mRegistrations.fetch_add(1, std::memory_order_relaxed); }
        void incrementRegistrationFailures() noexcept
        {
            mRegistrationFailures.fetch_add(1, std::memory_order_relaxed);
        }
        void incrementResolutions() noexcept { mResolutions.fetch_add(1, std::memory_order_relaxed); }
        void incrementResolutionFailures() noexcept
        {
            mResolutionFailures.fetch_add(1, std::memory_order_relaxed);
        }
        void incrementCreations() noexcept { mCreations.fetch_add(1, std::memory_order_relaxed); }
        void incrementCreationFailures() noexcept { mCreationFailures.fetch_add(1, std::memory_order_relaxed); }
        void incrementUnregistrations() noexcept { mUnregistrations.fetch_add(1, std::memory_order_relaxed); }
        void incrementUnregistrations(size_t count) noexcept
        {
            mUnregistrations.fetch_add(count, std::memory_order_relaxed);
        }
    };
};

// ============================================================================
// Registration policies
// ============================================================================

struct ServicePreventOverwritePolicy
{
    template <typename Registry, typename Key, typename Value, typename Stats>
    static bool insert(Registry& registry, const Key& key, Value&& value, Stats& stats)
    {
        auto [ptr, inserted] = registry.insert(key, std::forward<Value>(value));
        if (inserted)
        {
            stats.incrementRegistrations();
            (void)ptr;
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
// Resolve cache policies
// ============================================================================

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
        std::uint8_t mruIndex = 0;
    };

    static Cache& cache() noexcept
    {
        thread_local Cache c{};
        return c;
    }

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

// ============================================================================
// Stable map wrapper (std-only)
// ============================================================================

template <typename Key, typename Value, typename Hash, typename Eq>
class StableUnorderedMap
{
public:
    using key_type = Key;

    [[nodiscard]] std::size_t size() const noexcept { return mMap.size(); }

    void clear() { mMap.clear(); }

    template <typename K>
    [[nodiscard]] Value* find(const K& key) noexcept
    {
        auto it = mMap.find(key);
        return (it == mMap.end()) ? nullptr : it->second.get();
    }

    template <typename K>
    [[nodiscard]] const Value* find(const K& key) const noexcept
    {
        auto it = mMap.find(key);
        return (it == mMap.end()) ? nullptr : it->second.get();
    }

    template <typename K>
    [[nodiscard]] bool erase(const K& key)
    {
        auto it = mMap.find(key);
        if (it == mMap.end())
        {
            return false;
        }
        mMap.erase(it);
        return true;
    }

    // Insert if absent. Returns (ptr, inserted).
    template <typename V>
    [[nodiscard]] std::pair<Value*, bool> insert(const Key& key, V&& value)
    {
        auto it = mMap.find(key);
        if (it != mMap.end())
        {
            return {it->second.get(), false};
        }
        auto [newIt, inserted] = mMap.emplace(key, std::make_unique<Value>(std::forward<V>(value)));
        return {newIt->second.get(), inserted};
    }

    template <typename V>
    void insert_or_assign(const Key& key, V&& value)
    {
        auto it = mMap.find(key);
        if (it != mMap.end())
        {
            *(it->second) = std::forward<V>(value);
            return;
        }
        (void)mMap.emplace(key, std::make_unique<Value>(std::forward<V>(value)));
    }

private:
    std::unordered_map<Key, std::unique_ptr<Value>, Hash, Eq> mMap;
};

// ============================================================================
// Default type-id policy (same idea as FAT-P original)
// ============================================================================

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

// ============================================================================
// ServiceLocator (std-only)
// ============================================================================

template <typename ConcurrencyPolicy = SingleThreadedPolicy,
          typename RegistrationPolicy = ServicePreventOverwritePolicy,
          typename StatisticsPolicy = NoServiceLocatorStatisticsPolicy,
          typename TypeKeyPolicy = detail::DefaultServiceTypeKeyPolicy,
          typename ResolveCachePolicy = NoServiceLocatorResolveCachePolicy>
class ServiceLocator : private ConcurrencyPolicy
{
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
        using is_transparent = void;

        [[nodiscard]] std::size_t operator()(const void* p) const noexcept
        {
            const std::uintptr_t x = reinterpret_cast<std::uintptr_t>(p);

            if constexpr (sizeof(std::size_t) >= 8)
            {
                std::uint64_t z = static_cast<std::uint64_t>(x);
                z = (z ^ (z >> 30U)) * 0xBF58476D1CE4E5B9ULL;
                z = (z ^ (z >> 27U)) * 0x94D049BB133111EBULL;
                z = z ^ (z >> 31U);
                return static_cast<std::size_t>(z);
            }
            else
            {
                std::uint32_t h = static_cast<std::uint32_t>(x);
                h ^= h >> 16U;
                h *= 0x85ebca6bU;
                h ^= h >> 13U;
                h *= 0xc2b2ae35U;
                h ^= h >> 16U;
                return static_cast<std::size_t>(h);
            }
        }
    };

    struct ServiceKeyHash
    {
        using is_transparent = void;

        [[nodiscard]] std::size_t operator()(const ServiceKey& key) const noexcept
        {
            return hashImpl(key.mTypeId, key.mName);
        }

        [[nodiscard]] std::size_t operator()(const ServiceKeyView& key) const noexcept
        {
            return hashImpl(key.mTypeId, key.mName);
        }

    private:
        [[nodiscard]] static std::size_t hashImpl(const void* typeId, std::string_view name) noexcept
        {
            std::size_t h = TypeIdHash{}(typeId);
            const std::size_t hName = std::hash<std::string_view>{}(name);

            constexpr std::size_t kMagic = (sizeof(std::size_t) >= 8)
                ? static_cast<std::size_t>(0x9e3779b97f4a7c15ULL)
                : static_cast<std::size_t>(0x9e3779b9U);

            h ^= (hName + kMagic + (h << 6U) + (h >> 2U));

            if constexpr (sizeof(std::size_t) >= 8)
            {
                h ^= h >> 33U;
                h *= 0xff51afd7ed558ccdULL;
                h ^= h >> 33U;
                h *= 0xc4ceb9fe1a85ec53ULL;
                h ^= h >> 33U;
            }
            else
            {
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
        using is_transparent = void;

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

            ~Guard() { mGate.exit(); }

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
            assert(mDepth > 0);
            assert(mOwnerThread == std::this_thread::get_id());

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
        std::size_t mDepth = 0;
    };

    struct RootState
    {
        std::atomic<std::uint64_t> mRegistryEpoch{0};
        SingletonFactoryGate mSingletonFactoryGate{};
    };

    using UnnamedRegistry = StableUnorderedMap<const void*, ServiceEntry, TypeIdHash, std::equal_to<>>;
    using NamedRegistry = StableUnorderedMap<ServiceKey, ServiceEntry, ServiceKeyHash, ServiceKeyEq>;

public:
    static_assert(std::is_same_v<ConcurrencyPolicy, SingleThreadedPolicy> || StatisticsPolicy::kThreadSafe,
                  "Thread-safe ConcurrencyPolicy requires a thread-safe StatisticsPolicy.");

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

    [[nodiscard]] Scope makeScope() const& { return Scope(*this); }
    Scope makeScope() const&& = delete;

    [[nodiscard]] ServiceLocator makeChild() const& { return ServiceLocator(this); }
    ServiceLocator makeChild() const&& = delete;

    // --------------------------------------------------------------------
    // Registration
    // --------------------------------------------------------------------

    template <typename T>
    [[nodiscard]] RegisterResult registerInstance(T& instance, std::string_view name = {})
    {
        assertRegistrableServiceType<T>();
        const void* typeId = TypeKeyPolicy::template typeId<T>();

        ServiceEntry entry;
        entry.mKind = ServiceEntryKind::Instance;
        entry.mLifetime = ServiceLifetime::Singleton;
        entry.mInstance = std::addressof(instance);

        [[maybe_unused]] auto lock = writeLock();

        if (name.empty())
        {
            if (!RegistrationPolicy::insert(mUnnamedRegistry, typeId, std::move(entry), mStats))
            {
                return unexpected{ServiceErrorInfo{ServiceError::ServiceAlreadyExists,
                                                   "Instance registration rejected",
                                                   std::string(name)}};
            }
        }
        else
        {
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

        [[maybe_unused]] auto lock = writeLock();

        if (name.empty())
        {
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
            return detail::invokeFactoryToShared<T>(factory);
        };

        [[maybe_unused]] auto lock = writeLock();

        if (name.empty())
        {
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
        [[maybe_unused]] auto lock = writeLock();

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
        [[maybe_unused]] auto lock = writeLock();
        const std::size_t count = mUnnamedRegistry.size() + mNamedRegistry.size();
        mUnnamedRegistry.clear();
        mNamedRegistry.clear();
        if (count > 0)
        {
            bumpRegistryEpoch();
            mStats.incrementUnregistrations(count);
        }
    }

    [[nodiscard]] std::size_t size() const
    {
        [[maybe_unused]] auto lock = readLock();
        return mUnnamedRegistry.size() + mNamedRegistry.size();
    }

    [[nodiscard]] bool empty() const { return size() == 0; }

    template <typename T>
    [[nodiscard]] bool isRegistered(std::string_view name = {}) const
    {
        const void* typeId = TypeKeyPolicy::template typeId<T>();

        {
            [[maybe_unused]] auto lock = readLock();
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

        return mParent != nullptr && mParent->template isRegistered<T>(name);
    }

    [[nodiscard]] StatsType& stats() noexcept { return mStats; }
    [[nodiscard]] const StatsType& stats() const noexcept { return mStats; }

    // --------------------------------------------------------------------
    // Resolution (hot path)
    // --------------------------------------------------------------------

    template <typename T>
    [[nodiscard]] T* tryResolve(std::string_view name = {}) const
    {
        const void* typeId = TypeKeyPolicy::template typeId<T>();

        // Optional fast path: cache check without taking a lock.
        if constexpr (ResolveCachePolicy::kEnabled)
        {
            if (name.empty())
            {
                const std::uint64_t epoch = registryEpoch();
                if (auto* cached = ResolveCachePolicy::template tryGet<T>(*this, typeId, epoch))
                {
                    mStats.incrementResolutions();
                    return cached;
                }
            }
        }

        bool needsFactoryCreation = false;

        {
            [[maybe_unused]] auto lock = readLock();

            ServiceEntry* entry = nullptr;
            const bool cacheable = name.empty();
            std::uint64_t epoch = 0;
            if (cacheable)
            {
                epoch = registryEpoch();
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

                    needsFactoryCreation = true;
                }
                else if (!needsFactoryCreation)
                {
                    mStats.incrementResolutionFailures();
                    return nullptr;
                }
            }
        }

        if (needsFactoryCreation)
        {
            auto cached = resolveOrCreateSingleton(typeId, name);
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

        if (mParent != nullptr)
        {
            return mParent->template tryResolve<T>(name);
        }

        mStats.incrementResolutionFailures();
        return nullptr;
    }

    template <typename T>
    [[nodiscard]] Expected<std::reference_wrapper<T>, ServiceErrorInfo> resolveExpected(std::string_view name = {}) const
    {
        auto* ptr = tryResolve<T>(name);
        if (ptr == nullptr)
        {
            return unexpected{ServiceErrorInfo{ServiceError::ServiceNotFound,
                                               "No matching service registration",
                                               std::string(name)}};
        }
        return std::ref(*ptr);
    }

    template <typename T>
    [[nodiscard]] T& resolve(std::string_view name = {}) const
    {
        auto expected = resolveExpected<T>(name);
        if (!expected.has_value())
        {
            throw std::runtime_error("ServiceLocator::resolve failed: " + expected.error().fullMessage());
        }
        return expected.value().get();
    }

    template <typename T>
    [[nodiscard]] Expected<std::shared_ptr<T>, ServiceErrorInfo> createExpected(std::string_view name = {}) const
    {
        const void* typeId = TypeKeyPolicy::template typeId<T>();

        bool needsSingletonCreation = false;
        bool needsTransientCreation = false;
        std::function<std::shared_ptr<void>()> factoryCopy;

        {
            [[maybe_unused]] auto lock = readLock();

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
                    if (entry->mShared)
                    {
                        mStats.incrementCreations();
                        return std::static_pointer_cast<T>(entry->mShared);
                    }
                    needsSingletonCreation = true;
                }
                else
                {
                    factoryCopy = entry->mFactory;
                    needsTransientCreation = true;
                }
            }
        }

        if (needsSingletonCreation)
        {
            auto cached = resolveOrCreateSingleton(typeId, name);
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
                return unexpected{ServiceErrorInfo{ServiceError::FactoryThrew,
                                                   "Factory threw: non-std exception",
                                                   std::string(name)}};
            }
        }

        if (mParent != nullptr)
        {
            return mParent->template createExpected<T>(name);
        }

        mStats.incrementCreationFailures();
        return unexpected{ServiceErrorInfo{ServiceError::ServiceNotFound,
                                           "No matching service registration",
                                           std::string(name)}};
    }

    template <typename T>
    [[nodiscard]] std::shared_ptr<T> create(std::string_view name = {}) const
    {
        auto expected = createExpected<T>(name);
        if (!expected.has_value())
        {
            throw std::runtime_error("ServiceLocator::create failed: " + expected.error().fullMessage());
        }
        return expected.value();
    }

    template <typename T>
    [[nodiscard]] Expected<std::shared_ptr<T>, ServiceErrorInfo> resolveSharedExpected(std::string_view name = {}) const
    {
        const void* typeId = TypeKeyPolicy::template typeId<T>();

        bool needsFactoryCreation = false;

        {
            [[maybe_unused]] auto lock = readLock();

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

        if (needsFactoryCreation)
        {
            auto cached = resolveOrCreateSingleton(typeId, name);
            if (!cached.has_value())
            {
                mStats.incrementResolutionFailures();
                return unexpected{cached.error()};
            }
            mStats.incrementResolutions();
            return std::static_pointer_cast<T>(cached.value());
        }

        if (mParent != nullptr)
        {
            return mParent->template resolveSharedExpected<T>(name);
        }

        mStats.incrementResolutionFailures();
        return unexpected{ServiceErrorInfo{ServiceError::ServiceNotFound,
                                           "No matching service registration",
                                           std::string(name)}};
    }

    template <typename T>
    [[nodiscard]] std::shared_ptr<T> tryResolveShared(std::string_view name = {}) const
    {
        auto result = resolveSharedExpected<T>(name);
        return result.has_value() ? result.value() : std::shared_ptr<T>{};
    }

private:
    const ServiceLocator* mParent = nullptr;
    std::shared_ptr<RootState> mRootState{};
    mutable UnnamedRegistry mUnnamedRegistry{};
    mutable NamedRegistry mNamedRegistry{};
    mutable StatsType mStats{};

    [[nodiscard]] auto readLock() const { return static_cast<const ConcurrencyPolicy&>(*this).lock_shared(); }
    [[nodiscard]] auto writeLock() { return static_cast<ConcurrencyPolicy&>(*this).lock(); }

    [[nodiscard]] auto writeLock() const
    {
        auto& policy = const_cast<ConcurrencyPolicy&>(static_cast<const ConcurrencyPolicy&>(*this));
        return policy.lock();
    }

    [[nodiscard]] std::uint64_t registryEpoch() const noexcept
    {
        return mRootState->mRegistryEpoch.load(std::memory_order_relaxed);
    }

    void bumpRegistryEpoch() noexcept { mRootState->mRegistryEpoch.fetch_add(1, std::memory_order_relaxed); }

    template <typename T>
    static void assertRegistrableServiceType()
    {
        using U = std::remove_reference_t<T>;
        static_assert(!std::is_const_v<U> && !std::is_volatile_v<U>,
                      "ServiceLocator does not permit registration of cv-qualified service types.");
    }

    [[nodiscard]] Expected<std::shared_ptr<void>, ServiceErrorInfo> resolveOrCreateSingleton(const void* typeId,
                                                                                             std::string_view name) const
    {
        std::shared_ptr<typename ServiceEntry::SingletonState> state;
        std::function<std::shared_ptr<void>()> factoryCopy;

        {
            [[maybe_unused]] auto lock = writeLock();

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

            if (entry->mShared)
            {
                return entry->mShared;
            }

            if (!entry->mSingletonState)
            {
                entry->mSingletonState = std::make_shared<typename ServiceEntry::SingletonState>();
            }

            state = entry->mSingletonState;
            factoryCopy = entry->mFactory;
        }

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
                                                   "Singleton factory attempted to resolve itself (circular dependency)",
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

        assert(mRootState != nullptr);
        typename SingletonFactoryGate::Guard gate(mRootState->mSingletonFactoryGate);

        stateLock.lock();

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
                                                   "Singleton factory attempted to resolve itself (circular dependency)",
                                                   std::string(name)}};
            }

            state->mCv.wait(stateLock, [&state]() { return !state->mCreating; });

            if (state->mValue)
            {
                stateLock.unlock();
                return publishSingleton(typeId, name, state);
            }
        }

        state->mCreating = true;
        state->mCreatingThread = thisThread;
        stateLock.unlock();

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
    publishSingleton(const void* typeId,
                     std::string_view name,
                     const std::shared_ptr<typename ServiceEntry::SingletonState>& state) const
    {
        [[maybe_unused]] auto lock = writeLock();

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

        if (entry->mKind != ServiceEntryKind::Factory || entry->mLifetime != ServiceLifetime::Singleton ||
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
// Scope + Registration RAII helpers
// ============================================================================

template <typename ConcurrencyPolicy,
          typename RegistrationPolicy,
          typename StatisticsPolicy,
          typename TypeKeyPolicy,
          typename ResolveCachePolicy>
class ServiceLocator<ConcurrencyPolicy, RegistrationPolicy, StatisticsPolicy, TypeKeyPolicy, ResolveCachePolicy>::Scope
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

    [[nodiscard]] ServiceLocator& locator() noexcept { return mChild; }
    [[nodiscard]] const ServiceLocator& locator() const noexcept { return mChild; }

private:
    ServiceLocator mChild;
};

template <typename ConcurrencyPolicy,
          typename RegistrationPolicy,
          typename StatisticsPolicy,
          typename TypeKeyPolicy,
          typename ResolveCachePolicy>
class ServiceLocator<ConcurrencyPolicy, RegistrationPolicy, StatisticsPolicy, TypeKeyPolicy, ResolveCachePolicy>::Registration
{
public:
    Registration() = default;

    template <typename T>
    static Expected<Registration, ServiceErrorInfo>
    registerInstanceExpected(ServiceLocator& locator, T& instance, std::string_view name = {})
    {
        auto result = locator.template registerInstance<T>(instance, name);
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
        auto result = locator.template registerShared<T>(std::move(instance), name);
        if (!result.has_value())
        {
            return unexpected{result.error()};
        }
        return Registration(locator, TypeKeyPolicy::template typeId<T>(), std::string(name));
    }

    template <typename T, typename Factory>
    static Expected<Registration, ServiceErrorInfo>
    registerFactoryExpected(ServiceLocator& locator, Factory&& factory, ServiceLifetime lifetime, std::string_view name = {})
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

    ~Registration() { reset(); }

    void reset()
    {
        if (mLocator != nullptr && mTypeId != nullptr)
        {
            (void)mLocator->template unregisterByTypeId(mTypeId, mName);
        }
        mLocator = nullptr;
        mTypeId = nullptr;
        mName.clear();
    }

    [[nodiscard]] explicit operator bool() const noexcept { return mLocator != nullptr && mTypeId != nullptr; }

private:
    Registration(ServiceLocator& locator, const void* typeId, std::string name)
        : mLocator(&locator)
        , mTypeId(typeId)
        , mName(std::move(name))
    {
    }

    // A private helper because unregister<T>() needs a template type.
    [[nodiscard]] bool unregisterByTypeId(const void* typeId, std::string_view name)
    {
        auto lock = mLocator->writeLock();

        bool removed = false;
        if (name.empty())
        {
            removed = mLocator->mUnnamedRegistry.erase(typeId);
        }
        else
        {
            typename ServiceLocator::ServiceKeyView key{typeId, name};
            removed = mLocator->mNamedRegistry.erase(key);
        }

        if (removed)
        {
            mLocator->bumpRegistryEpoch();
            mLocator->mStats.incrementUnregistrations();
        }
        return removed;
    }

    ServiceLocator* mLocator = nullptr;
    const void* mTypeId = nullptr;
    std::string mName{};
};

// Convenience aliases mirroring the original header
using DefaultServiceLocator = ServiceLocator<SingleThreadedPolicy>;
using ThreadSafeServiceLocator = ServiceLocator<SharedMutexPolicy>;

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

} // namespace fat_p::std_only
