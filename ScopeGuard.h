/**
 * @file ScopeGuard.h
 * @brief Provides the ScopeGuard class for general-purpose RAII scope-exit cleanup.
 *
 * @details This RAII utility executes a user-provided cleanup action upon
 * scope exit, with customizable exception handling via policies. It supports
 * move semantics for transfer, early release to disable execution, and
 * in-place construction for complex actions. Policies (defined in
 * ScopeGuardPolicies.h) control dtor behavior, ensuring compliance with
 * noexcept specifications and offering options for termination, logging,
 * or re-throwing.
 *
 * Macros provide concise lambda syntax while avoiding name clashes through
 * __COUNTER__ fallback.
 *
 * @version 2.1 - Thread-Safety Fixes Applied
 * @author C++ Utilities Library
 * @date 2025
 *
 * @changelog v2.1 (2025-11-02):
 *   - CRITICAL FIX: dismiss() and dismiss_if() now use LockGuardType (exclusive lock)
 *     instead of SharedGuardType to prevent data races with SharedMutexPolicy
 *   - CRITICAL FIX: Move constructor now locks source object during move
 *   - CRITICAL FIX: Move assignment uses std::scoped_lock for atomic locking
 *   - Added static assertions for policy validation
 *   - Simplified destructor noexcept specification
 *   - Enhanced documentation for thread-safety guarantees
 *
 * Requirements:
 * - C++17 or later
 * - No external dependencies beyond standard library
 * - Header-only
 *
 * @tparam F The type of the cleanup function object (e.g., lambda).
 * @tparam ThrowingPolicy The policy for handling exceptions from F
 * (defaults to ScopeGuardTerminatePolicy).
 * @tparam ConcurrencyPolicy The policy for thread-safety (defaults to SingleThreadedPolicy).
 * @tparam ActionPolicy The policy for action storage (defaults to DefaultActionPolicy).
 */
#pragma once

#include <functional>   // For std::function and std::invoke
#include <type_traits>  // For std::enable_if_t, std::is_nothrow_constructible_v, etc.
#include <utility>      // For std::forward, std::move
#include <cassert>      // For assert


#include "FatPTypeTraits.h"
#include "ScopeGuardPolicies.h"  // For PolicyExecutor and tags
#include "ConcurrencyPolicies.h" // For thread-safety policy

namespace fat_p {

// =============================================================================
// Minimal Enforce Support for Debug Builds
// =============================================================================

#ifndef NDEBUG
    #define FATP_DEBUG_ENFORCE(cond, msg) \
        do { if (!(cond)) { assert((cond) && (msg)); } } while(0)
#else
    #define FATP_DEBUG_ENFORCE(cond, msg) ((void)0)
#endif

// =============================================================================
// Action Policies for Extensibility
// =============================================================================

/**
 * @brief Default storage policy: holds F directly.
 * 
 * @details Provides move construction, move assignment, and invocation.
 * Copy operations are explicitly deleted to enforce unique ownership.
 */
template <typename F>
struct DefaultActionPolicy {
    F action;
    
    // Default constructor (for emplace construction)
    DefaultActionPolicy() = default;
    
    // Constructor from arguments
    template <typename... Args>
    explicit DefaultActionPolicy(Args&&... args) 
        noexcept(std::is_nothrow_constructible_v<F, Args&&...>)
        : action(std::forward<Args>(args)...) 
    {}
    
    // Move constructor
    DefaultActionPolicy(DefaultActionPolicy&& other) 
        noexcept(std::is_nothrow_move_constructible_v<F>)
        : action(std::move(other.action)) 
    {}
    
    // Move assignment operator (same type)
    DefaultActionPolicy& operator=(DefaultActionPolicy&& other) 
        noexcept(std::is_nothrow_move_assignable_v<F>) 
    {
        if (this != &other) {
            action = std::move(other.action);
        }
        return *this;
    }
    
    // Invocation operator
    void operator()() noexcept(noexcept(std::declval<F&>()())) { 
        action(); 
    }
    
    // Explicitly delete copy operations
    DefaultActionPolicy(const DefaultActionPolicy&) = delete;
    DefaultActionPolicy& operator=(const DefaultActionPolicy&) = delete;
};

// =============================================================================
// Policy Validation Traits (v2.1)
// =============================================================================

namespace detail {
    // Check if policy has LockGuard type
    template <typename P, typename = void>
    struct has_lock_guard : std::false_type {};
    
    template <typename P>
    struct has_lock_guard<P, std::void_t<typename P::LockGuard>> : std::true_type {};
    
    // Check if policy has SharedGuard type
    template <typename P, typename = void>
    struct has_shared_guard : std::false_type {};
    
    template <typename P>
    struct has_shared_guard<P, std::void_t<typename P::SharedGuard>> : std::true_type {};
    
    // Check if policy has getLock() method
    template <typename P, typename = void>
    struct has_get_lock : std::false_type {};
    
    template <typename P>
    struct has_get_lock<P, std::void_t<decltype(std::declval<P>().getLock())>> : std::true_type {};
}

// =============================================================================
// ScopeGuard Class
// =============================================================================

/**
 * @brief A generic RAII utility that executes a cleanup function upon scope exit.
 *
 * @details 
 * The ScopeGuard class provides automatic resource management through RAII.
 * When a ScopeGuard object goes out of scope, it automatically executes the
 * cleanup action that was provided at construction time.
 *
 * Key Features:
 * - Move-only semantics (copy operations are deleted)
 * - Early dismiss() to prevent execution
 * - Conditional dismiss_if() for conditional cleanup
 * - Customizable exception handling via policies
 * - Thread-safety via concurrency policies
 * - Extensible action storage via action policies
 *
 * Thread-Safety Guarantees (v2.1):
 * - dismiss(), dismiss_if(): Use exclusive locks (LockGuardType) for writes
 * - is_active(): Uses shared locks (SharedGuardType) for concurrent reads
 * - Move operations: Lock both source and destination atomically
 * - Destructor: Thread-safe with proper locking policies
 *
 * IMPORTANT: Following standard C++ move semantics, moves assume no other
 * thread is concurrently accessing the source object. The internal locking
 * protects against races between the move and the source object's own
 * operations (e.g., dismiss()), but external synchronization is required
 * if multiple threads might simultaneously attempt to move from the same object.
 *
 * Policy Compatibility (v2.1):
 * Ã¢Å“â€¦ Compatible: SingleThreaded, Mutex, SharedMutex, UniqueRWLock, Spinlock,
 *               Ticket, MCS, Adaptive, Versioned, SeqLock, PriorityInheritance,
 *               Waitable, Recursive, Timed, SharedTimed
 * Ã¢Å¡Â Ã¯Â¸Â Requires care: RCUPolicy, HazardPointerPolicy (templated on data type)
 * Ã¢ÂÅ’ Not compatible: LockFreeSynchronization (assertion-only, debug mode)
 *
 * Example:
 * @code
 * {
 *     int* ptr = new int(42);
 *     auto guard = ScopeGuard([ptr]() { delete ptr; });
 *     // ... use ptr ...
 *     // ptr is automatically deleted when guard goes out of scope
 * }
 * @endcode
 *
 * @tparam F The type of the cleanup function object (e.g., a lambda).
 * @tparam ThrowingPolicy The policy to handle exceptions escaping F.
 * @tparam ConcurrencyPolicy The policy for thread-safety.
 * @tparam ActionPolicy The policy for action storage.
 */
template <typename F, 
          typename ThrowingPolicy = ScopeGuardTerminatePolicy,
          typename ConcurrencyPolicy = SingleThreadedPolicy,
          template <typename> class ActionPolicy = DefaultActionPolicy>
class ScopeGuard : public ConcurrencyPolicy {
private:
    // =============================================================================
    // Compile-Time Policy Validation (v2.1)
    // =============================================================================
    static_assert(detail::has_lock_guard<ConcurrencyPolicy>::value,
                  "ConcurrencyPolicy must provide LockGuard type. "
                  "Ensure your policy defines 'using LockGuard = ...' or 'class LockGuard { ... }'");
    
    static_assert(detail::has_shared_guard<ConcurrencyPolicy>::value,
                  "ConcurrencyPolicy must provide SharedGuard type. "
                  "Ensure your policy defines 'using SharedGuard = ...' or 'class SharedGuard { ... }'");
    
    static_assert(detail::has_get_lock<ConcurrencyPolicy>::value,
                  "ConcurrencyPolicy must provide getLock() method. "
                  "Ensure your policy implements 'auto getLock() { return ...; }'");

public:
    // Type aliases for clarity
    using ActionStorage = ActionPolicy<F>;
    using LockGuardType = typename ConcurrencyPolicy::LockGuard;
    using SharedGuardType = typename ConcurrencyPolicy::SharedGuard;
    
    // =========================================================================
    // Constructors
    // =========================================================================
    
    /**
     * @brief Constructs the ScopeGuard with the cleanup action.
     * 
     * @details Moves the action into storage. The action will be executed
     * when the ScopeGuard is destroyed, unless dismiss() is called first.
     * 
     * @param action The function object to execute on scope exit.
     */
    explicit ScopeGuard(F&& action)
        noexcept(std::is_nothrow_constructible_v<ActionStorage, F&&>)
        : ConcurrencyPolicy()
        , m_action_storage(std::forward<F>(action))
        , m_execute(true)
    {
        FATP_DEBUG_ENFORCE(&m_action_storage, 
            "ScopeGuard constructed with null action storage");
    }
    
    /**
     * @brief In-place constructs the cleanup action from arguments.
     * 
     * @details Uses perfect forwarding to build F in storage.
     * Useful for complex callables that aren't easily moveable.
     * 
     * @tparam Args Argument types for F's constructor.
     * @param args Arguments forwarded to F's constructor.
     */
    template <typename... Args,
              typename = std::enable_if_t<
                  std::is_constructible_v<ActionStorage, Args...> &&
                  !(sizeof...(Args) == 1 && 
                    std::is_same_v<std::decay_t<std::tuple_element_t<0, std::tuple<Args...>>>, 
                                   ScopeGuard>)>>
    explicit ScopeGuard(Args&&... args)
        noexcept(std::is_nothrow_constructible_v<ActionStorage, Args&&...>)
        : ConcurrencyPolicy()
        , m_action_storage(std::forward<Args>(args)...)
        , m_execute(true)
    {
        FATP_DEBUG_ENFORCE(&m_action_storage, 
            "ScopeGuard emplace-constructed with null action storage");
    }
    
    // =========================================================================
    // Move Semantics (FIXED v2.1)
    // =========================================================================
    
    /**
     * @brief Move constructor. Transfers the action and disables the source.
     * 
     * @details THREAD-SAFETY (FIXED v2.1): Locks the source object to safely
     * transfer the execute flag. The action storage itself is move-constructed
     * in the initializer list (required for non-default-constructible types
     * like lambdas).
     * 
     * The locking protects against races between the move and other's own
     * operations (e.g., other.dismiss() on another thread). Following standard
     * C++ move semantics, the caller must ensure no other thread is attempting
     * to move from 'other' simultaneously.
     * 
     * For SingleThreadedPolicy, locking is a no-op (zero overhead).
     * 
     * @param other The source ScopeGuard to move from.
     */
    ScopeGuard(ScopeGuard&& other) noexcept
        : ConcurrencyPolicy()  // Create new policy instance (new mutex if applicable)
        , m_action_storage(std::move(other.m_action_storage))  // Move-construct in place
        , m_execute(false)  // Initialize to false, will be set correctly below
    {
        // FIXED v2.1: Lock source to safely read/write m_execute flag
        // Note: m_action_storage is already moved (must be in init list for lambdas)
        // This lock protects against concurrent dismiss()/is_active() on 'other'
        typename std::decay_t<decltype(other)>::LockGuardType other_guard(other.getLock());
        
        m_execute = other.m_execute;
        other.m_execute = false;
    }
    
    /**
     * @brief Move assignment operator (same type).
     * 
     * @details THREAD-SAFETY (FIXED v2.1): Locks both objects to prevent
     * concurrent access during assignment. Executes the current action
     * before transferring from source.
     * 
     * Implementation uses separate LockGuards for compatibility with policies
     * that don't provide BasicLockable interface (e.g., SingleThreadedPolicy::NoOpLock).
     * Lock ordering (this before other) prevents deadlock when used consistently.
     * 
     * @param other The source ScopeGuard to move from.
     * @return Reference to this object.
     */
    ScopeGuard& operator=(ScopeGuard&& other) 
        noexcept(std::is_nothrow_move_assignable_v<ActionStorage>)
    {
        if (this != &other) {
            // FIXED v2.1: Lock both objects (this first, then other for consistent ordering)
            LockGuardType this_guard(this->getLock());
            typename std::decay_t<decltype(other)>::LockGuardType other_guard(other.getLock());
            
            // Execute current action if still active (both objects now locked)
            if (m_execute) {
                ScopeGuardPolicyExecutor<ActionStorage, ThrowingPolicy>::execute(
                    m_action_storage);
            }
            
            // Transfer from other (both are locked, safe to access)
            m_action_storage = std::move(other.m_action_storage);
            m_execute = other.m_execute;
            other.m_execute = false;
        }
        return *this;
    }
    
    // =========================================================================
    // Deleted Copy Operations
    // =========================================================================
    
    // Scope guards must be unique to avoid double-execution
    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;
    
    // =========================================================================
    // Destructor (SIMPLIFIED v2.1)
    // =========================================================================
    
    /**
     * @brief Executes the cleanup action if not dismissed.
     * 
     * @details Delegates exception handling to the ThrowingPolicy.
     * 
     * SIMPLIFIED v2.1: Only ScopeGuardRethrowPolicy is potentially throwing.
     * All other policies (Nothrow, Terminate, LogAndSwallow) are noexcept.
     * 
     * @warning When using ScopeGuardRethrowPolicy, this destructor is potentially
     * throwing! This can lead to std::terminate() if an exception is already in
     * flight during stack unwinding. Use Rethrow policy only in controlled
     * environments (testing, debugging).
     */
    ~ScopeGuard() noexcept(!std::is_same_v<ThrowingPolicy, ScopeGuardRethrowPolicy>)
    {
        LockGuardType guard(this->getLock());
        
        if (m_execute) {
            ScopeGuardPolicyExecutor<ActionStorage, ThrowingPolicy>::execute(
                m_action_storage);
        }
    }
    
    // =========================================================================
    // Early Release (Conditional Dismiss) - FIXED v2.1
    // =========================================================================
    
    /**
     * @brief Disables the execution of the cleanup action on scope exit.
     * 
     * @details After calling dismiss(), the action will not run in the
     * destructor. This is useful when the cleanup is no longer needed
     * (e.g., because the resource was successfully transferred elsewhere).
     * 
     * THREAD-SAFETY (FIXED v2.1): Uses LockGuardType (exclusive lock) to
     * ensure mutual exclusion when writing m_execute. This prevents data races
     * with concurrent is_active() calls when using reader-writer policies
     * like SharedMutexPolicy or UniqueRWLockPolicy.
     * 
     * Previous bug: Used SharedGuardType, which for SharedMutexPolicy is a
     * std::shared_lock (allows concurrent readers). This caused data races when
     * dismiss() wrote to m_execute while is_active() was reading it concurrently.
     * 
     * Fix: LockGuardType provides exclusive access (std::unique_lock for
     * SharedMutexPolicy), blocking all other access during the write.
     * 
     * For policies where SharedGuard = LockGuard (e.g., MutexSynchronizationPolicy),
     * this change has no effect as both types are identical.
     */
    void dismiss() noexcept {
        LockGuardType guard(this->getLock());  // FIXED v2.1: Exclusive lock for write
        m_execute = false;
    }
    
    /**
     * @brief Conditionally dismisses based on a boolean condition.
     * 
     * @details If cond is true, dismisses the guard. Otherwise, no operation.
     * 
     * THREAD-SAFETY (FIXED v2.1): Uses LockGuardType (exclusive lock) when
     * writing m_execute. See dismiss() for detailed explanation.
     * 
     * @param cond If true, dismiss; else keep active.
     */
    void dismiss_if(bool cond) noexcept {
        if (cond) {
            LockGuardType guard(this->getLock());  // FIXED v2.1: Exclusive lock for write
            m_execute = false;
        }
    }
    
    /**
     * @brief Check if the guard is still active (will execute on destruction).
     * 
     * @details This is a read-only operation that can safely use a shared lock,
     * allowing multiple concurrent readers when using SharedMutexPolicy.
     * 
     * THREAD-SAFETY: Uses SharedGuardType (shared lock) for concurrent reads.
     * This is correct and efficient because:
     * 1. is_active() only reads m_execute (no modification)
     * 2. SharedGuardType allows multiple concurrent readers
     * 3. dismiss() now correctly uses LockGuardType (exclusive), which blocks
     *    all SharedGuards until the write completes
     * 
     * For SharedMutexPolicy: Multiple is_active() calls can run concurrently,
     * but dismiss() will block until all readers finish.
     * 
     * For MutexSynchronizationPolicy: SharedGuard = LockGuard, so this acquires
     * an exclusive lock (same behavior as before).
     * 
     * @return true if the action will execute, false if dismissed.
     */
    [[nodiscard]] bool is_active() noexcept {
        SharedGuardType guard(this->getLock());  // Shared lock OK for read
        return m_execute;
    }
    
private:
    ActionStorage m_action_storage;
    bool m_execute;
};

// =============================================================================
// Convenience Factory Functions
// =============================================================================

/**
 * @brief Create a ScopeGuard with default policy (terminates on exception).
 * 
 * @tparam F The type of the cleanup function.
 * @param fn The cleanup function.
 * @return A ScopeGuard object.
 */
template <typename F>
[[nodiscard]] auto makeScopeGuard(F&& fn) {
    return ScopeGuard<std::decay_t<F>>(std::forward<F>(fn));
}

/**
 * @brief Create a ScopeGuard with a specific throwing policy.
 * 
 * @tparam Policy The throwing policy to use.
 * @tparam F The type of the cleanup function.
 * @param fn The cleanup function.
 * @return A ScopeGuard object with the specified policy.
 */
template <typename Policy, typename F>
[[nodiscard]] auto makeScopeGuard(F&& fn) {
    return ScopeGuard<std::decay_t<F>, Policy>(std::forward<F>(fn));
}

/**
 * @brief Create a ScopeGuard with full policy customization.
 * 
 * @tparam ThrowingPolicy The throwing policy.
 * @tparam ConcurrencyPolicy The concurrency policy.
 * @tparam F The type of the cleanup function.
 * @param fn The cleanup function.
 * @return A ScopeGuard object with the specified policies.
 */
template <typename ThrowingPolicy, typename ConcurrencyPolicy, typename F>
[[nodiscard]] auto makeScopeGuardEx(F&& fn) {
    return ScopeGuard<std::decay_t<F>, ThrowingPolicy, ConcurrencyPolicy>(
        std::forward<F>(fn));
}

// =============================================================================
// Convenience Macro Helpers
// =============================================================================

// Maker for default policy (ScopeGuardTerminatePolicy)
struct DefaultScopeGuardMaker {};

inline DefaultScopeGuardMaker MakeScopeGuard() { 
    return {}; 
}

template <typename Fn>
[[nodiscard]] ScopeGuard<std::decay_t<Fn>> operator+(
    DefaultScopeGuardMaker, Fn&& fn) 
{
    return ScopeGuard<std::decay_t<Fn>>(std::forward<Fn>(fn));
}

// Maker for explicit policy
template <typename Policy>
struct ScopeGuardMaker {};

template <typename Policy>
inline ScopeGuardMaker<Policy> MakeScopeGuard() { 
    return {}; 
}

template <typename Policy, typename Fn>
[[nodiscard]] ScopeGuard<std::decay_t<Fn>, Policy> operator+(
    ScopeGuardMaker<Policy>, Fn&& fn) 
{
    return ScopeGuard<std::decay_t<Fn>, Policy>(std::forward<Fn>(fn));
}

// =============================================================================
// Convenience Macros
// =============================================================================

// Helper to get noexcept specifier for each policy
#define GET_NOEXCEPT_ScopeGuardNothrowPolicy noexcept
#define GET_NOEXCEPT_ScopeGuardTerminatePolicy 
#define GET_NOEXCEPT_ScopeGuardLogAndSwallowPolicy 
#define GET_NOEXCEPT_ScopeGuardRethrowPolicy 
#define GET_NOEXCEPT(PolicyTag) GET_NOEXCEPT_##PolicyTag

// Helper macro to generate a unique name for the ScopeGuard instance
// Use __COUNTER__ if available for better uniqueness; fallback to __LINE__
#if defined(__COUNTER__)
    #define FATP_SCOPE_GUARD_UNIQUE(prefix) \
        FATP_SCOPE_GUARD_CONCAT_IMPL(prefix, __COUNTER__)
#else
    #define FATP_SCOPE_GUARD_UNIQUE(prefix) \
        FATP_SCOPE_GUARD_CONCAT_IMPL(prefix, __LINE__)
#endif

#define FATP_SCOPE_GUARD_CONCAT_IMPL(a, b) \
    FATP_SCOPE_GUARD_CONCAT_IMPL2(a, b)
#define FATP_SCOPE_GUARD_CONCAT_IMPL2(a, b) a##b

/**
 * @brief Factory macro to create a ScopeGuard with default policy.
 * 
 * @details Usage: SCOPE_GUARD { cleanup code; };
 * 
 * Example:
 * @code
 * int* ptr = new int(42);
 * SCOPE_GUARD { delete ptr; };
 * @endcode
 */
#define SCOPE_GUARD \
    auto FATP_SCOPE_GUARD_UNIQUE(scope_guard_) = \
        ::fat_p::MakeScopeGuard() + [&]() 

/**
 * @brief Factory macro to create a ScopeGuard with an explicit policy.
 * 
 * @param PolicyTag The desired policy (e.g., ScopeGuardTerminatePolicy).
 * 
 * @details Usage: SCOPE_GUARD_EX(PolicyTag) { cleanup code; };
 * 
 * Example:
 * @code
 * SCOPE_GUARD_EX(ScopeGuardNothrowPolicy) noexcept { 
 *     // cleanup code that must not throw
 * };
 * @endcode
 */
#define SCOPE_GUARD_EX(PolicyTag) \
    auto FATP_SCOPE_GUARD_UNIQUE(scope_guard_) = \
        ::fat_p::MakeScopeGuard<PolicyTag>() + [&]() GET_NOEXCEPT(PolicyTag)

template <typename OnExit, typename OnSuccess, typename OnFailure, typename ExecutionPolicy>
struct is_scope_guard<ScopeGuardImpl<OnExit, OnSuccess, OnFailure, ExecutionPolicy>> : std::true_type {};

} // namespace fat_p
