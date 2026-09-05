/** @file test_TensorInterop.cpp @brief Dependency-light Tensor interop tests. */

/*
FATP_META:
  meta_version: 1
  component: TensorInterop
  file_role: test
  path: components/Tensor/tests/test_TensorInterop.cpp
  namespace: fat_p::testing::tensor_interop
  layer: Testing
  summary: "Span, strided descriptor, mdspan, and fixed/dynamic conversion tests."
  api_stability: in_work
  related:
    docs:
      - components/Tensor/docs/Design Note - Tensor Architecture Additions Plan.md
    headers:
      - include/fat_p/TensorInterop.h
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: codex
    mode: manual
*/

#include "FatPTest.h"
#include "TensorInterop.h"

#include <cstddef>
#include <concepts>
#include <numeric>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace fat_p::testing::tensor_interop
{

template <typename T>
concept CanDescribeTemporary = requires(T&& value) { describeTensor(std::move(value)); };

template <typename T>
concept CanSpanTemporary = requires(T&& value) { contiguousSpan(std::move(value)); };

static_assert(!CanDescribeTemporary<Tensor<int>>);
static_assert(!CanSpanTemporary<Tensor<int>>);

FATP_TEST_CASE(contiguous_span_contract)
{
    Tensor<int> owner({2, 4});
    std::iota(owner.begin(), owner.end(), 1);
    auto ownerSpan = contiguousSpan(owner);
    static_assert(std::same_as<decltype(ownerSpan), std::span<int>>);
    FATP_ASSERT_EQ(ownerSpan.size(), std::size_t{8}, "Owner span should expose every contiguous value");
    ownerSpan[7] = 80;
    FATP_ASSERT_EQ(owner(1, 3), 80, "Mutable span should alias owner storage");

    const Tensor<int>& constant = owner;
    const auto constSpan = contiguousSpan(constant);
    static_assert(std::same_as<decltype(constSpan), const std::span<const int>>);
    FATP_ASSERT_EQ(constSpan[7], 80, "Const span should preserve element constness");

    const auto row = owner.rowView(1);
    const auto rowSpan = contiguousSpan(row);
    FATP_ASSERT_EQ(rowSpan.front(), 5, "Contiguous subviews should expose their logical origin");
    const auto transposed = owner.transposeView();
    FATP_ASSERT_THROWS(contiguousSpan(transposed), std::logic_error,
                       "Non-contiguous mappings should not masquerade as spans");

    Tensor<int> empty({0});
    FATP_ASSERT_TRUE(contiguousSpan(empty).empty(), "Empty contiguous spans should be representable");
    Tensor<int> scalar({}, 11);
    const auto scalarSpan = contiguousSpan(scalar);
    FATP_ASSERT_EQ(scalarSpan.size(), std::size_t{1}, "A rank-zero scalar should expose one span element");
    FATP_ASSERT_EQ(scalarSpan.front(), 11, "Scalar span interop should preserve its value");

    Tensor<int> line({3}, 5);
    const auto emptySlice = line.sliceView({Slice{0, 0}});
    FATP_ASSERT_TRUE(contiguousSpan(emptySlice).empty(),
                     "An empty slice of a live owner should remain valid span interop");
    return true;
}

FATP_TEST_CASE(strided_descriptor_roundtrip)
{
    Tensor<int> owner({2, 3});
    std::iota(owner.begin(), owner.end(), 1);
    auto reversed = owner.sliceView({All, Slice{std::nullopt, std::nullopt, -1}});
    const auto descriptor = describeTensor(reversed);
    static_assert(std::same_as<typename decltype(descriptor)::element_type, int>);
    FATP_ASSERT_TRUE(descriptor.extents == DynamicExtents({2, 3}), "Descriptor should own extents metadata");
    FATP_ASSERT_TRUE(descriptor.strides == TensorStrides({3, -1}), "Descriptor should preserve signed strides");
    FATP_ASSERT_EQ(descriptor.originOffset, std::ptrdiff_t{2}, "Descriptor should retain storage-relative origin");

    auto borrowed = descriptor.borrow();
    borrowed(1, 0) = 60;
    FATP_ASSERT_EQ(owner(1, 2), 60, "Descriptor borrowing should reconstruct the exact mapping");

    const auto constWrapperDescriptor = describeTensor(std::as_const(reversed));
    static_assert(std::same_as<typename decltype(constWrapperDescriptor)::element_type, int>);
    const auto constWrapperBorrow = constWrapperDescriptor.borrow();
    static_assert(std::same_as<decltype(constWrapperBorrow), const TensorView<int>>);
    FATP_ASSERT_EQ(constWrapperBorrow(0, 2), 1, "Const view wrappers preserve logical values and mutability");

    StridedTensorDescriptor<int> sharedDescriptor;
    {
        Tensor<int> sharedOwner({1}, 13);
        auto sharedView = sharedOwner.asSharedView();
        sharedDescriptor = describeTensor(sharedView);
    }
    const auto retainedBorrow = sharedDescriptor.borrow();
    FATP_ASSERT_EQ(retainedBorrow[0], 13,
                   "Descriptors made from shared views should retain their shared storage lifetime");

#ifndef NDEBUG
    StridedTensorDescriptor<int> expiredDescriptor;
    {
        Tensor<int> temporaryOwner({1}, 7);
        expiredDescriptor = describeTensor(temporaryOwner);
    }
    const auto expiredBorrow = expiredDescriptor.borrow();
    FATP_ASSERT_THROWS(expiredBorrow[0], std::runtime_error,
                       "Descriptor round-trips should preserve debug lifetime tracking");
#endif
    return true;
}

FATP_TEST_CASE(descriptor_storage_validation)
{
    StridedTensorDescriptor<int> dynamic;
    dynamic.storageLength = 1;
    dynamic.extents = DynamicExtents{1};
    dynamic.strides = TensorStrides{1};
    FATP_ASSERT_THROWS(dynamic.borrow(), std::invalid_argument,
                       "A nonempty dynamic descriptor must reject null storage");

    StridedTensorDescriptor<const int> readonly;
    readonly.storageLength = 1;
    readonly.extents = DynamicExtents{1};
    readonly.strides = TensorStrides{1};
    FATP_ASSERT_THROWS(readonly.borrow(), std::invalid_argument,
                       "A nonempty read-only descriptor must reject null storage");

    RankedStridedTensorDescriptor<int, 1> ranked;
    ranked.storageLength = 1;
    ranked.extents = RankedExtents<1>{1};
    ranked.strides = {1};
    FATP_ASSERT_THROWS(ranked.borrow(), std::invalid_argument,
                       "A nonempty ranked descriptor must reject null storage");

    RankedStridedTensorDescriptor<const int, 1> rankedReadonly;
    rankedReadonly.storageLength = 1;
    rankedReadonly.extents = RankedExtents<1>{1};
    rankedReadonly.strides = {1};
    FATP_ASSERT_THROWS(rankedReadonly.borrow(), std::invalid_argument,
                       "A nonempty read-only ranked descriptor must reject null storage");

    StridedTensorDescriptor<int> empty;
    empty.extents = DynamicExtents{0};
    empty.strides = TensorStrides{0};
    const auto emptyBorrow = empty.borrow();
    FATP_ASSERT_TRUE(emptyBorrow.empty(), "An empty descriptor may use null storage");

    RankedStridedTensorDescriptor<int, 1> rankedEmpty;
    rankedEmpty.extents = RankedExtents<1>{0};
    rankedEmpty.strides = {0};
    const auto rankedEmptyBorrow = rankedEmpty.borrow();
    FATP_ASSERT_TRUE(rankedEmptyBorrow.empty(), "An empty ranked descriptor may use null storage");
    return true;
}

FATP_TEST_CASE(static_dynamic_conversion)
{
    StaticTensor<int, Shape<2, 3>> fixed{1, 2, 3, 4, 5, 6};
    const auto dynamic = toTensor(fixed);
    FATP_ASSERT_TRUE(dynamic.extents() == DynamicExtents({2, 3}), "Static shape should become dynamic extents");
    FATP_ASSERT_EQ(dynamic(1, 2), 6, "Static-to-dynamic conversion should preserve row-major values");

    const auto roundtrip = toStaticTensor<Shape<2, 3>>(dynamic.transposeView().transposeView());
    FATP_ASSERT_EQ(roundtrip.at(1, 2), 6, "View-to-static conversion should follow logical order");
    FATP_ASSERT_THROWS((toStaticTensor<Shape<3, 2>>(dynamic)), std::invalid_argument,
                       "Static conversion should validate every extent");

    StaticTensor<int, Shape<>> fixedScalar{9};
    const auto dynamicScalar = toTensor(fixedScalar);
    FATP_ASSERT_EQ(dynamicScalar.rank(), std::size_t{0}, "Static rank-zero shape should remain a dynamic scalar");
    const auto scalarRoundtrip = toStaticTensor<Shape<>>(dynamicScalar);
    FATP_ASSERT_EQ(scalarRoundtrip.at(), 9, "Rank-zero conversion should round-trip");
    return true;
}

FATP_TEST_CASE(interop_element_constness)
{
    const auto check = []<typename Owner>(Owner& owner) {
        const auto view = owner.asView();
        const auto shared = owner.asSharedView();
        const auto readonly = std::as_const(owner).asConstView();
        auto descriptor = describeTensor(view);
        auto sharedDescriptor = describeTensor(shared);
        auto ownerDescriptor = describeTensor(std::as_const(owner));
        auto readonlyDescriptor = describeTensor(readonly);
        static_assert(std::same_as<typename decltype(descriptor)::element_type, int>);
        static_assert(std::same_as<typename decltype(sharedDescriptor)::element_type, int>);
        static_assert(std::same_as<typename decltype(ownerDescriptor)::element_type, const int>);
        static_assert(std::same_as<typename decltype(readonlyDescriptor)::element_type, const int>);
        auto span = contiguousSpan(view);
        auto sharedSpan = contiguousSpan(shared);
        static_assert(std::same_as<typename decltype(span)::element_type, int>);
        static_assert(std::same_as<typename decltype(sharedSpan)::element_type, int>);
        static_assert(std::same_as<typename decltype(contiguousSpan(std::as_const(owner)))::element_type, const int>);
        static_assert(std::same_as<typename decltype(contiguousSpan(readonly))::element_type, const int>);
        descriptor.borrow()[0] = 3;
        sharedSpan[1] = 4;
        FATP_ASSERT_EQ(owner[0], 3, "Const wrapper descriptor permits writes to mutable elements");
        FATP_ASSERT_EQ(owner[1], 4, "Const shared wrapper span aliases mutable elements");
#if FATP_HAS_MDSPAN
        auto mapped = asMdspan<1>(view);
        auto sharedMapped = asMdspan<1>(shared);
        static_assert(std::same_as<typename decltype(mapped)::element_type, int>);
        static_assert(std::same_as<typename decltype(sharedMapped)::element_type, int>);
        static_assert(std::same_as<typename decltype(asMdspan<1>(std::as_const(owner)))::element_type, const int>);
        static_assert(std::same_as<typename decltype(asMdspan<1>(readonly))::element_type, const int>);
        mapped[0] = 5;
        FATP_ASSERT_EQ(owner[0], 5, "Const wrapper mdspan aliases mutable elements");
#endif
        return true;
    };
    Tensor<int> dynamic({2}, 1);
    RankedTensor<int, 1> ranked({2}, 1);
    FATP_ASSERT_TRUE(check(dynamic), "Dynamic interop constness");
    FATP_ASSERT_TRUE(check(ranked), "Ranked interop constness");
    return true;
}

#if FATP_HAS_MDSPAN
template <std::size_t Rank, typename T>
concept CanMdspanTemporary = requires(T&& value) { asMdspan<Rank>(std::move(value)); };

static_assert(!CanMdspanTemporary<2, Tensor<int>>);

FATP_TEST_CASE(mdspan_mapping)
{
    Tensor<int> owner({2, 3});
    std::iota(owner.begin(), owner.end(), 1);
    auto mapping = asMdspan<2>(owner);
    const auto mappedValue = mapping[1, 2];
    FATP_ASSERT_EQ(mappedValue, 6, "mdspan should preserve extents and strides");
    mapping[0, 1] = 20;
    FATP_ASSERT_EQ(owner(0, 1), 20, "Mutable mdspan should alias Tensor storage");
    const auto reversed = owner.sliceView({All, Slice{std::nullopt, std::nullopt, -1}});
    FATP_ASSERT_THROWS(asMdspan<2>(reversed), std::invalid_argument,
                       "layout_stride interop should reject negative strides");

    Tensor<int> empty({0, 3});
    const auto emptyMapping = asMdspan<2>(empty);
    FATP_ASSERT_EQ(emptyMapping.extent(0), std::size_t{0}, "mdspan should preserve zero extents");
    FATP_ASSERT_TRUE(emptyMapping.mapping().stride(0) > 0 && emptyMapping.mapping().stride(1) > 0,
                     "layout_stride should receive valid positive placeholder strides for empty axes");
    return true;
}

FATP_TEST_CASE(mdspan_mapping_preconditions)
{
    int storage[8]{0, 1, 2, 3, 4, 5, 6, 7};
    const TensorLayout interleaved(8, 0, DynamicExtents{3, 2}, TensorStrides{2, 3});
    auto writable = TensorView<int>::borrow(storage, interleaved);
    const auto readonly = TensorView<const int>::borrow(storage, interleaved);
    FATP_ASSERT_TRUE(interleaved.isInjective(), "Interleaved example is a valid FatP mapping");
    FATP_ASSERT_THROWS(asMdspan<2>(writable), std::invalid_argument,
                       "Mutable adapter rejects injective mappings outside layout_stride's contract");
    FATP_ASSERT_THROWS(asMdspan<2>(readonly), std::invalid_argument,
                       "Const adapter checks the same mapping precondition");
    const auto ranked = asRankedView<2>(writable);
    FATP_ASSERT_THROWS(asMdspan<2>(ranked), std::invalid_argument, "Ranked adapter rejects interleaving too");

    // An unused singleton stride must not reject an otherwise representable mapping.
    const auto singleton = TensorView<int>::borrow(
        storage, TensorLayout(8, 0, DynamicExtents{3, 1}, TensorStrides{2, 3}));
    auto singletonMapped = asMdspan<2>(singleton);
    const auto last = singletonMapped[2, 0];
    FATP_ASSERT_EQ(last, 4, "Normalizing singleton strides preserves all element addresses");

    for (const auto& extents : {DynamicExtents{0, 2, 3}, DynamicExtents{2, 0, 3}, DynamicExtents{2, 3, 0}})
    {
        Tensor<int> empty(extents);
        const auto mapping = asMdspan<3>(empty).mapping();
        std::array<std::size_t, 3> order{0, 1, 2};
        bool valid = false;
        do
        {
            valid = mapping.stride(order[1]) >= mapping.stride(order[0]) * extents[order[0]] &&
                    mapping.stride(order[2]) >= mapping.stride(order[1]) * extents[order[1]];
        } while (!valid && std::next_permutation(order.begin(), order.end()));
        FATP_ASSERT_TRUE(valid, "Empty placeholders meet the permutation precondition at every zero-axis position");
        FATP_ASSERT_EQ(mapping.required_span_size(), std::size_t{0}, "Empty mdspan spans no storage");
    }
    Tensor<int> scalar(DynamicExtents{}, 9);
    const auto scalarMapping = asMdspan<0>(scalar);
    FATP_ASSERT_EQ(scalarMapping.mapping().required_span_size(), std::size_t{1}, "Rank-zero mapping remains scalar");
    return true;
}
#endif

} // namespace fat_p::testing::tensor_interop

namespace fat_p::testing
{

bool test_TensorInterop()
{
    FATP_PRINT_HEADER(TENSOR INTEROP)
    TestRunner runner;
    FATP_RUN_TEST_NS(runner, tensor_interop, contiguous_span_contract);
    FATP_RUN_TEST_NS(runner, tensor_interop, strided_descriptor_roundtrip);
    FATP_RUN_TEST_NS(runner, tensor_interop, descriptor_storage_validation);
    FATP_RUN_TEST_NS(runner, tensor_interop, static_dynamic_conversion);
    FATP_RUN_TEST_NS(runner, tensor_interop, interop_element_constness);
#if FATP_HAS_MDSPAN
    FATP_RUN_TEST_NS(runner, tensor_interop, mdspan_mapping);
    FATP_RUN_TEST_NS(runner, tensor_interop, mdspan_mapping_preconditions);
#endif
    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_TensorInterop() ? 0 : 1;
}
#endif
