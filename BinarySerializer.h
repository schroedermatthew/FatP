// BinarySerializer.h
// High-performance, header-only binary serialization library with multiple format policies
// Supports CustomBinaryPolicy (simple little-endian format) and CborPolicy (RFC 8949 CBOR)
// C++17, no dependencies beyond standard library
#ifndef FATP_BINARY_SERIALIZER_H
#define FATP_BINARY_SERIALIZER_H

#include "Expected.h"
#include "enforce.h"
#include <cstdint>
#include <string>
#include <vector>
#include <type_traits>
#include <cstring>
#include <ostream>
#include <istream>
#include <sstream>

#ifdef __AVX2__
#include <immintrin.h>
#endif

namespace fat_p {

// Custom error type to avoid Expected<string, string>
struct SerializationError {
    std::string message;
    explicit SerializationError(std::string msg) : message(std::move(msg)) {}
};

// =============================================================================
// Policy Definitions
// =============================================================================

struct CustomBinaryPolicy {
    static constexpr uint8_t TYPE_UINT8 = 0;
    static constexpr uint8_t TYPE_UINT16 = 1;
    static constexpr uint8_t TYPE_UINT32 = 2;
    static constexpr uint8_t TYPE_UINT64 = 3;
    static constexpr uint8_t TYPE_INT32 = 6;
    static constexpr uint8_t TYPE_INT64 = 7;
    static constexpr uint8_t TYPE_DOUBLE = 9;
    static constexpr uint8_t TYPE_BOOL = 10;
    static constexpr uint8_t TYPE_STRING = 11;
    static constexpr uint8_t TYPE_ARRAY = 13;

    template <typename T>
    static void encode_le(std::vector<uint8_t>& buffer, T value) {
        for (size_t i = 0; i < sizeof(T); ++i) {
            buffer.push_back(static_cast<uint8_t>(reinterpret_cast<const uint8_t*>(&value)[i]));
        }
    }

    template <typename T>
    static T decode_le(const std::vector<uint8_t>& data, size_t& pos) {
        enforce(pos + sizeof(T) <= data.size(), "Buffer underflow");
        T value;
        std::memcpy(&value, &data[pos], sizeof(T));
        pos += sizeof(T);
        return value;
    }

    static void copy_data(std::vector<uint8_t>& buffer, const uint8_t* src, size_t len) {
#ifdef __AVX2__
        size_t old_size = buffer.size();
        buffer.resize(old_size + len);
        uint8_t* dst = buffer.data() + old_size;
        size_t i = 0;
        for (; i + 32 <= len; i += 32) {
            __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src + i));
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + i), chunk);
        }
        std::memcpy(dst + i, src + i, len - i);
#else
        buffer.insert(buffer.end(), src, src + len);
#endif
    }
};

struct CborPolicy {
    static constexpr uint8_t MT_UINT = 0 << 5;
    static constexpr uint8_t MT_NEGINT = 1 << 5;
    static constexpr uint8_t MT_TEXTSTR = 3 << 5;
    static constexpr uint8_t MT_ARRAY = 4 << 5;
    static constexpr uint8_t MT_MAP = 5 << 5;
    static constexpr uint8_t MT_FLOAT = 7 << 5;

    static void encode_arg(std::vector<uint8_t>& buffer, uint64_t arg) {
        if (arg < 24) {
            // Already in initial byte
        } else if (arg <= 0xFF) {
            buffer.push_back(static_cast<uint8_t>(arg));
        } else if (arg <= 0xFFFF) {
            buffer.push_back(static_cast<uint8_t>(arg >> 8));
            buffer.push_back(static_cast<uint8_t>(arg & 0xFF));
        } else if (arg <= 0xFFFFFFFF) {
            buffer.push_back(static_cast<uint8_t>(arg >> 24));
            buffer.push_back(static_cast<uint8_t>((arg >> 16) & 0xFF));
            buffer.push_back(static_cast<uint8_t>((arg >> 8) & 0xFF));
            buffer.push_back(static_cast<uint8_t>(arg & 0xFF));
        } else {
            buffer.push_back(static_cast<uint8_t>(arg >> 56));
            buffer.push_back(static_cast<uint8_t>((arg >> 48) & 0xFF));
            buffer.push_back(static_cast<uint8_t>((arg >> 40) & 0xFF));
            buffer.push_back(static_cast<uint8_t>((arg >> 32) & 0xFF));
            buffer.push_back(static_cast<uint8_t>((arg >> 24) & 0xFF));
            buffer.push_back(static_cast<uint8_t>((arg >> 16) & 0xFF));
            buffer.push_back(static_cast<uint8_t>((arg >> 8) & 0xFF));
            buffer.push_back(static_cast<uint8_t>(arg & 0xFF));
        }
    }

    static uint64_t decode_arg(const std::vector<uint8_t>& data, size_t& pos, uint8_t ai) {
        if (ai < 24) return ai;
        enforce(pos < data.size(), "CBOR underflow");
        
        if (ai == 24) {
            return data[pos++];
        } else if (ai == 25) {
            enforce(pos + 2 <= data.size(), "CBOR underflow");
            uint64_t v = (static_cast<uint64_t>(data[pos]) << 8) | data[pos + 1];
            pos += 2;
            return v;
        } else if (ai == 26) {
            enforce(pos + 4 <= data.size(), "CBOR underflow");
            uint64_t v = (static_cast<uint64_t>(data[pos]) << 24) |
                        (static_cast<uint64_t>(data[pos + 1]) << 16) |
                        (static_cast<uint64_t>(data[pos + 2]) << 8) |
                        data[pos + 3];
            pos += 4;
            return v;
        } else if (ai == 27) {
            enforce(pos + 8 <= data.size(), "CBOR underflow");
            uint64_t v = (static_cast<uint64_t>(data[pos]) << 56) |
                        (static_cast<uint64_t>(data[pos + 1]) << 48) |
                        (static_cast<uint64_t>(data[pos + 2]) << 40) |
                        (static_cast<uint64_t>(data[pos + 3]) << 32) |
                        (static_cast<uint64_t>(data[pos + 4]) << 24) |
                        (static_cast<uint64_t>(data[pos + 5]) << 16) |
                        (static_cast<uint64_t>(data[pos + 6]) << 8) |
                        data[pos + 7];
            pos += 8;
            return v;
        }
        enforce(false, "Invalid CBOR AI");
        return 0;
    }

    static void copy_data(std::vector<uint8_t>& buffer, const uint8_t* src, size_t len) {
        CustomBinaryPolicy::copy_data(buffer, src, len);
    }
};

// =============================================================================
// Serializer Class
// =============================================================================

template <typename FormatPolicy = CustomBinaryPolicy>
class BinarySerializer {
public:
    // Serialize functions
    Expected<std::vector<uint8_t>, SerializationError> serialize(uint64_t value);
    Expected<std::vector<uint8_t>, SerializationError> serialize(int64_t value);
    Expected<std::vector<uint8_t>, SerializationError> serialize(bool value);
    Expected<std::vector<uint8_t>, SerializationError> serialize(double value);
    Expected<std::vector<uint8_t>, SerializationError> serialize(const std::string& value);
    
    template <typename T>
    Expected<std::vector<uint8_t>, SerializationError> serialize(const std::vector<T>& value);
    
    // Deserialize functions
    Expected<uint64_t, SerializationError> deserialize_uint64(const std::vector<uint8_t>& data);
    Expected<int64_t, SerializationError> deserialize_int64(const std::vector<uint8_t>& data);
    Expected<bool, SerializationError> deserialize_bool(const std::vector<uint8_t>& data);
    Expected<double, SerializationError> deserialize_double(const std::vector<uint8_t>& data);
    Expected<std::string, SerializationError> deserialize_string(const std::vector<uint8_t>& data);
    
    template <typename T>
    Expected<std::vector<T>, SerializationError> deserialize_vector(const std::vector<uint8_t>& data);
};

// =============================================================================
// CustomBinaryPolicy Implementations
// =============================================================================

template <>
inline Expected<std::vector<uint8_t>, SerializationError> 
BinarySerializer<CustomBinaryPolicy>::serialize(uint64_t value) {
    std::vector<uint8_t> buffer;
    buffer.push_back(CustomBinaryPolicy::TYPE_UINT64);
    CustomBinaryPolicy::encode_le(buffer, value);
    return buffer;
}

template <>
inline Expected<uint64_t, SerializationError>
BinarySerializer<CustomBinaryPolicy>::deserialize_uint64(const std::vector<uint8_t>& data) {
    size_t pos = 0;
    enforce(pos + 1 < data.size() && data[pos] == CustomBinaryPolicy::TYPE_UINT64, "Invalid uint64");
    ++pos;
    return CustomBinaryPolicy::decode_le<uint64_t>(data, pos);
}

template <>
inline Expected<std::vector<uint8_t>, SerializationError>
BinarySerializer<CustomBinaryPolicy>::serialize(int64_t value) {
    std::vector<uint8_t> buffer;
    buffer.push_back(CustomBinaryPolicy::TYPE_INT64);
    CustomBinaryPolicy::encode_le(buffer, value);
    return buffer;
}

template <>
inline Expected<int64_t, SerializationError>
BinarySerializer<CustomBinaryPolicy>::deserialize_int64(const std::vector<uint8_t>& data) {
    size_t pos = 0;
    enforce(pos + 1 < data.size() && data[pos] == CustomBinaryPolicy::TYPE_INT64, "Invalid int64");
    ++pos;
    return CustomBinaryPolicy::decode_le<int64_t>(data, pos);
}

template <>
inline Expected<std::vector<uint8_t>, SerializationError>
BinarySerializer<CustomBinaryPolicy>::serialize(bool value) {
    std::vector<uint8_t> buffer;
    buffer.push_back(CustomBinaryPolicy::TYPE_BOOL);
    buffer.push_back(value ? 1 : 0);
    return buffer;
}

template <>
inline Expected<bool, SerializationError>
BinarySerializer<CustomBinaryPolicy>::deserialize_bool(const std::vector<uint8_t>& data) {
    size_t pos = 0;
    enforce(pos + 2 <= data.size() && data[pos] == CustomBinaryPolicy::TYPE_BOOL, "Invalid bool");
    return data[pos + 1] != 0;
}

template <>
inline Expected<std::vector<uint8_t>, SerializationError>
BinarySerializer<CustomBinaryPolicy>::serialize(double value) {
    std::vector<uint8_t> buffer;
    buffer.push_back(CustomBinaryPolicy::TYPE_DOUBLE);
    CustomBinaryPolicy::encode_le(buffer, value);
    return buffer;
}

template <>
inline Expected<double, SerializationError>
BinarySerializer<CustomBinaryPolicy>::deserialize_double(const std::vector<uint8_t>& data) {
    size_t pos = 0;
    enforce(pos + 1 < data.size() && data[pos] == CustomBinaryPolicy::TYPE_DOUBLE, "Invalid double");
    ++pos;
    return CustomBinaryPolicy::decode_le<double>(data, pos);
}

template <>
inline Expected<std::vector<uint8_t>, SerializationError>
BinarySerializer<CustomBinaryPolicy>::serialize(const std::string& value) {
    std::vector<uint8_t> buffer;
    buffer.push_back(CustomBinaryPolicy::TYPE_STRING);
    CustomBinaryPolicy::encode_le(buffer, static_cast<uint64_t>(value.size()));
    CustomBinaryPolicy::copy_data(buffer, reinterpret_cast<const uint8_t*>(value.data()), value.size());
    return buffer;
}

template <>
inline Expected<std::string, SerializationError>
BinarySerializer<CustomBinaryPolicy>::deserialize_string(const std::vector<uint8_t>& data) {
    size_t pos = 0;
    enforce(pos + 1 < data.size() && data[pos] == CustomBinaryPolicy::TYPE_STRING, "Invalid string");
    ++pos;
    uint64_t len = CustomBinaryPolicy::decode_le<uint64_t>(data, pos);
    enforce(pos + len <= data.size(), "String underflow");
    std::string result(reinterpret_cast<const char*>(&data[pos]), len);
    return result;
}

template <>
template <typename T>
inline Expected<std::vector<uint8_t>, SerializationError>
BinarySerializer<CustomBinaryPolicy>::serialize(const std::vector<T>& value) {
    std::vector<uint8_t> buffer;
    buffer.push_back(CustomBinaryPolicy::TYPE_ARRAY);
    CustomBinaryPolicy::encode_le(buffer, static_cast<uint64_t>(value.size()));
    
    for (const auto& elem : value) {
        auto elem_buf = serialize(elem);
        if (!elem_buf) return make_unexpected(elem_buf.error());
        buffer.insert(buffer.end(), elem_buf->begin(), elem_buf->end());
    }
    return buffer;
}

template <>
template <typename T>
inline Expected<std::vector<T>, SerializationError>
BinarySerializer<CustomBinaryPolicy>::deserialize_vector(const std::vector<uint8_t>& data) {
    size_t pos = 0;
    enforce(pos + 1 < data.size() && data[pos] == CustomBinaryPolicy::TYPE_ARRAY, "Invalid array");
    ++pos;
    uint64_t len = CustomBinaryPolicy::decode_le<uint64_t>(data, pos);
    
    std::vector<T> result;
    result.reserve(len);
    
    for (uint64_t i = 0; i < len; ++i) {
        std::vector<uint8_t> elem_data(data.begin() + pos, data.end());
        
        Expected<T, SerializationError> elem;
        if constexpr (std::is_same_v<T, uint64_t>) {
            elem = deserialize_uint64(elem_data);
        } else if constexpr (std::is_same_v<T, int64_t>) {
            elem = deserialize_int64(elem_data);
        } else if constexpr (std::is_same_v<T, bool>) {
            elem = deserialize_bool(elem_data);
        } else if constexpr (std::is_same_v<T, double>) {
            elem = deserialize_double(elem_data);
        } else if constexpr (std::is_same_v<T, std::string>) {
            elem = deserialize_string(elem_data);
        } else {
            return make_unexpected(SerializationError("Unsupported vector element type"));
        }
        
        if (!elem) return make_unexpected(elem.error());
        
        // Advance position
        size_t elem_pos = 0;
        if constexpr (std::is_same_v<T, std::string>) {
            elem_pos += 1; // type byte
            elem_pos += sizeof(uint64_t); // length
            elem_pos += elem->size(); // data
        } else if constexpr (std::is_same_v<T, bool>) {
            elem_pos += 2;
        } else {
            elem_pos += 1 + sizeof(T);
        }
        pos += elem_pos;
        
        result.push_back(std::move(*elem));
    }
    
    return result;
}

// =============================================================================
// CborPolicy Implementations
// =============================================================================

template <>
inline Expected<std::vector<uint8_t>, SerializationError>
BinarySerializer<CborPolicy>::serialize(uint64_t value) {
    std::vector<uint8_t> buffer;
    uint8_t ai = static_cast<uint8_t>(value < 24 ? value : (value <= 0xFF ? 24 : (value <= 0xFFFF ? 25 : (value <= 0xFFFFFFFF ? 26 : 27))));
    buffer.push_back(CborPolicy::MT_UINT | ai);
    CborPolicy::encode_arg(buffer, value);
    return buffer;
}

template <>
inline Expected<uint64_t, SerializationError>
BinarySerializer<CborPolicy>::deserialize_uint64(const std::vector<uint8_t>& data) {
    size_t pos = 0;
    enforce(pos < data.size(), "Underflow");
    uint8_t head = data[pos++];
    enforce((head & 0xE0) == CborPolicy::MT_UINT, "Not uint");
    return CborPolicy::decode_arg(data, pos, head & 0x1F);
}

template <>
inline Expected<std::vector<uint8_t>, SerializationError>
BinarySerializer<CborPolicy>::serialize(int64_t value) {
    std::vector<uint8_t> buffer;
    if (value >= 0) {
        uint64_t uval = static_cast<uint64_t>(value);
        uint8_t ai = static_cast<uint8_t>(uval < 24 ? uval : (uval <= 0xFF ? 24 : (uval <= 0xFFFF ? 25 : (uval <= 0xFFFFFFFF ? 26 : 27))));
        buffer.push_back(CborPolicy::MT_UINT | ai);
        CborPolicy::encode_arg(buffer, uval);
    } else {
        uint64_t abs = static_cast<uint64_t>(-(value + 1));
        uint8_t ai = static_cast<uint8_t>(abs < 24 ? abs : (abs <= 0xFF ? 24 : (abs <= 0xFFFF ? 25 : (abs <= 0xFFFFFFFF ? 26 : 27))));
        buffer.push_back(CborPolicy::MT_NEGINT | ai);
        CborPolicy::encode_arg(buffer, abs);
    }
    return buffer;
}

template <>
inline Expected<int64_t, SerializationError>
BinarySerializer<CborPolicy>::deserialize_int64(const std::vector<uint8_t>& data) {
    size_t pos = 0;
    enforce(pos < data.size(), "Underflow");
    uint8_t head = data[pos++];
    uint8_t mt = head & 0xE0;
    uint8_t ai = head & 0x1F;
    
    if (mt == CborPolicy::MT_UINT) {
        return static_cast<int64_t>(CborPolicy::decode_arg(data, pos, ai));
    } else if (mt == CborPolicy::MT_NEGINT) {
        uint64_t abs = CborPolicy::decode_arg(data, pos, ai);
        return -static_cast<int64_t>(abs) - 1;
    }
    return make_unexpected(SerializationError("Not integer"));
}

template <>
inline Expected<std::vector<uint8_t>, SerializationError>
BinarySerializer<CborPolicy>::serialize(bool value) {
    std::vector<uint8_t> buffer;
    buffer.push_back(CborPolicy::MT_FLOAT | (value ? 21 : 20));
    return buffer;
}

template <>
inline Expected<bool, SerializationError>
BinarySerializer<CborPolicy>::deserialize_bool(const std::vector<uint8_t>& data) {
    size_t pos = 0;
    enforce(pos < data.size(), "Underflow");
    uint8_t head = data[pos];
    enforce((head & 0xE0) == CborPolicy::MT_FLOAT, "Not bool");
    uint8_t val = head & 0x1F;
    enforce(val == 20 || val == 21, "Invalid bool");
    return val == 21;
}

template <>
inline Expected<std::vector<uint8_t>, SerializationError>
BinarySerializer<CborPolicy>::serialize(double value) {
    std::vector<uint8_t> buffer;
    buffer.push_back(CborPolicy::MT_FLOAT | 27);
    CustomBinaryPolicy::encode_le(buffer, value);
    return buffer;
}

template <>
inline Expected<double, SerializationError>
BinarySerializer<CborPolicy>::deserialize_double(const std::vector<uint8_t>& data) {
    size_t pos = 0;
    enforce(pos < data.size(), "Underflow");
    uint8_t head = data[pos++];
    enforce((head & 0xE0) == CborPolicy::MT_FLOAT && (head & 0x1F) == 27, "Not double");
    return CustomBinaryPolicy::decode_le<double>(data, pos);
}

template <>
inline Expected<std::vector<uint8_t>, SerializationError>
BinarySerializer<CborPolicy>::serialize(const std::string& value) {
    std::vector<uint8_t> buffer;
    uint8_t ai = static_cast<uint8_t>(value.size() < 24 ? value.size() : (value.size() <= 0xFF ? 24 : (value.size() <= 0xFFFF ? 25 : (value.size() <= 0xFFFFFFFF ? 26 : 27))));
    buffer.push_back(CborPolicy::MT_TEXTSTR | ai);
    CborPolicy::encode_arg(buffer, value.size());
    CborPolicy::copy_data(buffer, reinterpret_cast<const uint8_t*>(value.data()), value.size());
    return buffer;
}

template <>
inline Expected<std::string, SerializationError>
BinarySerializer<CborPolicy>::deserialize_string(const std::vector<uint8_t>& data) {
    size_t pos = 0;
    enforce(pos < data.size(), "Underflow");
    uint8_t head = data[pos++];
    enforce((head & 0xE0) == CborPolicy::MT_TEXTSTR, "Not string");
    uint64_t len = CborPolicy::decode_arg(data, pos, head & 0x1F);
    enforce(pos + len <= data.size(), "String underflow");
    std::string result(reinterpret_cast<const char*>(&data[pos]), len);
    return result;
}

template <>
template <typename T>
inline Expected<std::vector<uint8_t>, SerializationError>
BinarySerializer<CborPolicy>::serialize(const std::vector<T>& value) {
    std::vector<uint8_t> buffer;
    uint8_t ai = static_cast<uint8_t>(value.size() < 24 ? value.size() : (value.size() <= 0xFF ? 24 : (value.size() <= 0xFFFF ? 25 : (value.size() <= 0xFFFFFFFF ? 26 : 27))));
    buffer.push_back(CborPolicy::MT_ARRAY | ai);
    CborPolicy::encode_arg(buffer, value.size());
    
    for (const auto& elem : value) {
        auto elem_buf = serialize(elem);
        if (!elem_buf) return make_unexpected(elem_buf.error());
        buffer.insert(buffer.end(), elem_buf->begin(), elem_buf->end());
    }
    return buffer;
}

template <>
template <typename T>
inline Expected<std::vector<T>, SerializationError>
BinarySerializer<CborPolicy>::deserialize_vector(const std::vector<uint8_t>& data) {
    size_t pos = 0;
    enforce(pos < data.size(), "Underflow");
    uint8_t head = data[pos++];
    enforce((head & 0xE0) == CborPolicy::MT_ARRAY, "Not array");
    uint64_t len = CborPolicy::decode_arg(data, pos, head & 0x1F);
    
    std::vector<T> result;
    result.reserve(len);
    
    for (uint64_t i = 0; i < len; ++i) {
        std::vector<uint8_t> elem_data(data.begin() + pos, data.end());
        
        Expected<T, SerializationError> elem;
        if constexpr (std::is_same_v<T, uint64_t>) {
            elem = deserialize_uint64(elem_data);
        } else if constexpr (std::is_same_v<T, int64_t>) {
            elem = deserialize_int64(elem_data);
        } else if constexpr (std::is_same_v<T, bool>) {
            elem = deserialize_bool(elem_data);
        } else if constexpr (std::is_same_v<T, double>) {
            elem = deserialize_double(elem_data);
        } else if constexpr (std::is_same_v<T, std::string>) {
            elem = deserialize_string(elem_data);
        } else {
            return make_unexpected(SerializationError("Unsupported vector element type"));
        }
        
        if (!elem) return make_unexpected(elem.error());
        
        // Calculate element size to advance position
        std::vector<uint8_t> temp_buf = *serialize(*elem);
        pos += temp_buf.size();
        
        result.push_back(std::move(*elem));
    }
    
    return result;
}

/**
 * @brief Binary output archive for serialization
 * @details Provides operator& interface for seamless serialization to std::ostream
 */
class BinaryOutputArchive {
public:
    using is_loading = std::false_type;
    
    explicit BinaryOutputArchive(std::ostream& os) : os_(os) {
        enforce(os.good(), "Output stream must be in good state");
    }
    
    template<typename T>
    BinaryOutputArchive& operator&(const T& value) {
        serialize_impl(value);
        return *this;
    }
    
    bool good() const { return os_.good(); }
    
private:
    std::ostream& os_;
    
    template<typename T>
    void serialize_impl(const T& value) {
        if constexpr (std::is_arithmetic_v<T> || std::is_enum_v<T>) {
            os_.write(reinterpret_cast<const char*>(&value), sizeof(T));
            enforce(os_.good(), "Failed to write arithmetic/enum value");
        } else if constexpr (std::is_same_v<T, std::string>) {
            size_t len = value.size();
            os_.write(reinterpret_cast<const char*>(&len), sizeof(len));
            if (len > 0) {
                os_.write(value.data(), static_cast<std::streamsize>(len));
            }
            enforce(os_.good(), "Failed to write string");
        } else {
            const_cast<T&>(value).serialize(*this);
        }
    }
};

/**
 * @brief Binary input archive for deserialization
 * @details Provides operator& interface for seamless deserialization from std::istream
 */
class BinaryInputArchive {
public:
    using is_loading = std::true_type;
    
    explicit BinaryInputArchive(std::istream& is) : is_(is) {
        enforce(is.good(), "Input stream must be in good state");
    }
    
    template<typename T>
    BinaryInputArchive& operator&(T& value) {
        deserialize_impl(value);
        return *this;
    }
    
    bool good() const { return is_.good(); }
    
private:
    std::istream& is_;
    
    template<typename T>
    void deserialize_impl(T& value) {
        if constexpr (std::is_arithmetic_v<T> || std::is_enum_v<T>) {
            is_.read(reinterpret_cast<char*>(&value), sizeof(T));
            enforce(is_.good(), "Failed to read arithmetic/enum value");
        } else if constexpr (std::is_same_v<T, std::string>) {
            size_t len;
            is_.read(reinterpret_cast<char*>(&len), sizeof(len));
            enforce(is_.good(), "Failed to read string length");
            enforce(len <= 1000000000, "String length too large");
            value.resize(len);
            if (len > 0) {
                is_.read(&value[0], static_cast<std::streamsize>(len));
                enforce(is_.good(), "Failed to read string data");
            }
        } else {
            value.serialize(*this);
        }
    }
};

}  // namespace fat_p

#endif  // FATP_BINARY_SERIALIZER_H
