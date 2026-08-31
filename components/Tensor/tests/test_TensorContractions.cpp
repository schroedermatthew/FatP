/** @file test_TensorContractions.cpp @brief Explicit-axis contraction contract and scalar-oracle tests. */

/*
FATP_META:
  meta_version: 1
  component: TensorContractions
  file_role: test
  path: components/Tensor/tests/test_TensorContractions.cpp
  namespace: fat_p::testing::tensor_contractions
  layer: Testing
  summary: "Contraction axes, layout, numeric, ownership, and execution tests."
  api_stability: in_work
  related:
    docs:
      - components/Tensor/docs/Design Note - Tensor Architecture Additions Plan.md
    headers:
      - include/fat_p/TensorContractions.h
      - include/fat_p/TensorExecution.h
    tests:
      - components/Tensor/tests/test_TensorContractions.cpp
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
#include "TensorContractions.h"
#include "TensorExecution.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory>
#include <memory_resource>
#include <numeric>
#include <random>
#include <stop_token>
#include <string>
#include <vector>

#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

namespace fat_p::testing::tensor_contractions
{

template <typename A, typename B, typename Allocator>
concept HasTensorDot = requires(const A& a, const B& b, const Allocator& allocator) {
    tensorDot(a, b, {}, {});
    tensorDot(a, b, {}, {}, allocator);
    tensorDot(a, b, {}, {}, TensorExecutionContext{}, allocator);
};

static_assert(HasTensorDot<Tensor<int>, Tensor<int>, std::allocator<std::int64_t>>);
static_assert(!HasTensorDot<Tensor<int>, Tensor<int>, std::allocator<int>>);
static_assert(!HasTensorDot<Tensor<int>, Tensor<double>, std::allocator<double>>);
static_assert(!HasTensorDot<Tensor<std::string>, Tensor<std::string>, std::allocator<std::string>>);

template <typename T, typename Result>
constexpr bool resultType()
{
    return std::same_as<
        typename decltype(tensorDot(std::declval<Tensor<T>>(), std::declval<Tensor<T>>(), {}, {}))::value_type,
        Result>;
}
static_assert(resultType<bool, std::size_t>());
static_assert(resultType<std::int8_t, std::int64_t>());
static_assert(resultType<std::uint8_t, std::uint64_t>());
static_assert(resultType<std::int16_t, std::int64_t>());
static_assert(resultType<std::uint16_t, std::uint64_t>());
static_assert(resultType<std::int32_t, std::int64_t>());
static_assert(resultType<std::uint32_t, std::uint64_t>());
static_assert(resultType<std::int64_t, std::int64_t>());
static_assert(resultType<std::uint64_t, std::uint64_t>());
static_assert(resultType<float, float>());
static_assert(resultType<double, double>());
static_assert(resultType<long double, long double>());

template <ReadableTensor Source>
auto readCoordinate(const Source& source, const std::vector<std::size_t>& coordinates)
{
    std::size_t linear = 0;
    for (std::size_t axis = 0; axis < coordinates.size(); ++axis)
    {
        linear = linear * source.extents()[axis] + coordinates[axis];
    }
    return source[linear];
}

// Independent recursive Cartesian oracle: no contraction plan, offset decoder,
// normalization, or arithmetic helpers from the implementation. Inputs are small.
template <ReadableTensor Left, ReadableTensor Right>
bool checkReference(const Left& a,
                    const Right& b,
                    const std::vector<TensorAxis>& leftAxes,
                    const std::vector<TensorAxis>& rightAxes)
{
    using Result = TensorMatmulType<typename Left::value_type>;
    const auto actual = tensorDot(a, b, leftAxes, rightAxes);
    std::vector<std::pair<bool, std::size_t>> freeAxes;
    std::vector<std::size_t> expectedExtents;
    for (std::size_t side = 0; side < 2; ++side)
    {
        const auto& axes = side == 0 ? leftAxes : rightAxes;
        const auto& extents = side == 0 ? a.extents() : b.extents();
        for (std::size_t axis = 0; axis < extents.rank(); ++axis)
        {
            if (std::find(axes.begin(), axes.end(), static_cast<TensorAxis>(axis)) == axes.end())
            {
                freeAxes.emplace_back(side == 0, axis);
                expectedExtents.push_back(extents[axis]);
            }
        }
    }
    FATP_ASSERT_TRUE(actual.extents().values() == expectedExtents, "Oracle free-axis ordering");
    std::vector<std::size_t> ac(a.rank(), 0);
    std::vector<std::size_t> bc(b.rank(), 0);
    std::size_t output = 0;
    bool equal = true;
    const auto visitOutput = [&](auto&& self, std::size_t depth) -> void {
        if (depth != freeAxes.size())
        {
            const auto [isLeft, axis] = freeAxes[depth];
            auto& coordinate = isLeft ? ac[axis] : bc[axis];
            const auto extent = isLeft ? a.extents()[axis] : b.extents()[axis];
            for (coordinate = 0; coordinate < extent; ++coordinate)
            {
                self(self, depth + 1);
            }
            return;
        }
        Result total{0};
        const auto visitInner = [&](auto&& innerSelf, std::size_t pair) -> void {
            if (pair == leftAxes.size())
            {
                total = static_cast<Result>(total + static_cast<Result>(readCoordinate(a, ac)) *
                                                        static_cast<Result>(readCoordinate(b, bc)));
                return;
            }
            const auto aa = static_cast<std::size_t>(leftAxes[pair]);
            const auto ba = static_cast<std::size_t>(rightAxes[pair]);
            for (std::size_t index = 0; index < a.extents()[aa]; ++index)
            {
                ac[aa] = index;
                bc[ba] = index;
                innerSelf(innerSelf, pair + 1);
            }
        };
        visitInner(visitInner, 0);
        equal = equal && actual[output++] == total;
    };
    visitOutput(visitOutput, 0);
    FATP_ASSERT_TRUE(equal && output == actual.size(), "Every result matches the recursive scalar oracle");
    return true;
}

FATP_TEST_CASE(shapes_and_axis_order)
{
    Tensor<int> a({2, 3, 4});
    Tensor<int> b({4, 5, 3});
    std::iota(a.begin(), a.end(), 1);
    std::iota(b.begin(), b.end(), -10);
    FATP_ASSERT_TRUE(checkReference(a, b, {2, 1}, {0, 2}), "Permuted pair order");
    FATP_ASSERT_TRUE(checkReference(a, b, {}, {}), "No axes means a generalized outer product");
    const auto positive = tensorDot(a, b, {2, 1}, {0, 2});
    const auto negative = tensorDot(a, b, {-1, -2}, {-3, -1});
    FATP_ASSERT_TRUE(std::equal(positive.begin(), positive.end(), negative.begin()), "Negative axes normalize");
    FATP_ASSERT_TRUE(positive.extents() == DynamicExtents({2, 5}), "Left free axes precede right free axes");
    Tensor<int> scalar(DynamicExtents{}, 3);
    FATP_ASSERT_TRUE(checkReference(scalar, a, {}, {}), "Rank-zero left scaling");
    FATP_ASSERT_TRUE(checkReference(a, scalar, {}, {}), "Rank-zero right scaling");
    const auto scalarProduct = tensorDot(scalar, scalar, {}, {});
    FATP_ASSERT_TRUE(scalarProduct.rank() == 0 && scalarProduct() == 9, "Scalar by scalar");
    FATP_ASSERT_TRUE(checkReference(a, a, {2, 0, 1}, {2, 0, 1}), "Full contraction has rank zero");
    Tensor<int> matrix({4, 7}, 2);
    const auto contracted = tensorDot(a, matrix, {2}, {0});
    FATP_ASSERT_TRUE(contracted.extents() == DynamicExtents({2, 3, 7}), "Multiple free axes retain order");
    auto view = a.permuteView({2, 0, 1});
    FATP_ASSERT_TRUE(checkReference(view, b, {0, 2}, {0, 2}), "Permuted view mapping");
    return true;
}

template <typename T>
bool randomLayouts()
{
    std::mt19937 random(7123);
    std::array<T, 1024> storage{};
    for (std::size_t i = 0; i < storage.size(); ++i)
    {
        storage[i] = static_cast<T>(i % 5);
    }
    for (int trial = 0; trial < 120; ++trial)
    {
        const auto ar = static_cast<std::size_t>(random() % 5);
        const auto br = static_cast<std::size_t>(random() % 5);
        const auto count = static_cast<std::size_t>(random()) % (std::min(ar, br) + 1);
        std::vector<TensorAxis> aa(ar), ba(br);
        std::iota(aa.begin(), aa.end(), TensorAxis{0});
        std::iota(ba.begin(), ba.end(), TensorAxis{0});
        std::shuffle(aa.begin(), aa.end(), random);
        std::shuffle(ba.begin(), ba.end(), random);
        aa.resize(count);
        ba.resize(count);
        std::vector<std::size_t> ae(ar), be(br);
        TensorStrides as(ar), bs(br);
        for (auto& extent : ae)
        {
            extent = random() % 4;
        }
        for (auto& extent : be)
        {
            extent = random() % 4;
        }
        for (std::size_t pair = 0; pair < count; ++pair)
        {
            be[static_cast<std::size_t>(ba[pair])] = ae[static_cast<std::size_t>(aa[pair])];
        }
        for (auto& stride : as)
        {
            stride = static_cast<std::ptrdiff_t>(random() % 15) - 7;
        }
        for (auto& stride : bs)
        {
            stride = static_cast<std::ptrdiff_t>(random() % 15) - 7;
        }
        const auto a =
            TensorView<const T>::borrow(storage.data(), TensorLayout(storage.size(), 400, DynamicExtents(ae), as));
        const auto b =
            TensorView<const T>::borrow(storage.data(), TensorLayout(storage.size(), 600, DynamicExtents(be), bs));
        FATP_ASSERT_TRUE(checkReference(a, b, aa, ba), "Seeded rank/axis/signed-stride Cartesian differential");
    }
    return true;
}

FATP_TEST_CASE(randomized_layout_differentials)
{
    FATP_ASSERT_TRUE(randomLayouts<bool>(), "Bool");
    FATP_ASSERT_TRUE(randomLayouts<std::int32_t>(), "Signed widening");
    FATP_ASSERT_TRUE(randomLayouts<std::uint64_t>(), "Unsigned");
    FATP_ASSERT_TRUE(randomLayouts<float>(), "Float");
    FATP_ASSERT_TRUE(randomLayouts<double>(), "Double");
    FATP_ASSERT_TRUE(randomLayouts<long double>(), "Long double");
    return true;
}

FATP_TEST_CASE(named_equivalence_and_plan_bounds)
{
    Tensor<int> a({3, 4}), b({4, 5});
    std::iota(a.begin(), a.end(), -4);
    std::iota(b.begin(), b.end(), 1);
    const auto matrix = tensorDot(a, b, {1}, {0});
    const auto expectedMatrix = matmul(a, b);
    FATP_ASSERT_TRUE(std::equal(matrix.begin(), matrix.end(), expectedMatrix.begin()), "Matmul equivalence");
    const auto av = a.rowView(0).squeezeView();
    const auto bv = b.columnView(1).squeezeView();
    const auto scalar = tensorDot(av, bv, {0}, {0});
    const auto expectedScalar = dot(av, bv);
    FATP_ASSERT_EQ(scalar(), expectedScalar(), "Dot equivalence");
    const auto products = tensorDot(av, bv, {}, {});
    const auto expectedProducts = outer(av, bv);
    FATP_ASSERT_TRUE(std::equal(products.begin(), products.end(), expectedProducts.begin()),
                     "Outer equivalence for integers (floating signed zero differs)");
    Tensor<int> higher({2, 1, 3, 2, 3}, 3);
    const auto permuted = higher.permuteView({4, 0, 3, 1, 2});
    FATP_ASSERT_TRUE(checkReference(higher, permuted, {4, 2, 0, 3, 1}, {0, 4, 1, 2, 3}),
                     "Rank-five owner/permuted full contraction");
    // A trillion output elements can be planned without allocating output storage
    // or a table of offsets. Only the public call materializes the result.
    constexpr std::size_t largeExtent = sizeof(std::ptrdiff_t) >= 8 ? 1000000 : 100;
    const TensorLayout left(1, 0, DynamicExtents{largeExtent, 8, 8}, {0, 0, 0});
    const TensorLayout right(1, 0, DynamicExtents{8, largeExtent, 8}, {0, 0, 0});
    const auto plan = tensor_detail::makeContractionShape(left, right, {2, 1}, {0, 2});
    FATP_ASSERT_TRUE(plan.outputExtents.logicalSize() == largeExtent * largeExtent && plan.inner == 64 &&
                         plan.innerRun == 8 && plan.innerExtents.size() == 1 && plan.leftOutputStrides.size() == 2 &&
                         plan.rightOutputStrides.size() == 2 && plan.leftInnerStrides.size() == 1 &&
                         plan.rightInnerStrides.size() == 1,
                     "Contraction metadata depends on rank, not logical size");
    return true;
}

FATP_TEST_CASE(empty_extreme_and_invalid_axes)
{
    const auto empty = TensorView<const int>::borrow(nullptr, TensorLayout(0, 0, DynamicExtents{2, 0, 3}, {0, 0, 0}));
    Tensor<int> other({0, 4}, 1);
    const auto zeros = tensorDot(empty, other, {1}, {0});
    FATP_ASSERT_TRUE(zeros.extents() == DynamicExtents({2, 3, 4}) && std::all_of(zeros.begin(),
                                                                                 zeros.end(),
                                                                                 [](auto x) {
                                                                                     return x == 0;
                                                                                 }),
                     "Zero inner dimension produces a nonempty zero result without reading null storage");
    const auto noOutput = tensorDot(empty, other, {}, {});
    FATP_ASSERT_TRUE(noOutput.empty(), "Free zero extent yields empty output");
    const auto maximum = static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max());
    const auto huge =
        TensorView<const int>::borrow(nullptr, TensorLayout(0, 0, DynamicExtents{0, maximum, maximum}, {0, 0, 0}));
    const auto hugeEmpty = tensorDot(huge, huge, {1, 2}, {1, 2});
    FATP_ASSERT_TRUE(hugeEmpty.empty(), "Do not overflow an unreachable contracted subdomain");
    FATP_ASSERT_THROWS(tensorDot(huge, huge, {0}, {0}), std::overflow_error, "Unrepresentable output is rejected");
    const int value = 7;
    const auto singleton = TensorView<const int>::borrow(
        &value,
        TensorLayout(1, 0, DynamicExtents{1}, {std::numeric_limits<std::ptrdiff_t>::min()}));
    const auto product = tensorDot(singleton, singleton, {0}, {0});
    FATP_ASSERT_EQ(product(), std::int64_t{49}, "Singleton extreme stride is unreachable");
    Tensor<int> a({2, 3}, 1);
    Tensor<int> b({3, 2}, 1);
    FATP_ASSERT_THROWS(tensorDot(a, b, {0}, {}), std::invalid_argument, "Axis counts");
    FATP_ASSERT_THROWS(tensorDot(a, b, {0, -2}, {1, 0}), std::invalid_argument, "Duplicate after normalization");
    FATP_ASSERT_THROWS(tensorDot(a, b, {0, 1}, {1, -1}), std::invalid_argument, "Right duplicate");
    FATP_ASSERT_THROWS(tensorDot(a, b, {0}, {0}), std::invalid_argument, "Paired extents mismatch");
    FATP_ASSERT_THROWS(tensorDot(a, b, {2}, {0}), std::out_of_range, "Positive axis bounds");
    FATP_ASSERT_THROWS(tensorDot(a, b, {0}, {-3}), std::out_of_range, "Negative axis bounds");
    FATP_ASSERT_THROWS(tensorDot(a, b, {std::numeric_limits<TensorAxis>::min()}, {0}),
                       std::out_of_range,
                       "Minimum signed axis");
    return true;
}

FATP_TEST_CASE(numeric_folds_and_failures)
{
    Tensor<std::int32_t> narrow({2}, 50000);
    const auto wide = tensorDot(narrow, narrow, {0}, {0});
    FATP_ASSERT_EQ(wide(), std::int64_t{5000000000LL}, "Widen before multiplication");
    Tensor<std::int64_t> large({2}, std::numeric_limits<std::int64_t>::max());
    Tensor<std::int64_t> one({2}, 1);
    FATP_ASSERT_THROWS(tensorDot(large, large, {0}, {0}), std::overflow_error, "Signed product overflow");
    FATP_ASSERT_THROWS(tensorDot(large, one, {0}, {0}), std::overflow_error, "Signed sum overflow");
    large.fill(std::numeric_limits<std::int64_t>::min());
    one.fill(-1);
    FATP_ASSERT_THROWS(tensorDot(large, one, {}, {}), std::overflow_error, "Minimum times minus one");
    Tensor<std::uint64_t> unsignedLarge({2}, std::numeric_limits<std::uint64_t>::max());
    Tensor<std::uint64_t> unsignedOne({2}, 1);
    FATP_ASSERT_THROWS(tensorDot(unsignedLarge, unsignedOne, {0}, {0}), std::overflow_error, "Unsigned sum");
    FATP_ASSERT_THROWS(tensorDot(unsignedLarge, unsignedLarge, {}, {}), std::overflow_error, "Unsigned product");
    Tensor<double> values({2, 2});
    values[0] = 1e20;
    values[1] = -1e20;
    values[2] = 3;
    values[3] = 4;
    Tensor<double> ones({2, 2}, 1);
    const auto rowFirst = tensorDot(values, ones, {0, 1}, {0, 1});
    const auto columnFirst = tensorDot(values, ones, {1, 0}, {1, 0});
    FATP_ASSERT_TRUE(rowFirst() == 7 && columnFirst() == 4, "User pair order controls floating fold");
    values.fill(-0.0);
    const auto signedZero = tensorDot(values, ones, {}, {});
    FATP_ASSERT_TRUE(!std::signbit(signedZero[0]), "Even empty-axis contraction folds from positive zero");
    values[0] = std::numeric_limits<double>::quiet_NaN();
    const auto nan = tensorDot(values, ones, {0, 1}, {0, 1});
    FATP_ASSERT_TRUE(std::isnan(nan()), "NaN propagates");
    values.fill(std::numeric_limits<double>::infinity());
    const auto infinity = tensorDot(values, ones, {0, 1}, {0, 1});
    FATP_ASSERT_TRUE(std::isinf(infinity()), "Infinity propagates");
    return true;
}

struct Counts
{
    std::size_t live = 0;
    std::size_t allocations = 0;
    bool fail = false;
};

template <typename T>
struct Allocator
{
    using value_type = T;
    Counts* counts;
    int identity;
    Allocator(Counts* state, int id)
        : counts(state)
        , identity(id)
    {
    }
    template <typename U>
    Allocator(const Allocator<U>& other)
        : counts(other.counts)
        , identity(other.identity)
    {
    }
    T* allocate(std::size_t n)
    {
        if (counts->fail)
        {
            throw std::bad_alloc();
        }
        auto* result = std::allocator<T>{}.allocate(n);
        counts->live += n;
        ++counts->allocations;
        return result;
    }
    void deallocate(T* p, std::size_t n) noexcept
    {
        counts->live -= n;
        std::allocator<T>{}.deallocate(p, n);
    }
    Allocator select_on_container_copy_construction() const
    {
        return {counts, identity + 100};
    }
    template <typename U>
    bool operator==(const Allocator<U>& other) const
    {
        return counts == other.counts && identity == other.identity;
    }
};

FATP_TEST_CASE(ownership_allocators_and_lifetimes)
{
    Counts counts;
    {
        Tensor<int, Allocator<int>> a(std::allocator_arg, Allocator<int>{&counts, 7}, DynamicExtents{2}, 3);
        Tensor<int, Allocator<int>> b(std::allocator_arg, Allocator<int>{&counts, 9}, DynamicExtents{2}, 2);
        const auto first = tensorDot(a, b, {0}, {0});
        FATP_ASSERT_EQ(first.get_allocator().identity, 107, "First owner SOCCC after rebind");
        const auto second = tensorDot(a.asView(), b, {0}, {0});
        FATP_ASSERT_EQ(second.get_allocator().identity, 109, "Right owner if left is a view");
        const auto views = tensorDot(a.asView(), b.asView(), {0}, {0});
        static_assert(std::same_as<typename decltype(views)::allocator_type, TensorAllocator<std::int64_t>>);
        auto explicitResult = tensorDot(a, b, {}, {}, Allocator<std::int64_t>{&counts, 33});
        FATP_ASSERT_EQ(explicitResult.get_allocator().identity, 33, "Explicit allocator unchanged");
        explicitResult[0] = 99;
        FATP_ASSERT_EQ(a[0], 3, "Fresh owner does not alias inputs");
        counts.fail = true;
        FATP_ASSERT_THROWS(tensorDot(a, b, {0}, {0}), std::bad_alloc, "Element allocation failure");
        FATP_ASSERT_THROWS(tensorDot(a, b, {1}, {0}), std::out_of_range, "Validation precedes element allocation");
        counts.fail = false;
    }
    FATP_ASSERT_EQ(counts.live, std::size_t{0}, "All result and source storage reclaimed");
    Tensor<std::int64_t> large({2}, std::numeric_limits<std::int64_t>::max());
    const auto allocationsBefore = counts.allocations;
    FATP_ASSERT_THROWS(tensorDot(large, large, {0}, {0}, Allocator<std::int64_t>{&counts, 0}),
                       std::overflow_error,
                       "Checked product after allocation");
    FATP_ASSERT_TRUE(counts.allocations == allocationsBefore + 1 && counts.live == 0, "Failure releases result");
    const auto retained = [] {
        Tensor<int> owner({2}, 4);
        return owner.asSharedView();
    }();
    const auto retainedResult = tensorDot(retained, retained, {0}, {0});
    FATP_ASSERT_EQ(retainedResult(), std::int64_t{32}, "Shared view keeps storage alive");
#ifndef NDEBUG
    const auto expired = [] {
        Tensor<int> owner({2}, 4);
        return owner.asView();
    }();
    FATP_ASSERT_THROWS(tensorDot(expired, retained, {0}, {0}), std::runtime_error, "Expired borrowed left");
    FATP_ASSERT_THROWS(tensorDot(retained, expired, {0}, {0}), std::runtime_error, "Expired borrowed right");
#endif
    return true;
}

FATP_TEST_CASE(explicit_execution)
{
    ThreadPool pool(3, 0);
    TensorExecutionOptions options;
    options.grainSize = 3;
    options.minimumWork = 0;
    const auto context = TensorExecutionContext::parallel(pool, options);
    Tensor<double> a({5, 3, 4}), b({4, 7, 3});
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        a[i] = static_cast<double>(i % 13) / 7;
    }
    for (std::size_t i = 0; i < b.size(); ++i)
    {
        b[i] = static_cast<double>(i % 11) / 3;
    }
    const auto av = a.sliceView({Slice{}, Slice{}, Slice{std::nullopt, std::nullopt, -1}});
    const auto serial = tensorDot(av, b, {2, 1}, {0, 2});
    const auto parallel = tensorDot(av, b, {2, 1}, {0, 2}, context);
    for (std::size_t i = 0; i < serial.size(); ++i)
    {
        FATP_ASSERT_EQ(std::bit_cast<std::uint64_t>(serial[i]),
                       std::bit_cast<std::uint64_t>(parallel[i]),
                       "Scheduling preserves each output fold bitwise");
    }
    const auto nested = pool.submit([&] {
                                return tensorDot(av, b, {2, 1}, {0, 2}, context);
                            })
                            .get();
    FATP_ASSERT_TRUE(std::equal(serial.begin(), serial.end(), nested.begin()), "Nested caller falls back to serial");
    Counts counts;
    const auto allocated = tensorDot(av, b, {2, 1}, {0, 2}, context, Allocator<double>{&counts, 17});
    FATP_ASSERT_EQ(allocated.get_allocator().identity, 17, "Context allocator unchanged");
    std::stop_source stop;
    stop.request_stop();
    options.cancellation = stop.get_token();
    const auto cancelled = TensorExecutionContext::parallel(pool, options);
    FATP_ASSERT_THROWS(tensorDot(av, b, {2, 1}, {0, 2}, cancelled), TensorExecutionCancelled, "Pre-cancel");
    FATP_ASSERT_THROWS(tensorDot(av, b, {3}, {0}, cancelled), std::out_of_range, "Validation before cancellation");
    options.cancellation = {};
    options.scratch = std::pmr::null_memory_resource();
    const auto noScratch = TensorExecutionContext::parallel(pool, options);
    FATP_ASSERT_THROWS(tensorDot(av, b, {2, 1}, {0, 2}, noScratch), std::bad_alloc, "Scratch failure before submit");
    Tensor<std::int64_t> large({8, 3}, std::numeric_limits<std::int64_t>::max());
    Tensor<std::int64_t> other({3, 8}, 2);
    FATP_ASSERT_THROWS(tensorDot(large, other, {1}, {0}, context), std::overflow_error, "Parallel checked failure");
    pool.wait_idle();
    pool.shutdown();
    FATP_ASSERT_THROWS(tensorDot(av, b, {2, 1}, {0, 2}, context), std::runtime_error, "Stopped pool submission");
    const auto scalar = tensorDot(a, a, {0, 1, 2}, {0, 1, 2}, context);
    FATP_ASSERT_TRUE(scalar.rank() == 0, "One output remains serial even with a stopped pool");
    const auto defaultSerial = tensorDot(av, b, {2, 1}, {0, 2}, TensorExecutionContext{});
    FATP_ASSERT_TRUE(std::equal(serial.begin(), serial.end(), defaultSerial.begin()), "Default context is serial");
    TensorExecutionOptions fallbackOptions;
    const auto belowThreshold = TensorExecutionContext::parallel(pool, fallbackOptions);
    const auto thresholdResult = tensorDot(av, b, {2, 1}, {0, 2}, belowThreshold);
    FATP_ASSERT_TRUE(std::equal(serial.begin(), serial.end(), thresholdResult.begin()), "Default cutoff fallback");
    fallbackOptions.minimumWork = 0;
    fallbackOptions.maxTasks = 1;
    const auto capped = tensorDot(av, b, {2, 1}, {0, 2}, TensorExecutionContext::parallel(pool, fallbackOptions));
    FATP_ASSERT_TRUE(std::equal(serial.begin(), serial.end(), capped.begin()), "Task cap fallback");
    fallbackOptions.maxTasks = 0;
    fallbackOptions.grainSize = 1000;
    const auto oneTile = tensorDot(av, b, {2, 1}, {0, 2}, TensorExecutionContext::parallel(pool, fallbackOptions));
    FATP_ASSERT_TRUE(std::equal(serial.begin(), serial.end(), oneTile.begin()), "Single tile fallback");
    std::stop_source availableStop;
    fallbackOptions.cancellation = availableStop.get_token();
    fallbackOptions.grainSize = 3;
    const auto tiled = tensorDot(av, b, {2, 1}, {0, 2}, TensorExecutionContext::serial(fallbackOptions));
    FATP_ASSERT_TRUE(std::equal(serial.begin(), serial.end(), tiled.begin()), "Repeated cancellation tile calls");
    return true;
}

} // namespace fat_p::testing::tensor_contractions

namespace fat_p::testing
{
bool test_TensorContractions()
{
    FATP_PRINT_HEADER(TENSOR CONTRACTIONS)
    TestRunner runner;
    FATP_RUN_TEST_NS(runner, tensor_contractions, shapes_and_axis_order);
    FATP_RUN_TEST_NS(runner, tensor_contractions, randomized_layout_differentials);
    FATP_RUN_TEST_NS(runner, tensor_contractions, named_equivalence_and_plan_bounds);
    FATP_RUN_TEST_NS(runner, tensor_contractions, empty_extreme_and_invalid_axes);
    FATP_RUN_TEST_NS(runner, tensor_contractions, numeric_folds_and_failures);
    FATP_RUN_TEST_NS(runner, tensor_contractions, ownership_allocators_and_lifetimes);
    FATP_RUN_TEST_NS(runner, tensor_contractions, explicit_execution);
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
    return fat_p::testing::test_TensorContractions() ? 0 : 1;
}
#endif
