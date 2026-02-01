/**
 * @file TensorIteration.h
 * @brief Composition helpers for efficient N-dimensional tensor iteration.
 *
 * @layer Domain
 *
 * This file provides convenient functions for iterating over N-dimensional
 * tensors using composition of optimized Stride1D/Stride2D policies.
 *
 * Performance strategy:
 * - 1D tensors use Stride1DPolicy directly
 * - 2D tensors use Stride2DPolicy directly
 * - 3D+ tensors use scalar outer loops with Stride2DPolicy for inner 2D
 *
 * This achieves equivalent performance to hand-written nested loops while
 * eliminating boilerplate and reducing opportunities for errors.
 *
 * @note This file requires both PolicyIterator.h and TensorStridePolicy.h
 *
 * Part of the Fat-P Library.
 *
 * @copyright This file is part of a proprietary library. All rights reserved.
 */

#pragma once

/*
FATP_META:
  meta_version: 1
  component: TensorIteration
  file_role: public_header
  path: include/fat_p/TensorIteration.h
  namespace: fat_p
  layer: Domain
  summary: "Public header for TensorIteration."
  api_stability: in_work
  related:
    docs_search: "TensorIteration"
    tests:
      - components/PolicyIterator/tests/test_PolicyIterator.cpp
    benchmarks:
      - components/PolicyIterator/benchmarks/benchmark_PolicyIterator.cpp
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
// Explicit includes for header self-containment (do not rely on transitive includes)
#include <cstddef>
#include <initializer_list>
#include <utility>

#include "enforce.h"
#include "SmallVector.h"

#include "PolicyIterator.h"
#include "TensorStridePolicy.h"

namespace fat_p::iterator
{

// ============================================================================
// PERFORMANCE MODEL / POLICY SELECTION
// ============================================================================
//
// TensorIteration provides high-level helpers for iterating over tensors using PolicyIterator,
// with an emphasis on practical performance for common layouts.
//
// The core idea:
//   - Use lightweight specialized policies in common hot cases (1D/2D row-major),
//   - Avoid paying the full generality overhead of TensorStridePolicy when it isn't needed.
//
// EXPECTATIONS:
//   - For 1D runtime-stride walks (e.g., scanning a column): prefer Stride1DPolicy behavior.
//   - For 2D row-major traversal (contiguous or padded rows): prefer Stride2DPolicy behavior.
//   - For arbitrary N-D / permuted axes / complex stride patterns: use TensorStridePolicy directly.
//
// LIMITATIONS / CONTRACT:
//   These helpers are optimized for monotonic row-major-like traversal in the innermost planes.
//   If you pass stride patterns that imply non-monotonic pointer progression (e.g., column-major
//   traversal over row-major storage without permuting dimensions), the specialized fast paths
//   are not applicable. In such cases:
//     - either use TensorStridePolicy (general, slower), or
//     - treat the usage as a contract violation (debug enforce / UB in release), depending on
//       your project policy.
//
// NOTE:
//   Tuned math libraries (e.g., Eigen) may still outperform iterator-based scalar loops on
//   contiguous reductions due to SIMD/vectorized kernels. Use the specialized policies where
//   appropriate, and use library kernels for peak contiguous throughput.
// ============================================================================

// ============================================================================
// Implementation Details
// ============================================================================

namespace detail
{

/**
 * @brief Recursive N-D iteration implementation.
 *
 * Strategy:
 * - 1D: Use Stride1DPolicy (optimal for strided 1D)
 * - 2D: Use Stride2DPolicy (optimal for 2D planes)
 * - 3D+: Scalar loop for outermost dim, recurse on inner (N-1)D
 *
 * The recursion depth is (N-2) for N dimensions, which is negligible
 * overhead since outer dimensions typically have small extents.
 */
template <typename T, typename Func>
void iterateNDImpl(T* base, const std::size_t* shape, const std::ptrdiff_t* strides, std::size_t ndim, Func& fn)
{
    if (ndim == 1)
    {
        // 1D: use Stride1DPolicy
        Stride1DPolicy<T> policy(shape[0], strides[0]);
        using Iter = PolicyIterator<T, Stride1DPolicy<T>>;

        T* bufferEnd = base + static_cast<std::ptrdiff_t>(shape[0]) * strides[0];
        auto it = Iter::begin(base, bufferEnd, policy);
        auto end = Iter::end(base, bufferEnd, policy);

        for (; it != end; ++it)
        {
            fn(*it);
        }
    }
    else if (ndim == 2)
    {
        // 2D: use Stride2DPolicy (the efficient inner kernel)
        Stride2DPolicy<T> policy(shape[0], shape[1], strides[0], strides[1]);
        using Iter = PolicyIterator<T, Stride2DPolicy<T>>;

        T* bufferEnd = base + static_cast<std::ptrdiff_t>(shape[0]) * strides[0];
        auto it = Iter::begin(base, bufferEnd, policy);
        auto end = Iter::end(base, bufferEnd, policy);

        for (; it != end; ++it)
        {
            fn(*it);
        }
    }
    else
    {
        // N-D where N >= 3: loop over outermost dimension, recurse
        for (std::size_t i = 0; i < shape[0]; ++i)
        {
            T* slice = base + static_cast<std::ptrdiff_t>(i) * strides[0];
            iterateNDImpl(slice, shape + 1, strides + 1, ndim - 1, fn);
        }
    }
}

/**
 * @brief Compute row-major strides for a given shape.
 * @param shape Tensor dimensions.
 * @param[out] strides Output strides (must be same size as shape).
 *
 * Row-major means stride[i] = product of shape[i+1..N-1].
 * Last dimension has stride 1.
 */
inline void computeRowMajorStrides(const std::size_t* shape, std::ptrdiff_t* strides, std::size_t ndim)
{
    std::ptrdiff_t stride = 1;
    for (std::size_t i = ndim; i-- > 0;)
    {
        strides[i] = stride;
        stride *= static_cast<std::ptrdiff_t>(shape[i]);
    }
}

} // namespace detail

// ============================================================================
// Public API: iterateND
// ============================================================================

/**
 * @brief Iterate all elements of an N-dimensional tensor.
 * @tparam T Element type.
 * @tparam Func Callable with signature void(T&) or void(const T&).
 *
 * @param base Pointer to first element.
 * @param shape Dimensions (outer to inner).
 * @param strides Memory strides for each dimension.
 * @param fn Function to apply to each element.
 *
 * @pre shape.size() == strides.size()
 * @pre shape.size() >= 1
 * @pre All shape dimensions > 0
 * @pre All strides > 0
 *
 * Iteration order: dim[0] (slowest) â†’ dim[1] â†’ ... â†’ dim[N-1] (fastest)
 *
 * @par Performance
 * Equivalent to hand-written nested loops. The innermost 2 dimensions
 * use the optimized Stride2DPolicy, achieving ~1.3x manual loop performance.
 *
 * @note This helper is optimized for monotonic row-major layouts (positive strides;
 *       innermost 2D plane is row-major). For arbitrary stride permutations
 *       (e.g., column-major traversal, negative strides), use TensorStridePolicy
 *       directly (more general but slower).
 *
 * @par Example
 * @code
 * // 3D volume sum
 * int64_t sum = 0;
 * iterateND(data, {100, 200, 300}, {200*300, 300, 1},
 *           [&](int64_t v) { sum += v; });
 *
 * // 4D NCHW tensor normalization
 * iterateND(tensor, {32, 64, 224, 224}, {64*224*224, 224*224, 224, 1},
 *           [&](float& v) { v = (v - mean) / stddev; });
 *
 * // Works for any dimensionality (1D to N-D)
 * iterateND(vec, {1000}, {1}, [](int& v) { ++v; });      // 1D
 * iterateND(mat, {100, 200}, {200, 1}, [](int& v) {}); // 2D
 * @endcode
 */
template <typename T, typename Func>
void iterateND(T* base,
               std::initializer_list<std::size_t> shape,
               std::initializer_list<std::ptrdiff_t> strides,
               Func&& fn)
{
    FATP_ENFORCE(shape.size() == strides.size(), "Shape and strides must have same size");
    FATP_ENFORCE(shape.size() >= 1, "At least 1 dimension required");

    // Use SmallVector to avoid heap allocation for typical tensor ranks (â‰¤8D)
    SmallVector<std::size_t, 8> shapeVec(shape);
    SmallVector<std::ptrdiff_t, 8> strideVec(strides);

    // Contract enforcement (debug): prevent UB from negative/zero strides before pointer arithmetic
    for (std::size_t i = 0; i < shapeVec.size(); ++i)
    {
        FATP_ENFORCE(shapeVec[i] > 0, "All dimensions must be > 0");
        FATP_ENFORCE(strideVec[i] > 0, "All strides must be > 0");
    }

    detail::iterateNDImpl(base, shapeVec.data(), strideVec.data(), shapeVec.size(), fn);
}

/**
 * @brief Iterate N-D tensor with row-major contiguous layout.
 *
 * Convenience overload that computes strides automatically assuming
 * C-style row-major order (last dimension contiguous, stride 1).
 *
 * @par Example
 * @code
 * // 3D volume - strides computed automatically as {H*W, W, 1}
 * iterateND(data, {100, 200, 300}, [](int64_t& v) { v *= 2; });
 *
 * // 5D tensor - strides computed as {d1*d2*d3*d4, d2*d3*d4, d3*d4, d4, 1}
 * iterateND(tensor, {8, 30, 3, 1080, 1920},
 *           [](uint8_t& pixel) { pixel = 255 - pixel; });
 * @endcode
 */
template <typename T, typename Func>
void iterateND(T* base, std::initializer_list<std::size_t> shape, Func&& fn)
{
    FATP_ENFORCE(shape.size() >= 1, "At least 1 dimension required");

    SmallVector<std::size_t, 8> shapeVec(shape);
    SmallVector<std::ptrdiff_t, 8> strideVec(shape.size());

    // Contract enforcement (debug): dimensions must be non-zero for row-major traversal
    for (std::size_t i = 0; i < shapeVec.size(); ++i)
    {
        FATP_ENFORCE(shapeVec[i] > 0, "All dimensions must be > 0");
    }

    detail::computeRowMajorStrides(shapeVec.data(), strideVec.data(), shapeVec.size());

    detail::iterateNDImpl(base, shapeVec.data(), strideVec.data(), shapeVec.size(), fn);
}

// ============================================================================
// Public API: reduceND
// ============================================================================

/**
 * @brief Reduce all elements of an N-D tensor to a single value.
 * @tparam T Element type.
 * @tparam Acc Accumulator type.
 * @tparam BinaryOp Binary operation with signature Acc(Acc, T).
 *
 * @param base Pointer to first element.
 * @param shape Tensor dimensions.
 * @param strides Memory strides.
 * @param init Initial accumulator value.
 * @param op Binary operation to combine accumulator with each element.
 * @return Final accumulated value.
 *
 * @par Example
 * @code
 * // Sum with explicit strides
 * auto sum = reduceND(data, {100, 200, 300}, {256*300, 300, 1},
 *                     int64_t{0}, std::plus<>{});
 *
 * // Find maximum
 * auto maxVal = reduceND(data, {D, H, W}, {dS, rS, 1},
 *                        std::numeric_limits<int64_t>::min(),
 *                        [](auto a, auto b) { return std::max(a, b); });
 *
 * // Product of all elements
 * auto product = reduceND(data, {10, 20}, {20, 1},
 *                         1.0, std::multiplies<>{});
 * @endcode
 */
template <typename T, typename Acc, typename BinaryOp>
[[nodiscard]] Acc reduceND(T* base,
                           std::initializer_list<std::size_t> shape,
                           std::initializer_list<std::ptrdiff_t> strides,
                           Acc init,
                           BinaryOp op)
{
    Acc acc = std::move(init);
    iterateND(base, shape, strides, [&](const T& val) {
        acc = op(std::move(acc), val);
    });
    return acc;
}

/**
 * @brief Reduce N-D tensor with row-major contiguous layout.
 */
template <typename T, typename Acc, typename BinaryOp>
[[nodiscard]] Acc reduceND(T* base, std::initializer_list<std::size_t> shape, Acc init, BinaryOp op)
{
    Acc acc = std::move(init);
    iterateND(base, shape, [&](const T& val) {
        acc = op(std::move(acc), val);
    });
    return acc;
}

// ============================================================================
// Public API: transformND
// ============================================================================

/**
 * @brief Transform all elements of an N-D tensor in place.
 * @tparam T Element type.
 * @tparam UnaryOp Unary operation with signature T(T) or T(const T&).
 *
 * @param base Pointer to first element.
 * @param shape Tensor dimensions.
 * @param strides Memory strides.
 * @param op Unary operation to apply to each element.
 *
 * @par Example
 * @code
 * // Negate all elements
 * transformND(data, {100, 200}, {200, 1}, std::negate<>{});
 *
 * // Apply gamma correction
 * transformND(pixels, {H, W, 3}, {W*3, 3, 1},
 *             [gamma](uint8_t v) {
 *                 return static_cast<uint8_t>(255 * std::pow(v/255.0, gamma));
 *             });
 *
 * // Normalize to [0, 1]
 * float maxVal = computeMax(data);
 * transformND(data, {D, H, W}, {dS, rS, 1},
 *             [maxVal](float v) { return v / maxVal; });
 * @endcode
 */
template <typename T, typename UnaryOp>
void transformND(T* base,
                 std::initializer_list<std::size_t> shape,
                 std::initializer_list<std::ptrdiff_t> strides,
                 UnaryOp op)
{
    iterateND(base, shape, strides, [&](T& val) {
        val = op(val);
    });
}

/**
 * @brief Transform N-D tensor with row-major contiguous layout.
 */
template <typename T, typename UnaryOp>
void transformND(T* base, std::initializer_list<std::size_t> shape, UnaryOp op)
{
    iterateND(base, shape, [&](T& val) {
        val = op(val);
    });
}

// ============================================================================
// Public API: Slice Iteration (for advanced use cases)
// ============================================================================

/**
 * @brief Iterate over (N-1)-D slices of an N-D tensor.
 * @tparam T Element type.
 * @tparam SliceFunc Callable with signature void(size_t index, T* sliceBase).
 *
 * @param base Pointer to first element.
 * @param shape Tensor dimensions.
 * @param strides Memory strides.
 * @param fn Function called for each slice with (slice index, slice pointer).
 *
 * Useful when you need to process each slice differently, or when you need
 * early termination, or for parallelization (each slice can be processed
 * independently).
 *
 * @par Example
 * @code
 * // Process each 2D frame of a 3D volume independently
 * forEachSlice(volume, {100, 200, 300}, {200*300, 300, 1},
 *     [&](std::size_t z, int* slice) {
 *         // slice points to frame z, shape is {200, 300}
 *         auto frameSum = reduceND(slice, {200, 300}, {300, 1},
 *                                  int64_t{0}, std::plus<>{});
 *         std::cout << "Frame " << z << " sum: " << frameSum << "\n";
 *     });
 *
 * // Find first slice containing a negative value
 * std::optional<size_t> foundSlice;
 * forEachSlice(data, {D, H, W}, {dS, rS, 1},
 *     [&](std::size_t d, int* slice) {
 *         if (foundSlice) return;  // already found, skip
 *         bool hasNegative = false;
 *         iterateND(slice, {H, W}, {rS, 1},
 *                   [&](int v) { if (v < 0) hasNegative = true; });
 *         if (hasNegative) foundSlice = d;
 *     });
 * @endcode
 */
template <typename T, typename SliceFunc>
void forEachSlice(T* base,
                  std::initializer_list<std::size_t> shape,
                  std::initializer_list<std::ptrdiff_t> strides,
                  SliceFunc&& fn)
{
    FATP_ENFORCE(shape.size() >= 2, "forEachSlice requires at least 2 dimensions");
    FATP_ENFORCE(shape.size() == strides.size(), "Shape and strides must have same size");

    SmallVector<std::size_t, 8> shapeVec(shape);
    SmallVector<std::ptrdiff_t, 8> strideVec(strides);

    // Contract enforcement (debug): prevent UB from negative/zero strides before pointer arithmetic
    for (std::size_t i = 0; i < shapeVec.size(); ++i)
    {
        FATP_ENFORCE(shapeVec[i] > 0, "All dimensions must be > 0");
        FATP_ENFORCE(strideVec[i] > 0, "All strides must be > 0");
    }

    for (std::size_t i = 0; i < shapeVec[0]; ++i)
    {
        T* slice = base + static_cast<std::ptrdiff_t>(i) * strideVec[0];
        fn(i, slice);
    }
}

/**
 * @brief Iterate over slices with row-major contiguous layout.
 */
template <typename T, typename SliceFunc>
void forEachSlice(T* base, std::initializer_list<std::size_t> shape, SliceFunc&& fn)
{
    FATP_ENFORCE(shape.size() >= 2, "forEachSlice requires at least 2 dimensions");

    SmallVector<std::size_t, 8> shapeVec(shape);
    SmallVector<std::ptrdiff_t, 8> strideVec(shape.size());

    // Contract enforcement (debug): dimensions must be non-zero for row-major traversal
    for (std::size_t i = 0; i < shapeVec.size(); ++i)
    {
        FATP_ENFORCE(shapeVec[i] > 0, "All dimensions must be > 0");
    }

    detail::computeRowMajorStrides(shapeVec.data(), strideVec.data(), shapeVec.size());

    for (std::size_t i = 0; i < shapeVec[0]; ++i)
    {
        T* slice = base + static_cast<std::ptrdiff_t>(i) * strideVec[0];
        fn(i, slice);
    }
}

} // namespace fat_p::iterator
