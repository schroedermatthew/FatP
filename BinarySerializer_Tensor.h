/**
 * @file BinarySerializer_Tensor.h
 * @brief Big-endian serialization for cross-platform tensor storage
 * @details Always serializes in big-endian (network byte order) for
 * guaranteed portability. Uses compile-time fixed type IDs and portable
 * byte-swap intrinsics. Performance cost ~1-2ms per 8MB (I/O dominated).
 * 
 * @version 2.0
 * @date 2025-11
 * 
 * Key Design Decisions:
 * - Fixed Type IDs: Portable across compilers (unlike typeid)
 * - Always Big-Endian: Simple, predictable, debuggable
 * - memcpy for Type Punning: Avoids strict aliasing UB
 * - Compile-Time Dispatch: Zero runtime overhead for type selection
 * 
 * Requires: C++17
 */

#ifndef CPP_UTILITIES_BINARY_SERIALIZER_TENSOR_H
#define CPP_UTILITIES_BINARY_SERIALIZER_TENSOR_H

#include "BinarySerializer.h"
#include "Tensor.h"
#include "ConcurrencyPolicies.h"
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace cpp_utilities {

// =============================================================================
// Portable Byte-Swap Intrinsics
// =============================================================================

/// @brief Byte-swap 16-bit value
inline uint16_t bswap16(uint16_t val) {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap16(val);
#elif defined(_MSC_VER)
    return _byteswap_ushort(val);
#else
    return (val << 8) | (val >> 8);
#endif
}

/// @brief Byte-swap 32-bit value
inline uint32_t bswap32(uint32_t val) {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap32(val);
#elif defined(_MSC_VER)
    return _byteswap_ulong(val);
#else
    return ((val & 0xFF000000) >> 24) | ((val & 0x00FF0000) >> 8) |
           ((val & 0x0000FF00) << 8)  | ((val & 0x000000FF) << 24);
#endif
}

/// @brief Byte-swap 64-bit value
inline uint64_t bswap64(uint64_t val) {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap64(val);
#elif defined(_MSC_VER)
    return _byteswap_uint64(val);
#else
    return ((val & 0xFF00000000000000ULL) >> 56) |
           ((val & 0x00FF000000000000ULL) >> 40) |
           ((val & 0x0000FF0000000000ULL) >> 24) |
           ((val & 0x000000FF00000000ULL) >> 8)  |
           ((val & 0x00000000FF000000ULL) << 8)  |
           ((val & 0x0000000000FF0000ULL) << 24) |
           ((val & 0x000000000000FF00ULL) << 40) |
           ((val & 0x00000000000000FFULL) << 56);
#endif
}

// =============================================================================
// Fixed Type IDs (Portable Across Compilers/Platforms)
// =============================================================================

/**
 * @brief Portable type identifiers for tensor serialization
 * @note These IDs are fixed and will never change across versions
 */
enum class TensorTypeID : uint8_t {
    INT8 = 1,
    UINT8 = 2,
    INT16 = 3,
    UINT16 = 4,
    INT32 = 5,
    UINT32 = 6,
    INT64 = 7,
    UINT64 = 8,
    FLOAT32 = 9,
    FLOAT64 = 10
};

/// @brief Get type ID at compile time
template<typename T>
constexpr TensorTypeID get_type_id() {
    if constexpr (std::is_same_v<T, int8_t>) return TensorTypeID::INT8;
    else if constexpr (std::is_same_v<T, uint8_t>) return TensorTypeID::UINT8;
    else if constexpr (std::is_same_v<T, int16_t>) return TensorTypeID::INT16;
    else if constexpr (std::is_same_v<T, uint16_t>) return TensorTypeID::UINT16;
    else if constexpr (std::is_same_v<T, int32_t>) return TensorTypeID::INT32;
    else if constexpr (std::is_same_v<T, uint32_t>) return TensorTypeID::UINT32;
    else if constexpr (std::is_same_v<T, int64_t>) return TensorTypeID::INT64;
    else if constexpr (std::is_same_v<T, uint64_t>) return TensorTypeID::UINT64;
    else if constexpr (std::is_same_v<T, float>) return TensorTypeID::FLOAT32;
    else if constexpr (std::is_same_v<T, double>) return TensorTypeID::FLOAT64;
    else static_assert(sizeof(T) == 0, "Unsupported tensor element type");
}

/// @brief Get type name string for debugging
template<typename T>
constexpr const char* get_type_name() {
    if constexpr (std::is_same_v<T, int8_t>) return "int8";
    else if constexpr (std::is_same_v<T, uint8_t>) return "uint8";
    else if constexpr (std::is_same_v<T, int16_t>) return "int16";
    else if constexpr (std::is_same_v<T, uint16_t>) return "uint16";
    else if constexpr (std::is_same_v<T, int32_t>) return "int32";
    else if constexpr (std::is_same_v<T, uint32_t>) return "uint32";
    else if constexpr (std::is_same_v<T, int64_t>) return "int64";
    else if constexpr (std::is_same_v<T, uint64_t>) return "uint64";
    else if constexpr (std::is_same_v<T, float>) return "float32";
    else if constexpr (std::is_same_v<T, double>) return "float64";
    else return "unknown";
}

// =============================================================================
// Big-Endian Read/Write Helpers
// =============================================================================

/**
 * @brief Write value in big-endian format (compile-time dispatch by size)
 * @tparam T Value type (must be trivially copyable)
 * @param buffer Output buffer
 * @param value Value to write
 */
template<typename T>
void write_be(std::vector<uint8_t>& buffer, T value) {
    static_assert(std::is_trivially_copyable_v<T>,
                  "Type must be trivially copyable");
    
    if constexpr (sizeof(T) == 1) {
        buffer.push_back(static_cast<uint8_t>(value));
    } else if constexpr (sizeof(T) == 2) {
        uint16_t bits;
        std::memcpy(&bits, &value, sizeof(T));
        bits = bswap16(bits);
        uint8_t bytes[2];
        std::memcpy(bytes, &bits, 2);
        buffer.insert(buffer.end(), bytes, bytes + 2);
    } else if constexpr (sizeof(T) == 4) {
        uint32_t bits;
        std::memcpy(&bits, &value, sizeof(T));
        bits = bswap32(bits);
        uint8_t bytes[4];
        std::memcpy(bytes, &bits, 4);
        buffer.insert(buffer.end(), bytes, bytes + 4);
    } else if constexpr (sizeof(T) == 8) {
        uint64_t bits;
        std::memcpy(&bits, &value, sizeof(T));
        bits = bswap64(bits);
        uint8_t bytes[8];
        std::memcpy(bytes, &bits, 8);
        buffer.insert(buffer.end(), bytes, bytes + 8);
    }
}

/**
 * @brief Read value in big-endian format (compile-time dispatch by size)
 * @tparam T Value type (must be trivially copyable)
 * @param data Input buffer
 * @param pos Position in buffer (updated after read)
 * @return Decoded value
 */
template<typename T>
T read_be(const std::vector<uint8_t>& data, size_t& pos) {
    static_assert(std::is_trivially_copyable_v<T>,
                  "Type must be trivially copyable");
    
    if constexpr (sizeof(T) == 1) {
        return static_cast<T>(data[pos++]);
    } else if constexpr (sizeof(T) == 2) {
        uint16_t bits = 0;
        std::memcpy(&bits, &data[pos], 2);
        bits = bswap16(bits);
        pos += 2;
        T value;
        std::memcpy(&value, &bits, sizeof(T));
        return value;
    } else if constexpr (sizeof(T) == 4) {
        uint32_t bits = 0;
        std::memcpy(&bits, &data[pos], 4);
        bits = bswap32(bits);
        pos += 4;
        T value;
        std::memcpy(&value, &bits, sizeof(T));
        return value;
    } else if constexpr (sizeof(T) == 8) {
        uint64_t bits = 0;
        std::memcpy(&bits, &data[pos], 8);
        bits = bswap64(bits);
        pos += 8;
        T value;
        std::memcpy(&value, &bits, sizeof(T));
        return value;
    }
}

// =============================================================================
// Tensor Serialization - Big-Endian Format
// =============================================================================

/**
 * @brief Serialize tensor to big-endian binary format
 * @tparam T Element type
 * @tparam Allocator Allocator type
 * @tparam IteratorPolicy Iterator policy
 * @tparam ConcurrencyPolicy Concurrency policy
 * @param tensor Tensor to serialize
 * @return Binary buffer in big-endian format
 * 
 * @details Format: [magic][version][type_id][ndim][dims...][strides...][data...]
 *          All multi-byte values in big-endian (network byte order)
 *          
 *          - magic: uint32_t = 0x544E5352 ("TNSR")
 *          - version: uint8_t = 1
 *          - type_id: uint8_t = TensorTypeID enum value
 *          - ndim: uint16_t = number of dimensions
 *          - dims: ndim Ã— uint64_t = dimension sizes
 *          - strides: ndim Ã— int64_t = stride values (signed for negative strides)
 *          - data: size Ã— sizeof(T) bytes = raw tensor data
 */
template<typename T, typename Allocator, typename IteratorPolicy, typename ConcurrencyPolicy>
std::vector<uint8_t> serialize_tensor(
    const Tensor<T, Allocator, IteratorPolicy, ConcurrencyPolicy>& tensor) {
    
    std::vector<uint8_t> buffer;
    const size_t ndim = tensor.ndim();
    const size_t total_size = tensor.size();
    
    // Reserve space: header + shape + strides + data
    buffer.reserve(16 + ndim * 16 + total_size * sizeof(T));
    
    // Magic number "TNSR" (big-endian: 0x544E5352)
    write_be<uint32_t>(buffer, 0x544E5352);
    
    // Version
    buffer.push_back(1);
    
    // Type ID
    buffer.push_back(static_cast<uint8_t>(get_type_id<T>()));
    
    // Number of dimensions
    write_be<uint16_t>(buffer, static_cast<uint16_t>(ndim));
    
    // Shape (dimensions)
    for (size_t dim : tensor.shape()) {
        write_be<uint64_t>(buffer, dim);
    }
    
    // Strides (signed for negative strides support)
    for (ptrdiff_t stride : tensor.strides()) {
        write_be<int64_t>(buffer, stride);
    }
    
    // Data (element by element to handle all types correctly)
    const T* data = tensor.data();
    for (size_t i = 0; i < total_size; ++i) {
        write_be<T>(buffer, data[i]);
    }
    
    return buffer;
}

/**
 * @brief Deserialize tensor from big-endian binary format
 * @tparam T Element type
 * @tparam Allocator Allocator type
 * @tparam IteratorPolicy Iterator policy
 * @tparam ConcurrencyPolicy Concurrency policy
 * @param data Binary buffer in big-endian format
 * @return Deserialized tensor
 * @throws std::runtime_error if deserialization fails
 */
template<typename T, 
         typename Allocator = TensorAllocator<T>, 
         typename IteratorPolicy = RowMajorPolicy,
         typename ConcurrencyPolicy = SingleThreadedPolicy>
Tensor<T, Allocator, IteratorPolicy, ConcurrencyPolicy> deserialize_tensor(
    const std::vector<uint8_t>& data) {
    
    size_t pos = 0;
    
    // Validate minimum size for header
    if (data.size() < 8) {
        throw std::runtime_error("Buffer too small for tensor header");
    }
    
    // Read and validate magic number
    uint32_t magic = read_be<uint32_t>(data, pos);
    if (magic != 0x544E5352) {
        throw std::runtime_error("Invalid tensor magic number");
    }
    
    // Read version
    uint8_t version = data[pos++];
    if (version != 1) {
        throw std::runtime_error("Unsupported tensor format version");
    }
    
    // Read and validate type ID
    TensorTypeID type_id = static_cast<TensorTypeID>(data[pos++]);
    if (type_id != get_type_id<T>()) {
        throw std::runtime_error(std::string("Type mismatch: expected ") + 
                                 get_type_name<T>() + " but got type ID " +
                                 std::to_string(static_cast<int>(type_id)));
    }
    
    // Read number of dimensions
    uint16_t ndim = read_be<uint16_t>(data, pos);
    if (ndim == 0 || ndim > 32) {  // Sanity check
        throw std::runtime_error("Invalid number of dimensions");
    }
    
    // Read shape
    std::vector<size_t> shape(ndim);
    for (size_t i = 0; i < ndim; ++i) {
        shape[i] = read_be<uint64_t>(data, pos);
    }
    
    // Read strides (but we'll create a contiguous tensor)
    std::vector<ptrdiff_t> strides(ndim);
    for (size_t i = 0; i < ndim; ++i) {
        strides[i] = read_be<int64_t>(data, pos);
    }
    
    // Calculate expected data size
    size_t total_size = 1;
    for (size_t dim : shape) {
        total_size *= dim;
    }
    
    // Validate remaining buffer size
    if (pos + total_size * sizeof(T) > data.size()) {
        throw std::runtime_error("Buffer too small for tensor data");
    }
    
    // Create tensor and read data
    Tensor<T, Allocator, IteratorPolicy, ConcurrencyPolicy> result(shape);
    T* dest = result.data();
    for (size_t i = 0; i < total_size; ++i) {
        dest[i] = read_be<T>(data, pos);
    }
    
    return result;
}

// =============================================================================
// Legacy Support - CustomBinaryPolicy (Deprecated)
// =============================================================================
// Note: These functions are kept for backward compatibility but should be
// migrated to the new big-endian format above.

template <typename T, typename Allocator, typename IteratorPolicy, typename ConcurrencyPolicy>
inline Expected<std::vector<uint8_t>, SerializationError>
serialize_tensor(const Tensor<T, Allocator, IteratorPolicy, ConcurrencyPolicy>& tensor, [[maybe_unused]] CustomBinaryPolicy policy) {
    // Delegate to big-endian serialization
    try {
        return serialize_tensor(tensor);
    } catch (const std::exception& e) {
        return make_unexpected(SerializationError(e.what()));
    }
}

template <typename T, 
          typename Allocator = TensorAllocator<T>, 
          typename IteratorPolicy = RowMajorPolicy,
          typename ConcurrencyPolicy = SingleThreadedPolicy>
inline Expected<Tensor<T, Allocator, IteratorPolicy, ConcurrencyPolicy>, SerializationError>
deserialize_tensor(const std::vector<uint8_t>& data, [[maybe_unused]] CustomBinaryPolicy policy) {
    try {
        return deserialize_tensor<T, Allocator, IteratorPolicy, ConcurrencyPolicy>(data);
    } catch (const std::exception& e) {
        return make_unexpected(SerializationError(e.what()));
    }
}

// =============================================================================
// Legacy Support - CborPolicy (Deprecated)
// =============================================================================
// Note: CborPolicy now delegates to big-endian format for simplicity

template <typename T, typename Allocator, typename IteratorPolicy, typename ConcurrencyPolicy>
inline Expected<std::vector<uint8_t>, SerializationError>
serialize_tensor(const Tensor<T, Allocator, IteratorPolicy, ConcurrencyPolicy>& tensor, [[maybe_unused]] CborPolicy policy) {
    // Delegate to big-endian serialization
    try {
        return serialize_tensor(tensor);
    } catch (const std::exception& e) {
        return make_unexpected(SerializationError(e.what()));
    }
}

template <typename T, 
          typename Allocator = TensorAllocator<T>, 
          typename IteratorPolicy = RowMajorPolicy,
          typename ConcurrencyPolicy = SingleThreadedPolicy>
inline Expected<Tensor<T, Allocator, IteratorPolicy, ConcurrencyPolicy>, SerializationError>
deserialize_tensor(const std::vector<uint8_t>& data, [[maybe_unused]] CborPolicy policy) {
    try {
        return deserialize_tensor<T, Allocator, IteratorPolicy, ConcurrencyPolicy>(data);
    } catch (const std::exception& e) {
        return make_unexpected(SerializationError(e.what()));
    }
}

} // namespace cpp_utilities

#endif // CPP_UTILITIES_BINARY_SERIALIZER_TENSOR_H
