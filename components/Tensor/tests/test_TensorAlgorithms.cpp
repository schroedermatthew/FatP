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
      - components/Tensor/docs/Design Note - Tensor Semantic Contract.md
      - components/Tensor/docs/User Manual - Tensor.md
    headers:
      - include/fat_p/TensorAlgorithms.h
      - include/fat_p/tensor/TensorIterationPlan.h
      - include/fat_p/tensor/TensorKernels.h
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
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <limits>
#include <memory>
#include <new>
#include <numeric>
#include <random>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

#include "FatPTest.h"
#include "TensorAlgorithms.h"
#include "TensorTestSupport.h"
#include "tensor/TensorIterationPlan.h"

#if defined(ENABLE_TEST_APPLICATION) && !defined(FATP_TENSOR_DISABLE_ALLOCATION_PROBE) && \
    (!defined(_MSC_VER) || _ITERATOR_DEBUG_LEVEL == 0)
// Standalone-only replacement: aggregate runners may already replace global new (IdGenerator).
// This executable must not link another global-new replacement such as the IdGenerator test TU.
// MSVC checked iterators allocate proxies inside noexcept vector moves; injected failure terminates
// in that standard-library path instead of throwing. Keep checked-iterator builds intact and run this
// sweep in the other standalone configurations. The disable switch also permits unmodified-new ASan.
// The serial Tensor metadata paths and explicit std::allocator<int> scratch use ordinary new.
// Over-aligned allocation is deliberately outside this probe. Injection is disarmed for all other tests.
namespace fat_p::testing::tensor_algorithms::allocation_probe
{
thread_local std::ptrdiff_t failIndex = -1;

void* allocate(std::size_t bytes)
{
    if (failIndex >= 0 && failIndex-- == 0)
    {
        failIndex = -1;
        throw std::bad_alloc();
    }
    if (void* storage = std::malloc(bytes == 0 ? 1 : bytes))
    {
        return storage;
    }
    throw std::bad_alloc();
}

class ScopedFailure
{
public:
    explicit ScopedFailure(std::ptrdiff_t index) noexcept { failIndex = index; }
    ~ScopedFailure() { failIndex = -1; }
    ScopedFailure(const ScopedFailure&) = delete;
    ScopedFailure& operator=(const ScopedFailure&) = delete;
};
} // namespace fat_p::testing::tensor_algorithms::allocation_probe

void* operator new(std::size_t bytes) { return fat_p::testing::tensor_algorithms::allocation_probe::allocate(bytes); }
void* operator new[](std::size_t bytes) { return fat_p::testing::tensor_algorithms::allocation_probe::allocate(bytes); }
void operator delete(void* storage) noexcept { std::free(storage); }
void operator delete[](void* storage) noexcept { std::free(storage); }
void operator delete(void* storage, std::size_t) noexcept { std::free(storage); }
void operator delete[](void* storage, std::size_t) noexcept { std::free(storage); }
#endif

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

struct CopyAllocationState
{
    std::size_t attempts = 0;
    std::size_t allocations = 0;
    std::size_t deallocations = 0;
    std::size_t lastCount = 0;
    int lastId = -1;
    bool fail = false;
};

template <typename T>
class CopyAllocator
{
public:
    using value_type = T;

    CopyAllocator() = default;
    CopyAllocator(CopyAllocationState& state, int id)
        : mState(&state)
        , mId(id)
    {
    }

    template <typename U>
    CopyAllocator(const CopyAllocator<U>& other)
        : mState(other.state())
        , mId(other.id())
    {
    }

    [[nodiscard]] T* allocate(std::size_t count)
    {
        if (mState != nullptr)
        {
            ++mState->attempts;
            mState->lastCount = count;
            mState->lastId = mId;
            if (mState->fail)
            {
                throw std::bad_alloc();
            }
        }
        auto* storage = std::allocator<T>{}.allocate(count);
        if (mState != nullptr)
        {
            ++mState->allocations;
        }
        return storage;
    }

    void deallocate(T* storage, std::size_t count) noexcept
    {
        if (mState != nullptr)
        {
            ++mState->deallocations;
        }
        std::allocator<T>{}.deallocate(storage, count);
    }

    [[nodiscard]] CopyAllocationState* state() const noexcept
    {
        return mState;
    }

    [[nodiscard]] int id() const noexcept
    {
        return mId;
    }

    [[nodiscard]] CopyAllocator select_on_container_copy_construction() const
    {
        auto selected = *this;
        selected.mId += 100;
        return selected;
    }

    template <typename U>
    [[nodiscard]] bool operator==(const CopyAllocator<U>& other) const noexcept
    {
        return mState == other.state() && mId == other.id();
    }

private:
    CopyAllocationState* mState = nullptr;
    int mId = 0;
};


template <typename Destination, typename Operand, bool Supported>
[[nodiscard]] consteval bool compoundAvailability()
{
    using allocator = std::allocator<typename Destination::value_type>;
    static_assert((requires(Destination& a, const Operand& b) { addAssign(a, b); }) == Supported);
    static_assert((requires(Destination& a, const Operand& b) { subtractAssign(a, b); }) == Supported);
    static_assert((requires(Destination& a, const Operand& b) { multiplyAssign(a, b); }) == Supported);
    static_assert((requires(Destination& a, const Operand& b) { divideAssign(a, b); }) == Supported);
    static_assert((requires(Destination& a, const Operand& b) { a += b; }) == Supported);
    static_assert((requires(Destination& a, const Operand& b) { a -= b; }) == Supported);
    static_assert((requires(Destination& a, const Operand& b) { a *= b; }) == Supported);
    static_assert((requires(Destination& a, const Operand& b) { a /= b; }) == Supported);
    static_assert((requires(Destination& a, const Operand& b) { addAssign(a, b, allocator{}); }) == Supported);
    static_assert((requires(Destination& a, const Operand& b) { subtractAssign(a, b, allocator{}); }) == Supported);
    static_assert((requires(Destination& a, const Operand& b) { multiplyAssign(a, b, allocator{}); }) == Supported);
    static_assert((requires(Destination& a, const Operand& b) { divideAssign(a, b, allocator{}); }) == Supported);
    if constexpr (Supported)
    {
        using reference = Destination&;
        static_assert(std::same_as<decltype(addAssign(std::declval<reference>(), std::declval<Operand>())), reference>);
        static_assert(std::same_as<decltype(subtractAssign(
            std::declval<reference>(), std::declval<Operand>())), reference>);
        static_assert(std::same_as<decltype(multiplyAssign(
            std::declval<reference>(), std::declval<Operand>())), reference>);
        static_assert(std::same_as<decltype(divideAssign(
            std::declval<reference>(), std::declval<Operand>())), reference>);
        static_assert(std::same_as<decltype(std::declval<reference>() += std::declval<Operand>()), reference>);
        static_assert(std::same_as<decltype(std::declval<reference>() -= std::declval<Operand>()), reference>);
        static_assert(std::same_as<decltype(std::declval<reference>() *= std::declval<Operand>()), reference>);
        static_assert(std::same_as<decltype(std::declval<reference>() /= std::declval<Operand>()), reference>);
    }
    return true;
}
template <typename Left, typename Right, typename Expected>
[[nodiscard]] consteval bool scalarArithmeticTypePair()
{
    constexpr bool supported = !std::same_as<Expected, void>;
    using owner = Tensor<Left>;
    using allocator = std::allocator<std::conditional_t<supported, Expected, Left>>;
    static_assert((requires(const owner& a, Right b) { add(a, b); }) == supported);
    static_assert((requires(const owner& a, Right b) { add(b, a); }) == supported);
    static_assert((requires(const owner& a, Right b) { subtract(a, b); }) == supported);
    static_assert((requires(const owner& a, Right b) { subtract(b, a); }) == supported);
    static_assert((requires(const owner& a, Right b) { multiply(a, b); }) == supported);
    static_assert((requires(const owner& a, Right b) { multiply(b, a); }) == supported);
    static_assert((requires(const owner& a, Right b) { divide(a, b); }) == supported);
    static_assert((requires(const owner& a, Right b) { divide(b, a); }) == supported);
    static_assert((requires(const owner& a, Right b) { add(a, b, allocator{}); }) == supported);
    static_assert((requires(const owner& a, Right b) { add(b, a, allocator{}); }) == supported);
    static_assert((requires(const owner& a, Right b) { subtract(a, b, allocator{}); }) == supported);
    static_assert((requires(const owner& a, Right b) { subtract(b, a, allocator{}); }) == supported);
    static_assert((requires(const owner& a, Right b) { multiply(a, b, allocator{}); }) == supported);
    static_assert((requires(const owner& a, Right b) { multiply(b, a, allocator{}); }) == supported);
    static_assert((requires(const owner& a, Right b) { divide(a, b, allocator{}); }) == supported);
    static_assert((requires(const owner& a, Right b) { divide(b, a, allocator{}); }) == supported);
    static_assert((requires(const owner& a, Right b) { a + b; }) == supported);
    static_assert((requires(const owner& a, Right b) { b + a; }) == supported);
    static_assert((requires(const owner& a, Right b) { a - b; }) == supported);
    static_assert((requires(const owner& a, Right b) { b - a; }) == supported);
    static_assert((requires(const owner& a, Right b) { a * b; }) == supported);
    static_assert((requires(const owner& a, Right b) { b * a; }) == supported);
    static_assert((requires(const owner& a, Right b) { a / b; }) == supported);
    static_assert((requires(const owner& a, Right b) { b / a; }) == supported);
    if constexpr (supported)
    {
        static_assert(std::same_as<decltype(std::declval<owner>() + std::declval<Right>()), Tensor<Expected>>);
        static_assert(std::same_as<decltype(std::declval<Right>() + std::declval<owner>()), Tensor<Expected>>);
        static_assert(std::same_as<decltype(std::declval<owner>() - std::declval<Right>()), Tensor<Expected>>);
        static_assert(std::same_as<decltype(std::declval<Right>() - std::declval<owner>()), Tensor<Expected>>);
        static_assert(std::same_as<decltype(std::declval<owner>() * std::declval<Right>()), Tensor<Expected>>);
        static_assert(std::same_as<decltype(std::declval<Right>() * std::declval<owner>()), Tensor<Expected>>);
        static_assert(std::same_as<decltype(std::declval<owner>() / std::declval<Right>()), Tensor<Expected>>);
        static_assert(std::same_as<decltype(std::declval<Right>() / std::declval<owner>()), Tensor<Expected>>);
        static_assert(std::same_as<decltype(divide(std::declval<owner>(), std::declval<Right>(), allocator{})),
                                   Tensor<Expected, allocator>>);
        static_assert(std::same_as<decltype(divide(std::declval<Right>(), std::declval<owner>(), allocator{})),
                                   Tensor<Expected, allocator>>);
    }
    return true;
}

template <typename To, typename Source>
concept CanTensorCast = requires(const Source& source) { cast<To>(source); };

template <typename To, typename Source, typename Allocator>
concept CanTensorCastWithAllocator = requires(const Source& source, const Allocator& allocator) {
    cast<To>(source, allocator);
};

template <typename To, typename From>
[[nodiscard]] bool verifyCastPair()
{
    static_assert(CanTensorCast<To, Tensor<From>>);
    static_assert(CanTensorCast<To, TensorView<const From>>);
    static_assert(CanTensorCast<To, SharedTensorView<const From>>);
    static_assert(CanTensorCastWithAllocator<To, Tensor<From>, std::allocator<To>>);
    static_assert(CanTensorCastWithAllocator<To, TensorView<const From>, std::allocator<To>>);
    static_assert(CanTensorCastWithAllocator<To, SharedTensorView<const From>, std::allocator<To>>);
    static_assert(std::same_as<decltype(cast<To>(std::declval<const Tensor<From>&>())), Tensor<To>>);
    const Tensor<From> source({2}, From{1});
    const auto converted = cast<To>(source);
    FATP_ASSERT_TRUE(converted.extents() == DynamicExtents({2}), "Each arithmetic cast must preserve extents");
    FATP_ASSERT_TRUE(converted[0] == To{1}, "Every supported arithmetic type represents one");
    const auto scalarZero = cast<To>(Tensor<From>(DynamicExtents{}, From{0}));
    FATP_ASSERT_TRUE(scalarZero[0] == To{0}, "Scalar casts preserve the zero value");
    return true;
}

template <typename To, typename... From>
[[nodiscard]] bool verifyCastRow(std::tuple<From...>)
{
    return (verifyCastPair<To, From>() && ...);
}

template <typename... Types>
[[nodiscard]] bool verifyCastMatrix(std::tuple<Types...> types)
{
    return (verifyCastRow<Types>(types) && ...);
}

template <typename From, typename To>
[[nodiscard]] bool verifyFloatIntegerEdges()
{
    const From upper = std::ldexp(From{1}, std::numeric_limits<To>::digits);
    Tensor<From> value({}, upper);
    FATP_ASSERT_THROWS(cast<To>(value), std::overflow_error, "The power-of-two upper integer boundary is excluded");
    // The greatest integral source value below the boundary, independent of integer max conversion.
    value[0] = std::floor(std::nextafter(upper, From{0}));
    const auto inside = cast<To>(value);
    FATP_ASSERT_EQ(static_cast<From>(inside[0]), value[0], "The preceding representable integer is accepted");
    if constexpr (std::signed_integral<To>)
    {
        value[0] = -upper;
        const auto lowest = cast<To>(value);
        FATP_ASSERT_EQ(lowest[0], std::numeric_limits<To>::lowest(), "Signed minimum is inclusive");
        value[0] = std::floor(std::nextafter(-upper, -std::numeric_limits<From>::infinity()));
        FATP_ASSERT_THROWS(cast<To>(value), std::overflow_error, "An integral value below signed minimum is excluded");
    }
    else
    {
        value[0] = From{-1};
        FATP_ASSERT_THROWS(cast<To>(value), std::overflow_error, "Negative integral input cannot convert to unsigned");
    }
    value[0] = -From{0};
    const auto zero = cast<To>(value);
    FATP_ASSERT_EQ(zero[0], To{0}, "Negative floating zero converts to integer zero");
    value[0] = From{1.5};
    FATP_ASSERT_THROWS(cast<To>(value), std::domain_error, "Fractional input is never silently truncated");
    value[0] = std::numeric_limits<From>::quiet_NaN();
    FATP_ASSERT_THROWS(cast<To>(value), std::domain_error, "NaN must fail before any floating-to-integer conversion");
    value[0] = std::numeric_limits<From>::infinity();
    FATP_ASSERT_THROWS(cast<To>(value), std::domain_error, "Infinity is a domain error, not integer overflow");
    return true;
}

template <typename Character>
[[nodiscard]] bool verifyCharacterCastRange()
{
    static_assert(std::signed_integral<Character> == std::is_signed_v<Character>);
    Tensor<Character> source({2}, std::numeric_limits<Character>::lowest());
    source[1] = std::numeric_limits<Character>::max();
    const auto wide = cast<std::int64_t>(source);
    FATP_ASSERT_EQ(wide[0], static_cast<std::int64_t>(std::numeric_limits<Character>::lowest()),
                   "Character minimum converts numerically, with its actual signedness");
    FATP_ASSERT_EQ(wide[1], static_cast<std::int64_t>(std::numeric_limits<Character>::max()),
                   "Character maximum converts numerically");
    const auto roundTrip = cast<Character>(wide);
    FATP_ASSERT_TRUE(roundTrip[0] == source[0] && roundTrip[1] == source[1],
                     "Character endpoints round-trip through signed64");
    const Tensor<std::int64_t> negative({}, INT64_C(-1));
    if constexpr (std::is_signed_v<Character>)
    {
        const auto character = cast<Character>(negative);
        const auto integer = cast<std::int64_t>(character);
        FATP_ASSERT_EQ(integer[0], INT64_C(-1), "Signed character negatives are preserved in both directions");
        FATP_ASSERT_THROWS(cast<std::uint64_t>(character), std::overflow_error,
                           "A negative signed character cannot wrap to unsigned64");
    }
    else
    {
        FATP_ASSERT_THROWS(cast<Character>(negative), std::overflow_error,
                           "Unsigned characters reject negative integer input");
    }
    return true;
}

template <typename From>
[[nodiscard]] bool verifyFloatIntegerType()
{
    return verifyFloatIntegerEdges<From, std::int8_t>() &&
           verifyFloatIntegerEdges<From, std::uint8_t>() &&
           verifyFloatIntegerEdges<From, std::int16_t>() &&
           verifyFloatIntegerEdges<From, std::uint16_t>() &&
           verifyFloatIntegerEdges<From, std::int32_t>() &&
           verifyFloatIntegerEdges<From, std::uint32_t>() &&
           verifyFloatIntegerEdges<From, std::int64_t>() &&
           verifyFloatIntegerEdges<From, std::uint64_t>();
}

template <typename Left, typename Right, typename Expected>
[[nodiscard]] consteval bool arithmeticTypePair()
{
    static_assert(scalarArithmeticTypePair<Left, Right, Expected>());
    constexpr bool supported = !std::same_as<Expected, void>;
    using left_tensor = Tensor<Left>;
    using right_tensor = Tensor<Right>;
    using explicit_allocator = std::allocator<std::conditional_t<supported, Expected, Left>>;
    static_assert(compoundAvailability<left_tensor, Right, supported>());
    static_assert(compoundAvailability<left_tensor, right_tensor, supported>());
    static_assert(TensorArithmeticCompatible<Left, Right> == supported);
    static_assert((requires(const left_tensor& a, const right_tensor& b) { add(a, b); }) == supported);
    static_assert((requires(const left_tensor& a, const right_tensor& b) { subtract(a, b); }) == supported);
    static_assert((requires(const left_tensor& a, const right_tensor& b) { multiply(a, b); }) == supported);
    static_assert((requires(const left_tensor& a, const right_tensor& b) { divide(a, b); }) == supported);
    static_assert((requires(const left_tensor& a, const right_tensor& b) { a + b; }) == supported);
    static_assert((requires(const left_tensor& a, const right_tensor& b) { a - b; }) == supported);
    static_assert((requires(const left_tensor& a, const right_tensor& b) { a * b; }) == supported);
    static_assert((requires(const left_tensor& a, const right_tensor& b) { a / b; }) == supported);
    static_assert((requires(const left_tensor& a, const right_tensor& b) {
        add(a, b, explicit_allocator{});
    }) == supported);
    static_assert((requires(const left_tensor& a, const right_tensor& b) {
        subtract(a, b, explicit_allocator{});
    }) == supported);
    static_assert((requires(const left_tensor& a, const right_tensor& b) {
        multiply(a, b, explicit_allocator{});
    }) == supported);
    static_assert((requires(const left_tensor& a, const right_tensor& b) {
        divide(a, b, explicit_allocator{});
    }) == supported);
    if constexpr (supported)
    {
        static_assert(std::same_as<TensorArithmeticType<Left, Right>, Expected>);
        static_assert(std::same_as<TensorArithmeticType<Right, Left>, Expected>);
        static_assert(std::same_as<decltype(std::declval<left_tensor>() + std::declval<right_tensor>()),
                                   Tensor<Expected>>);
        static_assert(std::same_as<decltype(std::declval<left_tensor>() - std::declval<right_tensor>()),
                                   Tensor<Expected>>);
        static_assert(std::same_as<decltype(std::declval<left_tensor>() * std::declval<right_tensor>()),
                                   Tensor<Expected>>);
        static_assert(std::same_as<decltype(std::declval<left_tensor>() / std::declval<right_tensor>()),
                                   Tensor<Expected>>);
        static_assert(std::same_as<decltype(divide(std::declval<left_tensor>(), std::declval<right_tensor>(),
                                                  explicit_allocator{})), Tensor<Expected, explicit_allocator>>);
    }
    return true;
}

template <typename Left, typename... Expected, typename... Right>
[[nodiscard]] consteval bool arithmeticTypeRow(std::tuple<Right...>)
{
    static_assert(sizeof...(Expected) == sizeof...(Right));
    return (arithmeticTypePair<Left, Right, Expected>() && ...);
}

template <typename Left, typename Right>
[[nodiscard]] consteval bool arithmeticRangeInvariant()
{
    static_assert(TensorArithmeticCompatible<Left, Right> == TensorArithmeticCompatible<Right, Left>);
    if constexpr (TensorArithmeticCompatible<Left, Right>)
    {
        using result_type = TensorArithmeticType<Left, Right>;
        static_assert(std::same_as<result_type, TensorArithmeticType<Right, Left>>);
        if constexpr (std::same_as<Left, Right>)
        {
            static_assert(std::same_as<result_type, Left>);
        }
        if constexpr (std::integral<Left> && std::integral<Right>)
        {
            static_assert(std::integral<result_type>);
            static_assert(std::numeric_limits<result_type>::digits >= std::numeric_limits<Left>::digits);
            static_assert(std::numeric_limits<result_type>::digits >= std::numeric_limits<Right>::digits);
            static_assert(std::is_signed_v<result_type> || (!std::is_signed_v<Left> && !std::is_signed_v<Right>));
            static_assert(static_cast<Left>(static_cast<result_type>(std::numeric_limits<Left>::lowest())) ==
                          std::numeric_limits<Left>::lowest());
            static_assert(static_cast<Left>(static_cast<result_type>(std::numeric_limits<Left>::max())) ==
                          std::numeric_limits<Left>::max());
            static_assert(static_cast<Right>(static_cast<result_type>(std::numeric_limits<Right>::lowest())) ==
                          std::numeric_limits<Right>::lowest());
            static_assert(static_cast<Right>(static_cast<result_type>(std::numeric_limits<Right>::max())) ==
                          std::numeric_limits<Right>::max());
        }
    }
    return true;
}

template <typename Left, typename... Right>
[[nodiscard]] consteval bool arithmeticRangeRow(std::tuple<Right...>)
{
    return (arithmeticRangeInvariant<Left, Right>() && ...);
}

template <typename... Types>
[[nodiscard]] consteval bool arithmeticRangeMatrix(std::tuple<Types...> types)
{
    return (arithmeticRangeRow<Types>(types) && ...);
}

template <typename Allocator>
concept MixedExplicitAllocator = requires(const Tensor<std::int16_t>& left, const Tensor<double>& right,
                                         const Allocator& allocator) {
    add(left, right, allocator);
    subtract(left, right, allocator);
    multiply(left, right, allocator);
    divide(left, right, allocator);
};

// Selection has deliberately different behavior after rebinding to floating point.
template <typename T>
class ArithmeticAllocator : public CopyAllocator<T>
{
public:
    using CopyAllocator<T>::CopyAllocator;

    template <typename U>
    ArithmeticAllocator(const ArithmeticAllocator<U>& other)
        : CopyAllocator<T>(*other.state(), other.id())
    {
    }

    [[nodiscard]] ArithmeticAllocator select_on_container_copy_construction() const
    {
        return ArithmeticAllocator(*this->state(), this->id() + (std::floating_point<T> ? 1000 : 100));
    }

    template <typename U>
    struct rebind
    {
        using other = ArithmeticAllocator<U>;
    };
};

class ThrowingCopyValue
{
public:
    ThrowingCopyValue()
    {
        ++mLiveCount;
    }

    ThrowingCopyValue(const ThrowingCopyValue& other)
        : mValue(other.mValue)
        , mRemaining(other.mRemaining)
    {
        ++mLiveCount;
    }

    ~ThrowingCopyValue()
    {
        --mLiveCount;
    }

    [[nodiscard]] static int liveCount() noexcept
    {
        return mLiveCount;
    }

    ThrowingCopyValue& operator=(const ThrowingCopyValue& other)
    {
        if (other.mRemaining != nullptr)
        {
            if (*other.mRemaining == 0)
            {
                throw std::runtime_error("injected element assignment failure");
            }
            --*other.mRemaining;
        }
        mValue = other.mValue;
        mRemaining = other.mRemaining;
        return *this;
    }

    void set(int value, int& remaining) noexcept
    {
        mValue = value;
        mRemaining = &remaining;
    }

    [[nodiscard]] int value() const noexcept
    {
        return mValue;
    }

private:
    inline static int mLiveCount = 0;
    int mValue = 0;
    int* mRemaining = nullptr;
};

struct NoDefaultValue
{
    NoDefaultValue() = delete;
    explicit NoDefaultValue(int)
    {
    }
};

template <typename Destination, typename Source>
concept CanCopyFrom = requires(Destination& destination, const Source& source) {
    copyFrom(destination, source);
};

template <typename Source>
concept CanMaterializeCopy = requires(const Source& source) {
    reshapeCopy(source, DynamicExtents{1});
};

static_assert(CanCopyFrom<Tensor<int>, TensorView<const int>>);
static_assert(CanCopyFrom<TensorView<int>, SharedTensorView<const int>>);
static_assert(!CanCopyFrom<TensorView<const int>, Tensor<int>>);
static_assert(!CanCopyFrom<const TensorView<int>, Tensor<int>>);
static_assert(!CanCopyFrom<const Tensor<int>, Tensor<int>>);
static_assert(!CanCopyFrom<Tensor<int>, Tensor<double>>);
static_assert(!CanCopyFrom<Tensor<NoDefaultValue>, Tensor<NoDefaultValue>>);
static_assert(!CanMaterializeCopy<Tensor<NoDefaultValue>>);
static_assert(!CanMaterializeCopy<Tensor<std::unique_ptr<int>>>);

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

FATP_TEST_CASE(copy_from_shifted_and_permuted_aliases)
{
    std::vector<int> storage{1, 2, 3, 4, 5, 6};
    auto source = TensorView<const int>::borrow(storage.data(), TensorLayout::contiguous(DynamicExtents{5}));
    auto destination = TensorView<int>::borrow(storage.data() + 1, TensorLayout::contiguous(DynamicExtents{5}));
    copyFrom(destination, source);
    FATP_ASSERT_TRUE(storage == std::vector<int>({1, 1, 2, 3, 4, 5}),
                     "Shifted storage bases must still detect forward overlap");

    std::iota(storage.begin(), storage.end(), 1);
    auto backwardsSource = TensorView<const int>::borrow(storage.data() + 1,
                                                        TensorLayout::contiguous(DynamicExtents{5}));
    auto backwardsDestination = TensorView<int>::borrow(storage.data(),
                                                       TensorLayout::contiguous(DynamicExtents{5}));
    copyFrom(backwardsDestination, backwardsSource);
    FATP_ASSERT_TRUE(storage == std::vector<int>({2, 3, 4, 5, 6, 6}),
                     "Backward overlapping copy should use the original source values");

    Tensor<int> matrix({3, 3});
    std::iota(matrix.begin(), matrix.end(), 1);
    const auto transposed = matrix.transposeView();
    auto* originalStorage = matrix.data();
    const auto originalLayout = matrix.layout();
    copyFrom(matrix, transposed);
    FATP_ASSERT_TRUE(std::vector<int>(matrix.begin(), matrix.end()) ==
                         std::vector<int>({1, 4, 7, 2, 5, 8, 3, 6, 9}),
                     "An in-place transpose copy must snapshot both off-diagonal halves");
    FATP_ASSERT_TRUE(matrix.data() == originalStorage && matrix.layout() == originalLayout,
                     "Element transfer must not replace the destination storage or layout");
    FATP_ASSERT_EQ(transposed[1], 2, "Existing borrowed views must remain valid after element transfer");

    auto reversed = TensorView<const int>::borrow(matrix.data(),
        TensorLayout(9, 8, DynamicExtents{3, 3}, TensorStrides{-3, -1}));
    copyFrom(matrix, reversed);
    FATP_ASSERT_TRUE(std::vector<int>(matrix.begin(), matrix.end()) ==
                         std::vector<int>({9, 6, 3, 8, 5, 2, 7, 4, 1}),
                     "Negative-stride aliases must read the entire original source first");
    copyFrom(matrix, matrix);
    FATP_ASSERT_EQ(matrix[0], 9, "Self-copy should retain values and storage");

    Tensor<int> row({4});
    std::iota(row.begin(), row.end(), 1);
    const auto repeated = row.sliceView({3}, {4}).broadcastView(DynamicExtents{4});
    copyFrom(row, repeated);
    FATP_ASSERT_TRUE(std::vector<int>(row.begin(), row.end()) == std::vector<int>({4, 4, 4, 4}),
                     "Read-only zero-stride aliases are valid copy sources");
    return true;
}

FATP_TEST_CASE(materialization_shapes_values_and_allocators)
{
    CopyAllocationState state;
    using Allocator = CopyAllocator<int>;
    Tensor<int, Allocator> source(std::allocator_arg, Allocator(state, 7), DynamicExtents{2, 3});
    std::iota(source.begin(), source.end(), 1);
    const auto packed = clone(source);
    FATP_ASSERT_EQ(packed.get_allocator().id(), 107, "Owner materialization must use SOCCC");
    FATP_ASSERT_NE(packed.data(), source.data(), "Already-contiguous input must still be copied");
    FATP_ASSERT_TRUE(exactEqual(packed, source), "Canonical copy must preserve values and shape");
    FATP_ASSERT_EQ(state.allocations, std::size_t{2}, "Nonempty materialization allocates one element buffer");

    const auto transposed = source.transposeView();
    const auto reshaped = reshapeCopy(transposed, DynamicExtents{6}, Allocator(state, 9));
    FATP_ASSERT_EQ(reshaped.get_allocator().id(), 9, "Explicit allocator must bypass SOCCC");
    FATP_ASSERT_TRUE(reshaped.extents() == DynamicExtents({6}) && reshaped.layout().isContiguous(),
                     "Reshape materialization must publish the target canonical layout");
    FATP_ASSERT_TRUE(std::vector<int>(reshaped.begin(), reshaped.end()) == std::vector<int>({1, 4, 2, 5, 3, 6}),
                     "Reshape must consume a noncontiguous input in logical row-major order");
    auto defaultViewCopy = clone(transposed);
    static_assert(std::same_as<decltype(defaultViewCopy), Tensor<int>>);
    defaultViewCopy[0] = 99;
    FATP_ASSERT_EQ(source[0], 1, "Materializing a view must not alias its source");

    const auto ownerReshape = reshapeCopy(source, DynamicExtents{3, 2});
    FATP_ASSERT_EQ(ownerReshape.get_allocator().id(), 107, "Reshape default allocator must also use SOCCC");
    const auto attempts = state.attempts;
    FATP_ASSERT_THROWS(reshapeCopy(source, DynamicExtents{5}, Allocator(state, 11)), std::invalid_argument,
                       "A mismatched logical count must be rejected before result allocation");
    FATP_ASSERT_EQ(state.attempts, attempts, "Shape rejection must allocate no element storage");

    Tensor<int> scalar(DynamicExtents{}, 42);
    const auto singleton = reshapeCopy(scalar, DynamicExtents{1, 1});
    const auto scalarAgain = reshapeCopy(singleton, DynamicExtents{});
    FATP_ASSERT_EQ(scalarAgain.rank(), std::size_t{0}, "A one-element tensor may reshape to a scalar");
    FATP_ASSERT_EQ(scalarAgain[0], 42, "Scalar materialization must visit its sole value");
    const auto broadcastCopy = clone(scalar.broadcastView(DynamicExtents{2, 3}));
    FATP_ASSERT_TRUE(std::vector<int>(broadcastCopy.begin(), broadcastCopy.end()) == std::vector<int>(6, 42),
                     "Materialization must expand read-only broadcast values");

    Tensor<int> empty(DynamicExtents{2, 0, 3});
    const auto emptyCopy = clone(empty, Allocator(state, 13));
    const auto emptyReshape = reshapeCopy(empty, DynamicExtents{0, 9}, Allocator(state, 13));
    FATP_ASSERT_TRUE(emptyCopy.empty() && emptyReshape.extents() == DynamicExtents({0, 9}),
                     "Empty materialization preserves or explicitly changes its zero-extent shape");
    FATP_ASSERT_EQ(state.attempts, attempts, "Empty results must allocate no element storage");

    SharedTensorView<const int> retained;
    {
        Tensor<int> transient({2, 2}, 8);
        retained = transient.asSharedView().asConstView();
    }
    const auto retainedCopy = reshapeCopy(retained, DynamicExtents{4});
    FATP_ASSERT_EQ(retainedCopy[3], 8, "A retained shared source is readable after owner destruction");
    SharedTensorView<int> retainedDestination;
    {
        Tensor<int> transient({4}, 0);
        retainedDestination = transient.asSharedView();
    }
    copyFrom(retainedDestination, retainedCopy);
    FATP_ASSERT_EQ(retainedDestination[3], 8, "Element transfer accepts a retained shared destination");
    return true;
}

FATP_TEST_CASE(copy_from_allocation_contract)
{
    CopyAllocationState state;
    using Allocator = CopyAllocator<int>;
    Tensor<int, Allocator> owner(std::allocator_arg, Allocator(state, 7), DynamicExtents{4});
    std::iota(owner.begin(), owner.end(), 1);
    copyFrom(owner, owner);
    FATP_ASSERT_EQ(state.lastId, 7, "Default scratch uses the destination allocator without SOCCC");
    FATP_ASSERT_EQ(state.allocations, std::size_t{2}, "Potential overlap allocates exactly one scratch buffer");
    FATP_ASSERT_EQ(state.deallocations, std::size_t{1}, "Scratch is released before returning");
    FATP_ASSERT_EQ(state.lastCount, owner.size(), "Scratch contains exactly the logical element count");

    CopyAllocationState explicitState;
    copyFrom(owner, owner, Allocator(explicitState, 23));
    FATP_ASSERT_EQ(explicitState.lastId, 23, "An explicit scratch allocator overrides destination ownership");
    FATP_ASSERT_EQ(explicitState.allocations, explicitState.deallocations, "Explicit scratch must not be retained");

    auto view = owner.asView();
    const auto ownerAttempts = state.attempts;
    copyFrom(view, owner);
    FATP_ASSERT_EQ(state.attempts, ownerAttempts,
                   "A view destination uses default scratch even when its source is an owner");
    auto shared = owner.asSharedView();
    const auto explicitAllocations = explicitState.allocations;
    copyFrom(shared, owner, Allocator(explicitState, 23));
    FATP_ASSERT_EQ(explicitState.allocations, explicitAllocations + 1,
                   "An overlapping shared destination must also snapshot through explicit scratch");
    FATP_ASSERT_EQ(shared[3], 4, "Shared destination staging must preserve logical values");
    FATP_ASSERT_EQ(explicitState.allocations, explicitState.deallocations,
                   "Shared destination ownership must not retain operation scratch");

    int disjointStorage[4]{};
    auto disjoint = TensorView<int>::borrow(disjointStorage, TensorLayout::contiguous(DynamicExtents{4}));
    explicitState.fail = true;
    const auto attempts = explicitState.attempts;
    copyFrom(disjoint, owner, Allocator(explicitState, 23));
    FATP_ASSERT_EQ(explicitState.attempts, attempts, "Proven-disjoint allocations must bypass scratch");
    FATP_ASSERT_EQ(disjointStorage[3], 4, "Direct copy must transfer all elements");

    int shiftedStorage[]{1, 2, 3, 4, 5, 6};
    const auto shiftedSource = TensorView<const int>::borrow(
        shiftedStorage + 1, TensorLayout::contiguous(DynamicExtents{2}));
    auto shiftedDestination = TensorView<int>::borrow(
        shiftedStorage + 3, TensorLayout::contiguous(DynamicExtents{2}));
    copyFrom(shiftedDestination, shiftedSource, Allocator(explicitState, 23));
    FATP_ASSERT_EQ(explicitState.attempts, attempts,
                   "Different bases in one allocation can still prove disjoint absolute spans");
    FATP_ASSERT_EQ(shiftedStorage[3], 2, "Shifted disjoint copy must not confuse relative offsets with addresses");
    FATP_ASSERT_EQ(shiftedStorage[4], 3, "Shifted disjoint copy must transfer the complete source");

    std::vector<int> storage{1, 2, 3, 4, 5, 6};
    auto first = TensorView<const int>::borrow(storage.data(),
                                              TensorLayout(6, 0, DynamicExtents{3}, TensorStrides{1}));
    auto last = TensorView<int>::borrow(storage.data(),
                                       TensorLayout(6, 3, DynamicExtents{3}, TensorStrides{1}));
    copyFrom(last, first, Allocator(explicitState, 23));
    FATP_ASSERT_EQ(explicitState.attempts, attempts, "Disjoint spans in the same allocation also bypass scratch");

    auto even = TensorView<const int>::borrow(storage.data(),
                                             TensorLayout(6, 0, DynamicExtents{3}, TensorStrides{2}));
    auto odd = TensorView<int>::borrow(storage.data(),
                                      TensorLayout(6, 1, DynamicExtents{3}, TensorStrides{2}));
    const auto before = storage;
    FATP_ASSERT_THROWS(copyFrom(odd, even, Allocator(explicitState, 23)), std::bad_alloc,
                       "Interleaved bounding spans conservatively require scratch");
    FATP_ASSERT_TRUE(storage == before, "Scratch allocation failure must leave all destination values unchanged");
    explicitState.fail = false;
    copyFrom(odd, even, Allocator(explicitState, 23));
    FATP_ASSERT_TRUE(storage == std::vector<int>({1, 1, 3, 3, 2, 2}),
                     "Conservative staging must still implement interleaved logical copying");
    return true;
}

FATP_TEST_CASE(copy_from_shared_endpoint_requires_staging)
{
    std::vector<int> storage{1, 2, 3, 4, 5};
    const auto original = storage;
    auto left = TensorView<int>::borrow(storage.data(),
                                       TensorLayout(5, 0, DynamicExtents{3}, TensorStrides{1}));
    auto right = TensorView<int>::borrow(storage.data(),
                                        TensorLayout(5, 2, DynamicExtents{3}, TensorStrides{1}));
    CopyAllocationState state;
    state.fail = true;
    const CopyAllocator<int> allocator(state, 31);
    FATP_ASSERT_THROWS(copyFrom(right, left, allocator), std::bad_alloc,
                       "Source maximum equal to destination minimum is overlap, not adjacency");
    FATP_ASSERT_TRUE(storage == original, "Failed endpoint staging must precede every destination write");
    FATP_ASSERT_THROWS(copyFrom(left, right, allocator), std::bad_alloc,
                       "Destination maximum equal to source minimum is also overlap");
    FATP_ASSERT_TRUE(storage == original, "Symmetric endpoint staging must also preserve all values on failure");
    FATP_ASSERT_EQ(state.attempts, std::size_t{2}, "Both endpoint directions must attempt scratch allocation");

    state.fail = false;
    copyFrom(right, left, allocator);
    FATP_ASSERT_TRUE(storage == std::vector<int>({1, 2, 1, 2, 3}),
                     "Forward endpoint overlap must copy the original source sequence");
    std::copy(original.begin(), original.end(), storage.begin());
    copyFrom(left, right, allocator);
    FATP_ASSERT_TRUE(storage == std::vector<int>({3, 4, 5, 4, 5}),
                     "Backward endpoint overlap must copy the original source sequence");
    FATP_ASSERT_EQ(state.allocations, state.deallocations, "Endpoint scratch buffers must be reclaimed");
    return true;
}

FATP_TEST_CASE(copy_from_validation_empty_and_exceptions)
{
    Tensor<int> source({2, 2}, 4);
    Tensor<int> destination({4}, 9);
    CopyAllocationState state;
    state.fail = true;
    FATP_ASSERT_THROWS(copyFrom(destination, source, CopyAllocator<int>(state, 1)), std::invalid_argument,
                       "Equal counts with different extents must not silently reshape or broadcast");
    FATP_ASSERT_EQ(state.attempts, std::size_t{0}, "Copy shape validation must precede element allocation");
    FATP_ASSERT_EQ(destination[0], 9, "Rejected copying must preserve destination values");

    Tensor<int> empty(DynamicExtents{0, 3});
    copyFrom(empty, empty, CopyAllocator<int>(state, 1));
    FATP_ASSERT_EQ(state.attempts, std::size_t{0}, "Empty self-copy performs no scratch element allocation");
    Tensor<int> differentEmpty(DynamicExtents{3, 0});
    FATP_ASSERT_THROWS(copyFrom(empty, differentEmpty), std::invalid_argument,
                       "Even empty copying requires identical extents");
    Tensor<int> scalar(DynamicExtents{}, 6);
    Tensor<int> scalarTarget(DynamicExtents{}, 0);
    copyFrom(scalarTarget, scalar);
    FATP_ASSERT_EQ(scalarTarget[0], 6, "Scalar copying transfers exactly one value");

    int invalidStorage[]{7, 8, 9};
    FATP_ASSERT_THROWS(TensorView<int>::borrow(invalidStorage,
        TensorLayout(3, 0, DynamicExtents{2, 2}, TensorStrides{1, 1})), std::invalid_argument,
        "Public mutable view construction must reject a noninjective copy destination");
    FATP_ASSERT_EQ(invalidStorage[0], 7, "Rejected noninjective destinations cannot be mutated");

#ifndef NDEBUG
    TensorView<int> expired;
    {
        Tensor<int> temporary({4}, 2);
        expired = temporary.asView();
    }
    FATP_ASSERT_THROWS(copyFrom(destination, expired), std::runtime_error,
                       "Expired sources must be checked before pointer arithmetic or reads");
    FATP_ASSERT_THROWS(copyFrom(expired, destination), std::runtime_error,
                       "Expired destinations must be checked before pointer arithmetic or writes");
    FATP_ASSERT_THROWS(reshapeCopy(expired, DynamicExtents{4}, CopyAllocator<int>(state, 1)), std::runtime_error,
                       "Reshape materialization validates lifetime before allocation");
    FATP_ASSERT_EQ(state.attempts, std::size_t{0}, "Expired lifetime rejection must not allocate element storage");
#endif

    CopyAllocationState throwingState;
    Tensor<ThrowingCopyValue> values({4});
    int remaining = 2;
    for (std::size_t index = 0; index < values.size(); ++index)
    {
        values[index].set(static_cast<int>(index + 1), remaining);
    }
    auto reversed = TensorView<const ThrowingCopyValue>::borrow(values.data(),
        TensorLayout(4, 3, DynamicExtents{4}, TensorStrides{-1}));
    const CopyAllocator<ThrowingCopyValue> allocator(throwingState, 5);
    FATP_ASSERT_THROWS(copyFrom(values, reversed, allocator), std::runtime_error,
                       "An element failure during staging must propagate");
    FATP_ASSERT_EQ(values[0].value(), 1, "A failed snapshot must not start destination writes");
    FATP_ASSERT_EQ(values[3].value(), 4, "A failed snapshot must preserve every destination element");
    FATP_ASSERT_EQ(throwingState.allocations, throwingState.deallocations,
                   "Partially filled scratch must be reclaimed on element failure");
    FATP_ASSERT_EQ(ThrowingCopyValue::liveCount(), 4, "Failed staging must destroy every scratch element");

    remaining = 2;
    FATP_ASSERT_THROWS(reshapeCopy(reversed, DynamicExtents{2, 2}, allocator), std::runtime_error,
                       "Reshape copy must propagate an element failure without publishing a partial result");
    FATP_ASSERT_EQ(ThrowingCopyValue::liveCount(), 4, "Failed result construction must destroy every result element");
    for (std::size_t index = 0; index < values.size(); ++index)
    {
        FATP_ASSERT_EQ(values[index].value(), static_cast<int>(index + 1),
                       "A failed snapshot or new result must preserve every source value");
    }

    remaining = 5;
    auto* originalStorage = values.data();
    FATP_ASSERT_THROWS(copyFrom(values, reversed, allocator), std::runtime_error,
                       "Throwing final assignment has a basic, not rollback, guarantee");
    FATP_ASSERT_EQ(values[0].value(), 4, "Assignments completed before the failure remain visible");
    FATP_ASSERT_EQ(values[1].value(), 2, "The throwing test element preserves its previous value");
    FATP_ASSERT_TRUE(values.data() == originalStorage && values.extents() == DynamicExtents({4}),
                     "Assignment failure must preserve destination storage and metadata");
    FATP_ASSERT_EQ(throwingState.allocations, throwingState.deallocations,
                   "Scratch must also be reclaimed after final assignment failure");
    FATP_ASSERT_EQ(ThrowingCopyValue::liveCount(), 4, "Final write failure must destroy every scratch element");

    Tensor<ThrowingCopyValue> disjoint({4});
    remaining = 1;
    const auto attempts = throwingState.attempts;
    FATP_ASSERT_THROWS(copyFrom(disjoint, values, allocator), std::runtime_error,
                       "Direct assignment failures also provide only the basic guarantee");
    FATP_ASSERT_EQ(disjoint[0].value(), 4, "Successful direct assignments remain visible after failure");
    FATP_ASSERT_EQ(throwingState.attempts, attempts, "Disjoint throwing copies must still bypass element scratch");
    return true;
}

FATP_TEST_CASE(randomized_materialization_and_overlap_oracle)
{
    std::mt19937_64 random(0xC0F1F20FULL);
    std::size_t checked = 0;
    for (std::size_t sample = 0; sample < 700; ++sample)
    {
        const auto rank = static_cast<std::size_t>(random() % 4);
        std::vector<std::size_t> extents(rank);
        TensorStrides sourceStrides(rank);
        TensorStrides destinationStrides(rank);
        for (std::size_t axis = 0; axis < rank; ++axis)
        {
            extents[axis] = static_cast<std::size_t>(random() % 4);
            sourceStrides[axis] = static_cast<std::ptrdiff_t>(random() % 13) - 6;
            destinationStrides[axis] = static_cast<std::ptrdiff_t>(random() % 13) - 6;
        }
        const auto sourceLayout = makeSmallLayout(extents, sourceStrides);
        const auto destinationLayout = makeSmallLayout(extents, destinationStrides);
        if (!destinationLayout.isInjective())
        {
            continue;
        }
        const auto sourceBase = static_cast<std::size_t>(random() % 4);
        const auto destinationBase = static_cast<std::size_t>(random() % 4);
        std::vector<int> storage(1 + std::max(sourceBase + sourceLayout.storageLength(),
                                            destinationBase + destinationLayout.storageLength()));
        std::iota(storage.begin(), storage.end(), 100);
        const auto before = storage;
        const auto sourceOffsets = tensor_support::enumerateOffsets(
            {extents, sourceStrides, sourceLayout.originOffset()});
        const auto destinationOffsets = tensor_support::enumerateOffsets(
            {extents, destinationStrides, destinationLayout.originOffset()});
        auto expected = before;
        std::vector<int> logicalValues;
        for (std::size_t index = 0; index < sourceOffsets.size(); ++index)
        {
            const auto value = before[sourceBase + static_cast<std::size_t>(sourceOffsets[index])];
            logicalValues.push_back(value);
            expected[destinationBase + static_cast<std::size_t>(destinationOffsets[index])] = value;
        }
        const auto source = TensorView<const int>::borrow(storage.data() + sourceBase, sourceLayout);
        auto destination = TensorView<int>::borrow(storage.data() + destinationBase, destinationLayout);
        const auto packed = clone(source);
        const auto reshaped = reshapeCopy(source, DynamicExtents{source.size()});
        FATP_ASSERT_TRUE(std::vector<int>(packed.begin(), packed.end()) == logicalValues,
                         "Canonical materialization must match the independent coordinate oracle");
        FATP_ASSERT_TRUE(std::vector<int>(reshaped.begin(), reshaped.end()) == logicalValues,
                         "Reshaped materialization must match the independent coordinate oracle");
        copyFrom(destination, source);
        FATP_ASSERT_TRUE(storage == expected,
                         "Aliased signed/broadcast copies must match a full pre-write source snapshot");
        ++checked;
    }
    FATP_ASSERT_GE(checked, std::size_t{400}, "The oracle must exercise a substantial valid-layout sample");
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

        const auto verify = [&](const auto& plan,
                                const std::vector<std::reference_wrapper<const TensorLayout>>& layouts) {
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

FATP_TEST_CASE(mixed_arithmetic_type_matrix)
{
    using I8 = std::int8_t;
    using U8 = std::uint8_t;
    using I16 = std::int16_t;
    using U16 = std::uint16_t;
    using I32 = std::int32_t;
    using U32 = std::uint32_t;
    using I64 = std::int64_t;
    using U64 = std::uint64_t;
    using F32 = float;
    using F64 = double;
    using FL = long double;
    using Types = std::tuple<I8, U8, I16, U16, I32, U32, I64, U64, F32, F64, FL>;
    static_assert(arithmeticTypeRow<I8,  I8, I16, I16, I32, I32, I64, I64, void, F32, F64, FL>(Types{}));
    static_assert(arithmeticTypeRow<U8, I16,  U8, I16, U16, I32, U32, I64,  U64, F32, F64, FL>(Types{}));
    static_assert(arithmeticTypeRow<I16,I16, I16, I16, I32, I32, I64, I64, void, F32, F64, FL>(Types{}));
    static_assert(arithmeticTypeRow<U16,I32, U16, I32, U16, I32, U32, I64,  U64, F32, F64, FL>(Types{}));
    static_assert(arithmeticTypeRow<I32,I32, I32, I32, I32, I32, I64, I64, void, F64, F64, FL>(Types{}));
    static_assert(arithmeticTypeRow<U32,I64, U32, I64, U32, I64, U32, I64,  U64, F64, F64, FL>(Types{}));
    static_assert(arithmeticTypeRow<I64,I64, I64, I64, I64, I64, I64, I64, void, F64, F64, FL>(Types{}));
    static_assert(arithmeticTypeRow<U64,void,U64,void,U64,void, U64,void, U64, F64, F64, FL>(Types{}));
    static_assert(arithmeticTypeRow<F32,F32, F32, F32, F32, F64, F64, F64, F64, F32, F64, FL>(Types{}));
    static_assert(arithmeticTypeRow<F64,F64, F64, F64, F64, F64, F64, F64, F64, F64, F64, FL>(Types{}));
    static_assert(arithmeticTypeRow<FL,  FL,  FL,  FL,  FL,  FL,  FL,  FL,  FL,  FL,  FL, FL>(Types{}));

    using StandardTypes = std::tuple<signed char, unsigned char, short, unsigned short, int, unsigned int,
                                     long, unsigned long, long long, unsigned long long, char, wchar_t,
                                     char8_t, char16_t, char32_t, float, double, long double>;
    static_assert(arithmeticRangeMatrix(StandardTypes{}));
    static_assert(arithmeticTypePair<bool, bool, void>());
    static_assert(arithmeticTypePair<bool, int, void>());
    static_assert(arithmeticTypePair<double, bool, void>());
    static_assert(arithmeticTypePair<NoDefaultValue, NoDefaultValue, void>());
    static_assert(arithmeticTypePair<int*, int*, void>());
    enum class Code { One };
    static_assert(arithmeticTypePair<Code, Code, void>());
    static_assert(!TensorArithmeticCompatible<void, int>);
    static_assert(std::same_as<TensorArithmeticType<const I16&, volatile U8>, I16>);
    static_assert(MixedExplicitAllocator<std::allocator<double>>);
    static_assert(!MixedExplicitAllocator<std::allocator<int>>);
    static_assert(!MixedExplicitAllocator<int>);
    return true;
}

FATP_TEST_CASE(mixed_arithmetic_exhaustive_byte_values)
{
    Tensor<std::int8_t> left({256, 1});
    Tensor<std::uint8_t> right({1, 256});
    for (int index = 0; index < 256; ++index)
    {
        left[static_cast<std::size_t>(index)] = static_cast<std::int8_t>(index - 128);
        right[static_cast<std::size_t>(index)] = static_cast<std::uint8_t>(index);
    }
    const auto added = left + right;
    const auto subtracted = left - right;
    const auto multiplied = left * right;
    const auto reversed = right - left;
    FATP_ASSERT_TRUE(added.extents() == DynamicExtents({256, 256}), "Mixed arithmetic must broadcast both inputs");
    for (int a = -128; a <= 127; ++a)
    {
        for (int b = 0; b <= 255; ++b)
        {
            const auto index = static_cast<std::size_t>((a + 128) * 256 + b);
            FATP_ASSERT_EQ(added[index], a + b, "Every signed/unsigned byte sum must preserve its mathematical value");
            FATP_ASSERT_EQ(subtracted[index], a - b, "Byte subtraction must convert before subtracting");
            FATP_ASSERT_EQ(multiplied[index], a * b, "Byte multiplication must not narrow to either input type");
            FATP_ASSERT_EQ(reversed[index], b - a, "Reversing operands must not change promotion");
        }
    }
    FATP_ASSERT_EQ(left[0], std::int8_t{-128}, "Arithmetic must preserve the left source");
    FATP_ASSERT_EQ(right[255], std::uint8_t{255}, "Arithmetic must preserve the right source");
    return true;
}

FATP_TEST_CASE(mixed_arithmetic_numeric_boundaries)
{
    const Tensor<std::int32_t> signedMaximum({}, std::numeric_limits<std::int32_t>::max());
    const Tensor<std::uint32_t> unsignedMaximum({}, std::numeric_limits<std::uint32_t>::max());
    const auto wideSum = add(signedMaximum, unsignedMaximum);
    const auto wideProduct = multiply(signedMaximum, unsignedMaximum);
    FATP_ASSERT_EQ(wideSum[0], INT64_C(6442450942), "Both 32-bit domains must fit the promoted signed result");
    FATP_ASSERT_EQ(wideProduct[0], INT64_C(9223372030412324865), "Multiplication must happen after widening");

    const Tensor<std::int16_t> maximum({}, std::numeric_limits<std::int16_t>::max());
    const Tensor<std::int16_t> minimum({}, std::numeric_limits<std::int16_t>::lowest());
    const Tensor<std::uint8_t> one({}, std::uint8_t{1});
    const Tensor<std::uint8_t> two({}, std::uint8_t{2});
    FATP_ASSERT_THROWS(add(maximum, one), std::overflow_error, "A mixed result still checks addition overflow");
    FATP_ASSERT_THROWS(subtract(minimum, one), std::overflow_error, "Mixed subtraction checks the lower boundary");
    FATP_ASSERT_THROWS(multiply(minimum, two), std::overflow_error, "Mixed multiplication checks negative overflow");
    const Tensor<std::uint8_t> byteMaximum({}, std::uint8_t{255});
    FATP_ASSERT_THROWS(add(byteMaximum, one), std::overflow_error, "Same-type byte addition must remain checked");
    FATP_ASSERT_THROWS(multiply(byteMaximum, two), std::overflow_error, "Same-type byte product must remain checked");
    const Tensor<std::uint64_t> unsignedZero({}, UINT64_C(0));
    const Tensor<std::uint64_t> largestUnsigned({}, std::numeric_limits<std::uint64_t>::max());
    FATP_ASSERT_THROWS(subtract(unsignedZero, one), std::overflow_error, "Mixed unsigned subtraction cannot wrap");
    FATP_ASSERT_THROWS(multiply(largestUnsigned, two), std::overflow_error, "Mixed unsigned product cannot wrap");

    const Tensor<float> largeFloat({}, 16777216.0F);
    const Tensor<std::int32_t> integerOne({}, 1);
    const Tensor<std::int16_t> shortOne({}, std::int16_t{1});
    const auto promoted = add(largeFloat, integerOne);
    const auto retained = add(largeFloat, shortOne);
    FATP_ASSERT_EQ(promoted[0], 16777217.0, "float plus int32 computes in double, not float then double");
    FATP_ASSERT_EQ(retained[0], 16777216.0F, "float plus int16 retains ordinary float arithmetic");
    const Tensor<double> zero({}, 0.0);
    const Tensor<std::int64_t> beyondDouble({}, INT64_C(9007199254740993));
    const auto rounded = add(beyondDouble, zero);
    const auto unsignedRounded = add(largestUnsigned, zero);
    FATP_ASSERT_EQ(rounded[0], 9007199254740992.0, "Integer to double conversion may round beyond 53 bits");
    FATP_ASSERT_EQ(unsignedRounded[0], 18446744073709551616.0, "uint64 to double is allowed and may round upward");

    const Tensor<float> negativeZero({}, -0.0F);
    const auto signedZero = multiply(negativeZero, shortOne);
    FATP_ASSERT_TRUE(signedZero[0] == 0.0F && std::signbit(signedZero[0]), "Floating multiplication keeps signed zero");
    const Tensor<float> infinity({}, std::numeric_limits<float>::infinity());
    const Tensor<std::int32_t> integerZero({}, 0);
    const auto invalidProduct = multiply(infinity, integerZero);
    FATP_ASSERT_TRUE(std::isnan(invalidProduct[0]), "Infinity times zero follows floating arithmetic");
    const Tensor<float> nan({}, std::numeric_limits<float>::quiet_NaN());
    const auto propagated = add(nan, integerOne);
    FATP_ASSERT_TRUE(std::isnan(propagated[0]), "NaNs propagate through promoted arithmetic");
    return true;
}

FATP_TEST_CASE(mixed_arithmetic_layout_oracle)
{
    std::int16_t shortStorage[]{1, 2, 3, 4, 5, 6};
    double doubleStorage[]{10.0, 20.0, 30.0};
    double outputStorage[]{-1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0};
    const auto reversed = TensorView<const std::int16_t>::borrow(
        shortStorage, TensorLayout(6, 2, DynamicExtents{2, 3}, TensorStrides{3, -1}));
    const auto broadcast = TensorView<const double>::borrow(
        doubleStorage, TensorLayout(3, 0, DynamicExtents{1, 3}, TensorStrides{0, 1}));
    auto output = TensorView<double>::borrow(
        outputStorage, TensorLayout(7, 4, DynamicExtents{2, 3}, TensorStrides{-4, 1}));
    tensor_detail::binaryKernel(reversed, broadcast, output, [](std::int16_t a, double b) {
        return static_cast<double>(a) + b;
    });
    FATP_ASSERT_TRUE(std::vector<double>(outputStorage, outputStorage + 7) ==
                     std::vector<double>({16.0, 25.0, 34.0, -1.0, 13.0, 22.0, 31.0}),
                     "Mixed kernel must use independent input and noncanonical output mappings");

    std::mt19937 generator(0xB1A4U);
    std::uniform_int_distribution<int> valueDistribution(-9, 9);
    std::uniform_int_distribution<int> strideDistribution(-5, 5);
    for (std::size_t trial = 0; trial < 600; ++trial)
    {
        const std::size_t rows = trial % 5;
        const std::size_t columns = (trial / 5) % 5;
        const bool leftBroadcast = trial % 2 == 0;
        const auto leftLayout = makeSmallLayout({rows, leftBroadcast ? 1 : columns},
                                               TensorStrides{strideDistribution(generator),
                                                             strideDistribution(generator)});
        const auto rightLayout = makeSmallLayout({leftBroadcast ? columns : 1},
                                                TensorStrides{strideDistribution(generator)});
        std::vector<std::int16_t> leftStorage(leftLayout.storageLength());
        std::vector<double> rightStorage(rightLayout.storageLength());
        for (auto& value : leftStorage)
        {
            value = static_cast<std::int16_t>(valueDistribution(generator));
        }
        for (auto& value : rightStorage)
        {
            value = static_cast<double>(valueDistribution(generator)) / 2.0;
        }
        const auto leftBefore = leftStorage;
        const auto rightBefore = rightStorage;
        const auto left = TensorView<const std::int16_t>::borrow(leftStorage.data(), leftLayout);
        const auto right = TensorView<const double>::borrow(rightStorage.data(), rightLayout);
        const auto added = add(left, right);
        const auto subtracted = subtract(left, right);
        const auto multiplied = multiply(left, right);
        const DynamicExtents expected{rows, columns};
        FATP_ASSERT_TRUE(added.extents() == expected && subtracted.extents() == expected &&
                         multiplied.extents() == expected, "Mixed signed-stride layouts must broadcast trailing axes");
        for (std::size_t index = 0; index < rows * columns; ++index)
        {
            const auto a = static_cast<double>(leftStorage[static_cast<std::size_t>(
                expectedBroadcastOffset(expected, leftLayout, index))]);
            const auto b = rightStorage[static_cast<std::size_t>(
                expectedBroadcastOffset(expected, rightLayout, index))];
            FATP_ASSERT_EQ(added[index], a + b, "Addition must use separate source coordinate mappings");
            FATP_ASSERT_EQ(subtracted[index], a - b, "Subtraction must use separate source coordinate mappings");
            FATP_ASSERT_EQ(multiplied[index], a * b, "Multiplication must use separate source coordinate mappings");
        }
        FATP_ASSERT_TRUE(leftStorage == leftBefore && rightStorage == rightBefore,
                         "Mixed operations cannot mutate overlapping or zero-stride read-only inputs");
    }
    return true;
}

FATP_TEST_CASE(mixed_arithmetic_allocators)
{
    CopyAllocationState leftState;
    CopyAllocationState rightState;
    CopyAllocationState explicitState;
    Tensor<std::int16_t, ArithmeticAllocator<std::int16_t>> left(
        std::allocator_arg, ArithmeticAllocator<std::int16_t>(leftState, 7), DynamicExtents{2}, std::int16_t{3});
    Tensor<double, ArithmeticAllocator<double>> right(
        std::allocator_arg, ArithmeticAllocator<double>(rightState, 9), DynamicExtents{2}, 0.5);
    {
        const auto added = add(left, right);
        const auto subtracted = subtract(left, right);
        const auto multiplied = multiply(left, right);
        static_assert(std::same_as<typename decltype(added)::allocator_type, ArithmeticAllocator<double>>);
        FATP_ASSERT_EQ(added.get_allocator().id(), 1007, "Rebind to double before type-sensitive SOCCC");
        FATP_ASSERT_EQ(subtracted.get_allocator().id(), 1007, "Subtraction selects the left owner's allocator");
        FATP_ASSERT_EQ(multiplied.get_allocator().id(), 1007, "Multiplication selects the left owner's allocator");
        FATP_ASSERT_EQ(leftState.allocations, std::size_t{4}, "Each nonempty result uses one element allocation");
        FATP_ASSERT_EQ(rightState.allocations, std::size_t{1}, "A second owner's allocator must not be selected");
        FATP_ASSERT_EQ(leftState.lastCount, std::size_t{2}, "Allocation count uses elements of the result type");
        FATP_ASSERT_EQ(added[0], 3.5, "Rebound allocation does not change numeric results");
    }
    FATP_ASSERT_EQ(leftState.deallocations, std::size_t{3}, "Each unpublished/expired result buffer is reclaimed");
    const auto rightSelected = add(left.asConstView(), right);
    FATP_ASSERT_EQ(rightSelected.get_allocator().id(), 1009,
                   "A right owner supplies the allocator after a left view");
    const auto leftSelected = add(left, right.asSharedView());
    FATP_ASSERT_EQ(leftSelected.get_allocator().id(), 1007, "Shared views do not supply owner allocators");
    const auto viewsOnly = multiply(left.asSharedView(), right.asConstView());
    static_assert(std::same_as<typename decltype(viewsOnly)::allocator_type, TensorAllocator<double>>);
    const auto explicitResult = subtract(left, right, ArithmeticAllocator<double>(explicitState, 23));
    FATP_ASSERT_EQ(explicitResult.get_allocator().id(), 23,
                   "Explicit allocator is neither replaced nor SOCCC-selected");
    FATP_ASSERT_EQ(explicitState.allocations, std::size_t{1}, "Explicit result uses its selected storage resource");

    Tensor<std::int16_t, ArithmeticAllocator<std::int16_t>> empty(
        std::allocator_arg, ArithmeticAllocator<std::int16_t>(leftState, 11), DynamicExtents{0, 2});
    const auto beforeEmpty = leftState.attempts;
    const auto emptyResult = add(empty, right);
    FATP_ASSERT_TRUE(emptyResult.empty(), "Empty mixed broadcast has no elements");
    FATP_ASSERT_EQ(emptyResult.get_allocator().id(), 1011, "Empty results still preserve allocator selection");
    FATP_ASSERT_EQ(leftState.attempts, beforeEmpty, "Empty results need no result element allocation");

    SharedTensorView<std::int16_t> retainedLeft;
    SharedTensorView<double> retainedRight;
    {
        Tensor<std::int16_t> temporaryLeft({}, std::int16_t{-3});
        Tensor<double> temporaryRight({}, 0.5);
        retainedLeft = temporaryLeft.asSharedView();
        retainedRight = temporaryRight.asSharedView();
    }
    const auto retainedResult = multiply(retainedLeft, retainedRight);
    FATP_ASSERT_EQ(retainedResult[0], -1.5, "Shared mixed operands retain both owners' storage after destruction");
    return true;
}

FATP_TEST_CASE(mixed_arithmetic_validation_and_cleanup)
{
    Tensor<std::int16_t> left({2}, std::int16_t{3});
    const Tensor<double> right({2}, 0.5);
    const Tensor<double> mismatch({3}, 0.5);
    CopyAllocationState state;
    state.fail = true;
    const CopyAllocator<double> allocator(state, 4);
    FATP_ASSERT_THROWS(add(left, mismatch, allocator), std::invalid_argument, "Add checks shape before allocation");
    FATP_ASSERT_THROWS(subtract(left, mismatch, allocator), std::invalid_argument, "Subtract validates shape first");
    FATP_ASSERT_THROWS(multiply(left, mismatch, allocator), std::invalid_argument, "Multiply validates shape first");
    FATP_ASSERT_EQ(state.attempts, std::size_t{0}, "Rejected broadcast cannot allocate result elements");
    const Tensor<std::int16_t> empty({0, 2});
    const Tensor<double> incompatibleEmpty({2, 2}, 1.0);
    FATP_ASSERT_THROWS(add(empty, incompatibleEmpty, allocator), std::invalid_argument,
                       "Zero extents cannot broadcast against extents greater than one");
    FATP_ASSERT_THROWS(subtract(empty, incompatibleEmpty, allocator), std::invalid_argument,
                       "Empty subtraction still validates broadcasting");
    FATP_ASSERT_THROWS(multiply(empty, incompatibleEmpty, allocator), std::invalid_argument,
                       "Empty multiplication still validates broadcasting");
    FATP_ASSERT_EQ(state.attempts, std::size_t{0}, "Incompatible empty inputs cannot allocate result elements");
#ifndef NDEBUG
    TensorView<std::int16_t> expired;
    TensorView<double> expiredRight;
    {
        Tensor<std::int16_t> temporary({2}, std::int16_t{1});
        Tensor<double> temporaryRight({2}, 1.0);
        expired = temporary.asView();
        expiredRight = temporaryRight.asView();
    }
    FATP_ASSERT_THROWS(add(expired, right, allocator), std::runtime_error, "Add validates borrowed lifetime first");
    FATP_ASSERT_THROWS(subtract(expired, right, allocator), std::runtime_error, "Subtract validates lifetime first");
    FATP_ASSERT_THROWS(multiply(expired, right, allocator), std::runtime_error, "Multiply validates lifetime first");
    FATP_ASSERT_THROWS(add(left, expiredRight, allocator), std::runtime_error, "Both input lifetimes are checked");
    FATP_ASSERT_THROWS(subtract(left, expiredRight, allocator), std::runtime_error,
                       "Right subtract lifetime is checked");
    FATP_ASSERT_THROWS(multiply(left, expiredRight, allocator), std::runtime_error,
                       "Right multiply lifetime is checked");
    FATP_ASSERT_EQ(state.attempts, std::size_t{0}, "Expired operands must not cause result element allocation");
#endif
    FATP_ASSERT_THROWS(add(left, right, allocator), std::bad_alloc, "Result allocation failure propagates");
    FATP_ASSERT_THROWS(subtract(left, right, allocator), std::bad_alloc, "Subtraction allocation failure propagates");
    FATP_ASSERT_THROWS(multiply(left, right, allocator), std::bad_alloc,
                       "Multiplication allocation failure propagates");
    FATP_ASSERT_EQ(state.attempts, std::size_t{3}, "Each failed result allocation is attempted once");
    FATP_ASSERT_EQ(state.allocations, std::size_t{0}, "Failed allocations publish no storage");
    FATP_ASSERT_EQ(left[0], std::int16_t{3}, "Allocation failure leaves the left operand unchanged");
    FATP_ASSERT_EQ(right[0], 0.5, "Allocation failure leaves the right operand unchanged");

    state.fail = false;
    left[1] = std::numeric_limits<std::int16_t>::max();
    const Tensor<std::uint8_t> two({2}, std::uint8_t{2});
    FATP_ASSERT_THROWS(add(left, two, CopyAllocator<std::int16_t>(state, 4)), std::overflow_error,
                       "Failure on a later output element must not publish partial results");
    FATP_ASSERT_THROWS(multiply(left, two, CopyAllocator<std::int16_t>(state, 4)), std::overflow_error,
                       "Partial multiplication results must also be reclaimed");
    left[1] = std::numeric_limits<std::int16_t>::lowest();
    FATP_ASSERT_THROWS(subtract(left, two, CopyAllocator<std::int16_t>(state, 4)), std::overflow_error,
                       "Partial subtraction results must also be reclaimed");
    FATP_ASSERT_EQ(state.allocations, std::size_t{3}, "Checked arithmetic may fail after allocating its result");
    FATP_ASSERT_EQ(state.deallocations, state.allocations, "RAII must release every failed result buffer");
    FATP_ASSERT_EQ(left[0], std::int16_t{3}, "Arithmetic failure leaves earlier input values unchanged");
    FATP_ASSERT_EQ(left[1], std::numeric_limits<std::int16_t>::lowest(), "Failing input values also stay unchanged");
    FATP_ASSERT_EQ(two[1], std::uint8_t{2}, "The right input remains unchanged after checked failure");
    return true;
}


FATP_TEST_CASE(cast_type_matrix_and_identity)
{
    using Types = std::tuple<bool, signed char, unsigned char, short, unsigned short, int, unsigned int,
                             long, unsigned long, long long, unsigned long long, char, wchar_t, char8_t,
                             char16_t, char32_t, float, double, long double>;
    FATP_ASSERT_TRUE(verifyCastMatrix(Types{}), "The complete 19-by-19 cast type matrix must work for zero and one");
    static_assert(!CanTensorCast<const int, Tensor<int>>);
    static_assert(!CanTensorCast<volatile int, Tensor<int>>);
    static_assert(!CanTensorCast<int&, Tensor<int>>);
    static_assert(!CanTensorCast<void, Tensor<int>>);
    static_assert(!CanTensorCast<int*, Tensor<int>>);
    static_assert(!CanTensorCast<int, Tensor<int*>>);
    static_assert(!CanTensorCast<int, Tensor<NoDefaultValue>>);
    static_assert(!CanTensorCast<int, int>);
    static_assert(!CanTensorCastWithAllocator<double, Tensor<int>, std::allocator<int>>);
    static_assert(!CanTensorCastWithAllocator<double, Tensor<int>, int>);
    enum class Code { One };
    static_assert(!CanTensorCast<Code, Tensor<int>>);
    static_assert(!CanTensorCast<int, Tensor<Code>>);
    static_assert(!CanTensorCast<std::byte, Tensor<int>>);

    Tensor<double> source({2}, std::numeric_limits<double>::quiet_NaN());
    source[1] = -0.0;
    auto independent = cast<double>(source);
    FATP_ASSERT_TRUE(independent.data() != source.data(), "Identity casts still materialize independent storage");
    FATP_ASSERT_TRUE(std::isnan(independent[0]) && std::signbit(independent[1]),
                     "Same-type casts preserve NaN category and signed zero");
    independent[1] = 5.0;
    FATP_ASSERT_TRUE(std::signbit(source[1]), "Writing a cast result cannot affect its source");
    return true;
}

FATP_TEST_CASE(cast_integer_and_bool_domains)
{
    FATP_ASSERT_TRUE(verifyCharacterCastRange<char>() && verifyCharacterCastRange<signed char>() &&
                     verifyCharacterCastRange<unsigned char>() && verifyCharacterCastRange<wchar_t>() &&
                     verifyCharacterCastRange<char8_t>() && verifyCharacterCastRange<char16_t>() &&
                     verifyCharacterCastRange<char32_t>(), "Character casts use implementation-defined signedness");
    Tensor<std::int16_t> value({}, std::int16_t{0});
    for (int integer = -256; integer < 512; ++integer)
    {
        value[0] = static_cast<std::int16_t>(integer);
        if (integer >= -128 && integer <= 127)
        {
            const auto result = cast<std::int8_t>(value);
            FATP_ASSERT_EQ(static_cast<int>(result[0]), integer, "Narrow signed casts preserve accepted values");
        }
        else
        {
            FATP_ASSERT_THROWS(cast<std::int8_t>(value), std::overflow_error, "Signed narrowing cannot wrap");
        }
        if (integer >= 0 && integer <= 255)
        {
            const auto result = cast<std::uint8_t>(value);
            FATP_ASSERT_EQ(static_cast<int>(result[0]), integer, "Narrow unsigned casts preserve accepted values");
        }
        else
        {
            FATP_ASSERT_THROWS(cast<std::uint8_t>(value), std::overflow_error, "Unsigned narrowing cannot wrap");
        }
        if (integer == 0 || integer == 1)
        {
            const auto result = cast<bool>(value);
            FATP_ASSERT_EQ(result[0], integer == 1, "Bool conversion accepts only its exact numeric domain");
        }
        else
        {
            FATP_ASSERT_THROWS(cast<bool>(value), std::domain_error, "Bool conversion is not generic truthiness");
        }
    }
    const Tensor<std::uint64_t> largest({}, std::numeric_limits<std::uint64_t>::max());
    FATP_ASSERT_THROWS(cast<std::int64_t>(largest), std::overflow_error, "uint64 maximum cannot fit signed64");
    const auto exact = cast<std::uint64_t>(largest);
    FATP_ASSERT_EQ(exact[0], std::numeric_limits<std::uint64_t>::max(), "Identity cast cannot route uint64 via int64");
    const Tensor<std::int64_t> negative({}, std::numeric_limits<std::int64_t>::lowest());
    FATP_ASSERT_THROWS(cast<std::uint64_t>(negative), std::overflow_error, "Signed minimum cannot become unsigned");
    const Tensor<std::int64_t> signedMaximum({}, std::numeric_limits<std::int64_t>::max());
    const auto widened = cast<std::uint64_t>(signedMaximum);
    FATP_ASSERT_EQ(widened[0], UINT64_C(9223372036854775807), "Nonnegative signed64 can convert to unsigned64");

    Tensor<double> binary({2}, -0.0);
    binary[1] = 1.0;
    const auto mask = cast<bool>(binary);
    FATP_ASSERT_TRUE(!mask[0] && mask[1], "Floating zero, including negative zero, and one map exactly to bool");
    const auto counts = cast<int>(mask);
    FATP_ASSERT_TRUE(counts[0] == 0 && counts[1] == 1, "Explicit bool casts make numeric arithmetic possible");
    const auto doubled = counts * 2;
    FATP_ASSERT_EQ(doubled[1], 2, "Cast bool values can participate in ordinary numeric operations");
    for (const double invalid : {0.5, -1.0, 2.0, std::numeric_limits<double>::infinity(),
                                 std::numeric_limits<double>::quiet_NaN()})
    {
        binary[1] = invalid;
        FATP_ASSERT_THROWS(cast<bool>(binary), std::domain_error, "Non-binary floating values cannot become bool");
    }
    return true;
}

FATP_TEST_CASE(cast_float_integer_boundaries)
{
    FATP_ASSERT_TRUE(verifyFloatIntegerType<float>(), "float conversion must check every fixed-width boundary");
    FATP_ASSERT_TRUE(verifyFloatIntegerType<double>(), "double conversion must check every fixed-width boundary");
    FATP_ASSERT_TRUE(verifyFloatIntegerType<long double>(), "long double must not be assumed wider than double");
    const Tensor<double> signedUpper({}, 9223372036854775808.0);
    const Tensor<double> unsignedUpper({}, 18446744073709551616.0);
    FATP_ASSERT_THROWS(cast<std::int64_t>(signedUpper), std::overflow_error, "Literal 2^63 is outside signed64");
    FATP_ASSERT_THROWS(cast<std::uint64_t>(unsignedUpper), std::overflow_error, "Literal 2^64 is outside unsigned64");
    const Tensor<double> roundedMaximum({}, static_cast<double>(std::numeric_limits<std::uint64_t>::max()));
    FATP_ASSERT_THROWS(cast<std::uint64_t>(roundedMaximum), std::overflow_error,
                       "Rounded uint64 maximum cannot be accepted as an inclusive floating bound");
    const Tensor<double> fractionalOutside({}, 256.5);
    FATP_ASSERT_THROWS(cast<std::uint8_t>(fractionalOutside), std::domain_error,
                       "Fractional rejection precedes range rejection");
    return true;
}

FATP_TEST_CASE(cast_floating_rounding_and_nonfinite)
{
    const Tensor<std::int64_t> notExact({}, INT64_C(9007199254740993));
    const auto rounded = cast<double>(notExact);
    FATP_ASSERT_EQ(rounded[0], 9007199254740992.0, "Integer-to-floating conversion permits precision loss");
    const Tensor<std::uint64_t> largest({}, std::numeric_limits<std::uint64_t>::max());
    const auto roundedUnsigned = cast<float>(largest);
    FATP_ASSERT_TRUE(std::isfinite(roundedUnsigned[0]), "Every supported integer fits standard float's range");
    const auto explicitExtended = cast<long double>(notExact);
    FATP_ASSERT_EQ(explicitExtended[0], static_cast<long double>(notExact[0]), "Explicit long double is permitted");

    Tensor<double> source({}, static_cast<double>(std::numeric_limits<float>::max()));
    const auto maximum = cast<float>(source);
    FATP_ASSERT_EQ(maximum[0], std::numeric_limits<float>::max(), "Exact finite floating maximum is accepted");
    source[0] = std::nextafter(source[0], std::numeric_limits<double>::infinity());
    FATP_ASSERT_THROWS(cast<float>(source), std::overflow_error,
                       "Any finite magnitude above target max is rejected, even if hardware might round it down");
    source[0] = -source[0];
    FATP_ASSERT_THROWS(cast<float>(source), std::overflow_error, "Negative finite overflow is also rejected");
    source[0] = std::numeric_limits<double>::lowest();
    FATP_ASSERT_THROWS(cast<float>(source), std::overflow_error, "Widest finite overflow is checked before casting");
    source[0] = static_cast<double>(std::numeric_limits<float>::denorm_min());
    const auto subnormal = cast<float>(source);
    FATP_ASSERT_EQ(subnormal[0], std::numeric_limits<float>::denorm_min(), "Representable subnormals are retained");
    source[0] = -std::numeric_limits<double>::denorm_min();
    const auto underflow = cast<float>(source);
    FATP_ASSERT_TRUE(underflow[0] == 0.0F && std::signbit(underflow[0]), "Tiny values may round to signed zero");
    source[0] = std::numeric_limits<double>::quiet_NaN();
    const auto nan = cast<float>(source);
    FATP_ASSERT_TRUE(std::isnan(nan[0]), "Cross-type NaNs preserve category, not payload");
    for (const double infinity : {std::numeric_limits<double>::infinity(),
                                  -std::numeric_limits<double>::infinity()})
    {
        source[0] = infinity;
        const auto converted = cast<float>(source);
        FATP_ASSERT_TRUE(std::isinf(converted[0]) && std::signbit(converted[0]) == std::signbit(infinity),
                         "Cross-type infinity preserves its sign");
    }
    return true;
}

FATP_TEST_CASE(cast_and_scalar_layout_oracle)
{
    std::mt19937 generator(0xCA57U);
    std::uniform_int_distribution<int> strideDistribution(-5, 5);
    std::uniform_int_distribution<int> valueDistribution(-9, 9);
    for (std::size_t trial = 0; trial < 600; ++trial)
    {
        const auto layout = makeSmallLayout({trial % 5, (trial / 5) % 5},
                                           TensorStrides{strideDistribution(generator), strideDistribution(generator)});
        std::vector<std::int16_t> storage(layout.storageLength());
        for (auto& value : storage)
        {
            value = static_cast<std::int16_t>(valueDistribution(generator));
        }
        const auto original = storage;
        const auto source = TensorView<const std::int16_t>::borrow(storage.data(), layout);
        const auto copied = cast<double>(source);
        const auto added = source + 0.5;
        const auto reverseAdded = 0.5 + source;
        const auto subtracted = source - 0.5;
        const auto reverseSubtracted = 0.5 - source;
        const auto multiplied = source * 0.5;
        const auto reverseMultiplied = 0.5 * source;
        FATP_ASSERT_TRUE(copied.extents() == source.extents() && added.extents() == source.extents() &&
                         subtracted.extents() == source.extents() && multiplied.extents() == source.extents(),
                         "Scalar operations and casts preserve the tensor's exact shape, including zeros");
        for (std::size_t index = 0; index < source.size(); ++index)
        {
            const auto offset = static_cast<std::size_t>(expectedBroadcastOffset(source.extents(), layout, index));
            const double expected = static_cast<double>(storage[offset]);
            FATP_ASSERT_EQ(copied[index], expected, "Cast uses logical coordinates instead of raw storage order");
            FATP_ASSERT_EQ(added[index], expected + 0.5, "Tensor plus scalar matches independent coordinates");
            FATP_ASSERT_EQ(reverseAdded[index], 0.5 + expected, "Scalar plus tensor preserves operand values");
            FATP_ASSERT_EQ(subtracted[index], expected - 0.5, "Tensor minus scalar preserves order");
            FATP_ASSERT_EQ(reverseSubtracted[index], 0.5 - expected, "Scalar minus tensor preserves order");
            FATP_ASSERT_EQ(multiplied[index], expected * 0.5, "Scalar multiplication uses logical strides");
            FATP_ASSERT_EQ(reverseMultiplied[index], 0.5 * expected, "Reverse multiplication uses logical strides");
        }
        FATP_ASSERT_TRUE(storage == original, "All view-based operations leave source storage untouched");
    }
    const Tensor<double> scalar({}, 2.0);
    const auto scalarResult = 3.0 - scalar;
    FATP_ASSERT_TRUE(scalarResult.rank() == 0 && scalarResult.size() == 1 && scalarResult[0] == 1.0,
                     "Rank zero is one element, not an empty tensor");
    FATP_ASSERT_THROWS(cast<bool>(scalar), std::domain_error, "Rank-zero casts must actually validate their value");
    return true;
}

FATP_TEST_CASE(scalar_numeric_boundaries_and_snapshot)
{
    const Tensor<std::int32_t> negative({}, -1);
    const auto mixed = negative + std::uint32_t{1};
    static_assert(std::same_as<typename decltype(mixed)::value_type, std::int64_t>);
    FATP_ASSERT_EQ(mixed[0], INT64_C(0), "Signed tensor plus unsigned scalar uses the existing signed promotion");
    const Tensor<float> large({}, 16777216.0F);
    const auto promoted = large + std::int32_t{1};
    const auto reverse = std::int32_t{1} + large;
    FATP_ASSERT_EQ(promoted[0], 16777217.0, "Scalar conversion occurs before promoted arithmetic");
    FATP_ASSERT_EQ(reverse[0], 16777217.0, "Reverse scalar promotion is identical");
    const Tensor<double> doubleZero({}, 0.0);
    const auto extendedMaximum = std::numeric_limits<long double>::max();
    const auto extendedLeft = doubleZero + extendedMaximum;
    const auto extendedRight = extendedMaximum - doubleZero;
    static_assert(std::same_as<typename decltype(extendedLeft)::value_type, long double>);
    static_assert(std::same_as<typename decltype(extendedRight)::value_type, long double>);
    FATP_ASSERT_TRUE(std::isfinite(extendedLeft[0]) && extendedLeft[0] == extendedMaximum,
                     "A supplied long-double scalar is never narrowed before arithmetic");
    FATP_ASSERT_EQ(extendedRight[0], extendedMaximum, "Reverse scalar order preserves long-double range");
    const Tensor<std::int16_t> maximum({}, std::numeric_limits<std::int16_t>::max());
    FATP_ASSERT_THROWS(maximum + std::uint8_t{1}, std::overflow_error, "Promoted scalar results still check overflow");
    FATP_ASSERT_THROWS(std::uint8_t{1} + maximum, std::overflow_error, "Reverse addition also checks overflow");
    const Tensor<std::int16_t> minimum({}, std::numeric_limits<std::int16_t>::lowest());
    FATP_ASSERT_THROWS(minimum - std::uint8_t{1}, std::overflow_error, "Forward subtraction checks signed minimum");
    FATP_ASSERT_THROWS(std::int16_t{0} - minimum, std::overflow_error, "Reverse subtraction checks operand order");
    FATP_ASSERT_THROWS(minimum * std::int16_t{-1}, std::overflow_error, "Signed minimum times negative one throws");
    FATP_ASSERT_THROWS(std::int16_t{-1} * minimum, std::overflow_error, "Reverse product cannot overflow silently");
    const Tensor<std::uint64_t> zero({}, UINT64_C(0));
    FATP_ASSERT_THROWS(zero - std::uint8_t{1}, std::overflow_error, "Unsigned scalar subtraction cannot wrap");
    const Tensor<float> negativeZero({}, -0.0F);
    const auto signedZero = negativeZero * std::int16_t{1};
    FATP_ASSERT_TRUE(std::signbit(signedZero[0]), "Scalar floating multiplication preserves signed zero");
    const auto nan = negativeZero * std::numeric_limits<double>::infinity();
    FATP_ASSERT_TRUE(std::isnan(nan[0]), "Floating scalar arithmetic follows ordinary nonfinite behavior");

    Tensor<int> owner({3}, 4);
    owner[1] = 7;
    const auto aliasScalar = owner + owner[1];
    const auto reverseAlias = owner[1] - owner;
    FATP_ASSERT_TRUE(aliasScalar[0] == 11 && aliasScalar[1] == 14 && reverseAlias[0] == 3 && reverseAlias[1] == 0,
                     "A scalar referring to an input element is snapshotted before computation");
    FATP_ASSERT_EQ(owner[1], 7, "Scalar operations do not write through a scalar alias");
    return true;
}

FATP_TEST_CASE(cast_and_scalar_allocator_contract)
{
    CopyAllocationState ownerState;
    CopyAllocationState explicitState;
    Tensor<std::int16_t, ArithmeticAllocator<std::int16_t>> owner(
        std::allocator_arg, ArithmeticAllocator<std::int16_t>(ownerState, 7), DynamicExtents{3}, std::int16_t{4});
    {
        const auto casted = cast<double>(owner);
        const auto added = owner + 0.5;
        const auto reverse = 0.5 - owner;
        const auto multiplied = 0.5 * owner;
        static_assert(std::same_as<typename decltype(casted)::allocator_type, ArithmeticAllocator<double>>);
        FATP_ASSERT_EQ(casted.get_allocator().id(), 1007, "Cast rebinds before type-sensitive SOCCC");
        FATP_ASSERT_EQ(added.get_allocator().id(), 1007, "Scalar arithmetic rebinds the tensor owner's allocator");
        FATP_ASSERT_EQ(reverse.get_allocator().id(), 1007, "A left scalar does not hide a right owner's allocator");
        FATP_ASSERT_EQ(multiplied.get_allocator().id(), 1007, "Scalar multiply follows the same allocator contract");
        FATP_ASSERT_EQ(ownerState.allocations, std::size_t{5}, "Only one result buffer per call is allocated");
        FATP_ASSERT_EQ(ownerState.lastCount, std::size_t{3}, "No rank-zero scalar element buffer is allocated");
    }
    FATP_ASSERT_EQ(ownerState.deallocations, std::size_t{4}, "All completed result buffers are reclaimed");
    const ArithmeticAllocator<double> allocator(explicitState, 19);
    const auto casted = cast<double>(owner, allocator);
    const auto forward = add(owner, 0.5, allocator);
    const auto backward = subtract(0.5, owner, allocator);
    const auto product = multiply(owner, 0.5, allocator);
    const auto explicitBorrow = cast<double>(owner.asConstView(), allocator);
    const auto explicitShared = cast<double>(owner.asSharedView(), allocator);
    FATP_ASSERT_EQ(casted.get_allocator().id(), 19, "Explicit cast allocators bypass SOCCC");
    FATP_ASSERT_EQ(forward.get_allocator().id(), 19, "Explicit scalar add allocator is unchanged");
    FATP_ASSERT_EQ(backward.get_allocator().id(), 19, "Explicit reverse subtraction allocator is unchanged");
    FATP_ASSERT_EQ(product.get_allocator().id(), 19, "Explicit scalar multiplication allocator is unchanged");
    FATP_ASSERT_EQ(explicitBorrow.get_allocator().id(), 19, "Borrowed casts use the exact explicit allocator");
    FATP_ASSERT_EQ(explicitShared.get_allocator().id(), 19, "Shared casts use the exact explicit allocator");
    FATP_ASSERT_EQ(explicitState.allocations, std::size_t{6}, "Explicit result allocation occurs once per call");

    const auto borrowedResult = cast<double>(owner.asConstView());
    const auto sharedResult = 0.5 + owner.asSharedView();
    static_assert(std::same_as<typename decltype(borrowedResult)::allocator_type, TensorAllocator<double>>);
    static_assert(std::same_as<typename decltype(sharedResult)::allocator_type, TensorAllocator<double>>);
    Tensor<std::int16_t, ArithmeticAllocator<std::int16_t>> empty(
        std::allocator_arg, ArithmeticAllocator<std::int16_t>(ownerState, 9), DynamicExtents{0, 3});
    const auto before = ownerState.attempts;
    const auto emptyCast = cast<double>(empty);
    const auto emptyScalar = empty + 0.5;
    FATP_ASSERT_TRUE(emptyCast.empty() && emptyScalar.empty(), "Empty inputs create empty outputs");
    FATP_ASSERT_EQ(emptyCast.get_allocator().id(), 1009, "Empty cast still selects the proper allocator");
    FATP_ASSERT_EQ(emptyScalar.get_allocator().id(), 1009, "Empty scalar operation still selects its allocator");
    FATP_ASSERT_EQ(ownerState.attempts, before, "Empty cast/scalar results allocate no element buffer");
    return true;
}

FATP_TEST_CASE(cast_and_scalar_failure_and_lifetime)
{
    CopyAllocationState state;
    state.fail = true;
    const CopyAllocator<int> integers(state, 11);
    const CopyAllocator<double> doubles(state, 11);
#ifndef NDEBUG
    TensorView<double> expired;
    {
        Tensor<double> temporary({2}, 2.0);
        expired = temporary.asView();
    }
    FATP_ASSERT_THROWS(cast<int>(expired, integers), std::runtime_error, "Cast validates lifetime before allocation");
    FATP_ASSERT_THROWS(add(expired, 1.0, doubles), std::runtime_error, "Scalar add validates lifetime first");
    FATP_ASSERT_THROWS(add(1.0, expired, doubles), std::runtime_error, "Reverse add validates lifetime first");
    FATP_ASSERT_THROWS(subtract(expired, 1.0, doubles), std::runtime_error, "Scalar subtract validates lifetime first");
    FATP_ASSERT_THROWS(subtract(1.0, expired, doubles), std::runtime_error,
                       "Reverse subtract validates lifetime first");
    FATP_ASSERT_THROWS(multiply(expired, 1.0, doubles), std::runtime_error, "Scalar multiply validates lifetime first");
    FATP_ASSERT_THROWS(multiply(1.0, expired, doubles), std::runtime_error,
                       "Reverse multiply validates lifetime first");
    FATP_ASSERT_EQ(state.attempts, std::size_t{0}, "Expired borrowed sources never allocate result elements");
#endif
    Tensor<double> source({2}, 1.0);
    FATP_ASSERT_THROWS(cast<int>(source, integers), std::bad_alloc, "Cast result allocation failure propagates");
    FATP_ASSERT_THROWS(add(source, 1.0, doubles), std::bad_alloc, "Scalar result allocation failure propagates");
    FATP_ASSERT_EQ(state.allocations, std::size_t{0}, "Failed allocation publishes no element storage");
    source[1] = 1.5;
    state.fail = false;
    FATP_ASSERT_THROWS(cast<int>(source, integers), std::domain_error,
                       "Conversion failure after a valid first element discards the partial result");
    source[1] = 2147483648.0;
    FATP_ASSERT_THROWS(cast<std::int32_t>(source, CopyAllocator<std::int32_t>(state, 11)), std::overflow_error,
                       "Later conversion overflow also cleans up its allocation");
    FATP_ASSERT_EQ(state.allocations, state.deallocations, "No cast failure leaks a result buffer");
    FATP_ASSERT_TRUE(source[0] == 1.0 && source[1] == 2147483648.0, "Failed casts preserve every source value");

    Tensor<int> overflowing({2}, 1);
    overflowing[1] = std::numeric_limits<int>::max();
    FATP_ASSERT_THROWS(add(overflowing, 1, integers), std::overflow_error, "Partial scalar addition is reclaimed");
    FATP_ASSERT_THROWS(multiply(2, overflowing, integers), std::overflow_error, "Partial reverse product is reclaimed");
    overflowing[1] = std::numeric_limits<int>::lowest();
    FATP_ASSERT_THROWS(subtract(0, overflowing, integers), std::overflow_error,
                       "Partial reverse difference is reclaimed");
    FATP_ASSERT_EQ(state.allocations, state.deallocations, "Scalar failure releases all unpublished result storage");
    FATP_ASSERT_EQ(overflowing[0], 1, "Earlier source elements remain unchanged after later failure");

    const Tensor<double> empty({0, 3});
    const auto emptyMask = cast<bool>(empty);
    const auto emptyInvalidScalar = empty * std::numeric_limits<double>::infinity();
    FATP_ASSERT_TRUE(emptyMask.empty() && emptyInvalidScalar.empty(), "Empty outputs invoke no element operation");
    SharedTensorView<double> shared;
    {
        Tensor<double> temporary({}, 2.0);
        shared = temporary.asSharedView();
    }
    const auto retainedCast = cast<int>(shared);
    const auto retainedScalar = 3.0 - shared;
    FATP_ASSERT_TRUE(retainedCast[0] == 2 && retainedScalar[0] == 1.0,
                     "Shared sources survive their owner's destruction");
    return true;
}

template <std::integral T>
[[nodiscard]] bool verifyIntegerDivisionBoundaries()
{
    const Tensor<T> maximum({}, std::numeric_limits<T>::max());
    const Tensor<T> one({}, T{1});
    const Tensor<T> zero({}, T{0});
    const auto identity = maximum / one;
    const auto half = maximum / T{2};
    const auto inverse = T{1} / maximum;
    FATP_ASSERT_TRUE(identity[0] == std::numeric_limits<T>::max(), "Integer maximum divided by one is exact");
    FATP_ASSERT_TRUE(half[0] == static_cast<T>(std::numeric_limits<T>::max() / T{2}),
                     "Integer division truncates without floating-point rounding");
    FATP_ASSERT_TRUE(inverse[0] == T{0}, "Reverse integer division preserves operand order");
    FATP_ASSERT_THROWS(divide(maximum, zero), std::domain_error, "Tensor integer zero divisor is rejected");
    FATP_ASSERT_THROWS(maximum / T{0}, std::domain_error, "Scalar integer zero divisor is rejected");
    FATP_ASSERT_THROWS(T{1} / zero, std::domain_error, "Reverse scalar zero divisor is rejected");
    FATP_ASSERT_THROWS(zero / zero, std::domain_error, "Integer zero over zero is a domain error");
    if constexpr (std::signed_integral<T>)
    {
        const Tensor<T> minimum({}, std::numeric_limits<T>::lowest());
        const Tensor<T> negativeOne({}, T{-1});
        FATP_ASSERT_THROWS(minimum / zero, std::domain_error,
                           "Signed minimum over zero remains a domain error, not overflow");
        FATP_ASSERT_THROWS(minimum / negativeOne, std::overflow_error,
                           "Signed minimum over negative one cannot fit even after C++ narrow promotions");
        FATP_ASSERT_THROWS(minimum / T{-1}, std::overflow_error, "Scalar division checks the signed minimum");
        FATP_ASSERT_THROWS(std::numeric_limits<T>::lowest() / negativeOne, std::overflow_error,
                           "Reverse scalar division checks the result's signed minimum");
        const auto minimumIdentity = minimum / one;
        FATP_ASSERT_TRUE(minimumIdentity[0] == std::numeric_limits<T>::lowest(), "Signed minimum over one is valid");
        const Tensor<T> negativeSeven({}, T{-7});
        const auto negative = negativeSeven / T{2};
        const auto positive = negativeSeven / T{-2};
        const auto reverse = T{7} / negativeSeven;
        FATP_ASSERT_TRUE(negative[0] == T{-3} && positive[0] == T{3} && reverse[0] == T{-1},
                         "Signed quotients truncate toward zero, not toward negative infinity");
    }
    return true;
}

template <typename... Types>
[[nodiscard]] bool verifyIntegerDivisionTypes(std::tuple<Types...>)
{
    return (verifyIntegerDivisionBoundaries<Types>() && ...);
}

FATP_TEST_CASE(division_integer_boundaries)
{
    using Types = std::tuple<signed char, unsigned char, short, unsigned short, int, unsigned int,
                             long, unsigned long, long long, unsigned long long, char, wchar_t,
                             char8_t, char16_t, char32_t>;
    return verifyIntegerDivisionTypes(Types{});
}

FATP_TEST_CASE(division_exhaustive_byte_values)
{
    Tensor<std::int8_t> numerators({256, 255});
    Tensor<std::int8_t> denominators({256, 255});
    Tensor<std::uint8_t> unsignedDenominators({1, 255});
    for (int divisorIndex = 0; divisorIndex < 255; ++divisorIndex)
    {
        unsignedDenominators[static_cast<std::size_t>(divisorIndex)] =
            static_cast<std::uint8_t>(divisorIndex + 1);
        for (int numerator = -128; numerator <= 127; ++numerator)
        {
            const auto index = static_cast<std::size_t>((numerator + 128) * 255 + divisorIndex);
            int denominator = divisorIndex < 128 ? divisorIndex - 128 : divisorIndex - 127;
            // The sole unrepresentable quotient is checked separately by the boundary test.
            if (numerator == -128 && denominator == -1)
            {
                denominator = 1;
            }
            numerators[index] = static_cast<std::int8_t>(numerator);
            denominators[index] = static_cast<std::int8_t>(denominator);
        }
    }
    const auto signedResult = numerators / denominators;
    const auto mixedResult = divide(numerators, unsignedDenominators);
    static_assert(std::same_as<typename decltype(signedResult)::value_type, std::int8_t>);
    static_assert(std::same_as<typename decltype(mixedResult)::value_type, std::int16_t>);
    for (std::size_t index = 0; index < numerators.size(); ++index)
    {
        const int numerator = numerators[index];
        const int denominator = denominators[index];
        const int unsignedDenominator = unsignedDenominators[index % 255];
        FATP_ASSERT_EQ(static_cast<int>(signedResult[index]), numerator / denominator,
                       "Every representable signed-byte quotient matches a wider integer oracle");
        FATP_ASSERT_EQ(static_cast<int>(mixedResult[index]), numerator / unsignedDenominator,
                       "Mixed signed/unsigned byte division converts before evaluation");
    }
    return true;
}

FATP_TEST_CASE(division_promotion_and_snapshot)
{
    const Tensor<std::int8_t> minimum({}, std::int8_t{-128});
    const auto widened = minimum / -1;
    const auto widenedTensor = minimum / Tensor<std::int16_t>({}, std::int16_t{-1});
    FATP_ASSERT_EQ(widened[0], 128, "An int scalar widens a narrow source before checking division overflow");
    FATP_ASSERT_EQ(widenedTensor[0], std::int16_t{128}, "Mixed tensor division checks the result, not source range");
    const Tensor<std::int16_t> shortMinimum({}, std::numeric_limits<std::int16_t>::lowest());
    const Tensor<std::int8_t> byteNegativeOne({}, std::int8_t{-1});
    FATP_ASSERT_THROWS(shortMinimum / byteNegativeOne, std::overflow_error,
                       "Mixed integer operands still reject overflow of their retained signed result type");
    FATP_ASSERT_THROWS(shortMinimum / std::int8_t{-1}, std::overflow_error,
                       "A narrower scalar does not widen or bypass the result's signed limit");
    const Tensor<std::uint8_t> unsignedByte({}, std::uint8_t{255});
    const auto signedQuotient = unsignedByte / std::int8_t{-1};
    FATP_ASSERT_EQ(signedQuotient[0], std::int16_t{-255}, "Unsigned input is converted to the signed common type");
    const Tensor<std::int32_t> preciseInteger({}, 16777217);
    const auto floatingQuotient = preciseInteger / 2.0F;
    const auto reverseFloating = 16777217 / Tensor<float>({}, 2.0F);
    FATP_ASSERT_EQ(floatingQuotient[0], 8388608.5, "float/int32 division computes in double from the start");
    FATP_ASSERT_EQ(reverseFloating[0], 8388608.5, "Reversed float/int32 operands use the same promotion");
    const Tensor<std::uint64_t> maximum({}, std::numeric_limits<std::uint64_t>::max());
    const auto integerQuotient = maximum / 2U;
    FATP_ASSERT_EQ(integerQuotient[0], UINT64_C(9223372036854775807), "uint64 division must not pass through double");
    const auto rounded = maximum / 1.0;
    FATP_ASSERT_EQ(rounded[0], 18446744073709551616.0, "Mixed floating division allows documented integer rounding");
    const Tensor<long double> extended({}, std::numeric_limits<long double>::max());
    const auto extendedQuotient = extended / 1.0;
    const auto extendedScalar = std::numeric_limits<long double>::max() / Tensor<double>({}, 1.0);
    FATP_ASSERT_TRUE(std::isfinite(extendedQuotient[0]) && extendedQuotient[0] == extended[0] &&
                     extendedScalar[0] == extended[0], "Long-double operands are never narrowed before division");
    Tensor<int> owner({2}, 2);
    owner[1] = 8;
    const auto forward = owner / owner[0];
    const auto backward = owner[1] / owner;
    FATP_ASSERT_TRUE(forward[0] == 1 && forward[1] == 4 && backward[0] == 4 && backward[1] == 1,
                     "Scalars aliasing source elements are captured in their original operand order");
    FATP_ASSERT_TRUE(owner[0] == 2 && owner[1] == 8, "Division does not modify aliased source elements");
    return true;
}

template <std::floating_point T>
[[nodiscard]] bool verifyFloatingDivision()
{
    const Tensor<T> seven({}, T{7});
    const Tensor<T> two({}, T{2});
    const auto finiteBinary = seven / two;
    const auto finiteScalar = seven / T{2};
    const auto finiteReverse = T{7} / two;
    FATP_ASSERT_TRUE(finiteBinary[0] == T{3.5} && finiteScalar[0] == T{3.5} && finiteReverse[0] == T{3.5},
                     "Ordinary finite floating quotients are checked even without IEC 559 special values");
    // Special-value promises apply to IEC 559 arithmetic with the default nontrapping environment.
    if constexpr (std::numeric_limits<T>::is_iec559)
    {
        constexpr T kInfinity = std::numeric_limits<T>::infinity();
        constexpr T kNaN = std::numeric_limits<T>::quiet_NaN();
        const std::vector<std::pair<T, T>> operands{
            {T{7}, T{2}}, {T{-7}, T{2}}, {T{7}, T{-2}}, {T{1}, T{0}}, {T{1}, -T{0}},
            {T{-1}, -T{0}}, {T{0}, T{0}}, {-T{0}, T{2}}, {T{0}, T{-2}}, {T{1}, -kInfinity},
            {kInfinity, T{-2}}, {kInfinity, kInfinity}, {kNaN, T{1}}, {T{1}, kNaN},
            {std::numeric_limits<T>::max(), T{0.5}}, {std::numeric_limits<T>::denorm_min(), T{2}},
            {-std::numeric_limits<T>::denorm_min(), T{2}}};
        for (const auto& [numerator, denominator] : operands)
        {
            const T expected = numerator / denominator;
            const Tensor<T> left({}, numerator);
            const Tensor<T> right({}, denominator);
            const auto binary = left / right;
            const auto scalarRight = left / denominator;
            const auto scalarLeft = numerator / right;
            for (const T actual : {binary[0], scalarRight[0], scalarLeft[0]})
            {
                if (std::isnan(expected))
                {
                    FATP_ASSERT_TRUE(std::isnan(actual), "Invalid floating quotients and NaNs preserve NaN category");
                }
                else
                {
                    FATP_ASSERT_TRUE(actual == expected && std::signbit(actual) == std::signbit(expected),
                                     "Floating division matches native value and sign, including zero and infinity");
                }
            }
        }
        const Tensor<int> integerOne({}, 1);
        const auto mixedZero = integerOne / -T{0};
        FATP_ASSERT_TRUE(std::isinf(mixedZero[0]) && std::signbit(mixedZero[0]),
                         "The floating result type selects zero-divisor semantics after promotion");
        const Tensor<int> integerZero({}, 0);
        const Tensor<T> floatingOne({}, T{1});
        const Tensor<T> floatingZero({}, T{0});
        const auto floatingOverIntegerZero = floatingOne / integerZero;
        const auto integerOverFloatingZero = integerOne / floatingZero;
        const auto floatingScalarOverIntegerZero = T{1} / integerZero;
        FATP_ASSERT_TRUE(std::isinf(floatingOverIntegerZero[0]) && floatingOverIntegerZero[0] > 0 &&
                         std::isinf(integerOverFloatingZero[0]) && integerOverFloatingZero[0] > 0 &&
                         std::isinf(floatingScalarOverIntegerZero[0]) && floatingScalarOverIntegerZero[0] > 0,
                         "An integer-source zero divisor is not a domain error for a promoted floating result");
    }
    return true;
}

FATP_TEST_CASE(division_floating_special_values)
{
    return verifyFloatingDivision<float>() && verifyFloatingDivision<double>() &&
           verifyFloatingDivision<long double>();
}

template <typename Right>
[[nodiscard]] bool verifyDivisionLayouts()
{
    using result_type = TensorArithmeticType<std::int16_t, Right>;
    std::mt19937 generator(0xD171DEU);
    std::uniform_int_distribution<int> valueDistribution(-9, 9);
    std::uniform_int_distribution<int> strideDistribution(-5, 5);
    for (std::size_t trial = 0; trial < 600; ++trial)
    {
        const std::size_t rows = trial % 5;
        const std::size_t columns = (trial / 5) % 5;
        const bool leftBroadcast = trial % 2 == 0;
        const auto leftLayout = makeSmallLayout({rows, leftBroadcast ? 1 : columns},
                                               TensorStrides{strideDistribution(generator),
                                                             strideDistribution(generator)});
        const auto rightLayout = makeSmallLayout({leftBroadcast ? columns : 1},
                                                TensorStrides{strideDistribution(generator)});
        std::vector<std::int16_t> leftStorage(leftLayout.storageLength());
        std::vector<Right> rightStorage(rightLayout.storageLength());
        for (auto& value : leftStorage)
        {
            const int sample = valueDistribution(generator);
            value = static_cast<std::int16_t>(sample == 0 ? 1 : sample);
        }
        for (auto& value : rightStorage)
        {
            const int sample = valueDistribution(generator);
            value = static_cast<Right>(sample == 0 ? 1 : sample);
        }
        const auto leftBefore = leftStorage;
        const auto rightBefore = rightStorage;
        const auto left = TensorView<const std::int16_t>::borrow(leftStorage.data(), leftLayout);
        const auto right = TensorView<const Right>::borrow(rightStorage.data(), rightLayout);
        const auto quotient = divide(left, right);
        const auto reverse = right / left;
        const auto scalarRight = left / Right{2};
        const auto scalarLeft = Right{6} / left;
        const DynamicExtents expected{rows, columns};
        FATP_ASSERT_TRUE(quotient.extents() == expected && reverse.extents() == expected,
                         "Division broadcasts both operand orders into canonical result extents");
        FATP_ASSERT_TRUE(scalarRight.extents() == left.extents() && scalarLeft.extents() == left.extents(),
                         "Scalar division preserves exact source extents");
        for (std::size_t index = 0; index < rows * columns; ++index)
        {
            const auto a = static_cast<result_type>(leftStorage[static_cast<std::size_t>(
                expectedBroadcastOffset(expected, leftLayout, index))]);
            const auto b = static_cast<result_type>(rightStorage[static_cast<std::size_t>(
                expectedBroadcastOffset(expected, rightLayout, index))]);
            FATP_ASSERT_EQ(quotient[index], static_cast<result_type>(a / b),
                           "Signed/overlapping/broadcast source offsets match the independent coordinate oracle");
            FATP_ASSERT_EQ(reverse[index], static_cast<result_type>(b / a), "Reverse division keeps operand order");
        }
        for (std::size_t index = 0; index < left.size(); ++index)
        {
            const auto a = static_cast<result_type>(leftStorage[static_cast<std::size_t>(
                expectedBroadcastOffset(left.extents(), leftLayout, index))]);
            FATP_ASSERT_EQ(scalarRight[index], static_cast<result_type>(a / result_type{2}),
                           "Scalar divisor uses the independent source mapping");
            FATP_ASSERT_EQ(scalarLeft[index], static_cast<result_type>(result_type{6} / a),
                           "Scalar numerator uses the independent source mapping");
        }
        FATP_ASSERT_TRUE(leftStorage == leftBefore && rightStorage == rightBefore,
                         "Division leaves both input storage domains unchanged");
    }
    return true;
}

FATP_TEST_CASE(division_layout_oracle)
{
    FATP_ASSERT_TRUE(verifyDivisionLayouts<std::int16_t>(), "Integer layout division must match its oracle");
    FATP_ASSERT_TRUE(verifyDivisionLayouts<double>(), "Mixed layout division must match its oracle");
    Tensor<int> owner({2, 2}, 2);
    owner[1] = 4;
    owner[2] = 8;
    const auto self = owner.asConstView() / owner.asSharedView();
    for (std::size_t index = 0; index < self.size(); ++index)
    {
        FATP_ASSERT_EQ(self[index], 1, "Overlapping readable operands may refer to the same elements");
    }
    const auto rankZero = Tensor<int>({}, 7) / Tensor<int>({}, 2);
    FATP_ASSERT_TRUE(rankZero.rank() == 0 && rankZero.size() == 1 && rankZero[0] == 3,
                     "Rank-zero division evaluates its single quotient");
    return true;
}

template <typename Left, typename Right, typename Allocator>
concept CanDivideWithAllocator = requires(const Left& left, const Right& right, const Allocator& allocator) {
    divide(left, right, allocator);
};

template <typename Left, typename Right>
concept CanDivide = requires(const Left& left, const Right& right) { divide(left, right); };

template <typename Source>
[[nodiscard]] consteval bool divisionSourceConstraints()
{
    using allocator = std::allocator<double>;
    static_assert(CanDivideWithAllocator<Source, Tensor<double>, allocator>);
    static_assert(CanDivideWithAllocator<Tensor<double>, Source, allocator>);
    static_assert(CanDivideWithAllocator<Source, double, allocator>);
    static_assert(CanDivideWithAllocator<double, Source, allocator>);
    static_assert(!CanDivideWithAllocator<Source, double, std::allocator<int>>);
    static_assert(!CanDivideWithAllocator<double, Source, std::allocator<int>>);
    static_assert(!CanDivideWithAllocator<Source, Tensor<double>, std::allocator<int>>);
    static_assert(!CanDivideWithAllocator<Source, double, int>);
    static_assert(!CanDivide<Source, bool> && !CanDivide<bool, Source>);
    static_assert(std::same_as<decltype(std::declval<Source>() / 2.0), Tensor<double>>);
    static_assert(std::same_as<decltype(2.0 / std::declval<Source>()), Tensor<double>>);
    return true;
}

FATP_TEST_CASE(division_constraints)
{
    static_assert(divisionSourceConstraints<Tensor<std::int16_t>>());
    static_assert(divisionSourceConstraints<TensorView<std::int16_t>>());
    static_assert(divisionSourceConstraints<TensorView<const std::int16_t>>());
    static_assert(divisionSourceConstraints<SharedTensorView<std::int16_t>>());
    static_assert(divisionSourceConstraints<SharedTensorView<const std::int16_t>>());
    static_assert(!CanDivide<int, int> && !CanDivide<double, double>);
    static_assert(!CanDivide<Tensor<bool>, int> && !CanDivide<int, Tensor<bool>>);
    static_assert(!CanDivide<Tensor<std::uint64_t>, int>);
    static_assert(!CanDivide<TensorView<const std::uint64_t>, std::int64_t>);
    static_assert(!CanDivide<std::int64_t, SharedTensorView<const std::uint64_t>>);
    enum class Code { One };
    static_assert(!CanDivide<Tensor<int>, Code> && !CanDivide<Code, Tensor<int>>);
    static_assert(!CanDivide<Tensor<int>, int*>);
    return true;
}

FATP_TEST_CASE(division_allocator_contract)
{
    CopyAllocationState leftState;
    CopyAllocationState rightState;
    CopyAllocationState explicitState;
    Tensor<std::int16_t, ArithmeticAllocator<std::int16_t>> left(
        std::allocator_arg, ArithmeticAllocator<std::int16_t>(leftState, 7), DynamicExtents{2}, std::int16_t{6});
    Tensor<double, ArithmeticAllocator<double>> right(
        std::allocator_arg, ArithmeticAllocator<double>(rightState, 9), DynamicExtents{2}, 2.0);
    const ArithmeticAllocator<double> allocator(explicitState, 23);
    {
        const auto binary = divide(left, right);
        const auto forward = left / 2.0;
        const auto reverse = 12.0 / left;
        const auto rightOwner = divide(left.asConstView(), right);
        const auto viewOnly = left.asSharedView() / right.asConstView();
        static_assert(std::same_as<typename decltype(binary)::allocator_type, ArithmeticAllocator<double>>);
        static_assert(std::same_as<typename decltype(viewOnly)::allocator_type, TensorAllocator<double>>);
        FATP_ASSERT_EQ(binary.get_allocator().id(), 1007, "Binary division rebinds the first owner before SOCCC");
        FATP_ASSERT_EQ(forward.get_allocator().id(), 1007, "Scalar division selects the tensor owner's allocator");
        FATP_ASSERT_EQ(reverse.get_allocator().id(), 1007, "A scalar numerator does not hide the tensor owner");
        FATP_ASSERT_EQ(rightOwner.get_allocator().id(), 1009, "A right owner supplies the allocator after a view");
        FATP_ASSERT_EQ(leftState.allocations, std::size_t{4}, "Exactly one result element buffer per owner call");
        FATP_ASSERT_EQ(rightState.allocations, std::size_t{2}, "The second owner is selected only after a view");
        FATP_ASSERT_EQ(leftState.lastCount, std::size_t{2}, "No temporary scalar Tensor element buffer is allocated");
        const auto explicitBinary = divide(left, right, allocator);
        const auto explicitForward = divide(left.asConstView(), 2.0, allocator);
        const auto explicitReverse = divide(12.0, left.asSharedView(), allocator);
        FATP_ASSERT_EQ(explicitBinary.get_allocator().id(), 23, "Explicit binary allocator bypasses owner selection");
        FATP_ASSERT_EQ(explicitForward.get_allocator().id(), 23, "Borrowed scalar division uses the exact allocator");
        FATP_ASSERT_EQ(explicitReverse.get_allocator().id(), 23, "Shared reverse division uses the exact allocator");
        FATP_ASSERT_EQ(explicitState.allocations, std::size_t{3}, "Each explicit call allocates one element buffer");
        FATP_ASSERT_TRUE(binary[0] == 3.0 && reverse[0] == 2.0 && explicitReverse[0] == 2.0,
                         "Allocator selection preserves quotient and operand order");
    }
    FATP_ASSERT_EQ(leftState.deallocations, std::size_t{3}, "Completed division result buffers are reclaimed");
    FATP_ASSERT_EQ(rightState.deallocations, std::size_t{1}, "The right-owner result is reclaimed");
    FATP_ASSERT_EQ(explicitState.allocations, explicitState.deallocations, "Explicit division storage is reclaimed");
    Tensor<std::int16_t, ArithmeticAllocator<std::int16_t>> empty(
        std::allocator_arg, ArithmeticAllocator<std::int16_t>(leftState, 11), DynamicExtents{0, 2});
    const auto before = leftState.attempts;
    const auto emptyBinary = empty / right;
    const auto emptyScalar = empty / std::int16_t{0};
    FATP_ASSERT_TRUE(emptyBinary.empty() && emptyScalar.empty(), "Empty division evaluates no zero divisor");
    FATP_ASSERT_EQ(emptyBinary.get_allocator().id(), 1011, "Empty floating result uses rebound SOCCC");
    FATP_ASSERT_EQ(emptyScalar.get_allocator().id(), 111, "Empty integer result retains integer SOCCC");
    FATP_ASSERT_EQ(leftState.attempts, before, "Empty division allocates no result elements");
    return true;
}

FATP_TEST_CASE(division_failure_and_lifetime)
{
    CopyAllocationState state;
    state.fail = true;
    const CopyAllocator<int> allocator(state, 11);
    Tensor<int> numerator({2}, 6);
    Tensor<int> denominator({2}, 2);
    const Tensor<int> incompatible({3}, 0);
    FATP_ASSERT_THROWS(divide(numerator, incompatible, allocator), std::invalid_argument,
                       "Broadcast shape errors precede result allocation and divisor errors");
    const Tensor<int> incompatibleEmpty({0, 3});
    const Tensor<int> otherEmpty({0, 2});
    FATP_ASSERT_THROWS(divide(incompatibleEmpty, otherEmpty, allocator), std::invalid_argument,
                       "Zero extents do not hide incompatible trailing broadcast dimensions");
#ifndef NDEBUG
    TensorView<int> expired;
    TensorView<int> expiredEmpty;
    {
        Tensor<int> temporary({2}, 2);
        Tensor<int> temporaryEmpty({0, 2});
        expired = temporary.asView();
        expiredEmpty = temporaryEmpty.asView();
    }
    FATP_ASSERT_THROWS(divide(expired, denominator, allocator), std::runtime_error,
                       "An expired numerator is rejected before allocation");
    FATP_ASSERT_THROWS(divide(numerator, expired, allocator), std::runtime_error,
                       "An expired denominator is rejected before allocation");
    FATP_ASSERT_THROWS(divide(expired, 0, allocator), std::runtime_error,
                       "Scalar zero does not hide an expired tensor lifetime");
    FATP_ASSERT_THROWS(divide(6, expired, allocator), std::runtime_error,
                       "Reverse division validates its borrowed source");
    FATP_ASSERT_THROWS(divide(expiredEmpty, 0, allocator), std::runtime_error,
                       "Empty borrowed inputs still undergo lifetime validation");
#endif
    FATP_ASSERT_EQ(state.attempts, std::size_t{0}, "Invalid shape or lifetime never allocates result elements");
    denominator[1] = 0;
    FATP_ASSERT_THROWS(divide(numerator, denominator, allocator), std::bad_alloc,
                       "Output allocation failure may precede a later integer division error");
    FATP_ASSERT_THROWS(divide(numerator, 0, allocator), std::bad_alloc,
                       "Scalar divisors are checked during element evaluation, not pre-scanned");
    FATP_ASSERT_THROWS(divide(6, denominator, allocator), std::bad_alloc,
                       "Reverse scalar result allocation failure propagates");
    const Tensor<int> empty({0, 2});
    const Tensor<int> zero({}, 0);
    const auto emptyScalar = divide(empty, 0, allocator);
    const auto emptyReverse = divide(0, empty, allocator);
    const auto emptyBinary = divide(empty, zero, allocator);
    const auto emptyBinaryReverse = divide(zero, empty, allocator);
    FATP_ASSERT_TRUE(emptyScalar.empty() && emptyReverse.empty() && emptyBinary.empty() && emptyBinaryReverse.empty(),
                     "Empty output evaluates neither integer zero division nor allocation failure");
    state.fail = false;
    FATP_ASSERT_THROWS(divide(numerator, denominator, allocator), std::domain_error,
                       "A later zero divisor discards a partially computed binary result");
    FATP_ASSERT_THROWS(divide(6, denominator, allocator), std::domain_error,
                       "Reverse scalar division discards its result after a later zero divisor");
    FATP_ASSERT_THROWS(divide(numerator, 0, allocator), std::domain_error,
                       "A scalar zero divisor also releases the unpublished result");
    FATP_ASSERT_TRUE(numerator[0] == 6 && numerator[1] == 6 && denominator[0] == 2 && denominator[1] == 0,
                     "Domain errors leave every input element unchanged");
    numerator[1] = std::numeric_limits<int>::lowest();
    denominator[1] = -1;
    FATP_ASSERT_THROWS(divide(numerator, denominator, allocator), std::overflow_error,
                       "Later signed quotient overflow cleans up binary output");
    FATP_ASSERT_THROWS(divide(numerator, -1, allocator), std::overflow_error,
                       "Later signed quotient overflow cleans up scalar output");
    FATP_ASSERT_THROWS(divide(std::numeric_limits<int>::lowest(), denominator, allocator), std::overflow_error,
                       "Later signed quotient overflow cleans up reverse scalar output");
    FATP_ASSERT_EQ(state.allocations, state.deallocations, "Every failed quotient releases its result buffer");
    FATP_ASSERT_TRUE(numerator[0] == 6 && numerator[1] == std::numeric_limits<int>::lowest() &&
                     denominator[0] == 2 && denominator[1] == -1, "Overflow leaves all inputs unchanged");
    SharedTensorView<int> retained;
    {
        Tensor<int> temporary({2}, 2);
        retained = temporary.asSharedView();
    }
    const auto retainedBinary = numerator / retained;
    const auto retainedScalar = retained / 2;
    const auto retainedReverse = 8 / retained;
    FATP_ASSERT_TRUE(retainedBinary[0] == 3 && retainedScalar[0] == 1 && retainedReverse[0] == 4,
                     "Shared numerator and denominator sources survive owner destruction");
    return true;
}


template <typename Destination, typename Operand, typename... Allocator>
Destination& runCompound(int operation, Destination& destination, const Operand& operand,
                         const Allocator&... allocator)
{
    switch (operation)
    {
        case 0: return addAssign(destination, operand, allocator...);
        case 1: return subtractAssign(destination, operand, allocator...);
        case 2: return multiplyAssign(destination, operand, allocator...);
        default: return divideAssign(destination, operand, allocator...);
    }
}

template <typename Destination, typename Operand, typename Allocator>
[[nodiscard]] consteval bool compoundAllocatorRejected()
{
    static_assert(!requires(Destination& a, const Operand& b, const Allocator& alloc) { addAssign(a, b, alloc); });
    static_assert(!requires(Destination& a, const Operand& b, const Allocator& alloc) { subtractAssign(a, b, alloc); });
    static_assert(!requires(Destination& a, const Operand& b, const Allocator& alloc) { multiplyAssign(a, b, alloc); });
    static_assert(!requires(Destination& a, const Operand& b, const Allocator& alloc) { divideAssign(a, b, alloc); });
    return true;
}

template <typename Destination>
[[nodiscard]] consteval bool compoundTemporaryRejected()
{
    static_assert(!requires(Destination&& a) { addAssign(std::move(a), 2); });
    static_assert(!requires(Destination&& a) { subtractAssign(std::move(a), 2); });
    static_assert(!requires(Destination&& a) { multiplyAssign(std::move(a), 2); });
    static_assert(!requires(Destination&& a) { divideAssign(std::move(a), 2); });
    static_assert(!requires(Destination&& a) { std::move(a) += 2; });
    static_assert(!requires(Destination&& a) { std::move(a) -= 2; });
    static_assert(!requires(Destination&& a) { std::move(a) *= 2; });
    static_assert(!requires(Destination&& a) { std::move(a) /= 2; });
    return true;
}

FATP_TEST_CASE(compound_constraints)
{
    static_assert(compoundAvailability<Tensor<int>, double, true>());
    static_assert(compoundAvailability<TensorView<int>, TensorView<const double>, true>());
    static_assert(compoundAvailability<SharedTensorView<int>, SharedTensorView<const double>, true>());
    static_assert(compoundAvailability<TensorView<int>, int, true>());
    static_assert(compoundAvailability<SharedTensorView<int>, double, true>());
    static_assert(compoundAvailability<TensorView<const int>, int, false>());
    static_assert(compoundAvailability<SharedTensorView<const int>, Tensor<int>, false>());
    static_assert(compoundAvailability<const Tensor<int>, int, false>());
    static_assert(compoundAvailability<const TensorView<int>, int, false>());
    static_assert(compoundAvailability<const SharedTensorView<int>, int, false>());
    static_assert(compoundAvailability<Tensor<int>, bool, false>());
    static_assert(compoundAvailability<Tensor<int>, int*, false>());
    enum class Code { One };
    static_assert(compoundAvailability<Tensor<int>, Code, false>());
    static_assert(compoundAllocatorRejected<Tensor<int>, double, std::allocator<double>>());
    static_assert(compoundAllocatorRejected<TensorView<int>, Tensor<double>, std::allocator<double>>());
    static_assert(compoundAllocatorRejected<SharedTensorView<int>, double, int>());
    static_assert(compoundTemporaryRejected<Tensor<int>>());
    static_assert(compoundTemporaryRejected<TensorView<int>>());
    static_assert(compoundTemporaryRejected<SharedTensorView<int>>());
    // Named tensor arithmetic must never capture native scalar compound assignment.
    static_assert(compoundAllocatorRejected<int, int, std::allocator<int>>());
    Tensor<int> owner({2}, 4);
    auto view = owner.asView();
    auto shared = owner.asSharedView();
    FATP_ASSERT_TRUE(&(owner += 2) == &owner && &(view -= 1) == &view &&
                     &(shared *= 2) == &shared && &(owner /= 2) == &owner,
                     "Every operator returns the original owner or view");
    FATP_ASSERT_TRUE(&addAssign(subtractAssign(owner, 1), 2) == &owner,
                     "Named compound operations chain using the original destination");
    FATP_ASSERT_EQ(owner[0], 6, "Chained owner and view updates retain write-through behavior");
    return true;
}

FATP_TEST_CASE(compound_broadcast_and_storage)
{
    Tensor<int> owner({2, 3}, 12);
    auto borrowed = owner.asView();
    auto shared = owner.asSharedView();
    auto* const originalData = owner.data();
    const auto originalStrides = owner.strides();
    const Tensor<int> row({1, 3}, 2);
    owner += row;
    owner -= row.asConstView();
    owner *= row.asSharedView();
    owner /= row;
    FATP_ASSERT_TRUE(owner.data() == originalData && owner.extents() == DynamicExtents({2, 3}) &&
                     owner.strides() == originalStrides, "Compound operations preserve storage and shape");
    FATP_ASSERT_TRUE(borrowed[5] == 12 && shared[0] == 12, "Existing borrowed and shared views remain valid");
    const Tensor<int> rankZeroRight({}, 2);
    owner += rankZeroRight;
    owner -= rankZeroRight;
    owner *= rankZeroRight;
    owner /= rankZeroRight;
    FATP_ASSERT_TRUE(borrowed[5] == 12 && shared[0] == 12,
                     "Rank-zero RHS broadcasting pads every axis of the destination");
    Tensor<int> scalar({}, 8);
    const Tensor<int> scalarRight({}, 2);
    scalar /= scalarRight;
    scalar += 1;
    FATP_ASSERT_TRUE(scalar.rank() == 0 && scalar.size() == 1 && scalar[0] == 5,
                     "Rank zero updates exactly one element");
    const Tensor<int> rankGrowing({1, 2, 3}, 1);
    const Tensor<int> shapeGrowing({4, 3}, 1);
    Tensor<int> singleton({1, 3}, 1);
    const Tensor<int> emptyRight({0, 3});
    for (int operation = 0; operation < 4; ++operation)
    {
        FATP_ASSERT_THROWS(runCompound(operation, owner, rankGrowing), std::invalid_argument,
                           "Even leading singleton RHS axes cannot change destination rank");
        FATP_ASSERT_THROWS(runCompound(operation, owner, shapeGrowing), std::invalid_argument,
                           "Incompatible broadcast shapes are rejected");
        FATP_ASSERT_THROWS(runCompound(operation, singleton, owner), std::invalid_argument,
                           "The destination is never broadcast to a larger output");
        FATP_ASSERT_THROWS(runCompound(operation, singleton, emptyRight), std::invalid_argument,
                           "Broadcast cannot shrink a nonempty destination to an empty output");
    }
    FATP_ASSERT_TRUE(owner[0] == 12 && owner[5] == 12 && singleton[0] == 1,
                     "Shape rejection never changes the destination");
    Tensor<int> empty({0, 3});
    const Tensor<int> zero({}, 0);
    for (int operation = 0; operation < 4; ++operation)
    {
        FATP_ASSERT_TRUE(&runCompound(operation, empty, zero) == &empty &&
                         &runCompound(operation, empty, 0) == &empty,
                         "Valid empty operations return their destination");
    }
    FATP_ASSERT_TRUE(empty.extents() == DynamicExtents({0, 3}), "Empty destination shape is preserved");
    return true;
}

FATP_TEST_CASE(compound_checked_conversion_rollback)
{
    for (int operation = 0; operation < 4; ++operation)
    {
        Tensor<std::int8_t> destination({2}, std::int8_t{6});
        destination[1] = operation == 1 || operation == 3 ? std::int8_t{-128} : std::int8_t{127};
        const auto original = clone(destination);
        const int scalar = operation == 3 ? -1 : (operation == 2 ? 2 : 1);
        const Tensor<int> operand({2}, scalar);
        FATP_ASSERT_THROWS(runCompound(operation, destination, scalar), std::overflow_error,
                           "Scalar result conversion checks destination range after valid earlier values");
        FATP_ASSERT_TRUE(exactEqual(destination, original), "A later scalar narrowing error rolls back all values");
        FATP_ASSERT_THROWS(runCompound(operation, destination, operand), std::overflow_error,
                           "Tensor result conversion checks the narrower destination range");
        FATP_ASSERT_TRUE(exactEqual(destination, original), "A later tensor narrowing error rolls back all values");

        Tensor<int> integers({2}, 4);
        integers[1] = 5;
        Tensor<double> fractions({2}, 2.0);
        fractions[1] = operation == 3 ? 2.0 : 0.5;
        FATP_ASSERT_THROWS(runCompound(operation, integers, fractions), std::domain_error,
                           "Fractional results are rejected instead of silently truncated on write-back");
        FATP_ASSERT_TRUE(integers[0] == 4 && integers[1] == 5, "A fractional later result rolls back earlier results");
    }
    Tensor<int> integers({2}, 7);
    integers /= 2;
    FATP_ASSERT_EQ(integers[0], 3, "Integral compute-type division truncates toward zero");
    FATP_ASSERT_THROWS(integers /= 2.0, std::domain_error, "Floating compute-type division uses checked conversion");
    Tensor<int> zeros({2}, 2);
    zeros[1] = 0;
    FATP_ASSERT_THROWS(integers /= zeros, std::domain_error, "A later zero divisor leaves all elements untouched");
    FATP_ASSERT_TRUE(integers[0] == 3 && integers[1] == 3, "Division failures do not publish partial values");
    integers[1] = std::numeric_limits<int>::max();
    FATP_ASSERT_THROWS(integers += 1, std::overflow_error, "Same-type checked addition never invokes overflow");
    FATP_ASSERT_TRUE(integers[0] == 3 && integers[1] == std::numeric_limits<int>::max(),
                     "Checked operation overflow preserves the destination");
    integers[1] = std::numeric_limits<int>::lowest();
    FATP_ASSERT_THROWS(integers /= -1, std::overflow_error, "Same-type signed minimum division is checked");
    FATP_ASSERT_TRUE(integers[0] == 3 && integers[1] == std::numeric_limits<int>::lowest(),
                     "Signed quotient overflow rolls back earlier results");
    Tensor<unsigned> unsignedValues({2}, 1u);
    unsignedValues[1] = 0u;
    FATP_ASSERT_THROWS(unsignedValues -= 1u, std::overflow_error, "Unsigned subtraction cannot wrap");
    FATP_ASSERT_TRUE(unsignedValues[0] == 1u && unsignedValues[1] == 0u, "Unsigned underflow is transactional");
    Tensor<float> floats({2}, 1.0F);
    Tensor<double> large({2}, 0.0);
    large[1] = 1e39;
    FATP_ASSERT_THROWS(floats += large, std::overflow_error, "Finite floating back-conversion checks range");
    FATP_ASSERT_TRUE(floats[0] == 1.0F && floats[1] == 1.0F, "Floating range failure preserves all values");
    floats += 0.1;
    FATP_ASSERT_EQ(floats[0], static_cast<float>(1.1), "In-range floating write-back allows rounding");
    if constexpr (std::numeric_limits<float>::is_iec559 && std::numeric_limits<double>::is_iec559)
    {
        floats *= std::numeric_limits<double>::infinity();
        FATP_ASSERT_TRUE(std::isinf(floats[0]), "Floating write-back preserves infinity category");
        floats += std::numeric_limits<double>::quiet_NaN();
        FATP_ASSERT_TRUE(std::isnan(floats[1]), "Floating write-back preserves NaN category");
        FATP_ASSERT_THROWS(integers += std::numeric_limits<double>::infinity(), std::domain_error,
                           "Non-finite floating results cannot be written into integers");
    }
    Tensor<std::uint64_t> wide({1}, std::uint64_t{3});
    wide += 1u;
    FATP_ASSERT_EQ(wide[0], std::uint64_t{4}, "Unsigned literals support full-width unsigned destinations");
    return true;
}

FATP_TEST_CASE(compound_aliasing_snapshots)
{
    for (bool reverse : {false, true})
    {
        std::vector<int> storage{2, 4, 6, 8, 10};
        auto destination = TensorView<int>::borrow(
            storage.data(), TensorLayout(storage.size(), reverse ? 0 : 1, DynamicExtents{4}, TensorStrides{1}));
        const auto source = TensorView<const int>::borrow(
            storage.data(), TensorLayout(storage.size(), reverse ? 1 : 0, DynamicExtents{4}, TensorStrides{1}));
        destination += source;
        const std::vector<int> expected = reverse ? std::vector<int>{6, 10, 14, 18, 10} :
                                                   std::vector<int>{2, 6, 10, 14, 18};
        FATP_ASSERT_TRUE(storage == expected, "Shifted overlap reads a complete pre-update snapshot");
    }
    Tensor<int> square({2, 2});
    for (std::size_t index = 0; index < square.size(); ++index)
    {
        square[index] = static_cast<int>(index + 1);
    }
    const auto transposed = square.transposeView();
    square += transposed;
    FATP_ASSERT_TRUE(square[0] == 2 && square[1] == 5 && square[2] == 5 && square[3] == 8,
                     "A transpose alias cannot observe partially updated rows");
    square += square;
    FATP_ASSERT_TRUE(square[0] == 4 && square[3] == 16, "Exact self aliasing is supported");
    Tensor<int> matrix({2, 3});
    for (std::size_t index = 0; index < matrix.size(); ++index)
    {
        matrix[index] = static_cast<int>(index + 1);
    }
    const auto firstRow = matrix.rowView(0);
    matrix *= firstRow;
    FATP_ASSERT_TRUE(matrix[0] == 1 && matrix[1] == 4 && matrix[2] == 9 &&
                     matrix[3] == 4 && matrix[4] == 10 && matrix[5] == 18,
                     "Broadcast aliases use original row values for every destination row");
    Tensor<int> values({3});
    values[0] = 2;
    values[1] = 3;
    values[2] = 4;
    values += values[0];
    FATP_ASSERT_TRUE(values[0] == 4 && values[1] == 5 && values[2] == 6,
                     "An aliased scalar is captured before destination writes");
    return true;
}

template <typename Right>
[[nodiscard]] bool compoundLayoutOracle()
{
    std::mt19937 generator(0xCA550001u);
    std::uniform_int_distribution<int> valueDistribution(1, 9);
    std::uniform_int_distribution<int> strideDistribution(-4, 4);
    for (std::size_t trial = 0; trial < 600; ++trial)
    {
        const auto rows = trial % 5;
        const auto columns = (trial / 5) % 5;
        const auto rowStride = static_cast<std::ptrdiff_t>(columns + 1) * ((trial & 1u) != 0u ? -1 : 1);
        const std::ptrdiff_t columnStride = (trial & 2u) != 0u ? -1 : 1;
        const auto destinationLayout = makeSmallLayout({rows, columns}, {rowStride, columnStride});
        const bool columnBroadcast = (trial & 4u) != 0u;
        const bool rowBroadcast = (trial & 8u) != 0u;
        const auto rightLayout = makeSmallLayout({rowBroadcast ? 1 : rows, columnBroadcast ? 1 : columns},
                                                  {strideDistribution(generator), strideDistribution(generator)});
        std::vector<int> original(destinationLayout.storageLength(), -77);
        for (std::size_t index = 0; index < destinationLayout.logicalSize(); ++index)
        {
            const auto offset = static_cast<std::size_t>(
                expectedBroadcastOffset(destinationLayout.extents(), destinationLayout, index));
            original[offset] = valueDistribution(generator);
        }
        std::vector<Right> rightStorage(rightLayout.storageLength());
        for (auto& value : rightStorage)
        {
            value = static_cast<Right>(valueDistribution(generator));
            if constexpr (std::floating_point<Right>)
            {
                value /= 2.0;
            }
        }
        const auto originalRight = rightStorage;
        const auto right = TensorView<const Right>::borrow(rightStorage.data(), rightLayout);
        for (int mode = 0; mode < 8; ++mode)
        {
            const int operation = mode % 4;
            const bool scalarRhs = mode >= 4;
            Right scalarValue = static_cast<Right>(trial % 5 + 1);
            if constexpr (std::floating_point<Right>)
            {
                scalarValue /= 2.0;
            }
            auto storage = original;
            auto expected = original;
            bool fractional = false;
            for (std::size_t index = 0; index < destinationLayout.logicalSize(); ++index)
            {
                const auto destinationOffset = static_cast<std::size_t>(
                    expectedBroadcastOffset(destinationLayout.extents(), destinationLayout, index));
                const auto rightOffset = static_cast<std::size_t>(
                    expectedBroadcastOffset(destinationLayout.extents(), rightLayout, index));
                // Independent native arithmetic on bounded values; no production numeric/iteration helpers.
                const auto left = static_cast<Right>(original[destinationOffset]);
                const auto rhs = scalarRhs ? scalarValue : rightStorage[rightOffset];
                Right result{};
                switch (operation)
                {
                    case 0: result = left + rhs; break;
                    case 1: result = left - rhs; break;
                    case 2: result = left * rhs; break;
                    default: result = left / rhs; break;
                }
                if constexpr (std::floating_point<Right>)
                {
                    fractional = fractional || std::trunc(result) != result;
                }
                expected[destinationOffset] = static_cast<int>(result);
            }
            auto destination = TensorView<int>::borrow(storage.data(), destinationLayout);
            const auto update = [&] {
                if (scalarRhs)
                {
                    runCompound(operation, destination, scalarValue);
                }
                else
                {
                    runCompound(operation, destination, right);
                }
            };
            if (fractional)
            {
                FATP_ASSERT_THROWS(update(), std::domain_error,
                                   "The independent oracle predicts a non-integral back-conversion");
                FATP_ASSERT_TRUE(storage == original, "Fractional failure preserves every logical and padding value");
            }
            else
            {
                update();
                FATP_ASSERT_TRUE(storage == expected, "Signed/padded destination matches the coordinate oracle");
            }
            FATP_ASSERT_TRUE(rightStorage == originalRight, "The independent RHS storage remains untouched");
        }
    }
    return true;
}

FATP_TEST_CASE(compound_layout_oracle)
{
    FATP_ASSERT_TRUE(compoundLayoutOracle<int>(), "Integral compound layout oracle");
    FATP_ASSERT_TRUE(compoundLayoutOracle<double>(), "Mixed floating/integer compound layout oracle");
    return true;
}

FATP_TEST_CASE(compound_allocator_contract)
{
    CopyAllocationState destinationState;
    CopyAllocationState rightState;
    CopyAllocationState explicitState;
    Tensor<std::int16_t, ArithmeticAllocator<std::int16_t>> destination(
        std::allocator_arg, ArithmeticAllocator<std::int16_t>(destinationState, 7),
        DynamicExtents{2}, std::int16_t{16});
    Tensor<double, ArithmeticAllocator<double>> right(
        std::allocator_arg, ArithmeticAllocator<double>(rightState, 9), DynamicExtents{2}, 2.0);
    auto* const originalData = destination.data();
    rightState.fail = true;
    const ArithmeticAllocator<std::int16_t> allocator(explicitState, 23);
    auto view = destination.asView();
    auto shared = destination.asSharedView();
    for (int operation = 0; operation < 4; ++operation)
    {
        const auto before = destinationState.allocations;
        runCompound(operation, destination, right);
        FATP_ASSERT_EQ(destinationState.allocations, before + 1, "Each nonempty update uses one scratch buffer");
        FATP_ASSERT_EQ(destinationState.lastId, 7, "Destination allocator is neither rebound nor passed through SOCCC");
        FATP_ASSERT_EQ(destinationState.lastCount, std::size_t{2}, "Scratch has destination element count");
        runCompound(operation, view, 2.0, allocator);
        runCompound(operation, shared, right.asConstView(), allocator);
        runCompound(operation, destination, 2.0, allocator);
    }
    FATP_ASSERT_EQ(rightState.attempts, std::size_t{1}, "The RHS allocator is never selected for scratch");
    FATP_ASSERT_EQ(explicitState.allocations, std::size_t{12}, "All explicit forms use one destination-typed buffer");
    FATP_ASSERT_EQ(explicitState.lastId, 23, "The supplied scratch allocator instance is used unchanged");
    FATP_ASSERT_EQ(explicitState.allocations, explicitState.deallocations,
                   "Scratch storage never escapes the operation");
    FATP_ASSERT_EQ(destinationState.allocations, destinationState.deallocations + 1,
                   "Only the destination's original element storage survives");
    const auto beforeViewDefault = destinationState.attempts;
    view += 2.0;
    shared -= 2.0;
    view *= 2.0;
    shared /= 2.0;
    FATP_ASSERT_EQ(destinationState.attempts, beforeViewDefault,
                   "Default view scratch cannot reach the backing owner's allocator");
    FATP_ASSERT_TRUE(destination.data() == originalData && destination.get_allocator().id() == 7,
                     "Compound operations do not swap, reallocate, or replace the destination allocator");
    Tensor<std::int16_t, ArithmeticAllocator<std::int16_t>> empty(
        std::allocator_arg, ArithmeticAllocator<std::int16_t>(destinationState, 11), DynamicExtents{0, 2});
    const auto before = destinationState.attempts;
    destinationState.fail = true;
    explicitState.fail = true;
    for (int operation = 0; operation < 4; ++operation)
    {
        runCompound(operation, empty, right);
        runCompound(operation, empty, 0);
        runCompound(operation, empty, 0, allocator);
    }
    FATP_ASSERT_EQ(destinationState.attempts, before, "Empty updates allocate no scratch elements");
    FATP_ASSERT_EQ(explicitState.allocations, std::size_t{12}, "Empty explicit calls allocate no elements");
    const auto originalValue = destination[0];
    for (int operation = 0; operation < 4; ++operation)
    {
        FATP_ASSERT_THROWS(runCompound(operation, destination, right), std::bad_alloc,
                           "Default owner scratch allocation failures propagate");
        FATP_ASSERT_TRUE(destination[0] == originalValue && destination[1] == originalValue &&
                         destination.data() == originalData && shared[1] == originalValue,
                         "Default allocation failure preserves owner storage and existing views");
    }
    return true;
}

FATP_TEST_CASE(compound_failure_and_lifetime)
{
    CopyAllocationState state;
    state.fail = true;
    const CopyAllocator<int> allocator(state, 11);
    Tensor<int> destination({2}, 6);
    const Tensor<int> right({2}, 2);
    const Tensor<int> incompatible({3}, 1);
    const Tensor<int> rankGrowing({1, 2}, 1);
    const Tensor<int> emptyMismatch({0, 3});
    Tensor<int> empty({0, 2});
    for (int operation = 0; operation < 4; ++operation)
    {
        FATP_ASSERT_THROWS(runCompound(operation, destination, incompatible, allocator), std::invalid_argument,
                           "Broadcast validation precedes element allocation");
        FATP_ASSERT_THROWS(runCompound(operation, destination, rankGrowing, allocator), std::invalid_argument,
                           "Destination-changing shape rejection precedes element allocation");
        FATP_ASSERT_THROWS(runCompound(operation, empty, emptyMismatch, allocator), std::invalid_argument,
                           "Empty inputs still require compatible dimensions");
    }
#ifndef NDEBUG
    TensorView<int> expired;
    TensorView<int> expiredEmpty;
    {
        Tensor<int> temporary({2}, 2);
        Tensor<int> temporaryEmpty({0, 2});
        expired = temporary.asView();
        expiredEmpty = temporaryEmpty.asView();
    }
    for (int operation = 0; operation < 4; ++operation)
    {
        FATP_ASSERT_THROWS(runCompound(operation, expired, 2, allocator), std::runtime_error,
                           "An expired destination is rejected before scratch element allocation");
        FATP_ASSERT_THROWS(runCompound(operation, expired, right, allocator), std::runtime_error,
                           "A tensor RHS does not mask an expired destination");
        FATP_ASSERT_THROWS(runCompound(operation, destination, expired, allocator), std::runtime_error,
                           "An expired RHS is rejected before scratch element allocation");
        FATP_ASSERT_THROWS(runCompound(operation, expiredEmpty, 0, allocator), std::runtime_error,
                           "Empty destinations still undergo lifetime validation");
        FATP_ASSERT_THROWS(runCompound(operation, empty, expiredEmpty, allocator), std::runtime_error,
                           "Empty RHS operands still undergo lifetime validation");
    }
#endif
    FATP_ASSERT_EQ(state.attempts, std::size_t{0}, "Invalid shape and lifetime allocate no scratch elements");
    auto* const originalData = destination.data();
    auto borrowed = destination.asView();
    for (int operation = 0; operation < 4; ++operation)
    {
        FATP_ASSERT_THROWS(runCompound(operation, destination, right, allocator), std::bad_alloc,
                           "Tensor scratch allocation failure propagates");
        FATP_ASSERT_THROWS(runCompound(operation, destination, 2, allocator), std::bad_alloc,
                           "Scalar scratch allocation failure propagates");
        FATP_ASSERT_TRUE(destination[0] == 6 && destination[1] == 6 &&
                         destination.data() == originalData && borrowed[1] == 6,
                         "Allocation failure preserves values, address, and existing views");
    }
    state.fail = false;
    Tensor<double> fractional({2}, 2.0);
    fractional[1] = 0.25;
    for (int operation = 0; operation < 3; ++operation)
    {
        FATP_ASSERT_THROWS(runCompound(operation, destination, fractional, allocator), std::domain_error,
                           "A later numeric error releases the partially computed scratch");
    }
    Tensor<int> divisors({2}, 2);
    divisors[1] = 0;
    FATP_ASSERT_THROWS(divideAssign(destination, divisors, allocator), std::domain_error,
                       "A later zero divisor also releases scratch");
    FATP_ASSERT_EQ(state.allocations, state.deallocations, "Every failed update reclaims scratch element storage");
    FATP_ASSERT_TRUE(destination[0] == 6 && destination[1] == 6, "Cleanup leaves the original destination untouched");
    SharedTensorView<int> retained;
    {
        Tensor<int> temporary({2}, 8);
        retained = temporary.asSharedView();
    }
    retained /= 2;
    destination += retained;
    FATP_ASSERT_TRUE(retained[0] == 4 && destination[0] == 10,
                     "Shared destinations and sources remain usable after owner destruction");
    return true;
}

template <typename T>
[[nodiscard]] bool compoundIntegerType()
{
    Tensor<T> values({2}, T{6});
    values += T{2};
    values -= T{1};
    values *= T{2};
    values /= T{2};
    FATP_ASSERT_TRUE(values[0] == T{7} && values[1] == T{7}, "Every standard integer supports compound arithmetic");
    values[1] = std::numeric_limits<T>::max();
    FATP_ASSERT_THROWS(values += T{1}, std::overflow_error, "Integer maximum cannot wrap during compound addition");
    FATP_ASSERT_TRUE(values[0] == T{7} && values[1] == std::numeric_limits<T>::max(),
                     "Integer overflow preserves earlier values for every standard integer type");
    if constexpr (std::is_signed_v<T>)
    {
        values[1] = std::numeric_limits<T>::lowest();
        FATP_ASSERT_THROWS(values /= T{-1}, std::overflow_error, "Narrow and character signed division is checked");
    }
    else
    {
        values[1] = T{0};
        FATP_ASSERT_THROWS(values -= T{1}, std::overflow_error, "Unsigned character subtraction cannot wrap");
    }
    FATP_ASSERT_TRUE(values[0] == T{7}, "Late integer errors do not change earlier elements");
    return true;
}

FATP_TEST_CASE(compound_integer_types)
{
    const bool verified = compoundIntegerType<char>() && compoundIntegerType<signed char>() &&
        compoundIntegerType<unsigned char>() && compoundIntegerType<wchar_t>() && compoundIntegerType<char8_t>() &&
        compoundIntegerType<char16_t>() && compoundIntegerType<char32_t>() && compoundIntegerType<short>() &&
        compoundIntegerType<unsigned short>() && compoundIntegerType<int>() && compoundIntegerType<unsigned>() &&
        compoundIntegerType<long>() && compoundIntegerType<unsigned long>() && compoundIntegerType<long long>() &&
        compoundIntegerType<unsigned long long>();
    FATP_ASSERT_TRUE(verified, "Compound integer and character boundaries");
    return true;
}

#if defined(ENABLE_TEST_APPLICATION) && !defined(FATP_TENSOR_DISABLE_ALLOCATION_PROBE) && \
    (!defined(_MSC_VER) || _ITERATOR_DEBUG_LEVEL == 0)
FATP_TEST_CASE(compound_allocation_failure_transaction)
{
    for (int shape = 0; shape < 4; ++shape)
    {
        for (bool scalarRhs : {false, true})
        {
            for (int operation = 0; operation < 4; ++operation)
            {
                bool reachedSuccess = false;
                for (std::ptrdiff_t failure = 0; failure < 256; ++failure)
                {
                    std::array<int, 8> storage{11, 22, 33, 44, 55, 66, 77, 88};
                    const auto original = storage;
                    const auto extents = shape == 2 ? DynamicExtents{} : DynamicExtents{shape == 3 ? 0u : 2u, 2};
                    const auto strides = shape == 2 ? TensorStrides{} : TensorStrides{shape == 0 ? -4 : 2, 1};
                    const std::ptrdiff_t origin = shape == 1 ? 1 : 4;
                    auto destination = TensorView<int>::borrow(
                        storage.data(), TensorLayout(8, origin, extents, strides));
                    const auto right = TensorView<const int>::borrow(storage.data(),
                        shape == 2 ? TensorLayout(8, 4, DynamicExtents{}, {}) :
                                     TensorLayout(8, 0, DynamicExtents{2}, TensorStrides{1}));
                    auto expected = original;
                    const std::size_t count = shape == 3 ? 0u : (shape == 2 ? 1u : 4u);
                    for (std::size_t index = 0; index < count; ++index)
                    {
                        const auto offset = shape == 2 ? std::size_t{4} :
                            static_cast<std::size_t>(origin + static_cast<std::ptrdiff_t>(index / 2) *
                            (shape == 0 ? -4 : 2) + static_cast<std::ptrdiff_t>(index % 2));
                        const int rhs = scalarRhs ? original[4] : original[shape == 2 ? 4 : index % 2];
                        switch (operation)
                        {
                            case 0: expected[offset] += rhs; break;
                            case 1: expected[offset] -= rhs; break;
                            case 2: expected[offset] *= rhs; break;
                            default: expected[offset] /= rhs; break;
                        }
                    }
                    bool rejected = false;
                    {
                        // No assertions, formatting, or test-framework work while allocation failure is armed.
                        allocation_probe::ScopedFailure injection(failure);
                        try
                        {
                            if (scalarRhs)
                            {
                                runCompound(operation, destination, storage[4], std::allocator<int>{});
                            }
                            else
                            {
                                runCompound(operation, destination, right, std::allocator<int>{});
                            }
                        }
                        catch (const std::bad_alloc&)
                        {
                            rejected = true;
                        }
                    }
                    if (rejected)
                    {
                        FATP_ASSERT_TRUE(storage == original,
                                         "Every injected allocation failure, including commit metadata, is atomic");
                    }
                    else
                    {
                        FATP_ASSERT_TRUE(storage == expected, "The first non-failing update has the expected values");
                        FATP_ASSERT_TRUE(count == 0 || failure > 0,
                                         "Nonempty operations must expose scratch allocation to this probe");
                        reachedSuccess = true;
                        break;
                    }
                }
                FATP_ASSERT_TRUE(reachedSuccess, "The allocation-failure sweep reaches the complete operation");
            }
        }
    }
    return true;
}
#endif

} // namespace fat_p::testing::tensor_algorithms

namespace fat_p::testing
{

bool test_TensorAlgorithms()
{
    FATP_PRINT_HEADER(TENSOR ALGORITHMS)
    TestRunner runner;
    FATP_RUN_TEST_NS(runner, tensor_algorithms, counted_signed_iteration);
    FATP_RUN_TEST_NS(runner, tensor_algorithms, fill_copy_and_negative_transform);
    FATP_RUN_TEST_NS(runner, tensor_algorithms, copy_from_shifted_and_permuted_aliases);
    FATP_RUN_TEST_NS(runner, tensor_algorithms, materialization_shapes_values_and_allocators);
    FATP_RUN_TEST_NS(runner, tensor_algorithms, copy_from_allocation_contract);
    FATP_RUN_TEST_NS(runner, tensor_algorithms, copy_from_shared_endpoint_requires_staging);
    FATP_RUN_TEST_NS(runner, tensor_algorithms, copy_from_validation_empty_and_exceptions);
    FATP_RUN_TEST_NS(runner, tensor_algorithms, randomized_materialization_and_overlap_oracle);
    FATP_RUN_TEST_NS(runner, tensor_algorithms, randomized_multi_layout_plan_oracle);
    FATP_RUN_TEST_NS(runner, tensor_algorithms, binary_broadcast_three_layouts);
    FATP_RUN_TEST_NS(runner, tensor_algorithms, equality_approximation_and_layout_independent_hash);
    FATP_RUN_TEST_NS(runner, tensor_algorithms, large_injective_destination_and_owner_allocator_selection);
    FATP_RUN_TEST_NS(runner, tensor_algorithms, zero_extent_broadcast);
    FATP_RUN_TEST_NS(runner, tensor_algorithms, integral_arithmetic_is_checked);
    FATP_RUN_TEST_NS(runner, tensor_algorithms, mixed_arithmetic_type_matrix);
    FATP_RUN_TEST_NS(runner, tensor_algorithms, mixed_arithmetic_exhaustive_byte_values);
    FATP_RUN_TEST_NS(runner, tensor_algorithms, mixed_arithmetic_numeric_boundaries);
    FATP_RUN_TEST_NS(runner, tensor_algorithms, mixed_arithmetic_layout_oracle);
    FATP_RUN_TEST_NS(runner, tensor_algorithms, mixed_arithmetic_allocators);
    FATP_RUN_TEST_NS(runner, tensor_algorithms, mixed_arithmetic_validation_and_cleanup);
    FATP_RUN_TEST_NS(runner, tensor_algorithms, cast_type_matrix_and_identity);
    FATP_RUN_TEST_NS(runner, tensor_algorithms, cast_integer_and_bool_domains);
    FATP_RUN_TEST_NS(runner, tensor_algorithms, cast_float_integer_boundaries);
    FATP_RUN_TEST_NS(runner, tensor_algorithms, cast_floating_rounding_and_nonfinite);
    FATP_RUN_TEST_NS(runner, tensor_algorithms, cast_and_scalar_layout_oracle);
    FATP_RUN_TEST_NS(runner, tensor_algorithms, scalar_numeric_boundaries_and_snapshot);
    FATP_RUN_TEST_NS(runner, tensor_algorithms, cast_and_scalar_allocator_contract);
    FATP_RUN_TEST_NS(runner, tensor_algorithms, cast_and_scalar_failure_and_lifetime);
    FATP_RUN_TEST_NS(runner, tensor_algorithms, division_integer_boundaries);
    FATP_RUN_TEST_NS(runner, tensor_algorithms, division_exhaustive_byte_values);
    FATP_RUN_TEST_NS(runner, tensor_algorithms, division_promotion_and_snapshot);
    FATP_RUN_TEST_NS(runner, tensor_algorithms, division_floating_special_values);
    FATP_RUN_TEST_NS(runner, tensor_algorithms, division_layout_oracle);
    FATP_RUN_TEST_NS(runner, tensor_algorithms, division_constraints);
    FATP_RUN_TEST_NS(runner, tensor_algorithms, division_allocator_contract);
    FATP_RUN_TEST_NS(runner, tensor_algorithms, division_failure_and_lifetime);
    FATP_RUN_TEST_NS(runner, tensor_algorithms, compound_constraints);
    FATP_RUN_TEST_NS(runner, tensor_algorithms, compound_broadcast_and_storage);
    FATP_RUN_TEST_NS(runner, tensor_algorithms, compound_checked_conversion_rollback);
    FATP_RUN_TEST_NS(runner, tensor_algorithms, compound_aliasing_snapshots);
    FATP_RUN_TEST_NS(runner, tensor_algorithms, compound_layout_oracle);
    FATP_RUN_TEST_NS(runner, tensor_algorithms, compound_allocator_contract);
    FATP_RUN_TEST_NS(runner, tensor_algorithms, compound_failure_and_lifetime);
    FATP_RUN_TEST_NS(runner, tensor_algorithms, compound_integer_types);
#if defined(ENABLE_TEST_APPLICATION) && !defined(FATP_TENSOR_DISABLE_ALLOCATION_PROBE) && \
    (!defined(_MSC_VER) || _ITERATOR_DEBUG_LEVEL == 0)
    FATP_RUN_TEST_NS(runner, tensor_algorithms, compound_allocation_failure_transaction);
#elif defined(ENABLE_TEST_APPLICATION)
    std::cout << "[SKIP] compound_allocation_failure_transaction: "
                 "disabled or MSVC checked-iterator runtime.\n";
#endif
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
    return fat_p::testing::test_TensorAlgorithms() ? 0 : 1;
}
#endif
