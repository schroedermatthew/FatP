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

#include <array>
#include <cstddef>
#include <functional>
#include <memory>
#include <numeric>
#include <span>
#include <stdexcept>
#include <type_traits>
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
                     "Multi-input concatenate should use cumulative axis boundaries");
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

} // namespace fat_p::testing::tensor_selection

namespace fat_p::testing
{

bool test_TensorSelection()
{
    FATP_PRINT_HEADER(TENSOR SELECTION)
    TestRunner runner;
    FATP_RUN_TEST_NS(runner, tensor_selection, stack_pair_and_many);
    FATP_RUN_TEST_NS(runner, tensor_selection, concatenate_pair_many_and_strided);
    FATP_RUN_TEST_NS(runner, tensor_selection, take_and_take_along_axis);
    FATP_RUN_TEST_NS(runner, tensor_selection, gather_nd_and_zero_depth);
    FATP_RUN_TEST_NS(runner, tensor_selection, validation_errors);
    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_TensorSelection() ? 0 : 1;
}
#endif
