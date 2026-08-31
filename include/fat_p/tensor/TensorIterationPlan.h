#pragma once

/*
FATP_META:
  meta_version: 1
  component: TensorAlgorithms
  file_role: internal_header
  path: include/fat_p/tensor/TensorIterationPlan.h
  namespace: fat_p::tensor_detail
  layer: Domain
  summary: "Reusable signed-offset iteration plan for one or more Tensor layouts."
  api_stability: in_work
  related:
    docs:
      - components/Tensor/docs/Design Note - Tensor Architecture Additions Plan.md
    headers:
      - include/fat_p/tensor/TensorLayout.h
      - include/fat_p/tensor/TensorKernels.h
    tests:
      - components/Tensor/tests/test_TensorAlgorithms.cpp
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

/**
 * @file TensorIterationPlan.h
 * @brief Rank-normalized, coalesced, counted traversal over validated layouts.
 */

#include "TensorLayout.h"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <stdexcept>
#include <utility>
#include <vector>

namespace fat_p::tensor_detail
{

class TensorIterationPlan
{
public:
    TensorIterationPlan(DynamicExtents iterationExtents,
                        std::initializer_list<std::reference_wrapper<const TensorLayout>> operands)
        : mExtents(std::move(iterationExtents))
        , mLogicalSize(mExtents.logicalSize())
    {
        if (operands.size() == 0)
        {
            throw std::invalid_argument("TensorIterationPlan requires at least one operand layout");
        }
        mOrigins.reserve(operands.size());
        mStrides.reserve(operands.size());
        for (const auto operandReference : operands)
        {
            const auto& operand = operandReference.get();
            mOrigins.push_back(operand.originOffset());
            mStrides.push_back(normalizeStrides(operand));
        }
        coalesceAxes();
    }

    [[nodiscard]] const DynamicExtents& extents() const noexcept { return mExtents; }
    [[nodiscard]] std::size_t logicalSize() const noexcept { return mLogicalSize; }
    [[nodiscard]] std::size_t rank() const noexcept { return mExtents.rank(); }
    [[nodiscard]] std::size_t operandCount() const noexcept { return mStrides.size(); }

    template <typename Function>
    void forEachOffset(Function&& function) const
    {
        if (mLogicalSize == 0)
        {
            return;
        }

        // Complete all traversal allocations before the first callback; compound commit relies on this.
        std::vector<std::size_t> coordinates(rank(), 0);
        std::vector<std::ptrdiff_t> offsets = mOrigins;
        for (std::size_t linearIndex = 0; linearIndex < mLogicalSize; ++linearIndex)
        {
            std::invoke(function, linearIndex, offsets);
            if (linearIndex + 1 == mLogicalSize || rank() == 0)
            {
                continue;
            }

            for (std::size_t reverseAxis = rank(); reverseAxis > 0; --reverseAxis)
            {
                const auto axis = reverseAxis - 1;
                ++coordinates[axis];
                if (coordinates[axis] < mExtents[axis])
                {
                    for (std::size_t operand = 0; operand < mStrides.size(); ++operand)
                    {
                        offsets[operand] = checkedOffsetAdd(offsets[operand], mStrides[operand][axis]);
                    }
                    break;
                }

                coordinates[axis] = 0;
                for (std::size_t operand = 0; operand < mStrides.size(); ++operand)
                {
                    const auto rewind = checkedStrideContribution(mExtents[axis] - 1, mStrides[operand][axis]);
                    offsets[operand] = checkedOffsetSubtract(offsets[operand], rewind);
                }
            }
        }
    }

    [[nodiscard]] static DynamicExtents
    broadcastExtents(std::initializer_list<std::reference_wrapper<const TensorLayout>> operands)
    {
        std::size_t resultRank = 0;
        for (const auto operand : operands)
        {
            resultRank = std::max(resultRank, operand.get().rank());
        }
        std::vector<std::size_t> result(resultRank, 1);
        for (const auto operandReference : operands)
        {
            const auto& operand = operandReference.get();
            const auto padding = resultRank - operand.rank();
            for (std::size_t axis = 0; axis < operand.rank(); ++axis)
            {
                auto& resultExtent = result[padding + axis];
                const auto operandExtent = operand.extents()[axis];
                if (resultExtent == 1)
                {
                    resultExtent = operandExtent;
                }
                else if (operandExtent != 1 && operandExtent != resultExtent)
                {
                    throw std::invalid_argument("Tensor operands are not broadcast-compatible");
                }
            }
        }
        return DynamicExtents(std::move(result));
    }

private:
    [[nodiscard]] TensorStrides normalizeStrides(const TensorLayout& operand) const
    {
        if (operand.rank() > mExtents.rank())
        {
            throw std::invalid_argument("Tensor operand rank exceeds iteration rank");
        }
        TensorStrides result(mExtents.rank(), 0);
        const auto padding = mExtents.rank() - operand.rank();
        for (std::size_t targetAxis = 0; targetAxis < mExtents.rank(); ++targetAxis)
        {
            if (targetAxis < padding)
            {
                continue;
            }
            const auto sourceAxis = targetAxis - padding;
            const auto sourceExtent = operand.extents()[sourceAxis];
            const auto targetExtent = mExtents[targetAxis];
            if (sourceExtent != targetExtent && sourceExtent != 1)
            {
                throw std::invalid_argument("Tensor operand cannot broadcast to the iteration extents");
            }
            result[targetAxis] = sourceExtent == 1 && targetExtent != 1 ? 0 : operand.strides()[sourceAxis];
        }
        return result;
    }

    [[nodiscard]] bool canCoalesce(std::size_t leftAxis) const
    {
        const auto rightAxis = leftAxis + 1;
        for (const auto& strides : mStrides)
        {
            const auto expected = checkedStrideContribution(mExtents[rightAxis], strides[rightAxis]);
            if (strides[leftAxis] != expected)
            {
                return false;
            }
        }
        return true;
    }

    void coalesceAxes()
    {
        if (mLogicalSize == 0 || mExtents.rank() < 2)
        {
            return;
        }

        auto extents = mExtents.values();
        std::size_t axis = extents.size() - 1;
        while (axis > 0)
        {
            const auto left = axis - 1;
            if (canCoalesce(left))
            {
                extents[left] *= extents[axis];
                extents.erase(extents.begin() + static_cast<std::ptrdiff_t>(axis));
                for (auto& strides : mStrides)
                {
                    strides[left] = strides[axis];
                    strides.erase(strides.begin() + static_cast<std::ptrdiff_t>(axis));
                }
                mExtents = DynamicExtents(extents);
                if (axis >= extents.size())
                {
                    axis = extents.size() - 1;
                }
            }
            else
            {
                --axis;
            }
        }
    }

    DynamicExtents mExtents;
    std::size_t mLogicalSize = 0;
    std::vector<std::ptrdiff_t> mOrigins;
    std::vector<TensorStrides> mStrides;
};

} // namespace fat_p::tensor_detail
