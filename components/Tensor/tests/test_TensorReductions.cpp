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

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <numeric>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "TensorReductions.h"

#include "FatPTest.h"
#include "TensorTestSupport.h"

namespace fat_p::testing::tensor_reductions
{

using tensor_support::LayoutSpec;

struct ReductionReference
{
    std::vector<std::size_t> extents;
    std::vector<std::vector<std::ptrdiff_t>> domains;
};

// Enumerate output coordinates first, then each selected source coordinate.
// No production axis normalization, linear-index decoding, or iteration plan is used.
ReductionReference referenceDomains(const LayoutSpec& source, std::size_t reducedMask, bool keepDimensions)
{
    if (source.extents.size() > 5 || source.extents.size() != source.strides.size() ||
        std::any_of(source.extents.begin(), source.extents.end(), [](auto extent) { return extent > 4; }))
    {
        throw std::logic_error("Reduction oracle used outside its bounded domain");
    }
    ReductionReference result;
    std::vector<std::size_t> retainedAxes;
    for (std::size_t axis = 0; axis < source.extents.size(); ++axis)
    {
        if ((reducedMask & (std::size_t{1} << axis)) == 0)
        {
            retainedAxes.push_back(axis);
            result.extents.push_back(source.extents[axis]);
        }
        else if (keepDimensions)
        {
            result.extents.push_back(1);
        }
    }
    std::vector<std::size_t> coordinates(source.extents.size(), 0);
    const auto visitDomain = [&](auto&& self, std::size_t axis) -> void {
        if (axis == source.extents.size())
        {
            auto offset = source.origin;
            for (std::size_t index = 0; index < coordinates.size(); ++index)
            {
                offset += static_cast<std::ptrdiff_t>(coordinates[index]) * source.strides[index];
            }
            result.domains.back().push_back(offset);
        }
        else if ((reducedMask & (std::size_t{1} << axis)) != 0)
        {
            for (std::size_t index = 0; index < source.extents[axis]; ++index)
            {
                coordinates[axis] = index;
                self(self, axis + 1);
            }
        }
        else
        {
            self(self, axis + 1);
        }
    };
    const auto visitOutput = [&](auto&& self, std::size_t position) -> void {
        if (position == retainedAxes.size())
        {
            result.domains.emplace_back();
            visitDomain(visitDomain, 0);
            return;
        }
        const auto axis = retainedAxes[position];
        for (std::size_t index = 0; index < source.extents[axis]; ++index)
        {
            coordinates[axis] = index;
            self(self, position + 1);
        }
    };
    visitOutput(visitOutput, 0);
    return result;
}

template <typename Result, typename Value>
bool checkResult(const Result& actual, const std::vector<std::size_t>& extents,
                 const std::vector<Value>& expected)
{
    FATP_ASSERT_TRUE(actual.extents() == DynamicExtents(extents), "Reduction output shape must match the oracle");
    FATP_ASSERT_EQ(actual.size(), expected.size(), "Every output coordinate needs one result");
    for (std::size_t index = 0; index < expected.size(); ++index)
    {
        FATP_ASSERT_EQ(actual[index], expected[index], "Reduction value differs at output " + std::to_string(index));
    }
    return true;
}

// Assertion macros retain arguments by reference. Copy a scalar while its
// temporary result owner is still alive instead of retaining an element reference.
template <typename Result>
auto scalarValue(const Result& result) -> typename Result::value_type
{
    return result();
}

// Numeric inputs here are only -1, 0, or 1. Integer reference arithmetic is exact;
// rounding, NaN, signed-zero, and overflow behavior have separate boundary tests.
template <typename Source>
bool verifyReference(const Source& source, const LayoutSpec& layout, const typename Source::value_type* root,
                     std::size_t reducedMask, const std::vector<TensorAxis>& axes, bool keepDimensions)
{
    using value_type = typename Source::value_type;
    const auto reference = referenceDomains(layout, reducedMask, keepDimensions);
    std::vector<std::int64_t> sums;
    std::vector<std::int64_t> products;
    std::vector<double> means;
    std::vector<value_type> minima;
    std::vector<value_type> maxima;
    std::vector<std::size_t> minimumIndices;
    std::vector<std::size_t> maximumIndices;
    std::vector<bool> conjunctions;
    std::vector<bool> disjunctions;
    bool hasEmptyDomain = false;
    for (const auto& domain : reference.domains)
    {
        std::int64_t total = 0;
        std::int64_t multiplied = 1;
        bool every = true;
        bool some = false;
        std::vector<value_type> values;
        for (const auto offset : domain)
        {
            const auto value = root[offset];
            FATP_ASSERT_TRUE(static_cast<double>(value) >= -1 && static_cast<double>(value) <= 1,
                             "Reference arithmetic requires bounded values");
            values.push_back(value);
            total += static_cast<std::int64_t>(value);
            multiplied *= static_cast<std::int64_t>(value);
            every = every && static_cast<bool>(value);
            some = some || static_cast<bool>(value);
        }
        sums.push_back(total);
        products.push_back(multiplied);
        conjunctions.push_back(every);
        disjunctions.push_back(some);
        if (values.empty())
        {
            hasEmptyDomain = true;
        }
        else
        {
            means.push_back(static_cast<double>(total) / static_cast<double>(values.size()));
            const auto least = std::min_element(values.begin(), values.end());
            const auto greatest = std::max_element(values.begin(), values.end());
            minima.push_back(*least);
            maxima.push_back(*greatest);
            minimumIndices.push_back(static_cast<std::size_t>(least - values.begin()));
            maximumIndices.push_back(static_cast<std::size_t>(greatest - values.begin()));
        }
    }
    FATP_ASSERT_TRUE(checkResult(sum(source, axes, keepDimensions), reference.extents, sums), "Sum oracle");
    FATP_ASSERT_TRUE(checkResult(product(source, axes, keepDimensions), reference.extents, products), "Product oracle");
    FATP_ASSERT_TRUE(checkResult(all(source, axes, keepDimensions), reference.extents, conjunctions), "All oracle");
    FATP_ASSERT_TRUE(checkResult(any(source, axes, keepDimensions), reference.extents, disjunctions), "Any oracle");
    if (hasEmptyDomain)
    {
        FATP_ASSERT_THROWS(mean(source, axes, keepDimensions), std::domain_error, "Empty mean domain must fail");
        FATP_ASSERT_THROWS(min(source, axes, keepDimensions), std::domain_error, "Empty min domain must fail");
        FATP_ASSERT_THROWS(max(source, axes, keepDimensions), std::domain_error, "Empty max domain must fail");
        FATP_ASSERT_THROWS(argmin(source, axes, keepDimensions), std::domain_error, "Empty argmin domain must fail");
        FATP_ASSERT_THROWS(argmax(source, axes, keepDimensions), std::domain_error, "Empty argmax domain must fail");
    }
    else
    {
        FATP_ASSERT_TRUE(checkResult(mean(source, axes, keepDimensions), reference.extents, means), "Mean oracle");
        FATP_ASSERT_TRUE(checkResult(min(source, axes, keepDimensions), reference.extents, minima), "Min oracle");
        FATP_ASSERT_TRUE(checkResult(max(source, axes, keepDimensions), reference.extents, maxima), "Max oracle");
        FATP_ASSERT_TRUE(checkResult(argmin(source, axes, keepDimensions), reference.extents, minimumIndices),
                         "Argmin oracle uses canonical reduced-axis order, not requested order");
        FATP_ASSERT_TRUE(checkResult(argmax(source, axes, keepDimensions), reference.extents, maximumIndices),
                         "Argmax oracle uses canonical reduced-axis order, not requested order");
    }
    std::vector<std::int64_t> initializedSums;
    std::vector<std::int64_t> initializedProducts;
    std::vector<value_type> initializedMinima;
    std::vector<value_type> initializedMaxima;
    for (const auto& domain : reference.domains)
    {
        std::int64_t total = 2;
        std::int64_t multiplied = 2;
        value_type least = 1;
        value_type greatest = 0;
        for (const auto offset : domain)
        {
            total += static_cast<std::int64_t>(root[offset]);
            multiplied *= static_cast<std::int64_t>(root[offset]);
            least = std::min(least, root[offset]);
            greatest = std::max(greatest, root[offset]);
        }
        initializedSums.push_back(total);
        initializedProducts.push_back(multiplied);
        initializedMinima.push_back(least);
        initializedMaxima.push_back(greatest);
    }
    FATP_ASSERT_TRUE(checkResult(sum(source, axes, keepDimensions, 2), reference.extents, initializedSums),
                     "Sum initial participates once in every domain");
    FATP_ASSERT_TRUE(checkResult(product(source, axes, keepDimensions, 2), reference.extents, initializedProducts),
                     "Product initial participates once in every domain");
    FATP_ASSERT_TRUE(checkResult(min(source, axes, keepDimensions, value_type{1}),
                                reference.extents, initializedMinima),
                     "Minimum initial participates even in nonempty domains");
    FATP_ASSERT_TRUE(checkResult(max(source, axes, keepDimensions, value_type{0}),
                                reference.extents, initializedMaxima),
                     "Maximum initial participates even in nonempty domains");
    return true;
}

std::vector<TensorAxis> requestedAxes(std::size_t rank, std::size_t mask, bool negative)
{
    std::vector<TensorAxis> axes;
    for (std::size_t position = rank; position > 0; --position)
    {
        const auto axis = position - 1;
        if ((mask & (std::size_t{1} << axis)) != 0)
        {
            axes.push_back(static_cast<TensorAxis>(axis) - (negative ? static_cast<TensorAxis>(rank) : 0));
        }
    }
    return axes;
}

std::size_t placeInStorage(LayoutSpec& layout)
{
    const auto bounds = tensor_support::reachableBounds(layout);
    if (!bounds)
    {
        layout.origin = 3;
        return 8;
    }
    layout.origin += 3 - bounds->first;
    return static_cast<std::size_t>(bounds->second - bounds->first + 7);
}

template <typename Value, typename Sum, typename Mean>
constexpr bool reductionTypes()
{
    using source = Tensor<Value>;
    using view = TensorView<const Value>;
    static_assert(std::same_as<TensorSumType<Value>, Sum>);
    static_assert(std::same_as<TensorMeanType<Value>, Mean>);
    static_assert(std::same_as<typename decltype(sum(std::declval<const source&>()))::value_type, Sum>);
    static_assert(std::same_as<typename decltype(product(std::declval<const view&>()))::value_type, Sum>);
    static_assert(std::same_as<typename decltype(mean(std::declval<const source&>()))::value_type, Mean>);
    static_assert(std::same_as<typename decltype(min(std::declval<const view&>()))::value_type, Value>);
    static_assert(std::same_as<typename decltype(max(std::declval<const source&>()))::value_type, Value>);
    static_assert(std::same_as<typename decltype(argmin(std::declval<const source&>()))::value_type, std::size_t>);
    static_assert(std::same_as<typename decltype(argmax(std::declval<const view&>()))::value_type, std::size_t>);
    static_assert(std::same_as<typename decltype(all(std::declval<const source&>()))::value_type, bool>);
    static_assert(std::same_as<typename decltype(any(std::declval<const view&>()))::value_type, bool>);
    return true;
}

static_assert(reductionTypes<bool, std::size_t, double>());
static_assert(reductionTypes<std::int8_t, std::int64_t, double>());
static_assert(reductionTypes<std::uint8_t, std::uint64_t, double>());
static_assert(reductionTypes<std::int16_t, std::int64_t, double>());
static_assert(reductionTypes<std::uint16_t, std::uint64_t, double>());
static_assert(reductionTypes<std::int32_t, std::int64_t, double>());
static_assert(reductionTypes<std::uint32_t, std::uint64_t, double>());
static_assert(reductionTypes<std::int64_t, std::int64_t, double>());
static_assert(reductionTypes<std::uint64_t, std::uint64_t, double>());
static_assert(reductionTypes<float, float, double>());
static_assert(reductionTypes<double, double, double>());
static_assert(reductionTypes<long double, long double, long double>());
static_assert(reductionTypes<char, std::conditional_t<std::is_signed_v<char>, std::int64_t, std::uint64_t>, double>());
static_assert(reductionTypes<char8_t, std::uint64_t, double>());
static_assert(reductionTypes<char16_t, std::uint64_t, double>());
static_assert(reductionTypes<char32_t, std::uint64_t, double>());
static_assert(reductionTypes<wchar_t,
                             std::conditional_t<std::is_signed_v<wchar_t>, std::int64_t, std::uint64_t>, double>());

template <typename Value>
bool checkFloatingTruth()
{
    Tensor<Value> source({4, 3});
    const auto nan = std::numeric_limits<Value>::quiet_NaN();
    const std::array<Value, 12> values{Value{0}, -Value{0}, Value{0}, nan,
                                      std::numeric_limits<Value>::infinity(), Value{-1},
                                      Value{0}, nan, Value{0}, -Value{0}, Value{1}, Value{0}};
    std::copy(values.begin(), values.end(), source.begin());
    FATP_ASSERT_TRUE(checkResult(all(source, {1}), {4}, std::vector<bool>{false, true, false, false}),
                     "Every floating type uses arithmetic-to-bool conversion");
    FATP_ASSERT_TRUE(checkResult(any(source, {1}), {4}, std::vector<bool>{false, true, true, true}),
                     "Floating any distinguishes signed zeros from NaNs and other nonzero values");
    return true;
}

template <typename Check>
bool forEachReduction(Check&& check)
{
    return check([](const auto& source, const auto&... options) { return sum(source, options...); }) &&
        check([](const auto& source, const auto&... options) { return product(source, options...); }) &&
        check([](const auto& source, const auto&... options) { return mean(source, options...); }) &&
        check([](const auto& source, const auto&... options) { return min(source, options...); }) &&
        check([](const auto& source, const auto&... options) { return max(source, options...); }) &&
        check([](const auto& source, const auto&... options) { return argmin(source, options...); }) &&
        check([](const auto& source, const auto&... options) { return argmax(source, options...); }) &&
        check([](const auto& source, const auto&... options) { return all(source, options...); }) &&
        check([](const auto& source, const auto&... options) { return any(source, options...); });
}

struct AllocationCounts
{
    std::size_t allocations = 0;
    std::size_t deallocations = 0;
    std::size_t liveElements = 0;
    bool fail = false;
};

template <typename T>
class SocccAllocator
{
public:
    using value_type = T;

    explicit SocccAllocator(int identity = 0, AllocationCounts* counts = nullptr) noexcept
        : mIdentity(identity)
        , mCounts(counts)
    {
    }

    template <typename U>
    SocccAllocator(const SocccAllocator<U>& other) noexcept
        : mIdentity(other.identity())
        , mCounts(other.counts())
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
            ++mCounts->deallocations;
            mCounts->liveElements -= count;
        }
        std::allocator<T>{}.deallocate(storage, count);
    }

    [[nodiscard]] SocccAllocator select_on_container_copy_construction() const noexcept
    {
        return SocccAllocator(mIdentity + 100, mCounts);
    }

    [[nodiscard]] int identity() const noexcept { return mIdentity; }
    [[nodiscard]] AllocationCounts* counts() const noexcept { return mCounts; }

    template <typename U>
    [[nodiscard]] bool operator==(const SocccAllocator<U>& other) const noexcept
    {
        return mIdentity == other.identity() && mCounts == other.counts();
    }

private:
    int mIdentity = 0;
    AllocationCounts* mCounts = nullptr;
};

FATP_TEST_CASE(coordinate_reference_anchors)
{
    const LayoutSpec layout{{2, 3, 2}, {1, 4, -2}, 2};
    const std::vector<std::vector<std::ptrdiff_t>> expected{{2, 0, 3, 1}, {6, 4, 7, 5}, {10, 8, 11, 9}};
    const auto reduced = referenceDomains(layout, 5, false);
    FATP_ASSERT_TRUE(reduced.extents == std::vector<std::size_t>{3}, "Oracle retains only the middle axis");
    FATP_ASSERT_TRUE(reduced.domains == expected, "Oracle must retain signed root offsets and canonical domain order");
    const auto kept = referenceDomains(layout, 5, true);
    FATP_ASSERT_TRUE(kept.extents == std::vector<std::size_t>({1, 3, 1}), "Oracle keeps selected singleton axes");
    FATP_ASSERT_TRUE(kept.domains == expected, "Keeping dimensions must not alter domain traversal");
    const auto scalar = referenceDomains(LayoutSpec{{}, {}, 4}, 0, false);
    FATP_ASSERT_TRUE(scalar.extents.empty() && scalar.domains ==
                         std::vector<std::vector<std::ptrdiff_t>>{{4}}, "Scalar has one one-element domain");
    const auto emptyOutput = referenceDomains(LayoutSpec{{0, 2}, {2, 1}, 0}, 2, false);
    FATP_ASSERT_TRUE(emptyOutput.domains.empty(), "A retained empty axis leaves no domains");
    const auto emptyDomains = referenceDomains(LayoutSpec{{2, 0}, {0, 0}, 0}, 2, false);
    FATP_ASSERT_TRUE(emptyDomains.domains == std::vector<std::vector<std::ptrdiff_t>>(2),
                     "A reduced empty axis leaves two empty domains");
    return true;
}

FATP_TEST_CASE(exhaustive_small_reduction_coordinates)
{
    std::size_t cases = 0;
    std::size_t shapeCount = 1;
    for (std::size_t rank = 0; rank <= 3; ++rank)
    {
        for (std::size_t shapeCode = 0; shapeCode < shapeCount; ++shapeCode)
        {
            std::vector<std::size_t> extents(rank);
            auto code = shapeCode;
            for (auto& extent : extents)
            {
                extent = code % 4;
                code /= 4;
            }
            for (std::size_t variant = 0; variant < 4; ++variant)
            {
                LayoutSpec layout{extents, std::vector<std::ptrdiff_t>(rank), 0};
                std::ptrdiff_t stride = variant == 1 ? 2 : 1;
                for (std::size_t position = rank; position > 0; --position)
                {
                    const auto axis = position - 1;
                    layout.strides[axis] = variant == 3 ? 0 : variant == 2 ? 1 :
                        variant == 1 && axis % 2 == 0 ? -stride : stride;
                    stride *= static_cast<std::ptrdiff_t>(std::max(std::size_t{1}, extents[axis]));
                }
                std::vector<int> storage(placeInStorage(layout));
                for (std::size_t index = 0; index < storage.size(); ++index)
                {
                    storage[index] = static_cast<int>((index * 7 + variant) % 3) - 1;
                }
                const auto before = storage;
                const auto source = TensorView<const int>::borrow(storage.data(),
                    TensorLayout(storage.size(), layout.origin, DynamicExtents(extents),
                                 TensorStrides(layout.strides)));
                const auto fullMask = (std::size_t{1} << rank) - 1;
                for (auto mask = rank == 0 ? std::size_t{0} : std::size_t{1}; mask <= fullMask; ++mask)
                {
                    for (const bool keep : {false, true})
                    {
                        const auto axes = mask == fullMask && variant == 0 ? std::vector<TensorAxis>{} :
                            requestedAxes(rank, mask, keep);
                        FATP_ASSERT_TRUE(verifyReference(source, layout, storage.data(), mask, axes, keep),
                                         "Exhaustive reduction case " + std::to_string(cases));
                        ++cases;
                    }
                }
                FATP_ASSERT_TRUE(storage == before, "Every reduction must preserve source and padding values");
            }
        }
        shapeCount *= 4;
    }
    FATP_ASSERT_EQ(cases, std::size_t{4008}, "The full bounded shape/layout/axis/keep-dimensions grid must run");
    return true;
}

FATP_TEST_CASE(randomized_signed_layout_reductions)
{
    tensor_support::DeterministicLayoutGenerator generator(0xC01D5EED);
    std::mt19937_64 random(0xA8152026);
    std::size_t nonempty = 0;
    std::size_t overlapping = 0;
    std::size_t reversed = 0;
    for (std::size_t sample = 0; sample < 600; ++sample)
    {
        const auto rank = sample % 6;
        auto layout = generator.next(rank, 4);
        std::vector<double> storage(placeInStorage(layout));
        for (auto& value : storage)
        {
            value = static_cast<double>(static_cast<int>(random() % 3) - 1);
        }
        const auto before = storage;
        const auto source = TensorView<const double>::borrow(storage.data(),
            TensorLayout(storage.size(), layout.origin, DynamicExtents(layout.extents), TensorStrides(layout.strides)));
        const auto fullMask = (std::size_t{1} << rank) - 1;
        const auto mask = rank == 0 ? std::size_t{0} : std::size_t{1} + static_cast<std::size_t>(random() % fullMask);
        const auto axes = sample % 7 == 0 ? std::vector<TensorAxis>{} : requestedAxes(rank, mask, sample % 2 == 0);
        const auto effectiveMask = axes.empty() ? fullMask : mask;
        FATP_ASSERT_TRUE(verifyReference(source, layout, storage.data(), effectiveMask, axes, sample % 2 != 0),
                         "Randomized reduction sample " + std::to_string(sample));
        FATP_ASSERT_TRUE(storage == before, "Read-only overlapping inputs and padding must not be modified");
        auto offsets = tensor_support::enumerateOffsets(layout);
        if (!offsets.empty())
        {
            ++nonempty;
            std::sort(offsets.begin(), offsets.end());
            overlapping += std::adjacent_find(offsets.begin(), offsets.end()) != offsets.end() ?
                std::size_t{1} : std::size_t{0};
            reversed += std::any_of(layout.strides.begin(), layout.strides.end(),
                                    [](auto stride) { return stride < 0; }) ? std::size_t{1} : std::size_t{0};
        }
    }
    FATP_ASSERT_GE(nonempty, std::size_t{250}, "Randomized coverage must exercise actual element reads");
    FATP_ASSERT_GE(overlapping, std::size_t{30}, "Randomized coverage must include aliased read-only inputs");
    FATP_ASSERT_GE(reversed, std::size_t{100}, "Randomized coverage must include nonempty negative-stride inputs");
    return true;
}

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

FATP_TEST_CASE(empty_output_does_not_count_unreachable_domains)
{
    const auto largest = std::numeric_limits<std::size_t>::max();
    const auto source = TensorView<const int>::borrow(nullptr,
        TensorLayout(0, 0, DynamicExtents{0, largest, 2}, TensorStrides{0, 0, 0}));
    const auto result = mean(source, {1, 2});
    FATP_ASSERT_TRUE(result.extents() == DynamicExtents{0},
                     "An empty output has no domains whose element count needs representation");
    FATP_ASSERT_TRUE(result.empty(), "An unreachable oversized domain must not prevent an empty mean result");
    FATP_ASSERT_TRUE(forEachReduction([&](const auto& operation) {
        const auto empty = operation(source, std::vector<TensorAxis>{1, 2}, true);
        FATP_ASSERT_TRUE(empty.extents() == DynamicExtents({0, 1, 1}),
                         "Every reduction must preserve kept singleton axes in an empty result");
        FATP_ASSERT_THROWS(operation(source, std::vector<TensorAxis>{0}), std::overflow_error,
                           "An enormous nonempty output is rejected by checked output-shape construction");
        return true;
    }), "All reductions agree that zero output means no reduction domains");
    FATP_ASSERT_THROWS(mean(source, {0, 1, 2}), std::domain_error,
                       "Reducing all axes creates a scalar empty domain, which mean must reject");
    const auto zeroLast = TensorView<const int>::borrow(nullptr,
        TensorLayout(0, 0, DynamicExtents{2, largest, 2, 0}, TensorStrides{0, 0, 0, 0}));
    const auto zeroMiddle = TensorView<const int>::borrow(nullptr,
        TensorLayout(0, 0, DynamicExtents{2, largest, 0, 2}, TensorStrides{0, 0, 0, 0}));
    for (const auto& input : {zeroLast, zeroMiddle})
    {
        const std::vector<TensorAxis> axes{1, 2, 3};
        FATP_ASSERT_TRUE(checkResult(sum(input, axes), {2}, std::vector<std::int64_t>{0, 0}),
                         "A zero extent dominates oversized factors regardless of position");
        FATP_ASSERT_TRUE(checkResult(product(input, axes), {2}, std::vector<std::int64_t>{1, 1}),
                         "Empty products use identity even with a late zero extent");
        FATP_ASSERT_TRUE(checkResult(all(input, axes), {2}, std::vector<bool>{true, true}), "Late-zero all identity");
        FATP_ASSERT_TRUE(checkResult(any(input, axes), {2}, std::vector<bool>{false, false}), "Late-zero any identity");
        FATP_ASSERT_THROWS(mean(input, axes), std::domain_error, "Zero detection precedes reduced-factor overflow");
        FATP_ASSERT_THROWS(min(input, axes), std::domain_error, "Late-zero extrema need initial");
        FATP_ASSERT_THROWS(max(input, axes), std::domain_error, "Late-zero maximum needs initial");
        FATP_ASSERT_THROWS(argmin(input, axes), std::domain_error, "Late-zero argmin has no winner");
        FATP_ASSERT_THROWS(argmax(input, axes), std::domain_error, "Late-zero argmax has no winner");
    }
    const auto pointerMaximum = static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max());
    const auto pointerOversized = TensorView<const int>::borrow(nullptr,
        TensorLayout(0, 0, DynamicExtents{0, largest}, TensorStrides{0, 0}));
    const auto productOversized = TensorView<const int>::borrow(nullptr,
        TensorLayout(0, 0, DynamicExtents{0, pointerMaximum, 3}, TensorStrides{0, 0, 0}));
    FATP_ASSERT_TRUE(forEachReduction([&](const auto& operation) {
        FATP_ASSERT_THROWS(operation(pointerOversized, std::vector<TensorAxis>{0}), std::overflow_error,
                           "Output size can fit size_t yet exceed ptrdiff_t");
        FATP_ASSERT_THROWS(operation(productOversized, std::vector<TensorAxis>{0}), std::overflow_error,
                           "Output multiplication can also overflow size_t itself");
        return true;
    }), "Both output-size rejection branches precede element allocation");
    return true;
}

FATP_TEST_CASE(integral_accumulator_boundaries)
{
    constexpr auto kMinimum = std::numeric_limits<std::int64_t>::lowest();
    constexpr auto kMaximum = std::numeric_limits<std::int64_t>::max();
    Tensor<std::int64_t> values({3});
    for (const auto pair : {std::array<std::int64_t, 2>{kMaximum, 1}, {kMinimum, -1}})
    {
        values[0] = pair[0];
        values[1] = pair[1];
        values[2] = -pair[1];
        FATP_ASSERT_THROWS(sum(values), std::overflow_error,
                           "Intermediate signed overflow fails even when the mathematical final sum fits");
        FATP_ASSERT_EQ(values[0], pair[0], "Failed sum must preserve the source");
    }
    Tensor<std::int64_t> factors({2});
    for (const auto pair : {std::array<std::int64_t, 2>{kMaximum, 2}, {kMinimum, -1},
                           {kMaximum, -2}, {-kMaximum, -2}, {-1, kMinimum}})
    {
        factors[0] = pair[0];
        factors[1] = pair[1];
        FATP_ASSERT_THROWS(product(factors), std::overflow_error, "All overflowing sign quadrants must throw");
        FATP_ASSERT_EQ(factors[0], pair[0], "Failed product must preserve its source");
        FATP_ASSERT_EQ(factors[1], pair[1], "Failed product must not change subsequent operands");
    }
    factors[0] = kMinimum;
    factors[1] = 1;
    FATP_ASSERT_EQ(scalarValue(product(factors)), kMinimum, "Minimum times one remains representable");
    FATP_ASSERT_THROWS(product(factors, {}, false, -1), std::overflow_error,
                       "Initial participates before the first source multiplication");
    factors[1] = 0;
    FATP_ASSERT_EQ(scalarValue(product(factors)), std::int64_t{0}, "Minimum times zero is defined");
    factors[0] = kMaximum;
    FATP_ASSERT_THROWS(sum(factors, {}, false, 1), std::overflow_error, "Initial addition is checked");
    Tensor<std::uint64_t> unsignedValues({2});
    unsignedValues[0] = std::numeric_limits<std::uint64_t>::max();
    unsignedValues[1] = 1;
    FATP_ASSERT_THROWS(sum(unsignedValues), std::overflow_error, "Unsigned sum must not wrap");
    unsignedValues[1] = 2;
    FATP_ASSERT_THROWS(product(unsignedValues), std::overflow_error, "Unsigned product must not wrap");
    Tensor<std::int8_t> narrowSigned({2}, std::numeric_limits<std::int8_t>::max());
    Tensor<std::uint8_t> narrowUnsigned({2}, std::numeric_limits<std::uint8_t>::max());
    FATP_ASSERT_EQ(scalarValue(sum(narrowSigned)), std::int64_t{254}, "Narrow sum widens before addition");
    FATP_ASSERT_EQ(scalarValue(product(narrowUnsigned)), std::uint64_t{65025},
                   "Narrow product widens before multiplication");
    Tensor<char16_t> codeUnits({2});
    codeUnits[0] = u'a';
    codeUnits[1] = u'b';
    FATP_ASSERT_EQ(scalarValue(sum(codeUnits)), std::uint64_t{195}, "Character elements are numeric code-unit values");
    Tensor<int> initialSelection({2, 3});
    const std::array<int, 6> initialInputs{-3, -2, -1, 1, 2, 3};
    std::copy(initialInputs.begin(), initialInputs.end(), initialSelection.begin());
    FATP_ASSERT_TRUE(checkResult(min(initialSelection, {1}, false, 0), {2}, std::vector<int>{-3, 0}),
                     "Interior minimum initial can be replaced in one domain and retained in another");
    FATP_ASSERT_TRUE(checkResult(max(initialSelection, {1}, false, 0), {2}, std::vector<int>{0, 3}),
                     "Interior maximum initial participates independently in every domain");
    return true;
}

FATP_TEST_CASE(floating_order_nan_infinity_and_signed_zero)
{
    Tensor<float> small({3});
    const auto largeFloat = std::ldexp(1.0F, std::numeric_limits<float>::digits);
    small[0] = largeFloat;
    small[1] = 1;
    small[2] = -largeFloat;
    const auto precedingFloat = std::nextafter(largeFloat, 0.0F);
    const float reversePartial = -largeFloat + 1.0F;
    FATP_ASSERT_EQ(largeFloat - precedingFloat, 1.0F, "Spacing below this power of two is one, not two");
    FATP_ASSERT_EQ(reversePartial, -precedingFloat, "Negative power of two plus one is exactly representable");
    FATP_ASSERT_EQ(reversePartial + largeFloat, 1.0F, "The independent reverse scalar fold produces one");
    FATP_ASSERT_EQ(scalarValue(sum(small)), 0.0F, "Float sum rounds each step in float, in logical order");
    FATP_ASSERT_EQ(scalarValue(sum(small.sliceView({Slice{{}, {}, -1}}))), 1.0F,
                   "Negative-stride input folds in logical order, not ascending storage order");
    if constexpr (std::numeric_limits<double>::digits > std::numeric_limits<float>::digits)
    {
        FATP_ASSERT_EQ(scalarValue(mean(small)), 1.0 / 3.0, "Float mean converts before accumulation in double");
    }
    Tensor<double> ordered({3});
    const auto largeDouble = std::ldexp(1.0, std::numeric_limits<double>::digits);
    ordered[0] = largeDouble;
    ordered[1] = 1;
    ordered[2] = -largeDouble;
    for (int repeat = 0; repeat < 5; ++repeat)
    {
        FATP_ASSERT_EQ(scalarValue(sum(ordered)), 0.0, "Serial sum is a left fold, not a reordered or compensated sum");
        FATP_ASSERT_EQ(scalarValue(mean(ordered)), 0.0, "Double mean uses the same double accumulator order");
    }
    const auto infinity = std::numeric_limits<double>::infinity();
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    Tensor<double> special({2, 3});
    const std::array<double, 6> inputs{4.0, nan, nan, -0.0, 0.0, -0.0};
    std::copy(inputs.begin(), inputs.end(), special.begin());
    const auto minima = min(special, {1});
    const auto maxima = max(special, {1});
    const auto minimumIndices = argmin(special, {1});
    const auto maximumIndices = argmax(special, {1});
    FATP_ASSERT_TRUE(std::isnan(minima[0]) && std::isnan(maxima[0]), "Both extrema propagate NaN");
    FATP_ASSERT_TRUE(std::signbit(minima[1]) && std::signbit(maxima[1]),
                     "Signed-zero ties preserve the first encountered representation");
    FATP_ASSERT_EQ(minimumIndices[0], std::size_t{1}, "First NaN wins in argmin");
    FATP_ASSERT_EQ(maximumIndices[0], std::size_t{1}, "First NaN wins in argmax");
    FATP_ASSERT_EQ(minimumIndices[1], std::size_t{0}, "Signed-zero ties keep index zero");
    FATP_ASSERT_EQ(maximumIndices[1], std::size_t{0}, "Argmax has the same tie rule");
    FATP_ASSERT_TRUE(std::isnan(min(special, {1}, false, nan)[1]), "Initial NaN participates before finite values");
    FATP_ASSERT_FALSE(std::signbit(max(special, {1}, false, 0.0)[1]), "A tied initial zero keeps its sign");
    Tensor<double> exceptional({2});
    exceptional[0] = infinity;
    exceptional[1] = -infinity;
    FATP_ASSERT_TRUE(std::isnan(sum(exceptional)()), "Opposite infinities produce NaN under ordinary addition");
    FATP_ASSERT_TRUE(std::isnan(mean(exceptional)()), "Mean preserves exceptional arithmetic results");
    FATP_ASSERT_EQ(scalarValue(min(exceptional)), -infinity, "Negative infinity is a minimum");
    FATP_ASSERT_EQ(scalarValue(max(exceptional)), infinity, "Positive infinity is a maximum");
    exceptional[1] = 0;
    FATP_ASSERT_TRUE(std::isnan(product(exceptional)()), "Infinity times zero uses floating arithmetic");
    exceptional[0] = std::numeric_limits<double>::max();
    exceptional[1] = 2;
    FATP_ASSERT_TRUE(std::isinf(product(exceptional)()), "Floating overflow is not an integral-overflow exception");
    Tensor<std::uint64_t> wide({}, std::numeric_limits<std::uint64_t>::max());
    FATP_ASSERT_EQ(scalarValue(mean(wide)), static_cast<double>(wide()),
                   "Integral mean permits integer-to-double rounding");
    Tensor<long double> extended({2});
    const auto increment = std::ldexp(1.0L, 2 - std::numeric_limits<long double>::digits);
    extended[0] = 1.0L;
    extended[1] = increment;
    FATP_ASSERT_EQ(scalarValue(sum(extended)), 1.0L + increment, "Long-double sum must not narrow through double");
    FATP_ASSERT_EQ(scalarValue(mean(extended)), (1.0L + increment) / 2.0L,
                   "Long-double mean retains its accumulator type");
    Tensor<double> negativeZeros({2}, -0.0);
    FATP_ASSERT_FALSE(std::signbit(sum(negativeZeros)()), "The default additive identity is positive zero");
    FATP_ASSERT_TRUE(std::signbit(sum(negativeZeros, {}, false, -0.0)()),
                     "An explicit negative-zero seed participates in the floating fold");
    return true;
}

FATP_TEST_CASE(boolean_truth_and_identities)
{
    FATP_ASSERT_TRUE(checkFloatingTruth<float>(), "Float truth table");
    FATP_ASSERT_TRUE(checkFloatingTruth<double>(), "Double truth table");
    FATP_ASSERT_TRUE(checkFloatingTruth<long double>(), "Long-double truth table");
    Tensor<int> nonUnit({}, 2);
    FATP_ASSERT_TRUE(all(nonUnit)() && any(nonUnit)(), "Nonunit integers are true, not only integer one");
    Tensor<bool> flags({2, 3});
    const std::array<bool, 6> inputs{true, false, true, true, true, true};
    std::copy(inputs.begin(), inputs.end(), flags.begin());
    const LayoutSpec layout{{2, 3}, {3, 1}, 0};
    FATP_ASSERT_TRUE(verifyReference(flags, layout, flags.data(), 2, {1}, false),
                     "Bool reductions retain numeric rules");
    Tensor<double> truth({2, 3});
    const std::array<double, 6> special{0.0, -0.0, 0.0, std::numeric_limits<double>::quiet_NaN(),
                                      std::numeric_limits<double>::infinity(), -2.0};
    std::copy(special.begin(), special.end(), truth.begin());
    FATP_ASSERT_TRUE(checkResult(all(truth, {1}), {2}, std::vector<bool>{false, true}),
                     "Only signed zeros convert to false; NaNs and nonzero numbers convert to true");
    FATP_ASSERT_TRUE(checkResult(any(truth, {1}), {2}, std::vector<bool>{false, true}), "Any uses the same conversion");
    Tensor<int> empty({2, 0});
    FATP_ASSERT_TRUE(checkResult(all(empty, {1}), {2}, std::vector<bool>{true, true}), "Empty all uses true");
    FATP_ASSERT_TRUE(checkResult(any(empty, {1}), {2}, std::vector<bool>{false, false}), "Empty any uses false");
    Tensor<bool> scalar({}, false);
    FATP_ASSERT_FALSE(all(scalar)(), "Rank-zero false is one false value, not an empty domain");
    FATP_ASSERT_FALSE(any(scalar)(), "Rank-zero any preserves false");
    FATP_ASSERT_EQ(scalarValue(product(scalar)), std::size_t{0}, "Bool product uses the counting integer type");
    return true;
}

FATP_TEST_CASE(axis_validation_and_source_preservation)
{
    Tensor<int> source({2, 3}, 1);
    const std::vector<int> before(source.begin(), source.end());
    FATP_ASSERT_TRUE(forEachReduction([&](const auto& operation) {
        for (const auto& axes : {std::vector<TensorAxis>{0, 0}, {1, -1}})
        {
            FATP_ASSERT_THROWS(operation(source, axes), std::invalid_argument,
                               "Repeated axes are rejected after negative-axis normalization");
        }
        for (const auto axis : {TensorAxis{2}, TensorAxis{-3}, std::numeric_limits<TensorAxis>::lowest()})
        {
            FATP_ASSERT_THROWS(operation(source, std::vector<TensorAxis>{axis}), std::out_of_range,
                               "Invalid axes fail without signed overflow");
        }
        Tensor<int> scalar({}, 1);
        FATP_ASSERT_THROWS(operation(scalar, std::vector<TensorAxis>{0}), std::out_of_range,
                           "A scalar has no explicit axis zero");
        Tensor<int> empty({0, 3});
        FATP_ASSERT_THROWS(operation(empty, std::vector<TensorAxis>{3}), std::out_of_range,
                           "Even empty sources must validate requested axes");
        return true;
    }), "All reduction entry points share axis-validation rules");
    FATP_ASSERT_TRUE(std::vector<int>(source.begin(), source.end()) == before,
                     "Validation failures leave source values unchanged");
    return true;
}

FATP_TEST_CASE(result_allocation_and_failure_contract)
{
    AllocationCounts ownerCounts;
    AllocationCounts explicitCounts;
    {
        Tensor<int, SocccAllocator<int>> source(std::allocator_arg, SocccAllocator<int>(7, &ownerCounts),
                                               DynamicExtents{2, 3}, 1);
        const auto before = std::vector<int>(source.begin(), source.end());
        FATP_ASSERT_TRUE(forEachReduction([&](const auto& operation) {
            const auto baseline = ownerCounts.allocations;
            {
                auto result = operation(source, std::vector<TensorAxis>{1});
                using result_type = typename decltype(result)::value_type;
                FATP_ASSERT_EQ(result.get_allocator().identity(), 107, "Default owner result uses rebound SOCCC");
                FATP_ASSERT_EQ(ownerCounts.allocations, baseline + 1, "A nonempty result allocates one element buffer");
                FATP_ASSERT_NE(static_cast<const void*>(result.data()), static_cast<const void*>(source.data()),
                               "A reduction result must own independent element storage");
                result[0] = result_type{};
                const auto borrowed = operation(source.asConstView());
                const auto shared = operation(std::as_const(source).asSharedView());
                static_assert(std::same_as<typename decltype(borrowed)::allocator_type, TensorAllocator<result_type>>);
                static_assert(std::same_as<typename decltype(shared)::allocator_type, TensorAllocator<result_type>>);
                const auto explicitBaseline = explicitCounts.allocations;
                {
                    const auto chosen = operation(source, SocccAllocator<result_type>(41, &explicitCounts));
                    FATP_ASSERT_EQ(chosen.get_allocator().identity(), 41, "Explicit allocator must not apply SOCCC");
                    FATP_ASSERT_EQ(explicitCounts.allocations, explicitBaseline + 1,
                                   "The explicit result allocator owns its one element buffer");
                }
                FATP_ASSERT_EQ(explicitCounts.liveElements, std::size_t{0}, "Result destruction releases its buffer");
                explicitCounts.fail = true;
                FATP_ASSERT_THROWS(operation(source, SocccAllocator<result_type>(41, &explicitCounts)), std::bad_alloc,
                                   "Result allocation failure must propagate without mutating the source");
                explicitCounts.fail = false;
                FATP_ASSERT_THROWS(operation(source, SocccAllocator<result_type>(41, &explicitCounts),
                                             std::vector<TensorAxis>{0, 0}), std::invalid_argument,
                                   "Axis validation precedes element allocation");
                FATP_ASSERT_EQ(explicitCounts.allocations, explicitBaseline + 1,
                               "Invalid axes and failed allocation publish no result buffer");
                Tensor<int> empty({0, 3});
                const auto emptyResult = operation(empty, SocccAllocator<result_type>(41, &explicitCounts),
                                                   std::vector<TensorAxis>{1});
                FATP_ASSERT_TRUE(emptyResult.empty(), "Empty output remains empty for every operation");
                FATP_ASSERT_EQ(emptyResult.get_allocator().identity(), 41,
                               "Empty outputs preserve explicit allocator identity, including mean's early return");
                FATP_ASSERT_EQ(explicitCounts.allocations, explicitBaseline + 1, "Empty results allocate no elements");
                Tensor<int, SocccAllocator<int>> emptyOwner(std::allocator_arg, SocccAllocator<int>(7, &ownerCounts),
                                                           DynamicExtents{0, 3});
                const auto emptyDefault = operation(emptyOwner, std::vector<TensorAxis>{1});
                FATP_ASSERT_EQ(emptyDefault.get_allocator().identity(), 107,
                               "Default empty results still use rebound owner SOCCC");
            }
            FATP_ASSERT_EQ(ownerCounts.liveElements, source.size(), "Only the source element buffer survives");
            FATP_ASSERT_TRUE(std::vector<int>(source.begin(), source.end()) == before,
                             "Independent result writes and failed calls never change the input");
            return true;
        }), "All nine reductions share result ownership and allocator contracts");
    }
    FATP_ASSERT_EQ(ownerCounts.liveElements, std::size_t{0}, "Owner and every result release their elements");
    FATP_ASSERT_EQ(ownerCounts.allocations, ownerCounts.deallocations, "Owner allocator has no outstanding buffers");
    FATP_ASSERT_EQ(explicitCounts.allocations, explicitCounts.deallocations, "Explicit allocator has no buffer leaks");

    AllocationCounts failureCounts;
    Tensor<std::int64_t> overflow({3});
    overflow[0] = std::numeric_limits<std::int64_t>::max();
    overflow[1] = 1;
    overflow[2] = -1;
    FATP_ASSERT_THROWS(sum(overflow, SocccAllocator<std::int64_t>(1, &failureCounts)), std::overflow_error,
                       "Arithmetic failure must clean up an already allocated result");
    FATP_ASSERT_EQ(failureCounts.allocations, std::size_t{1}, "The arithmetic failure occurs after result allocation");
    FATP_ASSERT_EQ(failureCounts.deallocations, std::size_t{1}, "Arithmetic failure destroys the unpublished result");
    return true;
}

FATP_TEST_CASE(shared_retention_and_borrowed_invalidation)
{
    const bool passed = forEachReduction([&](const auto& operation) {
        for (const bool empty : {false, true})
        {
            SharedTensorView<const int> retained;
            TensorView<const int> borrowed;
            std::vector<typename decltype(operation(std::declval<const Tensor<int>&>()))::value_type> expected;
            const std::vector<TensorAxis> axes{1};
            {
                const Tensor<int> owner(DynamicExtents{empty ? std::size_t{0} : std::size_t{2}, 3}, 1);
                retained = owner.asSharedView().sliceView({All, Slice{{}, {}, -1}});
                borrowed = owner.asConstView().sliceView({All, Slice{{}, {}, -1}});
                const auto result = operation(retained, axes);
                expected.assign(result.begin(), result.end());
            }
            FATP_ASSERT_TRUE(checkResult(operation(retained, axes),
                                        {empty ? std::size_t{0} : std::size_t{2}}, expected),
                             "A shared transformed source remains readable after owner destruction");
#ifndef NDEBUG
            FATP_ASSERT_THROWS(operation(borrowed, axes), std::runtime_error,
                               "Expired borrowed sources fail before reads, including empty outputs");
#endif
        }
        return true;
    });
    FATP_ASSERT_TRUE(passed, "Every reduction preserves shared ownership and validates borrowed lifetimes");
    return true;
}

} // namespace fat_p::testing::tensor_reductions

namespace fat_p::testing
{

bool test_TensorReductions()
{
    FATP_PRINT_HEADER(TENSOR REDUCTIONS)
    TestRunner runner;
    FATP_RUN_TEST_NS(runner, tensor_reductions, coordinate_reference_anchors);
    FATP_RUN_TEST_NS(runner, tensor_reductions, exhaustive_small_reduction_coordinates);
    FATP_RUN_TEST_NS(runner, tensor_reductions, randomized_signed_layout_reductions);
    FATP_RUN_TEST_NS(runner, tensor_reductions, all_and_axis_reductions);
    FATP_RUN_TEST_NS(runner, tensor_reductions, strided_scalar_and_argument_reductions);
    FATP_RUN_TEST_NS(runner, tensor_reductions, empty_initial_nan_and_ties);
    FATP_RUN_TEST_NS(runner, tensor_reductions, multiple_axes_bool_and_overflow);
    FATP_RUN_TEST_NS(runner, tensor_reductions, empty_output_does_not_count_unreachable_domains);
    FATP_RUN_TEST_NS(runner, tensor_reductions, integral_accumulator_boundaries);
    FATP_RUN_TEST_NS(runner, tensor_reductions, floating_order_nan_infinity_and_signed_zero);
    FATP_RUN_TEST_NS(runner, tensor_reductions, boolean_truth_and_identities);
    FATP_RUN_TEST_NS(runner, tensor_reductions, axis_validation_and_source_preservation);
    FATP_RUN_TEST_NS(runner, tensor_reductions, result_allocation_and_failure_contract);
    FATP_RUN_TEST_NS(runner, tensor_reductions, shared_retention_and_borrowed_invalidation);
    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_TensorReductions() ? 0 : 1;
}
#endif
