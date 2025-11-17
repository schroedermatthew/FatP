#include <iostream>
#include <string>

#include "Expected.h"
#include "PipeOperator.h"
#include "test_PipeOperator.h"
#include "FatPTest.h"

namespace fat_p::testing
{

bool test_basic_pipe() {
    auto add_ten = [](int x) { return x + 10; };
    auto multiply_two = [](int x) { return x * 2; };
    
    int result = 5 | add_ten | multiply_two;
    
    SIMPLE_ASSERT(result == 30, "Result should be 30 ((5+10)*2)");
    
    return true;
}

bool test_string_pipe() {
    auto to_upper = [](std::string s) {
        for (auto& c : s) c = std::toupper(c);
        return s;
    };
    auto add_exclamation = [](std::string s) { return s + "!"; };
    
    std::string result = std::string("hello") | to_upper | add_exclamation;
    
    SIMPLE_ASSERT(result == "HELLO!", "Result should be 'HELLO!'");
    
    return true;
}

bool test_expected_success() {
    auto add_ten = [](int x) -> Expected<int, std::string> { return x + 10; };
    auto multiply_two = [](int x) -> Expected<int, std::string> { return x * 2; };
    
    auto result = Expected<int, std::string>(5) | add_ten | multiply_two;
    
    SIMPLE_ASSERT(result.has_value(), "Result should have value");
    SIMPLE_ASSERT(*result == 30, "Result should be 30");
    
    return true;
}

bool test_expected_error() {
    auto add_ten = [](int x) -> Expected<int, std::string> { return x + 10; };
    auto fail = [](int) -> Expected<int, std::string> { 
        return unexpected<std::string>("error occurred");
    };
    auto multiply_two = [](int x) -> Expected<int, std::string> { return x * 2; };
    
    auto result = Expected<int, std::string>(5) | add_ten | fail | multiply_two;
    
    SIMPLE_ASSERT(!result.has_value(), "Result should be error");
    SIMPLE_ASSERT(result.error() == "error occurred", "Error message should be preserved");
    
    return true;
}

bool test_type_conversion() {
    auto int_to_string = [](int x) { return std::to_string(x); };
    auto append_text = [](std::string s) { return s + " units"; };
    
    std::string result = 42 | int_to_string | append_text;
    
    SIMPLE_ASSERT(result == "42 units", "Result should be '42 units'");
    
    return true;
}

bool test_complex_chain() {
    auto step1 = [](int x) -> Expected<int, std::string> { return x * 2; };
    auto step2 = [](int x) -> Expected<int, std::string> { return x + 5; };
    auto step3 = [](int x) -> Expected<int, std::string> { return x - 3; };
    
    auto result = Expected<int, std::string>(10) | step1 | step2 | step3;
    
    SIMPLE_ASSERT(result.has_value(), "Result should have value");
    SIMPLE_ASSERT(*result == 22, "Result should be 22 ((10*2)+5-3)");
    
    return true;
}

void benchmark_pipeoperator() {
    std::cout << "\n" << colors::cyan() << "PipeOperator Benchmarks:" << colors::reset() << "\n\n";
    
    auto add_ten = [](int x) { return x + 10; };
    auto multiply_two = [](int x) { return x * 2; };
    
    // Benchmark basic pipe
    double pipe_time = measure_perf([&add_ten, &multiply_two, i=0]() mutable {
        int result = i | add_ten | multiply_two;
        DoNotOptimize(result);
        ++i;
    }, 100000, 1000);
    std::cout << "Basic pipe: " << format_time(pipe_time) << "\n";
    
    // Benchmark Expected pipe
    auto add_ten_exp = [](int x) -> Expected<int, std::string> { return x + 10; };
    auto multiply_two_exp = [](int x) -> Expected<int, std::string> { return x * 2; };
    
    double exp_time = measure_perf([&add_ten_exp, &multiply_two_exp, i=0]() mutable {
        auto result = Expected<int, std::string>(i) | add_ten_exp | multiply_two_exp;
        DoNotOptimize(result);
        ++i;
    }, 100000, 1000);
    std::cout << "Expected pipe: " << format_time(exp_time) << "\n";
}

bool test_PipeOperator() {

    PRINT_HEADER(PIPE OPERATOR)

    TestRunner runner;

    RUN_TEST(runner, basic_pipe);
    RUN_TEST(runner, string_pipe);
    RUN_TEST(runner, expected_success);
    RUN_TEST(runner, expected_error);
    RUN_TEST(runner, type_conversion);
    RUN_TEST(runner, complex_chain);

    benchmark_pipeoperator();

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing
