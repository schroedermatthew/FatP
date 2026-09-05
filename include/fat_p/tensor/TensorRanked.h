#pragma once

/*
FATP_META:
  meta_version: 1
  component: TensorRanked
  file_role: internal_header
  path: include/fat_p/tensor/TensorRanked.h
  namespace: fat_p
  layer: Domain
  summary: "Fixed-rank, runtime-extents Tensor ownership, views, and dynamic-rank adapters."
  api_stability: in_work
  related:
    docs:
      - components/Tensor/docs/Design Note - Tensor Architecture Additions Plan.md
    headers:
      - include/fat_p/TensorRanked.h
      - include/fat_p/tensor/Tensor.h
    tests:
      - components/Tensor/tests/test_TensorRanked.cpp
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

/** @file TensorRanked.h @brief Fixed-rank, runtime-extents Tensor vocabulary. */

#include "Tensor.h"

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace fat_p
{

template <typename T, typename ShapeT, typename Policy>
class StaticTensor;

inline constexpr std::size_t kDynamicTensorRank = tensor_detail::kDynamicTensorRank;

template <std::size_t Rank>
using RankedExtents = tensor_detail::FixedRankExtents<Rank>;

template <std::size_t Rank>
using RankedTensorLayout = tensor_detail::TensorLayoutFor<Rank>;

template <typename T, std::size_t Rank>
using RankedTensorView = TensorView<T, Rank>;

template <typename T, std::size_t Rank>
using SharedRankedTensorView = SharedTensorView<T, Rank>;

template <typename T, std::size_t Rank, typename Allocator = TensorAllocator<T>>
using RankedTensor = Tensor<T, Allocator, Rank>;

template <typename T, std::size_t Rank>
struct RankedStridedTensorDescriptor
{
    using element_type = T;
    using value_type = std::remove_const_t<T>;
    using pointer = T*;

    pointer storageBase = nullptr;
    std::size_t storageLength = 0;
    std::ptrdiff_t originOffset = 0;
    RankedExtents<Rank> extents;
    std::array<std::ptrdiff_t, Rank> strides{};
    std::shared_ptr<void> sharedStorageLifetime;
    std::weak_ptr<tensor_detail::TensorLifetimeState> lifetime;
    bool tracked = false;

    [[nodiscard]] static constexpr std::size_t rank() noexcept { return Rank; }
    [[nodiscard]] std::size_t size() const noexcept { return extents.logicalSize(); }
    [[nodiscard]] bool empty() const noexcept { return size() == 0; }

    [[nodiscard]] RankedTensorLayout<Rank> layout() const
    {
        return RankedTensorLayout<Rank>(storageLength, originOffset, extents, strides);
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
            throw std::invalid_argument("A non-empty ranked Tensor descriptor requires storage");
        }
        return storageBase + originOffset;
    }

    [[nodiscard]] RankedTensorView<T, Rank> borrow() const &
    {
        return tensor_detail::TensorAccess::makeView<T, Rank>(storageBase, layout(), lifetime, tracked);
    }

    [[nodiscard]] RankedTensorView<T, Rank> borrow() const && = delete;
};

namespace tensor_detail
{

template <typename T>
struct PublicTensorStaticRank
    : std::integral_constant<std::size_t, tensorStaticRankValue<T>>
{
};

template <typename T, typename ShapeT, typename Policy>
struct PublicTensorStaticRank<StaticTensor<T, ShapeT, Policy>>
    : std::integral_constant<std::size_t, ShapeT::rank>
{
};

} // namespace tensor_detail

template <typename T>
struct TensorStaticRank
    : tensor_detail::PublicTensorStaticRank<std::remove_cvref_t<T>>
{
};

template <typename T>
inline constexpr std::size_t tensor_static_rank_v = TensorStaticRank<T>::value;

namespace tensor_detail
{

template <typename Result, typename Source>
[[nodiscard]] auto selectRankedResultAllocator(const Source& source)
{
    if constexpr (requires { source.get_allocator(); })
    {
        using source_allocator = std::remove_cvref_t<decltype(source.get_allocator())>;
        using result_allocator =
            typename std::allocator_traits<source_allocator>::template rebind_alloc<Result>;
        result_allocator rebound(source.get_allocator());
        return std::allocator_traits<result_allocator>::select_on_container_copy_construction(rebound);
    }
    else
    {
        return TensorAllocator<Result>{};
    }
}

template <typename Extents>
[[nodiscard]] DynamicExtents toDynamicExtents(const Extents& extents)
{
    DynamicExtents::container_type values;
    values.reserve(extents.rank());
    for (const auto extent : extents)
    {
        values.push_back(extent);
    }
    return DynamicExtents(std::move(values));
}

template <typename Layout>
[[nodiscard]] TensorLayout toDynamicLayout(const Layout& layout)
{
    return makeDynamicLayout(layout);
}

template <std::size_t Rank, typename Extents>
[[nodiscard]] RankedExtents<Rank> toRankedExtents(const Extents& extents)
{
    if (extents.rank() != Rank)
    {
        throw std::invalid_argument("Tensor rank does not match the requested ranked adapter");
    }
    std::array<std::size_t, Rank> result{};
    std::copy(extents.begin(), extents.end(), result.begin());
    return RankedExtents<Rank>(std::move(result));
}

template <std::size_t Rank, typename Layout>
[[nodiscard]] RankedTensorLayout<Rank> toRankedLayout(const Layout& layout)
{
    return makeFixedLayout<Rank>(layout);
}

} // namespace tensor_detail

template <WritableTensor Source>
    requires(tensor_static_rank_v<Source> != kDynamicTensorRank)
[[nodiscard]] auto asDynamicView(Source& source) -> TensorView<typename Source::value_type>
{
    tensor_detail::TensorAccess::validate(source);
    return tensor_detail::TensorAccess::makeView<typename Source::value_type, kDynamicTensorRank>(
        tensor_detail::TensorAccess::storageBase(source), tensor_detail::toDynamicLayout(source.layout()),
        tensor_detail::TensorAccess::lifetime(source), tensor_detail::TensorAccess::tracked(source));
}

template <ReadableTensor Source>
    requires(tensor_static_rank_v<Source> != kDynamicTensorRank)
[[nodiscard]] auto asDynamicView(const Source& source) -> TensorView<const typename Source::value_type>
{
    tensor_detail::TensorAccess::validate(source);
    return tensor_detail::TensorAccess::makeView<const typename Source::value_type, kDynamicTensorRank>(
        tensor_detail::TensorAccess::storageBase(source), tensor_detail::toDynamicLayout(source.layout()),
        tensor_detail::TensorAccess::lifetime(source), tensor_detail::TensorAccess::tracked(source));
}

template <typename T, std::size_t Rank>
    requires(Rank != kDynamicTensorRank)
[[nodiscard]] auto asDynamicView(const TensorView<T, Rank>& source) -> TensorView<T>
{
    tensor_detail::TensorAccess::validate(source);
    return tensor_detail::TensorAccess::makeView<T, kDynamicTensorRank>(
        tensor_detail::TensorAccess::storageBase(source), tensor_detail::toDynamicLayout(source.layout()),
        tensor_detail::TensorAccess::lifetime(source), tensor_detail::TensorAccess::tracked(source));
}

template <typename T, std::size_t Rank>
    requires(Rank != kDynamicTensorRank)
[[nodiscard]] auto asDynamicView(const SharedTensorView<T, Rank>& source) -> TensorView<T>
{
    tensor_detail::TensorAccess::validate(source);
    return tensor_detail::TensorAccess::makeView<T, kDynamicTensorRank>(
        tensor_detail::TensorAccess::storageBase(source), tensor_detail::toDynamicLayout(source.layout()),
        tensor_detail::TensorAccess::lifetime(source), tensor_detail::TensorAccess::tracked(source));
}

template <std::size_t Rank, WritableTensor Source>
[[nodiscard]] auto asRankedView(Source& source) -> RankedTensorView<typename Source::value_type, Rank>
{
    tensor_detail::TensorAccess::validate(source);
    return tensor_detail::TensorAccess::makeView<typename Source::value_type, Rank>(
        tensor_detail::TensorAccess::storageBase(source), tensor_detail::toRankedLayout<Rank>(source.layout()),
        tensor_detail::TensorAccess::lifetime(source), tensor_detail::TensorAccess::tracked(source));
}

template <std::size_t Rank, ReadableTensor Source>
[[nodiscard]] auto asRankedView(const Source& source) -> RankedTensorView<const typename Source::value_type, Rank>
{
    tensor_detail::TensorAccess::validate(source);
    return tensor_detail::TensorAccess::makeView<const typename Source::value_type, Rank>(
        tensor_detail::TensorAccess::storageBase(source), tensor_detail::toRankedLayout<Rank>(source.layout()),
        tensor_detail::TensorAccess::lifetime(source), tensor_detail::TensorAccess::tracked(source));
}

template <std::size_t Rank, typename T, std::size_t SourceRank>
[[nodiscard]] auto asRankedView(const TensorView<T, SourceRank>& source)
    -> RankedTensorView<T, Rank>
{
    tensor_detail::TensorAccess::validate(source);
    return tensor_detail::TensorAccess::makeView<T, Rank>(
        tensor_detail::TensorAccess::storageBase(source), tensor_detail::toRankedLayout<Rank>(source.layout()),
        tensor_detail::TensorAccess::lifetime(source), tensor_detail::TensorAccess::tracked(source));
}

template <std::size_t Rank, typename T, std::size_t SourceRank>
[[nodiscard]] auto asRankedView(const SharedTensorView<T, SourceRank>& source)
    -> RankedTensorView<T, Rank>
{
    tensor_detail::TensorAccess::validate(source);
    return tensor_detail::TensorAccess::makeView<T, Rank>(
        tensor_detail::TensorAccess::storageBase(source), tensor_detail::toRankedLayout<Rank>(source.layout()),
        tensor_detail::TensorAccess::lifetime(source), tensor_detail::TensorAccess::tracked(source));
}

template <typename Source>
    requires ReadableTensor<std::remove_cvref_t<Source>> && (!std::is_lvalue_reference_v<Source>)
void asDynamicView(Source&&) = delete;

template <std::size_t Rank, typename Source>
    requires ReadableTensor<std::remove_cvref_t<Source>> && (!std::is_lvalue_reference_v<Source>)
void asRankedView(Source&&) = delete;

template <typename T, std::size_t Rank>
[[nodiscard]] auto asDynamicSharedView(SharedRankedTensorView<T, Rank> source)
    -> SharedTensorView<T>
{
    tensor_detail::TensorAccess::validate(source);
    return tensor_detail::TensorAccess::makeSharedView<T, kDynamicTensorRank>(
        tensor_detail::TensorAccess::sharedLifetime(source),
        tensor_detail::TensorAccess::storageBase(source), tensor_detail::toDynamicLayout(source.layout()));
}

template <std::size_t Rank, typename T>
[[nodiscard]] auto asRankedSharedView(SharedTensorView<T> source)
    -> SharedRankedTensorView<T, Rank>
{
    tensor_detail::TensorAccess::validate(source);
    return tensor_detail::TensorAccess::makeSharedView<T, Rank>(
        tensor_detail::TensorAccess::sharedLifetime(source),
        tensor_detail::TensorAccess::storageBase(source), tensor_detail::toRankedLayout<Rank>(source.layout()));
}

template <typename T, typename Allocator, std::size_t Rank>
    requires(Rank != kDynamicTensorRank)
[[nodiscard]] auto asDynamicSharedView(Tensor<T, Allocator, Rank>& source)
    -> SharedTensorView<T>
{
    return asDynamicSharedView(source.asSharedView());
}

template <typename T, typename Allocator, std::size_t Rank>
    requires(Rank != kDynamicTensorRank)
[[nodiscard]] auto asDynamicSharedView(const Tensor<T, Allocator, Rank>& source)
    -> SharedTensorView<const T>
{
    return asDynamicSharedView(source.asSharedView());
}

template <typename T, typename Allocator, std::size_t Rank>
    requires(Rank != kDynamicTensorRank)
void asDynamicSharedView(Tensor<T, Allocator, Rank>&&) = delete;

template <typename T, typename Allocator, std::size_t Rank>
    requires(Rank != kDynamicTensorRank)
void asDynamicSharedView(const Tensor<T, Allocator, Rank>&&) = delete;

template <std::size_t Rank, typename T, typename Allocator>
[[nodiscard]] auto asRankedSharedView(Tensor<T, Allocator>& source)
    -> SharedRankedTensorView<T, Rank>
{
    return asRankedSharedView<Rank>(source.asSharedView());
}

template <std::size_t Rank, typename T, typename Allocator>
[[nodiscard]] auto asRankedSharedView(const Tensor<T, Allocator>& source)
    -> SharedRankedTensorView<const T, Rank>
{
    return asRankedSharedView<Rank>(source.asSharedView());
}

template <std::size_t Rank, typename T, typename Allocator>
void asRankedSharedView(Tensor<T, Allocator>&&) = delete;

template <std::size_t Rank, typename T, typename Allocator>
void asRankedSharedView(const Tensor<T, Allocator>&&) = delete;

template <typename T, typename Allocator, std::size_t Rank>
    requires(Rank != kDynamicTensorRank)
[[nodiscard]] auto describeTensor(Tensor<T, Allocator, Rank>& source)
    -> RankedStridedTensorDescriptor<T, Rank>
{
    tensor_detail::TensorAccess::validate(source);
    return {tensor_detail::TensorAccess::storageBase(source), source.layout().storageLength(),
            source.layout().originOffset(), source.extents(), source.strides(), {},
            tensor_detail::TensorAccess::lifetime(source), true};
}

template <typename T, typename Allocator, std::size_t Rank>
    requires(Rank != kDynamicTensorRank)
[[nodiscard]] auto describeTensor(const Tensor<T, Allocator, Rank>& source)
    -> RankedStridedTensorDescriptor<const T, Rank>
{
    tensor_detail::TensorAccess::validate(source);
    return {tensor_detail::TensorAccess::storageBase(source), source.layout().storageLength(),
            source.layout().originOffset(), source.extents(), source.strides(), {},
            tensor_detail::TensorAccess::lifetime(source), true};
}

template <typename T, std::size_t Rank>
    requires(Rank != kDynamicTensorRank)
[[nodiscard]] auto describeTensor(TensorView<T, Rank>& source)
    -> RankedStridedTensorDescriptor<T, Rank>
{
    tensor_detail::TensorAccess::validate(source);
    return {tensor_detail::TensorAccess::storageBase(source), source.layout().storageLength(),
            source.layout().originOffset(), source.extents(), source.strides(), {},
            tensor_detail::TensorAccess::lifetime(source), tensor_detail::TensorAccess::tracked(source)};
}

template <typename T, std::size_t Rank>
    requires(Rank != kDynamicTensorRank)
[[nodiscard]] auto describeTensor(const TensorView<T, Rank>& source)
    -> RankedStridedTensorDescriptor<T, Rank>
{
    tensor_detail::TensorAccess::validate(source);
    return {tensor_detail::TensorAccess::storageBase(source), source.layout().storageLength(),
            source.layout().originOffset(), source.extents(), source.strides(), {},
            tensor_detail::TensorAccess::lifetime(source), tensor_detail::TensorAccess::tracked(source)};
}

template <typename T, std::size_t Rank>
    requires(Rank != kDynamicTensorRank)
[[nodiscard]] auto describeTensor(SharedTensorView<T, Rank>& source)
    -> RankedStridedTensorDescriptor<T, Rank>
{
    tensor_detail::TensorAccess::validate(source);
    return {tensor_detail::TensorAccess::storageBase(source), source.layout().storageLength(),
            source.layout().originOffset(), source.extents(), source.strides(),
            tensor_detail::TensorAccess::sharedLifetime(source),
            tensor_detail::TensorAccess::lifetime(source), tensor_detail::TensorAccess::tracked(source)};
}

template <typename T, std::size_t Rank>
    requires(Rank != kDynamicTensorRank)
[[nodiscard]] auto describeTensor(const SharedTensorView<T, Rank>& source)
    -> RankedStridedTensorDescriptor<T, Rank>
{
    tensor_detail::TensorAccess::validate(source);
    return {tensor_detail::TensorAccess::storageBase(source), source.layout().storageLength(),
            source.layout().originOffset(), source.extents(), source.strides(),
            tensor_detail::TensorAccess::sharedLifetime(source),
            tensor_detail::TensorAccess::lifetime(source), tensor_detail::TensorAccess::tracked(source)};
}

template <typename T, typename Allocator, std::size_t Rank>
    requires(Rank != kDynamicTensorRank)
void describeTensor(Tensor<T, Allocator, Rank>&&) = delete;

template <typename T, typename Allocator, std::size_t Rank>
    requires(Rank != kDynamicTensorRank)
void describeTensor(const Tensor<T, Allocator, Rank>&&) = delete;

template <typename T, std::size_t Rank>
    requires(Rank != kDynamicTensorRank)
void describeTensor(TensorView<T, Rank>&&) = delete;

template <typename T, std::size_t Rank>
    requires(Rank != kDynamicTensorRank)
void describeTensor(const TensorView<T, Rank>&&) = delete;

template <typename T, std::size_t Rank>
    requires(Rank != kDynamicTensorRank)
void describeTensor(SharedTensorView<T, Rank>&&) = delete;

template <typename T, std::size_t Rank>
    requires(Rank != kDynamicTensorRank)
void describeTensor(const SharedTensorView<T, Rank>&&) = delete;

template <ReadableTensor Source>
    requires(tensor_static_rank_v<Source> != kDynamicTensorRank)
[[nodiscard]] auto toDynamicTensor(const Source& source)
{
    using value_type = typename Source::value_type;
    auto allocator = tensor_detail::selectRankedResultAllocator<value_type>(source);
    Tensor<value_type, decltype(allocator)> result(
        std::allocator_arg, allocator, tensor_detail::toDynamicExtents(source.extents()));
    tensor_detail::copyKernel(source, result);
    return result;
}

template <typename T, typename Allocator, std::size_t Rank>
    requires(Rank != kDynamicTensorRank)
[[nodiscard]] auto toDynamicTensor(Tensor<T, Allocator, Rank>&& source) -> Tensor<T, Allocator>
{
    return Tensor<T, Allocator>(std::move(source));
}

template <std::size_t Rank, ReadableTensor Source>
[[nodiscard]] auto toRankedTensor(const Source& source)
{
    using value_type = typename Source::value_type;
    const auto extents = tensor_detail::toRankedExtents<Rank>(source.extents());
    auto allocator = tensor_detail::selectRankedResultAllocator<value_type>(source);
    RankedTensor<value_type, Rank, decltype(allocator)> result(std::allocator_arg, allocator, extents);
    tensor_detail::copyKernel(source, result);
    return result;
}

template <std::size_t Rank, typename T, typename Allocator, std::size_t SourceRank>
    requires(Rank != SourceRank)
[[nodiscard]] auto toRankedTensor(Tensor<T, Allocator, SourceRank>&& source)
    -> RankedTensor<T, Rank, Allocator>
{
    return RankedTensor<T, Rank, Allocator>(std::move(source));
}

template <std::size_t Rank, typename T, typename Allocator>
[[nodiscard]] auto toRankedTensor(Tensor<T, Allocator, Rank>&& source)
    -> RankedTensor<T, Rank, Allocator>
{
    return RankedTensor<T, Rank, Allocator>(std::move(source));
}

} // namespace fat_p
