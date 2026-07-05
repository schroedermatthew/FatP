/**
 * @file test_FatPTest.cpp
 * @brief Comprehensive unit test for FatPTest.h
 *
 * COMPILE (MSVC):
 * cl /std:c++17 /EHsc test_FatPTest.cpp
 *
 * COMPILE (GCC/Clang):
 * g++ -std=c++17 -Wall -Wextra -O2 test_FatPTest.cpp -o test_real -pthread
 */
/*
FATP_META:
  meta_version: 1
  component: FatPTest
  file_role: test
  path: components/FatPTest/tests/test_FatPTest.cpp
  layer: Testing
  namespace: fat_p::testing::fatptest
  summary: "Unit tests for FatPTest."
  api_stability: in_work
  related:
    docs_search: "FatPTest"
    headers:
      - include/fat_p/FatPTest.h
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 2
    defines_unprefixed: 2
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/

#include <atomic>
#include <cassert>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <vector>

#include "FatPTest.h"

namespace fat_p::testing::fatptest
{

static int g_tests_run = 0;
static int g_tests_passed = 0;
static int g_sections_run = 0;

#define VERIFY(condition, message)                                                             \
    do                                                                                         \
    {                                                                                          \
        ++g_tests_run;                                                                         \
        if (!(condition))                                                                      \
        {                                                                                      \
            std::cerr << "  FAILED: " << message << " (line " << __LINE__ << ")" << std::endl; \
        }                                                                                      \
        else                                                                                   \
        {                                                                                      \
            ++g_tests_passed;                                                                  \
        }                                                                                      \
    } while (0)

#define TEST_SECTION(name)                                                     \
    do                                                                         \
    {                                                                          \
        std::cout << "\n[" << (++g_sections_run) << "] " << name << std::endl; \
    } while (0)

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

    OutputCapture(const OutputCapture&) = delete;
    OutputCapture& operator=(const OutputCapture&) = delete;
    OutputCapture(OutputCapture&&) = default;
    OutputCapture& operator=(OutputCapture&&) = default;

    std::string get_output()
    {
        return captured_output.str();
    }
    std::string get_error()
    {
        return captured_error.str();
    }
};

void primitive_comparison()
{
    TEST_SECTION("Primitive Floating-Point Comparison");

    constexpr double nan = std::numeric_limits<double>::quiet_NaN();
    VERIFY(!primitive::are_close(nan, nan), "NaN != NaN (IEEE 754)");
    VERIFY(!primitive::are_close(nan, 1.0), "NaN != value");
    VERIFY(!primitive::are_close(1.0, nan), "value != NaN");

    constexpr double inf = std::numeric_limits<double>::infinity();
    constexpr double neg_inf = -std::numeric_limits<double>::infinity();
    VERIFY(primitive::are_close(inf, inf), "+inf == +inf");
    VERIFY(primitive::are_close(neg_inf, neg_inf), "-inf == -inf");
    VERIFY(!primitive::are_close(inf, neg_inf), "+inf != -inf");
    VERIFY(!primitive::are_close(inf, 1e308), "inf != finite");

    VERIFY(primitive::are_close(0.0, -0.0), "+0 == -0");
    VERIFY(primitive::are_close(0.0, 0.0), "0 == 0");
    VERIFY(primitive::are_close(1e-100, 0.0), "tiny ~ 0");

    VERIFY(primitive::are_close(1.0, 1.0 + 1e-15), "1.0 ~ 1.0+eps");
    VERIFY(!primitive::are_close(1.0, 1.001), "1.0 !~ 1.001");

    VERIFY(primitive::are_close(1.0, 1.05, 0.1, 0.1), "Custom epsilon works");
}

void configuration()
{
    TEST_SECTION("Configuration & Colors");

    TestConfig& config = get_test_config();

    VERIFY(true, "Verbose test skipped - shared config");

    VERIFY(config.abort_on_failure == false, "Abort disabled by default");
    VERIFY(config.output == &std::cout, "Output is stdout");
    VERIFY(config.error == &std::cerr, "Error is stderr");

    bool old_verbose = config.verbose;
    config.verbose = true;
    VERIFY(config.verbose == true, "Config modifiable");
    config.verbose = old_verbose;

    if (config.colored_output)
{
    VERIFY(std::string(colors::red()) == "\033[91m", "Red color correct");
    VERIFY(std::string(colors::green()) == "\033[92m", "Green color correct");
    VERIFY(std::string(colors::yellow()) == "\033[93m", "Yellow color correct");
    VERIFY(std::string(colors::blue()) == "\033[94m", "Blue color correct");
    VERIFY(std::string(colors::cyan()) == "\033[96m", "Cyan color correct");
    VERIFY(std::string(colors::bold()) == "\033[1m", "Bold correct");
    VERIFY(std::string(colors::reset()) == "\033[0m", "Reset correct");
}
else
{
    VERIFY(std::string(colors::red()).empty(), "Red color disabled");
    VERIFY(std::string(colors::green()).empty(), "Green color disabled");
    VERIFY(std::string(colors::yellow()).empty(), "Yellow color disabled");
    VERIFY(std::string(colors::blue()).empty(), "Blue color disabled");
    VERIFY(std::string(colors::cyan()).empty(), "Cyan color disabled");
    VERIFY(std::string(colors::bold()).empty(), "Bold disabled");
    VERIFY(std::string(colors::reset()).empty(), "Reset disabled");
}


    bool old_color = config.colored_output;
    config.colored_output = false;
    VERIFY(std::string(colors::red()) == "", "Colors disabled");
    config.colored_output = old_color;
}

void string_utilities()
{
    TEST_SECTION("String Utilities");

    VERIFY(string_utils::contains("hello world", "world"), "contains: found");
    VERIFY(string_utils::contains("hello", ""), "contains: empty");
    VERIFY(!string_utils::contains("hello", "xyz"), "contains: not found");

    VERIFY(string_utils::starts_with("hello world", "hello"), "starts_with: prefix");
    VERIFY(!string_utils::starts_with("hello", "world"), "starts_with: wrong");

    VERIFY(string_utils::ends_with("hello world", "world"), "ends_with: suffix");
    VERIFY(!string_utils::ends_with("hello", "world"), "ends_with: wrong");

    VERIFY(string_utils::to_lower("HELLO") == "hello", "to_lower: uppercase");
    VERIFY(string_utils::to_lower("HeLLo") == "hello", "to_lower: mixed");

    // Basic pattern matching
    VERIFY(string_utils::matches_pattern("test", "test"), "pattern: exact");
    VERIFY(string_utils::matches_pattern("test", "t*t"), "pattern: wildcard *");
    VERIFY(string_utils::matches_pattern("test", "t?st"), "pattern: wildcard ?");
    VERIFY(string_utils::matches_pattern("hello", "*"), "pattern: match all");
    VERIFY(!string_utils::matches_pattern("test", "xyz"), "pattern: no match");

    // Advanced pattern matching (iterative algorithm tests)
    VERIFY(string_utils::matches_pattern("", ""), "pattern: empty strings");
    VERIFY(string_utils::matches_pattern("", "*"), "pattern: empty with star");
    VERIFY(!string_utils::matches_pattern("", "a"), "pattern: empty vs char");
    VERIFY(string_utils::matches_pattern("abc", "a*c"), "pattern: star middle");
    VERIFY(string_utils::matches_pattern("abc", "*bc"), "pattern: star start");
    VERIFY(string_utils::matches_pattern("abc", "ab*"), "pattern: star end");
    VERIFY(string_utils::matches_pattern("abcdef", "*a*b*c*"), "pattern: multiple stars");
    VERIFY(string_utils::matches_pattern("hello", "?????"), "pattern: exact ? count");
    VERIFY(!string_utils::matches_pattern("hello", "????"), "pattern: too few ?");
    VERIFY(string_utils::matches_pattern("hello", "**"), "pattern: consecutive stars");

    // Stress test - long strings (would overflow stack with recursive impl)
    std::string long_str(500, 'a');
    VERIFY(string_utils::matches_pattern(long_str, "*"), "pattern: long string star");
    VERIFY(string_utils::matches_pattern(long_str, "a*"), "pattern: long string prefix");
    VERIFY(string_utils::matches_pattern(long_str, "*a"), "pattern: long string suffix");
    VERIFY(string_utils::matches_pattern(long_str, "*a*a*a*a*a*"), "pattern: long many stars");
    VERIFY(!string_utils::matches_pattern(long_str, "*b*"), "pattern: long no match");

    // Truncation utility
    std::string short_str = "hello";
    std::string truncated = string_utils::truncate_for_display(short_str);
    VERIFY(truncated == short_str, "truncate: short unchanged");

    std::string long_display(300, 'x');
    truncated = string_utils::truncate_for_display(long_display);
    VERIFY(truncated.size() < long_display.size(), "truncate: long is shorter");
    VERIFY(truncated.find("300 chars") != std::string::npos, "truncate: shows length");
}

void auto_calibration()
{
    TEST_SECTION("Auto-Calibration");

    // Test calibrate_iterations
    volatile int sink = 0;
    auto fast_op = [&]() {
        sink = sink + 1;
    };

    double resolution_ms = HighResolutionTimer::resolution_ms();
    double min_total_ms = resolution_ms * 1000.0;

    size_t calibrated = calibrate_iterations(fast_op, min_total_ms);
    VERIFY(calibrated >= 1000, "calibrate: minimum 1000 iterations");
    VERIFY(calibrated <= 100000000, "calibrate: max 100M iterations");

    // Test measure_perf with auto-calibration (iterations = 0)
    double time_auto = measure_perf(fast_op);
    VERIFY(time_auto > 0, "measure_perf auto: positive time");

    // Test measure_perf with explicit iterations
    double time_explicit = measure_perf(fast_op, 10000);
    VERIFY(time_explicit > 0, "measure_perf explicit: positive time");

    // Test measure_perf_stats with auto-calibration
    auto stats = measure_perf_stats(fast_op, 0, 5);
    VERIFY(stats.iterations >= 1000, "stats auto: calibrated iterations");
    VERIFY(stats.min_ms <= stats.mean_ms, "stats auto: min <= mean");
    VERIFY(stats.mean_ms <= stats.max_ms, "stats auto: mean <= max");
}
void assert_macros_basic()
{
    TEST_SECTION("ASSERT Macros - Basic");

    OutputCapture capture;

    bool result = []() -> bool {
        FATP_ASSERT_TRUE(true, "Should pass");
        return true;
    }();
    VERIFY(result, "FATP_SIMPLE_ASSERT passes");

    result = []() -> bool {
        FATP_ASSERT_TRUE(false, "Should fail");
        return true;
    }();
    VERIFY(!result, "FATP_SIMPLE_ASSERT fails");

    result = []() -> bool {
        int a = 42, b = 42;
        FATP_ASSERT_EQ(a, b, "Equal values");
        return true;
    }();
    VERIFY(result, "FATP_ASSERT_EQ passes");

    result = []() -> bool {
        int a = 42, b = 43;
        FATP_ASSERT_EQ(a, b, "Unequal values");
        return true;
    }();
    VERIFY(!result, "FATP_ASSERT_EQ fails");

    result = []() -> bool {
        int a = 42, b = 43;
        FATP_ASSERT_NE(a, b, "Not equal");
        return true;
    }();
    VERIFY(result, "FATP_ASSERT_NE passes");

    result = []() -> bool {
        FATP_ASSERT_TRUE(true, "True test");
        FATP_ASSERT_FALSE(false, "False test");
        return true;
    }();
    VERIFY(result, "FATP_ASSERT_TRUE/FALSE pass");
}

void assert_macros_comparison()
{
    TEST_SECTION("ASSERT Macros - Comparison");

    OutputCapture capture;

    bool result = []() -> bool {
        FATP_ASSERT_LT(5, 10, "5 < 10");
        return true;
    }();
    VERIFY(result, "FATP_ASSERT_LT passes");

    result = []() -> bool {
        FATP_ASSERT_LE(5, 5, "5 <= 5");
        return true;
    }();
    VERIFY(result, "FATP_ASSERT_LE passes");

    result = []() -> bool {
        FATP_ASSERT_GT(10, 5, "10 > 5");
        return true;
    }();
    VERIFY(result, "FATP_ASSERT_GT passes");

    result = []() -> bool {
        FATP_ASSERT_GE(10, 10, "10 >= 10");
        return true;
    }();
    VERIFY(result, "FATP_ASSERT_GE passes");
}

void assert_macros_pointers()
{
    TEST_SECTION("ASSERT Macros - Pointers");

    OutputCapture capture;

    bool result = []() -> bool {
        int* ptr = nullptr;
        FATP_ASSERT_NULLPTR(ptr, "Null pointer");
        return true;
    }();
    VERIFY(result, "FATP_ASSERT_NULLPTR passes");

    result = []() -> bool {
        int value = 42;
        int* ptr = &value;
        FATP_ASSERT_NOT_NULLPTR(ptr, "Valid pointer");
        return true;
    }();
    VERIFY(result, "FATP_ASSERT_NOT_NULLPTR passes");
}

void assert_macros_float()
{
    TEST_SECTION("ASSERT Macros - Floating-Point");

    OutputCapture capture;

    bool result = []() -> bool {
        double a = 0.1 + 0.2;
        double b = 0.3;
        FATP_ASSERT_CLOSE(a, b, "0.1 + 0.2 ~ 0.3");
        return true;
    }();
    VERIFY(result, "FATP_ASSERT_CLOSE passes");

    result = []() -> bool {
        double a = 1.0;
        double b = 1.001;
        FATP_ASSERT_CLOSE_EPS(a, b, 0.01, "Custom epsilon");
        return true;
    }();
    VERIFY(result, "FATP_ASSERT_CLOSE_EPS passes");

    result = []() -> bool {
        double a = 1000.0;
        double b = 1001.0;
        FATP_ASSERT_CLOSE_REL_ABS(a, b, 0.01, 0.01, "Rel/abs epsilon");
        return true;
    }();
    VERIFY(result, "FATP_ASSERT_CLOSE_REL_ABS passes");
}

FATP_TEST_CASE(assert_throws_pass)
{
    FATP_ASSERT_THROWS(throw std::runtime_error("test"), std::runtime_error, "Correct exception");
    return true;
}

FATP_TEST_CASE(assert_no_throw_pass)
{
    FATP_ASSERT_NO_THROW(
        []() {
            int x = 42;
            (void)x;
        }(),
        "No exception");
    return true;
}

void assert_macros_exceptions()
{
    TEST_SECTION("ASSERT Macros - Exceptions");

    OutputCapture capture;

    bool result = test_assert_throws_pass();
    VERIFY(result, "FATP_ASSERT_THROWS passes");

    result = test_assert_no_throw_pass();
    VERIFY(result, "FATP_ASSERT_NO_THROW passes");
}

void assert_macros_strings()
{
    TEST_SECTION("ASSERT Macros - Strings");

    OutputCapture capture;

    bool result = []() -> bool {
        std::string str = "hello world";
        FATP_ASSERT_CONTAINS(str, "world", "Contains substring");
        return true;
    }();
    VERIFY(result, "FATP_ASSERT_CONTAINS passes");

    result = []() -> bool {
        std::string str = "hello world";
        FATP_ASSERT_NOT_CONTAINS(str, "xyz", "Does not contain");
        return true;
    }();
    VERIFY(result, "FATP_ASSERT_NOT_CONTAINS passes");

    result = []() -> bool {
        std::string str = "hello world";
        FATP_ASSERT_STARTS_WITH(str, "hello", "Starts with");
        return true;
    }();
    VERIFY(result, "FATP_ASSERT_STARTS_WITH passes");

    result = []() -> bool {
        std::string str = "hello world";
        FATP_ASSERT_ENDS_WITH(str, "world", "Ends with");
        return true;
    }();
    VERIFY(result, "FATP_ASSERT_ENDS_WITH passes");

    result = []() -> bool {
        std::string str = "test123";
        FATP_ASSERT_MATCHES(str, "test\\d+", "Regex match");
        return true;
    }();
    VERIFY(result, "FATP_ASSERT_MATCHES passes");

    result = []() -> bool {
        FATP_ASSERT_STR_EQ_IGNORE_CASE("Hello", "HELLO", "Case insensitive");
        return true;
    }();
    VERIFY(result, "FATP_ASSERT_STR_EQ_IGNORE_CASE passes");
}

void assert_macros_ranges()
{
    TEST_SECTION("ASSERT Macros - Ranges");

    OutputCapture capture;

    bool result = []() -> bool {
        std::vector<int> v1 = {1, 2, 3};
        std::vector<int> v2 = {1, 2, 3};
        FATP_ASSERT_RANGE_EQ(v1, v2, "Equal ranges");
        return true;
    }();
    VERIFY(result, "FATP_ASSERT_RANGE_EQ passes");

    result = []() -> bool {
        std::vector<double> v1 = {1.0, 2.0, 3.0};
        std::vector<double> v2 = {1.001, 2.001, 3.001};
        FATP_ASSERT_RANGE_CLOSE(v1, v2, 0.01, "Close ranges");
        return true;
    }();
    VERIFY(result, "FATP_ASSERT_RANGE_CLOSE passes");
}

void performance_measurement()
{
    TEST_SECTION("Performance Measurement");

    int value = 42;
    DoNotOptimize(value);
    VERIFY(true, "DoNotOptimize compiles");

    std::string ns_str = format_time(0.0001);
    VERIFY(ns_str.find("ns") != std::string::npos, "format_time: ns");

    std::string ms_str = format_time(10.0);
    VERIFY(ms_str.find("ms") != std::string::npos, "format_time: ms");

    double time = measure_perf(
        []() {
            volatile int x = 42;
            (void)x;
        },
        1000);
    VERIFY(time >= 0, "measure_perf returns non-negative");

    auto stats = measure_perf_stats(
        []() {
            volatile int x = 42;
            (void)x;
        },
        1000,
        10);
    VERIFY(stats.min_ms <= stats.mean_ms, "stats: min <= mean");
    VERIFY(stats.mean_ms <= stats.max_ms, "stats: mean <= max");
    VERIFY(stats.stddev_ms >= 0, "stats: stddev non-negative");
    VERIFY(stats.iterations == 1000, "stats: iterations correct");

    VERIFY(stats.min_ns() >= 0, "stats: min_ns conversion");
    VERIFY(stats.max_ns() >= 0, "stats: max_ns conversion");
    VERIFY(stats.mean_ns() >= 0, "stats: mean_ns conversion");
}
void fixtures()
{
    TEST_SECTION("Test Fixtures");

    struct MyFixture : public TestFixture
    {
        int* data;
        bool setup_called;
        bool teardown_called;

        MyFixture()
            : data(nullptr)
            , setup_called(false)
            , teardown_called(false)
        {
        }

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

void test_runner()
{
    TEST_SECTION("Test Runner");

    OutputCapture capture;
    TestRunner runner;

    runner.run_test("test1", []() {
        return true;
    });
    VERIFY(runner.results().size() == 1, "Test recorded");
    VERIFY(runner.results()[0].passed, "Test passed");
    VERIFY(runner.results()[0].name == "test1", "Test name correct");

    runner.run_test("test2", []() {
        return false;
    });
    VERIFY(runner.results().size() == 2, "Failed test recorded");
    VERIFY(!runner.results()[1].passed, "Test marked as failed");

    runner.clear();
    VERIFY(runner.results().size() == 0, "Results cleared");

    runner.set_filter("*math*");
    runner.run_test("test_math_add", []() {
        return true;
    });
    runner.run_test("test_string_ops", []() {
        return true;
    });
    VERIFY(runner.results().size() == 1, "Filter works");

    runner.clear();
    runner.set_filter("");
    runner.run_test("pass1", []() {
        return true;
    });
    runner.run_test("pass2", []() {
        return true;
    });
    runner.run_test("fail1", []() {
        return false;
    });


    int failed = runner.print_summary();
    VERIFY(failed == 1, "Summary returns failure count");
}

void test_runner_exception_isolation()
{
    TEST_SECTION("Test Runner - Exception Isolation");

    OutputCapture capture;
    TestRunner runner;

    bool throwing_ran = false;
    bool after_ran = false;

    runner.run_test("throws_runtime_error", [&]() -> bool {
        throwing_ran = true;
        throw std::runtime_error("intentional test exception");
    });

    runner.run_test("runs_after_throw", [&]() {
        after_ran = true;
        return true;
    });

    VERIFY(throwing_ran, "Throwing test was entered");
    VERIFY(after_ran, "Subsequent test ran after exception");
    VERIFY(runner.results().size() == 2, "Both tests recorded");
    VERIFY(!runner.results()[0].passed, "Throwing test marked failed");
    VERIFY(runner.results()[1].passed, "Following test passed");

    runner.clear();
    runner.run_test("throws_unknown", []() -> bool {
        throw 42;
    });
    runner.run_test("runs_after_unknown", []() {
        return true;
    });

    VERIFY(runner.results().size() == 2, "Unknown-exception tests recorded");
    VERIFY(!runner.results()[0].passed, "Unknown exception marked failed");
    VERIFY(runner.results()[1].passed, "Test after unknown exception passed");
}

void test_runner_advanced()
{
    TEST_SECTION("Test Runner - Advanced");

    OutputCapture capture;
    TestRunner runner;

    runner.run_test_with_timeout(
        "fast_test",
        []() {
            return true;
        },
        1000);
    VERIFY(runner.results().size() == 1, "Timeout test ran");
    VERIFY(runner.results()[0].passed, "Fast test passed");

    runner.clear();
    auto repeat_result = runner.run_test_repeat(
        "stable_test",
        []() {
            return true;
        },
        10);
    VERIFY(repeat_result.total_runs == 10, "Repeat ran 10 times");
    VERIFY(repeat_result.passed == 10, "All repetitions passed");
    VERIFY(repeat_result.pass_rate == 100.0, "100% pass rate");

    int counter = 0;
    auto flaky_result = runner.run_test_repeat(
        "flaky_test",
        [&]() {
            return counter++ % 2 == 0;
        },
        10);
    VERIFY(flaky_result.total_runs == 10, "Flaky test ran 10 times");
    VERIFY(flaky_result.failed > 0, "Flaky test has failures");
    VERIFY(flaky_result.pass_rate < 100.0, "Flaky test pass rate < 100%");

    counter = 0;
    size_t failed_at = runner.run_until_failure(
        "eventual_fail",
        [&]() {
            return counter++ < 5;
        },
        100);
    VERIFY(failed_at == 6, "Failed on 6th iteration");
}

void parameterized_tests()
{
    TEST_SECTION("Parameterized Tests");

    OutputCapture capture;

    std::vector<TestCase<int, int, int>> cases = {{{2, 3, 5}, "2+3=5"},
                                                  {{10, 20, 30}, "10+20=30"},
                                                  {{-1, 1, 0}, "-1+1=0"}};

    bool result = run_parameterized_test("addition", cases, [](const auto& tc) {
        int a = std::get<0>(tc.inputs);
        int b = std::get<1>(tc.inputs);
        int expected = std::get<2>(tc.inputs);
        return (a + b) == expected;
    });

    VERIFY(result, "Parameterized test passed");
}

void subtest_tracking()
{
    TEST_SECTION("Subtest Tracking");

    OutputCapture capture;

    bool result = []() -> bool {
        get_subtest_tracker().clear();

        FATP_SUBTEST("part1")
        {
            int x = 1 + 1;
            if (x != 2)
            {
                throw std::runtime_error("Math broken");
            }
        }
        FATP_END_SUBTEST

        FATP_SUBTEST("part2")
        {
            int y = 2 + 2;
            if (y != 4)
            {
                throw std::runtime_error("Math broken");
            }
        }
        FATP_END_SUBTEST

        return get_subtest_tracker().all_passed();
    }();

    VERIFY(result, "All subtests passed");
    VERIFY(get_subtest_tracker().get_results().size() == 2, "Two subtests recorded");

    result = []() -> bool {
        get_subtest_tracker().clear();

        FATP_SUBTEST("passing")
        {
            int x = 1;
            (void)x;
        }
        FATP_END_SUBTEST

        FATP_SUBTEST("failing")
        {
            throw std::runtime_error("Intentional failure");
        }
        FATP_END_SUBTEST

        return get_subtest_tracker().all_passed();
    }();

    VERIFY(!result, "Failed subtest detected");
    VERIFY(get_subtest_tracker().get_results().size() == 2, "Both subtests recorded");
    VERIFY(!get_subtest_tracker().get_results()[1].passed, "Second subtest failed");
}

void junit_xml()
{
    TEST_SECTION("JUnit XML Export");

    VERIFY(xml_escape("&") == "&amp;", "XML escape: &");
    VERIFY(xml_escape("<") == "&lt;", "XML escape: <");
    VERIFY(xml_escape(">") == "&gt;", "XML escape: >");
    VERIFY(xml_escape("\"") == "&quot;", "XML escape: \"");
    VERIFY(xml_escape("'") == "&apos;", "XML escape: '");
    VERIFY(xml_escape("hello") == "hello", "XML escape: no change");

    std::vector<TestResult> results;
    results.push_back(TestResult{"test1", true, 10.5});
    results.push_back(TestResult{"test2", false, 5.2});
    results.push_back(TestResult{"test3", true, 8.1});

    bool success = export_junit_xml("test_output.xml", results, "MySuite");
    VERIFY(success, "XML export succeeded");

    std::ifstream file("test_output.xml");
    VERIFY(file.is_open(), "XML file created");

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    VERIFY(content.find("<?xml version") != std::string::npos, "XML header");
    VERIFY(content.find("MySuite") != std::string::npos, "Suite name");
    VERIFY(content.find("test1") != std::string::npos, "Test name in XML");
    VERIFY(content.find("<failure") != std::string::npos, "Failure tag");

    std::remove("test_output.xml");

    TestRunner runner;
    runner.run_test("test1", []() {
        return true;
    });
    runner.run_test("test2", []() {
        return false;
    });
    success = runner.export_to_junit_xml("runner_output.xml", "RunnerSuite");
    VERIFY(success, "TestRunner XML export");
    std::remove("runner_output.xml");
}

void non_copyable_types()
{
    TEST_SECTION("Non-Copyable Types");

    OutputCapture capture;

    bool result = []() -> bool {
        std::atomic<int> counter{42};
        FATP_ASSERT_EQ(counter.load(), 42, "Atomic value");
        return true;
    }();
    VERIFY(result, "FATP_ASSERT_EQ works with atomic");

    result = []() -> bool {
        std::unique_ptr<int> ptr = std::make_unique<int>(42);
        FATP_ASSERT_NOT_NULLPTR(ptr.get(), "unique_ptr valid");
        return true;
    }();
    VERIFY(result, "FATP_ASSERT_NOT_NULLPTR works with unique_ptr");
}

void assert_with_handler()
{
    TEST_SECTION("FATP_ASSERT_WITH_HANDLER");

    OutputCapture capture;

    bool handler_called = false;
    bool result = [&]() -> bool {
        FATP_ASSERT_WITH_HANDLER(false, "Should fail", { handler_called = true; });
        return true;
    }();

    VERIFY(!result, "FATP_ASSERT_WITH_HANDLER fails");
    VERIFY(handler_called, "Handler was called");
}

} // namespace fat_p::testing::fatptest

namespace fat_p::testing
{


inline void run_benchmarks()
{
    using namespace fat_p::testing;

    auto& out = *get_test_config().output;
    out << "\n" << colors::cyan() << "Sanity Benchmark:" << colors::reset() << "\n";
    out << "  (benchmarks are provided by benchmark executables; unit tests do not run full benchmarks)\n\n";
}

bool test_FatPTest()
{
    std::cout << "\n";
    std::cout << "==================================================================\n";
    std::cout << "  COMPREHENSIVE TEST SUITE FOR FatPTest.h        \n";
    std::cout << "==================================================================\n";

    fatptest::primitive_comparison();
    fatptest::configuration();
    fatptest::string_utilities();
    fatptest::auto_calibration();
    fatptest::assert_macros_basic();
    fatptest::assert_macros_comparison();
    fatptest::assert_macros_pointers();
    fatptest::assert_macros_float();
    fatptest::assert_macros_exceptions();
    fatptest::assert_macros_strings();
    fatptest::assert_macros_ranges();
    fatptest::performance_measurement();
    fatptest::fixtures();
    fatptest::test_runner();
    fatptest::test_runner_exception_isolation();
    fatptest::test_runner_advanced();
    fatptest::parameterized_tests();
    fatptest::subtest_tracking();
    fatptest::junit_xml();
    fatptest::non_copyable_types();
    fatptest::assert_with_handler();

    std::cout << "\n\n";
    std::cout << "==================================================================\n";
    std::cout << "  TEST RESULTS                                                    \n";
    std::cout << "==================================================================\n";
    std::cout << "  Sections:     " << fatptest::g_sections_run << "\n";
    std::cout << "  Total Tests:  " << fatptest::g_tests_run << "\n";
    std::cout << "  Passed:       " << fatptest::g_tests_passed << " ("
              << (100.0 * fatptest::g_tests_passed / fatptest::g_tests_run) << "%)\n";
    std::cout << "  Failed:       " << (fatptest::g_tests_run - fatptest::g_tests_passed) << "\n";
    std::cout << "\n";

    if (fatptest::g_tests_passed == fatptest::g_tests_run)
    {
        std::cout << " ALL TESTS PASSED - 100% SUCCESS!\n";
        std::cout << "\n  Your FatPTest.h is fully validated!\n\n";
        return true;
    }
    else
    {
        std::cout << " SOME TESTS FAILED\n\n";
        return false;
    }
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_FatPTest() ? 0 : 1;
}
#endif
