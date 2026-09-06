#pragma once

/*
FATP_META:
  meta_version: 1
  component: TensorInterop
  file_role: internal_header
  path: include/fat_p/tensor/TensorInterop.h
  namespace: fat_p
  layer: Domain
  summary: "Dependency-light span, strided descriptor, mdspan, and StaticTensor interop."
  api_stability: in_work
  related:
    docs:
      - components/Tensor/docs/Design Note - Tensor Architecture Additions Plan.md
    headers:
      - include/fat_p/TensorInterop.h
      - include/fat_p/tensor/TensorRanked.h
      - include/fat_p/tensor/TensorStatic.h
    tests:
      - components/Tensor/tests/test_TensorInterop.cpp
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

/** @file TensorInterop.h @brief Explicit borrowed descriptors and fixed/dynamic conversion. */

#include "../CppFeatureDetection.h"
#include "TensorRanked.h"
#include "TensorStatic.h"

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace fat_p
{

namespace tensor_detail
{

// Const owners expose const elements; const view wrappers retain their element type.
template <typename Source>
using TensorInteropElement =
    std::remove_pointer_t<decltype(TensorAccess::storageBase(std::declval<Source&>()))>;

#if FATP_HAS_MDSPAN
template <std::size_t Rank, ReadableTensor Source>
[[nodiscard]] auto mdspanStrides(const Source& source)
{
    TensorAccess::validate(source);
    if (source.rank() != Rank)
    {
        throw std::invalid_argument("asMdspan rank must match the Tensor mapping");
    }
    if (!source.layout().isInjective())
    {
        throw std::invalid_argument("asMdspan requires an injective Tensor mapping");
    }
    std::array<std::size_t, Rank> strides{};
    std::array<std::size_t, Rank> order{};
    for (std::size_t axis = 0; axis < Rank; ++axis)
    {
        if (source.strides()[axis] < 0)
        {
            throw std::invalid_argument("std::layout_stride interop does not represent negative Tensor strides");
        }
        // Singleton strides do not affect addresses and may otherwise prevent
        // a valid layout_stride permutation (even when they were positive).
        strides[axis] = source.extents()[axis] == 1 || source.strides()[axis] == 0
                            ? std::size_t{1} : static_cast<std::size_t>(source.strides()[axis]);
        order[axis] = axis;
    }
    if (source.empty())
    {
        // Empty mappings have no addresses to preserve. Use positive canonical
        // placeholders rather than replacing each zero stride independently.
        std::size_t running = 1;
        for (std::size_t reverse = Rank; reverse > 0; --reverse)
        {
            const auto axis = reverse - 1;
            strides[axis] = running;
            const auto factor = std::max(source.extents()[axis], std::size_t{1});
            if (axis != 0)
            {
                if (running > std::numeric_limits<std::size_t>::max() / factor)
                {
                    throw std::overflow_error("Empty mdspan placeholder strides exceed size_t");
                }
                running *= factor;
            }
        }
    }
    const auto before = [&](std::size_t left, std::size_t right) {
        return strides[left] < strides[right] ||
               (strides[left] == strides[right] && source.extents()[left] < source.extents()[right]);
    };
    // Bounded insertion sort also handles rank zero and singleton arrays.
    for (std::size_t i = 1; i < Rank; ++i)
    {
        const auto axis = order[i];
        auto j = i;
        while (j > 0 && before(axis, order[j - 1]))
        {
            order[j] = order[j - 1];
            --j;
        }
        order[j] = axis;
    }
    for (std::size_t i = 1; i < Rank; ++i)
    {
        const auto previous = order[i - 1];
        const auto extent = source.extents()[previous];
        // layout_stride requires a permutation with nextStride >= stride*extent,
        // which is stronger than general injectivity. Divide to avoid overflow.
        if (extent != 0 && strides[previous] > strides[order[i]] / extent)
        {
            throw std::invalid_argument("Tensor strides do not satisfy std::layout_stride requirements");
        }
    }
    return strides;
}
#endif

} // namespace tensor_detail

template <typename T>
struct StridedTensorDescriptor
{
    using element_type = T;
    using value_type = std::remove_const_t<T>;
    using pointer = T*;

    pointer storageBase = nullptr;
    std::size_t storageLength = 0;
    std::ptrdiff_t originOffset = 0;
    DynamicExtents extents;
    TensorStrides strides;
    std::shared_ptr<void> sharedStorageLifetime;
    std::weak_ptr<tensor_detail::TensorLifetimeState> lifetime;
    bool tracked = false;

    [[nodiscard]] std::size_t rank() const noexcept { return extents.rank(); }
    [[nodiscard]] std::size_t size() const noexcept { return extents.logicalSize(); }
    [[nodiscard]] bool empty() const noexcept { return size() == 0; }

    [[nodiscard]] TensorLayout layout() const
    {
        return TensorLayout(storageLength, originOffset, extents, strides);
    }

    [[nodiscard]] pointer logicalData() const
    {
        const auto validatedLayout = layout();
        if (validatedLayout.isEmpty())
        {
            return storageBase;
        }
        if (storageBase == nullptr)
        {
            throw std::invalid_argument("A non-empty strided descriptor requires storage");
        }
        return storageBase + originOffset;
    }

    [[nodiscard]] TensorView<T> borrow() const &
    {
        return tensor_detail::TensorAccess::makeView(storageBase, layout(), lifetime, tracked);
    }

    [[nodiscard]] TensorView<T> borrow() const && = delete;
};

template <WritableTensor Source>
[[nodiscard]] auto describeTensor(Source& source) -> StridedTensorDescriptor<typename Source::value_type>
{
    tensor_detail::TensorAccess::validate(source);
    return {tensor_detail::TensorAccess::storageBase(source), source.layout().storageLength(),
            source.layout().originOffset(), source.extents(), source.strides(),
            tensor_detail::TensorAccess::sharedLifetime(source),
            tensor_detail::TensorAccess::lifetime(source), tensor_detail::TensorAccess::tracked(source)};
}

template <ReadableTensor Source>
[[nodiscard]] auto describeTensor(const Source& source)
    -> StridedTensorDescriptor<tensor_detail::TensorInteropElement<const Source>>
{
    tensor_detail::TensorAccess::validate(source);
    return {tensor_detail::TensorAccess::storageBase(source), source.layout().storageLength(),
            source.layout().originOffset(), source.extents(), source.strides(),
            tensor_detail::TensorAccess::sharedLifetime(source),
            tensor_detail::TensorAccess::lifetime(source), tensor_detail::TensorAccess::tracked(source)};
}

template <typename Source>
    requires ReadableTensor<std::remove_cvref_t<Source>> && (!std::is_lvalue_reference_v<Source>)
void describeTensor(Source&&) = delete;

template <WritableTensor Source>
[[nodiscard]] auto contiguousSpan(Source& source) -> std::span<typename Source::value_type>
{
    tensor_detail::TensorAccess::validate(source);
    if (!source.layout().isContiguous())
    {
        throw std::logic_error("contiguousSpan requires a contiguous Tensor mapping");
    }
    auto descriptor = describeTensor(source);
    return {descriptor.logicalData(), descriptor.size()};
}

template <ReadableTensor Source>
[[nodiscard]] auto contiguousSpan(const Source& source)
    -> std::span<tensor_detail::TensorInteropElement<const Source>>
{
    tensor_detail::TensorAccess::validate(source);
    if (!source.layout().isContiguous())
    {
        throw std::logic_error("contiguousSpan requires a contiguous Tensor mapping");
    }
    auto descriptor = describeTensor(source);
    return {descriptor.logicalData(), descriptor.size()};
}

template <typename Source>
    requires ReadableTensor<std::remove_cvref_t<Source>> && (!std::is_lvalue_reference_v<Source>)
void contiguousSpan(Source&&) = delete;

template <typename T, typename ShapeT, typename Policy>
[[nodiscard]] Tensor<T> toTensor(const StaticTensor<T, ShapeT, Policy>& source)
{
    std::vector<std::size_t> extents(ShapeT::dims.begin(), ShapeT::dims.end());
    Tensor<T> result(DynamicExtents(std::move(extents)));
    std::copy(source.begin(), source.end(), result.begin());
    return tensor_detail::TensorAccess::finishMaterialization(std::move(result));
}

template <typename T, typename ShapeT, typename Policy>
[[nodiscard]] auto toRankedTensor(const StaticTensor<T, ShapeT, Policy>& source)
    -> RankedTensor<T, ShapeT::rank>
{
    static_assert(ShapeT::rank != kDynamicTensorRank,
                  "StaticTensor conversion requires a compile-time rank");
    for (const auto extent : ShapeT::dims)
    {
        if (extent == 0)
        {
            throw std::invalid_argument("StaticTensor zero extents cannot be converted to RankedTensor");
        }
    }
    RankedExtents<ShapeT::rank> extents(ShapeT::dims.begin(), ShapeT::dims.end());
    RankedTensor<T, ShapeT::rank> result(extents);
    std::copy(source.begin(), source.end(), result.begin());
    return tensor_detail::TensorAccess::finishMaterialization(std::move(result));
}

template <typename ShapeT, typename Policy = UncheckedPolicy, ReadableTensor Source>
[[nodiscard]] auto toStaticTensor(const Source& source)
    -> StaticTensor<typename Source::value_type, ShapeT, Policy>
{
    if (source.rank() != ShapeT::rank)
    {
        throw std::invalid_argument("Dynamic Tensor extents do not match the requested StaticTensor shape");
    }
    for (std::size_t axis = 0; axis < source.rank(); ++axis)
    {
        if (source.extents()[axis] != ShapeT::dims[axis])
        {
            throw std::invalid_argument("Dynamic Tensor extents do not match the requested StaticTensor shape");
        }
    }
    StaticTensor<typename Source::value_type, ShapeT, Policy> result;
    for (std::size_t index = 0; index < source.size(); ++index)
    {
        result[index] = source[index];
    }
    return result;
}

#if FATP_HAS_MDSPAN
template <std::size_t Rank, WritableTensor Source>
    requires(tensor_detail::tensorStaticRankValue<Source> == tensor_detail::kDynamicTensorRank ||
             tensor_detail::tensorStaticRankValue<Source> == Rank)
[[nodiscard]] auto asMdspan(Source& source)
{
    const auto strides = tensor_detail::mdspanStrides<Rank>(source);
    std::array<std::size_t, Rank> extents{};
    std::copy(source.extents().begin(), source.extents().end(), extents.begin());
    using extents_type = std::dextents<std::size_t, Rank>;
    using mapping_type = typename std::layout_stride::template mapping<extents_type>;
    const auto descriptor = describeTensor(source);
    return std::mdspan<typename Source::value_type, extents_type, std::layout_stride>(
        descriptor.logicalData(), mapping_type(extents_type(extents), strides));
}

template <std::size_t Rank, ReadableTensor Source>
    requires(tensor_detail::tensorStaticRankValue<Source> == tensor_detail::kDynamicTensorRank ||
             tensor_detail::tensorStaticRankValue<Source> == Rank)
[[nodiscard]] auto asMdspan(const Source& source)
{
    const auto strides = tensor_detail::mdspanStrides<Rank>(source);
    std::array<std::size_t, Rank> extents{};
    std::copy(source.extents().begin(), source.extents().end(), extents.begin());
    using extents_type = std::dextents<std::size_t, Rank>;
    using mapping_type = typename std::layout_stride::template mapping<extents_type>;
    const auto descriptor = describeTensor(source);
    return std::mdspan<tensor_detail::TensorInteropElement<const Source>, extents_type, std::layout_stride>(
        descriptor.logicalData(), mapping_type(extents_type(extents), strides));
}

template <std::size_t Rank, typename Source>
    requires ReadableTensor<std::remove_cvref_t<Source>> && (!std::is_lvalue_reference_v<Source>)
void asMdspan(Source&&) = delete;
#endif

} // namespace fat_p
