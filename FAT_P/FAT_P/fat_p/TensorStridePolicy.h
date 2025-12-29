/**
 * @file TensorStridePolicy.h
 * @brief Multi-dimensional tensor iterator policy for PolicyIterator.
 *
 * @layer Policy
 *
 * Provides true N-dimensional tensor iteration with configurable shape and strides.
 * Uses SmallVector for zero-allocation storage of typical tensor dimensions (≤8D).
 * 
 * PERFORMANCE CHARACTERISTICS:
 * 
 * CONTIGUOUS ITERATION (row-major default, or strides matching natural layout):
 * - advance()/retreat(): O(1) constant time
 * - Detected automatically at construction
 * - Use shape-only constructor for guaranteed contiguous
 * 
 * NON-CONTIGUOUS ITERATION (strided, transposed, padded layouts):
 * - advance()/retreat(): O(1) amortized, O(dims) worst case on rollover
 * - currentOffset(): O(1) - cached value
 * 
 * Memory: Zero heap allocation for ≤8 dimensions (MaxInlineDims).
 * 
 * CONTIGUOUS vs NON-CONTIGUOUS DETECTION:
 * @code
 * // CONTIGUOUS (O(1) iteration):
 * TensorStridePolicy<T> policy({100, 200});              // Shape-only = row-major
 * TensorStridePolicy<T> policy({100, 200}, {200, 1});    // Explicit row-major strides
 * 
 * // NON-CONTIGUOUS (O(1) amortized iteration):
 * TensorStridePolicy<T> policy({100, 200}, {256, 1});    // Padded rows (pitch=256)
 * TensorStridePolicy<T> policy({200, 100}, {1, 200});    // Column-major traversal
 * @endcode
 * 
 * TRAVERSAL ORDER:
 * Iteration always varies the LAST dimension fastest (like an odometer).
 * The shape array determines traversal order; strides determine memory mapping.
 * 
 * To change traversal order, permute the dimension list (both shape AND strides).
 *
 * Example: 3x4 matrix in row-major storage, row-major traversal:
 *   Shape:  {3, 4}  (3 rows, 4 columns)
 *   Strides: {4, 1} (row stride = 4, column stride = 1)
 *   Iteration: [0,0], [0,1], [0,2], [0,3], [1,0], ... [2,3]
 *
 * Example: Same matrix, column-major traversal (permute dimensions):
 *   Shape:  {4, 3}  (4 columns, 3 rows - PERMUTED)
 *   Strides: {1, 4} (column stride = 1, row stride = 4 - PERMUTED)
 *   Iteration: [0,0], [1,0], [2,0], [0,1], ... [2,3]
 *
 * MEMORY FOOTPRINT:
 * With default MaxInlineDims=8, sizeof(TensorStridePolicy) ≈ 296 bytes.
 * This is the tradeoff for zero heap allocation on ≤8 dimensions.
 * If memory is critical, reduce MaxInlineDims (e.g., MaxInlineDims=4 for ≈152 bytes).
 *
 * MEMORY REQUIREMENTS:
 * The caller must ensure that ALL computed offsets lie within [0, end-base).
 * For padded/pitched layouts, this means (base,end) must span the full
 * allocated region, not just the logical element count.
 *
 * OFFSET CONSTRAINTS (CRITICAL):
 * - All computed offsets must be NON-NEGATIVE (offset >= 0)
 * - Negative strides that would produce negative offsets are INVALID
 * - Violating these constraints is undefined behavior in release builds
 *
 * OVERFLOW BEHAVIOR:
 * - Product of all shape dimensions must fit in std::size_t
 * - Computed strides and offsets must fit in std::ptrdiff_t
 * - Overflow in these computations is UNDEFINED BEHAVIOR
 * - No overflow detection is performed; caller must ensure validity
 *
 * COPYABILITY:
 * TensorStridePolicy is CopyConstructible. Iterators using this policy
 * require copy semantics for post-increment/decrement operations.
 *
 * @note Thread-safety: Not thread-safe; policy carries mutable iteration state.
 *
 * @see PolicyIterator.h for the base iterator class.
 * @see SmallVector.h for the inline-storage container used internally.
 */

#pragma once
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <utility>

#include "enforce.h"
#include "SmallVector.h"

namespace fat_p::iterator {

/// Default inline capacity for tensor dimension storage (covers up to 8D tensors without heap)
inline constexpr std::size_t kDefaultTensorDims = 8;

/**
 * @brief Multi-dimensional tensor iterator policy.
 * @tparam T Element type.
 * @tparam MaxInlineDims Maximum dimensions stored inline (default: 8).
 *
 * Iterates through all elements of an N-dimensional tensor. The shape defines
 * the size of each dimension (and traversal order - last dimension fastest),
 * while strides define memory offsets.
 * 
 * Uses cached indices with incremental offset updates for O(1) amortized advance/retreat.
 */
template <typename T, std::size_t MaxInlineDims = kDefaultTensorDims>
struct TensorStridePolicy {
    using iterator_category = std::bidirectional_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = T;
    using pointer = T*;
    using reference = T&;

    /// Marker for PolicyIterator to detect tensor policies
    static constexpr bool kIsTensorPolicy = true;

private:
    using ShapeVec = SmallVector<std::size_t, MaxInlineDims>;
    using StrideVec = SmallVector<std::ptrdiff_t, MaxInlineDims>;
    using IndexVec = SmallVector<std::size_t, MaxInlineDims>;

    ShapeVec mShape;              ///< Size of each dimension
    StrideVec mStrides;           ///< Memory stride for each dimension
    IndexVec mIndices;            ///< Current multi-dimensional indices (cached)
    std::ptrdiff_t mOffset;       ///< Current memory offset (cached, updated incrementally)
    std::size_t mPosition;        ///< Linear position 0..total (for atEnd check)
    std::size_t mTotal;           ///< Total number of elements
    bool mIsContiguous;           ///< True if iteration is memory-contiguous (enables O(1) advance)
    
    /// Check if strides represent contiguous row-major layout
    [[nodiscard]] bool computeIsContiguous() const noexcept {
        // Contiguous means: stride[i] == product of shape[i+1..N-1]
        // i.e., row-major layout with no gaps
        std::ptrdiff_t expectedStride = 1;
        for (std::size_t i = mShape.size(); i-- > 0; ) {
            if (mStrides[i] != expectedStride) {
                return false;
            }
            expectedStride *= static_cast<std::ptrdiff_t>(mShape[i]);
        }
        return true;
    }

public:
    /**
     * @brief Constructs tensor policy with given shape and strides.
     * @param shape Size of each dimension (e.g., {3, 4} for 3x4 matrix).
     * @param strides Memory stride for each dimension (e.g., {4, 1} for row-major).
     * @pre shape.size() == strides.size()
     * @pre shape must not be empty
     * @pre All shape dimensions must be > 0
     * @pre Product of shape dimensions must not overflow size_t
     */
    TensorStridePolicy(std::initializer_list<std::size_t> shape, 
                       std::initializer_list<std::ptrdiff_t> strides)
        : mShape(shape)
        , mStrides(strides)
        , mIndices(shape.size(), std::size_t{0})
        , mOffset(0)
        , mPosition(0)
        , mTotal(1)
        , mIsContiguous(false)  // Computed below
    {
        enforce(!mShape.empty(), "Shape cannot be empty");
        enforce(mShape.size() == mStrides.size(), "Shape and strides must have same dimensions");
        
        for (std::size_t i = 0; i < mShape.size(); ++i) {
            enforce(mShape[i] > 0, "All dimensions must be > 0");
            mTotal *= mShape[i];
        }
        mIsContiguous = computeIsContiguous();
    }

    /**
     * @brief Constructs tensor policy with shape and automatic row-major strides.
     * @param shape Size of each dimension.
     * @pre shape must not be empty
     * @pre All shape dimensions must be > 0
     * @pre Product of shape dimensions must not overflow size_t
     * @note Row-major strides are always contiguous, enabling O(1) advance.
     */
    explicit TensorStridePolicy(std::initializer_list<std::size_t> shape)
        : mShape(shape)
        , mStrides()
        , mIndices(shape.size(), std::size_t{0})
        , mOffset(0)
        , mPosition(0)
        , mTotal(1)
        , mIsContiguous(true)  // Row-major strides are always contiguous
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
     * @brief Returns cached memory offset from base pointer for current position.
     * @return Offset in elements from base pointer.
     * @pre !atEnd() - offset is undefined for end position
     * @note O(1) - returns cached value, no computation.
     */
    [[nodiscard]] std::ptrdiff_t currentOffset() const {
        enforce(!atEnd(), "Cannot compute offset for end position");
        return mOffset;
    }

    /**
     * @brief Returns true if iteration is memory-contiguous.
     * 
     * Contiguous iteration (row-major with no gaps) enables O(1) advance/retreat.
     * Non-contiguous iteration (strided, transposed, padded) uses odometer-style
     * index tracking with O(1) amortized, O(dims) worst case.
     */
    [[nodiscard]] bool isContiguous() const noexcept {
        return mIsContiguous;
    }

    /**
     * @brief Returns copy of current multi-dimensional indices.
     * @return Vector of indices, one per dimension.
     * @pre !atEnd() - indices are undefined for end position
     * @note O(dims) - computes from position for contiguous iteration.
     */
    [[nodiscard]] IndexVec currentIndices() const {
        enforce(!atEnd(), "Cannot compute indices for end position");
        
        // For contiguous iteration, indices are not maintained incrementally
        // Compute them from linear position
        if (mIsContiguous) {
            IndexVec result(mShape.size());
            std::size_t remaining = mPosition;
            for (std::size_t d = mShape.size(); d-- > 0; ) {
                result[d] = remaining % mShape[d];
                remaining /= mShape[d];
            }
            return result;
        }
        
        return mIndices;
    }

    // ----------------------------------------------------------------
    // Navigation (O(1) for contiguous, O(1) amortized for non-contiguous)
    // ----------------------------------------------------------------

    /// Advance to next position
    /// @pre !atEnd() - cannot advance past end
    /// @note O(1) for contiguous iteration; O(1) amortized, O(dims) worst case otherwise
    void advance() {
        enforce(!atEnd(), "Cannot advance past end");
        ++mPosition;
        
        if (mPosition >= mTotal) {
            // At end - indices/offset are now undefined
            return;
        }
        
        // FAST PATH: Contiguous iteration - just increment offset
        if (mIsContiguous) {
            ++mOffset;
            // Note: mIndices not updated (use currentIndices() only when needed)
            return;
        }
        
        // MEDIUM PATH: Non-contiguous, but last dimension doesn't rollover (common case)
        const std::size_t lastDim = mShape.size() - 1;
        ++mIndices[lastDim];
        mOffset += mStrides[lastDim];
        
        if (mIndices[lastDim] < mShape[lastDim]) {
            return;  // No rollover, done
        }
        
        // SLOW PATH: Rollover in last dimension - propagate carry
        mOffset -= static_cast<std::ptrdiff_t>(mIndices[lastDim]) * mStrides[lastDim];
        mIndices[lastDim] = 0;
        
        // Continue odometer increment for remaining dimensions
        for (std::size_t d = lastDim; d-- > 0; ) {
            ++mIndices[d];
            mOffset += mStrides[d];
            
            if (mIndices[d] < mShape[d]) {
                return;  // No rollover, done
            }
            
            // Rollover: reset this dimension, continue to next
            mOffset -= static_cast<std::ptrdiff_t>(mIndices[d]) * mStrides[d];
            mIndices[d] = 0;
        }
    }

    /// Retreat to previous position using odometer-style decrement
    /// @pre !atBegin() - cannot retreat before begin
    /// @note O(1) for contiguous iteration; O(1) amortized, O(dims) worst case otherwise
    void retreat() {
        enforce(!atBegin(), "Cannot retreat before begin");
        
        bool wasAtEnd = atEnd();
        --mPosition;
        
        // FAST PATH: Contiguous iteration
        if (mIsContiguous) {
            if (wasAtEnd) {
                // Transitioning from end to last element
                mOffset = static_cast<std::ptrdiff_t>(mTotal - 1);
            } else {
                --mOffset;
            }
            // Note: mIndices not updated (use currentIndices() only when needed)
            return;
        }
        
        // NON-CONTIGUOUS PATH
        if (wasAtEnd) {
            // Transitioning from end to last valid element
            // Reset indices to last element of each dimension
            mOffset = 0;
            for (std::size_t d = 0; d < mShape.size(); ++d) {
                mIndices[d] = mShape[d] - 1;
                mOffset += static_cast<std::ptrdiff_t>(mIndices[d]) * mStrides[d];
            }
            return;
        }
        
        // MEDIUM PATH: Last dimension doesn't borrow (common case)
        const std::size_t lastDim = mShape.size() - 1;
        if (mIndices[lastDim] > 0) {
            --mIndices[lastDim];
            mOffset -= mStrides[lastDim];
            return;
        }
        
        // SLOW PATH: Borrow from last dimension - propagate
        mIndices[lastDim] = mShape[lastDim] - 1;
        mOffset += static_cast<std::ptrdiff_t>(mIndices[lastDim]) * mStrides[lastDim];
        
        // Continue odometer decrement for remaining dimensions
        for (std::size_t d = lastDim; d-- > 0; ) {
            if (mIndices[d] > 0) {
                --mIndices[d];
                mOffset -= mStrides[d];
                return;
            }
            
            // Borrow: wrap this dimension to max, continue to next
            mIndices[d] = mShape[d] - 1;
            mOffset += static_cast<std::ptrdiff_t>(mIndices[d]) * mStrides[d];
        }
    }

    /// Set position to end (past last element)
    void setToEnd() {
        mPosition = mTotal;
        // Indices/offset become undefined at end
    }

    /// Set position to beginning
    void setToBegin() {
        mPosition = 0;
        mOffset = 0;
        for (std::size_t d = 0; d < mIndices.size(); ++d) {
            mIndices[d] = 0;
        }
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

// ============================================================================
// Lightweight Specializations for 1D and 2D
// ============================================================================
// These bypass the tensor machinery for maximum performance when dimensions
// are known. They work like StridePolicy but with bounds tracking.

/**
 * @brief Lightweight 1D strided iteration policy.
 * @tparam T Element type.
 *
 * For iterating with a fixed stride over a 1D range. Much faster than
 * TensorStridePolicy<T>({count}, {stride}) because it avoids SmallVector
 * and the tensor path in PolicyIterator.
 *
 * Use this when you know you're iterating a single dimension with a stride.
 *
 * Example: Iterate every 4th element
 * @code
 * Stride1DPolicy<int> policy(1000, 4);  // 1000 elements, stride 4
 * auto it = PolicyIterator<int, Stride1DPolicy<int>>::begin(data, data+4000, policy);
 * @endcode
 */
template <typename T>
struct Stride1DPolicy {
    using iterator_category = std::bidirectional_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = T;
    using pointer = T*;
    using reference = T&;

    /// Marker - NOT a tensor policy (avoids tensor path in PolicyIterator)
    static constexpr bool kIsTensorPolicy = false;

private:
    std::size_t mCount;           ///< Number of elements to visit
    std::ptrdiff_t mStride;       ///< Stride between elements
    std::size_t mPosition;        ///< Current position [0, mCount]

public:
    /**
     * @brief Construct 1D stride policy.
     * @param count Number of elements to iterate.
     * @param stride Distance between elements (default 1 = contiguous).
     */
    explicit Stride1DPolicy(std::size_t count, std::ptrdiff_t stride = 1)
        : mCount(count), mStride(stride), mPosition(0) {
        enforce(count > 0, "Count must be > 0");
        enforce(stride != 0, "Stride cannot be 0");
    }

    [[nodiscard]] bool atEnd() const noexcept { return mPosition >= mCount; }
    [[nodiscard]] bool atBegin() const noexcept { return mPosition == 0; }
    [[nodiscard]] std::size_t count() const noexcept { return mCount; }
    [[nodiscard]] std::ptrdiff_t stride() const noexcept { return mStride; }
    [[nodiscard]] std::size_t position() const noexcept { return mPosition; }

    void advance(pointer& ptr) {
        enforce(!atEnd(), "Cannot advance past end");
        ptr += mStride;
        ++mPosition;
    }

    void retreat(pointer& ptr) {
        enforce(!atBegin(), "Cannot retreat before begin");
        ptr -= mStride;
        --mPosition;
    }

    void setToEnd(pointer& ptr, pointer base) {
        ptr = base + static_cast<std::ptrdiff_t>(mCount) * mStride;
        mPosition = mCount;
    }
};

/**
 * @brief Lightweight 2D strided iteration policy.
 * @tparam T Element type.
 *
 * For iterating a 2D matrix with configurable traversal and memory layout.
 * Much faster than TensorStridePolicy<T>({rows, cols}, {rowStride, colStride}).
 *
 * Iteration is row-major (columns vary fastest) unless you swap the parameters.
 *
 * Example: Iterate column 0 of a 1000x1000 row-major matrix
 * @code
 * // Shape: 1000 rows, 1 col; Stride: 1000 between rows, 1 between cols
 * Stride2DPolicy<int> policy(1000, 1, 1000, 1);
 * @endcode
 *
 * Example: Full row-major iteration of 100x200 matrix
 * @code
 * Stride2DPolicy<int> policy(100, 200, 200, 1);  // 100 rows, 200 cols
 * @endcode
 */
template <typename T>
struct Stride2DPolicy {
    using iterator_category = std::bidirectional_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = T;
    using pointer = T*;
    using reference = T&;

    static constexpr bool kIsTensorPolicy = false;

private:
    std::size_t mRows;            ///< Number of rows (outer dimension)
    std::size_t mCols;            ///< Number of columns (inner dimension)
    std::ptrdiff_t mRowStride;    ///< Stride between rows
    std::ptrdiff_t mColStride;    ///< Stride between columns
    std::size_t mRow;             ///< Current row
    std::size_t mCol;             ///< Current column
    std::size_t mTotal;           ///< Total elements (rows * cols)
    std::size_t mPosition;        ///< Linear position for end check

public:
    /**
     * @brief Construct 2D stride policy.
     * @param rows Number of rows.
     * @param cols Number of columns.
     * @param rowStride Memory stride between rows.
     * @param colStride Memory stride between columns.
     */
    Stride2DPolicy(std::size_t rows, std::size_t cols,
                   std::ptrdiff_t rowStride, std::ptrdiff_t colStride)
        : mRows(rows), mCols(cols)
        , mRowStride(rowStride), mColStride(colStride)
        , mRow(0), mCol(0)
        , mTotal(rows * cols), mPosition(0) {
        enforce(rows > 0 && cols > 0, "Dimensions must be > 0");
    }

    /// Convenience: Row-major contiguous matrix
    Stride2DPolicy(std::size_t rows, std::size_t cols)
        : Stride2DPolicy(rows, cols, static_cast<std::ptrdiff_t>(cols), 1) {}

    [[nodiscard]] bool atEnd() const noexcept { return mPosition >= mTotal; }
    [[nodiscard]] bool atBegin() const noexcept { return mPosition == 0; }
    [[nodiscard]] std::size_t rows() const noexcept { return mRows; }
    [[nodiscard]] std::size_t cols() const noexcept { return mCols; }
    [[nodiscard]] std::size_t row() const noexcept { return mRow; }
    [[nodiscard]] std::size_t col() const noexcept { return mCol; }

    void advance(pointer& ptr) {
        enforce(!atEnd(), "Cannot advance past end");
        ++mPosition;
        
        // Advance column (inner dimension)
        ++mCol;
        ptr += mColStride;
        
        if (mCol >= mCols) {
            // Wrap to next row
            mCol = 0;
            ++mRow;
            // Adjust pointer: go back cols * colStride, forward one rowStride
            ptr += mRowStride - static_cast<std::ptrdiff_t>(mCols) * mColStride;
        }
    }

    void retreat(pointer& ptr) {
        enforce(!atBegin(), "Cannot retreat before begin");
        --mPosition;
        
        if (mCol > 0) {
            --mCol;
            ptr -= mColStride;
        } else {
            // Wrap to previous row's last column
            mCol = mCols - 1;
            --mRow;
            // Adjust pointer: forward (cols-1) * colStride, back one rowStride
            ptr += static_cast<std::ptrdiff_t>(mCols - 1) * mColStride - mRowStride;
        }
    }

    void setToEnd(pointer& ptr, pointer base) {
        mRow = mRows;
        mCol = 0;
        mPosition = mTotal;
        ptr = base + static_cast<std::ptrdiff_t>(mRows) * mRowStride;
    }
};

/**
 * @brief Helper function to create row-major tensor policy.
 * @tparam T Element type.
 * @param shape Dimensions of the tensor.
 * @return TensorStridePolicy with row-major strides.
 *
 * Row-major means stride[i] = product of all dimensions after i.
 * Traversal order is last-dimension-fastest (standard C/C++ order).
 */
template <typename T>
TensorStridePolicy<T> makeRowMajor(std::initializer_list<std::size_t> shape) {
    return TensorStridePolicy<T>(shape);
}

} // namespace fat_p::iterator
