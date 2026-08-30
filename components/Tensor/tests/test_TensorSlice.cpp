/** @file test_TensorSlice.cpp @brief Extended Tensor slicing and permutation tests. */

/*
FATP_META:
  meta_version: 1
  component: TensorSlice
  file_role: test
  path: components/Tensor/tests/test_TensorSlice.cpp
  namespace: fat_p::testing::tensor_slice
  layer: Testing
  summary: "Negative-step, ellipsis, axis insertion/removal, and permutation tests."
  api_stability: in_work
  related:
    docs:
      - components/Tensor/docs/Design Note - Tensor Architecture Additions Plan.md
    headers:
      - include/fat_p/Tensor.h
      - include/fat_p/TensorSlice.h
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
#include "Tensor.h"
#include "TensorSlice.h"

#include <cstddef>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace fat_p::testing::tensor_slice
{

FATP_TEST_CASE(negative_and_bounded_slices)
{
    Tensor<int> owner({3, 4});
    std::iota(owner.begin(), owner.end(), 1);

    const auto reversed = owner.sliceView({All, Slice{std::nullopt, std::nullopt, -1}});
    FATP_ASSERT_TRUE(reversed.extents() == DynamicExtents({3, 4}), "Reverse should preserve extents");
    FATP_ASSERT_EQ(reversed(0, 0), 4, "Negative step should move the logical origin to the final column");
    FATP_ASSERT_EQ(reversed(2, 3), 9, "Negative step should traverse back through the final row");
    FATP_ASSERT_TRUE(reversed.strides() == TensorStrides({4, -1}), "Reverse should negate the selected stride");

    const auto bounded = owner.sliceView(
        {Slice{-2, std::nullopt, 1}, Slice{1, -1, 2}});
    FATP_ASSERT_TRUE(bounded.extents() == DynamicExtents({2, 1}), "Negative bounds should normalize per axis");
    FATP_ASSERT_EQ(bounded(0, 0), 6, "Bounded slice should address the normalized start");
    FATP_ASSERT_EQ(bounded(1, 0), 10, "Bounded slice should preserve the parent row stride");

    Tensor<int> singleton({1}, 5);
    const auto minimumStep = singleton.sliceView(
        {Slice{std::nullopt, std::nullopt, std::numeric_limits<std::ptrdiff_t>::min()}});
    FATP_ASSERT_EQ(minimumStep[0], 5, "The minimum signed step should remain valid for a singleton slice");
    FATP_ASSERT_EQ(minimumStep.strides()[0], std::numeric_limits<std::ptrdiff_t>::min(),
                   "Negative stride multiplication should preserve the representable ptrdiff_t minimum");
    return true;
}

FATP_TEST_CASE(ellipsis_newaxis_and_integer_axis)
{
    Tensor<int> owner({2, 3, 4});
    std::iota(owner.begin(), owner.end(), 1);
    const auto selected = owner.sliceView(
        {std::ptrdiff_t{-1}, NewAxis, Ellipsis, Slice{0, std::nullopt, 2}});
    FATP_ASSERT_TRUE(selected.extents() == DynamicExtents({1, 3, 2}),
                     "Integer indexing should remove one axis and NewAxis should insert one");
    FATP_ASSERT_EQ(selected(0, 0, 0), 13, "Negative integer index should select the final leading plane");
    FATP_ASSERT_EQ(selected(0, 2, 1), 23, "Ellipsis should expand over the remaining middle axis");

    const Tensor<int>& constant = owner;
    static_assert(std::same_as<decltype(constant.sliceView({All})), TensorView<const int>>);
    return true;
}

FATP_TEST_CASE(permute_squeeze_and_unsqueeze)
{
    Tensor<int> owner({2, 3, 4});
    std::iota(owner.begin(), owner.end(), 1);
    const auto permuted = owner.permuteView({2, 0, 1});
    FATP_ASSERT_TRUE(permuted.extents() == DynamicExtents({4, 2, 3}), "Permutation should reorder extents");
    FATP_ASSERT_EQ(permuted(3, 1, 2), owner(1, 2, 3), "Permutation should reorder coordinates only");

    Tensor<int> withSingletons({1, 2, 1, 3});
    std::iota(withSingletons.begin(), withSingletons.end(), 1);
    const auto squeezed = withSingletons.squeezeView();
    FATP_ASSERT_TRUE(squeezed.extents() == DynamicExtents({2, 3}), "Default squeeze should remove all singleton axes");
    FATP_ASSERT_EQ(squeezed(1, 2), 6, "Squeeze should preserve logical addressing");
    const auto restored = squeezed.unsqueezeView(-1).unsqueezeView(0);
    FATP_ASSERT_TRUE(restored.extents() == DynamicExtents({1, 2, 3, 1}),
                     "Unsqueeze should normalize positive and negative insertion axes");
    FATP_ASSERT_EQ(restored(0, 1, 2, 0), 6, "Unsqueeze should be metadata-only");
    return true;
}

FATP_TEST_CASE(empty_shared_and_errors)
{
    Tensor<int> empty({2, 0, 3});
    const auto reversedEmpty = empty.sliceView({All, All, Slice{std::nullopt, std::nullopt, -1}});
    FATP_ASSERT_TRUE(reversedEmpty.empty(), "Negative slicing should preserve zero-extent emptiness");

    Tensor<int> hugeEmpty(DynamicExtents{std::numeric_limits<std::size_t>::max(), 2, 0});
    const auto hugeIdentity = hugeEmpty.sliceView({All});
    FATP_ASSERT_TRUE(hugeIdentity.extents() == hugeEmpty.extents(),
                     "All should preserve a huge extent when another axis makes the mapping empty");
    const auto hugeFinalIndex = hugeEmpty.sliceView({std::ptrdiff_t{-1}, All, All});
    FATP_ASSERT_TRUE(hugeFinalIndex.extents() == DynamicExtents({2, 0}),
                     "Negative integer indexing should normalize huge empty extents without narrowing");

    SharedTensorView<int> survivor;
    {
        Tensor<int> owner({2, 2}, 7);
        survivor = owner.asSharedView().sliceView({std::ptrdiff_t{1}, All}).unsqueezeView(0);
    }
    FATP_ASSERT_TRUE(survivor.extents() == DynamicExtents({1, 2}), "Shared transforms should retain layout metadata");
    FATP_ASSERT_EQ(survivor(0, 1), 7, "Shared transformed views should retain storage lifetime");

    Tensor<int> owner({2, 3}, 1);
    FATP_ASSERT_THROWS(owner.sliceView({Slice{0, 1, 0}}), std::invalid_argument,
                       "A zero slice step must be rejected");
    FATP_ASSERT_THROWS(owner.sliceView({Ellipsis, Ellipsis}), std::invalid_argument,
                       "Multiple ellipses must be rejected");
    FATP_ASSERT_THROWS(owner.sliceView({0, 0, 0}), std::invalid_argument,
                       "A slice cannot consume more axes than the source");
    FATP_ASSERT_THROWS(owner.sliceView({std::ptrdiff_t{2}}), std::out_of_range,
                       "Integer slice indices are bounds checked");
    FATP_ASSERT_THROWS(owner.permuteView({0, 0}), std::invalid_argument,
                       "Permutation axes must be unique");
    FATP_ASSERT_THROWS(owner.squeezeView({0}), std::invalid_argument,
                       "Squeeze must reject a non-singleton axis");
    FATP_ASSERT_THROWS(owner.unsqueezeView(3), std::out_of_range,
                       "Unsqueeze must reject an insertion beyond the result rank");
    return true;
}

} // namespace fat_p::testing::tensor_slice

namespace fat_p::testing
{

bool test_TensorSlice()
{
    FATP_PRINT_HEADER(TENSOR SLICE)
    TestRunner runner;
    FATP_RUN_TEST_NS(runner, tensor_slice, negative_and_bounded_slices);
    FATP_RUN_TEST_NS(runner, tensor_slice, ellipsis_newaxis_and_integer_axis);
    FATP_RUN_TEST_NS(runner, tensor_slice, permute_squeeze_and_unsqueeze);
    FATP_RUN_TEST_NS(runner, tensor_slice, empty_shared_and_errors);
    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_TensorSlice() ? 0 : 1;
}
#endif
