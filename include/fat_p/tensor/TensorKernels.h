#pragma once

/*
FATP_META:
  meta_version: 1
  component: TensorAlgorithms
  file_role: internal_header
  path: include/fat_p/tensor/TensorKernels.h
  namespace: fat_p::tensor_detail
  layer: Domain
  summary: "Serial copy, fill, transform, equality, and hash Tensor kernels."
  api_stability: in_work
  related:
    docs:
      - components/Tensor/docs/Design Note - Tensor Architecture Additions Plan.md
    headers:
      - include/fat_p/tensor/TensorIterationPlan.h
      - include/fat_p/tensor/TensorView.h
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
 * @file TensorKernels.h
 * @brief Scalar-reference kernels over TensorIterationPlan.
 */

#include "TensorIterationPlan.h"
#include "TensorView.h"

#include <cmath>
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace fat_p::tensor_detail
{

template <WritableTensor Destination, typename Value>
void fillKernel(Destination& destination, const Value& value)
{
    TensorAccess::validate(destination);
    if (!destination.layout().isInjective())
    {
        throw std::invalid_argument("Tensor fill destination must be injective");
    }
    auto* output = TensorAccess::storageBase(destination);
    const TensorIterationPlan plan(destination.extents(), {std::cref(destination.layout())});
    plan.forEachOffset([&](std::size_t, const auto& offsets) { output[offsets[0]] = value; });
}

template <ReadableTensor Source, WritableTensor Destination>
void validateCopy(const Source& source, Destination& destination)
{
    TensorAccess::validate(source);
    TensorAccess::validate(destination);
    if (source.extents() != destination.extents())
    {
        throw std::invalid_argument("Tensor copy requires identical logical extents");
    }
    if (!destination.layout().isInjective())
    {
        throw std::invalid_argument("Tensor copy destination must be injective");
    }
}

// Requires validated, live mappings of the same element type. Empty mappings
// must be handled before pointer arithmetic; endpoints are reachable elements,
// not potentially out-of-allocation sentinels. Interleaved spans are conservative.
template <ReadableTensor Source, ReadableTensor Destination>
[[nodiscard]] bool reachableRangesDisjoint(const Source& source, const Destination& destination)
{
    if (source.size() == 0 || destination.size() == 0)
    {
        return true;
    }
    const auto* input = TensorAccess::storageBase(source);
    const auto* output = TensorAccess::storageBase(destination);
    const std::less<const typename Source::value_type*> before;
    return before(input + *source.layout().maximumOffset(), output + *destination.layout().minimumOffset()) ||
           before(output + *destination.layout().maximumOffset(), input + *source.layout().minimumOffset());
}

// Internal primitive: the caller must establish disjointness or stage aliases.
template <ReadableTensor Source, WritableTensor Destination>
void copyKernel(const Source& source, Destination& destination)
{
    validateCopy(source, destination);
    const auto* input = TensorAccess::storageBase(source);
    auto* output = TensorAccess::storageBase(destination);
    const TensorIterationPlan plan(source.extents(),
                                   {std::cref(source.layout()), std::cref(destination.layout())});
    plan.forEachOffset(
        [&](std::size_t, const auto& offsets) { output[offsets[1]] = input[offsets[0]]; });
}

template <ReadableTensor Source, WritableTensor Destination, typename Function>
void unaryKernel(const Source& source, Destination& destination, Function&& function)
{
    TensorAccess::validate(source);
    TensorAccess::validate(destination);
    if (source.extents() != destination.extents())
    {
        throw std::invalid_argument("Tensor unary kernel requires identical logical extents");
    }
    if (!destination.layout().isInjective())
    {
        throw std::invalid_argument("Tensor unary destination must be injective");
    }
    const auto* input = TensorAccess::storageBase(source);
    auto* output = TensorAccess::storageBase(destination);
    const TensorIterationPlan plan(source.extents(),
                                   {std::cref(source.layout()), std::cref(destination.layout())});
    plan.forEachOffset([&](std::size_t, const auto& offsets) {
        output[offsets[1]] = std::invoke(function, input[offsets[0]]);
    });
}

template <ReadableTensor Left, ReadableTensor Right, WritableTensor Destination, typename Function>
void binaryKernel(const Left& left, const Right& right, Destination& destination, Function&& function)
{
    TensorAccess::validate(left);
    TensorAccess::validate(right);
    TensorAccess::validate(destination);
    const auto expected =
        TensorIterationPlan::broadcastExtents({std::cref(left.layout()), std::cref(right.layout())});
    if (destination.extents() != expected)
    {
        throw std::invalid_argument("Tensor binary destination has incorrect broadcast extents");
    }
    if (!destination.layout().isInjective())
    {
        throw std::invalid_argument("Tensor binary destination must be injective");
    }
    const auto* leftData = TensorAccess::storageBase(left);
    const auto* rightData = TensorAccess::storageBase(right);
    auto* output = TensorAccess::storageBase(destination);
    const TensorIterationPlan plan(expected, {std::cref(left.layout()), std::cref(right.layout()),
                                              std::cref(destination.layout())});
    plan.forEachOffset([&](std::size_t, const auto& offsets) {
        output[offsets[2]] = std::invoke(function, leftData[offsets[0]], rightData[offsets[1]]);
    });
}

template <ReadableTensor Left, ReadableTensor Right, typename Predicate>
[[nodiscard]] bool forEachPairKernel(const Left& left, const Right& right, Predicate&& predicate)
{
    TensorAccess::validate(left);
    TensorAccess::validate(right);
    if (left.extents() != right.extents())
    {
        return false;
    }
    const auto* leftData = TensorAccess::storageBase(left);
    const auto* rightData = TensorAccess::storageBase(right);
    const TensorIterationPlan plan(left.extents(), {std::cref(left.layout()), std::cref(right.layout())});
    plan.forEachOffset([&](std::size_t linearIndex, const auto& offsets) {
        std::invoke(predicate, linearIndex, leftData[offsets[0]], rightData[offsets[1]]);
    });
    return true;
}

template <ReadableTensor Left, ReadableTensor Right, typename Predicate>
[[nodiscard]] bool equalKernel(const Left& left, const Right& right, Predicate&& predicate)
{
    bool equal = true;
    if (!forEachPairKernel(left, right, [&](std::size_t, const auto& leftValue, const auto& rightValue) {
            if (equal && !std::invoke(predicate, leftValue, rightValue))
            {
                equal = false;
            }
        }))
    {
        return false;
    }
    return equal;
}

template <ReadableTensor Source, typename Hasher>
[[nodiscard]] std::size_t hashKernel(const Source& source, Hasher&& hasher)
{
    TensorAccess::validate(source);
    std::size_t seed = source.rank();
    const auto combine = [&seed](std::size_t value) {
        seed ^= value + static_cast<std::size_t>(0x9e3779b9U) + (seed << 6U) + (seed >> 2U);
    };
    for (const auto extent : source.extents())
    {
        combine(std::hash<std::size_t>{}(extent));
    }
    const auto* input = TensorAccess::storageBase(source);
    const TensorIterationPlan plan(source.extents(), {std::cref(source.layout())});
    plan.forEachOffset([&](std::size_t, const auto& offsets) { combine(std::invoke(hasher, input[offsets[0]])); });
    return seed;
}

} // namespace fat_p::tensor_detail
