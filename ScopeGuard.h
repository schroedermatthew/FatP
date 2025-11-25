/**
 * @file ScopeGuard.h
 * @brief Provides the ScopeGuard class for general-purpose RAII scope-exit cleanup.
 *
 * @details This RAII utility executes a user-provided cleanup action upon scope exit, 
 * with customizable exception handling via policies. It supports move semantics for 
 * transfer, early release to disable execution, and in-place construction for complex 
 * actions. Policies (defined in ScopeGuardPolicies.h) control destructor behavior, 
 * ensuring compliance with noexcept specifications and offering options for 
 * termination, logging, or re-throwing.
 *
 * Macros provide concise lambda syntax while avoiding name clashes through 
 * __COUNTER__ fallback.
 *
 * Requirements:
 * - C++17 or later
 * - No external dependencies beyond standard library
 * - Header-only
 * - Single-threaded use only
 *
 * @tparam F The type of the cleanup function object (e.g., lambda).
 * @tparam ThrowingPolicy The policy for handling exceptions from F 
 *         (defaults to ScopeGuardTerminatePolicy).
 * @tparam ActionPolicy The policy for action storage (defaults to DefaultActionPolicy).
 */
#pragma once

#include <functional>
#include <type_traits>
#include <utility>
#include <cassert>

#include "FatPTypeTraits.h"
#include "ScopeGuardPolicies.h"

namespace fat_p {

#ifndef NDEBUG
    #define FATP_DEBUG_ENFORCE(cond, msg) do { if (!(cond)) { assert((cond) && (msg)); } } while(0)
#else
    #define FATP_DEBUG_ENFORCE(cond, msg) ((void)0)
#endif

/**
 * @brief Default storage policy: holds F directly.
 * 
 * @details Provides move construction, move assignment, and invocation. 
 * Copy operations are explicitly deleted to enforce unique ownership.
 */
template <typename F>
struct DefaultActionPolicy {
    F action;

    DefaultActionPolicy() = default;

    template <typename... Args>
    explicit DefaultActionPolicy(Args&&... args) noexcept(std::is_nothrow_constructible_v<F, Args&&...>) 
        : action(std::forward<Args>(args)...) 
    {}

    DefaultActionPolicy(DefaultActionPolicy&& other) noexcept(std::is_nothrow_move_constructible_v<F>) 
        : action(std::move(other.action)) 
    {}

    DefaultActionPolicy& operator=(DefaultActionPolicy&& other) noexcept(std::is_nothrow_move_assignable_v<F>) {
        if (this != &other) {
            action = std::move(other.action);
        }
        return *this;
    }

    void operator()() noexcept(noexcept(std::declval<F&>()())) {
        action();
    }

    DefaultActionPolicy(const DefaultActionPolicy&) = delete;
    DefaultActionPolicy& operator=(const DefaultActionPolicy&) = delete;
};

/**
 * @brief A generic RAII utility that executes a cleanup function upon scope exit.
 *
 * @details The ScopeGuard class provides automatic resource management through RAII. 
 * When a ScopeGuard object goes out of scope, it automatically executes the cleanup 
 * action that was provided at construction time.
 *
 * Key Features:
 * - Move-only semantics (copy operations are deleted)
 * - Early dismiss() to prevent execution
 * - Conditional dismiss_if() for conditional cleanup
 * - Customizable exception handling via policies
 * - Extensible action storage via action policies
 *
 * IMPORTANT: ScopeGuard is designed for single-threaded use only. Each instance 
 * should be accessed by only one thread. For multi-threaded resource management, 
 * protect the resource itself with appropriate synchronization primitives.
 *
 * Example:
 * @code
 * {
 *     int* ptr = new int(42);
 *     auto guard = makeScopeGuard([ptr]() { delete ptr; });
 *     // ... use ptr ...
 *     // ptr is automatically deleted when guard goes out of scope
 * }
 * @endcode
 *
 * @tparam F The type of the cleanup function object (e.g., a lambda).
 * @tparam ThrowingPolicy The policy to handle exceptions escaping F.
 * @tparam ActionPolicy The policy for action storage.
 */
template <typename F, typename ThrowingPolicy = ScopeGuardTerminatePolicy, template <typename> class ActionPolicy = DefaultActionPolicy>
class ScopeGuard {
public:
    using ActionStorage = ActionPolicy<F>;

    /**
     * @brief Constructs the ScopeGuard with the cleanup action.
     * 
     * @details Moves the action into storage. The action will be executed when 
     * the ScopeGuard is destroyed, unless dismiss() is called first.
     * 
     * @param action The function object to execute on scope exit.
     */
    explicit ScopeGuard(F&& action) noexcept(std::is_nothrow_constructible_v<ActionStorage, F&&>) 
        : m_action_storage(std::forward<F>(action))
        , m_execute(true) 
    {
        FATP_DEBUG_ENFORCE(&m_action_storage, "ScopeGuard constructed with null action storage");
    }

    /**
     * @brief In-place constructs the cleanup action from arguments.
     * 
     * @details Uses perfect forwarding to build F in storage. Useful for complex 
     * callables that aren't easily moveable.
     * 
     * @tparam Args Argument types for F's constructor.
     * @param args Arguments forwarded to F's constructor.
     */
    template <typename... Args, typename = std::enable_if_t<std::is_constructible_v<ActionStorage, Args...> && 
        !(sizeof...(Args) == 1 && std::is_same_v<std::decay_t<std::tuple_element_t<0, std::tuple<Args...>>>, ScopeGuard>)>>
    explicit ScopeGuard(Args&&... args) noexcept(std::is_nothrow_constructible_v<ActionStorage, Args&&...>) 
        : m_action_storage(std::forward<Args>(args)...)
        , m_execute(true) 
    {
        FATP_DEBUG_ENFORCE(&m_action_storage, "ScopeGuard emplace-constructed with null action storage");
    }

    /**
     * @brief Move constructor. Transfers the action and disables the source.
     * 
     * @details The action storage is move-constructed in the initializer list 
     * (required for non-default-constructible types like lambdas). The source 
     * guard is disabled after the move.
     * 
     * @param other The source ScopeGuard to move from.
     */
    ScopeGuard(ScopeGuard&& other) noexcept 
        : m_action_storage(std::move(other.m_action_storage))
        , m_execute(other.m_execute) 
    {
        other.m_execute = false;
    }

    /**
     * @brief Move assignment operator.
     * 
     * @details Executes the current action if still active before transferring 
     * from source.
     * 
     * @param other The source ScopeGuard to move from.
     * @return Reference to this object.
     */
    ScopeGuard& operator=(ScopeGuard&& other) noexcept(std::is_nothrow_move_assignable_v<ActionStorage>) {
        if (this != &other) {
            if (m_execute) {
                ScopeGuardPolicyExecutor<ActionStorage, ThrowingPolicy>::execute(m_action_storage);
            }

            m_action_storage = std::move(other.m_action_storage);
            m_execute = other.m_execute;
            other.m_execute = false;
        }
        return *this;
    }

    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;

    /**
     * @brief Executes the cleanup action if not dismissed.
     * 
     * @details Delegates exception handling to the ThrowingPolicy.
     * 
     * @warning When using ScopeGuardRethrowPolicy, this destructor is potentially 
     * throwing! This can lead to std::terminate() if an exception is already in 
     * flight during stack unwinding. Use Rethrow policy only in controlled 
     * environments (testing, debugging).
     */
    ~ScopeGuard() noexcept(!std::is_same_v<ThrowingPolicy, ScopeGuardRethrowPolicy>) {
        if (m_execute) {
            ScopeGuardPolicyExecutor<ActionStorage, ThrowingPolicy>::execute(m_action_storage);
        }
    }

    /**
     * @brief Disables the execution of the cleanup action on scope exit.
     * 
     * @details After calling dismiss(), the action will not run in the destructor. 
     * This is useful when the cleanup is no longer needed (e.g., because the 
     * resource was successfully transferred elsewhere).
     */
    void dismiss() noexcept {
        m_execute = false;
    }

    /**
     * @brief Conditionally dismisses based on a boolean condition.
     * 
     * @details If cond is true, dismisses the guard. Otherwise, no operation.
     * 
     * @param cond If true, dismiss; else keep active.
     */
    void dismiss_if(bool cond) noexcept {
        if (cond) {
            m_execute = false;
        }
    }

    /**
     * @brief Check if the guard is still active (will execute on destruction).
     * 
     * @return true if the action will execute, false if dismissed.
     */
    [[nodiscard]] bool is_active() const noexcept {
        return m_execute;
    }

private:
    ActionStorage m_action_storage;
    bool m_execute;
};

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

struct DefaultScopeGuardMaker {};

inline DefaultScopeGuardMaker MakeScopeGuard() {
    return {};
}

template <typename Fn>
[[nodiscard]] ScopeGuard<std::decay_t<Fn>> operator+(DefaultScopeGuardMaker, Fn&& fn) {
    return ScopeGuard<std::decay_t<Fn>>(std::forward<Fn>(fn));
}

template <typename Policy>
struct ScopeGuardMaker {};

template <typename Policy>
inline ScopeGuardMaker<Policy> MakeScopeGuard() {
    return {};
}

template <typename Policy, typename Fn>
[[nodiscard]] ScopeGuard<std::decay_t<Fn>, Policy> operator+(ScopeGuardMaker<Policy>, Fn&& fn) {
    return ScopeGuard<std::decay_t<Fn>, Policy>(std::forward<Fn>(fn));
}

#define GET_NOEXCEPT_ScopeGuardNothrowPolicy noexcept
#define GET_NOEXCEPT_ScopeGuardTerminatePolicy
#define GET_NOEXCEPT_ScopeGuardLogAndSwallowPolicy
#define GET_NOEXCEPT_ScopeGuardRethrowPolicy
#define GET_NOEXCEPT(PolicyTag) GET_NOEXCEPT_##PolicyTag

#if defined(__COUNTER__)
    #define FATP_SCOPE_GUARD_UNIQUE(prefix) FATP_SCOPE_GUARD_CONCAT_IMPL(prefix, __COUNTER__)
#else
    #define FATP_SCOPE_GUARD_UNIQUE(prefix) FATP_SCOPE_GUARD_CONCAT_IMPL(prefix, __LINE__)
#endif

#define FATP_SCOPE_GUARD_CONCAT_IMPL(a, b) FATP_SCOPE_GUARD_CONCAT_IMPL2(a, b)
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
#define SCOPE_GUARD auto FATP_SCOPE_GUARD_UNIQUE(scope_guard_) = ::fat_p::MakeScopeGuard() + [&]()

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
#define SCOPE_GUARD_EX(PolicyTag) auto FATP_SCOPE_GUARD_UNIQUE(scope_guard_) = ::fat_p::MakeScopeGuard<PolicyTag>() + [&]() GET_NOEXCEPT(PolicyTag)

}
