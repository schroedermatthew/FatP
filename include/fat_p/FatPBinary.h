#pragma once

/*
FATP_META:
  meta_version: 1
  component: FatPBinary
  file_role: public_header
  path: include/fat_p/FatPBinary.h
  namespace: fat_p
  layer: Domain
  summary: "Public header for FatPBinary."
  api_stability: in_work
  related:
    docs_search: "FatPBinary"
    tests:
      - components/BinarySerialization/tests/test_FatPBinary.cpp
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
 * @file FatPBinary.h
 * @brief FAT-P binary serialization format
 *
 * @details
 * Fat-P integrated binary serialization with Expected-based error handling,
 * HpcVector-backed buffers, and native fat_p type support.
 * This is the FAT-P INTEGRATION layer built on top of BinaryLite.h.
 * For standalone binary serialization without fat_p dependencies, use BinaryLite.h.
 * Features:
 * - BinaryResult<T> = Expected<T, BinaryError> for explicit error handling
 * - BinaryBuffer = HpcVector<uint8_t, 64> for SIMD-aligned storage
 * - BinaryTraits<T> for extensible type serialization
 * - Native support for fat_p types (Expected, SmallVector, StrongId, EnumPlus)
 * - Little-endian format optimized for internal data exchange
 * C++20, header-only
 */

// These headers can be mocked for standalone testing
#ifndef FATP_EXPECTED_H
#include "Expected.h"
#endif

#ifndef FATP_HPC_VECTOR_H
#include "HpcVector.h"
#endif

#include "BinaryLite.h"

#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace fat_p
{
namespace binary_fatp
{

// ============================================================================
// Error / Result Types
// ============================================================================

struct BinaryError
{
    std::string message;

    BinaryError() = default;

    explicit BinaryError(std::string msg)
        : message(std::move(msg))
    {
    }
};

template <typename T>
using BinaryResult = Expected<T, BinaryError>;

// ============================================================================
// Internal Traits for Container Detection
// ============================================================================

namespace detail
{

template <typename Container, typename = void>
struct is_binary_byte_container : std::false_type
{
};

template <typename Container>
struct is_binary_byte_container<
    Container,
    std::void_t<typename Container::value_type,
                decltype(std::declval<Container&>().data()),
                decltype(std::declval<const Container&>().data()),
                decltype(std::declval<Container&>().size()),
                decltype(std::declval<Container&>().push_back(std::declval<std::uint8_t>()))>>
    : std::bool_constant<std::is_same_v<typename Container::value_type, std::uint8_t> &&
                         std::is_same_v<decltype(std::declval<Container&>().data()), std::uint8_t*> &&
                         std::is_same_v<decltype(std::declval<const Container&>().data()), const std::uint8_t*>>
{
};

template <typename Container>
inline constexpr bool is_binary_byte_container_v = is_binary_byte_container<Container>::value;

} // namespace detail

// ============================================================================
// Default Buffer Type
// ============================================================================

template <typename T = std::uint8_t, std::size_t Alignment = 64, typename Policy = memory::NumaLocalPolicy>
using DefaultBinaryBuffer = HpcVector<T, Alignment, Policy>;

using BinaryBuffer = DefaultBinaryBuffer<>;

// ============================================================================
// BinaryWriter: Expected-based writer over arbitrary byte containers
// ============================================================================

template <typename Buffer = BinaryBuffer>
class BinaryWriter
{
public:
    static_assert(detail::is_binary_byte_container_v<Buffer>,
                  "Buffer must store uint8_t and provide data()/size()/push_back()");

    explicit BinaryWriter(Buffer& buffer) noexcept
        : mBuffer(buffer)
    {
    }

    BinaryWriter(const BinaryWriter&) = delete;
    BinaryWriter& operator=(const BinaryWriter&) = delete;

    // ========================================================================
    // Unsigned integer writes
    // ========================================================================

    void writeUint8(std::uint8_t value)
    {
        mBuffer.push_back(static_cast<std::uint8_t>(binary::TypeTag::Uint8));
        mBuffer.push_back(value);
    }

    void writeUint16(std::uint16_t value)
    {
        mBuffer.push_back(static_cast<std::uint8_t>(binary::TypeTag::Uint16));
        writeLe(value);
    }

    void writeUint32(std::uint32_t value)
    {
        mBuffer.push_back(static_cast<std::uint8_t>(binary::TypeTag::Uint32));
        writeLe(value);
    }

    void writeUint64(std::uint64_t value)
    {
        mBuffer.push_back(static_cast<std::uint8_t>(binary::TypeTag::Uint64));
        writeLe(value);
    }

    template <typename UInt>
        requires (std::is_unsigned_v<UInt> && std::is_integral_v<UInt>)
    void writeUint(UInt value)
    {
        if constexpr (sizeof(UInt) == 1)
        {
            writeUint8(static_cast<std::uint8_t>(value));
        }
        else if constexpr (sizeof(UInt) == 2)
        {
            writeUint16(static_cast<std::uint16_t>(value));
        }
        else if constexpr (sizeof(UInt) == 4)
        {
            writeUint32(static_cast<std::uint32_t>(value));
        }
        else
        {
            writeUint64(static_cast<std::uint64_t>(value));
        }
    }

    // ========================================================================
    // Signed integer writes
    // ========================================================================

    void writeInt8(std::int8_t value)
    {
        mBuffer.push_back(static_cast<std::uint8_t>(binary::TypeTag::Int8));
        mBuffer.push_back(static_cast<std::uint8_t>(value));
    }

    void writeInt16(std::int16_t value)
    {
        mBuffer.push_back(static_cast<std::uint8_t>(binary::TypeTag::Int16));
        writeLe(value);
    }

    void writeInt32(std::int32_t value)
    {
        mBuffer.push_back(static_cast<std::uint8_t>(binary::TypeTag::Int32));
        writeLe(value);
    }

    void writeInt64(std::int64_t value)
    {
        mBuffer.push_back(static_cast<std::uint8_t>(binary::TypeTag::Int64));
        writeLe(value);
    }

    template <typename SInt>
        requires (std::is_signed_v<SInt> && std::is_integral_v<SInt>)
    void writeInt(SInt value)
    {
        if constexpr (sizeof(SInt) == 1)
        {
            writeInt8(static_cast<std::int8_t>(value));
        }
        else if constexpr (sizeof(SInt) == 2)
        {
            writeInt16(static_cast<std::int16_t>(value));
        }
        else if constexpr (sizeof(SInt) == 4)
        {
            writeInt32(static_cast<std::int32_t>(value));
        }
        else
        {
            writeInt64(static_cast<std::int64_t>(value));
        }
    }

    // ========================================================================
    // Other primitive writes
    // ========================================================================

    void writeFloat(float value)
    {
        mBuffer.push_back(static_cast<std::uint8_t>(binary::TypeTag::Float32));
        writeLe(value);
    }

    void writeDouble(double value)
    {
        mBuffer.push_back(static_cast<std::uint8_t>(binary::TypeTag::Float64));
        writeLe(value);
    }

    void writeBool(bool value)
    {
        mBuffer.push_back(static_cast<std::uint8_t>(binary::TypeTag::Bool));
        mBuffer.push_back(value ? 1U : 0U);
    }

    void writeString(const std::string& value)
    {
        mBuffer.push_back(static_cast<std::uint8_t>(binary::TypeTag::String));
        writeLe(static_cast<std::uint64_t>(value.size()));

        const auto* data = reinterpret_cast<const std::uint8_t*>(value.data());
        mBuffer.insert(mBuffer.end(), data, data + value.size());
    }

    void writeBytes(const std::uint8_t* data, std::size_t size)
    {
        mBuffer.push_back(static_cast<std::uint8_t>(binary::TypeTag::Bytes));
        writeLe(static_cast<std::uint64_t>(size));
        mBuffer.insert(mBuffer.end(), data, data + size);
    }

    template <typename ByteContainer>
    void writeBytes(const ByteContainer& bytes)
    {
        static_assert(detail::is_binary_byte_container_v<ByteContainer>, "ByteContainer must store uint8_t");
        writeBytes(bytes.data(), bytes.size());
    }

    void writeArrayHeader(std::uint64_t size)
    {
        mBuffer.push_back(static_cast<std::uint8_t>(binary::TypeTag::Array));
        writeLe(size);
    }

    void writeMapHeader(std::uint64_t size)
    {
        mBuffer.push_back(static_cast<std::uint8_t>(binary::TypeTag::Map));
        writeLe(size);
    }

    std::size_t size() const noexcept
    {
        return mBuffer.size();
    }

private:
    Buffer& mBuffer;

    template <typename T>
    void writeLe(T value)
    {
        static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable");

        const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
        mBuffer.insert(mBuffer.end(), bytes, bytes + sizeof(T));
    }
};

// ============================================================================
// BinaryReader: Expected-based reader over contiguous bytes
// ============================================================================

class BinaryReader
{
public:
    BinaryReader(const std::uint8_t* data, std::size_t size) noexcept
        : mData(data)
        , mSize(size)
        , mPos(0)
    {
    }

    template <typename Container>
        requires detail::is_binary_byte_container_v<Container>
    explicit BinaryReader(const Container& buffer) noexcept
        : BinaryReader(buffer.data(), buffer.size())
    {
    }

    std::size_t remaining() const noexcept
    {
        return mSize - mPos;
    }

    bool empty() const noexcept
    {
        return remaining() == 0U;
    }

    BinaryResult<binary::TypeTag> peekType() const
    {
        if (mPos >= mSize)
        {
            return make_unexpected(BinaryError("Buffer underflow peeking type"));
        }
        return static_cast<binary::TypeTag>(mData[mPos]);
    }

    // ========================================================================
    // Unsigned integer reads
    // ========================================================================

    BinaryResult<std::uint8_t> readUint8()
    {
        auto tag = expectTag(binary::TypeTag::Uint8);
        if (!tag)
        {
            return make_unexpected(tag.error());
        }
        return readByte();
    }

    BinaryResult<std::uint16_t> readUint16()
    {
        auto tag = expectTag(binary::TypeTag::Uint16);
        if (!tag)
        {
            return make_unexpected(tag.error());
        }
        return readLe<std::uint16_t>();
    }

    BinaryResult<std::uint32_t> readUint32()
    {
        auto tag = expectTag(binary::TypeTag::Uint32);
        if (!tag)
        {
            return make_unexpected(tag.error());
        }
        return readLe<std::uint32_t>();
    }

    BinaryResult<std::uint64_t> readUint64()
    {
        auto tag = expectTag(binary::TypeTag::Uint64);
        if (!tag)
        {
            return make_unexpected(tag.error());
        }
        return readLe<std::uint64_t>();
    }

    // ========================================================================
    // Signed integer reads
    // ========================================================================

    BinaryResult<std::int8_t> readInt8()
    {
        auto tag = expectTag(binary::TypeTag::Int8);
        if (!tag)
        {
            return make_unexpected(tag.error());
        }
        auto byte = readByte();
        if (!byte)
        {
            return make_unexpected(byte.error());
        }
        return static_cast<std::int8_t>(*byte);
    }

    BinaryResult<std::int16_t> readInt16()
    {
        auto tag = expectTag(binary::TypeTag::Int16);
        if (!tag)
        {
            return make_unexpected(tag.error());
        }
        return readLe<std::int16_t>();
    }

    BinaryResult<std::int32_t> readInt32()
    {
        auto tag = expectTag(binary::TypeTag::Int32);
        if (!tag)
        {
            return make_unexpected(tag.error());
        }
        return readLe<std::int32_t>();
    }

    BinaryResult<std::int64_t> readInt64()
    {
        auto tag = expectTag(binary::TypeTag::Int64);
        if (!tag)
        {
            return make_unexpected(tag.error());
        }
        return readLe<std::int64_t>();
    }

    // ========================================================================
    // Other primitive reads
    // ========================================================================

    BinaryResult<float> readFloat()
    {
        auto tag = expectTag(binary::TypeTag::Float32);
        if (!tag)
        {
            return make_unexpected(tag.error());
        }
        return readLe<float>();
    }

    BinaryResult<double> readDouble()
    {
        auto tag = expectTag(binary::TypeTag::Float64);
        if (!tag)
        {
            return make_unexpected(tag.error());
        }
        return readLe<double>();
    }

    BinaryResult<bool> readBool()
    {
        auto tag = expectTag(binary::TypeTag::Bool);
        if (!tag)
        {
            return make_unexpected(tag.error());
        }
        auto byte = readByte();
        if (!byte)
        {
            return make_unexpected(byte.error());
        }
        return *byte != 0;
    }

    BinaryResult<std::string> readString()
    {
        auto tag = expectTag(binary::TypeTag::String);
        if (!tag)
        {
            return make_unexpected(tag.error());
        }

        auto lenResult = readLe<std::uint64_t>();
        if (!lenResult)
        {
            return make_unexpected(lenResult.error());
        }

        const auto len = static_cast<std::size_t>(*lenResult);
        if (remaining() < len)
        {
            return make_unexpected(BinaryError("Buffer underflow reading string"));
        }

        std::string result(reinterpret_cast<const char*>(mData + mPos), len);
        mPos += len;
        return result;
    }

    BinaryResult<std::vector<std::uint8_t>> readBytes()
    {
        auto tag = expectTag(binary::TypeTag::Bytes);
        if (!tag)
        {
            return make_unexpected(tag.error());
        }

        auto lenResult = readLe<std::uint64_t>();
        if (!lenResult)
        {
            return make_unexpected(lenResult.error());
        }

        const auto len = static_cast<std::size_t>(*lenResult);
        if (remaining() < len)
        {
            return make_unexpected(BinaryError("Buffer underflow reading bytes"));
        }

        std::vector<std::uint8_t> result(mData + mPos, mData + mPos + len);
        mPos += len;
        return result;
    }

    BinaryResult<std::uint64_t> readArrayHeader()
    {
        auto tag = expectTag(binary::TypeTag::Array);
        if (!tag)
        {
            return make_unexpected(tag.error());
        }
        return readLe<std::uint64_t>();
    }

    BinaryResult<std::uint64_t> readMapHeader()
    {
        auto tag = expectTag(binary::TypeTag::Map);
        if (!tag)
        {
            return make_unexpected(tag.error());
        }
        return readLe<std::uint64_t>();
    }

private:
    const std::uint8_t* mData;
    std::size_t mSize;
    std::size_t mPos;

    BinaryResult<std::uint8_t> readByte()
    {
        if (mPos >= mSize)
        {
            return make_unexpected(BinaryError("Buffer underflow reading byte"));
        }
        return mData[mPos++];
    }

    template <typename T>
    BinaryResult<T> readLe()
    {
        static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable");

        if (remaining() < sizeof(T))
        {
            return make_unexpected(BinaryError("Buffer underflow"));
        }

        T value;
        std::memcpy(&value, mData + mPos, sizeof(T));
        mPos += sizeof(T);
        return value;
    }

    BinaryResult<void> expectTag(binary::TypeTag expected)
    {
        if (mPos >= mSize)
        {
            return make_unexpected(BinaryError("Buffer underflow reading tag"));
        }

        const auto actual = static_cast<binary::TypeTag>(mData[mPos++]);
        if (actual != expected)
        {
            return make_unexpected(BinaryError("Type mismatch: expected " + std::to_string(static_cast<int>(expected)) +
                                               " got " + std::to_string(static_cast<int>(actual))));
        }
        return {};
    }
};

// ============================================================================
// BinaryTraits: Fat-P style trait layer for binary serialization
// ============================================================================

template <typename T>
struct BinaryTraits;

// ----------------------------------------------------------------------------
// Signed integers
// ----------------------------------------------------------------------------

template <typename T>
    requires (std::is_integral_v<T> && std::is_signed_v<T>)
struct BinaryTraits<T>
{
    template <typename Writer>
    static void encode(Writer& writer, T value)
    {
        writer.writeInt(value);
    }

    template <typename Reader>
    static BinaryResult<T> decode(Reader& reader)
    {
        BinaryResult<std::int64_t> result;

        auto tag = reader.peekType();
        if (!tag)
        {
            return make_unexpected(tag.error());
        }

        switch (*tag)
        {
            case binary::TypeTag::Int8:
            {
                auto v = reader.readInt8();
                if (!v)
                {
                    return make_unexpected(v.error());
                }
                result = static_cast<std::int64_t>(*v);
                break;
            }
            case binary::TypeTag::Int16:
            {
                auto v = reader.readInt16();
                if (!v)
                {
                    return make_unexpected(v.error());
                }
                result = static_cast<std::int64_t>(*v);
                break;
            }
            case binary::TypeTag::Int32:
            {
                auto v = reader.readInt32();
                if (!v)
                {
                    return make_unexpected(v.error());
                }
                result = static_cast<std::int64_t>(*v);
                break;
            }
            case binary::TypeTag::Int64:
            {
                auto v = reader.readInt64();
                if (!v)
                {
                    return make_unexpected(v.error());
                }
                result = *v;
                break;
            }
            default:
                return make_unexpected(BinaryError("Expected signed integer"));
        }

        if (*result < static_cast<std::int64_t>(std::numeric_limits<T>::min()) ||
            *result > static_cast<std::int64_t>(std::numeric_limits<T>::max()))
        {
            return make_unexpected(BinaryError("Integer out of range"));
        }

        return static_cast<T>(*result);
    }
};

// ----------------------------------------------------------------------------
// Unsigned integers
// ----------------------------------------------------------------------------

template <typename T>
    requires (std::is_integral_v<T> && std::is_unsigned_v<T>)
struct BinaryTraits<T>
{
    template <typename Writer>
    static void encode(Writer& writer, T value)
    {
        writer.writeUint(value);
    }

    template <typename Reader>
    static BinaryResult<T> decode(Reader& reader)
    {
        BinaryResult<std::uint64_t> result;

        auto tag = reader.peekType();
        if (!tag)
        {
            return make_unexpected(tag.error());
        }

        switch (*tag)
        {
            case binary::TypeTag::Uint8:
            {
                auto v = reader.readUint8();
                if (!v)
                {
                    return make_unexpected(v.error());
                }
                result = static_cast<std::uint64_t>(*v);
                break;
            }
            case binary::TypeTag::Uint16:
            {
                auto v = reader.readUint16();
                if (!v)
                {
                    return make_unexpected(v.error());
                }
                result = static_cast<std::uint64_t>(*v);
                break;
            }
            case binary::TypeTag::Uint32:
            {
                auto v = reader.readUint32();
                if (!v)
                {
                    return make_unexpected(v.error());
                }
                result = static_cast<std::uint64_t>(*v);
                break;
            }
            case binary::TypeTag::Uint64:
            {
                auto v = reader.readUint64();
                if (!v)
                {
                    return make_unexpected(v.error());
                }
                result = *v;
                break;
            }
            default:
                return make_unexpected(BinaryError("Expected unsigned integer"));
        }

        if (*result > std::numeric_limits<T>::max())
        {
            return make_unexpected(BinaryError("Unsigned integer out of range"));
        }

        return static_cast<T>(*result);
    }
};

// ----------------------------------------------------------------------------
// Enums (via underlying type)
// ----------------------------------------------------------------------------

template <typename T>
    requires std::is_enum_v<T>
struct BinaryTraits<T>
{
    template <typename Writer>
    static void encode(Writer& writer, T value)
    {
        using Underlying = std::underlying_type_t<T>;
        BinaryTraits<Underlying>::encode(writer, static_cast<Underlying>(value));
    }

    template <typename Reader>
    static BinaryResult<T> decode(Reader& reader)
    {
        using Underlying = std::underlying_type_t<T>;
        auto v = BinaryTraits<Underlying>::decode(reader);
        if (!v)
        {
            return make_unexpected(v.error());
        }
        return static_cast<T>(*v);
    }
};

// ----------------------------------------------------------------------------
// bool
// ----------------------------------------------------------------------------

template <>
struct BinaryTraits<bool>
{
    template <typename Writer>
    static void encode(Writer& writer, bool value)
    {
        writer.writeBool(value);
    }

    template <typename Reader>
    static BinaryResult<bool> decode(Reader& reader)
    {
        return reader.readBool();
    }
};

// ----------------------------------------------------------------------------
// float
// ----------------------------------------------------------------------------

template <>
struct BinaryTraits<float>
{
    template <typename Writer>
    static void encode(Writer& writer, float value)
    {
        writer.writeFloat(value);
    }

    template <typename Reader>
    static BinaryResult<float> decode(Reader& reader)
    {
        return reader.readFloat();
    }
};

// ----------------------------------------------------------------------------
// double
// ----------------------------------------------------------------------------

template <>
struct BinaryTraits<double>
{
    template <typename Writer>
    static void encode(Writer& writer, double value)
    {
        writer.writeDouble(value);
    }

    template <typename Reader>
    static BinaryResult<double> decode(Reader& reader)
    {
        return reader.readDouble();
    }
};

// ----------------------------------------------------------------------------
// std::string
// ----------------------------------------------------------------------------

template <>
struct BinaryTraits<std::string>
{
    template <typename Writer>
    static void encode(Writer& writer, const std::string& value)
    {
        writer.writeString(value);
    }

    template <typename Reader>
    static BinaryResult<std::string> decode(Reader& reader)
    {
        return reader.readString();
    }
};

// ----------------------------------------------------------------------------
// std::vector
// ----------------------------------------------------------------------------

template <typename T, typename Alloc>
struct BinaryTraits<std::vector<T, Alloc>>
{
    template <typename Writer>
    static void encode(Writer& writer, const std::vector<T, Alloc>& vec)
    {
        writer.writeArrayHeader(static_cast<std::uint64_t>(vec.size()));
        for (const auto& elem : vec)
        {
            BinaryTraits<T>::encode(writer, elem);
        }
    }

    template <typename Reader>
    static BinaryResult<std::vector<T, Alloc>> decode(Reader& reader)
    {
        auto lenResult = reader.readArrayHeader();
        if (!lenResult)
        {
            return make_unexpected(lenResult.error());
        }

        const auto len = *lenResult;
        if (len > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))
        {
            return make_unexpected(BinaryError("Array length too large"));
        }

        std::vector<T, Alloc> result;
        result.reserve(static_cast<std::size_t>(len));

        for (std::uint64_t i = 0; i < len; ++i)
        {
            auto elem = BinaryTraits<T>::decode(reader);
            if (!elem)
            {
                return make_unexpected(elem.error());
            }
            result.push_back(std::move(*elem));
        }

        return result;
    }
};

// ----------------------------------------------------------------------------
// std::map
// ----------------------------------------------------------------------------

template <typename K, typename V, typename Compare, typename Alloc>
struct BinaryTraits<std::map<K, V, Compare, Alloc>>
{
    template <typename Writer>
    static void encode(Writer& writer, const std::map<K, V, Compare, Alloc>& m)
    {
        writer.writeMapHeader(static_cast<std::uint64_t>(m.size()));
        for (const auto& kv : m)
        {
            BinaryTraits<K>::encode(writer, kv.first);
            BinaryTraits<V>::encode(writer, kv.second);
        }
    }

    template <typename Reader>
    static BinaryResult<std::map<K, V, Compare, Alloc>> decode(Reader& reader)
    {
        auto lenResult = reader.readMapHeader();
        if (!lenResult)
        {
            return make_unexpected(lenResult.error());
        }

        const auto len = *lenResult;
        if (len > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))
        {
            return make_unexpected(BinaryError("Map length too large"));
        }

        std::map<K, V, Compare, Alloc> result;

        for (std::uint64_t i = 0; i < len; ++i)
        {
            auto key = BinaryTraits<K>::decode(reader);
            if (!key)
            {
                return make_unexpected(key.error());
            }

            auto value = BinaryTraits<V>::decode(reader);
            if (!value)
            {
                return make_unexpected(value.error());
            }

            result.emplace(std::move(*key), std::move(*value));
        }

        return result;
    }
};

// ============================================================================
// Free-Function Frontend API
// ============================================================================

template <typename T, typename Writer>
void binaryEncode(Writer& writer, const T& value)
{
    BinaryTraits<T>::encode(writer, value);
}

template <typename T, typename Reader>
BinaryResult<T> binaryDecode(Reader& reader)
{
    return BinaryTraits<T>::decode(reader);
}

template <typename T, typename ByteContainer>
BinaryResult<void> binaryEncodeTo(ByteContainer& buffer, const T& value)
{
    static_assert(detail::is_binary_byte_container_v<ByteContainer>,
                  "ByteContainer must store uint8_t and provide "
                  "data()/size()/push_back()");

    BinaryWriter<ByteContainer> writer(buffer);
    try
    {
        BinaryTraits<T>::encode(writer, value);
    }
    catch (const std::exception& ex)
    {
        return make_unexpected(BinaryError(ex.what()));
    }
    return {};
}

template <typename T, typename ByteContainer>
BinaryResult<T> binaryDecodeFrom(const ByteContainer& buffer)
{
    static_assert(detail::is_binary_byte_container_v<ByteContainer>,
                  "ByteContainer must store uint8_t and provide "
                  "data()/size()");

    BinaryReader reader(buffer);
    try
    {
        return BinaryTraits<T>::decode(reader);
    }
    catch (const std::exception& ex)
    {
        return make_unexpected(BinaryError(ex.what()));
    }
}

} // namespace binary_fatp
} // namespace fat_p
