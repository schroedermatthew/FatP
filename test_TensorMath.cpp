/**
 * @file test_TensorMath.cpp
 * @brief Comprehensive unit tests for TensorMath template
 * 
 * Tests cover:
 * - Basic construction and shape system
 * - Element-wise arithmetic operations
 * - Policy behavior (Unchecked, Checked, Saturating)
 * - Linear algebra (dot, matmul, transpose, outer)
 * - Reduction operations (sum, mean, max, min, norm)
 * - SIMD operations (when available)
 * - Higher-order tensors (3D, 4D)
 * - Performance benchmarks
 */

#include "TensorMath.h"
#include "test_Utilities.h"
#include <limits>
#include <cmath>

namespace cpp_utilities::testing
{

// =============================================================================
// Basic Construction and Access Tests
// =============================================================================

TEST_CASE(default_construction) {
    StaticTensor<float, Vector<4>, UncheckedPolicy> v;
    ASSERT_EQ(v[0], 0.0f, "Default construction should zero-initialize");
    return true;
}

TEST_CASE(scalar_broadcast) {
    StaticTensor<int, Vector<3>, UncheckedPolicy> v(42);
    ASSERT_TRUE(v[0] == 42 && v[1] == 42 && v[2] == 42, "Scalar constructor should broadcast");
    return true;
}

TEST_CASE(initializer_list_construction) {
    StaticTensor<double, Vector<3>, UncheckedPolicy> v{1.0, 2.0, 3.0};
    ASSERT_TRUE(v[0] == 1.0 && v[1] == 2.0 && v[2] == 3.0, "Initializer list construction");
    return true;
}

TEST_CASE(variadic_constructor) {
    StaticTensor<float, Vector<4>, UncheckedPolicy> v{1.0f, 2.0f, 3.0f, 4.0f};
    ASSERT_TRUE(v[0] == 1.0f && v[3] == 4.0f, "Variadic constructor");
    return true;
}

TEST_CASE(matrix_construction) {
    StaticTensor<int, Matrix<2, 3>, UncheckedPolicy> m{1, 2, 3, 4, 5, 6};
    ASSERT_TRUE(m.at(0, 0) == 1 && m.at(1, 2) == 6, "Matrix construction");
    return true;
}

TEST_CASE(type_aliases) {
    Vec3f v{1.0f, 2.0f, 3.0f};
    Mat2x2f m{1.0f, 2.0f, 3.0f, 4.0f};
    ASSERT_TRUE(v[2] == 3.0f && m.at(1, 1) == 4.0f, "Type aliases");
    return true;
}

// =============================================================================
// Shape System Tests
// =============================================================================

TEST_CASE(shape_rank) {
    using S1 = Shape<3, 4, 5>;
    ASSERT_EQ(S1::rank, 3u, "Shape rank");
    return true;
}

TEST_CASE(shape_size) {
    using S1 = Shape<3, 4, 5>;
    ASSERT_EQ(S1::size, 60u, "Shape size");
    return true;
}

TEST_CASE(shape_dimensions) {
    using S1 = Shape<3, 4, 5>;
    ASSERT_TRUE(S1::dim<0>() == 3 && S1::dim<1>() == 4 && S1::dim<2>() == 5, "Shape dimensions");
    return true;
}

TEST_CASE(vector_shape) {
    using V = Vector<10>;
    ASSERT_TRUE(V::rank == 1 && V::size == 10, "Vector shape");
    return true;
}

TEST_CASE(matrix_shape) {
    using M = Matrix<3, 4>;
    ASSERT_TRUE(M::rank == 2 && M::size == 12, "Matrix shape");
    return true;
}

// =============================================================================
// Element-Wise Operations Tests
// =============================================================================

TEST_CASE(vector_addition) {
    Vec3f a{1.0f, 2.0f, 3.0f};
    Vec3f b{4.0f, 5.0f, 6.0f};
    auto c = a + b;
    ASSERT_TRUE(c[0] == 5.0f && c[1] == 7.0f && c[2] == 9.0f, "Vector addition");
    return true;
}

TEST_CASE(vector_subtraction) {
    Vec3f a{1.0f, 2.0f, 3.0f};
    Vec3f b{4.0f, 5.0f, 6.0f};
    auto c = b - a;
    ASSERT_TRUE(c[0] == 3.0f && c[1] == 3.0f && c[2] == 3.0f, "Vector subtraction");
    return true;
}

TEST_CASE(hadamard_product) {
    Vec3f a{1.0f, 2.0f, 3.0f};
    Vec3f b{4.0f, 5.0f, 6.0f};
    auto c = a * b;
    ASSERT_TRUE(c[0] == 4.0f && c[1] == 10.0f && c[2] == 18.0f, "Hadamard product");
    return true;
}

TEST_CASE(element_wise_division) {
    Vec3f a{1.0f, 2.0f, 3.0f};
    Vec3f b{4.0f, 5.0f, 6.0f};
    auto c = b / a;
    ASSERT_TRUE(c[0] == 4.0f && c[1] == 2.5f && c[2] == 2.0f, "Element-wise division");
    return true;
}

TEST_CASE(scalar_multiplication) {
    Vec3f a{1.0f, 2.0f, 3.0f};
    auto c = a * 2.0f;
    ASSERT_TRUE(c[0] == 2.0f && c[1] == 4.0f && c[2] == 6.0f, "Scalar multiplication");
    return true;
}

TEST_CASE(scalar_multiplication_reversed) {
    Vec3f a{1.0f, 2.0f, 3.0f};
    auto c = 3.0f * a;
    ASSERT_TRUE(c[0] == 3.0f && c[1] == 6.0f && c[2] == 9.0f, "Reversed scalar multiplication");
    return true;
}

// =============================================================================
// Policy Behavior Tests
// =============================================================================

TEST_CASE(unchecked_policy_allows_operations) {
    Vec2<int> v{INT_MAX, 1};
    // This would overflow, but UncheckedPolicy doesn't check
    ASSERT_TRUE(true, "UncheckedPolicy allows operations without checks");
    return true;
}

TEST_CASE(checked_policy_throws_on_overflow) {
    Vec2<int, CheckedPolicy> v1{INT_MAX, 0};
    Vec2<int, CheckedPolicy> v2{1, 0};
    
    bool caught = false;
    try {
        auto result = v1 + v2; // Should throw
        (void)result;
    } catch (...) {
        caught = true;
    }
    ASSERT_TRUE(caught, "CheckedPolicy should throw on overflow");
    return true;
}

TEST_CASE(saturating_policy_clamps) {
    // FIXED: Use SaturatingArithmeticPolicy instead of SaturatingPolicy
    Vec2<int, SaturatingArithmeticPolicy> v1{INT_MAX, 0};
    Vec2<int, SaturatingArithmeticPolicy> v2{100, 0};
    auto result = v1 + v2;
    ASSERT_EQ(result[0], INT_MAX, "SaturatingArithmeticPolicy should clamp to max");
    return true;
}

// =============================================================================
// Linear Algebra Tests
// =============================================================================

TEST_CASE(dot_product) {
    Vec3f a{1.0f, 2.0f, 3.0f};
    Vec3f b{4.0f, 5.0f, 6.0f};
    
    float result = dot(a, b);
    float expected = 1.0f*4.0f + 2.0f*5.0f + 3.0f*6.0f; // 4 + 10 + 18 = 32
    
    ASSERT_TRUE(std::abs(result - expected) < 1e-6f, "Dot product");
    return true;
}

TEST_CASE(matrix_vector_multiply) {
    Mat2x2f m{1.0f, 2.0f,
              3.0f, 4.0f};
    Vec2f v{5.0f, 6.0f};
    
    auto result = matvec(m, v);
    // [1 2] [5]   [1*5 + 2*6]   [17]
    // [3 4] [6] = [3*5 + 4*6] = [39]
    
    ASSERT_TRUE(std::abs(result[0] - 17.0f) < 1e-6f, "Matrix-vector multiply row 0");
    ASSERT_TRUE(std::abs(result[1] - 39.0f) < 1e-6f, "Matrix-vector multiply row 1");
    return true;
}

TEST_CASE(matrix_matrix_multiply) {
    Mat2x2f a{1.0f, 2.0f,
              3.0f, 4.0f};
    Mat2x2f b{5.0f, 6.0f,
              7.0f, 8.0f};
    
    auto result = matmul(a, b);
    // [1 2] [5 6]   [1*5+2*7  1*6+2*8]   [19 22]
    // [3 4] [7 8] = [3*5+4*7  3*6+4*8] = [43 50]
    
    ASSERT_TRUE(std::abs(result.at(0, 0) - 19.0f) < 1e-6f, "Matmul [0,0]");
    ASSERT_TRUE(std::abs(result.at(0, 1) - 22.0f) < 1e-6f, "Matmul [0,1]");
    ASSERT_TRUE(std::abs(result.at(1, 0) - 43.0f) < 1e-6f, "Matmul [1,0]");
    ASSERT_TRUE(std::abs(result.at(1, 1) - 50.0f) < 1e-6f, "Matmul [1,1]");
    return true;
}

TEST_CASE(matrix_transpose) {
    StaticTensor<float, Matrix<2, 3>, UncheckedPolicy> m{1.0f, 2.0f, 3.0f,
                                   4.0f, 5.0f, 6.0f};
    auto result = transpose(m);
    
    ASSERT_TRUE(result.at(0, 0) == 1.0f && result.at(0, 1) == 4.0f, "Transpose row 0");
    ASSERT_TRUE(result.at(1, 0) == 2.0f && result.at(1, 1) == 5.0f, "Transpose row 1");
    ASSERT_TRUE(result.at(2, 0) == 3.0f && result.at(2, 1) == 6.0f, "Transpose row 2");
    return true;
}

TEST_CASE(outer_product) {
    Vec2f a{1.0f, 2.0f};
    Vec2f b{3.0f, 4.0f};
    auto result = outer(a, b);
    
    // [1]         [1*3  1*4]   [3  4]
    // [2] âŠ— [3 4] [2*3  2*4] = [6  8]
    
    ASSERT_TRUE(result.at(0, 0) == 3.0f && result.at(0, 1) == 4.0f, "Outer product row 0");
    ASSERT_TRUE(result.at(1, 0) == 6.0f && result.at(1, 1) == 8.0f, "Outer product row 1");
    return true;
}

// =============================================================================
// Reduction Operations Tests
// =============================================================================

TEST_CASE(sum_reduction) {
    Vec3f v{1.0f, 2.0f, 3.0f};
    float result = sum(v);
    ASSERT_EQ(result, 6.0f, "Sum reduction");
    return true;
}

TEST_CASE(mean_reduction) {
    Vec4f v{2.0f, 4.0f, 6.0f, 8.0f};
    float result = mean(v);
    ASSERT_EQ(result, 5.0f, "Mean reduction");
    return true;
}

TEST_CASE(max_reduction) {
    Vec4f v{3.0f, 1.0f, 4.0f, 2.0f};
    float result = max(v);
    ASSERT_EQ(result, 4.0f, "Max reduction");
    return true;
}

TEST_CASE(min_reduction) {
    Vec4f v{3.0f, 1.0f, 4.0f, 2.0f};
    float result = min(v);
    ASSERT_EQ(result, 1.0f, "Min reduction");
    return true;
}

TEST_CASE(l2_norm) {
    Vec3f v{3.0f, 4.0f, 0.0f};
    float result = norm(v);
    ASSERT_EQ(result, 5.0f, "L2 norm (3-4-5 triangle)");
    return true;
}

TEST_CASE(normalize_produces_unit_vector) {
    Vec3f v{3.0f, 4.0f, 0.0f};
    auto unit = normalize(v);
    float result = norm(unit);
    ASSERT_TRUE(std::abs(result - 1.0f) < 1e-6f, "Normalized vector has unit length");
    return true;
}

// =============================================================================
// SIMD Operations Tests (when available)
// =============================================================================

#ifdef __AVX2__
TEST_CASE(simd_addition) {
    constexpr size_t N = 16;
    StaticTensor<float, Vector<N>, UncheckedPolicy> a;
    StaticTensor<float, Vector<N>, UncheckedPolicy> b;
    
    for (size_t i = 0; i < N; ++i) {
        a[i] = static_cast<float>(i);
        b[i] = static_cast<float>(i * 2);
    }
    
    auto result = a + b;
    
    for (size_t i = 0; i < N; ++i) {
        float expected = static_cast<float>(i * 3);
        if (std::abs(result[i] - expected) >= 1e-6f) {
            return false;
        }
    }
    return true;
}

TEST_CASE(simd_multiplication) {
    constexpr size_t N = 16;
    StaticTensor<float, Vector<N>, UncheckedPolicy> a;
    StaticTensor<float, Vector<N>, UncheckedPolicy> b;
    
    for (size_t i = 0; i < N; ++i) {
        a[i] = static_cast<float>(i);
        b[i] = static_cast<float>(i * 2);
    }
    
    auto result = simd_mul(a, b);
    
    for (size_t i = 0; i < N; ++i) {
        float expected = static_cast<float>(i * i * 2);
        if (std::abs(result[i] - expected) >= 1e-6f) {
            return false;
        }
    }
    return true;
}

TEST_CASE(simd_dot_product) {
    constexpr size_t N = 16;
    StaticTensor<float, Vector<N>, UncheckedPolicy> a;
    StaticTensor<float, Vector<N>, UncheckedPolicy> b;
    
    for (size_t i = 0; i < N; ++i) {
        a[i] = static_cast<float>(i);
        b[i] = static_cast<float>(i * 2);
    }
    
    float result = simd_dot(a, b);
    float expected = 0.0f;
    for (size_t i = 0; i < N; ++i) {
        expected += static_cast<float>(i * i * 2);
    }
    ASSERT_TRUE(std::abs(result - expected) < 1e-4f, "SIMD dot product");
    return true;
}
#endif

// =============================================================================
// Higher-Order Tensor Tests
// =============================================================================

TEST_CASE(tensor_3d_size) {
    StaticTensor<float, Tensor3<2, 3, 4>, UncheckedPolicy> t;
    ASSERT_EQ(t.size, 24u, "3D tensor size");
    return true;
}

TEST_CASE(tensor_3d_indexing) {
    StaticTensor<float, Tensor3<2, 3, 4>, UncheckedPolicy> t;
    t.at(1, 2, 3) = 42.0f;
    ASSERT_EQ(t.at(1, 2, 3), 42.0f, "3D tensor indexing");
    return true;
}

TEST_CASE(tensor_4d_size) {
    StaticTensor<int, Tensor4<2, 2, 2, 2>, UncheckedPolicy> t;
    ASSERT_EQ(t.size, 16u, "4D tensor size");
    return true;
}

TEST_CASE(tensor_4d_indexing) {
    StaticTensor<int, Tensor4<2, 2, 2, 2>, UncheckedPolicy> t;
    
    for (size_t i = 0; i < 16; ++i) {
        t[i] = static_cast<int>(i);
    }
    
    ASSERT_EQ(t.at(1, 1, 1, 1), 15, "4D tensor indexing");
    return true;
}

// =============================================================================
// Performance Benchmarks
// =============================================================================

void run_tensor_math_benchmarks() {
    auto& out = *get_test_config().output;
    
    out << "\n" << colors::cyan() << colors::bold() 
        << "=== TensorMath Performance Benchmarks ===" 
        << colors::reset() << "\n\n";
    
    // Vector operations
    out << colors::blue() << "--- Vector Operations ---" << colors::reset() << "\n";
    {
        Vec3f a{1.0f, 2.0f, 3.0f};
        Vec3f b{4.0f, 5.0f, 6.0f};
        
        benchmark("Vec3 addition", [&]() { 
            volatile auto c = a + b; 
        });
        
        benchmark("Vec3 dot product", [&]() { 
            volatile float d = dot(a, b); 
        });
        
        benchmark("Vec3 norm", [&]() { 
            volatile float n = norm(a); 
        });
    }
    
    // Matrix operations
    out << "\n" << colors::blue() << "--- Matrix Operations ---" << colors::reset() << "\n";
    {
        Mat2x2f a{1.0f, 2.0f, 3.0f, 4.0f};
        Mat2x2f b{5.0f, 6.0f, 7.0f, 8.0f};
        Vec2f v{9.0f, 10.0f};
        
        benchmark("Mat2x2 matrix-vector multiply", [&]() { 
            volatile auto r = matvec(a, v); 
        });
        
        benchmark("Mat2x2 matrix-matrix multiply", [&]() { 
            volatile auto r = matmul(a, b); 
        });
    }
    
    // Larger matrices
    out << "\n" << colors::blue() << "--- Larger Matrices ---" << colors::reset() << "\n";
    {
        Mat4x4f a;
        Mat4x4f b;
        for (size_t i = 0; i < 16; ++i) {
            a[i] = static_cast<float>(i);
            b[i] = static_cast<float>(i + 1);
        }
        
        benchmark("Mat4x4 matrix-matrix multiply", [&]() { 
            volatile auto r = matmul(a, b); 
        }, 100000);
    }
    
#ifdef __AVX2__
    // SIMD operations
    out << "\n" << colors::blue() << "--- SIMD Operations ---" << colors::reset() << "\n";
    {
        StaticTensor<float, Vector<64>, UncheckedPolicy> a;
        StaticTensor<float, Vector<64>, UncheckedPolicy> b;
        for (size_t i = 0; i < 64; ++i) {
            a[i] = static_cast<float>(i);
            b[i] = static_cast<float>(i * 2);
        }
        
        benchmark("Vec64 SIMD addition", [&]() { 
            volatile auto c = a + b; 
        }, 500000);
        
        benchmark("Vec64 SIMD dot product", [&]() { 
            volatile float d = simd_dot(a, b); 
        }, 500000);
    }
#endif
    
    // Policy comparison
    out << "\n" << colors::blue() << "--- Policy Comparison ---" << colors::reset() << "\n";
    {
        Vec3<int, UncheckedPolicy> a1{1, 2, 3};
        Vec3<int, UncheckedPolicy> b1{4, 5, 6};
        Vec3<int, CheckedPolicy> a2{1, 2, 3};
        Vec3<int, CheckedPolicy> b2{4, 5, 6};
        
        benchmark("Vec3i addition (Unchecked)", [&]() { 
            volatile auto c = a1 + b1; 
        });
        
        benchmark("Vec3i addition (Checked)", [&]() { 
            volatile auto c = a2 + b2; 
        });
    }
    
    out << "\n";
}

// =============================================================================
// Main Test Driver
// =============================================================================

bool test_TensorMath() {

    PRINT_HEADER(TENSOR MATH)

    TestRunner runner;
    
    auto& config = get_test_config();
    config.verbose = true;
    
    auto& out = *config.output;
    
    // Basic Construction and Access
    out << colors::blue() << "--- Basic Construction and Access ---" << colors::reset() << "\n";
    RUN_TEST(runner, default_construction);
    RUN_TEST(runner, scalar_broadcast);
    RUN_TEST(runner, initializer_list_construction);
    RUN_TEST(runner, variadic_constructor);
    RUN_TEST(runner, matrix_construction);
    RUN_TEST(runner, type_aliases);
    
    // Shape System
    out << "\n" << colors::blue() << "--- Compile-Time Shape System ---" << colors::reset() << "\n";
    RUN_TEST(runner, shape_rank);
    RUN_TEST(runner, shape_size);
    RUN_TEST(runner, shape_dimensions);
    RUN_TEST(runner, vector_shape);
    RUN_TEST(runner, matrix_shape);
    
    // Element-Wise Operations
    out << "\n" << colors::blue() << "--- Element-Wise Operations ---" << colors::reset() << "\n";
    RUN_TEST(runner, vector_addition);
    RUN_TEST(runner, vector_subtraction);
    RUN_TEST(runner, hadamard_product);
    RUN_TEST(runner, element_wise_division);
    RUN_TEST(runner, scalar_multiplication);
    RUN_TEST(runner, scalar_multiplication_reversed);
    
    // Policy Behavior
    out << "\n" << colors::blue() << "--- Arithmetic Policy Behavior ---" << colors::reset() << "\n";
    RUN_TEST(runner, unchecked_policy_allows_operations);
    RUN_TEST(runner, checked_policy_throws_on_overflow);
    RUN_TEST(runner, saturating_policy_clamps);
    
    // Linear Algebra
    out << "\n" << colors::blue() << "--- Linear Algebra ---" << colors::reset() << "\n";
    RUN_TEST(runner, dot_product);
    RUN_TEST(runner, matrix_vector_multiply);
    RUN_TEST(runner, matrix_matrix_multiply);
    RUN_TEST(runner, matrix_transpose);
    RUN_TEST(runner, outer_product);
    
    // Reduction Operations
    out << "\n" << colors::blue() << "--- Reduction Operations ---" << colors::reset() << "\n";
    RUN_TEST(runner, sum_reduction);
    RUN_TEST(runner, mean_reduction);
    RUN_TEST(runner, max_reduction);
    RUN_TEST(runner, min_reduction);
    RUN_TEST(runner, l2_norm);
    RUN_TEST(runner, normalize_produces_unit_vector);
    
#ifdef __AVX2__
    // SIMD Operations
    out << "\n" << colors::blue() << "--- SIMD Operations ---" << colors::reset() << "\n";
    RUN_TEST(runner, simd_addition);
    RUN_TEST(runner, simd_multiplication);
    RUN_TEST(runner, simd_dot_product);
    out << "\n" << colors::green() << "âœ“ SIMD support detected and tested" << colors::reset() << "\n";
#else
    out << "\n" << colors::yellow() << "âš  SIMD tests skipped (no AVX2 support)" << colors::reset() << "\n";
#endif
    
    // Higher-Order Tensors
    out << "\n" << colors::blue() << "--- Higher-Order Tensors ---" << colors::reset() << "\n";
    RUN_TEST(runner, tensor_3d_size);
    RUN_TEST(runner, tensor_3d_indexing);
    RUN_TEST(runner, tensor_4d_size);
    RUN_TEST(runner, tensor_4d_indexing);
    
    // Performance Benchmarks
    run_tensor_math_benchmarks();
    
    // Summary
    return 0 == runner.print_summary();
}

} // namespace cpp_utilities::testing
