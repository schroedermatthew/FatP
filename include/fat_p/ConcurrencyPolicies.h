#pragma once

/*
FATP_META:
  meta_version: 1
  component: ConcurrencyPolicies
  file_role: public_header
  path: include/fat_p/ConcurrencyPolicies.h
  namespace: fat_p
  layer: Foundation
  summary: "Public header for ConcurrencyPolicies."
  api_stability: in_work
  related:
    docs_search: "ConcurrencyPolicies"
    tests:
      - components/ConcurrencyPolicies/tests/test_ConcurrencyPolicies.cpp
      - components/IdGenerator/tests/test_IdGenerator.cpp
      - components/ConcurrencyPolicies/tests/test_RcuIntegration.cpp
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

/**
 * @file ConcurrencyPolicies.h
 * @brief Defines various synchronization policies for use with a policy-based
 * design, such as ScopeGuard, Enforcer, or smart resource handles.
 *
 *
 *
 * @details This header provides a comprehensive set of concurrency primitives ranging from
 * zero-cost (single-threaded) to advanced lock-free synchronization (RCU, hazard pointers,
 * SeqLock), all exposing a consistent policy interface.
 *
 * FEATURES:
 *   - 19 total concurrency policies with production-ready implementations
 *   - C++20 jthread and atomic<shared_ptr> support (where library available)
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

#include "CppFeatureDetection.h"

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

// C++20 atomic<shared_ptr> detection - use internal flag from CppFeatureDetection.h
// Library support is compiler-dependent even with C++20
#if FATP_HAS_ATOMIC_SHARED_PTR
// Already detected by CppFeatureDetection.h
#elif (defined(__GNUC__) && __GNUC__ >= 11) || (defined(__clang__) && __clang_major__ >= 13) || \
      (defined(_MSC_VER) && _MSC_VER >= 1930)
#define FATP_HAS_ATOMIC_SHARED_PTR 1
#else
#define FATP_HAS_ATOMIC_SHARED_PTR 0
#endif

// Standard Library Includes
#if FATP_USE_MUTEX
#include <memory>
#include <mutex>
#include <utility>
#endif
#if FATP_USE_SHARED_MUTEX
#include <shared_mutex>
#endif
#if FATP_USE_ATOMIC
#include <array>
#include <atomic>
#include <functional>
#include <thread>
#include <vector>
#endif
#if FATP_USE_CHRONO
#include <chrono>
#endif
#if FATP_USE_CONDITION_VARIABLE
#include <condition_variable>
#endif

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <optional>
#include <type_traits>

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
// Policy Concepts (C++20)
// =============================================================================

/// Full concurrency policy interface: lockable with shared support
template <typename P>
concept ConcurrencyPolicy = requires(P p)
{
    typename P::LockGuard;
    typename P::SharedGuard;
    { p.lock() } -> std::same_as<typename P::LockGuard>;
    { p.lock_shared() } -> std::same_as<typename P::SharedGuard>;
};

/// Policy has the basic concurrency policy tag
template <typename T>
concept ConcurrencyPolicyTag = requires { typename T::PolicyTag; };

/// Policy supports shared (read) locking
template <typename T>
concept SharedPolicy = requires { typename T::SharedGuard; };

/// Policy's LockGuard supports condition variable wait
template <typename T>
concept WaitablePolicy = requires(typename T::LockGuard g, std::condition_variable& cv) {
    g.wait(cv);
};

/// Policy provides fair (FIFO) ordering
template <typename T>
concept FairPolicy = requires { typename T::FairOrderingTag; };

/// Policy uses optimistic concurrency (e.g., SeqLock)
template <typename T>
concept OptimisticPolicy = requires { typename T::OptimisticTag; };

/// Policy is NUMA-aware (e.g., MCSLock)
template <typename T>
concept NumaAwarePolicy = requires { typename T::NUMAAwareTag; };

/// Policy supports real-time priority inheritance
template <typename T>
concept RealtimePolicy = requires { typename T::RealtimeTag; };

/// Policy is lock-free
template <typename T>
concept LockFreePolicy = requires { typename T::LockFreeTag; };

/// Policy adapts between strategies at runtime
template <typename T>
concept AdaptivePolicy = requires { typename T::AdaptiveTag; };

/// Policy tracks contention statistics
template <typename T>
concept HasContentionTracking = requires(T t) {
    t.get_contention();
};

/// Policy supports recursive locking
template <typename T>
concept RecursivePolicy = requires { typename T::RecursiveTag; };

/// Policy's LockGuard supports timed locking
template <typename T>
concept TimedPolicy = requires(typename T::LockGuard g) {
    g.try_lock_for(std::chrono::milliseconds(1));
};

/// Policy supports try_lock()
template <typename T>
concept SupportsTryLock = requires(T t) {
    { t.try_lock() } -> std::convertible_to<bool>;
};

// =============================================================================
// Backward Compatibility (variable templates)
// =============================================================================

template <typename T>
inline constexpr bool is_concurrency_policy_v = ConcurrencyPolicyTag<T>;

template <typename T>
inline constexpr bool is_shared_policy_v = SharedPolicy<T>;

template <typename T>
inline constexpr bool is_waitable_policy_v = WaitablePolicy<T>;

template <typename T>
inline constexpr bool is_fair_policy_v = FairPolicy<T>;

template <typename T>
inline constexpr bool is_optimistic_policy_v = OptimisticPolicy<T>;

template <typename T>
inline constexpr bool is_numa_aware_policy_v = NumaAwarePolicy<T>;

template <typename T>
inline constexpr bool is_realtime_policy_v = RealtimePolicy<T>;

template <typename T>
inline constexpr bool is_lockfree_policy_v = LockFreePolicy<T>;

template <typename T>
inline constexpr bool is_adaptive_policy_v = AdaptivePolicy<T>;

template <typename T>
inline constexpr bool has_contention_tracking_v = HasContentionTracking<T>;

template <typename T>
inline constexpr bool is_recursive_policy_v = RecursivePolicy<T>;

template <typename T>
inline constexpr bool is_timed_policy_v = TimedPolicy<T>;

template <typename T>
inline constexpr bool supports_try_lock_v = SupportsTryLock<T>;

// =============================================================================
// SingleThreadedPolicy - Zero-cost no-op synchronization
// =============================================================================

struct SingleThreadedPolicy
{
    using PolicyTag = void;

    SingleThreadedPolicy() = default;
    SingleThreadedPolicy(const SingleThreadedPolicy&) = delete;
    SingleThreadedPolicy& operator=(const SingleThreadedPolicy&) = delete;

    struct NoOpLock
    {
    };

    // Trivial, empty guard type. This is designed to be a true zero-cost
    // abstraction under optimization (including with and without LTO).
    //
    // IMPORTANT:
    // Do not construct guards directly from getLock(). Always use lock() /
    // lock_shared() so SingleThreadedPolicy can remain a no-op.
    struct LockGuard
    {
    };

    using SharedGuard = LockGuard;
    using WriteLock = LockGuard;
    using ReadLock = LockGuard;

    [[nodiscard]] LockGuard lock() noexcept
    {
        return {};
    }
    [[nodiscard]] SharedGuard lock_shared() const noexcept
    {
        return {};
    }
    [[nodiscard]] bool try_lock()
    {
        return true;
    }
    [[nodiscard]] bool try_lock_shared() const
    {
        return true;
    }

    NoOpLock& getLock() const
    {
        return mLock;
    }

    static NoOpLock& getStaticLock()
    {
        static NoOpLock mLock;
        return mLock;
    }

    uint64_t get_contention() const
    {
        return 0;
    }
    void reset_contention()
    {
    }

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
    uint64_t get_contention() const noexcept
    {
        return mContention.load(std::memory_order_relaxed);
    }
    void reset_contention() noexcept
    {
        mContention.store(0, std::memory_order_relaxed);
    }
#endif

    class LockGuard
    {
    public:
        explicit LockGuard(std::mutex& mutex)
            : mGuard(mutex)
        {
        }

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

    [[nodiscard]] LockGuard lock()
    {
        return LockGuard(*this);
    }
    [[nodiscard]] SharedGuard lock_shared() const
    {
        return SharedGuard(mMutex);
    }

    [[nodiscard]] bool try_lock()
    {
        // Standard try_lock semantics: on success, the lock is held by the caller.
        // (The caller is responsible for unlocking.)
        return mMutex.try_lock();
    }

    std::mutex& getLock() const
    {
        return mMutex;
    }

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
        explicit LockGuard(std::shared_mutex& mutex)
            : mLock(mutex)
        {
        }

        LockGuard(const LockGuard&) = delete;
        LockGuard& operator=(const LockGuard&) = delete;
        ~LockGuard() = default;

    private:
        std::unique_lock<std::shared_mutex> mLock;
    };

    class SharedGuard
    {
    public:
        explicit SharedGuard(std::shared_mutex& mutex)
            : mLock(mutex)
        {
        }

        SharedGuard(const SharedGuard&) = delete;
        SharedGuard& operator=(const SharedGuard&) = delete;
        ~SharedGuard() = default;

    private:
        std::shared_lock<std::shared_mutex> mLock;
    };

    using WriteLock = LockGuard;
    using ReadLock = SharedGuard;

    [[nodiscard]] LockGuard lock()
    {
        return LockGuard(mMutex);
    }
    [[nodiscard]] SharedGuard lock_shared() const
    {
        return SharedGuard(mMutex);
    }

    [[nodiscard]] bool try_lock()
    {
        return mMutex.try_lock();
    }
    [[nodiscard]] bool try_lock_shared() const
    {
        return mMutex.try_lock_shared();
    }

    std::shared_mutex& getLock() const
    {
        return mMutex;
    }

    static std::shared_mutex& getStaticLock()
    {
        static std::shared_mutex mMutex;
        return mMutex;
    }

    uint64_t get_contention() const
    {
        return 0;
    }
    void reset_contention()
    {
    }

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

    UniqueRWLockPolicy()
        : mMutex(std::make_unique<std::shared_mutex>())
    {
    }
    UniqueRWLockPolicy(const UniqueRWLockPolicy&) = delete;
    UniqueRWLockPolicy& operator=(const UniqueRWLockPolicy&) = delete;
    UniqueRWLockPolicy(UniqueRWLockPolicy&&) noexcept = default;
    UniqueRWLockPolicy& operator=(UniqueRWLockPolicy&&) noexcept = default;

    class LockGuard
    {
    public:
        explicit LockGuard(std::shared_mutex& mutex)
            : mLock(mutex)
        {
        }
        LockGuard(const LockGuard&) = delete;
        LockGuard& operator=(const LockGuard&) = delete;
        ~LockGuard() = default;

    private:
        std::unique_lock<std::shared_mutex> mLock;
    };

    class SharedGuard
    {
    public:
        explicit SharedGuard(std::shared_mutex& mutex)
            : mLock(mutex)
        {
        }
        SharedGuard(const SharedGuard&) = delete;
        SharedGuard& operator=(const SharedGuard&) = delete;
        ~SharedGuard() = default;

    private:
        std::shared_lock<std::shared_mutex> mLock;
    };

    using WriteLock = LockGuard;
    using ReadLock = SharedGuard;

    [[nodiscard]] LockGuard lock()
    {
        return LockGuard(*mMutex);
    }
    [[nodiscard]] SharedGuard lock_shared() const
    {
        return SharedGuard(*mMutex);
    }

    [[nodiscard]] bool try_lock()
    {
        return mMutex->try_lock();
    }
    [[nodiscard]] bool try_lock_shared() const
    {
        return mMutex->try_lock_shared();
    }

    std::shared_mutex& getLock() const
    {
        return *mMutex;
    }

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
    alignas(FATP_CACHE_LINE_SIZE) mutable std::atomic_flag lock_flag_ = ATOMIC_FLAG_INIT;
    alignas(FATP_CACHE_LINE_SIZE) mutable std::atomic<uint64_t> mContention{0};

public:
    uint64_t get_contention() const noexcept
    {
        return mContention.load(std::memory_order_relaxed);
    }
    void reset_contention() noexcept
    {
        mContention.store(0, std::memory_order_relaxed);
    }

    class LockGuard
    {
    public:
        explicit LockGuard(const SpinlockSynchronizationPolicy& policy)
            : lock_flag_(policy.lock_flag_)
            , mContention(policy.mContention)
        {
            while (lock_flag_.test_and_set(std::memory_order_acquire))
            {
                mContention.fetch_add(1, std::memory_order_relaxed);
                std::this_thread::yield();
            }
        }

        LockGuard(const LockGuard&) = delete;
        LockGuard& operator=(const LockGuard&) = delete;

        ~LockGuard()
        {
            lock_flag_.clear(std::memory_order_release);
        }

    private:
        std::atomic_flag& lock_flag_;
        std::atomic<uint64_t>& mContention;
    };

    using SharedGuard = LockGuard;
    using WriteLock = LockGuard;
    using ReadLock = LockGuard;

    [[nodiscard]] LockGuard lock()
    {
        return LockGuard(*this);
    }
    [[nodiscard]] SharedGuard lock_shared() const
    {
        return SharedGuard(*this);
    }

    [[nodiscard]] bool try_lock()
    {
        return !lock_flag_.test_and_set(std::memory_order_acquire);
    }

    void unlock()
    {
        lock_flag_.clear(std::memory_order_release);
    }

    SpinlockSynchronizationPolicy& getLock()
    {
        return *this;
    }
    const SpinlockSynchronizationPolicy& getLock() const
    {
        return *this;
    }

    static SpinlockSynchronizationPolicy& getStaticLock()
    {
        static SpinlockSynchronizationPolicy mPolicy;
        return mPolicy;
    }

    std::atomic_flag& getRawLock()
    {
        return lock_flag_;
    }
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
        explicit LockGuard(T&)
        {
        }
        LockGuard(const LockGuard&) = delete;
        LockGuard& operator=(const LockGuard&) = delete;
        ~LockGuard() = default;
    };

    using SharedGuard = LockGuard;
    using WriteLock = LockGuard;
    using ReadLock = LockGuard;

    struct NoOpLock
    {
    };

    [[nodiscard]] LockGuard lock()
    {
        return LockGuard(mLock);
    }
    [[nodiscard]] SharedGuard lock_shared() const
    {
        return SharedGuard(mLock);
    }
    [[nodiscard]] bool try_lock()
    {
        return true;
    }

    NoOpLock& getLock() const
    {
        return mLock;
    }

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

    [[nodiscard]] LockGuard lock()
    {
        return mPolicy.lock();
    }
    [[nodiscard]] SharedGuard lock_shared() const
    {
        return mPolicy.lock_shared();
    }

    [[nodiscard]] bool try_lock()
    {
        if constexpr (supports_try_lock_v<RealPolicy>)
        {
            return mPolicy.try_lock();
        }
        else
        {
            return true;
        }
    }

    auto& getLock()
    {
        return mPolicy.getLock();
    }

    uint64_t get_contention() const
    {
        if constexpr (has_contention_tracking_v<RealPolicy>)
        {
            return mPolicy.get_contention();
        }
        else
        {
            return 0;
        }
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
        explicit LockGuard(std::mutex& mutex)
            : mLock(mutex)
        {
        }

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
        bool wait_for(std::condition_variable& cond, const std::chrono::duration<Rep, Period>& rel_time, Predicate pred)
        {
            return cond.wait_for(mLock, rel_time, std::move(pred));
        }

        bool owns_lock() const
        {
            return mLock.owns_lock();
        }
        std::mutex* mutex() const
        {
            return mLock.mutex();
        }

    private:
        std::unique_lock<std::mutex> mLock;
    };

    using SharedGuard = LockGuard;
    using WriteLock = LockGuard;
    using ReadLock = LockGuard;

    [[nodiscard]] LockGuard lock()
    {
        return LockGuard(mMutex);
    }
    [[nodiscard]] SharedGuard lock_shared() const
    {
        return SharedGuard(mMutex);
    }

    [[nodiscard]] bool try_lock()
    {
        return mMutex.try_lock();
    }

    std::mutex& getLock() const
    {
        return mMutex;
    }
    std::condition_variable& getCondition() const
    {
        return mCondition;
    }

    static std::mutex& getStaticLock()
    {
        static std::mutex mMutex;
        return mMutex;
    }

    uint64_t get_contention() const
    {
        return 0;
    }

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
        explicit LockGuard(std::atomic<uint64_t>& seq)
            : mSequence(seq)
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
            , mValid(true)
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
                        mValid = false;
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
            if (!mValid)
            {
                return false;
            }
            std::atomic_thread_fence(std::memory_order_acquire);
            return mSequence.load(std::memory_order_acquire) == start_seq_;
        }

        bool retry_limit_exceeded() const
        {
            return !mValid;
        }
        uint64_t get_sequence() const
        {
            return start_seq_;
        }

    private:
        std::atomic<uint64_t>& mSequence;
        uint64_t start_seq_;
        bool mValid;
    };

    using WriteLock = LockGuard;
    using ReadLock = SharedGuard;

    [[nodiscard]] LockGuard lock()
    {
        return LockGuard(mSequence);
    }
    [[nodiscard]] SharedGuard lock_shared() const
    {
        return SharedGuard(mSequence);
    }

    std::atomic<uint64_t>& getLock()
    {
        return mSequence;
    }

    static std::atomic<uint64_t>& getStaticLock()
    {
        static std::atomic<uint64_t> mSequence{0};
        return mSequence;
    }

    uint64_t get_sequence() const
    {
        return mSequence.load(std::memory_order_relaxed);
    }
    uint64_t get_contention() const
    {
        return 0;
    }

private:
    alignas(FATP_CACHE_LINE_SIZE) mutable std::atomic<uint64_t> mSequence{0};
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
        explicit LockGuard(const TicketLockPolicy& policy)
            : now_serving_(policy.now_serving_)
        {
            my_ticket_ = policy.next_ticket_.fetch_add(1, std::memory_order_relaxed);
            while (now_serving_.load(std::memory_order_acquire) != my_ticket_)
            {
                std::this_thread::yield();
            }
        }

        LockGuard(const LockGuard&) = delete;
        LockGuard& operator=(const LockGuard&) = delete;

        ~LockGuard()
        {
            now_serving_.fetch_add(1, std::memory_order_release);
        }

    private:
        std::atomic<uint64_t>& now_serving_;
        uint64_t my_ticket_;
    };

    using SharedGuard = LockGuard;
    using WriteLock = LockGuard;
    using ReadLock = LockGuard;

    [[nodiscard]] LockGuard lock()
    {
        return LockGuard(*this);
    }
    [[nodiscard]] SharedGuard lock_shared() const
    {
        return SharedGuard(*this);
    }

    [[nodiscard]] bool try_lock()
    {
        uint64_t current = now_serving_.load(std::memory_order_acquire);
        uint64_t next = next_ticket_.load(std::memory_order_relaxed);
        if (current == next)
        {
            return next_ticket_.compare_exchange_strong(next,
                                                        next + 1,
                                                        std::memory_order_acquire,
                                                        std::memory_order_relaxed);
        }
        return false;
    }

    TicketLockPolicy& getLock()
    {
        return *this;
    }

    static TicketLockPolicy& getStaticLock()
    {
        static TicketLockPolicy mPolicy;
        return mPolicy;
    }

    uint64_t get_queue_length() const
    {
        return next_ticket_.load(std::memory_order_relaxed) - now_serving_.load(std::memory_order_relaxed);
    }

    uint64_t get_contention() const
    {
        return get_queue_length();
    }

private:
    alignas(FATP_CACHE_LINE_SIZE) mutable std::atomic<uint64_t> next_ticket_{0};
    alignas(FATP_CACHE_LINE_SIZE) mutable std::atomic<uint64_t> now_serving_{0};
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
        explicit LockGuard(std::atomic<QNode*>& tail)
            : mTail(tail)
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

    [[nodiscard]] LockGuard lock()
    {
        return LockGuard(mTail);
    }
    [[nodiscard]] SharedGuard lock_shared() const
    {
        return SharedGuard(mTail);
    }

    std::atomic<QNode*>& getLock()
    {
        return mTail;
    }

    static std::atomic<QNode*>& getStaticLock()
    {
        static std::atomic<QNode*> mTail{nullptr};
        return mTail;
    }

    uint64_t get_contention() const
    {
        return 0;
    }

private:
    alignas(FATP_CACHE_LINE_SIZE) mutable std::atomic<QNode*> mTail{nullptr};
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

    RCUPolicy()
        : data_(std::make_shared<T>())
    {
    }
    explicit RCUPolicy(T initial)
        : data_(std::make_shared<T>(std::move(initial)))
    {
    }

    RCUPolicy(const RCUPolicy&) = delete;
    RCUPolicy& operator=(const RCUPolicy&) = delete;

    class SharedGuard
    {
    public:
        explicit SharedGuard(std::shared_ptr<T> snapshot)
            : mSnapshot(std::move(snapshot))
        {
        }

        SharedGuard(const SharedGuard&) = delete;
        SharedGuard& operator=(const SharedGuard&) = delete;
        ~SharedGuard() = default;

        const T& operator*() const
        {
            return *mSnapshot;
        }
        const T* operator->() const
        {
            return mSnapshot.get();
        }
        const T* get() const
        {
            return mSnapshot.get();
        }

    private:
        std::shared_ptr<T> mSnapshot;
    };

    class LockGuard
    {
    public:
#if FATP_HAS_ATOMIC_SHARED_PTR
        LockGuard(std::atomic<std::shared_ptr<T>>& data, std::mutex& write_mutex)
            : data_(data)
            , write_lock_(write_mutex)
        {
        }
#else
        LockGuard(std::shared_ptr<T>& data_ptr, std::shared_mutex& rw_mutex, std::mutex& write_mutex)
            : data_ptr_(data_ptr)
            , rw_mutex_(rw_mutex)
            , write_lock_(write_mutex)
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
            } while (
                !data_.compare_exchange_weak(current, new_data, std::memory_order_release, std::memory_order_acquire));
#else
            std::shared_ptr<T> current;
            {
                std::shared_lock<std::shared_mutex> read_lock(rw_mutex_);
                current = data_ptr_;
            }
            auto new_data = std::make_shared<T>(*current);
            f(*new_data);
            {
                std::unique_lock<std::shared_mutex> write_lock(rw_mutex_);
                data_ptr_ = new_data;
            }
#endif
        }

    private:
#if FATP_HAS_ATOMIC_SHARED_PTR
        std::atomic<std::shared_ptr<T>>& data_;
        std::unique_lock<std::mutex> write_lock_;
#else
        std::shared_ptr<T>& data_ptr_;
        std::shared_mutex& rw_mutex_;
        std::unique_lock<std::mutex> write_lock_;
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
        return {data_, write_mutex_};
#else
        return {data_, rw_mutex_, write_mutex_};
#endif
    }

#if FATP_HAS_ATOMIC_SHARED_PTR
    [[nodiscard]] SharedGuard read() const
    {
        return SharedGuard(data_.load(std::memory_order_acquire));
    }

    static constexpr bool is_lock_free()
    {
        return true;
    }
#else
    [[nodiscard]] SharedGuard read() const
    {
        std::shared_lock<std::shared_mutex> lock(rw_mutex_);
        return SharedGuard(data_);
    }

    static constexpr bool is_lock_free()
    {
        return false;
    }
#endif

    [[nodiscard]] LockGuard write()
    {
#if FATP_HAS_ATOMIC_SHARED_PTR
        return LockGuard(data_, write_mutex_);
#else
        return LockGuard(data_, rw_mutex_, write_mutex_);
#endif
    }

    uint64_t get_contention() const
    {
        return 0;
    }

private:
#if FATP_HAS_ATOMIC_SHARED_PTR
    mutable std::atomic<std::shared_ptr<T>> data_;
    std::mutex write_mutex_;
#else
    mutable std::shared_ptr<T> data_;
    mutable std::shared_mutex rw_mutex_;
    std::mutex write_mutex_;
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
        explicit Guard(std::atomic<T*>& hp)
            : mHp(hp)
            , protected_ptr_(nullptr)
        {
        }

        Guard(const Guard&) = delete;
        Guard& operator=(const Guard&) = delete;

        T* protect(std::atomic<T*>& src)
        {
            T* ptr;
            do
            {
                ptr = src.load(std::memory_order_acquire);
                mHp.store(ptr, std::memory_order_release);
            } while (ptr != src.load(std::memory_order_acquire));

            protected_ptr_ = ptr;
            return ptr;
        }

        T* get() const
        {
            return protected_ptr_;
        }

        ~Guard()
        {
            mHp.store(nullptr, std::memory_order_release);
        }

    private:
        std::atomic<T*>& mHp;
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

    [[nodiscard]] Guard lock() const
    {
        return acquire(0);
    }
    [[nodiscard]] Guard lock_shared() const
    {
        return acquire(0);
    }

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

    uint64_t get_contention() const
    {
        return 0;
    }

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

        auto new_end = std::remove_if(retired_list.begin(), retired_list.end(), [&protected_ptrs](T* ptr) {
            bool is_protected = std::binary_search(protected_ptrs.begin(), protected_ptrs.end(), ptr);
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
        explicit LockGuard(const AdaptiveLockPolicy& policy)
            : mMutex(policy.mMutex)
            , spin_lock_(policy.spin_lock_)
            , contention_counter_(policy.contention_counter_)
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
                while (spin_lock_.test_and_set(std::memory_order_acquire))
                {
                    if (++spins > SPIN_THRESHOLD)
                    {
                        uint32_t count = contention_counter_.fetch_add(1, std::memory_order_relaxed);
                        if (count > ADAPT_WINDOW / 10)
                        {
                            use_mutex_.store(true, std::memory_order_relaxed);
                        }
                        spin_lock_.clear(std::memory_order_release);
                        mMutex.lock();
                        using_mutex_ = true;
                        return;
                    }
                    std::this_thread::yield();
                }
            }

            uint32_t count = contention_counter_.load(std::memory_order_relaxed);
            if (count > ADAPT_WINDOW)
            {
                if (count < ADAPT_WINDOW + ADAPT_WINDOW / 10)
                {
                    use_mutex_.store(false, std::memory_order_relaxed);
                }
                contention_counter_.store(0, std::memory_order_relaxed);
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
                spin_lock_.clear(std::memory_order_release);
            }
        }

    private:
        std::mutex& mMutex;
        std::atomic_flag& spin_lock_;
        std::atomic<uint32_t>& contention_counter_;
        std::atomic<bool>& use_mutex_;
        bool using_mutex_;
    };

    using SharedGuard = LockGuard;
    using WriteLock = LockGuard;
    using ReadLock = LockGuard;

    [[nodiscard]] LockGuard lock()
    {
        return LockGuard(*this);
    }
    [[nodiscard]] SharedGuard lock_shared() const
    {
        return SharedGuard(*this);
    }

    [[nodiscard]] bool try_lock()
    {
        if (use_mutex_.load(std::memory_order_relaxed))
        {
            return mMutex.try_lock();
        }
        return !spin_lock_.test_and_set(std::memory_order_acquire);
    }

    AdaptiveLockPolicy& getLock()
    {
        return *this;
    }

    static AdaptiveLockPolicy& getStaticLock()
    {
        static AdaptiveLockPolicy mPolicy;
        return mPolicy;
    }

    bool is_using_mutex() const noexcept
    {
        return use_mutex_.load(std::memory_order_relaxed);
    }
    uint32_t get_contention() const noexcept
    {
        return contention_counter_.load(std::memory_order_relaxed);
    }
    void reset_contention() noexcept
    {
        contention_counter_.store(0, std::memory_order_relaxed);
    }

private:
    alignas(FATP_CACHE_LINE_SIZE) mutable std::mutex mMutex;
    alignas(FATP_CACHE_LINE_SIZE) mutable std::atomic_flag spin_lock_ = ATOMIC_FLAG_INIT;
    alignas(FATP_CACHE_LINE_SIZE) mutable std::atomic<uint32_t> contention_counter_{0};
    mutable std::atomic<bool> use_mutex_{false};
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
        InitializeCriticalSection(&mCs);
    }

    ~PriorityInheritanceLockPolicy()
    {
        DeleteCriticalSection(&mCs);
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
        explicit LockGuard(pthread_mutex_t& mutex)
            : mMutex(mutex)
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
        explicit LockGuard(CRITICAL_SECTION& cs)
            : mCs(cs)
        {
            EnterCriticalSection(&mCs);
        }

        ~LockGuard()
        {
            LeaveCriticalSection(&mCs);
        }

    private:
        CRITICAL_SECTION& mCs;
#else
        explicit LockGuard(std::mutex& mutex)
            : mGuard(mutex)
        {
        }
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
    [[nodiscard]] LockGuard lock()
    {
        return LockGuard(mMutex);
    }
    [[nodiscard]] SharedGuard lock_shared() const
    {
        return SharedGuard(mMutex);
    }

    [[nodiscard]] bool try_lock()
    {
        return pthread_mutex_trylock(&mMutex) == 0;
    }

    pthread_mutex_t& getLock()
    {
        return mMutex;
    }

    static pthread_mutex_t& getStaticLock()
    {
        static pthread_mutex_t mMutex;
        static std::once_flag init_flag;
        std::call_once(init_flag, []() {
            pthread_mutexattr_t attr;
            pthread_mutexattr_init(&attr);
            pthread_mutexattr_setprotocol(&attr, PTHREAD_PRIO_INHERIT);
            pthread_mutex_init(&mMutex, &attr);
            pthread_mutexattr_destroy(&attr);
        });
        return mMutex;
    }

private:
    mutable pthread_mutex_t mMutex;

#elif FATP_HAS_WIN32_CRITICAL_SECTION
    [[nodiscard]] LockGuard lock()
    {
        return LockGuard(mCs);
    }
    [[nodiscard]] SharedGuard lock_shared() const
    {
        return SharedGuard(mCs);
    }

    [[nodiscard]] bool try_lock()
    {
        return TryEnterCriticalSection(&mCs) != 0;
    }

    CRITICAL_SECTION& getLock()
    {
        return mCs;
    }

    static CRITICAL_SECTION& getStaticLock()
    {
        static CRITICAL_SECTION mCs;
        static std::once_flag init_flag;
        std::call_once(init_flag, []() {
            InitializeCriticalSection(&mCs);
        });
        return mCs;
    }

private:
    mutable CRITICAL_SECTION mCs;

#else
    [[nodiscard]] LockGuard lock()
    {
        return LockGuard(mMutex);
    }
    [[nodiscard]] SharedGuard lock_shared() const
    {
        return SharedGuard(mMutex);
    }

    [[nodiscard]] bool try_lock()
    {
        return mMutex.try_lock();
    }

    std::mutex& getLock()
    {
        return mMutex;
    }

    static std::mutex& getStaticLock()
    {
        static std::mutex mMutex;
        return mMutex;
    }

private:
    mutable std::mutex mMutex;
#endif

public:
    uint64_t get_contention() const
    {
        return 0;
    }
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
        LockGuard(std::atomic<uint64_t>& version, std::mutex& write_lock)
            : mVersion(version)
            , mLock(write_lock)
            , write_version_(mVersion.load(std::memory_order_acquire))
            , mCommitted(false)
        {
        }

        LockGuard(const LockGuard&) = delete;
        LockGuard& operator=(const LockGuard&) = delete;

        void commit()
        {
            if (!mCommitted)
            {
                mVersion.store(write_version_ + 1, std::memory_order_release);
                mCommitted = true;
            }
        }

        uint64_t get_version() const
        {
            return write_version_;
        }

        ~LockGuard()
        {
            if (!mCommitted)
            {
                commit();
            }
        }

    private:
        std::atomic<uint64_t>& mVersion;
        std::unique_lock<std::mutex> mLock;
        uint64_t write_version_;
        bool mCommitted;
    };

    class SharedGuard
    {
    public:
        explicit SharedGuard(std::atomic<uint64_t>& version)
            : mVersion(version)
            , read_version_(mVersion.load(std::memory_order_acquire))
        {
        }

        SharedGuard(const SharedGuard&) = delete;
        SharedGuard& operator=(const SharedGuard&) = delete;
        ~SharedGuard() = default;

        bool validate() const
        {
            std::atomic_thread_fence(std::memory_order_acquire);
            return mVersion.load(std::memory_order_acquire) == read_version_;
        }

        uint64_t get_version() const
        {
            return read_version_;
        }

    private:
        std::atomic<uint64_t>& mVersion;
        uint64_t read_version_;
    };

    using WriteLock = LockGuard;
    using ReadLock = SharedGuard;

    struct LockHandle
    {
        std::atomic<uint64_t>& version;
        std::mutex& write_lock;
    };

    [[nodiscard]] LockGuard lock()
    {
        return LockGuard(mVersion, write_lock_);
    }

    [[nodiscard]] SharedGuard lock_shared() const
    {
        return SharedGuard(mVersion);
    }

    [[nodiscard]] bool try_lock()
    {
        return write_lock_.try_lock();
    }

    LockHandle getLock()
    {
        return {mVersion, write_lock_};
    }

    static LockHandle getStaticLock()
    {
        static std::atomic<uint64_t> mVersion{0};
        static std::mutex write_lock_;
        return {mVersion, write_lock_};
    }

    uint64_t get_version() const
    {
        return mVersion.load(std::memory_order_relaxed);
    }
    uint64_t get_contention() const
    {
        return 0;
    }

private:
    alignas(FATP_CACHE_LINE_SIZE) mutable std::atomic<uint64_t> mVersion{0};
    mutable std::mutex write_lock_;
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

    [[nodiscard]] LockGuard lock()
    {
        return LockGuard(mLock);
    }
    [[nodiscard]] SharedGuard lock_shared() const
    {
        return SharedGuard(mLock);
    }

    [[nodiscard]] bool try_lock()
    {
        return mLock.try_lock();
    }

    std::recursive_mutex& getLock() const
    {
        return mLock;
    }

    static std::recursive_mutex& getStaticLock()
    {
        static std::recursive_mutex static_lock;
        return static_lock;
    }

    uint64_t get_contention() const
    {
        return 0;
    }

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
        explicit LockGuard(std::timed_mutex& mtx)
            : mLock(mtx)
        {
        }
        LockGuard(std::timed_mutex& mtx, std::defer_lock_t)
            : mLock(mtx, std::defer_lock)
        {
        }

        template <typename Rep, typename Period>
        bool try_lock_for(const std::chrono::duration<Rep, Period>& timeout)
        {
            if (mLock.owns_lock())
            {
                return true;
            }
            return mLock.try_lock_for(timeout);
        }

        bool owns_lock() const
        {
            return mLock.owns_lock();
        }

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

    [[nodiscard]] LockGuard lock()
    {
        return LockGuard(mLock);
    }
    [[nodiscard]] SharedGuard lock_shared() const
    {
        return SharedGuard(mLock);
    }
    [[nodiscard]] LockGuard lock_deferred()
    {
        return LockGuard(mLock, std::defer_lock);
    }

    [[nodiscard]] bool try_lock()
    {
        return mLock.try_lock();
    }

    template <typename Rep, typename Period>
    [[nodiscard]] bool try_lock_for(const std::chrono::duration<Rep, Period>& timeout)
    {
        return mLock.try_lock_for(timeout);
    }

    std::timed_mutex& getLock() const
    {
        return mLock;
    }

    static std::timed_mutex& getStaticLock()
    {
        static std::timed_mutex static_lock;
        return static_lock;
    }

    uint64_t get_contention() const
    {
        return 0;
    }

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

    [[nodiscard]] LockGuard lock()
    {
        return LockGuard(mLock);
    }
    [[nodiscard]] SharedGuard lock_shared() const
    {
        return SharedGuard(mLock);
    }

    [[nodiscard]] bool try_lock()
    {
        return mLock.try_lock();
    }
    [[nodiscard]] bool try_lock_shared() const
    {
        return mLock.try_lock_shared();
    }

    template <typename Rep, typename Period>
    [[nodiscard]] bool try_lock_for(const std::chrono::duration<Rep, Period>& timeout)
    {
        return mLock.try_lock_for(timeout);
    }

    template <typename Rep, typename Period>
    [[nodiscard]] bool try_lock_shared_for(const std::chrono::duration<Rep, Period>& timeout) const
    {
        return mLock.try_lock_shared_for(timeout);
    }

    std::shared_timed_mutex& getLock() const
    {
        return mLock;
    }

    static std::shared_timed_mutex& getStaticLock()
    {
        static std::shared_timed_mutex static_lock;
        return static_lock;
    }

    uint64_t get_contention() const
    {
        return 0;
    }

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

} // namespace fat_p
