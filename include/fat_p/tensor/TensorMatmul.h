#pragma once

/*
FATP_META:
  meta_version: 1
  component: TensorMatmul
  file_role: internal_header
  path: include/fat_p/tensor/TensorMatmul.h
  namespace: fat_p
  layer: Domain
  summary: "Native serial vector, matrix, and batched Tensor multiplication."
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

/** @file TensorMatmul.h @brief Dependency-free deterministic Tensor matmul kernels. */

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
    if (result.empty())
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

template <ReadableTensor Left, ReadableTensor Right, typename Allocator>
    requires SameTensorValue<Left, Right> && std::is_arithmetic_v<typename Left::value_type>
[[nodiscard]] auto matmul(const Left& left, const Right& right, const Allocator& allocator)
    -> Tensor<TensorMatmulType<typename Left::value_type>, Allocator>
{
    using result_type = TensorMatmulType<typename Left::value_type>;
    tensor_detail::TensorAccess::validate(left);
    tensor_detail::TensorAccess::validate(right);
    const auto shape = tensor_detail::makeMatmulShape(left.layout(), right.layout());
    if (left.rank() == 2 && right.rank() == 2 && left.layout().isContiguous() &&
        right.layout().isContiguous())
    {
        return tensor_detail::matmulContiguousMatrices<result_type>(left, right, shape, allocator);
    }
    return tensor_detail::matmulGeneric<result_type>(left, right, shape, allocator);
}

template <ReadableTensor Left, ReadableTensor Right>
    requires SameTensorValue<Left, Right> && std::is_arithmetic_v<typename Left::value_type>
[[nodiscard]] auto matmul(const Left& left, const Right& right)
{
    using result_type = TensorMatmulType<typename Left::value_type>;
    if constexpr (requires { left.get_allocator(); })
    {
        return matmul(left, right, tensor_detail::selectResultAllocator<result_type>(left));
    }
    else if constexpr (requires { right.get_allocator(); })
    {
        return matmul(left, right, tensor_detail::selectResultAllocator<result_type>(right));
    }
    else
    {
        return matmul(left, right, TensorAllocator<result_type>{});
    }
}

} // namespace fat_p
