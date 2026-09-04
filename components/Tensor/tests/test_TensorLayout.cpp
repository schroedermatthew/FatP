/**
 * @file test_TensorLayout.cpp
 * @brief Boundary and randomized tests for DynamicExtents and TensorLayout.
 */

/*
FATP_META:
  meta_version: 1
  component: TensorLayout
  file_role: test
  path: components/Tensor/tests/test_TensorLayout.cpp
  namespace: fat_p::testing::tensor_layout
  layer: Testing
  summary: "Checked inline extents, signed reachability, classification, and oracle tests."
  api_stability: in_work
  related:
    docs:
      - components/Tensor/docs/Design Note - Tensor Semantic Contract.md
      - components/Tensor/docs/Design Note - Tensor Architecture Additions Plan.md
    headers:
      - include/fat_p/TensorLayout.h
    tests:
      - components/Tensor/tests/TensorTestSupport.h
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
#include "TensorLayout.h"
#include "TensorTestSupport.h"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <set>
#include <stdexcept>
#include <vector>

namespace fat_p::testing::tensor_layout
{

FATP_TEST_CASE(dynamic_extents_and_axes)
{
    const DynamicExtents scalar;
    FATP_ASSERT_EQ(scalar.rank(), std::size_t{0}, "Default DynamicExtents should describe rank zero");
    FATP_ASSERT_EQ(scalar.logicalSize(), std::size_t{1}, "Rank zero should contain one logical element");

    const DynamicExtents matrix{2, 3};
    FATP_ASSERT_EQ(matrix.rank(), std::size_t{2}, "Matrix extents should have rank two");
    FATP_ASSERT_EQ(matrix.logicalSize(), std::size_t{6}, "Matrix logical size should be the checked product");

    const DynamicExtents empty{std::numeric_limits<std::size_t>::max(), 2, 0};
    FATP_ASSERT_EQ(empty.logicalSize(), std::size_t{0}, "A zero extent should short-circuit product overflow");
    FATP_ASSERT_TRUE(empty.hasZeroExtent(), "Zero-extent state should be explicit");

    FATP_ASSERT_EQ(normalizeAxis(0, 3), std::size_t{0}, "Positive axes should remain unchanged");
    FATP_ASSERT_EQ(normalizeAxis(-1, 3), std::size_t{2}, "Negative axes should normalize from the end");
    FATP_ASSERT_TRUE(normalizeAxes({-1, 0}, 3) == std::vector<std::size_t>({2, 0}),
                     "Axis lists should normalize deterministically");
    FATP_ASSERT_THROWS(normalizeAxis(3, 3), std::out_of_range, "Axis equal to rank should fail");
    FATP_ASSERT_THROWS(normalizeAxis(-4, 3), std::out_of_range, "Axis below negative rank should fail");
    FATP_ASSERT_THROWS(normalizeAxes({0, -3}, 3), std::invalid_argument,
                       "Duplicate normalized axes should fail");

    if constexpr (std::numeric_limits<std::size_t>::max() >
                  static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max()))
    {
        const auto unrepresentable = static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max()) + 1;
        FATP_ASSERT_THROWS(DynamicExtents({unrepresentable}), std::overflow_error,
                           "Nonempty logical size should fit ptrdiff_t");
    }
    FATP_ASSERT_THROWS(DynamicExtents({std::numeric_limits<std::size_t>::max(), 2}), std::overflow_error,
                       "Nonempty extent multiplication should be checked");
    return true;
}

FATP_TEST_CASE(inline_metadata_storage)
{
    const DynamicExtents commonRank{2, 3, 4, 5};
    FATP_ASSERT_TRUE(commonRank.values().usesInlineStorage(),
                     "Ranks through four should keep extent metadata inline");

    const auto commonLayout = TensorLayout::contiguous(commonRank);
    FATP_ASSERT_TRUE(commonLayout.extents().values().usesInlineStorage(),
                     "Copying a common-rank layout should retain inline extents");
    FATP_ASSERT_TRUE(commonLayout.strides().usesInlineStorage(),
                     "Canonical common-rank strides should remain inline");
    FATP_ASSERT_TRUE(commonLayout.strides() == TensorStrides({60, 20, 5, 1}),
                     "Inline canonical strides should preserve row-major values");

    const DynamicExtents higherRank{1, 1, 1, 1, 1};
    FATP_ASSERT_FALSE(higherRank.values().usesInlineStorage(),
                      "Ranks above four should use the unbounded heap fallback");
    FATP_ASSERT_EQ(higherRank.logicalSize(), std::size_t{1},
                   "Heap-backed extent metadata should preserve logical size");

    TensorStrides transitioning{4, 3, 2, 1};
    transitioning.insert(transitioning.begin(), 5);
    FATP_ASSERT_FALSE(transitioning.usesInlineStorage(),
                      "Growing stride metadata past four should spill to heap storage");
    transitioning.erase(transitioning.begin());
    FATP_ASSERT_TRUE(transitioning.usesInlineStorage(),
                     "Shrinking stride metadata to four should return it to inline storage");
    FATP_ASSERT_TRUE(transitioning == TensorStrides({4, 3, 2, 1}),
                     "Inline/heap transitions should preserve stride order and values");

    TensorStrides resized(6, 9);
    resized.resize(3);
    FATP_ASSERT_TRUE(resized.usesInlineStorage() && resized == TensorStrides({9, 9, 9}),
                     "Resizing heap metadata below the threshold should retain values inline");
    resized.resize(5, 7);
    FATP_ASSERT_FALSE(resized.usesInlineStorage(),
                      "Growing resized metadata above the threshold should restore heap storage");
    FATP_ASSERT_TRUE(resized == TensorStrides({9, 9, 9, 7, 7}),
                     "Repeated inline/heap resizing should preserve and initialize values");

    TensorStrides assigned{1};
    assigned = transitioning;
    FATP_ASSERT_TRUE(assigned.usesInlineStorage() && assigned == transitioning,
                     "Copy assignment should preserve inline metadata independently");
    assigned = resized;
    FATP_ASSERT_FALSE(assigned.usesInlineStorage(),
                      "Copy assignment should preserve heap fallback for higher ranks");
    FATP_ASSERT_TRUE(assigned == resized, "Heap metadata copy assignment should preserve every value");
    return true;
}

FATP_TEST_CASE(canonical_layouts)
{
    const auto scalar = TensorLayout::contiguous(DynamicExtents{});
    FATP_ASSERT_TRUE(scalar.kind() == TensorLayoutKind::Contiguous, "Rank-zero scalar should be contiguous");
    FATP_ASSERT_EQ(scalar.logicalSize(), std::size_t{1}, "Rank-zero scalar should contain one element");
    FATP_ASSERT_EQ(scalar.logicalOffset(0), std::ptrdiff_t{0}, "Scalar offset should equal its origin");

    const auto matrix = TensorLayout::contiguous(DynamicExtents{2, 3, 4});
    FATP_ASSERT_TRUE(matrix.strides() == TensorStrides({12, 4, 1}),
                     "Canonical strides should be row-major element strides");
    FATP_ASSERT_EQ(matrix.logicalOffset(23), std::ptrdiff_t{23},
                   "Canonical linear indexing should match storage order");

    const auto empty = TensorLayout::contiguous(DynamicExtents{2, 0, 3});
    FATP_ASSERT_TRUE(empty.kind() == TensorLayoutKind::Empty, "Any zero extent should classify as empty");
    FATP_ASSERT_TRUE(empty.isContiguous(), "Every empty mapping should be contiguous");
    FATP_ASSERT_FALSE(empty.minimumOffset().has_value(), "Empty mappings should have no reachable minimum");
    FATP_ASSERT_THROWS(empty.logicalOffset(0), std::out_of_range, "Empty mappings should reject linear access");

    for (std::size_t zeroAxis = 0; zeroAxis < 3; ++zeroAxis)
    {
        std::vector<std::size_t> extents{2, 3, 4};
        extents[zeroAxis] = 0;
        const auto zeroInAxis = TensorLayout::contiguous(DynamicExtents(extents));
        FATP_ASSERT_TRUE(zeroInAxis.isEmpty() && zeroInAxis.isContiguous(),
                         "A zero extent in every tested axis should create a contiguous empty layout");
    }

    const auto singleton = TensorLayout(6, 0, DynamicExtents{2, 1, 3}, TensorStrides{3, 999, 1});
    FATP_ASSERT_TRUE(singleton.isContiguous(), "Singleton-axis strides should not constrain contiguity");
    return true;
}

FATP_TEST_CASE(signed_reachability_and_classification)
{
    const TensorLayout reversed(6, 2, DynamicExtents{2, 3}, TensorStrides{3, -1});
    FATP_ASSERT_TRUE(reversed.kind() == TensorLayoutKind::InjectiveStrided,
                     "A reversed axis should be injective and non-contiguous");
    FATP_ASSERT_EQ(reversed.minimumOffset().value(), std::ptrdiff_t{0}, "Negative stride minimum should be checked");
    FATP_ASSERT_EQ(reversed.maximumOffset().value(), std::ptrdiff_t{5}, "Negative stride maximum should be checked");
    FATP_ASSERT_TRUE(
        std::vector<std::ptrdiff_t>({reversed.logicalOffset(0), reversed.logicalOffset(1), reversed.logicalOffset(2),
                                     reversed.logicalOffset(3), reversed.logicalOffset(4), reversed.logicalOffset(5)}) ==
            std::vector<std::ptrdiff_t>({2, 1, 0, 5, 4, 3}),
        "Logical order should carry signed offsets without forming pointers");

    const TensorLayout padded(7, 0, DynamicExtents{2, 3}, TensorStrides{4, 1});
    FATP_ASSERT_TRUE(padded.kind() == TensorLayoutKind::InjectiveStrided, "Padded rows should remain injective");

    const TensorLayout broadcast(3, 0, DynamicExtents{2, 3}, TensorStrides{0, 1});
    FATP_ASSERT_TRUE(broadcast.kind() == TensorLayoutKind::Broadcast,
                     "Expanded zero stride should classify as broadcast");
    FATP_ASSERT_TRUE(broadcast.isOverlapping(), "Broadcast layouts should be non-injective");

    const TensorLayout overlap(3, 0, DynamicExtents{2, 2}, TensorStrides{1, 1});
    FATP_ASSERT_TRUE(overlap.kind() == TensorLayoutKind::Overlapping,
                     "Repeated nonzero offsets should classify as overlap");

    const TensorLayout injectiveInterleaved(8, 0, DynamicExtents{3, 2}, TensorStrides{2, 3});
    FATP_ASSERT_TRUE(injectiveInterleaved.isInjective(), "Exact small-layout classification should accept interleaving");

    const TensorLayout largeInjectiveInterleaved(600'002, 0, DynamicExtents{300'000, 2},
                                                  TensorStrides{2, 3});
    FATP_ASSERT_TRUE(largeInjectiveInterleaved.isInjective(),
                     "Rank-two injectivity should remain exact beyond the bounded enumeration threshold");

    const TensorLayout largeUnresolved(1'000'004, 0, DynamicExtents{100'000, 2, 2},
                                       TensorStrides{10, 6, 7});
    FATP_ASSERT_TRUE(largeUnresolved.isIndeterminate(),
                     "A large higher-rank mapping without a proof should be reported as indeterminate, not overlap");
    FATP_ASSERT_FALSE(largeUnresolved.isOverlapping(),
                      "Indeterminate layouts must not make a false overlapping claim");
    return true;
}

FATP_TEST_CASE(validation_boundaries)
{
    FATP_ASSERT_THROWS(TensorLayout(4, 0, DynamicExtents{2, 2}, TensorStrides{1}), std::invalid_argument,
                       "Extent and stride rank mismatch should fail");
    FATP_ASSERT_THROWS(TensorLayout(4, -1, DynamicExtents{2, 2}, TensorStrides{2, 1}), std::out_of_range,
                       "Negative storage origin should fail");
    FATP_ASSERT_THROWS(TensorLayout(4, 0, DynamicExtents{2, 2}, TensorStrides{-2, 1}), std::out_of_range,
                       "Reachability before storage should fail");
    FATP_ASSERT_THROWS(TensorLayout(4, 3, DynamicExtents{2, 2}, TensorStrides{2, 1}), std::out_of_range,
                       "Reachability after storage should fail");
    FATP_ASSERT_THROWS(
        TensorLayout(8, 0, DynamicExtents{3}, TensorStrides{std::numeric_limits<std::ptrdiff_t>::max()}),
        std::overflow_error, "Stride contribution multiplication should be checked");
    FATP_ASSERT_THROWS(
        TensorLayout(static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max()),
                     std::numeric_limits<std::ptrdiff_t>::max() - 1, DynamicExtents{2}, TensorStrides{2}),
        std::overflow_error, "Reachable offset addition should be checked independently of multiplication");

    const auto maximum = static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max());
    const TensorLayout exactBoundary(maximum, 0, DynamicExtents{maximum}, TensorStrides{1});
    FATP_ASSERT_EQ(exactBoundary.maximumOffset().value(), std::numeric_limits<std::ptrdiff_t>::max() - 1,
                   "Exact ptrdiff_t storage boundary should remain representable");
    if constexpr (std::numeric_limits<std::size_t>::max() >
                  static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max()))
    {
        FATP_ASSERT_THROWS(TensorLayout(maximum + 1, 0, DynamicExtents{0}, TensorStrides{0}), std::overflow_error,
                           "Storage span should fit ptrdiff_t even for empty layouts");
    }
    return true;
}

FATP_TEST_CASE(randomized_scalar_oracle)
{
    using tensor_support::DeterministicLayoutGenerator;
    using tensor_support::LayoutSpec;
    using tensor_support::enumerateOffsets;
    using tensor_support::reachableBounds;

    DeterministicLayoutGenerator generator(0xC0FFEEULL);
    const std::vector<std::size_t> representativeRanks{0, 1, 2, 3, 8};
    for (const auto rank : representativeRanks)
    {
        for (std::size_t sample = 0; sample < 40; ++sample)
        {
            auto specification = generator.next(rank, 3);
            const auto originalOffsets = enumerateOffsets(specification);
            if (originalOffsets.empty())
            {
                const TensorLayout empty(0, 0, DynamicExtents(specification.extents), specification.strides);
                FATP_ASSERT_TRUE(empty.isEmpty(), "Generated zero extent should classify as empty");
                continue;
            }

            const auto originalBounds = reachableBounds(specification).value();
            specification.origin -= originalBounds.first;
            const auto adjustedOffsets = enumerateOffsets(specification);
            const auto adjustedBounds = reachableBounds(specification).value();
            const auto storageLength = static_cast<std::size_t>(adjustedBounds.second + 1);
            const TensorLayout layout(storageLength, specification.origin, DynamicExtents(specification.extents),
                                      specification.strides);

            FATP_ASSERT_EQ(layout.minimumOffset().value(), adjustedBounds.first,
                           "Validated minimum should equal scalar enumeration");
            FATP_ASSERT_EQ(layout.maximumOffset().value(), adjustedBounds.second,
                           "Validated maximum should equal scalar enumeration");
            for (std::size_t linear = 0; linear < adjustedOffsets.size(); ++linear)
            {
                FATP_ASSERT_EQ(layout.logicalOffset(linear), adjustedOffsets[linear],
                               "Layout offset should equal the independent scalar oracle");
            }
            const std::set<std::ptrdiff_t> uniqueOffsets(adjustedOffsets.begin(), adjustedOffsets.end());
            FATP_ASSERT_EQ(layout.isInjective(), uniqueOffsets.size() == adjustedOffsets.size(),
                           "Small-layout injectivity should equal exact scalar enumeration");
        }
    }

    const DynamicExtents rank32(std::vector<std::size_t>(32, 1));
    const TensorLayout highRank(1, 0, rank32, TensorStrides(32, std::numeric_limits<std::ptrdiff_t>::min()));
    FATP_ASSERT_EQ(highRank.rank(), std::size_t{32}, "Representative high-rank metadata should be accepted");
    FATP_ASSERT_TRUE(highRank.isContiguous(), "Singleton axes should make the high-rank mapping contiguous");
    return true;
}

} // namespace fat_p::testing::tensor_layout

namespace fat_p::testing
{

bool test_TensorLayout()
{
    FATP_PRINT_HEADER(TENSOR LAYOUT)
    TestRunner runner;
    FATP_RUN_TEST_NS(runner, tensor_layout, dynamic_extents_and_axes);
    FATP_RUN_TEST_NS(runner, tensor_layout, inline_metadata_storage);
    FATP_RUN_TEST_NS(runner, tensor_layout, canonical_layouts);
    FATP_RUN_TEST_NS(runner, tensor_layout, signed_reachability_and_classification);
    FATP_RUN_TEST_NS(runner, tensor_layout, validation_boundaries);
    FATP_RUN_TEST_NS(runner, tensor_layout, randomized_scalar_oracle);
    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_TensorLayout() ? 0 : 1;
}
#endif
