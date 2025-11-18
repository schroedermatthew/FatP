// test_FloatingPointComparison.cpp
// Comprehensive test suite with corrected logic

#include <iostream>
#include <iomanip>
#include <limits>
#include <cmath>
#include <algorithm>

#include "FloatingPointComparison.h"
#include "FatPTest.h"


namespace fat_p::testing {

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
    
    return true;
}

bool test_standard_boundary_values() {
    double eps = getDefaultEpsilon<double>();
    double a = 1.0;
    
    // Exactly at epsilon boundary
    double b_at_boundary = a + eps;
    ASSERT_TRUE(floatEqual(a, b_at_boundary), "Exactly at epsilon boundary passes");
    
    // Just beyond epsilon boundary - FIX: Use 2.0 instead of 1.001
    double b_beyond = a + eps * 2.0;
    ASSERT_FALSE(floatEqual(a, b_beyond), "Just beyond epsilon fails");
    
    // Exactly at custom epsilon boundary
    double c = 10.0;
    double d = 10.0 + 0.1;
    ASSERT_TRUE(floatEqual(c, d, 0.1), "At custom epsilon boundary");
    ASSERT_FALSE(floatEqual(c, d, 0.099), "Beyond custom epsilon boundary");
    
    return true;
}

bool test_standard_negative_values() {
    double eps = getDefaultEpsilon<double>();
    
    // Negative values within epsilon
    ASSERT_TRUE(floatEqual(-1.0, -1.0 + eps * 0.5), "Negative within epsilon");
    ASSERT_TRUE(floatEqual(-1.0, -1.0 - eps * 0.5), "Negative within epsilon (other direction)");
    
    // Negative values beyond epsilon
    ASSERT_FALSE(floatEqual(-1.0, -1.0 + eps * 2.0), "Negative beyond epsilon");
    
    // Large negative values
    ASSERT_TRUE(floatEqual(-1e6, -1e6 + eps * 0.5), "Large negative within epsilon");
    
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

bool test_standard_zero_comparisons() {
    double zero = 0.0;
    double tiny = std::numeric_limits<double>::denorm_min();
    double eps = getDefaultEpsilon<double>();
    
    // Zero vs tiny value (should pass with default epsilon)
    ASSERT_TRUE(floatEqual(zero, tiny), "Zero vs denorm_min");
    
    // Zero vs value just beyond epsilon
    ASSERT_FALSE(floatEqual(zero, eps * 1.1), "Zero vs value beyond epsilon");
    
    // Zero vs value at epsilon
    ASSERT_TRUE(floatEqual(zero, eps), "Zero vs value at epsilon");
    
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
    ASSERT_TRUE((floatEqual<float, UlpComparisonPolicy>(a, b, 4.0f)), "Within 4 ULPs");
    
    // Beyond 4 ULPs
    float c = 1.0f + 10.0f * std::numeric_limits<float>::epsilon();
    ASSERT_FALSE((floatEqual<float, UlpComparisonPolicy>(a, c, 4.0f)), "Beyond 4 ULPs");
        
    return true;
}

bool test_ulp_exact_boundaries() {
    float a = 1.0f;
    
    // Exactly 1 ULP apart
    float b = std::nextafter(a, 2.0f);
    ASSERT_TRUE((floatEqual<float, UlpComparisonPolicy>(a, b, 1.0f)), 
                "Exactly 1 ULP apart with tolerance 1.0");
    ASSERT_FALSE((floatEqual<float, UlpComparisonPolicy>(a, b, 0.5f)), 
                "Beyond 0.5 ULP tolerance");
    
    // Exactly 4 ULPs apart (default tolerance)
    float c = a;
    for (int i = 0; i < 4; ++i) {
        c = std::nextafter(c, 2.0f);
    }
    ASSERT_TRUE((floatEqual<float, UlpComparisonPolicy>(a, c)), 
                "Exactly 4 ULPs with default tolerance");
    
    // 5 ULPs apart - should fail with default
    float d = std::nextafter(c, 2.0f);
    ASSERT_FALSE((floatEqual<float, UlpComparisonPolicy>(a, d)), 
                "5 ULPs exceeds default tolerance");
    
    // Double precision test
    double da = 1.0;
    double db = std::nextafter(da, 2.0);
    ASSERT_TRUE((floatEqual<double, UlpComparisonPolicy>(da, db, 1.0)),
                "Double: exactly 1 ULP apart");
    
    return true;
}

bool test_ulp_negative_values() {
    // ULP with negative values (important for signed magnitude conversion)
    double neg_a = -1.0;
    double neg_b = std::nextafter(neg_a, -2.0);
    ASSERT_TRUE((floatEqual<double, UlpComparisonPolicy>(neg_a, neg_b, 1.0)),
                "Negative: 1 ULP apart");
    
    // Multiple ULPs in negative range
    double neg_c = neg_a;
    for (int i = 0; i < 3; ++i) {
        neg_c = std::nextafter(neg_c, -2.0);
    }
    ASSERT_TRUE((floatEqual<double, UlpComparisonPolicy>(neg_a, neg_c, 3.0)),
                "Negative: 3 ULPs apart");
    ASSERT_FALSE((floatEqual<double, UlpComparisonPolicy>(neg_a, neg_c, 2.0)),
                "Negative: beyond 2 ULP tolerance");
    
    // Large negative values
    double large_neg_a = -1e6;
    double large_neg_b = std::nextafter(large_neg_a, -2e6);
    ASSERT_TRUE((floatEqual<double, UlpComparisonPolicy>(large_neg_a, large_neg_b, 1.0)),
                "Large negative: 1 ULP apart");
    
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

bool test_ulp_subnormal_boundaries() {
    // Test the normal/subnormal boundary
    float normal_min = std::numeric_limits<float>::min();  // Smallest normal
    float subnormal_max = std::nextafter(normal_min, 0.0f);  // Largest subnormal
    
    // These span the normal/subnormal boundary - uses absolute tolerance
    ASSERT_TRUE((floatEqual<float, UlpComparisonPolicy>(normal_min, subnormal_max)),
                "Normal/subnormal boundary uses absolute tolerance");
    
    // Multiple subnormals
    float sub1 = std::numeric_limits<float>::denorm_min();
    float sub2 = sub1 * 2.0f;
    ASSERT_TRUE((floatEqual<float, UlpComparisonPolicy>(sub1, sub2)),
                "Two subnormals within absolute tolerance");
    
    // Double precision subnormal boundary
    double dnormal_min = std::numeric_limits<double>::min();
    double dsubnormal_max = std::nextafter(dnormal_min, 0.0);
    ASSERT_TRUE((floatEqual<double, UlpComparisonPolicy>(dnormal_min, dsubnormal_max)),
                "Double: normal/subnormal boundary");
    
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
    
    // Zero crossing
    double small_pos = std::numeric_limits<double>::min();
    double small_neg = -std::numeric_limits<double>::min();
    ASSERT_FALSE((floatEqual<double, UlpComparisonPolicy>(small_pos, small_neg)),
                "Small values crossing zero not equal");
    
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

bool test_relative_near_zero_weakness() {
    // This test documents the behavior of RelativeComparisonPolicy near zero
    
    double a = 1e-15;
    double b = 2e-15;  // 100% relative difference!
    
    // The difference (1e-15) is larger than the tolerance threshold (relEps * maxAbs = 2e-20).
    // This correctly FAILS, showing relative comparison works as designed.
    ASSERT_FALSE((floatEqual<double, RelativeComparisonPolicy>(a, b, 1e-5)),
                "Near-zero: relative policy correctly identifies large relative error");
    
    // FIX: Zero vs tiny now correctly fails (infinite relative error)
    // When one value is zero and the other is not, they have infinite relative difference
    double zero = 0.0;
    double tiny = 1e-320;
    ASSERT_FALSE((floatEqual<double, RelativeComparisonPolicy>(zero, tiny, 1e-5)),
                "Zero vs non-zero: correctly fails (infinite relative error)");
    
    return true;
}

bool test_relative_default_epsilon() {
    // Test that default epsilon works 
    double a = 1.0;
    double b = 1.0 + 50.0 * std::numeric_limits<double>::epsilon();
    
    // Should not crash with no epsilon provided (uses default)
    bool result = floatEqual<double, RelativeComparisonPolicy>(a, b);
    ASSERT_TRUE(result, "Default epsilon works");
    
    return true;
}

bool test_relative_zero_comparisons() {
    // Zero equals zero (both are zero, so maxAbs == 0.0)
    ASSERT_TRUE((floatEqual<double, RelativeComparisonPolicy>(0.0, 0.0, 1e-5)),
                "Zero equals zero");
    
    // Signed zeros (both are zero)
    ASSERT_TRUE((floatEqual<double, RelativeComparisonPolicy>(0.0, -0.0, 1e-5)),
                "Signed zeros equal");
    
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

bool test_hybrid_near_zero_robustness() {
    // Demonstrate that Hybrid provides flexibility near zero
    
    double a = 1e-15;
    double b = 2e-15;  // 100% relative difference
    
    // With strict absolute tolerance, should FAIL (difference is 1e-15 > 1e-16)
    ASSERT_FALSE((floatEqual<double, HybridComparisonPolicy>(a, b, 1e-5, 1e-16)),
                "Hybrid catches near-zero difference with strict absEps");
    
    // With relaxed absolute tolerance, should PASS (difference is 1e-15 <= 1e-14)
    ASSERT_TRUE((floatEqual<double, HybridComparisonPolicy>(a, b, 1e-5, 1e-14)),
                "Hybrid passes with relaxed absEps");
    
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

bool test_hybrid_boundary_conditions() {
    double relEps = 1e-5;
    double absEps = 1e-8;
    
    // Exactly at absolute boundary (near zero)
    double a1 = 0.0;
    double b1 = absEps;
    ASSERT_TRUE((floatEqual<double, HybridComparisonPolicy>(a1, b1, relEps, absEps)),
                "Exactly at absolute boundary");
    
    // Just beyond absolute boundary
    double b2 = absEps * 1.001;
    bool result = floatEqual<double, HybridComparisonPolicy>(a1, b2, relEps, absEps);
    ASSERT_FALSE(result, "Just beyond absolute boundary (near zero)");
    
    // Exactly at relative boundary (large values)
    double a3 = 1e6;
    double b3 = 1e6 * (1.0 + relEps);
    ASSERT_TRUE((floatEqual<double, HybridComparisonPolicy>(a3, b3, relEps, absEps)),
                "Exactly at relative boundary");
    
    return true;
}

// =============================================================================
// Test Suite 5: Cross-Policy Verification
// =============================================================================

bool test_policy_differences() {
    double a = 1.0;
    double b = 1.000001;  // Small absolute difference
    
    // Standard (absolute): should pass with large epsilon
    ASSERT_TRUE((floatEqual<double, StandardComparisonPolicy>(a, b, 1e-5)),
                "Standard absolute passes");
    
    // Standard: should fail with strict epsilon
    ASSERT_FALSE((floatEqual<double, StandardComparisonPolicy>(a, b, 1e-7)),
                "Standard absolute fails with strict epsilon");
    
    // Relative: check behavior
    bool rel_result = floatEqual<double, RelativeComparisonPolicy>(a, b, 1e-7);
    ASSERT_FALSE(rel_result, "Relative fails with strict tolerance");
    
    // Hybrid: demonstrates flexibility
    ASSERT_TRUE((floatEqual<double, HybridComparisonPolicy>(a, b, 1e-7, 1e-5)),
                "Hybrid uses absolute tolerance");
    ASSERT_FALSE((floatEqual<double, HybridComparisonPolicy>(a, b, 1e-7, 1e-7)),
                "Hybrid fails with strict both tolerances");
    
    return true;
}

bool test_policy_consistency_on_special_values() {
    double pos_inf = std::numeric_limits<double>::infinity();
    double neg_inf = -std::numeric_limits<double>::infinity();
    double nan = std::numeric_limits<double>::quiet_NaN();
    
    // All policies should agree on special values
    
    // NaN handling
    ASSERT_FALSE(floatEqual(nan, nan), "Standard: NaN != NaN");
    ASSERT_FALSE((floatEqual<double, UlpComparisonPolicy>(nan, nan)), "ULP: NaN != NaN");
    ASSERT_FALSE((floatEqual<double, RelativeComparisonPolicy>(nan, nan, 1e-5)), "Relative: NaN != NaN");
    ASSERT_FALSE((floatEqual<double, HybridComparisonPolicy>(nan, nan, 1e-5, 1e-8)), "Hybrid: NaN != NaN");
    
    // Infinity handling
    ASSERT_TRUE(floatEqual(pos_inf, pos_inf), "Standard: +inf == +inf");
    ASSERT_TRUE((floatEqual<double, UlpComparisonPolicy>(pos_inf, pos_inf)), "ULP: +inf == +inf");
    ASSERT_TRUE((floatEqual<double, RelativeComparisonPolicy>(pos_inf, pos_inf, 1e-5)), "Relative: +inf == +inf");
    ASSERT_TRUE((floatEqual<double, HybridComparisonPolicy>(pos_inf, pos_inf, 1e-5, 1e-8)), "Hybrid: +inf == +inf");
    
    // Mixed sign infinity
    ASSERT_FALSE(floatEqual(pos_inf, neg_inf), "Standard: +inf != -inf");
    ASSERT_FALSE((floatEqual<double, UlpComparisonPolicy>(pos_inf, neg_inf)), "ULP: +inf != -inf");
    ASSERT_FALSE((floatEqual<double, RelativeComparisonPolicy>(pos_inf, neg_inf, 1e-5)), "Relative: +inf != -inf");
    ASSERT_FALSE((floatEqual<double, HybridComparisonPolicy>(pos_inf, neg_inf, 1e-5, 1e-8)), "Hybrid: +inf != -inf");
    
    return true;
}

bool test_scale_transition_points() {
    // Test where different policies transition from one mode to another
    
    double relEps = 1e-5;
    double absEps = 1e-8;
    
    // Very small scale (absolute dominates)
    double a1 = 1e-10;
    double b1 = 1e-10 + 5e-11;
    ASSERT_TRUE((floatEqual<double, HybridComparisonPolicy>(a1, b1, relEps, absEps)),
                "Very small: absolute dominates");
    
    // Transition scale
    double a2 = 1e-3;
    double b2 = 1e-3 * (1.0 + relEps * 0.5);
    ASSERT_TRUE((floatEqual<double, HybridComparisonPolicy>(a2, b2, relEps, absEps)),
                "Transition scale: passes");
    
    // Large scale (relative dominates)
    double a3 = 1e6;
    double b3 = 1e6 + absEps * 1.5;
    ASSERT_TRUE((floatEqual<double, HybridComparisonPolicy>(a3, b3, relEps, absEps)),
                "Large scale: relative dominates");
    
    return true;
}

// =============================================================================
// Test Suite 6: Mixed-Sign Edge Cases
// =============================================================================

bool test_mixed_signs() {
    // Small positive vs small negative (should fail in all policies)
    // FIXED: Now passes with sign consistency check in policies
    double a = 1e-10;
    double b = -1e-10;
    
    ASSERT_FALSE(floatEqual(a, b), "Standard: Small opposite signs not equal");
    ASSERT_FALSE((floatEqual<double, UlpComparisonPolicy>(a, b)), 
                "ULP: Small opposite signs not equal");
    ASSERT_FALSE((floatEqual<double, RelativeComparisonPolicy>(a, b, 1e-5)), 
                "Relative: Small opposite signs not equal");
    ASSERT_FALSE((floatEqual<double, HybridComparisonPolicy>(a, b, 1e-5, 1e-8)),
                "Hybrid: Small opposite signs not equal");
    
    // Large positive vs large negative
    double c = 1e6;
    double d = -1e6;
    ASSERT_FALSE(floatEqual(c, d), "Standard: Large opposite signs not equal");
    ASSERT_FALSE((floatEqual<double, UlpComparisonPolicy>(c, d)),
                "ULP: Large opposite signs not equal");
    
    return true;
}

bool test_near_zero_crossing() {
    // Values very close to zero but opposite signs
    // FIXED: Now passes with sign consistency check in policies
    float tiny_pos = std::numeric_limits<float>::denorm_min();
    float tiny_neg = -std::numeric_limits<float>::denorm_min();
    
    ASSERT_FALSE(floatEqual(tiny_pos, tiny_neg),
                "Denorm opposite signs not equal");
    
    // Within epsilon but opposite signs
    double eps = getDefaultEpsilon<double>();
    ASSERT_FALSE(floatEqual(eps * 0.5, -eps * 0.5),
                "Within epsilon but opposite signs not equal");
    
    return true;
}

// =============================================================================
// Test Suite 7: approximateEqual Convenience Function
// =============================================================================

bool test_approximate_equal_basic() {
    // Basic usage with defaults
    double a = 1.0;
    double b = 1.0 + 50.0 * std::numeric_limits<double>::epsilon();
    ASSERT_TRUE(approximateEqual(a, b), "Basic approximate equality");
    
    // With custom tolerances
    double c = 1.0;
    double d = 1.1;
    ASSERT_TRUE(approximateEqual(c, d, 0.2, 0.2), "Custom tolerances");
    ASSERT_FALSE(approximateEqual(c, d, 0.05, 0.05), "Outside tolerances");
        
    return true;
}

bool test_approximate_equal_types() {
    // Float
    float f1 = 1.0f;
    float f2 = 1.0f + 50.0f * std::numeric_limits<float>::epsilon();
    ASSERT_TRUE(approximateEqual(f1, f2), "Float type works");
    
    // Double
    double d1 = 1.0;
    double d2 = 1.0 + 50.0 * std::numeric_limits<double>::epsilon();
    ASSERT_TRUE(approximateEqual(d1, d2), "Double type works");
    
    // Long double
    long double ld1 = 1.0L;
    long double ld2 = 1.0L + 50.0L * std::numeric_limits<long double>::epsilon();
    ASSERT_TRUE(approximateEqual(ld1, ld2), "Long double type works");
        
    return true;
}

bool test_approximate_equal_zero_handling() {
    double zero = 0.0;
    
    // Zero vs denorm
    double denorm = std::numeric_limits<double>::denorm_min();
    ASSERT_TRUE(approximateEqual(zero, denorm), "Zero vs denorm");
    
    // Zero vs value within absolute tolerance
    double eps = getDefaultEpsilon<double>();
    ASSERT_TRUE(approximateEqual(zero, eps * 0.5), "Zero vs half epsilon");
    
    // Zero vs value with custom tolerance
    ASSERT_TRUE(approximateEqual(zero, 1e-6, 1e-5, 1e-5), "Zero with custom tolerance");
    
    return true;
}

// =============================================================================
// Test Suite 8: Type-Specific Default Epsilons
// =============================================================================

bool test_default_epsilons() {
    // Float epsilon
    float float_eps = getDefaultEpsilon<float>();
    ASSERT_TRUE(float_eps > 0.0f, "Float epsilon is positive");
    std::cout << "    Float epsilon: " << std::scientific << float_eps << std::endl;
    
    // Double epsilon
    double double_eps = getDefaultEpsilon<double>();
    ASSERT_TRUE(double_eps > 0.0, "Double epsilon is positive");
    std::cout << "    Double epsilon: " << std::scientific << double_eps << std::endl;
    
    // Long double epsilon
    long double ld_eps = getDefaultEpsilon<long double>();
    ASSERT_TRUE(ld_eps > 0.0L, "Long double epsilon is positive");
    std::cout << "    Long double epsilon: " << std::scientific << ld_eps << std::defaultfloat << std::endl;
    
    // Verify ordering: float_eps > double_eps
    ASSERT_TRUE(float_eps > double_eps, "Float epsilon > double epsilon");
    
    return true;
}

bool test_epsilon_type_correctness() {
    // Verify that getDefaultEpsilon returns the correct type
    
    // Float
    auto float_eps = getDefaultEpsilon<float>();
    static_assert(std::is_same_v<decltype(float_eps), float>, "Float epsilon should be float type");
    ASSERT_TRUE(float_eps > 0.0f, "Float epsilon valid");
    
    // Double
    auto double_eps = getDefaultEpsilon<double>();
    static_assert(std::is_same_v<decltype(double_eps), double>, "Double epsilon should be double type");
    ASSERT_TRUE(double_eps > 0.0, "Double epsilon valid");
    
    // Long double
    auto ld_eps = getDefaultEpsilon<long double>();
    static_assert(std::is_same_v<decltype(ld_eps), long double>, "Long double epsilon should be long double type");
    ASSERT_TRUE(ld_eps > 0.0L, "Long double epsilon valid");
    
    return true;
}

// =============================================================================
// Test Suite 9: Edge Cases and Stress Tests
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
    
    // Near max value
    double max_val = std::numeric_limits<double>::max();
    ASSERT_TRUE(approximateEqual(max_val, max_val), "Max value equals itself");
    
    return true;
}

bool test_denormal_numbers() {
    // Float denormals
    float denorm_f = std::numeric_limits<float>::denorm_min();
    ASSERT_TRUE(approximateEqual(denorm_f, denorm_f), "Float denormal equals itself");
    ASSERT_TRUE(approximateEqual(denorm_f, 0.0f), "Float denormal ~= zero");
    
    // Double denormals
    double denorm_d = std::numeric_limits<double>::denorm_min();
    ASSERT_TRUE(approximateEqual(denorm_d, denorm_d), "Double denormal equals itself");
    ASSERT_TRUE(approximateEqual(denorm_d, 0.0), "Double denormal ~= zero");
    
    // Multiple denormals
    float denorm2_f = denorm_f * 2.0f;
    ASSERT_TRUE(approximateEqual(denorm_f, denorm2_f), "Float denormals close");
    
    return true;
}

bool test_pathological_cases() {
    // Values designed to stress-test the comparison logic
    
    // Very close to 1.0
    double a = 1.0;
    double b = 1.0 + std::numeric_limits<double>::epsilon();
    ASSERT_TRUE(approximateEqual(a, b), "1.0 + epsilon");
    
    // Powers of 2
    float pow2_a = 1024.0f;
    float pow2_b = std::nextafter(pow2_a, 2048.0f);
    ASSERT_TRUE((floatEqual<float, UlpComparisonPolicy>(pow2_a, pow2_b, 1.0f)),
                "Power of 2: 1 ULP");
    
    // Reciprocals
    double third = 1.0 / 3.0;
    double approx_third = 0.333333333333;
    ASSERT_TRUE(approximateEqual(third, approx_third, 1e-10, 1e-12),
                "1/3 approximation");
    
    // Values near sqrt(2)
    double sqrt2_calc = std::sqrt(2.0);
    double sqrt2_approx = 1.41421356237;
    ASSERT_TRUE(approximateEqual(sqrt2_calc, sqrt2_approx, 1e-10, 1e-11),
                "sqrt(2) approximation");
    
    return true;
}

bool test_consecutive_values() {
    // Test multiple consecutive floating-point values
    
    float base = 1.0f;
    float current = base;
    
    // Test first 10 consecutive values
    for (int i = 0; i < 10; ++i) {
        float next = std::nextafter(current, 2.0f);
        
        // Each should be within i+1 ULPs of base
        ASSERT_TRUE((floatEqual<float, UlpComparisonPolicy>(base, next, static_cast<float>(i + 1))),
                    "Consecutive value within ULP tolerance");
        
        // But should fail with i ULPs tolerance (except for i=0)
        if (i > 0) {
            ASSERT_FALSE((floatEqual<float, UlpComparisonPolicy>(base, next, static_cast<float>(i))),
                        "Consecutive value beyond ULP tolerance");
        }
        
        current = next;
    }
    
    return true;
}

// =============================================================================
// Test Suite 10: Long Double Comprehensive Testing
// =============================================================================

bool test_long_double_comprehensive() {
    long double ld_eps = getDefaultEpsilon<long double>();
    
    // Verify long double uses its precision
    long double a = 1.0L;
    long double b = 1.0L + ld_eps * 0.5L;
    ASSERT_TRUE(approximateEqual(a, b), "Long double within epsilon");
    
    // Beyond epsilon
    long double c = 1.0L + ld_eps * 2.0L;
    ASSERT_FALSE(approximateEqual(a, c), "Long double beyond epsilon");
    
    // Long double special values
    long double ld_inf = std::numeric_limits<long double>::infinity();
    ASSERT_TRUE(approximateEqual(ld_inf, ld_inf), "Long double infinity");
    
    // Long double denormals
    long double ld_denorm = std::numeric_limits<long double>::denorm_min();
    ASSERT_TRUE(approximateEqual(ld_denorm, 0.0L), "Long double denorm ~= zero");
    
    return true;
}

bool test_long_double_precision() {
    // Demonstrate long double precision
    
    long double ld1 = 1.0L;
    long double ld_eps_val = std::numeric_limits<long double>::epsilon();
    long double ld2 = ld1 + ld_eps_val * 0.5L;
    
    // Cast to double
    double d1 = static_cast<double>(ld1);
    double d2 = static_cast<double>(ld2);
    
    // Long double comparison should distinguish these
    ASSERT_TRUE(approximateEqual(ld1, ld2), "Long double: within epsilon");
    
    // After casting to double, they might be identical
    bool doubles_equal = (d1 == d2);
    std::cout << "    Long double -> double precision test: "
              << (doubles_equal ? "values became identical (as expected)" : "values still distinct")
              << std::endl;
    
    return true;
}

// =============================================================================
// Main Entry Point
// =============================================================================

bool test_FloatingPointComparison() {
    PRINT_HEADER("FLOATING-POINT COMPARISON - COMPREHENSIVE")

    TestRunner runner;
    
    std::cout << "\n=== StandardComparisonPolicy ===" << std::endl;
    RUN_TEST(runner, standard_basic);
    RUN_TEST(runner, standard_boundary_values);
    RUN_TEST(runner, standard_negative_values);
    RUN_TEST(runner, standard_special_values);
    RUN_TEST(runner, standard_zero_comparisons);
    
    std::cout << "\n=== UlpComparisonPolicy ===" << std::endl;
    RUN_TEST(runner, ulp_basic);
    RUN_TEST(runner, ulp_exact_boundaries);
    RUN_TEST(runner, ulp_negative_values);
    RUN_TEST(runner, ulp_subnormals);
    RUN_TEST(runner, ulp_subnormal_boundaries);
    RUN_TEST(runner, ulp_sign_sensitivity);
    
    std::cout << "\n=== RelativeComparisonPolicy ===" << std::endl;
    RUN_TEST(runner, relative_scale_independence);
    RUN_TEST(runner, relative_near_zero_weakness);
    RUN_TEST(runner, relative_default_epsilon);
    RUN_TEST(runner, relative_zero_comparisons);
    
    std::cout << "\n=== HybridComparisonPolicy ===" << std::endl;
    RUN_TEST(runner, hybrid_robustness);
    RUN_TEST(runner, hybrid_near_zero_robustness);
    RUN_TEST(runner, hybrid_parameter_flexibility);
    RUN_TEST(runner, hybrid_boundary_conditions);
    
    std::cout << "\n=== Cross-Policy Verification ===" << std::endl;
    RUN_TEST(runner, policy_differences);
    RUN_TEST(runner, policy_consistency_on_special_values);
    RUN_TEST(runner, scale_transition_points);
    
    std::cout << "\n=== Mixed-Sign Edge Cases ===" << std::endl;
    RUN_TEST(runner, mixed_signs);
    RUN_TEST(runner, near_zero_crossing);
    
    std::cout << "\n=== approximateEqual Convenience Function ===" << std::endl;
    RUN_TEST(runner, approximate_equal_basic);
    RUN_TEST(runner, approximate_equal_types);
    RUN_TEST(runner, approximate_equal_zero_handling);
    
    std::cout << "\n=== Type-Specific Default Epsilons ===" << std::endl;
    RUN_TEST(runner, default_epsilons);
    RUN_TEST(runner, epsilon_type_correctness);
    
    std::cout << "\n=== Edge Cases and Stress Tests ===" << std::endl;
    RUN_TEST(runner, extreme_values);
    RUN_TEST(runner, denormal_numbers);
    RUN_TEST(runner, pathological_cases);
    RUN_TEST(runner, consecutive_values);
    
    std::cout << "\n=== Long Double Comprehensive Testing ===" << std::endl;
    RUN_TEST(runner, long_double_comprehensive);
    RUN_TEST(runner, long_double_precision);
    
    // Print summary
    int failed = runner.print_summary();
    
    return (failed == 0);
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_FloatingPointComparison() ? 0 : 1;
}
#endif
