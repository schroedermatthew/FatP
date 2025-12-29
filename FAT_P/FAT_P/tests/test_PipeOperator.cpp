/**
 * @file test_PipeOperator.cpp
 * @brief Comprehensive unit tests for PipeOperator.h
 */

#include <iostream>
#include <string>

#include "Expected.h"
#include "PipeOperator.h"
#include "FatPTest.h"

namespace fat_p::testing
{

bool test_basic_pipe() {
    auto add_ten = [](int x) { return x + 10; };
    auto multiply_two = [](int x) { return x * 2; };
    
    int result = 5 | add_ten | multiply_two;
    
    ASSERT_TRUE(result == 30, "Result should be 30 ((5+10)*2)");
    
    return true;
}

bool test_string_pipe() {
    auto to_upper = [](std::string s) {
        for (auto& c : s) c = std::toupper(c);
        return s;
    };
    auto add_exclamation = [](std::string s) { return s + "!"; };
    
    std::string result = std::string("hello") | to_upper | add_exclamation;
    
    ASSERT_TRUE(result == "HELLO!", "Result should be 'HELLO!'");
    
    return true;
}

bool test_expected_success() {
    auto add_ten = [](int x) -> Expected<int, std::string> { return x + 10; };
    auto multiply_two = [](int x) -> Expected<int, std::string> { return x * 2; };
    
    auto result = Expected<int, std::string>(5) | add_ten | multiply_two;
    
    ASSERT_TRUE(result.has_value(), "Result should have value");
    ASSERT_TRUE(*result == 30, "Result should be 30");
    
    return true;
}

bool test_expected_error() {
    auto add_ten = [](int x) -> Expected<int, std::string> { return x + 10; };
    auto fail = [](int) -> Expected<int, std::string> { 
        return unexpected<std::string>("error occurred");
    };
    auto multiply_two = [](int x) -> Expected<int, std::string> { return x * 2; };
    
    auto result = Expected<int, std::string>(5) | add_ten | fail | multiply_two;
    
    ASSERT_TRUE(!result.has_value(), "Result should be error");
    ASSERT_TRUE(result.error() == "error occurred", "Error message should be preserved");
    
    return true;
}

bool test_type_conversion() {
    auto int_to_string = [](int x) { return std::to_string(x); };
    auto append_text = [](std::string s) { return s + " units"; };
    
    std::string result = 42 | int_to_string | append_text;
    
    ASSERT_TRUE(result == "42 units", "Result should be '42 units'");
    
    return true;
}

bool test_complex_chain() {
    auto step1 = [](int x) -> Expected<int, std::string> { return x * 2; };
    auto step2 = [](int x) -> Expected<int, std::string> { return x + 5; };
    auto step3 = [](int x) -> Expected<int, std::string> { return x - 3; };
    
    auto result = Expected<int, std::string>(10) | step1 | step2 | step3;
    
    ASSERT_TRUE(result.has_value(), "Result should have value");
    ASSERT_TRUE(*result == 22, "Result should be 22 ((10*2)+5-3)");
    
    return true;
}

bool test_void_to_value() {
    // Expected<void> success -> value producing function
    Expected<void, std::string> success;
    auto produce_int = []() { return 42; };
    
    auto result = std::move(success) | produce_int;
    
    ASSERT_TRUE(result.has_value(), "Void-to-value should succeed");
    ASSERT_TRUE(*result == 42, "Should contain 42");
    return true;
}

bool test_void_to_expected() {
    // Expected<void> success -> Expected producing function
    Expected<void, std::string> success;
    auto produce_expected = []() -> Expected<int, std::string> { return 100; };
    
    auto result = std::move(success) | produce_expected;
    
    ASSERT_TRUE(result.has_value(), "Void-to-expected should succeed");
    ASSERT_TRUE(*result == 100, "Should contain 100");
    return true;
}

bool test_void_error_propagation() {
    // Expected<void> error -> any function (should propagate error)
    Expected<void, std::string> failure(unexpected<std::string>("void error"));
    auto produce_int = []() { return 42; };
    
    auto result = std::move(failure) | produce_int;
    
    ASSERT_TRUE(!result.has_value(), "Error should propagate");
    ASSERT_TRUE(result.error() == "void error", "Error message preserved");
    return true;
}

bool test_void_chain() {
    // Chain: void -> void -> value
    int side_effect = 0;
    Expected<void, std::string> start;
    
    auto step1 = [&side_effect]() { side_effect = 1; };
    auto step2 = [&side_effect]() { side_effect += 10; };
    auto step3 = [&side_effect]() { return side_effect * 2; };
    
    auto result = std::move(start) | step1 | step2 | step3;
    
    ASSERT_TRUE(result.has_value(), "Chain should succeed");
    ASSERT_TRUE(*result == 22, "Should be (1+10)*2 = 22");
    ASSERT_TRUE(side_effect == 11, "Side effects should have run");
    return true;
}

bool test_void_to_void() {
    // Expected<void> -> void function -> Expected<void>
    int counter = 0;
    Expected<void, std::string> start;
    
    auto increment = [&counter]() { counter++; };
    
    auto result = std::move(start) | increment;
    
    ASSERT_TRUE(result.has_value(), "Void-to-void should succeed");
    ASSERT_TRUE(counter == 1, "Side effect should have run");
    return true;
}

bool test_const_void_pipe() {
    // Const lvalue void piping
    const Expected<void, std::string> success;
    auto produce = []() { return 99; };
    
    auto result = success | produce;
    
    ASSERT_TRUE(result.has_value(), "Const void pipe should work");
    ASSERT_TRUE(*result == 99, "Should contain 99");
    return true;
}

// Simple error code enum for tests where string error would cause T==E
enum class TestError { None, Invalid, NotFound };

bool test_mixed_pipeline() {
    // Real-world scenario: status -> config -> validation
    auto init = []() -> Expected<void, TestError> { return {}; };
    auto get_config = []() -> Expected<int, TestError> { return 42; };
    auto validate = [](int x) -> Expected<int, TestError> {
        if (x > 0) return x * 2;
        return unexpected<TestError>(TestError::Invalid);
    };
    auto format = [](int x) { return std::to_string(x) + " units"; };
    
    auto result = init() | get_config | validate | format;
    
    ASSERT_TRUE(result.has_value(), "Mixed pipeline should succeed");
    ASSERT_TRUE(*result == "84 units", "Should be formatted result");
    return true;
}

bool test_pipe_wrapper() {
    // Test explicit pipe() wrapper for disambiguation
    Expected<int, std::string> exp(5);
    auto double_it = [](int x) { return x * 2; };
    
    auto result = pipe(std::move(exp)) | double_it;
    
    ASSERT_TRUE(result.has_value(), "Pipe wrapper should work");
    ASSERT_TRUE(*result == 10, "Should be 10");
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
    RUN_TEST(runner, void_to_value);
    RUN_TEST(runner, void_to_expected);
    RUN_TEST(runner, void_error_propagation);
    RUN_TEST(runner, void_chain);
    RUN_TEST(runner, void_to_void);
    RUN_TEST(runner, const_void_pipe);
    RUN_TEST(runner, mixed_pipeline);
    RUN_TEST(runner, pipe_wrapper);

    benchmark_pipeoperator();

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_PipeOperator() ? 0 : 1;
}
#endif
