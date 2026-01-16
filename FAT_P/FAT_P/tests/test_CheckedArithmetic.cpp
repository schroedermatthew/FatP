/**
 * @file test_CheckedArithmetic.cpp
 * @brief Comprehensive unit tests and benchmarks for CheckedArithmetic
 *
 * Tests cover:
 * - Critical bug fixes (multiplication, division, FP validation)
 * - Enhanced functionality (noexcept, SIMD, type-safe shifts)
 * - Edge cases (denormals, signaling NaN, unsigned overflow)
 * - FP vector operations (NaN/Inf detection, policy compliance, SIMD consistency)
 * - Performance benchmarks comparing raw vs checked operations
 * - Policy overhead comparisons (Throw vs Expected vs Saturating)
 */
/*
FATP_META:
  meta_version: 1
  component: CheckedArithmetic
  file_role: test
  path: tests/test_CheckedArithmetic.cpp
  namespace: fat_p
  summary: "Unit tests for CheckedArithmetic."
  related:
    docs_search: "CheckedArithmetic"
    headers:
      - fat_p/CheckedArithmetic.h
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

#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

#include "CheckedArithmetic.h"
#include "FatPTest.h"

namespace fat_p::testing
{

// =============================================================================
// Test Helpers
// =============================================================================

template <typename Func>
bool test_throws(const char* operation_name, Func func)
{
    try
    {
        func();
        std::cout << colors::red() << "  ERROR: " << operation_name << " did not throw as expected" << colors::reset()
                  << std::endl;
        return false;
    }
    catch (const std::exception&)
    {
        return true;
    }
}

// =============================================================================
// CRITICAL FIX TESTS
// =============================================================================


// =============================================================================
// Test Cases (in nested namespace per fat_p guidelines)
// =============================================================================

namespace checkedarithmetic
{

FATP_TEST_CASE(mul_type_mismatch)
{
    std::cout << colors::cyan() << "\n[CRITICAL FIX TEST] Testing checked_mul type mismatch fix..." << colors::reset()
              << std::endl;

    {
        auto result1 = checked_mul<ReturnExpectedPolicy>(5, 0);
        FATP_ASSERT_TRUE(result1.has_value(), "5 * 0 should compile and succeed");
        FATP_ASSERT_EQ(*result1, 0, "5 * 0 should equal 0");

        auto result2 = checked_mul<ReturnExpectedPolicy>(0, 5);
        FATP_ASSERT_TRUE(result2.has_value(), "0 * 5 should compile and succeed");
        FATP_ASSERT_EQ(*result2, 0, "0 * 5 should equal 0");

        auto result3 = checked_mul<ReturnExpectedPolicy>(0, 0);
        FATP_ASSERT_TRUE(result3.has_value(), "0 * 0 should compile and succeed");
        FATP_ASSERT_EQ(*result3, 0, "0 * 0 should equal 0");

        auto result4 = checked_mul<ReturnExpectedPolicy>(1000000, 0);
        FATP_ASSERT_TRUE(result4.has_value(), "1000000 * 0 should succeed");
        FATP_ASSERT_EQ(*result4, 0, "1000000 * 0 should equal 0");
    }

    {
        auto result = checked_mul<SaturatingPolicy>(5, 0);
        FATP_ASSERT_EQ(result, 0, "Saturating policy should also work");
    }

    std::cout << colors::green() << "[CRITICAL FIX] checked_mul type mismatch: VERIFIED" << colors::reset()
              << std::endl;
    return true;
}

FATP_TEST_CASE(div_sign_aware_saturation)
{
    std::cout << colors::cyan() << "\n[CRITICAL FIX TEST] Testing sign-aware saturation..." << colors::reset()
              << std::endl;

    {
        auto pos_result = checked_div<SaturatingPolicy>(100, 0);
        FATP_ASSERT_EQ(pos_result, std::numeric_limits<int>::max(), "Positive / 0 should saturate to max");

        auto neg_result = checked_div<SaturatingPolicy>(-100, 0);
        FATP_ASSERT_EQ(neg_result, std::numeric_limits<int>::min(), "Negative / 0 should saturate to min");

        auto zero_result = checked_div<SaturatingPolicy>(0, 0);
        FATP_ASSERT_EQ(zero_result, 0, "0 / 0 should return 0");
    }

    {
        auto pos_result = checked_div_fp<SaturatingPolicy>(5.0, 0.0);
        FATP_ASSERT_EQ(pos_result, std::numeric_limits<double>::max(), "Positive / 0.0 should saturate to max");

        auto neg_result = checked_div_fp<SaturatingPolicy>(-5.0, 0.0);
        FATP_ASSERT_EQ(neg_result,
                       std::numeric_limits<double>::lowest(),
                       "Negative / 0.0 should saturate to lowest (most negative)");

        auto small_pos = checked_div_fp<SaturatingPolicy>(1e-100, 0.0);
        FATP_ASSERT_EQ(small_pos, std::numeric_limits<double>::max(), "Small positive / 0.0 should saturate to max");

        auto small_neg = checked_div_fp<SaturatingPolicy>(-1e-100, 0.0);
        FATP_ASSERT_EQ(small_neg,
                       std::numeric_limits<double>::lowest(),
                       "Small negative / 0.0 should saturate to lowest");
    }

    std::cout << colors::green() << "[CRITICAL FIX] Sign-aware saturation: VERIFIED" << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(fp_input_validation)
{
    std::cout << colors::cyan() << "\n[CRITICAL FIX TEST] Testing FP input validation..." << colors::reset()
              << std::endl;

    double nan_val = std::numeric_limits<double>::quiet_NaN();
    double inf_val = std::numeric_limits<double>::infinity();

    {
        FATP_ASSERT_TRUE(test_throws("NaN + 1",
                                     [&]() {
                                         (void)checked_add_fp<ThrowOnErrorPolicy>(nan_val, 1.0);
                                     }),
                         "Should throw on NaN input in addition");

        auto result = checked_add_fp<ReturnExpectedPolicy>(nan_val, 1.0);
        FATP_ASSERT_TRUE(!result.has_value(), "Should fail on NaN input");
        FATP_ASSERT_EQ(result.error(), MathError::NaN, "Should return NaN error");

        auto sat_result = checked_add_fp<SaturatingPolicy>(nan_val, 1.0);
        FATP_ASSERT_TRUE(std::isnan(sat_result), "Should return NaN for saturating policy");
    }

    {
        FATP_ASSERT_TRUE(test_throws("1 - NaN",
                                     [&]() {
                                         (void)checked_sub_fp<ThrowOnErrorPolicy>(1.0, nan_val);
                                     }),
                         "Should throw on NaN input in subtraction");
    }

    {
        FATP_ASSERT_TRUE(test_throws("NaN * 2",
                                     [&]() {
                                         (void)checked_mul_fp<ThrowOnErrorPolicy>(nan_val, 2.0);
                                     }),
                         "Should throw on NaN input in multiplication");
    }

    {
        FATP_ASSERT_TRUE(test_throws("NaN / 5",
                                     [&]() {
                                         (void)checked_div_fp<ThrowOnErrorPolicy>(nan_val, 5.0);
                                     }),
                         "Should throw on NaN input in division");
    }

    {
        FATP_ASSERT_TRUE(test_throws("Inf - Inf",
                                     [&]() {
                                         (void)checked_sub_fp<ThrowOnErrorPolicy>(inf_val, inf_val);
                                     }),
                         "Should throw on Inf - Inf");

        auto result = checked_sub_fp<ReturnExpectedPolicy>(inf_val, inf_val);
        FATP_ASSERT_TRUE(!result.has_value(), "Inf - Inf should fail");
        FATP_ASSERT_EQ(result.error(), MathError::NaN, "Should return NaN error for Inf - Inf");
    }

    {
        FATP_ASSERT_TRUE(test_throws("-Inf + Inf",
                                     [&]() {
                                         (void)checked_add_fp<ThrowOnErrorPolicy>(-inf_val, inf_val);
                                     }),
                         "Should throw on -Inf + Inf");
    }

    {
        auto result1 = checked_add_fp<ThrowOnErrorPolicy>(inf_val, 1.0);
        FATP_ASSERT_TRUE(std::isinf(result1) && result1 > 0, "Inf + 1.0 should succeed");

        auto result2 = checked_add_fp<ThrowOnErrorPolicy>(inf_val, inf_val);
        FATP_ASSERT_TRUE(std::isinf(result2) && result2 > 0, "Inf + Inf should succeed");

        auto result3 = checked_mul_fp<ThrowOnErrorPolicy>(inf_val, 2.0);
        FATP_ASSERT_TRUE(std::isinf(result3) && result3 > 0, "Inf * 2.0 should succeed");

        auto result4 = checked_div_fp<ThrowOnErrorPolicy>(inf_val, 2.0);
        FATP_ASSERT_TRUE(std::isinf(result4) && result4 > 0, "Inf / 2.0 should succeed");

        auto result5 = checked_div_fp<ThrowOnErrorPolicy>(2.0, inf_val);
        FATP_ASSERT_TRUE(result5 == 0.0, "2.0 / Inf should return 0");
    }

    {
        double large = std::numeric_limits<double>::max();

        FATP_ASSERT_TRUE(test_throws("Overflow: max * 2",
                                     [&]() {
                                         (void)checked_mul_fp<ThrowOnErrorPolicy>(large, 2.0);
                                     }),
                         "Should detect overflow (finite * finite -> Inf)");

        auto result = checked_mul_fp<ReturnExpectedPolicy>(large, 2.0);
        FATP_ASSERT_TRUE(!result.has_value(), "Overflow should be detected");
        FATP_ASSERT_EQ(result.error(), MathError::Inf, "Should return Inf error for overflow");
    }

    std::cout << colors::green() << "[CRITICAL FIX] FP input validation: VERIFIED" << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// ENHANCED FUNCTIONALITY TESTS
// =============================================================================

FATP_TEST_CASE(noexcept_specifications)
{
    std::cout << colors::cyan() << "\n[ENHANCEMENT TEST] Testing noexcept specifications..." << colors::reset()
              << std::endl;

    static_assert(noexcept(checked_add<ReturnExpectedPolicy>(1, 2)), "ReturnExpectedPolicy should be noexcept");
    static_assert(noexcept(checked_add<SaturatingPolicy>(1, 2)), "SaturatingPolicy should be noexcept");
    static_assert(!noexcept(checked_add<ThrowOnErrorPolicy>(1, 2)), "ThrowOnErrorPolicy should not be noexcept");

    static_assert(noexcept(checked_add_fp<ReturnExpectedPolicy>(1.0, 2.0)),
                  "FP ReturnExpectedPolicy should be noexcept");
    static_assert(noexcept(checked_add_fp<SaturatingPolicy>(1.0, 2.0)), "FP SaturatingPolicy should be noexcept");

    std::cout << colors::green() << "[ENHANCEMENT] noexcept specifications: WORKING" << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(type_safe_shifts)
{
    std::cout << colors::cyan() << "\n[ENHANCEMENT TEST] Testing type-safe shift operations..." << colors::reset()
              << std::endl;

    {
        int value = 5;
        auto result1 = checked_left_shift<ThrowOnErrorPolicy>(value, 2);
        FATP_ASSERT_EQ(result1, 20, "5 << 2 should equal 20");

        auto result2 = checked_left_shift<ThrowOnErrorPolicy>(value, 2u);
        FATP_ASSERT_EQ(result2, 20, "Should work with unsigned shift");

        auto result3 = checked_left_shift<ThrowOnErrorPolicy>(value, static_cast<size_t>(2));
        FATP_ASSERT_EQ(result3, 20, "Should work with size_t shift");
    }

    {
        FATP_ASSERT_TRUE(test_throws("Negative left shift",
                                     []() {
                                         (void)checked_left_shift<ThrowOnErrorPolicy>(5, -1);
                                     }),
                         "Should throw on negative left shift");

        auto result = checked_left_shift<ReturnExpectedPolicy>(5, -1);
        FATP_ASSERT_TRUE(!result.has_value(), "Should fail on negative shift");
        FATP_ASSERT_EQ(result.error(), MathError::InvalidArgument, "Should return InvalidArgument");
    }

    {
        FATP_ASSERT_TRUE(test_throws("Shift >= bitwidth",
                                     []() {
                                         (void)checked_left_shift<ThrowOnErrorPolicy>(5, 32);
                                     }),
                         "Should throw on shift >= bitwidth");
    }

    std::cout << colors::green() << "[ENHANCEMENT] Type-safe shifts: WORKING" << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(static_math_mod)
{
    std::cout << colors::cyan() << "\n[ENHANCEMENT TEST] Testing static_math::mod..." << colors::reset() << std::endl;

    {
        constexpr int result1 = static_math::mod<int, 10, 3>();
        FATP_ASSERT_EQ(result1, 1, "10 % 3 should equal 1");

        constexpr int result2 = static_math::mod<int, 17, 5>();
        FATP_ASSERT_EQ(result2, 2, "17 % 5 should equal 2");

        constexpr int result3 = static_math::mod<int, -10, 3>();
        FATP_ASSERT_EQ(result3, -1, "-10 % 3 should equal -1");
    }

    std::cout << colors::green() << "[ENHANCEMENT] static_math::mod: WORKING" << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(checked_abs)
{
    std::cout << colors::cyan() << "\n[NEW FEATURE TEST] Testing checked_abs for integers..." << colors::reset()
              << std::endl;

    // Normal values
    {
        auto result = checked_abs<ThrowOnErrorPolicy>(5);
        FATP_ASSERT_EQ(result, 5, "abs(5) should equal 5");

        result = checked_abs<ThrowOnErrorPolicy>(-5);
        FATP_ASSERT_EQ(result, 5, "abs(-5) should equal 5");

        result = checked_abs<ThrowOnErrorPolicy>(0);
        FATP_ASSERT_EQ(result, 0, "abs(0) should equal 0");
    }

    // Unsigned types (should be no-op)
    {
        auto result = checked_abs<ThrowOnErrorPolicy>(5u);
        FATP_ASSERT_EQ(result, 5u, "abs(5u) should equal 5u (unsigned no-op)");

        result = checked_abs<SaturatingPolicy>(std::numeric_limits<unsigned>::max());
        FATP_ASSERT_EQ(result, std::numeric_limits<unsigned>::max(), "abs(UINT_MAX) should equal UINT_MAX");
    }

    // MIN overflow - ThrowOnErrorPolicy
    {
        FATP_ASSERT_TRUE(test_throws("abs(INT_MIN)",
                                     []() {
                                         (void)checked_abs<ThrowOnErrorPolicy>(std::numeric_limits<int>::min());
                                     }),
                         "abs(INT_MIN) should throw");

        FATP_ASSERT_TRUE(test_throws("abs(LLONG_MIN)",
                                     []() {
                                         (void)checked_abs<ThrowOnErrorPolicy>(std::numeric_limits<long long>::min());
                                     }),
                         "abs(LLONG_MIN) should throw");
    }

    // MIN overflow - ReturnExpectedPolicy
    {
        auto result = checked_abs<ReturnExpectedPolicy>(std::numeric_limits<int>::min());
        FATP_ASSERT_TRUE(!result.has_value(), "abs(INT_MIN) should fail with Expected");
        FATP_ASSERT_EQ(result.error(), MathError::Overflow, "Should return Overflow error");

        auto result64 = checked_abs<ReturnExpectedPolicy>(std::numeric_limits<int64_t>::min());
        FATP_ASSERT_TRUE(!result64.has_value(), "abs(INT64_MIN) should fail with Expected");
    }

    // MIN overflow - SaturatingPolicy
    {
        auto result = checked_abs<SaturatingPolicy>(std::numeric_limits<int>::min());
        FATP_ASSERT_EQ(result, std::numeric_limits<int>::max(), "abs(INT_MIN) should saturate to INT_MAX");

        auto result64 = checked_abs<SaturatingPolicy>(std::numeric_limits<int64_t>::min());
        FATP_ASSERT_EQ(result64, std::numeric_limits<int64_t>::max(), "abs(INT64_MIN) should saturate to INT64_MAX");
    }

    // MIN overflow - InfTolerantPolicy
    {
        auto result = checked_abs<InfTolerantPolicy>(std::numeric_limits<int>::min());
        FATP_ASSERT_EQ(result,
                       std::numeric_limits<int>::max(),
                       "abs(INT_MIN) with InfTolerant should saturate to INT_MAX");
    }

    // Various signed types
    {
        auto r8 = checked_abs<SaturatingPolicy>(static_cast<int8_t>(-128));
        FATP_ASSERT_EQ(r8, static_cast<int8_t>(127), "abs(INT8_MIN) should saturate to 127");

        auto r16 = checked_abs<SaturatingPolicy>(static_cast<int16_t>(-32768));
        FATP_ASSERT_EQ(r16, static_cast<int16_t>(32767), "abs(INT16_MIN) should saturate to 32767");
    }

    std::cout << colors::green() << "[NEW FEATURE] checked_abs: PASSED" << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(shift_inf_tolerant)
{
    std::cout << colors::cyan() << "\n[NEW FEATURE TEST] Testing InfTolerantPolicy on shifts..." << colors::reset()
              << std::endl;

    // Invalid left shift - negative amount
    {
        auto result = checked_left_shift<InfTolerantPolicy>(5, -1);
        FATP_ASSERT_EQ(result, 0, "left_shift with negative amount should return 0");

        result = checked_left_shift<InfTolerantPolicy>(100, -10);
        FATP_ASSERT_EQ(result, 0, "left_shift with large negative should return 0");
    }

    // Invalid left shift - amount >= bitwidth
    {
        auto result = checked_left_shift<InfTolerantPolicy>(5, 32);
        FATP_ASSERT_EQ(result, 0, "left_shift by 32 (int32) should return 0");

        result = checked_left_shift<InfTolerantPolicy>(5, 64);
        FATP_ASSERT_EQ(result, 0, "left_shift by 64 should return 0");

        auto result64 = checked_left_shift<InfTolerantPolicy>(5LL, 64);
        FATP_ASSERT_EQ(result64, 0LL, "left_shift int64 by 64 should return 0");
    }

    // Invalid right shift - negative amount (unsigned)
    {
        auto result = checked_right_shift<InfTolerantPolicy>(100u, -1);
        FATP_ASSERT_EQ(result, 0u, "unsigned right_shift with negative should return 0");
    }

    // Invalid right shift - signed preserves sign
    {
        auto result = checked_right_shift<InfTolerantPolicy>(-5, 64);
        FATP_ASSERT_EQ(result, -1, "signed right_shift with invalid amount should return -1");

        result = checked_right_shift<InfTolerantPolicy>(-100, 32);
        FATP_ASSERT_EQ(result, -1, "negative value right_shift by 32 should return -1");

        result = checked_right_shift<InfTolerantPolicy>(100, 32);
        FATP_ASSERT_EQ(result, 0, "positive value right_shift by 32 should return 0");
    }

    // Valid shifts should work normally
    {
        auto result = checked_left_shift<InfTolerantPolicy>(5, 2);
        FATP_ASSERT_EQ(result, 20, "5 << 2 should equal 20");

        result = checked_right_shift<InfTolerantPolicy>(20, 2);
        FATP_ASSERT_EQ(result, 5, "20 >> 2 should equal 5");
    }

    std::cout << colors::green() << "[NEW FEATURE] InfTolerantPolicy shifts: PASSED" << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(scalar_int_inf_tolerant)
{
    std::cout << colors::cyan() << "\n[NEW FEATURE TEST] Testing InfTolerantPolicy on scalar integers..."
              << colors::reset() << std::endl;

    // checked_add overflow - should saturate like SaturatingPolicy
    {
        auto result = checked_add<InfTolerantPolicy>(std::numeric_limits<int>::max(), 1);
        FATP_ASSERT_EQ(result, std::numeric_limits<int>::max(), "InfTolerant add overflow should saturate to max");

        result = checked_add<InfTolerantPolicy>(std::numeric_limits<int>::min(), -1);
        FATP_ASSERT_EQ(result, std::numeric_limits<int>::lowest(), "InfTolerant add underflow should saturate to min");

        auto uresult = checked_add<InfTolerantPolicy>(std::numeric_limits<unsigned>::max(), 1u);
        FATP_ASSERT_EQ(uresult,
                       std::numeric_limits<unsigned>::max(),
                       "InfTolerant unsigned add overflow should saturate to max");
    }

    // checked_sub overflow
    {
        auto result = checked_sub<InfTolerantPolicy>(std::numeric_limits<int>::min(), 1);
        FATP_ASSERT_EQ(result, std::numeric_limits<int>::lowest(), "InfTolerant sub underflow should saturate to min");

        result = checked_sub<InfTolerantPolicy>(std::numeric_limits<int>::max(), -1);
        FATP_ASSERT_EQ(result, std::numeric_limits<int>::max(), "InfTolerant sub overflow should saturate to max");

        auto uresult = checked_sub<InfTolerantPolicy>(0u, 1u);
        FATP_ASSERT_EQ(uresult,
                       std::numeric_limits<unsigned>::lowest(),
                       "InfTolerant unsigned sub underflow should saturate to 0");
    }

    // checked_mul overflow
    {
        auto result = checked_mul<InfTolerantPolicy>(std::numeric_limits<int>::max(), 2);
        FATP_ASSERT_EQ(result, std::numeric_limits<int>::max(), "InfTolerant mul overflow should saturate to max");

        result = checked_mul<InfTolerantPolicy>(std::numeric_limits<int>::min(), 2);
        FATP_ASSERT_EQ(result, std::numeric_limits<int>::lowest(), "InfTolerant mul underflow should saturate to min");

        result = checked_mul<InfTolerantPolicy>(std::numeric_limits<int>::max(), -2);
        FATP_ASSERT_EQ(result,
                       std::numeric_limits<int>::lowest(),
                       "InfTolerant mul negative overflow should saturate to min");
    }

    // checked_div by zero
    {
        auto result = checked_div<InfTolerantPolicy>(100, 0);
        FATP_ASSERT_EQ(result,
                       std::numeric_limits<int>::max(),
                       "InfTolerant div by zero (positive) should saturate to max");

        result = checked_div<InfTolerantPolicy>(-100, 0);
        FATP_ASSERT_EQ(result,
                       std::numeric_limits<int>::lowest(),
                       "InfTolerant div by zero (negative) should saturate to min");

        result = checked_div<InfTolerantPolicy>(0, 0);
        FATP_ASSERT_EQ(result, 0, "InfTolerant 0/0 should return 0");
    }

    // checked_div MIN/-1 overflow
    {
        auto result = checked_div<InfTolerantPolicy>(std::numeric_limits<int>::min(), -1);
        FATP_ASSERT_EQ(result, std::numeric_limits<int>::max(), "InfTolerant MIN/-1 should saturate to max");
    }

    // checked_mod by zero
    {
        auto result = checked_mod<InfTolerantPolicy>(100, 0);
        FATP_ASSERT_EQ(result, 0, "InfTolerant mod by zero should return 0");
    }

    // checked_mod MIN%-1 overflow
    {
        auto result = checked_mod<InfTolerantPolicy>(std::numeric_limits<int>::min(), -1);
        FATP_ASSERT_EQ(result, 0, "InfTolerant MIN%-1 should return 0");
    }

    // Normal operations should work correctly
    {
        FATP_ASSERT_EQ(checked_add<InfTolerantPolicy>(5, 3), 8, "5+3 should equal 8");
        FATP_ASSERT_EQ(checked_sub<InfTolerantPolicy>(10, 3), 7, "10-3 should equal 7");
        FATP_ASSERT_EQ(checked_mul<InfTolerantPolicy>(6, 7), 42, "6*7 should equal 42");
        FATP_ASSERT_EQ(checked_div<InfTolerantPolicy>(20, 4), 5, "20/4 should equal 5");
        FATP_ASSERT_EQ(checked_mod<InfTolerantPolicy>(17, 5), 2, "17%5 should equal 2");
    }

    std::cout << colors::green() << "[NEW FEATURE] InfTolerantPolicy scalar integers: PASSED" << colors::reset()
              << std::endl;
    return true;
}

// =============================================================================
// INFTOLERANTPOLICY FLOATING-POINT TESTS (BUG FIX VERIFICATION)
// =============================================================================

FATP_TEST_CASE(inf_tolerant_fp_nan_handling)
{
    std::cout << colors::cyan() << "\n[BUG FIX TEST] Testing InfTolerantPolicy NaN handling (noexcept compliance)..."
              << colors::reset() << std::endl;

    double nan_val = std::numeric_limits<double>::quiet_NaN();
    double pos_inf = std::numeric_limits<double>::infinity();
    double neg_inf = -std::numeric_limits<double>::infinity();

    // InfTolerantPolicy is declared noexcept - it must NOT throw
    // Instead of throwing on NaN inputs, it should return NaN

    // NaN inputs - should return NaN, not throw
    {
        auto r1 = checked_add_fp<InfTolerantPolicy>(nan_val, 1.0);
        FATP_ASSERT_TRUE(std::isnan(r1), "InfTolerantPolicy should return NaN for NaN + 1.0");

        auto r2 = checked_sub_fp<InfTolerantPolicy>(1.0, nan_val);
        FATP_ASSERT_TRUE(std::isnan(r2), "InfTolerantPolicy should return NaN for 1.0 - NaN");

        auto r3 = checked_mul_fp<InfTolerantPolicy>(nan_val, 2.0);
        FATP_ASSERT_TRUE(std::isnan(r3), "InfTolerantPolicy should return NaN for NaN * 2.0");

        auto r4 = checked_div_fp<InfTolerantPolicy>(nan_val, 2.0);
        FATP_ASSERT_TRUE(std::isnan(r4), "InfTolerantPolicy should return NaN for NaN / 2.0");

        auto r5 = checked_div_fp<InfTolerantPolicy>(2.0, nan_val);
        FATP_ASSERT_TRUE(std::isnan(r5), "InfTolerantPolicy should return NaN for 2.0 / NaN");
    }

    // Inf-Inf undefined cases - should return NaN, not throw
    {
        auto r1 = checked_add_fp<InfTolerantPolicy>(pos_inf, neg_inf);
        FATP_ASSERT_TRUE(std::isnan(r1), "InfTolerantPolicy should return NaN for Inf + (-Inf)");

        auto r2 = checked_sub_fp<InfTolerantPolicy>(pos_inf, pos_inf);
        FATP_ASSERT_TRUE(std::isnan(r2), "InfTolerantPolicy should return NaN for Inf - Inf");

        auto r3 = checked_sub_fp<InfTolerantPolicy>(neg_inf, neg_inf);
        FATP_ASSERT_TRUE(std::isnan(r3), "InfTolerantPolicy should return NaN for -Inf - (-Inf)");
    }

    // Overflow to Inf - should return Inf (that's the whole point of InfTolerant)
    {
        double big = std::numeric_limits<double>::max();

        auto r1 = checked_add_fp<InfTolerantPolicy>(big, big);
        FATP_ASSERT_TRUE(std::isinf(r1) && r1 > 0, "InfTolerantPolicy should allow +Inf from overflow");

        auto r2 = checked_mul_fp<InfTolerantPolicy>(big, 2.0);
        FATP_ASSERT_TRUE(std::isinf(r2) && r2 > 0, "InfTolerantPolicy should allow +Inf from mul overflow");

        auto r3 = checked_mul_fp<InfTolerantPolicy>(-big, 2.0);
        FATP_ASSERT_TRUE(std::isinf(r3) && r3 < 0, "InfTolerantPolicy should allow -Inf from mul overflow");
    }

    // Division by zero - should return Inf (or NaN for 0/0)
    {
        auto r1 = checked_div_fp<InfTolerantPolicy>(1.0, 0.0);
        FATP_ASSERT_TRUE(std::isinf(r1) && r1 > 0, "InfTolerantPolicy should return +Inf for 1/0");

        auto r2 = checked_div_fp<InfTolerantPolicy>(-1.0, 0.0);
        FATP_ASSERT_TRUE(std::isinf(r2) && r2 < 0, "InfTolerantPolicy should return -Inf for -1/0");

        auto r3 = checked_div_fp<InfTolerantPolicy>(0.0, 0.0);
        FATP_ASSERT_TRUE(std::isnan(r3), "InfTolerantPolicy should return NaN for 0/0");
    }

    // Valid Inf operations - should work normally
    {
        auto r1 = checked_add_fp<InfTolerantPolicy>(pos_inf, 1.0);
        FATP_ASSERT_TRUE(std::isinf(r1) && r1 > 0, "Inf + 1.0 should return Inf");

        auto r2 = checked_add_fp<InfTolerantPolicy>(pos_inf, pos_inf);
        FATP_ASSERT_TRUE(std::isinf(r2) && r2 > 0, "Inf + Inf should return Inf");

        auto r3 = checked_mul_fp<InfTolerantPolicy>(pos_inf, 2.0);
        FATP_ASSERT_TRUE(std::isinf(r3) && r3 > 0, "Inf * 2.0 should return Inf");

        auto r4 = checked_div_fp<InfTolerantPolicy>(pos_inf, 2.0);
        FATP_ASSERT_TRUE(std::isinf(r4) && r4 > 0, "Inf / 2.0 should return Inf");

        auto r5 = checked_div_fp<InfTolerantPolicy>(2.0, pos_inf);
        FATP_ASSERT_TRUE(r5 == 0.0, "2.0 / Inf should return 0");
    }

    // checked_cast with InfTolerantPolicy
    {
        auto r1 = checked_cast<int, InfTolerantPolicy>(nan_val);
        FATP_ASSERT_EQ(r1, 0, "InfTolerantPolicy checked_cast of NaN to int should return 0");

        auto r2 = checked_cast<int, InfTolerantPolicy>(pos_inf);
        FATP_ASSERT_EQ(r2,
                       std::numeric_limits<int>::max(),
                       "InfTolerantPolicy checked_cast of +Inf to int should saturate to max");

        auto r3 = checked_cast<int, InfTolerantPolicy>(neg_inf);
        FATP_ASSERT_EQ(r3,
                       std::numeric_limits<int>::lowest(),
                       "InfTolerantPolicy checked_cast of -Inf to int should saturate to min");
    }

    std::cout << colors::green() << "[BUG FIX] InfTolerantPolicy NaN handling: PASSED" << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(scalar_fp_inf_tolerant)
{
    std::cout << colors::cyan() << "\n[NEW FEATURE TEST] Testing InfTolerantPolicy on scalar FP..." << colors::reset()
              << std::endl;

    double nan_val = std::numeric_limits<double>::quiet_NaN();
    double inf_val = std::numeric_limits<double>::infinity();
    double big = std::numeric_limits<double>::max();

    // NaN input - should return NaN, not throw
    {
        auto r1 = checked_add_fp<InfTolerantPolicy>(nan_val, 1.0);
        FATP_ASSERT_TRUE(std::isnan(r1), "InfTolerant add(NaN, 1) should return NaN");

        auto r2 = checked_sub_fp<InfTolerantPolicy>(1.0, nan_val);
        FATP_ASSERT_TRUE(std::isnan(r2), "InfTolerant sub(1, NaN) should return NaN");

        auto r3 = checked_mul_fp<InfTolerantPolicy>(nan_val, 2.0);
        FATP_ASSERT_TRUE(std::isnan(r3), "InfTolerant mul(NaN, 2) should return NaN");

        auto r4 = checked_div_fp<InfTolerantPolicy>(nan_val, 2.0);
        FATP_ASSERT_TRUE(std::isnan(r4), "InfTolerant div(NaN, 2) should return NaN");

        auto r5 = checked_mod_fp<InfTolerantPolicy>(nan_val, 2.0);
        FATP_ASSERT_TRUE(std::isnan(r5), "InfTolerant mod(NaN, 2) should return NaN");
    }

    // Inf-Inf undefined cases - should return NaN, not throw
    {
        auto r1 = checked_add_fp<InfTolerantPolicy>(inf_val, -inf_val);
        FATP_ASSERT_TRUE(std::isnan(r1), "InfTolerant add(Inf, -Inf) should return NaN");

        auto r2 = checked_sub_fp<InfTolerantPolicy>(inf_val, inf_val);
        FATP_ASSERT_TRUE(std::isnan(r2), "InfTolerant sub(Inf, Inf) should return NaN");
    }

    // Overflow to Inf - should return Inf (tolerate it)
    {
        auto r1 = checked_add_fp<InfTolerantPolicy>(big, big);
        FATP_ASSERT_TRUE(std::isinf(r1) && r1 > 0, "InfTolerant add overflow should return +Inf");

        auto r2 = checked_mul_fp<InfTolerantPolicy>(big, 2.0);
        FATP_ASSERT_TRUE(std::isinf(r2) && r2 > 0, "InfTolerant mul overflow should return +Inf");

        auto r3 = checked_mul_fp<InfTolerantPolicy>(-big, 2.0);
        FATP_ASSERT_TRUE(std::isinf(r3) && r3 < 0, "InfTolerant mul underflow should return -Inf");
    }

    // Division by zero - should return Inf
    {
        auto r1 = checked_div_fp<InfTolerantPolicy>(1.0, 0.0);
        FATP_ASSERT_TRUE(std::isinf(r1) && r1 > 0, "InfTolerant div(1, 0) should return +Inf");

        auto r2 = checked_div_fp<InfTolerantPolicy>(-1.0, 0.0);
        FATP_ASSERT_TRUE(std::isinf(r2) && r2 < 0, "InfTolerant div(-1, 0) should return -Inf");

        auto r3 = checked_div_fp<InfTolerantPolicy>(0.0, 0.0);
        FATP_ASSERT_TRUE(std::isnan(r3), "InfTolerant div(0, 0) should return NaN");
    }

    // checked_cast with InfTolerantPolicy
    {
        auto r1 = checked_cast<int, InfTolerantPolicy>(nan_val);
        FATP_ASSERT_EQ(r1, 0, "InfTolerant cast NaN to int should return 0");

        auto r2 = checked_cast<int, InfTolerantPolicy>(inf_val);
        FATP_ASSERT_EQ(r2, std::numeric_limits<int>::max(), "InfTolerant cast +Inf to int should saturate to max");

        auto r3 = checked_cast<int, InfTolerantPolicy>(-inf_val);
        FATP_ASSERT_EQ(r3, std::numeric_limits<int>::lowest(), "InfTolerant cast -Inf to int should saturate to min");

        auto r4 = checked_cast<double, InfTolerantPolicy>(nan_val);
        FATP_ASSERT_TRUE(std::isnan(r4), "InfTolerant cast NaN to double should return NaN");
    }

    // Normal operations should work correctly
    {
        auto r1 = checked_add_fp<InfTolerantPolicy>(1.5, 2.5);
        FATP_ASSERT_TRUE(std::abs(r1 - 4.0) < 1e-10, "1.5 + 2.5 should equal 4.0");

        auto r2 = checked_mul_fp<InfTolerantPolicy>(3.0, 4.0);
        FATP_ASSERT_TRUE(std::abs(r2 - 12.0) < 1e-10, "3.0 * 4.0 should equal 12.0");
    }

    std::cout << colors::green() << "[NEW FEATURE] InfTolerantPolicy scalar FP: PASSED" << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// EXPANDED EDGE CASE TESTS
// =============================================================================

FATP_TEST_CASE(fp_denormals)
{
    std::cout << colors::cyan() << "\n[EDGE CASE TEST] Testing denormal handling..." << colors::reset() << std::endl;

    double denorm = std::numeric_limits<double>::denorm_min();

    {
        auto result = checked_add_fp<ThrowOnErrorPolicy>(denorm, denorm);
        FATP_ASSERT_TRUE(std::abs(result - 2.0 * denorm) < 1e-320 || result == 2.0 * denorm,
                         "Should handle denormal addition");
    }

    {
        auto result = checked_mul_fp<ThrowOnErrorPolicy>(denorm, 2.0);
        FATP_ASSERT_TRUE(result > 0 && std::isfinite(result), "Should handle denormal multiplication");
    }

    {
        auto result = checked_div_fp<ThrowOnErrorPolicy>(denorm, 2.0);
        FATP_ASSERT_TRUE(result >= 0 && std::isfinite(result),
                         "Should handle division producing denormal or underflow to zero");
    }

    std::cout << colors::green() << "[EDGE CASE] Denormal handling: PASSED" << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(unsigned_overflow)
{
    std::cout << colors::cyan() << "\n[EDGE CASE TEST] Testing unsigned overflow patterns..." << colors::reset()
              << std::endl;

    unsigned int max_uint = std::numeric_limits<unsigned int>::max();

    {
        auto result = checked_add<ReturnExpectedPolicy>(max_uint, 1u);
        FATP_ASSERT_TRUE(!result.has_value(), "unsigned max + 1 should overflow");

        auto sat_result = checked_add<SaturatingPolicy>(max_uint, 1u);
        FATP_ASSERT_EQ(sat_result, max_uint, "Should saturate to max");
    }

    {
        auto result = checked_sub<ReturnExpectedPolicy>(0u, 1u);
        FATP_ASSERT_TRUE(!result.has_value(), "0u - 1u should underflow");

        auto sat_result = checked_sub<SaturatingPolicy>(0u, 1u);
        FATP_ASSERT_EQ(sat_result, 0u, "Should saturate to 0");
    }

    {
        auto result = checked_mul<ReturnExpectedPolicy>(max_uint, 2u);
        FATP_ASSERT_TRUE(!result.has_value(), "unsigned max * 2 should overflow");
    }

    std::cout << colors::green() << "[EDGE CASE] Unsigned overflow: PASSED" << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(mixed_sign_operations)
{
    std::cout << colors::cyan() << "\n[EDGE CASE TEST] Testing mixed-sign operations..." << colors::reset()
              << std::endl;

    {
        auto result = checked_mul<SaturatingPolicy>(-100, 100000000);
        FATP_ASSERT_EQ(result, std::numeric_limits<int>::min(), "Negative result should saturate to min");

        result = checked_mul<SaturatingPolicy>(100, -100000000);
        FATP_ASSERT_EQ(result, std::numeric_limits<int>::min(), "Mixed-sign overflow should saturate correctly");
    }

    {
        int near_max = std::numeric_limits<int>::max() - 10;
        auto result = checked_add<ThrowOnErrorPolicy>(near_max, -5);
        FATP_ASSERT_EQ(result, near_max - 5, "Should handle mixed-sign addition near max");

        int near_min = std::numeric_limits<int>::min() + 10;
        result = checked_add<ThrowOnErrorPolicy>(near_min, 5);
        FATP_ASSERT_EQ(result, near_min + 5, "Should handle mixed-sign addition near min");
    }

    std::cout << colors::green() << "[EDGE CASE] Mixed-sign operations: PASSED" << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// SIMD VALIDATION TESTS
// =============================================================================

FATP_TEST_CASE(simd_int32_correctness)
{
    std::cout << colors::cyan() << "\n[SIMD TEST] Testing int32 vector operations..." << colors::reset() << std::endl;

    std::vector<size_t> sizes = {1, 7, 8, 15, 16, 100, 1000};

    for (size_t size : sizes)
    {
        std::vector<int32_t> vec_a(size);
        std::vector<int32_t> vec_b(size);

        for (size_t i = 0; i < size; ++i)
        {
            vec_a[i] = static_cast<int32_t>(i);
            vec_b[i] = static_cast<int32_t>(i * 2);
        }

        auto result = checked_add_vec<ThrowOnErrorPolicy>(vec_a, vec_b);
        FATP_ASSERT_EQ(result.size(), size, "Result size should match input");

        for (size_t i = 0; i < size; ++i)
        {
            int32_t expected = vec_a[i] + vec_b[i];
            FATP_ASSERT_EQ(result[i], expected, "SIMD result should match scalar");
        }
    }

#ifdef __AVX2__
    std::cout << colors::green() << "[SIMD] int32 operations (AVX2): PASSED" << colors::reset() << std::endl;
#else
    std::cout << colors::yellow() << "[SIMD] int32 operations (scalar fallback): PASSED" << colors::reset()
              << std::endl;
#endif

    return true;
}

FATP_TEST_CASE(simd_overflow_detection)
{
    std::cout << colors::cyan() << "\n[SIMD TEST] Testing SIMD overflow detection..." << colors::reset() << std::endl;

    std::vector<int32_t> vec_a = {1, 2, std::numeric_limits<int32_t>::max(), 4, 5};
    std::vector<int32_t> vec_b = {1, 2, 1, 4, 5};

    FATP_ASSERT_TRUE(test_throws("Vector addition overflow",
                                 [&]() {
                                     (void)checked_add_vec<ThrowOnErrorPolicy>(vec_a, vec_b);
                                 }),
                     "Should detect overflow in vector operation");

    auto result = checked_add_vec<SaturatingPolicy>(vec_a, vec_b);
    FATP_ASSERT_EQ(result[0], 2, "Non-overflow elements should be correct");
    FATP_ASSERT_EQ(result[2], std::numeric_limits<int32_t>::max(), "Overflow element should saturate");

    std::cout << colors::green() << "[SIMD] Overflow detection: PASSED" << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// FP VECTOR OPERATIONS TESTS
// =============================================================================

FATP_TEST_CASE(fp_vec_sub_nan_detection)
{
    std::cout << colors::cyan() << "\n[FP VEC SUB] Testing NaN detection..." << colors::reset() << std::endl;

    std::vector<double> vec_a = {1.0, 2.0, std::numeric_limits<double>::quiet_NaN(), 4.0};
    std::vector<double> vec_b = {1.0, 2.0, 3.0, 4.0};

    FATP_ASSERT_TRUE(test_throws("Vector sub with NaN",
                                 [&]() {
                                     (void)checked_sub_vec_fp<ThrowOnErrorPolicy>(vec_a, vec_b);
                                 }),
                     "Should detect NaN in subtraction");

    auto result = checked_sub_vec_fp<ReturnExpectedPolicy>(vec_a, vec_b);
    FATP_ASSERT_TRUE(!result.has_value(), "ReturnExpected should return error for NaN");
    FATP_ASSERT_EQ(result.error(), MathError::NaN, "Error should be MathError::NaN");

    std::cout << colors::green() << "[FP VEC SUB] NaN detection: PASSED" << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(fp_vec_sub_inf_overflow)
{
    std::cout << colors::cyan() << "\n[FP VEC SUB] Testing Inf overflow detection..." << colors::reset() << std::endl;

    std::vector<double> vec_a = {1.0, std::numeric_limits<double>::max(), 3.0};
    std::vector<double> vec_b = {1.0, -std::numeric_limits<double>::max(), 3.0};

    FATP_ASSERT_TRUE(test_throws("Vector sub overflow to Inf",
                                 [&]() {
                                     (void)checked_sub_vec_fp<ThrowOnErrorPolicy>(vec_a, vec_b);
                                 }),
                     "Should detect Inf overflow from finite inputs");

    auto result = checked_sub_vec_fp<ReturnExpectedPolicy>(vec_a, vec_b);
    FATP_ASSERT_TRUE(!result.has_value(), "ReturnExpected should return error for Inf overflow");
    FATP_ASSERT_EQ(result.error(), MathError::Inf, "Error should be MathError::Inf");

    auto saturated = checked_sub_vec_fp<SaturatingPolicy>(vec_a, vec_b);
    FATP_ASSERT_EQ(saturated[0], 0.0, "Non-overflow elements should compute correctly");
    FATP_ASSERT_EQ(saturated[1], std::numeric_limits<double>::max(), "Overflow should saturate");

    std::cout << colors::green() << "[FP VEC SUB] Inf overflow: PASSED" << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(fp_vec_sub_inf_tolerant)
{
    std::cout << colors::cyan() << "\n[FP VEC SUB] Testing InfTolerant policy..." << colors::reset() << std::endl;

    std::vector<double> vec_a = {1.0, std::numeric_limits<double>::max(), 3.0};
    std::vector<double> vec_b = {1.0, -std::numeric_limits<double>::max(), 3.0};

    auto result = checked_sub_vec_fp<InfTolerantPolicy>(vec_a, vec_b);
    FATP_ASSERT_EQ(result[0], 0.0, "Normal elements should compute correctly");
    FATP_ASSERT_TRUE(std::isinf(result[1]), "InfTolerant should allow Inf results");
    FATP_ASSERT_EQ(result[2], 0.0, "Normal elements should compute correctly");

    std::cout << colors::green() << "[FP VEC SUB] InfTolerant: PASSED" << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(fp_vec_mul_nan_detection)
{
    std::cout << colors::cyan() << "\n[FP VEC MUL] Testing NaN detection..." << colors::reset() << std::endl;

    std::vector<double> vec_a = {1.0, 2.0, 0.0, 4.0};
    std::vector<double> vec_b = {1.0, 2.0, std::numeric_limits<double>::infinity(), 4.0};

    FATP_ASSERT_TRUE(test_throws("Vector mul with 0 * Inf -> NaN",
                                 [&]() {
                                     (void)checked_mul_vec_fp<ThrowOnErrorPolicy>(vec_a, vec_b);
                                 }),
                     "Should detect NaN from 0 * Inf");

    auto result = checked_mul_vec_fp<ReturnExpectedPolicy>(vec_a, vec_b);
    FATP_ASSERT_TRUE(!result.has_value(), "ReturnExpected should return error for NaN");

    std::cout << colors::green() << "[FP VEC MUL] NaN detection: PASSED" << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(fp_vec_mul_inf_overflow)
{
    std::cout << colors::cyan() << "\n[FP VEC MUL] Testing Inf overflow detection..." << colors::reset() << std::endl;

    const double large_val = std::numeric_limits<double>::max() / 2.0;
    std::vector<double> vec_a = {1.0, large_val, 3.0};
    std::vector<double> vec_b = {1.0, 3.0, 3.0};

    FATP_ASSERT_TRUE(test_throws("Vector mul overflow to Inf",
                                 [&]() {
                                     (void)checked_mul_vec_fp<ThrowOnErrorPolicy>(vec_a, vec_b);
                                 }),
                     "Should detect Inf overflow from finite multiplication");

    auto result = checked_mul_vec_fp<ReturnExpectedPolicy>(vec_a, vec_b);
    FATP_ASSERT_TRUE(!result.has_value(), "ReturnExpected should return error for Inf overflow");
    FATP_ASSERT_EQ(result.error(), MathError::Inf, "Error should be MathError::Inf");

    auto saturated = checked_mul_vec_fp<SaturatingPolicy>(vec_a, vec_b);
    FATP_ASSERT_EQ(saturated[0], 1.0, "Non-overflow elements should compute correctly");
    FATP_ASSERT_EQ(saturated[1], std::numeric_limits<double>::max(), "Overflow should saturate");

    std::cout << colors::green() << "[FP VEC MUL] Inf overflow: PASSED" << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(fp_vec_mul_mixed_overflow)
{
    std::cout << colors::cyan() << "\n[FP VEC MUL] Testing mixed overflow in vector..." << colors::reset() << std::endl;

    const double large_val = std::numeric_limits<double>::max() / 2.0;
    std::vector<double> vec_a = {1.0, 2.0, large_val, 4.0, 5.0};
    std::vector<double> vec_b = {2.0, 3.0, 10.0, 6.0, 7.0};

    FATP_ASSERT_TRUE(test_throws("Mixed overflow detection",
                                 [&]() {
                                     (void)checked_mul_vec_fp<ThrowOnErrorPolicy>(vec_a, vec_b);
                                 }),
                     "Should detect overflow in middle of vector");

    auto saturated = checked_mul_vec_fp<SaturatingPolicy>(vec_a, vec_b);
    FATP_ASSERT_EQ(saturated[0], 2.0, "Element 0 should be correct");
    FATP_ASSERT_EQ(saturated[1], 6.0, "Element 1 should be correct");
    FATP_ASSERT_EQ(saturated[2], std::numeric_limits<double>::max(), "Element 2 should saturate");
    FATP_ASSERT_EQ(saturated[3], 24.0, "Element 3 should be correct");
    FATP_ASSERT_EQ(saturated[4], 35.0, "Element 4 should be correct");

    std::cout << colors::green() << "[FP VEC MUL] Mixed overflow: PASSED" << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(fp_vec_mul_inf_tolerant)
{
    std::cout << colors::cyan() << "\n[FP VEC MUL] Testing InfTolerant policy..." << colors::reset() << std::endl;

    const double large_val = std::numeric_limits<double>::max() / 2.0;
    std::vector<double> vec_a = {1.0, large_val};
    std::vector<double> vec_b = {2.0, 10.0};

    auto result = checked_mul_vec_fp<InfTolerantPolicy>(vec_a, vec_b);
    FATP_ASSERT_EQ(result[0], 2.0, "Normal element should compute correctly");
    FATP_ASSERT_TRUE(std::isinf(result[1]), "InfTolerant should allow Inf results");

    std::cout << colors::green() << "[FP VEC MUL] InfTolerant: PASSED" << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(fp_vec_div_nan_detection)
{
    std::cout << colors::cyan() << "\n[FP VEC DIV] Testing NaN detection..." << colors::reset() << std::endl;

    std::vector<double> vec_a = {1.0, std::numeric_limits<double>::infinity(), 3.0};
    std::vector<double> vec_b = {2.0, std::numeric_limits<double>::infinity(), 3.0};

    FATP_ASSERT_TRUE(test_throws("Vector div with Inf / Inf -> NaN",
                                 [&]() {
                                     (void)checked_div_vec_fp<ThrowOnErrorPolicy>(vec_a, vec_b);
                                 }),
                     "Should detect NaN from Inf / Inf");

    auto result = checked_div_vec_fp<ReturnExpectedPolicy>(vec_a, vec_b);
    FATP_ASSERT_TRUE(!result.has_value(), "ReturnExpected should return error for NaN");

    std::cout << colors::green() << "[FP VEC DIV] NaN detection: PASSED" << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(fp_vec_div_inf_overflow)
{
    std::cout << colors::cyan() << "\n[FP VEC DIV] Testing Inf overflow detection..." << colors::reset() << std::endl;

    const double small_val = std::numeric_limits<double>::min();
    std::vector<double> vec_a = {1.0, std::numeric_limits<double>::max(), 3.0};
    std::vector<double> vec_b = {2.0, small_val, 3.0};

    FATP_ASSERT_TRUE(test_throws("Vector div overflow to Inf",
                                 [&]() {
                                     (void)checked_div_vec_fp<ThrowOnErrorPolicy>(vec_a, vec_b);
                                 }),
                     "Should detect Inf overflow from division");

    auto result = checked_div_vec_fp<ReturnExpectedPolicy>(vec_a, vec_b);
    FATP_ASSERT_TRUE(!result.has_value(), "ReturnExpected should return error for Inf overflow");
    FATP_ASSERT_EQ(result.error(), MathError::Inf, "Error should be MathError::Inf");

    auto saturated = checked_div_vec_fp<SaturatingPolicy>(vec_a, vec_b);
    FATP_ASSERT_EQ(saturated[0], 0.5, "Non-overflow elements should compute correctly");
    FATP_ASSERT_EQ(saturated[1], std::numeric_limits<double>::max(), "Overflow should saturate");

    std::cout << colors::green() << "[FP VEC DIV] Inf overflow: PASSED" << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(fp_vec_div_by_zero)
{
    std::cout << colors::cyan() << "\n[FP VEC DIV] Testing division by zero..." << colors::reset() << std::endl;

    std::vector<double> vec_a = {1.0, 2.0, 3.0};
    std::vector<double> vec_b = {2.0, 0.0, 3.0};

    FATP_ASSERT_TRUE(test_throws("Vector div by zero",
                                 [&]() {
                                     (void)checked_div_vec_fp<ThrowOnErrorPolicy>(vec_a, vec_b);
                                 }),
                     "Should detect division by zero");

    auto result = checked_div_vec_fp<ReturnExpectedPolicy>(vec_a, vec_b);
    FATP_ASSERT_TRUE(!result.has_value(), "ReturnExpected should return error for div by zero");
    FATP_ASSERT_EQ(result.error(), MathError::DivByZero, "Error should be DivByZero");

    std::cout << colors::green() << "[FP VEC DIV] Division by zero: PASSED" << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(fp_vec_div_inf_tolerant)
{
    std::cout << colors::cyan() << "\n[FP VEC DIV] Testing InfTolerant policy..." << colors::reset() << std::endl;

    const double small_val = std::numeric_limits<double>::min();
    std::vector<double> vec_a = {1.0, std::numeric_limits<double>::max()};
    std::vector<double> vec_b = {2.0, small_val};

    auto result = checked_div_vec_fp<InfTolerantPolicy>(vec_a, vec_b);
    FATP_ASSERT_EQ(result[0], 0.5, "Normal element should compute correctly");
    FATP_ASSERT_TRUE(std::isinf(result[1]), "InfTolerant should allow Inf results");

    std::vector<double> vec_c = {1.0, 2.0};
    std::vector<double> vec_d = {2.0, 0.0};
    auto result2 = checked_div_vec_fp<InfTolerantPolicy>(vec_c, vec_d);
    FATP_ASSERT_TRUE(std::isinf(result2[1]), "InfTolerant should produce Inf for div by zero");

    std::cout << colors::green() << "[FP VEC DIV] InfTolerant: PASSED" << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(fp_vec_simd_consistency)
{
    std::cout << colors::cyan() << "\n[FP VEC SIMD] Testing SIMD path consistency..." << colors::reset() << std::endl;

    std::vector<size_t> sizes = {1, 3, 4, 5, 8, 15, 16, 100};

    for (size_t size : sizes)
    {
        std::vector<double> vec_a(size);
        std::vector<double> vec_b(size);

        for (size_t i = 0; i < size; ++i)
        {
            vec_a[i] = static_cast<double>(i + 1) * 1.5;
            vec_b[i] = static_cast<double>(i + 1) * 0.5;
        }

        auto sub_result = checked_sub_vec_fp<ThrowOnErrorPolicy>(vec_a, vec_b);
        FATP_ASSERT_EQ(sub_result.size(), size, "Sub result size should match");

        auto mul_result = checked_mul_vec_fp<ThrowOnErrorPolicy>(vec_a, vec_b);
        FATP_ASSERT_EQ(mul_result.size(), size, "Mul result size should match");

        auto div_result = checked_div_vec_fp<ThrowOnErrorPolicy>(vec_a, vec_b);
        FATP_ASSERT_EQ(div_result.size(), size, "Div result size should match");

        for (size_t i = 0; i < size; ++i)
        {
            double expected_sub = vec_a[i] - vec_b[i];
            double expected_mul = vec_a[i] * vec_b[i];
            double expected_div = vec_a[i] / vec_b[i];

            FATP_ASSERT_EQ(sub_result[i], expected_sub, "SIMD sub should match scalar");
            FATP_ASSERT_EQ(mul_result[i], expected_mul, "SIMD mul should match scalar");
            FATP_ASSERT_EQ(div_result[i], expected_div, "SIMD div should match scalar");
        }
    }

#ifdef __AVX2__
    std::cout << colors::green() << "[FP VEC SIMD] Consistency (AVX2): PASSED" << colors::reset() << std::endl;
#else
    std::cout << colors::yellow() << "[FP VEC SIMD] Consistency (scalar fallback): PASSED" << colors::reset()
              << std::endl;
#endif

    return true;
}

FATP_TEST_CASE(fp_vec_boundary_detection)
{
    std::cout << colors::cyan() << "\n[FP VEC EDGE] Testing boundary overflow detection..." << colors::reset()
              << std::endl;

    const double max_val = std::numeric_limits<double>::max();
    const double min_val = std::numeric_limits<double>::min();

    {
        std::vector<double> vec_a = {max_val, max_val};
        std::vector<double> vec_b = {max_val, 2.0};

        FATP_ASSERT_TRUE(test_throws("Mul overflow at DBL_MAX",
                                     [&]() {
                                         (void)checked_mul_vec_fp<ThrowOnErrorPolicy>(vec_a, vec_b);
                                     }),
                         "Should detect DBL_MAX overflow");
    }

    {
        std::vector<double> vec_a = {max_val, 1.0};
        std::vector<double> vec_b = {min_val, 1.0};

        FATP_ASSERT_TRUE(test_throws("Div overflow DBL_MAX/DBL_MIN",
                                     [&]() {
                                         (void)checked_div_vec_fp<ThrowOnErrorPolicy>(vec_a, vec_b);
                                     }),
                         "Should detect division overflow");
    }

    {
        std::vector<double> vec_a = {max_val, 1.0};
        std::vector<double> vec_b = {-max_val, 1.0};

        FATP_ASSERT_TRUE(test_throws("Sub overflow to Inf",
                                     [&]() {
                                         (void)checked_sub_vec_fp<ThrowOnErrorPolicy>(vec_a, vec_b);
                                     }),
                         "Should detect subtraction overflow");
    }

    std::cout << colors::green() << "[FP VEC EDGE] Boundary detection: PASSED" << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// SIMD VECTOR INTEGRATION TESTS
// =============================================================================

/**
 * @brief Verify SimdVector width detection works correctly
 */
FATP_TEST_CASE(simd_width_detection)
{
    std::cout << colors::cyan() << "\n[SIMD INTEGRATION] Testing SimdVector width detection..." << colors::reset()
              << std::endl;

    std::cout << "  Float width: " << SimdVectorF::width << std::endl;
    std::cout << "  Double width: " << SimdVectorD::width << std::endl;
    std::cout << "  Architecture: " << SimdArchitecture::name << std::endl;

    FATP_ASSERT_TRUE(SimdVectorF::width > 0, "Float width must be positive");
    FATP_ASSERT_TRUE(SimdVectorD::width > 0, "Double width must be positive");

    std::cout << colors::green() << "[SIMD INTEGRATION] Width detection: PASSED" << colors::reset() << std::endl;
    return true;
}

/**
 * @brief Test tail processing for non-aligned vector sizes
 */
FATP_TEST_CASE(simd_tail_processing)
{
    std::cout << colors::cyan() << "\n[SIMD INTEGRATION] Testing tail processing..." << colors::reset() << std::endl;

    // Use odd size that won't align to any SIMD width
    const size_t odd_size = 17;
    std::vector<double> a(odd_size), b(odd_size);

    for (size_t i = 0; i < odd_size; ++i)
    {
        a[i] = static_cast<double>(i + 1);
        b[i] = static_cast<double>(i + 10);
    }

    auto result = checked_add_vec_fp<ReturnExpectedPolicy>(a, b);

    FATP_ASSERT_TRUE(result.has_value(), "Tail processing should succeed");
    FATP_ASSERT_TRUE(result.value().size() == odd_size, "Result size should match");

    // Verify all elements
    for (size_t i = 0; i < odd_size; ++i)
    {
        double expected = a[i] + b[i];
        FATP_ASSERT_TRUE(std::abs(result.value()[i] - expected) < 1e-10, "Element mismatch in tail processing");
    }

    std::cout << colors::green() << "[SIMD INTEGRATION] Tail processing: PASSED" << colors::reset() << std::endl;
    return true;
}

/**
 * @brief Test IEEE-754 Inf handling (Inf inputs are allowed)
 */
FATP_TEST_CASE(simd_inf_handling)
{
    std::cout << colors::cyan() << "\n[SIMD INTEGRATION] Testing IEEE-754 Inf handling..." << colors::reset()
              << std::endl;

    std::vector<double> a = {1.0, std::numeric_limits<double>::infinity(), 3.0, 4.0};
    std::vector<double> b = {10.0, 20.0, 30.0, 40.0};

    // Inf in input is ALLOWED (IEEE-754 compliant)
    // Inf + 20 = Inf, which is expected behavior not an error
    auto result = checked_add_vec_fp<ReturnExpectedPolicy>(a, b);

    FATP_ASSERT_TRUE(result.has_value(), "Inf input should be allowed (IEEE-754)");
    FATP_ASSERT_TRUE(std::isinf(result.value()[1]), "Inf + 20 = Inf");
    FATP_ASSERT_TRUE(result.value()[0] == 11.0, "Normal element 1+10=11");
    FATP_ASSERT_TRUE(result.value()[2] == 33.0, "Normal element 3+30=33");

    std::cout << colors::green() << "[SIMD INTEGRATION] Inf handling: PASSED" << colors::reset() << std::endl;
    return true;
}

/**
 * @brief Test overflow detection (finite inputs -> Inf result)
 */
FATP_TEST_CASE(simd_overflow_to_inf)
{
    std::cout << colors::cyan() << "\n[SIMD INTEGRATION] Testing overflow detection..." << colors::reset() << std::endl;

    std::vector<double> huge = {1e308, 1e308};
    std::vector<double> mult = {2.0, 2.0};

    auto result = checked_mul_vec_fp<ReturnExpectedPolicy>(huge, mult);

    // Large * 2 overflows to Inf - should fail
    FATP_ASSERT_TRUE(!result.has_value(), "Overflow should fail");
    FATP_ASSERT_TRUE(result.error() == MathError::Inf, "Error should be Inf (overflow)");

    std::cout << colors::green() << "[SIMD INTEGRATION] Overflow detection: PASSED" << colors::reset() << std::endl;
    return true;
}

/**
 * @brief Test float type uses SimdVector<float>
 */
FATP_TEST_CASE(simd_float_type)
{
    std::cout << colors::cyan() << "\n[SIMD INTEGRATION] Testing float SIMD path..." << colors::reset() << std::endl;

    std::vector<float> a = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
    std::vector<float> b = {10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 70.0f, 80.0f};

    auto result = checked_add_vec_fp<ReturnExpectedPolicy>(a, b);

    FATP_ASSERT_TRUE(result.has_value(), "Float vec add should succeed");
    FATP_ASSERT_TRUE(result.value()[0] == 11.0f, "1 + 10 = 11");
    FATP_ASSERT_TRUE(result.value()[7] == 88.0f, "8 + 80 = 88");

    std::cout << colors::green() << "[SIMD INTEGRATION] Float path: PASSED" << colors::reset() << std::endl;
    return true;
}

/**
 * @brief Test all four FP vector operations with basic values
 */
FATP_TEST_CASE(simd_all_ops)
{
    std::cout << colors::cyan() << "\n[SIMD INTEGRATION] Testing all FP vector ops..." << colors::reset() << std::endl;

    std::vector<double> a = {10.0, 20.0, 30.0, 40.0};
    std::vector<double> b = {2.0, 4.0, 5.0, 8.0};

    // Add
    auto add_result = checked_add_vec_fp<ReturnExpectedPolicy>(a, b);
    FATP_ASSERT_TRUE(add_result.has_value(), "Add should succeed");
    FATP_ASSERT_TRUE(add_result.value()[0] == 12.0, "10+2=12");

    // Sub
    auto sub_result = checked_sub_vec_fp<ReturnExpectedPolicy>(a, b);
    FATP_ASSERT_TRUE(sub_result.has_value(), "Sub should succeed");
    FATP_ASSERT_TRUE(sub_result.value()[0] == 8.0, "10-2=8");

    // Mul
    auto mul_result = checked_mul_vec_fp<ReturnExpectedPolicy>(a, b);
    FATP_ASSERT_TRUE(mul_result.has_value(), "Mul should succeed");
    FATP_ASSERT_TRUE(mul_result.value()[0] == 20.0, "10*2=20");

    // Div
    auto div_result = checked_div_vec_fp<ReturnExpectedPolicy>(a, b);
    FATP_ASSERT_TRUE(div_result.has_value(), "Div should succeed");
    FATP_ASSERT_TRUE(div_result.value()[0] == 5.0, "10/2=5");

    std::cout << colors::green() << "[SIMD INTEGRATION] All ops: PASSED" << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// FP VECTOR ERROR PATH TESTS
// =============================================================================

/**
 * @brief Test that ThrowOnError policy throws on finite→Inf overflow
 */
FATP_TEST_CASE(fp_vec_throw_on_overflow)
{
    std::cout << colors::cyan() << "\n[FP VEC ERROR] Testing ThrowOnError overflow..." << colors::reset() << std::endl;

    std::vector<double> huge = {1e308, 1.0, 2.0, 3.0};
    std::vector<double> mult = {2.0, 1.0, 1.0, 1.0};

    // First lane overflows: 1e308 * 2 = Inf
    FATP_ASSERT_TRUE(test_throws("Finite->Inf overflow",
                                 [&]() {
                                     (void)checked_mul_vec_fp<ThrowOnErrorPolicy>(huge, mult);
                                 }),
                     "Should throw on finite->Inf overflow");

    std::cout << colors::green() << "[FP VEC ERROR] ThrowOnError overflow: PASSED" << colors::reset() << std::endl;
    return true;
}

/**
 * @brief Test mixed lane Inf+(-Inf) triggers scalar fallback correctly
 *
 * When one lane has +Inf + -Inf (which produces NaN), the SIMD path
 * should detect this via has_nan() and fall back to scalar processing
 * for proper error classification.
 */
FATP_TEST_CASE(fp_vec_inf_inf_fallback)
{
    std::cout << colors::cyan() << "\n[FP VEC ERROR] Testing Inf+(-Inf) fallback..." << colors::reset() << std::endl;

    const double pos_inf = std::numeric_limits<double>::infinity();
    const double neg_inf = -std::numeric_limits<double>::infinity();

    // Mixed lanes: some normal, one has +Inf + -Inf = NaN
    std::vector<double> a = {1.0, pos_inf, 3.0, 4.0};
    std::vector<double> b = {10.0, neg_inf, 30.0, 40.0};

    // +Inf + -Inf is undefined → should produce error (NaN result)
    auto result = checked_add_vec_fp<ReturnExpectedPolicy>(a, b);

    FATP_ASSERT_TRUE(!result.has_value(), "Inf + (-Inf) should fail");
    FATP_ASSERT_TRUE(result.error() == MathError::NaN, "Error should be NaN");

    // Also test with ThrowOnError
    FATP_ASSERT_TRUE(test_throws("Inf + (-Inf)",
                                 [&]() {
                                     (void)checked_add_vec_fp<ThrowOnErrorPolicy>(a, b);
                                 }),
                     "Should throw on Inf + (-Inf)");

    std::cout << colors::green() << "[FP VEC ERROR] Inf+(-Inf) fallback: PASSED" << colors::reset() << std::endl;
    return true;
}

/**
 * @brief Test InfTolerant policy allows finite→Inf at vector level
 */
FATP_TEST_CASE(fp_vec_inf_tolerant_overflow)
{
    std::cout << colors::cyan() << "\n[FP VEC ERROR] Testing InfTolerant overflow..." << colors::reset() << std::endl;

    std::vector<double> huge = {1e308, 1.0, 2.0, 3.0};
    std::vector<double> mult = {2.0, 1.0, 1.0, 1.0};

    // InfTolerant should allow finite→Inf (returns Inf, not error)
    auto result = checked_mul_vec_fp<InfTolerantPolicy>(huge, mult);

    FATP_ASSERT_TRUE(result.size() == 4, "InfTolerant should return result");
    FATP_ASSERT_TRUE(std::isinf(result[0]), "First lane should be Inf");
    FATP_ASSERT_TRUE(result[1] == 1.0, "Second lane should be 1.0");
    FATP_ASSERT_TRUE(result[2] == 2.0, "Third lane should be 2.0");

    std::cout << colors::green() << "[FP VEC ERROR] InfTolerant overflow: PASSED" << colors::reset() << std::endl;
    return true;
}

/**
 * @brief Test InfTolerant propagates NaN correctly in vectors
 */
FATP_TEST_CASE(fp_vec_inf_tolerant_nan)
{
    std::cout << colors::cyan() << "\n[FP VEC ERROR] Testing InfTolerant NaN handling..." << colors::reset()
              << std::endl;

    const double qnan = std::numeric_limits<double>::quiet_NaN();

    // NaN in one lane should propagate
    std::vector<double> a = {1.0, qnan, 3.0, 4.0};
    std::vector<double> b = {10.0, 20.0, 30.0, 40.0};

    auto result = checked_add_vec_fp<InfTolerantPolicy>(a, b);

    // InfTolerant returns NaN for NaN inputs (doesn't error)
    FATP_ASSERT_TRUE(result.size() == 4, "InfTolerant should return result");
    FATP_ASSERT_TRUE(result[0] == 11.0, "First lane: 1+10=11");
    FATP_ASSERT_TRUE(std::isnan(result[1]), "Second lane should be NaN");
    FATP_ASSERT_TRUE(result[2] == 33.0, "Third lane: 3+30=33");

    std::cout << colors::green() << "[FP VEC ERROR] InfTolerant NaN: PASSED" << colors::reset() << std::endl;
    return true;
}

/**
 * @brief Test Inf-Inf subtraction in vector with InfTolerant (still produces NaN)
 */
FATP_TEST_CASE(fp_vec_inf_tolerant_inf_minus_inf)
{
    std::cout << colors::cyan() << "\n[FP VEC ERROR] Testing InfTolerant Inf-Inf..." << colors::reset() << std::endl;

    const double pos_inf = std::numeric_limits<double>::infinity();

    // Inf - Inf = NaN (even under InfTolerant, this is undefined)
    std::vector<double> a = {1.0, pos_inf, 3.0, 4.0};
    std::vector<double> b = {10.0, pos_inf, 30.0, 40.0};

    auto result = checked_sub_vec_fp<InfTolerantPolicy>(a, b);

    FATP_ASSERT_TRUE(result.size() == 4, "InfTolerant should return result");
    FATP_ASSERT_TRUE(result[0] == -9.0, "First lane: 1-10=-9");
    FATP_ASSERT_TRUE(std::isnan(result[1]), "Second lane: Inf-Inf=NaN");
    FATP_ASSERT_TRUE(result[2] == -27.0, "Third lane: 3-30=-27");

    std::cout << colors::green() << "[FP VEC ERROR] InfTolerant Inf-Inf: PASSED" << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// PERFORMANCE VALIDATION TESTS
// =============================================================================

FATP_TEST_CASE(performance_benchmarks)
{
    std::cout << colors::cyan() << "\n[PERFORMANCE TEST] Running benchmarks..." << colors::reset() << std::endl;

    const int ITERATIONS = 100000;
    volatile int32_t accumulator = 0;

    {
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < ITERATIONS; ++i)
        {
            auto res = checked_add<ThrowOnErrorPolicy>(100, 200);
            accumulator += res;
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        double avg_ns = static_cast<double>(duration.count()) / ITERATIONS;

        std::cout << "  Scalar checked_add: " << avg_ns << " ns/op" << std::endl;
        FATP_ASSERT_TRUE(avg_ns < 50.0, "Scalar addition should be fast (< 50ns)");
    }

    {
        std::vector<int32_t> vec_a(1000, 10);
        std::vector<int32_t> vec_b(1000, 20);

        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 100; ++i)
        {
            auto res = checked_add_vec<ThrowOnErrorPolicy>(vec_a, vec_b);
            accumulator += res[0];
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        double avg_us = static_cast<double>(duration.count()) / 100.0;

        std::cout << "  Vector checked_add_vec (1K elems): " << avg_us << " us/op" << std::endl;

#ifdef __AVX2__
        FATP_ASSERT_TRUE(avg_us < 20.0, "SIMD vector addition should be reasonable (< 20us for 1K)");
        if (avg_us < 5.0)
        {
            std::cout << colors::green() << "  [AVX2 ENABLED] Excellent performance" << colors::reset() << std::endl;
        }
        else
        {
            std::cout << colors::yellow() << "  [AVX2 ENABLED] Performance acceptable "
                      << "(thermal throttling or VM?)" << colors::reset() << std::endl;
        }
#else
        std::cout << colors::yellow() << "  [Scalar fallback] Performance acceptable" << colors::reset() << std::endl;
#endif
    }

    if (accumulator == 0x7FFFFFFF)
    {
        std::cout << "Unlikely sentinel" << std::endl;
    }

    std::cout << colors::green() << "[PERFORMANCE] Benchmarks: PASSED" << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// EXISTING CORE TESTS
// =============================================================================

FATP_TEST_CASE(checked_add)
{
    std::cout << colors::cyan() << "\nTesting checked_add..." << colors::reset() << std::endl;

    {
        auto result = checked_add<ThrowOnErrorPolicy>(10, 20);
        FATP_ASSERT_EQ(result, 30, "10 + 20 should equal 30");

        result = checked_add<ThrowOnErrorPolicy>(-10, -20);
        FATP_ASSERT_EQ(result, -30, "-10 + -20 should equal -30");
    }

    {
        FATP_ASSERT_TRUE(test_throws("INT_MAX + 1",
                                     []() {
                                         (void)checked_add<ThrowOnErrorPolicy>(std::numeric_limits<int>::max(), 1);
                                     }),
                         "Should throw on overflow");

        FATP_ASSERT_TRUE(test_throws("INT_MIN + (-1)",
                                     []() {
                                         (void)checked_add<ThrowOnErrorPolicy>(std::numeric_limits<int>::min(), -1);
                                     }),
                         "Should throw on underflow");
    }

    {
        auto result = checked_add<ReturnExpectedPolicy>(10, 20);
        FATP_ASSERT_TRUE(result.has_value(), "Should have value");
        FATP_ASSERT_EQ(*result, 30, "10 + 20 should equal 30");

        result = checked_add<ReturnExpectedPolicy>(std::numeric_limits<int>::max(), 1);
        FATP_ASSERT_TRUE(!result.has_value(), "Should not have value on overflow");
        FATP_ASSERT_EQ(result.error(), MathError::Overflow, "Should return Overflow error");
    }

    {
        auto result = checked_add<SaturatingPolicy>(std::numeric_limits<int>::max(), 1);
        FATP_ASSERT_EQ(result, std::numeric_limits<int>::max(), "Should saturate to max");

        result = checked_add<SaturatingPolicy>(std::numeric_limits<int>::min(), -1);
        FATP_ASSERT_EQ(result, std::numeric_limits<int>::min(), "Should saturate to min");
    }

    std::cout << colors::green() << "checked_add: Tests passed." << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(checked_sub)
{
    std::cout << colors::cyan() << "\nTesting checked_sub..." << colors::reset() << std::endl;

    {
        auto result = checked_sub<ThrowOnErrorPolicy>(20, 10);
        FATP_ASSERT_EQ(result, 10, "20 - 10 should equal 10");
    }

    {
        FATP_ASSERT_TRUE(test_throws("INT_MIN - 1",
                                     []() {
                                         (void)checked_sub<ThrowOnErrorPolicy>(std::numeric_limits<int>::min(), 1);
                                     }),
                         "Should throw on underflow");
    }

    std::cout << colors::green() << "checked_sub: Tests passed." << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(checked_mul)
{
    std::cout << colors::cyan() << "\nTesting checked_mul..." << colors::reset() << std::endl;

    {
        auto result = checked_mul<ThrowOnErrorPolicy>(5, 6);
        FATP_ASSERT_EQ(result, 30, "5 * 6 should equal 30");
    }

    {
        auto result = checked_mul<ThrowOnErrorPolicy>(0, 100);
        FATP_ASSERT_EQ(result, 0, "0 * 100 should equal 0");

        result = checked_mul<ThrowOnErrorPolicy>(100, 0);
        FATP_ASSERT_EQ(result, 0, "100 * 0 should equal 0");
    }

    {
        FATP_ASSERT_TRUE(test_throws("Large mul",
                                     []() {
                                         (void)checked_mul<ThrowOnErrorPolicy>(100000, 100000);
                                     }),
                         "Should throw on overflow");
    }

    std::cout << colors::green() << "checked_mul: Tests passed." << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(checked_div)
{
    std::cout << colors::cyan() << "\nTesting checked_div..." << colors::reset() << std::endl;

    {
        auto result = checked_div<ThrowOnErrorPolicy>(20, 5);
        FATP_ASSERT_EQ(result, 4, "20 / 5 should equal 4");
    }

    {
        FATP_ASSERT_TRUE(test_throws("Div by zero",
                                     []() {
                                         (void)checked_div<ThrowOnErrorPolicy>(10, 0);
                                     }),
                         "Should throw on div by zero");

        auto pos_result = checked_div<SaturatingPolicy>(10, 0);
        FATP_ASSERT_EQ(pos_result, std::numeric_limits<int>::max(), "Positive div zero saturates");

        auto neg_result = checked_div<SaturatingPolicy>(-10, 0);
        FATP_ASSERT_EQ(neg_result, std::numeric_limits<int>::min(), "Negative div zero saturates");
    }

    {
        FATP_ASSERT_TRUE(test_throws("INT_MIN / -1",
                                     []() {
                                         (void)checked_div<ThrowOnErrorPolicy>(std::numeric_limits<int>::min(), -1);
                                     }),
                         "Should throw on min/-1 overflow");
    }

    std::cout << colors::green() << "checked_div: Tests passed." << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(checked_fp_operations)
{
    std::cout << colors::cyan() << "\nTesting FP operations..." << colors::reset() << std::endl;

    {
        auto result = checked_add_fp<ThrowOnErrorPolicy>(1.5, 2.5);
        FATP_ASSERT_EQ(result, 4.0, "1.5 + 2.5 should equal 4.0");
    }

    {
        double nan_val = std::numeric_limits<double>::quiet_NaN();

        FATP_ASSERT_TRUE(test_throws("NaN + 1",
                                     [&]() {
                                         (void)checked_add_fp<ThrowOnErrorPolicy>(nan_val, 1.0);
                                     }),
                         "Should throw on NaN input");
    }

    {
        auto pos_result = checked_div_fp<SaturatingPolicy>(5.0, 0.0);
        FATP_ASSERT_EQ(pos_result, std::numeric_limits<double>::max(), "Positive FP div by zero saturates to max");

        auto neg_result = checked_div_fp<SaturatingPolicy>(-5.0, 0.0);
        FATP_ASSERT_EQ(neg_result,
                       std::numeric_limits<double>::lowest(),
                       "Negative FP div by zero saturates to lowest");
    }

    std::cout << colors::green() << "FP operations: Tests passed." << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(checked_mod)
{
    std::cout << colors::cyan() << "\nTesting checked_mod..." << colors::reset() << std::endl;

    {
        auto result = checked_mod<ThrowOnErrorPolicy>(17, 5);
        FATP_ASSERT_EQ(result, 2, "17 % 5 should equal 2");

        result = checked_mod<ThrowOnErrorPolicy>(-17, 5);
        FATP_ASSERT_EQ(result, -2, "-17 % 5 should equal -2");
    }

    {
        FATP_ASSERT_TRUE(test_throws("Mod by zero",
                                     []() {
                                         (void)checked_mod<ThrowOnErrorPolicy>(10, 0);
                                     }),
                         "Should throw on mod by zero");
    }

    std::cout << colors::green() << "checked_mod: Tests passed." << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(checked_negate)
{
    std::cout << colors::cyan() << "\nTesting checked_negate..." << colors::reset() << std::endl;

    {
        auto result = checked_negate<ThrowOnErrorPolicy>(10);
        FATP_ASSERT_EQ(result, -10, "Negation of positive should work");
    }

    {
        auto result = checked_negate<ThrowOnErrorPolicy>(-42);
        FATP_ASSERT_EQ(result, 42, "Negation of negative should work");
    }

    {
        auto result = checked_negate<ThrowOnErrorPolicy>(0);
        FATP_ASSERT_EQ(result, 0, "Negation of zero should be zero");
    }

    {
        FATP_ASSERT_TRUE(test_throws("Negate INT_MIN",
                                     []() {
                                         (void)checked_negate<ThrowOnErrorPolicy>(std::numeric_limits<int>::min());
                                     }),
                         "Should throw on INT_MIN negation (overflow)");
    }

    {
        auto result = checked_negate<ReturnExpectedPolicy>(std::numeric_limits<int>::min());
        FATP_ASSERT_TRUE(!result.has_value(), "Should fail on INT_MIN negation");
        FATP_ASSERT_EQ(result.error(), MathError::Overflow, "Should return Overflow error");
    }

    {
        auto result = checked_negate<SaturatingPolicy>(std::numeric_limits<int>::min());
        FATP_ASSERT_EQ(result, std::numeric_limits<int>::max(), "INT_MIN negation should saturate to INT_MAX");
    }

    {
        auto result = checked_negate<ThrowOnErrorPolicy>(int64_t{-1000000000000LL});
        FATP_ASSERT_EQ(result, 1000000000000LL, "int64 negation should work");
    }

    {
        FATP_ASSERT_TRUE(test_throws("Negate INT64_MIN",
                                     []() {
                                         (void)checked_negate<ThrowOnErrorPolicy>(std::numeric_limits<int64_t>::min());
                                     }),
                         "Should throw on INT64_MIN negation");
    }

    std::cout << colors::green() << "checked_negate: Tests passed." << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(bitwise_operations)
{
    std::cout << colors::cyan() << "\nTesting bitwise operations..." << colors::reset() << std::endl;

    {
        auto result = checked_and<ThrowOnErrorPolicy>(0b1100, 0b1010);
        FATP_ASSERT_EQ(result, 0b1000, "AND operation should work");

        result = checked_or<ThrowOnErrorPolicy>(0b1100, 0b1010);
        FATP_ASSERT_EQ(result, 0b1110, "OR operation should work");

        result = checked_xor<ThrowOnErrorPolicy>(0b1100, 0b1010);
        FATP_ASSERT_EQ(result, 0b0110, "XOR operation should work");
    }

    {
        auto result = checked_left_shift<ThrowOnErrorPolicy>(1, 4);
        FATP_ASSERT_EQ(result, 16, "1 << 4 should equal 16");

        result = checked_right_shift<ThrowOnErrorPolicy>(16, 2);
        FATP_ASSERT_EQ(result, 4, "16 >> 2 should equal 4");
    }

    std::cout << colors::green() << "Bitwise operations: Tests passed." << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(clamp_and_range)
{
    std::cout << colors::cyan() << "\nTesting clamp and range operations..." << colors::reset() << std::endl;

    {
        auto result = checked_clamp<ThrowOnErrorPolicy>(5, 0, 10);
        FATP_ASSERT_EQ(result, 5, "Value within range should be unchanged");

        result = checked_clamp<ThrowOnErrorPolicy>(-5, 0, 10);
        FATP_ASSERT_EQ(result, 0, "Value below range should clamp to min");

        result = checked_clamp<ThrowOnErrorPolicy>(15, 0, 10);
        FATP_ASSERT_EQ(result, 10, "Value above range should clamp to max");
    }

    {
        auto result = checked_in_range<ReturnExpectedPolicy>(5, 0, 10);
        FATP_ASSERT_TRUE(result.has_value() && *result == true, "5 should be in range [0,10]");

        result = checked_in_range<ReturnExpectedPolicy>(15, 0, 10);
        FATP_ASSERT_TRUE(result.has_value() && *result == false, "15 should not be in range [0,10]");
    }

    std::cout << colors::green() << "Clamp and range: Tests passed." << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// CHECKED CAST TESTS
// =============================================================================

FATP_TEST_CASE(checked_cast_basic)
{
    std::cout << colors::cyan() << "\nTesting checked_cast basic conversions..." << colors::reset() << std::endl;

    {
        int32_t small = 100;
        auto result = checked_cast<int64_t, ThrowOnErrorPolicy>(small);
        FATP_ASSERT_EQ(result, 100LL, "int32 -> int64 should work");
    }

    {
        int16_t small = 1000;
        auto result = checked_cast<int32_t, ThrowOnErrorPolicy>(small);
        FATP_ASSERT_EQ(result, 1000, "int16 -> int32 should work");
    }

    {
        uint32_t u = 100;
        auto result = checked_cast<uint64_t, ThrowOnErrorPolicy>(u);
        FATP_ASSERT_EQ(result, 100ULL, "uint32 -> uint64 should work");
    }

    {
        int32_t positive = 100;
        auto result = checked_cast<uint32_t, ThrowOnErrorPolicy>(positive);
        FATP_ASSERT_EQ(result, 100U, "positive int32 -> uint32 should work");
    }

    std::cout << colors::green() << "checked_cast basic: Tests passed." << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(checked_cast_narrowing)
{
    std::cout << colors::cyan() << "\nTesting checked_cast narrowing overflow..." << colors::reset() << std::endl;

    {
        int64_t big = 1000000000000LL;
        FATP_ASSERT_TRUE(test_throws("int64 -> int32 overflow",
                                     [&]() {
                                         (void)checked_cast<int32_t, ThrowOnErrorPolicy>(big);
                                     }),
                         "Should throw on narrowing overflow");
    }

    {
        int64_t big = 1000000000000LL;
        auto result = checked_cast<int32_t, ReturnExpectedPolicy>(big);
        FATP_ASSERT_TRUE(!result.has_value(), "Should fail on narrowing overflow");
        FATP_ASSERT_EQ(result.error(), MathError::Overflow, "Should return Overflow error");
    }

    {
        int64_t big = 1000000000000LL;
        auto result = checked_cast<int32_t, SaturatingPolicy>(big);
        FATP_ASSERT_EQ(result, std::numeric_limits<int32_t>::max(), "Should saturate to max");
    }

    {
        int64_t neg_big = -1000000000000LL;
        auto result = checked_cast<int32_t, SaturatingPolicy>(neg_big);
        FATP_ASSERT_EQ(result, std::numeric_limits<int32_t>::min(), "Should saturate to min");
    }

    std::cout << colors::green() << "checked_cast narrowing: Tests passed." << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(checked_cast_sign_conversion)
{
    std::cout << colors::cyan() << "\nTesting checked_cast sign conversions..." << colors::reset() << std::endl;

    {
        int32_t negative = -100;
        FATP_ASSERT_TRUE(test_throws("negative int -> unsigned",
                                     [&]() {
                                         (void)checked_cast<uint32_t, ThrowOnErrorPolicy>(negative);
                                     }),
                         "Should throw on negative to unsigned");
    }

    {
        int32_t negative = -100;
        auto result = checked_cast<uint32_t, ReturnExpectedPolicy>(negative);
        FATP_ASSERT_TRUE(!result.has_value(), "Should fail on negative to unsigned");
        FATP_ASSERT_EQ(result.error(), MathError::Underflow, "Should return Underflow error");
    }

    {
        int32_t negative = -100;
        auto result = checked_cast<uint32_t, SaturatingPolicy>(negative);
        FATP_ASSERT_EQ(result, 0U, "Negative should saturate to 0 for unsigned");
    }

    {
        uint64_t big_unsigned = static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) + 1;
        FATP_ASSERT_TRUE(test_throws("large unsigned -> signed",
                                     [&]() {
                                         (void)checked_cast<int64_t, ThrowOnErrorPolicy>(big_unsigned);
                                     }),
                         "Should throw on unsigned overflow to signed");
    }

    std::cout << colors::green() << "checked_cast sign conversion: Tests passed." << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(checked_cast_fp_to_int)
{
    std::cout << colors::cyan() << "\nTesting checked_cast FP to integer..." << colors::reset() << std::endl;

    {
        double d = 100.5;
        auto result = checked_cast<int32_t, ThrowOnErrorPolicy>(d);
        FATP_ASSERT_EQ(result, 100, "double -> int32 should truncate");
    }

    {
        double big = 1e15;
        FATP_ASSERT_TRUE(test_throws("large double -> int32",
                                     [&]() {
                                         (void)checked_cast<int32_t, ThrowOnErrorPolicy>(big);
                                     }),
                         "Should throw on FP overflow");
    }

    {
        double nan = std::numeric_limits<double>::quiet_NaN();
        FATP_ASSERT_TRUE(test_throws("NaN -> int",
                                     [&]() {
                                         (void)checked_cast<int32_t, ThrowOnErrorPolicy>(nan);
                                     }),
                         "Should throw on NaN");
    }

    {
        double inf = std::numeric_limits<double>::infinity();
        FATP_ASSERT_TRUE(test_throws("Inf -> int",
                                     [&]() {
                                         (void)checked_cast<int32_t, ThrowOnErrorPolicy>(inf);
                                     }),
                         "Should throw on Inf");
    }

    {
        double inf = std::numeric_limits<double>::infinity();
        auto result = checked_cast<int32_t, SaturatingPolicy>(inf);
        FATP_ASSERT_EQ(result, std::numeric_limits<int32_t>::max(), "Inf should saturate to max");

        double neg_inf = -std::numeric_limits<double>::infinity();
        auto neg_result = checked_cast<int32_t, SaturatingPolicy>(neg_inf);
        FATP_ASSERT_EQ(neg_result, std::numeric_limits<int32_t>::min(), "-Inf should saturate to min");
    }

    std::cout << colors::green() << "checked_cast FP to int: Tests passed." << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(checked_cast_fp_to_fp)
{
    std::cout << colors::cyan() << "\nTesting checked_cast FP to FP..." << colors::reset() << std::endl;

    {
        double d = 3.14159;
        auto result = checked_cast<float, ThrowOnErrorPolicy>(d);
        FATP_ASSERT_TRUE(std::abs(result - 3.14159f) < 0.0001f, "double -> float should work");
    }

    {
        float f = 2.71828f;
        auto result = checked_cast<double, ThrowOnErrorPolicy>(f);
        FATP_ASSERT_TRUE(std::abs(result - 2.71828) < 0.0001, "float -> double should work");
    }

    {
        double nan = std::numeric_limits<double>::quiet_NaN();
        auto result = checked_cast<float, SaturatingPolicy>(nan);
        FATP_ASSERT_TRUE(std::isnan(result), "NaN should convert to NaN with Saturating");
    }

    std::cout << colors::green() << "checked_cast FP to FP: Tests passed." << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(static_checked_cast)
{
    std::cout << colors::cyan() << "\nTesting static_checked_cast (compile-time)..." << colors::reset() << std::endl;

    {
        constexpr int32_t result = static_checked_cast<int32_t, int64_t, 100LL>();
        FATP_ASSERT_EQ(result, 100, "Compile-time cast should work");
    }

    {
        constexpr int16_t result = static_checked_cast<int16_t, int32_t, 1000>();
        FATP_ASSERT_EQ(result, 1000, "Compile-time narrowing should work when in range");
    }

    {
        constexpr uint32_t result = static_checked_cast<uint32_t, int32_t, 500>();
        FATP_ASSERT_EQ(result, 500U, "Compile-time sign conversion should work when positive");
    }

    std::cout << colors::green() << "static_checked_cast: Tests passed." << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// INTEGER SIMD ACCELERATION TESTS
// =============================================================================

FATP_TEST_CASE(intsimd_architecture)
{
    std::cout << colors::cyan() << "\n[Integer SIMD] Architecture detection..." << colors::reset() << std::endl;

    std::cout << "  Architecture: " << int_simd::IntSimdInfo::architecture() << "\n";
    std::cout << "  int32 width: " << int_simd::IntSimdInfo::int32_width() << "\n";
    std::cout << "  int64 width: " << int_simd::IntSimdInfo::int64_width() << "\n";
    std::cout << "  saturating hw: " << (int_simd::IntSimdInfo::has_saturating_hardware() ? "yes" : "no") << "\n";

    FATP_ASSERT_TRUE(int_simd::IntSimdInfo::int32_width() >= 1, "int32 width must be >= 1");
    FATP_ASSERT_TRUE(int_simd::IntSimdInfo::int64_width() >= 1, "int64 width must be >= 1");

    std::cout << colors::green() << "  Architecture detection: PASSED" << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(intsimd_basic_add_i32)
{
    std::cout << colors::cyan() << "\n[Integer SIMD] Basic add int32..." << colors::reset() << std::endl;

    std::vector<int32_t> a = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    std::vector<int32_t> b = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150, 160};

    auto result = checked_add_vec<ThrowOnErrorPolicy>(a, b);

    FATP_ASSERT_EQ(result.size(), a.size(), "Result size mismatch");
    for (size_t i = 0; i < a.size(); ++i)
    {
        FATP_ASSERT_EQ(result[i], a[i] + b[i], "Element mismatch at index");
    }

    std::cout << colors::green() << "  Basic add int32: PASSED" << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(intsimd_overflow_detection_i32)
{
    std::cout << colors::cyan() << "\n[Integer SIMD] Overflow detection int32..." << colors::reset() << std::endl;

    std::vector<int32_t> a = {1, std::numeric_limits<int32_t>::max(), 3, 4, 5, 6, 7, 8};
    std::vector<int32_t> b = {10, 1, 30, 40, 50, 60, 70, 80}; // Second element overflows

    auto result = checked_add_vec<ReturnExpectedPolicy>(a, b);

    FATP_ASSERT_TRUE(!result.has_value(), "Should detect overflow");
    FATP_ASSERT_EQ(result.error(), MathError::Overflow, "Should be overflow error");

    std::cout << colors::green() << "  Overflow detection int32: PASSED" << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(intsimd_saturating_add_i32)
{
    std::cout << colors::cyan() << "\n[Integer SIMD] Saturating add int32..." << colors::reset() << std::endl;

    std::vector<int32_t> a = {1, std::numeric_limits<int32_t>::max(), 3, 4, 5, 6, 7, 8};
    std::vector<int32_t> b = {10, 1, 30, 40, 50, 60, 70, 80};

    auto result = checked_add_vec<SaturatingPolicy>(a, b);

    FATP_ASSERT_EQ(result.size(), a.size(), "Result size mismatch");
    FATP_ASSERT_EQ(result[0], 11, "Normal add should work");
    FATP_ASSERT_EQ(result[1], std::numeric_limits<int32_t>::max(), "Overflow should saturate");
    FATP_ASSERT_EQ(result[2], 33, "Normal add should work");

    std::cout << colors::green() << "  Saturating add int32: PASSED" << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(intsimd_unsigned_add_u32)
{
    std::cout << colors::cyan() << "\n[Integer SIMD] Unsigned add uint32..." << colors::reset() << std::endl;

    std::vector<uint32_t> a = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    std::vector<uint32_t> b = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150, 160};

    auto result = checked_add_vec<ThrowOnErrorPolicy>(a, b);

    FATP_ASSERT_EQ(result.size(), a.size(), "Result size mismatch");
    for (size_t i = 0; i < a.size(); ++i)
    {
        FATP_ASSERT_EQ(result[i], a[i] + b[i], "Element mismatch at index");
    }

    std::cout << colors::green() << "  Unsigned add uint32: PASSED" << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(intsimd_unsigned_overflow_u32)
{
    std::cout << colors::cyan() << "\n[Integer SIMD] Unsigned overflow uint32..." << colors::reset() << std::endl;

    std::vector<uint32_t> a = {1, std::numeric_limits<uint32_t>::max(), 3, 4, 5, 6, 7, 8};
    std::vector<uint32_t> b = {10, 1, 30, 40, 50, 60, 70, 80};

    auto result = checked_add_vec<ReturnExpectedPolicy>(a, b);

    FATP_ASSERT_TRUE(!result.has_value(), "Should detect overflow");
    FATP_ASSERT_EQ(result.error(), MathError::Overflow, "Should be overflow error");

    std::cout << colors::green() << "  Unsigned overflow uint32: PASSED" << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(intsimd_tail_processing)
{
    std::cout << colors::cyan() << "\n[Integer SIMD] Tail processing..." << colors::reset() << std::endl;

    // Size 7: doesn't divide evenly by SIMD width (4 or 8)
    std::vector<int32_t> a = {1, 2, 3, 4, 5, 6, 7};
    std::vector<int32_t> b = {10, 20, 30, 40, 50, 60, 70};

    auto result = checked_add_vec<ThrowOnErrorPolicy>(a, b);

    FATP_ASSERT_EQ(result.size(), a.size(), "Result size mismatch");
    for (size_t i = 0; i < a.size(); ++i)
    {
        FATP_ASSERT_EQ(result[i], a[i] + b[i], "Element mismatch at index");
    }

    // Also test size 1 (pure scalar)
    std::vector<int32_t> a1 = {42};
    std::vector<int32_t> b1 = {58};
    auto result1 = checked_add_vec<ThrowOnErrorPolicy>(a1, b1);
    FATP_ASSERT_EQ(result1[0], 100, "Size 1 should work");

    std::cout << colors::green() << "  Tail processing: PASSED" << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(intsimd_sub_i32)
{
    std::cout << colors::cyan() << "\n[Integer SIMD] Subtraction int32..." << colors::reset() << std::endl;

    std::vector<int32_t> a = {100, 200, 300, 400, 500, 600, 700, 800};
    std::vector<int32_t> b = {10, 20, 30, 40, 50, 60, 70, 80};

    auto result = checked_sub_vec<ThrowOnErrorPolicy>(a, b);

    FATP_ASSERT_EQ(result.size(), a.size(), "Result size mismatch");
    for (size_t i = 0; i < a.size(); ++i)
    {
        FATP_ASSERT_EQ(result[i], a[i] - b[i], "Element mismatch at index");
    }

    // Test underflow detection
    std::vector<int32_t> c = {1, std::numeric_limits<int32_t>::min(), 3, 4};
    std::vector<int32_t> d = {10, 1, 30, 40}; // Second element underflows

    auto underflow_result = checked_sub_vec<ReturnExpectedPolicy>(c, d);
    FATP_ASSERT_TRUE(!underflow_result.has_value(), "Should detect underflow");
    FATP_ASSERT_EQ(underflow_result.error(), MathError::Underflow, "Should be underflow error");

    std::cout << colors::green() << "  Subtraction int32: PASSED" << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(intsimd_large_vectors)
{
    std::cout << colors::cyan() << "\n[Integer SIMD] Large vectors..." << colors::reset() << std::endl;

    // Test with 1000 elements to ensure multiple SIMD iterations
    const size_t n = 1000;
    std::vector<int32_t> a(n);
    std::vector<int32_t> b(n);

    for (size_t i = 0; i < n; ++i)
    {
        a[i] = static_cast<int32_t>(i);
        b[i] = static_cast<int32_t>(i * 2);
    }

    auto result = checked_add_vec<ThrowOnErrorPolicy>(a, b);

    FATP_ASSERT_EQ(result.size(), n, "Result size mismatch");
    for (size_t i = 0; i < n; ++i)
    {
        FATP_ASSERT_EQ(result[i], static_cast<int32_t>(i * 3), "Element mismatch at index");
    }

    std::cout << colors::green() << "  Large vectors: PASSED" << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(intsimd_mul_basic_i32)
{
    std::cout << colors::cyan() << "\n[Integer SIMD] Mul basic int32..." << colors::reset() << std::endl;

    std::vector<int32_t> a = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    std::vector<int32_t> b = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17};

    auto result = checked_mul_vec<ThrowOnErrorPolicy>(a, b);

    FATP_ASSERT_EQ(result.size(), a.size(), "Result size mismatch");
    for (size_t i = 0; i < a.size(); ++i)
    {
        FATP_ASSERT_EQ(result[i], a[i] * b[i], "Element mismatch at index");
    }

    std::cout << colors::green() << "  Mul basic int32: PASSED" << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(intsimd_mul_overflow_i32)
{
    std::cout << colors::cyan() << "\n[Integer SIMD] Mul overflow int32..." << colors::reset() << std::endl;

    // 50000 * 50000 = 2,500,000,000 > INT32_MAX (2,147,483,647)
    std::vector<int32_t> a = {1, 50000, 3, 4, 5, 6, 7, 8};
    std::vector<int32_t> b = {2, 50000, 6, 8, 10, 12, 14, 16};

    auto result = checked_mul_vec<ReturnExpectedPolicy>(a, b);

    FATP_ASSERT_TRUE(!result.has_value(), "Should detect overflow");
    FATP_ASSERT_EQ(result.error(), MathError::Overflow, "Should be overflow error");

    std::cout << colors::green() << "  Mul overflow int32: PASSED" << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(intsimd_mul_saturating_i32)
{
    std::cout << colors::cyan() << "\n[Integer SIMD] Mul saturating int32..." << colors::reset() << std::endl;

    std::vector<int32_t> a = {2, 50000, 3, -50000};
    std::vector<int32_t> b = {3, 50000, 4, 50000};

    auto result = checked_mul_vec<SaturatingPolicy>(a, b);

    FATP_ASSERT_EQ(result.size(), a.size(), "Result size mismatch");
    FATP_ASSERT_EQ(result[0], 6, "Normal mul should work");
    FATP_ASSERT_EQ(result[1], std::numeric_limits<int32_t>::max(), "Positive overflow should saturate to max");
    FATP_ASSERT_EQ(result[2], 12, "Normal mul should work");
    FATP_ASSERT_EQ(result[3], std::numeric_limits<int32_t>::min(), "Negative overflow should saturate to min");

    std::cout << colors::green() << "  Mul saturating int32: PASSED" << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(intsimd_mul_unsigned_u32)
{
    std::cout << colors::cyan() << "\n[Integer SIMD] Mul unsigned uint32..." << colors::reset() << std::endl;

    std::vector<uint32_t> a = {1, 2, 3, 4, 5, 6, 7, 8};
    std::vector<uint32_t> b = {10, 20, 30, 40, 50, 60, 70, 80};

    auto result = checked_mul_vec<ThrowOnErrorPolicy>(a, b);

    FATP_ASSERT_EQ(result.size(), a.size(), "Result size mismatch");
    for (size_t i = 0; i < a.size(); ++i)
    {
        FATP_ASSERT_EQ(result[i], a[i] * b[i], "Element mismatch at index");
    }

    std::cout << colors::green() << "  Mul unsigned uint32: PASSED" << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(intsimd_mul_unsigned_overflow_u32)
{
    std::cout << colors::cyan() << "\n[Integer SIMD] Mul unsigned overflow uint32..." << colors::reset() << std::endl;

    // 70000 * 70000 = 4,900,000,000 > UINT32_MAX (4,294,967,295)
    std::vector<uint32_t> a = {1, 70000, 3, 4};
    std::vector<uint32_t> b = {2, 70000, 6, 8};

    auto result = checked_mul_vec<ReturnExpectedPolicy>(a, b);

    FATP_ASSERT_TRUE(!result.has_value(), "Should detect overflow");
    FATP_ASSERT_EQ(result.error(), MathError::Overflow, "Should be overflow error");

    std::cout << colors::green() << "  Mul unsigned overflow uint32: PASSED" << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(intsimd_mul_large_vectors)
{
    std::cout << colors::cyan() << "\n[Integer SIMD] Mul large vectors..." << colors::reset() << std::endl;

    const size_t n = 1000;
    std::vector<int32_t> a(n);
    std::vector<int32_t> b(n);

    for (size_t i = 0; i < n; ++i)
    {
        a[i] = static_cast<int32_t>(i + 1);
        b[i] = 2;
    }

    auto result = checked_mul_vec<ThrowOnErrorPolicy>(a, b);

    FATP_ASSERT_EQ(result.size(), n, "Result size mismatch");
    for (size_t i = 0; i < n; ++i)
    {
        FATP_ASSERT_EQ(result[i], static_cast<int32_t>((i + 1) * 2), "Element mismatch at index");
    }

    std::cout << colors::green() << "  Mul large vectors: PASSED" << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// TEST EXTENSIONS: int64 SIMD, Edge Cases, Consistency Tests
// =============================================================================

FATP_TEST_CASE(intsimd_add_i64)
{
    std::cout << colors::cyan() << "\n[Integer SIMD] Add int64 (AVX2/NEON path)..." << colors::reset() << std::endl;

    std::vector<int64_t> a = {1, 2, 3, 4, 5, 6, 7, 8};
    std::vector<int64_t> b = {10, 20, 30, 40, 50, 60, 70, 80};

    auto result = checked_add_vec<ThrowOnErrorPolicy>(a, b);

    FATP_ASSERT_EQ(result.size(), a.size(), "Result size mismatch");
    for (size_t i = 0; i < a.size(); ++i)
    {
        FATP_ASSERT_EQ(result[i], a[i] + b[i], "Element mismatch at index");
    }

    // Test overflow detection
    std::vector<int64_t> c = {1, std::numeric_limits<int64_t>::max(), 3, 4};
    std::vector<int64_t> d = {10, 1, 30, 40};

    auto overflow_result = checked_add_vec<ReturnExpectedPolicy>(c, d);
    FATP_ASSERT_TRUE(!overflow_result.has_value(), "Should detect int64 overflow");
    FATP_ASSERT_EQ(overflow_result.error(), MathError::Overflow, "Should be overflow error");

    std::cout << colors::green() << "  Add int64: PASSED" << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(intsimd_sub_i64)
{
    std::cout << colors::cyan() << "\n[Integer SIMD] Sub int64 (AVX2/NEON path)..." << colors::reset() << std::endl;

    std::vector<int64_t> a = {100, 200, 300, 400, 500, 600, 700, 800};
    std::vector<int64_t> b = {10, 20, 30, 40, 50, 60, 70, 80};

    auto result = checked_sub_vec<ThrowOnErrorPolicy>(a, b);

    FATP_ASSERT_EQ(result.size(), a.size(), "Result size mismatch");
    for (size_t i = 0; i < a.size(); ++i)
    {
        FATP_ASSERT_EQ(result[i], a[i] - b[i], "Element mismatch at index");
    }

    // Test underflow detection
    std::vector<int64_t> c = {1, std::numeric_limits<int64_t>::min(), 3, 4};
    std::vector<int64_t> d = {10, 1, 30, 40};

    auto underflow_result = checked_sub_vec<ReturnExpectedPolicy>(c, d);
    FATP_ASSERT_TRUE(!underflow_result.has_value(), "Should detect int64 underflow");
    FATP_ASSERT_EQ(underflow_result.error(), MathError::Underflow, "Should be underflow error");

    std::cout << colors::green() << "  Sub int64: PASSED" << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(intsimd_mul_i64_scalar)
{
    std::cout << colors::cyan() << "\n[Integer SIMD] Mul int64 (scalar fallback)..." << colors::reset() << std::endl;

    // int64 mul goes through scalar path (no efficient SIMD for __int128 check)
    std::vector<int64_t> a = {1, 2, 3, 4, 5, 6, 7, 8};
    std::vector<int64_t> b = {10, 20, 30, 40, 50, 60, 70, 80};

    auto result = checked_mul_vec<ThrowOnErrorPolicy>(a, b);

    FATP_ASSERT_EQ(result.size(), a.size(), "Result size mismatch");
    for (size_t i = 0; i < a.size(); ++i)
    {
        FATP_ASSERT_EQ(result[i], a[i] * b[i], "Element mismatch at index");
    }

    // Test overflow detection: sqrt(INT64_MAX) ≈ 3e9, so 4e9 * 4e9 overflows
    std::vector<int64_t> c = {1, 4000000000LL, 3, 4};
    std::vector<int64_t> d = {10, 4000000000LL, 30, 40};

    auto overflow_result = checked_mul_vec<ReturnExpectedPolicy>(c, d);
    FATP_ASSERT_TRUE(!overflow_result.has_value(), "Should detect int64 overflow");
    FATP_ASSERT_EQ(overflow_result.error(), MathError::Overflow, "Should be overflow error");

    // Saturating policy
    auto sat_result = checked_mul_vec<SaturatingPolicy>(c, d);
    FATP_ASSERT_EQ(sat_result[0], 10LL, "Normal element should be correct");
    FATP_ASSERT_EQ(sat_result[1], std::numeric_limits<int64_t>::max(), "Overflow should saturate to max");

    std::cout << colors::green() << "  Mul int64 (scalar): PASSED" << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(intsimd_mul_edge_cases)
{
    std::cout << colors::cyan() << "\n[Integer SIMD] Mul edge cases (INT_MIN * -1, etc.)..." << colors::reset()
              << std::endl;

    // INT_MIN * -1 = overflow (result would be INT_MAX + 1)
    {
        std::vector<int32_t> a = {1, std::numeric_limits<int32_t>::min(), 3, 4, 5, 6, 7, 8};
        std::vector<int32_t> b = {2, -1, 6, 8, 10, 12, 14, 16};

        auto result = checked_mul_vec<ReturnExpectedPolicy>(a, b);
        FATP_ASSERT_TRUE(!result.has_value(), "INT_MIN * -1 should overflow");
        FATP_ASSERT_EQ(result.error(), MathError::Overflow, "Should be overflow error");
    }

    // INT_MAX * 2 = overflow
    {
        std::vector<int32_t> a = {std::numeric_limits<int32_t>::max(), 1, 2, 3};
        std::vector<int32_t> b = {2, 1, 2, 3};

        auto result = checked_mul_vec<ReturnExpectedPolicy>(a, b);
        FATP_ASSERT_TRUE(!result.has_value(), "INT_MAX * 2 should overflow");
    }

    // Mixed signs: large negative * large positive
    // Note: Unlike add/sub, multiplication overflow is always MathError::Overflow
    // regardless of sign. SaturatingPolicy does distinguish (clamps to min or max).
    {
        std::vector<int32_t> a = {1, -50000, 3, 4, 5, 6, 7, 8};
        std::vector<int32_t> b = {2, 50000, 6, 8, 10, 12, 14, 16};

        auto result = checked_mul_vec<ReturnExpectedPolicy>(a, b);
        FATP_ASSERT_TRUE(!result.has_value(), "-50000 * 50000 should overflow");
        FATP_ASSERT_EQ(result.error(), MathError::Overflow, "Mul overflow is always Overflow");
    }

    // Zero multiplication (should never overflow)
    {
        std::vector<int32_t> a = {0, std::numeric_limits<int32_t>::max(), 0, std::numeric_limits<int32_t>::min()};
        std::vector<int32_t> b = {std::numeric_limits<int32_t>::max(), 0, std::numeric_limits<int32_t>::min(), 0};

        auto result = checked_mul_vec<ThrowOnErrorPolicy>(a, b);
        FATP_ASSERT_EQ(result.size(), a.size(), "Result size mismatch");
        for (size_t i = 0; i < a.size(); ++i)
        {
            FATP_ASSERT_EQ(result[i], 0, "Zero * anything should be 0");
        }
    }

    // Saturating policy edge cases
    {
        std::vector<int32_t> a = {std::numeric_limits<int32_t>::min(),
                                  std::numeric_limits<int32_t>::max(),
                                  -50000,
                                  50000};
        std::vector<int32_t> b = {-1, 2, 50000, 50000};

        auto result = checked_mul_vec<SaturatingPolicy>(a, b);
        FATP_ASSERT_EQ(result[0], std::numeric_limits<int32_t>::max(), "INT_MIN * -1 saturates to max");
        FATP_ASSERT_EQ(result[1], std::numeric_limits<int32_t>::max(), "INT_MAX * 2 saturates to max");
        FATP_ASSERT_EQ(result[2], std::numeric_limits<int32_t>::min(), "-50000 * 50000 saturates to min");
        FATP_ASSERT_EQ(result[3], std::numeric_limits<int32_t>::max(), "50000 * 50000 saturates to max");
    }

    std::cout << colors::green() << "  Mul edge cases: PASSED" << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(intsimd_unsigned_mul_patterns)
{
    std::cout << colors::cyan() << "\n[Integer SIMD] Unsigned mul overflow patterns..." << colors::reset() << std::endl;

    // UINT32_MAX / 2 * 3 = overflow (result ~6.4 billion > 4.29 billion)
    {
        uint32_t half_max = std::numeric_limits<uint32_t>::max() / 2;
        std::vector<uint32_t> a = {1, half_max, 3, 4, 5, 6, 7, 8};
        std::vector<uint32_t> b = {2, 3, 6, 8, 10, 12, 14, 16};

        auto result = checked_mul_vec<ReturnExpectedPolicy>(a, b);
        FATP_ASSERT_TRUE(!result.has_value(), "UINT_MAX/2 * 3 should overflow");
        FATP_ASSERT_EQ(result.error(), MathError::Overflow, "Should be overflow error");
    }

    // Boundary: exactly UINT32_MAX (no overflow)
    {
        // 65535 * 65537 = 4294967295 = UINT32_MAX (exactly fits)
        std::vector<uint32_t> a = {65535, 1, 2, 3};
        std::vector<uint32_t> b = {65537, 1, 2, 3};

        auto result = checked_mul_vec<ThrowOnErrorPolicy>(a, b);
        FATP_ASSERT_EQ(result[0], std::numeric_limits<uint32_t>::max(), "65535 * 65537 should equal UINT32_MAX");
    }

    // Just over boundary: UINT32_MAX + 1 (overflow)
    {
        // 65536 * 65536 = 4294967296 > UINT32_MAX
        std::vector<uint32_t> a = {1, 65536, 3, 4};
        std::vector<uint32_t> b = {2, 65536, 6, 8};

        auto result = checked_mul_vec<ReturnExpectedPolicy>(a, b);
        FATP_ASSERT_TRUE(!result.has_value(), "65536 * 65536 should overflow");
    }

    // Saturating policy
    {
        std::vector<uint32_t> a = {65536, std::numeric_limits<uint32_t>::max()};
        std::vector<uint32_t> b = {65536, 2};

        auto result = checked_mul_vec<SaturatingPolicy>(a, b);
        FATP_ASSERT_EQ(result[0], std::numeric_limits<uint32_t>::max(), "Unsigned overflow saturates to max");
        FATP_ASSERT_EQ(result[1], std::numeric_limits<uint32_t>::max(), "UINT_MAX * 2 saturates to max");
    }

    std::cout << colors::green() << "  Unsigned mul patterns: PASSED" << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(intsimd_scalar_consistency)
{
    std::cout << colors::cyan() << "\n[Integer SIMD] SIMD vs Scalar consistency..." << colors::reset() << std::endl;

    // Generate test data with mix of normal and edge values
    std::vector<int32_t> a_i32(256);
    std::vector<int32_t> b_i32(256);

    for (size_t i = 0; i < 256; ++i)
    {
        a_i32[i] = static_cast<int32_t>(i * 1000 - 128000);
        b_i32[i] = static_cast<int32_t>(i * 500 - 64000);
    }

    // Inject some edge cases
    a_i32[10] = std::numeric_limits<int32_t>::max();
    b_i32[10] = 1; // Overflow for add
    a_i32[20] = std::numeric_limits<int32_t>::min();
    b_i32[20] = -1; // Underflow for sub
    a_i32[30] = 50000;
    b_i32[30] = 50000; // Overflow for mul

    // Test ADD: Compare SIMD result with scalar for non-overflow cases
    {
        auto vec_result = checked_add_vec<SaturatingPolicy>(a_i32, b_i32);

        for (size_t i = 0; i < a_i32.size(); ++i)
        {
            auto scalar_result = checked_add<SaturatingPolicy>(a_i32[i], b_i32[i]);
            FATP_ASSERT_EQ(vec_result[i], scalar_result, "Add: SIMD/scalar mismatch at index");
        }
    }

    // Test SUB
    {
        auto vec_result = checked_sub_vec<SaturatingPolicy>(a_i32, b_i32);

        for (size_t i = 0; i < a_i32.size(); ++i)
        {
            auto scalar_result = checked_sub<SaturatingPolicy>(a_i32[i], b_i32[i]);
            FATP_ASSERT_EQ(vec_result[i], scalar_result, "Sub: SIMD/scalar mismatch at index");
        }
    }

    // Test MUL (with smaller values to avoid most overflows)
    {
        std::vector<int32_t> a_small(256);
        std::vector<int32_t> b_small(256);
        for (size_t i = 0; i < 256; ++i)
        {
            a_small[i] = static_cast<int32_t>(i - 128);
            b_small[i] = static_cast<int32_t>((i % 100) - 50);
        }
        // Add overflow case
        a_small[100] = 50000;
        b_small[100] = 50000;

        auto vec_result = checked_mul_vec<SaturatingPolicy>(a_small, b_small);

        for (size_t i = 0; i < a_small.size(); ++i)
        {
            auto scalar_result = checked_mul<SaturatingPolicy>(a_small[i], b_small[i]);
            FATP_ASSERT_EQ(vec_result[i], scalar_result, "Mul: SIMD/scalar mismatch at index");
        }
    }

    // Test ReturnExpectedPolicy error detection consistency
    {
        std::vector<int32_t> a_err = {1, std::numeric_limits<int32_t>::max(), 3, 4, 5, 6, 7, 8};
        std::vector<int32_t> b_err = {1, 1, 1, 1, 1, 1, 1, 1};

        auto vec_result = checked_add_vec<ReturnExpectedPolicy>(a_err, b_err);
        FATP_ASSERT_TRUE(!vec_result.has_value(), "Vector should detect overflow");

        // Find which element overflows in scalar
        MathError expected_error = MathError::Overflow;
        for (size_t i = 0; i < a_err.size(); ++i)
        {
            auto scalar = checked_add<ReturnExpectedPolicy>(a_err[i], b_err[i]);
            if (!scalar.has_value())
            {
                expected_error = scalar.error();
                break;
            }
        }
        FATP_ASSERT_EQ(vec_result.error(), expected_error, "Error type should match scalar");
    }

    std::cout << colors::green() << "  SIMD vs Scalar consistency: PASSED" << colors::reset() << std::endl;
    return true;
}

FATP_TEST_CASE(intsimd_all_policies_consistency)
{
    std::cout << colors::cyan() << "\n[Integer SIMD] All policies produce consistent results..." << colors::reset()
              << std::endl;

    // Non-overflow case: all policies should produce same result
    {
        std::vector<int32_t> a = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
        std::vector<int32_t> b = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150, 160};

        auto throw_result = checked_add_vec<ThrowOnErrorPolicy>(a, b);
        auto expected_result = checked_add_vec<ReturnExpectedPolicy>(a, b);
        auto saturating_result = checked_add_vec<SaturatingPolicy>(a, b);

        FATP_ASSERT_TRUE(expected_result.has_value(), "Expected should succeed");

        for (size_t i = 0; i < a.size(); ++i)
        {
            FATP_ASSERT_EQ(throw_result[i], expected_result.value()[i], "Throw vs Expected mismatch");
            FATP_ASSERT_EQ(throw_result[i], saturating_result[i], "Throw vs Saturating mismatch");
        }
    }

    // Overflow case: Saturating should differ, Expected should fail
    {
        std::vector<int32_t> a = {1, std::numeric_limits<int32_t>::max(), 3, 4};
        std::vector<int32_t> b = {1, 1, 1, 1};

        auto expected_result = checked_add_vec<ReturnExpectedPolicy>(a, b);
        auto saturating_result = checked_add_vec<SaturatingPolicy>(a, b);

        FATP_ASSERT_TRUE(!expected_result.has_value(), "Expected should fail on overflow");
        FATP_ASSERT_EQ(saturating_result[1], std::numeric_limits<int32_t>::max(), "Saturating should clamp to max");
    }

    std::cout << colors::green() << "  All policies consistency: PASSED" << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// COMPREHENSIVE PERFORMANCE BENCHMARKS (New)
// =============================================================================


} // namespace checkedarithmetic

// =============================================================================
// Benchmark Functions
// =============================================================================

void run_checked_arithmetic_benchmarks()
{
    auto& out = *get_test_config().output;
    out << "\n"
        << colors::cyan() << colors::bold() << "=== CheckedArithmetic Performance Benchmarks ===" << colors::reset()
        << "\n\n";

    int32_t a = 12345, b = 6789;
    double fa = 1234.5, fb = 678.9;

    out << colors::blue() << "--- Integer Scalar Operations ---" << colors::reset() << "\n";

    benchmark("checked_add (int32)", [&]() {
        volatile int32_t r = checked_add<ThrowOnErrorPolicy>(a, b);
        (void)r;
    });

    benchmark("checked_sub (int32)", [&]() {
        volatile int32_t r = checked_sub<ThrowOnErrorPolicy>(a, b);
        (void)r;
    });

    benchmark("checked_mul (int32)", [&]() {
        volatile int32_t r = checked_mul<ThrowOnErrorPolicy>(a, b);
        (void)r;
    });

    benchmark("checked_div (int32)", [&]() {
        volatile int32_t r = checked_div<ThrowOnErrorPolicy>(a, b);
        (void)r;
    });

    benchmark("checked_mod (int32)", [&]() {
        volatile int32_t r = checked_mod<ThrowOnErrorPolicy>(a, b);
        (void)r;
    });

    out << "\n" << colors::blue() << "--- Floating-Point Scalar Operations ---" << colors::reset() << "\n";

    benchmark("checked_add_fp (double)", [&]() {
        volatile double r = checked_add_fp<ThrowOnErrorPolicy>(fa, fb);
        (void)r;
    });

    benchmark("checked_sub_fp (double)", [&]() {
        volatile double r = checked_sub_fp<ThrowOnErrorPolicy>(fa, fb);
        (void)r;
    });

    benchmark("checked_mul_fp (double)", [&]() {
        volatile double r = checked_mul_fp<ThrowOnErrorPolicy>(fa, fb);
        (void)r;
    });

    benchmark("checked_div_fp (double)", [&]() {
        volatile double r = checked_div_fp<ThrowOnErrorPolicy>(fa, fb);
        (void)r;
    });

    out << "\n" << colors::blue() << "--- Bitwise Operations ---" << colors::reset() << "\n";

    benchmark("checked_and", [&]() {
        volatile int32_t r = checked_and<ThrowOnErrorPolicy>(a, b);
        (void)r;
    });

    benchmark("checked_or", [&]() {
        volatile int32_t r = checked_or<ThrowOnErrorPolicy>(a, b);
        (void)r;
    });

    benchmark("checked_xor", [&]() {
        volatile int32_t r = checked_xor<ThrowOnErrorPolicy>(a, b);
        (void)r;
    });

    benchmark("checked_left_shift", [&]() {
        volatile int32_t r = checked_left_shift<ThrowOnErrorPolicy>(a, 4);
        (void)r;
    });

    benchmark("checked_right_shift", [&]() {
        volatile int32_t r = checked_right_shift<ThrowOnErrorPolicy>(a, 4);
        (void)r;
    });

    out << "\n" << colors::blue() << "--- Checked Cast Operations ---" << colors::reset() << "\n";

    int64_t big_val = 12345;
    double dbl_val = 1234.5;

    benchmark("checked_cast int64->int32", [&]() {
        volatile int32_t r = checked_cast<int32_t, ThrowOnErrorPolicy>(big_val);
        (void)r;
    });

    benchmark("checked_cast int32->int64", [&]() {
        volatile int64_t r = checked_cast<int64_t, ThrowOnErrorPolicy>(a);
        (void)r;
    });

    benchmark("checked_cast double->int32", [&]() {
        volatile int32_t r = checked_cast<int32_t, ThrowOnErrorPolicy>(dbl_val);
        (void)r;
    });

    benchmark("checked_cast int32->double", [&]() {
        volatile double r = checked_cast<double, ThrowOnErrorPolicy>(a);
        (void)r;
    });

    out << "\n" << colors::blue() << "--- Vector Operations (1K elements) ---" << colors::reset() << "\n";

    std::vector<int32_t> vec_a(1000, 100);
    std::vector<int32_t> vec_b(1000, 200);
    std::vector<double> vec_fa(1000, 100.5);
    std::vector<double> vec_fb(1000, 200.5);

    benchmark(
        "checked_add_vec (int32)",
        [&]() {
            auto r = checked_add_vec<ThrowOnErrorPolicy>(vec_a, vec_b);
            volatile int32_t x = r[0];
            (void)x;
        },
        10000);

    benchmark(
        "checked_sub_vec (int32)",
        [&]() {
            auto r = checked_sub_vec<ThrowOnErrorPolicy>(vec_a, vec_b);
            volatile int32_t x = r[0];
            (void)x;
        },
        10000);

    benchmark(
        "checked_mul_vec (int32)",
        [&]() {
            auto r = checked_mul_vec<ThrowOnErrorPolicy>(vec_a, vec_b);
            volatile int32_t x = r[0];
            (void)x;
        },
        10000);

    benchmark(
        "checked_add_vec_fp (double)",
        [&]() {
            auto r = checked_add_vec_fp<ThrowOnErrorPolicy>(vec_fa, vec_fb);
            volatile double x = r[0];
            (void)x;
        },
        10000);

    benchmark(
        "checked_mul_vec_fp (double)",
        [&]() {
            auto r = checked_mul_vec_fp<ThrowOnErrorPolicy>(vec_fa, vec_fb);
            volatile double x = r[0];
            (void)x;
        },
        10000);

    benchmark(
        "checked_div_vec_fp (double)",
        [&]() {
            auto r = checked_div_vec_fp<ThrowOnErrorPolicy>(vec_fa, vec_fb);
            volatile double x = r[0];
            (void)x;
        },
        10000);

#ifdef __AVX2__
    out << colors::green() << "\n  [AVX2 ENABLED]" << colors::reset() << "\n";
#else
    out << colors::yellow() << "\n  [Scalar fallback - no AVX2]" << colors::reset() << "\n";
#endif

    out << "\n";
}

void run_simd_vs_scalar_benchmarks()
{
    auto& out = *get_test_config().output;
    out << "\n"
        << colors::cyan() << colors::bold() << "=== SIMD vs Scalar Performance Comparison ===" << colors::reset()
        << "\n\n";

    out << colors::yellow() << "Comparing SIMD-accelerated vector ops vs pure scalar loops.\n"
        << "Speedup = Scalar Time / SIMD Time\n"
        << colors::reset() << "\n";

    // Test data: 4096 elements (multiple of all SIMD widths)
    const size_t N = 4096;
    std::vector<int32_t> a_i32(N);
    std::vector<int32_t> b_i32(N);
    std::vector<int32_t> result_i32(N);

    for (size_t i = 0; i < N; ++i)
    {
        a_i32[i] = static_cast<int32_t>(i % 1000);
        b_i32[i] = static_cast<int32_t>((i * 7) % 1000);
    }

    out << colors::blue() << "--- ADD (int32, " << N << " elements) ---" << colors::reset() << "\n";

    // SIMD path (via checked_add_vec)
    double simd_add_time = measure_perf(
        [&]() {
            auto r = checked_add_vec<ThrowOnErrorPolicy>(a_i32, b_i32);
            DoNotOptimize(r.data());
        },
        5000,
        100);
    out << "  SIMD (checked_add_vec):  " << format_time(simd_add_time) << "\n";

    // Pure scalar loop
    double scalar_add_time = measure_perf(
        [&]() {
            for (size_t i = 0; i < N; ++i)
            {
                result_i32[i] = checked_add<ThrowOnErrorPolicy>(a_i32[i], b_i32[i]);
            }
            DoNotOptimize(result_i32.data());
        },
        5000,
        100);
    out << "  Scalar loop:             " << format_time(scalar_add_time) << "\n";
    out << colors::green() << "  Speedup: " << std::fixed << std::setprecision(2) << (scalar_add_time / simd_add_time)
        << "x" << colors::reset() << "\n";

    out << "\n" << colors::blue() << "--- SUB (int32, " << N << " elements) ---" << colors::reset() << "\n";

    double simd_sub_time = measure_perf(
        [&]() {
            auto r = checked_sub_vec<ThrowOnErrorPolicy>(a_i32, b_i32);
            DoNotOptimize(r.data());
        },
        5000,
        100);
    out << "  SIMD (checked_sub_vec):  " << format_time(simd_sub_time) << "\n";

    double scalar_sub_time = measure_perf(
        [&]() {
            for (size_t i = 0; i < N; ++i)
            {
                result_i32[i] = checked_sub<ThrowOnErrorPolicy>(a_i32[i], b_i32[i]);
            }
            DoNotOptimize(result_i32.data());
        },
        5000,
        100);
    out << "  Scalar loop:             " << format_time(scalar_sub_time) << "\n";
    out << colors::green() << "  Speedup: " << std::fixed << std::setprecision(2) << (scalar_sub_time / simd_sub_time)
        << "x" << colors::reset() << "\n";

    out << "\n" << colors::blue() << "--- MUL (int32, " << N << " elements) ---" << colors::reset() << "\n";

    double simd_mul_time = measure_perf(
        [&]() {
            auto r = checked_mul_vec<ThrowOnErrorPolicy>(a_i32, b_i32);
            DoNotOptimize(r.data());
        },
        5000,
        100);
    out << "  SIMD (checked_mul_vec):  " << format_time(simd_mul_time) << "\n";

    double scalar_mul_time = measure_perf(
        [&]() {
            for (size_t i = 0; i < N; ++i)
            {
                result_i32[i] = checked_mul<ThrowOnErrorPolicy>(a_i32[i], b_i32[i]);
            }
            DoNotOptimize(result_i32.data());
        },
        5000,
        100);
    out << "  Scalar loop:             " << format_time(scalar_mul_time) << "\n";
    out << colors::green() << "  Speedup: " << std::fixed << std::setprecision(2) << (scalar_mul_time / simd_mul_time)
        << "x" << colors::reset() << "\n";

    // Per-element timing (measure_perf returns ms, convert to ns: ms * 1e6 = ns)
    out << "\n" << colors::blue() << "--- Per-Element Timing ---" << colors::reset() << "\n";
    double add_ns_per_elem = (simd_add_time * 1000000.0) / static_cast<double>(N);
    double sub_ns_per_elem = (simd_sub_time * 1000000.0) / static_cast<double>(N);
    double mul_ns_per_elem = (simd_mul_time * 1000000.0) / static_cast<double>(N);
    out << "  Add: " << std::fixed << std::setprecision(3) << add_ns_per_elem << " ns/element (SIMD)\n";
    out << "  Sub: " << std::fixed << std::setprecision(3) << sub_ns_per_elem << " ns/element (SIMD)\n";
    out << "  Mul: " << std::fixed << std::setprecision(3) << mul_ns_per_elem << " ns/element (SIMD)\n";

    out << "\n" << colors::blue() << "--- Architecture Info ---" << colors::reset() << "\n";
    out << "  Backend: " << int_simd::IntSimdInfo::architecture() << "\n";
    out << "  int32 width: " << int_simd::IntSimdInfo::int32_width() << " lanes\n";
    out << "  int64 width: " << int_simd::IntSimdInfo::int64_width() << " lanes\n";
    out << "  Saturating HW: " << (int_simd::IntSimdInfo::has_saturating_hardware() ? "yes" : "no") << "\n";

    out << "\n";
}

void run_policy_comparison_benchmarks()
{
    auto& out = *get_test_config().output;
    out << "\n"
        << colors::cyan() << colors::bold() << "=== Policy Overhead Comparison ===" << colors::reset() << "\n\n";

    out << colors::yellow() << "Comparing overhead of different error handling policies.\n"
        << "Lower times indicate less overhead.\n"
        << colors::reset() << "\n";

    int32_t a = 12345, b = 6789;

    out << colors::blue() << "--- Addition Policy Comparison ---" << colors::reset() << "\n";

    benchmark("ThrowOnErrorPolicy", [&]() {
        volatile int32_t r = checked_add<ThrowOnErrorPolicy>(a, b);
        (void)r;
    });

    benchmark("ReturnExpectedPolicy", [&]() {
        auto r = checked_add<ReturnExpectedPolicy>(a, b);
        volatile int32_t x = *r;
        (void)x;
    });

    benchmark("SaturatingPolicy", [&]() {
        volatile int32_t r = checked_add<SaturatingPolicy>(a, b);
        (void)r;
    });

    out << "\n" << colors::blue() << "--- Multiplication Policy Comparison ---" << colors::reset() << "\n";

    benchmark("ThrowOnErrorPolicy", [&]() {
        volatile int32_t r = checked_mul<ThrowOnErrorPolicy>(a, b);
        (void)r;
    });

    benchmark("ReturnExpectedPolicy", [&]() {
        auto r = checked_mul<ReturnExpectedPolicy>(a, b);
        volatile int32_t x = *r;
        (void)x;
    });

    benchmark("SaturatingPolicy", [&]() {
        volatile int32_t r = checked_mul<SaturatingPolicy>(a, b);
        (void)r;
    });

    out << "\n" << colors::blue() << "--- Division Policy Comparison ---" << colors::reset() << "\n";

    benchmark("ThrowOnErrorPolicy", [&]() {
        volatile int32_t r = checked_div<ThrowOnErrorPolicy>(a, b);
        (void)r;
    });

    benchmark("ReturnExpectedPolicy", [&]() {
        auto r = checked_div<ReturnExpectedPolicy>(a, b);
        volatile int32_t x = *r;
        (void)x;
    });

    benchmark("SaturatingPolicy", [&]() {
        volatile int32_t r = checked_div<SaturatingPolicy>(a, b);
        (void)r;
    });

    out << "\n" << colors::blue() << "--- FP Policy Comparison (with InfTolerant) ---" << colors::reset() << "\n";

    double fa = 1234.5, fb = 678.9;

    benchmark("ThrowOnErrorPolicy", [&]() {
        volatile double r = checked_mul_fp<ThrowOnErrorPolicy>(fa, fb);
        (void)r;
    });

    benchmark("ReturnExpectedPolicy", [&]() {
        auto r = checked_mul_fp<ReturnExpectedPolicy>(fa, fb);
        volatile double x = *r;
        (void)x;
    });

    benchmark("SaturatingPolicy", [&]() {
        volatile double r = checked_mul_fp<SaturatingPolicy>(fa, fb);
        (void)r;
    });

    benchmark("InfTolerantPolicy", [&]() {
        volatile double r = checked_mul_fp<InfTolerantPolicy>(fa, fb);
        (void)r;
    });

    out << "\n";
}

void run_raw_vs_checked_benchmarks()
{
    auto& out = *get_test_config().output;
    out << "\n"
        << colors::cyan() << colors::bold() << "=== Raw vs Checked - Overhead Measurement ===" << colors::reset()
        << "\n\n";

    out << colors::yellow() << "Comparing raw operations vs checked operations.\n"
        << "This shows the safety overhead of overflow detection.\n"
        << colors::reset() << "\n";

    int32_t a = 12345, b = 6789;
    double fa = 1234.5, fb = 678.9;

    out << colors::blue() << "--- Integer Addition ---" << colors::reset() << "\n";

    benchmark("Raw int add", [&]() {
        volatile int32_t r = a + b;
        (void)r;
    });

    benchmark("checked_add", [&]() {
        volatile int32_t r = checked_add<ThrowOnErrorPolicy>(a, b);
        (void)r;
    });

    out << "\n" << colors::blue() << "--- Integer Multiplication ---" << colors::reset() << "\n";

    benchmark("Raw int mul", [&]() {
        volatile int32_t r = a * b;
        (void)r;
    });

    benchmark("checked_mul", [&]() {
        volatile int32_t r = checked_mul<ThrowOnErrorPolicy>(a, b);
        (void)r;
    });

    out << "\n" << colors::blue() << "--- Integer Division ---" << colors::reset() << "\n";

    benchmark("Raw int div", [&]() {
        volatile int32_t r = a / b;
        (void)r;
    });

    benchmark("checked_div", [&]() {
        volatile int32_t r = checked_div<ThrowOnErrorPolicy>(a, b);
        (void)r;
    });

    out << "\n" << colors::blue() << "--- FP Addition ---" << colors::reset() << "\n";

    benchmark("Raw double add", [&]() {
        volatile double r = fa + fb;
        (void)r;
    });

    benchmark("checked_add_fp", [&]() {
        volatile double r = checked_add_fp<ThrowOnErrorPolicy>(fa, fb);
        (void)r;
    });

    out << "\n" << colors::blue() << "--- FP Multiplication ---" << colors::reset() << "\n";

    benchmark("Raw double mul", [&]() {
        volatile double r = fa * fb;
        (void)r;
    });

    benchmark("checked_mul_fp", [&]() {
        volatile double r = checked_mul_fp<ThrowOnErrorPolicy>(fa, fb);
        (void)r;
    });

    out << "\n" << colors::blue() << "--- Vector Addition (1K elements) ---" << colors::reset() << "\n";

    std::vector<int32_t> vec_a(1000, 100);
    std::vector<int32_t> vec_b(1000, 200);

    benchmark(
        "Raw vector add loop",
        [&]() {
            std::vector<int32_t> result(1000);
            for (size_t i = 0; i < 1000; ++i)
            {
                result[i] = vec_a[i] + vec_b[i];
            }
            volatile int32_t x = result[0];
            (void)x;
        },
        10000);

    benchmark(
        "checked_add_vec",
        [&]() {
            auto r = checked_add_vec<ThrowOnErrorPolicy>(vec_a, vec_b);
            volatile int32_t x = r[0];
            (void)x;
        },
        10000);

    out << "\n" << colors::cyan() << colors::bold() << "--- Interpretation ---" << colors::reset() << "\n\n";

    out << "1. " << colors::green() << "Scalar operations" << colors::reset()
        << ": Checked adds ~1-5ns overhead for safety.\n\n"
        << "2. " << colors::yellow() << "Multiplication/Division" << colors::reset()
        << ": Higher overhead due to complex overflow checks.\n\n"
        << "3. " << colors::green() << "Vector operations" << colors::reset()
        << ": SIMD amortizes per-element checking cost.\n\n"
        << "4. Use " << colors::cyan() << "SaturatingPolicy" << colors::reset()
        << " for lowest overhead when errors are recoverable.\n\n";
}

void run_vector_scaling_benchmarks()
{
    auto& out = *get_test_config().output;
    out << "\n" << colors::cyan() << colors::bold() << "=== Vector Size Scaling ===" << colors::reset() << "\n\n";

    out << colors::yellow() << "Showing how SIMD benefits scale with vector size.\n" << colors::reset() << "\n";

    std::vector<size_t> sizes = {64, 256, 1024, 4096, 16384};

    for (size_t sz : sizes)
    {
        std::vector<int32_t> va(sz, 100);
        std::vector<int32_t> vb(sz, 200);

        std::string name = "checked_add_vec (" + std::to_string(sz) + " elems)";
        benchmark(
            name.c_str(),
            [&]() {
                auto r = checked_add_vec<ThrowOnErrorPolicy>(va, vb);
                volatile int32_t x = r[0];
                (void)x;
            },
            1000);
    }

    out << "\n" << colors::blue() << "--- FP Vector Scaling ---" << colors::reset() << "\n";

    for (size_t sz : sizes)
    {
        std::vector<double> va(sz, 100.5);
        std::vector<double> vb(sz, 200.5);

        std::string name = "checked_mul_vec_fp (" + std::to_string(sz) + " elems)";
        benchmark(
            name.c_str(),
            [&]() {
                auto r = checked_mul_vec_fp<ThrowOnErrorPolicy>(va, vb);
                volatile double x = r[0];
                (void)x;
            },
            1000);
    }

    out << "\n";
}

// =============================================================================
// MAIN TEST RUNNER
// =============================================================================


bool test_CheckedArithmetic()
{
    FATP_PRINT_HEADER(CHECKED ARITHMETIC)

    TestRunner runner;

    auto& config = get_test_config();
    config.verbose = true;

    auto& out = *config.output;

    // Critical fix tests
    out << colors::blue() << "--- Critical Fix Tests ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, checkedarithmetic, mul_type_mismatch);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, div_sign_aware_saturation);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, fp_input_validation);

    // Enhancement tests
    out << "\n" << colors::blue() << "--- Enhancement Tests ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, checkedarithmetic, noexcept_specifications);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, type_safe_shifts);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, static_math_mod);

    // New feature tests
    out << "\n" << colors::blue() << "--- New Feature Tests ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, checkedarithmetic, checked_abs);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, shift_inf_tolerant);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, scalar_int_inf_tolerant);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, inf_tolerant_fp_nan_handling);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, scalar_fp_inf_tolerant);

    // Edge case tests
    out << "\n" << colors::blue() << "--- Edge Case Tests ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, checkedarithmetic, fp_denormals);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, unsigned_overflow);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, mixed_sign_operations);

    // SIMD tests
    out << "\n" << colors::blue() << "--- SIMD Validation Tests ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, checkedarithmetic, simd_int32_correctness);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, simd_overflow_detection);

    // FP vector tests
    out << "\n" << colors::blue() << "--- FP Vector Operation Tests ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, checkedarithmetic, fp_vec_sub_nan_detection);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, fp_vec_sub_inf_overflow);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, fp_vec_sub_inf_tolerant);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, fp_vec_mul_nan_detection);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, fp_vec_mul_inf_overflow);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, fp_vec_mul_mixed_overflow);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, fp_vec_mul_inf_tolerant);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, fp_vec_div_nan_detection);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, fp_vec_div_inf_overflow);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, fp_vec_div_by_zero);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, fp_vec_div_inf_tolerant);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, fp_vec_simd_consistency);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, fp_vec_boundary_detection);

    // SimdVector integration tests
    out << "\n" << colors::blue() << "--- SimdVector Integration Tests ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, checkedarithmetic, simd_width_detection);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, simd_tail_processing);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, simd_inf_handling);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, simd_overflow_to_inf);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, simd_float_type);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, simd_all_ops);

    // FP vector error path tests
    out << "\n" << colors::blue() << "--- FP Vector Error Path Tests ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, checkedarithmetic, fp_vec_throw_on_overflow);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, fp_vec_inf_inf_fallback);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, fp_vec_inf_tolerant_overflow);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, fp_vec_inf_tolerant_nan);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, fp_vec_inf_tolerant_inf_minus_inf);

    // Performance validation test
    out << "\n" << colors::blue() << "--- Performance Validation Test ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, checkedarithmetic, performance_benchmarks);

    // Core functionality tests
    out << "\n" << colors::blue() << "--- Core Functionality Tests ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, checkedarithmetic, checked_add);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, checked_sub);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, checked_mul);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, checked_div);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, checked_fp_operations);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, checked_mod);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, checked_negate);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, bitwise_operations);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, clamp_and_range);

    // Checked cast tests
    out << "\n" << colors::blue() << "--- Checked Cast Tests ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, checkedarithmetic, checked_cast_basic);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, checked_cast_narrowing);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, checked_cast_sign_conversion);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, checked_cast_fp_to_int);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, checked_cast_fp_to_fp);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, static_checked_cast);

    // Integer SIMD acceleration tests
    out << "\n" << colors::blue() << "--- Integer SIMD Acceleration Tests ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, checkedarithmetic, intsimd_architecture);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, intsimd_basic_add_i32);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, intsimd_overflow_detection_i32);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, intsimd_saturating_add_i32);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, intsimd_unsigned_add_u32);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, intsimd_unsigned_overflow_u32);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, intsimd_tail_processing);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, intsimd_sub_i32);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, intsimd_large_vectors);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, intsimd_mul_basic_i32);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, intsimd_mul_overflow_i32);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, intsimd_mul_saturating_i32);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, intsimd_mul_unsigned_u32);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, intsimd_mul_unsigned_overflow_u32);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, intsimd_mul_large_vectors);

    // Test Extensions: int64, edge cases, consistency
    out << "\n" << colors::blue() << "--- Integer SIMD Test Extensions ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, checkedarithmetic, intsimd_add_i64);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, intsimd_sub_i64);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, intsimd_mul_i64_scalar);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, intsimd_mul_edge_cases);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, intsimd_unsigned_mul_patterns);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, intsimd_scalar_consistency);
    FATP_RUN_TEST_NS(runner, checkedarithmetic, intsimd_all_policies_consistency);

    // Comprehensive performance benchmarks (new)
    run_checked_arithmetic_benchmarks();
    run_simd_vs_scalar_benchmarks();
    run_policy_comparison_benchmarks();
    run_raw_vs_checked_benchmarks();
    run_vector_scaling_benchmarks();

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_CheckedArithmetic() ? 0 : 1;
}
#endif
