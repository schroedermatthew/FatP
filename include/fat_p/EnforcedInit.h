#pragma once

/*
FATP_META:
  meta_version: 1
  component: EnforcedInit
  file_role: public_header
  path: include/fat_p/EnforcedInit.h
  namespace: fat_p
  layer: Foundation
  summary: "Public header for EnforcedInit."
  api_stability: in_work
  related:
    docs_search: "EnforcedInit"
    tests:
      - components/EnforcedInit/tests/test_EnforcedInit.cpp
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 2
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/

/**
 * @file EnforcedInit.h
 * @brief Provides the EnforcedInit<T> wrapper to enforce that an object is
 * explicitly initialized exactly once before it can be accessed.
 *
 *
 *
 * @details This utility leverages C++17's **std::optional<T>** for safe,
 * standardized memory management and lifecycle control. The library's
 * **contextual contract system** is used to enforce the core contract:
 * preventing access before initialization and preventing multiple calls
 * to init(). Supports conditional thread-safety via ConcurrencyPolicy,
 * customizable checks via CheckPolicy, and optional reset via ResetPolicy.
 * For trivial T, uses union+bool storage to optimize size/perf.
 * Lazy init support; move semantics if T movable.
 * Static asserts for T requirements (destructible, etc.).
 *
 * @version 2.1 - MSVC COMPILATION FIX
 * All critical issues fixed:
 * - Fixed const-correctness with getLock() calls
 * - Policies are default-constructed, not copied (correct design)
 * - Member functions defined outside class to avoid MSVC template parsing issues
 * - Removed dangerous const lazy_init overload
 * - Fixed ConditionVarPolicy deadlock
 * - Fixed SFINAE ambiguity in get() overloads
 * - Added noexcept specifications where appropriate
 */

#if !defined(FATP_USE_OPTIONAL)
#define FATP_USE_OPTIONAL 1
#endif

#if !defined(FATP_USE_ATOMIC)
#define FATP_USE_ATOMIC 1
#endif

#if FATP_USE_OPTIONAL
#include <optional>
#endif

#if FATP_USE_ATOMIC
#include <atomic>
#endif

#include <chrono>
#include <condition_variable>
#include <functional>
#include <initializer_list>
#include <thread>
#include <type_traits>
#include <utility>

#include "ConcurrencyPolicies.h"
#include "enforce.h"
#include "Expected.h"

namespace fat_p
{
// --- Policies for Customization ---
struct DefaultCheckPolicy
{
    template <typename T, typename... Args>
    static void pre_init_check(const Args&...) noexcept
    {
    }
    template <typename T>
    static void post_init_check(const T&) noexcept
    {
    }
};

struct NoResetPolicy
{
};
struct AllowResetPolicy
{
};

template <typename... Policies>
struct PolicyPack
{
    template <typename T, typename... Args>
    static void pre_init_check(Args&&... args)
    {
        (Policies::template pre_init_check<T>(std::forward<Args>(args)...), ...);
    }

    template <typename T>
    static void post_init_check(const T& value) noexcept
    {
        (Policies::template post_init_check<T>(value), ...);
    }
};

// Storage Policies
struct OptionalStoragePolicy
{
    template <typename T>
    using type = std::optional<T>;
};

struct UnionStoragePolicy
{
    template <typename T>
    struct type
    {
        bool init = false;
        union
        {
            T val;
        };

        constexpr type() noexcept
            : init(false)
        {
        }

        ~type() noexcept(std::is_nothrow_destructible_v<T>)
        {
            if (init)
            {
                val.~T();
            }
        }

        template <typename... Args>
        void emplace(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
        {
            new (&val) T(std::forward<Args>(args)...);
            init = true;
        }

        T& value() noexcept
        {
            return val;
        }
        const T& value() const noexcept
        {
            return val;
        }
        explicit operator bool() const noexcept
        {
            return init;
        }

        void reset() noexcept(std::is_nothrow_destructible_v<T>)
        {
            if (init)
            {
                val.~T();
                init = false;
            }
        }
    };
};

#if FATP_USE_ATOMIC
struct AtomicPolicy
{
    mutable std::atomic<bool> mInitFlag{false};

    class LockGuard
    {
    public:
        explicit LockGuard(std::atomic<bool>& flag) noexcept
            : mFlag(flag)
        {
            bool expected = false;
            while (!mFlag.compare_exchange_weak(expected, true, std::memory_order_acquire, std::memory_order_relaxed))
            {
                expected = false;
                std::this_thread::yield();
            }
        }

        ~LockGuard() noexcept
        {
            mFlag.store(false, std::memory_order_release);
        }

        LockGuard(const LockGuard&) = delete;
        LockGuard& operator=(const LockGuard&) = delete;

    private:
        std::atomic<bool>& mFlag;
    };

    using SharedGuard = LockGuard;

    std::atomic<bool>& getLock() const noexcept
    {
        return mInitFlag;
    }

    [[nodiscard]] LockGuard lock() noexcept
    {
        return LockGuard(getLock());
    }
    [[nodiscard]] SharedGuard lock_shared() const noexcept
    {
        return SharedGuard(getLock());
    }
};
#endif

struct ConditionVarPolicy
{
    mutable std::mutex mCvMutex;
    mutable std::condition_variable mCv;
    mutable std::atomic<bool> mInitializedFlag{false};

    using LockGuard = std::lock_guard<std::mutex>;
    using SharedGuard = std::lock_guard<std::mutex>;

    std::mutex& getLock() const noexcept
    {
        return mCvMutex;
    }

    [[nodiscard]] LockGuard lock() noexcept
    {
        return LockGuard(getLock());
    }
    [[nodiscard]] SharedGuard lock_shared() const noexcept
    {
        return SharedGuard(getLock());
    }
    template <typename Duration>
    bool wait_for_init(const Duration& timeout) const
    {
        std::unique_lock<std::mutex> lock(mCvMutex);
        return mCv.wait_for(lock, timeout, [this] {
            return mInitializedFlag.load(std::memory_order_acquire);
        });
    }

    // Called while holding mCvMutex lock (from init())
    void notify_init() noexcept
    {
        mInitializedFlag.store(true, std::memory_order_release);
        // Note: mCv.notify_all() should be called AFTER releasing the lock
        // We do it here because unlock happens when guard destructs
        mCv.notify_all();
    }

    bool is_notification_initialized() const noexcept
    {
        return mInitializedFlag.load(std::memory_order_acquire);
    }
};

// --- EnforcedInit Class ---
template <typename T,
          typename ConcurrencyPolicy = SingleThreadedPolicy,
          typename CheckPolicy = DefaultCheckPolicy,
          typename ResetPolicy = NoResetPolicy,
          typename StoragePolicy = OptionalStoragePolicy>
class EnforcedInit : public ConcurrencyPolicy
{
public:
    using value_type = T;
    static_assert(std::is_destructible_v<T>, "T must be destructible");

    EnforcedInit() noexcept = default;

    // Copy constructor - policies are NOT copyable, so default-construct them
    EnforcedInit(const EnforcedInit& other);

    // Copy assignment
    EnforcedInit& operator=(const EnforcedInit& other);

    // Move constructor - policies are NOT movable, so default-construct them
    EnforcedInit(EnforcedInit&& other) noexcept(
        std::is_nothrow_move_constructible_v<typename StoragePolicy::template type<T>>);

    // Move assignment
    EnforcedInit& operator=(EnforcedInit&& other) noexcept(
        std::is_nothrow_move_assignable_v<typename StoragePolicy::template type<T>>);

    // Core methods
    template <typename... Args>
    Expected<void, std::string> init(Args&&... args);

    template <typename U>
        requires std::is_constructible_v<T, std::initializer_list<U>>
    Expected<void, std::string> init(std::initializer_list<U> ilist);

    Expected<void, std::string> reset() noexcept(std::is_same_v<ResetPolicy, AllowResetPolicy>);

    template <typename F>
        requires std::is_invocable_r_v<T, F>
    void lazy_init(F&& f);

    template <typename F>
        requires (std::is_invocable_r_v<T, F> && !std::is_same_v<std::decay_t<F>, T>)
    [[nodiscard]] T& get(F&& f);

    [[nodiscard]] T& get();
    [[nodiscard]] const T& get() const;

    [[nodiscard]] T& operator*();
    [[nodiscard]] const T& operator*() const;
    [[nodiscard]] T* operator->();
    [[nodiscard]] const T* operator->() const;

    [[nodiscard]] bool is_initialized() const noexcept;

    template <typename Duration = std::chrono::seconds>
    bool wait_for_init(const Duration& timeout = Duration(30)) const;

private:
    void notify_init() noexcept;
    typename StoragePolicy::template type<T> mValue;
};

// --- Member Function Definitions ---

template <typename T, typename ConcurrencyPolicy, typename CheckPolicy, typename ResetPolicy, typename StoragePolicy>
EnforcedInit<T, ConcurrencyPolicy, CheckPolicy, ResetPolicy, StoragePolicy>::EnforcedInit(const EnforcedInit& other)
    : ConcurrencyPolicy()
{ // Default-construct policy, don't copy it
    static_assert(std::is_copy_constructible_v<T>, "T must be copyable for copy ctor");
    [[maybe_unused]] auto guard = other.lock_shared();
    mValue = other.mValue;
}

template <typename T, typename ConcurrencyPolicy, typename CheckPolicy, typename ResetPolicy, typename StoragePolicy>
EnforcedInit<T, ConcurrencyPolicy, CheckPolicy, ResetPolicy, StoragePolicy>&
EnforcedInit<T, ConcurrencyPolicy, CheckPolicy, ResetPolicy, StoragePolicy>::operator=(const EnforcedInit& other)
{
    static_assert(std::is_copy_assignable_v<T>, "T must be copyable for copy assign");
    if (this != &other)
    {
        // Avoid UB from relational comparison of unrelated pointers; std::less provides
        // a strict total ordering for pointers.
        if (std::less<const void*>{}(this, &other))
        {
            [[maybe_unused]] auto guard_this = this->lock();
            [[maybe_unused]] auto guard_other = other.lock_shared();
            mValue = other.mValue;
        }
        else
        {
            [[maybe_unused]] auto guard_other = other.lock_shared();
            [[maybe_unused]] auto guard_this = this->lock();
            mValue = other.mValue;
        }
    }
    return *this;
}

template <typename T, typename ConcurrencyPolicy, typename CheckPolicy, typename ResetPolicy, typename StoragePolicy>
EnforcedInit<T, ConcurrencyPolicy, CheckPolicy, ResetPolicy, StoragePolicy>::EnforcedInit(
    EnforcedInit&& other) noexcept(std::is_nothrow_move_constructible_v<typename StoragePolicy::template type<T>>)
    : ConcurrencyPolicy()
{ // Default-construct policy, don't move it
    static_assert(std::is_move_constructible_v<T>, "T must be movable for move ctor");
    if constexpr (std::is_same_v<ConcurrencyPolicy, SingleThreadedPolicy>)
    {
        mValue = std::move(other.mValue);
    }
    else
    {
        [[maybe_unused]] auto guard = other.lock();
        mValue = std::move(other.mValue);
    }
}

template <typename T, typename ConcurrencyPolicy, typename CheckPolicy, typename ResetPolicy, typename StoragePolicy>
EnforcedInit<T, ConcurrencyPolicy, CheckPolicy, ResetPolicy, StoragePolicy>&
EnforcedInit<T, ConcurrencyPolicy, CheckPolicy, ResetPolicy, StoragePolicy>::operator=(EnforcedInit&& other) noexcept(
    std::is_nothrow_move_assignable_v<typename StoragePolicy::template type<T>>)
{
    static_assert(std::is_move_assignable_v<T>, "T must be movable for move assign");
    if (this != &other)
    {
        if constexpr (std::is_same_v<ConcurrencyPolicy, SingleThreadedPolicy>)
        {
            mValue = std::move(other.mValue);
        }
        else
        {
            // Avoid UB from relational comparison of unrelated pointers; std::less provides
            // a strict total ordering for pointers.
            if (std::less<const void*>{}(this, &other))
            {
                [[maybe_unused]] auto guard_this = this->lock();
                [[maybe_unused]] auto guard_other = other.lock();
                mValue = std::move(other.mValue);
            }
            else
            {
                [[maybe_unused]] auto guard_other = other.lock();
                [[maybe_unused]] auto guard_this = this->lock();
                mValue = std::move(other.mValue);
            }
        }
    }
    return *this;
}

template <typename T, typename ConcurrencyPolicy, typename CheckPolicy, typename ResetPolicy, typename StoragePolicy>
template <typename... Args>
Expected<void, std::string>
EnforcedInit<T, ConcurrencyPolicy, CheckPolicy, ResetPolicy, StoragePolicy>::init(Args&&... args)
{
    [[maybe_unused]] auto guard = this->lock();
    if (mValue)
    {
        return make_unexpected("EnforcedInit object already initialized (init called twice)");
    }
    CheckPolicy::template pre_init_check<T>(args...);
    mValue.emplace(std::forward<Args>(args)...);
    CheckPolicy::template post_init_check<T>(mValue.value());
    notify_init();
    return {};
}

template <typename T, typename ConcurrencyPolicy, typename CheckPolicy, typename ResetPolicy, typename StoragePolicy>
template <typename U>
    requires std::is_constructible_v<T, std::initializer_list<U>>
Expected<void, std::string>
EnforcedInit<T, ConcurrencyPolicy, CheckPolicy, ResetPolicy, StoragePolicy>::init(std::initializer_list<U> ilist)
{
    [[maybe_unused]] auto guard = this->lock();
    if (mValue)
    {
        return make_unexpected("EnforcedInit object already initialized (init called twice)");
    }
    mValue.emplace(ilist);
    CheckPolicy::template post_init_check<T>(mValue.value());
    notify_init();
    return {};
}

template <typename T, typename ConcurrencyPolicy, typename CheckPolicy, typename ResetPolicy, typename StoragePolicy>
Expected<void, std::string>
EnforcedInit<T, ConcurrencyPolicy, CheckPolicy, ResetPolicy, StoragePolicy>::reset() noexcept(
    std::is_same_v<ResetPolicy, AllowResetPolicy>)
{
    [[maybe_unused]] auto guard = this->lock();
    if constexpr (std::is_same_v<ResetPolicy, AllowResetPolicy>)
    {
        mValue.reset();
        return {};
    }
    else
    {
        return make_unexpected("Reset not allowed by policy");
    }
}

template <typename T, typename ConcurrencyPolicy, typename CheckPolicy, typename ResetPolicy, typename StoragePolicy>
template <typename F>
    requires std::is_invocable_r_v<T, F>
void EnforcedInit<T, ConcurrencyPolicy, CheckPolicy, ResetPolicy, StoragePolicy>::lazy_init(F&& f)
{
    [[maybe_unused]] auto guard = this->lock();
    if (!mValue)
    {
        mValue.emplace(std::forward<F>(f)());
        CheckPolicy::template post_init_check<T>(mValue.value());
        notify_init();
    }
}

template <typename T, typename ConcurrencyPolicy, typename CheckPolicy, typename ResetPolicy, typename StoragePolicy>
template <typename F>
    requires (std::is_invocable_r_v<T, F> && !std::is_same_v<std::decay_t<F>, T>)
T& EnforcedInit<T, ConcurrencyPolicy, CheckPolicy, ResetPolicy, StoragePolicy>::get(F&& f)
{
    lazy_init(std::forward<F>(f));
    return get();
}

template <typename T, typename ConcurrencyPolicy, typename CheckPolicy, typename ResetPolicy, typename StoragePolicy>
T& EnforcedInit<T, ConcurrencyPolicy, CheckPolicy, ResetPolicy, StoragePolicy>::get()
{
    [[maybe_unused]] auto guard = this->lock_shared();
    FATP_ALWAYS_ENFORCE(static_cast<bool>(mValue), "Attempted to access EnforcedInit before init()");
    return mValue.value();
}

template <typename T, typename ConcurrencyPolicy, typename CheckPolicy, typename ResetPolicy, typename StoragePolicy>
const T& EnforcedInit<T, ConcurrencyPolicy, CheckPolicy, ResetPolicy, StoragePolicy>::get() const
{
    [[maybe_unused]] auto guard = this->lock_shared();
    FATP_ALWAYS_ENFORCE(static_cast<bool>(mValue), "Attempted to access EnforcedInit before init()");
    return mValue.value();
}

template <typename T, typename ConcurrencyPolicy, typename CheckPolicy, typename ResetPolicy, typename StoragePolicy>
T& EnforcedInit<T, ConcurrencyPolicy, CheckPolicy, ResetPolicy, StoragePolicy>::operator*()
{
    return get();
}

template <typename T, typename ConcurrencyPolicy, typename CheckPolicy, typename ResetPolicy, typename StoragePolicy>
const T& EnforcedInit<T, ConcurrencyPolicy, CheckPolicy, ResetPolicy, StoragePolicy>::operator*() const
{
    return get();
}

template <typename T, typename ConcurrencyPolicy, typename CheckPolicy, typename ResetPolicy, typename StoragePolicy>
T* EnforcedInit<T, ConcurrencyPolicy, CheckPolicy, ResetPolicy, StoragePolicy>::operator->()
{
    return &get();
}

template <typename T, typename ConcurrencyPolicy, typename CheckPolicy, typename ResetPolicy, typename StoragePolicy>
const T* EnforcedInit<T, ConcurrencyPolicy, CheckPolicy, ResetPolicy, StoragePolicy>::operator->() const
{
    return &get();
}

template <typename T, typename ConcurrencyPolicy, typename CheckPolicy, typename ResetPolicy, typename StoragePolicy>
bool EnforcedInit<T, ConcurrencyPolicy, CheckPolicy, ResetPolicy, StoragePolicy>::is_initialized() const noexcept
{
    [[maybe_unused]] auto guard = this->lock_shared();
    return static_cast<bool>(mValue);
}

template <typename T, typename ConcurrencyPolicy, typename CheckPolicy, typename ResetPolicy, typename StoragePolicy>
template <typename Duration>
bool EnforcedInit<T, ConcurrencyPolicy, CheckPolicy, ResetPolicy, StoragePolicy>::wait_for_init(
    const Duration& timeout) const
{
    if constexpr (std::is_base_of_v<ConditionVarPolicy, CheckPolicy>)
    {
        return static_cast<const ConditionVarPolicy*>(static_cast<const CheckPolicy*>(this))->wait_for_init(timeout);
    }
    else if constexpr (std::is_base_of_v<ConditionVarPolicy, ConcurrencyPolicy>)
    {
        return static_cast<const ConditionVarPolicy*>(static_cast<const ConcurrencyPolicy*>(this))
            ->wait_for_init(timeout);
    }
    else
    {
        auto start = std::chrono::steady_clock::now();
        while (!is_initialized())
        {
            if (std::chrono::steady_clock::now() - start >= timeout)
            {
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return true;
    }
}

template <typename T, typename ConcurrencyPolicy, typename CheckPolicy, typename ResetPolicy, typename StoragePolicy>
void EnforcedInit<T, ConcurrencyPolicy, CheckPolicy, ResetPolicy, StoragePolicy>::notify_init() noexcept
{
    if constexpr (std::is_base_of_v<ConditionVarPolicy, CheckPolicy>)
    {
        static_cast<ConditionVarPolicy*>(static_cast<CheckPolicy*>(this))->notify_init();
    }
    else if constexpr (std::is_base_of_v<ConditionVarPolicy, ConcurrencyPolicy>)
    {
        static_cast<ConditionVarPolicy*>(static_cast<ConcurrencyPolicy*>(this))->notify_init();
    }
}

template <typename T, typename... Policies>
using EnforcedInitUnique = EnforcedInit<std::unique_ptr<T>, Policies...>;

} // namespace fat_p
