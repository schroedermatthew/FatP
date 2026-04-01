#pragma once

// CArrayView.h
//
// A non-owning, zero-overhead view into fixed-size multidimensional C arrays.
// Deduces rank and dimensions from the array type via CTAD.
// Uses concepts to enforce that the number of indices matches the rank.
//
// Usage:
//     int arr[3][4][5] = {};
//     CArrayView v(arr);          // deduces CArrayView<int, 3, 3, 4, 5>
//     v(2, 1, 0) = 42;
//
//     const double m[8][8] = {};
//     auto cv = makeCArrayView(m); // CArrayView<const double, 2, 8, 8>
//
// Note: constexpr element access works for rank-1 arrays.  For rank > 1,
// the language does not permit pointer arithmetic across sub-array boundaries
// in constant expressions, so operator() is usable only at runtime.

#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

namespace ulib
{

// =====================================================================
// Forward declaration
// =====================================================================
template<typename T, std::size_t Rank, std::size_t... Dims>
    requires (sizeof...(Dims) == Rank)
class CArrayView;

// =====================================================================
// detail
// =====================================================================
namespace detail
{

// ----- flatPtr: follow nested [0] subscripts to the first scalar -----
//
// IMPORTANT -- layout assumption:
// For rank > 1 the returned pointer is used with arithmetic that spans
// across sub-array boundaries (e.g. treating int[3][4] as int[12]).
// Strictly speaking the C++ object model does not guarantee this;
// pointer arithmetic past the end of the innermost array is undefined
// behaviour per [expr.add].  In practice every major compiler and ABI
// lays out multidimensional C arrays contiguously and this is universally
// relied upon (std::mdspan implementations do the same thing).
//
// C++23's std::start_lifetime_as could be used to make this well-defined
// by starting an implicit-lifetime array<T, N> over the storage; this is
// left as a future improvement.  Until then callers should be aware of
// the theoretical UB.
template<typename T>
constexpr auto flatPtr(T& val) noexcept
    -> std::remove_all_extents_t<T>*
{
    if constexpr (std::is_array_v<T>)
        return flatPtr(val[0]);
    else
        return std::addressof(val);
}

// ----- BuildArrayT: reconstruct the C array type from element + dims -----
//       BuildArrayT<int, 3, 4, 5>  ==  int[3][4][5]
template<typename T, std::size_t... Dims>
struct BuildArrayType;

template<typename T>
struct BuildArrayType<T>
{
    using type = T;
};

template<typename T, std::size_t First, std::size_t... Rest>
struct BuildArrayType<T, First, Rest...>
{
    using type = typename BuildArrayType<T, Rest...>::type[First];
};

template<typename T, std::size_t... Dims>
using BuildArrayT = typename BuildArrayType<T, Dims...>::type;

// ----- ExtractDims: peel dimensions off a C array type -----
//       CArrayViewFor<int[3][4][5]>  ==  CArrayView<int, 3, 3, 4, 5>
template<typename Array, std::size_t... Accumulated>
struct ExtractDims
{
    using type = CArrayView<Array, sizeof...(Accumulated), Accumulated...>;
};

template<typename T, std::size_t N, std::size_t... Accumulated>
struct ExtractDims<T[N], Accumulated...>
    : ExtractDims<T, Accumulated..., N>
{};

template<typename Array>
using CArrayViewFor = typename ExtractDims<Array>::type;

} // namespace detail

// =====================================================================
// CArrayView
// =====================================================================
template<typename T, std::size_t Rank, std::size_t... Dims>
    requires (sizeof...(Dims) == Rank)
class CArrayView
{
    static_assert(Rank > 0,
        "CArrayView requires at least one dimension.  "
        "If you reached this via CTAD on an array with more than 8 dimensions, "
        "use makeCArrayView() instead.");
    static_assert(((Dims > 0) && ...),
        "All dimensions must be non-zero");

public:
    // ----- type aliases -----
    using element_type  = T;
    using value_type    = std::remove_cv_t<T>;
    using pointer       = T*;
    using const_pointer = const T*;
    using reference     = T&;
    using size_type     = std::size_t;
    using rank_type     = std::size_t;

    /// The C array type that this view maps onto.
    using array_type = detail::BuildArrayT<element_type, Dims...>;

    // ----- construction -----

    CArrayView() = delete;

    /// Construct from a matching C array.
    constexpr CArrayView(array_type& arr) noexcept
        : mData{detail::flatPtr(arr)}
    {}

    /// Construct from a raw pointer (caller guarantees at least size() elements).
    /// Static factory provides explicit opt-in for raw-pointer construction,
    /// which is inherently unsafe (no size verification).
    static constexpr CArrayView fromPointer(pointer p) noexcept
    {
        return CArrayView{p, CtorTag{}};
    }

    constexpr CArrayView(const CArrayView&) noexcept            = default;
    constexpr CArrayView& operator=(const CArrayView&) noexcept = default;

    /// Converting constructor: CArrayView<T,...> -> CArrayView<const T,...>
    template<typename U>
        requires (std::is_same_v<T, const U> && !std::is_const_v<U>)
    constexpr CArrayView(CArrayView<U, Rank, Dims...> other) noexcept
        : mData{other.data()}
    {}

    // ----- static queries -----

    static constexpr rank_type rank() noexcept { return Rank; }

    static constexpr size_type extent(rank_type r) noexcept
    {
        assert(r < Rank);
        return kDims[r];
    }

    /// Product of all dimensions.
    static constexpr size_type size() noexcept
    {
        return (Dims * ...);
    }

    /// Row-major stride for dimension r.
    static constexpr size_type stride(rank_type r) noexcept
    {
        assert(r < Rank);
        return kStrides[r];
    }

    /// Always false -- zero-extent dimensions are rejected at compile time.
    static constexpr bool empty() noexcept { return false; }

    // ----- element access -----

    /// Multi-index access.  Number of indices must equal Rank (concept-enforced).
    /// Requires std::integral to reject pointers and floating-point types;
    /// signed negative values are caught by the bounds check in debug builds.
    ///
    /// Note: for rank > 1 the underlying pointer arithmetic crosses sub-array
    /// boundaries.  See the comment on detail::flatPtr for discussion.
    template<std::integral... Indices>
        requires (sizeof...(Indices) == Rank
                  && (!std::is_same_v<std::remove_cvref_t<Indices>, bool> && ...))
    constexpr reference operator()(Indices... indices) const noexcept
    {
        assert(((indices >= Indices{0}) && ...) &&
               "CArrayView: negative index");
        const std::array<size_type, Rank> idx{static_cast<size_type>(indices)...};
        checkBounds(idx);
        return mData[linearOffset(std::make_index_sequence<Rank>{}, idx)];
    }

    /// Array-index access for generic code that builds index packs
    /// programmatically rather than expanding parameter packs.
    constexpr reference operator[](const std::array<size_type, Rank>& idx) const noexcept
    {
        checkBounds(idx);
        return mData[linearOffset(std::make_index_sequence<Rank>{}, idx)];
    }

    // ----- observers -----

    constexpr pointer data() const noexcept { return mData; }

private:
    pointer mData;

    struct CtorTag {};
    constexpr CArrayView(pointer p, CtorTag) noexcept : mData{p} {}

    // ----- compile-time tables -----

    static constexpr std::array<size_type, Rank> kDims{Dims...};

    static constexpr std::array<size_type, Rank> makeStrides() noexcept
    {
        std::array<size_type, Rank> s{};
        s[Rank - 1] = 1;
        for (size_type i = Rank - 1; i > 0; --i)
            s[i - 1] = s[i] * kDims[i];
        return s;
    }

    static constexpr std::array<size_type, Rank> kStrides = makeStrides();

    // ----- offset computation -----

    template<std::size_t... Is>
    static constexpr size_type linearOffset(
        std::index_sequence<Is...>,
        const std::array<size_type, Rank>& idx) noexcept
    {
        return ((idx[Is] * kStrides[Is]) + ...);
    }

    static constexpr void checkBounds(
        [[maybe_unused]] const std::array<size_type, Rank>& idx) noexcept
    {
        for ([[maybe_unused]] size_type i = 0; i < Rank; ++i)
            assert(idx[i] < kDims[i] && "CArrayView: index out of bounds");
    }
};

// =====================================================================
// CTAD -- explicit deduction guides for ranks 1 through 8
//
// The !is_array_v<T> constraint ensures only the guide matching the
// exact rank fires (e.g. int[2][3] won't match the rank-1 guide with
// T deduced as int[3]).
// =====================================================================

template<typename T, std::size_t A>
    requires (!std::is_array_v<T>)
CArrayView(T(&)[A])
    -> CArrayView<T, 1, A>;

template<typename T, std::size_t A, std::size_t B>
    requires (!std::is_array_v<T>)
CArrayView(T(&)[A][B])
    -> CArrayView<T, 2, A, B>;

template<typename T, std::size_t A, std::size_t B, std::size_t C>
    requires (!std::is_array_v<T>)
CArrayView(T(&)[A][B][C])
    -> CArrayView<T, 3, A, B, C>;

template<typename T, std::size_t A, std::size_t B, std::size_t C, std::size_t D>
    requires (!std::is_array_v<T>)
CArrayView(T(&)[A][B][C][D])
    -> CArrayView<T, 4, A, B, C, D>;

template<typename T, std::size_t A, std::size_t B, std::size_t C, std::size_t D,
         std::size_t E>
    requires (!std::is_array_v<T>)
CArrayView(T(&)[A][B][C][D][E])
    -> CArrayView<T, 5, A, B, C, D, E>;

template<typename T, std::size_t A, std::size_t B, std::size_t C, std::size_t D,
         std::size_t E, std::size_t F>
    requires (!std::is_array_v<T>)
CArrayView(T(&)[A][B][C][D][E][F])
    -> CArrayView<T, 6, A, B, C, D, E, F>;

template<typename T, std::size_t A, std::size_t B, std::size_t C, std::size_t D,
         std::size_t E, std::size_t F, std::size_t G>
    requires (!std::is_array_v<T>)
CArrayView(T(&)[A][B][C][D][E][F][G])
    -> CArrayView<T, 7, A, B, C, D, E, F, G>;

template<typename T, std::size_t A, std::size_t B, std::size_t C, std::size_t D,
         std::size_t E, std::size_t F, std::size_t G, std::size_t H>
    requires (!std::is_array_v<T>)
CArrayView(T(&)[A][B][C][D][E][F][G][H])
    -> CArrayView<T, 8, A, B, C, D, E, F, G, H>;

// Fallback for arrays deeper than rank 8.  The guides above all require
// !is_array_v<T>, so when T is still an array type (meaning the source
// array has more than 8 dimensions) none of them match.  This catch-all
// fires instead and deduces Rank=0, which triggers the static_assert
// inside the class body with a helpful message directing to makeCArrayView().
template<typename T, std::size_t A>
    requires std::is_array_v<T>
CArrayView(T(&)[A])
    -> CArrayView<std::remove_all_extents_t<T>, 0>;

// =====================================================================
// Factory (works for any rank via the ExtractDims trait)
// =====================================================================
template<typename Array>
    requires std::is_array_v<Array>
constexpr auto makeCArrayView(Array& arr) noexcept
{
    return detail::CArrayViewFor<Array>{arr};
}

} // namespace ulib
