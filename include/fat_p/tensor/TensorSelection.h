#pragma once

/*
FATP_META:
  meta_version: 1
  component: TensorSelection
  file_role: internal_header
  path: include/fat_p/tensor/TensorSelection.h
  namespace: fat_p
  layer: Domain
  summary: "Dependency-free Tensor composition and indexed-selection algorithms."
  api_stability: in_work
  related:
    docs:
      - components/Tensor/docs/Design Note - Tensor Architecture Additions Plan.md
    headers:
      - include/fat_p/TensorSelection.h
      - include/fat_p/tensor/TensorMatmul.h
    tests:
      - components/Tensor/tests/test_TensorSelection.cpp
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

/** @file TensorSelection.h @brief Tensor stack, concatenate, take, and gather operations. */

#include "TensorAlgorithms.h"

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <limits>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace fat_p
{

namespace tensor_selection_detail
{

inline std::size_t normalizeExistingAxis(TensorAxis axis, std::size_t rank)
{
    if (rank > static_cast<std::size_t>(std::numeric_limits<TensorAxis>::max()))
    {
        throw std::overflow_error("Tensor rank cannot be represented by TensorAxis");
    }
    auto normalized = axis;
    if (normalized < 0)
    {
        normalized += static_cast<TensorAxis>(rank);
    }
    if (normalized < 0 || static_cast<std::size_t>(normalized) >= rank)
    {
        throw std::out_of_range("Tensor axis is out of range");
    }
    return static_cast<std::size_t>(normalized);
}

inline std::size_t normalizeInsertionAxis(TensorAxis axis, std::size_t rank)
{
    if (rank >= static_cast<std::size_t>(std::numeric_limits<TensorAxis>::max()))
    {
        throw std::overflow_error("Tensor rank cannot be incremented");
    }
    const auto resultRank = rank + 1;
    auto normalized = axis;
    if (normalized < 0)
    {
        normalized += static_cast<TensorAxis>(resultRank);
    }
    if (normalized < 0 || static_cast<std::size_t>(normalized) >= resultRank)
    {
        throw std::out_of_range("Tensor insertion axis is out of range");
    }
    return static_cast<std::size_t>(normalized);
}

inline std::size_t normalizeIndex(std::ptrdiff_t index, std::size_t extent)
{
    if (index >= 0)
    {
        const auto converted = static_cast<std::size_t>(index);
        if (converted >= extent)
        {
            throw std::out_of_range("Tensor selection index is out of range");
        }
        return converted;
    }
    const auto magnitude = tensor_detail::offsetMagnitude(index);
    if (magnitude > extent)
    {
        throw std::out_of_range("Tensor selection index is out of range");
    }
    return extent - static_cast<std::size_t>(magnitude);
}

template <std::integral Index>
inline std::size_t normalizeIndexValue(Index index, std::size_t extent)
{
    using unsigned_index = std::make_unsigned_t<Index>;
    if constexpr (std::is_signed_v<Index>)
    {
        if (index < 0)
        {
            const auto magnitude = static_cast<unsigned_index>(-(index + 1)) + unsigned_index{1};
            if (magnitude > extent)
            {
                throw std::out_of_range("Tensor selection index is out of range");
            }
            return extent - static_cast<std::size_t>(magnitude);
        }
    }
    const auto converted = static_cast<unsigned_index>(index);
    if (converted >= extent)
    {
        throw std::out_of_range("Tensor selection index is out of range");
    }
    return static_cast<std::size_t>(converted);
}

template <typename Extents>
inline void decodeCoordinates(std::size_t linear, const Extents& extents,
                              std::vector<std::size_t>& coordinates)
{
    std::fill(coordinates.begin(), coordinates.end(), std::size_t{0});
    for (std::size_t reverseAxis = extents.rank(); reverseAxis > 0; --reverseAxis)
    {
        const auto axis = reverseAxis - 1;
        coordinates[axis] = linear % extents[axis];
        linear /= extents[axis];
    }
}

template <typename Extents>
inline std::size_t encodeCoordinates(const std::vector<std::size_t>& coordinates,
                                     const Extents& extents)
{
    std::size_t linear = 0;
    for (std::size_t axis = 0; axis < extents.rank(); ++axis)
    {
        linear = linear * extents[axis] + coordinates[axis];
    }
    return linear;
}

template <typename Extents>
inline std::size_t encodeCoordinatesSkippingAxis(const std::vector<std::size_t>& coordinates,
                                                 const Extents& extents,
                                                 std::size_t skippedAxis)
{
    std::size_t linear = 0;
    for (std::size_t outputAxis = 0, sourceAxis = 0; sourceAxis < extents.rank(); ++sourceAxis)
    {
        if (sourceAxis == skippedAxis)
        {
            ++outputAxis;
        }
        linear = linear * extents[sourceAxis] + coordinates[outputAxis++];
    }
    return linear;
}

inline std::size_t checkedExtentAdd(std::size_t left, std::size_t right)
{
    if (left > std::numeric_limits<std::size_t>::max() - right)
    {
        throw std::overflow_error("Tensor composition extent exceeds size_t");
    }
    return left + right;
}

template <typename Extents>
inline auto insertExtent(const Extents& source, std::size_t axis, std::size_t extent)
{
    constexpr auto SourceRank = tensor_detail::extentsStaticRank<Extents>;
    constexpr auto ResultRank = SourceRank == tensor_detail::kDynamicTensorRank
                                    ? tensor_detail::kDynamicTensorRank
                                    : SourceRank + 1;
    using ResultExtents = tensor_detail::TensorExtentsFor<ResultRank>;
    typename ResultExtents::container_type result{};
    if constexpr (ResultRank == tensor_detail::kDynamicTensorRank)
    {
        result.reserve(source.rank() + 1);
    }
    std::size_t next = 0;
    for (std::size_t outputAxis = 0; outputAxis <= source.rank(); ++outputAxis)
    {
        if (outputAxis == axis)
        {
            if constexpr (ResultRank == tensor_detail::kDynamicTensorRank)
            {
                result.push_back(extent);
            }
            else
            {
                result[next++] = extent;
            }
        }
        else
        {
            const auto value = source[outputAxis < axis ? outputAxis : outputAxis - 1];
            if constexpr (ResultRank == tensor_detail::kDynamicTensorRank)
            {
                result.push_back(value);
            }
            else
            {
                result[next++] = value;
            }
        }
    }
    return ResultExtents(std::move(result));
}

template <typename First, typename Second>
inline constexpr std::size_t compositionRank = tensor_detail::binaryResultRank<First, Second>;

template <typename First, typename Second>
inline constexpr bool compositionRanksCompatible =
    tensor_detail::tensorStaticRankValue<First> == tensor_detail::kDynamicTensorRank ||
    tensor_detail::tensorStaticRankValue<Second> == tensor_detail::kDynamicTensorRank ||
    tensor_detail::tensorStaticRankValue<First> == tensor_detail::tensorStaticRankValue<Second>;

template <typename First, typename Second>
inline constexpr std::size_t stackRank = compositionRank<First, Second> == tensor_detail::kDynamicTensorRank
                                             ? tensor_detail::kDynamicTensorRank
                                             : compositionRank<First, Second> + 1;

template <ReadableTensor First, ReadableTensor Second, typename Allocator>
    requires SameTensorValue<First, Second> && compositionRanksCompatible<First, Second>
[[nodiscard]] Tensor<typename First::value_type, Allocator, stackRank<First, Second>>
stackPair(const First& first, const Second& second, TensorAxis requestedAxis, const Allocator& allocator)
{
    tensor_detail::TensorAccess::validate(first);
    tensor_detail::TensorAccess::validate(second);
    if (first.extents() != second.extents())
    {
        throw std::invalid_argument("stack operands must have identical extents");
    }
    const auto axis = normalizeInsertionAxis(requestedAxis, first.rank());
    auto outputExtents = insertExtent(first.extents(), axis, 2);
    using ResultExtents = tensor_detail::TensorExtentsFor<stackRank<First, Second>>;
    ResultExtents resultExtents(outputExtents.begin(), outputExtents.end());
    Tensor<typename First::value_type, Allocator, stackRank<First, Second>> result(
        std::allocator_arg, allocator, std::move(resultExtents));
    std::vector<std::size_t> coordinates(result.rank(), 0);
    for (std::size_t linear = 0; linear < result.size(); ++linear)
    {
        decodeCoordinates(linear, result.extents(), coordinates);
        const auto operand = coordinates[axis];
        const auto sourceLinear = encodeCoordinatesSkippingAxis(coordinates, first.extents(), axis);
        result[linear] = operand == 0 ? first[sourceLinear] : second[sourceLinear];
    }
    return result;
}

template <ReadableTensor First, ReadableTensor Second, typename Allocator>
    requires SameTensorValue<First, Second> && compositionRanksCompatible<First, Second>
[[nodiscard]] Tensor<typename First::value_type, Allocator, compositionRank<First, Second>>
concatenatePair(const First& first, const Second& second, TensorAxis requestedAxis, const Allocator& allocator)
{
    tensor_detail::TensorAccess::validate(first);
    tensor_detail::TensorAccess::validate(second);
    if (first.rank() == 0 || first.rank() != second.rank())
    {
        throw std::invalid_argument("concatenate operands must have the same nonzero rank");
    }
    const auto axis = normalizeExistingAxis(requestedAxis, first.rank());
    auto output = first.extents().values();
    for (std::size_t current = 0; current < first.rank(); ++current)
    {
        if (current != axis && first.extents()[current] != second.extents()[current])
        {
            throw std::invalid_argument("concatenate non-axis extents must match");
        }
    }
    output[axis] = checkedExtentAdd(output[axis], second.extents()[axis]);
    using ResultExtents = tensor_detail::TensorExtentsFor<compositionRank<First, Second>>;
    ResultExtents resultExtents(output.begin(), output.end());
    Tensor<typename First::value_type, Allocator, compositionRank<First, Second>> result(
        std::allocator_arg, allocator, std::move(resultExtents));
    std::vector<std::size_t> coordinates(result.rank(), 0);
    for (std::size_t linear = 0; linear < result.size(); ++linear)
    {
        decodeCoordinates(linear, result.extents(), coordinates);
        if (coordinates[axis] < first.extents()[axis])
        {
            result[linear] = first[encodeCoordinates(coordinates, first.extents())];
        }
        else
        {
            coordinates[axis] -= first.extents()[axis];
            result[linear] = second[encodeCoordinates(coordinates, second.extents())];
        }
    }
    return result;
}

} // namespace tensor_selection_detail

template <ReadableTensor First, ReadableTensor Second, typename Allocator>
    requires SameTensorValue<First, Second> &&
             tensor_selection_detail::compositionRanksCompatible<First, Second>
[[nodiscard]] auto stack(const First& first, const Second& second, TensorAxis axis, const Allocator& allocator)
{
    return tensor_selection_detail::stackPair(first, second, axis, allocator);
}

template <ReadableTensor First, ReadableTensor Second>
    requires SameTensorValue<First, Second> &&
             tensor_selection_detail::compositionRanksCompatible<First, Second>
[[nodiscard]] auto stack(const First& first, const Second& second, TensorAxis axis = 0)
{
    using value_type = typename First::value_type;
    if constexpr (requires { first.get_allocator(); })
    {
        return stack(first, second, axis, tensor_detail::selectResultAllocator<value_type>(first));
    }
    else if constexpr (requires { second.get_allocator(); })
    {
        return stack(first, second, axis, tensor_detail::selectResultAllocator<value_type>(second));
    }
    else
    {
        return stack(first, second, axis, TensorAllocator<value_type>{});
    }
}

template <ReadableTensor First, ReadableTensor Second, typename Allocator>
    requires SameTensorValue<First, Second> &&
             tensor_selection_detail::compositionRanksCompatible<First, Second>
[[nodiscard]] auto concatenate(const First& first, const Second& second, TensorAxis axis,
                               const Allocator& allocator)
{
    return tensor_selection_detail::concatenatePair(first, second, axis, allocator);
}

template <ReadableTensor First, ReadableTensor Second>
    requires SameTensorValue<First, Second> &&
             tensor_selection_detail::compositionRanksCompatible<First, Second>
[[nodiscard]] auto concatenate(const First& first, const Second& second, TensorAxis axis = 0)
{
    using value_type = typename First::value_type;
    if constexpr (requires { first.get_allocator(); })
    {
        return concatenate(first, second, axis, tensor_detail::selectResultAllocator<value_type>(first));
    }
    else if constexpr (requires { second.get_allocator(); })
    {
        return concatenate(first, second, axis, tensor_detail::selectResultAllocator<value_type>(second));
    }
    else
    {
        return concatenate(first, second, axis, TensorAllocator<value_type>{});
    }
}

template <ReadableTensor Source, typename Allocator>
[[nodiscard]] Tensor<typename Source::value_type, Allocator,
                     tensor_detail::tensorStaticRankValue<Source> == tensor_detail::kDynamicTensorRank
                         ? tensor_detail::kDynamicTensorRank
                         : tensor_detail::tensorStaticRankValue<Source> + 1>
stack(std::span<const std::reference_wrapper<const Source>> inputs, TensorAxis requestedAxis,
      const Allocator& allocator)
{
    if (inputs.empty())
    {
        throw std::invalid_argument("stack requires at least one operand");
    }
    const auto& prototype = inputs.front().get();
    tensor_detail::TensorAccess::validate(prototype);
    const auto axis = tensor_selection_detail::normalizeInsertionAxis(requestedAxis, prototype.rank());
    for (const auto& inputReference : inputs)
    {
        const auto& input = inputReference.get();
        tensor_detail::TensorAccess::validate(input);
        if (input.extents() != prototype.extents())
        {
            throw std::invalid_argument("stack operands must have identical extents");
        }
    }
    constexpr auto ResultRank = tensor_detail::tensorStaticRankValue<Source> == tensor_detail::kDynamicTensorRank
                                    ? tensor_detail::kDynamicTensorRank
                                    : tensor_detail::tensorStaticRankValue<Source> + 1;
    Tensor<typename Source::value_type, Allocator, ResultRank> result(
        std::allocator_arg, allocator,
        tensor_selection_detail::insertExtent(prototype.extents(), axis, inputs.size()));
    std::vector<std::size_t> coordinates(result.rank(), 0);
    for (std::size_t linear = 0; linear < result.size(); ++linear)
    {
        tensor_selection_detail::decodeCoordinates(linear, result.extents(), coordinates);
        const auto inputIndex = coordinates[axis];
        result[linear] = inputs[inputIndex].get()[
            tensor_selection_detail::encodeCoordinatesSkippingAxis(coordinates, prototype.extents(), axis)];
    }
    return result;
}

template <ReadableTensor Source, typename Allocator>
[[nodiscard]] Tensor<typename Source::value_type, Allocator,
                     tensor_detail::tensorStaticRankValue<Source>>
concatenate(std::span<const std::reference_wrapper<const Source>> inputs, TensorAxis requestedAxis,
            const Allocator& allocator)
{
    if (inputs.empty())
    {
        throw std::invalid_argument("concatenate requires at least one operand");
    }
    const auto& prototype = inputs.front().get();
    tensor_detail::TensorAccess::validate(prototype);
    if (prototype.rank() == 0)
    {
        throw std::invalid_argument("concatenate does not accept rank-zero operands");
    }
    const auto axis = tensor_selection_detail::normalizeExistingAxis(requestedAxis, prototype.rank());
    auto output = prototype.extents().values();
    output[axis] = 0;
    std::vector<std::size_t> boundaries;
    boundaries.reserve(inputs.size());
    for (const auto& inputReference : inputs)
    {
        const auto& input = inputReference.get();
        tensor_detail::TensorAccess::validate(input);
        if (input.rank() != prototype.rank())
        {
            throw std::invalid_argument("concatenate operands must have identical rank");
        }
        for (std::size_t current = 0; current < prototype.rank(); ++current)
        {
            if (current != axis && input.extents()[current] != prototype.extents()[current])
            {
                throw std::invalid_argument("concatenate non-axis extents must match");
            }
        }
        output[axis] = tensor_selection_detail::checkedExtentAdd(output[axis], input.extents()[axis]);
        boundaries.push_back(output[axis]);
    }
    using ResultExtents = tensor_detail::TensorExtentsFor<tensor_detail::tensorStaticRankValue<Source>>;
    Tensor<typename Source::value_type, Allocator, tensor_detail::tensorStaticRankValue<Source>> result(
        std::allocator_arg, allocator, ResultExtents(output.begin(), output.end()));
    std::vector<std::size_t> coordinates(result.rank(), 0);
    for (std::size_t linear = 0; linear < result.size(); ++linear)
    {
        tensor_selection_detail::decodeCoordinates(linear, result.extents(), coordinates);
        std::size_t inputIndex = 0;
        std::size_t preceding = 0;
        while (coordinates[axis] >= boundaries[inputIndex])
        {
            preceding = boundaries[inputIndex++];
        }
        coordinates[axis] -= preceding;
        result[linear] = inputs[inputIndex].get()[
            tensor_selection_detail::encodeCoordinates(coordinates, inputs[inputIndex].get().extents())];
    }
    return result;
}

template <ReadableTensor Source>
[[nodiscard]] auto stack(std::span<const std::reference_wrapper<const Source>> inputs,
                         TensorAxis requestedAxis = 0)
{
    if (inputs.empty())
    {
        throw std::invalid_argument("stack requires at least one operand");
    }
    using value_type = typename Source::value_type;
    return stack(inputs, requestedAxis,
                 tensor_detail::selectResultAllocator<value_type>(inputs.front().get()));
}

template <ReadableTensor Source, typename Allocator>
[[nodiscard]] auto stack(std::initializer_list<std::reference_wrapper<const Source>> inputs,
                         TensorAxis requestedAxis, const Allocator& allocator)
{
    return stack(std::span<const std::reference_wrapper<const Source>>(inputs.begin(), inputs.size()),
                 requestedAxis, allocator);
}

template <ReadableTensor Source>
[[nodiscard]] auto stack(std::initializer_list<std::reference_wrapper<const Source>> inputs,
                         TensorAxis requestedAxis = 0)
{
    return stack(std::span<const std::reference_wrapper<const Source>>(inputs.begin(), inputs.size()),
                 requestedAxis);
}

template <ReadableTensor Source>
[[nodiscard]] auto concatenate(std::span<const std::reference_wrapper<const Source>> inputs,
                               TensorAxis requestedAxis = 0)
{
    if (inputs.empty())
    {
        throw std::invalid_argument("concatenate requires at least one operand");
    }
    using value_type = typename Source::value_type;
    return concatenate(inputs, requestedAxis,
                       tensor_detail::selectResultAllocator<value_type>(inputs.front().get()));
}

template <ReadableTensor Source, typename Allocator>
[[nodiscard]] auto concatenate(std::initializer_list<std::reference_wrapper<const Source>> inputs,
                               TensorAxis requestedAxis, const Allocator& allocator)
{
    return concatenate(std::span<const std::reference_wrapper<const Source>>(inputs.begin(), inputs.size()),
                       requestedAxis, allocator);
}

template <ReadableTensor Source>
[[nodiscard]] auto concatenate(std::initializer_list<std::reference_wrapper<const Source>> inputs,
                               TensorAxis requestedAxis = 0)
{
    return concatenate(std::span<const std::reference_wrapper<const Source>>(inputs.begin(), inputs.size()),
                       requestedAxis);
}

template <ReadableTensor Source, typename Allocator>
[[nodiscard]] Tensor<typename Source::value_type, Allocator,
                     tensor_detail::tensorStaticRankValue<Source>>
take(const Source& source, std::span<const std::ptrdiff_t> indices, TensorAxis requestedAxis,
     const Allocator& allocator)
{
    tensor_detail::TensorAccess::validate(source);
    if (source.rank() == 0)
    {
        throw std::invalid_argument("take requires a rank-one or greater source");
    }
    const auto axis = tensor_selection_detail::normalizeExistingAxis(requestedAxis, source.rank());
    std::vector<std::size_t> normalized;
    normalized.reserve(indices.size());
    for (const auto index : indices)
    {
        normalized.push_back(tensor_selection_detail::normalizeIndex(index, source.extents()[axis]));
    }
    auto output = source.extents().values();
    output[axis] = indices.size();
    using ResultExtents = tensor_detail::TensorExtentsFor<tensor_detail::tensorStaticRankValue<Source>>;
    Tensor<typename Source::value_type, Allocator, tensor_detail::tensorStaticRankValue<Source>> result(
        std::allocator_arg, allocator, ResultExtents(output.begin(), output.end()));
    std::vector<std::size_t> coordinates(result.rank(), 0);
    for (std::size_t linear = 0; linear < result.size(); ++linear)
    {
        tensor_selection_detail::decodeCoordinates(linear, result.extents(), coordinates);
        coordinates[axis] = normalized[coordinates[axis]];
        result[linear] = source[tensor_selection_detail::encodeCoordinates(coordinates, source.extents())];
    }
    return result;
}

template <ReadableTensor Source>
[[nodiscard]] auto take(const Source& source, std::span<const std::ptrdiff_t> indices,
                        TensorAxis requestedAxis = 0)
{
    using value_type = typename Source::value_type;
    return take(source, indices, requestedAxis,
                tensor_detail::selectResultAllocator<value_type>(source));
}

template <ReadableTensor Source, ReadableTensor Indices, typename Allocator>
    requires std::integral<typename Indices::value_type> &&
             (!std::same_as<std::remove_cv_t<typename Indices::value_type>, bool>)
[[nodiscard]] Tensor<typename Source::value_type, Allocator,
                     tensor_detail::tensorStaticRankValue<Indices>>
takeAlongAxis(const Source& source, const Indices& indices, TensorAxis requestedAxis,
              const Allocator& allocator)
{
    tensor_detail::TensorAccess::validate(source);
    tensor_detail::TensorAccess::validate(indices);
    if (source.rank() == 0 || source.rank() != indices.rank())
    {
        throw std::invalid_argument("takeAlongAxis requires equal nonzero ranks");
    }
    const auto axis = tensor_selection_detail::normalizeExistingAxis(requestedAxis, source.rank());
    for (std::size_t current = 0; current < source.rank(); ++current)
    {
        if (current != axis && source.extents()[current] != indices.extents()[current])
        {
            throw std::invalid_argument("takeAlongAxis non-axis extents must match");
        }
    }
    std::vector<std::size_t> normalized;
    normalized.reserve(indices.size());
    for (std::size_t linear = 0; linear < indices.size(); ++linear)
    {
        normalized.push_back(
            tensor_selection_detail::normalizeIndexValue(indices[linear], source.extents()[axis]));
    }
    Tensor<typename Source::value_type, Allocator, tensor_detail::tensorStaticRankValue<Indices>> result(
        std::allocator_arg, allocator, indices.extents());
    std::vector<std::size_t> coordinates(result.rank(), 0);
    for (std::size_t linear = 0; linear < result.size(); ++linear)
    {
        tensor_selection_detail::decodeCoordinates(linear, result.extents(), coordinates);
        coordinates[axis] = normalized[linear];
        result[linear] = source[tensor_selection_detail::encodeCoordinates(coordinates, source.extents())];
    }
    return result;
}

template <ReadableTensor Source, ReadableTensor Indices>
    requires std::integral<typename Indices::value_type> &&
             (!std::same_as<std::remove_cv_t<typename Indices::value_type>, bool>)
[[nodiscard]] auto takeAlongAxis(const Source& source, const Indices& indices, TensorAxis requestedAxis)
{
    using value_type = typename Source::value_type;
    return takeAlongAxis(source, indices, requestedAxis,
                         tensor_detail::selectBinaryResultAllocator<value_type>(source, indices));
}

namespace tensor_selection_detail
{

template <std::size_t OutputRank, ReadableTensor Source, ReadableTensor Indices, typename Allocator>
    requires std::integral<typename Indices::value_type> &&
             (!std::same_as<std::remove_cv_t<typename Indices::value_type>, bool>)
[[nodiscard]] Tensor<typename Source::value_type, Allocator, OutputRank>
gatherNDImpl(const Source& source, const Indices& indices, const Allocator& allocator)
{
    tensor_detail::TensorAccess::validate(source);
    tensor_detail::TensorAccess::validate(indices);
    if (indices.rank() == 0)
    {
        throw std::invalid_argument("gatherND indices must have rank one or greater");
    }
    const auto tupleDepth = indices.extents()[indices.rank() - 1];
    if (tupleDepth > source.rank())
    {
        throw std::invalid_argument("gatherND index tuples exceed the source rank");
    }
    std::vector<std::size_t> normalized;
    normalized.reserve(indices.size());
    if (tupleDepth != 0)
    {
        for (std::size_t linear = 0; linear < indices.size(); ++linear)
        {
            normalized.push_back(tensor_selection_detail::normalizeIndexValue(
                indices[linear], source.extents()[linear % tupleDepth]));
        }
    }
    std::vector<std::size_t> output;
    output.reserve(indices.rank() - 1 + source.rank() - tupleDepth);
    for (std::size_t axis = 0; axis + 1 < indices.rank(); ++axis)
    {
        output.push_back(indices.extents()[axis]);
    }
    for (std::size_t axis = tupleDepth; axis < source.rank(); ++axis)
    {
        output.push_back(source.extents()[axis]);
    }
    using ResultExtents = tensor_detail::TensorExtentsFor<OutputRank>;
    Tensor<typename Source::value_type, Allocator, OutputRank> result(
        std::allocator_arg, allocator, ResultExtents(output.begin(), output.end()));
    const auto prefixRank = indices.rank() - 1;
    std::vector<std::size_t> outputCoordinates(result.rank(), 0);
    std::vector<std::size_t> sourceCoordinates(source.rank(), 0);
    for (std::size_t linear = 0; linear < result.size(); ++linear)
    {
        tensor_selection_detail::decodeCoordinates(linear, result.extents(), outputCoordinates);
        std::size_t tupleLinear = 0;
        for (std::size_t axis = 0; axis < prefixRank; ++axis)
        {
            tupleLinear = tupleLinear * indices.extents()[axis] + outputCoordinates[axis];
        }
        std::fill(sourceCoordinates.begin(), sourceCoordinates.end(), std::size_t{0});
        for (std::size_t axis = 0; axis < tupleDepth; ++axis)
        {
            sourceCoordinates[axis] = normalized[tupleLinear * tupleDepth + axis];
        }
        for (std::size_t axis = tupleDepth; axis < source.rank(); ++axis)
        {
            sourceCoordinates[axis] = outputCoordinates[prefixRank + axis - tupleDepth];
        }
        result[linear] = source[
            tensor_selection_detail::encodeCoordinates(sourceCoordinates, source.extents())];
    }
    return result;
}

} // namespace tensor_selection_detail

template <ReadableTensor Source, ReadableTensor Indices, typename Allocator>
    requires std::integral<typename Indices::value_type> &&
             (!std::same_as<std::remove_cv_t<typename Indices::value_type>, bool>)
[[nodiscard]] Tensor<typename Source::value_type, Allocator>
gatherND(const Source& source, const Indices& indices, const Allocator& allocator)
{
    return tensor_selection_detail::gatherNDImpl<tensor_detail::kDynamicTensorRank>(source, indices,
                                                                                    allocator);
}

template <ReadableTensor Source, ReadableTensor Indices>
    requires std::integral<typename Indices::value_type> &&
             (!std::same_as<std::remove_cv_t<typename Indices::value_type>, bool>)
[[nodiscard]] auto gatherND(const Source& source, const Indices& indices)
{
    using value_type = typename Source::value_type;
    return gatherND(source, indices,
                    tensor_detail::selectBinaryResultAllocator<value_type>(source, indices));
}

template <std::size_t TupleDepth, ReadableTensor Source, ReadableTensor Indices, typename Allocator>
    requires std::integral<typename Indices::value_type> &&
             (!std::same_as<std::remove_cv_t<typename Indices::value_type>, bool>)
[[nodiscard]] auto gatherND(const Source& source, const Indices& indices, const Allocator& allocator)
{
    constexpr auto sourceRank = tensor_detail::tensorStaticRankValue<Source>;
    constexpr auto indicesRank = tensor_detail::tensorStaticRankValue<Indices>;
    constexpr auto outputRank = [=] {
        if constexpr (sourceRank == tensor_detail::kDynamicTensorRank ||
                      indicesRank == tensor_detail::kDynamicTensorRank)
        {
            return tensor_detail::kDynamicTensorRank;
        }
        else
        {
            static_assert(indicesRank > 0, "gatherND indices must have positive rank");
            static_assert(TupleDepth <= sourceRank, "gatherND tuple depth exceeds the source rank");
            return indicesRank - 1 + sourceRank - TupleDepth;
        }
    }();
    if constexpr (sourceRank != tensor_detail::kDynamicTensorRank)
    {
        static_assert(TupleDepth <= sourceRank, "gatherND tuple depth exceeds the source rank");
    }
    if constexpr (indicesRank != tensor_detail::kDynamicTensorRank)
    {
        static_assert(indicesRank > 0, "gatherND indices must have positive rank");
    }
    tensor_detail::TensorAccess::validate(source);
    tensor_detail::TensorAccess::validate(indices);
    if (indices.rank() == 0 || indices.extents()[indices.rank() - 1] != TupleDepth)
    {
        throw std::invalid_argument("gatherND final indices extent does not match its tuple depth");
    }
    return tensor_selection_detail::gatherNDImpl<outputRank>(source, indices, allocator);
}

template <std::size_t TupleDepth, ReadableTensor Source, ReadableTensor Indices>
    requires std::integral<typename Indices::value_type> &&
             (!std::same_as<std::remove_cv_t<typename Indices::value_type>, bool>)
[[nodiscard]] auto gatherND(const Source& source, const Indices& indices)
{
    using value_type = typename Source::value_type;
    return gatherND<TupleDepth>(
        source, indices, tensor_detail::selectBinaryResultAllocator<value_type>(source, indices));
}

} // namespace fat_p
