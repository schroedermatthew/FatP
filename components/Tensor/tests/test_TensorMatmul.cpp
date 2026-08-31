/** @file test_TensorMatmul.cpp @brief Named Tensor linear-algebra contract tests. */

/*
FATP_META:
  meta_version: 1
  component: TensorMatmul
  file_role: test
  path: components/Tensor/tests/test_TensorMatmul.cpp
  namespace: fat_p::testing::tensor_matmul
  layer: Testing
  summary: "Named linear-algebra shape, dtype, layout, lifetime, overflow, and allocator tests."
  api_stability: in_work
  related:
    docs:
      - components/Tensor/docs/Design Note - Tensor Architecture Additions Plan.md
    headers:
      - include/fat_p/TensorMatmul.h
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
#include "ScopeGuard.h"
#include "TensorMatmul.h"

#include <array>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

namespace fat_p::testing::tensor_matmul
{

template <typename T, typename Result>
constexpr bool linearAlgebraTypes()
{
    using source = Tensor<T>;
    return std::same_as<typename decltype(dot(std::declval<source>(), std::declval<source>()))::value_type, Result> &&
           std::same_as<typename decltype(outer(std::declval<source>(), std::declval<source>()))::value_type, Result> &&
           std::same_as<typename decltype(matmul(std::declval<source>(),
                                                std::declval<source>()))::value_type, Result> &&
           std::same_as<typename decltype(trace(std::declval<source>()))::value_type, Result> &&
           std::same_as<typename decltype(diagonal(std::declval<source>()))::value_type, T>;
}

template <typename Left, typename Right>
concept HasVectorProducts = requires(const Left& left, const Right& right) {
    dot(left, right);
    outer(left, right);
};

template <typename Allocator>
concept HasIntegralAllocations = requires(const Tensor<int>& source, const Allocator& allocator) {
    dot(source, source, allocator);
    outer(source, source, allocator);
    matmul(source, source, allocator);
    trace(source, allocator);
};

static_assert(linearAlgebraTypes<bool, std::size_t>());
static_assert(linearAlgebraTypes<std::int8_t, std::int64_t>());
static_assert(linearAlgebraTypes<std::uint8_t, std::uint64_t>());
static_assert(linearAlgebraTypes<std::int16_t, std::int64_t>());
static_assert(linearAlgebraTypes<std::uint16_t, std::uint64_t>());
static_assert(linearAlgebraTypes<std::int32_t, std::int64_t>());
static_assert(linearAlgebraTypes<std::uint32_t, std::uint64_t>());
static_assert(linearAlgebraTypes<std::int64_t, std::int64_t>());
static_assert(linearAlgebraTypes<std::uint64_t, std::uint64_t>());
static_assert(linearAlgebraTypes<float, float>());
static_assert(linearAlgebraTypes<double, double>());
static_assert(linearAlgebraTypes<long double, long double>());
static_assert(!HasVectorProducts<Tensor<int>, Tensor<float>>);
static_assert(!HasVectorProducts<Tensor<std::string>, Tensor<std::string>>);
static_assert(HasIntegralAllocations<std::allocator<std::int64_t>>);
static_assert(!HasIntegralAllocations<std::allocator<int>>);

struct AllocationCounts
{
    std::size_t allocations = 0;
    std::size_t liveElements = 0;
    bool fail = false;
};

template <typename T>
class ResultAllocator
{
public:
    using value_type = T;

    explicit ResultAllocator(int identity = 0, AllocationCounts* counts = nullptr) noexcept
        : mIdentity(identity), mCounts(counts)
    {
    }

    template <typename U>
    ResultAllocator(const ResultAllocator<U>& other) noexcept
        : mIdentity(other.identity()), mCounts(other.counts())
    {
    }

    [[nodiscard]] T* allocate(std::size_t count)
    {
        if (mCounts && mCounts->fail)
        {
            throw std::bad_alloc();
        }
        auto* storage = std::allocator<T>{}.allocate(count);
        if (mCounts)
        {
            ++mCounts->allocations;
            mCounts->liveElements += count;
        }
        return storage;
    }

    void deallocate(T* storage, std::size_t count) noexcept
    {
        if (mCounts)
        {
            mCounts->liveElements -= count;
        }
        std::allocator<T>{}.deallocate(storage, count);
    }

    [[nodiscard]] ResultAllocator select_on_container_copy_construction() const noexcept
    {
        return ResultAllocator(mIdentity + 100, mCounts);
    }

    [[nodiscard]] int identity() const noexcept { return mIdentity; }
    [[nodiscard]] AllocationCounts* counts() const noexcept { return mCounts; }

    template <typename U>
    [[nodiscard]] bool operator==(const ResultAllocator<U>& other) const noexcept
    {
        return mIdentity == other.identity() && mCounts == other.counts();
    }

private:
    int mIdentity = 0;
    AllocationCounts* mCounts = nullptr;
};

FATP_TEST_CASE(vector_and_matrix_forms)
{
    Tensor<int> leftVector({3});
    Tensor<int> rightVector({3});
    std::iota(leftVector.begin(), leftVector.end(), 1);
    rightVector[0] = 4;
    rightVector[1] = 5;
    rightVector[2] = 6;
    const auto dot = matmul(leftVector, rightVector);
    static_assert(std::same_as<typename decltype(dot)::value_type, std::int64_t>);
    FATP_ASSERT_EQ(dot.rank(), std::size_t{0}, "Vector dot product should return a scalar");
    FATP_ASSERT_EQ(dot(), std::int64_t{32}, "Vector dot product should widen and accumulate");

    Tensor<int> matrix({2, 3});
    std::iota(matrix.begin(), matrix.end(), 1);
    const auto matrixVector = matmul(matrix, leftVector);
    FATP_ASSERT_TRUE(matrixVector.extents() == DynamicExtents({2}),
                     "Matrix-vector multiplication should remove the final vector axis");
    FATP_ASSERT_TRUE(std::vector<std::int64_t>(matrixVector.begin(), matrixVector.end()) ==
                         std::vector<std::int64_t>({14, 32}),
                     "Matrix-vector multiplication should use row-major logical coordinates");

    Tensor<int> rightMatrix({3, 2});
    std::iota(rightMatrix.begin(), rightMatrix.end(), 1);
    Tensor<int> shortVector({3});
    shortVector[0] = 1;
    shortVector[1] = 2;
    shortVector[2] = 3;
    const auto vectorMatrix = matmul(shortVector, rightMatrix);
    FATP_ASSERT_TRUE(std::vector<std::int64_t>(vectorMatrix.begin(), vectorMatrix.end()) ==
                         std::vector<std::int64_t>({22, 28}),
                     "Vector-matrix multiplication should remove the leading vector axis");
    return true;
}

FATP_TEST_CASE(contiguous_strided_and_batched)
{
    Tensor<int> left({2, 3});
    Tensor<int> right({3, 2});
    std::iota(left.begin(), left.end(), 1);
    std::iota(right.begin(), right.end(), 1);
    const auto product = matmul(left, right);
    FATP_ASSERT_TRUE(product.extents() == DynamicExtents({2, 2}), "Matrix product shape should be M by N");
    FATP_ASSERT_TRUE(std::vector<std::int64_t>(product.begin(), product.end()) ==
                         std::vector<std::int64_t>({22, 28, 49, 64}),
                     "Contiguous matrix multiplication should match the scalar oracle");

    Tensor<int> source({2, 3});
    std::iota(source.begin(), source.end(), 1);
    Tensor<int> identity({2, 2}, 0);
    identity(0, 0) = 1;
    identity(1, 1) = 1;
    const auto strided = matmul(source.transposeView(), identity);
    FATP_ASSERT_TRUE(strided.extents() == DynamicExtents({3, 2}),
                     "Strided matrix operands should preserve matrix dimensions");
    FATP_ASSERT_TRUE(std::vector<std::int64_t>(strided.begin(), strided.end()) ==
                         std::vector<std::int64_t>({1, 4, 2, 5, 3, 6}),
                     "The generic kernel should honor signed logical strides");

    Tensor<int> batches({2, 2, 3});
    for (std::size_t index = 0; index < 6; ++index)
    {
        batches[index] = 1;
        batches[index + 6] = 2;
    }
    Tensor<int> broadcastRight({1, 3, 2}, 1);
    const auto batched = matmul(batches, broadcastRight);
    FATP_ASSERT_TRUE(batched.extents() == DynamicExtents({2, 2, 2}),
                     "Batch dimensions should use trailing broadcast rules");
    FATP_ASSERT_TRUE(std::vector<std::int64_t>(batched.begin(), batched.end()) ==
                         std::vector<std::int64_t>({3, 3, 3, 3, 6, 6, 6, 6}),
                     "Broadcast batches should reuse singleton operand batches");

    Tensor<double> compact({2, 3});
    compact[0] = 1.0e16;
    compact[1] = 1.0;
    compact[2] = -1.0e16;
    compact[3] = 2.0;
    compact[4] = 3.0;
    compact[5] = 4.0;
    Tensor<double> padded({2, 4}, 0.0);
    for (std::size_t row = 0; row < 2; ++row)
    {
        for (std::size_t column = 0; column < 3; ++column)
        {
            padded(row, column) = compact(row, column);
        }
    }
    const auto paddedView = padded.sliceView({All, Slice{0, 3}});
    Tensor<double> doubleRight({3, 2}, 1.0);
    const auto blockedResult = matmul(compact, doubleRight);
    const auto genericResult = matmul(paddedView, doubleRight);
    FATP_ASSERT_TRUE(std::vector<double>(blockedResult.begin(), blockedResult.end()) ==
                         std::vector<double>(genericResult.begin(), genericResult.end()),
                     "Blocked and generic kernels should preserve the same serial accumulation order");
    return true;
}

FATP_TEST_CASE(empty_and_zero_inner_dimensions)
{
    Tensor<int> left({2, 0});
    Tensor<int> right({0, 3});
    const auto zeroInner = matmul(left, right);
    FATP_ASSERT_TRUE(zeroInner.extents() == DynamicExtents({2, 3}),
                     "A zero contraction dimension may still produce a nonempty output");
    FATP_ASSERT_TRUE(std::vector<std::int64_t>(zeroInner.begin(), zeroInner.end()) ==
                         std::vector<std::int64_t>({0, 0, 0, 0, 0, 0}),
                     "A zero-length dot product should use the additive identity");

    Tensor<int> emptyBatch({0, 2, 3});
    Tensor<int> singletonBatch({1, 3, 2}, 1);
    const auto empty = matmul(emptyBatch, singletonBatch);
    FATP_ASSERT_TRUE(empty.extents() == DynamicExtents({0, 2, 2}),
                     "Zero batch extents should survive singleton broadcasting");
    FATP_ASSERT_TRUE(empty.empty(), "A zero batch should not evaluate any elements");
    return true;
}

FATP_TEST_CASE(validation_overflow_and_allocator)
{
    Tensor<int> scalar({}, 2);
    Tensor<int> vector({1}, 3);
    FATP_ASSERT_THROWS(matmul(scalar, vector), std::invalid_argument,
                       "Rank-zero operands are not matrix-multiplication inputs");
    FATP_ASSERT_THROWS(matmul(Tensor<int>({2, 3}), Tensor<int>({4, 2})), std::invalid_argument,
                       "Inner dimensions must match");
    FATP_ASSERT_THROWS(matmul(Tensor<int>({2, 2, 3}), Tensor<int>({3, 3, 2})), std::invalid_argument,
                       "Batch dimensions must be broadcast-compatible");

    Tensor<std::int64_t> overflowLeft({1, 1}, std::numeric_limits<std::int64_t>::max());
    Tensor<std::int64_t> overflowRight({1, 1}, 2);
    FATP_ASSERT_THROWS(matmul(overflowLeft, overflowRight), std::overflow_error,
                       "Integral multiplication overflow should be reported");

    const auto allocated = matmul(vector, vector, std::allocator<std::int64_t>{});
    static_assert(std::same_as<typename decltype(allocated)::allocator_type, std::allocator<std::int64_t>>);
    FATP_ASSERT_EQ(allocated(), std::int64_t{9}, "Explicit result allocators should be supported");

    Tensor<int, std::allocator<int>> standardLeft(std::allocator_arg, std::allocator<int>{},
                                                   DynamicExtents({1}), 3);
    const auto inherited = matmul(standardLeft, vector);
    static_assert(std::same_as<typename decltype(inherited)::allocator_type,
                               std::allocator<std::int64_t>>);
    FATP_ASSERT_EQ(inherited(), std::int64_t{9},
                   "matmul should rebind the first owning operand's allocator");
    return true;
}

FATP_TEST_CASE(named_shapes_and_ownership)
{
    Tensor<int> left({3});
    Tensor<int> right({3});
    std::iota(left.begin(), left.end(), 1);
    std::iota(right.begin(), right.end(), 4);
    const auto scalar = dot(left, right);
    FATP_ASSERT_TRUE(scalar.rank() == 0 && scalar() == 32, "dot returns a rank-zero owner");
    const auto products = outer(left, right);
    FATP_ASSERT_TRUE(products.extents() == DynamicExtents({3, 3}), "Outer product appends both lengths");
    FATP_ASSERT_TRUE(std::vector<std::int64_t>(products.begin(), products.end()) ==
                         std::vector<std::int64_t>({4, 5, 6, 8, 10, 12, 12, 15, 18}),
                     "Outer product is not a contraction");
    Tensor<int> batch({2, 2, 3});
    std::iota(batch.begin(), batch.end(), 1);
    auto values = diagonal(batch);
    const auto totals = trace(batch);
    FATP_ASSERT_TRUE(values.extents() == DynamicExtents({2, 2}), "Diagonal retains batch axes");
    FATP_ASSERT_TRUE(std::vector<int>(values.begin(), values.end()) == std::vector<int>({1, 5, 7, 11}),
                     "Rectangular main diagonals stop at the shorter axis");
    FATP_ASSERT_TRUE(totals.extents() == DynamicExtents({2}) && totals[0] == 6 && totals[1] == 18,
                     "Trace reduces only the final diagonal axis");
    values[0] = 99;
    FATP_ASSERT_EQ(batch[0], 1, "Diagonal extraction must not alias its source");
    Tensor<std::string> text({2, 2}, std::string("value"));
    const auto copied = diagonal(text);
    FATP_ASSERT_TRUE(copied.size() == 2 && copied[1] == "value", "Diagonal extraction is not arithmetic-only");
    return true;
}

FATP_TEST_CASE(vector_layout_scalar_references)
{
    std::array<int, 64> storage{};
    std::iota(storage.begin(), storage.end(), -20);
    for (const std::ptrdiff_t leftStride : {-3, -1, 0, 1, 3})
    {
        for (const std::ptrdiff_t rightStride : {-3, -1, 0, 1, 3})
        {
            for (std::size_t rows = 0; rows <= 4; ++rows)
            {
                const auto left = TensorView<const int>::borrow(storage.data(),
                    TensorLayout(storage.size(), 24, DynamicExtents{rows}, {leftStride}));
                for (std::size_t columns = 0; columns <= 4; ++columns)
                {
                    const auto right = TensorView<const int>::borrow(storage.data(),
                        TensorLayout(storage.size(), 32, DynamicExtents{columns}, {rightStride}));
                    const auto products = outer(left, right);
                    FATP_ASSERT_TRUE(products.extents() == DynamicExtents({rows, columns}),
                                     "Outer shape must retain zero and singleton axes");
                    std::int64_t expectedDot = 0;
                    for (std::size_t row = 0; row < rows; ++row)
                    {
                        const auto a = storage[static_cast<std::size_t>(
                            24 + static_cast<std::ptrdiff_t>(row) * leftStride)];
                        for (std::size_t column = 0; column < columns; ++column)
                        {
                            const auto b = storage[static_cast<std::size_t>(
                                32 + static_cast<std::ptrdiff_t>(column) * rightStride)];
                            const auto expected = static_cast<std::int64_t>(a) * b;
                            FATP_ASSERT_EQ(products(row, column), expected, "Signed-stride outer scalar oracle");
                            if (row == column)
                            {
                                expectedDot += expected;
                            }
                        }
                    }
                    if (rows == columns)
                    {
                        const auto total = dot(left, right);
                        FATP_ASSERT_EQ(total(), expectedDot, "Signed-stride dot scalar oracle");
                    }
                }
            }
        }
    }
    return true;
}

FATP_TEST_CASE(diagonal_layout_scalar_references)
{
    std::array<int, 128> storage{};
    std::iota(storage.begin(), storage.end(), -32);
    for (const std::ptrdiff_t batchStride : {-16, 0, 16})
    {
        for (const std::ptrdiff_t rowStride : {-5, 0, 1, 5})
        {
            for (const std::ptrdiff_t columnStride : {-3, 0, 1, 3})
            {
                for (std::size_t rows = 0; rows <= 3; ++rows)
                {
                    for (std::size_t columns = 0; columns <= 3; ++columns)
                    {
                        const auto source = TensorView<const int>::borrow(storage.data(),
                            TensorLayout(storage.size(), 64, DynamicExtents{2, rows, columns},
                                         {batchStride, rowStride, columnStride}));
                        const auto values = diagonal(source);
                        const auto totals = trace(source);
                        const auto length = std::min(rows, columns);
                        FATP_ASSERT_TRUE(values.extents() == DynamicExtents({2, length}),
                                         "Batched diagonal shape oracle");
                        FATP_ASSERT_TRUE(totals.extents() == DynamicExtents({2}), "Batched trace shape oracle");
                        for (std::size_t batch = 0; batch < 2; ++batch)
                        {
                            std::int64_t expectedTotal = 0;
                            for (std::size_t index = 0; index < length; ++index)
                            {
                                const auto offset = 64 + static_cast<std::ptrdiff_t>(batch) * batchStride +
                                    static_cast<std::ptrdiff_t>(index) * rowStride +
                                    static_cast<std::ptrdiff_t>(index) * columnStride;
                                const auto expected = storage[static_cast<std::size_t>(offset)];
                                FATP_ASSERT_EQ(values(batch, index), expected, "Independent diagonal coordinates");
                                expectedTotal += expected;
                            }
                            FATP_ASSERT_EQ(totals[batch], expectedTotal, "Independent trace fold");
                        }
                    }
                }
            }
        }
    }
    return true;
}

FATP_TEST_CASE(matmul_layout_scalar_references)
{
    std::array<int, 128> storage{};
    std::iota(storage.begin(), storage.end(), -32);
    const std::array<TensorStrides, 6> layouts{{{3, 1}, {1, 2}, {8, 2}, {-3, -1}, {0, 1}, {1, -1}}};
    for (const auto& leftStrides : layouts)
    {
        for (const auto& rightStrides : layouts)
        {
            const auto left = TensorView<const int>::borrow(storage.data(),
                TensorLayout(storage.size(), 32, DynamicExtents{2, 3}, leftStrides));
            const auto right = TensorView<const int>::borrow(storage.data(),
                TensorLayout(storage.size(), 64, DynamicExtents{3, 2}, rightStrides));
            const auto output = matmul(left, right);
            for (std::size_t row = 0; row < 2; ++row)
            {
                for (std::size_t column = 0; column < 2; ++column)
                {
                    std::int64_t expected = 0;
                    for (std::size_t inner = 0; inner < 3; ++inner)
                    {
                        const auto leftOffset = 32 + static_cast<std::ptrdiff_t>(row) * leftStrides[0] +
                            static_cast<std::ptrdiff_t>(inner) * leftStrides[1];
                        const auto rightOffset = 64 + static_cast<std::ptrdiff_t>(inner) * rightStrides[0] +
                            static_cast<std::ptrdiff_t>(column) * rightStrides[1];
                        expected += static_cast<std::int64_t>(storage[static_cast<std::size_t>(leftOffset)]) *
                            storage[static_cast<std::size_t>(rightOffset)];
                    }
                    FATP_ASSERT_EQ(output(row, column), expected, "Independent matrix coordinate oracle");
                }
            }
        }
    }
    return true;
}

FATP_TEST_CASE(empty_singleton_and_extreme_metadata)
{
    Tensor<int> emptyVector({0});
    const auto emptyDot = dot(emptyVector, emptyVector);
    FATP_ASSERT_TRUE(emptyDot.rank() == 0 && emptyDot() == 0, "Empty dot still allocates a scalar zero");
    Tensor<int> emptyMatrices({2, 0, 3});
    const auto zeros = trace(emptyMatrices);
    FATP_ASSERT_TRUE(zeros.extents() == DynamicExtents({2}) && zeros[0] == 0 && zeros[1] == 0,
                     "Empty diagonal domains produce one zero per batch");
    Tensor<int> emptyBatch({2, 0, 3, 4});
    const auto noDiagonals = diagonal(emptyBatch);
    const auto noTraces = trace(emptyBatch);
    FATP_ASSERT_TRUE(noDiagonals.extents() == DynamicExtents({2, 0, 3}) && noDiagonals.empty(),
                     "Zero batch preserves the remaining diagonal shape");
    FATP_ASSERT_TRUE(noTraces.extents() == DynamicExtents({2, 0}) && noTraces.empty(),
                     "Zero batch has no trace domains");

    int value = 7;
    const auto maximum = std::numeric_limits<std::ptrdiff_t>::max();
    const auto minimum = std::numeric_limits<std::ptrdiff_t>::min();
    for (const auto stride : {maximum, minimum})
    {
        const auto singleton = TensorView<const int>::borrow(&value,
            TensorLayout(1, 0, DynamicExtents{1, 1}, {stride, stride}));
        const auto copied = diagonal(singleton);
        const auto total = trace(singleton);
        FATP_ASSERT_TRUE(copied[0] == 7 && total() == 7, "Unused diagonal stride sum must not overflow");
        const auto noBatch = TensorView<const int>::borrow(nullptr,
            TensorLayout(0, 0, DynamicExtents{0, 2, 2}, {0, stride, stride}));
        const auto copiedEmpty = diagonal(noBatch);
        const auto totalEmpty = trace(noBatch);
        FATP_ASSERT_TRUE(copiedEmpty.empty() && totalEmpty.empty(), "Empty mapping never sums unused strides");
        const auto noInnerLeft = TensorView<const int>::borrow(nullptr,
            TensorLayout(0, 0, DynamicExtents{3, 0}, {stride, stride}));
        const auto noInnerRight = TensorView<const int>::borrow(nullptr,
            TensorLayout(0, 0, DynamicExtents{0, 3}, {stride, stride}));
        const auto zeroProducts = matmul(noInnerLeft, noInnerRight);
        FATP_ASSERT_TRUE(zeroProducts.extents() == DynamicExtents({3, 3}),
                         "Zero contraction still has a nonempty matrix output");
        for (const auto entry : zeroProducts)
        {
            FATP_ASSERT_EQ(entry, std::int64_t{0}, "Zero contraction must not evaluate unreachable row/column strides");
        }
    }
    const auto singleVector = TensorView<const int>::borrow(&value,
        TensorLayout(1, 0, DynamicExtents{1}, {minimum}));
    const auto product = outer(singleVector, singleVector);
    FATP_ASSERT_EQ(product(0, 0), std::int64_t{49}, "Singleton outer ignores unreachable vector strides");
    return true;
}

FATP_TEST_CASE(numeric_policy_and_failures)
{
    Tensor<std::int32_t> wide({2}, 50000);
    const auto wideDot = dot(wide, wide);
    const auto wideOuter = outer(wide, wide);
    FATP_ASSERT_EQ(wideDot(), std::int64_t{5000000000}, "Widen before multiplying, not after overflow");
    FATP_ASSERT_EQ(wideOuter(0, 0), std::int64_t{2500000000}, "Outer follows linear-algebra widening");
    Tensor<bool> flags({2}, true);
    const auto count = dot(flags, flags);
    const auto pairs = outer(flags, flags);
    Tensor<bool> mask({2, 2}, true);
    const auto trueTrace = trace(mask);
    FATP_ASSERT_TRUE(count() == 2 && pairs(0, 0) == 1 && trueTrace() == 2, "Boolean contractions count values");

    Tensor<double> negativeZero({1}, -0.0);
    Tensor<double> positive({1}, 2.0);
    const auto signedProduct = outer(negativeZero, positive);
    FATP_ASSERT_TRUE(std::signbit(signedProduct[0]), "Outer uses direct multiplication, not a zero-seeded fold");
    const auto zeroDot = dot(negativeZero, positive);
    Tensor<double> zeroMatrix({1, 1}, -0.0);
    const auto zeroTrace = trace(zeroMatrix);
    FATP_ASSERT_TRUE(!std::signbit(zeroDot()) && !std::signbit(zeroTrace()),
                     "Dot and trace start at positive zero under the default floating environment");
    Tensor<double> special({1}, std::numeric_limits<double>::infinity());
    Tensor<double> zero({1}, 0.0);
    const auto invalidProduct = outer(special, zero);
    FATP_ASSERT_TRUE(std::isnan(invalidProduct[0]), "Floating invalid arithmetic follows the underlying type");
    Tensor<double> cancellation({3, 3}, 0.0);
    cancellation(0, 0) = 1e16;
    cancellation(1, 1) = 1.0;
    cancellation(2, 2) = -1e16;
    const auto serialTrace = trace(cancellation);
    FATP_ASSERT_EQ(serialTrace(), 0.0, "Trace retains increasing diagonal-index accumulation order");

    AllocationCounts counts;
    ResultAllocator<std::int64_t> allocator(9, &counts);
    Tensor<std::int64_t> left({2}, 3);
    left[1] = std::numeric_limits<std::int64_t>::max();
    Tensor<std::int64_t> right({2}, 2);
    const auto original = std::vector<std::int64_t>(left.begin(), left.end());
    FATP_ASSERT_THROWS(dot(left, right, allocator), std::overflow_error, "Late product overflow in dot");
    FATP_ASSERT_THROWS(outer(left, right, allocator), std::overflow_error, "Late product overflow in outer");
    FATP_ASSERT_EQ(counts.liveElements, std::size_t{0}, "Both failed results must release their element buffers");
    right.fill(1);
    FATP_ASSERT_THROWS(dot(left, right, allocator), std::overflow_error, "Intermediate sum overflow in dot");
    Tensor<std::int64_t> matrix({2, 2}, 0);
    matrix(0, 0) = 3;
    matrix(1, 1) = std::numeric_limits<std::int64_t>::max();
    FATP_ASSERT_THROWS(trace(matrix, allocator), std::overflow_error, "Intermediate sum overflow in trace");
    FATP_ASSERT_TRUE(std::vector<std::int64_t>(left.begin(), left.end()) == original && matrix(0, 0) == 3 &&
                         matrix(1, 1) == std::numeric_limits<std::int64_t>::max(),
                     "Late arithmetic failures leave every source unchanged");
    FATP_ASSERT_EQ(counts.liveElements, std::size_t{0}, "Failed accumulation results are reclaimed");
    Tensor<std::uint64_t> unsignedMax({1}, std::numeric_limits<std::uint64_t>::max());
    Tensor<std::uint64_t> two({1}, 2);
    FATP_ASSERT_THROWS(outer(unsignedMax, two), std::overflow_error, "Unsigned product must not wrap");
    return true;
}

FATP_TEST_CASE(allocator_selection_and_validation)
{
    AllocationCounts counts;
    Tensor<int, ResultAllocator<int>> left(std::allocator_arg, ResultAllocator<int>(1), DynamicExtents{2}, 3);
    Tensor<int, ResultAllocator<int>> right(std::allocator_arg, ResultAllocator<int>(2), DynamicExtents{2}, 4);
    const auto a = dot(left, right);
    const auto b = outer(left.asConstView(), right);
    const auto c = outer(left, right);
    const auto views = dot(left.asSharedView(), right.asConstView());
    static_assert(std::same_as<typename decltype(views)::allocator_type, TensorAllocator<std::int64_t>>);
    FATP_ASSERT_TRUE(a.get_allocator().identity() == 101 && b.get_allocator().identity() == 102 &&
                         c.get_allocator().identity() == 101, "Select original first owner, with SOCCC exactly once");
    Tensor<int, ResultAllocator<int>> matrix(std::allocator_arg, ResultAllocator<int>(3), DynamicExtents{2, 3}, 5);
    const auto d = diagonal(matrix);
    const auto t = trace(matrix);
    FATP_ASSERT_TRUE(d.get_allocator().identity() == 103 && t.get_allocator().identity() == 103,
                     "Mapped intermediates must not lose the source allocator");
    {
        const auto explicitDot = dot(left, right, ResultAllocator<std::int64_t>(7, &counts));
        const auto explicitOuter = outer(left, right, ResultAllocator<std::int64_t>(7, &counts));
        const auto explicitDiagonal = diagonal(matrix, ResultAllocator<int>(7, &counts));
        const auto explicitTrace = trace(matrix, ResultAllocator<std::int64_t>(7, &counts));
        FATP_ASSERT_TRUE(explicitDot.get_allocator().identity() == 7 &&
                             explicitOuter.get_allocator().identity() == 7 &&
                             explicitDiagonal.get_allocator().identity() == 7 &&
                             explicitTrace.get_allocator().identity() == 7,
                         "Explicit result allocators are used unchanged");
        FATP_ASSERT_EQ(counts.allocations, std::size_t{4}, "One element allocation per nonempty result, no staging");
    }
    FATP_ASSERT_EQ(counts.liveElements, std::size_t{0}, "All allocated result elements are released");
    counts.fail = true;
    const ResultAllocator<std::int64_t> fail(8, &counts);
    FATP_ASSERT_THROWS(dot(matrix, left, fail), std::invalid_argument, "dot rank is checked before allocation");
    FATP_ASSERT_THROWS(outer(left, matrix, fail), std::invalid_argument, "outer rank is checked before allocation");
    FATP_ASSERT_THROWS(dot(left, Tensor<int>({3}), fail), std::invalid_argument, "dot length is validated first");
    FATP_ASSERT_THROWS(trace(left, fail), std::invalid_argument, "trace rank is validated first");
    FATP_ASSERT_THROWS(diagonal(left, ResultAllocator<int>(8, &counts)), std::invalid_argument,
                       "diagonal rank is validated first");
    FATP_ASSERT_THROWS(dot(left, right, fail), std::bad_alloc, "dot propagates element allocation failure");
    FATP_ASSERT_THROWS(outer(left, right, fail), std::bad_alloc, "outer propagates element allocation failure");
    FATP_ASSERT_THROWS(trace(matrix, fail), std::bad_alloc, "trace propagates element allocation failure");
    FATP_ASSERT_THROWS(diagonal(matrix, ResultAllocator<int>(8, &counts)), std::bad_alloc,
                       "diagonal propagates element allocation failure");
    Tensor<int> empty({0});
    const auto emptyResult = outer(empty, left, fail);
    FATP_ASSERT_TRUE(emptyResult.empty(), "An empty result allocates no element buffer");
    FATP_ASSERT_TRUE(left[0] == 3 && right[0] == 4 && matrix[0] == 5 && counts.liveElements == 0,
                     "Allocation failures leave sources and live allocation counts unchanged");
    return true;
}

FATP_TEST_CASE(shared_and_borrowed_lifetimes)
{
    const auto shared = [] {
        Tensor<int> owner({2, 2}, 3);
        return owner.asSharedView();
    }();
    const auto values = diagonal(shared);
    const auto total = trace(shared);
    FATP_ASSERT_TRUE(values[0] == 3 && total() == 6, "Shared source storage outlives its original owner");
    const auto temporary = diagonal(Tensor<int>({2, 3}, 7));
    FATP_ASSERT_EQ(temporary[1], 7, "Synchronous extraction from a temporary owner is safe");
#ifndef NDEBUG
    const auto expiredMatrix = [] {
        Tensor<int> owner({0, 2, 2});
        return owner.asConstView();
    }();
    const auto expiredVector = [] {
        Tensor<int> owner({0});
        return owner.asConstView();
    }();
    Tensor<int> empty({0});
    FATP_ASSERT_THROWS(diagonal(expiredMatrix), std::runtime_error, "Validate even an empty expired diagonal source");
    FATP_ASSERT_THROWS(trace(expiredMatrix), std::runtime_error, "Validate even an empty expired trace source");
    FATP_ASSERT_THROWS(dot(expiredVector, empty), std::runtime_error, "Validate expired left dot source");
    FATP_ASSERT_THROWS(outer(empty, expiredVector), std::runtime_error, "Validate expired right outer source");
#endif
    return true;
}

FATP_TEST_CASE(batched_vector_and_matrix_shape_table)
{
    struct ShapeCase
    {
        DynamicExtents left;
        DynamicExtents right;
        DynamicExtents output;
        std::int64_t expected;
    };
    const std::array<ShapeCase, 8> cases{{
        {{3}, {2, 3, 4}, {2, 4}, 18},
        {{2, 4, 3}, {3}, {2, 4}, 18},
        {{2, 1, 4, 3}, {1, 3, 3, 5}, {2, 3, 4, 5}, 18},
        {{0}, {2, 0, 4}, {2, 4}, 0},
        {{2, 4, 0}, {0}, {2, 4}, 0},
        {{3}, {0, 3, 4}, {0, 4}, 0},
        {{0, 4, 3}, {3}, {0, 4}, 0},
        {{2, 1, 4, 0}, {1, 3, 0, 5}, {2, 3, 4, 5}, 0},
    }};
    for (const auto& item : cases)
    {
        Tensor<int> left(item.left, 2);
        Tensor<int> right(item.right, 3);
        const auto result = matmul(left, right);
        FATP_ASSERT_TRUE(result.extents() == item.output, "Vector promotion and batch broadcasting shape table");
        for (const auto value : result)
        {
            FATP_ASSERT_EQ(value, item.expected, "Each contraction has the table's expected value");
        }
    }
    return true;
}

struct ThrowingElement
{
    int value = 0;
    inline static int assignmentsUntilThrow = -1;

    ThrowingElement& operator=(const ThrowingElement& other)
    {
        if (assignmentsUntilThrow == 0)
        {
            throw std::runtime_error("intentional diagonal copy failure");
        }
        if (assignmentsUntilThrow > 0)
        {
            --assignmentsUntilThrow;
        }
        value = other.value;
        return *this;
    }
};

FATP_TEST_CASE(diagonal_copy_failure_cleanup)
{
    Tensor<ThrowingElement> source({2, 2});
    source(0, 0).value = 7;
    source(1, 1).value = 9;
    AllocationCounts counts;
    ResultAllocator<ThrowingElement> allocator(8, &counts);
    ThrowingElement::assignmentsUntilThrow = 1;
    const auto reset = makeScopeGuard([] { ThrowingElement::assignmentsUntilThrow = -1; });
    FATP_ASSERT_THROWS(diagonal(source, allocator), std::runtime_error, "Second diagonal copy throws after progress");
    FATP_ASSERT_TRUE(source(0, 0).value == 7 && source(1, 1).value == 9,
                     "Throwing element copies do not modify their source");
    FATP_ASSERT_EQ(counts.liveElements, std::size_t{0}, "Unpublished diagonal owner is reclaimed after copy failure");
    return true;
}

template <typename T>
bool contiguousDotMatchesStrided()
{
    for (const std::size_t length : std::array<std::size_t, 6>{0, 1, 31, 32, 33, 65})
    {
        for (int scenario = 0; scenario < 5; ++scenario)
        {
            const T sentinel = std::numeric_limits<T>::quiet_NaN();
            std::vector<T> compact(length + 15, sentinel);
            std::vector<T> padded(length * 2 + 15, sentinel);
            std::vector<T> rightStorage(length + 15, sentinel);
            std::vector<T> rightPadded(length * 2 + 15, sentinel);
            const T large = std::ldexp(T{1}, std::numeric_limits<T>::digits);
            for (std::size_t i = 0; i < length; ++i)
            {
                T value = i % 3 == 0 ? large : i % 3 == 1 ? T{1} : -large;
                if (scenario == 1 && i == length - 1) { value = std::numeric_limits<T>::quiet_NaN(); }
                if (scenario == 2 && i == length - 1) { value = std::numeric_limits<T>::infinity(); }
                if (scenario == 3) { value = -T{0}; }
                if (scenario == 4) { value = (static_cast<T>((i * 17) % 23) - T{11}) / T{8}; }
                const T rightValue = scenario == 4 ? static_cast<T>(i + 1) / T{8} : T{1};
                compact[7 + i] = value;
                padded[7 + 2 * i] = value;
                rightStorage[7 + i] = rightValue;
                rightPadded[7 + 2 * i] = rightValue;
            }
            const auto contiguous = TensorView<const T>::borrow(compact.data(),
                TensorLayout(compact.size(), 7, DynamicExtents{length}, {1}));
            const auto strided = TensorView<const T>::borrow(padded.data(),
                TensorLayout(padded.size(), 7, DynamicExtents{length}, {2}));
            const auto right = TensorView<const T>::borrow(rightStorage.data(),
                TensorLayout(rightStorage.size(), 7, DynamicExtents{length}, {1}));
            const auto rightStrided = TensorView<const T>::borrow(rightPadded.data(),
                TensorLayout(rightPadded.size(), 7, DynamicExtents{length}, {2}));
            const auto actual = dot(contiguous, right);
            const auto generic = dot(strided, right);
            const auto genericRight = dot(contiguous, rightStrided);
            const auto genericBoth = dot(strided, rightStrided);
            const auto viaMatmul = matmul(contiguous, right);
            FATP_ASSERT_TRUE(actual.rank() == 0 && actual.size() == 1, "Vector dispatch preserves rank-zero output");
            if (std::isnan(generic()))
            {
                FATP_ASSERT_TRUE(std::isnan(actual()) && std::isnan(viaMatmul()) &&
                                     std::isnan(genericRight()) && std::isnan(genericBoth()),
                                 "All operand-layout pairings propagate NaN");
            }
            else
            {
                FATP_ASSERT_TRUE(actual() == generic() && viaMatmul() == generic() &&
                                     genericRight() == generic() && genericBoth() == generic() &&
                                     std::signbit(actual()) == std::signbit(generic()) &&
                                     std::signbit(genericRight()) == std::signbit(generic()) &&
                                     std::signbit(genericBoth()) == std::signbit(generic()),
                                 "Contiguous and generic paths retain serial cancellation, infinity, and zero signs");
            }
        }
    }
    return true;
}

FATP_TEST_CASE(contiguous_dot_dispatch_equivalence)
{
    FATP_ASSERT_TRUE(contiguousDotMatchesStrided<float>(), "Float dispatch equivalence");
    FATP_ASSERT_TRUE(contiguousDotMatchesStrided<double>(), "Double dispatch equivalence");
    FATP_ASSERT_TRUE(contiguousDotMatchesStrided<long double>(), "Long-double dispatch equivalence");
    const auto empty = TensorView<const int>::borrow(nullptr, TensorLayout(0, 0, DynamicExtents{0}, {1}));
    const auto emptyResult = dot(empty, empty);
    FATP_ASSERT_EQ(emptyResult(), std::int64_t{0}, "Empty contiguous dot never forms a storage pointer");
    const int value = 7;
    const auto singleton = TensorView<const int>::borrow(&value,
        TensorLayout(1, 0, DynamicExtents{1}, {std::numeric_limits<std::ptrdiff_t>::min()}));
    const auto singletonResult = dot(singleton, singleton);
    FATP_ASSERT_EQ(singletonResult(), std::int64_t{49}, "Singleton ignores its unreachable extreme stride");

    Tensor<std::int64_t> left({33}, 0);
    Tensor<std::int64_t> right({33}, 1);
    right[32] = 2;
    left[0] = 3;
    left[32] = std::numeric_limits<std::int64_t>::max();
    AllocationCounts counts;
    const ResultAllocator<std::int64_t> allocator(19, &counts);
    FATP_ASSERT_THROWS(dot(left, right, allocator), std::overflow_error, "Product overflow beyond the first block");
    right.fill(1);
    FATP_ASSERT_THROWS(dot(left, right, allocator), std::overflow_error, "Sum overflow beyond the first block");
    FATP_ASSERT_TRUE(counts.allocations == 2 && counts.liveElements == 0,
                     "Both checked failures reclaim the explicit-allocator result");
    FATP_ASSERT_TRUE(left[0] == 3 && left[32] == std::numeric_limits<std::int64_t>::max(),
                     "Failed contiguous contractions leave inputs unchanged");
    return true;
}

FATP_TEST_CASE(retired_pattern_replacements)
{
    Tensor<float> matrix({2, 3});
    std::iota(matrix.begin(), matrix.end(), 1.0f);
    const auto transposed = clone(matrix.transposeView());
    FATP_ASSERT_TRUE(transposed.extents() == DynamicExtents({3, 2}) && transposed(2, 1) == 6.0f,
                     "Transpose materialization replaces the old pattern");
    const auto rows = sum(matrix, {1});
    const auto columns = sum(matrix, {0});
    const auto total = sum(matrix);
    FATP_ASSERT_TRUE(rows[0] == 6.0f && rows[1] == 15.0f && columns[2] == 9.0f && total() == 21.0f,
                     "Named reductions cover row, column, and all-element patterns");
    const auto products = multiply(matrix, matrix);
    const auto frobenius = sum(products);
    FATP_ASSERT_TRUE(products(1, 2) == 36.0f && frobenius() == 91.0f,
                     "Composed multiplication and sum cover elementwise and Frobenius patterns");
    Tensor<int> integers({2, 2}, 50000);
    const auto widened = cast<std::int64_t>(integers);
    const auto integerFrobenius = sum(multiply(widened, widened));
    FATP_ASSERT_EQ(integerFrobenius(), std::int64_t{10000000000}, "Explicit cast widens Frobenius products first");
    return true;
}

} // namespace fat_p::testing::tensor_matmul

namespace fat_p::testing
{

bool test_TensorMatmul()
{
    FATP_PRINT_HEADER(TENSOR MATMUL)
    TestRunner runner;
    FATP_RUN_TEST_NS(runner, tensor_matmul, vector_and_matrix_forms);
    FATP_RUN_TEST_NS(runner, tensor_matmul, contiguous_strided_and_batched);
    FATP_RUN_TEST_NS(runner, tensor_matmul, empty_and_zero_inner_dimensions);
    FATP_RUN_TEST_NS(runner, tensor_matmul, validation_overflow_and_allocator);
    FATP_RUN_TEST_NS(runner, tensor_matmul, named_shapes_and_ownership);
    FATP_RUN_TEST_NS(runner, tensor_matmul, vector_layout_scalar_references);
    FATP_RUN_TEST_NS(runner, tensor_matmul, diagonal_layout_scalar_references);
    FATP_RUN_TEST_NS(runner, tensor_matmul, matmul_layout_scalar_references);
    FATP_RUN_TEST_NS(runner, tensor_matmul, empty_singleton_and_extreme_metadata);
    FATP_RUN_TEST_NS(runner, tensor_matmul, numeric_policy_and_failures);
    FATP_RUN_TEST_NS(runner, tensor_matmul, allocator_selection_and_validation);
    FATP_RUN_TEST_NS(runner, tensor_matmul, shared_and_borrowed_lifetimes);
    FATP_RUN_TEST_NS(runner, tensor_matmul, batched_vector_and_matrix_shape_table);
    FATP_RUN_TEST_NS(runner, tensor_matmul, diagonal_copy_failure_cleanup);
    FATP_RUN_TEST_NS(runner, tensor_matmul, contiguous_dot_dispatch_equivalence);
    FATP_RUN_TEST_NS(runner, tensor_matmul, retired_pattern_replacements);
    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
#ifdef _MSC_VER
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _set_error_mode(_OUT_TO_STDERR);
#ifdef _DEBUG
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
#endif
#endif
    return fat_p::testing::test_TensorMatmul() ? 0 : 1;
}
#endif
