#pragma once

/*
FATP_META:
  meta_version: 1
  component: TensorSerializer
  file_role: internal_header
  path: include/fat_p/tensor/TensorSerializer.h
  namespace: fat_p
  layer: Domain
  summary: "Internal implementation for portable Tensor binary serialization."
  api_stability: in_work
  related:
    docs_search: "TensorSerializer"
    tests:
      - components/Tensor/tests/test_TensorSerializer.cpp
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/

/**
 * @file TensorSerializer.h
 * @brief Tensor serialization and deserialization
 *
 * @details
 * Big-endian serialization for cross-platform tensor storage.
 * Supported targets must use 8-bit bytes, pure little- or big-endian storage,
 * two's-complement signed integers, and IEEE-754 binary32/binary64 floating
 * point. These requirements are enforced at compile time.
 * Requires: C++20, Tensor.h, Expected.h
 */

#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>
#if defined(_MSC_VER)
#include <intrin.h>
#endif

#include "Expected.h"
#include "Tensor.h"

namespace fat_p
{

// ============================================================================
// Error Type
// ============================================================================

enum class TensorSerializationErrorCode
{
    InvalidData,
    UnsupportedVersion,
    TypeMismatch,
    ResourceLimit,
    AllocationFailure
};

struct TensorSerializationError
{
    TensorSerializationErrorCode code = TensorSerializationErrorCode::InvalidData;
    std::string message;

    TensorSerializationError() = default;

    explicit TensorSerializationError(std::string msg)
        : message(std::move(msg))
    {
    }

    TensorSerializationError(TensorSerializationErrorCode error_code, std::string msg)
        : code(error_code)
        , message(std::move(msg))
    {
    }
};

template <typename T>
using TensorSerializationResult = Expected<T, TensorSerializationError>;

// ============================================================================
// Portable Byte-Swap Intrinsics
// ============================================================================

namespace detail
{

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

} // namespace detail

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
    static_assert(std::endian::native == std::endian::little || std::endian::native == std::endian::big,
                  "Tensor serialization requires a pure little- or big-endian target");
    static_assert(std::numeric_limits<unsigned char>::digits == 8, "Tensor serialization requires eight-bit bytes");
    if constexpr (std::is_integral_v<T> && std::is_signed_v<T>)
    {
        static_assert(static_cast<T>(~T{0}) == T{-1}, "Tensor serialization requires two's-complement signed integers");
    }
    if constexpr (std::is_same_v<T, float>)
    {
        static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559,
                      "Tensor Float32 serialization requires IEEE-754 binary32 float");
    }
    if constexpr (std::is_same_v<T, double>)
    {
        static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559,
                      "Tensor Float64 serialization requires IEEE-754 binary64 double");
    }

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
    static_assert(std::endian::native == std::endian::little || std::endian::native == std::endian::big,
                  "Big-endian conversion requires a pure little- or big-endian target");
    static_assert(sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8,
                  "Tensor serialization supports 1, 2, 4, or 8-byte values");

    if constexpr (sizeof(T) == 1)
    {
        buffer.push_back(static_cast<std::uint8_t>(value));
    }
    else if constexpr (sizeof(T) == 2)
    {
        std::uint16_t bits = 0;
        std::memcpy(&bits, &value, sizeof(T));
        if constexpr (std::endian::native == std::endian::little)
        {
            bits = bswap16(bits);
        }
        std::uint8_t bytes[2];
        std::memcpy(bytes, &bits, 2);
        buffer.insert(buffer.end(), bytes, bytes + 2);
    }
    else if constexpr (sizeof(T) == 4)
    {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(T));
        if constexpr (std::endian::native == std::endian::little)
        {
            bits = bswap32(bits);
        }
        std::uint8_t bytes[4];
        std::memcpy(bytes, &bits, 4);
        buffer.insert(buffer.end(), bytes, bytes + 4);
    }
    else if constexpr (sizeof(T) == 8)
    {
        std::uint64_t bits = 0;
        std::memcpy(&bits, &value, sizeof(T));
        if constexpr (std::endian::native == std::endian::little)
        {
            bits = bswap64(bits);
        }
        std::uint8_t bytes[8];
        std::memcpy(bytes, &bits, 8);
        buffer.insert(buffer.end(), bytes, bytes + 8);
    }
}

template <typename T>
T read_be(const std::vector<std::uint8_t>& data, std::size_t& pos)
{
    static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable");
    static_assert(std::endian::native == std::endian::little || std::endian::native == std::endian::big,
                  "Big-endian conversion requires a pure little- or big-endian target");
    static_assert(sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8,
                  "Tensor serialization supports 1, 2, 4, or 8-byte values");

    if (pos > data.size() || data.size() - pos < sizeof(T))
    {
        throw std::out_of_range("Truncated tensor field");
    }

    if constexpr (sizeof(T) == 1)
    {
        return static_cast<T>(data[pos++]);
    }
    else if constexpr (sizeof(T) == 2)
    {
        std::uint16_t bits = 0;
        std::memcpy(&bits, &data[pos], 2);
        if constexpr (std::endian::native == std::endian::little)
        {
            bits = bswap16(bits);
        }
        pos += 2;
        T value;
        std::memcpy(&value, &bits, sizeof(T));
        return value;
    }
    else if constexpr (sizeof(T) == 4)
    {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &data[pos], 4);
        if constexpr (std::endian::native == std::endian::little)
        {
            bits = bswap32(bits);
        }
        pos += 4;
        T value;
        std::memcpy(&value, &bits, sizeof(T));
        return value;
    }
    else if constexpr (sizeof(T) == 8)
    {
        std::uint64_t bits = 0;
        std::memcpy(&bits, &data[pos], 8);
        if constexpr (std::endian::native == std::endian::little)
        {
            bits = bswap64(bits);
        }
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
//   - version: uint8_t = 2
//   - type_id: uint8_t = TensorTypeID enum value
//   - ndim: uint16_t = number of dimensions
//   - dims: ndim Ã— uint64_t = dimension sizes
//   - strides: ndim Ã— int64_t = canonical row-major strides
//   - data: size Ã— sizeof(T) bytes = logical row-major values in big-endian
//
// A rank-zero tensor has ndim == 0 and exactly one payload element. An empty
// tensor has at least one zero dimension and no payload elements.
//
// ============================================================================

constexpr std::uint32_t TENSOR_MAGIC = 0x544E5352U; // "TNSR"
constexpr std::uint8_t TENSOR_FORMAT_VERSION = 2U;
constexpr std::size_t TENSOR_MAX_DIMENSIONS = 32U;

/**
 * @brief Allocation limits applied before deserializing Tensor payload storage.
 * @details The defaults bound a single tensor payload to 64 MiB and its element
 * count to 64 million. Callers handling untrusted data should normally choose a
 * smaller application-specific budget.
 */
struct TensorDeserializationLimits
{
    std::size_t max_dimensions = TENSOR_MAX_DIMENSIONS;
    std::uint64_t max_extent = std::numeric_limits<std::uint64_t>::max();
    std::size_t max_elements = 64U * 1024U * 1024U;
    std::size_t max_payload_bytes = 64U * 1024U * 1024U;
};

/**
 * @brief Serialize tensor to big-endian binary format
 * @tparam R Readable owner or view type
 * @param tensor Readable Tensor mapping to serialize in logical order
 * @return Binary buffer in big-endian format, or error
 */
template <ReadableTensor R>
TensorSerializationResult<std::vector<std::uint8_t>>
serialize_tensor(const R& tensor)
{
    using T = typename R::value_type;
    try
    {
        std::vector<std::uint8_t> buffer;
        const std::size_t ndim = tensor.rank();
        const std::size_t total_size = tensor.size();

        if (ndim > TENSOR_MAX_DIMENSIONS)
        {
            throw std::invalid_argument("Tensor rank exceeds serialization format limit");
        }

        const std::size_t metadata_size = 8U + ndim * 16U;
        if (total_size > (std::numeric_limits<std::size_t>::max() - metadata_size) / sizeof(T))
        {
            throw std::overflow_error("Serialized tensor size overflows size_t");
        }
        buffer.reserve(metadata_size + total_size * sizeof(T));

        detail::write_be<std::uint32_t>(buffer, TENSOR_MAGIC);

        buffer.push_back(TENSOR_FORMAT_VERSION);

        buffer.push_back(static_cast<std::uint8_t>(get_tensor_type_id<T>()));

        detail::write_be<std::uint16_t>(buffer, static_cast<std::uint16_t>(ndim));

        for (std::size_t dim : tensor.extents())
        {
            if constexpr (sizeof(std::size_t) > sizeof(std::uint64_t))
            {
                if (dim > static_cast<std::size_t>(std::numeric_limits<std::uint64_t>::max()))
                {
                    throw std::overflow_error("Tensor dimension overflows uint64 wire extent");
                }
            }
            detail::write_be<std::uint64_t>(buffer, static_cast<std::uint64_t>(dim));
        }

        std::vector<std::int64_t> canonical_strides(ndim, 1);
        for (std::size_t axis = ndim; axis > 1; --axis)
        {
            const std::size_t dimension = tensor.extent(axis - 1);
            if (dimension == 0)
            {
                canonical_strides[axis - 2] = 0;
                continue;
            }
            if (dimension > static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max()) ||
                canonical_strides[axis - 1] >
                    std::numeric_limits<std::int64_t>::max() / static_cast<std::int64_t>(dimension))
            {
                throw std::overflow_error("Canonical tensor stride overflows int64_t");
            }
            canonical_strides[axis - 2] = canonical_strides[axis - 1] * static_cast<std::int64_t>(dimension);
        }
        for (std::int64_t stride : canonical_strides)
        {
            detail::write_be<std::int64_t>(buffer, stride);
        }

        for (std::size_t i = 0; i < total_size; ++i)
        {
            detail::write_be<T>(buffer, tensor[i]);
        }

        return buffer;
    }
    catch (const std::bad_alloc& e)
    {
        return make_unexpected(TensorSerializationError(TensorSerializationErrorCode::AllocationFailure, e.what()));
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
 * @param data Binary buffer in big-endian format
 * @param allocator Allocator instance used for Tensor element storage
 * @param limits Rank, extent, element-count, and payload-byte allocation limits
 * @return Deserialized tensor, or error
 */
template <typename T, typename Allocator = TensorAllocator<T>>
TensorSerializationResult<Tensor<T, Allocator>>
deserialize_tensor(const std::vector<std::uint8_t>& data,
                   const Allocator& allocator,
                   const TensorDeserializationLimits& limits = {})
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
            return make_unexpected(TensorSerializationError(TensorSerializationErrorCode::UnsupportedVersion,
                                                            "Unsupported tensor format version"));
        }

        const auto type_id = static_cast<TensorTypeID>(data[pos++]);
        if (type_id != get_tensor_type_id<T>())
        {
            return make_unexpected(TensorSerializationError(TensorSerializationErrorCode::TypeMismatch,
                                                            std::string("Type mismatch: expected ") +
                                                                get_tensor_type_name<T>() + " but got type ID " +
                                                                std::to_string(static_cast<int>(type_id))));
        }

        const std::uint16_t ndim = detail::read_be<std::uint16_t>(data, pos);
        if (ndim > TENSOR_MAX_DIMENSIONS)
        {
            return make_unexpected(TensorSerializationError("Invalid number of dimensions"));
        }
        if (ndim > limits.max_dimensions)
        {
            return make_unexpected(TensorSerializationError(TensorSerializationErrorCode::ResourceLimit,
                                                            "Tensor rank exceeds deserialization limit"));
        }

        std::vector<std::size_t> shape(ndim);
        for (std::size_t i = 0; i < ndim; ++i)
        {
            const std::uint64_t dimension = detail::read_be<std::uint64_t>(data, pos);
            if (dimension > limits.max_extent)
            {
                return make_unexpected(TensorSerializationError(TensorSerializationErrorCode::ResourceLimit,
                                                                "Tensor extent exceeds deserialization limit"));
            }
            if (dimension > std::numeric_limits<std::size_t>::max())
            {
                return make_unexpected(TensorSerializationError("Tensor dimension overflows size_t"));
            }
            shape[i] = static_cast<std::size_t>(dimension);
        }

        std::vector<std::int64_t> serialized_strides(ndim);
        for (std::size_t i = 0; i < ndim; ++i)
        {
            serialized_strides[i] = detail::read_be<std::int64_t>(data, pos);
        }

        std::vector<std::int64_t> canonical_strides(ndim, 1);
        for (std::size_t axis = ndim; axis > 1; --axis)
        {
            const std::size_t dimension = shape[axis - 1];
            if (dimension == 0)
            {
                canonical_strides[axis - 2] = 0;
                continue;
            }
            if (dimension > static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max()) ||
                canonical_strides[axis - 1] >
                    std::numeric_limits<std::int64_t>::max() / static_cast<std::int64_t>(dimension))
            {
                return make_unexpected(TensorSerializationError("Canonical tensor stride overflows int64_t"));
            }
            canonical_strides[axis - 2] = canonical_strides[axis - 1] * static_cast<std::int64_t>(dimension);
        }
        if (serialized_strides != canonical_strides)
        {
            return make_unexpected(TensorSerializationError("Serialized tensor strides are not canonical"));
        }

        std::size_t total_size = 1;
        const bool has_zero_extent = std::find(shape.begin(), shape.end(), std::size_t(0)) != shape.end();
        if (has_zero_extent)
        {
            total_size = 0;
        }
        for (std::size_t dim : shape)
        {
            if (has_zero_extent)
            {
                break;
            }
            if (dim != 0 && total_size > std::numeric_limits<std::size_t>::max() / dim)
            {
                return make_unexpected(TensorSerializationError("Tensor shape size overflows size_t"));
            }
            total_size *= dim;
        }
        if (total_size > static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max()))
        {
            return make_unexpected(TensorSerializationError("Tensor size exceeds ptrdiff_t layout limit"));
        }
        if (total_size > limits.max_elements)
        {
            return make_unexpected(TensorSerializationError(TensorSerializationErrorCode::ResourceLimit,
                                                            "Tensor element count exceeds deserialization limit"));
        }
        if (total_size > limits.max_payload_bytes / sizeof(T))
        {
            return make_unexpected(TensorSerializationError(TensorSerializationErrorCode::ResourceLimit,
                                                            "Tensor payload exceeds deserialization byte limit"));
        }

        if (pos > data.size() || total_size > (data.size() - pos) / sizeof(T))
        {
            return make_unexpected(TensorSerializationError("Buffer too small for tensor data"));
        }
        const std::size_t payload_size = total_size * sizeof(T);
        if (data.size() - pos != payload_size)
        {
            return make_unexpected(TensorSerializationError("Tensor buffer has trailing data"));
        }

        Tensor<T, Allocator> result(std::allocator_arg, allocator, shape);
        for (std::size_t i = 0; i < total_size; ++i)
        {
            result[i] = detail::read_be<T>(data, pos);
        }

        return result;
    }
    catch (const std::bad_alloc& e)
    {
        return make_unexpected(TensorSerializationError(TensorSerializationErrorCode::AllocationFailure, e.what()));
    }
    catch (const std::exception& e)
    {
        return make_unexpected(TensorSerializationError(e.what()));
    }
}

/**
 * @brief Deserialize using a default-constructed allocator instance.
 */
template <typename T, typename Allocator = TensorAllocator<T>>
TensorSerializationResult<Tensor<T, Allocator>>
deserialize_tensor(const std::vector<std::uint8_t>& data, const TensorDeserializationLimits& limits = {})
{
    try
    {
        return deserialize_tensor<T, Allocator>(data, Allocator{}, limits);
    }
    catch (const std::exception& e)
    {
        return make_unexpected(TensorSerializationError(TensorSerializationErrorCode::AllocationFailure, e.what()));
    }
}

} // namespace fat_p
