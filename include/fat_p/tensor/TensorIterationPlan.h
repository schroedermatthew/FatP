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
#include <array>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

namespace fat_p::tensor_detail
{

template <typename Extents>
struct ExtentsStaticRank : std::integral_constant<std::size_t, kDynamicTensorRank>
{
};

template <std::size_t Rank>
struct ExtentsStaticRank<FixedRankExtents<Rank>> : std::integral_constant<std::size_t, Rank>
{
};

template <typename Extents>
inline constexpr std::size_t extentsStaticRank = ExtentsStaticRank<std::remove_cvref_t<Extents>>::value;

/**
 * @brief Allocation-free operand bookkeeping for rank-aware Tensor kernels.
 *
 * Fixed ranks use only arrays. Dynamic ranks retain unbounded metadata but keep
 * common ranks inline through TensorMetadataStorage. Operand count is always a
 * compile-time value, avoiding a separate allocation per kernel invocation.
 */
template <std::size_t Rank, std::size_t OperandCount>
class BasicTensorIterationPlan
{
    static_assert(OperandCount > 0, "A Tensor iteration plan requires an operand");

public:
    using extents_type = TensorExtentsFor<Rank>;
    using strides_type = TensorStridesFor<Rank>;
    using iteration_extents_type =
        std::conditional_t<Rank == kDynamicTensorRank, TensorMetadataStorage<std::size_t>,
                           std::array<std::size_t, Rank>>;
    using offsets_type = std::array<std::ptrdiff_t, OperandCount>;

    template <typename Extents, typename... Layouts>
        requires(sizeof...(Layouts) == OperandCount)
    explicit BasicTensorIterationPlan(const Extents& extents, const Layouts&... layouts)
        : mExtents(convertExtents(extents))
        , mLogicalSize(mExtents.logicalSize())
    {
        initializeLayouts(std::index_sequence_for<Layouts...>{}, layouts...);
        initializeIterationExtents();
        coalesceAxes();
    }

    [[nodiscard]] const extents_type& extents() const noexcept { return mExtents; }
    [[nodiscard]] std::size_t logicalSize() const noexcept { return mLogicalSize; }
    [[nodiscard]] std::size_t rank() const noexcept { return mExtents.rank(); }
    [[nodiscard]] static constexpr std::size_t operandCount() noexcept { return OperandCount; }

    template <typename Function>
    void forEachOffset(Function&& function) const
    {
        if (mLogicalSize == 0)
        {
            return;
        }

        using Coordinates = std::conditional_t<Rank == kDynamicTensorRank,
                                               TensorMetadataStorage<std::size_t>,
                                               std::array<std::size_t, Rank>>;
        Coordinates coordinates{};
        if constexpr (Rank == kDynamicTensorRank)
        {
            coordinates.resize(mActiveRank, 0);
        }
        offsets_type offsets = mOrigins;
        for (std::size_t linearIndex = 0; linearIndex < mLogicalSize; ++linearIndex)
        {
            std::invoke(function, linearIndex, offsets);
            if (linearIndex + 1 == mLogicalSize || mActiveRank == 0)
            {
                continue;
            }

            for (std::size_t reverseAxis = mActiveRank; reverseAxis > 0; --reverseAxis)
            {
                const auto axis = reverseAxis - 1;
                ++coordinates[axis];
                if (coordinates[axis] < mIterationExtents[axis])
                {
                    for (std::size_t operand = 0; operand < OperandCount; ++operand)
                    {
                        offsets[operand] = checkedOffsetAdd(offsets[operand], mStrides[operand][axis]);
                    }
                    break;
                }

                coordinates[axis] = 0;
                for (std::size_t operand = 0; operand < OperandCount; ++operand)
                {
                    const auto rewind =
                        checkedStrideContribution(mIterationExtents[axis] - 1, mStrides[operand][axis]);
                    offsets[operand] = checkedOffsetSubtract(offsets[operand], rewind);
                }
            }
        }
    }

private:
    template <typename Extents>
    [[nodiscard]] static extents_type convertExtents(const Extents& extents)
    {
        if constexpr (Rank == kDynamicTensorRank)
        {
            return makeDynamicExtents(extents);
        }
        else
        {
            return makeFixedExtents<Rank>(extents);
        }
    }

    template <typename Layout>
    [[nodiscard]] strides_type normalizeStrides(const Layout& operand) const
    {
        if (operand.rank() > rank())
        {
            throw std::invalid_argument("Tensor operand rank exceeds iteration rank");
        }
        strides_type result{};
        if constexpr (Rank == kDynamicTensorRank)
        {
            result.resize(rank(), 0);
        }
        const auto padding = rank() - operand.rank();
        for (std::size_t targetAxis = padding; targetAxis < rank(); ++targetAxis)
        {
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

    template <std::size_t... Indices, typename... Layouts>
    void initializeLayouts(std::index_sequence<Indices...>, const Layouts&... layouts)
    {
        ((mOrigins[Indices] = layouts.originOffset(), mStrides[Indices] = normalizeStrides(layouts)), ...);
    }

    void initializeIterationExtents()
    {
        mActiveRank = rank();
        if constexpr (Rank == kDynamicTensorRank)
        {
            mIterationExtents = mExtents.values();
        }
        else
        {
            std::copy(mExtents.begin(), mExtents.end(), mIterationExtents.begin());
        }
    }

    [[nodiscard]] bool canCoalesce(std::size_t leftAxis) const
    {
        const auto rightAxis = leftAxis + 1;
        for (const auto& strides : mStrides)
        {
            const auto expected = checkedStrideContribution(mIterationExtents[rightAxis], strides[rightAxis]);
            if (strides[leftAxis] != expected)
            {
                return false;
            }
        }
        return true;
    }

    void eraseIterationAxis(std::size_t axis)
    {
        if constexpr (Rank == kDynamicTensorRank)
        {
            mIterationExtents.erase(mIterationExtents.begin() + static_cast<std::ptrdiff_t>(axis));
            for (auto& strides : mStrides)
            {
                strides.erase(strides.begin() + static_cast<std::ptrdiff_t>(axis));
            }
        }
        else
        {
            for (std::size_t current = axis; current + 1 < mActiveRank; ++current)
            {
                mIterationExtents[current] = mIterationExtents[current + 1];
                for (auto& strides : mStrides)
                {
                    strides[current] = strides[current + 1];
                }
            }
        }
        --mActiveRank;
    }

    void coalesceAxes()
    {
        if (mLogicalSize == 0 || mActiveRank < 2)
        {
            return;
        }
        std::size_t axis = mActiveRank - 1;
        while (axis > 0)
        {
            const auto left = axis - 1;
            if (canCoalesce(left))
            {
                mIterationExtents[left] *= mIterationExtents[axis];
                for (auto& strides : mStrides)
                {
                    strides[left] = strides[axis];
                }
                eraseIterationAxis(axis);
                if (axis >= mActiveRank)
                {
                    axis = mActiveRank - 1;
                }
            }
            else
            {
                --axis;
            }
        }
    }

    extents_type mExtents;
    iteration_extents_type mIterationExtents{};
    std::size_t mActiveRank = 0;
    std::size_t mLogicalSize = 0;
    offsets_type mOrigins{};
    std::array<strides_type, OperandCount> mStrides{};
};

template <typename Extents, typename... Layouts>
[[nodiscard]] auto makeTensorIterationPlan(const Extents& extents, const Layouts&... layouts)
{
    return BasicTensorIterationPlan<extentsStaticRank<Extents>, sizeof...(Layouts)>(extents, layouts...);
}

template <typename LeftLayout, typename RightLayout>
inline constexpr std::size_t broadcastStaticRank = [] {
    if constexpr (extentsStaticRank<typename LeftLayout::extents_type> == kDynamicTensorRank ||
                  extentsStaticRank<typename RightLayout::extents_type> == kDynamicTensorRank)
    {
        return kDynamicTensorRank;
    }
    else
    {
        return extentsStaticRank<typename LeftLayout::extents_type> >
                extentsStaticRank<typename RightLayout::extents_type>
            ? extentsStaticRank<typename LeftLayout::extents_type>
            : extentsStaticRank<typename RightLayout::extents_type>;
    }
}();

template <typename LeftLayout, typename RightLayout>
[[nodiscard]] auto broadcastExtents(const LeftLayout& left, const RightLayout& right)
{
    constexpr auto ResultRank = broadcastStaticRank<LeftLayout, RightLayout>;
    const auto resultRank = std::max(left.rank(), right.rank());
    using ResultExtents = TensorExtentsFor<ResultRank>;
    typename ResultExtents::container_type values{};
    if constexpr (ResultRank == kDynamicTensorRank)
    {
        values.resize(resultRank, 1);
    }
    else
    {
        values.fill(1);
    }

    const auto merge = [&](const auto& operand) {
        const auto padding = resultRank - operand.rank();
        for (std::size_t axis = 0; axis < operand.rank(); ++axis)
        {
            auto& resultExtent = values[padding + axis];
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
    };
    merge(left);
    merge(right);
    return ResultExtents(std::move(values));
}

} // namespace fat_p::tensor_detail
