/**
 * @file test_Tensor.cpp
 * @brief Owner, allocator, and explicit materialization tests for Tensor.
 */

/*
FATP_META:
  meta_version: 1
  component: Tensor
  file_role: test
  path: components/Tensor/tests/test_Tensor.cpp
  namespace: fat_p::testing::tensor
  layer: Testing
  summary: "Owner value semantics, allocator behavior, access, and materialization tests."
  api_stability: in_work
  related:
    docs:
      - components/Tensor/docs/Design Note - Tensor Semantic Contract.md
      - components/Tensor/docs/Design Note - Tensor Architecture Additions Plan.md
    headers:
      - include/fat_p/Tensor.h
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
#include "Tensor.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace fat_p::testing::tensor
{

template <typename U>
concept MutableLvalueViewable = requires(U& owner) { owner.asView(); };

template <typename U>
concept MutableRvalueViewable = requires(U&& owner) { std::move(owner).asView(); };

template <typename U>
concept ConstMutableViewable = requires(const U& owner) { owner.asView(); };

template <typename U>
concept ConstViewable = requires(const U& owner) { owner.asConstView(); };

static_assert(MutableLvalueViewable<Tensor<int>>);
static_assert(!MutableRvalueViewable<Tensor<int>>);
static_assert(!ConstMutableViewable<Tensor<int>>);
static_assert(ConstViewable<Tensor<int>>);
static_assert(std::same_as<decltype(std::declval<const Tensor<int>&>().data()), const int*>);
static_assert(std::same_as<decltype(std::declval<const Tensor<int>&>().asConstView()), TensorView<const int>>);
static_assert(ReadableTensor<Tensor<int>>);
static_assert(WritableTensor<Tensor<int>>);
static_assert(ReadableTensor<TensorView<const int>>);
static_assert(!WritableTensor<TensorView<const int>>);
static_assert(TensorDTypeElement<std::int8_t>);
static_assert(TensorDTypeElement<std::uint8_t>);
static_assert(TensorDTypeElement<std::int16_t>);
static_assert(TensorDTypeElement<std::uint16_t>);
static_assert(TensorDTypeElement<std::int32_t>);
static_assert(TensorDTypeElement<std::uint32_t>);
static_assert(TensorDTypeElement<std::int64_t>);
static_assert(TensorDTypeElement<std::uint64_t>);
static_assert(TensorDTypeElement<float>);
static_assert(TensorDTypeElement<double>);
static_assert(TensorDTypeElement<const std::int32_t&>);
static_assert(!TensorDTypeElement<bool>);
static_assert(!TensorDTypeElement<char>);
static_assert(!TensorDTypeElement<long double>);
static_assert(tensorDTypeOf<const std::int32_t&>() == TensorDType::Int32);
static_assert(tensorDTypeName<double>() == std::string_view{"float64"});

FATP_TEST_CASE(canonical_dtype_vocabulary)
{
    constexpr std::array expectedDTypes{TensorDType::Int8,
                                        TensorDType::Uint8,
                                        TensorDType::Int16,
                                        TensorDType::Uint16,
                                        TensorDType::Int32,
                                        TensorDType::Uint32,
                                        TensorDType::Int64,
                                        TensorDType::Uint64,
                                        TensorDType::Float32,
                                        TensorDType::Float64};
    constexpr std::array<std::string_view, 10>
        expectedNames{"int8", "uint8", "int16", "uint16", "int32", "uint32", "int64", "uint64", "float32", "float64"};
    constexpr std::array<std::size_t, 10> expectedWidths{8, 8, 16, 16, 32, 32, 64, 64, 32, 64};
    const auto& descriptors = tensorDTypeDescriptors();

    FATP_ASSERT_EQ(descriptors.size(), expectedDTypes.size(), "Canonical dtype table should have ten entries");
    for (std::size_t index = 0; index < descriptors.size(); ++index)
    {
        const auto& descriptor = descriptors[index];
        FATP_ASSERT_TRUE(descriptor.dtype == expectedDTypes[index], "Canonical dtype order should remain explicit");
        FATP_ASSERT_TRUE(descriptor.name == expectedNames[index],
                         "Canonical dtype name should be compiler-independent");
        FATP_ASSERT_EQ(descriptor.bitWidth, expectedWidths[index], "Canonical dtype width should match its name");
        FATP_ASSERT_EQ(static_cast<std::uint8_t>(descriptor.dtype),
                       static_cast<std::uint8_t>(index + 1),
                       "Canonical dtype identifiers should remain explicit and ordered");

        const auto decoded = tensorDTypeFromId(static_cast<std::uint8_t>(index + 1));
        FATP_ASSERT_TRUE(decoded.has_value() && *decoded == descriptor.dtype,
                         "Every canonical dtype identifier should decode to its descriptor");
        FATP_ASSERT_TRUE(tensorDTypeDescriptor(descriptor.dtype) == &descriptor,
                         "Runtime descriptor lookup should return the canonical table entry");
        FATP_ASSERT_TRUE(tensorDTypeName(descriptor.dtype) == descriptor.name,
                         "Runtime dtype name should come from the canonical descriptor");
        FATP_ASSERT_EQ(tensorDTypeBitWidth(descriptor.dtype),
                       descriptor.bitWidth,
                       "Runtime dtype width should come from the canonical descriptor");
    }

    for (const std::uint8_t invalidId : {std::uint8_t{0}, std::uint8_t{11}, std::uint8_t{255}})
    {
        const auto invalidDType = static_cast<TensorDType>(invalidId);
        FATP_ASSERT_FALSE(tensorDTypeFromId(invalidId).has_value(), "Unknown dtype identifiers should not decode");
        FATP_ASSERT_TRUE(tensorDTypeDescriptor(invalidDType) == nullptr,
                         "Unknown dtype enum values should have no descriptor");
        FATP_ASSERT_TRUE(tensorDTypeName(invalidDType) == std::string_view{"unknown"},
                         "Unknown dtype enum values should have a stable diagnostic name");
        FATP_ASSERT_EQ(tensorDTypeBitWidth(invalidDType),
                       std::size_t{0},
                       "Unknown dtype enum values should have no logical width");
    }
    return true;
}

template <typename T, bool CopyPropagation, bool MovePropagation, bool SwapPropagation>
class TestAllocator
{
public:
    using value_type = T;
    using propagate_on_container_copy_assignment = std::bool_constant<CopyPropagation>;
    using propagate_on_container_move_assignment = std::bool_constant<MovePropagation>;
    using propagate_on_container_swap = std::bool_constant<SwapPropagation>;
    using is_always_equal = std::false_type;

    TestAllocator() = default;
    explicit TestAllocator(int id) : mId(id) {}

    template <typename U>
    TestAllocator(const TestAllocator<U, CopyPropagation, MovePropagation, SwapPropagation>& other)
        : mId(other.id())
    {
    }

    [[nodiscard]] T* allocate(std::size_t count)
    {
        lastAllocationId = mId;
        return std::allocator<T>{}.allocate(count);
    }

    void deallocate(T* storage, std::size_t count) noexcept
    {
        lastDeallocationId = mId;
        std::allocator<T>{}.deallocate(storage, count);
    }

    [[nodiscard]] int id() const noexcept { return mId; }

    [[nodiscard]] TestAllocator select_on_container_copy_construction() const
    {
        return TestAllocator(mId + 100);
    }

    template <typename U>
    struct rebind
    {
        using other = TestAllocator<U, CopyPropagation, MovePropagation, SwapPropagation>;
    };

    template <typename U>
    [[nodiscard]] bool operator==(
        const TestAllocator<U, CopyPropagation, MovePropagation, SwapPropagation>& other) const noexcept
    {
        return mId == other.id();
    }

    template <typename U>
    [[nodiscard]] bool operator!=(
        const TestAllocator<U, CopyPropagation, MovePropagation, SwapPropagation>& other) const noexcept
    {
        return !(*this == other);
    }

    static inline int lastAllocationId = -1;
    static inline int lastDeallocationId = -1;

private:
    template <typename, bool, bool, bool>
    friend class TestAllocator;
    int mId = 0;
};

struct ThrowingValue
{
    ThrowingValue() { ++liveCount; }
    ThrowingValue(const ThrowingValue& other) : value(other.value)
    {
        if (copiesBeforeThrow == 0)
        {
            throw std::runtime_error("injected Tensor element copy failure");
        }
        if (copiesBeforeThrow > 0)
        {
            --copiesBeforeThrow;
        }
        ++liveCount;
    }
    ThrowingValue(ThrowingValue&& other) noexcept : value(other.value) { ++liveCount; }
    ThrowingValue& operator=(const ThrowingValue&) = default;
    ThrowingValue& operator=(ThrowingValue&&) noexcept = default;
    ~ThrowingValue() { --liveCount; }

    int value = 0;
    static inline int liveCount = 0;
    static inline int copiesBeforeThrow = -1;
};

FATP_TEST_CASE(owner_scalar_empty_and_access)
{
    Tensor<int> defaultEmpty;
    FATP_ASSERT_EQ(defaultEmpty.rank(), std::size_t{1}, "Default Tensor should be canonical empty rank one");
    FATP_ASSERT_TRUE(defaultEmpty.extents() == DynamicExtents{0}, "Default Tensor should have extent zero");
    FATP_ASSERT_TRUE(defaultEmpty.empty(), "Default Tensor should contain no values");

    Tensor<int> scalar({}, 7);
    FATP_ASSERT_EQ(scalar.rank(), std::size_t{0}, "Empty extent list should create a rank-zero scalar");
    FATP_ASSERT_EQ(scalar.size(), std::size_t{1}, "Rank-zero scalar should contain one value");
    FATP_ASSERT_EQ(scalar(), 7, "Zero-index scalar access should read its value");
    scalar() = 9;
    FATP_ASSERT_EQ(scalar.atLinear(0), 9, "Scalar logical indexing should be writable");
    FATP_ASSERT_THROWS(scalar(0), std::invalid_argument, "Scalar access should enforce exact rank");

    Tensor<int> matrix({2, 3});
    std::iota(matrix.begin(), matrix.end(), 1);
    FATP_ASSERT_EQ(matrix(1, 2), 6, "Multidimensional access should use canonical row-major layout");
    FATP_ASSERT_EQ(matrix.atLinear(3), 4, "Linear access should follow logical row-major order");
    FATP_ASSERT_THROWS(matrix.atLinear(6), std::out_of_range,
                       "Checked linear access should reject an index equal to size");
    FATP_ASSERT_THROWS(matrix(0), std::invalid_argument, "Owner access should enforce exact rank");
    FATP_ASSERT_THROWS(matrix(2, 0), std::out_of_range, "Owner access should reject out-of-range indices");
    return true;
}

FATP_TEST_CASE(owner_copy_move_fill_and_hash)
{
    Tensor<std::string> source({2, 2}, std::string("value"));
    Tensor<std::string> copy(source);
    copy(0, 0) = "changed";
    FATP_ASSERT_EQ(source(0, 0), std::string("value"), "Owner copy should deep-copy values");

    Tensor<std::string> assigned({1}, std::string("old"));
    assigned = source;
    FATP_ASSERT_TRUE(assigned == source, "Copy assignment should preserve shape and values");
    assigned.fill("filled");
    FATP_ASSERT_TRUE(std::all_of(assigned.begin(), assigned.end(), [](const auto& value) {
                         return value == "filled";
                     }),
                     "fill should visit every owner element");

    const auto sourceHash = std::hash<Tensor<std::string>>{}(source);
    FATP_ASSERT_EQ(sourceHash, std::hash<Tensor<std::string>>{}(source.clone()),
                   "Equal owner values should hash equally");

    Tensor<std::string> moved(std::move(source));
    FATP_ASSERT_EQ(moved.size(), std::size_t{4}, "Move construction should transfer values");
    FATP_ASSERT_TRUE(source.extents() == DynamicExtents{0} && source.empty(),
                     "Moved-from owner should become canonical empty");
    Tensor<std::string> moveAssigned({1}, std::string("discard"));
    moveAssigned = std::move(moved);
    FATP_ASSERT_EQ(moveAssigned(1, 1), std::string("value"), "Move assignment should transfer storage");
    FATP_ASSERT_TRUE(moved.extents() == DynamicExtents{0} && moved.empty(),
                     "Move-assigned source should become canonical empty");
    return true;
}

FATP_TEST_CASE(owner_allocator_semantics)
{
    using NonPropagating = TestAllocator<int, false, false, false>;
    using Propagating = TestAllocator<int, true, true, true>;

    Tensor<int, NonPropagating> source(std::allocator_arg, NonPropagating(1), DynamicExtents{2}, 7);
    Tensor<int, NonPropagating> selected(source);
    FATP_ASSERT_EQ(selected.get_allocator().id(), 101, "Copy construction should use SOCCC");
    FATP_ASSERT_EQ(NonPropagating::lastAllocationId, 101, "SOCCC allocator should own copied storage");
    auto freeClone = clone(source);
    static_assert(std::same_as<decltype(freeClone), Tensor<int, NonPropagating>>);
    FATP_ASSERT_EQ(freeClone.get_allocator().id(), 101,
                   "Free clone of an owner should preserve its allocator type and apply SOCCC");
    FATP_ASSERT_EQ(freeClone[1], 7, "Allocator-aware free clone should preserve owner values");

    Tensor<int, NonPropagating> destination(std::allocator_arg, NonPropagating(2), DynamicExtents{1}, 0);
    destination = source;
    FATP_ASSERT_EQ(destination.get_allocator().id(), 2, "Non-propagating copy assignment should retain allocator");
    FATP_ASSERT_EQ(destination[1], 7, "Copy assignment should preserve values");

    Tensor<int, NonPropagating> moveSource(std::allocator_arg, NonPropagating(3), DynamicExtents{2}, 11);
    destination = std::move(moveSource);
    FATP_ASSERT_EQ(destination.get_allocator().id(), 2, "Unequal non-propagating move should retain allocator");
    FATP_ASSERT_EQ(destination[0], 11, "Unequal allocator move should materialize values");
    FATP_ASSERT_TRUE(moveSource.empty(), "Materialized move source should become canonical empty");

    Tensor<int, Propagating> propagatingSource(std::allocator_arg, Propagating(9), DynamicExtents{2}, 4);
    Tensor<int, Propagating> propagatingDestination(std::allocator_arg, Propagating(8), DynamicExtents{1}, 0);
    propagatingDestination = propagatingSource;
    FATP_ASSERT_EQ(propagatingDestination.get_allocator().id(), 9,
                   "Propagating copy assignment should adopt source allocator");
    propagatingDestination = std::move(propagatingSource);
    FATP_ASSERT_EQ(propagatingDestination.get_allocator().id(), 9,
                   "Propagating move assignment should adopt source allocator");

    Tensor<int, NonPropagating> left(std::allocator_arg, NonPropagating(20), DynamicExtents{1}, 1);
    Tensor<int, NonPropagating> right(std::allocator_arg, NonPropagating(21), DynamicExtents{1}, 2);
    FATP_ASSERT_THROWS(left.swap(right), std::invalid_argument,
                       "Unequal non-propagating member swap should reject before mutation");
    FATP_ASSERT_EQ(left[0], 1, "Rejected swap should leave the left owner unchanged");
    FATP_ASSERT_EQ(right[0], 2, "Rejected swap should leave the right owner unchanged");
    return true;
}

FATP_TEST_CASE(owner_adoption_and_external_borrowing)
{
    using AdoptionAllocator = TestAllocator<int, false, false, false>;
    int rawValues[]{1, 2, 3, 4};
    const TensorLayout matrixLayout(4, 0, DynamicExtents{2, 2}, TensorStrides{2, 1});
    auto borrowed = TensorView<int>::borrow(rawValues, matrixLayout);
    borrowed(1, 0) = 9;
    FATP_ASSERT_EQ(rawValues[2], 9, "Explicit external borrow should mutate caller-owned storage");

    bool deleted = false;
    {
        auto* adoptedValues = new int[3]{4, 5, 6};
        auto adopted = Tensor<int>::adopt(adoptedValues, DynamicExtents{3}, [&deleted](int* pointer) {
            deleted = true;
            delete[] pointer;
        });
        FATP_ASSERT_EQ(adopted[2], 6, "Adopted Tensor should expose external values");
    }
    FATP_ASSERT_TRUE(deleted, "Adopted storage should invoke its explicit deleter exactly at release");

    bool explicitlyDeleted = false;
    {
        auto* adoptedValues = new int[2]{8, 9};
        auto adopted = Tensor<int, AdoptionAllocator>::adopt(
            adoptedValues, DynamicExtents{2},
            [&explicitlyDeleted](int* pointer) {
                explicitlyDeleted = true;
                delete[] pointer;
            },
            AdoptionAllocator(42));
        FATP_ASSERT_EQ(adopted.get_allocator().id(), 42,
                       "Adoption should accept the allocator instance used by future owner operations");
        FATP_ASSERT_EQ(adopted[1], 9, "Allocator-aware adoption should retain external values");
    }
    FATP_ASSERT_TRUE(explicitlyDeleted, "Allocator-aware adoption should retain the supplied storage deleter");
    return true;
}

FATP_TEST_CASE(view_transforms_constness_and_clone)
{
    Tensor<int> matrix({3, 3});
    std::iota(matrix.begin(), matrix.end(), 1);

    auto column = matrix.columnView(1);
    FATP_ASSERT_TRUE(column.extents() == DynamicExtents({3, 1}), "Column view should retain rank-two extents");
    FATP_ASSERT_EQ(column[0], 2, "Column view should begin at the selected logical column");
    FATP_ASSERT_EQ(column[1], 5, "Column iteration should honor its row stride");
    column[2] = 70;
    FATP_ASSERT_EQ(matrix(2, 1), 70, "Mutable borrowed view should alias its owner");

    const auto transposed = std::as_const(matrix).transposeView();
    FATP_ASSERT_EQ(transposed(1, 2), 70, "Const transpose view should map signed layout metadata");
    FATP_ASSERT_TRUE(transposed.layout().kind() == TensorLayoutKind::InjectiveStrided,
                     "Transpose should be a non-contiguous injective mapping");

    auto compactColumn = clone(column);
    FATP_ASSERT_TRUE(compactColumn.layout().isContiguous(), "clone should create canonical contiguous ownership");
    FATP_ASSERT_TRUE(compactColumn.extents() == column.extents(), "clone should preserve logical extents");
    compactColumn[0] = -1;
    FATP_ASSERT_EQ(matrix(0, 1), 2, "Materialized clone should not alias its input view");

    const Tensor<int> scalar({}, 5);
    auto broadcast = scalar.broadcastView(DynamicExtents{2, 3});
    static_assert(std::same_as<decltype(broadcast), TensorView<const int>>);
    FATP_ASSERT_TRUE(broadcast.layout().isBroadcast(), "Expanded scalar should create a broadcast layout");
    FATP_ASSERT_EQ(std::accumulate(broadcast.begin(), broadcast.end(), 0), 30,
                   "Broadcast iterator should visit every logical value");
    auto materialized = clone(broadcast);
    FATP_ASSERT_EQ(materialized.size(), std::size_t{6}, "Broadcast clone should allocate every logical value");
    FATP_ASSERT_EQ(materialized(1, 2), 5, "Broadcast clone should preserve values");
    return true;
}

FATP_TEST_CASE(borrowed_and_shared_lifetime)
{
    TensorView<int> borrowed;
    SharedTensorView<int> shared;
    {
        Tensor<int> owner({2}, 8);
        borrowed = owner.asView();
        shared = owner.asSharedView();
        FATP_ASSERT_EQ(borrowed[0], 8, "Borrowed view should be valid while its owner lives");
    }
    FATP_ASSERT_EQ(shared[1], 8, "Shared view should retain storage after owner destruction");
#ifndef NDEBUG
    FATP_ASSERT_THROWS(borrowed[0], std::runtime_error,
                       "Debug borrowed view should diagnose access after owner destruction");
#endif

    Tensor<int> movable({2}, 3);
    auto invalidatedByMove = movable.asView();
    Tensor<int> destination(std::move(movable));
    FATP_ASSERT_EQ(destination[0], 3, "Moved owner should retain values");
#ifndef NDEBUG
    FATP_ASSERT_THROWS(invalidatedByMove[0], std::runtime_error,
                       "Debug borrowed view should diagnose owner move invalidation");
#endif

    SharedTensorView<int> sharedAcrossMove;
    Tensor<int> sharedOwner({2}, 12);
    sharedAcrossMove = sharedOwner.asSharedView();
    Tensor<int> movedSharedOwner(std::move(sharedOwner));
    FATP_ASSERT_EQ(sharedAcrossMove[1], 12, "Explicit shared view should survive owner movement");
    return true;
}

FATP_TEST_CASE(shared_alias_preservation_across_allocator_move)
{
    using StringAllocator = TestAllocator<std::string, false, false, false>;
    Tensor<std::string, StringAllocator> source(std::allocator_arg, StringAllocator(1), DynamicExtents{2},
                                                std::string("kept"));
    auto shared = source.asSharedView();
    Tensor<std::string, StringAllocator> destination(std::allocator_arg, StringAllocator(2), DynamicExtents{1},
                                                     std::string("old"));
    destination = std::move(source);
    FATP_ASSERT_EQ(destination[0], std::string("kept"), "Unequal allocator move should materialize values");
    FATP_ASSERT_EQ(shared[0], std::string("kept"), "Materialized move should preserve live shared aliases");
    return true;
}

FATP_TEST_CASE(invalidation_matrix_and_copy_exception_safety)
{
#ifndef NDEBUG
    Tensor<int> copyAssigned({2}, 1);
    auto copyAssignedView = copyAssigned.asView();
    const Tensor<int> replacement({2}, 2);
    copyAssigned = replacement;
    FATP_ASSERT_THROWS(copyAssignedView[0], std::runtime_error,
                       "Copy assignment should invalidate destination borrowed views");

    Tensor<int> moveSource({2}, 3);
    Tensor<int> moveDestination({2}, 4);
    auto moveSourceView = moveSource.asView();
    auto oldDestinationView = moveDestination.asView();
    moveDestination = std::move(moveSource);
    FATP_ASSERT_THROWS(moveSourceView[0], std::runtime_error,
                       "Move assignment should invalidate source borrowed views");
    FATP_ASSERT_THROWS(oldDestinationView[0], std::runtime_error,
                       "Move assignment should invalidate destination borrowed views");

    Tensor<int> left({1}, 5);
    Tensor<int> right({1}, 6);
    auto leftView = left.asView();
    auto rightView = right.asView();
    left.swap(right);
    FATP_ASSERT_THROWS(leftView[0], std::runtime_error, "Swap should invalidate left borrowed views");
    FATP_ASSERT_THROWS(rightView[0], std::runtime_error, "Swap should invalidate right borrowed views");
#endif

    FATP_ASSERT_EQ(ThrowingValue::liveCount, 0, "Throwing element test should start without live objects");
    {
        Tensor<ThrowingValue> source({3});
        source[0].value = 10;
        source[1].value = 20;
        source[2].value = 30;
        ThrowingValue::copiesBeforeThrow = 1;
        FATP_ASSERT_THROWS(([&]() { Tensor<ThrowingValue> failedCopy(source); }()), std::runtime_error,
                           "A throwing element copy should abort owner construction");
        ThrowingValue::copiesBeforeThrow = -1;
        FATP_ASSERT_EQ(ThrowingValue::liveCount, 3,
                       "Failed copy construction should destroy every partially constructed element");
        FATP_ASSERT_EQ(source[2].value, 30, "Failed copy construction should leave its source unchanged");
    }
    FATP_ASSERT_EQ(ThrowingValue::liveCount, 0, "All throwing-test elements should be released at scope exit");
    return true;
}

} // namespace fat_p::testing::tensor

namespace fat_p::testing
{

bool test_Tensor()
{
    FATP_PRINT_HEADER(TENSOR OWNER AND VIEWS)
    TestRunner runner;
    FATP_RUN_TEST_NS(runner, tensor, canonical_dtype_vocabulary);
    FATP_RUN_TEST_NS(runner, tensor, owner_scalar_empty_and_access);
    FATP_RUN_TEST_NS(runner, tensor, owner_copy_move_fill_and_hash);
    FATP_RUN_TEST_NS(runner, tensor, owner_allocator_semantics);
    FATP_RUN_TEST_NS(runner, tensor, owner_adoption_and_external_borrowing);
    FATP_RUN_TEST_NS(runner, tensor, view_transforms_constness_and_clone);
    FATP_RUN_TEST_NS(runner, tensor, borrowed_and_shared_lifetime);
    FATP_RUN_TEST_NS(runner, tensor, shared_alias_preservation_across_allocator_move);
    FATP_RUN_TEST_NS(runner, tensor, invalidation_matrix_and_copy_exception_safety);
    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_Tensor() ? 0 : 1;
}
#endif
