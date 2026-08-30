#pragma once

/*
FATP_META:
  meta_version: 1
  component: TensorSlice
  file_role: internal_header
  path: include/fat_p/tensor/TensorSlice.h
  namespace: fat_p
  layer: Domain
  summary: "Extended metadata-only Tensor slicing and permutation vocabulary."
  api_stability: in_work
  related:
    docs:
      - components/Tensor/docs/Design Note - Tensor Architecture Additions Plan.md
    headers:
      - include/fat_p/TensorSlice.h
      - include/fat_p/tensor/TensorLayout.h
    tests:
      - components/Tensor/tests/test_TensorSlice.cpp
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
 * @file TensorSlice.h
 * @brief Python-style slicing descriptors and pointer-free layout transforms.
 */

#include "TensorLayout.h"

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <limits>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace fat_p
{

struct Slice
{
    std::optional<std::ptrdiff_t> start;
    std::optional<std::ptrdiff_t> stop;
    std::ptrdiff_t step = 1;
};

struct AllSlice
{
};
struct NewAxisSlice
{
};
struct EllipsisSlice
{
};

inline constexpr AllSlice All{};
inline constexpr NewAxisSlice NewAxis{};
inline constexpr EllipsisSlice Ellipsis{};

using SliceSpec = std::variant<Slice, std::ptrdiff_t, AllSlice, NewAxisSlice, EllipsisSlice>;

namespace tensor_detail
{

struct NormalizedSlice
{
    std::ptrdiff_t start = 0;
    std::size_t length = 0;
    std::ptrdiff_t step = 1;
};

inline std::ptrdiff_t checkedSliceStrideProduct(std::ptrdiff_t left, std::ptrdiff_t right)
{
    if (left == 0 || right == 0)
    {
        return 0;
    }
    const auto leftMagnitude = offsetMagnitude(left);
    const auto rightMagnitude = offsetMagnitude(right);
    const bool negative = (left < 0) != (right < 0);
    constexpr auto maximum = std::numeric_limits<std::ptrdiff_t>::max();
    constexpr UnsignedOffset negativeLimit = static_cast<UnsignedOffset>(maximum) + UnsignedOffset{1};
    const auto limit = negative ? negativeLimit : static_cast<UnsignedOffset>(maximum);
    if (leftMagnitude > limit / rightMagnitude)
    {
        throw std::overflow_error("Tensor slice stride exceeds ptrdiff_t");
    }
    const auto product = leftMagnitude * rightMagnitude;
    if (!negative)
    {
        return static_cast<std::ptrdiff_t>(product);
    }
    if (product == negativeLimit)
    {
        return std::numeric_limits<std::ptrdiff_t>::min();
    }
    return -static_cast<std::ptrdiff_t>(product);
}

inline std::ptrdiff_t checkedExtentToDifference(std::size_t extent)
{
    if (extent > static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max()))
    {
        throw std::overflow_error("Tensor slice extent exceeds ptrdiff_t");
    }
    return static_cast<std::ptrdiff_t>(extent);
}

inline NormalizedSlice normalizeSlice(const Slice& slice, std::size_t extent)
{
    if (slice.step == 0)
    {
        throw std::invalid_argument("Tensor slice step cannot be zero");
    }
    const auto size = checkedExtentToDifference(extent);
    if (slice.step > 0)
    {
        auto start = slice.start.value_or(0);
        auto stop = slice.stop.value_or(size);
        if (slice.start && start < 0)
        {
            start = checkedOffsetAdd(start, size);
        }
        if (slice.stop && stop < 0)
        {
            stop = checkedOffsetAdd(stop, size);
        }
        start = std::clamp(start, std::ptrdiff_t{0}, size);
        stop = std::clamp(stop, std::ptrdiff_t{0}, size);
        const auto distance = stop > start ? static_cast<std::size_t>(stop - start) : std::size_t{0};
        const auto step = static_cast<std::size_t>(slice.step);
        const auto length = distance == 0 ? 0 : 1 + (distance - 1) / step;
        return {start, length, slice.step};
    }

    auto start = slice.start.value_or(size - 1);
    auto stop = slice.stop.value_or(-1);
    if (slice.start && start < 0)
    {
        start = checkedOffsetAdd(start, size);
    }
    if (slice.stop && stop < 0)
    {
        stop = checkedOffsetAdd(stop, size);
    }
    start = std::clamp(start, std::ptrdiff_t{-1}, size - 1);
    stop = std::clamp(stop, std::ptrdiff_t{-1}, size - 1);
    const auto distance = start > stop ? static_cast<std::size_t>(start - stop) : std::size_t{0};
    const auto stepMagnitude = offsetMagnitude(slice.step);
    const auto length = distance == 0 ? 0 : 1 + (distance - 1) / stepMagnitude;
    return {start, length, slice.step};
}

inline std::size_t normalizeSliceIndex(std::ptrdiff_t index, std::size_t extent)
{
    if (index >= 0)
    {
        const auto converted = static_cast<std::size_t>(index);
        if (converted >= extent)
        {
            throw std::out_of_range("Tensor slice index is outside the source extent");
        }
        return converted;
    }
    const auto magnitude = offsetMagnitude(index);
    if (magnitude > extent)
    {
        throw std::out_of_range("Tensor slice index is outside the source extent");
    }
    return extent - static_cast<std::size_t>(magnitude);
}

inline TensorLayout extendedSliceLayout(const TensorLayout& source, const std::vector<SliceSpec>& specifications)
{
    std::size_t consuming = 0;
    std::size_t ellipses = 0;
    for (const auto& specification : specifications)
    {
        if (std::holds_alternative<EllipsisSlice>(specification))
        {
            ++ellipses;
        }
        else if (!std::holds_alternative<NewAxisSlice>(specification))
        {
            ++consuming;
        }
    }
    if (ellipses > 1)
    {
        throw std::invalid_argument("Tensor slice accepts at most one ellipsis");
    }
    if (consuming > source.rank())
    {
        throw std::invalid_argument("Tensor slice consumes more axes than the source rank");
    }

    const auto ellipsisWidth = source.rank() - consuming;
    std::vector<SliceSpec> expanded;
    expanded.reserve(specifications.size() + ellipsisWidth + (ellipses == 0 ? 1 : 0));
    for (const auto& specification : specifications)
    {
        if (std::holds_alternative<EllipsisSlice>(specification))
        {
            for (std::size_t axis = 0; axis < ellipsisWidth; ++axis)
            {
                expanded.emplace_back(All);
            }
        }
        else
        {
            expanded.push_back(specification);
        }
    }
    if (ellipses == 0)
    {
        for (std::size_t axis = 0; axis < ellipsisWidth; ++axis)
        {
            expanded.emplace_back(All);
        }
    }

    std::vector<std::size_t> resultExtents;
    TensorStrides resultStrides;
    resultExtents.reserve(expanded.size());
    resultStrides.reserve(expanded.size());
    std::vector<std::pair<std::size_t, std::size_t>> originTerms;
    std::size_t sourceAxis = 0;

    for (const auto& specification : expanded)
    {
        if (std::holds_alternative<NewAxisSlice>(specification))
        {
            resultExtents.push_back(1);
            resultStrides.push_back(0);
            continue;
        }
        if (sourceAxis >= source.rank())
        {
            throw std::invalid_argument("Tensor slice consumes more axes than the source rank");
        }

        if (const auto* index = std::get_if<std::ptrdiff_t>(&specification))
        {
            originTerms.emplace_back(sourceAxis, normalizeSliceIndex(*index, source.extents()[sourceAxis]));
            ++sourceAxis;
            continue;
        }

        if (std::holds_alternative<AllSlice>(specification))
        {
            resultExtents.push_back(source.extents()[sourceAxis]);
            resultStrides.push_back(source.strides()[sourceAxis]);
            originTerms.emplace_back(sourceAxis, std::size_t{0});
            ++sourceAxis;
            continue;
        }

        Slice range;
        if (const auto* requested = std::get_if<Slice>(&specification))
        {
            range = *requested;
        }
        const auto normalized = normalizeSlice(range, source.extents()[sourceAxis]);
        resultExtents.push_back(normalized.length);
        resultStrides.push_back(checkedSliceStrideProduct(normalized.step, source.strides()[sourceAxis]));
        originTerms.emplace_back(sourceAxis,
                                 normalized.length == 0 ? std::size_t{0}
                                                        : static_cast<std::size_t>(normalized.start));
        ++sourceAxis;
    }
    if (sourceAxis != source.rank())
    {
        throw std::logic_error("Tensor slice expansion failed to consume the source rank");
    }

    DynamicExtents extents(std::move(resultExtents));
    auto origin = source.originOffset();
    if (extents.logicalSize() != 0)
    {
        for (const auto [axis, index] : originTerms)
        {
            origin = checkedOffsetAdd(origin,
                                      checkedStrideContribution(index, source.strides()[axis]));
        }
    }
    return TensorLayout(source.storageLength(), origin, std::move(extents), std::move(resultStrides));
}

inline TensorLayout permuteLayout(const TensorLayout& source, const std::vector<TensorAxis>& order)
{
    if (order.size() != source.rank())
    {
        throw std::invalid_argument("Tensor permutation rank must match the source rank");
    }
    const auto axes = normalizeAxes(order, source.rank());
    std::vector<std::size_t> extents;
    TensorStrides strides;
    extents.reserve(source.rank());
    strides.reserve(source.rank());
    for (const auto axis : axes)
    {
        extents.push_back(source.extents()[axis]);
        strides.push_back(source.strides()[axis]);
    }
    return TensorLayout(source.storageLength(), source.originOffset(), DynamicExtents(std::move(extents)),
                        std::move(strides));
}

inline TensorLayout squeezeLayout(const TensorLayout& source, const std::vector<TensorAxis>& requestedAxes)
{
    std::vector<std::size_t> axes;
    if (requestedAxes.empty())
    {
        for (std::size_t axis = 0; axis < source.rank(); ++axis)
        {
            if (source.extents()[axis] == 1)
            {
                axes.push_back(axis);
            }
        }
    }
    else
    {
        axes = normalizeAxes(requestedAxes, source.rank());
        for (const auto axis : axes)
        {
            if (source.extents()[axis] != 1)
            {
                throw std::invalid_argument("Tensor squeeze axis must have extent one");
            }
        }
    }

    std::vector<std::size_t> extents;
    TensorStrides strides;
    for (std::size_t axis = 0; axis < source.rank(); ++axis)
    {
        if (std::find(axes.begin(), axes.end(), axis) == axes.end())
        {
            extents.push_back(source.extents()[axis]);
            strides.push_back(source.strides()[axis]);
        }
    }
    return TensorLayout(source.storageLength(), source.originOffset(), DynamicExtents(std::move(extents)),
                        std::move(strides));
}

inline TensorLayout unsqueezeLayout(const TensorLayout& source, TensorAxis requestedAxis)
{
    if (source.rank() >= static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max()))
    {
        throw std::overflow_error("Tensor rank cannot be incremented");
    }
    const auto resultRank = static_cast<std::ptrdiff_t>(source.rank() + 1);
    auto axis = requestedAxis < 0 ? checkedOffsetAdd(requestedAxis, resultRank) : requestedAxis;
    if (axis < 0 || axis >= resultRank)
    {
        throw std::out_of_range("Tensor unsqueeze axis is outside the result rank");
    }
    auto extents = source.extents().values();
    auto strides = source.strides();
    extents.insert(extents.begin() + axis, 1);
    strides.insert(strides.begin() + axis, 0);
    return TensorLayout(source.storageLength(), source.originOffset(), DynamicExtents(std::move(extents)),
                        std::move(strides));
}

} // namespace tensor_detail

} // namespace fat_p
