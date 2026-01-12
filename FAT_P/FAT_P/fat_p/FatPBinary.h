/**
 * @file FatPBinary.h
 * @brief FAT-P binary serialization format
 *
 * @layer Domain
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
 * C++17, header-only
 */
#pragma once
/*
FATP_META:
  meta_version: 1
  component: FatPBinary
  file_role: public_header
  path: fat_p/FatPBinary.h
  namespace: fat_p
  layer: Domain
  summary: "Public header for FatPBinary."
  api_stability: in_work
  related:
    docs_search: "FatPBinary"
    tests:
      - tests/test_FatPBinary.cpp
  hygiene:
    pragma_once: true
    include_guard: true
    defines_total: 2
    defines_unprefixed: 1
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
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
    std::void_t<
        typename Container::value_type,
        decltype(std::declval<Container&>().data()),
        decltype(std::declval<const Container&>().data()),
        decltype(std::declval<Container&>().size()),
        decltype(std::declval<Container&>().push_back(
            std::declval<std::uint8_t>()))>>
    : std::bool_constant<
          std::is_same_v<typename Container::value_type, std::uint8_t> &&
          std::is_same_v<decltype(std::declval<Container&>().data()),
                         std::uint8_t*> &&
          std::is_same_v<decltype(std::declval<const Container&>().data()),
                         const std::uint8_t*>>
{
};

template <typename Container>
inline constexpr bool is_binary_byte_container_v =
    is_binary_byte_container<Container>::value;

} // namespace detail

// ============================================================================
// Default Buffer Type
// ============================================================================

template <typename T = std::uint8_t,
          std::size_t Alignment = 64,
          typename Policy = memory::NumaLocalPolicy>
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
        : buffer_(buffer)
    {
    }

    BinaryWriter(const BinaryWriter&) = delete;
    BinaryWriter& operator=(const BinaryWriter&) = delete;

    // ========================================================================
    // Unsigned integer writes
    // ========================================================================

    void write_uint8(std::uint8_t value)
    {
        buffer_.push_back(static_cast<std::uint8_t>(binary::TypeTag::Uint8));
        buffer_.push_back(value);
    }

    void write_uint16(std::uint16_t value)
    {
        buffer_.push_back(static_cast<std::uint8_t>(binary::TypeTag::Uint16));
        write_le(value);
    }

    void write_uint32(std::uint32_t value)
    {
        buffer_.push_back(static_cast<std::uint8_t>(binary::TypeTag::Uint32));
        write_le(value);
    }

    void write_uint64(std::uint64_t value)
    {
        buffer_.push_back(static_cast<std::uint8_t>(binary::TypeTag::Uint64));
        write_le(value);
    }

    template <typename UInt,
              typename = std::enable_if_t<std::is_unsigned_v<UInt> &&
                                          std::is_integral_v<UInt>>>
    void write_uint(UInt value)
    {
        if constexpr (sizeof(UInt) == 1)
        {
            write_uint8(static_cast<std::uint8_t>(value));
        }
        else if constexpr (sizeof(UInt) == 2)
        {
            write_uint16(static_cast<std::uint16_t>(value));
        }
        else if constexpr (sizeof(UInt) == 4)
        {
            write_uint32(static_cast<std::uint32_t>(value));
        }
        else
        {
            write_uint64(static_cast<std::uint64_t>(value));
        }
    }

    // ========================================================================
    // Signed integer writes
    // ========================================================================

    void write_int8(std::int8_t value)
    {
        buffer_.push_back(static_cast<std::uint8_t>(binary::TypeTag::Int8));
        buffer_.push_back(static_cast<std::uint8_t>(value));
    }

    void write_int16(std::int16_t value)
    {
        buffer_.push_back(static_cast<std::uint8_t>(binary::TypeTag::Int16));
        write_le(value);
    }

    void write_int32(std::int32_t value)
    {
        buffer_.push_back(static_cast<std::uint8_t>(binary::TypeTag::Int32));
        write_le(value);
    }

    void write_int64(std::int64_t value)
    {
        buffer_.push_back(static_cast<std::uint8_t>(binary::TypeTag::Int64));
        write_le(value);
    }

    template <typename SInt,
              typename = std::enable_if_t<std::is_signed_v<SInt> &&
                                          std::is_integral_v<SInt>>>
    void write_int(SInt value)
    {
        if constexpr (sizeof(SInt) == 1)
        {
            write_int8(static_cast<std::int8_t>(value));
        }
        else if constexpr (sizeof(SInt) == 2)
        {
            write_int16(static_cast<std::int16_t>(value));
        }
        else if constexpr (sizeof(SInt) == 4)
        {
            write_int32(static_cast<std::int32_t>(value));
        }
        else
        {
            write_int64(static_cast<std::int64_t>(value));
        }
    }

    // ========================================================================
    // Other primitive writes
    // ========================================================================

    void write_float(float value)
    {
        buffer_.push_back(static_cast<std::uint8_t>(binary::TypeTag::Float32));
        write_le(value);
    }

    void write_double(double value)
    {
        buffer_.push_back(static_cast<std::uint8_t>(binary::TypeTag::Float64));
        write_le(value);
    }

    void write_bool(bool value)
    {
        buffer_.push_back(static_cast<std::uint8_t>(binary::TypeTag::Bool));
        buffer_.push_back(value ? 1U : 0U);
    }

    void write_string(const std::string& value)
    {
        buffer_.push_back(static_cast<std::uint8_t>(binary::TypeTag::String));
        write_le(static_cast<std::uint64_t>(value.size()));

        const auto* data = reinterpret_cast<const std::uint8_t*>(value.data());
        buffer_.insert(buffer_.end(), data, data + value.size());
    }

    void write_bytes(const std::uint8_t* data, std::size_t size)
    {
        buffer_.push_back(static_cast<std::uint8_t>(binary::TypeTag::Bytes));
        write_le(static_cast<std::uint64_t>(size));
        buffer_.insert(buffer_.end(), data, data + size);
    }

    template <typename ByteContainer>
    void write_bytes(const ByteContainer& bytes)
    {
        static_assert(detail::is_binary_byte_container_v<ByteContainer>,
                      "ByteContainer must store uint8_t");
        write_bytes(bytes.data(), bytes.size());
    }

    void write_array_header(std::uint64_t size)
    {
        buffer_.push_back(static_cast<std::uint8_t>(binary::TypeTag::Array));
        write_le(size);
    }

    void write_map_header(std::uint64_t size)
    {
        buffer_.push_back(static_cast<std::uint8_t>(binary::TypeTag::Map));
        write_le(size);
    }

    std::size_t size() const noexcept
    {
        return buffer_.size();
    }

private:
    Buffer& buffer_;

    template <typename T>
    void write_le(T value)
    {
        static_assert(std::is_trivially_copyable_v<T>,
                      "Type must be trivially copyable");

        const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
        buffer_.insert(buffer_.end(), bytes, bytes + sizeof(T));
    }
};

// ============================================================================
// BinaryReader: Expected-based reader over contiguous bytes
// ============================================================================

class BinaryReader
{
public:
    BinaryReader(const std::uint8_t* data, std::size_t size) noexcept
        : data_(data)
        , size_(size)
        , pos_(0)
    {
    }

    template <typename Container,
              typename = std::enable_if_t<
                  detail::is_binary_byte_container_v<Container>>>
    explicit BinaryReader(const Container& buffer) noexcept
        : BinaryReader(buffer.data(), buffer.size())
    {
    }

    std::size_t remaining() const noexcept
    {
        return size_ - pos_;
    }

    bool empty() const noexcept
    {
        return remaining() == 0U;
    }

    BinaryResult<binary::TypeTag> peek_type() const
    {
        if (pos_ >= size_)
        {
            return make_unexpected(BinaryError("Buffer underflow peeking type"));
        }
        return static_cast<binary::TypeTag>(data_[pos_]);
    }

    // ========================================================================
    // Unsigned integer reads
    // ========================================================================

    BinaryResult<std::uint8_t> read_uint8()
    {
        auto tag = expect_tag(binary::TypeTag::Uint8);
        if (!tag)
        {
            return make_unexpected(tag.error());
        }
        return read_byte();
    }

    BinaryResult<std::uint16_t> read_uint16()
    {
        auto tag = expect_tag(binary::TypeTag::Uint16);
        if (!tag)
        {
            return make_unexpected(tag.error());
        }
        return read_le<std::uint16_t>();
    }

    BinaryResult<std::uint32_t> read_uint32()
    {
        auto tag = expect_tag(binary::TypeTag::Uint32);
        if (!tag)
        {
            return make_unexpected(tag.error());
        }
        return read_le<std::uint32_t>();
    }

    BinaryResult<std::uint64_t> read_uint64()
    {
        auto tag = expect_tag(binary::TypeTag::Uint64);
        if (!tag)
        {
            return make_unexpected(tag.error());
        }
        return read_le<std::uint64_t>();
    }

    // ========================================================================
    // Signed integer reads
    // ========================================================================

    BinaryResult<std::int8_t> read_int8()
    {
        auto tag = expect_tag(binary::TypeTag::Int8);
        if (!tag)
        {
            return make_unexpected(tag.error());
        }
        auto byte = read_byte();
        if (!byte)
        {
            return make_unexpected(byte.error());
        }
        return static_cast<std::int8_t>(*byte);
    }

    BinaryResult<std::int16_t> read_int16()
    {
        auto tag = expect_tag(binary::TypeTag::Int16);
        if (!tag)
        {
            return make_unexpected(tag.error());
        }
        return read_le<std::int16_t>();
    }

    BinaryResult<std::int32_t> read_int32()
    {
        auto tag = expect_tag(binary::TypeTag::Int32);
        if (!tag)
        {
            return make_unexpected(tag.error());
        }
        return read_le<std::int32_t>();
    }

    BinaryResult<std::int64_t> read_int64()
    {
        auto tag = expect_tag(binary::TypeTag::Int64);
        if (!tag)
        {
            return make_unexpected(tag.error());
        }
        return read_le<std::int64_t>();
    }

    // ========================================================================
    // Other primitive reads
    // ========================================================================

    BinaryResult<float> read_float()
    {
        auto tag = expect_tag(binary::TypeTag::Float32);
        if (!tag)
        {
            return make_unexpected(tag.error());
        }
        return read_le<float>();
    }

    BinaryResult<double> read_double()
    {
        auto tag = expect_tag(binary::TypeTag::Float64);
        if (!tag)
        {
            return make_unexpected(tag.error());
        }
        return read_le<double>();
    }

    BinaryResult<bool> read_bool()
    {
        auto tag = expect_tag(binary::TypeTag::Bool);
        if (!tag)
        {
            return make_unexpected(tag.error());
        }
        auto byte = read_byte();
        if (!byte)
        {
            return make_unexpected(byte.error());
        }
        return *byte != 0;
    }

    BinaryResult<std::string> read_string()
    {
        auto tag = expect_tag(binary::TypeTag::String);
        if (!tag)
        {
            return make_unexpected(tag.error());
        }

        auto len_result = read_le<std::uint64_t>();
        if (!len_result)
        {
            return make_unexpected(len_result.error());
        }

        const auto len = static_cast<std::size_t>(*len_result);
        if (remaining() < len)
        {
            return make_unexpected(
                BinaryError("Buffer underflow reading string"));
        }

        std::string result(reinterpret_cast<const char*>(data_ + pos_), len);
        pos_ += len;
        return result;
    }

    BinaryResult<std::vector<std::uint8_t>> read_bytes()
    {
        auto tag = expect_tag(binary::TypeTag::Bytes);
        if (!tag)
        {
            return make_unexpected(tag.error());
        }

        auto len_result = read_le<std::uint64_t>();
        if (!len_result)
        {
            return make_unexpected(len_result.error());
        }

        const auto len = static_cast<std::size_t>(*len_result);
        if (remaining() < len)
        {
            return make_unexpected(
                BinaryError("Buffer underflow reading bytes"));
        }

        std::vector<std::uint8_t> result(data_ + pos_, data_ + pos_ + len);
        pos_ += len;
        return result;
    }

    BinaryResult<std::uint64_t> read_array_header()
    {
        auto tag = expect_tag(binary::TypeTag::Array);
        if (!tag)
        {
            return make_unexpected(tag.error());
        }
        return read_le<std::uint64_t>();
    }

    BinaryResult<std::uint64_t> read_map_header()
    {
        auto tag = expect_tag(binary::TypeTag::Map);
        if (!tag)
        {
            return make_unexpected(tag.error());
        }
        return read_le<std::uint64_t>();
    }

private:
    const std::uint8_t* data_;
    std::size_t size_;
    std::size_t pos_;

    BinaryResult<std::uint8_t> read_byte()
    {
        if (pos_ >= size_)
        {
            return make_unexpected(BinaryError("Buffer underflow reading byte"));
        }
        return data_[pos_++];
    }

    template <typename T>
    BinaryResult<T> read_le()
    {
        static_assert(std::is_trivially_copyable_v<T>,
                      "Type must be trivially copyable");

        if (remaining() < sizeof(T))
        {
            return make_unexpected(BinaryError("Buffer underflow"));
        }

        T value;
        std::memcpy(&value, data_ + pos_, sizeof(T));
        pos_ += sizeof(T);
        return value;
    }

    BinaryResult<void> expect_tag(binary::TypeTag expected)
    {
        if (pos_ >= size_)
        {
            return make_unexpected(BinaryError("Buffer underflow reading tag"));
        }

        const auto actual = static_cast<binary::TypeTag>(data_[pos_++]);
        if (actual != expected)
        {
            return make_unexpected(BinaryError(
                "Type mismatch: expected " +
                std::to_string(static_cast<int>(expected)) +
                " got " + std::to_string(static_cast<int>(actual))));
        }
        return {};
    }
};

// ============================================================================
// BinaryTraits: Fat-P style trait layer for binary serialization
// ============================================================================

template <typename T, typename Enable = void>
struct BinaryTraits;

// ----------------------------------------------------------------------------
// Signed integers
// ----------------------------------------------------------------------------

template <typename T>
struct BinaryTraits<
    T,
    std::enable_if_t<std::is_integral_v<T> && std::is_signed_v<T>>>
{
    template <typename Writer>
    static void encode(Writer& writer, T value)
    {
        writer.write_int(value);
    }

    template <typename Reader>
    static BinaryResult<T> decode(Reader& reader)
    {
        BinaryResult<std::int64_t> result;

        auto tag = reader.peek_type();
        if (!tag)
        {
            return make_unexpected(tag.error());
        }

        switch (*tag)
        {
            case binary::TypeTag::Int8:
            {
                auto v = reader.read_int8();
                if (!v) return make_unexpected(v.error());
                result = static_cast<std::int64_t>(*v);
                break;
            }
            case binary::TypeTag::Int16:
            {
                auto v = reader.read_int16();
                if (!v) return make_unexpected(v.error());
                result = static_cast<std::int64_t>(*v);
                break;
            }
            case binary::TypeTag::Int32:
            {
                auto v = reader.read_int32();
                if (!v) return make_unexpected(v.error());
                result = static_cast<std::int64_t>(*v);
                break;
            }
            case binary::TypeTag::Int64:
            {
                auto v = reader.read_int64();
                if (!v) return make_unexpected(v.error());
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
struct BinaryTraits<
    T,
    std::enable_if_t<std::is_integral_v<T> && std::is_unsigned_v<T>>>
{
    template <typename Writer>
    static void encode(Writer& writer, T value)
    {
        writer.write_uint(value);
    }

    template <typename Reader>
    static BinaryResult<T> decode(Reader& reader)
    {
        BinaryResult<std::uint64_t> result;

        auto tag = reader.peek_type();
        if (!tag)
        {
            return make_unexpected(tag.error());
        }

        switch (*tag)
        {
            case binary::TypeTag::Uint8:
            {
                auto v = reader.read_uint8();
                if (!v) return make_unexpected(v.error());
                result = static_cast<std::uint64_t>(*v);
                break;
            }
            case binary::TypeTag::Uint16:
            {
                auto v = reader.read_uint16();
                if (!v) return make_unexpected(v.error());
                result = static_cast<std::uint64_t>(*v);
                break;
            }
            case binary::TypeTag::Uint32:
            {
                auto v = reader.read_uint32();
                if (!v) return make_unexpected(v.error());
                result = static_cast<std::uint64_t>(*v);
                break;
            }
            case binary::TypeTag::Uint64:
            {
                auto v = reader.read_uint64();
                if (!v) return make_unexpected(v.error());
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
struct BinaryTraits<T, std::enable_if_t<std::is_enum_v<T>>>
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
        writer.write_bool(value);
    }

    template <typename Reader>
    static BinaryResult<bool> decode(Reader& reader)
    {
        return reader.read_bool();
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
        writer.write_float(value);
    }

    template <typename Reader>
    static BinaryResult<float> decode(Reader& reader)
    {
        return reader.read_float();
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
        writer.write_double(value);
    }

    template <typename Reader>
    static BinaryResult<double> decode(Reader& reader)
    {
        return reader.read_double();
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
        writer.write_string(value);
    }

    template <typename Reader>
    static BinaryResult<std::string> decode(Reader& reader)
    {
        return reader.read_string();
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
        writer.write_array_header(static_cast<std::uint64_t>(vec.size()));
        for (const auto& elem : vec)
        {
            BinaryTraits<T>::encode(writer, elem);
        }
    }

    template <typename Reader>
    static BinaryResult<std::vector<T, Alloc>> decode(Reader& reader)
    {
        auto len_result = reader.read_array_header();
        if (!len_result)
        {
            return make_unexpected(len_result.error());
        }

        const auto len = *len_result;
        if (len > static_cast<std::uint64_t>(
                      std::numeric_limits<std::size_t>::max()))
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
        writer.write_map_header(static_cast<std::uint64_t>(m.size()));
        for (const auto& kv : m)
        {
            BinaryTraits<K>::encode(writer, kv.first);
            BinaryTraits<V>::encode(writer, kv.second);
        }
    }

    template <typename Reader>
    static BinaryResult<std::map<K, V, Compare, Alloc>> decode(Reader& reader)
    {
        auto len_result = reader.read_map_header();
        if (!len_result)
        {
            return make_unexpected(len_result.error());
        }

        const auto len = *len_result;
        if (len > static_cast<std::uint64_t>(
                      std::numeric_limits<std::size_t>::max()))
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
void binary_encode(Writer& writer, const T& value)
{
    BinaryTraits<T>::encode(writer, value);
}

template <typename T, typename Reader>
BinaryResult<T> binary_decode(Reader& reader)
{
    return BinaryTraits<T>::decode(reader);
}

template <typename T, typename ByteContainer>
BinaryResult<void> binary_encode_to(ByteContainer& buffer, const T& value)
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
BinaryResult<T> binary_decode_from(const ByteContainer& buffer)
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


// Local-scope convenience imports (does not pollute namespace fat_p)
#define USING_FATP_BINARY()                          \
    using fat_p::binary_fatp::BinaryError;           \
    using fat_p::binary_fatp::BinaryResult;          \
    using fat_p::binary_fatp::DefaultBinaryBuffer;   \
    using fat_p::binary_fatp::BinaryBuffer;          \
    using fat_p::binary_fatp::BinaryWriter;          \
    using fat_p::binary_fatp::BinaryReader;          \
    using fat_p::binary_fatp::BinaryTraits;          \
    using fat_p::binary_fatp::binary_encode;         \
    using fat_p::binary_fatp::binary_decode;         \
    using fat_p::binary_fatp::binary_encode_to;      \
    using fat_p::binary_fatp::binary_decode_from;
