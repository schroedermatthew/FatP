/**
 * @file CSRMatrix_HPC.h
 * @brief High-Performance CSR Matrix with NUMA locality, cache prefetching, and contract enforcement
 * 
 *
 * @layer Domain
 *
 * @version 1.1
 *
 * Enhanced version of CSRMatrix optimized for HPC workloads using Fat-P components:
 *
 * Performance Enhancements:
 * - HpcVector storage: NUMA-local allocation + cache-line alignment
 * - Software prefetching: Reduces memory latency in SpMV by 20-40%
 * - assume_aligned() hints: Enables compiler auto-vectorization
 * - NUMA-local workspaces: Avoids remote memory access in matmul/transpose
 *
 * Safety Enhancements:
 * - Enforce contracts: Debug-only precondition checking (zero overhead in release)
 * - CheckedArithmetic: Overflow-safe dimension calculations
 *
 * Design Notes:
 * - HpcCSRMatrix is immutable after construction (no set() method) to align with
 *   HpcVector's static allocation philosophy. Use COO constructor for building.
 * - Contract violations in release builds result in undefined behavior (by design)
 *   for maximum performance. Use debug builds during development.
 *
 * Usage:
 * @code
 * // Drop-in replacement for CSRMatrix
 * HpcCSRMatrix<double> A(1000, 1000, rows, cols, vals);
 * 
 * // Check NUMA support
 * if (A.is_numa_available()) {
 *     std::cout << "NUMA-aware allocation enabled\n";
 * }
 * 
 * // SpMV with automatic prefetching
 * std::vector<double> x(1000, 1.0), y(1000);
 * A.matvec(x.data(), y.data());
 * @endcode
 *
 * @see CSRMatrix.h for the standard version without HPC dependencies
 * @see HpcVector.h for NUMA + alignment details
 * @see CacheUtilities.h for prefetching details
 *
 * Dependencies:
 * - HpcVector.h (NUMA-aware, cache-aligned storage)
 * - enforce.h (contract checking)
 * - CacheUtilities.h (prefetching)
 * - CheckedArithmetic.h (overflow-safe arithmetic)
 *
 * Requires: C++17
 */

#pragma once

/*
FATP_META:
  meta_version: 1
  component: CSRMatrix_HPC
  file_role: public_header
  path: fat_p/CSRMatrix_HPC.h
  namespace: fat_p
  layer: Domain
  summary: "Public header for CSRMatrix_HPC."
  api_stability: in_work
  related:
    docs_search: "CSRMatrix_HPC"
    tests:
      - tests/test_CSRMatrix_HPC.cpp
      - tests/test_CSRMatrix_HPC_Parallel.cpp
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
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <numeric>
#include <ostream>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

// Fat-P HPC components
#include "HpcVector.h"
#include "enforce.h"
#include "CacheUtilities.h"
#include "CheckedArithmetic.h"
#include "FatPTypeTraits.h"
#include "ThreadPool.h"
#include "CSRMatrixPartitioning.h"

#include <future>

namespace fat_p
{

// ============================================================================
// Parallel Execution Configuration
// ============================================================================

/**
 * @brief Configuration for ThreadPool-based parallel SpMV execution
 *
 * @note For matrices with fewer than ~100K non-zeros, serial execution is often
 * faster due to thread pool overhead. The min_nnz_for_parallel threshold controls
 * this automatic fallback.
 */
struct HpcParallelConfig
{
    size_t min_nnz_per_task = 8192;      // Minimum non-zeros per task (was 1024)
    size_t max_tasks = 0;                // 0 = thread_count * 4
    size_t min_nnz_for_parallel = 50000; // Below this, use serial (new!)
    bool use_prefetch = true;            // Enable software prefetching
};

/**
 * @brief Default ThreadPool singleton for HpcCSRMatrix operations
 *
 * @details Lazily initialized with hardware_concurrency threads.
 * Use explicit ThreadPool for more control over thread count or affinity.
 */
inline ThreadPool& default_hpc_thread_pool()
{
    static ThreadPool pool;
    return pool;
}

// ============================================================================
// Work Partitioner (Internal)
// ============================================================================


/**
 * @brief High-Performance Compressed Sparse Row matrix
 * 
 * Enhanced CSRMatrix with NUMA-local storage, cache prefetching, and contract enforcement.
 * 
 * @tparam T Value type (must be arithmetic: float, double, signed int, etc.)
 * @tparam IndexType Index type for column indices (int32_t, int64_t)
 * @tparam Alignment Cache line alignment (default 64 bytes)
 * @tparam NumaPolicy NUMA allocation policy (default: local node)
 */
template <typename T, 
          typename IndexType = int32_t,
          std::size_t Alignment = 64,
          typename NumaPolicy = memory::NumaLocalPolicy>
class HpcCSRMatrix
{
    static_assert(std::is_arithmetic_v<T>, "HpcCSRMatrix requires arithmetic value type");
    static_assert(!std::is_same_v<T, bool>,
                  "HpcCSRMatrix does not support bool: std::vector<bool> is bit-packed "
                  "and lacks contiguous memory access via .data()");
    static_assert(!std::is_integral_v<T> || std::is_signed_v<T>,
                  "HpcCSRMatrix: Unsigned integer value types are not supported "
                  "(subtraction may produce negative intermediate values)");
    static_assert(std::is_integral_v<IndexType>, "HpcCSRMatrix requires integral index type");
    static_assert(std::is_signed_v<IndexType>,
                  "HpcCSRMatrix requires signed index type for bounds checking with negative values");

public:
    using value_type = T;
    using index_type = IndexType;
    using size_type = std::size_t;
    using ptr_type = std::size_t;
    
    // HPC storage types
    using value_vector = HpcVector<T, Alignment, NumaPolicy>;
    using index_vector = HpcVector<IndexType, Alignment, NumaPolicy>;
    using ptr_vector = HpcVector<ptr_type, Alignment, NumaPolicy>;

    static constexpr std::size_t alignment = Alignment;

    /**
     * @brief Policy for handling duplicate (row, col) entries in COO input
     */
    enum class DuplicatePolicy
    {
        Sum,   ///< Sum duplicate values (standard behavior, default)
        Keep,  ///< Keep all duplicates as separate entries (summed on access)
        Error  ///< Throw exception if duplicates exist
    };

private:
    size_type mRows;
    size_type mCols;
    value_vector mValues;
    index_vector col_indices_;
    ptr_vector row_ptrs_;

    // =========================================================================
    // Private Helpers
    // =========================================================================

    template <typename U = T>
    [[nodiscard]] static constexpr bool is_effectively_zero(U val) noexcept
    {
        if constexpr (std::is_floating_point_v<U>)
        {
            using std::abs;
            return abs(val) <= std::numeric_limits<U>::epsilon() * U{10};
        }
        else
        {
            return val == U{0};
        }
    }

    template <typename U = T>
    [[nodiscard]] static constexpr U default_epsilon() noexcept
    {
        if constexpr (std::is_floating_point_v<U>)
        {
            return std::numeric_limits<U>::epsilon() * U{100};
        }
        else
        {
            return U{0};
        }
    }

    // Checked dimension validation using CheckedArithmetic
    static void validate_dimensions(size_type rows, size_type cols)
    {
        constexpr auto idx_max = static_cast<size_type>(std::numeric_limits<IndexType>::max());
        if (rows > idx_max)
        {
            throw std::overflow_error(
                "HpcCSRMatrix: Row count exceeds IndexType limits (use larger IndexType)");
        }
        if (cols > idx_max)
        {
            throw std::overflow_error(
                "HpcCSRMatrix: Column count exceeds IndexType limits (use larger IndexType)");
        }
    }

    // Checked overflow for rows * cols using CheckedArithmetic
    static void validate_dense_size(size_type rows, size_type cols)
    {
        if (rows > 0 && cols > 0)
        {
            // Explicit alias ensures correct symbol binding regardless of header order
            using OverflowPolicy = ReturnExpectedPolicy;
            auto result = checked_mul<OverflowPolicy>(rows, cols);
            if (!result.has_value())
            {
                throw std::overflow_error("HpcCSRMatrix: matrix too large (rows * cols overflow)");
            }
        }
    }

public:
    // =========================================================================
    // Constructors
    // =========================================================================

    HpcCSRMatrix() : mRows(0), mCols(0)
    {
        row_ptrs_.push_back(0);
    }

    HpcCSRMatrix(const HpcCSRMatrix&) = default;
    HpcCSRMatrix& operator=(const HpcCSRMatrix&) = default;
    HpcCSRMatrix(HpcCSRMatrix&&) noexcept = default;
    HpcCSRMatrix& operator=(HpcCSRMatrix&&) noexcept = default;
    ~HpcCSRMatrix() = default;

    /**
     * @brief Construct empty matrix with given dimensions
     */
    explicit HpcCSRMatrix(size_type rows, size_type cols)
        : mRows(rows), mCols(cols)
    {
        validate_dimensions(rows, cols);
        row_ptrs_.resize(rows + 1, 0);
    }

    /**
     * @brief Construct from COO format with duplicate handling
     */
    template <typename RowContainer, typename ColContainer, typename ValContainer>
    HpcCSRMatrix(size_type rows,
                 size_type cols,
                 const RowContainer& row_indices,
                 const ColContainer& col_indices,
                 const ValContainer& values,
                 DuplicatePolicy dup_policy = DuplicatePolicy::Sum)
        : mRows(rows), mCols(cols)
    {
        validate_dimensions(rows, cols);

        FATP_ALWAYS_ENFORCE(row_indices.size() == col_indices.size() &&
                       col_indices.size() == values.size(),
                       "HpcCSRMatrix: COO arrays must have same size");

        size_type nnz_input = values.size();
        row_ptrs_.resize(rows + 1, 0);

        if (nnz_input == 0)
        {
            return;
        }

        // Create sorted indices
        std::vector<size_type> perm(nnz_input);
        std::iota(perm.begin(), perm.end(), size_type{0});

        std::sort(perm.begin(), perm.end(), [&](size_type a, size_type b) {
            auto ra = row_indices[a];
            auto rb = row_indices[b];
            if (ra != rb) return ra < rb;
            return col_indices[a] < col_indices[b];
        });

        // Validate bounds
        for (size_type i = 0; i < nnz_input; ++i)
        {
            auto r = row_indices[perm[i]];
            auto c = col_indices[perm[i]];
            FATP_ALWAYS_ENFORCE(r >= 0 && static_cast<size_type>(r) < rows,
                           "HpcCSRMatrix: row index out of range");
            FATP_ALWAYS_ENFORCE(c >= 0 && static_cast<size_type>(c) < cols,
                           "HpcCSRMatrix: column index out of range");
        }

        // Build CSR based on duplicate policy
        if (dup_policy == DuplicatePolicy::Keep)
        {
            mValues.reserve(nnz_input);
            col_indices_.reserve(nnz_input);

            for (size_type i = 0; i < nnz_input; ++i)
            {
                size_type idx = perm[i];
                T val = values[idx];

                if (!is_effectively_zero(val))
                {
                    size_type r = static_cast<size_type>(row_indices[idx]);
                    mValues.push_back(val);
                    col_indices_.push_back(static_cast<IndexType>(col_indices[idx]));
                    row_ptrs_[r + 1]++;
                }
            }
        }
        else
        {
            mValues.reserve(nnz_input);
            col_indices_.reserve(nnz_input);

            size_type i = 0;
            while (i < nnz_input)
            {
                size_type idx = perm[i];
                auto cur_row = row_indices[idx];
                auto cur_col = col_indices[idx];
                T sum = values[idx];

                size_type j = i + 1;
                while (j < nnz_input)
                {
                    size_type jdx = perm[j];
                    if (row_indices[jdx] != cur_row || col_indices[jdx] != cur_col)
                    {
                        break;
                    }

                    if (dup_policy == DuplicatePolicy::Error)
                    {
                        throw std::invalid_argument(
                            "HpcCSRMatrix: duplicate entry found with DuplicatePolicy::Error");
                    }

                    sum += values[jdx];
                    ++j;
                }

                if (!is_effectively_zero(sum))
                {
                    size_type r = static_cast<size_type>(cur_row);
                    mValues.push_back(sum);
                    col_indices_.push_back(static_cast<IndexType>(cur_col));
                    row_ptrs_[r + 1]++;
                }

                i = j;
            }
        }

        // Cumulative sum for row_ptrs_
        for (size_type i = 0; i < mRows; ++i)
        {
            row_ptrs_[i + 1] += row_ptrs_[i];
        }
    }

    /**
     * @brief Create from dense matrix
     */
    [[nodiscard]] static HpcCSRMatrix from_dense(const T* dense,
                                                  size_type rows,
                                                  size_type cols,
                                                  T epsilon = T{0})
    {
        FATP_ALWAYS_ENFORCE(dense != nullptr || (rows == 0 || cols == 0),
                       "HpcCSRMatrix: null pointer to dense matrix");

        validate_dimensions(rows, cols);
        validate_dense_size(rows, cols);

        HpcCSRMatrix result(rows, cols);
        result.mValues.reserve(rows * std::min(cols, size_type{16}));
        result.col_indices_.reserve(rows * std::min(cols, size_type{16}));

        using std::abs;
        for (size_type i = 0; i < rows; ++i)
        {
            // Prefetch next row while processing current
            if (i + 1 < rows)
            {
                perf::prefetch<perf::PrefetchLocality::Moderate>(dense + (i + 1) * cols);
            }

            for (size_type j = 0; j < cols; ++j)
            {
                T val = dense[i * cols + j];
                bool is_nonzero = false;

                if constexpr (std::is_floating_point_v<T>)
                {
                    is_nonzero = abs(val) > epsilon;
                }
                else
                {
                    is_nonzero = val != T{0};
                }

                if (is_nonzero)
                {
                    result.mValues.push_back(val);
                    result.col_indices_.push_back(static_cast<IndexType>(j));
                }
            }
            result.row_ptrs_[i + 1] = result.mValues.size();
        }

        return result;
    }

    // =========================================================================
    // Accessors
    // =========================================================================

    [[nodiscard]] size_type rows() const noexcept { return mRows; }
    [[nodiscard]] size_type cols() const noexcept { return mCols; }
    [[nodiscard]] size_type nnz() const noexcept { return mValues.size(); }
    [[nodiscard]] bool empty() const noexcept { return mValues.empty(); }

    [[nodiscard]] const value_vector& values() const noexcept { return mValues; }
    [[nodiscard]] const index_vector& col_indices() const noexcept { return col_indices_; }
    [[nodiscard]] const ptr_vector& row_ptrs() const noexcept { return row_ptrs_; }

    // =========================================================================
    // HPC-Specific Accessors
    // =========================================================================

    /**
     * @brief Check if NUMA-aware allocation is available on this platform
     * @return true if NUMA APIs are available and the matrix uses NUMA-local storage
     * @note This indicates the platform supports NUMA, not necessarily that data
     *       is on the calling thread's local node. Use with NumaLocalPolicy for
     *       guaranteed local placement when threads are properly bound.
     */
    [[nodiscard]] bool is_numa_available() const noexcept
    {
        return mValues.is_numa_available();
    }

    /**
     * @brief Get aligned pointer to values for SIMD operations
     * @return Pointer with alignment hint for compiler auto-vectorization
     */
    [[nodiscard]] T* values_aligned() noexcept
    {
        return mValues.assume_aligned();
    }

    [[nodiscard]] const T* values_aligned() const noexcept
    {
        // Safe: assume_aligned() is logically const (returns pointer, no mutation)
        return const_cast<value_vector&>(mValues).assume_aligned();
    }

    /**
     * @brief Get aligned pointer to column indices for SIMD operations
     * @return Pointer with alignment hint for compiler auto-vectorization
     */
    [[nodiscard]] IndexType* col_indices_aligned() noexcept
    {
        return col_indices_.assume_aligned();
    }

    [[nodiscard]] const IndexType* col_indices_aligned() const noexcept
    {
        // Safe: assume_aligned() is logically const (returns pointer, no mutation)
        return const_cast<index_vector&>(col_indices_).assume_aligned();
    }

    // =========================================================================
    // Element Access
    // =========================================================================

    /**
     * @brief Get element at (row, col)
     */
    [[nodiscard]] T operator()(size_type row, size_type col) const
    {
        FATP_ALWAYS_ENFORCE(row < mRows, "HpcCSRMatrix: row index out of range: ", row, " >= ", mRows);
        FATP_ALWAYS_ENFORCE(col < mCols, "HpcCSRMatrix: col index out of range: ", col, " >= ", mCols);

        ptr_type start = row_ptrs_[row];
        ptr_type end = row_ptrs_[row + 1];

        T sum = T{0};
        IndexType target_col = static_cast<IndexType>(col);

        for (ptr_type j = start; j < end; ++j)
        {
            if (col_indices_[j] == target_col)
            {
                sum += mValues[j];
            }
            else if (col_indices_[j] > target_col)
            {
                break;
            }
        }

        return sum;
    }

    // =========================================================================
    // Matrix-Vector Operations (with Optional Prefetching)
    // =========================================================================

    /**
     * @brief Sparse matrix-vector multiply: y = A * x
     * 
     * @param x Input vector (size = cols)
     * @param y Output vector (size = rows)
     * @param use_prefetch Enable software prefetching (default true). Disable on
     *        CPUs with aggressive hardware prefetchers (e.g., Intel Arrow Lake)
     *        where software prefetch adds overhead without benefit.
     */
    void matvec(const T* x, T* y, bool use_prefetch = true) const
    {
        FATP_ENFORCE(x != nullptr, "HpcCSRMatrix::matvec: x pointer is null");
        FATP_ENFORCE(y != nullptr, "HpcCSRMatrix::matvec: y pointer is null");
        FATP_ENFORCE(x != y, "HpcCSRMatrix::matvec: x and y must not alias (use separate buffers)");

        const T* vals = mValues.data();
        const IndexType* cols = col_indices_.data();
        const ptr_type* ptrs = row_ptrs_.data();

        for (size_type i = 0; i < mRows; ++i)
        {
            T sum = T{0};
            ptr_type start = ptrs[i];
            ptr_type end = ptrs[i + 1];

            // Prefetch next row's data
            if (use_prefetch && i + 1 < mRows)
            {
                ptr_type next_start = ptrs[i + 1];
                ptr_type next_end = ptrs[i + 2];
                if (next_start < next_end)
                {
                    perf::prefetch<perf::PrefetchLocality::High>(&vals[next_start]);
                    perf::prefetch<perf::PrefetchLocality::High>(&cols[next_start]);
                }
            }

            // Inner product with prefetching of x vector
            for (ptr_type j = start; j < end; ++j)
            {
                // Prefetch x entries for upcoming iterations
                if (use_prefetch && j + 4 < end)
                {
                    perf::prefetch<perf::PrefetchLocality::Low>(&x[cols[j + 4]]);
                }
                sum += vals[j] * x[cols[j]];
            }
            y[i] = sum;
        }
    }

    /**
     * @brief Sparse matrix-vector multiply: y = alpha * A * x + beta * y
     * 
     * @param alpha Scalar multiplier for A * x
     * @param x Input vector (size = cols)
     * @param beta Scalar multiplier for y (beta=0 handled specially to avoid NaN)
     * @param y Input/output vector (size = rows)
     * @param use_prefetch Enable software prefetching (default true). Disable on
     *        CPUs with aggressive hardware prefetchers (e.g., Intel Arrow Lake)
     *        where software prefetch adds overhead without benefit.
     */
    void matvec(T alpha, const T* x, T beta, T* y, bool use_prefetch = true) const
    {
        FATP_ENFORCE(x != nullptr, "HpcCSRMatrix::matvec: x pointer is null");
        FATP_ENFORCE(y != nullptr, "HpcCSRMatrix::matvec: y pointer is null");
        FATP_ENFORCE(x != y, "HpcCSRMatrix::matvec: x and y must not alias (use separate buffers)");

        const T* vals = mValues.data();
        const IndexType* cols = col_indices_.data();
        const ptr_type* ptrs = row_ptrs_.data();

        for (size_type i = 0; i < mRows; ++i)
        {
            T sum = T{0};
            ptr_type start = ptrs[i];
            ptr_type end = ptrs[i + 1];

            // Prefetch next row's data
            if (use_prefetch && i + 1 < mRows)
            {
                ptr_type next_start = ptrs[i + 1];
                ptr_type next_end = ptrs[i + 2];
                if (next_start < next_end)
                {
                    perf::prefetch<perf::PrefetchLocality::High>(&vals[next_start]);
                    perf::prefetch<perf::PrefetchLocality::High>(&cols[next_start]);
                }
            }

            for (ptr_type j = start; j < end; ++j)
            {
                if (use_prefetch && j + 4 < end)
                {
                    perf::prefetch<perf::PrefetchLocality::Low>(&x[cols[j + 4]]);
                }
                sum += vals[j] * x[cols[j]];
            }

            // NaN-safe beta handling
            if (beta == T{0})
            {
                y[i] = alpha * sum;
            }
            else
            {
                y[i] = alpha * sum + beta * y[i];
            }
        }
    }

    /**
     * @brief Convenience overload for std::vector
     */
    void matvec(T alpha, const std::vector<T>& x, T beta, std::vector<T>& y,
                bool use_prefetch = true) const
    {
        FATP_ALWAYS_ENFORCE(x.size() == mCols, "HpcCSRMatrix: x size mismatch");
        FATP_ALWAYS_ENFORCE(y.size() == mRows, "HpcCSRMatrix: y size mismatch");
        matvec(alpha, x.data(), beta, y.data(), use_prefetch);
    }

    /**
     * @brief Sparse matrix-vector multiply returning std::vector
     */
    [[nodiscard]] std::vector<T> operator*(const std::vector<T>& x) const
    {
        FATP_ALWAYS_ENFORCE(x.size() == mCols, "HpcCSRMatrix: vector size mismatch");
        std::vector<T> y(mRows);
        matvec(x.data(), y.data());
        return y;
    }

    // =========================================================================
    // Parallel Operations (ThreadPool-Based, Work-Balanced)
    // =========================================================================

    /**
     * @brief Parallel SpMV using ThreadPool with work-balanced partitioning
     *
     * @details Combines three optimization strategies:
     * 1. Work-balanced partitioning: Rows grouped by nnz, not count
     * 2. Work stealing: ThreadPool handles residual imbalance
     * 3. Software prefetching: Reduces memory latency within each task
     *
     * This typically outperforms OpenMP schedule(static) on matrices with
     * irregular row lengths (power-law degree distributions, etc).
     *
     * @param x Input vector (size = cols)
     * @param y Output vector (size = rows)
     * @param pool ThreadPool to use for parallel execution
     * @param config Parallel execution configuration
     */
    void matvec_parallel(
        const T* x,
        T* y,
        ThreadPool& pool,
        const HpcParallelConfig& config = {}) const
    {
        FATP_ENFORCE(x != nullptr, "HpcCSRMatrix::matvec_parallel: x pointer is null");
        FATP_ENFORCE(y != nullptr, "HpcCSRMatrix::matvec_parallel: y pointer is null");
        FATP_ENFORCE(x != y, "HpcCSRMatrix::matvec_parallel: x and y must not alias");

        const size_type n_rows = mRows;
        const size_type n_nnz = nnz();

        if (n_rows == 0 || n_nnz == 0)
        {
            std::fill(y, y + n_rows, T{0});
            return;
        }

        // Serial fallback for small matrices (parallel overhead not worth it)
        if (n_nnz < config.min_nnz_for_parallel)
        {
            matvec(x, y);
            return;
        }

        size_t num_tasks = config.max_tasks;
        if (num_tasks == 0)
        {
            num_tasks = pool.thread_count() * 4;
        }
        num_tasks = std::min(num_tasks,
                             (n_nnz + config.min_nnz_per_task - 1) / config.min_nnz_per_task);
        num_tasks = std::max(num_tasks, std::size_t{1});

        auto partitions = detail::compute_balanced_partitions(
            row_ptrs_.data(), n_rows, num_tasks);

        const T* vals = mValues.data();
        const IndexType* cols = col_indices_.data();
        const ptr_type* ptrs = row_ptrs_.data();
        const bool do_prefetch = config.use_prefetch;

        std::vector<std::future<void>> futures;
        futures.reserve(partitions.size());

        for (const auto& partition : partitions)
        {
            size_type start_row = partition.first;
            size_type end_row = partition.second;

            futures.push_back(pool.submit(
                [vals, cols, ptrs, x, y, start_row, end_row, do_prefetch]()
            {
                for (size_type i = start_row; i < end_row; ++i)
                {
                    T sum = T{0};
                    ptr_type start = ptrs[i];
                    ptr_type end = ptrs[i + 1];

                    // Prefetch next row's data
                    if (do_prefetch && i + 1 < end_row)
                    {
                        ptr_type next_start = ptrs[i + 1];
                        ptr_type next_end = ptrs[i + 2];
                        if (next_start < next_end)
                        {
                            perf::prefetch<perf::PrefetchLocality::High>(&vals[next_start]);
                            perf::prefetch<perf::PrefetchLocality::High>(&cols[next_start]);
                        }
                    }

                    for (ptr_type j = start; j < end; ++j)
                    {
                        if (do_prefetch && j + 4 < end)
                        {
                            perf::prefetch<perf::PrefetchLocality::Low>(&x[cols[j + 4]]);
                        }
                        sum += vals[j] * x[cols[j]];
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
     * @brief Parallel SpMV: y = alpha * A * x + beta * y using ThreadPool
     */
    void matvec_parallel(
        T alpha,
        const T* x,
        T beta,
        T* y,
        ThreadPool& pool,
        const HpcParallelConfig& config = {}) const
    {
        FATP_ENFORCE(x != nullptr, "HpcCSRMatrix::matvec_parallel: x pointer is null");
        FATP_ENFORCE(y != nullptr, "HpcCSRMatrix::matvec_parallel: y pointer is null");
        FATP_ENFORCE(x != y, "HpcCSRMatrix::matvec_parallel: x and y must not alias");

        const size_type n_rows = mRows;
        const size_type n_nnz = nnz();

        if (n_rows == 0)
        {
            return;
        }

        const bool beta_zero = (beta == T{0});

        if (n_nnz == 0)
        {
            if (beta_zero)
            {
                std::fill(y, y + n_rows, T{0});
            }
            else
            {
                for (size_type i = 0; i < n_rows; ++i)
                {
                    y[i] *= beta;
                }
            }
            return;
        }

        // Serial fallback for small matrices (parallel overhead not worth it)
        if (n_nnz < config.min_nnz_for_parallel)
        {
            matvec(alpha, x, beta, y);
            return;
        }

        size_t num_tasks = config.max_tasks;
        if (num_tasks == 0)
        {
            num_tasks = pool.thread_count() * 4;
        }
        num_tasks = std::min(num_tasks,
                             (n_nnz + config.min_nnz_per_task - 1) / config.min_nnz_per_task);
        num_tasks = std::max(num_tasks, std::size_t{1});

        auto partitions = detail::compute_balanced_partitions(
            row_ptrs_.data(), n_rows, num_tasks);

        const T* vals = mValues.data();
        const IndexType* cols = col_indices_.data();
        const ptr_type* ptrs = row_ptrs_.data();
        const bool do_prefetch = config.use_prefetch;

        std::vector<std::future<void>> futures;
        futures.reserve(partitions.size());

        for (const auto& partition : partitions)
        {
            size_type start_row = partition.first;
            size_type end_row = partition.second;

            futures.push_back(pool.submit(
                [vals, cols, ptrs, x, y, start_row, end_row, alpha, beta, beta_zero, do_prefetch]()
            {
                for (size_type i = start_row; i < end_row; ++i)
                {
                    T sum = T{0};
                    ptr_type start = ptrs[i];
                    ptr_type end = ptrs[i + 1];

                    if (do_prefetch && i + 1 < end_row)
                    {
                        ptr_type next_start = ptrs[i + 1];
                        ptr_type next_end = ptrs[i + 2];
                        if (next_start < next_end)
                        {
                            perf::prefetch<perf::PrefetchLocality::High>(&vals[next_start]);
                            perf::prefetch<perf::PrefetchLocality::High>(&cols[next_start]);
                        }
                    }

                    for (ptr_type j = start; j < end; ++j)
                    {
                        if (do_prefetch && j + 4 < end)
                        {
                            perf::prefetch<perf::PrefetchLocality::Low>(&x[cols[j + 4]]);
                        }
                        sum += vals[j] * x[cols[j]];
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
     * @brief Batch-submit version for reduced synchronization overhead
     *
     * @details Uses submit_batch + wait_idle instead of individual futures.
     * Lower overhead for smaller matrices where future creation dominates.
     */
    void matvec_parallel_batch(
        const T* x,
        T* y,
        ThreadPool& pool,
        const HpcParallelConfig& config = {}) const
    {
        FATP_ENFORCE(x != nullptr, "HpcCSRMatrix::matvec_parallel_batch: x pointer is null");
        FATP_ENFORCE(y != nullptr, "HpcCSRMatrix::matvec_parallel_batch: y pointer is null");
        FATP_ENFORCE(x != y, "HpcCSRMatrix::matvec_parallel_batch: x and y must not alias");

        const size_type n_rows = mRows;
        const size_type n_nnz = nnz();

        if (n_rows == 0 || n_nnz == 0)
        {
            std::fill(y, y + n_rows, T{0});
            return;
        }

        // Serial fallback for small matrices (parallel overhead not worth it)
        if (n_nnz < config.min_nnz_for_parallel)
        {
            matvec(x, y);
            return;
        }

        size_t num_tasks = config.max_tasks;
        if (num_tasks == 0)
        {
            num_tasks = pool.thread_count() * 4;
        }
        num_tasks = std::min(num_tasks,
                             (n_nnz + config.min_nnz_per_task - 1) / config.min_nnz_per_task);
        num_tasks = std::max(num_tasks, std::size_t{1});

        auto partitions = detail::compute_balanced_partitions(
            row_ptrs_.data(), n_rows, num_tasks);

        const T* vals = mValues.data();
        const IndexType* cols = col_indices_.data();
        const ptr_type* ptrs = row_ptrs_.data();
        const bool do_prefetch = config.use_prefetch;

        std::vector<std::function<void()>> tasks;
        tasks.reserve(partitions.size());

        for (const auto& partition : partitions)
        {
            size_type start_row = partition.first;
            size_type end_row = partition.second;

            tasks.emplace_back(
                [vals, cols, ptrs, x, y, start_row, end_row, do_prefetch]()
            {
                for (size_type i = start_row; i < end_row; ++i)
                {
                    T sum = T{0};
                    ptr_type start = ptrs[i];
                    ptr_type end = ptrs[i + 1];

                    if (do_prefetch && i + 1 < end_row)
                    {
                        ptr_type next_start = ptrs[i + 1];
                        ptr_type next_end = ptrs[i + 2];
                        if (next_start < next_end)
                        {
                            perf::prefetch<perf::PrefetchLocality::High>(&vals[next_start]);
                            perf::prefetch<perf::PrefetchLocality::High>(&cols[next_start]);
                        }
                    }

                    for (ptr_type j = start; j < end; ++j)
                    {
                        if (do_prefetch && j + 4 < end)
                        {
                            perf::prefetch<perf::PrefetchLocality::Low>(&x[cols[j + 4]]);
                        }
                        sum += vals[j] * x[cols[j]];
                    }
                    y[i] = sum;
                }
            });
        }

        pool.submit_batch(tasks);
        pool.wait_idle();
    }

    /**
     * @brief Parallel transpose using ThreadPool
     *
     * @details Two-phase algorithm:
     * 1. Parallel count: Each task counts columns in its row partition
     * 2. Parallel scatter: Each task scatters values to transposed positions
     *
     * Uses atomic increments for thread-safe position allocation.
     */
    [[nodiscard]] HpcCSRMatrix transpose_parallel(ThreadPool& pool) const
    {
        HpcCSRMatrix result(mCols, mRows);

        if (nnz() == 0)
        {
            return result;
        }

        const size_type n_rows = mRows;
        const size_type n_cols = mCols;
        const size_type n_nnz = nnz();

        size_t num_threads = pool.thread_count();
        auto partitions = detail::compute_balanced_partitions(
            row_ptrs_.data(), n_rows, num_threads);

        // Phase 1: Count columns in parallel
        std::vector<std::vector<ptr_type>> thread_counts(partitions.size());
        for (auto& tc : thread_counts)
        {
            tc.resize(n_cols + 1, 0);
        }

        {
            std::vector<std::future<void>> futures;
            futures.reserve(partitions.size());

            for (size_t t = 0; t < partitions.size(); ++t)
            {
                size_type start_row = partitions[t].first;
                size_type end_row = partitions[t].second;

                futures.push_back(pool.submit([this, &thread_counts, t, start_row, end_row]()
                {
                    for (size_type i = start_row; i < end_row; ++i)
                    {
                        for (ptr_type j = row_ptrs_[i]; j < row_ptrs_[i + 1]; ++j)
                        {
                            thread_counts[t][col_indices_[j] + 1]++;
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
        memory::NumaLocalVector<ptr_type> col_counts(n_cols + 1, 0);
        for (const auto& tc : thread_counts)
        {
            for (size_type i = 0; i <= n_cols; ++i)
            {
                col_counts[i] += tc[i];
            }
        }

        // Prefix sum
        for (size_type i = 0; i < n_cols; ++i)
        {
            col_counts[i + 1] += col_counts[i];
        }

        // Copy to result
        for (size_type i = 0; i <= n_cols; ++i)
        {
            result.row_ptrs_[i] = col_counts[i];
        }

        result.mValues.resize(n_nnz);
        result.col_indices_.resize(n_nnz);

        // Phase 2: Scatter in parallel with atomic positions
        std::vector<std::atomic<ptr_type>> write_pos(n_cols);
        for (size_type i = 0; i < n_cols; ++i)
        {
            write_pos[i].store(col_counts[i], std::memory_order_relaxed);
        }

        {
            std::vector<std::future<void>> futures;
            futures.reserve(partitions.size());

            for (size_t t = 0; t < partitions.size(); ++t)
            {
                size_type start_row = partitions[t].first;
                size_type end_row = partitions[t].second;

                futures.push_back(pool.submit([this, &result, &write_pos, start_row, end_row]()
                {
                    for (size_type i = start_row; i < end_row; ++i)
                    {
                        for (ptr_type j = row_ptrs_[i]; j < row_ptrs_[i + 1]; ++j)
                        {
                            IndexType col = col_indices_[j];
                            ptr_type dest = write_pos[col].fetch_add(1, std::memory_order_relaxed);

                            result.mValues[dest] = mValues[j];
                            result.col_indices_[dest] = static_cast<IndexType>(i);
                        }
                    }
                }));
            }

            for (auto& f : futures)
            {
                f.get();
            }
        }

        // Phase 3: Sort each row by column index (parallel)
        // The atomic scatter may have placed entries out of order
        {
            auto result_partitions = detail::compute_balanced_partitions(
                result.row_ptrs_.data(), n_cols, num_threads);

            std::vector<std::future<void>> futures;
            futures.reserve(result_partitions.size());

            for (const auto& partition : result_partitions)
            {
                size_type start_row = partition.first;
                size_type end_row = partition.second;

                futures.push_back(pool.submit([&result, start_row, end_row]()
                {
                    for (size_type i = start_row; i < end_row; ++i)
                    {
                        ptr_type row_start = result.row_ptrs_[i];
                        ptr_type row_end = result.row_ptrs_[i + 1];
                        ptr_type row_len = row_end - row_start;

                        if (row_len <= 1)
                        {
                            continue;
                        }

                        // Create permutation
                        std::vector<ptr_type> perm(row_len);
                        std::iota(perm.begin(), perm.end(), ptr_type{0});

                        std::sort(perm.begin(), perm.end(),
                            [&result, row_start](ptr_type a, ptr_type b)
                        {
                            return result.col_indices_[row_start + a]
                                 < result.col_indices_[row_start + b];
                        });

                        // Apply permutation
                        std::vector<IndexType> sorted_cols(row_len);
                        std::vector<T> sorted_vals(row_len);

                        for (ptr_type k = 0; k < row_len; ++k)
                        {
                            sorted_cols[k] = result.col_indices_[row_start + perm[k]];
                            sorted_vals[k] = result.mValues[row_start + perm[k]];
                        }

                        for (ptr_type k = 0; k < row_len; ++k)
                        {
                            result.col_indices_[row_start + k] = sorted_cols[k];
                            result.mValues[row_start + k] = sorted_vals[k];
                        }
                    }
                }));
            }

            for (auto& f : futures)
            {
                f.get();
            }
        }

        return result;
    }

    /**
     * @brief Parallel SpMV using default thread pool: y = A * x
     */
    void matvec_parallel(const T* x, T* y, const HpcParallelConfig& config = {}) const
    {
        matvec_parallel(x, y, default_hpc_thread_pool(), config);
    }

    /**
     * @brief Parallel SpMV using default thread pool: y = alpha * A * x + beta * y
     */
    void matvec_parallel(T alpha, const T* x, T beta, T* y,
                         const HpcParallelConfig& config = {}) const
    {
        matvec_parallel(alpha, x, beta, y, default_hpc_thread_pool(), config);
    }

    /**
     * @brief Parallel SpMV batch using default thread pool
     */
    void matvec_parallel_batch(const T* x, T* y,
                               const HpcParallelConfig& config = {}) const
    {
        matvec_parallel_batch(x, y, default_hpc_thread_pool(), config);
    }

    /**
     * @brief Parallel transpose using default thread pool
     */
    [[nodiscard]] HpcCSRMatrix transpose_parallel() const
    {
        return transpose_parallel(default_hpc_thread_pool());
    }

    // =========================================================================
    // Matrix Operations
    // =========================================================================

    /**
     * @brief Transpose matrix
     * @note Workspaces use NUMA-local allocation for memory locality on multi-socket systems
     */
    [[nodiscard]] HpcCSRMatrix transpose() const
    {
        HpcCSRMatrix result(mCols, mRows);

        if (nnz() == 0)
        {
            return result;
        }

        // Count entries per column (NUMA-local workspace)
        memory::NumaLocalVector<ptr_type> col_counts(mCols + 1, 0);
        for (size_type i = 0; i < col_indices_.size(); ++i)
        {
            col_counts[static_cast<size_type>(col_indices_[i]) + 1]++;
        }

        // Cumulative sum
        for (size_type i = 0; i < mCols; ++i)
        {
            col_counts[i + 1] += col_counts[i];
        }

        // Copy to result row_ptrs_
        for (size_type i = 0; i <= mCols; ++i)
        {
            result.row_ptrs_[i] = col_counts[i];
        }

        result.mValues.resize(nnz());
        result.col_indices_.resize(nnz());

        // Fill transposed values (reuse col_counts as write positions)
        memory::NumaLocalVector<ptr_type> write_pos(col_counts.begin(), col_counts.end());
        for (size_type i = 0; i < mRows; ++i)
        {
            ptr_type start = row_ptrs_[i];
            ptr_type end = row_ptrs_[i + 1];

            for (ptr_type j = start; j < end; ++j)
            {
                IndexType col = col_indices_[j];
                ptr_type dest = write_pos[col]++;
                result.mValues[dest] = mValues[j];
                result.col_indices_[dest] = static_cast<IndexType>(i);
            }
        }

        return result;
    }

    /**
     * @brief Matrix addition: C = A + B
     */
    [[nodiscard]] HpcCSRMatrix operator+(const HpcCSRMatrix& other) const
    {
        FATP_ALWAYS_ENFORCE(mRows == other.mRows && mCols == other.mCols,
                       "HpcCSRMatrix: dimension mismatch for addition");

        HpcCSRMatrix result(mRows, mCols);
        result.mValues.reserve(nnz() + other.nnz());
        result.col_indices_.reserve(nnz() + other.nnz());

        for (size_type i = 0; i < mRows; ++i)
        {
            ptr_type a_ptr = row_ptrs_[i];
            ptr_type a_end = row_ptrs_[i + 1];
            ptr_type b_ptr = other.row_ptrs_[i];
            ptr_type b_end = other.row_ptrs_[i + 1];

            while (a_ptr < a_end || b_ptr < b_end)
            {
                IndexType col_a = (a_ptr < a_end)
                    ? col_indices_[a_ptr]
                    : std::numeric_limits<IndexType>::max();
                IndexType col_b = (b_ptr < b_end)
                    ? other.col_indices_[b_ptr]
                    : std::numeric_limits<IndexType>::max();
                IndexType cur_col = std::min(col_a, col_b);

                T sum = T{0};

                while (a_ptr < a_end && col_indices_[a_ptr] == cur_col)
                {
                    sum += mValues[a_ptr++];
                }

                while (b_ptr < b_end && other.col_indices_[b_ptr] == cur_col)
                {
                    sum += other.mValues[b_ptr++];
                }

                if (!is_effectively_zero(sum))
                {
                    result.mValues.push_back(sum);
                    result.col_indices_.push_back(cur_col);
                }
            }

            result.row_ptrs_[i + 1] = result.mValues.size();
        }

        return result;
    }

    /**
     * @brief Matrix subtraction: C = A - B
     */
    [[nodiscard]] HpcCSRMatrix operator-(const HpcCSRMatrix& other) const
    {
        FATP_ALWAYS_ENFORCE(mRows == other.mRows && mCols == other.mCols,
                       "HpcCSRMatrix: dimension mismatch for subtraction");

        HpcCSRMatrix result(mRows, mCols);
        result.mValues.reserve(nnz() + other.nnz());
        result.col_indices_.reserve(nnz() + other.nnz());

        for (size_type i = 0; i < mRows; ++i)
        {
            ptr_type a_ptr = row_ptrs_[i];
            ptr_type a_end = row_ptrs_[i + 1];
            ptr_type b_ptr = other.row_ptrs_[i];
            ptr_type b_end = other.row_ptrs_[i + 1];

            while (a_ptr < a_end || b_ptr < b_end)
            {
                IndexType col_a = (a_ptr < a_end)
                    ? col_indices_[a_ptr]
                    : std::numeric_limits<IndexType>::max();
                IndexType col_b = (b_ptr < b_end)
                    ? other.col_indices_[b_ptr]
                    : std::numeric_limits<IndexType>::max();
                IndexType cur_col = std::min(col_a, col_b);

                T diff = T{0};

                while (a_ptr < a_end && col_indices_[a_ptr] == cur_col)
                {
                    diff += mValues[a_ptr++];
                }

                while (b_ptr < b_end && other.col_indices_[b_ptr] == cur_col)
                {
                    diff -= other.mValues[b_ptr++];
                }

                if (!is_effectively_zero(diff))
                {
                    result.mValues.push_back(diff);
                    result.col_indices_.push_back(cur_col);
                }
            }

            result.row_ptrs_[i + 1] = result.mValues.size();
        }

        return result;
    }

    /**
     * @brief Scalar multiplication: B = A * alpha
     */
    [[nodiscard]] HpcCSRMatrix operator*(T alpha) const
    {
        if (is_effectively_zero(alpha))
        {
            return HpcCSRMatrix(mRows, mCols);
        }

        HpcCSRMatrix result = *this;
        for (size_type i = 0; i < result.mValues.size(); ++i)
        {
            result.mValues[i] *= alpha;
        }
        return result;
    }

    HpcCSRMatrix& operator*=(T alpha)
    {
        if (is_effectively_zero(alpha))
        {
            mValues.clear();
            col_indices_.clear();
            for (size_type i = 0; i <= mRows; ++i)
            {
                row_ptrs_[i] = 0;
            }
            return *this;
        }

        for (size_type i = 0; i < mValues.size(); ++i)
        {
            mValues[i] *= alpha;
        }
        return *this;
    }

    HpcCSRMatrix& operator+=(const HpcCSRMatrix& other)
    {
        *this = *this + other;
        return *this;
    }

    HpcCSRMatrix& operator-=(const HpcCSRMatrix& other)
    {
        *this = *this - other;
        return *this;
    }

    /**
     * @brief Matrix-matrix multiplication: C = A * B (SpGEMM)
     * @note Workspaces use NUMA-local allocation for memory locality on multi-socket systems
     */
    [[nodiscard]] HpcCSRMatrix matmul(const HpcCSRMatrix& B) const
    {
        FATP_ALWAYS_ENFORCE(mCols == B.mRows,
                       "HpcCSRMatrix: incompatible dimensions for matmul");

        HpcCSRMatrix result(mRows, B.mCols);
        result.mValues.reserve(std::max(nnz(), B.nnz()));
        result.col_indices_.reserve(std::max(nnz(), B.nnz()));

        // Workspace (hoisted out of loop, NUMA-local for memory locality)
        memory::NumaLocalVector<T> accumulator(B.mCols, T{0});
        memory::NumaLocalVector<size_type> marker(B.mCols, static_cast<size_type>(-1));
        std::vector<IndexType> touched_cols;  // Small, frequently cleared
        touched_cols.reserve(std::min(B.mCols, size_type{256}));

        for (size_type i = 0; i < mRows; ++i)
        {
            touched_cols.clear();

            ptr_type a_start = row_ptrs_[i];
            ptr_type a_end = row_ptrs_[i + 1];

            for (ptr_type a_ptr = a_start; a_ptr < a_end; ++a_ptr)
            {
                IndexType k = col_indices_[a_ptr];
                T a_val = mValues[a_ptr];

                ptr_type b_start = B.row_ptrs_[k];
                ptr_type b_end = B.row_ptrs_[k + 1];

                for (ptr_type b_ptr = b_start; b_ptr < b_end; ++b_ptr)
                {
                    IndexType j_signed = B.col_indices_[b_ptr];
                    const size_type j = static_cast<size_type>(j_signed);

                    if (marker[j] != i)
                    {
                        marker[j] = i;
                        touched_cols.push_back(j_signed);
                    }

                    accumulator[j] += a_val * B.mValues[b_ptr];
                }
            }

            std::sort(touched_cols.begin(), touched_cols.end());

            for (IndexType j_signed : touched_cols)
            {
                const size_type j = static_cast<size_type>(j_signed);
                T val = accumulator[j];
                if (!is_effectively_zero(val))
                {
                    result.mValues.push_back(val);
                    result.col_indices_.push_back(j_signed);
                }
                accumulator[j] = T{0};
            }

            result.row_ptrs_[i + 1] = result.mValues.size();
        }

        return result;
    }

    // =========================================================================
    // Conversion
    // =========================================================================

    /**
     * @brief Convert to dense matrix
     */
    [[nodiscard]] std::vector<T> to_dense() const
    {
        validate_dense_size(mRows, mCols);

        std::vector<T> dense(mRows * mCols, T{0});

        for (size_type i = 0; i < mRows; ++i)
        {
            ptr_type start = row_ptrs_[i];
            ptr_type end = row_ptrs_[i + 1];

            for (ptr_type j = start; j < end; ++j)
            {
                dense[i * mCols + col_indices_[j]] += mValues[j];
            }
        }

        return dense;
    }

    /**
     * @brief Compute density
     */
    [[nodiscard]] double density() const
    {
        if (mRows == 0 || mCols == 0)
        {
            return 0.0;
        }
        validate_dense_size(mRows, mCols);
        return static_cast<double>(nnz()) / static_cast<double>(mRows * mCols);
    }

    [[nodiscard]] double sparsity() const
    {
        return 1.0 - density();
    }

    /**
     * @brief Check if matrix is symmetric (checks both structure and values)
     * @details Compares A with A^T. Matrices are symmetric if they have identical
     *          sparsity patterns (row_ptrs_, col_indices_) AND identical values.
     */
    [[nodiscard]] bool is_symmetric(T epsilon = default_epsilon()) const
    {
        if (mRows != mCols)
        {
            return false;
        }

        auto trans = transpose();

        // Check structural equality first (prevents false positives)
        if (nnz() != trans.nnz())
        {
            return false;
        }

        // Check row pointers match
        for (size_type i = 0; i <= mRows; ++i)
        {
            if (row_ptrs_[i] != trans.row_ptrs_[i])
            {
                return false;
            }
        }

        // Check column indices match
        for (size_type i = 0; i < col_indices_.size(); ++i)
        {
            if (col_indices_[i] != trans.col_indices_[i])
            {
                return false;
            }
        }

        // Check values match (within epsilon for floating-point)
        for (size_type i = 0; i < mValues.size(); ++i)
        {
            T diff = mValues[i] - trans.mValues[i];
            if constexpr (std::is_floating_point_v<T>)
            {
                using std::abs;
                if (abs(diff) > epsilon)
                {
                    return false;
                }
            }
            else
            {
                if (diff != T{0})
                {
                    return false;
                }
            }
        }

        return true;
    }

    // =========================================================================
    // Comparison
    // =========================================================================

    [[nodiscard]] bool operator==(const HpcCSRMatrix& other) const
    {
        if (mRows != other.mRows || mCols != other.mCols)
        {
            return false;
        }
        if (nnz() != other.nnz())
        {
            return false;
        }

        for (size_type i = 0; i < mValues.size(); ++i)
        {
            if (mValues[i] != other.mValues[i])
            {
                return false;
            }
        }
        for (size_type i = 0; i < col_indices_.size(); ++i)
        {
            if (col_indices_[i] != other.col_indices_[i])
            {
                return false;
            }
        }
        for (size_type i = 0; i < row_ptrs_.size(); ++i)
        {
            if (row_ptrs_[i] != other.row_ptrs_[i])
            {
                return false;
            }
        }

        return true;
    }

    [[nodiscard]] bool operator!=(const HpcCSRMatrix& other) const
    {
        return !(*this == other);
    }

    // =========================================================================
    // I/O
    // =========================================================================

    friend std::ostream& operator<<(std::ostream& os, const HpcCSRMatrix& m)
    {
        os << "HpcCSRMatrix(" << m.mRows << "x" << m.mCols << ", nnz=" << m.nnz();
        if (m.is_numa_available())
        {
            os << ", NUMA-enabled";
        }
        os << ")";
        return os;
    }
};

// =============================================================================
// Non-member operators
// =============================================================================

template <typename T, typename I, std::size_t A, typename P>
[[nodiscard]] HpcCSRMatrix<T, I, A, P> operator*(T alpha, const HpcCSRMatrix<T, I, A, P>& m)
{
    return m * alpha;
}

// =============================================================================
// Convenience Aliases
// =============================================================================

/// Standard HPC sparse matrix (double precision, local NUMA)
template <typename T = double, typename IndexType = int32_t>
using HpcSparseMatrix = HpcCSRMatrix<T, IndexType, 64, memory::NumaLocalPolicy>;

/// Interleaved NUMA sparse matrix (for shared read-only matrices)
template <typename T = double, typename IndexType = int32_t>
using HpcInterleavedSparseMatrix = HpcCSRMatrix<T, IndexType, 64, memory::NumaInterleavedPolicy>;

} // namespace fat_p
