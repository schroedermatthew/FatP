#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <cmath>

#include "Tensor.h"
#include "EqualityComparisons.h"
#include "EqualityTensor.h"
#include "test_TensorCompareSeialize.h"
#include "test_Utilities.h"

namespace cpp_utilities::testing
{

using namespace cpp_utilities;

// =============================================================================
// Test Suite 1: operator== (Exact Comparison for STL Containers)
// =============================================================================

bool test_tensor_exact_equality_integers() {
    Tensor<int> a({2, 3}, 42);
    Tensor<int> b({2, 3}, 42);
    Tensor<int> c({2, 3}, 43);
    
    SIMPLE_ASSERT(a == b, "Equal integer tensors should match");
    SIMPLE_ASSERT(a != c, "Different integer tensors should not match");
    SIMPLE_ASSERT(a == a, "Self-equality should work");
    
    return true;
}

bool test_tensor_exact_equality_floats() {
    Tensor<float> a({2, 2}, 1.0f);
    Tensor<float> b({2, 2}, 1.0f);
    Tensor<float> c({2, 2}, 1.0f + 1e-7f);  // Different bits
    
    SIMPLE_ASSERT(a == b, "Exact float match should work");
    SIMPLE_ASSERT(a != c, "Even tiny differences should fail with operator==");
    
    return true;
}

bool test_tensor_exact_equality_with_views() {
    Tensor<double> mat({3, 3}, 1.0);
    auto row1 = mat.row(0);
    auto row2 = mat.row(0);  // Same row
    auto row3 = mat.row(1);  // Different row but same values
    
    // Set row 1 to same values as row 0
    for (size_t i = 0; i < 3; ++i) {
        row3[i] = row1[i];
    }
    
    SIMPLE_ASSERT(row1 == row2, "Same view should be equal");
    SIMPLE_ASSERT(row1 == row3, "Views with same values should be equal");
    
    return true;
}

bool test_tensor_shape_mismatch() {
    Tensor<int> a({2, 3}, 1);
    Tensor<int> b({3, 2}, 1);
    Tensor<int> c({2, 3, 1}, 1);
    
    SIMPLE_ASSERT(a != b, "Different shapes should not be equal");
    SIMPLE_ASSERT(a != c, "Different ranks should not be equal");
    
    return true;
}

// =============================================================================
// Test Suite 2: approx_equal (Epsilon-Based Floating-Point Comparison)
// =============================================================================

bool test_tensor_approx_equal_default_epsilon() {
    // Float default epsilon is 1e-6
    Tensor<float> a({2, 2}, 1.0f);
    Tensor<float> b({2, 2}, 1.0f + 1e-7f);   // Within epsilon
    Tensor<float> c({2, 2}, 1.0f + 1e-5f);   // Beyond epsilon
    
    SIMPLE_ASSERT(a.approx_equal(b), "Within default epsilon should pass");
    SIMPLE_ASSERT(!a.approx_equal(c), "Beyond default epsilon should fail");
    
    return true;
}

bool test_tensor_approx_equal_custom_epsilon() {
    Tensor<double> a({3, 3}, 1.0);
    Tensor<double> b({3, 3}, 1.0 + 1e-8);
    Tensor<double> c({3, 3}, 1.0 + 1e-4);
    
    // Default epsilon for double is 1e-10, so b is beyond it
    SIMPLE_ASSERT(!a.approx_equal(b), "Beyond default epsilon should fail");
    
    // But with custom epsilon 1e-7, b should pass
    SIMPLE_ASSERT(a.approx_equal(b, 1e-7), "Within custom epsilon should pass");
    
    // c should pass with 1e-3 epsilon
    SIMPLE_ASSERT(a.approx_equal(c, 1e-3), "Within large custom epsilon should pass");
    
    return true;
}

bool test_tensor_approx_equal_relative_tolerance() {
    // Test relative tolerance for large values
    Tensor<float> a({2, 2}, 1e6f);
    Tensor<float> b({2, 2}, 1e6f + 1.0f);  // Absolute diff = 1.0
    
    // With default epsilon 1e-6, relative diff = 1.0 / 1e6 = 1e-6 (at boundary)
    // The implementation uses "diff > epsilon * max_val", so this should be close
    SIMPLE_ASSERT(a.approx_equal(b, 1e-5f), "Relative tolerance for large values");
    
    return true;
}

bool test_tensor_approx_equal_with_views() {
    Tensor<double> mat({3, 3}, 1.0);
    
    // Modify one row slightly
    auto row1 = mat.row(1);
    for (size_t i = 0; i < 3; ++i) {
        row1[i] += 1e-9;
    }
    
    auto row0 = mat.row(0);
    auto row1_view = mat.row(1);
    
    SIMPLE_ASSERT(row0.approx_equal(row1_view, 1e-8), "Views should work with approx_equal");
    
    return true;
}

bool test_tensor_approx_equal_integers() {
    // Integer tensors should use exact comparison even with approx_equal
    Tensor<int> a({2, 2}, 42);
    Tensor<int> b({2, 2}, 42);
    Tensor<int> c({2, 2}, 43);
    
    SIMPLE_ASSERT(a.approx_equal(b), "Equal integers should pass");
    SIMPLE_ASSERT(!a.approx_equal(c), "Different integers should fail");
    
    return true;
}

// =============================================================================
// Test Suite 3: std::hash Support (Unordered Containers)
// =============================================================================

bool test_tensor_hash_consistency() {
    Tensor<int> a({2, 2}, 42);
    Tensor<int> b({2, 2}, 42);
    
    size_t hash_a = std::hash<Tensor<int>>{}(a);
    size_t hash_b = std::hash<Tensor<int>>{}(b);
    
    // Equal tensors must have equal hashes
    SIMPLE_ASSERT(a == b, "Tensors should be equal");
    SIMPLE_ASSERT(hash_a == hash_b, "Equal tensors must have equal hashes");
    
    return true;
}

bool test_tensor_unordered_map() {
    std::unordered_map<Tensor<int>, std::string> tensor_map;
    
    Tensor<int> key1({2, 2}, 42);
    Tensor<int> key2({2, 2}, 42);  // Equal to key1
    Tensor<int> key3({2, 2}, 43);  // Different
    
    tensor_map[key1] = "value1";
    tensor_map[key3] = "value3";
    
    SIMPLE_ASSERT(tensor_map.size() == 2, "Map should have 2 entries");
    SIMPLE_ASSERT(tensor_map.count(key2) == 1, "Equal key should be found");
    SIMPLE_ASSERT(tensor_map[key2] == "value1", "Should retrieve correct value");
    
    return true;
}

bool test_tensor_unordered_set() {
    std::unordered_set<Tensor<float>> tensor_set;
    
    Tensor<float> t1({3, 3}, 1.0f);
    Tensor<float> t2({3, 3}, 1.0f);  // Equal to t1
    Tensor<float> t3({3, 3}, 2.0f);  // Different
    
    tensor_set.insert(t1);
    tensor_set.insert(t2);  // Should not create new entry
    tensor_set.insert(t3);
    
    SIMPLE_ASSERT(tensor_set.size() == 2, "Set should have 2 unique tensors");
    SIMPLE_ASSERT(tensor_set.count(t1) == 1, "Should find t1");
    SIMPLE_ASSERT(tensor_set.count(t2) == 1, "Should find t2 (equal to t1)");
    
    return true;
}

bool test_tensor_hash_with_nan() {
    // Test that NaN handling is consistent
    Tensor<float> a({2, 2}, std::nanf(""));
    Tensor<float> b({2, 2}, std::nanf(""));
    
    size_t hash_a = std::hash<Tensor<float>>{}(a);
    size_t hash_b = std::hash<Tensor<float>>{}(b);
    
    // Due to bit-cast, all NaNs should hash to same value
    SIMPLE_ASSERT(hash_a == hash_b, "NaN tensors should have consistent hashes");
    
    return true;
}

bool test_tensor_hash_different_strides() {
    Tensor<int> a({4, 4}, 1);
    auto view = a.row(0);
    
    // Hash includes strides, so different stride patterns should potentially differ
    size_t hash_full = std::hash<Tensor<int>>{}(a);
    size_t hash_view = std::hash<decltype(view)>{}(view);
    
    // They have different shapes/strides, so hashes should differ
    SIMPLE_ASSERT(hash_full != hash_view, "Different shapes should have different hashes");
    
    return true;
}

// =============================================================================
// Test Suite 4: EqualDispatcher Integration (Policy-Based for Test Frameworks)
// =============================================================================

bool test_tensor_equal_dispatcher_basic() {
    Tensor<double> a({10, 10}, 1.0);
    Tensor<double> b({10, 10}, 1.0 + 1e-8);
    
    // Using default StandardComparisonPolicy with custom epsilon
    bool result = areEqual(a, b, 1e-7);
    SIMPLE_ASSERT(result, "Policy-based comparison should work with epsilon");
    
    return true;
}

bool test_tensor_equal_dispatcher_hybrid() {
    Tensor<float> a({5, 5}, 1.0f);
    Tensor<float> b({5, 5}, 1.0f + 1e-7f);
    
    // HybridComparisonPolicy with relative and absolute tolerances
    bool result = areEqual<Tensor<float>, HybridComparisonPolicy>(a, b, 1e-6, 1e-6);
    SIMPLE_ASSERT(result, "HybridPolicy should pass with appropriate tolerances");
    
    return true;
}

// =============================================================================
// Test Suite 5: Edge Cases and Special Values
// =============================================================================

bool test_tensor_equality_with_infinity() {
    Tensor<double> a({2, 2}, std::numeric_limits<double>::infinity());
    Tensor<double> b({2, 2}, std::numeric_limits<double>::infinity());
    Tensor<double> c({2, 2}, -std::numeric_limits<double>::infinity());
    
    SIMPLE_ASSERT(a == b, "Positive infinities should be equal");
    SIMPLE_ASSERT(a != c, "Positive and negative infinities should differ");
    
    return true;
}

bool test_tensor_equality_with_zero() {
    Tensor<float> a({3, 3}, 0.0f);
    Tensor<float> b({3, 3}, -0.0f);
    
    // In IEEE 754, +0.0 == -0.0
    SIMPLE_ASSERT(a == b, "Positive and negative zero should be equal");
    
    return true;
}

bool test_tensor_empty_tensors() {
    Tensor<int> a({0});
    Tensor<int> b({0});
    
    SIMPLE_ASSERT(a == b, "Empty tensors should be equal");
    SIMPLE_ASSERT(a.approx_equal(b), "Empty tensors should be approx_equal");
    
    return true;
}

bool test_tensor_single_element() {
    Tensor<double> a({1}, 42.0);
    Tensor<double> b({1}, 42.0);
    Tensor<double> c({1}, 42.0 + 1e-9);
    
    SIMPLE_ASSERT(a == b, "Single element tensors should be equal");
    SIMPLE_ASSERT(a.approx_equal(c, 1e-8), "Single element approx_equal should work");
    
    return true;
}

// =============================================================================
// Performance Benchmarks
// =============================================================================

bool test_tensor_comparison_performance() {
    std::cout << colors::cyan() << "Test: Comparison Performance (informational)" << colors::reset() << "\n";

    const size_t N = 1000;
    Tensor<float> a({ N, N }, 1.0f);
    Tensor<float> b({ N, N }, 1.0f + 1e-7f);

    // Increase iterations for measurable timing
    const size_t iterations = 1000;

    auto time_exact = measure_perf([&]() {
        DoNotOptimize(a == b);
        }, iterations);

    auto time_approx = measure_perf([&]() {
        DoNotOptimize(a.approx_equal(b));
        }, iterations);

    std::cout << "  operator==:    " << format_time(time_exact) << "\n";
    std::cout << "  approx_equal:  " << format_time(time_approx) << "\n";

    // Guard against division by zero
    if (time_exact > 0.0) {
        double ratio = time_approx / time_exact;
        std::cout << "  Ratio (approx/exact): " << std::fixed
            << std::setprecision(2) << ratio << "x\n";
    }
    else {
        std::cout << "  Ratio: (too fast to measure reliably)\n";
    }

    return true;
}

bool test_tensor_hash_performance() {
    const size_t N = 1000;
    Tensor<float> a({N, N}, 1.0f);
    
    auto time_hash = measure_perf([&]() {
        size_t hash = std::hash<Tensor<float>>{}(a);
        DoNotOptimize(hash);
    }, 100);
    
    std::cout << "  Hash computation time: " << format_time(time_hash) << "\n";
    
    return true;
}

// =============================================================================
// Main Test Runner
// =============================================================================

bool test_TensorCompareSerialize() {
    PRINT_HEADER(TENSOR COMPARISONS AND EQUALITY)
    
    TestRunner runner;
    
    std::cout << colors::bold() << "=== Test Suite 1: operator== (Exact Comparison) ===" 
              << colors::reset() << "\n\n";
    runner.run_test("Exact equality - integers", test_tensor_exact_equality_integers);
    runner.run_test("Exact equality - floats", test_tensor_exact_equality_floats);
    runner.run_test("Exact equality - views", test_tensor_exact_equality_with_views);
    runner.run_test("Shape mismatch detection", test_tensor_shape_mismatch);
    
    std::cout << "\n" << colors::bold() << "=== Test Suite 2: approx_equal (Epsilon-Based) ===" 
              << colors::reset() << "\n\n";
    runner.run_test("Approx equal - default epsilon", test_tensor_approx_equal_default_epsilon);
    runner.run_test("Approx equal - custom epsilon", test_tensor_approx_equal_custom_epsilon);
    runner.run_test("Approx equal - relative tolerance", test_tensor_approx_equal_relative_tolerance);
    runner.run_test("Approx equal - with views", test_tensor_approx_equal_with_views);
    runner.run_test("Approx equal - integers", test_tensor_approx_equal_integers);
    
    std::cout << "\n" << colors::bold() << "=== Test Suite 3: std::hash (Unordered Containers) ===" 
              << colors::reset() << "\n\n";
    runner.run_test("Hash consistency", test_tensor_hash_consistency);
    runner.run_test("Unordered map usage", test_tensor_unordered_map);
    runner.run_test("Unordered set usage", test_tensor_unordered_set);
    runner.run_test("Hash with NaN", test_tensor_hash_with_nan);
    runner.run_test("Hash with different strides", test_tensor_hash_different_strides);
    
    std::cout << "\n" << colors::bold() << "=== Test Suite 4: EqualDispatcher (Policy-Based) ===" 
              << colors::reset() << "\n\n";
    runner.run_test("EqualDispatcher basic", test_tensor_equal_dispatcher_basic);
    runner.run_test("EqualDispatcher hybrid policy", test_tensor_equal_dispatcher_hybrid);
    
    std::cout << "\n" << colors::bold() << "=== Test Suite 5: Edge Cases ===" 
              << colors::reset() << "\n\n";
    runner.run_test("Equality with infinity", test_tensor_equality_with_infinity);
    runner.run_test("Equality with zero", test_tensor_equality_with_zero);
    runner.run_test("Empty tensors", test_tensor_empty_tensors);
    runner.run_test("Single element tensors", test_tensor_single_element);
    
    std::cout << "\n" << colors::bold() << "=== Performance Benchmarks ===" 
              << colors::reset() << "\n\n";
    runner.run_test("Comparison performance", test_tensor_comparison_performance);
    runner.run_test("Hash performance", test_tensor_hash_performance);
    
    return runner.print_summary() == 0;
}

} // namespace cpp_utilities::testing
