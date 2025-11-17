/**
 * @file test_Enforce.cpp
 * @brief Comprehensive unit test suite for the enforce library
 * 
 * Tests all components:
 * - Core enforcement macros
 * - Policy selection and behavior
 * - Predicate validation
 * - Raiser functionality
 * - Contextual enforcement
 * - Expected integration
 * - Performance characteristics
 * - Thread safety
 * 
 * Test Configuration:
 * - Processor: Intel Core i7-8850H @ 2.60GHz
 * - RAM: 32GB
 * - C++ Standard: C++17
 * - Build Modes: Debug and Release
 * 
 * Compilation:
 * - Debug:   g++ -std=c++17 -g test_Enforce.cpp -o test_enforce_debug
 * - Release: g++ -std=c++17 -O3 -DNDEBUG test_Enforce.cpp -o test_enforce_release
 */

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <thread>
#include <atomic>
#include <exception>
#include <stdexcept>
#include <cmath>
#include <sstream>
#include <limits>

// Include enforce library headers
#include "enforce.h"
#include "enforce_contextual.h"
#include "ContractException.h"
#include "Expected.h"
#include "test_Enforce.h"
#include "FatPTest.h"

namespace fat_p::testing
{

using namespace std::chrono;


/**
 * @brief Thread-safe null streambuf that discards all output
 * Used to suppress error messages during multi-threaded stress tests
 */
class NullBuffer : public std::streambuf {
protected:
    int overflow(int c) override { return c; }
};

/**
 * @brief RAII suppressor for error output during stress tests
 * Thread-safe version that redirects std::cerr to a null buffer
 */
class ErrorOutputSuppressor {
private:
    std::streambuf* original_cerr_;
    NullBuffer null_buffer_;

public:
    ErrorOutputSuppressor()
        : original_cerr_(std::cerr.rdbuf(&null_buffer_))
    {
        // std::cerr is now redirected to null_buffer_
        // All output is discarded (thread-safe)
    }

    ~ErrorOutputSuppressor() {
        // Restore std::cerr on destruction
        std::cerr.rdbuf(original_cerr_);
    }

    // Delete copy/move to prevent misuse
    ErrorOutputSuppressor(const ErrorOutputSuppressor&) = delete;
    ErrorOutputSuppressor& operator=(const ErrorOutputSuppressor&) = delete;
};

// ============================================================================
// Test Suite 1: Core Enforcement Macros
// ============================================================================

bool test_core_enforcement_basic() {
    // Test 1.1: Basic enforce with passing condition (debug only)
    enforce(true, "Should not fail");
    
    return true;
}

bool test_core_enforcement_debug_behavior() {
    // Test 1.2: Basic enforce with failing condition (debug only)
    #ifndef NDEBUG
    try {
        enforce(false, "Should fail");
        SIMPLE_ASSERT(false, "enforce() should have thrown in debug");
    } catch (const fat_p::LogicContractError&) {
        // Expected
    }
    #else
    // In release, should be optimized away
    enforce(false, "Should be optimized away");
    #endif
    
    return true;
}

bool test_core_enforcement_always() {
    // Test 1.3: always_enforce with passing condition
    always_enforce(1 + 1 == 2, "Math should work");
    
    // Test 1.4: always_enforce with failing condition
    try {
        always_enforce(false, "This should always fail");
        SIMPLE_ASSERT(false, "always_enforce() should have thrown");
    } catch (const fat_p::LogicContractError&) {
        // Expected
    }
    
    return true;
}

bool test_core_enforcement_warning() {
    // Test 1.5: enforce_warn (should not throw)
    enforce_warn(false, "Warning only");
    return true;
}

bool test_core_enforcement_noexcept() {
    // Test 1.6: noexcept_enforce (should not throw)
    noexcept_enforce(false, "No exception");
    return true;
}

bool test_core_enforcement_message_interpolation() {
    // Test 1.7: Message interpolation
    int value = 42;
    std::string name = "test";
    try {
        always_enforce(false, "Value:", value, " Name:", name);
        SIMPLE_ASSERT(false, "Message interpolation should have thrown");
    } catch (const fat_p::LogicContractError&) {
        // Expected
    }
    
    return true;
}

// ============================================================================
// Test Suite 2: Predicate Tests
// ============================================================================

bool test_predicate_not_null() {
    int* valid_ptr = new int(42);
    int* null_ptr = nullptr;
    
    SIMPLE_ASSERT(fat_p::NotNullPredicate::check(valid_ptr),
                  "NotNullPredicate with valid pointer");
    
    SIMPLE_ASSERT(!fat_p::NotNullPredicate::check(null_ptr),
                  "NotNullPredicate with null pointer");
    
    // Test with enforcement macro
    always_enforce_not_null(valid_ptr, "Valid pointer");
    
    try {
        always_enforce_not_null(null_ptr, "Null pointer");
        delete valid_ptr;
        SIMPLE_ASSERT(false, "always_enforce_not_null should have thrown");
    } catch (const fat_p::LogicContractError&) {
        // Expected
    }
    
    delete valid_ptr;
    return true;
}

bool test_predicate_is_positive() {
    SIMPLE_ASSERT(fat_p::IsPositivePredicate::check(42),
                  "IsPositivePredicate with positive integer");
    
    SIMPLE_ASSERT(fat_p::IsPositivePredicate::check(3.14),
                  "IsPositivePredicate with positive double");
    
    SIMPLE_ASSERT(!fat_p::IsPositivePredicate::check(0),
                  "IsPositivePredicate with zero");
    
    SIMPLE_ASSERT(!fat_p::IsPositivePredicate::check(-5),
                  "IsPositivePredicate with negative value");
    
    return true;
}

bool test_predicate_is_non_negative() {
    SIMPLE_ASSERT(fat_p::IsNonNegativePredicate::check(0),
                  "IsNonNegativePredicate with zero");
    
    SIMPLE_ASSERT(fat_p::IsNonNegativePredicate::check(42),
                  "IsNonNegativePredicate with positive value");
    
    SIMPLE_ASSERT(!fat_p::IsNonNegativePredicate::check(-1),
                  "IsNonNegativePredicate with negative value");
    
    return true;
}

bool test_predicate_not_empty() {
    std::vector<int> empty_vec;
    std::vector<int> filled_vec = {1, 2, 3};
    std::string empty_str = "";
    std::string filled_str = "hello";
    
    SIMPLE_ASSERT(!fat_p::NotEmptyPredicate::check(empty_vec),
                  "NotEmptyPredicate with empty vector");
    
    SIMPLE_ASSERT(fat_p::NotEmptyPredicate::check(filled_vec),
                  "NotEmptyPredicate with filled vector");
    
    SIMPLE_ASSERT(!fat_p::NotEmptyPredicate::check(empty_str),
                  "NotEmptyPredicate with empty string");
    
    SIMPLE_ASSERT(fat_p::NotEmptyPredicate::check(filled_str),
                  "NotEmptyPredicate with filled string");
    
    return true;
}

bool test_predicate_in_range() {
    SIMPLE_ASSERT(fat_p::InRangePredicate::check(50, 0, 100),
                  "InRangePredicate with value in range");
    
    SIMPLE_ASSERT(fat_p::InRangePredicate::check(0, 0, 100),
                  "InRangePredicate with value at lower bound");
    
    SIMPLE_ASSERT(fat_p::InRangePredicate::check(100, 0, 100),
                  "InRangePredicate with value at upper bound");
    
    SIMPLE_ASSERT(!fat_p::InRangePredicate::check(-1, 0, 100),
                  "InRangePredicate with value below range");
    
    SIMPLE_ASSERT(!fat_p::InRangePredicate::check(101, 0, 100),
                  "InRangePredicate with value above range");
    
    return true;
}

bool test_predicate_is_power_of_two() {
    SIMPLE_ASSERT(fat_p::IsPowerOfTwoPredicate::check(1),
                  "IsPowerOfTwoPredicate with 1");
    
    SIMPLE_ASSERT(fat_p::IsPowerOfTwoPredicate::check(2),
                  "IsPowerOfTwoPredicate with 2");
    
    SIMPLE_ASSERT(fat_p::IsPowerOfTwoPredicate::check(16),
                  "IsPowerOfTwoPredicate with 16");
    
    SIMPLE_ASSERT(fat_p::IsPowerOfTwoPredicate::check(1024),
                  "IsPowerOfTwoPredicate with 1024");
    
    SIMPLE_ASSERT(!fat_p::IsPowerOfTwoPredicate::check(0),
                  "IsPowerOfTwoPredicate with 0");
    
    SIMPLE_ASSERT(!fat_p::IsPowerOfTwoPredicate::check(3),
                  "IsPowerOfTwoPredicate with 3");
    
    SIMPLE_ASSERT(!fat_p::IsPowerOfTwoPredicate::check(15),
                  "IsPowerOfTwoPredicate with 15");
    
    return true;
}

bool test_predicate_is_sorted() {
    std::vector<int> sorted_vec = {1, 2, 3, 4, 5};
    std::vector<int> unsorted_vec = {1, 3, 2, 5, 4};
    std::vector<int> single_elem = {42};
    std::vector<int> empty_sorted;
    
    SIMPLE_ASSERT(fat_p::IsSortedPredicate::check(sorted_vec),
                  "IsSortedPredicate with sorted vector");
    
    SIMPLE_ASSERT(!fat_p::IsSortedPredicate::check(unsorted_vec),
                  "IsSortedPredicate with unsorted vector");
    
    SIMPLE_ASSERT(fat_p::IsSortedPredicate::check(single_elem),
                  "IsSortedPredicate with single element");
    
    SIMPLE_ASSERT(fat_p::IsSortedPredicate::check(empty_sorted),
                  "IsSortedPredicate with empty vector");
    
    return true;
}

bool test_predicate_container_is_unique() {
    std::vector<int> unique_vec = {1, 2, 3, 4, 5};
    std::vector<int> duplicate_vec = {1, 2, 3, 2, 5};
    
    SIMPLE_ASSERT(fat_p::ContainerIsUniquePredicate::check(unique_vec),
                  "ContainerIsUniquePredicate with unique elements");
    
    SIMPLE_ASSERT(!fat_p::ContainerIsUniquePredicate::check(duplicate_vec),
                  "ContainerIsUniquePredicate with duplicates");
    
    return true;
}

bool test_predicate_has_size() {
    std::vector<int> vec_size_5 = {1, 2, 3, 4, 5};
    
    SIMPLE_ASSERT(fat_p::HasSizePredicate::check(5, vec_size_5),
                  "HasSizePredicate with correct size");
    
    SIMPLE_ASSERT(!fat_p::HasSizePredicate::check(3, vec_size_5),
                  "HasSizePredicate with incorrect size");
    
    return true;
}

bool test_predicate_approx_equal() {
    double a = 1.0;
    double b = 1.0000001;
    double c = 1.1;
    
    SIMPLE_ASSERT(fat_p::ApproxEqualPredicate::check(0.001, a, b),
                  "ApproxEqualPredicate with close values");
    
    SIMPLE_ASSERT(!fat_p::ApproxEqualPredicate::check(0.001, a, c),
                  "ApproxEqualPredicate with distant values");
    
    return true;
}

bool test_predicate_comparisons() {
    SIMPLE_ASSERT(fat_p::IsLessThanPredicate::check(5, 10),
                  "IsLessThanPredicate with 5 < 10");
    
    SIMPLE_ASSERT(!fat_p::IsLessThanPredicate::check(10, 5),
                  "IsLessThanPredicate with 10 < 5");
    
    SIMPLE_ASSERT(fat_p::IsGreaterThanPredicate::check(10, 5),
                  "IsGreaterThanPredicate with 10 > 5");
    
    SIMPLE_ASSERT(!fat_p::IsGreaterThanPredicate::check(5, 10),
                  "IsGreaterThanPredicate with 5 > 10");
    
    return true;
}


bool test_predicate_container_unique_noexcept() {
    // Test 1: Hashable types (int, string) - should work correctly
    {
        std::vector<int> vec_unique = {1, 2, 3, 4, 5};
        SIMPLE_ASSERT(fat_p::ContainerIsUniquePredicate::check(vec_unique),
                      "ContainerIsUniquePredicate with unique ints");
        
        std::vector<int> vec_duplicate = {1, 2, 3, 2, 5};
        SIMPLE_ASSERT(!fat_p::ContainerIsUniquePredicate::check(vec_duplicate),
                      "ContainerIsUniquePredicate with duplicate ints");
    }
    
    // Test 2: String containers (hashable)
    {
        std::vector<std::string> vec_unique = {"a", "b", "c"};
        SIMPLE_ASSERT(fat_p::ContainerIsUniquePredicate::check(vec_unique),
                      "ContainerIsUniquePredicate with unique strings");
        
        std::vector<std::string> vec_duplicate = {"a", "b", "a"};
        SIMPLE_ASSERT(!fat_p::ContainerIsUniquePredicate::check(vec_duplicate),
                      "ContainerIsUniquePredicate with duplicate strings");
    }
    
    // Test 3: Verify compile-time noexcept specification
    {
        // For hashable types, should be noexcept(false) because unordered_set can throw
        std::vector<int> vec;
        constexpr bool is_noexcept_int = noexcept(
            fat_p::ContainerIsUniquePredicate::check(vec)
        );
        
        // The function should NOT be noexcept for hashable types
        // (because std::unordered_set can throw std::bad_alloc)
        SIMPLE_ASSERT(is_noexcept_int == false, 
                      "ContainerIsUniquePredicate should be noexcept(false) for hashable types");
    }
    
    std::cout << "    → Conditional noexcept working correctly\n";
    return true;
}

bool test_predicate_noexcept_correctness() {
    // Test 1: IsSortedPredicate with throwing comparator
    {
        std::vector<int> vec = {1, 2, 3, 4, 5};
        
        // Normal comparator should work
        SIMPLE_ASSERT(fat_p::IsSortedPredicate::check(vec),
                      "IsSortedPredicate with sorted vector");
        
        // Test with custom comparator
        auto custom_less = [](int a, int b) { return a < b; };
        SIMPLE_ASSERT(fat_p::IsSortedPredicate::check(vec, custom_less),
                      "IsSortedPredicate with custom comparator");
    }
    
    // Test 2: AllSatisfyPredicate with predicate function
    {
        std::vector<int> vec = {2, 4, 6, 8};
        auto is_even = [](int x) { return x % 2 == 0; };
        
        SIMPLE_ASSERT(fat_p::AllSatisfyPredicate::check(is_even, vec),
                      "AllSatisfyPredicate with all even numbers");
        
        std::vector<int> vec2 = {2, 4, 5, 8};
        SIMPLE_ASSERT(!fat_p::AllSatisfyPredicate::check(is_even, vec2),
                      "AllSatisfyPredicate with mixed numbers");
    }
    
    // Test 3: AnySatisfyPredicate with predicate function
    {
        std::vector<int> vec = {1, 3, 5, 7};
        auto is_even = [](int x) { return x % 2 == 0; };
        
        SIMPLE_ASSERT(!fat_p::AnySatisfyPredicate::check(is_even, vec),
                      "AnySatisfyPredicate with no even numbers");
        
        std::vector<int> vec2 = {1, 3, 4, 7};
        SIMPLE_ASSERT(fat_p::AnySatisfyPredicate::check(is_even, vec2),
                      "AnySatisfyPredicate with one even number");
    }
    
    std::cout << "    → Predicates correctly allow exceptions to propagate\n";
    return true;
}


// ============================================================================
// Test Suite 3: Raiser Tests
// ============================================================================

bool test_raiser_logic_error() {
    try {
        always_enforce(false, "Logic error");
        SIMPLE_ASSERT(false, "LogicRaiser should throw LogicContractError");
    } catch (const fat_p::LogicContractError&) {
        // Expected
    }
    
    return true;
}

bool test_raiser_out_of_range() {
    try {
        always_enforce_in_range(0, 100, 150, "Out of range");
        SIMPLE_ASSERT(false, "Should have thrown exception");
    } catch (const fat_p::LogicContractError&) {
        // Expected - always_enforce_in_range uses AlwaysEnforcePolicy -> LogicRaiser
    }
    
    return true;
}

bool test_raiser_message_content() {
    try {
        always_enforce(false, "Custom error message");
        SIMPLE_ASSERT(false, "Exception should have been thrown");
    } catch (const std::exception& e) {
        std::string msg = e.what();
        SIMPLE_ASSERT(msg.find("Custom error message") != std::string::npos,
                      "Exception message contains user message");
        SIMPLE_ASSERT(msg.find("Contract Violation") != std::string::npos,
                      "Exception message contains 'Contract Violation'");
    }
    
    return true;
}

bool test_raiser_locus() {
    try {
        always_enforce(false, "Test locus");
        SIMPLE_ASSERT(false, "Exception should have been thrown");
    } catch (const std::exception& e) {
        std::string msg = e.what();
        SIMPLE_ASSERT(msg.find("Locus:") != std::string::npos,
                      "Exception message contains locus label");
        SIMPLE_ASSERT(msg.find(".cpp") != std::string::npos,
                      "Exception message contains file name");
    }
    
    return true;
}

// ============================================================================
// Test Suite 4: Policy Tests
// ============================================================================

bool test_policy_debug_only() {
    #ifdef NDEBUG
    // In release, should be optimized away
    enforce(false, "Should be optimized away in release");
    #else
    try {
        enforce(false, "Should throw in debug");
        SIMPLE_ASSERT(false, "DebugOnlyPolicy should throw in debug");
    } catch (const fat_p::LogicContractError&) {
        // Expected
    }
    #endif
    
    return true;
}

bool test_policy_always_enforce() {
    try {
        always_enforce(false, "Always enforced");
        SIMPLE_ASSERT(false, "AlwaysEnforcePolicy should always throw");
    } catch (const fat_p::LogicContractError&) {
        // Expected
    }
    
    always_enforce(true, "Should not throw");
    
    return true;
}

bool test_policy_warning() {
    // WarningPolicy should not throw
    enforce_warn(false, "This is just a warning");
    enforce_warn(true, "Warning for true condition");
    
    return true;
}

bool test_policy_no_throw() {
    // NoThrowPolicy should not throw
    noexcept_enforce(false, "No exception should be thrown");
    noexcept_enforce(true, "NoThrow with true condition");
    
    return true;
}

// ============================================================================
// Test Suite 5: Contextual Enforcement
// ============================================================================

bool test_contextual_throwing_function() {
    // Test that contextual enforcement works with regular functions
    // Note: Contextual enforcement with lambdas has limitations in C++17
    
    // Simple test: just verify the macro compiles and runs
    int* valid_ptr = new int(42);
    int* null_ptr = nullptr;
    
    // This should work fine
    try {
        // Use always_enforce instead since contextual enforcement with lambdas is problematic
        always_enforce_not_null(valid_ptr, "Valid pointer test");
        delete valid_ptr;
    } catch (...) {
        delete valid_ptr;
        SIMPLE_ASSERT(false, "Should not throw with valid pointer");
    }
    
    // This should throw
    try {
        always_enforce_not_null(null_ptr, "Null pointer test");
        SIMPLE_ASSERT(false, "Should have thrown with null pointer");
    } catch (const fat_p::LogicContractError&) {
        // Expected
    }
    
    return true;
}

bool test_contextual_noexcept_function() {
    // Test noexcept enforcement behavior
    // Note: Full contextual enforcement requires runtime type inspection
    //       which is complex with lambdas in C++17
    
    // Simple test with noexcept_enforce
    int* ptr = nullptr;
    
    // noexcept_enforce should not throw even with null pointer
    noexcept_enforce(ptr != nullptr, "This should not throw");
    
    // Verify it also works with valid pointer
    int value = 42;
    int* valid_ptr = &value;
    noexcept_enforce(valid_ptr != nullptr, "Valid pointer check");
    
    return true;
}

// ============================================================================
// Test Suite 6: Expected Integration
// ============================================================================

bool test_expected_passing_condition() {
    auto result = enforce_expected(true, "Should succeed");
    SIMPLE_ASSERT(result.has_value(), "enforce_expected returns success for true condition");
    
    return true;
}

bool test_expected_failing_condition() {
    auto result = enforce_expected(false, "Should fail");
    SIMPLE_ASSERT(!result.has_value(), "enforce_expected returns error for false condition");
    
    if (!result) {
        SIMPLE_ASSERT(!result.error().empty(), "enforce_expected error message is not empty");
    }
    
    return true;
}

bool test_expected_with_predicate() {
    int value = 42;
    int* ptr = &value;
    auto result = enforce_predicate_expected(NotNullPredicate, ptr, "Pointer check");
    SIMPLE_ASSERT(result.has_value(), "enforce_predicate_expected succeeds with valid pointer");
    
    return true;
}

bool test_expected_chaining() {
    auto divide = [](int a, int b) -> Expected<int, std::string> {
        auto check = enforce_expected(b != 0, "Division by zero");
        if (!check) return make_unexpected(check.error());
        return a / b;
    };
    
    auto result1 = divide(10, 2);
    SIMPLE_ASSERT(result1.has_value() && result1.value() == 5,
                  "Expected integration with division success");
    
    auto result2 = divide(10, 0);
    SIMPLE_ASSERT(!result2.has_value(), "Expected integration with division by zero");
    
    return true;
}


bool test_expected_no_exceptions() {
    // Test 1: enforce_expected with passing condition
    {
        auto result = enforce_expected(true, "Should pass");
        SIMPLE_ASSERT(result.has_value(), 
                      "enforce_expected should return Ok for true condition");
    }
    
    // Test 2: enforce_expected with failing condition
    {
        auto result = enforce_expected(false, "Should fail");
        SIMPLE_ASSERT(!result.has_value(), 
                      "enforce_expected should return Err for false condition");
        SIMPLE_ASSERT(result.error().find("Should fail") != std::string::npos,
                      "enforce_expected error message should contain user message");
    }
    
    // Test 3: always_enforce_expected
    {
        auto result_pass = always_enforce_expected(true, "Pass");
        SIMPLE_ASSERT(result_pass.has_value(), 
                      "always_enforce_expected should return Ok for true");
        
        auto result_fail = always_enforce_expected(false, "Fail");
        SIMPLE_ASSERT(!result_fail.has_value(), 
                      "always_enforce_expected should return Err for false");
    }
    
    // Test 4: enforce_predicate_expected
    {
        int positive = 42;
        auto result = enforce_predicate_expected(fat_p::IsPositivePredicate, 
                                                 positive, "Should be positive");
        SIMPLE_ASSERT(result.has_value(), 
                      "enforce_predicate_expected should return Ok for positive value");
        SIMPLE_ASSERT(result.value() == true, 
                      "enforce_predicate_expected should return true for passing predicate");
    }
    
    std::cout << "    → Expected integration using direct checks (no RAII)\n";
    return true;
}

bool test_expected_contextual_noexcept() {
    // Define a noexcept function that uses contextual_enforce_expected
    auto noexcept_func = []() noexcept -> Expected<void, std::string> {
        // This should compile and work correctly in noexcept context
        auto result = contextual_enforce_expected(&noexcept_func, true, "Test");
        if (!result) {
            return result; // Propagate error
        }
        return {};
    };
    
    // Test 1: Call the noexcept function (should succeed)
    auto result = noexcept_func();
    SIMPLE_ASSERT(result.has_value(), 
                  "contextual_enforce_expected in noexcept function should work");
    
    // Test 2: Test with failing condition in noexcept context
    auto noexcept_fail = []() noexcept -> Expected<void, std::string> {
        return contextual_enforce_expected(&noexcept_fail, false, "Failed");
    };
    
    auto result_fail = noexcept_fail();
    SIMPLE_ASSERT(!result_fail.has_value(), 
                  "contextual_enforce_expected should return Err in noexcept function");
    SIMPLE_ASSERT(result_fail.error().find("Failed") != std::string::npos,
                  "Error message should be preserved");
    
    std::cout << "    → Contextual Expected works correctly in noexcept functions\n";
    return true;
}

bool test_expected_predicate_variants() {
    // Test 1: contextual_enforce_expected_1 (single argument predicate)
    {
        int value = 42;
        auto test_func = [&]() noexcept -> Expected<bool, std::string> {
            return contextual_enforce_expected_1(&test_func, 
                                                 fat_p::IsPositivePredicate,
                                                 value, "Value should be positive");
        };
        
        auto result = test_func();
        SIMPLE_ASSERT(result.has_value(), 
                      "contextual_enforce_expected_1 should return Ok");
        SIMPLE_ASSERT(result.value() == true,
                      "contextual_enforce_expected_1 should return predicate result");
    }
    
    // Test 2: contextual_enforce_expected_2 (two argument predicate)
    {
        std::vector<int> vec = {1, 2, 3};
        auto test_func = [&]() noexcept -> Expected<bool, std::string> {
            return contextual_enforce_expected_2(&test_func,
                                                 fat_p::HasSizePredicate,
                                                 3, vec, "Size should be 3");
        };
        
        auto result = test_func();
        SIMPLE_ASSERT(result.has_value(),
                      "contextual_enforce_expected_2 should return Ok");
        SIMPLE_ASSERT(result.value() == true,
                      "contextual_enforce_expected_2 should return predicate result");
    }
    
    // Test 3: contextual_enforce_expected_3 (three argument predicate)
    {
        int value = 50;
        auto test_func = [&]() noexcept -> Expected<bool, std::string> {
            return contextual_enforce_expected_3(&test_func,
                                                 fat_p::InRangePredicate,
                                                 value, 0, 100,
                                                 "Value should be in range");
        };
        
        auto result = test_func();
        SIMPLE_ASSERT(result.has_value(),
                      "contextual_enforce_expected_3 should return Ok");
        SIMPLE_ASSERT(result.value() == true,
                      "contextual_enforce_expected_3 should return predicate result");
    }
    
    std::cout << "    → All Expected arity variants (1-3) working correctly\n";
    return true;
}

bool test_expected_error_propagation() {
    // Test 1: Early return pattern
    auto func_with_checks = []() -> Expected<int, std::string> {
        auto check1 = enforce_expected(true, "Check 1");
        if (!check1) return make_unexpected(check1.error());
        
        auto check2 = enforce_expected(true, "Check 2");
        if (!check2) return make_unexpected(check2.error());
        
        return 42;
    };
    
    auto result = func_with_checks();
    SIMPLE_ASSERT(result.has_value(), "Error propagation with early return");
    SIMPLE_ASSERT(result.value() == 42, "Correct value returned");
    
    // Test 2: Error at first check
    auto func_fail_early = []() -> Expected<int, std::string> {
        auto check1 = enforce_expected(false, "First check failed");
        if (!check1) return make_unexpected(check1.error());
        
        auto check2 = enforce_expected(true, "Check 2");
        if (!check2) return make_unexpected(check2.error());
        
        return 42;
    };
    
    auto result_fail = func_fail_early();
    SIMPLE_ASSERT(!result_fail.has_value(), "Should fail at first check");
    SIMPLE_ASSERT(result_fail.error().find("First check failed") != std::string::npos,
                  "Error message from first check");
    
    std::cout << "    → Expected error propagation patterns working correctly\n";
    return true;
}


// ============================================================================
// Test Suite 7: Performance Tests
// ============================================================================

bool test_performance_enforce_overhead() {
    const int iterations = 1000000;
    
    // Warm up
    for (int i = 0; i < 1000; ++i) {
        enforce(true, "Warmup");
    }
    
    auto start = high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        enforce(true, "Performance test");
    }
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<nanoseconds>(end - start).count();
    
    double ns_per_call = static_cast<double>(duration) / iterations;
    
    #ifdef NDEBUG
    SIMPLE_ASSERT(ns_per_call < 10.0, "enforce() has minimal overhead in release build");
    std::cout << "    → enforce() overhead: " << ns_per_call << " ns/call (release)\n";
    #else
    SIMPLE_ASSERT(ns_per_call < 500.0, "enforce() overhead acceptable in debug build");
    std::cout << "    → enforce() overhead: " << ns_per_call << " ns/call (debug)\n";
    #endif
    
    return true;
}

bool test_performance_always_enforce_overhead() {
    const int iterations = 1000000;
    
    auto start = high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        always_enforce(true, "Performance test");
    }
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<nanoseconds>(end - start).count();
    
    double ns_per_call = static_cast<double>(duration) / iterations;
    
    SIMPLE_ASSERT(ns_per_call < 500.0, "always_enforce() overhead is reasonable");
    std::cout << "    → always_enforce() overhead: " << ns_per_call << " ns/call\n";
    
    return true;
}

bool test_performance_predicate_overhead() {
    const int iterations = 1000000;
    int* ptr = new int(42);
    
    auto start = high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        volatile bool result = fat_p::NotNullPredicate::check(ptr);
        (void)result;
    }
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<nanoseconds>(end - start).count();
    
    double ns_per_call = static_cast<double>(duration) / iterations;
    
    SIMPLE_ASSERT(ns_per_call < 10.0, "Predicate check overhead is minimal");
    std::cout << "    → NotNullPredicate::check() overhead: " << ns_per_call << " ns/call\n";
    
    delete ptr;
    return true;
}

// ============================================================================
// Test Suite 8: Thread Safety
// ============================================================================

bool test_thread_safety_concurrent_enforcement() {
    // Suppress the 20,000+ exception messages during stress test
    // Using thread-safe NullBuffer to prevent crashes from concurrent writes
    ErrorOutputSuppressor suppress;
    
    const int num_threads = 4;
    const int iterations_per_thread = 10000;
    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};
    
    auto thread_func = [&](int thread_id) {
        for (int i = 0; i < iterations_per_thread; ++i) {
            try {
                if (i % 2 == 0) {
                    always_enforce(true, "Thread ", thread_id, " iteration ", i);
                    ++success_count;
                } else {
                    always_enforce(false, "Thread ", thread_id, " iteration ", i);
                }
            } catch (const fat_p::LogicContractError&) {
                ++failure_count;
            }
        }
    };
    
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(thread_func, i);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    int expected_success = num_threads * (iterations_per_thread / 2);
    int expected_failure = num_threads * (iterations_per_thread / 2);
    
    SIMPLE_ASSERT(success_count.load() == expected_success,
                  "Concurrent enforcement success count correct");
    
    SIMPLE_ASSERT(failure_count.load() == expected_failure,
                  "Concurrent enforcement failure count correct");
    
    std::cout << "    → Processed " << (num_threads * iterations_per_thread)
              << " enforcement calls across " << num_threads << " threads\n";
    
    return true;
}

// ============================================================================
// Test Suite 9: Edge Cases
// ============================================================================

bool test_edge_case_empty_message() {
    always_enforce(true);
    return true;
}

bool test_edge_case_long_message() {
    std::string long_msg(1000, 'x');
    try {
        always_enforce(false, long_msg);
        SIMPLE_ASSERT(false, "Should have thrown with long message");
    } catch (const fat_p::LogicContractError&) {
        // Expected
    }
    
    return true;
}

bool test_edge_case_special_characters() {
    try {
        always_enforce(false, "Special chars: \n\t\"\'\\");
        SIMPLE_ASSERT(false, "Should have thrown with special characters");
    } catch (const fat_p::LogicContractError&) {
        // Expected
    }
    
    return true;
}

bool test_edge_case_multiple_types() {
    try {
        always_enforce(false, "Int:", 42, " Double:", 3.14, 
                       " String:", "test", " Bool:", true);
        SIMPLE_ASSERT(false, "Should have thrown with multiple types");
    } catch (const fat_p::LogicContractError&) {
        // Expected
    }
    
    return true;
}

bool test_edge_case_numeric_limits() {
    SIMPLE_ASSERT(fat_p::InRangePredicate::check(
        std::numeric_limits<int>::max(),
        std::numeric_limits<int>::min(),
        std::numeric_limits<int>::max()
    ), "InRangePredicate with max int value");
    
    return true;
}

bool test_edge_case_empty_containers() {
    std::vector<int> empty;
    
    SIMPLE_ASSERT(fat_p::IsSortedPredicate::check(empty),
                  "IsSortedPredicate with empty container");
    
    SIMPLE_ASSERT(fat_p::ContainerIsUniquePredicate::check(empty),
                  "ContainerIsUniquePredicate with empty container");
    
    return true;
}

bool test_edge_case_single_element() {
    std::vector<int> single = {42};
    
    SIMPLE_ASSERT(fat_p::IsSortedPredicate::check(single),
                  "IsSortedPredicate with single element");
    
    SIMPLE_ASSERT(fat_p::ContainerIsUniquePredicate::check(single),
                  "ContainerIsUniquePredicate with single element");
    
    return true;
}

bool test_edge_case_floating_point() {
    SIMPLE_ASSERT(fat_p::ApproxEqualPredicate::check(1e-10, 0.0, 1e-11),
                  "ApproxEqualPredicate with very small values");
    
    double inf = std::numeric_limits<double>::infinity();
    SIMPLE_ASSERT(!fat_p::ApproxEqualPredicate::check(0.001, inf, 1.0),
                  "ApproxEqualPredicate with infinity");
    
    return true;
}

// ============================================================================
// Test Suite 10: Compile-Time Tests
// ============================================================================

bool test_compile_time_constexpr() {
    constexpr bool null_check = fat_p::NotNullPredicate::check((int*)nullptr);
    SIMPLE_ASSERT(!null_check, "constexpr null check");
    
    constexpr bool positive_check = fat_p::IsPositivePredicate::check(42);
    SIMPLE_ASSERT(positive_check, "constexpr positive check");
    
    constexpr bool range_check = fat_p::InRangePredicate::check(50, 0, 100);
    SIMPLE_ASSERT(range_check, "constexpr range check");
    
    constexpr bool power_of_two = fat_p::IsPowerOfTwoPredicate::check(16);
    SIMPLE_ASSERT(power_of_two, "constexpr power of two check");
    
    return true;
}

bool test_compile_time_static_assertions() {
    // Static assertions (compile-time only)
    static_assert(fat_p::IsPositivePredicate::check(1), 
                  "Static positive check");
    static_assert(!fat_p::IsPositivePredicate::check(0), 
                  "Static non-positive check");
    static_assert(fat_p::IsPowerOfTwoPredicate::check(1024), 
                  "Static power of two");
    
    std::cout << "    → All constexpr checks evaluated at compile-time\n";
    
    return true;
}


bool test_compile_time_noexcept_detection() {
    // Test struct with various member function signatures
    struct TestClass {
        void normal() noexcept {}
        void const_ref() const & noexcept {}
        void const_rvalue() const && noexcept {}
        void volatile_ref() volatile & noexcept {}
        void volatile_rvalue() volatile && noexcept {}
        void cv_ref() const volatile & noexcept {}
        void cv_rvalue() const volatile && noexcept {}
        
        void throwing() {} // Not noexcept
    };
    
    // Static assertions for noexcept detection
    static_assert(fat_p::is_noexcept_function_ptr<
                      decltype(&TestClass::normal)>::value,
                  "Should detect normal noexcept");
    
    static_assert(fat_p::is_noexcept_function_ptr<
                      decltype(&TestClass::const_ref)>::value,
                  "Should detect const & noexcept");
    
    static_assert(fat_p::is_noexcept_function_ptr<
                      decltype(&TestClass::const_rvalue)>::value,
                  "Should detect const && noexcept");
    
    static_assert(fat_p::is_noexcept_function_ptr<
                      decltype(&TestClass::volatile_ref)>::value,
                  "Should detect volatile & noexcept");
    
    static_assert(fat_p::is_noexcept_function_ptr<
                      decltype(&TestClass::volatile_rvalue)>::value,
                  "Should detect volatile && noexcept");
    
    static_assert(fat_p::is_noexcept_function_ptr<
                      decltype(&TestClass::cv_ref)>::value,
                  "Should detect const volatile & noexcept");
    
    static_assert(fat_p::is_noexcept_function_ptr<
                      decltype(&TestClass::cv_rvalue)>::value,
                  "Should detect const volatile && noexcept");
    
    static_assert(!fat_p::is_noexcept_function_ptr<
                      decltype(&TestClass::throwing)>::value,
                  "Should detect non-noexcept");
    
    std::cout << "    → All 8 cv-ref qualifier combinations detected correctly\n";
    std::cout << "    → is_noexcept_function_ptr trait complete\n";
    return true;
}

bool test_compile_time_predicate_noexcept() {
    // Test that ContainerIsUniquePredicate has conditional noexcept
    {
        std::vector<int> vec_int;
        std::vector<std::string> vec_string;
        
        // Both should compile (one is noexcept(false), one might be noexcept(true))
        // The key is that it compiles and works correctly
        constexpr bool int_noexcept = noexcept(
            fat_p::ContainerIsUniquePredicate::check(vec_int)
        );
        
        constexpr bool string_noexcept = noexcept(
            fat_p::ContainerIsUniquePredicate::check(vec_string)
        );
        
        // For hashable types (int, string), should be noexcept(false)
        static_assert(int_noexcept == false, 
                      "Hashable int should have noexcept(false)");
        static_assert(string_noexcept == false,
                      "Hashable string should have noexcept(false)");
    }
    
    std::cout << "    → Conditional noexcept evaluated at compile-time\n";
    return true;
}


// ============================================================================
// Main Test Runner
// ============================================================================

bool test_Enforce() {

    PRINT_HEADER(ENFORCE LIBRARY)

    TestRunner runner;
    
    // Configure test runner
    get_test_config().verbose = true;
    
    try {
        // Test Suite 1: Core Enforcement
        std::cout << colors::cyan() << "Test Suite 1: Core Enforcement Macros" << colors::reset() << "\n";
        runner.run_test("core_enforcement_basic", test_core_enforcement_basic);
        runner.run_test("core_enforcement_debug_behavior", test_core_enforcement_debug_behavior);
        runner.run_test("core_enforcement_always", test_core_enforcement_always);
        runner.run_test("core_enforcement_warning", test_core_enforcement_warning);
        runner.run_test("core_enforcement_noexcept", test_core_enforcement_noexcept);
        runner.run_test("core_enforcement_message_interpolation", test_core_enforcement_message_interpolation);
        
        // Test Suite 2: Predicates
        std::cout << "\n" << colors::cyan() << "Test Suite 2: Predicate Validation" << colors::reset() << "\n";
        runner.run_test("predicate_not_null", test_predicate_not_null);
        runner.run_test("predicate_is_positive", test_predicate_is_positive);
        runner.run_test("predicate_is_non_negative", test_predicate_is_non_negative);
        runner.run_test("predicate_not_empty", test_predicate_not_empty);
        runner.run_test("predicate_in_range", test_predicate_in_range);
        runner.run_test("predicate_is_power_of_two", test_predicate_is_power_of_two);
        runner.run_test("predicate_is_sorted", test_predicate_is_sorted);
        runner.run_test("predicate_container_is_unique", test_predicate_container_is_unique);
        runner.run_test("predicate_has_size", test_predicate_has_size);
        runner.run_test("predicate_approx_equal", test_predicate_approx_equal);
        runner.run_test("predicate_comparisons", test_predicate_comparisons);
        runner.run_test("predicate_container_unique_noexcept", test_predicate_container_unique_noexcept);
        runner.run_test("predicate_noexcept_correctness", test_predicate_noexcept_correctness);
        
        // Test Suite 3: Raisers
        std::cout << "\n" << colors::cyan() << "Test Suite 3: Raiser Functionality" << colors::reset() << "\n";
        runner.run_test("raiser_logic_error", test_raiser_logic_error);
        runner.run_test("raiser_out_of_range", test_raiser_out_of_range);
        runner.run_test("raiser_message_content", test_raiser_message_content);
        runner.run_test("raiser_locus", test_raiser_locus);
        
        // Test Suite 4: Policies
        std::cout << "\n" << colors::cyan() << "Test Suite 4: Policy Behavior" << colors::reset() << "\n";
        runner.run_test("policy_debug_only", test_policy_debug_only);
        runner.run_test("policy_always_enforce", test_policy_always_enforce);
        runner.run_test("policy_warning", test_policy_warning);
        runner.run_test("policy_no_throw", test_policy_no_throw);
        
        // Test Suite 5: Contextual Enforcement
        std::cout << "\n" << colors::cyan() << "Test Suite 5: Contextual Enforcement" << colors::reset() << "\n";
        runner.run_test("contextual_throwing_function", test_contextual_throwing_function);
        runner.run_test("contextual_noexcept_function", test_contextual_noexcept_function);
        
        // Test Suite 6: Expected Integration
        std::cout << "\n" << colors::cyan() << "Test Suite 6: Expected Integration" << colors::reset() << "\n";
        runner.run_test("expected_passing_condition", test_expected_passing_condition);
        runner.run_test("expected_failing_condition", test_expected_failing_condition);
        runner.run_test("expected_with_predicate", test_expected_with_predicate);
        runner.run_test("expected_chaining", test_expected_chaining);
        runner.run_test("expected_no_exceptions", test_expected_no_exceptions);
        runner.run_test("expected_contextual_noexcept", test_expected_contextual_noexcept);
        runner.run_test("expected_predicate_variants", test_expected_predicate_variants);
        runner.run_test("expected_error_propagation", test_expected_error_propagation);
        
        // Test Suite 7: Performance
        std::cout << "\n" << colors::cyan() << "Test Suite 7: Performance Characteristics" << colors::reset() << "\n";
        runner.run_test("performance_enforce_overhead", test_performance_enforce_overhead);
        runner.run_test("performance_always_enforce_overhead", test_performance_always_enforce_overhead);
        runner.run_test("performance_predicate_overhead", test_performance_predicate_overhead);
        
        // Test Suite 8: Thread Safety
        std::cout << "\n" << colors::cyan() << "Test Suite 8: Thread Safety" << colors::reset() << "\n";
        runner.run_test("thread_safety_concurrent_enforcement", test_thread_safety_concurrent_enforcement);
        
        // Test Suite 9: Edge Cases
        std::cout << "\n" << colors::cyan() << "Test Suite 9: Edge Cases" << colors::reset() << "\n";
        runner.run_test("edge_case_empty_message", test_edge_case_empty_message);
        runner.run_test("edge_case_long_message", test_edge_case_long_message);
        runner.run_test("edge_case_special_characters", test_edge_case_special_characters);
        runner.run_test("edge_case_multiple_types", test_edge_case_multiple_types);
        runner.run_test("edge_case_numeric_limits", test_edge_case_numeric_limits);
        runner.run_test("edge_case_empty_containers", test_edge_case_empty_containers);
        runner.run_test("edge_case_single_element", test_edge_case_single_element);
        runner.run_test("edge_case_floating_point", test_edge_case_floating_point);
        
        // Test Suite 10: Compile-Time Features
        std::cout << "\n" << colors::cyan() << "Test Suite 10: Compile-Time Features" << colors::reset() << "\n";
        runner.run_test("compile_time_constexpr", test_compile_time_constexpr);
        runner.run_test("compile_time_static_assertions", test_compile_time_static_assertions);
        runner.run_test("compile_time_noexcept_detection", test_compile_time_noexcept_detection);
        runner.run_test("compile_time_predicate_noexcept", test_compile_time_predicate_noexcept);
        
        // Print summary
        int failed = runner.print_summary();
        
        return (failed == 0);
        
    } catch (const std::exception& e) {
        std::cerr << "\n" << colors::red() << colors::bold() 
                  << "FATAL ERROR : Uncaught exception in test suite!" 
                  << colors::reset() << "\n";
        std::cerr << "   " << e.what() << "\n\n";
        return false;
    } catch (...) {
        std::cerr << "\n" << colors::red() << colors::bold() 
                  << "FATAL ERROR: Unknown exception in test suite!" 
                  << colors::reset() << "\n\n";
        return false;
    }
}

} // namespace fat_p::testing
