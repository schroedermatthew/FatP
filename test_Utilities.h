// testing.h
#pragma once

#include <chrono>
#include <iostream>
#include <iomanip>
#include <string>
#include <cmath>
#include <vector>
#include <algorithm>

#include "FloatingPointComparison.h"

namespace cpp_utilities {
namespace testing {

// ============================================================================
// Configuration
// ============================================================================

/**
 * @brief Global configuration for testing utilities
 */
struct TestConfig {
    bool colored_output = true;      ///< Enable colored terminal output
    bool verbose = false;            ///< Enable verbose output
    bool abort_on_failure = false;   ///< Abort on first test failure
    std::ostream* output = &std::cout;  ///< Output stream for results
    std::ostream* error = &std::cerr;   ///< Output stream for errors
};

/// Global test configuration instance
inline TestConfig& get_test_config() {
    static TestConfig config;
    return config;
}

// ============================================================================
// Terminal Colors (ANSI escape codes)
// ============================================================================

namespace colors {
    inline const char* reset() { return get_test_config().colored_output ? "\033[0m" : ""; }
    inline const char* red() { return get_test_config().colored_output ? "\033[31m" : ""; }
    inline const char* green() { return get_test_config().colored_output ? "\033[32m" : ""; }
    inline const char* yellow() { return get_test_config().colored_output ? "\033[33m" : ""; }
    inline const char* blue() { return get_test_config().colored_output ? "\033[34m" : ""; }
    inline const char* magenta() { return get_test_config().colored_output ? "\033[35m" : ""; }
    inline const char* cyan() { return get_test_config().colored_output ? "\033[36m" : ""; }
    inline const char* bold() { return get_test_config().colored_output ? "\033[1m" : ""; }
}

// ============================================================================
// Assertion Macros
// ============================================================================

/**
 * @brief Simple assert macro for tests (no dependency on testing framework)
 * 
 * Usage:
 *   SIMPLE_ASSERT(condition, "error message");
 * 
 * If the condition is false, prints error message and returns false from the
 * calling function. The calling function must return bool.
 */
#define SIMPLE_ASSERT(condition, msg) \
    if (!(condition)) { \
        *cpp_utilities::testing::get_test_config().error \
            << cpp_utilities::testing::colors::red() << cpp_utilities::testing::colors::bold() \
            << "ASSERT FAILED: " << cpp_utilities::testing::colors::reset() \
            << cpp_utilities::testing::colors::red() \
            << msg << " at " << __FILE__ << ":" << __LINE__ \
            << cpp_utilities::testing::colors::reset() << std::endl; \
        if (cpp_utilities::testing::get_test_config().abort_on_failure) { \
            std::abort(); \
        } \
        return false; \
    }

/**
 * @brief Assert with custom failure handler
 * 
 * Usage:
 *   ASSERT_WITH_HANDLER(x == 42, "x should be 42", {
 *       cleanup_resources();
 *   });
 */
#define ASSERT_WITH_HANDLER(condition, msg, handler) \
    if (!(condition)) { \
        *cpp_utilities::testing::get_test_config().error \
            << cpp_utilities::testing::colors::red() << cpp_utilities::testing::colors::bold() \
            << "ASSERT FAILED: " << cpp_utilities::testing::colors::reset() \
            << cpp_utilities::testing::colors::red() \
            << msg << " at " << __FILE__ << ":" << __LINE__ \
            << cpp_utilities::testing::colors::reset() << std::endl; \
        handler; \
        return false; \
    }

/**
 * @brief Assert with equality comparison, showing actual vs expected
 */
#define ASSERT_EQ(actual, expected, msg) \
    if (!((actual) == (expected))) { \
        *cpp_utilities::testing::get_test_config().error \
            << cpp_utilities::testing::colors::red() << cpp_utilities::testing::colors::bold() \
            << "ASSERT_EQ FAILED: " << cpp_utilities::testing::colors::reset() \
            << cpp_utilities::testing::colors::red() << msg \
            << "\n  Expected: " << (expected) \
            << "\n  Actual:   " << (actual) \
            << "\n  at " << __FILE__ << ":" << __LINE__ \
            << cpp_utilities::testing::colors::reset() << std::endl; \
        if (cpp_utilities::testing::get_test_config().abort_on_failure) { \
            std::abort(); \
        } \
        return false; \
    }

/**
 * @brief Assert with inequality comparison
 */
#define ASSERT_NE(actual, expected, msg) \
    if ((actual) == (expected)) { \
        *cpp_utilities::testing::get_test_config().error \
            << cpp_utilities::testing::colors::red() << cpp_utilities::testing::colors::bold() \
            << "ASSERT_NE FAILED: " << cpp_utilities::testing::colors::reset() \
            << cpp_utilities::testing::colors::red() << msg \
            << "\n  Should not equal: " << (expected) \
            << "\n  at " << __FILE__ << ":" << __LINE__ \
            << cpp_utilities::testing::colors::reset() << std::endl; \
        return false; \
    }

/**
 * @brief Assert true
 */
#define ASSERT_TRUE(condition, msg) \
    if (!(condition)) { \
        *cpp_utilities::testing::get_test_config().error \
            << cpp_utilities::testing::colors::red() << cpp_utilities::testing::colors::bold() \
            << "ASSERT_TRUE FAILED: " << cpp_utilities::testing::colors::reset() \
            << cpp_utilities::testing::colors::red() << msg \
            << "\n  at " << __FILE__ << ":" << __LINE__ \
            << cpp_utilities::testing::colors::reset() << std::endl; \
        if (cpp_utilities::testing::get_test_config().abort_on_failure) { \
            std::abort(); \
        } \
        return false; \
    }

/**
 * @brief Assert false
 */
#define ASSERT_FALSE(condition, msg) \
    if ((condition)) { \
        *cpp_utilities::testing::get_test_config().error \
            << cpp_utilities::testing::colors::red() << cpp_utilities::testing::colors::bold() \
            << "ASSERT_FALSE FAILED: " << cpp_utilities::testing::colors::reset() \
            << cpp_utilities::testing::colors::red() << msg \
            << "\n  at " << __FILE__ << ":" << __LINE__ \
            << cpp_utilities::testing::colors::reset() << std::endl; \
        if (cpp_utilities::testing::get_test_config().abort_on_failure) { \
            std::abort(); \
        } \
        return false; \
    }

/**
 * @brief Assert that two floating-point values are approximately equal
 * Uses approximateEqual from EqualityComparisons.h with default epsilon
 */
#define ASSERT_CLOSE(actual, expected, msg) \
    if (!cpp_utilities::approximateEqual((actual), (expected))) { \
        *cpp_utilities::testing::get_test_config().error \
            << cpp_utilities::testing::colors::red() << cpp_utilities::testing::colors::bold() \
            << "ASSERT_CLOSE FAILED: " << cpp_utilities::testing::colors::reset() \
            << cpp_utilities::testing::colors::red() << msg \
            << "\n  Expected: " << (expected) \
            << "\n  Actual:   " << (actual) \
            << "\n  Diff:     " << std::abs((actual) - (expected)) \
            << "\n  at " << __FILE__ << ":" << __LINE__ \
            << cpp_utilities::testing::colors::reset() << std::endl; \
        if (cpp_utilities::testing::get_test_config().abort_on_failure) { \
            std::abort(); \
        } \
        return false; \
    }

/**
 * @brief Assert that two floating-point values are approximately equal with custom epsilon
 * Uses the same epsilon value for both relative and absolute tolerance
 */
#define ASSERT_CLOSE_EPS(actual, expected, epsilon, msg) \
    if (!cpp_utilities::approximateEqual((actual), (expected), (epsilon), (epsilon))) { \
        *cpp_utilities::testing::get_test_config().error \
            << cpp_utilities::testing::colors::red() << cpp_utilities::testing::colors::bold() \
            << "ASSERT_CLOSE_EPS FAILED: " << cpp_utilities::testing::colors::reset() \
            << cpp_utilities::testing::colors::red() << msg \
            << "\n  Expected: " << (expected) \
            << "\n  Actual:   " << (actual) \
            << "\n  Epsilon:  " << (epsilon) \
            << "\n  Diff:     " << std::abs((actual) - (expected)) \
            << "\n  at " << __FILE__ << ":" << __LINE__ \
            << cpp_utilities::testing::colors::reset() << std::endl; \
        if (cpp_utilities::testing::get_test_config().abort_on_failure) { \
            std::abort(); \
        } \
        return false; \
    }

/**
 * @brief Assert that two floating-point values are approximately equal with separate relative and absolute epsilon
 * Provides full control over the HybridComparisonPolicy parameters
 */
#define ASSERT_CLOSE_REL_ABS(actual, expected, rel_eps, abs_eps, msg) \
    if (!cpp_utilities::approximateEqual((actual), (expected), (rel_eps), (abs_eps))) { \
        *cpp_utilities::testing::get_test_config().error \
            << cpp_utilities::testing::colors::red() << cpp_utilities::testing::colors::bold() \
            << "ASSERT_CLOSE_REL_ABS FAILED: " << cpp_utilities::testing::colors::reset() \
            << cpp_utilities::testing::colors::red() << msg \
            << "\n  Expected: " << (expected) \
            << "\n  Actual:   " << (actual) \
            << "\n  Rel Eps:  " << (rel_eps) \
            << "\n  Abs Eps:  " << (abs_eps) \
            << "\n  Diff:     " << std::abs((actual) - (expected)) \
            << "\n  at " << __FILE__ << ":" << __LINE__ \
            << cpp_utilities::testing::colors::reset() << std::endl; \
        if (cpp_utilities::testing::get_test_config().abort_on_failure) { \
            std::abort(); \
        } \
        return false; \
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
template <typename T>
inline void DoNotOptimize(T const& value) {
#if defined(__GNUC__) || defined(__clang__)
    asm volatile("" : : "r,m"(value) : "memory");
#elif defined(_MSC_VER)
    // MSVC doesn't optimize away the pointer read
    volatile const T* ptr = &value;
    (void)ptr;
#else
    // Portable fallback
    static volatile const T* ptr = &value;
    (void)ptr;
#endif
}

/**
 * @brief Overload for non-const references
 */
template <typename T>
inline void DoNotOptimize(T& value) {
#if defined(__GNUC__) || defined(__clang__)
    asm volatile("" : "+r,m"(value) : : "memory");
#elif defined(_MSC_VER)
    volatile T* ptr = &value;
    (void)ptr;
#else
    static volatile T* ptr = &value;
    (void)ptr;
#endif
}


/**
 * @brief Statistics for benchmark results
 */
struct BenchmarkStats {
    double min_ms;
    double max_ms;
    double mean_ms;
    double median_ms;
    double stddev_ms;
    size_t iterations;
    
    double min_ns() const { return min_ms * 1000.0; }
    double max_ns() const { return max_ms * 1000.0; }
    double mean_ns() const { return mean_ms * 1000.0; }
    double median_ns() const { return median_ms * 1000.0; }
    double stddev_ns() const { return stddev_ms * 1000.0; }
};

/**
 * @brief Performance measurement helper with warm-up
 * 
 * Runs the given function N times and returns the average duration per call in milliseconds.
 * Includes warm-up iterations to prime caches.
 * 
 * @tparam Func Function type to measure
 * @param func The function to measure (should be fast, < 1ms per call)
 * @param iterations Number of iterations to run (default: 1,000,000)
 * @param warmup_iterations Number of warm-up iterations (default: 1000)
 * @return Average time per call in milliseconds
 */
template <typename Func>
double measure_perf(Func func, size_t iterations = 1000000, size_t warmup_iterations = 1000) {
    // Warm-up phase to prime caches
    for (size_t i = 0; i < warmup_iterations; ++i) {
        func();
    }
    
    // Measurement phase
    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < iterations; ++i) {
        func();
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    return std::chrono::duration<double, std::milli>(end - start).count() / iterations;
}

/**
 * @brief Advanced performance measurement with statistics
 * 
 * Runs multiple batches and collects statistics (min, max, mean, median, stddev).
 * 
 * @tparam Func Function type to measure
 * @param func The function to measure
 * @param iterations Number of iterations per batch
 * @param batches Number of batches to run (default: 10)
 * @return BenchmarkStats with detailed statistics
 */
template <typename Func>
BenchmarkStats measure_perf_stats(Func func, size_t iterations = 1000000, size_t batches = 10) {
    std::vector<double> times;
    times.reserve(batches);
    
    // Warm-up
    for (size_t i = 0; i < 1000; ++i) {
        func();
    }
    
    // Run batches
    for (size_t b = 0; b < batches; ++b) {
        auto start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < iterations; ++i) {
            func();
        }
        auto end = std::chrono::high_resolution_clock::now();
        double batch_ms = std::chrono::duration<double, std::milli>(end - start).count() / iterations;
        times.push_back(batch_ms);
    }
    
    // Calculate statistics
    std::sort(times.begin(), times.end());
    
    double min = times.front();
    double max = times.back();
    double sum = 0.0;
    for (double t : times) sum += t;
    double mean = sum / times.size();
    
    double median = times.size() % 2 == 0 
        ? (times[times.size()/2 - 1] + times[times.size()/2]) / 2.0
        : times[times.size()/2];
    
    double variance = 0.0;
    for (double t : times) {
        double diff = t - mean;
        variance += diff * diff;
    }
    variance /= times.size();
    double stddev = std::sqrt(variance);
    
    return BenchmarkStats{min, max, mean, median, stddev, iterations};
}

/**
 * @brief Formats time in appropriate units (ns, ÃŽÂ¼s, ms, s)
 */
inline std::string format_time(double time_ms) {
    if (time_ms < 0.001) {
        return std::to_string(time_ms * 1000000.0) + " ns";
    } else if (time_ms < 1.0) {
        return std::to_string(time_ms * 1000.0) + " us";
    } else if (time_ms < 1000.0) {
        return std::to_string(time_ms) + " ms";
    } else {
        return std::to_string(time_ms / 1000.0) + " s";
    }
}

/**
 * @brief Measures performance and prints results in a formatted way
 * 
 * @tparam Func Function type to measure
 * @param name Description of what's being measured
 * @param func The function to measure
 * @param iterations Number of iterations (default: 1,000,000)
 */
template <typename Func>
void benchmark(const char* name, Func func, size_t iterations = 1000000) {
    double avg_ms = measure_perf(func, iterations);
    
    auto& out = *get_test_config().output;
    out << colors::cyan() << name << colors::reset() << ":\n";
    out << "  Average time per operation: " << colors::bold() 
        << format_time(avg_ms) << colors::reset() << "\n";
    out << "  Total for " << iterations << " iterations: " 
        << format_time(avg_ms * iterations) << "\n";
}

/**
 * @brief Detailed benchmark with statistics
 * 
 * @tparam Func Function type to measure
 * @param name Description of what's being measured
 * @param func The function to measure
 * @param iterations Number of iterations per batch
 * @param batches Number of batches to run
 */
template <typename Func>
void benchmark_detailed(const char* name, Func func, size_t iterations = 1000000, size_t batches = 10) {
    auto stats = measure_perf_stats(func, iterations, batches);
    
    auto& out = *get_test_config().output;
    out << colors::cyan() << name << colors::reset() << " (" << batches << " batches):\n";
    out << "  Mean:   " << colors::bold() << format_time(stats.mean_ms) << colors::reset() << "\n";
    out << "  Median: " << format_time(stats.median_ms) << "\n";
    out << "  Min:    " << format_time(stats.min_ms) << "\n";
    out << "  Max:    " << format_time(stats.max_ms) << "\n";
    out << "  StdDev: " << format_time(stats.stddev_ms) << "\n";
    out << "  Total:  " << format_time(stats.mean_ms * iterations) << " per batch\n";
}

/**
 * @brief Compare two functions and show speedup/slowdown
 * 
 * @tparam Func1 First function type
 * @tparam Func2 Second function type
 * @param name1 Description of first function
 * @param func1 First function
 * @param name2 Description of second function
 * @param func2 Second function
 * @param iterations Number of iterations
 */
template <typename Func1, typename Func2>
void benchmark_compare(const char* name1, Func1 func1,
                      const char* name2, Func2 func2,
                      size_t iterations = 1000000) {
    auto& out = *get_test_config().output;
    
    out << colors::cyan() << "Comparing: " << colors::reset() 
        << name1 << " vs " << name2 << "\n";
    
    double time1 = measure_perf(func1, iterations);
    double time2 = measure_perf(func2, iterations);
    
    out << "  " << name1 << ": " << format_time(time1) << "\n";
    out << "  " << name2 << ": " << format_time(time2) << "\n";
    
    if (time1 < time2)
    {
        double speedup = time2 / time1;
        out << "  " << colors::green() << name1 << " is " 
            << std::fixed << std::setprecision(2) << speedup << "x faster" 
            << colors::reset() << "\n";
    } else if (time2 < time1) 
    {
        double speedup = time1 / time2;
        out << "  " << colors::green() << name2 << " is " 
            << std::fixed << std::setprecision(2) << speedup << "x faster" 
            << colors::reset() << "\n";
    }
    else
    {
        out << "  " << colors::yellow() << "Same performance" 
            << colors::reset() << "\n";
    }
}

// ============================================================================
// Test Runner
// ============================================================================

/**
 * @brief Test case result
 */
struct TestResult {
    std::string name;
    bool passed;
    std::string error_message;
    double duration_ms;
};

/**
 * @brief Simple test runner for organizing tests
 */
class TestRunner {
private:
    std::vector<TestResult> results_;
    
public:
    /**
     * @brief Run a test and record result
     * 
     * @param name Test name
     * @param test_func Test function that returns bool
     * @return True if test passed
     */
    template <typename Func>
    bool run_test(const char* name, Func test_func) {
        auto& out = *get_test_config().output;
        
        if (get_test_config().verbose) {
            out << colors::blue() << "Running: " << colors::reset() << name << " ... ";
            out.flush();
        }
        
        auto start = std::chrono::high_resolution_clock::now();
        bool passed = test_func();
        auto end = std::chrono::high_resolution_clock::now();
        double duration = std::chrono::duration<double, std::milli>(end - start).count();
        
        results_.push_back({name, passed, "", duration});
        
        if (get_test_config().verbose) {
            if (passed) {
                out << colors::green() << colors::bold() << "PASSED" 
                    << colors::reset() << " (" << duration << " ms)\n";
            } else {
                out << colors::red() << colors::bold() << "FAILED" 
                    << colors::reset() << "\n";
            }
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
        
        for (const auto& result : results_)
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
        
        out << "\n" << colors::bold() << "=== Test Summary ===" 
            << colors::reset() << "\n";
        out << colors::green() << "Passed: " << passed << colors::reset() << "\n";
        if (failed > 0)
        {
            out << colors::red() << "Failed: " << failed << colors::reset() << "\n";
        }
        else
        {
            out << "Failed: " << failed << "\n";
        }
        out << "Total:  " << (passed + failed) << "\n";
        
        if (0 > failed)
        {
            out << "\nFailed tests:\n";
            for (const auto& result : results_)
            {
                if (!result.passed)
                {
                    out << "  " << colors::red() << result.name 
                        << colors::reset() << "\n";
                }
            }
            out << "\n";
        }
        
        return failed;
    }
    
    /**
     * @brief Get all test results
     */
    const std::vector<TestResult>& results() const { return results_; }
    
    /**
     * @brief Clear all results
     */
    void clear() { results_.clear(); }
};

// ============================================================================
// Convenience Macros for Test Suites
// ============================================================================

/**
 * @brief Define a test function
 * 
 * Usage:
 *   TEST_CASE("my test") {
 *       ASSERT_EQ(1 + 1, 2, "Math works");
 *       return true;
 *   }
 */
#define TEST_CASE(name) \
    bool test_##name()

/**
 * @brief Run a test case with test runner
 * 
 * Usage:
 *   RUN_TEST(runner, my_test);
 */
#define RUN_TEST(runner, test_name) \
    runner.run_test(#test_name, test_##test_name)

 /**
  * \brief Prints a header for unit tests.
  *
  * This macro outputs a formatted header to std::cout, including the specified section name,
  * surrounded by separator lines, for organizing unit test output.
  *
  * Usage example:
  * \code
  * PRINT_HEADER(CONTRACT EXCEPTION);
  * \endcode
  *
  * \param section The name of the section (e.g., CONTRACT EXCEPTION). It will be stringified.
  */
#define PRINT_HEADER(section) \
    std::cout << "\n"; \
    std::cout << "==========================================================\n"; \
    std::cout << #section << " UNIT TESTS\n"; \
    std::cout << "==========================================================\n"; \
    std::cout << "\n";

} // namespace testing
} // namespace cpp_utilities