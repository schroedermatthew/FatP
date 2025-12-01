// test_EqualityComparisons.cpp
// Comprehensive test suite for EqualityComparisons library
// Tests all policies, edge cases, and container support
// 
// NOTE: This test file now tests the complete EqualityComparisons framework,
// which includes FloatingPointComparison.h for float comparison and
// EqualityComparisons.h for container comparison.
//
// For lightweight floating-point-only tests, see test_FloatingPointComparison.cpp
//
// MSVC-compatible macro usage (extra parentheses around template calls)

#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <array>
#include <tuple>
#include <limits>
#include <cmath>

#include "EqualityComparisons.h"
#include "FatPTest.h"

namespace fat_p::testing {

// =============================================================================
// Test Suite 1: StandardComparisonPolicy Edge Cases
// =============================================================================

bool test_standard_policy_infinities() {
    
    double pos_inf = std::numeric_limits<double>::infinity();
    double neg_inf = -std::numeric_limits<double>::infinity();
    
    // Same sign infinities should be equal
    ASSERT_TRUE(areEqual(pos_inf, pos_inf), "Positive infinity equals itself");
    ASSERT_TRUE(areEqual(neg_inf, neg_inf), "Negative infinity equals itself");
    
    // Different sign infinities should NOT be equal
    ASSERT_FALSE(areEqual(pos_inf, neg_inf), "Opposite infinities not equal");
    
    // Infinity compared to finite should NOT be equal
    ASSERT_FALSE(areEqual(pos_inf, 1e308), "Infinity != finite");
    ASSERT_FALSE(areEqual(1e308, pos_inf), "Finite != infinity");
    
    return true;
}

bool test_standard_policy_nans() {
    
    double nan1 = std::numeric_limits<double>::quiet_NaN();
    double nan2 = std::nan("");
    
    // NaN should never equal anything, even itself (IEEE 754)
    ASSERT_FALSE(areEqual(nan1, nan1), "NaN != NaN (IEEE 754)");
    ASSERT_FALSE(areEqual(nan1, nan2), "Different NaNs not equal");
    ASSERT_FALSE(areEqual(nan1, 0.0), "NaN != finite");
    
    return true;
}

bool test_standard_policy_signed_zeros() {
    
    double pos_zero = 0.0;
    double neg_zero = -0.0;
    
    // IEEE 754: +0 == -0
    ASSERT_TRUE(areEqual(pos_zero, neg_zero), "+0 equals -0 (IEEE 754)");
    ASSERT_TRUE(areEqual(neg_zero, pos_zero), "-0 equals +0 (IEEE 754)");
        
    return true;
}

bool test_standard_policy_denormals() {
    
    double denorm_min = std::numeric_limits<double>::denorm_min();
    
    // Denormal should equal itself
    ASSERT_TRUE(areEqual(denorm_min, denorm_min), "Denormal equals itself");
    
    // Denormal close to zero should be within default epsilon
    ASSERT_TRUE(areEqual(denorm_min, 0.0), "Denormal close to zero");
        
    return true;
}

// =============================================================================
// Test Suite 2: UlpComparisonPolicy
// =============================================================================

bool test_ulp_policy_basic() {
    
    float a = 1.0f;
    float b = 1.0f + std::numeric_limits<float>::epsilon();
    
    // Within 4 ULPs (default)
    // Use extra parentheses to protect template arguments from macro expansion
    ASSERT_TRUE((areEqual<float, UlpComparisonPolicy>(a, a)), "Same value");
    ASSERT_TRUE((areEqual<float, UlpComparisonPolicy>(a, b, 4.0f)), "Within 4 ULPs");
    
    // More than 4 ULPs apart
    float c = 1.0f + 10.0f * std::numeric_limits<float>::epsilon();
    ASSERT_FALSE((areEqual<float, UlpComparisonPolicy>(a, c, 4.0f)), "Beyond 4 ULPs");
    
    return true;
}

bool test_ulp_policy_subnormals() {

    // Float subnormals
    float denorm_f = std::numeric_limits<float>::denorm_min();
    ASSERT_TRUE((areEqual<float, UlpComparisonPolicy>(denorm_f, denorm_f)), 
                "Float subnormal equals itself");
    
    // Double subnormals
    double denorm_d = std::numeric_limits<double>::denorm_min();
    ASSERT_TRUE((areEqual<double, UlpComparisonPolicy>(denorm_d, denorm_d)), 
                "Double subnormal equals itself");
    
    return true;
}

bool test_ulp_policy_sign_matters() {
    
    double a = 1.0;
    double b = -1.0;
    
    // Different signs should NOT be equal (even if same magnitude)
    ASSERT_FALSE((areEqual<double, UlpComparisonPolicy>(a, b)), 
                 "Opposite signs not equal");
        
    return true;
}

// =============================================================================
// Test Suite 3: RelativeComparisonPolicy
// =============================================================================

bool test_relative_policy_scale_independence() {
    
    double relEps = 1e-5;
    
    // Small values
    double a1 = 1.0;
    double b1 = 1.0 + 0.5e-5;  // 0.5% relative error
    ASSERT_TRUE((areEqual<double, RelativeComparisonPolicy>(a1, b1, relEps)), 
                "Small values within tolerance");
    
    // Large values (same relative error)
    double a2 = 1e6;
    double b2 = 1e6 + 0.5;  // 0.5% relative error
    ASSERT_TRUE((areEqual<double, RelativeComparisonPolicy>(a2, b2, relEps)), 
                "Large values within tolerance");
        
    return true;
}

bool test_relative_policy_near_zero_weakness() {
    
    double relEps = 1e-5;
    
    // Near zero, relative policy should accept very small absolute differences
    double a = 1e-100;
    double b = 2e-100;  // 100% relative error, but both near zero
    
    // This demonstrates the weakness: relative error is huge,
    // but absolute difference is tiny
    (void)areEqual<double, RelativeComparisonPolicy>(a, b, relEps);
        
    return true;
}

// =============================================================================
// Test Suite 4: HybridComparisonPolicy
// =============================================================================

bool test_hybrid_policy_robust() {
 
    double relEps = 1e-5;
    double absEps = 1e-8;
    
    // Near zero - absolute tolerance should dominate
    double a1 = 1e-10;
    double b1 = 2e-10;
    ASSERT_TRUE((areEqual<double, HybridComparisonPolicy>(a1, b1, relEps, absEps)), 
                "Near zero handled by absolute tolerance");
    
    // Large values - relative tolerance should dominate
    double a2 = 1e6;
    double b2 = 1e6 + 0.5;
    ASSERT_TRUE((areEqual<double, HybridComparisonPolicy>(a2, b2, relEps, absEps)), 
                "Large values handled by relative tolerance");
    
    return true;
}

// =============================================================================
// Test Suite 5: Container Comparisons
// =============================================================================

bool test_vector_comparison() {
    std::vector<double> v1 = {1.0, 2.0, 3.0};
    std::vector<double> v2 = {1.0, 2.0, 3.0};
    std::vector<double> v3 = {1.0, 2.0, 3.0 + 50.0 * std::numeric_limits<double>::epsilon()};
    std::vector<double> v4 = {1.0, 2.0};  // Different size
    
    ASSERT_TRUE(areEqual(v1, v2), "Identical vectors");
    ASSERT_TRUE(areEqual(v1, v3), "Vectors within epsilon");
    ASSERT_FALSE(areEqual(v1, v4), "Different size vectors");
    
    return true;
}

bool test_nested_container_comparison() {
    
    std::vector<std::vector<double>> n1 = {{1.0, 2.0}, {3.0, 4.0}};
    std::vector<std::vector<double>> n2 = {{1.0, 2.0}, {3.0, 4.0}};
    std::vector<std::vector<double>> n3 = {
        {1.0, 2.0}, 
        {3.0, 4.0 + 50.0 * std::numeric_limits<double>::epsilon()}
    };
    
    ASSERT_TRUE(areEqual(n1, n2), "Identical nested vectors");
    ASSERT_TRUE(areEqual(n1, n3), "Nested vectors within epsilon");
    
    return true;
}

bool test_map_comparison() {
    
    std::map<std::string, double> m1 = {{"a", 1.0}, {"b", 2.0}};
    std::map<std::string, double> m2 = {{"a", 1.0}, {"b", 2.0}};
    std::map<std::string, double> m3 = {{"a", 1.0}, {"b", 2.0 + 50.0 * std::numeric_limits<double>::epsilon()}};
    std::map<std::string, double> m4 = {{"a", 1.0}};  // Different size
    
    ASSERT_TRUE(areEqual(m1, m2), "Identical maps");
    ASSERT_TRUE(areEqual(m1, m3), "Maps within epsilon");
    ASSERT_FALSE(areEqual(m1, m4), "Different size maps");
        
    return true;
}

// =============================================================================
// Test Suite 6: Pair and Tuple Comparisons
// =============================================================================

bool test_pair_comparison() {
    
    std::pair<double, double> p1 = {1.0, 2.0};
    std::pair<double, double> p2 = {1.0, 2.0};
    std::pair<double, double> p3 = {1.0 + 50.0 * std::numeric_limits<double>::epsilon(), 2.0};
    
    ASSERT_TRUE(areEqual(p1, p2), "Identical pairs");
    ASSERT_TRUE(areEqual(p1, p3), "Pairs within epsilon");
        
    return true;
}

bool test_tuple_comparison() {
    
    std::tuple<double, int, std::string> t1 = {1.0, 42, "test"};
    std::tuple<double, int, std::string> t2 = {1.0, 42, "test"};
    std::tuple<double, int, std::string> t3 = {1.0 + 50.0 * std::numeric_limits<double>::epsilon(), 42, "test"};
    std::tuple<double, int, std::string> t4 = {1.0, 42, "fail"};
    
    ASSERT_TRUE(areEqual(t1, t2), "Identical tuples");
    ASSERT_TRUE(areEqual(t1, t3), "Tuples within epsilon");
    ASSERT_FALSE(areEqual(t1, t4), "Different string in tuple");
    
    std::cout << colors::green() << "  âœ“ Tuple comparison correct" 
              << colors::reset() << std::endl;
    
    return true;
}

// =============================================================================
// Test Suite 7: Type-Specific Default Epsilons
// =============================================================================

bool test_type_specific_epsilons() {
    
    // Float with float epsilon
    float f1 = 1.0f;
    float f2 = 1.0f + 50.0f * std::numeric_limits<float>::epsilon();
    ASSERT_TRUE(areEqual(f1, f2), "Float within default epsilon");
    
    // Double with double epsilon
    double d1 = 1.0;
    double d2 = 1.0 + 50.0 * std::numeric_limits<double>::epsilon();
    ASSERT_TRUE(areEqual(d1, d2), "Double within default epsilon");
    
    
    return true;
}

// =============================================================================
// Test Suite 8: approximateEqual Convenience Function
// =============================================================================

bool test_approximate_equal_convenience() {
    
    double a = 1.0;
    double b = 1.0 + 50.0 * std::numeric_limits<double>::epsilon();
    
    // Uses HybridComparisonPolicy by default
    bool result = approximateEqual(a, b);
    ASSERT_TRUE(result, "approximateEqual uses robust hybrid policy");
    
    
    return true;
}

// =============================================================================
// Test Suite 9: floatEqual New Function (from FloatingPointComparison.h)
// =============================================================================

bool test_float_equal_basic() {
    
    // Basic usage with default policy - use values within default epsilon
    double a = 1.0;
    double b = 1.0 + 50.0 * std::numeric_limits<double>::epsilon();
    ASSERT_TRUE(floatEqual(a, b), "floatEqual with defaults");
    
    // With custom epsilon for larger differences
    ASSERT_TRUE(floatEqual(1.0, 1.1, 0.2), "floatEqual with custom epsilon");
    ASSERT_FALSE(floatEqual(1.0, 1.1, 0.05), "floatEqual outside epsilon");
        
    return true;
}

bool test_float_equal_with_policies() {
    
    double a = 1.0;
    double b = 1.0 + std::numeric_limits<double>::epsilon();
    
    // Standard policy
    bool std_result = floatEqual<double, StandardComparisonPolicy>(a, b);
    ASSERT_TRUE(std_result, "floatEqual with Standard policy");
    
    // ULP policy
    bool ulp_result = floatEqual<double, UlpComparisonPolicy>(a, b, 4.0);
    ASSERT_TRUE(ulp_result, "floatEqual with ULP policy");
    
    // Relative policy
    bool rel_result = floatEqual<double, RelativeComparisonPolicy>(a, b, 1e-9);
    ASSERT_TRUE(rel_result, "floatEqual with Relative policy");
    
    // Hybrid policy
    bool hyb_result = floatEqual<double, HybridComparisonPolicy>(a, b, 1e-9, 1e-9);
    ASSERT_TRUE(hyb_result, "floatEqual with Hybrid policy");
        
    return true;
}

// =============================================================================
// Test Suite 10: Integer and Non-Floating Comparisons
// =============================================================================

bool test_integer_comparison() {

    int a = 42;
    int b = 42;
    int c = 43;
    
    ASSERT_TRUE(areEqual(a, b), "Equal integers");
    ASSERT_FALSE(areEqual(a, c), "Unequal integers");
        
    return true;
}

bool test_string_comparison() {
    
    std::string s1 = "hello";
    std::string s2 = "hello";
    std::string s3 = "world";
    
    ASSERT_TRUE(areEqual(s1, s2), "Equal strings");
    ASSERT_FALSE(areEqual(s1, s3), "Unequal strings");
        
    return true;
}

// =============================================================================
// Main Entry Point (called from test runner)
// =============================================================================

bool test_EqualityComparisons() {
    PRINT_HEADER(EQUALITY COMPARISONS)

    TestRunner runner;
    
    // Run all test suites
    RUN_TEST(runner, standard_policy_infinities);
    RUN_TEST(runner, standard_policy_nans);
    RUN_TEST(runner, standard_policy_signed_zeros);
    RUN_TEST(runner, standard_policy_denormals);
    
    RUN_TEST(runner, ulp_policy_basic);
    RUN_TEST(runner, ulp_policy_subnormals);
    RUN_TEST(runner, ulp_policy_sign_matters);
    
    RUN_TEST(runner, relative_policy_scale_independence);
    RUN_TEST(runner, relative_policy_near_zero_weakness);

    RUN_TEST(runner, hybrid_policy_robust);
    
    RUN_TEST(runner, vector_comparison);
    RUN_TEST(runner, nested_container_comparison);
    RUN_TEST(runner, map_comparison);
    
    RUN_TEST(runner, pair_comparison);
    RUN_TEST(runner, tuple_comparison);
    
    RUN_TEST(runner, type_specific_epsilons);
    RUN_TEST(runner, approximate_equal_convenience);
    
    RUN_TEST(runner, float_equal_basic);
    RUN_TEST(runner, float_equal_with_policies);
    
    RUN_TEST(runner, integer_comparison);
    RUN_TEST(runner, string_comparison);
    
    // Print summary
    int failed = runner.print_summary();
    
    return (failed == 0);
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_EqualityComparisons() ? 0 : 1;
}
#endif

