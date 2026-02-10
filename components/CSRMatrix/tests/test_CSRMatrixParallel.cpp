/**
 * @file test_CSRMatrixParallel.cpp
 * @brief Comprehensive unit tests for CSRMatrixParallel.h
 */
/*
FATP_META:
  meta_version: 1
  component: CSRMatrixParallel
  file_role: test
  path: components/CSRMatrix/tests/test_CSRMatrixParallel.cpp
  layer: Testing
  namespace: fat_p
  summary: "Unit tests for CSRMatrixParallel."
  api_stability: in_work
  related:
    docs_search: "CSRMatrixParallel"
    headers:
      - include/fat_p/CSRMatrix.h
      - include/fat_p/CSRMatrixParallel.h
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

#include <iostream>
#include <random>
#include <set>

#include "CSRMatrix.h"
#include "CSRMatrixParallel.h"
#include "FatPTest.h"

namespace fat_p::testing::csrmatrixparallel
{

// ============================================================================
// Test Utilities
// ============================================================================

template <typename T>
double max_abs_diff(const std::vector<T>& a, const std::vector<T>& b)
{
    if (a.size() != b.size())
    {
        return std::numeric_limits<double>::infinity();
    }

    double max_diff = 0.0;
    for (size_t i = 0; i < a.size(); ++i)
    {
        max_diff = std::max(max_diff, std::abs(static_cast<double>(a[i] - b[i])));
    }
    return max_diff;
}

template <typename T, typename IndexType = int32_t>
fat_p::CSRMatrix<T, IndexType> generate_random_sparse(size_t rows, size_t cols, double density, std::mt19937& rng)
{
    std::vector<IndexType> row_indices;
    std::vector<IndexType> col_indices;
    std::vector<T> values;

    std::uniform_real_distribution<double> val_dist(-1.0, 1.0);
    std::uniform_real_distribution<double> prob_dist(0.0, 1.0);

    size_t expected_nnz = static_cast<size_t>(static_cast<double>(rows * cols) * density);
    row_indices.reserve(expected_nnz);
    col_indices.reserve(expected_nnz);
    values.reserve(expected_nnz);

    for (size_t i = 0; i < rows; ++i)
    {
        for (size_t j = 0; j < cols; ++j)
        {
            if (prob_dist(rng) < density)
            {
                row_indices.push_back(static_cast<IndexType>(i));
                col_indices.push_back(static_cast<IndexType>(j));
                values.push_back(static_cast<T>(val_dist(rng)));
            }
        }
    }

    return fat_p::CSRMatrix<T, IndexType>(rows, cols, row_indices, col_indices, values);
}

template <typename T, typename IndexType = int32_t>
fat_p::CSRMatrix<T, IndexType>
generate_powerlaw_sparse(size_t rows, size_t cols, size_t total_nnz, double alpha, std::mt19937& rng)
{
    std::vector<IndexType> row_indices;
    std::vector<IndexType> col_indices;
    std::vector<T> values;

    row_indices.reserve(total_nnz);
    col_indices.reserve(total_nnz);
    values.reserve(total_nnz);

    std::uniform_real_distribution<double> val_dist(-1.0, 1.0);
    std::uniform_int_distribution<size_t> col_dist(0, cols - 1);

    std::vector<double> weights(rows);
    for (size_t i = 0; i < rows; ++i)
    {
        weights[i] = std::pow(static_cast<double>(i + 1), -alpha);
    }

    double sum_weights = 0.0;
    for (double w : weights)
    {
        sum_weights += w;
    }
    for (auto& w : weights)
    {
        w = w / sum_weights * static_cast<double>(total_nnz);
    }

    std::set<std::pair<size_t, size_t>> entries;

    for (size_t i = 0; i < rows; ++i)
    {
        size_t row_nnz = static_cast<size_t>(weights[i] + 0.5);
        row_nnz = std::min(row_nnz, cols);

        size_t attempts = 0;
        size_t added = 0;
        while (added < row_nnz && attempts < row_nnz * 3)
        {
            size_t j = col_dist(rng);
            if (entries.insert({i, j}).second)
            {
                row_indices.push_back(static_cast<IndexType>(i));
                col_indices.push_back(static_cast<IndexType>(j));
                values.push_back(static_cast<T>(val_dist(rng)));
                added++;
            }
            attempts++;
        }
    }

    return fat_p::CSRMatrix<T, IndexType>(rows, cols, row_indices, col_indices, values);
}

// ============================================================================
// Unit Tests
// ============================================================================

FATP_TEST_CASE(balanced_partitions_basic)
{
    std::vector<std::size_t> row_ptrs = {0, 10, 20, 30, 40};
    auto partitions = fat_p::detail::compute_balanced_partitions(row_ptrs.data(), 4, 2);

    FATP_ASSERT_EQ(partitions.size(), 2u, "Should create 2 partitions");
    FATP_ASSERT_EQ(partitions[0].first, 0u, "First partition starts at 0");
    FATP_ASSERT_EQ(partitions[1].second, 4u, "Last partition ends at rows");

    return true;
}

FATP_TEST_CASE(balanced_partitions_uneven)
{
    std::vector<std::size_t> row_ptrs = {0, 100, 110, 120, 130};
    auto partitions = fat_p::detail::compute_balanced_partitions(row_ptrs.data(), 4, 2);

    FATP_ASSERT_GE(partitions.size(), 1u, "Should create at least 1 partition");
    FATP_ASSERT_EQ(partitions.back().second, 4u, "Last partition ends at rows");

    return true;
}

FATP_TEST_CASE(balanced_partitions_empty)
{
    std::vector<std::size_t> row_ptrs = {0};
    auto partitions = fat_p::detail::compute_balanced_partitions(row_ptrs.data(), 0, 4);

    FATP_ASSERT_EQ(partitions.size(), 0u, "Empty matrix should have no partitions");

    return true;
}

FATP_TEST_CASE(matvec_correctness_uniform)
{
    std::mt19937 rng(42);
    auto matrix = generate_random_sparse<double>(1000, 1000, 0.01, rng);

    std::vector<double> x(matrix.cols());
    std::uniform_real_distribution<double> dist(-1.0, 1.0);
    for (auto& val : x)
    {
        val = dist(rng);
    }

    std::vector<double> y_serial(matrix.rows());
    std::vector<double> y_parallel(matrix.rows());

    matrix.matvec(x.data(), y_serial.data());

    fat_p::ThreadPool pool(4);
    fat_p::matvec_threadpool(matrix, x.data(), y_parallel.data(), pool);

    double max_err = max_abs_diff(y_serial, y_parallel);
    FATP_ASSERT_LT(max_err, 1e-10, "Parallel result should match serial");

    return true;
}

FATP_TEST_CASE(matvec_correctness_powerlaw)
{
    std::mt19937 rng(123);
    auto matrix = generate_powerlaw_sparse<double>(1000, 1000, 50000, 2.0, rng);

    std::vector<double> x(matrix.cols());
    std::uniform_real_distribution<double> dist(-1.0, 1.0);
    for (auto& val : x)
    {
        val = dist(rng);
    }

    std::vector<double> y_serial(matrix.rows());
    std::vector<double> y_parallel(matrix.rows());

    matrix.matvec(x.data(), y_serial.data());

    fat_p::ThreadPool pool(4);
    fat_p::matvec_threadpool(matrix, x.data(), y_parallel.data(), pool);

    double max_err = max_abs_diff(y_serial, y_parallel);
    FATP_ASSERT_LT(max_err, 1e-10, "Parallel result should match serial for skewed matrix");

    return true;
}

FATP_TEST_CASE(matvec_alpha_beta)
{
    std::mt19937 rng(456);
    auto matrix = generate_random_sparse<double>(500, 500, 0.02, rng);

    std::vector<double> x(matrix.cols());
    std::vector<double> y_serial(matrix.rows(), 1.0);
    std::vector<double> y_parallel(matrix.rows(), 1.0);

    std::uniform_real_distribution<double> dist(-1.0, 1.0);
    for (auto& val : x)
    {
        val = dist(rng);
    }

    double alpha = 2.0;
    double beta = 0.5;

    matrix.matvec(alpha, x.data(), beta, y_serial.data());

    fat_p::ThreadPool pool(4);
    fat_p::matvec_threadpool(matrix, alpha, x.data(), beta, y_parallel.data(), pool);

    double max_err = max_abs_diff(y_serial, y_parallel);
    FATP_ASSERT_LT(max_err, 1e-10, "Alpha-beta parallel result should match serial");

    return true;
}

FATP_TEST_CASE(matvec_batch_correctness)
{
    std::mt19937 rng(789);
    auto matrix = generate_random_sparse<double>(1000, 1000, 0.01, rng);

    std::vector<double> x(matrix.cols());
    std::uniform_real_distribution<double> dist(-1.0, 1.0);
    for (auto& val : x)
    {
        val = dist(rng);
    }

    std::vector<double> y_serial(matrix.rows());
    std::vector<double> y_batch(matrix.rows());

    matrix.matvec(x.data(), y_serial.data());

    fat_p::ThreadPool pool(4);
    fat_p::matvec_threadpool_batch(matrix, x.data(), y_batch.data(), pool);

    double max_err = max_abs_diff(y_serial, y_batch);
    FATP_ASSERT_LT(max_err, 1e-10, "Batch parallel result should match serial");

    return true;
}

FATP_TEST_CASE(matvec_empty_matrix)
{
    fat_p::CSRMatrix<double> matrix(100, 100);
    std::vector<double> x(100, 1.0);
    std::vector<double> y(100, 999.0);

    fat_p::ThreadPool pool(4);
    fat_p::matvec_threadpool(matrix, x.data(), y.data(), pool);

    for (size_t i = 0; i < y.size(); ++i)
    {
        FATP_ASSERT_EQ(y[i], 0.0, "Empty matrix should produce zero output");
    }

    return true;
}

FATP_TEST_CASE(matvec_vector_interface)
{
    std::mt19937 rng(101);
    auto matrix = generate_random_sparse<double>(500, 500, 0.02, rng);

    std::vector<double> x(matrix.cols());
    std::uniform_real_distribution<double> dist(-1.0, 1.0);
    for (auto& val : x)
    {
        val = dist(rng);
    }

    fat_p::ThreadPool pool(4);
    auto y_parallel = fat_p::matvecParallel(matrix, x, pool);
    auto y_serial = matrix * x;

    double max_err = max_abs_diff(y_serial, y_parallel);
    FATP_ASSERT_LT(max_err, 1e-10, "Vector interface should match serial");

    return true;
}

FATP_TEST_CASE(transpose_parallel_correctness)
{
    std::mt19937 rng(202);
    auto matrix = generate_random_sparse<double>(500, 600, 0.02, rng);

    auto serial_transpose = matrix.transpose();

    fat_p::ThreadPool pool(4);
    auto parallel_transpose = fat_p::transpose_parallel(matrix, pool);

    FATP_ASSERT_EQ(serial_transpose.rows(), parallel_transpose.rows(), "Rows should match");
    FATP_ASSERT_EQ(serial_transpose.cols(), parallel_transpose.cols(), "Cols should match");
    FATP_ASSERT_EQ(serial_transpose.nnz(), parallel_transpose.nnz(), "NNZ should match");

    for (size_t i = 0; i < serial_transpose.rows(); ++i)
    {
        for (size_t j = 0; j < serial_transpose.cols(); ++j)
        {
            double s_val = serial_transpose(i, j);
            double p_val = parallel_transpose(i, j);
            FATP_ASSERT_CLOSE_EPS(s_val, p_val, 1e-12, "Element values should match");
        }
    }

    return true;
}

// ============================================================================

} // namespace fat_p::testing::csrmatrixparallel

namespace fat_p::testing
{

bool test_CSRMatrixParallel()
{
    FATP_PRINT_HEADER(CSRMATRIX PARALLEL)

    TestRunner runner;

    FATP_RUN_TEST_NS(runner, csrmatrixparallel, balanced_partitions_basic);
    FATP_RUN_TEST_NS(runner, csrmatrixparallel, balanced_partitions_uneven);
    FATP_RUN_TEST_NS(runner, csrmatrixparallel, balanced_partitions_empty);
    FATP_RUN_TEST_NS(runner, csrmatrixparallel, matvec_correctness_uniform);
    FATP_RUN_TEST_NS(runner, csrmatrixparallel, matvec_correctness_powerlaw);
    FATP_RUN_TEST_NS(runner, csrmatrixparallel, matvec_alpha_beta);
    FATP_RUN_TEST_NS(runner, csrmatrixparallel, matvec_batch_correctness);
    FATP_RUN_TEST_NS(runner, csrmatrixparallel, matvec_empty_matrix);
    FATP_RUN_TEST_NS(runner, csrmatrixparallel, matvec_vector_interface);
    FATP_RUN_TEST_NS(runner, csrmatrixparallel, transpose_parallel_correctness);


    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_CSRMatrixParallel() ? 0 : 1;
}
#endif
