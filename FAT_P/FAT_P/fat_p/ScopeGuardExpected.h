/**
 * @file ScopeGuardExpected.h
 * @brief Bridge utilities for combining ScopeGuard with Expected error handling.
 *
 *
 *
 * @layer Foundation
 *
 * @details This optional header provides convenience functions for common patterns
 * where RAII cleanup interacts with Expected-based error propagation. Include this
 * header only when you need both ScopeGuard and Expected in the same translation unit.
 *
 * This is a bridge header that includes both ScopeGuard.h and Expected.h. It does
 * not modify either component and creates no circular dependencies.
 *
 * Features:
 * - make_rollback_guard: Execute cleanup only on error
 * - make_success_guard: Execute cleanup only on success
 * - make_capturing_guard: Capture cleanup exceptions into Expected
 * - with_resource: RAII resource wrapper returning Expected
 *
 * IMPORTANT - LIFETIME SAFETY:
 * The functions make_rollback_guard and make_success_guard capture a reference to
 * the Expected parameter. Passing a temporary (rvalue) Expected would result in
 * undefined behavior when the guard executes. To prevent this, rvalue overloads
 * are explicitly deleted.
 *
 * Requirements:
 * - C++17 or later
 * - Header-only
 * - Single-threaded use only (inherits ScopeGuard's threading model)
 */
#pragma once

/*
FATP_META:
  meta_version: 1
  component: ScopeGuardExpected
  file_role: public_header
  path: fat_p/ScopeGuardExpected.h
  namespace: fat_p
  layer: Foundation
  summary: "Public header for ScopeGuardExpected."
  api_stability: in_work
  related:
    docs_search: "ScopeGuardExpected"
    tests:
      - tests/test_ScopeGuardExpected.cpp
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/
#include "Expected.h"
#include "ScopeGuard.h"

namespace fat_p
{

/**
 * @brief Creates a scope guard that executes cleanup only if result holds an error.
 *
 * @details Useful for rollback patterns where cleanup should only occur on failure.
 * The guard captures a reference to the Expected and checks its state at scope exit.
 *
 * @warning The Expected must outlive the returned guard. Passing a temporary is
 * prevented by a deleted overload.
 *
 * @tparam T Value type of the Expected
 * @tparam E Error type of the Expected
 * @tparam F Cleanup function type
 * @param result Reference to Expected that determines whether cleanup runs
 * @param cleanup Function to call if result contains an error at scope exit
 * @return ScopeGuard that conditionally executes cleanup
 *
 * Example:
 * @code
 * fat_p::Expected<FileHandle, Error> open_and_process(const char* path)
 * {
 *     auto file = open_file(path);
 *     if (!file) return fat_p::make_unexpected(file.error());
 *
 *     fat_p::Expected<void, Error> result;
 *     auto guard = fat_p::make_rollback_guard(result, [&] {
 *         delete_partial_output();
 *     });
 *
 *     result = write_header(*file);
 *     if (!result) return fat_p::make_unexpected(result.error());
 *
 *     result = write_body(*file);
 *     if (!result) return fat_p::make_unexpected(result.error());
 *
 *     return file;  // Success - guard checks result, sees no error, skips cleanup
 * }
 * @endcode
 */
template <typename T, typename E, typename F>
[[nodiscard]] auto make_rollback_guard(const Expected<T, E>& result, F&& cleanup)
{
    return makeScopeGuard([&result, cleanup = std::forward<F>(cleanup)]() {
        if (!result.has_value())
        {
            cleanup();
        }
    });
}

/**
 * @brief Deleted overload to prevent dangling reference from temporary Expected.
 *
 * @details Passing a temporary Expected would create a guard that holds a dangling
 * reference. This overload catches that error at compile time.
 */
template <typename T, typename E, typename F>
auto make_rollback_guard(const Expected<T, E>&& result, F&& cleanup) = delete;

/**
 * @brief Creates a scope guard that executes cleanup only if result holds a value.
 *
 * @details Useful for commit/finalize patterns where action should only occur on success.
 *
 * @warning The Expected must outlive the returned guard. Passing a temporary is
 * prevented by a deleted overload.
 *
 * @tparam T Value type of the Expected
 * @tparam E Error type of the Expected
 * @tparam F Cleanup function type
 * @param result Reference to Expected that determines whether cleanup runs
 * @param on_success Function to call if result contains a value at scope exit
 * @return ScopeGuard that conditionally executes on_success
 *
 * Example:
 * @code
 * fat_p::Expected<void, Error> transaction()
 * {
 *     begin_transaction();
 *
 *     fat_p::Expected<void, Error> result;
 *     auto commit_guard = fat_p::make_success_guard(result, [&] {
 *         commit_transaction();
 *     });
 *     auto rollback_guard = fat_p::make_rollback_guard(result, [&] {
 *         rollback_transaction();
 *     });
 *
 *     result = step1();
 *     if (!result) return result;
 *
 *     result = step2();
 *     return result;  // Commits on success, rolls back on failure
 * }
 * @endcode
 */
template <typename T, typename E, typename F>
[[nodiscard]] auto make_success_guard(const Expected<T, E>& result, F&& on_success)
{
    return makeScopeGuard([&result, on_success = std::forward<F>(on_success)]() {
        if (result.has_value())
        {
            on_success();
        }
    });
}

/**
 * @brief Deleted overload to prevent dangling reference from temporary Expected.
 */
template <typename T, typename E, typename F>
auto make_success_guard(const Expected<T, E>&& result, F&& on_success) = delete;

/**
 * @brief Creates a scope guard that captures cleanup exceptions into an Expected.
 *
 * @details Wraps cleanup in try/catch and stores any exception in the provided Expected.
 * The guard uses NothrowPolicy and swallows exceptions internally after capturing them.
 * This prevents unwanted error logging (since the error is explicitly handled by the caller).
 *
 * @tparam E Error type (must be constructible from const char*)
 * @tparam F Cleanup function type
 * @param error_sink Reference to Expected<void, E> where cleanup errors are stored
 * @param cleanup Function that may throw during cleanup
 * @return ScopeGuard with NothrowPolicy that captures errors
 *
 * Example:
 * @code
 * fat_p::Expected<Data, std::string> process_file(const char* path)
 * {
 *     FILE* f = fopen(path, "r");
 *     if (!f) return fat_p::make_unexpected(std::string("Failed to open file"));
 *
 *     fat_p::Expected<void, std::string> cleanup_result;
 *     auto guard = fat_p::make_capturing_guard(cleanup_result, [f] {
 *         if (fclose(f) != 0)
 *         {
 *             throw std::runtime_error("Failed to close file");
 *         }
 *     });
 *
 *     auto data = read_all(f);
 *
 *     // After scope exit, check cleanup_result if you care about close errors
 *     return data;
 * }
 * @endcode
 */
template <typename E, typename F>
[[nodiscard]] auto make_capturing_guard(Expected<void, E>& error_sink, F&& cleanup)
{
    static_assert(std::is_constructible_v<E, const char*>, "Error type E must be constructible from const char*");

    // Use NothrowPolicy because we handle exceptions inside the lambda.
    // This prevents ScopeGuardLogAndSwallowPolicy from logging handled errors.
    return makeScopeGuard<ScopeGuardNothrowPolicy>([&error_sink, cleanup = std::forward<F>(cleanup)]() noexcept {
        try
        {
            cleanup();
        }
        catch (const std::exception& e)
        {
            error_sink = make_unexpected(E(e.what()));
        }
        catch (...)
        {
            error_sink = make_unexpected(E("Unknown exception during cleanup"));
        }
    });
}

/**
 * @brief Executes an action with a resource and wraps the result in Expected.
 *
 * @details Combines resource acquisition, action execution, and cleanup into a single
 * expression that returns Expected. Cleanup always runs regardless of success/failure.
 *
 * EXCEPTION SAFETY: If action throws and cleanup also throws, the cleanup exception
 * is swallowed to allow the original action exception to propagate. This prevents
 * std::terminate from being called due to double-exception during stack unwinding.
 *
 * @tparam T Return type of action
 * @tparam E Error type for Expected (default: std::string)
 * @tparam Setup Function that acquires resource
 * @tparam Action Function that uses resource and returns T
 * @tparam Cleanup Function that releases resource
 * @param setup Resource acquisition function
 * @param action Function to execute with resource
 * @param cleanup Resource release function (always called)
 * @return Expected<T, E> containing result or error
 *
 * Example:
 * @code
 * auto result = fat_p::with_resource<Data, std::string>(
 *     [] { return acquire_database_connection(); },
 *     [](auto& conn) { return query_data(conn); },
 *     [](auto& conn) { release_connection(conn); }
 * );
 * @endcode
 */
template <typename T, typename E = std::string, typename Setup, typename Action, typename Cleanup>
[[nodiscard]] Expected<T, E> with_resource(Setup&& setup, Action&& action, Cleanup&& cleanup)
{
    static_assert(std::is_constructible_v<E, const char*>, "Error type E must be constructible from const char*");

    // Prevent nested Expected - if action returns Expected<U,F>, the user should use
    // with_expected_resource or handle the error explicitly
    using SetupResult = std::invoke_result_t<Setup>;
    using ActionResult = std::invoke_result_t<Action, SetupResult&>;
    static_assert(!is_expected_v<ActionResult> || std::is_void_v<T>,
                  "Action returns Expected - use with_expected_resource or unwrap manually to avoid "
                  "nested Expected<Expected<T,E>,E>");

    try
    {
        auto resource = setup();

        // Use NothrowPolicy to prevent std::terminate if both action and cleanup throw.
        // If action throws, we are unwinding. If cleanup also throws with TerminatePolicy,
        // the program would crash. Instead, we swallow cleanup exceptions to let the
        // original action exception propagate to the catch block below.
        auto guard = makeScopeGuard<ScopeGuardNothrowPolicy>([&resource, &cleanup]() noexcept {
            try
            {
                cleanup(resource);
            }
            catch (...)
            {
                // Swallow cleanup exception to preserve original exception
            }
        });

        if constexpr (std::is_void_v<T>)
        {
            action(resource);
            return {};
        }
        else
        {
            return action(resource);
        }
    }
    catch (const std::exception& e)
    {
        return make_unexpected(E(e.what()));
    }
    catch (...)
    {
        return make_unexpected(E("Unknown exception"));
    }
}

/**
 * @brief Executes an action with a resource, using Expected for error handling throughout.
 *
 * @details Similar to with_resource but setup returns Expected, allowing for
 * acquisition failures to be handled uniformly.
 *
 * EXCEPTION SAFETY: If action throws and cleanup also throws, the cleanup exception
 * is swallowed to allow the original action exception to propagate.
 *
 * @tparam T Return type of action
 * @tparam E Error type for Expected
 * @tparam R Resource type
 * @tparam Action Function that uses resource and returns Expected<T, E>
 * @tparam Cleanup Function that releases resource
 * @param resource_result Expected containing acquired resource or error
 * @param action Function to execute with resource
 * @param cleanup Resource release function (only called if resource was acquired)
 * @return Expected<T, E> containing result or error
 *
 * Example:
 * @code
 * auto result = fat_p::with_expected_resource<Data, Error>(
 *     acquire_connection(),  // Returns Expected<Connection, Error>
 *     [](auto& conn) { return query_data(conn); },
 *     [](auto& conn) { conn.close(); }
 * );
 * @endcode
 */
template <typename T, typename E, typename R, typename Action, typename Cleanup>
[[nodiscard]] Expected<T, E>
with_expected_resource(Expected<R, E>&& resource_result, Action&& action, Cleanup&& cleanup)
{
    if (!resource_result.has_value())
    {
        return make_unexpected(std::move(resource_result).error());
    }

    auto& resource = *resource_result;

    // Use NothrowPolicy to prevent std::terminate on double-exception
    auto guard = makeScopeGuard<ScopeGuardNothrowPolicy>([&resource, &cleanup]() noexcept {
        try
        {
            cleanup(resource);
        }
        catch (...)
        {
            // Swallow cleanup exception
        }
    });

    return action(resource);
}

} // namespace fat_p
