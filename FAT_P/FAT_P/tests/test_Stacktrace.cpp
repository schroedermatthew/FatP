/**
 * @file test_Stacktrace.cpp
 * @brief Comprehensive unit tests for Stacktrace.h
 */
/*
FATP_META:
  meta_version: 1
  component: Stacktrace
  file_role: test
  path: tests/test_Stacktrace.cpp
  namespace: fat_p
  summary: "Unit tests for Stacktrace."
  related:
    docs_search: "Stacktrace"
    headers:
      - fat_p/Stacktrace.h
      - fat_p/FatPTest.h
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/

#include <iostream>

#include "Stacktrace.h"
#include "FatPTest.h"

namespace fat_p::testing::stacktrace
{

TEST_CASE(current)
{
    auto trace = Stacktrace::current();
    
    ASSERT_FALSE(trace.frames().empty(), "Stacktrace should have frames");
    
    return true;
}

TEST_CASE(to_string)
{
    auto trace = Stacktrace::current();
    std::string str = trace.to_string();
    
    ASSERT_FALSE(str.empty(), "Stacktrace string should not be empty");
    
    return true;
}

TEST_CASE(frames)
{
    auto trace = Stacktrace::current();
    const auto& frames = trace.frames();
    
    ASSERT_GT(frames.size(), 0u, "Should have at least one frame");
    
    const auto& frame = frames[0];
    ASSERT_FALSE(frame.function.empty(), "Frame should have function name");
    
    return true;
}

void benchmark_stacktrace()
{
    std::cout << "\n" << colors::cyan() << "Stacktrace Benchmarks:" << colors::reset() << "\n\n";
    
    std::cout << "Note: Stacktrace uses placeholder implementation\n";
    std::cout << "Platform-specific unwinding needs to be implemented\n\n";
    
    // Benchmark current()
    double current_time = measure_perf([]() {
        auto trace = Stacktrace::current();
        DoNotOptimize(trace);
    }, 1000, 10);
    std::cout << "Current stacktrace: " << format_time(current_time) << "\n";
    
    // Benchmark to_string()
    auto trace = Stacktrace::current();
    double tostring_time = measure_perf([&trace]() {
        std::string str = trace.to_string();
        DoNotOptimize(str);
    }, 10000, 100);
    std::cout << "To string: " << format_time(tostring_time) << "\n";
}

} // namespace fat_p::testing::stacktrace

namespace fat_p::testing
{

bool test_Stacktrace()
{
    PRINT_HEADER(STACK TRACE)

    TestRunner runner;

    RUN_TEST_NS(runner, stacktrace, current);
    RUN_TEST_NS(runner, stacktrace, to_string);
    RUN_TEST_NS(runner, stacktrace, frames);

    stacktrace::benchmark_stacktrace();

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_Stacktrace() ? 0 : 1;
}
#endif
