#pragma once

/*
FATP_META:
  meta_version: 1
  component: TensorView
  file_role: internal_header
  path: include/fat_p/tensor/TensorView.h
  namespace: fat_p
  layer: Domain
  summary: "Borrowed and shared-lifetime Tensor mappings over validated layouts."
  api_stability: in_work
  related:
    docs:
      - components/Tensor/docs/Design Note - Tensor Semantic Contract.md
      - components/Tensor/docs/Design Note - Tensor Architecture Additions Plan.md
    headers:
      - include/fat_p/TensorView.h
      - include/fat_p/TensorLayout.h
    tests:
      - components/Tensor/tests/test_TensorView.cpp
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
 * @file TensorView.h
 * @brief Borrowed and shared-lifetime Tensor mappings.
 */

#include "TensorSlice.h"

#include <array>
#include <atomic>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace fat_p
{

template <typename T, typename Allocator, std::size_t Rank>
class Tensor;

template <typename T, std::size_t Rank = tensor_detail::kDynamicTensorRank>
class TensorView;

template <typename T, std::size_t Rank = tensor_detail::kDynamicTensorRank>
class SharedTensorView;

namespace tensor_detail
{

struct TensorAccess;

struct TensorLifetimeState
{
    std::atomic<bool> alive{true};
};

inline void invalidateLifetime(const std::shared_ptr<TensorLifetimeState>& state) noexcept
{
    if (state)
    {
        state->alive.store(false, std::memory_order_release);
    }
}

inline void checkLifetime(const std::weak_ptr<TensorLifetimeState>& state, bool tracked)
{
#ifndef NDEBUG
    if (tracked)
    {
        const auto locked = state.lock();
        if (!locked || !locked->alive.load(std::memory_order_acquire))
        {
            throw std::runtime_error("Dangling TensorView: source owner is no longer valid");
        }
    }
#else
    (void)state;
    (void)tracked;
#endif
}

template <std::size_t Rank>
[[nodiscard]] BasicTensorLayout<Rank>
sliceLayout(const BasicTensorLayout<Rank>& source, const std::vector<std::size_t>& start,
            const std::vector<std::size_t>& end)
{
    if (start.size() != source.rank() || end.size() != source.rank())
    {
        throw std::invalid_argument("Tensor slice bounds must match the source rank");
    }

    typename BasicTensorLayout<Rank>::extents_type::container_type resultExtents{};
    if constexpr (Rank == kDynamicTensorRank)
    {
        resultExtents.resize(source.rank());
    }
    bool empty = false;
    for (std::size_t axis = 0; axis < source.rank(); ++axis)
    {
        if (start[axis] > end[axis] || end[axis] > source.extents()[axis])
        {
            throw std::out_of_range("Tensor slice bounds are outside the source extents");
        }
        resultExtents[axis] = end[axis] - start[axis];
        empty = empty || resultExtents[axis] == 0;
    }

    std::ptrdiff_t origin = source.originOffset();
    if (!empty)
    {
        for (std::size_t axis = 0; axis < source.rank(); ++axis)
        {
            origin = checkedOffsetAdd(origin, checkedStrideContribution(start[axis], source.strides()[axis]));
        }
    }
    return BasicTensorLayout<Rank>(source.storageLength(), origin,
                                   typename BasicTensorLayout<Rank>::extents_type(std::move(resultExtents)),
                                   source.strides());
}

template <std::size_t Rank>
[[nodiscard]] BasicTensorLayout<Rank> transposeLayout(const BasicTensorLayout<Rank>& source)
{
    if (source.rank() != 2)
    {
        throw std::invalid_argument("transposeView requires a rank-two Tensor mapping");
    }
    typename BasicTensorLayout<Rank>::extents_type::container_type extents{};
    typename BasicTensorLayout<Rank>::strides_type strides{};
    if constexpr (Rank == kDynamicTensorRank)
    {
        extents.resize(2);
        strides.resize(2);
    }
    extents[0] = source.extents()[1];
    extents[1] = source.extents()[0];
    strides[0] = source.strides()[1];
    strides[1] = source.strides()[0];
    return BasicTensorLayout<Rank>(source.storageLength(), source.originOffset(),
                                   typename BasicTensorLayout<Rank>::extents_type(std::move(extents)),
                                   std::move(strides));
}

template <std::size_t SourceRank, std::size_t TargetRank>
[[nodiscard]] BasicTensorLayout<TargetRank>
reshapeLayoutImpl(const BasicTensorLayout<SourceRank>& source, TensorExtentsFor<TargetRank> target)
{
    if (!source.isContiguous())
    {
        throw std::invalid_argument("reshapeView requires a contiguous Tensor mapping");
    }
    if (source.logicalSize() != target.logicalSize())
    {
        throw std::invalid_argument("reshapeView cannot change the logical element count");
    }
    auto strides = BasicTensorLayout<TargetRank>::canonicalStrides(target);
    return BasicTensorLayout<TargetRank>(source.storageLength(), source.originOffset(),
                                         std::move(target), std::move(strides));
}

template <std::size_t SourceRank>
[[nodiscard]] TensorLayout reshapeLayout(const BasicTensorLayout<SourceRank>& source,
                                         DynamicExtents target)
{
    return reshapeLayoutImpl<SourceRank, kDynamicTensorRank>(source, std::move(target));
}

template <std::size_t TargetRank, std::size_t SourceRank>
[[nodiscard]] BasicTensorLayout<TargetRank>
reshapeLayout(const BasicTensorLayout<SourceRank>& source, FixedRankExtents<TargetRank> target)
{
    return reshapeLayoutImpl<SourceRank, TargetRank>(source, std::move(target));
}

template <std::size_t SourceRank, std::size_t TargetRank>
[[nodiscard]] BasicTensorLayout<TargetRank>
broadcastLayoutImpl(const BasicTensorLayout<SourceRank>& source, TensorExtentsFor<TargetRank> target)
{
    if (source.rank() > target.rank())
    {
        throw std::invalid_argument("broadcastView target rank is smaller than the source rank");
    }

    TensorStridesFor<TargetRank> resultStrides{};
    if constexpr (TargetRank == kDynamicTensorRank)
    {
        resultStrides.resize(target.rank(), 0);
    }
    const auto rankPadding = target.rank() - source.rank();
    for (std::size_t targetAxis = 0; targetAxis < target.rank(); ++targetAxis)
    {
        if (targetAxis < rankPadding)
        {
            resultStrides[targetAxis] = 0;
            continue;
        }
        const auto sourceAxis = targetAxis - rankPadding;
        const auto sourceExtent = source.extents()[sourceAxis];
        const auto targetExtent = target[targetAxis];
        if (sourceExtent != targetExtent && sourceExtent != 1)
        {
            throw std::invalid_argument("broadcastView extents are not broadcast-compatible");
        }
        resultStrides[targetAxis] = sourceExtent == 1 && targetExtent != 1 ? 0 : source.strides()[sourceAxis];
    }
    return BasicTensorLayout<TargetRank>(source.storageLength(), source.originOffset(), std::move(target),
                                         std::move(resultStrides));
}

template <std::size_t SourceRank>
[[nodiscard]] TensorLayout broadcastLayout(const BasicTensorLayout<SourceRank>& source,
                                           DynamicExtents target)
{
    return broadcastLayoutImpl<SourceRank, kDynamicTensorRank>(source, std::move(target));
}

template <std::size_t TargetRank, std::size_t SourceRank>
[[nodiscard]] BasicTensorLayout<TargetRank>
broadcastLayout(const BasicTensorLayout<SourceRank>& source, FixedRankExtents<TargetRank> target)
{
    return broadcastLayoutImpl<SourceRank, TargetRank>(source, std::move(target));
}

} // namespace tensor_detail

template <typename Element, std::size_t Rank = tensor_detail::kDynamicTensorRank>
class TensorLogicalIterator
{
public:
    using iterator_category = std::forward_iterator_tag;
    using iterator_concept = std::forward_iterator_tag;
    using value_type = std::remove_const_t<Element>;
    using difference_type = std::ptrdiff_t;
    using pointer = Element*;
    using reference = Element&;

    TensorLogicalIterator() = default;

    using layout_type = tensor_detail::TensorLayoutFor<Rank>;

    TensorLogicalIterator(pointer storageBase, layout_type layout, std::size_t linearIndex,
                          std::weak_ptr<tensor_detail::TensorLifetimeState> lifetime = {}, bool tracked = false)
        : mStorageBase(storageBase)
        , mLayout(std::move(layout))
        , mLinearIndex(linearIndex)
        , mLifetime(std::move(lifetime))
        , mTracked(tracked)
    {
    }

    [[nodiscard]] reference operator*() const
    {
        tensor_detail::checkLifetime(mLifetime, mTracked);
        if (mStorageBase == nullptr || mLinearIndex >= mLayout.logicalSize())
        {
            throw std::out_of_range("Cannot dereference a singular or end Tensor iterator");
        }
        return mStorageBase[mLayout.logicalOffset(mLinearIndex)];
    }

    [[nodiscard]] pointer operator->() const
    {
        return std::addressof(operator*());
    }

    TensorLogicalIterator& operator++()
    {
        ++mLinearIndex;
        return *this;
    }

    TensorLogicalIterator operator++(int)
    {
        auto copy = *this;
        ++(*this);
        return copy;
    }

    friend bool operator==(const TensorLogicalIterator& left, const TensorLogicalIterator& right)
    {
        if (left.mStorageBase != right.mStorageBase || left.mLayout != right.mLayout ||
            left.mLinearIndex != right.mLinearIndex || left.mTracked != right.mTracked)
        {
            return false;
        }
        return !left.mTracked || (!left.mLifetime.owner_before(right.mLifetime) &&
                                  !right.mLifetime.owner_before(left.mLifetime));
    }

private:
    pointer mStorageBase = nullptr;
    layout_type mLayout = [] {
        if constexpr (Rank == tensor_detail::kDynamicTensorRank)
        {
            return layout_type::contiguous(DynamicExtents{0});
        }
        else
        {
            return layout_type::contiguous(typename layout_type::extents_type{});
        }
    }();
    std::size_t mLinearIndex = 0;
    std::weak_ptr<tensor_detail::TensorLifetimeState> mLifetime;
    bool mTracked = false;
};

template <typename T, std::size_t Rank>
class TensorView
{
public:
    using element_type = T;
    using value_type = std::remove_const_t<T>;
    using reference = T&;
    using pointer = T*;
    using extents_type = tensor_detail::TensorExtentsFor<Rank>;
    using layout_type = tensor_detail::TensorLayoutFor<Rank>;
    using strides_type = tensor_detail::TensorStridesFor<Rank>;
    using iterator = TensorLogicalIterator<T, Rank>;
    using const_iterator = TensorLogicalIterator<const value_type, Rank>;
    static constexpr std::size_t static_rank = Rank;

    TensorView()
        requires(Rank == tensor_detail::kDynamicTensorRank)
    = default;
    TensorView(const TensorView&) = default;
    TensorView(TensorView&& other) noexcept
        : mStorageBase(other.mStorageBase)
        , mLayout(moveOrCopyLayout(other))
        , mLifetime(moveOrCopyLifetime(other))
        , mTracked(other.mTracked)
    {
        if constexpr (Rank == tensor_detail::kDynamicTensorRank)
        {
            other.mStorageBase = nullptr;
            other.mTracked = false;
        }
    }
    TensorView& operator=(const TensorView& other)
    {
        if (this != &other)
        {
            TensorView replacement(other);
            swapState(replacement);
        }
        return *this;
    }
    TensorView& operator=(TensorView&& other) noexcept
    {
        if (this != &other)
        {
            TensorView replacement(std::move(other));
            swapState(replacement);
        }
        return *this;
    }

    template <typename U>
        requires(std::is_const_v<T> && std::same_as<std::remove_const_t<T>, std::remove_const_t<U>> &&
                 !std::is_const_v<U>)
    TensorView(const TensorView<U, Rank>& other)
        : mStorageBase(other.mStorageBase)
        , mLayout(other.mLayout)
        , mLifetime(other.mLifetime)
        , mTracked(other.mTracked)
    {
    }

    [[nodiscard]] static TensorView borrow(pointer storageBase, layout_type layout)
    {
        return TensorView(storageBase, std::move(layout), {}, false);
    }

    [[nodiscard]] const layout_type& layout() const noexcept
    {
        return mLayout;
    }

    [[nodiscard]] const extents_type& extents() const noexcept
    {
        return mLayout.extents();
    }

    [[nodiscard]] const strides_type& strides() const noexcept
    {
        return mLayout.strides();
    }

    [[nodiscard]] std::size_t size() const noexcept
    {
        return mLayout.logicalSize();
    }

    [[nodiscard]] std::size_t rank() const noexcept
    {
        return mLayout.rank();
    }

    [[nodiscard]] std::size_t extent(std::size_t axis) const
    {
        return mLayout.extents().at(axis);
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return size() == 0;
    }

    [[nodiscard]] reference operator[](std::size_t linearIndex) const
    {
        checkAlive();
        return mStorageBase[mLayout.logicalOffset(linearIndex)];
    }

    [[nodiscard]] reference atLinear(std::size_t linearIndex) const
    {
        return (*this)[linearIndex];
    }

    template <std::integral... Indices>
        requires(Rank == tensor_detail::kDynamicTensorRank || sizeof...(Indices) == Rank)
    [[nodiscard]] reference operator()(Indices... indices) const
    {
        return atIndices(std::array<std::ptrdiff_t, sizeof...(Indices)>{static_cast<std::ptrdiff_t>(indices)...});
    }

    [[nodiscard]] iterator begin() const
    {
        checkAlive();
        return iterator(mStorageBase, mLayout, 0, mLifetime, mTracked);
    }

    [[nodiscard]] iterator end() const
    {
        checkAlive();
        return iterator(mStorageBase, mLayout, size(), mLifetime, mTracked);
    }

    [[nodiscard]] pointer data() const
    {
        checkAlive();
        if (!mLayout.isContiguous())
        {
            throw std::logic_error("TensorView::data requires a contiguous mapping");
        }
        if (empty() || mStorageBase == nullptr)
        {
            return mStorageBase;
        }
        return mStorageBase + mLayout.originOffset();
    }

    [[nodiscard]] TensorView sliceView(const std::vector<std::size_t>& start,
                                       const std::vector<std::size_t>& end) const
    {
        checkAlive();
        if constexpr (Rank == tensor_detail::kDynamicTensorRank)
        {
            return TensorView(mStorageBase, tensor_detail::sliceLayout(mLayout, start, end), mLifetime, mTracked);
        }
        else
        {
            return TensorView(mStorageBase, tensor_detail::sliceLayout(mLayout, start, end), mLifetime, mTracked);
        }
    }

    [[nodiscard]] auto sliceView(const std::vector<SliceSpec>& specifications) const
    {
        checkAlive();
        if constexpr (Rank == tensor_detail::kDynamicTensorRank)
        {
            return TensorView<T>(mStorageBase, tensor_detail::extendedSliceLayout(mLayout, specifications),
                                 mLifetime, mTracked);
        }
        else
        {
            return TensorView<T>(mStorageBase,
                                 tensor_detail::extendedSliceLayout(
                                     tensor_detail::makeDynamicLayout(mLayout), specifications),
                                 mLifetime, mTracked);
        }
    }

    [[nodiscard]] auto sliceView(std::initializer_list<SliceSpec> specifications) const
    {
        return sliceView(std::vector<SliceSpec>(specifications));
    }

    template <typename... Specifications>
        requires(Rank != tensor_detail::kDynamicTensorRank && sizeof...(Specifications) > 0 &&
                 tensor_detail::typedSliceConsumedAxes<Specifications...> <= Rank &&
                 tensor_detail::typedSliceEllipsisCount<Specifications...> <= 1 &&
                 (tensor_detail::isTypedSliceSpecification<Specifications> && ...))
    [[nodiscard]] auto sliceView(Specifications&&... specifications) const
    {
        constexpr auto ResultRank = tensor_detail::typedSliceResultRank<Rank, Specifications...>;
        checkAlive();
        std::vector<SliceSpec> dynamicSpecifications;
        dynamicSpecifications.reserve(sizeof...(Specifications));
        (dynamicSpecifications.push_back(
             tensor_detail::makeSliceSpec(std::forward<Specifications>(specifications))),
         ...);
        auto transformed = tensor_detail::extendedSliceLayout(
            tensor_detail::makeDynamicLayout(mLayout), dynamicSpecifications);
        return TensorView<T, ResultRank>(mStorageBase, tensor_detail::makeFixedLayout<ResultRank>(transformed),
                                         mLifetime, mTracked);
    }

    [[nodiscard]] TensorView permuteView(const std::vector<TensorAxis>& order) const
    {
        checkAlive();
        if constexpr (Rank == tensor_detail::kDynamicTensorRank)
        {
            return TensorView(mStorageBase, tensor_detail::permuteLayout(mLayout, order), mLifetime, mTracked);
        }
        else
        {
            auto transformed = tensor_detail::permuteLayout(tensor_detail::makeDynamicLayout(mLayout), order);
            return TensorView(mStorageBase, tensor_detail::makeFixedLayout<Rank>(transformed), mLifetime, mTracked);
        }
    }

    [[nodiscard]] auto squeezeView(const std::vector<TensorAxis>& axes = {}) const
    {
        checkAlive();
        if constexpr (Rank == tensor_detail::kDynamicTensorRank)
        {
            return TensorView<T>(mStorageBase, tensor_detail::squeezeLayout(mLayout, axes), mLifetime, mTracked);
        }
        else
        {
            return TensorView<T>(mStorageBase,
                                 tensor_detail::squeezeLayout(tensor_detail::makeDynamicLayout(mLayout), axes),
                                 mLifetime, mTracked);
        }
    }

    template <TensorAxis... Axes>
        requires(Rank != tensor_detail::kDynamicTensorRank && sizeof...(Axes) > 0)
    [[nodiscard]] auto squeezeView() const
    {
        static_assert(sizeof...(Axes) <= Rank, "Tensor squeeze removes more axes than the source rank");
        checkAlive();
        const std::vector<TensorAxis> axes{Axes...};
        auto transformed = tensor_detail::squeezeLayout(tensor_detail::makeDynamicLayout(mLayout), axes);
        return TensorView<T, Rank - sizeof...(Axes)>(
            mStorageBase, tensor_detail::makeFixedLayout<Rank - sizeof...(Axes)>(transformed),
            mLifetime, mTracked);
    }

    [[nodiscard]] auto unsqueezeView(TensorAxis axis) const
    {
        checkAlive();
        if constexpr (Rank == tensor_detail::kDynamicTensorRank)
        {
            return TensorView<T>(mStorageBase, tensor_detail::unsqueezeLayout(mLayout, axis), mLifetime, mTracked);
        }
        else
        {
            auto transformed = tensor_detail::unsqueezeLayout(tensor_detail::makeDynamicLayout(mLayout), axis);
            return TensorView<T, Rank + 1>(mStorageBase, tensor_detail::makeFixedLayout<Rank + 1>(transformed),
                                           mLifetime, mTracked);
        }
    }

    [[nodiscard]] TensorView rowView(std::size_t row) const
    {
        if (rank() != 2 || row >= extent(0))
        {
            throw std::out_of_range("rowView requires a valid row in a rank-two mapping");
        }
        return sliceView({row, 0}, {row + 1, extent(1)});
    }

    [[nodiscard]] TensorView columnView(std::size_t column) const
    {
        if (rank() != 2 || column >= extent(1))
        {
            throw std::out_of_range("columnView requires a valid column in a rank-two mapping");
        }
        return sliceView({0, column}, {extent(0), column + 1});
    }

    [[nodiscard]] TensorView transposeView() const
        requires(Rank == tensor_detail::kDynamicTensorRank || Rank == 2)
    {
        checkAlive();
        if constexpr (Rank == tensor_detail::kDynamicTensorRank)
        {
            return TensorView(mStorageBase, tensor_detail::transposeLayout(mLayout), mLifetime, mTracked);
        }
        else
        {
            return TensorView(mStorageBase, tensor_detail::transposeLayout(mLayout), mLifetime, mTracked);
        }
    }

    [[nodiscard]] TensorView<T> reshapeView(DynamicExtents target) const
    {
        checkAlive();
        if constexpr (Rank == tensor_detail::kDynamicTensorRank)
        {
            return TensorView<T>(mStorageBase, tensor_detail::reshapeLayout(mLayout, std::move(target)),
                                 mLifetime, mTracked);
        }
        else
        {
            return TensorView<T>(mStorageBase,
                                 tensor_detail::reshapeLayout(tensor_detail::makeDynamicLayout(mLayout),
                                                              std::move(target)),
                                 mLifetime, mTracked);
        }
    }

    template <std::size_t NewRank>
    [[nodiscard]] TensorView<T, NewRank>
    reshapeView(tensor_detail::FixedRankExtents<NewRank> target) const
    {
        checkAlive();
        return TensorView<T, NewRank>(mStorageBase,
                                      tensor_detail::reshapeLayout(mLayout, std::move(target)),
                                      mLifetime, mTracked);
    }

    [[nodiscard]] TensorView<const value_type> broadcastView(DynamicExtents target) const
    {
        checkAlive();
        if constexpr (Rank == tensor_detail::kDynamicTensorRank)
        {
            return TensorView<const value_type>(mStorageBase,
                                                tensor_detail::broadcastLayout(mLayout, std::move(target)),
                                                mLifetime, mTracked);
        }
        else
        {
            return TensorView<const value_type>(
                mStorageBase,
                tensor_detail::broadcastLayout(tensor_detail::makeDynamicLayout(mLayout), std::move(target)),
                mLifetime, mTracked);
        }
    }

    template <std::size_t NewRank>
    [[nodiscard]] TensorView<const value_type, NewRank>
    broadcastView(tensor_detail::FixedRankExtents<NewRank> target) const
        requires(Rank == tensor_detail::kDynamicTensorRank || NewRank >= Rank)
    {
        checkAlive();
        return TensorView<const value_type, NewRank>(
            mStorageBase, tensor_detail::broadcastLayout(mLayout, std::move(target)),
            mLifetime, mTracked);
    }

    [[nodiscard]] TensorView<const value_type, Rank> asConstView() const
        requires(!std::is_const_v<T>)
    {
        return TensorView<const value_type, Rank>(*this);
    }

private:
    template <typename, std::size_t>
    friend class TensorView;
    template <typename, std::size_t>
    friend class SharedTensorView;
    template <typename, typename, std::size_t>
    friend class Tensor;
    friend struct tensor_detail::TensorAccess;

    TensorView(pointer storageBase, layout_type layout,
               std::weak_ptr<tensor_detail::TensorLifetimeState> lifetime, bool tracked)
        : mStorageBase(storageBase)
        , mLayout(std::move(layout))
        , mLifetime(std::move(lifetime))
        , mTracked(tracked)
    {
        if (mLayout.logicalSize() != 0 && mStorageBase == nullptr)
        {
            throw std::invalid_argument("Cannot borrow a nonempty Tensor layout from a null pointer");
        }
        enforceWritableInjectivity(mLayout);
    }

    void swapState(TensorView& other) noexcept
    {
        static_assert(std::is_nothrow_swappable_v<layout_type>);
        using std::swap;
        swap(mStorageBase, other.mStorageBase);
        swap(mLayout, other.mLayout);
        mLifetime.swap(other.mLifetime);
        swap(mTracked, other.mTracked);
    }

    static void enforceWritableInjectivity(const layout_type& layout)
    {
        if constexpr (!std::is_const_v<T>)
        {
            if (!layout.isInjective())
            {
                throw std::invalid_argument("Mutable TensorView requires a proven-injective layout");
            }
        }
    }

    void checkAlive() const
    {
        tensor_detail::checkLifetime(mLifetime, mTracked);
    }

    template <std::size_t IndexRank>
    [[nodiscard]] reference atIndices(const std::array<std::ptrdiff_t, IndexRank>& indices) const
    {
        checkAlive();
        if (IndexRank != rank())
        {
            throw std::invalid_argument("TensorView index count does not match its rank");
        }
        std::ptrdiff_t offset = mLayout.originOffset();
        if constexpr (IndexRank > 0)
        {
            for (std::size_t axis = 0; axis < IndexRank; ++axis)
            {
                if (indices[axis] < 0 ||
                    static_cast<std::size_t>(indices[axis]) >= mLayout.extents()[axis])
                {
                    throw std::out_of_range("TensorView multidimensional index is out of range");
                }
                offset = tensor_detail::checkedOffsetAdd(
                    offset, tensor_detail::checkedStrideContribution(
                                static_cast<std::size_t>(indices[axis]), mLayout.strides()[axis]));
            }
        }
        return mStorageBase[offset];
    }

    [[nodiscard]] static layout_type moveOrCopyLayout(TensorView& other) noexcept
    {
        if constexpr (Rank == tensor_detail::kDynamicTensorRank)
        {
            return std::move(other.mLayout);
        }
        else
        {
            return other.mLayout;
        }
    }

    [[nodiscard]] static std::weak_ptr<tensor_detail::TensorLifetimeState>
    moveOrCopyLifetime(TensorView& other) noexcept
    {
        if constexpr (Rank == tensor_detail::kDynamicTensorRank)
        {
            return std::move(other.mLifetime);
        }
        else
        {
            return other.mLifetime;
        }
    }

    [[nodiscard]] static layout_type defaultLayout()
    {
        if constexpr (Rank == tensor_detail::kDynamicTensorRank)
        {
            return layout_type::contiguous(DynamicExtents{0});
        }
        else
        {
            return layout_type::contiguous(extents_type{});
        }
    }

    pointer mStorageBase = nullptr;
    layout_type mLayout = defaultLayout();
    std::weak_ptr<tensor_detail::TensorLifetimeState> mLifetime;
    bool mTracked = false;
};

template <typename T, std::size_t Rank>
class SharedTensorView
{
public:
    using element_type = T;
    using value_type = std::remove_const_t<T>;
    using reference = T&;
    using pointer = T*;
    using extents_type = tensor_detail::TensorExtentsFor<Rank>;
    using layout_type = tensor_detail::TensorLayoutFor<Rank>;
    using strides_type = tensor_detail::TensorStridesFor<Rank>;
    using iterator = typename TensorView<T, Rank>::iterator;
    static constexpr std::size_t static_rank = Rank;

    SharedTensorView()
        requires(Rank == tensor_detail::kDynamicTensorRank)
    = default;
    SharedTensorView(const SharedTensorView&) = default;
    SharedTensorView(SharedTensorView&& other) noexcept
        : mLifetime(moveOrCopySharedLifetime(other))
        , mView(moveOrCopyView(other))
    {
    }
    SharedTensorView& operator=(const SharedTensorView& other)
    {
        if (this != &other)
        {
            SharedTensorView replacement(other);
            swapState(replacement);
        }
        return *this;
    }
    SharedTensorView& operator=(SharedTensorView&& other) noexcept
    {
        if (this != &other)
        {
            SharedTensorView replacement(std::move(other));
            swapState(replacement);
        }
        return *this;
    }

    template <typename U>
        requires(std::is_const_v<T> && std::same_as<std::remove_const_t<T>, std::remove_const_t<U>> &&
                 !std::is_const_v<U>)
    SharedTensorView(const SharedTensorView<U, Rank>& other)
        : mLifetime(other.mLifetime)
        , mView(other.mView)
    {
    }

    [[nodiscard]] static SharedTensorView share(std::shared_ptr<void> lifetime, pointer storageBase,
                                                layout_type layout)
    {
        if (!lifetime)
        {
            throw std::invalid_argument("SharedTensorView requires a nonempty lifetime handle");
        }
        if (layout.logicalSize() != 0 && storageBase == nullptr)
        {
            throw std::invalid_argument("Cannot share a nonempty Tensor layout from a null pointer");
        }
        return SharedTensorView(std::move(lifetime), TensorView<T, Rank>::borrow(storageBase, std::move(layout)));
    }

    [[nodiscard]] const layout_type& layout() const noexcept { return mView.layout(); }
    [[nodiscard]] const extents_type& extents() const noexcept { return mView.extents(); }
    [[nodiscard]] const strides_type& strides() const noexcept { return mView.strides(); }
    [[nodiscard]] std::size_t size() const noexcept { return mView.size(); }
    [[nodiscard]] std::size_t rank() const noexcept { return mView.rank(); }
    [[nodiscard]] std::size_t extent(std::size_t axis) const { return mView.extent(axis); }
    [[nodiscard]] bool empty() const noexcept { return mView.empty(); }
    [[nodiscard]] reference operator[](std::size_t index) const { return mView[index]; }
    [[nodiscard]] reference atLinear(std::size_t index) const { return mView.atLinear(index); }

    template <std::integral... Indices>
        requires(Rank == tensor_detail::kDynamicTensorRank || sizeof...(Indices) == Rank)
    [[nodiscard]] reference operator()(Indices... indices) const
    {
        return mView(indices...);
    }

    [[nodiscard]] iterator begin() const { return mView.begin(); }
    [[nodiscard]] iterator end() const { return mView.end(); }
    [[nodiscard]] pointer data() const { return mView.data(); }

    [[nodiscard]] SharedTensorView sliceView(const std::vector<std::size_t>& start,
                                             const std::vector<std::size_t>& end) const
    {
        return SharedTensorView(mLifetime, mView.sliceView(start, end));
    }

    [[nodiscard]] auto sliceView(const std::vector<SliceSpec>& specifications) const
    {
        auto view = mView.sliceView(specifications);
        return SharedTensorView<T, decltype(view)::static_rank>(mLifetime, std::move(view));
    }

    template <TensorAxis... Axes>
        requires(Rank != tensor_detail::kDynamicTensorRank && sizeof...(Axes) > 0)
    [[nodiscard]] auto squeezeView() const
    {
        auto view = mView.template squeezeView<Axes...>();
        return SharedTensorView<T, decltype(view)::static_rank>(mLifetime, std::move(view));
    }

    [[nodiscard]] auto sliceView(std::initializer_list<SliceSpec> specifications) const
    {
        auto view = mView.sliceView(specifications);
        return SharedTensorView<T, decltype(view)::static_rank>(mLifetime, std::move(view));
    }

    template <typename... Specifications>
        requires(Rank != tensor_detail::kDynamicTensorRank && sizeof...(Specifications) > 0 &&
                 tensor_detail::typedSliceConsumedAxes<Specifications...> <= Rank &&
                 tensor_detail::typedSliceEllipsisCount<Specifications...> <= 1 &&
                 (tensor_detail::isTypedSliceSpecification<Specifications> && ...))
    [[nodiscard]] auto sliceView(Specifications&&... specifications) const
    {
        auto view = mView.sliceView(std::forward<Specifications>(specifications)...);
        return SharedTensorView<T, decltype(view)::static_rank>(mLifetime, std::move(view));
    }

    [[nodiscard]] SharedTensorView permuteView(const std::vector<TensorAxis>& order) const
    {
        return SharedTensorView(mLifetime, mView.permuteView(order));
    }

    [[nodiscard]] auto squeezeView(const std::vector<TensorAxis>& axes = {}) const
    {
        auto view = mView.squeezeView(axes);
        return SharedTensorView<T, decltype(view)::static_rank>(mLifetime, std::move(view));
    }

    [[nodiscard]] auto unsqueezeView(TensorAxis axis) const
    {
        auto view = mView.unsqueezeView(axis);
        return SharedTensorView<T, decltype(view)::static_rank>(mLifetime, std::move(view));
    }

    [[nodiscard]] SharedTensorView rowView(std::size_t row) const
    {
        return SharedTensorView(mLifetime, mView.rowView(row));
    }

    [[nodiscard]] SharedTensorView columnView(std::size_t column) const
    {
        return SharedTensorView(mLifetime, mView.columnView(column));
    }

    [[nodiscard]] SharedTensorView transposeView() const
        requires(Rank == tensor_detail::kDynamicTensorRank || Rank == 2)
    {
        return SharedTensorView(mLifetime, mView.transposeView());
    }

    [[nodiscard]] auto reshapeView(DynamicExtents target) const
    {
        auto view = mView.reshapeView(std::move(target));
        return SharedTensorView<T, decltype(view)::static_rank>(mLifetime, std::move(view));
    }

    template <std::size_t NewRank>
    [[nodiscard]] auto reshapeView(tensor_detail::FixedRankExtents<NewRank> target) const
    {
        auto view = mView.reshapeView(std::move(target));
        return SharedTensorView<T, NewRank>(mLifetime, std::move(view));
    }

    [[nodiscard]] auto broadcastView(DynamicExtents target) const
    {
        auto view = mView.broadcastView(std::move(target));
        return SharedTensorView<const value_type, decltype(view)::static_rank>(mLifetime, std::move(view));
    }

    template <std::size_t NewRank>
    [[nodiscard]] auto broadcastView(tensor_detail::FixedRankExtents<NewRank> target) const
        requires(Rank == tensor_detail::kDynamicTensorRank || NewRank >= Rank)
    {
        auto view = mView.broadcastView(std::move(target));
        return SharedTensorView<const value_type, NewRank>(mLifetime, std::move(view));
    }

    [[nodiscard]] SharedTensorView<const value_type, Rank> asConstView() const
        requires(!std::is_const_v<T>)
    {
        return SharedTensorView<const value_type, Rank>(*this);
    }

private:
    template <typename, std::size_t>
    friend class SharedTensorView;
    template <typename, typename, std::size_t>
    friend class Tensor;
    friend struct tensor_detail::TensorAccess;

    SharedTensorView(std::shared_ptr<void> lifetime, TensorView<T, Rank> view)
        : mLifetime(std::move(lifetime))
        , mView(std::move(view))
    {
    }

    void swapState(SharedTensorView& other) noexcept
    {
        mLifetime.swap(other.mLifetime);
        mView.swapState(other.mView);
    }

    [[nodiscard]] static std::shared_ptr<void> moveOrCopySharedLifetime(SharedTensorView& other) noexcept
    {
        if constexpr (Rank == tensor_detail::kDynamicTensorRank)
        {
            return std::move(other.mLifetime);
        }
        else
        {
            return other.mLifetime;
        }
    }

    [[nodiscard]] static TensorView<T, Rank> moveOrCopyView(SharedTensorView& other) noexcept
    {
        if constexpr (Rank == tensor_detail::kDynamicTensorRank)
        {
            return std::move(other.mView);
        }
        else
        {
            return other.mView;
        }
    }

    std::shared_ptr<void> mLifetime;
    TensorView<T, Rank> mView;
};

namespace tensor_detail
{

template <typename T>
struct IsRegisteredTensorFamily : std::false_type
{
};

template <typename T, typename Allocator, std::size_t Rank>
struct IsRegisteredTensorFamily<Tensor<T, Allocator, Rank>> : std::true_type
{
};

template <typename T, std::size_t Rank>
struct IsRegisteredTensorFamily<TensorView<T, Rank>> : std::true_type
{
};

template <typename T, std::size_t Rank>
struct IsRegisteredTensorFamily<SharedTensorView<T, Rank>> : std::true_type
{
};

template <typename T>
inline constexpr bool isRegisteredTensorFamily =
    IsRegisteredTensorFamily<std::remove_cvref_t<T>>::value;

} // namespace tensor_detail

template <typename R>
concept ReadableTensor = tensor_detail::isRegisteredTensorFamily<R> && requires(const R& readable, std::size_t index) {
    typename R::value_type;
    typename R::element_type;
    typename R::extents_type;
    typename R::layout_type;
    { readable.extents() } -> std::same_as<const typename R::extents_type&>;
    { readable.layout() } -> std::same_as<const typename R::layout_type&>;
    { readable.rank() } -> std::convertible_to<std::size_t>;
    { readable.size() } -> std::convertible_to<std::size_t>;
    readable[index];
};

template <typename W>
concept WritableTensor = ReadableTensor<W> && !std::is_const_v<typename W::element_type> &&
    requires(W& writable, std::size_t index, typename W::value_type value) {
        writable[index] = value;
    };

namespace tensor_detail
{

template <typename T, typename = void>
struct TensorStaticRankValue : std::integral_constant<std::size_t, kDynamicTensorRank>
{
};

template <typename T>
struct TensorStaticRankValue<T, std::void_t<decltype(std::remove_cvref_t<T>::static_rank)>>
    : std::integral_constant<std::size_t, std::remove_cvref_t<T>::static_rank>
{
};

template <typename T>
inline constexpr std::size_t tensorStaticRankValue = TensorStaticRankValue<T>::value;

} // namespace tensor_detail

namespace tensor_detail
{

struct TensorAccess
{
    template <typename R>
    static void validate(const R&) = delete;

    template <typename T, typename Allocator, std::size_t Rank>
    static void validate(const Tensor<T, Allocator, Rank>&) noexcept
    {
    }

    template <typename T, std::size_t Rank>
    static void validate(const TensorView<T, Rank>& view)
    {
        view.checkAlive();
    }

    template <typename T, std::size_t Rank>
    static void validate(const SharedTensorView<T, Rank>& view)
    {
        view.mView.checkAlive();
    }

    template <typename T, typename Allocator, std::size_t Rank>
    [[nodiscard]] static T* storageBase(Tensor<T, Allocator, Rank>& owner) noexcept
    {
        return owner.mStorage.get();
    }

    template <typename T, typename Allocator, std::size_t Rank>
    [[nodiscard]] static const T* storageBase(const Tensor<T, Allocator, Rank>& owner) noexcept
    {
        return owner.mStorage.get();
    }

    template <typename T, std::size_t Rank>
    [[nodiscard]] static T* storageBase(TensorView<T, Rank>& view) noexcept
    {
        return view.mStorageBase;
    }

    template <typename T, std::size_t Rank>
    [[nodiscard]] static T* storageBase(const TensorView<T, Rank>& view) noexcept
    {
        return view.mStorageBase;
    }

    template <typename T, std::size_t Rank>
    [[nodiscard]] static T* storageBase(SharedTensorView<T, Rank>& view) noexcept
    {
        return view.mView.mStorageBase;
    }

    template <typename T, std::size_t Rank>
    [[nodiscard]] static T* storageBase(const SharedTensorView<T, Rank>& view) noexcept
    {
        return view.mView.mStorageBase;
    }

    template <typename T, typename Allocator, std::size_t Rank>
    [[nodiscard]] static std::weak_ptr<TensorLifetimeState> lifetime(const Tensor<T, Allocator, Rank>& owner) noexcept
    {
        return owner.mLifetime;
    }

    template <typename T, std::size_t Rank>
    [[nodiscard]] static std::weak_ptr<TensorLifetimeState> lifetime(const TensorView<T, Rank>& view) noexcept
    {
        return view.mLifetime;
    }

    template <typename T, std::size_t Rank>
    [[nodiscard]] static std::weak_ptr<TensorLifetimeState> lifetime(const SharedTensorView<T, Rank>& view) noexcept
    {
        return view.mView.mLifetime;
    }

    template <typename T, typename Allocator, std::size_t Rank>
    [[nodiscard]] static bool tracked(const Tensor<T, Allocator, Rank>&) noexcept
    {
        return true;
    }

    template <typename T, std::size_t Rank>
    [[nodiscard]] static bool tracked(const TensorView<T, Rank>& view) noexcept
    {
        return view.mTracked;
    }

    template <typename T, std::size_t Rank>
    [[nodiscard]] static bool tracked(const SharedTensorView<T, Rank>& view) noexcept
    {
        return view.mView.mTracked;
    }

    template <typename T, typename Allocator, std::size_t Rank>
    [[nodiscard]] static std::shared_ptr<void> sharedLifetime(const Tensor<T, Allocator, Rank>&) noexcept
    {
        return {};
    }

    template <typename T, std::size_t Rank>
    [[nodiscard]] static std::shared_ptr<void> sharedLifetime(const TensorView<T, Rank>&) noexcept
    {
        return {};
    }

    template <typename T, std::size_t Rank>
    [[nodiscard]] static std::shared_ptr<void> sharedLifetime(const SharedTensorView<T, Rank>& view) noexcept
    {
        return view.mLifetime;
    }

    template <typename T, std::size_t Rank>
    [[nodiscard]] static TensorView<T, Rank>
    makeView(T* storageBase, tensor_detail::TensorLayoutFor<Rank> layout,
             std::weak_ptr<TensorLifetimeState> lifetimeState, bool isTracked)
    {
        return TensorView<T, Rank>(storageBase, std::move(layout), std::move(lifetimeState), isTracked);
    }

    template <typename T, std::size_t Rank>
    [[nodiscard]] static SharedTensorView<T, Rank>
    makeSharedView(std::shared_ptr<void> sharedLifetimeHandle, T* storageBase,
                   tensor_detail::TensorLayoutFor<Rank> layout)
    {
        return SharedTensorView<T, Rank>(
            std::move(sharedLifetimeHandle),
            TensorView<T, Rank>(storageBase, std::move(layout), {}, false));
    }
};

} // namespace tensor_detail

} // namespace fat_p
