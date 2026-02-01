/**
 * @file CSRMatrix.h
 * @brief Compressed Sparse Row (CSR) matrix format for sparse linear algebra
 *
 * High-performance sparse matrix storage and operations optimized for HPC workloads.
 * CSR format is optimal for sparse matrix-vector multiplication (SpMV).
 *
 * Key Features:
 * - Space-efficient storage (O(nnz) instead of O(m*n))
 * - Fast matrix-vector multiplication with NaN-safe alpha/beta form
 * - Parallel SpMV support via OpenMP
 * - Matrix transpose, addition, and multiplication
 * - Configurable duplicate handling in COO construction
 * - Conversion from/to dense format
 *
 * CSR Format:
 * - mValues: Non-zero values (size = nnz)
 * - col_indices_: Column index of each non-zero (size = nnz)
 * - row_ptrs_: Start of each row in values array (size = rows + 1)
 *
 * Design Notes:
 * - Enforces sparse semantics: explicit zeros are removed automatically
 * - DuplicatePolicy::Keep stores multiple entries per position; linear operations
 *   (matvec, to_dense) sum them, and operator() also sums for consistency
 *
 * @layer Domain
 */

#pragma once

/*
FATP_META:
  meta_version: 1
  component: CSRMatrix
  file_role: public_header
  path: include/fat_p/CSRMatrix.h
  namespace: fat_p
  layer: Domain
  summary: "Public header for CSRMatrix."
  api_stability: in_work
  related:
    docs_search: "CSRMatrix"
    tests:
      - components/CSRMatrix/tests/test_CSRMatrix.cpp
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
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <ostream>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace fat_p
{

/**
 * @brief Compressed Sparse Row matrix
 * @tparam T Value type (must be arithmetic: float, double, signed int, etc.)
 * @tparam IndexType Index type for column indices (int32_t, int64_t)
 */
template <typename T, typename IndexType = int32_t>
class CSRMatrix
{
    static_assert(std::is_arithmetic_v<T>, "CSRMatrix requires arithmetic value type");
    static_assert(!std::is_same_v<T, bool>,
                  "CSRMatrix does not support bool: std::vector<bool> is bit-packed "
                  "and lacks contiguous memory access via .data()");
    static_assert(!std::is_integral_v<T> || std::is_signed_v<T>,
                  "CSRMatrix: Unsigned integer value types are not supported "
                  "(subtraction may produce negative intermediate values)");
    static_assert(std::is_integral_v<IndexType>, "CSRMatrix requires integral index type");
    static_assert(std::is_signed_v<IndexType>,
                  "CSRMatrix requires signed index type for bounds checking with negative values");

public:
    using value_type = T;
    using index_type = IndexType;
    using size_type = std::size_t;
    using ptr_type = std::size_t;

    /**
     * @brief Policy for handling duplicate (row, col) entries in COO input
     *
     * @note With DuplicatePolicy::Keep, multiple entries may exist for the same
     *       (row, col) position. All access methods (operator(), matvec, to_dense)
     *       treat these as summed values for mathematical consistency.
     */
    enum class DuplicatePolicy
    {
        Sum,  ///< Sum duplicate values (standard behavior, default)
        Keep, ///< Keep all duplicates as separate entries (summed on access)
        Error ///< Throw exception if duplicates exist
    };

private:
    size_type mRows;
    size_type mCols;
    std::vector<T> mValues;
    std::vector<IndexType> col_indices_;
    std::vector<ptr_type> row_ptrs_;

    // Type-aware zero comparison
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

    // Type-appropriate default epsilon for symmetry checks
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

    // Validate that dimensions fit within IndexType range
    static void validate_dimensions(size_type rows, size_type cols)
    {
        constexpr auto idx_max = static_cast<size_type>(std::numeric_limits<IndexType>::max());
        if (rows > idx_max)
        {
            throw std::overflow_error("CSRMatrix: Row count exceeds IndexType limits (use larger IndexType)");
        }
        if (cols > idx_max)
        {
            throw std::overflow_error("CSRMatrix: Column count exceeds IndexType limits (use larger IndexType)");
        }
    }

public:
    // =========================================================================
    // Constructors
    // =========================================================================

    CSRMatrix()
        : mRows(0)
        , mCols(0)
    {
        row_ptrs_.push_back(0);
    }

    // Explicitly defaulted special members with noexcept for optimal STL container behavior
    CSRMatrix(const CSRMatrix&) = default;
    CSRMatrix& operator=(const CSRMatrix&) = default;
    CSRMatrix(CSRMatrix&&) noexcept = default;
    CSRMatrix& operator=(CSRMatrix&&) noexcept = default;
    ~CSRMatrix() = default;

    CSRMatrix(size_type rows, size_type cols)
        : mRows(rows)
        , mCols(cols)
    {
        validate_dimensions(rows, cols);
        row_ptrs_.resize(rows + 1, 0);
    }

    /**
     * @brief Construct from COO (Coordinate) format
     * @param rows Number of rows
     * @param cols Number of columns
     * @param row_indices Row indices of non-zeros
     * @param col_indices Column indices of non-zeros
     * @param values Values of non-zeros
     * @param policy How to handle duplicate (row, col) entries
     * @throws std::invalid_argument if array sizes mismatch or duplicates found (Error policy)
     * @throws std::out_of_range if indices are out of bounds
     * @throws std::overflow_error if dimensions exceed IndexType limits
     */
    CSRMatrix(size_type rows,
              size_type cols,
              const std::vector<IndexType>& row_indices,
              const std::vector<IndexType>& col_indices,
              const std::vector<T>& values,
              DuplicatePolicy policy = DuplicatePolicy::Sum)
        : mRows(rows)
        , mCols(cols)
    {
        validate_dimensions(rows, cols);

        if (row_indices.size() != col_indices.size() || row_indices.size() != values.size())
        {
            throw std::invalid_argument("CSRMatrix: COO arrays must have same size");
        }

        size_type nnz_input = values.size();

        if (nnz_input == 0)
        {
            row_ptrs_.resize(rows + 1, 0);
            return;
        }

        // Build and validate triplets
        using Triplet = std::tuple<IndexType, IndexType, T>;
        std::vector<Triplet> triplets;
        triplets.reserve(nnz_input);

        for (size_type i = 0; i < nnz_input; ++i)
        {
            IndexType r = row_indices[i];
            IndexType c = col_indices[i];

            if (r < 0 || static_cast<size_type>(r) >= rows || c < 0 || static_cast<size_type>(c) >= cols)
            {
                throw std::out_of_range("CSRMatrix: COO index out of bounds");
            }

            triplets.emplace_back(r, c, values[i]);
        }

        // Sort by (row, col)
        std::sort(triplets.begin(), triplets.end(), [](const Triplet& a, const Triplet& b) {
            if (std::get<0>(a) != std::get<0>(b))
            {
                return std::get<0>(a) < std::get<0>(b);
            }
            return std::get<1>(a) < std::get<1>(b);
        });

        // Apply duplicate policy
        std::vector<Triplet> processed;
        processed.reserve(triplets.size());

        switch (policy)
        {
            case DuplicatePolicy::Keep:
                processed = std::move(triplets);
                break;

            case DuplicatePolicy::Error:
            {
                for (size_type i = 0; i < triplets.size(); ++i)
                {
                    if (i > 0 && std::get<0>(triplets[i]) == std::get<0>(triplets[i - 1]) &&
                        std::get<1>(triplets[i]) == std::get<1>(triplets[i - 1]))
                    {
                        throw std::invalid_argument("CSRMatrix: duplicate COO entries");
                    }
                    processed.push_back(triplets[i]);
                }
                break;
            }

            case DuplicatePolicy::Sum:
            default:
            {
                for (const auto& t : triplets)
                {
                    if (!processed.empty() && std::get<0>(processed.back()) == std::get<0>(t) &&
                        std::get<1>(processed.back()) == std::get<1>(t))
                    {
                        std::get<2>(processed.back()) += std::get<2>(t);
                    }
                    else
                    {
                        processed.push_back(t);
                    }
                }
                break;
            }
        }

        // Build CSR structure - always filter zeros for sparse semantics
        mValues.reserve(processed.size());
        col_indices_.reserve(processed.size());
        row_ptrs_.resize(rows + 1, 0);

        for (const auto& [row, col, val] : processed)
        {
            // Always filter zeros regardless of policy (sparse invariant)
            if (!is_effectively_zero(val))
            {
                mValues.push_back(val);
                col_indices_.push_back(col);
                row_ptrs_[static_cast<size_type>(row) + 1]++;
            }
        }

        // Convert counts to cumulative sum
        for (size_type i = 0; i < rows; ++i)
        {
            row_ptrs_[i + 1] += row_ptrs_[i];
        }
    }

    /**
     * @brief Construct from dense matrix (row-major)
     * @param dense Pointer to dense matrix data (must not be null if rows*cols > 0)
     * @param rows Number of rows
     * @param cols Number of columns
     * @param epsilon Zero threshold (values with |val| <= epsilon are considered zero)
     * @throws std::invalid_argument if dense is null with non-zero dimensions
     * @throws std::overflow_error if dimensions exceed IndexType limits or matrix too large
     */
    [[nodiscard]] static CSRMatrix from_dense(const T* dense, size_type rows, size_type cols, T epsilon = T{0})
    {
        // Null pointer check
        if (dense == nullptr && rows > 0 && cols > 0)
        {
            throw std::invalid_argument("CSRMatrix: null pointer to dense matrix");
        }

        validate_dimensions(rows, cols);

        if (rows > 0 && cols > 0 && rows > std::numeric_limits<size_type>::max() / cols)
        {
            throw std::overflow_error("CSRMatrix: matrix too large for dense conversion");
        }

        CSRMatrix result(rows, cols);
        result.mValues.reserve(rows * std::min(cols, size_type{16}));
        result.col_indices_.reserve(rows * std::min(cols, size_type{16}));

        using std::abs;
        for (size_type i = 0; i < rows; ++i)
        {
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

    [[nodiscard]] size_type rows() const noexcept
    {
        return mRows;
    }
    [[nodiscard]] size_type cols() const noexcept
    {
        return mCols;
    }
    [[nodiscard]] size_type nnz() const noexcept
    {
        return mValues.size();
    }
    [[nodiscard]] bool empty() const noexcept
    {
        return mValues.empty();
    }

    [[nodiscard]] const std::vector<T>& values() const noexcept
    {
        return mValues;
    }
    [[nodiscard]] const std::vector<IndexType>& col_indices() const noexcept
    {
        return col_indices_;
    }
    [[nodiscard]] const std::vector<ptr_type>& row_ptrs() const noexcept
    {
        return row_ptrs_;
    }

    // =========================================================================
    // Element Access
    // =========================================================================

    /**
     * @brief Get element at (row, col)
     * @return Value at (row, col), or T{0} if not stored
     * @throws std::out_of_range if indices are out of bounds
     *
     * @note For DuplicatePolicy::Keep matrices, this returns the SUM of all
     *       entries at (row, col), consistent with matvec and to_dense semantics.
     */
    [[nodiscard]] T operator()(size_type row, size_type col) const
    {
        if (row >= mRows || col >= mCols)
        {
            throw std::out_of_range("CSRMatrix: index out of range");
        }

        ptr_type start = row_ptrs_[row];
        ptr_type end = row_ptrs_[row + 1];

        // Sum all entries at this (row, col) for consistency with matvec/to_dense
        // This handles DuplicatePolicy::Keep correctly
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
                // Columns are sorted, no more matches possible
                break;
            }
        }

        return sum;
    }

    /**
     * @brief Set element at (row, col)
     * @details O(nnz + rows) worst-case complexity: insertion/removal shifts all
     *          subsequent elements in mValues and col_indices_, and updates row_ptrs_
     *          for all rows after the modified one. For bulk construction, use COO
     *          constructor instead. Setting to zero removes the entry to maintain
     *          sparse invariant.
     *
     * @warning This method is inefficient for matrix construction. For a matrix with
     *          10^6 rows, each call iterates through all subsequent row pointers.
     *          Use the COO constructor for building matrices from scratch.
     *
     * @note This operation removes ALL existing entries at (row, col) before
     *       inserting the new value, effectively collapsing any duplicates from
     *       DuplicatePolicy::Keep construction.
     */
    void set(size_type row, size_type col, T value)
    {
        if (row >= mRows || col >= mCols)
        {
            throw std::out_of_range("CSRMatrix: index out of range");
        }

        ptr_type start = row_ptrs_[row];
        ptr_type end = row_ptrs_[row + 1];
        IndexType target_col = static_cast<IndexType>(col);

        // First, remove all existing entries at this position (handles duplicates)
        ptr_type write_pos = start;
        size_type removed_count = 0;
        for (ptr_type i = start; i < end; ++i)
        {
            if (col_indices_[i] != target_col)
            {
                if (write_pos != i)
                {
                    mValues[write_pos] = mValues[i];
                    col_indices_[write_pos] = col_indices_[i];
                }
                ++write_pos;
            }
            else
            {
                ++removed_count;
            }
        }

        // If we removed entries, compact the arrays using std::move for safety
        if (removed_count > 0)
        {
            // Shift remaining elements using std::move for overlapping ranges
            std::move(mValues.begin() + static_cast<std::ptrdiff_t>(end),
                      mValues.end(),
                      mValues.begin() + static_cast<std::ptrdiff_t>(end - removed_count));
            std::move(col_indices_.begin() + static_cast<std::ptrdiff_t>(end),
                      col_indices_.end(),
                      col_indices_.begin() + static_cast<std::ptrdiff_t>(end - removed_count));

            mValues.resize(mValues.size() - removed_count);
            col_indices_.resize(col_indices_.size() - removed_count);

            // Update row pointers
            for (size_type r = row + 1; r <= mRows; ++r)
            {
                row_ptrs_[r] -= removed_count;
            }
        }

        // Insert new value if non-zero
        if (!is_effectively_zero(value))
        {
            // Find insertion position (maintain sorted order)
            start = row_ptrs_[row];
            end = row_ptrs_[row + 1];
            ptr_type insert_pos = start;
            for (ptr_type i = start; i < end; ++i)
            {
                if (col_indices_[i] > target_col)
                {
                    break;
                }
                insert_pos = i + 1;
            }

            mValues.insert(mValues.begin() + static_cast<std::ptrdiff_t>(insert_pos), value);
            col_indices_.insert(col_indices_.begin() + static_cast<std::ptrdiff_t>(insert_pos), target_col);
            for (size_type r = row + 1; r <= mRows; ++r)
            {
                row_ptrs_[r]++;
            }
        }
    }

    // =========================================================================
    // Row Iteration
    // =========================================================================

    /**
     * @brief Lightweight view for iterating over a single row's entries
     */
    class RowView
    {
    public:
        class Iterator
        {
        public:
            using iterator_category = std::forward_iterator_tag;
            using value_type = std::pair<IndexType, T>;
            using difference_type = std::ptrdiff_t;
            using pointer = const value_type*;
            using reference = value_type;

            Iterator(const IndexType* col, const T* val)
                : mCol(col)
                , mVal(val)
            {
            }

            reference operator*() const
            {
                return {*mCol, *mVal};
            }
            Iterator& operator++()
            {
                ++mCol;
                ++mVal;
                return *this;
            }
            Iterator operator++(int)
            {
                Iterator tmp = *this;
                ++(*this);
                return tmp;
            }
            bool operator==(const Iterator& other) const
            {
                return mCol == other.mCol;
            }
            bool operator!=(const Iterator& other) const
            {
                return mCol != other.mCol;
            }

        private:
            const IndexType* mCol;
            const T* mVal;
        };

        RowView(const IndexType* col_begin, const IndexType* col_end, const T* val_begin)
            : col_begin_(col_begin)
            , col_end_(col_end)
            , val_begin_(val_begin)
        {
        }

        [[nodiscard]] Iterator begin() const
        {
            return Iterator(col_begin_, val_begin_);
        }
        [[nodiscard]] Iterator end() const
        {
            return Iterator(col_end_, val_begin_ + (col_end_ - col_begin_));
        }
        [[nodiscard]] size_type size() const
        {
            return static_cast<size_type>(col_end_ - col_begin_);
        }
        [[nodiscard]] bool empty() const
        {
            return col_begin_ == col_end_;
        }

        [[nodiscard]] std::pair<IndexType, T> operator[](size_type idx) const
        {
            return {col_begin_[idx], val_begin_[idx]};
        }

    private:
        const IndexType* col_begin_;
        const IndexType* col_end_;
        const T* val_begin_;
    };

    /**
     * @brief Get a view for iterating over row entries
     * @param row Row index
     * @return RowView for the specified row
     */
    [[nodiscard]] RowView row(size_type row) const
    {
        if (row >= mRows)
        {
            throw std::out_of_range("CSRMatrix: row index out of range");
        }
        ptr_type start = row_ptrs_[row];
        ptr_type end = row_ptrs_[row + 1];
        return RowView(col_indices_.data() + start, col_indices_.data() + end, mValues.data() + start);
    }

    // =========================================================================
    // Matrix-Vector Operations
    // =========================================================================

    /**
     * @brief Sparse matrix-vector multiply: y = A * x
     */
    void matvec(const T* x, T* y) const
    {
        for (size_type i = 0; i < mRows; ++i)
        {
            T sum = T{0};
            ptr_type start = row_ptrs_[i];
            ptr_type end = row_ptrs_[i + 1];

            for (ptr_type j = start; j < end; ++j)
            {
                sum += mValues[j] * x[col_indices_[j]];
            }
            y[i] = sum;
        }
    }

    /**
     * @brief Sparse matrix-vector multiply: y = alpha * A * x + beta * y
     * @details Handles beta=0 specially to avoid NaN propagation from uninitialized y
     */
    void matvec(T alpha, const T* x, T beta, T* y) const
    {
        for (size_type i = 0; i < mRows; ++i)
        {
            T sum = T{0};
            ptr_type start = row_ptrs_[i];
            ptr_type end = row_ptrs_[i + 1];

            for (ptr_type j = start; j < end; ++j)
            {
                sum += mValues[j] * x[col_indices_[j]];
            }

            // Safe overwrite when beta is zero to avoid NaN propagation
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
     * @brief Sparse matrix-vector multiply with std::vector: y = alpha * A * x + beta * y
     * @details Convenience overload for std::vector users
     */
    void matvec(T alpha, const std::vector<T>& x, T beta, std::vector<T>& y) const
    {
        if (x.size() != mCols || y.size() != mRows)
        {
            throw std::invalid_argument("CSRMatrix: vector size mismatch");
        }
        matvec(alpha, x.data(), beta, y.data());
    }

    /**
     * @brief Sparse matrix-vector multiply with std::vector
     */
    [[nodiscard]] std::vector<T> operator*(const std::vector<T>& x) const
    {
        if (x.size() != mCols)
        {
            throw std::invalid_argument("CSRMatrix: vector size mismatch");
        }

        std::vector<T> y(mRows);
        matvec(x.data(), y.data());
        return y;
    }

    /**
     * @brief Parallel sparse matrix-vector multiply (OpenMP)
     * @warning y must not alias x. In-place operation (y=A*y) is undefined behavior
     *          with OpenMP parallelization due to concurrent reads and writes.
     */
    void matvec_parallel(const T* x, T* y) const
    {
#if defined(_OPENMP)
#pragma omp parallel for schedule(static)
        for (std::ptrdiff_t i = 0; i < static_cast<std::ptrdiff_t>(mRows); ++i)
        {
            T sum = T{0};
            ptr_type start = row_ptrs_[i];
            ptr_type end = row_ptrs_[i + 1];

            for (ptr_type j = start; j < end; ++j)
            {
                sum += mValues[j] * x[col_indices_[j]];
            }
            y[i] = sum;
        }
#else
        matvec(x, y);
#endif
    }

    /**
     * @brief Parallel sparse matrix-vector multiply: y = alpha * A * x + beta * y (OpenMP)
     * @details Handles beta=0 specially to avoid NaN propagation from uninitialized y
     * @warning y must not alias x. In-place operation (y=A*y) is undefined behavior
     *          with OpenMP parallelization due to concurrent reads and writes.
     */
    void matvec_parallel(T alpha, const T* x, T beta, T* y) const
    {
#if defined(_OPENMP)
#pragma omp parallel for schedule(static)
        for (std::ptrdiff_t i = 0; i < static_cast<std::ptrdiff_t>(mRows); ++i)
        {
            T sum = T{0};
            ptr_type start = row_ptrs_[i];
            ptr_type end = row_ptrs_[i + 1];

            for (ptr_type j = start; j < end; ++j)
            {
                sum += mValues[j] * x[col_indices_[j]];
            }

            // Safe overwrite when beta is zero to avoid NaN propagation
            if (beta == T{0})
            {
                y[i] = alpha * sum;
            }
            else
            {
                y[i] = alpha * sum + beta * y[i];
            }
        }
#else
        matvec(alpha, x, beta, y);
#endif
    }

    // =========================================================================
    // Matrix Operations
    // =========================================================================

    /**
     * @brief Transpose matrix
     * @return Transposed matrix in CSR format
     */
    [[nodiscard]] CSRMatrix transpose() const
    {
        CSRMatrix result(mCols, mRows);

        if (nnz() == 0)
        {
            return result;
        }

        // Count entries per column (rows in transposed)
        std::vector<ptr_type> col_counts(mCols + 1, 0);
        for (IndexType col : col_indices_)
        {
            col_counts[static_cast<size_type>(col) + 1]++;
        }

        // Cumulative sum
        for (size_type i = 0; i < mCols; ++i)
        {
            col_counts[i + 1] += col_counts[i];
        }

        result.row_ptrs_ = col_counts;
        result.mValues.resize(nnz());
        result.col_indices_.resize(nnz());

        // Fill transposed values
        std::vector<ptr_type> write_pos = col_counts;
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
     * @brief Element-wise addition: C = A + B
     * @details Properly handles DuplicatePolicy::Keep matrices by consolidating
     *          all entries at the same (row, col) position into a single output entry.
     */
    [[nodiscard]] CSRMatrix operator+(const CSRMatrix& other) const
    {
        if (mRows != other.mRows || mCols != other.mCols)
        {
            throw std::invalid_argument("CSRMatrix: dimension mismatch for addition");
        }

        CSRMatrix result(mRows, mCols);
        result.mValues.reserve(nnz() + other.nnz());
        result.col_indices_.reserve(nnz() + other.nnz());

        for (size_type i = 0; i < mRows; ++i)
        {
            ptr_type a_ptr = row_ptrs_[i];
            ptr_type a_end = row_ptrs_[i + 1];
            ptr_type b_ptr = other.row_ptrs_[i];
            ptr_type b_end = other.row_ptrs_[i + 1];

            // Merge rows, consolidating all duplicates at each column position
            while (a_ptr < a_end || b_ptr < b_end)
            {
                // Determine the current minimum column
                IndexType col_a = (a_ptr < a_end) ? col_indices_[a_ptr] : std::numeric_limits<IndexType>::max();
                IndexType col_b = (b_ptr < b_end) ? other.col_indices_[b_ptr] : std::numeric_limits<IndexType>::max();
                IndexType cur_col = std::min(col_a, col_b);

                T sum = T{0};

                // Consume ALL entries at cur_col from A (handles duplicates)
                while (a_ptr < a_end && col_indices_[a_ptr] == cur_col)
                {
                    sum += mValues[a_ptr++];
                }

                // Consume ALL entries at cur_col from B (handles duplicates)
                while (b_ptr < b_end && other.col_indices_[b_ptr] == cur_col)
                {
                    sum += other.mValues[b_ptr++];
                }

                // Only store non-zero result
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
     * @brief Element-wise subtraction: C = A - B
     * @details Properly handles DuplicatePolicy::Keep matrices by consolidating
     *          all entries at the same (row, col) position into a single output entry.
     */
    [[nodiscard]] CSRMatrix operator-(const CSRMatrix& other) const
    {
        if (mRows != other.mRows || mCols != other.mCols)
        {
            throw std::invalid_argument("CSRMatrix: dimension mismatch for subtraction");
        }

        CSRMatrix result(mRows, mCols);
        result.mValues.reserve(nnz() + other.nnz());
        result.col_indices_.reserve(nnz() + other.nnz());

        for (size_type i = 0; i < mRows; ++i)
        {
            ptr_type a_ptr = row_ptrs_[i];
            ptr_type a_end = row_ptrs_[i + 1];
            ptr_type b_ptr = other.row_ptrs_[i];
            ptr_type b_end = other.row_ptrs_[i + 1];

            // Merge rows, consolidating all duplicates at each column position
            while (a_ptr < a_end || b_ptr < b_end)
            {
                // Determine the current minimum column
                IndexType col_a = (a_ptr < a_end) ? col_indices_[a_ptr] : std::numeric_limits<IndexType>::max();
                IndexType col_b = (b_ptr < b_end) ? other.col_indices_[b_ptr] : std::numeric_limits<IndexType>::max();
                IndexType cur_col = std::min(col_a, col_b);

                T diff = T{0};

                // Consume ALL entries at cur_col from A (handles duplicates)
                while (a_ptr < a_end && col_indices_[a_ptr] == cur_col)
                {
                    diff += mValues[a_ptr++];
                }

                // Subtract ALL entries at cur_col from B (handles duplicates)
                while (b_ptr < b_end && other.col_indices_[b_ptr] == cur_col)
                {
                    diff -= other.mValues[b_ptr++];
                }

                // Only store non-zero result
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
     * @note Multiplying by zero returns an empty sparse matrix (sparse invariant)
     */
    [[nodiscard]] CSRMatrix operator*(T alpha) const
    {
        // Maintain sparse invariant: multiplication by zero yields empty matrix
        if (is_effectively_zero(alpha))
        {
            return CSRMatrix(mRows, mCols);
        }

        CSRMatrix result = *this;
        for (auto& val : result.mValues)
        {
            val *= alpha;
        }
        return result;
    }

    /**
     * @brief In-place scalar multiplication: A *= alpha
     * @note Multiplying by zero clears the matrix (sparse invariant)
     */
    CSRMatrix& operator*=(T alpha)
    {
        // Maintain sparse invariant: multiplication by zero clears the matrix
        if (is_effectively_zero(alpha))
        {
            mValues.clear();
            col_indices_.clear();
            std::fill(row_ptrs_.begin(), row_ptrs_.end(), ptr_type{0});
            return *this;
        }

        for (auto& val : mValues)
        {
            val *= alpha;
        }
        return *this;
    }

    /**
     * @brief In-place addition: A += B
     */
    CSRMatrix& operator+=(const CSRMatrix& other)
    {
        *this = *this + other;
        return *this;
    }

    /**
     * @brief In-place subtraction: A -= B
     */
    CSRMatrix& operator-=(const CSRMatrix& other)
    {
        *this = *this - other;
        return *this;
    }

    /**
     * @brief Matrix-matrix multiplication: C = A * B (SpGEMM)
     */
    [[nodiscard]] CSRMatrix matmul(const CSRMatrix& B) const
    {
        if (mCols != B.mRows)
        {
            throw std::invalid_argument("CSRMatrix: incompatible dimensions for matmul");
        }

        CSRMatrix result(mRows, B.mCols);
        result.mValues.reserve(std::max(nnz(), B.nnz()));
        result.col_indices_.reserve(std::max(nnz(), B.nnz()));

        // Workspace (hoisted out of loop for performance)
        std::vector<T> accumulator(B.mCols, T{0});
        // Use size_type for marker to avoid overflow when mRows > IndexType::max
        std::vector<size_type> marker(B.mCols, static_cast<size_type>(-1));
        std::vector<IndexType> touched_cols;
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
                    IndexType j = B.col_indices_[b_ptr];

                    if (marker[j] != i)
                    {
                        marker[j] = i;
                        touched_cols.push_back(j);
                    }

                    accumulator[j] += a_val * B.mValues[b_ptr];
                }
            }

            // Sort columns and store non-zeros
            std::sort(touched_cols.begin(), touched_cols.end());

            for (IndexType j : touched_cols)
            {
                T val = accumulator[j];
                if (!is_effectively_zero(val))
                {
                    result.mValues.push_back(val);
                    result.col_indices_.push_back(j);
                }
                accumulator[j] = T{0};
            }

            result.row_ptrs_[i + 1] = result.mValues.size();
        }

        return result;
    }

    // =========================================================================
    // Comparison Operators
    // =========================================================================

    /**
     * @brief Structural equality comparison
     * @details Compares the CSR structure directly (row_ptrs, col_indices, values).
     *          Two matrices are equal if they have identical storage.
     * @note For floating-point types, this uses exact comparison. Use equals()
     *       with an epsilon for approximate comparison.
     */
    [[nodiscard]] bool operator==(const CSRMatrix& other) const
    {
        if (mRows != other.mRows || mCols != other.mCols)
        {
            return false;
        }
        return row_ptrs_ == other.row_ptrs_ && col_indices_ == other.col_indices_ && mValues == other.mValues;
    }

    [[nodiscard]] bool operator!=(const CSRMatrix& other) const
    {
        return !(*this == other);
    }

    /**
     * @brief Approximate equality comparison for floating-point matrices
     * @param other Matrix to compare against
     * @param epsilon Tolerance for value comparison
     */
    [[nodiscard]] bool equals(const CSRMatrix& other, T epsilon = default_epsilon<T>()) const
    {
        if (mRows != other.mRows || mCols != other.mCols)
        {
            return false;
        }
        if (row_ptrs_ != other.row_ptrs_ || col_indices_ != other.col_indices_)
        {
            return false;
        }
        if (mValues.size() != other.mValues.size())
        {
            return false;
        }

        using std::abs;
        for (size_type i = 0; i < mValues.size(); ++i)
        {
            if constexpr (std::is_floating_point_v<T>)
            {
                if (abs(mValues[i] - other.mValues[i]) > epsilon)
                {
                    return false;
                }
            }
            else
            {
                if (mValues[i] != other.mValues[i])
                {
                    return false;
                }
            }
        }
        return true;
    }

    // =========================================================================
    // Utility Functions
    // =========================================================================

    /**
     * @brief Convert to dense matrix (row-major)
     * @details Uses accumulation (+=) to correctly handle DuplicatePolicy::Keep
     *          matrices where multiple entries may exist for the same position.
     *          This matches the semantics of matvec and matmul.
     */
    [[nodiscard]] std::vector<T> to_dense() const
    {
        if (mRows > 0 && mCols > 0 && mRows > std::numeric_limits<size_type>::max() / mCols)
        {
            throw std::overflow_error("CSRMatrix: matrix too large for dense conversion");
        }

        std::vector<T> dense(mRows * mCols, T{0});

        for (size_type i = 0; i < mRows; ++i)
        {
            ptr_type start = row_ptrs_[i];
            ptr_type end = row_ptrs_[i + 1];

            for (ptr_type j = start; j < end; ++j)
            {
                // Use += to sum duplicates (consistent with matvec/matmul semantics)
                dense[i * mCols + col_indices_[j]] += mValues[j];
            }
        }

        return dense;
    }

    /**
     * @brief Compute density (fraction of non-zeros)
     * @throws std::overflow_error if rows * cols would overflow
     */
    [[nodiscard]] double density() const
    {
        if (mRows == 0 || mCols == 0)
        {
            return 0.0;
        }
        if (mRows > std::numeric_limits<size_type>::max() / mCols)
        {
            throw std::overflow_error("CSRMatrix: matrix too large for density calculation");
        }
        return static_cast<double>(nnz()) / static_cast<double>(mRows * mCols);
    }

    /**
     * @brief Compute sparsity (fraction of zeros)
     * @throws std::overflow_error if rows * cols would overflow
     */
    [[nodiscard]] double sparsity() const
    {
        return 1.0 - density();
    }

    /**
     * @brief Check if matrix is symmetric
     * @details O(nnz) implementation using transpose comparison.
     *          For DuplicatePolicy::Keep matrices, this checks structural symmetry
     *          of the CSR arrays, not the aggregated values.
     */
    [[nodiscard]] bool is_symmetric(T epsilon = default_epsilon<T>()) const
    {
        if (mRows != mCols)
        {
            return false;
        }

        CSRMatrix AT = transpose();

        if (nnz() != AT.nnz())
        {
            return false;
        }

        if (row_ptrs_ != AT.row_ptrs_ || col_indices_ != AT.col_indices_)
        {
            return false;
        }

        using std::abs;
        for (size_type i = 0; i < mValues.size(); ++i)
        {
            if constexpr (std::is_floating_point_v<T>)
            {
                if (abs(mValues[i] - AT.mValues[i]) > epsilon)
                {
                    return false;
                }
            }
            else
            {
                if (mValues[i] != AT.mValues[i])
                {
                    return false;
                }
            }
        }

        return true;
    }

    /**
     * @brief Remove explicit zeros from the structure
     * @param epsilon Tolerance for zero detection
     */
    void remove_zeros(T epsilon = default_epsilon<T>())
    {
        size_type write_pos = 0;

        for (size_type i = 0; i < mRows; ++i)
        {
            ptr_type read_start = row_ptrs_[i];
            row_ptrs_[i] = write_pos;

            ptr_type read_end = row_ptrs_[i + 1];
            for (ptr_type j = read_start; j < read_end; ++j)
            {
                using std::abs;
                bool is_nonzero = false;

                if constexpr (std::is_floating_point_v<T>)
                {
                    is_nonzero = abs(mValues[j]) > epsilon;
                }
                else
                {
                    is_nonzero = mValues[j] != T{0};
                }

                if (is_nonzero)
                {
                    mValues[write_pos] = mValues[j];
                    col_indices_[write_pos] = col_indices_[j];
                    ++write_pos;
                }
            }
        }

        row_ptrs_[mRows] = write_pos;
        mValues.resize(write_pos);
        col_indices_.resize(write_pos);
    }

    /**
     * @brief Shrink internal storage to fit current contents
     */
    void shrink_to_fit()
    {
        mValues.shrink_to_fit();
        col_indices_.shrink_to_fit();
        row_ptrs_.shrink_to_fit();
    }

    /**
     * @brief Get number of non-zeros in a specific row
     */
    [[nodiscard]] size_type row_nnz(size_type row) const
    {
        if (row >= mRows)
        {
            throw std::out_of_range("CSRMatrix: row index out of range");
        }
        return row_ptrs_[row + 1] - row_ptrs_[row];
    }

    // =========================================================================
    // Stream Output
    // =========================================================================

    /**
     * @brief Output matrix summary to stream
     */
    friend std::ostream& operator<<(std::ostream& os, const CSRMatrix& mat)
    {
        os << "CSRMatrix(" << mat.mRows << "x" << mat.mCols << ", nnz=" << mat.nnz() << ")";
        return os;
    }
};

// =============================================================================
// Free Functions
// =============================================================================

/**
 * @brief Create identity matrix
 */
template <typename T, typename IndexType = int32_t>
[[nodiscard]] CSRMatrix<T, IndexType> identity_matrix(std::size_t n)
{
    std::vector<IndexType> indices(n);
    std::iota(indices.begin(), indices.end(), IndexType{0});

    std::vector<T> values(n, T{1});

    return CSRMatrix<T, IndexType>(n, n, indices, indices, values);
}

/**
 * @brief Create diagonal matrix from vector
 */
template <typename T, typename IndexType = int32_t>
[[nodiscard]] CSRMatrix<T, IndexType> diagonal_matrix(const std::vector<T>& diag)
{
    std::size_t n = diag.size();
    std::vector<IndexType> indices(n);
    std::iota(indices.begin(), indices.end(), IndexType{0});

    return CSRMatrix<T, IndexType>(n, n, indices, indices, diag);
}

/**
 * @brief Scalar * matrix multiplication
 */
template <typename T, typename IndexType>
[[nodiscard]] CSRMatrix<T, IndexType> operator*(T alpha, const CSRMatrix<T, IndexType>& mat)
{
    return mat * alpha;
}

} // namespace fat_p
