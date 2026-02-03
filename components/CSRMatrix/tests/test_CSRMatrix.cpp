/**
 * @file test_CSRMatrix.cpp
 * @brief Comprehensive tests for CSRMatrix sparse matrix implementation
 *
 * Test coverage includes:
 * - Construction (default, sized, COO, dense)
 * - Element access (operator(), set)
 * - Matrix operations (transpose, add, subtract, matmul)
 * - Matrix-vector multiplication (matvec, parallel)
 * - Utility functions (density, sparsity, is_symmetric, remove_zeros)
 * - Factory functions (identity_matrix, diagonal_matrix)
 * - Edge cases (empty, single element, large indices)
 * - Error handling (bounds, dimension mismatch, overflow)
 * - DuplicatePolicy semantics (Sum, Keep, Error)
 * - Equality operators and compound assignment
 */
/*
FATP_META:
  meta_version: 1
  component: CSRMatrix
  file_role: test
  path: components/CSRMatrix/tests/test_CSRMatrix.cpp
  layer: Testing
  namespace: fat_p
  summary: "Unit tests for CSRMatrix."
  api_stability: in_work
  related:
    docs_search: "CSRMatrix"
    headers:
      - include/fat_p/CSRMatrix.h
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

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <vector>

#include "CSRMatrix.h"
#include "FatPTest.h"

namespace fat_p::testing::csrmatrix
{

// ============================================================================
// Construction Tests
// ============================================================================

FATP_TEST_CASE(default_construction)
{
    CSRMatrix<double> mat;
    FATP_ASSERT_EQ(mat.rows(), 0u, "Default construction: rows should be 0");
    FATP_ASSERT_EQ(mat.cols(), 0u, "Default construction: cols should be 0");
    FATP_ASSERT_EQ(mat.nnz(), 0u, "Default construction: nnz should be 0");
    FATP_ASSERT_TRUE(mat.empty(), "Default construction: should be empty");
    return true;
}

FATP_TEST_CASE(sized_construction)
{
    CSRMatrix<double> mat(10, 20);
    FATP_ASSERT_EQ(mat.rows(), 10u, "Sized construction: rows should be 10");
    FATP_ASSERT_EQ(mat.cols(), 20u, "Sized construction: cols should be 20");
    FATP_ASSERT_EQ(mat.nnz(), 0u, "Sized construction: nnz should be 0");
    FATP_ASSERT_TRUE(mat.empty(), "Sized construction: should be empty");
    return true;
}

FATP_TEST_CASE(coo_construction_sum_policy)
{
    // 3x3 matrix with entries
    std::vector<int> rows = {0, 0, 1, 2};
    std::vector<int> cols = {0, 1, 1, 2};
    std::vector<double> vals = {1.0, 2.0, 3.0, 4.0};

    CSRMatrix<double, int> mat(3, 3, rows, cols, vals, CSRMatrix<double, int>::DuplicatePolicy::Sum);

    FATP_ASSERT_EQ(mat.rows(), 3u, "COO Sum: rows");
    FATP_ASSERT_EQ(mat.cols(), 3u, "COO Sum: cols");
    FATP_ASSERT_EQ(mat.nnz(), 4u, "COO Sum: nnz");
    FATP_ASSERT_CLOSE(mat(0, 0), 1.0, "COO Sum: (0,0)");
    FATP_ASSERT_CLOSE(mat(0, 1), 2.0, "COO Sum: (0,1)");
    FATP_ASSERT_CLOSE(mat(1, 1), 3.0, "COO Sum: (1,1)");
    FATP_ASSERT_CLOSE(mat(2, 2), 4.0, "COO Sum: (2,2)");
    return true;
}

FATP_TEST_CASE(coo_construction_sum_duplicates)
{
    // Duplicates at (0,1): 2.0 + 5.0 = 7.0
    std::vector<int> rows = {0, 0, 0, 1};
    std::vector<int> cols = {0, 1, 1, 1};
    std::vector<double> vals = {1.0, 2.0, 5.0, 3.0};

    CSRMatrix<double, int> mat(2, 2, rows, cols, vals, CSRMatrix<double, int>::DuplicatePolicy::Sum);

    FATP_ASSERT_EQ(mat.nnz(), 3u, "COO Sum duplicates: nnz after merge");
    FATP_ASSERT_CLOSE(mat(0, 1), 7.0, "COO Sum duplicates: summed value");
    return true;
}

FATP_TEST_CASE(coo_construction_keep_policy)
{
    // Keep all entries including duplicates
    std::vector<int> rows = {0, 0, 1};
    std::vector<int> cols = {1, 1, 0};
    std::vector<double> vals = {2.0, 3.0, 1.0};

    CSRMatrix<double, int> mat(2, 2, rows, cols, vals, CSRMatrix<double, int>::DuplicatePolicy::Keep);

    FATP_ASSERT_EQ(mat.nnz(), 3u, "COO Keep: all entries preserved");
    // operator() should return sum for consistency
    FATP_ASSERT_CLOSE(mat(0, 1), 5.0, "COO Keep: operator() returns sum");
    return true;
}

FATP_TEST_CASE(coo_construction_keep_filters_zeros)
{
    // Keep policy should still filter explicit zeros (sparse invariant)
    std::vector<int> rows = {0, 0, 1};
    std::vector<int> cols = {0, 1, 0};
    std::vector<double> vals = {1.0, 0.0, 2.0}; // Zero at (0,1)

    CSRMatrix<double, int> mat(2, 2, rows, cols, vals, CSRMatrix<double, int>::DuplicatePolicy::Keep);

    FATP_ASSERT_EQ(mat.nnz(), 2u, "COO Keep: zeros filtered");
    FATP_ASSERT_CLOSE(mat(0, 1), 0.0, "COO Keep: zero not stored");
    return true;
}

FATP_TEST_CASE(coo_construction_error_policy)
{
    std::vector<int> rows = {0, 0};
    std::vector<int> cols = {1, 1}; // Duplicate!
    std::vector<double> vals = {2.0, 3.0};

    FATP_ASSERT_THROWS((CSRMatrix<double, int>(2, 2, rows, cols, vals, CSRMatrix<double, int>::DuplicatePolicy::Error)),
                       std::invalid_argument,
                       "COO Error policy: should throw on duplicates");
    return true;
}

FATP_TEST_CASE(coo_construction_out_of_bounds)
{
    std::vector<int> rows = {0, 5}; // 5 is out of bounds for 3x3
    std::vector<int> cols = {0, 0};
    std::vector<double> vals = {1.0, 2.0};

    FATP_ASSERT_THROWS((CSRMatrix<double, int>(3, 3, rows, cols, vals)),
                       std::out_of_range,
                       "COO: should throw on out-of-bounds row");
    return true;
}

FATP_TEST_CASE(coo_construction_negative_index)
{
    std::vector<int> rows = {0, -1}; // Negative!
    std::vector<int> cols = {0, 0};
    std::vector<double> vals = {1.0, 2.0};

    FATP_ASSERT_THROWS((CSRMatrix<double, int>(3, 3, rows, cols, vals)),
                       std::out_of_range,
                       "COO: negative row index should throw");
    return true;
}

FATP_TEST_CASE(coo_construction_mismatched_sizes)
{
    std::vector<int> rows = {0, 1};
    std::vector<int> cols = {0}; // Size mismatch!
    std::vector<double> vals = {1.0, 2.0};

    FATP_ASSERT_THROWS((CSRMatrix<double, int>(3, 3, rows, cols, vals)),
                       std::invalid_argument,
                       "COO: should throw on mismatched array sizes");
    return true;
}

FATP_TEST_CASE(from_dense_construction)
{
    // Row-major dense: [[1, 0, 2], [0, 3, 0], [4, 0, 5]]
    std::vector<double> dense = {1.0, 0.0, 2.0, 0.0, 3.0, 0.0, 4.0, 0.0, 5.0};

    auto mat = CSRMatrix<double, int>::from_dense(dense.data(), 3, 3);

    FATP_ASSERT_EQ(mat.rows(), 3u, "from_dense: rows");
    FATP_ASSERT_EQ(mat.cols(), 3u, "from_dense: cols");
    FATP_ASSERT_EQ(mat.nnz(), 5u, "from_dense: nnz");
    FATP_ASSERT_CLOSE(mat(0, 0), 1.0, "from_dense: (0,0)");
    FATP_ASSERT_CLOSE(mat(0, 2), 2.0, "from_dense: (0,2)");
    FATP_ASSERT_CLOSE(mat(1, 1), 3.0, "from_dense: (1,1)");
    FATP_ASSERT_CLOSE(mat(2, 0), 4.0, "from_dense: (2,0)");
    FATP_ASSERT_CLOSE(mat(2, 2), 5.0, "from_dense: (2,2)");
    return true;
}

FATP_TEST_CASE(from_dense_null_pointer)
{
    FATP_ASSERT_THROWS((CSRMatrix<double, int>::from_dense(nullptr, 3, 3)),
                       std::invalid_argument,
                       "from_dense: should throw on null pointer with non-zero dimensions");

    // Null with zero dimensions should be OK
    auto mat = CSRMatrix<double, int>::from_dense(nullptr, 0, 0);
    FATP_ASSERT_TRUE(mat.empty(), "from_dense: null with zero dimensions is OK");
    return true;
}

// ============================================================================
// Element Access Tests
// ============================================================================

FATP_TEST_CASE(element_access_valid)
{
    std::vector<int> rows = {0, 1, 2};
    std::vector<int> cols = {0, 1, 2};
    std::vector<double> vals = {1.0, 2.0, 3.0};

    CSRMatrix<double, int> mat(3, 3, rows, cols, vals);

    FATP_ASSERT_CLOSE(mat(0, 0), 1.0, "Access: diagonal (0,0)");
    FATP_ASSERT_CLOSE(mat(1, 1), 2.0, "Access: diagonal (1,1)");
    FATP_ASSERT_CLOSE(mat(2, 2), 3.0, "Access: diagonal (2,2)");
    FATP_ASSERT_CLOSE(mat(0, 1), 0.0, "Access: zero element");
    return true;
}

FATP_TEST_CASE(element_access_out_of_bounds)
{
    CSRMatrix<double, int> mat(3, 3);

    FATP_ASSERT_THROWS(mat(3, 0), std::out_of_range, "Access: row out of bounds");
    FATP_ASSERT_THROWS(mat(0, 3), std::out_of_range, "Access: col out of bounds");
    return true;
}

FATP_TEST_CASE(element_set)
{
    CSRMatrix<double, int> mat(3, 3);

    mat.set(0, 0, 1.0);
    mat.set(1, 2, 2.0);
    mat.set(2, 1, 3.0);

    FATP_ASSERT_EQ(mat.nnz(), 3u, "Set: nnz after insertions");
    FATP_ASSERT_CLOSE(mat(0, 0), 1.0, "Set: value at (0,0)");
    FATP_ASSERT_CLOSE(mat(1, 2), 2.0, "Set: value at (1,2)");
    FATP_ASSERT_CLOSE(mat(2, 1), 3.0, "Set: value at (2,1)");

    // Update existing
    mat.set(0, 0, 5.0);
    FATP_ASSERT_CLOSE(mat(0, 0), 5.0, "Set: updated value");
    FATP_ASSERT_EQ(mat.nnz(), 3u, "Set: nnz unchanged after update");

    // Set to zero removes entry
    mat.set(0, 0, 0.0);
    FATP_ASSERT_EQ(mat.nnz(), 2u, "Set: nnz decreased after zero");
    FATP_ASSERT_CLOSE(mat(0, 0), 0.0, "Set: value is now zero");
    return true;
}

// ============================================================================
// DuplicatePolicy::Keep Semantics Tests
// ============================================================================

FATP_TEST_CASE(keep_policy_operator_consistency)
{
    // This test verifies the fix for operator() + Keep inconsistency
    // operator() should return the SUM of all entries, matching matvec/to_dense

    std::vector<int> rows = {0, 0, 0};
    std::vector<int> cols = {1, 1, 1}; // Three entries at (0,1)
    std::vector<double> vals = {2.0, 3.0, 4.0};

    CSRMatrix<double, int> mat(1, 3, rows, cols, vals, CSRMatrix<double, int>::DuplicatePolicy::Keep);

    FATP_ASSERT_EQ(mat.nnz(), 3u, "Keep: all entries stored");

    // operator() should sum duplicates
    double accessed = mat(0, 1);
    FATP_ASSERT_CLOSE(accessed, 9.0, "Keep: operator() returns sum (2+3+4=9)");

    // to_dense should also sum
    auto dense = mat.to_dense();
    FATP_ASSERT_CLOSE(dense[1], 9.0, "Keep: to_dense returns sum");

    // matvec should use summed value
    std::vector<double> x = {1.0, 1.0, 1.0};
    std::vector<double> y = mat * x;
    FATP_ASSERT_CLOSE(y[0], 9.0, "Keep: matvec uses summed value");
    return true;
}

FATP_TEST_CASE(keep_policy_set_collapses_duplicates)
{
    // set() should remove all existing entries at a position
    std::vector<int> rows = {0, 0};
    std::vector<int> cols = {1, 1};
    std::vector<double> vals = {2.0, 3.0};

    CSRMatrix<double, int> mat(1, 2, rows, cols, vals, CSRMatrix<double, int>::DuplicatePolicy::Keep);

    FATP_ASSERT_EQ(mat.nnz(), 2u, "Keep: initial duplicates");

    // set should collapse to single entry
    mat.set(0, 1, 10.0);
    FATP_ASSERT_EQ(mat.nnz(), 1u, "Keep: set collapses duplicates");
    FATP_ASSERT_CLOSE(mat(0, 1), 10.0, "Keep: new value after set");
    return true;
}

// ============================================================================
// Matrix Operations Tests
// ============================================================================

FATP_TEST_CASE(transpose_basic)
{
    // [[1, 2], [3, 4]] -> [[1, 3], [2, 4]]
    std::vector<double> dense = {1.0, 2.0, 3.0, 4.0};
    auto mat = CSRMatrix<double, int>::from_dense(dense.data(), 2, 2);

    auto transposed = mat.transpose();

    FATP_ASSERT_EQ(transposed.rows(), 2u, "Transpose: rows");
    FATP_ASSERT_EQ(transposed.cols(), 2u, "Transpose: cols");
    FATP_ASSERT_CLOSE(transposed(0, 0), 1.0, "Transpose: (0,0)");
    FATP_ASSERT_CLOSE(transposed(0, 1), 3.0, "Transpose: (0,1)");
    FATP_ASSERT_CLOSE(transposed(1, 0), 2.0, "Transpose: (1,0)");
    FATP_ASSERT_CLOSE(transposed(1, 1), 4.0, "Transpose: (1,1)");
    return true;
}

FATP_TEST_CASE(transpose_rectangular)
{
    // 2x3 -> 3x2
    std::vector<double> dense = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};
    auto mat = CSRMatrix<double, int>::from_dense(dense.data(), 2, 3);

    auto transposed = mat.transpose();

    FATP_ASSERT_EQ(transposed.rows(), 3u, "Transpose rect: rows");
    FATP_ASSERT_EQ(transposed.cols(), 2u, "Transpose rect: cols");
    return true;
}

FATP_TEST_CASE(matrix_addition)
{
    std::vector<int> rows1 = {0, 1};
    std::vector<int> cols1 = {0, 1};
    std::vector<double> vals1 = {1.0, 2.0};

    std::vector<int> rows2 = {0, 1};
    std::vector<int> cols2 = {1, 1};
    std::vector<double> vals2 = {3.0, 4.0};

    CSRMatrix<double, int> A(2, 2, rows1, cols1, vals1);
    CSRMatrix<double, int> B(2, 2, rows2, cols2, vals2);

    auto C = A + B;

    FATP_ASSERT_CLOSE(C(0, 0), 1.0, "Add: (0,0)");
    FATP_ASSERT_CLOSE(C(0, 1), 3.0, "Add: (0,1)");
    FATP_ASSERT_CLOSE(C(1, 1), 6.0, "Add: (1,1) = 2+4");
    return true;
}

FATP_TEST_CASE(matrix_addition_with_keep_duplicates)
{
    // Test that operator+ correctly consolidates duplicates from Keep policy
    std::vector<int> a_rows = {0, 0};
    std::vector<int> a_cols = {1, 1};
    std::vector<double> a_vals = {2.0, 3.0};
    CSRMatrix<double, int> A(1, 2, a_rows, a_cols, a_vals, CSRMatrix<double, int>::DuplicatePolicy::Keep);

    std::vector<int> b_rows = {0};
    std::vector<int> b_cols = {1};
    std::vector<double> b_vals = {4.0};
    CSRMatrix<double, int> B(1, 2, b_rows, b_cols, b_vals);

    auto C = A + B;

    // Mathematical correctness: 2+3+4 = 9
    FATP_ASSERT_CLOSE(C(0, 1), 9.0, "Add Keep: 2+3+4=9");
    // Structural correctness: result should have unique columns
    FATP_ASSERT_EQ(C.nnz(), 1u, "Add Keep: result consolidated to unique columns");
    return true;
}

FATP_TEST_CASE(matrix_subtraction)
{
    std::vector<int> rows = {0, 1};
    std::vector<int> cols = {0, 1};
    std::vector<double> vals1 = {5.0, 8.0};
    std::vector<double> vals2 = {2.0, 3.0};

    CSRMatrix<double, int> A(2, 2, rows, cols, vals1);
    CSRMatrix<double, int> B(2, 2, rows, cols, vals2);

    auto C = A - B;

    FATP_ASSERT_CLOSE(C(0, 0), 3.0, "Sub: (0,0)");
    FATP_ASSERT_CLOSE(C(1, 1), 5.0, "Sub: (1,1)");
    return true;
}

FATP_TEST_CASE(scalar_multiplication)
{
    std::vector<int> rows = {0, 1};
    std::vector<int> cols = {0, 1};
    std::vector<double> vals = {2.0, 3.0};

    CSRMatrix<double, int> mat(2, 2, rows, cols, vals);

    auto scaled = mat * 2.5;

    FATP_ASSERT_CLOSE(scaled(0, 0), 5.0, "Scale: (0,0)");
    FATP_ASSERT_CLOSE(scaled(1, 1), 7.5, "Scale: (1,1)");

    // Test commutative form
    auto scaled2 = 2.5 * mat;
    FATP_ASSERT_CLOSE(scaled2(0, 0), 5.0, "Scale commutative: (0,0)");
    return true;
}

FATP_TEST_CASE(scalar_multiplication_by_zero)
{
    std::vector<int> rows = {0, 1};
    std::vector<int> cols = {0, 1};
    std::vector<double> vals = {2.0, 3.0};

    CSRMatrix<double, int> mat(2, 2, rows, cols, vals);
    FATP_ASSERT_EQ(mat.nnz(), 2u, "Before zero scale: nnz");

    // Multiply by zero should yield empty sparse matrix (sparse invariant)
    auto zero_scaled = mat * 0.0;
    FATP_ASSERT_EQ(zero_scaled.nnz(), 0u, "Scale by zero: nnz should be 0");
    FATP_ASSERT_EQ(zero_scaled.rows(), 2u, "Scale by zero: rows preserved");
    FATP_ASSERT_EQ(zero_scaled.cols(), 2u, "Scale by zero: cols preserved");
    FATP_ASSERT_CLOSE(zero_scaled(0, 0), 0.0, "Scale by zero: value is zero");

    // Test in-place version
    CSRMatrix<double, int> mat2(2, 2, rows, cols, vals);
    mat2 *= 0.0;
    FATP_ASSERT_EQ(mat2.nnz(), 0u, "*= zero: nnz should be 0");
    FATP_ASSERT_EQ(mat2.rows(), 2u, "*= zero: rows preserved");
    FATP_ASSERT_EQ(mat2.cols(), 2u, "*= zero: cols preserved");

    // Test commutative form
    CSRMatrix<double, int> mat3(2, 2, rows, cols, vals);
    auto zero_scaled2 = 0.0 * mat3;
    FATP_ASSERT_EQ(zero_scaled2.nnz(), 0u, "0 * mat: nnz should be 0");
    return true;
}

FATP_TEST_CASE(compound_assignment_operators)
{
    std::vector<int> rows = {0, 1};
    std::vector<int> cols = {0, 1};
    std::vector<double> vals = {2.0, 3.0};

    CSRMatrix<double, int> A(2, 2, rows, cols, vals);
    CSRMatrix<double, int> B(2, 2, rows, cols, vals);

    // Test operator*=
    A *= 2.0;
    FATP_ASSERT_CLOSE(A(0, 0), 4.0, "*=: (0,0)");
    FATP_ASSERT_CLOSE(A(1, 1), 6.0, "*=: (1,1)");

    // Test operator+=
    A += B;
    FATP_ASSERT_CLOSE(A(0, 0), 6.0, "+=: (0,0) = 4+2");
    FATP_ASSERT_CLOSE(A(1, 1), 9.0, "+=: (1,1) = 6+3");

    // Test operator-=
    A -= B;
    FATP_ASSERT_CLOSE(A(0, 0), 4.0, "-=: (0,0) = 6-2");
    FATP_ASSERT_CLOSE(A(1, 1), 6.0, "-=: (1,1) = 9-3");
    return true;
}

FATP_TEST_CASE(matmul_basic)
{
    // A = [[1, 2], [3, 4]], B = [[5, 6], [7, 8]]
    // C = A*B = [[19, 22], [43, 50]]
    std::vector<double> denseA = {1.0, 2.0, 3.0, 4.0};
    std::vector<double> denseB = {5.0, 6.0, 7.0, 8.0};

    auto A = CSRMatrix<double, int>::from_dense(denseA.data(), 2, 2);
    auto B = CSRMatrix<double, int>::from_dense(denseB.data(), 2, 2);

    auto C = A.matmul(B);

    FATP_ASSERT_CLOSE(C(0, 0), 19.0, "Matmul: (0,0)");
    FATP_ASSERT_CLOSE(C(0, 1), 22.0, "Matmul: (0,1)");
    FATP_ASSERT_CLOSE(C(1, 0), 43.0, "Matmul: (1,0)");
    FATP_ASSERT_CLOSE(C(1, 1), 50.0, "Matmul: (1,1)");
    return true;
}

FATP_TEST_CASE(matmul_rectangular)
{
    // A = 2x3, B = 3x2 -> C = 2x2
    std::vector<double> denseA = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};
    std::vector<double> denseB = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};

    auto A = CSRMatrix<double, int>::from_dense(denseA.data(), 2, 3);
    auto B = CSRMatrix<double, int>::from_dense(denseB.data(), 3, 2);

    auto C = A.matmul(B);

    FATP_ASSERT_EQ(C.rows(), 2u, "Matmul rect: rows");
    FATP_ASSERT_EQ(C.cols(), 2u, "Matmul rect: cols");
    // C[0,0] = 1*1 + 2*3 + 3*5 = 22
    FATP_ASSERT_CLOSE(C(0, 0), 22.0, "Matmul rect: (0,0)");
    return true;
}

FATP_TEST_CASE(matmul_size_mismatch)
{
    // A = 2x3, B = 2x2 -> incompatible (3 != 2)
    CSRMatrix<double, int> A(2, 3);
    CSRMatrix<double, int> B(2, 2);

    FATP_ASSERT_THROWS(A.matmul(B), std::invalid_argument, "Matmul: should throw on dimension mismatch");
    return true;
}

FATP_TEST_CASE(matmul_index_overflow)
{
    // Regression test for marker overflow bug
    // When using small IndexType (int8_t), rows > 127 would overflow the marker
    // The fix uses size_type for the marker vector

    constexpr std::size_t dim = 150; // > int8_t max (127)

    // Create sparse identity-like matrices
    std::vector<int8_t> rows, cols;
    std::vector<double> vals;
    rows.reserve(dim);
    cols.reserve(dim);
    vals.reserve(dim);

    for (std::size_t i = 0; i < dim; ++i)
    {
        if (i <= 127)
        {
            rows.push_back(static_cast<int8_t>(i));
            cols.push_back(static_cast<int8_t>(i));
            vals.push_back(1.0);
        }
    }

    // The dimension validation should throw because dim > int8_t::max
    FATP_ASSERT_THROWS((CSRMatrix<double, int8_t>(dim, dim, rows, cols, vals)),
                       std::overflow_error,
                       "Matmul overflow: dimension validation catches int8_t overflow");
    return true;
}

FATP_TEST_CASE(matmul_large_index_type)
{
    // Test with int64_t IndexType for large matrices
    std::vector<int64_t> rows = {0, 1, 2};
    std::vector<int64_t> cols = {0, 1, 2};
    std::vector<double> vals = {1.0, 1.0, 1.0};

    CSRMatrix<double, int64_t> A(3, 3, rows, cols, vals);
    auto C = A.matmul(A);

    FATP_ASSERT_EQ(C.nnz(), 3u, "Matmul int64: identity * identity = identity");
    return true;
}

FATP_TEST_CASE(matmul_accumulator_reset)
{
    // Verify accumulator is properly reset between rows
    // Row0 touches column1, Row1 touches nothing
    std::vector<double> denseA = {0.0, 1.0, 0.0, 0.0};
    std::vector<double> denseB = {1.0, 0.0, 0.0, 1.0};

    auto A = CSRMatrix<double, int>::from_dense(denseA.data(), 2, 2);
    auto B = CSRMatrix<double, int>::from_dense(denseB.data(), 2, 2);

    auto C = A.matmul(B);

    FATP_ASSERT_CLOSE(C(0, 1), 1.0, "Matmul accumulator: (0,1)");
    FATP_ASSERT_CLOSE(C(1, 1), 0.0, "Matmul accumulator: (1,1) should be 0");
    return true;
}

// ============================================================================
// Matrix-Vector Multiplication Tests
// ============================================================================

FATP_TEST_CASE(matvec_basic)
{
    // [[1, 2], [3, 4]] * [1, 2] = [5, 11]
    std::vector<double> dense = {1.0, 2.0, 3.0, 4.0};
    auto mat = CSRMatrix<double, int>::from_dense(dense.data(), 2, 2);

    std::vector<double> x = {1.0, 2.0};
    auto y = mat * x;

    FATP_ASSERT_CLOSE(y[0], 5.0, "Matvec: y[0]");
    FATP_ASSERT_CLOSE(y[1], 11.0, "Matvec: y[1]");
    return true;
}

FATP_TEST_CASE(matvec_alpha_beta)
{
    std::vector<double> dense = {1.0, 2.0, 3.0, 4.0};
    auto mat = CSRMatrix<double, int>::from_dense(dense.data(), 2, 2);

    std::vector<double> x = {1.0, 2.0};
    std::vector<double> y = {10.0, 20.0};

    // y = 2 * A*x + 3 * y = 2*[5, 11] + 3*[10, 20] = [40, 82]
    mat.matvec(2.0, x.data(), 3.0, y.data());

    FATP_ASSERT_CLOSE(y[0], 40.0, "Matvec alpha-beta: y[0]");
    FATP_ASSERT_CLOSE(y[1], 82.0, "Matvec alpha-beta: y[1]");
    return true;
}

FATP_TEST_CASE(matvec_alpha_beta_vector_overload)
{
    std::vector<double> dense = {1.0, 2.0, 3.0, 4.0};
    auto mat = CSRMatrix<double, int>::from_dense(dense.data(), 2, 2);

    std::vector<double> x = {1.0, 2.0};
    std::vector<double> y = {10.0, 20.0};

    // Test std::vector overload
    mat.matvec(2.0, x, 3.0, y);

    FATP_ASSERT_CLOSE(y[0], 40.0, "Matvec vector overload: y[0]");
    FATP_ASSERT_CLOSE(y[1], 82.0, "Matvec vector overload: y[1]");
    return true;
}

FATP_TEST_CASE(matvec_beta_zero_nan_safe)
{
    // When beta=0, uninitialized y should not propagate NaN
    std::vector<double> dense = {1.0, 0.0, 0.0, 1.0};
    auto mat = CSRMatrix<double, int>::from_dense(dense.data(), 2, 2);

    std::vector<double> x = {2.0, 3.0};
    std::vector<double> y = {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN()};

    mat.matvec(1.0, x.data(), 0.0, y.data());

    FATP_ASSERT_FALSE(std::isnan(y[0]), "Matvec beta=0: y[0] not NaN");
    FATP_ASSERT_FALSE(std::isnan(y[1]), "Matvec beta=0: y[1] not NaN");
    FATP_ASSERT_CLOSE(y[0], 2.0, "Matvec beta=0: y[0] value");
    FATP_ASSERT_CLOSE(y[1], 3.0, "Matvec beta=0: y[1] value");
    return true;
}

FATP_TEST_CASE(matvec_size_mismatch)
{
    CSRMatrix<double, int> mat(2, 3);   // 2x3 matrix
    std::vector<double> x = {1.0, 2.0}; // Size 2, need size 3

    FATP_ASSERT_THROWS(mat * x, std::invalid_argument, "Matvec: should throw on vector size mismatch");
    return true;
}

FATP_TEST_CASE(matvec_parallel_matches_serial)
{
    std::vector<double> dense = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0};
    auto mat = CSRMatrix<double, int>::from_dense(dense.data(), 3, 3);

    std::vector<double> x = {1.0, 2.0, 3.0};
    std::vector<double> y_serial(3);
    std::vector<double> y_parallel(3);

    mat.matvec(x.data(), y_serial.data());
    mat.matvec_parallel(x.data(), y_parallel.data());

    FATP_ASSERT_CLOSE(y_parallel[0], y_serial[0], "Parallel matvec: matches serial [0]");
    FATP_ASSERT_CLOSE(y_parallel[1], y_serial[1], "Parallel matvec: matches serial [1]");
    FATP_ASSERT_CLOSE(y_parallel[2], y_serial[2], "Parallel matvec: matches serial [2]");
    return true;
}

FATP_TEST_CASE(matvec_parallel_alpha_beta)
{
    std::vector<double> dense = {1.0, 2.0, 3.0, 4.0};
    auto mat = CSRMatrix<double, int>::from_dense(dense.data(), 2, 2);

    std::vector<double> x = {1.0, 2.0};
    std::vector<double> y_serial = {10.0, 20.0};
    std::vector<double> y_parallel = {10.0, 20.0};

    mat.matvec(2.0, x.data(), 3.0, y_serial.data());
    mat.matvec_parallel(2.0, x.data(), 3.0, y_parallel.data());

    FATP_ASSERT_CLOSE(y_parallel[0], y_serial[0], "Parallel alpha-beta: matches serial [0]");
    FATP_ASSERT_CLOSE(y_parallel[1], y_serial[1], "Parallel alpha-beta: matches serial [1]");
    return true;
}

// ============================================================================
// Equality Operator Tests
// ============================================================================

FATP_TEST_CASE(equality_operators)
{
    std::vector<int> rows = {0, 1};
    std::vector<int> cols = {0, 1};
    std::vector<double> vals = {1.0, 2.0};

    CSRMatrix<double, int> A(2, 2, rows, cols, vals);
    CSRMatrix<double, int> B(2, 2, rows, cols, vals);
    CSRMatrix<double, int> C(2, 2, rows, cols, {1.0, 3.0}); // Different values

    FATP_ASSERT_TRUE(A == B, "Equality: identical matrices");
    FATP_ASSERT_FALSE(A != B, "Inequality: identical matrices");
    FATP_ASSERT_FALSE(A == C, "Equality: different values");
    FATP_ASSERT_TRUE(A != C, "Inequality: different values");

    CSRMatrix<double, int> D(3, 3); // Different dimensions
    FATP_ASSERT_FALSE(A == D, "Equality: different dimensions");
    return true;
}

FATP_TEST_CASE(approximate_equality)
{
    std::vector<int> rows = {0, 1};
    std::vector<int> cols = {0, 1};
    std::vector<double> vals1 = {1.0, 2.0};
    std::vector<double> vals2 = {1.0 + 1e-12, 2.0 - 1e-12};

    CSRMatrix<double, int> A(2, 2, rows, cols, vals1);
    CSRMatrix<double, int> B(2, 2, rows, cols, vals2);

    FATP_ASSERT_FALSE(A == B, "Exact equality: tiny differences matter");
    FATP_ASSERT_TRUE(A.equals(B, 1e-10), "Approximate equality: within epsilon");
    FATP_ASSERT_FALSE(A.equals(B, 1e-14), "Approximate equality: outside epsilon");
    return true;
}

// ============================================================================
// Utility Function Tests
// ============================================================================

FATP_TEST_CASE(density_sparsity)
{
    std::vector<int> rows = {0, 1, 2, 3};
    std::vector<int> cols = {0, 1, 2, 3};
    std::vector<double> vals = {1.0, 2.0, 3.0, 4.0};

    CSRMatrix<double, int> mat(4, 4, rows, cols, vals);

    // 4 non-zeros in 16 elements = 0.25 density
    FATP_ASSERT_CLOSE(mat.density(), 0.25, "Density: 4/16");
    FATP_ASSERT_CLOSE(mat.sparsity(), 0.75, "Sparsity: 12/16");
    return true;
}

FATP_TEST_CASE(is_symmetric)
{
    // Symmetric: [[1, 2], [2, 3]]
    std::vector<int> rows = {0, 0, 1, 1};
    std::vector<int> cols = {0, 1, 0, 1};
    std::vector<double> vals = {1.0, 2.0, 2.0, 3.0};

    CSRMatrix<double, int> symmetric(2, 2, rows, cols, vals);
    FATP_ASSERT_TRUE(symmetric.is_symmetric(), "is_symmetric: symmetric matrix");

    // Non-symmetric: [[1, 2], [3, 4]]
    std::vector<double> vals2 = {1.0, 2.0, 3.0, 4.0};
    CSRMatrix<double, int> nonsymmetric(2, 2, rows, cols, vals2);
    FATP_ASSERT_FALSE(nonsymmetric.is_symmetric(), "is_symmetric: non-symmetric matrix");

    // Rectangular cannot be symmetric
    CSRMatrix<double, int> rect(2, 3);
    FATP_ASSERT_FALSE(rect.is_symmetric(), "is_symmetric: rectangular");
    return true;
}

FATP_TEST_CASE(remove_zeros)
{
    std::vector<int> rows = {0, 0, 1};
    std::vector<int> cols = {0, 1, 1};
    std::vector<double> vals = {1.0, 2.0, 3.0};

    CSRMatrix<double, int> mat(2, 2, rows, cols, vals);

    // Set a value that's above is_effectively_zero threshold but below remove_zeros epsilon
    // is_effectively_zero uses epsilon*10 ≈ 2.2e-15 for double
    // So 1e-12 is stored but considered "removable" with epsilon=1e-10
    mat.set(0, 0, 1e-12);
    FATP_ASSERT_EQ(mat.nnz(), 3u, "Before remove_zeros");

    mat.remove_zeros(1e-10); // Epsilon larger than 1e-12
    FATP_ASSERT_EQ(mat.nnz(), 2u, "After remove_zeros");
    return true;
}

FATP_TEST_CASE(row_nnz)
{
    std::vector<int> rows = {0, 0, 0, 1, 2, 2};
    std::vector<int> cols = {0, 1, 2, 0, 0, 2};
    std::vector<double> vals = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};

    CSRMatrix<double, int> mat(3, 3, rows, cols, vals);

    FATP_ASSERT_EQ(mat.row_nnz(0), 3u, "row_nnz: row 0 has 3");
    FATP_ASSERT_EQ(mat.row_nnz(1), 1u, "row_nnz: row 1 has 1");
    FATP_ASSERT_EQ(mat.row_nnz(2), 2u, "row_nnz: row 2 has 2");

    FATP_ASSERT_THROWS(mat.row_nnz(3), std::out_of_range, "row_nnz: out of bounds");
    return true;
}

FATP_TEST_CASE(to_dense)
{
    std::vector<int> rows = {0, 1, 2};
    std::vector<int> cols = {2, 0, 1};
    std::vector<double> vals = {1.0, 2.0, 3.0};

    CSRMatrix<double, int> mat(3, 3, rows, cols, vals);
    auto dense = mat.to_dense();

    // Row-major: [[0, 0, 1], [2, 0, 0], [0, 3, 0]]
    FATP_ASSERT_EQ(dense.size(), 9u, "to_dense: size");
    FATP_ASSERT_CLOSE(dense[0 * 3 + 2], 1.0, "to_dense: (0,2)");
    FATP_ASSERT_CLOSE(dense[1 * 3 + 0], 2.0, "to_dense: (1,0)");
    FATP_ASSERT_CLOSE(dense[2 * 3 + 1], 3.0, "to_dense: (2,1)");
    FATP_ASSERT_CLOSE(dense[0 * 3 + 0], 0.0, "to_dense: zeros");
    return true;
}

FATP_TEST_CASE(to_dense_keep_duplicates)
{
    // Verify to_dense properly sums Keep-policy duplicates
    std::vector<int> rows = {0, 0, 0};
    std::vector<int> cols = {1, 1, 1};
    std::vector<double> vals = {2.0, 3.0, 4.0};

    CSRMatrix<double, int> mat(1, 3, rows, cols, vals, CSRMatrix<double, int>::DuplicatePolicy::Keep);

    auto dense = mat.to_dense();
    FATP_ASSERT_CLOSE(dense[1], 9.0, "to_dense Keep: sums duplicates (2+3+4=9)");
    return true;
}

FATP_TEST_CASE(shrink_to_fit)
{
    CSRMatrix<double, int> mat(10, 10);

    // Add and remove elements to create excess capacity
    for (int i = 0; i < 10; ++i)
    {
        mat.set(i, i, static_cast<double>(i + 1));
    }
    for (int i = 5; i < 10; ++i)
    {
        mat.set(i, i, 0.0);
    }

    FATP_ASSERT_EQ(mat.nnz(), 5u, "Before shrink: nnz");
    mat.shrink_to_fit();
    FATP_ASSERT_EQ(mat.nnz(), 5u, "After shrink: nnz unchanged");
    // Can't easily test capacity reduction, but at least verify it compiles
    return true;
}

// ============================================================================
// Factory Function Tests
// ============================================================================

FATP_TEST_CASE(identity_matrix)
{
    auto I = identity_matrix<double, int>(4);

    FATP_ASSERT_EQ(I.rows(), 4u, "Identity: rows");
    FATP_ASSERT_EQ(I.cols(), 4u, "Identity: cols");
    FATP_ASSERT_EQ(I.nnz(), 4u, "Identity: nnz");

    for (std::size_t i = 0; i < 4; ++i)
    {
        FATP_ASSERT_CLOSE(I(i, i), 1.0, "Identity: diagonal");
        if (i > 0)
        {
            FATP_ASSERT_CLOSE(I(i, i - 1), 0.0, "Identity: off-diagonal");
        }
    }
    return true;
}

FATP_TEST_CASE(diagonal_matrix)
{
    std::vector<double> diag = {1.0, 2.0, 3.0, 4.0};
    auto D = diagonal_matrix<double, int>(diag);

    FATP_ASSERT_EQ(D.rows(), 4u, "Diagonal: rows");
    FATP_ASSERT_EQ(D.cols(), 4u, "Diagonal: cols");
    FATP_ASSERT_EQ(D.nnz(), 4u, "Diagonal: nnz");

    FATP_ASSERT_CLOSE(D(0, 0), 1.0, "Diagonal: (0,0)");
    FATP_ASSERT_CLOSE(D(1, 1), 2.0, "Diagonal: (1,1)");
    FATP_ASSERT_CLOSE(D(2, 2), 3.0, "Diagonal: (2,2)");
    FATP_ASSERT_CLOSE(D(3, 3), 4.0, "Diagonal: (3,3)");
    return true;
}

// ============================================================================
// Edge Case Tests
// ============================================================================

FATP_TEST_CASE(empty_matrix)
{
    CSRMatrix<double> mat;

    FATP_ASSERT_TRUE(mat.empty(), "Empty: is empty");
    FATP_ASSERT_EQ(mat.nnz(), 0u, "Empty: nnz");

    auto transposed = mat.transpose();
    FATP_ASSERT_TRUE(transposed.empty(), "Empty: transpose empty");

    auto dense = mat.to_dense();
    FATP_ASSERT_TRUE(dense.empty(), "Empty: to_dense empty");
    return true;
}

FATP_TEST_CASE(single_element)
{
    std::vector<int> rows = {0};
    std::vector<int> cols = {0};
    std::vector<double> vals = {42.0};

    CSRMatrix<double, int> mat(1, 1, rows, cols, vals);

    FATP_ASSERT_EQ(mat.nnz(), 1u, "Single: nnz");
    FATP_ASSERT_CLOSE(mat(0, 0), 42.0, "Single: value");

    auto transposed = mat.transpose();
    FATP_ASSERT_CLOSE(transposed(0, 0), 42.0, "Single: transpose");
    return true;
}

FATP_TEST_CASE(empty_rows)
{
    // Matrix with row 1 empty: [[1, 0], [0, 0], [0, 2]]
    std::vector<int> rows = {0, 2};
    std::vector<int> cols = {0, 1};
    std::vector<double> vals = {1.0, 2.0};

    CSRMatrix<double, int> mat(3, 2, rows, cols, vals);

    FATP_ASSERT_EQ(mat.row_nnz(0), 1u, "Empty rows: row 0 nnz");
    FATP_ASSERT_EQ(mat.row_nnz(1), 0u, "Empty rows: row 1 empty");
    FATP_ASSERT_EQ(mat.row_nnz(2), 1u, "Empty rows: row 2 nnz");

    auto dense = mat.to_dense();
    FATP_ASSERT_CLOSE(dense[1 * 2 + 0], 0.0, "Empty rows: (1,0) is zero");
    FATP_ASSERT_CLOSE(dense[1 * 2 + 1], 0.0, "Empty rows: (1,1) is zero");
    return true;
}

FATP_TEST_CASE(row_iteration)
{
    std::vector<int> rows = {0, 0, 1, 2, 2};
    std::vector<int> cols = {1, 3, 2, 0, 4};
    std::vector<double> vals = {1.0, 2.0, 3.0, 4.0, 5.0};

    CSRMatrix<double, int> mat(3, 5, rows, cols, vals);

    auto row0 = mat.row(0);
    FATP_ASSERT_EQ(row0.size(), 2u, "Row iter: row 0 size");

    std::vector<std::pair<int, double>> entries;
    for (auto [col, val] : row0)
    {
        entries.emplace_back(col, val);
    }

    FATP_ASSERT_EQ(entries.size(), 2u, "Row iter: collected entries");
    FATP_ASSERT_EQ(entries[0].first, 1, "Row iter: first col");
    FATP_ASSERT_CLOSE(entries[0].second, 1.0, "Row iter: first val");
    FATP_ASSERT_EQ(entries[1].first, 3, "Row iter: second col");
    FATP_ASSERT_CLOSE(entries[1].second, 2.0, "Row iter: second val");

    // Test operator[]
    auto entry = row0[0];
    FATP_ASSERT_EQ(entry.first, 1, "Row iter operator[]: col");
    FATP_ASSERT_CLOSE(entry.second, 1.0, "Row iter operator[]: val");
    return true;
}

FATP_TEST_CASE(ostream_output)
{
    std::vector<int> rows = {0, 1};
    std::vector<int> cols = {0, 1};
    std::vector<double> vals = {1.0, 2.0};

    CSRMatrix<double, int> mat(3, 4, rows, cols, vals);

    std::ostringstream oss;
    oss << mat;

    std::string output = oss.str();
    FATP_ASSERT_TRUE(output.find("3x4") != std::string::npos, "ostream: contains dimensions");
    FATP_ASSERT_TRUE(output.find("nnz=2") != std::string::npos, "ostream: contains nnz");
    return true;
}

// ============================================================================
// Type Safety Tests
// ============================================================================

FATP_TEST_CASE(dimension_overflow_validation)
{
    // Test that constructors validate dimensions against IndexType limits
    // Using int8_t as IndexType, max value is 127

    // Should throw: 200 > 127
    FATP_ASSERT_THROWS((CSRMatrix<double, int8_t>(200, 10)),
                       std::overflow_error,
                       "Dimension overflow: rows exceed int8_t");

    FATP_ASSERT_THROWS((CSRMatrix<double, int8_t>(10, 200)),
                       std::overflow_error,
                       "Dimension overflow: cols exceed int8_t");

    // Should succeed: 100 <= 127
    CSRMatrix<double, int8_t> small_mat(100, 100);
    FATP_ASSERT_EQ(small_mat.rows(), 100u, "Dimension within int8_t range OK");
    return true;
}

FATP_TEST_CASE(integer_value_types)
{
    // Signed integers should work
    std::vector<int> rows = {0, 1};
    std::vector<int> cols = {0, 1};
    std::vector<int> int_vals = {1, 2};

    CSRMatrix<int, int> int_mat(2, 2, rows, cols, int_vals);
    FATP_ASSERT_EQ(int_mat(0, 0), 1, "Signed int value type OK");

    std::vector<long> long_vals = {1L, 2L};
    CSRMatrix<long, int> long_mat(2, 2, rows, cols, long_vals);
    FATP_ASSERT_EQ(long_mat(0, 0), 1L, "Signed long value type OK");

    // -------------------------------------------------------------------------
    // Compile-time type restrictions (static_assert prevents instantiation):
    // -------------------------------------------------------------------------
    //
    // CSRMatrix<unsigned int, int> mat1;  // Error: unsigned value types not supported
    //                                     // (subtraction may produce negative intermediates)
    //
    // CSRMatrix<double, uint32_t> mat2;   // Error: unsigned index types not supported
    //                                     // (signed indices required for bounds checking)
    //
    // CSRMatrix<bool> mat3;               // Error: bool not supported because
    //                                     // std::vector<bool> is bit-packed and lacks
    //                                     // contiguous memory access via .data()
    // -------------------------------------------------------------------------

    return true;
}

} // namespace fat_p::testing::csrmatrix

// ============================================================================

namespace fat_p::testing
{

bool test_CSRMatrix()
{
    FATP_PRINT_HEADER(CSRMatrix)

    auto& config = get_test_config();
    config.verbose = true;

    TestRunner runner;

    FATP_RUN_TEST_NS(runner, csrmatrix, default_construction);
    FATP_RUN_TEST_NS(runner, csrmatrix, sized_construction);
    FATP_RUN_TEST_NS(runner, csrmatrix, coo_construction_sum_policy);
    FATP_RUN_TEST_NS(runner, csrmatrix, coo_construction_sum_duplicates);
    FATP_RUN_TEST_NS(runner, csrmatrix, coo_construction_keep_policy);
    FATP_RUN_TEST_NS(runner, csrmatrix, coo_construction_keep_filters_zeros);
    FATP_RUN_TEST_NS(runner, csrmatrix, coo_construction_error_policy);
    FATP_RUN_TEST_NS(runner, csrmatrix, coo_construction_out_of_bounds);
    FATP_RUN_TEST_NS(runner, csrmatrix, coo_construction_negative_index);
    FATP_RUN_TEST_NS(runner, csrmatrix, coo_construction_mismatched_sizes);
    FATP_RUN_TEST_NS(runner, csrmatrix, from_dense_construction);
    FATP_RUN_TEST_NS(runner, csrmatrix, from_dense_null_pointer);
    FATP_RUN_TEST_NS(runner, csrmatrix, element_access_valid);
    FATP_RUN_TEST_NS(runner, csrmatrix, element_access_out_of_bounds);
    FATP_RUN_TEST_NS(runner, csrmatrix, element_set);
    FATP_RUN_TEST_NS(runner, csrmatrix, keep_policy_operator_consistency);
    FATP_RUN_TEST_NS(runner, csrmatrix, keep_policy_set_collapses_duplicates);
    FATP_RUN_TEST_NS(runner, csrmatrix, transpose_basic);
    FATP_RUN_TEST_NS(runner, csrmatrix, transpose_rectangular);
    FATP_RUN_TEST_NS(runner, csrmatrix, matrix_addition);
    FATP_RUN_TEST_NS(runner, csrmatrix, matrix_addition_with_keep_duplicates);
    FATP_RUN_TEST_NS(runner, csrmatrix, matrix_subtraction);
    FATP_RUN_TEST_NS(runner, csrmatrix, scalar_multiplication);
    FATP_RUN_TEST_NS(runner, csrmatrix, scalar_multiplication_by_zero);
    FATP_RUN_TEST_NS(runner, csrmatrix, compound_assignment_operators);
    FATP_RUN_TEST_NS(runner, csrmatrix, matmul_basic);
    FATP_RUN_TEST_NS(runner, csrmatrix, matmul_rectangular);
    FATP_RUN_TEST_NS(runner, csrmatrix, matmul_size_mismatch);
    FATP_RUN_TEST_NS(runner, csrmatrix, matmul_index_overflow);
    FATP_RUN_TEST_NS(runner, csrmatrix, matmul_large_index_type);
    FATP_RUN_TEST_NS(runner, csrmatrix, matmul_accumulator_reset);
    FATP_RUN_TEST_NS(runner, csrmatrix, matvec_basic);
    FATP_RUN_TEST_NS(runner, csrmatrix, matvec_alpha_beta);
    FATP_RUN_TEST_NS(runner, csrmatrix, matvec_alpha_beta_vector_overload);
    FATP_RUN_TEST_NS(runner, csrmatrix, matvec_beta_zero_nan_safe);
    FATP_RUN_TEST_NS(runner, csrmatrix, matvec_size_mismatch);
    FATP_RUN_TEST_NS(runner, csrmatrix, matvec_parallel_matches_serial);
    FATP_RUN_TEST_NS(runner, csrmatrix, matvec_parallel_alpha_beta);
    FATP_RUN_TEST_NS(runner, csrmatrix, equality_operators);
    FATP_RUN_TEST_NS(runner, csrmatrix, approximate_equality);
    FATP_RUN_TEST_NS(runner, csrmatrix, density_sparsity);
    FATP_RUN_TEST_NS(runner, csrmatrix, is_symmetric);
    FATP_RUN_TEST_NS(runner, csrmatrix, remove_zeros);
    FATP_RUN_TEST_NS(runner, csrmatrix, row_nnz);
    FATP_RUN_TEST_NS(runner, csrmatrix, to_dense);
    FATP_RUN_TEST_NS(runner, csrmatrix, to_dense_keep_duplicates);
    FATP_RUN_TEST_NS(runner, csrmatrix, shrink_to_fit);
    FATP_RUN_TEST_NS(runner, csrmatrix, identity_matrix);
    FATP_RUN_TEST_NS(runner, csrmatrix, diagonal_matrix);
    FATP_RUN_TEST_NS(runner, csrmatrix, empty_matrix);
    FATP_RUN_TEST_NS(runner, csrmatrix, single_element);
    FATP_RUN_TEST_NS(runner, csrmatrix, empty_rows);
    FATP_RUN_TEST_NS(runner, csrmatrix, row_iteration);
    FATP_RUN_TEST_NS(runner, csrmatrix, ostream_output);
    FATP_RUN_TEST_NS(runner, csrmatrix, dimension_overflow_validation);
    FATP_RUN_TEST_NS(runner, csrmatrix, integer_value_types);

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_CSRMatrix() ? 0 : 1;
}
#endif
