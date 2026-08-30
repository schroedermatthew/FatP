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

#include "TensorAlgorithms.h"

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace fat_p
{

template <typename T>
using TensorSumType = std::conditional_t<
    std::same_as<std::remove_cv_t<T>, bool>, std::size_t,
    std::conditional_t<std::integral<T> && (sizeof(T) < sizeof(std::int64_t)),
                       std::conditional_t<std::signed_integral<T>, std::int64_t, std::uint64_t>, T>>;

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

inline ReductionShape makeReductionShape(const DynamicExtents& source, const std::vector<TensorAxis>& requested,
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
        axes = normalizeAxes(requested, source.rank());
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

inline std::size_t reductionElementCount(const DynamicExtents& source, const ReductionShape& shape)
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

inline ReductionCoordinates decodeReductionCoordinates(std::size_t linearIndex, const DynamicExtents& source,
                                                       const ReductionShape& shape,
                                                       std::vector<std::size_t>& coordinates)
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

template <ReadableTensor Source, typename Result, typename Allocator, typename Combine>
[[nodiscard]] Tensor<Result, Allocator>
reduceInitialized(const Source& source, const std::vector<TensorAxis>& axes, bool keepDimensions,
                  Result initial, Combine&& combine, const Allocator& allocator)
{
    tensor_detail::TensorAccess::validate(source);
    const auto shape = makeReductionShape(source.extents(), axes, keepDimensions);
    Tensor<Result, Allocator> result(std::allocator_arg, allocator, shape.outputExtents, initial);
    if (source.empty() || result.empty())
    {
        return result;
    }
    const auto* input = TensorAccess::storageBase(source);
    const TensorIterationPlan plan(source.extents(), {std::cref(source.layout())});
    std::vector<std::size_t> coordinates(source.rank(), 0);
    plan.forEachOffset([&](std::size_t linearIndex, const auto& offsets) {
        const auto output =
            decodeReductionCoordinates(linearIndex, source.extents(), shape, coordinates).outputLinear;
        result[output] = std::invoke(combine, result[output], static_cast<Result>(input[offsets[0]]));
    });
    return result;
}

template <ReadableTensor Source, typename Allocator, typename Compare>
[[nodiscard]] Tensor<typename Source::value_type, Allocator>
reduceExtremum(const Source& source, const std::vector<TensorAxis>& axes, bool keepDimensions,
              std::optional<typename Source::value_type> initial, Compare&& compare,
              const Allocator& allocator)
{
    using value_type = typename Source::value_type;
    TensorAccess::validate(source);
    const auto shape = makeReductionShape(source.extents(), axes, keepDimensions);
    Tensor<value_type, Allocator> result(std::allocator_arg, allocator, shape.outputExtents);
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
    const TensorIterationPlan plan(source.extents(), {std::cref(source.layout())});
    std::vector<std::size_t> coordinates(source.rank(), 0);
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

template <ReadableTensor Source, typename Allocator, typename Compare>
[[nodiscard]] Tensor<std::size_t, Allocator>
reduceArgExtremum(const Source& source, const std::vector<TensorAxis>& axes, bool keepDimensions,
                 Compare&& compare, const Allocator& allocator)
{
    using value_type = typename Source::value_type;
    TensorAccess::validate(source);
    const auto shape = makeReductionShape(source.extents(), axes, keepDimensions);
    Tensor<std::size_t, Allocator> result(std::allocator_arg, allocator, shape.outputExtents, 0);
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
    const TensorIterationPlan plan(source.extents(), {std::cref(source.layout())});
    std::vector<std::size_t> coordinateBuffer(source.rank(), 0);
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

} // namespace tensor_detail

template <ReadableTensor Source, typename Allocator>
    requires std::is_arithmetic_v<typename Source::value_type> &&
             tensor_detail::AllocatorFor<Allocator, TensorSumType<typename Source::value_type>>
[[nodiscard]] auto sum(const Source& source, const Allocator& allocator,
                       const std::vector<TensorAxis>& axes = {}, bool keepDimensions = false,
                       std::optional<TensorSumType<typename Source::value_type>> initial = std::nullopt)
{
    using result_type = TensorSumType<typename Source::value_type>;
    return tensor_detail::reduceInitialized(source, axes, keepDimensions, initial.value_or(result_type{0}),
                                            tensor_detail::checkedSameTypeAdd<result_type>, allocator);
}

template <ReadableTensor Source>
    requires std::is_arithmetic_v<typename Source::value_type>
[[nodiscard]] auto sum(const Source& source, const std::vector<TensorAxis>& axes = {}, bool keepDimensions = false,
                       std::optional<TensorSumType<typename Source::value_type>> initial = std::nullopt)
{
    using result_type = TensorSumType<typename Source::value_type>;
    return sum(source, tensor_detail::selectResultAllocator<result_type>(source), axes, keepDimensions, initial);
}

template <ReadableTensor Source, typename Allocator>
    requires std::is_arithmetic_v<typename Source::value_type> &&
             tensor_detail::AllocatorFor<Allocator, TensorSumType<typename Source::value_type>>
[[nodiscard]] auto product(const Source& source, const Allocator& allocator,
                           const std::vector<TensorAxis>& axes = {}, bool keepDimensions = false,
                           std::optional<TensorSumType<typename Source::value_type>> initial = std::nullopt)
{
    using result_type = TensorSumType<typename Source::value_type>;
    return tensor_detail::reduceInitialized(source, axes, keepDimensions, initial.value_or(result_type{1}),
                                            tensor_detail::checkedSameTypeMultiply<result_type>, allocator);
}

template <ReadableTensor Source>
    requires std::is_arithmetic_v<typename Source::value_type>
[[nodiscard]] auto product(const Source& source, const std::vector<TensorAxis>& axes = {},
                           bool keepDimensions = false,
                           std::optional<TensorSumType<typename Source::value_type>> initial = std::nullopt)
{
    using result_type = TensorSumType<typename Source::value_type>;
    return product(source, tensor_detail::selectResultAllocator<result_type>(source), axes, keepDimensions,
                   initial);
}

template <ReadableTensor Source, typename Allocator>
    requires std::is_arithmetic_v<typename Source::value_type> &&
             tensor_detail::AllocatorFor<Allocator, TensorMeanType<typename Source::value_type>>
[[nodiscard]] auto mean(const Source& source, const Allocator& allocator,
                        const std::vector<TensorAxis>& axes = {}, bool keepDimensions = false)
{
    using result_type = TensorMeanType<typename Source::value_type>;
    tensor_detail::TensorAccess::validate(source);
    const auto shape = tensor_detail::makeReductionShape(source.extents(), axes, keepDimensions);
    const auto count = tensor_detail::reductionElementCount(source.extents(), shape);
    if (shape.outputExtents.logicalSize() != 0 && count == 0)
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

template <ReadableTensor Source>
    requires std::is_arithmetic_v<typename Source::value_type>
[[nodiscard]] auto mean(const Source& source, const std::vector<TensorAxis>& axes = {}, bool keepDimensions = false)
{
    using result_type = TensorMeanType<typename Source::value_type>;
    return mean(source, tensor_detail::selectResultAllocator<result_type>(source), axes, keepDimensions);
}

template <ReadableTensor Source, typename Allocator>
    requires std::is_arithmetic_v<typename Source::value_type> &&
             tensor_detail::AllocatorFor<Allocator, typename Source::value_type>
[[nodiscard]] auto min(const Source& source, const Allocator& allocator,
                       const std::vector<TensorAxis>& axes = {}, bool keepDimensions = false,
                       std::optional<typename Source::value_type> initial = std::nullopt)
{
    return tensor_detail::reduceExtremum(source, axes, keepDimensions, initial, std::less<>{}, allocator);
}

template <ReadableTensor Source>
    requires std::is_arithmetic_v<typename Source::value_type>
[[nodiscard]] auto min(const Source& source, const std::vector<TensorAxis>& axes = {}, bool keepDimensions = false,
                       std::optional<typename Source::value_type> initial = std::nullopt)
{
    return min(source, tensor_detail::selectResultAllocator<typename Source::value_type>(source), axes,
               keepDimensions, initial);
}

template <ReadableTensor Source, typename Allocator>
    requires std::is_arithmetic_v<typename Source::value_type> &&
             tensor_detail::AllocatorFor<Allocator, typename Source::value_type>
[[nodiscard]] auto max(const Source& source, const Allocator& allocator,
                       const std::vector<TensorAxis>& axes = {}, bool keepDimensions = false,
                       std::optional<typename Source::value_type> initial = std::nullopt)
{
    return tensor_detail::reduceExtremum(source, axes, keepDimensions, initial, std::greater<>{}, allocator);
}

template <ReadableTensor Source>
    requires std::is_arithmetic_v<typename Source::value_type>
[[nodiscard]] auto max(const Source& source, const std::vector<TensorAxis>& axes = {}, bool keepDimensions = false,
                       std::optional<typename Source::value_type> initial = std::nullopt)
{
    return max(source, tensor_detail::selectResultAllocator<typename Source::value_type>(source), axes,
               keepDimensions, initial);
}

template <ReadableTensor Source, typename Allocator>
    requires std::is_arithmetic_v<typename Source::value_type> &&
             tensor_detail::AllocatorFor<Allocator, std::size_t>
[[nodiscard]] auto argmin(const Source& source, const Allocator& allocator,
                          const std::vector<TensorAxis>& axes = {}, bool keepDimensions = false)
{
    return tensor_detail::reduceArgExtremum(source, axes, keepDimensions, std::less<>{}, allocator);
}

template <ReadableTensor Source>
    requires std::is_arithmetic_v<typename Source::value_type>
[[nodiscard]] auto argmin(const Source& source, const std::vector<TensorAxis>& axes = {},
                          bool keepDimensions = false)
{
    return argmin(source, tensor_detail::selectResultAllocator<std::size_t>(source), axes, keepDimensions);
}

template <ReadableTensor Source, typename Allocator>
    requires std::is_arithmetic_v<typename Source::value_type> &&
             tensor_detail::AllocatorFor<Allocator, std::size_t>
[[nodiscard]] auto argmax(const Source& source, const Allocator& allocator,
                          const std::vector<TensorAxis>& axes = {}, bool keepDimensions = false)
{
    return tensor_detail::reduceArgExtremum(source, axes, keepDimensions, std::greater<>{}, allocator);
}

template <ReadableTensor Source>
    requires std::is_arithmetic_v<typename Source::value_type>
[[nodiscard]] auto argmax(const Source& source, const std::vector<TensorAxis>& axes = {},
                          bool keepDimensions = false)
{
    return argmax(source, tensor_detail::selectResultAllocator<std::size_t>(source), axes, keepDimensions);
}

} // namespace fat_p
