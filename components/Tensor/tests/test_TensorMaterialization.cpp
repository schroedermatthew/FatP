/**
 * @file test_TensorMaterialization.cpp
 * @brief Rank-zero result publication without element construction or extra allocation.
 */

/*
FATP_META:
  meta_version: 1
  component: Tensor
  file_role: test
  path: components/Tensor/tests/test_TensorMaterialization.cpp
  namespace: fat_p::testing::tensor_materialization
  layer: Testing
  summary: "Assignment-only element and allocation-failure regressions for scalar materialization."
  api_stability: in_work
  related:
    docs:
      - components/Tensor/docs/Design Note - Tensor Semantic Contract.md
    headers:
      - include/fat_p/TensorAlgorithms.h
      - include/fat_p/TensorRanked.h
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
#include "TensorAlgorithms.h"
#include "TensorContractions.h"
#include "TensorExecution.h"
#include "TensorInterop.h"
#include "TensorMatmul.h"
#include "TensorRanked.h"
#include "TensorReductions.h"
#include "TensorSelection.h"

#include <array>
#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace fat_p::testing::tensor_materialization
{

struct AssignmentOnly
{
    int value = 0;

    AssignmentOnly() = default;
    AssignmentOnly(const AssignmentOnly&) = delete;
    AssignmentOnly(AssignmentOnly&&) = delete;
    AssignmentOnly& operator=(const AssignmentOnly&) = default;
};

static_assert(std::is_default_constructible_v<AssignmentOnly>);
static_assert(std::is_copy_assignable_v<AssignmentOnly>);
static_assert(!std::is_copy_constructible_v<AssignmentOnly>);
static_assert(!std::is_move_constructible_v<AssignmentOnly>);

struct AllocationProbe
{
    std::size_t remaining = 1;
    std::size_t attempts = 0;
    std::size_t allocations = 0;
    std::size_t deallocations = 0;
};

// This tracks only element storage, independent of layout and test-framework allocations.
// Limiting the budget to one makes redundant scalar publication allocation observable
// as a failure, including in builds that disable optional copy elision.
template <typename T>
class BudgetAllocator
{
public:
    using value_type = T;

    BudgetAllocator()
        : mProbe(std::make_shared<AllocationProbe>())
    {
    }

    explicit BudgetAllocator(std::shared_ptr<AllocationProbe> probe)
        : mProbe(std::move(probe))
    {
    }

    template <typename U>
    BudgetAllocator(const BudgetAllocator<U>& other)
        : mProbe(other.probe())
    {
    }

    [[nodiscard]] T* allocate(std::size_t count)
    {
        ++mProbe->attempts;
        if (mProbe->remaining == 0)
        {
            throw std::bad_alloc();
        }
        auto* storage = std::allocator<T>{}.allocate(count);
        --mProbe->remaining;
        ++mProbe->allocations;
        return storage;
    }

    void deallocate(T* storage, std::size_t count) noexcept
    {
        ++mProbe->deallocations;
        std::allocator<T>{}.deallocate(storage, count);
    }

    [[nodiscard]] const std::shared_ptr<AllocationProbe>& probe() const noexcept { return mProbe; }

    template <typename U>
    [[nodiscard]] bool operator==(const BudgetAllocator<U>& other) const noexcept
    {
        return mProbe == other.probe();
    }

private:
    std::shared_ptr<AllocationProbe> mProbe;
};

template <typename Value, typename Operation>
bool oneScalarAllocation(const char* operationName, Value expected, Operation operation)
{
    const auto probe = std::make_shared<AllocationProbe>();
    {
        const auto result = operation(BudgetAllocator<Value>(probe));
        const auto borrow = result.asConstView();
        static_assert(tensor_static_rank_v<decltype(result)> == 0);
        FATP_ASSERT_EQ(result.size(), std::size_t{1}, operationName);
        FATP_ASSERT_EQ(result(), expected, operationName);
        FATP_ASSERT_EQ(borrow(), expected, operationName);
        FATP_ASSERT_TRUE(result.get_allocator().probe() == probe, operationName);
        FATP_ASSERT_EQ(probe->attempts, std::size_t{1}, operationName);
        FATP_ASSERT_EQ(probe->allocations, std::size_t{1}, operationName);
        FATP_ASSERT_EQ(probe->deallocations, std::size_t{0}, operationName);
    }
    FATP_ASSERT_EQ(probe->deallocations, std::size_t{1}, operationName);
    return true;
}

FATP_TEST_CASE(assignment_only_scalar_transform)
{
    RankedTensor<AssignmentOnly, 0> source;
    source().value = 42;
    auto sourceBorrow = source.asConstView();
    auto sourceShared = source.asSharedView();
    const auto identity = [](const AssignmentOnly& value) -> const AssignmentOnly& { return value; };

    auto ownerResult = transform(source, identity);
    auto viewResult = transform(sourceBorrow, identity);
    auto sharedResult = transform(sourceShared, identity);
    auto allocatedResult = transform(sourceBorrow, identity, std::allocator<AssignmentOnly>{});
    static_assert(tensor_static_rank_v<decltype(ownerResult)> == 0);
    static_assert(tensor_static_rank_v<decltype(viewResult)> == 0);
    FATP_ASSERT_EQ(ownerResult().value, 42, "Owner transform supports assignment-only elements");
    FATP_ASSERT_EQ(viewResult().value, 42, "Borrowed transform supports assignment-only elements");
    FATP_ASSERT_EQ(sharedResult().value, 42, "Shared transform supports assignment-only elements");
    FATP_ASSERT_EQ(allocatedResult().value, 42, "Explicit allocator preserves assignment-only support");

    ownerResult().value = 7;
    FATP_ASSERT_EQ(source().value, 42, "Materialized transform has independent storage");
    FATP_ASSERT_EQ(sourceBorrow().value, 42, "Source borrow survives transform publication");
    FATP_ASSERT_EQ(sourceShared().value, 42, "Source shared view survives transform publication");
    return true;
}

FATP_TEST_CASE(assignment_only_scalar_clone_gather_and_adapters)
{
    RankedTensor<AssignmentOnly, 0> source;
    source().value = 17;
    const auto borrow = source.asConstView();
    auto cloned = clone(borrow);
    auto allocatedClone = clone(borrow, std::allocator<AssignmentOnly>{});
    auto sharedClone = clone(source.asSharedView());
    FATP_ASSERT_EQ(cloned().value, 17, "Borrowed scalar clone needs only element assignment");
    FATP_ASSERT_EQ(allocatedClone().value, 17, "Allocated scalar clone needs only element assignment");
    FATP_ASSERT_EQ(sharedClone().value, 17, "Shared scalar clone needs only element assignment");
    cloned().value = 99;
    FATP_ASSERT_EQ(borrow().value, 17, "Cloned value is independent and source borrow remains valid");

    RankedTensor<AssignmentOnly, 1> vector(RankedExtents<1>{2});
    vector[0].value = 23;
    vector[1].value = 41;
    RankedTensor<int, 1> indices(RankedExtents<1>{1}, 1);
    auto gathered = gatherND<1>(vector, indices);
    auto allocatedGather = gatherND<1>(vector.asConstView(), indices, std::allocator<AssignmentOnly>{});
    static_assert(tensor_static_rank_v<decltype(gathered)> == 0);
    FATP_ASSERT_EQ(gathered().value, 41, "Scalar gather copies the selected value by assignment");
    FATP_ASSERT_EQ(allocatedGather().value, 41, "Explicit allocator scalar gather accepts assignment-only values");
    gathered().value = 5;
    FATP_ASSERT_EQ(vector[0].value, 23, "Gather preserves the unselected source element");
    FATP_ASSERT_EQ(vector[1].value, 41, "Gather preserves the selected source element");

    Tensor<AssignmentOnly> dynamic(DynamicExtents{});
    dynamic[0].value = 53;
    auto ranked = toRankedTensor<0>(dynamic);
    auto rankedView = toRankedTensor<0>(dynamic.asConstView());
    auto reshaped = reshapeCopy(dynamic, RankedExtents<0>{});
    FATP_ASSERT_EQ(ranked().value, 53, "Rank-zero conversion accepts assignment-only values");
    FATP_ASSERT_EQ(rankedView().value, 53, "View rank-zero conversion accepts assignment-only values");
    FATP_ASSERT_EQ(reshaped().value, 53, "Rank-zero reshape copy accepts assignment-only values");
    ranked().value = 3;
    FATP_ASSERT_EQ(dynamic[0].value, 53, "Rank conversion keeps source values intact");

    StaticTensor<AssignmentOnly, Shape<>> fixed;
    fixed[0].value = 61;
    const auto& fixedReference = fixed;
    auto staticConverted = toRankedTensor(fixedReference);
    static_assert(tensor_static_rank_v<decltype(staticConverted)> == 0);
    FATP_ASSERT_EQ(staticConverted().value, 61, "Static scalar interop accepts assignment-only values");
    staticConverted().value = 2;
    FATP_ASSERT_EQ(fixed[0].value, 61, "Static scalar conversion preserves the source");
    return true;
}

FATP_TEST_CASE(scalar_arithmetic_has_one_result_allocation)
{
    RankedTensor<int, 0> left;
    RankedTensor<int, 0> right;
    left() = 12;
    right() = 3;
    FATP_ASSERT_TRUE(oneScalarAllocation("add scalar tensors", 15,
        [&](auto allocator) { return add(left, right, allocator); }), "add");
    FATP_ASSERT_TRUE(oneScalarAllocation("subtract scalar tensors", 9,
        [&](auto allocator) { return subtract(left, right, allocator); }), "subtract");
    FATP_ASSERT_TRUE(oneScalarAllocation("multiply scalar tensors", 36,
        [&](auto allocator) { return multiply(left, right, allocator); }), "multiply");
    FATP_ASSERT_TRUE(oneScalarAllocation("divide scalar tensors", 4,
        [&](auto allocator) { return divide(left, right, allocator); }), "divide");
    FATP_ASSERT_TRUE(oneScalarAllocation("scalar arithmetic", 14,
        [&](auto allocator) { return add(left, 2, allocator); }), "scalar add");
    FATP_ASSERT_TRUE(oneScalarAllocation("scalar left arithmetic", 12,
        [&](auto allocator) { return divide(36, right, allocator); }), "scalar divide");
    FATP_ASSERT_TRUE(oneScalarAllocation("negate scalar", -12,
        [&](auto allocator) { return negate(left, allocator); }), "negate");
    FATP_ASSERT_TRUE(oneScalarAllocation("absolute scalar", 12,
        [&](auto allocator) { return abs(left, allocator); }), "abs");
    FATP_ASSERT_TRUE(oneScalarAllocation("scalar cast", 12.0,
        [&](auto allocator) { return cast<double>(left, allocator); }), "cast");
    FATP_ASSERT_TRUE(oneScalarAllocation("scalar transform", 24,
        [&](auto allocator) { return transform(left, [](int value) { return value * 2; }, allocator); }),
        "transform");
    FATP_ASSERT_EQ(left(), 12, "Scalar arithmetic preserves the left operand");
    FATP_ASSERT_EQ(right(), 3, "Scalar arithmetic preserves the right operand");
    return true;
}

FATP_TEST_CASE(scalar_reductions_have_one_result_allocation)
{
    RankedTensor<int, 1> source(RankedExtents<1>{3});
    source[0] = 2;
    source[1] = 4;
    source[2] = 3;
    using Sum = TensorSumType<int>;
    using Mean = TensorMeanType<int>;
    FATP_ASSERT_TRUE(oneScalarAllocation("sum", Sum{9},
        [&](auto allocator) { return sum(source, allocator); }), "sum");
    FATP_ASSERT_TRUE(oneScalarAllocation("typed sum", Sum{9},
        [&](auto allocator) { return sum<false, 0>(source, allocator); }), "typed sum");
    FATP_ASSERT_TRUE(oneScalarAllocation("product", Sum{24},
        [&](auto allocator) { return product(source, allocator); }), "product");
    FATP_ASSERT_TRUE(oneScalarAllocation("mean", Mean{3},
        [&](auto allocator) { return mean(source, allocator); }), "mean");
    FATP_ASSERT_TRUE(oneScalarAllocation("min", 2,
        [&](auto allocator) { return min(source, allocator); }), "min");
    FATP_ASSERT_TRUE(oneScalarAllocation("max", 4,
        [&](auto allocator) { return max(source, allocator); }), "max");
    FATP_ASSERT_TRUE(oneScalarAllocation("argmin", std::size_t{0},
        [&](auto allocator) { return argmin(source, allocator); }), "argmin");
    FATP_ASSERT_TRUE(oneScalarAllocation("argmax", std::size_t{1},
        [&](auto allocator) { return argmax(source, allocator); }), "argmax");
    FATP_ASSERT_TRUE(oneScalarAllocation("all", true,
        [&](auto allocator) { return all(source, allocator); }), "all");
    FATP_ASSERT_TRUE(oneScalarAllocation("any", true,
        [&](auto allocator) { return any(source, allocator); }), "any");

    RankedTensor<int, 1> empty(RankedExtents<1>{0});
    FATP_ASSERT_TRUE(oneScalarAllocation("empty sum", Sum{0},
        [&](auto allocator) { return sum(empty, allocator); }), "empty sum");
    FATP_ASSERT_TRUE(oneScalarAllocation("empty product", Sum{1},
        [&](auto allocator) { return product(empty, allocator); }), "empty product");
    FATP_ASSERT_TRUE(oneScalarAllocation("empty all", true,
        [&](auto allocator) { return all(empty, allocator); }), "empty all");
    FATP_ASSERT_TRUE(oneScalarAllocation("empty any", false,
        [&](auto allocator) { return any(empty, allocator); }), "empty any");
    FATP_ASSERT_TRUE(source[0] == 2 && source[1] == 4 && source[2] == 3,
                     "Reductions preserve input values");
    return true;
}

FATP_TEST_CASE(scalar_contractions_have_one_result_allocation)
{
    RankedTensor<int, 1> left(RankedExtents<1>{3}, 2);
    RankedTensor<int, 1> right(RankedExtents<1>{3}, 4);
    constexpr std::array<TensorAxis, 1> axes{0};
    using Product = TensorMatmulType<int>;
    FATP_ASSERT_TRUE(oneScalarAllocation("vector matmul", Product{24},
        [&](auto allocator) { return matmul(left, right, allocator); }), "matmul");
    FATP_ASSERT_TRUE(oneScalarAllocation("dot", Product{24},
        [&](auto allocator) { return dot(left, right, allocator); }), "dot");
    FATP_ASSERT_TRUE(oneScalarAllocation("full tensorDot", Product{24},
        [&](auto allocator) { return tensorDot(left, right, axes, axes, allocator); }), "tensorDot");

    ThreadPool pool(2, 0);
    TensorExecutionOptions options;
    options.grainSize = 1;
    options.minimumWork = 0;
    const std::array contexts{TensorExecutionContext::serial(), TensorExecutionContext::parallel(pool, options)};
    for (const auto& context : contexts)
    {
        FATP_ASSERT_TRUE(oneScalarAllocation("context vector matmul", Product{24},
            [&](auto allocator) { return matmul(left, right, context, allocator); }), "context matmul");
        FATP_ASSERT_TRUE(oneScalarAllocation("context dot", Product{24},
            [&](auto allocator) { return dot(left, right, context, allocator); }), "context dot");
        FATP_ASSERT_TRUE(oneScalarAllocation("context full tensorDot", Product{24},
            [&](auto allocator) { return tensorDot(left, right, axes, axes, context, allocator); }),
            "context tensorDot");
    }

    RankedTensor<int, 1> empty(RankedExtents<1>{0});
    FATP_ASSERT_TRUE(oneScalarAllocation("empty dot", Product{0},
        [&](auto allocator) { return dot(empty, empty, allocator); }), "empty dot");
    FATP_ASSERT_TRUE(oneScalarAllocation("empty full tensorDot", Product{0},
        [&](auto allocator) { return tensorDot(empty, empty, axes, axes, allocator); }), "empty tensorDot");
    return true;
}

FATP_TEST_CASE(scalar_selection_and_adapters_have_one_result_allocation)
{
    RankedTensor<int, 1> vector(RankedExtents<1>{2}, 31);
    RankedTensor<int, 1> indices(RankedExtents<1>{1}, 1);
    FATP_ASSERT_TRUE(oneScalarAllocation("scalar gatherND", 31,
        [&](auto allocator) { return gatherND<1>(vector, indices, allocator); }), "gatherND");
    RankedTensor<int, 0> scalar;
    scalar() = 43;
    FATP_ASSERT_TRUE(oneScalarAllocation("scalar view clone", 43,
        [&](auto allocator) { return clone(scalar.asConstView(), allocator); }), "clone");
    FATP_ASSERT_TRUE(oneScalarAllocation("scalar reshape", 43,
        [&](auto allocator) { return reshapeCopy(scalar, RankedExtents<0>{}, allocator); }), "reshape");

    const auto probe = std::make_shared<AllocationProbe>();
    probe->remaining = 2; // One source element and one result element.
    {
        Tensor<int, BudgetAllocator<int>> source(std::allocator_arg, BudgetAllocator<int>(probe),
                                                  DynamicExtents{}, 59);
        const auto borrow = source.asConstView();
        {
            const auto converted = toRankedTensor<0>(source);
            FATP_ASSERT_EQ(converted(), 59, "Rank-zero adapter preserves its value");
            FATP_ASSERT_TRUE(converted.get_allocator().probe() == probe, "Adapter preserves selected allocator");
            FATP_ASSERT_EQ(probe->allocations, std::size_t{2}, "Adapter adds exactly one element allocation");
        }
        FATP_ASSERT_EQ(probe->deallocations, std::size_t{1}, "Converted scalar releases its storage");
        FATP_ASSERT_EQ(borrow[0], 59, "Conversion and result destruction preserve source borrow");
    }
    FATP_ASSERT_EQ(probe->attempts, std::size_t{2}, "Conversion needs no additional publication allocation");
    FATP_ASSERT_EQ(probe->deallocations, std::size_t{2}, "Both owned element allocations are released");
    return true;
}

} // namespace fat_p::testing::tensor_materialization

namespace fat_p::testing
{

bool test_TensorMaterialization()
{
    FATP_PRINT_HEADER(TENSOR RESULT MATERIALIZATION)
    TestRunner runner;
    FATP_RUN_TEST_NS(runner, tensor_materialization, assignment_only_scalar_transform);
    FATP_RUN_TEST_NS(runner, tensor_materialization, assignment_only_scalar_clone_gather_and_adapters);
    FATP_RUN_TEST_NS(runner, tensor_materialization, scalar_arithmetic_has_one_result_allocation);
    FATP_RUN_TEST_NS(runner, tensor_materialization, scalar_reductions_have_one_result_allocation);
    FATP_RUN_TEST_NS(runner, tensor_materialization, scalar_contractions_have_one_result_allocation);
    FATP_RUN_TEST_NS(runner, tensor_materialization, scalar_selection_and_adapters_have_one_result_allocation);
    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_TensorMaterialization() ? 0 : 1;
}
#endif
