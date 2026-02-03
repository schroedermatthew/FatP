/**
 * @file test_CSRMatrix_HPC.cpp
 * @brief Comprehensive unit tests for CSRMatrix_HPC.h
 */
/*
FATP_META:
  meta_version: 1
  component: CSRMatrix_HPC
  file_role: test
  path: components/CSRMatrix/tests/test_CSRMatrix_HPC.cpp
  layer: Testing
  namespace: fat_p
  summary: "Unit tests for CSRMatrix_HPC."
  api_stability: in_work
  related:
    docs_search: "CSRMatrix_HPC"
    headers:
      - include/fat_p/CSRMatrix_HPC.h
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

#include <cstddef>
#include <iomanip>
#include <iostream>
#include <vector>

#include "CSRMatrix_HPC.h"
#include "FatPTest.h"

namespace fat_p::testing::csrmatrix_hpc
{

// =============================================================================
// Structural Tests
// =============================================================================

FATP_TEST_CASE(is_symmetric_structural_check)
{
    // Edge case: Matrix where A(0,1)=5, A(1,0)=0
    // Values arrays would be identical [5], but structure differs
    // Old buggy code would return true; fixed code returns false

    using Matrix = fat_p::HpcCSRMatrix<double>;

    // Build non-symmetric matrix: A(0,1) = 5 only
    std::vector<int32_t> rows_a = {0};
    std::vector<int32_t> cols_a = {1};
    std::vector<double> vals_a = {5.0};
    Matrix A(2, 2, rows_a, cols_a, vals_a);

    // A is NOT symmetric (A(1,0) = 0, A(0,1) = 5)
    FATP_ASSERT_TRUE(!A.is_symmetric(), "Non-symmetric matrix identified as symmetric");

    // Build symmetric matrix
    std::vector<int32_t> rows_b = {0, 1};
    std::vector<int32_t> cols_b = {1, 0};
    std::vector<double> vals_b = {5.0, 5.0};
    Matrix B(2, 2, rows_b, cols_b, vals_b);

    FATP_ASSERT_TRUE(B.is_symmetric(), "Symmetric matrix not recognized");

    return true;
}

FATP_TEST_CASE(is_symmetric_diagonal)
{
    using Matrix = fat_p::HpcCSRMatrix<double>;

    // Diagonal matrix is always symmetric
    std::vector<int32_t> rows = {0, 1, 2};
    std::vector<int32_t> cols = {0, 1, 2};
    std::vector<double> vals = {1.0, 2.0, 3.0};
    Matrix D(3, 3, rows, cols, vals);

    FATP_ASSERT_TRUE(D.is_symmetric(), "Diagonal matrix should be symmetric");

    return true;
}

FATP_TEST_CASE(is_symmetric_empty)
{
    using Matrix = fat_p::HpcCSRMatrix<double>;

    Matrix E(5, 5);
    FATP_ASSERT_TRUE(E.is_symmetric(), "Empty matrix should be symmetric");

    return true;
}

// =============================================================================
// SpMV Tests
// =============================================================================

FATP_TEST_CASE(matvec_basic)
{
    using Matrix = fat_p::HpcCSRMatrix<double>;

    // Matrix with non-empty last row (tests prefetch safety)
    // [ 1  0  0 ]
    // [ 0  2  0 ]
    // [ 3  0  4 ]
    std::vector<int32_t> rows = {0, 1, 2, 2};
    std::vector<int32_t> cols = {0, 1, 0, 2};
    std::vector<double> vals = {1.0, 2.0, 3.0, 4.0};
    Matrix A(3, 3, rows, cols, vals);

    double x[3] = {1.0, 2.0, 3.0};
    double y[3] = {0.0, 0.0, 0.0};

    A.matvec(x, y);

    // y[0] = 1*1 = 1
    // y[1] = 2*2 = 4
    // y[2] = 3*1 + 4*3 = 15
    FATP_ASSERT_CLOSE(y[0], 1.0, "y[0] should be 1.0");
    FATP_ASSERT_CLOSE(y[1], 4.0, "y[1] should be 4.0");
    FATP_ASSERT_CLOSE(y[2], 15.0, "y[2] should be 15.0");

    return true;
}

FATP_TEST_CASE(matvec_alpha_beta)
{
    using Matrix = fat_p::HpcCSRMatrix<double>;

    std::vector<int32_t> rows = {0, 1, 2, 2};
    std::vector<int32_t> cols = {0, 1, 0, 2};
    std::vector<double> vals = {1.0, 2.0, 3.0, 4.0};
    Matrix A(3, 3, rows, cols, vals);

    double x[3] = {1.0, 2.0, 3.0};
    double y[3] = {10.0, 10.0, 10.0};

    // y = 2*A*x + 3*y
    A.matvec(2.0, x, 3.0, y);

    // y[0] = 2*1 + 3*10 = 32
    // y[1] = 2*4 + 3*10 = 38
    // y[2] = 2*15 + 3*10 = 60
    FATP_ASSERT_CLOSE(y[0], 32.0, "y[0] should be 32.0");
    FATP_ASSERT_CLOSE(y[1], 38.0, "y[1] should be 38.0");
    FATP_ASSERT_CLOSE(y[2], 60.0, "y[2] should be 60.0");

    return true;
}

FATP_TEST_CASE(matvec_empty_rows)
{
    using Matrix = fat_p::HpcCSRMatrix<double>;

    // Matrix with empty middle row
    // [ 1  0 ]
    // [ 0  0 ]
    // [ 0  2 ]
    std::vector<int32_t> rows = {0, 2};
    std::vector<int32_t> cols = {0, 1};
    std::vector<double> vals = {1.0, 2.0};
    Matrix A(3, 2, rows, cols, vals);

    double x[2] = {3.0, 4.0};
    double y[3] = {99.0, 99.0, 99.0};

    A.matvec(x, y);

    FATP_ASSERT_CLOSE(y[0], 3.0, "y[0] should be 3.0");
    FATP_ASSERT_CLOSE(y[1], 0.0, "y[1] should be 0.0");
    FATP_ASSERT_CLOSE(y[2], 8.0, "y[2] should be 8.0");

    return true;
}

FATP_TEST_CASE(matvec_single_element)
{
    using Matrix = fat_p::HpcCSRMatrix<double>;

    std::vector<int32_t> rows = {0};
    std::vector<int32_t> cols = {0};
    std::vector<double> vals = {7.0};
    Matrix A(1, 1, rows, cols, vals);

    double x[1] = {3.0};
    double y[1] = {0.0};

    A.matvec(x, y);

    FATP_ASSERT_CLOSE(y[0], 21.0, "y[0] should be 21.0");

    return true;
}

FATP_TEST_CASE(matvec_prefetch_toggle)
{
    using Matrix = fat_p::HpcCSRMatrix<double>;

    // Build a matrix large enough for prefetch to matter
    constexpr std::size_t N = 500;
    std::vector<int32_t> rows, cols;
    std::vector<double> vals;

    for (std::size_t i = 0; i < N; ++i)
    {
        for (int offset = -3; offset <= 3; ++offset)
        {
            auto j = static_cast<std::ptrdiff_t>(i) + offset;
            if (j >= 0 && static_cast<std::size_t>(j) < N)
            {
                rows.push_back(static_cast<int32_t>(i));
                cols.push_back(static_cast<int32_t>(j));
                vals.push_back(offset == 0 ? 4.0 : -1.0);
            }
        }
    }

    Matrix A(N, N, rows, cols, vals);

    std::vector<double> x(N, 1.0);
    std::vector<double> y_prefetch(N);
    std::vector<double> y_no_prefetch(N);

    // Test y = A*x with prefetch on and off
    A.matvec(x.data(), y_prefetch.data(), true);
    A.matvec(x.data(), y_no_prefetch.data(), false);

    for (std::size_t i = 0; i < N; ++i)
    {
        FATP_ASSERT_CLOSE(y_prefetch[i], y_no_prefetch[i], "Prefetch toggle should not affect correctness");
    }

    // Test alpha/beta form
    std::vector<double> y1(N, 10.0);
    std::vector<double> y2(N, 10.0);

    A.matvec(2.0, x.data(), 0.5, y1.data(), true);
    A.matvec(2.0, x.data(), 0.5, y2.data(), false);

    for (std::size_t i = 0; i < N; ++i)
    {
        FATP_ASSERT_CLOSE(y1[i], y2[i], "Prefetch toggle should not affect alpha/beta correctness");
    }

    return true;
}

// =============================================================================
// Matrix Operations Tests
// =============================================================================

FATP_TEST_CASE(transpose_basic)
{
    using Matrix = fat_p::HpcCSRMatrix<double>;

    std::vector<int32_t> rows = {0, 0, 1, 2};
    std::vector<int32_t> cols = {0, 2, 1, 0};
    std::vector<double> vals = {1.0, 2.0, 3.0, 4.0};
    Matrix A(3, 3, rows, cols, vals);

    auto AT = A.transpose();

    FATP_ASSERT_CLOSE(AT(0, 0), 1.0, "AT(0,0) should be 1.0");
    FATP_ASSERT_CLOSE(AT(2, 0), 2.0, "AT(2,0) should be 2.0");
    FATP_ASSERT_CLOSE(AT(1, 1), 3.0, "AT(1,1) should be 3.0");
    FATP_ASSERT_CLOSE(AT(0, 2), 4.0, "AT(0,2) should be 4.0");
    FATP_ASSERT_EQ(AT.rows(), A.cols(), "Transposed rows should equal original cols");
    FATP_ASSERT_EQ(AT.cols(), A.rows(), "Transposed cols should equal original rows");

    return true;
}

FATP_TEST_CASE(transpose_empty)
{
    using Matrix = fat_p::HpcCSRMatrix<double>;

    Matrix E(3, 5);
    auto ET = E.transpose();

    FATP_ASSERT_EQ(ET.rows(), static_cast<std::size_t>(5), "Transposed rows");
    FATP_ASSERT_EQ(ET.cols(), static_cast<std::size_t>(3), "Transposed cols");
    FATP_ASSERT_EQ(ET.nnz(), static_cast<std::size_t>(0), "Transposed nnz");

    return true;
}

FATP_TEST_CASE(matmul_identity)
{
    using Matrix = fat_p::HpcCSRMatrix<double>;

    // Identity * A = A
    std::vector<int32_t> eye_rows = {0, 1, 2};
    std::vector<int32_t> eye_cols = {0, 1, 2};
    std::vector<double> eye_vals = {1.0, 1.0, 1.0};
    Matrix I(3, 3, eye_rows, eye_cols, eye_vals);

    std::vector<int32_t> a_rows = {0, 1, 2};
    std::vector<int32_t> a_cols = {1, 2, 0};
    std::vector<double> a_vals = {5.0, 7.0, 3.0};
    Matrix A(3, 3, a_rows, a_cols, a_vals);

    auto C = I.matmul(A);

    FATP_ASSERT_CLOSE(C(0, 1), 5.0, "C(0,1) should be 5.0");
    FATP_ASSERT_CLOSE(C(1, 2), 7.0, "C(1,2) should be 7.0");
    FATP_ASSERT_CLOSE(C(2, 0), 3.0, "C(2,0) should be 3.0");
    FATP_ASSERT_EQ(C.nnz(), A.nnz(), "I*A should have same nnz as A");

    return true;
}

FATP_TEST_CASE(addition_basic)
{
    using Matrix = fat_p::HpcCSRMatrix<double>;

    std::vector<int32_t> rows_a = {0, 1};
    std::vector<int32_t> cols_a = {0, 1};
    std::vector<double> vals_a = {1.0, 2.0};
    Matrix A(2, 2, rows_a, cols_a, vals_a);

    std::vector<int32_t> rows_b = {0, 1};
    std::vector<int32_t> cols_b = {1, 0};
    std::vector<double> vals_b = {3.0, 4.0};
    Matrix B(2, 2, rows_b, cols_b, vals_b);

    auto C = A + B;

    FATP_ASSERT_CLOSE(C(0, 0), 1.0, "C(0,0) should be 1.0");
    FATP_ASSERT_CLOSE(C(0, 1), 3.0, "C(0,1) should be 3.0");
    FATP_ASSERT_CLOSE(C(1, 0), 4.0, "C(1,0) should be 4.0");
    FATP_ASSERT_CLOSE(C(1, 1), 2.0, "C(1,1) should be 2.0");

    return true;
}

FATP_TEST_CASE(subtraction_basic)
{
    using Matrix = fat_p::HpcCSRMatrix<double>;

    std::vector<int32_t> rows = {0, 1};
    std::vector<int32_t> cols = {0, 1};
    std::vector<double> vals = {5.0, 7.0};
    Matrix A(2, 2, rows, cols, vals);

    auto C = A - A;

    FATP_ASSERT_EQ(C.nnz(), static_cast<std::size_t>(0), "A-A should have zero nnz");

    return true;
}

FATP_TEST_CASE(scalar_multiply)
{
    using Matrix = fat_p::HpcCSRMatrix<double>;

    std::vector<int32_t> rows = {0, 1};
    std::vector<int32_t> cols = {0, 1};
    std::vector<double> vals = {2.0, 3.0};
    Matrix A(2, 2, rows, cols, vals);

    auto B = A * 4.0;

    FATP_ASSERT_CLOSE(B(0, 0), 8.0, "B(0,0) should be 8.0");
    FATP_ASSERT_CLOSE(B(1, 1), 12.0, "B(1,1) should be 12.0");

    return true;
}

// =============================================================================
// HPC Feature Tests
// =============================================================================

FATP_TEST_CASE(numa_api)
{
    using Matrix = fat_p::HpcCSRMatrix<double>;

    Matrix A(10, 10);

    // Should compile and return a bool
    [[maybe_unused]] bool numa_status = A.is_numa_available();

    return true;
}

FATP_TEST_CASE(aligned_accessors)
{
    using Matrix = fat_p::HpcCSRMatrix<double>;

    std::vector<int32_t> rows = {0, 1};
    std::vector<int32_t> cols = {0, 1};
    std::vector<double> vals = {1.0, 2.0};
    Matrix A(2, 2, rows, cols, vals);
    const Matrix& A_const = A;

    // Non-const access
    double* v = A.values_aligned();
    int32_t* c = A.col_indices_aligned();
    FATP_ASSERT_TRUE(v != nullptr, "values_aligned() returned null");
    FATP_ASSERT_TRUE(c != nullptr, "col_indices_aligned() returned null");

    // Const access
    const double* vals_c = A_const.values_aligned();
    const int32_t* cols_c = A_const.col_indices_aligned();
    FATP_ASSERT_TRUE(vals_c != nullptr, "const values_aligned() returned null");
    FATP_ASSERT_TRUE(cols_c != nullptr, "const col_indices_aligned() returned null");

    return true;
}

FATP_TEST_CASE(from_dense)
{
    using Matrix = fat_p::HpcCSRMatrix<double>;

    std::vector<double> dense = {1.0, 0.0, 2.0, 0.0, 3.0, 0.0, 4.0, 0.0, 5.0};

    auto A = Matrix::from_dense(dense.data(), 3, 3);

    FATP_ASSERT_EQ(A.nnz(), static_cast<std::size_t>(5), "nnz should be 5");
    FATP_ASSERT_CLOSE(A(0, 0), 1.0, "A(0,0) should be 1.0");
    FATP_ASSERT_CLOSE(A(0, 2), 2.0, "A(0,2) should be 2.0");
    FATP_ASSERT_CLOSE(A(1, 1), 3.0, "A(1,1) should be 3.0");
    FATP_ASSERT_CLOSE(A(2, 0), 4.0, "A(2,0) should be 4.0");
    FATP_ASSERT_CLOSE(A(2, 2), 5.0, "A(2,2) should be 5.0");

    return true;
}

FATP_TEST_CASE(to_dense)
{
    using Matrix = fat_p::HpcCSRMatrix<double>;

    std::vector<int32_t> rows = {0, 1, 2};
    std::vector<int32_t> cols = {0, 1, 2};
    std::vector<double> vals = {1.0, 2.0, 3.0};
    Matrix A(3, 3, rows, cols, vals);

    auto dense = A.to_dense();

    FATP_ASSERT_EQ(dense.size(), static_cast<std::size_t>(9), "dense size should be 9");
    FATP_ASSERT_CLOSE(dense[0], 1.0, "dense[0] should be 1.0");
    FATP_ASSERT_CLOSE(dense[4], 2.0, "dense[4] should be 2.0");
    FATP_ASSERT_CLOSE(dense[8], 3.0, "dense[8] should be 3.0");

    return true;
}

FATP_TEST_CASE(density_sparsity)
{
    using Matrix = fat_p::HpcCSRMatrix<double>;

    std::vector<int32_t> rows = {0, 1};
    std::vector<int32_t> cols = {0, 1};
    std::vector<double> vals = {1.0, 2.0};
    Matrix A(4, 4, rows, cols, vals);

    FATP_ASSERT_CLOSE(A.density(), 2.0 / 16.0, "density should be 2/16");
    FATP_ASSERT_CLOSE(A.sparsity(), 14.0 / 16.0, "sparsity should be 14/16");

    return true;
}

// =============================================================================

} // namespace fat_p::testing::csrmatrix_hpc

namespace fat_p::testing
{

bool test_CSRMatrix_HPC()
{
    FATP_PRINT_HEADER(HpcCSRMatrix)

    auto& config = get_test_config();
    config.verbose = true;

    TestRunner runner;

    // Structural tests
    FATP_RUN_TEST_NS(runner, csrmatrix_hpc, is_symmetric_structural_check);
    FATP_RUN_TEST_NS(runner, csrmatrix_hpc, is_symmetric_diagonal);
    FATP_RUN_TEST_NS(runner, csrmatrix_hpc, is_symmetric_empty);

    // SpMV tests
    FATP_RUN_TEST_NS(runner, csrmatrix_hpc, matvec_basic);
    FATP_RUN_TEST_NS(runner, csrmatrix_hpc, matvec_alpha_beta);
    FATP_RUN_TEST_NS(runner, csrmatrix_hpc, matvec_empty_rows);
    FATP_RUN_TEST_NS(runner, csrmatrix_hpc, matvec_single_element);
    FATP_RUN_TEST_NS(runner, csrmatrix_hpc, matvec_prefetch_toggle);

    // Matrix operation tests
    FATP_RUN_TEST_NS(runner, csrmatrix_hpc, transpose_basic);
    FATP_RUN_TEST_NS(runner, csrmatrix_hpc, transpose_empty);
    FATP_RUN_TEST_NS(runner, csrmatrix_hpc, matmul_identity);
    FATP_RUN_TEST_NS(runner, csrmatrix_hpc, addition_basic);
    FATP_RUN_TEST_NS(runner, csrmatrix_hpc, subtraction_basic);
    FATP_RUN_TEST_NS(runner, csrmatrix_hpc, scalar_multiply);

    // HPC feature tests
    FATP_RUN_TEST_NS(runner, csrmatrix_hpc, numa_api);
    FATP_RUN_TEST_NS(runner, csrmatrix_hpc, aligned_accessors);
    FATP_RUN_TEST_NS(runner, csrmatrix_hpc, from_dense);
    FATP_RUN_TEST_NS(runner, csrmatrix_hpc, to_dense);
    FATP_RUN_TEST_NS(runner, csrmatrix_hpc, density_sparsity);

    // Benchmarks

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_CSRMatrix_HPC() ? 0 : 1;
}
#endif
