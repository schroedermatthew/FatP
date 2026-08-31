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
      - components/Tensor/tests/test_TensorAlgorithms.cpp
      - components/Tensor/tests/test_TensorLayout.cpp
      - components/Tensor/tests/test_TensorReductions.cpp
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
 * @file TensorTestSupport.h
 * @brief Deterministic randomized-layout inputs and implementation-independent oracles.
 */

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <stdexcept>
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

// A dense table of root-storage positions, deliberately independent of Tensor
// layouts, normalization helpers, and iteration plans. Only small test domains
// belong here; this is an oracle, not a second production transform engine.
struct CoordinateReference
{
    std::vector<std::size_t> extents;
    std::vector<std::ptrdiff_t> offsets;
};

struct CoordinateSelection
{
    std::optional<std::size_t> sourceAxis;
    std::vector<std::size_t> indices;
    bool keepAxis = true;
};

inline std::vector<std::size_t> coordinateRange(std::size_t extent)
{
    std::vector<std::size_t> result;
    for (std::size_t coordinate = 0; coordinate < extent; ++coordinate)
    {
        result.push_back(coordinate);
    }
    return result;
}

inline CoordinateReference selectCoordinates(const CoordinateReference& source,
                                              const std::vector<CoordinateSelection>& selections)
{
    CoordinateReference result;
    std::vector<bool> consumed(source.extents.size(), false);
    for (const auto& selection : selections)
    {
        if (!selection.keepAxis && selection.indices.size() != 1)
        {
            throw std::logic_error("Reference axis removal needs one coordinate");
        }
        if (selection.sourceAxis)
        {
            const auto axis = *selection.sourceAxis;
            if (axis >= consumed.size() || consumed[axis])
            {
                throw std::logic_error("Reference source axes must be unique and in range");
            }
            consumed[axis] = true;
            for (const auto index : selection.indices)
            {
                if (index >= source.extents[axis])
                {
                    throw std::logic_error("Reference coordinate outside source extent");
                }
            }
        }
        if (selection.keepAxis)
        {
            result.extents.push_back(selection.indices.size());
        }
    }
    if (std::find(consumed.begin(), consumed.end(), false) != consumed.end())
    {
        throw std::logic_error("Reference selection must consume every source axis");
    }

    std::vector<std::size_t> coordinates(source.extents.size(), 0);
    const auto visit = [&](auto&& self, std::size_t selectionIndex) -> void {
        if (selectionIndex == selections.size())
        {
            std::size_t linearIndex = 0;
            for (std::size_t axis = 0; axis < coordinates.size(); ++axis)
            {
                linearIndex = linearIndex * source.extents[axis] + coordinates[axis];
            }
            result.offsets.push_back(source.offsets.at(linearIndex));
            return;
        }
        const auto& selection = selections[selectionIndex];
        for (const auto index : selection.indices)
        {
            if (selection.sourceAxis)
            {
                coordinates[*selection.sourceAxis] = index;
            }
            self(self, selectionIndex + 1);
        }
    };
    visit(visit, 0);
    return result;
}

// Explicitly enumerate a bounded axis and filter coordinates by endpoint and
// step congruence. No production slice-length or stride arithmetic is reused.
inline std::vector<std::size_t> smallSliceCoordinates(std::size_t extent, std::optional<int> start,
                                                     std::optional<int> stop, int step)
{
    if (extent > 16 || step == 0 || step < -8 || step > 8 ||
        (start && (*start < -32 || *start > 32)) || (stop && (*stop < -32 || *stop > 32)))
    {
        throw std::logic_error("Small slice oracle used outside its bounded domain");
    }
    const auto size = static_cast<int>(extent);
    const auto endpoint = [&](std::optional<int> value, int omitted) {
        if (!value)
        {
            return omitted;
        }
        const auto normalized = *value < 0 ? *value + size : *value;
        const auto lower = step > 0 ? 0 : -1;
        const auto upper = step > 0 ? size : size - 1;
        return std::min(upper, std::max(lower, normalized));
    };
    const auto first = endpoint(start, step > 0 ? 0 : size - 1);
    const auto last = endpoint(stop, step > 0 ? size : -1);
    std::vector<std::size_t> result;
    for (int position = 0; position < size; ++position)
    {
        const auto coordinate = step > 0 ? position : size - 1 - position;
        const bool selected = step > 0 ? coordinate >= first && coordinate < last &&
                                            (coordinate - first) % step == 0
                                      : coordinate <= first && coordinate > last &&
                                            (first - coordinate) % (-step) == 0;
        if (selected)
        {
            result.push_back(static_cast<std::size_t>(coordinate));
        }
    }
    return result;
}

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
