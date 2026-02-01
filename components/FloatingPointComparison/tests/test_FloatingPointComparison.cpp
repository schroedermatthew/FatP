/**
 * @file test_FloatingPointComparison.cpp
 * @brief Comprehensive unit tests for FloatingPointComparison.h
 */
/*
FATP_META:
  meta_version: 1
  component: FloatingPointComparison
  file_role: test
  path: components/FloatingPointComparison/tests/test_FloatingPointComparison.cpp
  namespace: fat_p
  summary: "Unit tests for FloatingPointComparison."
  api_stability: in_work
  related:
    docs_search: "FloatingPointComparison"
    headers:
      - include/fat_p/FloatingPointComparison.h
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

// test_FloatingPointComparison.cpp
// Comprehensive test suite with corrected logic for "Noise Floor" stability
// Now includes benchmark tests
//
// BOUNDARY TESTING BEST PRACTICE:
// For reliable boundary tests, prefer std::nextafter() over literal arithmetic:
//   GOOD:  std::nextafter(1.0, 2.0)     // Exact next representable float
//   AVOID: 1.0 + epsilon                 // May not land exactly on boundary
// This ensures tests behave consistently across platforms and compilers.

#include "FatPTest.h"
#include "FloatingPointComparison.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <vector>

namespace fat_p::testing::floatingpointcomparison
{

// =============================================================================
// Test Suite 1: StandardComparisonPolicy
// =============================================================================
FATP_TEST_CASE(standard_basic)
{
    // Exact equality
    FATP_ASSERT_TRUE(floatEqual(1.0, 1.0), "Exact equality");

    // Within default epsilon
    constexpr double eps = getDefaultEpsilon<double>();
    FATP_ASSERT_TRUE(floatEqual(1.0, 1.0 + eps * 0.5), "Within default epsilon");

    // Beyond default epsilon
    FATP_ASSERT_FALSE(floatEqual(1.0, 1.0 + eps * 2.0), "Beyond default epsilon");

    // Custom epsilon
    FATP_ASSERT_TRUE(floatEqual(1.0, 1.1, 0.2), "Custom epsilon 0.2");
    FATP_ASSERT_FALSE(floatEqual(1.0, 1.1, 0.05), "Custom epsilon 0.05");

    return true;
}

FATP_TEST_CASE(standard_boundary_values)
{
    constexpr double eps = getDefaultEpsilon<double>();
    double a = 1.0;

    // Exactly at epsilon boundary
    double b_at_boundary = a + eps;
    FATP_ASSERT_TRUE(floatEqual(a, b_at_boundary), "Exactly at epsilon boundary passes");

    // Just beyond epsilon boundary
    double b_beyond = a + eps * 2.0;
    FATP_ASSERT_FALSE(floatEqual(a, b_beyond), "Just beyond epsilon fails");

    // Exactly at custom epsilon boundary
    double c = 10.0;
    double d = 10.0 + 0.1;
    FATP_ASSERT_TRUE(floatEqual(c, d, 0.1), "At custom epsilon boundary");
    FATP_ASSERT_FALSE(floatEqual(c, d, 0.099), "Beyond custom epsilon boundary");

    return true;
}

FATP_TEST_CASE(standard_negative_values)
{
    constexpr double eps = getDefaultEpsilon<double>();

    // Negative values within epsilon
    FATP_ASSERT_TRUE(floatEqual(-1.0, -1.0 + eps * 0.5), "Negative within epsilon");
    FATP_ASSERT_TRUE(floatEqual(-1.0, -1.0 - eps * 0.5), "Negative within epsilon (other direction)");

    // Negative values beyond epsilon
    FATP_ASSERT_FALSE(floatEqual(-1.0, -1.0 + eps * 2.0), "Negative beyond epsilon");

    // Large negative values
    FATP_ASSERT_TRUE(floatEqual(-1e6, -1e6 + eps * 0.5), "Large negative within epsilon");

    return true;
}

FATP_TEST_CASE(standard_special_values)
{
    constexpr double pos_inf = std::numeric_limits<double>::infinity();
    constexpr double neg_inf = -std::numeric_limits<double>::infinity();
    constexpr double nan = std::numeric_limits<double>::quiet_NaN();

    // Infinities
    FATP_ASSERT_TRUE(floatEqual(pos_inf, pos_inf), "+inf == +inf");
    FATP_ASSERT_TRUE(floatEqual(neg_inf, neg_inf), "-inf == -inf");
    FATP_ASSERT_FALSE(floatEqual(pos_inf, neg_inf), "+inf != -inf");
    FATP_ASSERT_FALSE(floatEqual(pos_inf, 1.0), "+inf != finite");

    // NaN
    FATP_ASSERT_FALSE(floatEqual(nan, nan), "NaN != NaN (IEEE 754)");
    FATP_ASSERT_FALSE(floatEqual(nan, 0.0), "NaN != 0");

    // Signed zeros
    FATP_ASSERT_TRUE(floatEqual(0.0, -0.0), "+0 == -0");
    FATP_ASSERT_TRUE(floatEqual(-0.0, 0.0), "-0 == +0");

    return true;
}

FATP_TEST_CASE(standard_zero_comparisons)
{
    double zero = 0.0;
    constexpr double tiny = std::numeric_limits<double>::denorm_min();
    constexpr double eps = getDefaultEpsilon<double>();

    // Zero vs tiny value (should pass with default epsilon)
    FATP_ASSERT_TRUE(floatEqual(zero, tiny), "Zero vs denorm_min");

    // Zero vs value just beyond epsilon
    FATP_ASSERT_FALSE(floatEqual(zero, eps * 1.1), "Zero vs value beyond epsilon");

    // Zero vs value at epsilon
    FATP_ASSERT_TRUE(floatEqual(zero, eps), "Zero vs value at epsilon");

    return true;
}

// =============================================================================
// Test Suite 2: UlpComparisonPolicy
// =============================================================================
FATP_TEST_CASE(ulp_basic)
{
    float a = 1.0f;
    constexpr float b = 1.0f + std::numeric_limits<float>::epsilon();

    // Within 4 ULPs (default)
    FATP_ASSERT_TRUE((floatEqual<float, UlpComparisonPolicy>(a, a)), "Same value");
    FATP_ASSERT_TRUE((floatEqual<float, UlpComparisonPolicy>(a, b, 4.0f)), "Within 4 ULPs");

    // Beyond 4 ULPs
    constexpr float c = 1.0f + 10.0f * std::numeric_limits<float>::epsilon();
    FATP_ASSERT_FALSE((floatEqual<float, UlpComparisonPolicy>(a, c, 4.0f)), "Beyond 4 ULPs");

    return true;
}

FATP_TEST_CASE(ulp_exact_boundaries)
{
    float a = 1.0f;

    // Exactly 1 ULP apart
    float b = std::nextafter(a, 2.0f);
    FATP_ASSERT_TRUE((floatEqual<float, UlpComparisonPolicy>(a, b, 1.0f)), "Exactly 1 ULP apart with tolerance 1.0");
    FATP_ASSERT_FALSE((floatEqual<float, UlpComparisonPolicy>(a, b, 0.5f)), "Beyond 0.5 ULP tolerance");

    // Exactly 4 ULPs apart (default tolerance)
    float c = a;
    for (int i = 0; i < 4; ++i)
    {
        c = std::nextafter(c, 2.0f);
    }
    FATP_ASSERT_TRUE((floatEqual<float, UlpComparisonPolicy>(a, c)), "Exactly 4 ULPs with default tolerance");

    // 5 ULPs apart - should fail with default
    float d = std::nextafter(c, 2.0f);
    FATP_ASSERT_FALSE((floatEqual<float, UlpComparisonPolicy>(a, d)), "5 ULPs exceeds default tolerance");

    // Double precision test
    double da = 1.0;
    double db = std::nextafter(da, 2.0);
    FATP_ASSERT_TRUE((floatEqual<double, UlpComparisonPolicy>(da, db, 1.0)), "Double: exactly 1 ULP apart");

    return true;
}

FATP_TEST_CASE(ulp_negative_values)
{
    // ULP with negative values (important for signed magnitude conversion)
    double neg_a = -1.0;
    double neg_b = std::nextafter(neg_a, -2.0);
    FATP_ASSERT_TRUE((floatEqual<double, UlpComparisonPolicy>(neg_a, neg_b, 1.0)), "Negative: 1 ULP apart");

    // Multiple ULPs in negative range
    double neg_c = neg_a;
    for (int i = 0; i < 3; ++i)
    {
        neg_c = std::nextafter(neg_c, -2.0);
    }
    FATP_ASSERT_TRUE((floatEqual<double, UlpComparisonPolicy>(neg_a, neg_c, 3.0)), "Negative: 3 ULPs apart");
    FATP_ASSERT_FALSE((floatEqual<double, UlpComparisonPolicy>(neg_a, neg_c, 2.0)), "Negative: beyond 2 ULP tolerance");

    // Large negative values
    double large_neg_a = -1e6;
    double large_neg_b = std::nextafter(large_neg_a, -2e6);
    FATP_ASSERT_TRUE((floatEqual<double, UlpComparisonPolicy>(large_neg_a, large_neg_b, 1.0)),
                     "Large negative: 1 ULP apart");

    return true;
}

FATP_TEST_CASE(ulp_subnormals)
{
    // Float subnormals
    constexpr float denorm_f = std::numeric_limits<float>::denorm_min();
    FATP_ASSERT_TRUE((floatEqual<float, UlpComparisonPolicy>(denorm_f, denorm_f)), "Float subnormal equals itself");

    // Double subnormals
    constexpr double denorm_d = std::numeric_limits<double>::denorm_min();
    FATP_ASSERT_TRUE((floatEqual<double, UlpComparisonPolicy>(denorm_d, denorm_d)), "Double subnormal equals itself");

    // Near-zero hybrid handling
    FATP_ASSERT_TRUE((floatEqual<float, UlpComparisonPolicy>(0.0f, denorm_f)), "Zero close to subnormal");

    return true;
}

FATP_TEST_CASE(ulp_subnormal_boundaries)
{
    // Test subnormals near the boundary to normal numbers
    constexpr float min_normal_f = std::numeric_limits<float>::min();
    constexpr float denorm_f = std::numeric_limits<float>::denorm_min();

    // ULP policy uses absolute tolerance (1e-6 for float) when subnormals are involved
    // min_normal (~1.175e-38) and denorm_min (~1.4e-45) both fall within 1e-6
    // This is expected and practical behavior - subnormals are treated with larger tolerance
    FATP_ASSERT_TRUE((floatEqual<float, UlpComparisonPolicy>(min_normal_f, denorm_f)),
                     "Normal and subnormal within subnormal absolute tolerance");

    return true;
}

FATP_TEST_CASE(ulp_sign_sensitivity)
{
    // ULP comparison is STRICT about signs (can't cross zero)
    float a = 1.0f;
    float b = -1.0f;

    FATP_ASSERT_FALSE((floatEqual<float, UlpComparisonPolicy>(a, b, 1000.0f)), "Different signs always fail");

    // Even tiny opposite-sign values fail
    double tiny_pos = 1e-10;
    double tiny_neg = -1e-10;
    FATP_ASSERT_FALSE((floatEqual<double, UlpComparisonPolicy>(tiny_pos, tiny_neg, 1000.0)),
                      "Tiny opposite signs fail");

    return true;
}

// =============================================================================
// Test Suite 3: RelativeComparisonPolicy
// =============================================================================
FATP_TEST_CASE(relative_scale_independence)
{
    double rel_eps = 1e-6;

    // Small scale
    double small_a = 1e-8;
    double small_b = 1e-8 * (1.0 + rel_eps * 0.5);
    FATP_ASSERT_TRUE((floatEqual<double, RelativeComparisonPolicy>(small_a, small_b, rel_eps)),
                     "Small scale: within relative tolerance");

    // Large scale
    double large_a = 1e8;
    double large_b = 1e8 * (1.0 + rel_eps * 0.5);
    FATP_ASSERT_TRUE((floatEqual<double, RelativeComparisonPolicy>(large_a, large_b, rel_eps)),
                     "Large scale: within relative tolerance");

    return true;
}

FATP_TEST_CASE(relative_near_zero_weakness)
{
    double rel_eps = 1e-6;

    // Relative policy struggles near zero
    double near_zero_a = 1e-15;
    double near_zero_b = 2e-15;

    // Even though difference is tiny, relative error is 100%!
    FATP_ASSERT_FALSE((floatEqual<double, RelativeComparisonPolicy>(near_zero_a, near_zero_b, rel_eps)),
                      "Near zero: relative policy fails");

    return true;
}

FATP_TEST_CASE(relative_default_epsilon)
{
    double a = 1.0;
    double b = 1.0 + getDefaultEpsilon<double>() * 0.5;

    FATP_ASSERT_TRUE((floatEqual<double, RelativeComparisonPolicy>(a, b)), "Default epsilon works");

    return true;
}

FATP_TEST_CASE(relative_zero_comparisons)
{
    // Zeros are always equal in relative comparison
    FATP_ASSERT_TRUE((floatEqual<double, RelativeComparisonPolicy>(0.0, 0.0)), "Zero equals zero");
    FATP_ASSERT_TRUE((floatEqual<double, RelativeComparisonPolicy>(0.0, -0.0)), "+0 equals -0");

    // But any non-zero vs zero fails (infinite relative error)
    constexpr double eps = getDefaultEpsilon<double>();
    FATP_ASSERT_FALSE((floatEqual<double, RelativeComparisonPolicy>(0.0, eps)), "Zero vs epsilon fails");

    return true;
}

// =============================================================================
// Test Suite 4: HybridComparisonPolicy
// =============================================================================
FATP_TEST_CASE(hybrid_robustness)
{
    double rel_eps = 1e-6;
    double abs_eps = 1e-12;

    // Works at small scale (absolute tolerance kicks in)
    double small_a = 1e-13;
    double small_b = 2e-13;
    FATP_ASSERT_TRUE((floatEqual<double, HybridComparisonPolicy>(small_a, small_b, rel_eps, abs_eps)),
                     "Small scale: absolute tolerance works");

    // Works at large scale (relative tolerance kicks in)
    double large_a = 1e8;
    double large_b = 1e8 * (1.0 + rel_eps * 0.5);
    FATP_ASSERT_TRUE((floatEqual<double, HybridComparisonPolicy>(large_a, large_b, rel_eps, abs_eps)),
                     "Large scale: relative tolerance works");

    return true;
}

FATP_TEST_CASE(hybrid_near_zero_robustness)
{
    double rel_eps = 1e-6;
    double abs_eps = 1e-12;

    // Hybrid handles near-zero gracefully
    double a = 5e-13;
    double b = -5e-13;

    FATP_ASSERT_TRUE(approximateEqual(a, b, rel_eps, abs_eps), "Near zero: absolute tolerance catches it");

    return true;
}

FATP_TEST_CASE(hybrid_parameter_flexibility)
{
    // Can use same value for both parameters
    double eps = 1e-9;
    double a = 1.0;
    double b = 1.0 + eps * 0.5;

    FATP_ASSERT_TRUE(approximateEqual(a, b, eps, eps), "Single epsilon for both tolerances");

    // Or different values
    double rel_eps = 1e-6;
    double abs_eps = 1e-12;
    FATP_ASSERT_TRUE(approximateEqual(a, b, rel_eps, abs_eps), "Different tolerances");

    return true;
}

FATP_TEST_CASE(hybrid_boundary_conditions)
{
    double rel_eps = 1e-6;
    double abs_eps = 1e-12;

    // At the boundary between absolute and relative regimes
    double boundary_val = abs_eps / rel_eps; // ~ 1e-6

    double a = boundary_val;
    double b = boundary_val * (1.0 + rel_eps * 0.5);

    FATP_ASSERT_TRUE(approximateEqual(a, b, rel_eps, abs_eps), "Boundary between absolute and relative regimes");

    return true;
}

// =============================================================================
// Test Suite 5: Cross-Policy Verification
// =============================================================================
FATP_TEST_CASE(policy_differences)
{
    // Show where policies differ
    double a = 1e-10;
    double b = -1e-10;

    // Standard/Hybrid allow sign crossing (noise floor)
    FATP_ASSERT_TRUE(floatEqual(a, b, 1e-6), "Standard allows sign crossing");
    FATP_ASSERT_TRUE(approximateEqual(a, b, 1e-6, 1e-6), "Hybrid allows sign crossing");

    // Relative/ULP are strict about signs
    FATP_ASSERT_FALSE((floatEqual<double, RelativeComparisonPolicy>(a, b, 1e-6)), "Relative rejects sign crossing");
    FATP_ASSERT_FALSE((floatEqual<double, UlpComparisonPolicy>(a, b, 1000.0)), "ULP rejects sign crossing");

    return true;
}

FATP_TEST_CASE(policy_consistency_on_special_values)
{
    constexpr double pos_inf = std::numeric_limits<double>::infinity();
    constexpr double neg_inf = -std::numeric_limits<double>::infinity();
    constexpr double nan = std::numeric_limits<double>::quiet_NaN();

    // All policies agree on infinities
    FATP_ASSERT_TRUE(floatEqual(pos_inf, pos_inf), "Standard: +inf == +inf");
    FATP_ASSERT_TRUE((floatEqual<double, RelativeComparisonPolicy>(pos_inf, pos_inf)), "Relative: +inf == +inf");
    FATP_ASSERT_TRUE((floatEqual<double, UlpComparisonPolicy>(pos_inf, pos_inf)), "ULP: +inf == +inf");
    FATP_ASSERT_TRUE(approximateEqual(pos_inf, pos_inf), "Hybrid: +inf == +inf");

    // All policies agree on NaN
    FATP_ASSERT_FALSE(floatEqual(nan, nan), "Standard: NaN != NaN");
    FATP_ASSERT_FALSE((floatEqual<double, RelativeComparisonPolicy>(nan, nan)), "Relative: NaN != NaN");
    FATP_ASSERT_FALSE((floatEqual<double, UlpComparisonPolicy>(nan, nan)), "ULP: NaN != NaN");
    FATP_ASSERT_FALSE(approximateEqual(nan, nan), "Hybrid: NaN != NaN");

    return true;
}

FATP_TEST_CASE(scale_transition_points)
{
    // Test transition between absolute and relative dominance
    double rel_eps = 1e-6;
    double abs_eps = 1e-12;

    // Very small: absolute dominates
    double tiny_a = 1e-14;
    double tiny_b = 2e-14;
    FATP_ASSERT_TRUE(approximateEqual(tiny_a, tiny_b, rel_eps, abs_eps), "Tiny scale: absolute dominates");

    // Medium: transition zone
    double med_a = 1e-9;
    double med_b = 1e-9 + 5e-13;
    FATP_ASSERT_TRUE(approximateEqual(med_a, med_b, rel_eps, abs_eps), "Medium scale: transition zone");

    // Large: relative dominates
    double large_a = 1e6;
    double large_b = 1e6 + 0.1;
    FATP_ASSERT_TRUE(approximateEqual(large_a, large_b, rel_eps, abs_eps), "Large scale: relative dominates");

    return true;
}

// =============================================================================
// Test Suite 6: Mixed-Sign Edge Cases
// =============================================================================
FATP_TEST_CASE(mixed_signs_noise_floor)
{
    // This is the KEY TEST for noise floor semantics
    double noise_floor = 1e-6;

    double pos_noise = +5e-7;
    double neg_noise = -5e-7;
    double zero = 0.0;

    // All should be considered equal within noise floor
    FATP_ASSERT_TRUE(approximateEqual(pos_noise, zero, noise_floor, noise_floor), "Positive noise equals zero");
    FATP_ASSERT_TRUE(approximateEqual(neg_noise, zero, noise_floor, noise_floor), "Negative noise equals zero");
    FATP_ASSERT_TRUE(approximateEqual(pos_noise, neg_noise, noise_floor, noise_floor),
                     "Opposite-sign noise values equal (CRITICAL)");

    return true;
}

FATP_TEST_CASE(near_zero_crossing_stability)
{
    // Simulates value oscillating around zero
    std::vector<double> oscillating = {1e-7, -2e-7, 3e-7, -1e-7, 2e-7};
    double noise_floor = 1e-6;

    // All should be considered "at zero"
    for (double val : oscillating)
    {
        FATP_ASSERT_TRUE(approximateEqual(val, 0.0, noise_floor, noise_floor), "Oscillating value treated as zero");
    }

    // Consecutive values should be consistent
    for (size_t i = 0; i < oscillating.size() - 1; ++i)
    {
        FATP_ASSERT_TRUE(approximateEqual(oscillating[i], oscillating[i + 1], noise_floor, noise_floor),
                         "Consecutive oscillating values equal");
    }

    return true;
}

// =============================================================================
// Test Suite 7: approximateEqual Convenience Function
// =============================================================================
FATP_TEST_CASE(approximate_equal_basic)
{
    // Default parameters - use difference within default epsilon
    double a = 1.0;
    double b = 1.0 + 1e-15; // Within default epsilon (~2.22e-14)
    FATP_ASSERT_TRUE(approximateEqual(a, b), "Default parameters");

    // Custom parameters
    double c = 1.0;
    double d = 1.0 + 1e-10;
    FATP_ASSERT_TRUE(approximateEqual(c, d, 1e-8, 1e-8), "Custom parameters");

    return true;
}

FATP_TEST_CASE(approximate_equal_types)
{
    // Float - default epsilon is ~1.19e-5
    float f1 = 1.0f;
    float f2 = 1.0f + 1e-6f; // Within default epsilon
    FATP_ASSERT_TRUE(approximateEqual(f1, f2), "Float comparison");

    // Double - default epsilon is ~2.22e-14
    double d1 = 1.0;
    double d2 = 1.0 + 1e-15; // Within default epsilon
    FATP_ASSERT_TRUE(approximateEqual(d1, d2), "Double comparison");

    // Long double - use very small difference within default epsilon
    long double ld1 = 1.0L;
    long double ld2 = 1.0L + 1e-18L; // Much smaller to be safe
    FATP_ASSERT_TRUE(approximateEqual(ld1, ld2), "Long double comparison");

    return true;
}

FATP_TEST_CASE(approximate_equal_zero_handling)
{
    double zero = 0.0;
    double tiny = 1e-14;
    double eps = 1e-12;

    FATP_ASSERT_TRUE(approximateEqual(zero, tiny, eps, eps), "Zero vs tiny within tolerance");

    return true;
}

// =============================================================================
// Test Suite 8: Type-Specific Default Epsilons
// =============================================================================
FATP_TEST_CASE(default_epsilons)
{
    constexpr float f_eps = getDefaultEpsilon<float>();
    constexpr double d_eps = getDefaultEpsilon<double>();
    constexpr long double ld_eps = getDefaultEpsilon<long double>();

    // Float epsilon should be larger (less precision)
    FATP_ASSERT_TRUE(f_eps > d_eps, "Float epsilon > double epsilon");

    // All should be positive
    FATP_ASSERT_TRUE(f_eps > 0, "Float epsilon positive");
    FATP_ASSERT_TRUE(d_eps > 0, "Double epsilon positive");
    FATP_ASSERT_TRUE(ld_eps > 0, "Long double epsilon positive");

    return true;
}

FATP_TEST_CASE(epsilon_type_correctness)
{
    // Ensure epsilon types match parameter types
    constexpr float f_eps = getDefaultEpsilon<float>();
    constexpr double d_eps = getDefaultEpsilon<double>();

    float f1 = 1.0f;
    float f2 = 1.0f + f_eps * 0.5f;
    FATP_ASSERT_TRUE(floatEqual(f1, f2), "Float uses float epsilon");

    double d1 = 1.0;
    double d2 = 1.0 + d_eps * 0.5;
    FATP_ASSERT_TRUE(floatEqual(d1, d2), "Double uses double epsilon");

    return true;
}

// =============================================================================
// Test Suite 9: Edge Cases and Stress Tests
// =============================================================================
FATP_TEST_CASE(extreme_values)
{
    // Very large values
    constexpr double huge_a = std::numeric_limits<double>::max() * 0.5;
    constexpr double huge_b = std::numeric_limits<double>::max() * 0.5;
    FATP_ASSERT_TRUE(approximateEqual(huge_a, huge_b), "Huge values equal");

    // Very small positive values
    constexpr double tiny_a = std::numeric_limits<double>::min();
    constexpr double tiny_b = std::numeric_limits<double>::min();
    FATP_ASSERT_TRUE(approximateEqual(tiny_a, tiny_b), "Tiny positive values equal");

    return true;
}

FATP_TEST_CASE(denormal_numbers)
{
    constexpr double denorm = std::numeric_limits<double>::denorm_min();

    // Denormals should equal zero with default epsilon
    FATP_ASSERT_TRUE(approximateEqual(denorm, 0.0), "Denormal ~= zero");

    // Multiple denormals
    FATP_ASSERT_TRUE(approximateEqual(denorm, denorm * 2.0), "Denormals ~= each other");

    return true;
}

FATP_TEST_CASE(pathological_cases)
{
    // Accumulated rounding error
    double sum = 0.0;
    for (int i = 0; i < 10; ++i)
    {
        sum += 0.1;
    }
    FATP_ASSERT_TRUE(approximateEqual(sum, 1.0), "Accumulated rounding error");

    // Reciprocals
    double third = 1.0 / 3.0;
    double approx_third = 0.333333333333;
    FATP_ASSERT_TRUE(approximateEqual(third, approx_third, 1e-10, 1e-12), "1/3 approximation");

    // Values near sqrt(2)
    double sqrt2_calc = std::sqrt(2.0);
    double sqrt2_approx = 1.41421356237;
    FATP_ASSERT_TRUE(approximateEqual(sqrt2_calc, sqrt2_approx, 1e-10, 1e-11), "sqrt(2) approximation");

    return true;
}

FATP_TEST_CASE(consecutive_values)
{
    // Test multiple consecutive floating-point values

    float base = 1.0f;
    float current = base;

    // Test first 10 consecutive values
    for (int i = 0; i < 10; ++i)
    {
        float next = std::nextafter(current, 2.0f);

        // Each should be within i+1 ULPs of base
        FATP_ASSERT_TRUE((floatEqual<float, UlpComparisonPolicy>(base, next, static_cast<float>(i + 1))),
                         "Consecutive value within ULP tolerance");

        // But should fail with i ULPs tolerance (except for i=0)
        if (i > 0)
        {
            FATP_ASSERT_FALSE((floatEqual<float, UlpComparisonPolicy>(base, next, static_cast<float>(i))),
                              "Consecutive value beyond ULP tolerance");
        }

        current = next;
    }

    return true;
}

// =============================================================================
// Test Suite 10: Long Double Comprehensive Testing
// =============================================================================
FATP_TEST_CASE(long_double_comprehensive)
{
    constexpr long double ld_eps = getDefaultEpsilon<long double>();

    // Verify long double uses its precision
    long double a = 1.0L;
    long double b = 1.0L + ld_eps * 0.5L;
    FATP_ASSERT_TRUE(approximateEqual(a, b), "Long double within epsilon");

    // Beyond epsilon
    long double c = 1.0L + ld_eps * 2.0L;
    FATP_ASSERT_FALSE(approximateEqual(a, c), "Long double beyond epsilon");

    // Long double special values
    constexpr long double ld_inf = std::numeric_limits<long double>::infinity();
    FATP_ASSERT_TRUE(approximateEqual(ld_inf, ld_inf), "Long double infinity");

    // Long double denormals
    constexpr long double ld_denorm = std::numeric_limits<long double>::denorm_min();
    FATP_ASSERT_TRUE(approximateEqual(ld_denorm, 0.0L), "Long double denorm ~= zero");

    return true;
}

FATP_TEST_CASE(long_double_precision)
{
    // Demonstrate long double precision

    long double ld1 = 1.0L;
    constexpr long double ld_eps_val = std::numeric_limits<long double>::epsilon();
    long double ld2 = ld1 + ld_eps_val * 0.5L;

    // Cast to double
    double d1 = static_cast<double>(ld1);
    double d2 = static_cast<double>(ld2);

    // Long double comparison should distinguish these
    FATP_ASSERT_TRUE(approximateEqual(ld1, ld2), "Long double: within epsilon");

    // After casting to double, they might be identical
    bool doubles_equal = (d1 == d2);
    std::cout << "    Long double -> double precision test: "
              << (doubles_equal ? "values became identical (as expected)" : "values still distinct") << std::endl;

    return true;
}

// =============================================================================
// Test Suite 11: Control System Simulation (NEW)
// =============================================================================
FATP_TEST_CASE(control_system_noise)
{
    // Simulates PID controller output oscillating around setpoint
    double setpoint = 0.0;

    // Simulated sensor noise (Frame N and Frame N+1)
    double frame_N = +1e-9;
    double frame_N1 = -1e-9;

    // Defined noise floor for the system
    double noise_floor = 1e-6;

    // Verify that the oscillation is considered "stable" (equal to setpoint)
    FATP_ASSERT_TRUE(approximateEqual(frame_N, setpoint, noise_floor, noise_floor), "Control: +Noise equals setpoint");
    FATP_ASSERT_TRUE(approximateEqual(frame_N1, setpoint, noise_floor, noise_floor), "Control: -Noise equals setpoint");

    // Verify that the two frames are consistent with each other
    // This is the critical "Sign Consistency Trap" fix.
    // Difference is 2e-9, which is <= 1e-6 (noise_floor).
    FATP_ASSERT_TRUE(approximateEqual(frame_N, frame_N1, noise_floor, noise_floor),
                     "Control: +Noise equals -Noise (Inter-frame stability)");

    // Verify that actual signal change is detected
    double real_signal = 1e-5; // Larger than noise floor
    FATP_ASSERT_FALSE(approximateEqual(real_signal, setpoint, noise_floor, noise_floor),
                      "Control: Real signal detected");

    return true;
}

// =============================================================================
// BENCHMARK INFRASTRUCTURE
// =============================================================================

class BenchmarkData
{
public:
    static constexpr size_t DATASET_SIZE = 10000;
    std::vector<double> values_near_zero;
    std::vector<double> values_normal;
    std::vector<double> values_large;
    std::vector<double> values_mixed;

    BenchmarkData()
    {
        std::mt19937_64 rng(42);
        std::uniform_real_distribution<double> dist_near_zero(1e-10, 1e-8);
        std::uniform_real_distribution<double> dist_normal(1.0, 10.0);
        std::uniform_real_distribution<double> dist_large(1e6, 1e8);
        std::uniform_real_distribution<double> dist_mixed(1e-10, 1e8);

        values_near_zero.reserve(DATASET_SIZE);
        values_normal.reserve(DATASET_SIZE);
        values_large.reserve(DATASET_SIZE);
        values_mixed.reserve(DATASET_SIZE);

        for (size_t i = 0; i < DATASET_SIZE; ++i)
        {
            values_near_zero.push_back(dist_near_zero(rng));
            values_normal.push_back(dist_normal(rng));
            values_large.push_back(dist_large(rng));
            values_mixed.push_back(dist_mixed(rng));
        }
    }
};

// =============================================================================
// BENCHMARK FUNCTIONS
// =============================================================================
// =============================================================================
// BENCHMARK RUNNER
// =============================================================================

void run_benchmarks()
{
    using namespace fat_p::testing;

    auto& out = *get_test_config().output;
    out << "\n" << colors::cyan() << "Sanity Benchmark:" << colors::reset() << "\n";
    out << "  (benchmarks are provided by benchmark executables; unit tests do not run full benchmarks)\n\n";
}




// =============================================================================
// Sanity Benchmark (kept fast; full benchmark suite remains in run_benchmarks())
// =============================================================================
} // namespace fat_p::testing::floatingpointcomparison

// ============================================================================
// Public Interface (per Fat-P Test Suite Style Guide)
// ============================================================================

namespace fat_p::testing
{

bool test_FloatingPointComparison()
{
    FATP_PRINT_HEADER("FLOATING-POINT COMPARISON - ROBUST CONTROL")
    TestRunner runner;

    std::cout << "\n=== StandardComparisonPolicy ===" << std::endl;
    FATP_RUN_TEST_NS(runner, floatingpointcomparison, standard_basic);
    FATP_RUN_TEST_NS(runner, floatingpointcomparison, standard_boundary_values);
    FATP_RUN_TEST_NS(runner, floatingpointcomparison, standard_negative_values);
    FATP_RUN_TEST_NS(runner, floatingpointcomparison, standard_special_values);
    FATP_RUN_TEST_NS(runner, floatingpointcomparison, standard_zero_comparisons);

    std::cout << "\n=== UlpComparisonPolicy ===" << std::endl;
    FATP_RUN_TEST_NS(runner, floatingpointcomparison, ulp_basic);
    FATP_RUN_TEST_NS(runner, floatingpointcomparison, ulp_exact_boundaries);
    FATP_RUN_TEST_NS(runner, floatingpointcomparison, ulp_negative_values);
    FATP_RUN_TEST_NS(runner, floatingpointcomparison, ulp_subnormals);
    FATP_RUN_TEST_NS(runner, floatingpointcomparison, ulp_subnormal_boundaries);
    FATP_RUN_TEST_NS(runner, floatingpointcomparison, ulp_sign_sensitivity);

    std::cout << "\n=== RelativeComparisonPolicy ===" << std::endl;
    FATP_RUN_TEST_NS(runner, floatingpointcomparison, relative_scale_independence);
    FATP_RUN_TEST_NS(runner, floatingpointcomparison, relative_near_zero_weakness);
    FATP_RUN_TEST_NS(runner, floatingpointcomparison, relative_default_epsilon);
    FATP_RUN_TEST_NS(runner, floatingpointcomparison, relative_zero_comparisons);

    std::cout << "\n=== HybridComparisonPolicy ===" << std::endl;
    FATP_RUN_TEST_NS(runner, floatingpointcomparison, hybrid_robustness);
    FATP_RUN_TEST_NS(runner, floatingpointcomparison, hybrid_near_zero_robustness);
    FATP_RUN_TEST_NS(runner, floatingpointcomparison, hybrid_parameter_flexibility);
    FATP_RUN_TEST_NS(runner, floatingpointcomparison, hybrid_boundary_conditions);

    std::cout << "\n=== Cross-Policy Verification ===" << std::endl;
    FATP_RUN_TEST_NS(runner, floatingpointcomparison, policy_differences);
    FATP_RUN_TEST_NS(runner, floatingpointcomparison, policy_consistency_on_special_values);
    FATP_RUN_TEST_NS(runner, floatingpointcomparison, scale_transition_points);

    std::cout << "\n=== Mixed-Sign Edge Cases ===" << std::endl;
    FATP_RUN_TEST_NS(runner, floatingpointcomparison, mixed_signs_noise_floor);
    FATP_RUN_TEST_NS(runner, floatingpointcomparison, near_zero_crossing_stability);

    std::cout << "\n=== approximateEqual Convenience Function ===" << std::endl;
    FATP_RUN_TEST_NS(runner, floatingpointcomparison, approximate_equal_basic);
    FATP_RUN_TEST_NS(runner, floatingpointcomparison, approximate_equal_types);
    FATP_RUN_TEST_NS(runner, floatingpointcomparison, approximate_equal_zero_handling);

    std::cout << "\n=== Type-Specific Default Epsilons ===" << std::endl;
    FATP_RUN_TEST_NS(runner, floatingpointcomparison, default_epsilons);
    FATP_RUN_TEST_NS(runner, floatingpointcomparison, epsilon_type_correctness);

    std::cout << "\n=== Edge Cases and Stress Tests ===" << std::endl;
    FATP_RUN_TEST_NS(runner, floatingpointcomparison, extreme_values);
    FATP_RUN_TEST_NS(runner, floatingpointcomparison, denormal_numbers);
    FATP_RUN_TEST_NS(runner, floatingpointcomparison, pathological_cases);
    FATP_RUN_TEST_NS(runner, floatingpointcomparison, consecutive_values);

    std::cout << "\n=== Long Double Comprehensive Testing ===" << std::endl;
    FATP_RUN_TEST_NS(runner, floatingpointcomparison, long_double_comprehensive);
    FATP_RUN_TEST_NS(runner, floatingpointcomparison, long_double_precision);

    std::cout << "\n=== Control System Simulation ===" << std::endl;
    FATP_RUN_TEST_NS(runner, floatingpointcomparison, control_system_noise);



    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    bool success = fat_p::testing::test_FloatingPointComparison();

    return success ? 0 : 1;
}
#endif
