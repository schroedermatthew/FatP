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
#include <cstdlib>
#include <iterator>
#include <limits>
#include <memory>
#include <new>
#include <numeric>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#if defined(ENABLE_TEST_APPLICATION) && !defined(FATP_TENSOR_DISABLE_ALLOCATION_PROBE) && \
    (!defined(_MSC_VER) || _ITERATOR_DEBUG_LEVEL == 0)
// Scope injection to standalone runs. MSVC checked vector iterators can allocate
// inside noexcept moves; their proxy machinery is outside this metadata probe.
namespace fat_p::testing::tensor_view::allocation_probe
{
thread_local std::ptrdiff_t failIndex = -1;

void* allocate(std::size_t bytes)
{
    if (failIndex >= 0 && failIndex-- == 0)
    {
        failIndex = -1;
        throw std::bad_alloc();
    }
    if (void* storage = std::malloc(bytes == 0 ? 1 : bytes))
    {
        return storage;
    }
    throw std::bad_alloc();
}

struct ScopedFailure
{
    explicit ScopedFailure(std::ptrdiff_t index) noexcept { failIndex = index; }
    ~ScopedFailure() { failIndex = -1; }
    ScopedFailure(const ScopedFailure&) = delete;
    ScopedFailure& operator=(const ScopedFailure&) = delete;
};
} // namespace fat_p::testing::tensor_view::allocation_probe

void* operator new(std::size_t bytes) { return fat_p::testing::tensor_view::allocation_probe::allocate(bytes); }
void* operator new[](std::size_t bytes) { return fat_p::testing::tensor_view::allocation_probe::allocate(bytes); }
void operator delete(void* storage) noexcept { std::free(storage); }
void operator delete[](void* storage) noexcept { std::free(storage); }
void operator delete(void* storage, std::size_t) noexcept { std::free(storage); }
void operator delete[](void* storage, std::size_t) noexcept { std::free(storage); }
#endif

namespace fat_p::testing::tensor_view
{

template <typename Owner>
concept RvalueBorrow = requires(Owner&& owner) { std::move(owner).asView(); };

static_assert(!RvalueBorrow<Tensor<int>>);
static_assert(std::same_as<decltype(std::declval<const Tensor<int>&>().rowView(0)), TensorView<const int>>);
static_assert(std::same_as<decltype(std::declval<Tensor<int>&>().rowView(0)), TensorView<int>>);
static_assert(std::same_as<decltype(std::declval<const Tensor<int>&>().broadcastView(DynamicExtents{1})),
                           TensorView<const int>>);
static_assert(std::forward_iterator<TensorLogicalIterator<int>>);
static_assert(std::forward_iterator<TensorLogicalIterator<const int, 0>>);
static_assert(std::forward_iterator<TensorLogicalIterator<int, 6>>);
static_assert(std::indirectly_writable<TensorLogicalIterator<int>, int>);
static_assert(!std::indirectly_writable<TensorLogicalIterator<const int>, int>);

template <std::size_t Rank>
bool verifyIteratorCoordinates(const BasicTensorLayout<Rank>& layout)
{
    std::vector<int> storage(layout.storageLength() + 1);
    std::iota(storage.begin(), storage.end(), 11);
    std::vector<std::ptrdiff_t> expected;
    // Independent nested coordinate enumeration: neither logicalOffset nor the
    // iteration plan participates in this oracle.
    const auto enumerate = [&](const auto& self, std::size_t axis, std::ptrdiff_t offset) -> void {
        if (axis == layout.rank())
        {
            expected.push_back(offset);
            return;
        }
        for (std::size_t coordinate = 0; coordinate < layout.extents()[axis]; ++coordinate)
        {
            self(self, axis + 1, offset + static_cast<std::ptrdiff_t>(coordinate) * layout.strides()[axis]);
        }
    };
    if (layout.logicalSize() != 0)
    {
        enumerate(enumerate, 0, layout.originOffset());
    }
    const auto view = TensorView<const int, Rank>::borrow(storage.data(), layout);
    auto current = view.begin();
    const auto end = view.end();
    for (std::size_t index = 0; index < expected.size(); ++index)
    {
        FATP_ASSERT_TRUE(current != end, "Iterator must visit every oracle coordinate");
        FATP_ASSERT_EQ(*current, storage[expected[index]], "Iterator value agrees with coordinate enumeration");
        FATP_ASSERT_TRUE(current.operator->() == storage.data() + expected[index],
                         "Iterator arrow must address the physical oracle element");
        auto copy = current;
        TensorLogicalIterator<const int, Rank> assigned;
        assigned = current;
        auto positioned = TensorLogicalIterator<const int, Rank>(storage.data(), layout, index);
        FATP_ASSERT_TRUE(positioned == current && assigned == current,
                         "Construction and assignment preserve the iterator position and mapping");
        const auto previous = current++;
        FATP_ASSERT_TRUE(previous == copy, "Postincrement retains the independent previous position");
        FATP_ASSERT_EQ(*copy, storage[expected[index]], "Advancing an iterator cannot move its copies");
        ++assigned;
        ++positioned;
        FATP_ASSERT_TRUE(assigned == current && positioned == current,
                         "Copied and arbitrarily initialized cursors must advance consistently");
        if (index + 1 < expected.size())
        {
            FATP_ASSERT_EQ(*positioned, storage[expected[index + 1]],
                           "Arbitrary-position construction initializes carry coordinates");
        }
    }
    FATP_ASSERT_TRUE(current == end, "Last coordinate must advance to the logical end sentinel");
    FATP_ASSERT_THROWS((*current), std::out_of_range, "Empty and exhausted iterators reject dereference");
    return true;
}

FATP_TEST_CASE(iterator_coordinate_oracle)
{
    for (std::size_t rank = 0; rank <= 8; ++rank)
    {
        for (std::size_t variant = 0; variant < 12; ++variant)
        {
            std::vector<std::size_t> extents(rank);
            TensorStrides strides(rank);
            std::ptrdiff_t minimum = 0;
            std::ptrdiff_t maximum = 0;
            std::ptrdiff_t magnitude = 1;
            for (std::size_t reverseAxis = rank; reverseAxis > 0; --reverseAxis)
            {
                const auto axis = reverseAxis - 1;
                extents[axis] = 1 + (variant + axis * 7) % 3;
                strides[axis] = (variant + axis) % 2 == 0 ? magnitude : -magnitude;
                if (variant % 3 == 0 && axis % 2 == 0)
                {
                    strides[axis] = 0;
                }
                const auto contribution = static_cast<std::ptrdiff_t>(extents[axis] - 1) * strides[axis];
                minimum += contribution < 0 ? contribution : 0;
                maximum += contribution > 0 ? contribution : 0;
                magnitude *= static_cast<std::ptrdiff_t>(extents[axis] + 1);
            }
            const auto origin = 3 - minimum;
            const TensorLayout layout(static_cast<std::size_t>(origin + maximum + 1), origin,
                                      DynamicExtents(extents), strides);
            FATP_ASSERT_TRUE(verifyIteratorCoordinates(TensorLayout::contiguous(DynamicExtents(extents))),
                             "Contiguous cursor handles arbitrary ranks and singleton axes");
            FATP_ASSERT_TRUE(verifyIteratorCoordinates(layout),
                             "Dynamic-rank signed/padded/broadcast mappings match the coordinate oracle");
            switch (rank)
            {
            case 0: FATP_ASSERT_TRUE(verifyIteratorCoordinates(tensor_detail::makeFixedLayout<0>(layout)),
                                     "Fixed scalar iterator agrees with oracle"); break;
            case 2: FATP_ASSERT_TRUE(verifyIteratorCoordinates(tensor_detail::makeFixedLayout<2>(layout)),
                                     "Fixed rank-two iterator agrees with oracle"); break;
            case 6: FATP_ASSERT_TRUE(verifyIteratorCoordinates(tensor_detail::makeFixedLayout<6>(layout)),
                                     "Fixed high-rank iterator agrees with oracle"); break;
            default: break;
            }
        }
    }
    for (std::size_t rank = 1; rank <= 8; ++rank)
    {
        for (std::size_t emptyAxis = 0; emptyAxis < rank; ++emptyAxis)
        {
            std::vector<std::size_t> extents(rank, 2);
            extents[emptyAxis] = 0;
            const TensorLayout empty(0, 0, DynamicExtents(extents),
                                     TensorStrides(rank, std::numeric_limits<std::ptrdiff_t>::min()));
            FATP_ASSERT_TRUE(verifyIteratorCoordinates(empty),
                             "Empty mappings must never evaluate their unused extreme strides");
            if (rank == 6)
            {
                FATP_ASSERT_TRUE(verifyIteratorCoordinates(tensor_detail::makeFixedLayout<6>(empty)),
                                 "Fixed empty mappings avoid all offset arithmetic");
            }
        }
    }
    return true;
}

FATP_TEST_CASE(iterator_extreme_offsets_and_sentinel)
{
    int storage[]{11, 22};
    constexpr auto maximum = std::numeric_limits<std::ptrdiff_t>::max();
    // These pointer-free layouts describe enormous spans. Only offsets 0 and 1
    // are dereferenced against the actual array; advancing never forms pointers.
    const TensorLayout extreme(static_cast<std::size_t>(maximum), 0,
                               DynamicExtents{2, 2}, TensorStrides{1, maximum - 2});
    TensorLogicalIterator<const int> cursor(storage, extreme, 0);
    FATP_ASSERT_EQ(*cursor, 11, "Extreme-stride cursor starts at the logical origin");
    ++cursor;
    ++cursor;
    FATP_ASSERT_EQ(*cursor, 22, "Carry rewinds before advancing the outer coordinate");
    ++cursor;
    ++cursor;
    FATP_ASSERT_TRUE(cursor == TensorLogicalIterator<const int>(storage, extreme, 4),
                     "Final increment must not calculate an unrepresentable offset");
    FATP_ASSERT_THROWS((*cursor), std::out_of_range, "Extreme layout end cannot be dereferenced");

    const auto hugeCount = static_cast<std::size_t>(maximum);
    const TensorLayout broadcast(1, 0, DynamicExtents{hugeCount}, TensorStrides{0});
    TensorLogicalIterator<const int> huge(storage, broadcast, hugeCount - 2);
    FATP_ASSERT_EQ(*huge, 11, "Large broadcast coordinates leave the physical offset unchanged");
    ++huge;
    FATP_ASSERT_EQ(*huge, 11, "Last broadcast coordinate remains reachable");
    ++huge;
    FATP_ASSERT_TRUE(huge == TensorLogicalIterator<const int>(storage, broadcast, hugeCount),
                     "Maximum-size broadcast reaches end without coordinate overflow");

    const TensorLayout singleton(1, 0, DynamicExtents{1},
                                  TensorStrides{std::numeric_limits<std::ptrdiff_t>::min()});
    auto singletonCursor = TensorLogicalIterator<int>(storage, singleton, 0);
    *singletonCursor = 33;
    ++singletonCursor;
    FATP_ASSERT_EQ(storage[0], 33, "Mutable iterator dereference retains write-through semantics");
    FATP_ASSERT_THROWS((*singletonCursor), std::out_of_range,
                       "Singleton extreme strides are never used to advance past end");
    return true;
}

FATP_TEST_CASE(iterator_retains_debug_lifetime_checks)
{
#ifndef NDEBUG
    TensorLogicalIterator<int> borrowed;
    TensorLogicalIterator<int> borrowedEnd;
    {
        Tensor<int> owner({2}, 19);
        borrowed = owner.asView().begin();
        borrowedEnd = owner.asView().end();
        FATP_ASSERT_EQ(*borrowed, 19, "Tracked iterator is usable while its owner lives");
    }
    FATP_ASSERT_THROWS((*borrowed), std::runtime_error, "Cached offset cannot skip lifetime validation");
    FATP_ASSERT_THROWS(borrowed.operator->(), std::runtime_error, "Iterator arrow validates lifetime");
    FATP_ASSERT_THROWS((*borrowedEnd), std::runtime_error, "Lifetime failure precedes end validation");
#endif
    return true;
}

FATP_TEST_CASE(iterator_assignment_allocation_failure)
{
#if defined(ENABLE_TEST_APPLICATION) && !defined(FATP_TENSOR_DISABLE_ALLOCATION_PROBE) && \
    (!defined(_MSC_VER) || _ITERATOR_DEBUG_LEVEL == 0)
    std::vector<int> sourceStorage(128);
    std::iota(sourceStorage.begin(), sourceStorage.end(), 100);
    const TensorLayout sourceLayout(128, 0, DynamicExtents{2, 2, 2, 2, 2, 2}, {64, 32, 16, 8, 4, 1});
    const TensorLogicalIterator<const int> source(sourceStorage.data(), sourceLayout, 17);
    int targetStorage[]{200, 201, 202, 203, 204, 205, 206, 207};
    const TensorLayout targetLayout(8, 2, DynamicExtents{2, 3}, {4, -1});
    std::size_t failures = 0;
    bool succeeded = false;
    for (std::ptrdiff_t index = 0; index < 16 && !succeeded; ++index)
    {
        TensorLogicalIterator<const int> target(targetStorage, targetLayout, 1);
        const auto original = target;
        bool failed = false;
        try
        {
            allocation_probe::ScopedFailure injection(index);
            target = source;
        }
        catch (const std::bad_alloc&)
        {
            failed = true;
        }
        FATP_ASSERT_EQ(*source, 133, "Failed or successful iterator assignment never modifies its source");
        if (failed)
        {
            ++failures;
            FATP_ASSERT_TRUE(target == original, "Allocation failure preserves the destination equality domain");
            FATP_ASSERT_EQ(*target, 201, "Allocation failure preserves the destination element");
            ++target;
            FATP_ASSERT_EQ(*target, 200, "After failure the original reversed cursor must still advance");
        }
        else
        {
            succeeded = true;
            FATP_ASSERT_TRUE(target == source, "Successful assignment commits the source mapping and position");
            FATP_ASSERT_EQ(*target, 133, "Successful assignment updates the cached offset");
            ++target;
            FATP_ASSERT_EQ(*target, 136, "Successful assignment commits coordinates needed for the next carry");
        }
    }
    FATP_ASSERT_TRUE(failures > 0 && succeeded, "Sweep covers every metadata allocation and successful assignment");
#else
    std::cout << "[SKIP] Iterator allocation failure requires standalone replacement-new support\n";
#endif
    return true;
}

FATP_TEST_CASE(external_mapping_validation)
{
    Tensor<int> indexOwner({1}, 7);
    auto indexView = indexOwner.asView();
    const auto& constIndexView = indexView;
    FATP_ASSERT_THROWS(indexView(1ULL << 32), std::out_of_range, "View index must not truncate");
    FATP_ASSERT_THROWS(indexView(-1, 0), std::invalid_argument, "Rank validation precedes index conversion");
    FATP_ASSERT_THROWS(constIndexView(1ULL << 32), std::out_of_range, "Const view index must not truncate");
    FATP_ASSERT_THROWS(indexView.rowView(0), std::invalid_argument, "Row rank mismatch is invalid_argument");
    FATP_ASSERT_THROWS(indexView.columnView(0), std::invalid_argument, "Column rank mismatch is invalid_argument");
    Tensor<int> matrix({1, 1}, 7);
    FATP_ASSERT_THROWS(matrix.asView().rowView(1), std::out_of_range, "Row bounds remain out_of_range");
    FATP_ASSERT_THROWS(matrix.asView().columnView(1), std::out_of_range, "Column bounds remain out_of_range");
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
    auto storage = std::make_shared<int>(23);
    std::weak_ptr<int> weak = storage;
    std::shared_ptr<void> owningNull(storage, nullptr);
    auto aliased = SharedTensorView<int>::share(owningNull, storage.get(),
                                               TensorLayout::contiguous(DynamicExtents{1}));
    storage.reset();
    owningNull.reset();
    FATP_ASSERT_FALSE(weak.expired(), "Null stored pointer can still own the backing allocation");
    FATP_ASSERT_EQ(aliased[0], 23, "Owning null alias retains storage");
    int cell = 5;
    std::shared_ptr<void> nonowner(std::shared_ptr<void>{}, &cell);
    FATP_ASSERT_THROWS(SharedTensorView<int>::share(nonowner, &cell,
                          TensorLayout::contiguous(DynamicExtents{1})),
                       std::invalid_argument, "A stored pointer alone does not establish shared ownership");
    FATP_ASSERT_THROWS(aliased.rowView(0), std::invalid_argument, "Shared row access requires rank two");
    FATP_ASSERT_THROWS(aliased.columnView(0), std::invalid_argument, "Shared column access requires rank two");
    TensorView<int> borrowed;
    SharedTensorView<int> shared;
    {
        Tensor<int> owner({2}, 17);
        borrowed = owner.asView();
        shared = owner.asSharedView();
    }
    FATP_ASSERT_EQ(shared[0], 17, "Shared view should retain element storage");
#ifndef NDEBUG
    FATP_ASSERT_THROWS(borrowed.rowView(0), std::runtime_error, "Lifetime failure precedes rank validation");
    FATP_ASSERT_THROWS(borrowed.columnView(0), std::runtime_error, "Column access checks lifetime first");
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
    TensorLogicalIterator<int, 0> singularScalarIterator;
    FATP_ASSERT_THROWS((*singularScalarIterator), std::out_of_range,
                       "A default rank-zero iterator should diagnose singular dereference");

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

FATP_TEST_CASE(dynamic_view_moves_leave_empty_sources)
{
    static_assert(std::is_nothrow_move_constructible_v<TensorView<int>>);
    static_assert(std::is_nothrow_move_constructible_v<SharedTensorView<int>>);
    for (const std::size_t rank : {4U, 5U})
    {
        Tensor<int> owner(DynamicExtents(std::vector<std::size_t>(rank, 2)));
        for (std::size_t i = 0; i < owner.size(); ++i)
        {
            owner[i] = static_cast<int>(i);
        }
        for (const bool tracked : {false, true})
        {
            auto source = tracked ? owner.asView() : TensorView<int>::borrow(owner.data(), owner.layout());
            auto moved = std::move(source);
            FATP_ASSERT_TRUE(source.empty() && source.extents() == DynamicExtents{0},
                             "Dynamic moved view has a canonical empty layout at both storage ranks");
            FATP_ASSERT_TRUE(source.data() == nullptr && source.begin() == source.end(),
                             "Empty moved view has no dangling pointer or tracking token");
            FATP_ASSERT_THROWS(source[0], std::out_of_range, "Moved view cannot address original storage");
            FATP_ASSERT_EQ(moved[owner.size() - 1], static_cast<int>(owner.size() - 1),
                           "Transferred mapping still addresses the last element");
            source = std::move(moved);
            FATP_ASSERT_TRUE(moved.empty() && moved.data() == nullptr,
                             "Dynamic view move assignment empties its source");
            FATP_ASSERT_EQ(source[owner.size() - 1], static_cast<int>(owner.size() - 1),
                           "Move assignment preserves destination values");
        }
        SharedTensorView<int> retained;
        {
            Tensor<int> temporary(owner);
            auto shared = temporary.asSharedView();
            auto moved = std::move(shared);
            FATP_ASSERT_TRUE(shared.empty() && shared.data() == nullptr,
                             "Shared dynamic move empties its source");
            retained = std::move(moved);
            FATP_ASSERT_TRUE(moved.empty(), "Shared move assignment empties its source");
        }
        FATP_ASSERT_EQ(retained[owner.size() - 1], static_cast<int>(owner.size() - 1),
                       "Transferred shared view retains storage after owner destruction");
    }
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
    FATP_RUN_TEST_NS(runner, tensor_view, dynamic_view_moves_leave_empty_sources);
    FATP_RUN_TEST_NS(runner, tensor_view, iterator_identity_and_writable_layout_guards);
    FATP_RUN_TEST_NS(runner, tensor_view, iterator_coordinate_oracle);
    FATP_RUN_TEST_NS(runner, tensor_view, iterator_extreme_offsets_and_sentinel);
    FATP_RUN_TEST_NS(runner, tensor_view, iterator_retains_debug_lifetime_checks);
    FATP_RUN_TEST_NS(runner, tensor_view, iterator_assignment_allocation_failure);
    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_TensorView() ? 0 : 1;
}
#endif
