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

TEST_CASE(default_construction)
{
    CSRMatrix<double> mat;
    ASSERT_EQ(mat.rows(), 0u, "Default construction: rows should be 0");
    ASSERT_EQ(mat.cols(), 0u, "Default construction: cols should be 0");
    ASSERT_EQ(mat.nnz(), 0u, "Default construction: nnz should be 0");
    ASSERT_TRUE(mat.empty(), "Default construction: should be empty");
    return true;
}

TEST_CASE(sized_construction)
{
    CSRMatrix<double> mat(10, 20);
    ASSERT_EQ(mat.rows(), 10u, "Sized construction: rows should be 10");
    ASSERT_EQ(mat.cols(), 20u, "Sized construction: cols should be 20");
    ASSERT_EQ(mat.nnz(), 0u, "Sized construction: nnz should be 0");
    ASSERT_TRUE(mat.empty(), "Sized construction: should be empty");
    return true;
}

TEST_CASE(coo_construction_sum_policy)
{
    // 3x3 matrix with entries
    std::vector<int> rows = {0, 0, 1, 2};
    std::vector<int> cols = {0, 1, 1, 2};
    std::vector<double> vals = {1.0, 2.0, 3.0, 4.0};

    CSRMatrix<double, int> mat(3, 3, rows, cols, vals,
                                CSRMatrix<double, int>::DuplicatePolicy::Sum);

    ASSERT_EQ(mat.rows(), 3u, "COO Sum: rows");
    ASSERT_EQ(mat.cols(), 3u, "COO Sum: cols");
    ASSERT_EQ(mat.nnz(), 4u, "COO Sum: nnz");
    ASSERT_CLOSE(mat(0, 0), 1.0, "COO Sum: (0,0)");
    ASSERT_CLOSE(mat(0, 1), 2.0, "COO Sum: (0,1)");
    ASSERT_CLOSE(mat(1, 1), 3.0, "COO Sum: (1,1)");
    ASSERT_CLOSE(mat(2, 2), 4.0, "COO Sum: (2,2)");
    return true;
}

TEST_CASE(coo_construction_sum_duplicates)
{
    // Duplicates at (0,1): 2.0 + 5.0 = 7.0
    std::vector<int> rows = {0, 0, 0, 1};
    std::vector<int> cols = {0, 1, 1, 1};
    std::vector<double> vals = {1.0, 2.0, 5.0, 3.0};

    CSRMatrix<double, int> mat(2, 2, rows, cols, vals,
                                CSRMatrix<double, int>::DuplicatePolicy::Sum);

    ASSERT_EQ(mat.nnz(), 3u, "COO Sum duplicates: nnz after merge");
    ASSERT_CLOSE(mat(0, 1), 7.0, "COO Sum duplicates: summed value");
    return true;
}

TEST_CASE(coo_construction_keep_policy)
{
    // Keep all entries including duplicates
    std::vector<int> rows = {0, 0, 1};
    std::vector<int> cols = {1, 1, 0};
    std::vector<double> vals = {2.0, 3.0, 1.0};

    CSRMatrix<double, int> mat(2, 2, rows, cols, vals,
                                CSRMatrix<double, int>::DuplicatePolicy::Keep);

    ASSERT_EQ(mat.nnz(), 3u, "COO Keep: all entries preserved");
    // operator() should return sum for consistency
    ASSERT_CLOSE(mat(0, 1), 5.0, "COO Keep: operator() returns sum");
    return true;
}

TEST_CASE(coo_construction_keep_filters_zeros)
{
    // Keep policy should still filter explicit zeros (sparse invariant)
    std::vector<int> rows = {0, 0, 1};
    std::vector<int> cols = {0, 1, 0};
    std::vector<double> vals = {1.0, 0.0, 2.0};  // Zero at (0,1)

    CSRMatrix<double, int> mat(2, 2, rows, cols, vals,
                                CSRMatrix<double, int>::DuplicatePolicy::Keep);

    ASSERT_EQ(mat.nnz(), 2u, "COO Keep: zeros filtered");
    ASSERT_CLOSE(mat(0, 1), 0.0, "COO Keep: zero not stored");
    return true;
}

TEST_CASE(coo_construction_error_policy)
{
    std::vector<int> rows = {0, 0};
    std::vector<int> cols = {1, 1};  // Duplicate!
    std::vector<double> vals = {2.0, 3.0};

    ASSERT_THROWS(
        (CSRMatrix<double, int>(2, 2, rows, cols, vals,
                                CSRMatrix<double, int>::DuplicatePolicy::Error)),
        std::invalid_argument,
        "COO Error policy: should throw on duplicates"
    );
    return true;
}

TEST_CASE(coo_construction_out_of_bounds)
{
    std::vector<int> rows = {0, 5};  // 5 is out of bounds for 3x3
    std::vector<int> cols = {0, 0};
    std::vector<double> vals = {1.0, 2.0};

    ASSERT_THROWS(
        (CSRMatrix<double, int>(3, 3, rows, cols, vals)),
        std::out_of_range,
        "COO: should throw on out-of-bounds row"
    );
    return true;
}

TEST_CASE(coo_construction_negative_index)
{
    std::vector<int> rows = {0, -1};  // Negative!
    std::vector<int> cols = {0, 0};
    std::vector<double> vals = {1.0, 2.0};

    ASSERT_THROWS(
        (CSRMatrix<double, int>(3, 3, rows, cols, vals)),
        std::out_of_range,
        "COO: negative row index should throw"
    );
    return true;
}

TEST_CASE(coo_construction_mismatched_sizes)
{
    std::vector<int> rows = {0, 1};
    std::vector<int> cols = {0};  // Size mismatch!
    std::vector<double> vals = {1.0, 2.0};

    ASSERT_THROWS(
        (CSRMatrix<double, int>(3, 3, rows, cols, vals)),
        std::invalid_argument,
        "COO: should throw on mismatched array sizes"
    );
    return true;
}

TEST_CASE(from_dense_construction)
{
    // Row-major dense: [[1, 0, 2], [0, 3, 0], [4, 0, 5]]
    std::vector<double> dense = {1.0, 0.0, 2.0, 0.0, 3.0, 0.0, 4.0, 0.0, 5.0};

    auto mat = CSRMatrix<double, int>::from_dense(dense.data(), 3, 3);

    ASSERT_EQ(mat.rows(), 3u, "from_dense: rows");
    ASSERT_EQ(mat.cols(), 3u, "from_dense: cols");
    ASSERT_EQ(mat.nnz(), 5u, "from_dense: nnz");
    ASSERT_CLOSE(mat(0, 0), 1.0, "from_dense: (0,0)");
    ASSERT_CLOSE(mat(0, 2), 2.0, "from_dense: (0,2)");
    ASSERT_CLOSE(mat(1, 1), 3.0, "from_dense: (1,1)");
    ASSERT_CLOSE(mat(2, 0), 4.0, "from_dense: (2,0)");
    ASSERT_CLOSE(mat(2, 2), 5.0, "from_dense: (2,2)");
    return true;
}

TEST_CASE(from_dense_null_pointer)
{
    ASSERT_THROWS(
        (CSRMatrix<double, int>::from_dense(nullptr, 3, 3)),
        std::invalid_argument,
        "from_dense: should throw on null pointer with non-zero dimensions"
    );

    // Null with zero dimensions should be OK
    auto mat = CSRMatrix<double, int>::from_dense(nullptr, 0, 0);
    ASSERT_TRUE(mat.empty(), "from_dense: null with zero dimensions is OK");
    return true;
}

// ============================================================================
// Element Access Tests
// ============================================================================

TEST_CASE(element_access_valid)
{
    std::vector<int> rows = {0, 1, 2};
    std::vector<int> cols = {0, 1, 2};
    std::vector<double> vals = {1.0, 2.0, 3.0};

    CSRMatrix<double, int> mat(3, 3, rows, cols, vals);

    ASSERT_CLOSE(mat(0, 0), 1.0, "Access: diagonal (0,0)");
    ASSERT_CLOSE(mat(1, 1), 2.0, "Access: diagonal (1,1)");
    ASSERT_CLOSE(mat(2, 2), 3.0, "Access: diagonal (2,2)");
    ASSERT_CLOSE(mat(0, 1), 0.0, "Access: zero element");
    return true;
}

TEST_CASE(element_access_out_of_bounds)
{
    CSRMatrix<double, int> mat(3, 3);

    ASSERT_THROWS(mat(3, 0), std::out_of_range, "Access: row out of bounds");
    ASSERT_THROWS(mat(0, 3), std::out_of_range, "Access: col out of bounds");
    return true;
}

TEST_CASE(element_set)
{
    CSRMatrix<double, int> mat(3, 3);

    mat.set(0, 0, 1.0);
    mat.set(1, 2, 2.0);
    mat.set(2, 1, 3.0);

    ASSERT_EQ(mat.nnz(), 3u, "Set: nnz after insertions");
    ASSERT_CLOSE(mat(0, 0), 1.0, "Set: value at (0,0)");
    ASSERT_CLOSE(mat(1, 2), 2.0, "Set: value at (1,2)");
    ASSERT_CLOSE(mat(2, 1), 3.0, "Set: value at (2,1)");

    // Update existing
    mat.set(0, 0, 5.0);
    ASSERT_CLOSE(mat(0, 0), 5.0, "Set: updated value");
    ASSERT_EQ(mat.nnz(), 3u, "Set: nnz unchanged after update");

    // Set to zero removes entry
    mat.set(0, 0, 0.0);
    ASSERT_EQ(mat.nnz(), 2u, "Set: nnz decreased after zero");
    ASSERT_CLOSE(mat(0, 0), 0.0, "Set: value is now zero");
    return true;
}

// ============================================================================
// DuplicatePolicy::Keep Semantics Tests
// ============================================================================

TEST_CASE(keep_policy_operator_consistency)
{
    // This test verifies the fix for operator() + Keep inconsistency
    // operator() should return the SUM of all entries, matching matvec/to_dense

    std::vector<int> rows = {0, 0, 0};
    std::vector<int> cols = {1, 1, 1};  // Three entries at (0,1)
    std::vector<double> vals = {2.0, 3.0, 4.0};

    CSRMatrix<double, int> mat(1, 3, rows, cols, vals,
                                CSRMatrix<double, int>::DuplicatePolicy::Keep);

    ASSERT_EQ(mat.nnz(), 3u, "Keep: all entries stored");

    // operator() should sum duplicates
    double accessed = mat(0, 1);
    ASSERT_CLOSE(accessed, 9.0, "Keep: operator() returns sum (2+3+4=9)");

    // to_dense should also sum
    auto dense = mat.to_dense();
    ASSERT_CLOSE(dense[1], 9.0, "Keep: to_dense returns sum");

    // matvec should use summed value
    std::vector<double> x = {1.0, 1.0, 1.0};
    std::vector<double> y = mat * x;
    ASSERT_CLOSE(y[0], 9.0, "Keep: matvec uses summed value");
    return true;
}

TEST_CASE(keep_policy_set_collapses_duplicates)
{
    // set() should remove all existing entries at a position
    std::vector<int> rows = {0, 0};
    std::vector<int> cols = {1, 1};
    std::vector<double> vals = {2.0, 3.0};

    CSRMatrix<double, int> mat(1, 2, rows, cols, vals,
                                CSRMatrix<double, int>::DuplicatePolicy::Keep);

    ASSERT_EQ(mat.nnz(), 2u, "Keep: initial duplicates");

    // set should collapse to single entry
    mat.set(0, 1, 10.0);
    ASSERT_EQ(mat.nnz(), 1u, "Keep: set collapses duplicates");
    ASSERT_CLOSE(mat(0, 1), 10.0, "Keep: new value after set");
    return true;
}

// ============================================================================
// Matrix Operations Tests
// ============================================================================

TEST_CASE(transpose_basic)
{
    // [[1, 2], [3, 4]] -> [[1, 3], [2, 4]]
    std::vector<double> dense = {1.0, 2.0, 3.0, 4.0};
    auto mat = CSRMatrix<double, int>::from_dense(dense.data(), 2, 2);

    auto transposed = mat.transpose();

    ASSERT_EQ(transposed.rows(), 2u, "Transpose: rows");
    ASSERT_EQ(transposed.cols(), 2u, "Transpose: cols");
    ASSERT_CLOSE(transposed(0, 0), 1.0, "Transpose: (0,0)");
    ASSERT_CLOSE(transposed(0, 1), 3.0, "Transpose: (0,1)");
    ASSERT_CLOSE(transposed(1, 0), 2.0, "Transpose: (1,0)");
    ASSERT_CLOSE(transposed(1, 1), 4.0, "Transpose: (1,1)");
    return true;
}

TEST_CASE(transpose_rectangular)
{
    // 2x3 -> 3x2
    std::vector<double> dense = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};
    auto mat = CSRMatrix<double, int>::from_dense(dense.data(), 2, 3);

    auto transposed = mat.transpose();

    ASSERT_EQ(transposed.rows(), 3u, "Transpose rect: rows");
    ASSERT_EQ(transposed.cols(), 2u, "Transpose rect: cols");
    return true;
}

TEST_CASE(matrix_addition)
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

    ASSERT_CLOSE(C(0, 0), 1.0, "Add: (0,0)");
    ASSERT_CLOSE(C(0, 1), 3.0, "Add: (0,1)");
    ASSERT_CLOSE(C(1, 1), 6.0, "Add: (1,1) = 2+4");
    return true;
}

TEST_CASE(matrix_addition_with_keep_duplicates)
{
    // Test that operator+ correctly consolidates duplicates from Keep policy
    std::vector<int> a_rows = {0, 0};
    std::vector<int> a_cols = {1, 1};
    std::vector<double> a_vals = {2.0, 3.0};
    CSRMatrix<double, int> A(1, 2, a_rows, a_cols, a_vals,
                              CSRMatrix<double, int>::DuplicatePolicy::Keep);

    std::vector<int> b_rows = {0};
    std::vector<int> b_cols = {1};
    std::vector<double> b_vals = {4.0};
    CSRMatrix<double, int> B(1, 2, b_rows, b_cols, b_vals);

    auto C = A + B;

    // Mathematical correctness: 2+3+4 = 9
    ASSERT_CLOSE(C(0, 1), 9.0, "Add Keep: 2+3+4=9");
    // Structural correctness: result should have unique columns
    ASSERT_EQ(C.nnz(), 1u, "Add Keep: result consolidated to unique columns");
    return true;
}

TEST_CASE(matrix_subtraction)
{
    std::vector<int> rows = {0, 1};
    std::vector<int> cols = {0, 1};
    std::vector<double> vals1 = {5.0, 8.0};
    std::vector<double> vals2 = {2.0, 3.0};

    CSRMatrix<double, int> A(2, 2, rows, cols, vals1);
    CSRMatrix<double, int> B(2, 2, rows, cols, vals2);

    auto C = A - B;

    ASSERT_CLOSE(C(0, 0), 3.0, "Sub: (0,0)");
    ASSERT_CLOSE(C(1, 1), 5.0, "Sub: (1,1)");
    return true;
}

TEST_CASE(scalar_multiplication)
{
    std::vector<int> rows = {0, 1};
    std::vector<int> cols = {0, 1};
    std::vector<double> vals = {2.0, 3.0};

    CSRMatrix<double, int> mat(2, 2, rows, cols, vals);

    auto scaled = mat * 2.5;

    ASSERT_CLOSE(scaled(0, 0), 5.0, "Scale: (0,0)");
    ASSERT_CLOSE(scaled(1, 1), 7.5, "Scale: (1,1)");

    // Test commutative form
    auto scaled2 = 2.5 * mat;
    ASSERT_CLOSE(scaled2(0, 0), 5.0, "Scale commutative: (0,0)");
    return true;
}

TEST_CASE(scalar_multiplication_by_zero)
{
    std::vector<int> rows = {0, 1};
    std::vector<int> cols = {0, 1};
    std::vector<double> vals = {2.0, 3.0};

    CSRMatrix<double, int> mat(2, 2, rows, cols, vals);
    ASSERT_EQ(mat.nnz(), 2u, "Before zero scale: nnz");

    // Multiply by zero should yield empty sparse matrix (sparse invariant)
    auto zero_scaled = mat * 0.0;
    ASSERT_EQ(zero_scaled.nnz(), 0u, "Scale by zero: nnz should be 0");
    ASSERT_EQ(zero_scaled.rows(), 2u, "Scale by zero: rows preserved");
    ASSERT_EQ(zero_scaled.cols(), 2u, "Scale by zero: cols preserved");
    ASSERT_CLOSE(zero_scaled(0, 0), 0.0, "Scale by zero: value is zero");

    // Test in-place version
    CSRMatrix<double, int> mat2(2, 2, rows, cols, vals);
    mat2 *= 0.0;
    ASSERT_EQ(mat2.nnz(), 0u, "*= zero: nnz should be 0");
    ASSERT_EQ(mat2.rows(), 2u, "*= zero: rows preserved");
    ASSERT_EQ(mat2.cols(), 2u, "*= zero: cols preserved");

    // Test commutative form
    CSRMatrix<double, int> mat3(2, 2, rows, cols, vals);
    auto zero_scaled2 = 0.0 * mat3;
    ASSERT_EQ(zero_scaled2.nnz(), 0u, "0 * mat: nnz should be 0");
    return true;
}

TEST_CASE(compound_assignment_operators)
{
    std::vector<int> rows = {0, 1};
    std::vector<int> cols = {0, 1};
    std::vector<double> vals = {2.0, 3.0};

    CSRMatrix<double, int> A(2, 2, rows, cols, vals);
    CSRMatrix<double, int> B(2, 2, rows, cols, vals);

    // Test operator*=
    A *= 2.0;
    ASSERT_CLOSE(A(0, 0), 4.0, "*=: (0,0)");
    ASSERT_CLOSE(A(1, 1), 6.0, "*=: (1,1)");

    // Test operator+=
    A += B;
    ASSERT_CLOSE(A(0, 0), 6.0, "+=: (0,0) = 4+2");
    ASSERT_CLOSE(A(1, 1), 9.0, "+=: (1,1) = 6+3");

    // Test operator-=
    A -= B;
    ASSERT_CLOSE(A(0, 0), 4.0, "-=: (0,0) = 6-2");
    ASSERT_CLOSE(A(1, 1), 6.0, "-=: (1,1) = 9-3");
    return true;
}

TEST_CASE(matmul_basic)
{
    // A = [[1, 2], [3, 4]], B = [[5, 6], [7, 8]]
    // C = A*B = [[19, 22], [43, 50]]
    std::vector<double> denseA = {1.0, 2.0, 3.0, 4.0};
    std::vector<double> denseB = {5.0, 6.0, 7.0, 8.0};

    auto A = CSRMatrix<double, int>::from_dense(denseA.data(), 2, 2);
    auto B = CSRMatrix<double, int>::from_dense(denseB.data(), 2, 2);

    auto C = A.matmul(B);

    ASSERT_CLOSE(C(0, 0), 19.0, "Matmul: (0,0)");
    ASSERT_CLOSE(C(0, 1), 22.0, "Matmul: (0,1)");
    ASSERT_CLOSE(C(1, 0), 43.0, "Matmul: (1,0)");
    ASSERT_CLOSE(C(1, 1), 50.0, "Matmul: (1,1)");
    return true;
}

TEST_CASE(matmul_rectangular)
{
    // A = 2x3, B = 3x2 -> C = 2x2
    std::vector<double> denseA = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};
    std::vector<double> denseB = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};

    auto A = CSRMatrix<double, int>::from_dense(denseA.data(), 2, 3);
    auto B = CSRMatrix<double, int>::from_dense(denseB.data(), 3, 2);

    auto C = A.matmul(B);

    ASSERT_EQ(C.rows(), 2u, "Matmul rect: rows");
    ASSERT_EQ(C.cols(), 2u, "Matmul rect: cols");
    // C[0,0] = 1*1 + 2*3 + 3*5 = 22
    ASSERT_CLOSE(C(0, 0), 22.0, "Matmul rect: (0,0)");
    return true;
}

TEST_CASE(matmul_size_mismatch)
{
    // A = 2x3, B = 2x2 -> incompatible (3 != 2)
    CSRMatrix<double, int> A(2, 3);
    CSRMatrix<double, int> B(2, 2);

    ASSERT_THROWS(
        A.matmul(B),
        std::invalid_argument,
        "Matmul: should throw on dimension mismatch"
    );
    return true;
}

TEST_CASE(matmul_index_overflow)
{
    // Regression test for marker overflow bug
    // When using small IndexType (int8_t), rows > 127 would overflow the marker
    // The fix uses size_type for the marker vector

    constexpr std::size_t dim = 150;  // > int8_t max (127)

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
    ASSERT_THROWS(
        (CSRMatrix<double, int8_t>(dim, dim, rows, cols, vals)),
        std::overflow_error,
        "Matmul overflow: dimension validation catches int8_t overflow"
    );
    return true;
}

TEST_CASE(matmul_large_index_type)
{
    // Test with int64_t IndexType for large matrices
    std::vector<int64_t> rows = {0, 1, 2};
    std::vector<int64_t> cols = {0, 1, 2};
    std::vector<double> vals = {1.0, 1.0, 1.0};

    CSRMatrix<double, int64_t> A(3, 3, rows, cols, vals);
    auto C = A.matmul(A);

    ASSERT_EQ(C.nnz(), 3u, "Matmul int64: identity * identity = identity");
    return true;
}

TEST_CASE(matmul_accumulator_reset)
{
    // Verify accumulator is properly reset between rows
    // Row0 touches column1, Row1 touches nothing
    std::vector<double> denseA = {0.0, 1.0, 0.0, 0.0};
    std::vector<double> denseB = {1.0, 0.0, 0.0, 1.0};

    auto A = CSRMatrix<double, int>::from_dense(denseA.data(), 2, 2);
    auto B = CSRMatrix<double, int>::from_dense(denseB.data(), 2, 2);

    auto C = A.matmul(B);

    ASSERT_CLOSE(C(0, 1), 1.0, "Matmul accumulator: (0,1)");
    ASSERT_CLOSE(C(1, 1), 0.0, "Matmul accumulator: (1,1) should be 0");
    return true;
}

// ============================================================================
// Matrix-Vector Multiplication Tests
// ============================================================================

TEST_CASE(matvec_basic)
{
    // [[1, 2], [3, 4]] * [1, 2] = [5, 11]
    std::vector<double> dense = {1.0, 2.0, 3.0, 4.0};
    auto mat = CSRMatrix<double, int>::from_dense(dense.data(), 2, 2);

    std::vector<double> x = {1.0, 2.0};
    auto y = mat * x;

    ASSERT_CLOSE(y[0], 5.0, "Matvec: y[0]");
    ASSERT_CLOSE(y[1], 11.0, "Matvec: y[1]");
    return true;
}

TEST_CASE(matvec_alpha_beta)
{
    std::vector<double> dense = {1.0, 2.0, 3.0, 4.0};
    auto mat = CSRMatrix<double, int>::from_dense(dense.data(), 2, 2);

    std::vector<double> x = {1.0, 2.0};
    std::vector<double> y = {10.0, 20.0};

    // y = 2 * A*x + 3 * y = 2*[5, 11] + 3*[10, 20] = [40, 82]
    mat.matvec(2.0, x.data(), 3.0, y.data());

    ASSERT_CLOSE(y[0], 40.0, "Matvec alpha-beta: y[0]");
    ASSERT_CLOSE(y[1], 82.0, "Matvec alpha-beta: y[1]");
    return true;
}

TEST_CASE(matvec_alpha_beta_vector_overload)
{
    std::vector<double> dense = {1.0, 2.0, 3.0, 4.0};
    auto mat = CSRMatrix<double, int>::from_dense(dense.data(), 2, 2);

    std::vector<double> x = {1.0, 2.0};
    std::vector<double> y = {10.0, 20.0};

    // Test std::vector overload
    mat.matvec(2.0, x, 3.0, y);

    ASSERT_CLOSE(y[0], 40.0, "Matvec vector overload: y[0]");
    ASSERT_CLOSE(y[1], 82.0, "Matvec vector overload: y[1]");
    return true;
}

TEST_CASE(matvec_beta_zero_nan_safe)
{
    // When beta=0, uninitialized y should not propagate NaN
    std::vector<double> dense = {1.0, 0.0, 0.0, 1.0};
    auto mat = CSRMatrix<double, int>::from_dense(dense.data(), 2, 2);

    std::vector<double> x = {2.0, 3.0};
    std::vector<double> y = {std::numeric_limits<double>::quiet_NaN(),
                             std::numeric_limits<double>::quiet_NaN()};

    mat.matvec(1.0, x.data(), 0.0, y.data());

    ASSERT_FALSE(std::isnan(y[0]), "Matvec beta=0: y[0] not NaN");
    ASSERT_FALSE(std::isnan(y[1]), "Matvec beta=0: y[1] not NaN");
    ASSERT_CLOSE(y[0], 2.0, "Matvec beta=0: y[0] value");
    ASSERT_CLOSE(y[1], 3.0, "Matvec beta=0: y[1] value");
    return true;
}

TEST_CASE(matvec_size_mismatch)
{
    CSRMatrix<double, int> mat(2, 3);  // 2x3 matrix
    std::vector<double> x = {1.0, 2.0};  // Size 2, need size 3

    ASSERT_THROWS(
        mat * x,
        std::invalid_argument,
        "Matvec: should throw on vector size mismatch"
    );
    return true;
}

TEST_CASE(matvec_parallel_matches_serial)
{
    std::vector<double> dense = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0};
    auto mat = CSRMatrix<double, int>::from_dense(dense.data(), 3, 3);

    std::vector<double> x = {1.0, 2.0, 3.0};
    std::vector<double> y_serial(3);
    std::vector<double> y_parallel(3);

    mat.matvec(x.data(), y_serial.data());
    mat.matvec_parallel(x.data(), y_parallel.data());

    ASSERT_CLOSE(y_parallel[0], y_serial[0], "Parallel matvec: matches serial [0]");
    ASSERT_CLOSE(y_parallel[1], y_serial[1], "Parallel matvec: matches serial [1]");
    ASSERT_CLOSE(y_parallel[2], y_serial[2], "Parallel matvec: matches serial [2]");
    return true;
}

TEST_CASE(matvec_parallel_alpha_beta)
{
    std::vector<double> dense = {1.0, 2.0, 3.0, 4.0};
    auto mat = CSRMatrix<double, int>::from_dense(dense.data(), 2, 2);

    std::vector<double> x = {1.0, 2.0};
    std::vector<double> y_serial = {10.0, 20.0};
    std::vector<double> y_parallel = {10.0, 20.0};

    mat.matvec(2.0, x.data(), 3.0, y_serial.data());
    mat.matvec_parallel(2.0, x.data(), 3.0, y_parallel.data());

    ASSERT_CLOSE(y_parallel[0], y_serial[0], "Parallel alpha-beta: matches serial [0]");
    ASSERT_CLOSE(y_parallel[1], y_serial[1], "Parallel alpha-beta: matches serial [1]");
    return true;
}

// ============================================================================
// Equality Operator Tests
// ============================================================================

TEST_CASE(equality_operators)
{
    std::vector<int> rows = {0, 1};
    std::vector<int> cols = {0, 1};
    std::vector<double> vals = {1.0, 2.0};

    CSRMatrix<double, int> A(2, 2, rows, cols, vals);
    CSRMatrix<double, int> B(2, 2, rows, cols, vals);
    CSRMatrix<double, int> C(2, 2, rows, cols, {1.0, 3.0});  // Different values

    ASSERT_TRUE(A == B, "Equality: identical matrices");
    ASSERT_FALSE(A != B, "Inequality: identical matrices");
    ASSERT_FALSE(A == C, "Equality: different values");
    ASSERT_TRUE(A != C, "Inequality: different values");

    CSRMatrix<double, int> D(3, 3);  // Different dimensions
    ASSERT_FALSE(A == D, "Equality: different dimensions");
    return true;
}

TEST_CASE(approximate_equality)
{
    std::vector<int> rows = {0, 1};
    std::vector<int> cols = {0, 1};
    std::vector<double> vals1 = {1.0, 2.0};
    std::vector<double> vals2 = {1.0 + 1e-12, 2.0 - 1e-12};

    CSRMatrix<double, int> A(2, 2, rows, cols, vals1);
    CSRMatrix<double, int> B(2, 2, rows, cols, vals2);

    ASSERT_FALSE(A == B, "Exact equality: tiny differences matter");
    ASSERT_TRUE(A.equals(B, 1e-10), "Approximate equality: within epsilon");
    ASSERT_FALSE(A.equals(B, 1e-14), "Approximate equality: outside epsilon");
    return true;
}

// ============================================================================
// Utility Function Tests
// ============================================================================

TEST_CASE(density_sparsity)
{
    std::vector<int> rows = {0, 1, 2, 3};
    std::vector<int> cols = {0, 1, 2, 3};
    std::vector<double> vals = {1.0, 2.0, 3.0, 4.0};

    CSRMatrix<double, int> mat(4, 4, rows, cols, vals);

    // 4 non-zeros in 16 elements = 0.25 density
    ASSERT_CLOSE(mat.density(), 0.25, "Density: 4/16");
    ASSERT_CLOSE(mat.sparsity(), 0.75, "Sparsity: 12/16");
    return true;
}

TEST_CASE(is_symmetric)
{
    // Symmetric: [[1, 2], [2, 3]]
    std::vector<int> rows = {0, 0, 1, 1};
    std::vector<int> cols = {0, 1, 0, 1};
    std::vector<double> vals = {1.0, 2.0, 2.0, 3.0};

    CSRMatrix<double, int> symmetric(2, 2, rows, cols, vals);
    ASSERT_TRUE(symmetric.is_symmetric(), "is_symmetric: symmetric matrix");

    // Non-symmetric: [[1, 2], [3, 4]]
    std::vector<double> vals2 = {1.0, 2.0, 3.0, 4.0};
    CSRMatrix<double, int> nonsymmetric(2, 2, rows, cols, vals2);
    ASSERT_FALSE(nonsymmetric.is_symmetric(), "is_symmetric: non-symmetric matrix");

    // Rectangular cannot be symmetric
    CSRMatrix<double, int> rect(2, 3);
    ASSERT_FALSE(rect.is_symmetric(), "is_symmetric: rectangular");
    return true;
}

TEST_CASE(remove_zeros)
{
    std::vector<int> rows = {0, 0, 1};
    std::vector<int> cols = {0, 1, 1};
    std::vector<double> vals = {1.0, 2.0, 3.0};

    CSRMatrix<double, int> mat(2, 2, rows, cols, vals);

    // Set a value that's above is_effectively_zero threshold but below remove_zeros epsilon
    // is_effectively_zero uses epsilon*10 ≈ 2.2e-15 for double
    // So 1e-12 is stored but considered "removable" with epsilon=1e-10
    mat.set(0, 0, 1e-12);
    ASSERT_EQ(mat.nnz(), 3u, "Before remove_zeros");

    mat.remove_zeros(1e-10);  // Epsilon larger than 1e-12
    ASSERT_EQ(mat.nnz(), 2u, "After remove_zeros");
    return true;
}

TEST_CASE(row_nnz)
{
    std::vector<int> rows = {0, 0, 0, 1, 2, 2};
    std::vector<int> cols = {0, 1, 2, 0, 0, 2};
    std::vector<double> vals = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};

    CSRMatrix<double, int> mat(3, 3, rows, cols, vals);

    ASSERT_EQ(mat.row_nnz(0), 3u, "row_nnz: row 0 has 3");
    ASSERT_EQ(mat.row_nnz(1), 1u, "row_nnz: row 1 has 1");
    ASSERT_EQ(mat.row_nnz(2), 2u, "row_nnz: row 2 has 2");

    ASSERT_THROWS(mat.row_nnz(3), std::out_of_range, "row_nnz: out of bounds");
    return true;
}

TEST_CASE(to_dense)
{
    std::vector<int> rows = {0, 1, 2};
    std::vector<int> cols = {2, 0, 1};
    std::vector<double> vals = {1.0, 2.0, 3.0};

    CSRMatrix<double, int> mat(3, 3, rows, cols, vals);
    auto dense = mat.to_dense();

    // Row-major: [[0, 0, 1], [2, 0, 0], [0, 3, 0]]
    ASSERT_EQ(dense.size(), 9u, "to_dense: size");
    ASSERT_CLOSE(dense[0*3 + 2], 1.0, "to_dense: (0,2)");
    ASSERT_CLOSE(dense[1*3 + 0], 2.0, "to_dense: (1,0)");
    ASSERT_CLOSE(dense[2*3 + 1], 3.0, "to_dense: (2,1)");
    ASSERT_CLOSE(dense[0*3 + 0], 0.0, "to_dense: zeros");
    return true;
}

TEST_CASE(to_dense_keep_duplicates)
{
    // Verify to_dense properly sums Keep-policy duplicates
    std::vector<int> rows = {0, 0, 0};
    std::vector<int> cols = {1, 1, 1};
    std::vector<double> vals = {2.0, 3.0, 4.0};

    CSRMatrix<double, int> mat(1, 3, rows, cols, vals,
                                CSRMatrix<double, int>::DuplicatePolicy::Keep);

    auto dense = mat.to_dense();
    ASSERT_CLOSE(dense[1], 9.0, "to_dense Keep: sums duplicates (2+3+4=9)");
    return true;
}

TEST_CASE(shrink_to_fit)
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

    ASSERT_EQ(mat.nnz(), 5u, "Before shrink: nnz");
    mat.shrink_to_fit();
    ASSERT_EQ(mat.nnz(), 5u, "After shrink: nnz unchanged");
    // Can't easily test capacity reduction, but at least verify it compiles
    return true;
}

// ============================================================================
// Factory Function Tests
// ============================================================================

TEST_CASE(identity_matrix)
{
    auto I = identity_matrix<double, int>(4);

    ASSERT_EQ(I.rows(), 4u, "Identity: rows");
    ASSERT_EQ(I.cols(), 4u, "Identity: cols");
    ASSERT_EQ(I.nnz(), 4u, "Identity: nnz");

    for (std::size_t i = 0; i < 4; ++i)
    {
        ASSERT_CLOSE(I(i, i), 1.0, "Identity: diagonal");
        if (i > 0)
        {
            ASSERT_CLOSE(I(i, i-1), 0.0, "Identity: off-diagonal");
        }
    }
    return true;
}

TEST_CASE(diagonal_matrix)
{
    std::vector<double> diag = {1.0, 2.0, 3.0, 4.0};
    auto D = diagonal_matrix<double, int>(diag);

    ASSERT_EQ(D.rows(), 4u, "Diagonal: rows");
    ASSERT_EQ(D.cols(), 4u, "Diagonal: cols");
    ASSERT_EQ(D.nnz(), 4u, "Diagonal: nnz");

    ASSERT_CLOSE(D(0, 0), 1.0, "Diagonal: (0,0)");
    ASSERT_CLOSE(D(1, 1), 2.0, "Diagonal: (1,1)");
    ASSERT_CLOSE(D(2, 2), 3.0, "Diagonal: (2,2)");
    ASSERT_CLOSE(D(3, 3), 4.0, "Diagonal: (3,3)");
    return true;
}

// ============================================================================
// Edge Case Tests
// ============================================================================

TEST_CASE(empty_matrix)
{
    CSRMatrix<double> mat;

    ASSERT_TRUE(mat.empty(), "Empty: is empty");
    ASSERT_EQ(mat.nnz(), 0u, "Empty: nnz");

    auto transposed = mat.transpose();
    ASSERT_TRUE(transposed.empty(), "Empty: transpose empty");

    auto dense = mat.to_dense();
    ASSERT_TRUE(dense.empty(), "Empty: to_dense empty");
    return true;
}

TEST_CASE(single_element)
{
    std::vector<int> rows = {0};
    std::vector<int> cols = {0};
    std::vector<double> vals = {42.0};

    CSRMatrix<double, int> mat(1, 1, rows, cols, vals);

    ASSERT_EQ(mat.nnz(), 1u, "Single: nnz");
    ASSERT_CLOSE(mat(0, 0), 42.0, "Single: value");

    auto transposed = mat.transpose();
    ASSERT_CLOSE(transposed(0, 0), 42.0, "Single: transpose");
    return true;
}

TEST_CASE(empty_rows)
{
    // Matrix with row 1 empty: [[1, 0], [0, 0], [0, 2]]
    std::vector<int> rows = {0, 2};
    std::vector<int> cols = {0, 1};
    std::vector<double> vals = {1.0, 2.0};

    CSRMatrix<double, int> mat(3, 2, rows, cols, vals);

    ASSERT_EQ(mat.row_nnz(0), 1u, "Empty rows: row 0 nnz");
    ASSERT_EQ(mat.row_nnz(1), 0u, "Empty rows: row 1 empty");
    ASSERT_EQ(mat.row_nnz(2), 1u, "Empty rows: row 2 nnz");

    auto dense = mat.to_dense();
    ASSERT_CLOSE(dense[1*2 + 0], 0.0, "Empty rows: (1,0) is zero");
    ASSERT_CLOSE(dense[1*2 + 1], 0.0, "Empty rows: (1,1) is zero");
    return true;
}

TEST_CASE(row_iteration)
{
    std::vector<int> rows = {0, 0, 1, 2, 2};
    std::vector<int> cols = {1, 3, 2, 0, 4};
    std::vector<double> vals = {1.0, 2.0, 3.0, 4.0, 5.0};

    CSRMatrix<double, int> mat(3, 5, rows, cols, vals);

    auto row0 = mat.row(0);
    ASSERT_EQ(row0.size(), 2u, "Row iter: row 0 size");

    std::vector<std::pair<int, double>> entries;
    for (auto [col, val] : row0)
    {
        entries.emplace_back(col, val);
    }

    ASSERT_EQ(entries.size(), 2u, "Row iter: collected entries");
    ASSERT_EQ(entries[0].first, 1, "Row iter: first col");
    ASSERT_CLOSE(entries[0].second, 1.0, "Row iter: first val");
    ASSERT_EQ(entries[1].first, 3, "Row iter: second col");
    ASSERT_CLOSE(entries[1].second, 2.0, "Row iter: second val");

    // Test operator[]
    auto entry = row0[0];
    ASSERT_EQ(entry.first, 1, "Row iter operator[]: col");
    ASSERT_CLOSE(entry.second, 1.0, "Row iter operator[]: val");
    return true;
}

TEST_CASE(ostream_output)
{
    std::vector<int> rows = {0, 1};
    std::vector<int> cols = {0, 1};
    std::vector<double> vals = {1.0, 2.0};

    CSRMatrix<double, int> mat(3, 4, rows, cols, vals);

    std::ostringstream oss;
    oss << mat;

    std::string output = oss.str();
    ASSERT_TRUE(output.find("3x4") != std::string::npos, "ostream: contains dimensions");
    ASSERT_TRUE(output.find("nnz=2") != std::string::npos, "ostream: contains nnz");
    return true;
}

// ============================================================================
// Type Safety Tests
// ============================================================================

TEST_CASE(dimension_overflow_validation)
{
    // Test that constructors validate dimensions against IndexType limits
    // Using int8_t as IndexType, max value is 127

    // Should throw: 200 > 127
    ASSERT_THROWS(
        (CSRMatrix<double, int8_t>(200, 10)),
        std::overflow_error,
        "Dimension overflow: rows exceed int8_t"
    );

    ASSERT_THROWS(
        (CSRMatrix<double, int8_t>(10, 200)),
        std::overflow_error,
        "Dimension overflow: cols exceed int8_t"
    );

    // Should succeed: 100 <= 127
    CSRMatrix<double, int8_t> small_mat(100, 100);
    ASSERT_EQ(small_mat.rows(), 100u, "Dimension within int8_t range OK");
    return true;
}

TEST_CASE(integer_value_types)
{
    // Signed integers should work
    std::vector<int> rows = {0, 1};
    std::vector<int> cols = {0, 1};
    std::vector<int> int_vals = {1, 2};

    CSRMatrix<int, int> int_mat(2, 2, rows, cols, int_vals);
    ASSERT_EQ(int_mat(0, 0), 1, "Signed int value type OK");

    std::vector<long> long_vals = {1L, 2L};
    CSRMatrix<long, int> long_mat(2, 2, rows, cols, long_vals);
    ASSERT_EQ(long_mat(0, 0), 1L, "Signed long value type OK");

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

// ============================================================================
// Benchmarks
// ============================================================================

void benchmark_component()
{
    std::cout << "\n" << colors::cyan() << "CSRMatrix Benchmarks:"
              << colors::reset() << "\n\n";

    // Setup: 10k x 10k matrix with ~1% density
    constexpr size_t N = 10000;
    constexpr size_t nnz_target = N * N / 100;

    std::vector<int> rows;
    std::vector<int> cols;
    std::vector<double> vals;
    rows.reserve(nnz_target);
    cols.reserve(nnz_target);
    vals.reserve(nnz_target);

    // Simple deterministic pseudo-random generation (LCG)
    uint64_t rng = 12345;
    for (size_t i = 0; i < nnz_target; ++i)
    {
        rng = rng * 6364136223846793005ULL + 1;
        rows.push_back(static_cast<int>((rng >> 32) % N));
        rng = rng * 6364136223846793005ULL + 1;
        cols.push_back(static_cast<int>((rng >> 32) % N));
        vals.push_back(1.0);
    }

    CSRMatrix<double, int> A(N, N, rows, cols, vals);
    std::vector<double> x(N, 1.0);
    std::vector<double> y(N);

    std::cout << "Matrix: " << N << "x" << N << ", nnz=" << A.nnz()
              << " (density=" << std::fixed << std::setprecision(2)
              << A.density() * 100.0 << "%)\n\n";

    // Benchmark 1: Serial SpMV
    constexpr int iterations = 100;
    constexpr int warmup = 10;

    double time_spmv = measure_perf([&]() {
        A.matvec(x.data(), y.data());
        DoNotOptimize(y);
    }, iterations, warmup);

    std::cout << "SpMV (Serial):  " << format_time(time_spmv) << "\n";

    // Benchmark 2: Parallel SpMV
    double time_spmv_par = measure_perf([&]() {
        A.matvec_parallel(x.data(), y.data());
        DoNotOptimize(y);
    }, iterations, warmup);

    std::cout << "SpMV (OpenMP):  " << format_time(time_spmv_par) << "\n";

    // Benchmark 3: Transpose
    double time_transpose = measure_perf([&]() {
        auto AT = A.transpose();
        DoNotOptimize(AT);
    }, 10, 2);

    std::cout << "Transpose:      " << format_time(time_transpose) << "\n";

    // Benchmark 4: Matrix-Matrix Multiply (smaller matrix)
    constexpr size_t M = 1000;
    constexpr size_t nnz_small = M * M / 100;

    std::vector<int> small_rows, small_cols;
    std::vector<double> small_vals;
    small_rows.reserve(nnz_small);
    small_cols.reserve(nnz_small);
    small_vals.reserve(nnz_small);

    rng = 67890;
    for (size_t i = 0; i < nnz_small; ++i)
    {
        rng = rng * 6364136223846793005ULL + 1;
        small_rows.push_back(static_cast<int>((rng >> 32) % M));
        rng = rng * 6364136223846793005ULL + 1;
        small_cols.push_back(static_cast<int>((rng >> 32) % M));
        small_vals.push_back(1.0);
    }

    CSRMatrix<double, int> B(M, M, small_rows, small_cols, small_vals);

    double time_matmul = measure_perf([&]() {
        auto C = B.matmul(B);
        DoNotOptimize(C);
    }, 10, 2);

    std::cout << "MatMul (" << M << "x" << M << "): " << format_time(time_matmul) << "\n";
}

// ============================================================================
// Test Registration
// ============================================================================

void register_tests(TestRunner& runner)
{
    // Construction
    RUN_TEST_NS(runner, csrmatrix, default_construction);
    RUN_TEST_NS(runner, csrmatrix, sized_construction);
    RUN_TEST_NS(runner, csrmatrix, coo_construction_sum_policy);
    RUN_TEST_NS(runner, csrmatrix, coo_construction_sum_duplicates);
    RUN_TEST_NS(runner, csrmatrix, coo_construction_keep_policy);
    RUN_TEST_NS(runner, csrmatrix, coo_construction_keep_filters_zeros);
    RUN_TEST_NS(runner, csrmatrix, coo_construction_error_policy);
    RUN_TEST_NS(runner, csrmatrix, coo_construction_out_of_bounds);
    RUN_TEST_NS(runner, csrmatrix, coo_construction_negative_index);
    RUN_TEST_NS(runner, csrmatrix, coo_construction_mismatched_sizes);
    RUN_TEST_NS(runner, csrmatrix, from_dense_construction);
    RUN_TEST_NS(runner, csrmatrix, from_dense_null_pointer);

    // Element access
    RUN_TEST_NS(runner, csrmatrix, element_access_valid);
    RUN_TEST_NS(runner, csrmatrix, element_access_out_of_bounds);
    RUN_TEST_NS(runner, csrmatrix, element_set);

    // DuplicatePolicy::Keep semantics
    RUN_TEST_NS(runner, csrmatrix, keep_policy_operator_consistency);
    RUN_TEST_NS(runner, csrmatrix, keep_policy_set_collapses_duplicates);

    // Matrix operations
    RUN_TEST_NS(runner, csrmatrix, transpose_basic);
    RUN_TEST_NS(runner, csrmatrix, transpose_rectangular);
    RUN_TEST_NS(runner, csrmatrix, matrix_addition);
    RUN_TEST_NS(runner, csrmatrix, matrix_addition_with_keep_duplicates);
    RUN_TEST_NS(runner, csrmatrix, matrix_subtraction);
    RUN_TEST_NS(runner, csrmatrix, scalar_multiplication);
    RUN_TEST_NS(runner, csrmatrix, scalar_multiplication_by_zero);
    RUN_TEST_NS(runner, csrmatrix, compound_assignment_operators);
    RUN_TEST_NS(runner, csrmatrix, matmul_basic);
    RUN_TEST_NS(runner, csrmatrix, matmul_rectangular);
    RUN_TEST_NS(runner, csrmatrix, matmul_size_mismatch);
    RUN_TEST_NS(runner, csrmatrix, matmul_index_overflow);
    RUN_TEST_NS(runner, csrmatrix, matmul_large_index_type);
    RUN_TEST_NS(runner, csrmatrix, matmul_accumulator_reset);

    // Matrix-vector multiplication
    RUN_TEST_NS(runner, csrmatrix, matvec_basic);
    RUN_TEST_NS(runner, csrmatrix, matvec_alpha_beta);
    RUN_TEST_NS(runner, csrmatrix, matvec_alpha_beta_vector_overload);
    RUN_TEST_NS(runner, csrmatrix, matvec_beta_zero_nan_safe);
    RUN_TEST_NS(runner, csrmatrix, matvec_size_mismatch);
    RUN_TEST_NS(runner, csrmatrix, matvec_parallel_matches_serial);
    RUN_TEST_NS(runner, csrmatrix, matvec_parallel_alpha_beta);

    // Equality operators
    RUN_TEST_NS(runner, csrmatrix, equality_operators);
    RUN_TEST_NS(runner, csrmatrix, approximate_equality);

    // Utility functions
    RUN_TEST_NS(runner, csrmatrix, density_sparsity);
    RUN_TEST_NS(runner, csrmatrix, is_symmetric);
    RUN_TEST_NS(runner, csrmatrix, remove_zeros);
    RUN_TEST_NS(runner, csrmatrix, row_nnz);
    RUN_TEST_NS(runner, csrmatrix, to_dense);
    RUN_TEST_NS(runner, csrmatrix, to_dense_keep_duplicates);
    RUN_TEST_NS(runner, csrmatrix, shrink_to_fit);

    // Factory functions
    RUN_TEST_NS(runner, csrmatrix, identity_matrix);
    RUN_TEST_NS(runner, csrmatrix, diagonal_matrix);

    // Edge cases
    RUN_TEST_NS(runner, csrmatrix, empty_matrix);
    RUN_TEST_NS(runner, csrmatrix, single_element);
    RUN_TEST_NS(runner, csrmatrix, empty_rows);
    RUN_TEST_NS(runner, csrmatrix, row_iteration);
    RUN_TEST_NS(runner, csrmatrix, ostream_output);

    // Type safety
    RUN_TEST_NS(runner, csrmatrix, dimension_overflow_validation);
    RUN_TEST_NS(runner, csrmatrix, integer_value_types);
}

} // namespace fat_p::testing::csrmatrix

namespace fat_p::testing
{

bool test_CSRMatrix()
{
    PRINT_HEADER(CSRMatrix)

    auto& config = get_test_config();
    config.verbose = true;

    TestRunner runner;

    csrmatrix::register_tests(runner);

    csrmatrix::benchmark_component();

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
