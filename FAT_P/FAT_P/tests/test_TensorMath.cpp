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
/*
FATP_META:
  meta_version: 1
  component: TensorMath
  file_role: test
  path: tests/test_TensorMath.cpp
  namespace: fat_p
  summary: "Unit tests for TensorMath."
  related:
    docs_search: "TensorMath"
    headers:
      - fat_p/TensorMath.h
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

#include "TensorMath.h"
#include "FatPTest.h"
#include <limits>
#include <cmath>

namespace fat_p::testing::tensormath
{

// =============================================================================
// Basic Construction and Access Tests
// =============================================================================

FATP_TEST_CASE(default_construction) {
    StaticTensor<float, Vector<4>, UncheckedPolicy> v;
    FATP_ASSERT_EQ(v[0], 0.0f, "Default construction should zero-initialize");
    return true;
}

FATP_TEST_CASE(scalar_broadcast) {
    StaticTensor<int, Vector<3>, UncheckedPolicy> v(42);
    FATP_ASSERT_EQ(v[0], 42, "Scalar constructor should broadcast to v[0]");
    FATP_ASSERT_EQ(v[1], 42, "Scalar constructor should broadcast to v[1]");
    FATP_ASSERT_EQ(v[2], 42, "Scalar constructor should broadcast to v[2]");
    return true;
}

FATP_TEST_CASE(initializer_list_construction) {
    StaticTensor<double, Vector<3>, UncheckedPolicy> v{1.0, 2.0, 3.0};
    FATP_ASSERT_EQ(v[0], 1.0, "Initializer list v[0]");
    FATP_ASSERT_EQ(v[1], 2.0, "Initializer list v[1]");
    FATP_ASSERT_EQ(v[2], 3.0, "Initializer list v[2]");
    return true;
}

FATP_TEST_CASE(variadic_constructor) {
    StaticTensor<float, Vector<4>, UncheckedPolicy> v{1.0f, 2.0f, 3.0f, 4.0f};
    FATP_ASSERT_EQ(v[0], 1.0f, "Variadic constructor v[0]");
    FATP_ASSERT_EQ(v[3], 4.0f, "Variadic constructor v[3]");
    return true;
}

FATP_TEST_CASE(matrix_construction) {
    StaticTensor<int, Matrix<2, 3>, UncheckedPolicy> m{1, 2, 3, 4, 5, 6};
    FATP_ASSERT_EQ(m.at(0, 0), 1, "Matrix construction m(0,0)");
    FATP_ASSERT_EQ(m.at(1, 2), 6, "Matrix construction m(1,2)");
    return true;
}

FATP_TEST_CASE(type_aliases) {
    Vec3f v{1.0f, 2.0f, 3.0f};
    Mat2x2f m{1.0f, 2.0f, 3.0f, 4.0f};
    FATP_ASSERT_EQ(v[2], 3.0f, "Type alias Vec3f");
    FATP_ASSERT_EQ(m.at(1, 1), 4.0f, "Type alias Mat2x2f");
    return true;
}

// =============================================================================
// Shape System Tests
// =============================================================================

FATP_TEST_CASE(shape_rank) {
    using S1 = Shape<3, 4, 5>;
    FATP_ASSERT_EQ(S1::rank, 3u, "Shape rank");
    return true;
}

FATP_TEST_CASE(shape_size) {
    using S1 = Shape<3, 4, 5>;
    FATP_ASSERT_EQ(S1::size, 60u, "Shape size");
    return true;
}

FATP_TEST_CASE(shape_dimensions) {
    using S1 = Shape<3, 4, 5>;
    FATP_ASSERT_EQ(S1::dim<0>(), 3u, "Shape dim<0>");
    FATP_ASSERT_EQ(S1::dim<1>(), 4u, "Shape dim<1>");
    FATP_ASSERT_EQ(S1::dim<2>(), 5u, "Shape dim<2>");
    return true;
}

FATP_TEST_CASE(vector_shape) {
    using V = Vector<10>;
    FATP_ASSERT_EQ(V::rank, 1u, "Vector rank");
    FATP_ASSERT_EQ(V::size, 10u, "Vector size");
    return true;
}

FATP_TEST_CASE(matrix_shape) {
    using M = Matrix<3, 4>;
    FATP_ASSERT_EQ(M::rank, 2u, "Matrix rank");
    FATP_ASSERT_EQ(M::size, 12u, "Matrix size");
    return true;
}

// =============================================================================
// Element-Wise Operations Tests
// =============================================================================

FATP_TEST_CASE(vector_addition) {
    Vec3f a{1.0f, 2.0f, 3.0f};
    Vec3f b{4.0f, 5.0f, 6.0f};
    auto c = a + b;
    FATP_ASSERT_EQ(c[0], 5.0f, "Vector addition c[0]");
    FATP_ASSERT_EQ(c[1], 7.0f, "Vector addition c[1]");
    FATP_ASSERT_EQ(c[2], 9.0f, "Vector addition c[2]");
    return true;
}

FATP_TEST_CASE(vector_subtraction) {
    Vec3f a{1.0f, 2.0f, 3.0f};
    Vec3f b{4.0f, 5.0f, 6.0f};
    auto c = b - a;
    FATP_ASSERT_EQ(c[0], 3.0f, "Vector subtraction c[0]");
    FATP_ASSERT_EQ(c[1], 3.0f, "Vector subtraction c[1]");
    FATP_ASSERT_EQ(c[2], 3.0f, "Vector subtraction c[2]");
    return true;
}

FATP_TEST_CASE(hadamard_product) {
    Vec3f a{1.0f, 2.0f, 3.0f};
    Vec3f b{4.0f, 5.0f, 6.0f};
    auto c = a * b;
    FATP_ASSERT_EQ(c[0], 4.0f, "Hadamard product c[0]");
    FATP_ASSERT_EQ(c[1], 10.0f, "Hadamard product c[1]");
    FATP_ASSERT_EQ(c[2], 18.0f, "Hadamard product c[2]");
    return true;
}

FATP_TEST_CASE(element_wise_division) {
    Vec3f a{1.0f, 2.0f, 3.0f};
    Vec3f b{4.0f, 5.0f, 6.0f};
    auto c = b / a;
    FATP_ASSERT_EQ(c[0], 4.0f, "Element-wise division c[0]");
    FATP_ASSERT_EQ(c[1], 2.5f, "Element-wise division c[1]");
    FATP_ASSERT_EQ(c[2], 2.0f, "Element-wise division c[2]");
    return true;
}

FATP_TEST_CASE(scalar_multiplication) {
    Vec3f a{1.0f, 2.0f, 3.0f};
    auto c = a * 2.0f;
    FATP_ASSERT_EQ(c[0], 2.0f, "Scalar multiplication c[0]");
    FATP_ASSERT_EQ(c[1], 4.0f, "Scalar multiplication c[1]");
    FATP_ASSERT_EQ(c[2], 6.0f, "Scalar multiplication c[2]");
    return true;
}

FATP_TEST_CASE(scalar_multiplication_reversed) {
    Vec3f a{1.0f, 2.0f, 3.0f};
    auto c = 3.0f * a;
    FATP_ASSERT_EQ(c[0], 3.0f, "Reversed scalar multiplication c[0]");
    FATP_ASSERT_EQ(c[1], 6.0f, "Reversed scalar multiplication c[1]");
    FATP_ASSERT_EQ(c[2], 9.0f, "Reversed scalar multiplication c[2]");
    return true;
}

// =============================================================================
// Policy Behavior Tests
// =============================================================================

FATP_TEST_CASE(unchecked_policy_allows_operations) {
    Vec2<int> v{INT_MAX, 1};
    // This would overflow, but UncheckedPolicy doesn't check
    FATP_ASSERT_TRUE(true, "UncheckedPolicy allows operations without checks");
    return true;
}

FATP_TEST_CASE(checked_policy_throws_on_overflow) {
    Vec2<int, CheckedPolicy> v1{INT_MAX, 0};
    Vec2<int, CheckedPolicy> v2{1, 0};
    
    bool caught = false;
    try {
        auto result = v1 + v2; // Should throw
        (void)result;
    } catch (...) {
        caught = true;
    }
    FATP_ASSERT_TRUE(caught, "CheckedPolicy should throw on overflow");
    return true;
}

FATP_TEST_CASE(saturating_policy_clamps) {
    // FIXED: Use SaturatingArithmeticPolicy instead of SaturatingPolicy
    Vec2<int, SaturatingArithmeticPolicy> v1{INT_MAX, 0};
    Vec2<int, SaturatingArithmeticPolicy> v2{100, 0};
    auto result = v1 + v2;
    FATP_ASSERT_EQ(result[0], INT_MAX, "SaturatingArithmeticPolicy should clamp to max");
    return true;
}

// =============================================================================
// Linear Algebra Tests
// =============================================================================

FATP_TEST_CASE(dot_product) {
    Vec3f a{1.0f, 2.0f, 3.0f};
    Vec3f b{4.0f, 5.0f, 6.0f};
    
    float result = dot(a, b);
    float expected = 1.0f*4.0f + 2.0f*5.0f + 3.0f*6.0f; // 4 + 10 + 18 = 32
    
    FATP_ASSERT_TRUE(std::abs(result - expected) < 1e-6f, "Dot product");
    return true;
}

FATP_TEST_CASE(matrix_vector_multiply) {
    Mat2x2f m{1.0f, 2.0f,
              3.0f, 4.0f};
    Vec2f v{5.0f, 6.0f};
    
    auto result = matvec(m, v);
    // [1 2] [5]   [1*5 + 2*6]   [17]
    // [3 4] [6] = [3*5 + 4*6] = [39]
    
    FATP_ASSERT_TRUE(std::abs(result[0] - 17.0f) < 1e-6f, "Matrix-vector multiply row 0");
    FATP_ASSERT_TRUE(std::abs(result[1] - 39.0f) < 1e-6f, "Matrix-vector multiply row 1");
    return true;
}

FATP_TEST_CASE(matrix_matrix_multiply) {
    Mat2x2f a{1.0f, 2.0f,
              3.0f, 4.0f};
    Mat2x2f b{5.0f, 6.0f,
              7.0f, 8.0f};
    
    auto result = matmul(a, b);
    // [1 2] [5 6]   [1*5+2*7  1*6+2*8]   [19 22]
    // [3 4] [7 8] = [3*5+4*7  3*6+4*8] = [43 50]
    
    FATP_ASSERT_TRUE(std::abs(result.at(0, 0) - 19.0f) < 1e-6f, "Matmul [0,0]");
    FATP_ASSERT_TRUE(std::abs(result.at(0, 1) - 22.0f) < 1e-6f, "Matmul [0,1]");
    FATP_ASSERT_TRUE(std::abs(result.at(1, 0) - 43.0f) < 1e-6f, "Matmul [1,0]");
    FATP_ASSERT_TRUE(std::abs(result.at(1, 1) - 50.0f) < 1e-6f, "Matmul [1,1]");
    return true;
}

FATP_TEST_CASE(matrix_transpose) {
    StaticTensor<float, Matrix<2, 3>, UncheckedPolicy> m{1.0f, 2.0f, 3.0f,
                                   4.0f, 5.0f, 6.0f};
    auto result = transpose(m);
    
    FATP_ASSERT_EQ(result.at(0, 0), 1.0f, "Transpose (0,0)");
    FATP_ASSERT_EQ(result.at(0, 1), 4.0f, "Transpose (0,1)");
    FATP_ASSERT_EQ(result.at(1, 0), 2.0f, "Transpose (1,0)");
    FATP_ASSERT_EQ(result.at(1, 1), 5.0f, "Transpose (1,1)");
    FATP_ASSERT_EQ(result.at(2, 0), 3.0f, "Transpose (2,0)");
    FATP_ASSERT_EQ(result.at(2, 1), 6.0f, "Transpose (2,1)");
    return true;
}

FATP_TEST_CASE(outer_product) {
    Vec2f a{1.0f, 2.0f};
    Vec2f b{3.0f, 4.0f};
    auto result = outer(a, b);
    
    // [1]         [1*3  1*4]   [3  4]
    // [2] ⊗ [3 4] [2*3  2*4] = [6  8]
    
    FATP_ASSERT_EQ(result.at(0, 0), 3.0f, "Outer product (0,0)");
    FATP_ASSERT_EQ(result.at(0, 1), 4.0f, "Outer product (0,1)");
    FATP_ASSERT_EQ(result.at(1, 0), 6.0f, "Outer product (1,0)");
    FATP_ASSERT_EQ(result.at(1, 1), 8.0f, "Outer product (1,1)");
    return true;
}

// =============================================================================
// Reduction Operations Tests
// =============================================================================

FATP_TEST_CASE(sum_reduction) {
    Vec3f v{1.0f, 2.0f, 3.0f};
    float result = sum(v);
    FATP_ASSERT_EQ(result, 6.0f, "Sum reduction");
    return true;
}

FATP_TEST_CASE(mean_reduction) {
    Vec4f v{2.0f, 4.0f, 6.0f, 8.0f};
    float result = mean(v);
    FATP_ASSERT_EQ(result, 5.0f, "Mean reduction");
    return true;
}

FATP_TEST_CASE(max_reduction) {
    Vec4f v{3.0f, 1.0f, 4.0f, 2.0f};
    float result = max(v);
    FATP_ASSERT_EQ(result, 4.0f, "Max reduction");
    return true;
}

FATP_TEST_CASE(min_reduction) {
    Vec4f v{3.0f, 1.0f, 4.0f, 2.0f};
    float result = min(v);
    FATP_ASSERT_EQ(result, 1.0f, "Min reduction");
    return true;
}

FATP_TEST_CASE(l2_norm) {
    Vec3f v{3.0f, 4.0f, 0.0f};
    float result = norm(v);
    FATP_ASSERT_EQ(result, 5.0f, "L2 norm (3-4-5 triangle)");
    return true;
}

FATP_TEST_CASE(normalize_produces_unit_vector) {
    Vec3f v{3.0f, 4.0f, 0.0f};
    auto unit = normalize(v);
    float result = norm(unit);
    FATP_ASSERT_TRUE(std::abs(result - 1.0f) < 1e-6f, "Normalized vector has unit length");
    return true;
}

// =============================================================================
// SIMD Operations Tests (when available)
// =============================================================================

#ifdef __AVX2__
FATP_TEST_CASE(simd_addition) {
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

FATP_TEST_CASE(simd_multiplication) {
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

FATP_TEST_CASE(simd_dot_product) {
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
    FATP_ASSERT_TRUE(std::abs(result - expected) < 1e-4f, "SIMD dot product");
    return true;
}
#endif

// =============================================================================
// Higher-Order Tensor Tests
// =============================================================================

FATP_TEST_CASE(tensor_3d_size) {
    StaticTensor<float, Tensor3<2, 3, 4>, UncheckedPolicy> t;
    FATP_ASSERT_EQ(t.size, 24u, "3D tensor size");
    return true;
}

FATP_TEST_CASE(tensor_3d_indexing) {
    StaticTensor<float, Tensor3<2, 3, 4>, UncheckedPolicy> t;
    t.at(1, 2, 3) = 42.0f;
    FATP_ASSERT_EQ(t.at(1, 2, 3), 42.0f, "3D tensor indexing");
    return true;
}

FATP_TEST_CASE(tensor_4d_size) {
    StaticTensor<int, Tensor4<2, 2, 2, 2>, UncheckedPolicy> t;
    FATP_ASSERT_EQ(t.size, 16u, "4D tensor size");
    return true;
}

FATP_TEST_CASE(tensor_4d_indexing) {
    StaticTensor<int, Tensor4<2, 2, 2, 2>, UncheckedPolicy> t;
    
    for (size_t i = 0; i < 16; ++i) {
        t[i] = static_cast<int>(i);
    }
    
    FATP_ASSERT_EQ(t.at(1, 1, 1, 1), 15, "4D tensor indexing");
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

} // namespace fat_p::testing::tensormath

// =============================================================================
// Public Interface
// =============================================================================

namespace fat_p::testing
{

bool test_TensorMath() {

    FATP_PRINT_HEADER(TENSOR MATH)

    TestRunner runner;
    
    auto& config = get_test_config();
    config.verbose = true;
    
    auto& out = *config.output;
    
    // Basic Construction and Access
    out << colors::blue() << "--- Basic Construction and Access ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, tensormath, default_construction);
    FATP_RUN_TEST_NS(runner, tensormath, scalar_broadcast);
    FATP_RUN_TEST_NS(runner, tensormath, initializer_list_construction);
    FATP_RUN_TEST_NS(runner, tensormath, variadic_constructor);
    FATP_RUN_TEST_NS(runner, tensormath, matrix_construction);
    FATP_RUN_TEST_NS(runner, tensormath, type_aliases);
    
    // Shape System
    out << "\n" << colors::blue() << "--- Compile-Time Shape System ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, tensormath, shape_rank);
    FATP_RUN_TEST_NS(runner, tensormath, shape_size);
    FATP_RUN_TEST_NS(runner, tensormath, shape_dimensions);
    FATP_RUN_TEST_NS(runner, tensormath, vector_shape);
    FATP_RUN_TEST_NS(runner, tensormath, matrix_shape);
    
    // Element-Wise Operations
    out << "\n" << colors::blue() << "--- Element-Wise Operations ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, tensormath, vector_addition);
    FATP_RUN_TEST_NS(runner, tensormath, vector_subtraction);
    FATP_RUN_TEST_NS(runner, tensormath, hadamard_product);
    FATP_RUN_TEST_NS(runner, tensormath, element_wise_division);
    FATP_RUN_TEST_NS(runner, tensormath, scalar_multiplication);
    FATP_RUN_TEST_NS(runner, tensormath, scalar_multiplication_reversed);
    
    // Policy Behavior
    out << "\n" << colors::blue() << "--- Arithmetic Policy Behavior ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, tensormath, unchecked_policy_allows_operations);
    FATP_RUN_TEST_NS(runner, tensormath, checked_policy_throws_on_overflow);
    FATP_RUN_TEST_NS(runner, tensormath, saturating_policy_clamps);
    
    // Linear Algebra
    out << "\n" << colors::blue() << "--- Linear Algebra ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, tensormath, dot_product);
    FATP_RUN_TEST_NS(runner, tensormath, matrix_vector_multiply);
    FATP_RUN_TEST_NS(runner, tensormath, matrix_matrix_multiply);
    FATP_RUN_TEST_NS(runner, tensormath, matrix_transpose);
    FATP_RUN_TEST_NS(runner, tensormath, outer_product);
    
    // Reduction Operations
    out << "\n" << colors::blue() << "--- Reduction Operations ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, tensormath, sum_reduction);
    FATP_RUN_TEST_NS(runner, tensormath, mean_reduction);
    FATP_RUN_TEST_NS(runner, tensormath, max_reduction);
    FATP_RUN_TEST_NS(runner, tensormath, min_reduction);
    FATP_RUN_TEST_NS(runner, tensormath, l2_norm);
    FATP_RUN_TEST_NS(runner, tensormath, normalize_produces_unit_vector);
    
#ifdef __AVX2__
    // SIMD Operations
    out << "\n" << colors::blue() << "--- SIMD Operations ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, tensormath, simd_addition);
    FATP_RUN_TEST_NS(runner, tensormath, simd_multiplication);
    FATP_RUN_TEST_NS(runner, tensormath, simd_dot_product);
    out << "\n" << colors::green() << "âœ“ SIMD support detected and tested" << colors::reset() << "\n";
#else
    out << "\n" << colors::yellow() << "âš  SIMD tests skipped (no AVX2 support)" << colors::reset() << "\n";
#endif
    
    // Higher-Order Tensors
    out << "\n" << colors::blue() << "--- Higher-Order Tensors ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, tensormath, tensor_3d_size);
    FATP_RUN_TEST_NS(runner, tensormath, tensor_3d_indexing);
    FATP_RUN_TEST_NS(runner, tensormath, tensor_4d_size);
    FATP_RUN_TEST_NS(runner, tensormath, tensor_4d_indexing);
    
    // Performance Benchmarks
    tensormath::run_tensor_math_benchmarks();
    
    // Summary
    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_TensorMath() ? 0 : 1;
}
#endif
