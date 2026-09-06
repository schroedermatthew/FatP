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
#include <array>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <new>
#include <set>
#include <stdexcept>
#include <vector>

#if defined(ENABLE_TEST_APPLICATION) && !defined(FATP_TENSOR_DISABLE_ALLOCATION_PROBE)
// Count ordinary metadata allocations only in the standalone executable. Unlike
// element allocator counters, this observes the classifier's hash nodes too.
// No failures are injected, so MSVC checked-iterator builds can use this probe.
namespace fat_p::testing::tensor_layout::allocation_probe
{
thread_local std::size_t* activeCounter = nullptr;

void* allocate(std::size_t bytes)
{
    if (void* storage = std::malloc(bytes == 0 ? 1 : bytes))
    {
        if (activeCounter != nullptr)
        {
            ++*activeCounter;
        }
        return storage;
    }
    throw std::bad_alloc();
}

class ScopedCounter
{
public:
    explicit ScopedCounter(std::size_t& count) noexcept : mPrevious(activeCounter) { activeCounter = &count; }
    ~ScopedCounter() { activeCounter = mPrevious; }
    ScopedCounter(const ScopedCounter&) = delete;
    ScopedCounter& operator=(const ScopedCounter&) = delete;

private:
    std::size_t* mPrevious;
};
} // namespace fat_p::testing::tensor_layout::allocation_probe

void* operator new(std::size_t bytes) { return fat_p::testing::tensor_layout::allocation_probe::allocate(bytes); }
void* operator new[](std::size_t bytes) { return fat_p::testing::tensor_layout::allocation_probe::allocate(bytes); }
void operator delete(void* storage) noexcept { std::free(storage); }
void operator delete[](void* storage) noexcept { std::free(storage); }
void operator delete(void* storage, std::size_t) noexcept { std::free(storage); }
void operator delete[](void* storage, std::size_t) noexcept { std::free(storage); }
#endif

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

    TensorStrides pushAliased{4, 3, 2, 1};
    pushAliased.push_back(pushAliased[1]);
    FATP_ASSERT_FALSE(pushAliased.usesInlineStorage(),
                      "Appending past inline capacity should spill aliased metadata to heap storage");
    FATP_ASSERT_TRUE(pushAliased == TensorStrides({4, 3, 2, 1, 3}),
                     "Inline-to-heap push_back should preserve an aliased source value");

    TensorStrides spillInsertAliased{4, 3, 2, 1};
    spillInsertAliased.insert(spillInsertAliased.begin() + 1, spillInsertAliased.back());
    FATP_ASSERT_FALSE(spillInsertAliased.usesInlineStorage(),
                      "Inserting past inline capacity should spill aliased metadata to heap storage");
    FATP_ASSERT_TRUE(spillInsertAliased == TensorStrides({4, 1, 3, 2, 1}),
                     "Inline-to-heap insert should preserve an aliased source value");

    TensorStrides inlineInsertAliased{4, 3, 2};
    inlineInsertAliased.insert(inlineInsertAliased.begin(), inlineInsertAliased[1]);
    FATP_ASSERT_TRUE(inlineInsertAliased.usesInlineStorage(),
                     "An insertion within inline capacity should remain inline");
    FATP_ASSERT_TRUE(inlineInsertAliased == TensorStrides({3, 4, 3, 2}),
                     "Inline insert should preserve a source value aliased by the shifted range");

    TensorStrides resizeAliased{4, 3, 2, 1};
    resizeAliased.resize(6, resizeAliased[1]);
    FATP_ASSERT_FALSE(resizeAliased.usesInlineStorage(),
                      "Resizing past inline capacity should spill aliased metadata to heap storage");
    FATP_ASSERT_TRUE(resizeAliased == TensorStrides({4, 3, 2, 1, 3, 3}),
                     "Inline-to-heap resize should preserve an aliased fill value");

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

FATP_TEST_CASE(higher_rank_injectivity_proofs)
{
    struct Case
    {
        std::size_t storageLength;
        std::ptrdiff_t origin;
        std::array<std::size_t, 3> extents;
        std::array<std::ptrdiff_t, 3> strides;
        TensorLayoutKind expected;
    };
    const std::array cases{
        Case{8000, 0, {20, 20, 20}, {1, 20, 400}, TensorLayoutKind::InjectiveStrided},
        Case{8000, 380, {20, 20, 20}, {1, -20, 400}, TensorLayoutKind::InjectiveStrided},
        // These interleaved addresses require the exact fallback, not packing.
        Case{18, 0, {3, 2, 2}, {2, 3, 10}, TensorLayoutKind::InjectiveStrided},
        // No pair aliases by itself, but 2 + 3 = 5 creates a three-axis collision.
        Case{11, 0, {2, 2, 2}, {2, 3, 5}, TensorLayoutKind::Overlapping},
        Case{1'000'004, 0, {100'000, 2, 2}, {10, 6, 7}, TensorLayoutKind::Indeterminate}};
    for (const auto& test : cases)
    {
        const TensorLayout dynamic(test.storageLength, test.origin,
                                   DynamicExtents(test.extents.begin(), test.extents.end()),
                                   TensorStrides(test.strides.begin(), test.strides.end()));
        const BasicTensorLayout<3> fixed(test.storageLength, test.origin,
                                         tensor_detail::FixedRankExtents<3>(test.extents), test.strides);
        FATP_ASSERT_TRUE(dynamic.kind() == test.expected, "Dynamic classification should preserve exact/proven results");
        FATP_ASSERT_TRUE(fixed.kind() == test.expected, "Fixed-rank classification should agree with dynamic layouts");
    }

    const auto maximum = std::numeric_limits<std::ptrdiff_t>::max();
    const auto minimum = std::numeric_limits<std::ptrdiff_t>::min();
    const TensorLayout extreme(static_cast<std::size_t>(maximum), maximum - 4,
                                DynamicExtents{2, 2, 2, 1}, TensorStrides{-(maximum - 4), 2, 1, minimum});
    const BasicTensorLayout<4> fixedExtreme(static_cast<std::size_t>(maximum), maximum - 4,
                                           {2, 2, 2, 1}, {-(maximum - 4), 2, 1, minimum});
    FATP_ASSERT_TRUE(extreme.isInjective() && fixedExtreme.isInjective(),
                     "Signed packing at the reachable-span limit should ignore extreme singleton strides");
    FATP_ASSERT_EQ(extreme.minimumOffset().value(), std::ptrdiff_t{0}, "Extreme mapping should start at zero");
    FATP_ASSERT_EQ(extreme.maximumOffset().value(), maximum - 1, "Extreme mapping should stay within storage");
    const BasicTensorLayout<5> interspersedSingletons(8, 0, {2, 1, 2, 1, 2}, {1, maximum, 4, minimum, 2});
    FATP_ASSERT_TRUE(interspersedSingletons.isInjective(), "Packing should sort only active fixed-rank axes");
    return true;
}

FATP_TEST_CASE(packed_layout_metadata_allocations)
{
#if defined(ENABLE_TEST_APPLICATION) && !defined(FATP_TENSOR_DISABLE_ALLOCATION_PROBE)
    std::array<std::size_t, 2> dynamicAllocations{};
    std::array<std::size_t, 2> fixedAllocations{};
    // Strides describe a common 3D permutation on either side of the exact
    // classification cutoff. Its metadata cost must not scale with element count.
    for (std::size_t index = 0; index < 2; ++index)
    {
        const auto side = std::size_t{20} + index;
        const auto stride = static_cast<std::ptrdiff_t>(side);
        TensorLayoutKind dynamicKind;
        TensorLayoutKind fixedKind;
        {
            allocation_probe::ScopedCounter counter(dynamicAllocations[index]);
            const TensorLayout layout(side * side * side, 0, DynamicExtents{side, side, side},
                                       TensorStrides{1, stride, stride * stride});
            dynamicKind = layout.kind();
        }
        {
            allocation_probe::ScopedCounter counter(fixedAllocations[index]);
            const BasicTensorLayout<3> layout(side * side * side, 0, {side, side, side},
                                              {1, stride, stride * stride});
            fixedKind = layout.kind();
        }
        FATP_ASSERT_TRUE(dynamicKind == TensorLayoutKind::InjectiveStrided && fixedKind == dynamicKind,
                         "Permuted layouts should be proven injective for both rank families");
    }
    FATP_ASSERT_EQ(dynamicAllocations[0], dynamicAllocations[1],
                   "A smaller permutation must not allocate one hash node per logical element");
    FATP_ASSERT_EQ(fixedAllocations[0], std::size_t{0}, "Small packed fixed-rank classification needs no heap scratch");
    FATP_ASSERT_EQ(fixedAllocations[1], std::size_t{0}, "Large packed fixed-rank classification needs no heap scratch");
#else
    std::cout << "[SKIP] Metadata allocation counting requires standalone replacement-new support\n";
#endif
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

FATP_TEST_CASE(metadata_moves_preserve_invariants)
{
    static_assert(std::is_nothrow_move_constructible_v<DynamicExtents>);
    static_assert(std::is_nothrow_move_assignable_v<TensorLayout>);
    for (const std::size_t rank : {4U, 5U})
    {
        for (const bool empty : {false, true})
        {
            std::vector<std::size_t> values(rank, 2);
            if (empty)
            {
                values[0] = 0;
            }
            const DynamicExtents expected(values);
            DynamicExtents source = expected;
            DynamicExtents moved(std::move(source));
            FATP_ASSERT_TRUE(moved == expected, "Extent moves preserve the destination count and axes");
            FATP_ASSERT_TRUE(source == DynamicExtents{}, "Moved-from extents are coherent scalars");
            FATP_ASSERT_EQ(TensorLayout::contiguous(source).logicalSize(), std::size_t{1},
                           "Moved-from extents can construct a valid scalar layout");
            source = std::move(moved);
            FATP_ASSERT_TRUE(source == expected && moved == DynamicExtents{},
                             "Move assignment preserves both extent invariants");

            const auto expectedLayout = TensorLayout::contiguous(expected);
            auto layout = expectedLayout;
            auto transferred = std::move(layout);
            const auto emptyLayout = TensorLayout::contiguous(DynamicExtents{0});
            FATP_ASSERT_TRUE(transferred == expectedLayout && layout == emptyLayout,
                             "Moved-from layout resets all cached properties");
            FATP_ASSERT_THROWS(layout.logicalOffset(0), std::out_of_range,
                               "Moved-from layout cannot address elements through a stale count");
            layout = std::move(transferred);
            FATP_ASSERT_TRUE(layout == expectedLayout && transferred == emptyLayout,
                             "Layout move assignment resets the source too");
        }
    }
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
    FATP_RUN_TEST_NS(runner, tensor_layout, metadata_moves_preserve_invariants);
    FATP_RUN_TEST_NS(runner, tensor_layout, canonical_layouts);
    FATP_RUN_TEST_NS(runner, tensor_layout, signed_reachability_and_classification);
    FATP_RUN_TEST_NS(runner, tensor_layout, higher_rank_injectivity_proofs);
    FATP_RUN_TEST_NS(runner, tensor_layout, packed_layout_metadata_allocations);
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
