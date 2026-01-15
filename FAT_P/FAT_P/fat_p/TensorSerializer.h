/**
 * @file TensorSerializer.h
 * @brief Tensor serialization and deserialization
 *
 * @layer Domain
 *
 * @details
 * Big-endian serialization for cross-platform tensor storage
 * Always serializes in big-endian (network byte order) for guaranteed portability.
 * Uses compile-time fixed type IDs and portable byte-swap intrinsics.
 * Requires: C++17, Tensor.h, Expected.h
 */
#pragma once
/*
FATP_META:
  meta_version: 1
  component: TensorSerializer
  file_role: public_header
  path: fat_p/TensorSerializer.h
  namespace: fat_p
  layer: Domain
  summary: "Public header for TensorSerializer."
  api_stability: in_work
  related:
    docs_search: "TensorSerializer"
    tests:
      - tests/test_TensorSerializer.cpp
  hygiene:
    pragma_once: true
    include_guard: true
    defines_total: 1
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/

#include "Expected.h"
#include "Tensor.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

namespace fat_p
{

// ============================================================================
// Error Type
// ============================================================================

struct TensorSerializationError
{
    std::string message;

    TensorSerializationError() = default;

    explicit TensorSerializationError(std::string msg)
        : message(std::move(msg))
    {
    }
};

template <typename T>
using TensorSerializationResult = Expected<T, TensorSerializationError>;

// ============================================================================
// Portable Byte-Swap Intrinsics
// ============================================================================

inline std::uint16_t bswap16(std::uint16_t val)
{
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap16(val);
#elif defined(_MSC_VER)
    return _byteswap_ushort(val);
#else
    return static_cast<std::uint16_t>((val << 8U) | (val >> 8U));
#endif
}

inline std::uint32_t bswap32(std::uint32_t val)
{
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap32(val);
#elif defined(_MSC_VER)
    return _byteswap_ulong(val);
#else
    return ((val & 0xFF000000U) >> 24U) | ((val & 0x00FF0000U) >> 8U) | ((val & 0x0000FF00U) << 8U) |
           ((val & 0x000000FFU) << 24U);
#endif
}

inline std::uint64_t bswap64(std::uint64_t val)
{
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap64(val);
#elif defined(_MSC_VER)
    return _byteswap_uint64(val);
#else
    return ((val & 0xFF00000000000000ULL) >> 56U) | ((val & 0x00FF000000000000ULL) >> 40U) |
           ((val & 0x0000FF0000000000ULL) >> 24U) | ((val & 0x000000FF00000000ULL) >> 8U) |
           ((val & 0x00000000FF000000ULL) << 8U) | ((val & 0x0000000000FF0000ULL) << 24U) |
           ((val & 0x000000000000FF00ULL) << 40U) | ((val & 0x00000000000000FFULL) << 56U);
#endif
}

// ============================================================================
// Fixed Type IDs (Portable Across Compilers/Platforms)
// ============================================================================

enum class TensorTypeID : std::uint8_t
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

template <typename T>
constexpr TensorTypeID get_tensor_type_id()
{
    if constexpr (std::is_same_v<T, std::int8_t>)
    {
        return TensorTypeID::Int8;
    }
    else if constexpr (std::is_same_v<T, std::uint8_t>)
    {
        return TensorTypeID::Uint8;
    }
    else if constexpr (std::is_same_v<T, std::int16_t>)
    {
        return TensorTypeID::Int16;
    }
    else if constexpr (std::is_same_v<T, std::uint16_t>)
    {
        return TensorTypeID::Uint16;
    }
    else if constexpr (std::is_same_v<T, std::int32_t>)
    {
        return TensorTypeID::Int32;
    }
    else if constexpr (std::is_same_v<T, std::uint32_t>)
    {
        return TensorTypeID::Uint32;
    }
    else if constexpr (std::is_same_v<T, std::int64_t>)
    {
        return TensorTypeID::Int64;
    }
    else if constexpr (std::is_same_v<T, std::uint64_t>)
    {
        return TensorTypeID::Uint64;
    }
    else if constexpr (std::is_same_v<T, float>)
    {
        return TensorTypeID::Float32;
    }
    else if constexpr (std::is_same_v<T, double>)
    {
        return TensorTypeID::Float64;
    }
    else
    {
        static_assert(sizeof(T) == 0, "Unsupported tensor element type");
    }
}

template <typename T>
constexpr const char* get_tensor_type_name()
{
    if constexpr (std::is_same_v<T, std::int8_t>)
    {
        return "int8";
    }
    else if constexpr (std::is_same_v<T, std::uint8_t>)
    {
        return "uint8";
    }
    else if constexpr (std::is_same_v<T, std::int16_t>)
    {
        return "int16";
    }
    else if constexpr (std::is_same_v<T, std::uint16_t>)
    {
        return "uint16";
    }
    else if constexpr (std::is_same_v<T, std::int32_t>)
    {
        return "int32";
    }
    else if constexpr (std::is_same_v<T, std::uint32_t>)
    {
        return "uint32";
    }
    else if constexpr (std::is_same_v<T, std::int64_t>)
    {
        return "int64";
    }
    else if constexpr (std::is_same_v<T, std::uint64_t>)
    {
        return "uint64";
    }
    else if constexpr (std::is_same_v<T, float>)
    {
        return "float32";
    }
    else if constexpr (std::is_same_v<T, double>)
    {
        return "float64";
    }
    else
    {
        return "unknown";
    }
}

// ============================================================================
// Big-Endian Read/Write Helpers
// ============================================================================

namespace detail
{

template <typename T>
void write_be(std::vector<std::uint8_t>& buffer, T value)
{
    static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable");

    if constexpr (sizeof(T) == 1)
    {
        buffer.push_back(static_cast<std::uint8_t>(value));
    }
    else if constexpr (sizeof(T) == 2)
    {
        std::uint16_t bits = 0;
        std::memcpy(&bits, &value, sizeof(T));
        bits = bswap16(bits);
        std::uint8_t bytes[2];
        std::memcpy(bytes, &bits, 2);
        buffer.insert(buffer.end(), bytes, bytes + 2);
    }
    else if constexpr (sizeof(T) == 4)
    {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(T));
        bits = bswap32(bits);
        std::uint8_t bytes[4];
        std::memcpy(bytes, &bits, 4);
        buffer.insert(buffer.end(), bytes, bytes + 4);
    }
    else if constexpr (sizeof(T) == 8)
    {
        std::uint64_t bits = 0;
        std::memcpy(&bits, &value, sizeof(T));
        bits = bswap64(bits);
        std::uint8_t bytes[8];
        std::memcpy(bytes, &bits, 8);
        buffer.insert(buffer.end(), bytes, bytes + 8);
    }
}

template <typename T>
T read_be(const std::vector<std::uint8_t>& data, std::size_t& pos)
{
    static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable");

    if constexpr (sizeof(T) == 1)
    {
        return static_cast<T>(data[pos++]);
    }
    else if constexpr (sizeof(T) == 2)
    {
        std::uint16_t bits = 0;
        std::memcpy(&bits, &data[pos], 2);
        bits = bswap16(bits);
        pos += 2;
        T value;
        std::memcpy(&value, &bits, sizeof(T));
        return value;
    }
    else if constexpr (sizeof(T) == 4)
    {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &data[pos], 4);
        bits = bswap32(bits);
        pos += 4;
        T value;
        std::memcpy(&value, &bits, sizeof(T));
        return value;
    }
    else if constexpr (sizeof(T) == 8)
    {
        std::uint64_t bits = 0;
        std::memcpy(&bits, &data[pos], 8);
        bits = bswap64(bits);
        pos += 8;
        T value;
        std::memcpy(&value, &bits, sizeof(T));
        return value;
    }
}

} // namespace detail

// ============================================================================
// Tensor Serialization Format
// ============================================================================
//
// Format: [magic][version][type_id][ndim][dims...][strides...][data...]
//
// All multi-byte values in big-endian (network byte order):
//   - magic: uint32_t = 0x544E5352 ("TNSR")
//   - version: uint8_t = 1
//   - type_id: uint8_t = TensorTypeID enum value
//   - ndim: uint16_t = number of dimensions
//   - dims: ndim × uint64_t = dimension sizes
//   - strides: ndim × int64_t = stride values (signed for negative strides)
//   - data: size × sizeof(T) bytes = tensor data in big-endian
//
// ============================================================================

constexpr std::uint32_t TENSOR_MAGIC = 0x544E5352U; // "TNSR"
constexpr std::uint8_t TENSOR_FORMAT_VERSION = 1U;

/**
 * @brief Serialize tensor to big-endian binary format
 * @tparam T Element type
 * @tparam Allocator Allocator type
 * @tparam IteratorPolicy Iterator policy
 * @tparam ConcurrencyPolicy Concurrency policy
 * @param tensor Tensor to serialize
 * @return Binary buffer in big-endian format, or error
 */
template <typename T, typename Allocator, typename IteratorPolicy, typename ConcurrencyPolicy>
TensorSerializationResult<std::vector<std::uint8_t>>
serialize_tensor(const Tensor<T, Allocator, IteratorPolicy, ConcurrencyPolicy>& tensor)
{
    try
    {
        std::vector<std::uint8_t> buffer;
        const std::size_t ndim = tensor.ndim();
        const std::size_t total_size = tensor.size();

        buffer.reserve(16 + ndim * 16 + total_size * sizeof(T));

        detail::write_be<std::uint32_t>(buffer, TENSOR_MAGIC);

        buffer.push_back(TENSOR_FORMAT_VERSION);

        buffer.push_back(static_cast<std::uint8_t>(get_tensor_type_id<T>()));

        detail::write_be<std::uint16_t>(buffer, static_cast<std::uint16_t>(ndim));

        for (std::size_t dim : tensor.shape())
        {
            detail::write_be<std::uint64_t>(buffer, dim);
        }

        for (std::ptrdiff_t stride : tensor.strides())
        {
            detail::write_be<std::int64_t>(buffer, stride);
        }

        const T* data = tensor.data();
        for (std::size_t i = 0; i < total_size; ++i)
        {
            detail::write_be<T>(buffer, data[i]);
        }

        return buffer;
    }
    catch (const std::exception& e)
    {
        return make_unexpected(TensorSerializationError(e.what()));
    }
}

/**
 * @brief Deserialize tensor from big-endian binary format
 * @tparam T Element type
 * @tparam Allocator Allocator type (defaults to TensorAllocator<T>)
 * @tparam IteratorPolicy Iterator policy (defaults to RowMajorPolicy)
 * @tparam ConcurrencyPolicy Concurrency policy (defaults to SingleThreadedPolicy)
 * @param data Binary buffer in big-endian format
 * @return Deserialized tensor, or error
 */
template <typename T,
          typename Allocator = TensorAllocator<T>,
          typename IteratorPolicy = RowMajorPolicy,
          typename ConcurrencyPolicy = SingleThreadedPolicy>
TensorSerializationResult<Tensor<T, Allocator, IteratorPolicy, ConcurrencyPolicy>>
deserialize_tensor(const std::vector<std::uint8_t>& data)
{
    try
    {
        std::size_t pos = 0;

        if (data.size() < 8)
        {
            return make_unexpected(TensorSerializationError("Buffer too small for tensor header"));
        }

        const std::uint32_t magic = detail::read_be<std::uint32_t>(data, pos);
        if (magic != TENSOR_MAGIC)
        {
            return make_unexpected(TensorSerializationError("Invalid tensor magic number"));
        }

        const std::uint8_t version = data[pos++];
        if (version != TENSOR_FORMAT_VERSION)
        {
            return make_unexpected(TensorSerializationError("Unsupported tensor format version"));
        }

        const auto type_id = static_cast<TensorTypeID>(data[pos++]);
        if (type_id != get_tensor_type_id<T>())
        {
            return make_unexpected(TensorSerializationError(std::string("Type mismatch: expected ") +
                                                            get_tensor_type_name<T>() + " but got type ID " +
                                                            std::to_string(static_cast<int>(type_id))));
        }

        const std::uint16_t ndim = detail::read_be<std::uint16_t>(data, pos);
        if (ndim == 0 || ndim > 32)
        {
            return make_unexpected(TensorSerializationError("Invalid number of dimensions"));
        }

        std::vector<std::size_t> shape(ndim);
        for (std::size_t i = 0; i < ndim; ++i)
        {
            shape[i] = detail::read_be<std::uint64_t>(data, pos);
        }

        std::vector<std::ptrdiff_t> strides(ndim);
        for (std::size_t i = 0; i < ndim; ++i)
        {
            strides[i] = detail::read_be<std::int64_t>(data, pos);
        }

        std::size_t total_size = 1;
        for (std::size_t dim : shape)
        {
            total_size *= dim;
        }

        if (pos + total_size * sizeof(T) > data.size())
        {
            return make_unexpected(TensorSerializationError("Buffer too small for tensor data"));
        }

        Tensor<T, Allocator, IteratorPolicy, ConcurrencyPolicy> result(shape);
        T* dest = result.data();
        for (std::size_t i = 0; i < total_size; ++i)
        {
            dest[i] = detail::read_be<T>(data, pos);
        }

        return result;
    }
    catch (const std::exception& e)
    {
        return make_unexpected(TensorSerializationError(e.what()));
    }
}

} // namespace fat_p
