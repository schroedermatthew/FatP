#pragma once

/*
FATP_META:
  meta_version: 1
  component: TensorReductions
  file_role: internal_header
  path: include/fat_p/tensor/TensorReductions.h
  namespace: fat_p
  layer: Domain
  summary: "Deterministic serial axis reductions for readable Tensor mappings."
  api_stability: in_work
  related:
    docs:
      - components/Tensor/docs/Design Note - Tensor Architecture Additions Plan.md
    headers:
      - include/fat_p/TensorReductions.h
      - include/fat_p/tensor/TensorAlgorithms.h
    tests:
      - components/Tensor/tests/test_TensorReductions.cpp
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

/** @file TensorReductions.h @brief Checked deterministic reductions over selected axes. */

#include <algorithm>
#include <array>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include "TensorAlgorithms.h"

namespace fat_p
{

/** @brief Sum/product accumulator: bool counts in size_t, narrow integers widen to 64 bits. */
template <typename T>
using TensorSumType = std::conditional_t<
    std::same_as<std::remove_cv_t<T>, bool>, std::size_t,
    std::conditional_t<std::integral<T> && (sizeof(T) < sizeof(std::int64_t)),
                       std::conditional_t<std::signed_integral<T>, std::int64_t, std::uint64_t>, T>>;

/** @brief Mean accumulator and result: double, except long double remains long double. */
template <typename T>
using TensorMeanType = std::conditional_t<std::same_as<std::remove_cv_t<T>, long double>, long double, double>;

namespace tensor_detail
{

struct ReductionShape
{
    std::vector<std::size_t> axes;
    std::vector<bool> reduced;
    DynamicExtents outputExtents;
    bool keepDimensions = false;
};

template <typename Extents, typename RequestedAxes>
inline ReductionShape makeReductionShape(const Extents& source, const RequestedAxes& requested,
                                         bool keepDimensions)
{
    std::vector<std::size_t> axes;
    if (requested.empty())
    {
        axes.resize(source.rank());
        std::iota(axes.begin(), axes.end(), std::size_t{0});
    }
    else
    {
        const auto normalized = normalizeAxes(requested, source.rank());
        axes.assign(normalized.begin(), normalized.end());
        std::sort(axes.begin(), axes.end());
    }

    std::vector<bool> reduced(source.rank(), false);
    for (const auto axis : axes)
    {
        reduced[axis] = true;
    }
    std::vector<std::size_t> output;
    for (std::size_t axis = 0; axis < source.rank(); ++axis)
    {
        if (reduced[axis])
        {
            if (keepDimensions)
            {
                output.push_back(1);
            }
        }
        else
        {
            output.push_back(source[axis]);
        }
    }
    return {std::move(axes), std::move(reduced), DynamicExtents(std::move(output)), keepDimensions};
}

template <typename Extents>
inline std::size_t reductionElementCount(const Extents& source, const ReductionShape& shape)
{
    std::vector<std::size_t> extents;
    extents.reserve(shape.axes.size());
    for (const auto axis : shape.axes)
    {
        extents.push_back(source[axis]);
    }
    return checkedLogicalSize(extents);
}

struct ReductionCoordinates
{
    std::size_t outputLinear = 0;
    std::size_t reducedLinear = 0;
};

template <typename Extents, typename Coordinates>
inline ReductionCoordinates decodeReductionCoordinates(std::size_t linearIndex, const Extents& source,
                                                       const ReductionShape& shape, Coordinates& coordinates)
{
    std::fill(coordinates.begin(), coordinates.end(), std::size_t{0});
    auto remainder = linearIndex;
    for (std::size_t reverseAxis = source.rank(); reverseAxis > 0; --reverseAxis)
    {
        const auto axis = reverseAxis - 1;
        coordinates[axis] = remainder % source[axis];
        remainder /= source[axis];
    }

    ReductionCoordinates result;
    for (std::size_t axis = 0; axis < source.rank(); ++axis)
    {
        if (shape.reduced[axis])
        {
            result.reducedLinear = result.reducedLinear * source[axis] + coordinates[axis];
        }
        else
        {
            result.outputLinear = result.outputLinear * source[axis] + coordinates[axis];
        }
    }
    return result;
}

template <std::size_t OutputRank = kDynamicTensorRank, ReadableTensor Source, typename Axes,
          typename Result, typename Allocator, typename Combine>
[[nodiscard]] Tensor<Result, Allocator, OutputRank>
reduceInitialized(const Source& source, const Axes& axes, bool keepDimensions,
                  Result initial, Combine&& combine, const Allocator& allocator)
{
    tensor_detail::TensorAccess::validate(source);
    const auto shape = makeReductionShape(source.extents(), axes, keepDimensions);
    auto outputExtents = [&] {
        if constexpr (OutputRank == kDynamicTensorRank)
        {
            return shape.outputExtents;
        }
        else
        {
            return makeFixedExtents<OutputRank>(shape.outputExtents);
        }
    }();
    Tensor<Result, Allocator, OutputRank> result(std::allocator_arg, allocator, std::move(outputExtents), initial);
    if (source.empty() || result.empty())
    {
        return result;
    }
    const auto* input = TensorAccess::storageBase(source);
    const auto plan = makeTensorIterationPlan(source.extents(), source.layout());
    using coordinates_type = std::conditional_t<tensorStaticRankValue<Source> == kDynamicTensorRank,
                                                std::vector<std::size_t>,
                                                std::array<std::size_t, tensorStaticRankValue<Source>>>;
    coordinates_type coordinates{};
    if constexpr (tensorStaticRankValue<Source> == kDynamicTensorRank)
    {
        coordinates.resize(source.rank(), 0);
    }
    plan.forEachOffset([&](std::size_t linearIndex, const auto& offsets) {
        const auto output =
            decodeReductionCoordinates(linearIndex, source.extents(), shape, coordinates).outputLinear;
        result[output] = std::invoke(combine, result[output], static_cast<Result>(input[offsets[0]]));
    });
    return result;
}

template <std::size_t OutputRank = kDynamicTensorRank, ReadableTensor Source, typename Axes,
          typename Allocator, typename Compare>
[[nodiscard]] Tensor<typename Source::value_type, Allocator, OutputRank>
reduceExtremum(const Source& source, const Axes& axes, bool keepDimensions,
              std::optional<typename Source::value_type> initial, Compare&& compare,
              const Allocator& allocator)
{
    using value_type = typename Source::value_type;
    TensorAccess::validate(source);
    const auto shape = makeReductionShape(source.extents(), axes, keepDimensions);
    auto outputExtents = [&] {
        if constexpr (OutputRank == kDynamicTensorRank)
        {
            return shape.outputExtents;
        }
        else
        {
            return makeFixedExtents<OutputRank>(shape.outputExtents);
        }
    }();
    Tensor<value_type, Allocator, OutputRank> result(std::allocator_arg, allocator,
                                                     std::move(outputExtents));
    if (result.empty())
    {
        return result;
    }
    std::vector<bool> initialized(result.size(), initial.has_value());
    if (initial)
    {
        result.fill(*initial);
    }
    if (source.empty())
    {
        if (!initial)
        {
            throw std::domain_error("Tensor extremum reduction has an empty domain");
        }
        return result;
    }

    const auto* input = TensorAccess::storageBase(source);
    const auto plan = makeTensorIterationPlan(source.extents(), source.layout());
    using coordinates_type = std::conditional_t<tensorStaticRankValue<Source> == kDynamicTensorRank,
                                                std::vector<std::size_t>,
                                                std::array<std::size_t, tensorStaticRankValue<Source>>>;
    coordinates_type coordinates{};
    if constexpr (tensorStaticRankValue<Source> == kDynamicTensorRank)
    {
        coordinates.resize(source.rank(), 0);
    }
    plan.forEachOffset([&](std::size_t linearIndex, const auto& offsets) {
        const auto output =
            decodeReductionCoordinates(linearIndex, source.extents(), shape, coordinates).outputLinear;
        const auto& candidate = input[offsets[0]];
        bool replace = !initialized[output];
        if (!replace)
        {
            if constexpr (std::floating_point<value_type>)
            {
                replace = std::isnan(candidate) ? !std::isnan(result[output])
                                                : (!std::isnan(result[output]) &&
                                                   std::invoke(compare, candidate, result[output]));
            }
            else
            {
                replace = std::invoke(compare, candidate, result[output]);
            }
        }
        if (replace)
        {
            result[output] = candidate;
            initialized[output] = true;
        }
    });
    return result;
}

template <std::size_t OutputRank = kDynamicTensorRank, ReadableTensor Source, typename Axes,
          typename Allocator, typename Compare>
[[nodiscard]] Tensor<std::size_t, Allocator, OutputRank>
reduceArgExtremum(const Source& source, const Axes& axes, bool keepDimensions,
                 Compare&& compare, const Allocator& allocator)
{
    using value_type = typename Source::value_type;
    TensorAccess::validate(source);
    const auto shape = makeReductionShape(source.extents(), axes, keepDimensions);
    auto outputExtents = [&] {
        if constexpr (OutputRank == kDynamicTensorRank)
        {
            return shape.outputExtents;
        }
        else
        {
            return makeFixedExtents<OutputRank>(shape.outputExtents);
        }
    }();
    Tensor<std::size_t, Allocator, OutputRank> result(std::allocator_arg, allocator,
                                                      std::move(outputExtents), 0);
    if (result.empty())
    {
        return result;
    }
    if (source.empty())
    {
        throw std::domain_error("Tensor arg-extremum reduction has an empty domain");
    }
    std::vector<value_type> best(result.size());
    std::vector<bool> initialized(result.size(), false);
    const auto* input = TensorAccess::storageBase(source);
    const auto plan = makeTensorIterationPlan(source.extents(), source.layout());
    using coordinates_type = std::conditional_t<tensorStaticRankValue<Source> == kDynamicTensorRank,
                                                std::vector<std::size_t>,
                                                std::array<std::size_t, tensorStaticRankValue<Source>>>;
    coordinates_type coordinateBuffer{};
    if constexpr (tensorStaticRankValue<Source> == kDynamicTensorRank)
    {
        coordinateBuffer.resize(source.rank(), 0);
    }
    plan.forEachOffset([&](std::size_t linearIndex, const auto& offsets) {
        const auto coordinates =
            decodeReductionCoordinates(linearIndex, source.extents(), shape, coordinateBuffer);
        const auto& candidate = input[offsets[0]];
        bool replace = !initialized[coordinates.outputLinear];
        if (!replace)
        {
            if constexpr (std::floating_point<value_type>)
            {
                replace = std::isnan(candidate) ? !std::isnan(best[coordinates.outputLinear])
                                                : (!std::isnan(best[coordinates.outputLinear]) &&
                                                   std::invoke(compare, candidate,
                                                               best[coordinates.outputLinear]));
            }
            else
            {
                replace = std::invoke(compare, candidate, best[coordinates.outputLinear]);
            }
        }
        if (replace)
        {
            best[coordinates.outputLinear] = candidate;
            result[coordinates.outputLinear] = coordinates.reducedLinear;
            initialized[coordinates.outputLinear] = true;
        }
    });
    return result;
}

template <typename Source>
concept ArithmeticTensor =
    ReadableTensor<Source> && std::is_arithmetic_v<typename Source::value_type>;

} // namespace tensor_detail

/**
 * @brief Reduce arithmetic values in logical row-major order into a new owning tensor.
 * @details An empty axis list means all axes. Negative axes normalize against source rank;
 * duplicates are invalid. keepDimensions retains reduced axes as singletons. Each domain folds
 * from initial (zero by default) in TensorSumType; each integral addition is checked before
 * evaluation, including intermediate overflow. Floating arithmetic uses the accumulator type's
 * ordinary operations, without compensation or a cross-platform bitwise reproducibility promise.
 * Source storage is never modified. Explicit allocators are used unchanged; default overloads
 * use rebound owner SOCCC or TensorAllocator for views. Metadata may allocate separately.
 * @throws std::overflow_error If integral accumulation or result shape is not representable.
 * @throws std::invalid_argument If an axis is repeated.
 * @throws std::out_of_range If an axis is outside the source rank.
 * @throws std::runtime_error For expired borrowed sources in assertions-enabled builds.
 */
template <tensor_detail::ArithmeticTensor Source, typename Allocator>
    requires tensor_detail::AllocatorFor<Allocator, TensorSumType<typename Source::value_type>>
[[nodiscard]] auto sum(const Source& source, const Allocator& allocator,
                       const std::vector<TensorAxis>& axes, bool keepDimensions = false,
                       std::optional<TensorSumType<typename Source::value_type>> initial = std::nullopt)
{
    using result_type = TensorSumType<typename Source::value_type>;
    return tensor_detail::reduceInitialized(source, axes, keepDimensions, initial.value_or(result_type{0}),
                                            tensor_detail::checkedSameTypeAdd<result_type>, allocator);
}

template <tensor_detail::ArithmeticTensor Source>
[[nodiscard]] auto sum(const Source& source, const std::vector<TensorAxis>& axes, bool keepDimensions = false,
                       std::optional<TensorSumType<typename Source::value_type>> initial = std::nullopt)
{
    using result_type = TensorSumType<typename Source::value_type>;
    return sum(source, tensor_detail::selectResultAllocator<result_type>(source), axes, keepDimensions, initial);
}

/** @brief Product uses the sum accumulator type, checked integral multiplication, and initial one. */
template <tensor_detail::ArithmeticTensor Source, typename Allocator>
    requires tensor_detail::AllocatorFor<Allocator, TensorSumType<typename Source::value_type>>
[[nodiscard]] auto product(const Source& source, const Allocator& allocator,
                           const std::vector<TensorAxis>& axes, bool keepDimensions = false,
                           std::optional<TensorSumType<typename Source::value_type>> initial = std::nullopt)
{
    using result_type = TensorSumType<typename Source::value_type>;
    return tensor_detail::reduceInitialized(source, axes, keepDimensions, initial.value_or(result_type{1}),
                                            tensor_detail::checkedSameTypeMultiply<result_type>, allocator);
}

template <tensor_detail::ArithmeticTensor Source>
[[nodiscard]] auto product(const Source& source, const std::vector<TensorAxis>& axes,
                           bool keepDimensions = false,
                           std::optional<TensorSumType<typename Source::value_type>> initial = std::nullopt)
{
    using result_type = TensorSumType<typename Source::value_type>;
    return product(source, tensor_detail::selectResultAllocator<result_type>(source), axes, keepDimensions,
                   initial);
}

/**
 * @brief Convert each input to TensorMeanType, sum serially, then divide by the domain count.
 * @details Integral-to-floating conversion may round; no exact-integer or compensated mean is promised.
 * @throws std::domain_error If a nonempty output contains an empty reduction domain.
 */
template <tensor_detail::ArithmeticTensor Source, typename Allocator>
    requires tensor_detail::AllocatorFor<Allocator, TensorMeanType<typename Source::value_type>>
[[nodiscard]] auto mean(const Source& source, const Allocator& allocator,
                        const std::vector<TensorAxis>& axes, bool keepDimensions = false)
{
    using result_type = TensorMeanType<typename Source::value_type>;
    tensor_detail::TensorAccess::validate(source);
    const auto shape = tensor_detail::makeReductionShape(source.extents(), axes, keepDimensions);
    if (shape.outputExtents.logicalSize() == 0)
    {
        return Tensor<result_type, Allocator>(std::allocator_arg, allocator, shape.outputExtents);
    }
    const auto count = tensor_detail::reductionElementCount(source.extents(), shape);
    if (count == 0)
    {
        throw std::domain_error("Tensor mean reduction has an empty domain");
    }
    auto result = tensor_detail::reduceInitialized(source, axes, keepDimensions, result_type{0},
                                                   std::plus<result_type>{}, allocator);
    if (count != 0)
    {
        for (auto& value : result)
        {
            value /= static_cast<result_type>(count);
        }
    }
    return result;
}

template <tensor_detail::ArithmeticTensor Source>
[[nodiscard]] auto mean(const Source& source, const std::vector<TensorAxis>& axes, bool keepDimensions = false)
{
    using result_type = TensorMeanType<typename Source::value_type>;
    return mean(source, tensor_detail::selectResultAllocator<result_type>(source), axes, keepDimensions);
}

/** @brief Minimum preserves the first tie or NaN; an initial value participates before source values. */
template <tensor_detail::ArithmeticTensor Source, typename Allocator>
    requires tensor_detail::AllocatorFor<Allocator, typename Source::value_type>
[[nodiscard]] auto min(const Source& source, const Allocator& allocator,
                       const std::vector<TensorAxis>& axes, bool keepDimensions = false,
                       std::optional<typename Source::value_type> initial = std::nullopt)
{
    return tensor_detail::reduceExtremum(source, axes, keepDimensions, initial, std::less<>{}, allocator);
}

template <tensor_detail::ArithmeticTensor Source>
[[nodiscard]] auto min(const Source& source, const std::vector<TensorAxis>& axes, bool keepDimensions = false,
                       std::optional<typename Source::value_type> initial = std::nullopt)
{
    return min(source, tensor_detail::selectResultAllocator<typename Source::value_type>(source), axes,
               keepDimensions, initial);
}

/** @brief Maximum preserves the first tie or NaN; an initial value also defines an empty domain. */
template <tensor_detail::ArithmeticTensor Source, typename Allocator>
    requires tensor_detail::AllocatorFor<Allocator, typename Source::value_type>
[[nodiscard]] auto max(const Source& source, const Allocator& allocator,
                       const std::vector<TensorAxis>& axes, bool keepDimensions = false,
                       std::optional<typename Source::value_type> initial = std::nullopt)
{
    return tensor_detail::reduceExtremum(source, axes, keepDimensions, initial, std::greater<>{}, allocator);
}

template <tensor_detail::ArithmeticTensor Source>
[[nodiscard]] auto max(const Source& source, const std::vector<TensorAxis>& axes, bool keepDimensions = false,
                       std::optional<typename Source::value_type> initial = std::nullopt)
{
    return max(source, tensor_detail::selectResultAllocator<typename Source::value_type>(source), axes,
               keepDimensions, initial);
}

/**
 * @brief Index of the first minimum/NaN in row-major order of reduced axes, sorted by source axis.
 * @details No initial value is accepted: every reported index must refer to a source coordinate.
 */
template <tensor_detail::ArithmeticTensor Source, typename Allocator>
    requires tensor_detail::AllocatorFor<Allocator, std::size_t>
[[nodiscard]] auto argmin(const Source& source, const Allocator& allocator,
                          const std::vector<TensorAxis>& axes, bool keepDimensions = false)
{
    return tensor_detail::reduceArgExtremum(source, axes, keepDimensions, std::less<>{}, allocator);
}

template <tensor_detail::ArithmeticTensor Source>
[[nodiscard]] auto argmin(const Source& source, const std::vector<TensorAxis>& axes,
                          bool keepDimensions = false)
{
    return argmin(source, tensor_detail::selectResultAllocator<std::size_t>(source), axes, keepDimensions);
}

/**
 * @brief Index of the first maximum/NaN; empty domains with a nonempty output throw domain_error.
 * @details No initial value is accepted: every reported index must refer to a source coordinate.
 */
template <tensor_detail::ArithmeticTensor Source, typename Allocator>
    requires tensor_detail::AllocatorFor<Allocator, std::size_t>
[[nodiscard]] auto argmax(const Source& source, const Allocator& allocator,
                          const std::vector<TensorAxis>& axes, bool keepDimensions = false)
{
    return tensor_detail::reduceArgExtremum(source, axes, keepDimensions, std::greater<>{}, allocator);
}

template <tensor_detail::ArithmeticTensor Source>
[[nodiscard]] auto argmax(const Source& source, const std::vector<TensorAxis>& axes,
                          bool keepDimensions = false)
{
    return argmax(source, tensor_detail::selectResultAllocator<std::size_t>(source), axes, keepDimensions);
}

/**
 * @brief Boolean conjunction over arithmetic inputs, with true for an empty domain.
 * @details Each value converts to bool: signed zeros are false; NaNs and infinities are true.
 * Uses the serial reduction kernel without a short-circuit or allocation-free guarantee.
 */
template <tensor_detail::ArithmeticTensor Source, typename Allocator>
    requires tensor_detail::AllocatorFor<Allocator, bool>
[[nodiscard]] auto all(const Source& source, const Allocator& allocator,
                       const std::vector<TensorAxis>& axes, bool keepDimensions = false)
{
    return tensor_detail::reduceInitialized(source, axes, keepDimensions, true, std::logical_and<bool>{}, allocator);
}

template <tensor_detail::ArithmeticTensor Source>
[[nodiscard]] auto all(const Source& source, const std::vector<TensorAxis>& axes, bool keepDimensions = false)
{
    return all(source, tensor_detail::selectResultAllocator<bool>(source), axes, keepDimensions);
}

/** @brief Boolean disjunction with the same truth conversion as all, and false for an empty domain. */
template <tensor_detail::ArithmeticTensor Source, typename Allocator>
    requires tensor_detail::AllocatorFor<Allocator, bool>
[[nodiscard]] auto any(const Source& source, const Allocator& allocator,
                       const std::vector<TensorAxis>& axes, bool keepDimensions = false)
{
    return tensor_detail::reduceInitialized(source, axes, keepDimensions, false, std::logical_or<bool>{}, allocator);
}

template <tensor_detail::ArithmeticTensor Source>
[[nodiscard]] auto any(const Source& source, const std::vector<TensorAxis>& axes, bool keepDimensions = false)
{
    return any(source, tensor_detail::selectResultAllocator<bool>(source), axes, keepDimensions);
}

namespace tensor_detail
{

template <std::size_t Rank, TensorAxis... Axes>
consteval bool validStaticReductionAxes()
{
    if constexpr (sizeof...(Axes) == 0)
    {
        return true;
    }
    else
    {
        std::array<std::size_t, sizeof...(Axes)> normalized{};
        std::size_t index = 0;
        bool valid = true;
        const auto append = [&](TensorAxis axis) {
            const auto signedRank = static_cast<TensorAxis>(Rank);
            const auto value = axis < 0 ? axis + signedRank : axis;
            if (value < 0 || value >= signedRank)
            {
                valid = false;
                return;
            }
            const auto converted = static_cast<std::size_t>(value);
            for (std::size_t prior = 0; prior < index; ++prior)
            {
                if (normalized[prior] == converted)
                {
                    valid = false;
                }
            }
            normalized[index++] = converted;
        };
        (append(Axes), ...);
        return valid;
    }
}

template <bool KeepDimensions, typename Source, TensorAxis... Axes>
inline constexpr std::size_t staticReductionOutputRank = [] {
    constexpr auto sourceRank = tensorStaticRankValue<Source>;
    static_assert(sourceRank != kDynamicTensorRank,
                  "Compile-time reduction axes require a fixed-rank Tensor source");
    static_assert(validStaticReductionAxes<sourceRank, Axes...>(),
                  "A compile-time Tensor reduction axis is invalid or duplicated");
    constexpr auto reducedCount = sizeof...(Axes) == 0 ? sourceRank : sizeof...(Axes);
    return KeepDimensions ? sourceRank : sourceRank - reducedCount;
}();

template <TensorAxis... Axes>
[[nodiscard]] constexpr std::array<TensorAxis, sizeof...(Axes)> staticAxes()
{
    return {Axes...};
}

} // namespace tensor_detail

template <bool KeepDimensions, TensorAxis... Axes, ReadableTensor Source, typename Allocator>
    requires std::is_arithmetic_v<typename Source::value_type> &&
             (tensor_detail::tensorStaticRankValue<Source> != tensor_detail::kDynamicTensorRank) &&
             tensor_detail::AllocatorFor<Allocator, TensorSumType<typename Source::value_type>>
[[nodiscard]] auto sum(const Source& source, const Allocator& allocator,
                       std::optional<TensorSumType<typename Source::value_type>> initial = std::nullopt)
{
    using result_type = TensorSumType<typename Source::value_type>;
    constexpr auto outputRank = tensor_detail::staticReductionOutputRank<KeepDimensions, Source, Axes...>;
    return tensor_detail::reduceInitialized<outputRank>(
        source, tensor_detail::staticAxes<Axes...>(), KeepDimensions, initial.value_or(result_type{0}),
        tensor_detail::checkedSameTypeAdd<result_type>, allocator);
}

template <bool KeepDimensions, TensorAxis... Axes, ReadableTensor Source>
    requires std::is_arithmetic_v<typename Source::value_type> &&
             (tensor_detail::tensorStaticRankValue<Source> != tensor_detail::kDynamicTensorRank)
[[nodiscard]] auto sum(const Source& source,
                       std::optional<TensorSumType<typename Source::value_type>> initial = std::nullopt)
{
    using result_type = TensorSumType<typename Source::value_type>;
    return sum<KeepDimensions, Axes...>(source, tensor_detail::selectResultAllocator<result_type>(source), initial);
}

template <bool KeepDimensions, TensorAxis... Axes, ReadableTensor Source, typename Allocator>
    requires std::is_arithmetic_v<typename Source::value_type> &&
             (tensor_detail::tensorStaticRankValue<Source> != tensor_detail::kDynamicTensorRank) &&
             tensor_detail::AllocatorFor<Allocator, TensorSumType<typename Source::value_type>>
[[nodiscard]] auto product(const Source& source, const Allocator& allocator,
                           std::optional<TensorSumType<typename Source::value_type>> initial = std::nullopt)
{
    using result_type = TensorSumType<typename Source::value_type>;
    constexpr auto outputRank = tensor_detail::staticReductionOutputRank<KeepDimensions, Source, Axes...>;
    return tensor_detail::reduceInitialized<outputRank>(
        source, tensor_detail::staticAxes<Axes...>(), KeepDimensions, initial.value_or(result_type{1}),
        tensor_detail::checkedSameTypeMultiply<result_type>, allocator);
}

template <bool KeepDimensions, TensorAxis... Axes, ReadableTensor Source>
    requires std::is_arithmetic_v<typename Source::value_type> &&
             (tensor_detail::tensorStaticRankValue<Source> != tensor_detail::kDynamicTensorRank)
[[nodiscard]] auto product(const Source& source,
                           std::optional<TensorSumType<typename Source::value_type>> initial = std::nullopt)
{
    using result_type = TensorSumType<typename Source::value_type>;
    return product<KeepDimensions, Axes...>(
        source, tensor_detail::selectResultAllocator<result_type>(source), initial);
}

template <bool KeepDimensions, TensorAxis... Axes, ReadableTensor Source, typename Allocator>
    requires std::is_arithmetic_v<typename Source::value_type> &&
             (tensor_detail::tensorStaticRankValue<Source> != tensor_detail::kDynamicTensorRank) &&
             tensor_detail::AllocatorFor<Allocator, TensorMeanType<typename Source::value_type>>
[[nodiscard]] auto mean(const Source& source, const Allocator& allocator)
{
    using result_type = TensorMeanType<typename Source::value_type>;
    constexpr auto outputRank = tensor_detail::staticReductionOutputRank<KeepDimensions, Source, Axes...>;
    const auto requested = tensor_detail::staticAxes<Axes...>();
    const auto shape = tensor_detail::makeReductionShape(source.extents(), requested, KeepDimensions);
    const auto count = tensor_detail::reductionElementCount(source.extents(), shape);
    if (shape.outputExtents.logicalSize() != 0 && count == 0)
    {
        throw std::domain_error("Tensor mean reduction has an empty domain");
    }
    auto result = tensor_detail::reduceInitialized<outputRank>(source, requested, KeepDimensions,
                                                               result_type{0}, std::plus<result_type>{}, allocator);
    for (auto& value : result)
    {
        value /= static_cast<result_type>(count);
    }
    return result;
}

template <bool KeepDimensions, TensorAxis... Axes, ReadableTensor Source>
    requires std::is_arithmetic_v<typename Source::value_type> &&
             (tensor_detail::tensorStaticRankValue<Source> != tensor_detail::kDynamicTensorRank)
[[nodiscard]] auto mean(const Source& source)
{
    using result_type = TensorMeanType<typename Source::value_type>;
    return mean<KeepDimensions, Axes...>(source, tensor_detail::selectResultAllocator<result_type>(source));
}

template <bool KeepDimensions, TensorAxis... Axes, ReadableTensor Source, typename Allocator>
    requires std::is_arithmetic_v<typename Source::value_type> &&
             (tensor_detail::tensorStaticRankValue<Source> != tensor_detail::kDynamicTensorRank) &&
             tensor_detail::AllocatorFor<Allocator, typename Source::value_type>
[[nodiscard]] auto min(const Source& source, const Allocator& allocator,
                       std::optional<typename Source::value_type> initial = std::nullopt)
{
    constexpr auto outputRank = tensor_detail::staticReductionOutputRank<KeepDimensions, Source, Axes...>;
    return tensor_detail::reduceExtremum<outputRank>(source, tensor_detail::staticAxes<Axes...>(),
                                                     KeepDimensions, initial, std::less<>{}, allocator);
}

template <bool KeepDimensions, TensorAxis... Axes, ReadableTensor Source>
    requires std::is_arithmetic_v<typename Source::value_type> &&
             (tensor_detail::tensorStaticRankValue<Source> != tensor_detail::kDynamicTensorRank)
[[nodiscard]] auto min(const Source& source,
                       std::optional<typename Source::value_type> initial = std::nullopt)
{
    return min<KeepDimensions, Axes...>(
        source, tensor_detail::selectResultAllocator<typename Source::value_type>(source), initial);
}

template <bool KeepDimensions, TensorAxis... Axes, ReadableTensor Source, typename Allocator>
    requires std::is_arithmetic_v<typename Source::value_type> &&
             (tensor_detail::tensorStaticRankValue<Source> != tensor_detail::kDynamicTensorRank) &&
             tensor_detail::AllocatorFor<Allocator, typename Source::value_type>
[[nodiscard]] auto max(const Source& source, const Allocator& allocator,
                       std::optional<typename Source::value_type> initial = std::nullopt)
{
    constexpr auto outputRank = tensor_detail::staticReductionOutputRank<KeepDimensions, Source, Axes...>;
    return tensor_detail::reduceExtremum<outputRank>(source, tensor_detail::staticAxes<Axes...>(),
                                                     KeepDimensions, initial, std::greater<>{}, allocator);
}

template <bool KeepDimensions, TensorAxis... Axes, ReadableTensor Source>
    requires std::is_arithmetic_v<typename Source::value_type> &&
             (tensor_detail::tensorStaticRankValue<Source> != tensor_detail::kDynamicTensorRank)
[[nodiscard]] auto max(const Source& source,
                       std::optional<typename Source::value_type> initial = std::nullopt)
{
    return max<KeepDimensions, Axes...>(
        source, tensor_detail::selectResultAllocator<typename Source::value_type>(source), initial);
}

template <bool KeepDimensions, TensorAxis... Axes, ReadableTensor Source, typename Allocator>
    requires std::is_arithmetic_v<typename Source::value_type> &&
             (tensor_detail::tensorStaticRankValue<Source> != tensor_detail::kDynamicTensorRank) &&
             tensor_detail::AllocatorFor<Allocator, std::size_t>
[[nodiscard]] auto argmin(const Source& source, const Allocator& allocator)
{
    constexpr auto outputRank = tensor_detail::staticReductionOutputRank<KeepDimensions, Source, Axes...>;
    return tensor_detail::reduceArgExtremum<outputRank>(source, tensor_detail::staticAxes<Axes...>(),
                                                        KeepDimensions, std::less<>{}, allocator);
}

template <bool KeepDimensions, TensorAxis... Axes, ReadableTensor Source>
    requires std::is_arithmetic_v<typename Source::value_type> &&
             (tensor_detail::tensorStaticRankValue<Source> != tensor_detail::kDynamicTensorRank)
[[nodiscard]] auto argmin(const Source& source)
{
    return argmin<KeepDimensions, Axes...>(source, tensor_detail::selectResultAllocator<std::size_t>(source));
}

template <bool KeepDimensions, TensorAxis... Axes, ReadableTensor Source, typename Allocator>
    requires std::is_arithmetic_v<typename Source::value_type> &&
             (tensor_detail::tensorStaticRankValue<Source> != tensor_detail::kDynamicTensorRank) &&
             tensor_detail::AllocatorFor<Allocator, std::size_t>
[[nodiscard]] auto argmax(const Source& source, const Allocator& allocator)
{
    constexpr auto outputRank = tensor_detail::staticReductionOutputRank<KeepDimensions, Source, Axes...>;
    return tensor_detail::reduceArgExtremum<outputRank>(source, tensor_detail::staticAxes<Axes...>(),
                                                        KeepDimensions, std::greater<>{}, allocator);
}

template <bool KeepDimensions, TensorAxis... Axes, ReadableTensor Source>
    requires std::is_arithmetic_v<typename Source::value_type> &&
             (tensor_detail::tensorStaticRankValue<Source> != tensor_detail::kDynamicTensorRank)
[[nodiscard]] auto argmax(const Source& source)
{
    return argmax<KeepDimensions, Axes...>(source, tensor_detail::selectResultAllocator<std::size_t>(source));
}

template <bool KeepDimensions, TensorAxis... Axes, ReadableTensor Source, typename Allocator>
    requires std::is_arithmetic_v<typename Source::value_type> &&
             (tensor_detail::tensorStaticRankValue<Source> != tensor_detail::kDynamicTensorRank) &&
             tensor_detail::AllocatorFor<Allocator, bool>
[[nodiscard]] auto all(const Source& source, const Allocator& allocator)
{
    constexpr auto outputRank = tensor_detail::staticReductionOutputRank<KeepDimensions, Source, Axes...>;
    return tensor_detail::reduceInitialized<outputRank>(source, tensor_detail::staticAxes<Axes...>(),
                                                        KeepDimensions, true, std::logical_and<bool>{}, allocator);
}

template <bool KeepDimensions, TensorAxis... Axes, ReadableTensor Source>
    requires std::is_arithmetic_v<typename Source::value_type> &&
             (tensor_detail::tensorStaticRankValue<Source> != tensor_detail::kDynamicTensorRank)
[[nodiscard]] auto all(const Source& source)
{
    return all<KeepDimensions, Axes...>(source, tensor_detail::selectResultAllocator<bool>(source));
}

template <bool KeepDimensions, TensorAxis... Axes, ReadableTensor Source, typename Allocator>
    requires std::is_arithmetic_v<typename Source::value_type> &&
             (tensor_detail::tensorStaticRankValue<Source> != tensor_detail::kDynamicTensorRank) &&
             tensor_detail::AllocatorFor<Allocator, bool>
[[nodiscard]] auto any(const Source& source, const Allocator& allocator)
{
    constexpr auto outputRank = tensor_detail::staticReductionOutputRank<KeepDimensions, Source, Axes...>;
    return tensor_detail::reduceInitialized<outputRank>(source, tensor_detail::staticAxes<Axes...>(),
                                                        KeepDimensions, false, std::logical_or<bool>{}, allocator);
}

template <bool KeepDimensions, TensorAxis... Axes, ReadableTensor Source>
    requires std::is_arithmetic_v<typename Source::value_type> &&
             (tensor_detail::tensorStaticRankValue<Source> != tensor_detail::kDynamicTensorRank)
[[nodiscard]] auto any(const Source& source)
{
    return any<KeepDimensions, Axes...>(source, tensor_detail::selectResultAllocator<bool>(source));
}

// A fixed-rank source makes reduce-all's rank statically knowable. Keep the
// vector-axis overloads for explicitly runtime-selected axes, but ensure the
// ordinary one-argument spelling retains rank zero.
template <tensor_detail::ArithmeticTensor Source, typename Allocator>
    requires tensor_detail::AllocatorFor<Allocator, TensorSumType<typename Source::value_type>>
[[nodiscard]] auto sum(const Source& source, const Allocator& allocator)
{
    if constexpr (tensor_detail::tensorStaticRankValue<Source> != tensor_detail::kDynamicTensorRank)
    {
        return sum<false>(source, allocator);
    }
    else
    {
        return sum(source, allocator, std::vector<TensorAxis>{});
    }
}

template <tensor_detail::ArithmeticTensor Source>
[[nodiscard]] auto sum(const Source& source)
{
    if constexpr (tensor_detail::tensorStaticRankValue<Source> != tensor_detail::kDynamicTensorRank)
    {
        return sum<false>(source);
    }
    else
    {
        return sum(source, std::vector<TensorAxis>{});
    }
}

template <tensor_detail::ArithmeticTensor Source, typename Allocator>
    requires tensor_detail::AllocatorFor<Allocator, TensorSumType<typename Source::value_type>>
[[nodiscard]] auto product(const Source& source, const Allocator& allocator)
{
    if constexpr (tensor_detail::tensorStaticRankValue<Source> != tensor_detail::kDynamicTensorRank)
    {
        return product<false>(source, allocator);
    }
    else
    {
        return product(source, allocator, std::vector<TensorAxis>{});
    }
}

template <tensor_detail::ArithmeticTensor Source>
[[nodiscard]] auto product(const Source& source)
{
    if constexpr (tensor_detail::tensorStaticRankValue<Source> != tensor_detail::kDynamicTensorRank)
    {
        return product<false>(source);
    }
    else
    {
        return product(source, std::vector<TensorAxis>{});
    }
}

template <tensor_detail::ArithmeticTensor Source, typename Allocator>
    requires tensor_detail::AllocatorFor<Allocator, TensorMeanType<typename Source::value_type>>
[[nodiscard]] auto mean(const Source& source, const Allocator& allocator)
{
    if constexpr (tensor_detail::tensorStaticRankValue<Source> != tensor_detail::kDynamicTensorRank)
    {
        return mean<false>(source, allocator);
    }
    else
    {
        return mean(source, allocator, std::vector<TensorAxis>{});
    }
}

template <tensor_detail::ArithmeticTensor Source>
[[nodiscard]] auto mean(const Source& source)
{
    if constexpr (tensor_detail::tensorStaticRankValue<Source> != tensor_detail::kDynamicTensorRank)
    {
        return mean<false>(source);
    }
    else
    {
        return mean(source, std::vector<TensorAxis>{});
    }
}

template <tensor_detail::ArithmeticTensor Source, typename Allocator>
    requires tensor_detail::AllocatorFor<Allocator, typename Source::value_type>
[[nodiscard]] auto min(const Source& source, const Allocator& allocator)
{
    if constexpr (tensor_detail::tensorStaticRankValue<Source> != tensor_detail::kDynamicTensorRank)
    {
        return min<false>(source, allocator);
    }
    else
    {
        return min(source, allocator, std::vector<TensorAxis>{});
    }
}

template <tensor_detail::ArithmeticTensor Source>
[[nodiscard]] auto min(const Source& source)
{
    if constexpr (tensor_detail::tensorStaticRankValue<Source> != tensor_detail::kDynamicTensorRank)
    {
        return min<false>(source);
    }
    else
    {
        return min(source, std::vector<TensorAxis>{});
    }
}

template <tensor_detail::ArithmeticTensor Source, typename Allocator>
    requires tensor_detail::AllocatorFor<Allocator, typename Source::value_type>
[[nodiscard]] auto max(const Source& source, const Allocator& allocator)
{
    if constexpr (tensor_detail::tensorStaticRankValue<Source> != tensor_detail::kDynamicTensorRank)
    {
        return max<false>(source, allocator);
    }
    else
    {
        return max(source, allocator, std::vector<TensorAxis>{});
    }
}

template <tensor_detail::ArithmeticTensor Source>
[[nodiscard]] auto max(const Source& source)
{
    if constexpr (tensor_detail::tensorStaticRankValue<Source> != tensor_detail::kDynamicTensorRank)
    {
        return max<false>(source);
    }
    else
    {
        return max(source, std::vector<TensorAxis>{});
    }
}

template <tensor_detail::ArithmeticTensor Source, typename Allocator>
    requires tensor_detail::AllocatorFor<Allocator, std::size_t>
[[nodiscard]] auto argmin(const Source& source, const Allocator& allocator)
{
    if constexpr (tensor_detail::tensorStaticRankValue<Source> != tensor_detail::kDynamicTensorRank)
    {
        return argmin<false>(source, allocator);
    }
    else
    {
        return argmin(source, allocator, std::vector<TensorAxis>{});
    }
}

template <tensor_detail::ArithmeticTensor Source>
[[nodiscard]] auto argmin(const Source& source)
{
    if constexpr (tensor_detail::tensorStaticRankValue<Source> != tensor_detail::kDynamicTensorRank)
    {
        return argmin<false>(source);
    }
    else
    {
        return argmin(source, std::vector<TensorAxis>{});
    }
}

template <tensor_detail::ArithmeticTensor Source, typename Allocator>
    requires tensor_detail::AllocatorFor<Allocator, std::size_t>
[[nodiscard]] auto argmax(const Source& source, const Allocator& allocator)
{
    if constexpr (tensor_detail::tensorStaticRankValue<Source> != tensor_detail::kDynamicTensorRank)
    {
        return argmax<false>(source, allocator);
    }
    else
    {
        return argmax(source, allocator, std::vector<TensorAxis>{});
    }
}

template <tensor_detail::ArithmeticTensor Source>
[[nodiscard]] auto argmax(const Source& source)
{
    if constexpr (tensor_detail::tensorStaticRankValue<Source> != tensor_detail::kDynamicTensorRank)
    {
        return argmax<false>(source);
    }
    else
    {
        return argmax(source, std::vector<TensorAxis>{});
    }
}

template <tensor_detail::ArithmeticTensor Source, typename Allocator>
    requires tensor_detail::AllocatorFor<Allocator, bool>
[[nodiscard]] auto all(const Source& source, const Allocator& allocator)
{
    if constexpr (tensor_detail::tensorStaticRankValue<Source> != tensor_detail::kDynamicTensorRank)
    {
        return all<false>(source, allocator);
    }
    else
    {
        return all(source, allocator, std::vector<TensorAxis>{});
    }
}

template <tensor_detail::ArithmeticTensor Source>
[[nodiscard]] auto all(const Source& source)
{
    if constexpr (tensor_detail::tensorStaticRankValue<Source> != tensor_detail::kDynamicTensorRank)
    {
        return all<false>(source);
    }
    else
    {
        return all(source, std::vector<TensorAxis>{});
    }
}

template <tensor_detail::ArithmeticTensor Source, typename Allocator>
    requires tensor_detail::AllocatorFor<Allocator, bool>
[[nodiscard]] auto any(const Source& source, const Allocator& allocator)
{
    if constexpr (tensor_detail::tensorStaticRankValue<Source> != tensor_detail::kDynamicTensorRank)
    {
        return any<false>(source, allocator);
    }
    else
    {
        return any(source, allocator, std::vector<TensorAxis>{});
    }
}

template <tensor_detail::ArithmeticTensor Source>
[[nodiscard]] auto any(const Source& source)
{
    if constexpr (tensor_detail::tensorStaticRankValue<Source> != tensor_detail::kDynamicTensorRank)
    {
        return any<false>(source);
    }
    else
    {
        return any(source, std::vector<TensorAxis>{});
    }
}

} // namespace fat_p
