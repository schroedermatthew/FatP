#pragma once

/*
FATP_META:
  meta_version: 1
  component: Tensor
  file_role: internal_header
  path: include/fat_p/tensor/Tensor.h
  namespace: fat_p
  layer: Domain
  summary: "Allocator-aware owning dynamic Tensor with canonical contiguous storage."
  api_stability: in_work
  related:
    docs:
      - components/Tensor/docs/Design Note - Tensor Semantic Contract.md
      - components/Tensor/docs/Design Note - Tensor Architecture Additions Plan.md
      - components/Tensor/docs/User Manual - Tensor.md
    headers:
      - include/fat_p/Tensor.h
      - include/fat_p/TensorView.h
      - include/fat_p/TensorLayout.h
    tests:
      - components/Tensor/tests/test_Tensor.cpp
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
 * @file Tensor.h
 * @brief Canonical-contiguous owning dynamic Tensor.
 */

#include "AlignedVector.h"
#include "TensorKernels.h"
#include "TensorView.h"

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace fat_p
{

template <typename T>
using TensorAllocator = AlignedAllocator<T, 64>;

template <typename T, typename Allocator = TensorAllocator<T>,
          std::size_t Rank = tensor_detail::kDynamicTensorRank>
class Tensor
{
public:
    using element_type = T;
    using value_type = T;
    using allocator_type = Allocator;
    using allocator_traits = std::allocator_traits<allocator_type>;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = value_type*;
    using const_pointer = const value_type*;
    using iterator = pointer;
    using const_iterator = const_pointer;
    using extents_type = tensor_detail::TensorExtentsFor<Rank>;
    using layout_type = tensor_detail::TensorLayoutFor<Rank>;
    using strides_type = tensor_detail::TensorStridesFor<Rank>;
    static constexpr std::size_t static_rank = Rank;

    static_assert(std::same_as<typename allocator_traits::value_type, value_type>,
                  "Tensor allocator value_type must match Tensor value_type");
    static_assert(std::same_as<typename allocator_traits::pointer, pointer>,
                  "Tensor currently requires allocator_traits<Allocator>::pointer to equal T*");

    Tensor()
        requires(std::default_initializable<allocator_type> && std::default_initializable<value_type>)
        : Tensor(std::allocator_arg, allocator_type{}, defaultExtents())
    {
    }

    explicit Tensor(const allocator_type& allocator)
        requires std::default_initializable<value_type>
        : Tensor(std::allocator_arg, allocator, defaultExtents())
    {
    }

    explicit Tensor(extents_type extents)
        requires(std::default_initializable<allocator_type> && std::default_initializable<value_type>)
        : Tensor(std::allocator_arg, allocator_type{}, std::move(extents))
    {
    }

    Tensor(std::initializer_list<size_type> extents)
        requires(std::default_initializable<allocator_type> && std::default_initializable<value_type>)
        : Tensor(extents_type(extents))
    {
    }

    explicit Tensor(const std::vector<size_type>& extents)
        requires(std::default_initializable<allocator_type> && std::default_initializable<value_type>)
        : Tensor(extents_type(extents))
    {
    }

    Tensor(extents_type extents, const value_type& value)
        requires std::default_initializable<allocator_type>
        : Tensor(std::allocator_arg, allocator_type{}, std::move(extents), value)
    {
    }

    Tensor(std::initializer_list<size_type> extents, const value_type& value)
        requires std::default_initializable<allocator_type>
        : Tensor(extents_type(extents), value)
    {
    }

    Tensor(const std::vector<size_type>& extents, const value_type& value)
        requires std::default_initializable<allocator_type>
        : Tensor(extents_type(extents), value)
    {
    }

    Tensor(std::allocator_arg_t, const allocator_type& allocator, extents_type extents)
        requires std::default_initializable<value_type>
        : mAllocator(allocator)
        , mLayout(layout_type::contiguous(std::move(extents)))
        , mStorage(makeStorage(mAllocator, mLayout.logicalSize(), [](allocator_type& active, pointer location,
                                                                    size_type) {
            allocator_traits::construct(active, location);
        }))
        , mLifetime(std::make_shared<tensor_detail::TensorLifetimeState>())
    {
    }

    Tensor(std::allocator_arg_t, const allocator_type& allocator, std::initializer_list<size_type> extents)
        requires std::default_initializable<value_type>
        : Tensor(std::allocator_arg, allocator, extents_type(extents))
    {
    }

    Tensor(std::allocator_arg_t, const allocator_type& allocator, const std::vector<size_type>& extents)
        requires std::default_initializable<value_type>
        : Tensor(std::allocator_arg, allocator, extents_type(extents))
    {
    }

    Tensor(std::allocator_arg_t, const allocator_type& allocator, extents_type extents,
           const value_type& value)
        : mAllocator(allocator)
        , mLayout(layout_type::contiguous(std::move(extents)))
        , mStorage(makeStorage(mAllocator, mLayout.logicalSize(), [&value](allocator_type& active,
                                                                           pointer location, size_type) {
            allocator_traits::construct(active, location, value);
        }))
        , mLifetime(std::make_shared<tensor_detail::TensorLifetimeState>())
    {
    }

    Tensor(std::allocator_arg_t, const allocator_type& allocator, std::initializer_list<size_type> extents,
           const value_type& value)
        : Tensor(std::allocator_arg, allocator, extents_type(extents), value)
    {
    }

    Tensor(std::allocator_arg_t, const allocator_type& allocator, const std::vector<size_type>& extents,
           const value_type& value)
        : Tensor(std::allocator_arg, allocator, extents_type(extents), value)
    {
    }

    Tensor(const Tensor& other)
        : Tensor(other, allocator_traits::select_on_container_copy_construction(other.mAllocator))
    {
    }

    Tensor(const Tensor& other, const allocator_type& allocator)
        : mAllocator(allocator)
        , mLayout(other.mLayout)
        , mStorage(makeStorage(mAllocator, other.size(), [&other](allocator_type& active, pointer location,
                                                                  size_type index) {
            allocator_traits::construct(active, location, other.data()[index]);
        }))
        , mLifetime(std::make_shared<tensor_detail::TensorLifetimeState>())
    {
    }

    template <std::size_t OtherRank>
        requires(OtherRank != Rank)
    explicit Tensor(const Tensor<T, allocator_type, OtherRank>& other)
        : mAllocator(allocator_traits::select_on_container_copy_construction(other.mAllocator))
        , mLayout(layout_type::contiguous(convertExtents(other.extents())))
        , mStorage(makeStorage(mAllocator, other.size(), [&other](allocator_type& active, pointer location,
                                                                  size_type index) {
            allocator_traits::construct(active, location, other[index]);
        }))
        , mLifetime(std::make_shared<tensor_detail::TensorLifetimeState>())
    {
    }

    template <std::size_t OtherRank>
        requires(OtherRank != Rank)
    explicit Tensor(Tensor<T, allocator_type, OtherRank>&& other)
        : Tensor(CrossRankMoveTag{}, other, convertExtents(other.extents()))
    {
    }

    Tensor(Tensor&& other)
        : mAllocator(moveConstructAllocator(other))
        , mLifetime(std::make_shared<tensor_detail::TensorLifetimeState>())
    {
        stealStorageFrom(other);
    }

    Tensor(Tensor&& other, const allocator_type& allocator)
        : mAllocator(allocator)
        , mLifetime(std::make_shared<tensor_detail::TensorLifetimeState>())
    {
        if (allocatorCompatible(other.mAllocator))
        {
            stealStorageFrom(other);
        }
        else
        {
            materializeMoveFrom(other);
        }
    }

    ~Tensor()
    {
        tensor_detail::invalidateLifetime(mLifetime);
    }

    Tensor& operator=(const Tensor& other)
    {
        if (this == &other)
        {
            return *this;
        }

        allocator_type targetAllocator = mAllocator;
        if constexpr (allocator_traits::propagate_on_container_copy_assignment::value)
        {
            targetAllocator = other.mAllocator;
        }
        Tensor replacement(other, targetAllocator);
        if constexpr (allocator_traits::propagate_on_container_copy_assignment::value)
        {
            mAllocator = targetAllocator;
        }
        commitStorage(std::move(replacement));
        return *this;
    }

    Tensor& operator=(Tensor&& other)
    {
        if (this == &other)
        {
            return *this;
        }

        if constexpr (Rank == 0)
        {
            validateAliasPreservingElementMove(other);
        }

        if constexpr (allocator_traits::propagate_on_container_move_assignment::value)
        {
            if constexpr (Rank == 0)
            {
                allocator_type targetAllocator = other.mAllocator;
                auto nextLifetime = std::make_shared<tensor_detail::TensorLifetimeState>();
                auto nextSourceLifetime = std::make_shared<tensor_detail::TensorLifetimeState>();
                auto replacementStorage = makeRankZeroMoveStorage(other, targetAllocator);
                mAllocator = targetAllocator;
                tensor_detail::invalidateLifetime(mLifetime);
                tensor_detail::invalidateLifetime(other.mLifetime);
                mLayout = other.mLayout;
                mStorage = std::move(replacementStorage);
                mLifetime = std::move(nextLifetime);
                other.mLifetime = std::move(nextSourceLifetime);
            }
            else
            {
                auto nextLifetime = std::make_shared<tensor_detail::TensorLifetimeState>();
                auto nextSourceLifetime = std::make_shared<tensor_detail::TensorLifetimeState>();
                mAllocator = std::move(other.mAllocator);
                replaceByStealing(other, std::move(nextLifetime), std::move(nextSourceLifetime));
            }
        }
        else if (allocatorCompatible(other.mAllocator))
        {
            auto nextLifetime = std::make_shared<tensor_detail::TensorLifetimeState>();
            auto nextSourceLifetime = std::make_shared<tensor_detail::TensorLifetimeState>();
            replaceByStealing(other, std::move(nextLifetime), std::move(nextSourceLifetime));
        }
        else
        {
            Tensor replacement(std::move(other), mAllocator);
            commitStorage(std::move(replacement));
        }
        return *this;
    }

    [[nodiscard]] allocator_type get_allocator() const
    {
        return mAllocator;
    }

    [[nodiscard]] const layout_type& layout() const noexcept { return mLayout; }
    [[nodiscard]] const extents_type& extents() const noexcept { return mLayout.extents(); }
    [[nodiscard]] const strides_type& strides() const noexcept { return mLayout.strides(); }
    [[nodiscard]] size_type size() const noexcept { return mLayout.logicalSize(); }
    [[nodiscard]] size_type rank() const noexcept { return mLayout.rank(); }
    [[nodiscard]] size_type extent(size_type axis) const { return mLayout.extents().at(axis); }
    [[nodiscard]] bool empty() const noexcept { return size() == 0; }

    [[nodiscard]] pointer data() noexcept { return mStorage.get(); }
    [[nodiscard]] const_pointer data() const noexcept { return mStorage.get(); }

    [[nodiscard]] reference operator[](size_type linearIndex)
    {
        return mStorage.get()[linearIndex];
    }

    [[nodiscard]] const_reference operator[](size_type linearIndex) const
    {
        return mStorage.get()[linearIndex];
    }

    [[nodiscard]] reference atLinear(size_type linearIndex)
    {
        if (linearIndex >= size())
        {
            throw std::out_of_range("Tensor logical linear index is out of range");
        }
        return (*this)[linearIndex];
    }
    [[nodiscard]] const_reference atLinear(size_type linearIndex) const
    {
        if (linearIndex >= size())
        {
            throw std::out_of_range("Tensor logical linear index is out of range");
        }
        return (*this)[linearIndex];
    }

    template <std::integral... Indices>
        requires(Rank == tensor_detail::kDynamicTensorRank || sizeof...(Indices) == Rank)
    [[nodiscard]] reference operator()(Indices... indices)
    {
        if (sizeof...(Indices) != rank())
        {
            throw std::invalid_argument("Tensor index count does not match its rank");
        }
        return atIndices(std::array<difference_type, sizeof...(Indices)>{tensor_detail::checkedElementIndexCast<difference_type>(indices)...});
    }

    template <std::integral... Indices>
        requires(Rank == tensor_detail::kDynamicTensorRank || sizeof...(Indices) == Rank)
    [[nodiscard]] const_reference operator()(Indices... indices) const
    {
        if (sizeof...(Indices) != rank())
        {
            throw std::invalid_argument("Tensor index count does not match its rank");
        }
        return atIndices(std::array<difference_type, sizeof...(Indices)>{tensor_detail::checkedElementIndexCast<difference_type>(indices)...});
    }

    template <std::integral... Indices>
        requires(Rank == tensor_detail::kDynamicTensorRank || sizeof...(Indices) == Rank)
    [[nodiscard]] reference at(Indices... indices)
    {
        return (*this)(indices...);
    }

    template <std::integral... Indices>
        requires(Rank == tensor_detail::kDynamicTensorRank || sizeof...(Indices) == Rank)
    [[nodiscard]] const_reference at(Indices... indices) const
    {
        return (*this)(indices...);
    }

    [[nodiscard]] iterator begin() noexcept { return data(); }
    [[nodiscard]] iterator end() noexcept { return data() == nullptr ? nullptr : data() + size(); }
    [[nodiscard]] const_iterator begin() const noexcept { return data(); }
    [[nodiscard]] const_iterator end() const noexcept { return data() == nullptr ? nullptr : data() + size(); }
    [[nodiscard]] const_iterator cbegin() const noexcept { return begin(); }
    [[nodiscard]] const_iterator cend() const noexcept { return end(); }

    void fill(const value_type& value)
    {
        tensor_detail::fillKernel(*this, value);
    }

    [[nodiscard]] Tensor clone() const
    {
        return Tensor(*this);
    }

    [[nodiscard]] TensorView<value_type, Rank> asView() &
    {
        return TensorView<value_type, Rank>(data(), mLayout, mLifetime, true);
    }

    [[nodiscard]] TensorView<value_type, Rank> asView() && = delete;

    [[nodiscard]] TensorView<const value_type, Rank> asConstView() const &
    {
        return TensorView<const value_type, Rank>(data(), mLayout, mLifetime, true);
    }

    [[nodiscard]] TensorView<const value_type, Rank> asConstView() const && = delete;

    [[nodiscard]] SharedTensorView<value_type, Rank> asSharedView() &
    {
        return SharedTensorView<value_type, Rank>(sharedLifetimeHandle(),
                                                  TensorView<value_type, Rank>(data(), mLayout, {}, false));
    }

    [[nodiscard]] SharedTensorView<value_type, Rank> asSharedView() && = delete;

    [[nodiscard]] SharedTensorView<const value_type, Rank> asSharedView() const &
    {
        return SharedTensorView<const value_type, Rank>(
            sharedLifetimeHandle(), TensorView<const value_type, Rank>(data(), mLayout, {}, false));
    }

    [[nodiscard]] SharedTensorView<const value_type, Rank> asSharedView() const && = delete;

    [[nodiscard]] TensorView<value_type, Rank> sliceView(const std::vector<size_type>& start,
                                                         const std::vector<size_type>& finish) &
    {
        return asView().sliceView(start, finish);
    }

    [[nodiscard]] TensorView<const value_type, Rank> sliceView(const std::vector<size_type>& start,
                                                               const std::vector<size_type>& finish) const &
    {
        return asConstView().sliceView(start, finish);
    }

    [[nodiscard]] auto sliceView(const std::vector<SliceSpec>& specifications) &
    {
        return asView().sliceView(specifications);
    }

    [[nodiscard]] auto sliceView(const std::vector<SliceSpec>& specifications) const &
    {
        return asConstView().sliceView(specifications);
    }

    [[nodiscard]] auto sliceView(std::initializer_list<SliceSpec> specifications) &
    {
        return asView().sliceView(specifications);
    }

    [[nodiscard]] auto sliceView(std::initializer_list<SliceSpec> specifications) const &
    {
        return asConstView().sliceView(specifications);
    }

    template <typename... Specifications>
        requires(Rank != tensor_detail::kDynamicTensorRank && sizeof...(Specifications) > 0 &&
                 tensor_detail::typedSliceConsumedAxes<Specifications...> <= Rank &&
                 tensor_detail::typedSliceEllipsisCount<Specifications...> <= 1 &&
                 (tensor_detail::isTypedSliceSpecification<Specifications> && ...))
    [[nodiscard]] auto sliceView(Specifications&&... specifications) &
    {
        return asView().sliceView(std::forward<Specifications>(specifications)...);
    }

    template <typename... Specifications>
        requires(Rank != tensor_detail::kDynamicTensorRank && sizeof...(Specifications) > 0 &&
                 tensor_detail::typedSliceConsumedAxes<Specifications...> <= Rank &&
                 tensor_detail::typedSliceEllipsisCount<Specifications...> <= 1 &&
                 (tensor_detail::isTypedSliceSpecification<Specifications> && ...))
    [[nodiscard]] auto sliceView(Specifications&&... specifications) const &
    {
        return asConstView().sliceView(std::forward<Specifications>(specifications)...);
    }

    [[nodiscard]] TensorView<value_type, Rank> permuteView(const std::vector<TensorAxis>& order) &
    {
        return asView().permuteView(order);
    }

    [[nodiscard]] TensorView<const value_type, Rank> permuteView(const std::vector<TensorAxis>& order) const &
    {
        return asConstView().permuteView(order);
    }

    [[nodiscard]] auto squeezeView(const std::vector<TensorAxis>& axes = {}) &
    {
        return asView().squeezeView(axes);
    }

    [[nodiscard]] auto squeezeView(const std::vector<TensorAxis>& axes = {}) const &
    {
        return asConstView().squeezeView(axes);
    }

    template <TensorAxis... Axes>
        requires(Rank != tensor_detail::kDynamicTensorRank && sizeof...(Axes) > 0)
    [[nodiscard]] auto squeezeView() &
    {
        return asView().template squeezeView<Axes...>();
    }

    template <TensorAxis... Axes>
        requires(Rank != tensor_detail::kDynamicTensorRank && sizeof...(Axes) > 0)
    [[nodiscard]] auto squeezeView() const &
    {
        return asConstView().template squeezeView<Axes...>();
    }

    [[nodiscard]] auto unsqueezeView(TensorAxis axis) &
    {
        return asView().unsqueezeView(axis);
    }

    [[nodiscard]] auto unsqueezeView(TensorAxis axis) const &
    {
        return asConstView().unsqueezeView(axis);
    }

    [[nodiscard]] TensorView<value_type, Rank> rowView(size_type row) & { return asView().rowView(row); }
    [[nodiscard]] TensorView<const value_type, Rank> rowView(size_type row) const &
    {
        return asConstView().rowView(row);
    }
    [[nodiscard]] TensorView<value_type, Rank> columnView(size_type column) &
    {
        return asView().columnView(column);
    }
    [[nodiscard]] TensorView<const value_type, Rank> columnView(size_type column) const &
    {
        return asConstView().columnView(column);
    }
    [[nodiscard]] TensorView<value_type, Rank> transposeView() &
        requires(Rank == tensor_detail::kDynamicTensorRank || Rank == 2)
    {
        return asView().transposeView();
    }
    [[nodiscard]] TensorView<const value_type, Rank> transposeView() const &
        requires(Rank == tensor_detail::kDynamicTensorRank || Rank == 2)
    {
        return asConstView().transposeView();
    }
    [[nodiscard]] TensorView<value_type> reshapeView(DynamicExtents target) &
    {
        return asView().reshapeView(std::move(target));
    }
    [[nodiscard]] TensorView<const value_type> reshapeView(DynamicExtents target) const &
    {
        return asConstView().reshapeView(std::move(target));
    }
    [[nodiscard]] TensorView<const value_type> broadcastView(DynamicExtents target) const &
    {
        return asConstView().broadcastView(std::move(target));
    }

    template <std::size_t NewRank>
    [[nodiscard]] TensorView<value_type, NewRank>
    reshapeView(tensor_detail::FixedRankExtents<NewRank> target) &
    {
        return asView().reshapeView(std::move(target));
    }

    template <std::size_t NewRank>
    [[nodiscard]] TensorView<const value_type, NewRank>
    reshapeView(tensor_detail::FixedRankExtents<NewRank> target) const &
    {
        return asConstView().reshapeView(std::move(target));
    }

    template <std::size_t NewRank>
    [[nodiscard]] TensorView<const value_type, NewRank>
    broadcastView(tensor_detail::FixedRankExtents<NewRank> target) const &
        requires(Rank == tensor_detail::kDynamicTensorRank || NewRank >= Rank)
    {
        return asConstView().broadcastView(std::move(target));
    }

    // Borrowed derived views may not escape a temporary owner.
    TensorView<value_type, Rank> sliceView(const std::vector<size_type>&,
                                           const std::vector<size_type>&) && = delete;
    TensorView<const value_type, Rank> sliceView(const std::vector<size_type>&,
                                                 const std::vector<size_type>&) const && = delete;

    void sliceView(const std::vector<SliceSpec>&) && = delete;
    void sliceView(const std::vector<SliceSpec>&) const && = delete;
    void sliceView(std::initializer_list<SliceSpec>) && = delete;
    void sliceView(std::initializer_list<SliceSpec>) const && = delete;

    template <typename... Specifications>
        requires(Rank != tensor_detail::kDynamicTensorRank && sizeof...(Specifications) > 0 &&
                 tensor_detail::typedSliceConsumedAxes<Specifications...> <= Rank &&
                 tensor_detail::typedSliceEllipsisCount<Specifications...> <= 1 &&
                 (tensor_detail::isTypedSliceSpecification<Specifications> && ...))
    void sliceView(Specifications&&...) && = delete;

    template <typename... Specifications>
        requires(Rank != tensor_detail::kDynamicTensorRank && sizeof...(Specifications) > 0 &&
                 tensor_detail::typedSliceConsumedAxes<Specifications...> <= Rank &&
                 tensor_detail::typedSliceEllipsisCount<Specifications...> <= 1 &&
                 (tensor_detail::isTypedSliceSpecification<Specifications> && ...))
    void sliceView(Specifications&&...) const && = delete;

    TensorView<value_type, Rank> permuteView(const std::vector<TensorAxis>&) && = delete;
    TensorView<const value_type, Rank> permuteView(const std::vector<TensorAxis>&) const && = delete;

    void squeezeView(const std::vector<TensorAxis>& = {}) && = delete;
    void squeezeView(const std::vector<TensorAxis>& = {}) const && = delete;

    template <TensorAxis... Axes>
        requires(Rank != tensor_detail::kDynamicTensorRank && sizeof...(Axes) > 0)
    void squeezeView() && = delete;

    template <TensorAxis... Axes>
        requires(Rank != tensor_detail::kDynamicTensorRank && sizeof...(Axes) > 0)
    void squeezeView() const && = delete;

    void unsqueezeView(TensorAxis) && = delete;
    void unsqueezeView(TensorAxis) const && = delete;
    TensorView<value_type, Rank> rowView(size_type) && = delete;
    TensorView<const value_type, Rank> rowView(size_type) const && = delete;
    TensorView<value_type, Rank> columnView(size_type) && = delete;
    TensorView<const value_type, Rank> columnView(size_type) const && = delete;

    TensorView<value_type, Rank> transposeView() &&
        requires(Rank == tensor_detail::kDynamicTensorRank || Rank == 2)
        = delete;
    TensorView<const value_type, Rank> transposeView() const &&
        requires(Rank == tensor_detail::kDynamicTensorRank || Rank == 2)
        = delete;

    TensorView<value_type> reshapeView(DynamicExtents) && = delete;
    TensorView<const value_type> reshapeView(DynamicExtents) const && = delete;
    TensorView<const value_type> broadcastView(DynamicExtents) && = delete;
    TensorView<const value_type> broadcastView(DynamicExtents) const && = delete;

    template <std::size_t NewRank>
    TensorView<value_type, NewRank> reshapeView(tensor_detail::FixedRankExtents<NewRank>) && = delete;

    template <std::size_t NewRank>
    TensorView<const value_type, NewRank>
    reshapeView(tensor_detail::FixedRankExtents<NewRank>) const && = delete;

    template <std::size_t NewRank>
    TensorView<const value_type, NewRank>
    broadcastView(tensor_detail::FixedRankExtents<NewRank>) &&
        requires(Rank == tensor_detail::kDynamicTensorRank || NewRank >= Rank)
        = delete;

    template <std::size_t NewRank>
    TensorView<const value_type, NewRank>
    broadcastView(tensor_detail::FixedRankExtents<NewRank>) const &&
        requires(Rank == tensor_detail::kDynamicTensorRank || NewRank >= Rank)
        = delete;

    [[nodiscard]] SharedTensorView<value_type, Rank> sharedSliceView(const std::vector<size_type>& start,
                                                                     const std::vector<size_type>& finish) &
    {
        return asSharedView().sliceView(start, finish);
    }
    [[nodiscard]] SharedTensorView<const value_type, Rank>
    sharedSliceView(const std::vector<size_type>& start, const std::vector<size_type>& finish) const &
    {
        return asSharedView().sliceView(start, finish);
    }

    SharedTensorView<value_type, Rank>
    sharedSliceView(const std::vector<size_type>&, const std::vector<size_type>&) && = delete;
    SharedTensorView<const value_type, Rank>
    sharedSliceView(const std::vector<size_type>&, const std::vector<size_type>&) const && = delete;

    template <typename Deleter>
    [[nodiscard]] static Tensor adopt(pointer storage, extents_type extents, Deleter deleter,
                                      const allocator_type& futureAllocator)
    {
        auto layout = layout_type::contiguous(std::move(extents));
        if (layout.logicalSize() != 0 && storage == nullptr)
        {
            throw std::invalid_argument("Cannot adopt null storage for a nonempty Tensor");
        }
        return Tensor(AdoptTag{}, futureAllocator, std::move(layout),
                      std::shared_ptr<value_type[]>(storage, std::move(deleter)));
    }

    template <typename Deleter>
    [[nodiscard]] static Tensor adopt(pointer storage, extents_type extents, Deleter deleter)
        requires std::default_initializable<allocator_type>
    {
        return adopt(storage, std::move(extents), std::move(deleter), allocator_type{});
    }

    void swap(Tensor& other)
    {
        if (this == &other)
        {
            return;
        }
        if constexpr (!allocator_traits::propagate_on_container_swap::value &&
                      !allocator_traits::is_always_equal::value)
        {
            if (!(mAllocator == other.mAllocator))
            {
                throw std::invalid_argument("Cannot swap Tensors with unequal non-propagating allocators");
            }
        }

        auto leftLifetime = std::make_shared<tensor_detail::TensorLifetimeState>();
        auto rightLifetime = std::make_shared<tensor_detail::TensorLifetimeState>();
        if constexpr (allocator_traits::propagate_on_container_swap::value)
        {
            using std::swap;
            swap(mAllocator, other.mAllocator);
        }
        tensor_detail::invalidateLifetime(mLifetime);
        tensor_detail::invalidateLifetime(other.mLifetime);
        using std::swap;
        swap(mLayout, other.mLayout);
        swap(mStorage, other.mStorage);
        mLifetime = std::move(leftLifetime);
        other.mLifetime = std::move(rightLifetime);
    }

    friend void swap(Tensor& left, Tensor& right) noexcept(noexcept(left.swap(right)))
    {
        left.swap(right);
    }

    friend bool operator==(const Tensor& left, const Tensor& right)
    {
        if (left.extents() != right.extents())
        {
            return false;
        }
        return tensor_detail::equalKernel(left, right, std::equal_to<value_type>{});
    }

private:
    friend struct tensor_detail::TensorAccess;
    template <typename, typename, std::size_t>
    friend class Tensor;

    template <typename Extents>
    [[nodiscard]] static extents_type convertExtents(const Extents& extents)
    {
        if constexpr (Rank == tensor_detail::kDynamicTensorRank)
        {
            return tensor_detail::makeDynamicExtents(extents);
        }
        else
        {
            return tensor_detail::makeFixedExtents<Rank>(extents);
        }
    }

    [[nodiscard]] static extents_type defaultExtents()
    {
        if constexpr (Rank == tensor_detail::kDynamicTensorRank)
        {
            return extents_type{0};
        }
        else
        {
            return extents_type{};
        }
    }

    struct AdoptTag
    {
    };

    struct CrossRankMoveTag
    {
    };

    [[nodiscard]] static allocator_type moveConstructAllocator(Tensor& other)
    {
        if constexpr (Rank == 0)
        {
            return other.mAllocator;
        }
        else
        {
            return std::move(other.mAllocator);
        }
    }

    template <std::size_t OtherRank>
    [[nodiscard]] static allocator_type
    moveConstructAllocator(Tensor<T, allocator_type, OtherRank>& other)
    {
        if constexpr (OtherRank == 0)
        {
            return other.mAllocator;
        }
        else
        {
            return std::move(other.mAllocator);
        }
    }

    template <std::size_t OtherRank>
    Tensor(CrossRankMoveTag, Tensor<T, allocator_type, OtherRank>& other,
           extents_type checkedExtents)
        : mAllocator(moveConstructAllocator(other))
        , mLayout(layout_type::contiguous(std::move(checkedExtents)))
        , mLifetime(std::make_shared<tensor_detail::TensorLifetimeState>())
    {
        if constexpr (OtherRank == 0)
        {
            validateAliasPreservingElementMove(other);
        }
        auto nextSourceLifetime = std::make_shared<tensor_detail::TensorLifetimeState>();
        if constexpr (OtherRank == 0)
        {
            mStorage = makeRankZeroMoveStorage(other, mAllocator);
            tensor_detail::invalidateLifetime(other.mLifetime);
            other.mLifetime = std::move(nextSourceLifetime);
        }
        else
        {
            tensor_detail::invalidateLifetime(other.mLifetime);
            mStorage = std::move(other.mStorage);
            other.mLayout = decltype(other.mLayout)::contiguous(other.defaultExtents());
            other.mLifetime = std::move(nextSourceLifetime);
        }
    }

    Tensor(AdoptTag, const allocator_type& allocator, layout_type layout, std::shared_ptr<value_type[]> storage)
        : mAllocator(allocator)
        , mLayout(std::move(layout))
        , mStorage(std::move(storage))
        , mLifetime(std::make_shared<tensor_detail::TensorLifetimeState>())
    {
    }

    template <typename Constructor>
    [[nodiscard]] static std::shared_ptr<value_type[]> makeStorage(const allocator_type& allocator, size_type count,
                                                                   Constructor&& constructor)
    {
        if (count == 0)
        {
            return {};
        }
        auto allocatorState = std::make_shared<allocator_type>(allocator);
        pointer raw = allocator_traits::allocate(*allocatorState, count);
        size_type constructed = 0;
        try
        {
            for (; constructed < count; ++constructed)
            {
                constructor(*allocatorState, raw + constructed, constructed);
            }
        }
        catch (...)
        {
            while (constructed > 0)
            {
                --constructed;
                allocator_traits::destroy(*allocatorState, raw + constructed);
            }
            allocator_traits::deallocate(*allocatorState, raw, count);
            throw;
        }

        auto deleter = [allocatorState, count](pointer storage) noexcept {
            for (size_type index = count; index > 0; --index)
            {
                allocator_traits::destroy(*allocatorState, storage + (index - 1));
            }
            allocator_traits::deallocate(*allocatorState, storage, count);
        };
        return std::shared_ptr<value_type[]>(raw, std::move(deleter));
    }

    [[nodiscard]] bool allocatorCompatible(const allocator_type& other) const
    {
        if constexpr (allocator_traits::is_always_equal::value)
        {
            return true;
        }
        else
        {
            return mAllocator == other;
        }
    }

    template <std::size_t OtherRank>
    static void validateAliasPreservingElementMove(const Tensor<T, allocator_type, OtherRank>& other)
    {
        static_assert(OtherRank == 0);
        if constexpr (!std::copy_constructible<value_type>)
        {
            if (other.mStorage.use_count() > 1)
            {
                throw std::logic_error(
                    "Cannot safely move shared rank-zero Tensor storage while preserving aliases for a non-copyable value "
                    "type.");
            }
        }
    }

    template <std::size_t OtherRank>
    [[nodiscard]] std::shared_ptr<value_type[]>
    makeRankZeroMoveStorage(Tensor<T, allocator_type, OtherRank>& other,
                            const allocator_type& targetAllocator)
    {
        static_assert(OtherRank == 0);
        validateAliasPreservingElementMove(other);
        if constexpr (std::copy_constructible<value_type>)
        {
            if (other.mStorage.use_count() > 1)
            {
                return makeStorage(targetAllocator, other.size(), [&other](allocator_type& active, pointer location,
                                                                            size_type index) {
                    allocator_traits::construct(active, location, other.data()[index]);
                });
            }
        }
        return makeStorage(targetAllocator, other.size(), [&other](allocator_type& active, pointer location,
                                                                    size_type index) {
            allocator_traits::construct(active, location, std::move(other.data()[index]));
        });
    }

    void materializeMoveFrom(Tensor& other)
    {
        const bool mustPreserveAliases = other.mStorage.use_count() > 1;
        if constexpr (!std::copy_constructible<value_type>)
        {
            if (mustPreserveAliases)
            {
                throw std::logic_error(
                    "Cannot move shared Tensor storage across unequal allocators for a non-copyable value type");
            }
        }
        auto nextSourceLifetime = std::make_shared<tensor_detail::TensorLifetimeState>();
        if (mustPreserveAliases)
        {
            if constexpr (std::copy_constructible<value_type>)
            {
                mLayout = other.mLayout;
                mStorage = makeStorage(mAllocator, other.size(), [&other](allocator_type& active, pointer location,
                                                                          size_type index) {
                    allocator_traits::construct(active, location, other.data()[index]);
                });
            }
        }
        else
        {
            mLayout = other.mLayout;
            mStorage = makeStorage(mAllocator, other.size(), [&other](allocator_type& active, pointer location,
                                                                      size_type index) {
                allocator_traits::construct(active, location, std::move(other.data()[index]));
            });
        }
        if constexpr (Rank == 0)
        {
            tensor_detail::invalidateLifetime(other.mLifetime);
            other.mLifetime = std::move(nextSourceLifetime);
        }
        else
        {
            resetMovedFrom(other, std::move(nextSourceLifetime));
        }
    }

    void stealStorageFrom(Tensor& other)
    {
        if constexpr (Rank == 0)
        {
            materializeMoveFrom(other);
        }
        else
        {
            auto nextSourceLifetime = std::make_shared<tensor_detail::TensorLifetimeState>();
            mLayout = std::move(other.mLayout);
            mStorage = std::move(other.mStorage);
            resetMovedFrom(other, std::move(nextSourceLifetime));
        }
    }

    void replaceByStealing(Tensor& other, std::shared_ptr<tensor_detail::TensorLifetimeState> nextLifetime,
                          std::shared_ptr<tensor_detail::TensorLifetimeState> nextSourceLifetime)
    {
        if constexpr (Rank == 0)
        {
            auto replacementStorage = makeRankZeroMoveStorage(other, mAllocator);
            tensor_detail::invalidateLifetime(mLifetime);
            tensor_detail::invalidateLifetime(other.mLifetime);
            mLayout = other.mLayout;
            mStorage = std::move(replacementStorage);
            mLifetime = std::move(nextLifetime);
            other.mLifetime = std::move(nextSourceLifetime);
        }
        else
        {
            tensor_detail::invalidateLifetime(mLifetime);
            tensor_detail::invalidateLifetime(other.mLifetime);
            mLayout = std::move(other.mLayout);
            mStorage = std::move(other.mStorage);
            mLifetime = std::move(nextLifetime);
            other.mLayout = layout_type::contiguous(defaultExtents());
            other.mLifetime = std::move(nextSourceLifetime);
        }
    }

    void resetMovedFrom(Tensor& other, std::shared_ptr<tensor_detail::TensorLifetimeState> nextLifetime)
    {
        tensor_detail::invalidateLifetime(other.mLifetime);
        other.mLayout = layout_type::contiguous(defaultExtents());
        other.mStorage.reset();
        other.mLifetime = std::move(nextLifetime);
    }

    void commitStorage(Tensor&& replacement)
    {
        tensor_detail::invalidateLifetime(mLifetime);
        mLayout = std::move(replacement.mLayout);
        mStorage = std::move(replacement.mStorage);
        mLifetime = std::move(replacement.mLifetime);
    }

    [[nodiscard]] std::shared_ptr<void> sharedLifetimeHandle() const
    {
        if (mStorage)
        {
            return std::shared_ptr<void>(mStorage, static_cast<void*>(mStorage.get()));
        }
        return std::shared_ptr<void>(mLifetime, static_cast<void*>(mLifetime.get()));
    }

    template <std::size_t IndexRank>
    [[nodiscard]] reference atIndices(const std::array<difference_type, IndexRank>& indices)
    {
        return const_cast<reference>(std::as_const(*this).atIndices(indices));
    }

    template <std::size_t IndexRank>
    [[nodiscard]] const_reference atIndices(const std::array<difference_type, IndexRank>& indices) const
    {
        if (IndexRank != rank())
        {
            throw std::invalid_argument("Tensor index count does not match its rank");
        }
        difference_type offset = mLayout.originOffset();
        if constexpr (IndexRank > 0)
        {
            for (size_type axis = 0; axis < IndexRank; ++axis)
            {
                if (indices[axis] < 0 || static_cast<size_type>(indices[axis]) >= extent(axis))
                {
                    throw std::out_of_range("Tensor multidimensional index is out of range");
                }
                offset = tensor_detail::checkedOffsetAdd(
                    offset, tensor_detail::checkedStrideContribution(
                                static_cast<size_type>(indices[axis]), mLayout.strides()[axis]));
            }
        }
        return mStorage.get()[offset];
    }

    allocator_type mAllocator;
    layout_type mLayout = layout_type::contiguous(defaultExtents());
    std::shared_ptr<value_type[]> mStorage;
    std::shared_ptr<tensor_detail::TensorLifetimeState> mLifetime;
};

template <ReadableTensor R, typename Allocator>
[[nodiscard]] auto clone(const R& source, const Allocator& allocator)
    -> Tensor<typename R::value_type, Allocator, tensor_detail::tensorStaticRankValue<R>>
{
    using value_type = typename R::value_type;
    tensor_detail::TensorAccess::validate(source);
    Tensor<value_type, Allocator, tensor_detail::tensorStaticRankValue<R>> result(
        std::allocator_arg, allocator, source.extents());
    tensor_detail::copyKernel(source, result);
    return result;
}

template <ReadableTensor R>
[[nodiscard]] auto clone(const R& source)
{
    using value_type = typename R::value_type;
    if constexpr (requires { source.get_allocator(); source.clone(); })
    {
        return source.clone();
    }
    else
    {
        return clone(source, TensorAllocator<value_type>{});
    }
}

template <typename T, typename LeftAllocator, std::size_t LeftRank, typename RightAllocator,
          std::size_t RightRank>
    requires(LeftRank != RightRank || !std::same_as<LeftAllocator, RightAllocator>)
[[nodiscard]] bool operator==(const Tensor<T, LeftAllocator, LeftRank>& left,
                              const Tensor<T, RightAllocator, RightRank>& right)
{
    if (left.extents() != right.extents())
    {
        return false;
    }
    return tensor_detail::equalKernel(left, right, std::equal_to<T>{});
}

} // namespace fat_p

namespace std
{

template <typename T, typename Allocator, std::size_t Rank>
struct hash<fat_p::Tensor<T, Allocator, Rank>>
{
    [[nodiscard]] size_t operator()(const fat_p::Tensor<T, Allocator, Rank>& tensor) const
    {
        return fat_p::tensor_detail::hashKernel(tensor, std::hash<T>{});
    }
};

} // namespace std
