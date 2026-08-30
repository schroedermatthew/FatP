/** @file test_TensorView.cpp @brief Borrowed and shared Tensor view conformance tests. */

/*
FATP_META:
  meta_version: 1
  component: TensorView
  file_role: test
  path: components/Tensor/tests/test_TensorView.cpp
  namespace: fat_p::testing::tensor_view
  layer: Testing
  summary: "Borrowed/shared lifetime, constness, transform, and external mapping tests."
  api_stability: in_work
  related:
    docs:
      - components/Tensor/docs/Design Note - Tensor Semantic Contract.md
      - components/Tensor/docs/Design Note - Tensor Architecture Additions Plan.md
    headers:
      - include/fat_p/TensorView.h
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
#include "TensorView.h"

#include <cstddef>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace fat_p::testing::tensor_view
{

template <typename Owner>
concept RvalueBorrow = requires(Owner&& owner) { std::move(owner).asView(); };

static_assert(!RvalueBorrow<Tensor<int>>);
static_assert(std::same_as<decltype(std::declval<const Tensor<int>&>().rowView(0)), TensorView<const int>>);
static_assert(std::same_as<decltype(std::declval<Tensor<int>&>().rowView(0)), TensorView<int>>);
static_assert(std::same_as<decltype(std::declval<const Tensor<int>&>().broadcastView(DynamicExtents{1})),
                           TensorView<const int>>);

FATP_TEST_CASE(external_mapping_validation)
{
    int values[]{1, 2, 3, 4, 5, 6};
    auto reversed = TensorView<int>::borrow(values, TensorLayout(6, 2, DynamicExtents{2, 3}, TensorStrides{3, -1}));
    FATP_ASSERT_EQ(reversed(0, 0), 3, "Negative-stride view should start at its logical origin");
    FATP_ASSERT_EQ(reversed(1, 2), 4, "Negative-stride view should remain inside validated storage");
    FATP_ASSERT_THROWS(TensorView<int>::borrow(nullptr, TensorLayout::contiguous(DynamicExtents{1})),
                       std::invalid_argument, "Nonempty external view should reject a null base");
    return true;
}

FATP_TEST_CASE(metadata_transforms)
{
    Tensor<int> owner({3, 4});
    std::iota(owner.begin(), owner.end(), 1);
    auto interior = owner.sliceView({1, 1}, {3, 4});
    FATP_ASSERT_TRUE(interior.extents() == DynamicExtents({2, 3}), "Slice should compute half-open extents");
    FATP_ASSERT_EQ(interior(0, 0), 6, "Slice should advance its logical origin");
    FATP_ASSERT_EQ(interior(1, 2), 12, "Slice should preserve parent row stride");
    FATP_ASSERT_THROWS(interior.data(), std::logic_error,
                       "Non-contiguous slice should reject contiguous data access");

    auto transposed = owner.transposeView();
    FATP_ASSERT_TRUE(transposed.extents() == DynamicExtents({4, 3}), "Transpose should exchange extents");
    FATP_ASSERT_EQ(transposed(3, 2), 12, "Transpose should exchange strides without copying");

    auto reshaped = owner.reshapeView(DynamicExtents{2, 6});
    FATP_ASSERT_EQ(reshaped(1, 5), 12, "Contiguous reshape should preserve logical order");
    FATP_ASSERT_THROWS(transposed.reshapeView(DynamicExtents{2, 6}), std::invalid_argument,
                       "Non-contiguous reshape view should reject implicit materialization");
    return true;
}

FATP_TEST_CASE(readonly_broadcast)
{
    Tensor<int> row({1, 3});
    row[0] = 2;
    row[1] = 4;
    row[2] = 6;
    const auto broadcast = row.broadcastView(DynamicExtents{2, 3});
    FATP_ASSERT_TRUE(broadcast.layout().kind() == TensorLayoutKind::Broadcast,
                     "Expanded singleton axis should classify as broadcast");
    FATP_ASSERT_EQ(broadcast(1, 2), 6, "Broadcast should alias the singleton source row");
    FATP_ASSERT_FALSE(WritableTensor<decltype(broadcast)>, "Broadcast view should be read-only by element type");
    return true;
}

FATP_TEST_CASE(shared_and_borrowed_lifetime)
{
    TensorView<int> borrowed;
    SharedTensorView<int> shared;
    {
        Tensor<int> owner({2}, 17);
        borrowed = owner.asView();
        shared = owner.asSharedView();
    }
    FATP_ASSERT_EQ(shared[0], 17, "Shared view should retain element storage");
#ifndef NDEBUG
    FATP_ASSERT_THROWS(borrowed[0], std::runtime_error,
                       "Debug borrowed view should diagnose owner destruction");
#endif
    auto sharedSlice = shared.sliceView({0}, {1});
    shared = SharedTensorView<int>{};
    FATP_ASSERT_EQ(sharedSlice[0], 17, "Derived shared view should retain the same lifetime handle");
    return true;
}

FATP_TEST_CASE(iterator_identity_and_writable_layout_guards)
{
    Tensor<int> owner({3});
    std::iota(owner.begin(), owner.end(), 1);
    const auto first = owner.asView();
    const auto copy = first;
    FATP_ASSERT_TRUE(first.begin() == copy.begin(),
                     "Copied views over the same mapping should share an iterator equality domain");
    FATP_ASSERT_TRUE(first.end() == copy.end(),
                     "Copied views should expose mutually reachable end iterators");
    FATP_ASSERT_TRUE(std::vector<int>(first.begin(), copy.end()) == std::vector<int>({1, 2, 3}),
                     "A begin/end pair from copied views should terminate and preserve logical order");
    FATP_ASSERT_TRUE(owner.asView().begin() == owner.asView().begin(),
                     "Equivalent temporary views should produce equal begin iterators");
    FATP_ASSERT_TRUE(owner.asView().end() == owner.asView().end(),
                     "Equivalent temporary views should produce equal end iterators");

    Tensor<int> empty({0});
    const auto emptyFirst = empty.asView();
    const auto emptyCopy = emptyFirst;
    FATP_ASSERT_TRUE(emptyFirst.begin() == emptyCopy.end(),
                     "Copied empty views should form an immediately terminated range");

    int cell = 7;
    const TensorLayout broadcast(1, 0, DynamicExtents{4}, TensorStrides{0});
    FATP_ASSERT_THROWS(TensorView<int>::borrow(&cell, broadcast), std::invalid_argument,
                       "Mutable external views should reject broadcast aliasing at construction");
    const auto readonlyBroadcast = TensorView<const int>::borrow(&cell, broadcast);
    FATP_ASSERT_EQ(readonlyBroadcast[3], 7, "Read-only external broadcast mappings should remain representable");

    int overlappingStorage[]{1, 2, 3};
    const TensorLayout overlap(3, 0, DynamicExtents{2, 2}, TensorStrides{1, 1});
    FATP_ASSERT_THROWS(TensorView<int>::borrow(overlappingStorage, overlap), std::invalid_argument,
                       "Mutable external views should reject overlapping mappings at construction");
    const auto lifetime = std::make_shared<int>(1);
    FATP_ASSERT_THROWS(SharedTensorView<int>::share(lifetime, overlappingStorage, overlap), std::invalid_argument,
                       "Mutable shared views should enforce the same injectivity boundary");
    return true;
}

} // namespace fat_p::testing::tensor_view

namespace fat_p::testing
{

bool test_TensorView()
{
    FATP_PRINT_HEADER(TENSOR VIEW)
    TestRunner runner;
    FATP_RUN_TEST_NS(runner, tensor_view, external_mapping_validation);
    FATP_RUN_TEST_NS(runner, tensor_view, metadata_transforms);
    FATP_RUN_TEST_NS(runner, tensor_view, readonly_broadcast);
    FATP_RUN_TEST_NS(runner, tensor_view, shared_and_borrowed_lifetime);
    FATP_RUN_TEST_NS(runner, tensor_view, iterator_identity_and_writable_layout_guards);
    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_TensorView() ? 0 : 1;
}
#endif
