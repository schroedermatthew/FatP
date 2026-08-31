#pragma once

/*
FATP_META:
  meta_version: 1
  component: TensorMatmul
  file_role: internal_header
  path: include/fat_p/tensor/TensorMatmul.h
  namespace: fat_p
  layer: Domain
  summary: "Named Tensor linear algebra with checked products, diagonal extraction, and trace."
  api_stability: in_work
  related:
    docs:
      - components/Tensor/docs/Design Note - Tensor Architecture Additions Plan.md
    headers:
      - include/fat_p/TensorMatmul.h
      - include/fat_p/tensor/TensorReductions.h
    tests:
      - components/Tensor/tests/test_TensorMatmul.cpp
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: codex
    mode: manual
*/

/** @file TensorMatmul.h @brief Dependency-free named Tensor linear algebra. */

#include "TensorReductions.h"

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace fat_p
{

template <typename T>
using TensorMatmulType = TensorSumType<T>;

namespace tensor_detail
{

template <ReadableTensor Source>
[[nodiscard]] auto diagonalReadMapping(const Source& source)
{
    TensorAccess::validate(source);
    if (source.rank() < 2)
    {
        throw std::invalid_argument("diagonal and trace require rank two or greater");
    }
    const auto rowAxis = source.rank() - 2;
    const auto length = std::min(source.extents()[rowAxis], source.extents()[rowAxis + 1]);
    auto extents = source.extents().values();
    extents.pop_back();
    extents.back() = length;
    auto strides = source.strides();
    strides.pop_back();
    // No diagonal transition exists in an empty mapping or a singleton diagonal.
    // Such layouts may legally contain otherwise unrepresentable stride sums.
    strides.back() = !source.empty() && length > 1
                         ? checkedOffsetAdd(source.strides()[rowAxis], source.strides()[rowAxis + 1])
                         : 0;
    TensorLayout layout(source.layout().storageLength(), source.layout().originOffset(),
                        DynamicExtents(std::move(extents)), std::move(strides));
    // These const mappings are consumed synchronously, never exposed as escaping views.
    return TensorAccess::makeView(TensorAccess::storageBase(source), std::move(layout),
                                  TensorAccess::lifetime(source), TensorAccess::tracked(source));
}

template <ReadableTensor Source>
[[nodiscard]] auto outerReadMapping(const Source& source, bool column)
{
    const auto length = source.extents()[0];
    const auto stride = source.strides()[0];
    TensorLayout layout(source.layout().storageLength(), source.layout().originOffset(),
                        column ? DynamicExtents{length, 1} : DynamicExtents{1, length},
                        column ? TensorStrides{stride, 0} : TensorStrides{0, stride});
    return TensorAccess::makeView(TensorAccess::storageBase(source), std::move(layout),
                                  TensorAccess::lifetime(source), TensorAccess::tracked(source));
}

struct MatmulShape
{
    DynamicExtents outputExtents;
    std::vector<std::size_t> batchExtents;
    TensorStrides leftBatchStrides;
    TensorStrides rightBatchStrides;
    std::size_t rows = 1;
    std::size_t inner = 0;
    std::size_t columns = 1;
    bool leftVector = false;
    bool rightVector = false;
};

inline MatmulShape makeMatmulShape(const TensorLayout& left, const TensorLayout& right)
{
    if (left.rank() == 0 || right.rank() == 0)
    {
        throw std::invalid_argument("matmul operands must have rank one or greater");
    }
    const bool leftVector = left.rank() == 1;
    const bool rightVector = right.rank() == 1;
    const auto leftBatchRank = leftVector ? std::size_t{0} : left.rank() - 2;
    const auto rightBatchRank = rightVector ? std::size_t{0} : right.rank() - 2;
    const auto batchRank = std::max(leftBatchRank, rightBatchRank);

    const auto leftInner = leftVector ? left.extents()[0] : left.extents()[left.rank() - 1];
    const auto rightInner = rightVector ? right.extents()[0] : right.extents()[right.rank() - 2];
    if (leftInner != rightInner)
    {
        throw std::invalid_argument("matmul inner dimensions must match");
    }

    std::vector<std::size_t> batchExtents(batchRank, 1);
    TensorStrides leftBatchStrides(batchRank, 0);
    TensorStrides rightBatchStrides(batchRank, 0);
    for (std::size_t axis = 0; axis < batchRank; ++axis)
    {
        const auto leftPadding = batchRank - leftBatchRank;
        const auto rightPadding = batchRank - rightBatchRank;
        const auto leftExtent = axis < leftPadding ? std::size_t{1} : left.extents()[axis - leftPadding];
        const auto rightExtent = axis < rightPadding ? std::size_t{1} : right.extents()[axis - rightPadding];
        if (leftExtent != rightExtent && leftExtent != 1 && rightExtent != 1)
        {
            throw std::invalid_argument("matmul batch dimensions are not broadcast-compatible");
        }
        batchExtents[axis] = leftExtent == 1 ? rightExtent : leftExtent;
        if (axis >= leftPadding && !(leftExtent == 1 && batchExtents[axis] != 1))
        {
            leftBatchStrides[axis] = left.strides()[axis - leftPadding];
        }
        if (axis >= rightPadding && !(rightExtent == 1 && batchExtents[axis] != 1))
        {
            rightBatchStrides[axis] = right.strides()[axis - rightPadding];
        }
    }

    std::vector<std::size_t> output = batchExtents;
    const auto rows = leftVector ? std::size_t{1} : left.extents()[left.rank() - 2];
    const auto columns = rightVector ? std::size_t{1} : right.extents()[right.rank() - 1];
    if (!leftVector)
    {
        output.push_back(rows);
    }
    if (!rightVector)
    {
        output.push_back(columns);
    }
    return {DynamicExtents(std::move(output)), std::move(batchExtents), std::move(leftBatchStrides),
            std::move(rightBatchStrides), rows, leftInner, columns, leftVector, rightVector};
}

inline std::pair<std::ptrdiff_t, std::ptrdiff_t> matmulBatchOrigins(std::size_t batchLinear,
                                                                  const MatmulShape& shape,
                                                                  const TensorLayout& left,
                                                                  const TensorLayout& right)
{
    auto leftOffset = left.originOffset();
    auto rightOffset = right.originOffset();
    auto remainder = batchLinear;
    for (std::size_t reverseAxis = shape.batchExtents.size(); reverseAxis > 0; --reverseAxis)
    {
        const auto axis = reverseAxis - 1;
        const auto coordinate = remainder % shape.batchExtents[axis];
        remainder /= shape.batchExtents[axis];
        leftOffset = checkedOffsetAdd(leftOffset,
                                      checkedStrideContribution(coordinate, shape.leftBatchStrides[axis]));
        rightOffset = checkedOffsetAdd(rightOffset,
                                       checkedStrideContribution(coordinate, shape.rightBatchStrides[axis]));
    }
    return {leftOffset, rightOffset};
}

template <typename Result, ReadableTensor Left, ReadableTensor Right, typename Allocator>
[[nodiscard]] Tensor<Result, Allocator> matmulGeneric(const Left& left, const Right& right,
                                                     const MatmulShape& shape, const Allocator& allocator)
{
    Tensor<Result, Allocator> result(std::allocator_arg, allocator, shape.outputExtents, Result{0});
    if (result.empty() || shape.inner == 0)
    {
        return result;
    }
    const auto* leftData = TensorAccess::storageBase(left);
    const auto* rightData = TensorAccess::storageBase(right);
    const auto batchCount = checkedLogicalSize(shape.batchExtents);
    std::size_t outputLinear = 0;
    for (std::size_t batch = 0; batch < batchCount; ++batch)
    {
        const auto [leftBatch, rightBatch] = matmulBatchOrigins(batch, shape, left.layout(), right.layout());
        for (std::size_t row = 0; row < shape.rows; ++row)
        {
            auto leftRow = leftBatch;
            if (!shape.leftVector)
            {
                leftRow = checkedOffsetAdd(
                    leftRow, checkedStrideContribution(row, left.strides()[left.rank() - 2]));
            }
            for (std::size_t column = 0; column < shape.columns; ++column)
            {
                auto rightColumn = rightBatch;
                if (!shape.rightVector)
                {
                    rightColumn = checkedOffsetAdd(
                        rightColumn,
                        checkedStrideContribution(column, right.strides()[right.rank() - 1]));
                }
                Result total = 0;
                for (std::size_t inner = 0; inner < shape.inner; ++inner)
                {
                    auto leftOffset = leftRow;
                    auto rightOffset = rightColumn;
                    if (shape.leftVector)
                    {
                        leftOffset = checkedOffsetAdd(
                            leftOffset, checkedStrideContribution(inner, left.strides()[0]));
                    }
                    else
                    {
                        leftOffset = checkedOffsetAdd(
                            leftOffset, checkedStrideContribution(inner, left.strides()[left.rank() - 1]));
                    }
                    if (shape.rightVector)
                    {
                        rightOffset = checkedOffsetAdd(
                            rightOffset, checkedStrideContribution(inner, right.strides()[0]));
                    }
                    else
                    {
                        rightOffset = checkedOffsetAdd(
                            rightOffset, checkedStrideContribution(inner, right.strides()[right.rank() - 2]));
                    }
                    const auto term = checkedSameTypeMultiply(static_cast<Result>(leftData[leftOffset]),
                                                              static_cast<Result>(rightData[rightOffset]));
                    total = checkedSameTypeAdd(total, term);
                }
                result[outputLinear++] = total;
            }
        }
    }
    return result;
}

template <typename Result, ReadableTensor Left, ReadableTensor Right, typename Allocator>
[[nodiscard]] Tensor<Result, Allocator> matmulContiguousMatrices(const Left& left, const Right& right,
                                                                const MatmulShape& shape,
                                                                const Allocator& allocator)
{
    Tensor<Result, Allocator> result(std::allocator_arg, allocator, shape.outputExtents, Result{0});
    if (result.empty() || shape.inner == 0)
    {
        return result;
    }
    const auto* leftData = TensorAccess::storageBase(left) + left.layout().originOffset();
    const auto* rightData = TensorAccess::storageBase(right) + right.layout().originOffset();
    constexpr std::size_t block = 32;
    for (std::size_t rowBlock = 0; rowBlock < shape.rows; rowBlock += block)
    {
        for (std::size_t innerBlock = 0; innerBlock < shape.inner; innerBlock += block)
        {
            for (std::size_t columnBlock = 0; columnBlock < shape.columns; columnBlock += block)
            {
                const auto rowEnd = std::min(rowBlock + block, shape.rows);
                const auto innerEnd = std::min(innerBlock + block, shape.inner);
                const auto columnEnd = std::min(columnBlock + block, shape.columns);
                for (std::size_t row = rowBlock; row < rowEnd; ++row)
                {
                    for (std::size_t inner = innerBlock; inner < innerEnd; ++inner)
                    {
                        const Result leftValue = static_cast<Result>(leftData[row * shape.inner + inner]);
                        for (std::size_t column = columnBlock; column < columnEnd; ++column)
                        {
                            auto& output = result[row * shape.columns + column];
                            const auto term = checkedSameTypeMultiply(
                                leftValue, static_cast<Result>(rightData[inner * shape.columns + column]));
                            output = checkedSameTypeAdd(output, term);
                        }
                    }
                }
            }
        }
    }
    return result;
}

} // namespace tensor_detail

/**
 * @brief Multiply vectors, matrices, or broadcasted batches into a new owner.
 * @param left Left arithmetic operand of rank one or greater.
 * @param right Same-element-type right operand with matching contraction extent.
 * @param allocator Exact result allocator for TensorMatmulType (TensorSumType).
 * @return Canonical owner; two vectors produce rank zero. Empty contractions sum to zero.
 * @throws std::invalid_argument If ranks, inner dimensions, or batch dimensions disagree.
 * @throws std::overflow_error If integral products, sums, or output layout overflow.
 * @throws std::bad_alloc If element or metadata allocation fails.
 * @throws std::runtime_error For expired borrowed sources in assertions-enabled builds.
 */
template <ReadableTensor Left, ReadableTensor Right, typename Allocator>
    requires SameTensorValue<Left, Right> && std::is_arithmetic_v<typename Left::value_type> &&
             tensor_detail::AllocatorFor<Allocator, TensorMatmulType<typename Left::value_type>>
[[nodiscard]] auto matmul(const Left& left, const Right& right, const Allocator& allocator)
    -> Tensor<TensorMatmulType<typename Left::value_type>, Allocator>
{
    using result_type = TensorMatmulType<typename Left::value_type>;
    tensor_detail::TensorAccess::validate(left);
    tensor_detail::TensorAccess::validate(right);
    const auto shape = tensor_detail::makeMatmulShape(left.layout(), right.layout());
    // A contiguous vector pair is the same 1 x K by K x 1 traversal with a scalar result.
    // Mixed vector/matrix and batched forms retain the generic signed-stride path.
    const bool contiguousKernelShape = (left.rank() == 2 && right.rank() == 2) ||
                                       (left.rank() == 1 && right.rank() == 1);
    if (contiguousKernelShape && left.layout().isContiguous() && right.layout().isContiguous())
    {
        return tensor_detail::matmulContiguousMatrices<result_type>(left, right, shape, allocator);
    }
    return tensor_detail::matmulGeneric<result_type>(left, right, shape, allocator);
}

/** @brief Matmul with rebound first-owner SOCCC, or TensorAllocator for view-only inputs. */
template <ReadableTensor Left, ReadableTensor Right>
    requires SameTensorValue<Left, Right> && std::is_arithmetic_v<typename Left::value_type>
[[nodiscard]] auto matmul(const Left& left, const Right& right)
{
    using result_type = TensorMatmulType<typename Left::value_type>;
    return matmul(left, right, tensor_detail::selectBinaryResultAllocator<result_type>(left, right));
}

/**
 * @brief Compute a checked vector dot product into a rank-zero owner.
 * @param left Rank-one arithmetic operand.
 * @param right Rank-one operand with the same element type and length.
 * @param allocator Exact allocator for the TensorMatmulType result.
 * @return Sum of widened products, starting at zero; an empty dot product is zero.
 * @throws std::invalid_argument If either rank is not one or lengths differ.
 * @throws std::overflow_error If an integral product or intermediate sum overflows.
 * @throws std::bad_alloc If element or metadata allocation fails.
 * @throws std::runtime_error For expired borrowed sources in assertions-enabled builds.
 */
template <ReadableTensor Left, ReadableTensor Right, typename Allocator>
    requires SameTensorValue<Left, Right> && std::is_arithmetic_v<typename Left::value_type> &&
             tensor_detail::AllocatorFor<Allocator, TensorMatmulType<typename Left::value_type>>
[[nodiscard]] auto dot(const Left& left, const Right& right, const Allocator& allocator)
{
    tensor_detail::TensorAccess::validate(left);
    tensor_detail::TensorAccess::validate(right);
    if (left.rank() != 1 || right.rank() != 1)
    {
        throw std::invalid_argument("dot requires two rank-one operands");
    }
    if (left.extents()[0] != right.extents()[0])
    {
        throw std::invalid_argument("dot vector lengths must match");
    }
    return matmul(left, right, allocator);
}

/** @brief Dot product with rebound first-owner SOCCC, or TensorAllocator for views. */
template <ReadableTensor Left, ReadableTensor Right>
    requires SameTensorValue<Left, Right> && std::is_arithmetic_v<typename Left::value_type>
[[nodiscard]] auto dot(const Left& left, const Right& right)
{
    using result_type = TensorMatmulType<typename Left::value_type>;
    return dot(left, right, tensor_detail::selectBinaryResultAllocator<result_type>(left, right));
}

/**
 * @brief Compute pairwise vector products into a new rank-two owner.
 * @param left Rank-one arithmetic operand of length M.
 * @param right Same-element-type rank-one operand of length N.
 * @param allocator Exact allocator for the TensorMatmulType result.
 * @return Shape {M,N}, with operands widened before each product and no additive fold.
 * @throws std::invalid_argument If either operand is not rank one.
 * @throws std::overflow_error If an integral product or output layout overflows.
 * @throws std::bad_alloc If element or metadata allocation fails.
 * @throws std::runtime_error For expired borrowed sources in assertions-enabled builds.
 */
template <ReadableTensor Left, ReadableTensor Right, typename Allocator>
    requires SameTensorValue<Left, Right> && std::is_arithmetic_v<typename Left::value_type> &&
             tensor_detail::AllocatorFor<Allocator, TensorMatmulType<typename Left::value_type>>
[[nodiscard]] auto outer(const Left& left, const Right& right, const Allocator& allocator)
{
    using result_type = TensorMatmulType<typename Left::value_type>;
    tensor_detail::TensorAccess::validate(left);
    tensor_detail::TensorAccess::validate(right);
    if (left.rank() != 1 || right.rank() != 1)
    {
        throw std::invalid_argument("outer requires two rank-one operands");
    }
    const auto column = tensor_detail::outerReadMapping(left, true);
    const auto row = tensor_detail::outerReadMapping(right, false);
    Tensor<result_type, Allocator> result(std::allocator_arg, allocator,
                                         DynamicExtents{left.extents()[0], right.extents()[0]});
    tensor_detail::binaryKernel(column, row, result, [](const auto& a, const auto& b) {
        return tensor_detail::checkedSameTypeMultiply(static_cast<result_type>(a), static_cast<result_type>(b));
    });
    return result;
}

/** @brief Outer product with rebound first-owner SOCCC, or TensorAllocator for views. */
template <ReadableTensor Left, ReadableTensor Right>
    requires SameTensorValue<Left, Right> && std::is_arithmetic_v<typename Left::value_type>
[[nodiscard]] auto outer(const Left& left, const Right& right)
{
    using result_type = TensorMatmulType<typename Left::value_type>;
    return outer(left, right, tensor_detail::selectBinaryResultAllocator<result_type>(left, right));
}

/**
 * @brief Copy the main diagonal of the final two axes into an independent owner.
 * @param source Rank-two or batched source with default-initializable, copy-assignable elements.
 * @param allocator Exact allocator for the unchanged element type.
 * @return Shape {...,min(M,N)} for source shape {...,M,N}; no source storage is aliased.
 * @throws std::invalid_argument If source rank is less than two.
 * @throws std::overflow_error If output layout arithmetic is not representable.
 * @throws std::bad_alloc If element or metadata allocation fails.
 * @throws std::runtime_error For expired borrowed sources in assertions-enabled builds.
 * @details Element construction or copying exceptions propagate without modifying the source.
 */
template <tensor_detail::CopyMaterializableTensor Source, typename Allocator>
    requires tensor_detail::AllocatorFor<Allocator, typename Source::value_type>
[[nodiscard]] auto diagonal(const Source& source, const Allocator& allocator)
{
    return clone(tensor_detail::diagonalReadMapping(source), allocator);
}

/** @brief Diagonal copy with owner SOCCC, or TensorAllocator for a view input. */
template <tensor_detail::CopyMaterializableTensor Source>
[[nodiscard]] auto diagonal(const Source& source)
{
    return diagonal(source, tensor_detail::selectResultAllocator<typename Source::value_type>(source));
}

/**
 * @brief Sum the main diagonal of the final two axes, retaining all batch axes.
 * @param source Rank-two or batched arithmetic source, including rectangular matrices.
 * @param allocator Exact allocator for the TensorSumType result.
 * @return Shape {...} for source shape {...,M,N}; rank two gives rank zero. Folds from positive zero.
 * @throws std::invalid_argument If source rank is less than two.
 * @throws std::overflow_error If an integral intermediate sum or output layout overflows.
 * @throws std::bad_alloc If element or metadata allocation fails.
 * @throws std::runtime_error For expired borrowed sources in assertions-enabled builds.
 */
template <ReadableTensor Source, typename Allocator>
    requires std::is_arithmetic_v<typename Source::value_type> &&
             tensor_detail::AllocatorFor<Allocator, TensorSumType<typename Source::value_type>>
[[nodiscard]] auto trace(const Source& source, const Allocator& allocator)
{
    return sum(tensor_detail::diagonalReadMapping(source), allocator, {TensorAxis{-1}});
}

/** @brief Trace with rebound owner SOCCC, or TensorAllocator for a view input. */
template <ReadableTensor Source>
    requires std::is_arithmetic_v<typename Source::value_type>
[[nodiscard]] auto trace(const Source& source)
{
    return trace(source, tensor_detail::selectResultAllocator<TensorSumType<typename Source::value_type>>(source));
}

} // namespace fat_p
