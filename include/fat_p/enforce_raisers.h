#pragma once

/*
FATP_META:
  meta_version: 1
  component: enforce_raisers
  file_role: public_header
  path: include/fat_p/enforce_raisers.h
  namespace: fat_p
  layer: Foundation
  summary: "Public header for enforce_raisers."
  api_stability: in_work
  related:
    docs_search: "enforce_raisers"
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 3
    defines_unprefixed: 1
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/

/**
 * @file enforce_raisers.h
 * @brief Defines the policy classes that determine how a contract failure
 * is handled (e.g., throw exception, log warning, abort, or ignore).
 *
 *
 *
 * @details This system centralizes error consequence definition using
 * policy classes (Raisers), allowing enforcement macros to maintain a
 * clean, consistent interface while providing flexible failure handling
 * tailored to different contexts (logic, runtime, allocation).
 */

#if !defined(FATP_USE_IOSTREAM)
#define FATP_USE_IOSTREAM 1
#endif

#if !defined(FATP_USE_ATOMIC)
#define FATP_USE_ATOMIC 1
#endif

#if FATP_USE_IOSTREAM
#include <iostream>
#endif

#if FATP_USE_ATOMIC
#include <atomic>
#endif

#include <cstdlib>
#include <functional>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>

#include "ContractException.h"
#include "Expected.h"

namespace fat_p
{

// ============================================================================
// Local SingleThreadedPolicy stub (Foundation layer independence)
// ============================================================================
// This minimal policy avoids including ConcurrencyPolicies.h (Concurrency layer)
// from this Foundation-layer header. Users needing thread-safe CustomRaiser
// should include ConcurrencyPolicies.h and specify the policy explicitly:
//   #include "ConcurrencyPolicies.h"
//   using MyRaiser = CustomRaiser<MyException, fat_p::SingleThreadedPolicy>;
//   // Or for thread-safe:
//   using MyRaiser = CustomRaiser<MyException, fat_p::MutexSynchronizationPolicy>;
// ============================================================================

namespace detail
{

struct LocalNoOpLock
{
};

struct LocalSingleThreadedPolicy
{
    using LockType = LocalNoOpLock;

    struct SharedGuard
    {
        template <typename T>
        explicit SharedGuard(T&) noexcept
        {
        }
    };

    static LocalNoOpLock& getStaticLock()
    {
        static LocalNoOpLock lock;
        return lock;
    }
};

} // namespace detail

using ViolationHandlerFunction = std::function<void(const std::string&)>;

namespace detail
{

inline std::mutex& getViolationMutex()
{
    static std::mutex mtx;
    return mtx;
}

inline void writeToStderr(const std::string& prefix, const std::string& message)
{
#if FATP_USE_IOSTREAM
    std::lock_guard<std::mutex> lock(getViolationMutex());
    std::cerr << prefix << message << std::endl;
#else
    (void)prefix;
    (void)message;
#endif
}

} // namespace detail

/**
 * @brief Global function used as the default handler for non-throwing
 * contract failures.
 *
 * @details This default handler prints a message to standard error. In
 * debug builds (when NDEBUG is NOT defined), it calls `std::abort()`
 * immediately after printing to ensure rapid detection of internal bugs.
 *
 * @param message The diagnostic message detailing the violation.
 */
inline void default_violation_handler(const std::string& message)
{
    detail::writeToStderr("CONTRACT VIOLATION: ", message);
#ifndef NDEBUG
    std::abort();
#endif
}

namespace detail
{

struct ViolationHandlerStorage
{
    std::mutex mutex;
    ViolationHandlerFunction handler = default_violation_handler;

    static ViolationHandlerStorage& instance()
    {
        static ViolationHandlerStorage storage;
        return storage;
    }
};

} // namespace detail

/**
 * @brief Set a custom violation handler for non-throwing contract failures.
 *
 * @details This function is thread-safe. The handler will be called by
 * NoThrowRaiser when a contract violation occurs. The default handler
 * logs to stderr and aborts in debug builds.
 *
 * @param new_handler The new handler function to install.
 */
inline void set_violation_handler(ViolationHandlerFunction new_handler)
{
    auto& storage = detail::ViolationHandlerStorage::instance();
    std::lock_guard<std::mutex> lock(storage.mutex);
    storage.handler = std::move(new_handler);
}

/**
 * @brief Get the current violation handler.
 *
 * @details This function is thread-safe.
 *
 * @return A copy of the current handler function.
 */
inline ViolationHandlerFunction get_violation_handler()
{
    auto& storage = detail::ViolationHandlerStorage::instance();
    std::lock_guard<std::mutex> lock(storage.mutex);
    return storage.handler;
}

/**
 * @brief Reset the violation handler to the default.
 *
 * @details This function is thread-safe.
 */
inline void reset_violation_handler()
{
    set_violation_handler(default_violation_handler);
}

/**
 * @brief A policy base class that throws an exception of type E on failure.
 *
 * @tparam E The exception type to throw. Must inherit from `std::exception`
 * and be constructible from `const std::string&`.
 * @tparam ConcurrencyPolicy Policy for thread-safety in raisers.
 */
template <typename E, typename ConcurrencyPolicy = detail::LocalSingleThreadedPolicy>
struct CustomRaiser : public ConcurrencyPolicy
{
    static_assert(std::is_constructible<E, const std::string&>::value,
                  "E must be constructible from const std::string&");

    /**
     * @brief Logs the failure message and throws the specified exception.
     * @param message The diagnostic message detailing the violation.
     */
    static void fail(const std::string& message)
    {
        typename ConcurrencyPolicy::SharedGuard guard(CustomRaiser::getStaticLock());
        detail::writeToStderr("Exception: ", message);
        throw E(message);
    }
};

/**
 * @brief Raiser for standard logical contract checks (preconditions, invariants).
 *
 * @details Used by the default enforcement macros (e.g., 'enforce' or
 * 'always_enforce'). Throws `LogicContractError` which inherits `std::logic_error`.
 */
struct LogicRaiser : CustomRaiser<LogicContractError>
{
};

/**
 * @brief Raiser for general runtime contract checks.
 *
 * @details Used for environmental or resource checks that are not
 * specific to memory allocation (e.g., file system availability, timeout
 * failures). Throws `RuntimeContractError` which inherits `std::runtime_error`.
 */
struct RuntimeRaiser : CustomRaiser<RuntimeContractError>
{
};

/**
 * @brief Raiser for memory allocation contract checks.
 *
 * @details Used by checks against memory resource limits (e.g., stack
 * or pool allocators). Throws `AllocContractError` which inherits
 * `std::runtime_error` and is semantically similar to `std::bad_alloc`.
 */
struct AllocRaiser : CustomRaiser<AllocContractError>
{
};

/**
 * @brief Raiser that causes immediate, fatal termination of the program.
 *
 * @details Used by specialized macros (e.g., 'abort_enforce') for
 * unrecoverable state corruption where throwing an exception is not safe
 * or desirable.
 */
struct AbortRaiser
{
    /**
     * @brief Logs the failure message to stderr and calls std::abort().
     * @param message The diagnostic message detailing the violation.
     */
    static void fail(const std::string& message)
    {
        detail::writeToStderr("CONTRACT ABORT: ", message);
        std::abort();
    }
};

/**
 * @brief Raiser used for warnings. Logs the message to cerr and continues execution.
 *
 * @details Used by macros like 'enforce_warn'. This policy ensures that
 * execution continues after logging the violation.
 */
struct WarningToCerrRaiser
{
    /**
     * @brief Logs the failure message as a warning to std::cerr.
     * @param message The diagnostic message detailing the violation.
     */
    static void fail(const std::string& message)
    {
        detail::writeToStderr("CONTRACT WARNING: ", message);
    }
};

/**
 * @brief Raiser for non-throwing contracts. Calls the global violation handler and
 * continues.
 *
 * @details Used by macros like 'noexcept_enforce'. This ensures no
 * exception is thrown, making it safe for `noexcept` functions.
 */
struct NoThrowRaiser
{
    /**
     * @brief Calls the non-throwing violation handler.
     * @param message The diagnostic message detailing the violation.
     */
    static void fail(const std::string& message)
    {
        auto& storage = detail::ViolationHandlerStorage::instance();
        std::lock_guard<std::mutex> lock(storage.mutex);
        storage.handler(message);
    }
};

/**
 * @brief The zero-overhead raiser. Its fail method contains no code.
 *
 * @details Used by 'enforce' in release mode (when NDEBUG is defined)
 * to achieve zero runtime cost.
 */
struct NoOpRaiser
{
    /**
     * @brief Performs no action, ensuring zero overhead.
     * @param message The diagnostic message (ignored).
     */
    static void fail(const std::string&)
    {
    }
};

/**
 * @brief Throws `std::invalid_argument`.
 * @details Inherits `std::logic_error`. Used for checks on function
 * arguments that are logically invalid.
 */
struct InvalidArgumentRaiser : CustomRaiser<std::invalid_argument>
{
};

/**
 * @brief Throws `std::out_of_range`.
 * @details Inherits `std::logic_error`. Used for checks on container
 * indices or position arguments.
 */
struct OutOfRangeRaiser : CustomRaiser<std::out_of_range>
{
};

/**
 * @brief Throws `std::domain_error`.
 * @details Inherits `std::logic_error`. Used for checks where a result
 * cannot be represented (e.g., a math function input is out of its domain).
 */
struct DomainErrorRaiser : CustomRaiser<std::domain_error>
{
};

/**
 * @brief Throws `std::length_error`.
 * @details Inherits `std::logic_error`. Used for checks where an operation
 * would exceed the maximum allowable length of an object or container.
 */
struct LengthErrorRaiser : CustomRaiser<std::length_error>
{
};

/**
 * @brief Throws `std::overflow_error`.
 * @details Inherits `std::runtime_error`. Used for checks on arithmetic
 * operations resulting in an overflow.
 */
struct OverflowRaiser : CustomRaiser<std::overflow_error>
{
};

/**
 * @brief Throws `std::system_error`.
 * @details Inherits `std::runtime_error`. Used for checks on operations
 * that fail due to an underlying operating system or I/O resource error.
 */
struct SystemErrorRaiser
{
    static void fail(const std::string& message)
    {
        detail::writeToStderr("Exception: ", message);
        throw std::system_error(std::make_error_code(std::errc::state_not_recoverable), message);
    }
};

/**
 * @brief Macro to define a custom raiser for a user-defined exception type.
 *
 * @details This creates a struct CustomRaiserName that throws ExcType with
 * a prefixed message. ExcType must derive from std::exception and be
 * constructible from std::string.
 *
 * @param CustomRaiserName The name of the new raiser struct (e.g., MyRaiser).
 * @param ExceptionType The custom exception type (e.g., MyCustomError).
 * @param prefix A string literal for the message prefix (e.g., "My Error: ").
 */
#define FATP_DEFINE_CUSTOM_RAISER(CustomRaiserName, ExceptionType, prefix)               \
    struct CustomRaiserName                                                              \
    {                                                                                    \
        static_assert(std::is_base_of_v<std::exception, ExceptionType>,                  \
                      #ExceptionType " must derive from std::exception.");               \
        static_assert(std::is_constructible_v<ExceptionType, const std::string&>,        \
                      #ExceptionType " must be constructible from const std::string&."); \
                                                                                         \
        static void fail(const std::string& message)                                     \
        {                                                                                \
            ::fat_p::detail::writeToStderr("Exception: ", message);                      \
            throw ExceptionType(std::string(prefix) + message);                          \
        }                                                                                \
    };

template <typename E>
struct ExpectedException : std::exception
{
    E error;

    explicit ExpectedException(E err)
        : error(std::move(err))
    {
    }

    const char* what() const noexcept override
    {
        return "Expected enforcement failed";
    }
};

template <typename E = std::string>
struct ExpectedRaiser
{
    /**
     * @brief Throws ExpectedException with the error message.
     * @param message The diagnostic message.
     */
    [[noreturn]] static void fail(const std::string& message)
    {
        throw ExpectedException<E>(E(message));
    }
};

} // namespace fat_p
