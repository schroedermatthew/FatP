#pragma once

/*
FATP_META:
  meta_version: 1
  component: TensorLayout
  file_role: internal_header
  path: include/fat_p/tensor/TensorLayout.h
  namespace: fat_p
  layer: Domain
  summary: "Validated pointer-free Tensor layout metadata with signed reachability."
  api_stability: in_work
  related:
    docs:
      - components/Tensor/docs/Design Note - Tensor Semantic Contract.md
      - components/Tensor/docs/Design Note - Tensor Architecture Additions Plan.md
      - components/Tensor/docs/User Manual - TensorLayout.md
    headers:
      - include/fat_p/TensorLayout.h
      - include/fat_p/tensor/TensorExtents.h
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
 * @file TensorLayout.h
 * @brief Validated pointer-free Tensor layout metadata.
 */

#include "TensorExtents.h"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

namespace fat_p
{

enum class TensorLayoutKind
{
    Empty,
    Contiguous,
    InjectiveStrided,
    Broadcast,
    Overlapping,
    Indeterminate
};

namespace tensor_detail
{

using UnsignedOffset = std::make_unsigned_t<std::ptrdiff_t>;

inline UnsignedOffset offsetMagnitude(std::ptrdiff_t value) noexcept
{
    if (value >= 0)
    {
        return static_cast<UnsignedOffset>(value);
    }
    return static_cast<UnsignedOffset>(-(value + 1)) + UnsignedOffset{1};
}

inline std::ptrdiff_t checkedOffsetAdd(std::ptrdiff_t left, std::ptrdiff_t right)
{
    constexpr auto minimum = std::numeric_limits<std::ptrdiff_t>::min();
    constexpr auto maximum = std::numeric_limits<std::ptrdiff_t>::max();
    if ((right > 0 && left > maximum - right) || (right < 0 && left < minimum - right))
    {
        throw std::overflow_error("Tensor layout offset addition exceeds ptrdiff_t");
    }
    return left + right;
}

inline std::ptrdiff_t checkedOffsetSubtract(std::ptrdiff_t left, std::ptrdiff_t right)
{
    constexpr auto minimum = std::numeric_limits<std::ptrdiff_t>::min();
    constexpr auto maximum = std::numeric_limits<std::ptrdiff_t>::max();
    if ((right > 0 && left < minimum + right) || (right < 0 && left > maximum + right))
    {
        throw std::overflow_error("Tensor layout offset subtraction exceeds ptrdiff_t");
    }
    return left - right;
}

inline std::ptrdiff_t checkedStrideContribution(std::size_t index, std::ptrdiff_t stride)
{
    if (index == 0 || stride == 0)
    {
        return 0;
    }

    constexpr auto maximum = std::numeric_limits<std::ptrdiff_t>::max();
    constexpr UnsignedOffset maximumNegativeMagnitude = static_cast<UnsignedOffset>(maximum) + UnsignedOffset{1};
    const auto magnitude = offsetMagnitude(stride);
    const auto limit = stride < 0 ? maximumNegativeMagnitude : static_cast<UnsignedOffset>(maximum);
    if (magnitude != 0 && index > static_cast<std::size_t>(limit / magnitude))
    {
        throw std::overflow_error("Tensor layout stride contribution exceeds ptrdiff_t");
    }
    const auto product = static_cast<UnsignedOffset>(index) * magnitude;
    if (stride >= 0)
    {
        return static_cast<std::ptrdiff_t>(product);
    }
    if (product == maximumNegativeMagnitude)
    {
        return std::numeric_limits<std::ptrdiff_t>::min();
    }
    return -static_cast<std::ptrdiff_t>(product);
}

inline std::ptrdiff_t checkedPositiveStrideProduct(std::ptrdiff_t value, std::size_t factor)
{
    if (value < 0)
    {
        throw std::logic_error("Canonical Tensor stride must be non-negative");
    }
    constexpr auto maximum = std::numeric_limits<std::ptrdiff_t>::max();
    if (factor != 0 && static_cast<std::size_t>(value) > static_cast<std::size_t>(maximum) / factor)
    {
        throw std::overflow_error("Canonical Tensor stride exceeds ptrdiff_t");
    }
    return static_cast<std::ptrdiff_t>(static_cast<std::size_t>(value) * factor);
}

} // namespace tensor_detail

class TensorLayout
{
public:
    TensorLayout(std::size_t storageLength, std::ptrdiff_t originOffset, DynamicExtents extents,
                 TensorStrides strides)
        : mStorageLength(storageLength)
        , mOriginOffset(originOffset)
        , mExtents(std::move(extents))
        , mStrides(std::move(strides))
    {
        validateAndClassify();
    }

    [[nodiscard]] static TensorLayout contiguous(DynamicExtents extents)
    {
        const auto storageLength = extents.logicalSize();
        return TensorLayout(storageLength, 0, extents, canonicalStrides(extents));
    }

    [[nodiscard]] static TensorStrides canonicalStrides(const DynamicExtents& extents)
    {
        TensorStrides result(extents.rank(), 0);
        std::ptrdiff_t runningStride = 1;
        for (std::size_t reverseAxis = extents.rank(); reverseAxis > 0; --reverseAxis)
        {
            const auto axis = reverseAxis - 1;
            if (extents[axis] == 0 || runningStride == 0)
            {
                result[axis] = 0;
                runningStride = 0;
                continue;
            }
            result[axis] = runningStride;
            if (axis > 0 && extents[axis - 1] != 0)
            {
                runningStride = tensor_detail::checkedPositiveStrideProduct(runningStride, extents[axis]);
            }
        }
        return result;
    }

    [[nodiscard]] std::size_t storageLength() const noexcept
    {
        return mStorageLength;
    }

    [[nodiscard]] std::ptrdiff_t originOffset() const noexcept
    {
        return mOriginOffset;
    }

    [[nodiscard]] const DynamicExtents& extents() const noexcept
    {
        return mExtents;
    }

    [[nodiscard]] const TensorStrides& strides() const noexcept
    {
        return mStrides;
    }

    [[nodiscard]] std::size_t rank() const noexcept
    {
        return mExtents.rank();
    }

    [[nodiscard]] std::size_t logicalSize() const noexcept
    {
        return mExtents.logicalSize();
    }

    [[nodiscard]] TensorLayoutKind kind() const noexcept
    {
        return mKind;
    }

    [[nodiscard]] bool isEmpty() const noexcept
    {
        return mKind == TensorLayoutKind::Empty;
    }

    [[nodiscard]] bool isContiguous() const noexcept
    {
        return mKind == TensorLayoutKind::Empty || mKind == TensorLayoutKind::Contiguous;
    }

    [[nodiscard]] bool isInjective() const noexcept
    {
        return mKind == TensorLayoutKind::Empty || mKind == TensorLayoutKind::Contiguous ||
            mKind == TensorLayoutKind::InjectiveStrided;
    }

    [[nodiscard]] bool isBroadcast() const noexcept
    {
        return mKind == TensorLayoutKind::Broadcast;
    }

    [[nodiscard]] bool isOverlapping() const noexcept
    {
        return mKind == TensorLayoutKind::Broadcast || mKind == TensorLayoutKind::Overlapping;
    }

    [[nodiscard]] bool isIndeterminate() const noexcept
    {
        return mKind == TensorLayoutKind::Indeterminate;
    }

    friend bool operator==(const TensorLayout&, const TensorLayout&) = default;

    [[nodiscard]] const std::optional<std::ptrdiff_t>& minimumOffset() const noexcept
    {
        return mMinimumOffset;
    }

    [[nodiscard]] const std::optional<std::ptrdiff_t>& maximumOffset() const noexcept
    {
        return mMaximumOffset;
    }

    [[nodiscard]] std::ptrdiff_t logicalOffset(std::size_t linearIndex) const
    {
        if (linearIndex >= logicalSize())
        {
            throw std::out_of_range("Tensor logical linear index is out of range");
        }
        std::size_t remainder = linearIndex;
        std::ptrdiff_t offset = mOriginOffset;
        for (std::size_t reverseAxis = rank(); reverseAxis > 0; --reverseAxis)
        {
            const auto axis = reverseAxis - 1;
            const auto index = remainder % mExtents[axis];
            remainder /= mExtents[axis];
            offset = tensor_detail::checkedOffsetAdd(
                offset, tensor_detail::checkedStrideContribution(index, mStrides[axis]));
        }
        return offset;
    }

private:
    void validateAndClassify()
    {
        if (mExtents.rank() != mStrides.size())
        {
            throw std::invalid_argument("Tensor extents and strides must have the same rank");
        }
        constexpr auto maximumOffset = static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max());
        if (mStorageLength > maximumOffset)
        {
            throw std::overflow_error("Tensor storage length exceeds ptrdiff_t");
        }
        if (mOriginOffset < 0 || static_cast<std::size_t>(mOriginOffset) > mStorageLength)
        {
            throw std::out_of_range("Tensor layout origin is outside the storage span");
        }

        if (logicalSize() == 0)
        {
            mKind = TensorLayoutKind::Empty;
            return;
        }
        if (mStorageLength == 0 || static_cast<std::size_t>(mOriginOffset) == mStorageLength)
        {
            throw std::out_of_range("Nonempty Tensor layout origin must address storage");
        }

        std::ptrdiff_t minimum = mOriginOffset;
        std::ptrdiff_t maximum = mOriginOffset;
        for (std::size_t axis = 0; axis < rank(); ++axis)
        {
            const auto contribution =
                tensor_detail::checkedStrideContribution(mExtents[axis] - std::size_t{1}, mStrides[axis]);
            if (contribution < 0)
            {
                minimum = tensor_detail::checkedOffsetAdd(minimum, contribution);
            }
            else
            {
                maximum = tensor_detail::checkedOffsetAdd(maximum, contribution);
            }
        }
        if (minimum < 0 || maximum < 0 || static_cast<std::size_t>(maximum) >= mStorageLength)
        {
            throw std::out_of_range("Tensor layout reaches outside the storage span");
        }
        mMinimumOffset = minimum;
        mMaximumOffset = maximum;

        if (hasCanonicalContiguousMapping())
        {
            mKind = TensorLayoutKind::Contiguous;
        }
        else if (hasExpandedZeroStride())
        {
            mKind = TensorLayoutKind::Broadcast;
        }
        else
        {
            mKind = classifyInjectivity();
        }
    }

    [[nodiscard]] bool hasCanonicalContiguousMapping() const
    {
        std::ptrdiff_t expectedStride = 1;
        for (std::size_t reverseAxis = rank(); reverseAxis > 0; --reverseAxis)
        {
            const auto axis = reverseAxis - 1;
            if (mExtents[axis] > 1 && mStrides[axis] != expectedStride)
            {
                return false;
            }
            expectedStride = tensor_detail::checkedPositiveStrideProduct(expectedStride, mExtents[axis]);
        }
        return true;
    }

    [[nodiscard]] bool hasExpandedZeroStride() const noexcept
    {
        for (std::size_t axis = 0; axis < rank(); ++axis)
        {
            if (mExtents[axis] > 1 && mStrides[axis] == 0)
            {
                return true;
            }
        }
        return false;
    }

    struct ActiveAxis
    {
        tensor_detail::UnsignedOffset magnitude;
        std::size_t extent;
    };

    [[nodiscard]] static bool pairOverlaps(const ActiveAxis& left, const ActiveAxis& right) noexcept
    {
        const auto divisor = std::gcd(left.magnitude, right.magnitude);
        const auto leftStep = right.magnitude / divisor;
        const auto rightStep = left.magnitude / divisor;
        return leftStep < left.extent && rightStep < right.extent;
    }

    [[nodiscard]] TensorLayoutKind classifyInjectivity() const
    {
        std::vector<ActiveAxis> activeAxes;
        activeAxes.reserve(rank());
        for (std::size_t axis = 0; axis < rank(); ++axis)
        {
            if (mExtents[axis] > 1)
            {
                activeAxes.push_back({tensor_detail::offsetMagnitude(mStrides[axis]), mExtents[axis]});
            }
        }

        if (activeAxes.size() <= 1)
        {
            return TensorLayoutKind::InjectiveStrided;
        }
        if (activeAxes.size() == 2)
        {
            return pairOverlaps(activeAxes[0], activeAxes[1]) ? TensorLayoutKind::Overlapping
                                                              : TensorLayoutKind::InjectiveStrided;
        }

        // Exact enumeration is deliberately bounded so metadata-only view
        // transforms do not perform work proportional to a large tensor.
        constexpr std::size_t exactLimit = 8'192;
        if (logicalSize() <= exactLimit)
        {
            std::unordered_set<std::ptrdiff_t> offsets;
            offsets.reserve(logicalSize());
            for (std::size_t linearIndex = 0; linearIndex < logicalSize(); ++linearIndex)
            {
                if (!offsets.insert(logicalOffset(linearIndex)).second)
                {
                    return TensorLayoutKind::Overlapping;
                }
            }
            return TensorLayoutKind::InjectiveStrided;
        }

        std::sort(activeAxes.begin(), activeAxes.end(), [](const ActiveAxis& left, const ActiveAxis& right) {
            return std::tie(left.magnitude, left.extent) < std::tie(right.magnitude, right.extent);
        });

        tensor_detail::UnsignedOffset coveredSpan = 0;
        bool greedyProof = true;
        for (const auto& axis : activeAxes)
        {
            if (axis.magnitude <= coveredSpan)
            {
                greedyProof = false;
                break;
            }
            const auto contribution = static_cast<tensor_detail::UnsignedOffset>(axis.extent - 1) * axis.magnitude;
            coveredSpan += contribution;
        }
        if (greedyProof)
        {
            return TensorLayoutKind::InjectiveStrided;
        }

        // A bounded two-axis collision is a constructive proof of overlap.
        // If neither that nor the packing proof decides a large higher-rank
        // mapping, report uncertainty instead of falsely calling it overlap.
        for (std::size_t left = 0; left < activeAxes.size(); ++left)
        {
            for (std::size_t right = left + 1; right < activeAxes.size(); ++right)
            {
                if (pairOverlaps(activeAxes[left], activeAxes[right]))
                {
                    return TensorLayoutKind::Overlapping;
                }
            }
        }
        return TensorLayoutKind::Indeterminate;
    }

    std::size_t mStorageLength = 0;
    std::ptrdiff_t mOriginOffset = 0;
    DynamicExtents mExtents;
    TensorStrides mStrides;
    TensorLayoutKind mKind = TensorLayoutKind::Empty;
    std::optional<std::ptrdiff_t> mMinimumOffset;
    std::optional<std::ptrdiff_t> mMaximumOffset;
};

} // namespace fat_p
