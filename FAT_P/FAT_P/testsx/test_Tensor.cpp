/**
 * @file test_Tensor.cpp
 * @brief Comprehensive unit tests for Tensor.h
 */
/*
FATP_META:
  meta_version: 1
  component: Tensor
  file_role: test
  path: tests/test_Tensor.cpp
  namespace: fat_p::testing::tensor
  summary: "Unit tests for Tensor."
  related:
    docs_search: "Tensor"
    headers:
      - fat_p/Tensor.h
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

#include <iostream>
#include <numeric>
#include <vector>

#include "Tensor.h"
#include "FatPTest.h"

namespace fat_p::testing::tensor
{

// =============================================================================
// Basic Construction and Access Tests
// =============================================================================

FATP_TEST_CASE(construction) {
    // Default construction
    Tensor<double> empty;
    FATP_ASSERT_EQ(empty.size(), size_t(0), "Empty tensor should have size 0");
    FATP_ASSERT_TRUE(empty.empty(), "Empty tensor should be empty");
    
    // 1D tensor
    Tensor<double> vec({10});
    FATP_ASSERT_EQ(vec.size(), size_t(10), "Vector should have size 10");
    FATP_ASSERT_EQ(vec.ndim(), size_t(1), "Vector should be 1D");
    FATP_ASSERT_EQ(vec.dim(0), size_t(10), "Vector first dimension should be 10");
    
    // 2D tensor
    Tensor<double> mat({5, 4});
    FATP_ASSERT_EQ(mat.size(), size_t(20), "Matrix should have size 20");
    FATP_ASSERT_EQ(mat.ndim(), size_t(2), "Matrix should be 2D");
    FATP_ASSERT_EQ(mat.dim(0), size_t(5), "Matrix rows should be 5");
    FATP_ASSERT_EQ(mat.dim(1), size_t(4), "Matrix cols should be 4");
    
    // 3D tensor
    Tensor<double> tensor3d({2, 3, 4});
    FATP_ASSERT_EQ(tensor3d.size(), size_t(24), "3D tensor should have size 24");
    FATP_ASSERT_EQ(tensor3d.ndim(), size_t(3), "Should be 3D");
    
    // Construction with initial value
    Tensor<double> initialized({5, 5}, 3.14);
    FATP_ASSERT_EQ(initialized(2, 2), 3.14, "Should be initialized to 3.14");
    
    return true;
}

FATP_TEST_CASE(element_access) {
    Tensor<double> mat({3, 4});
    
    // Variadic indexing
    mat(0, 0) = 1.0;
    mat(1, 2) = 2.5;
    mat(2, 3) = 3.7;
    
    FATP_ASSERT_TRUE(mat(0, 0) == 1.0, "Element (0,0) should be 1.0");
    FATP_ASSERT_TRUE(mat(1, 2) == 2.5, "Element (1,2) should be 2.5");
    FATP_ASSERT_TRUE(mat(2, 3) == 3.7, "Element (2,3) should be 3.7");
    
    // Linear indexing
    mat[0] = 10.0;
    FATP_ASSERT_EQ(mat[0], 10.0, "Linear index 0 should work");
    
    // Bounds checking
    bool caught_exception = false;
    try {
        mat.at(10, 10);
    } catch (const std::out_of_range&) {
        caught_exception = true;
    }
    FATP_ASSERT_TRUE(caught_exception, "Should throw on out of bounds access");
    
    return true;
}

FATP_TEST_CASE(copy_semantics) {
    Tensor<double> original({3, 3}, 5.0);
    
    // Copy constructor
    Tensor<double> copy1(original);
    FATP_ASSERT_TRUE(copy1.shape() == original.shape(), "Copy should have same shape");
    FATP_ASSERT_TRUE(copy1(1, 1) == 5.0, "Copy should have same values");
    
    // Modify copy (should not affect original)
    copy1(1, 1) = 99.0;
    FATP_ASSERT_TRUE(original(1, 1) == 5.0, "Original should be unchanged");
    FATP_ASSERT_TRUE(copy1(1, 1) == 99.0, "Copy should be modified");
    
    // Copy assignment
    Tensor<double> copy2({2, 2});
    copy2 = original;
    FATP_ASSERT_TRUE(copy2.shape() == original.shape(), "Assigned copy should have same shape");
    FATP_ASSERT_TRUE(copy2(1, 1) == 5.0, "Assigned copy should have same values");
    
    return true;
}

FATP_TEST_CASE(move_semantics) {
    Tensor<double> original({100, 100});
    original.fill(42.0);
    double* original_ptr = original.data();
    
    // Move constructor
    Tensor<double> moved(std::move(original));
    FATP_ASSERT_EQ(moved.data(), original_ptr, "Move should transfer ownership");
    FATP_ASSERT_TRUE(moved(50, 50) == 42.0, "Moved tensor should have correct values");
    FATP_ASSERT_TRUE(original.empty(), "Original should be empty after move");
    
    // Move assignment
    Tensor<double> target({10, 10});
    target = std::move(moved);
    FATP_ASSERT_EQ(target.data(), original_ptr, "Move assignment should transfer ownership");
    
    return true;
}

// =============================================================================
// Iterator Policy Tests
// =============================================================================

FATP_TEST_CASE(row_major_iterator) {
    RowMajorTensor<int> mat({3, 4});
    
    // Fill with sequential values
    int value = 0;
    for (auto& elem : mat) {
        elem = value++;
    }
    
    // Check row-major order
    FATP_ASSERT_TRUE(mat(0, 0) == 0, "Should be 0");
    FATP_ASSERT_TRUE(mat(0, 1) == 1, "Should be 1");
    FATP_ASSERT_TRUE(mat(0, 2) == 2, "Should be 2");
    FATP_ASSERT_TRUE(mat(0, 3) == 3, "Should be 3");
    FATP_ASSERT_TRUE(mat(1, 0) == 4, "Should be 4");
    FATP_ASSERT_TRUE(mat(1, 1) == 5, "Should be 5");
    
    // Verify iteration order
    std::vector<int> collected;
    for (const auto& elem : mat) {
        collected.push_back(elem);
    }
    
    FATP_ASSERT_EQ(collected.size(), 12, "Should collect 12 elements");
    for (size_t i = 0; i < collected.size(); ++i) {
        FATP_ASSERT_EQ(collected[i], static_cast<int>(i), "Elements should be sequential");
    }
    
    return true;
}

FATP_TEST_CASE(column_major_iterator) {
    ColumnMajorTensor<int> mat({3, 4});
    
    // Fill matrix with known values (row-major style for comparison)
    for (size_t i = 0; i < 3; ++i) {
        for (size_t j = 0; j < 4; ++j) {
            mat(i, j) = static_cast<int>(i * 4 + j);
        }
    }
    
    // Iterate in column-major order
    std::vector<int> collected;
    for (const auto& elem : mat) {
        collected.push_back(elem);
    }
    
    FATP_ASSERT_EQ(collected.size(), 12, "Should collect 12 elements");
    
    // Column-major order: col 0 (0, 4, 8), col 1 (1, 5, 9), etc.
    std::vector<int> expected = {0, 4, 8, 1, 5, 9, 2, 6, 10, 3, 7, 11};
    for (size_t i = 0; i < collected.size(); ++i) {
        FATP_ASSERT_EQ(collected[i], expected[i], "Column-major order should match expected");
    }
    
    return true;
}

FATP_TEST_CASE(strided_iterator) {
    StridedTensor<double> mat({4, 5});
    
    // Fill with values
    for (size_t i = 0; i < 4; ++i) {
        for (size_t j = 0; j < 5; ++j) {
            mat(i, j) = static_cast<double>(i * 5 + j);
        }
    }
    
    // Get a column view (has stride)
    auto col = mat.col(2);
    
    std::vector<double> collected;
    for (const auto& elem : col) {
        collected.push_back(elem);
    }
    
    // Column 2 should contain: 2, 7, 12, 17
    FATP_ASSERT_GE(collected.size(), 4, "Should have at least 4 elements");
    FATP_ASSERT_EQ(collected[0], 2.0, "First element should be 2");
    
    return true;
}

FATP_TEST_CASE(blocked_iterator) {
    constexpr size_t BlockSize = 2;
    BlockedTensor<int, BlockSize> mat({4, 4});
    
    // Fill with sequential values
    for (size_t i = 0; i < 4; ++i) {
        for (size_t j = 0; j < 4; ++j) {
            mat(i, j) = static_cast<int>(i * 4 + j);
        }
    }
    
    // Iterate in blocked order
    std::vector<int> collected;
    for (const auto& elem : mat) {
        collected.push_back(elem);
    }
    
    FATP_ASSERT_EQ(collected.size(), 16, "Should collect 16 elements");
    
    // Blocked order with 2x2 blocks:
    // Block (0,0): 0, 1, 4, 5
    // Block (0,1): 2, 3, 6, 7
    // Block (1,0): 8, 9, 12, 13
    // Block (1,1): 10, 11, 14, 15
    
    return true;
}

FATP_TEST_CASE(iterator_operations) {
    Tensor<double> vec({10});
    
    // Fill with values
    for (size_t i = 0; i < 10; ++i) {
        vec[i] = static_cast<double>(i);
    }
    
    // Test iterator arithmetic
    auto it = vec.begin();
    FATP_ASSERT_EQ(*it, 0.0, "Begin should point to first element");
    
    ++it;
    FATP_ASSERT_EQ(*it, 1.0, "Increment should move to next element");
    
    it += 3;
    FATP_ASSERT_EQ(*it, 4.0, "Should support += operator");
    
    --it;
    FATP_ASSERT_EQ(*it, 3.0, "Decrement should work");
    
    // Test iterator comparison
    auto it2 = vec.begin();
    FATP_ASSERT_TRUE(it != it2, "Different positions should not be equal");
    
    // Test const iterators
    const Tensor<double>& const_vec = vec;
    auto cit = const_vec.cbegin();
    FATP_ASSERT_EQ(*cit, 0.0, "Const iterator should work");
    
    return true;
}

// =============================================================================
// View and Slicing Tests
// =============================================================================

FATP_TEST_CASE(views) {
    Tensor<double> mat({10, 10});
    
    // Fill with sequential values
    for (size_t i = 0; i < 10; ++i) {
        for (size_t j = 0; j < 10; ++j) {
            mat(i, j) = static_cast<double>(i * 10 + j);
        }
    }
    
    // Create submatrix view
    auto submat = mat.view({2, 3}, {5, 7});
    FATP_ASSERT_EQ(submat.dim(0), 3, "Submatrix should have 3 rows");
    FATP_ASSERT_EQ(submat.dim(1), 4, "Submatrix should have 4 columns");
    
    // Modify view
    submat(0, 0) = 999.0;
    FATP_ASSERT_TRUE(mat(2, 3) == 999.0, "View modification should affect original");
    
    return true;
}

FATP_TEST_CASE(row_column_views) {
    Tensor<double> mat({5, 4});
    
    // Fill matrix
    for (size_t i = 0; i < 5; ++i) {
        for (size_t j = 0; j < 4; ++j) {
            mat(i, j) = static_cast<double>(i * 4 + j);
        }
    }
    
    // Test row view
    auto row = mat.row(2);
    FATP_ASSERT_EQ(row.dim(0), 1, "Row should have 1 row");
    FATP_ASSERT_EQ(row.dim(1), 4, "Row should have 4 columns");
    FATP_ASSERT_TRUE(row(0, 0) == 8.0, "Row values should be correct");
    
    // Test column view
    auto col = mat.col(1);
    FATP_ASSERT_EQ(col.dim(0), 5, "Column should have 5 rows");
    FATP_ASSERT_EQ(col.dim(1), 1, "Column should have 1 column");
    FATP_ASSERT_TRUE(col(0, 0) == 1.0, "Column values should be correct");
    
    return true;
}

FATP_TEST_CASE(transpose) {
    Tensor<double> mat({3, 4});
    
    // Fill matrix
    for (size_t i = 0; i < 3; ++i) {
        for (size_t j = 0; j < 4; ++j) {
            mat(i, j) = static_cast<double>(i * 4 + j);
        }
    }
    
    // Transpose
    auto transposed = mat.transpose();
    FATP_ASSERT_EQ(transposed.dim(0), 4, "Transposed should have 4 rows");
    FATP_ASSERT_EQ(transposed.dim(1), 3, "Transposed should have 3 columns");
    
    // Check values
    FATP_ASSERT_TRUE(transposed(0, 0) == mat(0, 0), "Diagonal should match");
    FATP_ASSERT_TRUE(transposed(1, 2) == mat(2, 1), "Off-diagonal should be swapped");
    
    return true;
}

FATP_TEST_CASE(reshape) {
    Tensor<double> vec({12});
    
    // Fill with sequential values
    for (size_t i = 0; i < 12; ++i) {
        vec[i] = static_cast<double>(i);
    }
    
    // Reshape to matrix
    auto mat = vec.reshape({3, 4});
    FATP_ASSERT_EQ(mat.dim(0), 3, "Reshaped should have 3 rows");
    FATP_ASSERT_EQ(mat.dim(1), 4, "Reshaped should have 4 columns");
    FATP_ASSERT_TRUE(mat(0, 0) == 0.0, "Values should be preserved");
    FATP_ASSERT_TRUE(mat(1, 0) == 4.0, "Values should follow row-major order");
    
    return true;
}

FATP_TEST_CASE(view_lifetime) {
    Tensor<double> view;
    
    {
        Tensor<double> temp({5, 5}, 42.0);
        view = temp.row(2);
    }
    // temp is destroyed, but view should still be valid (shared_ptr)
    
    FATP_ASSERT_TRUE(view(0, 0) == 42.0, "View should survive original tensor destruction");
    
    return true;
}

// =============================================================================
// Operations Tests
// =============================================================================

FATP_TEST_CASE(element_wise_operations) {
    Tensor<double> a({3, 3}, 2.0);
    Tensor<double> b({3, 3}, 3.0);
    
    // Addition
    auto c = a + b;
    FATP_ASSERT_TRUE(c(1, 1) == 5.0, "Addition should work");
    
    // Subtraction
    auto d = b - a;
    FATP_ASSERT_TRUE(d(1, 1) == 1.0, "Subtraction should work");
    
    // Multiplication
    auto e = a * b;
    FATP_ASSERT_TRUE(e(1, 1) == 6.0, "Multiplication should work");
    
    // Scalar operations
    auto f = a * 2.0;
    FATP_ASSERT_TRUE(f(1, 1) == 4.0, "Scalar multiplication should work");
    
    auto g = a / 2.0;
    FATP_ASSERT_TRUE(g(1, 1) == 1.0, "Scalar division should work");
    
    return true;
}

FATP_TEST_CASE(reductions) {
    Tensor<double> mat({3, 4});
    
    // Fill with sequential values
    for (size_t i = 0; i < 12; ++i) {
        mat[i] = static_cast<double>(i);
    }
    
    // Sum: 0+1+2+...+11 = 66
    double s = mat.sum();
    FATP_ASSERT_EQ(s, 66.0, "Sum should be 66");
    
    // Mean: 66/12 = 5.5
    double m = mat.mean();
    FATP_ASSERT_LT(std::abs(m - 5.5), 1e-10, "Mean should be 5.5");
    
    // Max
    double max_val = mat.max();
    FATP_ASSERT_EQ(max_val, 11.0, "Max should be 11");
    
    // Min
    double min_val = mat.min();
    FATP_ASSERT_EQ(min_val, 0.0, "Min should be 0");
    
    return true;
}

FATP_TEST_CASE(fill) {
    Tensor<double> mat({5, 5});
    mat.fill(7.5);
    
    // Check all elements
    for (size_t i = 0; i < 5; ++i) {
        for (size_t j = 0; j < 5; ++j) {
            FATP_ASSERT_TRUE(mat(i, j) == 7.5, "All elements should be 7.5");
        }
    }
    
    return true;
}

// =============================================================================
// Performance Benchmarks
// =============================================================================

void benchmark_iterators() {
    std::cout << "\n" << colors::cyan() << "Iterator Policy Benchmarks:" << colors::reset() << "\n\n";
    
    const size_t size = 1000;
    
    // Row-major iteration
    RowMajorTensor<double> row_major({size, size});
    double row_time = measure_perf([&row_major]() {
        double sum = 0.0;
        for (const auto& elem : row_major) {
            sum += elem;
        }
        DoNotOptimize(sum);
    }, 100, 10);
    std::cout << "Row-major iteration (1000x1000): " << format_time(row_time) << "\n";
    
    // Column-major iteration
    ColumnMajorTensor<double> col_major({size, size});
    double col_time = measure_perf([&col_major]() {
        double sum = 0.0;
        for (const auto& elem : col_major) {
            sum += elem;
        }
        DoNotOptimize(sum);
    }, 100, 10);
    std::cout << "Column-major iteration (1000x1000): " << format_time(col_time) << "\n";
    
    // Strided iteration
    StridedTensor<double> strided({size, size});
    auto col_view = strided.col(size / 2);
    double strided_time = measure_perf([&col_view]() {
        double sum = 0.0;
        for (const auto& elem : col_view) {
            sum += elem;
        }
        DoNotOptimize(sum);
    }, 1000, 100);
    std::cout << "Strided iteration (column view): " << format_time(strided_time) << "\n";
    
    // Blocked iteration
    BlockedTensor<double, 64> blocked({size, size});
    double blocked_time = measure_perf([&blocked]() {
        double sum = 0.0;
        for (const auto& elem : blocked) {
            sum += elem;
        }
        DoNotOptimize(sum);
    }, 100, 10);
    std::cout << "Blocked iteration (64x64 blocks): " << format_time(blocked_time) << "\n";
}

void benchmark_element_access() {
    std::cout << "\n" << colors::cyan() << "Element Access Benchmarks:" << colors::reset() << "\n\n";
    
    Tensor<double> mat({1000, 1000});
    
    // Variadic indexing
    double access_time = measure_perf([&mat, i=0, j=0]() mutable {
        double val = mat(i, j);
        DoNotOptimize(val);
        ++j;
        if (j >= 1000) { j = 0; ++i; if (i >= 1000) i = 0; }
    }, 100000, 1000);
    std::cout << "Variadic indexing mat(i,j): " << format_time(access_time) << "\n";
    
    // Linear indexing
    double linear_time = measure_perf([&mat, idx=0]() mutable {
        double val = mat[idx];
        DoNotOptimize(val);
        ++idx;
        if (idx >= 1000000) idx = 0;
    }, 100000, 1000);
    std::cout << "Linear indexing mat[idx]: " << format_time(linear_time) << "\n";
}

void benchmark_operations() {
    std::cout << "\n" << colors::cyan() << "Operation Benchmarks:" << colors::reset() << "\n\n";
    
    Tensor<double> a({1000, 1000}, 1.0);
    Tensor<double> b({1000, 1000}, 2.0);
    
    // Element-wise addition
    double add_time = measure_perf([&a, &b]() {
        auto c = a + b;
        DoNotOptimize(c);
    }, 100, 10);
    std::cout << "Element-wise addition (1000x1000): " << format_time(add_time) << "\n";
    
    // Scalar multiplication
    double scalar_time = measure_perf([&a]() {
        auto c = a * 2.0;
        DoNotOptimize(c);
    }, 100, 10);
    std::cout << "Scalar multiplication (1000x1000): " << format_time(scalar_time) << "\n";
    
    // Sum reduction
    double sum_time = measure_perf([&a]() {
        double s = a.sum();
        DoNotOptimize(s);
    }, 100, 10);
    std::cout << "Sum reduction (1000x1000): " << format_time(sum_time) << "\n";
}

// =============================================================================
// Broadcasting Tests (v4.2)
// =============================================================================

FATP_TEST_CASE(broadcasting_shape) {
    using fat_p::Tensor;
    Tensor<double> a({3, 4});
    Tensor<double> b({3, 4});
    
    // Same shape - should broadcast
    auto result1 = a.compute_broadcast_shape(b.shape());
    FATP_ASSERT_TRUE(result1.has_value(), "Same shapes should broadcast");
    FATP_ASSERT_TRUE(result1.value() == std::vector<size_t>({3, 4}), "Broadcast shape should be {3,4}");
    
    // Scalar-like (single element) broadcast
    Tensor<double> scalar({1, 1});
    auto result2 = a.compute_broadcast_shape(scalar.shape());
    FATP_ASSERT_TRUE(result2.has_value(), "Scalar should broadcast to any shape");
    FATP_ASSERT_TRUE(result2.value() == std::vector<size_t>({3, 4}), "Broadcast should expand scalar");
    
    // 1D to 2D broadcast
    Tensor<double> vec({4});
    auto result3 = a.compute_broadcast_shape(vec.shape());
    FATP_ASSERT_TRUE(result3.has_value(), "1D to 2D should broadcast");
    FATP_ASSERT_TRUE(result3.value() == std::vector<size_t>({3, 4}), "Should broadcast to {3,4}");
    
    // Incompatible shapes
    Tensor<double> incompatible({3, 5});
    auto result4 = a.compute_broadcast_shape(incompatible.shape());
    FATP_ASSERT_FALSE(result4.has_value(), "Incompatible shapes should fail");
    
    return true;
}

FATP_TEST_CASE(broadcast_to) {
    using fat_p::Tensor;
    Tensor<double> scalar({1}, 5.0);
    
    // Broadcast scalar to 2D
    auto result = scalar.broadcast_to({3, 4});
    FATP_ASSERT_TRUE(result.has_value(), "Broadcast should succeed");
    
    auto& broadcasted = result.value();
    FATP_ASSERT_TRUE(broadcasted.shape() == std::vector<size_t>({3, 4}), "Shape should be {3,4}");
    FATP_ASSERT_EQ(broadcasted.size(), 12, "Size should be 12");
    
    // All elements should be 5.0
    for (size_t i = 0; i < 3; ++i) {
        for (size_t j = 0; j < 4; ++j) {
            FATP_ASSERT_TRUE(broadcasted(i, j) == 5.0, "All elements should be 5.0");
        }
    }
    
    return true;
}

FATP_TEST_CASE(broadcasting_operations) {
    using fat_p::Tensor;
    Tensor<double> a({3, 4});
    Tensor<double> b({3, 4});
    
    // Fill with test data
    for (size_t i = 0; i < a.size(); ++i) {
        a[i] = static_cast<double>(i);
        b[i] = static_cast<double>(i * 2);
    }
    
    // Same shape operations (fast path)
    auto add_result = a.add_safe(b);
    FATP_ASSERT_TRUE(add_result.has_value(), "add_safe should succeed for same shapes");
    FATP_ASSERT_EQ(add_result.value()[0], 0.0, "First element should be 0");
    FATP_ASSERT_EQ(add_result.value()[1], 3.0, "Second element should be 1+2=3");
    
    auto sub_result = a.sub_safe(b);
    FATP_ASSERT_TRUE(sub_result.has_value(), "sub_safe should succeed");
    
    auto mul_result = a.mul_safe(b);
    FATP_ASSERT_TRUE(mul_result.has_value(), "mul_safe should succeed");
    
    // Broadcasting with scalar-like tensor
    Tensor<double> scalar({1}, 10.0);
    auto broad_result = scalar.add_safe(a);
    FATP_ASSERT_TRUE(broad_result.has_value(), "Broadcasting add should work");
    
    return true;
}

FATP_TEST_CASE(is_broadcastable) {
    using fat_p::Tensor;
    Tensor<double> a({3, 4});
    Tensor<double> b({3, 4});
    Tensor<double> c({1, 4});
    Tensor<double> d({3, 5});
    
    FATP_ASSERT_TRUE(a.is_broadcastable(b.shape()), "Same shapes should be broadcastable");
    FATP_ASSERT_TRUE(a.is_broadcastable(c.shape()), "{3,4} and {1,4} should be broadcastable");
    FATP_ASSERT_FALSE(a.is_broadcastable(d.shape()), "{3,4} and {3,5} should not be broadcastable");
    
    return true;
}

// =============================================================================
// Expected.h Integration Tests (v4.2)
// =============================================================================

FATP_TEST_CASE(safe_operations) {
    using fat_p::Tensor;
    Tensor<double> a({3, 4});
    Tensor<double> b({3, 4});
    a.fill(1.0);
    b.fill(2.0);
    
    // Successful operations
    auto add_result = a.add_safe(b);
    FATP_ASSERT_TRUE(add_result.has_value(), "add_safe should succeed with matching shapes");
    FATP_ASSERT_TRUE(add_result.value().shape() == a.shape(), "Result should have same shape");
    FATP_ASSERT_EQ(add_result.value()[0], 3.0, "Addition should work correctly");
    
    auto sub_result = a.sub_safe(b);
    FATP_ASSERT_TRUE(sub_result.has_value(), "sub_safe should succeed");
    FATP_ASSERT_EQ(sub_result.value()[0], -1.0, "Subtraction should work correctly");
    
    auto mul_result = a.mul_safe(b);
    FATP_ASSERT_TRUE(mul_result.has_value(), "mul_safe should succeed");
    FATP_ASSERT_EQ(mul_result.value()[0], 2.0, "Multiplication should work correctly");
    
    // Failed operations (shape mismatch without broadcasting support)
    Tensor<double> c({2, 5});
    auto fail_result = a.add_safe(c);
    FATP_ASSERT_FALSE(fail_result.has_value(), "Should fail with incompatible shapes");
    FATP_ASSERT_FALSE(fail_result.error().empty(), "Error message should not be empty");
    
    return true;
}

FATP_TEST_CASE(safe_view) {
    using fat_p::Tensor;
    Tensor<double> a({5, 5});
    a.fill(42.0);
    
    // Valid view
    auto view_result = a.view_safe({0, 0}, {2, 2});
    FATP_ASSERT_TRUE(view_result.has_value(), "Valid view should succeed");
    FATP_ASSERT_TRUE(view_result.value().shape() == std::vector<size_t>({2, 2}), "View shape should be {2,2}");
    
    // Invalid view (start >= end)
    auto bad_view1 = a.view_safe({2, 2}, {1, 1});
    FATP_ASSERT_FALSE(bad_view1.has_value(), "Invalid start/end should fail");
    
    // Invalid view (out of bounds)
    auto bad_view2 = a.view_safe({0, 0}, {10, 10});
    FATP_ASSERT_FALSE(bad_view2.has_value(), "Out of bounds view should fail");
    
    // Invalid view (dimension mismatch)
    auto bad_view3 = a.view_safe({0}, {2});
    FATP_ASSERT_FALSE(bad_view3.has_value(), "Dimension mismatch should fail");
    
    return true;
}

FATP_TEST_CASE(safe_reshape) {
    using fat_p::Tensor;
    Tensor<double> a({3, 4});
    a.fill(1.0);
    
    // Valid reshape
    auto reshape_result = a.reshape_safe({2, 6});
    FATP_ASSERT_TRUE(reshape_result.has_value(), "Valid reshape should succeed");
    FATP_ASSERT_TRUE(reshape_result.value().shape() == std::vector<size_t>({2, 6}), "Shape should be {2,6}");
    FATP_ASSERT_EQ(reshape_result.value().size(), 12, "Size should be preserved");
    
    // Invalid reshape (size mismatch)
    auto bad_reshape = a.reshape_safe({2, 5});
    FATP_ASSERT_FALSE(bad_reshape.has_value(), "Size mismatch reshape should fail");
    FATP_ASSERT_FALSE(bad_reshape.error().empty(), "Error message should be provided");
    
    return true;
}

// =============================================================================
// Expression Template Tests (v4.2)
// =============================================================================

FATP_TEST_CASE(lazy_evaluation) {
    using fat_p::Tensor;
    Tensor<double> a({1000});
    Tensor<double> b({1000});
    Tensor<double> c({1000});
    
    a.fill(1.0);
    b.fill(2.0);
    c.fill(3.0);
    
    // Create lazy expression (doesn't compute yet)
    auto lazy_expr = a.lazy_add(b).lazy_add(c);
    
    // Evaluate expression (single loop)
    Tensor<double> result({1000});
    result = lazy_expr;
    
    FATP_ASSERT_EQ(result[0], 6.0, "Lazy evaluation should give correct result");
    FATP_ASSERT_EQ(result[999], 6.0, "All elements should be computed");
    
    return true;
}

FATP_TEST_CASE(lazy_operations) {
    using fat_p::Tensor;
    Tensor<double> a({100});
    Tensor<double> b({100});
    
    for (size_t i = 0; i < 100; ++i) {
        a[i] = static_cast<double>(i);
        b[i] = static_cast<double>(i * 2);
    }
    
    // Lazy add
    auto lazy_add = a.lazy_add(b);
    Tensor<double> result_add({100});
    result_add = lazy_add;
    FATP_ASSERT_EQ(result_add[0], 0.0, "Lazy add: 0+0=0");
    FATP_ASSERT_EQ(result_add[1], 3.0, "Lazy add: 1+2=3");
    FATP_ASSERT_EQ(result_add[99], 297.0, "Lazy add: 99+198=297");
    
    // Lazy subtract
    auto lazy_sub = a.lazy_sub(b);
    Tensor<double> result_sub({100});
    result_sub = lazy_sub;
    FATP_ASSERT_EQ(result_sub[0], 0.0, "Lazy sub: 0-0=0");
    FATP_ASSERT_EQ(result_sub[1], -1.0, "Lazy sub: 1-2=-1");
    
    // Lazy multiply
    auto lazy_mul = a.lazy_mul(b);
    Tensor<double> result_mul({100});
    result_mul = lazy_mul;
    FATP_ASSERT_EQ(result_mul[0], 0.0, "Lazy mul: 0*0=0");
    FATP_ASSERT_EQ(result_mul[2], 8.0, "Lazy mul: 2*4=8");
    
    // Lazy scalar multiply
    auto lazy_scalar = a.lazy_mul_scalar(5.0);
    Tensor<double> result_scalar({100});
    result_scalar = lazy_scalar;
    FATP_ASSERT_EQ(result_scalar[0], 0.0, "Lazy scalar: 0*5=0");
    FATP_ASSERT_EQ(result_scalar[10], 50.0, "Lazy scalar: 10*5=50");
    
    return true;
}

FATP_TEST_CASE(expression_chaining) {
    using fat_p::Tensor;
    Tensor<double> a({50});
    Tensor<double> b({50});
    Tensor<double> c({50});
    Tensor<double> d({50});
    
    a.fill(1.0);
    b.fill(2.0);
    c.fill(3.0);
    d.fill(4.0);
    
    // Chain multiple operations (single loop evaluation)
    auto expr = a.lazy_add(b).lazy_add(c).lazy_add(d);
    Tensor<double> result({50});
    result = expr;
    
    FATP_ASSERT_EQ(result[0], 10.0, "Chained expression: 1+2+3+4=10");
    FATP_ASSERT_EQ(result[25], 10.0, "All elements should be 10");
    
    // Mixed operations
    auto mixed = a.lazy_add(b).lazy_mul(c);  // (a+b)*c
    Tensor<double> result2({50});
    result2 = mixed;
    
    FATP_ASSERT_EQ(result2[0], 9.0, "Mixed: (1+2)*3=9");
    
    return true;
}

// =============================================================================
// Parallel Operations Tests (v4.3)
// =============================================================================

FATP_TEST_CASE(parallel_operations) {
    using fat_p::Tensor;
    
    // Test parallel addition with large tensor (>100K threshold)
    Tensor<double> a({500, 500});  // 250K elements
    Tensor<double> b({500, 500});
    a.fill(1.0);
    b.fill(2.0);
    
    auto result = a + b;
    FATP_ASSERT_EQ(result[0], 3.0, "Parallel addition should work");
    FATP_ASSERT_EQ(result[result.size() - 1], 3.0, "All elements computed");
    
    // Test parallel subtraction
    auto sub_result = a - b;
    FATP_ASSERT_EQ(sub_result[0], -1.0, "Parallel subtraction should work");
    
    // Test parallel multiplication
    auto mul_result = a * b;
    FATP_ASSERT_EQ(mul_result[0], 2.0, "Parallel multiplication should work");
    
    return true;
}

FATP_TEST_CASE(contract_exceptions) {
    using fat_p::Tensor;
    using fat_p::DomainContractError;
    using fat_p::LogicContractError;
    using fat_p::OutOfRangeContractError;
    
    // Test DomainContractError for shape mismatch
    Tensor<double> a({3, 4});
    Tensor<double> b({2, 5});
    a.fill(1.0);
    b.fill(2.0);
    
    bool caught_domain_error = false;
    try {
        auto result = a + b;  // Shape mismatch
    } catch (const DomainContractError& e) {
        caught_domain_error = true;
        FATP_ASSERT_TRUE(std::string(e.what()).find("Shape") != std::string::npos,
                     "Error message should mention shape");
    } catch (...) {
        FATP_ASSERT_TRUE(false, "Should throw DomainContractError specifically");
    }
    FATP_ASSERT_TRUE(caught_domain_error, "Should throw DomainContractError for shape mismatch");
    
    // Test OutOfRangeContractError for out of bounds access
    Tensor<double> t({3, 4});
    t.fill(1.0);
    bool caught_range_error = false;
    try {
        t.at(10, 10);  // Out of bounds
    } catch (const OutOfRangeContractError&) {
        caught_range_error = true;
    } catch (const std::out_of_range&) {
        // Also acceptable - OutOfRangeContractError inherits from std::out_of_range
        caught_range_error = true;
    }
    FATP_ASSERT_TRUE(caught_range_error, "Should throw OutOfRangeContractError for out of bounds access");
    
    return true;
}

FATP_TEST_CASE(parallel_threshold) {
    using fat_p::Tensor;
    
    // Small tensor (below 100K threshold) - uses serial
    Tensor<double> small({100, 100});  // 10K elements
    small.fill(5.0);
    
    Tensor<double> small2({100, 100});
    small2.fill(3.0);
    
    auto small_result = small + small2;
    FATP_ASSERT_EQ(small_result[0], 8.0, "Small tensor addition should work (serial)");
    
    // Large tensor (above 100K threshold) - uses parallel
    Tensor<double> large({400, 400});  // 160K elements
    large.fill(5.0);
    
    Tensor<double> large2({400, 400});
    large2.fill(3.0);
    
    auto large_result = large + large2;
    FATP_ASSERT_EQ(large_result[0], 8.0, "Large tensor addition should work (parallel)");
    FATP_ASSERT_EQ(large_result[large_result.size() - 1], 8.0, "All elements computed correctly");
    
    return true;
}

// =============================================================================
// v5.1 Tests - Enhanced Safety Features
// =============================================================================

FATP_TEST_CASE(enhanced_bounds_checking) {
    Tensor<double> mat({10, 20});
    
    // Valid access
    try {
        mat.at(5, 10) = 42.0;
        FATP_ASSERT_TRUE(mat.at(5, 10) == 42.0, "Valid at() should work");
    } catch (...) {
        FATP_ASSERT_TRUE(false, "Valid access should not throw");
        return false;
    }
    
    // Invalid access - out of bounds
    bool caught_error = false;
    try {
        mat.at(15, 25) = 99.0;
    } catch (const std::out_of_range& e) {
        caught_error = true;
        (void)e;  // Used only in debug builds
        #ifndef NDEBUG
        std::string msg = e.what();
        FATP_ASSERT_TRUE(msg.find("dimension") != std::string::npos ||
                     msg.find("out of range") != std::string::npos,
                     "Error message should be detailed in debug mode");
        #endif
    }
    FATP_ASSERT_TRUE(caught_error, "Should throw on out of bounds access");
    
    #ifndef NDEBUG
    bool caught_dimension_error = false;
    try {
        mat.at(5);  // Only 1 index for 2D tensor
    } catch (const std::out_of_range& e) {
        caught_dimension_error = true;
        std::string msg = e.what();
        FATP_ASSERT_TRUE(msg.find("mismatch") != std::string::npos,
                     "Should mention dimension mismatch");
    }
    FATP_ASSERT_TRUE(caught_dimension_error, "Should throw on dimension mismatch");
    #endif
    
    return true;
}

FATP_TEST_CASE(at_linear) {
    Tensor<int> vec({100});
    
    // Valid linear access
    try {
        vec.at_linear(50) = 42;
        FATP_ASSERT_EQ(vec.at_linear(50), 42, "at_linear should work");
    } catch (...) {
        FATP_ASSERT_TRUE(false, "Valid linear access should not throw");
        return false;
    }
    
    // Invalid linear access
    bool caught_error = false;
    try {
        vec.at_linear(150) = 99;
    } catch (const std::out_of_range&) {
        caught_error = true;
    }
    FATP_ASSERT_TRUE(caught_error, "Should throw on out of bounds");
    
    // Test with const tensor
    const Tensor<int> const_vec({50});
    try {
        int val = const_vec.at_linear(25);
        (void)val;
    } catch (...) {
        FATP_ASSERT_TRUE(false, "Const linear access should work");
        return false;
    }
    
    return true;
}

FATP_TEST_CASE(lifetime_tracking_integration) {
    #ifndef NDEBUG
    {
        Tensor<double> tensor({10, 10}, 1.0);
        auto tracker = tensor.create_tracker("test_tensor");
        auto view = tracker.create_view();
        
        FATP_ASSERT_TRUE(view.is_valid(), "View should be valid while tensor exists");
        FATP_ASSERT_TRUE((*view)(5, 5) == 1.0, "View should access tensor data");
    }
    
    {
        Tensor<double> tensor({20, 30});
        auto slice = tensor.create_tracked_slice({5, 5}, {15, 25}, "slice");
    }
    
    {
        Tensor<double> mat({50, 100});
        auto row = mat.create_tracked_row(25, "row_25");
        auto col = mat.create_tracked_col(75, "col_75");
    }
    
    {
        Tensor<double> mat({10, 10});
        bool caught_error = false;
        try {
            auto row = mat.create_tracked_row(15, "invalid_row");
        } catch (const std::out_of_range&) {
            caught_error = true;
        }
        FATP_ASSERT_TRUE(caught_error, "Should catch invalid row index");
    }
    #else
    Tensor<double> tensor({10, 10});
    auto slice = tensor.create_tracked_slice({0, 0}, {5, 5});
    auto row = tensor.create_tracked_row(5);
    auto col = tensor.create_tracked_col(5);
    FATP_ASSERT_EQ(slice.size(), 25, "Slice should have correct size");
    #endif
    
    return true;
}

FATP_TEST_CASE(rcu_tensor_integration) {
    #if defined(FATP_USE_ATOMIC) && defined(FATP_USE_SHARED_MUTEX)
    
    {
        RCUPolicy<Tensor<double>> rcu_tensor(Tensor<double>({10, 10}));
        
        {
            auto guard = rcu_tensor.write();
            guard.update([](Tensor<double>& t) { t.fill(42.0); });
        }
        
        {
            auto guard = rcu_tensor.read();
            const auto& t = *guard;
            FATP_ASSERT_TRUE(t(5, 5) == 42.0, "RCU read should work");
        }
    }
    
    {
        RCUPolicy<Tensor<float>> weights(Tensor<float>({100, 100}));
        
        {
            auto guard = weights.write();
            guard.update([](Tensor<float>& w) {
                for (size_t i = 0; i < w.size(); ++i) {
                    w[i] = static_cast<float>(i) * 0.01f;
                }
            });
        }
        
        {
            auto guard = weights.read();
            const auto& w = *guard;
            FATP_ASSERT_LT(std::abs(w[0] - 0.0f), 0.001f, "First element should be ~0");
            FATP_ASSERT_LT(std::abs(w[100] - 1.0f), 0.001f, "Element 100 should be ~1");
        }
    }
    
    #endif
    
    return true;
}

bool test_v51_safety_comprehensive() {
    {
        Tensor<int> tensor3d({5, 6, 7});
        
        try {
            tensor3d.at(2, 3, 4) = 100;
            FATP_ASSERT_TRUE(tensor3d.at(2, 3, 4) == 100, "3D access should work");
        } catch (...) {
            FATP_ASSERT_TRUE(false, "Valid 3D access should not throw");
            return false;
        }
        
        bool caught = false;
        try {
            tensor3d.at(5, 6, 7) = 200;
        } catch (const std::out_of_range&) {
            caught = true;
        }
        FATP_ASSERT_TRUE(caught, "Should catch 3D out of bounds");
    }
    
    {
        Tensor<double> mat({100, 200});
        mat.fill(1.0);
        auto view = mat.view({10, 10}, {20, 20});
        
        try {
            view.at(5, 5) = 42.0;
            FATP_ASSERT_TRUE(view.at(5, 5) == 42.0, "View at() should work");
        } catch (...) {
            FATP_ASSERT_TRUE(false, "Valid view access should work");
            return false;
        }
    }
    
    {
        Tensor<float> small({10});
        Tensor<float> medium({1000});
        
        try {
            small.at_linear(9) = 1.0f;
            medium.at_linear(999) = 2.0f;
            
            FATP_ASSERT_EQ(small.at_linear(9), 1.0f, "Small tensor access");
            FATP_ASSERT_EQ(medium.at_linear(999), 2.0f, "Medium tensor access");
        } catch (...) {
            FATP_ASSERT_TRUE(false, "Valid linear access should work");
            return false;
        }
    }
    
    {
        const Tensor<double> const_tensor({5, 5}, 42.0);
        
        try {
            double val = const_tensor.at(2, 3);
            FATP_ASSERT_EQ(val, 42.0, "Const at() should work");
            
            val = const_tensor.at_linear(12);
            FATP_ASSERT_EQ(val, 42.0, "Const at_linear() should work");
        } catch (...) {
            FATP_ASSERT_TRUE(false, "Const access should work");
            return false;
        }
    }
    
    return true;
}

bool test_v51_zero_overhead_release() {
    #ifdef NDEBUG
    Tensor<double> tensor({100, 100});
    
    try {
        tensor.at(50, 50) = 1.0;
        tensor.at_linear(5000) = 2.0;
    } catch (...) {
        FATP_ASSERT_TRUE(false, "Basic operations should work in release");
        return false;
    }
    
    auto tracker = tensor.create_tracker();
    auto slice = tensor.create_tracked_slice({0, 0}, {10, 10});
    auto row = tensor.create_tracked_row(50);
    auto col = tensor.create_tracked_col(75);
    #endif
    
    return true;
}

} // namespace fat_p::testing::tensor

// =============================================================================
// Public Interface
// =============================================================================

namespace fat_p::testing
{

bool test_Tensor() {
    FATP_PRINT_HEADER(TENSOR WITH ITERATOR POLICIES - v5.1)
    
    TestRunner runner;
    
    // Basic tests
    FATP_RUN_TEST_NS(runner, tensor, construction);
    FATP_RUN_TEST_NS(runner, tensor, element_access);
    FATP_RUN_TEST_NS(runner, tensor, copy_semantics);
    FATP_RUN_TEST_NS(runner, tensor, move_semantics);
    
    // Iterator policy tests
    FATP_RUN_TEST_NS(runner, tensor, row_major_iterator);
    FATP_RUN_TEST_NS(runner, tensor, column_major_iterator);
    FATP_RUN_TEST_NS(runner, tensor, strided_iterator);
    FATP_RUN_TEST_NS(runner, tensor, blocked_iterator);
    FATP_RUN_TEST_NS(runner, tensor, iterator_operations);
    
    // View and slicing tests
    FATP_RUN_TEST_NS(runner, tensor, views);
    FATP_RUN_TEST_NS(runner, tensor, row_column_views);
    FATP_RUN_TEST_NS(runner, tensor, transpose);
    FATP_RUN_TEST_NS(runner, tensor, reshape);
    FATP_RUN_TEST_NS(runner, tensor, view_lifetime);
    
    // Operations tests
    FATP_RUN_TEST_NS(runner, tensor, element_wise_operations);
    FATP_RUN_TEST_NS(runner, tensor, reductions);
    FATP_RUN_TEST_NS(runner, tensor, fill);
    
    // v4.2 tests
    FATP_RUN_TEST_NS(runner, tensor, broadcasting_shape);
    FATP_RUN_TEST_NS(runner, tensor, broadcast_to);
    FATP_RUN_TEST_NS(runner, tensor, broadcasting_operations);
    FATP_RUN_TEST_NS(runner, tensor, is_broadcastable);
    FATP_RUN_TEST_NS(runner, tensor, safe_operations);
    FATP_RUN_TEST_NS(runner, tensor, safe_view);
    FATP_RUN_TEST_NS(runner, tensor, safe_reshape);
    FATP_RUN_TEST_NS(runner, tensor, lazy_evaluation);
    FATP_RUN_TEST_NS(runner, tensor, lazy_operations);
    FATP_RUN_TEST_NS(runner, tensor, expression_chaining);
    
    // v4.3 tests - Parallel operations and ContractException
    FATP_RUN_TEST_NS(runner, tensor, parallel_operations);
    FATP_RUN_TEST_NS(runner, tensor, contract_exceptions);
    FATP_RUN_TEST_NS(runner, tensor, parallel_threshold);
    
    // v5.1 tests - Enhanced Safety Features
    FATP_RUN_TEST_NS(runner, tensor, enhanced_bounds_checking);
    FATP_RUN_TEST_NS(runner, tensor, at_linear);
    FATP_RUN_TEST_NS(runner, tensor, lifetime_tracking_integration);
    FATP_RUN_TEST_NS(runner, tensor, rcu_tensor_integration);
    FATP_RUN_TEST_NS(runner, tensor, v51_safety_comprehensive);
    FATP_RUN_TEST_NS(runner, tensor, v51_zero_overhead_release);
    
    // Run benchmarks
    tensor::benchmark_iterators();
    tensor::benchmark_element_access();
    tensor::benchmark_operations();
    
    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_Tensor() ? 0 : 1;
}
#endif
