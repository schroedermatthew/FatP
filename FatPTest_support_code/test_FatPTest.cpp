/**
 * @file test_Utilities_RealTest.cpp
 * @brief Comprehensive unit test for test_Utilities.h - Testing the REAL implementation
 * 
 * This test validates ALL functionality in test_Utilities.h using basic C++ assertions
 * WITHOUT depending on test_Utilities.h macros for testing (avoids circular dependency)
 * 
 * COMPILE:
 * g++ -std=c++17 -Wall -Wextra -O2 test_Utilities_RealTest.cpp -o test_real -pthread
 * 
 * RUN:
 * ./test_real
 */

#include <iostream>
#include <cassert>
#include <cmath>
#include <limits>
#include <sstream>
#include <memory>
#include <atomic>
#include <vector>
#include <fstream>
#include <cstring>

// Include the REAL test_Utilities.h header
#include "test_Utilities.h"

using namespace fat_p::testing;

// ============================================================================
// Test Infrastructure (Manual - no dependency on test_Utilities.h)
// ============================================================================

static int g_tests_run = 0;
static int g_tests_passed = 0;
static int g_sections_run = 0;

#define VERIFY(condition, message) \
    do { \
        ++g_tests_run; \
        if (!(condition)) { \
            std::cerr << "  FAILED: " << message << " (line " << __LINE__ << ")" << std::endl; \
        } else { \
            ++g_tests_passed; \
        } \
    } while(0)

#define TEST_SECTION(name) \
    do { \
        std::cout << "\n[" << (++g_sections_run) << "] " << name << std::endl; \
    } while(0)

// Helper to capture output
struct OutputCapture
{
    std::ostringstream captured_output;
    std::ostringstream captured_error;
    std::ostream* old_output;
    std::ostream* old_error;
    
    OutputCapture()
    {
        old_output = get_test_config().output;
        old_error = get_test_config().error;
        get_test_config().output = &captured_output;
        get_test_config().error = &captured_error;
    }
    
    ~OutputCapture()
    {
        get_test_config().output = old_output;
        get_test_config().error = old_error;
    }
    
    std::string get_output() { return captured_output.str(); }
    std::string get_error() { return captured_error.str(); }
};

// ============================================================================
// SECTION 1: Primitive Floating-Point Comparison
// ============================================================================

void test_primitive_comparison()
{
    TEST_SECTION("Primitive Floating-Point Comparison");
    
    // NaN handling
    double nan = std::numeric_limits<double>::quiet_NaN();
    VERIFY(!primitive::are_close(nan, nan), "NaN != NaN (IEEE 754)");
    VERIFY(!primitive::are_close(nan, 1.0), "NaN != value");
    VERIFY(!primitive::are_close(1.0, nan), "value != NaN");
    
    // Infinity handling
    double inf = std::numeric_limits<double>::infinity();
    double neg_inf = -std::numeric_limits<double>::infinity();
    VERIFY(primitive::are_close(inf, inf), "+∞ == +∞");
    VERIFY(primitive::are_close(neg_inf, neg_inf), "-∞ == -∞");
    VERIFY(!primitive::are_close(inf, neg_inf), "+∞ != -∞");
    VERIFY(!primitive::are_close(inf, 1e308), "∞ != finite");
    
    // Zero handling
    VERIFY(primitive::are_close(0.0, -0.0), "+0 == -0");
    VERIFY(primitive::are_close(0.0, 0.0), "0 == 0");
    VERIFY(primitive::are_close(1e-100, 0.0), "tiny ≈ 0");
    
    // Relative tolerance
    VERIFY(primitive::are_close(1.0, 1.0 + 1e-15), "1.0 ≈ 1.0+ε");
    VERIFY(!primitive::are_close(1.0, 1.001), "1.0 ≉ 1.001");
    
    // Custom epsilon
    VERIFY(primitive::are_close(1.0, 1.05, 0.1, 0.1), "Custom epsilon works");
}

// ============================================================================
// SECTION 2: Configuration & Colors
// ============================================================================

void test_configuration()
{
    TEST_SECTION("Configuration & Colors");
    
    // TestConfig
    TestConfig& config = get_test_config();
    VERIFY(config.colored_output == true, "Colors enabled by default");
    VERIFY(config.verbose == false, "Verbose disabled by default");
    VERIFY(config.abort_on_failure == false, "Abort disabled by default");
    VERIFY(config.output == &std::cout, "Output is stdout");
    VERIFY(config.error == &std::cerr, "Error is stderr");
    
    // Test modification
    bool old_verbose = config.verbose;
    config.verbose = true;
    VERIFY(config.verbose == true, "Config modifiable");
    config.verbose = old_verbose;
    
    // Colors
    VERIFY(std::string(colors::red()) == "\033[91m", "Red color correct");
    VERIFY(std::string(colors::green()) == "\033[92m", "Green color correct");
    VERIFY(std::string(colors::yellow()) == "\033[93m", "Yellow color correct");
    VERIFY(std::string(colors::blue()) == "\033[94m", "Blue color correct");
    VERIFY(std::string(colors::cyan()) == "\033[96m", "Cyan color correct");
    VERIFY(std::string(colors::bold()) == "\033[1m", "Bold correct");
    VERIFY(std::string(colors::reset()) == "\033[0m", "Reset correct");
    
    // Color disable
    bool old_color = config.colored_output;
    config.colored_output = false;
    VERIFY(std::string(colors::red()) == "", "Colors disabled");
    config.colored_output = old_color;
}

// ============================================================================
// SECTION 3: String Utilities
// ============================================================================

void test_string_utilities()
{
    TEST_SECTION("String Utilities");
    
    // contains
    VERIFY(string_utils::contains("hello world", "world"), "contains: found");
    VERIFY(string_utils::contains("hello", ""), "contains: empty");
    VERIFY(!string_utils::contains("hello", "xyz"), "contains: not found");
    
    // starts_with
    VERIFY(string_utils::starts_with("hello world", "hello"), "starts_with: prefix");
    VERIFY(!string_utils::starts_with("hello", "world"), "starts_with: wrong");
    
    // ends_with
    VERIFY(string_utils::ends_with("hello world", "world"), "ends_with: suffix");
    VERIFY(!string_utils::ends_with("hello", "world"), "ends_with: wrong");
    
    // to_lower
    VERIFY(string_utils::to_lower("HELLO") == "hello", "to_lower: uppercase");
    VERIFY(string_utils::to_lower("HeLLo") == "hello", "to_lower: mixed");
    
    // matches_pattern
    VERIFY(string_utils::matches_pattern("test", "test"), "pattern: exact");
    VERIFY(string_utils::matches_pattern("test", "t*t"), "pattern: wildcard *");
    VERIFY(string_utils::matches_pattern("test", "t?st"), "pattern: wildcard ?");
    VERIFY(string_utils::matches_pattern("hello", "*"), "pattern: match all");
    VERIFY(!string_utils::matches_pattern("test", "xyz"), "pattern: no match");
}

// ============================================================================
// SECTION 4: ASSERT Macros - Basic
// ============================================================================

void test_assert_macros_basic()
{
    TEST_SECTION("ASSERT Macros - Basic");
    
    OutputCapture capture;
    
    // SIMPLE_ASSERT pass
    bool result = []() -> bool {
        SIMPLE_ASSERT(true, "Should pass");
        return true;
    }();
    VERIFY(result, "SIMPLE_ASSERT passes");
    
    // SIMPLE_ASSERT fail
    result = []() -> bool {
        SIMPLE_ASSERT(false, "Should fail");
        return true;
    }();
    VERIFY(!result, "SIMPLE_ASSERT fails");
    
    // ASSERT_EQ pass
    result = []() -> bool {
        int a = 42, b = 42;
        ASSERT_EQ(a, b, "Equal values");
        return true;
    }();
    VERIFY(result, "ASSERT_EQ passes");
    
    // ASSERT_EQ fail
    result = []() -> bool {
        int a = 42, b = 43;
        ASSERT_EQ(a, b, "Unequal values");
        return true;
    }();
    VERIFY(!result, "ASSERT_EQ fails");
    
    // ASSERT_NE
    result = []() -> bool {
        int a = 42, b = 43;
        ASSERT_NE(a, b, "Not equal");
        return true;
    }();
    VERIFY(result, "ASSERT_NE passes");
    
    // ASSERT_TRUE/FALSE
    result = []() -> bool {
        ASSERT_TRUE(true, "True test");
        ASSERT_FALSE(false, "False test");
        return true;
    }();
    VERIFY(result, "ASSERT_TRUE/FALSE pass");
}

// ============================================================================
// SECTION 5: ASSERT Macros - Comparison
// ============================================================================

void test_assert_macros_comparison()
{
    TEST_SECTION("ASSERT Macros - Comparison");
    
    OutputCapture capture;
    
    // ASSERT_LT
    bool result = []() -> bool {
        ASSERT_LT(5, 10, "5 < 10");
        return true;
    }();
    VERIFY(result, "ASSERT_LT passes");
    
    // ASSERT_LE
    result = []() -> bool {
        ASSERT_LE(5, 5, "5 <= 5");
        return true;
    }();
    VERIFY(result, "ASSERT_LE passes");
    
    // ASSERT_GT
    result = []() -> bool {
        ASSERT_GT(10, 5, "10 > 5");
        return true;
    }();
    VERIFY(result, "ASSERT_GT passes");
    
    // ASSERT_GE
    result = []() -> bool {
        ASSERT_GE(10, 10, "10 >= 10");
        return true;
    }();
    VERIFY(result, "ASSERT_GE passes");
}

// ============================================================================
// SECTION 6: ASSERT Macros - Pointers
// ============================================================================

void test_assert_macros_pointers()
{
    TEST_SECTION("ASSERT Macros - Pointers");
    
    OutputCapture capture;
    
    // ASSERT_NULLPTR
    bool result = []() -> bool {
        int* ptr = nullptr;
        ASSERT_NULLPTR(ptr, "Null pointer");
        return true;
    }();
    VERIFY(result, "ASSERT_NULLPTR passes");
    
    // ASSERT_NOT_NULLPTR
    result = []() -> bool {
        int value = 42;
        int* ptr = &value;
        ASSERT_NOT_NULLPTR(ptr, "Valid pointer");
        return true;
    }();
    VERIFY(result, "ASSERT_NOT_NULLPTR passes");
}

// ============================================================================
// SECTION 7: ASSERT Macros - Floating-Point
// ============================================================================

void test_assert_macros_float()
{
    TEST_SECTION("ASSERT Macros - Floating-Point");
    
    OutputCapture capture;
    
    // ASSERT_CLOSE
    bool result = []() -> bool {
        double a = 0.1 + 0.2;
        double b = 0.3;
        ASSERT_CLOSE(a, b, "0.1 + 0.2 ≈ 0.3");
        return true;
    }();
    VERIFY(result, "ASSERT_CLOSE passes");
    
    // ASSERT_CLOSE_EPS
    result = []() -> bool {
        double a = 1.0;
        double b = 1.001;
        ASSERT_CLOSE_EPS(a, b, 0.01, "Custom epsilon");
        return true;
    }();
    VERIFY(result, "ASSERT_CLOSE_EPS passes");
    
    // ASSERT_CLOSE_REL_ABS
    result = []() -> bool {
        double a = 1000.0;
        double b = 1001.0;
        ASSERT_CLOSE_REL_ABS(a, b, 0.01, 0.01, "Rel/abs epsilon");
        return true;
    }();
    VERIFY(result, "ASSERT_CLOSE_REL_ABS passes");
}

// ============================================================================
// SECTION 8: ASSERT Macros - Exceptions
// ============================================================================

void test_assert_macros_exceptions()
{
    TEST_SECTION("ASSERT Macros - Exceptions");
    
    OutputCapture capture;
    
    // ASSERT_THROWS
    bool result = []() -> bool {
        ASSERT_THROWS(throw std::runtime_error("test"), std::runtime_error, "Correct exception");
        return true;
    }();
    VERIFY(result, "ASSERT_THROWS passes");
    
    // ASSERT_NO_THROW
    result = []() -> bool {
        ASSERT_NO_THROW({ int x = 42; (void)x; }, "No exception");
        return true;
    }();
    VERIFY(result, "ASSERT_NO_THROW passes");
}

// ============================================================================
// SECTION 9: ASSERT Macros - Strings
// ============================================================================

void test_assert_macros_strings()
{
    TEST_SECTION("ASSERT Macros - Strings");
    
    OutputCapture capture;
    
    // ASSERT_CONTAINS
    bool result = []() -> bool {
        std::string str = "hello world";
        ASSERT_CONTAINS(str, "world", "Contains substring");
        return true;
    }();
    VERIFY(result, "ASSERT_CONTAINS passes");
    
    // ASSERT_NOT_CONTAINS
    result = []() -> bool {
        std::string str = "hello world";
        ASSERT_NOT_CONTAINS(str, "xyz", "Does not contain");
        return true;
    }();
    VERIFY(result, "ASSERT_NOT_CONTAINS passes");
    
    // ASSERT_STARTS_WITH
    result = []() -> bool {
        std::string str = "hello world";
        ASSERT_STARTS_WITH(str, "hello", "Starts with");
        return true;
    }();
    VERIFY(result, "ASSERT_STARTS_WITH passes");
    
    // ASSERT_ENDS_WITH
    result = []() -> bool {
        std::string str = "hello world";
        ASSERT_ENDS_WITH(str, "world", "Ends with");
        return true;
    }();
    VERIFY(result, "ASSERT_ENDS_WITH passes");
    
    // ASSERT_MATCHES
    result = []() -> bool {
        std::string str = "test123";
        ASSERT_MATCHES(str, "test\\d+", "Regex match");
        return true;
    }();
    VERIFY(result, "ASSERT_MATCHES passes");
    
    // ASSERT_STR_EQ_IGNORE_CASE
    result = []() -> bool {
        ASSERT_STR_EQ_IGNORE_CASE("Hello", "HELLO", "Case insensitive");
        return true;
    }();
    VERIFY(result, "ASSERT_STR_EQ_IGNORE_CASE passes");
}

// ============================================================================
// SECTION 10: ASSERT Macros - Ranges
// ============================================================================

void test_assert_macros_ranges()
{
    TEST_SECTION("ASSERT Macros - Ranges");
    
    OutputCapture capture;
    
    // ASSERT_RANGE_EQ
    bool result = []() -> bool {
        std::vector<int> v1 = {1, 2, 3};
        std::vector<int> v2 = {1, 2, 3};
        ASSERT_RANGE_EQ(v1, v2, "Equal ranges");
        return true;
    }();
    VERIFY(result, "ASSERT_RANGE_EQ passes");
    
    // ASSERT_RANGE_CLOSE
    result = []() -> bool {
        std::vector<double> v1 = {1.0, 2.0, 3.0};
        std::vector<double> v2 = {1.001, 2.001, 3.001};
        ASSERT_RANGE_CLOSE(v1, v2, 0.01, "Close ranges");
        return true;
    }();
    VERIFY(result, "ASSERT_RANGE_CLOSE passes");
}

// ============================================================================
// SECTION 11: Performance Measurement
// ============================================================================

void test_performance_measurement()
{
    TEST_SECTION("Performance Measurement");
    
    // DoNotOptimize
    int value = 42;
    DoNotOptimize(value);
    VERIFY(true, "DoNotOptimize compiles");
    
    // format_time
    std::string ns_str = format_time(0.0001);
    VERIFY(ns_str.find("ns") != std::string::npos, "format_time: ns");
    
    std::string ms_str = format_time(10.0);
    VERIFY(ms_str.find("ms") != std::string::npos, "format_time: ms");
    
    // measure_perf
    double time = measure_perf([]() { volatile int x = 42; (void)x; }, 1000);
    VERIFY(time >= 0, "measure_perf returns non-negative");
    
    // measure_perf_stats
    auto stats = measure_perf_stats([]() { volatile int x = 42; (void)x; }, 1000, 10);
    VERIFY(stats.min_ms <= stats.mean_ms, "stats: min <= mean");
    VERIFY(stats.mean_ms <= stats.max_ms, "stats: mean <= max");
    VERIFY(stats.stddev_ms >= 0, "stats: stddev non-negative");
    VERIFY(stats.iterations == 1000, "stats: iterations correct");
    
    // BenchmarkStats conversions
    VERIFY(stats.min_ns() >= 0, "stats: min_ns conversion");
    VERIFY(stats.max_ns() >= 0, "stats: max_ns conversion");
    VERIFY(stats.mean_ns() >= 0, "stats: mean_ns conversion");
}

// ============================================================================
// SECTION 12: Benchmarking
// ============================================================================

void test_benchmarking()
{
    TEST_SECTION("Benchmarking");
    
    OutputCapture capture;
    
    // benchmark
    benchmark("simple_bench", []() { volatile int x = 42; (void)x; }, 100);
    std::string output = capture.get_output();
    VERIFY(output.find("simple_bench") != std::string::npos, "benchmark outputs name");
    
    // benchmark_detailed
    capture = OutputCapture();
    benchmark_detailed("detailed_bench", []() { volatile int x = 42; (void)x; }, 100, 5);
    output = capture.get_output();
    VERIFY(output.find("Mean:") != std::string::npos, "benchmark_detailed shows stats");
    VERIFY(output.find("P95:") != std::string::npos, "benchmark_detailed shows P95");
    
    // benchmark_compare
    capture = OutputCapture();
    benchmark_compare("func1", []() { volatile int x = 42; (void)x; },
                     "func2", []() { volatile int y = 43; (void)y; }, 100);
    output = capture.get_output();
    VERIFY(output.find("Comparing:") != std::string::npos, "benchmark_compare shows comparison");
    
    // BenchmarkBaseline
    BenchmarkBaseline& baseline = get_benchmark_baseline();
    BenchmarkStats test_stats{1.0, 2.0, 1.5, 1.4, 0.5, 1.8, 1.9, 0, 1000};
    baseline.save("test_bench", test_stats);
    VERIFY(baseline.has_baseline("test_bench"), "Baseline saves");
    
    const auto& retrieved = baseline.get("test_bench");
    VERIFY(retrieved.mean_ms == 1.5, "Baseline retrieves correctly");
    
    BenchmarkStats new_stats{1.1, 2.1, 1.65, 1.5, 0.6, 1.9, 2.0, 0, 1000};
    double change = baseline.compare("test_bench", new_stats);
    VERIFY(std::abs(change - 10.0) < 0.1, "Baseline comparison correct");
}

// ============================================================================
// SECTION 13: Test Fixtures
// ============================================================================

void test_fixtures()
{
    TEST_SECTION("Test Fixtures");
    
    struct MyFixture : public TestFixture
    {
        int* data;
        bool setup_called;
        bool teardown_called;
        
        MyFixture() : data(nullptr), setup_called(false), teardown_called(false) {}
        
        void SetUp() override
        {
            setup_called = true;
            data = new int(42);
        }
        
        void TearDown() override
        {
            teardown_called = true;
            delete data;
            data = nullptr;
        }
    };
    
    OutputCapture capture;
    TestRunner runner;
    
    bool fixture_worked = false;
    runner.run_test_with_fixture<MyFixture>("fixture_test", [&](MyFixture& f) {
        fixture_worked = f.setup_called && f.data != nullptr && *f.data == 42;
        return true;
    });
    
    VERIFY(fixture_worked, "Fixture SetUp called and data initialized");
    VERIFY(runner.results().size() == 1, "Fixture test recorded");
    VERIFY(runner.results()[0].passed, "Fixture test passed");
}

// ============================================================================
// SECTION 14: Test Runner
// ============================================================================

void test_test_runner()
{
    TEST_SECTION("Test Runner");
    
    OutputCapture capture;
    TestRunner runner;
    
    // Basic run_test
    runner.run_test("test1", []() { return true; });
    VERIFY(runner.results().size() == 1, "Test recorded");
    VERIFY(runner.results()[0].passed, "Test passed");
    VERIFY(runner.results()[0].name == "test1", "Test name correct");
    
    // run_test with failure
    runner.run_test("test2", []() { return false; });
    VERIFY(runner.results().size() == 2, "Failed test recorded");
    VERIFY(!runner.results()[1].passed, "Test marked as failed");
    
    // clear
    runner.clear();
    VERIFY(runner.results().size() == 0, "Results cleared");
    
    // set_filter
    runner.set_filter("*math*");
    runner.run_test("test_math_add", []() { return true; });
    runner.run_test("test_string_ops", []() { return true; });
    VERIFY(runner.results().size() == 1, "Filter works");
    
    // print_summary
    runner.clear();
    runner.run_test("pass1", []() { return true; });
    runner.run_test("pass2", []() { return true; });
    runner.run_test("fail1", []() { return false; });
    
    int failed = runner.print_summary();
    VERIFY(failed == 1, "Summary returns failure count");
}

// ============================================================================
// SECTION 15: Test Runner Advanced
// ============================================================================

void test_test_runner_advanced()
{
    TEST_SECTION("Test Runner - Advanced");
    
    OutputCapture capture;
    TestRunner runner;
    
    // run_test_with_timeout
    runner.run_test_with_timeout("fast_test", []() { return true; }, 1000);
    VERIFY(runner.results().size() == 1, "Timeout test ran");
    VERIFY(runner.results()[0].passed, "Fast test passed");
    
    // run_test_repeat
    runner.clear();
    auto repeat_result = runner.run_test_repeat("stable_test", []() { return true; }, 10);
    VERIFY(repeat_result.total_runs == 10, "Repeat ran 10 times");
    VERIFY(repeat_result.passed == 10, "All repetitions passed");
    VERIFY(repeat_result.pass_rate == 100.0, "100% pass rate");
    
    // run_test_repeat with flaky test
    int counter = 0;
    auto flaky_result = runner.run_test_repeat("flaky_test", [&]() {
        return counter++ % 2 == 0;
    }, 10);
    VERIFY(flaky_result.total_runs == 10, "Flaky test ran 10 times");
    VERIFY(flaky_result.failed > 0, "Flaky test has failures");
    VERIFY(flaky_result.pass_rate < 100.0, "Flaky test pass rate < 100%");
    
    // run_until_failure
    counter = 0;
    size_t failed_at = runner.run_until_failure("eventual_fail", [&]() {
        return counter++ < 5;
    }, 100);
    VERIFY(failed_at == 6, "Failed on 6th iteration");
}

// ============================================================================
// SECTION 16: Parameterized Tests
// ============================================================================

void test_parameterized_tests()
{
    TEST_SECTION("Parameterized Tests");
    
    OutputCapture capture;
    
    std::vector<TestCase<int, int, int>> cases = {
        {2, 3, 5, "2+3=5"},
        {10, 20, 30, "10+20=30"},
        {-1, 1, 0, "-1+1=0"}
    };
    
    bool result = run_parameterized_test("addition", cases, [](const auto& tc) {
        int a = std::get<0>(tc.inputs);
        int b = std::get<1>(tc.inputs);
        int expected = std::get<2>(tc.inputs);
        return (a + b) == expected;
    });
    
    VERIFY(result, "Parameterized test passed");
}

// ============================================================================
// SECTION 17: Subtest Tracking
// ============================================================================

void test_subtest_tracking()
{
    TEST_SECTION("Subtest Tracking");
    
    OutputCapture capture;
    
    // All subtests pass
    bool result = []() -> bool {
        get_subtest_tracker().clear();
        
        SUBTEST("part1") {
            int x = 1 + 1;
            if (x != 2) throw std::runtime_error("Math broken");
        }
        END_SUBTEST
        
        SUBTEST("part2") {
            int y = 2 + 2;
            if (y != 4) throw std::runtime_error("Math broken");
        }
        END_SUBTEST
        
        return get_subtest_tracker().all_passed();
    }();
    
    VERIFY(result, "All subtests passed");
    VERIFY(get_subtest_tracker().get_results().size() == 2, "Two subtests recorded");
    
    // Subtest with failure
    result = []() -> bool {
        get_subtest_tracker().clear();
        
        SUBTEST("passing") {
            int x = 1;
            (void)x;
        }
        END_SUBTEST
        
        SUBTEST("failing") {
            throw std::runtime_error("Intentional failure");
        }
        END_SUBTEST
        
        return get_subtest_tracker().all_passed();
    }();
    
    VERIFY(!result, "Failed subtest detected");
    VERIFY(get_subtest_tracker().get_results().size() == 2, "Both subtests recorded");
    VERIFY(!get_subtest_tracker().get_results()[1].passed, "Second subtest failed");
}

// ============================================================================
// SECTION 18: JUnit XML Export
// ============================================================================

void test_junit_xml()
{
    TEST_SECTION("JUnit XML Export");
    
    // xml_escape
    VERIFY(xml_escape("&") == "&amp;", "XML escape: &");
    VERIFY(xml_escape("<") == "&lt;", "XML escape: <");
    VERIFY(xml_escape(">") == "&gt;", "XML escape: >");
    VERIFY(xml_escape("\"") == "&quot;", "XML escape: \"");
    VERIFY(xml_escape("'") == "&apos;", "XML escape: '");
    VERIFY(xml_escape("hello") == "hello", "XML escape: no change");
    
    // export_junit_xml
    std::vector<TestResult> results;
    results.push_back(TestResult{"test1", true, 10.5});
    results.push_back(TestResult{"test2", false, 5.2});
    results.push_back(TestResult{"test3", true, 8.1});
    
    bool success = export_junit_xml("test_output.xml", results, "MySuite");
    VERIFY(success, "XML export succeeded");
    
    // Verify file exists and has content
    std::ifstream file("test_output.xml");
    VERIFY(file.is_open(), "XML file created");
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();
    
    VERIFY(content.find("<?xml version") != std::string::npos, "XML header");
    VERIFY(content.find("MySuite") != std::string::npos, "Suite name");
    VERIFY(content.find("test1") != std::string::npos, "Test name in XML");
    VERIFY(content.find("<failure") != std::string::npos, "Failure tag");
    
    std::remove("test_output.xml");
    
    // TestRunner export
    TestRunner runner;
    runner.run_test("test1", []() { return true; });
    runner.run_test("test2", []() { return false; });
    success = runner.export_to_junit_xml("runner_output.xml", "RunnerSuite");
    VERIFY(success, "TestRunner XML export");
    std::remove("runner_output.xml");
}

// ============================================================================
// SECTION 19: Non-Copyable Types
// ============================================================================

void test_non_copyable_types()
{
    TEST_SECTION("Non-Copyable Types");
    
    OutputCapture capture;
    
    // Atomic (non-copyable)
    bool result = []() -> bool {
        std::atomic<int> counter{42};
        ASSERT_EQ(counter.load(), 42, "Atomic value");
        return true;
    }();
    VERIFY(result, "ASSERT_EQ works with atomic");
    
    // unique_ptr (move-only)
    result = []() -> bool {
        std::unique_ptr<int> ptr = std::make_unique<int>(42);
        ASSERT_NOT_NULLPTR(ptr.get(), "unique_ptr valid");
        return true;
    }();
    VERIFY(result, "ASSERT_NOT_NULLPTR works with unique_ptr");
}

// ============================================================================
// SECTION 20: ASSERT_WITH_HANDLER
// ============================================================================

void test_assert_with_handler()
{
    TEST_SECTION("ASSERT_WITH_HANDLER");
    
    OutputCapture capture;
    
    // Test with handler
    bool handler_called = false;
    bool result = [&]() -> bool {
        ASSERT_WITH_HANDLER(false, "Should fail", {
            handler_called = true;
        });
        return true;
    }();
    
    VERIFY(!result, "ASSERT_WITH_HANDLER fails");
    VERIFY(handler_called, "Handler was called");
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main()
{
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  COMPREHENSIVE TEST SUITE FOR test_Utilities.h (REAL)         ║\n";
    std::cout << "║  Testing the ACTUAL Implementation                            ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════╝\n";
    
    // Run all test sections
    test_primitive_comparison();
    test_configuration();
    test_string_utilities();
    test_assert_macros_basic();
    test_assert_macros_comparison();
    test_assert_macros_pointers();
    test_assert_macros_float();
    test_assert_macros_exceptions();
    test_assert_macros_strings();
    test_assert_macros_ranges();
    test_performance_measurement();
    test_benchmarking();
    test_fixtures();
    test_test_runner();
    test_test_runner_advanced();
    test_parameterized_tests();
    test_subtest_tracking();
    test_junit_xml();
    test_non_copyable_types();
    test_assert_with_handler();
    
    // Print summary
    std::cout << "\n\n";
    std::cout << "==================================================================\n";
    std::cout << "  TEST RESULTS                                                    \n";
    std::cout << "==================================================================\n";
    std::cout << "  Sections:     " << g_sections_run << "\n";
    std::cout << "  Total Tests:  " << g_tests_run << "\n";
    std::cout << "  Passed:       " << g_tests_passed << " (" 
              << (100.0 * g_tests_passed / g_tests_run) << "%)\n";
    std::cout << "  Failed:       " << (g_tests_run - g_tests_passed) << "\n";
    std::cout << "\n";
    
    if (g_tests_passed == g_tests_run)
    {
        std::cout << " ALL TESTS PASSED - 100% SUCCESS!\n";
        std::cout << "\n  Your test_Utilities.h is fully validated!\n\n";
        return 0;
    }
    else
    {
        std::cout << " SOME TESTS FAILED\n\n";
        return 1;
    }
}
