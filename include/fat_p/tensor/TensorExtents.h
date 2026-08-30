#pragma once

/*
FATP_META:
  meta_version: 1
  component: TensorLayout
  file_role: internal_header
  path: include/fat_p/tensor/TensorExtents.h
  namespace: fat_p
  layer: Domain
  summary: "Checked runtime Tensor extents and normalized axis vocabulary."
  api_stability: in_work
  related:
    docs:
      - components/Tensor/docs/Design Note - Tensor Semantic Contract.md
      - components/Tensor/docs/Design Note - Tensor Architecture Additions Plan.md
    headers:
      - include/fat_p/TensorLayout.h
      - include/fat_p/tensor/TensorLayout.h
    tests:
      - components/Tensor/tests/test_TensorLayout.cpp
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
 * @file TensorExtents.h
 * @brief Checked runtime extents and axis normalization for Tensor layouts.
 */

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace fat_p
{

using TensorAxis = std::ptrdiff_t;
using TensorStrides = std::vector<std::ptrdiff_t>;

namespace tensor_detail
{

inline std::size_t checkedLogicalSize(const std::vector<std::size_t>& extents)
{
    if (extents.empty())
    {
        return 1;
    }
    if (std::find(extents.begin(), extents.end(), std::size_t{0}) != extents.end())
    {
        return 0;
    }

    std::size_t result = 1;
    constexpr auto maximumPointerDifference = static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max());
    for (const auto extent : extents)
    {
        if (extent > std::numeric_limits<std::size_t>::max() / result)
        {
            throw std::overflow_error("Tensor extent product exceeds size_t");
        }
        result *= extent;
        if (result > maximumPointerDifference)
        {
            throw std::overflow_error("Tensor extent product exceeds ptrdiff_t");
        }
    }
    return result;
}

} // namespace tensor_detail

class DynamicExtents
{
public:
    using value_type = std::size_t;
    using container_type = std::vector<value_type>;
    using const_iterator = container_type::const_iterator;

    DynamicExtents()
        : mLogicalSize(1)
    {
    }

    DynamicExtents(std::initializer_list<value_type> extents)
        : DynamicExtents(container_type(extents))
    {
    }

    explicit DynamicExtents(container_type extents)
        : mExtents(std::move(extents))
        , mLogicalSize(tensor_detail::checkedLogicalSize(mExtents))
    {
    }

    [[nodiscard]] std::size_t rank() const noexcept
    {
        return mExtents.size();
    }

    [[nodiscard]] std::size_t logicalSize() const noexcept
    {
        return mLogicalSize;
    }

    [[nodiscard]] bool hasZeroExtent() const noexcept
    {
        return mLogicalSize == 0;
    }

    [[nodiscard]] const container_type& values() const noexcept
    {
        return mExtents;
    }

    [[nodiscard]] value_type operator[](std::size_t axis) const noexcept
    {
        return mExtents[axis];
    }

    [[nodiscard]] value_type at(std::size_t axis) const
    {
        return mExtents.at(axis);
    }

    [[nodiscard]] const_iterator begin() const noexcept
    {
        return mExtents.begin();
    }

    [[nodiscard]] const_iterator end() const noexcept
    {
        return mExtents.end();
    }

    friend bool operator==(const DynamicExtents&, const DynamicExtents&) = default;

private:
    container_type mExtents;
    std::size_t mLogicalSize = 1;
};

inline std::size_t normalizeAxis(TensorAxis axis, std::size_t rank)
{
    if (rank > static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max()))
    {
        throw std::overflow_error("Tensor rank cannot be represented as ptrdiff_t for axis normalization");
    }
    const auto signedRank = static_cast<std::ptrdiff_t>(rank);
    const auto normalized = axis < 0 ? axis + signedRank : axis;
    if (normalized < 0 || normalized >= signedRank)
    {
        throw std::out_of_range("Tensor axis is outside the layout rank");
    }
    return static_cast<std::size_t>(normalized);
}

inline std::vector<std::size_t> normalizeAxes(const std::vector<TensorAxis>& axes, std::size_t rank)
{
    std::vector<std::size_t> result;
    result.reserve(axes.size());
    for (const auto axis : axes)
    {
        const auto normalized = normalizeAxis(axis, rank);
        if (std::find(result.begin(), result.end(), normalized) != result.end())
        {
            throw std::invalid_argument("Tensor axes contain a duplicate axis");
        }
        result.push_back(normalized);
    }
    return result;
}

} // namespace fat_p
