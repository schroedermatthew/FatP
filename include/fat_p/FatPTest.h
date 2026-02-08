#pragma once

/*
FATP_META:
  meta_version: 1
  component: FatPTest
  file_role: public_header
  path: include/fat_p/FatPTest.h
  namespace: fat_p
  layer: Testing
  summary: "Public header for FatPTest."
  api_stability: in_work
  related:
    docs_search: "FatPTest"
    tests:
      - components/AlignedVector/tests/test_AlignedVector.cpp
      - components/AllocationStrategies/tests/test_AllocationStrategies.cpp
      - components/AsyncOperations/tests/test_AsyncOperations.cpp
      - components/AtomicSharedPtr/tests/test_AtomicSharedPtr.cpp
      - components/BinarySerialization/tests/test_BinaryLite.cpp
      - components/BitSet/tests/test_BitSet.cpp
      - components/CacheUtilities/tests/test_CacheUtilities.cpp
      - components/Cbor/tests/test_CborLite.cpp
      - components/Cbor/tests/test_CborStreamLite.cpp
      - components/CheckedArithmetic/tests/test_CheckedArithmetic.cpp
      - components/CircularBuffer/tests/test_CircularBuffer.cpp
      - components/Concepts/tests/test_Concepts.cpp
      - components/ConcurrencyPolicies/tests/test_ConcurrencyPolicies.cpp
      - components/ConstexprUtilities/tests/test_ConstexprUtilities.cpp
      - components/ContractException/tests/test_ContractException.cpp
      - components/CoroutineTask/tests/test_CoroutineTask.cpp
      - components/CSRMatrix/tests/test_CSRMatrix.cpp
      - components/CSRMatrix/tests/test_CSRMatrixParallel.cpp
      - components/CSRMatrix/tests/test_CSRMatrix_HPC.cpp
      - components/CSRMatrix/tests/test_CSRMatrix_HPC_Parallel.cpp
      - components/DebugOnly/tests/test_DebugOnly.cpp
      - components/DiagnosticLogger/tests/test_DiagnosticLogger_Core.cpp
      - components/DiagnosticLogger/tests/test_DiagnosticLogger_IO.cpp
      - components/DiagnosticLogger/tests/test_DiagnosticLogger_Json.cpp
      - components/DiagnosticLogger/tests/test_DiagnosticLogger_ScopeGuard.cpp
      - components/Enforce/tests/test_Enforce.cpp
      - components/EnforcedInit/tests/test_EnforcedInit.cpp
      - components/EnhancedBoundsChecking/tests/test_EnhancedBoundsChecking.cpp
      - components/EnumPlus/tests/test_EnumPlus.cpp
      - components/Equality/tests/test_EqualityAny.cpp
      - components/Equality/tests/test_EqualityComparisons.cpp
      - components/Expected/tests/test_Expected.cpp
      - components/Factory/tests/test_Factory.cpp
      - components/FatPHashMap/tests/test_FastHashMap.cpp
      - components/FatPTest/tests/test_FatP.cpp
      - components/FatPBenchmarkRunner/tests/test_FatPBenchmarkRunner.cpp
      - components/BinarySerialization/tests/test_FatPBinary.cpp
      - components/Cbor/tests/test_FatPCbor.cpp
      - components/Cbor/tests/test_FatPCborStream.cpp
      - components/Concepts/tests/test_FatPConcepts.cpp
      - components/Json/tests/test_FatPJson.cpp
      - components/Json/tests/test_FatPJsonStream.cpp
      - components/FatPTest/tests/test_FatPTest.cpp
      - components/FeatureManager/tests/test_FeatureManager.cpp
      - components/FlatMapSet/tests/test_FlatMap.cpp
      - components/FlatMapSet/tests/test_FlatSet.cpp
      - components/FloatingPointComparison/tests/test_FloatingPointComparison.cpp
      - components/HpcVector/tests/test_HpcVector.cpp
      - components/IdGenerator/tests/test_IdGenerator.cpp
      - components/IntrusiveList/tests/test_IntrusiveList.cpp
      - components/Json/tests/test_JsonLite.cpp
      - components/Json/tests/test_JsonStreamLite.cpp
      - components/LockFreeContainers/tests/test_LockFreeQueue.cpp
      - components/LockFreeContainers/tests/test_LockFreeRingBuffer.cpp
      - components/MemoryMappedFile/tests/test_MemoryMappedFile.cpp
      - components/NumaAllocator/tests/test_NumaAllocator.cpp
      - components/ObjectPool/tests/test_ObjectPool.cpp
      - components/PipeOperator/tests/test_PipeOperator.cpp
      - components/PolicyIterator/tests/test_PolicyIterator.cpp
      - components/RateLimiter/tests/test_RateLimiter.cpp
      - components/ConcurrencyPolicies/tests/test_RcuIntegration.cpp
      - components/Reflection/tests/test_Reflection.cpp
      - components/ScopeGuard/tests/test_ScopeGuard.cpp
      - components/ScopeGuard/tests/test_ScopeGuardExpected.cpp
      - components/ServiceLocator/tests/test_ServiceLocator.cpp
      - components/Signal/tests/test_Signal.cpp
      - components/SimdVector/tests/test_SimdVector.cpp
      - components/SlidingFileWindow/tests/test_SlidingFileWindow.cpp
      - components/SlotMap/tests/test_SlotMap.cpp
      - components/SmallVector/tests/test_SmallVector.cpp
      - components/SortedContainer/tests/test_SortedContainer.cpp
      - components/SparseSet/tests/test_SparseSet.cpp
      - components/FatPHashMap/tests/test_StableHashMap.cpp
      - components/Stacktrace/tests/test_Stacktrace.cpp
      - components/StateMachine/tests/test_StateMachine.cpp
      - components/Stringify/tests/test_Stringify.cpp
      - components/StringPool/tests/test_StringPool.cpp
      - components/StrongId/tests/test_StrongId.cpp
      - components/Tensor/tests/test_Tensor.cpp
      - components/Tensor/tests/test_TensorComparison.cpp
      - components/Tensor/tests/test_TensorEinsum.cpp
      - components/Tensor/tests/test_TensorMath.cpp
      - components/Tensor/tests/test_TensorSerializer.cpp
      - components/Tensor/tests/test_TensorStorage.cpp
      - components/ThreadPool/tests/test_ThreadPool.cpp
      - components/ValueGuard/tests/test_ValueGuard.cpp
      - components/ViewLifetimeTracking/tests/test_ViewLifetimeTracking.cpp
    benchmarks:
      - benchmarks/benchmark_EqualityComparisonsAny.cpp
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 36
    defines_unprefixed: 35
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/

/**
 * @file FatPTest.h
 * @brief Zero-dependency test infrastructure for header-only C++ libraries
 *
 * DESIGN PHILOSOPHY:
 * This test infrastructure is intentionally lightweight and self-contained,
 * requiring no external testing frameworks (GoogleTest, Boost.Test, Catch2, etc.).
 *
 * FEATURES:
 * - 18+ assertion macros (equality, comparison, floating-point, exceptions)
 * - Test fixtures with automatic setup/teardown
 * - Parameterized/data-driven tests
 * - String assertion utilities (contains, starts_with, ends_with, regex)
 * - Container/range comparison with detailed diff output
 * - Test filtering by name pattern
 * - Enhanced benchmarking (percentiles, outliers, baseline comparison)
 * - Colored output with ANSI codes
 * - Test runner infrastructure
 * - Zero external dependencies
 *
 * RATIONALE FOR ZERO DEPENDENCIES:
 * - The components under test are header-only and require no external dependencies
 * - Test infrastructure should not impose dependency requirements on users
 * - GoogleTest and Boost.Test are powerful but heavyweight (linking, installation)
 * - For header-only libraries, a simple assertion framework is sufficient
 * - Enables testing in minimal environments (embedded, CI without package managers)
 * - Reduces build complexity and compilation time
 *
 * CRITICAL DESIGN DECISION - CIRCULAR DEPENDENCY AVOIDANCE:
 * This file must remain independent of all components being tested. It provides
 * primitive floating-point comparison via primitive::are_close() which is
 * intentionally simple and obviously correct by inspection.
 *
 * - For production code: Use FloatingPointComparison.h
 * - For test code: Use the FATP_ASSERT_CLOSE* macros provided here
 *
 * The primitive comparison is NOT production-quality. It exists solely to enable
 * independent testing of FloatingPointComparison.h and other components without
 * creating circular dependencies.
 *
 * THREAD SAFETY:
 * This test infrastructure is NOT thread-safe. All tests must be executed in a
 * single-threaded context. Do not run multiple test runners concurrently or
 * execute tests from multiple threads simultaneously.
 *
cd build * @note This infrastructure provides: assertions, benchmarking, colored output,
 *       floating-point comparison, exception testing, and test runners - all
 *       with zero external dependencies beyond the C++ standard library.
 */

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <ctime>
#include <exception>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include <filesystem>
#include <cstdlib>

#if defined(_WIN32) || defined(_WIN64)
// Track whether we defined these macros so we can clean up after ourselves
#ifndef NOMINMAX
#define NOMINMAX
#define FATP_DEFINED_NOMINMAX_FATPTEST
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#define FATP_DEFINED_WIN32_LEAN_AND_MEAN_FATPTEST
#endif
#ifndef FATP_ENABLE_PDH_STATS
#define FATP_ENABLE_PDH_STATS
#endif
#include <Windows.h>
// Clean up macros we defined to avoid polluting user's compile environment
#ifdef FATP_DEFINED_NOMINMAX_FATPTEST
#undef NOMINMAX
#undef FATP_DEFINED_NOMINMAX_FATPTEST
#endif
#ifdef FATP_DEFINED_WIN32_LEAN_AND_MEAN_FATPTEST
#undef WIN32_LEAN_AND_MEAN
#undef FATP_DEFINED_WIN32_LEAN_AND_MEAN_FATPTEST
#endif
#else
#include <unistd.h> // For isatty()
#endif

namespace fat_p
{
namespace testing
{

// =============================================================================
// Test Artifacts
// =============================================================================
// A small helper for tests that need temporary files.
//
// Resolution order:
//  1) Compile-time override: FATP_TEST_ARTIFACTS_DIR (string literal)
//  2) Environment variable:  FATP_TEST_ARTIFACTS_DIR
//  3) Default:              <current working dir>/artifacts
//
// The directory is created on first use (best-effort).
inline std::filesystem::path artifact_dir()
{
#if defined(FATP_TEST_ARTIFACTS_DIR)
    return std::filesystem::path(FATP_TEST_ARTIFACTS_DIR);
#else
    static const std::filesystem::path sPath = []() -> std::filesystem::path
    {
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996) // getenv is safe here - single-threaded init
#endif
        if (const char* env = std::getenv("FATP_TEST_ARTIFACTS_DIR"); env && env[0] != '\0')
        {
            return std::filesystem::path(env);
        }
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

        std::error_code ec;
        std::filesystem::path cwd = std::filesystem::current_path(ec);
        if (ec)
        {
            cwd = std::filesystem::path(".");
        }

        return cwd / "artifacts";
    }();

    std::error_code ec;
    std::filesystem::create_directories(sPath, ec);
    return sPath;
#endif
}

inline std::string artifact_file(const std::string& filename)
{
    std::filesystem::path p = artifact_dir();
    p /= filename;
    return p.string();
}


// ============================================================================
// Primitive Floating-Point Comparison (Test Infrastructure Only)
// ============================================================================

/**
 * @brief Primitive floating-point comparison for test infrastructure ONLY
 *
 * This namespace provides a simple, obviously-correct comparison function
 * independent of FloatingPointComparison.h to avoid circular dependencies.
 * The implementation is intentionally minimal and verifiable by inspection.
 *
 * ALGORITHM: Hybrid absolute + relative tolerance comparison
 * 1. NaN handling: NaN never equals anything (including itself) - IEEE 754 compliant
 * 2. Infinity handling: Same-sign infinities are equal, mixed signs are not
 * 3. Exact equality: Catches +/-0 equality and identical values (optimization)
 * 4. Absolute tolerance: Handles comparisons near zero
 * 5. Relative tolerance: Handles comparisons at large magnitudes
 *
 * DEFAULT EPSILON VALUES:
 * - Relative epsilon: 100 * machine epsilon (standard practice)
 * - Absolute epsilon: 1 * machine epsilon (100x tighter for near-zero values)
 *
 * This is NOT for production use - it's intentionally simplified for test
 * infrastructure. Production code should use FloatingPointComparison.h which
 * provides additional features like ULP comparison, configurable policies,
 * and diagnostic logging.
 *
 * @note The 100x scaling factor for relative epsilon is a widely-accepted
 *       default that balances precision with practical tolerance for rounding
 *       errors in typical floating-point calculations.
 */
namespace primitive
{
template <typename T>
constexpr T RELATIVE_EPSILON_SCALE = static_cast<T>(100);

template <std::floating_point T>
constexpr T get_default_epsilon()
{
    return std::numeric_limits<T>::epsilon() * RELATIVE_EPSILON_SCALE<T>;
}

template <std::floating_point T>
inline bool are_close(T a,
                      T b,
                      T rel_eps = get_default_epsilon<T>(),
                      T abs_eps = get_default_epsilon<T>() / RELATIVE_EPSILON_SCALE<T>)
{

    if (std::isnan(a) || std::isnan(b))
    {
        return false;
    }

    if (std::isinf(a) || std::isinf(b))
    {
        if (std::isinf(a) && std::isinf(b))
        {
            return (a > 0) == (b > 0);
        }
        return false;
    }

    if (a == b)
    {
        return true;
    }

    T diff = std::fabs(a - b);

    if (diff <= abs_eps)
    {
        return true;
    }

    T max_abs = std::max(std::fabs(a), std::fabs(b));
    return diff <= rel_eps * max_abs;
}
} // namespace primitive

// ============================================================================
// Configuration
// ============================================================================

/**
 * @brief Detect if terminal supports ANSI colors
 *
 * On Windows: Checks if virtual terminal processing is available
 * On Linux/Mac: Checks if stdout is a TTY
 */
inline bool detect_terminal_colors() noexcept
{
#if defined(_WIN32) || defined(_WIN64)
    // Try to enable virtual terminal processing on Windows 10+
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode))
    {
        return false;
    }

    // Check if already enabled or try to enable
    if (dwMode & ENABLE_VIRTUAL_TERMINAL_PROCESSING)
    {
        return true;
    }

    // Try to enable VT processing
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (SetConsoleMode(hOut, dwMode))
    {
        return true;
    }

    return false; // Old Windows without VT support
#else
    // POSIX: check if stdout is a terminal
    return isatty(fileno(stdout)) != 0;
#endif
}

/**
 * @brief Global configuration for testing utilities
 */
struct TestConfig
{
    bool colored_output = detect_terminal_colors();
    bool verbose = true;
    bool abort_on_failure = false;
    std::ostream* output = &std::cout;
    std::ostream* error = &std::cerr;
};

inline TestConfig& get_test_config() noexcept
{
    static TestConfig config;
    return config;
}

// ============================================================================
// Terminal Colors (ANSI escape codes)
// ============================================================================

namespace colors
{
inline const char* reset() noexcept
{
    return get_test_config().colored_output ? "\033[0m" : "";
}

inline const char* red() noexcept
{
    return get_test_config().colored_output ? "\033[91m" : "";
}

inline const char* green() noexcept
{
    return get_test_config().colored_output ? "\033[92m" : "";
}

inline const char* yellow() noexcept
{
    return get_test_config().colored_output ? "\033[93m" : "";
}

inline const char* blue() noexcept
{
    return get_test_config().colored_output ? "\033[94m" : "";
}

inline const char* magenta() noexcept
{
    return get_test_config().colored_output ? "\033[95m" : "";
}

inline const char* cyan() noexcept
{
    return get_test_config().colored_output ? "\033[96m" : "";
}

inline const char* bold() noexcept
{
    return get_test_config().colored_output ? "\033[1m" : "";
}
} // namespace colors

// ============================================================================
// String Utilities
// ============================================================================

namespace string_utils
{
inline bool contains(const std::string& str, const std::string& substr) noexcept
{
    return str.find(substr) != std::string::npos;
}

inline bool starts_with(const std::string& str, const std::string& prefix) noexcept
{
    if (prefix.size() > str.size())
    {
        return false;
    }
    return str.compare(0, prefix.size(), prefix) == 0;
}

inline bool ends_with(const std::string& str, const std::string& suffix) noexcept
{
    if (suffix.size() > str.size())
    {
        return false;
    }
    return str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

inline std::string to_lower(const std::string& str)
{
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return result;
}

/**
 * @brief Pattern matching implementation with wildcard support
 *
 * Supports '*' (any characters) and '?' (single character) wildcards.
 *
 * Uses an iterative algorithm with explicit backtracking to avoid stack overflow.
 * The algorithm handles wildcards by remembering positions for backtracking
 * when a match fails after consuming characters with '*'.
 */
inline bool matches_pattern(const std::string& str, const std::string& pattern) noexcept
{
    size_t str_pos = 0;
    size_t pat_pos = 0;
    size_t star_pat_pos = std::string::npos; // Position after last '*' in pattern
    size_t star_str_pos = 0;                 // Position in str when '*' was matched

    while (str_pos < str.size())
    {
        if (pat_pos < pattern.size() && (pattern[pat_pos] == '?' || pattern[pat_pos] == str[str_pos]))
        {
            // Character match or '?' wildcard
            ++str_pos;
            ++pat_pos;
        }
        else if (pat_pos < pattern.size() && pattern[pat_pos] == '*')
        {
            // '*' wildcard - remember this position for backtracking
            star_pat_pos = pat_pos;
            star_str_pos = str_pos;
            ++pat_pos; // Try matching '*' with empty string first
        }
        else if (star_pat_pos != std::string::npos)
        {
            // Mismatch, but we have a '*' to backtrack to
            // Let '*' consume one more character and retry
            pat_pos = star_pat_pos + 1;
            ++star_str_pos;
            str_pos = star_str_pos;
        }
        else
        {
            // No match and no '*' to backtrack to
            return false;
        }
    }

    // Consume any remaining '*' wildcards in pattern
    while (pat_pos < pattern.size() && pattern[pat_pos] == '*')
    {
        ++pat_pos;
    }

    return pat_pos == pattern.size();
}

/**
 * @brief Truncate a string for display in error messages
 *
 * For strings longer than max_length, returns first and last portions
 * with an ellipsis in the middle showing the total length.
 *
 * @param str String to potentially truncate
 * @param max_length Maximum display length (default: 200)
 * @return Original string if short enough, truncated version otherwise
 */
inline std::string truncate_for_display(const std::string& str, size_t max_length = 200)
{
    if (str.size() <= max_length)
    {
        return str;
    }

    size_t head_len = max_length / 2 - 10;
    size_t tail_len = max_length / 2 - 10;

    std::ostringstream oss;
    oss << str.substr(0, head_len) << "... [" << str.size() << " chars total] ..." << str.substr(str.size() - tail_len);
    return oss.str();
}
} // namespace string_utils

// ============================================================================
// Subtest Tracking (Forward Declaration)
// ============================================================================

class SubtestTracker;
inline SubtestTracker& get_subtest_tracker();

// ============================================================================
// Assertion Macros
// ============================================================================

/**
 * @brief Simple assert macro for tests (no dependency on testing framework)
 *
 * Usage:
 *   FATP_SIMPLE_ASSERT(condition, "error message");
 *
 * If the condition is false, prints error message and returns false from the
 * calling function. The calling function must return bool.
 */
#define FATP_SIMPLE_ASSERT(condition, msg)                                                                            \
    if (!(condition))                                                                                                 \
    {                                                                                                                 \
        *fat_p::testing::get_test_config().error << fat_p::testing::colors::red() << fat_p::testing::colors::bold()   \
                                                 << "ASSERT FAILED: " << fat_p::testing::colors::reset()              \
                                                 << fat_p::testing::colors::red() << msg << " at " << __FILE__ << ":" \
                                                 << __LINE__ << fat_p::testing::colors::reset() << std::endl;         \
        if (fat_p::testing::get_test_config().abort_on_failure)                                                       \
        {                                                                                                             \
            std::abort();                                                                                             \
        }                                                                                                             \
        if (fat_p::testing::get_subtest_tracker().is_inside_subtest())                                                \
        {                                                                                                             \
            fat_p::testing::get_subtest_tracker().fail_current_subtest(msg);                                          \
        }                                                                                                             \
        else                                                                                                          \
        {                                                                                                             \
            return false;                                                                                             \
        }                                                                                                             \
    }

/**
 * @brief Assert with custom failure handler
 *
 * Usage:
 *   FATP_ASSERT_WITH_HANDLER(x == 42, "x should be 42", {
 *       cleanup_resources();
 *   });
 */
#define FATP_ASSERT_WITH_HANDLER(condition, msg, handler)                                                             \
    if (!(condition))                                                                                                 \
    {                                                                                                                 \
        *fat_p::testing::get_test_config().error << fat_p::testing::colors::red() << fat_p::testing::colors::bold()   \
                                                 << "ASSERT FAILED: " << fat_p::testing::colors::reset()              \
                                                 << fat_p::testing::colors::red() << msg << " at " << __FILE__ << ":" \
                                                 << __LINE__ << fat_p::testing::colors::reset() << std::endl;         \
        handler;                                                                                                      \
        if (fat_p::testing::get_subtest_tracker().is_inside_subtest())                                                \
        {                                                                                                             \
            fat_p::testing::get_subtest_tracker().fail_current_subtest(msg);                                          \
        }                                                                                                             \
        else                                                                                                          \
        {                                                                                                             \
            return false;                                                                                             \
        }                                                                                                             \
    }

// =============================================================================
// Safe comparison helpers (C++20)
//
// Use std::cmp_equal/cmp_less/etc. for standard integer types to handle
// mixed signed/unsigned comparisons without warnings. Fall back to regular
// operators for non-integer types (floats, strings, char, bool, etc.).
//
// Note: std::cmp_* rejects char, bool, and character types even though they
// satisfy std::integral. The standard_integer concept matches exactly the
// types std::cmp_* accepts.
// =============================================================================

namespace detail
{

template <typename T>
concept standard_integer = std::integral<T>
    && !std::same_as<std::remove_cv_t<T>, bool>
    && !std::same_as<std::remove_cv_t<T>, char>
    && !std::same_as<std::remove_cv_t<T>, wchar_t>
    && !std::same_as<std::remove_cv_t<T>, char8_t>
    && !std::same_as<std::remove_cv_t<T>, char16_t>
    && !std::same_as<std::remove_cv_t<T>, char32_t>;

template <standard_integer A, standard_integer B>
bool safe_eq(const A& a, const B& b) { return std::cmp_equal(a, b); }
template <typename A, typename B>
bool safe_eq(const A& a, const B& b) { return a == b; }

template <standard_integer A, standard_integer B>
bool safe_ne(const A& a, const B& b) { return std::cmp_not_equal(a, b); }
template <typename A, typename B>
bool safe_ne(const A& a, const B& b) { return a != b; }

template <standard_integer A, standard_integer B>
bool safe_lt(const A& a, const B& b) { return std::cmp_less(a, b); }
template <typename A, typename B>
bool safe_lt(const A& a, const B& b) { return a < b; }

template <standard_integer A, standard_integer B>
bool safe_le(const A& a, const B& b) { return std::cmp_less_equal(a, b); }
template <typename A, typename B>
bool safe_le(const A& a, const B& b) { return a <= b; }

template <standard_integer A, standard_integer B>
bool safe_gt(const A& a, const B& b) { return std::cmp_greater(a, b); }
template <typename A, typename B>
bool safe_gt(const A& a, const B& b) { return a > b; }

template <standard_integer A, standard_integer B>
bool safe_ge(const A& a, const B& b) { return std::cmp_greater_equal(a, b); }
template <typename A, typename B>
bool safe_ge(const A& a, const B& b) { return a >= b; }

} // namespace detail

/**
 * @brief Assert with equality comparison, showing actual vs expected
 *
 * Uses auto&& (universal/forwarding reference) to handle all value categories
 * without copying. Binds lvalues as lvalue references and extends lifetime
 * of rvalues. Works with non-copyable types like std::atomic.
 */
#define FATP_ASSERT_EQ(actual, expected, msg)                                                                         \
    {                                                                                                                 \
        auto&& actual_val = (actual);                                                                                 \
        auto&& expected_val = (expected);                                                                             \
        if (!fat_p::testing::detail::safe_eq(actual_val, expected_val))                                                                            \
        {                                                                                                             \
            *fat_p::testing::get_test_config().error                                                                  \
                << fat_p::testing::colors::red() << fat_p::testing::colors::bold()                                    \
                << "FATP_ASSERT_EQ FAILED: " << fat_p::testing::colors::reset() << fat_p::testing::colors::red()      \
                << msg << "\n  Expected: " << expected_val << "\n  Actual:   " << actual_val << "\n  at " << __FILE__ \
                << ":" << __LINE__ << fat_p::testing::colors::reset() << std::endl;                                   \
            if (fat_p::testing::get_test_config().abort_on_failure)                                                   \
            {                                                                                                         \
                std::abort();                                                                                         \
            }                                                                                                         \
            if (fat_p::testing::get_subtest_tracker().is_inside_subtest())                                            \
            {                                                                                                         \
                fat_p::testing::get_subtest_tracker().fail_current_subtest(msg);                                      \
            }                                                                                                         \
            else                                                                                                      \
            {                                                                                                         \
                return false;                                                                                         \
            }                                                                                                         \
        }                                                                                                             \
    }

/**
 * @brief Assert with inequality comparison
 *
 * Uses auto&& (universal/forwarding reference) to handle all value categories
 * without copying. Binds lvalues as lvalue references and extends lifetime
 * of rvalues. Works with non-copyable types like std::atomic.
 */
#define FATP_ASSERT_NE(actual, expected, msg)                                                                    \
    {                                                                                                            \
        auto&& actual_val = (actual);                                                                            \
        auto&& expected_val = (expected);                                                                        \
        if (fat_p::testing::detail::safe_eq(actual_val, expected_val))                                                                          \
        {                                                                                                        \
            *fat_p::testing::get_test_config().error                                                             \
                << fat_p::testing::colors::red() << fat_p::testing::colors::bold()                               \
                << "FATP_ASSERT_NE FAILED: " << fat_p::testing::colors::reset() << fat_p::testing::colors::red() \
                << msg << "\n  Should not equal: " << expected_val << "\n  at " << __FILE__ << ":" << __LINE__   \
                << fat_p::testing::colors::reset() << std::endl;                                                 \
            if (fat_p::testing::get_test_config().abort_on_failure)                                              \
            {                                                                                                    \
                std::abort();                                                                                    \
            }                                                                                                    \
            if (fat_p::testing::get_subtest_tracker().is_inside_subtest())                                       \
            {                                                                                                    \
                fat_p::testing::get_subtest_tracker().fail_current_subtest(msg);                                 \
            }                                                                                                    \
            else                                                                                                 \
            {                                                                                                    \
                return false;                                                                                    \
            }                                                                                                    \
        }                                                                                                        \
    }

/**
 * @brief Assert less than
 *
 * Uses auto&& (universal/forwarding reference) to handle all value categories
 * without copying. Binds lvalues as lvalue references and extends lifetime
 * of rvalues.
 */
#define FATP_ASSERT_LT(actual, expected, msg)                                                                     \
    {                                                                                                             \
        auto&& actual_val = (actual);                                                                             \
        auto&& expected_val = (expected);                                                                         \
        if (!fat_p::testing::detail::safe_lt(actual_val, expected_val))                                                                         \
        {                                                                                                         \
            *fat_p::testing::get_test_config().error                                                              \
                << fat_p::testing::colors::red() << fat_p::testing::colors::bold()                                \
                << "FATP_ASSERT_LT FAILED: " << fat_p::testing::colors::reset() << fat_p::testing::colors::red()  \
                << msg << "\n  Expected: " << actual_val << " < " << expected_val << "\n  at " << __FILE__ << ":" \
                << __LINE__ << fat_p::testing::colors::reset() << std::endl;                                      \
            if (fat_p::testing::get_test_config().abort_on_failure)                                               \
            {                                                                                                     \
                std::abort();                                                                                     \
            }                                                                                                     \
            if (fat_p::testing::get_subtest_tracker().is_inside_subtest())                                        \
            {                                                                                                     \
                fat_p::testing::get_subtest_tracker().fail_current_subtest(msg);                                  \
            }                                                                                                     \
            else                                                                                                  \
            {                                                                                                     \
                return false;                                                                                     \
            }                                                                                                     \
        }                                                                                                         \
    }

/**
 * @brief Assert less than or equal
 *
 * Uses auto&& (universal/forwarding reference) to handle all value categories
 * without copying. Binds lvalues as lvalue references and extends lifetime
 * of rvalues.
 */
#define FATP_ASSERT_LE(actual, expected, msg)                                                                      \
    {                                                                                                              \
        auto&& actual_val = (actual);                                                                              \
        auto&& expected_val = (expected);                                                                          \
        if (!fat_p::testing::detail::safe_le(actual_val, expected_val))                                                                         \
        {                                                                                                          \
            *fat_p::testing::get_test_config().error                                                               \
                << fat_p::testing::colors::red() << fat_p::testing::colors::bold()                                 \
                << "FATP_ASSERT_LE FAILED: " << fat_p::testing::colors::reset() << fat_p::testing::colors::red()   \
                << msg << "\n  Expected: " << actual_val << " <= " << expected_val << "\n  at " << __FILE__ << ":" \
                << __LINE__ << fat_p::testing::colors::reset() << std::endl;                                       \
            if (fat_p::testing::get_test_config().abort_on_failure)                                                \
            {                                                                                                      \
                std::abort();                                                                                      \
            }                                                                                                      \
            if (fat_p::testing::get_subtest_tracker().is_inside_subtest())                                         \
            {                                                                                                      \
                fat_p::testing::get_subtest_tracker().fail_current_subtest(msg);                                   \
            }                                                                                                      \
            else                                                                                                   \
            {                                                                                                      \
                return false;                                                                                      \
            }                                                                                                      \
        }                                                                                                          \
    }

/**
 * @brief Assert greater than
 *
 * Uses auto&& (universal/forwarding reference) to handle all value categories
 * without copying. Binds lvalues as lvalue references and extends lifetime
 * of rvalues.
 */
#define FATP_ASSERT_GT(actual, expected, msg)                                                                     \
    {                                                                                                             \
        auto&& actual_val = (actual);                                                                             \
        auto&& expected_val = (expected);                                                                         \
        if (!fat_p::testing::detail::safe_gt(actual_val, expected_val))                                                                         \
        {                                                                                                         \
            *fat_p::testing::get_test_config().error                                                              \
                << fat_p::testing::colors::red() << fat_p::testing::colors::bold()                                \
                << "FATP_ASSERT_GT FAILED: " << fat_p::testing::colors::reset() << fat_p::testing::colors::red()  \
                << msg << "\n  Expected: " << actual_val << " > " << expected_val << "\n  at " << __FILE__ << ":" \
                << __LINE__ << fat_p::testing::colors::reset() << std::endl;                                      \
            if (fat_p::testing::get_test_config().abort_on_failure)                                               \
            {                                                                                                     \
                std::abort();                                                                                     \
            }                                                                                                     \
            if (fat_p::testing::get_subtest_tracker().is_inside_subtest())                                        \
            {                                                                                                     \
                fat_p::testing::get_subtest_tracker().fail_current_subtest(msg);                                  \
            }                                                                                                     \
            else                                                                                                  \
            {                                                                                                     \
                return false;                                                                                     \
            }                                                                                                     \
        }                                                                                                         \
    }

/**
 * @brief Assert greater than or equal
 *
 * Uses auto&& (universal/forwarding reference) to handle all value categories
 * without copying. Binds lvalues as lvalue references and extends lifetime
 * of rvalues.
 */
#define FATP_ASSERT_GE(actual, expected, msg)                                                                      \
    {                                                                                                              \
        auto&& actual_val = (actual);                                                                              \
        auto&& expected_val = (expected);                                                                          \
        if (!fat_p::testing::detail::safe_ge(actual_val, expected_val))                                                                         \
        {                                                                                                          \
            *fat_p::testing::get_test_config().error                                                               \
                << fat_p::testing::colors::red() << fat_p::testing::colors::bold()                                 \
                << "FATP_ASSERT_GE FAILED: " << fat_p::testing::colors::reset() << fat_p::testing::colors::red()   \
                << msg << "\n  Expected: " << actual_val << " >= " << expected_val << "\n  at " << __FILE__ << ":" \
                << __LINE__ << fat_p::testing::colors::reset() << std::endl;                                       \
            if (fat_p::testing::get_test_config().abort_on_failure)                                                \
            {                                                                                                      \
                std::abort();                                                                                      \
            }                                                                                                      \
            if (fat_p::testing::get_subtest_tracker().is_inside_subtest())                                         \
            {                                                                                                      \
                fat_p::testing::get_subtest_tracker().fail_current_subtest(msg);                                   \
            }                                                                                                      \
            else                                                                                                   \
            {                                                                                                      \
                return false;                                                                                      \
            }                                                                                                      \
        }                                                                                                          \
    }

/**
 * @brief Assert true
 */
#define FATP_ASSERT_TRUE(condition, msg)                                                                             \
    if (!(condition))                                                                                                \
    {                                                                                                                \
        *fat_p::testing::get_test_config().error << fat_p::testing::colors::red() << fat_p::testing::colors::bold()  \
                                                 << "FATP_ASSERT_TRUE FAILED: " << fat_p::testing::colors::reset()   \
                                                 << fat_p::testing::colors::red() << msg << "\n  at " << __FILE__    \
                                                 << ":" << __LINE__ << fat_p::testing::colors::reset() << std::endl; \
        if (fat_p::testing::get_test_config().abort_on_failure)                                                      \
        {                                                                                                            \
            std::abort();                                                                                            \
        }                                                                                                            \
        if (fat_p::testing::get_subtest_tracker().is_inside_subtest())                                               \
        {                                                                                                            \
            fat_p::testing::get_subtest_tracker().fail_current_subtest(msg);                                         \
        }                                                                                                            \
        else                                                                                                         \
        {                                                                                                            \
            return false;                                                                                            \
        }                                                                                                            \
    }

/**
 * @brief Assert false
 */
#define FATP_ASSERT_FALSE(condition, msg)                                                                            \
    if ((condition))                                                                                                 \
    {                                                                                                                \
        *fat_p::testing::get_test_config().error << fat_p::testing::colors::red() << fat_p::testing::colors::bold()  \
                                                 << "FATP_ASSERT_FALSE FAILED: " << fat_p::testing::colors::reset()  \
                                                 << fat_p::testing::colors::red() << msg << "\n  at " << __FILE__    \
                                                 << ":" << __LINE__ << fat_p::testing::colors::reset() << std::endl; \
        if (fat_p::testing::get_test_config().abort_on_failure)                                                      \
        {                                                                                                            \
            std::abort();                                                                                            \
        }                                                                                                            \
        if (fat_p::testing::get_subtest_tracker().is_inside_subtest())                                               \
        {                                                                                                            \
            fat_p::testing::get_subtest_tracker().fail_current_subtest(msg);                                         \
        }                                                                                                            \
        else                                                                                                         \
        {                                                                                                            \
            return false;                                                                                            \
        }                                                                                                            \
    }

/**
 * @brief Assert pointer is nullptr
 */
#define FATP_ASSERT_NULLPTR(ptr, msg)                                                                                 \
    {                                                                                                                 \
        auto&& ptr_val = (ptr);                                                                                       \
        if (ptr_val != nullptr)                                                                                       \
        {                                                                                                             \
            *fat_p::testing::get_test_config().error                                                                  \
                << fat_p::testing::colors::red() << fat_p::testing::colors::bold()                                    \
                << "FATP_ASSERT_NULLPTR FAILED: " << fat_p::testing::colors::reset() << fat_p::testing::colors::red() \
                << msg << "\n  Expected: nullptr"                                                                     \
                << "\n  Actual:   " << static_cast<const void*>(ptr_val) << "\n  at " << __FILE__ << ":" << __LINE__  \
                << fat_p::testing::colors::reset() << std::endl;                                                      \
            if (fat_p::testing::get_test_config().abort_on_failure)                                                   \
            {                                                                                                         \
                std::abort();                                                                                         \
            }                                                                                                         \
            if (fat_p::testing::get_subtest_tracker().is_inside_subtest())                                            \
            {                                                                                                         \
                fat_p::testing::get_subtest_tracker().fail_current_subtest(msg);                                      \
            }                                                                                                         \
            else                                                                                                      \
            {                                                                                                         \
                return false;                                                                                         \
            }                                                                                                         \
        }                                                                                                             \
    }

/**
 * @brief Assert pointer is not nullptr
 */
#define FATP_ASSERT_NOT_NULLPTR(ptr, msg)                                                                    \
    {                                                                                                        \
        auto&& ptr_val = (ptr);                                                                              \
        if (ptr_val == nullptr)                                                                              \
        {                                                                                                    \
            *fat_p::testing::get_test_config().error                                                         \
                << fat_p::testing::colors::red() << fat_p::testing::colors::bold()                           \
                << "FATP_ASSERT_NOT_NULLPTR FAILED: " << fat_p::testing::colors::reset()                     \
                << fat_p::testing::colors::red() << msg << "\n  Expected: non-null pointer"                  \
                << "\n  Actual:   nullptr"                                                                   \
                << "\n  at " << __FILE__ << ":" << __LINE__ << fat_p::testing::colors::reset() << std::endl; \
            if (fat_p::testing::get_test_config().abort_on_failure)                                          \
            {                                                                                                \
                std::abort();                                                                                \
            }                                                                                                \
            if (fat_p::testing::get_subtest_tracker().is_inside_subtest())                                   \
            {                                                                                                \
                fat_p::testing::get_subtest_tracker().fail_current_subtest(msg);                             \
            }                                                                                                \
            else                                                                                             \
            {                                                                                                \
                return false;                                                                                \
            }                                                                                                \
        }                                                                                                    \
    }

/**
 * @brief Assert that two floating-point values are approximately equal
 * Uses primitive::are_close() with default epsilon
 *
 * Uses auto&& (universal/forwarding reference) to handle all value categories
 * without copying. Binds lvalues as lvalue references and extends lifetime
 * of rvalues.
 */
#define FATP_ASSERT_CLOSE(actual, expected, msg)                                                                       \
    {                                                                                                                  \
        auto&& actual_val = (actual);                                                                                  \
        auto&& expected_val = (expected);                                                                              \
        if (!fat_p::testing::primitive::are_close(actual_val, expected_val))                                           \
        {                                                                                                              \
            *fat_p::testing::get_test_config().error                                                                   \
                << fat_p::testing::colors::red() << fat_p::testing::colors::bold()                                     \
                << "FATP_ASSERT_CLOSE FAILED: " << fat_p::testing::colors::reset() << fat_p::testing::colors::red()    \
                << msg << "\n  Expected: " << expected_val << "\n  Actual:   " << actual_val                           \
                << "\n  Diff:     " << std::abs(actual_val - expected_val) << "\n  at " << __FILE__ << ":" << __LINE__ \
                << fat_p::testing::colors::reset() << std::endl;                                                       \
            if (fat_p::testing::get_test_config().abort_on_failure)                                                    \
            {                                                                                                          \
                std::abort();                                                                                          \
            }                                                                                                          \
            if (fat_p::testing::get_subtest_tracker().is_inside_subtest())                                             \
            {                                                                                                          \
                fat_p::testing::get_subtest_tracker().fail_current_subtest(msg);                                       \
            }                                                                                                          \
            else                                                                                                       \
            {                                                                                                          \
                return false;                                                                                          \
            }                                                                                                          \
        }                                                                                                              \
    }

/**
 * @brief Assert that two floating-point values are approximately equal with custom epsilon
 * Uses the same epsilon value for both relative and absolute tolerance
 *
 * Uses auto&& (universal/forwarding reference) to handle all value categories
 * without copying. Binds lvalues as lvalue references and extends lifetime
 * of rvalues.
 */
#define FATP_ASSERT_CLOSE_EPS(actual, expected, epsilon, msg)                                                          \
    {                                                                                                                  \
        auto&& actual_val = (actual);                                                                                  \
        auto&& expected_val = (expected);                                                                              \
        auto&& epsilon_val = (epsilon);                                                                                \
        if (!fat_p::testing::primitive::are_close(actual_val, expected_val, epsilon_val, epsilon_val))                 \
        {                                                                                                              \
            *fat_p::testing::get_test_config().error                                                                   \
                << fat_p::testing::colors::red() << fat_p::testing::colors::bold()                                     \
                << "FATP_ASSERT_CLOSE_EPS FAILED: " << fat_p::testing::colors::reset()                                 \
                << fat_p::testing::colors::red() << msg << "\n  Expected: " << expected_val                            \
                << "\n  Actual:   " << actual_val << "\n  Epsilon:  " << epsilon_val                                   \
                << "\n  Diff:     " << std::abs(actual_val - expected_val) << "\n  at " << __FILE__ << ":" << __LINE__ \
                << fat_p::testing::colors::reset() << std::endl;                                                       \
            if (fat_p::testing::get_test_config().abort_on_failure)                                                    \
            {                                                                                                          \
                std::abort();                                                                                          \
            }                                                                                                          \
            if (fat_p::testing::get_subtest_tracker().is_inside_subtest())                                             \
            {                                                                                                          \
                fat_p::testing::get_subtest_tracker().fail_current_subtest(msg);                                       \
            }                                                                                                          \
            else                                                                                                       \
            {                                                                                                          \
                return false;                                                                                          \
            }                                                                                                          \
        }                                                                                                              \
    }

/**
 * @brief Assert that two floating-point values are approximately equal
 *        with separate relative and absolute epsilon
 * Provides full control over the HybridComparisonPolicy parameters
 *
 * Uses auto&& (universal/forwarding reference) to handle all value categories
 * without copying. Binds lvalues as lvalue references and extends lifetime
 * of rvalues.
 */
#define FATP_ASSERT_CLOSE_REL_ABS(actual, expected, rel_eps, abs_eps, msg)                                    \
    {                                                                                                         \
        auto&& actual_val = (actual);                                                                         \
        auto&& expected_val = (expected);                                                                     \
        auto&& rel_eps_val = (rel_eps);                                                                       \
        auto&& abs_eps_val = (abs_eps);                                                                       \
        if (!fat_p::testing::primitive::are_close(actual_val, expected_val, rel_eps_val, abs_eps_val))        \
        {                                                                                                     \
            *fat_p::testing::get_test_config().error                                                          \
                << fat_p::testing::colors::red() << fat_p::testing::colors::bold()                            \
                << "FATP_ASSERT_CLOSE_REL_ABS FAILED: " << fat_p::testing::colors::reset()                    \
                << fat_p::testing::colors::red() << msg << "\n  Expected: " << expected_val                   \
                << "\n  Actual:   " << actual_val << "\n  Rel Eps:  " << rel_eps_val                          \
                << "\n  Abs Eps:  " << abs_eps_val << "\n  Diff:     " << std::abs(actual_val - expected_val) \
                << "\n  at " << __FILE__ << ":" << __LINE__ << fat_p::testing::colors::reset() << std::endl;  \
            if (fat_p::testing::get_test_config().abort_on_failure)                                           \
            {                                                                                                 \
                std::abort();                                                                                 \
            }                                                                                                 \
            if (fat_p::testing::get_subtest_tracker().is_inside_subtest())                                    \
            {                                                                                                 \
                fat_p::testing::get_subtest_tracker().fail_current_subtest(msg);                              \
            }                                                                                                 \
            else                                                                                              \
            {                                                                                                 \
                return false;                                                                                 \
            }                                                                                                 \
        }                                                                                                     \
    }

/**
 * @brief Assert that an expression throws a specific exception type
 *
 * This macro verifies that an expression throws the expected exception type.
 * If the expression throws a different exception type, the macro reports which
 * exception was actually thrown and displays its message if available.
 *
 * Usage:
 *   FATP_ASSERT_THROWS(my_function(), std::runtime_error, "Should throw runtime_error");
 *
 * If the expression does not throw the expected exception type, prints error
 * message and returns false from the calling function.
 *
 * @param expression Expression to evaluate (should throw)
 * @param exception_type Expected exception type
 * @param msg Error message to display on failure
 */
#define FATP_ASSERT_THROWS(expression, exception_type, msg)                                                          \
    {                                                                                                                \
        bool threw_correct = false;                                                                                  \
        bool threw_wrong = false;                                                                                    \
        std::string wrong_exception_msg;                                                                             \
        try                                                                                                          \
        {                                                                                                            \
            (void)(expression);                                                                                      \
        }                                                                                                            \
        catch (const exception_type&)                                                                                \
        {                                                                                                            \
            threw_correct = true;                                                                                    \
        }                                                                                                            \
        catch (const std::exception& e)                                                                              \
        {                                                                                                            \
            threw_wrong = true;                                                                                      \
            wrong_exception_msg = e.what();                                                                          \
        }                                                                                                            \
        catch (...)                                                                                                  \
        {                                                                                                            \
            threw_wrong = true;                                                                                      \
            wrong_exception_msg = "(unknown exception type)";                                                        \
        }                                                                                                            \
        if (!threw_correct)                                                                                          \
        {                                                                                                            \
            *fat_p::testing::get_test_config().error                                                                 \
                << fat_p::testing::colors::red() << fat_p::testing::colors::bold()                                   \
                << "FATP_ASSERT_THROWS FAILED: " << fat_p::testing::colors::reset() << fat_p::testing::colors::red() \
                << msg;                                                                                              \
            if (threw_wrong)                                                                                         \
            {                                                                                                        \
                *fat_p::testing::get_test_config().error                                                             \
                    << "\n  Expected: " << #exception_type << "\n  " << fat_p::testing::colors::yellow()             \
                    << "Got different exception: " << wrong_exception_msg << fat_p::testing::colors::red();          \
            }                                                                                                        \
            else                                                                                                     \
            {                                                                                                        \
                *fat_p::testing::get_test_config().error << "\n  Expected exception: " << #exception_type            \
                                                         << "\n  But no exception was thrown";                       \
            }                                                                                                        \
            *fat_p::testing::get_test_config().error << "\n  at " << __FILE__ << ":" << __LINE__                     \
                                                     << fat_p::testing::colors::reset() << std::endl;                \
            if (fat_p::testing::get_test_config().abort_on_failure)                                                  \
            {                                                                                                        \
                std::abort();                                                                                        \
            }                                                                                                        \
            if (fat_p::testing::get_subtest_tracker().is_inside_subtest())                                           \
            {                                                                                                        \
                fat_p::testing::get_subtest_tracker().fail_current_subtest(msg);                                     \
            }                                                                                                        \
            else                                                                                                     \
            {                                                                                                        \
                return false;                                                                                        \
            }                                                                                                        \
        }                                                                                                            \
    }

/**
 * @brief Assert that an expression does not throw any exception
 *
 * This macro verifies that an expression executes without throwing any exception.
 * If an exception is thrown, it displays the exception message if available.
 *
 * Usage:
 *   FATP_ASSERT_NO_THROW(my_function(), "Should not throw");
 *
 * If the expression throws any exception, prints error message and returns
 * false from the calling function.
 *
 * @param expression Expression to evaluate (should not throw)
 * @param msg Error message to display on failure
 */
#define FATP_ASSERT_NO_THROW(expression, msg)                                                                          \
    {                                                                                                                  \
        bool threw = false;                                                                                            \
        std::string exception_msg;                                                                                     \
        try                                                                                                            \
        {                                                                                                              \
            (expression);                                                                                              \
        }                                                                                                              \
        catch (const std::exception& e)                                                                                \
        {                                                                                                              \
            threw = true;                                                                                              \
            exception_msg = e.what();                                                                                  \
        }                                                                                                              \
        catch (...)                                                                                                    \
        {                                                                                                              \
            threw = true;                                                                                              \
            exception_msg = "(unknown exception type)";                                                                \
        }                                                                                                              \
        if (threw)                                                                                                     \
        {                                                                                                              \
            *fat_p::testing::get_test_config().error                                                                   \
                << fat_p::testing::colors::red() << fat_p::testing::colors::bold()                                     \
                << "FATP_ASSERT_NO_THROW FAILED: " << fat_p::testing::colors::reset() << fat_p::testing::colors::red() \
                << msg << "\n  Unexpected exception: " << exception_msg << "\n  at " << __FILE__ << ":" << __LINE__    \
                << fat_p::testing::colors::reset() << std::endl;                                                       \
            if (fat_p::testing::get_test_config().abort_on_failure)                                                    \
            {                                                                                                          \
                std::abort();                                                                                          \
            }                                                                                                          \
            if (fat_p::testing::get_subtest_tracker().is_inside_subtest())                                             \
            {                                                                                                          \
                fat_p::testing::get_subtest_tracker().fail_current_subtest(msg);                                       \
            }                                                                                                          \
            else                                                                                                       \
            {                                                                                                          \
                return false;                                                                                          \
            }                                                                                                          \
        }                                                                                                              \
    }

// ============================================================================
// String Assertions
// ============================================================================

/**
 * @brief Assert that a string contains a substring
 *
 * Usage:
 *   FATP_ASSERT_CONTAINS("hello world", "world", "Should contain world");
 *
 * Long strings are truncated in error output for readability.
 */
#define FATP_ASSERT_CONTAINS(str, substr, msg)                                                                         \
    {                                                                                                                  \
        auto&& str_val = (str);                                                                                        \
        auto&& substr_val = (substr);                                                                                  \
        if (!fat_p::testing::string_utils::contains(str_val, substr_val))                                              \
        {                                                                                                              \
            *fat_p::testing::get_test_config().error                                                                   \
                << fat_p::testing::colors::red() << fat_p::testing::colors::bold()                                     \
                << "FATP_ASSERT_CONTAINS FAILED: " << fat_p::testing::colors::reset() << fat_p::testing::colors::red() \
                << msg << "\n  String:    \"" << fat_p::testing::string_utils::truncate_for_display(str_val) << "\""   \
                << "\n  Substring: \"" << substr_val << "\" (not found)"                                               \
                << "\n  at " << __FILE__ << ":" << __LINE__ << fat_p::testing::colors::reset() << std::endl;           \
            if (fat_p::testing::get_test_config().abort_on_failure)                                                    \
            {                                                                                                          \
                std::abort();                                                                                          \
            }                                                                                                          \
            if (fat_p::testing::get_subtest_tracker().is_inside_subtest())                                             \
            {                                                                                                          \
                fat_p::testing::get_subtest_tracker().fail_current_subtest(msg);                                       \
            }                                                                                                          \
            else                                                                                                       \
            {                                                                                                          \
                return false;                                                                                          \
            }                                                                                                          \
        }                                                                                                              \
    }

/**
 * @brief Assert that a string does not contain a substring
 *
 * Long strings are truncated in error output for readability.
 */
#define FATP_ASSERT_NOT_CONTAINS(str, substr, msg)                                                           \
    {                                                                                                        \
        auto&& str_val = (str);                                                                              \
        auto&& substr_val = (substr);                                                                        \
        if (fat_p::testing::string_utils::contains(str_val, substr_val))                                     \
        {                                                                                                    \
            *fat_p::testing::get_test_config().error                                                         \
                << fat_p::testing::colors::red() << fat_p::testing::colors::bold()                           \
                << "FATP_ASSERT_NOT_CONTAINS FAILED: " << fat_p::testing::colors::reset()                    \
                << fat_p::testing::colors::red() << msg << "\n  String:    \""                               \
                << fat_p::testing::string_utils::truncate_for_display(str_val) << "\""                       \
                << "\n  Substring: \"" << substr_val << "\" (found but should not be)"                       \
                << "\n  at " << __FILE__ << ":" << __LINE__ << fat_p::testing::colors::reset() << std::endl; \
            if (fat_p::testing::get_test_config().abort_on_failure)                                          \
            {                                                                                                \
                std::abort();                                                                                \
            }                                                                                                \
            if (fat_p::testing::get_subtest_tracker().is_inside_subtest())                                   \
            {                                                                                                \
                fat_p::testing::get_subtest_tracker().fail_current_subtest(msg);                             \
            }                                                                                                \
            else                                                                                             \
            {                                                                                                \
                return false;                                                                                \
            }                                                                                                \
        }                                                                                                    \
    }

/**
 * @brief Assert that a string starts with a prefix
 *
 * Long strings are truncated in error output for readability.
 */
#define FATP_ASSERT_STARTS_WITH(str, prefix, msg)                                                            \
    {                                                                                                        \
        auto&& str_val = (str);                                                                              \
        auto&& prefix_val = (prefix);                                                                        \
        if (!fat_p::testing::string_utils::starts_with(str_val, prefix_val))                                 \
        {                                                                                                    \
            *fat_p::testing::get_test_config().error                                                         \
                << fat_p::testing::colors::red() << fat_p::testing::colors::bold()                           \
                << "FATP_ASSERT_STARTS_WITH FAILED: " << fat_p::testing::colors::reset()                     \
                << fat_p::testing::colors::red() << msg << "\n  String: \""                                  \
                << fat_p::testing::string_utils::truncate_for_display(str_val) << "\""                       \
                << "\n  Prefix: \"" << prefix_val << "\" (not found)"                                        \
                << "\n  at " << __FILE__ << ":" << __LINE__ << fat_p::testing::colors::reset() << std::endl; \
            if (fat_p::testing::get_test_config().abort_on_failure)                                          \
            {                                                                                                \
                std::abort();                                                                                \
            }                                                                                                \
            if (fat_p::testing::get_subtest_tracker().is_inside_subtest())                                   \
            {                                                                                                \
                fat_p::testing::get_subtest_tracker().fail_current_subtest(msg);                             \
            }                                                                                                \
            else                                                                                             \
            {                                                                                                \
                return false;                                                                                \
            }                                                                                                \
        }                                                                                                    \
    }

/**
 * @brief Assert that a string ends with a suffix
 *
 * Long strings are truncated in error output for readability.
 */
#define FATP_ASSERT_ENDS_WITH(str, suffix, msg)                                                              \
    {                                                                                                        \
        auto&& str_val = (str);                                                                              \
        auto&& suffix_val = (suffix);                                                                        \
        if (!fat_p::testing::string_utils::ends_with(str_val, suffix_val))                                   \
        {                                                                                                    \
            *fat_p::testing::get_test_config().error                                                         \
                << fat_p::testing::colors::red() << fat_p::testing::colors::bold()                           \
                << "FATP_ASSERT_ENDS_WITH FAILED: " << fat_p::testing::colors::reset()                       \
                << fat_p::testing::colors::red() << msg << "\n  String: \""                                  \
                << fat_p::testing::string_utils::truncate_for_display(str_val) << "\""                       \
                << "\n  Suffix: \"" << suffix_val << "\" (not found)"                                        \
                << "\n  at " << __FILE__ << ":" << __LINE__ << fat_p::testing::colors::reset() << std::endl; \
            if (fat_p::testing::get_test_config().abort_on_failure)                                          \
            {                                                                                                \
                std::abort();                                                                                \
            }                                                                                                \
            if (fat_p::testing::get_subtest_tracker().is_inside_subtest())                                   \
            {                                                                                                \
                fat_p::testing::get_subtest_tracker().fail_current_subtest(msg);                             \
            }                                                                                                \
            else                                                                                             \
            {                                                                                                \
                return false;                                                                                \
            }                                                                                                \
        }                                                                                                    \
    }

/**
 * @brief Assert that a string matches a regular expression
 */
#define FATP_ASSERT_MATCHES(str, pattern, msg)                                                                       \
    {                                                                                                                \
        auto&& str_val = (str);                                                                                      \
        auto&& pattern_val = (pattern);                                                                              \
        try                                                                                                          \
        {                                                                                                            \
            std::regex regex_pattern(pattern_val);                                                                   \
            if (!std::regex_match(str_val, regex_pattern))                                                           \
            {                                                                                                        \
                *fat_p::testing::get_test_config().error                                                             \
                    << fat_p::testing::colors::red() << fat_p::testing::colors::bold()                               \
                    << "FATP_ASSERT_MATCHES FAILED: " << fat_p::testing::colors::reset()                             \
                    << fat_p::testing::colors::red() << msg << "\n  String:  \"" << str_val << "\""                  \
                    << "\n  Pattern: \"" << pattern_val << "\" (no match)"                                           \
                    << "\n  at " << __FILE__ << ":" << __LINE__ << fat_p::testing::colors::reset() << std::endl;     \
                if (fat_p::testing::get_test_config().abort_on_failure)                                              \
                {                                                                                                    \
                    std::abort();                                                                                    \
                }                                                                                                    \
                if (fat_p::testing::get_subtest_tracker().is_inside_subtest())                                       \
                {                                                                                                    \
                    fat_p::testing::get_subtest_tracker().fail_current_subtest(msg);                                 \
                }                                                                                                    \
                else                                                                                                 \
                {                                                                                                    \
                    return false;                                                                                    \
                }                                                                                                    \
            }                                                                                                        \
        }                                                                                                            \
        catch (const std::regex_error& e)                                                                            \
        {                                                                                                            \
            *fat_p::testing::get_test_config().error                                                                 \
                << fat_p::testing::colors::red() << fat_p::testing::colors::bold()                                   \
                << "FATP_ASSERT_MATCHES ERROR: " << fat_p::testing::colors::reset() << fat_p::testing::colors::red() \
                << msg << "\n  Invalid regex pattern: \"" << pattern_val << "\""                                     \
                << "\n  Error: " << e.what() << "\n  at " << __FILE__ << ":" << __LINE__                             \
                << fat_p::testing::colors::reset() << std::endl;                                                     \
            if (fat_p::testing::get_test_config().abort_on_failure)                                                  \
            {                                                                                                        \
                std::abort();                                                                                        \
            }                                                                                                        \
            if (fat_p::testing::get_subtest_tracker().is_inside_subtest())                                           \
            {                                                                                                        \
                fat_p::testing::get_subtest_tracker().fail_current_subtest(msg);                                     \
            }                                                                                                        \
            else                                                                                                     \
            {                                                                                                        \
                return false;                                                                                        \
            }                                                                                                        \
        }                                                                                                            \
    }

/**
 * @brief Assert that two strings are equal (case-insensitive)
 *
 * Long strings are truncated in error output for readability.
 */
#define FATP_ASSERT_STR_EQ_IGNORE_CASE(str1, str2, msg)                                                       \
    {                                                                                                         \
        auto&& str1_val = (str1);                                                                             \
        auto&& str2_val = (str2);                                                                             \
        std::string lower1 = fat_p::testing::string_utils::to_lower(str1_val);                                \
        std::string lower2 = fat_p::testing::string_utils::to_lower(str2_val);                                \
        if (lower1 != lower2)                                                                                 \
        {                                                                                                     \
            *fat_p::testing::get_test_config().error                                                          \
                << fat_p::testing::colors::red() << fat_p::testing::colors::bold()                            \
                << "FATP_ASSERT_STR_EQ_IGNORE_CASE FAILED: " << fat_p::testing::colors::reset()               \
                << fat_p::testing::colors::red() << msg << "\n  Expected: \""                                 \
                << fat_p::testing::string_utils::truncate_for_display(str2_val) << "\""                       \
                << "\n  Actual:   \"" << fat_p::testing::string_utils::truncate_for_display(str1_val) << "\"" \
                << "\n  at " << __FILE__ << ":" << __LINE__ << fat_p::testing::colors::reset() << std::endl;  \
            if (fat_p::testing::get_test_config().abort_on_failure)                                           \
            {                                                                                                 \
                std::abort();                                                                                 \
            }                                                                                                 \
            if (fat_p::testing::get_subtest_tracker().is_inside_subtest())                                    \
            {                                                                                                 \
                fat_p::testing::get_subtest_tracker().fail_current_subtest(msg);                              \
            }                                                                                                 \
            else                                                                                              \
            {                                                                                                 \
                return false;                                                                                 \
            }                                                                                                 \
        }                                                                                                     \
    }

// ============================================================================
// Container/Range Assertions
// ============================================================================

/**
 * @brief Assert that two containers have equal elements
 *
 * Provides detailed output showing which elements differ
 */
#define FATP_ASSERT_RANGE_EQ(actual, expected, msg)                                                                    \
    {                                                                                                                  \
        auto&& actual_val = (actual);                                                                                  \
        auto&& expected_val = (expected);                                                                              \
                                                                                                                       \
        size_t actual_size = static_cast<size_t>(std::distance(std::begin(actual_val), std::end(actual_val)));          \
        size_t expected_size = static_cast<size_t>(std::distance(std::begin(expected_val), std::end(expected_val)));    \
                                                                                                                       \
        bool ranges_equal = true;                                                                                      \
        std::ostringstream diff_output;                                                                                \
                                                                                                                       \
        if (actual_size != expected_size)                                                                              \
        {                                                                                                              \
            ranges_equal = false;                                                                                      \
            diff_output << "\n  Size mismatch: " << actual_size << " != " << expected_size;                            \
        }                                                                                                              \
                                                                                                                       \
        auto actual_it = std::begin(actual_val);                                                                       \
        auto actual_end = std::end(actual_val);                                                                        \
        auto expected_it = std::begin(expected_val);                                                                   \
        auto expected_end = std::end(expected_val);                                                                    \
                                                                                                                       \
        size_t index = 0;                                                                                              \
        while (actual_it != actual_end && expected_it != expected_end)                                                 \
        {                                                                                                              \
            if (!(*actual_it == *expected_it))                                                                         \
            {                                                                                                          \
                ranges_equal = false;                                                                                  \
                diff_output << "\n  Element [" << index << "]: " << *actual_it << " != " << *expected_it;              \
            }                                                                                                          \
            ++actual_it;                                                                                               \
            ++expected_it;                                                                                             \
            ++index;                                                                                                   \
        }                                                                                                              \
                                                                                                                       \
        if (!ranges_equal)                                                                                             \
        {                                                                                                              \
            *fat_p::testing::get_test_config().error                                                                   \
                << fat_p::testing::colors::red() << fat_p::testing::colors::bold()                                     \
                << "FATP_ASSERT_RANGE_EQ FAILED: " << fat_p::testing::colors::reset() << fat_p::testing::colors::red() \
                << msg << diff_output.str() << "\n  at " << __FILE__ << ":" << __LINE__                                \
                << fat_p::testing::colors::reset() << std::endl;                                                       \
            if (fat_p::testing::get_test_config().abort_on_failure)                                                    \
            {                                                                                                          \
                std::abort();                                                                                          \
            }                                                                                                          \
            if (fat_p::testing::get_subtest_tracker().is_inside_subtest())                                             \
            {                                                                                                          \
                fat_p::testing::get_subtest_tracker().fail_current_subtest(msg);                                       \
            }                                                                                                          \
            else                                                                                                       \
            {                                                                                                          \
                return false;                                                                                          \
            }                                                                                                          \
        }                                                                                                              \
    }

/**
 * @brief Assert that two floating-point containers have approximately equal elements
 */
#define FATP_ASSERT_RANGE_CLOSE(actual, expected, epsilon, msg)                                                \
    {                                                                                                          \
        auto&& actual_val = (actual);                                                                          \
        auto&& expected_val = (expected);                                                                      \
        auto&& epsilon_val = (epsilon);                                                                        \
                                                                                                               \
        size_t actual_size = static_cast<size_t>(std::distance(std::begin(actual_val), std::end(actual_val)));  \
        size_t expected_size = static_cast<size_t>(std::distance(std::begin(expected_val), std::end(expected_val))); \
                                                                                                               \
        bool ranges_equal = true;                                                                              \
        std::ostringstream diff_output;                                                                        \
                                                                                                               \
        if (actual_size != expected_size)                                                                      \
        {                                                                                                      \
            ranges_equal = false;                                                                              \
            diff_output << "\n  Size mismatch: " << actual_size << " != " << expected_size;                    \
        }                                                                                                      \
                                                                                                               \
        auto actual_it = std::begin(actual_val);                                                               \
        auto actual_end = std::end(actual_val);                                                                \
        auto expected_it = std::begin(expected_val);                                                           \
        auto expected_end = std::end(expected_val);                                                            \
                                                                                                               \
        size_t index = 0;                                                                                      \
        while (actual_it != actual_end && expected_it != expected_end)                                         \
        {                                                                                                      \
            if (!fat_p::testing::primitive::are_close(*actual_it, *expected_it, epsilon_val, epsilon_val))     \
            {                                                                                                  \
                ranges_equal = false;                                                                          \
                diff_output << "\n  Element [" << index << "]: " << *actual_it << " != " << *expected_it       \
                            << " (diff: " << std::abs(*actual_it - *expected_it) << ")";                       \
            }                                                                                                  \
            ++actual_it;                                                                                       \
            ++expected_it;                                                                                     \
            ++index;                                                                                           \
        }                                                                                                      \
                                                                                                               \
        if (!ranges_equal)                                                                                     \
        {                                                                                                      \
            *fat_p::testing::get_test_config().error                                                           \
                << fat_p::testing::colors::red() << fat_p::testing::colors::bold()                             \
                << "FATP_ASSERT_RANGE_CLOSE FAILED: " << fat_p::testing::colors::reset()                       \
                << fat_p::testing::colors::red() << msg << diff_output.str() << "\n  Epsilon: " << epsilon_val \
                << "\n  at " << __FILE__ << ":" << __LINE__ << fat_p::testing::colors::reset() << std::endl;   \
            if (fat_p::testing::get_test_config().abort_on_failure)                                            \
            {                                                                                                  \
                std::abort();                                                                                  \
            }                                                                                                  \
            if (fat_p::testing::get_subtest_tracker().is_inside_subtest())                                     \
            {                                                                                                  \
                fat_p::testing::get_subtest_tracker().fail_current_subtest(msg);                               \
            }                                                                                                  \
            else                                                                                               \
            {                                                                                                  \
                return false;                                                                                  \
            }                                                                                                  \
        }                                                                                                      \
    }

// ============================================================================
// Performance Measurement
// ============================================================================

/**
 * @brief Prevents the compiler from optimizing away the value
 *
 * This function forces the compiler to treat the value as used,
 * preventing dead code elimination in benchmarks.
 *
 * @tparam T Type of value
 * @param value Value to preserve
 */
#if defined(_MSC_VER)
// MSVC requires special handling - use a global volatile sink
// to truly prevent optimization
namespace benchmark_detail
{
// Volatile sink that actually stores value-derived data
// Using char array to handle any type size
inline volatile char benchmark_sink_storage[sizeof(void*)];

template <typename T>
__forceinline void use_value(T const& value) noexcept
{
    // Read actual bytes from the value (not just its address)
    // This creates a true data dependency on the value's computation
    // Use const void* intermediate to handle volatile types correctly
    const volatile void* vptr = &value;
    const volatile char* bytes = static_cast<const volatile char*>(vptr);

    // Write first byte of value to volatile sink
    // The compiler cannot know what this byte will be without computing value
    benchmark_sink_storage[0] = bytes[0];

    // For larger types, also write the last byte to catch more optimizations
    if constexpr (sizeof(T) > 1)
    {
        benchmark_sink_storage[1] = bytes[sizeof(T) - 1];
    }

    _ReadWriteBarrier();
}

// Specialization for pointer types - store the pointer value
template <typename T>
__forceinline void use_value(T* const& ptr) noexcept
{
    // Store the pointer value itself (the address it points to)
    const volatile void* vptr = &ptr;
    const volatile char* bytes = static_cast<const volatile char*>(vptr);
    benchmark_sink_storage[0] = bytes[0];
    if constexpr (sizeof(T*) > 1)
    {
        benchmark_sink_storage[1] = bytes[sizeof(T*) - 1];
    }
    _ReadWriteBarrier();
}
} // namespace benchmark_detail

template <typename T>
__forceinline void DoNotOptimize(T const& value) noexcept
{
    benchmark_detail::use_value(value);
}

template <typename T>
__forceinline void DoNotOptimize(T& value) noexcept
{
    benchmark_detail::use_value(value);
    _ReadWriteBarrier();
}

#else
// GCC/Clang implementation using inline assembly

template <typename T>
inline void DoNotOptimize(T const& value) noexcept
{
    asm volatile("" : : "r,m"(value) : "memory");
}

/**
 * @brief Overload for non-const references
 */
template <typename T>
inline void DoNotOptimize(T& value) noexcept
{
#if defined(__clang__)
    asm volatile("" : "+r,m"(value) : : "memory");
#else
    asm volatile("" : "+m"(value) : : "memory");
#endif
}

#endif

/**
 * @brief Statistics for benchmark results
 */
struct BenchmarkStats
{
    double min_ms;
    double max_ms;
    double mean_ms;
    double median_ms;
    double stddev_ms;
    double p95_ms;
    double p99_ms;
    size_t outliers;
    size_t iterations;

    [[nodiscard]] double min_ns() const noexcept
    {
        return min_ms * 1000000.0;
    }
    [[nodiscard]] double max_ns() const noexcept
    {
        return max_ms * 1000000.0;
    }
    [[nodiscard]] double mean_ns() const noexcept
    {
        return mean_ms * 1000000.0;
    }
    [[nodiscard]] double median_ns() const noexcept
    {
        return median_ms * 1000000.0;
    }
    [[nodiscard]] double stddev_ns() const noexcept
    {
        return stddev_ms * 1000000.0;
    }
    [[nodiscard]] double p95_ns() const noexcept
    {
        return p95_ms * 1000000.0;
    }
    [[nodiscard]] double p99_ns() const noexcept
    {
        return p99_ms * 1000000.0;
    }
};

/**
 * @brief Baseline storage for benchmark regression detection
 */
class BenchmarkBaseline
{
private:
    std::map<std::string, BenchmarkStats> mBaselines;

public:
    void save(const std::string& name, const BenchmarkStats& stats)
    {
        mBaselines[name] = stats;
    }

    [[nodiscard]] bool has_baseline(const std::string& name) const
    {
        return mBaselines.find(name) != mBaselines.end();
    }

    [[nodiscard]] const BenchmarkStats& get(const std::string& name) const
    {
        return mBaselines.at(name);
    }

    [[nodiscard]] double compare(const std::string& name, const BenchmarkStats& current) const
    {
        if (!has_baseline(name))
        {
            return 0.0;
        }

        const auto& baseline = mBaselines.at(name);
        return ((current.mean_ms - baseline.mean_ms) / baseline.mean_ms) * 100.0;
    }
};

inline BenchmarkBaseline& get_benchmark_baseline()
{
    static BenchmarkBaseline baseline;
    return baseline;
}

// ============================================================================
// High-Resolution Timer (Platform-Specific)
// ============================================================================

/**
 * @brief High-resolution timer abstraction
 *
 * On Windows, uses QueryPerformanceCounter for sub-microsecond precision.
 * On other platforms, uses std::chrono::high_resolution_clock.
 *
 * The Windows high_resolution_clock often aliases to system_clock with ~1ms
 * resolution, making it unsuitable for micro-benchmarks. QueryPerformanceCounter
 * provides true high-resolution timing on Windows.
 */
class HighResolutionTimer
{
public:
#if defined(_WIN32) || defined(_WIN64)
    using time_point = LARGE_INTEGER;

    static time_point now()
    {
        LARGE_INTEGER t;
        QueryPerformanceCounter(&t);
        return t;
    }

    static double elapsed_ms(const time_point& start, const time_point& end)
    {
        LONGLONG freq = get_frequency();
        if (freq == 0)
        {
            return 0.0; // Safety: avoid division by zero
        }
        return static_cast<double>(end.QuadPart - start.QuadPart) * 1000.0 / static_cast<double>(freq);
    }

    static double resolution_ms()
    {
        LONGLONG freq = get_frequency();
        if (freq == 0)
        {
            return 1.0; // Fallback to 1ms resolution
        }
        return 1000.0 / static_cast<double>(freq);
    }

    static const char* timer_name()
    {
        return "QueryPerformanceCounter";
    }

private:
    // Cache the frequency - it doesn't change during execution
    // Returns 0 on failure (extremely rare - only on ancient hardware)
    static LONGLONG get_frequency()
    {
        static LONGLONG frequency = []() -> LONGLONG {
            LARGE_INTEGER freq;
            if (!QueryPerformanceFrequency(&freq) || freq.QuadPart == 0)
            {
                return 0; // Fallback: will cause elapsed_ms to return 0
            }
            return freq.QuadPart;
        }();
        return frequency;
    }

public:
#else
    using time_point = std::chrono::high_resolution_clock::time_point;

    static time_point now()
    {
        return std::chrono::high_resolution_clock::now();
    }

    static double elapsed_ms(const time_point& start, const time_point& end)
    {
        return std::chrono::duration<double, std::milli>(end - start).count();
    }

    static double resolution_ms()
    {
        using clock = std::chrono::high_resolution_clock;
        return static_cast<double>(clock::period::num) / clock::period::den * 1000.0;
    }

    static const char* timer_name()
    {
        return "std::chrono::high_resolution_clock";
    }
#endif
};

// ============================================================================
// PDH CPU Monitor (Optional - Windows Only)
// ============================================================================
// Provides accurate CPU frequency monitoring including thermal throttling
// and turbo boost detection via Windows Performance Data Helper (PDH).
//
// LIMITATIONS:
// 1. English Windows Only (uses hardcoded counter strings)
// 2. Requires FATP_ENABLE_PDH_STATS to be defined
// 3. pdh.dll is loaded dynamically - no link-time dependency
// ============================================================================

#if defined(FATP_ENABLE_PDH_STATS) && (defined(_WIN32) || defined(_WIN64))

class PdhCpuMonitor
{
private:
    // Forward declare PDH types to avoid requiring pdh.h
    using PDH_HQUERY = void*;
    using PDH_HCOUNTER = void*;
    using PDH_STATUS = long;

    struct PDH_FMT_COUNTERVALUE
    {
        DWORD CStatus;
        union
        {
            LONG longValue;
            double doubleValue;
            LONGLONG largeValue;
            LPCSTR AnsiStringValue;
            LPCWSTR WideStringValue;
        };
    };

    static constexpr DWORD PDH_FMT_DOUBLE = 0x00000200;
    static constexpr PDH_STATUS ERROR_SUCCESS_PDH = 0;

    // Typedefs for dynamic loading
    using PdhOpenQueryA_t = PDH_STATUS(WINAPI*)(LPCSTR, DWORD_PTR, PDH_HQUERY*);
    using PdhAddCounterA_t = PDH_STATUS(WINAPI*)(PDH_HQUERY, LPCSTR, DWORD_PTR, PDH_HCOUNTER*);
    using PdhCollectQueryData_t = PDH_STATUS(WINAPI*)(PDH_HQUERY);
    using PdhGetFormattedCounterValue_t = PDH_STATUS(WINAPI*)(PDH_HCOUNTER, DWORD, LPDWORD, PDH_FMT_COUNTERVALUE*);
    using PdhCloseQuery_t = PDH_STATUS(WINAPI*)(PDH_HQUERY);

    HMODULE mHPdh = nullptr;
    PDH_HQUERY mHQuery = nullptr;
    PDH_HCOUNTER mHCounter = nullptr;

    // Function Pointers
    PdhOpenQueryA_t mPOpenQuery = nullptr;
    PdhAddCounterA_t mPAddCounter = nullptr;
    PdhCollectQueryData_t mPCollectData = nullptr;
    PdhGetFormattedCounterValue_t mPGetValue = nullptr;
    PdhCloseQuery_t mPCloseQuery = nullptr;

    bool mInitialized = false;

public:
    PdhCpuMonitor()
    {
        // 1. Load DLL dynamically
        mHPdh = LoadLibraryA("pdh.dll");
        if (!mHPdh)
        {
            return;
        }

        // 2. Resolve function pointers
        mPOpenQuery = reinterpret_cast<PdhOpenQueryA_t>(GetProcAddress(mHPdh, "PdhOpenQueryA"));
        mPAddCounter = reinterpret_cast<PdhAddCounterA_t>(GetProcAddress(mHPdh, "PdhAddCounterA"));
        mPCollectData = reinterpret_cast<PdhCollectQueryData_t>(GetProcAddress(mHPdh, "PdhCollectQueryData"));
        mPGetValue =
            reinterpret_cast<PdhGetFormattedCounterValue_t>(GetProcAddress(mHPdh, "PdhGetFormattedCounterValue"));
        mPCloseQuery = reinterpret_cast<PdhCloseQuery_t>(GetProcAddress(mHPdh, "PdhCloseQuery"));

        if (!mPOpenQuery || !mPAddCounter || !mPCollectData || !mPGetValue || !mPCloseQuery)
        {
            return;
        }

        // 3. Open Query
        if (mPOpenQuery(nullptr, 0, &mHQuery) != ERROR_SUCCESS_PDH)
        {
            return;
        }

        // 4. Add Counter (ENGLISH WINDOWS ONLY)
        // "Processor Information" supports >64 cores, unlike legacy "Processor"
        const char* counterPath = "\\Processor Information(_Total)\\% of Maximum Frequency";
        if (mPAddCounter(mHQuery, counterPath, 0, &mHCounter) != ERROR_SUCCESS_PDH)
        {
            // Fallback for older Windows versions
            counterPath = "\\Processor(_Total)\\% Processor Performance";
            if (mPAddCounter(mHQuery, counterPath, 0, &mHCounter) != ERROR_SUCCESS_PDH)
            {
                return;
            }
        }

        // 5. Prime the counter (first read is always invalid for rate counters)
        mPCollectData(mHQuery);
        mInitialized = true;
    }

    ~PdhCpuMonitor()
    {
        if (mHQuery && mPCloseQuery)
        {
            mPCloseQuery(mHQuery);
        }
        if (mHPdh)
        {
            FreeLibrary(mHPdh);
        }
    }

    // Non-copyable
    PdhCpuMonitor(const PdhCpuMonitor&) = delete;
    PdhCpuMonitor& operator=(const PdhCpuMonitor&) = delete;

    /**
     * @brief Returns current CPU frequency as a percentage of base frequency
     *
     * Examples:
     *   100.0 = Running at base clock
     *   150.0 = Turbo boost (50% above base)
     *   50.0  = Throttled to half speed
     *
     * @return Percentage (0.0 on error)
     */
    [[nodiscard]] double get_frequency_percentage()
    {
        if (!mInitialized)
        {
            return 0.0;
        }

        // Collect new data sample
        if (mPCollectData(mHQuery) != ERROR_SUCCESS_PDH)
        {
            return 0.0;
        }

        PDH_FMT_COUNTERVALUE value{};
        if (mPGetValue(mHCounter, PDH_FMT_DOUBLE, nullptr, &value) == ERROR_SUCCESS_PDH)
        {
            return value.doubleValue;
        }
        return 0.0;
    }

    /**
     * @brief Calculate actual frequency in MHz
     * @param base_freq_mhz Base CPU frequency
     * @return Current frequency in MHz (0.0 on error)
     */
    [[nodiscard]] double get_current_freq_mhz(double base_freq_mhz)
    {
        double pct = get_frequency_percentage();
        if (pct <= 0.0)
        {
            return 0.0;
        }
        return base_freq_mhz * (pct / 100.0);
    }

    [[nodiscard]] bool is_available() const
    {
        return mInitialized;
    }
};

#endif // FATP_ENABLE_PDH_STATS && Windows

// ============================================================================
// System Information (Platform-Specific)
// ============================================================================

/**
 * @brief Captures system information for benchmark context
 *
 * Provides CPU model, core count, frequency, and timestamp information
 * to help interpret benchmark results. Platform-specific implementations
 * for Windows and Linux.
 *
 * Usage:
 *   auto info = SystemInfo::capture();
 *   info.print();
 */
class SystemInfo
{
public:
    std::string cpu_model;
    int logical_cores = 0;
    int physical_cores = 0;
    double base_freq_mhz = 0.0;
    double current_freq_mhz = 0.0;  // May be 0 if unavailable
    double cpu_temp_celsius = -1.0; // -1 if unavailable
    std::string timestamp;
    std::string os_info;

    /**
     * @brief Capture current system information
     * @return SystemInfo populated with available metrics
     */
    static SystemInfo capture()
    {
        SystemInfo info;
        info.logical_cores = static_cast<int>(std::thread::hardware_concurrency());
        info.timestamp = get_timestamp();

#if defined(_WIN32) || defined(_WIN64)
        info.cpu_model = get_cpu_model_windows();
        info.base_freq_mhz = get_cpu_freq_windows();
        info.current_freq_mhz = get_current_freq_windows(info.base_freq_mhz);
        info.physical_cores = get_physical_cores_windows();
        info.os_info = get_os_info_windows();
#else
        info.cpu_model = get_cpu_model_linux();
        info.base_freq_mhz = get_cpu_freq_linux();
        info.current_freq_mhz = get_current_freq_linux();
        info.physical_cores = get_physical_cores_linux();
        info.cpu_temp_celsius = get_cpu_temp_linux();
        info.os_info = get_os_info_linux();
#endif

        // Fallback for physical cores
        if (info.physical_cores <= 0)
        {
            info.physical_cores = info.logical_cores;
        }

        return info;
    }

    /**
     * @brief Print system information to stdout
     */
    void print() const
    {
        std::cout << colors::cyan() << "System Information:" << colors::reset() << "\n";
        std::cout << "  CPU: " << cpu_model << "\n";
        std::cout << "  Cores: " << physical_cores << " physical, " << logical_cores << " logical\n";

        if (base_freq_mhz > 0)
        {
            std::cout << "  Base Frequency: " << static_cast<int>(base_freq_mhz) << " MHz";
            if (current_freq_mhz > 0)
            {
                std::cout << " (current: " << static_cast<int>(current_freq_mhz) << " MHz";
                double throttle_pct = (1.0 - current_freq_mhz / base_freq_mhz) * 100.0;
                if (throttle_pct > 5.0)
                {
                    std::cout << ", " << colors::yellow() << std::fixed << std::setprecision(0) << throttle_pct
                              << "% throttled" << colors::reset();
                }
                else if (throttle_pct < -5.0)
                {
                    // Turbo boost - running above base frequency
                    std::cout << ", " << colors::green() << "turbo" << colors::reset();
                }
                std::cout << ")";
            }
            std::cout << "\n";
        }

        if (cpu_temp_celsius > 0)
        {
            std::cout << "  CPU Temperature: " << static_cast<int>(cpu_temp_celsius) << " C";
            if (cpu_temp_celsius > 80)
            {
                std::cout << " " << colors::red() << "(HOT)" << colors::reset();
            }
            else if (cpu_temp_celsius > 70)
            {
                std::cout << " " << colors::yellow() << "(warm)" << colors::reset();
            }
            std::cout << "\n";
        }

        std::cout << "  OS: " << os_info << "\n";
        std::cout << "  Timer: " << HighResolutionTimer::timer_name() << " (resolution: " << std::fixed
                  << std::setprecision(2) << HighResolutionTimer::resolution_ms() * 1e6 << " ns)\n";
        std::cout << "  Timestamp: " << timestamp << "\n";
        std::cout << "\n";
    }

    /**
     * @brief Get a one-line summary suitable for benchmark headers
     */
    [[nodiscard]] std::string one_line_summary() const
    {
        std::ostringstream oss;
        oss << cpu_model;
        if (physical_cores > 0)
        {
            oss << " (" << physical_cores << "C/" << logical_cores << "T";
            if (base_freq_mhz > 0)
            {
                oss << " @ " << static_cast<int>(base_freq_mhz) << " MHz";
            }
            oss << ")";
        }
        return oss.str();
    }

    /**
     * @brief Get throttle percentage (0 = no throttling, 50 = running at half speed)
     * @return Throttle percentage, or -1 if current frequency unavailable
     */
    [[nodiscard]] double throttle_percentage() const
    {
        if (current_freq_mhz <= 0 || base_freq_mhz <= 0)
        {
            return -1.0; // Unknown
        }
        return (1.0 - current_freq_mhz / base_freq_mhz) * 100.0;
    }

    /**
     * @brief Check if CPU appears to be thermally throttled (>10% reduction)
     */
    [[nodiscard]] bool is_throttled() const
    {
        double pct = throttle_percentage();
        return pct > 10.0;
    }

private:
    static std::string get_timestamp()
    {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::tm tm_buf{};

#if defined(_WIN32) || defined(_WIN64)
        localtime_s(&tm_buf, &time);
#else
        localtime_r(&time, &tm_buf);
#endif

        std::ostringstream oss;
        oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }

#if defined(_WIN32) || defined(_WIN64)
    // Windows implementations

    static std::string get_cpu_model_windows()
    {
        char buffer[256] = {0};
        DWORD size = sizeof(buffer);
        HKEY hKey;

        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                          "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                          0,
                          KEY_READ,
                          &hKey) == ERROR_SUCCESS)
        {
            RegQueryValueExA(hKey, "ProcessorNameString", nullptr, nullptr, reinterpret_cast<LPBYTE>(buffer), &size);
            RegCloseKey(hKey);
        }

        // Trim whitespace
        std::string result(buffer);
        size_t start = result.find_first_not_of(" \t");
        size_t end = result.find_last_not_of(" \t");
        if (start != std::string::npos && end != std::string::npos)
        {
            result = result.substr(start, end - start + 1);
        }

        return result.empty() ? "Unknown CPU" : result;
    }

    static double get_cpu_freq_windows()
    {
        DWORD freq = 0;
        DWORD size = sizeof(DWORD);
        HKEY hKey;

        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                          "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                          0,
                          KEY_READ,
                          &hKey) == ERROR_SUCCESS)
        {
            RegQueryValueExA(hKey, "~MHz", nullptr, nullptr, reinterpret_cast<LPBYTE>(&freq), &size);
            RegCloseKey(hKey);
        }

        return static_cast<double>(freq);
    }

    /**
     * @brief Get current CPU frequency on Windows using PDH
     *
     * This function is only available when FATP_ENABLE_PDH_STATS is defined.
     * It uses PDH (Performance Data Helper) to get actual CPU frequency
     * including thermal throttling and turbo boost.
     *
     * LIMITATIONS:
     * - English Windows only (hardcoded counter strings)
     * - Requires pdh.dll (present on all modern Windows)
     *
     * @param base_freq_mhz Base CPU frequency for percentage calculation
     * @return Current frequency in MHz, or 0 if unavailable
     */
    static double get_current_freq_windows([[maybe_unused]] double base_freq_mhz)
    {
#if defined(FATP_ENABLE_PDH_STATS)
        // Use static monitor instance for efficiency (created once)
        static PdhCpuMonitor monitor;
        return monitor.get_current_freq_mhz(base_freq_mhz);
#else
        return 0.0; // Feature not enabled
#endif
    }

    static int get_physical_cores_windows()
    {
        DWORD length = 0;
        GetLogicalProcessorInformation(nullptr, &length);

        if (length == 0)
        {
            return 0;
        }

        std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> buffer(length / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION));

        if (!GetLogicalProcessorInformation(buffer.data(), &length))
        {
            return 0;
        }

        int physical_cores = 0;
        for (const auto& info : buffer)
        {
            if (info.Relationship == RelationProcessorCore)
            {
                ++physical_cores;
            }
        }

        return physical_cores;
    }

    static std::string get_os_info_windows()
    {
        std::ostringstream oss;
        oss << "Windows";

        // Try to get version from registry (works on Windows 10+)
        HKEY hKey;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ, &hKey) ==
            ERROR_SUCCESS)
        {
            char product[256] = {0};
            char build[64] = {0};
            DWORD size = sizeof(product);

            if (RegQueryValueExA(hKey, "ProductName", nullptr, nullptr, reinterpret_cast<LPBYTE>(product), &size) ==
                ERROR_SUCCESS)
            {
                oss.str("");
                oss << product;
            }

            size = sizeof(build);
            if (RegQueryValueExA(hKey, "CurrentBuild", nullptr, nullptr, reinterpret_cast<LPBYTE>(build), &size) ==
                ERROR_SUCCESS)
            {
                oss << " (Build " << build << ")";
            }

            RegCloseKey(hKey);
        }

        // Add architecture
#if defined(_M_X64) || defined(__x86_64__)
        oss << " x64";
#elif defined(_M_IX86) || defined(__i386__)
        oss << " x86";
#elif defined(_M_ARM64) || defined(__aarch64__)
        oss << " ARM64";
#endif

        return oss.str();
    }

#else
    // Linux implementations

    static std::string get_cpu_model_linux()
    {
        std::ifstream cpuinfo("/proc/cpuinfo");
        std::string line;

        while (std::getline(cpuinfo, line))
        {
            if (line.find("model name") != std::string::npos)
            {
                size_t pos = line.find(':');
                if (pos != std::string::npos)
                {
                    std::string model = line.substr(pos + 1);
                    // Trim whitespace
                    size_t start = model.find_first_not_of(" \t");
                    size_t end = model.find_last_not_of(" \t");
                    if (start != std::string::npos && end != std::string::npos)
                    {
                        return model.substr(start, end - start + 1);
                    }
                }
            }
        }

        return "Unknown CPU";
    }

    static double get_cpu_freq_linux()
    {
        // Try to get base/max frequency from cpufreq
        std::ifstream freq_file("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq");
        if (freq_file)
        {
            double khz;
            freq_file >> khz;
            return khz / 1000.0; // Convert to MHz
        }

        // Fallback: parse from /proc/cpuinfo
        std::ifstream cpuinfo("/proc/cpuinfo");
        std::string line;

        while (std::getline(cpuinfo, line))
        {
            if (line.find("cpu MHz") != std::string::npos)
            {
                size_t pos = line.find(':');
                if (pos != std::string::npos)
                {
                    return std::stod(line.substr(pos + 1));
                }
            }
        }

        return 0.0;
    }

    static double get_current_freq_linux()
    {
        std::ifstream freq_file("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");
        if (freq_file)
        {
            double khz;
            freq_file >> khz;
            return khz / 1000.0; // Convert to MHz
        }
        return 0.0;
    }

    static int get_physical_cores_linux()
    {
        std::ifstream cpuinfo("/proc/cpuinfo");
        std::string line;
        int cores = 0;

        // Count unique core ids
        std::vector<int64_t> core_ids;
        int current_physical_id = -1;
        int current_core_id = -1;

        while (std::getline(cpuinfo, line))
        {
            if (line.find("physical id") != std::string::npos)
            {
                size_t pos = line.find(':');
                if (pos != std::string::npos)
                {
                    current_physical_id = std::stoi(line.substr(pos + 1));
                }
            }
            else if (line.find("core id") != std::string::npos)
            {
                size_t pos = line.find(':');
                if (pos != std::string::npos)
                {
                    current_core_id = std::stoi(line.substr(pos + 1));
                    // Create unique ID from physical_id and core_id
                    // Use 64-bit to avoid overflow with large physical_id values
                    int64_t unique_id = static_cast<int64_t>(current_physical_id) * 100000LL + current_core_id;
                    if (std::find(core_ids.begin(), core_ids.end(), unique_id) == core_ids.end())
                    {
                        core_ids.push_back(unique_id);
                    }
                }
            }
            else if (line.find("cpu cores") != std::string::npos && cores == 0)
            {
                // Fallback: use "cpu cores" field
                size_t pos = line.find(':');
                if (pos != std::string::npos)
                {
                    cores = std::stoi(line.substr(pos + 1));
                }
            }
        }

        return core_ids.empty() ? cores : static_cast<int>(core_ids.size());
    }

    static double get_cpu_temp_linux()
    {
        // Try common hwmon paths
        const char* paths[] = {"/sys/class/thermal/thermal_zone0/temp",
                               "/sys/class/hwmon/hwmon0/temp1_input",
                               "/sys/class/hwmon/hwmon1/temp1_input",
                               "/sys/class/hwmon/hwmon2/temp1_input"};

        for (const char* path : paths)
        {
            std::ifstream temp_file(path);
            if (temp_file)
            {
                double millidegrees;
                temp_file >> millidegrees;
                return millidegrees / 1000.0; // Convert to Celsius
            }
        }

        return -1.0; // Not available
    }

    static std::string get_os_info_linux()
    {
        std::ostringstream oss;

        // Try to read from /etc/os-release
        std::ifstream os_release("/etc/os-release");
        std::string line;
        std::string pretty_name;

        while (std::getline(os_release, line))
        {
            if (line.find("PRETTY_NAME=") == 0)
            {
                pretty_name = line.substr(12);
                // Remove quotes
                if (!pretty_name.empty() && pretty_name.front() == '"')
                {
                    pretty_name = pretty_name.substr(1);
                }
                if (!pretty_name.empty() && pretty_name.back() == '"')
                {
                    pretty_name.pop_back();
                }
                break;
            }
        }

        if (!pretty_name.empty())
        {
            oss << pretty_name;
        }
        else
        {
            oss << "Linux";
        }

        // Add architecture
#if defined(__x86_64__)
        oss << " x86_64";
#elif defined(__i386__)
        oss << " i386";
#elif defined(__aarch64__)
        oss << " aarch64";
#elif defined(__arm__)
        oss << " arm";
#endif

        return oss.str();
    }
#endif
};

/**
 * @brief Calibrates iteration count to ensure measurement precision
 *
 * Runs a quick calibration to estimate operation time, then calculates
 * the minimum iterations needed for reliable measurements.
 *
 * @tparam Func Function type to measure
 * @param func The function to calibrate
 * @param min_total_ms Minimum total measurement time for precision (default: 0.1ms)
 * @param max_iterations Maximum iterations to suggest (default: 100,000,000)
 * @return Suggested iteration count
 */
template <typename Func>
[[nodiscard]] size_t calibrate_iterations(Func func, double min_total_ms = 0.1, size_t max_iterations = 100000000)
{
    constexpr size_t calibration_iterations = 1000;

    for (size_t i = 0; i < 100; ++i)
    {
        func();
    }

    auto start = HighResolutionTimer::now();
    for (size_t i = 0; i < calibration_iterations; ++i)
    {
        func();
    }
    auto end = HighResolutionTimer::now();

    double calibration_ms = HighResolutionTimer::elapsed_ms(start, end);

    if (calibration_ms <= 0.0)
    {
        return max_iterations;
    }

    double time_per_op_ms = calibration_ms / static_cast<double>(calibration_iterations);

    if (time_per_op_ms <= 0.0)
    {
        return max_iterations;
    }

    size_t needed = static_cast<size_t>(std::ceil(min_total_ms / time_per_op_ms));

    needed = std::max(needed, static_cast<size_t>(1000));
    needed = std::min(needed, max_iterations);

    return needed;
}

/**
 * @brief Performance measurement helper with warm-up and optional auto-calibration
 *
 * Runs the given function N times and returns the average duration per call in milliseconds.
 * Includes warm-up iterations to prime caches and checks timer resolution.
 *
 * When iterations is set to 0 (default), auto-calibrates to determine optimal iteration count.
 * This ensures sufficient measurement precision for both fast and slow operations.
 *
 * Uses QueryPerformanceCounter on Windows for accurate sub-microsecond measurements.
 * Uses std::chrono::high_resolution_clock on other platforms.
 *
 * @tparam Func Function type to measure
 * @param func The function to measure (should be fast, < 1ms per call)
 * @param iterations Number of iterations to run (0 = auto-calibrate, default)
 * @param warmup_iterations Number of warm-up iterations (default: 1000)
 * @return Average time per call in milliseconds
 */
template <typename Func>
[[nodiscard]] double measure_perf(Func func, size_t iterations = 0, size_t warmup_iterations = 1000)
{
    double resolution_ms = HighResolutionTimer::resolution_ms();
    double min_total_ms = resolution_ms * 1000.0;

    if (iterations == 0)
    {
        iterations = calibrate_iterations(func, min_total_ms);
    }

    for (size_t i = 0; i < warmup_iterations; ++i)
    {
        func();
    }

    auto start = HighResolutionTimer::now();
    for (size_t i = 0; i < iterations; ++i)
    {
        func();
    }
    auto end = HighResolutionTimer::now();

    double total_ms = HighResolutionTimer::elapsed_ms(start, end);
    double time_ms = total_ms / static_cast<double>(iterations);

    if (total_ms < min_total_ms)
    {
        *get_test_config().error << colors::yellow() << "Warning: Total measurement (" << total_ms
                                 << " ms) may have insufficient precision. Consider increasing iterations."
                                 << colors::reset() << std::endl;
    }

    return time_ms;
}

/**
 * @brief Advanced performance measurement with enhanced statistics
 *
 * Runs multiple batches and collects comprehensive statistics including
 * percentiles and outlier detection.
 *
 * When iterations is set to 0 (default), auto-calibrates to determine optimal iteration count.
 * This ensures sufficient measurement precision for both fast and slow operations.
 *
 * Uses QueryPerformanceCounter on Windows for accurate sub-microsecond measurements.
 * Uses std::chrono::high_resolution_clock on other platforms.
 *
 * @tparam Func Function type to measure
 * @param func The function to measure
 * @param iterations Number of iterations per batch (0 = auto-calibrate, default)
 * @param batches Number of batches to run (default: 20, minimum: 5)
 * @return BenchmarkStats with detailed statistics
 */
template <typename Func>
[[nodiscard]] BenchmarkStats measure_perf_stats(Func func, size_t iterations = 0, size_t batches = 20)
{
    if (batches < 5)
    {
        batches = 5;
    }

    double resolution_ms = HighResolutionTimer::resolution_ms();
    double min_total_ms = resolution_ms * 1000.0;

    if (iterations == 0)
    {
        iterations = calibrate_iterations(func, min_total_ms);
    }

    std::vector<double> times;
    times.reserve(batches);

    for (size_t i = 0; i < 1000; ++i)
    {
        func();
    }

    for (size_t b = 0; b < batches; ++b)
    {
        auto start = HighResolutionTimer::now();
        for (size_t i = 0; i < iterations; ++i)
        {
            func();
        }
        auto end = HighResolutionTimer::now();
        double batch_ms = HighResolutionTimer::elapsed_ms(start, end) / static_cast<double>(iterations);
        times.push_back(batch_ms);
    }

    std::sort(times.begin(), times.end());

    double min = times.front();
    double max = times.back();
    double sum = 0.0;
    for (double t : times)
    {
        sum += t;
    }
    double mean = sum / static_cast<double>(times.size());

    // Clamp mean to [min, max] to handle floating-point precision issues
    // with extremely small measurements where accumulation errors can
    // cause mean to fall slightly outside the valid range
    mean = std::max(min, std::min(mean, max));

    double median =
        times.size() % 2 == 0 ? (times[times.size() / 2 - 1] + times[times.size() / 2]) / 2.0 : times[times.size() / 2];

    if (times.empty())
    {
        return BenchmarkStats{0, 0, 0, 0, 0, 0, 0, 0, iterations};
    }

    size_t p95_idx = static_cast<size_t>(std::floor(static_cast<double>(times.size() - 1) * 0.95));
    double p95 = times[p95_idx];

    size_t p99_idx = static_cast<size_t>(std::floor(static_cast<double>(times.size() - 1) * 0.99));
    double p99 = times[p99_idx];

    double variance = 0.0;
    if (times.size() > 1)
    {
        for (double t : times)
        {
            double diff = t - mean;
            variance += diff * diff;
        }
        variance /= static_cast<double>(times.size() - 1);
    }
    double stddev = std::sqrt(variance);

    size_t outliers = 0;
    double outlier_threshold = mean + 2.0 * stddev;
    for (double t : times)
    {
        if (t > outlier_threshold)
        {
            ++outliers;
        }
    }

    return BenchmarkStats{min, max, mean, median, stddev, p95, p99, outliers, iterations};
}

/**
 * @brief Formats time in appropriate units (ns, us, ms, s)
 *
 * @param time_ms Time in milliseconds
 * @return Formatted string with appropriate unit
 */
[[nodiscard]] inline std::string format_time(double time_ms)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3);

    if (time_ms < 0.001)
    {
        oss << (time_ms * 1000000.0) << " ns";
    }
    else if (time_ms < 1.0)
    {
        oss << (time_ms * 1000.0) << " us";
    }
    else if (time_ms < 1000.0)
    {
        oss << time_ms << " ms";
    }
    else
    {
        oss << (time_ms / 1000.0) << " s";
    }

    return oss.str();
}

/**
 * @brief Print benchmark context (timestamp and CPU frequency)
 *
 * Prints a compact line with current timestamp and CPU frequency information
 * suitable for benchmark output. Shows throttle/turbo status when available.
 *
 * @param out Output stream to write to
 */
inline void print_benchmark_context(std::ostream& out)
{
    auto info = SystemInfo::capture();

    out << "  " << colors::blue() << "[" << info.timestamp << "]";

    if (info.base_freq_mhz > 0)
    {
        out << " CPU: ";
        if (info.current_freq_mhz > 0)
        {
            out << static_cast<int>(info.current_freq_mhz) << " MHz";
            double throttle_pct = info.throttle_percentage();
            if (throttle_pct > 5.0)
            {
                out << " (" << colors::yellow() << std::fixed << std::setprecision(0) << throttle_pct << "% throttled"
                    << colors::reset() << colors::blue() << ")";
            }
            else if (throttle_pct < -5.0)
            {
                out << " (" << colors::green() << "turbo" << colors::reset() << colors::blue() << ")";
            }
        }
        else
        {
            out << static_cast<int>(info.base_freq_mhz) << " MHz (base)";
        }
    }

    out << colors::reset() << "\n";
}

/**
 * @brief Measures performance and prints results in a formatted way
 *
 * Includes timestamp and CPU frequency information for benchmark context.
 * When iterations is 0 (default), auto-calibrates for optimal precision.
 *
 * @tparam Func Function type to measure
 * @param name Description of what's being measured
 * @param func The function to measure
 * @param iterations Number of iterations (0 = auto-calibrate)
 */
template <typename Func>
void benchmark(const char* name, Func func, size_t iterations = 0)
{
    double resolution_ms = HighResolutionTimer::resolution_ms();
    double min_total_ms = resolution_ms * 1000.0;

    if (iterations == 0)
    {
        iterations = calibrate_iterations(func, min_total_ms);
    }

    double avg_ms = measure_perf(func, iterations);

    auto& out = *get_test_config().output;
    out << colors::cyan() << name << colors::reset() << ":\n";
    print_benchmark_context(out);
    out << "  Average time per operation: " << colors::bold() << format_time(avg_ms) << colors::reset() << "\n";
    out << "  Total for " << iterations << " iterations: " << format_time(avg_ms * static_cast<double>(iterations))
        << "\n";
}

/**
 * @brief Detailed benchmark with enhanced statistics
 *
 * Includes percentiles, outlier detection, and optional baseline comparison.
 * When iterations is 0 (default), auto-calibrates for optimal precision.
 *
 * @tparam Func Function type to measure
 * @param name Description of what's being measured
 * @param func The function to measure
 * @param iterations Number of iterations per batch (0 = auto-calibrate)
 * @param batches Number of batches to run
 * @param save_baseline If true, saves this run as baseline for future comparisons
 */
template <typename Func>
void benchmark_detailed(const char* name,
                        Func func,
                        size_t iterations = 0,
                        size_t batches = 20,
                        bool save_baseline = false)
{
    double resolution_ms = HighResolutionTimer::resolution_ms();
    double min_total_ms = resolution_ms * 1000.0;

    if (iterations == 0)
    {
        iterations = calibrate_iterations(func, min_total_ms);
    }

    auto stats = measure_perf_stats(func, iterations, batches);

    auto& out = *get_test_config().output;
    out << colors::cyan() << name << colors::reset() << " (" << batches << " batches):\n";
    print_benchmark_context(out);
    out << "  Mean:   " << colors::bold() << format_time(stats.mean_ms) << colors::reset() << "\n";
    out << "  Median: " << format_time(stats.median_ms) << "\n";
    out << "  Min:    " << format_time(stats.min_ms) << "\n";
    out << "  Max:    " << format_time(stats.max_ms) << "\n";
    out << "  P95:    " << format_time(stats.p95_ms) << "\n";
    out << "  P99:    " << format_time(stats.p99_ms) << "\n";
    out << "  StdDev: " << format_time(stats.stddev_ms) << "\n";

    if (stats.outliers > 0)
    {
        out << "  " << colors::yellow() << "Outliers: " << stats.outliers << " (" << std::fixed << std::setprecision(1)
            << (100.0 * stats.outliers / batches) << "%)" << colors::reset() << "\n";
    }

    auto& baseline = get_benchmark_baseline();
    if (baseline.has_baseline(name))
    {
        double change_pct = baseline.compare(name, stats);
        if (std::abs(change_pct) > 5.0)
        {
            const char* color = change_pct > 0 ? colors::red() : colors::green();
            const char* symbol = change_pct > 0 ? "^" : "v";
            out << "  " << color << "Baseline: " << symbol << " " << std::fixed << std::setprecision(1)
                << std::abs(change_pct) << "%" << colors::reset() << "\n";
        }
        else
        {
            out << "  Baseline: ~= (within 5%)\n";
        }
    }

    if (save_baseline)
    {
        baseline.save(name, stats);
        out << "  " << colors::blue() << "(Saved as baseline)" << colors::reset() << "\n";
    }

    out << "  Total:  " << format_time(stats.mean_ms * iterations) << " per batch\n";
}

/**
 * @brief Compare two functions and show speedup/slowdown
 *
 * Measures both functions and calculates the speedup ratio. If function1 takes
 * less time than function2, then function1 is faster by a factor of time2/time1.
 *
 * Example: If func1 takes 100ms and func2 takes 200ms, func1 is 2.0x faster.
 *
 * The speedup calculation uses the ratio of execution times:
 * - Speedup = slower_time / faster_time
 * - A speedup of 2.0x means the faster function takes half the time
 * - Functions with very similar times are reported as having the same performance
 *
 * When iterations is 0 (default), auto-calibrates for optimal precision.
 * Note: Both functions are calibrated independently to ensure fair comparison.
 *
 * @tparam Func1 First function type
 * @tparam Func2 Second function type
 * @param name1 Description of first function
 * @param func1 First function
 * @param name2 Description of second function
 * @param func2 Second function
 * @param iterations Number of iterations (0 = auto-calibrate each function independently)
 */
template <typename Func1, typename Func2>
void benchmark_compare(const char* name1, Func1 func1, const char* name2, Func2 func2, size_t iterations = 0)
{
    auto& out = *get_test_config().output;

    out << colors::cyan() << "Comparing: " << colors::reset() << name1 << " vs " << name2 << "\n";
    print_benchmark_context(out);

    double time1 = measure_perf(func1, iterations);
    double time2 = measure_perf(func2, iterations);

    out << "  " << name1 << ": " << format_time(time1) << "\n";
    out << "  " << name2 << ": " << format_time(time2) << "\n";

    if (time1 < time2)
    {
        double speedup = time2 / time1;
        out << "  " << colors::green() << name1 << " is " << std::fixed << std::setprecision(2) << speedup << "x faster"
            << colors::reset() << "\n";
    }
    else if (time2 < time1)
    {
        double speedup = time1 / time2;
        out << "  " << colors::green() << name2 << " is " << std::fixed << std::setprecision(2) << speedup << "x faster"
            << colors::reset() << "\n";
    }
    else
    {
        out << "  " << colors::yellow() << "Same performance" << colors::reset() << "\n";
    }
}

// ============================================================================
// Test Fixtures
// ============================================================================

/**
 * @brief Base class for test fixtures
 *
 * Test fixtures provide setup and teardown hooks that run before and after
 * each test. This ensures proper resource initialization and cleanup even
 * if tests fail or throw exceptions.
 *
 * Usage:
 *   struct MyFixture : public TestFixture
 *   {
 *       Database* db;
 *
 *       void SetUp() override
 *       {
 *           db = new Database();
 *           db->connect();
 *       }
 *
 *       void TearDown() override
 *       {
 *           db->disconnect();
 *           delete db;
 *       }
 *   };
 */
struct TestFixture
{
    virtual void SetUp()
    {
    }
    virtual void TearDown()
    {
    }
    virtual ~TestFixture() = default;
};

// ============================================================================
// Test Runner
// ============================================================================

/**
 * @brief Test case result
 */
struct TestResult
{
    std::string name;
    bool passed;
    double duration_ms;
};

/**
 * @brief Subtest result tracking
 */
struct SubtestResult
{
    std::string name;
    bool passed;
    std::string failure_message;
};

/**
 * @brief Global subtest tracker (thread-local for safety)
 */
class SubtestTracker
{
private:
    std::vector<SubtestResult> mSubtests;
    bool current_subtest_passed_;
    std::string current_subtest_name_;
    bool inside_subtest_;

public:
    SubtestTracker()
        : current_subtest_passed_(true)
        , inside_subtest_(false)
    {
    }

    void begin_subtest(const std::string& name)
    {
        inside_subtest_ = true;
        current_subtest_name_ = name;
        current_subtest_passed_ = true;
    }

    void fail_current_subtest(const std::string& message)
    {
        current_subtest_passed_ = false;
        mSubtests.push_back({current_subtest_name_, false, message});
    }

    void end_subtest()
    {
        inside_subtest_ = false;
        if (current_subtest_passed_)
        {
            mSubtests.push_back({current_subtest_name_, true, ""});
        }
        current_subtest_name_.clear();
    }

    [[nodiscard]] const std::vector<SubtestResult>& get_results() const
    {
        return mSubtests;
    }

    void clear()
    {
        mSubtests.clear();
        current_subtest_passed_ = true;
        current_subtest_name_.clear();
        inside_subtest_ = false;
    }

    [[nodiscard]] bool all_passed() const
    {
        for (const auto& result : mSubtests)
        {
            if (!result.passed)
            {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool is_inside_subtest() const
    {
        return inside_subtest_;
    }
};

inline SubtestTracker& get_subtest_tracker()
{
    static thread_local SubtestTracker tracker;
    return tracker;
}


/**
 * @brief Simple test runner for organizing tests
 */
class TestRunner
{
private:
    std::vector<TestResult> mResults;
    std::string filter_pattern_;

public:
    /**
     * @brief Set filter pattern for test selection
     *
     * Supports wildcards: * (any characters) and ? (single character)
     *
     * Examples:
     *   set_filter("*Math*")     - Run tests with "Math" in the name
     *   set_filter("test_add*")  - Run tests starting with "test_add"
     *   set_filter("*_slow")     - Run tests ending with "_slow"
     */
    void set_filter(const std::string& pattern)
    {
        filter_pattern_ = pattern;
    }

    /**
     * @brief Check if test name matches filter
     */
    [[nodiscard]] bool matches_filter(const char* name) const noexcept
    {
        if (filter_pattern_.empty())
        {
            return true;
        }
        return string_utils::matches_pattern(name, filter_pattern_);
    }

private:
    /**
     * @brief Print timestamp and CPU frequency info before running a test
     *
     * Only prints when verbose mode is enabled. Shows current time and
     * CPU frequency (with throttle/turbo status if available via PDH).
     */
    void print_test_context(std::ostream& out) const
    {
        if (!get_test_config().verbose)
        {
            return;
        }

        // Get timestamp
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::tm tm_buf{};
#if defined(_WIN32) || defined(_WIN64)
        localtime_s(&tm_buf, &time);
#else
        localtime_r(&time, &tm_buf);
#endif

        out << colors::cyan() << "[" << std::put_time(&tm_buf, "%H:%M:%S") << "] " << colors::reset();

        // Get current frequency if PDH is available
#if defined(FATP_ENABLE_PDH_STATS) && (defined(_WIN32) || defined(_WIN64))
        static PdhCpuMonitor monitor;
        if (monitor.is_available())
        {
            double pct = monitor.get_frequency_percentage();
            if (pct > 0)
            {
                // Get base frequency from registry (cached)
                static double base_freq = []() {
                    DWORD freq = 0;
                    DWORD size = sizeof(DWORD);
                    HKEY hKey;
                    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                                      "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                                      0,
                                      KEY_READ,
                                      &hKey) == ERROR_SUCCESS)
                    {
                        RegQueryValueExA(hKey, "~MHz", nullptr, nullptr, reinterpret_cast<LPBYTE>(&freq), &size);
                        RegCloseKey(hKey);
                    }
                    return static_cast<double>(freq);
                }();

                double current_freq = base_freq * (pct / 100.0);
                out << colors::cyan() << "CPU: " << static_cast<int>(current_freq) << " MHz";

                double throttle_pct = 100.0 - pct;
                if (throttle_pct > 5.0)
                {
                    out << " (" << colors::yellow() << static_cast<int>(throttle_pct) << "% throttled"
                        << colors::reset() << colors::cyan() << ")";
                }
                else if (throttle_pct < -5.0)
                {
                    out << " (" << colors::green() << "turbo" << colors::reset() << colors::cyan() << ")";
                }
                out << colors::reset() << " ";
            }
        }
#endif
    }

public:
    /**
     * @brief Run a test and record result
     *
     * @param name Test name
     * @param test_func Test function that returns bool
     * @return True if test passed
     */
    template <typename Func>
    bool run_test(const char* name, Func test_func)
    {
        if (!matches_filter(name))
        {
            return true;
        }

        get_subtest_tracker().clear();

        auto& out = *get_test_config().output;

        if (get_test_config().verbose)
        {
            print_test_context(out);
            out << colors::blue() << "Running: " << colors::reset() << name << " ... ";
            out.flush();
        }

        auto start = std::chrono::high_resolution_clock::now();
        bool passed = test_func();
        auto end = std::chrono::high_resolution_clock::now();
        double duration = std::chrono::duration<double, std::milli>(end - start).count();

        mResults.push_back({name, passed, duration});

        get_subtest_tracker().clear();

        if (get_test_config().verbose)
        {
            if (passed)
            {
                out << colors::green() << colors::bold() << "PASSED" << colors::reset() << " (" << duration << " ms)\n";
            }
            else
            {
                out << colors::red() << colors::bold() << "FAILED" << colors::reset() << "\n";
            }
        }

        return passed;
    }

    /**
     * @brief Run a test with a fixture
     *
     * Guarantees SetUp() and TearDown() are called even if test fails
     *
     * @tparam FixtureType Type of fixture (must derive from TestFixture)
     * @param name Test name
     * @param test_func Test function that takes fixture reference and returns bool
     * @return True if test passed
     */
    template <typename FixtureType, typename Func>
    bool run_test_with_fixture(const char* name, Func test_func)
    {
        if (!matches_filter(name))
        {
            return true;
        }

        auto& out = *get_test_config().output;

        if (get_test_config().verbose)
        {
            print_test_context(out);
            out << colors::blue() << "Running: " << colors::reset() << name << " ... ";
            out.flush();
        }

        bool passed = false;
        double duration = 0.0;
        FixtureType fixture;

        try
        {
            fixture.SetUp();

            auto start = std::chrono::high_resolution_clock::now();
            passed = test_func(fixture);
            auto end = std::chrono::high_resolution_clock::now();
            duration = std::chrono::duration<double, std::milli>(end - start).count();

            fixture.TearDown();
        }
        catch (const std::exception& e)
        {
            std::string teardown_error;
            try
            {
                fixture.TearDown();
            }
            catch (const std::exception& td_ex)
            {
                teardown_error = td_ex.what();
            }
            catch (...)
            {
                teardown_error = "(unknown exception)";
            }

            out << colors::red() << colors::bold() << "EXCEPTION: " << colors::reset() << colors::red() << e.what();
            if (!teardown_error.empty())
            {
                out << "\n  " << colors::yellow() << "TearDown also failed: " << teardown_error << colors::reset();
            }
            out << colors::reset() << std::endl;
            passed = false;
        }
        catch (...)
        {
            std::string teardown_error;
            try
            {
                fixture.TearDown();
            }
            catch (const std::exception& td_ex)
            {
                teardown_error = td_ex.what();
            }
            catch (...)
            {
                teardown_error = "(unknown exception)";
            }

            out << colors::red() << colors::bold() << "UNKNOWN EXCEPTION";
            if (!teardown_error.empty())
            {
                out << "\n  " << colors::yellow() << "TearDown also failed: " << teardown_error << colors::reset();
            }
            out << colors::reset() << std::endl;
            passed = false;
        }
        mResults.push_back({name, passed, duration});

        if (get_test_config().verbose)
        {
            if (passed)
            {
                out << colors::green() << colors::bold() << "PASSED" << colors::reset() << " (" << duration << " ms)\n";
            }
            else
            {
                out << colors::red() << colors::bold() << "FAILED" << colors::reset() << "\n";
            }
        }

        return passed;
    }

    /**
     * @brief Run a test with a timeout
     *
     * Runs a test function with a maximum time limit. If the test doesn't complete
     * within the timeout period, it's marked as failed. Uses std::async for
     * portable timeout support across platforms.
     *
     * IMPORTANT LIMITATION: On timeout, the test thread continues running in the
     * background. This is a fundamental C++ limitation - there is no safe, portable
     * way to kill threads. Thread cancellation violates RAII principles and can
     * corrupt process state.
     *
     * GUIDELINES:
     * - Use timeouts to detect hanging tests, not enforce precise execution limits
     * - If a test times out, consider aborting the entire test suite
     * - Ensure tests either naturally terminate or have no dangerous side effects
     * - This is the same approach used by Google Test and other major frameworks
     *
     * @tparam Func Test function type (should return bool)
     * @param name Test name for reporting
     * @param test_func Test function to run
     * @param timeout_ms Timeout in milliseconds (default: 5000ms = 5 seconds)
     * @return True if test passed within timeout
     */
    template <typename Func>
    bool run_test_with_timeout(const char* name, Func test_func, size_t timeout_ms = 5000)
    {
        if (!matches_filter(name))
        {
            return true;
        }

        auto& out = *get_test_config().output;

        if (get_test_config().verbose)
        {
            print_test_context(out);
            out << colors::blue() << "Running (timeout " << timeout_ms << "ms): " << colors::reset() << name << " ... ";
            out.flush();
        }

        bool passed = false;
        double duration = 0.0;
        bool timed_out = false;

        auto start = std::chrono::high_resolution_clock::now();

        auto future = std::async(std::launch::async, test_func);

        if (future.wait_for(std::chrono::milliseconds(timeout_ms)) == std::future_status::timeout)
        {
            timed_out = true;
            passed = false;
            duration = static_cast<double>(timeout_ms);
        }
        else
        {
            try
            {
                passed = future.get();
                auto end = std::chrono::high_resolution_clock::now();
                duration = std::chrono::duration<double, std::milli>(end - start).count();
            }
            catch (const std::exception& e)
            {
                out << colors::red() << colors::bold() << "EXCEPTION: " << colors::reset() << colors::red() << e.what()
                    << colors::reset() << std::endl;
                passed = false;
                auto end = std::chrono::high_resolution_clock::now();
                duration = std::chrono::duration<double, std::milli>(end - start).count();
            }
            catch (...)
            {
                out << colors::red() << colors::bold() << "UNKNOWN EXCEPTION" << colors::reset() << std::endl;
                passed = false;
                auto end = std::chrono::high_resolution_clock::now();
                duration = std::chrono::duration<double, std::milli>(end - start).count();
            }
        }

        mResults.push_back({name, passed, duration});

        if (get_test_config().verbose)
        {
            if (timed_out)
            {
                out << colors::red() << colors::bold() << "TIMEOUT" << colors::reset() << " (>" << timeout_ms
                    << " ms)\n";
            }
            else if (passed)
            {
                out << colors::green() << colors::bold() << "PASSED" << colors::reset() << " (" << duration << " ms)\n";
            }
            else
            {
                out << colors::red() << colors::bold() << "FAILED" << colors::reset() << "\n";
            }
        }
        else if (timed_out)
        {
            out << colors::red() << "TIMEOUT: " << name << " (>" << timeout_ms << " ms)" << colors::reset() << "\n";
        }

        return passed;
    }

    /**
     * @brief Print summary of all test results
     *
     * @return Number of failed tests
     */
    int print_summary() const
    {
        auto& out = *get_test_config().output;

        int passed = 0;
        int failed = 0;

        for (const auto& result : mResults)
        {
            if (result.passed)
            {
                ++passed;
            }
            else
            {
                ++failed;
            }
        }

        out << "\n" << colors::bold() << "=== Test Summary ===" << colors::reset() << "\n";
        out << colors::green() << "Passed: " << passed << colors::reset() << "\n";
        if (failed > 0)
        {
            out << colors::red() << "Failed: " << failed << colors::reset() << "\n";
        }
        else
        {
            out << "Failed: " << failed << "\n";
        }

        if (!filter_pattern_.empty())
        {
            out << "Filter: \"" << filter_pattern_ << "\"\n";
        }

        out << "Total:  " << (passed + failed) << "\n";

        if (failed > 0)
        {
            out << "\nFailed tests:\n";
            for (const auto& result : mResults)
            {
                if (!result.passed)
                {
                    out << "  " << colors::red() << result.name << colors::reset() << "\n";
                }
            }
            out << "\n";
        }

        return failed;
    }

    /**
     * @brief Get all test results
     */
    [[nodiscard]] const std::vector<TestResult>& results() const
    {
        return mResults;
    }

    /**
     * @brief Clear all results and reset filter
     */
    void clear()
    {
        mResults.clear();
        filter_pattern_.clear();
    }

    /**
     * @brief Export results to JUnit XML format
     */
    bool export_to_junit_xml(const std::string& filename, const std::string& suite_name = "TestSuite") const;

    /**
     * @brief Result of repeated test execution
     */
    struct RepetitionResult
    {
        std::string name;
        size_t total_runs = 0;
        size_t passed = 0;
        size_t failed = 0;
        double pass_rate = 0.0;
        std::vector<size_t> failed_runs;
    };

    /**
     * @brief Run a test multiple times to detect flakiness
     *
     * Runs the same test repeatedly and tracks which runs pass/fail.
     * Useful for detecting non-deterministic bugs, race conditions,
     * and other intermittent failures.
     *
     * Classification:
     * - 100% pass rate: Stable
     * - 95-99% pass rate: Slightly flaky
     * - 50-94% pass rate: Very flaky
     * - <50% pass rate: Consistently failing
     *
     * @tparam Func Test function type (should return bool)
     * @param name Test name for reporting
     * @param test_func Test function to run repeatedly
     * @param repetitions Number of times to run the test (default: 100)
     * @return RepetitionResult with detailed statistics
     *
     * @example
     * auto result = runner.run_test_repeat("flaky_test", test_func, 100);
     * if (result.pass_rate < 100.0) {
     *     std::cout << "Test is flaky! Pass rate: " << result.pass_rate << "%\n";
     * }
     */
    template <typename Func>
    [[nodiscard]] RepetitionResult run_test_repeat(const char* name, Func test_func, size_t repetitions = 100)
    {
        if (!matches_filter(name))
        {
            return RepetitionResult{name, 0, 0, 0, 0.0, {}};
        }

        RepetitionResult result;
        result.name = name;
        result.total_runs = repetitions;
        result.passed = 0;
        result.failed = 0;

        auto& out = *get_test_config().output;
        print_test_context(out);
        out << colors::blue() << "Repeating: " << colors::reset() << name << " (" << repetitions << " times)\n";

        size_t progress_interval = repetitions / 10;
        if (progress_interval == 0)
        {
            progress_interval = 1;
        }

        for (size_t i = 0; i < repetitions; ++i)
        {
            bool passed = test_func();

            if (passed)
            {
                ++result.passed;
            }
            else
            {
                ++result.failed;
                result.failed_runs.push_back(i + 1);
            }

            if (get_test_config().verbose)
            {
                if (((i + 1) % progress_interval == 0) || (i + 1 == repetitions))
                {
                    out << "  Progress: " << (i + 1) << "/" << repetitions << "\r";
                    out.flush();
                }
            }
        }

        if (get_test_config().verbose)
        {
            out << "\n";
        }

        result.pass_rate = (100.0 * static_cast<double>(result.passed)) / static_cast<double>(result.total_runs);

        out << "  Results: " << colors::green() << result.passed << " passed" << colors::reset() << ", ";
        if (result.failed > 0)
        {
            out << colors::red() << result.failed << " failed" << colors::reset();
        }
        else
        {
            out << result.failed << " failed";
        }
        out << "\n";

        out << "  Pass rate: " << std::fixed << std::setprecision(1) << result.pass_rate << "%\n";

        if (result.failed > 0 && result.failed <= 10)
        {
            out << colors::red() << "  Failed on runs: ";
            for (size_t run : result.failed_runs)
            {
                out << run << " ";
            }
            out << colors::reset() << "\n";
        }
        else if (result.failed > 10)
        {
            out << colors::red() << "  Failed on " << result.failed << " runs (first 10): ";
            for (size_t i = 0; i < 10; ++i)
            {
                out << result.failed_runs[i] << " ";
            }
            out << "..." << colors::reset() << "\n";
        }

        if (result.failed == 0)
        {
            out << colors::green() << colors::bold() << "  Status: Stable" << colors::reset() << "\n";
        }
        else if (result.pass_rate >= 95.0)
        {
            out << colors::yellow() << colors::bold() << "  Status: Slightly flaky" << colors::reset() << "\n";
        }
        else if (result.pass_rate >= 50.0)
        {
            out << colors::red() << colors::bold() << "  Status: Very flaky!" << colors::reset() << "\n";
        }
        else
        {
            out << colors::red() << colors::bold() << "  Status: Consistently failing!" << colors::reset() << "\n";
        }

        out << "\n";

        return result;
    }

    /**
     * @brief Run a test repeatedly until it fails or max runs reached
     *
     * Useful for stress testing and finding rare race conditions.
     * Continues running until the test fails or max_runs is reached.
     *
     * @tparam Func Test function type (should return bool)
     * @param name Test name for reporting
     * @param test_func Test function to run
     * @param max_runs Maximum number of runs (default: 1000)
     * @return Number of runs before failure (0 if no failure found)
     *
     * @example
     * size_t failed_at = runner.run_until_failure("stress_test", test_func, 10000);
     * if (failed_at == 0) {
     *     std::cout << "Test is robust!\n";
     * } else {
     *     std::cout << "Test failed after " << failed_at << " runs\n";
     * }
     */
    template <typename Func>
    [[nodiscard]] size_t run_until_failure(const char* name, Func test_func, size_t max_runs = 1000)
    {
        if (!matches_filter(name))
        {
            return 0;
        }

        auto& out = *get_test_config().output;
        out << colors::blue() << "Running until failure: " << colors::reset() << name << " (max " << max_runs
            << " runs)\n";

        size_t progress_interval = max_runs / 10;
        if (progress_interval == 0)
        {
            progress_interval = 1;
        }

        for (size_t i = 0; i < max_runs; ++i)
        {
            bool passed = test_func();

            if (!passed)
            {
                out << "\n"
                    << colors::red() << colors::bold() << "  [FAIL] Failed on run " << (i + 1) << "/" << max_runs
                    << colors::reset() << "\n\n";
                return i + 1;
            }

            if (get_test_config().verbose)
            {
                if (((i + 1) % progress_interval == 0) || (i + 1 == max_runs))
                {
                    out << "  Completed: " << (i + 1) << "/" << max_runs << "\r";
                    out.flush();
                }
            }
        }

        out << "\n"
            << colors::green() << colors::bold() << "  [PASS] Test passed all " << max_runs << " runs!"
            << colors::reset() << "\n\n";

        return 0;
    }
};

// ============================================================================
// Parameterized Tests
// ============================================================================

/**
 * @brief Helper for running parameterized tests
 *
 * Usage:
 *   std::vector<TestCase<int, int, int>> cases = {
 *       {{2, 3, 5}, "2+3"},
 *       {{10, 20, 30}, "10+20"},
 *       {{-1, 1, 0}, "-1+1"}
 *   };
 *
 *   run_parameterized_test("addition", cases, [](const auto& tc) {
 *       FATP_ASSERT_EQ(add(std::get<0>(tc.inputs), std::get<1>(tc.inputs)),
 *                 std::get<2>(tc.inputs), tc.description);
 *       return true;
 *   });
 */
template <typename... Args>
struct TestCase
{
    std::tuple<Args...> inputs;
    std::string description;
};

template <typename TestCaseType, typename Func>
bool run_parameterized_test(const char* test_name, const std::vector<TestCaseType>& test_cases, Func test_func)
{
    auto& out = *get_test_config().output;

    if (get_test_config().verbose)
    {
        out << colors::blue() << "Running parameterized: " << colors::reset() << test_name << " (" << test_cases.size()
            << " cases)\n";
    }

    size_t passed = 0;
    size_t failed = 0;

    for (size_t i = 0; i < test_cases.size(); ++i)
    {
        const auto& tc = test_cases[i];

        if (get_test_config().verbose)
        {
            out << "  Case " << (i + 1) << "/" << test_cases.size() << " [" << tc.description << "] ... ";
            out.flush();
        }

        bool result = test_func(tc);

        if (result)
        {
            ++passed;
            if (get_test_config().verbose)
            {
                out << colors::green() << "PASSED" << colors::reset() << "\n";
            }
        }
        else
        {
            ++failed;
            if (get_test_config().verbose)
            {
                out << colors::red() << "FAILED" << colors::reset() << "\n";
            }
            else
            {
                out << colors::red() << "FAILED: " << test_name << " case " << (i + 1) << " [" << tc.description << "]"
                    << colors::reset() << "\n";
            }
        }
    }

    if (get_test_config().verbose)
    {
        out << "  Summary: " << passed << " passed, " << failed << " failed\n";
    }

    return failed == 0;
}

// ============================================================================
// JUnit XML Output
// ============================================================================

/**
 * @brief XML escape helper for JUnit output
 */
inline std::string xml_escape(const std::string& str)
{
    std::string result;
    result.reserve(str.size() * 2);

    for (char c : str)
    {
        switch (c)
        {
            case '&':
                result += "&amp;";
                break;
            case '<':
                result += "&lt;";
                break;
            case '>':
                result += "&gt;";
                break;
            case '"':
                result += "&quot;";
                break;
            case '\'':
                result += "&apos;";
                break;
            default:
                result += c;
                break;
        }
    }

    return result;
}

/**
 * @brief Export test results to JUnit XML format
 *
 * Creates a JUnit-compatible XML file that can be consumed by CI/CD systems
 * like Jenkins, GitLab CI, GitHub Actions, etc.
 *
 * @param filename Output filename (e.g., "test_results.xml")
 * @param results Vector of test results to export
 * @param suite_name Name of the test suite (default: "TestSuite")
 * @return True if file was written successfully
 *
 * @example
 * TestRunner runner;
 * // ... run tests ...
 * export_junit_xml("results.xml", runner.results(), "MyTests");
 */
inline bool export_junit_xml(const std::string& filename,
                             const std::vector<TestResult>& results,
                             const std::string& suite_name = "TestSuite")
{
    std::ofstream file(filename);
    if (!file.is_open())
    {
        std::cerr << "Failed to open file for JUnit XML output: " << filename << std::endl;
        return false;
    }

    size_t total_tests = results.size();
    size_t failures = 0;
    double total_time = 0.0;

    for (const auto& result : results)
    {
        if (!result.passed)
        {
            ++failures;
        }
        total_time += result.duration_ms;
    }

    std::time_t now = std::time(nullptr);
    char timestamp[100];
    std::tm time_info{};

#if defined(_WIN32) || defined(_WIN64)
    localtime_s(&time_info, &now);
#elif defined(__linux__) || defined(__unix__)
    localtime_r(&now, &time_info);
#else
    localtime_r(&now, &time_info);
#endif
    std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", &time_info);

    file << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    file << "<testsuites>\n";
    file << "  <testsuite name=\"" << xml_escape(suite_name) << "\" "
         << "tests=\"" << total_tests << "\" "
         << "failures=\"" << failures << "\" "
         << "errors=\"0\" "
         << "time=\"" << std::fixed << std::setprecision(3) << (total_time / 1000.0) << "\" "
         << "timestamp=\"" << timestamp << "\">\n";

    for (const auto& result : results)
    {
        file << "    <testcase name=\"" << xml_escape(result.name) << "\" "
             << "time=\"" << std::fixed << std::setprecision(3) << (result.duration_ms / 1000.0) << "\"";

        if (result.passed)
        {
            file << "/>\n";
        }
        else
        {
            file << ">\n";
            file << "      <failure message=\"Test failed\"/>\n";
            file << "    </testcase>\n";
        }
    }

    file << "  </testsuite>\n";
    file << "</testsuites>\n";

    if (!file.good())
    {
        std::cerr << "Error writing JUnit XML output: " << filename << std::endl;
        return false;
    }

    file.close();
    return file.good();
}

inline bool TestRunner::export_to_junit_xml(const std::string& filename, const std::string& suite_name) const
{
    return export_junit_xml(filename, mResults, suite_name);
}

// ============================================================================
// Convenience Macros for Test Suites
// ============================================================================

/**
 * @brief Define a test function
 *
 * Usage:
 *   FATP_TEST_CASE("my test")
 *   {
 *       FATP_ASSERT_EQ(1 + 1, 2, "Math works");
 *       return true;
 *   }
 */
#define FATP_TEST_CASE(name) bool test_##name()

/**
 * @brief Define a test function with fixture
 *
 * Usage:
 *   FATP_TEST_CASE_F(MyFixture, "my test")
 *   {
 *       FATP_ASSERT_NOT_NULLPTR(fixture.db, "Database initialized");
 *       return true;
 *   }
 */
#define FATP_TEST_CASE_F(fixture_type, name) bool test_##name(fixture_type& fixture)

/**
 * @brief Run a test case with test runner
 *
 * Usage:
 *   FATP_RUN_TEST(runner, my_test);
 */
#define FATP_RUN_TEST(runner, test_name) runner.run_test(#test_name, test_##test_name)

/**
 * @brief Run a test case from a specific namespace
 *
 * Usage:
 *   FATP_RUN_TEST_NS(runner, strongid, my_test);
 *
 * Expands to: runner.run_test("my_test", strongid::test_my_test)
 */
#define FATP_RUN_TEST_NS(runner, ns, test_name) runner.run_test(#test_name, ns::test_##test_name)

/**
 * @brief Run a test case with fixture
 *
 * Usage:
 *   FATP_RUN_TEST_F(runner, MyFixture, my_test);
 */
#define FATP_RUN_TEST_F(runner, fixture_type, test_name) \
    runner.run_test_with_fixture<fixture_type>(#test_name, test_##test_name)

/**
 * @brief Prints a formatted header for unit test sections
 *
 * This macro outputs a formatted header with the specified section name,
 * surrounded by separator lines, for organizing unit test output. Respects
 * the configured output stream from TestConfig.
 *
 * Usage example:
 * \code
 * FATP_PRINT_HEADER(CONTRACT EXCEPTION);
 * \endcode
 *
 * @param section The name of the section (e.g., CONTRACT EXCEPTION). It will be stringified.
 */
#define FATP_PRINT_HEADER(section)                                                                                \
    *fat_p::testing::get_test_config().output << "\n==========================================================\n" \
                                              << #section << " UNIT TESTS\n"                                      \
                                              << "==========================================================\n\n";

/**
 * @brief Define a subtest that continues even if it fails
 *
 * Subtests allow you to break a test into multiple parts. If one subtest fails,
 * subsequent subtests will still run. At the end, the overall test passes only
 * if all subtests passed.
 *
 * IMPORTANT: FATP_SUBTEST and FATP_END_SUBTEST must be paired. The FATP_SUBTEST macro begins
 * a try block and FATP_END_SUBTEST closes it. Forgetting FATP_END_SUBTEST will result
 * in a compilation error.
 *
 * BEHAVIOR: Exceptions thrown inside FATP_SUBTEST blocks are caught and recorded as
 * failures. Assertion macros (ASSERT_*) also work correctly within FATP_SUBTEST blocks,
 * recording failures without exiting the test function.
 *
 * Usage:
 *   bool test_complex()
 *   {
 *       FATP_SUBTEST("initialization") {
 *           FATP_ASSERT_EQ(init(), 0, "Init succeeds");
 *       }
 *       FATP_END_SUBTEST
 *
 *       FATP_SUBTEST("processing") {
 *           FATP_ASSERT_EQ(process(), 0, "Process succeeds");
 *       }
 *       FATP_END_SUBTEST
 *
 *       return fat_p::testing::get_subtest_tracker().all_passed();
 *   }
 */
#define FATP_SUBTEST(name)                                                                                          \
    if (fat_p::testing::get_subtest_tracker().get_results().empty())                                                \
    {                                                                                                               \
        *fat_p::testing::get_test_config().output << "\n";                                                          \
    }                                                                                                               \
    fat_p::testing::get_subtest_tracker().begin_subtest(name);                                                      \
    *fat_p::testing::get_test_config().output << "  " << fat_p::testing::colors::blue()                             \
                                              << "Subtest: " << fat_p::testing::colors::reset() << name << " ... "; \
    fat_p::testing::get_test_config().output->flush();                                                              \
    try

#define FATP_END_SUBTEST                                                                                           \
    catch (const std::exception& e)                                                                                \
    {                                                                                                              \
        fat_p::testing::get_subtest_tracker().fail_current_subtest(e.what());                                      \
        *fat_p::testing::get_test_config().output << fat_p::testing::colors::red() << "FAILED"                     \
                                                  << fat_p::testing::colors::reset() << " (" << e.what() << ")\n"; \
    }                                                                                                              \
    catch (...)                                                                                                    \
    {                                                                                                              \
        fat_p::testing::get_subtest_tracker().fail_current_subtest("Unknown exception");                           \
        *fat_p::testing::get_test_config().output << fat_p::testing::colors::red() << "FAILED"                     \
                                                  << fat_p::testing::colors::reset() << " (unknown exception)\n";  \
    }                                                                                                              \
    if (fat_p::testing::get_subtest_tracker().get_results().empty() ||                                             \
        fat_p::testing::get_subtest_tracker().get_results().back().passed)                                         \
    {                                                                                                              \
        *fat_p::testing::get_test_config().output << fat_p::testing::colors::green() << "PASSED"                   \
                                                  << fat_p::testing::colors::reset() << "\n";                      \
    }                                                                                                              \
    fat_p::testing::get_subtest_tracker().end_subtest();

} // namespace testing
} // namespace fat_p