#pragma once

/*
FATP_META:
  meta_version: 1
  component: Tensor
  file_role: test
  path: components/Tensor/tests/TensorTestSupport.h
  namespace: fat_p::testing::tensor_support
  layer: Testing
  summary: "Deterministic tensor layout generators and scalar offset oracles."
  api_stability: in_work
  related:
    docs:
      - components/Tensor/docs/Design Note - Tensor Architecture Additions Plan.md
      - components/Tensor/docs/Design Note - Tensor Semantic Contract.md
    tests:
      - components/Tensor/tests/test_Tensor.cpp
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
 * @file TensorTestSupport.h
 * @brief Deterministic randomized-layout inputs and implementation-independent oracles.
 */

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <utility>
#include <vector>

namespace fat_p::testing::tensor_support
{

struct LayoutSpec
{
    std::vector<std::size_t> extents;
    std::vector<std::ptrdiff_t> strides;
    std::ptrdiff_t origin = 0;
};

inline std::vector<std::ptrdiff_t> enumerateOffsets(const LayoutSpec& layout)
{
    if (layout.extents.size() != layout.strides.size())
    {
        return {};
    }
    if (std::find(layout.extents.begin(), layout.extents.end(), std::size_t{0}) != layout.extents.end())
    {
        return {};
    }
    if (layout.extents.empty())
    {
        return {layout.origin};
    }

    std::size_t logicalSize = 1;
    for (const auto extent : layout.extents)
    {
        logicalSize *= extent;
    }

    std::vector<std::ptrdiff_t> offsets;
    offsets.reserve(logicalSize);
    for (std::size_t linear = 0; linear < logicalSize; ++linear)
    {
        std::size_t remainder = linear;
        std::ptrdiff_t offset = layout.origin;
        for (std::size_t reverseAxis = layout.extents.size(); reverseAxis > 0; --reverseAxis)
        {
            const std::size_t axis = reverseAxis - 1;
            const auto index = remainder % layout.extents[axis];
            remainder /= layout.extents[axis];
            offset += static_cast<std::ptrdiff_t>(index) * layout.strides[axis];
        }
        offsets.push_back(offset);
    }
    return offsets;
}

inline std::optional<std::pair<std::ptrdiff_t, std::ptrdiff_t>> reachableBounds(const LayoutSpec& layout)
{
    const auto offsets = enumerateOffsets(layout);
    if (offsets.empty())
    {
        return std::nullopt;
    }
    const auto [minimum, maximum] = std::minmax_element(offsets.begin(), offsets.end());
    return std::pair<std::ptrdiff_t, std::ptrdiff_t>{*minimum, *maximum};
}

class DeterministicLayoutGenerator
{
public:
    explicit DeterministicLayoutGenerator(std::uint64_t seed)
        : mRandom(seed)
    {
    }

    LayoutSpec next(std::size_t rank, std::size_t maximumExtent = 4)
    {
        LayoutSpec result;
        result.extents.reserve(rank);
        result.strides.reserve(rank);
        std::uniform_int_distribution<std::size_t> extentDistribution(0, maximumExtent);
        std::uniform_int_distribution<std::ptrdiff_t> strideDistribution(-7, 7);
        std::uniform_int_distribution<std::ptrdiff_t> originDistribution(-5, 5);
        for (std::size_t axis = 0; axis < rank; ++axis)
        {
            result.extents.push_back(extentDistribution(mRandom));
            result.strides.push_back(strideDistribution(mRandom));
        }
        result.origin = originDistribution(mRandom);
        return result;
    }

private:
    std::mt19937_64 mRandom;
};

} // namespace fat_p::testing::tensor_support
