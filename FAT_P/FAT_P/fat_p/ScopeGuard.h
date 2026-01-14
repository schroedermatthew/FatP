/**
 * @file ScopeGuard.h
 * @brief Provides the ScopeGuard class for general-purpose RAII scope-exit cleanup.
 *
 *
 *
 * @layer Foundation
 *
 * @details This RAII utility executes a user-provided cleanup action upon scope exit,
 * with customizable exception handling via policies. It supports move semantics for
 * transfer, early dismiss to disable execution, and in-place construction for complex
 * actions. Policies (defined in ScopeGuardPolicies.h) control destructor behavior,
 * ensuring compliance with noexcept specifications and offering options for
 * termination, logging, or re-throwing.
 *
 * IMPORTANT - LAMBDA CONTROL FLOW WARNING:
 * The macros (FATP_SCOPE_GUARD, FATP_SCOPE_EXIT, FATP_SCOPE_FAIL, FATP_SCOPE_SUCCESS) create lambdas.
 * - 'return' inside the block returns from the LAMBDA, not the enclosing function!
 * - 'break' and 'continue' are NOT valid inside the block.
 * - This differs from D's scope(exit) or some other languages.
 *
 * Example of WRONG usage:
 * @code
 * void process() {
 *     FATP_SCOPE_EXIT {
 *         if (error) return;  // WRONG: Returns from lambda, cleanup continues!
 *         cleanup();
 *     };
 *     // ...
 * }
 * @endcode
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

/*
FATP_META:
  meta_version: 1
  component: ScopeGuard
  file_role: public_header
  path: fat_p/ScopeGuard.h
  namespace: fat_p
  layer: Foundation
  summary: "Public header for ScopeGuard."
  api_stability: in_work
  related:
    docs_search: "ScopeGuard"
    tests:
      - tests/test_ScopeGuard.cpp
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 14
    defines_unprefixed: 10
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/
#include <exception>
#include <type_traits>
#include <utility>

#include "FatPTypeTraits.h"
#include "ScopeGuardPolicies.h"

namespace fat_p
{

/**
 * @brief Default storage policy: holds F directly.
 *
 * @details Provides move construction, move assignment, and invocation.
 * Copy operations are explicitly deleted to enforce unique ownership.
 */
template <typename F>
struct DefaultActionPolicy
{
    F action;

    DefaultActionPolicy() = default;

    template <typename... Args>
    explicit DefaultActionPolicy(Args&&... args) noexcept(std::is_nothrow_constructible_v<F, Args&&...>)
        : action(std::forward<Args>(args)...)
    {
    }

    DefaultActionPolicy(DefaultActionPolicy&& other) noexcept(std::is_nothrow_move_constructible_v<F>)
        : action(std::move(other.action))
    {
    }

    DefaultActionPolicy& operator=(DefaultActionPolicy&& other) noexcept(std::is_nothrow_move_assignable_v<F>)
    {
        if (this != &other)
        {
            action = std::move(other.action);
        }
        return *this;
    }

    void operator()() noexcept(noexcept(std::declval<F&>()()))
    {
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
template <typename F,
          typename ThrowingPolicy = ScopeGuardTerminatePolicy,
          template <typename> class ActionPolicy = DefaultActionPolicy>
class ScopeGuard
{
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
    template <typename... Args,
              typename = std::enable_if_t<
                  std::is_constructible_v<ActionStorage, Args...> &&
                  !(sizeof...(Args) == 1 &&
                    std::is_same_v<std::decay_t<std::tuple_element_t<0, std::tuple<Args...>>>, ScopeGuard>)>>
    explicit ScopeGuard(Args&&... args) noexcept(std::is_nothrow_constructible_v<ActionStorage, Args&&...>)
        : m_action_storage(std::forward<Args>(args)...)
        , m_execute(true)
    {
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
    ScopeGuard(ScopeGuard&& other) noexcept(std::is_nothrow_move_constructible_v<ActionStorage>)
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
    ScopeGuard& operator=(ScopeGuard&& other) noexcept(std::is_nothrow_move_assignable_v<ActionStorage>)
    {
        if (this != &other)
        {
            if (m_execute)
            {
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
    ~ScopeGuard() noexcept(!std::is_same_v<ThrowingPolicy, ScopeGuardRethrowPolicy>)
    {
        if (m_execute)
        {
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
    void dismiss() noexcept
    {
        m_execute = false;
    }

    /**
     * @brief Conditionally dismisses based on a boolean condition.
     *
     * @details If cond is true, dismisses the guard. Otherwise, no operation.
     *
     * @param cond If true, dismiss; else keep active.
     */
    void dismiss_if(bool cond) noexcept
    {
        if (cond)
        {
            m_execute = false;
        }
    }

    /**
     * @brief Check if the guard is still active (will execute on destruction).
     *
     * @return true if the action will execute, false if dismissed.
     */
    [[nodiscard]] bool is_active() const noexcept
    {
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
 *
 * @warning The returned guard uses a lambda internally. 'return' inside the cleanup
 * function returns from the lambda, not the enclosing function.
 */
template <typename F>
[[nodiscard]] auto makeScopeGuard(F&& fn) noexcept(std::is_nothrow_constructible_v<ScopeGuard<std::decay_t<F>>, F&&>)
{
    return ScopeGuard<std::decay_t<F>>(std::forward<F>(fn));
}

/**
 * @brief Create a ScopeGuard with a specific throwing policy.
 *
 * @tparam Policy The throwing policy to use.
 * @tparam F The type of the cleanup function.
 * @param fn The cleanup function.
 * @return A ScopeGuard object with the specified policy.
 *
 * @warning The returned guard uses a lambda internally. 'return' inside the cleanup
 * function returns from the lambda, not the enclosing function.
 */
template <typename Policy, typename F>
[[nodiscard]] auto
makeScopeGuard(F&& fn) noexcept(std::is_nothrow_constructible_v<ScopeGuard<std::decay_t<F>, Policy>, F&&>)
{
    return ScopeGuard<std::decay_t<F>, Policy>(std::forward<F>(fn));
}

struct DefaultScopeGuardMaker
{
};

inline DefaultScopeGuardMaker MakeScopeGuard()
{
    return {};
}

template <typename Fn>
[[nodiscard]] ScopeGuard<std::decay_t<Fn>> operator+(DefaultScopeGuardMaker, Fn&& fn)
{
    return ScopeGuard<std::decay_t<Fn>>(std::forward<Fn>(fn));
}

template <typename Policy>
struct ScopeGuardMaker
{
};

template <typename Policy>
inline ScopeGuardMaker<Policy> MakeScopeGuard()
{
    return {};
}

template <typename Policy, typename Fn>
[[nodiscard]] ScopeGuard<std::decay_t<Fn>, Policy> operator+(ScopeGuardMaker<Policy>, Fn&& fn)
{
    return ScopeGuard<std::decay_t<Fn>, Policy>(std::forward<Fn>(fn));
}

#define FATP_GET_NOEXCEPT_ScopeGuardNothrowPolicy noexcept
#define FATP_GET_NOEXCEPT_ScopeGuardTerminatePolicy
#define FATP_GET_NOEXCEPT_ScopeGuardLogAndSwallowPolicy
#define FATP_GET_NOEXCEPT_ScopeGuardRethrowPolicy
#define FATP_GET_NOEXCEPT(PolicyTag) FATP_GET_NOEXCEPT_##PolicyTag

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
 * @details Usage: FATP_SCOPE_GUARD { cleanup code; };
 *
 * @warning 'return' inside the block returns from the lambda, NOT the enclosing function.
 * @warning 'break' and 'continue' are not valid inside the block.
 *
 * Example:
 * @code
 * int* ptr = new int(42);
 * FATP_SCOPE_GUARD { delete ptr; };
 * @endcode
 */
#define FATP_SCOPE_GUARD auto FATP_SCOPE_GUARD_UNIQUE(scope_guard_) = ::fat_p::MakeScopeGuard() + [&]()

/**
 * @brief Factory macro to create a ScopeGuard with an explicit policy.
 *
 * @param PolicyTag The desired policy (e.g., ScopeGuardTerminatePolicy).
 *
 * @details Usage: FATP_SCOPE_GUARD_EX(PolicyTag) { cleanup code; };
 *
 * @warning 'return' inside the block returns from the lambda, NOT the enclosing function.
 * @warning 'break' and 'continue' are not valid inside the block.
 *
 * Example:
 * @code
 * FATP_SCOPE_GUARD_EX(ScopeGuardNothrowPolicy) noexcept {
 *     // cleanup code that must not throw
 * };
 * @endcode
 */
#define FATP_SCOPE_GUARD_EX(PolicyTag)           \
    auto FATP_SCOPE_GUARD_UNIQUE(scope_guard_) = \
        ::fat_p::MakeScopeGuard<PolicyTag>() + [&]() FATP_GET_NOEXCEPT(PolicyTag)

/**
 * @brief Alias for FATP_SCOPE_GUARD - executes on any scope exit.
 *
 * @details This is an alias for FATP_SCOPE_GUARD that matches common naming conventions
 * used in other libraries (Folly, Boost.ScopeExit).
 *
 * @warning 'return' inside the block returns from the lambda, NOT the enclosing function.
 * @warning 'break' and 'continue' are not valid inside the block.
 *
 * Example:
 * @code
 * FILE* f = fopen("data.txt", "r");
 * FATP_SCOPE_EXIT { if (f) fclose(f); };
 * @endcode
 */
#define FATP_SCOPE_EXIT FATP_SCOPE_GUARD

// =============================================================================
// Exception-Aware Scope Guards (C++17 std::uncaught_exceptions)
// =============================================================================

/**
 * @brief Scope guard that executes only when leaving scope due to an exception.
 *
 * @details Uses std::uncaught_exceptions() to detect if an exception is in flight.
 * The action executes only if more exceptions are uncaught at destruction than
 * at construction, indicating the scope is being exited due to stack unwinding.
 *
 * Example:
 * @code
 * void transfer_money(Account& from, Account& to, Money amount) {
 *     from.withdraw(amount);
 *     FATP_SCOPE_FAIL { from.deposit(amount); };  // Rollback on exception
 *     to.deposit(amount);  // May throw
 * }
 * @endcode
 */
template <typename F>
class ScopeGuardOnFail
{
public:
    explicit ScopeGuardOnFail(F&& action) noexcept(std::is_nothrow_move_constructible_v<F>)
        : m_action(std::forward<F>(action))
        , m_uncaught_exceptions(std::uncaught_exceptions())
        , m_active(true)
    {
    }

    ScopeGuardOnFail(ScopeGuardOnFail&& other) noexcept(std::is_nothrow_move_constructible_v<F>)
        : m_action(std::move(other.m_action))
        , m_uncaught_exceptions(other.m_uncaught_exceptions)
        , m_active(other.m_active)
    {
        other.m_active = false;
    }

    ~ScopeGuardOnFail() noexcept
    {
        if (m_active && std::uncaught_exceptions() > m_uncaught_exceptions)
        {
            try
            {
                m_action();
            }
            catch (...)
            {
                // Swallow exceptions in destructor during stack unwinding
            }
        }
    }

    void dismiss() noexcept
    {
        m_active = false;
    }
    [[nodiscard]] bool is_active() const noexcept
    {
        return m_active;
    }

    ScopeGuardOnFail(const ScopeGuardOnFail&) = delete;
    ScopeGuardOnFail& operator=(const ScopeGuardOnFail&) = delete;

    /**
     * @brief Move assignment operator.
     * @details If this guard is active and we are currently unwinding due to an exception,
     * executes the current action before taking ownership of the source's action.
     * Adopts the source's uncaught_exceptions baseline to maintain correct behavior.
     */
    ScopeGuardOnFail& operator=(ScopeGuardOnFail&& other) noexcept(std::is_nothrow_move_assignable_v<F>)
    {
        if (this != &other)
        {
            // Execute current action if active and unwinding
            if (m_active && std::uncaught_exceptions() > m_uncaught_exceptions)
            {
                try
                {
                    m_action();
                }
                catch (...)
                {
                    // Swallow - we're about to overwrite anyway
                }
            }

            m_action = std::move(other.m_action);
            m_uncaught_exceptions = other.m_uncaught_exceptions;
            m_active = other.m_active;
            other.m_active = false;
        }
        return *this;
    }

private:
    F m_action;
    int m_uncaught_exceptions;
    bool m_active;
};

/**
 * @brief Scope guard that executes only when leaving scope normally (no exception).
 *
 * @details Uses std::uncaught_exceptions() to detect normal scope exit.
 * The action executes only if the same number of exceptions are uncaught at
 * destruction as at construction, indicating normal (non-exceptional) exit.
 *
 * Example:
 * @code
 * void process_transaction() {
 *     begin_transaction();
 *     FATP_SCOPE_SUCCESS { commit_transaction(); };  // Only on success
 *     FATP_SCOPE_FAIL { rollback_transaction(); };   // Only on failure
 *     do_work();  // May throw
 * }
 * @endcode
 */
template <typename F>
class ScopeGuardOnSuccess
{
public:
    explicit ScopeGuardOnSuccess(F&& action) noexcept(std::is_nothrow_move_constructible_v<F>)
        : m_action(std::forward<F>(action))
        , m_uncaught_exceptions(std::uncaught_exceptions())
        , m_active(true)
    {
    }

    ScopeGuardOnSuccess(ScopeGuardOnSuccess&& other) noexcept(std::is_nothrow_move_constructible_v<F>)
        : m_action(std::move(other.m_action))
        , m_uncaught_exceptions(other.m_uncaught_exceptions)
        , m_active(other.m_active)
    {
        other.m_active = false;
    }

    ~ScopeGuardOnSuccess() noexcept
    {
        if (m_active && std::uncaught_exceptions() == m_uncaught_exceptions)
        {
            try
            {
                m_action();
            }
            catch (...)
            {
                // Swallow exceptions - we're in a destructor
            }
        }
    }

    void dismiss() noexcept
    {
        m_active = false;
    }
    [[nodiscard]] bool is_active() const noexcept
    {
        return m_active;
    }

    ScopeGuardOnSuccess(const ScopeGuardOnSuccess&) = delete;
    ScopeGuardOnSuccess& operator=(const ScopeGuardOnSuccess&) = delete;

    /**
     * @brief Move assignment operator.
     * @details If this guard is active and we are NOT unwinding, executes the current
     * action before taking ownership of the mSource_LIT_0__s
     * uncaught_exceptions baseline to maintain correct behavior.
     */
    ScopeGuardOnSuccess& operator=(ScopeGuardOnSuccess&& other) noexcept(std::is_nothrow_move_assignable_v<F>)
    {
        if (this != &other)
        {
            // Execute current action if active and NOT unwinding
            if (m_active && std::uncaught_exceptions() == m_uncaught_exceptions)
            {
                try
                {
                    m_action();
                }
                catch (...)
                {
                    // Swallow - we're about to overwrite anyway
                }
            }

            m_action = std::move(other.m_action);
            m_uncaught_exceptions = other.m_uncaught_exceptions;
            m_active = other.m_active;
            other.m_active = false;
        }
        return *this;
    }

private:
    F m_action;
    int m_uncaught_exceptions;
    bool m_active;
};

/**
 * @brief Create a ScopeGuardOnFail that executes only on exception.
 *
 * @tparam F The type of the cleanup function.
 * @param fn The cleanup function to execute on exception.
 * @return A ScopeGuardOnFail object.
 *
 * @warning 'return' inside the cleanup function returns from the lambda, not the
 * enclosing function.
 */
template <typename F>
[[nodiscard]] auto
makeScopeGuardOnFail(F&& fn) noexcept(std::is_nothrow_constructible_v<ScopeGuardOnFail<std::decay_t<F>>, F&&>)
{
    return ScopeGuardOnFail<std::decay_t<F>>(std::forward<F>(fn));
}

/**
 * @brief Create a ScopeGuardOnSuccess that executes only on normal exit.
 *
 * @tparam F The type of the cleanup function.
 * @param fn The cleanup function to execute on normal exit.
 * @return A ScopeGuardOnSuccess object.
 *
 * @warning 'return' inside the cleanup function returns from the lambda, not the
 * enclosing function.
 */
template <typename F>
[[nodiscard]] auto
makeScopeGuardOnSuccess(F&& fn) noexcept(std::is_nothrow_constructible_v<ScopeGuardOnSuccess<std::decay_t<F>>, F&&>)
{
    return ScopeGuardOnSuccess<std::decay_t<F>>(std::forward<F>(fn));
}

// Maker structs for macro support
struct ScopeGuardOnFailMaker
{
};
struct ScopeGuardOnSuccessMaker
{
};

inline ScopeGuardOnFailMaker MakeScopeGuardOnFail()
{
    return {};
}
inline ScopeGuardOnSuccessMaker MakeScopeGuardOnSuccess()
{
    return {};
}

template <typename Fn>
[[nodiscard]] ScopeGuardOnFail<std::decay_t<Fn>> operator+(ScopeGuardOnFailMaker, Fn&& fn)
{
    return ScopeGuardOnFail<std::decay_t<Fn>>(std::forward<Fn>(fn));
}

template <typename Fn>
[[nodiscard]] ScopeGuardOnSuccess<std::decay_t<Fn>> operator+(ScopeGuardOnSuccessMaker, Fn&& fn)
{
    return ScopeGuardOnSuccess<std::decay_t<Fn>>(std::forward<Fn>(fn));
}

/**
 * @brief Macro for scope guard that executes only on exception (failure).
 *
 * @details Usage: FATP_SCOPE_FAIL { rollback_code; };
 *
 * @warning 'return' inside the block returns from the lambda, NOT the enclosing function.
 * @warning 'break' and 'continue' are not valid inside the block.
 *
 * @note The lambda is intentionally NOT noexcept. If the cleanup throws, the destructor's
 * internal try-catch will swallow the exception (we're already unwinding due to another
 * exception, so throwing would call std::terminate anyway).
 */
#define FATP_SCOPE_FAIL auto FATP_SCOPE_GUARD_UNIQUE(scope_fail_) = ::fat_p::MakeScopeGuardOnFail() + [&]()

/**
 * @brief Macro for scope guard that executes only on normal exit (success).
 *
 * @details Usage: FATP_SCOPE_SUCCESS { commit_code; };
 *
 * @warning 'return' inside the block returns from the lambda, NOT the enclosing function.
 * @warning 'break' and 'continue' are not valid inside the block.
 *
 * @note The lambda is intentionally NOT noexcept. If the cleanup throws, the destructor's
 * internal try-catch will swallow the exception to maintain destructor noexcept guarantee.
 */
#define FATP_SCOPE_SUCCESS auto FATP_SCOPE_GUARD_UNIQUE(scope_success_) = ::fat_p::MakeScopeGuardOnSuccess() + [&]()

// =============================================================================
// Type Trait Specializations
// =============================================================================

template <typename F, typename ThrowingPolicy, template <typename> class ActionPolicy>
struct is_scope_guard<ScopeGuard<F, ThrowingPolicy, ActionPolicy>> : std::true_type
{
};

template <typename F>
struct is_scope_guard<ScopeGuardOnFail<F>> : std::true_type
{
};

template <typename F>
struct is_scope_guard<ScopeGuardOnSuccess<F>> : std::true_type
{
};

} // namespace fat_p
