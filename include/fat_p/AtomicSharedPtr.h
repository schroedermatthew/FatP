#pragma once

/*
FATP_META:
  meta_version: 1
  component: AtomicSharedPtr
  file_role: public_header
  path: include/fat_p/AtomicSharedPtr.h
  namespace: fat_p
  layer: Concurrency
  summary: "Public header for AtomicSharedPtr."
  api_stability: in_work
  related:
    docs_search: "AtomicSharedPtr"
    tests:
      - components/AtomicSharedPtr/tests/test_AtomicSharedPtr.cpp
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
 * @file AtomicSharedPtr.h
 * @brief Minimal thread-safe atomic wrapper for std::shared_ptr.
 *
 * @details Thin wrapper providing atomic operations on shared_ptr<T>.
 * Uses std::atomic<shared_ptr> (C++20).
 *
 * This is an **atomic handle**, not an atomic object - atomicity applies
 * only to the pointer/control block, not to the managed object T.
 *
 * @note Requires C++20.
 * @author Fat-P Library
 * @version 1.0
 * @date 2025
 */

#include "CppFeatureDetection.h"

#include <atomic>
#include <memory>
#include <stdexcept>

// Use the standard library feature test macro for accurate detection.
// A compiler might support C++20 syntax but the stdlib might not have
// std::atomic<shared_ptr> yet. __cpp_lib_atomic_shared_ptr is authoritative.
// Note: The macro is defined by <memory> when the feature is available.
#if FATP_HAS_ATOMIC_SHARED_PTR
#define FATP_HAS_CPP20_ATOMIC_SHARED_PTR 1
#else
#define FATP_HAS_CPP20_ATOMIC_SHARED_PTR 0
#endif

namespace fat_p
{

/**
 * @brief Thread-safe atomic wrapper for std::shared_ptr<T>.
 *
 * @tparam T The managed object type
 * @tparam ThrowOnNull If true, load() throws on null; if false, returns null
 */
template <typename T, bool ThrowOnNull = false>
class AtomicSharedPtr
{
private:
#if FATP_HAS_CPP20_ATOMIC_SHARED_PTR
    std::atomic<std::shared_ptr<T>> mPtr;
#else
    std::shared_ptr<T> mPtr;
#endif

public:
    using value_type = T;
    using pointer_type = std::shared_ptr<T>;

    AtomicSharedPtr() noexcept
        : mPtr(nullptr)
    {
    }

    explicit AtomicSharedPtr(std::shared_ptr<T> p) noexcept
        : mPtr(std::move(p))
    {
    }

    AtomicSharedPtr(const AtomicSharedPtr&) = delete;
    AtomicSharedPtr& operator=(const AtomicSharedPtr&) = delete;
    AtomicSharedPtr(AtomicSharedPtr&&) = delete;
    AtomicSharedPtr& operator=(AtomicSharedPtr&&) = delete;

    // ========================================================================
    // Core Operations
    // ========================================================================

    [[nodiscard]] std::shared_ptr<T> load(std::memory_order order = std::memory_order_acquire) const
    {
#if FATP_HAS_CPP20_ATOMIC_SHARED_PTR
        auto result = mPtr.load(order);
#else
        auto result = std::atomic_load_explicit(&mPtr, order);
#endif
        if constexpr (ThrowOnNull)
        {
            if (!result)
            {
                throw std::runtime_error("AtomicSharedPtr::load() returned null");
            }
        }
        return result;
    }

    [[nodiscard]] std::shared_ptr<T> raw_load(std::memory_order order = std::memory_order_acquire) const noexcept
    {
#if FATP_HAS_CPP20_ATOMIC_SHARED_PTR
        return mPtr.load(order);
#else
        return std::atomic_load_explicit(&mPtr, order);
#endif
    }

    void store(std::shared_ptr<T> p, std::memory_order order = std::memory_order_release) noexcept
    {
#if FATP_HAS_CPP20_ATOMIC_SHARED_PTR
        mPtr.store(std::move(p), order);
#else
        std::atomic_store_explicit(&mPtr, std::move(p), order);
#endif
    }

    [[nodiscard]] std::shared_ptr<T> exchange(std::shared_ptr<T> p,
                                              std::memory_order order = std::memory_order_acq_rel) noexcept
    {
#if FATP_HAS_CPP20_ATOMIC_SHARED_PTR
        return mPtr.exchange(std::move(p), order);
#else
        return std::atomic_exchange_explicit(&mPtr, std::move(p), order);
#endif
    }

    bool compare_exchange_weak(std::shared_ptr<T>& expected,
                               std::shared_ptr<T> desired,
                               std::memory_order success = std::memory_order_acq_rel,
                               std::memory_order failure = std::memory_order_acquire) noexcept
    {
#if FATP_HAS_CPP20_ATOMIC_SHARED_PTR
        return mPtr.compare_exchange_weak(expected, std::move(desired), success, failure);
#else
        return std::atomic_compare_exchange_weak_explicit(&mPtr, &expected, std::move(desired), success, failure);
#endif
    }

    bool compare_exchange_strong(std::shared_ptr<T>& expected,
                                 std::shared_ptr<T> desired,
                                 std::memory_order success = std::memory_order_acq_rel,
                                 std::memory_order failure = std::memory_order_acquire) noexcept
    {
#if FATP_HAS_CPP20_ATOMIC_SHARED_PTR
        return mPtr.compare_exchange_strong(expected, std::move(desired), success, failure);
#else
        return std::atomic_compare_exchange_strong_explicit(&mPtr, &expected, std::move(desired), success, failure);
#endif
    }

    // ========================================================================
    // Wait/Notify (C++20 std::atomic wait/notify)
    // ========================================================================

#if FATP_HAS_CPP20_ATOMIC_SHARED_PTR
    void wait(std::shared_ptr<T> old, std::memory_order order = std::memory_order_acquire) const
    {
        mPtr.wait(std::move(old), order);
    }

    void notify_one() noexcept
    {
        mPtr.notify_one();
    }
    void notify_all() noexcept
    {
        mPtr.notify_all();
    }
#endif

    // ========================================================================
    // Utilities
    // ========================================================================

    [[nodiscard]] bool is_lock_free() const noexcept
    {
#if FATP_HAS_CPP20_ATOMIC_SHARED_PTR
        return mPtr.is_lock_free();
#else
        // The free-function shared_ptr atomics have no lock-free query.
        // They typically use a global lock table, so conservatively return false.
        return false;
#endif
    }

    [[nodiscard]] static constexpr bool is_always_lock_free() noexcept
    {
#if FATP_HAS_CPP20_ATOMIC_SHARED_PTR
        return std::atomic<std::shared_ptr<T>>::is_always_lock_free;
#else
        return false;
#endif
    }

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return raw_load() != nullptr;
    }
};

// ============================================================================
// Factory Function
// ============================================================================

template <typename T, bool ThrowOnNull = false, typename... Args>
[[nodiscard]] AtomicSharedPtr<T, ThrowOnNull> make_atomic_shared_ptr(Args&&... args)
{
    return AtomicSharedPtr<T, ThrowOnNull>(std::make_shared<T>(std::forward<Args>(args)...));
}

// ============================================================================
// Type Trait
// ============================================================================

template <typename T>
struct is_atomic_shared_ptr : std::false_type
{
};

template <typename T, bool B>
struct is_atomic_shared_ptr<AtomicSharedPtr<T, B>> : std::true_type
{
};

template <typename T>
inline constexpr bool is_atomic_shared_ptr_v = is_atomic_shared_ptr<T>::value;

} // namespace fat_p
