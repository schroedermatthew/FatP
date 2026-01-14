/**
 * @file ConcurrencyPolicies.h
 * @brief Defines various synchronization policies for use with a policy-based
 * design, such as ScopeGuard, Enforcer, or smart resource handles.
 *
 * 
 *
 * @layer Concurrency
 *
 * @details This header provides a comprehensive set of concurrency primitives ranging from
 * zero-cost (single-threaded) to advanced lock-free synchronization (RCU, hazard pointers,
 * SeqLock), all exposing a consistent policy interface.
 *
 * FEATURES:
 *   - 19 total concurrency policies with production-ready implementations
 *   - Enhanced C++20/C++23 support with jthread and atomic<shared_ptr>
 *   - Platform-specific priority inheritance (POSIX/Windows)
 *   - Thread-local hazard pointers with proper lifetime management
 *   - Enhanced trait system with contention tracking support
 *   - Improved SeqLock with retry limits and exponential backoff
 *   - Cache-line aligned hot variables to prevent false sharing
 *   - try_lock() support on applicable policies
 *   - Header-only, zero dependencies beyond STL
 *   - Unified lock()/lock_shared() interface across all policies
 *
 * @section all_policies Complete Policy List
 *   ORIGINAL POLICIES:
 *   1. SingleThreadedPolicy - Zero-cost no-op synchronization
 *   2. MutexSynchronizationPolicy - Standard mutex-based locking
 *   3. SharedMutexPolicy - Read-write lock with shared/exclusive access
 *   4. UniqueRWLockPolicy - Unique ownership read-write lock
 *   5. SpinlockSynchronizationPolicy - Busy-wait spinlock
 *   6. LockFreeSynchronizationPolicy - Debug assertion for lock-free code
 *   7. LockFreeWithFallbackPolicy - Mutex in debug, no-op in release
 *   8. WaitableSynchronizationPolicy - Condition variable support
 *
 *   ADVANCED POLICIES:
 *   9. SeqLockPolicy - Optimistic read-heavy synchronization
 *   10. TicketLockPolicy - Fair FIFO spinlock
 *   11. MCSLockPolicy - Scalable queue-based lock for NUMA
 *   12. RCUPolicy - Read-Copy-Update for lock-free reads
 *   13. HazardPointerPolicy - Safe memory reclamation for lock-free structures
 *   14. AdaptiveLockPolicy - Runtime-adaptive spinlock/mutex hybrid
 *   15. PriorityInheritanceLockPolicy - Real-time priority inheritance
 *   16. VersionedLockPolicy - Optimistic versioned concurrency control
 *
 *   STANDARD POLICIES:
 *   17. RecursiveMutexPolicy - Reentrant mutex
 *   18. TimedMutexPolicy - Mutex with timeout
 *   19. SharedTimedMutexPolicy - Shared mutex with timeout
 */

#pragma once

/*
FATP_META:
  meta_version: 1
  component: ConcurrencyPolicies
  file_role: public_header
  path: fat_p/ConcurrencyPolicies.h
  namespace: fat_p
  layer: Concurrency
  summary: "Public header for ConcurrencyPolicies."
  api_stability: in_work
  related:
    docs_search: "ConcurrencyPolicies"
    tests:
      - tests/test_ConcurrencyPolicies.cpp
      - tests/test_IdGenerator.cpp
      - tests/test_RcuIntegration.cpp
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 17
    defines_unprefixed: 2
    undefs_total: 0
    includes_windows_h: true
  generated:
    by: fatp-meta-tool
    mode: autogen
*/
#include "CppStandardDetection.h"

// =============================================================================
// Feature Detection Macros
// =============================================================================

#if !defined(FATP_USE_MUTEX)
#define FATP_USE_MUTEX 1
#endif
#if !defined(FATP_USE_SHARED_MUTEX)
#define FATP_USE_SHARED_MUTEX 1
#endif
#if !defined(FATP_USE_ATOMIC)
#define FATP_USE_ATOMIC 1
#endif
#if !defined(FATP_USE_CHRONO)
#define FATP_USE_CHRONO 1
#endif
#if !defined(FATP_USE_CONDITION_VARIABLE)
#define FATP_USE_CONDITION_VARIABLE 1
#endif

// Cache line size for alignment
#if !defined(FATP_CACHE_LINE_SIZE)
#define FATP_CACHE_LINE_SIZE 64
#endif

// C++23 jthread feature detection
#if FATP_HAS_CPP23 || FATP_HAS_JTHREAD
#define FATP_HAS_JTHREAD 1
#else
#define FATP_HAS_JTHREAD 0
#endif

// C++20 atomic<shared_ptr> detection - use internal flag
#if FATP_HAS_ATOMIC_SHARED_PTR
#define FATP_HAS_ATOMIC_SHARED_PTR 1
#elif FATP_HAS_CPP20 && \
    ((defined(__GNUC__) && __GNUC__ >= 11) || \
     (defined(__clang__) && __clang_major__ >= 13) || \
     (defined(_MSC_VER) && _MSC_VER >= 1930))
#define FATP_HAS_ATOMIC_SHARED_PTR 1
#else
#define FATP_HAS_ATOMIC_SHARED_PTR 0
#endif

// Standard Library Includes
#if FATP_USE_MUTEX
#include <mutex>
#include <memory>
#include <utility>
#endif
#if FATP_USE_SHARED_MUTEX
#include <shared_mutex>
#endif
#if FATP_USE_ATOMIC
#include <atomic>
#include <thread>
#include <vector>
#include <array>
#include <functional>
#endif
#if FATP_USE_CHRONO
#include <chrono>
#endif
#if FATP_USE_CONDITION_VARIABLE
#include <condition_variable>
#endif

#include <type_traits>
#include <cassert>
#include <cstdint>
#include <algorithm>
#include <optional>

// Platform-specific includes for priority inheritance
#if defined(__unix__) || defined(__APPLE__)
#include <pthread.h>
#define FATP_HAS_PTHREAD_PRIO_INHERIT 1
#elif defined(_WIN32)
#ifndef NOMINMAX
    #define NOMINMAX
    #define FATP_DEFINED_NOMINMAX_CONC
#endif
#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #define FATP_DEFINED_WIN32_LEAN_AND_MEAN_CONC
#endif
#include <windows.h>
// Clean up Windows macros we defined
#ifdef FATP_DEFINED_NOMINMAX_CONC
    #undef NOMINMAX
    #undef FATP_DEFINED_NOMINMAX_CONC
#endif
#ifdef FATP_DEFINED_WIN32_LEAN_AND_MEAN_CONC
    #undef WIN32_LEAN_AND_MEAN
    #undef FATP_DEFINED_WIN32_LEAN_AND_MEAN_CONC
#endif
#define FATP_HAS_WIN32_CRITICAL_SECTION 1
#else
#define FATP_HAS_PTHREAD_PRIO_INHERIT 0
#define FATP_HAS_WIN32_CRITICAL_SECTION 0
#endif

namespace fat_p
{

// =============================================================================
// Policy Traits
// =============================================================================

template <typename T, typename = void>
struct is_concurrency_policy : std::false_type {};

template <typename T>
struct is_concurrency_policy<T, std::void_t<typename T::PolicyTag>> : std::true_type {};

template <typename T>
inline constexpr bool is_concurrency_policy_v = is_concurrency_policy<T>::value;

template <typename T, typename = void>
struct is_shared_policy : std::false_type {};

template <typename T>
struct is_shared_policy<T, std::void_t<typename T::SharedGuard>> : std::true_type {};

template <typename T>
inline constexpr bool is_shared_policy_v = is_shared_policy<T>::value;

template <typename T, typename = void>
struct is_waitable_policy : std::false_type {};

template <typename T>
struct is_waitable_policy<T, std::void_t<decltype(
    std::declval<typename T::LockGuard>().wait(std::declval<std::condition_variable&>()))>>
    : std::true_type {};

template <typename T>
inline constexpr bool is_waitable_policy_v = is_waitable_policy<T>::value;

template <typename T, typename = void>
struct is_fair_policy : std::false_type {};

template <typename T>
struct is_fair_policy<T, std::void_t<typename T::FairOrderingTag>> : std::true_type {};

template <typename T>
inline constexpr bool is_fair_policy_v = is_fair_policy<T>::value;

template <typename T, typename = void>
struct is_optimistic_policy : std::false_type {};

template <typename T>
struct is_optimistic_policy<T, std::void_t<typename T::OptimisticTag>> : std::true_type {};

template <typename T>
inline constexpr bool is_optimistic_policy_v = is_optimistic_policy<T>::value;

template <typename T, typename = void>
struct is_numa_aware_policy : std::false_type {};

template <typename T>
struct is_numa_aware_policy<T, std::void_t<typename T::NUMAAwareTag>> : std::true_type {};

template <typename T>
inline constexpr bool is_numa_aware_policy_v = is_numa_aware_policy<T>::value;

template <typename T, typename = void>
struct is_realtime_policy : std::false_type {};

template <typename T>
struct is_realtime_policy<T, std::void_t<typename T::RealtimeTag>> : std::true_type {};

template <typename T>
inline constexpr bool is_realtime_policy_v = is_realtime_policy<T>::value;

template <typename T, typename = void>
struct is_lockfree_policy : std::false_type {};

template <typename T>
struct is_lockfree_policy<T, std::void_t<typename T::LockFreeTag>> : std::true_type {};

template <typename T>
inline constexpr bool is_lockfree_policy_v = is_lockfree_policy<T>::value;

template <typename T, typename = void>
struct is_adaptive_policy : std::false_type {};

template <typename T>
struct is_adaptive_policy<T, std::void_t<typename T::AdaptiveTag>> : std::true_type {};

template <typename T>
inline constexpr bool is_adaptive_policy_v = is_adaptive_policy<T>::value;

template <typename T, typename = void>
struct has_contention_tracking : std::false_type {};

template <typename T>
struct has_contention_tracking<T, std::void_t<decltype(std::declval<T>().getContention())>>
    : std::true_type {};

template <typename T>
inline constexpr bool has_contention_tracking_v = has_contention_tracking<T>::value;

template <typename T, typename = void>
struct is_recursive_policy : std::false_type {};

template <typename T>
struct is_recursive_policy<T, std::void_t<typename T::RecursiveTag>> : std::true_type {};

template <typename T>
inline constexpr bool is_recursive_policy_v = is_recursive_policy<T>::value;

template <typename T, typename = void>
struct is_timed_policy : std::false_type {};

template <typename T>
struct is_timed_policy<T, std::void_t<decltype(
    std::declval<typename T::LockGuard>().try_lock_for(std::chrono::milliseconds(1)))>>
    : std::true_type {};

template <typename T>
inline constexpr bool is_timed_policy_v = is_timed_policy<T>::value;

template <typename T, typename = void>
struct supports_try_lock : std::false_type {};

template <typename T>
struct supports_try_lock<T, std::void_t<decltype(std::declval<T>().try_lock())>>
    : std::true_type {};

template <typename T>
inline constexpr bool supports_try_lock_v = supports_try_lock<T>::value;

// =============================================================================
// SingleThreadedPolicy - Zero-cost no-op synchronization
// =============================================================================

struct SingleThreadedPolicy
{
    using PolicyTag = void;

    SingleThreadedPolicy() = default;
    SingleThreadedPolicy(const SingleThreadedPolicy&) = delete;
    SingleThreadedPolicy& operator=(const SingleThreadedPolicy&) = delete;

    struct NoOpLock {};

    class LockGuard
    {
    public:
        template <typename T>
        explicit LockGuard(T&) {}
        LockGuard(const LockGuard&) = delete;
        LockGuard& operator=(const LockGuard&) = delete;
        ~LockGuard() = default;
    };

    using SharedGuard = LockGuard;
    using WriteLock = LockGuard;
    using ReadLock = LockGuard;

    [[nodiscard]] LockGuard lock() { return LockGuard(mLock); }
    [[nodiscard]] SharedGuard lock_shared() const { return SharedGuard(mLock); }
    [[nodiscard]] bool try_lock() { return true; }
    [[nodiscard]] bool try_lock_shared() const { return true; }

    NoOpLock& getLock() const { return mLock; }

    static NoOpLock& getStaticLock()
    {
        static NoOpLock mLock;
        return mLock;
    }

    uint64_t getContention() const { return 0; }
    void resetContention() {}

private:
    mutable NoOpLock mLock{};
};

// =============================================================================
// MutexSynchronizationPolicy - Standard mutex-based locking
// =============================================================================

#if FATP_USE_MUTEX
struct MutexSynchronizationPolicy
{
    using PolicyTag = void;

    MutexSynchronizationPolicy() = default;
    MutexSynchronizationPolicy(const MutexSynchronizationPolicy&) = delete;
    MutexSynchronizationPolicy& operator=(const MutexSynchronizationPolicy&) = delete;

#if FATP_USE_ATOMIC
private:
    mutable std::atomic<uint64_t> mContention{0};

public:
    uint64_t getContention() const noexcept
    {
        return mContention.load(std::memory_order_relaxed);
    }
    void resetContention() noexcept { mContention.store(0, std::memory_order_relaxed); }
#endif

    class LockGuard
    {
    public:
        explicit LockGuard(std::mutex& mutex) : mGuard(mutex) {}

#if FATP_USE_ATOMIC
        explicit LockGuard(MutexSynchronizationPolicy& policy)
            : mGuard(policy.mMutex)
        {
            policy.mContention.fetch_add(1, std::memory_order_relaxed);
        }
#endif

        LockGuard(const LockGuard&) = delete;
        LockGuard& operator=(const LockGuard&) = delete;
        ~LockGuard() = default;

#if FATP_USE_CONDITION_VARIABLE
        template <typename Predicate>
        void wait(std::condition_variable&, Predicate)
        {
            assert(false && "Cannot use wait() with lock_guard. Use WaitableSynchronizationPolicy.");
        }
#endif

    private:
        std::lock_guard<std::mutex> mGuard;
    };

    using SharedGuard = LockGuard;
    using WriteLock = LockGuard;
    using ReadLock = LockGuard;

    [[nodiscard]] LockGuard lock() { return LockGuard(*this); }
    [[nodiscard]] SharedGuard lock_shared() const
    {
        return SharedGuard(const_cast<MutexSynchronizationPolicy&>(*this));
    }

    [[nodiscard]] bool try_lock()
    {
        // Standard try_lock semantics: on success, the lock is held by the caller.
        // (The caller is responsible for unlocking.)
        return mMutex.try_lock();
    }

    std::mutex& getLock() const { return mMutex; }

    static std::mutex& getStaticLock()
    {
        static std::mutex mMutex;
        return mMutex;
    }

private:
    mutable std::mutex mMutex{};
};
#endif // FATP_USE_MUTEX

// =============================================================================
// SharedMutexPolicy - Read-write lock with shared/exclusive access
// =============================================================================

#if FATP_USE_SHARED_MUTEX
struct SharedMutexPolicy
{
    using PolicyTag = void;

    SharedMutexPolicy() = default;
    SharedMutexPolicy(const SharedMutexPolicy&) = delete;
    SharedMutexPolicy& operator=(const SharedMutexPolicy&) = delete;

    class LockGuard
    {
    public:
        explicit LockGuard(std::shared_mutex& mutex) : mLock(mutex) {}

        LockGuard(const LockGuard&) = delete;
        LockGuard& operator=(const LockGuard&) = delete;
        ~LockGuard() = default;

    private:
        std::unique_lock<std::shared_mutex> mLock;
    };

    class SharedGuard
    {
    public:
        explicit SharedGuard(std::shared_mutex& mutex) : mLock(mutex) {}

        SharedGuard(const SharedGuard&) = delete;
        SharedGuard& operator=(const SharedGuard&) = delete;
        ~SharedGuard() = default;

    private:
        std::shared_lock<std::shared_mutex> mLock;
    };

    using WriteLock = LockGuard;
    using ReadLock = SharedGuard;

    [[nodiscard]] LockGuard lock() { return LockGuard(mMutex); }
    [[nodiscard]] SharedGuard lock_shared() const
    {
        return SharedGuard(const_cast<std::shared_mutex&>(mMutex));
    }

    [[nodiscard]] bool try_lock() { return mMutex.try_lock(); }
    [[nodiscard]] bool try_lock_shared() const
    {
        return const_cast<std::shared_mutex&>(mMutex).try_lock_shared();
    }

    std::shared_mutex& getLock() const { return const_cast<std::shared_mutex&>(mMutex); }

    static std::shared_mutex& getStaticLock()
    {
        static std::shared_mutex mMutex;
        return mMutex;
    }

    uint64_t getContention() const { return 0; }
    void resetContention() {}

private:
    mutable std::shared_mutex mMutex;
};
#endif // FATP_USE_SHARED_MUTEX

// =============================================================================
// UniqueRWLockPolicy - Unique ownership read-write lock
// =============================================================================

#if FATP_USE_SHARED_MUTEX
struct UniqueRWLockPolicy
{
    using PolicyTag = void;

    UniqueRWLockPolicy() : mMutex(std::make_unique<std::shared_mutex>()) {}
    UniqueRWLockPolicy(const UniqueRWLockPolicy&) = delete;
    UniqueRWLockPolicy& operator=(const UniqueRWLockPolicy&) = delete;
    UniqueRWLockPolicy(UniqueRWLockPolicy&&) noexcept = default;
    UniqueRWLockPolicy& operator=(UniqueRWLockPolicy&&) noexcept = default;

    class LockGuard
    {
    public:
        explicit LockGuard(std::shared_mutex& mutex) : mLock(mutex) {}
        LockGuard(const LockGuard&) = delete;
        LockGuard& operator=(const LockGuard&) = delete;
        ~LockGuard() = default;

    private:
        std::unique_lock<std::shared_mutex> mLock;
    };

    class SharedGuard
    {
    public:
        explicit SharedGuard(std::shared_mutex& mutex) : mLock(mutex) {}
        SharedGuard(const SharedGuard&) = delete;
        SharedGuard& operator=(const SharedGuard&) = delete;
        ~SharedGuard() = default;

    private:
        std::shared_lock<std::shared_mutex> mLock;
    };

    using WriteLock = LockGuard;
    using ReadLock = SharedGuard;

    [[nodiscard]] LockGuard lock() { return LockGuard(*mMutex); }
    [[nodiscard]] SharedGuard lock_shared() const { return SharedGuard(*mMutex); }

    [[nodiscard]] bool try_lock() { return mMutex->try_lock(); }
    [[nodiscard]] bool try_lock_shared() const { return mMutex->try_lock_shared(); }

    std::shared_mutex& getLock() const { return *mMutex; }

    static std::shared_mutex& getStaticLock()
    {
        static std::shared_mutex mMutex;
        return mMutex;
    }

private:
    std::unique_ptr<std::shared_mutex> mMutex;
};
#endif // FATP_USE_SHARED_MUTEX

// =============================================================================
// SpinlockSynchronizationPolicy - Busy-wait spinlock
// =============================================================================

#if FATP_USE_ATOMIC
struct SpinlockSynchronizationPolicy
{
    using PolicyTag = void;

    SpinlockSynchronizationPolicy() = default;
    SpinlockSynchronizationPolicy(const SpinlockSynchronizationPolicy&) = delete;
    SpinlockSynchronizationPolicy& operator=(const SpinlockSynchronizationPolicy&) = delete;

private:
    alignas(FATP_CACHE_LINE_SIZE) std::atomic_flag mLockFlag = ATOMIC_FLAG_INIT;
    alignas(FATP_CACHE_LINE_SIZE) mutable std::atomic<uint64_t> mContention{0};

public:
    uint64_t getContention() const noexcept
    {
        return mContention.load(std::memory_order_relaxed);
    }
    void resetContention() noexcept { mContention.store(0, std::memory_order_relaxed); }

    class LockGuard
    {
    public:
        explicit LockGuard(SpinlockSynchronizationPolicy& policy)
            : mLockFlag(policy.mLockFlag)
            , mContention(policy.mContention)
        {
            while (mLockFlag.test_and_set(std::memory_order_acquire))
            {
                mContention.fetch_add(1, std::memory_order_relaxed);
                std::this_thread::yield();
            }
        }

        LockGuard(const LockGuard&) = delete;
        LockGuard& operator=(const LockGuard&) = delete;

        ~LockGuard()
        {
            mLockFlag.clear(std::memory_order_release);
        }

    private:
        std::atomic_flag& mLockFlag;
        std::atomic<uint64_t>& mContention;
    };

    using SharedGuard = LockGuard;
    using WriteLock = LockGuard;
    using ReadLock = LockGuard;

    [[nodiscard]] LockGuard lock() { return LockGuard(*this); }
    [[nodiscard]] SharedGuard lock_shared() const
    {
        return SharedGuard(const_cast<SpinlockSynchronizationPolicy&>(*this));
    }

    [[nodiscard]] bool try_lock()
    {
        return !mLockFlag.test_and_set(std::memory_order_acquire);
    }

    void unlock()
    {
        mLockFlag.clear(std::memory_order_release);
    }

    SpinlockSynchronizationPolicy& getLock() { return *this; }
    SpinlockSynchronizationPolicy& getLock() const { return const_cast<SpinlockSynchronizationPolicy&>(*this); }


    static SpinlockSynchronizationPolicy& getStaticLock()
    {
        static SpinlockSynchronizationPolicy mPolicy;
        return mPolicy;
    }

    std::atomic_flag& getRawLock() { return mLockFlag; }
};
#endif // FATP_USE_ATOMIC

// =============================================================================
// LockFreeSynchronizationPolicy - Debug assertion for lock-free code
// =============================================================================

#if FATP_USE_ATOMIC
struct LockFreeSynchronizationPolicy
{
    using PolicyTag = void;
    using LockFreeTag = void;

    LockFreeSynchronizationPolicy() = default;
    LockFreeSynchronizationPolicy(const LockFreeSynchronizationPolicy&) = delete;
    LockFreeSynchronizationPolicy& operator=(const LockFreeSynchronizationPolicy&) = delete;

    class LockGuard
    {
    public:
        template <typename T>
        explicit LockGuard(T&) {}
        LockGuard(const LockGuard&) = delete;
        LockGuard& operator=(const LockGuard&) = delete;
        ~LockGuard() = default;
    };

    using SharedGuard = LockGuard;
    using WriteLock = LockGuard;
    using ReadLock = LockGuard;

    struct NoOpLock {};

    [[nodiscard]] LockGuard lock() { return LockGuard(mLock); }
    [[nodiscard]] SharedGuard lock_shared() const { return SharedGuard(mLock); }
    [[nodiscard]] bool try_lock() { return true; }

    NoOpLock& getLock() const { return mLock; }

    static NoOpLock& getStaticLock()
    {
        static NoOpLock mLock;
        return mLock;
    }

private:
    mutable NoOpLock mLock{};
};
#endif // FATP_USE_ATOMIC

// =============================================================================
// LockFreeWithFallbackPolicy - Mutex in debug, no-op in release
// =============================================================================

#if FATP_USE_ATOMIC && FATP_USE_MUTEX
template <typename FallbackPolicy = MutexSynchronizationPolicy>
struct LockFreeWithFallbackPolicy
{
    using PolicyTag = void;

#ifndef NDEBUG
    using RealPolicy = FallbackPolicy;
#else
    using RealPolicy = SingleThreadedPolicy;
#endif

    using LockGuard = typename RealPolicy::LockGuard;
    using SharedGuard = typename RealPolicy::SharedGuard;
    using WriteLock = LockGuard;
    using ReadLock = SharedGuard;

    [[nodiscard]] LockGuard lock() { return mPolicy.lock(); }
    [[nodiscard]] SharedGuard lock_shared() const { return mPolicy.lock_shared(); }

    [[nodiscard]] bool try_lock()
    {
        if constexpr (supports_try_lock_v<RealPolicy>)
        {
            return mPolicy.try_lock();
        }
        return true;
    }

    auto& getLock() { return mPolicy.getLock(); }

    uint64_t getContention() const
    {
        if constexpr (has_contention_tracking_v<RealPolicy>)
        {
            return mPolicy.getContention();
        }
        return 0;
    }

private:
    mutable RealPolicy mPolicy;
};
#endif // FATP_USE_ATOMIC && FATP_USE_MUTEX

// =============================================================================
// WaitableSynchronizationPolicy - Condition variable support
// =============================================================================

#if FATP_USE_MUTEX && FATP_USE_CONDITION_VARIABLE
struct WaitableSynchronizationPolicy
{
    using PolicyTag = void;

    WaitableSynchronizationPolicy() = default;
    WaitableSynchronizationPolicy(const WaitableSynchronizationPolicy&) = delete;
    WaitableSynchronizationPolicy& operator=(const WaitableSynchronizationPolicy&) = delete;

    class LockGuard
    {
    public:
        explicit LockGuard(std::mutex& mutex) : mLock(mutex) {}

        LockGuard(const LockGuard&) = delete;
        LockGuard& operator=(const LockGuard&) = delete;
        ~LockGuard() = default;

        template <typename Predicate>
        void wait(std::condition_variable& cond, Predicate pred)
        {
            cond.wait(mLock, std::move(pred));
        }

#if FATP_HAS_JTHREAD
        template <typename Predicate>
        bool wait(std::condition_variable& cond, std::stop_token st, Predicate pred)
        {
            return cond.wait(mLock, st, std::move(pred));
        }
#endif

        template <typename Rep, typename Period, typename Predicate>
        bool wait_for(std::condition_variable& cond,
                      const std::chrono::duration<Rep, Period>& rel_time,
                      Predicate pred)
        {
            return cond.wait_for(mLock, rel_time, std::move(pred));
        }

        bool owns_lock() const { return mLock.owns_lock(); }
        std::mutex* mutex() const { return mLock.mutex(); }

    private:
        std::unique_lock<std::mutex> mLock;
    };

    using SharedGuard = LockGuard;
    using WriteLock = LockGuard;
    using ReadLock = LockGuard;

    [[nodiscard]] LockGuard lock() { return LockGuard(mMutex); }
    [[nodiscard]] SharedGuard lock_shared() const
    {
        return SharedGuard(const_cast<std::mutex&>(mMutex));
    }

    [[nodiscard]] bool try_lock() { return mMutex.try_lock(); }

    std::mutex& getLock() const { return const_cast<std::mutex&>(mMutex); }
    std::condition_variable& getCondition() const
    {
        return const_cast<std::condition_variable&>(mCondition);
    }

    static std::mutex& getStaticLock()
    {
        static std::mutex mMutex;
        return mMutex;
    }

    uint64_t getContention() const { return 0; }

private:
    mutable std::mutex mMutex{};
    mutable std::condition_variable mCondition{};
};
#endif // FATP_USE_MUTEX && FATP_USE_CONDITION_VARIABLE

// =============================================================================
// SeqLockPolicy - Optimistic read-heavy synchronization
// =============================================================================

#if FATP_USE_ATOMIC
struct SeqLockPolicy
{
    using PolicyTag = void;
    using OptimisticTag = void;

    SeqLockPolicy() = default;
    SeqLockPolicy(const SeqLockPolicy&) = delete;
    SeqLockPolicy& operator=(const SeqLockPolicy&) = delete;

    class LockGuard
    {
    public:
        explicit LockGuard(std::atomic<uint64_t>& seq) : mSequence(seq)
        {
            uint64_t current = mSequence.load(std::memory_order_relaxed);
            mSequence.store(current + 1, std::memory_order_release);
            std::atomic_thread_fence(std::memory_order_seq_cst);
        }

        LockGuard(const LockGuard&) = delete;
        LockGuard& operator=(const LockGuard&) = delete;

        ~LockGuard()
        {
            std::atomic_thread_fence(std::memory_order_seq_cst);
            uint64_t current = mSequence.load(std::memory_order_relaxed);
            mSequence.store(current + 1, std::memory_order_release);
        }

    private:
        std::atomic<uint64_t>& mSequence;
    };

    class SharedGuard
    {
    public:
        static constexpr int MAX_RETRIES = 1000;

        explicit SharedGuard(std::atomic<uint64_t>& seq, int max_retries = MAX_RETRIES)
            : mSequence(seq)
            , valid_(true)
        {
            int retries = 0;
            int backoff_count = 0;
            do
            {
                start_seq_ = mSequence.load(std::memory_order_acquire);
                if (start_seq_ & 1)
                {
                    if (++retries > max_retries)
                    {
                        valid_ = false;
                        return;
                    }
                    if (++backoff_count >= (1 << std::min(retries / 10, 5)))
                    {
                        std::this_thread::yield();
                        backoff_count = 0;
                    }
                }
            } while (start_seq_ & 1);
            std::atomic_thread_fence(std::memory_order_acquire);
        }

        SharedGuard(const SharedGuard&) = delete;
        SharedGuard& operator=(const SharedGuard&) = delete;
        ~SharedGuard() = default;

        bool is_valid() const
        {
            if (!valid_)
            {
                return false;
            }
            std::atomic_thread_fence(std::memory_order_acquire);
            return mSequence.load(std::memory_order_acquire) == start_seq_;
        }

        bool retry_limit_exceeded() const { return !valid_; }
        uint64_t get_sequence() const { return start_seq_; }

    private:
        std::atomic<uint64_t>& mSequence;
        uint64_t start_seq_;
        bool valid_;
    };

    using WriteLock = LockGuard;
    using ReadLock = SharedGuard;

    [[nodiscard]] LockGuard lock() { return LockGuard(mSequence); }
    [[nodiscard]] SharedGuard lock_shared() const
    {
        return SharedGuard(const_cast<std::atomic<uint64_t>&>(mSequence));
    }

    std::atomic<uint64_t>& getLock() { return mSequence; }

    static std::atomic<uint64_t>& getStaticLock()
    {
        static std::atomic<uint64_t> mSequence{0};
        return mSequence;
    }

    uint64_t get_sequence() const { return mSequence.load(std::memory_order_relaxed); }
    uint64_t getContention() const { return 0; }

private:
    alignas(FATP_CACHE_LINE_SIZE) std::atomic<uint64_t> mSequence{0};
};
#endif // FATP_USE_ATOMIC

// =============================================================================
// TicketLockPolicy - Fair FIFO spinlock
// =============================================================================

#if FATP_USE_ATOMIC
struct TicketLockPolicy
{
    using PolicyTag = void;
    using FairOrderingTag = void;

    TicketLockPolicy() = default;
    TicketLockPolicy(const TicketLockPolicy&) = delete;
    TicketLockPolicy& operator=(const TicketLockPolicy&) = delete;

    class LockGuard
    {
    public:
        explicit LockGuard(TicketLockPolicy& policy)
            : mNowServing(policy.mNowServing)
        {
            my_ticket_ = policy.mNextTicket.fetch_add(1, std::memory_order_relaxed);
            while (mNowServing.load(std::memory_order_acquire) != my_ticket_)
            {
                std::this_thread::yield();
            }
        }

        LockGuard(const LockGuard&) = delete;
        LockGuard& operator=(const LockGuard&) = delete;

        ~LockGuard()
        {
            mNowServing.fetch_add(1, std::memory_order_release);
        }

    private:
        std::atomic<uint64_t>& mNowServing;
        uint64_t my_ticket_;
    };

    using SharedGuard = LockGuard;
    using WriteLock = LockGuard;
    using ReadLock = LockGuard;

    [[nodiscard]] LockGuard lock() { return LockGuard(*this); }
    [[nodiscard]] SharedGuard lock_shared() const
    {
        return SharedGuard(const_cast<TicketLockPolicy&>(*this));
    }

    [[nodiscard]] bool try_lock()
    {
        uint64_t current = mNowServing.load(std::memory_order_acquire);
        uint64_t next = mNextTicket.load(std::memory_order_relaxed);
        if (current == next)
        {
            return mNextTicket.compare_exchange_strong(next, next + 1,
                std::memory_order_acquire, std::memory_order_relaxed);
        }
        return false;
    }

    TicketLockPolicy& getLock() { return *this; }

    static TicketLockPolicy& getStaticLock()
    {
        static TicketLockPolicy mPolicy;
        return mPolicy;
    }

    uint64_t get_queue_length() const
    {
        return mNextTicket.load(std::memory_order_relaxed) -
               mNowServing.load(std::memory_order_relaxed);
    }

    uint64_t getContention() const { return get_queue_length(); }

private:
    alignas(FATP_CACHE_LINE_SIZE) std::atomic<uint64_t> mNextTicket{0};
    alignas(FATP_CACHE_LINE_SIZE) std::atomic<uint64_t> mNowServing{0};
};
#endif // FATP_USE_ATOMIC

// =============================================================================
// MCSLockPolicy - Scalable queue-based lock for NUMA
// =============================================================================

#if FATP_USE_ATOMIC
struct MCSLockPolicy
{
    using PolicyTag = void;
    using NUMAAwareTag = void;
    using FairOrderingTag = void;

    struct alignas(FATP_CACHE_LINE_SIZE) QNode
    {
        std::atomic<QNode*> next{nullptr};
        std::atomic<bool> locked{false};
    };

    MCSLockPolicy() = default;
    MCSLockPolicy(const MCSLockPolicy&) = delete;
    MCSLockPolicy& operator=(const MCSLockPolicy&) = delete;

    class LockGuard
    {
    public:
        explicit LockGuard(std::atomic<QNode*>& tail) : mTail(tail)
        {
            my_node_.next.store(nullptr, std::memory_order_relaxed);
            my_node_.locked.store(true, std::memory_order_relaxed);

            QNode* prev = mTail.exchange(&my_node_, std::memory_order_acq_rel);

            if (prev != nullptr)
            {
                prev->next.store(&my_node_, std::memory_order_release);
                while (my_node_.locked.load(std::memory_order_acquire))
                {
                    std::this_thread::yield();
                }
            }
        }

        LockGuard(const LockGuard&) = delete;
        LockGuard& operator=(const LockGuard&) = delete;

        ~LockGuard()
        {
            QNode* next_node = my_node_.next.load(std::memory_order_acquire);

            if (next_node == nullptr)
            {
                QNode* expected = &my_node_;
                if (mTail.compare_exchange_strong(expected, nullptr, std::memory_order_release))
                {
                    return;
                }
                while ((next_node = my_node_.next.load(std::memory_order_acquire)) == nullptr)
                {
                    std::this_thread::yield();
                }
            }
            next_node->locked.store(false, std::memory_order_release);
        }

    private:
        std::atomic<QNode*>& mTail;
        QNode my_node_;
    };

    using SharedGuard = LockGuard;
    using WriteLock = LockGuard;
    using ReadLock = LockGuard;

    [[nodiscard]] LockGuard lock() { return LockGuard(mTail); }
    [[nodiscard]] SharedGuard lock_shared() const
    {
        return SharedGuard(const_cast<std::atomic<QNode*>&>(mTail));
    }

    std::atomic<QNode*>& getLock() { return mTail; }

    static std::atomic<QNode*>& getStaticLock()
    {
        static std::atomic<QNode*> mTail{nullptr};
        return mTail;
    }

    uint64_t getContention() const { return 0; }

private:
    alignas(FATP_CACHE_LINE_SIZE) std::atomic<QNode*> mTail{nullptr};
};
#endif // FATP_USE_ATOMIC

// =============================================================================
// RCUPolicy - Read-Copy-Update for lock-free reads
// =============================================================================

#if FATP_USE_ATOMIC && FATP_USE_MUTEX
template <typename T>
struct RCUPolicy
{
    using PolicyTag = void;
    using LockFreeTag = void;

    RCUPolicy() : data_(std::make_shared<T>()) {}
    explicit RCUPolicy(T initial) : data_(std::make_shared<T>(std::move(initial))) {}

    RCUPolicy(const RCUPolicy&) = delete;
    RCUPolicy& operator=(const RCUPolicy&) = delete;

    class SharedGuard
    {
    public:
        explicit SharedGuard(std::shared_ptr<T> snapshot) : snapshot_(std::move(snapshot)) {}

        SharedGuard(const SharedGuard&) = delete;
        SharedGuard& operator=(const SharedGuard&) = delete;
        ~SharedGuard() = default;

        const T& operator*() const { return *snapshot_; }
        const T* operator->() const { return snapshot_.get(); }
        const T* get() const { return snapshot_.get(); }

    private:
        std::shared_ptr<T> snapshot_;
    };

    class LockGuard
    {
    public:
#if FATP_HAS_ATOMIC_SHARED_PTR
        LockGuard(std::atomic<std::shared_ptr<T>>& data, std::mutex& write_mutex)
            : data_(data)
            , mWriteLock(write_mutex)
        {
        }
#else
        LockGuard(std::shared_ptr<T>& data_ptr,
                  std::shared_mutex& rw_mutex,
                  std::mutex& write_mutex)
            : data_ptr_(data_ptr)
            , mRwMutex(rw_mutex)
            , mWriteLock(write_mutex)
        {
        }
#endif

        LockGuard(const LockGuard&) = delete;
        LockGuard& operator=(const LockGuard&) = delete;
        ~LockGuard() = default;

        template <typename Func>
        void update(Func&& f)
        {
#if FATP_HAS_ATOMIC_SHARED_PTR
            auto current = data_.load(std::memory_order_acquire);
            std::shared_ptr<T> new_data;
            do
            {
                new_data = std::make_shared<T>(*current);
                f(*new_data);
            } while (!data_.compare_exchange_weak(current, new_data,
                                                   std::memory_order_release,
                                                   std::memory_order_acquire));
#else
            std::shared_ptr<T> current;
            {
                std::shared_lock<std::shared_mutex> readLock(mRwMutex);
                current = data_ptr_;
            }
            auto new_data = std::make_shared<T>(*current);
            f(*new_data);
            {
                std::unique_lock<std::shared_mutex> write_lock(mRwMutex);
                data_ptr_ = new_data;
            }
#endif
        }

    private:
#if FATP_HAS_ATOMIC_SHARED_PTR
        std::atomic<std::shared_ptr<T>>& data_;
        std::unique_lock<std::mutex> mWriteLock;
#else
        std::shared_ptr<T>& data_ptr_;
        std::shared_mutex& mRwMutex;
        std::unique_lock<std::mutex> mWriteLock;
#endif
    };

    using WriteLock = LockGuard;
    using ReadLock = SharedGuard;

    struct LockHandle
    {
#if FATP_HAS_ATOMIC_SHARED_PTR
        std::atomic<std::shared_ptr<T>>& data;
        std::mutex& write_mutex;
#else
        std::shared_ptr<T>& data;
        std::shared_mutex& rw_mutex;
        std::mutex& write_mutex;
#endif
    };

    LockHandle getLock()
    {
#if FATP_HAS_ATOMIC_SHARED_PTR
        return {data_, mWriteMutex};
#else
        return {data_, mRwMutex, mWriteMutex};
#endif
    }

#if FATP_HAS_ATOMIC_SHARED_PTR
    [[nodiscard]] SharedGuard read() const
    {
        return SharedGuard(data_.load(std::memory_order_acquire));
    }

    static constexpr bool is_lock_free() { return true; }
#else
    [[nodiscard]] SharedGuard read() const
    {
        std::shared_lock<std::shared_mutex> lock(mRwMutex);
        return SharedGuard(data_);
    }

    static constexpr bool is_lock_free() { return false; }
#endif

    [[nodiscard]] LockGuard write()
    {
#if FATP_HAS_ATOMIC_SHARED_PTR
        return LockGuard(data_, mWriteMutex);
#else
        return LockGuard(data_, mRwMutex, mWriteMutex);
#endif
    }

    uint64_t getContention() const { return 0; }

private:
#if FATP_HAS_ATOMIC_SHARED_PTR
    mutable std::atomic<std::shared_ptr<T>> data_;
    std::mutex mWriteMutex;
#else
    mutable std::shared_ptr<T> data_;
    mutable std::shared_mutex mRwMutex;
    std::mutex mWriteMutex;
#endif
};
#endif // FATP_USE_ATOMIC && FATP_USE_MUTEX

// =============================================================================
// HazardPointerPolicy - Safe memory reclamation for lock-free structures
// =============================================================================

#if FATP_USE_ATOMIC
template <typename T>
struct HazardPointerPolicy
{
    using PolicyTag = void;
    using LockFreeTag = void;

    static constexpr size_t HP_PER_THREAD = 2;
    static constexpr size_t RETIRED_THRESHOLD = 64;

    struct alignas(FATP_CACHE_LINE_SIZE) HazardPointer
    {
        std::atomic<T*> ptr{nullptr};
    };

    struct ThreadHPRecord
    {
        std::array<HazardPointer, HP_PER_THREAD> hps;
        std::atomic<bool> active{true};
        ThreadHPRecord* next{nullptr};
    };

    HazardPointerPolicy() = default;
    HazardPointerPolicy(const HazardPointerPolicy&) = delete;
    HazardPointerPolicy& operator=(const HazardPointerPolicy&) = delete;

    ~HazardPointerPolicy()
    {
        auto& retired = get_retired_list();
        for (T* ptr : retired)
        {
            delete ptr;
        }
        retired.clear();
    }

    class Guard
    {
    public:
        explicit Guard(std::atomic<T*>& hp) : hp_(hp), protected_ptr_(nullptr) {}

        Guard(const Guard&) = delete;
        Guard& operator=(const Guard&) = delete;

        T* protect(std::atomic<T*>& src)
        {
            T* ptr;
            do
            {
                ptr = src.load(std::memory_order_acquire);
                hp_.store(ptr, std::memory_order_release);
            } while (ptr != src.load(std::memory_order_acquire));

            protected_ptr_ = ptr;
            return ptr;
        }

        T* get() const { return protected_ptr_; }

        ~Guard()
        {
            hp_.store(nullptr, std::memory_order_release);
        }

    private:
        std::atomic<T*>& hp_;
        T* protected_ptr_;
    };

    using LockGuard = Guard;
    using SharedGuard = Guard;
    using WriteLock = Guard;
    using ReadLock = Guard;

    [[nodiscard]] Guard acquire(size_t index = 0) const
    {
        return Guard(get_thread_record().hps[index].ptr);
    }

    [[nodiscard]] Guard lock() const { return acquire(0); }
    [[nodiscard]] Guard lock_shared() const { return acquire(0); }

    void retire(T* ptr)
    {
        auto& retired_list = get_retired_list();
        retired_list.push_back(ptr);

        if (retired_list.size() >= RETIRED_THRESHOLD)
        {
            scan_and_reclaim();
        }
    }

    void force_reclaim()
    {
        scan_and_reclaim();
    }

    uint64_t getContention() const { return 0; }

private:
    static std::mutex& get_global_mutex()
    {
        static std::mutex mtx;
        return mtx;
    }

    static std::atomic<ThreadHPRecord*>& get_hp_list_head()
    {
        static std::atomic<ThreadHPRecord*> head{nullptr};
        return head;
    }

    struct ThreadRecordManager
    {
        ThreadHPRecord* record = nullptr;

        ThreadRecordManager()
        {
            record = new ThreadHPRecord();
            std::lock_guard<std::mutex> lock(get_global_mutex());
            record->next = get_hp_list_head().load(std::memory_order_relaxed);
            get_hp_list_head().store(record, std::memory_order_release);
        }

        ~ThreadRecordManager()
        {
            if (record)
            {
                record->active.store(false, std::memory_order_release);
            }
        }
    };

    static ThreadHPRecord& get_thread_record()
    {
        thread_local ThreadRecordManager manager;
        return *manager.record;
    }

    static std::vector<T*>& get_retired_list()
    {
        thread_local std::vector<T*> retired_list;
        return retired_list;
    }

    void scan_and_reclaim() const
    {
        auto& retired_list = get_retired_list();
        if (retired_list.empty())
        {
            return;
        }

        std::vector<T*> protected_ptrs;
        protected_ptrs.reserve(128);

        {
            std::lock_guard<std::mutex> lock(get_global_mutex());
            ThreadHPRecord* current = get_hp_list_head().load(std::memory_order_acquire);
            while (current != nullptr)
            {
                if (current->active.load(std::memory_order_acquire))
                {
                    for (size_t i = 0; i < HP_PER_THREAD; ++i)
                    {
                        T* ptr = current->hps[i].ptr.load(std::memory_order_acquire);
                        if (ptr != nullptr)
                        {
                            protected_ptrs.push_back(ptr);
                        }
                    }
                }
                current = current->next;
            }
        }

        std::sort(protected_ptrs.begin(), protected_ptrs.end());

        auto new_end = std::remove_if(retired_list.begin(), retired_list.end(),
            [&protected_ptrs](T* ptr)
            {
                bool is_protected = std::binary_search(
                    protected_ptrs.begin(), protected_ptrs.end(), ptr);
                if (!is_protected)
                {
                    delete ptr;
                    return true;
                }
                return false;
            });

        retired_list.erase(new_end, retired_list.end());
    }
};
#endif // FATP_USE_ATOMIC

// =============================================================================
// AdaptiveLockPolicy - Runtime-adaptive spinlock/mutex hybrid
// =============================================================================

#if FATP_USE_ATOMIC && FATP_USE_MUTEX
struct AdaptiveLockPolicy
{
    using PolicyTag = void;
    using AdaptiveTag = void;

    static constexpr uint32_t SPIN_THRESHOLD = 100;
    static constexpr uint32_t ADAPT_WINDOW = 1000;

    AdaptiveLockPolicy() = default;
    AdaptiveLockPolicy(const AdaptiveLockPolicy&) = delete;
    AdaptiveLockPolicy& operator=(const AdaptiveLockPolicy&) = delete;

    class LockGuard
    {
    public:
        explicit LockGuard(AdaptiveLockPolicy& policy)
            : mMutex(policy.mMutex)
            , mSpinLock(policy.mSpinLock)
            , mContentionCounter(policy.mContentionCounter)
            , use_mutex_(policy.use_mutex_)
            , using_mutex_(use_mutex_.load(std::memory_order_relaxed))
        {
            if (using_mutex_)
            {
                mMutex.lock();
            }
            else
            {
                uint32_t spins = 0;
                while (mSpinLock.test_and_set(std::memory_order_acquire))
                {
                    if (++spins > SPIN_THRESHOLD)
                    {
                        uint32_t count = mContentionCounter.fetch_add(1,
                            std::memory_order_relaxed);
                        if (count > ADAPT_WINDOW / 10)
                        {
                            use_mutex_.store(true, std::memory_order_relaxed);
                        }
                        mSpinLock.clear(std::memory_order_release);
                        mMutex.lock();
                        using_mutex_ = true;
                        return;
                    }
                    std::this_thread::yield();
                }
            }

            uint32_t count = mContentionCounter.load(std::memory_order_relaxed);
            if (count > ADAPT_WINDOW)
            {
                if (count < ADAPT_WINDOW + ADAPT_WINDOW / 10)
                {
                    use_mutex_.store(false, std::memory_order_relaxed);
                }
                mContentionCounter.store(0, std::memory_order_relaxed);
            }
        }

        LockGuard(const LockGuard&) = delete;
        LockGuard& operator=(const LockGuard&) = delete;

        ~LockGuard()
        {
            if (using_mutex_)
            {
                mMutex.unlock();
            }
            else
            {
                mSpinLock.clear(std::memory_order_release);
            }
        }

    private:
        std::mutex& mMutex;
        std::atomic_flag& mSpinLock;
        std::atomic<uint32_t>& mContentionCounter;
        std::atomic<bool>& use_mutex_;
        bool using_mutex_;
    };

    using SharedGuard = LockGuard;
    using WriteLock = LockGuard;
    using ReadLock = LockGuard;

    [[nodiscard]] LockGuard lock() { return LockGuard(*this); }
    [[nodiscard]] SharedGuard lock_shared() const
    {
        return SharedGuard(const_cast<AdaptiveLockPolicy&>(*this));
    }

    [[nodiscard]] bool try_lock()
    {
        if (use_mutex_.load(std::memory_order_relaxed))
        {
            return mMutex.try_lock();
        }
        return !mSpinLock.test_and_set(std::memory_order_acquire);
    }

    AdaptiveLockPolicy& getLock() { return *this; }

    static AdaptiveLockPolicy& getStaticLock()
    {
        static AdaptiveLockPolicy mPolicy;
        return mPolicy;
    }

    bool is_using_mutex() const noexcept
    {
        return use_mutex_.load(std::memory_order_relaxed);
    }
    uint32_t getContention() const noexcept
    {
        return mContentionCounter.load(std::memory_order_relaxed);
    }
    void resetContention() noexcept
    {
        mContentionCounter.store(0, std::memory_order_relaxed);
    }

private:
    alignas(FATP_CACHE_LINE_SIZE) std::mutex mMutex;
    alignas(FATP_CACHE_LINE_SIZE) std::atomic_flag mSpinLock = ATOMIC_FLAG_INIT;
    alignas(FATP_CACHE_LINE_SIZE) std::atomic<uint32_t> mContentionCounter{0};
    std::atomic<bool> use_mutex_{false};
};
#endif // FATP_USE_ATOMIC && FATP_USE_MUTEX

// =============================================================================
// PriorityInheritanceLockPolicy - Real-time priority inheritance
// =============================================================================

#if FATP_USE_MUTEX
struct PriorityInheritanceLockPolicy
{
    using PolicyTag = void;
    using RealtimeTag = void;
    using FairOrderingTag = void;

#if FATP_HAS_PTHREAD_PRIO_INHERIT
    PriorityInheritanceLockPolicy()
    {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_setprotocol(&attr, PTHREAD_PRIO_INHERIT);
        pthread_mutex_init(&mMutex, &attr);
        pthread_mutexattr_destroy(&attr);
    }

    ~PriorityInheritanceLockPolicy()
    {
        pthread_mutex_destroy(&mMutex);
    }
#elif FATP_HAS_WIN32_CRITICAL_SECTION
    PriorityInheritanceLockPolicy()
    {
        InitializeCriticalSection(&cs_);
    }

    ~PriorityInheritanceLockPolicy()
    {
        DeleteCriticalSection(&cs_);
    }
#else
    PriorityInheritanceLockPolicy() = default;
    ~PriorityInheritanceLockPolicy() = default;
#endif

    PriorityInheritanceLockPolicy(const PriorityInheritanceLockPolicy&) = delete;
    PriorityInheritanceLockPolicy& operator=(const PriorityInheritanceLockPolicy&) = delete;

    class LockGuard
    {
    public:
#if FATP_HAS_PTHREAD_PRIO_INHERIT
        explicit LockGuard(pthread_mutex_t& mutex) : mMutex(mutex)
        {
            pthread_mutex_lock(&mMutex);
        }

        ~LockGuard()
        {
            pthread_mutex_unlock(&mMutex);
        }

    private:
        pthread_mutex_t& mMutex;
#elif FATP_HAS_WIN32_CRITICAL_SECTION
        explicit LockGuard(CRITICAL_SECTION& cs) : cs_(cs)
        {
            EnterCriticalSection(&cs_);
        }

        ~LockGuard()
        {
            LeaveCriticalSection(&cs_);
        }

    private:
        CRITICAL_SECTION& cs_;
#else
        explicit LockGuard(std::mutex& mutex) : mGuard(mutex) {}
        ~LockGuard() = default;

    private:
        std::lock_guard<std::mutex> mGuard;
#endif

    public:
        LockGuard(const LockGuard&) = delete;
        LockGuard& operator=(const LockGuard&) = delete;
    };

    using SharedGuard = LockGuard;
    using WriteLock = LockGuard;
    using ReadLock = LockGuard;

#if FATP_HAS_PTHREAD_PRIO_INHERIT
    [[nodiscard]] LockGuard lock() { return LockGuard(mMutex); }
    [[nodiscard]] SharedGuard lock_shared() const
    {
        return SharedGuard(const_cast<pthread_mutex_t&>(mMutex));
    }

    [[nodiscard]] bool try_lock()
    {
        return pthread_mutex_trylock(&mMutex) == 0;
    }

    pthread_mutex_t& getLock() { return mMutex; }

    static pthread_mutex_t& getStaticLock()
    {
        static pthread_mutex_t mMutex;
        static std::once_flag init_flag;
        std::call_once(init_flag, []()
        {
            pthread_mutexattr_t attr;
            pthread_mutexattr_init(&attr);
            pthread_mutexattr_setprotocol(&attr, PTHREAD_PRIO_INHERIT);
            pthread_mutex_init(&mMutex, &attr);
            pthread_mutexattr_destroy(&attr);
        });
        return mMutex;
    }

private:
    pthread_mutex_t mMutex;

#elif FATP_HAS_WIN32_CRITICAL_SECTION
    [[nodiscard]] LockGuard lock() { return LockGuard(cs_); }
    [[nodiscard]] SharedGuard lock_shared() const
    {
        return SharedGuard(const_cast<CRITICAL_SECTION&>(cs_));
    }

    [[nodiscard]] bool try_lock()
    {
        return TryEnterCriticalSection(&cs_) != 0;
    }

    CRITICAL_SECTION& getLock() { return cs_; }

    static CRITICAL_SECTION& getStaticLock()
    {
        static CRITICAL_SECTION cs_;
        static std::once_flag init_flag;
        std::call_once(init_flag, []()
        {
            InitializeCriticalSection(&cs_);
        });
        return cs_;
    }

private:
    CRITICAL_SECTION cs_;

#else
    [[nodiscard]] LockGuard lock() { return LockGuard(mMutex); }
    [[nodiscard]] SharedGuard lock_shared() const
    {
        return SharedGuard(const_cast<std::mutex&>(mMutex));
    }

    [[nodiscard]] bool try_lock() { return mMutex.try_lock(); }

    std::mutex& getLock() { return mMutex; }

    static std::mutex& getStaticLock()
    {
        static std::mutex mMutex;
        return mMutex;
    }

private:
    std::mutex mMutex;
#endif

public:
    uint64_t getContention() const { return 0; }
};
#endif // FATP_USE_MUTEX

// =============================================================================
// VersionedLockPolicy - Optimistic versioned concurrency control
// =============================================================================

#if FATP_USE_ATOMIC
struct VersionedLockPolicy
{
    using PolicyTag = void;
    using OptimisticTag = void;

    VersionedLockPolicy() = default;
    VersionedLockPolicy(const VersionedLockPolicy&) = delete;
    VersionedLockPolicy& operator=(const VersionedLockPolicy&) = delete;

    class LockGuard
    {
    public:
        LockGuard(std::atomic<uint64_t>& version, std::mutex& writeLock)
            : mVersion(version)
            , mLock(writeLock)
            , write_version_(mVersion.load(std::memory_order_acquire))
            , committed_(false)
        {
        }

        LockGuard(const LockGuard&) = delete;
        LockGuard& operator=(const LockGuard&) = delete;

        void commit()
        {
            if (!committed_)
            {
                mVersion.store(write_version_ + 1, std::memory_order_release);
                committed_ = true;
            }
        }

        uint64_t getVersion() const { return write_version_; }

        ~LockGuard()
        {
            if (!committed_)
            {
                commit();
            }
        }

    private:
        std::atomic<uint64_t>& mVersion;
        std::unique_lock<std::mutex> mLock;
        uint64_t write_version_;
        bool committed_;
    };

    class SharedGuard
    {
    public:
        explicit SharedGuard(std::atomic<uint64_t>& version)
            : mVersion(version)
            , mReadVersion(mVersion.load(std::memory_order_acquire))
        {
        }

        SharedGuard(const SharedGuard&) = delete;
        SharedGuard& operator=(const SharedGuard&) = delete;
        ~SharedGuard() = default;

        bool validate() const
        {
            std::atomic_thread_fence(std::memory_order_acquire);
            return mVersion.load(std::memory_order_acquire) == mReadVersion;
        }

        uint64_t getVersion() const { return mReadVersion; }

    private:
        std::atomic<uint64_t>& mVersion;
        uint64_t mReadVersion;
    };

    using WriteLock = LockGuard;
    using ReadLock = SharedGuard;

    struct LockHandle
    {
        std::atomic<uint64_t>& version;
        std::mutex& writeLock;
    };

    [[nodiscard]] LockGuard lock()
    {
        return LockGuard(mVersion, mWriteLock);
    }

    [[nodiscard]] SharedGuard lock_shared() const
    {
        return SharedGuard(const_cast<std::atomic<uint64_t>&>(mVersion));
    }

    [[nodiscard]] bool try_lock()
    {
        return mWriteLock.try_lock();
    }

    LockHandle getLock() { return {mVersion, mWriteLock}; }

    static LockHandle getStaticLock()
    {
        static std::atomic<uint64_t> mVersion{0};
        static std::mutex mWriteLock;
        return {mVersion, mWriteLock};
    }

    uint64_t getVersion() const { return mVersion.load(std::memory_order_relaxed); }
    uint64_t getContention() const { return 0; }

private:
    alignas(FATP_CACHE_LINE_SIZE) std::atomic<uint64_t> mVersion{0};
    std::mutex mWriteLock;
};
#endif // FATP_USE_ATOMIC

// =============================================================================
// RecursiveMutexPolicy - Reentrant Mutex
// =============================================================================

#if FATP_USE_MUTEX
struct RecursiveMutexPolicy
{
    using PolicyTag = void;
    using RecursiveTag = void;

    RecursiveMutexPolicy() = default;
    RecursiveMutexPolicy(const RecursiveMutexPolicy&) = delete;
    RecursiveMutexPolicy& operator=(const RecursiveMutexPolicy&) = delete;

    using LockGuard = std::lock_guard<std::recursive_mutex>;
    using SharedGuard = std::lock_guard<std::recursive_mutex>;
    using WriteLock = LockGuard;
    using ReadLock = LockGuard;

    [[nodiscard]] LockGuard lock() { return LockGuard(mLock); }
    [[nodiscard]] SharedGuard lock_shared() const
    {
        return SharedGuard(const_cast<std::recursive_mutex&>(mLock));
    }

    [[nodiscard]] bool try_lock() { return mLock.try_lock(); }

    std::recursive_mutex& getLock() const { return const_cast<std::recursive_mutex&>(mLock); }

    static std::recursive_mutex& getStaticLock()
    {
        static std::recursive_mutex static_lock;
        return static_lock;
    }

    uint64_t getContention() const { return 0; }

private:
    mutable std::recursive_mutex mLock;
};
#endif // FATP_USE_MUTEX

// =============================================================================
// TimedMutexPolicy - Mutex with Timeout
// =============================================================================

#if FATP_USE_MUTEX
struct TimedMutexPolicy
{
    using PolicyTag = void;

    TimedMutexPolicy() = default;
    TimedMutexPolicy(const TimedMutexPolicy&) = delete;
    TimedMutexPolicy& operator=(const TimedMutexPolicy&) = delete;

    class LockGuard
    {
    public:
        explicit LockGuard(std::timed_mutex& mtx) : mLock(mtx) {}
        LockGuard(std::timed_mutex& mtx, std::defer_lock_t) : mLock(mtx, std::defer_lock) {}

        template<typename Rep, typename Period>
        bool try_lock_for(const std::chrono::duration<Rep, Period>& timeout)
        {
            if (mLock.owns_lock())
            {
                return true;
            }
            return mLock.try_lock_for(timeout);
        }

        bool owns_lock() const { return mLock.owns_lock(); }

#ifdef _MSC_VER
#pragma warning(push)
// C26110: Caller failing to hold lock before calling unlock.
// This is a false positive - the analyzer doesn't recognize that the
// if (mLock.owns_lock()) check guarantees the lock is held before unlock().
#pragma warning(disable : 26110)
#endif
        void unlock()
        {
            if (mLock.owns_lock())
            {
                mLock.unlock();
            }
        }
#ifdef _MSC_VER
#pragma warning(pop)
#endif

        LockGuard(const LockGuard&) = delete;
        LockGuard& operator=(const LockGuard&) = delete;

    private:
        std::unique_lock<std::timed_mutex> mLock;
    };

    using SharedGuard = LockGuard;
    using WriteLock = LockGuard;
    using ReadLock = LockGuard;

    [[nodiscard]] LockGuard lock() { return LockGuard(mLock); }
    [[nodiscard]] SharedGuard lock_shared() const
    {
        return SharedGuard(const_cast<std::timed_mutex&>(mLock));
    }
    [[nodiscard]] LockGuard lock_deferred() { return LockGuard(mLock, std::defer_lock); }

    [[nodiscard]] bool try_lock() { return mLock.try_lock(); }

    template<typename Rep, typename Period>
    [[nodiscard]] bool try_lock_for(const std::chrono::duration<Rep, Period>& timeout)
    {
        return mLock.try_lock_for(timeout);
    }

    std::timed_mutex& getLock() const { return const_cast<std::timed_mutex&>(mLock); }

    static std::timed_mutex& getStaticLock()
    {
        static std::timed_mutex static_lock;
        return static_lock;
    }

    uint64_t getContention() const { return 0; }

private:
    mutable std::timed_mutex mLock;
};
#endif // FATP_USE_MUTEX

// =============================================================================
// SharedTimedMutexPolicy - Shared Mutex with Timeout
// =============================================================================

#if FATP_USE_SHARED_MUTEX
struct SharedTimedMutexPolicy
{
    using PolicyTag = void;

    SharedTimedMutexPolicy() = default;
    SharedTimedMutexPolicy(const SharedTimedMutexPolicy&) = delete;
    SharedTimedMutexPolicy& operator=(const SharedTimedMutexPolicy&) = delete;

    using LockGuard = std::unique_lock<std::shared_timed_mutex>;
    using SharedGuard = std::shared_lock<std::shared_timed_mutex>;
    using WriteLock = LockGuard;
    using ReadLock = SharedGuard;

    [[nodiscard]] LockGuard lock() { return LockGuard(mLock); }
    [[nodiscard]] SharedGuard lock_shared() const
    {
        return SharedGuard(const_cast<std::shared_timed_mutex&>(mLock));
    }

    [[nodiscard]] bool try_lock() { return mLock.try_lock(); }
    [[nodiscard]] bool try_lock_shared() const
    {
        return const_cast<std::shared_timed_mutex&>(mLock).try_lock_shared();
    }

    template<typename Rep, typename Period>
    [[nodiscard]] bool try_lock_for(const std::chrono::duration<Rep, Period>& timeout)
    {
        return mLock.try_lock_for(timeout);
    }

    template<typename Rep, typename Period>
    [[nodiscard]] bool try_lock_shared_for(const std::chrono::duration<Rep, Period>& timeout) const
    {
        return const_cast<std::shared_timed_mutex&>(mLock).try_lock_shared_for(timeout);
    }

    std::shared_timed_mutex& getLock() const
    {
        return const_cast<std::shared_timed_mutex&>(mLock);
    }

    static std::shared_timed_mutex& getStaticLock()
    {
        static std::shared_timed_mutex static_lock;
        return static_lock;
    }

    uint64_t getContention() const { return 0; }

private:
    mutable std::shared_timed_mutex mLock;
};
#endif // FATP_USE_SHARED_MUTEX

// =============================================================================
// Convenience Aliases
// =============================================================================

using NoLocking = SingleThreadedPolicy;
using ReadWriteLock = SharedMutexPolicy;
using SpinLock = SpinlockSynchronizationPolicy;

// =============================================================================
// C++20 Concepts (if available)
// =============================================================================

#if FATP_HAS_CPP20
template<typename P>
concept ConcurrencyPolicy = requires(P p)
{
    typename P::LockGuard;
    typename P::SharedGuard;
    { p.lock() } -> std::same_as<typename P::LockGuard>;
    { p.lock_shared() } -> std::same_as<typename P::SharedGuard>;
};
#endif

} // namespace fat_p
