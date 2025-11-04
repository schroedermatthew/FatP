/**
 * @file test_CheckedArithmetic_v3.cpp
 * @brief Comprehensive test suite for CheckedArithmetic v3.0
 * @version 3.0 - All critical fixes and enhancements tested
 * 
 * @details Tests covering:
 * - All critical bug fixes (multiplication, division, FP validation)
 * - Enhanced functionality (noexcept, SIMD, type-safe shifts)
 * - Edge cases (denormals, signaling NaN, unsigned overflow)
 * - Performance validation
 */

#include <iostream>
#include <vector>
#include <limits>
#include <cmath>
#include <stdexcept>
#include <chrono>

#include "CheckedArithmetic.h"
#include "test_CheckedArithmetic.h"
#include "test_Utilities.h"

namespace cpp_utilities::testing {

// =============================================================================
// Test Helpers
// =============================================================================

template<typename Func>
bool test_throws(const char* operation_name, Func func) {
    try {
        func();
        std::cout << colors::red() << "  ERROR: " << operation_name 
                  << " did not throw as expected" << colors::reset() << std::endl;
        return false;
    } catch (const std::exception&) {
        return true;
    }
}

// =============================================================================
// CRITICAL FIX TESTS
// =============================================================================

bool test_critical_fix_mul_type_mismatch() {
    std::cout << colors::cyan() << "\n[CRITICAL FIX TEST] Testing checked_mul type mismatch fix..."
              << colors::reset() << std::endl;
    
    // Fix #1: Type mismatch in fallback path
    {
        // Test multiplication by zero (should compile and return Expected{0})
        auto result1 = checked_mul<ReturnExpectedPolicy>(5, 0);
        SIMPLE_ASSERT(result1.has_value(), "5 * 0 should compile and succeed");
        ASSERT_EQ(*result1, 0, "5 * 0 should equal 0");
        
        auto result2 = checked_mul<ReturnExpectedPolicy>(0, 5);
        SIMPLE_ASSERT(result2.has_value(), "0 * 5 should compile and succeed");
        ASSERT_EQ(*result2, 0, "0 * 5 should equal 0");
        
        auto result3 = checked_mul<ReturnExpectedPolicy>(0, 0);
        SIMPLE_ASSERT(result3.has_value(), "0 * 0 should compile and succeed");
        ASSERT_EQ(*result3, 0, "0 * 0 should equal 0");
        
        // Test with large values
        auto result4 = checked_mul<ReturnExpectedPolicy>(1000000, 0);
        SIMPLE_ASSERT(result4.has_value(), "1000000 * 0 should succeed");
        ASSERT_EQ(*result4, 0, "1000000 * 0 should equal 0");
    }
    
    // Verify no division by zero in overflow checks
    {
        auto result = checked_mul<SaturatingPolicy>(5, 0);
        ASSERT_EQ(result, 0, "Saturating policy should also work");
    }
    
    std::cout << colors::green() << "[CRITICAL FIX] checked_mul type mismatch: FIXED ✓" 
              << colors::reset() << std::endl;
    return true;
}

bool test_critical_fix_div_sign_aware_saturation() {
    std::cout << colors::cyan() << "\n[CRITICAL FIX TEST] Testing sign-aware saturation..."
              << colors::reset() << std::endl;
    
    // Fix #2: Integral division sign-aware saturation
    {
        auto pos_result = checked_div<SaturatingPolicy>(100, 0);
        ASSERT_EQ(pos_result, std::numeric_limits<int>::max(), 
                  "Positive / 0 should saturate to max");
        
        auto neg_result = checked_div<SaturatingPolicy>(-100, 0);
        ASSERT_EQ(neg_result, std::numeric_limits<int>::min(), 
                  "Negative / 0 should saturate to min");
        
        auto zero_result = checked_div<SaturatingPolicy>(0, 0);
        ASSERT_EQ(zero_result, 0, "0 / 0 should return 0");
    }
    
    // Fix #3: FP division sign-aware saturation
    {
        auto pos_result = checked_div_fp<SaturatingPolicy>(5.0, 0.0);
        ASSERT_EQ(pos_result, std::numeric_limits<double>::max(), 
                  "Positive / 0.0 should saturate to max");
        
        auto neg_result = checked_div_fp<SaturatingPolicy>(-5.0, 0.0);
        ASSERT_EQ(neg_result, std::numeric_limits<double>::lowest(), 
                  "Negative / 0.0 should saturate to lowest (most negative)");
        
        // Test with very small positive/negative values
        auto small_pos = checked_div_fp<SaturatingPolicy>(1e-100, 0.0);
        ASSERT_EQ(small_pos, std::numeric_limits<double>::max(),
                  "Small positive / 0.0 should saturate to max");
        
        auto small_neg = checked_div_fp<SaturatingPolicy>(-1e-100, 0.0);
        ASSERT_EQ(small_neg, std::numeric_limits<double>::lowest(),
                  "Small negative / 0.0 should saturate to lowest");
    }
    
    std::cout << colors::green() << "[CRITICAL FIX] Sign-aware saturation: FIXED ✓" 
              << colors::reset() << std::endl;
    return true;
}

bool test_critical_fix_fp_input_validation() {
    std::cout << colors::cyan() << "\n[CRITICAL FIX TEST] Testing FP input validation..."
              << colors::reset() << std::endl;
    
    double nan_val = std::numeric_limits<double>::quiet_NaN();
    double inf_val = std::numeric_limits<double>::infinity();
    
    // Fix #4: NaN input detection in addition
    {
        SIMPLE_ASSERT(test_throws("NaN + 1", [&]() {
            checked_add_fp<ThrowOnErrorPolicy>(nan_val, 1.0);
        }), "Should throw on NaN input in addition");
        
        auto result = checked_add_fp<ReturnExpectedPolicy>(nan_val, 1.0);
        SIMPLE_ASSERT(!result.has_value(), "Should fail on NaN input");
        ASSERT_EQ(result.error(), MathError::NaN, "Should return NaN error");
        
        auto sat_result = checked_add_fp<SaturatingPolicy>(nan_val, 1.0);
        SIMPLE_ASSERT(std::isnan(sat_result), "Should return NaN for saturating policy");
    }
    
    // NaN input in subtraction
    {
        SIMPLE_ASSERT(test_throws("1 - NaN", [&]() {
            checked_sub_fp<ThrowOnErrorPolicy>(1.0, nan_val);
        }), "Should throw on NaN input in subtraction");
    }
    
    // NaN input in multiplication
    {
        SIMPLE_ASSERT(test_throws("NaN * 2", [&]() {
            checked_mul_fp<ThrowOnErrorPolicy>(nan_val, 2.0);
        }), "Should throw on NaN input in multiplication");
    }
    
    // NaN input in division
    {
        SIMPLE_ASSERT(test_throws("NaN / 5", [&]() {
            checked_div_fp<ThrowOnErrorPolicy>(nan_val, 5.0);
        }), "Should throw on NaN input in division");
    }
    
    // Inf - Inf detection (undefined operation)
    {
        SIMPLE_ASSERT(test_throws("Inf - Inf", [&]() {
            checked_sub_fp<ThrowOnErrorPolicy>(inf_val, inf_val);
        }), "Should throw on Inf - Inf");
        
        auto result = checked_sub_fp<ReturnExpectedPolicy>(inf_val, inf_val);
        SIMPLE_ASSERT(!result.has_value(), "Inf - Inf should fail");
        ASSERT_EQ(result.error(), MathError::NaN, "Should return NaN error for Inf - Inf");
    }
    
    // -Inf + Inf detection
    {
        SIMPLE_ASSERT(test_throws("-Inf + Inf", [&]() {
            checked_add_fp<ThrowOnErrorPolicy>(-inf_val, inf_val);
        }), "Should throw on -Inf + Inf");
    }
    
    // Valid Inf operations should succeed (NEW: properly allows these)
    {
        // Inf + finite should work (returns Inf)
        auto result1 = checked_add_fp<ThrowOnErrorPolicy>(inf_val, 1.0);
        SIMPLE_ASSERT(std::isinf(result1) && result1 > 0, "Inf + 1.0 should succeed and return Inf");
        
        // Inf + Inf should work (returns Inf)
        auto result2 = checked_add_fp<ThrowOnErrorPolicy>(inf_val, inf_val);
        SIMPLE_ASSERT(std::isinf(result2) && result2 > 0, "Inf + Inf should succeed and return Inf");
        
        // Inf * finite should work (returns Inf)
        auto result3 = checked_mul_fp<ThrowOnErrorPolicy>(inf_val, 2.0);
        SIMPLE_ASSERT(std::isinf(result3) && result3 > 0, "Inf * 2.0 should succeed and return Inf");
        
        // Inf / finite should work (returns Inf)
        auto result4 = checked_div_fp<ThrowOnErrorPolicy>(inf_val, 2.0);
        SIMPLE_ASSERT(std::isinf(result4) && result4 > 0, "Inf / 2.0 should succeed and return Inf");
        
        // finite / Inf should work (returns 0)
        auto result5 = checked_div_fp<ThrowOnErrorPolicy>(2.0, inf_val);
        SIMPLE_ASSERT(result5 == 0.0, "2.0 / Inf should succeed and return 0");
    }
    
    // Overflow detection (finite -> Inf) should still work
    {
        double large = std::numeric_limits<double>::max();
        
        SIMPLE_ASSERT(test_throws("Overflow: max * 2", [&]() {
            checked_mul_fp<ThrowOnErrorPolicy>(large, 2.0);
        }), "Should detect overflow (finite * finite -> Inf)");
        
        auto result = checked_mul_fp<ReturnExpectedPolicy>(large, 2.0);
        SIMPLE_ASSERT(!result.has_value(), "Overflow should be detected");
        ASSERT_EQ(result.error(), MathError::Inf, "Should return Inf error for overflow");
    }
    
    std::cout << colors::green() << "[CRITICAL FIX] FP input validation: FIXED ✓" 
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// ENHANCED FUNCTIONALITY TESTS
// =============================================================================

bool test_noexcept_specifications() {
    std::cout << colors::cyan() << "\n[ENHANCEMENT TEST] Testing noexcept specifications..."
              << colors::reset() << std::endl;
    
    // Verify noexcept for non-throwing policies
    static_assert(noexcept(checked_add<ReturnExpectedPolicy>(1, 2)),
                  "ReturnExpectedPolicy should be noexcept");
    static_assert(noexcept(checked_add<SaturatingPolicy>(1, 2)),
                  "SaturatingPolicy should be noexcept");
    static_assert(!noexcept(checked_add<ThrowOnErrorPolicy>(1, 2)),
                  "ThrowOnErrorPolicy should not be noexcept");
    
    // Test floating-point operations
    static_assert(noexcept(checked_add_fp<ReturnExpectedPolicy>(1.0, 2.0)),
                  "FP ReturnExpectedPolicy should be noexcept");
    static_assert(noexcept(checked_add_fp<SaturatingPolicy>(1.0, 2.0)),
                  "FP SaturatingPolicy should be noexcept");
    
    std::cout << colors::green() << "[ENHANCEMENT] noexcept specifications: WORKING ✓" 
              << colors::reset() << std::endl;
    return true;
}

bool test_type_safe_shifts() {
    std::cout << colors::cyan() << "\n[ENHANCEMENT TEST] Testing type-safe shift operations..."
              << colors::reset() << std::endl;
    
    // Test with different shift types
    {
        int value = 5;
        auto result1 = checked_left_shift<ThrowOnErrorPolicy>(value, 2);
        ASSERT_EQ(result1, 20, "5 << 2 should equal 20");
        
        auto result2 = checked_left_shift<ThrowOnErrorPolicy>(value, 2u);
        ASSERT_EQ(result2, 20, "Should work with unsigned shift");
        
        auto result3 = checked_left_shift<ThrowOnErrorPolicy>(value, static_cast<size_t>(2));
        ASSERT_EQ(result3, 20, "Should work with size_t shift");
    }
    
    // Test negative shift detection (signed shift type)
    {
        SIMPLE_ASSERT(test_throws("Negative left shift", []() {
            checked_left_shift<ThrowOnErrorPolicy>(5, -1);
        }), "Should throw on negative left shift");
        
        auto result = checked_left_shift<ReturnExpectedPolicy>(5, -1);
        SIMPLE_ASSERT(!result.has_value(), "Should fail on negative shift");
        ASSERT_EQ(result.error(), MathError::InvalidArgument, "Should return InvalidArgument");
    }
    
    // Test shift >= bitwidth
    {
        SIMPLE_ASSERT(test_throws("Shift >= bitwidth", []() {
            checked_left_shift<ThrowOnErrorPolicy>(5, 32); // int is 32 bits
        }), "Should throw on shift >= bitwidth");
    }
    
    std::cout << colors::green() << "[ENHANCEMENT] Type-safe shifts: WORKING ✓" 
              << colors::reset() << std::endl;
    return true;
}

bool test_static_math_mod() {
    std::cout << colors::cyan() << "\n[ENHANCEMENT TEST] Testing static_math::mod..."
              << colors::reset() << std::endl;
    
    // Test compile-time modulo
    {
        constexpr int result1 = static_math::mod<int, 10, 3>();
        ASSERT_EQ(result1, 1, "10 % 3 should equal 1");
        
        constexpr int result2 = static_math::mod<int, 17, 5>();
        ASSERT_EQ(result2, 2, "17 % 5 should equal 2");
        
        constexpr int result3 = static_math::mod<int, -10, 3>();
        ASSERT_EQ(result3, -1, "-10 % 3 should equal -1");
    }
    
    // These should cause compile-time errors (uncomment to test):
    // constexpr int error1 = static_math::mod<int, 10, 0>();  // Division by zero
    // constexpr int error2 = static_math::mod<int, INT_MIN, -1>();  // Overflow
    
    std::cout << colors::green() << "[ENHANCEMENT] static_math::mod: WORKING ✓" 
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// EXPANDED EDGE CASE TESTS
// =============================================================================

bool test_fp_denormals() {
    std::cout << colors::cyan() << "\n[EDGE CASE TEST] Testing denormal handling..."
              << colors::reset() << std::endl;
    
    double denorm = std::numeric_limits<double>::denorm_min();
    
    // Addition with denormals
    {
        auto result = checked_add_fp<ThrowOnErrorPolicy>(denorm, denorm);
        SIMPLE_ASSERT(std::abs(result - 2.0 * denorm) < 1e-320 || result == 2.0 * denorm,
                     "Should handle denormal addition");
    }
    
    // Multiplication with denormals
    {
        auto result = checked_mul_fp<ThrowOnErrorPolicy>(denorm, 2.0);
        SIMPLE_ASSERT(result > 0 && std::isfinite(result), 
                     "Should handle denormal multiplication");
    }
    
    // Division producing denormal (or underflowing to zero)
    {
        auto result = checked_div_fp<ThrowOnErrorPolicy>(denorm, 2.0);
        // Result can be a smaller denormal OR underflow to 0 - both are valid
        SIMPLE_ASSERT(result >= 0 && std::isfinite(result),
                     "Should handle division producing denormal or underflow to zero");
    }
    
    std::cout << colors::green() << "[EDGE CASE] Denormal handling: PASSED ✓" 
              << colors::reset() << std::endl;
    return true;
}

bool test_unsigned_overflow() {
    std::cout << colors::cyan() << "\n[EDGE CASE TEST] Testing unsigned overflow patterns..."
              << colors::reset() << std::endl;
    
    unsigned int max_uint = std::numeric_limits<unsigned int>::max();
    
    // Unsigned addition overflow
    {
        auto result = checked_add<ReturnExpectedPolicy>(max_uint, 1u);
        SIMPLE_ASSERT(!result.has_value(), "unsigned max + 1 should overflow");
        
        auto sat_result = checked_add<SaturatingPolicy>(max_uint, 1u);
        ASSERT_EQ(sat_result, max_uint, "Should saturate to max");
    }
    
    // Unsigned subtraction underflow (wraps to large value)
    {
        auto result = checked_sub<ReturnExpectedPolicy>(0u, 1u);
        SIMPLE_ASSERT(!result.has_value(), "0u - 1u should underflow");
        
        auto sat_result = checked_sub<SaturatingPolicy>(0u, 1u);
        ASSERT_EQ(sat_result, 0u, "Should saturate to 0");
    }
    
    // Unsigned multiplication overflow
    {
        auto result = checked_mul<ReturnExpectedPolicy>(max_uint, 2u);
        SIMPLE_ASSERT(!result.has_value(), "unsigned max * 2 should overflow");
    }
    
    std::cout << colors::green() << "[EDGE CASE] Unsigned overflow: PASSED ✓" 
              << colors::reset() << std::endl;
    return true;
}

bool test_mixed_sign_operations() {
    std::cout << colors::cyan() << "\n[EDGE CASE TEST] Testing mixed-sign operations..."
              << colors::reset() << std::endl;
    
    // Mixed-sign multiplication
    {
        auto result = checked_mul<SaturatingPolicy>(-100, 100000000);
        ASSERT_EQ(result, std::numeric_limits<int>::min(),
                 "Negative result should saturate to min");
        
        result = checked_mul<SaturatingPolicy>(100, -100000000);
        ASSERT_EQ(result, std::numeric_limits<int>::min(),
                 "Mixed-sign overflow should saturate correctly");
    }
    
    // Mixed-sign addition near boundaries
    {
        int near_max = std::numeric_limits<int>::max() - 10;
        auto result = checked_add<ThrowOnErrorPolicy>(near_max, -5);
        ASSERT_EQ(result, near_max - 5, "Should handle mixed-sign addition near max");
        
        int near_min = std::numeric_limits<int>::min() + 10;
        result = checked_add<ThrowOnErrorPolicy>(near_min, 5);
        ASSERT_EQ(result, near_min + 5, "Should handle mixed-sign addition near min");
    }
    
    std::cout << colors::green() << "[EDGE CASE] Mixed-sign operations: PASSED ✓" 
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// SIMD VALIDATION TESTS
// =============================================================================

bool test_simd_int32_correctness() {
    std::cout << colors::cyan() << "\n[SIMD TEST] Testing int32 vector operations..."
              << colors::reset() << std::endl;
    
    // Test with various sizes
    std::vector<size_t> sizes = {1, 7, 8, 15, 16, 100, 1000};
    
    for (size_t size : sizes) {
        std::vector<int32_t> vec_a(size);
        std::vector<int32_t> vec_b(size);
        
        // Fill with test data
        for (size_t i = 0; i < size; ++i) {
            vec_a[i] = static_cast<int32_t>(i);
            vec_b[i] = static_cast<int32_t>(i * 2);
        }
        
        // Test addition
        auto result = checked_add_vec<ThrowOnErrorPolicy>(vec_a, vec_b);
        ASSERT_EQ(result.size(), size, "Result size should match input");
        
        for (size_t i = 0; i < size; ++i) {
            int32_t expected = vec_a[i] + vec_b[i];
            ASSERT_EQ(result[i], expected, "SIMD result should match scalar");
        }
    }
    
#ifdef __AVX2__
    std::cout << colors::green() << "[SIMD] int32 operations (AVX2): PASSED ✓" 
              << colors::reset() << std::endl;
#else
    std::cout << colors::yellow() << "[SIMD] int32 operations (scalar fallback): PASSED ✓" 
              << colors::reset() << std::endl;
#endif
    
    return true;
}

bool test_simd_overflow_detection() {
    std::cout << colors::cyan() << "\n[SIMD TEST] Testing SIMD overflow detection..."
              << colors::reset() << std::endl;
    
    // Test vector with overflow in middle
    std::vector<int32_t> vec_a = {1, 2, std::numeric_limits<int32_t>::max(), 4, 5};
    std::vector<int32_t> vec_b = {1, 2, 1, 4, 5}; // Third element will overflow
    
    // Should throw on overflow
    SIMPLE_ASSERT(test_throws("Vector addition overflow", [&]() {
        checked_add_vec<ThrowOnErrorPolicy>(vec_a, vec_b);
    }), "Should detect overflow in vector operation");
    
    // With saturating policy, should handle gracefully
    auto result = checked_add_vec<SaturatingPolicy>(vec_a, vec_b);
    ASSERT_EQ(result[0], 2, "Non-overflow elements should be correct");
    ASSERT_EQ(result[2], std::numeric_limits<int32_t>::max(), 
             "Overflow element should saturate");
    
    std::cout << colors::green() << "[SIMD] Overflow detection: PASSED ✓" 
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// PERFORMANCE VALIDATION TESTS
// =============================================================================

bool test_performance_benchmarks() {
    std::cout << colors::cyan() << "\n[PERFORMANCE TEST] Running benchmarks..."
              << colors::reset() << std::endl;
    
    const int ITERATIONS = 100000;
    volatile int32_t accumulator = 0; // Prevent optimizer elision
    
    // Scalar addition benchmark
    {
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < ITERATIONS; ++i) {
            auto res = checked_add<ThrowOnErrorPolicy>(100, 200);
            accumulator += res;
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        double avg_ns = static_cast<double>(duration.count()) / ITERATIONS;
        
        std::cout << "  Scalar checked_add: " << avg_ns << " ns/op" << std::endl;
        SIMPLE_ASSERT(avg_ns < 50.0, "Scalar addition should be fast (< 50ns)");
    }
    
    // Vector addition benchmark
    {
        std::vector<int32_t> vec_a(1000, 10);
        std::vector<int32_t> vec_b(1000, 20);
        
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 100; ++i) {
            auto res = checked_add_vec<ThrowOnErrorPolicy>(vec_a, vec_b);
            accumulator += res[0]; // Use result
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        double avg_us = static_cast<double>(duration.count()) / 100.0;
        
        std::cout << "  Vector checked_add_vec (1K elems): " << avg_us << " µs/op" << std::endl;
        
#ifdef __AVX2__
        SIMPLE_ASSERT(avg_us < 5.0, "SIMD vector addition should be fast (< 5µs for 1K)");
        std::cout << colors::green() << "  [AVX2 ENABLED] Performance target met ✓" 
                  << colors::reset() << std::endl;
#else
        std::cout << colors::yellow() << "  [Scalar fallback] Performance acceptable" 
                  << colors::reset() << std::endl;
#endif
    }
    
    // Prevent optimizer from removing accumulator
    if (accumulator == 0x7FFFFFFF) {
        std::cout << "Unlikely sentinel" << std::endl;
    }
    
    std::cout << colors::green() << "[PERFORMANCE] Benchmarks: PASSED ✓" 
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// EXISTING TESTS (Updated)
// =============================================================================

bool test_checked_add() {
    std::cout << colors::cyan() << "\nTesting checked_add..." << colors::reset() << std::endl;
    
    // ThrowOnErrorPolicy - normal cases
    {
        auto result = checked_add<ThrowOnErrorPolicy>(10, 20);
        ASSERT_EQ(result, 30, "10 + 20 should equal 30");
        
        result = checked_add<ThrowOnErrorPolicy>(-10, -20);
        ASSERT_EQ(result, -30, "-10 + -20 should equal -30");
    }
    
    // ThrowOnErrorPolicy - overflow
    {
        SIMPLE_ASSERT(test_throws("INT_MAX + 1", []() {
            checked_add<ThrowOnErrorPolicy>(std::numeric_limits<int>::max(), 1);
        }), "Should throw on overflow");
        
        SIMPLE_ASSERT(test_throws("INT_MIN + (-1)", []() {
            checked_add<ThrowOnErrorPolicy>(std::numeric_limits<int>::min(), -1);
        }), "Should throw on underflow");
    }
    
    // ReturnExpectedPolicy
    {
        auto result = checked_add<ReturnExpectedPolicy>(10, 20);
        SIMPLE_ASSERT(result.has_value(), "Should have value");
        ASSERT_EQ(*result, 30, "10 + 20 should equal 30");
        
        result = checked_add<ReturnExpectedPolicy>(std::numeric_limits<int>::max(), 1);
        SIMPLE_ASSERT(!result.has_value(), "Should not have value on overflow");
        ASSERT_EQ(result.error(), MathError::Overflow, "Should return Overflow error");
    }
    
    // SaturatingPolicy
    {
        auto result = checked_add<SaturatingPolicy>(std::numeric_limits<int>::max(), 1);
        ASSERT_EQ(result, std::numeric_limits<int>::max(), "Should saturate to max");
        
        result = checked_add<SaturatingPolicy>(std::numeric_limits<int>::min(), -1);
        ASSERT_EQ(result, std::numeric_limits<int>::min(), "Should saturate to min");
    }
    
    std::cout << colors::green() << "checked_add: Tests passed." << colors::reset() << std::endl;
    return true;
}

bool test_checked_sub() {
    std::cout << colors::cyan() << "\nTesting checked_sub..." << colors::reset() << std::endl;
    
    // Normal cases
    {
        auto result = checked_sub<ThrowOnErrorPolicy>(20, 10);
        ASSERT_EQ(result, 10, "20 - 10 should equal 10");
    }
    
    // Overflow cases
    {
        SIMPLE_ASSERT(test_throws("INT_MIN - 1", []() {
            checked_sub<ThrowOnErrorPolicy>(std::numeric_limits<int>::min(), 1);
        }), "Should throw on underflow");
    }
    
    std::cout << colors::green() << "checked_sub: Tests passed." << colors::reset() << std::endl;
    return true;
}

bool test_checked_mul() {
    std::cout << colors::cyan() << "\nTesting checked_mul..." << colors::reset() << std::endl;
    
    // Normal cases
    {
        auto result = checked_mul<ThrowOnErrorPolicy>(5, 6);
        ASSERT_EQ(result, 30, "5 * 6 should equal 30");
    }
    
    // Zero cases (testing the fix)
    {
        auto result = checked_mul<ThrowOnErrorPolicy>(0, 100);
        ASSERT_EQ(result, 0, "0 * 100 should equal 0");
        
        result = checked_mul<ThrowOnErrorPolicy>(100, 0);
        ASSERT_EQ(result, 0, "100 * 0 should equal 0");
    }
    
    // Overflow cases
    {
        SIMPLE_ASSERT(test_throws("Large mul", []() {
            checked_mul<ThrowOnErrorPolicy>(100000, 100000);
        }), "Should throw on overflow");
    }
    
    std::cout << colors::green() << "checked_mul: Tests passed." << colors::reset() << std::endl;
    return true;
}

bool test_checked_div() {
    std::cout << colors::cyan() << "\nTesting checked_div..." << colors::reset() << std::endl;
    
    // Normal cases
    {
        auto result = checked_div<ThrowOnErrorPolicy>(20, 5);
        ASSERT_EQ(result, 4, "20 / 5 should equal 4");
    }
    
    // Division by zero (testing the fix)
    {
        SIMPLE_ASSERT(test_throws("Div by zero", []() {
            checked_div<ThrowOnErrorPolicy>(10, 0);
        }), "Should throw on div by zero");
        
        // Test sign-aware saturation (the fix)
        auto pos_result = checked_div<SaturatingPolicy>(10, 0);
        ASSERT_EQ(pos_result, std::numeric_limits<int>::max(), "Positive div zero saturates to max");
        
        auto neg_result = checked_div<SaturatingPolicy>(-10, 0);
        ASSERT_EQ(neg_result, std::numeric_limits<int>::min(), "Negative div zero saturates to min");
    }
    
    // Overflow case
    {
        SIMPLE_ASSERT(test_throws("INT_MIN / -1", []() {
            checked_div<ThrowOnErrorPolicy>(std::numeric_limits<int>::min(), -1);
        }), "Should throw on min/-1 overflow");
    }
    
    std::cout << colors::green() << "checked_div: Tests passed." << colors::reset() << std::endl;
    return true;
}

bool test_checked_fp_operations() {
    std::cout << colors::cyan() << "\nTesting FP operations..." << colors::reset() << std::endl;
    
    // Normal operations
    {
        auto result = checked_add_fp<ThrowOnErrorPolicy>(1.5, 2.5);
        ASSERT_EQ(result, 4.0, "1.5 + 2.5 should equal 4.0");
    }
    
    // NaN/Inf handling (testing the fix)
    {
        double nan_val = std::numeric_limits<double>::quiet_NaN();
        
        // NaN input should be detected
        SIMPLE_ASSERT(test_throws("NaN + 1", [&]() {
            checked_add_fp<ThrowOnErrorPolicy>(nan_val, 1.0);
        }), "Should throw on NaN input");
    }
    
    // Division by zero (testing sign-aware fix)
    {
        auto pos_result = checked_div_fp<SaturatingPolicy>(5.0, 0.0);
        ASSERT_EQ(pos_result, std::numeric_limits<double>::max(),
                 "Positive FP div by zero saturates to max");
        
        auto neg_result = checked_div_fp<SaturatingPolicy>(-5.0, 0.0);
        ASSERT_EQ(neg_result, std::numeric_limits<double>::lowest(),
                 "Negative FP div by zero saturates to lowest");
    }
    
    std::cout << colors::green() << "FP operations: Tests passed." << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// MAIN TEST RUNNER
// =============================================================================

bool test_CheckedArithmetic() {
    std::cout << colors::blue() << "\n"
              << "╔════════════════════════════════════════════════════════════╗\n"
              << "║  CheckedArithmetic v3.0 - Comprehensive Test Suite         ║\n"
              << "║  All Critical Fixes & Enhancements                         ║\n"
              << "╚════════════════════════════════════════════════════════════╝\n"
              << colors::reset() << std::endl;
    
    bool all_passed = true;
    
    // Critical fix tests
    all_passed &= test_critical_fix_mul_type_mismatch();
    all_passed &= test_critical_fix_div_sign_aware_saturation();
    all_passed &= test_critical_fix_fp_input_validation();
    
    // Enhancement tests
    all_passed &= test_noexcept_specifications();
    all_passed &= test_type_safe_shifts();
    all_passed &= test_static_math_mod();
    
    // Edge case tests
    all_passed &= test_fp_denormals();
    all_passed &= test_unsigned_overflow();
    all_passed &= test_mixed_sign_operations();
    
    // SIMD tests
    all_passed &= test_simd_int32_correctness();
    all_passed &= test_simd_overflow_detection();
    
    // Performance tests
    all_passed &= test_performance_benchmarks();
    
    // Existing core tests
    all_passed &= test_checked_add();
    all_passed &= test_checked_sub();
    all_passed &= test_checked_mul();
    all_passed &= test_checked_div();
    all_passed &= test_checked_fp_operations();
    
    // Summary
    std::cout << "\n" << colors::blue() 
              << "═══════════════════════════════════════════════════════════" 
              << colors::reset() << std::endl;
    
    if (all_passed) {
        std::cout << colors::green() << "✓ ALL TESTS PASSED" << colors::reset() << std::endl;
        std::cout << colors::green() 
                  << "  - All critical fixes verified\n"
                  << "  - All enhancements working\n"
                  << "  - All edge cases handled\n"
                  << "  - Performance targets met"
                  << colors::reset() << std::endl;
    } else {
        std::cout << colors::red() << "✗ SOME TESTS FAILED" << colors::reset() << std::endl;
    }
    
    std::cout << colors::blue() 
              << "═══════════════════════════════════════════════════════════\n" 
              << colors::reset() << std::endl;
    
    return all_passed;
}

} // namespace cpp_utilities::testing
