/** @file test_TensorReductions.cpp @brief Tensor axis-reduction conformance tests. */

/*
FATP_META:
  meta_version: 1
  component: TensorReductions
  file_role: test
  path: components/Tensor/tests/test_TensorReductions.cpp
  namespace: fat_p::testing::tensor_reductions
  layer: Testing
  summary: "Axis, dtype, empty-domain, NaN, and overflow reduction tests."
  api_stability: in_work
  related:
    docs:
      - components/Tensor/docs/Design Note - Tensor Architecture Additions Plan.md
    headers:
      - include/fat_p/TensorReductions.h
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
#include "TensorReductions.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace fat_p::testing::tensor_reductions
{

template <typename T>
class SocccAllocator
{
public:
    using value_type = T;

    explicit SocccAllocator(int identity = 0) noexcept
        : mIdentity(identity)
    {
    }

    template <typename U>
    SocccAllocator(const SocccAllocator<U>& other) noexcept
        : mIdentity(other.identity())
    {
    }

    [[nodiscard]] T* allocate(std::size_t count) { return std::allocator<T>{}.allocate(count); }
    void deallocate(T* storage, std::size_t count) noexcept
    {
        std::allocator<T>{}.deallocate(storage, count);
    }

    [[nodiscard]] SocccAllocator select_on_container_copy_construction() const noexcept
    {
        return SocccAllocator(mIdentity + 100);
    }

    [[nodiscard]] int identity() const noexcept { return mIdentity; }

    template <typename U>
    [[nodiscard]] bool operator==(const SocccAllocator<U>& other) const noexcept
    {
        return mIdentity == other.identity();
    }

private:
    int mIdentity = 0;
};

FATP_TEST_CASE(all_and_axis_reductions)
{
    Tensor<int> values({2, 3});
    std::iota(values.begin(), values.end(), 1);

    const auto total = sum(values);
    static_assert(std::same_as<typename decltype(total)::value_type, std::int64_t>);
    FATP_ASSERT_EQ(total.rank(), std::size_t{0}, "All-axis sum should produce a scalar");
    FATP_ASSERT_EQ(total(), std::int64_t{21}, "All-axis sum should widen small integers");
    const auto totalProduct = product(values);
    FATP_ASSERT_EQ(totalProduct(), std::int64_t{720}, "Product should use the same widened accumulator");

    const auto rowSums = sum(values, {1});
    FATP_ASSERT_TRUE(rowSums.extents() == DynamicExtents({2}), "Reducing columns should retain rows");
    FATP_ASSERT_TRUE(std::vector<std::int64_t>(rowSums.begin(), rowSums.end()) ==
                         std::vector<std::int64_t>({6, 15}),
                     "Axis sum should preserve output order");

    const auto columnMeans = mean(values, {-2}, true);
    FATP_ASSERT_TRUE(columnMeans.extents() == DynamicExtents({1, 3}),
                     "keepDimensions should retain a singleton reduced axis");
    FATP_ASSERT_EQ(columnMeans(0, 0), 2.5, "Mean should return floating-point results for integral inputs");
    FATP_ASSERT_EQ(columnMeans(0, 2), 4.5, "Negative axes should normalize against source rank");

    Tensor<int, std::allocator<int>> standardAllocated(std::allocator_arg, std::allocator<int>{},
                                                        DynamicExtents({2}), 3);
    const auto inherited = sum(standardAllocated);
    static_assert(std::same_as<typename decltype(inherited)::allocator_type,
                               std::allocator<std::int64_t>>);
    FATP_ASSERT_EQ(inherited(), std::int64_t{6},
                   "Type-changing reductions should rebind the source owner allocator");
    const auto explicitMean = mean(standardAllocated, std::allocator<double>{});
    static_assert(std::same_as<typename decltype(explicitMean)::allocator_type,
                               std::allocator<double>>);
    FATP_ASSERT_EQ(explicitMean(), 3.0, "Reductions should also accept an explicit result allocator");

    Tensor<int, SocccAllocator<int>> stateful(std::allocator_arg, SocccAllocator<int>(7),
                                               DynamicExtents({2}), 4);
    const auto statefulSum = sum(stateful);
    static_assert(std::same_as<typename decltype(statefulSum)::allocator_type,
                               SocccAllocator<std::int64_t>>);
    FATP_ASSERT_EQ(statefulSum.get_allocator().identity(), 107,
                   "Default reductions should apply SOCCC after rebinding allocator state");
    return true;
}

FATP_TEST_CASE(strided_scalar_and_argument_reductions)
{
    Tensor<int> values({2, 3});
    std::iota(values.begin(), values.end(), 1);
    const auto transposed = values.transposeView();
    const auto transposedRows = sum(transposed, {1});
    FATP_ASSERT_TRUE(std::vector<std::int64_t>(transposedRows.begin(), transposedRows.end()) ==
                         std::vector<std::int64_t>({5, 7, 9}),
                     "Reductions should consume non-contiguous views directly");

    const auto reversed = values.sliceView({All, Slice{std::nullopt, std::nullopt, -1}});
    const auto reversedRows = sum(reversed, {1});
    FATP_ASSERT_TRUE(std::vector<std::int64_t>(reversedRows.begin(), reversedRows.end()) ==
                         std::vector<std::int64_t>({6, 15}),
                     "Reduction order should support negative strides");

    const auto maxima = argmax(values, {1});
    const auto minima = argmin(values, {1});
    FATP_ASSERT_TRUE(std::vector<std::size_t>(maxima.begin(), maxima.end()) == std::vector<std::size_t>({2, 2}),
                     "argmax should return the first flattened reduced coordinate");
    FATP_ASSERT_TRUE(std::vector<std::size_t>(minima.begin(), minima.end()) == std::vector<std::size_t>({0, 0}),
                     "argmin should return the first flattened reduced coordinate");

    Tensor<int> scalar({}, 9);
    const auto scalarSum = sum(scalar);
    const auto scalarMean = mean(scalar);
    FATP_ASSERT_EQ(scalarSum(), std::int64_t{9}, "Rank-zero reduction should preserve its one value");
    FATP_ASSERT_EQ(scalarMean(), 9.0, "Rank-zero mean should divide by one");
    return true;
}

FATP_TEST_CASE(empty_initial_nan_and_ties)
{
    Tensor<int> emptyDomains({0, 3});
    const auto sums = sum(emptyDomains, {0});
    const auto products = product(emptyDomains, {0});
    FATP_ASSERT_TRUE(std::vector<std::int64_t>(sums.begin(), sums.end()) ==
                         std::vector<std::int64_t>({0, 0, 0}),
                     "Empty sums should use the additive identity per output");
    FATP_ASSERT_TRUE(std::vector<std::int64_t>(products.begin(), products.end()) ==
                         std::vector<std::int64_t>({1, 1, 1}),
                     "Empty products should use the multiplicative identity per output");
    FATP_ASSERT_THROWS(mean(emptyDomains, {0}), std::domain_error,
                       "Mean should reject a nonempty output containing empty domains");
    FATP_ASSERT_THROWS(min(emptyDomains, {0}), std::domain_error,
                       "Minimum should reject an empty domain without an initial value");
    const auto initialized = min(emptyDomains, {0}, false, 42);
    FATP_ASSERT_TRUE(std::vector<int>(initialized.begin(), initialized.end()) == std::vector<int>({42, 42, 42}),
                     "An explicit initial value should define empty extrema");
    FATP_ASSERT_TRUE(mean(emptyDomains, {1}).empty(),
                     "No empty-domain error is needed when the output itself is empty");

    Tensor<double> special({3});
    special[0] = 2.0;
    special[1] = std::numeric_limits<double>::quiet_NaN();
    special[2] = 1.0;
    const auto specialMinimum = min(special);
    const auto specialArgument = argmin(special);
    FATP_ASSERT_TRUE(std::isnan(specialMinimum()), "Extrema should propagate the first NaN");
    FATP_ASSERT_EQ(specialArgument(), std::size_t{1}, "Arg-extrema should point at the first propagated NaN");

    Tensor<int> ties({4});
    ties[0] = 3;
    ties[1] = 1;
    ties[2] = 1;
    ties[3] = 4;
    const auto tieArgument = argmin(ties);
    FATP_ASSERT_EQ(tieArgument(), std::size_t{1}, "Argument reductions should choose the first tie");
    return true;
}

FATP_TEST_CASE(multiple_axes_bool_and_overflow)
{
    Tensor<int> cube({2, 2, 2});
    std::iota(cube.begin(), cube.end(), 1);
    const auto indices = argmax(cube, {0, 2});
    FATP_ASSERT_TRUE(std::vector<std::size_t>(indices.begin(), indices.end()) == std::vector<std::size_t>({3, 3}),
                     "Multiple reduced axes should use row-major flattened coordinates");
    const auto kept = sum(cube, {0, -1}, true);
    FATP_ASSERT_TRUE(kept.extents() == DynamicExtents({1, 2, 1}),
                     "Multiple axes should preserve ordered singleton dimensions");
    FATP_ASSERT_EQ(kept(0, 0, 0), std::int64_t{14}, "Multiple-axis sum should combine the correct domain");
    FATP_ASSERT_EQ(kept(0, 1, 0), std::int64_t{22}, "Multiple-axis sum should preserve the middle axis");

    Tensor<bool> flags({4});
    flags[0] = true;
    flags[1] = false;
    flags[2] = true;
    flags[3] = true;
    const auto flagSum = sum(flags);
    FATP_ASSERT_EQ(flagSum(), std::size_t{3}, "Boolean sum should count true values");

    Tensor<std::int64_t> overflow({2});
    overflow[0] = std::numeric_limits<std::int64_t>::max();
    overflow[1] = 1;
    FATP_ASSERT_THROWS(sum(overflow), std::overflow_error,
                       "Integral accumulator overflow should be reported before evaluation");
    FATP_ASSERT_THROWS(sum(cube, {0, 0}), std::invalid_argument,
                       "Duplicate reduction axes should be rejected");
    return true;
}

} // namespace fat_p::testing::tensor_reductions

namespace fat_p::testing
{

bool test_TensorReductions()
{
    FATP_PRINT_HEADER(TENSOR REDUCTIONS)
    TestRunner runner;
    FATP_RUN_TEST_NS(runner, tensor_reductions, all_and_axis_reductions);
    FATP_RUN_TEST_NS(runner, tensor_reductions, strided_scalar_and_argument_reductions);
    FATP_RUN_TEST_NS(runner, tensor_reductions, empty_initial_nan_and_ties);
    FATP_RUN_TEST_NS(runner, tensor_reductions, multiple_axes_bool_and_overflow);
    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_TensorReductions() ? 0 : 1;
}
#endif
