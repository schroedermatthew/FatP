/**
 * @file test_TensorEinsum.cpp
 * @brief Comprehensive tests for Einstein summation notation
 */
/*
FATP_META:
  meta_version: 1
  component: Tensor
  file_role: test
  path: components/Tensor/tests/test_TensorEinsum.cpp
  layer: Testing
  namespace: fat_p
  summary: "Unit tests for TensorEinsum."
  api_stability: in_work
  related:
    docs_search: "TensorEinsum"
    headers:
      - include/fat_p/Tensor.h
      - include/fat_p/TensorEinsum.h
      - include/fat_p/FatPTest.h
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

#include "FatPTest.h"
#include "Tensor.h"
#include "TensorEinsum.h"

namespace fat_p::testing::tensoreinsum
{

// =============================================================================
// Test Suite 1: Matrix Multiplication
// =============================================================================

FATP_TEST_CASE(matmul)
{
    // Create test matrices
    Tensor<float> A({2, 3});
    A(0, 0) = 1.0f;
    A(0, 1) = 2.0f;
    A(0, 2) = 3.0f;
    A(1, 0) = 4.0f;
    A(1, 1) = 5.0f;
    A(1, 2) = 6.0f;

    Tensor<float> B({3, 2});
    B(0, 0) = 7.0f;
    B(0, 1) = 8.0f;
    B(1, 0) = 9.0f;
    B(1, 1) = 10.0f;
    B(2, 0) = 11.0f;
    B(2, 1) = 12.0f;

    // Matrix multiplication: ij,jk->ik
    auto C = einsum("ij,jk->ik", A, B);

    FATP_ASSERT_EQ(C.extent(0), 2u, "Result should have 2 rows");
    FATP_ASSERT_EQ(C.extent(1), 2u, "Result should have 2 columns");

    // C[0][0] = 1*7 + 2*9 + 3*11 = 58
    FATP_ASSERT_CLOSE_EPS(C(0, 0), 58.0f, 1e-5f, "C(0,0) should be 58.0");

    // C[0][1] = 1*8 + 2*10 + 3*12 = 64
    FATP_ASSERT_CLOSE_EPS(C(0, 1), 64.0f, 1e-5f, "C(0,1) should be 64.0");

    // C[1][0] = 4*7 + 5*9 + 6*11 = 139
    FATP_ASSERT_CLOSE_EPS(C(1, 0), 139.0f, 1e-5f, "C(1,0) should be 139.0");

    // C[1][1] = 4*8 + 5*10 + 6*12 = 154
    FATP_ASSERT_CLOSE_EPS(C(1, 1), 154.0f, 1e-5f, "C(1,1) should be 154.0");

    // Test convenience function
    auto D = matmul_einsum(A, B);
    FATP_ASSERT_CLOSE_EPS(D(0, 0), 58.0f, 1e-5f, "matmul_einsum should give same result");

    return true;
}

// =============================================================================
// Test Suite 2: Batch Matrix Multiplication
// =============================================================================

FATP_TEST_CASE(batch_matmul)
{
    // Create batch of matrices (batch_size=2, 2x3 matrices)
    Tensor<float> A({2, 2, 3});

    // Batch 0
    A(0, 0, 0) = 1.0f;
    A(0, 0, 1) = 2.0f;
    A(0, 0, 2) = 3.0f;
    A(0, 1, 0) = 4.0f;
    A(0, 1, 1) = 5.0f;
    A(0, 1, 2) = 6.0f;

    // Batch 1
    A(1, 0, 0) = 7.0f;
    A(1, 0, 1) = 8.0f;
    A(1, 0, 2) = 9.0f;
    A(1, 1, 0) = 10.0f;
    A(1, 1, 1) = 11.0f;
    A(1, 1, 2) = 12.0f;

    // Create batch of matrices (batch_size=2, 3x2 matrices)
    Tensor<float> B({2, 3, 2});

    // Batch 0
    B(0, 0, 0) = 1.0f;
    B(0, 0, 1) = 2.0f;
    B(0, 1, 0) = 3.0f;
    B(0, 1, 1) = 4.0f;
    B(0, 2, 0) = 5.0f;
    B(0, 2, 1) = 6.0f;

    // Batch 1
    B(1, 0, 0) = 7.0f;
    B(1, 0, 1) = 8.0f;
    B(1, 1, 0) = 9.0f;
    B(1, 1, 1) = 10.0f;
    B(1, 2, 0) = 11.0f;
    B(1, 2, 1) = 12.0f;

    // Batch matrix multiplication: bij,bjk->bik
    auto C = einsum("bij,bjk->bik", A, B);

    FATP_ASSERT_EQ(C.extent(0), 2u, "Result should have batch_size=2");
    FATP_ASSERT_EQ(C.extent(1), 2u, "Result should have 2 rows");
    FATP_ASSERT_EQ(C.extent(2), 2u, "Result should have 2 columns");

    // Batch 0, C[0][0][0] = 1*1 + 2*3 + 3*5 = 22
    FATP_ASSERT_CLOSE_EPS(C(0, 0, 0), 22.0f, 1e-5f, "C(0,0,0) should be 22.0");

    // Batch 0, C[0][0][1] = 1*2 + 2*4 + 3*6 = 28
    FATP_ASSERT_CLOSE_EPS(C(0, 0, 1), 28.0f, 1e-5f, "C(0,0,1) should be 28.0");

    // Batch 1, C[1][0][0] = 7*7 + 8*9 + 9*11 = 220
    FATP_ASSERT_CLOSE_EPS(C(1, 0, 0), 220.0f, 1e-5f, "C(1,0,0) should be 220.0");

    // Test convenience function
    auto D = batch_matmul_einsum(A, B);
    FATP_ASSERT_CLOSE_EPS(D(0, 0, 0), 22.0f, 1e-5f, "batch_matmul_einsum should give same result");

    return true;
}

// =============================================================================
// Test Suite 3: Outer Product
// =============================================================================

FATP_TEST_CASE(outer)
{
    std::cout << colors::cyan() << "Test Suite 3: Outer Product" << colors::reset() << "\n";

    // Create vectors
    Tensor<float> a({3});
    a[0] = 1.0f;
    a[1] = 2.0f;
    a[2] = 3.0f;

    Tensor<float> b({4});
    b[0] = 4.0f;
    b[1] = 5.0f;
    b[2] = 6.0f;
    b[3] = 7.0f;

    // Outer product: i,j->ij
    auto C = einsum("i,j->ij", a, b);

    FATP_ASSERT_EQ(C.extent(0), 3u, "Result should have 3 rows");
    FATP_ASSERT_EQ(C.extent(1), 4u, "Result should have 4 columns");

    FATP_ASSERT_CLOSE_EPS(C(0, 0), 4.0f, 1e-5f, "C(0,0) = 1*4 = 4");
    FATP_ASSERT_CLOSE_EPS(C(0, 3), 7.0f, 1e-5f, "C(0,3) = 1*7 = 7");
    FATP_ASSERT_CLOSE_EPS(C(1, 1), 10.0f, 1e-5f, "C(1,1) = 2*5 = 10");
    FATP_ASSERT_CLOSE_EPS(C(2, 2), 18.0f, 1e-5f, "C(2,2) = 3*6 = 18");

    // Test convenience function
    auto D = outer_einsum(a, b);
    FATP_ASSERT_CLOSE_EPS(D(0, 0), 4.0f, 1e-5f, "outer_einsum should give same result");

    return true;
}

// =============================================================================
// Test Suite 4: Inner Product (Dot Product)
// =============================================================================

FATP_TEST_CASE(dot)
{
    std::cout << colors::cyan() << "Test Suite 4: Inner Product (Dot Product)" << colors::reset() << "\n";

    // Create vectors
    Tensor<float> a({4});
    a[0] = 1.0f;
    a[1] = 2.0f;
    a[2] = 3.0f;
    a[3] = 4.0f;

    Tensor<float> b({4});
    b[0] = 5.0f;
    b[1] = 6.0f;
    b[2] = 7.0f;
    b[3] = 8.0f;

    // Inner product: i,i->
    auto result = einsum("i,i->", a, b);

    FATP_ASSERT_EQ(result.size(), 1u, "Result should be scalar (size 1)");

    // 1*5 + 2*6 + 3*7 + 4*8 = 5 + 12 + 21 + 32 = 70
    FATP_ASSERT_CLOSE_EPS(result[0], 70.0f, 1e-5f, "Dot product should be 70.0");

    // Test convenience function
    float dot_result = dot_einsum(a, b);
    FATP_ASSERT_CLOSE_EPS(dot_result, 70.0f, 1e-5f, "dot_einsum should give same result");

    return true;
}

FATP_TEST_CASE(frobenius_inner_product)
{
    Tensor<float> a({2, 2});
    Tensor<float> b({2, 2});
    for (size_t i = 0; i < 4; ++i)
    {
        a[i] = static_cast<float>(i + 1);
        b[i] = static_cast<float>(i + 5);
    }

    auto result = einsum("ij,ij->", a, b);
    FATP_ASSERT_EQ(result.size(), 1u, "Frobenius inner product should return a scalar tensor");
    FATP_ASSERT_CLOSE_EPS(result[0], 70.0f, 1e-5f, "Frobenius inner product should sum element-wise products");

    bool mismatch_rejected = false;
    try
    {
        Tensor<float> mismatched({2, 3});
        [[maybe_unused]] auto invalid = einsum("ij,ij->", a, mismatched);
    }
    catch (const std::exception&)
    {
        mismatch_rejected = true;
    }
    FATP_ASSERT_TRUE(mismatch_rejected, "Frobenius inner product must reject mismatched shapes");

    return true;
}

// =============================================================================
// Test Suite 5: Transpose
// =============================================================================

FATP_TEST_CASE(transpose)
{
    std::cout << colors::cyan() << "Test Suite 5: Transpose" << colors::reset() << "\n";

    // Create matrix
    Tensor<float> A({2, 3});
    A(0, 0) = 1.0f;
    A(0, 1) = 2.0f;
    A(0, 2) = 3.0f;
    A(1, 0) = 4.0f;
    A(1, 1) = 5.0f;
    A(1, 2) = 6.0f;

    // Transpose: ij->ji
    auto AT = einsum("ij->ji", A);

    FATP_ASSERT_EQ(AT.extent(0), 3u, "Transposed should have 3 rows");
    FATP_ASSERT_EQ(AT.extent(1), 2u, "Transposed should have 2 columns");

    FATP_ASSERT_CLOSE_EPS(AT(0, 0), 1.0f, 1e-5f, "AT(0,0) should be 1.0");
    FATP_ASSERT_CLOSE_EPS(AT(1, 0), 2.0f, 1e-5f, "AT(1,0) should be 2.0");
    FATP_ASSERT_CLOSE_EPS(AT(2, 0), 3.0f, 1e-5f, "AT(2,0) should be 3.0");
    FATP_ASSERT_CLOSE_EPS(AT(0, 1), 4.0f, 1e-5f, "AT(0,1) should be 4.0");
    FATP_ASSERT_CLOSE_EPS(AT(2, 1), 6.0f, 1e-5f, "AT(2,1) should be 6.0");

    // Test convenience function
    auto AT2 = transpose_einsum(A);
    FATP_ASSERT_CLOSE_EPS(AT2(0, 0), 1.0f, 1e-5f, "transpose_einsum should give same result");

    return true;
}

// =============================================================================
// Test Suite 6: Trace
// =============================================================================

FATP_TEST_CASE(trace)
{
    std::cout << colors::cyan() << "Test Suite 6: Trace" << colors::reset() << "\n";

    // Create square matrix
    Tensor<float> A({3, 3});
    A(0, 0) = 1.0f;
    A(0, 1) = 2.0f;
    A(0, 2) = 3.0f;
    A(1, 0) = 4.0f;
    A(1, 1) = 5.0f;
    A(1, 2) = 6.0f;
    A(2, 0) = 7.0f;
    A(2, 1) = 8.0f;
    A(2, 2) = 9.0f;

    // Trace: ii->
    auto trace_result = einsum("ii->", A);

    FATP_ASSERT_EQ(trace_result.size(), 1u, "Trace result should be scalar");

    // Trace = 1 + 5 + 9 = 15
    FATP_ASSERT_CLOSE_EPS(trace_result[0], 15.0f, 1e-5f, "Trace should be 15.0");

    // Test convenience function
    float trace_val = trace_einsum(A);
    FATP_ASSERT_CLOSE_EPS(trace_val, 15.0f, 1e-5f, "trace_einsum should give same result");

    return true;
}

// =============================================================================
// Test Suite 7: Sum Along Axis
// =============================================================================

FATP_TEST_CASE(sum_axis)
{
    std::cout << colors::cyan() << "Test Suite 7: Sum Along Axis" << colors::reset() << "\n";

    // Create matrix
    Tensor<float> A({2, 3});
    A(0, 0) = 1.0f;
    A(0, 1) = 2.0f;
    A(0, 2) = 3.0f;
    A(1, 0) = 4.0f;
    A(1, 1) = 5.0f;
    A(1, 2) = 6.0f;

    // Sum along columns (keep rows): ij->i
    auto row_sums = einsum("ij->i", A);

    FATP_ASSERT_EQ(row_sums.size(), 2u, "Should have 2 row sums");
    FATP_ASSERT_CLOSE_EPS(row_sums[0], 6.0f, 1e-5f, "Row 0 sum = 1+2+3 = 6");
    FATP_ASSERT_CLOSE_EPS(row_sums[1], 15.0f, 1e-5f, "Row 1 sum = 4+5+6 = 15");

    // Sum along rows (keep columns): ij->j
    auto col_sums = einsum("ij->j", A);

    FATP_ASSERT_EQ(col_sums.size(), 3u, "Should have 3 column sums");
    FATP_ASSERT_CLOSE_EPS(col_sums[0], 5.0f, 1e-5f, "Col 0 sum = 1+4 = 5");
    FATP_ASSERT_CLOSE_EPS(col_sums[1], 7.0f, 1e-5f, "Col 1 sum = 2+5 = 7");
    FATP_ASSERT_CLOSE_EPS(col_sums[2], 9.0f, 1e-5f, "Col 2 sum = 3+6 = 9");

    return true;
}

// =============================================================================
// Test Suite 8: Sum All Elements
// =============================================================================
FATP_TEST_CASE(sum_all)
{
    std::cout << colors::cyan() << "Test Suite 8: Sum All Elements" << colors::reset() << "\n";

    // Create matrix
    Tensor<float> A({2, 3});
    A(0, 0) = 1.0f;
    A(0, 1) = 2.0f;
    A(0, 2) = 3.0f;
    A(1, 0) = 4.0f;
    A(1, 1) = 5.0f;
    A(1, 2) = 6.0f;

    // Sum all elements: ij->
    auto total_sum = einsum("ij->", A);

    FATP_ASSERT_EQ(total_sum.size(), 1u, "Result should be scalar");
    FATP_ASSERT_CLOSE_EPS(total_sum[0], 21.0f, 1e-5f, "Sum of all elements should be 21");

    return true;
}

// =============================================================================
// Test Suite 9: Element-wise Product
// =============================================================================

FATP_TEST_CASE(elementwise)
{
    std::cout << colors::cyan() << "Test Suite 9: Element-wise Product" << colors::reset() << "\n";

    // Create matrices
    Tensor<float> A({2, 2});
    A(0, 0) = 1.0f;
    A(0, 1) = 2.0f;
    A(1, 0) = 3.0f;
    A(1, 1) = 4.0f;

    Tensor<float> B({2, 2});
    B(0, 0) = 5.0f;
    B(0, 1) = 6.0f;
    B(1, 0) = 7.0f;
    B(1, 1) = 8.0f;

    // Element-wise product: ij,ij->ij
    auto C = einsum("ij,ij->ij", A, B);

    FATP_ASSERT_CLOSE_EPS(C(0, 0), 5.0f, 1e-5f, "C(0,0) = 1*5 = 5");
    FATP_ASSERT_CLOSE_EPS(C(0, 1), 12.0f, 1e-5f, "C(0,1) = 2*6 = 12");
    FATP_ASSERT_CLOSE_EPS(C(1, 0), 21.0f, 1e-5f, "C(1,0) = 3*7 = 21");
    FATP_ASSERT_CLOSE_EPS(C(1, 1), 32.0f, 1e-5f, "C(1,1) = 4*8 = 32");

    return true;
}

// =============================================================================
// Test Suite 10: Error Handling
// =============================================================================

FATP_TEST_CASE(diagonal_and_strided_inputs)
{
    Tensor<int> square({2, 2});
    square(0, 0) = 1;
    square(0, 1) = 2;
    square(1, 0) = 3;
    square(1, 1) = 4;
    auto diagonal = einsum("ii->i", square);
    FATP_ASSERT_EQ(diagonal[0], 1, "Diagonal element 0");
    FATP_ASSERT_EQ(diagonal[1], 4, "Diagonal element 1");

    Tensor<int> matrix({3, 3});
    for (size_t i = 0; i < matrix.size(); ++i)
    {
        matrix[i] = static_cast<int>(i);
    }
    auto column = matrix.columnView(1);
    auto sum = einsum("ij->", column);
    FATP_ASSERT_EQ(sum[0], 12, "Sum-all must traverse a strided column logically");

    auto product = einsum("ij,ij->ij", column, column);
    FATP_ASSERT_EQ(product[0], 1, "Strided element-wise product 0");
    FATP_ASSERT_EQ(product[1], 16, "Strided element-wise product 1");
    FATP_ASSERT_EQ(product[2], 49, "Strided element-wise product 2");

    return true;
}

FATP_TEST_CASE(errors)
{
    std::cout << colors::cyan() << "Test Suite 10: Error Handling" << colors::reset() << "\n";

    Tensor<float> A({2, 3});
    Tensor<float> B({2, 3}); // Wrong shape for matmul

    // Test dimension mismatch
    bool caught = false;
    try
    {
        auto C = einsum("ij,jk->ik", A, B); // 2x3 * 2x3 invalid
    }
    catch (const std::exception&)
    {
        caught = true;
    }
    FATP_ASSERT_TRUE(caught, "Should throw on dimension mismatch");

    // Test invalid notation
    caught = false;
    try
    {
        auto C = einsum("xyz", A); // Invalid notation (no ->)
    }
    catch (const std::exception&)
    {
        caught = true;
    }
    FATP_ASSERT_TRUE(caught, "Should throw on invalid notation");

    // Test unsupported pattern
    caught = false;
    try
    {
        auto C = einsum("ijk,klm->ijlm", A, B); // Unsupported pattern
    }
    catch (const std::exception&)
    {
        caught = true;
    }
    FATP_ASSERT_TRUE(caught, "Should throw on unsupported pattern");

    caught = false;
    try
    {
        [[maybe_unused]] auto C = einsum("i,i->i", A, B);
    }
    catch (const std::exception&)
    {
        caught = true;
    }
    FATP_ASSERT_TRUE(caught, "Label count must match each tensor rank");

    return true;
}

} // namespace fat_p::testing::tensoreinsum

// =============================================================================
// Public Interface
// =============================================================================

namespace fat_p::testing
{


inline void run_benchmarks()
{
    using namespace fat_p::testing;

    auto& out = *get_test_config().output;
    out << "\n" << colors::cyan() << "Sanity Benchmark:" << colors::reset() << "\n";
    out << "  (benchmarks are provided by benchmark executables; unit tests do not run full benchmarks)\n\n";
}

bool test_TensorEinsum()
{
    FATP_PRINT_HEADER(TENSOR EINSUM)

    TestRunner runner;

    FATP_RUN_TEST_NS(runner, tensoreinsum, matmul);
    FATP_RUN_TEST_NS(runner, tensoreinsum, batch_matmul);
    FATP_RUN_TEST_NS(runner, tensoreinsum, outer);
    FATP_RUN_TEST_NS(runner, tensoreinsum, dot);
    FATP_RUN_TEST_NS(runner, tensoreinsum, frobenius_inner_product);
    FATP_RUN_TEST_NS(runner, tensoreinsum, transpose);
    FATP_RUN_TEST_NS(runner, tensoreinsum, trace);
    FATP_RUN_TEST_NS(runner, tensoreinsum, sum_axis);
    FATP_RUN_TEST_NS(runner, tensoreinsum, sum_all);
    FATP_RUN_TEST_NS(runner, tensoreinsum, elementwise);
    FATP_RUN_TEST_NS(runner, tensoreinsum, diagonal_and_strided_inputs);
    FATP_RUN_TEST_NS(runner, tensoreinsum, errors);

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_TensorEinsum() ? 0 : 1;
}
#endif
