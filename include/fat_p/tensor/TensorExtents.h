#pragma once

/*
FATP_META:
  meta_version: 1
  component: TensorLayout
  file_role: internal_header
  path: include/fat_p/tensor/TensorExtents.h
  namespace: fat_p
  layer: Domain
  summary: "Checked runtime Tensor extents with inline common-rank metadata and normalized axes."
  api_stability: in_work
  related:
    docs:
      - components/Tensor/docs/Design Note - Tensor Semantic Contract.md
      - components/Tensor/docs/Design Note - Tensor Architecture Additions Plan.md
    headers:
      - include/fat_p/TensorLayout.h
      - include/fat_p/tensor/TensorLayout.h
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
 * @file TensorExtents.h
 * @brief Checked runtime extents and axis normalization for Tensor layouts.
 */

#include <algorithm>
#include <array>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace fat_p
{

using TensorAxis = std::ptrdiff_t;

namespace tensor_detail
{

inline constexpr std::size_t kTensorInlineRank = 4;
inline constexpr std::size_t kDynamicTensorRank = std::numeric_limits<std::size_t>::max();

/**
 * @brief Small-buffer storage for runtime Tensor extents and strides.
 *
 * @details Tensor metadata contains only trivial integral values. Keeping the
 * common ranks inline avoids making every owner, view, and iterator allocate
 * separate extent and stride buffers. Higher ranks transparently spill to a
 * standard vector and remain unbounded.
 */
template <typename T, std::size_t InlineCapacity = kTensorInlineRank>
class TensorMetadataStorage
{
    static_assert(InlineCapacity > 0, "Tensor metadata inline capacity must be positive");
    static_assert(std::is_trivially_copyable_v<T>, "Tensor metadata values must be trivially copyable");

    struct InlineStorage
    {
        std::array<T, InlineCapacity> values{};
        std::size_t size = 0;
    };

    using HeapStorage = std::vector<T>;

public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = value_type*;
    using const_pointer = const value_type*;
    using iterator = pointer;
    using const_iterator = const_pointer;

    TensorMetadataStorage() noexcept = default;

    TensorMetadataStorage(std::initializer_list<value_type> values)
    {
        assign(values.begin(), values.size());
    }

    template <std::input_iterator Iterator>
    TensorMetadataStorage(Iterator first, Iterator last)
    {
        for (; first != last; ++first)
        {
            push_back(*first);
        }
    }

    explicit TensorMetadataStorage(size_type count)
    {
        resize(count);
    }

    TensorMetadataStorage(size_type count, const value_type& value)
    {
        resize(count, value);
    }

    TensorMetadataStorage(const std::vector<value_type>& values)
    {
        assign(values.data(), values.size());
    }

    TensorMetadataStorage(std::vector<value_type>&& values)
    {
        if (values.size() <= InlineCapacity)
        {
            assign(values.data(), values.size());
        }
        else
        {
            mStorage.template emplace<HeapStorage>(std::move(values));
        }
    }

    TensorMetadataStorage(const TensorMetadataStorage& other)
    {
        // Keep a fully constructed inline alternative while a heap copy can throw.
        if (const auto* values = std::get_if<InlineStorage>(&other.mStorage))
        {
            mStorage.template emplace<InlineStorage>(*values);
        }
        else
        {
            HeapStorage heapValues(std::get<HeapStorage>(other.mStorage));
            mStorage.template emplace<HeapStorage>(std::move(heapValues));
        }
    }
    TensorMetadataStorage(TensorMetadataStorage&&) noexcept = default;

    TensorMetadataStorage& operator=(const TensorMetadataStorage& other)
    {
        if (this != &other)
        {
            TensorMetadataStorage replacement(other);
            mStorage = std::move(replacement.mStorage);
        }
        return *this;
    }

    TensorMetadataStorage& operator=(TensorMetadataStorage&&) noexcept = default;

    [[nodiscard]] size_type size() const noexcept
    {
        if (const auto* values = std::get_if<InlineStorage>(&mStorage))
        {
            return values->size;
        }
        return std::get<HeapStorage>(mStorage).size();
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return size() == 0;
    }

    [[nodiscard]] size_type capacity() const noexcept
    {
        if (std::holds_alternative<InlineStorage>(mStorage))
        {
            return InlineCapacity;
        }
        return std::get<HeapStorage>(mStorage).capacity();
    }

    [[nodiscard]] bool usesInlineStorage() const noexcept
    {
        return std::holds_alternative<InlineStorage>(mStorage);
    }

    [[nodiscard]] pointer data() noexcept
    {
        if (auto* values = std::get_if<InlineStorage>(&mStorage))
        {
            return values->values.data();
        }
        return std::get<HeapStorage>(mStorage).data();
    }

    [[nodiscard]] const_pointer data() const noexcept
    {
        if (const auto* values = std::get_if<InlineStorage>(&mStorage))
        {
            return values->values.data();
        }
        return std::get<HeapStorage>(mStorage).data();
    }

    [[nodiscard]] iterator begin() noexcept { return data(); }
    [[nodiscard]] const_iterator begin() const noexcept { return data(); }
    [[nodiscard]] const_iterator cbegin() const noexcept { return data(); }
    [[nodiscard]] iterator end() noexcept { return data() + size(); }
    [[nodiscard]] const_iterator end() const noexcept { return data() + size(); }
    [[nodiscard]] const_iterator cend() const noexcept { return data() + size(); }

    [[nodiscard]] reference operator[](size_type index) noexcept { return data()[index]; }
    [[nodiscard]] const_reference operator[](size_type index) const noexcept { return data()[index]; }

    [[nodiscard]] reference at(size_type index)
    {
        if (index >= size())
        {
            throw std::out_of_range("Tensor metadata index is out of range");
        }
        return (*this)[index];
    }

    [[nodiscard]] const_reference at(size_type index) const
    {
        if (index >= size())
        {
            throw std::out_of_range("Tensor metadata index is out of range");
        }
        return (*this)[index];
    }

    [[nodiscard]] reference front() noexcept { return (*this)[0]; }
    [[nodiscard]] const_reference front() const noexcept { return (*this)[0]; }
    [[nodiscard]] reference back() noexcept { return (*this)[size() - 1]; }
    [[nodiscard]] const_reference back() const noexcept { return (*this)[size() - 1]; }

    void reserve(size_type requested)
    {
        if (requested <= capacity())
        {
            return;
        }
        if (auto* values = std::get_if<HeapStorage>(&mStorage))
        {
            values->reserve(requested);
            return;
        }
        spillToHeap(requested);
    }

    void clear() noexcept
    {
        mStorage.template emplace<InlineStorage>();
    }

    void push_back(const value_type& value)
    {
        if (auto* values = std::get_if<InlineStorage>(&mStorage))
        {
            if (values->size < InlineCapacity)
            {
                values->values[values->size++] = value;
                return;
            }
            const value_type preservedValue = value;
            spillToHeap(values->size + 1);
            std::get<HeapStorage>(mStorage).push_back(preservedValue);
            return;
        }
        std::get<HeapStorage>(mStorage).push_back(value);
    }

    void push_back(value_type&& value)
    {
        push_back(static_cast<const value_type&>(value));
    }

    template <typename... Args>
    reference emplace_back(Args&&... args)
    {
        value_type value(std::forward<Args>(args)...);
        push_back(std::move(value));
        return back();
    }

    void pop_back()
    {
        if (auto* values = std::get_if<InlineStorage>(&mStorage))
        {
            --values->size;
            return;
        }
        auto& values = std::get<HeapStorage>(mStorage);
        values.pop_back();
        moveToInlineIfPossible();
    }

    void resize(size_type requested)
    {
        resize(requested, value_type{});
    }

    void resize(size_type requested, const value_type& value)
    {
        if (requested <= InlineCapacity)
        {
            if (const auto* heapValues = std::get_if<HeapStorage>(&mStorage))
            {
                InlineStorage inlineValues;
                const auto retained = std::min(requested, heapValues->size());
                std::copy_n(heapValues->begin(), retained, inlineValues.values.begin());
                std::fill(inlineValues.values.begin() + static_cast<difference_type>(retained),
                          inlineValues.values.begin() + static_cast<difference_type>(requested), value);
                inlineValues.size = requested;
                mStorage.template emplace<InlineStorage>(std::move(inlineValues));
                return;
            }
            auto& values = std::get<InlineStorage>(mStorage);
            while (values.size < requested)
            {
                values.values[values.size++] = value;
            }
            values.size = requested;
            return;
        }

        if (std::holds_alternative<InlineStorage>(mStorage))
        {
            const value_type preservedValue = value;
            spillToHeap(requested);
            std::get<HeapStorage>(mStorage).resize(requested, preservedValue);
            return;
        }
        std::get<HeapStorage>(mStorage).resize(requested, value);
    }

    iterator insert(const_iterator position, const value_type& value)
    {
        const auto index = static_cast<size_type>(position - begin());
        if (auto* values = std::get_if<InlineStorage>(&mStorage))
        {
            const value_type preservedValue = value;
            if (values->size < InlineCapacity)
            {
                std::move_backward(values->values.begin() + static_cast<difference_type>(index),
                                   values->values.begin() + static_cast<difference_type>(values->size),
                                   values->values.begin() + static_cast<difference_type>(values->size + 1));
                values->values[index] = preservedValue;
                ++values->size;
                return begin() + static_cast<difference_type>(index);
            }
            spillToHeap(values->size + 1);
            auto& heapValues = std::get<HeapStorage>(mStorage);
            heapValues.insert(heapValues.begin() + static_cast<difference_type>(index), preservedValue);
            return heapValues.data() + static_cast<difference_type>(index);
        }
        auto& values = std::get<HeapStorage>(mStorage);
        values.insert(values.begin() + static_cast<difference_type>(index), value);
        return values.data() + static_cast<difference_type>(index);
    }

    iterator insert(const_iterator position, value_type&& value)
    {
        return insert(position, static_cast<const value_type&>(value));
    }

    iterator erase(const_iterator position)
    {
        return erase(position, position + 1);
    }

    iterator erase(const_iterator first, const_iterator last)
    {
        const auto firstIndex = static_cast<size_type>(first - begin());
        const auto lastIndex = static_cast<size_type>(last - begin());
        if (auto* values = std::get_if<InlineStorage>(&mStorage))
        {
            std::move(values->values.begin() + static_cast<difference_type>(lastIndex),
                      values->values.begin() + static_cast<difference_type>(values->size),
                      values->values.begin() + static_cast<difference_type>(firstIndex));
            values->size -= lastIndex - firstIndex;
            return begin() + static_cast<difference_type>(firstIndex);
        }

        auto& values = std::get<HeapStorage>(mStorage);
        values.erase(values.begin() + static_cast<difference_type>(firstIndex),
                     values.begin() + static_cast<difference_type>(lastIndex));
        moveToInlineIfPossible();
        return begin() + static_cast<difference_type>(firstIndex);
    }

    friend bool operator==(const TensorMetadataStorage& left, const TensorMetadataStorage& right) noexcept
    {
        return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin());
    }

    friend bool operator==(const TensorMetadataStorage& left, const std::vector<value_type>& right) noexcept
    {
        return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin());
    }

    friend bool operator==(const std::vector<value_type>& left, const TensorMetadataStorage& right) noexcept
    {
        return right == left;
    }

private:
    void assign(const_pointer values, size_type count)
    {
        if (count <= InlineCapacity)
        {
            InlineStorage inlineValues;
            if (count != 0)
            {
                std::copy_n(values, count, inlineValues.values.begin());
            }
            inlineValues.size = count;
            mStorage.template emplace<InlineStorage>(std::move(inlineValues));
            return;
        }
        HeapStorage heapValues(values, values + count);
        mStorage.template emplace<HeapStorage>(std::move(heapValues));
    }

    void spillToHeap(size_type requestedCapacity)
    {
        const auto& inlineValues = std::get<InlineStorage>(mStorage);
        HeapStorage heapValues;
        heapValues.reserve(std::max(requestedCapacity, InlineCapacity * 2));
        for (size_type index = 0; index < InlineCapacity && index < inlineValues.size; ++index)
        {
            heapValues.push_back(inlineValues.values[index]);
        }
        mStorage.template emplace<HeapStorage>(std::move(heapValues));
    }

    void moveToInlineIfPossible()
    {
        auto* heapValues = std::get_if<HeapStorage>(&mStorage);
        if (heapValues == nullptr || heapValues->size() > InlineCapacity)
        {
            return;
        }
        InlineStorage inlineValues;
        std::copy(heapValues->begin(), heapValues->end(), inlineValues.values.begin());
        inlineValues.size = heapValues->size();
        mStorage.template emplace<InlineStorage>(std::move(inlineValues));
    }

    std::variant<InlineStorage, HeapStorage> mStorage;
};

template <typename Extents>
inline std::size_t checkedLogicalSize(const Extents& extents)
{
    if (extents.empty())
    {
        return 1;
    }
    if (std::find(extents.begin(), extents.end(), std::size_t{0}) != extents.end())
    {
        return 0;
    }

    std::size_t result = 1;
    constexpr auto maximumPointerDifference = static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max());
    for (const auto extent : extents)
    {
        if (extent > std::numeric_limits<std::size_t>::max() / result)
        {
            throw std::overflow_error("Tensor extent product exceeds size_t");
        }
        result *= extent;
        if (result > maximumPointerDifference)
        {
            throw std::overflow_error("Tensor extent product exceeds ptrdiff_t");
        }
    }
    return result;
}

} // namespace tensor_detail

using TensorStrides = tensor_detail::TensorMetadataStorage<std::ptrdiff_t>;

class DynamicExtents
{
public:
    using value_type = std::size_t;
    using container_type = tensor_detail::TensorMetadataStorage<value_type>;
    using const_iterator = container_type::const_iterator;

    DynamicExtents()
        : mLogicalSize(1)
    {
    }

    DynamicExtents(const DynamicExtents&) = default;
    DynamicExtents& operator=(const DynamicExtents&) = default;

    DynamicExtents(DynamicExtents&& other) noexcept
        : mExtents(std::move(other.mExtents))
        , mLogicalSize(other.mLogicalSize)
    {
        other.mExtents.clear();
        other.mLogicalSize = 1;
    }

    DynamicExtents& operator=(DynamicExtents&& other) noexcept
    {
        if (this != &other)
        {
            mExtents = std::move(other.mExtents);
            mLogicalSize = other.mLogicalSize;
            other.mExtents.clear();
            other.mLogicalSize = 1;
        }
        return *this;
    }

    DynamicExtents(std::initializer_list<value_type> extents)
        : DynamicExtents(container_type(extents))
    {
    }

    explicit DynamicExtents(container_type extents)
        : mExtents(std::move(extents))
        , mLogicalSize(tensor_detail::checkedLogicalSize(mExtents))
    {
    }

    template <std::input_iterator Iterator>
    DynamicExtents(Iterator first, Iterator last)
        : DynamicExtents(container_type(first, last))
    {
    }

    [[nodiscard]] std::size_t rank() const noexcept
    {
        return mExtents.size();
    }

    [[nodiscard]] std::size_t logicalSize() const noexcept
    {
        return mLogicalSize;
    }

    [[nodiscard]] bool hasZeroExtent() const noexcept
    {
        return mLogicalSize == 0;
    }

    [[nodiscard]] const container_type& values() const noexcept
    {
        return mExtents;
    }

    [[nodiscard]] value_type operator[](std::size_t axis) const noexcept
    {
        return mExtents[axis];
    }

    [[nodiscard]] value_type at(std::size_t axis) const
    {
        return mExtents.at(axis);
    }

    [[nodiscard]] const_iterator begin() const noexcept
    {
        return mExtents.begin();
    }

    [[nodiscard]] const_iterator end() const noexcept
    {
        return mExtents.end();
    }

    friend bool operator==(const DynamicExtents&, const DynamicExtents&) = default;

private:
    container_type mExtents;
    std::size_t mLogicalSize = 1;
};

namespace tensor_detail
{

/**
 * @brief Fixed-rank, runtime-valued Tensor extents.
 *
 * @details The rank is part of the type while every extent remains a runtime
 * value. Metadata is stored directly in the object and never allocates.
 */
template <std::size_t Rank>
class FixedRankExtents
{
    static_assert(Rank != kDynamicTensorRank, "FixedRankExtents requires a static rank");

public:
    using value_type = std::size_t;
    using container_type = std::array<value_type, Rank>;
    using const_iterator = typename container_type::const_iterator;

    FixedRankExtents()
        : mLogicalSize(checkedLogicalSize(mExtents))
    {
    }

    FixedRankExtents(std::initializer_list<value_type> extents)
        : FixedRankExtents(extents.begin(), extents.end())
    {
    }

    explicit FixedRankExtents(const container_type& extents)
        : mExtents(extents)
        , mLogicalSize(checkedLogicalSize(mExtents))
    {
    }

    explicit FixedRankExtents(container_type&& extents)
        : mExtents(std::move(extents))
        , mLogicalSize(checkedLogicalSize(mExtents))
    {
    }

    explicit FixedRankExtents(const std::vector<value_type>& extents)
        : FixedRankExtents(extents.begin(), extents.end())
    {
    }

    template <std::input_iterator Iterator>
    FixedRankExtents(Iterator first, Iterator last)
    {
        std::size_t count = 0;
        for (; first != last; ++first)
        {
            if (count == Rank)
            {
                throw std::invalid_argument("Ranked Tensor extents must match the static rank");
            }
            mExtents[count] = static_cast<value_type>(*first);
            ++count;
        }
        if (count != Rank)
        {
            throw std::invalid_argument("Ranked Tensor extents must match the static rank");
        }
        mLogicalSize = checkedLogicalSize(mExtents);
    }

    [[nodiscard]] static FixedRankExtents fromSpan(std::span<const value_type> extents)
    {
        return FixedRankExtents(extents.begin(), extents.end());
    }

    [[nodiscard]] static constexpr std::size_t rank() noexcept { return Rank; }
    [[nodiscard]] std::size_t logicalSize() const noexcept { return mLogicalSize; }
    [[nodiscard]] bool hasZeroExtent() const noexcept { return mLogicalSize == 0; }
    [[nodiscard]] const container_type& values() const noexcept { return mExtents; }
    [[nodiscard]] value_type operator[](std::size_t axis) const noexcept { return mExtents[axis]; }
    [[nodiscard]] value_type at(std::size_t axis) const { return mExtents.at(axis); }
    [[nodiscard]] const_iterator begin() const noexcept { return mExtents.begin(); }
    [[nodiscard]] const_iterator end() const noexcept { return mExtents.end(); }

    friend bool operator==(const FixedRankExtents&, const FixedRankExtents&) = default;

private:
    container_type mExtents{};
    std::size_t mLogicalSize = Rank == 0 ? 1 : 0;
};

template <std::size_t Rank>
using TensorExtentsFor = std::conditional_t<Rank == kDynamicTensorRank, DynamicExtents, FixedRankExtents<Rank>>;

template <std::size_t Rank>
using TensorStridesFor =
    std::conditional_t<Rank == kDynamicTensorRank, TensorStrides, std::array<std::ptrdiff_t, Rank>>;

} // namespace tensor_detail

template <std::size_t Rank>
[[nodiscard]] constexpr bool operator==(const DynamicExtents& left,
                                        const tensor_detail::FixedRankExtents<Rank>& right) noexcept
{
    if (left.rank() != Rank)
    {
        return false;
    }
    for (std::size_t axis = 0; axis < Rank; ++axis)
    {
        if (left[axis] != right[axis])
        {
            return false;
        }
    }
    return true;
}

template <std::size_t Rank>
[[nodiscard]] constexpr bool operator==(const tensor_detail::FixedRankExtents<Rank>& left,
                                        const DynamicExtents& right) noexcept
{
    return right == left;
}

template <std::size_t LeftRank, std::size_t RightRank>
    requires(LeftRank != RightRank)
[[nodiscard]] constexpr bool operator==(
    const tensor_detail::FixedRankExtents<LeftRank>&,
    const tensor_detail::FixedRankExtents<RightRank>&) noexcept
{
    return false;
}

inline std::size_t normalizeAxis(TensorAxis axis, std::size_t rank)
{
    if (rank > static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max()))
    {
        throw std::overflow_error("Tensor rank cannot be represented as ptrdiff_t for axis normalization");
    }
    const auto signedRank = static_cast<std::ptrdiff_t>(rank);
    const auto normalized = axis < 0 ? axis + signedRank : axis;
    if (normalized < 0 || normalized >= signedRank)
    {
        throw std::out_of_range("Tensor axis is outside the layout rank");
    }
    return static_cast<std::size_t>(normalized);
}

inline std::vector<std::size_t> normalizeAxes(const std::vector<TensorAxis>& axes, std::size_t rank)
{
    std::vector<std::size_t> result;
    result.reserve(axes.size());
    for (const auto axis : axes)
    {
        const auto normalized = normalizeAxis(axis, rank);
        if (std::find(result.begin(), result.end(), normalized) != result.end())
        {
            throw std::invalid_argument("Tensor axes contain a duplicate axis");
        }
        result.push_back(normalized);
    }
    return result;
}

template <std::size_t AxisCount>
[[nodiscard]] std::array<std::size_t, AxisCount>
normalizeAxes(const std::array<TensorAxis, AxisCount>& axes, std::size_t rank)
{
    std::array<std::size_t, AxisCount> result{};
    for (std::size_t index = 0; index < AxisCount; ++index)
    {
        const auto normalized = normalizeAxis(axes[index], rank);
        if (std::find(result.begin(), result.begin() + static_cast<std::ptrdiff_t>(index), normalized) !=
            result.begin() + static_cast<std::ptrdiff_t>(index))
        {
            throw std::invalid_argument("Tensor axes contain a duplicate axis");
        }
        result[index] = normalized;
    }
    return result;
}

} // namespace fat_p
