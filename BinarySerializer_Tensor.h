// BinarySerializer_Tensor.h
// Integration header for serializing Tensor objects with BinarySerializer
// Supports both CustomBinaryPolicy and CborPolicy
#ifndef CPP_UTILITIES_BINARY_SERIALIZER_TENSOR_H
#define CPP_UTILITIES_BINARY_SERIALIZER_TENSOR_H

#include "BinarySerializer.h"
#include "Tensor.h"
#include <typeinfo>

namespace cpp_utilities {

// Helper to get type name as string
template <typename T>
std::string get_type_name() {
    return typeid(T).name();
}

// =============================================================================
// Tensor Serialization - CustomBinaryPolicy
// =============================================================================

template <typename T, typename Allocator, typename IteratorPolicy>
inline Expected<std::vector<uint8_t>, SerializationError>
serialize_tensor(const Tensor<T, Allocator, IteratorPolicy>& tensor, CustomBinaryPolicy policy) {
    std::vector<uint8_t> buffer;
    
    // Serialize type name
    std::string type_str = get_type_name<T>();
    buffer.push_back(CustomBinaryPolicy::TYPE_STRING);
    CustomBinaryPolicy::encode_le(buffer, static_cast<uint64_t>(type_str.size()));
    CustomBinaryPolicy::copy_data(buffer, reinterpret_cast<const uint8_t*>(type_str.data()), type_str.size());
    
    // Serialize shape
    const auto& shape = tensor.shape();
    buffer.push_back(CustomBinaryPolicy::TYPE_ARRAY);
    CustomBinaryPolicy::encode_le(buffer, static_cast<uint64_t>(shape.size()));
    for (auto dim : shape) {
        buffer.push_back(CustomBinaryPolicy::TYPE_UINT64);
        CustomBinaryPolicy::encode_le(buffer, dim);
    }
    
    // Serialize strides
    const auto& strides = tensor.strides();
    buffer.push_back(CustomBinaryPolicy::TYPE_ARRAY);
    CustomBinaryPolicy::encode_le(buffer, static_cast<uint64_t>(strides.size()));
    for (auto stride : strides) {
        buffer.push_back(CustomBinaryPolicy::TYPE_INT64);
        CustomBinaryPolicy::encode_le(buffer, static_cast<int64_t>(stride));
    }
    
    // Serialize raw data
    size_t data_size = tensor.size() * sizeof(T);
    buffer.push_back(CustomBinaryPolicy::TYPE_STRING);  // Use STRING type for byte array
    CustomBinaryPolicy::encode_le(buffer, static_cast<uint64_t>(data_size));
    CustomBinaryPolicy::copy_data(buffer, reinterpret_cast<const uint8_t*>(tensor.data()), data_size);
    
    return buffer;
}

template <typename T, typename Allocator = TensorAllocator<T>, typename IteratorPolicy = RowMajorPolicy>
inline Expected<Tensor<T, Allocator, IteratorPolicy>, SerializationError>
deserialize_tensor(const std::vector<uint8_t>& data, CustomBinaryPolicy policy) {
    size_t pos = 0;
    
    // Deserialize type name (for verification)
    enforce(pos + 1 < data.size() && data[pos] == CustomBinaryPolicy::TYPE_STRING, "Expected type string");
    ++pos;
    size_t type_size = CustomBinaryPolicy::decode_le<uint64_t>(data, pos);
    std::string type_str(reinterpret_cast<const char*>(&data[pos]), type_size);
    pos += type_size;
    // Optional: verify type_str == get_type_name<T>()
    
    // Deserialize shape
    enforce(pos + 1 < data.size() && data[pos] == CustomBinaryPolicy::TYPE_ARRAY, "Expected shape array");
    ++pos;
    size_t shape_size = CustomBinaryPolicy::decode_le<uint64_t>(data, pos);
    std::vector<size_t> shape(shape_size);
    for (size_t& dim : shape) {
        enforce(pos + 1 < data.size() && data[pos] == CustomBinaryPolicy::TYPE_UINT64, "Expected uint64");
        ++pos;
        dim = CustomBinaryPolicy::decode_le<uint64_t>(data, pos);
    }
    
    // Deserialize strides
    enforce(pos + 1 < data.size() && data[pos] == CustomBinaryPolicy::TYPE_ARRAY, "Expected strides array");
    ++pos;
    size_t strides_size = CustomBinaryPolicy::decode_le<uint64_t>(data, pos);
    std::vector<ptrdiff_t> strides(strides_size);
    for (ptrdiff_t& stride : strides) {
        enforce(pos + 1 < data.size() && data[pos] == CustomBinaryPolicy::TYPE_INT64, "Expected int64");
        ++pos;
        stride = static_cast<ptrdiff_t>(CustomBinaryPolicy::decode_le<int64_t>(data, pos));
    }
    
    // Deserialize data
    enforce(pos + 1 < data.size() && data[pos] == CustomBinaryPolicy::TYPE_STRING, "Expected byte string");
    ++pos;
    size_t data_size = CustomBinaryPolicy::decode_le<uint64_t>(data, pos);
    
    // Calculate expected size
    size_t expected_size = 1;
    for (auto dim : shape) expected_size *= dim;
    enforce(data_size == expected_size * sizeof(T), "Data size mismatch");
    
    // Create tensor and copy data
    Tensor<T, Allocator, IteratorPolicy> result(shape);
    enforce(pos + data_size <= data.size(), "Data underflow");
    std::memcpy(result.data(), &data[pos], data_size);
    
    return result;
}

// =============================================================================
// Tensor Serialization - CborPolicy
// =============================================================================

template <typename T, typename Allocator, typename IteratorPolicy>
inline Expected<std::vector<uint8_t>, SerializationError>
serialize_tensor(const Tensor<T, Allocator, IteratorPolicy>& tensor, CborPolicy policy) {
    std::vector<uint8_t> buffer;
    
    // CBOR: Map { "type": str, "shape": array<uint>, "strides": array<int>, "data": bytestr }
    buffer.push_back(CborPolicy::MT_MAP | 4);  // 4 key-value pairs
    
    // Key "type"
    const char* type_key = "type";
    buffer.push_back(CborPolicy::MT_TEXTSTR | 4);
    CborPolicy::copy_data(buffer, reinterpret_cast<const uint8_t*>(type_key), 4);
    
    std::string type_str = get_type_name<T>();
    uint8_t ai = static_cast<uint8_t>(type_str.size() < 24 ? type_str.size() : 24);
    buffer.push_back(CborPolicy::MT_TEXTSTR | ai);
    if (type_str.size() >= 24) CborPolicy::encode_arg(buffer, type_str.size());
    CborPolicy::copy_data(buffer, reinterpret_cast<const uint8_t*>(type_str.data()), type_str.size());
    
    // Key "shape"
    const char* shape_key = "shape";
    buffer.push_back(CborPolicy::MT_TEXTSTR | 5);
    CborPolicy::copy_data(buffer, reinterpret_cast<const uint8_t*>(shape_key), 5);
    
    const auto& shape = tensor.shape();
    ai = static_cast<uint8_t>(shape.size() < 24 ? shape.size() : 24);
    buffer.push_back(CborPolicy::MT_ARRAY | ai);
    if (shape.size() >= 24) CborPolicy::encode_arg(buffer, shape.size());
    for (auto dim : shape) {
        ai = static_cast<uint8_t>(dim < 24 ? dim : (dim <= 0xFF ? 24 : (dim <= 0xFFFF ? 25 : (dim <= 0xFFFFFFFF ? 26 : 27))));
        buffer.push_back(CborPolicy::MT_UINT | ai);
        CborPolicy::encode_arg(buffer, dim);
    }
    
    // Key "strides"
    const char* strides_key = "strides";
    buffer.push_back(CborPolicy::MT_TEXTSTR | 7);
    CborPolicy::copy_data(buffer, reinterpret_cast<const uint8_t*>(strides_key), 7);
    
    const auto& strides = tensor.strides();
    ai = static_cast<uint8_t>(strides.size() < 24 ? strides.size() : 24);
    buffer.push_back(CborPolicy::MT_ARRAY | ai);
    if (strides.size() >= 24) CborPolicy::encode_arg(buffer, strides.size());
    for (auto stride : strides) {
        if (stride >= 0) {
            uint64_t uval = static_cast<uint64_t>(stride);
            ai = static_cast<uint8_t>(uval < 24 ? uval : (uval <= 0xFF ? 24 : (uval <= 0xFFFF ? 25 : (uval <= 0xFFFFFFFF ? 26 : 27))));
            buffer.push_back(CborPolicy::MT_UINT | ai);
            CborPolicy::encode_arg(buffer, uval);
        } else {
            uint64_t abs = static_cast<uint64_t>(-(stride + 1));
            ai = static_cast<uint8_t>(abs < 24 ? abs : (abs <= 0xFF ? 24 : (abs <= 0xFFFF ? 25 : (abs <= 0xFFFFFFFF ? 26 : 27))));
            buffer.push_back(CborPolicy::MT_NEGINT | ai);
            CborPolicy::encode_arg(buffer, abs);
        }
    }
    
    // Key "data"
    const char* data_key = "data";
    buffer.push_back(CborPolicy::MT_TEXTSTR | 4);
    CborPolicy::copy_data(buffer, reinterpret_cast<const uint8_t*>(data_key), 4);
    
    size_t data_size = tensor.size() * sizeof(T);
    ai = static_cast<uint8_t>(data_size < 24 ? data_size : 24);
    buffer.push_back(CborPolicy::MT_TEXTSTR | ai);  // Use text string for binary data
    if (data_size >= 24) CborPolicy::encode_arg(buffer, data_size);
    CborPolicy::copy_data(buffer, reinterpret_cast<const uint8_t*>(tensor.data()), data_size);
    
    return buffer;
}

template <typename T, typename Allocator = TensorAllocator<T>, typename IteratorPolicy = RowMajorPolicy>
inline Expected<Tensor<T, Allocator, IteratorPolicy>, SerializationError>
deserialize_tensor(const std::vector<uint8_t>& data, CborPolicy policy) {
    size_t pos = 0;
    
    // Decode map header
    enforce(pos < data.size(), "Underflow");
    uint8_t head = data[pos++];
    enforce((head & 0xE0) == CborPolicy::MT_MAP, "Not a map");
    uint8_t ai = head & 0x1F;
    uint64_t map_size = (ai < 24) ? ai : CborPolicy::decode_arg(data, pos, ai);
    enforce(map_size == 4, "Expected 4 keys in tensor map");
    
    std::string type_str;
    std::vector<size_t> shape;
    std::vector<ptrdiff_t> strides;
    std::vector<uint8_t> raw_data;
    
    // Parse map entries
    for (uint64_t i = 0; i < map_size; ++i) {
        // Read key
        enforce(pos < data.size(), "Underflow");
        head = data[pos++];
        enforce((head & 0xE0) == CborPolicy::MT_TEXTSTR, "Key not string");
        ai = head & 0x1F;
        uint64_t key_len = (ai < 24) ? ai : CborPolicy::decode_arg(data, pos, ai);
        std::string key(reinterpret_cast<const char*>(&data[pos]), key_len);
        pos += key_len;
        
        // Read value based on key
        if (key == "type") {
            head = data[pos++];
            enforce((head & 0xE0) == CborPolicy::MT_TEXTSTR, "Type not string");
            ai = head & 0x1F;
            uint64_t len = (ai < 24) ? ai : CborPolicy::decode_arg(data, pos, ai);
            type_str = std::string(reinterpret_cast<const char*>(&data[pos]), len);
            pos += len;
        } else if (key == "shape") {
            head = data[pos++];
            enforce((head & 0xE0) == CborPolicy::MT_ARRAY, "Shape not array");
            ai = head & 0x1F;
            uint64_t arr_len = (ai < 24) ? ai : CborPolicy::decode_arg(data, pos, ai);
            shape.resize(arr_len);
            for (auto& dim : shape) {
                head = data[pos++];
                enforce((head & 0xE0) == CborPolicy::MT_UINT, "Not uint");
                ai = head & 0x1F;
                dim = (ai < 24) ? ai : CborPolicy::decode_arg(data, pos, ai);
            }
        } else if (key == "strides") {
            head = data[pos++];
            enforce((head & 0xE0) == CborPolicy::MT_ARRAY, "Strides not array");
            ai = head & 0x1F;
            uint64_t arr_len = (ai < 24) ? ai : CborPolicy::decode_arg(data, pos, ai);
            strides.resize(arr_len);
            for (auto& stride : strides) {
                head = data[pos++];
                uint8_t mt = head & 0xE0;
                ai = head & 0x1F;
                if (mt == CborPolicy::MT_UINT) {
                    stride = static_cast<ptrdiff_t>((ai < 24) ? ai : CborPolicy::decode_arg(data, pos, ai));
                } else if (mt == CborPolicy::MT_NEGINT) {
                    uint64_t abs_val = (ai < 24) ? ai : CborPolicy::decode_arg(data, pos, ai);
                    stride = -static_cast<ptrdiff_t>(abs_val) - 1;
                } else {
                    return make_unexpected(SerializationError("Invalid stride type"));
                }
            }
        } else if (key == "data") {
            head = data[pos++];
            enforce((head & 0xE0) == CborPolicy::MT_TEXTSTR, "Data not string");
            ai = head & 0x1F;
            uint64_t len = (ai < 24) ? ai : CborPolicy::decode_arg(data, pos, ai);
            
            size_t expected_size = 1;
            for (auto dim : shape) expected_size *= dim;
            enforce(len == expected_size * sizeof(T), "Data size mismatch");
            
            raw_data.assign(data.begin() + pos, data.begin() + pos + len);
            pos += len;
        }
    }
    
    // Create tensor and copy data
    Tensor<T, Allocator, IteratorPolicy> result(shape);
    std::memcpy(result.data(), raw_data.data(), raw_data.size());
    
    return result;
}

}  // namespace cpp_utilities

#endif  // CPP_UTILITIES_BINARY_SERIALIZER_TENSOR_H
