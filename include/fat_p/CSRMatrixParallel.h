#pragma once

/*
FATP_META:
  meta_version: 1
  component: CSRMatrixParallel
  file_role: public_header
  path: include/fat_p/CSRMatrixParallel.h
  namespace: fat_p
  layer: Domain
  summary: "Public header for CSRMatrixParallel."
  api_stability: in_work
  related:
    docs_search: "CSRMatrixParallel"
    tests:
      - components/CSRMatrix/tests/test_CSRMatrixParallel.cpp
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/

/**
 * @file CSRMatrixParallel.h
 * @brief ThreadPool-based parallel operations for CSRMatrix
 *
 *
 *
 * @details Extends CSRMatrix with work-balanced parallel algorithms using
 * fat_p::ThreadPool instead of OpenMP. Key advantages:
 * - Work stealing automatically balances irregular row lengths
 * - No OpenMP dependency (header-only, pure C++)
 * - Consistent with rest of fat_p library
 *
 * Performance vs OpenMP:
 * - OpenMP schedule(static): Poor for sparse matrices (load imbalance)
 * - OpenMP schedule(dynamic): Better, but still row-granularity
 * - ThreadPool: Work-balanced partitions + work stealing = best scaling
 *
 * @version 1.2
 */

#include "CSRMatrix.h"
#include "CSRMatrixPartitioning.h"
#include "ThreadPool.h"

#include <algorithm>
#include <atomic>
#include <functional>
#include <future>
#include <numeric>
#include <vector>

namespace fat_p
{

// ============================================================================
// Parallel Execution Policy
// ============================================================================

/**
 * @brief Configuration for parallel SpMV execution
 *
 * @note For matrices with fewer than ~100K non-zeros, serial execution is often
 * faster due to thread pool overhead. The min_nnz_for_parallel threshold controls
 * this automatic fallback.
 */
struct ParallelConfig
{
    std::size_t min_nnz_per_task = 8192;      // Minimum non-zeros per task (was 1024)
    std::size_t max_tasks = 0;                // 0 = use thread_count * 4
    std::size_t min_nnz_for_parallel = 50000; // Below this, use serial (new!)
    bool balance_by_nnz = true;               // true = balance by work, false = by rows
};

// ============================================================================
// Work Partitioner
// ============================================================================

// ============================================================================
// CSRMatrixParallel - ThreadPool-based Operations
// ============================================================================

/**
 * @brief Parallel SpMV using ThreadPool: y = A * x
 *
 * @details Work-balanced implementation that:
 * 1. Partitions rows by nnz (not count) for balanced work
 * 2. Submits partitions as tasks to ThreadPool
 * 3. Work stealing handles any residual imbalance
 *
 * @tparam T Value type
 * @tparam IndexType Index type
 * @param matrix The CSR matrix
 * @param x Input vector (size = cols)
 * @param y Output vector (size = rows)
 * @param pool ThreadPool to use
 * @param config Parallel execution configuration
 */
template <typename T, typename IndexType>
void matvec_threadpool(const CSRMatrix<T, IndexType>& matrix,
                       const T* x,
                       T* y,
                       ThreadPool& pool,
                       const ParallelConfig& config = {})
{
    using size_type = std::size_t;
    using ptr_type = typename CSRMatrix<T, IndexType>::ptr_type;

    const size_type rows = matrix.rows();
    const size_type nnz = matrix.nnz();

    if (rows == 0 || nnz == 0)
    {
        std::fill(y, y + rows, T{0});
        return;
    }

    // Serial fallback for small matrices (parallel overhead not worth it)
    if (nnz < config.min_nnz_for_parallel)
    {
        matrix.matvec(x, y);
        return;
    }

    size_type num_tasks = config.max_tasks;
    if (num_tasks == 0)
    {
        num_tasks = pool.thread_count() * 4;
    }

    num_tasks = std::min(num_tasks, (nnz + config.min_nnz_per_task - 1) / config.min_nnz_per_task);
    num_tasks = std::max(num_tasks, size_type{1});

    auto partitions = detail::compute_balanced_partitions(matrix.row_ptrs().data(), rows, num_tasks);

    const T* values = matrix.values().data();
    const IndexType* col_indices = matrix.col_indices().data();
    const ptr_type* row_ptrs = matrix.row_ptrs().data();

    std::vector<std::future<void>> futures;
    futures.reserve(partitions.size());

    for (const auto& partition : partitions)
    {
        size_type start_row = partition.first;
        size_type end_row = partition.second;

        futures.push_back(pool.submit([values, col_indices, row_ptrs, x, y, start_row, end_row]() {
            for (size_type i = start_row; i < end_row; ++i)
            {
                T sum = T{0};
                ptr_type j_start = row_ptrs[i];
                ptr_type j_end = row_ptrs[i + 1];

                for (ptr_type j = j_start; j < j_end; ++j)
                {
                    sum += values[j] * x[col_indices[j]];
                }

                y[i] = sum;
            }
        }));
    }

    for (auto& f : futures)
    {
        f.get();
    }
}

/**
 * @brief Parallel SpMV with alpha/beta: y = alpha * A * x + beta * y
 */
template <typename T, typename IndexType>
void matvec_threadpool(const CSRMatrix<T, IndexType>& matrix,
                       T alpha,
                       const T* x,
                       T beta,
                       T* y,
                       ThreadPool& pool,
                       const ParallelConfig& config = {})
{
    using size_type = std::size_t;
    using ptr_type = typename CSRMatrix<T, IndexType>::ptr_type;

    const size_type rows = matrix.rows();
    const size_type nnz = matrix.nnz();

    if (rows == 0)
    {
        return;
    }

    const bool beta_zero = (beta == T{0});

    if (nnz == 0)
    {
        if (beta_zero)
        {
            std::fill(y, y + rows, T{0});
        }
        else
        {
            for (size_type i = 0; i < rows; ++i)
            {
                y[i] *= beta;
            }
        }
        return;
    }

    // Serial fallback for small matrices
    if (nnz < config.min_nnz_for_parallel)
    {
        matrix.matvec(alpha, x, beta, y);
        return;
    }

    size_type num_tasks = config.max_tasks;
    if (num_tasks == 0)
    {
        num_tasks = pool.thread_count() * 4;
    }
    num_tasks = std::min(num_tasks, (nnz + config.min_nnz_per_task - 1) / config.min_nnz_per_task);
    num_tasks = std::max(num_tasks, size_type{1});

    auto partitions = detail::compute_balanced_partitions(matrix.row_ptrs().data(), rows, num_tasks);

    const T* values = matrix.values().data();
    const IndexType* col_indices = matrix.col_indices().data();
    const ptr_type* row_ptrs = matrix.row_ptrs().data();

    std::vector<std::future<void>> futures;
    futures.reserve(partitions.size());

    for (const auto& partition : partitions)
    {
        size_type start_row = partition.first;
        size_type end_row = partition.second;

        futures.push_back(
            pool.submit([values, col_indices, row_ptrs, x, y, start_row, end_row, alpha, beta, beta_zero]() {
                for (size_type i = start_row; i < end_row; ++i)
                {
                    T sum = T{0};
                    ptr_type j_start = row_ptrs[i];
                    ptr_type j_end = row_ptrs[i + 1];

                    for (ptr_type j = j_start; j < j_end; ++j)
                    {
                        sum += values[j] * x[col_indices[j]];
                    }

                    if (beta_zero)
                    {
                        y[i] = alpha * sum;
                    }
                    else
                    {
                        y[i] = alpha * sum + beta * y[i];
                    }
                }
            }));
    }

    for (auto& f : futures)
    {
        f.get();
    }
}

/**
 * @brief Batch submit version for reduced synchronization overhead
 *
 * @details Uses submit_batch + wait_idle instead of individual futures.
 * Faster for small matrices where future overhead dominates.
 */
template <typename T, typename IndexType>
void matvec_threadpool_batch(const CSRMatrix<T, IndexType>& matrix,
                             const T* x,
                             T* y,
                             ThreadPool& pool,
                             const ParallelConfig& config = {})
{
    using size_type = std::size_t;
    using ptr_type = typename CSRMatrix<T, IndexType>::ptr_type;

    const size_type rows = matrix.rows();
    const size_type nnz = matrix.nnz();

    if (rows == 0 || nnz == 0)
    {
        std::fill(y, y + rows, T{0});
        return;
    }

    // Serial fallback for small matrices
    if (nnz < config.min_nnz_for_parallel)
    {
        matrix.matvec(x, y);
        return;
    }

    size_type num_tasks = config.max_tasks;
    if (num_tasks == 0)
    {
        num_tasks = pool.thread_count() * 4;
    }
    num_tasks = std::min(num_tasks, (nnz + config.min_nnz_per_task - 1) / config.min_nnz_per_task);
    num_tasks = std::max(num_tasks, size_type{1});

    auto partitions = detail::compute_balanced_partitions(matrix.row_ptrs().data(), rows, num_tasks);

    std::vector<std::function<void()>> tasks;
    tasks.reserve(partitions.size());

    const T* values_ptr = matrix.values().data();
    const IndexType* col_indices_ptr = matrix.col_indices().data();
    const ptr_type* row_ptrs_ptr = matrix.row_ptrs().data();

    for (const auto& partition : partitions)
    {
        size_type start_row = partition.first;
        size_type end_row = partition.second;

        tasks.emplace_back([values_ptr, col_indices_ptr, row_ptrs_ptr, x, y, start_row, end_row]() {
            for (size_type i = start_row; i < end_row; ++i)
            {
                T sum = T{0};
                ptr_type j_start = row_ptrs_ptr[i];
                ptr_type j_end = row_ptrs_ptr[i + 1];

                for (ptr_type j = j_start; j < j_end; ++j)
                {
                    sum += values_ptr[j] * x[col_indices_ptr[j]];
                }

                y[i] = sum;
            }
        });
    }

    pool.submit_batch(tasks);
    pool.wait_idle();
}

// ============================================================================
// Convenience Wrappers
// ============================================================================

/**
 * @brief std::vector interface for parallel SpMV
 */
template <typename T, typename IndexType>
std::vector<T> matvecParallel(const CSRMatrix<T, IndexType>& matrix,
                               const std::vector<T>& x,
                               ThreadPool& pool,
                               const ParallelConfig& config = {})
{
    if (x.size() != matrix.cols())
    {
        throw std::invalid_argument("CSRMatrix: vector size mismatch");
    }

    std::vector<T> y(matrix.rows());
    matvec_threadpool(matrix, x.data(), y.data(), pool, config);
    return y;
}

/**
 * @brief Global ThreadPool singleton for convenience
 *
 * @details Lazily initialized. Use explicit ThreadPool for more control.
 */
inline ThreadPool& default_thread_pool()
{
    static ThreadPool pool;
    return pool;
}

/**
 * @brief Parallel SpMV using default thread pool
 */
template <typename T, typename IndexType>
std::vector<T> matvecParallel(const CSRMatrix<T, IndexType>& matrix, const std::vector<T>& x)
{
    return matvecParallel(matrix, x, default_thread_pool());
}

// ============================================================================
// Parallel Transpose
// ============================================================================

/**
 * @brief Parallel matrix transpose using ThreadPool
 *
 * @details Two-phase algorithm:
 * 1. Parallel count: Each thread counts columns in its row range
 * 2. Parallel scatter: Each thread scatters its rows into transposed positions
 *
 * The result is constructed via COO format which handles sorting internally.
 */
template <typename T, typename IndexType>
CSRMatrix<T, IndexType> transpose_parallel(const CSRMatrix<T, IndexType>& matrix, ThreadPool& pool)
{
    using size_type = std::size_t;
    using ptr_type = typename CSRMatrix<T, IndexType>::ptr_type;

    const size_type rows = matrix.rows();
    const size_type cols = matrix.cols();
    const size_type nnz = matrix.nnz();

    if (rows == 0 || cols == 0 || nnz == 0)
    {
        return CSRMatrix<T, IndexType>(cols, rows);
    }

    const T* values = matrix.values().data();
    const IndexType* col_indices = matrix.col_indices().data();
    const ptr_type* row_ptrs = matrix.row_ptrs().data();

    size_type num_threads = pool.thread_count();
    auto partitions = detail::compute_balanced_partitions(row_ptrs, rows, num_threads);

    // Phase 1: Count columns in parallel
    std::vector<std::vector<size_type>> thread_counts(partitions.size());
    for (auto& tc : thread_counts)
    {
        tc.resize(cols + 1, 0);
    }

    {
        std::vector<std::future<void>> futures;
        futures.reserve(partitions.size());

        for (size_type t = 0; t < partitions.size(); ++t)
        {
            size_type start_row = partitions[t].first;
            size_type end_row = partitions[t].second;

            futures.push_back(pool.submit([row_ptrs, col_indices, &thread_counts, t, start_row, end_row]() {
                for (size_type i = start_row; i < end_row; ++i)
                {
                    for (ptr_type j = row_ptrs[i]; j < row_ptrs[i + 1]; ++j)
                    {
                        thread_counts[t][static_cast<size_type>(col_indices[j]) + 1]++;
                    }
                }
            }));
        }

        for (auto& f : futures)
        {
            f.get();
        }
    }

    // Merge counts (serial, fast)
    std::vector<size_type> col_counts(cols + 1, 0);
    for (const auto& tc : thread_counts)
    {
        for (size_type i = 0; i <= cols; ++i)
        {
            col_counts[i] += tc[i];
        }
    }

    // Prefix sum
    for (size_type i = 0; i < cols; ++i)
    {
        col_counts[i + 1] += col_counts[i];
    }

    // Build result via COO format
    std::vector<IndexType> result_row_indices(nnz);
    std::vector<IndexType> result_col_indices(nnz);
    std::vector<T> result_values(nnz);

    // Phase 2: Scatter in parallel with atomic positions
    std::vector<std::atomic<size_type>> write_pos(cols);
    for (size_type i = 0; i < cols; ++i)
    {
        write_pos[i].store(col_counts[i], std::memory_order_relaxed);
    }

    {
        std::vector<std::future<void>> futures;
        futures.reserve(partitions.size());

        for (size_type t = 0; t < partitions.size(); ++t)
        {
            size_type start_row = partitions[t].first;
            size_type end_row = partitions[t].second;

            futures.push_back(pool.submit([values,
                                           col_indices,
                                           row_ptrs,
                                           &result_row_indices,
                                           &result_col_indices,
                                           &result_values,
                                           &write_pos,
                                           start_row,
                                           end_row]() {
                for (size_type i = start_row; i < end_row; ++i)
                {
                    for (ptr_type j = row_ptrs[i]; j < row_ptrs[i + 1]; ++j)
                    {
                        IndexType col = col_indices[j];
                        size_type dest = write_pos[static_cast<size_type>(col)].fetch_add(1, std::memory_order_relaxed);

                        result_row_indices[dest] = col;
                        result_col_indices[dest] = static_cast<IndexType>(i);
                        result_values[dest] = values[j];
                    }
                }
            }));
        }

        for (auto& f : futures)
        {
            f.get();
        }
    }

    // Construct result from COO (handles sorting internally)
    return CSRMatrix<T, IndexType>(cols, rows, result_row_indices, result_col_indices, result_values);
}

} // namespace fat_p
