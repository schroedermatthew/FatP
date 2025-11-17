/**
 * @file ViewLifetimeTracking.h
 * @brief Debug-only lifetime tracking for views and references
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

#ifndef FATP_VIEW_LIFETIME_TRACKING_H
#define FATP_VIEW_LIFETIME_TRACKING_H

#pragma once

#include <memory>
#include <atomic>
#include <string>
#include <stdexcept>
#include <sstream>

namespace fat_p {

// =============================================================================
// Lifetime Tracking Exception
// =============================================================================

/**
 * @brief Exception thrown when accessing invalid view
 */
class DanglingReferenceError : public std::runtime_error {
public:
    explicit DanglingReferenceError(const std::string& msg)
        : std::runtime_error(msg) {}
};

// =============================================================================
// Debug-Only Lifetime Token
// =============================================================================

#ifndef NDEBUG

/**
 * @brief Token representing object lifetime (debug builds only)
 * @details Shared between source object and views
 */
class LifetimeToken {
public:
    LifetimeToken() : valid_(true) {}
    
    ~LifetimeToken() {
        invalidate();
    }
    
    // Non-copyable, non-movable
    LifetimeToken(const LifetimeToken&) = delete;
    LifetimeToken& operator=(const LifetimeToken&) = delete;
    
    void invalidate() {
        valid_.store(false, std::memory_order_release);
    }
    
    bool is_valid() const {
        return valid_.load(std::memory_order_acquire);
    }
    
private:
    std::atomic<bool> valid_;
};

/**
 * @brief RAII wrapper tracking object lifetime
 * @tparam T Type of object being tracked
 */
template<typename T>
class LifetimeTracker {
public:
    explicit LifetimeTracker(T& obj, const char* name = "Object")
        : obj_(&obj)
        , name_(name)
        , token_(std::make_shared<LifetimeToken>())
    {}
    
    ~LifetimeTracker() {
        if (token_) {
            token_->invalidate();
        }
    }
    
    // Movable but not copyable
    LifetimeTracker(LifetimeTracker&& other) noexcept
        : obj_(other.obj_)
        , name_(other.name_)
        , token_(std::move(other.token_))
    {
        other.obj_ = nullptr;
    }
    
    LifetimeTracker& operator=(LifetimeTracker&& other) noexcept {
        if (this != &other) {
            obj_ = other.obj_;
            name_ = other.name_;
            token_ = std::move(other.token_);
            other.obj_ = nullptr;
        }
        return *this;
    }
    
    LifetimeTracker(const LifetimeTracker&) = delete;
    LifetimeTracker& operator=(const LifetimeTracker&) = delete;
    
    /**
     * @brief Create tracked view of object
     * @return View that can check if source is still valid
     */
    class TrackedView {
    public:
        TrackedView(T* obj, std::shared_ptr<LifetimeToken> token, const char* name)
            : obj_(obj), token_(std::move(token)), name_(name)
        {}
        
        T* get() const {
            check_valid();
            return obj_;
        }
        
        T& operator*() const {
            return *get();
        }
        
        T* operator->() const {
            return get();
        }
        
        bool is_valid() const {
            return token_ && token_->is_valid();
        }
        
        void check_valid() const {
            if (!is_valid()) {
                std::ostringstream oss;
                oss << "Dangling reference: " << name_ 
                    << " has been destroyed";
                throw DanglingReferenceError(oss.str());
            }
        }
        
    private:
        T* obj_;
        std::shared_ptr<LifetimeToken> token_;
        const char* name_;
    };
    
    TrackedView create_view() const {
        return TrackedView(obj_, token_, name_);
    }
    
    T* get() const { return obj_; }
    T& operator*() const { return *obj_; }
    T* operator->() const { return obj_; }
    
private:
    T* obj_;
    const char* name_;
    std::shared_ptr<LifetimeToken> token_;
};

#else // NDEBUG - Release build

/**
 * @brief No-op lifetime tracker for release builds (zero overhead)
 */
template<typename T>
class LifetimeTracker {
public:
    explicit LifetimeTracker(T& obj, const char* = nullptr) : obj_(&obj) {}
    
    class TrackedView {
    public:
        TrackedView(T* obj) : obj_(obj) {}
        T* get() const { return obj_; }
        T& operator*() const { return *obj_; }
        T* operator->() const { return obj_; }
        bool is_valid() const { return true; }
        void check_valid() const { /* no-op */ }
    private:
        T* obj_;
    };
    
    TrackedView create_view() const {
        return TrackedView(obj_);
    }
    
    T* get() const { return obj_; }
    T& operator*() const { return *obj_; }
    T* operator->() const { return obj_; }
    
private:
    T* obj_;
};

#endif // NDEBUG

// =============================================================================
// Weak Pointer Utilities
// =============================================================================

/**
 * @brief Check if weak_ptr is expired and throw if so
 * @details Debug-only version with better error messages
 */
template<typename T>
std::shared_ptr<T> checked_lock(const std::weak_ptr<T>& wp, 
                                const char* context = "Object") {
    auto sp = wp.lock();
    if (!sp) {
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
template<typename T>
std::shared_ptr<T> safe_lock(const std::weak_ptr<T>& wp) noexcept {
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
template<typename ViewT>
class ViewGuard {
public:
    explicit ViewGuard(const ViewT& view, const char* scope = "Scope")
        : view_(view), scope_(scope)
    {
        view_.check_valid();
    }
    
    ~ViewGuard() {
        if (!view_.is_valid()) {
            // Can't throw in destructor, but can log
            std::cerr << "WARNING: View became invalid during " << scope_ << std::endl;
        }
    }
    
private:
    const ViewT& view_;
    const char* scope_;
};
#else
template<typename ViewT>
class ViewGuard {
public:
    explicit ViewGuard(const ViewT&, const char* = nullptr) {}
};
#endif

// =============================================================================
// Convenience Macros
// =============================================================================

#ifndef NDEBUG
/**
 * @brief Create tracked view with automatic naming
 */
#define TRACKED_VIEW(obj) \
    fat_p::LifetimeTracker<std::decay_t<decltype(obj)>>(obj, #obj)

/**
 * @brief Create view guard with scope name
 */
#define VIEW_GUARD(view) \
    fat_p::ViewGuard<std::decay_t<decltype(view)>> \
    FATP_CONCAT(view_guard_, __LINE__)(view, __func__)

// Helper for unique names
#define FATP_CONCAT_IMPL(x, y) x##y
#define FATP_CONCAT(x, y) FATP_CONCAT_IMPL(x, y)

#else
#define TRACKED_VIEW(obj) (obj)
#define VIEW_GUARD(view) ((void)0)
#endif

} // namespace fat_p

#endif // FATP_VIEW_LIFETIME_TRACKING_H
