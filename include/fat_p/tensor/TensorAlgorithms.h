#pragma once

/*
FATP_META:
  meta_version: 1
  component: TensorAlgorithms
  file_role: internal_header
  path: include/fat_p/tensor/TensorAlgorithms.h
  namespace: fat_p
  layer: Domain
  summary: "Serial owner/view Tensor algorithms built on the unified iteration plan."
  api_stability: in_work
  related:
    docs:
      - components/Tensor/docs/Design Note - Tensor Architecture Additions Plan.md
      - components/Tensor/docs/Design Note - Tensor Semantic Contract.md
      - components/Tensor/docs/User Manual - Tensor.md
    headers:
      - include/fat_p/TensorAlgorithms.h
      - include/fat_p/tensor/TensorKernels.h
    tests:
      - components/Tensor/tests/test_TensorAlgorithms.cpp
  hygiene:
    pragma_once: true
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
#include <cmath>
#include <concepts>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "Tensor.h"
#include "TensorKernels.h"

namespace fat_p
{

namespace tensor_detail
{

// Selection is symmetric and range-preserving for integer operands. void marks
// unsupported pairs so public constraints reject them without a hard error.
template <typename Left, typename Right>
[[nodiscard]] consteval auto arithmeticTypeIdentity()
{
    if constexpr (!std::is_arithmetic_v<Left> || !std::is_arithmetic_v<Right> ||
                  std::same_as<Left, bool> || std::same_as<Right, bool>)
    {
        return std::type_identity<void>{};
    }
    else if constexpr ((std::integral<Left> &&
                        std::numeric_limits<Left>::digits > std::numeric_limits<std::uint64_t>::digits) ||
                       (std::integral<Right> &&
                        std::numeric_limits<Right>::digits > std::numeric_limits<std::uint64_t>::digits))
    {
        return std::type_identity<void>{};
    }
    else if constexpr (std::same_as<Left, Right>)
    {
        return std::type_identity<Left>{};
    }
    else if constexpr (std::floating_point<Left> || std::floating_point<Right>)
    {
        using common_type = std::common_type_t<Left, Right>;
        using integer_type = std::conditional_t<std::integral<Left>, Left, Right>;
        if constexpr ((std::integral<Left> || std::integral<Right>) &&
                      std::same_as<common_type, float> &&
                      (std::numeric_limits<float>::radix != 2 ||
                       std::numeric_limits<float>::digits < std::numeric_limits<integer_type>::digits))
        {
            return std::type_identity<double>{};
        }
        else
        {
            return std::type_identity<common_type>{};
        }
    }
    else if constexpr (std::is_signed_v<Left> == std::is_signed_v<Right>)
    {
        if constexpr (std::numeric_limits<Left>::digits > std::numeric_limits<Right>::digits)
        {
            return std::type_identity<Left>{};
        }
        else if constexpr (std::numeric_limits<Right>::digits > std::numeric_limits<Left>::digits)
        {
            return std::type_identity<Right>{};
        }
        else
        {
            // Only equal-range ties use the usual arithmetic conversion rank.
            return std::type_identity<std::common_type_t<Left, Right>>{};
        }
    }
    else
    {
        using signed_type = std::conditional_t<std::is_signed_v<Left>, Left, Right>;
        using unsigned_type = std::conditional_t<std::is_unsigned_v<Left>, Left, Right>;
        constexpr int requiredDigits = std::numeric_limits<unsigned_type>::digits;
        if constexpr (std::numeric_limits<signed_type>::digits >= requiredDigits)
        {
            return std::type_identity<signed_type>{};
        }
        else if constexpr (std::numeric_limits<std::int16_t>::digits >= requiredDigits)
        {
            return std::type_identity<std::int16_t>{};
        }
        else if constexpr (std::numeric_limits<std::int32_t>::digits >= requiredDigits)
        {
            return std::type_identity<std::int32_t>{};
        }
        else if constexpr (std::numeric_limits<std::int64_t>::digits >= requiredDigits)
        {
            return std::type_identity<std::int64_t>{};
        }
        else
        {
            return std::type_identity<void>{};
        }
    }
}

template <typename Result, typename Source>
[[nodiscard]] auto selectResultAllocator(const Source& source)
{
    if constexpr (requires { source.get_allocator(); })
    {
        using source_allocator = std::remove_cvref_t<decltype(source.get_allocator())>;
        using result_allocator =
            typename std::allocator_traits<source_allocator>::template rebind_alloc<Result>;
        result_allocator rebound(source.get_allocator());
        return std::allocator_traits<result_allocator>::select_on_container_copy_construction(rebound);
    }
    else
    {
        return TensorAllocator<Result>{};
    }
}

template <typename Source>
inline constexpr std::size_t unaryResultRank = tensorStaticRankValue<Source>;

template <typename Left, typename Right>
inline constexpr std::size_t binaryResultRank = [] {
    if constexpr (tensorStaticRankValue<Left> == kDynamicTensorRank ||
                  tensorStaticRankValue<Right> == kDynamicTensorRank)
    {
        return kDynamicTensorRank;
    }
    else
    {
        return tensorStaticRankValue<Left> > tensorStaticRankValue<Right>
            ? tensorStaticRankValue<Left>
            : tensorStaticRankValue<Right>;
    }
}();

template <typename Value, typename Allocator, typename Source>
using UnaryTensorResult = Tensor<Value, Allocator, unaryResultRank<Source>>;

template <typename Value, typename Allocator, typename Left, typename Right>
using BinaryTensorResult = Tensor<Value, Allocator, binaryResultRank<Left, Right>>;

// The first owner wins; views have no allocator to propagate.
template <typename Result, typename Left, typename Right>
[[nodiscard]] auto selectBinaryResultAllocator(const Left& left, const Right& right)
{
    if constexpr (requires { left.get_allocator(); })
    {
        return selectResultAllocator<Result>(left);
    }
    else
    {
        return selectResultAllocator<Result>(right);
    }
}

template <typename Allocator, typename Value>
concept AllocatorFor = requires {
    typename Allocator::value_type;
} && std::same_as<typename Allocator::value_type, Value>;

template <typename Source>
concept CopyMaterializableTensor = ReadableTensor<Source> &&
    std::default_initializable<typename Source::value_type> &&
    std::assignable_from<typename Source::value_type&, const typename Source::value_type&>;

template <typename Source>
concept FloatingMathTensor = ReadableTensor<Source> &&
    (std::same_as<typename Source::value_type, float> ||
     std::same_as<typename Source::value_type, double> ||
     std::same_as<typename Source::value_type, long double>);

template <typename T>
[[nodiscard]] constexpr T checkedSameTypeAdd(const T& left, const T& right)
{
    if constexpr (std::integral<T> && !std::same_as<T, bool>)
    {
        constexpr T minimum = std::numeric_limits<T>::lowest();
        constexpr T maximum = std::numeric_limits<T>::max();
        if constexpr (std::unsigned_integral<T>)
        {
            if (right > maximum - left)
            {
                throw std::overflow_error("Tensor integer addition overflow");
            }
        }
        else if ((right > 0 && left > maximum - right) ||
                 (right < 0 && left < minimum - right))
        {
            throw std::overflow_error("Tensor integer addition overflow");
        }
    }
    return static_cast<T>(left + right);
}

template <typename T>
[[nodiscard]] constexpr T checkedSameTypeSubtract(const T& left, const T& right)
{
    if constexpr (std::integral<T> && !std::same_as<T, bool>)
    {
        constexpr T minimum = std::numeric_limits<T>::lowest();
        constexpr T maximum = std::numeric_limits<T>::max();
        if constexpr (std::unsigned_integral<T>)
        {
            if (left < right)
            {
                throw std::overflow_error("Tensor integer subtraction overflow");
            }
        }
        else if ((right > 0 && left < minimum + right) ||
                 (right < 0 && left > maximum + right))
        {
            throw std::overflow_error("Tensor integer subtraction overflow");
        }
    }
    return static_cast<T>(left - right);
}

template <typename T>
[[nodiscard]] constexpr T checkedSameTypeNegate(const T& value)
{
    static_assert(!std::same_as<typename decltype(arithmeticTypeIdentity<T, T>())::type, void>);
    if constexpr (std::unsigned_integral<T>)
    {
        if (value != 0)
        {
            throw std::overflow_error("Tensor unsigned negation requires zero");
        }
        return value;
    }
    else
    {
        if constexpr (std::signed_integral<T>)
        {
            if (value == std::numeric_limits<T>::lowest())
            {
                throw std::overflow_error("Tensor integer negation overflow");
            }
        }
        // Subtracting from zero would lose the required +0 -> -0 sign change.
        return static_cast<T>(-value);
    }
}

template <typename T>
[[nodiscard]] T checkedSameTypeAbs(const T& value)
{
    static_assert(!std::same_as<typename decltype(arithmeticTypeIdentity<T, T>())::type, void>);
    if constexpr (std::floating_point<T>)
    {
        // std::fabs is not required to be constexpr in the C++20 baseline.
        return std::fabs(value);
    }
    else if constexpr (std::signed_integral<T>)
    {
        if (value == std::numeric_limits<T>::lowest())
        {
            throw std::overflow_error("Tensor integer absolute value overflow");
        }
        return value < 0 ? static_cast<T>(-value) : value;
    }
    else
    {
        return value;
    }
}

template <typename T>
[[nodiscard]] constexpr T checkedSameTypeMultiply(const T& left, const T& right)
{
    if constexpr (std::integral<T> && !std::same_as<T, bool>)
    {
        constexpr T minimum = std::numeric_limits<T>::lowest();
        constexpr T maximum = std::numeric_limits<T>::max();
        if constexpr (std::unsigned_integral<T>)
        {
            if (right != 0 && left > maximum / right)
            {
                throw std::overflow_error("Tensor integer multiplication overflow");
            }
        }
        else
        {
            const bool overflow =
                (left > 0 && right > 0 && left > maximum / right) ||
                (left > 0 && right < 0 && right < minimum / left) ||
                (left < 0 && right > 0 && left < minimum / right) ||
                (left < 0 && right < 0 && left < maximum / right);
            if (overflow)
            {
                throw std::overflow_error("Tensor integer multiplication overflow");
            }
        }
    }
    return static_cast<T>(left * right);
}


template <typename T>
[[nodiscard]] constexpr T checkedSameTypeDivide(const T& left, const T& right)
{
    if constexpr (std::integral<T> && !std::same_as<T, bool>)
    {
        if (right == T{0})
        {
            throw std::domain_error("Tensor integer division by zero");
        }
        if constexpr (std::signed_integral<T>)
        {
            // Check the result type, including narrow integers promoted by C++ division.
            if (left == std::numeric_limits<T>::lowest() && right == T{-1})
            {
                throw std::overflow_error("Tensor integer division overflow");
            }
        }
    }
    return static_cast<T>(left / right);
}

template <typename T>
concept CastValue = std::same_as<T, std::remove_cvref_t<T>> &&
    ((std::integral<T> &&
      std::numeric_limits<T>::digits <= std::numeric_limits<std::uint64_t>::digits - (std::is_signed_v<T> ? 1 : 0)) ||
     std::same_as<T, float> || std::same_as<T, double> || std::same_as<T, long double>);

// Range/domain checked, not lossless: floating destinations may round.
// Keep the checks before the language conversion, especially at 2^63 and 2^64.
template <CastValue To, CastValue From>
[[nodiscard]] To checkedScalarCast(From value)
{
    if constexpr (std::same_as<To, From>)
    {
        return value;
    }
    else if constexpr (std::same_as<To, bool>)
    {
        if (value != From{0} && value != From{1})
        {
            throw std::domain_error("Tensor cast to bool requires exactly zero or one");
        }
    }
    else if constexpr (std::same_as<From, bool>)
    {
        return static_cast<To>(value);
    }
    else if constexpr (std::integral<From> && std::integral<To>)
    {
        bool outside = false;
        if constexpr (std::signed_integral<From>)
        {
            const auto wide = static_cast<std::int64_t>(value);
            if constexpr (std::signed_integral<To>)
            {
                outside = wide < static_cast<std::int64_t>(std::numeric_limits<To>::lowest()) ||
                          wide > static_cast<std::int64_t>(std::numeric_limits<To>::max());
            }
            else
            {
                outside = wide < 0 ||
                          static_cast<std::uint64_t>(wide) >
                              static_cast<std::uint64_t>(std::numeric_limits<To>::max());
            }
        }
        else
        {
            outside = static_cast<std::uint64_t>(value) >
                      static_cast<std::uint64_t>(std::numeric_limits<To>::max());
        }
        if (outside)
        {
            throw std::overflow_error("Tensor integer cast out of range");
        }
    }
    else if constexpr (std::floating_point<From> && std::integral<To>)
    {
        if (!std::isfinite(value) || std::trunc(value) != value)
        {
            throw std::domain_error("Tensor cast to integer requires a finite integral value");
        }
        const From upperExclusive = std::ldexp(From{1}, std::numeric_limits<To>::digits);
        const From lowerInclusive = std::is_signed_v<To> ? -upperExclusive : From{0};
        if (value < lowerInclusive || value >= upperExclusive)
        {
            throw std::overflow_error("Tensor floating-to-integer cast out of range");
        }
    }
    else if constexpr (std::floating_point<From> && std::floating_point<To>)
    {
        if (std::isnan(value))
        {
            if constexpr (std::numeric_limits<To>::has_quiet_NaN)
            {
                return std::numeric_limits<To>::quiet_NaN();
            }
            else
            {
                throw std::domain_error("Tensor cast destination cannot represent NaN");
            }
        }
        if (std::isinf(value))
        {
            if constexpr (std::numeric_limits<To>::has_infinity)
            {
                return std::signbit(value) ? -std::numeric_limits<To>::infinity()
                                           : std::numeric_limits<To>::infinity();
            }
            else
            {
                throw std::domain_error("Tensor cast destination cannot represent infinity");
            }
        }
        using comparison_type = std::common_type_t<From, To>;
        const auto maximum = static_cast<comparison_type>(std::numeric_limits<To>::max());
        const auto wide = static_cast<comparison_type>(value);
        if (wide < -maximum || wide > maximum)
        {
            throw std::overflow_error("Tensor floating cast out of range");
        }
    }
    // Identity and bool-source branches already returned. Discard this return
    // for those instantiations rather than leaving unreachable generated code.
    if constexpr (!std::same_as<To, From> && !std::same_as<From, bool>)
    {
        // All supported <=64-bit integers fit the range of standard floating types.
        return static_cast<To>(value);
    }
}

template <ReadableTensor Source, typename Allocator, typename Operation>
[[nodiscard]] UnaryTensorResult<typename Source::value_type, Allocator, Source>
unaryArithmetic(const Source& source, const Allocator& allocator, Operation operation)
{
    TensorAccess::validate(source);
    UnaryTensorResult<typename Source::value_type, Allocator, Source> result(
        std::allocator_arg, allocator, source.extents());
    unaryKernel(source, result, operation);
    return result;
}

template <bool ScalarFirst, typename Result, ReadableTensor Source, typename Scalar,
          typename Allocator, typename Operation>
[[nodiscard]] UnaryTensorResult<Result, Allocator, Source>
scalarArithmetic(const Source& source, Scalar scalar, const Allocator& allocator, Operation operation)
{
    TensorAccess::validate(source);
    const Result convertedScalar = static_cast<Result>(scalar);
    UnaryTensorResult<Result, Allocator, Source> result(std::allocator_arg, allocator, source.extents());
    unaryKernel(source, result, [convertedScalar, operation](const auto& value) {
        if constexpr (ScalarFirst)
        {
            return std::invoke(operation, convertedScalar, static_cast<Result>(value));
        }
        else
        {
            return std::invoke(operation, static_cast<Result>(value), convertedScalar);
        }
    });
    return result;
}

} // namespace tensor_detail

/** @brief Whether two element types have a supported binary arithmetic result. */
template <typename Left, typename Right>
concept TensorArithmeticCompatible = !std::same_as<
    typename decltype(tensor_detail::arithmeticTypeIdentity<std::remove_cvref_t<Left>,
                                                           std::remove_cvref_t<Right>>())::type,
    void>;

/** @brief Symmetric binary result type; bool and nonrepresentable signed/unsigned pairs are rejected. */
template <typename Left, typename Right>
    requires TensorArithmeticCompatible<Left, Right>
using TensorArithmeticType =
    typename decltype(tensor_detail::arithmeticTypeIdentity<std::remove_cvref_t<Left>,
                                                           std::remove_cvref_t<Right>>())::type;

template <typename Left, typename Right>
concept SameTensorValue = ReadableTensor<Left> && ReadableTensor<Right> &&
    std::same_as<typename Left::value_type, typename Right::value_type>;


/**
 * @brief Convert logical values into a fresh canonical owner without changing extents.
 * @tparam To Unqualified standard arithmetic result type, including bool (integers up to 64 bits).
 * @tparam Source Readable owner or view with a supported arithmetic value type.
 * @tparam Allocator Result allocator whose value_type is To; used unchanged.
 * @param source Source mapping, never modified.
 * @param allocator Allocator instance for the result element buffer.
 * @return Independent Tensor<To, Allocator>, including for same-type casts.
 * @details Integer results require finite integral values in range; bool requires exactly 0 or 1.
 * Floating results may round or underflow to zero; finite overflow is rejected. Floating NaNs and
 * infinities are preserved by category when supported, without a NaN payload/sign guarantee.
 * Linear work and result storage plus iteration metadata; no intermediate element buffer.
 * Lifetime validation precedes allocation; later conversion failure reclaims the unpublished result.
 * @throws std::domain_error For fractional/nonfinite integer inputs or non-binary bool inputs.
 * @throws std::overflow_error For values outside the destination's numeric range.
 * @throws std::bad_alloc On result or metadata allocation failure.
 * @throws std::runtime_error On expired borrowed input in assertions-enabled builds.
 */
template <tensor_detail::CastValue To, ReadableTensor Source, typename Allocator>
    requires tensor_detail::CastValue<typename Source::value_type> && tensor_detail::AllocatorFor<Allocator, To>
[[nodiscard]] tensor_detail::UnaryTensorResult<To, Allocator, Source>
cast(const Source& source, const Allocator& allocator)
{
    tensor_detail::TensorAccess::validate(source);
    tensor_detail::UnaryTensorResult<To, Allocator, Source> result(std::allocator_arg, allocator,
                                                                   source.extents());
    tensor_detail::unaryKernel(source, result,
                              tensor_detail::checkedScalarCast<To, typename Source::value_type>);
    return result;
}

/** @brief Checked cast using the owner's result-rebound SOCCC allocator, or TensorAllocator<To> for a view. */
template <tensor_detail::CastValue To, ReadableTensor Source>
    requires tensor_detail::CastValue<typename Source::value_type>
[[nodiscard]] auto cast(const Source& source)
{
    return cast<To>(source, tensor_detail::selectResultAllocator<To>(source));
}

/**
 * @brief Copy logical row-major values into a new canonical owner with the requested shape.
 * @details Source and target must have equal logical element counts, not necessarily equal ranks.
 * Noncontiguous sources are supported; the result never aliases its source. Requires a
 * default-initializable, copy-assignable value type and uses the exact allocator supplied.
 * Linear element work and element storage, plus iteration metadata; no partial result is published
 * on failure. Callers must synchronize concurrent writes to the source or its aliases.
 * @throws std::invalid_argument If the logical element count changes (before element allocation).
 * @throws std::bad_alloc If result or metadata allocation fails.
 * @throws std::runtime_error For an expired borrowed source in assertions-enabled builds.
 */
template <tensor_detail::CopyMaterializableTensor Source, typename Allocator>
    requires tensor_detail::AllocatorFor<Allocator, typename Source::value_type>
[[nodiscard]] auto reshapeCopy(const Source& source, DynamicExtents target, const Allocator& allocator)
    -> Tensor<typename Source::value_type, Allocator>
{
    tensor_detail::TensorAccess::validate(source);
    if (source.size() != target.logicalSize())
    {
        throw std::invalid_argument("reshapeCopy cannot change the logical element count");
    }
    Tensor<typename Source::value_type, Allocator> result(std::allocator_arg, allocator, std::move(target));
    auto destination = result.reshapeView(source.extents());
    tensor_detail::copyKernel(source, destination);
    return result;
}

/** @brief Reshaped copy using owner SOCCC, or TensorAllocator for a view input. */
template <tensor_detail::CopyMaterializableTensor Source>
[[nodiscard]] auto reshapeCopy(const Source& source, DynamicExtents target)
{
    return reshapeCopy(source, std::move(target),
                       tensor_detail::selectResultAllocator<typename Source::value_type>(source));
}

template <tensor_detail::CopyMaterializableTensor Source, std::size_t NewRank, typename Allocator>
    requires tensor_detail::AllocatorFor<Allocator, typename Source::value_type>
[[nodiscard]] auto reshapeCopy(const Source& source, tensor_detail::FixedRankExtents<NewRank> target,
                               const Allocator& allocator)
    -> Tensor<typename Source::value_type, Allocator, NewRank>
{
    tensor_detail::TensorAccess::validate(source);
    if (source.size() != target.logicalSize())
    {
        throw std::invalid_argument("reshapeCopy cannot change the logical element count");
    }
    Tensor<typename Source::value_type, Allocator, NewRank> result(std::allocator_arg, allocator,
                                                                   std::move(target));
    auto destination = result.reshapeView(source.extents());
    tensor_detail::copyKernel(source, destination);
    if constexpr (NewRank == 0)
    {
        return tensor_detail::TensorAccess::finishMaterialization(std::move(result));
    }
    else
    {
        return result;
    }
}

template <tensor_detail::CopyMaterializableTensor Source, std::size_t NewRank>
[[nodiscard]] auto reshapeCopy(const Source& source, tensor_detail::FixedRankExtents<NewRank> target)
{
    return reshapeCopy(source, std::move(target),
                       tensor_detail::selectResultAllocator<typename Source::value_type>(source));
}

/**
 * @brief Copy values into a non-const lvalue destination without resizing or rebinding it.
 * @details Requires identical extents, the same default-initializable/copy-assignable value type,
 * and an injective destination. Potentially overlapping address ranges snapshot the entire source
 * using the exact scratch allocator before any destination write. Proven-disjoint ranges copy
 * directly. Linear element work; zero scratch elements when disjoint or empty, otherwise size()
 * scratch elements, plus iteration metadata. Caller synchronizes all concurrent access to aliases.
 * @throws std::invalid_argument On shape mismatch, before mutation. Mutable view factories reject
 * noninjective layouts at construction; the copy path retains a defensive injectivity check.
 * @note Lifetime checks, validation, allocation and snapshot failure leave the destination unchanged.
 * Borrowed-lifetime diagnostics are assertions-enabled checks, not support for dangling Release views.
 * A throwing final element assignment may leave partial values, but storage and mapping remain valid
 * (the element type must itself preserve validity on assignment failure). There is no rollback.
 */
template <WritableTensor Destination, tensor_detail::CopyMaterializableTensor Source, typename Allocator>
    requires SameTensorValue<Source, Destination> && (!std::is_const_v<Destination>) &&
             tensor_detail::AllocatorFor<Allocator, typename Source::value_type>
void copyFrom(Destination& destination, const Source& source, const Allocator& scratchAllocator)
{
    tensor_detail::validateCopy(source, destination);
    if (source.size() == 0)
    {
        return;
    }
    if (tensor_detail::reachableRangesDisjoint(source, destination))
    {
        tensor_detail::copyKernel(source, destination);
        return;
    }
    const auto snapshot = clone(source, scratchAllocator);
    tensor_detail::copyKernel(snapshot, destination);
}

/**
 * @brief Copy using the destination owner's exact allocator for scratch (without SOCCC).
 * @details A view destination uses TensorAllocator<value_type>; source ownership does not select scratch.
 */
template <WritableTensor Destination, tensor_detail::CopyMaterializableTensor Source>
    requires SameTensorValue<Source, Destination> && (!std::is_const_v<Destination>)
void copyFrom(Destination& destination, const Source& source)
{
    if constexpr (requires { destination.get_allocator(); })
    {
        copyFrom(destination, source, destination.get_allocator());
    }
    else
    {
        copyFrom(destination, source, TensorAllocator<typename Source::value_type>{});
    }
}

/**
 * @brief Broadcasted binary arithmetic returning a new canonical owner.
 * @details Operands convert to TensorArithmeticType before the operation. Integer results check
 * overflow/underflow; floating results use ordinary arithmetic and may round integer conversions.
 * bool and integer pairs without an exact common integer representation are not supported.
 * Input lifetimes and broadcast shape are validated before result element allocation.
 * An explicit result allocator is used unchanged; otherwise the first owner from left to right
 * supplies an allocator rebound to the result type, then selected with SOCCC. Views only use
 * TensorAllocator<Result>. No failure publishes a partial result or modifies either input.
 * @throws std::overflow_error On integer result overflow/underflow or shape arithmetic overflow.
 * @throws std::invalid_argument On incompatible broadcast extents.
 * @throws std::bad_alloc On allocation failure (including iteration metadata).
 * @throws std::runtime_error On expired borrowed input in assertions-enabled builds.
 */
template <ReadableTensor Left, ReadableTensor Right, typename Allocator>
    requires TensorArithmeticCompatible<typename Left::value_type, typename Right::value_type> &&
             tensor_detail::AllocatorFor<
                 Allocator, TensorArithmeticType<typename Left::value_type, typename Right::value_type>>
[[nodiscard]] auto add(const Left& left, const Right& right, const Allocator& allocator)
    -> tensor_detail::BinaryTensorResult<
        TensorArithmeticType<typename Left::value_type, typename Right::value_type>, Allocator, Left, Right>
{
    using value_type = TensorArithmeticType<typename Left::value_type, typename Right::value_type>;
    tensor_detail::TensorAccess::validate(left);
    tensor_detail::TensorAccess::validate(right);
    const auto extents = tensor_detail::broadcastExtents(left.layout(), right.layout());
    tensor_detail::BinaryTensorResult<value_type, Allocator, Left, Right> result(std::allocator_arg, allocator,
                                                                                 extents);
    tensor_detail::binaryKernel(left, right, result, [](const auto& leftValue, const auto& rightValue) {
        return tensor_detail::checkedSameTypeAdd(static_cast<value_type>(leftValue),
                                                      static_cast<value_type>(rightValue));
    });
    return result;
}

template <ReadableTensor Left, ReadableTensor Right>
    requires TensorArithmeticCompatible<typename Left::value_type, typename Right::value_type>
[[nodiscard]] auto add(const Left& left, const Right& right)
{
    using value_type = TensorArithmeticType<typename Left::value_type, typename Right::value_type>;
    return add(left, right, tensor_detail::selectBinaryResultAllocator<value_type>(left, right));
}

template <ReadableTensor Left, ReadableTensor Right, typename Allocator>
    requires TensorArithmeticCompatible<typename Left::value_type, typename Right::value_type> &&
             tensor_detail::AllocatorFor<
                 Allocator, TensorArithmeticType<typename Left::value_type, typename Right::value_type>>
[[nodiscard]] auto subtract(const Left& left, const Right& right, const Allocator& allocator)
    -> tensor_detail::BinaryTensorResult<
        TensorArithmeticType<typename Left::value_type, typename Right::value_type>, Allocator, Left, Right>
{
    using value_type = TensorArithmeticType<typename Left::value_type, typename Right::value_type>;
    tensor_detail::TensorAccess::validate(left);
    tensor_detail::TensorAccess::validate(right);
    const auto extents = tensor_detail::broadcastExtents(left.layout(), right.layout());
    tensor_detail::BinaryTensorResult<value_type, Allocator, Left, Right> result(std::allocator_arg, allocator,
                                                                                 extents);
    tensor_detail::binaryKernel(left, right, result, [](const auto& leftValue, const auto& rightValue) {
        return tensor_detail::checkedSameTypeSubtract(static_cast<value_type>(leftValue),
                                                      static_cast<value_type>(rightValue));
    });
    return result;
}

template <ReadableTensor Left, ReadableTensor Right>
    requires TensorArithmeticCompatible<typename Left::value_type, typename Right::value_type>
[[nodiscard]] auto subtract(const Left& left, const Right& right)
{
    using value_type = TensorArithmeticType<typename Left::value_type, typename Right::value_type>;
    return subtract(left, right, tensor_detail::selectBinaryResultAllocator<value_type>(left, right));
}

template <ReadableTensor Left, ReadableTensor Right, typename Allocator>
    requires TensorArithmeticCompatible<typename Left::value_type, typename Right::value_type> &&
             tensor_detail::AllocatorFor<
                 Allocator, TensorArithmeticType<typename Left::value_type, typename Right::value_type>>
[[nodiscard]] auto multiply(const Left& left, const Right& right, const Allocator& allocator)
    -> tensor_detail::BinaryTensorResult<
        TensorArithmeticType<typename Left::value_type, typename Right::value_type>, Allocator, Left, Right>
{
    using value_type = TensorArithmeticType<typename Left::value_type, typename Right::value_type>;
    tensor_detail::TensorAccess::validate(left);
    tensor_detail::TensorAccess::validate(right);
    const auto extents = tensor_detail::broadcastExtents(left.layout(), right.layout());
    tensor_detail::BinaryTensorResult<value_type, Allocator, Left, Right> result(std::allocator_arg, allocator,
                                                                                 extents);
    tensor_detail::binaryKernel(left, right, result, [](const auto& leftValue, const auto& rightValue) {
        return tensor_detail::checkedSameTypeMultiply(static_cast<value_type>(leftValue),
                                                      static_cast<value_type>(rightValue));
    });
    return result;
}

template <ReadableTensor Left, ReadableTensor Right>
    requires TensorArithmeticCompatible<typename Left::value_type, typename Right::value_type>
[[nodiscard]] auto multiply(const Left& left, const Right& right)
{
    using value_type = TensorArithmeticType<typename Left::value_type, typename Right::value_type>;
    return multiply(left, right, tensor_detail::selectBinaryResultAllocator<value_type>(left, right));
}

/**
 * @brief Elementwise division with broadcasting and TensorArithmeticType promotion.
 * @details Converts both operands before division. Integer quotients truncate toward zero;
 * floating results use native division without reconfiguring the caller's floating-point environment.
 * IEC 559 special-value behavior requires a supporting, nontrapping, semantics-preserving build.
 * Uses the binary arithmetic lifetime, allocator, and unpublished-result cleanup contract.
 * Empty output evaluates no quotient; rank zero evaluates one.
 * @throws std::domain_error On an evaluated zero divisor when the result type is integral.
 * @throws std::overflow_error On integer result lowest() / -1 or shape arithmetic overflow.
 * @throws std::invalid_argument On incompatible broadcast extents.
 * @throws std::bad_alloc On result or metadata allocation failure, possibly before a numeric error.
 * @throws std::runtime_error On expired borrowed input in assertions-enabled builds.
 */
template <ReadableTensor Left, ReadableTensor Right, typename Allocator>
    requires TensorArithmeticCompatible<typename Left::value_type, typename Right::value_type> &&
             tensor_detail::AllocatorFor<
                 Allocator, TensorArithmeticType<typename Left::value_type, typename Right::value_type>>
[[nodiscard]] auto divide(const Left& left, const Right& right, const Allocator& allocator)
    -> tensor_detail::BinaryTensorResult<
        TensorArithmeticType<typename Left::value_type, typename Right::value_type>, Allocator, Left, Right>
{
    using value_type = TensorArithmeticType<typename Left::value_type, typename Right::value_type>;
    tensor_detail::TensorAccess::validate(left);
    tensor_detail::TensorAccess::validate(right);
    const auto extents = tensor_detail::broadcastExtents(left.layout(), right.layout());
    tensor_detail::BinaryTensorResult<value_type, Allocator, Left, Right> result(std::allocator_arg, allocator,
                                                                                 extents);
    tensor_detail::binaryKernel(left, right, result, [](const auto& leftValue, const auto& rightValue) {
        return tensor_detail::checkedSameTypeDivide(static_cast<value_type>(leftValue),
                                                    static_cast<value_type>(rightValue));
    });
    return result;
}

template <ReadableTensor Left, ReadableTensor Right>
    requires TensorArithmeticCompatible<typename Left::value_type, typename Right::value_type>
[[nodiscard]] auto divide(const Left& left, const Right& right)
{
    using value_type = TensorArithmeticType<typename Left::value_type, typename Right::value_type>;
    return divide(left, right, tensor_detail::selectBinaryResultAllocator<value_type>(left, right));
}

/**
 * @brief Scalar arithmetic in either operand order with TensorArithmeticType promotion.
 * @details The scalar is snapshotted by value; no scalar Tensor or intermediate element buffer is
 * allocated. Output extents match the readable operand. The tensor owner's allocator is rebound
 * before SOCCC, views use TensorAllocator<Result>, and explicit allocators are used unchanged.
 * Integer results are checked; bool operands are excluded, just as for tensor/tensor arithmetic.
 * Source lifetime validation precedes result allocation; no failure modifies the source.
 */
template <ReadableTensor Source, typename Scalar, typename Allocator>
    requires TensorArithmeticCompatible<typename Source::value_type, Scalar> &&
             tensor_detail::AllocatorFor<Allocator, TensorArithmeticType<typename Source::value_type, Scalar>>
[[nodiscard]] auto add(const Source& source, Scalar scalar, const Allocator& allocator)
    -> tensor_detail::UnaryTensorResult<
        TensorArithmeticType<typename Source::value_type, Scalar>, Allocator, Source>
{
    using result_type = TensorArithmeticType<typename Source::value_type, Scalar>;
    return tensor_detail::scalarArithmetic<false, result_type>(
        source, scalar, allocator, tensor_detail::checkedSameTypeAdd<result_type>);
}

template <ReadableTensor Source, typename Scalar>
    requires TensorArithmeticCompatible<typename Source::value_type, Scalar>
[[nodiscard]] auto add(const Source& source, Scalar scalar)
{
    using result_type = TensorArithmeticType<typename Source::value_type, Scalar>;
    return add(source, scalar, tensor_detail::selectResultAllocator<result_type>(source));
}

template <ReadableTensor Source, typename Scalar, typename Allocator>
    requires TensorArithmeticCompatible<typename Source::value_type, Scalar> &&
             tensor_detail::AllocatorFor<Allocator, TensorArithmeticType<typename Source::value_type, Scalar>>
[[nodiscard]] auto add(Scalar scalar, const Source& source, const Allocator& allocator)
    -> tensor_detail::UnaryTensorResult<
        TensorArithmeticType<typename Source::value_type, Scalar>, Allocator, Source>
{
    using result_type = TensorArithmeticType<typename Source::value_type, Scalar>;
    return tensor_detail::scalarArithmetic<true, result_type>(
        source, scalar, allocator, tensor_detail::checkedSameTypeAdd<result_type>);
}

template <ReadableTensor Source, typename Scalar>
    requires TensorArithmeticCompatible<typename Source::value_type, Scalar>
[[nodiscard]] auto add(Scalar scalar, const Source& source)
{
    using result_type = TensorArithmeticType<typename Source::value_type, Scalar>;
    return add(scalar, source, tensor_detail::selectResultAllocator<result_type>(source));
}

template <ReadableTensor Source, typename Scalar, typename Allocator>
    requires TensorArithmeticCompatible<typename Source::value_type, Scalar> &&
             tensor_detail::AllocatorFor<Allocator, TensorArithmeticType<typename Source::value_type, Scalar>>
[[nodiscard]] auto subtract(const Source& source, Scalar scalar, const Allocator& allocator)
    -> tensor_detail::UnaryTensorResult<
        TensorArithmeticType<typename Source::value_type, Scalar>, Allocator, Source>
{
    using result_type = TensorArithmeticType<typename Source::value_type, Scalar>;
    return tensor_detail::scalarArithmetic<false, result_type>(
        source, scalar, allocator, tensor_detail::checkedSameTypeSubtract<result_type>);
}

template <ReadableTensor Source, typename Scalar>
    requires TensorArithmeticCompatible<typename Source::value_type, Scalar>
[[nodiscard]] auto subtract(const Source& source, Scalar scalar)
{
    using result_type = TensorArithmeticType<typename Source::value_type, Scalar>;
    return subtract(source, scalar, tensor_detail::selectResultAllocator<result_type>(source));
}

template <ReadableTensor Source, typename Scalar, typename Allocator>
    requires TensorArithmeticCompatible<typename Source::value_type, Scalar> &&
             tensor_detail::AllocatorFor<Allocator, TensorArithmeticType<typename Source::value_type, Scalar>>
[[nodiscard]] auto subtract(Scalar scalar, const Source& source, const Allocator& allocator)
    -> tensor_detail::UnaryTensorResult<
        TensorArithmeticType<typename Source::value_type, Scalar>, Allocator, Source>
{
    using result_type = TensorArithmeticType<typename Source::value_type, Scalar>;
    return tensor_detail::scalarArithmetic<true, result_type>(
        source, scalar, allocator, tensor_detail::checkedSameTypeSubtract<result_type>);
}

template <ReadableTensor Source, typename Scalar>
    requires TensorArithmeticCompatible<typename Source::value_type, Scalar>
[[nodiscard]] auto subtract(Scalar scalar, const Source& source)
{
    using result_type = TensorArithmeticType<typename Source::value_type, Scalar>;
    return subtract(scalar, source, tensor_detail::selectResultAllocator<result_type>(source));
}

template <ReadableTensor Source, typename Scalar, typename Allocator>
    requires TensorArithmeticCompatible<typename Source::value_type, Scalar> &&
             tensor_detail::AllocatorFor<Allocator, TensorArithmeticType<typename Source::value_type, Scalar>>
[[nodiscard]] auto multiply(const Source& source, Scalar scalar, const Allocator& allocator)
    -> tensor_detail::UnaryTensorResult<
        TensorArithmeticType<typename Source::value_type, Scalar>, Allocator, Source>
{
    using result_type = TensorArithmeticType<typename Source::value_type, Scalar>;
    return tensor_detail::scalarArithmetic<false, result_type>(
        source, scalar, allocator, tensor_detail::checkedSameTypeMultiply<result_type>);
}

template <ReadableTensor Source, typename Scalar>
    requires TensorArithmeticCompatible<typename Source::value_type, Scalar>
[[nodiscard]] auto multiply(const Source& source, Scalar scalar)
{
    using result_type = TensorArithmeticType<typename Source::value_type, Scalar>;
    return multiply(source, scalar, tensor_detail::selectResultAllocator<result_type>(source));
}

template <ReadableTensor Source, typename Scalar, typename Allocator>
    requires TensorArithmeticCompatible<typename Source::value_type, Scalar> &&
             tensor_detail::AllocatorFor<Allocator, TensorArithmeticType<typename Source::value_type, Scalar>>
[[nodiscard]] auto multiply(Scalar scalar, const Source& source, const Allocator& allocator)
    -> tensor_detail::UnaryTensorResult<
        TensorArithmeticType<typename Source::value_type, Scalar>, Allocator, Source>
{
    using result_type = TensorArithmeticType<typename Source::value_type, Scalar>;
    return tensor_detail::scalarArithmetic<true, result_type>(
        source, scalar, allocator, tensor_detail::checkedSameTypeMultiply<result_type>);
}

template <ReadableTensor Source, typename Scalar>
    requires TensorArithmeticCompatible<typename Source::value_type, Scalar>
[[nodiscard]] auto multiply(Scalar scalar, const Source& source)
{
    using result_type = TensorArithmeticType<typename Source::value_type, Scalar>;
    return multiply(scalar, source, tensor_detail::selectResultAllocator<result_type>(source));
}

/**
 * @brief Divide by a scalar or divide a scalar by each logical element.
 * @details Uses the scalar snapshot, result type, allocator, and lifetime contract above.
 * Quotients and exceptions follow tensor/tensor divide; empty inputs evaluate no divisor.
 * @throws std::domain_error On an evaluated zero divisor when the result type is integral.
 * @throws std::overflow_error On signed integral result lowest() / -1.
 */
template <ReadableTensor Source, typename Scalar, typename Allocator>
    requires TensorArithmeticCompatible<typename Source::value_type, Scalar> &&
             tensor_detail::AllocatorFor<Allocator, TensorArithmeticType<typename Source::value_type, Scalar>>
[[nodiscard]] auto divide(const Source& source, Scalar scalar, const Allocator& allocator)
    -> tensor_detail::UnaryTensorResult<
        TensorArithmeticType<typename Source::value_type, Scalar>, Allocator, Source>
{
    using result_type = TensorArithmeticType<typename Source::value_type, Scalar>;
    return tensor_detail::scalarArithmetic<false, result_type>(
        source, scalar, allocator, tensor_detail::checkedSameTypeDivide<result_type>);
}

template <ReadableTensor Source, typename Scalar>
    requires TensorArithmeticCompatible<typename Source::value_type, Scalar>
[[nodiscard]] auto divide(const Source& source, Scalar scalar)
{
    using result_type = TensorArithmeticType<typename Source::value_type, Scalar>;
    return divide(source, scalar, tensor_detail::selectResultAllocator<result_type>(source));
}

template <ReadableTensor Source, typename Scalar, typename Allocator>
    requires TensorArithmeticCompatible<typename Source::value_type, Scalar> &&
             tensor_detail::AllocatorFor<Allocator, TensorArithmeticType<typename Source::value_type, Scalar>>
[[nodiscard]] auto divide(Scalar scalar, const Source& source, const Allocator& allocator)
    -> tensor_detail::UnaryTensorResult<
        TensorArithmeticType<typename Source::value_type, Scalar>, Allocator, Source>
{
    using result_type = TensorArithmeticType<typename Source::value_type, Scalar>;
    return tensor_detail::scalarArithmetic<true, result_type>(
        source, scalar, allocator, tensor_detail::checkedSameTypeDivide<result_type>);
}

template <ReadableTensor Source, typename Scalar>
    requires TensorArithmeticCompatible<typename Source::value_type, Scalar>
[[nodiscard]] auto divide(Scalar scalar, const Source& source)
{
    using result_type = TensorArithmeticType<typename Source::value_type, Scalar>;
    return divide(scalar, source, tensor_detail::selectResultAllocator<result_type>(source));
}

template <ReadableTensor Source, typename Function, typename Allocator>
    requires tensor_detail::AllocatorFor<Allocator, typename Source::value_type>
[[nodiscard]] auto transform(const Source& source, Function&& function, const Allocator& allocator)
    -> tensor_detail::UnaryTensorResult<typename Source::value_type, Allocator, Source>
{
    using value_type = typename Source::value_type;
    tensor_detail::TensorAccess::validate(source);
    tensor_detail::UnaryTensorResult<value_type, Allocator, Source> result(std::allocator_arg, allocator,
                                                                           source.extents());
    tensor_detail::unaryKernel(source, result, std::forward<Function>(function));
    return result;
}

template <ReadableTensor Source, typename Function>
[[nodiscard]] auto transform(const Source& source, Function&& function)
{
    using value_type = typename Source::value_type;
    if constexpr (requires { source.get_allocator(); })
    {
        using allocator_type = decltype(source.get_allocator());
        return transform(source, std::forward<Function>(function),
                         std::allocator_traits<allocator_type>::select_on_container_copy_construction(
                             source.get_allocator()));
    }
    else
    {
        return transform(source, std::forward<Function>(function), TensorAllocator<value_type>{});
    }
}

template <ReadableTensor Left, ReadableTensor Right>
    requires SameTensorValue<Left, Right>
[[nodiscard]] bool exactEqual(const Left& left, const Right& right)
{
    return tensor_detail::equalKernel(left, right, std::equal_to<typename Left::value_type>{});
}

template <ReadableTensor Left, ReadableTensor Right, typename Tolerance>
    requires SameTensorValue<Left, Right> && std::floating_point<typename Left::value_type> &&
             std::floating_point<Tolerance>
[[nodiscard]] bool approxEqual(const Left& left, const Right& right, Tolerance absoluteTolerance,
                               Tolerance relativeTolerance = Tolerance{})
{
    using comparison_type = std::common_type_t<typename Left::value_type, Tolerance>;
    return tensor_detail::equalKernel(left, right, [&](const auto& leftValue, const auto& rightValue) {
        if (leftValue == rightValue)
        {
            return true;
        }
        if (!std::isfinite(leftValue) || !std::isfinite(rightValue))
        {
            return false;
        }
        using std::abs;
        const auto promotedLeft = static_cast<comparison_type>(leftValue);
        const auto promotedRight = static_cast<comparison_type>(rightValue);
        const auto promotedAbsolute = static_cast<comparison_type>(absoluteTolerance);
        const auto promotedRelative = static_cast<comparison_type>(relativeTolerance);
        const auto difference = abs(promotedLeft - promotedRight);
        const auto scale = std::max(abs(promotedLeft), abs(promotedRight));
        if (std::isfinite(difference))
        {
            return difference <= promotedAbsolute + promotedRelative * scale;
        }

        // Finite values of opposite sign can have a difference outside the
        // comparison type even though each operand is representable. Scale
        // both sides of the tolerance relation before comparing so two
        // overflowing intermediates cannot compare equal as infinity.
        const auto divisor = std::max(scale, comparison_type{1});
        const auto normalizedDifference = abs(promotedLeft / divisor - promotedRight / divisor);
        const auto normalizedTolerance =
            promotedAbsolute / divisor + promotedRelative * (scale / divisor);
        return normalizedDifference <= normalizedTolerance;
    });
}

template <ReadableTensor Left, ReadableTensor Right>
    requires TensorArithmeticCompatible<typename Left::value_type, typename Right::value_type>
[[nodiscard]] auto operator+(const Left& left, const Right& right)
{
    return add(left, right);
}

template <ReadableTensor Left, ReadableTensor Right>
    requires TensorArithmeticCompatible<typename Left::value_type, typename Right::value_type>
[[nodiscard]] auto operator-(const Left& left, const Right& right)
{
    return subtract(left, right);
}

template <ReadableTensor Left, ReadableTensor Right>
    requires TensorArithmeticCompatible<typename Left::value_type, typename Right::value_type>
[[nodiscard]] auto operator*(const Left& left, const Right& right)
{
    return multiply(left, right);
}

template <ReadableTensor Source, typename Scalar>
    requires TensorArithmeticCompatible<typename Source::value_type, Scalar>
[[nodiscard]] auto operator+(const Source& source, Scalar scalar)
{
    return add(source, scalar);
}

template <ReadableTensor Source, typename Scalar>
    requires TensorArithmeticCompatible<typename Source::value_type, Scalar>
[[nodiscard]] auto operator+(Scalar scalar, const Source& source)
{
    return add(scalar, source);
}

template <ReadableTensor Source, typename Scalar>
    requires TensorArithmeticCompatible<typename Source::value_type, Scalar>
[[nodiscard]] auto operator-(const Source& source, Scalar scalar)
{
    return subtract(source, scalar);
}

template <ReadableTensor Source, typename Scalar>
    requires TensorArithmeticCompatible<typename Source::value_type, Scalar>
[[nodiscard]] auto operator-(Scalar scalar, const Source& source)
{
    return subtract(scalar, source);
}

template <ReadableTensor Source, typename Scalar>
    requires TensorArithmeticCompatible<typename Source::value_type, Scalar>
[[nodiscard]] auto operator*(const Source& source, Scalar scalar)
{
    return multiply(source, scalar);
}

template <ReadableTensor Source, typename Scalar>
    requires TensorArithmeticCompatible<typename Source::value_type, Scalar>
[[nodiscard]] auto operator*(Scalar scalar, const Source& source)
{
    return multiply(scalar, source);
}

template <ReadableTensor Left, ReadableTensor Right>
    requires TensorArithmeticCompatible<typename Left::value_type, typename Right::value_type>
[[nodiscard]] auto operator/(const Left& left, const Right& right)
{
    return divide(left, right);
}

template <ReadableTensor Source, typename Scalar>
    requires TensorArithmeticCompatible<typename Source::value_type, Scalar>
[[nodiscard]] auto operator/(const Source& source, Scalar scalar)
{
    return divide(source, scalar);
}

template <ReadableTensor Source, typename Scalar>
    requires TensorArithmeticCompatible<typename Source::value_type, Scalar>
[[nodiscard]] auto operator/(Scalar scalar, const Source& source)
{
    return divide(scalar, source);
}


/**
 * @brief Negate each logical value into an independent canonical owner, preserving dtype and extents.
 * @param source Readable owner or view, never modified; bool and non-arithmetic elements are excluded.
 * @param allocator Result allocator with the source value_type, used unchanged.
 * @return Tensor<Source::value_type, Allocator>, with no implicit narrow-integer promotion.
 * @details Signed integers reject lowest(); unsigned integers accept only zero. Cast explicitly before
 * negation when a wider signed result is needed. Floating negation uses native unary minus, including
 * signed zero, infinity, and NaN behavior under the caller's floating-point environment.
 * Empty inputs evaluate no elements. A nonempty result uses one element buffer and no element scratch.
 * @throws std::overflow_error An integer result is not representable in the source dtype.
 * @throws std::runtime_error A tracked borrowed source has expired (debug lifetime checking).
 * @throws std::bad_alloc Result or iteration metadata allocation fails; unpublished storage is released.
 * @note O(n * r + r * r) worst-case work and O(n + r) space, for n=size() and r=max(rank(),1).
 * Concurrent reads require stable storage without writes. Validation precedes result element allocation;
 * allocation failure may precede numeric overflow. Floating flags and traps are not intercepted.
 */
template <ReadableTensor Source, typename Allocator>
    requires TensorArithmeticCompatible<typename Source::value_type, typename Source::value_type> &&
             tensor_detail::AllocatorFor<Allocator, typename Source::value_type>
[[nodiscard]] tensor_detail::UnaryTensorResult<typename Source::value_type, Allocator, Source>
negate(const Source& source, const Allocator& allocator)
{
    return tensor_detail::unaryArithmetic(source, allocator,
                                         tensor_detail::checkedSameTypeNegate<typename Source::value_type>);
}

/**
 * @brief Negate values with the owner's copy-selected allocator, or the default allocator for views.
 * @param source Readable owner or view; never modified.
 * @return Fresh canonical owner preserving the source dtype and extents.
 * @throws std::overflow_error, std::runtime_error, std::bad_alloc As in negate(source, allocator).
 * @note Same complexity, numeric rules, lifetime requirements, and thread safety as the explicit overload.
 */
template <ReadableTensor Source>
    requires TensorArithmeticCompatible<typename Source::value_type, typename Source::value_type>
[[nodiscard]] auto negate(const Source& source)
{
    return negate(source, tensor_detail::selectResultAllocator<typename Source::value_type>(source));
}

/**
 * @brief Unary minus; equivalent to negate(source), including checked unsigned and narrow integer rules.
 * @param source Readable owner or view; never modified.
 * @return Fresh canonical owner preserving dtype and extents, with negate's allocator selection.
 * @throws std::overflow_error, std::runtime_error, std::bad_alloc As in negate(source).
 * @note Same complexity and thread safety as negate(source).
 */
template <ReadableTensor Source>
    requires TensorArithmeticCompatible<typename Source::value_type, typename Source::value_type>
[[nodiscard]] auto operator-(const Source& source)
{
    return negate(source);
}

/**
 * @brief Materialize absolute values without changing dtype or extents.
 * @param source Readable owner or view, never modified; bool and non-arithmetic elements are excluded.
 * @param allocator Result allocator with the source value_type, used unchanged.
 * @return Independent canonical Tensor<Source::value_type, Allocator>; unsigned values are copied.
 * @details Signed lowest() throws before negation; floating values use std::fabs, including -0 -> +0,
 * negative infinity -> positive infinity, and NaN category preservation on IEC 559 implementations.
 * No NaN sign/payload or floating-environment restoration guarantee is made.
 * @throws std::overflow_error A signed integer absolute value is not representable.
 * @throws std::runtime_error A tracked borrowed source has expired (debug lifetime checking).
 * @throws std::bad_alloc Result or iteration metadata allocation fails; unpublished storage is released.
 * @note Same allocation, validation, empty-input, complexity, and thread-safety rules as negate(source, allocator).
 */
template <ReadableTensor Source, typename Allocator>
    requires TensorArithmeticCompatible<typename Source::value_type, typename Source::value_type> &&
             tensor_detail::AllocatorFor<Allocator, typename Source::value_type>
[[nodiscard]] tensor_detail::UnaryTensorResult<typename Source::value_type, Allocator, Source>
abs(const Source& source, const Allocator& allocator)
{
    return tensor_detail::unaryArithmetic(source, allocator,
                                         tensor_detail::checkedSameTypeAbs<typename Source::value_type>);
}

/**
 * @brief Materialize absolute values with owner copy-selection, or the default allocator for views.
 * @param source Readable owner or view; never modified.
 * @return Fresh canonical owner preserving the source dtype and extents.
 * @throws std::overflow_error, std::runtime_error, std::bad_alloc As in abs(source, allocator).
 * @note Same complexity, numeric rules, lifetime requirements, and thread safety as the explicit overload.
 */
template <ReadableTensor Source>
    requires TensorArithmeticCompatible<typename Source::value_type, typename Source::value_type>
[[nodiscard]] auto abs(const Source& source)
{
    return abs(source, tensor_detail::selectResultAllocator<typename Source::value_type>(source));
}

// Floating math shares negate/abs's owner/view, allocation, lifetime, and exception-safety contract.
// Only float, double, and long double participate; integers require an explicit cast before evaluation.
// Each element uses its typed standard-library overload. Domain/range errors are not translated into
// C++ exceptions: native NaN/infinity/underflow behavior, errno, flags, and enabled traps apply.
// No errno/flag clearing, restoration, cross-platform accuracy, or fast-math guarantee is provided.
// Validation precedes element allocation; std::bad_alloc and tracked-lifetime std::runtime_error propagate.
// Empty inputs evaluate no math, rank-zero inputs evaluate once, and source storage is never modified.
// Complexity: O(n * r + r * r) traversal/setup plus n scalar math calls, O(n + r) result/metadata space,
// where n=size() and r=max(rank(),1). Concurrent reads require stable storage without concurrent writes.

/** @brief Materialize typed std::sqrt values with unchanged dtype and extents.
 * @param source Readable floating owner/view. @param allocator Same-value-type allocator, used unchanged.
 * @return Independent canonical owner with native floating results; no Tensor numeric exceptions.
 * @throws std::bad_alloc Allocation fails. @throws std::runtime_error A tracked borrowed source expired. */
template <tensor_detail::FloatingMathTensor Source, typename Allocator>
    requires tensor_detail::AllocatorFor<Allocator, typename Source::value_type>
[[nodiscard]] tensor_detail::UnaryTensorResult<typename Source::value_type, Allocator, Source>
sqrt(const Source& source, const Allocator& allocator)
{
    using value_type = typename Source::value_type;
    return tensor_detail::unaryArithmetic(source, allocator, [](value_type value) { return std::sqrt(value); });
}

/** @brief Materialize square roots using owner copy-selection, or the default allocator for views.
 * @param source Readable floating owner/view, never modified.
 * @return Independent same-dtype owner; all rules of sqrt(source, allocator) apply. */
template <tensor_detail::FloatingMathTensor Source>
[[nodiscard]] auto sqrt(const Source& source)
{
    return sqrt(source, tensor_detail::selectResultAllocator<typename Source::value_type>(source));
}

/** @brief Materialize typed std::exp values with unchanged dtype and extents.
 * @param source Readable floating owner/view. @param allocator Same-value-type allocator, used unchanged.
 * @return Independent canonical owner with native floating results; no Tensor numeric exceptions.
 * @throws std::bad_alloc Allocation fails. @throws std::runtime_error A tracked borrowed source expired. */
template <tensor_detail::FloatingMathTensor Source, typename Allocator>
    requires tensor_detail::AllocatorFor<Allocator, typename Source::value_type>
[[nodiscard]] tensor_detail::UnaryTensorResult<typename Source::value_type, Allocator, Source>
exp(const Source& source, const Allocator& allocator)
{
    using value_type = typename Source::value_type;
    return tensor_detail::unaryArithmetic(source, allocator, [](value_type value) { return std::exp(value); });
}

/** @brief Materialize exponentials using owner copy-selection, or the default allocator for views.
 * @param source Readable floating owner/view, never modified.
 * @return Independent same-dtype owner; all rules of exp(source, allocator) apply. */
template <tensor_detail::FloatingMathTensor Source>
[[nodiscard]] auto exp(const Source& source)
{
    return exp(source, tensor_detail::selectResultAllocator<typename Source::value_type>(source));
}

/** @brief Materialize typed std::log (natural logarithm) values with unchanged dtype and extents.
 * @param source Readable floating owner/view. @param allocator Same-value-type allocator, used unchanged.
 * @return Independent canonical owner with native floating results; no Tensor numeric exceptions.
 * @throws std::bad_alloc Allocation fails. @throws std::runtime_error A tracked borrowed source expired. */
template <tensor_detail::FloatingMathTensor Source, typename Allocator>
    requires tensor_detail::AllocatorFor<Allocator, typename Source::value_type>
[[nodiscard]] tensor_detail::UnaryTensorResult<typename Source::value_type, Allocator, Source>
log(const Source& source, const Allocator& allocator)
{
    using value_type = typename Source::value_type;
    return tensor_detail::unaryArithmetic(source, allocator, [](value_type value) { return std::log(value); });
}

/** @brief Materialize natural logarithms using owner copy-selection, or the default allocator for views.
 * @param source Readable floating owner/view, never modified.
 * @return Independent same-dtype owner; all rules of log(source, allocator) apply. */
template <tensor_detail::FloatingMathTensor Source>
[[nodiscard]] auto log(const Source& source)
{
    return log(source, tensor_detail::selectResultAllocator<typename Source::value_type>(source));
}

namespace tensor_detail
{

template <typename Operand>
[[nodiscard]] consteval auto compoundOperandTypeIdentity()
{
    if constexpr (ReadableTensor<Operand>)
    {
        return std::type_identity<typename Operand::value_type>{};
    }
    else
    {
        return std::type_identity<std::remove_cvref_t<Operand>>{};
    }
}

template <typename Operand>
using CompoundOperandValue = typename decltype(compoundOperandTypeIdentity<Operand>())::type;

template <typename Destination, typename Operand>
concept CompoundCompatible = WritableTensor<Destination> && (!std::is_const_v<Destination>) &&
    CastValue<typename Destination::value_type> &&
    TensorArithmeticCompatible<typename Destination::value_type, CompoundOperandValue<Operand>>;

template <typename Destination>
[[nodiscard]] auto selectCompoundScratchAllocator(const Destination& destination)
{
    if constexpr (requires { destination.get_allocator(); })
    {
        return destination.get_allocator();
    }
    else
    {
        return TensorAllocator<typename Destination::value_type>{};
    }
}

template <ReadableTensor Source, WritableTensor Destination>
void commitCompoundResult(const Source& scratch, Destination& destination)
{
    static_assert(std::is_nothrow_copy_assignable_v<typename Destination::value_type>);
    // Fresh scratch storage is disjoint. copyKernel completes validation and all plan/traversal
    // allocations before its first store. For validated layouts, each counted offset (including
    // carry/rewind intermediates) addresses an in-range coordinate, so offset checks cannot throw.
    // The remaining stores are nonthrowing primitive assignments; storage and lifetimes are retained.
    copyKernel(scratch, destination);
}

template <typename Destination, typename Operand, typename Allocator, typename Operation>
    requires CompoundCompatible<Destination, Operand> &&
             AllocatorFor<Allocator, typename Destination::value_type>
Destination& compoundArithmetic(Destination& destination, const Operand& operand,
                                const Allocator& allocator, Operation operation)
{
    using value_type = typename Destination::value_type;
    using result_type = TensorArithmeticType<value_type, CompoundOperandValue<Operand>>;
    TensorAccess::validate(destination);
    if (!destination.layout().isInjective())
    {
        throw std::invalid_argument("Tensor compound destination must be injective");
    }
    if constexpr (ReadableTensor<Operand>)
    {
        TensorAccess::validate(operand);
    }

    constexpr auto destinationRank = tensorStaticRankValue<Destination>;
    constexpr auto operandRank = [] {
        if constexpr (ReadableTensor<Operand>)
        {
            return tensorStaticRankValue<Operand>;
        }
        else
        {
            return kDynamicTensorRank;
        }
    }();
    if constexpr (ReadableTensor<Operand> && destinationRank != kDynamicTensorRank &&
                  operandRank != kDynamicTensorRank && operandRank > destinationRank)
    {
        throw std::invalid_argument("Tensor compound assignment cannot change destination extents");
    }
    else
    {
        if constexpr (ReadableTensor<Operand>)
        {
            const auto extents = broadcastExtents(destination.layout(), operand.layout());
            if (extents != destination.extents())
            {
                throw std::invalid_argument("Tensor compound assignment cannot change destination extents");
            }
        }
        if (destination.size() == 0)
        {
            return destination;
        }

        if constexpr (ReadableTensor<Operand>)
        {
            Tensor<value_type, Allocator, tensorStaticRankValue<Destination>> scratch(
                std::allocator_arg, allocator, destination.extents());
            binaryKernel(destination, operand, scratch, [operation](const auto& left, const auto& right) {
                return checkedScalarCast<value_type>(
                    std::invoke(operation, static_cast<result_type>(left), static_cast<result_type>(right)));
            });
            commitCompoundResult(scratch, destination);
        }
        else
        {
            const result_type scalar = static_cast<result_type>(operand);
            Tensor<value_type, Allocator, tensorStaticRankValue<Destination>> scratch(
                std::allocator_arg, allocator, destination.extents());
            unaryKernel(destination, scratch, [scalar, operation](const auto& left) {
                return checkedScalarCast<value_type>(
                    std::invoke(operation, static_cast<result_type>(left), scalar));
            });
            commitCompoundResult(scratch, destination);
        }
        return destination;
    }
}

} // namespace tensor_detail

/**
 * @brief Add into a non-const lvalue owner or writable view without changing its storage or extents.
 * @tparam Destination Non-const owning Tensor or writable borrowed/shared view.
 * @tparam Operand Readable Tensor or scalar with a supported arithmetic promotion pair.
 * @tparam Allocator Scratch allocator whose value_type matches the destination.
 * @param destination Existing injective mapping to update.
 * @param operand Scalar or tensor RHS; tensor broadcasting must preserve destination extents.
 * @param scratchAllocator Exact allocator instance for the operation-local element buffer.
 * @details All compound operations compute in TensorArithmeticType, then use checked cast rules
 * back to the destination type. Tensor operands must broadcast into exactly the destination extents.
 * Scalars and overlapping RHS values are read before any destination write. One destination-typed
 * scratch element buffer stages a nonempty update; empty updates evaluate and allocate no elements.
 * Explicit scratch allocators must match the destination value type and are used unchanged.
 * Otherwise the destination owner's exact allocator is used without SOCCC; views use TensorAllocator.
 * All C++ exceptions leave destination values, mapping, storage, allocator, and view validity unchanged.
 * Metadata may allocate separately. Callers exclude concurrent/reentrant alias mutation and FP traps.
 * @return The original destination by reference, for chaining.
 * @note Complexity: O(n * r + r * r) worst-case work and O(n + r) scratch, for n=size() and r=max(rank(),1).
 * @note Thread-safety: Caller synchronizes every conflicting access to destination and RHS aliases.
 * @throws std::invalid_argument On incompatible/destination-changing shapes or a noninjective destination.
 * @throws std::domain_error On invalid checked conversions or integral division by zero.
 * @throws std::overflow_error On checked arithmetic or destination-conversion range failure.
 * @throws std::bad_alloc On scratch or metadata allocation failure, including pre-commit metadata.
 * @throws std::runtime_error On expired borrowed inputs in assertions-enabled builds.
 */
template <typename Destination, typename Operand, typename Allocator>
    requires tensor_detail::CompoundCompatible<Destination, Operand> &&
             tensor_detail::AllocatorFor<Allocator, typename Destination::value_type>
Destination& addAssign(Destination& destination, const Operand& operand, const Allocator& scratchAllocator)
{
    using result_type = TensorArithmeticType<typename Destination::value_type,
                                             tensor_detail::CompoundOperandValue<Operand>>;
    return tensor_detail::compoundArithmetic(
        destination, operand, scratchAllocator, tensor_detail::checkedSameTypeAdd<result_type>);
}

/**
 * @brief Add using the destination's default scratch selection and transactional addAssign contract.
 * @param destination Non-const lvalue owner or writable view; storage and extents stay fixed.
 * @param operand Scalar or broadcastable tensor RHS.
 * @return The original destination by reference.
 * @throws std::invalid_argument On incompatible/destination-changing shapes or a noninjective destination.
 * @throws std::domain_error On invalid checked conversions or integral division by zero.
 * @throws std::overflow_error On checked arithmetic or destination-conversion range failure.
 * @throws std::bad_alloc On scratch or metadata allocation failure.
 * @throws std::runtime_error On expired borrowed inputs in assertions-enabled builds.
 * @see addAssign for the common transaction, allocator, complexity, and concurrency contract.
 */
template <typename Destination, typename Operand>
    requires tensor_detail::CompoundCompatible<Destination, Operand>
Destination& addAssign(Destination& destination, const Operand& operand)
{
    return addAssign(destination, operand, tensor_detail::selectCompoundScratchAllocator(destination));
}

/**
 * @brief Subtract with the transactional addAssign destination and scratch contract.
 * @param destination Non-const lvalue owner or writable view; storage and extents stay fixed.
 * @param operand Scalar or broadcastable tensor RHS.
 * @param scratchAllocator Exact destination-value-typed allocator for operation-local scratch.
 * @return The original destination by reference.
 * @throws std::invalid_argument On incompatible/destination-changing shapes or a noninjective destination.
 * @throws std::domain_error On invalid checked conversions or integral division by zero.
 * @throws std::overflow_error On checked arithmetic or destination-conversion range failure.
 * @throws std::bad_alloc On scratch or metadata allocation failure.
 * @throws std::runtime_error On expired borrowed inputs in assertions-enabled builds.
 * @see addAssign for the common transaction, allocator, complexity, and concurrency contract.
 */
template <typename Destination, typename Operand, typename Allocator>
    requires tensor_detail::CompoundCompatible<Destination, Operand> &&
             tensor_detail::AllocatorFor<Allocator, typename Destination::value_type>
Destination& subtractAssign(Destination& destination, const Operand& operand, const Allocator& scratchAllocator)
{
    using result_type = TensorArithmeticType<typename Destination::value_type,
                                             tensor_detail::CompoundOperandValue<Operand>>;
    return tensor_detail::compoundArithmetic(
        destination, operand, scratchAllocator, tensor_detail::checkedSameTypeSubtract<result_type>);
}

/**
 * @brief Subtract using the destination's default scratch selection and transactional addAssign contract.
 * @param destination Non-const lvalue owner or writable view; storage and extents stay fixed.
 * @param operand Scalar or broadcastable tensor RHS.
 * @return The original destination by reference.
 * @throws std::invalid_argument On incompatible/destination-changing shapes or a noninjective destination.
 * @throws std::domain_error On invalid checked conversions or integral division by zero.
 * @throws std::overflow_error On checked arithmetic or destination-conversion range failure.
 * @throws std::bad_alloc On scratch or metadata allocation failure.
 * @throws std::runtime_error On expired borrowed inputs in assertions-enabled builds.
 * @see addAssign for the common transaction, allocator, complexity, and concurrency contract.
 */
template <typename Destination, typename Operand>
    requires tensor_detail::CompoundCompatible<Destination, Operand>
Destination& subtractAssign(Destination& destination, const Operand& operand)
{
    return subtractAssign(destination, operand, tensor_detail::selectCompoundScratchAllocator(destination));
}

/**
 * @brief Multiply with the transactional addAssign destination and scratch contract.
 * @param destination Non-const lvalue owner or writable view; storage and extents stay fixed.
 * @param operand Scalar or broadcastable tensor RHS.
 * @param scratchAllocator Exact destination-value-typed allocator for operation-local scratch.
 * @return The original destination by reference.
 * @throws std::invalid_argument On incompatible/destination-changing shapes or a noninjective destination.
 * @throws std::domain_error On invalid checked conversions or integral division by zero.
 * @throws std::overflow_error On checked arithmetic or destination-conversion range failure.
 * @throws std::bad_alloc On scratch or metadata allocation failure.
 * @throws std::runtime_error On expired borrowed inputs in assertions-enabled builds.
 * @see addAssign for the common transaction, allocator, complexity, and concurrency contract.
 */
template <typename Destination, typename Operand, typename Allocator>
    requires tensor_detail::CompoundCompatible<Destination, Operand> &&
             tensor_detail::AllocatorFor<Allocator, typename Destination::value_type>
Destination& multiplyAssign(Destination& destination, const Operand& operand, const Allocator& scratchAllocator)
{
    using result_type = TensorArithmeticType<typename Destination::value_type,
                                             tensor_detail::CompoundOperandValue<Operand>>;
    return tensor_detail::compoundArithmetic(
        destination, operand, scratchAllocator, tensor_detail::checkedSameTypeMultiply<result_type>);
}

/**
 * @brief Multiply using the destination's default scratch selection and transactional addAssign contract.
 * @param destination Non-const lvalue owner or writable view; storage and extents stay fixed.
 * @param operand Scalar or broadcastable tensor RHS.
 * @return The original destination by reference.
 * @throws std::invalid_argument On incompatible/destination-changing shapes or a noninjective destination.
 * @throws std::domain_error On invalid checked conversions or integral division by zero.
 * @throws std::overflow_error On checked arithmetic or destination-conversion range failure.
 * @throws std::bad_alloc On scratch or metadata allocation failure.
 * @throws std::runtime_error On expired borrowed inputs in assertions-enabled builds.
 * @see addAssign for the common transaction, allocator, complexity, and concurrency contract.
 */
template <typename Destination, typename Operand>
    requires tensor_detail::CompoundCompatible<Destination, Operand>
Destination& multiplyAssign(Destination& destination, const Operand& operand)
{
    return multiplyAssign(destination, operand, tensor_detail::selectCompoundScratchAllocator(destination));
}

/**
 * @brief Divide with the transactional addAssign destination and scratch contract.
 * @param destination Non-const lvalue owner or writable view; storage and extents stay fixed.
 * @param operand Scalar or broadcastable tensor RHS.
 * @param scratchAllocator Exact destination-value-typed allocator for operation-local scratch.
 * @return The original destination by reference.
 * @throws std::invalid_argument On incompatible/destination-changing shapes or a noninjective destination.
 * @throws std::domain_error On invalid checked conversions or integral division by zero.
 * @throws std::overflow_error On checked arithmetic or destination-conversion range failure.
 * @throws std::bad_alloc On scratch or metadata allocation failure.
 * @throws std::runtime_error On expired borrowed inputs in assertions-enabled builds.
 * @see addAssign for the common transaction, allocator, complexity, and concurrency contract.
 */
template <typename Destination, typename Operand, typename Allocator>
    requires tensor_detail::CompoundCompatible<Destination, Operand> &&
             tensor_detail::AllocatorFor<Allocator, typename Destination::value_type>
Destination& divideAssign(Destination& destination, const Operand& operand, const Allocator& scratchAllocator)
{
    using result_type = TensorArithmeticType<typename Destination::value_type,
                                             tensor_detail::CompoundOperandValue<Operand>>;
    return tensor_detail::compoundArithmetic(
        destination, operand, scratchAllocator, tensor_detail::checkedSameTypeDivide<result_type>);
}

/**
 * @brief Divide using the destination's default scratch selection and transactional addAssign contract.
 * @param destination Non-const lvalue owner or writable view; storage and extents stay fixed.
 * @param operand Scalar or broadcastable tensor RHS.
 * @return The original destination by reference.
 * @throws std::invalid_argument On incompatible/destination-changing shapes or a noninjective destination.
 * @throws std::domain_error On invalid checked conversions or integral division by zero.
 * @throws std::overflow_error On checked arithmetic or destination-conversion range failure.
 * @throws std::bad_alloc On scratch or metadata allocation failure.
 * @throws std::runtime_error On expired borrowed inputs in assertions-enabled builds.
 * @see addAssign for the common transaction, allocator, complexity, and concurrency contract.
 */
template <typename Destination, typename Operand>
    requires tensor_detail::CompoundCompatible<Destination, Operand>
Destination& divideAssign(Destination& destination, const Operand& operand)
{
    return divideAssign(destination, operand, tensor_detail::selectCompoundScratchAllocator(destination));
}

/**
 * @brief Transactional compound operator forwarding to addAssign.
 * @param destination Non-const lvalue owner or writable view; storage and extents stay fixed.
 * @param operand Scalar or broadcastable tensor RHS.
 * @return The original destination by reference.
 * @throws std::invalid_argument On incompatible/destination-changing shapes or a noninjective destination.
 * @throws std::domain_error On invalid checked conversions or integral division by zero.
 * @throws std::overflow_error On checked arithmetic or destination-conversion range failure.
 * @throws std::bad_alloc On scratch or metadata allocation failure.
 * @throws std::runtime_error On expired borrowed inputs in assertions-enabled builds.
 * @see addAssign for the common transaction, allocator, complexity, and concurrency contract.
 */
template <typename Destination, typename Operand>
    requires tensor_detail::CompoundCompatible<Destination, Operand>
Destination& operator+=(Destination& destination, const Operand& operand)
{
    return addAssign(destination, operand);
}

/**
 * @brief Transactional compound operator forwarding to subtractAssign.
 * @param destination Non-const lvalue owner or writable view; storage and extents stay fixed.
 * @param operand Scalar or broadcastable tensor RHS.
 * @return The original destination by reference.
 * @throws std::invalid_argument On incompatible/destination-changing shapes or a noninjective destination.
 * @throws std::domain_error On invalid checked conversions or integral division by zero.
 * @throws std::overflow_error On checked arithmetic or destination-conversion range failure.
 * @throws std::bad_alloc On scratch or metadata allocation failure.
 * @throws std::runtime_error On expired borrowed inputs in assertions-enabled builds.
 * @see addAssign for the common transaction, allocator, complexity, and concurrency contract.
 */
template <typename Destination, typename Operand>
    requires tensor_detail::CompoundCompatible<Destination, Operand>
Destination& operator-=(Destination& destination, const Operand& operand)
{
    return subtractAssign(destination, operand);
}

/**
 * @brief Transactional compound operator forwarding to multiplyAssign.
 * @param destination Non-const lvalue owner or writable view; storage and extents stay fixed.
 * @param operand Scalar or broadcastable tensor RHS.
 * @return The original destination by reference.
 * @throws std::invalid_argument On incompatible/destination-changing shapes or a noninjective destination.
 * @throws std::domain_error On invalid checked conversions or integral division by zero.
 * @throws std::overflow_error On checked arithmetic or destination-conversion range failure.
 * @throws std::bad_alloc On scratch or metadata allocation failure.
 * @throws std::runtime_error On expired borrowed inputs in assertions-enabled builds.
 * @see addAssign for the common transaction, allocator, complexity, and concurrency contract.
 */
template <typename Destination, typename Operand>
    requires tensor_detail::CompoundCompatible<Destination, Operand>
Destination& operator*=(Destination& destination, const Operand& operand)
{
    return multiplyAssign(destination, operand);
}

/**
 * @brief Transactional compound operator forwarding to divideAssign.
 * @param destination Non-const lvalue owner or writable view; storage and extents stay fixed.
 * @param operand Scalar or broadcastable tensor RHS.
 * @return The original destination by reference.
 * @throws std::invalid_argument On incompatible/destination-changing shapes or a noninjective destination.
 * @throws std::domain_error On invalid checked conversions or integral division by zero.
 * @throws std::overflow_error On checked arithmetic or destination-conversion range failure.
 * @throws std::bad_alloc On scratch or metadata allocation failure.
 * @throws std::runtime_error On expired borrowed inputs in assertions-enabled builds.
 * @see addAssign for the common transaction, allocator, complexity, and concurrency contract.
 */
template <typename Destination, typename Operand>
    requires tensor_detail::CompoundCompatible<Destination, Operand>
Destination& operator/=(Destination& destination, const Operand& operand)
{
    return divideAssign(destination, operand);
}


} // namespace fat_p
