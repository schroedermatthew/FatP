#pragma once

/*
FATP_META:
  meta_version: 1
  component: enforce_enforcers
  file_role: public_header
  path: include/fat_p/enforce_enforcers.h
  namespace: fat_p
  layer: Foundation
  summary: "Public header for enforce_enforcers."
  api_stability: in_work
  related:
    docs_search: "enforce_enforcers"
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 5
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/

/**
 * @file enforce_enforcers.h
 * @brief Defines the core RAII object used by all contract enforcement
 * macros, specializing on the chosen Raiser policy for failure handling.
 *
 *
 *
 * @details This file contains the primary Enforcer class and the factory
 * functions necessary to implement the fluent contract syntax:
 * `enforce(cond)("msg", value, ...).`
 */

#include "CppFeatureDetection.h"
#include "FatPConfig.h"

#include <exception>
#include <concepts>
#include <source_location>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include "ContractException.h"
#include "enforce_raisers.h"
#include "Stringify.h"

// ==================================================================================
// Portable Compiler Attributes
// ==================================================================================

// FATP_NOINLINE: Provided by FatPConfig.h (Rule F: single source of truth)

namespace fat_p
{

/**
 * @brief Utility struct for building the final diagnostic message string.
 *
 * @details This builder uses `std::ostringstream` and a fold
 * expression with `toString` to safely concatenate any number
 * of streamable arguments into a single message.
 */
struct MessageBuilder
{
    std::ostringstream ss;

    /**
     * @brief Safely converts and appends a single argument to the stream.
     * @tparam T The type of the message argument.
     * @param msg The message argument to append.
     */
    template <typename T>
    void append_message(T&& msg)
    {
        ss << toString(std::forward<T>(msg));
    }

    /**
     * @brief Concatenates a variadic list of arguments into the message.
     * @tparam Msgs The types of the message arguments.
     * @param msgs The arguments to concatenate.
     */
    template <typename... Msgs>
    void format(Msgs&&... msgs)
    {
        (append_message(std::forward<Msgs>(msgs)), ...);
    }

    /**
     * @brief Formats and returns the complete, structured error message.
     * @param loc The source location where the failure occurred.
     * @param expression The source code text of the failed condition.
     * @return The complete error message string.
     */
    std::string get_message(std::source_location loc, const std::string& expression) const
    {
        return "\n\tCondition: " + expression + "\n\tLocation: " + loc.file_name() + ":" +
               std::to_string(loc.line()) + "\n\tFunction: " + loc.function_name() + "\n\tMessage: " + ss.str();
    }
};

/// @brief A Raiser whose fail() is noexcept Ã¢â‚¬â€ used to derive the Enforcer
/// destructor's noexcept specification automatically. Any raiser that marks
/// its fail() noexcept will produce a noexcept Enforcer destructor.
template <typename R>
concept nothrow_raiser = requires(const std::string& msg) {
    { R::fail(msg) } noexcept;
};

// --- 1. The Core Enforcer (Active Check) ---
/**
 * @brief The main RAII object created by all active contract macros.
 *
 * @details The destructor is guaranteed to run, checking the condition
 * result and invoking the Raiser policy on failure.
 *
 * CRITICAL: This class is designed for ZERO overhead when the condition passes.
 * All members are trivially destructible (pointers/bool/source_location only).
 * The expensive operations (string building, ostringstream, exception throwing)
 * only happen in the failure path, which is marked cold.
 *
 * @tparam Raiser The failure consequence policy (e.g., throw, log, abort).
 */
template <typename Raiser>
class Enforcer
{
    const bool mPassed;
    const std::source_location mLoc;
    const char* const mExpression;
    const char* mUserMessage = nullptr; // Points to static string or nullptr - NO allocation

public:
    /**
     * @brief Constructor for the Enforcer.
     * @param passed The result of the condition check (true if passed).
     * @param expression_str The source code text of the condition.
     * @param loc The source location of the contract call.
     */
    constexpr Enforcer(bool passed, const char* expression_str, std::source_location loc) noexcept
        : mPassed(passed)
        , mLoc(loc)
        , mExpression(expression_str)
    {
    }

    /**
     * @brief The core RAII check logic runs here upon scope exit.
     *
     * @details Calls `Raiser::fail` if the condition was false. The failure
     * path is marked cold to help the optimizer keep the hot path tight.
     */
    constexpr ~Enforcer() noexcept(nothrow_raiser<Raiser>)
    {
        if (!mPassed) [[unlikely]]
        {
            fail_impl();
        }
    }

private:
    /**
     * @brief Cold path - only called on failure
     *
     * Marked noinline to prevent the exception handling machinery from
     * polluting the hot path where the condition passes.
     */
    FATP_NOINLINE
    void fail_impl()
    {
        std::string full_message = "\n\tCondition: ";
        full_message += mExpression;
        full_message += "\n\tLocation: ";
        full_message += mLoc.file_name();
        full_message += ":";
        full_message += std::to_string(mLoc.line());
        full_message += "\n\tFunction: ";
        full_message += mLoc.function_name();
        if (mUserMessage)
        {
            full_message += "\n\tMessage: ";
            full_message += mUserMessage;
        }

        Raiser::fail(full_message);
    }

public:
    /**
     * @brief Set diagnostic message (simple const char* version).
     */
    void operator()(const char* msg) noexcept
    {
        if (!mPassed) [[unlikely]]
        {
            mUserMessage = msg;
        }
    }

    /**
     * @brief Set diagnostic message (variadic version for complex messages).
     *
     * Uses ostringstream to format multiple arguments. Only called on failure.
     */
    template <typename... Msgs>
    void operator()(Msgs&&... msgs)
    {
        if (!mPassed) [[unlikely]]
        {
            if constexpr (sizeof...(Msgs) > 0)
            {
                // Build message on heap - only happens on failure
                static thread_local std::string formatted_message;
                formatted_message.clear();
                std::ostringstream ss;
                (ss << ... << toString(std::forward<Msgs>(msgs)));
                formatted_message = ss.str();
                mUserMessage = formatted_message.c_str();
            }
        }
    }

    /**
     * @brief The asterisk operator forces full evaluation of the Enforcer
     * object before it leaves scope.
     */
    Enforcer& operator*()
    {
        return *this;
    }
};

// --- 2. The NoOp Enforcer (Passive Check) ---
/**
 * @brief A specialized Enforcer used when checks are disabled
 * (e.g., enforce() in Release).
 *
 * @details All methods are empty, resulting in zero overhead due to
 * optimization. It is used when the policy is `NoOpRaiser`.
 */
class NoOpEnforcer
{
public:
    /**
     * @brief Zero-overhead constructor.
     */
    constexpr NoOpEnforcer(bool /* passed */, const char* /* expression_str */, std::source_location /* loc */) noexcept
    {
    }

    /**
     * @brief Zero-overhead destructor.
     */
    constexpr ~NoOpEnforcer() noexcept
    {
    }

    /**
     * @brief Zero-overhead message operator.
     */
    template <typename... Msgs>
    constexpr void operator()(Msgs&&...) noexcept
    {
    }

    /**
     * @brief Zero-overhead dereference operator.
     */
    constexpr NoOpEnforcer& operator*() noexcept
    {
        return *this;
    }
};

// --- 3. Factory Function for Policy Selection ---
/**
 * @brief Factory function that returns the appropriate Enforcer type
 * based on the Raiser policy.
 *
 * @details Uses `if constexpr` to select between the standard
 * `Enforcer` and the zero-overhead `NoOpEnforcer` at compile time.
 * @tparam Raiser The failure consequence policy.
 * @param passed The result of the condition check.
 * @param expression_str The source code text of the condition.
 * @param loc The source location of the contract call.
 * @return Either an `Enforcer<Raiser>` or `NoOpEnforcer` instance.
 */
template <typename Raiser>
[[nodiscard]] constexpr auto MakeEnforcer(bool passed, const char* expression_str, std::source_location loc)
{
    if constexpr (std::is_same_v<Raiser, NoOpRaiser>)
    {
        return NoOpEnforcer(passed, expression_str, loc);
    }
    else
    {
        return Enforcer<Raiser>(passed, expression_str, loc);
    }
}

} // namespace fat_p
