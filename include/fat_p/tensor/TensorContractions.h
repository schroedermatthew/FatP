#pragma once

/** @file TensorContractions.h @brief Checked, ordered contraction of arbitrary Tensor axes. */

/*
FATP_META:
  meta_version: 1
  component: TensorContractions
  file_role: internal_header
  path: include/fat_p/tensor/TensorContractions.h
  namespace: fat_p
  layer: Domain
  summary: "Explicit-axis contraction planning and serial output kernels."
  api_stability: in_work
  related:
    docs:
      - components/Tensor/docs/Design Note - Tensor Architecture Additions Plan.md
    headers:
      - include/fat_p/TensorContractions.h
      - include/fat_p/TensorMatmul.h
    tests:
      - components/Tensor/tests/test_TensorContractions.cpp
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

#include "TensorMatmul.h"

#include <array>
#include <cstddef>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace fat_p
{
namespace tensor_detail
{

// Metadata only: no operand packing or size-proportional offset tables.
// Output coordinates precede contracted coordinates. Each list retains its order.
struct ContractionShape
{
    DynamicExtents outputExtents;
    std::vector<std::size_t> innerExtents;
    TensorStrides leftOutputStrides;
    TensorStrides rightOutputStrides;
    TensorStrides leftInnerStrides;
    TensorStrides rightInnerStrides;
    std::size_t inner = 0;
    std::size_t innerRun = 1;
    std::ptrdiff_t leftInnerStep = 0;
    std::ptrdiff_t rightInnerStep = 0;
    std::size_t innerActiveRank = 0;
};

template <std::size_t LeftRank, std::size_t RightRank, std::size_t PairCount>
struct FixedContractionShape
{
    static constexpr std::size_t OutputRank = LeftRank + RightRank - 2 * PairCount;
    FixedRankExtents<OutputRank> outputExtents;
    std::array<std::size_t, PairCount> innerExtents{};
    std::array<std::ptrdiff_t, OutputRank> leftOutputStrides{};
    std::array<std::ptrdiff_t, OutputRank> rightOutputStrides{};
    std::array<std::ptrdiff_t, PairCount> leftInnerStrides{};
    std::array<std::ptrdiff_t, PairCount> rightInnerStrides{};
    std::size_t inner = 0;
    std::size_t innerRun = 1;
    std::ptrdiff_t leftInnerStep = 0;
    std::ptrdiff_t rightInnerStep = 0;
    std::size_t innerActiveRank = PairCount;
};

template <typename LeftLayout, typename RightLayout>
inline ContractionShape makeContractionShape(const LeftLayout& left,
                                             const RightLayout& right,
                                             const std::vector<TensorAxis>& leftAxes,
                                             const std::vector<TensorAxis>& rightAxes)
{
    if (leftAxes.size() != rightAxes.size())
    {
        throw std::invalid_argument("tensorDot requires equal axis-list lengths");
    }
    const auto normalizedLeft = normalizeAxes(leftAxes, left.rank());
    const auto normalizedRight = normalizeAxes(rightAxes, right.rank());
    std::vector<bool> leftContracted(left.rank(), false);
    std::vector<bool> rightContracted(right.rank(), false);
    ContractionShape shape;
    for (std::size_t pair = 0; pair < normalizedLeft.size(); ++pair)
    {
        const auto a = normalizedLeft[pair];
        const auto b = normalizedRight[pair];
        if (left.extents()[a] != right.extents()[b])
        {
            throw std::invalid_argument("tensorDot paired axis extents must match");
        }
        leftContracted[a] = true;
        rightContracted[b] = true;
        shape.innerExtents.push_back(left.extents()[a]);
        shape.leftInnerStrides.push_back(left.strides()[a]);
        shape.rightInnerStrides.push_back(right.strides()[b]);
    }
    std::vector<std::size_t> output;
    for (std::size_t axis = 0; axis < left.rank(); ++axis)
    {
        if (!leftContracted[axis])
        {
            output.push_back(left.extents()[axis]);
            shape.leftOutputStrides.push_back(left.strides()[axis]);
            shape.rightOutputStrides.push_back(0);
        }
    }
    for (std::size_t axis = 0; axis < right.rank(); ++axis)
    {
        if (!rightContracted[axis])
        {
            output.push_back(right.extents()[axis]);
            shape.leftOutputStrides.push_back(0);
            shape.rightOutputStrides.push_back(right.strides()[axis]);
        }
    }
    shape.outputExtents = DynamicExtents(std::move(output));
    // A zero free extent makes even a huge contracted domain unreachable.
    // Still validate all axis pairs above, but do not multiply that domain.
    if (shape.outputExtents.logicalSize() != 0)
    {
        shape.inner = checkedLogicalSize(shape.innerExtents);
    }
    if (shape.inner != 0 && !shape.innerExtents.empty())
    {
        // Keep the final contracted axis as a strided run: decode the outer
        // contracted coordinates once per run, not once per scalar product.
        shape.innerRun = shape.innerExtents.back();
        shape.leftInnerStep = shape.leftInnerStrides.back();
        shape.rightInnerStep = shape.rightInnerStrides.back();
        shape.innerExtents.pop_back();
        shape.leftInnerStrides.pop_back();
        shape.rightInnerStrides.pop_back();
    }
    shape.innerActiveRank = shape.innerExtents.size();
    return shape;
}

template <std::size_t LeftRank, std::size_t RightRank, std::size_t PairCount>
[[nodiscard]] FixedContractionShape<LeftRank, RightRank, PairCount>
makeFixedContractionShape(const BasicTensorLayout<LeftRank>& left,
                          const BasicTensorLayout<RightRank>& right,
                          const std::array<TensorAxis, PairCount>& leftAxes,
                          const std::array<TensorAxis, PairCount>& rightAxes)
{
    static_assert(PairCount <= LeftRank && PairCount <= RightRank,
                  "tensorDot contracts more axes than an operand has");
    const auto normalizedLeft = normalizeAxes(leftAxes, LeftRank);
    const auto normalizedRight = normalizeAxes(rightAxes, RightRank);
    std::array<bool, LeftRank> leftContracted{};
    std::array<bool, RightRank> rightContracted{};
    FixedContractionShape<LeftRank, RightRank, PairCount> shape;
    for (std::size_t pair = 0; pair < PairCount; ++pair)
    {
        const auto leftAxis = normalizedLeft[pair];
        const auto rightAxis = normalizedRight[pair];
        if (left.extents()[leftAxis] != right.extents()[rightAxis])
        {
            throw std::invalid_argument("tensorDot paired axis extents must match");
        }
        leftContracted[leftAxis] = true;
        rightContracted[rightAxis] = true;
        shape.innerExtents[pair] = left.extents()[leftAxis];
        shape.leftInnerStrides[pair] = left.strides()[leftAxis];
        shape.rightInnerStrides[pair] = right.strides()[rightAxis];
    }

    std::array<std::size_t, FixedContractionShape<LeftRank, RightRank, PairCount>::OutputRank>
        outputExtents{};
    std::size_t outputAxis = 0;
    for (std::size_t axis = 0; axis < LeftRank; ++axis)
    {
        if (!leftContracted[axis])
        {
            outputExtents[outputAxis] = left.extents()[axis];
            shape.leftOutputStrides[outputAxis] = left.strides()[axis];
            ++outputAxis;
        }
    }
    for (std::size_t axis = 0; axis < RightRank; ++axis)
    {
        if (!rightContracted[axis])
        {
            outputExtents[outputAxis] = right.extents()[axis];
            shape.rightOutputStrides[outputAxis] = right.strides()[axis];
            ++outputAxis;
        }
    }
    shape.outputExtents =
        FixedRankExtents<FixedContractionShape<LeftRank, RightRank, PairCount>::OutputRank>(
            std::move(outputExtents));
    if (shape.outputExtents.logicalSize() != 0)
    {
        shape.inner = checkedLogicalSize(shape.innerExtents);
    }
    if constexpr (PairCount > 0)
    {
        if (shape.inner != 0)
        {
            shape.innerRun = shape.innerExtents[PairCount - 1];
            shape.leftInnerStep = shape.leftInnerStrides[PairCount - 1];
            shape.rightInnerStep = shape.rightInnerStrides[PairCount - 1];
            shape.innerActiveRank = PairCount - 1;
        }
    }
    return shape;
}

template <typename Extents, typename LeftStrides, typename RightStrides>
inline std::pair<std::ptrdiff_t, std::ptrdiff_t> contractionOffsets(std::size_t linear,
                                                                    const Extents& extents,
                                                                    const LeftStrides& leftStrides,
                                                                    const RightStrides& rightStrides,
                                                                    std::ptrdiff_t leftOrigin,
                                                                    std::ptrdiff_t rightOrigin,
                                                                    std::size_t activeRank)
{
    // Only called for reachable coordinates, never for an empty domain.
    for (std::size_t reverse = activeRank; reverse > 0; --reverse)
    {
        const auto axis = reverse - 1;
        const auto coordinate = linear % extents[axis];
        linear /= extents[axis];
        leftOrigin = checkedOffsetAdd(leftOrigin, checkedStrideContribution(coordinate, leftStrides[axis]));
        rightOrigin = checkedOffsetAdd(rightOrigin, checkedStrideContribution(coordinate, rightStrides[axis]));
    }
    return {leftOrigin, rightOrigin};
}


template <typename Extents, typename LeftStrides, typename RightStrides>
inline std::pair<std::ptrdiff_t, std::ptrdiff_t> contractionOffsets(
    std::size_t linear, const Extents& extents, const LeftStrides& leftStrides,
    const RightStrides& rightStrides, std::ptrdiff_t leftOrigin,
    std::ptrdiff_t rightOrigin)
{
    return contractionOffsets(linear, extents, leftStrides, rightStrides,
                              leftOrigin, rightOrigin, extents.size());
}

template <ReadableTensor Left, ReadableTensor Right, typename Shape, typename Result,
          typename Allocator, std::size_t OutputRank>
void contractionRows(const Left& left,
                     const Right& right,
                     const Shape& shape,
                     Tensor<Result, Allocator, OutputRank>& result,
                     std::size_t first,
                     std::size_t last)
{
    const auto* a = TensorAccess::storageBase(left);
    const auto* b = TensorAccess::storageBase(right);
    for (auto output = first; output < last; ++output)
    {
        const auto origins = contractionOffsets(output,
                                                shape.outputExtents.values(),
                                                shape.leftOutputStrides,
                                                shape.rightOutputStrides,
                                                left.layout().originOffset(),
                                                right.layout().originOffset());
        Result total{0};
        for (std::size_t inner = 0; inner < shape.inner / shape.innerRun; ++inner)
        {
            auto offsets = contractionOffsets(inner,
                                              shape.innerExtents,
                                              shape.leftInnerStrides,
                                              shape.rightInnerStrides,
                                              origins.first,
                                              origins.second,
                                              shape.innerActiveRank);
            for (std::size_t index = 0; index < shape.innerRun; ++index)
            {
                const auto product = checkedSameTypeMultiply(static_cast<Result>(a[offsets.first]),
                                                             static_cast<Result>(b[offsets.second]));
                total = checkedSameTypeAdd(total, product);
                // Both endpoints are reachable coordinates of validated layouts.
                // Never advance after the last element (including singleton runs),
                // so neither signed addition nor pointer arithmetic escapes storage.
                if (index + 1 < shape.innerRun)
                {
                    offsets.first += shape.leftInnerStep;
                    offsets.second += shape.rightInnerStep;
                }
            }
        }
        result[output] = total;
    }
}

} // namespace tensor_detail

namespace tensor_detail
{

template <std::size_t PairCount, typename Left, typename Right>
inline constexpr std::size_t contractionStaticRank = [] {
    constexpr auto leftRank = tensorStaticRankValue<Left>;
    constexpr auto rightRank = tensorStaticRankValue<Right>;
    if constexpr (leftRank == kDynamicTensorRank || rightRank == kDynamicTensorRank)
    {
        return kDynamicTensorRank;
    }
    else
    {
        static_assert(PairCount <= leftRank && PairCount <= rightRank,
                      "tensorDot contracts more axes than an operand has");
        return leftRank + rightRank - 2 * PairCount;
    }
}();

} // namespace tensor_detail

/**
 * @brief Contract explicitly paired axes into a new canonical-contiguous owner.
 * @param left Arithmetic readable owner or view.
 * @param right Readable operand with the same value_type as left.
 * @param leftAxes Unique axes, with negative axes normalized against left.rank().
 * @param rightAxes Unique paired axes, with equal count and matching extents.
 * @param allocator Exact allocator for the TensorMatmulType result.
 * @return Left free axes followed by right free axes, each in original order.
 * @throws std::invalid_argument For repeated axes, unequal list lengths, or unequal paired extents.
 * @throws std::out_of_range For an axis outside its operand rank.
 * @throws std::overflow_error For unrepresentable layout/domain or integral product/sum.
 * @throws std::bad_alloc For element or metadata allocation failure.
 * @throws std::runtime_error For expired tracked borrowed views in assertions-enabled builds.
 * @details Empty axis lists mean a generalized outer product, not an all-axis reduction.
 * All-axis contraction returns rank zero. No batch broadcasting or conjugation is applied.
 * Each fold starts at positive zero and visits contracted coordinates in supplied pair
 * order, last listed axis fastest. Widening precedes multiplication; every integer
 * product and sum is checked. Zero-length contracted domains produce zeros without
 * reading storage. Inputs are never modified; no operand packing is performed.
 * Metadata uses standard allocators, independently of the result element allocator.
 * Serial by default; include TensorExecution.h for explicit context overloads.
 */
template <ReadableTensor Left, ReadableTensor Right, typename Allocator>
    requires SameTensorValue<Left, Right> && std::is_arithmetic_v<typename Left::value_type> &&
             tensor_detail::AllocatorFor<Allocator, TensorMatmulType<typename Left::value_type>>
[[nodiscard]] auto tensorDot(const Left& left,
                             const Right& right,
                             const std::vector<TensorAxis>& leftAxes,
                             const std::vector<TensorAxis>& rightAxes,
                             const Allocator& allocator)
    -> Tensor<TensorMatmulType<typename Left::value_type>, Allocator>
{
    using result_type = TensorMatmulType<typename Left::value_type>;
    tensor_detail::TensorAccess::validate(left);
    tensor_detail::TensorAccess::validate(right);
    const auto shape = tensor_detail::makeContractionShape(left.layout(), right.layout(), leftAxes, rightAxes);
    Tensor<result_type, Allocator> result(std::allocator_arg, allocator, shape.outputExtents, result_type{0});
    if (!result.empty() && shape.inner != 0)
    {
        tensor_detail::contractionRows(left, right, shape, result, 0, result.size());
    }
    return tensor_detail::TensorAccess::finishMaterialization(std::move(result));
}

/** @brief tensorDot with rebound first-owner SOCCC, or TensorAllocator for view-only inputs. */
template <ReadableTensor Left, ReadableTensor Right>
    requires SameTensorValue<Left, Right> && std::is_arithmetic_v<typename Left::value_type>
[[nodiscard]] auto tensorDot(const Left& left,
                             const Right& right,
                             const std::vector<TensorAxis>& leftAxes,
                             const std::vector<TensorAxis>& rightAxes)
{
    using result_type = TensorMatmulType<typename Left::value_type>;
    return tensorDot(left,
                     right,
                     leftAxes,
                     rightAxes,
                     tensor_detail::selectBinaryResultAllocator<result_type>(left, right));
}

template <std::size_t PairCount, ReadableTensor Left, ReadableTensor Right, typename Allocator>
    requires SameTensorValue<Left, Right> && std::is_arithmetic_v<typename Left::value_type> &&
             tensor_detail::AllocatorFor<Allocator, TensorMatmulType<typename Left::value_type>>
[[nodiscard]] auto tensorDot(const Left& left, const Right& right,
                             const std::array<TensorAxis, PairCount>& leftAxes,
                             const std::array<TensorAxis, PairCount>& rightAxes,
                             const Allocator& allocator)
{
    constexpr auto outputRank = tensor_detail::contractionStaticRank<PairCount, Left, Right>;
    using result_type = TensorMatmulType<typename Left::value_type>;
    tensor_detail::TensorAccess::validate(left);
    tensor_detail::TensorAccess::validate(right);
    const auto shape = [&] {
        if constexpr (outputRank == tensor_detail::kDynamicTensorRank)
        {
            const std::vector<TensorAxis> dynamicLeft(leftAxes.begin(), leftAxes.end());
            const std::vector<TensorAxis> dynamicRight(rightAxes.begin(), rightAxes.end());
            return tensor_detail::makeContractionShape(
                left.layout(), right.layout(), dynamicLeft, dynamicRight);
        }
        else
        {
            return tensor_detail::makeFixedContractionShape(
                left.layout(), right.layout(), leftAxes, rightAxes);
        }
    }();
    Tensor<result_type, Allocator, outputRank> result(
        std::allocator_arg, allocator, shape.outputExtents, result_type{0});
    if (!result.empty() && shape.inner != 0)
    {
        tensor_detail::contractionRows(left, right, shape, result, 0, result.size());
    }
    return tensor_detail::TensorAccess::finishMaterialization(std::move(result));
}

template <std::size_t PairCount, ReadableTensor Left, ReadableTensor Right>
    requires SameTensorValue<Left, Right> && std::is_arithmetic_v<typename Left::value_type>
[[nodiscard]] auto tensorDot(const Left& left, const Right& right,
                             const std::array<TensorAxis, PairCount>& leftAxes,
                             const std::array<TensorAxis, PairCount>& rightAxes)
{
    using result_type = TensorMatmulType<typename Left::value_type>;
    return tensorDot(left, right, leftAxes, rightAxes,
                     tensor_detail::selectBinaryResultAllocator<result_type>(left, right));
}

} // namespace fat_p
