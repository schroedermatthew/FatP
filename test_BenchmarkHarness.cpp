
#include <array>
#include <iostream>
#include <vector>

#include "BenchmarkHarness.h"
#include "test_BenchmarkHarness.h"
#include "test_Utilities.h"

namespace cpp_utilities::testing
{

bool test_benchmark_harness_basic_benchmark() {
    BenchmarkHarness harness("Test Suite");
    
    harness.add_benchmark("Simple", []() {
        volatile int x = 0;
        x = x + 1;
    });
    
    auto results = harness.run();
    
    SIMPLE_ASSERT(results.size() == 1, "Should have 1 result");
    SIMPLE_ASSERT(results[0].name == "Simple", "Name should match");
    SIMPLE_ASSERT(results[0].samples > 0, "Should have samples");
    SIMPLE_ASSERT(results[0].mean_ns > 0, "Should have mean time");
    
    return true;
}

bool test_benchmark_harness_multiple_benchmarks() {
    BenchmarkHarness harness("Multiple Tests");
    
    harness.add_benchmark("Fast", []() {
        volatile int x = 1;
    });
    
    harness.add_benchmark("Slow", []() {
        volatile int sum = 0;
        for (int i = 0; i < 100; ++i) {
            sum += i;
        }
    });
    
    auto results = harness.run();
    
    SIMPLE_ASSERT(results.size() == 2, "Should have 2 results");
    SIMPLE_ASSERT(results[1].mean_ns > results[0].mean_ns, "Slow should be slower");
    
    return true;
}

bool test_benchmark_harness_statistics() {
    BenchmarkConfig config;
    config.min_samples = 100;
    config.filter_outliers = true;
    
    BenchmarkHarness harness("Stats Test", config);
    
    harness.add_benchmark("Consistent", []() {
        volatile int x = 42;
    });
    
    auto results = harness.run();
    
    SIMPLE_ASSERT(results[0].samples >= 100, "Should have min samples");
    SIMPLE_ASSERT(results[0].min_ns <= results[0].median_ns, "Min should be <= median");
    SIMPLE_ASSERT(results[0].median_ns <= results[0].max_ns, "Median should be <= max");
    SIMPLE_ASSERT(results[0].p95_ns >= results[0].median_ns, "P95 should be >= median");
    
    return true;
}

bool test_benchmark_harness_comparison() {
    BenchmarkHarness harness("Comparison Test");
    
    harness.add_benchmark("Vector Reserve", []() {
        std::vector<int> v;
        v.reserve(1000);
        for (int i = 0; i < 1000; ++i) {
            v.push_back(i);
        }
    });
    
    harness.add_benchmark("Vector No Reserve", []() {
        std::vector<int> v;
        for (int i = 0; i < 1000; ++i) {
            v.push_back(i);
        }
    });
    
    auto results = harness.run();
    
    SIMPLE_ASSERT(results.size() == 2, "Should have 2 results");
    // Reserve should generally be faster
    SIMPLE_ASSERT(results[0].mean_ns < results[1].mean_ns * 2, "Reserve should help");
    
    return true;
}

void demo_benchmark_harness() {
    std::cout << "\n" << colors::cyan() << "BenchmarkHarness Demo:" << colors::reset() << "\n\n";
    
    BenchmarkHarness harness("Algorithm Comparison");
    
    // Add various algorithms to compare
    harness.add_benchmark("std::vector::push_back", []() {
        std::vector<int> v;
        v.reserve(100);
        for (int i = 0; i < 100; ++i) {
            v.push_back(i);
        }
    });
    
    harness.add_benchmark("std::vector::emplace_back", []() {
        std::vector<int> v;
        v.reserve(100);
        for (int i = 0; i < 100; ++i) {
            v.emplace_back(i);
        }
    });
    
    harness.add_benchmark("std::array fill", []() {
        std::array<int, 100> arr;
        for (int i = 0; i < 100; ++i) {
            arr[i] = i;
        }
    });
    
    harness.run();
    harness.print_report();
}

bool test_BenchmarkHarness() {

    PRINT_HEADER(BENCHMARK HARNESS)

    TestRunner runner;

    RUN_TEST(runner, benchmark_harness_basic_benchmark);
    RUN_TEST(runner, benchmark_harness_multiple_benchmarks);
    RUN_TEST(runner, benchmark_harness_statistics);
    RUN_TEST(runner, benchmark_harness_comparison);

    demo_benchmark_harness();

    return 0 == runner.print_summary();

}

} // namespace cpp_utilities::testing
