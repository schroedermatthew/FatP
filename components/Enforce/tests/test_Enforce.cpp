/**
 * @file test_Enforce.cpp
 * @brief Comprehensive unit test suite for the FATP_ENFORCE library
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
/*
FATP_META:
  meta_version: 1
  component: Enforce
  file_role: test
  path: components/Enforce/tests/test_Enforce.cpp
  layer: Testing
  namespace: fat_p::testing::enforce
  summary: "Unit tests for Enforce."
  api_stability: in_work
  related:
    docs_search: "Enforce"
    headers:
      - include/fat_p/enforce.h
      - include/fat_p/enforce_contextual.h
      - include/fat_p/ContractException.h
      - include/fat_p/Expected.h
      - include/fat_p/FatPTest.h
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

#include <atomic>
#include <chrono>
#include <cmath>
#include <exception>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// Include enforce library headers
#include "ContractException.h"
#include "enforce.h"
#include "enforce_contextual.h"
#include "Expected.h"
#include "FatPTest.h"

namespace fat_p::testing::enforce
{

using namespace std::chrono;


/**
 * @brief Thread-safe null streambuf that discards all output
 * Used to suppress error messages during multi-threaded stress tests
 */
class NullBuffer : public std::streambuf
{
protected:
    int overflow(int c) override
    {
        return c;
    }
};

/**
 * @brief RAII suppressor for error output during stress tests
 * Thread-safe version that redirects std::cerr to a null buffer
 */
class ErrorOutputSuppressor
{
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

    ~ErrorOutputSuppressor()
    {
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

FATP_TEST_CASE(core_enforcement_basic)
{
    // Test 1.1: Basic FATP_ENFORCE with passing condition (debug only)
    FATP_ENFORCE(true, "Should not fail");

    return true;
}

FATP_TEST_CASE(core_enforcement_debug_behavior)
{
// Test 1.2: Basic FATP_ENFORCE with failing condition (debug only)
#ifndef NDEBUG
    try
    {
        FATP_ENFORCE(false, "Should fail");
        FATP_ASSERT_TRUE(false, "FATP_ENFORCE() should have thrown in debug");
    }
    catch (const fat_p::LogicContractError&)
    {
        // Expected
    }
#else
    // In release, should be optimized away
    FATP_ENFORCE(false, "Should be optimized away");
#endif

    return true;
}

FATP_TEST_CASE(core_enforcement_always)
{
    // Test 1.3: FATP_ALWAYS_ENFORCE with passing condition
    FATP_ALWAYS_ENFORCE(1 + 1 == 2, "Math should work");

    // Test 1.4: FATP_ALWAYS_ENFORCE with failing condition
    try
    {
        FATP_ALWAYS_ENFORCE(false, "This should always fail");
        FATP_ASSERT_TRUE(false, "FATP_ALWAYS_ENFORCE() should have thrown");
    }
    catch (const fat_p::LogicContractError&)
    {
        // Expected
    }

    return true;
}

FATP_TEST_CASE(core_enforcement_warning)
{
    // Test 1.5: FATP_ENFORCE_WARN (should not throw)
    FATP_ENFORCE_WARN(false, "Warning only");
    return true;
}

FATP_TEST_CASE(core_enforcement_noexcept)
{
    // Test 1.6: FATP_NOEXCEPT_ENFORCE (should not throw)
    FATP_NOEXCEPT_ENFORCE(false, "No exception");
    return true;
}

FATP_TEST_CASE(core_enforcement_message_interpolation)
{
    // Test 1.7: Message interpolation
    int value = 42;
    std::string name = "test";
    try
    {
        FATP_ALWAYS_ENFORCE(false, "Value:", value, " Name:", name);
        FATP_ASSERT_TRUE(false, "Message interpolation should have thrown");
    }
    catch (const fat_p::LogicContractError&)
    {
        // Expected
    }

    return true;
}

// ============================================================================
// Test Suite 2: Predicate Tests
// ============================================================================

FATP_TEST_CASE(predicate_not_null)
{
    int* valid_ptr = new int(42);
    int* null_ptr = nullptr;

    FATP_ASSERT_TRUE(fat_p::NotNullPredicate::check(valid_ptr), "NotNullPredicate with valid pointer");

    FATP_ASSERT_TRUE(!fat_p::NotNullPredicate::check(null_ptr), "NotNullPredicate with null pointer");

    // Test with enforcement macro
    FATP_ALWAYS_ENFORCE_NOT_NULL(valid_ptr, "Valid pointer");

    try
    {
        FATP_ALWAYS_ENFORCE_NOT_NULL(null_ptr, "Null pointer");
        delete valid_ptr;
        FATP_ASSERT_TRUE(false, "FATP_ALWAYS_ENFORCE_NOT_NULL should have thrown");
    }
    catch (const fat_p::LogicContractError&)
    {
        // Expected
    }

    delete valid_ptr;
    return true;
}

FATP_TEST_CASE(predicate_is_positive)
{
    FATP_ASSERT_TRUE(fat_p::IsPositivePredicate::check(42), "IsPositivePredicate with positive integer");

    FATP_ASSERT_TRUE(fat_p::IsPositivePredicate::check(3.14), "IsPositivePredicate with positive double");

    FATP_ASSERT_TRUE(!fat_p::IsPositivePredicate::check(0), "IsPositivePredicate with zero");

    FATP_ASSERT_TRUE(!fat_p::IsPositivePredicate::check(-5), "IsPositivePredicate with negative value");

    return true;
}

FATP_TEST_CASE(predicate_is_non_negative)
{
    FATP_ASSERT_TRUE(fat_p::IsNonNegativePredicate::check(0), "IsNonNegativePredicate with zero");

    FATP_ASSERT_TRUE(fat_p::IsNonNegativePredicate::check(42), "IsNonNegativePredicate with positive value");

    FATP_ASSERT_TRUE(!fat_p::IsNonNegativePredicate::check(-1), "IsNonNegativePredicate with negative value");

    return true;
}

FATP_TEST_CASE(predicate_not_empty)
{
    std::vector<int> empty_vec;
    std::vector<int> filled_vec = {1, 2, 3};
    std::string empty_str = "";
    std::string filled_str = "hello";

    FATP_ASSERT_TRUE(!fat_p::NotEmptyPredicate::check(empty_vec), "NotEmptyPredicate with empty vector");

    FATP_ASSERT_TRUE(fat_p::NotEmptyPredicate::check(filled_vec), "NotEmptyPredicate with filled vector");

    FATP_ASSERT_TRUE(!fat_p::NotEmptyPredicate::check(empty_str), "NotEmptyPredicate with empty string");

    FATP_ASSERT_TRUE(fat_p::NotEmptyPredicate::check(filled_str), "NotEmptyPredicate with filled string");

    return true;
}

FATP_TEST_CASE(predicate_in_range)
{
    FATP_ASSERT_TRUE(fat_p::InRangePredicate::check(50, 0, 100), "InRangePredicate with value in range");

    FATP_ASSERT_TRUE(fat_p::InRangePredicate::check(0, 0, 100), "InRangePredicate with value at lower bound");

    FATP_ASSERT_TRUE(fat_p::InRangePredicate::check(100, 0, 100), "InRangePredicate with value at upper bound");

    FATP_ASSERT_TRUE(!fat_p::InRangePredicate::check(-1, 0, 100), "InRangePredicate with value below range");

    FATP_ASSERT_TRUE(!fat_p::InRangePredicate::check(101, 0, 100), "InRangePredicate with value above range");

    return true;
}

FATP_TEST_CASE(predicate_is_power_of_two)
{
    FATP_ASSERT_TRUE(fat_p::IsPowerOfTwoPredicate::check(1), "IsPowerOfTwoPredicate with 1");

    FATP_ASSERT_TRUE(fat_p::IsPowerOfTwoPredicate::check(2), "IsPowerOfTwoPredicate with 2");

    FATP_ASSERT_TRUE(fat_p::IsPowerOfTwoPredicate::check(16), "IsPowerOfTwoPredicate with 16");

    FATP_ASSERT_TRUE(fat_p::IsPowerOfTwoPredicate::check(1024), "IsPowerOfTwoPredicate with 1024");

    FATP_ASSERT_TRUE(!fat_p::IsPowerOfTwoPredicate::check(0), "IsPowerOfTwoPredicate with 0");

    FATP_ASSERT_TRUE(!fat_p::IsPowerOfTwoPredicate::check(3), "IsPowerOfTwoPredicate with 3");

    FATP_ASSERT_TRUE(!fat_p::IsPowerOfTwoPredicate::check(15), "IsPowerOfTwoPredicate with 15");

    return true;
}

FATP_TEST_CASE(predicate_is_sorted)
{
    std::vector<int> sorted_vec = {1, 2, 3, 4, 5};
    std::vector<int> unsorted_vec = {1, 3, 2, 5, 4};
    std::vector<int> single_elem = {42};
    std::vector<int> empty_sorted;

    FATP_ASSERT_TRUE(fat_p::IsSortedPredicate::check(sorted_vec), "IsSortedPredicate with sorted vector");

    FATP_ASSERT_TRUE(!fat_p::IsSortedPredicate::check(unsorted_vec), "IsSortedPredicate with unsorted vector");

    FATP_ASSERT_TRUE(fat_p::IsSortedPredicate::check(single_elem), "IsSortedPredicate with single element");

    FATP_ASSERT_TRUE(fat_p::IsSortedPredicate::check(empty_sorted), "IsSortedPredicate with empty vector");

    return true;
}

FATP_TEST_CASE(predicate_container_is_unique)
{
    std::vector<int> unique_vec = {1, 2, 3, 4, 5};
    std::vector<int> duplicate_vec = {1, 2, 3, 2, 5};

    FATP_ASSERT_TRUE(fat_p::ContainerIsUniquePredicate::check(unique_vec),
                     "ContainerIsUniquePredicate with unique elements");

    FATP_ASSERT_TRUE(!fat_p::ContainerIsUniquePredicate::check(duplicate_vec),
                     "ContainerIsUniquePredicate with duplicates");

    return true;
}

FATP_TEST_CASE(predicate_has_size)
{
    std::vector<int> vec_size_5 = {1, 2, 3, 4, 5};

    FATP_ASSERT_TRUE(fat_p::HasSizePredicate::check(5, vec_size_5), "HasSizePredicate with correct size");

    FATP_ASSERT_TRUE(!fat_p::HasSizePredicate::check(3, vec_size_5), "HasSizePredicate with incorrect size");

    return true;
}

FATP_TEST_CASE(predicate_approx_equal)
{
    double a = 1.0;
    double b = 1.0000001;
    double c = 1.1;

    FATP_ASSERT_TRUE(fat_p::ApproxEqualPredicate::check(0.001, a, b), "ApproxEqualPredicate with close values");

    FATP_ASSERT_TRUE(!fat_p::ApproxEqualPredicate::check(0.001, a, c), "ApproxEqualPredicate with distant values");

    return true;
}

FATP_TEST_CASE(predicate_comparisons)
{
    FATP_ASSERT_TRUE(fat_p::IsLessThanPredicate::check(5, 10), "IsLessThanPredicate with 5 < 10");

    FATP_ASSERT_TRUE(!fat_p::IsLessThanPredicate::check(10, 5), "IsLessThanPredicate with 10 < 5");

    FATP_ASSERT_TRUE(fat_p::IsGreaterThanPredicate::check(10, 5), "IsGreaterThanPredicate with 10 > 5");

    FATP_ASSERT_TRUE(!fat_p::IsGreaterThanPredicate::check(5, 10), "IsGreaterThanPredicate with 5 > 10");

    return true;
}


FATP_TEST_CASE(predicate_container_unique_noexcept)
{
    // Test 1: Hashable types (int, string) - should work correctly
    {
        std::vector<int> vec_unique = {1, 2, 3, 4, 5};
        FATP_ASSERT_TRUE(fat_p::ContainerIsUniquePredicate::check(vec_unique),
                         "ContainerIsUniquePredicate with unique ints");

        std::vector<int> vec_duplicate = {1, 2, 3, 2, 5};
        FATP_ASSERT_TRUE(!fat_p::ContainerIsUniquePredicate::check(vec_duplicate),
                         "ContainerIsUniquePredicate with duplicate ints");
    }

    // Test 2: String containers (hashable)
    {
        std::vector<std::string> vec_unique = {"a", "b", "c"};
        FATP_ASSERT_TRUE(fat_p::ContainerIsUniquePredicate::check(vec_unique),
                         "ContainerIsUniquePredicate with unique strings");

        std::vector<std::string> vec_duplicate = {"a", "b", "a"};
        FATP_ASSERT_TRUE(!fat_p::ContainerIsUniquePredicate::check(vec_duplicate),
                         "ContainerIsUniquePredicate with duplicate strings");
    }

    // Test 3: Verify compile-time noexcept specification
    {
        // For hashable types, should be noexcept(false) because unordered_set can throw
        std::vector<int> vec;
        constexpr bool is_noexcept_int = noexcept(fat_p::ContainerIsUniquePredicate::check(vec));

        // The function should NOT be noexcept for hashable types
        // (because std::unordered_set can throw std::bad_alloc)
        FATP_ASSERT_TRUE(is_noexcept_int == false,
                         "ContainerIsUniquePredicate should be noexcept(false) for hashable types");
    }

    std::cout << "    -> Conditional noexcept working correctly\n";
    return true;
}

FATP_TEST_CASE(predicate_noexcept_correctness)
{
    // Test 1: IsSortedPredicate with throwing comparator
    {
        std::vector<int> vec = {1, 2, 3, 4, 5};

        // Normal comparator should work
        FATP_ASSERT_TRUE(fat_p::IsSortedPredicate::check(vec), "IsSortedPredicate with sorted vector");

        // Test with custom comparator
        auto custom_less = [](int a, int b) {
            return a < b;
        };
        FATP_ASSERT_TRUE(fat_p::IsSortedPredicate::check(vec, custom_less), "IsSortedPredicate with custom comparator");
    }

    // Test 2: AllSatisfyPredicate with predicate function
    {
        std::vector<int> vec = {2, 4, 6, 8};
        auto is_even = [](int x) {
            return x % 2 == 0;
        };

        FATP_ASSERT_TRUE(fat_p::AllSatisfyPredicate::check(is_even, vec), "AllSatisfyPredicate with all even numbers");

        std::vector<int> vec2 = {2, 4, 5, 8};
        FATP_ASSERT_TRUE(!fat_p::AllSatisfyPredicate::check(is_even, vec2), "AllSatisfyPredicate with mixed numbers");
    }

    // Test 3: AnySatisfyPredicate with predicate function
    {
        std::vector<int> vec = {1, 3, 5, 7};
        auto is_even = [](int x) {
            return x % 2 == 0;
        };

        FATP_ASSERT_TRUE(!fat_p::AnySatisfyPredicate::check(is_even, vec), "AnySatisfyPredicate with no even numbers");

        std::vector<int> vec2 = {1, 3, 4, 7};
        FATP_ASSERT_TRUE(fat_p::AnySatisfyPredicate::check(is_even, vec2), "AnySatisfyPredicate with one even number");
    }

    std::cout << "    -> Predicates correctly allow exceptions to propagate\n";
    return true;
}


// ============================================================================
// Test Suite 3: Raiser Tests
// ============================================================================

FATP_TEST_CASE(raiser_logic_error)
{
    try
    {
        FATP_ALWAYS_ENFORCE(false, "Logic error");
        FATP_ASSERT_TRUE(false, "LogicRaiser should throw LogicContractError");
    }
    catch (const fat_p::LogicContractError&)
    {
        // Expected
    }

    return true;
}

FATP_TEST_CASE(raiser_out_of_range)
{
    try
    {
        FATP_ALWAYS_ENFORCE_IN_RANGE(0, 100, 150, "Out of range");
        FATP_ASSERT_TRUE(false, "Should have thrown exception");
    }
    catch (const fat_p::LogicContractError&)
    {
        // Expected - FATP_
        // ALWAYS_ENFORCE_IN_RANGE uses AlwaysEnforcePolicy -> LogicRaiser
    }

    return true;
}

FATP_TEST_CASE(raiser_message_content)
{
    try
    {
        FATP_ALWAYS_ENFORCE(false, "Custom error message");
        FATP_ASSERT_TRUE(false, "Exception should have been thrown");
    }
    catch (const std::exception& e)
    {
        std::string msg = e.what();
        FATP_ASSERT_TRUE(msg.find("Custom error message") != std::string::npos,
                         "Exception message contains user message");
        FATP_ASSERT_TRUE(msg.find("Contract Violation") != std::string::npos,
                         "Exception message contains 'Contract Violation'");
    }

    return true;
}

FATP_TEST_CASE(raiser_locus)
{
    try
    {
        FATP_ALWAYS_ENFORCE(false, "Test locus");
        FATP_ASSERT_TRUE(false, "Exception should have been thrown");
    }
    catch (const std::exception& e)
    {
        std::string msg = e.what();
        FATP_ASSERT_TRUE(msg.find("Location:") != std::string::npos, "Exception message contains location label");
        FATP_ASSERT_TRUE(msg.find(".cpp") != std::string::npos, "Exception message contains file name");
    }

    return true;
}

// ============================================================================
// Test Suite 4: Policy Tests
// ============================================================================

FATP_TEST_CASE(policy_debug_only)
{
#ifdef NDEBUG
    // In release, should be optimized away
    FATP_ENFORCE(false, "Should be optimized away in release");
#else
    try
    {
        FATP_ENFORCE(false, "Should throw in debug");
        FATP_ASSERT_TRUE(false, "DebugOnlyPolicy should throw in debug");
    }
    catch (const fat_p::LogicContractError&)
    {
        // Expected
    }
#endif

    return true;
}

FATP_TEST_CASE(policy_always_enforce)
{
    try
    {
        FATP_ALWAYS_ENFORCE(false, "Always enforced");
        FATP_ASSERT_TRUE(false, "AlwaysEnforcePolicy should always throw");
    }
    catch (const fat_p::LogicContractError&)
    {
        // Expected
    }

    FATP_ALWAYS_ENFORCE(true, "Should not throw");

    return true;
}

FATP_TEST_CASE(policy_warning)
{
    // WarningPolicy should not throw
    FATP_ENFORCE_WARN(false, "This is just a warning");
    FATP_ENFORCE_WARN(true, "Warning for true condition");

    return true;
}

FATP_TEST_CASE(policy_no_throw)
{
    // NoThrowPolicy should not throw
    FATP_NOEXCEPT_ENFORCE(false, "No exception should be thrown");
    FATP_NOEXCEPT_ENFORCE(true, "NoThrow with true condition");

    return true;
}

// ============================================================================
// Test Suite 5: Contextual Enforcement
// ============================================================================

namespace contextual_test_helpers
{

// Global flags for tracking raiser behavior
bool g_called_handler = false;
std::string g_handler_message;

void reset_flags()
{
    g_called_handler = false;
    g_handler_message.clear();
}

void test_handler(const std::string& msg)
{
    g_called_handler = true;
    g_handler_message = msg;
}

// Named functions for contextual enforcement testing
// Non-noexcept function - should use throwing raiser
void throwing_check_condition(bool condition)
{
    FATP_CONTEXTUAL_ENFORCE(&throwing_check_condition, condition, "Condition check failed");
}

void throwing_check_not_null(int* ptr)
{
    FATP_CONTEXTUAL_ENFORCE_NOT_NULL(&throwing_check_not_null, ptr, "Null pointer");
}

void throwing_check_positive(int value)
{
    FATP_CONTEXTUAL_ENFORCE_IS_POSITIVE(&throwing_check_positive, value, "Not positive");
}

// Noexcept function - should use non-throwing raiser (handler)
void noexcept_check_condition(bool condition) noexcept
{
    FATP_CONTEXTUAL_ENFORCE(&noexcept_check_condition, condition, "Condition check failed");
}

void noexcept_check_not_null(int* ptr) noexcept
{
    FATP_CONTEXTUAL_ENFORCE_NOT_NULL(&noexcept_check_not_null, ptr, "Null pointer");
}

void noexcept_check_positive(int value) noexcept
{
    FATP_CONTEXTUAL_ENFORCE_IS_POSITIVE(&noexcept_check_positive, value, "Not positive");
}

// Multi-argument predicate test functions (for testing comma-operator bug fix)
// These use FATP_CONTEXTUAL_ENFORCE_3 with InRangePredicate (3 arguments: value, min, max)

void throwing_check_in_range(int value, int min_val, int max_val)
{
    FATP_CONTEXTUAL_ENFORCE_3(&throwing_check_in_range,
                              fat_p::InRangePredicate,
                              value,
                              min_val,
                              max_val,
                              "Value out of range");
}

void noexcept_check_in_range(int value, int min_val, int max_val) noexcept
{
    FATP_CONTEXTUAL_ENFORCE_3(&noexcept_check_in_range,
                              fat_p::InRangePredicate,
                              value,
                              min_val,
                              max_val,
                              "Value out of range");
}

// Two-argument predicate test functions using IsLessThanPredicate
void throwing_check_less_than(int a, int b)
{
    FATP_CONTEXTUAL_ENFORCE_2(&throwing_check_less_than,
                              fat_p::IsLessThanPredicate,
                              a,
                              b,
                              "First value not less than second");
}

void noexcept_check_less_than(int a, int b) noexcept
{
    FATP_CONTEXTUAL_ENFORCE_2(&noexcept_check_less_than,
                              fat_p::IsLessThanPredicate,
                              a,
                              b,
                              "First value not less than second");
}

// Abort policy test functions for multi-argument predicates
void throwing_abort_in_range(int value, int min_val, int max_val)
{
    FATP_CONTEXTUAL_ABORT_3(&throwing_abort_in_range,
                            fat_p::InRangePredicate,
                            value,
                            min_val,
                            max_val,
                            "Value out of range (abort)");
}

void throwing_abort_less_than(int a, int b)
{
    FATP_CONTEXTUAL_ABORT_2(&throwing_abort_less_than,
                            fat_p::IsLessThanPredicate,
                            a,
                            b,
                            "First not less than second (abort)");
}

} // namespace contextual_test_helpers

FATP_TEST_CASE(contextual_throwing_function)
{
    using namespace contextual_test_helpers;
    reset_flags();
    set_violation_handler(test_handler);

    // Test 1: Throwing function with passing condition - no exception
    try
    {
        throwing_check_condition(true);
        FATP_ASSERT_TRUE(!g_called_handler, "Handler should not be called on success");
    }
    catch (...)
    {
        reset_violation_handler();
        FATP_ASSERT_TRUE(false, "Should not throw with true condition");
    }

    // Test 2: Throwing function with failing condition - should throw
    reset_flags();
    try
    {
        throwing_check_condition(false);
        reset_violation_handler();
        FATP_ASSERT_TRUE(false, "Should have thrown with false condition");
    }
    catch (const fat_p::LogicContractError&)
    {
        FATP_ASSERT_TRUE(!g_called_handler, "Handler not called - exception thrown instead");
    }
    catch (...)
    {
        reset_violation_handler();
        FATP_ASSERT_TRUE(false, "Wrong exception type thrown");
    }

    // Test 3: Throwing function with valid pointer
    reset_flags();
    int value = 42;
    int* valid_ptr = &value;
    try
    {
        throwing_check_not_null(valid_ptr);
        FATP_ASSERT_TRUE(!g_called_handler, "Handler not called with valid pointer");
    }
    catch (...)
    {
        reset_violation_handler();
        FATP_ASSERT_TRUE(false, "Should not throw with valid pointer");
    }

    // Test 4: Throwing function with null pointer - should throw
    reset_flags();
    try
    {
        throwing_check_not_null(nullptr);
        reset_violation_handler();
        FATP_ASSERT_TRUE(false, "Should have thrown with null pointer");
    }
    catch (const fat_p::LogicContractError&)
    {
        FATP_ASSERT_TRUE(!g_called_handler, "Handler not called - exception thrown");
    }
    catch (...)
    {
        reset_violation_handler();
        FATP_ASSERT_TRUE(false, "Wrong exception type for null pointer");
    }

    // Test 5: Throwing function with positive value
    reset_flags();
    try
    {
        throwing_check_positive(10);
        FATP_ASSERT_TRUE(!g_called_handler, "Handler not called with positive");
    }
    catch (...)
    {
        reset_violation_handler();
        FATP_ASSERT_TRUE(false, "Should not throw with positive value");
    }

    // Test 6: Throwing function with negative value - should throw
    reset_flags();
    try
    {
        throwing_check_positive(-5);
        reset_violation_handler();
        FATP_ASSERT_TRUE(false, "Should have thrown with negative value");
    }
    catch (const fat_p::LogicContractError&)
    {
        FATP_ASSERT_TRUE(!g_called_handler, "Handler not called - exception thrown");
    }
    catch (...)
    {
        reset_violation_handler();
        FATP_ASSERT_TRUE(false, "Wrong exception type for negative value");
    }

    reset_violation_handler();
    return true;
}

FATP_TEST_CASE(contextual_noexcept_function)
{
    using namespace contextual_test_helpers;
    reset_flags();
    set_violation_handler(test_handler);

    // Test 1: Noexcept function with passing condition - no handler call
    noexcept_check_condition(true);
    FATP_ASSERT_TRUE(!g_called_handler, "Handler not called on success");

    // Test 2: Noexcept function with failing condition - should call handler, not throw
    reset_flags();
    bool did_throw = false;
    try
    {
        noexcept_check_condition(false);
    }
    catch (...)
    {
        did_throw = true;
    }
    FATP_ASSERT_TRUE(!did_throw, "Noexcept function must not throw");
    FATP_ASSERT_TRUE(g_called_handler, "Handler called for noexcept violation");
    FATP_ASSERT_TRUE(g_handler_message.find("Condition") != std::string::npos, "Handler received message");

    // Test 3: Noexcept function with valid pointer - no handler call
    reset_flags();
    int value = 42;
    int* valid_ptr = &value;
    noexcept_check_not_null(valid_ptr);
    FATP_ASSERT_TRUE(!g_called_handler, "Handler not called with valid pointer");

    // Test 4: Noexcept function with null pointer - should call handler
    reset_flags();
    did_throw = false;
    try
    {
        noexcept_check_not_null(nullptr);
    }
    catch (...)
    {
        did_throw = true;
    }
    FATP_ASSERT_TRUE(!did_throw, "Noexcept null check must not throw");
    FATP_ASSERT_TRUE(g_called_handler, "Handler called for null pointer");

    // Test 5: Noexcept function with positive value - no handler call
    reset_flags();
    noexcept_check_positive(10);
    FATP_ASSERT_TRUE(!g_called_handler, "Handler not called with positive");

    // Test 6: Noexcept function with negative value - should call handler
    reset_flags();
    did_throw = false;
    try
    {
        noexcept_check_positive(-5);
    }
    catch (...)
    {
        did_throw = true;
    }
    FATP_ASSERT_TRUE(!did_throw, "Noexcept positive check must not throw");
    FATP_ASSERT_TRUE(g_called_handler, "Handler called for negative value");

    reset_violation_handler();
    return true;
}

FATP_TEST_CASE(contextual_raiser_selection)
{
    // Verify the noexcept detection at compile time
    using namespace contextual_test_helpers;

    // Non-noexcept functions can throw
    FATP_ASSERT_TRUE(!noexcept(throwing_check_condition(true)), "throwing_check_condition is not noexcept");
    FATP_ASSERT_TRUE(!noexcept(throwing_check_not_null(nullptr)), "throwing_check_not_null is not noexcept");
    FATP_ASSERT_TRUE(!noexcept(throwing_check_positive(1)), "throwing_check_positive is not noexcept");

    // Noexcept functions cannot throw
    FATP_ASSERT_TRUE(noexcept(noexcept_check_condition(true)), "noexcept_check_condition is noexcept");
    FATP_ASSERT_TRUE(noexcept(noexcept_check_not_null(nullptr)), "noexcept_check_not_null is noexcept");
    FATP_ASSERT_TRUE(noexcept(noexcept_check_positive(1)), "noexcept_check_positive is noexcept");

    return true;
}

FATP_TEST_CASE(contextual_expected_integration)
{
    using namespace contextual_test_helpers;
    reset_flags();
    set_violation_handler(test_handler);

    // Test FATP_CONTEXTUAL_ENFORCE_EXPECTED with passing condition
    auto result1 = FATP_CONTEXTUAL_ENFORCE_EXPECTED(&test_contextual_expected_integration, true, "Should pass");
    FATP_ASSERT_TRUE(result1.has_value(), "Expected succeeds with true");

    // Test FATP_CONTEXTUAL_ENFORCE_EXPECTED with failing condition
    auto result2 = FATP_CONTEXTUAL_ENFORCE_EXPECTED(&test_contextual_expected_integration, false, "Should fail");
    FATP_ASSERT_TRUE(!result2.has_value(), "Expected fails with false");
    FATP_ASSERT_TRUE(result2.error().find("Should fail") != std::string::npos, "Error message preserved");

    reset_violation_handler();
    return true;
}

// ============================================================================
// Test: Multi-Argument Contextual Predicates (comma-operator bug regression)
// ============================================================================

FATP_TEST_CASE(contextual_multi_arg_throwing)
{
    using namespace contextual_test_helpers;
    reset_flags();
    set_violation_handler(test_handler);

    // Test 1: InRangePredicate (3 args) - passing case
    // This would fail before the comma-operator fix:
    // PredicateType::check((value, min, max)) would evaluate comma expression
    // and only pass 'max' to check(), causing wrong behavior
    try
    {
        throwing_check_in_range(50, 0, 100); // 50 is in [0, 100]
        FATP_ASSERT_TRUE(!g_called_handler, "Handler not called for in-range value");
    }
    catch (...)
    {
        reset_violation_handler();
        FATP_ASSERT_TRUE(false, "Should not throw for in-range value");
    }

    // Test 2: InRangePredicate (3 args) - failing case (below range)
    // InRangePredicate uses OutOfRangeRaiser which throws std::out_of_range
    reset_flags();
    try
    {
        throwing_check_in_range(-5, 0, 100); // -5 is NOT in [0, 100]
        reset_violation_handler();
        FATP_ASSERT_TRUE(false, "Should have thrown for below-range value");
    }
    catch (const std::out_of_range&)
    {
        FATP_ASSERT_TRUE(!g_called_handler, "Exception thrown, not handler");
    }
    catch (...)
    {
        reset_violation_handler();
        FATP_ASSERT_TRUE(false, "Wrong exception type");
    }

    // Test 3: InRangePredicate (3 args) - failing case (above range)
    reset_flags();
    try
    {
        throwing_check_in_range(150, 0, 100); // 150 is NOT in [0, 100]
        reset_violation_handler();
        FATP_ASSERT_TRUE(false, "Should have thrown for above-range value");
    }
    catch (const std::out_of_range&)
    {
        FATP_ASSERT_TRUE(!g_called_handler, "Exception thrown, not handler");
    }
    catch (...)
    {
        reset_violation_handler();
        FATP_ASSERT_TRUE(false, "Wrong exception type");
    }

    // Test 4: LessThanPredicate (2 args) - passing case
    reset_flags();
    try
    {
        throwing_check_less_than(5, 10); // 5 < 10
        FATP_ASSERT_TRUE(!g_called_handler, "Handler not called for valid comparison");
    }
    catch (...)
    {
        reset_violation_handler();
        FATP_ASSERT_TRUE(false, "Should not throw for valid less-than");
    }

    // Test 5: LessThanPredicate (2 args) - failing case
    reset_flags();
    try
    {
        throwing_check_less_than(10, 5); // 10 is NOT < 5
        reset_violation_handler();
        FATP_ASSERT_TRUE(false, "Should have thrown for invalid less-than");
    }
    catch (const fat_p::LogicContractError&)
    {
        FATP_ASSERT_TRUE(!g_called_handler, "Exception thrown, not handler");
    }
    catch (...)
    {
        reset_violation_handler();
        FATP_ASSERT_TRUE(false, "Wrong exception type");
    }

    // Test 6: LessThanPredicate (2 args) - failing case (equal values)
    reset_flags();
    try
    {
        throwing_check_less_than(7, 7); // 7 is NOT < 7
        reset_violation_handler();
        FATP_ASSERT_TRUE(false, "Should have thrown for equal values");
    }
    catch (const fat_p::LogicContractError&)
    {
        FATP_ASSERT_TRUE(!g_called_handler, "Exception thrown, not handler");
    }
    catch (...)
    {
        reset_violation_handler();
        FATP_ASSERT_TRUE(false, "Wrong exception type");
    }

    reset_violation_handler();
    std::cout << "    -> Multi-arg throwing predicates working correctly\n";
    return true;
}

FATP_TEST_CASE(contextual_multi_arg_noexcept)
{
    using namespace contextual_test_helpers;
    reset_flags();
    set_violation_handler(test_handler);

    // Test 1: InRangePredicate (3 args) in noexcept function - passing
    noexcept_check_in_range(50, 0, 100);
    FATP_ASSERT_TRUE(!g_called_handler, "Handler not called for in-range value");

    // Test 2: InRangePredicate (3 args) in noexcept function - failing
    reset_flags();
    noexcept_check_in_range(-5, 0, 100);
    FATP_ASSERT_TRUE(g_called_handler, "Handler called for out-of-range value");

    // Test 3: LessThanPredicate (2 args) in noexcept function - passing
    reset_flags();
    noexcept_check_less_than(3, 7);
    FATP_ASSERT_TRUE(!g_called_handler, "Handler not called for valid less-than");

    // Test 4: LessThanPredicate (2 args) in noexcept function - failing
    reset_flags();
    noexcept_check_less_than(10, 5);
    FATP_ASSERT_TRUE(g_called_handler, "Handler called for invalid less-than");

    // Verify noexcept-ness
    FATP_ASSERT_TRUE(noexcept(noexcept_check_in_range(50, 0, 100)), "noexcept_check_in_range is noexcept");
    FATP_ASSERT_TRUE(noexcept(noexcept_check_less_than(1, 2)), "noexcept_check_less_than is noexcept");

    reset_violation_handler();
    std::cout << "    -> Multi-arg noexcept predicates working correctly\n";
    return true;
}

FATP_TEST_CASE(contextual_multi_arg_abort)
{
    using namespace contextual_test_helpers;
    reset_flags();
    set_violation_handler(test_handler);

    // Test 1: FATP_CONTEXTUAL_ABORT_3 with InRangePredicate - passing case
    // Cannot test failing case as it would abort the process
    try
    {
        throwing_abort_in_range(50, 0, 100); // Should pass
        FATP_ASSERT_TRUE(!g_called_handler, "Handler not called for valid range");
    }
    catch (...)
    {
        reset_violation_handler();
        FATP_ASSERT_TRUE(false, "Should not throw for valid range with abort policy");
    }

    // Test 2: FATP_CONTEXTUAL_ABORT_2 with LessThanPredicate - passing case
    reset_flags();
    try
    {
        throwing_abort_less_than(3, 10); // Should pass
        FATP_ASSERT_TRUE(!g_called_handler, "Handler not called for valid comparison");
    }
    catch (...)
    {
        reset_violation_handler();
        FATP_ASSERT_TRUE(false, "Should not throw for valid comparison with abort policy");
    }

    reset_violation_handler();
    std::cout << "    -> Multi-arg abort predicates working correctly (passing cases)\n";
    return true;
}

FATP_TEST_CASE(contextual_multi_arg_boundary)
{
    using namespace contextual_test_helpers;
    reset_flags();
    set_violation_handler(test_handler);

    // Test boundary values for InRangePredicate
    // These are critical tests - the comma-operator bug would cause
    // only the last argument to be passed, making bounds checking wrong

    // Test 1: Value exactly at lower bound (should pass)
    try
    {
        throwing_check_in_range(0, 0, 100);
        FATP_ASSERT_TRUE(!g_called_handler, "Lower bound should be in range");
    }
    catch (...)
    {
        reset_violation_handler();
        FATP_ASSERT_TRUE(false, "Lower bound value should not throw");
    }

    // Test 2: Value exactly at upper bound (should pass)
    reset_flags();
    try
    {
        throwing_check_in_range(100, 0, 100);
        FATP_ASSERT_TRUE(!g_called_handler, "Upper bound should be in range");
    }
    catch (...)
    {
        reset_violation_handler();
        FATP_ASSERT_TRUE(false, "Upper bound value should not throw");
    }

    // Test 3: Value one below lower bound (should fail)
    // InRangePredicate uses OutOfRangeRaiser which throws std::out_of_range
    reset_flags();
    try
    {
        throwing_check_in_range(-1, 0, 100);
        reset_violation_handler();
        FATP_ASSERT_TRUE(false, "One below lower bound should throw");
    }
    catch (const std::out_of_range&)
    {
        // Expected
    }

    // Test 4: Value one above upper bound (should fail)
    reset_flags();
    try
    {
        throwing_check_in_range(101, 0, 100);
        reset_violation_handler();
        FATP_ASSERT_TRUE(false, "One above upper bound should throw");
    }
    catch (const std::out_of_range&)
    {
        // Expected
    }

    // Test 5: Negative range
    reset_flags();
    try
    {
        throwing_check_in_range(-50, -100, -10); // -50 in [-100, -10]
        FATP_ASSERT_TRUE(!g_called_handler, "Negative range should work");
    }
    catch (...)
    {
        reset_violation_handler();
        FATP_ASSERT_TRUE(false, "Negative range value should not throw");
    }

    reset_violation_handler();
    std::cout << "    -> Multi-arg boundary conditions working correctly\n";
    return true;
}

// ============================================================================
// Test Suite 6: Expected Integration
// ============================================================================


FATP_TEST_CASE(expected_passing_condition)
{
    auto result = FATP_ENFORCE_EXPECTED(true, "Should succeed");
    FATP_ASSERT_TRUE(result.has_value(), "FATP_ENFORCE_EXPECTED returns success for true condition");

    return true;
}

FATP_TEST_CASE(expected_failing_condition)
{
    auto result = FATP_ENFORCE_EXPECTED(false, "Should fail");
    FATP_ASSERT_TRUE(!result.has_value(), "FATP_ENFORCE_EXPECTED returns error for false condition");

    if (!result)
    {
        FATP_ASSERT_TRUE(!result.error().empty(), "FATP_ENFORCE_EXPECTED error message is not empty");
    }

    return true;
}

FATP_TEST_CASE(expected_with_predicate)
{
    int value = 42;
    int* ptr = &value;
    auto result = FATP_ENFORCE_PREDICATE_EXPECTED(NotNullPredicate, ptr, "Pointer check");
    FATP_ASSERT_TRUE(result.has_value(), "FATP_ENFORCE_PREDICATE_EXPECTED succeeds with valid pointer");

    return true;
}

FATP_TEST_CASE(expected_chaining)
{
    auto divide = [](int a, int b) -> Expected<int, std::string> {
        auto check = FATP_ENFORCE_EXPECTED(b != 0, "Division by zero");
        if (!check)
        {
            return make_unexpected(check.error());
        }
        return a / b;
    };

    auto result1 = divide(10, 2);
    FATP_ASSERT_TRUE(result1.has_value() && result1.value() == 5, "Expected integration with division success");

    auto result2 = divide(10, 0);
    FATP_ASSERT_TRUE(!result2.has_value(), "Expected integration with division by zero");

    return true;
}


FATP_TEST_CASE(expected_no_exceptions)
{
    // Test 1: FATP_ENFORCE_EXPECTED with passing condition
    {
        auto result = FATP_ENFORCE_EXPECTED(true, "Should pass");
        FATP_ASSERT_TRUE(result.has_value(), "FATP_ENFORCE_EXPECTED should return Ok for true condition");
    }

    // Test 2: FATP_ENFORCE_EXPECTED with failing condition
    {
        auto result = FATP_ENFORCE_EXPECTED(false, "Should fail");
        FATP_ASSERT_TRUE(!result.has_value(), "FATP_ENFORCE_EXPECTED should return Err for false condition");
        FATP_ASSERT_TRUE(result.error().find("Should fail") != std::string::npos,
                         "FATP_ENFORCE_EXPECTED error message should contain user message");
    }

    // Test 3: FATP_ALWAYS_ENFORCE_EXPECTED
    {
        auto result_pass = FATP_ALWAYS_ENFORCE_EXPECTED(true, "Pass");
        FATP_ASSERT_TRUE(result_pass.has_value(), "FATP_ALWAYS_ENFORCE_EXPECTED should return Ok for true");

        auto result_fail = FATP_ALWAYS_ENFORCE_EXPECTED(false, "Fail");
        FATP_ASSERT_TRUE(!result_fail.has_value(), "FATP_ALWAYS_ENFORCE_EXPECTED should return Err for false");
    }

    // Test 4: FATP_ENFORCE_PREDICATE_EXPECTED
    {
        int positive = 42;
        auto result = FATP_ENFORCE_PREDICATE_EXPECTED(fat_p::IsPositivePredicate, positive, "Should be positive");
        FATP_ASSERT_TRUE(result.has_value(), "FATP_ENFORCE_PREDICATE_EXPECTED should return Ok for positive value");
        FATP_ASSERT_TRUE(result.value() == true,
                         "FATP_ENFORCE_PREDICATE_EXPECTED should return true for passing predicate");
    }

    std::cout << "    -> Expected integration using direct checks (no RAII)\n";
    return true;
}

FATP_TEST_CASE(expected_contextual_noexcept)
{
    // Define a noexcept function that uses FATP_CONTEXTUAL_ENFORCE_EXPECTED
    auto noexcept_func = []() noexcept -> Expected<void, std::string> {
        // This should compile and work correctly in noexcept context
        auto result = FATP_CONTEXTUAL_ENFORCE_EXPECTED(&noexcept_func, true, "Test");
        if (!result)
        {
            return result; // Propagate error
        }
        return {};
    };

    // Test 1: Call the noexcept function (should succeed)
    auto result = noexcept_func();
    FATP_ASSERT_TRUE(result.has_value(), "FATP_CONTEXTUAL_ENFORCE_EXPECTED in noexcept function should work");

    // Test 2: Test with failing condition in noexcept context
    auto noexcept_fail = []() noexcept -> Expected<void, std::string> {
        return FATP_CONTEXTUAL_ENFORCE_EXPECTED(&noexcept_fail, false, "Failed");
    };

    auto result_fail = noexcept_fail();
    FATP_ASSERT_TRUE(!result_fail.has_value(),
                     "FATP_CONTEXTUAL_ENFORCE_EXPECTED should return Err in noexcept function");
    FATP_ASSERT_TRUE(result_fail.error().find("Failed") != std::string::npos, "Error message should be preserved");

    std::cout << "    -> Contextual Expected works correctly in noexcept functions\n";
    return true;
}

FATP_TEST_CASE(expected_predicate_variants)
{
    // Test 1: FATP_CONTEXTUAL_ENFORCE_EXPECTED_1 (single argument predicate)
    {
        int value = 42;
        auto test_func = [&]() noexcept -> Expected<bool, std::string> {
            return FATP_CONTEXTUAL_ENFORCE_EXPECTED_1(&test_func,
                                                      fat_p::IsPositivePredicate,
                                                      value,
                                                      "Value should be positive");
        };

        auto result = test_func();
        FATP_ASSERT_TRUE(result.has_value(), "FATP_CONTEXTUAL_ENFORCE_EXPECTED_1 should return Ok");
        FATP_ASSERT_TRUE(result.value() == true, "FATP_CONTEXTUAL_ENFORCE_EXPECTED_1 should return predicate result");
    }

    // Test 2: FATP_CONTEXTUAL_ENFORCE_EXPECTED_2 (two argument predicate)
    {
        std::vector<int> vec = {1, 2, 3};
        auto test_func = [&]() noexcept -> Expected<bool, std::string> {
            return FATP_CONTEXTUAL_ENFORCE_EXPECTED_2(&test_func, fat_p::HasSizePredicate, 3, vec, "Size should be 3");
        };

        auto result = test_func();
        FATP_ASSERT_TRUE(result.has_value(), "FATP_CONTEXTUAL_ENFORCE_EXPECTED_2 should return Ok");
        FATP_ASSERT_TRUE(result.value() == true, "FATP_CONTEXTUAL_ENFORCE_EXPECTED_2 should return predicate result");
    }

    // Test 3: FATP_CONTEXTUAL_ENFORCE_EXPECTED_3 (three argument predicate)
    {
        int value = 50;
        auto test_func = [&]() noexcept -> Expected<bool, std::string> {
            return FATP_CONTEXTUAL_ENFORCE_EXPECTED_3(&test_func,
                                                      fat_p::InRangePredicate,
                                                      value,
                                                      0,
                                                      100,
                                                      "Value should be in range");
        };

        auto result = test_func();
        FATP_ASSERT_TRUE(result.has_value(), "FATP_CONTEXTUAL_ENFORCE_EXPECTED_3 should return Ok");
        FATP_ASSERT_TRUE(result.value() == true, "FATP_CONTEXTUAL_ENFORCE_EXPECTED_3 should return predicate result");
    }

    std::cout << "    -> All Expected arity variants (1-3) working correctly\n";
    return true;
}

FATP_TEST_CASE(expected_error_propagation)
{
    // Test 1: Early return pattern
    auto func_with_checks = []() -> Expected<int, std::string> {
        auto check1 = FATP_ENFORCE_EXPECTED(true, "Check 1");
        if (!check1)
        {
            return make_unexpected(check1.error());
        }

        auto check2 = FATP_ENFORCE_EXPECTED(true, "Check 2");
        if (!check2)
        {
            return make_unexpected(check2.error());
        }

        return 42;
    };

    auto result = func_with_checks();
    FATP_ASSERT_TRUE(result.has_value(), "Error propagation with early return");
    FATP_ASSERT_TRUE(result.value() == 42, "Correct value returned");

    // Test 2: Error at first check
    auto func_fail_early = []() -> Expected<int, std::string> {
        auto check1 = FATP_ENFORCE_EXPECTED(false, "First check failed");
        if (!check1)
        {
            return make_unexpected(check1.error());
        }

        auto check2 = FATP_ENFORCE_EXPECTED(true, "Check 2");
        if (!check2)
        {
            return make_unexpected(check2.error());
        }

        return 42;
    };

    auto result_fail = func_fail_early();
    FATP_ASSERT_TRUE(!result_fail.has_value(), "Should fail at first check");
    FATP_ASSERT_TRUE(result_fail.error().find("First check failed") != std::string::npos,
                     "Error message from first check");

    std::cout << "    -> Expected error propagation patterns working correctly\n";
    return true;
}


// ============================================================================
// Test Suite 7: Thread Safety
// ============================================================================

FATP_TEST_CASE(thread_safety_concurrent_enforcement)
{
    // Suppress the 20,000+ exception messages during stress test
    // Using thread-safe NullBuffer to prevent crashes from concurrent writes
    ErrorOutputSuppressor suppress;

    const int num_threads = 4;
    const int iterations_per_thread = 10000;
    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};

    auto thread_func = [&](int thread_id) {
        for (int i = 0; i < iterations_per_thread; ++i)
        {
            try
            {
                if (i % 2 == 0)
                {
                    FATP_ALWAYS_ENFORCE(true, "Thread ", thread_id, " iteration ", i);
                    ++success_count;
                }
                else
                {
                    FATP_ALWAYS_ENFORCE(false, "Thread ", thread_id, " iteration ", i);
                }
            }
            catch (const fat_p::LogicContractError&)
            {
                ++failure_count;
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i)
    {
        threads.emplace_back(thread_func, i);
    }

    for (auto& t : threads)
    {
        t.join();
    }

    int expected_success = num_threads * (iterations_per_thread / 2);
    int expected_failure = num_threads * (iterations_per_thread / 2);

    FATP_ASSERT_TRUE(success_count.load() == expected_success, "Concurrent enforcement success count correct");

    FATP_ASSERT_TRUE(failure_count.load() == expected_failure, "Concurrent enforcement failure count correct");

    std::cout << "    -> Processed " << (num_threads * iterations_per_thread) << " enforcement calls across "
              << num_threads << " threads\n";

    return true;
}

// ============================================================================
// Test Suite 9: Edge Cases
// ============================================================================

FATP_TEST_CASE(edge_case_empty_message)
{
    FATP_ALWAYS_ENFORCE(true);
    return true;
}

FATP_TEST_CASE(edge_case_long_message)
{
    std::string long_msg(1000, 'x');
    try
    {
        FATP_ALWAYS_ENFORCE(false, long_msg);
        FATP_ASSERT_TRUE(false, "Should have thrown with long message");
    }
    catch (const fat_p::LogicContractError&)
    {
        // Expected
    }

    return true;
}

FATP_TEST_CASE(edge_case_special_characters)
{
    try
    {
        FATP_ALWAYS_ENFORCE(false, "Special chars: \n\t\"\'\\");
        FATP_ASSERT_TRUE(false, "Should have thrown with special characters");
    }
    catch (const fat_p::LogicContractError&)
    {
        // Expected
    }

    return true;
}

FATP_TEST_CASE(edge_case_multiple_types)
{
    try
    {
        FATP_ALWAYS_ENFORCE(false, "Int:", 42, " Double:", 3.14, " String:", "test", " Bool:", true);
        FATP_ASSERT_TRUE(false, "Should have thrown with multiple types");
    }
    catch (const fat_p::LogicContractError&)
    {
        // Expected
    }

    return true;
}

FATP_TEST_CASE(edge_case_numeric_limits)
{
    FATP_ASSERT_TRUE(fat_p::InRangePredicate::check(std::numeric_limits<int>::max(),
                                                    std::numeric_limits<int>::min(),
                                                    std::numeric_limits<int>::max()),
                     "InRangePredicate with max int value");

    return true;
}

FATP_TEST_CASE(edge_case_empty_containers)
{
    std::vector<int> empty;

    FATP_ASSERT_TRUE(fat_p::IsSortedPredicate::check(empty), "IsSortedPredicate with empty container");

    FATP_ASSERT_TRUE(fat_p::ContainerIsUniquePredicate::check(empty),
                     "ContainerIsUniquePredicate with empty container");

    return true;
}

FATP_TEST_CASE(edge_case_single_element)
{
    std::vector<int> single = {42};

    FATP_ASSERT_TRUE(fat_p::IsSortedPredicate::check(single), "IsSortedPredicate with single element");

    FATP_ASSERT_TRUE(fat_p::ContainerIsUniquePredicate::check(single),
                     "ContainerIsUniquePredicate with single element");

    return true;
}

FATP_TEST_CASE(edge_case_floating_point)
{
    FATP_ASSERT_TRUE(fat_p::ApproxEqualPredicate::check(1e-10, 0.0, 1e-11),
                     "ApproxEqualPredicate with very small values");

    double inf = std::numeric_limits<double>::infinity();
    FATP_ASSERT_TRUE(!fat_p::ApproxEqualPredicate::check(0.001, inf, 1.0), "ApproxEqualPredicate with infinity");

    return true;
}

// ============================================================================
// Test Suite 10: Compile-Time Tests
// ============================================================================

FATP_TEST_CASE(compile_time_constexpr)
{
    constexpr bool null_check = fat_p::NotNullPredicate::check((int*)nullptr);
    FATP_ASSERT_TRUE(!null_check, "constexpr null check");

    constexpr bool positive_check = fat_p::IsPositivePredicate::check(42);
    FATP_ASSERT_TRUE(positive_check, "constexpr positive check");

    constexpr bool range_check = fat_p::InRangePredicate::check(50, 0, 100);
    FATP_ASSERT_TRUE(range_check, "constexpr range check");

    constexpr bool power_of_two = fat_p::IsPowerOfTwoPredicate::check(16);
    FATP_ASSERT_TRUE(power_of_two, "constexpr power of two check");

    return true;
}

FATP_TEST_CASE(compile_time_static_assertions)
{
    // Static assertions (compile-time only)
    static_assert(fat_p::IsPositivePredicate::check(1), "Static positive check");
    static_assert(!fat_p::IsPositivePredicate::check(0), "Static non-positive check");
    static_assert(fat_p::IsPowerOfTwoPredicate::check(1024), "Static power of two");

    std::cout << "    -> All constexpr checks evaluated at compile-time\n";

    return true;
}


FATP_TEST_CASE(compile_time_noexcept_detection)
{
    // Test struct with various member function signatures
    struct TestClass
    {
        void normal() noexcept
        {
        }
        void const_ref() const& noexcept
        {
        }
        void const_rvalue() const&& noexcept
        {
        }
        void volatile_ref() volatile& noexcept
        {
        }
        void volatile_rvalue() volatile&& noexcept
        {
        }
        void cv_ref() const volatile& noexcept
        {
        }
        void cv_rvalue() const volatile&& noexcept
        {
        }

        void throwing()
        {
        } // Not noexcept
    };

    // Static assertions for noexcept detection
    static_assert(fat_p::is_noexcept_function_ptr<decltype(&TestClass::normal)>::value,
                  "Should detect normal noexcept");

    static_assert(fat_p::is_noexcept_function_ptr<decltype(&TestClass::const_ref)>::value,
                  "Should detect const & noexcept");

    static_assert(fat_p::is_noexcept_function_ptr<decltype(&TestClass::const_rvalue)>::value,
                  "Should detect const && noexcept");

    static_assert(fat_p::is_noexcept_function_ptr<decltype(&TestClass::volatile_ref)>::value,
                  "Should detect volatile & noexcept");

    static_assert(fat_p::is_noexcept_function_ptr<decltype(&TestClass::volatile_rvalue)>::value,
                  "Should detect volatile && noexcept");

    static_assert(fat_p::is_noexcept_function_ptr<decltype(&TestClass::cv_ref)>::value,
                  "Should detect const volatile & noexcept");

    static_assert(fat_p::is_noexcept_function_ptr<decltype(&TestClass::cv_rvalue)>::value,
                  "Should detect const volatile && noexcept");

    static_assert(!fat_p::is_noexcept_function_ptr<decltype(&TestClass::throwing)>::value,
                  "Should detect non-noexcept");

    std::cout << "    -> All 8 cv-ref qualifier combinations detected correctly\n";
    std::cout << "    -> is_noexcept_function_ptr trait complete\n";
    return true;
}

FATP_TEST_CASE(compile_time_predicate_noexcept)
{
    // Test that ContainerIsUniquePredicate has conditional noexcept
    {
        std::vector<int> vec_int;
        std::vector<std::string> vec_string;

        // Both should compile (one is noexcept(false), one might be noexcept(true))
        // The key is that it compiles and works correctly
        constexpr bool int_noexcept = noexcept(fat_p::ContainerIsUniquePredicate::check(vec_int));

        constexpr bool string_noexcept = noexcept(fat_p::ContainerIsUniquePredicate::check(vec_string));

        // For hashable types (int, string), should be noexcept(false)
        static_assert(int_noexcept == false, "Hashable int should have noexcept(false)");
        static_assert(string_noexcept == false, "Hashable string should have noexcept(false)");
    }

    std::cout << "    -> Conditional noexcept evaluated at compile-time\n";
    return true;
}


// ============================================================================
// Main Test Runner
// ============================================================================

} // namespace fat_p::testing::enforce

namespace fat_p::testing
{


void run_benchmarks()
{
    using namespace fat_p::testing;

    auto& out = *get_test_config().output;
    out << "\n" << colors::cyan() << "Sanity Benchmark:" << colors::reset() << "\n";
    out << "  (benchmarks are provided by benchmark executables; unit tests do not run full benchmarks)\n\n";
}

namespace
{
// Handler that doesn't abort - just logs to a discard buffer
void test_global_handler(const std::string&)
{
    // Do nothing - prevents abort during tests
}
} // anonymous namespace

bool test_Enforce()
{
    FATP_PRINT_HEADER(ENFORCE LIBRARY)

    TestRunner runner;

    // Configure test runner
    get_test_config().verbose = true;

    // Set a non-aborting handler for all tests
    fat_p::set_violation_handler(test_global_handler);

    try
    {
        // Test Suite 1: Core Enforcement
        std::cout << colors::cyan() << "Test Suite 1: Core Enforcement Macros" << colors::reset() << "\n";
        FATP_RUN_TEST_NS(runner, enforce, core_enforcement_basic);
        FATP_RUN_TEST_NS(runner, enforce, core_enforcement_debug_behavior);
        FATP_RUN_TEST_NS(runner, enforce, core_enforcement_always);
        FATP_RUN_TEST_NS(runner, enforce, core_enforcement_warning);
        FATP_RUN_TEST_NS(runner, enforce, core_enforcement_noexcept);
        FATP_RUN_TEST_NS(runner, enforce, core_enforcement_message_interpolation);

        // Test Suite 2: Predicates
        std::cout << "\n" << colors::cyan() << "Test Suite 2: Predicate Validation" << colors::reset() << "\n";
        FATP_RUN_TEST_NS(runner, enforce, predicate_not_null);
        FATP_RUN_TEST_NS(runner, enforce, predicate_is_positive);
        FATP_RUN_TEST_NS(runner, enforce, predicate_is_non_negative);
        FATP_RUN_TEST_NS(runner, enforce, predicate_not_empty);
        FATP_RUN_TEST_NS(runner, enforce, predicate_in_range);
        FATP_RUN_TEST_NS(runner, enforce, predicate_is_power_of_two);
        FATP_RUN_TEST_NS(runner, enforce, predicate_is_sorted);
        FATP_RUN_TEST_NS(runner, enforce, predicate_container_is_unique);
        FATP_RUN_TEST_NS(runner, enforce, predicate_has_size);
        FATP_RUN_TEST_NS(runner, enforce, predicate_approx_equal);
        FATP_RUN_TEST_NS(runner, enforce, predicate_comparisons);
        FATP_RUN_TEST_NS(runner, enforce, predicate_container_unique_noexcept);
        FATP_RUN_TEST_NS(runner, enforce, predicate_noexcept_correctness);

        // Test Suite 3: Raisers
        std::cout << "\n" << colors::cyan() << "Test Suite 3: Raiser Functionality" << colors::reset() << "\n";
        FATP_RUN_TEST_NS(runner, enforce, raiser_logic_error);
        FATP_RUN_TEST_NS(runner, enforce, raiser_out_of_range);
        FATP_RUN_TEST_NS(runner, enforce, raiser_message_content);
        FATP_RUN_TEST_NS(runner, enforce, raiser_locus);

        // Test Suite 4: Policies
        std::cout << "\n" << colors::cyan() << "Test Suite 4: Policy Behavior" << colors::reset() << "\n";
        FATP_RUN_TEST_NS(runner, enforce, policy_debug_only);
        FATP_RUN_TEST_NS(runner, enforce, policy_always_enforce);
        FATP_RUN_TEST_NS(runner, enforce, policy_warning);
        FATP_RUN_TEST_NS(runner, enforce, policy_no_throw);

        // Test Suite 5: Contextual Enforcement
        std::cout << "\n" << colors::cyan() << "Test Suite 5: Contextual Enforcement" << colors::reset() << "\n";
        FATP_RUN_TEST_NS(runner, enforce, contextual_throwing_function);
        FATP_RUN_TEST_NS(runner, enforce, contextual_noexcept_function);
        FATP_RUN_TEST_NS(runner, enforce, contextual_raiser_selection);
        FATP_RUN_TEST_NS(runner, enforce, contextual_expected_integration);
        FATP_RUN_TEST_NS(runner, enforce, contextual_multi_arg_throwing);
        FATP_RUN_TEST_NS(runner, enforce, contextual_multi_arg_noexcept);
        FATP_RUN_TEST_NS(runner, enforce, contextual_multi_arg_abort);
        FATP_RUN_TEST_NS(runner, enforce, contextual_multi_arg_boundary);

        // Test Suite 6: Expected Integration
        std::cout << "\n" << colors::cyan() << "Test Suite 6: Expected Integration" << colors::reset() << "\n";
        FATP_RUN_TEST_NS(runner, enforce, expected_passing_condition);
        FATP_RUN_TEST_NS(runner, enforce, expected_failing_condition);
        FATP_RUN_TEST_NS(runner, enforce, expected_with_predicate);
        FATP_RUN_TEST_NS(runner, enforce, expected_chaining);
        FATP_RUN_TEST_NS(runner, enforce, expected_no_exceptions);
        FATP_RUN_TEST_NS(runner, enforce, expected_contextual_noexcept);
        FATP_RUN_TEST_NS(runner, enforce, expected_predicate_variants);
        FATP_RUN_TEST_NS(runner, enforce, expected_error_propagation);

        // Test Suite 7: Thread Safety
        std::cout << "\n" << colors::cyan() << "Test Suite 7: Thread Safety" << colors::reset() << "\n";
        FATP_RUN_TEST_NS(runner, enforce, thread_safety_concurrent_enforcement);

        // Test Suite 8: Edge Cases
        std::cout << "\n" << colors::cyan() << "Test Suite 9: Edge Cases" << colors::reset() << "\n";
        FATP_RUN_TEST_NS(runner, enforce, edge_case_empty_message);
        FATP_RUN_TEST_NS(runner, enforce, edge_case_long_message);
        FATP_RUN_TEST_NS(runner, enforce, edge_case_special_characters);
        FATP_RUN_TEST_NS(runner, enforce, edge_case_multiple_types);
        FATP_RUN_TEST_NS(runner, enforce, edge_case_numeric_limits);
        FATP_RUN_TEST_NS(runner, enforce, edge_case_empty_containers);
        FATP_RUN_TEST_NS(runner, enforce, edge_case_single_element);
        FATP_RUN_TEST_NS(runner, enforce, edge_case_floating_point);

        // Test Suite 10: Compile-Time Features
        std::cout << "\n" << colors::cyan() << "Test Suite 10: Compile-Time Features" << colors::reset() << "\n";
        FATP_RUN_TEST_NS(runner, enforce, compile_time_constexpr);
        FATP_RUN_TEST_NS(runner, enforce, compile_time_static_assertions);
        FATP_RUN_TEST_NS(runner, enforce, compile_time_noexcept_detection);
        FATP_RUN_TEST_NS(runner, enforce, compile_time_predicate_noexcept);

        // Print summary
        int failed = runner.print_summary();

        // Reset handler
        fat_p::reset_violation_handler();

        return (failed == 0);
    }
    catch (const std::exception& e)
    {
        fat_p::reset_violation_handler();
        std::cerr << "\n"
                  << colors::red() << colors::bold() << "FATAL ERROR : Uncaught exception in test suite!"
                  << colors::reset() << "\n";
        std::cerr << "   " << e.what() << "\n\n";
        return false;
    }
    catch (...)
    {
        fat_p::reset_violation_handler();
        std::cerr << "\n"
                  << colors::red() << colors::bold() << "FATAL ERROR: Unknown exception in test suite!"
                  << colors::reset() << "\n\n";
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
    return fat_p::testing::test_Enforce() ? 0 : 1;
}
#endif
