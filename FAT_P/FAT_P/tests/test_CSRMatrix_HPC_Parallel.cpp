/**
 * @file test_CSRMatrix_HPC_Parallel.cpp
 * @brief Comprehensive unit tests for CSRMatrix_HPC_Parallel.h
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
fat_p::HpcCSRMatrix<T, IndexType> generate_random_sparse(
    size_t rows,
    size_t cols,
    double density,
    std::mt19937& rng)
{
    std::vector<IndexType> row_indices;
    std::vector<IndexType> col_indices;
    std::vector<T> values;

    std::uniform_real_distribution<double> val_dist(-1.0, 1.0);
    std::uniform_real_distribution<double> prob_dist(0.0, 1.0);

    size_t expected_nnz = static_cast<size_t>(rows * cols * density);
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
fat_p::HpcCSRMatrix<T, IndexType> generate_powerlaw_sparse(
    size_t rows,
    size_t cols,
    size_t total_nnz,
    double alpha,
    std::mt19937& rng)
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

TEST_CASE(matvec_parallel_correctness_uniform)
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
    ASSERT_LT(max_err, 1e-10, "ThreadPool result should match serial");

    return true;
}

TEST_CASE(matvec_parallel_correctness_powerlaw)
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
    ASSERT_LT(max_err, 1e-10, "ThreadPool should match serial for skewed matrix");

    return true;
}

TEST_CASE(matvec_parallel_alpha_beta)
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
    ASSERT_LT(max_err, 1e-10, "Alpha-beta ThreadPool result should match serial");

    return true;
}

TEST_CASE(matvec_parallel_batch_correctness)
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
    ASSERT_LT(max_err, 1e-10, "Batch result should match serial");

    return true;
}

TEST_CASE(matvec_parallel_empty_matrix)
{
    fat_p::HpcCSRMatrix<double> matrix(100, 100);
    std::vector<double> x(100, 1.0);
    std::vector<double> y(100, 999.0);

    fat_p::ThreadPool pool(4);
    matrix.matvec_parallel(x.data(), y.data(), pool);

    for (size_t i = 0; i < y.size(); ++i)
    {
        ASSERT_EQ(y[i], 0.0, "Empty matrix should produce zero output");
    }

    return true;
}

TEST_CASE(transpose_parallel_correctness)
{
    std::mt19937 rng(202);
    auto matrix = generate_random_sparse<double>(500, 600, 0.02, rng);

    auto serial_transpose = matrix.transpose();

    fat_p::ThreadPool pool(4);
    auto parallel_transpose = matrix.transpose_parallel(pool);

    ASSERT_EQ(serial_transpose.rows(), parallel_transpose.rows(), "Rows should match");
    ASSERT_EQ(serial_transpose.cols(), parallel_transpose.cols(), "Cols should match");
    ASSERT_EQ(serial_transpose.nnz(), parallel_transpose.nnz(), "NNZ should match");

    for (size_t i = 0; i < serial_transpose.rows(); ++i)
    {
        for (size_t j = 0; j < serial_transpose.cols(); ++j)
        {
            double s_val = serial_transpose(i, j);
            double p_val = parallel_transpose(i, j);
            ASSERT_CLOSE_EPS(s_val, p_val, 1e-12, "Element values should match");
        }
    }

    return true;
}

TEST_CASE(matvec_prefetch_toggle)
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
    ASSERT_LT(serial_err, 1e-10, "Serial prefetch toggle should not affect correctness");

    // Parallel with prefetch on/off
    fat_p::ThreadPool pool(4);

    fat_p::HpcParallelConfig config_prefetch;
    config_prefetch.use_prefetch = true;
    matrix.matvec_parallel(x.data(), y_parallel_pf.data(), pool, config_prefetch);

    fat_p::HpcParallelConfig config_no_prefetch;
    config_no_prefetch.use_prefetch = false;
    matrix.matvec_parallel(x.data(), y_parallel_no_pf.data(), pool, config_no_prefetch);

    double parallel_err = max_abs_diff(y_parallel_pf, y_parallel_no_pf);
    ASSERT_LT(parallel_err, 1e-10, "Parallel prefetch toggle should not affect correctness");

    // Cross-validate serial vs parallel
    double cross_err = max_abs_diff(y_serial_pf, y_parallel_pf);
    ASSERT_LT(cross_err, 1e-10, "Serial and parallel results should match");

    return true;
}

TEST_CASE(numa_available_check)
{
    fat_p::HpcCSRMatrix<double> matrix(10, 10);

    // This should not crash, regardless of NUMA availability
    bool numa = matrix.is_numa_available();
    (void)numa;  // Suppress unused warning

    return true;
}

TEST_CASE(default_pool_convenience_overloads)
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

    ASSERT_LT(err1, 1e-10, "Default pool parallel should match serial");
    ASSERT_LT(err2, 1e-10, "Default pool batch should match serial");

    // Test transpose convenience overload
    auto trans_serial = matrix.transpose();
    auto trans_parallel = matrix.transpose_parallel();

    ASSERT_EQ(trans_serial.nnz(), trans_parallel.nnz(), "Transpose nnz should match");

    return true;
}

// ============================================================================
// Benchmarks
// ============================================================================

void benchmark_spmv()
{
    using namespace fat_p::testing;

    std::cout << "\n" << colors::cyan() << "HpcCSRMatrix ThreadPool Benchmarks:"
              << colors::reset() << "\n\n";

    std::mt19937 rng(42);

    fat_p::ThreadPool pool;
    std::cout << "ThreadPool threads: " << pool.thread_count() << "\n";
    std::cout << "NUMA available: " << std::boolalpha
              << fat_p::HpcCSRMatrix<double>().is_numa_available() << "\n\n";

    auto run_benchmark = [&](const char* name,
                             fat_p::HpcCSRMatrix<double, int32_t>& matrix,
                             int iterations)
    {
        std::vector<double> x(matrix.cols());
        std::uniform_real_distribution<double> dist(-1.0, 1.0);
        for (auto& val : x)
        {
            val = dist(rng);
        }

        std::vector<double> y_serial(matrix.rows());
        std::vector<double> y_serial_no_pf(matrix.rows());
        std::vector<double> y_threadpool(matrix.rows());
        std::vector<double> y_batch(matrix.rows());

        std::cout << name << " (" << matrix.rows() << "x" << matrix.cols()
                  << ", nnz=" << matrix.nnz() << "):\n";

        double serial_time = measure_perf([&]()
        {
            matrix.matvec(x.data(), y_serial.data(), true);
            DoNotOptimize(y_serial.data());
        }, iterations);

        double serial_no_pf_time = measure_perf([&]()
        {
            matrix.matvec(x.data(), y_serial_no_pf.data(), false);
            DoNotOptimize(y_serial_no_pf.data());
        }, iterations);

        double threadpool_time = measure_perf([&]()
        {
            matrix.matvec_parallel(x.data(), y_threadpool.data(), pool);
            DoNotOptimize(y_threadpool.data());
        }, iterations);

        double batch_time = measure_perf([&]()
        {
            matrix.matvec_parallel_batch(x.data(), y_batch.data(), pool);
            DoNotOptimize(y_batch.data());
        }, iterations);

        std::cout << "  Serial (prefetch):    " << format_time(serial_time) << "\n";
        std::cout << "  Serial (no prefetch): " << format_time(serial_no_pf_time);
        if (serial_no_pf_time < serial_time)
        {
            std::cout << " (" << std::fixed << std::setprecision(1)
                      << ((serial_time / serial_no_pf_time - 1.0) * 100.0)
                      << "% faster)";
        }
        std::cout << "\n";

        // Use the faster serial time as baseline for parallel comparison
        double best_serial = std::min(serial_time, serial_no_pf_time);

        std::cout << "  ThreadPool: " << format_time(threadpool_time)
                  << " (" << std::fixed << std::setprecision(2)
                  << (best_serial / threadpool_time) << "x)\n";
        std::cout << "  Batch:      " << format_time(batch_time)
                  << " (" << (best_serial / batch_time) << "x)\n";

        double max_err = max_abs_diff(y_serial, y_threadpool);
        std::cout << "  Max error:  " << std::scientific << std::setprecision(2)
                  << max_err << "\n\n";
    };

    auto matrix1 = generate_random_sparse<double>(10000, 10000, 0.01, rng);
    run_benchmark("Uniform 1%", matrix1, 100);

    auto matrix2 = generate_powerlaw_sparse<double>(10000, 10000, 500000, 1.5, rng);
    run_benchmark("Power-law (alpha=1.5)", matrix2, 100);

    auto matrix3 = generate_powerlaw_sparse<double>(10000, 10000, 500000, 2.5, rng);
    run_benchmark("Power-law (alpha=2.5)", matrix3, 100);
}

} // namespace fat_p::testing::hpccsr_parallel

namespace fat_p::testing
{

bool test_CSRMatrix_HPC_Parallel()
{
    PRINT_HEADER(HPCCSRMATRIX THREADPOOL PARALLEL)

    TestRunner runner;

    RUN_TEST_NS(runner, hpccsr_parallel, matvec_parallel_correctness_uniform);
    RUN_TEST_NS(runner, hpccsr_parallel, matvec_parallel_correctness_powerlaw);
    RUN_TEST_NS(runner, hpccsr_parallel, matvec_parallel_alpha_beta);
    RUN_TEST_NS(runner, hpccsr_parallel, matvec_parallel_batch_correctness);
    RUN_TEST_NS(runner, hpccsr_parallel, matvec_parallel_empty_matrix);
    RUN_TEST_NS(runner, hpccsr_parallel, transpose_parallel_correctness);
    RUN_TEST_NS(runner, hpccsr_parallel, matvec_prefetch_toggle);
    RUN_TEST_NS(runner, hpccsr_parallel, numa_available_check);
    RUN_TEST_NS(runner, hpccsr_parallel, default_pool_convenience_overloads);

    hpccsr_parallel::benchmark_spmv();

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
