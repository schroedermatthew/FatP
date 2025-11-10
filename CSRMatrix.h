/**
 * @file CSRMatrix.h
 * @brief Compressed Sparse Row (CSR) matrix format for sparse linear algebra
 * @version 1.0
 * 
 * @details High-performance sparse matrix storage and operations.
 * CSR format is optimal for sparse matrix-vector multiplication (SpMV).
 * 
 * Key Features:
 * - Space-efficient storage (O(nnz) instead of O(m*n))
 * - Fast matrix-vector multiplication
 * - Parallel SpMV support
 * - Matrix transpose
 * - Element-wise operations
 * - Conversion from/to dense format
 * - Matrix-matrix multiplication
 * 
 * CSR Format:
 * - values: Non-zero values (size = nnz)
 * - col_indices: Column index of each non-zero (size = nnz)
 * - row_ptrs: Start of each row in values array (size = rows + 1)
 * 
 * Use Cases:
 * - Finite element methods
 * - Graph algorithms
 * - Machine learning (sparse features)
 * - Large-scale scientific computing
 * 
 * Performance:
 * - SpMV: O(nnz) vs O(m*n) for dense
 * - Memory: ~10-100x less than dense for typical sparsity
 * 
 * Requires: C++17
 * 
 * @author cpp_utilities
 * @date 2025
 */

#pragma once

#include <vector>
#include <algorithm>
#include <stdexcept>
#include <cstddef>
#include <numeric>
#include <type_traits>
#include <utility>
#include <tuple>

namespace cpp_utilities {
namespace sparse {

// =============================================================================
// CSR Matrix
// =============================================================================

/**
 * @brief Compressed Sparse Row matrix
 * @tparam T Value type (float, double, etc.)
 * @tparam IndexType Index type (int32_t, int64_t, size_t)
 */
template<typename T, typename IndexType = int32_t>
class CSRMatrix {
public:
    using value_type = T;
    using index_type = IndexType;
    using size_type = std::size_t;
    
private:
    size_type rows_;
    size_type cols_;
    std::vector<T> values_;              // Non-zero values
    std::vector<IndexType> col_indices_; // Column indices
    std::vector<IndexType> row_ptrs_;    // Row pointers (size = rows + 1)
    
public:
    // Constructors
    CSRMatrix() : rows_(0), cols_(0) {
        row_ptrs_.push_back(0);
    }
    
    /**
     * @brief Construct empty matrix with dimensions
     */
    CSRMatrix(size_type rows, size_type cols) 
        : rows_(rows), cols_(cols)
    {
        row_ptrs_.resize(rows + 1, 0);
    }
    
    /**
     * @brief Construct from COO (Coordinate) format
     * @param rows Number of rows
     * @param cols Number of columns
     * @param row_indices Row indices of non-zeros
     * @param col_indices Column indices of non-zeros
     * @param values Values of non-zeros
     */
    CSRMatrix(size_type rows, size_type cols,
              const std::vector<IndexType>& row_indices,
              const std::vector<IndexType>& col_indices,
              const std::vector<T>& values)
        : rows_(rows), cols_(cols)
    {
        if (row_indices.size() != col_indices.size() || 
            row_indices.size() != values.size()) {
            throw std::invalid_argument("CSRMatrix: COO arrays must have same size");
        }
        
        size_type nnz = values.size();
        
        // Create triplets (row, col, value) and sort by row then column
        std::vector<std::tuple<IndexType, IndexType, T>> triplets;
        triplets.reserve(nnz);
        
        for (size_type i = 0; i < nnz; ++i) {
            triplets.emplace_back(row_indices[i], col_indices[i], values[i]);
        }
        
        std::sort(triplets.begin(), triplets.end(),
            [](const auto& a, const auto& b) {
                if (std::get<0>(a) != std::get<0>(b)) {
                    return std::get<0>(a) < std::get<0>(b);
                }
                return std::get<1>(a) < std::get<1>(b);
            });
        
        // Build CSR structure
        values_.reserve(nnz);
        col_indices_.reserve(nnz);
        row_ptrs_.resize(rows + 1, 0);
        
        for (const auto& [row, col, val] : triplets) {
            values_.push_back(val);
            col_indices_.push_back(col);
            row_ptrs_[row + 1]++;
        }
        
        // Convert counts to cumulative sum
        for (size_type i = 0; i < rows; ++i) {
            row_ptrs_[i + 1] += row_ptrs_[i];
        }
    }
    
    /**
     * @brief Construct from dense matrix
     * @param dense Dense matrix in row-major format
     * @param rows Number of rows
     * @param cols Number of columns
     * @param epsilon Zero threshold (values <= epsilon are considered zero)
     */
    static CSRMatrix from_dense(const T* dense, size_type rows, size_type cols, 
                                 T epsilon = T{0}) {
        CSRMatrix result(rows, cols);
        
        for (size_type i = 0; i < rows; ++i) {
            for (size_type j = 0; j < cols; ++j) {
                T val = dense[i * cols + j];
                if (std::abs(val) > epsilon) {
                    result.values_.push_back(val);
                    result.col_indices_.push_back(static_cast<IndexType>(j));
                }
            }
            result.row_ptrs_[i + 1] = static_cast<IndexType>(result.values_.size());
        }
        
        return result;
    }
    
    // Accessors
    size_type rows() const noexcept { return rows_; }
    size_type cols() const noexcept { return cols_; }
    size_type nnz() const noexcept { return values_.size(); }
    
    const std::vector<T>& values() const noexcept { return values_; }
    const std::vector<IndexType>& col_indices() const noexcept { return col_indices_; }
    const std::vector<IndexType>& row_ptrs() const noexcept { return row_ptrs_; }
    
    /**
     * @brief Get element at (row, col)
     * @return Value at (row, col), or 0 if not stored
     */
    T operator()(size_type row, size_type col) const {
        if (row >= rows_ || col >= cols_) {
            throw std::out_of_range("CSRMatrix: index out of range");
        }
        
        IndexType start = row_ptrs_[row];
        IndexType end = row_ptrs_[row + 1];
        
        for (IndexType i = start; i < end; ++i) {
            if (col_indices_[i] == static_cast<IndexType>(col)) {
                return values_[i];
            }
        }
        
        return T{0}; // Zero if not found
    }
    
    /**
     * @brief Set element at (row, col)
     * @details This is slow for CSR format - use COO construction for building matrices
     */
    void set(size_type row, size_type col, T value) {
        if (row >= rows_ || col >= cols_) {
            throw std::out_of_range("CSRMatrix: index out of range");
        }
        
        IndexType start = row_ptrs_[row];
        IndexType end = row_ptrs_[row + 1];
        
        // Find insertion point
        IndexType insert_pos = start;
        for (IndexType i = start; i < end; ++i) {
            if (col_indices_[i] == static_cast<IndexType>(col)) {
                // Update existing value
                values_[i] = value;
                return;
            }
            if (col_indices_[i] > static_cast<IndexType>(col)) {
                break;
            }
            insert_pos = i + 1;
        }
        
        // Insert new value
        values_.insert(values_.begin() + insert_pos, value);
        col_indices_.insert(col_indices_.begin() + insert_pos, static_cast<IndexType>(col));
        
        // Update row pointers
        for (size_type i = row + 1; i <= rows_; ++i) {
            row_ptrs_[i]++;
        }
    }
    
    // =========================================================================
    // Matrix-Vector Operations
    // =========================================================================
    
    /**
     * @brief Sparse matrix-vector multiply: y = A * x
     * @param x Input vector (size = cols)
     * @param y Output vector (size = rows)
     */
    void matvec(const T* x, T* y) const {
        for (size_type i = 0; i < rows_; ++i) {
            T sum = T{0};
            IndexType start = row_ptrs_[i];
            IndexType end = row_ptrs_[i + 1];
            
            for (IndexType j = start; j < end; ++j) {
                sum += values_[j] * x[col_indices_[j]];
            }
            
            y[i] = sum;
        }
    }
    
    /**
     * @brief Sparse matrix-vector multiply with std::vector: y = A * x
     */
    std::vector<T> operator*(const std::vector<T>& x) const {
        if (x.size() != cols_) {
            throw std::invalid_argument("CSRMatrix: vector size mismatch");
        }
        
        std::vector<T> y(rows_);
        matvec(x.data(), y.data());
        return y;
    }
    
    /**
     * @brief Sparse matrix-vector multiply-add: y = alpha * A * x + beta * y
     */
    void matvec(T alpha, const T* x, T beta, T* y) const {
        for (size_type i = 0; i < rows_; ++i) {
            T sum = T{0};
            IndexType start = row_ptrs_[i];
            IndexType end = row_ptrs_[i + 1];
            
            for (IndexType j = start; j < end; ++j) {
                sum += values_[j] * x[col_indices_[j]];
            }
            
            y[i] = alpha * sum + beta * y[i];
        }
    }
    
    /**
     * @brief Parallel sparse matrix-vector multiply (OpenMP)
     */
    void matvec_parallel(const T* x, T* y) const {
#if defined(_OPENMP)
        #pragma omp parallel for
        for (size_type i = 0; i < rows_; ++i) {
            T sum = T{0};
            IndexType start = row_ptrs_[i];
            IndexType end = row_ptrs_[i + 1];
            
            for (IndexType j = start; j < end; ++j) {
                sum += values_[j] * x[col_indices_[j]];
            }
            
            y[i] = sum;
        }
#else
        matvec(x, y); // Fallback to serial
#endif
    }
    
    // =========================================================================
    // Matrix Operations
    // =========================================================================
    
    /**
     * @brief Transpose matrix
     * @return Transposed matrix in CSR format
     */
    CSRMatrix transpose() const {
        CSRMatrix result(cols_, rows_);
        
        // Count non-zeros per column (which will be rows in transposed)
        std::vector<IndexType> col_counts(cols_ + 1, 0);
        for (IndexType col : col_indices_) {
            col_counts[col + 1]++;
        }
        
        // Convert to cumulative sum (row_ptrs for transposed)
        for (size_type i = 0; i < cols_; ++i) {
            col_counts[i + 1] += col_counts[i];
        }
        
        result.row_ptrs_ = col_counts;
        result.values_.resize(nnz());
        result.col_indices_.resize(nnz());
        
        // Fill in transposed values
        std::vector<IndexType> col_ptrs = col_counts; // Working copy
        
        for (size_type i = 0; i < rows_; ++i) {
            IndexType start = row_ptrs_[i];
            IndexType end = row_ptrs_[i + 1];
            
            for (IndexType j = start; j < end; ++j) {
                IndexType col = col_indices_[j];
                IndexType dest = col_ptrs[col]++;
                
                result.values_[dest] = values_[j];
                result.col_indices_[dest] = static_cast<IndexType>(i);
            }
        }
        
        return result;
    }
    
    /**
     * @brief Element-wise addition: C = A + B
     */
    CSRMatrix operator+(const CSRMatrix& other) const {
        if (rows_ != other.rows_ || cols_ != other.cols_) {
            throw std::invalid_argument("CSRMatrix: dimension mismatch");
        }
        
        CSRMatrix result(rows_, cols_);
        
        for (size_type i = 0; i < rows_; ++i) {
            IndexType a_start = row_ptrs_[i];
            IndexType a_end = row_ptrs_[i + 1];
            IndexType b_start = other.row_ptrs_[i];
            IndexType b_end = other.row_ptrs_[i + 1];
            
            IndexType a_ptr = a_start;
            IndexType b_ptr = b_start;
            
            // Merge sorted rows
            while (a_ptr < a_end || b_ptr < b_end) {
                if (a_ptr >= a_end) {
                    // Only B has elements left
                    result.values_.push_back(other.values_[b_ptr]);
                    result.col_indices_.push_back(other.col_indices_[b_ptr]);
                    b_ptr++;
                }
                else if (b_ptr >= b_end) {
                    // Only A has elements left
                    result.values_.push_back(values_[a_ptr]);
                    result.col_indices_.push_back(col_indices_[a_ptr]);
                    a_ptr++;
                }
                else {
                    IndexType a_col = col_indices_[a_ptr];
                    IndexType b_col = other.col_indices_[b_ptr];
                    
                    if (a_col < b_col) {
                        result.values_.push_back(values_[a_ptr]);
                        result.col_indices_.push_back(a_col);
                        a_ptr++;
                    }
                    else if (b_col < a_col) {
                        result.values_.push_back(other.values_[b_ptr]);
                        result.col_indices_.push_back(b_col);
                        b_ptr++;
                    }
                    else { // a_col == b_col
                        T sum = values_[a_ptr] + other.values_[b_ptr];
                        if (sum != T{0}) { // Don't store explicit zeros
                            result.values_.push_back(sum);
                            result.col_indices_.push_back(a_col);
                        }
                        a_ptr++;
                        b_ptr++;
                    }
                }
            }
            
            result.row_ptrs_[i + 1] = static_cast<IndexType>(result.values_.size());
        }
        
        return result;
    }
    
    /**
     * @brief Scalar multiplication: B = alpha * A
     */
    CSRMatrix operator*(T alpha) const {
        CSRMatrix result = *this;
        for (auto& val : result.values_) {
            val *= alpha;
        }
        return result;
    }
    
    /**
     * @brief Matrix-matrix multiplication: C = A * B (SpGEMM)
     * @details Uses row-wise product algorithm
     */
    CSRMatrix matmul(const CSRMatrix& B) const {
        if (cols_ != B.rows_) {
            throw std::invalid_argument("CSRMatrix: incompatible dimensions for multiplication");
        }
        
        CSRMatrix result(rows_, B.cols_);
        std::vector<T> row_buffer(B.cols_, T{0});
        std::vector<bool> row_mask(B.cols_, false);
        std::vector<IndexType> row_indices;
        row_indices.reserve(B.cols_);
        
        for (size_type i = 0; i < rows_; ++i) {
            row_indices.clear();
            
            // Compute row i of result
            IndexType a_start = row_ptrs_[i];
            IndexType a_end = row_ptrs_[i + 1];
            
            for (IndexType a_ptr = a_start; a_ptr < a_end; ++a_ptr) {
                IndexType k = col_indices_[a_ptr];
                T a_val = values_[a_ptr];
                
                IndexType b_start = B.row_ptrs_[k];
                IndexType b_end = B.row_ptrs_[k + 1];
                
                for (IndexType b_ptr = b_start; b_ptr < b_end; ++b_ptr) {
                    IndexType j = B.col_indices_[b_ptr];
                    T b_val = B.values_[b_ptr];
                    
                    if (!row_mask[j]) {
                        row_mask[j] = true;
                        row_indices.push_back(j);
                    }
                    
                    row_buffer[j] += a_val * b_val;
                }
            }
            
            // Sort and add to result
            std::sort(row_indices.begin(), row_indices.end());
            
            for (IndexType j : row_indices) {
                if (row_buffer[j] != T{0}) {
                    result.values_.push_back(row_buffer[j]);
                    result.col_indices_.push_back(j);
                }
                row_buffer[j] = T{0};
                row_mask[j] = false;
            }
            
            result.row_ptrs_[i + 1] = static_cast<IndexType>(result.values_.size());
        }
        
        return result;
    }
    
    // =========================================================================
    // Utility Functions
    // =========================================================================
    
    /**
     * @brief Convert to dense matrix (row-major)
     */
    std::vector<T> to_dense() const {
        std::vector<T> dense(rows_ * cols_, T{0});
        
        for (size_type i = 0; i < rows_; ++i) {
            IndexType start = row_ptrs_[i];
            IndexType end = row_ptrs_[i + 1];
            
            for (IndexType j = start; j < end; ++j) {
                dense[i * cols_ + col_indices_[j]] = values_[j];
            }
        }
        
        return dense;
    }
    
    /**
     * @brief Compute sparsity (fraction of non-zeros)
     */
    double sparsity() const noexcept {
        if (rows_ == 0 || cols_ == 0) return 0.0;
        return static_cast<double>(nnz()) / (rows_ * cols_);
    }
    
    /**
     * @brief Check if matrix is symmetric
     */
    bool is_symmetric(T epsilon = T{1e-10}) const {
        if (rows_ != cols_) return false;
        
        for (size_type i = 0; i < rows_; ++i) {
            IndexType start = row_ptrs_[i];
            IndexType end = row_ptrs_[i + 1];
            
            for (IndexType j = start; j < end; ++j) {
                IndexType col = col_indices_[j];
                T val = values_[j];
                T transpose_val = (*this)(col, i);
                
                if (std::abs(val - transpose_val) > epsilon) {
                    return false;
                }
            }
        }
        
        return true;
    }
};

// =============================================================================
// Convenience Functions
// =============================================================================

/**
 * @brief Create identity matrix
 */
template<typename T, typename IndexType = int32_t>
CSRMatrix<T, IndexType> identity_matrix(std::size_t n) {
    std::vector<IndexType> row_indices(n);
    std::vector<IndexType> col_indices(n);
    std::vector<T> values(n, T{1});
    
    std::iota(row_indices.begin(), row_indices.end(), 0);
    std::iota(col_indices.begin(), col_indices.end(), 0);
    
    return CSRMatrix<T, IndexType>(n, n, row_indices, col_indices, values);
}

/**
 * @brief Create diagonal matrix
 */
template<typename T, typename IndexType = int32_t>
CSRMatrix<T, IndexType> diagonal_matrix(const std::vector<T>& diag) {
    std::size_t n = diag.size();
    std::vector<IndexType> row_indices(n);
    std::vector<IndexType> col_indices(n);
    
    std::iota(row_indices.begin(), row_indices.end(), 0);
    std::iota(col_indices.begin(), col_indices.end(), 0);
    
    return CSRMatrix<T, IndexType>(n, n, row_indices, col_indices, diag);
}

} // namespace sparse
} // namespace cpp_utilities
