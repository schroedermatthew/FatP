#pragma once

/*
FATP_META:
  meta_version: 1
  component: Tensor
  file_role: public_header
  path: include/fat_p/Tensor.h
  namespace: fat_p
  layer: Domain
  summary: "Public header for Tensor."
  api_stability: in_work
  related:
    docs_search: "Tensor"
    tests:
      - components/ConcurrencyPolicies/tests/test_RcuIntegration.cpp
      - components/Tensor/tests/test_Tensor.cpp
      - components/Tensor/tests/test_TensorComparison.cpp
      - components/Tensor/tests/test_TensorEinsum.cpp
      - components/Tensor/tests/test_TensorSerializer.cpp
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
 * @file Tensor.h
 * @brief High-performance N-dimensional tensor with policy-based iterators
 *
 *
 * @version 5.1 - Enhanced Safety Features: Bounds Checking, View Lifetime Tracking, RCU Integration
 *
 * @details Production-ready tensor implementation for HPC optimization and scientific computing.
 * Features policy-based iterator system for different traversal patterns and full library integration.
 *
 * Key Features:
 * - Safe shared ownership with std::shared_ptr (no dangling views)
 * - Enhanced bounds checking with detailed error messages (v5.1, debug only)
 * - View lifetime tracking to detect dangling references (v5.1, debug only)
 * - RCU concurrency policy for lock-free tensor sharing (v5.1)
 * - Policy-based iterators (RowMajor, ColumnMajor, Blocked, Strided)
 * - Stride-aware operations
 * - SIMD-aligned memory using AlignedVector (32/64-byte alignment)
 * - View/slice support (zero-copy, safe lifetime)
 * - NumPy-style broadcasting operations
 * - Expected.h integration for safe operations
 * - Complete expression templates with lazy evaluation
 * - Typed contract exceptions (DomainContractError, etc.)
 * - ThreadPool integration for parallel operations
 * - Contextual enforce for noexcept-safe checks
 * - Reshape without reallocation
 * - Debug-only enforce checks (zero overhead in release builds)
 * - SIMD-optimized operations (AVX2/AVX10/AVX-512)
 * - Overflow-safe size computation with CheckedArithmetic
 * - JSON serialization (JsonLite.h)
 * - Binary serialization (BinarySerializer.h)
 * - Equality comparisons with epsilon tolerance
 * - std::hash support
 *
 * Performance:
 * - Float operations: ~490 us for 1M elements (AVX2: 8 floats/instruction)
 * - Double operations: ~1.05 ms for 1M elements (AVX2: 4 doubles/instruction)
 * - With AVX10.1: Even faster performance with wider SIMD registers
 * - Expression templates: 2-5x speedup for chained operations (zero temp allocations)
 * - Parallel operations: 2-8x speedup on multi-core systems (for large tensors)
 * - Debug checks compiled out in release builds (-DNDEBUG)
 *
 * SIMD Support:
 * - MSVC: /arch:AVX2 or /arch:AVX10.1 (recommended)
 * - GCC/Clang: -march=native or -mavx2
 *
 * Requires: C++17
 *
 * @author cpp_utilities
 * @date 2025
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iterator>
#include <memory>
#include <numeric>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <typeinfo>
#include <vector>

#include "AlignedVector.h"
#include "CheckedArithmetic.h"
#include "ConcurrencyPolicies.h"
#include "ContractException.h"
#include "enforce.h"
#include "EnhancedBoundsChecking.h"
#include "Expected.h"
#include "TensorStorage.h"
#include "ThreadPool.h"
#include "ViewLifetimeTracking.h"

#ifdef __AVX2__
#include <immintrin.h>
#endif

#ifdef __AVX512F__
#include <immintrin.h>
// Note: AVX10.1 (/arch:AVX10.1 in MSVC) defines __AVX512F__ and provides
// access to AVX-512 instructions. The current AVX2 implementation also works
// with AVX10/AVX-512 and provides good performance. Future versions may add
// explicit AVX-512 optimizations for even better performance.
#endif

namespace fat_p
{

// Forward declarations for serialization
struct JsonValue;
template <typename FormatPolicy>
class BinarySerializer;

// =============================================================================
// Forward Declarations
// =============================================================================

template <typename T, typename Allocator, typename IteratorPolicy, typename ConcurrencyPolicy>
class Tensor;

// =============================================================================
// Aligned Allocator (using library's AlignedAllocator)
// =============================================================================

/**
 * @brief SIMD-aligned allocator for tensor memory (from AlignedVector.h)
 */
template <typename T, size_t Alignment = 64>
using TensorAllocator = AlignedAllocator<T, Alignment>;

// =============================================================================
// Stride Iterator (Base for all iterators)
// =============================================================================

/**
 * @brief Generic stride-aware iterator
 */
template <typename T>
class StrideIterator
{
public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type = T;
    using difference_type = ptrdiff_t;
    using pointer = T*;
    using reference = T&;

    StrideIterator()
        : mPtr(nullptr)
        , mStride(1)
    {
    }
    StrideIterator(T* ptr, ptrdiff_t stride)
        : mPtr(ptr)
        , mStride(stride)
    {
    }

    reference operator*() const
    {
        return *mPtr;
    }
    pointer operator->() const
    {
        return mPtr;
    }

    StrideIterator& operator++()
    {
        mPtr += mStride;
        return *this;
    }

    StrideIterator operator++(int)
    {
        StrideIterator tmp = *this;
        ++(*this);
        return tmp;
    }

    StrideIterator& operator--()
    {
        mPtr -= mStride;
        return *this;
    }

    StrideIterator operator--(int)
    {
        StrideIterator tmp = *this;
        --(*this);
        return tmp;
    }

    StrideIterator& operator+=(difference_type n)
    {
        mPtr += n * mStride;
        return *this;
    }

    StrideIterator& operator-=(difference_type n)
    {
        mPtr -= n * mStride;
        return *this;
    }

    StrideIterator operator+(difference_type n) const
    {
        return StrideIterator(mPtr + n * mStride, mStride);
    }

    StrideIterator operator-(difference_type n) const
    {
        return StrideIterator(mPtr - n * mStride, mStride);
    }

    difference_type operator-(const StrideIterator& other) const
    {
#ifndef NDEBUG
        FATP_ALWAYS_ENFORCE(mStride != 0, "Iterator has zero stride");
#endif

        ptrdiff_t ptr_diff = mPtr - other.mPtr;

#ifndef NDEBUG
        FATP_ALWAYS_ENFORCE(ptr_diff % mStride == 0,
                            "Iterator distance not a multiple of stride - "
                            "iterators may be from different views or incompatible");
#endif

        return ptr_diff / mStride;
    }

    reference operator[](difference_type n) const
    {
        return *(mPtr + n * mStride);
    }

    bool operator==(const StrideIterator& other) const
    {
        return mPtr == other.mPtr;
    }
    bool operator!=(const StrideIterator& other) const
    {
        return mPtr != other.mPtr;
    }
    bool operator<(const StrideIterator& other) const
    {
        return mPtr < other.mPtr;
    }
    bool operator<=(const StrideIterator& other) const
    {
        return mPtr <= other.mPtr;
    }
    bool operator>(const StrideIterator& other) const
    {
        return mPtr > other.mPtr;
    }
    bool operator>=(const StrideIterator& other) const
    {
        return mPtr >= other.mPtr;
    }

private:
    T* mPtr;
    ptrdiff_t mStride;
};

// =============================================================================
// Iterator Policies
// =============================================================================

/**
 * @brief Row-major iterator policy (default, C-style)
 * Iterates elements in row-major order: row by row
 */
struct RowMajorPolicy
{
    template <typename T>
    using iterator_type = StrideIterator<T>;

    template <typename T>
    using const_iterator_type = StrideIterator<const T>;

    template <typename T>
    static iterator_type<T> make_begin(T* data, const std::vector<size_t>&, const std::vector<ptrdiff_t>&)
    {
        return iterator_type<T>(data, 1);
    }

    template <typename T>
    static iterator_type<T> make_end(T* data, const std::vector<size_t>&, const std::vector<ptrdiff_t>&, size_t size)
    {
        return iterator_type<T>(data + size, 1);
    }

    static constexpr const char* name()
    {
        return "RowMajor";
    }
};

/**
 * @brief Column-major iterator policy (Fortran-style)
 * Iterates elements in column-major order: column by column
 */
struct ColumnMajorPolicy
{
    template <typename T>
    class ColumnIterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = ptrdiff_t;
        using pointer = T*;
        using reference = T&;

        ColumnIterator(T* ptr, const std::vector<size_t>& shape, size_t row, size_t col)
            : mPtr(ptr)
            , mShape(shape)
            , mCurrentRow(row)
            , mCurrentCol(col)
        {
            if (mShape.size() < 2)
            {
                mShape = {1, mShape.empty() ? 0 : mShape[0]};
            }
        }

        reference operator*() const
        {
            return mPtr[mCurrentRow * mShape[1] + mCurrentCol];
        }

        pointer operator->() const
        {
            return &mPtr[mCurrentRow * mShape[1] + mCurrentCol];
        }

        ColumnIterator& operator++()
        {
            ++mCurrentRow;
            if (mCurrentRow >= mShape[0])
            {
                mCurrentRow = 0;
                ++mCurrentCol;
            }
            return *this;
        }

        ColumnIterator operator++(int)
        {
            ColumnIterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const ColumnIterator& other) const
        {
            return mCurrentRow == other.mCurrentRow && mCurrentCol == other.mCurrentCol;
        }

        bool operator!=(const ColumnIterator& other) const
        {
            return !(*this == other);
        }

    private:
        T* mPtr;
        std::vector<size_t> mShape;
        size_t mCurrentRow;
        size_t mCurrentCol;
    };

    template <typename T>
    using iterator_type = ColumnIterator<T>;

    template <typename T>
    using const_iterator_type = ColumnIterator<const T>;

    template <typename T>
    static iterator_type<T> make_begin(T* data, const std::vector<size_t>& shape, const std::vector<ptrdiff_t>&)
    {
        return iterator_type<T>(data, shape, 0, 0);
    }

    template <typename T>
    static iterator_type<T> make_end(T* data, const std::vector<size_t>& shape, const std::vector<ptrdiff_t>&, size_t)
    {
        size_t cols = shape.size() < 2 ? (shape.empty() ? 0 : shape[0]) : shape[1];
        return iterator_type<T>(data, shape, 0, cols);
    }

    static constexpr const char* name()
    {
        return "ColumnMajor";
    }
};

/**
 * @brief Strided iterator policy
 * Properly handles views with non-contiguous strides
 */
struct StridedPolicy
{
    template <typename T>
    class MultiDimIterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = ptrdiff_t;
        using pointer = T*;
        using reference = T&;

        MultiDimIterator(T* data, const std::vector<size_t>& shape, const std::vector<ptrdiff_t>& strides)
            : mBase(data)
            , mShape(shape)
            , mStrides(strides)
        {
            mIndices.resize(shape.size(), 0);
            compute_ptr();
        }

        MultiDimIterator(T* data, const std::vector<size_t>& shape, const std::vector<ptrdiff_t>& strides, bool at_end)
            : mBase(data)
            , mShape(shape)
            , mStrides(strides)
        {
            if (at_end)
            {
                mIndices.resize(shape.size(), 0);
                if (!shape.empty())
                {
                    mIndices[0] = shape[0];
                }
            }
            else
            {
                mIndices.resize(shape.size(), 0);
            }
            compute_ptr();
        }

        reference operator*() const
        {
            return *mPtr;
        }
        pointer operator->() const
        {
            return mPtr;
        }

        MultiDimIterator& operator++()
        {
            // Increment indices in row-major order
            if (mShape.empty())
            {
                return *this;
            }

            for (size_t i = mShape.size(); i > 0; --i)
            {
                size_t idx = i - 1;
                ++mIndices[idx];
                if (mIndices[idx] < mShape[idx])
                {
                    compute_ptr();
                    return *this;
                }
                mIndices[idx] = 0;
            }
            // Reached end
            if (!mShape.empty())
            {
                mIndices[0] = mShape[0];
            }
            compute_ptr();
            return *this;
        }

        MultiDimIterator operator++(int)
        {
            MultiDimIterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const MultiDimIterator& other) const
        {
            return mIndices == other.mIndices;
        }

        bool operator!=(const MultiDimIterator& other) const
        {
            return !(*this == other);
        }

    private:
        void compute_ptr()
        {
            mPtr = mBase;
            for (size_t i = 0; i < mIndices.size(); ++i)
            {
                mPtr += static_cast<ptrdiff_t>(mIndices[i]) * mStrides[i];
            }
        }

        T* mBase;
        T* mPtr;
        std::vector<size_t> mShape;
        std::vector<ptrdiff_t> mStrides;
        std::vector<size_t> mIndices;
    };

    template <typename T>
    using iterator_type = MultiDimIterator<T>;

    template <typename T>
    using const_iterator_type = MultiDimIterator<const T>;

    template <typename T>
    static iterator_type<T> make_begin(T* data, const std::vector<size_t>& shape, const std::vector<ptrdiff_t>& strides)
    {
        return iterator_type<T>(data, shape, strides);
    }

    template <typename T>
    static iterator_type<T>
    make_end(T* data, const std::vector<size_t>& shape, const std::vector<ptrdiff_t>& strides, size_t)
    {
        return iterator_type<T>(data, shape, strides, true);
    }

    static constexpr const char* name()
    {
        return "Strided";
    }
};

/**
 * @brief Blocked iterator policy (cache-friendly)
 * Iterates in cache-friendly blocks
 */
template <size_t BlockSize = 64>
struct BlockedPolicy
{
    template <typename T>
    class BlockIterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = ptrdiff_t;
        using pointer = T*;
        using reference = T&;

        BlockIterator(T* ptr,
                      const std::vector<size_t>& shape,
                      size_t block_row,
                      size_t block_col,
                      size_t in_block_row,
                      size_t in_block_col)
            : mPtr(ptr)
            , mShape(shape)
            , mBlockRow(block_row)
            , mBlockCol(block_col)
            , mInBlockRow(in_block_row)
            , mInBlockCol(in_block_col)
        {
            if (mShape.size() < 2)
            {
                mShape = {1, mShape.empty() ? 0 : mShape[0]};
            }
        }

        reference operator*() const
        {
            size_t global_row = std::min(mBlockRow * BlockSize + mInBlockRow, mShape[0] - 1);
            size_t global_col = std::min(mBlockCol * BlockSize + mInBlockCol, mShape[1] - 1);
            return mPtr[global_row * mShape[1] + global_col];
        }

        pointer operator->() const
        {
            size_t global_row = std::min(mBlockRow * BlockSize + mInBlockRow, mShape[0] - 1);
            size_t global_col = std::min(mBlockCol * BlockSize + mInBlockCol, mShape[1] - 1);
            return &mPtr[global_row * mShape[1] + global_col];
        }

        BlockIterator& operator++()
        {
            ++mInBlockCol;
            if (mInBlockCol >= BlockSize || mBlockCol * BlockSize + mInBlockCol >= mShape[1])
            {
                mInBlockCol = 0;
                ++mInBlockRow;
                if (mInBlockRow >= BlockSize || mBlockRow * BlockSize + mInBlockRow >= mShape[0])
                {
                    mInBlockRow = 0;
                    ++mBlockCol;
                    if (mBlockCol * BlockSize >= mShape[1])
                    {
                        mBlockCol = 0;
                        ++mBlockRow;
                    }
                }
            }
            return *this;
        }

        BlockIterator operator++(int)
        {
            BlockIterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const BlockIterator& other) const
        {
            return mBlockRow == other.mBlockRow && mBlockCol == other.mBlockCol &&
                   mInBlockRow == other.mInBlockRow && mInBlockCol == other.mInBlockCol;
        }

        bool operator!=(const BlockIterator& other) const
        {
            return !(*this == other);
        }

    private:
        T* mPtr;
        std::vector<size_t> mShape;
        size_t mBlockRow, mBlockCol;
        size_t mInBlockRow, mInBlockCol;
    };

    template <typename T>
    using iterator_type = BlockIterator<T>;

    template <typename T>
    using const_iterator_type = BlockIterator<const T>;

    template <typename T>
    static iterator_type<T> make_begin(T* data, const std::vector<size_t>& shape, const std::vector<ptrdiff_t>&)
    {
        return iterator_type<T>(data, shape, 0, 0, 0, 0);
    }

    template <typename T>
    static iterator_type<T> make_end(T* data, const std::vector<size_t>& shape, const std::vector<ptrdiff_t>&, size_t)
    {
        size_t rows = shape.empty() ? 1 : shape[0];
        size_t num_block_rows = (rows + BlockSize - 1) / BlockSize;
        return iterator_type<T>(data, shape, num_block_rows, 0, 0, 0);
    }

    static constexpr const char* name()
    {
        return "Blocked";
    }
};

// =============================================================================
// Expression Templates (Lazy Evaluation Infrastructure)
// =============================================================================

// Forward declarations for expression templates
template <typename L, typename R>
struct LazyAdd;
template <typename L, typename R>
struct LazySubtract;
template <typename L, typename R>
struct LazyMultiply;
template <typename L>
struct LazyScalarMultiply;

/**
 * @brief Expression template tag for type identification
 */
template <typename E>
struct is_tensor_expression : std::false_type
{
};

template <typename T, typename Allocator, typename IteratorPolicy, typename ConcurrencyPolicy>
struct is_tensor_expression<Tensor<T, Allocator, IteratorPolicy, ConcurrencyPolicy>> : std::true_type
{
};

/**
 * @brief Lazy addition expression
 */
template <typename L, typename R>
struct LazyAdd
{
    L lhs; // Store by value for expression temps, by reference for Tensors via std::reference_wrapper
    R rhs;

    using value_type = typename std::remove_reference_t<L>::value_type;

    LazyAdd(const L& l, const R& r)
        : lhs(l)
        , rhs(r)
    {
    }

    value_type operator[](size_t i) const
    {
        return lhs[i] + rhs[i];
    }

    size_t size() const
    {
        return lhs.size();
    }
    const std::vector<size_t>& shape() const
    {
        return lhs.shape();
    }

    // Chain support - allow chaining expressions
    template <typename Other>
    LazyAdd<LazyAdd, Other> lazy_add(const Other& other) const
    {
        return LazyAdd<LazyAdd, Other>(*this, other);
    }

    template <typename Other>
    LazySubtract<LazyAdd, Other> lazy_sub(const Other& other) const
    {
        return LazySubtract<LazyAdd, Other>(*this, other);
    }

    template <typename Other>
    LazyMultiply<LazyAdd, Other> lazy_mul(const Other& other) const
    {
        return LazyMultiply<LazyAdd, Other>(*this, other);
    }
};

template <typename L, typename R>
struct is_tensor_expression<LazyAdd<L, R>> : std::true_type
{
};

/**
 * @brief Lazy subtraction expression
 */
template <typename L, typename R>
struct LazySubtract
{
    L lhs;
    R rhs;

    using value_type = typename std::remove_reference_t<L>::value_type;

    LazySubtract(const L& l, const R& r)
        : lhs(l)
        , rhs(r)
    {
    }

    value_type operator[](size_t i) const
    {
        return lhs[i] - rhs[i];
    }

    size_t size() const
    {
        return lhs.size();
    }
    const std::vector<size_t>& shape() const
    {
        return lhs.shape();
    }

    // Chain support
    template <typename Other>
    LazyAdd<LazySubtract, Other> lazy_add(const Other& other) const
    {
        return LazyAdd<LazySubtract, Other>(*this, other);
    }

    template <typename Other>
    LazySubtract<LazySubtract, Other> lazy_sub(const Other& other) const
    {
        return LazySubtract<LazySubtract, Other>(*this, other);
    }

    template <typename Other>
    LazyMultiply<LazySubtract, Other> lazy_mul(const Other& other) const
    {
        return LazyMultiply<LazySubtract, Other>(*this, other);
    }
};

template <typename L, typename R>
struct is_tensor_expression<LazySubtract<L, R>> : std::true_type
{
};

/**
 * @brief Lazy multiplication expression
 */
template <typename L, typename R>
struct LazyMultiply
{
    L lhs;
    R rhs;

    using value_type = typename std::remove_reference_t<L>::value_type;

    LazyMultiply(const L& l, const R& r)
        : lhs(l)
        , rhs(r)
    {
    }

    value_type operator[](size_t i) const
    {
        return lhs[i] * rhs[i];
    }

    size_t size() const
    {
        return lhs.size();
    }
    const std::vector<size_t>& shape() const
    {
        return lhs.shape();
    }

    // Chain support
    template <typename Other>
    LazyAdd<LazyMultiply, Other> lazy_add(const Other& other) const
    {
        return LazyAdd<LazyMultiply, Other>(*this, other);
    }

    template <typename Other>
    LazySubtract<LazyMultiply, Other> lazy_sub(const Other& other) const
    {
        return LazySubtract<LazyMultiply, Other>(*this, other);
    }

    template <typename Other>
    LazyMultiply<LazyMultiply, Other> lazy_mul(const Other& other) const
    {
        return LazyMultiply<LazyMultiply, Other>(*this, other);
    }
};

template <typename L, typename R>
struct is_tensor_expression<LazyMultiply<L, R>> : std::true_type
{
};

/**
 * @brief Lazy scalar multiplication expression
 */
template <typename L>
struct LazyScalarMultiply
{
    L lhs;
    typename std::remove_reference_t<L>::value_type scalar;

    using value_type = typename std::remove_reference_t<L>::value_type;

    LazyScalarMultiply(const L& l, value_type s)
        : lhs(l)
        , scalar(s)
    {
    }

    value_type operator[](size_t i) const
    {
        return lhs[i] * scalar;
    }

    size_t size() const
    {
        return lhs.size();
    }
    const std::vector<size_t>& shape() const
    {
        return lhs.shape();
    }
};

template <typename L>
struct is_tensor_expression<LazyScalarMultiply<L>> : std::true_type
{
};

// =============================================================================
// Tensor Class
// =============================================================================

/**
 * @brief N-dimensional tensor with policy-based iterators and optimized operations
 * @tparam T Element type (default: double)
 * @tparam Allocator Memory allocator (default: TensorAllocator with 64-byte alignment)
 * @tparam IteratorPolicy Iterator traversal policy (default: RowMajorPolicy)
 */
template <typename T = double,
          typename Allocator = TensorAllocator<T>,
          typename IteratorPolicy = RowMajorPolicy,
          typename ConcurrencyPolicy = SingleThreadedPolicy>
class Tensor
{
public:
    using value_type = T;
    using allocator_type = Allocator;
    using iterator_policy = IteratorPolicy;
    using concurrency_policy = ConcurrencyPolicy;
    using iterator = typename IteratorPolicy::template iterator_type<T>;
    using const_iterator = typename IteratorPolicy::template const_iterator_type<const T>;
    using reference = T&;
    using const_reference = const T&;

    // Friend declarations for serialization (allow private member access)
    template <typename U, typename A, typename I>
    friend void to_json(JsonValue& j, const Tensor<U, A, I>& tensor);

    template <typename U, typename A, typename I>
    friend void from_json(const JsonValue& j, Tensor<U, A, I>& tensor);

    template <typename U, typename Policy>
    friend struct EqualDispatcher;

    // =========================================================================
    // Constructors
    // =========================================================================

    /**
     * @brief Default constructor (empty tensor)
     */
    Tensor()
        : mData(nullptr)
        , mSize(0)
    {
    }

    /**
     * @brief Construct tensor with given shape
     */
    explicit Tensor(std::vector<size_t> shape)
        : mShape(std::move(shape))
        , mSize(compute_size(mShape))
    {
        if (mSize > 0)
        {
            T* raw = mAllocator.allocate(mSize);
            shared_data_ = std::shared_ptr<T[]>(raw, [this](T* p) {
                mAllocator.deallocate(p, mSize);
            });
            mData = shared_data_.get();
            std::fill(mData, mData + mSize, T{});
        }
        mStrides = compute_strides(mShape);
    }

    /**
     * @brief Construct tensor with shape and initial value
     */
    Tensor(std::vector<size_t> shape, const T& value)
        : Tensor(std::move(shape))
    {
        fill(value);
    }

    /**
     * @brief Copy constructor (deep copy)
     */
    Tensor(const Tensor& other)
        : mShape(other.mShape)
        , mStrides(other.mStrides)
        , mSize(other.mSize)
    {
        if (mSize > 0)
        {
            T* raw = mAllocator.allocate(mSize);
            shared_data_ = std::shared_ptr<T[]>(raw, [this](T* p) {
                mAllocator.deallocate(p, mSize);
            });
            mData = shared_data_.get();
            std::copy(other.mData, other.mData + mSize, mData);
        }
    }

    /**
     * @brief Move constructor
     */
    Tensor(Tensor&& other) noexcept
        : shared_data_(std::move(other.shared_data_))
        , mData(other.mData)
        , mShape(std::move(other.mShape))
        , mStrides(std::move(other.mStrides))
        , mSize(other.mSize)
    {
        other.mData = nullptr;
        other.mSize = 0;
    }

    /**
     * @brief Copy assignment (deep copy)
     */
    Tensor& operator=(const Tensor& other)
    {
        if (this != &other)
        {
            Tensor tmp(other);
            swap(tmp);
        }
        return *this;
    }

    /**
     * @brief Move assignment
     */
    Tensor& operator=(Tensor&& other) noexcept
    {
        if (this != &other)
        {
            Tensor tmp(std::move(other));
            swap(tmp);
        }
        return *this;
    }

    // =========================================================================
    // Element Access
    // =========================================================================

    /**
     * @brief Multi-dimensional indexing (variadic)
     */
    template <typename... Indices>
    reference operator()(Indices... indices)
    {
        return mData[compute_offset(static_cast<size_t>(indices)...)];
    }

    template <typename... Indices>
    const_reference operator()(Indices... indices) const
    {
        return mData[compute_offset(static_cast<size_t>(indices)...)];
    }

    /**
     * @brief Bounds-checked access with enhanced error messages
     * @details In debug builds, provides detailed error messages showing index and valid range.
     *          In release builds (-DNDEBUG), provides basic bounds checking.
     */
    template <typename... Indices>
    reference at(Indices... indices)
    {
        static_assert(sizeof...(indices) > 0, "at() requires at least one index");

// Validate index count matches tensor dimensions
#ifndef NDEBUG
        if (sizeof...(indices) != mShape.size())
        {
            std::ostringstream oss;
            oss << "Tensor::at index count mismatch: tensor has " << mShape.size() << " dimensions, but got "
                << sizeof...(indices) << " indices";
            throw std::out_of_range(oss.str());
        }
#endif

        // Check each dimension's bounds
        std::vector<size_t> idx_vec = {static_cast<size_t>(indices)...};
#ifndef NDEBUG
        // Debug build: enhanced error messages with dimension info
        debug_bounds_check_nd(idx_vec, mShape, "Tensor::at");
#else
        // Release build: efficient bounds check that works for all layouts
        for (size_t i = 0; i < idx_vec.size(); ++i)
        {
            if (idx_vec[i] >= mShape[i])
            {
                throw std::out_of_range("Tensor index out of bounds");
            }
        }
#endif

        size_t offset = compute_offset(static_cast<size_t>(indices)...);
        return mData[offset];
    }

    template <typename... Indices>
    const_reference at(Indices... indices) const
    {
        static_assert(sizeof...(indices) > 0, "at() requires at least one index");

// Validate index count matches tensor dimensions
#ifndef NDEBUG
        if (sizeof...(indices) != mShape.size())
        {
            std::ostringstream oss;
            oss << "Tensor::at index count mismatch: tensor has " << mShape.size() << " dimensions, but got "
                << sizeof...(indices) << " indices";
            throw std::out_of_range(oss.str());
        }
#endif

        // Check each dimension's bounds
        std::vector<size_t> idx_vec = {static_cast<size_t>(indices)...};
#ifndef NDEBUG
        // Debug build: enhanced error messages with dimension info
        debug_bounds_check_nd(idx_vec, mShape, "Tensor::at");
#else
        // Release build: efficient bounds check that works for all layouts
        for (size_t i = 0; i < idx_vec.size(); ++i)
        {
            if (idx_vec[i] >= mShape[i])
            {
                throw std::out_of_range("Tensor index out of bounds");
            }
        }
#endif

        size_t offset = compute_offset(static_cast<size_t>(indices)...);
        return mData[offset];
    }

    /**
     * @brief Linear bounds-checked access (always checks, even in release)
     */
    reference at_linear(size_t index)
    {
        if (index >= mSize)
        {
#ifndef NDEBUG
            std::ostringstream oss;
            oss << "Tensor::at_linear index " << index << " out of range [0, " << mSize << ")";
            throw std::out_of_range(oss.str());
#else
            throw std::out_of_range("Tensor::at_linear index out of bounds");
#endif
        }
        return mData[index];
    }

    const_reference at_linear(size_t index) const
    {
        if (index >= mSize)
        {
#ifndef NDEBUG
            std::ostringstream oss;
            oss << "Tensor::at_linear index " << index << " out of range [0, " << mSize << ")";
            throw std::out_of_range(oss.str());
#else
            throw std::out_of_range("Tensor::at_linear index out of bounds");
#endif
        }
        return mData[index];
    }

    /**
     * @brief Linear indexing
     */
    reference operator[](size_t index)
    {
        return mData[index];
    }
    const_reference operator[](size_t index) const
    {
        return mData[index];
    }

    // =========================================================================
    // Iterators
    // =========================================================================

    iterator begin()
    {
        return IteratorPolicy::template make_begin<T>(mData, mShape, mStrides);
    }

    iterator end()
    {
        return IteratorPolicy::template make_end<T>(mData, mShape, mStrides, mSize);
    }

    const_iterator begin() const
    {
        return IteratorPolicy::template make_begin<const T>(mData, mShape, mStrides);
    }

    const_iterator end() const
    {
        return IteratorPolicy::template make_end<const T>(mData, mShape, mStrides, mSize);
    }

    const_iterator cbegin() const
    {
        return begin();
    }
    const_iterator cend() const
    {
        return end();
    }

    // =========================================================================
    // Shape and Metadata
    // =========================================================================

    const std::vector<size_t>& shape() const
    {
        return mShape;
    }
    const std::vector<ptrdiff_t>& strides() const
    {
        return mStrides;
    }
    size_t size() const
    {
        return mSize;
    }
    size_t ndim() const
    {
        return mShape.size();
    }
    size_t dim(size_t axis) const
    {
        return mShape.at(axis);
    }
    bool empty() const
    {
        return mSize == 0;
    }

    T* data()
    {
        return mData;
    }
    const T* data() const
    {
        return mData;
    }

    /**
     * @brief Get type name for serialization
     */
    static std::string type_name()
    {
        if constexpr (std::is_same_v<T, float>)
        {
            return "float";
        }
        else if constexpr (std::is_same_v<T, double>)
        {
            return "double";
        }
        else if constexpr (std::is_same_v<T, int32_t>)
        {
            return "int32";
        }
        else if constexpr (std::is_same_v<T, int64_t>)
        {
            return "int64";
        }
        else if constexpr (std::is_same_v<T, uint32_t>)
        {
            return "uint32";
        }
        else if constexpr (std::is_same_v<T, uint64_t>)
        {
            return "uint64";
        }
        else
        {
            return typeid(T).name(); // Fallback
        }
    }

    // =========================================================================
    // Views and Slicing
    // =========================================================================

    /**
     * @brief Create view of tensor slice
     */
    Tensor view(std::vector<size_t> start, std::vector<size_t> end) const
    {
        FATP_ENFORCE(start.size() == mShape.size() && end.size() == mShape.size(),
                     "View dimensions must match tensor dimensions (expected ",
                     mShape.size(),
                     ", got start:",
                     start.size(),
                     ", end:",
                     end.size(),
                     ")");

        for (size_t i = 0; i < start.size(); ++i)
        {
            FATP_ENFORCE(start[i] < end[i] && end[i] <= mShape[i],
                         "Invalid view range at dimension ",
                         i,
                         " (start:",
                         start[i],
                         ", end:",
                         end[i],
                         ", max:",
                         mShape[i],
                         ")");
        }

        std::vector<size_t> new_shape(mShape.size());
        for (size_t i = 0; i < mShape.size(); ++i)
        {
            new_shape[i] = end[i] - start[i];
        }

        size_t offset = compute_offset_from_vector(start);
        return Tensor(shared_data_, mData + offset, new_shape, mStrides);
    }

    /**
     * @brief Get row view (2D tensors)
     */
    Tensor row(size_t index) const
    {
        FATP_ENFORCE(mShape.size() == 2, "row() requires 2D tensor, got ", mShape.size(), "D");
        FATP_ENFORCE(index < mShape[0], "Row index out of bounds: ", index, " >= ", mShape[0]);
        return view({index, 0}, {index + 1, mShape[1]});
    }

    /**
     * @brief Get column view (2D tensors)
     */
    Tensor col(size_t index) const
    {
        FATP_ENFORCE(mShape.size() == 2, "col() requires 2D tensor, got ", mShape.size(), "D");
        FATP_ENFORCE(index < mShape[1], "Column index out of bounds: ", index, " >= ", mShape[1]);
        return view({0, index}, {mShape[0], index + 1});
    }

    /**
     * @brief Transpose (2D tensors)
     */
    Tensor transpose() const
    {
        FATP_ENFORCE(mShape.size() == 2, "transpose() requires 2D tensor, got ", mShape.size(), "D");

        std::vector<size_t> new_shape = {mShape[1], mShape[0]};
        std::vector<ptrdiff_t> new_strides = {mStrides[1], mStrides[0]};

        return Tensor(shared_data_, mData, new_shape, new_strides);
    }

    /**
     * @brief Reshape tensor (must maintain size)
     */
    Tensor reshape(std::vector<size_t> new_shape) const
    {
        [[maybe_unused]] size_t new_size = compute_size(new_shape);
        FATP_ENFORCE(new_size == mSize, "Reshape size mismatch: current=", mSize, ", requested=", new_size);

        std::vector<ptrdiff_t> new_strides = compute_strides(new_shape);
        return Tensor(shared_data_, mData, new_shape, new_strides);
    }

    // =========================================================================
    // Lifetime Tracking Support (v5.1)
    // =========================================================================

    /**
     * @brief Create a lifetime tracker for this tensor
     * @details In debug builds, tracks tensor lifetime to detect dangling views.
     *          In release builds, returns a lightweight wrapper with zero overhead.
     *
     * Usage:
     *   auto tracker = tensor.create_tracker();
     *   auto view = tracker.create_view();
     *   view.check_valid();  // Throws in debug if tensor destroyed
     */
    LifetimeTracker<Tensor> create_tracker(const char* name = "Tensor")
    {
        return LifetimeTracker<Tensor>(*this, name);
    }

    /**
     * @brief Create a tracked view (const version)
     */
    LifetimeTracker<const Tensor> create_tracker(const char* name = "Tensor") const
    {
        return LifetimeTracker<const Tensor>(*this, name);
    }

/**
 * @brief Create a tracked view with slice
 * @details Combines view() with lifetime tracking
 *
 * Usage:
 *   auto tracked_slice = tensor.create_tracked_slice({0, 0}, {10, 10});
 *   // In debug: tracked_slice.check_valid() validates tensor still exists
 */
#ifndef NDEBUG
    auto
    create_tracked_slice(std::vector<size_t> start, std::vector<size_t> end, const char* name = "TensorSlice") const
    {
        // Validate bounds with enhanced checking
        for (size_t i = 0; i < start.size(); ++i)
        {
            debug_validate_slice(start[i], end[i], size_t{1}, mShape[i], "Tensor::create_tracked_slice");
        }

        Tensor slice_view = view(start, end);
        return slice_view.create_tracker(name);
    }
#else
    auto create_tracked_slice(std::vector<size_t> start, std::vector<size_t> end, const char* = nullptr) const
    {
        return view(start, end);
    }
#endif

/**
 * @brief Create tracked row view (2D tensors)
 */
#ifndef NDEBUG
    auto create_tracked_row(size_t index, const char* name = "TensorRow") const
    {
        debug_bounds_check(index, size_t{0}, mShape[0], "Tensor::create_tracked_row");
        Tensor row_view = row(index);
        return row_view.create_tracker(name);
    }
#else
    auto create_tracked_row(size_t index, const char* = nullptr) const
    {
        return row(index);
    }
#endif

/**
 * @brief Create tracked column view (2D tensors)
 */
#ifndef NDEBUG
    auto create_tracked_col(size_t index, const char* name = "TensorColumn") const
    {
        debug_bounds_check(index, size_t{0}, mShape[1], "Tensor::create_tracked_col");
        Tensor col_view = col(index);
        return col_view.create_tracker(name);
    }
#else
    auto create_tracked_col(size_t index, const char* = nullptr) const
    {
        return col(index);
    }
#endif

    // =========================================================================
    // AVX-512 Optimized Operations (v5.0)
    // =========================================================================

#ifdef __AVX512F__
    /**
     * @brief AVX-512 element-wise addition (16 floats or 8 doubles per instruction)
     */
    template <typename U = T>
        requires std::is_same_v<U, float>
    void add_avx512(const T* a, const T* b, T* result, size_t count) const
    {
        size_t i = 0;
        const size_t vec_size = 16; // 512 bits / 32 bits per float

        // Process 16 floats at a time
        for (; i + vec_size <= count; i += vec_size)
        {
            __m512 va = _mm512_loadu_ps(a + i);
            __m512 vb = _mm512_loadu_ps(b + i);
            __m512 vr = _mm512_add_ps(va, vb);
            _mm512_storeu_ps(result + i, vr);
        }

        // Handle remainder with mask
        if (i < count)
        {
            __mmask16 mask = (__mmask16)((1ULL << (count - i)) - 1);
            __m512 va = _mm512_maskz_loadu_ps(mask, a + i);
            __m512 vb = _mm512_maskz_loadu_ps(mask, b + i);
            __m512 vr = _mm512_add_ps(va, vb);
            _mm512_mask_storeu_ps(result + i, mask, vr);
        }
    }

    template <typename U = T>
        requires std::is_same_v<U, double>
    void add_avx512(const T* a, const T* b, T* result, size_t count) const
    {
        size_t i = 0;
        const size_t vec_size = 8; // 512 bits / 64 bits per double

        // Process 8 doubles at a time
        for (; i + vec_size <= count; i += vec_size)
        {
            __m512d va = _mm512_loadu_pd(a + i);
            __m512d vb = _mm512_loadu_pd(b + i);
            __m512d vr = _mm512_add_pd(va, vb);
            _mm512_storeu_pd(result + i, vr);
        }

        // Handle remainder with mask
        if (i < count)
        {
            __mmask8 mask = (__mmask8)((1ULL << (count - i)) - 1);
            __m512d va = _mm512_maskz_loadu_pd(mask, a + i);
            __m512d vb = _mm512_maskz_loadu_pd(mask, b + i);
            __m512d vr = _mm512_add_pd(va, vb);
            _mm512_mask_storeu_pd(result + i, mask, vr);
        }
    }

    /**
     * @brief AVX-512 element-wise multiplication with FMA
     */
    template <typename U = T>
        requires std::is_same_v<U, float>
    void mul_avx512(const T* a, const T* b, T* result, size_t count) const
    {
        size_t i = 0;
        const size_t vec_size = 16;

        for (; i + vec_size <= count; i += vec_size)
        {
            __m512 va = _mm512_loadu_ps(a + i);
            __m512 vb = _mm512_loadu_ps(b + i);
            __m512 vr = _mm512_mul_ps(va, vb);
            _mm512_storeu_ps(result + i, vr);
        }

        if (i < count)
        {
            __mmask16 mask = (__mmask16)((1ULL << (count - i)) - 1);
            __m512 va = _mm512_maskz_loadu_ps(mask, a + i);
            __m512 vb = _mm512_maskz_loadu_ps(mask, b + i);
            __m512 vr = _mm512_mul_ps(va, vb);
            _mm512_mask_storeu_ps(result + i, mask, vr);
        }
    }

    template <typename U = T>
        requires std::is_same_v<U, double>
    void mul_avx512(const T* a, const T* b, T* result, size_t count) const
    {
        size_t i = 0;
        const size_t vec_size = 8;

        for (; i + vec_size <= count; i += vec_size)
        {
            __m512d va = _mm512_loadu_pd(a + i);
            __m512d vb = _mm512_loadu_pd(b + i);
            __m512d vr = _mm512_mul_pd(va, vb);
            _mm512_storeu_pd(result + i, vr);
        }

        if (i < count)
        {
            __mmask8 mask = (__mmask8)((1ULL << (count - i)) - 1);
            __m512d va = _mm512_maskz_loadu_pd(mask, a + i);
            __m512d vb = _mm512_maskz_loadu_pd(mask, b + i);
            __m512d vr = _mm512_mul_pd(va, vb);
            _mm512_mask_storeu_pd(result + i, mask, vr);
        }
    }
#endif // __AVX512F__

    // =========================================================================
    // Vectorized Broadcasting (v5.0)
    // =========================================================================

    /**
     * @brief Detect if tensor can be broadcast (strides indicate broadcast pattern)
     */
    bool is_broadcast_compatible(const Tensor& other) const
    {
        if (mShape.size() != other.mShape.size())
        {
            return false;
        }

        for (size_t i = 0; i < mShape.size(); ++i)
        {
            // Dimension must be 1 (broadcast) or match
            if (mShape[i] != 1 && other.mShape[i] != 1 && mShape[i] != other.mShape[i])
            {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief SIMD-optimized scalar broadcast addition
     */
    Tensor broadcast_add_scalar(const T& scalar) const
    {
        Tensor result(mShape);

#ifdef __AVX512F__
        if constexpr (std::is_same_v<T, float>)
        {
            __m512 vs = _mm512_set1_ps(scalar);
            size_t i = 0;
            const size_t vec_size = 16;

            for (; i + vec_size <= mSize; i += vec_size)
            {
                __m512 va = _mm512_loadu_ps(mData + i);
                __m512 vr = _mm512_add_ps(va, vs);
                _mm512_storeu_ps(result.mData + i, vr);
            }

            if (i < mSize)
            {
                __mmask16 mask = (__mmask16)((1ULL << (mSize - i)) - 1);
                __m512 va = _mm512_maskz_loadu_ps(mask, mData + i);
                __m512 vr = _mm512_add_ps(va, vs);
                _mm512_mask_storeu_ps(result.mData + i, mask, vr);
            }
        }
        else if constexpr (std::is_same_v<T, double>)
        {
            __m512d vs = _mm512_set1_pd(scalar);
            size_t i = 0;
            const size_t vec_size = 8;

            for (; i + vec_size <= mSize; i += vec_size)
            {
                __m512d va = _mm512_loadu_pd(mData + i);
                __m512d vr = _mm512_add_pd(va, vs);
                _mm512_storeu_pd(result.mData + i, vr);
            }

            if (i < mSize)
            {
                __mmask8 mask = (__mmask8)((1ULL << (mSize - i)) - 1);
                __m512d va = _mm512_maskz_loadu_pd(mask, mData + i);
                __m512d vr = _mm512_add_pd(va, vs);
                _mm512_mask_storeu_pd(result.mData + i, mask, vr);
            }
        }
        else
#elif defined(__AVX2__)
        if constexpr (std::is_same_v<T, float>)
        {
            __m256 vs = _mm256_set1_ps(scalar);
            size_t i = 0;
            const size_t vec_size = 8;

            for (; i + vec_size <= mSize; i += vec_size)
            {
                __m256 va = _mm256_loadu_ps(mData + i);
                __m256 vr = _mm256_add_ps(va, vs);
                _mm256_storeu_ps(result.mData + i, vr);
            }

            for (; i < mSize; ++i)
            {
                result.mData[i] = mData[i] + scalar;
            }
        }
        else if constexpr (std::is_same_v<T, double>)
        {
            __m256d vs = _mm256_set1_pd(scalar);
            size_t i = 0;
            const size_t vec_size = 4;

            for (; i + vec_size <= mSize; i += vec_size)
            {
                __m256d va = _mm256_loadu_pd(mData + i);
                __m256d vr = _mm256_add_pd(va, vs);
                _mm256_storeu_pd(result.mData + i, vr);
            }

            for (; i < mSize; ++i)
            {
                result.mData[i] = mData[i] + scalar;
            }
        }
        else
#endif
        {
            // Fallback: scalar code
            for (size_t i = 0; i < mSize; ++i)
            {
                result.mData[i] = mData[i] + scalar;
            }
        }

        return result;
    }

    /**
     * @brief SIMD-optimized vector broadcast (add vector to each row/column)
     */
    Tensor broadcast_add_vector(const Tensor& vec) const
    {
        // Only handle 2D + 1D broadcast for now
        if (mShape.size() != 2 || vec.mShape.size() != 1)
        {
            // Fall back to generic broadcast
            return *this + vec;
        }

        Tensor result(mShape);
        size_t rows = mShape[0];
        size_t cols = mShape[1];

        if (vec.size() == cols)
        {
            // Broadcast vector to each row
#ifdef __AVX512F__
            if constexpr (std::is_same_v<T, float> && (cols % 16 == 0))
            {
                for (size_t i = 0; i < rows; ++i)
                {
                    for (size_t j = 0; j < cols; j += 16)
                    {
                        __m512 va = _mm512_loadu_ps(&(*this)(i, j));
                        __m512 vb = _mm512_loadu_ps(&vec[j]);
                        __m512 vr = _mm512_add_ps(va, vb);
                        _mm512_storeu_ps(&result(i, j), vr);
                    }
                }
            }
            else
#endif
            {
                for (size_t i = 0; i < rows; ++i)
                {
                    for (size_t j = 0; j < cols; ++j)
                    {
                        result(i, j) = (*this)(i, j) + vec[j];
                    }
                }
            }
        }
        else if (vec.size() == rows)
        {
            // Broadcast vector to each column
            for (size_t i = 0; i < rows; ++i)
            {
                T broadcast_val = vec[i];
#ifdef __AVX512F__
                if constexpr (std::is_same_v<T, float>)
                {
                    __m512 vb = _mm512_set1_ps(broadcast_val);
                    size_t j = 0;
                    for (; j + 16 <= cols; j += 16)
                    {
                        __m512 va = _mm512_loadu_ps(&(*this)(i, j));
                        __m512 vr = _mm512_add_ps(va, vb);
                        _mm512_storeu_ps(&result(i, j), vr);
                    }
                    for (; j < cols; ++j)
                    {
                        result(i, j) = (*this)(i, j) + broadcast_val;
                    }
                }
                else
#endif
                {
                    for (size_t j = 0; j < cols; ++j)
                    {
                        result(i, j) = (*this)(i, j) + broadcast_val;
                    }
                }
            }
        }
        else
        {
            throw std::invalid_argument("Vector size must match rows or columns for broadcast");
        }

        return result;
    }

    // =========================================================================
    // Operations (SIMD-Optimized)
    // =========================================================================

    /**
     * @brief Fill tensor with value
     */
    void fill(const T& value)
    {
        std::fill(mData, mData + mSize, value);
    }

private:
    /**
     * @brief Check shape compatibility for binary operations
     * @throws DomainContractError if shapes don't match
     */
    void check_shape_compatibility(const Tensor& other, const char* op) const
    {
        if (mShape != other.mShape)
        {
            throw DomainContractError(std::string("Shape mismatch in ") + op + ": " +
                                      "lhs=" + std::to_string(mShape.size()) + "D, " +
                                      "rhs=" + std::to_string(other.mShape.size()) + "D");
        }
    }

    /**
     * @brief SIMD-optimized element-wise addition for contiguous data
     */
    void simd_add(const T* a, const T* b, T* result, size_t n) const
    {
#ifdef __AVX2__
        if constexpr (std::is_same_v<T, float>)
        {
            size_t i = 0;
            const size_t simd_width = 8; // AVX processes 8 floats at once

            for (; i + simd_width <= n; i += simd_width)
            {
                __m256 va = _mm256_loadu_ps(a + i);
                __m256 vb = _mm256_loadu_ps(b + i);
                __m256 vr = _mm256_add_ps(va, vb);
                _mm256_storeu_ps(result + i, vr);
            }

            // Handle remainder
            for (; i < n; ++i)
            {
                result[i] = a[i] + b[i];
            }
        }
        else if constexpr (std::is_same_v<T, double>)
        {
            size_t i = 0;
            const size_t simd_width = 4; // AVX processes 4 doubles at once

            for (; i + simd_width <= n; i += simd_width)
            {
                __m256d va = _mm256_loadu_pd(a + i);
                __m256d vb = _mm256_loadu_pd(b + i);
                __m256d vr = _mm256_add_pd(va, vb);
                _mm256_storeu_pd(result + i, vr);
            }

            for (; i < n; ++i)
            {
                result[i] = a[i] + b[i];
            }
        }
        else
        {
            // Fallback for non-SIMD types
            for (size_t i = 0; i < n; ++i)
            {
                result[i] = a[i] + b[i];
            }
        }
#else
        // Non-SIMD fallback
        for (size_t i = 0; i < n; ++i)
        {
            result[i] = a[i] + b[i];
        }
#endif
    }

    /**
     * @brief SIMD-optimized element-wise subtraction
     */
    void simd_sub(const T* a, const T* b, T* result, size_t n) const
    {
#ifdef __AVX2__
        if constexpr (std::is_same_v<T, float>)
        {
            size_t i = 0;
            const size_t simd_width = 8;

            for (; i + simd_width <= n; i += simd_width)
            {
                __m256 va = _mm256_loadu_ps(a + i);
                __m256 vb = _mm256_loadu_ps(b + i);
                __m256 vr = _mm256_sub_ps(va, vb);
                _mm256_storeu_ps(result + i, vr);
            }

            for (; i < n; ++i)
            {
                result[i] = a[i] - b[i];
            }
        }
        else if constexpr (std::is_same_v<T, double>)
        {
            size_t i = 0;
            const size_t simd_width = 4;

            for (; i + simd_width <= n; i += simd_width)
            {
                __m256d va = _mm256_loadu_pd(a + i);
                __m256d vb = _mm256_loadu_pd(b + i);
                __m256d vr = _mm256_sub_pd(va, vb);
                _mm256_storeu_pd(result + i, vr);
            }

            for (; i < n; ++i)
            {
                result[i] = a[i] - b[i];
            }
        }
        else
        {
            for (size_t i = 0; i < n; ++i)
            {
                result[i] = a[i] - b[i];
            }
        }
#else
        for (size_t i = 0; i < n; ++i)
        {
            result[i] = a[i] - b[i];
        }
#endif
    }

    /**
     * @brief SIMD-optimized element-wise multiplication
     */
    void simd_mul(const T* a, const T* b, T* result, size_t n) const
    {
#ifdef __AVX2__
        if constexpr (std::is_same_v<T, float>)
        {
            size_t i = 0;
            const size_t simd_width = 8;

            for (; i + simd_width <= n; i += simd_width)
            {
                __m256 va = _mm256_loadu_ps(a + i);
                __m256 vb = _mm256_loadu_ps(b + i);
                __m256 vr = _mm256_mul_ps(va, vb);
                _mm256_storeu_ps(result + i, vr);
            }

            for (; i < n; ++i)
            {
                result[i] = a[i] * b[i];
            }
        }
        else if constexpr (std::is_same_v<T, double>)
        {
            size_t i = 0;
            const size_t simd_width = 4;

            for (; i + simd_width <= n; i += simd_width)
            {
                __m256d va = _mm256_loadu_pd(a + i);
                __m256d vb = _mm256_loadu_pd(b + i);
                __m256d vr = _mm256_mul_pd(va, vb);
                _mm256_storeu_pd(result + i, vr);
            }

            for (; i < n; ++i)
            {
                result[i] = a[i] * b[i];
            }
        }
        else
        {
            for (size_t i = 0; i < n; ++i)
            {
                result[i] = a[i] * b[i];
            }
        }
#else
        for (size_t i = 0; i < n; ++i)
        {
            result[i] = a[i] * b[i];
        }
#endif
    }

    /**
     * @brief SIMD-optimized scalar multiplication
     */
    void simd_scalar_mul(const T* a, T scalar, T* result, size_t n) const
    {
#ifdef __AVX2__
        if constexpr (std::is_same_v<T, float>)
        {
            size_t i = 0;
            const size_t simd_width = 8;
            __m256 vs = _mm256_set1_ps(scalar);

            for (; i + simd_width <= n; i += simd_width)
            {
                __m256 va = _mm256_loadu_ps(a + i);
                __m256 vr = _mm256_mul_ps(va, vs);
                _mm256_storeu_ps(result + i, vr);
            }

            for (; i < n; ++i)
            {
                result[i] = a[i] * scalar;
            }
        }
        else if constexpr (std::is_same_v<T, double>)
        {
            size_t i = 0;
            const size_t simd_width = 4;
            __m256d vs = _mm256_set1_pd(scalar);

            for (; i + simd_width <= n; i += simd_width)
            {
                __m256d va = _mm256_loadu_pd(a + i);
                __m256d vr = _mm256_mul_pd(va, vs);
                _mm256_storeu_pd(result + i, vr);
            }

            for (; i < n; ++i)
            {
                result[i] = a[i] * scalar;
            }
        }
        else
        {
            for (size_t i = 0; i < n; ++i)
            {
                result[i] = a[i] * scalar;
            }
        }
#else
        for (size_t i = 0; i < n; ++i)
        {
            result[i] = a[i] * scalar;
        }
#endif
    }

    /**
     * @brief Parallel SIMD-optimized addition using ThreadPool for large tensors
     * @param a First input array
     * @param b Second input array
     * @param result Output array
     * @param n Number of elements
     * @param min_size Minimum size to use parallel execution (default 10000)
     */
    void parallel_simd_add(const T* a, const T* b, T* result, size_t n, size_t min_size = 10000) const
    {
        if (n < min_size)
        {
            // Use serial SIMD for small tensors
            simd_add(a, b, result, n);
            return;
        }

        static thread_local std::optional<ThreadPool> pool;
        if (!pool)
        {
            pool.emplace(std::thread::hardware_concurrency());
        }

        const size_t num_threads = pool->thread_count();
        const size_t chunk_size = (n + num_threads - 1) / num_threads;

        std::vector<std::future<void>> futures;
        futures.reserve(num_threads);

        for (size_t i = 0; i < num_threads; ++i)
        {
            size_t start = i * chunk_size;
            size_t end = std::min(start + chunk_size, n);

            if (start >= n)
            {
                break;
            }

            futures.push_back(pool->submit([this, a, b, result, start, end]() {
                simd_add(a + start, b + start, result + start, end - start);
            }));
        }

        // Wait for all tasks to complete
        for (auto& fut : futures)
        {
            fut.get();
        }
    }

    /**
     * @brief Parallel SIMD-optimized subtraction using ThreadPool
     */
    void parallel_simd_sub(const T* a, const T* b, T* result, size_t n, size_t min_size = 10000) const
    {
        if (n < min_size)
        {
            simd_sub(a, b, result, n);
            return;
        }

        static thread_local std::optional<ThreadPool> pool;
        if (!pool)
        {
            pool.emplace(std::thread::hardware_concurrency());
        }

        const size_t num_threads = pool->thread_count();
        const size_t chunk_size = (n + num_threads - 1) / num_threads;

        std::vector<std::future<void>> futures;
        futures.reserve(num_threads);

        for (size_t i = 0; i < num_threads; ++i)
        {
            size_t start = i * chunk_size;
            size_t end = std::min(start + chunk_size, n);

            if (start >= n)
            {
                break;
            }

            futures.push_back(pool->submit([this, a, b, result, start, end]() {
                simd_sub(a + start, b + start, result + start, end - start);
            }));
        }

        for (auto& fut : futures)
        {
            fut.get();
        }
    }

    /**
     * @brief Parallel SIMD-optimized multiplication using ThreadPool
     */
    void parallel_simd_mul(const T* a, const T* b, T* result, size_t n, size_t min_size = 10000) const
    {
        if (n < min_size)
        {
            simd_mul(a, b, result, n);
            return;
        }

        static thread_local std::optional<ThreadPool> pool;
        if (!pool)
        {
            pool.emplace(std::thread::hardware_concurrency());
        }

        const size_t num_threads = pool->thread_count();
        const size_t chunk_size = (n + num_threads - 1) / num_threads;

        std::vector<std::future<void>> futures;
        futures.reserve(num_threads);

        for (size_t i = 0; i < num_threads; ++i)
        {
            size_t start = i * chunk_size;
            size_t end = std::min(start + chunk_size, n);

            if (start >= n)
            {
                break;
            }

            futures.push_back(pool->submit([this, a, b, result, start, end]() {
                simd_mul(a + start, b + start, result + start, end - start);
            }));
        }

        for (auto& fut : futures)
        {
            fut.get();
        }
    }

public:
    // =========================================================================
    // Broadcasting Support (NumPy-style)
    // =========================================================================

    /**
     * @brief Compute broadcast shape for two tensors (NumPy-style broadcasting rules)
     * @return Expected containing broadcast shape on success, error message on failure
     */
    Expected<std::vector<size_t>, std::string> compute_broadcast_shape(const std::vector<size_t>& other_shape) const
    {
        size_t max_rank = std::max(mShape.size(), other_shape.size());
        std::vector<size_t> result(max_rank, 1);

        for (size_t i = 0; i < max_rank; ++i)
        {
            size_t d1 = (i < mShape.size()) ? mShape[mShape.size() - i - 1] : 1;
            size_t d2 = (i < other_shape.size()) ? other_shape[other_shape.size() - i - 1] : 1;

            if (d1 != d2 && d1 != 1 && d2 != 1)
            {
                return make_unexpected(std::string("Incompatible shapes for broadcasting"));
            }
            result[max_rank - i - 1] = std::max(d1, d2);
        }
        return result;
    }

    /**
     * @brief Check if two shapes are broadcastable
     */
    bool is_broadcastable(const std::vector<size_t>& other_shape) const
    {
        return compute_broadcast_shape(other_shape).has_value();
    }

    /**
     * @brief Broadcast this tensor to a target shape
     * @param target_shape Desired output shape
     * @return Expected containing broadcasted tensor on success
     */
    Expected<Tensor, std::string> broadcast_to(const std::vector<size_t>& target_shape) const
    {
        auto result = compute_broadcast_shape(target_shape);
        if (!result.has_value())
        {
            return make_unexpected(result.error());
        }

        const auto& bcast_shape = result.value();
        if (bcast_shape != target_shape)
        {
            return make_unexpected(std::string("Computed broadcast shape doesn't match target"));
        }

        // Create output tensor
        Tensor output(target_shape);

        // Fill output using broadcasting rules
        for (size_t i = 0; i < output.mSize; ++i)
        {
            std::vector<size_t> out_indices(target_shape.size());
            size_t temp = i;
            for (int d = static_cast<int>(target_shape.size()) - 1; d >= 0; --d)
            {
                auto ud = static_cast<size_t>(d);
                out_indices[ud] = temp % target_shape[ud];
                temp /= target_shape[ud];
            }

            // Map to input indices (broadcasting)
            std::vector<size_t> in_indices(mShape.size());
            for (size_t d = 0; d < mShape.size(); ++d)
            {
                size_t out_idx = out_indices[target_shape.size() - mShape.size() + d];
                in_indices[d] = (mShape[d] == 1) ? 0 : out_idx;
            }

            output.mData[i] = mData[compute_offset_from_vector(in_indices)];
        }

        return output;
    }

public:
    /**
     * @brief Element-wise addition with SIMD optimization (auto-parallel for large tensors)
     * Uses parallel execution for tensors > 10,000 elements
     */
    Tensor operator+(const Tensor& other) const
    {
        check_shape_compatibility(other, "addition");
        Tensor result(mShape);
        parallel_simd_add(mData, other.mData, result.mData, mSize);
        return result;
    }

    /**
     * @brief Safe element-wise addition with broadcasting support (Expected return)
     * @return Expected containing result tensor or error message
     */
    Expected<Tensor, std::string> add_safe(const Tensor& other) const noexcept
    {
        if (mShape == other.mShape)
        {
            // Fast path: same shape
            Tensor result(mShape);
            parallel_simd_add(mData, other.mData, result.mData, mSize);
            return result;
        }

        // Try broadcasting
        auto bcast_shape = compute_broadcast_shape(other.mShape);
        if (!bcast_shape.has_value())
        {
            return make_unexpected(bcast_shape.error());
        }

        // Broadcast and compute
        auto lhs_bcast = broadcast_to(bcast_shape.value());
        if (!lhs_bcast.has_value())
        {
            return make_unexpected(lhs_bcast.error());
        }

        auto rhs_bcast = other.broadcast_to(bcast_shape.value());
        if (!rhs_bcast.has_value())
        {
            return make_unexpected(rhs_bcast.error());
        }

        Tensor result(bcast_shape.value());
        parallel_simd_add(lhs_bcast.value().mData, rhs_bcast.value().mData, result.mData, result.mSize);
        return result;
    }

    /**
     * @brief Element-wise subtraction with SIMD optimization (auto-parallel for large tensors)
     */
    Tensor operator-(const Tensor& other) const
    {
        check_shape_compatibility(other, "subtraction");
        Tensor result(mShape);
        parallel_simd_sub(mData, other.mData, result.mData, mSize);
        return result;
    }

    /**
     * @brief Safe element-wise subtraction with broadcasting support
     */
    Expected<Tensor, std::string> sub_safe(const Tensor& other) const noexcept
    {
        if (mShape == other.mShape)
        {
            Tensor result(mShape);
            parallel_simd_sub(mData, other.mData, result.mData, mSize);
            return result;
        }

        auto bcast_shape = compute_broadcast_shape(other.mShape);
        if (!bcast_shape.has_value())
        {
            return make_unexpected(bcast_shape.error());
        }

        auto lhs_bcast = broadcast_to(bcast_shape.value());
        if (!lhs_bcast.has_value())
        {
            return make_unexpected(lhs_bcast.error());
        }

        auto rhs_bcast = other.broadcast_to(bcast_shape.value());
        if (!rhs_bcast.has_value())
        {
            return make_unexpected(rhs_bcast.error());
        }

        Tensor result(bcast_shape.value());
        parallel_simd_sub(lhs_bcast.value().mData, rhs_bcast.value().mData, result.mData, result.mSize);
        return result;
    }

    /**
     * @brief Element-wise multiplication with SIMD optimization (auto-parallel for large tensors)
     */
    Tensor operator*(const Tensor& other) const
    {
        check_shape_compatibility(other, "multiplication");
        Tensor result(mShape);
        parallel_simd_mul(mData, other.mData, result.mData, mSize);
        return result;
    }

    /**
     * @brief Safe element-wise multiplication with broadcasting support
     */
    Expected<Tensor, std::string> mul_safe(const Tensor& other) const noexcept
    {
        if (mShape == other.mShape)
        {
            Tensor result(mShape);
            parallel_simd_mul(mData, other.mData, result.mData, mSize);
            return result;
        }

        auto bcast_shape = compute_broadcast_shape(other.mShape);
        if (!bcast_shape.has_value())
        {
            return make_unexpected(bcast_shape.error());
        }

        auto lhs_bcast = broadcast_to(bcast_shape.value());
        if (!lhs_bcast.has_value())
        {
            return make_unexpected(lhs_bcast.error());
        }

        auto rhs_bcast = other.broadcast_to(bcast_shape.value());
        if (!rhs_bcast.has_value())
        {
            return make_unexpected(rhs_bcast.error());
        }

        Tensor result(bcast_shape.value());
        parallel_simd_mul(lhs_bcast.value().mData, rhs_bcast.value().mData, result.mData, result.mSize);
        return result;
    }

    /**
     * @brief Safe view creation with Expected return
     * @return Expected containing view tensor or error message
     */
    Expected<Tensor, std::string> view_safe(const std::vector<size_t>& start_indices,
                                            const std::vector<size_t>& end_indices) const noexcept
    {
        if (start_indices.size() != mShape.size() || end_indices.size() != mShape.size())
        {
            return make_unexpected(std::string("Index dimensions must match tensor rank"));
        }

        for (size_t i = 0; i < start_indices.size(); ++i)
        {
            if (start_indices[i] >= end_indices[i])
            {
                return make_unexpected(std::string("Start index must be less than end index"));
            }
            if (end_indices[i] > mShape[i])
            {
                return make_unexpected(std::string("End index out of bounds"));
            }
        }

        return view(start_indices, end_indices);
    }

    /**
     * @brief Safe reshape with Expected return
     */
    Expected<Tensor, std::string> reshape_safe(const std::vector<size_t>& new_shape) const noexcept
    {
        size_t new_size = 1;
        for (auto dim : new_shape)
        {
            new_size *= dim;
        }

        if (new_size != mSize)
        {
            return make_unexpected(std::string("Reshape size mismatch"));
        }

        return reshape(new_shape);
    }

public:
    /**
     * @brief Element-wise addition with SIMD optimization (REMOVED - replaced above)
     */
    // Tensor operator+(const Tensor& other) const { ... }  // Already added above

    /**
     * @brief Element-wise subtraction with SIMD optimization (REMOVED - replaced above)
     */
    // Tensor operator-(const Tensor& other) const { ... }  // Already added above

    /**
     * @brief Element-wise multiplication with SIMD optimization (REMOVED - replaced above)
     */
    // Tensor operator*(const Tensor& other) const { ... }  // Already added above

    /**
     * @brief Scalar multiplication with SIMD optimization
     */
    Tensor operator*(const T& scalar) const
    {
        Tensor result(mShape);
        simd_scalar_mul(mData, scalar, result.mData, mSize);
        return result;
    }

    /**
     * @brief Scalar division
     */
    Tensor operator/(const T& scalar) const
    {
        FATP_ENFORCE(scalar != T{0}, "Division by zero in tensor scalar division");
        Tensor result(mShape);
        simd_scalar_mul(mData, T{1} / scalar, result.mData, mSize);
        return result;
    }

    /**
     * @brief Exact equality comparison operator (for std::unordered_map)
     *
     * @note This performs EXACT (bitwise) comparison, which is required for
     *       hash map consistency. For floating-point tensors, use approx_equal()
     *       or the EqualDispatcher from EqualityComparisons.h for epsilon-based
     *       comparison in tests.
     */
    bool operator==(const Tensor& other) const
    {
        // Check shape match
        if (mShape != other.mShape)
        {
            return false;
        }
        if (mSize != other.mSize)
        {
            return false;
        }

        // Exact comparison using iterators (stride-aware)
        auto it1 = begin();
        auto it2 = other.begin();
        for (size_t i = 0; i < mSize; ++i, ++it1, ++it2)
        {
            if (*it1 != *it2)
            {
                return false; // Bitwise comparison
            }
        }
        return true;
    }

    /**
     * @brief Inequality comparison operator
     */
    bool operator!=(const Tensor& other) const
    {
        return !(*this == other);
    }

    /**
     * @brief Approximate equality comparison with epsilon tolerance
     *
     * @param other Tensor to compare with
     * @param epsilon Tolerance for floating-point comparison (default: 1e-6 for float, 1e-10 for double)
     * @return true if tensors are approximately equal
     *
     * @note This is the recommended comparison method for floating-point tensors.
     *       For use with EqualityComparisons.h policies, see EqualDispatcher specialization.
     */
    bool approx_equal(const Tensor& other, T epsilon = default_epsilon()) const
    {
        // Check shape match
        if (mShape != other.mShape)
        {
            return false;
        }
        if (mSize != other.mSize)
        {
            return false;
        }

        // Epsilon-aware comparison using iterators (stride-aware)
        auto it1 = begin();
        auto it2 = other.begin();

        if constexpr (std::is_floating_point_v<T>)
        {
            for (size_t i = 0; i < mSize; ++i, ++it1, ++it2)
            {
                T diff = std::abs(*it1 - *it2);
                T max_val = std::max(std::abs(*it1), std::abs(*it2));

                // Relative comparison for large values, absolute for small
                if (max_val > T{1})
                {
                    if (diff > epsilon * max_val)
                    {
                        return false;
                    }
                }
                else
                {
                    if (diff > epsilon)
                    {
                        return false;
                    }
                }
            }
        }
        else
        {
            // For integer types, fall back to exact comparison
            for (size_t i = 0; i < mSize; ++i, ++it1, ++it2)
            {
                if (*it1 != *it2)
                {
                    return false;
                }
            }
        }
        return true;
    }

    /**
     * @brief Approximate inequality comparison
     */
    bool approx_not_equal(const Tensor& other, T epsilon = default_epsilon()) const
    {
        return !approx_equal(other, epsilon);
    }

    // =========================================================================
    // Expression Templates - Lazy Evaluation Support
    // =========================================================================

    /**
     * @brief Assignment from expression template (evaluates the expression)
     * @tparam Expr Expression type (LazyAdd, LazySubtract, etc.)
     */
    template <typename Expr>
    Tensor& operator=(const Expr& expr)
        requires is_tensor_expression<Expr>::value
    {
        if (mShape != expr.shape())
        {
            // Reallocate if shapes don't match
            mShape = expr.shape();
            mSize = compute_size(mShape);
            mStrides = compute_strides(mShape);

            T* raw = mAllocator.allocate(mSize);
            shared_data_ = std::shared_ptr<T[]>(raw, [this](T* p) {
                mAllocator.deallocate(p, mSize);
            });
            mData = shared_data_.get();
        }

        // Evaluate expression in single pass (loop fusion)
        for (size_t i = 0; i < mSize; ++i)
        {
            mData[i] = expr[i];
        }
        return *this;
    }

    /**
     * @brief Create lazy addition expression (doesn't compute immediately)
     * @note Use for chained operations: a.lazy_add(b).lazy_add(c) avoids temporaries
     */
    template <typename Other>
    LazyAdd<Tensor, Other> lazy_add(const Other& other) const
    {
        return LazyAdd<Tensor, Other>(*this, other);
    }

    /**
     * @brief Create lazy subtraction expression
     */
    template <typename Other>
    LazySubtract<Tensor, Other> lazy_sub(const Other& other) const
    {
        return LazySubtract<Tensor, Other>(*this, other);
    }

    /**
     * @brief Create lazy multiplication expression
     */
    template <typename Other>
    LazyMultiply<Tensor, Other> lazy_mul(const Other& other) const
    {
        return LazyMultiply<Tensor, Other>(*this, other);
    }

    /**
     * @brief Create lazy scalar multiplication expression
     */
    LazyScalarMultiply<Tensor> lazy_mul_scalar(const T& scalar) const
    {
        return LazyScalarMultiply<Tensor>(*this, scalar);
    }

private:
    /**
     * @brief Default epsilon for floating-point comparisons
     */
    static constexpr T default_epsilon()
    {
        if constexpr (std::is_same_v<T, float>)
        {
            return T{1e-6f};
        }
        else if constexpr (std::is_same_v<T, double>)
        {
            return T{1e-10};
        }
        else
        {
            return T{0}; // Exact for integers
        }
    }

public:
    /**
     * @brief Sum all elements
     */
    T sum() const
    {
        return std::accumulate(mData, mData + mSize, T{0});
    }

    /**
     * @brief Mean of all elements
     */
    T mean() const
    {
        return sum() / static_cast<T>(mSize);
    }

    /**
     * @brief Maximum element
     */
    T max() const
    {
        return *std::max_element(mData, mData + mSize);
    }

    /**
     * @brief Minimum element
     */
    T min() const
    {
        return *std::min_element(mData, mData + mSize);
    }

    // =========================================================================
    // Utility
    // =========================================================================

    void swap(Tensor& other) noexcept
    {
        std::swap(shared_data_, other.shared_data_);
        std::swap(mData, other.mData);
        std::swap(mShape, other.mShape);
        std::swap(mStrides, other.mStrides);
        std::swap(mSize, other.mSize);
    }

private:
    // Private constructor for views
    Tensor(std::shared_ptr<T[]> shared_data, T* data, std::vector<size_t> shape, std::vector<ptrdiff_t> strides)
        : shared_data_(std::move(shared_data))
        , mData(data)
        , mShape(std::move(shape))
        , mStrides(std::move(strides))
        , mSize(compute_size(mShape))
    {
    }

    /**
     * @brief Compute total size from shape (overflow-safe with CheckedArithmetic)
     */
    static size_t compute_size(const std::vector<size_t>& shape)
    {
        if (shape.empty())
        {
            return 0;
        }

        size_t size = 1;
        for (auto dim : shape)
        {
            // Use checked_mul from CheckedArithmetic.h to detect overflow
            size = checked_mul<ThrowOnErrorPolicy>(size, dim);
        }
        return size;
    }

    /**
     * @brief Compute strides from shape (row-major, overflow-safe)
     */
    static std::vector<ptrdiff_t> compute_strides(const std::vector<size_t>& shape)
    {
        if (shape.empty())
        {
            return {};
        }

        std::vector<ptrdiff_t> strides(shape.size());
        ptrdiff_t stride = 1;
        for (size_t i = shape.size(); i > 0; --i)
        {
            strides[i - 1] = stride;
            // Use checked_mul to prevent overflow in stride computation
            stride = static_cast<ptrdiff_t>(checked_mul<ThrowOnErrorPolicy>(static_cast<size_t>(stride), shape[i - 1]));
        }
        return strides;
    }

    // Compute linear offset from multi-dimensional indices (overflow-safe)
    template <typename... Indices>
    size_t compute_offset(Indices... indices) const
    {
        std::array<size_t, sizeof...(Indices)> idx_array = {indices...};
        ptrdiff_t offset = 0; // Use signed arithmetic throughout

        for (size_t i = 0; i < idx_array.size(); ++i)
        {
            // Compute with signed arithmetic to handle negative strides
            ptrdiff_t idx_signed = static_cast<ptrdiff_t>(idx_array[i]);
            ptrdiff_t term = checked_mul<ThrowOnErrorPolicy>(idx_signed, mStrides[i]);
            offset = checked_add<ThrowOnErrorPolicy>(offset, term);
        }

        // Offset should be non-negative for valid indices
        FATP_ALWAYS_ENFORCE(offset >= 0, "Computed offset is negative - invalid indices");
        return static_cast<size_t>(offset);
    }

    // Compute offset from vector of indices (overflow-safe)
    size_t compute_offset_from_vector(const std::vector<size_t>& indices) const
    {
        ptrdiff_t offset = 0; // Use signed arithmetic throughout

        for (size_t i = 0; i < indices.size(); ++i)
        {
            // Compute with signed arithmetic to handle negative strides
            ptrdiff_t idx_signed = static_cast<ptrdiff_t>(indices[i]);
            ptrdiff_t term = checked_mul<ThrowOnErrorPolicy>(idx_signed, mStrides[i]);
            offset = checked_add<ThrowOnErrorPolicy>(offset, term);
        }

        // Offset should be non-negative for valid indices
        FATP_ALWAYS_ENFORCE(offset >= 0, "Computed offset is negative - invalid indices");
        return static_cast<size_t>(offset);
    }

    /**
     * @brief Check if tensor has contiguous row-major strides
     * @details For views, strides may not be contiguous. This helper is used
     *          to determine if simple offset bounds checking is valid.
     */
    bool is_contiguous() const
    {
        if (mShape.empty())
        {
            return true;
        }

        ptrdiff_t expected_stride = 1;
        for (size_t i = mShape.size(); i > 0; --i)
        {
            if (mStrides[i - 1] != expected_stride)
            {
                return false;
            }
            expected_stride *= static_cast<ptrdiff_t>(mShape[i - 1]);
        }
        return true;
    }

    std::shared_ptr<T[]>
        shared_data_; // Using std::shared_ptr for now - consider TensorStorage for 10-20% performance gain
    T* mData;
    std::vector<size_t> mShape;
    std::vector<ptrdiff_t> mStrides;
    size_t mSize;
    Allocator mAllocator;
};

// =============================================================================
// Type Aliases (Optimized Backend)
// =============================================================================

template <typename T, typename Alloc = TensorAllocator<T>>
using RowMajorTensor = Tensor<T, Alloc, RowMajorPolicy>;

template <typename T, typename Alloc = TensorAllocator<T>>
using ColumnMajorTensor = Tensor<T, Alloc, ColumnMajorPolicy>;

template <typename T, typename Alloc = TensorAllocator<T>>
using StridedTensor = Tensor<T, Alloc, StridedPolicy>;

template <typename T, size_t BlockSize = 64, typename Alloc = TensorAllocator<T>>
using BlockedTensor = Tensor<T, Alloc, BlockedPolicy<BlockSize>>;

// Convenience alias for default tensor
template <typename T>
using OptimizedTensor = Tensor<T, TensorAllocator<T>, RowMajorPolicy>;

} // namespace fat_p

// =============================================================================
// std::hash Specialization (must be in std namespace)
// =============================================================================
namespace std
{
template <typename T, typename Alloc, typename IteratorPolicy>
struct hash<fat_p::Tensor<T, Alloc, IteratorPolicy>>
{
    size_t operator()(const fat_p::Tensor<T, Alloc, IteratorPolicy>& tensor) const
    {
        size_t seed = 0;

        // Hash shape
        for (auto dim : tensor.shape())
        {
            seed ^= std::hash<size_t>{}(dim) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }

        // Hash strides
        for (auto stride : tensor.strides())
        {
            seed ^= std::hash<ptrdiff_t>{}(stride) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }

        // Hash data (stride-aware via iterator)
        auto it = tensor.begin();
        for (size_t i = 0; i < tensor.size(); ++i, ++it)
        {
            size_t elem_hash;
            if constexpr (std::is_floating_point_v<T>)
            {
                // Bit-cast float to int for hash (handles NaN consistently)
                typename std::conditional<sizeof(T) == 4, uint32_t, uint64_t>::type bits;
                std::memcpy(&bits, &(*it), sizeof(T));
                elem_hash = std::hash<decltype(bits)>{}(bits);
            }
            else
            {
                elem_hash = std::hash<T>{}(*it);
            }
            seed ^= elem_hash + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
        return seed;
    }
};
} // namespace std
