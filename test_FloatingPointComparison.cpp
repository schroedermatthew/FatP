// test_FloatingPointComparison.cpp
// Lightweight test suite for FloatingPointComparison library
// Tests floating-point comparison policies in isolation (no container dependencies)

#include <iostream>
#include <iomanip>
#include <limits>
#include <cmath>

#include "FloatingPointComparison.h"
#include "test_FloatingPointComparison.h"
#include "test_Utilities.h"

namespace cpp_utilities::testing {

using namespace cpp_utilities;

// =============================================================================
// Test Suite 1: StandardComparisonPolicy
// =============================================================================

bool test_standard_basic() {

    // Exact equality
    ASSERT_TRUE(floatEqual(1.0, 1.0), "Exact equality");
    
    // Within default epsilon
    double eps = getDefaultEpsilon<double>();
    ASSERT_TRUE(floatEqual(1.0, 1.0 + eps * 0.5), "Within default epsilon");
    
    // Beyond default epsilon
    ASSERT_FALSE(floatEqual(1.0, 1.0 + eps * 2.0), "Beyond default epsilon");
    
    // Custom epsilon
    ASSERT_TRUE(floatEqual(1.0, 1.1, 0.2), "Custom epsilon 0.2");
    ASSERT_FALSE(floatEqual(1.0, 1.1, 0.05), "Custom epsilon 0.05");
    
    std::cout << colors::green() << "  ✓ Standard basic tests passed" 
              << colors::reset() << std::endl;
    
    return true;
}

bool test_standard_special_values() {
    
    double pos_inf = std::numeric_limits<double>::infinity();
    double neg_inf = -std::numeric_limits<double>::infinity();
    double nan = std::numeric_limits<double>::quiet_NaN();
    
    // Infinities
    ASSERT_TRUE(floatEqual(pos_inf, pos_inf), "+inf == +inf");
    ASSERT_TRUE(floatEqual(neg_inf, neg_inf), "-inf == -inf");
    ASSERT_FALSE(floatEqual(pos_inf, neg_inf), "+inf != -inf");
    ASSERT_FALSE(floatEqual(pos_inf, 1.0), "+inf != finite");
    
    // NaN
    ASSERT_FALSE(floatEqual(nan, nan), "NaN != NaN (IEEE 754)");
    ASSERT_FALSE(floatEqual(nan, 0.0), "NaN != 0");
    
    // Signed zeros
    ASSERT_TRUE(floatEqual(0.0, -0.0), "+0 == -0");
    ASSERT_TRUE(floatEqual(-0.0, 0.0), "-0 == +0");
        
    return true;
}

// =============================================================================
// Test Suite 2: UlpComparisonPolicy
// =============================================================================

bool test_ulp_basic() {
    
    float a = 1.0f;
    float b = 1.0f + std::numeric_limits<float>::epsilon();
    
    // Within 4 ULPs (default)
    ASSERT_TRUE((floatEqual<float, UlpComparisonPolicy>(a, a)), "Same value");
    ASSERT_TRUE((floatEqual<float, UlpComparisonPolicy>(a, b, 4.0)), "Within 4 ULPs");
    
    // Beyond 4 ULPs
    float c = 1.0f + 10.0f * std::numeric_limits<float>::epsilon();
    ASSERT_FALSE((floatEqual<float, UlpComparisonPolicy>(a, c, 4.0)), "Beyond 4 ULPs");
        
    return true;
}

bool test_ulp_subnormals() {
    
    // Float subnormals
    float denorm_f = std::numeric_limits<float>::denorm_min();
    ASSERT_TRUE((floatEqual<float, UlpComparisonPolicy>(denorm_f, denorm_f)), 
                "Float subnormal equals itself");
    
    // Double subnormals
    double denorm_d = std::numeric_limits<double>::denorm_min();
    ASSERT_TRUE((floatEqual<double, UlpComparisonPolicy>(denorm_d, denorm_d)), 
                "Double subnormal equals itself");
    
    // Near-zero hybrid handling
    ASSERT_TRUE((floatEqual<float, UlpComparisonPolicy>(0.0f, denorm_f)), 
                "Zero close to subnormal");
    
    return true;
}

bool test_ulp_sign_sensitivity() {
    
    double a = 1.0;
    double b = -1.0;
    
    // Different signs should NOT be equal
    ASSERT_FALSE((floatEqual<double, UlpComparisonPolicy>(a, b)), 
                 "Opposite signs not equal");
    
    // Same sign, different values
    double c = 1.0;
    double d = 1.0 + std::numeric_limits<double>::epsilon();
    ASSERT_TRUE((floatEqual<double, UlpComparisonPolicy>(c, d, 4.0)), 
                "Same sign, within ULPs");
    
    return true;
}

// =============================================================================
// Test Suite 3: RelativeComparisonPolicy
// =============================================================================

bool test_relative_scale_independence() {
    
    double relEps = 1e-5;
    
    // Small values
    double a1 = 1.0;
    double b1 = 1.0 + 0.5e-5;  // 0.5% relative error
    ASSERT_TRUE((floatEqual<double, RelativeComparisonPolicy>(a1, b1, relEps)), 
                "Small values within tolerance");
    
    // Large values (same relative error)
    double a2 = 1e6;
    double b2 = 1e6 + 0.5;  // 0.5% relative error (same proportion)
    ASSERT_TRUE((floatEqual<double, RelativeComparisonPolicy>(a2, b2, relEps)), 
                "Large values within tolerance");
        
    return true;
}

bool test_relative_default_epsilon() {
    
    // Test that default epsilon works (bug fix verification)
    double a = 1.0;
    double b = 1.0 + 50.0 * std::numeric_limits<double>::epsilon();
    
    // Should not crash with no epsilon provided (uses default)
    bool result = floatEqual<double, RelativeComparisonPolicy>(a, b);
    ASSERT_TRUE(result, "Default epsilon works");
    
    return true;
}

// =============================================================================
// Test Suite 4: HybridComparisonPolicy
// =============================================================================

bool test_hybrid_robustness() {
    
    double relEps = 1e-5;
    double absEps = 1e-8;
    
    // Near zero - absolute tolerance dominates
    double a1 = 1e-10;
    double b1 = 2e-10;
    ASSERT_TRUE((floatEqual<double, HybridComparisonPolicy>(a1, b1, relEps, absEps)), 
                "Near zero uses absolute tolerance");
    
    // Large values - relative tolerance dominates
    double a2 = 1e6;
    double b2 = 1e6 + 0.5;  // Small relative error
    ASSERT_TRUE((floatEqual<double, HybridComparisonPolicy>(a2, b2, relEps, absEps)), 
                "Large values use relative tolerance");
    
    // Medium values - should work well
    double a3 = 100.0;
    double b3 = 100.0 + 0.0001;
    ASSERT_TRUE((floatEqual<double, HybridComparisonPolicy>(a3, b3, relEps, absEps)), 
                "Medium values handled correctly");
        
    return true;
}

bool test_hybrid_parameter_flexibility() {
    
    double a = 1.0;
    double b = 1.0 + 50.0 * std::numeric_limits<double>::epsilon();
    
    // With 2 parameters
    bool result1 = floatEqual<double, HybridComparisonPolicy>(a, b, 1e-5, 1e-8);
    ASSERT_TRUE(result1, "Two parameters work");
    
    // With 1 parameter (uses for both rel and abs)
    bool result2 = floatEqual<double, HybridComparisonPolicy>(a, b, 1e-8);
    ASSERT_TRUE(result2, "One parameter works");
    
    // With 0 parameters (uses defaults)
    bool result3 = floatEqual<double, HybridComparisonPolicy>(a, b);
    ASSERT_TRUE(result3, "Zero parameters work (defaults)");
        
    return true;
}

// =============================================================================
// Test Suite 5: approximateEqual Convenience Function
// =============================================================================

bool test_approximate_equal_basic() {
    
    // Basic usage with defaults - use values within default epsilon
    double a = 1.0;
    double b = 1.0 + 50.0 * std::numeric_limits<double>::epsilon();
    ASSERT_TRUE(approximateEqual(a, b), "Basic approximate equality");
    
    // With custom tolerances for larger differences
    double c = 1.0;
    double d = 1.1;
    ASSERT_TRUE(approximateEqual(c, d, 0.2, 0.2), "Custom tolerances");
    ASSERT_FALSE(approximateEqual(c, d, 0.05, 0.05), "Outside tolerances");
        
    return true;
}

bool test_approximate_equal_types() {
    
    // Float - use values within float epsilon range
    float f1 = 1.0f;
    float f2 = 1.0f + 50.0f * std::numeric_limits<float>::epsilon();
    ASSERT_TRUE(approximateEqual(f1, f2), "Float type works");
    
    // Double - use values within double epsilon range
    double d1 = 1.0;
    double d2 = 1.0 + 50.0 * std::numeric_limits<double>::epsilon();
    ASSERT_TRUE(approximateEqual(d1, d2), "Double type works");
    
    // Long double - use values within long double epsilon range
    long double ld1 = 1.0L;
    long double ld2 = 1.0L + 50.0L * std::numeric_limits<long double>::epsilon();
    ASSERT_TRUE(approximateEqual(ld1, ld2), "Long double type works");
        
    return true;
}

// =============================================================================
// Test Suite 6: Type-Specific Default Epsilons
// =============================================================================

bool test_default_epsilons() {
    
    // Float epsilon
    float float_eps = static_cast<float>(getDefaultEpsilon<float>());
    ASSERT_TRUE(float_eps > 0.0f, "Float epsilon is positive");
    std::cout << "    Float epsilon: " << std::scientific << float_eps << std::endl;
    
    // Double epsilon
    double double_eps = getDefaultEpsilon<double>();
    ASSERT_TRUE(double_eps > 0.0, "Double epsilon is positive");
    std::cout << "    Double epsilon: " << std::scientific << double_eps << std::endl;
    
    // Long double epsilon
    double ld_eps = getDefaultEpsilon<long double>();
    ASSERT_TRUE(ld_eps > 0.0, "Long double epsilon is positive");
    std::cout << "    Long double epsilon: " << std::scientific << ld_eps << std::defaultfloat << std::endl;
    
    // Verify ordering: float_eps > double_eps > ld_eps
    ASSERT_TRUE(float_eps > double_eps, "Float epsilon > double epsilon");
    
    
    return true;
}

// =============================================================================
// Test Suite 7: Edge Cases and Stress Tests
// =============================================================================

bool test_extreme_values() {

    // Very large values
    double large1 = 1e308;
    double large2 = 1e308;
    ASSERT_TRUE(approximateEqual(large1, large2), "Very large values equal");
    
    // Very small values
    double small1 = 1e-308;
    double small2 = 1e-308;
    ASSERT_TRUE(approximateEqual(small1, small2), "Very small values equal");
    
    // Mixed scales
    double a = 1e100;
    double b = 1e-100;
    ASSERT_FALSE(approximateEqual(a, b), "Mixed scales not equal");
    
    return true;
}

bool test_denormal_numbers() {
    
    // Float denormals
    float denorm_f = std::numeric_limits<float>::denorm_min();
    ASSERT_TRUE(approximateEqual(denorm_f, denorm_f), "Float denormal equals itself");
    ASSERT_TRUE(approximateEqual(denorm_f, 0.0f), "Float denormal ≈ zero");
    
    // Double denormals
    double denorm_d = std::numeric_limits<double>::denorm_min();
    ASSERT_TRUE(approximateEqual(denorm_d, denorm_d), "Double denormal equals itself");
    ASSERT_TRUE(approximateEqual(denorm_d, 0.0), "Double denormal ≈ zero");
        
    return true;
}

// =============================================================================
// Main Entry Point
// =============================================================================

bool test_FloatingPointComparison() {

    PRINT_HEADER(FLOATING - POINT COMPARISON)

    TestRunner runner;
    
    RUN_TEST(runner, standard_basic);
    RUN_TEST(runner, standard_special_values);
    
    RUN_TEST(runner, ulp_basic);
    RUN_TEST(runner, ulp_subnormals);
    RUN_TEST(runner, ulp_sign_sensitivity);
    
    RUN_TEST(runner, relative_scale_independence);
    RUN_TEST(runner, relative_default_epsilon);
    
    RUN_TEST(runner, hybrid_robustness);
    RUN_TEST(runner, hybrid_parameter_flexibility);
    
    RUN_TEST(runner, approximate_equal_basic);
    RUN_TEST(runner, approximate_equal_types);
    
    RUN_TEST(runner, default_epsilons);
    
    RUN_TEST(runner, extreme_values);
    RUN_TEST(runner, denormal_numbers);
    
    // Print summary
    int failed = runner.print_summary();
    
    return (failed == 0);
}

} // namespace cpp_utilities::testing
