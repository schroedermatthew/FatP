/**
 * @file test_ConstexprUtilities.cpp
 * @brief Comprehensive unit tests for ConstexprUtilities
 *
 * @details Complete test suite for:
 * - Hash functions (32-bit and 64-bit FNV-1a)
 * - is_power_of_two (signed and unsigned integers)
 * - to_string_view (integers and floating-point)
 * - constexpr_concat (string concatenation)
 * - ConstexprString operations
 * 
 * @version 2.0 - Enhanced with full integer support, 64-bit hash, and floating-point
 */

#include <iostream>
#include <string>
#include <limits>
#include <cstdint>
#include <cmath>

#include "ConstexprUtilities.h"
#include "test_ConstexprUtilities.h"
#include "test_Utilities.h"

using namespace cpp_utilities::testing;
using namespace cpp_utilities;

namespace cpp_utilities::testing
{

// =============================================================================
// Test Suite 1: Hash Functions (32-bit)
// =============================================================================

bool test_constexpr_hash_basic() {
    std::cout << colors::cyan() << "\nTesting: constexpr_hash (32-bit)..."
              << colors::reset() << std::endl;
    
    // Empty string - FNV offset basis
    static_assert(constexpr_hash("") == 2166136261U);
    ASSERT_EQ(constexpr_hash(""), 2166136261U, "Empty string hash");
    
    // Single character
    static_assert(constexpr_hash("a") != constexpr_hash("b"));
    ASSERT_TRUE(constexpr_hash("a") != constexpr_hash("b"), "Different characters");
    
    // Stability - same input produces same output
    ASSERT_EQ(constexpr_hash("test"), constexpr_hash("test"), "Hash stability");
    ASSERT_EQ(constexpr_hash("Hello World"), constexpr_hash("Hello World"), "Hash stability long");
    
    // Uniqueness - different strings produce different hashes (probabilistic)
    ASSERT_TRUE(constexpr_hash("abc") != constexpr_hash("acb"), "Different order");
    ASSERT_TRUE(constexpr_hash("test") != constexpr_hash("Test"), "Case sensitivity");
    ASSERT_TRUE(constexpr_hash("hello") != constexpr_hash("world"), "Different strings");
    
    // Known test vectors
    static_assert(constexpr_hash("hello") == constexpr_hash("hello"));
    
    std::cout << colors::green() << "  ✓ All 32-bit hash tests passed" 
              << colors::reset() << std::endl;
    return true;
}

bool test_constexpr_hash_collision_resistance() {
    std::cout << colors::cyan() << "\nTesting: constexpr_hash collision resistance..."
              << colors::reset() << std::endl;
    
    // These should all be different
    uint32_t h1 = constexpr_hash("The quick brown fox");
    uint32_t h2 = constexpr_hash("The quick brown fo");
    uint32_t h3 = constexpr_hash("The quick brown foxx");
    uint32_t h4 = constexpr_hash("the quick brown fox");
    
    ASSERT_TRUE(h1 != h2, "Different length strings");
    ASSERT_TRUE(h1 != h3, "Extra character");
    ASSERT_TRUE(h1 != h4, "Case difference");
    ASSERT_TRUE(h2 != h3, "Length difference");
    
    // Special characters
    ASSERT_TRUE(constexpr_hash("test!") != constexpr_hash("test?"), "Different special chars");
    ASSERT_TRUE(constexpr_hash("123") != constexpr_hash("321"), "Number strings");
    
    std::cout << colors::green() << "  ✓ All collision resistance tests passed" 
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// Test Suite 2: Hash Functions (64-bit)
// =============================================================================

bool test_constexpr_hash64_basic() {
    std::cout << colors::cyan() << "\nTesting: constexpr_hash64 (64-bit)..."
              << colors::reset() << std::endl;
    
    // Empty string - FNV offset basis for 64-bit
    static_assert(constexpr_hash64("") == 14695981039346656037ULL);
    ASSERT_EQ(constexpr_hash64(""), 14695981039346656037ULL, "Empty string hash64");
    
    // Single character
    static_assert(constexpr_hash64("a") != constexpr_hash64("b"));
    ASSERT_TRUE(constexpr_hash64("a") != constexpr_hash64("b"), "Different characters");
    
    // Stability
    ASSERT_EQ(constexpr_hash64("test"), constexpr_hash64("test"), "Hash64 stability");
    
    // Different from 32-bit version
    static_assert(constexpr_hash("test") != constexpr_hash64("test"));
    ASSERT_TRUE(constexpr_hash("test") != constexpr_hash64("test"), "32-bit != 64-bit");
    
    // Uniqueness
    ASSERT_TRUE(constexpr_hash64("abc") != constexpr_hash64("acb"), "Different order");
    ASSERT_TRUE(constexpr_hash64("hello") != constexpr_hash64("world"), "Different strings");
    
    std::cout << colors::green() << "  ✓ All 64-bit hash tests passed" 
              << colors::reset() << std::endl;
    return true;
}

bool test_constexpr_hash64_collision_resistance() {
    std::cout << colors::cyan() << "\nTesting: constexpr_hash64 enhanced collision resistance..."
              << colors::reset() << std::endl;
    
    // 64-bit should have better collision resistance than 32-bit
    uint64_t h1 = constexpr_hash64("The quick brown fox jumps over the lazy dog");
    uint64_t h2 = constexpr_hash64("The quick brown fox jumps over the lazy do");
    uint64_t h3 = constexpr_hash64("The quick brown fox jumps over the lazy dogg");
    
    ASSERT_TRUE(h1 != h2, "Different length");
    ASSERT_TRUE(h1 != h3, "Extra character");
    ASSERT_TRUE(h2 != h3, "Length difference");
    
    // Very similar strings
    ASSERT_TRUE(constexpr_hash64("aaaaaa") != constexpr_hash64("aaaaab"), "Single char diff");
    ASSERT_TRUE(constexpr_hash64("test123") != constexpr_hash64("test124"), "Number diff");
    
    std::cout << colors::green() << "  ✓ All 64-bit collision resistance tests passed" 
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// Test Suite 3: is_power_of_two (Signed Integers)
// =============================================================================

bool test_is_power_of_two_signed() {
    std::cout << colors::cyan() << "\nTesting: is_power_of_two (signed integers)..."
              << colors::reset() << std::endl;
    
    // CRITICAL: Negative values should return false
    static_assert(!is_power_of_two(INT32_MIN), "INT32_MIN not power of 2");
    static_assert(!is_power_of_two(INT64_MIN), "INT64_MIN not power of 2");
    static_assert(!is_power_of_two(-1), "-1 not power of 2");
    static_assert(!is_power_of_two(-2), "-2 not power of 2");
    static_assert(!is_power_of_two(-4), "-4 not power of 2");
    
    ASSERT_FALSE(is_power_of_two(INT32_MIN), "INT32_MIN is NOT power of 2");
    ASSERT_FALSE(is_power_of_two(INT64_MIN), "INT64_MIN is NOT power of 2");
    ASSERT_FALSE(is_power_of_two(-1), "-1 is NOT power of 2");
    ASSERT_FALSE(is_power_of_two(-2), "-2 is NOT power of 2");
    ASSERT_FALSE(is_power_of_two(-1024), "-1024 is NOT power of 2");
    
    // Zero should return false
    static_assert(!is_power_of_two(0), "0 not power of 2");
    ASSERT_FALSE(is_power_of_two(0), "0 is NOT power of 2");
    
    // Valid powers of two
    static_assert(is_power_of_two(1), "1 is power of 2");
    static_assert(is_power_of_two(2), "2 is power of 2");
    static_assert(is_power_of_two(4), "4 is power of 2");
    static_assert(is_power_of_two(8), "8 is power of 2");
    static_assert(is_power_of_two(16), "16 is power of 2");
    static_assert(is_power_of_two(1024), "1024 is power of 2");
    static_assert(is_power_of_two(1 << 20), "2^20 is power of 2");
    
    ASSERT_TRUE(is_power_of_two(1), "1 is power of 2");
    ASSERT_TRUE(is_power_of_two(2), "2 is power of 2");
    ASSERT_TRUE(is_power_of_two(4), "4 is power of 2");
    ASSERT_TRUE(is_power_of_two(1024), "1024 is power of 2");
    ASSERT_TRUE(is_power_of_two(1 << 30), "2^30 is power of 2");
    
    // Invalid positives
    static_assert(!is_power_of_two(3), "3 not power of 2");
    static_assert(!is_power_of_two(5), "5 not power of 2");
    static_assert(!is_power_of_two(6), "6 not power of 2");
    static_assert(!is_power_of_two(INT32_MAX), "INT32_MAX not power of 2");
    
    ASSERT_FALSE(is_power_of_two(3), "3 is NOT power of 2");
    ASSERT_FALSE(is_power_of_two(7), "7 is NOT power of 2");
    ASSERT_FALSE(is_power_of_two(1023), "1023 is NOT power of 2");
    ASSERT_FALSE(is_power_of_two(INT32_MAX), "INT32_MAX is NOT power of 2");
    
    std::cout << colors::green() << "  ✓ All signed is_power_of_two tests passed" 
              << colors::reset() << std::endl;
    return true;
}

bool test_is_power_of_two_unsigned() {
    std::cout << colors::cyan() << "\nTesting: is_power_of_two (unsigned integers)..."
              << colors::reset() << std::endl;
    
    // Zero
    static_assert(!is_power_of_two(0U), "0U not power of 2");
    ASSERT_FALSE(is_power_of_two(0U), "0U is NOT power of 2");
    
    // Valid powers of two
    static_assert(is_power_of_two(1U), "1U is power of 2");
    static_assert(is_power_of_two(2U), "2U is power of 2");
    static_assert(is_power_of_two(4U), "4U is power of 2");
    static_assert(is_power_of_two(1U << 31), "2^31 is power of 2");
    
    ASSERT_TRUE(is_power_of_two(1U), "1U is power of 2");
    ASSERT_TRUE(is_power_of_two(2U), "2U is power of 2");
    ASSERT_TRUE(is_power_of_two(256U), "256U is power of 2");
    ASSERT_TRUE(is_power_of_two(UINT32_MAX / 2U + 1U), "2^31 is power of 2");
    
    // Invalid
    static_assert(!is_power_of_two(3U), "3U not power of 2");
    static_assert(!is_power_of_two(UINT32_MAX), "UINT32_MAX not power of 2");
    
    ASSERT_FALSE(is_power_of_two(3U), "3U is NOT power of 2");
    ASSERT_FALSE(is_power_of_two(255U), "255U is NOT power of 2");
    ASSERT_FALSE(is_power_of_two(UINT32_MAX), "UINT32_MAX is NOT power of 2");
    
    // 64-bit unsigned
    static_assert(is_power_of_two(1ULL << 40), "2^40 is power of 2");
    ASSERT_TRUE(is_power_of_two(1ULL << 50), "2^50 is power of 2");
    ASSERT_FALSE(is_power_of_two(UINT64_MAX), "UINT64_MAX is NOT power of 2");
    
    std::cout << colors::green() << "  ✓ All unsigned is_power_of_two tests passed" 
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// Test Suite 4: to_string_view (Integer)
// =============================================================================

bool test_to_string_view_zero() {
    std::cout << colors::cyan() << "\nTesting: to_string_view (zero)..."
              << colors::reset() << std::endl;
    
    ASSERT_EQ(to_string_view(0), "0", "Zero conversion");
    ASSERT_EQ(to_string_view(0U), "0", "Zero unsigned");
    ASSERT_EQ(to_string_view(0LL), "0", "Zero long long");
    
    std::cout << colors::green() << "  ✓ All zero tests passed" 
              << colors::reset() << std::endl;
    return true;
}

bool test_to_string_view_single_digit() {
    std::cout << colors::cyan() << "\nTesting: to_string_view (single digit)..."
              << colors::reset() << std::endl;
    
    // Positive single digits
    ASSERT_EQ(to_string_view(1), "1", "Single digit 1");
    ASSERT_EQ(to_string_view(5), "5", "Single digit 5");
    ASSERT_EQ(to_string_view(9), "9", "Single digit 9");
    
    // Negative single digits
    ASSERT_EQ(to_string_view(-1), "-1", "Negative single digit");
    ASSERT_EQ(to_string_view(-5), "-5", "Negative -5");
    ASSERT_EQ(to_string_view(-9), "-9", "Negative -9");
    
    std::cout << colors::green() << "  ✓ All single digit tests passed" 
              << colors::reset() << std::endl;
    return true;
}

bool test_to_string_view_multi_digit() {
    std::cout << colors::cyan() << "\nTesting: to_string_view (multi-digit)..."
              << colors::reset() << std::endl;
    
    // Two digits
    ASSERT_EQ(to_string_view(10), "10", "Two digits 10");
    ASSERT_EQ(to_string_view(42), "42", "Two digits 42");
    ASSERT_EQ(to_string_view(99), "99", "Two digits 99");
    
    // Three digits
    ASSERT_EQ(to_string_view(123), "123", "Three digits");
    ASSERT_EQ(to_string_view(456), "456", "Three digits 456");
    
    // Large numbers
    ASSERT_EQ(to_string_view(12345), "12345", "Five digits");
    ASSERT_EQ(to_string_view(123456789), "123456789", "Nine digits");
    
    // Negative multi-digit
    ASSERT_EQ(to_string_view(-10), "-10", "Negative two digits");
    ASSERT_EQ(to_string_view(-42), "-42", "Negative -42");
    ASSERT_EQ(to_string_view(-12345), "-12345", "Negative five digits");
    
    std::cout << colors::green() << "  ✓ All multi-digit tests passed" 
              << colors::reset() << std::endl;
    return true;
}

bool test_to_string_view_edge_cases() {
    std::cout << colors::cyan() << "\nTesting: to_string_view (edge cases)..."
              << colors::reset() << std::endl;
    
    // INT32 limits
    ASSERT_EQ(to_string_view(INT32_MAX), "2147483647", "INT32_MAX");
    ASSERT_EQ(to_string_view(INT32_MIN), "-2147483648", "INT32_MIN (CRITICAL)");
    
    // UINT32 limits
    ASSERT_EQ(to_string_view(UINT32_MAX), "4294967295", "UINT32_MAX");
    
    // INT64 limits
    ASSERT_EQ(to_string_view(INT64_MAX), "9223372036854775807", "INT64_MAX");
    ASSERT_EQ(to_string_view(INT64_MIN), "-9223372036854775808", "INT64_MIN");
    
    // UINT64 limits
    ASSERT_EQ(to_string_view(UINT64_MAX), "18446744073709551615", "UINT64_MAX");
    
    std::cout << colors::green() << "  ✓ All edge case tests passed" 
              << colors::reset() << std::endl;
    return true;
}

bool test_to_string_view_c_string() {
    std::cout << colors::cyan() << "\nTesting: to_string_view (C-string)..."
              << colors::reset() << std::endl;
    
    ASSERT_EQ(to_string_view("test"), "test", "C-string conversion");
    ASSERT_EQ(to_string_view("Hello World"), "Hello World", "C-string with space");
    ASSERT_EQ(to_string_view(""), "", "Empty C-string");
    
    std::cout << colors::green() << "  ✓ All C-string tests passed" 
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// Test Suite 5: to_string_view (Floating-Point)
// =============================================================================

bool test_to_string_view_float_basic() {
    std::cout << colors::cyan() << "\nTesting: to_string_view (floating-point basic)..."
              << colors::reset() << std::endl;
    
    // Zero
    auto zero = to_string_view(0.0);
    ASSERT_TRUE(zero == "0.000000" || zero.substr(0, 2) == "0.", "Float zero");
    
    // Positive
    auto positive = to_string_view(3.14);
    ASSERT_TRUE(positive.substr(0, 3) == "3.1", "Float positive");
    
    // Negative
    auto negative = to_string_view(-2.5);
    ASSERT_TRUE(negative[0] == '-', "Float negative sign");
    ASSERT_TRUE(negative.find("2.5") != std::string_view::npos, "Float negative value");
    
    // Precision
    auto prec2 = to_string_view(3.14159, 2);
    ASSERT_TRUE(prec2.find("3.14") != std::string_view::npos, "Float precision 2");
    
    std::cout << colors::green() << "  ✓ All basic floating-point tests passed" 
              << colors::reset() << std::endl;
    return true;
}

bool test_to_string_view_float_special() {
    std::cout << colors::cyan() << "\nTesting: to_string_view (floating-point special values)..."
              << colors::reset() << std::endl;
    
    // NaN
    auto nan_val = to_string_view(std::numeric_limits<double>::quiet_NaN());
    ASSERT_EQ(nan_val, "nan", "NaN representation");
    
    // Infinity
    auto inf_val = to_string_view(std::numeric_limits<double>::infinity());
    ASSERT_EQ(inf_val, "inf", "Positive infinity");
    
    auto neg_inf_val = to_string_view(-std::numeric_limits<double>::infinity());
    ASSERT_EQ(neg_inf_val, "-inf", "Negative infinity");
    
    std::cout << colors::green() << "  ✓ All special floating-point tests passed" 
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// Test Suite 6: constexpr_concat
// =============================================================================

bool test_constexpr_concat_basic() {
    std::cout << colors::cyan() << "\nTesting: constexpr_concat (basic)..."
              << colors::reset() << std::endl;
    
    // Two strings
    auto s1 = constexpr_concat("Hello", " World");
    ASSERT_EQ(s1.size(), 11, "Two strings size");
    char buf1[12];
    s1.to_array(buf1, sizeof(buf1));
    ASSERT_EQ(std::string_view(buf1), "Hello World", "Two strings");
    
    // Three strings
    auto s2 = constexpr_concat("A", "B", "C");
    ASSERT_EQ(s2.size(), 3, "Three strings size");
    char buf2[4];
    s2.to_array(buf2, sizeof(buf2));
    ASSERT_EQ(std::string_view(buf2), "ABC", "Three strings");
    
    // Multiple strings
    auto s3 = constexpr_concat("One", " ", "Two", " ", "Three");
    char buf3[20];
    s3.to_array(buf3, sizeof(buf3));
    ASSERT_EQ(std::string_view(buf3), "One Two Three", "Five strings");
    
    std::cout << colors::green() << "  ✓ All basic concat tests passed" 
              << colors::reset() << std::endl;
    return true;
}

bool test_constexpr_concat_with_integers() {
    std::cout << colors::cyan() << "\nTesting: constexpr_concat (with integers)..."
              << colors::reset() << std::endl;
    
    // String + integer
    auto s1 = constexpr_concat("Value: ", to_string_view(42));
    char buf1[128];  // Fixed size for MSVC compatibility
    s1.to_array(buf1, sizeof(buf1));
    ASSERT_EQ(std::string_view(buf1), "Value: 42", "String + integer");
    
    // Multiple integers
    auto s2 = constexpr_concat("Numbers: ", to_string_view(1), ", ", to_string_view(2), ", ", to_string_view(3));
    char buf2[128];  // Fixed size for MSVC compatibility
    s2.to_array(buf2, sizeof(buf2));
    ASSERT_EQ(std::string_view(buf2), "Numbers: 1, 2, 3", "Multiple integers");
    
    // Negative integers
    auto s3 = constexpr_concat("Negative: ", to_string_view(-42));
    char buf3[128];  // Fixed size for MSVC compatibility
    s3.to_array(buf3, sizeof(buf3));
    ASSERT_EQ(std::string_view(buf3), "Negative: -42", "Negative integer");
    
    // Large integers
    auto s4 = constexpr_concat("Max: ", to_string_view(INT32_MAX));
    char buf4[128];  // Fixed size for MSVC compatibility
    s4.to_array(buf4, sizeof(buf4));
    ASSERT_EQ(std::string_view(buf4), "Max: 2147483647", "INT32_MAX");
    
    std::cout << colors::green() << "  ✓ All concat with integers tests passed" 
              << colors::reset() << std::endl;
    return true;
}

bool test_constexpr_concat_edge_cases() {
    std::cout << colors::cyan() << "\nTesting: constexpr_concat (edge cases)..."
              << colors::reset() << std::endl;
    
    // Empty strings
    auto empty = constexpr_concat("", "", "");
    ASSERT_EQ(empty.size(), 0, "Empty concat");
    
    // Single string
    auto single = constexpr_concat("Single");
    ASSERT_EQ(single.size(), 6, "Single string size");
    char buf_single[7];
    single.to_array(buf_single, sizeof(buf_single));
    ASSERT_EQ(std::string_view(buf_single), "Single", "Single string");
    
    // Very long concatenation
    auto long_str = constexpr_concat(
        "This ", "is ", "a ", "very ", "long ", "string ", "that ", "tests ",
        "the ", "limits ", "of ", "compile-time ", "concatenation. ",
        "Value: ", to_string_view(INT64_MAX)
    );
    ASSERT_TRUE(long_str.size() > 50, "Long string handling");
    
    std::cout << colors::green() << "  ✓ All concat edge case tests passed" 
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// Test Suite 7: ConstexprString::to_string()
// =============================================================================

bool test_constexpr_string_to_string() {
    std::cout << colors::cyan() << "\nTesting: ConstexprString::to_string()..."
              << colors::reset() << std::endl;
    
    // Basic conversion
    auto cs1 = constexpr_concat("Hello ", "World");
    std::string result1 = cs1.to_string();
    ASSERT_EQ(result1, "Hello World", "Basic to_string");
    ASSERT_EQ(result1.size(), cs1.size(), "Size matches");
    
    // With integers
    auto cs2 = constexpr_concat("Count: ", to_string_view(123));
    std::string result2 = cs2.to_string();
    ASSERT_EQ(result2, "Count: 123", "to_string with integer");
    
    // Empty
    auto cs3 = constexpr_concat("", "");
    std::string result3 = cs3.to_string();
    ASSERT_EQ(result3, "", "Empty to_string");
    ASSERT_EQ(result3.size(), 0, "Empty size");
    
    // Complex
    auto cs4 = constexpr_concat(
        "Result: ", to_string_view(42), " (", to_string_view(-10), ")"
    );
    std::string result4 = cs4.to_string();
    ASSERT_EQ(result4, "Result: 42 (-10)", "Complex to_string");
    
    std::cout << colors::green() << "  ✓ All to_string() tests passed" 
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// Test Suite 9: Integration Tests
// =============================================================================

bool test_integration_complex_scenarios() {
    std::cout << colors::cyan() << "\nTesting: Complex integration scenarios..."
              << colors::reset() << std::endl;
    
    // Scenario 1: Error message generation
    auto error_msg = constexpr_concat(
        "Error at line ", to_string_view(42), ": Invalid value ", to_string_view(-1)
    );
    auto error_str = error_msg.to_string();
    ASSERT_EQ(error_str, "Error at line 42: Invalid value -1", "Error message");
    
    // Scenario 2: Configuration string
    auto config = constexpr_concat(
        "Config: threads=", to_string_view(8), 
        ", buffer_size=", to_string_view(1024),
        ", enabled=", to_string_view(1)
    );
    auto config_str = config.to_string();
    ASSERT_TRUE(config_str.find("threads=8") != std::string::npos, "Config threads");
    ASSERT_TRUE(config_str.find("buffer_size=1024") != std::string::npos, "Config buffer");
    
    // Scenario 3: Hash-based switch
    constexpr uint32_t cmd_hash = constexpr_hash("start");
    constexpr uint32_t expected = constexpr_hash("start");
    static_assert(cmd_hash == expected);
    ASSERT_EQ(cmd_hash, expected, "Hash-based switch");
    
    // Scenario 4: Power-of-two validation with error message
    constexpr int buffer_size = 1024;
    static_assert(is_power_of_two(buffer_size));
    if (!is_power_of_two(buffer_size)) {
        auto msg = constexpr_concat(
            "Buffer size ", to_string_view(buffer_size), " must be power of two"
        ).to_string();
        SIMPLE_ASSERT(false, msg.c_str());
    }
    ASSERT_TRUE(is_power_of_two(buffer_size), "Power of two validation");
    
    std::cout << colors::green() << "  ✓ All integration tests passed" 
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// Performance Benchmarks
// =============================================================================

void run_constexpr_utilities_benchmarks() {
    std::cout << "\n" << colors::bold() << colors::cyan()
              << "=== ConstexprUtilities Performance Benchmarks ==="
              << colors::reset() << std::endl;

    const size_t ITERATIONS = 10'000'000;
    
    // Hash benchmarks
    benchmark("constexpr_hash (short string)", []() {
        volatile uint32_t result = constexpr_hash("test");
        (void)result;
    }, ITERATIONS);
    
    benchmark("constexpr_hash (long string)", []() {
        volatile uint32_t result = constexpr_hash("The quick brown fox jumps over the lazy dog");
        (void)result;
    }, ITERATIONS);
    
    benchmark("constexpr_hash64 (short string)", []() {
        volatile uint64_t result = constexpr_hash64("test");
        (void)result;
    }, ITERATIONS);
    
    benchmark("constexpr_hash64 (long string)", []() {
        volatile uint64_t result = constexpr_hash64("The quick brown fox jumps over the lazy dog");
        (void)result;
    }, ITERATIONS);
    
    // is_power_of_two benchmarks
    benchmark("is_power_of_two (positive)", []() {
        volatile bool result = is_power_of_two(1024);
        (void)result;
    }, ITERATIONS);
    
    benchmark("is_power_of_two (negative)", []() {
        volatile bool result = is_power_of_two(-1);
        (void)result;
    }, ITERATIONS);
    
    benchmark("is_power_of_two (non-power)", []() {
        volatile bool result = is_power_of_two(1023);
        (void)result;
    }, ITERATIONS);
    
    // String conversion benchmarks
    benchmark("to_string_view (single digit)", []() {
        auto sv = to_string_view(5);
        volatile std::size_t len = sv.size();
        (void)len;
    }, ITERATIONS);
    
    benchmark("to_string_view (multi-digit)", []() {
        auto sv = to_string_view(12345);
        volatile std::size_t len = sv.size();
        (void)len;
    }, ITERATIONS);
    
    benchmark("to_string_view (negative)", []() {
        auto sv = to_string_view(-12345);
        volatile std::size_t len = sv.size();
        (void)len;
    }, ITERATIONS);
    
    benchmark("to_string_view (INT32_MAX)", []() {
        auto sv = to_string_view(INT32_MAX);
        volatile std::size_t len = sv.size();
        (void)len;
    }, ITERATIONS);
    
    // Concatenation benchmarks
    benchmark("constexpr_concat (2 strings)", []() {
        auto cs = constexpr_concat("Hello", " World");
        volatile std::size_t size = cs.size();
        (void)size;
    }, ITERATIONS);
    
    benchmark("constexpr_concat (with integer)", []() {
        auto cs = constexpr_concat("Value: ", to_string_view(42));
        volatile std::size_t size = cs.size();
        (void)size;
    }, ITERATIONS);
    
    // Runtime conversion benchmark (lower iterations due to allocation)
    benchmark("ConstexprString::to_string()", []() {
        auto cs = constexpr_concat("Value: ", to_string_view(123));
        auto s = cs.to_string();
        volatile std::size_t len = s.size();
        (void)len;
    }, 1'000'000);
    
    std::cout << "\n" << colors::blue()
              << "[NOTE] Benchmarks measure runtime performance with thread-local storage"
              << colors::reset() << std::endl;
}

// =============================================================================
// Main Test Runner
// =============================================================================

bool test_ConstexprUtilities() {
    std::cout << colors::bold() << colors::cyan()
              << "\n======================================"<< std::endl;
    std::cout << "ConstexprUtilities v2.0 - Complete Test Suite" << std::endl;
    std::cout << "Enhanced with Full Integer Support, 64-bit Hash, and Floating-Point" << std::endl;
    std::cout << "======================================"
              << colors::reset() << std::endl;

    TestRunner runner;
    
    // Hash tests
    runner.run_test("constexpr_hash (32-bit basic)", test_constexpr_hash_basic);
    runner.run_test("constexpr_hash (collision resistance)", test_constexpr_hash_collision_resistance);
    runner.run_test("constexpr_hash64 (64-bit basic)", test_constexpr_hash64_basic);
    runner.run_test("constexpr_hash64 (collision resistance)", test_constexpr_hash64_collision_resistance);
    
    // is_power_of_two tests
    runner.run_test("is_power_of_two (signed)", test_is_power_of_two_signed);
    runner.run_test("is_power_of_two (unsigned)", test_is_power_of_two_unsigned);
    
    // Integer string conversion tests
    runner.run_test("to_string_view (zero)", test_to_string_view_zero);
    runner.run_test("to_string_view (single digit)", test_to_string_view_single_digit);
    runner.run_test("to_string_view (multi-digit)", test_to_string_view_multi_digit);
    runner.run_test("to_string_view (edge cases)", test_to_string_view_edge_cases);
    runner.run_test("to_string_view (C-string)", test_to_string_view_c_string);
    
    // Floating-point tests
    runner.run_test("to_string_view (float basic)", test_to_string_view_float_basic);
    runner.run_test("to_string_view (float special)", test_to_string_view_float_special);
    
    // Concatenation tests
    runner.run_test("constexpr_concat (basic)", test_constexpr_concat_basic);
    runner.run_test("constexpr_concat (with integers)", test_constexpr_concat_with_integers);
    runner.run_test("constexpr_concat (edge cases)", test_constexpr_concat_edge_cases);
    
    // Runtime conversion tests
    runner.run_test("ConstexprString::to_string()", test_constexpr_string_to_string);
    
    // Integration tests
    runner.run_test("Integration (complex scenarios)", test_integration_complex_scenarios);
    
    int failed = runner.print_summary();
    
    if (failed == 0) {
        run_constexpr_utilities_benchmarks();
    }
    
    return failed == 0;
}

} // namespace cpp_utilities::testing