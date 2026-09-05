/**
 * @file test_TensorRanked.cpp
 * @brief Behavioral and compile-time coverage for fixed-rank, runtime-extents tensors.
 */

/*
FATP_META:
  meta_version: 1
  component: TensorRanked
  file_role: test
  path: components/Tensor/tests/test_TensorRanked.cpp
  namespace: fat_p::testing::tensorranked
  layer: Testing
  summary: "Behavioral, result-rank, layout, lifetime, and interop tests for TensorRanked."
  api_stability: in_work
  related:
    docs:
      - components/Tensor/docs/User Manual - TensorRanked.md
      - components/Tensor/docs/Design Note - Tensor Semantic Contract.md
    headers:
      - include/fat_p/TensorRanked.h
      - include/fat_p/TensorAlgorithms.h
      - include/fat_p/TensorReductions.h
      - include/fat_p/TensorSelection.h
      - include/fat_p/TensorMatmul.h
      - include/fat_p/TensorContractions.h
      - include/fat_p/TensorExecution.h
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
#include "FatPConcepts.h"
#include "TensorAlgorithms.h"
#include "TensorContractions.h"
#include "TensorExecution.h"
#include "TensorInterop.h"
#include "TensorMatmul.h"
#include "TensorRanked.h"
#include "TensorReductions.h"
#include "TensorSelection.h"

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <numeric>
#include <optional>
#include <sstream>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

namespace fat_p::testing::tensorranked
{

static_assert(tensor_static_rank_v<Tensor<int>> == kDynamicTensorRank);
static_assert(tensor_static_rank_v<RankedTensor<int, 0>> == 0);
static_assert(tensor_static_rank_v<RankedTensorView<const int, 8>> == 8);
static_assert(tensor_static_rank_v<StaticTensor<int, Matrix<2, 3>>> == 2);
static_assert(tensor_static_rank_v<const StaticTensor<int, Matrix<2, 3>>&> == 2);
static_assert(!std::default_initializable<RankedTensorView<int, 0>>);
static_assert(!std::default_initializable<RankedTensorView<int, 2>>);
static_assert(!std::default_initializable<SharedRankedTensorView<int, 2>>);

struct UnregisteredTensorLike
{
    using value_type = int;
    using element_type = int;
    using extents_type = DynamicExtents;
    using layout_type = TensorLayout;

    [[nodiscard]] const extents_type& extents() const;
    [[nodiscard]] const layout_type& layout() const;
    [[nodiscard]] std::size_t rank() const;
    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] int operator[](std::size_t) const;
};

static_assert(ReadableTensor<RankedTensor<int, 2>>);
static_assert(ReadableTensor<RankedTensorView<const int, 2>>);
static_assert(WritableTensor<SharedRankedTensorView<int, 2>>);
static_assert(!ReadableTensor<UnregisteredTensorLike>);
static_assert(concepts::tensor_type<RankedTensor<int, 2>>);
static_assert(concepts::tensor_view_type<RankedTensorView<int, 2>>);
static_assert(concepts::tensor_view_type<SharedRankedTensorView<int, 2>>);

template <typename Source>
concept OneIndexCall = requires(Source& source) { source(0); };

template <typename Source>
concept TwoIndexCall = requires(Source& source) { source(0, 0); };

template <typename Source>
concept ThreeIndexCall = requires(Source& source) { source(0, 0, 0); };

template <typename Source>
concept TemporaryDynamicBorrow = requires(Source source) { asDynamicView(std::move(source)); };

template <typename Left, typename Right>
concept StackPair = requires(const Left& left, const Right& right) { stack(left, right); };

template <typename Left, typename Right>
concept ConcatenatePair = requires(const Left& left, const Right& right) {
    concatenate(left, right);
};

template <typename Source>
concept Transposable = requires(Source& source) { source.transposeView(); };

template <typename Source>
concept BroadcastDown = requires(const Source& source) {
    source.broadcastView(RankedExtents<1>{1});
};

template <typename Source>
concept OverconsumingSlice = requires(Source& source) { source.sliceView(0, 0, 0); };

template <typename Source>
concept OverconsumingRetainedSlice = requires(Source& source) {
    source.sliceView(All, All, All);
};

template <typename Source>
concept DuplicateEllipsisSlice = requires(Source& source) {
    source.sliceView(Ellipsis, Ellipsis);
};

enum ImplicitSliceIndex
{
    implicitSliceIndexZero = 0
};

template <typename Source, typename Specification>
concept SingleTypedSlice = requires(Source& source, Specification& specification) {
    source.sliceView(specification);
};

template <typename Source>
concept TemporarySharedSlice = requires(Source&& source, const std::vector<std::size_t>& start,
                                        const std::vector<std::size_t>& finish) {
    static_cast<Source&&>(source).sharedSliceView(start, finish);
};

template <typename Source>
concept TemporaryDynamicSharedAdapter = requires(Source&& source) {
    asDynamicSharedView(static_cast<Source&&>(source));
};

template <typename Source>
concept TemporaryRankedSharedAdapter = requires(Source&& source) {
    asRankedSharedView<2>(static_cast<Source&&>(source));
};

static_assert(!OneIndexCall<RankedTensor<int, 2>>);
static_assert(TwoIndexCall<RankedTensor<int, 2>>);
static_assert(!ThreeIndexCall<RankedTensor<int, 2>>);
static_assert(!OneIndexCall<RankedTensorView<int, 2>>);
static_assert(TwoIndexCall<RankedTensorView<int, 2>>);
static_assert(!TemporaryDynamicBorrow<RankedTensor<int, 2>>);
static_assert(!StackPair<RankedTensor<int, 2>, RankedTensor<int, 3>>);
static_assert(!ConcatenatePair<RankedTensor<int, 2>, RankedTensor<int, 3>>);
static_assert(StackPair<RankedTensor<int, 2>, Tensor<int>>);
static_assert(ConcatenatePair<RankedTensor<int, 2>, Tensor<int>>);
static_assert(Transposable<RankedTensor<int, 2>>);
static_assert(!Transposable<RankedTensor<int, 3>>);
static_assert(!BroadcastDown<RankedTensor<int, 2>>);
static_assert(!OverconsumingSlice<RankedTensor<int, 2>>);
static_assert(!OverconsumingRetainedSlice<RankedTensor<int, 2>>);
static_assert(!OverconsumingRetainedSlice<RankedTensorView<int, 2>>);
static_assert(!OverconsumingRetainedSlice<SharedRankedTensorView<int, 2>>);
static_assert(!DuplicateEllipsisSlice<RankedTensor<int, 2>>);
static_assert(!DuplicateEllipsisSlice<RankedTensorView<int, 2>>);
static_assert(!DuplicateEllipsisSlice<SharedRankedTensorView<int, 2>>);
static_assert(SingleTypedSlice<RankedTensor<int, 2>, Slice>);
static_assert(!SingleTypedSlice<RankedTensor<int, 2>, bool>);
static_assert(!SingleTypedSlice<RankedTensor<int, 2>, ImplicitSliceIndex>);
static_assert(!SingleTypedSlice<RankedTensor<int, 2>, SliceSpec>);
static_assert(!SingleTypedSlice<RankedTensorView<int, 2>, SliceSpec>);
static_assert(!SingleTypedSlice<SharedRankedTensorView<int, 2>, SliceSpec>);
static_assert(!TemporarySharedSlice<RankedTensor<int, 2>>);
static_assert(!TemporarySharedSlice<const RankedTensor<int, 2>>);
static_assert(!TemporaryDynamicSharedAdapter<RankedTensor<int, 2>>);
static_assert(!TemporaryDynamicSharedAdapter<const RankedTensor<int, 2>>);
static_assert(!TemporaryRankedSharedAdapter<Tensor<int>>);
static_assert(!TemporaryRankedSharedAdapter<const Tensor<int>>);

template <typename T>
class TaggedAllocator
{
public:
    using value_type = T;

    TaggedAllocator() = default;
    explicit TaggedAllocator(int identifier) : mIdentifier(identifier) {}
    TaggedAllocator(const TaggedAllocator&) = default;
    TaggedAllocator& operator=(const TaggedAllocator&) = default;

    TaggedAllocator(TaggedAllocator&& other) noexcept
        : mIdentifier(std::exchange(other.mIdentifier, -1))
    {
    }

    TaggedAllocator& operator=(TaggedAllocator&& other) noexcept
    {
        mIdentifier = std::exchange(other.mIdentifier, -1);
        return *this;
    }

    template <typename U>
    TaggedAllocator(const TaggedAllocator<U>& other) : mIdentifier(other.identifier())
    {
    }

    [[nodiscard]] T* allocate(std::size_t count)
    {
        return std::allocator<T>{}.allocate(count);
    }

    void deallocate(T* storage, std::size_t count) noexcept
    {
        std::allocator<T>{}.deallocate(storage, count);
    }

    [[nodiscard]] int identifier() const noexcept { return mIdentifier; }

    template <typename U>
    [[nodiscard]] bool operator==(const TaggedAllocator<U>& other) const noexcept
    {
        return mIdentifier == other.identifier();
    }

    template <typename U>
    struct rebind
    {
        using other = TaggedAllocator<U>;
    };

    using propagate_on_container_move_assignment = std::true_type;

private:
    int mIdentifier = 0;
};

struct AllocationProbe
{
    std::size_t allocations = 0;
    std::size_t deallocations = 0;
    bool failNext = false;
};

template <typename T, bool PropagateOnMove = false>
class FailingAllocator
{
public:
    using value_type = T;

    FailingAllocator()
        : mProbe(std::make_shared<AllocationProbe>())
    {
    }

    explicit FailingAllocator(std::shared_ptr<AllocationProbe> probe)
        : mProbe(std::move(probe))
    {
    }

    template <typename U>
    FailingAllocator(const FailingAllocator<U, PropagateOnMove>& other)
        : mProbe(other.probe())
    {
    }

    [[nodiscard]] T* allocate(std::size_t count)
    {
        if (mProbe->failNext)
        {
            mProbe->failNext = false;
            throw std::bad_alloc();
        }
        ++mProbe->allocations;
        return std::allocator<T>{}.allocate(count);
    }

    void deallocate(T* storage, std::size_t count) noexcept
    {
        ++mProbe->deallocations;
        std::allocator<T>{}.deallocate(storage, count);
    }

    [[nodiscard]] const std::shared_ptr<AllocationProbe>& probe() const noexcept { return mProbe; }

    template <typename U>
    [[nodiscard]] bool operator==(const FailingAllocator<U, PropagateOnMove>& other) const noexcept
    {
        return mProbe == other.probe();
    }

    template <typename U>
    struct rebind
    {
        using other = FailingAllocator<U, PropagateOnMove>;
    };

    using propagate_on_container_move_assignment = std::bool_constant<PropagateOnMove>;

private:
    std::shared_ptr<AllocationProbe> mProbe;
};

struct MoveObserved
{
    int value = 0;

    MoveObserved() = default;
    explicit MoveObserved(int initialValue) : value(initialValue) {}
    MoveObserved(const MoveObserved&) = default;
    MoveObserved& operator=(const MoveObserved&) = default;

    MoveObserved(MoveObserved&& other) noexcept
        : value(std::exchange(other.value, -1))
    {
    }

    MoveObserved& operator=(MoveObserved&& other) noexcept
    {
        value = std::exchange(other.value, -1);
        return *this;
    }
};

struct MoveOnlyObserved
{
    int value = 0;

    MoveOnlyObserved() = default;
    MoveOnlyObserved(const MoveOnlyObserved&) = delete;
    MoveOnlyObserved& operator=(const MoveOnlyObserved&) = delete;

    MoveOnlyObserved(MoveOnlyObserved&& other) noexcept
        : value(std::exchange(other.value, -1))
    {
    }

    MoveOnlyObserved& operator=(MoveOnlyObserved&& other) noexcept
    {
        value = std::exchange(other.value, -1);
        return *this;
    }
};

template <std::size_t Rank>
bool compareRankFamilies(const std::array<std::size_t, Rank>& dimensions)
{
    RankedExtents<Rank> rankedExtents(dimensions);
    const auto spanExtents = RankedExtents<Rank>::fromSpan(
        std::span<const std::size_t>(dimensions.data(), dimensions.size()));
    FATP_ASSERT_TRUE(rankedExtents == spanExtents, "fromSpan retains the exact fixed-rank dimensions");

    RankedTensor<int, Rank> ranked(rankedExtents);
    Tensor<int> dynamic(DynamicExtents(dimensions.begin(), dimensions.end()));
    std::iota(ranked.begin(), ranked.end(), 1);
    std::iota(dynamic.begin(), dynamic.end(), 1);
    FATP_ASSERT_TRUE(ranked.layout() == dynamic.layout(),
                     "Ranked and dynamic canonical layouts have identical semantics");
    FATP_ASSERT_TRUE(exactEqual(ranked, dynamic),
                     "Ranked and dynamic owners expose the same logical values");

    const auto rankedResult = add(ranked, ranked);
    const auto dynamicResult = add(dynamic, dynamic);
    static_assert(tensor_static_rank_v<decltype(rankedResult)> == Rank);
    FATP_ASSERT_TRUE(exactEqual(rankedResult, dynamicResult),
                     "Ranked and dynamic elementwise kernels agree");
    return true;
}

template <std::size_t Rank>
bool compareLayouts(std::size_t storageLength, std::ptrdiff_t origin,
                    const std::array<std::size_t, Rank>& dimensions,
                    const std::array<std::ptrdiff_t, Rank>& strides)
{
    const RankedTensorLayout<Rank> ranked(storageLength, origin, RankedExtents<Rank>(dimensions), strides);
    const TensorLayout dynamic(storageLength, origin,
                               DynamicExtents(dimensions.begin(), dimensions.end()),
                               TensorStrides(strides.begin(), strides.end()));
    FATP_ASSERT_TRUE(ranked == dynamic, "Ranked and dynamic layout validation results agree");
    for (std::size_t linear = 0; linear < ranked.logicalSize(); ++linear)
    {
        FATP_ASSERT_EQ(ranked.logicalOffset(linear), dynamic.logicalOffset(linear),
                       "Ranked and dynamic layouts produce identical logical offsets");
    }
    return true;
}

FATP_TEST_CASE(extents_layout_and_rank_storage)
{
    const RankedExtents<0> scalarExtents;
    const RankedExtents<5> highRank{2, 1, 3, 1, 4};
    const std::array<std::size_t, 2> wrongRank{2, 3};
    FATP_ASSERT_EQ(scalarExtents.logicalSize(), 1u, "Rank-zero extents describe one scalar");
    FATP_ASSERT_EQ(highRank.logicalSize(), 24u, "Fixed-rank extents retain runtime sizes");
    FATP_ASSERT_THROWS(RankedExtents<3>::fromSpan(wrongRank), std::invalid_argument,
                       "fromSpan rejects a runtime dimension count that differs from the static rank");
    std::istringstream streamedDimensions("2 3 4");
    auto firstDimension = std::istream_iterator<std::size_t>(streamedDimensions);
    const auto lastDimension = std::istream_iterator<std::size_t>();
    const RankedExtents<3> inputIteratorExtents(firstDimension, lastDimension);
    const RankedExtents<3> expectedInputIteratorExtents{2, 3, 4};
    FATP_ASSERT_TRUE(inputIteratorExtents == expectedInputIteratorExtents,
                     "Fixed-rank extents consume single-pass input iterators exactly once");
    static_assert(std::same_as<RankedExtents<5>::container_type, std::array<std::size_t, 5>>);

    const auto contiguous = RankedTensorLayout<5>::contiguous(highRank);
    static_assert(std::same_as<RankedTensorLayout<5>::strides_type,
                               std::array<std::ptrdiff_t, 5>>);
    FATP_ASSERT_TRUE(contiguous.isContiguous(), "Ranked canonical layout is contiguous");

    const RankedTensorLayout<2> padded(7, 0, RankedExtents<2>{2, 3}, {4, 1});
    const RankedTensorLayout<2> reversed(6, 5, RankedExtents<2>{2, 3}, {-3, -1});
    const RankedTensorLayout<2> broadcast(3, 0, RankedExtents<2>{2, 3}, {0, 1});
    const RankedTensorLayout<2> overlap(3, 0, RankedExtents<2>{2, 2}, {1, 1});
    const RankedTensorLayout<3> unresolved(1'000'004, 0, RankedExtents<3>{100'000, 2, 2},
                                            {10, 6, 7});
    FATP_ASSERT_TRUE(padded.kind() == TensorLayoutKind::InjectiveStrided,
                     "Ranked padded layout remains injective");
    FATP_ASSERT_EQ(reversed.logicalOffset(5), 0, "Ranked reversed layout uses signed offsets");
    FATP_ASSERT_TRUE(broadcast.kind() == TensorLayoutKind::Broadcast,
                     "Ranked zero stride classifies as broadcast");
    FATP_ASSERT_TRUE(overlap.kind() == TensorLayoutKind::Overlapping,
                     "Ranked repeated offsets classify as overlap");
    FATP_ASSERT_TRUE(unresolved.isIndeterminate(),
                     "Ranked large unresolved layouts preserve conservative classification");
    return true;
}

FATP_TEST_CASE(rank_family_differential_matrix)
{
    FATP_ASSERT_TRUE(compareRankFamilies(std::array<std::size_t, 0>{}),
                     "Rank-zero family differential passes");
    FATP_ASSERT_TRUE(compareRankFamilies(std::array<std::size_t, 1>{4}),
                     "Rank-one family differential passes");
    FATP_ASSERT_TRUE(compareRankFamilies(std::array<std::size_t, 2>{2, 3}),
                     "Rank-two family differential passes");
    FATP_ASSERT_TRUE(compareRankFamilies(std::array<std::size_t, 4>{2, 1, 2, 2}),
                     "Rank-four family differential passes");
    FATP_ASSERT_TRUE(compareRankFamilies(std::array<std::size_t, 5>{1, 2, 1, 2, 2}),
                     "Rank-five family differential passes");
    FATP_ASSERT_TRUE(compareRankFamilies(std::array<std::size_t, 8>{1, 2, 1, 2, 1, 2, 1, 2}),
                     "Rank-eight family differential passes");
    return true;
}

FATP_TEST_CASE(layout_differential_boundaries)
{
    FATP_ASSERT_TRUE(compareLayouts(1, 0, std::array<std::size_t, 0>{},
                                    std::array<std::ptrdiff_t, 0>{}),
                     "Rank-zero layout differential passes");
    FATP_ASSERT_TRUE(compareLayouts(4, 3, std::array<std::size_t, 1>{4},
                                    std::array<std::ptrdiff_t, 1>{-1}),
                     "Reversed layout differential passes");
    FATP_ASSERT_TRUE(compareLayouts(7, 0, std::array<std::size_t, 2>{2, 3},
                                    std::array<std::ptrdiff_t, 2>{4, 1}),
                     "Padded layout differential passes");
    FATP_ASSERT_TRUE(compareLayouts(3, 0, std::array<std::size_t, 4>{2, 3, 1, 1},
                                    std::array<std::ptrdiff_t, 4>{0, 1, 0, 0}),
                     "Broadcast layout differential passes");
    FATP_ASSERT_TRUE(compareLayouts(3, 0, std::array<std::size_t, 5>{2, 2, 1, 1, 1},
                                    std::array<std::ptrdiff_t, 5>{1, 1, 0, 0, 0}),
                     "Overlapping layout differential passes");
    FATP_ASSERT_TRUE(compareLayouts(
                         1'000'004, 0,
                         std::array<std::size_t, 8>{100'000, 2, 2, 1, 1, 1, 1, 1},
                         std::array<std::ptrdiff_t, 8>{10, 6, 7, 0, 0, 0, 0, 0}),
                     "Indeterminate high-rank layout differential passes");
    FATP_ASSERT_TRUE(compareLayouts(0, 0, std::array<std::size_t, 4>{2, 0, 3, 4},
                                    std::array<std::ptrdiff_t, 4>{0, 0, 0, 0}),
                     "Empty layout differential passes");
    FATP_ASSERT_TRUE(compareLayouts(8'199, 0, std::array<std::size_t, 4>{8, 8, 8, 16},
                                    std::array<std::ptrdiff_t, 4>{1'025, 128, 16, 1}),
                     "Exact-enumeration boundary differential passes");
    constexpr auto largestOffset = std::numeric_limits<std::ptrdiff_t>::max();
    FATP_ASSERT_TRUE(compareLayouts(static_cast<std::size_t>(largestOffset), 0,
                                    std::array<std::size_t, 1>{2},
                                    std::array<std::ptrdiff_t, 1>{largestOffset - 1}),
                     "Representable arithmetic boundary differential passes");
    FATP_ASSERT_THROWS((RankedExtents<2>{std::numeric_limits<std::size_t>::max(), 2}),
                       std::overflow_error, "Ranked extent multiplication is checked");
    FATP_ASSERT_THROWS((DynamicExtents{std::numeric_limits<std::size_t>::max(), 2}),
                       std::overflow_error, "Dynamic extent multiplication is checked identically");
    return true;
}

FATP_TEST_CASE(owner_defaults_moves_and_views)
{
    RankedTensor<int, 3> empty;
    FATP_ASSERT_EQ(empty.rank(), 3u, "Ranked default retains its static rank");
    FATP_ASSERT_EQ(empty.size(), 0u, "Positive-rank default has all-zero extents");

    RankedTensor<int, 0> scalar;
    FATP_ASSERT_EQ(scalar.size(), 1u, "Rank-zero default owns one value-initialized scalar");
    FATP_ASSERT_EQ(scalar(), 0, "Rank-zero default value is value-initialized");
    scalar() = 17;
    auto scalarView = scalar.asView();
    RankedTensor<int, 0> moved(std::move(scalar));
    FATP_ASSERT_EQ(moved(), 17, "Rank-zero move transfers the scalar value");
    FATP_ASSERT_EQ(scalar.size(), 1u, "Moved-from rank-zero owner remains a valid scalar");
#ifndef NDEBUG
    FATP_ASSERT_THROWS(scalarView(), std::runtime_error,
                       "Rank-zero move invalidates existing borrowed views");
#endif

    using TaggedScalar = Tensor<int, TaggedAllocator<int>, 0>;
    TaggedScalar taggedScalar(std::allocator_arg, TaggedAllocator<int>(91), RankedExtents<0>{}, 25);
    TaggedScalar taggedMoved(std::move(taggedScalar));
    FATP_ASSERT_EQ(taggedScalar.get_allocator().identifier(), 91,
                   "Rank-zero move construction preserves the live source allocator");
    FATP_ASSERT_EQ(taggedMoved.get_allocator().identifier(), 91,
                   "Rank-zero move construction copies allocator state to the destination");
    TaggedScalar taggedAssigned(std::allocator_arg, TaggedAllocator<int>(92), RankedExtents<0>{}, 3);
    taggedAssigned = std::move(taggedMoved);
    FATP_ASSERT_EQ(taggedMoved.get_allocator().identifier(), 91,
                   "Propagating rank-zero move assignment preserves the live source allocator");
    FATP_ASSERT_EQ(taggedAssigned.get_allocator().identifier(), 91,
                   "Propagating rank-zero move assignment copies source allocator state");

    RankedTensor<int, 2> matrix({2, 3}, 4);
    auto view = matrix.asView();
    auto movedView = std::move(view);
    FATP_ASSERT_EQ(view(1, 2), 4, "Ranked view move construction keeps the source binding valid");
    FATP_ASSERT_EQ(movedView(1, 2), 4, "Ranked view move construction copies the binding");

    auto* originalStorage = matrix.data();
    auto dynamic = toDynamicTensor(std::move(matrix));
    FATP_ASSERT_TRUE(dynamic.data() == originalStorage,
                     "Compatible ranked-to-dynamic rvalue conversion transfers element storage");
    FATP_ASSERT_EQ(matrix.size(), 0u, "Transferred positive-rank source becomes its empty default");

    using TaggedTensor = Tensor<int, TaggedAllocator<int>>;
    TaggedTensor mismatched(std::allocator_arg, TaggedAllocator<int>(71), DynamicExtents{2, 2}, 9);
    auto mismatchedView = mismatched.asView();
    auto* mismatchedStorage = mismatched.data();
    FATP_ASSERT_THROWS(toRankedTensor<3>(std::move(mismatched)), std::invalid_argument,
                       "A failed rvalue rank conversion reports the mismatch");
    FATP_ASSERT_EQ(mismatched.get_allocator().identifier(), 71,
                   "Failed rank validation does not move the source allocator");
    FATP_ASSERT_TRUE(mismatched.data() == mismatchedStorage,
                     "Failed rank validation does not move source storage");
    FATP_ASSERT_EQ(mismatchedView(1, 1), 9,
                   "Failed rank validation leaves existing borrowed views alive");

    TaggedTensor transferable(std::allocator_arg, TaggedAllocator<int>(83), DynamicExtents{2, 2}, 11);
    auto* transferableStorage = transferable.data();
    auto rankedTransfer = toRankedTensor<2>(std::move(transferable));
    static_assert(tensor_static_rank_v<decltype(rankedTransfer)> == 2);
    FATP_ASSERT_TRUE(rankedTransfer.data() == transferableStorage,
                     "Compatible dynamic-to-ranked conversion transfers element storage");
    FATP_ASSERT_EQ(rankedTransfer.get_allocator().identifier(), 83,
                   "Dynamic-to-ranked transfer preserves allocator identity");
    FATP_ASSERT_EQ(transferable.size(), 0u,
                   "Transferred dynamic source returns to its canonical empty state");

    auto* sameRankStorage = rankedTransfer.data();
    auto sameRankTransfer = toRankedTensor<2>(std::move(rankedTransfer));
    FATP_ASSERT_TRUE(sameRankTransfer.data() == sameRankStorage,
                     "Same-rank rvalue conversion transfers rather than copying storage");

    RankedTensor<int, 0> conversionScalar(RankedExtents<0>{}, 23);
    auto* scalarStorage = conversionScalar.data();
    auto dynamicScalar = toDynamicTensor(std::move(conversionScalar));
    FATP_ASSERT_TRUE(dynamicScalar.data() != scalarStorage,
                     "Rank-zero cross-family transfer uses element movement rather than stealing storage");
    FATP_ASSERT_EQ(dynamicScalar(), 23, "Rank-zero cross-family transfer preserves the destination value");
    FATP_ASSERT_EQ(conversionScalar.size(), 1u,
                   "Rank-zero cross-family source remains a valid scalar");
    auto taggedDynamicScalar = toDynamicTensor(std::move(taggedAssigned));
    FATP_ASSERT_EQ(taggedAssigned.get_allocator().identifier(), 91,
                   "Rank-zero cross-family transfer preserves the live source allocator");
    FATP_ASSERT_EQ(taggedDynamicScalar.get_allocator().identifier(), 91,
                   "Rank-zero cross-family transfer copies allocator state to the destination");
    return true;
}

FATP_TEST_CASE(rank_zero_move_preserves_shared_alias_values)
{
    using PropagatingScalar = Tensor<MoveObserved, TaggedAllocator<MoveObserved>, 0>;

    PropagatingScalar propagatingUniqueSource(
        std::allocator_arg, TaggedAllocator<MoveObserved>(101), RankedExtents<0>{}, MoveObserved(11));
    PropagatingScalar propagatingUniqueDestination(
        std::allocator_arg, TaggedAllocator<MoveObserved>(102), RankedExtents<0>{}, MoveObserved(1));
    propagatingUniqueDestination = std::move(propagatingUniqueSource);
    FATP_ASSERT_EQ(propagatingUniqueDestination().value, 11,
                   "Propagating rank-zero assignment moves a uniquely owned value");
    FATP_ASSERT_EQ(propagatingUniqueSource().value, -1,
                   "A unique propagating rank-zero source contains its moved-from value");
    FATP_ASSERT_EQ(propagatingUniqueDestination.get_allocator().identifier(), 101,
                   "Propagating rank-zero assignment adopts the source allocator");

    PropagatingScalar propagatingSharedSource(
        std::allocator_arg, TaggedAllocator<MoveObserved>(103), RankedExtents<0>{}, MoveObserved(21));
    auto propagatingAlias = propagatingSharedSource.asSharedView();
    PropagatingScalar propagatingSharedDestination(
        std::allocator_arg, TaggedAllocator<MoveObserved>(104), RankedExtents<0>{}, MoveObserved(2));
    propagatingSharedDestination = std::move(propagatingSharedSource);
    FATP_ASSERT_EQ(propagatingSharedDestination().value, 21,
                   "Propagating rank-zero assignment materializes the destination value");
    FATP_ASSERT_EQ(propagatingSharedSource().value, 21,
                   "Propagating rank-zero assignment copies a source with shared aliases");
    FATP_ASSERT_EQ(propagatingAlias().value, 21,
                   "Propagating rank-zero assignment preserves shared alias values");
    FATP_ASSERT_EQ(propagatingSharedDestination.get_allocator().identifier(), 103,
                   "Shared propagating rank-zero assignment still adopts the source allocator");

    auto propagatingSourceProbe = std::make_shared<AllocationProbe>();
    auto propagatingDestinationProbe = std::make_shared<AllocationProbe>();
    using PropagatingFailingScalar =
        Tensor<MoveObserved, FailingAllocator<MoveObserved, true>, 0>;
    PropagatingFailingScalar failingSharedSource(
        std::allocator_arg, FailingAllocator<MoveObserved, true>(propagatingSourceProbe),
        RankedExtents<0>{}, MoveObserved(25));
    auto failingSharedAlias = failingSharedSource.asSharedView();
    auto failingSharedBorrow = failingSharedSource.asView();
    PropagatingFailingScalar failingSharedDestination(
        std::allocator_arg, FailingAllocator<MoveObserved, true>(propagatingDestinationProbe),
        RankedExtents<0>{}, MoveObserved(5));
    auto failingDestinationBorrow = failingSharedDestination.asView();
    propagatingSourceProbe->failNext = true;
    FATP_ASSERT_THROWS((failingSharedDestination = std::move(failingSharedSource)), std::bad_alloc,
                       "Propagating rank-zero assignment should allocate before committing allocator state");
    FATP_ASSERT_TRUE(failingSharedDestination.get_allocator().probe() == propagatingDestinationProbe,
                     "Failed propagating assignment preserves the destination allocator");
    FATP_ASSERT_EQ(failingSharedDestination().value, 5,
                   "Failed propagating assignment preserves the destination value");
    FATP_ASSERT_EQ(failingDestinationBorrow().value, 5,
                   "Failed propagating assignment preserves destination borrows");
    FATP_ASSERT_TRUE(failingSharedSource.get_allocator().probe() == propagatingSourceProbe,
                     "Failed propagating assignment preserves the source allocator");
    FATP_ASSERT_EQ(failingSharedSource().value, 25,
                   "Failed propagating assignment preserves the shared source value");
    FATP_ASSERT_EQ(failingSharedAlias().value, 25,
                   "Failed propagating assignment preserves shared aliases");
    FATP_ASSERT_EQ(failingSharedBorrow().value, 25,
                   "Failed propagating assignment preserves source borrows");

    auto uniqueSourceProbe = std::make_shared<AllocationProbe>();
    PropagatingFailingScalar failingUniqueSource(
        std::allocator_arg, FailingAllocator<MoveObserved, true>(uniqueSourceProbe),
        RankedExtents<0>{}, MoveObserved(27));
    uniqueSourceProbe->failNext = true;
    FATP_ASSERT_THROWS((failingSharedDestination = std::move(failingUniqueSource)), std::bad_alloc,
                       "A propagating allocation failure should precede moving a unique source element");
    FATP_ASSERT_EQ(failingUniqueSource().value, 27,
                   "Failed propagating allocation leaves a unique source element unchanged");
    FATP_ASSERT_TRUE(failingSharedDestination.get_allocator().probe() == propagatingDestinationProbe,
                     "Repeated propagating failure still preserves the destination allocator");
    FATP_ASSERT_EQ(failingDestinationBorrow().value, 5,
                   "Repeated propagating failure keeps destination borrows alive");

    auto compatibleProbe = std::make_shared<AllocationProbe>();
    using CompatibleScalar = Tensor<MoveObserved, FailingAllocator<MoveObserved>, 0>;
    CompatibleScalar compatibleUniqueSource(
        std::allocator_arg, FailingAllocator<MoveObserved>(compatibleProbe), RankedExtents<0>{}, MoveObserved(31));
    CompatibleScalar compatibleUniqueDestination(
        std::allocator_arg, FailingAllocator<MoveObserved>(compatibleProbe), RankedExtents<0>{}, MoveObserved(3));
    compatibleUniqueDestination = std::move(compatibleUniqueSource);
    FATP_ASSERT_EQ(compatibleUniqueDestination().value, 31,
                   "Compatible rank-zero assignment moves a uniquely owned value");
    FATP_ASSERT_EQ(compatibleUniqueSource().value, -1,
                   "A unique compatible rank-zero source contains its moved-from value");

    CompatibleScalar compatibleSharedSource(
        std::allocator_arg, FailingAllocator<MoveObserved>(compatibleProbe), RankedExtents<0>{}, MoveObserved(41));
    auto compatibleAlias = compatibleSharedSource.asSharedView();
    CompatibleScalar compatibleSharedDestination(
        std::allocator_arg, FailingAllocator<MoveObserved>(compatibleProbe), RankedExtents<0>{}, MoveObserved(4));
    compatibleSharedDestination = std::move(compatibleSharedSource);
    FATP_ASSERT_EQ(compatibleSharedDestination().value, 41,
                   "Compatible rank-zero assignment materializes the destination value");
    FATP_ASSERT_EQ(compatibleSharedSource().value, 41,
                   "Compatible rank-zero assignment copies a source with shared aliases");
    FATP_ASSERT_EQ(compatibleAlias().value, 41,
                   "Compatible rank-zero assignment preserves shared alias values");

    RankedTensor<MoveObserved, 0> crossRankUnique(RankedExtents<0>{}, MoveObserved(51));
    auto dynamicUnique = toDynamicTensor(std::move(crossRankUnique));
    FATP_ASSERT_EQ(dynamicUnique().value, 51,
                   "Rank-zero-to-dynamic conversion moves a uniquely owned value");
    FATP_ASSERT_EQ(crossRankUnique().value, -1,
                   "A unique rank-zero cross-rank source contains its moved-from value");

    RankedTensor<MoveObserved, 0> crossRankShared(RankedExtents<0>{}, MoveObserved(61));
    auto crossRankAlias = crossRankShared.asSharedView();
    auto dynamicShared = toDynamicTensor(std::move(crossRankShared));
    FATP_ASSERT_EQ(dynamicShared().value, 61,
                   "Rank-zero-to-dynamic conversion materializes the destination value");
    FATP_ASSERT_EQ(crossRankShared().value, 61,
                   "Rank-zero-to-dynamic conversion copies a source with shared aliases");
    FATP_ASSERT_EQ(crossRankAlias().value, 61,
                   "Rank-zero-to-dynamic conversion preserves shared alias values");

    using NonCopyableScalar = Tensor<MoveOnlyObserved, TaggedAllocator<MoveOnlyObserved>, 0>;
    NonCopyableScalar nonCopyableSource(
        std::allocator_arg, TaggedAllocator<MoveOnlyObserved>(105), RankedExtents<0>{});
    nonCopyableSource().value = 71;
    auto nonCopyableAlias = nonCopyableSource.asSharedView();
    auto nonCopyableBorrow = nonCopyableSource.asView();
    NonCopyableScalar nonCopyableDestination(
        std::allocator_arg, TaggedAllocator<MoveOnlyObserved>(106), RankedExtents<0>{});
    nonCopyableDestination().value = 7;
    auto destinationBorrow = nonCopyableDestination.asView();
    FATP_ASSERT_THROWS((nonCopyableDestination = std::move(nonCopyableSource)), std::logic_error,
                       "Shared non-copyable rank-zero assignment should fail before mutation");
    FATP_ASSERT_EQ(nonCopyableSource().value, 71,
                   "Rejected shared non-copyable assignment preserves the source owner");
    FATP_ASSERT_EQ(nonCopyableAlias().value, 71,
                   "Rejected shared non-copyable assignment preserves the shared alias");
    FATP_ASSERT_EQ(nonCopyableBorrow().value, 71,
                   "Rejected shared non-copyable assignment preserves source borrows");
    FATP_ASSERT_EQ(nonCopyableDestination().value, 7,
                   "Rejected shared non-copyable assignment preserves the destination owner");
    FATP_ASSERT_EQ(destinationBorrow().value, 7,
                   "Rejected shared non-copyable assignment preserves destination borrows");
    FATP_ASSERT_EQ(nonCopyableSource.get_allocator().identifier(), 105,
                   "Rejected shared non-copyable assignment preserves the source allocator");
    FATP_ASSERT_EQ(nonCopyableDestination.get_allocator().identifier(), 106,
                   "Rejected shared non-copyable assignment preserves the destination allocator");

    NonCopyableScalar nonCopyableCrossRank(
        std::allocator_arg, TaggedAllocator<MoveOnlyObserved>(107), RankedExtents<0>{});
    nonCopyableCrossRank().value = 81;
    auto nonCopyableCrossRankAlias = nonCopyableCrossRank.asSharedView();
    auto nonCopyableCrossRankBorrow = nonCopyableCrossRank.asView();
    FATP_ASSERT_THROWS(toDynamicTensor(std::move(nonCopyableCrossRank)), std::logic_error,
                       "Shared non-copyable rank-zero conversion should fail before mutation");
    FATP_ASSERT_EQ(nonCopyableCrossRank().value, 81,
                   "Rejected shared non-copyable conversion preserves the source owner");
    FATP_ASSERT_EQ(nonCopyableCrossRankAlias().value, 81,
                   "Rejected shared non-copyable conversion preserves the shared alias");
    FATP_ASSERT_EQ(nonCopyableCrossRankBorrow().value, 81,
                   "Rejected shared non-copyable conversion preserves source borrows");
    FATP_ASSERT_EQ(nonCopyableCrossRank.get_allocator().identifier(), 107,
                   "Rejected shared non-copyable conversion preserves the source allocator");
    return true;
}

FATP_TEST_CASE(borrowed_and_shared_ranked_lifetimes)
{
    std::optional<RankedTensorView<int, 2>> borrowed;
    std::optional<SharedRankedTensorView<int, 2>> shared;
    {
        RankedTensor<int, 2> owner({2, 2}, 6);
        borrowed.emplace(owner.asView());
        shared.emplace(owner.asSharedView());
    }
    FATP_ASSERT_EQ((*shared)(1, 1), 6,
                   "A shared ranked view retains element storage after owner destruction");
#ifndef NDEBUG
    FATP_ASSERT_THROWS((*borrowed)(0, 0), std::runtime_error,
                       "A tracked borrowed ranked view detects owner expiry");
#endif
    return true;
}

FATP_TEST_CASE(allocation_failure_preserves_rank_zero_source)
{
    auto probe = std::make_shared<AllocationProbe>();
    using ProbeScalar = Tensor<int, FailingAllocator<int>, 0>;
    {
        ProbeScalar source(std::allocator_arg, FailingAllocator<int>(probe), RankedExtents<0>{}, 31);
        auto borrowed = source.asView();
        FATP_ASSERT_EQ(probe->allocations, std::size_t{1},
                       "Rank-zero owner performs one element-storage allocation");
        probe->failNext = true;
        FATP_ASSERT_THROWS((ProbeScalar(std::move(source))), std::bad_alloc,
                           "Injected rank-zero move allocation failure propagates");
        FATP_ASSERT_EQ(source(), 31,
                       "Failed rank-zero move leaves the source element unchanged");
        FATP_ASSERT_EQ(borrowed(), 31,
                       "Failed rank-zero move leaves existing borrowed views alive");
        FATP_ASSERT_TRUE(source.get_allocator().probe() == probe,
                         "Failed rank-zero move preserves source allocator state");
    }
    FATP_ASSERT_EQ(probe->allocations, probe->deallocations,
                   "Rank-zero owner releases every successful element allocation");
    return true;
}

FATP_TEST_CASE(view_transforms_and_adapters)
{
    RankedTensor<int, 3> source({2, 1, 3}, 5);
    const auto permuted = source.permuteView({2, 1, 0});
    static_assert(tensor_static_rank_v<decltype(permuted)> == 3);
    const auto unsqueezed = source.unsqueezeView(0);
    static_assert(tensor_static_rank_v<decltype(unsqueezed)> == 4);
    const auto squeezed = source.squeezeView<1>();
    static_assert(tensor_static_rank_v<decltype(squeezed)> == 2);
    const auto runtimeSqueezed = source.squeezeView();
    static_assert(tensor_static_rank_v<decltype(runtimeSqueezed)> == kDynamicTensorRank);
    const auto sliced = source.sliceView(All, std::ptrdiff_t{0}, NewAxis, All);
    static_assert(tensor_static_rank_v<decltype(sliced)> == 3);
    const auto finalPlane = source.sliceView(std::ptrdiff_t{-1}, All, All);
    FATP_ASSERT_EQ(finalPlane(0, 2), 5,
                   "Typed slicing preserves signed negative-index normalization");
    const auto narrowUnsignedPlane = source.sliceView(std::uint8_t{1}, All, All);
    FATP_ASSERT_EQ(narrowUnsignedPlane(0, 2), 5,
                   "Typed slicing accepts representable narrow unsigned indices");
    using UnsignedDifference = std::make_unsigned_t<std::ptrdiff_t>;
    constexpr auto firstUnrepresentable =
        static_cast<UnsignedDifference>(std::numeric_limits<std::ptrdiff_t>::max()) + 1;
    FATP_ASSERT_THROWS(source.sliceView(firstUnrepresentable, All, All), std::overflow_error,
                       "Typed slicing rejects the first unsigned index above ptrdiff_t");
    FATP_ASSERT_THROWS(source.sliceView(std::numeric_limits<UnsignedDifference>::max(), All, All),
                       std::overflow_error,
                       "Typed slicing rejects unsigned indices that exceed ptrdiff_t");
    const SliceSpec runtimeIndex = std::ptrdiff_t{1};
    const auto runtimeIndexSlice = source.sliceView(std::vector<SliceSpec>{runtimeIndex});
    static_assert(tensor_static_rank_v<decltype(runtimeIndexSlice)> == kDynamicTensorRank);
    FATP_ASSERT_TRUE(runtimeIndexSlice.rank() == 2 && runtimeIndexSlice.extents() == DynamicExtents({1, 3}) &&
                         runtimeIndexSlice(0, 2) == 5,
                     "Runtime SliceSpec index alternatives use the dynamic-rank slice path");
    const SliceSpec runtimeNewAxis = NewAxis;
    const auto runtimeNewAxisSlice = source.sliceView({runtimeNewAxis});
    static_assert(tensor_static_rank_v<decltype(runtimeNewAxisSlice)> == kDynamicTensorRank);
    FATP_ASSERT_TRUE(runtimeNewAxisSlice.rank() == 4 &&
                         runtimeNewAxisSlice.extents() == DynamicExtents({1, 2, 1, 3}),
                     "Runtime SliceSpec new-axis alternatives use the initializer-list path");
    const auto reshaped = source.reshapeView(RankedExtents<2>{2, 3});
    static_assert(tensor_static_rank_v<decltype(reshaped)> == 2);
    const auto runtimeReshaped = source.reshapeView(DynamicExtents{3, 2});
    static_assert(tensor_static_rank_v<decltype(runtimeReshaped)> == kDynamicTensorRank);
    FATP_ASSERT_TRUE(runtimeReshaped.extents() == DynamicExtents({3, 2}) && runtimeReshaped(2, 1) == 5,
                     "A runtime reshape target returns a dynamic view from a ranked owner");
    const auto sharedRuntimeReshaped = source.asSharedView().reshapeView(DynamicExtents{6});
    static_assert(tensor_static_rank_v<decltype(sharedRuntimeReshaped)> == kDynamicTensorRank);
    FATP_ASSERT_EQ(sharedRuntimeReshaped(5), 5,
                   "A shared ranked view preserves lifetime through a dynamic-rank reshape");
    const auto broadcast = source.broadcastView(RankedExtents<4>{4, 2, 1, 3});
    static_assert(tensor_static_rank_v<decltype(broadcast)> == 4);
    FATP_ASSERT_EQ(broadcast(3, 1, 0, 2), 5, "Ranked broadcast preserves values");

    auto dynamicView = asDynamicView(source);
    static_assert(tensor_static_rank_v<decltype(dynamicView)> == kDynamicTensorRank);
    auto rankedAgain = asRankedView<3>(dynamicView);
    FATP_ASSERT_EQ(rankedAgain(1, 0, 2), 5, "Borrowed adapters preserve the mapping");
    FATP_ASSERT_THROWS(asRankedView<2>(dynamicView), std::invalid_argument,
                       "Dynamic-to-ranked adapter checks rank before publishing a view");
    const auto constRankedWrapper = source.asView();
    auto shallowDynamic = asDynamicView(constRankedWrapper);
    static_assert(std::same_as<decltype(shallowDynamic), TensorView<int>>);
    const auto constDynamicWrapper = dynamicView;
    auto shallowRanked = asRankedView<3>(constDynamicWrapper);
    static_assert(std::same_as<decltype(shallowRanked), RankedTensorView<int, 3>>);
    shallowRanked(0, 0, 0) = 12;
    FATP_ASSERT_EQ(source(0, 0, 0), 12,
                   "View adapter constness follows the element type, not wrapper constness");

    auto shared = source.asSharedView();
    auto dynamicShared = asDynamicSharedView(shared);
    auto rankedShared = asRankedSharedView<3>(dynamicShared);
    FATP_ASSERT_EQ(rankedShared(0, 0, 1), 5, "Shared adapters retain the storage lifetime");
    auto directDynamicShared = asDynamicSharedView(source);
    Tensor<int> dynamicOwner({2, 1, 3}, 5);
    auto directRankedShared = asRankedSharedView<3>(dynamicOwner);
    FATP_ASSERT_EQ(directDynamicShared(1, 0, 1), 5,
                   "Owner-to-dynamic shared adaptation retains ranked storage");
    FATP_ASSERT_EQ(directRankedShared(0, 0, 2), 5,
                   "Owner-to-ranked shared adaptation retains dynamic storage");
    return true;
}

FATP_TEST_CASE(arithmetic_reductions_and_selection_result_ranks)
{
    RankedTensor<int, 3> left({2, 2, 3}, 2);
    RankedTensor<int, 3> right({1, 2, 3}, 3);
    auto added = add(left, right);
    auto scaled = multiply(added, 2);
    auto converted = cast<double>(scaled);
    auto negated = negate(left);
    auto transformed = transform(left, [](int value) { return value + 1; });
    auto reshapedCopy = reshapeCopy(left, RankedExtents<2>{4, 3});
    auto explicitClone = clone(left, TensorAllocator<int>{});
    static_assert(tensor_static_rank_v<decltype(added)> == 3);
    static_assert(tensor_static_rank_v<decltype(scaled)> == 3);
    static_assert(tensor_static_rank_v<decltype(converted)> == 3);
    static_assert(tensor_static_rank_v<decltype(negated)> == 3);
    static_assert(tensor_static_rank_v<decltype(transformed)> == 3);
    static_assert(tensor_static_rank_v<decltype(reshapedCopy)> == 2);
    static_assert(tensor_static_rank_v<decltype(explicitClone)> == 3);
    FATP_ASSERT_EQ(converted(1, 1, 2), 10.0, "Ranked arithmetic broadcasts and preserves rank");

    Tensor<int> dynamic({1, 2, 3}, 1);
    auto mixed = add(left, dynamic);
    static_assert(tensor_static_rank_v<decltype(mixed)> == kDynamicTensorRank);
    RankedTensor<int, 1> vector({3}, 4);
    auto unequalRank = add(left, vector);
    static_assert(tensor_static_rank_v<decltype(unequalRank)> == 3);
    FATP_ASSERT_EQ(unequalRank(1, 1, 2), 6,
                   "Unequal ranked operands broadcast to the maximum static rank");

    auto reduced = sum<false, 0, 2>(left);
    auto kept = sum<true, 0, 2>(left);
    auto scalar = sum<false>(left);
    auto reducedProduct = product<false, 0, 2>(left);
    auto reducedMean = mean<false, 0, 2>(left);
    auto reducedMin = min<false, 0, 2>(left);
    auto reducedMax = max<false, 0, 2>(left);
    auto reducedArgmin = argmin<false, 0, 2>(left);
    auto reducedArgmax = argmax<false, 0, 2>(left);
    auto reducedAll = all<false, 0, 2>(left);
    auto reducedAny = any<false, 0, 2>(left);
    auto plainSum = sum(left);
    auto plainProduct = product(left);
    auto plainMean = mean(left);
    auto plainMin = min(left);
    auto plainMax = max(left);
    auto plainArgmin = argmin(left);
    auto plainArgmax = argmax(left);
    auto plainAll = all(left);
    auto plainAny = any(left);
    auto plainAllocatedSum = sum(left, TensorAllocator<std::int64_t>{});
    static_assert(tensor_static_rank_v<decltype(reduced)> == 1);
    static_assert(tensor_static_rank_v<decltype(kept)> == 3);
    static_assert(tensor_static_rank_v<decltype(scalar)> == 0);
    static_assert(tensor_static_rank_v<decltype(reducedProduct)> == 1);
    static_assert(tensor_static_rank_v<decltype(reducedMean)> == 1);
    static_assert(tensor_static_rank_v<decltype(reducedMin)> == 1);
    static_assert(tensor_static_rank_v<decltype(reducedMax)> == 1);
    static_assert(tensor_static_rank_v<decltype(reducedArgmin)> == 1);
    static_assert(tensor_static_rank_v<decltype(reducedArgmax)> == 1);
    static_assert(tensor_static_rank_v<decltype(reducedAll)> == 1);
    static_assert(tensor_static_rank_v<decltype(reducedAny)> == 1);
    static_assert(tensor_static_rank_v<decltype(plainSum)> == 0);
    static_assert(tensor_static_rank_v<decltype(plainProduct)> == 0);
    static_assert(tensor_static_rank_v<decltype(plainMean)> == 0);
    static_assert(tensor_static_rank_v<decltype(plainMin)> == 0);
    static_assert(tensor_static_rank_v<decltype(plainMax)> == 0);
    static_assert(tensor_static_rank_v<decltype(plainArgmin)> == 0);
    static_assert(tensor_static_rank_v<decltype(plainArgmax)> == 0);
    static_assert(tensor_static_rank_v<decltype(plainAll)> == 0);
    static_assert(tensor_static_rank_v<decltype(plainAny)> == 0);
    static_assert(tensor_static_rank_v<decltype(plainAllocatedSum)> == 0);
    auto runtimeReduced = sum(left, std::vector<TensorAxis>{0}, false);
    static_assert(tensor_static_rank_v<decltype(runtimeReduced)> == kDynamicTensorRank);
    FATP_ASSERT_EQ(reduced(1), 12, "Typed reduction removes selected axes");
    FATP_ASSERT_EQ(scalar(), 24, "Typed empty-axis-list reduction reduces all axes");
    FATP_ASSERT_EQ(reducedProduct(0), 64, "Typed product uses the ranked reduction path");
    FATP_ASSERT_EQ(reducedMean(0), 2.0, "Typed mean uses the ranked reduction path");
    FATP_ASSERT_TRUE(reducedMin(0) == 2 && reducedMax(0) == 2 && reducedArgmin(0) == 0 &&
                         reducedArgmax(0) == 0 && reducedAll(0) && reducedAny(0),
                     "Every typed reduction family preserves its fixed output rank");
    FATP_ASSERT_TRUE(plainSum() == 24 && plainProduct() == 4'096 && plainMean() == 2.0 &&
                         plainMin() == 2 && plainMax() == 2 && plainArgmin() == 0 &&
                         plainArgmax() == 0 && plainAll() && plainAny(),
                     "Plain fixed-rank reductions reduce all axes into a rank-zero owner");
    FATP_ASSERT_EQ(plainAllocatedSum(), std::int64_t{24},
                   "Explicit-allocator fixed-rank reduce-all retains rank zero");

    auto stacked = stack(left, left, 0);
    auto concatenated = concatenate(left, right, 0);
    const std::array<std::ptrdiff_t, 1> selection{1};
    auto taken = take(left, selection, 0);
    RankedTensor<std::ptrdiff_t, 3> alongIndices({2, 1, 3}, 0);
    auto along = takeAlongAxis(left, alongIndices, 1);
    RankedTensor<std::ptrdiff_t, 2> tuples({2, 1}, 0);
    tuples[1] = 1;
    auto gathered = gatherND<1>(left, tuples);
    static_assert(tensor_static_rank_v<decltype(stacked)> == 4);
    static_assert(tensor_static_rank_v<decltype(concatenated)> == 3);
    static_assert(tensor_static_rank_v<decltype(taken)> == 3);
    static_assert(tensor_static_rank_v<decltype(along)> == 3);
    static_assert(tensor_static_rank_v<decltype(gathered)> == 3);

    left += right;
    RankedTensor<int, 4> rankTooHigh({2, 2, 2, 3}, 1);
    FATP_ASSERT_THROWS(left += rankTooHigh, std::invalid_argument,
                       "Compound arithmetic reports an incompatible higher fixed rank at runtime");
    FATP_ASSERT_EQ(left(1, 1, 2), 5,
                   "Ranked compound assignment retains the destination family and shape");
    return true;
}

FATP_TEST_CASE(static_axis_mean_validation_and_empty_output)
{
    const auto largest = std::numeric_limits<std::size_t>::max();
    const auto unreachable = RankedTensorView<const int, 3>::borrow(
        nullptr, RankedTensorLayout<3>(0, 0, RankedExtents<3>{0, largest, 2},
                                       std::array<std::ptrdiff_t, 3>{0, 0, 0}));
    const auto result = mean<false, 1, 2>(unreachable);
    static_assert(tensor_static_rank_v<decltype(result)> == 1);
    FATP_ASSERT_TRUE(result.empty() && result.extents() == RankedExtents<1>{0},
                     "Typed mean should not count unreachable domains for an empty output");

    const RankedTensor<int, 2> emptyDomains({2, 0});
    FATP_ASSERT_THROWS((mean<false, 1>(emptyDomains)), std::domain_error,
                       "Typed mean should reject reachable empty reduction domains");

#ifndef NDEBUG
    std::optional<RankedTensorView<const int, 2>> expired;
    {
        const RankedTensor<int, 2> owner({2, 0});
        expired.emplace(owner.asConstView());
    }
    FATP_ASSERT_THROWS((mean<false, 1>(*expired)), std::runtime_error,
                       "Typed mean should validate an expired borrow before inspecting its domain");
#endif
    return true;
}

FATP_TEST_CASE(linear_algebra_contractions_and_execution)
{
    RankedTensor<int, 2> left({2, 3}, 1);
    RankedTensor<int, 2> right({3, 4}, 2);
    auto product = matmul(left, right);
    static_assert(tensor_static_rank_v<decltype(product)> == 2);
    FATP_ASSERT_EQ(product(1, 3), 6, "Ranked matmul computes the expected value");

    RankedTensor<int, 1> x({3}, 2);
    RankedTensor<int, 1> y({3}, 4);
    auto dotProduct = dot(x, y);
    auto outerProduct = outer(x, y);
    auto diagonalCopy = diagonal(left);
    auto traceValue = trace(left);
    static_assert(tensor_static_rank_v<decltype(dotProduct)> == 0);
    static_assert(tensor_static_rank_v<decltype(outerProduct)> == 2);
    static_assert(tensor_static_rank_v<decltype(diagonalCopy)> == 1);
    static_assert(tensor_static_rank_v<decltype(traceValue)> == 0);
    FATP_ASSERT_EQ(dotProduct(), 24, "Ranked dot returns a rank-zero owner");

    constexpr std::array<TensorAxis, 1> leftAxes{1};
    constexpr std::array<TensorAxis, 1> rightAxes{0};
    auto contraction = tensorDot(left, right, leftAxes, rightAxes);
    static_assert(tensor_static_rank_v<decltype(contraction)> == 2);
    const TensorExecutionContext serial;
    auto explicitMatmul = matmul(left, right, serial);
    static_assert(tensor_static_rank_v<decltype(explicitMatmul)> == 2);
    auto explicitSerial = tensorDot(left, right, leftAxes, rightAxes, serial);
    static_assert(tensor_static_rank_v<decltype(explicitSerial)> == 2);
    FATP_ASSERT_TRUE(exactEqual(contraction, explicitSerial),
                     "Explicit serial execution preserves ranked result semantics");
    return true;
}

FATP_TEST_CASE(descriptor_equality_hash_and_static_conversion)
{
    RankedTensor<int, 2> ranked({2, 2}, 7);
    auto descriptor = describeTensor(ranked);
    static_assert(std::same_as<decltype(descriptor), RankedStridedTensorDescriptor<int, 2>>);
    FATP_ASSERT_EQ(descriptor.borrow()(1, 1), 7, "Ranked descriptor borrows the original mapping");
    const auto mutableViewObject = ranked.asView();
    auto mutableViewDescriptor = describeTensor(mutableViewObject);
    static_assert(std::same_as<decltype(mutableViewDescriptor),
                               RankedStridedTensorDescriptor<int, 2>>);
    mutableViewDescriptor.borrow()(0, 0) = 8;
    FATP_ASSERT_EQ(ranked(0, 0), 8,
                   "Const qualification of a view wrapper does not change element mutability");
    const auto sharedViewObject = ranked.asSharedView();
    auto sharedViewDescriptor = describeTensor(sharedViewObject);
    static_assert(std::same_as<decltype(sharedViewDescriptor),
                               RankedStridedTensorDescriptor<int, 2>>);
    sharedViewDescriptor.borrow()(0, 1) = 9;
    FATP_ASSERT_EQ(ranked(0, 1), 9,
                   "Const qualification of a shared-view wrapper preserves element mutability");
    auto nonConstSharedViewObject = ranked.asSharedView();
    auto nonConstSharedDescriptor = describeTensor(nonConstSharedViewObject);
    static_assert(std::same_as<decltype(nonConstSharedDescriptor),
                               RankedStridedTensorDescriptor<int, 2>>);

    Tensor<int> dynamic({2, 2}, 7);
    RankedTensor<int, 3> differentRank({1, 2, 2}, 7);
    static_assert(requires { ranked == differentRank; });
    FATP_ASSERT_TRUE(!(ranked == differentRank),
                     "Unequal compile-time ranks compare false without ill-formed metadata access");
    auto comparable = ranked.clone();
    comparable.fill(7);
    FATP_ASSERT_TRUE(comparable == dynamic, "Owner equality ignores rank family and layout representation");
    FATP_ASSERT_EQ(std::hash<decltype(comparable)>{}(comparable), std::hash<decltype(dynamic)>{}(dynamic),
                   "Hashing depends on logical shape and values, not rank family");

    StaticTensor<int, Matrix<2, 2>> fixed{1, 2, 3, 4};
    auto converted = toRankedTensor(fixed);
    static_assert(tensor_static_rank_v<decltype(converted)> == 2);
    FATP_ASSERT_EQ(converted(1, 1), 4, "StaticTensor exact shape converts to RankedTensor");
    auto fixedAgain = toStaticTensor<Matrix<2, 2>>(ranked);
    FATP_ASSERT_EQ(fixedAgain.at(1, 1), 7,
                   "RankedTensor converts to an exactly matching StaticTensor shape");
    return true;
}

} // namespace fat_p::testing::tensorranked

namespace fat_p::testing
{

bool test_TensorRanked()
{
    FATP_PRINT_HEADER(TENSOR FIXED RANK RUNTIME EXTENTS)
    TestRunner runner;
    FATP_RUN_TEST_NS(runner, tensorranked, extents_layout_and_rank_storage);
    FATP_RUN_TEST_NS(runner, tensorranked, rank_family_differential_matrix);
    FATP_RUN_TEST_NS(runner, tensorranked, layout_differential_boundaries);
    FATP_RUN_TEST_NS(runner, tensorranked, owner_defaults_moves_and_views);
    FATP_RUN_TEST_NS(runner, tensorranked, rank_zero_move_preserves_shared_alias_values);
    FATP_RUN_TEST_NS(runner, tensorranked, allocation_failure_preserves_rank_zero_source);
    FATP_RUN_TEST_NS(runner, tensorranked, borrowed_and_shared_ranked_lifetimes);
    FATP_RUN_TEST_NS(runner, tensorranked, view_transforms_and_adapters);
    FATP_RUN_TEST_NS(runner, tensorranked, arithmetic_reductions_and_selection_result_ranks);
    FATP_RUN_TEST_NS(runner, tensorranked, static_axis_mean_validation_and_empty_output);
    FATP_RUN_TEST_NS(runner, tensorranked, linear_algebra_contractions_and_execution);
    FATP_RUN_TEST_NS(runner, tensorranked, descriptor_equality_hash_and_static_conversion);
    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_TensorRanked() ? 0 : 1;
}
#endif
