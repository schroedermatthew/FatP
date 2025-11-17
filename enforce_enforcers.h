/**
 * @file enforce_enforcers.h
 * @brief Defines the core RAII object used by all contract enforcement
 * macros, specializing on the chosen Raiser policy for failure handling.
 *
 * @details This file contains the primary Enforcer class and the factory
 * functions necessary to implement the fluent contract syntax:
 * `enforce(cond)("msg", value, ...).`
 */
#pragma once
#include <exception>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <sstream>

#include "enforce_raisers.h" // Contains Raiser policy structs
#include "Stringify.h" // For type-safe string conversion
#include "ContractException.h" // Defines the ContractViolationError base

namespace fat_p {
    /**
     * @brief Utility struct for building the final diagnostic message string.
     *
     * @details This builder uses `std::ostringstream` and a C++17 fold
     * expression with `toStringIfStreamable` to safely concatenate any number
     * of streamable arguments into a single message.
     */
    struct MessageBuilder {
        std::ostringstream ss;
        /**
         * @brief Safely converts and appends a single argument to the stream.
         * @tparam T The type of the message argument.
         * @param msg The message argument to append.
         */
        template <typename T>
        void append_message(T&& msg) {
            // Calls the shared utility function to safely convert the
            // argument to a string before streaming it.
            ss << toString(std::forward<T>(msg));
        }
        /**
         * @brief Concatenates a variadic list of arguments into the message.
         * @tparam Msgs The types of the message arguments.
         * @param msgs The arguments to concatenate.
         */
        template <typename... Msgs>
        void format(Msgs&&... msgs) {
            // Concatenates all messages using a C++17 fold expression.
            (append_message(std::forward<Msgs>(msgs)), ...);
        }
        /**
         * @brief Formats and returns the complete, structured error message.
         * @param locus The file and line number where the failure occurred.
         * @param expression The source code text of the failed condition.
         * @return The complete error message string.
         */
        std::string get_message(const char* locus,
            const std::string& expression) const {
            return "\n\tCondition: " + expression +
                "\n\tLocus: " + locus +
                "\n\tMessage: " + ss.str();
        }
    };
    
    // Helper trait to detect ExpectedRaiser<E> for any E
    template <typename T>
    struct is_expected_raiser : std::false_type {};
    
    template <typename E>
    struct is_expected_raiser<ExpectedRaiser<E>> : std::true_type {};
    
    template <typename T>
    inline constexpr bool is_expected_raiser_v = is_expected_raiser<T>::value;
    
    // --- 1. The Core Enforcer (Active Check) ---
    /**
     * @brief The main RAII object created by all active contract macros.
     *
     * @details The destructor is guaranteed to run, checking the condition
     * result and invoking the Raiser policy on failure.
     * @tparam Raiser The failure consequence policy (e.g., throw, log, abort).
     */
    template <typename Raiser>
    class Enforcer {
        const bool passed_ = true;
        const char* const locus_ = nullptr;
        const std::string expression_;
        MessageBuilder message_builder_;
    public:
        /**
         * @brief Constructor for the Enforcer.
         * @param passed The result of the condition check (true if passed).
         * @param expression_str The source code text of the condition.
         * @param locus The file and line number of the contract call.
         */
        Enforcer(bool passed, const char* expression_str, const char* locus)
            noexcept
            : passed_(passed), locus_(locus), expression_(expression_str)
        {
        }
        /**
         * @brief The core RAII check logic runs here upon scope exit.
         *
         * @details Calls `Raiser::fail` if the condition was false. The
         * `noexcept` specification correctly matches the policy, guaranteeing
         * no exceptions escape if a non-throwing raiser is used.
         */
        ~Enforcer() noexcept(std::is_same_v<Raiser, NoThrowRaiser> ||
            std::is_same_v<Raiser, WarningToCerrRaiser> ||
            std::is_same_v<Raiser, NoOpRaiser>) {
            if (!passed_) {
                // Note: If Raiser is NoOpRaiser, this call is optimized away.
                if constexpr (std::is_same_v<Raiser, NoThrowRaiser> ||
                              std::is_same_v<Raiser, WarningToCerrRaiser> ||
                              std::is_same_v<Raiser, NoOpRaiser>) {
                    // Non-throwing raisers don't need try-catch
                    Raiser::fail(message_builder_.get_message(
                        locus_, expression_));
                } else {
                    // Throwing raisers need try-catch for proper exception handling
                    try {
                        Raiser::fail(message_builder_.get_message(
                            locus_, expression_));
                    }
                    catch (...) {
                        // Allows stack unwinding to proceed for non-std::exception.
                        throw;
                    }
                }
            }
        }
        /**
         * @brief Overloaded function call operator to set the final diagnostic
         * message.
         *
         * @details This enables the fluent syntax:
         * `enforce(cond)("msg", value, ...).`
         * @tparam Msgs The types of the message arguments.
         * @param msgs The arguments to format into the message.
         */
        template <typename... Msgs>
        void operator()(Msgs&&... msgs) {
            if (!passed_) {
                // All arguments are passed to the MessageBuilder for safe
                // conversion and formatting.
                message_builder_.format(std::forward<Msgs>(msgs)...);
            }
        }
        /**
         * @brief The asterisk operator forces full evaluation of the Enforcer
         * object before it leaves scope.
         *
         * @return Reference to self.
         */
        Enforcer& operator*() { return *this; }
    };
    // --- 2. The NoOp Enforcer (Passive Check) ---
    /**
     * @brief A specialized Enforcer used when checks are disabled
     * (e.g., enforce() in Release).
     *
     * @details All methods are empty, resulting in zero overhead due to
     * optimization. It is used when the policy is `NoOpRaiser`.
     */
    class NoOpEnforcer {
    public:
        /**
         * @brief Zero-overhead constructor.
         */
        NoOpEnforcer(bool /* passed */, const char* /* expression_str */,
            const char* /* locus */) noexcept {
        }
        /**
         * @brief Zero-overhead destructor.
         */
        ~NoOpEnforcer() noexcept {}
        /**
         * @brief Zero-overhead message operator.
         */
        template <typename... Msgs>
        void operator()(Msgs&&...) {}
        /**
         * @brief Zero-overhead dereference operator.
         */
        NoOpEnforcer& operator*() { return *this; }
    };
    // --- 3. Factory Function for Policy Selection ---
    /**
     * @brief Factory function that returns the appropriate Enforcer type
     * based on the Raiser policy.
     *
     * @details Uses C++17 `if constexpr` to select between the standard
     * `Enforcer` and the zero-overhead `NoOpEnforcer` at compile time.
     * @tparam Raiser The failure consequence policy.
     * @param passed The result of the condition check.
     * @param expression_str The source code text of the condition.
     * @param locus The file and line number of the contract call.
     * @return Either an `Enforcer<Raiser>` or `NoOpEnforcer` instance.
     */
    template <typename Raiser>
    [[nodiscard]] auto MakeEnforcer(bool passed,
        const char* expression_str,
        const char* locus) {
        // C++17: Compile-time check to select the zero-cost path
        if constexpr (std::is_same_v<Raiser, NoOpRaiser>) {
            return NoOpEnforcer(passed, expression_str, locus);
        }
        else {
            // Otherwise, use the standard Enforcer with the specified Raiser.
            return Enforcer<Raiser>(passed, expression_str, locus);
        }
    }
} // namespace fat_p