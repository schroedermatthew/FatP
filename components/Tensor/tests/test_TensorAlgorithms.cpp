/**
 * @file test_TensorAlgorithms.cpp
 * @brief Unified Tensor iteration-plan and serial-kernel tests.
 */

/*
FATP_META:
  meta_version: 1
  component: TensorAlgorithms
  file_role: test
  path: components/Tensor/tests/test_TensorAlgorithms.cpp
  namespace: fat_p::testing::tensor_algorithms
  layer: Testing
  summary: "Differential tests for signed, broadcast, and multi-operand Tensor kernels."
  api_stability: in_work
  related:
    docs:
      - components/Tensor/docs/Design Note - Tensor Architecture Additions Plan.md
    headers:
      - include/fat_p/TensorAlgorithms.h
      - include/fat_p/tensor/TensorIterationPlan.h
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
#include "TensorAlgorithms.h"
#include "TensorTestSupport.h"
#include "tensor/TensorIterationPlan.h"

#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <numeric>
#include <random>
#include <stdexcept>
#include <vector>

namespace fat_p::testing::tensor_algorithms
{

template <typename T>
class TaggedAllocator
{
public:
    using value_type = T;

    TaggedAllocator() = default;
    explicit TaggedAllocator(int id) : mId(id) {}

    template <typename U>
    TaggedAllocator(const TaggedAllocator<U>& other) : mId(other.id())
    {
    }

    [[nodiscard]] T* allocate(std::size_t count) { return std::allocator<T>{}.allocate(count); }
    void deallocate(T* storage, std::size_t count) noexcept { std::allocator<T>{}.deallocate(storage, count); }
    [[nodiscard]] int id() const noexcept { return mId; }
    [[nodiscard]] TaggedAllocator select_on_container_copy_construction() const
    {
        return TaggedAllocator(mId + 100);
    }

    template <typename U>
    struct rebind
    {
        using other = TaggedAllocator<U>;
    };

    template <typename U>
    [[nodiscard]] bool operator==(const TaggedAllocator<U>& other) const noexcept
    {
        return mId == other.id();
    }

private:
    int mId = 0;
};

template <typename Left, typename Right>
concept ApproximatelyComparable = requires(const Left& left, const Right& right) {
    approxEqual(left, right, 0.1);
};

static_assert(ApproximatelyComparable<Tensor<double>, Tensor<double>>);
static_assert(!ApproximatelyComparable<Tensor<int>, Tensor<int>>);

[[nodiscard]] TensorLayout makeSmallLayout(const std::vector<std::size_t>& extents,
                                           const TensorStrides& strides)
{
    const DynamicExtents checkedExtents(extents);
    if (checkedExtents.logicalSize() == 0)
    {
        return TensorLayout(0, 0, checkedExtents, strides);
    }
    std::ptrdiff_t minimum = 0;
    std::ptrdiff_t maximum = 0;
    for (std::size_t axis = 0; axis < extents.size(); ++axis)
    {
        const auto contribution = static_cast<std::ptrdiff_t>(extents[axis] - 1) * strides[axis];
        minimum += std::min<std::ptrdiff_t>(0, contribution);
        maximum += std::max<std::ptrdiff_t>(0, contribution);
    }
    const auto origin = -minimum;
    const auto storageLength = static_cast<std::size_t>(maximum - minimum + 1);
    return TensorLayout(storageLength, origin, checkedExtents, strides);
}

[[nodiscard]] std::ptrdiff_t expectedBroadcastOffset(const DynamicExtents& target,
                                                     const TensorLayout& operand,
                                                     std::size_t linearIndex)
{
    std::vector<std::size_t> coordinates(target.rank(), 0);
    auto remainder = linearIndex;
    for (std::size_t reverseAxis = target.rank(); reverseAxis > 0; --reverseAxis)
    {
        const auto axis = reverseAxis - 1;
        coordinates[axis] = remainder % target[axis];
        remainder /= target[axis];
    }
    auto offset = operand.originOffset();
    const auto padding = target.rank() - operand.rank();
    for (std::size_t sourceAxis = 0; sourceAxis < operand.rank(); ++sourceAxis)
    {
        const auto targetAxis = padding + sourceAxis;
        const auto coordinate = operand.extents()[sourceAxis] == 1 ? std::size_t{0} : coordinates[targetAxis];
        offset += static_cast<std::ptrdiff_t>(coordinate) * operand.strides()[sourceAxis];
    }
    return offset;
}

FATP_TEST_CASE(counted_signed_iteration)
{
    const TensorLayout reversed(6, 2, DynamicExtents{2, 3}, TensorStrides{3, -1});
    const tensor_detail::TensorIterationPlan plan(DynamicExtents{2, 3}, {std::cref(reversed)});
    std::vector<std::ptrdiff_t> visited;
    plan.forEachOffset([&](std::size_t, const auto& offsets) { visited.push_back(offsets[0]); });
    FATP_ASSERT_TRUE(visited == std::vector<std::ptrdiff_t>({2, 1, 0, 5, 4, 3}),
                     "Counted plan should carry negative offsets in logical order");
    FATP_ASSERT_EQ(plan.logicalSize(), std::size_t{6}, "Plan should retain the pre-coalescing logical size");

    const TensorLayout scalar = TensorLayout::contiguous(DynamicExtents{});
    const tensor_detail::TensorIterationPlan scalarPlan(DynamicExtents{}, {std::cref(scalar)});
    std::size_t scalarVisits = 0;
    std::ptrdiff_t scalarOffset = -1;
    scalarPlan.forEachOffset([&](std::size_t, const auto& offsets) {
        ++scalarVisits;
        scalarOffset = offsets[0];
    });
    FATP_ASSERT_EQ(scalarVisits, std::size_t{1}, "Rank-zero plan should visit exactly once");
    FATP_ASSERT_EQ(scalarOffset, std::ptrdiff_t{0}, "Rank-zero plan should visit its origin");

    const TensorLayout empty = TensorLayout::contiguous(DynamicExtents{2, 0, 3});
    const tensor_detail::TensorIterationPlan emptyPlan(empty.extents(), {std::cref(empty)});
    std::size_t emptyVisits = 0;
    emptyPlan.forEachOffset([&](std::size_t, const auto&) { ++emptyVisits; });
    FATP_ASSERT_EQ(emptyVisits, std::size_t{0}, "Zero-extent plan should never invoke its callback");
    return true;
}

FATP_TEST_CASE(fill_copy_and_negative_transform)
{
    int storage[]{0, 0, 0, 0, 0, 0};
    auto reversed = TensorView<int>::borrow(storage, TensorLayout(6, 2, DynamicExtents{2, 3}, TensorStrides{3, -1}));
    tensor_detail::fillKernel(reversed, 4);
    FATP_ASSERT_TRUE(std::vector<int>(storage, storage + 6) == std::vector<int>({4, 4, 4, 4, 4, 4}),
                     "fill kernel should visit every negative-stride destination exactly once");

    storage[0] = 1;
    storage[1] = 2;
    storage[2] = 3;
    storage[3] = 4;
    storage[4] = 5;
    storage[5] = 6;
    auto transformed = transform(reversed, [](int value) { return value * 10; });
    FATP_ASSERT_TRUE(std::vector<int>(transformed.begin(), transformed.end()) ==
                         std::vector<int>({30, 20, 10, 60, 50, 40}),
                     "Unary transform should materialize signed logical order");

    Tensor<int> copied({2, 3});
    tensor_detail::copyKernel(reversed, copied);
    FATP_ASSERT_TRUE(std::vector<int>(copied.begin(), copied.end()) == std::vector<int>({3, 2, 1, 6, 5, 4}),
                     "Copy kernel should materialize negative-stride logical order");
    Tensor<int> wrongShape({6});
    FATP_ASSERT_THROWS(tensor_detail::copyKernel(reversed, wrongShape), std::invalid_argument,
                       "Copy kernel should reject mismatched destination extents");
    return true;
}

FATP_TEST_CASE(randomized_multi_layout_plan_oracle)
{
    std::mt19937_64 random(0x51A7EDULL);
    for (std::size_t sample = 0; sample < 160; ++sample)
    {
        const auto rank = static_cast<std::size_t>(random() % 5);
        std::vector<std::size_t> targetValues(rank, 1);
        for (auto& extent : targetValues)
        {
            extent = static_cast<std::size_t>(random() % 4);
        }
        const DynamicExtents target(targetValues);

        const auto makeOperand = [&](std::size_t salt) {
            const auto sourceRank = rank == 0 ? std::size_t{0}
                                              : static_cast<std::size_t>((random() + salt) % (rank + 1));
            const auto padding = rank - sourceRank;
            std::vector<std::size_t> sourceExtents(sourceRank, 1);
            TensorStrides sourceStrides(sourceRank, 0);
            for (std::size_t axis = 0; axis < sourceRank; ++axis)
            {
                const auto targetExtent = targetValues[padding + axis];
                sourceExtents[axis] = ((random() + salt + axis) & 1U) == 0U ? std::size_t{1} : targetExtent;
                sourceStrides[axis] = static_cast<std::ptrdiff_t>(random() % 11) - 5;
            }
            return makeSmallLayout(sourceExtents, sourceStrides);
        };

        const auto first = makeOperand(1);
        const auto second = makeOperand(2);
        const auto third = makeOperand(3);
        const tensor_detail::TensorIterationPlan one(target, {std::cref(first)});
        const tensor_detail::TensorIterationPlan two(target, {std::cref(first), std::cref(second)});
        const tensor_detail::TensorIterationPlan three(target,
                                                        {std::cref(first), std::cref(second), std::cref(third)});

        const auto verify = [&](const auto& plan, const std::vector<std::reference_wrapper<const TensorLayout>>& layouts) {
            std::vector<std::vector<std::ptrdiff_t>> actual;
            plan.forEachOffset([&](std::size_t, const auto& offsets) { actual.push_back(offsets); });
            if (actual.size() != target.logicalSize())
            {
                return false;
            }
            for (std::size_t linear = 0; linear < actual.size(); ++linear)
            {
                for (std::size_t operand = 0; operand < layouts.size(); ++operand)
                {
                    if (actual[linear][operand] != expectedBroadcastOffset(target, layouts[operand].get(), linear))
                    {
                        return false;
                    }
                }
            }
            return true;
        };

        FATP_ASSERT_TRUE(verify(one, {std::cref(first)}),
                         "Randomized one-layout plan should match the independent coordinate oracle");
        FATP_ASSERT_TRUE(verify(two, {std::cref(first), std::cref(second)}),
                         "Randomized two-layout plan should match the independent coordinate oracle");
        FATP_ASSERT_TRUE(verify(three, {std::cref(first), std::cref(second), std::cref(third)}),
                         "Randomized three-layout plan should match the independent coordinate oracle");
    }
    return true;
}

FATP_TEST_CASE(binary_broadcast_three_layouts)
{
    int leftStorage[]{1, 2, 3, 4, 5, 6};
    int rightStorage[]{10, 20, 30};
    int outputStorage[]{-1, -1, -1, -1, -1, -1, -1};
    const auto left = TensorView<const int>::borrow(
        leftStorage, TensorLayout(6, 2, DynamicExtents{2, 3}, TensorStrides{3, -1}));
    const auto right = TensorView<const int>::borrow(
        rightStorage, TensorLayout(3, 0, DynamicExtents{1, 3}, TensorStrides{3, 1}));
    auto output = TensorView<int>::borrow(
        outputStorage, TensorLayout(7, 0, DynamicExtents{2, 3}, TensorStrides{4, 1}));

    tensor_detail::binaryKernel(left, right, output, std::plus<int>{});
    FATP_ASSERT_TRUE(std::vector<int>(outputStorage, outputStorage + 7) ==
                         std::vector<int>({13, 22, 31, -1, 16, 25, 34}),
                     "Three-layout kernel should combine reversed, broadcast, and padded mappings");

    auto owned = add(left, right);
    FATP_ASSERT_TRUE(std::vector<int>(owned.begin(), owned.end()) == std::vector<int>({13, 22, 31, 16, 25, 34}),
                     "Public add should allocate canonical broadcast output");
    return true;
}

FATP_TEST_CASE(equality_approximation_and_layout_independent_hash)
{
    Tensor<int> owner({2, 3});
    std::iota(owner.begin(), owner.end(), 1);
    int transposedStorage[]{1, 4, 2, 5, 3, 6};
    const auto sameLogical = TensorView<const int>::borrow(
        transposedStorage, TensorLayout(6, 0, DynamicExtents{2, 3}, TensorStrides{1, 2}));
    FATP_ASSERT_TRUE(exactEqual(owner, sameLogical), "Equality should compare logical values, not physical layout");
    FATP_ASSERT_EQ(tensor_detail::hashKernel(owner, std::hash<int>{}),
                   tensor_detail::hashKernel(sameLogical, std::hash<int>{}),
                   "Hash should be independent of physical layout");

    Tensor<double> closeLeft({2}, 1.0);
    Tensor<double> closeRight({2}, 1.0 + 1e-8);
    FATP_ASSERT_TRUE(approxEqual(closeLeft, closeRight, 1e-7), "Approximate equality should honor tolerance");
    FATP_ASSERT_FALSE(approxEqual(closeLeft, closeRight, 1e-10),
                      "Approximate equality should report a value outside tolerance");

    const auto infinity = std::numeric_limits<double>::infinity();
    const Tensor<double> positiveInfinity({1}, infinity);
    const Tensor<double> negativeInfinity({1}, -infinity);
    FATP_ASSERT_TRUE(approxEqual(positiveInfinity, positiveInfinity, 0.0),
                     "Equal same-sign infinities should compare approximately equal");
    FATP_ASSERT_FALSE(approxEqual(positiveInfinity, negativeInfinity, 1.0),
                      "Opposite infinities should not compare approximately equal");
    const Tensor<double> finite({1}, 1.0);
    FATP_ASSERT_FALSE(approxEqual(positiveInfinity, finite, 1e-6, 1e-5),
                      "An infinity must not compare equal to a finite value through relative tolerance");
    FATP_ASSERT_FALSE(approxEqual(finite, positiveInfinity, 1e-6, 1e-5),
                      "Infinity handling should be symmetric");
    return true;
}

FATP_TEST_CASE(large_injective_destination_and_owner_allocator_selection)
{
    std::vector<int> storage(600'002, 0);
    auto large = TensorView<int>::borrow(
        storage.data(), TensorLayout(storage.size(), 0, DynamicExtents{300'000, 2}, TensorStrides{2, 3}));
    tensor_detail::fillKernel(large, 9);
    FATP_ASSERT_EQ(storage[0], 9, "Large exact rank-two mapping should accept writes at its first offset");
    FATP_ASSERT_EQ(storage[600'001], 9, "Large exact rank-two mapping should accept writes at its last offset");

    using Allocator = TaggedAllocator<int>;
    Tensor<int, Allocator> owner(std::allocator_arg, Allocator(7), DynamicExtents{2}, 5);
    const auto view = owner.asConstView();
    auto result = add(view, owner);
    static_assert(std::same_as<decltype(result), Tensor<int, Allocator>>);
    FATP_ASSERT_EQ(result.get_allocator().id(), 107,
                   "A binary algorithm should use SOCCC from the first owner argument left-to-right");
    FATP_ASSERT_EQ(result[1], 10, "Allocator selection should not alter binary values");
    return true;
}

FATP_TEST_CASE(zero_extent_broadcast)
{
    const Tensor<int> empty({0, 3});
    const Tensor<int> singleton({1, 3}, 7);
    const auto result = add(empty, singleton);
    FATP_ASSERT_TRUE(result.extents() == DynamicExtents({0, 3}),
                     "Broadcasting zero with one should preserve the zero extent");
    FATP_ASSERT_TRUE(result.empty(), "Zero-extent broadcast result should contain no elements");
    return true;
}

FATP_TEST_CASE(integral_arithmetic_is_checked)
{
    const Tensor<int> maximum({1}, std::numeric_limits<int>::max());
    const Tensor<int> one({1}, 1);
    FATP_ASSERT_THROWS(add(maximum, one), std::overflow_error,
                       "Signed addition overflow should be reported before evaluation");

    const Tensor<int> minimum({1}, std::numeric_limits<int>::lowest());
    const Tensor<int> negativeOne({1}, -1);
    FATP_ASSERT_THROWS(multiply(minimum, negativeOne), std::overflow_error,
                       "Signed multiplication overflow should be reported before evaluation");

    const Tensor<unsigned> zero({1}, 0U);
    const Tensor<unsigned> unsignedOne({1}, 1U);
    FATP_ASSERT_THROWS(subtract(zero, unsignedOne), std::overflow_error,
                       "Unsigned subtraction underflow should be reported before evaluation");

    FATP_ASSERT_EQ(maximum[0], std::numeric_limits<int>::max(),
                   "Checked arithmetic must not modify its input Tensor");
    return true;
}

} // namespace fat_p::testing::tensor_algorithms

namespace fat_p::testing
{

bool test_TensorAlgorithms()
{
    FATP_PRINT_HEADER(TENSOR ALGORITHMS)
    TestRunner runner;
    FATP_RUN_TEST_NS(runner, tensor_algorithms, counted_signed_iteration);
    FATP_RUN_TEST_NS(runner, tensor_algorithms, fill_copy_and_negative_transform);
    FATP_RUN_TEST_NS(runner, tensor_algorithms, randomized_multi_layout_plan_oracle);
    FATP_RUN_TEST_NS(runner, tensor_algorithms, binary_broadcast_three_layouts);
    FATP_RUN_TEST_NS(runner, tensor_algorithms, equality_approximation_and_layout_independent_hash);
    FATP_RUN_TEST_NS(runner, tensor_algorithms, large_injective_destination_and_owner_allocator_selection);
    FATP_RUN_TEST_NS(runner, tensor_algorithms, zero_extent_broadcast);
    FATP_RUN_TEST_NS(runner, tensor_algorithms, integral_arithmetic_is_checked);
    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_TensorAlgorithms() ? 0 : 1;
}
#endif
