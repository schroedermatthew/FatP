/**
 * @file ScopeGuardPolicies.h
 * @brief Defines policies governing the exception-handling behavior of ScopeGuard destructors.
 *
 * 
 *
 * @layer Foundation
 *
 * @details This file provides a set of policy tags and their corresponding executor 
 * specializations. Each policy dictates how exceptions from the cleanup action are 
 * managed in the ScopeGuard destructor. The policies ensure compliance with C++'s 
 * destructor exception safety rules while offering flexibility for different use cases, 
 * such as strict no-throw enforcement or diagnostic logging.
 *
 * Available Policies:
 * - ScopeGuardNothrowPolicy: Requires noexcept actions (compile-time check)
 * - ScopeGuardTerminatePolicy: Calls std::terminate() on exception (default)
 * - ScopeGuardLogAndSwallowPolicy: Logs and suppresses exceptions
 * - ScopeGuardRethrowPolicy: Re-throws exceptions (use with caution!)
 *
 * Requirements:
 * - C++17 or later
 * - No external dependencies beyond standard library
 * - Header-only
 * - Single-threaded use only
 */
#pragma once

/*
FATP_META:
  meta_version: 1
  component: ScopeGuardPolicies
  file_role: public_header
  path: fat_p/ScopeGuardPolicies.h
  namespace: fat_p
  layer: Foundation
  summary: "Public header for ScopeGuardPolicies."
  api_stability: in_work
  related:
    docs_search: "ScopeGuardPolicies"
    tests:
      - tests/test_ScopeGuard.cpp
  hygiene:
    pragma_once: true
    include_guard: true
    defines_total: 2
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/
#include <type_traits>
#include <string>
#include <iostream>
#include <exception>

namespace fat_p {

/**
 * @brief Enable exception support (default: enabled).
 * 
 * @details Set to 0 to disable exception handling in policies (useful for 
 * embedded systems or when compiling with -fno-exceptions).
 */
#ifndef FATP_USE_EXCEPTION
    #define FATP_USE_EXCEPTION 1
#endif

/**
 * @brief Enable diagnostic logging for ScopeGuard errors.
 * 
 * @details When enabled, ScopeGuardLogAndSwallowPolicy will output error 
 * messages to std::cerr. Set to 0 to disable all diagnostic output.
 */
#ifndef FATP_SCOPE_GUARD_LOG_ERRORS
    #define FATP_SCOPE_GUARD_LOG_ERRORS 1
#endif

/**
 * @brief Conditionally print error message to std::cerr.
 * 
 * @details Simple, single-threaded logging for cleanup failures. For 
 * multi-threaded applications, synchronization should be handled at a 
 * higher level if garbled output is a concern.
 * 
 * @param message The error message to print.
 */
#if FATP_SCOPE_GUARD_LOG_ERRORS
inline void conditionalPrintError(const std::string& message) {
    std::cerr << "[ScopeGuard ERROR] " << message << std::endl;
}

inline void conditionalPrintError(const char* message) {
    std::cerr << "[ScopeGuard ERROR] " << message << std::endl;
}

template <typename MessageGen, typename = std::enable_if_t<std::is_invocable_r_v<std::string, MessageGen>>>
inline void conditionalPrintError(MessageGen&& messageGen) {
    try {
        std::cerr << "[ScopeGuard ERROR] " << std::forward<MessageGen>(messageGen)() << std::endl;
    } catch (...) {
        std::cerr << "[ScopeGuard ERROR] Failed to generate error message" << std::endl;
    }
}
#else
inline void conditionalPrintError(const std::string&) { }
inline void conditionalPrintError(const char*) { }
template <typename MessageGen>
inline void conditionalPrintError(MessageGen&&) { }
#endif

/**
 * @brief Policy that requires the cleanup action to be noexcept.
 * 
 * @details This is the safest, strictest policy. If the action is not noexcept, 
 * a static_assert will fail at compile time.
 * 
 * Use when:
 * - Maximum safety is required
 * - Actions are guaranteed not to throw
 * - Compile-time verification is desired
 */
struct ScopeGuardNothrowPolicy {};

/**
 * @brief Policy that catches any exception and calls std::terminate().
 * 
 * @details This is the default behavior if an exception escapes a destructor, 
 * but this policy makes that intent explicit for actions that might throw.
 * 
 * Use when:
 * - Action might throw but should be fatal
 * - Consistency with standard destructor behavior is desired
 * - Program should abort on cleanup failure
 * 
 * This is the default policy.
 */
struct ScopeGuardTerminatePolicy {};

/**
 * @brief Policy that catches any exception, logs a message, and suppresses the throw.
 * 
 * @details This allows a throwing action, but guarantees the destructor itself 
 * will not throw, which is essential for C++ safety. The log is configurable 
 * via FATP_SCOPE_GUARD_LOG_ERRORS.
 * 
 * Use when:
 * - Cleanup errors should be logged but not fatal
 * - Program should continue even if cleanup fails
 * - Debugging information is needed
 */
struct ScopeGuardLogAndSwallowPolicy {};

/**
 * @brief Policy that catches any exception and re-throws it.
 * 
 * @details This policy is useful for debugging or testing scenarios where 
 * propagating exceptions from destructors is desired, despite the risk of 
 * std::terminate() in noexcept contexts.
 * 
 * WARNING: This policy makes the destructor potentially throwing!
 * Use with extreme caution and only in controlled environments.
 * 
 * Use when:
 * - Testing exception behavior
 * - Debugging cleanup failures
 * - Controlled environments where throwing from dtors is acceptable
 * 
 * DO NOT use in production code unless you fully understand the implications.
 * If an exception is already in flight (during stack unwinding), re-throwing 
 * from a destructor will call std::terminate().
 */
struct ScopeGuardRethrowPolicy {};

/**
 * @brief Base template for executing the ScopeGuard action based on policy.
 * 
 * @tparam F The type of the cleanup function object.
 * @tparam Policy The policy tag defining exception behavior.
 */
template <typename F, typename Policy>
struct ScopeGuardPolicyExecutor;

/**
 * @brief Executor for ScopeGuardNothrowPolicy.
 * 
 * @details Enforces noexcept at compile time. The action is called directly 
 * without any exception handling overhead.
 */
template <typename F>
struct ScopeGuardPolicyExecutor<F, ScopeGuardNothrowPolicy> {
    static_assert(std::is_nothrow_invocable_v<F>, 
        "ScopeGuardNothrowPolicy requires action to be noexcept. " 
        "Either make your action noexcept or use a different policy.");

    static void execute(F& action) noexcept {
        action();
    }
};

/**
 * @brief Executor for ScopeGuardTerminatePolicy.
 * 
 * @details Catches any exception and calls std::terminate(). This matches the 
 * standard behavior when an exception escapes a destructor.
 */
template <typename F>
struct ScopeGuardPolicyExecutor<F, ScopeGuardTerminatePolicy> {
    static void execute(F& action) noexcept {
#if FATP_USE_EXCEPTION
        try {
            action();
        }
        catch (const std::exception& e) {
            conditionalPrintError([&]() -> std::string {
                return std::string("ScopeGuard action threw std::exception: ") + e.what() + " - Terminating...";
            });
            std::terminate();
        }
        catch (...) {
            conditionalPrintError("ScopeGuard action threw unknown exception - Terminating...");
            std::terminate();
        }
#else
        action();
#endif
    }
};

/**
 * @brief Executor for ScopeGuardLogAndSwallowPolicy.
 * 
 * @details Catches exceptions, logs via conditionalPrintError, and suppresses 
 * throwing. This ensures the destructor remains noexcept while still providing 
 * diagnostic information.
 */
template <typename F>
struct ScopeGuardPolicyExecutor<F, ScopeGuardLogAndSwallowPolicy> {
    static void execute(F& action) noexcept {
#if FATP_USE_EXCEPTION
        try {
            action();
        }
        catch (const std::exception& e) {
            conditionalPrintError([&]() -> std::string {
                return std::string("Action threw std::exception: ") + e.what() + " (exception swallowed)";
            });
        }
        catch (...) {
            conditionalPrintError("Action threw unknown exception (exception swallowed)");
        }
#else
        action();
#endif
    }
};

/**
 * @brief Executor for ScopeGuardRethrowPolicy.
 * 
 * @details Catches any exception and re-throws it. This makes the destructor 
 * potentially throwing!
 * 
 * WARNING: Use with extreme caution. This violates the normal expectation that 
 * destructors are noexcept.
 * 
 * Typical use cases:
 * - Unit testing exception behavior
 * - Debugging cleanup failures in controlled environments
 * - Non-destructor contexts (e.g., explicit release() methods)
 */
template <typename F>
struct ScopeGuardPolicyExecutor<F, ScopeGuardRethrowPolicy> {
    static void execute(F& action) noexcept(false) {
#if FATP_USE_EXCEPTION
        try {
            action();
        }
        catch (...) {
            throw;
        }
#else
        action();
#endif
    }
};

}
