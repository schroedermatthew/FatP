#pragma once

/*
FATP_META:
  meta_version: 1
  component: Tensor
  file_role: internal_header
  path: include/fat_p/tensor/TensorDType.h
  namespace: fat_p
  layer: Domain
  summary: "Canonical implementation-independent dtype vocabulary for dynamic Tensor values."
  api_stability: in_work
  related:
    docs:
      - components/Tensor/docs/Design Note - Tensor Semantic Contract.md
      - components/Tensor/docs/Design Note - Tensor Architecture Additions Plan.md
      - components/Tensor/docs/User Manual - Tensor.md
    headers:
      - include/fat_p/Tensor.h
    tests:
      - components/Tensor/tests/test_Tensor.cpp
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

/**
 * @file TensorDType.h
 * @brief Canonical identifiers and names for supported Tensor scalar dtypes.
 * @details Metadata operations are constexpr, allocate no memory, and read only
 * immutable inline state, so concurrent calls are safe.
 */

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <type_traits>

namespace fat_p
{

/** @brief Implementation-independent identifier for a supported Tensor scalar dtype. */
enum class TensorDType : std::uint8_t
{
    Int8 = 1,
    Uint8 = 2,
    Int16 = 3,
    Uint16 = 4,
    Int32 = 5,
    Uint32 = 6,
    Int64 = 7,
    Uint64 = 8,
    Float32 = 9,
    Float64 = 10
};

/** @brief Runtime metadata for one canonical Tensor dtype. */
struct TensorDTypeDescriptor
{
    TensorDType dtype;     ///< Explicit canonical identifier.
    std::string_view name; ///< Canonical lowercase name.
    std::size_t bitWidth;  ///< Logical scalar width in bits.
};

namespace tensor_detail
{

template <typename T>
struct TensorDTypeTraits;

template <>
struct TensorDTypeTraits<std::int8_t>
{
    static constexpr TensorDType kDType = TensorDType::Int8;
};

template <>
struct TensorDTypeTraits<std::uint8_t>
{
    static constexpr TensorDType kDType = TensorDType::Uint8;
};

template <>
struct TensorDTypeTraits<std::int16_t>
{
    static constexpr TensorDType kDType = TensorDType::Int16;
};

template <>
struct TensorDTypeTraits<std::uint16_t>
{
    static constexpr TensorDType kDType = TensorDType::Uint16;
};

template <>
struct TensorDTypeTraits<std::int32_t>
{
    static constexpr TensorDType kDType = TensorDType::Int32;
};

template <>
struct TensorDTypeTraits<std::uint32_t>
{
    static constexpr TensorDType kDType = TensorDType::Uint32;
};

template <>
struct TensorDTypeTraits<std::int64_t>
{
    static constexpr TensorDType kDType = TensorDType::Int64;
};

template <>
struct TensorDTypeTraits<std::uint64_t>
{
    static constexpr TensorDType kDType = TensorDType::Uint64;
};

template <>
struct TensorDTypeTraits<float>
{
    static constexpr TensorDType kDType = TensorDType::Float32;
};

template <>
struct TensorDTypeTraits<double>
{
    static constexpr TensorDType kDType = TensorDType::Float64;
};

inline constexpr std::array<TensorDTypeDescriptor, 10> kTensorDTypeDescriptors{{
    {TensorDType::Int8, "int8", 8},
    {TensorDType::Uint8, "uint8", 8},
    {TensorDType::Int16, "int16", 16},
    {TensorDType::Uint16, "uint16", 16},
    {TensorDType::Int32, "int32", 32},
    {TensorDType::Uint32, "uint32", 32},
    {TensorDType::Int64, "int64", 64},
    {TensorDType::Uint64, "uint64", 64},
    {TensorDType::Float32, "float32", 32},
    {TensorDType::Float64, "float64", 64},
}};

} // namespace tensor_detail

/** @brief True when T has a canonical Tensor dtype entry. */
template <typename T>
concept TensorDTypeElement = requires { tensor_detail::TensorDTypeTraits<std::remove_cvref_t<T>>::kDType; };

/** @brief Returns the canonical dtype for T without using RTTI or compiler type text. */
template <TensorDTypeElement T>
[[nodiscard]] consteval TensorDType tensorDTypeOf() noexcept
{
    return tensor_detail::TensorDTypeTraits<std::remove_cvref_t<T>>::kDType;
}

/** @brief Returns all canonical dtype descriptors in identifier order. */
[[nodiscard]] constexpr const std::array<TensorDTypeDescriptor, 10>& tensorDTypeDescriptors() noexcept
{
    return tensor_detail::kTensorDTypeDescriptors;
}

/** @brief Returns a descriptor pointer, or nullptr when dtype is not recognized. */
[[nodiscard]] constexpr const TensorDTypeDescriptor* tensorDTypeDescriptor(TensorDType dtype) noexcept
{
    const auto id = static_cast<std::uint8_t>(dtype);
    if (id == 0 || id > tensor_detail::kTensorDTypeDescriptors.size())
    {
        return nullptr;
    }

    const auto& descriptor = tensor_detail::kTensorDTypeDescriptors[id - 1];
    return descriptor.dtype == dtype ? &descriptor : nullptr;
}

/** @brief Decodes a canonical numeric identifier, returning nullopt for unknown values. */
[[nodiscard]] constexpr std::optional<TensorDType> tensorDTypeFromId(std::uint8_t id) noexcept
{
    const auto dtype = static_cast<TensorDType>(id);
    return tensorDTypeDescriptor(dtype) == nullptr ? std::nullopt : std::optional<TensorDType>{dtype};
}

/** @brief Returns the canonical lowercase name, or "unknown" for an invalid enum value. */
[[nodiscard]] constexpr std::string_view tensorDTypeName(TensorDType dtype) noexcept
{
    const auto* descriptor = tensorDTypeDescriptor(dtype);
    return descriptor == nullptr ? std::string_view{"unknown"} : descriptor->name;
}

/** @brief Returns the canonical lowercase name for T. */
template <TensorDTypeElement T>
[[nodiscard]] consteval std::string_view tensorDTypeName() noexcept
{
    return tensorDTypeName(tensorDTypeOf<T>());
}

/** @brief Returns the logical bit width, or zero for an invalid enum value. */
[[nodiscard]] constexpr std::size_t tensorDTypeBitWidth(TensorDType dtype) noexcept
{
    const auto* descriptor = tensorDTypeDescriptor(dtype);
    return descriptor == nullptr ? 0 : descriptor->bitWidth;
}

} // namespace fat_p
