/**
 * @file test_EqualityAny.cpp
 * @brief Comprehensive test suite for fat_p::EqualityAny
 *
 * Tests all features including:
 * - Basic std::any comparison with all policies
 * - Type registry functionality
 * - Nested std::any comparison
 * - Recursion depth limiting
 * - Empty std::any handling
 * - Explicit epsilon overloads
 * - Pre-registered types
 *
 * REGRESSION TESTS:
 * - Nested empty std::any returns EQUAL (not bad_any_cast)
 * - Depth guard ordering is correct
 * - kMaxAnyRecursionDepth levels of nesting permitted
 */
/*
FATP_META:
  meta_version: 1
  component: EqualityAny
  file_role: test
  path: tests/test_EqualityAny.cpp
  namespace: fat_p
  summary: "Unit tests for EqualityAny."
  related:
    docs_search: "EqualityAny"
    headers:
      - fat_p/EqualityAny.h
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

#include <any>
#include <atomic>
#include <cstddef>
#include <deque>
#include <iostream>
#include <limits>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include "EqualityAny.h"
#include "FatPTest.h"

namespace fat_p::testing::equalityany
{

// ============================================================================
// Test Constants
// ============================================================================

constexpr double kEps = std::numeric_limits<double>::epsilon();

// ============================================================================
// Test Suite 1: Basic std::any Comparison
// ============================================================================

FATP_TEST_CASE(basic_int)
{
    std::any a1 = 42;
    std::any a2 = 42;
    std::any a3 = 99;

    FATP_ASSERT_TRUE(areEqual(a1, a2), "Same int values in any");
    FATP_ASSERT_FALSE(areEqual(a1, a3), "Different int values in any");

    return true;
}

FATP_TEST_CASE(basic_double)
{
    std::any a1 = 1.0;
    std::any a2 = 1.0;
    std::any a3 = 1.0 + 50.0 * kEps;
    std::any a4 = 999.0;

    FATP_ASSERT_TRUE(areEqual(a1, a2), "Same double values in any");
    FATP_ASSERT_TRUE(areEqual(a1, a3), "Double values within epsilon");
    FATP_ASSERT_FALSE(areEqual(a1, a4), "Different double values in any");

    return true;
}

FATP_TEST_CASE(basic_string)
{
    std::any a1 = std::string("hello");
    std::any a2 = std::string("hello");
    std::any a3 = std::string("world");

    FATP_ASSERT_TRUE(areEqual(a1, a2), "Same string values in any");
    FATP_ASSERT_FALSE(areEqual(a1, a3), "Different string values in any");

    return true;
}

FATP_TEST_CASE(type_mismatch)
{
    std::any a1 = 42;
    std::any a2 = 42.0;
    std::any a3 = std::string("42");

    FATP_ASSERT_FALSE(areEqual(a1, a2), "int vs double type mismatch");
    FATP_ASSERT_FALSE(areEqual(a1, a3), "int vs string type mismatch");

    return true;
}

// ============================================================================
// Test Suite 2: Empty std::any Handling
// ============================================================================

FATP_TEST_CASE(both_empty)
{
    std::any a1;
    std::any a2;

    FATP_ASSERT_TRUE(areEqual(a1, a2), "Both empty any values are equal");

    return true;
}

FATP_TEST_CASE(one_empty)
{
    std::any a1;
    std::any a2 = 42;

    FATP_ASSERT_FALSE(areEqual(a1, a2), "Empty vs non-empty any");
    FATP_ASSERT_FALSE(areEqual(a2, a1), "Non-empty vs empty any");

    return true;
}

// ============================================================================
// Test Suite 3: Nested std::any (REGRESSION TESTS)
// ============================================================================
// These tests verify correct handling of nested std::any values.

FATP_TEST_CASE(nested_empty_any_regression)
{
    // REGRESSION: Nested empty any must compare equal, not throw bad_any_cast
    std::any outer1, outer2;
    outer1.emplace<std::any>(); // outer holds empty inner
    outer2.emplace<std::any>(); // outer holds empty inner

    // Verify the structure
    FATP_ASSERT_TRUE(outer1.has_value(), "Outer has value (contains an any)");
    const std::any& inner = std::any_cast<const std::any&>(outer1);
    FATP_ASSERT_FALSE(inner.has_value(), "Inner is empty");

    // The actual comparison
    bool result = areEqual<StandardComparisonPolicy>(outer1, outer2);
    FATP_ASSERT_TRUE(result, "REGRESSION: Nested empty any must compare EQUAL");

    return true;
}

FATP_TEST_CASE(nested_any_with_values)
{
    std::any outer1, outer2, outer3;
    outer1.emplace<std::any>(42);
    outer2.emplace<std::any>(42);
    outer3.emplace<std::any>(99);

    FATP_ASSERT_TRUE(areEqual<StandardComparisonPolicy>(outer1, outer2), "Nested any with same int value");
    FATP_ASSERT_FALSE(areEqual<StandardComparisonPolicy>(outer1, outer3), "Nested any with different int value");

    return true;
}

FATP_TEST_CASE(nested_any_type_mismatch_no_throw)
{
    // REGRESSION: Ensure type mismatch in nested any returns false, not throws
    std::any outer1, outer2;
    outer1.emplace<std::any>(42);   // int
    outer2.emplace<std::any>(42.0); // double

    bool result = areEqual<StandardComparisonPolicy>(outer1, outer2);
    FATP_ASSERT_FALSE(result, "Nested any with type mismatch (int vs double) returns false");

    return true;
}

FATP_TEST_CASE(nested_any_empty_vs_value_no_throw)
{
    // REGRESSION: Ensure empty vs value in nested any returns false, not throws
    std::any outer1, outer2;
    outer1.emplace<std::any>();   // empty inner
    outer2.emplace<std::any>(42); // int inner

    bool result = areEqual<StandardComparisonPolicy>(outer1, outer2);
    FATP_ASSERT_FALSE(result, "Nested any: empty vs value returns false");

    return true;
}

FATP_TEST_CASE(deeply_nested_any)
{
    // 3 levels: any -> any -> any -> int
    std::any level3_1 = 42;
    std::any level2_1;
    level2_1.emplace<std::any>(level3_1);
    std::any level1_1;
    level1_1.emplace<std::any>(level2_1);

    std::any level3_2 = 42;
    std::any level2_2;
    level2_2.emplace<std::any>(level3_2);
    std::any level1_2;
    level1_2.emplace<std::any>(level2_2);

    std::any level3_3 = 99;
    std::any level2_3;
    level2_3.emplace<std::any>(level3_3);
    std::any level1_3;
    level1_3.emplace<std::any>(level2_3);

    FATP_ASSERT_TRUE(areEqual<StandardComparisonPolicy>(level1_1, level1_2), "3-level nesting with same value");
    FATP_ASSERT_FALSE(areEqual<StandardComparisonPolicy>(level1_1, level1_3), "3-level nesting with different value");

    return true;
}

// ============================================================================
// Test Suite 4: Recursion Depth Limiting
// ============================================================================
// Contract: Each areEqual(any,any) call increments depth before the check.
// Check is `depth > kMaxAnyRecursionDepth` (default 10).
// So depth 1-10 are valid (up to 9 levels of any-in-any nesting).
// Depth 11+ fails (10+ levels of nesting).

FATP_TEST_CASE(depth_limit_safe)
{
    // 8 levels of nesting -> 9 areEqual calls -> depth reaches 9 -> valid
    constexpr size_t safeNesting = kMaxAnyRecursionDepth - 2;
    std::any current1 = 42;
    std::any current2 = 42;

    for (size_t i = 0; i < safeNesting; ++i)
    {
        std::any next1, next2;
        next1.emplace<std::any>(std::move(current1));
        next2.emplace<std::any>(std::move(current2));
        current1 = std::move(next1);
        current2 = std::move(next2);
    }

    bool result = areEqual<StandardComparisonPolicy>(current1, current2);
    FATP_ASSERT_TRUE(result, "8 levels of nesting (depth 9) should succeed");

    return true;
}

FATP_TEST_CASE(depth_limit_boundary)
{
    // 9 levels of nesting -> 10 areEqual calls -> depth reaches 10 -> valid (last valid)
    constexpr size_t boundaryNesting = kMaxAnyRecursionDepth - 1;
    std::any current1 = 42;
    std::any current2 = 42;

    for (size_t i = 0; i < boundaryNesting; ++i)
    {
        std::any next1, next2;
        next1.emplace<std::any>(std::move(current1));
        next2.emplace<std::any>(std::move(current2));
        current1 = std::move(next1);
        current2 = std::move(next2);
    }

    bool result = areEqual<StandardComparisonPolicy>(current1, current2);
    FATP_ASSERT_TRUE(result, "9 levels of nesting (depth 10) should succeed (boundary)");

    return true;
}

FATP_TEST_CASE(depth_limit_exceeded)
{
    // 10 levels of nesting -> 11 areEqual calls -> depth reaches 11 -> FAILS
    constexpr size_t excessiveNesting = kMaxAnyRecursionDepth;
    std::any current1 = 42;
    std::any current2 = 42;

    for (size_t i = 0; i < excessiveNesting; ++i)
    {
        std::any next1, next2;
        next1.emplace<std::any>(std::move(current1));
        next2.emplace<std::any>(std::move(current2));
        current1 = std::move(next1);
        current2 = std::move(next2);
    }

    bool result = areEqual<StandardComparisonPolicy>(current1, current2);
    FATP_ASSERT_FALSE(result, "10 levels of nesting (depth 11) should fail");

    return true;
}

// ============================================================================
// Test Suite 5: Explicit Epsilon Overloads
// ============================================================================

FATP_TEST_CASE(explicit_single_epsilon)
{
    std::any a1 = 1.0;
    std::any a2 = 1.1;

    FATP_ASSERT_FALSE(areEqual<StandardComparisonPolicy>(a1, a2, 0.05), "1.0 vs 1.1 with eps=0.05 should differ");
    FATP_ASSERT_TRUE(areEqual<StandardComparisonPolicy>(a1, a2, 0.2), "1.0 vs 1.1 with eps=0.2 should be equal");

    return true;
}

FATP_TEST_CASE(explicit_two_epsilon)
{
    std::any a1 = 1.0;
    std::any a2 = 1.1;

    // HybridComparisonPolicy with relEps=0.2, absEps=0.2
    FATP_ASSERT_TRUE((areEqual<HybridComparisonPolicy>(a1, a2, 0.2, 0.2)), "Hybrid with large epsilon should match");

    // HybridComparisonPolicy with tight tolerances
    FATP_ASSERT_FALSE((areEqual<HybridComparisonPolicy>(a1, a2, 0.01, 0.01)),
                      "Hybrid with tight epsilon should differ");

    return true;
}

// ============================================================================
// Test Suite 5b: Epsilon Propagation for Container-in-Any
// ============================================================================
// These tests verify that epsilon flows through to contained doubles
// when comparing registered container types held in std::any.

FATP_TEST_CASE(epsilon_propagation_vector_double)
{
    // Test that epsilon propagates into vector<double> held in any
    constexpr double tiny = 1e-9;
    std::any a1 = std::vector<double>{1.0, 2.0, 3.0};
    std::any a2 = std::vector<double>{1.0 + tiny, 2.0, 3.0};

    // With large epsilon, should be equal
    FATP_ASSERT_TRUE(areEqual<StandardComparisonPolicy>(a1, a2, 1e-6),
                     "vector<double> in any: within epsilon should be equal");

    // With tiny epsilon, should differ
    FATP_ASSERT_FALSE(areEqual<StandardComparisonPolicy>(a1, a2, 1e-12),
                      "vector<double> in any: outside epsilon should differ");

    return true;
}

FATP_TEST_CASE(epsilon_propagation_deque_double)
{
    // Test that epsilon propagates into deque<double> held in any
    constexpr double tiny = 1e-9;
    std::any a1 = std::deque<double>{1.0, 2.0};
    std::any a2 = std::deque<double>{1.0 + tiny, 2.0};

    FATP_ASSERT_TRUE(areEqual<StandardComparisonPolicy>(a1, a2, 1e-6),
                     "deque<double> in any: within epsilon should be equal");

    FATP_ASSERT_FALSE(areEqual<StandardComparisonPolicy>(a1, a2, 1e-12),
                      "deque<double> in any: outside epsilon should differ");

    return true;
}

FATP_TEST_CASE(epsilon_propagation_tuple_double)
{
    // Test that epsilon propagates into tuple<double,double,double> held in any
    constexpr double tiny = 1e-9;
    using TupleT = std::tuple<double, double, double>;
    std::any a1 = TupleT{1.0, 2.0, 3.0};
    std::any a2 = TupleT{1.0 + tiny, 2.0, 3.0};

    FATP_ASSERT_TRUE(areEqual<StandardComparisonPolicy>(a1, a2, 1e-6),
                     "tuple<double> in any: within epsilon should be equal");

    FATP_ASSERT_FALSE(areEqual<StandardComparisonPolicy>(a1, a2, 1e-12),
                      "tuple<double> in any: outside epsilon should differ");

    return true;
}

FATP_TEST_CASE(epsilon_propagation_nested_vector_any)
{
    // Test epsilon flows through vector<any> containing any(vector<double>)
    constexpr double tiny = 1e-9;

    std::vector<std::any> v1;
    v1.emplace_back(std::vector<double>{1.0, 2.0});

    std::vector<std::any> v2;
    v2.emplace_back(std::vector<double>{1.0 + tiny, 2.0});

    // Explicitly specify policy to lock down test intent
    FATP_ASSERT_TRUE((areEqual<std::vector<std::any>, StandardComparisonPolicy>(v1, v2, 1e-6)),
                     "nested vector<any> containing vector<double>: within epsilon");

    FATP_ASSERT_FALSE((areEqual<std::vector<std::any>, StandardComparisonPolicy>(v1, v2, 1e-12)),
                      "nested vector<any> containing vector<double>: outside epsilon");

    return true;
}

// ============================================================================
// Test Suite 6: All Comparison Policies
// ============================================================================

FATP_TEST_CASE(standard_policy)
{
    std::any a1 = 1.0;
    std::any a2 = 1.0 + 50.0 * kEps;

    FATP_ASSERT_TRUE((areEqual<StandardComparisonPolicy>(a1, a2)), "Standard policy within default epsilon");

    return true;
}

FATP_TEST_CASE(ulp_policy)
{
    std::any a1 = 1.0;
    std::any a2 = 1.0 + kEps;

    FATP_ASSERT_TRUE((areEqual<UlpComparisonPolicy>(a1, a2, 4.0)), "ULP policy within 4 ULPs");

    return true;
}

FATP_TEST_CASE(relative_policy)
{
    std::any a1 = 1.0;
    std::any a2 = 1.0 + 0.5e-5;

    FATP_ASSERT_TRUE((areEqual<RelativeComparisonPolicy>(a1, a2, 1e-5)), "Relative policy within tolerance");

    return true;
}

FATP_TEST_CASE(hybrid_policy)
{
    std::any a1 = 1.0;
    std::any a2 = 1.0 + 50.0 * kEps;

    FATP_ASSERT_TRUE((areEqual<HybridComparisonPolicy>(a1, a2, 1e-5, 1e-8)), "Hybrid policy within tolerance");

    return true;
}

// ============================================================================
// Test Suite 7: Pre-Registered Container Types
// ============================================================================

FATP_TEST_CASE(vector_int_in_any)
{
    std::any a1 = std::vector<int>{1, 2, 3};
    std::any a2 = std::vector<int>{1, 2, 3};
    std::any a3 = std::vector<int>{1, 2, 4};

    FATP_ASSERT_TRUE(areEqual(a1, a2), "Same vector<int> in any");
    FATP_ASSERT_FALSE(areEqual(a1, a3), "Different vector<int> in any");

    return true;
}

FATP_TEST_CASE(vector_double_in_any)
{
    std::any a1 = std::vector<double>{1.0, 2.0, 3.0};
    std::any a2 = std::vector<double>{1.0, 2.0, 3.0};
    std::any a3 = std::vector<double>{1.0, 2.0, 9.0};

    FATP_ASSERT_TRUE(areEqual(a1, a2), "Same vector<double> in any");
    FATP_ASSERT_FALSE(areEqual(a1, a3), "Different vector<double> in any");

    return true;
}

FATP_TEST_CASE(deque_double_in_any)
{
    std::any a1 = std::deque<double>{1.0, 2.0};
    std::any a2 = std::deque<double>{1.0, 2.0};
    std::any a3 = std::deque<double>{1.0, 9.0};

    FATP_ASSERT_TRUE(areEqual(a1, a2), "Same deque<double> in any");
    FATP_ASSERT_FALSE(areEqual(a1, a3), "Different deque<double> in any");

    return true;
}

FATP_TEST_CASE(pair_in_any)
{
    std::any a1 = std::pair<double, double>{1.0, 2.0};
    std::any a2 = std::pair<double, double>{1.0, 2.0};
    std::any a3 = std::pair<double, double>{1.0, 9.0};

    FATP_ASSERT_TRUE(areEqual(a1, a2), "Same pair in any");
    FATP_ASSERT_FALSE(areEqual(a1, a3), "Different pair in any");

    return true;
}

FATP_TEST_CASE(tuple_in_any)
{
    // Note: std::tuple<double, double, double> is pre-registered
    using TupleType = std::tuple<double, double, double>;
    std::any a1 = TupleType{1.0, 42.0, 3.0};
    std::any a2 = TupleType{1.0, 42.0, 3.0};
    std::any a3 = TupleType{1.0, 99.0, 3.0};

    FATP_ASSERT_TRUE(areEqual(a1, a2), "Same tuple in any");
    FATP_ASSERT_FALSE(areEqual(a1, a3), "Different tuple in any");

    return true;
}

// ============================================================================
// Test Suite 8: Unregistered Types
// ============================================================================

FATP_TEST_CASE(unregistered_type)
{
    struct CustomType
    {
        int x;
        bool operator==(const CustomType& other) const
        {
            return x == other.x;
        }
    };

    std::any a1 = CustomType{42};
    std::any a2 = CustomType{42};

    // Unregistered type should return false (and log error)
    bool result = areEqual(a1, a2);
    FATP_ASSERT_FALSE(result, "Unregistered type returns false");

    return true;
}

// ============================================================================
// Test Suite 9: Edge Cases
// ============================================================================

FATP_TEST_CASE(same_any_object)
{
    std::any a = 42;

    FATP_ASSERT_TRUE(areEqual(a, a), "Same any object compared to itself");

    return true;
}

FATP_TEST_CASE(any_floating_edge_cases)
{
    // Verify floating-point edge cases work through std::any dispatch
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();

    std::any nan1 = nan;
    std::any nan2 = nan;
    std::any inf1 = inf;
    std::any inf2 = inf;
    std::any negInf = -inf;
    std::any zero = 0.0;

    // NaN != NaN (IEEE semantics preserved through any)
    FATP_ASSERT_FALSE(areEqual<StandardComparisonPolicy>(nan1, nan2), "NaN in any should not equal NaN");

    // +inf == +inf
    FATP_ASSERT_TRUE(areEqual<StandardComparisonPolicy>(inf1, inf2), "+inf in any should equal +inf");

    // +inf != -inf
    FATP_ASSERT_FALSE(areEqual<StandardComparisonPolicy>(inf1, negInf), "+inf in any should not equal -inf");

    // inf != 0
    FATP_ASSERT_FALSE(areEqual<StandardComparisonPolicy>(inf1, zero), "inf in any should not equal zero");

    return true;
}

FATP_TEST_CASE(epsilon_edge_values)
{
    // Test behavior with edge-case epsilon values
    std::any a1 = 1.0;
    std::any a2 = 1.5;

    // Negative epsilon: should behave as if no tolerance (mismatch)
    bool negEpsResult = areEqual<StandardComparisonPolicy>(a1, a2, -1.0);
    FATP_ASSERT_FALSE(negEpsResult, "Negative epsilon should not match different values");

    // Very large epsilon: should match anything finite
    bool hugeEpsResult = areEqual<StandardComparisonPolicy>(a1, a2, 1e10);
    FATP_ASSERT_TRUE(hugeEpsResult, "Huge epsilon should match close values");

    // Zero epsilon: exact match only
    std::any a3 = 1.0;
    std::any a4 = 1.0;
    bool zeroEpsResult = areEqual<StandardComparisonPolicy>(a3, a4, 0.0);
    FATP_ASSERT_TRUE(zeroEpsResult, "Zero epsilon should match identical values");

    std::any a5 = 1.0;
    std::any a6 = 1.0 + 1e-15;
    bool zeroEpsMismatch = areEqual<StandardComparisonPolicy>(a5, a6, 0.0);
    FATP_ASSERT_FALSE(zeroEpsMismatch, "Zero epsilon should not match slightly different values");

    return true;
}

FATP_TEST_CASE(vector_of_any_heterogeneous)
{
    // Test std::vector<std::any> with heterogeneous element types.
    // This exercises the iterable path where each element is compared
    // via the std::any comparator.
    std::vector<std::any> v1;
    v1.emplace_back(42);
    v1.emplace_back(1.0);
    v1.emplace_back(std::string("hello"));

    std::vector<std::any> v2 = v1;

    FATP_ASSERT_TRUE(areEqual(v1, v2), "vector<any> with int/double/string should compare equal element-wise");

    v2[1] = 999.0;
    FATP_ASSERT_FALSE(areEqual(v1, v2), "vector<any> should compare not equal when one element differs");

    return true;
}

FATP_TEST_CASE(vector_of_any_with_empty_element)
{
    // Test vector<any> containing empty any elements
    std::vector<std::any> v1 = {std::any(42), std::any{}, std::any(std::string("hi"))};
    std::vector<std::any> v2 = {std::any(42), std::any{}, std::any(std::string("hi"))};
    std::vector<std::any> v3 = {std::any(42), std::any(7), std::any(std::string("hi"))};

    FATP_ASSERT_TRUE(areEqual(v1, v2), "vector<any> with empty element should compare equal");
    FATP_ASSERT_FALSE(areEqual(v1, v3), "empty any vs non-empty any should differ");

    return true;
}

FATP_TEST_CASE(vector_of_any_type_mismatch)
{
    // Same 'value' but different type should NOT be equal
    std::vector<std::any> v1 = {std::any(42)};   // int
    std::vector<std::any> v2 = {std::any(42.0)}; // double

    FATP_ASSERT_FALSE(areEqual(v1, v2), "int(42) vs double(42.0) in any should differ (type mismatch)");

    return true;
}

FATP_TEST_CASE(vector_of_any_unregistered_element)
{
    // Custom unregistered type inside vector<any>
    // Has operator== so if registry policy ever relaxes, test still compiles
    struct CustomType
    {
        int x;
        bool operator==(const CustomType& other) const
        {
            return x == other.x;
        }
    };

    std::vector<std::any> v1 = {std::any(42), std::any(CustomType{1})};
    std::vector<std::any> v2 = {std::any(42), std::any(CustomType{1})};

    // Should return false (unregistered type) without crashing
    bool result = areEqual(v1, v2);
    FATP_ASSERT_FALSE(result, "vector<any> with unregistered element type should return false");

    return true;
}

// ============================================================================
// Test Suite 10: Thread Safety
// ============================================================================

FATP_TEST_CASE(registry_initialization)
{
    // Ensure registry is properly initialized
    std::any a1 = 42;
    std::any a2 = 42;

    // First comparison triggers initialization
    bool result1 = areEqual(a1, a2);
    FATP_ASSERT_TRUE(result1, "First comparison after init");

    // Subsequent comparisons should work
    bool result2 = areEqual(a1, a2);
    FATP_ASSERT_TRUE(result2, "Second comparison after init");

    return true;
}

FATP_TEST_CASE(concurrent_registry_access)
{
    // Stress test concurrent access to the registry
    // This catches accidental non-thread-safe init paths
    constexpr int kNumThreads = 8;
    constexpr int kIterationsPerThread = 1000;

    std::atomic<int> successCount{0};
    std::atomic<int> failureCount{0};
    std::vector<std::thread> threads;

    // Barrier to synchronize thread start
    std::atomic<bool> startFlag{false};

    for (int t = 0; t < kNumThreads; ++t)
    {
        threads.emplace_back([&, t]() {
            // Wait for all threads to be ready
            while (!startFlag.load(std::memory_order_acquire))
            {
                std::this_thread::yield();
            }

            for (int i = 0; i < kIterationsPerThread; ++i)
            {
                // Mix of different registered types to stress registry lookups
                std::any a1, a2;
                switch (i % 4)
                {
                    case 0:
                        a1 = 42 + t;
                        a2 = 42 + t;
                        break;
                    case 1:
                        a1 = 3.14 + static_cast<double>(t);
                        a2 = 3.14 + static_cast<double>(t);
                        break;
                    case 2:
                        a1 = std::string("test");
                        a2 = std::string("test");
                        break;
                    case 3:
                        a1 = std::vector<double>{1.0, 2.0};
                        a2 = std::vector<double>{1.0, 2.0};
                        break;
                }

                if (areEqual(a1, a2))
                {
                    successCount.fetch_add(1, std::memory_order_relaxed);
                }
                else
                {
                    failureCount.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    // Release all threads
    startFlag.store(true, std::memory_order_release);

    // Wait for completion
    for (auto& thread : threads)
    {
        thread.join();
    }

    int expected = kNumThreads * kIterationsPerThread;
    FATP_ASSERT_EQ(successCount.load(), expected, "All concurrent comparisons should succeed");
    FATP_ASSERT_EQ(failureCount.load(), 0, "No concurrent comparisons should fail");

    return true;
}

FATP_TEST_CASE(depth_exceeded_no_throw)
{
    // REGRESSION: Verify that exceeding depth limit returns false, not throws
    constexpr size_t excessiveNesting = kMaxAnyRecursionDepth + 5;
    std::any current1 = 42;
    std::any current2 = 42;

    for (size_t i = 0; i < excessiveNesting; ++i)
    {
        std::any next1, next2;
        next1.emplace<std::any>(std::move(current1));
        next2.emplace<std::any>(std::move(current2));
        current1 = std::move(next1);
        current2 = std::move(next2);
    }

    bool threw = false;
    bool result = false;
    try
    {
        result = areEqual<StandardComparisonPolicy>(current1, current2);
    }
    catch (...)
    {
        threw = true;
    }

    FATP_ASSERT_FALSE(threw, "Depth exceeded must not throw");
    FATP_ASSERT_FALSE(result, "Depth exceeded must return false");

    return true;
}

FATP_TEST_CASE(depth_reset_after_exceeded)
{
    // Verify depth counter properly resets after an exceeded-depth comparison
    // (guards against depth counter not unwinding on early returns)

    // First: trigger depth exceeded
    constexpr size_t excessiveNesting = kMaxAnyRecursionDepth + 5;
    std::any deep1 = 42;
    std::any deep2 = 42;
    for (size_t i = 0; i < excessiveNesting; ++i)
    {
        std::any next1, next2;
        next1.emplace<std::any>(std::move(deep1));
        next2.emplace<std::any>(std::move(deep2));
        deep1 = std::move(next1);
        deep2 = std::move(next2);
    }

    bool exceededResult = areEqual<StandardComparisonPolicy>(deep1, deep2);
    FATP_ASSERT_FALSE(exceededResult, "Deep nesting should fail");

    // Now: a normal comparison should still work (depth counter reset)
    std::any simple1 = 42;
    std::any simple2 = 42;
    bool normalResult = areEqual<StandardComparisonPolicy>(simple1, simple2);
    FATP_ASSERT_TRUE(normalResult, "Normal comparison after exceeded should succeed");

    return true;
}

// ============================================================================
// Benchmarks
// ============================================================================

void run_benchmarks()
{
    std::cout << colors::cyan() << "\nEqualityAny Benchmarks:" << colors::reset() << "\n";

#ifdef NDEBUG
    std::any a1 = std::vector<double>(1000, 1.0);
    std::any a2 = std::vector<double>(1000, 1.0);

    volatile bool result = false;

    double time = measure_perf(
        [&]() {
            result = areEqual(a1, a2);
        },
        1000,
        100);
    DoNotOptimize(result);

    std::cout << "  any<vector<double>[1000]> comparison: " << format_time(time) << "\n";
#else
    std::cout << "  [Debug build - skipping benchmarks]\n";
#endif
}

} // namespace fat_p::testing::equalityany

// ============================================================================
// Public Interface
// ============================================================================

namespace fat_p::testing
{

bool test_EqualityAny()
{
    FATP_PRINT_HEADER(EQUALITY ANY)

    TestRunner runner;
    auto& out = *get_test_config().output;

    // Basic comparisons
    out << colors::blue() << "--- Basic Comparisons ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, equalityany, basic_int);
    FATP_RUN_TEST_NS(runner, equalityany, basic_double);
    FATP_RUN_TEST_NS(runner, equalityany, basic_string);
    FATP_RUN_TEST_NS(runner, equalityany, type_mismatch);

    // Empty handling
    out << "\n" << colors::blue() << "--- Empty Handling ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, equalityany, both_empty);
    FATP_RUN_TEST_NS(runner, equalityany, one_empty);

    // Nested any (REGRESSION TESTS)
    out << "\n" << colors::blue() << "--- Nested Any (Regression Tests) ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, equalityany, nested_empty_any_regression);
    FATP_RUN_TEST_NS(runner, equalityany, nested_any_with_values);
    FATP_RUN_TEST_NS(runner, equalityany, nested_any_type_mismatch_no_throw);
    FATP_RUN_TEST_NS(runner, equalityany, nested_any_empty_vs_value_no_throw);
    FATP_RUN_TEST_NS(runner, equalityany, deeply_nested_any);

    // Depth limiting
    out << "\n" << colors::blue() << "--- Recursion Depth Limiting ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, equalityany, depth_limit_safe);
    FATP_RUN_TEST_NS(runner, equalityany, depth_limit_boundary);
    FATP_RUN_TEST_NS(runner, equalityany, depth_limit_exceeded);

    // Explicit epsilon
    out << "\n" << colors::blue() << "--- Explicit Epsilon ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, equalityany, explicit_single_epsilon);
    FATP_RUN_TEST_NS(runner, equalityany, explicit_two_epsilon);

    // Epsilon propagation for container-in-any
    out << "\n" << colors::blue() << "--- Epsilon Propagation (Container-in-Any) ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, equalityany, epsilon_propagation_vector_double);
    FATP_RUN_TEST_NS(runner, equalityany, epsilon_propagation_deque_double);
    FATP_RUN_TEST_NS(runner, equalityany, epsilon_propagation_tuple_double);
    FATP_RUN_TEST_NS(runner, equalityany, epsilon_propagation_nested_vector_any);

    // All policies
    out << "\n" << colors::blue() << "--- All Comparison Policies ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, equalityany, standard_policy);
    FATP_RUN_TEST_NS(runner, equalityany, ulp_policy);
    FATP_RUN_TEST_NS(runner, equalityany, relative_policy);
    FATP_RUN_TEST_NS(runner, equalityany, hybrid_policy);

    // Pre-registered containers
    out << "\n" << colors::blue() << "--- Pre-Registered Container Types ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, equalityany, vector_int_in_any);
    FATP_RUN_TEST_NS(runner, equalityany, vector_double_in_any);
    FATP_RUN_TEST_NS(runner, equalityany, deque_double_in_any);
    FATP_RUN_TEST_NS(runner, equalityany, pair_in_any);
    FATP_RUN_TEST_NS(runner, equalityany, tuple_in_any);

    // Unregistered types
    out << "\n" << colors::blue() << "--- Unregistered Types ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, equalityany, unregistered_type);

    // Edge cases
    out << "\n" << colors::blue() << "--- Edge Cases ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, equalityany, same_any_object);
    FATP_RUN_TEST_NS(runner, equalityany, any_floating_edge_cases);
    FATP_RUN_TEST_NS(runner, equalityany, epsilon_edge_values);
    // Containers of std::any
    out << "\n" << colors::blue() << "--- Containers of std::any ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, equalityany, vector_of_any_heterogeneous);
    FATP_RUN_TEST_NS(runner, equalityany, vector_of_any_with_empty_element);
    FATP_RUN_TEST_NS(runner, equalityany, vector_of_any_type_mismatch);
    FATP_RUN_TEST_NS(runner, equalityany, vector_of_any_unregistered_element);

    // Thread safety
    out << "\n" << colors::blue() << "--- Thread Safety ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, equalityany, registry_initialization);
    FATP_RUN_TEST_NS(runner, equalityany, concurrent_registry_access);
    FATP_RUN_TEST_NS(runner, equalityany, depth_exceeded_no_throw);
    FATP_RUN_TEST_NS(runner, equalityany, depth_reset_after_exceeded);

    equalityany::run_benchmarks();

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_EqualityAny() ? 0 : 1;
}
#endif
