/**
 * @file TensorStridePolicy.h
 * @brief Multi-dimensional tensor iterator policy for PolicyIterator.
 *
 * @layer Policy
 *
 * Provides true N-dimensional tensor iteration with configurable shape and strides.
 * Iterates through all elements by varying indices from innermost dimension first,
 * handling rollover like an odometer. Supports non-contiguous memory layouts.
 *
 * Example: For a 3x4 matrix in row-major layout:
 * - Shape: {3, 4} (3 rows, 4 columns)
 * - Strides: {4, 1} (row stride = 4, column stride = 1)
 * - Iteration order: [0,0], [0,1], [0,2], [0,3], [1,0], ... [2,3]
 *
 * For column-major iteration of the same matrix:
 * - Use strides: {1, 3} (swap iteration order)
 * - Iteration order: [0,0], [1,0], [2,0], [0,1], ... [2,3]
 *
 * @note Complexity: O(dims) for currentOffset() due to index calculation.
 * @note Thread-safety: Not thread-safe; policy carries mutable iteration state.
 *
 * @see PolicyIterator.h for the base iterator class.
 */

#pragma once
#include <cstddef>
#include <iterator>
#include <numeric>
#include <utility>
#include <vector>

#include "enforce.h"

namespace fat_p::iterator {

/**
 * @brief Multi-dimensional tensor iterator policy.
 * @tparam T Element type.
 *
 * Iterates through all elements of an N-dimensional tensor. The shape defines
 * the size of each dimension, and strides define memory offsets. Position is
 * tracked as a linear index from 0 to total-1, with total being the end state.
 */
template <typename T>
struct TensorStridePolicy {
    using iterator_category = std::bidirectional_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = T;
    using pointer = T*;
    using reference = T&;

    /// Marker for PolicyIterator to detect tensor policies
    static constexpr bool kIsTensorPolicy = true;

    std::vector<std::size_t> mShape;       ///< Size of each dimension
    std::vector<std::ptrdiff_t> mStrides;  ///< Memory stride for each dimension
    std::size_t mPosition;                  ///< Linear position 0..total (total = end)
    std::size_t mTotal;                     ///< Total number of elements

    /**
     * @brief Constructs tensor policy with given shape and strides.
     * @param shape Size of each dimension (e.g., {3, 4} for 3x4 matrix).
     * @param strides Memory stride for each dimension (e.g., {4, 1} for row-major).
     * @pre shape.size() == strides.size()
     * @pre shape must not be empty
     * @pre All shape dimensions must be > 0
     */
    TensorStridePolicy(std::vector<std::size_t> shape, std::vector<std::ptrdiff_t> strides)
        : mShape(std::move(shape))
        , mStrides(std::move(strides))
        , mPosition(0)
        , mTotal(1)
    {
        enforce(!mShape.empty(), "Shape cannot be empty");
        enforce(mShape.size() == mStrides.size(), "Shape and strides must have same dimensions");
        
        for (std::size_t i = 0; i < mShape.size(); ++i) {
            enforce(mShape[i] > 0, "All dimensions must be > 0");
            mTotal *= mShape[i];
        }
    }

    /**
     * @brief Constructs tensor policy with shape and automatic row-major strides.
     * @param shape Size of each dimension.
     */
    explicit TensorStridePolicy(std::vector<std::size_t> shape)
        : mShape(std::move(shape))
        , mStrides()
        , mPosition(0)
        , mTotal(1)
    {
        enforce(!mShape.empty(), "Shape cannot be empty");
        
        mStrides.resize(mShape.size());
        
        // Compute row-major strides: stride[i] = product of all dimensions after i
        std::ptrdiff_t stride = 1;
        for (std::size_t i = mShape.size(); i-- > 0; ) {
            enforce(mShape[i] > 0, "All dimensions must be > 0");
            mStrides[i] = stride;
            stride *= static_cast<std::ptrdiff_t>(mShape[i]);
        }
        
        mTotal = static_cast<std::size_t>(stride);
    }

    // ----------------------------------------------------------------
    // Position and offset computation
    // ----------------------------------------------------------------

    /**
     * @brief Computes memory offset from base pointer for current position.
     * @return Offset in elements from base pointer.
     *
     * Converts linear position to multi-dimensional indices, then computes
     * the memory offset using strides.
     */
    [[nodiscard]] std::ptrdiff_t currentOffset() const {
        std::ptrdiff_t offset = 0;
        std::size_t pos = mPosition;
        
        // Convert linear position to indices (innermost dimension first)
        // and accumulate offset using strides
        for (std::size_t d = mShape.size(); d-- > 0; ) {
            std::size_t idx = pos % mShape[d];
            offset += static_cast<std::ptrdiff_t>(idx) * mStrides[d];
            pos /= mShape[d];
        }
        
        return offset;
    }

    /**
     * @brief Returns multi-dimensional indices for current position.
     * @return Vector of indices, one per dimension.
     */
    [[nodiscard]] std::vector<std::size_t> currentIndices() const {
        std::vector<std::size_t> indices(mShape.size());
        std::size_t pos = mPosition;
        
        for (std::size_t d = mShape.size(); d-- > 0; ) {
            indices[d] = pos % mShape[d];
            pos /= mShape[d];
        }
        
        return indices;
    }

    // ----------------------------------------------------------------
    // Navigation
    // ----------------------------------------------------------------

    /// Advance to next position
    void advance() {
        if (mPosition < mTotal) {
            ++mPosition;
        }
    }

    /// Retreat to previous position
    void retreat() {
        if (mPosition > 0) {
            --mPosition;
        }
    }

    /// Set position to end (past last element)
    void setToEnd() {
        mPosition = mTotal;
    }

    /// Set position to beginning
    void setToBegin() {
        mPosition = 0;
    }

    // ----------------------------------------------------------------
    // State queries
    // ----------------------------------------------------------------

    /// Check if at end position
    [[nodiscard]] bool atEnd() const { return mPosition >= mTotal; }

    /// Check if at beginning position
    [[nodiscard]] bool atBegin() const { return mPosition == 0; }

    /// Get current linear position
    [[nodiscard]] std::size_t position() const { return mPosition; }

    /// Get total number of elements
    [[nodiscard]] std::size_t total() const { return mTotal; }

    /// Get number of dimensions
    [[nodiscard]] std::size_t dims() const { return mShape.size(); }

    /// Get shape of specific dimension
    [[nodiscard]] std::size_t shape(std::size_t dim) const {
        enforce(dim < mShape.size(), "Dimension out of range");
        return mShape[dim];
    }

    /// Get stride of specific dimension
    [[nodiscard]] std::ptrdiff_t stride(std::size_t dim) const {
        enforce(dim < mStrides.size(), "Dimension out of range");
        return mStrides[dim];
    }
};

/**
 * @brief Helper function to create row-major tensor policy.
 * @tparam T Element type.
 * @param shape Dimensions of the tensor.
 * @return TensorStridePolicy with row-major strides.
 */
template <typename T>
TensorStridePolicy<T> makeRowMajor(std::vector<std::size_t> shape) {
    return TensorStridePolicy<T>(std::move(shape));
}

/**
 * @brief Helper function to create column-major tensor policy.
 * @tparam T Element type.
 * @param shape Dimensions of the tensor.
 * @return TensorStridePolicy with column-major strides.
 */
template <typename T>
TensorStridePolicy<T> makeColumnMajor(std::vector<std::size_t> shape) {
    enforce(!shape.empty(), "Shape cannot be empty");
    
    std::vector<std::ptrdiff_t> strides(shape.size());
    
    // Column-major: stride[i] = product of all dimensions before i
    std::ptrdiff_t stride = 1;
    for (std::size_t i = 0; i < shape.size(); ++i) {
        strides[i] = stride;
        stride *= static_cast<std::ptrdiff_t>(shape[i]);
    }
    
    return TensorStridePolicy<T>(std::move(shape), std::move(strides));
}

} // namespace fat_p::iterator
