/**
 * @file test_TensorComparison.cpp
 * @brief Comprehensive unit tests for Tensor comparison operations
 */
/*
FATP_META:
  meta_version: 1
  component: TensorComparison
  file_role: test
  path: components/Tensor/tests/test_TensorComparison.cpp
  namespace: fat_p
  summary: "Unit tests for TensorComparison."
  api_stability: in_work
  related:
    docs_search: "TensorComparison"
    headers:
      - include/fat_p/EqualityComparisons.h
      - include/fat_p/EqualityTensor.h
      - include/fat_p/FatPTest.h
      - include/fat_p/Tensor.h
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

#include <cmath>
#include <iostream>
#include <unordered_map>
#include <unordered_set>

#include "EqualityComparisons.h"
#include "EqualityTensor.h"
#include "FatPTest.h"
#include "Tensor.h"

namespace fat_p::testing::tensorcomparison
{

// ============================================================================
// Test Suite 1: operator== (Exact Comparison for STL Containers)
// ============================================================================

FATP_TEST_CASE(exact_equality_integers)
{
    Tensor<int> a({2, 3}, 42);
    Tensor<int> b({2, 3}, 42);
    Tensor<int> c({2, 3}, 43);

    FATP_ASSERT_TRUE(a == b, "Equal integer tensors should match");
    FATP_ASSERT_TRUE(a != c, "Different integer tensors should not match");
    FATP_ASSERT_TRUE(a == a, "Self-equality should work");

    return true;
}

FATP_TEST_CASE(exact_equality_floats)
{
    Tensor<float> a({2, 2}, 1.0f);
    Tensor<float> b({2, 2}, 1.0f);
    Tensor<float> c({2, 2}, 1.0f + 1e-7f);

    FATP_ASSERT_TRUE(a == b, "Exact float match should work");
    FATP_ASSERT_TRUE(a != c, "Even tiny differences should fail with operator==");

    return true;
}

FATP_TEST_CASE(exact_equality_views)
{
    Tensor<double> mat({3, 3}, 1.0);
    auto row1 = mat.row(0);
    auto row2 = mat.row(0);
    auto row3 = mat.row(1);

    for (std::size_t i = 0; i < 3; ++i)
    {
        row3[i] = row1[i];
    }

    FATP_ASSERT_TRUE(row1 == row2, "Same view should be equal");
    FATP_ASSERT_TRUE(row1 == row3, "Views with same values should be equal");

    return true;
}

FATP_TEST_CASE(shape_mismatch)
{
    Tensor<int> a({2, 3}, 1);
    Tensor<int> b({3, 2}, 1);
    Tensor<int> c({2, 3, 1}, 1);

    FATP_ASSERT_TRUE(a != b, "Different shapes should not be equal");
    FATP_ASSERT_TRUE(a != c, "Different ranks should not be equal");

    return true;
}

// ============================================================================
// Test Suite 2: approx_equal (Epsilon-Based Floating-Point Comparison)
// ============================================================================

FATP_TEST_CASE(approx_equal_default_epsilon)
{
    Tensor<float> a({2, 2}, 1.0f);
    Tensor<float> b({2, 2}, 1.0f + 1e-7f);
    Tensor<float> c({2, 2}, 1.0f + 1e-5f);

    FATP_ASSERT_TRUE(a.approx_equal(b), "Within default epsilon should pass");
    FATP_ASSERT_TRUE(!a.approx_equal(c), "Beyond default epsilon should fail");

    return true;
}

FATP_TEST_CASE(approx_equal_custom_epsilon)
{
    Tensor<double> a({3, 3}, 1.0);
    Tensor<double> b({3, 3}, 1.0 + 1e-8);
    Tensor<double> c({3, 3}, 1.0 + 1e-4);

    FATP_ASSERT_TRUE(!a.approx_equal(b), "Beyond default epsilon should fail");
    FATP_ASSERT_TRUE(a.approx_equal(b, 1e-7), "Within custom epsilon should pass");
    FATP_ASSERT_TRUE(a.approx_equal(c, 1e-3), "Within large custom epsilon should pass");

    return true;
}

FATP_TEST_CASE(approx_equal_relative_tolerance)
{
    Tensor<float> a({2, 2}, 1e6f);
    Tensor<float> b({2, 2}, 1e6f + 1.0f);

    FATP_ASSERT_TRUE(a.approx_equal(b, 1e-5f), "Relative tolerance for large values");

    return true;
}

FATP_TEST_CASE(approx_equal_views)
{
    Tensor<double> mat({3, 3}, 1.0);

    auto row1 = mat.row(1);
    for (std::size_t i = 0; i < 3; ++i)
    {
        row1[i] += 1e-9;
    }

    auto row0 = mat.row(0);
    auto row1_view = mat.row(1);

    FATP_ASSERT_TRUE(row0.approx_equal(row1_view, 1e-8), "Views should work with approx_equal");

    return true;
}

FATP_TEST_CASE(approx_equal_integers)
{
    Tensor<int> a({2, 2}, 42);
    Tensor<int> b({2, 2}, 42);
    Tensor<int> c({2, 2}, 43);

    FATP_ASSERT_TRUE(a.approx_equal(b), "Equal integers should pass");
    FATP_ASSERT_TRUE(!a.approx_equal(c), "Different integers should fail");

    return true;
}

// ============================================================================
// Test Suite 3: std::hash Support (Unordered Containers)
// ============================================================================

FATP_TEST_CASE(hash_consistency)
{
    Tensor<int> a({2, 2}, 42);
    Tensor<int> b({2, 2}, 42);

    std::size_t hash_a = std::hash<Tensor<int>>{}(a);
    std::size_t hash_b = std::hash<Tensor<int>>{}(b);

    FATP_ASSERT_TRUE(a == b, "Tensors should be equal");
    FATP_ASSERT_TRUE(hash_a == hash_b, "Equal tensors must have equal hashes");

    return true;
}

FATP_TEST_CASE(unordered_map)
{
    std::unordered_map<Tensor<int>, std::string> tensor_map;

    Tensor<int> key1({2, 2}, 42);
    Tensor<int> key2({2, 2}, 42);
    Tensor<int> key3({2, 2}, 43);

    tensor_map[key1] = "value1";
    tensor_map[key3] = "value3";

    FATP_ASSERT_EQ(tensor_map.size(), 2, "Map should have 2 entries");
    FATP_ASSERT_EQ(tensor_map.count(key2), 1, "Equal key should be found");
    FATP_ASSERT_EQ(tensor_map[key2], "value1", "Should retrieve correct value");

    return true;
}

FATP_TEST_CASE(unordered_set)
{
    std::unordered_set<Tensor<float>> tensor_set;

    Tensor<float> t1({3, 3}, 1.0f);
    Tensor<float> t2({3, 3}, 1.0f);
    Tensor<float> t3({3, 3}, 2.0f);

    tensor_set.insert(t1);
    tensor_set.insert(t2);
    tensor_set.insert(t3);

    FATP_ASSERT_EQ(tensor_set.size(), 2, "Set should have 2 unique tensors");
    FATP_ASSERT_EQ(tensor_set.count(t1), 1, "Should find t1");
    FATP_ASSERT_EQ(tensor_set.count(t2), 1, "Should find t2 (equal to t1)");

    return true;
}

FATP_TEST_CASE(hash_with_nan)
{
    Tensor<float> a({2, 2}, std::nanf(""));
    Tensor<float> b({2, 2}, std::nanf(""));

    std::size_t hash_a = std::hash<Tensor<float>>{}(a);
    std::size_t hash_b = std::hash<Tensor<float>>{}(b);

    FATP_ASSERT_TRUE(hash_a == hash_b, "NaN tensors should have consistent hashes");

    return true;
}

FATP_TEST_CASE(hash_different_strides)
{
    Tensor<int> a({4, 4}, 1);
    auto view = a.row(0);

    std::size_t hash_full = std::hash<Tensor<int>>{}(a);
    std::size_t hash_view = std::hash<decltype(view)>{}(view);

    FATP_ASSERT_TRUE(hash_full != hash_view, "Different shapes should have different hashes");

    return true;
}

// ============================================================================
// Test Suite 4: EqualDispatcher Integration (Policy-Based for Test Frameworks)
// ============================================================================

FATP_TEST_CASE(equal_dispatcher_basic)
{
    Tensor<double> a({10, 10}, 1.0);
    Tensor<double> b({10, 10}, 1.0 + 1e-8);

    bool result = areEqual(a, b, 1e-7);
    FATP_ASSERT_TRUE(result, "Policy-based comparison should work with epsilon");

    return true;
}

FATP_TEST_CASE(equal_dispatcher_hybrid)
{
    Tensor<float> a({5, 5}, 1.0f);
    Tensor<float> b({5, 5}, 1.0f + 1e-7f);

    bool result = areEqual<Tensor<float>, HybridComparisonPolicy>(a, b, 1e-6f, 1e-6f);
    FATP_ASSERT_TRUE(result, "HybridPolicy should pass with appropriate tolerances");

    return true;
}

// ============================================================================
// Test Suite 5: Edge Cases and Special Values
// ============================================================================

FATP_TEST_CASE(equality_with_infinity)
{
    Tensor<double> a({2, 2}, std::numeric_limits<double>::infinity());
    Tensor<double> b({2, 2}, std::numeric_limits<double>::infinity());
    Tensor<double> c({2, 2}, -std::numeric_limits<double>::infinity());

    FATP_ASSERT_TRUE(a == b, "Positive infinities should be equal");
    FATP_ASSERT_TRUE(a != c, "Positive and negative infinities should differ");

    return true;
}

FATP_TEST_CASE(equality_with_zero)
{
    Tensor<float> a({3, 3}, 0.0f);
    Tensor<float> b({3, 3}, -0.0f);

    FATP_ASSERT_TRUE(a == b, "Positive and negative zero should be equal");

    return true;
}

FATP_TEST_CASE(empty_tensors)
{
    Tensor<int> a({0});
    Tensor<int> b({0});

    FATP_ASSERT_TRUE(a == b, "Empty tensors should be equal");
    FATP_ASSERT_TRUE(a.approx_equal(b), "Empty tensors should be approx_equal");

    return true;
}

FATP_TEST_CASE(single_element)
{
    Tensor<double> a({1}, 42.0);
    Tensor<double> b({1}, 42.0);
    Tensor<double> c({1}, 42.0 + 1e-9);

    FATP_ASSERT_TRUE(a == b, "Single element tensors should be equal");
    FATP_ASSERT_TRUE(a.approx_equal(c, 1e-8), "Single element approx_equal should work");

    return true;
}

} // namespace fat_p::testing::tensorcomparison

// ============================================================================

namespace fat_p::testing
{

bool test_TensorComparison()
{
    FATP_PRINT_HEADER(TENSOR COMPARISON)

    TestRunner runner;

    // Exact equality (operator==)
    FATP_RUN_TEST_NS(runner, tensorcomparison, exact_equality_integers);
    FATP_RUN_TEST_NS(runner, tensorcomparison, exact_equality_floats);
    FATP_RUN_TEST_NS(runner, tensorcomparison, exact_equality_views);
    FATP_RUN_TEST_NS(runner, tensorcomparison, shape_mismatch);

    // Approximate equality
    FATP_RUN_TEST_NS(runner, tensorcomparison, approx_equal_default_epsilon);
    FATP_RUN_TEST_NS(runner, tensorcomparison, approx_equal_custom_epsilon);
    FATP_RUN_TEST_NS(runner, tensorcomparison, approx_equal_relative_tolerance);
    FATP_RUN_TEST_NS(runner, tensorcomparison, approx_equal_views);
    FATP_RUN_TEST_NS(runner, tensorcomparison, approx_equal_integers);

    // Hash support
    FATP_RUN_TEST_NS(runner, tensorcomparison, hash_consistency);
    FATP_RUN_TEST_NS(runner, tensorcomparison, unordered_map);
    FATP_RUN_TEST_NS(runner, tensorcomparison, unordered_set);
    FATP_RUN_TEST_NS(runner, tensorcomparison, hash_with_nan);
    FATP_RUN_TEST_NS(runner, tensorcomparison, hash_different_strides);

    // EqualDispatcher integration
    FATP_RUN_TEST_NS(runner, tensorcomparison, equal_dispatcher_basic);
    FATP_RUN_TEST_NS(runner, tensorcomparison, equal_dispatcher_hybrid);

    // Edge cases
    FATP_RUN_TEST_NS(runner, tensorcomparison, equality_with_infinity);
    FATP_RUN_TEST_NS(runner, tensorcomparison, equality_with_zero);
    FATP_RUN_TEST_NS(runner, tensorcomparison, empty_tensors);
    FATP_RUN_TEST_NS(runner, tensorcomparison, single_element);


    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_TensorComparison() ? 0 : 1;
}
#endif
