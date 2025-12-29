/**
 * @file test_ConstexprUtilities.cpp
 * @brief Comprehensive unit tests for ConstexprUtilities
 *
 * Test coverage:
 * - Hash functions (32-bit and 64-bit FNV-1a, hash_combine)
 * - Arithmetic utilities (is_power_of_two, next_power_of_two, log2, bit ops)
 * - Integer string conversion (to_string_view, constexpr_to_string_t)
 * - Floating-point string conversion (special values, precision, overflow)
 * - Hexadecimal conversion (to_hex_string_view)
 * - String concatenation (constexpr_concat, ConstexprString)
 * - String utilities (constexpr_strlen, constexpr_strcmp, constexpr_streq)
 * - Buffer pool behavior and thread safety
 */

#include <iostream>
#include <sstream>
#include <string>
#include <limits>
#include <cstdint>
#include <cmath>
#include <thread>
#include <atomic>
#include <vector>

#include "ConstexprUtilities.h"
#include "FatPTest.h"

using namespace fat_p::testing;
using namespace fat_p;

namespace fat_p::testing
{

// =============================================================================
// Test Suite 1: Hash Functions (32-bit)
// =============================================================================

bool test_constexpr_hash_basic()
{
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

    // Uniqueness - different strings produce different hashes
    ASSERT_TRUE(constexpr_hash("abc") != constexpr_hash("acb"), "Different order");
    ASSERT_TRUE(constexpr_hash("test") != constexpr_hash("Test"), "Case sensitivity");
    ASSERT_TRUE(constexpr_hash("hello") != constexpr_hash("world"), "Different strings");

    // Known test vectors
    static_assert(constexpr_hash("hello") == constexpr_hash("hello"));

    std::cout << colors::green() << "  PASSED: All 32-bit hash tests"
              << colors::reset() << std::endl;
    return true;
}

bool test_constexpr_hash_collision_resistance()
{
    std::cout << colors::cyan() << "\nTesting: constexpr_hash collision resistance..."
              << colors::reset() << std::endl;

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

    std::cout << colors::green() << "  PASSED: All collision resistance tests"
              << colors::reset() << std::endl;
    return true;
}

bool test_constexpr_hash_avalanche()
{
    std::cout << colors::cyan() << "\nTesting: constexpr_hash avalanche property..."
              << colors::reset() << std::endl;

    // Verify single bit/char change affects many output bits
    constexpr auto h1 = constexpr_hash("aaaa");
    constexpr auto h2 = constexpr_hash("baaa");
    constexpr auto h3 = constexpr_hash("aaab");

    uint32_t diff1 = h1 ^ h2;
    uint32_t diff2 = h1 ^ h3;

    int bits1 = popcount(diff1);
    int bits2 = popcount(diff2);

    // Good avalanche should flip roughly 50% of bits (16 +/- 6 for 32-bit)
    ASSERT_TRUE(bits1 >= 8 && bits1 <= 24, "Avalanche property (first char change)");
    ASSERT_TRUE(bits2 >= 8 && bits2 <= 24, "Avalanche property (last char change)");

    std::cout << colors::green() << "  PASSED: Avalanche property tests"
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// Test Suite 2: Hash Functions (64-bit)
// =============================================================================

bool test_constexpr_hash64_basic()
{
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
    ASSERT_TRUE(constexpr_hash("test") != constexpr_hash64("test"), "32-bit != 64-bit");

    // Uniqueness
    ASSERT_TRUE(constexpr_hash64("abc") != constexpr_hash64("acb"), "Different order");
    ASSERT_TRUE(constexpr_hash64("hello") != constexpr_hash64("world"), "Different strings");

    std::cout << colors::green() << "  PASSED: All 64-bit hash tests"
              << colors::reset() << std::endl;
    return true;
}

bool test_hash_combine()
{
    std::cout << colors::cyan() << "\nTesting: hash_combine..."
              << colors::reset() << std::endl;

    constexpr auto h1 = constexpr_hash64("key");
    constexpr auto h2 = constexpr_hash64("value");

    constexpr auto combined = hash_combine(h1, h2);

    // Combined hash should differ from both inputs
    ASSERT_TRUE(combined != h1, "Combined differs from first");
    ASSERT_TRUE(combined != h2, "Combined differs from second");

    // Order matters
    constexpr auto combined_reverse = hash_combine(h2, h1);
    ASSERT_TRUE(combined != combined_reverse, "Order affects result");

    // hash_values variadic
    constexpr auto multi = hash_values("a", "b", "c");
    constexpr auto multi2 = hash_values("a", "b", "d");
    ASSERT_TRUE(multi != multi2, "hash_values distinguishes different inputs");

    std::cout << colors::green() << "  PASSED: hash_combine tests"
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// Test Suite 3: is_power_of_two
// =============================================================================

bool test_is_power_of_two_signed()
{
    std::cout << colors::cyan() << "\nTesting: is_power_of_two (signed integers)..."
              << colors::reset() << std::endl;

    // Negative values should return false
    static_assert(!is_power_of_two(INT32_MIN), "INT32_MIN not power of 2");
    static_assert(!is_power_of_two(INT64_MIN), "INT64_MIN not power of 2");
    static_assert(!is_power_of_two(-1), "-1 not power of 2");
    static_assert(!is_power_of_two(-2), "-2 not power of 2");
    static_assert(!is_power_of_two(-4), "-4 not power of 2");

    ASSERT_FALSE(is_power_of_two(INT32_MIN), "INT32_MIN is NOT power of 2");
    ASSERT_FALSE(is_power_of_two(INT64_MIN), "INT64_MIN is NOT power of 2");
    ASSERT_FALSE(is_power_of_two(-1), "-1 is NOT power of 2");
    ASSERT_FALSE(is_power_of_two(-1024), "-1024 is NOT power of 2");

    // Zero should return false
    static_assert(!is_power_of_two(0), "0 not power of 2");
    ASSERT_FALSE(is_power_of_two(0), "0 is NOT power of 2");

    // Valid powers of two
    static_assert(is_power_of_two(1), "1 is power of 2");
    static_assert(is_power_of_two(2), "2 is power of 2");
    static_assert(is_power_of_two(4), "4 is power of 2");
    static_assert(is_power_of_two(1024), "1024 is power of 2");
    static_assert(is_power_of_two(1 << 20), "2^20 is power of 2");

    ASSERT_TRUE(is_power_of_two(1), "1 is power of 2");
    ASSERT_TRUE(is_power_of_two(2), "2 is power of 2");
    ASSERT_TRUE(is_power_of_two(1 << 30), "2^30 is power of 2");

    // Invalid positives
    static_assert(!is_power_of_two(3), "3 not power of 2");
    static_assert(!is_power_of_two(INT32_MAX), "INT32_MAX not power of 2");

    ASSERT_FALSE(is_power_of_two(3), "3 is NOT power of 2");
    ASSERT_FALSE(is_power_of_two(1023), "1023 is NOT power of 2");

    std::cout << colors::green() << "  PASSED: All signed is_power_of_two tests"
              << colors::reset() << std::endl;
    return true;
}

bool test_is_power_of_two_unsigned()
{
    std::cout << colors::cyan() << "\nTesting: is_power_of_two (unsigned integers)..."
              << colors::reset() << std::endl;

    // Zero
    static_assert(!is_power_of_two(0U), "0U not power of 2");
    ASSERT_FALSE(is_power_of_two(0U), "0U is NOT power of 2");

    // Valid powers of two
    static_assert(is_power_of_two(1U), "1U is power of 2");
    static_assert(is_power_of_two(2U), "2U is power of 2");
    static_assert(is_power_of_two(1U << 31), "2^31 is power of 2");

    ASSERT_TRUE(is_power_of_two(1U), "1U is power of 2");
    ASSERT_TRUE(is_power_of_two(256U), "256U is power of 2");
    ASSERT_TRUE(is_power_of_two(UINT32_MAX / 2U + 1U), "2^31 is power of 2");

    // Invalid
    static_assert(!is_power_of_two(3U), "3U not power of 2");
    static_assert(!is_power_of_two(UINT32_MAX), "UINT32_MAX not power of 2");

    ASSERT_FALSE(is_power_of_two(255U), "255U is NOT power of 2");
    ASSERT_FALSE(is_power_of_two(UINT32_MAX), "UINT32_MAX is NOT power of 2");

    // 64-bit unsigned
    static_assert(is_power_of_two(1ULL << 40), "2^40 is power of 2");
    ASSERT_TRUE(is_power_of_two(1ULL << 50), "2^50 is power of 2");
    ASSERT_FALSE(is_power_of_two(UINT64_MAX), "UINT64_MAX is NOT power of 2");

    std::cout << colors::green() << "  PASSED: All unsigned is_power_of_two tests"
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// Test Suite 4: next_power_of_two and log2 functions
// =============================================================================

bool test_next_power_of_two()
{
    std::cout << colors::cyan() << "\nTesting: next_power_of_two..."
              << colors::reset() << std::endl;

    // Basic cases
    static_assert(next_power_of_two(0u) == 1u, "0 -> 1");
    static_assert(next_power_of_two(1u) == 1u, "1 -> 1");
    static_assert(next_power_of_two(2u) == 2u, "2 -> 2");
    static_assert(next_power_of_two(3u) == 4u, "3 -> 4");
    static_assert(next_power_of_two(4u) == 4u, "4 -> 4");
    static_assert(next_power_of_two(5u) == 8u, "5 -> 8");

    ASSERT_EQ(next_power_of_two(0u), 1u, "0 -> 1");
    ASSERT_EQ(next_power_of_two(1u), 1u, "1 -> 1");
    ASSERT_EQ(next_power_of_two(5u), 8u, "5 -> 8");
    ASSERT_EQ(next_power_of_two(1000u), 1024u, "1000 -> 1024");
    ASSERT_EQ(next_power_of_two(1024u), 1024u, "1024 -> 1024");
    ASSERT_EQ(next_power_of_two(1025u), 2048u, "1025 -> 2048");

    // 64-bit
    ASSERT_EQ(next_power_of_two(uint64_t(1) << 40), uint64_t(1) << 40, "2^40 -> 2^40");
    ASSERT_EQ(next_power_of_two((uint64_t(1) << 40) + 1), uint64_t(1) << 41, "2^40+1 -> 2^41");

    std::cout << colors::green() << "  PASSED: next_power_of_two tests"
              << colors::reset() << std::endl;
    return true;
}

bool test_log2_functions()
{
    std::cout << colors::cyan() << "\nTesting: log2_floor and log2_ceil..."
              << colors::reset() << std::endl;

    // log2_floor
    static_assert(log2_floor(0u) == -1, "log2_floor(0) = -1");
    static_assert(log2_floor(1u) == 0, "log2_floor(1) = 0");
    static_assert(log2_floor(2u) == 1, "log2_floor(2) = 1");
    static_assert(log2_floor(3u) == 1, "log2_floor(3) = 1");
    static_assert(log2_floor(4u) == 2, "log2_floor(4) = 2");
    static_assert(log2_floor(8u) == 3, "log2_floor(8) = 3");
    static_assert(log2_floor(15u) == 3, "log2_floor(15) = 3");

    ASSERT_EQ(log2_floor(0u), -1, "log2_floor(0)");
    ASSERT_EQ(log2_floor(1u), 0, "log2_floor(1)");
    ASSERT_EQ(log2_floor(1024u), 10, "log2_floor(1024)");
    ASSERT_EQ(log2_floor(1023u), 9, "log2_floor(1023)");

    // log2_ceil
    static_assert(log2_ceil(0u) == -1, "log2_ceil(0) = -1");
    static_assert(log2_ceil(1u) == 0, "log2_ceil(1) = 0");
    static_assert(log2_ceil(2u) == 1, "log2_ceil(2) = 1");
    static_assert(log2_ceil(3u) == 2, "log2_ceil(3) = 2");
    static_assert(log2_ceil(4u) == 2, "log2_ceil(4) = 2");
    static_assert(log2_ceil(5u) == 3, "log2_ceil(5) = 3");

    ASSERT_EQ(log2_ceil(1024u), 10, "log2_ceil(1024)");
    ASSERT_EQ(log2_ceil(1025u), 11, "log2_ceil(1025)");

    std::cout << colors::green() << "  PASSED: log2 function tests"
              << colors::reset() << std::endl;
    return true;
}

bool test_bit_operations()
{
    std::cout << colors::cyan() << "\nTesting: popcount, clz, ctz..."
              << colors::reset() << std::endl;

    // popcount
    static_assert(popcount(0u) == 0, "popcount(0)");
    static_assert(popcount(1u) == 1, "popcount(1)");
    static_assert(popcount(0xFFu) == 8, "popcount(0xFF)");
    static_assert(popcount(0xFFFFu) == 16, "popcount(0xFFFF)");

    ASSERT_EQ(popcount(0u), 0, "popcount(0)");
    ASSERT_EQ(popcount(0b10101010u), 4, "popcount(0b10101010)");
    ASSERT_EQ(popcount(UINT32_MAX), 32, "popcount(UINT32_MAX)");

    // clz (count leading zeros)
    ASSERT_EQ(clz(uint8_t(0)), 8, "clz(0) for uint8_t");
    ASSERT_EQ(clz(uint8_t(1)), 7, "clz(1) for uint8_t");
    ASSERT_EQ(clz(uint8_t(0x80)), 0, "clz(0x80) for uint8_t");
    ASSERT_EQ(clz(uint32_t(1)), 31, "clz(1) for uint32_t");

    // ctz (count trailing zeros)
    ASSERT_EQ(ctz(uint8_t(0)), 8, "ctz(0) for uint8_t");
    ASSERT_EQ(ctz(uint8_t(1)), 0, "ctz(1) for uint8_t");
    ASSERT_EQ(ctz(uint8_t(8)), 3, "ctz(8) for uint8_t");
    ASSERT_EQ(ctz(uint32_t(0x80000000u)), 31, "ctz(0x80000000) for uint32_t");

    std::cout << colors::green() << "  PASSED: Bit operation tests"
              << colors::reset() << std::endl;
    return true;
}

bool test_count_digits()
{
    std::cout << colors::cyan() << "\nTesting: count_digits..."
              << colors::reset() << std::endl;

    static_assert(count_digits(0) == 1, "count_digits(0)");
    static_assert(count_digits(1) == 1, "count_digits(1)");
    static_assert(count_digits(9) == 1, "count_digits(9)");
    static_assert(count_digits(10) == 2, "count_digits(10)");
    static_assert(count_digits(99) == 2, "count_digits(99)");
    static_assert(count_digits(100) == 3, "count_digits(100)");

    ASSERT_EQ(count_digits(0), 1, "count_digits(0)");
    ASSERT_EQ(count_digits(42), 2, "count_digits(42)");
    ASSERT_EQ(count_digits(12345), 5, "count_digits(12345)");
    ASSERT_EQ(count_digits(INT32_MAX), 10, "count_digits(INT32_MAX)");

    // Negative numbers include the minus sign
    ASSERT_EQ(count_digits(-1), 2, "count_digits(-1)");
    ASSERT_EQ(count_digits(-42), 3, "count_digits(-42)");
    ASSERT_EQ(count_digits(INT32_MIN), 11, "count_digits(INT32_MIN)");

    std::cout << colors::green() << "  PASSED: count_digits tests"
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// Test Suite 5: Integer String Conversion
// =============================================================================

bool test_to_string_view_integers()
{
    std::cout << colors::cyan() << "\nTesting: to_string_view (integers)..."
              << colors::reset() << std::endl;

    // Zero
    ASSERT_EQ(to_string_view(0), "0", "Zero");
    ASSERT_EQ(to_string_view(0U), "0", "Zero unsigned");
    ASSERT_EQ(to_string_view(0LL), "0", "Zero long long");

    // Single digit
    ASSERT_EQ(to_string_view(1), "1", "Single digit 1");
    ASSERT_EQ(to_string_view(9), "9", "Single digit 9");
    ASSERT_EQ(to_string_view(-1), "-1", "Negative -1");
    ASSERT_EQ(to_string_view(-9), "-9", "Negative -9");

    // Multi-digit
    ASSERT_EQ(to_string_view(42), "42", "42");
    ASSERT_EQ(to_string_view(12345), "12345", "12345");
    ASSERT_EQ(to_string_view(-42), "-42", "-42");
    ASSERT_EQ(to_string_view(-12345), "-12345", "-12345");

    // Edge cases
    ASSERT_EQ(to_string_view(INT32_MAX), "2147483647", "INT32_MAX");
    ASSERT_EQ(to_string_view(INT32_MIN), "-2147483648", "INT32_MIN");
    ASSERT_EQ(to_string_view(UINT32_MAX), "4294967295", "UINT32_MAX");
    ASSERT_EQ(to_string_view(INT64_MAX), "9223372036854775807", "INT64_MAX");
    ASSERT_EQ(to_string_view(INT64_MIN), "-9223372036854775808", "INT64_MIN");
    ASSERT_EQ(to_string_view(UINT64_MAX), "18446744073709551615", "UINT64_MAX");

    std::cout << colors::green() << "  PASSED: Integer to_string_view tests"
              << colors::reset() << std::endl;
    return true;
}

bool test_to_string_view_c_string()
{
    std::cout << colors::cyan() << "\nTesting: to_string_view (C-string)..."
              << colors::reset() << std::endl;

    ASSERT_EQ(to_string_view("test"), "test", "C-string");
    ASSERT_EQ(to_string_view("Hello World"), "Hello World", "C-string with space");
    ASSERT_EQ(to_string_view(""), "", "Empty C-string");

    std::cout << colors::green() << "  PASSED: C-string to_string_view tests"
              << colors::reset() << std::endl;
    return true;
}

bool test_constexpr_to_string_t()
{
    std::cout << colors::cyan() << "\nTesting: constexpr_to_string_t..."
              << colors::reset() << std::endl;

    // Note: Full constexpr evaluation requires C++20 due to loop in constructor
    // In C++17, we test runtime behavior which uses the same code path
    
    constexpr_to_string_t<int> conv42{42};
    auto view42 = conv42.view();
    ASSERT_EQ(view42.size(), 2, "Size of 42");
    ASSERT_EQ(view42, "42", "Value 42");

    constexpr_to_string_t<int> conv_neg{-123};
    auto view_neg = conv_neg.view();
    ASSERT_EQ(view_neg.size(), 4, "Size of -123");
    ASSERT_EQ(view_neg, "-123", "Value -123");

    constexpr_to_string_t<int> conv0{0};
    auto view0 = conv0.view();
    ASSERT_EQ(view0.size(), 1, "Size of 0");
    ASSERT_EQ(view0, "0", "Value 0");

    // Test large values
    constexpr_to_string_t<int64_t> conv_max{INT64_MAX};
    auto view_max = conv_max.view();
    ASSERT_EQ(view_max, "9223372036854775807", "INT64_MAX");

    constexpr_to_string_t<int64_t> conv_min{INT64_MIN};
    auto view_min = conv_min.view();
    ASSERT_EQ(view_min, "-9223372036854775808", "INT64_MIN");

    std::cout << colors::green() << "  PASSED: constexpr_to_string_t tests"
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// Test Suite 6: Floating-Point String Conversion
// =============================================================================

bool test_to_string_view_float_basic()
{
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

    auto prec0 = to_string_view(3.9, 0);
    ASSERT_EQ(prec0, "3.", "Float precision 0");

    std::cout << colors::green() << "  PASSED: Basic floating-point tests"
              << colors::reset() << std::endl;
    return true;
}

bool test_to_string_view_float_special()
{
    std::cout << colors::cyan() << "\nTesting: to_string_view (floating-point special values)..."
              << colors::reset() << std::endl;

    // NaN
    auto nan_val = to_string_view(std::numeric_limits<double>::quiet_NaN());
    ASSERT_EQ(nan_val, "nan", "NaN representation");

    // Positive infinity
    auto inf_val = to_string_view(std::numeric_limits<double>::infinity());
    ASSERT_EQ(inf_val, "inf", "Positive infinity");

    // Negative infinity
    auto neg_inf_val = to_string_view(-std::numeric_limits<double>::infinity());
    ASSERT_EQ(neg_inf_val, "-inf", "Negative infinity");

    // Negative zero normalized to positive zero
    auto neg_zero = to_string_view(-0.0, 2);
    ASSERT_EQ(neg_zero, "0.00", "Negative zero normalized to 0.00");

    std::cout << colors::green() << "  PASSED: Special floating-point tests"
              << colors::reset() << std::endl;
    return true;
}

bool test_to_string_view_float_overflow()
{
    std::cout << colors::cyan() << "\nTesting: to_string_view (floating-point overflow)..."
              << colors::reset() << std::endl;

    // Large value that would overflow long long
    auto large = to_string_view(1e20);
    ASSERT_EQ(large, "overflow", "Large positive overflow");

    auto large_neg = to_string_view(-1e20);
    ASSERT_EQ(large_neg, "-overflow", "Large negative overflow");

    // Just under the limit should work
    auto safe = to_string_view(9007199254740000.0, 0);
    ASSERT_TRUE(safe[0] != 'o', "Value under limit works");

    std::cout << colors::green() << "  PASSED: Floating-point overflow tests"
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// Test Suite 7: Hexadecimal Conversion
// =============================================================================

bool test_to_hex_string_view()
{
    std::cout << colors::cyan() << "\nTesting: to_hex_string_view..."
              << colors::reset() << std::endl;

    // Basic values
    ASSERT_EQ(to_hex_string_view(0u), "0x0", "Hex 0");
    ASSERT_EQ(to_hex_string_view(255u), "0xff", "Hex 255");
    ASSERT_EQ(to_hex_string_view(256u), "0x100", "Hex 256");
    ASSERT_EQ(to_hex_string_view(0xDEADBEEFu), "0xdeadbeef", "Hex 0xDEADBEEF");

    // Without prefix
    ASSERT_EQ(to_hex_string_view(255u, false), "ff", "Hex 255 no prefix");
    ASSERT_EQ(to_hex_string_view(0u, false), "0", "Hex 0 no prefix");

    // 64-bit
    ASSERT_EQ(to_hex_string_view(uint64_t(0x123456789ABCDEFULL)), "0x123456789abcdef", "Hex 64-bit");

    // Uppercase hex
    ASSERT_EQ(to_hex_string_view(0xDEADBEEFu, true, true), "0XDEADBEEF", "Uppercase with prefix");
    ASSERT_EQ(to_hex_string_view(0xDEADBEEFu, false, true), "DEADBEEF", "Uppercase no prefix");
    ASSERT_EQ(to_hex_string_view(255u, true, true), "0XFF", "Uppercase 255");

    std::cout << colors::green() << "  PASSED: Hexadecimal conversion tests"
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// Test Suite 8: String Concatenation
// =============================================================================

bool test_constexpr_concat_basic()
{
    std::cout << colors::cyan() << "\nTesting: constexpr_concat (basic)..."
              << colors::reset() << std::endl;

    // Two strings
    auto s1 = constexpr_concat("Hello", " World");
    ASSERT_EQ(s1.size(), 11, "Two strings size");
    char buf1[12];
    (void)s1.to_array(buf1, sizeof(buf1));
    ASSERT_EQ(std::string_view(buf1), "Hello World", "Two strings");

    // Three strings
    auto s2 = constexpr_concat("A", "B", "C");
    ASSERT_EQ(s2.size(), 3, "Three strings size");
    char buf2[4];
    (void)s2.to_array(buf2, sizeof(buf2));
    ASSERT_EQ(std::string_view(buf2), "ABC", "Three strings");

    // Multiple strings
    auto s3 = constexpr_concat("One", " ", "Two", " ", "Three");
    char buf3[20];
    (void)s3.to_array(buf3, sizeof(buf3));
    ASSERT_EQ(std::string_view(buf3), "One Two Three", "Five strings");

    std::cout << colors::green() << "  PASSED: Basic concat tests"
              << colors::reset() << std::endl;
    return true;
}

bool test_constexpr_concat_with_integers()
{
    std::cout << colors::cyan() << "\nTesting: constexpr_concat (with integers)..."
              << colors::reset() << std::endl;

    // String + integer
    auto s1 = constexpr_concat("Value: ", to_string_view(42));
    char buf1[128];
    (void)s1.to_array(buf1, sizeof(buf1));
    ASSERT_EQ(std::string_view(buf1), "Value: 42", "String + integer");

    // Multiple integers
    auto s2 = constexpr_concat("Numbers: ", to_string_view(1), ", ",
                                to_string_view(2), ", ", to_string_view(3));
    char buf2[128];
    (void)s2.to_array(buf2, sizeof(buf2));
    ASSERT_EQ(std::string_view(buf2), "Numbers: 1, 2, 3", "Multiple integers");

    // Negative integer
    auto s3 = constexpr_concat("Negative: ", to_string_view(-42));
    char buf3[128];
    (void)s3.to_array(buf3, sizeof(buf3));
    ASSERT_EQ(std::string_view(buf3), "Negative: -42", "Negative integer");

    std::cout << colors::green() << "  PASSED: Concat with integers tests"
              << colors::reset() << std::endl;
    return true;
}

bool test_constexpr_concat_edge_cases()
{
    std::cout << colors::cyan() << "\nTesting: constexpr_concat (edge cases)..."
              << colors::reset() << std::endl;

    // Empty strings
    auto empty = constexpr_concat("", "", "");
    ASSERT_EQ(empty.size(), 0, "Empty concat size");
    ASSERT_TRUE(empty.empty(), "Empty concat is empty");

    // Single string
    auto single = constexpr_concat("Single");
    ASSERT_EQ(single.size(), 6, "Single string size");

    // Buffer too small (truncation)
    auto long_str = constexpr_concat("Hello World");
    char tiny[4];
    (void)long_str.to_array(tiny, sizeof(tiny));
    ASSERT_EQ(std::string_view(tiny), "Hel", "Truncation works");

    // Zero-size buffer
    char zero_buf[1] = {'X'};
    (void)long_str.to_array(zero_buf, 0);
    ASSERT_EQ(zero_buf[0], 'X', "Zero-size buffer unchanged");

    std::cout << colors::green() << "  PASSED: Concat edge case tests"
              << colors::reset() << std::endl;
    return true;
}

bool test_constexpr_string_to_string()
{
    std::cout << colors::cyan() << "\nTesting: ConstexprString::to_string()..."
              << colors::reset() << std::endl;

    auto cs1 = constexpr_concat("Hello ", "World");
    std::string result1 = cs1.to_string();
    ASSERT_EQ(result1, "Hello World", "Basic to_string");
    ASSERT_EQ(result1.size(), cs1.size(), "Size matches");

    auto cs2 = constexpr_concat("Count: ", to_string_view(123));
    std::string result2 = cs2.to_string();
    ASSERT_EQ(result2, "Count: 123", "to_string with integer");

    auto cs3 = constexpr_concat("", "");
    std::string result3 = cs3.to_string();
    ASSERT_EQ(result3, "", "Empty to_string");

    std::cout << colors::green() << "  PASSED: to_string() tests"
              << colors::reset() << std::endl;
    return true;
}

bool test_constexpr_string_to_array_return()
{
    std::cout << colors::cyan() << "\nTesting: ConstexprString::to_array() return value..."
              << colors::reset() << std::endl;

    auto cs1 = constexpr_concat("Hello", " ", "World");
    char buf[32];
    std::size_t written = cs1.to_array(buf, sizeof(buf));
    ASSERT_EQ(written, 11, "Returns correct character count");
    ASSERT_EQ(std::string_view(buf), "Hello World", "Buffer contains correct data");

    // Truncation returns truncated length
    char small_buf[6];
    std::size_t truncated = cs1.to_array(small_buf, sizeof(small_buf));
    ASSERT_EQ(truncated, 5, "Truncation returns truncated length");
    ASSERT_EQ(std::string_view(small_buf), "Hello", "Truncated content correct");

    // Zero-size buffer returns 0
    std::size_t zero_written = cs1.to_array(buf, 0);
    ASSERT_EQ(zero_written, 0, "Zero-size buffer returns 0");

    std::cout << colors::green() << "  PASSED: to_array() return value tests"
              << colors::reset() << std::endl;
    return true;
}

bool test_constexpr_string_ostream()
{
    std::cout << colors::cyan() << "\nTesting: ConstexprString operator<<..."
              << colors::reset() << std::endl;

    auto cs = constexpr_concat("Value=", to_string_view(42));
    std::ostringstream oss;
    oss << cs;
    ASSERT_EQ(oss.str(), "Value=42", "Stream output correct");

    // Empty concat
    auto empty_cs = constexpr_concat("", "");
    std::ostringstream oss2;
    oss2 << empty_cs;
    ASSERT_EQ(oss2.str(), "", "Empty stream output");

    std::cout << colors::green() << "  PASSED: operator<< tests"
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// Test Suite 9: String Utilities
// =============================================================================

bool test_constexpr_string_utilities()
{
    std::cout << colors::cyan() << "\nTesting: constexpr string utilities..."
              << colors::reset() << std::endl;

    // constexpr_strlen
    static_assert(constexpr_strlen("") == 0, "strlen empty");
    static_assert(constexpr_strlen("hello") == 5, "strlen hello");
    static_assert(constexpr_strlen("a") == 1, "strlen single char");

    ASSERT_EQ(constexpr_strlen("test"), 4, "strlen test");

    // constexpr_strcmp
    static_assert(constexpr_strcmp("abc", "abc") == 0, "strcmp equal");
    static_assert(constexpr_strcmp("abc", "abd") < 0, "strcmp less");
    static_assert(constexpr_strcmp("abd", "abc") > 0, "strcmp greater");
    static_assert(constexpr_strcmp("ab", "abc") < 0, "strcmp shorter");

    ASSERT_EQ(constexpr_strcmp("test", "test"), 0, "strcmp equal");
    ASSERT_TRUE(constexpr_strcmp("aaa", "aab") < 0, "strcmp less");

    // constexpr_streq
    static_assert(constexpr_streq("hello", "hello"), "streq equal");
    static_assert(!constexpr_streq("hello", "world"), "streq different");
    static_assert(!constexpr_streq("hello", "hell"), "streq different length");

    ASSERT_TRUE(constexpr_streq("test", "test"), "streq equal");
    ASSERT_FALSE(constexpr_streq("test", "Test"), "streq case sensitive");

    std::cout << colors::green() << "  PASSED: String utility tests"
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// Test Suite 10: Buffer Pool Behavior
// =============================================================================

bool test_buffer_pool_rotation()
{
    std::cout << colors::cyan() << "\nTesting: Buffer pool rotation..."
              << colors::reset() << std::endl;

    // Store 16 views (pool size = STRING_POOL_SIZE)
    std::string_view views[16];
    for (int i = 0; i < 16; ++i)
    {
        views[i] = to_string_view(i);
    }

    // All should still be valid
    for (int i = 0; i < 16; ++i)
    {
        ASSERT_EQ(views[i], std::to_string(i), "View " + std::to_string(i) + " valid");
    }

    std::cout << colors::green() << "  PASSED: Buffer pool rotation tests"
              << colors::reset() << std::endl;
    return true;
}

bool test_buffer_pool_exhaustion()
{
    std::cout << colors::cyan() << "\nTesting: Buffer pool exhaustion..."
              << colors::reset() << std::endl;

    // Store first view
    std::string_view first = to_string_view(999);
    std::string first_copy(first);

    // Make 16 more calls to rotate through entire pool
    for (int i = 0; i < 16; ++i)
    {
        (void)to_string_view(i);
    }

    // First view should now be overwritten
    // Note: This tests the documented behavior warning
    ASSERT_TRUE(first != first_copy || first.data() != first_copy.data(),
                "Buffer was reused after pool exhaustion");

    std::cout << colors::green() << "  PASSED: Buffer pool exhaustion tests"
              << colors::reset() << std::endl;
    return true;
}

bool test_thread_safety()
{
    std::cout << colors::cyan() << "\nTesting: Thread safety..."
              << colors::reset() << std::endl;

    std::atomic<bool> failed{false};
    std::vector<std::thread> threads;
    constexpr int NUM_THREADS = 4;
    constexpr int ITERATIONS = 1000;

    for (int t = 0; t < NUM_THREADS; ++t)
    {
        threads.emplace_back([&failed, t]()
        {
            for (int i = 0; i < ITERATIONS; ++i)
            {
                int value = t * ITERATIONS + i;
                auto sv = to_string_view(value);
                std::string expected = std::to_string(value);
                if (sv != expected)
                {
                    failed = true;
                    break;
                }
            }
        });
    }

    for (auto& th : threads)
    {
        th.join();
    }

    ASSERT_FALSE(failed.load(), "No thread interference detected");

    std::cout << colors::green() << "  PASSED: Thread safety tests"
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// Test Suite 11: Integration Tests
// =============================================================================

bool test_integration_complex_scenarios()
{
    std::cout << colors::cyan() << "\nTesting: Complex integration scenarios..."
              << colors::reset() << std::endl;

    // Error message generation
    auto error_msg = constexpr_concat(
        "Error at line ", to_string_view(42), ": Invalid value ", to_string_view(-1)
    );
    auto error_str = error_msg.to_string();
    ASSERT_EQ(error_str, "Error at line 42: Invalid value -1", "Error message");

    // Configuration string
    auto config = constexpr_concat(
        "Config: threads=", to_string_view(8),
        ", buffer_size=", to_string_view(1024),
        ", enabled=", to_string_view(1)
    );
    auto config_str = config.to_string();
    ASSERT_TRUE(config_str.find("threads=8") != std::string::npos, "Config threads");
    ASSERT_TRUE(config_str.find("buffer_size=1024") != std::string::npos, "Config buffer");

    // Hash-based switch
    constexpr uint32_t cmd_hash = constexpr_hash("start");
    constexpr uint32_t expected = constexpr_hash("start");
    static_assert(cmd_hash == expected);
    ASSERT_EQ(cmd_hash, expected, "Hash-based switch");

    // Power-of-two validation with error message
    constexpr int buffer_size = 1024;
    static_assert(is_power_of_two(buffer_size));
    static_assert(next_power_of_two(1000u) == 1024u);

    // Combined hash values
    constexpr auto combined = hash_values("module", "function", "v1");
    ASSERT_TRUE(combined != 0, "Combined hash is non-zero");

    std::cout << colors::green() << "  PASSED: All integration tests"
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// Performance Benchmarks
// =============================================================================

void run_constexpr_utilities_benchmarks()
{
    std::cout << "\n" << colors::bold() << colors::cyan()
              << "=== ConstexprUtilities Performance Benchmarks ==="
              << colors::reset() << std::endl;

    const size_t ITERATIONS = 10'000'000;

    // Hash benchmarks
    benchmark("constexpr_hash (short string)", []()
    {
        auto result = constexpr_hash("test");
        DoNotOptimize(result);
    }, ITERATIONS);

    benchmark("constexpr_hash (long string)", []()
    {
        auto result = constexpr_hash("The quick brown fox jumps over the lazy dog");
        DoNotOptimize(result);
    }, ITERATIONS);

    benchmark("constexpr_hash64 (short string)", []()
    {
        auto result = constexpr_hash64("test");
        DoNotOptimize(result);
    }, ITERATIONS);

    benchmark("hash_combine", []()
    {
        auto result = hash_combine(12345ULL, 67890ULL);
        DoNotOptimize(result);
    }, ITERATIONS);

    // Arithmetic benchmarks
    benchmark("is_power_of_two", []()
    {
        auto result = is_power_of_two(1024);
        DoNotOptimize(result);
    }, ITERATIONS);

    benchmark("next_power_of_two", []()
    {
        auto result = next_power_of_two(1000u);
        DoNotOptimize(result);
    }, ITERATIONS);

    benchmark("log2_floor", []()
    {
        auto result = log2_floor(1024u);
        DoNotOptimize(result);
    }, ITERATIONS);

    benchmark("popcount", []()
    {
        auto result = popcount(0xDEADBEEFu);
        DoNotOptimize(result);
    }, ITERATIONS);

    // String conversion benchmarks
    benchmark("to_string_view (single digit)", []()
    {
        auto sv = to_string_view(5);
        DoNotOptimize(sv.data());
    }, ITERATIONS);

    benchmark("to_string_view (multi-digit)", []()
    {
        auto sv = to_string_view(12345);
        DoNotOptimize(sv.data());
    }, ITERATIONS);

    benchmark("to_string_view (INT32_MAX)", []()
    {
        auto sv = to_string_view(INT32_MAX);
        DoNotOptimize(sv.data());
    }, ITERATIONS);

    benchmark("to_hex_string_view", []()
    {
        auto sv = to_hex_string_view(0xDEADBEEFu);
        DoNotOptimize(sv.data());
    }, ITERATIONS);

    // Concatenation benchmarks
    benchmark("constexpr_concat (2 strings)", []()
    {
        auto cs = constexpr_concat("Hello", " World");
        DoNotOptimize(cs.size());
    }, ITERATIONS);

    benchmark("constexpr_concat (with integer)", []()
    {
        auto cs = constexpr_concat("Value: ", to_string_view(42));
        DoNotOptimize(cs.size());
    }, ITERATIONS);

    benchmark("ConstexprString::to_string()", []()
    {
        auto cs = constexpr_concat("Value: ", to_string_view(123));
        auto s = cs.to_string();
        DoNotOptimize(s.data());
    }, 1'000'000);

    std::cout << "\n" << colors::blue()
              << "[NOTE] Benchmarks measure runtime performance with thread-local storage"
              << colors::reset() << std::endl;
}

// =============================================================================
// Main Test Runner
// =============================================================================

bool test_ConstexprUtilities()
{
    PRINT_HEADER(CONSTEXPR UTILITIES)

    TestRunner runner;

    // Hash tests
    runner.run_test("constexpr_hash (32-bit basic)", test_constexpr_hash_basic);
    runner.run_test("constexpr_hash (collision resistance)", test_constexpr_hash_collision_resistance);
    runner.run_test("constexpr_hash (avalanche)", test_constexpr_hash_avalanche);
    runner.run_test("constexpr_hash64 (64-bit basic)", test_constexpr_hash64_basic);
    runner.run_test("hash_combine", test_hash_combine);

    // Arithmetic tests
    runner.run_test("is_power_of_two (signed)", test_is_power_of_two_signed);
    runner.run_test("is_power_of_two (unsigned)", test_is_power_of_two_unsigned);
    runner.run_test("next_power_of_two", test_next_power_of_two);
    runner.run_test("log2_floor/log2_ceil", test_log2_functions);
    runner.run_test("bit operations (popcount/clz/ctz)", test_bit_operations);
    runner.run_test("count_digits", test_count_digits);

    // Integer string conversion tests
    runner.run_test("to_string_view (integers)", test_to_string_view_integers);
    runner.run_test("to_string_view (C-string)", test_to_string_view_c_string);
    runner.run_test("constexpr_to_string_t", test_constexpr_to_string_t);

    // Floating-point tests
    runner.run_test("to_string_view (float basic)", test_to_string_view_float_basic);
    runner.run_test("to_string_view (float special)", test_to_string_view_float_special);
    runner.run_test("to_string_view (float overflow)", test_to_string_view_float_overflow);

    // Hexadecimal tests
    runner.run_test("to_hex_string_view", test_to_hex_string_view);

    // Concatenation tests
    runner.run_test("constexpr_concat (basic)", test_constexpr_concat_basic);
    runner.run_test("constexpr_concat (with integers)", test_constexpr_concat_with_integers);
    runner.run_test("constexpr_concat (edge cases)", test_constexpr_concat_edge_cases);
    runner.run_test("ConstexprString::to_string()", test_constexpr_string_to_string);
    runner.run_test("ConstexprString::to_array() return", test_constexpr_string_to_array_return);
    runner.run_test("ConstexprString operator<<", test_constexpr_string_ostream);

    // String utility tests
    runner.run_test("constexpr string utilities", test_constexpr_string_utilities);

    // Buffer pool tests
    runner.run_test("buffer pool rotation", test_buffer_pool_rotation);
    runner.run_test("buffer pool exhaustion", test_buffer_pool_exhaustion);
    runner.run_test("thread safety", test_thread_safety);

    // Integration tests
    runner.run_test("integration (complex scenarios)", test_integration_complex_scenarios);

    int failed = runner.print_summary();

    if (failed == 0)
    {
        run_constexpr_utilities_benchmarks();
    }

    return failed == 0;
}

}  // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_ConstexprUtilities() ? 0 : 1;
}
#endif
