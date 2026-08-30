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

#include "Tensor.h"
#include "TensorKernels.h"

#include <algorithm>
#include <cmath>
#include <concepts>
#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace fat_p
{

namespace tensor_detail
{

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

template <typename Allocator, typename Value>
concept AllocatorFor = requires {
    typename std::allocator_traits<Allocator>::value_type;
} && std::same_as<typename std::allocator_traits<Allocator>::value_type, Value>;

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

} // namespace tensor_detail

template <typename Left, typename Right>
concept SameTensorValue = ReadableTensor<Left> && ReadableTensor<Right> &&
    std::same_as<typename Left::value_type, typename Right::value_type>;

template <ReadableTensor Left, ReadableTensor Right, typename Allocator>
    requires SameTensorValue<Left, Right>
[[nodiscard]] auto add(const Left& left, const Right& right, const Allocator& allocator)
    -> Tensor<typename Left::value_type, Allocator>
{
    using value_type = typename Left::value_type;
    const auto extents = tensor_detail::TensorIterationPlan::broadcastExtents(
        {std::cref(left.layout()), std::cref(right.layout())});
    Tensor<value_type, Allocator> result(std::allocator_arg, allocator, extents);
    tensor_detail::binaryKernel(left, right, result, tensor_detail::checkedSameTypeAdd<value_type>);
    return result;
}

template <ReadableTensor Left, ReadableTensor Right>
    requires SameTensorValue<Left, Right>
[[nodiscard]] auto add(const Left& left, const Right& right)
{
    using value_type = typename Left::value_type;
    if constexpr (requires { left.get_allocator(); })
    {
        using allocator_type = decltype(left.get_allocator());
        return add(left, right,
                   std::allocator_traits<allocator_type>::select_on_container_copy_construction(
                       left.get_allocator()));
    }
    else if constexpr (requires { right.get_allocator(); })
    {
        using allocator_type = decltype(right.get_allocator());
        return add(left, right,
                   std::allocator_traits<allocator_type>::select_on_container_copy_construction(
                       right.get_allocator()));
    }
    else
    {
        return add(left, right, TensorAllocator<value_type>{});
    }
}

template <ReadableTensor Left, ReadableTensor Right, typename Allocator>
    requires SameTensorValue<Left, Right>
[[nodiscard]] auto subtract(const Left& left, const Right& right, const Allocator& allocator)
    -> Tensor<typename Left::value_type, Allocator>
{
    using value_type = typename Left::value_type;
    const auto extents = tensor_detail::TensorIterationPlan::broadcastExtents(
        {std::cref(left.layout()), std::cref(right.layout())});
    Tensor<value_type, Allocator> result(std::allocator_arg, allocator, extents);
    tensor_detail::binaryKernel(left, right, result, tensor_detail::checkedSameTypeSubtract<value_type>);
    return result;
}

template <ReadableTensor Left, ReadableTensor Right>
    requires SameTensorValue<Left, Right>
[[nodiscard]] auto subtract(const Left& left, const Right& right)
{
    using value_type = typename Left::value_type;
    if constexpr (requires { left.get_allocator(); })
    {
        using allocator_type = decltype(left.get_allocator());
        return subtract(left, right,
                        std::allocator_traits<allocator_type>::select_on_container_copy_construction(
                            left.get_allocator()));
    }
    else if constexpr (requires { right.get_allocator(); })
    {
        using allocator_type = decltype(right.get_allocator());
        return subtract(left, right,
                        std::allocator_traits<allocator_type>::select_on_container_copy_construction(
                            right.get_allocator()));
    }
    else
    {
        return subtract(left, right, TensorAllocator<value_type>{});
    }
}

template <ReadableTensor Left, ReadableTensor Right, typename Allocator>
    requires SameTensorValue<Left, Right>
[[nodiscard]] auto multiply(const Left& left, const Right& right, const Allocator& allocator)
    -> Tensor<typename Left::value_type, Allocator>
{
    using value_type = typename Left::value_type;
    const auto extents = tensor_detail::TensorIterationPlan::broadcastExtents(
        {std::cref(left.layout()), std::cref(right.layout())});
    Tensor<value_type, Allocator> result(std::allocator_arg, allocator, extents);
    tensor_detail::binaryKernel(left, right, result, tensor_detail::checkedSameTypeMultiply<value_type>);
    return result;
}

template <ReadableTensor Left, ReadableTensor Right>
    requires SameTensorValue<Left, Right>
[[nodiscard]] auto multiply(const Left& left, const Right& right)
{
    using value_type = typename Left::value_type;
    if constexpr (requires { left.get_allocator(); })
    {
        using allocator_type = decltype(left.get_allocator());
        return multiply(left, right,
                        std::allocator_traits<allocator_type>::select_on_container_copy_construction(
                            left.get_allocator()));
    }
    else if constexpr (requires { right.get_allocator(); })
    {
        using allocator_type = decltype(right.get_allocator());
        return multiply(left, right,
                        std::allocator_traits<allocator_type>::select_on_container_copy_construction(
                            right.get_allocator()));
    }
    else
    {
        return multiply(left, right, TensorAllocator<value_type>{});
    }
}

template <ReadableTensor Source, typename Function, typename Allocator>
[[nodiscard]] auto transform(const Source& source, Function&& function, const Allocator& allocator)
    -> Tensor<typename Source::value_type, Allocator>
{
    using value_type = typename Source::value_type;
    Tensor<value_type, Allocator> result(std::allocator_arg, allocator, source.extents());
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
        const auto difference = abs(leftValue - rightValue);
        const auto scale = std::max(abs(leftValue), abs(rightValue));
        return difference <= absoluteTolerance + relativeTolerance * scale;
    });
}

template <ReadableTensor Left, ReadableTensor Right>
    requires SameTensorValue<Left, Right>
[[nodiscard]] auto operator+(const Left& left, const Right& right)
{
    return add(left, right);
}

template <ReadableTensor Left, ReadableTensor Right>
    requires SameTensorValue<Left, Right>
[[nodiscard]] auto operator-(const Left& left, const Right& right)
{
    return subtract(left, right);
}

template <ReadableTensor Left, ReadableTensor Right>
    requires SameTensorValue<Left, Right>
[[nodiscard]] auto operator*(const Left& left, const Right& right)
{
    return multiply(left, right);
}

} // namespace fat_p
