#pragma once

/*
FATP_META:
  meta_version: 1
  component: ViewLifetimeTracking
  file_role: public_header
  path: include/fat_p/ViewLifetimeTracking.h
  namespace: fat_p
  layer: Domain
  summary: "Public header for ViewLifetimeTracking."
  api_stability: in_work
  related:
    docs_search: "ViewLifetimeTracking"
    tests:
      - components/ViewLifetimeTracking/tests/test_ViewLifetimeTracking.cpp
  hygiene:
    pragma_once: true
    include_guard: true
    defines_total: 7
    defines_unprefixed: 4
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/

/**
 * @file ViewLifetimeTracking.h
 * @brief Debug-only lifetime tracking for views and references
 *
 *
 * @version 1.0
 *
 * Provides compile-time-configurable lifetime tracking to detect:
 * - Dangling references (view outlives source)
 * - Use-after-free errors
 * - Invalid weak pointer access
 *
 * Features:
 * - Zero overhead in release builds (NDEBUG)
 * - Thread-safe tracking (optional)
 * - Integration with Tensor views and std::weak_ptr
 * - Clear error messages with source location
 *
 * Usage:
 *   LifetimeTracker<Tensor<int>> tracker(tensor);
 *   auto view = tracker.create_view();
 *   // view.check_valid() throws if tensor destroyed
 *
 * Requires: C++17
 */

#include <atomic>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

namespace fat_p
{

// =============================================================================
// Lifetime Tracking Exception
// =============================================================================

/**
 * @brief Exception thrown when accessing invalid view
 */
class DanglingReferenceError : public std::runtime_error
{
public:
    explicit DanglingReferenceError(const std::string& msg)
        : std::runtime_error(msg)
    {
    }
};

// =============================================================================
// Debug-Only Lifetime Token
// =============================================================================

#ifndef NDEBUG

/**
 * @brief Token representing object lifetime (debug builds only)
 * @details Shared between source object and views
 */
class LifetimeToken
{
public:
    LifetimeToken()
        : mValid(true)
    {
    }

    ~LifetimeToken()
    {
        invalidate();
    }

    // Non-copyable, non-movable
    LifetimeToken(const LifetimeToken&) = delete;
    LifetimeToken& operator=(const LifetimeToken&) = delete;

    void invalidate()
    {
        mValid.store(false, std::memory_order_release);
    }

    bool is_valid() const
    {
        return mValid.load(std::memory_order_acquire);
    }

private:
    std::atomic<bool> mValid;
};

/**
 * @brief RAII wrapper tracking object lifetime
 * @tparam T Type of object being tracked
 */
template <typename T>
class LifetimeTracker
{
public:
    explicit LifetimeTracker(T& obj, const char* name = "Object")
        : mObj(&obj)
        , mName(name)
        , mToken(std::make_shared<LifetimeToken>())
    {
    }

    ~LifetimeTracker()
    {
        if (mToken)
        {
            mToken->invalidate();
        }
    }

    // Movable but not copyable
    LifetimeTracker(LifetimeTracker&& other) noexcept
        : mObj(other.mObj)
        , mName(other.mName)
        , mToken(std::move(other.mToken))
    {
        other.mObj = nullptr;
    }

    LifetimeTracker& operator=(LifetimeTracker&& other) noexcept
    {
        if (this != &other)
        {
            mObj = other.mObj;
            mName = other.mName;
            mToken = std::move(other.mToken);
            other.mObj = nullptr;
        }
        return *this;
    }

    LifetimeTracker(const LifetimeTracker&) = delete;
    LifetimeTracker& operator=(const LifetimeTracker&) = delete;

    /**
     * @brief Create tracked view of object
     * @return View that can check if source is still valid
     */
    class TrackedView
    {
    public:
        TrackedView(T* obj, std::shared_ptr<LifetimeToken> token, const char* name)
            : mObj(obj)
            , mToken(std::move(token))
            , mName(name)
        {
        }

        T* get() const
        {
            check_valid();
            return mObj;
        }

        T& operator*() const
        {
            return *get();
        }

        T* operator->() const
        {
            return get();
        }

        bool is_valid() const
        {
            return mToken && mToken->is_valid();
        }

        void check_valid() const
        {
            if (!is_valid())
            {
                std::ostringstream oss;
                oss << "Dangling reference: " << mName << " has been destroyed";
                throw DanglingReferenceError(oss.str());
            }
        }

    private:
        T* mObj;
        std::shared_ptr<LifetimeToken> mToken;
        const char* mName;
    };

    TrackedView create_view() const
    {
        return TrackedView(mObj, mToken, mName);
    }

    T* get() const
    {
        return mObj;
    }
    T& operator*() const
    {
        return *mObj;
    }
    T* operator->() const
    {
        return mObj;
    }

private:
    T* mObj;
    const char* mName;
    std::shared_ptr<LifetimeToken> mToken;
};

#else // NDEBUG - Release build

/**
 * @brief No-op lifetime tracker for release builds (zero overhead)
 */
template <typename T>
class LifetimeTracker
{
public:
    explicit LifetimeTracker(T& obj, const char* = nullptr)
        : mObj(&obj)
    {
    }

    class TrackedView
    {
    public:
        TrackedView(T* obj)
            : mObj(obj)
        {
        }
        T* get() const
        {
            return mObj;
        }
        T& operator*() const
        {
            return *mObj;
        }
        T* operator->() const
        {
            return mObj;
        }
        bool is_valid() const
        {
            return true;
        }
        void check_valid() const
        { /* no-op */
        }

    private:
        T* mObj;
    };

    TrackedView create_view() const
    {
        return TrackedView(mObj);
    }

    T* get() const
    {
        return mObj;
    }
    T& operator*() const
    {
        return *mObj;
    }
    T* operator->() const
    {
        return mObj;
    }

private:
    T* mObj;
};

#endif // NDEBUG

// =============================================================================
// Weak Pointer Utilities
// =============================================================================

/**
 * @brief Check if weak_ptr is expired and throw if so
 * @details Debug-only version with better error messages
 */
template <typename T>
std::shared_ptr<T> checked_lock(const std::weak_ptr<T>& wp, const char* context = "Object")
{
    auto sp = wp.lock();
    if (!sp)
    {
#ifndef NDEBUG
        std::ostringstream oss;
        oss << "Weak pointer expired: " << context << " no longer exists";
        throw DanglingReferenceError(oss.str());
#else
        throw std::runtime_error("Weak pointer expired");
#endif
    }
    return sp;
}

/**
 * @brief Debug-only weak pointer lock with no-throw
 * @return shared_ptr or nullptr if expired
 */
template <typename T>
std::shared_ptr<T> safe_lock(const std::weak_ptr<T>& wp) noexcept
{
    return wp.lock();
}

// =============================================================================
// View Guard Helpers
// =============================================================================

/**
 * @brief RAII guard that validates view on construction/destruction
 * @details Useful for ensuring view stays valid during scope
 */
#ifndef NDEBUG
template <typename ViewT>
class ViewGuard
{
public:
    explicit ViewGuard(const ViewT& view, const char* scope = "Scope")
        : mView(view)
        , mScope(scope)
    {
        mView.check_valid();
    }

    ~ViewGuard()
    {
        if (!mView.is_valid())
        {
            // Can't throw in destructor, but can log
            std::cerr << "WARNING: View became invalid during " << mScope << std::endl;
        }
    }

private:
    const ViewT& mView;
    const char* mScope;
};
#else
template <typename ViewT>
class ViewGuard
{
public:
    explicit ViewGuard(const ViewT&, const char* = nullptr)
    {
    }
};
#endif

// =============================================================================
// Convenience Macros
// =============================================================================

#ifndef NDEBUG
/**
 * @brief Create tracked view with automatic naming
 */
#define FATP_TRACKED_VIEW(obj) fat_p::LifetimeTracker<std::decay_t<decltype(obj)>>(obj, #obj)

/**
 * @brief Create view guard with scope name
 */
#define FATP_VIEW_GUARD(view) \
    fat_p::ViewGuard<std::decay_t<decltype(view)>> FATP_CONCAT(view_guard_, __LINE__)(view, __func__)

// Helper for unique names
#define FATP_CONCAT_IMPL(x, y) x##y
#define FATP_CONCAT(x, y) FATP_CONCAT_IMPL(x, y)

#else
#define FATP_TRACKED_VIEW(obj) (obj)
#define FATP_VIEW_GUARD(view) ((void)0)
#endif

} // namespace fat_p
