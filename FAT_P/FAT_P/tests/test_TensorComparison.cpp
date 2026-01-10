/**
 * @file test_TensorComparison.cpp
 * @brief Comprehensive unit tests for Tensor comparison operations
 */
/*
FATP_META:
  meta_version: 1
  component: TensorComparison
  file_role: test
  path: tests/test_TensorComparison.cpp
  namespace: fat_p
  summary: "Unit tests for TensorComparison."
  related:
    docs_search: "TensorComparison"
    headers:
      - fat_p/EqualityComparisons.h
      - fat_p/EqualityTensor.h
      - fat_p/FatPTest.h
      - fat_p/Tensor.h
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

TEST_CASE(exact_equality_integers)
{
    Tensor<int> a({2, 3}, 42);
    Tensor<int> b({2, 3}, 42);
    Tensor<int> c({2, 3}, 43);

    ASSERT_TRUE(a == b, "Equal integer tensors should match");
    ASSERT_TRUE(a != c, "Different integer tensors should not match");
    ASSERT_TRUE(a == a, "Self-equality should work");

    return true;
}

TEST_CASE(exact_equality_floats)
{
    Tensor<float> a({2, 2}, 1.0f);
    Tensor<float> b({2, 2}, 1.0f);
    Tensor<float> c({2, 2}, 1.0f + 1e-7f);

    ASSERT_TRUE(a == b, "Exact float match should work");
    ASSERT_TRUE(a != c, "Even tiny differences should fail with operator==");

    return true;
}

TEST_CASE(exact_equality_views)
{
    Tensor<double> mat({3, 3}, 1.0);
    auto row1 = mat.row(0);
    auto row2 = mat.row(0);
    auto row3 = mat.row(1);

    for (std::size_t i = 0; i < 3; ++i)
    {
        row3[i] = row1[i];
    }

    ASSERT_TRUE(row1 == row2, "Same view should be equal");
    ASSERT_TRUE(row1 == row3, "Views with same values should be equal");

    return true;
}

TEST_CASE(shape_mismatch)
{
    Tensor<int> a({2, 3}, 1);
    Tensor<int> b({3, 2}, 1);
    Tensor<int> c({2, 3, 1}, 1);

    ASSERT_TRUE(a != b, "Different shapes should not be equal");
    ASSERT_TRUE(a != c, "Different ranks should not be equal");

    return true;
}

// ============================================================================
// Test Suite 2: approx_equal (Epsilon-Based Floating-Point Comparison)
// ============================================================================

TEST_CASE(approx_equal_default_epsilon)
{
    Tensor<float> a({2, 2}, 1.0f);
    Tensor<float> b({2, 2}, 1.0f + 1e-7f);
    Tensor<float> c({2, 2}, 1.0f + 1e-5f);

    ASSERT_TRUE(a.approx_equal(b), "Within default epsilon should pass");
    ASSERT_TRUE(!a.approx_equal(c), "Beyond default epsilon should fail");

    return true;
}

TEST_CASE(approx_equal_custom_epsilon)
{
    Tensor<double> a({3, 3}, 1.0);
    Tensor<double> b({3, 3}, 1.0 + 1e-8);
    Tensor<double> c({3, 3}, 1.0 + 1e-4);

    ASSERT_TRUE(!a.approx_equal(b), "Beyond default epsilon should fail");
    ASSERT_TRUE(a.approx_equal(b, 1e-7), "Within custom epsilon should pass");
    ASSERT_TRUE(a.approx_equal(c, 1e-3), "Within large custom epsilon should pass");

    return true;
}

TEST_CASE(approx_equal_relative_tolerance)
{
    Tensor<float> a({2, 2}, 1e6f);
    Tensor<float> b({2, 2}, 1e6f + 1.0f);

    ASSERT_TRUE(a.approx_equal(b, 1e-5f), "Relative tolerance for large values");

    return true;
}

TEST_CASE(approx_equal_views)
{
    Tensor<double> mat({3, 3}, 1.0);

    auto row1 = mat.row(1);
    for (std::size_t i = 0; i < 3; ++i)
    {
        row1[i] += 1e-9;
    }

    auto row0 = mat.row(0);
    auto row1_view = mat.row(1);

    ASSERT_TRUE(row0.approx_equal(row1_view, 1e-8), "Views should work with approx_equal");

    return true;
}

TEST_CASE(approx_equal_integers)
{
    Tensor<int> a({2, 2}, 42);
    Tensor<int> b({2, 2}, 42);
    Tensor<int> c({2, 2}, 43);

    ASSERT_TRUE(a.approx_equal(b), "Equal integers should pass");
    ASSERT_TRUE(!a.approx_equal(c), "Different integers should fail");

    return true;
}

// ============================================================================
// Test Suite 3: std::hash Support (Unordered Containers)
// ============================================================================

TEST_CASE(hash_consistency)
{
    Tensor<int> a({2, 2}, 42);
    Tensor<int> b({2, 2}, 42);

    std::size_t hash_a = std::hash<Tensor<int>>{}(a);
    std::size_t hash_b = std::hash<Tensor<int>>{}(b);

    ASSERT_TRUE(a == b, "Tensors should be equal");
    ASSERT_TRUE(hash_a == hash_b, "Equal tensors must have equal hashes");

    return true;
}

TEST_CASE(unordered_map)
{
    std::unordered_map<Tensor<int>, std::string> tensor_map;

    Tensor<int> key1({2, 2}, 42);
    Tensor<int> key2({2, 2}, 42);
    Tensor<int> key3({2, 2}, 43);

    tensor_map[key1] = "value1";
    tensor_map[key3] = "value3";

    ASSERT_EQ(tensor_map.size(), 2, "Map should have 2 entries");
    ASSERT_EQ(tensor_map.count(key2), 1, "Equal key should be found");
    ASSERT_EQ(tensor_map[key2], "value1", "Should retrieve correct value");

    return true;
}

TEST_CASE(unordered_set)
{
    std::unordered_set<Tensor<float>> tensor_set;

    Tensor<float> t1({3, 3}, 1.0f);
    Tensor<float> t2({3, 3}, 1.0f);
    Tensor<float> t3({3, 3}, 2.0f);

    tensor_set.insert(t1);
    tensor_set.insert(t2);
    tensor_set.insert(t3);

    ASSERT_EQ(tensor_set.size(), 2, "Set should have 2 unique tensors");
    ASSERT_EQ(tensor_set.count(t1), 1, "Should find t1");
    ASSERT_EQ(tensor_set.count(t2), 1, "Should find t2 (equal to t1)");

    return true;
}

TEST_CASE(hash_with_nan)
{
    Tensor<float> a({2, 2}, std::nanf(""));
    Tensor<float> b({2, 2}, std::nanf(""));

    std::size_t hash_a = std::hash<Tensor<float>>{}(a);
    std::size_t hash_b = std::hash<Tensor<float>>{}(b);

    ASSERT_TRUE(hash_a == hash_b, "NaN tensors should have consistent hashes");

    return true;
}

TEST_CASE(hash_different_strides)
{
    Tensor<int> a({4, 4}, 1);
    auto view = a.row(0);

    std::size_t hash_full = std::hash<Tensor<int>>{}(a);
    std::size_t hash_view = std::hash<decltype(view)>{}(view);

    ASSERT_TRUE(hash_full != hash_view, "Different shapes should have different hashes");

    return true;
}

// ============================================================================
// Test Suite 4: EqualDispatcher Integration (Policy-Based for Test Frameworks)
// ============================================================================

TEST_CASE(equal_dispatcher_basic)
{
    Tensor<double> a({10, 10}, 1.0);
    Tensor<double> b({10, 10}, 1.0 + 1e-8);

    bool result = areEqual(a, b, 1e-7);
    ASSERT_TRUE(result, "Policy-based comparison should work with epsilon");

    return true;
}

TEST_CASE(equal_dispatcher_hybrid)
{
    Tensor<float> a({5, 5}, 1.0f);
    Tensor<float> b({5, 5}, 1.0f + 1e-7f);

    bool result = areEqual<Tensor<float>, HybridComparisonPolicy>(a, b, 1e-6f, 1e-6f);
    ASSERT_TRUE(result, "HybridPolicy should pass with appropriate tolerances");

    return true;
}

// ============================================================================
// Test Suite 5: Edge Cases and Special Values
// ============================================================================

TEST_CASE(equality_with_infinity)
{
    Tensor<double> a({2, 2}, std::numeric_limits<double>::infinity());
    Tensor<double> b({2, 2}, std::numeric_limits<double>::infinity());
    Tensor<double> c({2, 2}, -std::numeric_limits<double>::infinity());

    ASSERT_TRUE(a == b, "Positive infinities should be equal");
    ASSERT_TRUE(a != c, "Positive and negative infinities should differ");

    return true;
}

TEST_CASE(equality_with_zero)
{
    Tensor<float> a({3, 3}, 0.0f);
    Tensor<float> b({3, 3}, -0.0f);

    ASSERT_TRUE(a == b, "Positive and negative zero should be equal");

    return true;
}

TEST_CASE(empty_tensors)
{
    Tensor<int> a({0});
    Tensor<int> b({0});

    ASSERT_TRUE(a == b, "Empty tensors should be equal");
    ASSERT_TRUE(a.approx_equal(b), "Empty tensors should be approx_equal");

    return true;
}

TEST_CASE(single_element)
{
    Tensor<double> a({1}, 42.0);
    Tensor<double> b({1}, 42.0);
    Tensor<double> c({1}, 42.0 + 1e-9);

    ASSERT_TRUE(a == b, "Single element tensors should be equal");
    ASSERT_TRUE(a.approx_equal(c, 1e-8), "Single element approx_equal should work");

    return true;
}

// ============================================================================
// Benchmarks
// ============================================================================

void benchmark_tensorcomparison()
{
    std::cout << "\n" << colors::cyan() << "TensorComparison Benchmarks:"
              << colors::reset() << "\n\n";

    const std::size_t N = 1000;
    Tensor<float> a({N, N}, 1.0f);
    Tensor<float> b({N, N}, 1.0f + 1e-7f);

    double time_exact = measure_perf([&]()
    {
        DoNotOptimize(a == b);
    }, 1000, 100);
    std::cout << "operator== (" << N << "x" << N << "): " << format_time(time_exact) << "\n";

    double time_approx = measure_perf([&]()
    {
        DoNotOptimize(a.approx_equal(b));
    }, 1000, 100);
    std::cout << "approx_equal (" << N << "x" << N << "): " << format_time(time_approx) << "\n";

    if (time_exact > 0.0)
    {
        double ratio = time_approx / time_exact;
        std::cout << "Ratio (approx/exact): " << std::fixed
                  << std::setprecision(2) << ratio << "x\n";
    }

    double time_hash = measure_perf([&]()
    {
        std::size_t hash = std::hash<Tensor<float>>{}(a);
        DoNotOptimize(hash);
    }, 100, 10);
    std::cout << "std::hash (" << N << "x" << N << "): " << format_time(time_hash) << "\n";
}

} // namespace fat_p::testing::tensorcomparison

namespace fat_p::testing
{

bool test_TensorComparison()
{
    PRINT_HEADER(TENSOR COMPARISON)

    TestRunner runner;

    // Exact equality (operator==)
    RUN_TEST_NS(runner, tensorcomparison, exact_equality_integers);
    RUN_TEST_NS(runner, tensorcomparison, exact_equality_floats);
    RUN_TEST_NS(runner, tensorcomparison, exact_equality_views);
    RUN_TEST_NS(runner, tensorcomparison, shape_mismatch);

    // Approximate equality
    RUN_TEST_NS(runner, tensorcomparison, approx_equal_default_epsilon);
    RUN_TEST_NS(runner, tensorcomparison, approx_equal_custom_epsilon);
    RUN_TEST_NS(runner, tensorcomparison, approx_equal_relative_tolerance);
    RUN_TEST_NS(runner, tensorcomparison, approx_equal_views);
    RUN_TEST_NS(runner, tensorcomparison, approx_equal_integers);

    // Hash support
    RUN_TEST_NS(runner, tensorcomparison, hash_consistency);
    RUN_TEST_NS(runner, tensorcomparison, unordered_map);
    RUN_TEST_NS(runner, tensorcomparison, unordered_set);
    RUN_TEST_NS(runner, tensorcomparison, hash_with_nan);
    RUN_TEST_NS(runner, tensorcomparison, hash_different_strides);

    // EqualDispatcher integration
    RUN_TEST_NS(runner, tensorcomparison, equal_dispatcher_basic);
    RUN_TEST_NS(runner, tensorcomparison, equal_dispatcher_hybrid);

    // Edge cases
    RUN_TEST_NS(runner, tensorcomparison, equality_with_infinity);
    RUN_TEST_NS(runner, tensorcomparison, equality_with_zero);
    RUN_TEST_NS(runner, tensorcomparison, empty_tensors);
    RUN_TEST_NS(runner, tensorcomparison, single_element);

    tensorcomparison::benchmark_tensorcomparison();

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
