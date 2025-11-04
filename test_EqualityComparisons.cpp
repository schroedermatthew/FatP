// test_EqualityComparisons.cpp
// Comprehensive test suite for EqualityComparisons library
// Tests all policies, edge cases, and container support
// FIXED: MSVC-compatible macro usage (extra parentheses around template calls)

#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <array>
#include <tuple>
#include <limits>
#include <cmath>

#include "EqualityComparisons.h"
#include "test_EqualityComparisons.h"
#include "test_Utilities.h"

namespace cpp_utilities::testing {

using namespace cpp_utilities;

// =============================================================================
// Test Suite 1: StandardComparisonPolicy Edge Cases
// =============================================================================

bool test_standard_policy_infinities() {
    std::cout << colors::cyan() << "Testing StandardPolicy with infinities..." 
              << colors::reset() << std::endl;
    
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
    
    std::cout << colors::green() << "  ✓ Infinity handling correct" 
              << colors::reset() << std::endl;
    
    return true;
}

bool test_standard_policy_nans() {
    std::cout << colors::cyan() << "Testing StandardPolicy with NaNs..." 
              << colors::reset() << std::endl;
    
    double nan1 = std::numeric_limits<double>::quiet_NaN();
    double nan2 = std::nan("");
    
    // NaN should never equal anything, even itself (IEEE 754)
    ASSERT_FALSE(areEqual(nan1, nan1), "NaN != NaN (IEEE 754)");
    ASSERT_FALSE(areEqual(nan1, nan2), "Different NaNs not equal");
    ASSERT_FALSE(areEqual(nan1, 0.0), "NaN != finite");
    
    std::cout << colors::green() << "  ✓ NaN handling correct" 
              << colors::reset() << std::endl;
    
    return true;
}

bool test_standard_policy_signed_zeros() {
    std::cout << colors::cyan() << "Testing StandardPolicy with signed zeros..." 
              << colors::reset() << std::endl;
    
    double pos_zero = 0.0;
    double neg_zero = -0.0;
    
    // IEEE 754: +0 == -0
    ASSERT_TRUE(areEqual(pos_zero, neg_zero), "+0 equals -0 (IEEE 754)");
    ASSERT_TRUE(areEqual(neg_zero, pos_zero), "-0 equals +0 (IEEE 754)");
    
    std::cout << colors::green() << "  ✓ Signed zero handling correct" 
              << colors::reset() << std::endl;
    
    return true;
}

bool test_standard_policy_denormals() {
    std::cout << colors::cyan() << "Testing StandardPolicy with denormals..." 
              << colors::reset() << std::endl;
    
    double denorm_min = std::numeric_limits<double>::denorm_min();
    
    // Denormal should equal itself
    ASSERT_TRUE(areEqual(denorm_min, denorm_min), "Denormal equals itself");
    
    // Denormal close to zero should be within default epsilon
    ASSERT_TRUE(areEqual(denorm_min, 0.0), "Denormal close to zero");
    
    std::cout << colors::green() << "  ✓ Denormal handling correct" 
              << colors::reset() << std::endl;
    
    return true;
}

// =============================================================================
// Test Suite 2: UlpComparisonPolicy
// =============================================================================

bool test_ulp_policy_basic() {
    std::cout << colors::cyan() << "Testing UlpPolicy basic comparisons..." 
              << colors::reset() << std::endl;
    
    float a = 1.0f;
    float b = 1.0f + std::numeric_limits<float>::epsilon();
    
    // Within 4 ULPs (default)
    // FIX: Use extra parentheses to protect template arguments from macro expansion
    ASSERT_TRUE((areEqual<float, UlpComparisonPolicy>(a, a)), "Same value");
    ASSERT_TRUE((areEqual<float, UlpComparisonPolicy>(a, b, 4.0)), "Within 4 ULPs");
    
    // More than 4 ULPs apart
    float c = 1.0f + 10.0f * std::numeric_limits<float>::epsilon();
    ASSERT_FALSE((areEqual<float, UlpComparisonPolicy>(a, c, 4.0)), "Beyond 4 ULPs");
    
    std::cout << colors::green() << "  ✓ ULP basic comparisons correct" 
              << colors::reset() << std::endl;
    
    return true;
}

bool test_ulp_policy_subnormals() {
    std::cout << colors::cyan() << "Testing UlpPolicy with subnormals..." 
              << colors::reset() << std::endl;
    
    // Float subnormals
    float denorm_f = std::numeric_limits<float>::denorm_min();
    ASSERT_TRUE((areEqual<float, UlpComparisonPolicy>(denorm_f, denorm_f)), 
                "Float subnormal equals itself");
    
    // Double subnormals
    double denorm_d = std::numeric_limits<double>::denorm_min();
    ASSERT_TRUE((areEqual<double, UlpComparisonPolicy>(denorm_d, denorm_d)), 
                "Double subnormal equals itself");
    
    std::cout << colors::green() << "  ✓ ULP subnormal handling correct" 
              << colors::reset() << std::endl;
    
    return true;
}

bool test_ulp_policy_sign_matters() {
    std::cout << colors::cyan() << "Testing UlpPolicy sign handling..." 
              << colors::reset() << std::endl;
    
    double a = 1.0;
    double b = -1.0;
    
    // Different signs should NOT be equal (even if same magnitude)
    ASSERT_FALSE((areEqual<double, UlpComparisonPolicy>(a, b)), 
                 "Opposite signs not equal");
    
    std::cout << colors::green() << "  ✓ ULP sign handling correct" 
              << colors::reset() << std::endl;
    
    return true;
}

// =============================================================================
// Test Suite 3: RelativeComparisonPolicy
// =============================================================================

bool test_relative_policy_scale_independence() {
    std::cout << colors::cyan() << "Testing RelativePolicy scale independence..." 
              << colors::reset() << std::endl;
    
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
    
    std::cout << colors::green() << "  ✓ Relative policy scale-independent" 
              << colors::reset() << std::endl;
    
    return true;
}

bool test_relative_policy_near_zero_weakness() {
    std::cout << colors::cyan() << "Testing RelativePolicy near-zero weakness..." 
              << colors::reset() << std::endl;
    
    double relEps = 1e-5;
    
    // Near zero, relative policy should accept very small absolute differences
    double a = 1e-100;
    double b = 2e-100;  // 100% relative error, but both near zero
    
    // This demonstrates the weakness: relative error is huge,
    // but absolute difference is tiny
    bool result = areEqual<double, RelativeComparisonPolicy>(a, b, relEps);
    
    std::cout << colors::yellow() << "  ⚠ Near-zero comparison: " << result 
              << " (relative policy weakness)" << colors::reset() << std::endl;
    
    return true;  // Not a failure, just documenting behavior
}

// =============================================================================
// Test Suite 4: HybridComparisonPolicy (RECOMMENDED)
// =============================================================================

bool test_hybrid_policy_robust() {
    std::cout << colors::cyan() << "Testing HybridPolicy robustness..." 
              << colors::reset() << std::endl;
    
    double relEps = 1e-5;
    double absEps = 1e-9;
    
    // Near zero: absolute tolerance handles it
    double a1 = 1e-100;
    double b1 = 1e-100 + 1e-10;
    ASSERT_TRUE((areEqual<double, HybridComparisonPolicy>(a1, b1, relEps, absEps)), 
                "Near-zero handled by absolute");
    
    // Large values: relative tolerance handles it
    double a2 = 1e6;
    double b2 = 1e6 * (1.0 + 0.5e-5);
    ASSERT_TRUE((areEqual<double, HybridComparisonPolicy>(a2, b2, relEps, absEps)), 
                "Large values handled by relative");
    
    std::cout << colors::green() << "  ✓ Hybrid policy handles all scales" 
              << colors::reset() << std::endl;
    
    return true;
}

// =============================================================================
// Test Suite 5: Container Comparisons
// =============================================================================

bool test_vector_comparison() {
    std::cout << colors::cyan() << "Testing vector comparison..." 
              << colors::reset() << std::endl;
    
    std::vector<double> v1 = {1.0, 2.0, 3.0};
    std::vector<double> v2 = {1.0, 2.0, 3.0};
    std::vector<double> v3 = {1.0, 2.0, 3.0 + 50.0 * std::numeric_limits<double>::epsilon()};
    std::vector<double> v4 = {1.0, 2.0};  // Different size
    
    ASSERT_TRUE(areEqual(v1, v2), "Identical vectors");
    ASSERT_TRUE(areEqual(v1, v3), "Vectors within epsilon");
    ASSERT_FALSE(areEqual(v1, v4), "Different size vectors");
    
    std::cout << colors::green() << "  ✓ Vector comparison correct" 
              << colors::reset() << std::endl;
    
    return true;
}

bool test_nested_container_comparison() {
    std::cout << colors::cyan() << "Testing nested container comparison..." 
              << colors::reset() << std::endl;
    
    using NestedVec = std::vector<std::vector<double>>;
    
    NestedVec n1 = {{1.0, 2.0}, {3.0, 4.0}};
    NestedVec n2 = {{1.0, 2.0}, {3.0, 4.0}};
    NestedVec n3 = {{1.0, 2.0}, {3.0, 4.0 + 50.0 * std::numeric_limits<double>::epsilon()}};
    
    ASSERT_TRUE(areEqual(n1, n2), "Identical nested vectors");
    ASSERT_TRUE(areEqual(n1, n3), "Nested vectors within epsilon");
    
    std::cout << colors::green() << "  ✓ Nested container comparison correct" 
              << colors::reset() << std::endl;
    
    return true;
}

bool test_map_comparison() {
    std::cout << colors::cyan() << "Testing map comparison..." 
              << colors::reset() << std::endl;
    
    std::map<std::string, double> m1 = {{"a", 1.0}, {"b", 2.0}};
    std::map<std::string, double> m2 = {{"a", 1.0}, {"b", 2.0}};
    std::map<std::string, double> m3 = {{"a", 1.0}, {"b", 2.0 + 50.0 * std::numeric_limits<double>::epsilon()}};
    std::map<std::string, double> m4 = {{"a", 1.0}};  // Different size
    
    ASSERT_TRUE(areEqual(m1, m2), "Identical maps");
    ASSERT_TRUE(areEqual(m1, m3), "Maps within epsilon");
    ASSERT_FALSE(areEqual(m1, m4), "Different size maps");
    
    std::cout << colors::green() << "  ✓ Map comparison correct" 
              << colors::reset() << std::endl;
    
    return true;
}

// =============================================================================
// Test Suite 6: Pair and Tuple Comparisons
// =============================================================================

bool test_pair_comparison() {
    std::cout << colors::cyan() << "Testing pair comparison..." 
              << colors::reset() << std::endl;
    
    std::pair<double, double> p1 = {1.0, 2.0};
    std::pair<double, double> p2 = {1.0, 2.0};
    std::pair<double, double> p3 = {1.0 + 50.0 * std::numeric_limits<double>::epsilon(), 2.0};
    
    ASSERT_TRUE(areEqual(p1, p2), "Identical pairs");
    ASSERT_TRUE(areEqual(p1, p3), "Pairs within epsilon");
    
    std::cout << colors::green() << "  ✓ Pair comparison correct" 
              << colors::reset() << std::endl;
    
    return true;
}

bool test_tuple_comparison() {
    std::cout << colors::cyan() << "Testing tuple comparison..." 
              << colors::reset() << std::endl;
    
    std::tuple<double, int, std::string> t1 = {1.0, 42, "test"};
    std::tuple<double, int, std::string> t2 = {1.0, 42, "test"};
    std::tuple<double, int, std::string> t3 = {1.0 + 50.0 * std::numeric_limits<double>::epsilon(), 42, "test"};
    std::tuple<double, int, std::string> t4 = {1.0, 42, "fail"};
    
    ASSERT_TRUE(areEqual(t1, t2), "Identical tuples");
    ASSERT_TRUE(areEqual(t1, t3), "Tuples within epsilon");
    ASSERT_FALSE(areEqual(t1, t4), "Different string in tuple");
    
    std::cout << colors::green() << "  ✓ Tuple comparison correct" 
              << colors::reset() << std::endl;
    
    return true;
}

// =============================================================================
// Test Suite 7: Type-Specific Default Epsilons
// =============================================================================

bool test_type_specific_epsilons() {
    std::cout << colors::cyan() << "Testing type-specific default epsilons..." 
              << colors::reset() << std::endl;
    
    // Float with float epsilon
    float f1 = 1.0f;
    float f2 = 1.0f + 50.0f * std::numeric_limits<float>::epsilon();
    ASSERT_TRUE(areEqual(f1, f2), "Float within default epsilon");
    
    // Double with double epsilon
    double d1 = 1.0;
    double d2 = 1.0 + 50.0 * std::numeric_limits<double>::epsilon();
    ASSERT_TRUE(areEqual(d1, d2), "Double within default epsilon");
    
    std::cout << colors::green() << "  ✓ Type-specific epsilons correct" 
              << colors::reset() << std::endl;
    
    return true;
}

// =============================================================================
// Test Suite 8: approximateEqual Convenience Function
// =============================================================================

bool test_approximate_equal_convenience() {
    std::cout << colors::cyan() << "Testing approximateEqual convenience function..." 
              << colors::reset() << std::endl;
    
    double a = 1.0;
    double b = 1.0 + 50.0 * std::numeric_limits<double>::epsilon();
    
    // Uses HybridComparisonPolicy by default
    bool result = approximateEqual(a, b);
    ASSERT_TRUE(result, "approximateEqual uses robust hybrid policy");
    
    std::cout << colors::green() << "  ✓ approximateEqual convenience function works" 
              << colors::reset() << std::endl;
    
    return true;
}

// =============================================================================
// Test Suite 9: Integer and Non-Floating Comparisons
// =============================================================================

bool test_integer_comparison() {
    std::cout << colors::cyan() << "Testing integer comparison..." 
              << colors::reset() << std::endl;
    
    int a = 42;
    int b = 42;
    int c = 43;
    
    ASSERT_TRUE(areEqual(a, b), "Equal integers");
    ASSERT_FALSE(areEqual(a, c), "Unequal integers");
    
    std::cout << colors::green() << "  ✓ Integer comparison correct" 
              << colors::reset() << std::endl;
    
    return true;
}

bool test_string_comparison() {
    std::cout << colors::cyan() << "Testing string comparison..." 
              << colors::reset() << std::endl;
    
    std::string s1 = "hello";
    std::string s2 = "hello";
    std::string s3 = "world";
    
    ASSERT_TRUE(areEqual(s1, s2), "Equal strings");
    ASSERT_FALSE(areEqual(s1, s3), "Unequal strings");
    
    std::cout << colors::green() << "  ✓ String comparison correct" 
              << colors::reset() << std::endl;
    
    return true;
}

// =============================================================================
// Main Entry Point (called from test runner)
// =============================================================================

bool test_EqualityComparisons() {
    std::cout << colors::bold() << "\n=== EqualityComparisons Test Suite ===" 
              << colors::reset() << "\n\n";
    
    TestRunner runner;
    
    // Run all test suites
    std::cout << colors::bold() << "=== StandardComparisonPolicy Tests ===" 
              << colors::reset() << "\n";
    runner.run_test("Infinities", test_standard_policy_infinities);
    runner.run_test("NaNs", test_standard_policy_nans);
    runner.run_test("Signed Zeros", test_standard_policy_signed_zeros);
    runner.run_test("Denormals", test_standard_policy_denormals);
    
    std::cout << "\n" << colors::bold() << "=== UlpComparisonPolicy Tests ===" 
              << colors::reset() << "\n";
    runner.run_test("Basic ULP", test_ulp_policy_basic);
    runner.run_test("ULP Subnormals", test_ulp_policy_subnormals);
    runner.run_test("ULP Sign", test_ulp_policy_sign_matters);
    
    std::cout << "\n" << colors::bold() << "=== RelativeComparisonPolicy Tests ===" 
              << colors::reset() << "\n";
    runner.run_test("Scale Independence", test_relative_policy_scale_independence);
    runner.run_test("Near-Zero Weakness", test_relative_policy_near_zero_weakness);
    
    std::cout << "\n" << colors::bold() << "=== HybridComparisonPolicy Tests ===" 
              << colors::reset() << "\n";
    runner.run_test("Hybrid Robustness", test_hybrid_policy_robust);
    
    std::cout << "\n" << colors::bold() << "=== Container Comparison Tests ===" 
              << colors::reset() << "\n";
    runner.run_test("Vector", test_vector_comparison);
    runner.run_test("Nested Containers", test_nested_container_comparison);
    runner.run_test("Map", test_map_comparison);
    
    std::cout << "\n" << colors::bold() << "=== Pair and Tuple Tests ===" 
              << colors::reset() << "\n";
    runner.run_test("Pair", test_pair_comparison);
    runner.run_test("Tuple", test_tuple_comparison);
    
    std::cout << "\n" << colors::bold() << "=== Type-Specific Tests ===" 
              << colors::reset() << "\n";
    runner.run_test("Type-Specific Epsilons", test_type_specific_epsilons);
    runner.run_test("approximateEqual", test_approximate_equal_convenience);
    
    std::cout << "\n" << colors::bold() << "=== Non-Floating Types Tests ===" 
              << colors::reset() << "\n";
    runner.run_test("Integer", test_integer_comparison);
    runner.run_test("String", test_string_comparison);
    
    // Print summary
    int failed = runner.print_summary();
    
    return (failed == 0);
}

} // namespace cpp_utilities::testing
