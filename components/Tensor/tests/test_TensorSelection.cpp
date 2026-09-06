/** @file test_TensorSelection.cpp @brief Tensor composition and indexed-selection tests. */

/*
FATP_META:
  meta_version: 1
  component: TensorSelection
  file_role: test
  path: components/Tensor/tests/test_TensorSelection.cpp
  namespace: fat_p::testing::tensor_selection
  layer: Testing
  summary: "Stack, concatenate, take, takeAlongAxis, and gatherND conformance tests."
  api_stability: in_work
  related:
    docs:
      - components/Tensor/docs/Design Note - Tensor Architecture Additions Plan.md
    headers:
      - include/fat_p/TensorSelection.h
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
#include "TensorSelection.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <numeric>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace fat_p::testing::tensor_selection
{

FATP_TEST_CASE(stack_pair_and_many)
{
    Tensor<int> first({2, 2});
    Tensor<int> second({2, 2});
    std::iota(first.begin(), first.end(), 1);
    std::iota(second.begin(), second.end(), 5);
    const auto paired = stack(first, second, 1);
    FATP_ASSERT_TRUE(paired.extents() == DynamicExtents({2, 2, 2}),
                     "Stack should insert an operand axis");
    FATP_ASSERT_TRUE(std::vector<int>(paired.begin(), paired.end()) ==
                         std::vector<int>({1, 2, 5, 6, 3, 4, 7, 8}),
                     "Stack should preserve each operand's logical order");

    Tensor<int> third({2}, 3);
    Tensor<int> fourth({2}, 4);
    Tensor<int> fifth({2}, 5);
    const std::array<std::reference_wrapper<const Tensor<int>>, 3> inputs = {
        std::cref(third), std::cref(fourth), std::cref(fifth)};
    const auto many = stack(std::span<const std::reference_wrapper<const Tensor<int>>>(inputs), -1);
    FATP_ASSERT_TRUE(many.extents() == DynamicExtents({2, 3}),
                     "Multi-input stack should normalize negative insertion axes");
    FATP_ASSERT_TRUE(std::vector<int>(many.begin(), many.end()) == std::vector<int>({3, 4, 5, 3, 4, 5}),
                     "Multi-input stack should address all inputs");
    const auto convenient = stack({std::cref(third), std::cref(fourth), std::cref(fifth)}, -1);
    FATP_ASSERT_TRUE(convenient.extents() == many.extents(),
                     "Initializer-list composition should avoid manual span construction");

    Tensor<int, std::allocator<int>> standardAllocated(std::allocator_arg, std::allocator<int>{},
                                                        DynamicExtents({2}), 6);
    const auto inherited = stack(standardAllocated, third);
    static_assert(std::same_as<typename decltype(inherited)::allocator_type, std::allocator<int>>);
    FATP_ASSERT_EQ(inherited(0, 0), 6,
                   "Composition should inherit the first owning operand's result allocator");
    return true;
}

FATP_TEST_CASE(concatenate_pair_many_and_strided)
{
    Tensor<int> owner({2, 3});
    std::iota(owner.begin(), owner.end(), 1);
    const auto transposed = owner.transposeView();
    Tensor<int> tail({3, 1}, 9);
    const auto paired = concatenate(transposed, tail, 1);
    FATP_ASSERT_TRUE(paired.extents() == DynamicExtents({3, 3}),
                     "Concatenate should add the selected extents");
    FATP_ASSERT_TRUE(std::vector<int>(paired.begin(), paired.end()) ==
                         std::vector<int>({1, 4, 9, 2, 5, 9, 3, 6, 9}),
                     "Concatenate should consume strided inputs logically");

    Tensor<int> empty({2, 0});
    Tensor<int> middle({2, 1}, 7);
    Tensor<int> end({2, 2}, 8);
    const std::array<std::reference_wrapper<const Tensor<int>>, 3> inputs = {
        std::cref(empty), std::cref(middle), std::cref(end)};
    const auto many = concatenate(std::span<const std::reference_wrapper<const Tensor<int>>>(inputs), 1);
    FATP_ASSERT_TRUE(many.extents() == DynamicExtents({2, 3}),
                     "Zero extents should contribute zero length during concatenate");
    FATP_ASSERT_TRUE(std::vector<int>(many.begin(), many.end()) == std::vector<int>({7, 8, 8, 7, 8, 8}),
                     "Multi-input concatenate should place each operand in its destination slice");
    return true;
}

FATP_TEST_CASE(concatenate_many_small_inputs)
{
    // This shape previously restarted an input-boundary scan for every element:
    // 32,768 singleton operands required over 536 million boundary advances.
    constexpr std::size_t count = 32768;
    Tensor<int, TensorAllocator<int>, 1> storage({count});
    std::iota(storage.begin(), storage.end(), 0);
    std::vector<TensorView<const int, 1>> pieces;
    pieces.reserve(count + 3);
    const auto empty = std::as_const(storage).sliceView({0}, {0});
    pieces.push_back(empty);
    for (std::size_t index = 0; index < count; ++index)
    {
        pieces.push_back(std::as_const(storage).sliceView({index}, {index + 1}));
        if (index == count / 2)
        {
            pieces.push_back(empty);
        }
    }
    pieces.push_back(empty);
    std::vector<std::reference_wrapper<const TensorView<const int, 1>>> inputs;
    inputs.reserve(pieces.size());
    for (const auto& piece : pieces)
    {
        inputs.emplace_back(piece);
    }
    const auto joined = concatenate(std::span<const std::reference_wrapper<const TensorView<const int, 1>>>(inputs), -1);
    static_assert(decltype(joined)::static_rank == 1);
    FATP_ASSERT_EQ(joined.size(), count, "Empty operands contribute no values");
    for (std::size_t index = 0; index < count; ++index)
    {
        FATP_ASSERT_EQ(joined[index], static_cast<int>(index), "Every singleton appears exactly once and in order");
    }
    return true;
}

// The oracle enumerates coordinates recursively and reads physical storage with
// a direct signed-stride dot product. It shares no index decoder with Selection.
using OracleShape = std::vector<std::size_t>;

template <typename Function>
void enumerateCoordinates(const OracleShape& shape, Function&& function)
{
    OracleShape coordinate(shape.size());
    const auto visit = [&](auto&& self, std::size_t axis) -> void {
        if (axis == shape.size())
        {
            function(coordinate);
            return;
        }
        for (coordinate[axis] = 0; coordinate[axis] < shape[axis]; ++coordinate[axis])
        {
            self(self, axis + 1);
        }
    };
    visit(visit, 0);
}

template <typename Result, typename Expected>
bool matchesCoordinates(const Result& result, const OracleShape& shape, Expected&& expected)
{
    if (OracleShape(result.extents().begin(), result.extents().end()) != shape)
    {
        return false;
    }
    bool matches = true;
    std::size_t linear = 0;
    enumerateCoordinates(shape, [&](const OracleShape& coordinate) {
        matches = (result[linear++] == expected(coordinate)) && matches;
    });
    return matches && linear == result.size();
}

bool checkSelectionCoordinates(const OracleShape& shape, int variant)
{
    const auto rank = shape.size();
    std::vector<std::ptrdiff_t> strides(rank);
    std::ptrdiff_t step = 2;
    std::ptrdiff_t minimum = 0;
    std::ptrdiff_t maximum = 0;
    for (std::size_t axis = rank; axis-- > 0;)
    {
        // Padded, alternating signed, overlapping, and broadcast read layouts.
        strides[axis] = variant == 0 ? step : variant == 1 ? (axis % 2 ? step : -step) : variant == 2 ? 1 : 0;
        step *= static_cast<std::ptrdiff_t>(std::max(std::size_t{1}, shape[axis]));
        const auto displacement = static_cast<std::ptrdiff_t>(shape[axis] ? shape[axis] - 1 : 0) * strides[axis];
        minimum += std::min(std::ptrdiff_t{0}, displacement);
        maximum += std::max(std::ptrdiff_t{0}, displacement);
    }
    const auto origin = -minimum;
    std::vector<int> storage(static_cast<std::size_t>(maximum - minimum + 1));
    std::iota(storage.begin(), storage.end(), 10);
    auto otherStorage = storage;
    for (auto& value : otherStorage)
    {
        value += 1000;
    }
    const TensorLayout layout(storage.size(), origin, DynamicExtents(shape), TensorStrides(strides));
    const auto source = TensorView<const int>::borrow(storage.data(), layout);
    const auto other = TensorView<const int>::borrow(otherStorage.data(), layout);
    const auto direct = [&](const OracleShape& coordinate) {
        auto offset = origin;
        for (std::size_t axis = 0; axis < rank; ++axis)
        {
            offset += static_cast<std::ptrdiff_t>(coordinate[axis]) * strides[axis];
        }
        return storage[static_cast<std::size_t>(offset)];
    };
    const std::array<std::reference_wrapper<const TensorView<const int>>, 3> references = {
        std::cref(source), std::cref(other), std::cref(source)};
    const auto inputs = std::span<const std::reference_wrapper<const TensorView<const int>>>(references);
    for (std::size_t axis = 0; axis <= rank; ++axis)
    {
        for (const auto requested : {static_cast<TensorAxis>(axis),
                                    static_cast<TensorAxis>(axis) - static_cast<TensorAxis>(rank + 1)})
        {
            OracleShape output(rank + 1);
            for (std::size_t current = 0; current <= rank; ++current)
            {
                output[current] = current == axis ? 2 : shape[current < axis ? current : current - 1];
            }
            const auto expected = [&](OracleShape coordinate) {
                const auto which = coordinate[axis];
                coordinate.erase(coordinate.begin() + static_cast<std::ptrdiff_t>(axis));
                return direct(coordinate) + (which == 1 ? 1000 : 0);
            };
            FATP_ASSERT_TRUE(matchesCoordinates(stack(source, other, requested), output, expected),
                             "Pair stack matches independent coordinates for every insertion axis");
            output[axis] = 3;
            FATP_ASSERT_TRUE(matchesCoordinates(stack(inputs, requested), output, expected),
                             "Multi-input stack matches independent coordinates");
        }
    }
    for (std::size_t axis = 0; axis < rank; ++axis)
    {
        for (const auto requested : {static_cast<TensorAxis>(axis),
                                    static_cast<TensorAxis>(axis) - static_cast<TensorAxis>(rank)})
        {
            auto output = shape;
            output[axis] *= 2;
            const auto expected = [&](OracleShape coordinate) {
                const auto which = coordinate[axis] / shape[axis];
                coordinate[axis] %= shape[axis];
                return direct(coordinate) + (which == 1 ? 1000 : 0);
            };
            FATP_ASSERT_TRUE(matchesCoordinates(concatenate(source, other, requested), output, expected),
                             "Pair concatenate matches independent coordinates");
            output[axis] = shape[axis] * inputs.size();
            FATP_ASSERT_TRUE(matchesCoordinates(concatenate(inputs, requested), output, expected),
                             "Multi-input concatenate matches signed-stride and broadcast coordinates");
        }
        std::vector<std::ptrdiff_t> selected;
        if (shape[axis] != 0)
        {
            selected = {-static_cast<std::ptrdiff_t>(shape[axis]), -1, 0,
                        static_cast<std::ptrdiff_t>(shape[axis] - 1)};
        }
        auto output = shape;
        output[axis] = selected.size();
        const auto expected = [&](OracleShape coordinate) {
            const auto index = selected[coordinate[axis]];
            coordinate[axis] = static_cast<std::size_t>(index < 0 ? index + static_cast<std::ptrdiff_t>(shape[axis]) : index);
            return direct(coordinate);
        };
        const auto taken = take(source, std::span<const std::ptrdiff_t>(selected),
                                static_cast<TensorAxis>(axis) - static_cast<TensorAxis>(rank));
        FATP_ASSERT_TRUE(matchesCoordinates(taken, output, expected), "Take matches independent coordinates");
        Tensor<std::int64_t> indices{DynamicExtents(output)};
        std::size_t next = 0;
        enumerateCoordinates(output, [&](const OracleShape& coordinate) { indices[next++] = selected[coordinate[axis]]; });
        FATP_ASSERT_TRUE(matchesCoordinates(takeAlongAxis(source, indices, static_cast<TensorAxis>(axis)), output, expected),
                         "Take-along-axis matches independent coordinates");
    }
    for (std::size_t depth = 0; depth <= rank; ++depth)
    {
        const auto prefixEnd = shape.begin() + static_cast<std::ptrdiff_t>(depth);
        const bool emptyPrefix = std::find(shape.begin(), prefixEnd, 0) != prefixEnd;
        const auto tuples = emptyPrefix ? std::size_t{0} : std::size_t{2};
        Tensor<std::int64_t> indices({tuples, depth});
        for (std::size_t tuple = 0; tuple < tuples; ++tuple)
        {
            for (std::size_t axis = 0; axis < depth; ++axis)
            {
                indices[tuple * depth + axis] = tuple ? -1 : -static_cast<std::int64_t>(shape[axis]);
            }
        }
        OracleShape output{tuples};
        output.insert(output.end(), prefixEnd, shape.end());
        FATP_ASSERT_TRUE(matchesCoordinates(gatherND(source, indices), output, [&](const OracleShape& coordinate) {
            OracleShape root(rank);
            for (std::size_t axis = 0; axis < depth; ++axis)
            {
                root[axis] = coordinate[0] ? shape[axis] - 1 : 0;
            }
            for (std::size_t axis = depth; axis < rank; ++axis)
            {
                root[axis] = coordinate[1 + axis - depth];
            }
            return direct(root);
        }), "Gather matches independent coordinates, including zero-depth and empty tuples");
    }
    return true;
}

FATP_TEST_CASE(selection_coordinate_oracle)
{
    for (std::size_t rank = 0, count = 1; rank <= 3; ++rank, count *= 4)
    {
        for (std::size_t code = 0; code < count; ++code)
        {
            OracleShape shape(rank);
            auto remaining = code;
            for (auto& extent : shape)
            {
                extent = remaining % 4;
                remaining /= 4;
            }
            for (int variant = 0; variant < 4; ++variant)
            {
                FATP_ASSERT_TRUE(checkSelectionCoordinates(shape, variant), "Exhaustive small-shape selection oracle");
            }
        }
    }
    return true;
}

FATP_TEST_CASE(take_and_take_along_axis)
{
    Tensor<int> source({3, 4});
    std::iota(source.begin(), source.end(), 1);
    const std::array<std::ptrdiff_t, 4> columns = {3, 1, 1, -4};
    const auto selected = take(source.transposeView(), std::span<const std::ptrdiff_t>(columns), 0);
    FATP_ASSERT_TRUE(selected.extents() == DynamicExtents({4, 3}),
                     "take should replace one source extent with the index count");
    FATP_ASSERT_TRUE(std::vector<int>(selected.begin(), selected.end()) ==
                         std::vector<int>({4, 8, 12, 2, 6, 10, 2, 6, 10, 1, 5, 9}),
                     "take should support duplicates, negatives, and strided sources");

    Tensor<std::ptrdiff_t> indices({3, 2});
    indices[0] = 0;
    indices[1] = -1;
    indices[2] = 2;
    indices[3] = 1;
    indices[4] = -1;
    indices[5] = 0;
    const auto along = takeAlongAxis(source, indices, 1);
    FATP_ASSERT_TRUE(along.extents() == DynamicExtents({3, 2}),
                     "takeAlongAxis output should match the indices tensor");
    FATP_ASSERT_TRUE(std::vector<int>(along.begin(), along.end()) == std::vector<int>({1, 4, 7, 6, 12, 9}),
                     "Each takeAlongAxis index should apply at its full output coordinate");

    Tensor<std::ptrdiff_t, std::allocator<std::ptrdiff_t>> allocatedIndices(
        std::allocator_arg, std::allocator<std::ptrdiff_t>{}, DynamicExtents{3, 1});
    allocatedIndices[0] = 0;
    allocatedIndices[1] = 1;
    allocatedIndices[2] = 2;
    const auto inherited = takeAlongAxis(source.asConstView(), allocatedIndices, 1);
    static_assert(std::same_as<typename decltype(inherited)::allocator_type, std::allocator<int>>);
    FATP_ASSERT_TRUE(std::vector<int>(inherited.begin(), inherited.end()) == std::vector<int>({1, 6, 11}),
                     "takeAlongAxis should select the indices owner when the source is a view");
    return true;
}

FATP_TEST_CASE(gather_nd_and_zero_depth)
{
    Tensor<int> source({2, 3, 2});
    std::iota(source.begin(), source.end(), 1);
    Tensor<int> tuples({3, 2});
    tuples[0] = 0;
    tuples[1] = 1;
    tuples[2] = -1;
    tuples[3] = 0;
    tuples[4] = 1;
    tuples[5] = -1;
    const auto gathered = gatherND(source, tuples);
    FATP_ASSERT_TRUE(gathered.extents() == DynamicExtents({3, 2}),
                     "gatherND should append unreplaced source dimensions");
    FATP_ASSERT_TRUE(std::vector<int>(gathered.begin(), gathered.end()) ==
                         std::vector<int>({3, 4, 7, 8, 11, 12}),
                     "gatherND should normalize each tuple component independently");

    Tensor<int> noComponents({2, 0});
    const auto duplicated = gatherND(source, noComponents);
    FATP_ASSERT_TRUE(duplicated.extents() == DynamicExtents({2, 2, 3, 2}),
                     "Zero-depth tuples should append the complete source shape");
    FATP_ASSERT_EQ(duplicated(0, 1, 2, 1), source(1, 2, 1),
                   "Each zero-depth tuple should select the complete source");
    FATP_ASSERT_EQ(duplicated(1, 1, 2, 1), source(1, 2, 1),
                   "Zero-depth gather should duplicate the complete source per tuple");

    Tensor<int> oneEmptyTuple({0});
    const auto oneCopy = gatherND(source, oneEmptyTuple);
    FATP_ASSERT_TRUE(oneCopy.extents() == source.extents(),
                     "A rank-one zero-depth index tensor represents one empty tuple");
    FATP_ASSERT_TRUE(std::vector<int>(oneCopy.begin(), oneCopy.end()) ==
                         std::vector<int>(source.begin(), source.end()),
                     "One empty tuple should select the complete source once");

    Tensor<int> noTuples({0, 1});
    const auto noCopies = gatherND(source, noTuples);
    FATP_ASSERT_TRUE(noCopies.extents() == DynamicExtents({0, 3, 2}) && noCopies.empty(),
                     "A zero prefix extent should produce no gathered tuples");

    Tensor<int, std::allocator<int>> allocatedTuples(
        std::allocator_arg, std::allocator<int>{}, DynamicExtents{1, 2});
    allocatedTuples[0] = 1;
    allocatedTuples[1] = 2;
    const auto inherited = gatherND(source.asConstView(), allocatedTuples);
    const auto typedInherited = gatherND<2>(source.asConstView(), allocatedTuples);
    static_assert(std::same_as<typename decltype(inherited)::allocator_type, std::allocator<int>>);
    static_assert(std::same_as<typename decltype(typedInherited)::allocator_type, std::allocator<int>>);
    FATP_ASSERT_TRUE(std::vector<int>(inherited.begin(), inherited.end()) == std::vector<int>({11, 12}),
                     "gatherND should select the indices owner when the source is a view");
    FATP_ASSERT_TRUE(std::vector<int>(typedInherited.begin(), typedInherited.end()) ==
                         std::vector<int>({11, 12}),
                     "Typed gatherND should use the same owner-selection rule");
    return true;
}

FATP_TEST_CASE(validation_errors)
{
    Tensor<int> matrix({2, 3}, 1);
    Tensor<int> other({2, 2}, 1);
    FATP_ASSERT_THROWS(stack(matrix, other), std::invalid_argument,
                       "Stack should reject different extents");
    FATP_ASSERT_THROWS(concatenate(matrix, other, 0), std::invalid_argument,
                       "Concatenate should reject mismatched non-axis extents");
    Tensor<int> scalar({}, 1);
    FATP_ASSERT_THROWS(concatenate(scalar, scalar), std::invalid_argument,
                       "Concatenate should reject rank-zero operands");

    const std::array<std::ptrdiff_t, 1> bad = {3};
    FATP_ASSERT_THROWS(take(matrix, std::span<const std::ptrdiff_t>(bad), 0), std::out_of_range,
                       "take should bounds-check every supplied index");
    Tensor<int> badAlong({1, 1}, 0);
    FATP_ASSERT_THROWS(takeAlongAxis(matrix, badAlong, 1), std::invalid_argument,
                       "takeAlongAxis should require matching non-axis extents");
    Tensor<int> deepTuples({1, 3}, 0);
    FATP_ASSERT_THROWS(gatherND(matrix, deepTuples), std::invalid_argument,
                       "gatherND tuple depth cannot exceed source rank");
    Tensor<int> scalarIndices({}, 0);
    FATP_ASSERT_THROWS(gatherND(matrix, scalarIndices), std::invalid_argument,
                       "gatherND indices must carry a tuple dimension");
    return true;
}

struct SelectionAllocationState
{
    std::size_t attempts = 0;
    std::size_t liveElements = 0;
    bool fail = false;
};

template <typename T>
struct SelectionAllocator
{
    using value_type = T;

    SelectionAllocationState* state = nullptr;
    int identity = 0;

    SelectionAllocator() = default;
    SelectionAllocator(SelectionAllocationState& counts, int id) noexcept : state(&counts), identity(id) {}
    template <typename U>
    SelectionAllocator(const SelectionAllocator<U>& other) noexcept : state(other.state), identity(other.identity) {}

    T* allocate(std::size_t count)
    {
        if (state)
        {
            ++state->attempts;
            if (state->fail)
            {
                throw std::bad_alloc();
            }
        }
        auto* result = std::allocator<T>{}.allocate(count);
        if (state)
        {
            state->liveElements += count;
        }
        return result;
    }
    void deallocate(T* storage, std::size_t count) noexcept
    {
        if (state)
        {
            state->liveElements -= count;
        }
        std::allocator<T>{}.deallocate(storage, count);
    }
    SelectionAllocator select_on_container_copy_construction() const noexcept
    {
        auto selected = *this;
        selected.identity += 100;
        return selected;
    }
    template <typename U>
    bool operator==(const SelectionAllocator<U>& other) const noexcept
    {
        return state == other.state && identity == other.identity;
    }
};

template <typename Source, typename Indices, typename Allocator>
auto runSelection(int operation, const Source& source, const Indices& indices, const Allocator& allocator)
{
    const std::array<std::reference_wrapper<const Source>, 2> references = {std::cref(source), std::cref(source)};
    const auto inputs = std::span<const std::reference_wrapper<const Source>>(references);
    const std::array<std::ptrdiff_t, 1> first = {0};
    const auto selected = indices.empty() ? std::span<const std::ptrdiff_t>{} : std::span<const std::ptrdiff_t>(first);
    switch (operation)
    {
        case 0: return stack(source, source, 0, allocator);
        case 1: return stack(inputs, 0, allocator);
        case 2: return concatenate(source, source, 0, allocator);
        case 3: return concatenate(inputs, 0, allocator);
        case 4: return take(source, selected, 0, allocator);
        case 5: return takeAlongAxis(source, indices, 0, allocator);
        case 6: return gatherND(source, indices, allocator);
        default: return gatherND<1>(source, indices, allocator);
    }
}

FATP_TEST_CASE(selection_allocator_and_failure_contract)
{
    SelectionAllocationState ownerState;
    SelectionAllocationState explicitState;
    using Owner = Tensor<int, SelectionAllocator<int>>;
    const Owner first(std::allocator_arg, SelectionAllocator<int>(ownerState, 7), DynamicExtents{2}, 31);
    const Owner second(std::allocator_arg, SelectionAllocator<int>(ownerState, 9), DynamicExtents{1}, 42);
    const std::array<std::reference_wrapper<const Owner>, 2> references = {std::cref(first), std::cref(second)};
    const auto inputs = std::span<const std::reference_wrapper<const Owner>>(references);
    const SelectionAllocator<int> supplied(explicitState, 23);
    {
        const auto inherited = concatenate(inputs, 0);
        FATP_ASSERT_EQ(inherited.get_allocator().identity, 107, "Multi-input concatenate applies SOCCC to the first owner");
        const auto explicitResult = concatenate(inputs, 0, supplied);
        FATP_ASSERT_EQ(explicitResult.get_allocator().identity, 23, "Multi-input concatenate preserves the supplied allocator");
        FATP_ASSERT_TRUE(explicitResult[0] == 31 && explicitResult[1] == 31 && explicitResult[2] == 42,
                         "Allocator selection preserves input placement");
    }
    FATP_ASSERT_EQ(explicitState.liveElements, std::size_t{0}, "Result destruction releases allocated elements");

    const Tensor<int> source({2}, 17);
    const Tensor<int> indices({1}, 0);
    const Tensor<int> empty({0});
    const Tensor<int> emptyIndices({0});
    for (int operation = 0; operation < 8; ++operation)
    {
        explicitState.fail = true;
        FATP_ASSERT_THROWS(runSelection(operation, source, indices, supplied), std::bad_alloc,
                           "Every selection entry point propagates result allocation failure");
        FATP_ASSERT_EQ(explicitState.liveElements, std::size_t{0}, "Allocation failure leaves no partial result buffer");
        // Typed depth-one gather needs one component per tuple; the other empty
        // operations also accept the rank-one zero-depth index representation.
        if (operation != 7)
        {
            const auto emptyResult = runSelection(operation, empty, emptyIndices, supplied);
            FATP_ASSERT_TRUE(emptyResult.empty(), "Empty selection results allocate no element buffer");
        }
        explicitState.fail = false;
        const auto successful = runSelection(operation, source, indices, supplied);
        FATP_ASSERT_EQ(successful.get_allocator().identity, 23, "Selection forwards the exact explicit allocator");
        for (const auto value : successful)
        {
            FATP_ASSERT_EQ(value, 17, "Successful selection retains source values after allocation failure");
        }
    }
    FATP_ASSERT_TRUE(source[0] == 17 && source[1] == 17 && indices[0] == 0,
                     "Allocation failures never mutate source values or indices");
    FATP_ASSERT_EQ(explicitState.liveElements, std::size_t{0}, "Every temporary result releases its elements");

    const Tensor<int> mismatch({3, 1}, 0);
    explicitState.fail = true;
    const auto before = explicitState.attempts;
    FATP_ASSERT_THROWS(concatenate(source, mismatch, 0, supplied), std::invalid_argument,
                       "Rank validation precedes result allocation");
    const std::array<std::ptrdiff_t, 1> outside = {2};
    FATP_ASSERT_THROWS(take(source, outside, 0, supplied), std::out_of_range,
                       "Take validates index bounds before result allocation");
    const Tensor<std::uint64_t> wideIndices({1}, std::numeric_limits<std::uint64_t>::max());
    FATP_ASSERT_THROWS(takeAlongAxis(source, wideIndices, 0, supplied), std::out_of_range,
                       "Unsigned index bounds are checked before allocation without narrowing");
    const Tensor<std::int64_t> negativeIndices({1}, std::numeric_limits<std::int64_t>::min());
    FATP_ASSERT_THROWS(gatherND(source, negativeIndices, supplied), std::out_of_range,
                       "The most-negative tuple component is rejected without signed overflow");
    FATP_ASSERT_EQ(explicitState.attempts, before, "Invalid shape and indices allocate no result elements");
    return true;
}

FATP_TEST_CASE(selection_shared_and_borrowed_lifetimes)
{
    const auto retained = [] {
        Tensor<int> owner({2}, 19);
        return owner.asSharedView();
    }();
    const Tensor<int> indices({1}, 0);
    const std::allocator<int> allocator;
    for (int operation = 0; operation < 8; ++operation)
    {
        const auto result = runSelection(operation, retained, indices, allocator);
        for (const auto value : result)
        {
            FATP_ASSERT_EQ(value, 19, "All selection operations consume storage retained by shared views");
        }
    }
#ifndef NDEBUG
    const auto expired = [] {
        Tensor<int> owner({2}, 19);
        return owner.asConstView();
    }();
    const auto expiredEmpty = [] {
        Tensor<int> owner({0});
        return owner.asConstView();
    }();
    const Tensor<int> source({2}, 19);
    const Tensor<int> emptyIndices({0});
    SelectionAllocationState state;
    state.fail = true;
    const SelectionAllocator<int> failing(state, 11);
    for (int operation = 0; operation < 8; ++operation)
    {
        FATP_ASSERT_THROWS(runSelection(operation, expired, indices, failing), std::runtime_error,
                           "Selection validates expired borrowed sources before allocation");
        FATP_ASSERT_THROWS(runSelection(operation, expiredEmpty, emptyIndices, failing), std::runtime_error,
                           "Selection validates lifetime even for empty borrowed sources");
    }
    FATP_ASSERT_THROWS(takeAlongAxis(source, expired, 0, failing), std::runtime_error,
                       "Take-along-axis validates borrowed indices before allocation");
    FATP_ASSERT_THROWS(gatherND(source, expiredEmpty, failing), std::runtime_error,
                       "Zero-depth gather still validates borrowed indices");
    const auto valid = source.asConstView();
    const std::array<std::reference_wrapper<const TensorView<const int>>, 2> laterExpired = {
        std::cref(valid), std::cref(expired)};
    const auto inputs = std::span<const std::reference_wrapper<const TensorView<const int>>>(laterExpired);
    FATP_ASSERT_THROWS(concatenate(inputs, 0, failing), std::runtime_error,
                       "Concatenate validates every operand before allocation, including later inputs");
    FATP_ASSERT_THROWS(stack(inputs, 0, failing), std::runtime_error,
                       "Stack validates every operand before allocation");
    FATP_ASSERT_EQ(state.attempts, std::size_t{0}, "Expired sources and indices never allocate result elements");
#endif
    return true;
}

struct ThrowingSelectionValue
{
    inline static int assignmentsUntilFailure = -1;
    int value = 0;

    ThrowingSelectionValue& operator=(const ThrowingSelectionValue& other)
    {
        if (assignmentsUntilFailure == 0)
        {
            throw std::runtime_error("selection element assignment failed");
        }
        if (assignmentsUntilFailure > 0)
        {
            --assignmentsUntilFailure;
        }
        value = other.value;
        return *this;
    }
};

FATP_TEST_CASE(concatenate_partial_copy_cleanup)
{
    Tensor<ThrowingSelectionValue> first({2});
    Tensor<ThrowingSelectionValue> second({2});
    first[0].value = 11;
    first[1].value = 12;
    second[0].value = 21;
    second[1].value = 22;
    const std::array<std::reference_wrapper<const Tensor<ThrowingSelectionValue>>, 2> references = {
        std::cref(first), std::cref(second)};
    const auto inputs = std::span<const std::reference_wrapper<const Tensor<ThrowingSelectionValue>>>(references);
    SelectionAllocationState state;
    const SelectionAllocator<ThrowingSelectionValue> allocator(state, 3);
    ThrowingSelectionValue::assignmentsUntilFailure = 3;
    FATP_ASSERT_THROWS(concatenate(inputs, 0, allocator), std::runtime_error,
                       "A throwing assignment in a later input discards the partial materialization");
    ThrowingSelectionValue::assignmentsUntilFailure = -1;
    FATP_ASSERT_EQ(state.liveElements, std::size_t{0}, "Late copy failure releases the entire result buffer");
    FATP_ASSERT_TRUE(first[0].value == 11 && first[1].value == 12 && second[0].value == 21 && second[1].value == 22,
                     "A failed later copy preserves every source element");
    const auto result = concatenate(inputs, 0, allocator);
    FATP_ASSERT_TRUE(result[0].value == 11 && result[1].value == 12 && result[2].value == 21 && result[3].value == 22,
                     "The same sources remain usable after partial-copy cleanup");
    return true;
}

} // namespace fat_p::testing::tensor_selection

namespace fat_p::testing
{

bool test_TensorSelection()
{
    FATP_PRINT_HEADER(TENSOR SELECTION)
    TestRunner runner;
    FATP_RUN_TEST_NS(runner, tensor_selection, stack_pair_and_many);
    FATP_RUN_TEST_NS(runner, tensor_selection, concatenate_pair_many_and_strided);
    FATP_RUN_TEST_NS(runner, tensor_selection, concatenate_many_small_inputs);
    FATP_RUN_TEST_NS(runner, tensor_selection, selection_coordinate_oracle);
    FATP_RUN_TEST_NS(runner, tensor_selection, take_and_take_along_axis);
    FATP_RUN_TEST_NS(runner, tensor_selection, gather_nd_and_zero_depth);
    FATP_RUN_TEST_NS(runner, tensor_selection, validation_errors);
    FATP_RUN_TEST_NS(runner, tensor_selection, selection_allocator_and_failure_contract);
    FATP_RUN_TEST_NS(runner, tensor_selection, selection_shared_and_borrowed_lifetimes);
    FATP_RUN_TEST_NS(runner, tensor_selection, concatenate_partial_copy_cleanup);
    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_TensorSelection() ? 0 : 1;
}
#endif
