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

template <typename T, typename Allocator>
class Tensor;

template <typename T>
class TensorView;

template <typename T>
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

inline TensorLayout sliceLayout(const TensorLayout& source, const std::vector<std::size_t>& start,
                                const std::vector<std::size_t>& end)
{
    if (start.size() != source.rank() || end.size() != source.rank())
    {
        throw std::invalid_argument("Tensor slice bounds must match the source rank");
    }

    std::vector<std::size_t> resultExtents(source.rank());
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
    return TensorLayout(source.storageLength(), origin, DynamicExtents(std::move(resultExtents)), source.strides());
}

inline TensorLayout transposeLayout(const TensorLayout& source)
{
    if (source.rank() != 2)
    {
        throw std::invalid_argument("transposeView requires a rank-two Tensor mapping");
    }
    return TensorLayout(source.storageLength(), source.originOffset(),
                        DynamicExtents{source.extents()[1], source.extents()[0]},
                        TensorStrides{source.strides()[1], source.strides()[0]});
}

inline TensorLayout reshapeLayout(const TensorLayout& source, DynamicExtents target)
{
    if (!source.isContiguous())
    {
        throw std::invalid_argument("reshapeView requires a contiguous Tensor mapping");
    }
    if (source.logicalSize() != target.logicalSize())
    {
        throw std::invalid_argument("reshapeView cannot change the logical element count");
    }
    return TensorLayout(source.storageLength(), source.originOffset(), target, TensorLayout::canonicalStrides(target));
}

inline TensorLayout broadcastLayout(const TensorLayout& source, DynamicExtents target)
{
    if (source.rank() > target.rank())
    {
        throw std::invalid_argument("broadcastView target rank is smaller than the source rank");
    }

    TensorStrides resultStrides(target.rank(), 0);
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
    return TensorLayout(source.storageLength(), source.originOffset(), std::move(target), std::move(resultStrides));
}

} // namespace tensor_detail

template <typename Element>
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

    TensorLogicalIterator(pointer storageBase, TensorLayout layout, std::size_t linearIndex,
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
        if (mLinearIndex >= mLayout.logicalSize())
        {
            throw std::out_of_range("Cannot dereference the end Tensor iterator");
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
    TensorLayout mLayout = TensorLayout::contiguous(DynamicExtents{0});
    std::size_t mLinearIndex = 0;
    std::weak_ptr<tensor_detail::TensorLifetimeState> mLifetime;
    bool mTracked = false;
};

template <typename T>
class TensorView
{
public:
    using element_type = T;
    using value_type = std::remove_const_t<T>;
    using reference = T&;
    using pointer = T*;
    using iterator = TensorLogicalIterator<T>;
    using const_iterator = TensorLogicalIterator<const value_type>;

    TensorView() = default;
    TensorView(const TensorView&) = default;
    TensorView(TensorView&&) noexcept = default;
    TensorView& operator=(const TensorView&) = default;
    TensorView& operator=(TensorView&&) noexcept = default;

    template <typename U>
        requires(std::is_const_v<T> && std::same_as<std::remove_const_t<T>, std::remove_const_t<U>> &&
                 !std::is_const_v<U>)
    TensorView(const TensorView<U>& other)
        : mStorageBase(other.mStorageBase)
        , mLayout(other.mLayout)
        , mLifetime(other.mLifetime)
        , mTracked(other.mTracked)
    {
    }

    [[nodiscard]] static TensorView borrow(pointer storageBase, TensorLayout layout)
    {
        if (layout.logicalSize() != 0 && storageBase == nullptr)
        {
            throw std::invalid_argument("Cannot borrow a nonempty Tensor layout from a null pointer");
        }
        enforceWritableInjectivity(layout);
        return TensorView(storageBase, std::move(layout), {}, false);
    }

    [[nodiscard]] const TensorLayout& layout() const noexcept
    {
        return mLayout;
    }

    [[nodiscard]] const DynamicExtents& extents() const noexcept
    {
        return mLayout.extents();
    }

    [[nodiscard]] const TensorStrides& strides() const noexcept
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
        return TensorView(mStorageBase, tensor_detail::sliceLayout(mLayout, start, end), mLifetime, mTracked);
    }

    [[nodiscard]] TensorView sliceView(const std::vector<SliceSpec>& specifications) const
    {
        checkAlive();
        return TensorView(mStorageBase, tensor_detail::extendedSliceLayout(mLayout, specifications), mLifetime,
                          mTracked);
    }

    [[nodiscard]] TensorView sliceView(std::initializer_list<SliceSpec> specifications) const
    {
        return sliceView(std::vector<SliceSpec>(specifications));
    }

    [[nodiscard]] TensorView permuteView(const std::vector<TensorAxis>& order) const
    {
        checkAlive();
        return TensorView(mStorageBase, tensor_detail::permuteLayout(mLayout, order), mLifetime, mTracked);
    }

    [[nodiscard]] TensorView squeezeView(const std::vector<TensorAxis>& axes = {}) const
    {
        checkAlive();
        return TensorView(mStorageBase, tensor_detail::squeezeLayout(mLayout, axes), mLifetime, mTracked);
    }

    [[nodiscard]] TensorView unsqueezeView(TensorAxis axis) const
    {
        checkAlive();
        return TensorView(mStorageBase, tensor_detail::unsqueezeLayout(mLayout, axis), mLifetime, mTracked);
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
    {
        checkAlive();
        return TensorView(mStorageBase, tensor_detail::transposeLayout(mLayout), mLifetime, mTracked);
    }

    [[nodiscard]] TensorView reshapeView(DynamicExtents target) const
    {
        checkAlive();
        return TensorView(mStorageBase, tensor_detail::reshapeLayout(mLayout, std::move(target)), mLifetime, mTracked);
    }

    [[nodiscard]] TensorView<const value_type> broadcastView(DynamicExtents target) const
    {
        checkAlive();
        return TensorView<const value_type>(mStorageBase,
                                            tensor_detail::broadcastLayout(mLayout, std::move(target)), mLifetime,
                                            mTracked);
    }

    [[nodiscard]] TensorView<const value_type> asConstView() const
        requires(!std::is_const_v<T>)
    {
        return TensorView<const value_type>(*this);
    }

private:
    template <typename>
    friend class TensorView;
    template <typename>
    friend class SharedTensorView;
    template <typename, typename>
    friend class Tensor;
    friend struct tensor_detail::TensorAccess;

    TensorView(pointer storageBase, TensorLayout layout,
               std::weak_ptr<tensor_detail::TensorLifetimeState> lifetime, bool tracked)
        : mStorageBase(storageBase)
        , mLayout(std::move(layout))
        , mLifetime(std::move(lifetime))
        , mTracked(tracked)
    {
        enforceWritableInjectivity(mLayout);
    }

    static void enforceWritableInjectivity(const TensorLayout& layout)
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

    template <std::size_t Rank>
    [[nodiscard]] reference atIndices(const std::array<std::ptrdiff_t, Rank>& indices) const
    {
        checkAlive();
        if (Rank != rank())
        {
            throw std::invalid_argument("TensorView index count does not match its rank");
        }
        std::ptrdiff_t offset = mLayout.originOffset();
        for (std::size_t axis = 0; axis < Rank; ++axis)
        {
            if (indices[axis] < 0 || static_cast<std::size_t>(indices[axis]) >= mLayout.extents()[axis])
            {
                throw std::out_of_range("TensorView multidimensional index is out of range");
            }
            offset = tensor_detail::checkedOffsetAdd(
                offset, tensor_detail::checkedStrideContribution(static_cast<std::size_t>(indices[axis]),
                                                                  mLayout.strides()[axis]));
        }
        return mStorageBase[offset];
    }

    pointer mStorageBase = nullptr;
    TensorLayout mLayout = TensorLayout::contiguous(DynamicExtents{0});
    std::weak_ptr<tensor_detail::TensorLifetimeState> mLifetime;
    bool mTracked = false;
};

template <typename T>
class SharedTensorView
{
public:
    using element_type = T;
    using value_type = std::remove_const_t<T>;
    using reference = T&;
    using pointer = T*;
    using iterator = typename TensorView<T>::iterator;

    SharedTensorView() = default;
    SharedTensorView(const SharedTensorView&) = default;
    SharedTensorView(SharedTensorView&&) noexcept = default;
    SharedTensorView& operator=(const SharedTensorView&) = default;
    SharedTensorView& operator=(SharedTensorView&&) noexcept = default;

    template <typename U>
        requires(std::is_const_v<T> && std::same_as<std::remove_const_t<T>, std::remove_const_t<U>> &&
                 !std::is_const_v<U>)
    SharedTensorView(const SharedTensorView<U>& other)
        : mLifetime(other.mLifetime)
        , mView(other.mView)
    {
    }

    [[nodiscard]] static SharedTensorView share(std::shared_ptr<void> lifetime, pointer storageBase,
                                                TensorLayout layout)
    {
        if (!lifetime)
        {
            throw std::invalid_argument("SharedTensorView requires a nonempty lifetime handle");
        }
        if (layout.logicalSize() != 0 && storageBase == nullptr)
        {
            throw std::invalid_argument("Cannot share a nonempty Tensor layout from a null pointer");
        }
        return SharedTensorView(std::move(lifetime), TensorView<T>::borrow(storageBase, std::move(layout)));
    }

    [[nodiscard]] const TensorLayout& layout() const noexcept { return mView.layout(); }
    [[nodiscard]] const DynamicExtents& extents() const noexcept { return mView.extents(); }
    [[nodiscard]] const TensorStrides& strides() const noexcept { return mView.strides(); }
    [[nodiscard]] std::size_t size() const noexcept { return mView.size(); }
    [[nodiscard]] std::size_t rank() const noexcept { return mView.rank(); }
    [[nodiscard]] std::size_t extent(std::size_t axis) const { return mView.extent(axis); }
    [[nodiscard]] bool empty() const noexcept { return mView.empty(); }
    [[nodiscard]] reference operator[](std::size_t index) const { return mView[index]; }
    [[nodiscard]] reference atLinear(std::size_t index) const { return mView.atLinear(index); }

    template <std::integral... Indices>
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

    [[nodiscard]] SharedTensorView sliceView(const std::vector<SliceSpec>& specifications) const
    {
        return SharedTensorView(mLifetime, mView.sliceView(specifications));
    }

    [[nodiscard]] SharedTensorView sliceView(std::initializer_list<SliceSpec> specifications) const
    {
        return SharedTensorView(mLifetime, mView.sliceView(specifications));
    }

    [[nodiscard]] SharedTensorView permuteView(const std::vector<TensorAxis>& order) const
    {
        return SharedTensorView(mLifetime, mView.permuteView(order));
    }

    [[nodiscard]] SharedTensorView squeezeView(const std::vector<TensorAxis>& axes = {}) const
    {
        return SharedTensorView(mLifetime, mView.squeezeView(axes));
    }

    [[nodiscard]] SharedTensorView unsqueezeView(TensorAxis axis) const
    {
        return SharedTensorView(mLifetime, mView.unsqueezeView(axis));
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
    {
        return SharedTensorView(mLifetime, mView.transposeView());
    }

    [[nodiscard]] SharedTensorView reshapeView(DynamicExtents target) const
    {
        return SharedTensorView(mLifetime, mView.reshapeView(std::move(target)));
    }

    [[nodiscard]] SharedTensorView<const value_type> broadcastView(DynamicExtents target) const
    {
        return SharedTensorView<const value_type>(mLifetime, mView.broadcastView(std::move(target)));
    }

    [[nodiscard]] SharedTensorView<const value_type> asConstView() const
        requires(!std::is_const_v<T>)
    {
        return SharedTensorView<const value_type>(*this);
    }

private:
    template <typename>
    friend class SharedTensorView;
    template <typename, typename>
    friend class Tensor;
    friend struct tensor_detail::TensorAccess;

    SharedTensorView(std::shared_ptr<void> lifetime, TensorView<T> view)
        : mLifetime(std::move(lifetime))
        , mView(std::move(view))
    {
    }

    std::shared_ptr<void> mLifetime;
    TensorView<T> mView;
};

template <typename R>
concept ReadableTensor = requires(const R& readable, std::size_t index) {
    typename R::value_type;
    { readable.extents() } -> std::same_as<const DynamicExtents&>;
    { readable.layout() } -> std::same_as<const TensorLayout&>;
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

struct TensorAccess
{
    template <typename R>
    static void validate(const R&) noexcept
    {
    }

    template <typename T>
    static void validate(const TensorView<T>& view)
    {
        view.checkAlive();
    }

    template <typename T>
    static void validate(const SharedTensorView<T>& view)
    {
        view.mView.checkAlive();
    }

    template <typename T, typename Allocator>
    [[nodiscard]] static T* storageBase(Tensor<T, Allocator>& owner) noexcept
    {
        return owner.mStorage.get();
    }

    template <typename T, typename Allocator>
    [[nodiscard]] static const T* storageBase(const Tensor<T, Allocator>& owner) noexcept
    {
        return owner.mStorage.get();
    }

    template <typename T>
    [[nodiscard]] static T* storageBase(TensorView<T>& view) noexcept
    {
        return view.mStorageBase;
    }

    template <typename T>
    [[nodiscard]] static const T* storageBase(const TensorView<T>& view) noexcept
    {
        return view.mStorageBase;
    }

    template <typename T>
    [[nodiscard]] static T* storageBase(SharedTensorView<T>& view) noexcept
    {
        return view.mView.mStorageBase;
    }

    template <typename T>
    [[nodiscard]] static const T* storageBase(const SharedTensorView<T>& view) noexcept
    {
        return view.mView.mStorageBase;
    }

    template <typename R>
    [[nodiscard]] static std::weak_ptr<TensorLifetimeState> lifetime(const R&) noexcept
    {
        return {};
    }

    template <typename T, typename Allocator>
    [[nodiscard]] static std::weak_ptr<TensorLifetimeState> lifetime(const Tensor<T, Allocator>& owner) noexcept
    {
        return owner.mLifetime;
    }

    template <typename T>
    [[nodiscard]] static std::weak_ptr<TensorLifetimeState> lifetime(const TensorView<T>& view) noexcept
    {
        return view.mLifetime;
    }

    template <typename T>
    [[nodiscard]] static std::weak_ptr<TensorLifetimeState> lifetime(const SharedTensorView<T>& view) noexcept
    {
        return view.mView.mLifetime;
    }

    template <typename R>
    [[nodiscard]] static bool tracked(const R&) noexcept
    {
        return false;
    }

    template <typename T, typename Allocator>
    [[nodiscard]] static bool tracked(const Tensor<T, Allocator>&) noexcept
    {
        return true;
    }

    template <typename T>
    [[nodiscard]] static bool tracked(const TensorView<T>& view) noexcept
    {
        return view.mTracked;
    }

    template <typename T>
    [[nodiscard]] static bool tracked(const SharedTensorView<T>& view) noexcept
    {
        return view.mView.mTracked;
    }

    template <typename R>
    [[nodiscard]] static std::shared_ptr<void> sharedLifetime(const R&) noexcept
    {
        return {};
    }

    template <typename T>
    [[nodiscard]] static std::shared_ptr<void> sharedLifetime(const SharedTensorView<T>& view) noexcept
    {
        return view.mLifetime;
    }

    template <typename T>
    [[nodiscard]] static TensorView<T> makeView(T* storageBase, TensorLayout layout,
                                                std::weak_ptr<TensorLifetimeState> lifetimeState,
                                                bool isTracked)
    {
        return TensorView<T>(storageBase, std::move(layout), std::move(lifetimeState), isTracked);
    }
};

} // namespace tensor_detail

} // namespace fat_p
