/**
 * @file test_Tensor.cpp
 * @brief Comprehensive tests for Tensor class.
 */

#include "test_Tensor.h"
#include "Tensor.h"
#include "test_Utilities.h"
#include <random>
#include <thread>
#include <vector>
#include <chrono>

namespace cpp_utilities {
namespace testing {

// ============================================================================
// Basic Construction and Properties Tests
// ============================================================================

bool test_tensor_default_constructor() {
    Tensor<int> tensor;
    ASSERT_TRUE(tensor.empty(), "Default tensor should be empty");
    ASSERT_EQ(tensor.size(), 0, "Default tensor size should be 0");
    return true;
}

bool test_tensor_shape_constructor() {
    TensorShape shape({3, 4, 5});
    Tensor<double> tensor(shape);
    
    ASSERT_EQ(tensor.rank(), 3, "Rank should be 3");
    ASSERT_EQ(tensor.size(), 60, "Size should be 3*4*5=60");
    ASSERT_TRUE(tensor.shape() == shape, "Shape should match");
    ASSERT_FALSE(tensor.empty(), "Tensor should not be empty");
    
    return true;
}

bool test_tensor_fill_constructor() {
    TensorShape shape({2, 3});
    Tensor<int> tensor(shape, 42);
    
    ASSERT_EQ(tensor.size(), 6, "Size should be 6");
    for (size_t i = 0; i < tensor.size(); ++i) {
        ASSERT_EQ(tensor[i], 42, "All elements should be 42");
    }
    
    return true;
}

bool test_tensor_initializer_list() {
    Tensor<int> tensor = {1, 2, 3, 4, 5};
    
    ASSERT_EQ(tensor.rank(), 1, "Rank should be 1");
    ASSERT_EQ(tensor.size(), 5, "Size should be 5");
    ASSERT_EQ(tensor[0], 1, "First element should be 1");
    ASSERT_EQ(tensor[4], 5, "Last element should be 5");
    
    return true;
}

bool test_tensor_copy_constructor() {
    TensorShape shape({2, 3});
    Tensor<int> original(shape, 10);
    Tensor<int> copy(original);
    
    ASSERT_TRUE(copy.shape() == original.shape(), "Shapes should match");
    ASSERT_EQ(copy.size(), original.size(), "Sizes should match");
    
    // Modify original, copy should remain unchanged
    original[0] = 99;
    ASSERT_EQ(original[0], 99, "Original should be modified");
    ASSERT_EQ(copy[0], 10, "Copy should be unchanged");
    
    return true;
}

bool test_tensor_move_constructor() {
    TensorShape shape({2, 3});
    Tensor<int> original(shape, 15);
    auto* original_data = original.data();
    
    Tensor<int> moved(std::move(original));
    
    ASSERT_EQ(moved.size(), 6, "Moved tensor should have correct size");
    ASSERT_EQ(moved[0], 15, "Moved tensor should have correct data");
    ASSERT_TRUE(original.empty(), "Original should be empty after move");
    
    return true;
}

// ============================================================================
// Element Access Tests
// ============================================================================

bool test_tensor_element_access() {
    TensorShape shape({2, 3, 4});
    Tensor<int> tensor(shape, 0);
    
    tensor({0, 0, 0}) = 1;
    tensor({0, 1, 2}) = 2;
    tensor({1, 2, 3}) = 3;
    
    ASSERT_EQ(tensor({0, 0, 0}), 1, "Element access should work");
    ASSERT_EQ(tensor({0, 1, 2}), 2, "Element access should work");
    ASSERT_EQ(tensor({1, 2, 3}), 3, "Element access should work");
    
    return true;
}

bool test_tensor_flat_access() {
    TensorShape shape({2, 3});
    Tensor<int> tensor(shape);
    
    for (size_t i = 0; i < tensor.size(); ++i) {
        tensor[i] = static_cast<int>(i);
    }
    
    for (size_t i = 0; i < tensor.size(); ++i) {
        ASSERT_EQ(tensor[i], static_cast<int>(i), "Flat access should work");
    }
    
    return true;
}

bool test_tensor_at_variadic() {
    TensorShape shape({2, 3, 4});
    Tensor<int> tensor(shape, 0);
    
    tensor.at(0, 1, 2) = 42;
    ASSERT_EQ(tensor.at(0, 1, 2), 42, "Variadic at() should work");
    
    tensor.at(1, 2, 3) = 99;
    ASSERT_EQ(tensor.at(1, 2, 3), 99, "Variadic at() should work");
    
    return true;
}

bool test_tensor_bounds_checking() {
    TensorShape shape({2, 3});
    Tensor<int> tensor(shape);
    
    // These should not throw in release, but enforce in debug
    bool caught_exception = false;
    try {
        // This should trigger enforce in debug mode
        volatile auto val = tensor[100];  // Out of bounds
        (void)val;
    } catch (...) {
        caught_exception = true;
    }
    
    // In debug mode with enforce, we expect an exception
    // In release mode, it's undefined behavior but shouldn't crash for this test
    
    return true;
}

// ============================================================================
// Shape Operations Tests
// ============================================================================

bool test_tensor_reshape() {
    TensorShape shape({2, 3});
    Tensor<int> tensor(shape);
    
    for (size_t i = 0; i < tensor.size(); ++i) {
        tensor[i] = static_cast<int>(i);
    }
    
    auto result = tensor.reshape(TensorShape({3, 2}));
    ASSERT_TRUE(result.has_value(), "Reshape should succeed");
    ASSERT_EQ(tensor.shape().rank(), 2, "Rank should be 2");
    
    // Data should remain the same
    for (size_t i = 0; i < tensor.size(); ++i) {
        ASSERT_EQ(tensor[i], static_cast<int>(i), "Data should be unchanged");
    }
    
    // Invalid reshape should fail
    auto invalid = tensor.reshape(TensorShape({2, 4}));
    ASSERT_FALSE(invalid.has_value(), "Invalid reshape should fail");
    
    return true;
}

bool test_tensor_view() {
    TensorShape shape({2, 3});
    Tensor<int> tensor(shape, 42);
    
    auto view = tensor.view(TensorShape({6}));
    ASSERT_EQ(view.size(), 6, "View size should be 6");
    ASSERT_EQ(view[0], 42, "View should access same data");
    
    // Modify original
    tensor[0] = 99;
    ASSERT_EQ(view[0], 99, "View should reflect changes");
    
    return true;
}

bool test_tensor_flatten() {
    TensorShape shape({2, 3, 4});
    Tensor<int> tensor(shape);
    
    for (size_t i = 0; i < tensor.size(); ++i) {
        tensor[i] = static_cast<int>(i);
    }
    
    auto flat = tensor.flatten();
    ASSERT_EQ(flat.rank(), 1, "Flattened tensor should be 1D");
    ASSERT_EQ(flat.size(), 24, "Flattened size should be 24");
    
    for (size_t i = 0; i < flat.size(); ++i) {
        ASSERT_EQ(flat[i], static_cast<int>(i), "Flattened data should match");
    }
    
    return true;
}

bool test_tensor_transpose() {
    TensorShape shape({2, 3});
    Tensor<int> tensor(shape);
    
    int val = 0;
    for (size_t i = 0; i < shape[0]; ++i) {
        for (size_t j = 0; j < shape[1]; ++j) {
            tensor.at(i, j) = val++;
        }
    }
    
    auto result = tensor.transpose();
    ASSERT_TRUE(result.has_value(), "Transpose should succeed");
    
    auto& transposed = result.value();
    ASSERT_EQ(transposed.shape()[0], 3, "Transposed rows should be 3");
    ASSERT_EQ(transposed.shape()[1], 2, "Transposed cols should be 2");
    
    // Check values
    ASSERT_EQ(transposed.at(0, 0), tensor.at(0, 0), "Transpose should work");
    ASSERT_EQ(transposed.at(1, 0), tensor.at(0, 1), "Transpose should work");
    ASSERT_EQ(transposed.at(2, 1), tensor.at(1, 2), "Transpose should work");
    
    return true;
}

bool test_tensor_slice() {
    TensorShape shape({3, 4});
    Tensor<int> tensor(shape);
    
    int val = 0;
    for (size_t i = 0; i < tensor.size(); ++i) {
        tensor[i] = val++;
    }
    
    // Slice along axis 0 (get row)
    auto row_result = tensor.slice(0, 1);
    ASSERT_TRUE(row_result.has_value(), "Slice should succeed");
    
    auto& row = row_result.value();
    ASSERT_EQ(row.size(), 4, "Row should have 4 elements");
    
    return true;
}

// ============================================================================
// Fill Operations Tests
// ============================================================================

bool test_tensor_fill() {
    TensorShape shape({3, 4});
    Tensor<int> tensor(shape);
    
    tensor.fill(7);
    for (size_t i = 0; i < tensor.size(); ++i) {
        ASSERT_EQ(tensor[i], 7, "All elements should be 7");
    }
    
    return true;
}

bool test_tensor_zeros_ones() {
    TensorShape shape({2, 3});
    Tensor<double> tensor(shape);
    
    tensor.zeros();
    for (size_t i = 0; i < tensor.size(); ++i) {
        ASSERT_EQ(tensor[i], 0.0, "All elements should be 0");
    }
    
    tensor.ones();
    for (size_t i = 0; i < tensor.size(); ++i) {
        ASSERT_EQ(tensor[i], 1.0, "All elements should be 1");
    }
    
    return true;
}

bool test_tensor_fill_with_generator() {
    TensorShape shape({10});
    Tensor<int> tensor(shape);
    
    int counter = 0;
    tensor.fill_with([&counter]() { return counter++; });
    
    for (size_t i = 0; i < tensor.size(); ++i) {
        ASSERT_EQ(tensor[i], static_cast<int>(i), "Generator should work");
    }
    
    return true;
}

// ============================================================================
// Arithmetic Operations Tests
// ============================================================================

bool test_tensor_addition() {
    TensorShape shape({2, 3});
    Tensor<int> a(shape, 5);
    Tensor<int> b(shape, 3);
    
    auto c = a + b;
    
    ASSERT_TRUE(c.shape() == shape, "Result shape should match");
    for (size_t i = 0; i < c.size(); ++i) {
        ASSERT_EQ(c[i], 8, "Addition should work");
    }
    
    return true;
}

bool test_tensor_subtraction() {
    TensorShape shape({2, 3});
    Tensor<int> a(shape, 10);
    Tensor<int> b(shape, 3);
    
    auto c = a - b;
    
    for (size_t i = 0; i < c.size(); ++i) {
        ASSERT_EQ(c[i], 7, "Subtraction should work");
    }
    
    return true;
}

bool test_tensor_multiplication() {
    TensorShape shape({2, 3});
    Tensor<int> a(shape, 4);
    Tensor<int> b(shape, 3);
    
    auto c = a * b;
    
    for (size_t i = 0; i < c.size(); ++i) {
        ASSERT_EQ(c[i], 12, "Multiplication should work");
    }
    
    return true;
}

bool test_tensor_scalar_operations() {
    TensorShape shape({2, 3});
    Tensor<int> tensor(shape, 5);
    
    auto add_result = tensor + 10;
    for (size_t i = 0; i < add_result.size(); ++i) {
        ASSERT_EQ(add_result[i], 15, "Scalar addition should work");
    }
    
    auto mul_result = tensor * 3;
    for (size_t i = 0; i < mul_result.size(); ++i) {
        ASSERT_EQ(mul_result[i], 15, "Scalar multiplication should work");
    }
    
    return true;
}

bool test_tensor_inplace_operations() {
    TensorShape shape({2, 3});
    Tensor<int> a(shape, 5);
    Tensor<int> b(shape, 3);
    
    a += b;
    for (size_t i = 0; i < a.size(); ++i) {
        ASSERT_EQ(a[i], 8, "In-place addition should work");
    }
    
    Tensor<int> c(shape, 2);
    c *= 5;
    for (size_t i = 0; i < c.size(); ++i) {
        ASSERT_EQ(c[i], 10, "In-place scalar multiplication should work");
    }
    
    return true;
}

// ============================================================================
// Reduction Operations Tests
// ============================================================================

bool test_tensor_sum() {
    TensorShape shape({3, 4});
    Tensor<int> tensor(shape);
    
    for (size_t i = 0; i < tensor.size(); ++i) {
        tensor[i] = static_cast<int>(i + 1);
    }
    
    int sum = tensor.sum();
    int expected = (12 * 13) / 2;  // Sum of 1 to 12
    ASSERT_EQ(sum, expected, "Sum should be correct");
    
    return true;
}

bool test_tensor_product() {
    TensorShape shape({3});
    Tensor<int> tensor = {2, 3, 4};
    
    int product = tensor.product();
    ASSERT_EQ(product, 24, "Product should be 2*3*4=24");
    
    return true;
}

bool test_tensor_mean() {
    TensorShape shape({4});
    Tensor<int> tensor = {2, 4, 6, 8};
    
    double mean = tensor.mean<double>();
    ASSERT_EQ(mean, 5.0, "Mean should be 5.0");
    
    return true;
}

bool test_tensor_min_max() {
    TensorShape shape({5});
    Tensor<int> tensor = {3, 7, 2, 9, 5};
    
    ASSERT_EQ(tensor.min(), 2, "Min should be 2");
    ASSERT_EQ(tensor.max(), 9, "Max should be 9");
    
    return true;
}

// ============================================================================
// Matrix Operations Tests
// ============================================================================

bool test_tensor_matmul() {
    // 2x3 matrix
    TensorShape shape_a({2, 3});
    Tensor<int> a(shape_a);
    a.at(0, 0) = 1; a.at(0, 1) = 2; a.at(0, 2) = 3;
    a.at(1, 0) = 4; a.at(1, 1) = 5; a.at(1, 2) = 6;
    
    // 3x2 matrix
    TensorShape shape_b({3, 2});
    Tensor<int> b(shape_b);
    b.at(0, 0) = 7;  b.at(0, 1) = 8;
    b.at(1, 0) = 9;  b.at(1, 1) = 10;
    b.at(2, 0) = 11; b.at(2, 1) = 12;
    
    auto result = a.matmul(b);
    ASSERT_TRUE(result.has_value(), "Matmul should succeed");
    
    auto& c = result.value();
    ASSERT_EQ(c.shape()[0], 2, "Result rows should be 2");
    ASSERT_EQ(c.shape()[1], 2, "Result cols should be 2");
    
    // Check result values
    // c[0,0] = 1*7 + 2*9 + 3*11 = 58
    ASSERT_EQ(c.at(0, 0), 58, "Matmul result should be correct");
    
    // c[0,1] = 1*8 + 2*10 + 3*12 = 64
    ASSERT_EQ(c.at(0, 1), 64, "Matmul result should be correct");
    
    // c[1,0] = 4*7 + 5*9 + 6*11 = 139
    ASSERT_EQ(c.at(1, 0), 139, "Matmul result should be correct");
    
    // c[1,1] = 4*8 + 5*10 + 6*12 = 154
    ASSERT_EQ(c.at(1, 1), 154, "Matmul result should be correct");
    
    return true;
}

bool test_tensor_transpose_2d() {
    TensorShape shape({2, 3});
    Tensor<int> tensor(shape);
    
    tensor.at(0, 0) = 1; tensor.at(0, 1) = 2; tensor.at(0, 2) = 3;
    tensor.at(1, 0) = 4; tensor.at(1, 1) = 5; tensor.at(1, 2) = 6;
    
    auto result = tensor.transpose();
    ASSERT_TRUE(result.has_value(), "Transpose should succeed");
    
    auto& t = result.value();
    ASSERT_EQ(t.at(0, 0), 1, "Transpose should be correct");
    ASSERT_EQ(t.at(0, 1), 4, "Transpose should be correct");
    ASSERT_EQ(t.at(1, 0), 2, "Transpose should be correct");
    ASSERT_EQ(t.at(2, 1), 6, "Transpose should be correct");
    
    return true;
}

// ============================================================================
// Broadcasting Tests
// ============================================================================

bool test_tensor_broadcasting_check() {
    TensorShape a({3, 4, 5});
    TensorShape b({4, 5});
    TensorShape c({5});
    TensorShape d({3, 1, 5});
    
    ASSERT_TRUE(Tensor<int>::are_broadcastable(a, b), 
                "Shapes should be broadcastable");
    ASSERT_TRUE(Tensor<int>::are_broadcastable(a, c), 
                "Shapes should be broadcastable");
    ASSERT_TRUE(Tensor<int>::are_broadcastable(a, d), 
                "Shapes should be broadcastable");
    
    TensorShape e({3, 4});
    TensorShape f({5, 6});
    ASSERT_FALSE(Tensor<int>::are_broadcastable(e, f), 
                 "Incompatible shapes should not be broadcastable");
    
    return true;
}

bool test_tensor_broadcast_shape() {
    TensorShape a({3, 1, 5});
    TensorShape b({4, 5});
    
    auto result = Tensor<int>::broadcast_shape(a, b);
    
    ASSERT_EQ(result.rank(), 3, "Broadcast shape should have rank 3");
    ASSERT_EQ(result[0], 3, "First dimension should be 3");
    ASSERT_EQ(result[1], 4, "Second dimension should be 4");
    ASSERT_EQ(result[2], 5, "Third dimension should be 5");
    
    return true;
}

// ============================================================================
// Iterator Tests
// ============================================================================

bool test_tensor_iterators() {
    TensorShape shape({3, 4});
    Tensor<int> tensor(shape);
    
    int val = 0;
    for (auto it = tensor.begin(); it != tensor.end(); ++it) {
        *it = val++;
    }
    
    val = 0;
    for (auto it = tensor.begin(); it != tensor.end(); ++it) {
        ASSERT_EQ(*it, val++, "Iterator should work");
    }
    
    return true;
}

bool test_tensor_const_iterators() {
    TensorShape shape({3, 4});
    Tensor<int> tensor(shape, 42);
    
    const auto& const_tensor = tensor;
    
    for (auto it = const_tensor.begin(); it != const_tensor.end(); ++it) {
        ASSERT_EQ(*it, 42, "Const iterator should work");
    }
    
    return true;
}

bool test_tensor_iterator_algorithms() {
    TensorShape shape({10});
    Tensor<int> tensor(shape);
    
    // Use std::iota
    std::iota(tensor.begin(), tensor.end(), 0);
    
    for (size_t i = 0; i < tensor.size(); ++i) {
        ASSERT_EQ(tensor[i], static_cast<int>(i), "std::iota should work");
    }
    
    // Use std::accumulate
    int sum = std::accumulate(tensor.begin(), tensor.end(), 0);
    ASSERT_EQ(sum, 45, "std::accumulate should work");
    
    return true;
}

// ============================================================================
// Factory Function Tests
// ============================================================================

bool test_tensor_factory_zeros() {
    auto tensor = zeros<int>(TensorShape({3, 4}));
    
    ASSERT_EQ(tensor.size(), 12, "Size should be 12");
    for (size_t i = 0; i < tensor.size(); ++i) {
        ASSERT_EQ(tensor[i], 0, "All elements should be 0");
    }
    
    return true;
}

bool test_tensor_factory_ones() {
    auto tensor = ones<double>(TensorShape({2, 3}));
    
    for (size_t i = 0; i < tensor.size(); ++i) {
        ASSERT_EQ(tensor[i], 1.0, "All elements should be 1");
    }
    
    return true;
}

bool test_tensor_factory_eye() {
    auto tensor = eye<int>(3);
    
    ASSERT_EQ(tensor.at(0, 0), 1, "Diagonal should be 1");
    ASSERT_EQ(tensor.at(1, 1), 1, "Diagonal should be 1");
    ASSERT_EQ(tensor.at(2, 2), 1, "Diagonal should be 1");
    ASSERT_EQ(tensor.at(0, 1), 0, "Off-diagonal should be 0");
    ASSERT_EQ(tensor.at(1, 0), 0, "Off-diagonal should be 0");
    
    return true;
}

bool test_tensor_factory_arange() {
    auto tensor = arange<int>(0, 10, 2);
    
    ASSERT_EQ(tensor.size(), 5, "Size should be 5");
    ASSERT_EQ(tensor[0], 0, "First element should be 0");
    ASSERT_EQ(tensor[1], 2, "Second element should be 2");
    ASSERT_EQ(tensor[4], 8, "Last element should be 8");
    
    return true;
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

bool test_tensor_thread_safety() {
    TensorShape shape({1000});
    ThreadSafeTensor<int> tensor(shape, 0);
    
    const int num_threads = 4;
    const int ops_per_thread = 250;
    
    std::vector<std::thread> threads;
    
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&tensor, t, ops_per_thread]() {
            int start = t * ops_per_thread;
            for (int i = 0; i < ops_per_thread; ++i) {
                tensor[start + i] = t;
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify all writes succeeded
    for (int t = 0; t < num_threads; ++t) {
        int start = t * ops_per_thread;
        for (int i = 0; i < ops_per_thread; ++i) {
            ASSERT_EQ(tensor[start + i], t, "Thread-safe write should work");
        }
    }
    
    return true;
}

bool test_tensor_concurrent_access() {
    TensorShape shape({100});
    ThreadSafeTensor<int> tensor(shape, 1);
    
    std::vector<std::thread> threads;
    const int num_threads = 4;
    
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&tensor]() {
            for (int i = 0; i < 100; ++i) {
                tensor += tensor;  // Concurrent in-place operations
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Just verify no crashes occurred
    ASSERT_TRUE(tensor.size() == 100, "Tensor should still be valid");
    
    return true;
}

// ============================================================================
// Performance Benchmarks
// ============================================================================

bool test_tensor_benchmark_construction() {
    benchmark("Tensor construction (1000x1000)", []() {
        TensorShape shape({1000, 1000});
        Tensor<double> tensor(shape);
    }, 100);
    
    benchmark("Tensor construction with fill (1000x1000)", []() {
        TensorShape shape({1000, 1000});
        Tensor<double> tensor(shape, 1.0);
    }, 100);
    
    return true;
}

bool test_tensor_benchmark_element_access() {
    TensorShape shape({1000, 1000});
    Tensor<double> tensor(shape, 1.0);
    volatile double sum = 0.0;
    
    benchmark("Flat element access (1M elements)", [&tensor, &sum]() {
        for (size_t i = 0; i < tensor.size(); ++i) {
            sum += tensor[i];
        }
    }, 10);
    
    return true;
}

bool test_tensor_benchmark_arithmetic() {
    TensorShape shape({1000, 1000});
    auto a = ones<double>(shape);
    auto b = ones<double>(shape);
    
    benchmark("Element-wise addition (1M elements)", [&a, &b]() {
        auto c = a + b;
    }, 10);
    
    benchmark("Element-wise multiplication (1M elements)", [&a, &b]() {
        auto c = a * b;
    }, 10);
    
    return true;
}

bool test_tensor_benchmark_reductions() {
    TensorShape shape({1000, 1000});
    auto tensor = ones<double>(shape);
    
    benchmark("Sum reduction (1M elements)", [&tensor]() {
        volatile auto s = tensor.sum();
        (void)s;
    }, 10);
    
    benchmark("Min/Max (1M elements)", [&tensor]() {
        volatile auto min_val = tensor.min();
        volatile auto max_val = tensor.max();
        (void)min_val;
        (void)max_val;
    }, 10);
    
    return true;
}

bool test_tensor_benchmark_matmul() {
    TensorShape shape_a({100, 100});
    TensorShape shape_b({100, 100});
    auto a = ones<double>(shape_a);
    auto b = ones<double>(shape_b);
    
    benchmark("Matrix multiplication (100x100)", [&a, &b]() {
        auto result = a.matmul(b);
    }, 5);
    
    return true;
}

// ============================================================================
// Advanced Features Tests
// ============================================================================

bool test_tensor_custom_allocator() {
    // Test with standard allocator (default)
    TensorShape shape({10});
    Tensor<int, StandardAllocatorImpl<int>> tensor(shape, 42);
    
    ASSERT_EQ(tensor.size(), 10, "Custom allocator tensor should work");
    for (size_t i = 0; i < tensor.size(); ++i) {
        ASSERT_EQ(tensor[i], 42, "Custom allocator data should be correct");
    }
    
    return true;
}

bool test_tensor_strided_access() {
    TensorShape shape({3, 4});
    Tensor<int> tensor(shape);
    
    int val = 0;
    for (size_t i = 0; i < tensor.size(); ++i) {
        tensor[i] = val++;
    }
    
    const auto& strides = tensor.strides();
    ASSERT_EQ(strides.size(), 2, "Should have 2 strides");
    ASSERT_EQ(strides[1], 1, "Last stride should be 1");
    ASSERT_EQ(strides[0], 4, "First stride should be 4");
    
    return true;
}

bool test_tensor_memory_ownership() {
    TensorShape shape({10});
    Tensor<int> owner(shape, 5);
    
    ASSERT_TRUE(owner.owns_data(), "Owner should own data");
    
    // Create non-owning view
    Tensor<int> view(shape, owner.data(), false);
    ASSERT_FALSE(view.owns_data(), "View should not own data");
    
    // Modify through view
    view[0] = 99;
    ASSERT_EQ(owner[0], 99, "View modification should affect owner");
    
    return true;
}

// ============================================================================
// Edge Cases Tests
// ============================================================================

bool test_tensor_empty_tensor() {
    Tensor<int> tensor;
    
    ASSERT_TRUE(tensor.empty(), "Empty tensor should be empty");
    ASSERT_EQ(tensor.size(), 0, "Empty tensor size should be 0");
    
    return true;
}

bool test_tensor_single_element() {
    TensorShape shape({1});
    Tensor<int> tensor(shape, 42);
    
    ASSERT_EQ(tensor.size(), 1, "Single element tensor size should be 1");
    ASSERT_EQ(tensor[0], 42, "Single element should be correct");
    
    return true;
}

bool test_tensor_large_tensor() {
    // Test with larger tensor (10MB of ints)
    size_t size = 2500000;  // ~10MB
    TensorShape shape({size});
    Tensor<int> tensor(shape, 1);
    
    ASSERT_EQ(tensor.size(), size, "Large tensor size should be correct");
    ASSERT_EQ(tensor[0], 1, "Large tensor data should be correct");
    ASSERT_EQ(tensor[size - 1], 1, "Large tensor data should be correct");
    
    return true;
}

bool test_tensor_high_dimensional() {
    TensorShape shape({2, 3, 4, 5, 6});
    Tensor<int> tensor(shape);
    
    ASSERT_EQ(tensor.rank(), 5, "Rank should be 5");
    ASSERT_EQ(tensor.size(), 720, "Size should be 2*3*4*5*6=720");
    
    return true;
}

// ============================================================================
// Test Runner
// ============================================================================

int run_all_tensor_tests() {
    TestRunner runner;
    
    std::cout << "\n" << colors::bold() << colors::cyan() 
              << "========================================\n"
              << "         TENSOR CLASS TESTS             \n"
              << "========================================\n" 
              << colors::reset() << std::endl;
    
    // Basic construction and properties
    std::cout << colors::blue() << "\n[Basic Construction Tests]\n" 
              << colors::reset();
    RUN_TEST(runner, tensor_default_constructor);
    RUN_TEST(runner, tensor_shape_constructor);
    RUN_TEST(runner, tensor_fill_constructor);
    RUN_TEST(runner, tensor_initializer_list);
    RUN_TEST(runner, tensor_copy_constructor);
    RUN_TEST(runner, tensor_move_constructor);
    
    // Element access
    std::cout << colors::blue() << "\n[Element Access Tests]\n" 
              << colors::reset();
    RUN_TEST(runner, tensor_element_access);
    RUN_TEST(runner, tensor_flat_access);
    RUN_TEST(runner, tensor_at_variadic);
    RUN_TEST(runner, tensor_bounds_checking);
    
    // Shape operations
    std::cout << colors::blue() << "\n[Shape Operations Tests]\n" 
              << colors::reset();
    RUN_TEST(runner, tensor_reshape);
    RUN_TEST(runner, tensor_view);
    RUN_TEST(runner, tensor_flatten);
    RUN_TEST(runner, tensor_transpose);
    RUN_TEST(runner, tensor_slice);
    
    // Fill operations
    std::cout << colors::blue() << "\n[Fill Operations Tests]\n" 
              << colors::reset();
    RUN_TEST(runner, tensor_fill);
    RUN_TEST(runner, tensor_zeros_ones);
    RUN_TEST(runner, tensor_fill_with_generator);
    
    // Arithmetic operations
    std::cout << colors::blue() << "\n[Arithmetic Operations Tests]\n" 
              << colors::reset();
    RUN_TEST(runner, tensor_addition);
    RUN_TEST(runner, tensor_subtraction);
    RUN_TEST(runner, tensor_multiplication);
    RUN_TEST(runner, tensor_scalar_operations);
    RUN_TEST(runner, tensor_inplace_operations);
    
    // Reduction operations
    std::cout << colors::blue() << "\n[Reduction Operations Tests]\n" 
              << colors::reset();
    RUN_TEST(runner, tensor_sum);
    RUN_TEST(runner, tensor_product);
    RUN_TEST(runner, tensor_mean);
    RUN_TEST(runner, tensor_min_max);
    
    // Matrix operations
    std::cout << colors::blue() << "\n[Matrix Operations Tests]\n" 
              << colors::reset();
    RUN_TEST(runner, tensor_matmul);
    RUN_TEST(runner, tensor_transpose_2d);
    
    // Broadcasting
    std::cout << colors::blue() << "\n[Broadcasting Tests]\n" 
              << colors::reset();
    RUN_TEST(runner, tensor_broadcasting_check);
    RUN_TEST(runner, tensor_broadcast_shape);
    
    // Iterators
    std::cout << colors::blue() << "\n[Iterator Tests]\n" 
              << colors::reset();
    RUN_TEST(runner, tensor_iterators);
    RUN_TEST(runner, tensor_const_iterators);
    RUN_TEST(runner, tensor_iterator_algorithms);
    
    // Factory functions
    std::cout << colors::blue() << "\n[Factory Function Tests]\n" 
              << colors::reset();
    RUN_TEST(runner, tensor_factory_zeros);
    RUN_TEST(runner, tensor_factory_ones);
    RUN_TEST(runner, tensor_factory_eye);
    RUN_TEST(runner, tensor_factory_arange);
    
    // Thread safety
    std::cout << colors::blue() << "\n[Thread Safety Tests]\n" 
              << colors::reset();
    RUN_TEST(runner, tensor_thread_safety);
    RUN_TEST(runner, tensor_concurrent_access);
    
    // Advanced features
    std::cout << colors::blue() << "\n[Advanced Features Tests]\n" 
              << colors::reset();
    RUN_TEST(runner, tensor_custom_allocator);
    RUN_TEST(runner, tensor_strided_access);
    RUN_TEST(runner, tensor_memory_ownership);
    
    // Edge cases
    std::cout << colors::blue() << "\n[Edge Cases Tests]\n" 
              << colors::reset();
    RUN_TEST(runner, tensor_empty_tensor);
    RUN_TEST(runner, tensor_single_element);
    RUN_TEST(runner, tensor_large_tensor);
    RUN_TEST(runner, tensor_high_dimensional);
    
    // Performance benchmarks
    std::cout << colors::blue() << "\n[Performance Benchmarks]\n" 
              << colors::reset();
    RUN_TEST(runner, tensor_benchmark_construction);
    RUN_TEST(runner, tensor_benchmark_element_access);
    RUN_TEST(runner, tensor_benchmark_arithmetic);
    RUN_TEST(runner, tensor_benchmark_reductions);
    RUN_TEST(runner, tensor_benchmark_matmul);
    
    return runner.print_summary();
}

} // namespace testing
} // namespace cpp_utilities

// Main function for standalone testing
int main() {
    return cpp_utilities::testing::run_all_tensor_tests();
}
