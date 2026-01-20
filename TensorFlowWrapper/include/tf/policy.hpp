// tf/policy.hpp
// Thread-safety policies for TensorFlow C++20 wrapper
//
// MERGED IMPLEMENTATION - Best of ChatGPT + Claude:
// - ChatGPT: shared_ptr for mutex (allows policy copying), explicit Guard concept
// - Claude: static_assert verification, comprehensive documentation
//
// Fixes applied:
// - P0: No adopt_lock (returns unique_lock/shared_lock directly)
// - P1: Policies are copyable/movable via shared_ptr
// - P1: Concept matches implementations exactly

#pragma once

#include <concepts>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <type_traits>
#include <utility>

namespace tf::policy {

// ============================================================================
// Concepts
// ============================================================================

/// A Guard must be movable (for returning from scoped_lock/scoped_shared)
template<class G>
concept Guard = std::movable<G>;

/// A LockPolicy must provide scoped_lock() and scoped_shared() returning Guards
template<class P>
concept LockPolicy = requires(const P& p) {
    { p.scoped_lock() } -> Guard;
    { p.scoped_shared() } -> Guard;
};

// ============================================================================
// NoLock Policy - Zero overhead for single-threaded use
// ============================================================================

struct NoLock {
    /// Empty guard - compiler will eliminate entirely
    struct guard {
        constexpr guard() noexcept = default;
        constexpr guard(const guard&) noexcept = default;
        constexpr guard& operator=(const guard&) noexcept = default;
        constexpr guard(guard&&) noexcept = default;
        constexpr guard& operator=(guard&&) noexcept = default;
        ~guard() = default;
    };
    
    [[nodiscard]] constexpr guard scoped_lock() const noexcept { return {}; }
    [[nodiscard]] constexpr guard scoped_shared() const noexcept { return {}; }
};

// Compile-time verification
static_assert(std::is_empty_v<NoLock>, "NoLock must be zero-size");
static_assert(std::is_empty_v<NoLock::guard>, "NoLock::guard must be zero-size");
static_assert(std::is_trivially_copyable_v<NoLock::guard>, "NoLock::guard must be trivially copyable");
static_assert(LockPolicy<NoLock>, "NoLock must satisfy LockPolicy");
static_assert(Guard<NoLock::guard>, "NoLock::guard must satisfy Guard");

// ============================================================================
// Mutex Policy - Exclusive locking for multi-threaded writes
// ============================================================================

class Mutex {
public:
    using mutex_type = std::mutex;
    using guard_type = std::unique_lock<mutex_type>;
    
    /// Default constructor - creates new mutex
    Mutex() : m_(std::make_shared<mutex_type>()) {}
    
    /// Copyable (shares the mutex) - useful for multiple objects sharing one lock
    Mutex(const Mutex&) = default;
    Mutex& operator=(const Mutex&) = default;
    
    /// Movable
    Mutex(Mutex&&) noexcept = default;
    Mutex& operator=(Mutex&&) noexcept = default;
    
    /// Returns unique_lock that locks on construction (NO adopt_lock!)
    [[nodiscard]] guard_type scoped_lock() const { 
        return guard_type(*m_); 
    }
    
    /// Mutex has no shared mode - falls back to exclusive
    [[nodiscard]] guard_type scoped_shared() const { 
        return guard_type(*m_); 
    }
    
    /// Check if two Mutex objects share the same underlying mutex
    [[nodiscard]] bool shares_mutex_with(const Mutex& other) const noexcept {
        return m_ == other.m_;
    }

private:
    // shared_ptr allows copying and makes Mutex movable
    std::shared_ptr<mutex_type> m_;
};

static_assert(LockPolicy<Mutex>, "Mutex must satisfy LockPolicy");
static_assert(Guard<Mutex::guard_type>, "Mutex::guard_type must satisfy Guard");
static_assert(std::is_copy_constructible_v<Mutex>, "Mutex must be copyable");
static_assert(std::is_move_constructible_v<Mutex>, "Mutex must be movable");

// ============================================================================
// SharedMutex Policy - Reader-writer locking (many readers, exclusive writers)
// ============================================================================

class SharedMutex {
public:
    using mutex_type = std::shared_mutex;
    using exclusive_guard_type = std::unique_lock<mutex_type>;
    using shared_guard_type = std::shared_lock<mutex_type>;
    
    /// Default constructor - creates new mutex
    SharedMutex() : m_(std::make_shared<mutex_type>()) {}
    
    /// Copyable (shares the mutex)
    SharedMutex(const SharedMutex&) = default;
    SharedMutex& operator=(const SharedMutex&) = default;
    
    /// Movable
    SharedMutex(SharedMutex&&) noexcept = default;
    SharedMutex& operator=(SharedMutex&&) noexcept = default;
    
    /// Exclusive lock for writers
    [[nodiscard]] exclusive_guard_type scoped_lock() const { 
        return exclusive_guard_type(*m_); 
    }
    
    /// Shared lock for readers (multiple can hold simultaneously)
    [[nodiscard]] shared_guard_type scoped_shared() const { 
        return shared_guard_type(*m_); 
    }
    
    /// Check if two SharedMutex objects share the same underlying mutex
    [[nodiscard]] bool shares_mutex_with(const SharedMutex& other) const noexcept {
        return m_ == other.m_;
    }

private:
    std::shared_ptr<mutex_type> m_;
};

static_assert(LockPolicy<SharedMutex>, "SharedMutex must satisfy LockPolicy");
static_assert(Guard<SharedMutex::exclusive_guard_type>, "SharedMutex::exclusive_guard_type must satisfy Guard");
static_assert(Guard<SharedMutex::shared_guard_type>, "SharedMutex::shared_guard_type must satisfy Guard");
static_assert(std::is_copy_constructible_v<SharedMutex>, "SharedMutex must be copyable");
static_assert(std::is_move_constructible_v<SharedMutex>, "SharedMutex must be movable");

} // namespace tf::policy
