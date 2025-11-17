/**
 * @file test_TensorEinsum.cpp  
 * @brief Comprehensive tests for Einstein summation notation
 */

#include <iostream>
#include <cmath>

#include "Tensor.h"
#include "TensorEinsum.h"
#include "test_TensorEinsum.h"
#include "FatPTest.h"

namespace fat_p::testing
{

// =============================================================================
// Test Suite 1: Matrix Multiplication
// =============================================================================

bool test_einsum_matmul() {
    std::cout << colors::cyan() << "Test Suite 1: Matrix Multiplication" << colors::reset() << "\n";
    
    // Create test matrices
    Tensor<float> A({2, 3});
    A(0, 0) = 1.0f; A(0, 1) = 2.0f; A(0, 2) = 3.0f;
    A(1, 0) = 4.0f; A(1, 1) = 5.0f; A(1, 2) = 6.0f;
    
    Tensor<float> B({3, 2});
    B(0, 0) = 7.0f; B(0, 1) = 8.0f;
    B(1, 0) = 9.0f; B(1, 1) = 10.0f;
    B(2, 0) = 11.0f; B(2, 1) = 12.0f;
    
    // Matrix multiplication: ij,jk->ik
    auto C = einsum("ij,jk->ik", A, B);
    
    SIMPLE_ASSERT(C.shape()[0] == 2u, "Result should have 2 rows");
    SIMPLE_ASSERT(C.shape()[1] == 2u, "Result should have 2 columns");
    
    // C[0][0] = 1*7 + 2*9 + 3*11 = 58
    ASSERT_CLOSE_EPS(C(0, 0), 58.0f, 1e-5f, "C(0,0) should be 58.0");
    
    // C[0][1] = 1*8 + 2*10 + 3*12 = 64
    ASSERT_CLOSE_EPS(C(0, 1), 64.0f, 1e-5f, "C(0,1) should be 64.0");
    
    // C[1][0] = 4*7 + 5*9 + 6*11 = 139
    ASSERT_CLOSE_EPS(C(1, 0), 139.0f, 1e-5f, "C(1,0) should be 139.0");
    
    // C[1][1] = 4*8 + 5*10 + 6*12 = 154
    ASSERT_CLOSE_EPS(C(1, 1), 154.0f, 1e-5f, "C(1,1) should be 154.0");
    
    // Test convenience function
    auto D = matmul_einsum(A, B);
    ASSERT_CLOSE_EPS(D(0, 0), 58.0f, 1e-5f, "matmul_einsum should give same result");
    
    return true;
}

// =============================================================================
// Test Suite 2: Batch Matrix Multiplication
// =============================================================================

bool test_einsum_batch_matmul() {
    std::cout << colors::cyan() << "Test Suite 2: Batch Matrix Multiplication" << colors::reset() << "\n";
    
    // Create batch of matrices (batch_size=2, 2x3 matrices)
    Tensor<float> A({2, 2, 3});
    
    // Batch 0
    A(0, 0, 0) = 1.0f; A(0, 0, 1) = 2.0f; A(0, 0, 2) = 3.0f;
    A(0, 1, 0) = 4.0f; A(0, 1, 1) = 5.0f; A(0, 1, 2) = 6.0f;
    
    // Batch 1
    A(1, 0, 0) = 7.0f; A(1, 0, 1) = 8.0f; A(1, 0, 2) = 9.0f;
    A(1, 1, 0) = 10.0f; A(1, 1, 1) = 11.0f; A(1, 1, 2) = 12.0f;
    
    // Create batch of matrices (batch_size=2, 3x2 matrices)
    Tensor<float> B({2, 3, 2});
    
    // Batch 0
    B(0, 0, 0) = 1.0f; B(0, 0, 1) = 2.0f;
    B(0, 1, 0) = 3.0f; B(0, 1, 1) = 4.0f;
    B(0, 2, 0) = 5.0f; B(0, 2, 1) = 6.0f;
    
    // Batch 1
    B(1, 0, 0) = 7.0f; B(1, 0, 1) = 8.0f;
    B(1, 1, 0) = 9.0f; B(1, 1, 1) = 10.0f;
    B(1, 2, 0) = 11.0f; B(1, 2, 1) = 12.0f;
    
    // Batch matrix multiplication: bij,bjk->bik
    auto C = einsum("bij,bjk->bik", A, B);
    
    SIMPLE_ASSERT(C.shape()[0] == 2u, "Result should have batch_size=2");
    SIMPLE_ASSERT(C.shape()[1] == 2u, "Result should have 2 rows");
    SIMPLE_ASSERT(C.shape()[2] == 2u, "Result should have 2 columns");
    
    // Batch 0, C[0][0][0] = 1*1 + 2*3 + 3*5 = 22
    ASSERT_CLOSE_EPS(C(0, 0, 0), 22.0f, 1e-5f, "C(0,0,0) should be 22.0");
    
    // Batch 0, C[0][0][1] = 1*2 + 2*4 + 3*6 = 28
    ASSERT_CLOSE_EPS(C(0, 0, 1), 28.0f, 1e-5f, "C(0,0,1) should be 28.0");
    
    // Batch 1, C[1][0][0] = 7*7 + 8*9 + 9*11 = 220
    ASSERT_CLOSE_EPS(C(1, 0, 0), 220.0f, 1e-5f, "C(1,0,0) should be 220.0");
    
    // Test convenience function
    auto D = batch_matmul_einsum(A, B);
    ASSERT_CLOSE_EPS(D(0, 0, 0), 22.0f, 1e-5f, "batch_matmul_einsum should give same result");
    
    return true;
}

// =============================================================================
// Test Suite 3: Outer Product
// =============================================================================

bool test_einsum_outer() {
    std::cout << colors::cyan() << "Test Suite 3: Outer Product" << colors::reset() << "\n";
    
    // Create vectors
    Tensor<float> a({3});
    a[0] = 1.0f; a[1] = 2.0f; a[2] = 3.0f;
    
    Tensor<float> b({4});
    b[0] = 4.0f; b[1] = 5.0f; b[2] = 6.0f; b[3] = 7.0f;
    
    // Outer product: i,j->ij
    auto C = einsum("i,j->ij", a, b);
    
    SIMPLE_ASSERT(C.shape()[0] == 3u, "Result should have 3 rows");
    SIMPLE_ASSERT(C.shape()[1] == 4u, "Result should have 4 columns");
    
    ASSERT_CLOSE_EPS(C(0, 0), 4.0f, 1e-5f, "C(0,0) = 1*4 = 4");
    ASSERT_CLOSE_EPS(C(0, 3), 7.0f, 1e-5f, "C(0,3) = 1*7 = 7");
    ASSERT_CLOSE_EPS(C(1, 1), 10.0f, 1e-5f, "C(1,1) = 2*5 = 10");
    ASSERT_CLOSE_EPS(C(2, 2), 18.0f, 1e-5f, "C(2,2) = 3*6 = 18");
    
    // Test convenience function
    auto D = outer_einsum(a, b);
    ASSERT_CLOSE_EPS(D(0, 0), 4.0f, 1e-5f, "outer_einsum should give same result");
    
    return true;
}

// =============================================================================
// Test Suite 4: Inner Product (Dot Product)
// =============================================================================

bool test_einsum_dot() {
    std::cout << colors::cyan() << "Test Suite 4: Inner Product (Dot Product)" << colors::reset() << "\n";
    
    // Create vectors
    Tensor<float> a({4});
    a[0] = 1.0f; a[1] = 2.0f; a[2] = 3.0f; a[3] = 4.0f;
    
    Tensor<float> b({4});
    b[0] = 5.0f; b[1] = 6.0f; b[2] = 7.0f; b[3] = 8.0f;
    
    // Inner product: i,i->
    auto result = einsum("i,i->", a, b);
    
    SIMPLE_ASSERT(result.size() == 1u, "Result should be scalar (size 1)");
    
    // 1*5 + 2*6 + 3*7 + 4*8 = 5 + 12 + 21 + 32 = 70
    ASSERT_CLOSE_EPS(result[0], 70.0f, 1e-5f, "Dot product should be 70.0");
    
    // Test convenience function
    float dot_result = dot_einsum(a, b);
    ASSERT_CLOSE_EPS(dot_result, 70.0f, 1e-5f, "dot_einsum should give same result");
    
    return true;
}

// =============================================================================
// Test Suite 5: Transpose
// =============================================================================

bool test_einsum_transpose() {
    std::cout << colors::cyan() << "Test Suite 5: Transpose" << colors::reset() << "\n";
    
    // Create matrix
    Tensor<float> A({2, 3});
    A(0, 0) = 1.0f; A(0, 1) = 2.0f; A(0, 2) = 3.0f;
    A(1, 0) = 4.0f; A(1, 1) = 5.0f; A(1, 2) = 6.0f;
    
    // Transpose: ij->ji
    auto AT = einsum("ij->ji", A);
    
    SIMPLE_ASSERT(AT.shape()[0] == 3u, "Transposed should have 3 rows");
    SIMPLE_ASSERT(AT.shape()[1] == 2u, "Transposed should have 2 columns");
    
    ASSERT_CLOSE_EPS(AT(0, 0), 1.0f, 1e-5f, "AT(0,0) should be 1.0");
    ASSERT_CLOSE_EPS(AT(1, 0), 2.0f, 1e-5f, "AT(1,0) should be 2.0");
    ASSERT_CLOSE_EPS(AT(2, 0), 3.0f, 1e-5f, "AT(2,0) should be 3.0");
    ASSERT_CLOSE_EPS(AT(0, 1), 4.0f, 1e-5f, "AT(0,1) should be 4.0");
    ASSERT_CLOSE_EPS(AT(2, 1), 6.0f, 1e-5f, "AT(2,1) should be 6.0");
    
    // Test convenience function
    auto AT2 = transpose_einsum(A);
    ASSERT_CLOSE_EPS(AT2(0, 0), 1.0f, 1e-5f, "transpose_einsum should give same result");
    
    return true;
}

// =============================================================================
// Test Suite 6: Trace
// =============================================================================

bool test_einsum_trace() {
    std::cout << colors::cyan() << "Test Suite 6: Trace" << colors::reset() << "\n";
    
    // Create square matrix
    Tensor<float> A({3, 3});
    A(0, 0) = 1.0f; A(0, 1) = 2.0f; A(0, 2) = 3.0f;
    A(1, 0) = 4.0f; A(1, 1) = 5.0f; A(1, 2) = 6.0f;
    A(2, 0) = 7.0f; A(2, 1) = 8.0f; A(2, 2) = 9.0f;
    
    // Trace: ii->
    auto trace_result = einsum("ii->", A);
    
    SIMPLE_ASSERT(trace_result.size() == 1u, "Trace result should be scalar");
    
    // Trace = 1 + 5 + 9 = 15
    ASSERT_CLOSE_EPS(trace_result[0], 15.0f, 1e-5f, "Trace should be 15.0");
    
    // Test convenience function
    float trace_val = trace_einsum(A);
    ASSERT_CLOSE_EPS(trace_val, 15.0f, 1e-5f, "trace_einsum should give same result");
    
    return true;
}

// =============================================================================
// Test Suite 7: Sum Along Axis
// =============================================================================

bool test_einsum_sum_axis() {
    std::cout << colors::cyan() << "Test Suite 7: Sum Along Axis" << colors::reset() << "\n";
    
    // Create matrix
    Tensor<float> A({2, 3});
    A(0, 0) = 1.0f; A(0, 1) = 2.0f; A(0, 2) = 3.0f;
    A(1, 0) = 4.0f; A(1, 1) = 5.0f; A(1, 2) = 6.0f;
    
    // Sum along columns (keep rows): ij->i
    auto row_sums = einsum("ij->i", A);
    
    SIMPLE_ASSERT(row_sums.size() == 2u, "Should have 2 row sums");
    ASSERT_CLOSE_EPS(row_sums[0], 6.0f, 1e-5f, "Row 0 sum = 1+2+3 = 6");
    ASSERT_CLOSE_EPS(row_sums[1], 15.0f, 1e-5f, "Row 1 sum = 4+5+6 = 15");
    
    // Sum along rows (keep columns): ij->j
    auto col_sums = einsum("ij->j", A);
    
    SIMPLE_ASSERT(col_sums.size() == 3u, "Should have 3 column sums");
    ASSERT_CLOSE_EPS(col_sums[0], 5.0f, 1e-5f, "Col 0 sum = 1+4 = 5");
    ASSERT_CLOSE_EPS(col_sums[1], 7.0f, 1e-5f, "Col 1 sum = 2+5 = 7");
    ASSERT_CLOSE_EPS(col_sums[2], 9.0f, 1e-5f, "Col 2 sum = 3+6 = 9");
    
    return true;
}

// =============================================================================
// Test Suite 8: Sum All Elements
// =============================================================================
bool test_einsum_sum_all() {
    std::cout << colors::cyan() << "Test Suite 8: Sum All Elements" << colors::reset() << "\n";
    
    // Create matrix
    Tensor<float> A({2, 3});
    A(0, 0) = 1.0f; A(0, 1) = 2.0f; A(0, 2) = 3.0f;
    A(1, 0) = 4.0f; A(1, 1) = 5.0f; A(1, 2) = 6.0f;
    
    // Sum all elements: ij->
    auto total_sum = einsum("ij->", A);
    
    SIMPLE_ASSERT(total_sum.size() == 1u, "Result should be scalar");
    ASSERT_CLOSE_EPS(total_sum[0], 21.0f, 1e-5f, "Sum of all elements should be 21");
    
    return true;
}

// =============================================================================
// Test Suite 9: Element-wise Product
// =============================================================================

bool test_einsum_elementwise() {
    std::cout << colors::cyan() << "Test Suite 9: Element-wise Product" << colors::reset() << "\n";
    
    // Create matrices
    Tensor<float> A({2, 2});
    A(0, 0) = 1.0f; A(0, 1) = 2.0f;
    A(1, 0) = 3.0f; A(1, 1) = 4.0f;
    
    Tensor<float> B({2, 2});
    B(0, 0) = 5.0f; B(0, 1) = 6.0f;
    B(1, 0) = 7.0f; B(1, 1) = 8.0f;
    
    // Element-wise product: ij,ij->ij
    auto C = einsum("ij,ij->ij", A, B);
    
    ASSERT_CLOSE_EPS(C(0, 0), 5.0f, 1e-5f, "C(0,0) = 1*5 = 5");
    ASSERT_CLOSE_EPS(C(0, 1), 12.0f, 1e-5f, "C(0,1) = 2*6 = 12");
    ASSERT_CLOSE_EPS(C(1, 0), 21.0f, 1e-5f, "C(1,0) = 3*7 = 21");
    ASSERT_CLOSE_EPS(C(1, 1), 32.0f, 1e-5f, "C(1,1) = 4*8 = 32");
    
    return true;
}

// =============================================================================
// Test Suite 10: Error Handling
// =============================================================================

bool test_einsum_errors() {
    std::cout << colors::cyan() << "Test Suite 10: Error Handling" << colors::reset() << "\n";
    
    Tensor<float> A({2, 3});
    Tensor<float> B({2, 3});  // Wrong shape for matmul
    
    // Test dimension mismatch
    bool caught = false;
    try {
        auto C = einsum("ij,jk->ik", A, B);  // 2x3 * 2x3 invalid
    } catch (const std::exception&) {
        caught = true;
    }
    SIMPLE_ASSERT(caught, "Should throw on dimension mismatch");
    
    // Test invalid notation
    caught = false;
    try {
        auto C = einsum("xyz", A);  // Invalid notation (no ->)
    } catch (const std::exception&) {
        caught = true;
    }
    SIMPLE_ASSERT(caught, "Should throw on invalid notation");
    
    // Test unsupported pattern
    caught = false;
    try {
        auto C = einsum("ijk,klm->ijlm", A, B);  // Unsupported pattern
    } catch (const std::exception&) {
        caught = true;
    }
    SIMPLE_ASSERT(caught, "Should throw on unsupported pattern");
    
    return true;
}

// =============================================================================
// Main Test Function
// =============================================================================

bool test_TensorEinsum() {
    PRINT_HEADER(TENSOR EINSUM)
    
    TestRunner runner;
    
    RUN_TEST(runner, einsum_matmul);
    RUN_TEST(runner, einsum_batch_matmul);
    RUN_TEST(runner, einsum_outer);
    RUN_TEST(runner, einsum_dot);
    RUN_TEST(runner, einsum_transpose);
    RUN_TEST(runner, einsum_trace);
    RUN_TEST(runner, einsum_sum_axis);
    RUN_TEST(runner, einsum_sum_all);
    RUN_TEST(runner, einsum_elementwise);
    RUN_TEST(runner, einsum_errors);
    
    return 0 == runner.print_summary();
}

} // namespace fat_p::testing
