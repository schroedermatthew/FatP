#include <iostream>

#include "Stacktrace.h"
#include "test_Stacktrace.h"
#include "test_Utilities.h"

namespace cpp_utilities::testing
{

bool test_current() {
    auto trace = Stacktrace::current();
    
    SIMPLE_ASSERT(!trace.frames().empty(), "Stacktrace should have frames");
    
    return true;
}

bool test_to_string() {
    auto trace = Stacktrace::current();
    std::string str = trace.to_string();
    
    SIMPLE_ASSERT(!str.empty(), "Stacktrace string should not be empty");
    
    return true;
}

bool test_frames() {
    auto trace = Stacktrace::current();
    const auto& frames = trace.frames();
    
    SIMPLE_ASSERT(frames.size() > 0, "Should have at least one frame");
    
    const auto& frame = frames[0];
    SIMPLE_ASSERT(!frame.function.empty(), "Frame should have function name");
    
    return true;
}

void benchmark_stacktrace() {
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

bool test_Stacktrace() {

    PRINT_HEADER(STACK TRACE)

    TestRunner runner;

    RUN_TEST(runner, current);
    RUN_TEST(runner, to_string);
    RUN_TEST(runner, frames);

    benchmark_stacktrace();

    return 0 == runner.print_summary();
}

} // namespace cpp_utilities::testing
