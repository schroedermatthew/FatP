/**
 * @file test_CSRMatrix_HPC_Parallel.cpp
 * @brief Comprehensive unit tests for CSRMatrix_HPC_Parallel.h
 */
/*
FATP_META:
  meta_version: 1
  component: CSRMatrix_HPC_Parallel
  file_role: test
  path: components/CSRMatrix/tests/test_CSRMatrix_HPC_Parallel.cpp
  layer: Testing
  namespace: fat_p
  summary: "Unit tests for CSRMatrix_HPC_Parallel."
  api_stability: in_work
  related:
    docs_search: "CSRMatrix_HPC_Parallel"
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

#include <algorithm>
#include <iostream>
#include <random>
#include <set>

#include "CSRMatrix_HPC.h"
#include "FatPTest.h"

namespace fat_p::testing::hpccsr_parallel
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
fat_p::HpcCSRMatrix<T, IndexType> generate_random_sparse(size_t rows, size_t cols, double density, std::mt19937& rng)
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

    return fat_p::HpcCSRMatrix<T, IndexType>(rows, cols, row_indices, col_indices, values);
}

template <typename T, typename IndexType = int32_t>
fat_p::HpcCSRMatrix<T, IndexType>
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

    return fat_p::HpcCSRMatrix<T, IndexType>(rows, cols, row_indices, col_indices, values);
}

// ============================================================================
// Unit Tests
// ============================================================================

FATP_TEST_CASE(matvec_parallel_correctness_uniform)
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
    matrix.matvec_parallel(x.data(), y_parallel.data(), pool);

    double max_err = max_abs_diff(y_serial, y_parallel);
    FATP_ASSERT_LT(max_err, 1e-10, "ThreadPool result should match serial");

    return true;
}

FATP_TEST_CASE(matvec_parallel_correctness_powerlaw)
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
    matrix.matvec_parallel(x.data(), y_parallel.data(), pool);

    double max_err = max_abs_diff(y_serial, y_parallel);
    FATP_ASSERT_LT(max_err, 1e-10, "ThreadPool should match serial for skewed matrix");

    return true;
}

FATP_TEST_CASE(matvec_parallel_alpha_beta)
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
    matrix.matvec_parallel(alpha, x.data(), beta, y_parallel.data(), pool);

    double max_err = max_abs_diff(y_serial, y_parallel);
    FATP_ASSERT_LT(max_err, 1e-10, "Alpha-beta ThreadPool result should match serial");

    return true;
}

FATP_TEST_CASE(matvec_parallel_batch_correctness)
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
    matrix.matvec_parallel_batch(x.data(), y_batch.data(), pool);

    double max_err = max_abs_diff(y_serial, y_batch);
    FATP_ASSERT_LT(max_err, 1e-10, "Batch result should match serial");

    return true;
}

FATP_TEST_CASE(matvec_parallel_empty_matrix)
{
    fat_p::HpcCSRMatrix<double> matrix(100, 100);
    std::vector<double> x(100, 1.0);
    std::vector<double> y(100, 999.0);

    fat_p::ThreadPool pool(4);
    matrix.matvec_parallel(x.data(), y.data(), pool);

    for (size_t i = 0; i < y.size(); ++i)
    {
        FATP_ASSERT_EQ(y[i], 0.0, "Empty matrix should produce zero output");
    }

    return true;
}

FATP_TEST_CASE(transpose_parallel_correctness)
{
    std::mt19937 rng(202);
    auto matrix = generate_random_sparse<double>(500, 600, 0.02, rng);

    auto serial_transpose = matrix.transpose();

    fat_p::ThreadPool pool(4);
    auto parallel_transpose = matrix.transpose_parallel(pool);

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

FATP_TEST_CASE(matvec_prefetch_toggle)
{
    std::mt19937 rng(303);
    auto matrix = generate_random_sparse<double>(1000, 1000, 0.01, rng);

    std::vector<double> x(matrix.cols());
    std::uniform_real_distribution<double> dist(-1.0, 1.0);
    for (auto& val : x)
    {
        val = dist(rng);
    }

    std::vector<double> y_serial_pf(matrix.rows());
    std::vector<double> y_serial_no_pf(matrix.rows());
    std::vector<double> y_parallel_pf(matrix.rows());
    std::vector<double> y_parallel_no_pf(matrix.rows());

    // Serial with prefetch on/off
    matrix.matvec(x.data(), y_serial_pf.data(), true);
    matrix.matvec(x.data(), y_serial_no_pf.data(), false);

    double serial_err = max_abs_diff(y_serial_pf, y_serial_no_pf);
    FATP_ASSERT_LT(serial_err, 1e-10, "Serial prefetch toggle should not affect correctness");

    // Parallel with prefetch on/off
    fat_p::ThreadPool pool(4);

    fat_p::HpcParallelConfig config_prefetch;
    config_prefetch.use_prefetch = true;
    matrix.matvec_parallel(x.data(), y_parallel_pf.data(), pool, config_prefetch);

    fat_p::HpcParallelConfig config_no_prefetch;
    config_no_prefetch.use_prefetch = false;
    matrix.matvec_parallel(x.data(), y_parallel_no_pf.data(), pool, config_no_prefetch);

    double parallel_err = max_abs_diff(y_parallel_pf, y_parallel_no_pf);
    FATP_ASSERT_LT(parallel_err, 1e-10, "Parallel prefetch toggle should not affect correctness");

    // Cross-validate serial vs parallel
    double cross_err = max_abs_diff(y_serial_pf, y_parallel_pf);
    FATP_ASSERT_LT(cross_err, 1e-10, "Serial and parallel results should match");

    return true;
}

FATP_TEST_CASE(numa_available_check)
{
    fat_p::HpcCSRMatrix<double> matrix(10, 10);

    // This should not crash, regardless of NUMA availability
    bool numa = matrix.is_numa_available();
    (void)numa; // Suppress unused warning

    return true;
}

FATP_TEST_CASE(default_pool_convenience_overloads)
{
    std::mt19937 rng(999);
    auto matrix = generate_random_sparse<double>(500, 500, 0.02, rng);

    std::vector<double> x(matrix.cols());
    std::uniform_real_distribution<double> dist(-1.0, 1.0);
    for (auto& val : x)
    {
        val = dist(rng);
    }

    std::vector<double> y_serial(matrix.rows());
    std::vector<double> y_parallel(matrix.rows());
    std::vector<double> y_batch(matrix.rows());

    // Serial reference
    matrix.matvec(x.data(), y_serial.data());

    // Convenience overloads (use default pool, no explicit pool parameter)
    matrix.matvec_parallel(x.data(), y_parallel.data());
    matrix.matvec_parallel_batch(x.data(), y_batch.data());

    double err1 = max_abs_diff(y_serial, y_parallel);
    double err2 = max_abs_diff(y_serial, y_batch);

    FATP_ASSERT_LT(err1, 1e-10, "Default pool parallel should match serial");
    FATP_ASSERT_LT(err2, 1e-10, "Default pool batch should match serial");

    // Test transpose convenience overload
    auto trans_serial = matrix.transpose();
    auto trans_parallel = matrix.transpose_parallel();

    FATP_ASSERT_EQ(trans_serial.nnz(), trans_parallel.nnz(), "Transpose nnz should match");

    return true;
}

// ============================================================================

} // namespace fat_p::testing::hpccsr_parallel

namespace fat_p::testing
{

bool test_CSRMatrix_HPC_Parallel()
{
    FATP_PRINT_HEADER(HPCCSRMATRIX THREADPOOL PARALLEL)

    TestRunner runner;

    FATP_RUN_TEST_NS(runner, hpccsr_parallel, matvec_parallel_correctness_uniform);
    FATP_RUN_TEST_NS(runner, hpccsr_parallel, matvec_parallel_correctness_powerlaw);
    FATP_RUN_TEST_NS(runner, hpccsr_parallel, matvec_parallel_alpha_beta);
    FATP_RUN_TEST_NS(runner, hpccsr_parallel, matvec_parallel_batch_correctness);
    FATP_RUN_TEST_NS(runner, hpccsr_parallel, matvec_parallel_empty_matrix);
    FATP_RUN_TEST_NS(runner, hpccsr_parallel, transpose_parallel_correctness);
    FATP_RUN_TEST_NS(runner, hpccsr_parallel, matvec_prefetch_toggle);
    FATP_RUN_TEST_NS(runner, hpccsr_parallel, numa_available_check);
    FATP_RUN_TEST_NS(runner, hpccsr_parallel, default_pool_convenience_overloads);


    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_CSRMatrix_HPC_Parallel() ? 0 : 1;
}
#endif
