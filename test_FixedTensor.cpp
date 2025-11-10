/**
 * @file test_FixedTensor.cpp
 * @brief Comprehensive tests for FixedTensor - compile-time shape checking
 */

#include "test_FixedTensor.h"
#include "test_Utilities.h"
#include "FixedTensor.h"
#include <iostream>
#include <sstream>
#include <cmath>

namespace cpp_utilities {
namespace testing {

// =============================================================================
// Test Suite 1: Basic Construction and Access
// =============================================================================

bool test_fixed_tensor_basic() {
    std::cout << colors::cyan() << "Test Suite 1: Basic Construction and Access" << colors::reset() << "\n";
    
    // Default construction
    FixedTensor<float, 3, 4> mat1;
    SIMPLE_ASSERT(mat1.get_size() == 12u, "Size should be 12");
    SIMPLE_ASSERT(mat1.get_rank() == 2u, "Rank should be 2");
    
    // Fill constructor
    FixedTensor<float, 3, 4> mat2(5.0f);
    SIMPLE_ASSERT(mat2(0, 0) == 5.0f, "Fill value should be 5.0");
    SIMPLE_ASSERT(mat2(2, 3) == 5.0f, "All elements should be filled");
    
    // Initializer list
    FixedTensor<int, 2, 2> mat3{1, 2, 3, 4};
    SIMPLE_ASSERT(mat3(0, 0) == 1, "mat3(0,0) should be 1");
    SIMPLE_ASSERT(mat3(0, 1) == 2, "mat3(0,1) should be 2");
    SIMPLE_ASSERT(mat3(1, 0) == 3, "mat3(1,0) should be 3");
    SIMPLE_ASSERT(mat3(1, 1) == 4, "mat3(1,1) should be 4");
    
    // Array constructor
    std::array<double, 6> arr = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};
    FixedTensor<double, 2, 3> mat4(arr);
    SIMPLE_ASSERT(mat4(0, 0) == 1.0, "mat4(0,0) should be 1.0");
    SIMPLE_ASSERT(mat4(1, 2) == 6.0, "mat4(1,2) should be 6.0");
    
    // Flat indexing
    SIMPLE_ASSERT(mat3[0] == 1, "Flat index [0] should be 1");
    SIMPLE_ASSERT(mat3[3] == 4, "Flat index [3] should be 4");
    
    // Iterators
    int sum = 0;
    for (auto val : mat3) {
        sum += val;
    }
    SIMPLE_ASSERT(sum == 10, "Sum via iterator should be 10");
    
    return true;
}

// =============================================================================
// Test Suite 2: Compile-Time Shape Checking
// =============================================================================

bool test_fixed_tensor_shape_checking() {
    std::cout << colors::cyan() << "Test Suite 2: Compile-Time Shape Checking" << colors::reset() << "\n";
    
    // These should compile
    FixedTensor<float, 3, 4> A;
    FixedTensor<float, 4, 5> B;
    
    // Matrix multiplication: 3x4 * 4x5 = 3x5
    auto C = matmul(A, B);
    static_assert(decltype(C)::rank == 2, "Result should be 2D");
    static_assert(decltype(C)::size == 15, "Result should have 15 elements");
    
    // Shapes equal check
    FixedTensor<float, 3, 4> D;
    static_assert(shapes_equal_v<decltype(A)::shape_type, decltype(D)::shape_type>,
                  "A and D should have equal shapes");
    
    // Matrix multiplication compatibility
    static_assert(matmul_compatible_v<Shape<3, 4>, Shape<4, 5>>,
                  "3x4 and 4x5 should be matmul compatible");
    
    static_assert(!matmul_compatible_v<Shape<3, 4>, Shape<3, 5>>,
                  "3x4 and 3x5 should NOT be matmul compatible");
    
    // The following would be compile errors (commented out):
    // auto bad = matmul(A, D);  // ERROR: 3x4 * 3x4 invalid
    // FixedTensor<float, 3, 3> E;
    // auto bad2 = matmul(A, E);  // ERROR: 3x4 * 3x3 invalid
    
    return true;
}

// =============================================================================
// Test Suite 3: Element-wise Operations
// =============================================================================

bool test_fixed_tensor_elementwise() {
    std::cout << colors::cyan() << "Test Suite 3: Element-wise Operations" << colors::reset() << "\n";
    
    FixedTensor<float, 2, 3> A{1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    FixedTensor<float, 2, 3> B{2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f};
    
    // Addition
    auto C = A + B;
    ASSERT_CLOSE_EPS(C(0, 0), 3.0f, 1e-6f, "C(0,0) should be 3.0");
    ASSERT_CLOSE_EPS(C(1, 2), 13.0f, 1e-6f, "C(1,2) should be 13.0");
    
    // Subtraction
    auto D = B - A;
    ASSERT_CLOSE_EPS(D(0, 0), 1.0f, 1e-6f, "D(0,0) should be 1.0");
    ASSERT_CLOSE_EPS(D(1, 1), 1.0f, 1e-6f, "D(1,1) should be 1.0");
    
    // Element-wise multiplication (Hadamard)
    auto E = A * B;
    ASSERT_CLOSE_EPS(E(0, 0), 2.0f, 1e-6f, "E(0,0) should be 2.0");
    ASSERT_CLOSE_EPS(E(1, 2), 42.0f, 1e-6f, "E(1,2) should be 42.0");
    
    // Element-wise division
    auto F = B / A;
    ASSERT_CLOSE_EPS(F(0, 0), 2.0f, 1e-6f, "F(0,0) should be 2.0");
    ASSERT_CLOSE_EPS(F(0, 1), 1.5f, 1e-6f, "F(0,1) should be 1.5");
    
    // Scalar operations
    auto G = A * 2.0f;
    ASSERT_CLOSE_EPS(G(0, 0), 2.0f, 1e-6f, "G(0,0) should be 2.0");
    ASSERT_CLOSE_EPS(G(1, 2), 12.0f, 1e-6f, "G(1,2) should be 12.0");
    
    auto H = A / 2.0f;
    ASSERT_CLOSE_EPS(H(0, 0), 0.5f, 1e-6f, "H(0,0) should be 0.5");
    
    auto I = A + 10.0f;
    ASSERT_CLOSE_EPS(I(0, 0), 11.0f, 1e-6f, "I(0,0) should be 11.0");
    
    auto J = B - 1.0f;
    ASSERT_CLOSE_EPS(J(0, 0), 1.0f, 1e-6f, "J(0,0) should be 1.0");
    
    return true;
}

// =============================================================================
// Test Suite 4: Matrix Multiplication
// =============================================================================

bool test_fixed_tensor_matmul() {
    std::cout << colors::cyan() << "Test Suite 4: Matrix Multiplication" << colors::reset() << "\n";
    
    // 2x3 * 3x2 = 2x2
    FixedTensor<float, 2, 3> A{
        1.0f, 2.0f, 3.0f,
        4.0f, 5.0f, 6.0f
    };
    
    FixedTensor<float, 3, 2> B{
        7.0f, 8.0f,
        9.0f, 10.0f,
        11.0f, 12.0f
    };
    
    auto C = matmul(A, B);
    
    // C[0][0] = 1*7 + 2*9 + 3*11 = 7 + 18 + 33 = 58
    ASSERT_CLOSE_EPS(C(0, 0), 58.0f, 1e-5f, "C(0,0) should be 58.0");
    
    // C[0][1] = 1*8 + 2*10 + 3*12 = 8 + 20 + 36 = 64
    ASSERT_CLOSE_EPS(C(0, 1), 64.0f, 1e-5f, "C(0,1) should be 64.0");
    
    // C[1][0] = 4*7 + 5*9 + 6*11 = 28 + 45 + 66 = 139
    ASSERT_CLOSE_EPS(C(1, 0), 139.0f, 1e-5f, "C(1,0) should be 139.0");
    
    // C[1][1] = 4*8 + 5*10 + 6*12 = 32 + 50 + 72 = 154
    ASSERT_CLOSE_EPS(C(1, 1), 154.0f, 1e-5f, "C(1,1) should be 154.0");
    
    return true;
}

// =============================================================================
// Test Suite 5: Vector Operations
// =============================================================================

bool test_fixed_tensor_vector_ops() {
    std::cout << colors::cyan() << "Test Suite 5: Vector Operations" << colors::reset() << "\n";
    
    // Dot product
    FixedTensor<float, 4> a{1.0f, 2.0f, 3.0f, 4.0f};
    FixedTensor<float, 4> b{5.0f, 6.0f, 7.0f, 8.0f};
    
    float dp = dot(a, b);
    // 1*5 + 2*6 + 3*7 + 4*8 = 5 + 12 + 21 + 32 = 70
    ASSERT_CLOSE_EPS(dp, 70.0f, 1e-5f, "Dot product should be 70.0");
    
    // L2 norm
    FixedTensor<float, 3> vec{3.0f, 4.0f, 0.0f};
    float n = norm(vec);
    ASSERT_CLOSE_EPS(n, 5.0f, 1e-5f, "Norm of [3,4,0] should be 5.0");
    
    // Normalize
    auto normalized = normalize(vec);
    ASSERT_CLOSE_EPS(normalized[0], 0.6f, 1e-5f, "normalized[0] should be 0.6");
    ASSERT_CLOSE_EPS(normalized[1], 0.8f, 1e-5f, "normalized[1] should be 0.8");
    ASSERT_CLOSE_EPS(normalized[2], 0.0f, 1e-5f, "normalized[2] should be 0.0");
    
    // Verify unit length
    float norm_length = norm(normalized);
    ASSERT_CLOSE_EPS(norm_length, 1.0f, 1e-5f, "Normalized vector should have length 1.0");
    
    return true;
}

// =============================================================================
// Test Suite 6: Reduction Operations
// =============================================================================

bool test_fixed_tensor_reductions() {
    std::cout << colors::cyan() << "Test Suite 6: Reduction Operations" << colors::reset() << "\n";
    
    FixedTensor<int, 2, 3> mat{1, 2, 3, 4, 5, 6};
    
    // Sum
    int s = mat.sum();
    SIMPLE_ASSERT(s == 21, "Sum should be 21");
    
    // Mean
    int m = mat.mean();
    SIMPLE_ASSERT(m == 3, "Mean should be 3 (integer division)");
    
    // Max
    int max_val = mat.max();
    SIMPLE_ASSERT(max_val == 6, "Max should be 6");
    
    // Min
    int min_val = mat.min();
    SIMPLE_ASSERT(min_val == 1, "Min should be 1");
    
    // Float version for accurate mean
    FixedTensor<float, 2, 3> fmat{1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    float fmean = fmat.mean();
    ASSERT_CLOSE_EPS(fmean, 3.5f, 1e-5f, "Float mean should be 3.5");
    
    return true;
}

// =============================================================================
// Test Suite 7: In-place Operations
// =============================================================================

bool test_fixed_tensor_inplace() {
    std::cout << colors::cyan() << "Test Suite 7: In-place Operations" << colors::reset() << "\n";
    
    FixedTensor<float, 2, 2> A{1.0f, 2.0f, 3.0f, 4.0f};
    FixedTensor<float, 2, 2> B{5.0f, 6.0f, 7.0f, 8.0f};
    
    // +=
    A += B;
    ASSERT_CLOSE_EPS(A(0, 0), 6.0f, 1e-6f, "A(0,0) should be 6.0 after +=");
    ASSERT_CLOSE_EPS(A(1, 1), 12.0f, 1e-6f, "A(1,1) should be 12.0 after +=");
    
    // -=
    A -= B;
    ASSERT_CLOSE_EPS(A(0, 0), 1.0f, 1e-6f, "A(0,0) should be 1.0 after -=");
    ASSERT_CLOSE_EPS(A(1, 1), 4.0f, 1e-6f, "A(1,1) should be 4.0 after -=");
    
    // *=
    A *= 2.0f;
    ASSERT_CLOSE_EPS(A(0, 0), 2.0f, 1e-6f, "A(0,0) should be 2.0 after *=");
    ASSERT_CLOSE_EPS(A(1, 1), 8.0f, 1e-6f, "A(1,1) should be 8.0 after *=");
    
    // /=
    A /= 2.0f;
    ASSERT_CLOSE_EPS(A(0, 0), 1.0f, 1e-6f, "A(0,0) should be 1.0 after /=");
    ASSERT_CLOSE_EPS(A(1, 1), 4.0f, 1e-6f, "A(1,1) should be 4.0 after /=");
    
    return true;
}

// =============================================================================
// Test Suite 8: Comparison Operations
// =============================================================================

bool test_fixed_tensor_comparison() {
    std::cout << colors::cyan() << "Test Suite 8: Comparison Operations" << colors::reset() << "\n";
    
    FixedTensor<int, 2, 2> A{1, 2, 3, 4};
    FixedTensor<int, 2, 2> B{1, 2, 3, 4};
    FixedTensor<int, 2, 2> C{1, 2, 3, 5};
    
    // Equality
    SIMPLE_ASSERT(A == B, "A should equal B");
    SIMPLE_ASSERT(!(A == C), "A should not equal C");
    
    // Inequality
    SIMPLE_ASSERT(!(A != B), "A should not be != B");
    SIMPLE_ASSERT(A != C, "A should be != C");
    
    return true;
}

// =============================================================================
// Test Suite 9: Checked Arithmetic Policy
// =============================================================================

bool test_fixed_tensor_checked_arithmetic() {
    std::cout << colors::cyan() << "Test Suite 9: Checked Arithmetic Policy" << colors::reset() << "\n";
    
    // Unchecked (default) - no overflow checking
    FixedTensor<int, 2> unchecked{100, 200};
    auto result1 = unchecked + unchecked;  // No error, may overflow
    (void)result1;  // Suppress unused warning
    
    // Checked - overflow detection
    // Note: In this simplified implementation, we just use regular FixedTensor
    FixedTensor<int, 2> checked{100, 200};
    auto result2 = checked + checked;
    SIMPLE_ASSERT(result2[0] == 200, "checked[0] + checked[0] should be 200");
    SIMPLE_ASSERT(result2[1] == 400, "checked[1] + checked[1] should be 400");
    
    // This would throw on overflow (INT_MAX + 1) if we had real checked arithmetic:
    // FixedTensor<int, 1> overflow{std::numeric_limits<int>::max()};
    // auto bad = overflow + overflow;  // Would throw
    
    return true;
}

// =============================================================================
// Test Suite 10: Type Aliases
// =============================================================================

bool test_fixed_tensor_aliases() {
    std::cout << colors::cyan() << "Test Suite 10: Type Aliases" << colors::reset() << "\n";
    
    // Vector aliases
    FixedVec2f v2{1.0f, 2.0f};
    SIMPLE_ASSERT(v2.get_size() == 2u, "FixedVec2f should have size 2");
    
    FixedVec3d v3{1.0, 2.0, 3.0};
    SIMPLE_ASSERT(v3.get_size() == 3u, "FixedVec3d should have size 3");
    
    FixedVec4f v4{1.0f, 2.0f, 3.0f, 4.0f};
    SIMPLE_ASSERT(v4.get_size() == 4u, "FixedVec4f should have size 4");
    
    // Matrix aliases
    FixedMat2x2f m2{1.0f, 2.0f, 3.0f, 4.0f};
    SIMPLE_ASSERT(m2.get_size() == 4u, "FixedMat2x2f should have size 4");
    
    FixedMat3x3d m3;
    SIMPLE_ASSERT(m3.get_size() == 9u, "FixedMat3x3d should have size 9");
    
    FixedMat4x4f m4;
    SIMPLE_ASSERT(m4.get_size() == 16u, "FixedMat4x4f should have size 16");
    
    // Checked variants
    FixedVecChecked<int, 3> checked_vec{1, 2, 3};
    SIMPLE_ASSERT(checked_vec.get_size() == 3u, "FixedVecChecked should work");
    
    FixedMatChecked<float, 2, 2> checked_mat;
    SIMPLE_ASSERT(checked_mat.get_size() == 4u, "FixedMatChecked should work");
    
    return true;
}

// =============================================================================
// Main Test Function
// =============================================================================

bool test_FixedTensor() {
    PRINT_HEADER(FIXED TENSOR)
    
    TestRunner runner;
    
    RUN_TEST(runner, fixed_tensor_basic);
    RUN_TEST(runner, fixed_tensor_shape_checking);
    RUN_TEST(runner, fixed_tensor_elementwise);
    RUN_TEST(runner, fixed_tensor_matmul);
    RUN_TEST(runner, fixed_tensor_vector_ops);
    RUN_TEST(runner, fixed_tensor_reductions);
    RUN_TEST(runner, fixed_tensor_inplace);
    RUN_TEST(runner, fixed_tensor_comparison);
    RUN_TEST(runner, fixed_tensor_checked_arithmetic);
    RUN_TEST(runner, fixed_tensor_aliases);
    
    return 0 == runner.print_summary();
}

} // namespace testing
} // namespace cpp_utilities
