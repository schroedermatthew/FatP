/** @file test_TensorSlice.cpp @brief Coordinate-oracle, aliasing, and lifetime tests for Tensor transforms. */

/*
FATP_META:
  meta_version: 1
  component: TensorSlice
  file_role: test
  path: components/Tensor/tests/test_TensorSlice.cpp
  namespace: fat_p::testing::tensor_slice
  layer: Testing
  summary: "Bounded exhaustive and randomized coordinate-oracle tests for metadata-only Tensor transforms."
  api_stability: in_work
  related:
    docs:
      - components/Tensor/docs/Design Note - Tensor Architecture Additions Plan.md
      - components/Tensor/docs/Design Note - Tensor Semantic Contract.md
    headers:
      - include/fat_p/Tensor.h
      - include/fat_p/TensorSlice.h
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
#include "Tensor.h"
#include "TensorSlice.h"
#include "TensorTestSupport.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <memory>
#include <numeric>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace fat_p::testing::tensor_slice
{

using tensor_support::CoordinateReference;
using tensor_support::CoordinateSelection;
using tensor_support::coordinateRange;
using tensor_support::selectCoordinates;
using tensor_support::smallSliceCoordinates;

[[nodiscard]] CoordinateReference denseReference(const std::vector<std::size_t>& extents)
{
    std::size_t count = 1;
    for (const auto extent : extents)
    {
        count *= extent;
    }
    CoordinateReference result{extents, std::vector<std::ptrdiff_t>(count)};
    std::iota(result.offsets.begin(), result.offsets.end(), std::ptrdiff_t{0});
    return result;
}

[[nodiscard]] std::vector<CoordinateSelection> identitySelections(const CoordinateReference& source)
{
    std::vector<CoordinateSelection> result;
    for (std::size_t axis = 0; axis < source.extents.size(); ++axis)
    {
        result.push_back({axis, coordinateRange(source.extents[axis]), true});
    }
    return result;
}

// Dispatch the bounded generated ranks to the independent variadic access path.
// Unlike iterators/operator[], operator() recomputes a multidimensional offset.
template <std::size_t Rank = 0, typename View>
[[nodiscard]] bool verifyCoordinateAccess(const View& view, const CoordinateReference& expected, const int* root)
{
    if (expected.extents.size() == Rank)
    {
        for (std::size_t index = 0; index < expected.offsets.size(); ++index)
        {
            std::array<std::ptrdiff_t, Rank> coordinates{};
            auto remainder = index;
            for (std::size_t reverseAxis = Rank; reverseAxis > 0; --reverseAxis)
            {
                const auto axis = reverseAxis - 1;
                coordinates[axis] = static_cast<std::ptrdiff_t>(remainder % expected.extents[axis]);
                remainder /= expected.extents[axis];
            }
            const auto& value = std::apply([&](auto... axes) -> decltype(auto) { return view(axes...); }, coordinates);
            FATP_ASSERT_TRUE(std::addressof(value) == root + expected.offsets[index],
                             "Multidimensional access must match the independently selected root address");
        }
        return true;
    }
    if constexpr (Rank < 12)
    {
        return verifyCoordinateAccess<Rank + 1>(view, expected, root);
    }
    else
    {
        FATP_ASSERT_TRUE(false, "Generated rank exceeds the multidimensional oracle's bounded dispatch");
        return false;
    }
}

template <typename View>
[[nodiscard]] bool verifyView(const View& view, const CoordinateReference& expected, const int* root)
{
    FATP_ASSERT_TRUE(view.extents().values() == expected.extents, "View shape must match the coordinate oracle");
    FATP_ASSERT_EQ(view.size(), expected.offsets.size(), "View size must match explicit coordinate enumeration");
    std::size_t visited = 0;
    for (const auto& value : view)
    {
        FATP_ASSERT_LT(visited, expected.offsets.size(), "View iterator must terminate at the logical count");
        FATP_ASSERT_TRUE(std::addressof(value) == root + expected.offsets[visited],
                         "Iterator must alias the expected root-storage element");
        ++visited;
    }
    FATP_ASSERT_EQ(visited, expected.offsets.size(), "View iterator must visit every expected coordinate");
    for (std::size_t index = 0; index < expected.offsets.size(); ++index)
    {
        FATP_ASSERT_TRUE(std::addressof(view.atLinear(index)) == root + expected.offsets[index],
                         "Transformed access must alias the independently selected root address");
        FATP_ASSERT_EQ(view[index], root[expected.offsets[index]], "Transformed value must match its root coordinate");
    }
    if (expected.offsets.empty())
    {
        FATP_ASSERT_FALSE(view.layout().minimumOffset().has_value(), "Empty transforms have no reachable minimum");
        FATP_ASSERT_FALSE(view.layout().maximumOffset().has_value(), "Empty transforms have no reachable maximum");
    }
    return verifyCoordinateAccess(view, expected, root);
}

[[nodiscard]] bool verifyWrites(TensorView<int> view, const CoordinateReference& reference,
                                 std::vector<int>& storage)
{
    const auto original = storage;
    auto expected = storage;
    for (std::size_t index = 0; index < reference.offsets.size(); ++index)
    {
        const auto value = -1000 - static_cast<int>(index);
        view[index] = value;
        expected[static_cast<std::size_t>(reference.offsets[index])] = value;
    }
    FATP_ASSERT_TRUE(storage == expected, "Writes must touch exactly the oracle's selected root elements");
    std::copy(original.begin(), original.end(), storage.begin());
    return true;
}

struct GeneratedSlice
{
    std::vector<SliceSpec> specifications;
    CoordinateReference expected;
};

// Generate public syntax and coordinate selections together. Ellipsis replaces
// a known All interval; implicit trailing axes are explicit only in the oracle.
// The reference does not parse or expand the production SliceSpec grammar.
[[nodiscard]] GeneratedSlice generateSlice(const CoordinateReference& source, std::mt19937_64& random)
{
    const auto rank = source.extents.size();
    const bool useEllipsis = random() % 2 == 0;
    const auto firstImplicit = static_cast<std::size_t>(random() % (rank + 1));
    const auto implicitEnd = useEllipsis
        ? firstImplicit + static_cast<std::size_t>(random() % (rank - firstImplicit + 1)) : rank;
    GeneratedSlice result;
    std::vector<CoordinateSelection> selections;
    for (std::size_t axis = 0; axis <= rank; ++axis)
    {
        const bool insideEllipsis = useEllipsis && axis > firstImplicit && axis < implicitEnd;
        const bool syntaxVisible = useEllipsis || axis <= firstImplicit;
        if (!insideEllipsis && syntaxVisible && random() % 5 == 0)
        {
            result.specifications.emplace_back(NewAxis);
            selections.push_back({std::nullopt, {0}, true});
        }
        if (useEllipsis && axis == firstImplicit)
        {
            result.specifications.emplace_back(Ellipsis);
        }
        if (axis == rank)
        {
            break;
        }
        const auto extent = source.extents[axis];
        if (axis >= firstImplicit && axis < implicitEnd)
        {
            selections.push_back({axis, coordinateRange(extent), true});
            continue;
        }
        const auto choice = random() % 3;
        if (choice == 0 && extent != 0)
        {
            const auto coordinate = static_cast<std::size_t>(random() % extent);
            const auto index = static_cast<std::ptrdiff_t>(coordinate) -
                (random() % 2 == 0 ? static_cast<std::ptrdiff_t>(extent) : 0);
            result.specifications.emplace_back(index);
            selections.push_back({axis, {coordinate}, false});
        }
        else if (choice == 1)
        {
            const auto bound = [&]() -> std::optional<int> {
                return random() % 3 == 0 ? std::nullopt : std::optional<int>(static_cast<int>(random() % 13) - 6);
            };
            const auto start = bound();
            const auto stop = bound();
            const int magnitude = static_cast<int>(random() % 3) + 1;
            const int step = random() % 2 == 0 ? magnitude : -magnitude;
            result.specifications.emplace_back(Slice{start, stop, step});
            selections.push_back({axis, smallSliceCoordinates(extent, start, stop, step), true});
        }
        else
        {
            result.specifications.emplace_back(All);
            selections.push_back({axis, coordinateRange(extent), true});
        }
    }
    result.expected = selectCoordinates(source, selections);
    return result;
}

FATP_TEST_CASE(coordinate_oracle_anchors)
{
    // Literal results independently checked against Python's built-in list slicing.
    struct SliceAnchor
    {
        std::size_t extent;
        std::optional<int> start;
        std::optional<int> stop;
        int step;
        std::vector<std::size_t> expected;
    };
    const std::vector<SliceAnchor> anchors{
        {0, {}, {}, 1, {}}, {0, {}, {}, -1, {}}, {1, {}, {}, -1, {0}}, {1, {}, -1, -1, {}},
        {4, -10, 10, 1, {0, 1, 2, 3}}, {4, -10, 10, 2, {0, 2}},
        {4, 10, -10, -1, {3, 2, 1, 0}}, {4, 10, -10, -2, {3, 1}},
        {4, -1, -5, -2, {3, 1}}, {4, -5, {}, -1, {}}, {4, {}, -5, -1, {3, 2, 1, 0}},
        {4, {}, -1, -1, {}}, {4, -2, {}, -1, {2, 1, 0}}, {4, 1, -1, 2, {1}},
        {5, -4, -1, 2, {1, 3}}, {5, 4, 0, -2, {4, 2}}, {5, -1, -4, -2, {4, 2}},
        {5, 0, 4, -1, {}}, {5, -20, 20, 3, {0, 3}}, {5, 3, -20, -3, {3, 0}}};
    for (const auto& anchor : anchors)
    {
        FATP_ASSERT_TRUE(smallSliceCoordinates(anchor.extent, anchor.start, anchor.stop, anchor.step) ==
                             anchor.expected,
                         "Oracle normalization must match independently pinned list-slicing results");
    }
    const auto source = denseReference({2, 3});
    const auto selected = selectCoordinates(source, {{0, {1, 0}, true}, {std::nullopt, {0}, true}, {1, {2}, false}});
    FATP_ASSERT_TRUE(selected.extents == std::vector<std::size_t>({2, 1}), "Oracle axis removal/insertion anchor");
    FATP_ASSERT_TRUE(selected.offsets == std::vector<std::ptrdiff_t>({5, 2}), "Oracle selection order anchor");
    const auto transposed = selectCoordinates(source, {{1, {0, 1, 2}, true}, {0, {0, 1}, true}});
    FATP_ASSERT_TRUE(transposed.offsets == std::vector<std::ptrdiff_t>({0, 3, 1, 4, 2, 5}),
                     "Oracle permutation must reorder source coordinates, not only result extents");
    FATP_ASSERT_TRUE(smallSliceCoordinates(4, {}, {}, -1) == std::vector<std::size_t>({3, 2, 1, 0}),
                     "Omitted reverse stop is before coordinate zero");
    FATP_ASSERT_TRUE(smallSliceCoordinates(4, {}, -1, -1).empty(),
                     "Explicit negative-one reverse stop is the last coordinate, not the omitted sentinel");
    FATP_ASSERT_TRUE(smallSliceCoordinates(5, -20, 20, 2) == std::vector<std::size_t>({0, 2, 4}),
                     "Oracle clips endpoints before selecting step congruence");
    const auto scalar = selectCoordinates(denseReference({1, 1}), {{0, {0}, false}, {1, {0}, false}});
    FATP_ASSERT_TRUE(scalar.extents.empty() && scalar.offsets == std::vector<std::ptrdiff_t>({0}),
                     "Removing every singleton axis must produce one scalar coordinate");
    const auto empty = selectCoordinates(denseReference({2, 0}), {{0, {1}, false}, {1, {}, true}});
    FATP_ASSERT_TRUE(empty.extents == std::vector<std::size_t>({0}) && empty.offsets.empty(),
                     "An empty selected axis must suppress leaf visits");
    return true;
}

FATP_TEST_CASE(exhaustive_small_signed_slices)
{
    std::vector<std::optional<int>> bounds{std::nullopt};
    for (int bound = -8; bound <= 8; ++bound)
    {
        bounds.emplace_back(bound);
    }
    std::size_t cases = 0;
    for (std::size_t extent = 0; extent <= 5; ++extent)
    {
        for (const std::ptrdiff_t stride : {1, 2, -2, 0})
        {
            const auto origin = stride < 0 && extent != 0 ? 2 * static_cast<std::ptrdiff_t>(extent - 1) + 1 : 1;
            std::vector<int> storage(2 * extent + 3);
            std::iota(storage.begin(), storage.end(), 100);
            const tensor_support::LayoutSpec spec{{extent}, {stride}, origin};
            const CoordinateReference source{{extent}, tensor_support::enumerateOffsets(spec)};
            const auto view = TensorView<const int>::borrow(storage.data(),
                TensorLayout(storage.size(), origin, DynamicExtents{extent}, TensorStrides{stride}));
            for (const auto start : bounds)
            {
                for (const auto stop : bounds)
                {
                    for (int step = -4; step <= 4; ++step)
                    {
                        if (step == 0)
                        {
                            continue;
                        }
                        const auto expected = selectCoordinates(source,
                            {{0, smallSliceCoordinates(extent, start, stop, step), true}});
                        FATP_ASSERT_TRUE(verifyView(view.sliceView({Slice{start, stop, step}}), expected,
                                                   storage.data()),
                                         "Exhaustive small signed-slice case must match the coordinate oracle");
                        ++cases;
                    }
                }
            }
        }
    }
    FATP_ASSERT_EQ(cases, std::size_t{62'208}, "Every bounded endpoint/step/stride combination must run");
    return true;
}

FATP_TEST_CASE(exhaustive_small_permutations_and_axis_transforms)
{
    for (std::size_t rank = 0; rank <= 4; ++rank)
    {
        std::size_t shapeCount = 1;
        for (std::size_t axis = 0; axis < rank; ++axis)
        {
            shapeCount *= 3;
        }
        for (std::size_t shapeCode = 0; shapeCode < shapeCount; ++shapeCode)
        {
            auto code = shapeCode;
            std::vector<std::size_t> extents(rank);
            for (auto& extent : extents)
            {
                extent = code % 3;
                code /= 3;
            }
            const auto source = denseReference(extents);
            Tensor<int> owner{DynamicExtents(extents)};
            std::iota(owner.begin(), owner.end(), 100);
            std::vector<TensorAxis> order(rank);
            std::iota(order.begin(), order.end(), TensorAxis{0});
            do
            {
                std::vector<CoordinateSelection> selections;
                auto negativeOrder = order;
                for (std::size_t index = 0; index < rank; ++index)
                {
                    const auto axis = static_cast<std::size_t>(order[index]);
                    selections.push_back({axis, coordinateRange(extents[axis]), true});
                    negativeOrder[index] -= static_cast<TensorAxis>(rank);
                }
                const auto expected = selectCoordinates(source, selections);
                FATP_ASSERT_TRUE(verifyView(owner.permuteView(order), expected, owner.data()),
                                 "Every small permutation must match coordinate reordering");
                FATP_ASSERT_TRUE(verifyView(owner.permuteView(negativeOrder), expected, owner.data()),
                                 "Negative permutation axes must name the same source axes");
            } while (std::next_permutation(order.begin(), order.end()));

            for (std::size_t inserted = 0; inserted <= rank; ++inserted)
            {
                auto selections = identitySelections(source);
                selections.insert(selections.begin() + static_cast<std::ptrdiff_t>(inserted),
                                  {std::nullopt, {0}, true});
                const auto expected = selectCoordinates(source, selections);
                const auto positive = static_cast<TensorAxis>(inserted);
                const auto negative = positive - static_cast<TensorAxis>(rank + 1);
                FATP_ASSERT_TRUE(verifyView(owner.unsqueezeView(positive), expected, owner.data()),
                                 "Every insertion position must preserve root addresses");
                const auto result = owner.unsqueezeView(negative);
                FATP_ASSERT_TRUE(verifyView(result, expected, owner.data()), "Negative insertion axis must normalize");
                FATP_ASSERT_TRUE(verifyView(result.squeezeView({positive}), source, owner.data()),
                                 "Squeezing only the inserted axis must invert unsqueeze");
            }
            auto allSingletons = identitySelections(source);
            for (std::size_t axis = 0; axis < rank; ++axis)
            {
                allSingletons[axis].keepAxis = extents[axis] != 1;
            }
            FATP_ASSERT_TRUE(verifyView(owner.squeezeView(), selectCoordinates(source, allSingletons), owner.data()),
                             "Default squeeze must remove exactly the singleton dimensions");
            for (std::size_t mask = 1; mask < (std::size_t{1} << rank); ++mask)
            {
                auto selections = identitySelections(source);
                std::vector<TensorAxis> axes;
                bool valid = true;
                for (std::size_t axis = 0; axis < rank; ++axis)
                {
                    if ((mask & (std::size_t{1} << axis)) != 0)
                    {
                        valid = valid && extents[axis] == 1;
                        axes.push_back(static_cast<TensorAxis>(axis) - static_cast<TensorAxis>(rank));
                        selections[axis].keepAxis = false;
                    }
                }
                if (valid)
                {
                    FATP_ASSERT_TRUE(verifyView(owner.squeezeView(axes), selectCoordinates(source, selections),
                                               owner.data()), "Every requested singleton subset must be removed");
                }
                else
                {
                    FATP_ASSERT_THROWS(owner.squeezeView(axes), std::invalid_argument,
                                       "A subset containing any nonsingleton must be rejected");
                }
            }
            const CoordinateReference flattened{{source.offsets.size()}, source.offsets};
            FATP_ASSERT_TRUE(verifyView(owner.reshapeView(DynamicExtents{source.offsets.size()}), flattened,
                                       owner.data()), "Canonical scalar/empty reshape must preserve logical order");
        }
    }
    return true;
}

FATP_TEST_CASE(randomized_composed_transform_oracle)
{
    std::mt19937_64 random(0x51CE2026ULL);
    tensor_support::DeterministicLayoutGenerator generator(0xA1151CEULL);
    std::size_t writableCases = 0;
    std::size_t readonlyAliasedCases = 0;
    std::size_t nonemptyCases = 0;
    for (std::size_t sample = 0; sample < 500; ++sample)
    {
        auto spec = generator.next(static_cast<std::size_t>(random() % 5), 3);
        spec.origin = 0;
        const auto bounds = tensor_support::reachableBounds(spec);
        spec.origin = bounds ? 2 - bounds->first : 0;
        const auto length = bounds ? static_cast<std::size_t>(bounds->second - bounds->first + 5) : 1;
        std::vector<int> storage(length);
        std::iota(storage.begin(), storage.end(), 100);
        const TensorLayout layout(length, spec.origin, DynamicExtents(spec.extents), spec.strides);
        auto current = TensorView<const int>::borrow(storage.data(), layout);
        CoordinateReference reference{spec.extents, tensor_support::enumerateOffsets(spec)};
        std::optional<TensorView<int>> writable;
        if (layout.isInjective())
        {
            writable = TensorView<int>::borrow(storage.data(), layout);
            ++writableCases;
        }
        else
        {
            FATP_ASSERT_THROWS(TensorView<int>::borrow(storage.data(), layout), std::invalid_argument,
                               "Overlapping or indeterminate mappings must reject mutable borrowing");
            ++readonlyAliasedCases;
        }
        for (std::size_t operation = 0; operation < 6; ++operation)
        {
            const auto rank = reference.extents.size();
            const auto choice = random() % 4;
            if (choice == 0 && rank < 6)
            {
                const auto generated = generateSlice(reference, random);
                current = current.sliceView(generated.specifications);
                if (writable)
                {
                    *writable = writable->sliceView(generated.specifications);
                }
                reference = generated.expected;
            }
            else if (choice == 1)
            {
                std::vector<TensorAxis> order(rank);
                std::iota(order.begin(), order.end(), TensorAxis{0});
                std::shuffle(order.begin(), order.end(), random);
                std::vector<CoordinateSelection> selections;
                for (auto& axis : order)
                {
                    const auto sourceAxis = static_cast<std::size_t>(axis);
                    selections.push_back({sourceAxis, coordinateRange(reference.extents[sourceAxis]), true});
                    if (random() % 2 == 0)
                    {
                        axis -= static_cast<TensorAxis>(rank);
                    }
                }
                current = current.permuteView(order);
                if (writable)
                {
                    *writable = writable->permuteView(order);
                }
                reference = selectCoordinates(reference, selections);
            }
            else if (choice == 2 && rank < 6)
            {
                const auto axis = static_cast<std::size_t>(random() % (rank + 1));
                auto selections = identitySelections(reference);
                selections.insert(selections.begin() + static_cast<std::ptrdiff_t>(axis),
                                  {std::nullopt, {0}, true});
                const auto negative = static_cast<TensorAxis>(axis) - static_cast<TensorAxis>(rank + 1);
                current = current.unsqueezeView(negative);
                if (writable)
                {
                    *writable = writable->unsqueezeView(negative);
                }
                reference = selectCoordinates(reference, selections);
            }
            else
            {
                auto selections = identitySelections(reference);
                for (std::size_t axis = 0; axis < rank; ++axis)
                {
                    selections[axis].keepAxis = reference.extents[axis] != 1;
                }
                current = current.squeezeView();
                if (writable)
                {
                    *writable = writable->squeezeView();
                }
                reference = selectCoordinates(reference, selections);
            }
            FATP_ASSERT_TRUE(verifyView(current, reference, storage.data()),
                             "Composed oracle mismatch at deterministic sample " + std::to_string(sample));
            if (!reference.offsets.empty())
            {
                ++nonemptyCases;
            }
            if (writable)
            {
                FATP_ASSERT_TRUE(verifyView(*writable, reference, storage.data()),
                                 "Mutable transform must match oracle");
                FATP_ASSERT_TRUE(verifyWrites(*writable, reference, storage), "Mutable transform must write through");
            }
        }
    }
    FATP_ASSERT_GT(writableCases, std::size_t{100}, "Random suite must exercise injective source mappings");
    FATP_ASSERT_GT(readonlyAliasedCases, std::size_t{20}, "Random suite must exercise read-only aliased mappings");
    FATP_ASSERT_GT(nonemptyCases, std::size_t{300}, "Compositions must not become a vacuous all-empty oracle");
    return true;
}

FATP_TEST_CASE(rectangular_convenience_and_broadcast_oracle)
{
    for (std::size_t rows = 0; rows <= 3; ++rows)
    {
        for (std::size_t columns = 0; columns <= 3; ++columns)
        {
            const tensor_support::LayoutSpec spec{{rows, columns}, {10, -2}, 6};
            std::vector<int> storage(40);
            std::iota(storage.begin(), storage.end(), 100);
            const CoordinateReference reference{spec.extents, tensor_support::enumerateOffsets(spec)};
            const auto view = TensorView<int>::borrow(storage.data(),
                TensorLayout(storage.size(), spec.origin, DynamicExtents(spec.extents), spec.strides));
            const auto transposed = selectCoordinates(reference,
                {{1, coordinateRange(columns), true}, {0, coordinateRange(rows), true}});
            FATP_ASSERT_TRUE(verifyView(view.transposeView(), transposed, storage.data()),
                             "Transpose must reorder coordinates on padded negative-stride input");
            for (std::size_t row = 0; row < rows; ++row)
            {
                const auto expected = selectCoordinates(reference,
                    {{0, {row}, true}, {1, coordinateRange(columns), true}});
                FATP_ASSERT_TRUE(verifyView(view.rowView(row), expected, storage.data()),
                                 "Row extraction keeps a singleton row dimension");
            }
            for (std::size_t column = 0; column < columns; ++column)
            {
                const auto expected = selectCoordinates(reference,
                    {{0, coordinateRange(rows), true}, {1, {column}, true}});
                FATP_ASSERT_TRUE(verifyView(view.columnView(column), expected, storage.data()),
                                 "Column extraction keeps a singleton column dimension");
            }
            for (std::size_t firstRow = 0; firstRow <= rows; ++firstRow)
            {
                for (std::size_t lastRow = firstRow; lastRow <= rows; ++lastRow)
                {
                    for (std::size_t firstColumn = 0; firstColumn <= columns; ++firstColumn)
                    {
                        for (std::size_t lastColumn = firstColumn; lastColumn <= columns; ++lastColumn)
                        {
                            auto selectedRows = coordinateRange(lastRow);
                            selectedRows.erase(selectedRows.begin(),
                                               selectedRows.begin() + static_cast<std::ptrdiff_t>(firstRow));
                            auto selectedColumns = coordinateRange(lastColumn);
                            selectedColumns.erase(selectedColumns.begin(),
                                selectedColumns.begin() + static_cast<std::ptrdiff_t>(firstColumn));
                            const auto expected = selectCoordinates(reference,
                                {{0, selectedRows, true}, {1, selectedColumns, true}});
                            const auto result = view.sliceView({firstRow, firstColumn}, {lastRow, lastColumn});
                            FATP_ASSERT_TRUE(verifyView(result, expected, storage.data()),
                                             "Every small rectangular interval must preserve root coordinates");
                            FATP_ASSERT_TRUE(verifyWrites(result, expected, storage),
                                             "Rectangular slices must write through without touching padding");
                        }
                    }
                }
            }
        }
    }
    for (std::size_t rows = 0; rows <= 3; ++rows)
    {
        for (std::size_t columns = 0; columns <= 3; ++columns)
        {
            Tensor<int> source({1, columns});
            std::iota(source.begin(), source.end(), 100);
            const auto reference = denseReference({1, columns});
            const auto expected = selectCoordinates(reference,
                {{std::nullopt, std::vector<std::size_t>(2, 0), true},
                 {0, std::vector<std::size_t>(rows, 0), true}, {1, coordinateRange(columns), true}});
            const auto result = source.broadcastView(DynamicExtents{2, rows, columns});
            static_assert(std::same_as<decltype(result)::element_type, const int>);
            FATP_ASSERT_TRUE(verifyView(result, expected, source.data()),
                             "Broadcast must repeat explicit source coordinates, including empty targets");
        }
    }
    Tensor<int> scalar(DynamicExtents{}, 7);
    const auto repeated = scalar.broadcastView(DynamicExtents{2, 3});
    FATP_ASSERT_TRUE(verifyView(repeated, CoordinateReference{{2, 3}, std::vector<std::ptrdiff_t>(6, 0)},
                               scalar.data()), "Scalar broadcast must repeat its sole root address");
    return true;
}

struct ElementAllocationCounts
{
    std::size_t allocations = 0;
    std::size_t deallocations = 0;
};

template <typename T>
class SliceAllocator
{
public:
    using value_type = T;

    explicit SliceAllocator(ElementAllocationCounts& counts)
        : mCounts(&counts)
    {
    }

    template <typename U>
    SliceAllocator(const SliceAllocator<U>& other)
        : mCounts(other.counts())
    {
    }

    [[nodiscard]] T* allocate(std::size_t count)
    {
        auto* result = std::allocator<T>{}.allocate(count);
        ++mCounts->allocations;
        return result;
    }

    void deallocate(T* storage, std::size_t count) noexcept
    {
        ++mCounts->deallocations;
        std::allocator<T>{}.deallocate(storage, count);
    }

    [[nodiscard]] ElementAllocationCounts* counts() const noexcept
    {
        return mCounts;
    }

    template <typename U>
    [[nodiscard]] bool operator==(const SliceAllocator<U>& other) const noexcept
    {
        return mCounts == other.counts();
    }

private:
    ElementAllocationCounts* mCounts;
};

FATP_TEST_CASE(transform_allocation_aliasing_and_lifetime)
{
    ElementAllocationCounts counts;
    std::vector<SharedTensorView<int>> retained;
    std::vector<TensorView<int>> borrowed;
    std::vector<CoordinateReference> references;
    SharedTensorView<const int> broadcast;
    int* root = nullptr;
    {
        Tensor<int, SliceAllocator<int>> owner(std::allocator_arg, SliceAllocator<int>(counts), DynamicExtents{2, 3});
        root = owner.data();
        std::iota(owner.begin(), owner.end(), 100);
        const auto shared = owner.asSharedView();
        const auto view = owner.asView();
        retained = {shared.sliceView({All, Slice{{}, {}, -1}}), shared.permuteView({1, 0}),
                    shared.unsqueezeView(1).squeezeView({1}), shared.rowView(1), shared.columnView(2),
                    shared.transposeView(), shared.reshapeView(DynamicExtents{3, 2}),
                    shared.sliceView({0, 1}, {2, 3})};
        borrowed = {view.sliceView({All, Slice{{}, {}, -1}}), view.permuteView({1, 0}),
                    view.unsqueezeView(1).squeezeView({1}), view.rowView(1), view.columnView(2),
                    view.transposeView(), view.reshapeView(DynamicExtents{3, 2}),
                    view.sliceView({0, 1}, {2, 3})};
        const auto reference = denseReference({2, 3});
        const auto transpose = selectCoordinates(reference, {{1, {0, 1, 2}, true}, {0, {0, 1}, true}});
        references = {selectCoordinates(reference, {{0, {0, 1}, true}, {1, {2, 1, 0}, true}}), transpose,
                      reference, selectCoordinates(reference, {{0, {1}, true}, {1, {0, 1, 2}, true}}),
                      selectCoordinates(reference, {{0, {0, 1}, true}, {1, {2}, true}}), transpose,
                      CoordinateReference{{3, 2}, reference.offsets},
                      selectCoordinates(reference, {{0, {0, 1}, true}, {1, {1, 2}, true}})};
        broadcast = shared.rowView(0).broadcastView(DynamicExtents{4, 3});
        const auto& constant = owner;
        static_assert(std::same_as<decltype(constant.permuteView({1, 0})), TensorView<const int>>);
        static_assert(std::same_as<decltype(constant.squeezeView()), TensorView<const int>>);
        static_assert(std::same_as<decltype(constant.unsqueezeView(0)), TensorView<const int>>);
        static_assert(std::same_as<decltype(shared.asConstView().permuteView({1, 0})), SharedTensorView<const int>>);
        static_assert(std::same_as<decltype(shared.asConstView().sliceView({All})), SharedTensorView<const int>>);
        static_assert(std::same_as<decltype(shared.broadcastView(DynamicExtents{2, 3})), SharedTensorView<const int>>);
        for (std::size_t index = 0; index < references.size(); ++index)
        {
            FATP_ASSERT_TRUE(verifyView(retained[index], references[index], root), "Shared transform aliases source");
            FATP_ASSERT_TRUE(verifyView(borrowed[index], references[index], root), "Borrowed transform aliases source");
        }
        FATP_ASSERT_EQ(counts.allocations, std::size_t{1}, "View transforms allocate no owner element buffers");
        FATP_ASSERT_EQ(counts.deallocations, std::size_t{0}, "View transforms must not replace owner element storage");
    }
    FATP_ASSERT_EQ(counts.deallocations, std::size_t{0},
                   "Shared transforms must retain storage after owner destruction");
    for (std::size_t index = 0; index < references.size(); ++index)
    {
        FATP_ASSERT_TRUE(verifyView(retained[index], references[index], root),
                         "Shared transformed lifetime must survive");
#ifndef NDEBUG
        FATP_ASSERT_THROWS(borrowed[index].atLinear(0), std::runtime_error,
                           "Each borrowed transform must reject an invalidated owner in Debug");
#endif
    }
    retained[0][0] = 999;
    FATP_ASSERT_EQ(root[2], 999, "Retained transformed writes must still reach the original allocation");
    retained.clear();
    FATP_ASSERT_EQ(counts.deallocations, std::size_t{0}, "Read-only broadcast must also retain shared ownership");
    FATP_ASSERT_EQ(broadcast(3, 2), 999, "Retained broadcast must observe writes through other aliases");
    broadcast = {};
    FATP_ASSERT_EQ(counts.deallocations, std::size_t{1},
                   "Last shared transform must release exactly one element buffer");
    return true;
}

FATP_TEST_CASE(empty_transform_allocation_and_lifetime)
{
    ElementAllocationCounts counts;
    std::vector<SharedTensorView<int>> retained;
    const std::vector<CoordinateReference> references{
        {{2, 0, 3}, {}}, {{3, 2, 0}, {}}, {{2, 1, 0, 3}, {}}, {{2, 0, 3}, {}},
        {{2, 0}, {}}, {{0, 2}, {}}, {{1, 0}, {}}, {{0, 1}, {}}, {{2, 0, 2}, {}}};
    const CoordinateReference broadcastReference{{4, 2, 0, 3}, {}};
    SharedTensorView<const int> broadcast;
    TensorView<int> borrowed;
    {
        Tensor<int, SliceAllocator<int>> owner(std::allocator_arg, SliceAllocator<int>(counts),
                                               DynamicExtents{2, 0, 3});
        FATP_ASSERT_EQ(counts.allocations, std::size_t{0}, "Empty owners allocate no element buffer");
        const auto shared = owner.asSharedView();
        retained.push_back(shared.sliceView({All, All, Slice{{}, {}, -1}}));
        retained.push_back(shared.permuteView({2, 0, 1}));
        retained.push_back(shared.unsqueezeView(1));
        retained.push_back(retained.back().squeezeView({1}));
        retained.push_back(shared.reshapeView(DynamicExtents{2, 0}));
        retained.push_back(retained.back().transposeView());
        retained.push_back(shared.reshapeView(DynamicExtents{2, 0}).rowView(1));
        retained.push_back(shared.reshapeView(DynamicExtents{0, 3}).columnView(2));
        retained.push_back(shared.sliceView({0, 0, 1}, {2, 0, 3}));
        broadcast = shared.broadcastView(DynamicExtents{4, 2, 0, 3});
        borrowed = owner.permuteView({2, 0, 1}).unsqueezeView(0).squeezeView({0});
        FATP_ASSERT_EQ(retained.size(), references.size(), "Every retained empty transform needs an oracle shape");
        for (std::size_t index = 0; index < retained.size(); ++index)
        {
            const auto& view = retained[index];
            FATP_ASSERT_TRUE(verifyView(view, references[index], nullptr),
                             "Empty transform shape must match its oracle");
            FATP_ASSERT_EQ(counts.allocations, std::size_t{0}, "Empty transforms allocate no owner element buffers");
            FATP_ASSERT_TRUE(view.empty() && view.data() == nullptr, "Empty transformed data must stay null");
            FATP_ASSERT_EQ(view.layout().storageLength(), owner.layout().storageLength(),
                           "Empty transforms preserve backing storage length");
            FATP_ASSERT_EQ(view.layout().originOffset(), owner.layout().originOffset(),
                           "Empty transforms preserve the source origin without address arithmetic");
            FATP_ASSERT_FALSE(view.layout().minimumOffset().has_value(), "Empty transforms have no reachable minimum");
            FATP_ASSERT_FALSE(view.layout().maximumOffset().has_value(), "Empty transforms have no reachable maximum");
        }
        FATP_ASSERT_TRUE(verifyView(broadcast, broadcastReference, nullptr),
                         "Empty broadcast shape must match its oracle");
        FATP_ASSERT_TRUE(verifyView(borrowed, CoordinateReference{{3, 2, 0}, {}}, nullptr),
                         "Empty borrowed chain must preserve its permuted shape");
        FATP_ASSERT_EQ(counts.allocations, std::size_t{0}, "Broadcast and borrowed empty chains allocate no elements");
    }
    FATP_ASSERT_EQ(counts.deallocations, std::size_t{0}, "Empty shared storage has no element allocation to release");
    for (std::size_t index = 0; index < retained.size(); ++index)
    {
        const auto& view = retained[index];
        const auto chained = view.unsqueezeView(0).squeezeView({0});
        FATP_ASSERT_TRUE(chained.empty() && chained.data() == nullptr,
                         "Empty shared transforms retain a usable lifetime handle after owner destruction");
        FATP_ASSERT_TRUE(verifyView(chained, references[index], nullptr), "Post-destruction empty shape must survive");
    }
    FATP_ASSERT_TRUE(verifyView(broadcast, broadcastReference, nullptr),
                     "Post-destruction broadcast shape must survive");
    FATP_ASSERT_TRUE(broadcast.empty() && broadcast.data() == nullptr, "Read-only empty shared broadcast stays usable");
#ifndef NDEBUG
    FATP_ASSERT_THROWS(borrowed.data(), std::runtime_error, "Even empty borrowed chains invalidate with their owner");
#endif
    retained.clear();
    broadcast = {};
    FATP_ASSERT_EQ(counts.allocations, std::size_t{0}, "No empty operation allocates element storage");
    FATP_ASSERT_EQ(counts.deallocations, std::size_t{0}, "Empty lifetime handles never deallocate element storage");

    int storage[8]{};
    const auto external = TensorView<int>::borrow(storage,
        TensorLayout(8, 5, DynamicExtents{2, 0, 3}, TensorStrides{9, 3, -1}));
    const auto transformed = external.sliceView({All, All, Slice{{}, {}, -1}})
        .permuteView({2, 0, 1}).unsqueezeView(0).squeezeView({0}).reshapeView(DynamicExtents{0, 6});
    FATP_ASSERT_TRUE(verifyView(transformed, CoordinateReference{{0, 6}, {}}, storage),
                     "An empty external mapping must preserve its requested final shape");
    FATP_ASSERT_EQ(transformed.layout().storageLength(), std::size_t{8},
                   "Empty transforms must not discard backing span");
    FATP_ASSERT_EQ(transformed.layout().originOffset(), std::ptrdiff_t{5},
                   "Empty transforms must not reset a nonzero origin");
    return true;
}

FATP_TEST_CASE(extreme_slice_bounds_and_validation)
{
    constexpr auto minimum = std::numeric_limits<std::ptrdiff_t>::min();
    constexpr auto maximum = std::numeric_limits<std::ptrdiff_t>::max();
    Tensor<int> source({4});
    std::iota(source.begin(), source.end(), 100);
    FATP_ASSERT_TRUE(verifyView(source.sliceView({Slice{minimum, maximum, 1}}), denseReference({4}), source.data()),
                     "Extreme positive endpoints must clip without signed overflow");
    FATP_ASSERT_TRUE(verifyView(source.sliceView({Slice{maximum, minimum, -1}}),
                               CoordinateReference{{4}, {3, 2, 1, 0}}, source.data()),
                     "Extreme negative endpoints must clip without signed overflow");
    FATP_ASSERT_TRUE(verifyView(source.sliceView({Slice{{}, {}, minimum}}),
                               CoordinateReference{{1}, {3}}, source.data()),
                     "Minimum signed step selects one element");
    FATP_ASSERT_TRUE(verifyView(source.sliceView({Slice{{}, {}, maximum}}),
                               CoordinateReference{{1}, {0}}, source.data()),
                     "Maximum signed step selects one element");
    FATP_ASSERT_TRUE(verifyView(source.sliceView({Ellipsis, NewAxis}),
                               CoordinateReference{{4, 1}, {0, 1, 2, 3}}, source.data()),
                     "Trailing NewAxis follows expanded ellipsis axes");
    FATP_ASSERT_TRUE(verifyView(source.sliceView({All, Ellipsis, NewAxis}),
                               CoordinateReference{{4, 1}, {0, 1, 2, 3}}, source.data()),
                     "Zero-width ellipsis must consume no axis");
    FATP_ASSERT_THROWS(source.sliceView({minimum}), std::out_of_range, "Minimum integer index is out of range");
    FATP_ASSERT_THROWS(source.sliceView({maximum}), std::out_of_range, "Maximum integer index is out of range");
    FATP_ASSERT_THROWS(source.permuteView({1}), std::out_of_range, "Permutation rejects out-of-range axes");
    FATP_ASSERT_THROWS(source.permuteView({}), std::invalid_argument, "Permutation requires exact rank");
    FATP_ASSERT_THROWS(source.unsqueezeView(minimum), std::out_of_range, "Extreme negative insertion is rejected");
    FATP_ASSERT_THROWS(source.unsqueezeView(maximum), std::out_of_range, "Extreme positive insertion is rejected");
    FATP_ASSERT_THROWS(source.reshapeView(DynamicExtents{3}), std::invalid_argument, "Reshape preserves count");
    FATP_ASSERT_THROWS(source.broadcastView(DynamicExtents{3}), std::invalid_argument, "Broadcast validates extents");
    FATP_ASSERT_THROWS(source.sliceView({0}, {5}), std::out_of_range, "Rectangular endpoints are not clipped");
    FATP_ASSERT_THROWS(source.sliceView({3}, {1}), std::out_of_range, "Rectangular endpoints must be ordered");
    FATP_ASSERT_THROWS(source.sliceView({0, 0}, {1, 1}), std::invalid_argument, "Rectangular slicing validates rank");
    Tensor<int> singleton({1}, 7);
    FATP_ASSERT_THROWS(singleton.squeezeView({0, -1}), std::invalid_argument,
                       "Duplicate normalized squeeze axes are rejected");
    const auto extremeStride = TensorView<int>::borrow(singleton.data(),
        TensorLayout(1, 0, DynamicExtents{1}, TensorStrides{maximum}));
    FATP_ASSERT_THROWS(extremeStride.sliceView({Slice{{}, {}, 2}}), std::overflow_error,
                       "Unrepresentable result strides are rejected even for singleton mappings");
    Tensor<int> matrix({2, 2}, 8);
    FATP_ASSERT_THROWS(matrix.permuteView({0, -2}), std::invalid_argument,
                       "Duplicate normalized permutation axes are rejected");
    FATP_ASSERT_THROWS(matrix.transposeView().reshapeView(DynamicExtents{4}), std::invalid_argument,
                       "Noncontiguous reshape must remain explicit materialization");
    FATP_ASSERT_TRUE(verifyView(source.asView(), denseReference({4}), source.data()),
                     "Rejected transforms must not modify their source metadata or storage");
    return true;
}

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
    FATP_ASSERT_THROWS(hugeEmpty.sliceView({Slice{}, All, All}), std::overflow_error,
                       "Signed Slice descriptors require an extent representable in ptrdiff_t, unlike All");
    FATP_ASSERT_THROWS(hugeEmpty.sliceView({Slice{0, 1, 1}, All, All}), std::overflow_error,
                       "Even bounded Slice requests reject an unrepresentable source-axis extent");

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
    FATP_RUN_TEST_NS(runner, tensor_slice, coordinate_oracle_anchors);
    FATP_RUN_TEST_NS(runner, tensor_slice, exhaustive_small_signed_slices);
    FATP_RUN_TEST_NS(runner, tensor_slice, exhaustive_small_permutations_and_axis_transforms);
    FATP_RUN_TEST_NS(runner, tensor_slice, randomized_composed_transform_oracle);
    FATP_RUN_TEST_NS(runner, tensor_slice, rectangular_convenience_and_broadcast_oracle);
    FATP_RUN_TEST_NS(runner, tensor_slice, transform_allocation_aliasing_and_lifetime);
    FATP_RUN_TEST_NS(runner, tensor_slice, empty_transform_allocation_and_lifetime);
    FATP_RUN_TEST_NS(runner, tensor_slice, extreme_slice_bounds_and_validation);
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
