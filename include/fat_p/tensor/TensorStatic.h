#pragma once

/*
FATP_META:
  meta_version: 1
  component: TensorStatic
  file_role: internal_header
  path: include/fat_p/tensor/TensorStatic.h
  namespace: fat_p
  layer: Domain
  summary: "Internal implementation for compile-time StaticTensor mathematics."
  api_stability: in_work
  related:
    docs_search: "TensorStatic"
    tests:
      - components/Tensor/tests/test_TensorStatic.cpp
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
 * @file TensorStatic.h
 * @brief Fixed-size N-dimensional tensor arithmetic
 *
 *
 * @version 1.1 - Fixed SaturatingPolicy namespace issue
 *
 * @details Provides compile-time fixed-size tensors (scalars, vectors, matrices,
 * and higher-order tensors) with policy-based error checking and SIMD acceleration.
 * Complements SimdMath.h (graphics-focused) with general numeric computing.
 *
 * Key features:
 * - Compile-time shape checking (no runtime dimension errors)
 * - Policy-based arithmetic (Checked, Unchecked, Saturating)
 * - Explicit AVX2 helpers for selected float operations
 * - Compile-time shapes and policy dispatch
 * - No heap allocations for fixed-size tensors
 *
 * @comparison
 * - vs SimdMath: General N-D tensors vs specialized Vec3/Mat4x4
 * - vs Eigen: Header-only, no dependencies, policy-based checking
 * - vs xtensor: Compile-time shapes, no heap, simpler API
 *
 * Requires: C++20, CheckedArithmetic.h (for checked policies)
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#ifdef __AVX2__
#include <immintrin.h>
#endif
#include <limits>
#include <numeric>
#include <stdexcept>
#include <type_traits>

#include "CheckedArithmetic.h"

namespace fat_p
{

// =============================================================================
// TENSOR ARITHMETIC POLICIES
// =============================================================================
// NOTE: These are TensorStatic-specific policies with static member functions.
// They are DIFFERENT from CheckedArithmetic policy tags (ThrowOnErrorPolicy,
// SaturatingPolicy, etc.) which are used as template parameters.
// =============================================================================

/**
 * @brief Policy for unchecked arithmetic (maximum performance)
 * @details No bounds checking, overflow detection, or NaN/Inf validation.
 * Use in performance-critical inner loops with validated inputs.
 */
struct UncheckedPolicy
{
    template <typename T>
    static constexpr T add(T a, T b) noexcept
    {
        return a + b;
    }

    template <typename T>
    static constexpr T sub(T a, T b) noexcept
    {
        return a - b;
    }

    template <typename T>
    static constexpr T mul(T a, T b) noexcept
    {
        return a * b;
    }

    template <typename T>
    static constexpr T div(T a, T b) noexcept
    {
        return a / b;
    }
};

/**
 * @brief Policy delegating to CheckedArithmetic for safety
 * @details Uses CheckedArithmetic's ThrowOnErrorPolicy for overflow detection.
 */
struct CheckedPolicy
{
    template <typename T>
    static constexpr T add(T a, T b)
    {
        if constexpr (std::is_floating_point_v<T>)
        {
            return checked_add_fp<ThrowOnErrorPolicy>(a, b);
        }
        else
        {
            return checked_add<ThrowOnErrorPolicy>(a, b);
        }
    }

    template <typename T>
    static constexpr T sub(T a, T b)
    {
        if constexpr (std::is_floating_point_v<T>)
        {
            return checked_sub_fp<ThrowOnErrorPolicy>(a, b);
        }
        else
        {
            return checked_sub<ThrowOnErrorPolicy>(a, b);
        }
    }

    template <typename T>
    static constexpr T mul(T a, T b)
    {
        if constexpr (std::is_floating_point_v<T>)
        {
            return checked_mul_fp<ThrowOnErrorPolicy>(a, b);
        }
        else
        {
            return checked_mul<ThrowOnErrorPolicy>(a, b);
        }
    }

    template <typename T>
    static constexpr T div(T a, T b)
    {
        if constexpr (std::is_floating_point_v<T>)
        {
            return checked_div_fp<ThrowOnErrorPolicy>(a, b);
        }
        else
        {
            return checked_div<ThrowOnErrorPolicy>(a, b);
        }
    }
};

/**
 * @brief Policy for saturating arithmetic (clamp on overflow)
 * @details Uses CheckedArithmetic's SaturatingPolicy tag internally.
 * Named SaturatingArithmeticPolicy to avoid conflict with the CheckedArithmetic tag type.
 */
struct SaturatingArithmeticPolicy
{
    template <typename T>
    static constexpr T add(T a, T b) noexcept
    {
        if constexpr (std::is_floating_point_v<T>)
        {
            return checked_add_fp<fat_p::SaturatingPolicy>(a, b);
        }
        else
        {
            return checked_add<fat_p::SaturatingPolicy>(a, b);
        }
    }

    template <typename T>
    static constexpr T sub(T a, T b) noexcept
    {
        if constexpr (std::is_floating_point_v<T>)
        {
            return checked_sub_fp<fat_p::SaturatingPolicy>(a, b);
        }
        else
        {
            return checked_sub<fat_p::SaturatingPolicy>(a, b);
        }
    }

    template <typename T>
    static constexpr T mul(T a, T b) noexcept
    {
        if constexpr (std::is_floating_point_v<T>)
        {
            return checked_mul_fp<fat_p::SaturatingPolicy>(a, b);
        }
        else
        {
            return checked_mul<fat_p::SaturatingPolicy>(a, b);
        }
    }

    template <typename T>
    static constexpr T div(T a, T b) noexcept
    {
        if constexpr (std::is_floating_point_v<T>)
        {
            return checked_div_fp<fat_p::SaturatingPolicy>(a, b);
        }
        else
        {
            return checked_div<fat_p::SaturatingPolicy>(a, b);
        }
    }
};

// =============================================================================
// COMPILE-TIME SHAPE UTILITIES
// =============================================================================

namespace tensor_static_detail
{

template <size_t Product>
consteval bool shapeSizeFits()
{
    return true;
}

template <size_t Product, size_t Dim, size_t... RemainingDims>
consteval bool shapeSizeFits()
{
    if constexpr (Dim == 0)
    {
        // Shape emits the existing positive-dimension diagnostic.
        return true;
    }
    else if constexpr (Dim > std::numeric_limits<size_t>::max() / Product)
    {
        return false;
    }
    else
    {
        return shapeSizeFits<Product * Dim, RemainingDims...>();
    }
}

} // namespace tensor_static_detail

/**
 * @brief Compile-time shape representation
 */
template <size_t... Dims>
    requires(tensor_static_detail::shapeSizeFits<1, Dims...>())
struct Shape
{
    static_assert(((Dims > 0) && ...), "StaticTensor dimensions must be greater than zero");
    static constexpr size_t rank = sizeof...(Dims);
    static constexpr std::array<size_t, rank> dims = {Dims...};
    static constexpr size_t size = (Dims * ... * 1);

    // Shape comparison
    template <size_t... OtherDims>
    static constexpr bool equals(Shape<OtherDims...>)
    {
        return std::is_same_v<Shape<Dims...>, Shape<OtherDims...>>;
    }

    // Get dimension at index I
    template <size_t I>
    static constexpr size_t dim()
    {
        static_assert(I < rank, "Dimension index out of bounds");
        return dims[I];
    }
};

// Convenient aliases
template <size_t N>
using Vector = Shape<N>;
template <size_t M, size_t N>
using Matrix = Shape<M, N>;
template <size_t D1, size_t D2, size_t D3>
using Tensor3 = Shape<D1, D2, D3>;
template <size_t D1, size_t D2, size_t D3, size_t D4>
using Tensor4 = Shape<D1, D2, D3, D4>;

// =============================================================================
// CORE TENSOR TYPE
// =============================================================================

/**
 * @brief Fixed-size N-dimensional tensor with compile-time shape
 * @tparam T Element type (float, double, int32_t, etc.)
 * @tparam ShapeT Shape specification (Shape<Dims...>)
 * @tparam Policy Arithmetic policy (UncheckedPolicy, CheckedPolicy, SaturatingArithmeticPolicy)
 */
template <typename T, typename ShapeT, typename Policy = UncheckedPolicy>
class StaticTensor
{
public:
    using value_type = T;
    using shape_type = ShapeT;
    using policy_type = Policy;

    static constexpr size_t rank = ShapeT::rank;
    static constexpr size_t size = ShapeT::size;

    // Storage: stack-allocated array (no heap)
    alignas(32) std::array<T, size> mData;

    // Constructors
    constexpr StaticTensor()
        : mData{}
    {
    }

    constexpr explicit StaticTensor(T scalar)
    {
        mData.fill(scalar);
    }

    constexpr StaticTensor(std::initializer_list<T> init)
    {
        if (init.size() != size)
        {
            throw std::invalid_argument("StaticTensor initializer size does not match its shape");
        }
        std::copy(init.begin(), init.end(), mData.begin());
    }

    template <typename... Args>
        requires(sizeof...(Args) == size && (std::is_convertible_v<Args, T> && ...))
    constexpr explicit StaticTensor(Args... args)
        : mData{static_cast<T>(args)...}
    {
    }

    // Element access
    constexpr T& operator[](size_t idx) noexcept
    {
        return mData[idx];
    }
    constexpr const T& operator[](size_t idx) const noexcept
    {
        return mData[idx];
    }

    // Multi-dimensional indexing
    template <typename... Indices>
    constexpr T& at(Indices... indices)
    {
        static_assert(sizeof...(indices) == rank, "Index count must match tensor rank");
        return mData[compute_offset(std::array<size_t, rank>{static_cast<size_t>(indices)...})];
    }

    template <typename... Indices>
    constexpr const T& at(Indices... indices) const
    {
        static_assert(sizeof...(indices) == rank, "Index count must match tensor rank");
        return mData[compute_offset(std::array<size_t, rank>{static_cast<size_t>(indices)...})];
    }

    // Iterators
    auto begin() noexcept
    {
        return mData.begin();
    }
    auto end() noexcept
    {
        return mData.end();
    }
    auto begin() const noexcept
    {
        return mData.begin();
    }
    auto end() const noexcept
    {
        return mData.end();
    }

    // Raw data access
    T* data() noexcept
    {
        return mData.data();
    }
    const T* data() const noexcept
    {
        return mData.data();
    }

private:
    // Compute linear offset from multi-dimensional indices
    constexpr size_t compute_offset(const std::array<size_t, rank>& indices) const
    {
        size_t offset = 0;
        size_t stride = 1;
        for (size_t i = rank; i > 0; --i)
        {
            size_t dim_idx = i - 1;
            if (indices[dim_idx] >= ShapeT::dims[dim_idx])
            {
                throw std::out_of_range("StaticTensor index is out of range");
            }
            offset += indices[dim_idx] * stride;
            stride *= ShapeT::dims[dim_idx];
        }
        return offset;
    }
};

// =============================================================================
// ELEMENT-WISE OPERATIONS
// =============================================================================

/**
 * @brief Element-wise addition
 */
template <typename T, typename S, typename P>
constexpr StaticTensor<T, S, P> operator+(const StaticTensor<T, S, P>& a, const StaticTensor<T, S, P>& b)
{
    StaticTensor<T, S, P> result;
    for (size_t i = 0; i < S::size; ++i)
    {
        result[i] = P::add(a[i], b[i]);
    }
    return result;
}

/**
 * @brief Element-wise subtraction
 */
template <typename T, typename S, typename P>
constexpr StaticTensor<T, S, P> operator-(const StaticTensor<T, S, P>& a, const StaticTensor<T, S, P>& b)
{
    StaticTensor<T, S, P> result;
    for (size_t i = 0; i < S::size; ++i)
    {
        result[i] = P::sub(a[i], b[i]);
    }
    return result;
}

/**
 * @brief Element-wise multiplication (Hadamard product)
 */
template <typename T, typename S, typename P>
constexpr StaticTensor<T, S, P> operator*(const StaticTensor<T, S, P>& a, const StaticTensor<T, S, P>& b)
{
    StaticTensor<T, S, P> result;
    for (size_t i = 0; i < S::size; ++i)
    {
        result[i] = P::mul(a[i], b[i]);
    }
    return result;
}

/**
 * @brief Element-wise division
 */
template <typename T, typename S, typename P>
constexpr StaticTensor<T, S, P> operator/(const StaticTensor<T, S, P>& a, const StaticTensor<T, S, P>& b)
{
    StaticTensor<T, S, P> result;
    for (size_t i = 0; i < S::size; ++i)
    {
        result[i] = P::div(a[i], b[i]);
    }
    return result;
}

/**
 * @brief Scalar multiplication
 */
template <typename T, typename S, typename P>
constexpr StaticTensor<T, S, P> operator*(const StaticTensor<T, S, P>& tensor, T scalar)
{
    StaticTensor<T, S, P> result;
    for (size_t i = 0; i < S::size; ++i)
    {
        result[i] = P::mul(tensor[i], scalar);
    }
    return result;
}

template <typename T, typename S, typename P>
constexpr StaticTensor<T, S, P> operator*(T scalar, const StaticTensor<T, S, P>& tensor)
{
    return tensor * scalar;
}

/**
 * @brief Scalar division
 */
template <typename T, typename S, typename P>
constexpr StaticTensor<T, S, P> operator/(const StaticTensor<T, S, P>& tensor, T scalar)
{
    StaticTensor<T, S, P> result;
    for (size_t i = 0; i < S::size; ++i)
    {
        result[i] = P::div(tensor[i], scalar);
    }
    return result;
}

/**
 * @brief Unary negation
 */
template <typename T, typename S, typename P>
constexpr StaticTensor<T, S, P> operator-(const StaticTensor<T, S, P>& tensor)
{
    StaticTensor<T, S, P> result;
    for (size_t i = 0; i < S::size; ++i)
    {
        const auto policyResult = P::sub(T{0}, tensor[i]);
        if constexpr (std::is_floating_point_v<T>)
        {
            result[i] = tensor[i] == T{0} ? -tensor[i] : policyResult;
        }
        else
        {
            result[i] = policyResult;
        }
    }
    return result;
}

// =============================================================================
// SIMD OPTIMIZATIONS (AVX2)
// =============================================================================

#ifdef __AVX2__

/**
 * @brief SIMD-optimized element-wise multiplication for float vectors
 * @details Uses AVX2 for 8-wide SIMD (256-bit).
 */
template <size_t N>
inline StaticTensor<float, Vector<N>, UncheckedPolicy>
simd_mul(const StaticTensor<float, Vector<N>, UncheckedPolicy>& a,
         const StaticTensor<float, Vector<N>, UncheckedPolicy>& b)
{
    StaticTensor<float, Vector<N>, UncheckedPolicy> result;

    constexpr size_t simd_width = 8;
    constexpr size_t simd_blocks = N / simd_width;

    for (size_t i = 0; i < simd_blocks; ++i)
    {
        __m256 va = _mm256_load_ps(&a.mData[i * simd_width]);
        __m256 vb = _mm256_load_ps(&b.mData[i * simd_width]);
        __m256 vr = _mm256_mul_ps(va, vb);
        _mm256_store_ps(&result.mData[i * simd_width], vr);
    }

    for (size_t i = simd_blocks * simd_width; i < N; ++i)
    {
        result[i] = a[i] * b[i];
    }

    return result;
}

/**
 * @brief SIMD-optimized dot product for float vectors
 */
template <size_t N>
inline float simd_dot(const StaticTensor<float, Vector<N>, UncheckedPolicy>& a,
                      const StaticTensor<float, Vector<N>, UncheckedPolicy>& b)
{
    constexpr size_t simd_width = 8;
    constexpr size_t simd_blocks = N / simd_width;

    __m256 sum_vec = _mm256_setzero_ps();

    for (size_t i = 0; i < simd_blocks; ++i)
    {
        __m256 va = _mm256_load_ps(&a.mData[i * simd_width]);
        __m256 vb = _mm256_load_ps(&b.mData[i * simd_width]);
        __m256 prod = _mm256_mul_ps(va, vb);
        sum_vec = _mm256_add_ps(sum_vec, prod);
    }

    // Horizontal sum
    alignas(32) float temp[8];
    _mm256_store_ps(temp, sum_vec);
    float sum = temp[0] + temp[1] + temp[2] + temp[3] + temp[4] + temp[5] + temp[6] + temp[7];

    // Scalar tail
    for (size_t i = simd_blocks * simd_width; i < N; ++i)
    {
        sum += a[i] * b[i];
    }

    return sum;
}

#endif // __AVX2__

// =============================================================================
// LINEAR ALGEBRA OPERATIONS
// =============================================================================

/**
 * @brief Dot product (inner product) for vectors
 */
template <typename T, size_t N, typename P>
constexpr T dot(const StaticTensor<T, Vector<N>, P>& a, const StaticTensor<T, Vector<N>, P>& b)
{
    T result = T{0};
    for (size_t i = 0; i < N; ++i)
    {
        result = P::add(result, P::mul(a[i], b[i]));
    }
    return result;
}

/**
 * @brief Matrix-vector multiplication
 * @details (MxN) @ (Nx1) -> (Mx1)
 */
template <typename T, size_t M, size_t N, typename P>
constexpr StaticTensor<T, Vector<M>, P> matvec(const StaticTensor<T, Matrix<M, N>, P>& mat,
                                               const StaticTensor<T, Vector<N>, P>& vec)
{
    StaticTensor<T, Vector<M>, P> result;
    for (size_t i = 0; i < M; ++i)
    {
        T sum = T{0};
        for (size_t j = 0; j < N; ++j)
        {
            sum = P::add(sum, P::mul(mat.at(i, j), vec[j]));
        }
        result[i] = sum;
    }
    return result;
}

/**
 * @brief Matrix-matrix multiplication
 * @details (MxK) @ (KxN) -> (MxN)
 */
template <typename T, size_t M, size_t K, size_t N, typename P>
constexpr StaticTensor<T, Matrix<M, N>, P> matmul(const StaticTensor<T, Matrix<M, K>, P>& a,
                                                  const StaticTensor<T, Matrix<K, N>, P>& b)
{
    StaticTensor<T, Matrix<M, N>, P> result;
    for (size_t i = 0; i < M; ++i)
    {
        for (size_t j = 0; j < N; ++j)
        {
            T sum = T{0};
            for (size_t k = 0; k < K; ++k)
            {
                sum = P::add(sum, P::mul(a.at(i, k), b.at(k, j)));
            }
            result.at(i, j) = sum;
        }
    }
    return result;
}

/**
 * @brief Transpose a matrix
 */
template <typename T, size_t M, size_t N, typename P>
constexpr StaticTensor<T, Matrix<N, M>, P> transpose(const StaticTensor<T, Matrix<M, N>, P>& mat)
{
    StaticTensor<T, Matrix<N, M>, P> result;
    for (size_t i = 0; i < M; ++i)
    {
        for (size_t j = 0; j < N; ++j)
        {
            result.at(j, i) = mat.at(i, j);
        }
    }
    return result;
}

/**
 * @brief Outer product (tensor product) of two vectors
 * @details (Mx1) otimes (Nx1) -> (MxN)
 */
template <typename T, size_t M, size_t N, typename P>
constexpr StaticTensor<T, Matrix<M, N>, P> outer(const StaticTensor<T, Vector<M>, P>& a,
                                                 const StaticTensor<T, Vector<N>, P>& b)
{
    StaticTensor<T, Matrix<M, N>, P> result;
    for (size_t i = 0; i < M; ++i)
    {
        for (size_t j = 0; j < N; ++j)
        {
            result.at(i, j) = P::mul(a[i], b[j]);
        }
    }
    return result;
}

// =============================================================================
// REDUCTION OPERATIONS
// =============================================================================

/** @brief Sum accumulator: bool counts in size_t and narrow integers widen to 64 bits. */
template <typename T>
using StaticTensorSumType = std::conditional_t<
    std::is_same_v<std::remove_cv_t<T>, bool>, std::size_t,
    std::conditional_t<std::is_integral_v<T> && (sizeof(T) < sizeof(std::int64_t)),
                       std::conditional_t<std::is_signed_v<T>, std::int64_t, std::uint64_t>, T>>;

/** @brief Mean accumulator/result for arithmetic values: double, except long double stays long double. */
template <typename T>
using StaticTensorMeanType = std::conditional_t<
    std::is_arithmetic_v<std::remove_cv_t<T>>,
    std::conditional_t<std::is_same_v<std::remove_cv_t<T>, long double>, long double, double>, T>;

/**
 * @brief Sum of all elements
 */
template <typename T, typename S, typename P>
constexpr StaticTensorSumType<T> sum(const StaticTensor<T, S, P>& tensor)
{
    using result_type = StaticTensorSumType<T>;
    result_type result = result_type{0};
    for (size_t i = 0; i < S::size; ++i)
    {
        result = P::add(result, static_cast<result_type>(tensor[i]));
    }
    return result;
}

/**
 * @brief Mean of all elements
 */
template <typename T, typename S, typename P>
constexpr StaticTensorMeanType<T> mean(const StaticTensor<T, S, P>& tensor)
{
    using result_type = StaticTensorMeanType<T>;
    result_type result = result_type{0};
    for (size_t i = 0; i < S::size; ++i)
    {
        result = P::add(result, static_cast<result_type>(tensor[i]));
    }
    return P::div(result, static_cast<result_type>(S::size));
}

/**
 * @brief Maximum element
 */
template <typename T, typename S, typename P>
constexpr T max(const StaticTensor<T, S, P>& tensor)
{
    T result = tensor[0];
    for (size_t i = 1; i < S::size; ++i)
    {
        if constexpr (std::is_floating_point_v<T>)
        {
            if (result != result)
            {
                return result;
            }
            if (tensor[i] != tensor[i])
            {
                return tensor[i];
            }
        }
        if (result < tensor[i])
        {
            result = tensor[i];
        }
    }
    return result;
}

/**
 * @brief Minimum element
 */
template <typename T, typename S, typename P>
constexpr T min(const StaticTensor<T, S, P>& tensor)
{
    T result = tensor[0];
    for (size_t i = 1; i < S::size; ++i)
    {
        if constexpr (std::is_floating_point_v<T>)
        {
            if (result != result)
            {
                return result;
            }
            if (tensor[i] != tensor[i])
            {
                return tensor[i];
            }
        }
        if (tensor[i] < result)
        {
            result = tensor[i];
        }
    }
    return result;
}

/**
 * @brief L2 norm (Euclidean norm)
 */
template <typename T, size_t N, typename P>
    requires std::is_floating_point_v<T>
T norm(const StaticTensor<T, Vector<N>, P>& vec)
{
    T scale = T{0};
    T scaledSum = T{1};
    bool hasNonzeroFiniteValue = false;
    bool hasInfinity = false;
    for (size_t i = 0; i < N; ++i)
    {
        if (std::isnan(vec[i]))
        {
            return std::sqrt(P::mul(vec[i], vec[i]));
        }
        if (std::isinf(vec[i]))
        {
            hasInfinity = true;
            continue;
        }

        const T magnitude = std::abs(vec[i]);
        if (magnitude == T{0})
        {
            continue;
        }
        if (!hasNonzeroFiniteValue)
        {
            scale = magnitude;
            scaledSum = T{1};
            hasNonzeroFiniteValue = true;
        }
        else if (scale < magnitude)
        {
            const T ratio = P::div(scale, magnitude);
            scaledSum = P::add(T{1}, P::mul(scaledSum, P::mul(ratio, ratio)));
            scale = magnitude;
        }
        else
        {
            const T ratio = P::div(magnitude, scale);
            scaledSum = P::add(scaledSum, P::mul(ratio, ratio));
        }
    }
    if (hasInfinity)
    {
        const T infinity = std::numeric_limits<T>::infinity();
        return std::sqrt(P::mul(infinity, infinity));
    }
    if (!hasNonzeroFiniteValue)
    {
        return T{0};
    }
    return P::mul(scale, static_cast<T>(std::sqrt(scaledSum)));
}

/**
 * @brief Normalize vector to unit length
 */
template <typename T, size_t N, typename P>
    requires std::is_floating_point_v<T>
StaticTensor<T, Vector<N>, P> normalize(const StaticTensor<T, Vector<N>, P>& vec)
{
    T n = norm(vec);
    if (n == T{0})
    {
        throw std::domain_error("Cannot normalize a zero-length StaticTensor vector");
    }
    StaticTensor<T, Vector<N>, P> result;
    for (size_t i = 0; i < N; ++i)
    {
        result[i] = P::div(vec[i], n);
    }
    return result;
}

// =============================================================================
// CONVENIENT TYPE ALIASES
// =============================================================================

// Common vector types
template <typename T, typename P = UncheckedPolicy>
using Vec2 = StaticTensor<T, Vector<2>, P>;
template <typename T, typename P = UncheckedPolicy>
using Vec3 = StaticTensor<T, Vector<3>, P>;
template <typename T, typename P = UncheckedPolicy>
using Vec4 = StaticTensor<T, Vector<4>, P>;

// Common matrix types
template <typename T, typename P = UncheckedPolicy>
using Mat2x2 = StaticTensor<T, Matrix<2, 2>, P>;
template <typename T, typename P = UncheckedPolicy>
using Mat3x3 = StaticTensor<T, Matrix<3, 3>, P>;
template <typename T, typename P = UncheckedPolicy>
using Mat4x4 = StaticTensor<T, Matrix<4, 4>, P>;

// Float specializations
using Vec2f = Vec2<float>;
using Vec3f = Vec3<float>;
using Vec4f = Vec4<float>;
using Mat2x2f = Mat2x2<float>;
using Mat3x3f = Mat3x3<float>;
using Mat4x4f = Mat4x4<float>;

// Double specializations
using Vec2d = Vec2<double>;
using Vec3d = Vec3<double>;
using Vec4d = Vec4<double>;
using Mat2x2d = Mat2x2<double>;
using Mat3x3d = Mat3x3<double>;
using Mat4x4d = Mat4x4<double>;

// Integer specializations
using Vec2i = Vec2<int32_t>;
using Vec3i = Vec3<int32_t>;
using Vec4i = Vec4<int32_t>;

// Checked variants
template <typename T>
using Vec2c = Vec2<T, CheckedPolicy>;
template <typename T>
using Vec3c = Vec3<T, CheckedPolicy>;
template <typename T>
using Vec4c = Vec4<T, CheckedPolicy>;
template <typename T>
using Mat2x2c = Mat2x2<T, CheckedPolicy>;
template <typename T>
using Mat3x3c = Mat3x3<T, CheckedPolicy>;
template <typename T>
using Mat4x4c = Mat4x4<T, CheckedPolicy>;

// Saturating variants
template <typename T>
using Vec2s = Vec2<T, SaturatingArithmeticPolicy>;
template <typename T>
using Vec3s = Vec3<T, SaturatingArithmeticPolicy>;
template <typename T>
using Vec4s = Vec4<T, SaturatingArithmeticPolicy>;
template <typename T>
using Mat2x2s = Mat2x2<T, SaturatingArithmeticPolicy>;
template <typename T>
using Mat3x3s = Mat3x3<T, SaturatingArithmeticPolicy>;
template <typename T>
using Mat4x4s = Mat4x4<T, SaturatingArithmeticPolicy>;

} // namespace fat_p
